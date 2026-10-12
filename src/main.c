#include "lexer.h"

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

static void print_quoted(const char *text) {
    putchar('"');

    for (const unsigned char *cursor = (const unsigned char *)text;
         *cursor != '\0'; ++cursor) {
        switch (*cursor) {
        case '\\':
            fputs("\\\\", stdout);
            break;
        case '"':
            fputs("\\\"", stdout);
            break;
        case '\n':
            fputs("\\n", stdout);
            break;
        case '\t':
            fputs("\\t", stdout);
            break;
        case '\r':
            fputs("\\r", stdout);
            break;
        default:
            putchar(*cursor);
            break;
        }
    }

    putchar('"');
}

static void print_tokens(const token_list_t *tokens) {
    for (size_t i = 0; i < tokens->count; ++i) {
        const token_t *token = &tokens->tokens[i];

        printf("%zu: %-13s",
               token->position,
               token_type_name(token->type));

        if (token->lexeme != NULL) {
            putchar(' ');
            print_quoted(token->lexeme);
        }

        putchar('\n');
    }
}

static int process_input(const char *input) {
    token_list_t tokens;
    lexer_error_t error;

    if (!lexer_tokenize(input, &tokens, &error)) {
        fprintf(stderr, "mysh: lexer: byte %zu: %s\n",
                error.position, error.message);
        return 2;
    }

    print_tokens(&tokens);
    token_list_free(&tokens);

    if (ferror(stdout) || fflush(stdout) == EOF) {
        fputs("mysh: output: failed to write tokens\n", stderr);
        return 1;
    }

    return 0;
}

int main(int argc, char **argv) {
    char *input = NULL;
    bool read_from_stdin = true;

    if (argc == 3 && strcmp(argv[1], "-c") == 0) {
        input = argv[2];
        read_from_stdin = false;
    } else if (argc == 1) {
    } else if (argc == 2 &&
               strcmp(argv[1], "--dump-tokens") == 0) {
    } else {
        fprintf(stderr,
                "usage: %s [--dump-tokens | -c command]\n",
                argv[0]);
        return 2;
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

            last_status = process_input(line);
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

    int result = process_input(input);

    if (read_from_stdin) {
        free(input);
    }

    return result;
}