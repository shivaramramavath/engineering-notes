# Memory Optimization

Redis keeps the entire dataset in RAM, so memory is the resource you run out of first and pay for most. Optimizing it means **measuring where the bytes go**, then choosing compact encodings, right-sized structures and bounded data.

```
Memory used = your data + per-key overhead + encoding overhead
            + fragmentation + buffers (clients, replication, AOF) + fork headroom
```

## Measure first

Never optimize blind. Start with the server-wide view, then drill down to keys.

### Server-wide: `INFO memory`

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

const m = parseInfo(await redis.info("memory"));
console.log({
  used: m.used_memory_human,                  // what Redis allocated
  rss: m.used_memory_rss_human,               // what the OS gave the process
  peak: m.used_memory_peak_human,
  fragmentation: m.mem_fragmentation_ratio,   // rss / used
  maxmemory: m.maxmemory_human,               // "0B" means unlimited
});
```

| Field | What it tells you |
|-------|-------------------|
| `used_memory` | Bytes Redis allocated for data and internal structures |
| `used_memory_rss` | Bytes the OS reports for the process |
| `mem_fragmentation_ratio` | `rss / used`. Around 1.0 to 1.5 is healthy |
| `used_memory_peak` | High-water mark since start. Size your instance for this |
| `used_memory_overhead` | Non-data memory (buffers, dict overhead) |
| `mem_clients_normal` | Memory used by client connections |

### Per-key: `MEMORY USAGE`

```ts
await redis.memory("USAGE", "user:1042");            // bytes, including overhead
await redis.object("ENCODING", "user:1042");         // "listpack", "hashtable", ...
await redis.call("MEMORY", "USAGE", "big:list", "SAMPLES", "0"); // exact for nested types
```

For nested types (lists, sets, hashes) `MEMORY USAGE` samples 5 elements by default and extrapolates. Pass `SAMPLES 0` for an exact figure at higher cost.

### Find the heavy keys

From the shell:

```bash
redis-cli --bigkeys     # biggest key per type (by element count)
redis-cli --memkeys     # biggest keys by bytes (Redis 6.0+)
redis-cli MEMORY DOCTOR # plain-English hints
```

From Node, sampling safely with `SCAN` and a pipeline:

```ts
async function topKeysByMemory(match = "*", limit = 10) {
  const top: { key: string; bytes: number }[] = [];

  for await (const keys of redis.scanStream({ match, count: 500 })) {
    if (keys.length === 0) continue;
    const pipe = redis.pipeline();
    for (const k of keys as string[]) pipe.memory("USAGE", k);
    const res = (await pipe.exec()) ?? [];

    res.forEach(([err, bytes], i) => {
      if (!err && typeof bytes === "number") top.push({ key: keys[i], bytes });
    });
    top.sort((a, b) => b.bytes - a.bytes);
    top.length = Math.min(top.length, limit);
  }
  return top;
}
```

Run this against a replica or off-peak. It is non-blocking, but it still adds load.

## Per-key overhead

Every key carries fixed bookkeeping (dictionary entry, object header, expiry entry if a TTL is set), typically **on the order of 50 to 100 bytes** before your data. With 50 million tiny keys, overhead can outweigh the data itself.

Consequences:

- Millions of tiny string keys are the most expensive way to store small values
- Prefer hashes for many small related values (see [Hashes](../04_data-structures/02_hashes.md))
- Keep key names short but readable. `u:1042:s` saves bytes over `user:1042:session` but costs clarity, so compress names only when the key count is huge

## Compact encodings

Redis stores small values in compact encodings and switches to larger ones past configurable limits.

| Type | Compact encoding | Full encoding | Controlled by |
|------|------------------|---------------|---------------|
| String | `int` (number), `embstr` (up to 44 bytes) | `raw` | Value size |
| Hash | `listpack` | `hashtable` | `hash-max-listpack-entries` (128), `hash-max-listpack-value` (64) |
| List | `listpack` nodes inside a `quicklist` | n/a | `list-max-listpack-size` (-2, which is 8 KB per node) |
| Set | `intset` (all integers), `listpack` (7.2+) | `hashtable` | `set-max-intset-entries` (512), `set-max-listpack-entries` (128) |
| Sorted set | `listpack` | `skiplist` | `zset-max-listpack-entries` (128), `zset-max-listpack-value` (64) |

Defaults shown are for recent Redis versions. Older versions used the name `ziplist` instead of `listpack`, and the old config names still work as aliases. Check your version.

```ts
await redis.object("ENCODING", "user:1042");   // "listpack"
await redis.object("ENCODING", "counter");     // "int"
await redis.object("ENCODING", "online:ids");  // "intset"
```

Once a value converts to the large encoding it usually does not convert back, even if you shrink it. If a hash grew past the limit once, it stays a `hashtable`.

### Tuning the thresholds

Raising the limits keeps more objects compact (saving memory) but makes each operation scan a longer array (costing CPU). Raise them modestly and measure.

```bash
# redis.conf, or at runtime with CONFIG SET
hash-max-listpack-entries 256
hash-max-listpack-value 128
```

```ts
await redis.config("SET", "hash-max-listpack-entries", "256");
```

Keep entries in the low hundreds. Past that, linear scans inside a listpack start to hurt latency.

## Use the right data type

| Need | Wasteful | Compact |
|------|----------|---------|
| Many small objects | One string key per object | One hash per object |
| Per-user boolean flags | One key per flag | Bitmap (`SETBIT`), 1 bit per user |
| Unique visitor count | Set of all IDs | HyperLogLog, about 12 KB for any size, about 0.81% error |
| Set of integer IDs | Set of strings | Integer-only set (`intset`) |
| Counters per entity | One key per counter | `HINCRBY` fields in one hash |

```ts
// 10 million users, one flag each: about 1.2 MB as a bitmap
await redis.setbit("flag:beta", 1042, 1);
await redis.getbit("flag:beta", 1042);

// Unique visitors without storing IDs
await redis.pfadd("visitors:2026-10-03", "u1", "u2", "u1");
await redis.pfcount("visitors:2026-10-03");     // 2
```

See [Bitmaps and Bitfields](../04_data-structures/06_bitmaps-and-bitfields.md) and [HyperLogLog and Geo](../04_data-structures/07_hyperloglog-and-geo.md).

## Bucketing small keys into hashes

When you must store hundreds of millions of tiny key-value pairs, group them into small hashes so each stays in `listpack` encoding.

```ts
const BUCKET_SIZE = 100; // stay below hash-max-listpack-entries

function slot(id: number) {
  return { key: `b:${Math.floor(id / BUCKET_SIZE)}`, field: String(id % BUCKET_SIZE) };
}

async function setValue(id: number, value: string) {
  const { key, field } = slot(id);
  await redis.hset(key, field, value);
}

async function getValue(id: number) {
  const { key, field } = slot(id);
  return redis.hget(key, field);
}
```

Trade-offs:

- No per-entry TTL (only on Redis 7.4+ with `HEXPIRE`), so expiry is per bucket
- Eviction removes whole buckets, not single entries
- Values must stay under `hash-max-listpack-value` bytes
- Worth it only at very large key counts, so measure before adopting

## Bound everything

Unbounded growth is the most common cause of memory incidents.

| Technique | Example |
|-----------|---------|
| TTL on cache and session keys | `SET key value EX 3600` |
| Cap lists | `LPUSH` then `LTRIM log:recent 0 999` |
| Cap streams | `XADD events MAXLEN ~ 100000 * ...` |
| Cap sorted sets | `ZREMRANGEBYRANK board 0 -1001` keeps the top 1000 |
| Time-bucket hashes | `stats:2026-10` per month, then expire old buckets |

```ts
await redis.multi()
  .lpush("log:recent", entry)
  .ltrim("log:recent", 0, 999)
  .exec();

await redis.xadd("events", "MAXLEN", "~", 100000, "*", "type", "login");
```

The `~` makes trimming approximate, which is much cheaper because Redis trims whole internal nodes.

## `maxmemory` and eviction

Always set a limit in production, or one runaway key can take down the host.

```bash
maxmemory 2gb
maxmemory-policy allkeys-lru   # cache: evict any key
# maxmemory-policy noeviction  # primary store or queues: fail writes instead
```

Leave headroom: `BGSAVE` and AOF rewrites fork the process, and under heavy writes copy-on-write can temporarily need significant extra memory. Do not give Redis more than roughly 60 to 70 percent of host RAM when persistence is on. Policies are covered in [Memory and Eviction](../02_redis-fundamentals/05_memory-and-eviction.md).

## Big keys

A key holding millions of elements causes memory spikes, slow commands, uneven cluster slots and long replication stalls on delete.

- Find them with `--bigkeys` / `--memkeys`
- Split by bucket (`feed:42:2026-10`) or by hash of the ID
- Delete with `UNLINK`, never `DEL`, so freeing happens in a background thread (see [Delete and Unlink](../05_key-management/03_delete-and-unlink.md))
- Enable lazy freeing so eviction and expiry do not block:

```bash
lazyfree-lazy-eviction yes
lazyfree-lazy-expire yes
lazyfree-lazy-server-del yes
replica-lazy-flush yes
```

## Fragmentation

`mem_fragmentation_ratio` far above 1.5 means the OS holds memory Redis is not using (freed chunks that cannot be returned). A ratio below 1.0 means Redis is swapping, which is an emergency.

```bash
activedefrag yes            # background defrag (requires the bundled jemalloc)
```

```ts
const m = parseInfo(await redis.info("memory"));
if (Number(m.mem_fragmentation_ratio) > 1.5) {
  console.warn("High fragmentation, check for churn of varied-size values");
}
```

Causes: heavy churn of different-sized values, large deletions, and many expiries. A restart or a failover to a fresh replica also clears it.

## Reduce value size

Smaller values mean less memory, less network and faster replication.

- Store numbers as numbers: `"12345"` becomes an `int` encoding, while padded or formatted strings do not
- Drop fields you never read before caching an object
- Use short, consistent field names in hashes if you store millions of them
- Pick a compact format and compress large payloads, covered in [Serialization](./04_serialization.md)

## Quick checklist

```
[ ] maxmemory and an eviction policy are set
[ ] Every cache/session key has a TTL
[ ] No unbounded lists, streams, sorted sets or hashes
[ ] No big keys (checked with --memkeys)
[ ] Many small objects use hashes, not one key each
[ ] mem_fragmentation_ratio between about 1.0 and 1.5
[ ] Persistence headroom left for fork (copy-on-write)
```

## Pitfalls

| Pitfall | Fix |
|---------|-----|
| Optimizing without measuring | `INFO memory`, `MEMORY USAGE`, `--memkeys` first |
| Millions of tiny string keys | Hashes, or bucketing |
| No `maxmemory` | Always set it in production |
| Setting `maxmemory` equal to host RAM | Leave room for fork, buffers and the OS |
| `DEL` on a huge key | `UNLINK` |
| Raising listpack limits too high | Keep entries in the low hundreds and benchmark |
| Ignoring client and replication buffers | Watch `mem_clients_normal` and set output buffer limits |
| Assuming a shrunk hash returns to listpack | It usually does not. Rebuild the key |
| Storing full JSON when you read two fields | Use a hash or trim the payload |

## Key takeaways

- Measure with `INFO memory`, `MEMORY USAGE` and `--memkeys` before changing anything
- Per-key overhead dominates when values are tiny, so use hashes and compact encodings
- Choose structures that fit the job: bitmaps, HyperLogLog, intsets
- Bound every collection and set a TTL, then cap the instance with `maxmemory`
- Leave fork headroom, watch fragmentation and use `UNLINK` for big deletes

**Previous:** [Command Optimization](./02_command-optimization.md) | **Next:** [Serialization](./04_serialization.md)