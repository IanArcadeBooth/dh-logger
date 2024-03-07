
CC=/usr/bin/gcc

LFLAGS=-ldaqhats

main: main.c
	$(CC) $(CFLAGS) $< -o $@ $(LFLAGS)

