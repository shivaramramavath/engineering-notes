# Redis CLI and Insight

Two tools you will use constantly: `redis-cli` for fast terminal work and **Redis Insight** for visual browsing.

## Part 1: redis-cli

### Connect

```bash
redis-cli                                   # localhost:6379
redis-cli -h 127.0.0.1 -p 6379              # explicit host/port
redis-cli -a "yourpassword"                 # with password
redis-cli --user app --pass "secret"        # ACL user
redis-cli -u redis://user:pass@host:6379/0  # URL form
redis-cli --tls -h host -p 6380             # TLS
docker exec -it redis redis-cli             # inside a Docker container
```

### First commands

```
127.0.0.1:6379> PING
PONG
127.0.0.1:6379> SET name "Ada"
OK
127.0.0.1:6379> GET name
"Ada"
127.0.0.1:6379> SET otp 123456 EX 30
OK
127.0.0.1:6379> TTL otp
(integer) 27
127.0.0.1:6379> INCR visits
(integer) 1
127.0.0.1:6379> DEL name
(integer) 1
```

### Explore data types

```
HSET user:1 name Ada age 36
HGETALL user:1
LPUSH tasks "send-email" "resize-image"
LRANGE tasks 0 -1
SADD tags redis nodejs
SMEMBERS tags
ZADD scores 100 alice 250 bob
ZRANGE scores 0 -1 WITHSCORES
TYPE user:1
```

### Inspect the server

```
INFO                  # full stats
INFO memory           # memory section only
DBSIZE                # number of keys in current DB
SELECT 1              # switch database
CONFIG GET maxmemory  # read config
CLIENT LIST           # connected clients
SLOWLOG GET 10        # slowest recent commands
```

### Find keys safely

```
SCAN 0 MATCH user:* COUNT 100
```

```bash
redis-cli --scan --pattern 'user:*'
```

> **Never run `KEYS *` on a production server.** It is O(N) and blocks all clients. Use `SCAN`.

### Monitoring and diagnostics

```bash
redis-cli monitor                # stream every command (debug only, adds overhead)
redis-cli --stat                 # rolling stats
redis-cli --latency              # measure latency
redis-cli --bigkeys              # find large keys (uses SCAN)
redis-cli --memkeys              # memory usage per key type
redis-cli MEMORY USAGE user:1    # bytes used by one key
```

### Handy tricks

```bash
redis-cli DEL $(redis-cli --scan --pattern 'tmp:*')   # OK for dev; use UNLINK in bulk
redis-cli FLUSHDB                                     # wipe current DB (dev only!)
redis-cli --rdb backup.rdb                            # download a snapshot
redis-cli -x SET blob < file.bin                      # read value from stdin
```

Inside the interactive shell: press **Tab** for command completion, `HELP SET` for command docs, `CLEAR` to clear the screen.

## Part 2: Redis Insight

**Redis Insight** is the official free GUI for Redis.

### Install

- **Docker:** see the Compose file in [01_redis-installation](./01_redis-installation.md), then open <http://localhost:5540>
- **Desktop app:** download from the Redis website (Windows, macOS, Linux)

### Connect to a database

1. Click **Add Redis database**
2. Enter host, port, and username/password if any
3. Click **Add**

(Reminder: from Insight running in Docker Compose, the host is `redis`, not `localhost`.)

### Key features

| Feature             | Use it for                                                 |
| ------------------- | ---------------------------------------------------------- |
| **Browser**         | Search, view, edit and delete keys with type-aware editors |
| **Workbench**       | Run commands with syntax help and history                  |
| **CLI panel**       | Built-in redis-cli                                         |
| **Profiler**        | Live view of commands (like `MONITOR`)                     |
| **Slow Log**        | See slow commands                                          |
| **Memory Analysis** | Find big keys and memory hogs                              |
| **Pub/Sub**         | Subscribe to channels and publish test messages            |
| **Streams viewer**  | Inspect stream entries and consumer groups                 |

### Workflow tip

Run your Node.js code, then open **Browser** in Insight and watch keys appear, change and expire. It builds intuition faster than reading docs.

### Cautions

- Browsing with a `*` filter on a huge production DB can be heavy. Filter narrowly.
- Avoid the Profiler on busy production servers.
- Use a **read-only ACL user** for production inspection.

## CLI vs Insight

| Task                                      | Prefer           |
| ----------------------------------------- | ---------------- |
| Quick check, scripting, SSH into a server | `redis-cli`      |
| Browsing many keys, viewing nested data   | Insight          |
| Debugging a live app visually             | Insight Profiler |
| Automation and CI                         | `redis-cli`      |

**Next:** [ioredis Setup](./03_ioredis-setup.md)
