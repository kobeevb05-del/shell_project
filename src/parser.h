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

bool parser_parse(const token_list_t *tokens,
                  ast_node_t **result,
                  parser_error_t *error);

#endif
