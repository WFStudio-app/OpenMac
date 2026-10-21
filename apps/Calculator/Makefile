# OpenMac Calculator.app — build (original code, proprietary license - see ../../LICENSE)
CC      ?= cc
CFLAGS  ?= -O2 -Wall -Wextra
PREFIX  ?= $(HOME)/.omac

all: calculator

calculator: main.c calc.c calc.h
	$(CC) $(CFLAGS) -o $@ main.c calc.c -lm

test: calculator
	./tests/run_tests.sh

install: calculator
	mkdir -p $(PREFIX)/bin $(PREFIX)/Applications/Calculator.app/Contents/MacOS
	cp calculator $(PREFIX)/bin/calculator
	cp calculator $(PREFIX)/Applications/Calculator.app/Contents/MacOS/Calculator

clean:
	rm -f calculator

.PHONY: all test install clean
