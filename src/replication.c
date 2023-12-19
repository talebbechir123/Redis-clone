#include "../include/replication.h"



void hashtable_replicate(hashtable *ht, char *filename){
    //create a file or open a file
    FILE *fp = fopen(filename, "w+");
    if(fp == NULL){
        perror("fopen");
        exit(EXIT_FAILURE);
    }
    int i = 0;
    hashtable_element *pair;
    for(i = 0; i < ht->size; i++){
        pair = ht->table[i];
        while(pair != NULL && pair->key != NULL){
            fprintf(fp, "%s,%s\n", pair->key, (char *)pair->value);
            pair = pair->next;
        }
    }
    fclose(fp);
}


char *strremove(char *str, const char *sub) {
    size_t len = strlen(sub);
    if (len > 0) {
        char *p = str;
        while ((p = strstr(p, sub)) != NULL) {
            memmove(p, p + len, strlen(p + len) + 1);
        }
    }
    return str;
}


// convert a text file to a hashtable

hashtable* hashtable_convert(hashtable*ht,char *filename){
    FILE *fp = fopen(filename, "r");
    if(fp == NULL){
        perror("fopen");
        exit(EXIT_FAILURE);
    }
    char *line = NULL;
    size_t len = 0;
    ssize_t read;
    char *key;
    char *value;
    while((read = getline(&line, &len, fp)) != -1){
        // the key is the first token before the comma
        char *tmpkey=malloc(sizeof(char)*100);
        char *tmpvalue=malloc(sizeof(char)*100);
        key = strtok(line, ",");
        tmpkey = strcpy(tmpkey, key);
        printf("key:%s\n", key);
       
        
        // the value is the second token after the comma
        value = strtok(NULL, ",");
        printf("value:%s\n", value);
        tmpvalue = strcpy(tmpvalue, value);
        
        
        // remove the newline character from the value
         //value = strremove(value, "\n");
        //hashtable_set(ht, key, value);
      //  printf("key: %s, value: %s\n", key, value);
         hashtable_set(ht, tmpkey, tmpvalue);
        free(tmpkey);
        free(tmpvalue);
        
    }
   // put the values and keys into the hashtable
    






    fclose(fp);
    if(line){
        free(line);
    }
    //copy the hashtable

    return ht;
  
}

// write a hashtable as a json file

void hashtable_write_json(hashtable *ht, char *filename){
    //write the hashtable to a json file
    FILE *fp = fopen(filename, "w+");
    if(fp == NULL){
        perror("fopen");
        exit(EXIT_FAILURE);
    }
    int i = 0;
    hashtable_element *pair;
    fprintf(fp, "{\n");
    for(i = 0; i < ht->size; i++){
        pair = ht->table[i];
        while(pair != NULL && pair->key != NULL){
            fprintf(fp, "\t\"%s\": \"%s\"", pair->key, (char *)pair->value);
            if(pair->next != NULL){
                fprintf(fp, ",");
            }
            fprintf(fp, "\n");
            pair = pair->next;
        }
    }
    fprintf(fp, "}\n");
    fclose(fp);

}



// open the database files and read the data into a hashtable
// take the lastest version of the database file
hashtable *hashtable_restore(){
    //open the file from htDB directory
    // find the latest file
    // recursively search through the directory
    // find the file with the latest timestamp
    // read the file into a hashtable
    // return the hashtable
    // if there is no file, return an empty hashtable
    struct dirent *dp;
    DIR *dfd;
    char *dir = "htDB";
    char *file;
    char *latest_file;
    char *latest_file_path;
    time_t latest_time = 0;
    struct stat attr;
    if((dfd = opendir(dir)) == NULL){
        fprintf(stderr, "can't open %s\n", dir);
        return NULL;
    }
    while((dp = readdir(dfd)) != NULL){
        file = dp->d_name;
        if(strcmp(file, ".") == 0 || strcmp(file, "..") == 0){
            continue;
        }
        char *path = malloc(strlen(dir) + strlen(file) + 2);
        strcpy(path, dir);
        strcat(path, "/");
        strcat(path, file);
        stat(path, &attr);
        if(attr.st_mtime > latest_time){
            latest_time = attr.st_mtime;
            latest_file = file;
            latest_file_path = path;
        }
    }
    printf("latest file: %s\n", latest_file);
    closedir(dfd);
    if(latest_time == 0){
        return hashtable_new(65536);
    }
    //add the path to the file
    printf ("latest file path: %s\n", latest_file_path);
    hashtable *ht = hashtable_new(65536);
   ht= hashtable_convert(ht,latest_file_path);
    return ht;
}