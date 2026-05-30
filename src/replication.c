#include "../include/replication.h"

void hashtable_replicate(hashtable *ht, const char *filename) {
    FILE *fp = fopen(filename, "w");
    if (!fp) { perror("fopen"); return; }

    time_t now = time(NULL);
    for (int i = 0; i < ht->size; i++) {
        hashtable_element *cur = ht->table[i];
        while (cur) {
            /* Skip expired entries */
            if (cur->expire_at == 0 || cur->expire_at > now)
                fprintf(fp, "%s\t%s\t%ld\n", cur->key, cur->value,
                        (long)cur->expire_at);
            cur = cur->next;
        }
    }
    fclose(fp);
}

hashtable *hashtable_convert(hashtable *ht, const char *filename) {
    FILE *fp = fopen(filename, "r");
    if (!fp) { perror("fopen"); return ht; }

    char *line = NULL;
    size_t len = 0;
    time_t now = time(NULL);

    while (getline(&line, &len, fp) != -1) {
        /* Strip trailing newline */
        size_t l = strlen(line);
        while (l > 0 && (line[l-1] == '\n' || line[l-1] == '\r'))
            line[--l] = '\0';

        /* Format: key\tvalue\texpire_at */
        char *key   = strtok(line, "\t");
        char *value = strtok(NULL, "\t");
        char *exp_s = strtok(NULL, "\t");
        if (!key || !value) continue;

        time_t expire_at = exp_s ? (time_t)atol(exp_s) : 0;
        if (expire_at != 0 && expire_at <= now) continue;  /* already expired */

        hashtable_set(ht, key, value, expire_at);
    }

    free(line);
    fclose(fp);
    return ht;
}

hashtable *hashtable_restore(void) {
    const char *dir = "htDB";
    DIR *dfd = opendir(dir);
    if (!dfd) return hashtable_new(65536);

    char   best_path[512] = {0};
    time_t best_mtime     = 0;
    struct dirent *dp;

    while ((dp = readdir(dfd)) != NULL) {
        if (dp->d_name[0] == '.') continue;

        char path[512];
        snprintf(path, sizeof(path), "%s/%s", dir, dp->d_name);

        struct stat st;
        if (stat(path, &st) == 0 && st.st_mtime > best_mtime) {
            best_mtime = st.st_mtime;
            snprintf(best_path, sizeof(best_path), "%s", path);
        }
    }
    closedir(dfd);

    hashtable *ht = hashtable_new(65536);
    if (best_mtime > 0) {
        printf("[restore] loading snapshot: %s\n", best_path);
        hashtable_convert(ht, best_path);
    }
    return ht;
}
