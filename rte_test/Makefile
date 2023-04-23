

all: rte_test
	@echo "build done"

uname_S := $(shell sh -c 'uname -s 2>/dev/null || echo not')
uname_M := $(shell sh -c 'uname -m 2>/dev/null || echo not')

ifeq ($(uname_S),Darwin)
CC = clang
CXX = clang++
FINAL_LDFLAGS+= -Wall -pthread  -rdynamic -ggdb -O3
else
CC = gcc
CXX = g++
FINAL_LDFLAGS+= -Wall -pthread -rdynamic -ggdb -fno-common -O3
endif

rte_test: main.c rte_ring.c rte_ring.h
	$(CC) main.c rte_ring.c -g -ggdb -ldl $(FINAL_LDFLAGS) -o $@

.PHONY clean:
	@rm -rf *.o
	@rm -rf rte_test