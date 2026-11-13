#include "executor.h"
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

int executor_run(const ast_node_t *node)
{
    if (node == NULL) {
        return 0;
    }
    if (node->type != AST_SIMPLE) {
        fputs("mysh: executor: execution is not implemented yet\n", stderr);
        return 2;
    }
    if (node->redirection_count != 0){
        fputs("mysh: executor: redirections are not supported yet\n", stderr);
        return 2;
    }
    const ast_simple_t *command = node->data.simple;

pid_t pid = fork();
if (pid == -1){
    fprintf(stderr, "mysh: %s: fork: %s\n", command->argv[0], strerror(errno));
    return 1;
}

if (pid == 0){
    fprintf(stderr, "mysh: %s: execution is not implemented yet\n", command->argv[0]);
    _exit(2);
}

while (waitpid(pid, NULL, 0) == -1){
    if (errno == EINTR){
        continue;
    }
    fprintf(stderr, "mysh: %s: waitpid: %s\n", command->argv[0], strerror(errno));
    return 1;
}
return 2;
}

/* «Я вызываю fork() и различаю процессы по возвращённому значению. 
При минус единице обрабатываю ошибку создания. 
При нуле выполняю ветку ребёнка: пока он выводит диагностику 
и завершается через _exit(). Родитель ожидает конкретного ребёнка 
через waitpid(), повторяя ожидание при EINTR. 
Запуск внешней программы и извлечение её статуса добавлю следующими шагами». */


