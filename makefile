CC=gcc 
CFLAGS=-g -Wall

OFLAGS=-O3

FILES=  main.c server.c server.h

OBJDIR=obj

SRCDIR=src

INCDIR=include
# create a directory 


all: server client test DB


test.o: $(SRCDIR)/test.c $(INCDIR)/hashtable.h $(INCDIR)/replication.h
	$(CC) $(CFLAGS) $(OFLAGS) -c $(SRCDIR)/test.c


test: test.o hashtable.o replication.o
	$(CC) $(CFLAGS) $(OFLAGS) -o test test.o hashtable.o replication.o

replication.o: $(SRCDIR)/replication.c $(INCDIR)/replication.h
	$(CC) $(CFLAGS) $(OFLAGS) -c $(SRCDIR)/replication.c

hashtable.o: $(SRCDIR)/hashtable.c $(INCDIR)/hashtable.h
	$(CC) $(CFLAGS) $(OFLAGS) -c $(SRCDIR)/hashtable.c

server.o: server.c $(INCDIR)/server.h $(INCDIR)/hashtable.h $(INCDIR)/replication.h
	$(CC) $(CFLAGS) $(OFLAGS) -c server.c

server: server.o hashtable.o resp.o replication.o
	$(CC) $(CFLAGS) $(OFLAGS) -o server server.o hashtable.o resp.o replication.o

resp.o: $(SRCDIR)/resp.c $(INCDIR)/resp.h
	$(CC) $(CFLAGS) $(OFLAGS) -c $(SRCDIR)/resp.c
# client.h is in the include path
client.o: $(SRCDIR)/client.c $(INCDIR)/server.h 
	$(CC) $(CFLAGS) $(OFLAGS) -c $(SRCDIR)/client.c
#create a client executable htDB directory if it does not exist
DB:
	mkdir -p htDB
clean:
	rm -f main *.o *~ *# *.gch *.swp *.out server client test server_thread server2 htDB/*.txt

