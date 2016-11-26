CC=gcc
LD=gcc
CFLAGS=-g -Wall 
CPPFLAGS=-I. -I/home/jguo32/Distributed_Systems/project/include
SP_LIBRARY_DIR=/home/jguo32/Distributed_Systems/project

all: server client

.c.o:
	$(CC) $(CFLAGS) $(CPPFLAGS) -c $<

server:  $(SP_LIBRARY_DIR)/libspread-core.a server.o
	$(LD) -o $@ server.o  $(SP_LIBRARY_DIR)/libspread-core.a -ldl -lm -lrt -lnsl $(SP_LIBRARY_DIR)/libspread-util.a

client:  $(SP_LIBRARY_DIR)/libspread-core.a client.o
	$(LD) -o $@ client.o  $(SP_LIBRARY_DIR)/libspread-core.a -ldl -lm -lrt -lnsl $(SP_LIBRARY_DIR)/libspread-util.a

clean:
	rm -f *.o server client

