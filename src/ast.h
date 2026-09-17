#ifndef AST_H
#define AST_H

/* Типы узлов абстрактного синтаксического дерева (AST) */
typedef enum {
    AST_SIMPLE_CMD, /* Простая команда (например: ls -l) */
    AST_PIPELINE,   /* Конвейер (cmd1 | cmd2) */
    AST_AND_OR,     /* Логические списки (cmd1 && cmd2, cmd1 || cmd2) */
    AST_SUBSHELL    /* Подоболочка ( (cmd) ) */
} ast_node_type_t;

#endif /* AST_H */