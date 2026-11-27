#ifndef REDIR_H
#define REDIR_H
#include "ast.h"

// повторяет dup2 при EINTR
int redir_dup2(int source, int target);

// применяет перенаправления узла (<, >, >>) слева направо
int redir_apply(const ast_node_t *node);

// сохраняет стандартные потоки (0/1), затрагиваемые перенаправлениями узла
int redir_save_std(const ast_node_t *node, int saved[2]);

// восстанавливает ранее сохранённые потоки (0/1) и закрывает временные копии
int redir_restore_std(const int saved[2]);

#endif