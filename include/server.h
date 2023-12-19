#ifndef __SERVER_H__
#define __SERVER_H__


#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <unistd.h>
#include "hashtable.h"

#define SA struct sockaddr
#define SAI struct sockaddr_in
#define TV struct timeval


#define CHECK(x)                                            \
    do                                                      \
    {                                                       \
        if (!(x))                                           \
        {                                                   \
            fprintf(stderr, "%s:%d: ", __func__, __LINE__); \
            perror(#x);                                     \
            exit(EXIT_FAILURE);                             \
        }                                                   \
    } while (0)




#define MAXCLIENTS 100



// a struct for request object 

typedef struct request {
    char *data;
    char *type;
} request_t;

// a struct for response object
typedef struct response
{
    char *data;
    char *type;

} response_t;


int server_init(int port);

char * response( char * request);





#endif