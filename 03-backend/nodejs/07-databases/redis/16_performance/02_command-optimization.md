# Command Optimization

Because Redis executes commands one at a time, **the cost of your commands is the cost of your system**. This lesson is about choosing commands and shapes of data that stay cheap: avoiding O(N) traps, taming big keys, cutting round trips, and finding offenders with `SLOWLOG`.

## Know the complexity

Every command documents its **time complexity**. Three cost classes matter in practice:

| Class | Examples | Behavior |
|-------|----------|----------|
| **O(1) / O(log N)** | `GET`, `SET`, `HGET`, `SADD`, `ZADD`, `ZRANK`, `LPUSH`, `INCR` | Safe at any size |
| **O(N) in what you ask for** | `MGET` (N keys), `LRANGE a b`, `ZRANGE a b`, `HMGET` | Safe if **you bound N** |
| **O(N) in the size of the key or keyspace** | `KEYS`, `SMEMBERS`, `HGETALL`, `HKEYS`, `LRANGE 0 -1`, `ZRANGE 0 -1`, `SORT`, `SUNION`/`SINTER` on big sets, `DEL` of a big key, `FLUSHALL` (sync) | **Dangerous on big keys**: blocks everyone |

The question for every command is: **"what is N, and who controls it?"** If the answer is "the data, which grows", you have a time bomb.

### Dangerous commands and their replacements

| Instead of | Problem | Use |
|------------|---------|-----|
| `KEYS pattern` | O(keyspace), blocking | `SCAN` / `scanStream` ([Scan](../05_key-management/02_scan-and-iteration.md)) |
| `SMEMBERS big` | O(N), blocking, huge reply | `SSCAN`, `SCARD`, `SISMEMBER`, `SRANDMEMBER` |
| `HGETALL big` | Same | `HSCAN`, or `HMGET` only the fields you need |
| `LRANGE key 0 -1` | Whole list | Bounded ranges, `LTRIM`, or `LLEN` |
| `ZRANGE key 0 -1` | Whole set | `ZRANGE … LIMIT`, `ZCOUNT`, `ZREVRANGE 0 99` |
| `DEL bigkey` | Frees memory on the main thread | `UNLINK` ([Delete and Unlink](../05_key-management/03_delete-and-unlink.md)) |
| `SINTER` / `SUNION` on huge sets | CPU and memory | Precompute with `SINTERSTORE` and a TTL, or reshape the data |
| `SORT` on big collections | O(N log N) | Keep the data sorted (sorted sets) |
| `FLUSHALL` / `FLUSHDB` (sync) | Blocks | `ASYNC` in dev only. Block in prod with ACLs |
| `MONITOR` | Roughly halves throughput, leaks data | Short, sampled sessions, never in production |
| Long Lua scripts | Block the server | Short scripts, bounded loops ([Lua](../06_advanced-commands/03_lua-scripts.md)) |
| `SAVE` | Blocks while writing the snapshot | `BGSAVE` |

## Finding the offenders

### 1. `SLOWLOG` and `INFO commandstats`

Covered in [Latency and Benchmarking](./01_latency-and-benchmarking.md#built-in-diagnostics). Use **both**: `SLOWLOG` finds individual slow calls, and `commandstats` finds commands that are cheap each time but **dominate total CPU** through volume.

### 2. Big-key and hot-key scans

```bash
redis-cli --bigkeys             # the largest key per type (uses SCAN)
redis-cli --memkeys             # memory per key, sampled
redis-cli --hotkeys             # requires an LFU eviction policy
redis-cli MEMORY USAGE mykey SAMPLES 0
```

`--bigkeys` reports by **element count** (and string length), not bytes. `--memkeys` and `MEMORY USAGE` report **bytes**. Run them against a **replica** or off-peak, because they scan the keyspace.

```ts
export async function bigKeys(redis: Redis, pattern = "*", top = 20) {
  const found: { key: string; type: string; bytes: number }[] = [];

  for await (const keys of redis.scanStream({ match: pattern, count: 500 })) {
    if (!keys.length) continue;
    const p = redis.pipeline();
    keys.forEach((k: string) => { p.memory("USAGE", k, "SAMPLES", 5); p.type(k); });
    const res = (await p.exec()) ?? [];

    keys.forEach((k: string, i: number) => {
      const bytes = res[i * 2]?.[1];
      const type = res[i * 2 + 1]?.[1];
      if (typeof bytes === "number") found.push({ key: k, type: String(type), bytes });
    });
  }
  return found.sort((a, b) => b.bytes - a.bytes).slice(0, top);
}
```

### 3. Client-side visibility

Wrap your Redis access so slow or huge operations show up in application metrics ([`LatencyRecorder`](./01_latency-and-benchmarking.md#measuring-from-nodejs)), and log the **size** of replies you read (`Buffer.byteLength`, array length), because a sudden jump in reply size is an early warning.

## Big keys

A **big key** is a single key whose value or collection is large enough to cause trouble. There is no universal threshold, but a rough guide is **strings above a few hundred KB** and **collections above tens of thousands of elements** (or above a few MB in total).

### Why they hurt

| Effect | Why |
|--------|-----|
| **Latency spikes** | Any O(N) operation on it blocks the thread |
| **Slow deletes and expiry** | Freeing memory is synchronous unless you `UNLINK` |
| **Replication and failover lag** | Big values saturate the replication stream |
| **Network saturation** | One read moves megabytes |
| **Cluster imbalance** | A key can't be split across shards |
| **Painful migration and resharding** | `MIGRATE` of a huge key blocks |
| **Memory fragmentation and spikes** | Large allocations and copies |

### Fixes

| Fix | How |
|-----|-----|
| **Cap it** | `LTRIM`, `ZREMRANGEBYRANK`, `XADD … MAXLEN ~`, TTLs |
| **Split it (bucketing)** | Spread one logical collection over many keys |
| **Change the structure** | A big list used for membership checks should be a set |
| **Shrink the values** | [Memory Optimization](./03_memory-optimization.md), [Serialization](./04_serialization.md) |
| **Keep blobs elsewhere** | Store the object in object storage, keep a reference in Redis |
| **Read in pages** | Never fetch the whole thing |
| **Delete safely** | `UNLINK`, or incremental deletion |

Bucketing a huge set (for example per-day or by hash):

```ts
const BUCKETS = 64;

const bucketOf = (member: string) =>
  createHash("md5").update(member).digest().readUInt32BE(0) % BUCKETS;

const setKey = (name: string, member: string) => `shop:${name}:${bucketOf(member)}`;

await redis.sadd(setKey("visitors", userId), userId);                             // writes: one small set
const isMember = await redis.sismember(setKey("visitors", userId), userId);       // reads: one small set

// total count: sum across buckets (pipelined)
const p = redis.pipeline();
for (let b = 0; b < BUCKETS; b++) p.scard(`shop:visitors:${b}`);
const total = ((await p.exec()) ?? []).reduce((n, [, c]) => n + Number(c), 0);
```

In Cluster, buckets spread across shards automatically, which also helps balance.

## Cut round trips

Most "Redis is slow" reports are really "my code makes 500 sequential calls". This is the highest-value optimization.

### The N+1 pattern

```ts
// ✗ N+1: one round trip per id
const users = [];
for (const id of ids) users.push(await redis.hgetall(`user:${id}`));

// ✓ one round trip
const p = redis.pipeline();
ids.forEach((id) => p.hgetall(`user:${id}`));
const users = ((await p.exec()) ?? []).map(([, u]) => u);

// ✓ even simpler with concurrent calls and auto pipelining
const users = await Promise.all(ids.map((id) => redis.hgetall(`user:${id}`)));     // with enableAutoPipelining: true
```

| Situation | Best tool |
|-----------|-----------|
| Many strings | `MGET` / `MSET` (one command) |
| Many hashes, sets, mixed types | Pipeline or auto pipelining |
| Read-modify-write that must be atomic | Lua |
| Concurrent calls from many request handlers | Auto pipelining ([Pipelines](../06_advanced-commands/01_pipelines-and-auto-pipelining.md)) |
| Independent writes at the end of a request | One pipeline |

### Fetch only what you need

```ts
await redis.hgetall("user:1");                              // ✗ every field, including big ones
await redis.hmget("user:1", "name", "plan");                // ✓ two fields

await redis.zrange("lb", 0, -1, "WITHSCORES");              // ✗ the whole leaderboard
await redis.zrange("lb", 0, 9, "REV", "WITHSCORES");        // ✓ the top 10
```

### Bound every batch

Giant `MGET`s and pipelines are **also** big operations. Chunk them:

```ts
async function mgetChunked(redis: Redis, keys: string[], size = 500) {
  const out: (string | null)[] = [];
  for (let i = 0; i < keys.length; i += size) out.push(...(await redis.mget(keys.slice(i, i + size))));
  return out;
}
```

A 100,000-key `MGET` blocks Redis for a noticeable time and builds a huge reply in Node. Chunks of a few hundred to a few thousand keep each command short and let other clients interleave.

### Let Redis do the work

Move **small, bounded** logic to Redis to save round trips (a Lua script that does check-and-set, or `SET NX EX` instead of get-then-set). Keep scripts **short**, because they block the server ([Lua](../06_advanced-commands/03_lua-scripts.md#rules-and-limits)). Prefer a single built-in command when one exists.

## Choose the structure by the operation

| Operation | Good | Poor |
|-----------|------|------|
| Membership test | Set (`SISMEMBER` O(1)) | List (`LPOS` O(N)) |
| Top-N / ranking | Sorted set | Sorting in Node after `SMEMBERS` |
| Unique count | HyperLogLog, or `SCARD` on a bounded set | `SMEMBERS` and counting in Node |
| Recent items | Capped list (`LPUSH` + `LTRIM`) or stream with `MAXLEN` | Unbounded list |
| Per-object fields | Hash (`HMGET`, `HINCRBY`) | JSON string you rewrite for each field |
| Range by time | Sorted set (`BYSCORE`) | Scanning keys |
| Existence of many IDs | Bitmap | A key per ID |

See [Choosing the Right Structure](../04_data-structures/09_choosing-the-right-structure.md).

## Expiry and eviction costs

- **Mass expiry at the same instant** spikes CPU in the active-expire cycle, so add TTL **jitter**
- Deleting or evicting a **big key** frees memory on the main thread. Use `UNLINK` and the `lazyfree-*` options ([Delete and Unlink](../05_key-management/03_delete-and-unlink.md#lazy-freeing-configuration))
- `EXPIRE` on millions of keys at once is itself a burst, so spread such jobs out

## Pub/Sub and blocking commands

- `PUBLISH` costs about **subscribers plus matching patterns**, so a few thousand pattern subscriptions slow every publish ([Pub/Sub Fundamentals](../09_pub-sub/01_pub-sub-fundamentals.md#patterns))
- Slow subscribers accumulate **output buffers** and are disconnected at the limit (`client-output-buffer-limit pubsub`)
- Blocking commands (`BLPOP`, `XREADGROUP BLOCK`) need **dedicated connections**, and a queue of idle blocked connections is cheap, but thousands of connections are not ([Connection Management](../08_nodejs-integration/01_connection-management.md))
- Avoid `WAIT` on hot paths unless you need it, since it adds a replica round trip to the request

## Client-side costs in Node.js

Redis can be fast while your **process** is slow:

| Cost | Mitigation |
|------|------------|
| Sequential `await` in loops | `Promise.all`, pipelines, auto pipelining |
| Huge `JSON.parse` / `stringify` on the event loop | Smaller payloads, [binary formats or compression](./04_serialization.md), worker threads |
| Large replies materialized as JS objects (`hgetall` on big hashes) | Fetch fewer fields, page |
| Per-request connections | One shared client |
| Giant `Promise.all` over 100k items | Batch in chunks of hundreds |
| Per-call tracing and logging of full values | Log sizes and key names only |

## Server settings worth knowing (and not cargo-culting)

Defaults are good. Change these only with measurements, and understand the trade-off:

| Setting | Default | Effect and caution |
|---------|---------|--------------------|
| `hz` | 10 | Background task frequency (expiry, timeouts). Higher means more CPU and faster expiry. `dynamic-hz yes` adapts it |
| `io-threads` | 1 | Extra threads for socket **I/O** (commands still execute on one thread). Can help with very high connection counts or large payloads. It is off by default, and recent Redis versions improved threaded I/O, so check your version's guidance and **benchmark** |
| `lazyfree-lazy-*` | version-dependent | Free memory in the background on eviction, expiry, deletion |
| `slowlog-log-slower-than` | 10000 µs | Lower it temporarily to catch more |
| `latency-monitor-threshold` | 0 (off) | Set (for example 100 ms) to record latency events |
| `maxmemory-clients` | 0 | Caps the total memory used by client buffers (Redis 7+) |
| `client-output-buffer-limit` | per class | Protects against slow consumers |
| `tcp-keepalive` | 300 | Detect dead peers |
| `appendfsync` | `everysec` | `always` is much slower. Consider only if you truly need it |
| `activerehashing` | `yes` | Incremental rehashing of the main hash table. Rarely touched |
| `proto-max-bulk-len` | 512 MB | Max single value. Lower it to refuse absurd writes |

Platform settings (THP off, `vm.overcommit_memory=1`, no swap) matter more than most of these ([Latency and Benchmarking](./01_latency-and-benchmarking.md#system-settings-that-matter)).

## Guardrails: stop dangerous commands from shipping

**Enforcement beats good intentions.** Layer these:

| Layer | How |
|-------|-----|
| **ACLs** | Application users get `-@dangerous` (blocks `KEYS`, `FLUSHALL`, `DEBUG`, `CONFIG`, …) ([17_security](../17_security/README.md)) |
| **Lint** | Ban `.keys(` and `.flushall(` in code review or ESLint rules |
| **Dev wrapper** | Throw or warn in development and tests |
| **Code review checklist** | "What is N? Is it bounded?" |

A simple development guard:

```ts
export function guard(redis: Redis, opts: { forbid?: string[]; warn?: string[] } = {}) {
  const forbid = new Set((opts.forbid ?? ["keys", "flushall", "flushdb", "monitor", "debug"]).map((s) => s.toLowerCase()));
  const warn = new Set((opts.warn ?? ["smembers", "hgetall", "lrange", "sunion", "sinter"]).map((s) => s.toLowerCase()));

  return new Proxy(redis, {
    get(target, prop, receiver) {
      const value = Reflect.get(target, prop, receiver);
      if (typeof prop !== "string" || typeof value !== "function") return value;

      const name = prop.toLowerCase();
      if (forbid.has(name)) return () => { throw new Error(`redis.${name}() is forbidden in this codebase`); };
      if (warn.has(name)) return (...args: unknown[]) => {
        console.warn(`redis.${name}() can be O(N). Make sure the key is bounded`);
        return value.apply(target, args);
      };
      return value.bind(target);
    },
  });
}

export const redis = process.env.NODE_ENV === "production" ? rawRedis : guard(rawRedis);
```

This doesn't cover `multi()` or `pipeline()` objects. It is a **safety net for development**, and **ACLs are the real enforcement in production**.

## Proving an optimization

1. Capture a **baseline** (percentiles, `commandstats`, Redis CPU) under a realistic load
2. Make **one** change
3. Re-run the **same** load
4. Compare. Keep the change only if the numbers justify the complexity

```ts
const before = await load({ concurrency: 50, warmupMs: 2_000, durationMs: 10_000, op: nPlusOne });
const after  = await load({ concurrency: 50, warmupMs: 2_000, durationMs: 10_000, op: pipelined });
console.table({ before, after });
```

## Testing

- **Bounded growth:** after N pushes, `LLEN` equals the cap (tests for `LTRIM` and `MAXLEN` logic)
- **No N+1:** count commands issued per request with a counting wrapper or Redis `INFO stats` deltas, and assert a ceiling
- **Page size limits:** an endpoint can't request unbounded pages
- **Big-key regression:** seed realistic data and run `bigKeys()`, failing if a key exceeds a byte budget
- **ACL tests:** the app user can't run `KEYS` or `FLUSHALL`

```ts
it("a request issues a bounded number of Redis commands", async () => {
  const before = Number(parseInfo(await redis.info("stats")).total_commands_processed);
  await request(app).get("/api/feed?limit=50");
  const after = Number(parseInfo(await redis.info("stats")).total_commands_processed);
  expect(after - before).toBeLessThan(10);                      // would be 50+ with an N+1 pattern
});
```

## Pitfalls

| Pitfall | Fix |
|---------|-----|
| `KEYS`, `SMEMBERS`, `HGETALL` and `LRANGE 0 -1` on growing data | Bounded reads, `SCAN` family |
| `DEL` on big keys | `UNLINK` |
| N+1 loops | Pipelines, `MGET`, auto pipelining |
| One giant `MGET` or pipeline | Chunk to hundreds or thousands |
| Unbounded lists, sets, streams | Caps, TTLs, `MAXLEN` |
| One hot or huge key in a Cluster | Split and bucket |
| Server tuning before access-pattern fixes | Fix usage first |
| `MONITOR` in production | Short sampled sessions only |
| Long Lua scripts | Short, bounded scripts |
| Guardrails only by convention | ACLs, lint, dev wrappers |
| Optimizing without numbers | Baseline, change one thing, compare |

## Key takeaways

- Ask of every command: **"what is N, and is it bounded?"**
- Find offenders with `SLOWLOG`, `INFO commandstats` and big-key scans, and **fix usage before tuning the server**
- **Cut round trips** (pipelines, `MGET`, auto pipelining), **fetch less**, and **chunk big batches**
- **Cap, split or move** big keys, and delete with `UNLINK`
- Enforce safety with **ACLs and linting**, and prove every optimization with before and after numbers

**Next:** [Memory Optimization](./03_memory-optimization.md)