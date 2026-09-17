#ifndef TOKEN_H
#define TOKEN_H

/* Типы токенов, которые может встретить лексер */
typedef enum {
    TOKEN_WORD,       /* Обычное слово, команда или аргумент */
    TOKEN_PIPE,       /* | */
    TOKEN_AND,        /* && */
    TOKEN_OR,         /* || */
    TOKEN_BG,         /* & */
    TOKEN_SEQ,        /* ; */
    TOKEN_REDIR_IN,   /* < */
    TOKEN_REDIR_OUT,  /* > */
    TOKEN_REDIR_APP,  /* >> */
    TOKEN_LPAREN,     /* ( */
    TOKEN_RPAREN,     /* ) */
    TOKEN_EOF         /* Конец файла/ввода */
} token_type_t;

/* Структура одного токена */
typedef struct {
    token_type_t type;
    char *lexeme;     /* Текст токена. Для операторов может быть NULL */
} token_t;

#endif /* TOKEN_H */