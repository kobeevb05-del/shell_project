#!/bin/sh
# В каждом примере отступ показывает, какому узлу принадлежит следующая строка.
set -eu

program=${1:-./myshell}
tmpdir=$(mktemp -d "${TMPDIR:-/tmp}/myshell-ast.XXXXXX")
trap 'rm -rf "$tmpdir"' 0
trap 'exit 1' HUP INT TERM

check_ast() {
    cat >"$tmpdir/expected"
    "$program" --dump-ast -c "$1" >"$tmpdir/actual"
    diff -u "$tmpdir/expected" "$tmpdir/actual"

    printf '%s' "$1" | "$program" --dump-ast >"$tmpdir/actual"
    diff -u "$tmpdir/expected" "$tmpdir/actual"
}

check_error() {
    phase=$1
    position=$2
    shift 2
    status=0
    "$program" "$@" >"$tmpdir/actual" 2>"$tmpdir/error" || status=$?
    if [ "$status" -ne 2 ] || [ -s "$tmpdir/actual" ] ||
       ! grep -F "mysh: $phase: byte $position:" "$tmpdir/error" >/dev/null; then
        printf 'Expected %s error at byte %s, exit 2 and empty stdout for [%s]. Got exit %s.\n' \
            "$phase" "$position" "$*" "$status" >&2
        cat "$tmpdir/actual" "$tmpdir/error" >&2
        exit 1
    fi
}

check_syntax_error() {
    check_error parser "$2" --dump-ast -c "$1"
}

check_ast '' <<'TREE'
EMPTY
TREE

check_ast '# только комментарий

' <<'TREE'
EMPTY
TREE

# Содержимое каждого аргумента различимо, включая пустое слово и перевод строки.
check_ast 'echo "" a"b c"d "q\"\\x" "#" "line
two"' <<'TREE'
SIMPLE "echo" "" "ab cd" "q\"\\x" "#" "line\ntwo"
TREE

check_ast '> "one two" echo <in x >>out y' <<'TREE'
SIMPLE "echo" "x" "y"
  REDIR > "one two"
  REDIR < "in"
  REDIR >> "out"
TREE

# Цифра в кавычках или перед пробелом — обычный аргумент.
check_ast 'echo "2">file 2 >out' <<'TREE'
SIMPLE "echo" "2" "2"
  REDIR > "file"
  REDIR > "out"
TREE

check_ast 'a | b | c' <<'TREE'
PIPE
  PIPE
    SIMPLE "a"
    SIMPLE "b"
  SIMPLE "c"
TREE

# a || b && c разбирается как (a || b) && c; конвейер связывается сильнее.
check_ast 'a || b && c | d; e' <<'TREE'
SEQUENCE
  AND
    OR
      SIMPLE "a"
      SIMPLE "b"
    PIPE
      SIMPLE "c"
      SIMPLE "d"
  SIMPLE "e"
TREE

# Здесь в фон уходит только b && c. Команда a уже была отделена точкой с запятой.
check_ast 'a; b && c & d' <<'TREE'
SEQUENCE
  SEQUENCE
    SIMPLE "a"
    BACKGROUND
      AND
        SIMPLE "b"
        SIMPLE "c"
  SIMPLE "d"
TREE

check_ast 'a &' <<'TREE'
BACKGROUND
  SIMPLE "a"
TREE

check_ast '
a; # следующая команда на новой строке

b;
' <<'TREE'
SEQUENCE
  SIMPLE "a"
  SIMPLE "b"
TREE

check_ast '((a; b) >out) <in >>log | c' <<'TREE'
PIPE
  SUBSHELL
    SUBSHELL
      SEQUENCE
        SIMPLE "a"
        SIMPLE "b"
      REDIR > "out"
    REDIR < "in"
    REDIR >> "log"
  SIMPLE "c"
TREE

# Во время построения и печати дерева файл не должен быть открыт на запись.
"$program" --dump-ast -c "echo hi > \"$tmpdir/side-effect\"" >/dev/null
test ! -e "$tmpdir/side-effect"

check_syntax_error '| a' 1
check_syntax_error 'a |' 4
check_syntax_error 'a &&' 5
check_syntax_error 'a || )' 6
check_syntax_error 'a | | b' 5
check_syntax_error 'a >' 4
check_syntax_error 'a >> | b' 6
check_syntax_error '>out' 5
check_syntax_error '(a) <' 6
check_syntax_error '()' 2
check_syntax_error '(a' 3
check_syntax_error 'a)' 2
check_syntax_error '(a)b' 4
check_syntax_error 'a(b)' 2
check_syntax_error 'ok
bad |' 9
check_syntax_error 'cat >/dev/tcp/host/80' 6

check_error lexer 8 --dump-ast -c 'echo a 2>f'
check_error lexer 8 --dump-ast -c 'echo x &>f next'
check_error lexer 1 --dump-ast -c '{fd}>f cmd'

# До появления исполнителя обычный -c печатает токены только после проверки грамматики.
"$program" --dump-tokens -c 'echo hi' >"$tmpdir/expected"
"$program" -c 'echo hi' >"$tmpdir/actual"
diff -u "$tmpdir/expected" "$tmpdir/actual"
check_error parser 7 -c 'echo |'

echo 'AST tests passed'
