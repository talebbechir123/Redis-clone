#ifndef __HASHTABLE_H__
#define __HASHTABLE_H__

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <time.h>

typedef struct hashtable_element {
    char *key;
    char *value;
    time_t expire_at;   /* 0 = never expires */
    struct hashtable_element *next;
} hashtable_element;

typedef struct hashtable {
    int size;
    int count;
    hashtable_element **table;
} hashtable;

hashtable  *hashtable_new(int size);
void        hashtable_free(hashtable *ht);
void        hashtable_flush(hashtable *ht);
void        hashtable_element_free(hashtable_element *he);

uint32_t    hashtable_hash(hashtable *ht, const char *key);

/* Set (upsert). expire_at=0 means no expiry.
   Returns 0=inserted, 1=updated, -1=error. */
int   hashtable_set(hashtable *ht, const char *key, const char *value, time_t expire_at);

/* Set only if key does not exist.
   Returns 0=inserted, -1=key already exists, -2=error. */
int   hashtable_setnx(hashtable *ht, const char *key, const char *value, time_t expire_at);

/* Returns value string or NULL (key missing / expired). */
char *hashtable_get(hashtable *ht, const char *key);

/* Returns 0=deleted, -1=not found. */
int   hashtable_delete(hashtable *ht, const char *key);

/* Set or replace TTL. Returns 0=ok, -1=key not found. */
int   hashtable_expire(hashtable *ht, const char *key, int seconds);

/* Remove TTL from key. Returns 0=ok, -1=key not found / no TTL. */
int   hashtable_persist(hashtable *ht, const char *key);

/* Returns remaining seconds, -1=no TTL, -2=key not found. */
long  hashtable_ttl(hashtable *ht, const char *key);

/* Increment numeric value by delta. Stores result in *out.
   Returns 0=ok, -1=key not integer, -2=error. */
int   hashtable_incr(hashtable *ht, const char *key, long delta, long *out);

/* Append suffix to value. Stores new length in *new_len.
   Returns 0=ok, -1=error. */
int   hashtable_append(hashtable *ht, const char *key, const char *suffix, size_t *new_len);

/* Return NULL-terminated array of matching key strings.
   Caller must free each string and the array itself.
   Pattern: "*" = all, otherwise simple fnmatch with * wildcard. */
char **hashtable_keys(hashtable *ht, const char *pattern, int *count);

int   hashtable_count(hashtable *ht);

/* Evict all expired keys and return the number removed. */
int   hashtable_evict_expired(hashtable *ht);

void  hashtable_print(hashtable *ht);

#endif
