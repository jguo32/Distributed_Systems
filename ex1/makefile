CC=gcc

CFLAGS = -g -c -Wall -pedantic

all: test ncp rcv t_ncp t_rcv

test: test.o sendto_dbg.o
	    $(CC) -o test test.o sendto_dbg.o  

ncp: ncp.o
	    $(CC) -o ncp ncp.o

rcv: rcv.o
	    $(CC) -o rcv rcv.o

t_ncp: t_ncp.o
	    $(CC) -o t_ncp t_ncp.o

t_rcv: t_rcv.o
	    $(CC) -o t_rcv t_rcv.o

clean:
	rm *.o
	rm test
	rm ncp
	rm rcv
	rm t_ncp
	rm t_rcv

clear:
	rm copy*

%.o:    %.c
	$(CC) $(CFLAGS) $*.c
