#include "executor.h"
#include "comm.h"
#include "redir.h"
#include "signals.h"
#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

static int print_errno(const char *name)
{
    fprintf(stderr, "mysh: %s: %s\n", name, strerror(errno));
    return 1;
}

/* Ищет команду без '/' в PATH
 При успехе не возвращается, при ошибке возвращает -1 и задаёт errno*/
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

        // Проверяем размер до сложения: каталог, имя, '/' и '\0'
        if (name_length > SIZE_MAX - 2 ||
            directory_length > SIZE_MAX - name_length - 2) {
            errno = ENAMETOOLONG;
            return -1;
        }

        // Пустой элемент PATH означает текущий каталог
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

typedef int (*builtin_fn)(const ast_simple_t *);

// проверка на встроенную команду
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

    int saved[2];
    if (redir_save_std(node, saved) != 0) {
        return 1;
    }

    int status = redir_apply(node);
    if (status == 0) {
        status = fn(node->data.simple);
    }
    if (redir_restore_std(saved) != 0) {
        status = 1;
    }
    return status;
}
// настройка перенаправлений в ребенке, запуск встр/внеш ком
static void run_child_stage(const ast_node_t *node)
{
    if (redir_apply(node) != 0) {
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

static int wait_pipeline(size_t count, size_t started, const pid_t *pids)
{
    size_t remaining = started;
    int last_status = 0;

    while (remaining > 0) {
        int status;
        pid_t pid = waitpid(-1, &status, 0);
        if (pid == -1 && errno == EINTR) {
            continue;
        }
        if (pid == -1) {
            return print_errno("waitpid");
        }

        for (size_t i = 0; i < started; ++i) {
            if (pids[i] != pid) {
                continue;
            }
            --remaining;
            if (i + 1 == count) {
                if (WIFEXITED(status)) {
                    last_status = WEXITSTATUS(status);
                } else if (WIFSIGNALED(status)) {
                    last_status = 128 + WTERMSIG(status);
                } else {
                    last_status = 1;
                }
            }
            break;
        }
    }

    return last_status;
}

// запуск конвейера
static int run_pipeline(const ast_node_t *node, bool background)
{
    // дерево | в массив
    size_t count = 1;
    for (const ast_node_t *p = node; p->type == AST_PIPE; p = p->data.binary.left) {
        ++count;
    }

    const ast_node_t **stages = calloc(count, sizeof(*stages));
    pid_t *pids = calloc(count, sizeof(*pids));
    if (stages == NULL || pids == NULL) {
        free(stages);
        free(pids);
        return print_errno("calloc");
    }

    for (size_t i = count - 1; i > 0; --i) {
        stages[i] = node->data.binary.right;
        node = node->data.binary.left;
    }
    stages[0] = node;

    sigset_t old_mask;
    if (signals_block_chld(&old_mask) != 0) { // блок SIGCHLD
        free(stages);
        free(pids);
        return 1;
    }

    size_t started = 0;
    int prev_read = -1;
    int result = 0;

    for (size_t i = 0; i < count; ++i) {
        int pipe_fds[2] = {-1, -1};
        if (i + 1 < count && pipe(pipe_fds) == -1) {
            result = print_errno("pipe");
            break;
        }

        pid_t pid = fork();
        if (pid == 0) {
            if (signals_restore_mask(&old_mask) != 0) {
                _exit(1);
            }
            //ребенок закрывает конец чтения нового потока
            if (pipe_fds[0] != -1 && close(pipe_fds[0]) == -1) {
                _exit(print_errno("close"));
            }
            // prev_read на вход
            if (prev_read != -1) {
                if (redir_dup2(prev_read, STDIN_FILENO) != 0) {
                    _exit(1);
                }
                if (close(prev_read) == -1) {
                    _exit(print_errno("close"));
                }
            }
            // pipe_fds на выход
            if (pipe_fds[1] != -1) {
                if (redir_dup2(pipe_fds[1], STDOUT_FILENO) != 0) {
                    _exit(1);
                }
                if (close(pipe_fds[1]) == -1) {
                    _exit(print_errno("close"));
                }
            }
            run_child_stage(stages[i]);
        }

        if (pid == -1) {
            result = print_errno("fork");
        } else {
            pids[started++] = pid;
        }

        // родитель закрывает остатки
        if (prev_read != -1 && close(prev_read) == -1) {
            result = print_errno("close");
        }
        if (pipe_fds[1] != -1 && close(pipe_fds[1]) == -1) {
            result = print_errno("close");
        }
        prev_read = pipe_fds[0];

        if (result != 0) {
            break;
        }
    }

    if (prev_read != -1 && close(prev_read) == -1) {
        result = print_errno("close");
    }

    if (result != 0) {
        for (size_t i = 0; i < started; ++i) {
            if (kill(pids[i], SIGKILL) == -1 && errno != ESRCH) {
                print_errno("kill");
            }
        }
    }

    if (!background || result != 0) {
        int wait_res = wait_pipeline(count, started, pids);
        if (result == 0) {
            result = wait_res;
        }
    }

    if (signals_restore_mask(&old_mask) != 0) {
        result = 1;
    }
    free(stages);
    free(pids);
    return result;
}

// проверка узлов на работу с exec
static bool is_supported(const ast_node_t *node)
{
    if (node->type == AST_SIMPLE) {
        return true;
    }
    if (node->type == AST_PIPE || node->type == AST_SEQUENCE) {
        return is_supported(node->data.binary.left) &&
               is_supported(node->data.binary.right);
    }
    if (node->type == AST_BACKGROUND) {
        const ast_node_t *child = node->data.child;
        return (child->type == AST_SIMPLE || child->type == AST_PIPE) &&
               is_supported(child);
    }
    return false;
}

static int run_node(const ast_node_t *node)
{
    if (node->type == AST_SEQUENCE) {
        run_node(node->data.binary.left);
        return run_node(node->data.binary.right);
    }
    if (node->type == AST_BACKGROUND) {
        return run_pipeline(node->data.child, true);
    }
    if (node->type == AST_SIMPLE) {
        builtin_fn fn = find_builtin(node->data.simple->argv[0]);
        if (fn != NULL) {
            return run_builtin_in_parent(node, fn);
        }
    }
    return run_pipeline(node, false);
}

int executor_run(const ast_node_t *node)
{
    if (node == NULL) {
        return 0;
    }
    if (!is_supported(node)) {
        fputs("mysh: executor: unsupported command syntax\n", stderr);
        return 2;
    }
    if (signals_init_background() != 0) {
        return 1;
    }
    return run_node(node);
}