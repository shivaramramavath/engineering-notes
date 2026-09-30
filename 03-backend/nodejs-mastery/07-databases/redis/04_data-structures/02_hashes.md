# Hashes

A hash is a **map of field → value under one key**. It is the natural fit for objects such as user profiles, product records or settings.

```
user:1042  ─►  { name: "Ada", email: "ada@example.com", plan: "pro", logins: "17" }
```

All fields and values are strings.

## Command tour

```ts
await redis.hset("user:1042", { name: "Ada", plan: "pro", logins: 0 });  // returns count of NEW fields
await redis.hget("user:1042", "name");                 // "Ada"
await redis.hmget("user:1042", "name", "plan", "zzz"); // ["Ada", "pro", null]
await redis.hgetall("user:1042");                      // { name: "Ada", plan: "pro", logins: "0" }
await redis.hexists("user:1042", "email");             // 1 or 0
await redis.hdel("user:1042", "plan");                 // returns count removed
await redis.hlen("user:1042");                         // number of fields
await redis.hkeys("user:1042");                        // field names
await redis.hvals("user:1042");                        // values
await redis.hsetnx("user:1042", "createdAt", Date.now()); // only if the field is missing
await redis.hincrby("user:1042", "logins", 1);         // atomic integer increment
await redis.hincrbyfloat("user:1042", "balance", 9.5); // atomic float increment
await redis.hstrlen("user:1042", "name");              // length of a value
await redis.hrandfield("user:1042");                   // random field (Redis 6.2+)
```

Notes:

- `HSET` with several fields replaced the old `HMSET`, which is deprecated
- `HGETALL` on a missing key returns `{}`, not `null`
- `HINCRBY` on a missing field starts from `0`

## Storing and loading typed objects

Because values are strings, convert on the way in and out:

```ts
interface User {
  id: number;
  name: string;
  email: string;
  logins: number;
  active: boolean;
}

const userKey = (id: number) => `user:${id}`;

async function saveUser(u: User) {
  await redis.hset(userKey(u.id), {
    id: u.id,
    name: u.name,
    email: u.email,
    logins: u.logins,
    active: u.active ? 1 : 0,
  });
}

async function loadUser(id: number): Promise<User | null> {
  const h = await redis.hgetall(userKey(id));
  if (Object.keys(h).length === 0) return null;     // missing key
  return {
    id: Number(h.id),
    name: h.name,
    email: h.email,
    logins: Number(h.logins),
    active: h.active === "1",
  };
}
```

### Partial updates (the main advantage)

```ts
await redis.hset("user:1042", { email: "new@example.com" });   // touch one field
await redis.hincrby("user:1042", "logins", 1);                 // atomic, no read needed
```

## Hash vs JSON string

| Need | Hash | JSON string |
|------|------|-------------|
| Read/update a single field | Yes, O(1) | Rewrite everything |
| Atomic field counters | `HINCRBY` | Not possible |
| Read the whole object | `HGETALL` | `GET` + parse |
| Nested objects and arrays | Awkward | Natural |
| Memory for small objects | Compact | Similar or larger |
| TTL | On the whole key only | On the whole key |

Guideline: **flat objects with independent fields → hash. Nested or read-as-a-whole → JSON string.** If one field is nested, you can store that field as a JSON string inside the hash.

## TTL on hashes

TTL applies to the **whole key**. Create the hash and its expiry together so a crash can't leave a hash without TTL:

```ts
await redis.multi()
  .hset("session:abc", { userId: 42, role: "admin" })
  .expire("session:abc", 1800)
  .exec();
```

Redis **7.4 and newer** add per-field expiry (`HEXPIRE`, `HPEXPIRE`, `HTTL`, `HPERSIST`). If your server supports it and your ioredis version doesn't type these commands, use `call`:

```ts
await redis.call("HEXPIRE", "session:abc", "60", "FIELDS", "1", "tempToken");
```

Check your server version before relying on this.

## Patterns

### 1. Session or profile store

```ts
await redis.multi()
  .hset(`session:${sid}`, { userId, ip, userAgent, createdAt: Date.now() })
  .expire(`session:${sid}`, 3600)
  .exec();
```

### 2. Per-object counters

```ts
await redis.hincrby(`stats:post:42`, "views", 1);
await redis.hincrby(`stats:post:42`, "likes", 1);
const stats = await redis.hgetall("stats:post:42");
```

### 3. Lookup index next to the object

```ts
// object
await redis.hset("user:1042", { email: "ada@example.com" });
// index: email → id
await redis.set("idx:user:email:ada@example.com", 1042);

const id = await redis.get("idx:user:email:ada@example.com");
```

Update object and index together (use `multi`) so they don't drift.

### 4. Settings and feature flags per tenant

```ts
await redis.hset("tenant:7:flags", { newUi: "on", beta: "off" });
const on = (await redis.hget("tenant:7:flags", "newUi")) === "on";
```

## Memory advantage

Small hashes use a compact **listpack** encoding (controlled by `hash-max-listpack-entries`, default 128, and `hash-max-listpack-value`, default 64 bytes). Beyond that they convert to a full hash table.

```ts
await redis.object("ENCODING", "user:1042"); // "listpack" or "hashtable"
```

Many small hashes are much cheaper than many separate string keys, which is why hashes are the standard choice for large numbers of small objects.

## Large hashes

`HGETALL`, `HKEYS` and `HVALS` are **O(N)** and block the server on huge hashes. Iterate instead:

```ts
for await (const batch of redis.hscanStream("big:hash", { count: 200 })) {
  // batch = [field1, value1, field2, value2, ...]
  for (let i = 0; i < batch.length; i += 2) {
    console.log(batch[i], batch[i + 1]);
  }
}
```

Better still: avoid unbounded hashes. Split by bucket (for example `stats:2026-09` per month).

## Complexity

| Command | Cost |
|---------|------|
| `HGET`, `HSET` (one field), `HDEL`, `HEXISTS`, `HINCRBY` | O(1) |
| `HMGET`, `HSET` (many fields) | O(N) fields given |
| `HGETALL`, `HKEYS`, `HVALS` | O(N) all fields |
| `HSCAN` | O(1) per call |

## Pitfalls

| Pitfall | Fix |
|---------|-----|
| Expecting numbers back | Convert with `Number()` |
| `HGETALL` on unbounded hashes | `HSCAN` or split the hash |
| Writing hash and `EXPIRE` separately | `multi()` or pipeline |
| Nesting objects directly | Flatten, or store nested parts as JSON strings |
| Assuming `hgetall` returns `null` for missing | It returns `{}` |
| Putting everything in one giant hash | Shard by entity or time |

## Key takeaways

- Hashes model objects with atomic field updates
- Convert types on read, since all values are strings
- TTL is per key (per-field TTL only on Redis 7.4+)
- Avoid `HGETALL` on large hashes and use `HSCAN`

**Next:** [Lists](./03_lists.md)
