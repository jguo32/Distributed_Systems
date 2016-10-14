CC=gcc

CFLAGS = -g -c -Wall -pedantic

all: test mcast start_mcast

test: test.o recv_dbg.o
	    $(CC) -o test test.o recv_dbg.o  

mcast: mcast.o
	    $(CC) -o mcast mcast.o   

start_mcast: start_mcast.o
	    $(CC) -o start_mcast start_mcast.o

clean:
	rm *.o
	rm test
	rm mcast
	rm start_mcast

%.o:    %.c
	$(CC) $(CFLAGS) $*.c
