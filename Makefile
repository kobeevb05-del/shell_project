CC = gcc
CFLAGS = -Wall -Wextra -std=c23 -D_POSIX_C_SOURCE=200809L
TARGET = myshell
SOURCES = src/main.c src/lexer.c src/ast.c src/parser.c
HEADERS = src/token.h src/lexer.h src/ast.h src/parser.h

.PHONY: all debug clean test

all: $(TARGET)

$(TARGET): $(SOURCES) $(HEADERS)
	$(CC) $(CFLAGS) $(SOURCES) -o $@

debug:
	$(MAKE) clean
	$(MAKE) CFLAGS="$(CFLAGS) -g -fsanitize=address,undefined" $(TARGET)

test: $(TARGET)
	sh tests/test.sh ./$(TARGET)

clean:
	rm -f $(TARGET) myshell_dbg
