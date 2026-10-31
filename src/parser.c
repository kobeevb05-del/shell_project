/* Синтаксический анализ и построение AST. */
#include "parser.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

static bool append_argument(ast_simple_t *command, const char *word)
{
    if (command->argc > SIZE_MAX / sizeof(*command->argv) - 2) {
        return false;
    }

    char *copy = strdup(word);
    if (copy == NULL) {
        return false;
    }

    char **argv = realloc(command->argv,
                          (command->argc + 2) * sizeof(*argv));
    if (argv == NULL) {
        free(copy);
        return false;
    }

    command->argv = argv;
    command->argv[command->argc] = copy;
    command->argc++;
    command->argv[command->argc] = NULL;

    return true;
}

static void parser_set_error(parser_error_t *error,
                             size_t position,
                             const char *message)
{
    if (error != NULL) {
        error->position = position;
        error->message = message;
    }
}

static ast_simple_t *parse_simple(const token_list_t *tokens, size_t *index, parser_error_t *error)
{
    const token_t *token = &tokens->tokens[*index];

    if (token->type != TOKEN_WORD) {
        parser_set_error(error, token->position, "expected command");
        return NULL;

    }
    ast_simple_t *command = calloc(1, sizeof(*command));
    if (command == NULL) {
        parser_set_error(error, token->position, "out of memory");
        return NULL;
    }
    while (tokens->tokens[*index].type == TOKEN_WORD){
        token = &tokens->tokens[*index];

        if (!append_argument(command, token->lexeme)){
            parser_set_error(error, token->position, "out of memory");
            ast_simple_free(command);
            return NULL;
        }
        ++(*index);
    }
    return command;
}

bool parser_parse(const token_list_t *tokens, ast_simple_t **result, parser_error_t *error)
{
    size_t index = 0;
    *result = NULL;
    parser_set_error(error, 0, NULL);

    while (tokens->tokens[index].type == TOKEN_NEWLINE){
        ++index;
    }

    if (tokens->tokens[index].type == TOKEN_EOF){
        return true;
    }

    ast_simple_t *command = parse_simple(tokens, &index, error);
    if (command == NULL){
        return false;
    }

    while (tokens->tokens[index].type == TOKEN_NEWLINE){
        ++index;
    }

    const token_t *token = &tokens->tokens[index];

    if (token->type != TOKEN_EOF){
        parser_set_error(error, token->position, "unexpected token after command");
        ast_simple_free(command);
        return false;
    }
    *result = command;
    return true;
}