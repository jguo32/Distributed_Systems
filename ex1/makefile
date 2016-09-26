CC=gcc

CFLAGS = -g -c -Wall -pedantic

all: test ncp rcv

test: test.o sendto_dbg.o
	    $(CC) -o test test.o sendto_dbg.o  

ncp: ncp.o
	    $(CC) -o ncp ncp.o

rcv: rcv.o
	    $(CC) -o rcv rcv.o

clean:
	rm *.o
	rm test
	rm ncp
	rm rcv
clear:
	rm copy*

%.o:    %.c
	$(CC) $(CFLAGS) $*.c
