CC=gcc

CFLAGS = -g -c -Wall -pedantic

all: test mcast

test: test.o recv_dbg.o
	    $(CC) -o test test.o recv_dbg.o  

mcast: mcast.o
	    $(CC) -o mcast mcast.o   

clean:
	rm *.o
	rm test
	rm mcast

%.o:    %.c
	$(CC) $(CFLAGS) $*.c
