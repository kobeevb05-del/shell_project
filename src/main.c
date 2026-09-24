#include "ast.h"
#include "lexer.h"
#include "parser.h"

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

typedef enum {
    MODE_AST,
    MODE_TOKENS
} output_mode_t;

static char *read_all(FILE *input) {
    char *line = NULL;
    size_t line_capacity = 0;
    char *buffer = NULL;
    size_t length = 0;
    size_t capacity = 0;
    ssize_t bytes;

    while ((bytes = getline(&line, &line_capacity, input)) != -1) {
        size_t required = length + (size_t)bytes + 1;
        if (required > capacity) {
            size_t new_capacity = capacity == 0 ? 256 : capacity;
            while (new_capacity < required) {
                new_capacity *= 2;
            }
            char *new_buffer = realloc(buffer, new_capacity);
            if (new_buffer == NULL) {
                free(line);
                free(buffer);
                return NULL;
            }
            buffer = new_buffer;
            capacity = new_capacity;
        }
        memcpy(buffer + length, line, (size_t)bytes);
        length += (size_t)bytes;
        buffer[length] = '\0';
    }
    free(line);
    if (ferror(input)) {
        free(buffer);
        return NULL;
    }
    if (buffer == NULL) {
        buffer = calloc(1, 1);
    }
    return buffer;
}

static void print_quoted(const char *text) {
    putchar('"');
    for (const unsigned char *cursor = (const unsigned char *)text;
         *cursor != '\0'; ++cursor) {
        switch (*cursor) {
        case '\\': fputs("\\\\", stdout); break;
        case '"': fputs("\\\"", stdout); break;
        case '\n': fputs("\\n", stdout); break;
        case '\t': fputs("\\t", stdout); break;
        case '\r': fputs("\\r", stdout); break;
        default: putchar(*cursor); break;
        }
    }
    putchar('"');
}

static void print_tokens(const token_list_t *tokens) {
    for (size_t i = 0; i < tokens->count; ++i) {
        const token_t *token = &tokens->tokens[i];
        printf("%zu: %-13s", token->position, token_type_name(token->type));
        if (token->lexeme != NULL) {
            putchar(' ');
            print_quoted(token->lexeme);
        }
        putchar('\n');
    }
}

static int process_input(const char *input, output_mode_t mode) {
    token_list_t tokens;
    lexer_error_t lexer_error;
    if (!lexer_tokenize(input, &tokens, &lexer_error)) {
        fprintf(stderr, "lexer error at character %zu: %s\n",
                lexer_error.position, lexer_error.message);
        return 2;
    }
    if (mode == MODE_TOKENS) {
        print_tokens(&tokens);
        token_list_free(&tokens);
        return 0;
    }

    ast_list_t *ast = NULL;
    parser_error_t parser_error;
    if (!parser_parse(&tokens, &ast, &parser_error)) {
        fprintf(stderr, "parser error at character %zu: %s\n",
                parser_error.position, parser_error.message);
        token_list_free(&tokens);
        return 2;
    }
    ast_print(stdout, ast);
    ast_free(ast);
    token_list_free(&tokens);
    return 0;
}

int main(int argc, char **argv) {
    output_mode_t mode = MODE_AST;
    char *input = NULL;
    bool read_from_stdin = true;
    if (argc == 2 && strcmp(argv[1], "--dump-tokens") == 0) {
        mode = MODE_TOKENS;
    } else if (argc == 2 && strcmp(argv[1], "--dump-ast") == 0) {
        mode = MODE_AST;
    } else if (argc == 3 && strcmp(argv[1], "-c") == 0) {
        input = argv[2];
        read_from_stdin = false;
    } else if (argc != 1) {
        fprintf(stderr, "usage: %s [--dump-tokens | --dump-ast | -c command]\n",
                argv[0]);
        return 2;
    }

    if (read_from_stdin) {
        if (isatty(STDIN_FILENO) && argc == 1) {
            fputs("mysh> ", stdout);
            fflush(stdout);
        }
        input = read_all(stdin);
        if (input == NULL) {
            fputs("myshell: failed to read input\n", stderr);
            return 1;
        }
    }
    int result = process_input(input, mode);
    if (read_from_stdin) {
        free(input);
    }
    return result;
}
