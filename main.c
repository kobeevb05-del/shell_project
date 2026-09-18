#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(void) {
    char *line = NULL;       /* Указатель на буфер строки */
    size_t len = 0;          /* Размер выделенного буфера */
    ssize_t read_bytes;      /* Сколько байт реально прочитано */

    /* Интерактивный цикл REPL */
    while (1) {
        printf("mysh> ");
        fflush(stdout); /* Принудительно выводим приглашение на экран */

        read_bytes = getline(&line, &len, stdin);
        if (read_bytes == -1) {
            /* Если произошла ошибка или достигнут конец файла */
            printf("\nОшибка чтения или конец файла. Выход.\n");
            break;
        }
        /* Если пользователь ввел exit (учитываем символ переноса строки \n) */
        if (strcmp(line, "exit\n") == 0) {
            break;
        }

        /* Пока мы просто печатаем длину введенной строки и саму строку */
        printf("Прочитано байт: %zd. Ввод: %s", read_bytes, line);
    }

    /* Освобождаем память, которую выделил getline */
    free(line);
    
    return 0;
}