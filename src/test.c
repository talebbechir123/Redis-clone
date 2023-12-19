#include "../include/replication.h"



int main(){

 

    // create a new hashtable

    hashtable *ht = hashtable_new(65536);
   // add some key/value pairs

   hashtable_set(ht, "key1", "inky");
   hashtable_set(ht, "key2", "pinky");
   hashtable_set(ht, "key3", "blinky");
   hashtable_set(ht, "key4", "floyd");
   hashtable_set(ht, "james", "floyd");
   hashtable_set(ht, "jane", "floyd");
    hashtable_set(ht, "1", "papa");

printf("-------------------------------------\n");
    //print the contents of the hashtable
    hashtable_print(ht);

    // retrieve some key/value pairs
    char* k = hashtable_get(ht, "key1");
    printf("-------------------------------------\n");

    printf("%s\n", k);

    // delete some key/value pairs
    hashtable_delete(ht, "key1");

    printf("-------------------------------------\n");

    // print the contents of the hashtable
    hashtable_print(ht);

    printf("-------------------------------------\n");
    // test writing to a file
    hashtable_replicate(ht, "test.txt");
    //hashtable_write_json(ht, "test.txt");
    printf("test.txt written\n");
    // write from the file to a new hashtable
    printf("-------------------------------------\n");
    hashtable *ht2 = hashtable_new(65536);
    ht2= hashtable_convert(ht2,"test.txt");
    printf("-------------------------------------\n");
    hashtable_print(ht2);
    printf("-------------------------------------\n");








  


    return 0;
}