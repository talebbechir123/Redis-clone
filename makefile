CC      = gcc
CFLAGS  = -g -Wall -Wextra -Wno-unused-parameter
OFLAGS  = -O2
SRCDIR  = src
INCDIR  = include

all: server client

# --- object files ---

hashtable.o: $(SRCDIR)/hashtable.c $(INCDIR)/hashtable.h
	$(CC) $(CFLAGS) $(OFLAGS) -c $(SRCDIR)/hashtable.c

replication.o: $(SRCDIR)/replication.c $(INCDIR)/replication.h $(INCDIR)/hashtable.h
	$(CC) $(CFLAGS) $(OFLAGS) -c $(SRCDIR)/replication.c

resp.o: $(SRCDIR)/resp.c $(INCDIR)/resp.h
	$(CC) $(CFLAGS) $(OFLAGS) -c $(SRCDIR)/resp.c

server.o: server.c $(INCDIR)/server.h $(INCDIR)/hashtable.h $(INCDIR)/resp.h $(INCDIR)/replication.h
	$(CC) $(CFLAGS) $(OFLAGS) -c server.c

client.o: $(SRCDIR)/client.c
	$(CC) $(CFLAGS) $(OFLAGS) -c $(SRCDIR)/client.c

# --- executables ---

server: server.o hashtable.o resp.o replication.o
	$(CC) $(CFLAGS) $(OFLAGS) -o server server.o hashtable.o resp.o replication.o

client: client.o
	$(CC) $(CFLAGS) $(OFLAGS) -o client client.o

# --- utilities ---

DB:
	mkdir -p htDB

clean:
	rm -f *.o server client
	rm -f htDB/*.db htDB/*.txt

.PHONY: all clean DB
