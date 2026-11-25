#ifndef PARSER_H
#define PARSER_H

#include <stdbool.h>
#include <stddef.h>

#include "ast.h"
#include "lexer.h"

typedef struct {
    size_t position;
    const char *message;
} parser_error_t;

typedef struct {
    bool ok;
    union {
        ast_node_t *tree;
        parser_error_t error;
    };
} parser_result_t;

parser_result_t parser_parse(const token_list_t *tokens);

#endif