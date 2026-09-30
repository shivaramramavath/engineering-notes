# Lists

A list is an **ordered sequence of strings**, implemented as a linked structure optimized for **adding and removing at both ends**.

```
head ◄─ "c" ─ "b" ─ "a" ─► tail        LPUSH adds at head, RPUSH adds at tail
```

## Command tour

```ts
await redis.lpush("tasks", "a", "b", "c");     // head: c, b, a   (returns length)
await redis.rpush("tasks", "d");               // tail
await redis.lrange("tasks", 0, -1);            // ["c","b","a","d"]  (-1 = last)
await redis.llen("tasks");                     // 4
await redis.lindex("tasks", 1);                // "b"
await redis.lpop("tasks");                     // "c"
await redis.rpop("tasks");                     // "d"
await redis.lpop("tasks", 2);                  // pop up to 2 (Redis 6.2+), returns array
await redis.lset("tasks", 0, "z");             // replace by index
await redis.linsert("tasks", "BEFORE", "a", "x");
await redis.lrem("tasks", 1, "x");             // remove 1 occurrence (count 0 = all)
await redis.ltrim("tasks", 0, 99);             // keep only indexes 0..99
await redis.lpos("tasks", "a");                // index of first match (Redis 6.0.6+)
await redis.lmove("src", "dst", "RIGHT", "LEFT"); // atomically pop from src, push to dst
```

Notes:

- `LRANGE` uses **inclusive** indexes, and negative values count from the end
- Popping from an empty list returns `null`
- An empty list **disappears** automatically
- `LMOVE` replaces the deprecated `RPOPLPUSH`

## Blocking commands

Block until an element arrives instead of polling:

```ts
const res = await blocker.blpop("queue:jobs", 5);   // wait up to 5s
if (res) {
  const [key, value] = res;                         // ["queue:jobs", "payload"]
}
```

`BRPOP`, `BLMOVE` and `BLMPOP` work the same way. **Use a dedicated connection** (`redis.duplicate()`) because a blocking command occupies the connection.

## Patterns

### 1. Simple FIFO queue

```ts
// producer
await redis.lpush("queue:emails", JSON.stringify({ to: "a@b.com" }));

// consumer: take from the opposite end
const item = await blocker.brpop("queue:emails", 5);
```

`LPUSH` + `RPOP` gives first-in-first-out.

### 2. Stack (LIFO)

```ts
await redis.lpush("undo", "action1");
await redis.lpop("undo");
```

### 3. Capped "recent items" feed

```ts
await redis.multi()
  .lpush(`recent:${userId}`, itemId)
  .ltrim(`recent:${userId}`, 0, 49)          // keep newest 50
  .exec();

const recent = await redis.lrange(`recent:${userId}`, 0, 9);  // latest 10
```

### 4. Reliable queue with a processing list

A plain `RPOP` loses the job if the worker crashes. Move it to a processing list first:

```ts
while (running) {
  const job = await blocker.blmove("queue:jobs", "queue:processing", "RIGHT", "LEFT", 2);
  if (!job) continue;

  try {
    await handle(JSON.parse(job));
    await redis.lrem("queue:processing", 1, job);    // acknowledge
  } catch (err) {
    // leave it in processing. A reaper job re-queues stale items
  }
}
```

You still need a reaper for stuck items. For anything serious, prefer **Streams with consumer groups** (module 10) or **BullMQ** (module 14).

### 5. Activity log with a size cap

```ts
await redis.rpush("log:app", line);
await redis.ltrim("log:app", -1000, -1);        // keep the last 1000 lines
```

## Encoding and memory

Small lists use a compact **listpack**, and larger ones use a **quicklist** (a linked list of listpacks). Both are memory-efficient for typical sizes.

## Complexity

| Command | Cost |
|---------|------|
| `LPUSH`, `RPUSH`, `LPOP`, `RPOP`, `LLEN`, `LMOVE` | O(1) |
| `LINDEX`, `LSET` | O(N) (fast at the ends, slow in the middle) |
| `LRANGE` | O(S + N) (start offset + elements returned) |
| `LINSERT`, `LREM`, `LPOS` | O(N) |
| `LTRIM` | O(N) elements removed |

Lists are **not** a good fit for random access in the middle of a very long list.

## Lists vs alternatives

| Need | Better choice |
|------|---------------|
| Queue with acknowledgements and retries | Streams or BullMQ |
| Fan-out to many independent consumers | Streams or Pub/Sub |
| Time-ordered feed with ranges by time | Sorted set |
| Unique elements | Set |
| Priority queue | Sorted set |

## Pitfalls

| Pitfall | Fix |
|---------|-----|
| Using a list as a durable queue with no processing list | Use `LMOVE` or Streams |
| Blocking commands on the shared connection | Dedicated connection |
| Very long `BLPOP` timeouts during shutdown | Short timeouts plus a `running` flag |
| Unbounded lists (feeds, logs) | `LTRIM` after each push |
| `LRANGE 0 -1` on huge lists | Page with ranges |
| Indexing into the middle of huge lists | Reconsider the structure |

## Key takeaways

- Lists are fast at both ends and slow in the middle
- `LPUSH` + `BRPOP` gives a simple queue, `LMOVE` makes it safer
- Cap growing lists with `LTRIM`
- For reliable messaging, graduate to Streams or BullMQ

**Next:** [Sets](./04_sets.md)
