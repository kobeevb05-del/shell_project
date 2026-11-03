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
 *   *result содержит дерево команд или NULL для пустого ввода.
 * При ошибке возвращает false:
 *   *result == NULL, частично построенное дерево освобождено.
 *
 * Дерево владеет копиями строк, токены остаются у вызывающего.
 * Готовое дерево нужно освободить через ast_node_free().
 * error может быть NULL; message освобождать не нужно.
 */
bool parser_parse(const token_list_t *tokens,
                  ast_node_t **result,
                  parser_error_t *error);

#endif
