#ifndef __REPLICATION_H__
#define __REPLICATION_H__

#include "hashtable.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>

/* Save the hashtable to a CSV file (key,value,expire_at per line).
   Expired entries are skipped. */
void hashtable_replicate(hashtable *ht, const char *filename);

/* Load a CSV snapshot into an existing hashtable. */
hashtable *hashtable_convert(hashtable *ht, const char *filename);

/* Find the most-recently-modified snapshot in htDB/ and restore it.
   Returns a new hashtable (may be empty if no snapshots exist). */
hashtable *hashtable_restore(void);

#endif
