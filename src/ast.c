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
 void ast_simple_print(FILE *output, const ast_simple_t *command)
 {
    if (command == NULL) {
        return;
    }
    fprintf(output, "SIMPLE argc=%zu\n", command->argc); //fprintf принимает поток первым аргументом
        for (size_t i=0; i < command-> argc; ++i){
            fprintf(output, " argv[%zu] = \"%s\"\n", i, command->argv[i]);
        } 
 }