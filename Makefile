TARGET=netns

CFLAGS=-std=c99 -pedantic -Wall -Wextra -Werror -O2 -g
CPPFLAGS=-D_GNU_SOURCE
LDFLAGS=

SRC=$(wildcard *.c)
OBJ=$(SRC:%.c=%.o)

all: $(TARGET)

$(TARGET): $(OBJ)
	$(CC) $^ -o $@

clean:
	rm -rf  $(TARGET) $(OBJ)

.PHONY: all clean
