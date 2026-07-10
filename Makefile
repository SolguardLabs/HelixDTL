CC ?= cc
CFLAGS ?= -std=c11 -Wall -Wextra -Werror -O2 -g
CPPFLAGS ?= -Iinclude
SRC := $(wildcard src/*.c)
BIN := build/helixdtl

.PHONY: all test clean

all: $(BIN)

$(BIN): $(SRC) include/helix.h
	mkdir -p build
	$(CC) $(CFLAGS) $(CPPFLAGS) $(SRC) -o $(BIN)

test: $(BIN)
	node --test tests/node/*.test.js

clean:
	rm -rf build
