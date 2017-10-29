# MY IMPLEMENTATION
CFLAGS=-D_POSIX_SOURCE -Wall -Werror -pedantic -std=c99 -D_GNU_SOURCE

# if a mac - use clang, else gcc
CLANG=$(shell which clang)
ifeq ($(CLANG), /usr/bin/clang)
	CC=clang
else
	CC=cc
endif

all: server

server: helper-functions.o server.o
	$(CC) $(CFLAGS) server.o helper-functions.o -o server

helper-functions.o: helper-functions.c
	$(CC) $(CFLAGS) -c helper-functions.c

server.o: server.c
	$(CC) $(CFLAGS) -c server.c

clean:
	rm -f *o server
