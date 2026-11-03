/* Синтаксический анализ и построение AST. */
#include "parser.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

/* Глубина рекурсивного спуска ограничена размером стека процесса. */
enum { MAX_SUBSHELL_DEPTH = 1024 };

typedef struct {
    const token_t *current;
    parser_error_t *error;
    size_t subshell_depth;
} parser_t;

static ast_node_t *parse_list(parser_t *parser);
static void parser_set_error(parser_t *parser, const char *message);

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


static void skip_newlines(parser_t *parser)
{
    while (parser->current->type == TOKEN_NEWLINE) {
        ++parser->current;
    }
}

static bool is_redirection(token_type_t type)
{
    return type == TOKEN_REDIR_IN || type == TOKEN_REDIR_OUT ||
           type == TOKEN_REDIR_APP;
}

/* Вызывается на операторе перенаправления. Следующее слово — имя файла. */
static bool parse_redirection(parser_t *parser, ast_node_t *node)
{
    ast_redir_type_t type = AST_REDIR_OUT;
    if (parser->current->type == TOKEN_REDIR_IN) {
        type = AST_REDIR_IN;
    } else if (parser->current->type == TOKEN_REDIR_APP) {
        type = AST_REDIR_APPEND;
    }
    ++parser->current;
    if (parser->current->type != TOKEN_WORD) {
        parser_set_error(parser, "expected filename after redirection");
        return false;
    }
    const char *path = parser->current->lexeme;
    if (strncmp(path, "/dev/tcp/", sizeof("/dev/tcp/") - 1) == 0) {
        parser_set_error(parser, "unsupported /dev/tcp redirection");
        return false;
    }
    if (!ast_node_add_redirection(node, type, path)) {
        parser_set_error(parser, "out of memory");
        return false;
    }
    ++parser->current;
    return true;
}

static ast_node_t *parse_simple(parser_t *parser)
{
    ast_simple_t *command = calloc(1, sizeof(*command));
    ast_node_t *node = ast_node_new_simple(command);
    if (node == NULL) {
        ast_simple_free(command);
        parser_set_error(parser, "out of memory");
        return NULL;
    }

    for (;;) {
        if (parser->current->type == TOKEN_WORD) {
            if (!append_argument(command, parser->current->lexeme)) {
                parser_set_error(parser, "out of memory");
                ast_node_free(node);
                return NULL;
            }
            ++parser->current;
        } else if (is_redirection(parser->current->type)) {
            if (!parse_redirection(parser, node)) {
                ast_node_free(node);
                return NULL;
            }
        } else {
            break;
        }
    }
    if (command->argc == 0) {
        parser_set_error(parser, "expected command name");
        ast_node_free(node);
        return NULL;
    }
    return node;
}

/* Обе обёртки забирают владение поддеревьями и при ошибке. */
static ast_node_t *wrap_node(parser_t *parser, ast_type_t type, ast_node_t *child)
{
    ast_node_t *node = ast_node_new_unary(type, child);
    if (node == NULL) {
        ast_node_free(child);
        parser_set_error(parser, "out of memory");
    }
    return node;
}

static ast_node_t *join_nodes(parser_t *parser, ast_type_t type,
                              ast_node_t *left, ast_node_t *right)
{
    if (right == NULL) {
        /* Ошибку разбора правого операнда уже записала вызванная функция. */
        ast_node_free(left);
        return NULL;
    }
    ast_node_t *node = ast_node_new_binary(type, left, right);
    if (node == NULL) {
        ast_node_free(left);
        ast_node_free(right);
        parser_set_error(parser, "out of memory");
    }
    return node;
}

static void parser_set_error(parser_t *parser, const char *message)
{
    if (parser->error != NULL) {
        parser->error->position = parser->current->position;
        parser->error->message = message;
    }
}

static bool parser_match(parser_t *parser, token_type_t expected)
{
    if (parser->current->type != expected) {
        return false;
    }

    if (expected != TOKEN_EOF) {
        ++parser->current;
    }
    return true;
}

static bool parser_expect(parser_t *parser, token_type_t expected,
                          const char *message)
{
    if (parser_match(parser, expected)) {
        return true;
    }

    parser_set_error(parser, message);
    return false;
}

static ast_node_t *parse_subshell(parser_t *parser)
{
    if (parser->subshell_depth == MAX_SUBSHELL_DEPTH) {
        parser_set_error(parser, "subshell nesting limit exceeded");
        return NULL;
    }

    ++parser->current;  /* Пропускаем открывающую скобку. */
    skip_newlines(parser);

    ++parser->subshell_depth;
    ast_node_t *child = parse_list(parser);
    --parser->subshell_depth;

    if (child == NULL) {
        return NULL;
    }
    if (!parser_expect(parser, TOKEN_RPAREN, "expected ')'")) {
        ast_node_free(child);
        return NULL;
    }

    ast_node_t *node = wrap_node(parser, AST_SUBSHELL, child);
    while (node != NULL && is_redirection(parser->current->type)) {
        if (!parse_redirection(parser, node)) {
            ast_node_free(node);
            return NULL;
        }
    }
    return node;
}

static ast_node_t *parse_command(parser_t *parser)
{
    if (parser->current->type == TOKEN_LPAREN) {
        return parse_subshell(parser);
    }

    if (parser->current->type != TOKEN_WORD &&
        !is_redirection(parser->current->type)) {
        parser_set_error(parser, "expected command");
        return NULL;
    }

    return parse_simple(parser);
}
static ast_node_t *parse_pipeline(parser_t *parser)
{
    ast_node_t *left = parse_command(parser);

    while (left != NULL && parser_match(parser, TOKEN_PIPE)) {
        ast_node_t *right = parse_command(parser);
        left = join_nodes(parser, AST_PIPE, left, right);
    }

    return left;
}

static ast_node_t *parse_and_or(parser_t *parser)
{
    ast_node_t *left = parse_pipeline(parser);
    while (left != NULL && (parser->current->type == TOKEN_AND ||
                            parser->current->type == TOKEN_OR)) {
        ast_type_t type = parser->current->type == TOKEN_AND ? AST_AND : AST_OR;
        ++parser->current;
        left = join_nodes(parser, type, left, parse_pipeline(parser));
    }
    return left;
}

static ast_node_t *parse_list(parser_t *parser)
{
    ast_node_t *list = NULL;
    for (;;) {
        ast_node_t *item = parse_and_or(parser);
        token_type_t separator = parser->current->type;
        /* В a ; b & фоновым становится только b. */
        if (item != NULL && separator == TOKEN_BG) {
            item = wrap_node(parser, AST_BACKGROUND, item);
        }
        list = (list == NULL) ? item : join_nodes(parser, AST_SEQUENCE, list, item);
        if (list == NULL) {
        return NULL;
        }
        if (separator != TOKEN_SEQ && separator != TOKEN_BG &&
            separator != TOKEN_NEWLINE) {
            return list;
        }
        ++parser->current;
        skip_newlines(parser);
        if (parser->current->type == TOKEN_EOF ||
            parser->current->type == TOKEN_RPAREN) {
            return list;
        }
    }
}

bool parser_parse(const token_list_t *tokens, ast_node_t **result,
                  parser_error_t *error)
{
    parser_t parser = {.current = tokens->tokens, .error = error};
    *result = NULL;
    if (error != NULL) {
        *error = (parser_error_t){0};
    }
    skip_newlines(&parser);
    if (parser.current->type == TOKEN_EOF) {
        return true;
    }

    ast_node_t *tree = parse_list(&parser);
    if (tree == NULL) {
        return false;
    }
    if (!parser_expect(&parser, TOKEN_EOF, "unexpected token after command")) {
    ast_node_free(tree);
    return false;
    }
    *result = tree;
    return true;
}
