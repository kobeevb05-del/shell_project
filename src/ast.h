#ifndef AST_H
#define AST_H

#include <stdbool.h>
#include <stdio.h>

typedef enum {
    AST_REDIR_IN,
    AST_REDIR_OUT,
    AST_REDIR_APPEND
} ast_redirection_type_t;

typedef enum {
    AST_OP_AND,
    AST_OP_OR
} ast_and_or_operator_t;

typedef enum {
    AST_SEPARATOR_SEQUENCE,
    AST_SEPARATOR_BACKGROUND
} ast_separator_t;

typedef enum {
    AST_COMMAND_SIMPLE,
    AST_COMMAND_SUBSHELL
} ast_command_type_t;

typedef struct ast_list ast_list_t;
typedef struct ast_and_or ast_and_or_t;
typedef struct ast_pipeline ast_pipeline_t;
typedef struct ast_command ast_command_t;

typedef struct ast_simple_element {
    bool is_redirection;
    union {
        char *word;
        struct {
            ast_redirection_type_t type;
            char *target;
        } redirection;
    } value;
    struct ast_simple_element *next;
} ast_simple_element_t;

typedef struct ast_redirection {
    ast_redirection_type_t type;
    char *target;
    struct ast_redirection *next;
} ast_redirection_t;

struct ast_command {
    ast_command_type_t type;
    union {
        ast_simple_element_t *elements;
        struct {
            ast_list_t *list;
            ast_redirection_t *redirections;
        } subshell;
    } value;
};

typedef struct ast_pipeline_command {
    ast_command_t *command;
    struct ast_pipeline_command *next;
} ast_pipeline_command_t;

struct ast_pipeline {
    ast_pipeline_command_t *commands;
};

typedef struct ast_and_or_item {
    ast_and_or_operator_t operator;
    ast_pipeline_t *pipeline;
    struct ast_and_or_item *next;
} ast_and_or_item_t;

struct ast_and_or {
    ast_pipeline_t *first;
    ast_and_or_item_t *rest;
};

typedef struct ast_list_item {
    ast_and_or_t *and_or;
    bool has_separator;
    ast_separator_t separator;
    struct ast_list_item *next;
} ast_list_item_t;

struct ast_list {
    ast_list_item_t *items;
};

void ast_free(ast_list_t *list);
void ast_command_free(ast_command_t *command);
void ast_pipeline_free(ast_pipeline_t *pipeline);
void ast_and_or_free(ast_and_or_t *and_or);
void ast_print(FILE *output, const ast_list_t *list);

#endif
