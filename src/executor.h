#ifndef EXECUTOR_H
#define EXECUTOR_H

#include "ast.h"

/* Выполняет команду, возвращает её статус. Дерево не освобождает. */
int executor_run(const ast_node_t *node);

#endif