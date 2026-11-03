/* Дерево команд: создание узлов, печать и освобождение их содержимого. */
#include "ast.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

static ast_node_t *new_node(ast_type_t type)
{
    ast_node_t *node = calloc(1, sizeof(*node));
    if (node != NULL) {
        node->type = type;
    }
    return node;
}

ast_node_t *ast_node_new_simple(ast_simple_t *command)
{
    if (command == NULL) {
        return NULL;
    }

    ast_node_t *node = new_node(AST_SIMPLE);
    if (node != NULL) {
        node->data.simple = command;
    }
    return node;
}

ast_node_t *ast_node_new_binary(ast_type_t type, ast_node_t *left, ast_node_t *right)
{
    if (left == NULL || right == NULL ||
        (type != AST_PIPE && type != AST_AND && type != AST_OR && type != AST_SEQUENCE)) {
        return NULL;
    }

    ast_node_t *node = new_node(type);
    if (node != NULL) {
        node->data.binary.left = left;
        node->data.binary.right = right;
    }
    return node;
}

ast_node_t *ast_node_new_unary(ast_type_t type, ast_node_t *child)
{
    if (child == NULL || (type != AST_BACKGROUND && type != AST_SUBSHELL)) {
        return NULL;
    }

    ast_node_t *node = new_node(type);
    if (node != NULL) {
        node->data.child = child;
    }
    return node;
}

bool ast_node_add_redirection(ast_node_t *node, ast_redir_type_t type, const char *path)
{
    if (node == NULL || (node->type != AST_SIMPLE && node->type != AST_SUBSHELL) ||
        path == NULL || node->redirection_count == SIZE_MAX / sizeof(*node->redirections)) {
        return false;
    }

    char *copy = strdup(path);
    if (copy == NULL) {
        return false;
    }

    size_t count = node->redirection_count;
    ast_redirection_t *redirections = realloc(node->redirections,
                                              (count + 1) * sizeof(*redirections));
    if (redirections == NULL) {
        free(copy);
        return false;
    }

    redirections[count] = (ast_redirection_t){ .type = type, .path = copy };
    node->redirections = redirections;
    node->redirection_count = count + 1;
    return true;
}

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

void ast_node_free(ast_node_t *node)
{
    while (node != NULL) {
        ast_node_t *next = NULL;
        switch (node->type) {
        case AST_SIMPLE:
            ast_simple_free(node->data.simple);
            break;
        case AST_BACKGROUND:
        case AST_SUBSHELL:
            next = node->data.child;
            break;
        case AST_PIPE:
        case AST_AND:
        case AST_OR:
        case AST_SEQUENCE:
            /* Длинная цепочка левоассоциативных операций обходится циклом. */
            ast_node_free(node->data.binary.right);
            next = node->data.binary.left;
            break;
        }

        for (size_t i = 0; i < node->redirection_count; ++i) {
            free(node->redirections[i].path);
        }
        free(node->redirections);
        free(node);
        node = next;
    }
}

void ast_print_quoted(FILE *output, const char *text)
{
    fputc('"', output);
    for (const unsigned char *p = (const unsigned char *)text; *p != '\0'; ++p) {
        switch (*p) {
        case '\\': fputs("\\\\", output); break;
        case '"':  fputs("\\\"", output); break;
        case '\n': fputs("\\n", output);  break;
        case '\t': fputs("\\t", output);  break;
        case '\r': fputs("\\r", output);  break;
        default:
            if (*p < 0x20 || *p == 0x7f) {
                fprintf(output, "\\x%02X", (unsigned int)*p);
            } else {
                fputc(*p, output);
            }
        }
    }
    fputc('"', output);
}

void ast_simple_print(FILE *output, const ast_simple_t *command)
{
    if (command == NULL) {
        return;
    }

    fprintf(output, "SIMPLE argc=%zu\n", command->argc);
    for (size_t i = 0; i < command->argc; ++i) {
        fprintf(output, " argv[%zu] = ", i);
        ast_print_quoted(output, command->argv[i]);
        fputc('\n', output);
    }
}

static void indent(FILE *output, size_t depth)
{
    for (size_t i = 0; i < depth; ++i) {
        fputs("  ", output);
    }
}

static void print_node(FILE *output, const ast_node_t *node, size_t depth)
{
    static const char *const names[] = {
        [AST_SIMPLE] = "SIMPLE", [AST_PIPE] = "PIPE", [AST_AND] = "AND",
        [AST_OR] = "OR", [AST_SEQUENCE] = "SEQUENCE",
        [AST_BACKGROUND] = "BACKGROUND", [AST_SUBSHELL] = "SUBSHELL"
    };
    static const char *const operators[] = {
        [AST_REDIR_IN] = "<", [AST_REDIR_OUT] = ">", [AST_REDIR_APPEND] = ">>"
    };

    indent(output, depth);
    fputs(names[node->type], output);
    if (node->type == AST_SIMPLE) {
        const ast_simple_t *command = node->data.simple;
        for (size_t i = 0; i < command->argc; ++i) {
            fputc(' ', output);
            ast_print_quoted(output, command->argv[i]);
        }
    }
    fputc('\n', output);

    switch (node->type) {
    case AST_SIMPLE:
        break;
    case AST_BACKGROUND:
    case AST_SUBSHELL:
        print_node(output, node->data.child, depth + 1);
        break;
    case AST_PIPE:
    case AST_AND:
    case AST_OR:
    case AST_SEQUENCE:
        print_node(output, node->data.binary.left, depth + 1);
        print_node(output, node->data.binary.right, depth + 1);
        break;
    }
    for (size_t i = 0; i < node->redirection_count; ++i) {
        const ast_redirection_t *redir = &node->redirections[i];
        indent(output, depth + 1);
        fprintf(output, "REDIR %s ", operators[redir->type]);
        ast_print_quoted(output, redir->path);
        fputc('\n', output);
    }
}

void ast_node_print(FILE *output, const ast_node_t *node)
{
    if (node == NULL) {
        fputs("EMPTY\n", output);
    } else {
        print_node(output, node, 0);
    }
}
