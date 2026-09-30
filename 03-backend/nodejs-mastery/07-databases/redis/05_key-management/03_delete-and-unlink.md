# Delete and Unlink

Removing data is as important as writing it. Done badly, deletion blocks Redis, breaks caches, or removes the wrong thing.

## `DEL` vs `UNLINK`

| | `DEL` | `UNLINK` |
|---|-------|----------|
| Removes the key from the keyspace | Immediately | Immediately |
| Frees the memory | **Synchronously** on the main thread | **In a background thread** (for large values) |
| Cost for a huge value | O(N), **blocks all clients** | O(1) on the main thread |
| Return value | Number of keys removed | Number of keys removed |
| Available since | Always | Redis 4.0 |

```ts
await redis.del("cache:a");
await redis.unlink("cache:a", "cache:b", "cache:c");      // preferred
```

For small values they behave almost the same, since Redis frees tiny objects immediately even with `UNLINK`. For anything that might be big (hashes, sets, lists, sorted sets, streams, large strings), **use `UNLINK` by default**.

## What "big" costs

Freeing a set with 10 million members with `DEL` can stall Redis for a noticeable time. Everything else waits: reads, writes, replication, health checks. Clients time out, and failover can trigger.

## Deleting one key

```ts
const removed = await redis.unlink("shop:cache:product:88:v3");
// 1 if it existed, 0 otherwise
```

The return value tells you whether the key was there, which is useful for "first one wins" logic.

## Deleting many keys

### Known list

```ts
await redis.unlink(...keys);                      // one command, many keys
```

Send in chunks if the list is large (thousands):

```ts
function chunk<T>(arr: T[], size: number): T[][] {
  const out: T[][] = [];
  for (let i = 0; i < arr.length; i += size) out.push(arr.slice(i, i + size));
  return out;
}

for (const part of chunk(keys, 500)) {
  await redis.unlink(...part);
}
```

**Cluster:** multi-key commands need all keys in one slot, or you get `CROSSSLOT`. Either group by slot, use hash tags, or delete keys one at a time:

```ts
await Promise.all(part.map((k) => cluster.unlink(k)));
```

### By pattern

Never `redis-cli KEYS ... | xargs DEL`. Scan and unlink in batches:

```ts
async function deleteByPattern(pattern: string) {
  let removed = 0;
  for await (const keys of redis.scanStream({ match: pattern, count: 500 })) {
    if (keys.length) removed += await redis.unlink(...keys);
  }
  return removed;
}

await deleteByPattern("shop:cache:product:*");
```

Remember the [`keyPrefix` pitfall](./02_scan-and-iteration.md#pitfall-keyprefix) when the client has a prefix.

## Deleting parts of a key

| Structure | Command | Notes |
|-----------|---------|-------|
| Hash | `HDEL key field ...` | |
| Set | `SREM key member ...` | |
| List | `LREM key count value`, `LTRIM key start stop` | `LTRIM` to keep only a range |
| Sorted set | `ZREM`, `ZREMRANGEBYRANK`, `ZREMRANGEBYSCORE` | Trim by rank or score |
| Stream | `XDEL`, `XTRIM` | `XTRIM` for retention |
| String | `SETRANGE` / rewrite | |
| Any | `EXPIRE key 0` | Deletes immediately (but frees synchronously like `DEL`) |

Emptying an aggregate **removes the key automatically**.

## Draining a huge collection gradually

Sometimes you want to delete a giant collection without a single large operation (for example, on an old Redis without lazy freeing, or to throttle):

```ts
async function drainSet(key: string, batch = 1000) {
  for await (const members of redis.sscanStream(key, { count: batch })) {
    if (members.length) await redis.srem(key, ...members);
  }
}

async function drainHash(key: string, batch = 1000) {
  for await (const flat of redis.hscanStream(key, { count: batch })) {
    const fields: string[] = [];
    for (let i = 0; i < flat.length; i += 2) fields.push(flat[i]);
    if (fields.length) await redis.hdel(key, ...fields);
  }
}

async function drainZset(key: string, batch = 1000) {
  while ((await redis.zremrangebyrank(key, 0, batch - 1)) > 0) {
    // keeps trimming from the bottom
  }
}

async function drainList(key: string, batch = 1000) {
  while ((await redis.llen(key)) > 0) {
    await redis.ltrim(key, batch, -1);
  }
}
```

In modern Redis, a plain `UNLINK` on the whole key is usually simpler and just as safe.

## Lazy freeing configuration

Redis can free memory in the background for more situations:

```
lazyfree-lazy-user-del     yes   # make DEL behave like UNLINK
lazyfree-lazy-eviction     yes   # evictions free memory in the background
lazyfree-lazy-expire       yes   # expired keys are freed in the background
lazyfree-lazy-server-del   yes   # implicit deletes (RENAME, SET overwrite, ...)
lazyfree-lazy-user-flush   yes   # FLUSHDB/FLUSHALL default to ASYNC
replica-lazy-flush         yes   # replicas flush asynchronously on full sync
```

Trade-off: memory is released slightly later, and `INFO memory` reflects that. Defaults differ across Redis versions, so check yours:

```ts
await redis.config("GET", "lazyfree-*");
```

## `RENAME` and implicit deletes

`SET` on an existing big key, and `RENAME` onto an existing key, **free the old value implicitly**, synchronously unless `lazyfree-lazy-server-del` is on. If the old value is huge, unlink it first:

```ts
await redis.unlink("report:latest");
await redis.rename("report:tmp", "report:latest");
```

## Flushing

```ts
await redis.flushdb("ASYNC");     // remove all keys in the current DB in the background
await redis.flushall("ASYNC");    // all databases
```

`SYNC` blocks until finished. Both are **destructive**. Guard them in code:

```ts
if (process.env.NODE_ENV === "production") throw new Error("flush is disabled");
```

Better still: block them with ACLs for application users (`-@dangerous`, covered in `17_security`).

## Deletion patterns

### 1. Prefer TTL over manual delete

If data can expire, let it:

```ts
await redis.set(key, value, "EX", 600);     // no delete code needed
```

### 2. Invalidate on write

```ts
await db.products.update(id, changes);
await redis.unlink(keys.productCache(id));
```

Details, races and alternatives are in `07_caching/03_cache-invalidation.md`.

### 3. Tracked-key invalidation

Remember which keys belong together, then remove them as a group:

```ts
// when caching
await redis.multi()
  .set(cacheKey, json, "EX", 600)
  .sadd(`shop:idx:user:${id}:cachekeys`, cacheKey)
  .expire(`shop:idx:user:${id}:cachekeys`, 700)
  .exec();

// when the user changes
const tracked = await redis.smembers(`shop:idx:user:${id}:cachekeys`);   // small, bounded set
if (tracked.length) await redis.unlink(...tracked, `shop:idx:user:${id}:cachekeys`);
```

In Cluster, keep the tracked keys and the index under one hash tag or delete keys individually.

### 4. Version bump (no delete at all)

```ts
await redis.incr("shop:ver:products");      // all old entries become unreachable, then expire
```

See [Key Design](./01_key-design.md#namespace-versioning-bulk-invalidation).

### 5. Build then swap

Rebuild a collection without a window where it is empty or half-filled:

```ts
const tmp = `lb:global:tmp:${Date.now()}`;
await redis.zadd(tmp, ...flatScoresAndMembers);
await redis.expire(tmp, 3600);              // safety net
await redis.rename(tmp, "lb:global");       // atomic replace
```

### 6. Delete only if I still own it (compare-and-delete)

`GET` then `DEL` is a race. Use a Lua script so the check and delete are atomic. This is the classic **lock release**:

```ts
const releaseLock = `
if redis.call("GET", KEYS[1]) == ARGV[1] then
  return redis.call("DEL", KEYS[1])
else
  return 0
end`;

const released = await redis.eval(releaseLock, 1, "shop:lock:order:5501", token);
```

More in `06_advanced-commands/03_lua-scripts.md` and `11_distributed-locks`.

### 7. Read and delete atomically

```ts
const otp = await redis.getdel(`shop:otp:${phone}`);   // Redis 6.2+, one-time read
```

## Safety checklist for bulk deletes

- [ ] Ran the same pattern with `SCAN` and only **counted** first
- [ ] Sampled a few matching keys to confirm the pattern
- [ ] Used `UNLINK`, not `DEL`
- [ ] Batches of about 100 to 1000, with an optional pause
- [ ] Off-peak, or against a maintenance window for very large jobs
- [ ] Verified `keyPrefix` behavior
- [ ] Cluster handled (per-node scans, per-key deletes or hash tags)
- [ ] Logged how many keys were removed

Dry-run helper:

```ts
async function countMatches(pattern: string) {
  let n = 0;
  for await (const keys of redis.scanStream({ match: pattern, count: 500 })) n += keys.length;
  return n;
}
```

## Pitfalls

| Pitfall | Consequence | Fix |
|---------|-------------|-----|
| `DEL` on a huge key | Blocks Redis | `UNLINK` |
| `KEYS pattern` then delete | Blocks Redis | `SCAN` + `UNLINK` |
| Deleting with a `keyPrefix` client using scanned names | Wrong keys targeted | Unprefixed client or builder |
| `GET` then `DEL` for ownership checks | Race conditions | Lua compare-and-delete |
| Multi-key `UNLINK` in Cluster across slots | `CROSSSLOT` error | Group by slot or delete individually |
| `FLUSHALL` on a shared instance | Everyone's data gone | ACLs, guards, separate instances |
| Manual deletes where a TTL would do | Extra code and bugs | `EXPIRE` at write time |
| Forgetting related indexes | Dangling references | Delete data and index together in `multi()` |

## Key takeaways

- `UNLINK` frees memory in the background, so use it by default
- Delete by pattern with `SCAN` in batches, never `KEYS`
- Prefer TTLs, version bumps and swaps over mass deletion
- Use Lua for conditional deletes, and guard flushes

**Next module:** [06_advanced-commands](../06_advanced-commands/README.md)
