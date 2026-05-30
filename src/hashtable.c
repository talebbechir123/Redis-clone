#include "../include/hashtable.h"
#include <fnmatch.h>

/* FNV-1a 32-bit hash — excellent distribution, fast on short keys */
uint32_t hashtable_hash(hashtable *ht, const char *key) {
    uint32_t h = 2166136261u;
    for (const unsigned char *p = (const unsigned char *)key; *p; p++) {
        h ^= *p;
        h *= 16777619u;
    }
    return h % (uint32_t)ht->size;
}

hashtable *hashtable_new(int size) {
    hashtable *ht = malloc(sizeof(hashtable));
    if (!ht) return NULL;
    ht->size  = size;
    ht->count = 0;
    ht->table = calloc(size, sizeof(hashtable_element *));
    if (!ht->table) { free(ht); return NULL; }
    return ht;
}

void hashtable_element_free(hashtable_element *he) {
    if (!he) return;
    free(he->key);
    free(he->value);
    free(he);
}

void hashtable_flush(hashtable *ht) {
    for (int i = 0; i < ht->size; i++) {
        hashtable_element *cur = ht->table[i];
        while (cur) {
            hashtable_element *next = cur->next;
            hashtable_element_free(cur);
            cur = next;
        }
        ht->table[i] = NULL;
    }
    ht->count = 0;
}

void hashtable_free(hashtable *ht) {
    if (!ht) return;
    hashtable_flush(ht);
    free(ht->table);
    free(ht);
}

static int is_expired(hashtable_element *e) {
    return e->expire_at != 0 && time(NULL) >= e->expire_at;
}

/* Internal: find element and its predecessor for a key.
   Sets *prev to the element before the found one (or NULL if at head).
   Returns pointer to the found element, or NULL. */
static hashtable_element *find_element(hashtable *ht, const char *key,
                                        hashtable_element **prev_out) {
    uint32_t bin = hashtable_hash(ht, key);
    hashtable_element *prev = NULL;
    hashtable_element *cur  = ht->table[bin];
    while (cur) {
        if (strcmp(cur->key, key) == 0) {
            if (prev_out) *prev_out = prev;
            return cur;
        }
        prev = cur;
        cur  = cur->next;
    }
    return NULL;
}

/* Internal: remove an element from its bucket (caller must free it). */
static void unlink_element(hashtable *ht, uint32_t bin,
                            hashtable_element *prev, hashtable_element *cur) {
    if (prev)
        prev->next = cur->next;
    else
        ht->table[bin] = cur->next;
    ht->count--;
}

int hashtable_set(hashtable *ht, const char *key, const char *value, time_t expire_at) {
    hashtable_element *prev = NULL;
    hashtable_element *cur  = find_element(ht, key, &prev);

    if (cur) {
        /* Update in place — treat expired keys as gone then re-insert below */
        if (is_expired(cur)) {
            uint32_t bin = hashtable_hash(ht, key);
            unlink_element(ht, bin, prev, cur);
            hashtable_element_free(cur);
            /* fall through to insert */
        } else {
            free(cur->value);
            cur->value     = strdup(value);
            cur->expire_at = expire_at;
            if (!cur->value) return -1;
            return 1;  /* updated */
        }
    }

    /* Insert new */
    hashtable_element *ne = malloc(sizeof(hashtable_element));
    if (!ne) return -1;
    ne->key       = strdup(key);
    ne->value     = strdup(value);
    ne->expire_at = expire_at;
    ne->next      = NULL;
    if (!ne->key || !ne->value) {
        hashtable_element_free(ne);
        return -1;
    }
    uint32_t bin  = hashtable_hash(ht, key);
    ne->next      = ht->table[bin];
    ht->table[bin] = ne;
    ht->count++;
    return 0;  /* inserted */
}

int hashtable_setnx(hashtable *ht, const char *key, const char *value, time_t expire_at) {
    hashtable_element *prev = NULL;
    hashtable_element *cur  = find_element(ht, key, &prev);

    if (cur && !is_expired(cur))
        return -1;  /* key exists and is alive */

    /* Either not found or expired — evict if expired then insert */
    if (cur) {
        uint32_t bin = hashtable_hash(ht, key);
        unlink_element(ht, bin, prev, cur);
        hashtable_element_free(cur);
    }

    hashtable_element *ne = malloc(sizeof(hashtable_element));
    if (!ne) return -2;
    ne->key       = strdup(key);
    ne->value     = strdup(value);
    ne->expire_at = expire_at;
    ne->next      = NULL;
    if (!ne->key || !ne->value) {
        hashtable_element_free(ne);
        return -2;
    }
    uint32_t bin   = hashtable_hash(ht, key);
    ne->next       = ht->table[bin];
    ht->table[bin] = ne;
    ht->count++;
    return 0;
}

char *hashtable_get(hashtable *ht, const char *key) {
    hashtable_element *prev = NULL;
    hashtable_element *cur  = find_element(ht, key, &prev);
    if (!cur) return NULL;
    if (is_expired(cur)) {
        uint32_t bin = hashtable_hash(ht, key);
        unlink_element(ht, bin, prev, cur);
        hashtable_element_free(cur);
        return NULL;
    }
    return cur->value;
}

int hashtable_delete(hashtable *ht, const char *key) {
    hashtable_element *prev = NULL;
    hashtable_element *cur  = find_element(ht, key, &prev);
    if (!cur) return -1;
    uint32_t bin = hashtable_hash(ht, key);
    unlink_element(ht, bin, prev, cur);
    hashtable_element_free(cur);
    return 0;
}

int hashtable_expire(hashtable *ht, const char *key, int seconds) {
    hashtable_element *cur = find_element(ht, key, NULL);
    if (!cur || is_expired(cur)) return -1;
    cur->expire_at = time(NULL) + seconds;
    return 0;
}

int hashtable_persist(hashtable *ht, const char *key) {
    hashtable_element *cur = find_element(ht, key, NULL);
    if (!cur || is_expired(cur) || cur->expire_at == 0) return -1;
    cur->expire_at = 0;
    return 0;
}

long hashtable_ttl(hashtable *ht, const char *key) {
    hashtable_element *cur = find_element(ht, key, NULL);
    if (!cur || is_expired(cur)) return -2;
    if (cur->expire_at == 0)     return -1;
    long remaining = (long)(cur->expire_at - time(NULL));
    return remaining < 0 ? 0 : remaining;
}

int hashtable_incr(hashtable *ht, const char *key, long delta, long *out) {
    hashtable_element *cur = find_element(ht, key, NULL);
    long val = 0;

    if (cur && !is_expired(cur)) {
        char *end;
        val = strtol(cur->value, &end, 10);
        if (*end != '\0') return -1;  /* not an integer */
    }

    val += delta;
    char buf[32];
    snprintf(buf, sizeof(buf), "%ld", val);

    time_t exp = (cur && !is_expired(cur)) ? cur->expire_at : 0;
    if (hashtable_set(ht, key, buf, exp) == -1) return -2;
    if (out) *out = val;
    return 0;
}

int hashtable_append(hashtable *ht, const char *key, const char *suffix, size_t *new_len) {
    hashtable_element *cur = find_element(ht, key, NULL);
    if (!cur || is_expired(cur)) {
        if (hashtable_set(ht, key, suffix, 0) == -1) return -1;
        if (new_len) *new_len = strlen(suffix);
        return 0;
    }

    size_t old_len = strlen(cur->value);
    size_t suf_len = strlen(suffix);
    char  *joined  = malloc(old_len + suf_len + 1);
    if (!joined) return -1;
    memcpy(joined, cur->value, old_len);
    memcpy(joined + old_len, suffix, suf_len + 1);

    free(cur->value);
    cur->value = joined;
    if (new_len) *new_len = old_len + suf_len;
    return 0;
}

char **hashtable_keys(hashtable *ht, const char *pattern, int *count) {
    int   cap  = 64;
    int   n    = 0;
    char **arr = malloc(cap * sizeof(char *));
    if (!arr) { *count = 0; return NULL; }

    for (int i = 0; i < ht->size; i++) {
        hashtable_element *cur = ht->table[i];
        while (cur) {
            hashtable_element *next = cur->next;
            if (!is_expired(cur)) {
                if (fnmatch(pattern, cur->key, 0) == 0) {
                    if (n == cap) {
                        cap *= 2;
                        char **tmp = realloc(arr, cap * sizeof(char *));
                        if (!tmp) {
                            for (int j = 0; j < n; j++) free(arr[j]);
                            free(arr);
                            *count = 0;
                            return NULL;
                        }
                        arr = tmp;
                    }
                    arr[n++] = strdup(cur->key);
                }
            }
            cur = next;
        }
    }
    *count = n;
    return arr;
}

int hashtable_count(hashtable *ht) {
    /* Count only live (non-expired) entries */
    int n = 0;
    for (int i = 0; i < ht->size; i++) {
        hashtable_element *cur = ht->table[i];
        while (cur) {
            if (!is_expired(cur)) n++;
            cur = cur->next;
        }
    }
    return n;
}

int hashtable_evict_expired(hashtable *ht) {
    int removed = 0;
    for (int i = 0; i < ht->size; i++) {
        hashtable_element *prev = NULL;
        hashtable_element *cur  = ht->table[i];
        while (cur) {
            hashtable_element *next = cur->next;
            if (is_expired(cur)) {
                unlink_element(ht, i, prev, cur);
                hashtable_element_free(cur);
                removed++;
                /* prev stays the same */
            } else {
                prev = cur;
            }
            cur = next;
        }
    }
    return removed;
}

void hashtable_print(hashtable *ht) {
    for (int i = 0; i < ht->size; i++) {
        hashtable_element *cur = ht->table[i];
        while (cur) {
            if (!is_expired(cur))
                printf("%s -> %s\n", cur->key, cur->value);
            cur = cur->next;
        }
    }
}
