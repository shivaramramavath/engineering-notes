# Redis Architecture

Understanding the internals explains Redis performance, its limits and why some commands are dangerous.

## Big picture

```
 Node.js app (ioredis)
        │  TCP + RESP
        ▼
 ┌───────────────────────────────────────────┐
 │ Redis server                              │
 │  Network I/O  →  Event loop  →  Command   │
 │  (sockets)       (single       execution  │
 │                  thread)       (in memory)│
 │                                   │       │
 │              Persistence  ◄───────┤       │
 │              (RDB / AOF)          │       │
 │              Replication ◄────────┘       │
 └───────────────────────────────────────────┘
```

## 1. Single-threaded command execution

Redis executes commands **one at a time on a single main thread**, driven by an event loop (multiplexing sockets with `epoll`/`kqueue`).

**Why this is fast**

- No locks or context switching between commands
- Data lives in RAM, so operations take microseconds
- Efficient data structures and a tiny protocol

**Consequences**

- Every command is **atomic** by nature
- One slow command (`KEYS *`, huge `SMEMBERS`, long Lua script) **blocks everyone**
- One instance uses one CPU core for commands; scale out with Cluster

## 2. Threaded I/O (Redis 6+)

Reading and writing sockets can be offloaded to extra **I/O threads** (`io-threads` config, off by default). Command execution stays single-threaded. Background threads also handle tasks like lazy freeing (`UNLINK`), AOF fsync and closing files.

## 3. RESP protocol

Clients talk to Redis over TCP using **RESP** (REdis Serialization Protocol), a simple text-based format.

```
Client:  *3\r\n$3\r\nSET\r\n$5\r\ngreet\r\n$5\r\nhello\r\n
Server:  +OK\r\n
```

- RESP2: strings, errors, integers, bulk strings, arrays
- RESP3: adds maps, sets, doubles, booleans and push messages
- ioredis handles encoding and decoding for you

**Why it matters for Node.js:** a request is a round trip. Ten sequential commands cost ten round trips. Pipelining sends them together (covered in `06_advanced-commands`).

## 4. Memory model

- All data is in RAM, keyed in a global hash table
- Values use compact encodings (e.g. `listpack`, `intset`) for small collections, switching to full structures as they grow
- `maxmemory` plus an **eviction policy** (`allkeys-lru`, `volatile-ttl`, `noeviction`, ...) decides what happens when memory is full
- Expiry uses **lazy deletion** (checked on access) plus periodic active sampling
- Memory fragmentation can make RSS larger than logical data size

## 5. Persistence

| Mode          | How it works                                               | Trade-off                                     |
| ------------- | ---------------------------------------------------------- | --------------------------------------------- |
| **RDB**       | Point-in-time snapshots via forked child                   | Compact, fast restart, may lose recent writes |
| **AOF**       | Append-only log of writes (`fsync` always / everysec / no) | Better durability, larger files               |
| **RDB + AOF** | Both enabled                                               | Common production choice                      |
| **None**      | Pure cache                                                 | Fastest, data lost on restart                 |

Snapshots use `fork()` with copy-on-write, so heavy write load during a save can raise memory use.

## 6. Replication

```
   Primary ──async──► Replica 1
      └────async────► Replica 2
```

- Replicas copy the primary's data asynchronously
- Use them for **read scaling** and **failover**
- Asynchronous replication means a failover can lose the last few writes

## 7. High availability with Sentinel

**Redis Sentinel** monitors a primary and its replicas, detects failure, promotes a replica and tells clients the new address. ioredis supports Sentinel natively.

## 8. Horizontal scaling with Cluster

- Keyspace split into **16384 hash slots**
- Slot = `CRC16(key) mod 16384`, each primary owns a range of slots
- Multi-key commands must target keys in the **same slot** (use hash tags: `{user:1}:profile`, `{user:1}:cart`)
- Clients follow `MOVED` / `ASK` redirects. ioredis `Cluster` does this automatically

```
 Slots 0-5460      Slots 5461-10922     Slots 10923-16383
 ┌─────────┐       ┌─────────┐          ┌─────────┐
 │Primary A│       │Primary B│          │Primary C│
 └────┬────┘       └────┬────┘          └────┬────┘
   Replica A          Replica B           Replica C
```

## 9. How ioredis fits in

- Opens a TCP connection and **multiplexes** commands over it (no pool needed for most apps)
- Queues commands while reconnecting (`offlineQueue`)
- Supports pipelines, transactions, Lua, Pub/Sub, Streams
- Requires **separate connections** for subscribers (a subscribing connection can only run subscribe commands)

```js
const redis = new Redis(); // commands
const subscriber = new Redis(); // dedicated to SUBSCRIBE

await subscriber.subscribe("news");
subscriber.on("message", (channel, msg) => console.log(channel, msg));
await redis.publish("news", "hello");
```

## Design implications

| Fact                         | What to do                                                |
| ---------------------------- | --------------------------------------------------------- |
| Single-threaded execution    | Avoid O(N) commands on large keys; use `SCAN`, not `KEYS` |
| Round trips dominate latency | Pipeline or batch commands                                |
| Memory is finite             | Set `maxmemory`, TTLs and an eviction policy              |
| Async replication            | Don't assume zero data loss on failover                   |
| Cluster slots                | Design keys with hash tags for multi-key operations       |

## Key takeaways

- Redis is fast because of **RAM + single-threaded simplicity + efficient structures**.
- The same single thread means **one slow command hurts all clients**.
- Persistence, replication, Sentinel and Cluster are layers you add for durability and scale.

**Next:** [When Not to Use Redis](./04_when-not-to-use-redis.md)
