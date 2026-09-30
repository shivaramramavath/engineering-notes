# Streams Overview

A stream is an **append-only log of entries**. Each entry has an auto-generated ID and a set of field/value pairs. Streams support blocking reads, replay, and **consumer groups** for distributed processing.

This lesson is the introduction. Module `10_streams` covers producers, consumers, groups, acknowledgements and patterns in depth.

```
orders  ─►  1727700000000-0 {type: created, id: 1}
            1727700000005-0 {type: paid,    id: 1}
            1727700000011-0 {type: created, id: 2}   ◄─ newest
```

## Entry IDs

Format: `<millisecondsTime>-<sequence>`, for example `1727700000000-0`.

- `*` asks Redis to generate the ID
- IDs are strictly increasing
- Range queries can use `-` (smallest) and `+` (largest)

## Writing

```ts
const id = await redis.xadd("orders", "*", "type", "created", "orderId", "1001");
// "1727700000000-0"
```

Arguments after the ID are alternating **field, value**.

Cap the stream so it doesn't grow forever:

```ts
// approximate trim (~) is much cheaper than exact
await redis.xadd("orders", "MAXLEN", "~", 100_000, "*", "type", "paid", "orderId", "1001");

// trim by age (Redis 6.2+): drop entries older than an ID
await redis.xtrim("orders", "MINID", "~", `${Date.now() - 7 * 86_400_000}-0`);
```

## Reading

### By range

```ts
await redis.xlen("orders");
await redis.xrange("orders", "-", "+");                 // all entries
await redis.xrange("orders", "-", "+", "COUNT", 10);    // first 10
await redis.xrevrange("orders", "+", "-", "COUNT", 10); // newest 10
```

Reply shape:

```
[
  ["1727700000000-0", ["type", "created", "orderId", "1001"]],
  ["1727700000005-0", ["type", "paid",    "orderId", "1001"]],
]
```

Helper to convert entries to objects:

```ts
type Entry = { id: string; data: Record<string, string> };

function parseEntries(raw: [string, string[]][]): Entry[] {
  return raw.map(([id, fields]) => {
    const data: Record<string, string> = {};
    for (let i = 0; i < fields.length; i += 2) data[fields[i]] = fields[i + 1];
    return { id, data };
  });
}
```

### With `XREAD` (tail a stream)

```ts
// read up to 10 entries with ID greater than "0" (from the beginning)
const res = await redis.xread("COUNT", 10, "STREAMS", "orders", "0");
// [["orders", [[id, fields], ...]]]  or null if nothing new

// block up to 5 seconds for NEW entries only ("$" = only newer than now)
const next = await blocker.xread("BLOCK", 5000, "STREAMS", "orders", "$");
```

A typical tail loop remembers the last ID it saw:

```ts
let lastId = "$";
while (running) {
  const res = await blocker.xread("BLOCK", 5000, "COUNT", 100, "STREAMS", "orders", lastId);
  if (!res) continue;
  for (const [, entries] of res) {
    for (const [id, fields] of parseEntries(entries as any).map((e) => [e.id, e.data] as const)) {
      await handle(id, fields);
      lastId = id;
    }
  }
}
```

Use a **dedicated connection** for blocking reads.

## Consumer groups (preview)

A consumer group lets many workers share one stream, with each entry delivered to **one** consumer and tracked until acknowledged.

```ts
// create the group (MKSTREAM creates the stream if missing)
await redis.xgroup("CREATE", "orders", "workers", "$", "MKSTREAM");

// a worker reads entries assigned to it (">" = new, never-delivered entries)
const batch = await blocker.xreadgroup(
  "GROUP", "workers", "worker-1",
  "COUNT", 10, "BLOCK", 5000,
  "STREAMS", "orders", ">"
);

// after successful processing
await redis.xack("orders", "workers", entryId);

// entries delivered but not yet acknowledged
await redis.xpending("orders", "workers");
```

If a worker dies, its unacknowledged entries can be claimed by another worker (`XAUTOCLAIM`). The full workflow is in module 10.

## Streams vs Lists vs Pub/Sub

| | Stream | List | Pub/Sub |
|---|--------|------|---------|
| Persistence of messages | Yes (until trimmed) | Until popped | **None** (fire and forget) |
| Multiple independent consumers | Yes (each tracks its own position) | No (pop removes) | Yes (live only) |
| Work sharing across workers | Consumer groups | Competing pops | No |
| Acknowledgements and retry | Yes | Manual | No |
| Replay history | Yes | No | No |
| Offline consumers catch up | Yes | Partially | No |
| Simplicity | Medium | Simple | Simple |

Rule of thumb: **Pub/Sub for ephemeral notifications, lists for the simplest queues, streams when you need durability, replay or acknowledgements.**

## Complexity

| Command | Cost |
|---------|------|
| `XADD` | O(1) (plus trimming cost if used) |
| `XLEN` | O(1) |
| `XRANGE`, `XREVRANGE` | O(log N + M) |
| `XREAD` | O(N) entries returned |
| `XREADGROUP` (new entries) | O(M) entries returned |
| `XACK` | O(1) per ID |
| `XTRIM` | O(N) entries removed |

## Memory and retention

- Streams live in memory, so **always cap or trim them**
- `MAXLEN ~ N` and `MINID ~ id` use approximate trimming, which is efficient and recommended
- Persisted via RDB/AOF like any other key
- Deleting individual entries (`XDEL`) is possible but rarely needed

## Pitfalls

| Pitfall | Fix |
|---------|-----|
| Unbounded stream growth | `MAXLEN ~` or `MINID` on `XADD`/`XTRIM` |
| Blocking `XREAD` on a shared connection | Dedicated connection |
| Reading with `0` repeatedly and reprocessing | Track and reuse the last ID |
| Forgetting `XACK` in consumer groups | Growing pending list, redelivery |
| Using Pub/Sub when messages must not be lost | Streams |
| Treating stream fields as typed | Everything is a string |

## Key takeaways

- Streams are append-only logs with IDs, blocking reads and consumer groups
- Cap them with `MAXLEN ~` or `MINID ~`
- Choose streams over Pub/Sub or lists when you need replay or acknowledgements
- Full details in [10_streams](../10_streams/README.md)

**Next:** [Choosing the Right Structure](./09_choosing-the-right-structure.md)
