# Bitmaps and Bitfields

Bitmaps and bitfields are not separate types. They are **bit-level operations on a regular string**. They give extreme memory efficiency for boolean flags and small packed counters.

## Bitmaps

Think of a string as a long array of bits, where the **offset** is the position (often a user ID).

```
offset:  0 1 2 3 4 5 6 7 8 ...
bit:     0 1 0 0 1 0 0 0 1 ...
```

### Command tour

```ts
await redis.setbit("active:2026-09-30", 1042, 1);   // returns the PREVIOUS bit value
await redis.getbit("active:2026-09-30", 1042);      // 1
await redis.bitcount("active:2026-09-30");          // number of bits set to 1
await redis.bitcount("active:2026-09-30", 0, 99);   // over a byte range
await redis.bitpos("active:2026-09-30", 1);         // offset of the first 1 bit
```

Combine bitmaps with `BITOP`:

```ts
await redis.bitop("AND", "dest", "active:day1", "active:day2");  // active on BOTH days
await redis.bitop("OR",  "dest", "active:day1", "active:day2");  // active on EITHER day
await redis.bitop("XOR", "dest", "a", "b");
await redis.bitop("NOT", "dest", "a");
```

`BITOP` returns the length of the resulting string in bytes.

### Size

- The maximum offset is **2^32 - 1** (about 4.29 billion bits, 512 MB)
- Setting a high offset allocates memory up to that point: `SETBIT k 8000000 1` creates about 1 MB
- 1 million users cost about **125 KB**

## Patterns

### 1. Daily active users

```ts
const key = `dau:${new Date().toISOString().slice(0, 10)}`;
await redis.setbit(key, userId, 1);
await redis.expire(key, 60 * 60 * 24 * 90);         // keep 90 days

const dau = await redis.bitcount(key);
```

### 2. Retention (active today AND yesterday)

```ts
await redis.bitop("AND", "tmp:retained", "dau:2026-09-30", "dau:2026-09-29");
const retained = await redis.bitcount("tmp:retained");
await redis.del("tmp:retained");
```

### 3. Weekly active (active on ANY day)

```ts
await redis.bitop("OR", "tmp:wau", ...last7DayKeys);
const wau = await redis.bitcount("tmp:wau");
```

### 4. Per-user feature or permission flags

```ts
const FLAGS = { emailOptIn: 0, betaTester: 1, verified: 2 } as const;
await redis.setbit(`flags:user:${id}`, FLAGS.verified, 1);
const verified = (await redis.getbit(`flags:user:${id}`, FLAGS.verified)) === 1;
```

### 5. Attendance / check-in grid

```ts
await redis.setbit(`checkin:${userId}:2026`, dayOfYear, 1);
const daysAttended = await redis.bitcount(`checkin:${userId}:2026`);
```

## Bitmap trade-offs

| Good for | Poor for |
|----------|----------|
| Dense integer IDs (1 to N) | Sparse or huge IDs (UUIDs, 10-digit ids) |
| Counting, AND/OR across days | Storing anything besides 0/1 |
| Tiny memory per flag | Listing members without scanning bits |

If IDs are sparse, either **map IDs to a dense index** or use a set / HyperLogLog. A user with ID `4,000,000,000` forces a ~500 MB allocation.

`BITOP` on very large bitmaps is O(N) and can block, so run it off the request path.

## Bitfields

`BITFIELD` treats a string as an array of **fixed-width integers** (signed `i1` to `i64`, unsigned `u1` to `u63`) and lets you read, write and increment them atomically.

### Command tour

```ts
// SET a u8 at bit offset 0 to 200, then GET it back
await redis.bitfield("stats", "SET", "u8", 0, 200, "GET", "u8", 0);   // [0, 200]

// INCRBY the u8 at offset 0 by 1
await redis.bitfield("stats", "INCRBY", "u8", 0, 1);                  // [201]

// '#N' means "the Nth field of this width", so counters don't overlap
await redis.bitfield("stats", "INCRBY", "u8", "#0", 1);               // counter 0
await redis.bitfield("stats", "INCRBY", "u8", "#1", 1);               // counter 1
await redis.bitfield("stats", "GET", "u8", "#0", "GET", "u8", "#1");  // [n0, n1]
```

Each sub-command returns one value in the reply array, so you can batch several in one call.

### Overflow behavior

```ts
await redis.bitfield("c", "OVERFLOW", "SAT",  "INCRBY", "u4", 0, 20);  // saturate at 15
await redis.bitfield("c", "OVERFLOW", "WRAP", "INCRBY", "u4", 0, 20);  // wrap around (default)
await redis.bitfield("c", "OVERFLOW", "FAIL", "INCRBY", "u4", 0, 20);  // returns null instead
```

`OVERFLOW` applies to the `INCRBY` commands that **follow** it.

## Bitfield patterns

### 1. Many tiny counters in one key

```ts
// 4-bit counters (0 to 15) for 1000 items in only 500 bytes
await redis.bitfield("votes:poll1", "OVERFLOW", "SAT", "INCRBY", "u4", `#${optionIndex}`, 1);
```

### 2. Compact per-user stats

```ts
// three u16 counters packed in 6 bytes: logins, purchases, reports
await redis.bitfield(`ustats:${id}`, "INCRBY", "u16", "#0", 1);
```

### 3. Rate counters that must not wrap

Use `OVERFLOW SAT` so a burst can't wrap back to zero.

## Complexity

| Command | Cost |
|---------|------|
| `SETBIT`, `GETBIT` | O(1) |
| `BITCOUNT` | O(N) bytes in range (fast, optimized) |
| `BITPOS` | O(N) |
| `BITOP` | O(N) over the longest string |
| `BITFIELD` | O(1) per sub-command |

## Pitfalls

| Pitfall | Fix |
|---------|-----|
| Huge offsets allocating hundreds of MB | Dense IDs or a different structure |
| Running `BITOP` on giant bitmaps inline | Run in background, delete temp keys |
| Forgetting `SETBIT` returns the old value | Use it to detect first-time sets |
| Never expiring daily keys | `EXPIRE` on creation |
| Mixing bitfield widths on the same key | Keep one layout per key |

## Key takeaways

- Bitmaps store one bit per ID: ideal for dense, boolean, large-scale tracking
- `BITCOUNT` and `BITOP` give cheap analytics (DAU, retention)
- Bitfields pack small integers with atomic increments and overflow control
- Watch out for sparse IDs and large `BITOP`s

**Next:** [HyperLogLog and Geo](./07_hyperloglog-and-geo.md)
