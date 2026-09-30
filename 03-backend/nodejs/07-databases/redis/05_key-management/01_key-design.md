# Key Design

A key scheme is an API. It is hard to change once data exists, and every team member touches it. Design it once, write it down, and generate keys from one place.

This lesson builds on the basics in [Keys and Values](../02_redis-fundamentals/01_keys-and-values.md).

## Principles

1. **Predictable**: given an entity and an ID, anyone can derive the key
2. **Hierarchical**: general to specific, separated by `:`
3. **Scannable**: a prefix should select a meaningful group
4. **Bounded**: every key class has a size and lifetime policy
5. **Cluster-safe**: related keys that are used together share a hash tag
6. **Versionable**: you can change the shape of cached data without a migration

## Structure

```
<env?>:<app>:<domain>:<entity>:<id>[:<attribute>][:<bucket>]
```

| Segment | Purpose | Example |
|---------|---------|---------|
| env | Separate dev/staging/prod when sharing an instance (avoid this if you can) | `prod` |
| app | Separate applications on one instance | `shop` |
| domain / purpose | Kind of use, useful for eviction and cleanup | `cache`, `session`, `lock`, `rate`, `queue` |
| entity + id | The thing | `user:1042` |
| attribute | Sub-data | `cart`, `followers` |
| bucket | Time or shard bucket | `2026-09-30`, `03` |

Examples:

```
shop:user:1042                     hash, profile
shop:user:1042:cart                hash
shop:cache:product:88:v3           string, versioned cache entry
shop:session:9f8a7c                hash + TTL
shop:lock:order:5501               string + TTL
shop:rate:ip:203.0.113.7:2026-09-30T15:04     counter + TTL
shop:dau:2026-09-30                bitmap + TTL
shop:idx:user:email:ada@example.com   lookup index
```

## Key classes and policies

Give every class an explicit lifetime and growth policy. Put this table in your repo.

| Class | Structure | TTL / bound | Eviction OK? | Notes |
|-------|-----------|-------------|--------------|-------|
| `cache:*` | string / hash | Always (minutes to hours, with jitter) | Yes | Rebuildable from the source of truth |
| `session:*` | hash | Sliding, with an absolute cap | No | Losing it logs users out |
| `lock:*` | string | Always (seconds) | No | Must expire on crash |
| `rate:*` | string / zset | Window length | Yes | |
| `queue:*` | list / stream | None, but trimmed | No | Bounded by consumers |
| `idx:*` | set / zset | Same as the object it indexes | Depends | Rebuildable? Say so |
| `user:*` | hash | None | No | Primary data. Should it even live in Redis? |
| `tmp:*` | any | Always (short) | Yes | Scratch results |

If you run cache and critical data in one instance, use `volatile-lru` and TTL only the evictable classes, or better, **separate instances**.

## Generate keys from one place

```ts
// src/redis/keys.ts
const P = "shop";

const seg = (v: string | number) => {
  const s = String(v);
  if (/[\s:*?\[\]\\]/.test(s)) throw new Error(`invalid key segment: ${s}`);
  return s;
};

export const keys = {
  user:        (id: number | string) => `${P}:user:${seg(id)}`,
  userCart:    (id: number | string) => `${P}:user:${seg(id)}:cart`,
  session:     (sid: string)         => `${P}:session:${seg(sid)}`,
  productCache:(id: number | string, v = 3) => `${P}:cache:product:${seg(id)}:v${v}`,
  lock:        (name: string)        => `${P}:lock:${seg(name)}`,
  rate:        (ip: string, minute: string) => `${P}:rate:ip:${seg(ip)}:${seg(minute)}`,
  dau:         (day: string)         => `${P}:dau:${seg(day)}`,
  emailIndex:  (email: string)       => `${P}:idx:user:email:${email.toLowerCase()}`,

  // patterns for SCAN (never for KEYS)
  patterns: {
    productCache: () => `${P}:cache:product:*`,
    userAll: (id: number | string) => `${P}:user:${seg(id)}*`,
  },
};
```

Benefits:

- One place to audit and refactor
- Validation of dynamic segments stops key injection (`"1:admin"`, or `*` in a pattern)
- Patterns stay next to the keys they match

A fuller version is in `08_nodejs-integration/05_redis-key-builder.md`.

## Never build keys from raw user input

```ts
// Bad
const key = `shop:user:${req.query.id}`;        // "1:cart" would collide with another key

// Good
const key = keys.user(Number(req.query.id));    // validate type first
```

Also hash long or unpredictable input (URLs, search terms, JSON):

```ts
import { createHash } from "node:crypto";
const h = createHash("sha1").update(url).digest("hex").slice(0, 16);
const key = `shop:cache:page:${h}`;
```

Store the original in the value if you need to see it later.

## Versioned keys

Change the shape of cached data without deleting anything:

```ts
`shop:cache:product:88:v3`   // code v3 reads and writes only v3
```

- Deploy code that uses `v4`
- Old `v3` keys stop being read and **age out through their TTL**
- Rollback is trivial

This only works if cache keys **always have TTLs**.

### Namespace versioning (bulk invalidation)

Store a version number and include it in every key of a group:

```ts
async function productKey(id: number) {
  const ver = (await redis.get("shop:ver:products")) ?? "1";
  return `shop:cache:product:${id}:ns${ver}`;
}

// invalidate ALL product cache entries at once
await redis.incr("shop:ver:products");
```

No scans and no deletes. Old entries expire on their own. Cache the version in-process for a few seconds to avoid a Redis read per lookup.

## Cluster-safe keys and hash tags

In Redis Cluster, a key belongs to a slot: `CRC16(key) mod 16384`. **Multi-key commands, Lua scripts and transactions need all keys in the same slot.**

If a key contains `{...}`, **only the text inside the first braces is hashed**:

```
{user:1042}:profile
{user:1042}:cart
{user:1042}:orders
```

All three land in the same slot, so this works in Cluster:

```ts
await redis.multi()
  .hset("{user:1042}:profile", { name: "Ada" })
  .sadd("{user:1042}:orders", 9001)
  .exec();
```

Rules:

- Use tags only for keys that are **operated on together**
- Choose the tag carefully: a tag such as `{global}` puts everything on **one node** (a hot shard)
- Design tags at the start. Changing them means moving data

```ts
userProfile: (id: number) => `{user:${id}}:profile`,
userOrders:  (id: number) => `{user:${id}}:orders`,
```

## Cardinality and bucketing

Avoid unbounded single keys. Split by a natural dimension:

| Instead of | Use |
|-----------|-----|
| `visitors:all` (set) | `visitors:2026-09-30` per day |
| `events` (stream) | Capped with `MAXLEN ~` or per-tenant streams |
| `stats:post:42` forever | `stats:post:42:2026-09` monthly |
| One giant hash of all users | One hash per user |

Bucketing keeps each key small, makes expiry natural (`EXPIRE` the whole bucket), and spreads load across slots in Cluster.

## Hot keys and big keys

| Problem | Symptom | Mitigation |
|---------|---------|------------|
| **Hot key** (one key gets huge traffic) | One node or one core saturated | Local in-process cache, replicate as `key:0..N` shards and read randomly, or reduce work per request |
| **Big key** (huge value or collection) | Latency spikes, slow deletes, slow replication | Split into buckets, cap size, delete with `UNLINK` |

Find them:

```bash
redis-cli --bigkeys          # largest key per type (uses SCAN)
redis-cli --memkeys          # memory per key
redis-cli --hotkeys          # needs an LFU maxmemory-policy
```

```ts
await redis.memory("USAGE", "shop:user:1042");
await redis.object("FREQ", "shop:user:1042");        // LFU policy only
await redis.object("IDLETIME", "shop:user:1042");    // seconds since last access (non-LFU)
```

## Multi-tenant keys

Put the tenant early so a whole tenant can be found, measured or removed:

```
shop:t:acme:user:1042
shop:t:acme:cache:product:88
```

If tenants need strong isolation, use separate instances or ACL key patterns (`~shop:t:acme:*`) rather than only naming.

## Multiple environments

Prefer **separate Redis instances** (or at least logical databases in non-Cluster setups) for dev, staging and prod. If they must share one instance, prefix with the environment and use ACL users restricted to that prefix:

```
ACL SETUSER app-staging on >secret ~staging:* +@all -@dangerous
```

## Document the scheme

Keep a `KEYS.md` next to the builder:

```markdown
| Pattern | Type | TTL | Owner | Purpose |
|---------|------|-----|-------|---------|
| shop:cache:product:{id}:v{n} | string | 10 min ± 60s | catalog | Product JSON cache |
| shop:session:{sid} | hash | 30 min sliding, 24 h cap | auth | Session data |
| shop:lock:{name} | string | 10 s | orders | Distributed lock |
```

It pays for itself the first time someone asks "what is this key and can I delete it?"

## Finding keys safely

| Need | Use |
|------|-----|
| Does a key exist? | `EXISTS` |
| What type is it? | `TYPE` |
| How many keys? | `DBSIZE` |
| Keys matching a pattern | `SCAN` with `MATCH` (next lesson), **never** `KEYS` |
| Keys belonging to an entity, frequently | Maintain an **index set** of the keys |

```ts
// index of cache keys per user, for fast targeted invalidation
await redis.multi()
  .set(cacheKey, value, "EX", 600)
  .sadd(`shop:idx:user:${id}:cachekeys`, cacheKey)
  .expire(`shop:idx:user:${id}:cachekeys`, 700)
  .exec();
```

## Key design pitfalls

| Pitfall | Consequence | Fix |
|---------|-------------|-----|
| Inline template strings everywhere | Drift, typos, orphaned keys | One builder |
| No TTL policy per class | Memory growth | Class table with TTLs |
| Raw user input in keys | Collisions, injection | Validate or hash |
| Mixed case (`User:1` vs `user:1`) | Duplicate data | Lowercase in the builder |
| Timestamps with high resolution in keys (`:${Date.now()}`) | Millions of unique keys | Bucket to minute or hour |
| Broad hash tags (`{app}`) | Hot shard | Tag by entity |
| Forgetting that `keyPrefix` skips `SCAN` and Pub/Sub | Confusing bugs | Prefer the builder, or handle prefixes explicitly |

## Key takeaways

- Treat the key scheme as an API: hierarchical, documented, generated in one place
- Give every key class a TTL or trim policy, and separate cache from critical data
- Version cache keys, and use namespace versions for bulk invalidation
- Use hash tags deliberately for Cluster, and bucket unbounded collections

**Next:** [Scan and Iteration](./02_scan-and-iteration.md)
