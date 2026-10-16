#include "ast.h"

#include <stdlib.h>

void ast_simple_free(ast_simple_t *command)
{
    if (command == NULL) {
        return;
    }

    for (size_t i = 0; i < command->argc; ++i) {
        free(command->argv[i]);
    }

    free(command->argv);
    free(command);
}