CC = gcc
CFLAGS = -Wall -std=gnu11
LDLIBS = -lpthread

all: server client

common.o: common.c common.h
	$(CC) $(CFLAGS) -c common.c -o common.o

libcommon.a: common.o
	ar rcs libcommon.a common.o

server: server.c common.h libcommon.a
	$(CC) $(CFLAGS) -o server server.c -L. -lcommon $(LDLIBS)

client: client.c common.h libcommon.a
	$(CC) $(CFLAGS) -o client client.c -L. -lcommon $(LDLIBS)

clean:
	rm -f server client *.o *.a

.PHONY: all clean