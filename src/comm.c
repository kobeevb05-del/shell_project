#include "comm.h"
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

int comm_cd(const ast_simple_t *command)
{
    if (command->argc > 2) {
        fputs("mysh: cd: too many arguments\n", stderr);
        return 1;
    }

    if (command->argc == 2 && command->argv[1][0] == '-') {
        fputs("mysh: cd: options and 'cd -' are not supported\n",
              stderr);
        return 2;
    }

    const char *path = command->argc == 2
        ? command->argv[1]
        : getenv("HOME");

    if (path == NULL) {
        fputs("mysh: cd: HOME not set\n", stderr);
        return 1;
    }

    /* Пустой HOME: оставляем текущий каталог. */
    if (command->argc == 1 && path[0] == '\0') {
        return 0;
    }

    if (chdir(path) == -1) {
        int saved_errno = errno;
        fprintf(stderr, "mysh: cd: %s: %s\n",
                path, strerror(saved_errno));
        return 1;
    }

    return 0;
}