#include "../include/hashtable.h"




hashtable *hashtable_new(int size){
    
        hashtable *ht = malloc(sizeof(hashtable));
        ht->size = size;
        ht->table = malloc(sizeof(hashtable_element *) * size);
        int i;
        for (i = 0; i < size; i++) ht->table[i] = NULL;
        return ht;

}

// hash a string for a particular hash table

int hashtable_hash(hashtable *ht, char *key){
        
            unsigned long int hashval=0;
            int i = 0;
            // Convert our string to an integer
            while (hashval < ULONG_MAX && i < strlen(key)) {
                hashval = hashval << 8;
                hashval += key[i];
                i++;
            }
            return hashval % ht->size;
}

// create a key-value pair

hashtable_element *hashtable_newpair(char *key, char*value){
    
        hashtable_element *newpair;
        newpair = malloc(sizeof(hashtable_element));
        newpair->key = strdup(key);
        newpair->value = value;
        newpair->next = NULL;
        return newpair;

}

// insert a key-value pair into a hash table

int hashtable_set(hashtable *ht, char *key, char *value){
        //return O if insert is successful and -1 if not
            int bin = 0;
            hashtable_element *newpair = NULL;
            hashtable_element *next = NULL;
            hashtable_element *last = NULL;
            bin = hashtable_hash(ht, key);
            next = ht->table[bin];
            while (next != NULL && next->key != NULL && strcmp(key, next->key) > 0) {
                last = next;
                next = next->next;
            }
            // There's already a pair.  Let's replace that string.
            if (next != NULL && next->key != NULL && strcmp(key, next->key) == 0) {
                free(next->value);
                next->value = value;
                // Nope, could't find it.  Time to grow a pair.
                return -1;
            } else {
                newpair = hashtable_newpair(key, value);
                // We're at the start of the linked list in this bin.
                if (next == ht->table[bin]) {
                    newpair->next = next;
                    ht->table[bin] = newpair;
                    // We're at the end of the linked list in this bin.
                } else if (next == NULL) {
                    last->next = newpair;
                    // We're in the middle of the list.
                } else {
                    newpair->next = next;
                    last->next = newpair;
                }
            }

            return 0;


}

// retrieve a key-value pair from a hash table

char* hashtable_get(hashtable *ht, char *key){
        
            int bin = 0;
            hashtable_element *pair;
            bin = hashtable_hash(ht, key);
            // Step through the bin, looking for our value.
            pair = ht->table[bin];
            while (pair != NULL && pair->key != NULL && strcmp(key, pair->key) > 0) {
                pair = pair->next;
            }
            // Did we actually find anything?
            if (pair == NULL || pair->key == NULL || strcmp(key, pair->key) != 0) {
                return NULL;
            } else {
                return pair->value;
            }

            // return the value of the key
            // return pair->value;

            return pair->value;


        
}


// delete a key-value pair from a hash table

int hashtable_delete(hashtable *ht, char *key){
            //return O if delete is successful and -1 if not
                int bin = 0;
                hashtable_element *pair;
                hashtable_element *last;
                bin = hashtable_hash(ht, key);
                pair = ht->table[bin];
                last = NULL;
                while (pair != NULL && pair->key != NULL && strcmp(key, pair->key) > 0) {
                    last = pair;
                    pair = pair->next;
                }
                // If it's the first element, remove it from the table.
                if (pair == ht->table[bin]) {
                    ht->table[bin] = pair->next;
                } else if (pair == NULL) {
                    // Nope, couldn't find it.
                    return -1;
                } else {
                    // Found it.
                    last->next = pair->next;
                }
                //hashtable_element_free(pair);
                return 0;
}

// print the contents of the hash table

void hashtable_print(hashtable *ht){
                
                    int i = 0;
                    hashtable_element *pair;
                    for (i = 0; i < ht->size; i++) {
                        pair = ht->table[i];
                        while (pair != NULL && pair->key != NULL) {
                            printf("%s,%s\n", pair->key, (char *)pair->value);
                            pair = pair->next;
                        }
                    }
                
}



// free the memory allocated for a hash table

void hashtable_free(hashtable *ht) {
                    
                        int i = 0;
                        hashtable_element *pair;
                        hashtable_element *next;
                        for (i = 0; i < ht->size; i++) {
                            pair = ht->table[i];
                            while (pair != NULL && pair->key != NULL) {
                                next = pair->next;
                                hashtable_element_free(pair);
                                pair = next;
                            }
                        }
                        free(ht->table);
                        free(ht);
                    
}


// free the memory allocated for a hash table element

void hashtable_element_free(hashtable_element *he){
                        
                            if (he == NULL) return;
                            free(he->key);
                            free(he->value);
                            free(he);
                        
}



int find_h(hashtable *ht, char *key){
    int bin = 0;
    hashtable_element *pair;
    bin = hashtable_hash(ht, key);
    // Step through the bin, looking for our value.
    pair = ht->table[bin];
    while (pair != NULL && pair->key != NULL && strcmp(key, pair->key) > 0) {
        pair = pair->next;
    }
    // Did we actually find anything?
    if (pair == NULL || pair->key == NULL || strcmp(key, pair->key) != 0) {
        return 0;
    } else {
        return 1;
    }
}




