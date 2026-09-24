#!/bin/sh
set -eu

program=${1:-./myshell}
tmp=${TMPDIR:-/tmp}/myshell-test-$$
trap 'rm -f "$tmp"' EXIT HUP INT TERM

check_ast() {
    input=$1
    pattern=$2
    "$program" -c "$input" >"$tmp"
    grep -F "$pattern" "$tmp" >/dev/null
}

check_tokens() {
    input=$1
    pattern=$2
    printf '%s\n' "$input" | "$program" --dump-tokens >"$tmp"
    grep -F "$pattern" "$tmp" >/dev/null
}

check_error() {
    input=$1
    if "$program" -c "$input" >"$tmp" 2>&1; then
        echo "expected failure for: $input" >&2
        exit 1
    fi
}

check_tokens 'echo hello' 'WORD          "hello"'
check_tokens 'echo "hello world"' 'WORD          "hello world"'
check_tokens "echo 'a|b'" 'WORD          "a|b"'
check_tokens 'echo a"b c"d' 'WORD          "ab cd"'
check_tokens "echo \"\" ''" 'WORD          ""'
check_tokens 'echo a\ b' 'WORD          "a b"'
check_tokens 'echo a && echo b || echo c' 'AND'
check_tokens 'echo a && echo b || echo c' 'OR'
check_tokens 'echo hi | cat' 'PIPE'
check_tokens 'echo hi >>out' 'REDIR_APPEND'
check_tokens 'echo in <infile' 'REDIR_IN'
check_tokens 'echo hi # comment' 'EOF'
check_tokens 'echo "say \\\"hi\\\""' 'WORD          "say \\\"hi\\\""'
check_tokens 'echo "x\\q"' 'WORD          "x\\q"'
check_tokens 'echo "x\q"' 'WORD          "x\\q"'
check_tokens "echo 'x\\\\y'" 'WORD          "x\\\\y"'

check_ast 'echo hello' 'Word "hello"'
check_ast 'echo "hello world"' 'Word "hello world"'
check_ast "echo 'a|b'" 'Word "a|b"'
check_ast 'echo a"b c"d' 'Word "ab cd"'
check_ast "echo \"\" ''" 'Word ""'
check_ast 'echo a\ b' 'Word "a b"'
check_ast 'echo a && echo b || echo c' '&&'
check_ast 'echo a && echo b || echo c' '||'
check_ast 'echo hi | cat' 'Pipeline'
check_ast '>out echo hi' 'Redirection > out'
check_ast 'echo hi >>out' 'Redirection >> out'
check_ast '(echo one; echo two) >out' 'Subshell'
check_ast 'echo hi # comment' 'Word "hi"'
check_tokens 'echo one & echo two' 'BACKGROUND'
check_ast 'echo one & echo two' '&'
check_ast 'echo one;;echo two' 'Empty'

check_error "echo 'unclosed"
check_error 'echo "unclosed'
check_error '| echo'
check_error 'echo &&'
check_error 'echo >'
check_error '(echo'
check_error 'echo)'
check_error 'echo |'

echo 'lexer/parser tests passed'
