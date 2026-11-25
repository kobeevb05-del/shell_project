#include "executor.h"
#include "comm.h"
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <stdint.h>
#include <stdlib.h>
#include <fcntl.h>
#include <signal.h>

static int redirect_file(const char *path, int target, int flags)
{
    int fd;
    do {
        fd = open(path, flags, 0666);
    } while (fd == -1 && errno == EINTR);

    if (fd == -1) {
        return -1;
    }
    if (fd == target) {
        return 0;
    }

    int result;
    do {
        result = dup2(fd, target);
    } while (result == -1 && errno == EINTR);

    int dup_error = errno;
    int close_result = close(fd);
    if (result == -1) {
        errno = dup_error;
        return -1;
    }
    return close_result;
}

/* Ищет команду без '/' в PATH.
 * При успехе не возвращается; при ошибке возвращает -1 и задаёт errno*/
static int exec_from_path(const ast_simple_t *command)
{
    if (command->argv[0][0] == '\0') {
        errno = ENOENT;
        return -1;
    }

    const char *path = getenv("PATH");
    if (path == NULL) {
        path = "/bin:/usr/bin";
    }

    size_t name_length = strlen(command->argv[0]);
    int search_error = ENOENT;
    const char *directory = path;

    while (directory != NULL) {
        const char *separator = strchr(directory, ':');
        size_t directory_length = separator == NULL
            ? strlen(directory)
            : (size_t)(separator - directory);

        /* Проверяем размер до сложения: каталог, имя, '/' и '\0'. */
        if (name_length > SIZE_MAX - 2 ||
            directory_length > SIZE_MAX - name_length - 2) {
            errno = ENAMETOOLONG;
            return -1;
        }

        /* Пустой элемент PATH означает текущий каталог. */
        size_t prefix_length = directory_length == 0
            ? 0 : directory_length + 1;

        char *candidate = malloc(prefix_length + name_length + 1);
        if (candidate == NULL) {
            return -1;
        }

        if (directory_length != 0) {
            memcpy(candidate, directory, directory_length);
            candidate[directory_length] = '/';
        }
        memcpy(candidate + prefix_length,
               command->argv[0], name_length + 1);

        execv(candidate, command->argv);

        int exec_error = errno;
        free(candidate);

        if (exec_error == EACCES) {
            search_error = EACCES;
        }

        if (exec_error != ENOENT &&
            exec_error != ENOTDIR &&
            exec_error != EACCES) {
            errno = exec_error;
            return -1;
        }

        directory = separator == NULL ? NULL : separator + 1;
    }

    errno = search_error;
    return -1;
}
static int print_errno(const char *name)
{
    fprintf(stderr, "mysh: %s: %s\n", name, strerror(errno));
    return 1;
}

/* Подбирает завершившиеся фоновые процессы, чтобы не оставались зомби. */
static void reap_background(int sig)
{
    (void)sig;
    int saved_errno = errno;
    for (;;) {
        pid_t pid = waitpid(-1, NULL, WNOHANG);
        if (pid > 0) {
            continue;
        }
        if (pid == -1 && errno == EINTR) {
            continue;
        }
        break;
    }
    errno = saved_errno;
}
static int dup2_retry(int source, int target)
{
    int result;
    do {
        result = dup2(source, target);
    } while (result == -1 && errno == EINTR);
    if (result == -1) {
        return print_errno("dup2");
    }
    return 0;
}

static int apply_redirections(const ast_node_t *node)
{
    for (size_t i = 0; i < node->redirection_count; ++i) {
        const ast_redirection_t *redir = &node->redirections[i];
        int target = STDOUT_FILENO;
        int flags = O_WRONLY | O_CREAT | O_TRUNC;
        if (redir->type == AST_REDIR_IN) {
            target = STDIN_FILENO;
            flags = O_RDONLY;
        } else if (redir->type == AST_REDIR_APPEND) {
            flags = O_WRONLY | O_CREAT | O_APPEND;
        }
        if (redirect_file(redir->path, target, flags) == -1) {
            return print_errno(redir->path);
        }
    }
    return 0;
}
typedef int (*builtin_fn)(const ast_simple_t *);
static builtin_fn find_builtin(const char *name)
{
    if (strcmp(name, "cd") == 0) {
        return comm_cd;
    }
    if (strcmp(name, "pwd") == 0) {
        return comm_pwd;
    }
    if (strcmp(name, "echo") == 0) {
        return comm_echo;
    }
    return NULL;
}
static int run_builtin_in_parent(const ast_node_t *node, builtin_fn fn)
{
    if (node->redirection_count == 0) {
        return fn(node->data.simple);
    }
    int saved[2] = {-1, -1};
    for (size_t i = 0; i < node->redirection_count; ++i) {
        int target = node->redirections[i].type == AST_REDIR_IN
            ? STDIN_FILENO : STDOUT_FILENO;
        if (saved[target] != -1) {
            continue;
        }
        do {
            saved[target] = dup(target);
        } while (saved[target] == -1 && errno == EINTR);
        if (saved[target] == -1) {
            print_errno("dup");
            for (int k = 0; k < 2; ++k) {
                if (saved[k] != -1) {
                    close(saved[k]);
                }
            }
            return 1;
        }
    }
    int status = apply_redirections(node);
    if (status == 0) {
        status = fn(node->data.simple);
    }
    for (int fd = 0; fd < 2; ++fd) {
        if (saved[fd] == -1) {
            continue;
        }
        if (dup2_retry(saved[fd], fd) != 0) {
            status = 1;
        }
        if (close(saved[fd]) == -1) {
            status = print_errno("close");
        }
    }
    return status;
}
static void run_child_stage(const ast_node_t *node)
{
    if (apply_redirections(node) != 0) {
        _exit(1);
    }
    const ast_simple_t *command = node->data.simple;
    builtin_fn fn = find_builtin(command->argv[0]);
    if (fn != NULL) {
        _exit(fn(command));
    }
    if (strchr(command->argv[0], '/') != NULL) {
        execv(command->argv[0], command->argv);
    } else {
        exec_from_path(command);
    }
    int code = errno == ENOENT ? 127 : 126;
    print_errno(command->argv[0]);
    _exit(code);
}