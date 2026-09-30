# Cache Invalidation

"There are only two hard things in computer science: cache invalidation and naming things." The difficulty is real: the cache and the database are **two systems**, updated at different times, and you can't update both atomically.

The goal is not perfection. It is **bounded staleness**: you know the worst case, and it is acceptable.

## The toolbox

| Technique | How | Staleness | Effort |
|-----------|-----|-----------|--------|
| **TTL only** | Let entries expire | Up to the TTL | None |
| **Delete on write** | `UNLINK` the key after the DB write | Tiny window | Low |
| **Tags** | Track keys per group, delete the group | Tiny window | Medium |
| **Versioned keys / namespaces** | Bump a version, old keys become unreachable | None (instant) | Low |
| **Event-driven** | Publish change events, consumers invalidate | Small | Medium |
| **Version-checked writes** | Only accept newer data into the cache | Very small | Higher |

Combine them: **delete on write + TTL as a backstop** covers most systems.

## 1. TTL only

```ts
await redis.set(key, json, "EX", 60);
```

Simple and robust: even if every other mechanism fails, data is at most 60 seconds old. Choose the TTL from the business tolerance ("how stale is acceptable?"), not from convenience.

## 2. Delete on write

```ts
await db.products.update(id, changes);          // commit first
await redis.unlink(keys.productCache(id));      // then invalidate
```

Rules:

- **Commit, then invalidate.** If you invalidate first, a concurrent reader can repopulate the cache with the old data before your commit lands
- Delete **every key derived from the changed data** (item, list pages, aggregates)
- Use `UNLINK` (see [Delete and Unlink](../05_key-management/03_delete-and-unlink.md))
- If the delete fails, log it loudly. The TTL is now your only protection

### Derived data

One change often affects several cached views:

```ts
async function updateProduct(id: number, changes: Partial<Product>) {
  const p = await db.products.update(id, changes);
  await redis.unlink(
    keys.productCache(id),
    keys.categoryListCache(p.categoryId),
    keys.homepageCache()
  );
}
```

This gets brittle as views multiply. Tags solve it.

## 3. Tag-based invalidation

Record which cache keys belong to which **tag**, then invalidate by tag.

```ts
export class TaggedCache {
  constructor(private redis: Redis) {}

  async set(key: string, value: unknown, ttl: number, tags: string[]) {
    const m = this.redis.multi().set(key, JSON.stringify(value), "EX", ttl);
    for (const tag of tags) {
      m.sadd(`shop:tag:${tag}`, key);
      m.expire(`shop:tag:${tag}`, ttl * 2);          // tag sets shouldn't live forever
    }
    await m.exec();
  }

  async invalidateTag(tag: string) {
    const tagKey = `shop:tag:${tag}`;
    const keys = await this.redis.smembers(tagKey);  // bounded, see notes
    if (keys.length) await this.redis.unlink(...keys, tagKey);
  }
}
```

```ts
const cache = new TaggedCache(redis);

await cache.set(`shop:cache:product:88`, product, 600, ["product:88", "category:5", "products"]);
await cache.set(`shop:cache:list:cat5:p1`, list, 120, ["category:5", "products"]);

// a product changed:
await cache.invalidateTag("product:88");
// the whole category changed:
await cache.invalidateTag("category:5");
```

Notes:

- Tag sets are **bounded by design**: don't tag with a giant group like `all`, or `SMEMBERS` becomes a blocking call. Iterate with `SSCAN` for large tags
- The tag set's TTL should exceed the TTLs of its members, so it doesn't disappear while entries live. Members expiring on their own leave harmless dangling entries in the set, which vanish with the set
- In Cluster, use hash tags so keys and their tag set share a slot, or unlink keys individually

## 4. Versioned keys and namespaces

Instead of deleting, make old entries **unreachable**:

```ts
async function ver(ns: string) {
  return (await redis.get(`shop:ver:${ns}`)) ?? "1";
}

async function productListKey(catId: number, page: number) {
  return `shop:cache:products:${await ver("products")}:cat${catId}:p${page}`;
}

// invalidate everything under "products" in O(1)
await redis.incr("shop:ver:products");
```

- Instant invalidation, no scan, no delete
- Old entries **age out through their TTL**, so TTLs are mandatory
- Costs an extra Redis read per lookup: cache the version in-process for a second or two, or fetch it in the same pipeline as the data

Use it for **wide invalidations** ("everything about products", "all pages of this feed"). Use plain deletes for single keys.

## 5. Event-driven invalidation

When several services or instances cache the same data, broadcast the change.

### Pub/Sub (fast, at-most-once)

Good for clearing **in-process (L1) caches** on every app instance:

```ts
const sub = redis.duplicate();
await sub.subscribe("shop:invalidate");

sub.on("message", (_channel, key) => {
  l1.delete(key);                                 // local in-process cache
});

async function invalidate(key: string) {
  await redis.multi()
    .unlink(key)
    .publish("shop:invalidate", key)
    .exec();
}
```

Pub/Sub is **fire-and-forget**: an instance that is disconnected misses the message. So keep L1 TTLs short (seconds) as the backstop.

### Streams (durable)

For consumers that must not miss changes (search indexes, downstream caches), publish change events to a stream and let consumer groups process them (`10_streams`).

### From the database

Change-data-capture (CDC) or database triggers can emit invalidations, so caches follow the source of truth even when changes come from outside your app.

## The race you should know about

Even with commit-then-delete, this interleaving can leave **stale data in the cache**:

```
Reader                         Writer
──────                         ──────
GET key  → miss
SELECT row (old value)
                               UPDATE row (new value)
                               DEL key
SET key = old value            ← stale entry now cached
```

It is rare, since it requires the reader's DB read to complete before the write and its cache write to land after the delete, but it happens at scale. The stale entry lives until its TTL.

Mitigations, from cheapest to strongest:

| Mitigation | Idea | Cost |
|------------|------|------|
| **Short TTL** | Bound the damage | Free |
| **Delayed second delete** | Delete again a moment after the write | Not durable (timer lost on crash) |
| **Lock during rebuild** | Reader and writer serialize on a key | More latency and complexity |
| **Version-checked cache writes** | Refuse to overwrite newer data | Needs a version number from the database |

### Delayed double delete

```ts
await db.products.update(id, changes);
await redis.unlink(key);
setTimeout(() => redis.unlink(key).catch(() => {}), 500);     // second chance
```

Cheap and helpful, but an in-process timer dies with the process. Treat it as an optimization, not a guarantee.

### Version-checked writes

If your rows carry a monotonically increasing `version` (or `updated_at`), store it with the cached value and let the cache reject older writes:

```lua
-- KEYS[1] = hash key
-- ARGV[1] = version, ARGV[2] = JSON payload, ARGV[3] = ttl seconds
local cur = redis.call("HGET", KEYS[1], "v")
if cur and tonumber(cur) >= tonumber(ARGV[1]) then
  return 0                                   -- cache already has this version or newer
end
redis.call("HSET", KEYS[1], "v", ARGV[1], "d", ARGV[2])
redis.call("EXPIRE", KEYS[1], tonumber(ARGV[3]))
return 1
```

```ts
redis.defineCommand("cacheSetIfNewer", { numberOfKeys: 1, lua: SET_IF_NEWER });
await (redis as any).cacheSetIfNewer(key, row.version, JSON.stringify(row), 600);
```

Caveat: deleting the key also deletes its version, so the guard disappears. For strict protection, **write the new version instead of deleting**, or keep a short-lived tombstone.

For most applications: **delete on write + short TTL** is the right balance. Reach for versions only where staleness is expensive.

## Choosing an approach

| Situation | Approach |
|-----------|----------|
| Staleness of seconds or minutes is fine | TTL only |
| Single object edited by your app | Delete on write + TTL |
| Many cached views derived from one entity | Tags |
| Invalidate a whole family instantly | Namespace version |
| Multiple instances with in-process caches | Pub/Sub + short L1 TTL |
| Other services must react to changes | Streams or CDC |
| Staleness is costly | Version-checked writes, short TTL, or **don't cache it** |

## Testing invalidation

- Write a test that **updates, then reads**, and asserts the new value (it catches missing deletes)
- Add a test per derived key for each write path
- Log invalidations with the key or tag (counts, not payloads) for debugging
- Run a periodic **consistency sampler** that compares a random sample of cache entries with the database and reports mismatches

## Pitfalls

| Pitfall | Fix |
|---------|-----|
| Invalidating **before** the commit | Commit, then invalidate |
| Forgetting derived keys | Tags or a central invalidation function |
| No TTL as a backstop | Always set one |
| Huge tag sets with `SMEMBERS` | Smaller tags, or `SSCAN` |
| Relying on Pub/Sub for correctness | It's best-effort, so add short TTLs |
| `KEYS`/`SCAN` to find keys to invalidate | Tags or namespace versions |
| Assuming the race can't happen | Short TTLs, and double-delete or versions where it matters |

## Key takeaways

- Aim for **bounded staleness**, not perfect consistency
- **Commit, then delete**, with a TTL as the backstop
- Tags and namespace versions scale invalidation across many derived keys
- Pub/Sub clears in-process caches, but it's not a guarantee

**Next:** [Cache Problems](./04_cache-problems.md)
