#ifndef __HASHTABLE_H__
#define __HASHTABLE_H__

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include <math.h>

#define ULONG_MAX 4294967295UL


// implementation of a hashtable element
typedef struct hashtable_element
{
    char *key;
    char *value;
    struct hashtable_element *next;
} hashtable_element;

// implementation of a hashtable

typedef struct hashtable
{
    int size;
    int count;
    hashtable_element **table;
} hashtable;

// create a new hashtable

hashtable *hashtable_new(int size);

// hash a string for a particular hash table

int hashtable_hash(hashtable *ht, char *key);

// create a key-value pair

hashtable_element *hashtable_newpair(char *key, char *value);

// insert a key-value pair into a hash table

int hashtable_set(hashtable *ht, char *key, char *value);

// retrieve a key-value pair from a hash table
char* hashtable_get(hashtable *ht, char *key);


int find_h(hashtable *ht, char *key);

// delete a key-value pair from a hash table

int hashtable_delete(hashtable *ht, char *key);

// print the contents of the hash table

void hashtable_print(hashtable *ht);


// free the memory allocated for a hash table

void hashtable_free(hashtable *ht);

// free the memory allocated for a hash table element

void hashtable_element_free(hashtable_element *he);














#endif

