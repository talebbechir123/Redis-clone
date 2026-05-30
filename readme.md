# pASE1 — Redis-Compatible In-Memory Key-Value Server

A high-performance, Redis-compatible in-memory key-value store written in C.
Implements the [RESP protocol](https://redis.io/docs/reference/protocol-spec/), meaning it works out of the box with `redis-cli` and any Redis client library.

**Author:** Ahmed Taleb Bechir

---

## Features

- **RESP protocol** — fully compatible with `redis-cli` and Redis client libraries
- **epoll-based I/O** — O(1) event dispatch, handles thousands of concurrent connections
- **TTL support** — per-key expiry with passive eviction and periodic sweeps
- **Persistence** — automatic snapshot-based durability with configurable save intervals
- **Auto-restore** — loads the latest snapshot on startup with no user prompt
- **25+ commands** — full string, numeric, bulk, and TTL command set
- **Case-insensitive commands** — `SET`, `set`, `Set` all work
- **FNV-1a hash** — excellent key distribution, low collision rate
- **Zero compiler warnings** — built with `-Wall -Wextra`

---

## Building

```bash
make          # builds both server and client
make server   # server only
make client   # client only
make clean    # remove compiled artifacts
```

**Requirements:** GCC, Linux (uses `epoll`)

---

## Running

**Server:**
```bash
./server <port> [replication_interval]

# Example: listen on 6379, save every 50 inserts
./server 6379 50
```

**Built-in client:**
```bash
./client 127.0.0.1 6379
```

**redis-cli** (recommended):
```bash
redis-cli -p 6379
```

---

## Command Reference

### String operations
| Command | Description |
|---------|-------------|
| `SET key value [EX seconds]` | Set a key, optionally with expiry |
| `SETNX key value` | Set only if key does not exist (returns 1/0) |
| `SETEX key seconds value` | Set with expiry |
| `GET key` | Get the value of a key |
| `GETSET key value` | Set new value, return old value |
| `GETDEL key` | Get value and delete the key |
| `APPEND key value` | Append to a string, return new length |
| `STRLEN key` | Return the length of a string |

### Numeric operations
| Command | Description |
|---------|-------------|
| `INCR key` | Increment integer value by 1 |
| `INCRBY key amount` | Increment by a given amount |
| `DECR key` | Decrement integer value by 1 |
| `DECRBY key amount` | Decrement by a given amount |

### Key management
| Command | Description |
|---------|-------------|
| `DEL key [key ...]` | Delete one or more keys, returns count deleted |
| `EXISTS key [key ...]` | Count how many of the given keys exist |
| `RENAME key newkey` | Rename a key (preserves TTL) |
| `TYPE key` | Returns `string` or `none` |
| `KEYS pattern` | List keys matching a glob pattern (e.g. `user:*`) |
| `DBSIZE` | Number of live keys in the database |
| `FLUSHDB` | Delete all keys |

### Bulk operations
| Command | Description |
|---------|-------------|
| `MSET key value [key value ...]` | Set multiple keys atomically |
| `MGET key [key ...]` | Get multiple values in one call |

### TTL / expiry
| Command | Description |
|---------|-------------|
| `EXPIRE key seconds` | Set a TTL on an existing key |
| `EXPIREAT key timestamp` | Set expiry as a Unix timestamp |
| `TTL key` | Remaining TTL in seconds (-1 = no TTL, -2 = missing) |
| `PERSIST key` | Remove the TTL from a key |

### Server
| Command | Description |
|---------|-------------|
| `PING [message]` | Health check |
| `ECHO message` | Echo a message back |
| `TIME` | Return the server's current time |
| `INFO` | Database stats (key count, replication interval) |
| `SAVE` | Manually trigger a database snapshot |
| `COPY filename` | Export the database to a named file |
| `QUIT` | Close the current connection |
| `HELP` | Print the command list |

---

## Architecture

### Networking — epoll
The server uses Linux `epoll` in edge-triggered mode (`EPOLLET`). Unlike `select`, epoll does not scan all file descriptors on every call — it receives only the fds that became ready. This gives O(1) event dispatch regardless of how many clients are connected.

Each accepted connection gets its own `client_t` struct containing a dedicated `RespRequest` and `RespResponse` buffer, stored directly in the epoll event's `data.ptr`. No global shared state between connections.

### Hash table — FNV-1a
Keys are hashed with the [FNV-1a 32-bit](http://www.isthe.com/chongo/tech/comp/fnv/) algorithm, which has excellent avalanche behavior and is significantly faster and more uniform than simple polynomial hashes. Collisions are resolved by chaining.

### TTL expiry — two-level strategy
1. **Lazy expiry** — `GET`, `EXISTS`, `TTL`, etc. check `expire_at` on every access and delete the key inline if it has expired.
2. **Active sweep** — every 5 seconds the server scans all buckets and evicts expired keys, preventing unbounded memory growth from keys that are never accessed again.

### Persistence — snapshot-based
On every N inserts (configurable via `replication_interval`), `SIGUSR1` is raised. The signal handler sets a flag; the main loop checks the flag after each `epoll_wait` and writes a timestamped snapshot to `htDB/snapshot_YYYYMMDD_HHMMSS.db`. The snapshot format is tab-separated (`key\tvalue\texpire_at`) so values containing commas or spaces are handled safely.

On startup the server finds the most recently modified file in `htDB/` and restores it, skipping any keys whose TTL has already passed.

---

## Project Structure

```
pASE1/
├── server.c              # Main server: epoll loop, command dispatch
├── include/
│   ├── server.h          # Server declarations and macros
│   ├── hashtable.h       # Hash table interface
│   ├── resp.h            # RESP protocol structs
│   └── replication.h     # Persistence interface
├── src/
│   ├── hashtable.c       # FNV-1a hash, TTL, INCR, APPEND, KEYS, ...
│   ├── resp.c            # RESP encoder / decoder
│   ├── replication.c     # Snapshot save and restore
│   └── client.c          # Interactive RESP client
├── htDB/                 # Runtime snapshot directory (gitignored)
└── makefile
```

---

## References

- [RESP Protocol Specification](https://redis.io/docs/reference/protocol-spec/)
- [Beej's Guide to Network Programming](https://beej.us/guide/bgnet/html/)
- [FNV Hash](http://www.isthe.com/chongo/tech/comp/fnv/)
- [epoll — Linux manual page](https://man7.org/linux/man-pages/man7/epoll.7.html)
- [Redis Command Reference](https://redis.io/commands/)
- [RESP C implementation reference](https://github.com/interma/RESP)
