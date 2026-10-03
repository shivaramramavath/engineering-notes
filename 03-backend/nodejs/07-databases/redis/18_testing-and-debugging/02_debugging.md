# Debugging

Debugging Redis problems is mostly **observation**: what did the client send, what did the server receive, and what state is it in? Work from the outside in: Node first, then `redis-cli`, then server diagnostics.

```
Symptom ──► Which layer?
              │
              ├─ Client (Node): events, status, command trace, DEBUG=ioredis:*
              ├─ Network:       connect/timeout errors, TLS, DNS, redis-cli --latency
              ├─ Server:        INFO, SLOWLOG, LATENCY, CLIENT LIST, MONITOR
              └─ Data:          TYPE, TTL, OBJECT ENCODING, MEMORY USAGE, key prefix, DB
```

## Debugging mindset

1. **Reproduce** with the smallest script possible (one client, one command)
2. **Observe** before changing anything: logs, `INFO`, `SLOWLOG`
3. **Narrow down** the layer: same command from `redis-cli` works? Then it is the client or the data
4. **Change one thing at a time**
5. **Write a regression test** when you find the cause

A very useful first question: *does the same command work from `redis-cli` against the same server, with the same credentials?*

## Toolbox

| Tool | Use for | Production-safe? |
|------|---------|------------------|
| `redis-cli` | Manual commands, `--scan`, `--bigkeys`, `--latency` | Yes (read-only commands) |
| RedisInsight (GUI) | Browsing keys, slow log, memory analysis | Yes, with a read-only user |
| `INFO` | Health, memory, clients, stats, replication | Yes |
| `SLOWLOG` | Slow commands | Yes |
| `LATENCY` | Latency spikes and their causes | Yes |
| `CLIENT LIST` | Who is connected, from where, doing what | Yes |
| `MONITOR` | Live stream of every command | **No** (use for seconds, on a test or replica) |
| `KEYS *` | Listing keys | **No** (use `SCAN`) |
| `DEBUG ...` | Internals, sleeps, reloads | **No** (disable in production) |
| `DEBUG=ioredis:*` | Client-side protocol logs | Development only |

## Observing from Node

### Log connection events

```ts
import Redis from "ioredis";

const redis = new Redis({ host: "redis.internal", connectionName: "orders-api" });

redis.on("connect",      () => log.info("redis connect"));
redis.on("ready",        () => log.info("redis ready"));
redis.on("error",        (err) => log.error({ err: err.message, code: (err as any).code }, "redis error"));
redis.on("close",        () => log.warn("redis connection closed"));
redis.on("reconnecting", (ms: number) => log.warn({ ms }, "redis reconnecting"));
redis.on("end",          () => log.warn("redis connection ended (no more retries)"));

setInterval(() => log.debug({ status: redis.status }), 30_000);
```

`redis.status` is one of `wait`, `connecting`, `connect`, `ready`, `reconnecting`, `close`, `end`. A client stuck in `reconnecting` explains many "Redis is slow" reports.

Setting `connectionName` makes the client show up by name in `CLIENT LIST`, which is invaluable when ten services share one server.

### ioredis debug logs

```bash
DEBUG=ioredis:* node app.js
DEBUG=ioredis:redis node app.js       # commands and replies
DEBUG=ioredis:cluster node app.js     # slot and node handling
```

This prints connection steps, queued commands and reconnect attempts. It is verbose and may include data, so keep it out of production.

### See the commands your app sends

Option 1: ioredis' built-in `monitor()` (a wrapper around `MONITOR`):

```ts
const monitor = await redis.monitor();
monitor.on("monitor", (time, args, source, database) => {
  console.log(new Date(Number(time) * 1000).toISOString(), database, args.join(" "));
});
// ... reproduce the problem ...
monitor.disconnect();
```

`MONITOR` makes the server echo **every** command from **every** client. It slows the server, and it exposes secrets (`AUTH`, values). Use it on a development instance or for a few seconds at most.

Option 2: a tracing client for development:

```ts
import Redis, { Command } from "ioredis";

class TracingRedis extends Redis {
  sendCommand(command: Command, stream?: any): unknown {
    const start = performance.now();
    const done = () => {
      const ms = (performance.now() - start).toFixed(1);
      console.log(`[redis] ${command.name} ${String(command.args[0] ?? "")} ${ms}ms`);
    };
    command.promise.then(done, done);        // log on success and on failure
    return super.sendCommand(command, stream);
  }
}
```

This relies on ioredis internals that can change between versions, so keep it dev-only.

Option 3 (safest, works in production): time **your own** Redis calls:

```ts
async function timed<T>(label: string, fn: () => Promise<T>, slowMs = 50): Promise<T> {
  const start = performance.now();
  try {
    return await fn();
  } finally {
    const ms = performance.now() - start;
    if (ms > slowMs) log.warn({ label, ms: Math.round(ms) }, "slow redis call");
  }
}

const user = await timed("user.get", () => redis.hgetall(`user:${id}`));
```

Slow calls from the app side but nothing in `SLOWLOG` points at the network, a saturated event loop or a queued connection instead of Redis itself.

## `redis-cli` essentials

```bash
redis-cli -h host -p 6379 --user app --pass '...'    # connect (add --tls for TLS)
redis-cli -n 2 GET mykey                              # database 2
redis-cli --raw GET mykey                             # no escaping of unicode

# inspect a key
TYPE user:1042
TTL user:1042                    # -1 no expiry, -2 key does not exist
OBJECT ENCODING user:1042
MEMORY USAGE user:1042
DEBUG OBJECT user:1042           # internals (dev only)

# explore safely
redis-cli --scan --pattern 'session:*' | head
redis-cli --bigkeys
redis-cli --memkeys

# server health
redis-cli --stat                 # rolling one-line stats
redis-cli --latency              # round-trip latency
redis-cli --latency-history
redis-cli --intrinsic-latency 5  # baseline of the machine itself (run on the server)
```

Use `SCAN` (via `--scan`), never `KEYS *`, on anything real. See [Scan and Iteration](../05_key-management/02_scan-and-iteration.md).

## Server-side diagnostics

### `INFO`

```ts
function parseInfo(raw: string): Record<string, string> {
  const out: Record<string, string> = {};
  for (const line of raw.split("\r\n")) {
    if (!line || line.startsWith("#")) continue;
    const i = line.indexOf(":");
    if (i > 0) out[line.slice(0, i)] = line.slice(i + 1);
  }
  return out;
}

const stats = parseInfo(await redis.info("stats"));
const hits = Number(stats.keyspace_hits);
const misses = Number(stats.keyspace_misses);
console.log("hit ratio", hits / Math.max(1, hits + misses));
```

| Section | Field | What it tells you |
|---------|-------|-------------------|
| `clients` | `connected_clients`, `blocked_clients` | Connection leaks, stuck blocking calls |
| `stats` | `instantaneous_ops_per_sec` | Current load |
| `stats` | `keyspace_hits`, `keyspace_misses` | Cache effectiveness |
| `stats` | `expired_keys`, `evicted_keys` | Why keys vanished |
| `stats` | `rejected_connections` | `maxclients` reached |
| `memory` | `used_memory`, `maxmemory`, `mem_fragmentation_ratio` | Memory pressure |
| `persistence` | `rdb_last_bgsave_status`, `aof_last_write_status` | Failing saves (can block writes) |
| `persistence` | `latest_fork_usec` | Fork cost that causes latency spikes |
| `replication` | `role`, `master_link_status`, `master_repl_offset` | Replica health and lag |
| `keyspace` | `db0:keys=...,expires=...` | Key counts and how many have a TTL |
| `commandstats` | `cmdstat_<name>` | Which commands dominate, with call counts and time |

`INFO commandstats` is excellent for finding "who is calling `KEYS` a million times".

### `SLOWLOG`

```bash
# redis.conf (or CONFIG SET)
slowlog-log-slower-than 10000    # microseconds: log anything over 10 ms
slowlog-max-len 128
```

```ts
const entries = (await redis.call("SLOWLOG", "GET", "10")) as any[];
for (const [id, ts, micros, args, client, name] of entries) {
  console.log(id, new Date(ts * 1000).toISOString(), `${micros}µs`, args.slice(0, 3).join(" "), client, name);
}
```

The slow log records only **execution time inside Redis**, not queueing or network time. Typical offenders: `KEYS`, `SMEMBERS`/`HGETALL` on huge collections, `DEL` of big keys, long Lua scripts and `SORT`.

### `LATENCY`

```bash
latency-monitor-threshold 100    # in redis.conf: record events slower than 100 ms
```

```bash
redis-cli LATENCY LATEST
redis-cli LATENCY DOCTOR         # human-readable analysis
redis-cli LATENCY HISTORY command
```

It names the **cause** of spikes: fork for persistence, AOF fsync, expiry cycles, slow commands.

### `CLIENT LIST`

```ts
const raw = (await redis.client("LIST")) as string;
const clients = raw.trim().split("\n").map((line) =>
  Object.fromEntries(line.split(" ").map((kv) => kv.split("=", 2) as [string, string])),
);
// each client: id, addr, name, age, idle, db, cmd, flags, qbuf, omem ...
const idleForever = clients.filter((c) => Number(c.idle) > 3600);
```

| Field | Meaning |
|-------|---------|
| `name` | Value of `connectionName` (use it!) |
| `addr` | Client address |
| `age`, `idle` | Seconds connected, seconds since the last command |
| `cmd` | Last command run |
| `flags` | `S` replica, `P` pub/sub, `b` blocked, `x` in MULTI |
| `omem` | Output buffer memory (large values mean a slow consumer) |

Many connections from one `addr` with the same name usually means a **connection leak**: a client created per request. Kill a misbehaving client with `CLIENT KILL ID <id>`.

## Error reference

Most Redis problems announce themselves in an error message.

| Error | Meaning | Usual cause and fix |
|-------|---------|---------------------|
| `ECONNREFUSED` | Nothing listening | Wrong host or port, Redis down, container not exposed, firewall |
| `ETIMEDOUT` / `connect timeout` | No response to the TCP connect | Network, security group, wrong IP. Tune `connectTimeout` |
| `ECONNRESET` | Peer closed the connection | TLS mismatch (plain client on TLS port), `timeout` setting, server restart, proxy idle timeout |
| `ENOTFOUND` | DNS failure | Typo, service name not resolvable from this network |
| `Connection is closed.` | Command sent after `quit()` or `end` | Reusing a closed client, shutdown order bug |
| `Stream isn't writeable and enableOfflineQueue options is false` | Not connected and offline queue disabled | Expected when failing fast. Handle it, or enable the queue |
| `Reached the max retries per request limit (which is 20)` (`MaxRetriesPerRequestError`) | Reconnect kept failing | Redis unreachable for a while. Blocking/BullMQ connections need `maxRetriesPerRequest: null` |
| `NOAUTH Authentication required.` | No credentials sent | Add `username` and `password` |
| `WRONGPASS invalid username-password pair` | Bad credentials or disabled user | Check the secret and `ACL GETUSER` |
| `NOPERM ...` | ACL denies the command, key or channel | See [Authentication and ACL](../17_security/01_authentication-and-acl.md), check `ACL LOG` |
| `WRONGTYPE Operation against a key holding the wrong kind of value` | Wrong command for the key's type | `TYPE key`, key naming collisions, stale data from an old deploy |
| `ERR value is not an integer or out of range` | `INCR` on a non-number, `EX` with a float | Check the stored value, use `PX` for fractions |
| `OOM command not allowed when used memory > 'maxmemory'` | Memory full with `noeviction` | Free memory, add TTLs, change policy, scale |
| `LOADING Redis is loading the dataset in memory` | Server still starting | Retry with backoff. Keep `enableReadyCheck: true` |
| `READONLY You can't write against a read only replica.` | Writing to a replica, often after failover | Connect via Sentinel or Cluster aware client, check topology and DNS caching |
| `BUSY Redis is busy running a script` | A long Lua script blocks the server | Shorten the script. `SCRIPT KILL` if it has not written yet |
| `NOSCRIPT No matching script. Please use EVAL.` | Script cache flushed (restart, failover) | Fall back to `EVAL`. ioredis `defineCommand` handles this for you |
| `MISCONF Redis is configured to save RDB snapshots, but it's currently unable to persist to disk` | Failed background save blocks writes | Check disk space and permissions, `INFO persistence` |
| `ERR max number of clients reached` | `maxclients` hit | Connection leak or too many instances. Check `CLIENT LIST` |
| `CROSSSLOT Keys in request don't hash to the same slot` | Multi-key operation across Cluster slots | Hash tags: `{user:42}:profile` |
| `MOVED` / `ASK` | Key lives on another node | Use a Cluster-aware client. Normally handled for you |
| `EXECABORT Transaction discarded because of previous errors` | A command in `MULTI` failed at queue time | Fix the offending command |
| `Connection in subscriber mode, only subscriber commands may be used` | Normal commands on a subscribed connection | Use `redis.duplicate()` for a separate subscriber |

Always log `err.name`, `err.message` and `err.code`, and **never** log connection options (they contain passwords).

## Scenario playbooks

### "My key disappeared"

Work down the list. Each check eliminates a cause.

| Check | Command | Meaning |
|-------|---------|---------|
| Does it exist at all? | `EXISTS key` | Rule out typos |
| Right database? | `redis-cli -n <db>`, check ioredis `db` option | Keys are per DB (and Cluster has only DB 0) |
| Right prefix? | Look for `keyPrefix` in client options | `redis-cli` shows the full key |
| Expired? | `INFO stats` → `expired_keys`, check your TTLs | A `SET` without `KEEPTTL` also *removes* a TTL |
| Evicted? | `INFO stats` → `evicted_keys`, `CONFIG GET maxmemory-policy` | Cache policies delete under memory pressure |
| Deleted by code? | `MONITOR` briefly on dev, or search for `DEL`/`UNLINK`/`RENAME` | Cleanup jobs, other services |
| Server restarted without persistence? | `INFO server` → `uptime_in_seconds`, `INFO persistence` | Data in memory was lost |
| Failover with async replication? | `INFO replication` | Recent writes may not have reached the replica |
| Different environment? | Compare `host`, `db`, secrets | Staging versus production mix-ups |

```bash
redis-cli EXISTS user:1042
redis-cli TTL user:1042
redis-cli --scan --pattern '*1042*'      # find it under another name or prefix
```

### "Redis is slow"

```bash
redis-cli --latency                  # network + server round-trip
redis-cli SLOWLOG GET 10             # slow commands
redis-cli LATENCY DOCTOR
redis-cli INFO commandstats          # who calls what, how often
redis-cli INFO persistence           # latest_fork_usec, background saves running
redis-cli INFO memory                # mem_fragmentation_ratio < 1 means swapping
redis-cli CLIENT LIST                # blocked clients, huge output buffers
```

| Finding | Cause | Fix |
|---------|-------|-----|
| `KEYS`, `SMEMBERS`, `HGETALL` in slow log | O(N) on big collections | `SCAN`-family commands, split keys |
| Spikes every few minutes | Fork for RDB or AOF rewrite | Smaller dataset per instance, check `latest_fork_usec`, disable THP on Linux |
| Latency fine on server, slow in app | Event loop busy, connection pool, sequential awaits | Pipeline, profile Node, check `redis.status` |
| High `evicted_keys` plus slow | Memory-bound instance | Add memory, shrink values, fix TTLs |
| `mem_fragmentation_ratio` below 1 | Swapping to disk | Add RAM or reduce dataset immediately |
| Latency scales with value size | Large values | Compress, split, see [Serialization](../16_performance/04_serialization.md) |
| Many tiny round trips | N+1 pattern | Pipelining and `MGET` |

More in [Latency and Benchmarking](../16_performance/01_latency-and-benchmarking.md).

### "Connections keep growing"

```ts
const raw = (await redis.client("LIST")) as string;
const byName: Record<string, number> = {};
for (const line of raw.trim().split("\n")) {
  const name = /(?:^| )name=(\S*)/.exec(line)?.[1] || "(unnamed)";
  byName[name] = (byName[name] ?? 0) + 1;
}
console.table(byName);
```

Look for a service whose count only rises: that is a `new Redis()` per request or per job. Create **one** client per process (plus dedicated ones for subscribers and blocking calls) and close it on shutdown. See [Graceful Shutdown](../03_ioredis-basics/06_graceful-shutdown.md).

### "WRONGTYPE"

```bash
redis-cli TYPE user:1042          # string? hash? list?
redis-cli OBJECT ENCODING user:1042
```

Causes: two features sharing a key name, an old deploy that wrote a different structure, or `SET` on what should be a hash. Fix with namespaced keys from one key-builder, and migrate or delete the stale key.

### Pub/Sub messages are missing

```bash
redis-cli PUBSUB CHANNELS 'orders:*'      # active channels
redis-cli PUBSUB NUMSUB orders:created    # subscribers per channel
redis-cli PUBSUB NUMPAT                   # active pattern subscriptions
```

- `NUMSUB` is `0`: nobody was subscribed when you published. Pub/Sub keeps no history
- Subscriber and publisher use different databases or Redis instances (channels are global to a server, not per DB)
- The subscriber connection died and re-subscribed after the message (ioredis re-subscribes automatically by default, but messages in between are lost)
- Need delivery guarantees? Use Streams. See [Pub/Sub Fundamentals](../09_pub-sub/01_pub-sub-fundamentals.md)

### Stream or consumer group stuck

```bash
redis-cli XINFO STREAM orders
redis-cli XINFO GROUPS orders           # pending counts, lag, last-delivered-id
redis-cli XINFO CONSUMERS orders workers
redis-cli XPENDING orders workers - + 10   # which messages are un-acked and for how long
```

A growing pending list means consumers read but do not `XACK` (crash, exception before ack). Reclaim with `XAUTOCLAIM`. A stream that keeps growing needs `MAXLEN`/`MINID` trimming. See [Consumer Groups and Acks](../10_streams/03_consumer-groups-and-acks.md).

### A lock never releases

```bash
redis-cli GET lock:report
redis-cli PTTL lock:report      # -1: no TTL (deadlock risk), -2: gone
```

No TTL means the lock was created without `EX`/`PX`. A lock that vanishes early means work outlasted the TTL. See [Locking Fundamentals](../11_distributed-locks/01_locking-fundamentals.md).

### Lua script misbehaves

- Return values: log them with `cjson.encode` and return a string, since Lua numbers become **integers** (decimals are dropped) and Lua `false` becomes `nil`
- Write to the server log from the script:

```lua
redis.log(redis.LOG_WARNING, "current=" .. tostring(current))
```

- Step through it with the built-in Lua debugger (development server only):

```bash
redis-cli --ldb --eval script.lua key1 key2 , arg1 arg2
```

The comma separates keys from arguments. In debug mode use `step`, `print`, `break` and `continue`.

### Cluster issues

```bash
redis-cli -c -h node1 -p 6379             # -c follows MOVED redirects
redis-cli CLUSTER INFO                    # cluster_state:ok, slots assigned
redis-cli CLUSTER NODES                   # topology, flags, fail states
redis-cli CLUSTER KEYSLOT '{user:42}:profile'
redis-cli CLUSTER SHARDS                  # (7.0+) shard layout
```

```ts
// SCAN and KEYS run per node: check every master
const masters = cluster.nodes("master");
const counts = await Promise.all(masters.map((n) => n.dbsize()));
```

### Replication lag and failover

```bash
redis-cli INFO replication
# role:master, connected_slaves:2, slave0:...,lag=0
# on a replica: master_link_status:up, master_last_io_seconds_ago, slave_repl_offset
redis-cli -p 26379 SENTINEL masters      # (Sentinel) current primary and quorum state
```

Compare `master_repl_offset` on the primary with `slave_repl_offset` on replicas. A growing gap means a slow replica or saturated link. Reading from replicas always carries a small staleness risk.

### Persistence problems

```bash
redis-cli INFO persistence    # rdb_last_bgsave_status:ok, aof_last_write_status:ok, loading:0
redis-cli LASTSAVE
```

If `rdb_last_bgsave_status` is `err`, writes may be refused (`MISCONF`). Check disk space, write permissions on `dir`, and available memory for the fork.

## Debugging data problems from code

### Inspect a key generically

```ts
async function inspect(key: string) {
  const type = await redis.type(key);
  const ttl = await redis.pttl(key);
  const encoding = await redis.object("ENCODING", key);
  const bytes = await redis.memory("USAGE", key);

  let preview: unknown;
  switch (type) {
    case "string": preview = await redis.get(key); break;
    case "hash":   preview = await redis.hgetall(key); break;
    case "list":   preview = await redis.lrange(key, 0, 9); break;
    case "set":    preview = (await redis.sscan(key, 0, "COUNT", 10))[1]; break;
    case "zset":   preview = await redis.zrange(key, 0, 9, "WITHSCORES"); break;
    case "stream": preview = await redis.xrange(key, "-", "+", "COUNT", 5); break;
  }
  return { key, type, ttlMs: ttl, encoding, bytes, preview };
}
```

The `preview` reads are bounded so this is safe even on large keys.

## Debugging safely in production

| Do | Don't |
|----|-------|
| Use `SCAN`, `--scan`, bounded reads (`LRANGE 0 9`, `HSCAN`) | `KEYS *`, `HGETALL` or `SMEMBERS` on unknown keys |
| Read `INFO`, `SLOWLOG`, `LATENCY`, `CLIENT LIST` | `MONITOR` on a busy primary |
| Inspect from a **replica** when possible | Run `DEBUG` commands |
| Use a read-only ACL user for investigations | Debug as an admin user by default |
| Name connections and tag logs with request IDs | Log values or credentials |
| Copy a key to a test instance with `DUMP`/`RESTORE` to experiment | Experiment on the live key |
| Keep a record of every write you make | Run `FLUSHALL`, `DEL pattern` loops casually |

```bash
# copy a key to a dev instance for experiments (the original stays untouched because of COPY)
redis-cli MIGRATE dev-redis 6379 session:abc 0 5000 COPY
```

Create a **read-only debugging user** in advance so you are not tempted to use the admin account during an incident (the `command|subcommand` rules need Redis 7.0+):

```
user debug on >... ~* &* +@read +@connection +info +slowlog +latency +client|list +memory +config|get +scan +type +ttl +pttl +object
```

## Quick reference

| I want to... | Command |
|--------------|---------|
| See a key's type, TTL, size | `TYPE`, `TTL`, `MEMORY USAGE` |
| Find keys safely | `redis-cli --scan --pattern 'x:*'` |
| See slow commands | `SLOWLOG GET 10` |
| See who is connected | `CLIENT LIST` |
| See command mix | `INFO commandstats` |
| Check cache effectiveness | `INFO stats` → hits and misses |
| See why keys vanish | `INFO stats` → `expired_keys`, `evicted_keys` |
| Check replication | `INFO replication` |
| Check persistence | `INFO persistence` |
| Measure latency | `redis-cli --latency` |
| Watch commands live (briefly) | `MONITOR` (dev only) |

## Pitfalls

| Pitfall | Fix |
|---------|-----|
| Leaving `MONITOR` running | It slows the server and leaks secrets. Use for seconds |
| Using `KEYS *` to look around | `SCAN` / `--scan` |
| Guessing instead of reading `INFO` | Check stats, slow log and clients first |
| Blaming Redis for app-side slowness | Time calls in the app and compare with `SLOWLOG` |
| Not naming connections | Set `connectionName` per service |
| Ignoring `error` events | Always attach a handler and log `code` and `message` |
| Logging connection options or URLs | Redact secrets |
| Debugging as admin on the primary | Read-only user, replica where possible |
| Forgetting `keyPrefix` when looking for keys | `redis-cli` shows the full prefixed key |
| Forgetting the DB number | Check `db` in the client and `-n` in `redis-cli` |

## Key takeaways

- Debug in layers: client, network, server, data
- Name connections and log events so problems are attributable
- `INFO`, `SLOWLOG`, `LATENCY` and `CLIENT LIST` answer most questions without touching your data
- Learn the error messages: most name their own cause
- `MONITOR`, `KEYS` and `DEBUG` are for development, not production
- Prepare a read-only debug user and a few saved commands before you need them

**Previous:** [Testing with Redis](./01_testing-with-redis.md) | **Next:** [Common Pitfalls](./03_common-pitfalls.md)
