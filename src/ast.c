#include "ast.h"

#include <stdlib.h>

static const char *redirection_name(ast_redirection_type_t type) {
    switch (type) {
    case AST_REDIR_IN: return "<";
    case AST_REDIR_OUT: return ">";
    case AST_REDIR_APPEND: return ">>";
    }
    return "?";
}

void ast_command_free(ast_command_t *command) {
    if (command == NULL) {
        return;
    }
    if (command->type == AST_COMMAND_SIMPLE) {
        ast_simple_element_t *element = command->value.elements;
        while (element != NULL) {
            ast_simple_element_t *next = element->next;
            if (element->is_redirection) {
                free(element->value.redirection.target);
            } else {
                free(element->value.word);
            }
            free(element);
            element = next;
        }
    } else {
        ast_free(command->value.subshell.list);
        ast_redirection_t *redirection = command->value.subshell.redirections;
        while (redirection != NULL) {
            ast_redirection_t *next = redirection->next;
            free(redirection->target);
            free(redirection);
            redirection = next;
        }
    }
    free(command);
}

void ast_pipeline_free(ast_pipeline_t *pipeline) {
    if (pipeline == NULL) {
        return;
    }
    ast_pipeline_command_t *item = pipeline->commands;
    while (item != NULL) {
        ast_pipeline_command_t *next = item->next;
        ast_command_free(item->command);
        free(item);
        item = next;
    }
    free(pipeline);
}

void ast_and_or_free(ast_and_or_t *and_or) {
    if (and_or == NULL) {
        return;
    }
    ast_pipeline_free(and_or->first);
    ast_and_or_item_t *item = and_or->rest;
    while (item != NULL) {
        ast_and_or_item_t *next = item->next;
        ast_pipeline_free(item->pipeline);
        free(item);
        item = next;
    }
    free(and_or);
}

void ast_free(ast_list_t *list) {
    if (list == NULL) {
        return;
    }
    ast_list_item_t *item = list->items;
    while (item != NULL) {
        ast_list_item_t *next = item->next;
        ast_and_or_free(item->and_or);
        free(item);
        item = next;
    }
    free(list);
}

static void print_indent(FILE *output, unsigned depth) {
    for (unsigned i = 0; i < depth; ++i) {
        fputs("  ", output);
    }
}

static void print_list(FILE *output, const ast_list_t *list, unsigned depth);

static void print_command(FILE *output, const ast_command_t *command,
                          unsigned depth) {
    print_indent(output, depth);
    if (command->type == AST_COMMAND_SIMPLE) {
        fputs("SimpleCommand\n", output);
        for (const ast_simple_element_t *element = command->value.elements;
             element != NULL; element = element->next) {
            print_indent(output, depth + 1);
            if (element->is_redirection) {
                fprintf(output, "Redirection %s %s\n",
                        redirection_name(element->value.redirection.type),
                        element->value.redirection.target);
            } else {
                fprintf(output, "Word \"%s\"\n", element->value.word);
            }
        }
        return;
    }
    fputs("Subshell\n", output);
    print_list(output, command->value.subshell.list, depth + 1);
    for (const ast_redirection_t *redirection =
             command->value.subshell.redirections;
         redirection != NULL; redirection = redirection->next) {
        print_indent(output, depth + 1);
        fprintf(output, "Redirection %s %s\n",
                redirection_name(redirection->type), redirection->target);
    }
}

static void print_pipeline(FILE *output, const ast_pipeline_t *pipeline,
                           unsigned depth) {
    print_indent(output, depth);
    fputs("Pipeline\n", output);
    for (const ast_pipeline_command_t *item = pipeline->commands;
         item != NULL; item = item->next) {
        print_command(output, item->command, depth + 1);
    }
}

static void print_and_or(FILE *output, const ast_and_or_t *and_or,
                         unsigned depth) {
    print_indent(output, depth);
    fputs("AndOr\n", output);
    print_pipeline(output, and_or->first, depth + 1);
    for (const ast_and_or_item_t *item = and_or->rest; item != NULL;
         item = item->next) {
        print_indent(output, depth + 1);
        fprintf(output, "%s\n", item->operator == AST_OP_AND ? "&&" : "||");
        print_pipeline(output, item->pipeline, depth + 2);
    }
}

static void print_list(FILE *output, const ast_list_t *list, unsigned depth) {
    print_indent(output, depth);
    fputs("List\n", output);
    for (const ast_list_item_t *item = list->items; item != NULL;
         item = item->next) {
        if (item->and_or != NULL) {
            print_and_or(output, item->and_or, depth + 1);
        } else {
            print_indent(output, depth + 1);
            fputs("Empty\n", output);
        }
        if (item->has_separator) {
            print_indent(output, depth + 1);
            fputs(item->separator == AST_SEPARATOR_SEQUENCE ? ";\n" : "&\n",
                  output);
        }
    }
}

void ast_print(FILE *output, const ast_list_t *list) {
    print_list(output, list, 0);
}
