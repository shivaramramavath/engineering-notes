# Common Pitfalls

A catalog of mistakes that show up again and again in Redis code, grouped by area. Each entry shows what goes wrong and what to do instead. Skim it before a code review, and use it as a source of regression tests.

```
Connections ─► Commands ─► Data ─► Expiry ─► Atomicity ─► Scripts ─► Messaging ─► Cluster ─► Durability
```

## 1. Connections

### One client per request

```ts
// Wrong: a new connection (and TCP/TLS handshake) for every request
app.get("/user/:id", async (req, res) => {
  const redis = new Redis();
  res.json(await redis.hgetall(`user:${req.params.id}`));
});

// Right: one long-lived client per process
const redis = new Redis(process.env.REDIS_URL!);
app.get("/user/:id", async (req, res) => {
  res.json(await redis.hgetall(`user:${req.params.id}`));
});
```

Symptoms: connection count climbs, `max number of clients reached`, rising latency. See [Connection Management](../08_nodejs-integration/01_connection-management.md).

### Sharing one connection for everything

| Situation | Problem | Fix |
|-----------|---------|-----|
| `SUBSCRIBE` on the main client | The connection enters subscriber mode and rejects normal commands | `redis.duplicate()` for the subscriber |
| `BLPOP`, `BRPOP`, `XREAD BLOCK` on the main client | Blocks every other command on that connection | Dedicated connection per blocking consumer |
| `WATCH` on a shared client | Other requests can interfere with the watched state | A dedicated connection per transaction, or use Lua |

```ts
const sub = redis.duplicate();
await sub.subscribe("events");
```

### No `error` handler

Connection errors are emitted as events. Without a listener, ioredis can only log them, and you never see the cause.

```ts
redis.on("error", (err) => log.error({ err: err.message }, "redis error"));
```

### Wrong defaults for the situation

| Setting | Default | Watch out for |
|---------|---------|---------------|
| `enableOfflineQueue` | `true` | Commands pile up while disconnected, using memory and adding latency. Disable if you want to fail fast |
| `maxRetriesPerRequest` | `20` | Commands fail with `MaxRetriesPerRequestError` after repeated reconnects. BullMQ workers and blocking connections need `null` |
| `connectTimeout` | 10 s | Slow to notice a dead endpoint. Tune it |
| `commandTimeout` | none | A hung server hangs your request. Set a timeout where latency matters |
| `retryStrategy` | Exponential up to 2 s | Fine for most, but add jitter for large fleets |

### Not closing on shutdown

Leaving connections open keeps the process alive, hangs tests and drops in-flight commands.

```ts
process.on("SIGTERM", async () => {
  await server.close();
  await redis.quit();             // waits for pending replies, unlike disconnect()
});
```

### `localhost` inside a container

Inside Docker, `localhost` is the container itself. Use the service name (`host: "redis"`) or `host.docker.internal` for a host-side Redis.

## 2. Commands

| Pitfall | Why it hurts | Instead |
|---------|--------------|---------|
| `KEYS pattern` in production | O(N) and blocks the server | `SCAN` / `scanStream` |
| `HGETALL`, `SMEMBERS`, `LRANGE 0 -1` on unbounded collections | Blocks and floods memory | `HSCAN`, `SSCAN`, bounded ranges, split keys |
| `DEL` on a huge key | Blocks while freeing memory | `UNLINK` |
| `await` inside a loop | One round trip per iteration | Pipeline, `MGET`, `Promise.all` on a pipeline |
| `FLUSHALL` / `FLUSHDB` as cleanup | Wipes everything, including other tenants | Scoped deletes by prefix |
| `SELECT` for multi-tenancy | Not supported in Cluster, easy to mix up | Key prefixes |
| `MONITOR` or `DEBUG` in production | Slows the server, exposes secrets | See [Debugging](./02_debugging.md) |

```ts
// Wrong: N round trips
for (const id of ids) users.push(await redis.hgetall(`user:${id}`));

// Right: one round trip
const pipe = redis.pipeline();
ids.forEach((id) => pipe.hgetall(`user:${id}`));
const res = await pipe.exec();
```

### `SCAN` surprises

- It can return the **same key more than once**. Deduplicate if it matters
- `COUNT` is a hint, not a limit or a page size
- `MATCH` filters **after** the keys are fetched, so a rare pattern still walks the whole keyspace
- Keys created or deleted during the scan may or may not appear

See [Scan and Iteration](../05_key-management/02_scan-and-iteration.md).

### `keyPrefix` surprises

The ioredis `keyPrefix` option prefixes key arguments, but:

- Keys **returned** by `KEYS` and `SCAN` include the prefix. Passing them back to a command prefixes them again, producing keys that do not exist
- Keys built inside Lua from `ARGV` are not prefixed
- Channel names are not prefixed

Verify the behavior against your ioredis version, or build keys explicitly in one place (see [Redis Key Builder](../08_nodejs-integration/05_redis-key-builder.md)) and skip the option.

## 3. Data and types

### Everything comes back as a string

```ts
await redis.set("count", 5);
typeof (await redis.get("count"));        // "string"

const h = await redis.hgetall("user:1");  // { logins: "17", active: "1" }
Number(h.logins);                          // convert explicitly
h.active === "1";                          // booleans too
```

### `undefined`, `null` and objects as values

```ts
// Wrong: undefined or null fields are turned into strings or empty values, not skipped
await redis.hset("user:1", { name: "Ada", nickname: undefined });

// Right: remove empty fields first
const clean = Object.fromEntries(Object.entries(obj).filter(([, v]) => v != null));
await redis.hset("user:1", clean);

// Wrong: nested objects become "[object Object]"
await redis.hset("user:1", { prefs: { theme: "dark" } });

// Right: serialize nested parts yourself
await redis.hset("user:1", { prefs: JSON.stringify({ theme: "dark" }) });
```

### Large integers and floats

- JavaScript numbers are only exact up to 2^53 - 1. `INCR` returns a JS number, so counters beyond that lose precision. Read as a string and use `BigInt`, or reset before reaching it
- Never store money as floats. Use integer cents, or `INCRBYFLOAT` knowing it returns a decimal string
- `HGETALL` and `GET` return strings, so `"0.1" + "0.2"` concatenates

### `WRONGTYPE`

Two features sharing a key name, or a deploy that changed a key's structure. Namespace keys (`cache:user:1` vs `session:user:1`), build them in one place and check with `TYPE key`.

### JSON gotchas

`Date`, `Map`, `Set` and `BigInt` do not survive `JSON.stringify` as you expect (see [Serialization](../16_performance/04_serialization.md)).

### Version-dependent commands

A command that works on your laptop's Redis 7.4 can fail on a managed 6.2 instance.

| Feature | Since |
|---------|-------|
| `SET ... KEEPTTL`, ACLs, RESP3 | 6.0 |
| `GETEX`, `GETDEL`, `HRANDFIELD`, `SET ... GET` | 6.2 |
| `EXPIRE` with `NX`/`XX`/`GT`/`LT`, `LMPOP`, Functions, listpack encodings | 7.0 |
| Per-field hash expiry (`HEXPIRE`) | 7.4 |

Check with `INFO server` (`redis_version`) and test against the production version.

## 4. Expiry and TTL

### `SET` removes the TTL

```ts
await redis.set("k", "v", "EX", 60);
await redis.set("k", "v2");               // Wrong: the key is now permanent
await redis.set("k", "v2", "KEEPTTL");    // Right: keeps the remaining TTL (Redis 6.0+)
await redis.set("k", "v2", "EX", 60);     // Or set the TTL again
```

Most other write commands (`INCR`, `HSET`, `LPUSH`) keep the TTL. Overwriting commands (`SET`, `GETSET`, `RESTORE`) do not.

### Separate `SET` and `EXPIRE`

```ts
// Wrong: a crash between the two leaves a key that never expires
await redis.set("session:1", data);
await redis.expire("session:1", 3600);

// Right: one command, or one transaction
await redis.set("session:1", data, "EX", 3600);
await redis.multi().hset("session:2", obj).expire("session:2", 3600).exec();
```

### `EX` wants an integer

```ts
await redis.set("k", "v", "EX", 1.5);   // ERR value is not an integer or out of range
await redis.set("k", "v", "PX", 1500);  // use milliseconds
```

### More expiry traps

- `EXPIRE key 0` or a negative value **deletes the key immediately**
- `EXPIRE` on a missing key returns `0` and does nothing
- A hash has one TTL for the whole key (unless you use 7.4+ field expiry)
- Expiry is lazy plus sampled, so `DBSIZE` can include keys that are already logically expired
- Do not compare Redis TTLs with the app server's clock. Use relative TTLs, or `redis.time()` if you need the server clock
- A cache with **no TTL** grows until it hits `maxmemory`. See [Expiration and TTL](../02_redis-fundamentals/03_expiration-and-ttl.md)

## 5. Atomicity and transactions

### Read-modify-write

```ts
// Wrong: two requests read the same value, both write n + 1
const n = Number(await redis.get("stock"));
await redis.set("stock", n - 1);

// Right: one atomic command
await redis.decr("stock");

// Right: conditional logic that must be atomic belongs in Lua (or WATCH/MULTI)
```

### Check-then-set

```ts
// Wrong: race between EXISTS and SET
if (!(await redis.exists("lock:job"))) await redis.set("lock:job", "1");

// Right
const ok = await redis.set("lock:job", token, "EX", 30, "NX");
```

### `MULTI` does not roll back

```ts
const res = await redis.multi()
  .set("a", "1")
  .lpush("a", "x")      // WRONGTYPE at execution time
  .incr("b")
  .exec();
// res = [[null, "OK"], [Error("WRONGTYPE ..."), undefined], [null, 1]]
```

- `exec()` resolves even when individual commands failed. **Check every `[err, value]` pair**
- Commands that ran stay applied. There is no rollback
- If `WATCH` detects a change, `exec()` returns `null`. Retry the whole transaction
- In Cluster, all keys in a transaction must hash to the same slot

See [Transactions](../06_advanced-commands/02_transactions.md).

### Pipelines are not atomic

A pipeline batches **round trips**, not isolation. Other clients' commands can interleave, and a failed command does not stop the rest. Results come back as `[err, value]` pairs, so inspect them:

```ts
const results = await redis.pipeline().get("a").incr("b").exec();
for (const [err, value] of results ?? []) {
  if (err) log.error({ err: err.message }, "pipeline command failed");
}
```

Very large pipelines (hundreds of thousands of commands) hold lots of memory on both sides. Send them in batches.

## 6. Lua scripts

| Pitfall | Detail | Fix |
|---------|--------|-----|
| Decimals disappear | A Lua number returned to Redis becomes an **integer** (`3.7` becomes `3`) | Return `tostring(x)` |
| `false` becomes `nil` | Lua `false` converts to a nil reply, `true` to `1` | Return explicit numbers or strings |
| `ARGV` are strings | `ARGV[1] + 1` can misbehave | `tonumber(ARGV[1])` |
| Keys not declared in `KEYS` | Breaks Cluster routing and ACL key checks | Pass every key through `KEYS` |
| Long loops | The server is blocked for the whole run (`BUSY` for others) | Keep scripts short and bounded |
| Non-determinism assumptions | Scripts must be safe to replicate | Avoid relying on time or random outside Redis helpers |
| `NOSCRIPT` after restart or failover | The script cache is empty | Use `defineCommand` (handles it) or retry with `EVAL` |
| `redis.call` errors abort the script | An error raises immediately | Use `redis.pcall` when you want to handle errors |

```ts
// Wrong: user data concatenated into the script text
await redis.eval(`return redis.call("GET", "user:${id}")`, 0);

// Right: data through KEYS/ARGV
await redis.eval(`return redis.call("GET", KEYS[1])`, 1, `user:${id}`);
```

See [Lua Scripts](../06_advanced-commands/03_lua-scripts.md).

## 7. Caching

| Pitfall | Effect | Fix |
|---------|--------|-----|
| No TTL | Unbounded memory | TTL on every cached key |
| Same TTL everywhere | Mass expiry causes a stampede | Add random jitter to TTLs |
| No protection against concurrent misses | Many requests rebuild one value (thundering herd) | Per-key lock, request coalescing, stale-while-revalidate |
| Caching `null` and errors without a plan | Repeated misses hit the database (penetration) | Cache negative results briefly |
| Caching failure breaks the request | Redis outage means API outage | Fail open: catch errors, fall back to the source |
| Invalidating after the database write only | Races serve stale data | Delete after commit, short TTL as a safety net |
| Cache and primary data in one instance with `allkeys-lru` | Eviction can delete **non-cache** data such as queues or sessions | Separate instances, or use `volatile-*` policies with TTLs only on cache keys |

```ts
const jitter = (base: number) => base + Math.floor(Math.random() * base * 0.1);
await redis.set(key, value, "EX", jitter(3600));
```

See [Cache Problems](../07_caching/04_cache-problems.md).

## 8. Messaging

### Pub/Sub

- Delivery is **at most once** and has no history. Offline subscribers miss messages
- Subscribers need their own connection
- Do slow work outside the `message` handler, or you delay the next message
- A connection drop loses messages in the gap, even though ioredis re-subscribes automatically
- Need replay, acknowledgements or consumer groups? Use [Streams](../10_streams/01_stream-fundamentals.md)

### Streams

| Pitfall | Fix |
|---------|-----|
| Consumers read but never `XACK` | Ack after successful processing, in a `finally`-safe way |
| Crashed consumers leave messages pending forever | `XAUTOCLAIM` or `XCLAIM` after an idle threshold |
| Assuming exactly-once | Delivery is at-least-once, so make handlers idempotent |
| Stream grows forever | `XADD ... MAXLEN ~ N` or `XTRIM` |
| Creating a group on a missing stream fails | `XGROUP CREATE ... MKSTREAM`, and handle `BUSYGROUP` (already exists) |

### Queues

- A list-based queue with `RPOP` loses a job if the worker dies after popping. Use reliable patterns (`LMOVE` to a processing list, Streams, or BullMQ)
- Retried jobs without backoff or a dead-letter queue turn into infinite loops
- Jobs must be idempotent, because they will occasionally run twice

See [Retries and Dead Letter Queue](../14_queues-and-workers/03_retries-and-dead-letter-queue.md).

## 9. Distributed locks

| Pitfall | Consequence | Fix |
|---------|-------------|-----|
| `SET lock 1 NX` without a TTL | A crash leaves the lock forever | Always `EX` or `PX` |
| Releasing with plain `DEL` | You can delete another client's lock after yours expired | Store a unique token, release with a compare-and-delete Lua script |
| TTL shorter than the work | Two holders at once | Size the TTL generously and renew while working |
| Treating a lock as a hard guarantee | A pause or failover can still cause overlap | Use fencing tokens for critical sections, make operations idempotent |

```ts
const RELEASE = `
  if redis.call("GET", KEYS[1]) == ARGV[1] then
    return redis.call("DEL", KEYS[1])
  end
  return 0
`;
const token = crypto.randomUUID();
const got = await redis.set("lock:job", token, "PX", 30_000, "NX");
// ... work ...
await redis.eval(RELEASE, 1, "lock:job", token);
```

See [Locking Fundamentals](../11_distributed-locks/01_locking-fundamentals.md).

## 10. Cluster and replication

| Pitfall | Detail | Fix |
|---------|--------|-----|
| `CROSSSLOT` | `MGET`, `MULTI`, Lua or set operations across slots | Hash tags: `{user:42}:profile`, `{user:42}:cart` |
| `KEYS` and `SCAN` | Only run on one node | Loop over `cluster.nodes("master")` |
| `SELECT` | Only database 0 exists | Prefix keys instead |
| One hot key | One slot, one node takes all the load | Shard the key, add a local cache |
| Overusing hash tags | All data lands in one slot | Use tags only where multi-key operations require them |
| Reading from replicas for read-after-write | Replication is asynchronous, so you can read stale data | Read your own writes from the primary |
| `READONLY` errors after failover | Client still points at the old primary | Use Sentinel/Cluster aware clients and avoid DNS caching |
| Assuming replicas are backups | A bad write or `FLUSHALL` replicates instantly | Real backups, see [Backup and Disaster Recovery](../19_redis-production/03_backup-and-disaster-recovery.md) |

## 11. Durability and memory

- Redis is **not durable by default for every write**. With AOF `everysec` you can lose about a second of writes, and with RDB-only you can lose everything since the last snapshot
- Replication is asynchronous. A failover can drop acknowledged writes. `WAIT` narrows the window but does not close it
- No `maxmemory` leads to an out-of-memory kill. `maxmemory` with `noeviction` leads to `OOM` write errors. Pick deliberately
- A fork during `BGSAVE` can need extra memory under write load. Leave headroom
- Backups that are never restored are not backups

See [Persistence](../02_redis-fundamentals/04_persistence.md) and [Memory Optimization](../16_performance/03_memory-optimization.md).

## 12. Testing

| Pitfall | Fix |
|---------|-----|
| Tests share keys or call `FLUSHALL` | Per-test prefix and scoped cleanup |
| Only mocked Redis | Add real-Redis integration tests |
| Fixed sleeps for TTL tests | Poll with a timeout |
| Fake timers expected to move the server clock | They do not. Use short real TTLs |
| Open connections keep Jest or Vitest from exiting | `quit()` in `afterAll` |
| Only happy-path tests | Test down, slow, dropped and restarted Redis |

See [Testing with Redis](./01_testing-with-redis.md).

## 13. Security

| Pitfall | Fix |
|---------|-----|
| Redis reachable from the internet | Private network, firewall, `bind` |
| No auth, or one shared admin password | ACL users per service |
| User input inside keys, patterns or script text | Validate, escape, pass data through `ARGV` |
| Secrets in logs and connection URLs | Redact, use a secret manager |

See [Security Checklist](../17_security/03_security-checklist.md).

## Self-review checklist

```
Connections
[ ] One shared client per process; separate ones for subscribers and blocking reads
[ ] error handler attached; quit() on shutdown; connectionName set
Commands
[ ] No KEYS, no unbounded HGETALL/SMEMBERS, UNLINK for big deletes
[ ] No await-in-a-loop; pipelines and MGET where it matters
Data
[ ] Types converted on read; no undefined/null fields; nested values serialized
[ ] Keys built in one place; namespaces prevent WRONGTYPE
Expiry
[ ] Every cache/session key has a TTL (with jitter); SET does not silently drop TTLs
Atomicity
[ ] No read-modify-write or check-then-set; MULTI/pipeline results inspected
Scripts
[ ] Keys in KEYS, data in ARGV; return strings for decimals; scripts bounded
Messaging
[ ] Pub/Sub not used where delivery matters; streams acked and trimmed; jobs idempotent
Locks
[ ] TTL set; unique token; compare-and-delete release
Cluster
[ ] Multi-key operations share a hash tag; SCAN across all masters
Durability
[ ] maxmemory and policy chosen on purpose; persistence matches the data's value; backups tested
```

## Key takeaways

- Most Redis bugs are one of a few families: connection handling, O(N) commands, string-typed data, lost TTLs, non-atomic sequences and wrong assumptions about delivery or durability
- Prefer single atomic commands, then Lua, over multi-step client logic
- Treat TTLs, timeouts and failure handling as part of the feature, not as extras
- Check results of pipelines and transactions: errors are returned, not thrown
- Test against the real thing, and keep this list next to your code review checklist

**Previous:** [Debugging](./02_debugging.md) | **Next:** [Deployment](../19_redis-production/01_deployment.md)
