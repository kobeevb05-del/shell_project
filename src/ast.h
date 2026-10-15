#ifndef AST_H
#define AST_H

#include <stddef.h>
#include <stdio.h>

typedef struct {
    char **argv;
    size_t argc;
} ast_simple_t;

/* Освобождает аргументы и саму структуру команды. */
void ast_simple_free(ast_simple_t *command);

/* Печатает представление команды в указанный поток. */
void ast_simple_print(FILE *output, const ast_simple_t *command);

#endif