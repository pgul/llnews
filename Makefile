CC = gcc
LINK = gcc
CFLAGS = -c -Wall -O2 -g -o $@
LFLAGS = -s -o $@ -lsocket -lresolv

all:	file2space waitack

file2space:	file2space.o sendbuf.o log.o
	$(LINK) $(LFLAGS) $^

waitack:	waitack.o log.o
	$(LINK) $(LFLAGS) $^

sendbuf.o:	sendbuf.c conf.h send.h
	$(CC) $(CFLAGS) $<

file2space.o:	file2space.c conf.h send.h
	$(CC) $(CFLAGS) $<

waitack.o:	waitack.c conf.h send.h
	$(CC) $(CFLAGS) $<

log.o:	log.c send.h
	$(CC) $(CFLAGS) $<

