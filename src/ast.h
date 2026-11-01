#ifndef AST_H
#define AST_H

#include <stddef.h>
#include <stdio.h>

typedef enum {
    AST_SIMPLE, // простые команды
    AST_PIPE,   // конвейер ls|wc
    AST_AND,    // условие cmd1 && cmd2
    AST_OR,     // условие cmd2 || cmd2
    AST_SEQUENCE, // последовательность cmd1 ; cmd2
    AST_BACKGROUND, // фоновое выполнение cmd &
    AST_SUBSHELL // подоболочка (cmd1 ; cmd2 )
} ast_type_t;

typedef struct {
    char **argv;
    size_t argc;
} ast_simple_t;

// type определяет используемый вариант data; узел владеет его содержимым
typedef struct ast_node {
    ast_type_t type;
    union {
        ast_simple_t *simple;
        struct {
            struct ast_node *left;
            struct ast_node *right;
        } binary;
        struct ast_node *child;
    } data;
} ast_node_t;

/*
 * Оборачивает динамически выделенную команду в узел AST_SIMPLE.
 * При успехе узел принимает владение командой.
 * При ошибке возвращает NULL и не освобождает команду.
 */
ast_node_t *ast_node_new_simple(ast_simple_t *command);

/* Освобождает аргументы и саму структуру команды. */
void ast_simple_free(ast_simple_t *command);

/* Печатает представление команды в указанный поток. */
void ast_simple_print(FILE *output, const ast_simple_t *command);

#endif
