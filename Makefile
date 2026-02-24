CC := cc
CFLAGS := -std=c99 -Wall -Wextra -Werror -pedantic -D_POSIX_C_SOURCE=200809L -Iinclude
LDFLAGS :=

SRC := $(wildcard src/*.c)
OBJ := $(patsubst src/%.c,build/%.o,$(SRC))
TARGET := mdnsd

.PHONY: all clean install

all: $(TARGET)

build:
	mkdir -p build

$(TARGET): build $(OBJ)
	$(CC) $(OBJ) -o $@ $(LDFLAGS)

build/%.o: src/%.c
	$(CC) $(CFLAGS) -c $< -o $@

install: $(TARGET)
	install -m 0755 $(TARGET) /usr/local/bin/$(TARGET)

clean:
	rm -rf build $(TARGET)
