# OpenMac osfmk-shim build (original code, proprietary license - see ../LICENSE)
CC      ?= cc
CXX     ?= c++
CFLAGS  ?= -O2 -Wall -fPIC
CXXFLAGS?= -O2 -Wall -fPIC -std=c++17
PREFIX  ?= $(HOME)/.omac

LIB_SRCS = mach_shim.c mach_ipc.c mach_task.c
LIB_OBJS = $(LIB_SRCS:.c=.o)
CPP_LIB_SRC = cpp/OMKernel.cpp
CPP_LIB_OBJ = $(CPP_LIB_SRC:.cpp=.o)

all: libopenmach.so libopenmachcpp.so ioreg_cpp

libopenmach.so: $(LIB_OBJS)
	$(CC) -shared -o $@ $^

%.o: %.c mach.h
	$(CC) $(CFLAGS) -c -o $@ $<

cpp/%.o: cpp/%.cpp cpp/OMKernel.hpp mach.h
	$(CXX) $(CXXFLAGS) -c -o $@ $<

libopenmachcpp.so: $(CPP_LIB_OBJ) libopenmach.so
	$(CXX) -shared -o $@ $(CPP_LIB_OBJ) -L. -lopenmach -Wl,-rpath,'$$ORIGIN'

ioreg_cpp: cpp/ioreg_main.cpp cpp/OMIOKit.hpp libopenmachcpp.so
	$(CXX) $(CXXFLAGS) -o $@ cpp/ioreg_main.cpp -L. -lopenmachcpp -lopenmach -Wl,-rpath,$(CURDIR)

test_mach: tests/test_mach.c mach.h libopenmach.so
	$(CC) $(CFLAGS) -o $@ tests/test_mach.c -L. -lopenmach -Wl,-rpath,$(CURDIR)

test_stress: tests/test_stress.c mach.h libopenmach.so
	$(CC) $(CFLAGS) -o $@ tests/test_stress.c -L. -lopenmach -Wl,-rpath,$(CURDIR)

test_task: tests/test_task.c mach.h libopenmach.so
	$(CC) $(CFLAGS) -o $@ tests/test_task.c -L. -lopenmach -Wl,-rpath,$(CURDIR) -lpthread

test_cpp: tests/test_cpp.cpp $(CPP_LIB_OBJ) libopenmach.so
	$(CXX) $(CXXFLAGS) -o $@ tests/test_cpp.cpp $(CPP_LIB_OBJ) -L. -lopenmach -Wl,-rpath,$(CURDIR)

test: all test_mach test_task test_cpp
	OMAC_IPC_DIR=$${OMAC_IPC_DIR:-/tmp/omach-test} ./test_mach
	OMAC_IPC_DIR=$${OMAC_IPC_DIR:-/tmp/omach-test} ./test_task
	./test_cpp

stress: all test_stress
OMAC_IPC_DIR=$${OMAC_IPC_DIR:-/tmp/omach-stress-ipc} ./test_stress $${STRESS_SECONDS:-60}

install: all
	mkdir -p $(PREFIX)/lib $(PREFIX)/include $(PREFIX)/libexec
	cp libopenmach.so libopenmachcpp.so $(PREFIX)/lib/
	cp mach.h $(PREFIX)/include/
	cp cpp/*.hpp $(PREFIX)/include/
	cp ioreg_cpp $(PREFIX)/libexec/

clean:
	rm -f *.o libopenmach.so libopenmachcpp.so test_mach test_stress test_task test_cpp ioreg_cpp cpp/*.o

.PHONY: all test install clean stress
