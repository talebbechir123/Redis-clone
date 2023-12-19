#include "include/server.h"
#include "include/resp.h"
#include <fcntl.h>
#include <signal.h>
#include "include/replication.h"
#include <errno.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#define BUF_SIZE 1024

int REPLICATION_INTERVAL =100;
int HashINSERTS = 0;
//database save flag 0 = no save, 1 = save
int DATABASE_SAVE_FLAG =0;




// function that returns the current time
char* GetTimestamp()
{
	time_t rawtime;
	struct tm* timeinfo;

	time(&rawtime);
	timeinfo = localtime(&rawtime);
	char* time = asctime(timeinfo);

	char* newLine = strstr(time, "\n");
	if (newLine)
	{
		*newLine = 0;
	}

	return time;
}

void save_db(hashtable* ht){
    char* timestamp = GetTimestamp();
        printf("Timestamp: %s\n", timestamp);
        char* filename = malloc(strlen("htDB/hashtable_") + strlen(timestamp) + strlen(".txt") + 1);
        strcpy(filename, "htDB/hashtable_");
        strcat(filename, timestamp);
        strcat(filename, ".txt");
        printf("Filename: %s\n", filename);
        hashtable_replicate(ht, filename);
}

//function that initializes that loads the help menu from a file so that it can be printed to the client
int help_init(char* help){
    FILE *fp;
    char* line = NULL;
    size_t len = 0;
    ssize_t read;
    //help is in the Resource folder
    fp = fopen("Resource/help.txt", "r");
    if(fp == NULL){
        printf("Error opening file\n");
        exit(EXIT_FAILURE);
    }
    while((read = getline(&line, &len, fp)) != -1){
        strcat(help, line);
    }
    fclose(fp);
    if(line){
        free(line);
    }
    return strlen(help);
}


//function that gives us the status of the hashtable
int info_init(char* info, hashtable* ht){
    char* timestamp = GetTimestamp();
    char* filename = malloc(strlen("htDB/hashtable_") + strlen(timestamp) + strlen(".txt") + 1);
    strcpy(filename, "htDB/hashtable_");
    strcat(filename, timestamp);
    strcat(filename, ".txt");
    hashtable_replicate(ht, filename);
    FILE *fp;
    char* line = NULL;
    size_t len = 0;
    ssize_t read;
    //help is in the Resource folder
    fp = fopen(filename, "r");
    if(fp == NULL){
        printf("Error opening file\n");
        exit(EXIT_FAILURE);
    }
    while((read = getline(&line, &len, fp)) != -1){
        strcat(info, line);
    }
    fclose(fp);
    if(line){
        free(line);
    }
    return strlen(info);
}




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


//function that handles the request from the client 
int request_handler(char * request, hashtable *ht,char* buf, int rnum, RespRequest *req, RespResponse *res){
     // return 0 if decode is successful and -1 if not
     int result_decode ;
		if ((result_decode = decode_request(req, buf, rnum)) == 0 && 
			(req->state == OK || req->state == PART_OK) ){
			if (req->argc > 1 && strncmp(request_argv(req,0), "add", 3) == 0) {
				int total = 0;
				for (int i = 1; i < req->argc; i++)
					total += atoi(request_argv(req,i));
				result_decode= encode_response_integer(res, total);
			}else if (req->argc > 1 && strncmp(request_argv(req,0), "set", 3) == 0) {
                // set returns 0 if insert is successful and -1 if not
                //if DATABASE_SAVE_FLAG is 1 then save the database
        
                int result = hashtable_set(ht, request_argv(req,1), request_argv(req,2));
                if(result == 0){
                   result_decode= encode_response_status(res,1,"OK");
                   printf("HashINSERTS: %d\n", HashINSERTS);
                     HashINSERTS++;
                      printf("HashINSERTS: %d\n", HashINSERTS);
                if(HashINSERTS == REPLICATION_INTERVAL){
                    raise(SIGUSR1);
                    HashINSERTS = REPLICATION_INTERVAL;
                }
                }else if(result == -1){
                 result_decode=encode_response_status(res,0,"ERR key already exists");
                }
             
               // encode_response_status(res,1,"OK");
               
                //raise(SIGUSR1);
            }else if (req->argc ==1 && strncmp(request_argv(req,0), "PING", 3) == 0) {
                encode_response_status(res,1,"PONG");
            }else if (req->argc ==2 && strncmp(request_argv(req,0), "get", 3) == 0) {
                 // get the value from the hashtable of the key
                 // key is the second argument
                char* key = request_argv(req,1);
               printf("Key: %s\n", key);
               char *value = hashtable_get(ht, key);
               //check if the value is null
                if(value == NULL){
                result_decode=encode_response_status(res,0,"ERR key does not exist");
                     return result_decode;
                }else {
                      printf("Value: %s\n", value);
                // 
              result_decode=  encode_response_string(res, hashtable_get(ht, key), strlen(hashtable_get(ht, key)));
                }
            }else if (req->argc ==1 && strncmp(request_argv(req,0), "QUIT", 3) == 0) {
               result_decode= encode_response_status(res,1,"OK");
                exit(EXIT_SUCCESS);
               
            
			}else if (req->argc ==2 && strncmp(request_argv(req,0), "DEL", 3) == 0) {
               //if delete is successful hastable_delete returns 0
                //if delete is unsuccessful hashtable_delete returns -1

                int result = hashtable_delete(ht, request_argv(req,1));
                if(result == 0){
                 result_decode=   encode_response_status(res,1,"OK");
                }else if(result == -1){
                  result_decode=  encode_response_status(res,0,"ERR key does not exist");
                }
               
            
            } else if(req->argc ==1 && strncmp(request_argv(req,0), "TIME", 4) == 0){
                time_t rawtime;
                struct tm * timeinfo;
                time ( &rawtime );
                timeinfo = localtime ( &rawtime );
                char* time = asctime(timeinfo);
                char* newLine = strstr(time, "\n");
                if (newLine)
                {
                    *newLine = 0;
                }
              result_decode=  encode_response_string(res, time, strlen(time));
            
            }else if (req->argc ==1 && strncmp(request_argv(req,0), "SAVE", 4) == 0) {
                //save the database
                save_db(ht);
                result_decode= encode_response_status(res,1,"OK");
            
            }else if (req->argc ==1 && strncmp(request_argv(req,0), "RESTORE", 7) == 0) {
                //restore the database
                hashtable_restore();
                result_decode= encode_response_status(res,1,"OK");
            }else if (req->argc ==2 && strncmp(request_argv(req,0), "COPY",3) == 0) {
                //copy the database to a file
                char* filename = request_argv(req,1);
                hashtable_replicate(ht, filename);
                result_decode= encode_response_status(res,1,"OK");
            
           }else if (req->argc ==2 && strncmp(request_argv(req,0), "EXISTS",5) == 0) {
            //check if the key exists
            //if the key exists return 1
            //if the key does not exist return 0
            char* key = request_argv(req,1);
            int result = find_h(ht,key);
            if(result == 1){
                result_decode= encode_response_status(res,1,"OK");
            }else if(result == 0){
                result_decode= encode_response_status(res,0,"ERR key does not exist");
            }
            
           } else if (req->argc ==1 && strncmp(request_argv(req,0), "--HELP",5) == 0) {

            //print the help menu
            //print the commands and their descriptions
            //print the arguments and their descriptions
            char help[1024];
            int help_size = help_init(help);
            result_decode= encode_response_string(res, help, help_size);            
           }else if (req->argc ==2 && strncmp(request_argv(req,0), "ECHO",3) == 0) {

            //echo the argument back to the client
            //print the argument to the server console
            char* echo = request_argv(req,1);
            printf("Echo: %s\n", echo);
            result_decode= encode_response_string(res, echo, strlen(echo));
           }else if (req->argc ==1 && strncmp(request_argv(req,0), "INFO",3) == 0) {
                
                char info[1024];
                int info_size = info_init(info, ht);
                result_decode= encode_response_string(res, info, info_size);
            
           }else {
				result_decode= encode_response_status(res,0,"ERR unknown command");
			}
		}
		else {

			result_decode=encode_response_status(res,0,"ERR decode command fail");
		}

        return result_decode;
   
}



//create a signal that will export the hashtable to a file every 10 inserts
void sig_handler(int signo){
    //turn arg into a hashtable
    //hashtable *ht = (hashtable *)arg;
    if(signo == SIGUSR1){
       // turn the database save flag to 1
      //
      DATABASE_SAVE_FLAG = 1;
    }
}

int main(int argc , char **argv){

    if(argc != 3){
        printf("Usage: ./server <port> <replication interval>\n");
        exit(EXIT_FAILURE);
    }

    int PORT = atoi(argv[1]);
    REPLICATION_INTERVAL = atoi(argv[2]);
    int socket_fd; 
    int CLIENTS[MAXCLIENTS];
    hashtable* ht;
   // int client_count = 0;
    int max_fd;
    fd_set read_fds;
    //fd_set write_fds;
   // fd_set except_fds;
    struct timeval tv;
    //create a directory for the hashtable files
   int result = mkdir("htDB", 0777);
    if(result == -1){
         printf("Error creating directory\n");
         if (errno == EEXIST){
             printf("Directory already exists\n");
         }else{
             printf("Error creating directory\n");
             exit(EXIT_FAILURE);
         }

    }else if(result == 0){
        printf("Directory created\n");
    }
    //how to check for errno EEXIST

    //initialize server socket
    socket_fd = server_init(PORT);

    //listen for connections
    if(listen(socket_fd, MAXCLIENTS) < 0){
        perror("listen");
        exit(EXIT_FAILURE);
    }
    // do you want to restore the previous hashtable?
    printf("Do you want to restore the previous hashtable? (y/n)\n");
    char answer;
    scanf("%c", &answer);
    if(answer == 'y'){
        printf("Restoring previous hashtable\n");
        ht =hashtable_restore();
    }else if(answer == 'n'){
        printf("Creating new hashtable\n");
        ht = hashtable_new(65536);
    }else{
        printf("Invalid input\n");
        exit(EXIT_FAILURE);
    }
    //initialize the hashtable
    //ht = hashtable_new(65536);
   //  ht =hashtable_restore();
   // if(ht == NULL){
      // printf("ht is empty\n");
       // ht = hashtable_new(65536);
   // }
    //accept connections
     socklen_t addrlen = sizeof(struct sockaddr_in);
    struct sigaction act;
    act.sa_handler = sig_handler;
    sigemptyset(&act.sa_mask);
    act.sa_flags = 0;
    sigaction(SIGUSR1, &act, NULL);
    RespRequest *req = create_request(BUF_SIZE);
	RespResponse *res = create_response(BUF_SIZE);
    //// Set up file descriptors for select()
    FD_ZERO(&read_fds);
    FD_SET(socket_fd, &read_fds);
    max_fd = socket_fd;
    tv.tv_sec = 0;
    tv.tv_usec = 100000;



	while (1) {

        while (DATABASE_SAVE_FLAG == 0){

        

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
           // int client_address_length = sizeof(client_address);
            client_socket = accept(socket_fd, (struct sockaddr*) &client_address, &addrlen);
            if (client_socket < 0) {
                perror("accept()");
                continue;
            }
            printf("new client connected socket :%d\n", client_socket);
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
               // printf("Handling data on socket %d\n", max_fd);
                char buf[1024];
                // read the request from the client
                int rnum = read(i,buf,sizeof(buf));
                if (rnum < 0) {
                    perror("read()");
                    continue;
                }else if (rnum == 0) {
                   break;
                }

               // printf("rnum: %d\n", rnum);
               int request_result = request_handler(buf, ht, buf, rnum, req, res);
                // printf("request_result: %d\n", request_result);
                if(request_result == -1){
                    perror("request_handler()");
                    continue;
                }
                    //printf("request_result: %d\n", request_result);
                int num = write(i, res->buf, res->used_size);
                    if (num < 0) {
                        perror("write()");
                        continue;
                    }else if (num == 0) {
                        break;
                    }
               
                //write the response to the client

               

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

  }
    //save the database
    save_db(ht);

  DATABASE_SAVE_FLAG = 0;
		
	}
	destroy_request(req);
	destroy_response(res);
    //close all sockets
    for(int i = 0; i < MAXCLIENTS; i++){
        if(CLIENTS[i] != 0){
            close(CLIENTS[i]);
        }
    }

	close(socket_fd);


return 0;
}

