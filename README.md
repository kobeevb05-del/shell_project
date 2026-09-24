# Учебный shell: lexer и parser

Проект реализует лексический и синтаксический анализ командной строки с
построением AST. Выполнения команд пока нет.

## Сборка и проверка

```sh
make
make test
make debug
make clean
```

Обычная сборка использует GCC, C23, `-Wall -Wextra` и POSIX API. Цель `debug`
пересобирает программу с AddressSanitizer и UndefinedBehaviorSanitizer.

## Запуск

```sh
printf 'echo "hello world" | cat\n' | ./myshell --dump-tokens
printf 'echo hello && (pwd; echo done) > result.txt\n' | ./myshell --dump-ast
./myshell -c 'echo hello | cat'
```

Без аргументов программа читает команды из стандартного ввода и выводит AST.
Ошибки лексического и синтаксического анализа выводятся в stderr с позицией
символа.

Поддерживаются слова и кавычки, комментарии `#`, `|`, `&`, `&&`, `;`, `||`,
скобки subshell, перенаправления `<`, `>` и `>>`. Подстановки переменных,
glob, выполнение команд, pipeline execution, job control и встроенные команды
не реализованы.
