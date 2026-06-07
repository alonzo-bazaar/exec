CC=gcc
CXX=g++
CFLAGS=-Wall -Wextra -Wpedantic -ggdb -g3 -O0
LIBFILES=exec.h

exec:
	$(CC) $(CFLAGS) -o exec main.c

execpp:
	$(CXX) $(CFLAGS) -o execpp main.cpp

all: exec execpp

.PHONY: all
