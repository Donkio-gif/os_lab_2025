CC = gcc
CFLAGS = -Wall -std=gnu11
LDLIBS = -lpthread

all: server client

server: server.c
	$(CC) $(CFLAGS) -o server server.c $(LDLIBS)

client: client.c
	$(CC) $(CFLAGS) -o client client.c $(LDLIBS)

clean:
	rm -f server client

.PHONY: all clean