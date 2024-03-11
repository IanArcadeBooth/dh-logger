
CC=/usr/bin/gcc

CFLAGS=-O0 -g -fsanitize=address,undefined

LFLAGS=-ldaqhats -lm

all: main stop_scan check_scan check_chunks

%.o: %.c
	$(CC) $(CFLAGS) -c $<

main: main.c wav.o
	$(CC) $(CFLAGS) $< wav.o -o $@ $(LFLAGS)

stop_scan: stop_scan.c
	$(CC) $< -o $@ $(LFLAGS)

check_scan: check_scan.c
	$(CC) $< -o $@ $(LFLAGS)

check_chunks: check_chunks.c
	$(CC) $< -o $@
