#include "lexer.h"
#include <stdlib.h>
#include <string.h>

typedef struct {
    char *data; /*указатель на начало массива*/
    size_t length; /*сколько записали */
    size_t capacity; /*сколько выделено памяти */
} string_builder_t;

static bool builder_append(string_builder_t *builder, char value) {
    if (builder->length + 1 >= builder->capacity) {
        size_t capacity = builder->capacity == 0 ? 16 : builder->capacity * 2;
        char *data = realloc(builder->data, capacity);
        if (data == NULL) {
            return false;
        }
        builder->data = data;
        builder->capacity = capacity;
    }
    builder->data[builder->length++] = value;
    return true;
}

static char *builder_finish(string_builder_t *builder) {
    if (!builder_append(builder, '\0')) {
        free(builder->data);
        builder->data = NULL;
        return NULL;
    }
    return builder->data;
}

static bool add_token(token_list_t *list, token_type_t type, char *lexeme, size_t position) {
    if (list->count == list->capacity) {
        size_t capacity = list->capacity == 0 ? 16 : list->capacity * 2;
        token_t *tokens = realloc(list->tokens, capacity * sizeof(*tokens));
        if (tokens == NULL) {
            return false;
        }
        list->tokens = tokens;
        list->capacity = capacity;
    }
    list->tokens[list->count++] = (token_t){
        .type = type,
        .lexeme = lexeme,
        .position = position,
    };
    return true;
}

void token_list_init(token_list_t *list) {
    *list = (token_list_t){0};
}

void token_list_free(token_list_t *list) {
    for (size_t i = 0; i < list->count; ++i) {
        free(list->tokens[i].lexeme);
    }
    free(list->tokens);
    *list = (token_list_t){0};
}

static bool fail(token_list_t *list, lexer_error_t *error, size_t position,
                 const char *message) {
    token_list_free(list);
    if (error != NULL) {
        error->position = position;
        error->message = message;
    }
    return false;
}

static bool is_operator(char c) {
    return c == '|' || c == '&' || c == ';' || c == '(' || c == ')' ||
           c == '<' || c == '>';
}

static token_type_t operator_type(char c, char next, size_t *width) {
    *width = 1;
    switch (c) {
    case '|':
        if (next == '|') {
            *width = 2;
            return TOKEN_OR;
        }
        return TOKEN_PIPE;
    case '&':
        if (next == '&') {
            *width = 2;
            return TOKEN_AND;
        }
        return TOKEN_BG;
    case ';': return TOKEN_SEQ;
    case '(': return TOKEN_LPAREN;
    case ')': return TOKEN_RPAREN;
    case '<': return TOKEN_REDIR_IN;
    case '>':
        if (next == '>') {
            *width = 2;
            return TOKEN_REDIR_APP;
        }
        return TOKEN_REDIR_OUT;
    default: return TOKEN_EOF;
    }
}

bool lexer_tokenize(const char *input, token_list_t *list,
                    lexer_error_t *error) {
    token_list_init(list);

    if (error != NULL) {
        *error = (lexer_error_t){0};
    }

    size_t i = 0;

    while (input[i] != '\0') {
        /* Продолжение строки между токенами. */
        if (input[i] == '\\' && input[i + 1] == '\n') {
            i += 2;
            continue;
        }

        if (input[i] == ' ' || input[i] == '\t') {
            ++i;
            continue;
        }

        /* Сохраняем границу строки отдельным токеном. */
        if (input[i] == '\n') {
            if (!add_token(list, TOKEN_NEWLINE, NULL, i + 1)) {
                return fail(list, error, i + 1, "out of memory");
            }

            ++i;
            continue;
        }

        /* Здесь мы находимся на границе токенов. (комментарии) */
        if (input[i] == '#') {
            while (input[i] != '\0' && input[i] != '\n') {
                ++i;
            }
            continue;
        }

        if (is_operator(input[i])) {
            size_t position = i;
            size_t next = i + 1;

            /*
             * Продолжение строки может находиться между
             * двумя символами одного оператора.
             */
            while (input[next] == '\\' && input[next + 1] == '\n') {
                next += 2;
            }

            size_t width;
            token_type_t type =
                operator_type(input[i], input[next], &width);

            char *lexeme = malloc(width + 1);
            if (lexeme == NULL) {
                return fail(list, error, position + 1,
                            "out of memory");
            }

            lexeme[0] = input[i];

            if (width == 2) {
                lexeme[1] = input[next];
            }

            lexeme[width] = '\0';

            if (!add_token(list, type, lexeme, position + 1)) {
                free(lexeme);
                return fail(list, error, position + 1,
                            "out of memory");
            }

            if (width == 2) {
                i = next + 1;
            } else {
                ++i;
            }

            continue;
        }

        size_t start = i;
        bool started = false;
        string_builder_t builder = {0};

        while (input[i] != '\0' &&
               input[i] != ' ' &&
               input[i] != '\t' &&
               input[i] != '\n' &&
               !is_operator(input[i])) {
            char c = input[i];

            if (c == '\'') {
                size_t quote_position = i;
                ++i;
                started = true;

                /* В одинарных кавычках всё сохраняется буквально. */
                while (input[i] != '\0' && input[i] != '\'') {
                    if (!builder_append(&builder, input[i])) {
                        free(builder.data);
                        return fail(list, error, quote_position + 1,
                                    "out of memory");
                    }
                    ++i;
                }

                if (input[i] == '\0') {
                    free(builder.data);
                    return fail(list, error, quote_position + 1,
                                "unterminated single quote");
                }

                ++i;
            } else if (c == '"') {
                size_t quote_position = i;
                ++i;
                started = true;

                while (input[i] != '\0' && input[i] != '"') {
                    if (input[i] == '\\') {
                        char next = input[i + 1];

                        if (next == '\n') {
                            i += 2;
                            continue;
                        }

                        if (next == '"' || next == '\\') {
                            c = next;
                            i += 2;
                        } else {
                            /* Перед остальными символами сохраняем \. */
                            c = input[i];
                            ++i;
                        }
                    } else {
                        c = input[i];
                        ++i;
                    }

                    if (!builder_append(&builder, c)) {
                        free(builder.data);
                        return fail(list, error, quote_position + 1,
                                    "out of memory");
                    }
                }

                if (input[i] == '\0') {
                    free(builder.data);
                    return fail(list, error, quote_position + 1,
                                "unterminated double quote");
                }

                ++i;
            } else if (c == '\\') {
                size_t slash_position = i;
                ++i;

                if (input[i] == '\0') {
                    free(builder.data);
                    return fail(list, error, slash_position + 1,
                                "backslash at end of input");
                }

                /* Продолжение строки внутри слова. */
                if (input[i] == '\n') {
                    ++i;
                    continue;
                }

                started = true;

                if (!builder_append(&builder, input[i])) {
                    free(builder.data);
                    return fail(list, error, slash_position + 1,
                                "out of memory");
                }

                ++i;
            } else {
                started = true;

                if (!builder_append(&builder, input[i])) {
                    free(builder.data);
                    return fail(list, error, i + 1,
                                "out of memory");
                }

                ++i;
            }
        }

        if (started) {
            char *lexeme = builder_finish(&builder);

            if (lexeme == NULL) {
                return fail(list, error, start + 1,
                            "out of memory");
            }

            if (!add_token(list, TOKEN_WORD, lexeme, start + 1)) {
                free(lexeme);
                return fail(list, error, start + 1,
                            "out of memory");
            }
        } else {
            free(builder.data);
        }
    }

    if (!add_token(list, TOKEN_EOF, NULL, i + 1)) {
        return fail(list, error, i + 1, "out of memory");
    }

    return true;
}

const char *token_type_name(token_type_t type) {
    switch (type) {
    case TOKEN_WORD:
        return "WORD";
    case TOKEN_PIPE:
        return "PIPE";
    case TOKEN_AND:
        return "AND";
    case TOKEN_OR:
        return "OR";
    case TOKEN_BG:
        return "BACKGROUND";
    case TOKEN_SEQ:
        return "SEQUENCE";
    case TOKEN_REDIR_IN:
        return "REDIR_IN";
    case TOKEN_REDIR_OUT:
        return "REDIR_OUT";
    case TOKEN_REDIR_APP:
        return "REDIR_APPEND";
    case TOKEN_LPAREN:
        return "LPAREN";
    case TOKEN_RPAREN:
        return "RPAREN";
    case TOKEN_NEWLINE:
        return "NEWLINE";
    case TOKEN_EOF:
        return "EOF";
    }

    return "UNKNOWN";
}