#include "comm.h"
#include <stdint.h>
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

int comm_pwd(const ast_simple_t *command)
{
    if (command->argc != 1) {
        fputs("mysh: pwd: arguments are not supported\n", stderr);
        return 2;
    }

    size_t capacity = 256; /* Начальный размер, не предел длины пути. */
    char *path = NULL;

    for (;;) {
        char *new_path = realloc(path, capacity);
        if (new_path == NULL) {
            goto failure;
        }
        path = new_path;

        char *result;
        do {
            result = getcwd(path, capacity);
        } while (result == NULL && errno == EINTR);

        if (result != NULL) {
            break;
        }

        if (errno != ERANGE) {
            goto failure;
        }

        if (capacity > SIZE_MAX / 2) {
            errno = ENOMEM;
            goto failure;
        }

        capacity *= 2;
    }

    if (dprintf(STDOUT_FILENO, "%s\n", path) < 0) {
        goto failure;
    }

    free(path);
    return 0;

failure:
    {
        int saved_errno = errno;
        free(path);
        fprintf(stderr, "mysh: pwd: %s\n", strerror(saved_errno));
        return 1;
    }
}

int comm_echo(const ast_simple_t *command)
{
    size_t first = 1;
    bool print_newline = true;

    while (first < command->argc &&
           strcmp(command->argv[first], "-n") == 0) {
        print_newline = false;
        ++first;
    }

    for (size_t i = first; i < command->argc; ++i) {
        const char *separator = i == first ? "" : " ";
        if (dprintf(STDOUT_FILENO, "%s%s", separator, command->argv[i]) < 0) {
            goto write_error;
        }
    }

    if (print_newline && dprintf(STDOUT_FILENO, "\n") < 0) {
        goto write_error;
    }
    return 0;

write_error:
    perror("mysh: echo");
    return 1;
}