CC = gcc
CFLAGS = -Wall -Wextra -std=c23 -D_POSIX_C_SOURCE=200809L
TARGET = myshell
SOURCES = src/main.c src/lexer.c src/parser.c src/ast.c

.PHONY: all debug clean test

all: $(TARGET)

$(TARGET): $(SOURCES) src/token.h src/lexer.h src/ast.h src/parser.h
	$(CC) $(CFLAGS) $(SOURCES) -o $@

debug:
	$(MAKE) clean
	$(MAKE) CFLAGS="$(CFLAGS) -g -fsanitize=address,undefined" $(TARGET)

test: $(TARGET)
	sh tests/test.sh ./$(TARGET)

clean:
	rm -f $(TARGET)
