#include "parser.h"

#include <stdlib.h>
#include <string.h>

typedef struct {
    const token_list_t *tokens;
    size_t index;
    parser_error_t *error;
} parser_t;

static const token_t *current(parser_t *parser) {
    return &parser->tokens->tokens[parser->index];
}

static bool syntax_error(parser_t *parser, const char *message) {
    if (parser->error != NULL) {
        parser->error->position = current(parser)->position;
        parser->error->message = message;
    }
    return false;
}

static bool allocation_error(parser_t *parser) {
    return syntax_error(parser, "out of memory");
}

static ast_list_t *parse_list(parser_t *parser, bool nested);

static ast_simple_element_t *new_simple_word(char *word) {
    ast_simple_element_t *element = malloc(sizeof(*element));
    if (element != NULL) {
        *element = (ast_simple_element_t){
            .is_redirection = false,
            .value.word = word,
        };
    }
    return element;
}

static ast_simple_element_t *new_simple_redirection(ast_redirection_type_t type,
                                                     char *target) {
    ast_simple_element_t *element = malloc(sizeof(*element));
    if (element != NULL) {
        *element = (ast_simple_element_t){
            .is_redirection = true,
            .value.redirection = {.type = type, .target = target},
        };
    }
    return element;
}

static ast_redirection_type_t redirection_type(token_type_t type) {
    switch (type) {
    case TOKEN_REDIR_IN: return AST_REDIR_IN;
    case TOKEN_REDIR_OUT: return AST_REDIR_OUT;
    default: return AST_REDIR_APPEND;
    }
}

static bool is_redirection(token_type_t type) {
    return type == TOKEN_REDIR_IN || type == TOKEN_REDIR_OUT ||
           type == TOKEN_REDIR_APP;
}

static char *copy_text(const char *text) {
    size_t length = strlen(text) + 1;
    char *copy = malloc(length);
    if (copy != NULL) {
        memcpy(copy, text, length);
    }
    return copy;
}

static ast_command_t *parse_simple(parser_t *parser) {
    ast_simple_element_t *head = NULL;
    ast_simple_element_t **tail = &head;
    bool has_word = false;
    while (current(parser)->type == TOKEN_WORD ||
           is_redirection(current(parser)->type)) {
        ast_simple_element_t *element = NULL;
        if (current(parser)->type == TOKEN_WORD) {
            char *word = copy_text(current(parser)->lexeme);
            if (word == NULL) {
                goto memory_error;
            }
            element = new_simple_word(word);
            if (element == NULL) {
                free(word);
                goto memory_error;
            }
            has_word = true;
            ++parser->index;
        } else {
            token_type_t operator_type = current(parser)->type;
            ++parser->index;
            if (current(parser)->type != TOKEN_WORD) {
                syntax_error(parser, "expected a word after redirection");
                goto error;
            }
            char *target = copy_text(current(parser)->lexeme);
            if (target == NULL) {
                goto memory_error;
            }
            element = new_simple_redirection(redirection_type(operator_type), target);
            if (element == NULL) {
                free(target);
                goto memory_error;
            }
            ++parser->index;
        }
        *tail = element;
        tail = &element->next;
    }
    if (!has_word) {
        syntax_error(parser, "simple command must contain a word");
        goto error;
    }

    ast_command_t *command = malloc(sizeof(*command));
    if (command == NULL) {
        goto memory_error;
    }
    command->type = AST_COMMAND_SIMPLE;
    command->value.elements = head;
    return command;

memory_error:
    allocation_error(parser);
error:
    while (head != NULL) {
        ast_simple_element_t *next = head->next;
        if (head->is_redirection) {
            free(head->value.redirection.target);
        } else {
            free(head->value.word);
        }
        free(head);
        head = next;
    }
    return NULL;
}

static ast_redirection_t *parse_subshell_redirections(parser_t *parser) {
    ast_redirection_t *head = NULL;
    ast_redirection_t **tail = &head;
    while (is_redirection(current(parser)->type)) {
        ast_redirection_type_t type = redirection_type(current(parser)->type);
        ++parser->index;
        if (current(parser)->type != TOKEN_WORD) {
            syntax_error(parser, "expected a word after redirection");
            while (head != NULL) {
                ast_redirection_t *next = head->next;
                free(head->target);
                free(head);
                head = next;
            }
            return NULL;
        }
        char *target = copy_text(current(parser)->lexeme);
        if (target == NULL) {
            allocation_error(parser);
            while (head != NULL) {
                ast_redirection_t *next = head->next;
                free(head->target);
                free(head);
                head = next;
            }
            return NULL;
        }
        ast_redirection_t *redirection = malloc(sizeof(*redirection));
        if (redirection == NULL) {
            free(target);
            allocation_error(parser);
            while (head != NULL) {
                ast_redirection_t *next = head->next;
                free(head->target);
                free(head);
                head = next;
            }
            return NULL;
        }
        *redirection = (ast_redirection_t){
            .type = type,
            .target = target,
        };
        *tail = redirection;
        tail = &redirection->next;
        ++parser->index;
    }
    return head;
}

static ast_command_t *parse_command(parser_t *parser) {
    if (current(parser)->type != TOKEN_LPAREN) {
        return parse_simple(parser);
    }
    ++parser->index;
    ast_list_t *list = parse_list(parser, true);
    if (list == NULL) {
        return NULL;
    }
    if (current(parser)->type != TOKEN_RPAREN) {
        syntax_error(parser, "expected ')' to close subshell");
        ast_free(list);
        return NULL;
    }
    ++parser->index;
    ast_redirection_t *redirections = parse_subshell_redirections(parser);
    if (parser->error != NULL && parser->error->message != NULL) {
        ast_free(list);
        while (redirections != NULL) {
            ast_redirection_t *next = redirections->next;
            free(redirections->target);
            free(redirections);
            redirections = next;
        }
        return NULL;
    }
    ast_command_t *command = malloc(sizeof(*command));
    if (command == NULL) {
        allocation_error(parser);
        ast_free(list);
        while (redirections != NULL) {
            ast_redirection_t *next = redirections->next;
            free(redirections->target);
            free(redirections);
            redirections = next;
        }
        return NULL;
    }
    command->type = AST_COMMAND_SUBSHELL;
    command->value.subshell.list = list;
    command->value.subshell.redirections = redirections;
    return command;
}

static ast_pipeline_t *parse_pipeline(parser_t *parser) {
    ast_pipeline_t *pipeline = calloc(1, sizeof(*pipeline));
    if (pipeline == NULL) {
        allocation_error(parser);
        return NULL;
    }
    ast_pipeline_command_t **tail = &pipeline->commands;
    for (;;) {
        ast_command_t *command = parse_command(parser);
        if (command == NULL) {
            ast_pipeline_free(pipeline);
            return NULL;
        }
        ast_pipeline_command_t *item = malloc(sizeof(*item));
        if (item == NULL) {
            allocation_error(parser);
            ast_command_free(command);
            ast_pipeline_free(pipeline);
            return NULL;
        }
        *item = (ast_pipeline_command_t){.command = command};
        *tail = item;
        tail = &item->next;
        if (current(parser)->type != TOKEN_PIPE) {
            break;
        }
        ++parser->index;
        if (current(parser)->type != TOKEN_WORD &&
            current(parser)->type != TOKEN_LPAREN &&
            !is_redirection(current(parser)->type)) {
            syntax_error(parser, "expected a command after '|'");
            ast_pipeline_free(pipeline);
            return NULL;
        }
    }
    return pipeline;
}

static ast_and_or_t *parse_and_or(parser_t *parser) {
    ast_and_or_t *and_or = calloc(1, sizeof(*and_or));
    if (and_or == NULL) {
        allocation_error(parser);
        return NULL;
    }
    and_or->first = parse_pipeline(parser);
    if (and_or->first == NULL) {
        free(and_or);
        return NULL;
    }
    ast_and_or_item_t **tail = &and_or->rest;
    while (current(parser)->type == TOKEN_AND || current(parser)->type == TOKEN_OR) {
        ast_and_or_operator_t operator = current(parser)->type == TOKEN_AND
                                             ? AST_OP_AND
                                             : AST_OP_OR;
        ++parser->index;
        if (current(parser)->type != TOKEN_WORD &&
            current(parser)->type != TOKEN_LPAREN &&
            !is_redirection(current(parser)->type)) {
            syntax_error(parser, "expected a pipeline after && or ||");
            ast_and_or_free(and_or);
            return NULL;
        }
        ast_pipeline_t *pipeline = parse_pipeline(parser);
        if (pipeline == NULL) {
            ast_and_or_free(and_or);
            return NULL;
        }
        ast_and_or_item_t *item = malloc(sizeof(*item));
        if (item == NULL) {
            allocation_error(parser);
            ast_pipeline_free(pipeline);
            ast_and_or_free(and_or);
            return NULL;
        }
        *item = (ast_and_or_item_t){.operator = operator, .pipeline = pipeline};
        *tail = item;
        tail = &item->next;
    }
    return and_or;
}

static bool is_list_separator(token_type_t type) {
    return type == TOKEN_SEQ || type == TOKEN_BG;
}

static ast_list_t *parse_list(parser_t *parser, bool nested) {
    ast_list_t *list = calloc(1, sizeof(*list));
    if (list == NULL) {
        allocation_error(parser);
        return NULL;
    }
    ast_list_item_t **tail = &list->items;
    if (current(parser)->type == TOKEN_EOF ||
        (nested && current(parser)->type == TOKEN_RPAREN)) {
        if (nested) {
            syntax_error(parser, "empty subshell is not a command");
            free(list);
            return NULL;
        }
        return list;
    }

    for (;;) {
        if (current(parser)->type == TOKEN_RPAREN && nested) {
            break;
        }
        if (current(parser)->type == TOKEN_EOF) {
            break;
        }
        if (is_list_separator(current(parser)->type)) {
            if (list->items == NULL) {
                syntax_error(parser, "operator without a command");
                ast_free(list);
                return NULL;
            }
            ast_list_item_t *empty = calloc(1, sizeof(*empty));
            if (empty == NULL) {
                allocation_error(parser);
                ast_free(list);
                return NULL;
            }
            empty->has_separator = true;
            empty->separator = current(parser)->type == TOKEN_SEQ
                                   ? AST_SEPARATOR_SEQUENCE
                                   : AST_SEPARATOR_BACKGROUND;
            *tail = empty;
            tail = &empty->next;
            ++parser->index;
            if (current(parser)->type == TOKEN_EOF ||
                (nested && current(parser)->type == TOKEN_RPAREN)) {
                ast_list_item_t *trailing_empty = calloc(1, sizeof(*trailing_empty));
                if (trailing_empty == NULL) {
                    allocation_error(parser);
                    ast_free(list);
                    return NULL;
                }
                *tail = trailing_empty;
                break;
            }
            continue;
        }
        ast_and_or_t *and_or = parse_and_or(parser);
        if (and_or == NULL) {
            ast_free(list);
            return NULL;
        }
        ast_list_item_t *item = malloc(sizeof(*item));
        if (item == NULL) {
            allocation_error(parser);
            ast_and_or_free(and_or);
            ast_free(list);
            return NULL;
        }
        *item = (ast_list_item_t){.and_or = and_or};
        *tail = item;
        tail = &item->next;

        if (!is_list_separator(current(parser)->type)) {
            break;
        }
        item->has_separator = true;
        item->separator = current(parser)->type == TOKEN_SEQ
                              ? AST_SEPARATOR_SEQUENCE
                              : AST_SEPARATOR_BACKGROUND;
        ++parser->index;
        if (current(parser)->type == TOKEN_EOF ||
            (nested && current(parser)->type == TOKEN_RPAREN)) {
            ast_list_item_t *empty = calloc(1, sizeof(*empty));
            if (empty == NULL) {
                allocation_error(parser);
                ast_free(list);
                return NULL;
            }
            *tail = empty;
            break;
        }
    }
    return list;
}

bool parser_parse(const token_list_t *tokens, ast_list_t **result,
                  parser_error_t *error) {
    *result = NULL;
    parser_error_t local_error = {0};
    if (error != NULL) *error = local_error;
    if (tokens == NULL || tokens->count == 0 ||
        tokens->tokens[tokens->count - 1].type != TOKEN_EOF) {
        if (error != NULL) {
            error->message = "token stream has no EOF token";
        }
        return false;
    }
    parser_t parser = {.tokens = tokens, .error = &local_error};
    ast_list_t *list = parse_list(&parser, false);
    if (list == NULL) {
        if (error != NULL) *error = local_error;
        return false;
    }
    if (current(&parser)->type != TOKEN_EOF) {
        syntax_error(&parser, current(&parser)->type == TOKEN_RPAREN
                                   ? "unexpected ')'"
                                   : "unexpected token after command");
        ast_free(list);
        if (error != NULL) *error = local_error;
        return false;
    }
    *result = list;
    if (error != NULL) *error = local_error;
    return true;
}
