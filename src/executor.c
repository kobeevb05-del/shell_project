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

/* Ищет команду без '/' в PATH.
 * При успехе не возвращается; при ошибке возвращает -1 и задаёт errno.
 */
static int exec_from_path(const ast_simple_t *command)
{
    if (command->argv[0][0] == '\0') {
        errno = ENOENT;
        return -1;
    }

    const char *path = getenv("PATH");
    if (path == NULL) {
        /* Выбранные каталоги по умолчанию для нашей Linux-системы. */
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

int executor_run(const ast_node_t *node)
{
    if (node == NULL) {
        return 0;
    }
    if (node->type != AST_SIMPLE) {
        fputs("mysh: executor: execution is not implemented yet\n", stderr);
        return 2;
    }
    if (node->redirection_count != 0){
        fputs("mysh: executor: redirections are not supported yet\n", stderr);
        return 2;
    }
    const ast_simple_t *command = node->data.simple;

        if (strcmp(command->argv[0], "cd") == 0) {
        return comm_cd(command);
    }

    pid_t pid = fork();
    if (pid == -1){
    fprintf(stderr, "mysh: %s: fork: %s\n", command->argv[0], strerror(errno));
    return 1;
}

    if (pid == 0) {
    if (strchr(command->argv[0], '/') != NULL) {
    execv(command->argv[0], command->argv);
    } else {
    exec_from_path(command);
}

    int saved_errno = errno;
    fprintf(stderr, "mysh: %s: %s\n",
            command->argv[0], strerror(saved_errno));

    /* 127 — ENOENT; 126 — остальные ошибки запуска. */
    _exit(saved_errno == ENOENT ? 127 : 126);
}

int wait_status;

    while (waitpid(pid, &wait_status, 0) == -1){
    if (errno == EINTR){
        continue;
    }
    fprintf(stderr, "mysh: %s: waitpid: %s\n", command->argv[0], strerror(errno));
    return 1;
}

    if (WIFEXITED(wait_status)) {
        return WEXITSTATUS(wait_status);
}

    if (WIFSIGNALED(wait_status)){
        return 128 + WTERMSIG(wait_status);
}

     fputs("mysh: executor: child did not exit nirmally\n", stderr);
return 1;
}




