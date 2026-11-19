#ifndef BUILTINS_H
#define BUILTINS_H

#include "ast.h"

/* выполняет встроенную команду cd и возвращает её статус. */
int comm_cd(const ast_simple_t *command);
int comm_pwd(const ast_simple_t *command);
int comm_echo(const ast_simple_t *command);
#endif