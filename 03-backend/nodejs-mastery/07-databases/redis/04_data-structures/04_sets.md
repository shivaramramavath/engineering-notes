# Sets

A set is an **unordered collection of unique strings**. Membership checks are O(1), and Redis can compute unions, intersections and differences on the server.

## Command tour

```ts
await redis.sadd("tags:post:1", "redis", "nodejs", "cache");  // returns count of NEW members
await redis.srem("tags:post:1", "cache");                     // remove
await redis.sismember("tags:post:1", "redis");                // 1 or 0
await redis.smismember("tags:post:1", "redis", "go");         // [1, 0]  (Redis 6.2+)
await redis.scard("tags:post:1");                             // size
await redis.smembers("tags:post:1");                          // all members (O(N)!)
await redis.srandmember("tags:post:1", 2);                    // 2 random members, not removed
await redis.spop("tags:post:1");                              // remove and return a random member
await redis.smove("src", "dst", "member");                    // move between sets atomically
```

## Set algebra

```ts
await redis.sadd("friends:ada", "bob", "cy", "dee");
await redis.sadd("friends:bob", "ada", "cy", "eli");

await redis.sinter("friends:ada", "friends:bob");     // ["cy"]              mutual friends
await redis.sunion("friends:ada", "friends:bob");     // everyone in either
await redis.sdiff("friends:ada", "friends:bob");      // in ada's but not bob's

// store results in a new key (cacheable, can get a TTL)
await redis.sinterstore("tmp:mutual", "friends:ada", "friends:bob");
await redis.expire("tmp:mutual", 60);

await redis.sintercard(2, "friends:ada", "friends:bob"); // count only, Redis 7.0+
```

`SINTERCARD` gives the size of the intersection without building the result. It also accepts `LIMIT` to stop early.

## Patterns

### 1. Tags and categories

```ts
await redis.sadd("post:1:tags", "redis", "nodejs");
await redis.sadd("tag:redis:posts", 1, 2, 5);        // reverse index
const postsWithBoth = await redis.sinter("tag:redis:posts", "tag:nodejs:posts");
```

### 2. Unique visitors (exact)

```ts
await redis.sadd(`visitors:${today}`, userId);
const unique = await redis.scard(`visitors:${today}`);
```

Exact but memory grows with members. For millions of uniques, see [HyperLogLog](./07_hyperloglog-and-geo.md).

### 3. Membership and permission checks

```ts
const allowed = (await redis.sismember(`role:admin:users`, userId)) === 1;
```

### 4. Deduplication

```ts
const isNew = (await redis.sadd("processed:orders", orderId)) === 1;
if (isNew) await process(orderId);
```

`SADD` returns `1` only for a new member, which makes it an atomic "first time" test.

### 5. Random selection

```ts
const winner = await redis.spop("raffle:entries");        // draws and removes
const sample = await redis.srandmember("quiz:pool", 5);   // sample, no removal
```

### 6. Following / relationships

```ts
await redis.sadd(`following:${a}`, b);
await redis.sadd(`followers:${b}`, a);
```

Update both sides in a `multi()`.

## Presence with a caveat

Sets have **no per-member TTL**. If you store "online users" in a set, a user who disconnects uncleanly stays forever. For presence with expiry, use a **sorted set** with the last-seen timestamp as the score, and remove old entries with `ZREMRANGEBYSCORE` (see [Sorted Sets](./05_sorted-sets.md)).

## Large sets

`SMEMBERS` is O(N) and blocks the server on big sets. Iterate instead:

```ts
for await (const batch of redis.sscanStream("big:set", { count: 500 })) {
  for (const member of batch) { /* ... */ }
}
```

## Cluster note

Multi-key commands (`SINTER`, `SUNION`, `SDIFF`, `SMOVE`) require all keys in the **same hash slot** in Redis Cluster. Use hash tags: `{user:1}:friends`, `{user:1}:blocked`.

## Encoding and memory

- All-integer small sets use a compact **intset**
- Small mixed sets use **listpack** (Redis 7.2+)
- Larger sets use a **hashtable**, which is more memory-hungry

## Complexity

| Command | Cost |
|---------|------|
| `SADD`, `SREM`, `SISMEMBER`, `SCARD`, `SPOP` | O(1) per member |
| `SMEMBERS` | O(N) |
| `SINTER` | O(N·M) worst case (N = smallest set, M = number of sets) |
| `SUNION`, `SDIFF` | O(total members) |
| `SSCAN` | O(1) per call |

## Pitfalls

| Pitfall | Fix |
|---------|-----|
| `SMEMBERS` on large sets | `SSCAN` or `SCARD` |
| Expecting order | Sets are unordered. Use a sorted set |
| Set as presence tracker with no cleanup | Sorted set with timestamp scores |
| Heavy `SINTER` on huge sets in the request path | Precompute with `SINTERSTORE` and a TTL |
| Multi-key set ops in Cluster | Hash tags |
| Unbounded set growth | TTL on the key or periodic rotation |

## Key takeaways

- Sets give O(1) membership and built-in set algebra
- `SADD` return value doubles as an atomic "is this new?" check
- Avoid `SMEMBERS` on large sets and members have no TTL
- Use hash tags for multi-key operations in Cluster

**Next:** [Sorted Sets](./05_sorted-sets.md)
