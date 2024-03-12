.PHONY: all install clean
CC=/usr/bin/gcc

LFLAGS=-ldaqhats

all: dh_logger dh_stop_scan dh_check_scan check_chunks

/usr/include/ini.h:
	@echo "Must install inih: apt install inih-dev"

/usr/local/include/daqhats/daqhats.h:
	@echo "Must install daqhats library to /usr/local"

%.o: %.c /usr/local/include/daqhats/daqhats.h
	$(CC) $(CFLAGS) -c $<

dh_logger: main.c wav.o /usr/include/ini.h
	$(CC) $(CFLAGS) $< wav.o -o $@ $(LFLAGS) -lm -linih

dh_stop_scan: stop_scan.c
	$(CC) $< -o $@ $(LFLAGS)

dh_check_scan: check_scan.c
	$(CC) $< -o $@ $(LFLAGS)

check_chunks: check_chunks.c
	$(CC) $< -o $@

install:
	mkdir -p $(HOME)/.config/systemd/user
	cp dh_logger.service $(HOME)/.config/systemd/user/

clean:
	rm -f dh_logger dh_stop_scan dh_check_scan check_chunks
	rm -f *.o
