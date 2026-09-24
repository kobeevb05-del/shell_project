#ifndef LEXER_H
#define LEXER_H

#include "token.h"
#include <stdbool.h>
#include <stddef.h> /* Для size_t */

typedef struct {
    token_t *tokens;  /* Указатель на массив токенов */
    size_t count;     /* Текущее количество токенов */
    size_t capacity;  /* Сколько памяти выделено (вместимость) */
} token_list_t;

typedef struct {
    size_t position;
    const char *message;
} lexer_error_t;

/* Функции для работы со списком токенов */
void token_list_init(token_list_t *list);
void token_list_free(token_list_t *list);
bool lexer_tokenize(const char *input, token_list_t *list, lexer_error_t *error);
const char *token_type_name(token_type_t type);

#endif
