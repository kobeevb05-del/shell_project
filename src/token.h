#ifndef TOKEN_H
#define TOKEN_H

#include <stddef.h>

/* Типы токенов командной строки. */
typedef enum {
    TOKEN_WORD,
    TOKEN_PIPE,
    TOKEN_AND,
    TOKEN_OR,
    TOKEN_BG,
    TOKEN_SEQ,
    TOKEN_REDIR_IN,
    TOKEN_REDIR_OUT,
    TOKEN_REDIR_APP,
    TOKEN_LPAREN,
    TOKEN_RPAREN,
    TOKEN_NEWLINE,
    TOKEN_EOF
} token_type_t;

/* Позиция — номер байта в исходном вводе, начиная с единицы. */
typedef struct {
    token_type_t type;
    char *lexeme;
    size_t position;
} token_t;

#endif