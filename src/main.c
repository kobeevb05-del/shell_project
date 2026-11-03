/* Чтение ввода, выбор режима и передача результата между фазами разбора. */
#include "parser.h"

#include <errno.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>


static ssize_t read_line(char **line, size_t *capacity, FILE *stream) {
    for (;;) {
        errno = 0;
        ssize_t result = getline(line, capacity, stream);

        if (result >= 0 || feof(stream)) {
            return result;
        }

        if (errno != EINTR) {
            return -1;
        }

        clearerr(stream);
    }
}

static char *read_all(FILE *stream) {
    char *line = NULL;
    size_t line_capacity = 0;

    char *buffer = NULL;
    size_t length = 0;
    size_t capacity = 0;

    ssize_t bytes;

    while ((bytes = read_line(&line, &line_capacity, stream)) >= 0) {
        size_t chunk_size = (size_t)bytes;

        if (chunk_size > SIZE_MAX - length - 1) {
            free(line);
            free(buffer);
            errno = ENOMEM;
            return NULL;
        }

        size_t required = length + chunk_size + 1;

        if (required > capacity) {
            size_t new_capacity = capacity == 0 ? 256 : capacity;

            while (new_capacity < required) {
                if (new_capacity > SIZE_MAX / 2) {
                    new_capacity = required;
                    break;
                }
                new_capacity *= 2;
            }

            char *new_buffer = realloc(buffer, new_capacity);

            if (new_buffer == NULL) {
                free(line);
                free(buffer);
                errno = ENOMEM;
                return NULL;
            }

            buffer = new_buffer;
            capacity = new_capacity;
        }

        memcpy(buffer + length, line, chunk_size);
        length += chunk_size;
        buffer[length] = '\0';
    }

    int saved_errno = errno;
    bool read_failed = !feof(stream);
    free(line);

    if (read_failed) {
        free(buffer);
        errno = saved_errno != 0 ? saved_errno : EIO;
        return NULL;
    }

    if (buffer == NULL) {
        buffer = calloc(1, 1);
    }

    return buffer;
}

static void print_tokens(const token_list_t *tokens) {
    for (size_t i = 0; i < tokens->count; ++i) {
        const token_t *token = &tokens->tokens[i];

        printf("%zu: %-13s",
               token->position,
               token_type_name(token->type));

        if (token->lexeme != NULL) {
            putchar(' ');
            ast_print_quoted(stdout, token->lexeme);
        }

        putchar('\n');
    }
}

typedef enum { MODE_NORMAL, MODE_TOKENS, MODE_AST } input_mode_t;

static int process_input(const char *input, input_mode_t mode) {
    token_list_t tokens;
    lexer_error_t lex_error;

    if (!lexer_tokenize(input, &tokens, &lex_error)) {
        fprintf(stderr, "mysh: lexer: byte %zu: %s\n",
                lex_error.position, lex_error.message);
        return 2;
    }

    if (mode == MODE_TOKENS) {
        print_tokens(&tokens);
    } else {
        ast_node_t *root;
        parser_error_t parse_error;
        if (!parser_parse(&tokens, &root, &parse_error)) {
            fprintf(stderr, "mysh: parser: byte %zu: %s\n",
                    parse_error.position, parse_error.message);
            token_list_free(&tokens);
            return 2;
        }
        if (mode == MODE_AST) {
            ast_node_print(stdout, root);
        } else {
            /* До подключения исполнителя обычный запуск показывает токены. */
            print_tokens(&tokens);
        }
        ast_node_free(root);
    }
    token_list_free(&tokens);

    if (ferror(stdout) || fflush(stdout) == EOF) {
        fputs("mysh: output: write failed\n", stderr);
        return 1;
    }

    return 0;
}

int main(int argc, char **argv) {
    char *input = NULL;
    input_mode_t mode = MODE_NORMAL;
    int arg = 1;
    if (arg < argc && strcmp(argv[arg], "--dump-tokens") == 0) {
        mode = MODE_TOKENS;
        ++arg;
    } else if (arg < argc && strcmp(argv[arg], "--dump-ast") == 0) {
        mode = MODE_AST;
        ++arg;
    }
    bool read_from_stdin = arg == argc;
    if (!read_from_stdin) {
        if (argc - arg != 2 || strcmp(argv[arg], "-c") != 0) {
            fprintf(stderr, "usage: %s [--dump-tokens | --dump-ast] [-c command]\n",
                    argv[0]);
            return 2;
        }
        input = argv[arg + 1];
    }

    if (read_from_stdin && argc == 1 && isatty(STDIN_FILENO)) {
        char *line = NULL;
        size_t capacity = 0;
        int last_status = 0;

        for (;;) {
            if (fputs("mysh> ", stdout) == EOF ||
                fflush(stdout) == EOF) {
                fputs("mysh: output: failed to write prompt\n",
                      stderr);
                last_status = 1;
                break;
            }

            ssize_t bytes = read_line(&line, &capacity, stdin);

            if (bytes < 0) {
                if (feof(stdin)) {
                    putchar('\n');
                } else {
                    int saved_errno = errno != 0 ? errno : EIO;
                    fprintf(stderr, "mysh: input: %s\n",
                            strerror(saved_errno));
                    last_status = 1;
                }

                break;
            }

            last_status = process_input(line, mode);
        }

        free(line);
        return last_status;
    }

    if (read_from_stdin) {
        input = read_all(stdin);

        if (input == NULL) {
            fprintf(stderr, "mysh: input: %s\n", strerror(errno));
            return 1;
        }
    }

    int result = process_input(input, mode);

    if (read_from_stdin) {
        free(input);
    }

    return result;
}
