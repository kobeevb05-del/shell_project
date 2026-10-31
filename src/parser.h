#ifndef PARSER_H
#define PARSER_H

#include <stdbool.h>
#include <stddef.h>

#include "ast.h"
#include "lexer.h"

typedef struct {
    size_t position;
    const char *message;
} parser_error_t;

/*
 * tokens — результат успешной работы лексера, включая EOF.
 * result — обязательный указатель для записи результата.
 *
 * При успехе возвращает true:
 *   *result содержит команду или NULL для пустого ввода.
 * При ошибке возвращает false:
 *   *result == NULL, частично построенная команда освобождена.
 *
 * Команда владеет копиями строк; токены остаются у вызывающего.
 * Готовую команду нужно освободить через ast_simple_free().
 * error может быть NULL; message освобождать не нужно.
 */
bool parser_parse(const token_list_t *tokens,
                  ast_simple_t **result,
                  parser_error_t *error);

#endif