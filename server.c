#include "include/server.h"
#include "include/resp.h"
#include "include/replication.h"

#include <sys/epoll.h>
#include <fcntl.h>
#include <signal.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#include <strings.h>   /* strcasecmp */
#include <ctype.h>
#include <stdarg.h>

#define BUF_SIZE         4096
#define MAX_EVENTS       256
#define EXPIRE_INTERVAL  5    /* seconds between passive TTL sweeps */

/* ------------------------------------------------------------------ globals */

static volatile sig_atomic_t g_save_flag = 0;
static int g_repl_interval = 100;    /* save every N inserts */
static int g_insert_count  = 0;

/* ------------------------------------------------------------------ logging */

static void log_info(const char *fmt, ...) {
    time_t t = time(NULL);
    struct tm *tm = localtime(&t);
    char ts[32];
    strftime(ts, sizeof(ts), "%Y-%m-%d %H:%M:%S", tm);
    printf("[%s] INFO  ", ts);
    va_list ap; va_start(ap, fmt); vprintf(fmt, ap); va_end(ap);
    putchar('\n');
}

/* ------------------------------------------------------------------ per-client state */

typedef struct {
    int          fd;
    RespRequest *req;
    RespResponse *res;
    char         addr[INET_ADDRSTRLEN];
} client_t;

static client_t *client_new(int fd, const char *addr) {
    client_t *c = malloc(sizeof(client_t));
    if (!c) return NULL;
    c->fd  = fd;
    c->req = create_request(BUF_SIZE);
    c->res = create_response(BUF_SIZE);
    if (!c->req || !c->res) { free(c); return NULL; }
    snprintf(c->addr, sizeof(c->addr), "%s", addr);
    return c;
}

static void client_free(client_t *c) {
    if (!c) return;
    destroy_request(c->req);
    destroy_response(c->res);
    free(c);
}

/* ------------------------------------------------------------------ signal */

static void sig_handler(int signo) {
    if (signo == SIGUSR1) g_save_flag = 1;
}

/* ------------------------------------------------------------------ helpers */

static void set_nonblocking(int fd) {
    int flags = fcntl(fd, F_GETFL, 0);
    fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

int server_init(int port) {
    int fd;
    struct sockaddr_in addr;
    int opt = 1;

    if ((fd = socket(AF_INET, SOCK_STREAM, 0)) < 0) { perror("socket"); exit(1); }
    setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    set_nonblocking(fd);

    addr.sin_family      = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port        = htons(port);

    if (bind(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) { perror("bind"); exit(1); }
    return fd;
}

static void save_db(hashtable *ht) {
    char path[256];
    time_t now  = time(NULL);
    struct tm *tm = localtime(&now);
    char ts[64];
    strftime(ts, sizeof(ts), "%Y%m%d_%H%M%S", tm);
    snprintf(path, sizeof(path), "htDB/snapshot_%s.db", ts);
    hashtable_replicate(ht, path);
    log_info("snapshot saved -> %s (%d keys)", path, hashtable_count(ht));
}

/* ------------------------------------------------------------------ command dispatch */

/*
 * Returns 0 on success, -1 on encode failure.
 * Writes response into c->res.
 */
static int handle_command(client_t *c, hashtable *ht) {
    RespRequest  *req = c->req;
    RespResponse *res = c->res;

    if (req->state != OK && req->state != PART_OK)
        return encode_response_status(res, 0, "ERR bad request");

    if (req->argc == 0)
        return encode_response_status(res, 0, "ERR empty command");

    const char *cmd = request_argv(req, 0);

    /* ---- PING ---- */
    if (strcasecmp(cmd, "PING") == 0) {
        if (req->argc >= 2)
            return encode_response_string(res, request_argv(req, 1),
                                          strlen(request_argv(req, 1)));
        return encode_response_status(res, 1, "PONG");
    }

    /* ---- ECHO ---- */
    if (strcasecmp(cmd, "ECHO") == 0) {
        if (req->argc < 2) return encode_response_status(res, 0, "ERR wrong number of arguments");
        const char *msg = request_argv(req, 1);
        return encode_response_string(res, msg, strlen(msg));
    }

    /* ---- QUIT ---- */
    if (strcasecmp(cmd, "QUIT") == 0) {
        encode_response_status(res, 1, "OK");
        return 1;  /* signal caller to close */
    }

    /* ---- SET key value [EX seconds] ---- */
    if (strcasecmp(cmd, "SET") == 0) {
        if (req->argc < 3) return encode_response_status(res, 0, "ERR wrong number of arguments");
        const char *key = request_argv(req, 1);
        const char *val = request_argv(req, 2);
        time_t expire_at = 0;

        /* Parse optional [EX seconds] */
        for (int i = 3; i < req->argc - 1; i++) {
            if (strcasecmp(request_argv(req, i), "EX") == 0) {
                int secs = atoi(request_argv(req, i + 1));
                if (secs <= 0) return encode_response_status(res, 0, "ERR invalid expire time");
                expire_at = time(NULL) + secs;
                i++;
            }
        }
        int r = hashtable_set(ht, key, val, expire_at);
        if (r == -1) return encode_response_status(res, 0, "ERR out of memory");
        if (r == 0) {
            g_insert_count++;
            if (g_insert_count >= g_repl_interval) {
                raise(SIGUSR1);
                g_insert_count = 0;
            }
        }
        return encode_response_status(res, 1, "OK");
    }

    /* ---- SETNX key value ---- */
    if (strcasecmp(cmd, "SETNX") == 0) {
        if (req->argc < 3) return encode_response_status(res, 0, "ERR wrong number of arguments");
        int r = hashtable_setnx(ht, request_argv(req, 1), request_argv(req, 2), 0);
        return encode_response_integer(res, r == 0 ? 1 : 0);
    }

    /* ---- SETEX key seconds value ---- */
    if (strcasecmp(cmd, "SETEX") == 0) {
        if (req->argc < 4) return encode_response_status(res, 0, "ERR wrong number of arguments");
        int secs = atoi(request_argv(req, 2));
        if (secs <= 0) return encode_response_status(res, 0, "ERR invalid expire time");
        time_t exp = time(NULL) + secs;
        int r = hashtable_set(ht, request_argv(req, 1), request_argv(req, 3), exp);
        if (r == -1) return encode_response_status(res, 0, "ERR out of memory");
        return encode_response_status(res, 1, "OK");
    }

    /* ---- GET key ---- */
    if (strcasecmp(cmd, "GET") == 0) {
        if (req->argc < 2) return encode_response_status(res, 0, "ERR wrong number of arguments");
        char *val = hashtable_get(ht, request_argv(req, 1));
        if (!val) return encode_response_status(res, 0, "ERR key does not exist");
        return encode_response_string(res, val, strlen(val));
    }

    /* ---- GETSET key value ---- */
    if (strcasecmp(cmd, "GETSET") == 0) {
        if (req->argc < 3) return encode_response_status(res, 0, "ERR wrong number of arguments");
        char *old = hashtable_get(ht, request_argv(req, 1));
        int r;
        if (old) {
            char *copy = strdup(old);
            r = hashtable_set(ht, request_argv(req, 1), request_argv(req, 2), 0);
            if (r != -1) { encode_response_string(res, copy, strlen(copy)); }
            else           encode_response_status(res, 0, "ERR out of memory");
            free(copy);
            return r == -1 ? -1 : 0;
        }
        hashtable_set(ht, request_argv(req, 1), request_argv(req, 2), 0);
        return encode_response_status(res, 0, "ERR key does not exist");
    }

    /* ---- GETDEL key ---- */
    if (strcasecmp(cmd, "GETDEL") == 0) {
        if (req->argc < 2) return encode_response_status(res, 0, "ERR wrong number of arguments");
        char *val = hashtable_get(ht, request_argv(req, 1));
        if (!val) return encode_response_status(res, 0, "ERR key does not exist");
        char *copy = strdup(val);
        hashtable_delete(ht, request_argv(req, 1));
        encode_response_string(res, copy, strlen(copy));
        free(copy);
        return 0;
    }

    /* ---- DEL key [key ...] ---- */
    if (strcasecmp(cmd, "DEL") == 0) {
        if (req->argc < 2) return encode_response_status(res, 0, "ERR wrong number of arguments");
        int deleted = 0;
        for (int i = 1; i < req->argc; i++)
            if (hashtable_delete(ht, request_argv(req, i)) == 0) deleted++;
        return encode_response_integer(res, deleted);
    }

    /* ---- EXISTS key [key ...] ---- */
    if (strcasecmp(cmd, "EXISTS") == 0) {
        if (req->argc < 2) return encode_response_status(res, 0, "ERR wrong number of arguments");
        int found = 0;
        for (int i = 1; i < req->argc; i++)
            if (hashtable_get(ht, request_argv(req, i)) != NULL) found++;
        return encode_response_integer(res, found);
    }

    /* ---- INCR key ---- */
    if (strcasecmp(cmd, "INCR") == 0) {
        if (req->argc < 2) return encode_response_status(res, 0, "ERR wrong number of arguments");
        long result;
        int r = hashtable_incr(ht, request_argv(req, 1), 1, &result);
        if (r == -1) return encode_response_status(res, 0, "ERR value is not an integer");
        return encode_response_integer(res, (int)result);
    }

    /* ---- INCRBY key amount ---- */
    if (strcasecmp(cmd, "INCRBY") == 0) {
        if (req->argc < 3) return encode_response_status(res, 0, "ERR wrong number of arguments");
        long delta = atol(request_argv(req, 2));
        long result;
        int r = hashtable_incr(ht, request_argv(req, 1), delta, &result);
        if (r == -1) return encode_response_status(res, 0, "ERR value is not an integer");
        return encode_response_integer(res, (int)result);
    }

    /* ---- DECR key ---- */
    if (strcasecmp(cmd, "DECR") == 0) {
        if (req->argc < 2) return encode_response_status(res, 0, "ERR wrong number of arguments");
        long result;
        int r = hashtable_incr(ht, request_argv(req, 1), -1, &result);
        if (r == -1) return encode_response_status(res, 0, "ERR value is not an integer");
        return encode_response_integer(res, (int)result);
    }

    /* ---- DECRBY key amount ---- */
    if (strcasecmp(cmd, "DECRBY") == 0) {
        if (req->argc < 3) return encode_response_status(res, 0, "ERR wrong number of arguments");
        long delta = atol(request_argv(req, 2));
        long result;
        int r = hashtable_incr(ht, request_argv(req, 1), -delta, &result);
        if (r == -1) return encode_response_status(res, 0, "ERR value is not an integer");
        return encode_response_integer(res, (int)result);
    }

    /* ---- APPEND key value ---- */
    if (strcasecmp(cmd, "APPEND") == 0) {
        if (req->argc < 3) return encode_response_status(res, 0, "ERR wrong number of arguments");
        size_t new_len;
        int r = hashtable_append(ht, request_argv(req, 1), request_argv(req, 2), &new_len);
        if (r == -1) return encode_response_status(res, 0, "ERR out of memory");
        return encode_response_integer(res, (int)new_len);
    }

    /* ---- STRLEN key ---- */
    if (strcasecmp(cmd, "STRLEN") == 0) {
        if (req->argc < 2) return encode_response_status(res, 0, "ERR wrong number of arguments");
        char *val = hashtable_get(ht, request_argv(req, 1));
        return encode_response_integer(res, val ? (int)strlen(val) : 0);
    }

    /* ---- MSET key value [key value ...] ---- */
    if (strcasecmp(cmd, "MSET") == 0) {
        if (req->argc < 3 || (req->argc % 2) == 0)
            return encode_response_status(res, 0, "ERR wrong number of arguments");
        for (int i = 1; i < req->argc; i += 2)
            hashtable_set(ht, request_argv(req, i), request_argv(req, i + 1), 0);
        return encode_response_status(res, 1, "OK");
    }

    /* ---- MGET key [key ...] ---- */
    if (strcasecmp(cmd, "MGET") == 0) {
        if (req->argc < 2) return encode_response_status(res, 0, "ERR wrong number of arguments");
        int n = req->argc - 1;
        encode_response_array(res, n);
        for (int i = 1; i <= n; i++) {
            char *val = hashtable_get(ht, request_argv(req, i));
            if (val) encode_response_string(res, val, strlen(val));
            else     encode_response_status(res, 0, "nil");
        }
        return 0;
    }

    /* ---- RENAME key newkey ---- */
    if (strcasecmp(cmd, "RENAME") == 0) {
        if (req->argc < 3) return encode_response_status(res, 0, "ERR wrong number of arguments");
        char *val = hashtable_get(ht, request_argv(req, 1));
        if (!val)  return encode_response_status(res, 0, "ERR no such key");
        long ttl = hashtable_ttl(ht, request_argv(req, 1));
        time_t exp = (ttl >= 0) ? time(NULL) + ttl : 0;
        char *copy = strdup(val);
        hashtable_delete(ht, request_argv(req, 1));
        hashtable_set(ht, request_argv(req, 2), copy, exp);
        free(copy);
        return encode_response_status(res, 1, "OK");
    }

    /* ---- TYPE key ---- */
    if (strcasecmp(cmd, "TYPE") == 0) {
        if (req->argc < 2) return encode_response_status(res, 0, "ERR wrong number of arguments");
        char *val = hashtable_get(ht, request_argv(req, 1));
        return encode_response_status(res, 1, val ? "string" : "none");
    }

    /* ---- EXPIRE key seconds ---- */
    if (strcasecmp(cmd, "EXPIRE") == 0) {
        if (req->argc < 3) return encode_response_status(res, 0, "ERR wrong number of arguments");
        int secs = atoi(request_argv(req, 2));
        if (secs <= 0) return encode_response_status(res, 0, "ERR invalid expire time");
        int r = hashtable_expire(ht, request_argv(req, 1), secs);
        return encode_response_integer(res, r == 0 ? 1 : 0);
    }

    /* ---- EXPIREAT key timestamp ---- */
    if (strcasecmp(cmd, "EXPIREAT") == 0) {
        if (req->argc < 3) return encode_response_status(res, 0, "ERR wrong number of arguments");
        long ts = atol(request_argv(req, 2));
        if (ts <= (long)time(NULL)) return encode_response_integer(res, 0);
        /* Use expire with calculated delta */
        int delta = (int)(ts - (long)time(NULL));
        int r = hashtable_expire(ht, request_argv(req, 1), delta);
        return encode_response_integer(res, r == 0 ? 1 : 0);
    }

    /* ---- TTL key ---- */
    if (strcasecmp(cmd, "TTL") == 0) {
        if (req->argc < 2) return encode_response_status(res, 0, "ERR wrong number of arguments");
        long ttl = hashtable_ttl(ht, request_argv(req, 1));
        return encode_response_integer(res, (int)ttl);
    }

    /* ---- PERSIST key ---- */
    if (strcasecmp(cmd, "PERSIST") == 0) {
        if (req->argc < 2) return encode_response_status(res, 0, "ERR wrong number of arguments");
        int r = hashtable_persist(ht, request_argv(req, 1));
        return encode_response_integer(res, r == 0 ? 1 : 0);
    }

    /* ---- KEYS pattern ---- */
    if (strcasecmp(cmd, "KEYS") == 0) {
        const char *pat = (req->argc >= 2) ? request_argv(req, 1) : "*";
        int count;
        char **keys = hashtable_keys(ht, pat, &count);
        encode_response_array(res, count);
        for (int i = 0; i < count; i++) {
            encode_response_string(res, keys[i], strlen(keys[i]));
            free(keys[i]);
        }
        free(keys);
        return 0;
    }

    /* ---- DBSIZE ---- */
    if (strcasecmp(cmd, "DBSIZE") == 0)
        return encode_response_integer(res, hashtable_count(ht));

    /* ---- FLUSHDB ---- */
    if (strcasecmp(cmd, "FLUSHDB") == 0) {
        hashtable_flush(ht);
        return encode_response_status(res, 1, "OK");
    }

    /* ---- TIME ---- */
    if (strcasecmp(cmd, "TIME") == 0) {
        char buf[64];
        time_t now = time(NULL);
        struct tm *tm = localtime(&now);
        strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", tm);
        return encode_response_string(res, buf, strlen(buf));
    }

    /* ---- SAVE ---- */
    if (strcasecmp(cmd, "SAVE") == 0) {
        save_db(ht);
        return encode_response_status(res, 1, "OK");
    }

    /* ---- COPY filename ---- */
    if (strcasecmp(cmd, "COPY") == 0) {
        if (req->argc < 2) return encode_response_status(res, 0, "ERR wrong number of arguments");
        hashtable_replicate(ht, request_argv(req, 1));
        return encode_response_status(res, 1, "OK");
    }

    /* ---- INFO ---- */
    if (strcasecmp(cmd, "INFO") == 0) {
        char buf[256];
        snprintf(buf, sizeof(buf),
                 "keys:%d\r\nrepl_interval:%d\r\ninsert_count:%d\r\n",
                 hashtable_count(ht), g_repl_interval, g_insert_count);
        return encode_response_string(res, buf, strlen(buf));
    }

    /* ---- HELP ---- */
    if (strcasecmp(cmd, "HELP") == 0 || strcasecmp(cmd, "--HELP") == 0) {
        static const char help[] =
            "Commands:\r\n"
            "  SET key value [EX seconds]\r\n"
            "  SETNX key value\r\n"
            "  SETEX key seconds value\r\n"
            "  GET key\r\n"
            "  GETSET key value\r\n"
            "  GETDEL key\r\n"
            "  DEL key [key ...]\r\n"
            "  EXISTS key [key ...]\r\n"
            "  INCR key\r\n"
            "  INCRBY key amount\r\n"
            "  DECR key\r\n"
            "  DECRBY key amount\r\n"
            "  APPEND key value\r\n"
            "  STRLEN key\r\n"
            "  MSET key value [key value ...]\r\n"
            "  MGET key [key ...]\r\n"
            "  RENAME key newkey\r\n"
            "  TYPE key\r\n"
            "  EXPIRE key seconds\r\n"
            "  EXPIREAT key timestamp\r\n"
            "  TTL key\r\n"
            "  PERSIST key\r\n"
            "  KEYS pattern\r\n"
            "  DBSIZE\r\n"
            "  FLUSHDB\r\n"
            "  PING [message]\r\n"
            "  ECHO message\r\n"
            "  TIME\r\n"
            "  INFO\r\n"
            "  SAVE\r\n"
            "  COPY filename\r\n"
            "  QUIT\r\n";
        return encode_response_string(res, help, strlen(help));
    }

    return encode_response_status(res, 0, "ERR unknown command");
}

/* ------------------------------------------------------------------ main */

int main(int argc, char **argv) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <port> [repl_interval]\n", argv[0]);
        return 1;
    }

    int port = atoi(argv[1]);
    if (argc >= 3) g_repl_interval = atoi(argv[2]);

    /* Ensure htDB directory exists */
    if (mkdir("htDB", 0777) == -1 && errno != EEXIST) {
        perror("mkdir htDB"); return 1;
    }

    /* Signal setup */
    struct sigaction sa = {0};
    sa.sa_handler = sig_handler;
    sigaction(SIGUSR1, &sa, NULL);

    /* Auto-restore latest snapshot (no blocking stdin prompt) */
    hashtable *ht = hashtable_restore();
    log_info("database loaded: %d keys", hashtable_count(ht));

    /* Server socket */
    int server_fd = server_init(port);
    if (listen(server_fd, SOMAXCONN) < 0) { perror("listen"); return 1; }
    log_info("listening on port %d", port);

    /* epoll setup */
    int epoll_fd = epoll_create1(0);
    if (epoll_fd < 0) { perror("epoll_create1"); return 1; }

    struct epoll_event ev = {0};
    ev.events   = EPOLLIN;
    ev.data.fd  = server_fd;
    if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, server_fd, &ev) < 0) {
        perror("epoll_ctl"); return 1;
    }

    struct epoll_event events[MAX_EVENTS];
    time_t last_expire_sweep = time(NULL);

    log_info("server ready (repl_interval=%d)", g_repl_interval);

    while (1) {
        /* Use a 1-second timeout so we can run periodic tasks */
        int nfds = epoll_wait(epoll_fd, events, MAX_EVENTS, 1000);
        if (nfds < 0) {
            if (errno == EINTR) continue;
            perror("epoll_wait");
            break;
        }

        /* Periodic: save if signal raised */
        if (g_save_flag) {
            save_db(ht);
            g_save_flag = 0;
        }

        /* Periodic: passive TTL sweep */
        time_t now = time(NULL);
        if (now - last_expire_sweep >= EXPIRE_INTERVAL) {
            int evicted = hashtable_evict_expired(ht);
            if (evicted > 0) log_info("TTL sweep evicted %d keys", evicted);
            last_expire_sweep = now;
        }

        for (int i = 0; i < nfds; i++) {
            int fd = events[i].data.fd;

            /* New connection */
            if (fd == server_fd) {
                struct sockaddr_in client_addr;
                socklen_t addrlen = sizeof(client_addr);
                int client_fd = accept(server_fd, (struct sockaddr *)&client_addr, &addrlen);
                if (client_fd < 0) {
                    if (errno != EAGAIN) perror("accept");
                    continue;
                }
                set_nonblocking(client_fd);

                char addr_str[INET_ADDRSTRLEN];
                inet_ntop(AF_INET, &client_addr.sin_addr, addr_str, sizeof(addr_str));

                client_t *client = client_new(client_fd, addr_str);
                if (!client) { close(client_fd); continue; }

                struct epoll_event cev = {0};
                cev.events    = EPOLLIN | EPOLLET;
                cev.data.ptr  = client;
                if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, client_fd, &cev) < 0) {
                    perror("epoll_ctl add client");
                    client_free(client);
                    close(client_fd);
                    continue;
                }
                log_info("client connected: %s (fd=%d)", addr_str, client_fd);
                continue;
            }

            /* Data from existing client */
            client_t *client = (client_t *)events[i].data.ptr;

            /* Handle disconnection or error */
            if (events[i].events & (EPOLLHUP | EPOLLERR)) {
                log_info("client disconnected: %s (fd=%d)", client->addr, client->fd);
                epoll_ctl(epoll_fd, EPOLL_CTL_DEL, client->fd, NULL);
                close(client->fd);
                client_free(client);
                continue;
            }

            /* Read loop (edge-triggered: read until EAGAIN) */
            int close_conn = 0;
            while (!close_conn) {
                char buf[BUF_SIZE];
                ssize_t rnum = read(client->fd, buf, sizeof(buf));

                if (rnum < 0) {
                    if (errno != EAGAIN && errno != EWOULDBLOCK)
                        close_conn = 1;
                    break;
                }
                if (rnum == 0) { close_conn = 1; break; }

                reset_request(client->req);
                reset_response(client->res);

                if (decode_request(client->req, buf, (uint32_t)rnum) != 0) {
                    encode_response_status(client->res, 0, "ERR parse error");
                } else {
                    int cmd_result = handle_command(client, ht);
                    if (cmd_result == 1) close_conn = 1;  /* QUIT */
                }

                /* Write response */
                ssize_t wnum = write(client->fd, client->res->buf, client->res->used_size);
                if (wnum < 0) { close_conn = 1; break; }
            }

            if (close_conn) {
                log_info("client disconnected: %s (fd=%d)", client->addr, client->fd);
                epoll_ctl(epoll_fd, EPOLL_CTL_DEL, client->fd, NULL);
                close(client->fd);
                client_free(client);
            }
        }
    }

    /* Cleanup */
    save_db(ht);
    hashtable_free(ht);
    close(epoll_fd);
    close(server_fd);
    return 0;
}
