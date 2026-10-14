/* Лексер: слова с кавычками, операторы и позиции в исходном вводе. */
#include "lexer.h"
#include <stdint.h>
#include <stdlib.h>

enum { INITIAL_CAPACITY = 16 };

typedef struct {
    char *data;
    size_t length;
    size_t capacity;
} string_builder_t;

static bool builder_append(string_builder_t *builder, char value) {
    if (builder->length == builder->capacity) {
        if (builder->capacity > SIZE_MAX / 2) return false;
        size_t capacity = builder->capacity ? builder->capacity * 2 : INITIAL_CAPACITY;
        char *data = realloc(builder->data, capacity);
        if (data == NULL) return false;
        builder->data = data;
        builder->capacity = capacity;
    }
    builder->data[builder->length++] = value;
    return true;
}

static bool add_token(token_list_t *list, token_type_t type,
                      char *lexeme, size_t position) {
    if (list->count == list->capacity) {
        if (list->capacity > SIZE_MAX / sizeof(token_t) / 2) return false;
        size_t capacity = list->capacity ? list->capacity * 2 : INITIAL_CAPACITY;
        token_t *tokens = realloc(list->tokens, capacity * sizeof(*tokens));
        if (tokens == NULL) return false;
        list->tokens = tokens;
        list->capacity = capacity;
    }
    list->tokens[list->count++] = (token_t){type, lexeme, position};
    return true;
}

void token_list_init(token_list_t *list) {
    *list = (token_list_t){0};
}

void token_list_free(token_list_t *list) {
    for (size_t i = 0; i < list->count; ++i) free(list->tokens[i].lexeme);
    free(list->tokens);
    token_list_init(list);
}

static void set_error(lexer_error_t *error, size_t position, const char *message) {
    if (error != NULL) *error = (lexer_error_t){position, message};
}

static bool is_operator(char c) {
    return c == '|' || c == '&' || c == ';' || c == '(' || c == ')' ||
           c == '<' || c == '>';
}

static token_type_t operator_type(char c, char next, size_t *width) {
    *width = c == next && (c == '|' || c == '&' || c == '>') ? 2 : 1;
    switch (c) {
    case '|': return *width == 2 ? TOKEN_OR : TOKEN_PIPE;
    case '&': return *width == 2 ? TOKEN_AND : TOKEN_BG;
    case '>': return *width == 2 ? TOKEN_REDIR_APP : TOKEN_REDIR_OUT;
    case ';': return TOKEN_SEQ;
    case '(': return TOKEN_LPAREN;
    case ')': return TOKEN_RPAREN;
    case '<': return TOKEN_REDIR_IN;
    default: return TOKEN_EOF;
    }
}

/* Вызывается в начале слова. quote: '\0' — без кавычек, иначе '\'' или '"'. */
static char *read_word(const char *input, size_t *index, lexer_error_t *error) {
    string_builder_t builder = {0};
    size_t start = *index, i = start, quote_position = start;
    size_t error_position = start;
    char quote = '\0';
    const char *message = "out of memory";

    while (input[i] != '\0') {
        char c = input[i];
        if (quote == '\0') {
            if (c == ' ' || c == '\t' || c == '\n' || is_operator(c)) break;
            if (c == '\'' || c == '"') {
                quote = c;
                quote_position = i++;
                continue;
            }
        } else if (c == quote) {
            quote = '\0';
            ++i;
            continue;
        }

        if (c == '\\' && quote != '\'') {
            char next = input[i + 1];
            if (next == '\n') {
                i += 2;
                continue;
            }
            if (quote == '\0' && next == '\0') {
                error_position = i;
                message = "backslash at end of input";
                goto error;
            }
            if (quote == '\0' || next == '"' || next == '\\') {
                if (!builder_append(&builder, next)) goto memory_error;
                i += 2;
                continue;
            }
        }
        if (!builder_append(&builder, c)) goto memory_error;
        ++i;
    }

    if (quote != '\0') {
        error_position = quote_position;
        message = quote == '\'' ? "unterminated single quote" : "unterminated double quote";
        goto error;
    }
    error_position = start;
    /* Даже пустые кавычки дают строку с завершающим нулём. */
    if (!builder_append(&builder, '\0')) goto error;
    *index = i;
    return builder.data;

memory_error:
    error_position = quote != '\0' ? quote_position : i;
error:
    free(builder.data);
    set_error(error, error_position + 1, message);
    return NULL;
}

bool lexer_tokenize(const char *input, token_list_t *list, lexer_error_t *error) {
    token_list_init(list);
    set_error(error, 0, NULL);
    size_t i = 0, position = 0;
    char *lexeme = NULL;

    while (input[i] != '\0') {
        if (input[i] == '\\' && input[i + 1] == '\n') {
            i += 2;
            continue;
        }
        if (input[i] == ' ' || input[i] == '\t') {
            ++i;
            continue;
        }
        /* Здесь # находится в начале слова; внутри read_word() он обычный. */
        if (input[i] == '#') {
            while (input[i] != '\0' && input[i] != '\n') ++i;
            continue;
        }

        position = i;
        token_type_t type;
        if (input[i] == '\n') {
            type = TOKEN_NEWLINE;
            ++i;
        } else if (is_operator(input[i])) {
            size_t next = i + 1, width;
            /* Учитываем продолжения строки между символами одного оператора. */
            while (input[next] == '\\' && input[next + 1] == '\n') next += 2;
            type = operator_type(input[i], input[next], &width);
            lexeme = malloc(width + 1);
            if (lexeme == NULL) goto memory_error;
            lexeme[0] = input[i];
            if (width == 2) lexeme[1] = input[next];
            lexeme[width] = '\0';
            i = width == 2 ? next + 1 : i + 1;
        } else {
            type = TOKEN_WORD;
            lexeme = read_word(input, &i, error);
            if (lexeme == NULL) goto error;
        }
        if (!add_token(list, type, lexeme, position + 1)) goto memory_error;
        lexeme = NULL; /* Строкой теперь владеет список токенов. */
    }
    position = i;
    if (!add_token(list, TOKEN_EOF, NULL, i + 1)) goto memory_error;
    return true;

memory_error:
    set_error(error, position + 1, "out of memory");
error:
    free(lexeme);
    token_list_free(list);
    return false;
}

const char *token_type_name(token_type_t type) {
    switch (type) {
    case TOKEN_WORD: return "WORD";
    case TOKEN_PIPE: return "PIPE";
    case TOKEN_AND: return "AND";
    case TOKEN_OR: return "OR";
    case TOKEN_BG: return "BACKGROUND";
    case TOKEN_SEQ: return "SEQUENCE";
    case TOKEN_REDIR_IN: return "REDIR_IN";
    case TOKEN_REDIR_OUT: return "REDIR_OUT";
    case TOKEN_REDIR_APP: return "REDIR_APPEND";
    case TOKEN_LPAREN: return "LPAREN";
    case TOKEN_RPAREN: return "RPAREN";
    case TOKEN_NEWLINE: return "NEWLINE";
    case TOKEN_EOF: return "EOF";
    }
    return "UNKNOWN";
}
