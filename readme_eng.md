### AISE project report
### Group members
-1 Ahmed taleb BECHIR

-2 Valentin DENIS

### Implementation choices

## project structure
we create a TCP server on port 6379, the server also takes a replication interval argument from the user, which it uses to set a global variable meant to indicate the size at which the server saves the hastable as a JSON file into a directory called htDB 

the server does that with the help of signals, a signal SIGUSR1,
SIGUSR1 is triggered by the client_handler function, which raises the signal when HashINSERTS hits the replication threshold.
The signal handler function then set DATABASE_SAVE_FLAG to 1.

The server operates on two loops, one that's infinite and another one that checks whether DATABASE_SAVE_FLAG is set to 1 or 0.
We chose this implementation because we didn't want to declare our hashtable as a global variable and then use the signal handler function to save it, which would have had some undefined behavior, and we needed to make sure our hashtable is safe from interference and overboard behavior.

As for multiplexing, we chose to use select for being able to handle multiple clients. Select allowed us to avoid using threads and avoid degraded performance in the case of multiple concurrent clients. 

As for the atomicity of requests, we defined two objects, a request and a response object. At the start of server we create the objects and once we start the connection on the socket and start accepting clients, if clients send a request, we first reset the object, and then we read the client request and parse it using our RESP parser and set the request object with corresponding response, then in turn we send the appropriate response.

When it comes to the replication of data, we implemented a method to allow a user at the start of the server whether they'd want to restore the latest save of the database and if not to create a new clean hastable.


# include folder 

-1 client.h
-2 server.h
-3 resp.h
-4 hastable.h
-5 replication.h

# src folder

-1 client.c
-2 server.c
-3 resp.c
-4 hastable.c
-5 replication.c

# htDB folder

contains the saved database files

# Resource folder
contains help text files

# Makefile

make server : to compile the server
make client : to compile the client
make clean : to delete object files and executable files
make DB : create htDB directory

# Commands implemented 
set      save

get      copy

ping    Exists

del      Help

quit     Echo

time    INFO

save   


#### References

https://h-digitalbusiness.com/multi-threading-and-multiplexing-in-c/
https://beej.us/guide/bgnet/html/
https://github.com/interma/RESP/tree/master
https://amitshekhar.me/blog/resp-redis-serialization-protocol
https://dzone.com/articles/parallel-tcpip-socket-server-with-multi-threading
https://redis.io/docs/reference/modules/
https://medium.com/@daijue/the-basics-of-replication-in-redis-4b92a3b275bd



```

