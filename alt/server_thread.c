#include "../include/server.h"
#include "../include/resp.h"
#include <pthread.h>
#include <fcntl.h>
#include <signal.h>

#define BUF_SIZE 1024

hashtable *ht;
int server_init(int port){
    int socket_fd;
    struct sockaddr_in server_addr;
    int opt = 1;

    if((socket_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0){
        perror("socket failed");
        exit(EXIT_FAILURE);
    }
    
    if (setsockopt(socket_fd, SOL_SOCKET, SO_REUSEADDR , &opt, sizeof(opt))){
        perror("setsockopt");
        exit(EXIT_FAILURE);
    }
    //fcntl(socket_fd, F_SETFL, O_NONBLOCK);

    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(port);

    if(bind(socket_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0){
        perror("bind failed");
        exit(EXIT_FAILURE);
    }
    printf("Server listening on port %d\n", port);



    return socket_fd;
}


//function that handles the request from the client 
void request_handler(char * request, hashtable *ht,char* buf, int rnum, RespRequest *req, RespResponse *res){

		if (decode_request(req, buf, rnum) == 0 && 
			(req->state == OK || req->state == PART_OK) ){
			if (req->argc > 1 && strncmp(request_argv(req,0), "add", 3) == 0) {
				int total = 0;
				for (int i = 1; i < req->argc; i++)
					total += atoi(request_argv(req,i));
				encode_response_integer(res, total);
			}else if (req->argc > 1 && strncmp(request_argv(req,0), "set", 3) == 0) {
                hashtable_set(ht, request_argv(req,1), request_argv(req,2));
                encode_response_status(res,1,"OK");
            }else if (req->argc ==1 && strncmp(request_argv(req,0), "PING", 3) == 0) {
                encode_response_status(res,1,"PONG");
            }else if (req->argc =2 && strncmp(request_argv(req,0), "get", 3) == 0) {
                 // get the value from the hashtable of the key
                 // key is the second argument
                char* key = request_argv(req,1);
               printf("Key: %s\n", key);
               char *value = hashtable_get(ht, key);
                //
               printf("Value: %s\n", value);
                // 
                encode_response_string(res, hashtable_get(ht, key), strlen(hashtable_get(ht, key)));

                // print all the keys and values in the hashtable
                 //hashtable_print(ht);

               // encode_response_string(res, value, strlen(value));
            }else if (req->argc ==1 && strncmp(request_argv(req,0), "QUIT", 3) == 0) {
                encode_response_status(res,1,"OK");
               
            
			}else if (req->argc =2 && strncmp(request_argv(req,0), "DEL", 3) == 0) {
                hashtable_delete(ht, request_argv(req,1));
                encode_response_status(res,1,"OK");
            
            } else {
				encode_response_status(res,0,"ERR unknown command");
			}
		}
		else {
			encode_response_status(res,0,"ERR decode command fail");
		}
   
}

//create an object with the client request and response and socket id
typedef struct client{
    int socket_id;
    RespRequest *req;
    RespResponse *res;
    char *buf;
    int rnum;
}client;

void* Client_thread_handler(void* param){
    // get the client object
    client *c = (client*)param;
    // get the socket id
    int socket_id = c->socket_id;
    // get the request object
    RespRequest *req = c->req;
    // get the response object
    RespResponse *res = c->res;
    // get the buffer
    char *buf = c->buf;
    // get the number of bytes read
    int rnum = c->rnum;
    // write to the hashtable
    request_handler(buf, ht, buf, rnum, req, res);
    // write to the client
    write(socket_id, res->buf, res->used_size);
    
    // close the socket
    close(socket_id);
}

int main(int argc , char **argv){

    int PORT = atoi(argv[1]);
    int socket_fd; 
    int CLIENTS[MAXCLIENTS];
    int client_count = 0;
    int max_fd;

    //initialize all client sockets to 0
    for(int i = 0; i < MAXCLIENTS; i++){
        CLIENTS[i] = 0;
    }

    //initialize server socket
    socket_fd = server_init(PORT);

    //listen for connections
    if(listen(socket_fd, 3) < 0){
        perror("listen");
        exit(EXIT_FAILURE);
    }

    //accept connections
    int addrlen = sizeof(struct sockaddr_in);
    struct sockaddr_in client_addr;
    int new_socket;
    // create a hashtable
    hashtable *ht = hashtable_new(65536);
    RespRequest *req = create_request(BUF_SIZE);
	RespResponse *res = create_response(BUF_SIZE);
	while (1) {
		reset_request(req);
		reset_response(res);	

		socklen_t addr_len = sizeof(struct sockaddr_in);
		int cfd = accept(socket_fd, (struct sockaddr*)&client_addr, &addr_len);
		if (cfd < 0) {
			perror("accept()");
			continue;
		}
        //check for O_NONBLOCK
		char buf[1024];
        // create a client object
        client *c = malloc(sizeof(client));
        c->socket_id = cfd;
        c->req = req;
        c->res = res;
		int rnum = read(cfd,buf,sizeof(buf));
        c->buf = buf;
        c->rnum = rnum;

        //
        pthread_t tid;
        pthread_create(&tid, NULL, Client_thread_handler, c);
        pthread_join(tid, NULL);
        // printf thread id
        //
        printf("Thread id: %ld\n", tid);

       // request_handler(buf, ht, buf, rnum, req, res);
		//write(cfd, res->buf, res->used_size);
	    //	close(cfd);
	}
	destroy_request(req);
	destroy_response(res);

	close(socket_fd);


return 0;
}

