
#ifndef __REPLICATION_H__
#define __REPLICATION_H__
#include "hashtable.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <dirent.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>






// replication of a hashtable to a text file

void hashtable_replicate(hashtable *ht, char *filename);

// convert a text file to a hashtable

hashtable* hashtable_convert(hashtable*ht,char *filename);
char *strremove(char *str, const char *sub);
// write a hashtable as a json fil
hashtable *convert_json(char *filename);

void hashtable_write_json(hashtable *ht, char *filename);

void readJsonFileAndInsert(hashtable* ht, const char* filename);



hashtable *hashtable_restore();





#endif