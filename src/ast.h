#ifndef AST_H
#define AST_H

#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>

typedef enum {
    AST_SIMPLE,
    AST_PIPE,
    AST_AND,
    AST_OR,
    AST_SEQUENCE,
    AST_BACKGROUND,
    AST_SUBSHELL
} ast_type_t;

typedef enum {
    AST_REDIR_IN,
    AST_REDIR_OUT,
    AST_REDIR_APPEND
} ast_redir_type_t;

typedef struct {
    ast_redir_type_t type;
    char *path;
} ast_redirection_t;

typedef struct {
    char **argv;
    size_t argc;
} ast_simple_t;

/* Узел владеет командой или потомками, а также путями перенаправлений. */
typedef struct ast_node {
    ast_type_t type;
    /* Для SIMPLE и SUBSHELL, в порядке следования во вводе. */
    ast_redirection_t *redirections;
    size_t redirection_count;
    union {
        ast_simple_t *simple;
        struct {
            struct ast_node *left;
            struct ast_node *right;
        } binary;
        struct ast_node *child;
    } data;
} ast_node_t;

/* Конструкторы принимают владение аргументами только при успехе. */
ast_node_t *ast_node_new_simple(ast_simple_t *command);
ast_node_t *ast_node_new_binary(ast_type_t type, ast_node_t *left, ast_node_t *right);
ast_node_t *ast_node_new_unary(ast_type_t type, ast_node_t *child);

/* Копирует путь. При ошибке возвращает false и оставляет узел прежним. */
bool ast_node_add_redirection(ast_node_t *node, ast_redir_type_t type, const char *path);

void ast_simple_free(ast_simple_t *command);
void ast_node_free(ast_node_t *node);

void ast_print_quoted(FILE *output, const char *text);
void ast_simple_print(FILE *output, const ast_simple_t *command);
void ast_node_print(FILE *output, const ast_node_t *node);

#endif
