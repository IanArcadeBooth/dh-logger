
CC=/usr/bin/gcc

CFLAGS=-O0 -g -fsanitize=address,undefined

LFLAGS=-ldaqhats

all: dh_logger dh_stop_scan dh_check_scan check_chunks

%.o: %.c
	$(CC) $(CFLAGS) -c $<

dh_logger: main.c wav.o
	$(CC) $(CFLAGS) $< wav.o -o $@ $(LFLAGS) -lm -linih

dh_stop_scan: stop_scan.c
	$(CC) $< -o $@ $(LFLAGS)

dh_check_scan: check_scan.c
	$(CC) $< -o $@ $(LFLAGS)

check_chunks: check_chunks.c
	$(CC) $< -o $@

clean:
	rm -f dh_logger dh_stop_scan dh_check_scan check_chunks
	rm -f *.o
