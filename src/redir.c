#include "redir.h"
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

static int redir_error(const char *name)
{
    fprintf(stderr, "mysh: %s: %s\n", name, strerror(errno));
    return 1;
}

// открывает файл и подключает его к target (0 или 1)
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

// дублирует source в target с повтором при сигнале EINTR
int redir_dup2(int source, int target)
{
    int result;
    do {
        result = dup2(source, target);
    } while (result == -1 && errno == EINTR);

    if (result == -1) {
        return redir_error("dup2");
    }
    return 0;
}

// применяет все перенаправления команды (<, >, >>) слева направо
int redir_apply(const ast_node_t *node)
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
            return redir_error(redir->path);
        }
    }
    return 0;
}

// сохраняет исходные stdin/stdout через dup перед встроенной командой
int redir_save_std(const ast_node_t *node, int saved[2])
{
    saved[0] = -1;
    saved[1] = -1;

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
            redir_error("dup");
            for (int k = 0; k < 2; ++k) {
                if (saved[k] != -1) {
                    close(saved[k]);
                }
            }
            return 1;
        }
    }
    return 0;
}

//  восстанавливает stdin/stdout после встроенной команды и закрывает копии
int redir_restore_std(const int saved[2])
{
    int status = 0;
    for (int fd = 0; fd < 2; ++fd) {
        if (saved[fd] == -1) {
            continue;
        }
        if (redir_dup2(saved[fd], fd) != 0) {
            status = 1;
        }
        if (close(saved[fd]) == -1) {
            status = redir_error("close");
        }
    }
    return status;
}