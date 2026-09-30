# Scan and Iteration

To walk many keys (or many members of a big collection) without blocking Redis, use the `SCAN` family. It returns a **small batch per call** and a **cursor** to continue.

## Why not `KEYS`?

`KEYS pattern` is **O(N)** over the whole keyspace in **one blocking call**. On a large database it can freeze Redis for seconds while every client waits.

| | `KEYS` | `SCAN` |
|---|--------|--------|
| Blocking | Yes, for the whole scan | Only for each small batch |
| Result | Complete at one instant | Incremental, built over many calls |
| Production-safe | **No** | Yes |
| Server state | None | None (the cursor carries the state) |

## The SCAN family

| Command | Iterates | ioredis stream helper |
|---------|----------|----------------------|
| `SCAN` | Keys in the current database | `scanStream` |
| `HSCAN` | Fields (and values) of a hash | `hscanStream` |
| `SSCAN` | Members of a set | `sscanStream` |
| `ZSCAN` | Members (and scores) of a sorted set | `zscanStream` |

## How the cursor works

```
SCAN 0            ──►  [ "17", [k1, k2, k3] ]
SCAN 17           ──►  [ "9",  [k4, k5]     ]
SCAN 9            ──►  [ "0",  [k6]         ]   cursor "0" = finished
```

- Start with cursor `"0"`
- Use the cursor returned by the previous call
- **A returned cursor of `"0"` means the iteration is complete** (an empty batch does not)

## Manual loop

```ts
async function scanKeys(pattern: string, count = 200): Promise<string[]> {
  const found: string[] = [];
  let cursor = "0";

  do {
    const [next, keys] = await redis.scan(cursor, "MATCH", pattern, "COUNT", count);
    cursor = next;
    found.push(...keys);
  } while (cursor !== "0");

  return found;
}
```

Batches can be **empty** even when more keys exist, because `MATCH` filters after Redis has examined the batch. Always loop until the cursor is `"0"`.

## `scanStream` (recommended in ioredis)

```ts
const stream = redis.scanStream({ match: "shop:cache:*", count: 200 });

stream.on("data", (keys: string[]) => {
  // one batch of keys (possibly empty)
});
stream.on("end", () => console.log("done"));
stream.on("error", (err) => console.error(err));
```

Async iteration is simpler:

```ts
for await (const keys of redis.scanStream({ match: "shop:cache:*", count: 200 })) {
  for (const key of keys) {
    // ...
  }
}
```

Options: `match`, `count`, and `type` (Redis 6.0+):

```ts
redis.scanStream({ match: "shop:*", count: 500, type: "hash" });   // only hashes
```

### Backpressure

If processing is slower than scanning, pause the stream:

```ts
const stream = redis.scanStream({ match: "shop:cache:*", count: 500 });

stream.on("data", async (keys: string[]) => {
  stream.pause();
  try {
    await processBatch(keys);
  } finally {
    stream.resume();
  }
});
```

With `for await`, backpressure is automatic.

## Scanning collections

```ts
// hash: batches are flat [field, value, field, value, ...]
for await (const batch of redis.hscanStream("stats:post:42", { count: 100 })) {
  for (let i = 0; i < batch.length; i += 2) {
    const field = batch[i];
    const value = batch[i + 1];
  }
}

// set: batches are arrays of members
for await (const members of redis.sscanStream("visitors:2026-09-30", { count: 500 })) {
  for (const m of members) { /* ... */ }
}

// sorted set: batches are flat [member, score, member, score, ...]
for await (const batch of redis.zscanStream("lb:global", { count: 500 })) {
  for (let i = 0; i < batch.length; i += 2) {
    const member = batch[i];
    const score = Number(batch[i + 1]);
  }
}
```

`SSCAN` and friends are the safe replacement for `SMEMBERS`, `HGETALL` and `ZRANGE 0 -1` on big collections. `MATCH` also works on them (matching field or member names).

## `COUNT` and `MATCH`

| Option | Meaning |
|--------|---------|
| `COUNT n` | A **hint** for how much work per call (default 10). Not a limit on results |
| `MATCH pattern` | Glob filter applied **after** the batch is read |
| `TYPE t` | Type filter for `SCAN` (applied after reading) |

Guidance:

- Use `COUNT` 100 to 1000 for keyspace scans. Larger values mean fewer round trips but a longer block per call
- `MATCH` does **not** make the scan cheaper: a rare pattern still walks the entire keyspace
- Glob syntax: `*` any characters, `?` one character, `[abc]` set, `[^a]` negation, `\` escape

```ts
redis.scanStream({ match: "shop:cache:product:[0-9]*" });
```

## Guarantees and gotchas

| Behavior | Detail |
|----------|--------|
| Full coverage | Every key that existed for the **entire** iteration is returned |
| Duplicates | A key **may be returned more than once** |
| Concurrent changes | Keys added or removed during the scan **may or may not** appear |
| Not a snapshot | You never get a consistent point-in-time view |
| Cost | O(1) per call, O(N) for the full iteration |

Consequences for your code:

```ts
const seen = new Set<string>();
for await (const keys of redis.scanStream({ match: "shop:cache:*", count: 500 })) {
  for (const key of keys) {
    if (seen.has(key)) continue;        // handle duplicates (or make the work idempotent)
    seen.add(key);
    await handle(key);
  }
}
```

Better: make the per-key action **idempotent** (`UNLINK`, `EXPIRE`) so duplicates are harmless, and skip the `Set` for huge keyspaces.

## Pitfall: `keyPrefix`

`keyPrefix` is applied to commands' key arguments but **not** to `SCAN` patterns or to the keys it returns:

```ts
const redis = new Redis({ keyPrefix: "shop:" });
await redis.set("user:1", "Ada");                       // stored as "shop:user:1"

for await (const keys of redis.scanStream({ match: "shop:user:*" })) {
  // keys = ["shop:user:1"]          <-- full names
  await redis.del(keys);            // BUG: deletes "shop:shop:user:1"
}
```

Fixes:

- Use a **separate client without `keyPrefix`** for scanning and cleanup, or
- Strip the prefix before reusing the key, or
- Avoid `keyPrefix` and use a key builder (recommended)

## Scanning a Cluster

`SCAN` works **per node**. In a Cluster, scan every primary:

```ts
import { Cluster } from "ioredis";

async function scanCluster(cluster: Cluster, pattern: string, onKeys: (k: string[]) => Promise<void>) {
  const masters = cluster.nodes("master");
  await Promise.all(
    masters.map(async (node) => {
      for await (const keys of node.scanStream({ match: pattern, count: 500 })) {
        await onKeys(keys);
      }
    })
  );
}
```

## Real-world recipes

### 1. Delete keys by pattern (safely)

```ts
async function deleteByPattern(pattern: string, count = 500) {
  let removed = 0;
  for await (const keys of redis.scanStream({ match: pattern, count })) {
    if (keys.length === 0) continue;
    removed += await redis.unlink(...keys);       // non-blocking delete
  }
  return removed;
}
```

More on this in [Delete and Unlink](./03_delete-and-unlink.md).

### 2. Audit keys without a TTL

```ts
async function findKeysWithoutTtl(pattern: string, limit = 1000) {
  const offenders: string[] = [];

  for await (const keys of redis.scanStream({ match: pattern, count: 500 })) {
    if (keys.length === 0) continue;

    const pipeline = redis.pipeline();
    keys.forEach((k: string) => pipeline.ttl(k));
    const results = await pipeline.exec();

    results?.forEach(([err, ttl], i) => {
      if (!err && ttl === -1) offenders.push(keys[i]);   // -1 = exists, no expiry
    });

    if (offenders.length >= limit) break;
  }
  return offenders;
}
```

### 3. Find the biggest keys

```ts
async function biggestKeys(pattern = "*", top = 20) {
  const sizes: { key: string; bytes: number }[] = [];

  for await (const keys of redis.scanStream({ match: pattern, count: 500 })) {
    if (keys.length === 0) continue;

    const pipeline = redis.pipeline();
    keys.forEach((k: string) => pipeline.memory("USAGE", k, "SAMPLES", 5));
    const results = await pipeline.exec();

    results?.forEach(([err, bytes], i) => {
      if (!err && typeof bytes === "number") sizes.push({ key: keys[i], bytes });
    });
  }
  return sizes.sort((a, b) => b.bytes - a.bytes).slice(0, top);
}
```

`SAMPLES` limits the work for large aggregates (an estimate instead of an exact count).

### 4. Migrate or rename a key family

```ts
for await (const keys of redis.scanStream({ match: "old:cache:*", count: 500 })) {
  for (const oldKey of keys) {
    await redis.rename(oldKey, oldKey.replace("old:", "shop:"));   // atomic per key
  }
}
```

In Cluster, `RENAME` requires both names in the same slot, so use `DUMP`/`RESTORE` or copy instead.

## Operating scans safely in production

- Run heavy scans **off-peak**, or against a **replica** for read-only audits
- Use moderate `COUNT` and, for very large keyspaces, **throttle** between batches:

```ts
await new Promise((r) => setTimeout(r, 5));
```

- Estimate progress with `DBSIZE` (`processed / dbsize`)
- Log counts, not keys, in production
- Prefer **maintained indexes** for lookups you do often (see [Key Design](./01_key-design.md)) instead of scanning repeatedly

## CLI equivalents

```bash
redis-cli --scan --pattern 'shop:cache:*'
redis-cli --scan --pattern 'shop:cache:*' --count 500
redis-cli --scan --pattern 'shop:tmp:*' | xargs -L 100 redis-cli unlink
```

```
SSCAN myset 0 MATCH a* COUNT 100
```

## Pitfalls

| Pitfall | Fix |
|---------|-----|
| Using `KEYS` | `SCAN` / `scanStream` |
| Stopping at the first empty batch | Loop until the cursor is `"0"` |
| Treating `COUNT` as a result limit | It is a work hint |
| Assuming no duplicates | Make actions idempotent or dedupe |
| `keyPrefix` double-prefixing on delete | Unprefixed client or a key builder |
| Scanning one node of a Cluster | Scan every primary |
| Scanning frequently for lookups | Maintain an index |
| Huge `COUNT` values | Keep it at 100 to 1000 |

## Key takeaways

- `SCAN` iterates incrementally with a cursor. Finished means cursor `"0"`
- `MATCH` filters after reading, so scanning is still O(N) overall
- Expect duplicates and a non-snapshot view, and keep actions idempotent
- Use `scanStream`, mind `keyPrefix`, and scan each Cluster primary

**Next:** [Delete and Unlink](./03_delete-and-unlink.md)
