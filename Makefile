CC = gcc
CFLAGS = -Wall -Wextra -std=c2x -D_POSIX_C_SOURCE=200809L
TARGET = myshell


SRCS = src/main.c

all: $(TARGET)

$(TARGET): $(SRCS)
	$(CC) $(CFLAGS) $(SRCS) -o $(TARGET)

debug: CFLAGS += -g -fsanitize=address,undefined
debug: clean $(TARGET)

clean:
	rm -f $(TARGET)