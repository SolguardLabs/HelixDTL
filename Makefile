CC ?= cc
CFLAGS ?= -std=c11 -Wall -Wextra -Werror -O2 -g
CPPFLAGS ?= -Iinclude
SRC := $(wildcard src/*.c)
BIN := build/helixdtl
RISK_TEST_BIN := build/helix-risk-test

.PHONY: all test clean

all: $(BIN)

$(BIN): $(SRC) include/helix.h include/helix_risk.h
	mkdir -p build
	$(CC) $(CFLAGS) $(CPPFLAGS) $(SRC) -o $(BIN)

$(RISK_TEST_BIN): src/amount.c src/risk.c tests/c/risk_model_test.c include/helix.h include/helix_risk.h
	mkdir -p build
	$(CC) $(CFLAGS) $(CPPFLAGS) src/amount.c src/risk.c tests/c/risk_model_test.c -o $(RISK_TEST_BIN)

test: $(BIN) $(RISK_TEST_BIN)
	./$(RISK_TEST_BIN)
	node --test tests/node/*.test.js

clean:
	rm -rf build
