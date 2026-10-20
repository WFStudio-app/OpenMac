# OpenMac osfmk-shim build (original code, proprietary license - see ../LICENSE)
CC      ?= cc
CFLAGS  ?= -O2 -Wall -fPIC
PREFIX  ?= $(HOME)/.omac

LIB_SRCS = mach_shim.c mach_ipc.c
LIB_OBJS = $(LIB_SRCS:.c=.o)

all: libopenmach.so

libopenmach.so: $(LIB_OBJS)
	$(CC) -shared -o $@ $^

%.o: %.c mach.h
	$(CC) $(CFLAGS) -c -o $@ $<

test_mach: tests/test_mach.c mach.h libopenmach.so
	$(CC) $(CFLAGS) -o $@ tests/test_mach.c -L. -lopenmach -Wl,-rpath,$(CURDIR)

test_stress: tests/test_stress.c mach.h libopenmach.so
	$(CC) $(CFLAGS) -o $@ tests/test_stress.c -L. -lopenmach -Wl,-rpath,$(CURDIR)

test: all test_mach
	./test_mach

stress: all test_stress
	OMAC_IPC_DIR=$${OMAC_IPC_DIR:-/tmp/omach-stress-ipc} ./test_stress $${STRESS_SECONDS:-60}

install: all
	mkdir -p $(PREFIX)/lib $(PREFIX)/include
	cp libopenmach.so $(PREFIX)/lib/
	cp mach.h $(PREFIX)/include/

clean:
	rm -f *.o libopenmach.so test_mach test_stress

.PHONY: all test install clean stress
