#!/bin/sh
# Проверяем весь поток токенов, позиции и диагностику лексера.
set -eu

program=${1:-./myshell}
tmp=$(mktemp "${TMPDIR:-/tmp}/myshell-test.XXXXXX")
trap 'rm -f "$tmp" "$tmp.actual"' 0
trap 'exit 1' HUP INT TERM

check_tokens() {
    "$program" -c "$1" >"$tmp"
    # Убираем только выравнивание столбцов, сохраняя пробелы внутри слов.
    sed -E 's/^([0-9]+: [A-Z_]+) +/\1 /; s/ +$//' "$tmp" >"$tmp.actual"
    diff -u - "$tmp.actual"
}

check_error() {
    status=0
    "$program" -c "$1" >"$tmp" 2>&1 || status=$?
    if [ "$status" -ne 2 ]; then
        printf 'Expected exit code 2, got %s for: %s\n' "$status" "$1" >&2
        return 1
    fi
    grep -F "$2" "$tmp" >/dev/null
}

# empty input
check_tokens '' <<'TOKENS'
1: EOF
TOKENS

# adjacent word segments
check_tokens 'echo a"b c"d' <<'TOKENS'
1: WORD "echo"
6: WORD "ab cd"
13: EOF
TOKENS

# two empty arguments
check_tokens 'echo "" '\'''\''' <<'TOKENS'
1: WORD "echo"
6: WORD ""
9: WORD ""
11: EOF
TOKENS

# comment at word boundary
check_tokens 'echo a#b # ignored
' <<'TOKENS'
1: WORD "echo"
6: WORD "a#b"
19: NEWLINE
20: EOF
TOKENS

# quoted and escaped hash
check_tokens 'echo ""#abc \#def' <<'TOKENS'
1: WORD "echo"
6: WORD "#abc"
13: WORD "#def"
18: EOF
TOKENS

# double quotes preserve other escapes
check_tokens 'echo "a\tb"' <<'TOKENS'
1: WORD "echo"
6: WORD "a\\tb"
12: EOF
TOKENS

# double quote and backslash escapes
check_tokens 'echo "a\"b\\c"' <<'TOKENS'
1: WORD "echo"
6: WORD "a\"b\\c"
15: EOF
TOKENS

# single quotes are literal
check_tokens 'echo '\''a\b|c'\''' <<'TOKENS'
1: WORD "echo"
6: WORD "a\\b|c"
13: EOF
TOKENS

# all operators
check_tokens '| & ; && || ( ) < > >>' <<'TOKENS'
1: PIPE "|"
3: BACKGROUND "&"
5: SEQUENCE ";"
7: AND "&&"
10: OR "||"
13: LPAREN "("
15: RPAREN ")"
17: REDIR_IN "<"
19: REDIR_OUT ">"
21: REDIR_APPEND ">>"
23: EOF
TOKENS

# empty line
check_tokens 'a

b' <<'TOKENS'
1: WORD "a"
2: NEWLINE
3: NEWLINE
4: WORD "b"
5: EOF
TOKENS

# word continuation
check_tokens 'echo a\
b' <<'TOKENS'
1: WORD "echo"
6: WORD "ab"
10: EOF
TOKENS

# operator continuation
check_tokens 'a &\
& b' <<'TOKENS'
1: WORD "a"
3: AND "&&"
8: WORD "b"
9: EOF
TOKENS

# comment after continuation
check_tokens 'echo \
# comment
x' <<'TOKENS'
1: WORD "echo"
17: NEWLINE
18: WORD "x"
19: EOF
TOKENS

# quoted newline
check_tokens 'echo '\''a
b'\''' <<'TOKENS'
1: WORD "echo"
6: WORD "a\nb"
11: EOF
TOKENS

# double quote continuation
check_tokens 'echo "a\
b"' <<'TOKENS'
1: WORD "echo"
6: WORD "ab"
12: EOF
TOKENS

# buffer growth
check_tokens 'aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa' <<'TOKENS'
1: WORD "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
129: EOF
TOKENS

# token array growth
check_tokens 'x x x x x x x x x x x x x x x x x x' <<'TOKENS'
1: WORD "x"
3: WORD "x"
5: WORD "x"
7: WORD "x"
9: WORD "x"
11: WORD "x"
13: WORD "x"
15: WORD "x"
17: WORD "x"
19: WORD "x"
21: WORD "x"
23: WORD "x"
25: WORD "x"
27: WORD "x"
29: WORD "x"
31: WORD "x"
33: WORD "x"
35: WORD "x"
36: EOF
TOKENS

check_error 'echo '\''x' 'byte 6: unterminated single quote'
check_error 'echo "x' 'byte 6: unterminated double quote'
check_error 'echo \' 'byte 6: backslash at end of input'

echo 'lexer tests passed'
