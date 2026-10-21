# OpenMac Stickies.app — build (original code, proprietary license - see ../../LICENSE)
CC      ?= cc
CFLAGS  ?= -O2 -Wall -Wextra
PREFIX  ?= $(HOME)/.omac

all: stickies

stickies: stickies.c
	$(CC) $(CFLAGS) -o $@ stickies.c

test: stickies
	./tests/run_tests.sh

install: stickies
	mkdir -p $(PREFIX)/bin $(PREFIX)/Applications/Stickies.app/Contents/MacOS
	cp stickies $(PREFIX)/bin/stickies
	cp stickies $(PREFIX)/Applications/Stickies.app/Contents/MacOS/Stickies

clean:
	rm -f stickies
