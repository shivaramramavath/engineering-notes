# Expiration and TTL

Expiry lets Redis delete keys automatically after a time. It is the foundation of caching, sessions, OTPs, rate limits and locks.

## Setting expiry

### At write time (preferred, atomic)

```ts
await redis.set("otp:1042", "483920", "EX", 300); // 300 seconds
await redis.set("tmp", "x", "PX", 1500); // 1500 milliseconds
await redis.set("promo", "on", "EXAT", 1790000000); // absolute Unix time (seconds)
```

Setting value and TTL in a single command avoids a window where the key exists without an expiry.

### On an existing key

```ts
await redis.expire("session:abc", 3600); // seconds
await redis.pexpire("session:abc", 3600000); // milliseconds
await redis.expireat("session:abc", 1790000000); // absolute time
```

Since Redis 7.0 you can add conditions:

```ts
await redis.expire("k", 60, "NX"); // only if no expiry is set
await redis.expire("k", 60, "XX"); // only if an expiry already exists
await redis.expire("k", 60, "GT"); // only if new TTL is greater than current
await redis.expire("k", 60, "LT"); // only if new TTL is less than current
```

### Other write commands with expiry

```ts
await redis.setex("k", 60, "v"); // SET + EX (older form)
await redis.getex("k", "EX", 60); // read and refresh TTL (sliding expiration)
```

## Reading and removing expiry

```ts
await redis.ttl("k"); // seconds remaining
await redis.pttl("k"); // milliseconds remaining
await redis.expiretime("k"); // absolute Unix time (Redis 7+)
await redis.persist("k"); // remove expiry, key lives forever
```

### TTL return values

| Value  | Meaning                          |
| ------ | -------------------------------- |
| `>= 0` | Seconds remaining                |
| `-1`   | Key exists but has **no expiry** |
| `-2`   | Key **does not exist**           |

## Important behaviors

| Action                                               | Effect on TTL             |
| ---------------------------------------------------- | ------------------------- |
| `SET key value` (plain)                              | **Removes** the TTL       |
| `SET key value KEEPTTL`                              | Keeps the existing TTL    |
| `INCR`, `APPEND`, `HSET`, `LPUSH` (modify the value) | TTL **unchanged**         |
| `RENAME`                                             | TTL moves to the new name |
| `DEL` / key expires                                  | Key and TTL are gone      |
| `PERSIST`                                            | Removes TTL               |

A classic bug:

```ts
await redis.set("session:1", "data", "EX", 3600);
await redis.set("session:1", "new-data"); // TTL silently gone, key now never expires
// Fix:
await redis.set("session:1", "new-data", "KEEPTTL");
```

Also note: TTL is **per key**, not per field. A hash cannot have field-level expiry in older versions (recent Redis releases add hash field expiration, so check your server version before relying on it).

## How Redis expires keys

Redis combines two mechanisms:

1. **Lazy (passive) expiration**: when a key is accessed, Redis checks its expiry and deletes it if past due
2. **Active expiration**: several times per second, Redis samples keys that have TTLs and removes expired ones. If many are expired, it repeats

Consequences:

- An expired key is **never returned**, but may briefly still occupy memory
- Mass expiry at the same instant causes a CPU spike and (for caches) a **stampede**
- **Replicas do not expire keys on their own**. The primary sends a `DEL` to them. Replicas hide logically expired keys from reads

## Common TTL patterns

### 1. Cache with jitter

Add randomness so keys created together don't expire together:

```ts
const base = 600;
const jitter = Math.floor(Math.random() * 60);
await redis.set(key, value, "EX", base + jitter);
```

### 2. Sliding session

```ts
// Refresh TTL on each request
await redis.expire(`session:${sid}`, 1800);
// or read and refresh in one go
const data = await redis.getex(`session:${sid}`, "EX", 1800);
```

### 3. Absolute session lifetime

Store the creation time and cap it, so sliding refresh cannot extend a session forever:

```ts
await redis.expire(key, 1800, "LT"); // never extend beyond a fixed limit you computed
```

### 4. One-time codes (OTP)

```ts
await redis.set(`otp:${phone}`, code, "EX", 300);
const ok = (await redis.get(`otp:${phone}`)) === input;
if (ok) await redis.del(`otp:${phone}`);
```

### 5. Lock with expiry

```ts
const got = await redis.set(`lock:${res}`, token, "PX", 10000, "NX"); // "OK" or null
```

### 6. Fixed-window counter

```ts
const k = `rate:${ip}:${Math.floor(Date.now() / 60000)}`;
const n = await redis.incr(k);
if (n === 1) await redis.expire(k, 60);
```

(A crash between `INCR` and `EXPIRE` would leave a key without TTL. `12_rate-limiting` shows the atomic Lua version.)

## Keyspace notifications (optional)

Redis can publish an event when a key expires:

```
CONFIG SET notify-keyspace-events Ex
```

```ts
const sub = new Redis();
await sub.subscribe("__keyevent@0__:expired");
sub.on("message", (_ch, key) => console.log("expired:", key));
```

Notifications are **best-effort and not guaranteed** (they can be missed on disconnects). Do not build critical logic solely on them.

## Best practices

- Set a TTL on **every cache key** so memory stays bounded
- Prefer `SET ... EX` over `SET` followed by `EXPIRE`
- Add jitter to bulk-created cache entries
- Remember that a plain `SET` clears the TTL (use `KEEPTTL`)
- Monitor keys with no TTL: `TTL` returning `-1` on cache keys is a smell

## Key takeaways

- Use `SET key value EX seconds` for atomic write plus expiry
- `TTL` returns `-1` (no expiry) or `-2` (missing key) as special values
- Redis expires keys lazily and actively, and replicas rely on the primary
- Jitter prevents synchronized mass expiry

**Next:** [Persistence](./04_persistence.md)
