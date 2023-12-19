#include "../include/server.h"
#include "../include/resp.h"
#include <fcntl.h>
#include <signal.h>
#include "../include/replication.h"
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
    fcntl(socket_fd, F_SETFL, O_NONBLOCK);

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



// function that handles the response to the client
char * response( char * request){

    if (strncmp(request ,"PING",3) == 0){
        return "+PONG";

    }else if(strncmp(request ,"ECHO",3) == 0){
        return "+ECHO\r\n";
    
    }else if(strncmp(request, "TIME",3) == 0){
        return "+TIME\r\n";
    
    }else if(strncmp(request, "QUIT",3) == 0){
        return "+OK\r\n";
    
    }else if(strncmp(request, "SET",2) == 0){
        return "+OK\r\n";
    
    }else return "-ERR\r\n";

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
                raise(SIGUSR1);
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

//create a signal that will export the hashtable to a file every 10 inserts
void sig_handler(int signo){
    if(signo == SIGUSR1){
        printf("Received SIGUSR1\n");
     // open directory
        // if directory does not exist, create it
        // if directory exists, open it
        // create a file with the timestamp
        // export the hashtable to the file
        // close the file
        // close the directory
        hashtable_replicate(ht, "hashtable.txt");
    }
}

int main(int argc , char **argv){

    int PORT = atoi(argv[1]);
    int socket_fd; 
    int CLIENTS[MAXCLIENTS];
    int client_count = 0;
    int max_fd;
    fd_set read_fds;
    fd_set write_fds;
    fd_set except_fds;
    struct timeval tv;
    //initialize server socket
    socket_fd = server_init(PORT);

    //listen for connections
    if(listen(socket_fd, 10) < 0){
        perror("listen");
        exit(EXIT_FAILURE);
    }

    //accept connections
    int addrlen = sizeof(struct sockaddr_in);
    struct sockaddr_in client_addr;
    int new_socket;
    struct sigaction act;
    act.sa_handler = sig_handler;
    sigemptyset(&act.sa_mask);
    act.sa_flags = 0;
    sigaction(SIGUSR1, &act, NULL);
    // create a hashtable
    ht = hashtable_new(65536);
    RespRequest *req = create_request(BUF_SIZE);
	RespResponse *res = create_response(BUF_SIZE);
    //// Set up file descriptors for select()
    FD_ZERO(&read_fds);
    FD_SET(socket_fd, &read_fds);
    max_fd = socket_fd;
    tv.tv_sec = 0;
    tv.tv_usec = 100000;



	while (1) {

        // Wait for incoming connections or data
        fd_set tmp_fds = read_fds;
        int ret = select(max_fd+1, &tmp_fds, NULL, NULL, &tv);
        if (ret < 0) {
            perror("select()");
            exit(EXIT_FAILURE);
        } else if (ret == 0) {
            // Timeout
            continue;
        }

        
		reset_request(req);
		reset_response(res);	
         // Handle incoming connections
        if (FD_ISSET(socket_fd, &tmp_fds)) {
            int client_socket;
            struct sockaddr_in client_address;
            int client_address_length = sizeof(client_address);
            client_socket = accept(socket_fd, (struct sockaddr*) &client_address, &client_address_length);
            if (client_socket < 0) {
                perror("accept()");
                continue;
            }
            FD_SET(client_socket, &read_fds);
            // add client socket to array of sockets
            for(int i = 0; i < MAXCLIENTS; i++){
                if(CLIENTS[i] == 0){
                   CLIENTS[i] = client_socket;
                    break;
                }
            }
            // Add client socket to read_fds
           
           if (client_socket > max_fd) {
               max_fd = client_socket;
           }
        }

        // Handle client data
        // Handle incoming data
        for (int i = 0; i <MAXCLIENTS ; i++) {
            reset_request(req);
		    reset_response(res);
            if (FD_ISSET(i, &tmp_fds)) {
                printf("Handling data on socket %d\n", max_fd);
                char buf[1024];
                // read the request from the client
                int rnum = read(i,buf,sizeof(buf));
                if (rnum < 0) {
                    perror("read()");
                    continue;
                }else if (rnum == 0) {
                   break;
                }

                printf("rnum: %d\n", rnum);
                request_handler(buf, ht, buf, rnum, req, res);
                int num = write(i, res->buf, res->used_size);
                if (num < 0) {
                    perror("write()");
                    continue;
                }else if (num == 0) {
                    break;
                }

                //if quit, close the connection
                if (strncmp(buf, "QUIT", 4) == 0) {
                    close(i);
                    FD_CLR(i, &read_fds);
                    for(int j = 0; j < MAXCLIENTS; j++){
                        if(CLIENTS[j] == i){
                            CLIENTS[j] = 0;
                            break;
                        }
                    }
                }


                
            
            }
        }



		//socklen_t addr_len = sizeof(struct sockaddr_in);
		//int cfd = accept(socket_fd, (struct sockaddr*)&client_addr, &addr_len);
	//	if (cfd < 0) {
		//	perror("accept()");
		//	continue;
		//}
		//char buf[1024];
        // read the request from the client
		//int rnum = read(cfd,buf,sizeof(buf));
     //   request_handler(buf, ht, buf, rnum, req, res);
	//	write(cfd, res->buf, res->used_size);
        //if quit, close the connection
	  // close(cfd);
	}
	destroy_request(req);
	destroy_response(res);

	close(socket_fd);


return 0;
}

