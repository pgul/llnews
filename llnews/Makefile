CC = gcc
LINK = gcc
CFLAGS = -c -Wall -O2 -s -o $@
LFLAGS = -s -o $@ # -lsocket

OBJECTS = llnews.o recvbuf.o log.o

all:	llnews

install:	llnews
	mkdir -p /usr/local/ll/bin
	mkdir -p /usr/local/ll/received
	chown news:news /usr/local/ll /usr/local/ll/bin /usr/local/ll/received
	cp llnews llrecv /usr/local/ll/bin
	chown news:news /usr/local/ll/bin/llnews /usr/local/ll/bin/llrecv
	chmod 755 /usr/local/ll/bin/llnews /usr/local/ll/bin/llrecv

clean:
	rm -f *.o

llnews:	$(OBJECTS)
	$(LINK) $(LFLAGS) $(OBJECTS)

recvbuf.o:	recvbuf.c conf.h recv.h
	$(CC) $(CFLAGS) $<

llnews.o:	llnews.c conf.h recv.h
	$(CC) $(CFLAGS) $<

log.o:	log.c conf.h recv.h
	$(CC) $(CFLAGS) $<

