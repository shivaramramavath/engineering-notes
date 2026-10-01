# Stream Fundamentals

## What a stream is

A stream is a **log of entries** stored under one Redis key. Each entry has:

- An **ID**, assigned in increasing order
- A set of **field/value string pairs**

```
shop:stream:orders
  1727700000000-0  { type: "created", orderId: "1001", totalCents: "4999" }
  1727700000005-0  { type: "paid",    orderId: "1001" }
  1727700000011-0  { type: "created", orderId: "1002", totalCents: "1250" }
```

Unlike a list, **reading doesn't remove entries**. Unlike Pub/Sub, entries **persist** until you trim them, so consumers can start late, restart, or replay.

## If you know Kafka

| Kafka | Redis Streams |
|-------|---------------|
| Topic partition | A stream key |
| Offset | Entry ID |
| Consumer group | Consumer group |
| Committed offset | Group's last-delivered ID, plus the pending list |
| Retention policy | `MAXLEN` / `MINID` trimming (you choose it, and it's memory-bound) |
| Partitioning for scale | Several stream keys, chosen by you ([patterns](./04_stream-patterns.md)) |

## Entry IDs

Format: `<milliseconds>-<sequence>`, for example `1727700000000-0`.

- `*` lets Redis generate it from the server clock
- The sequence number distinguishes entries created in the same millisecond
- IDs are **strictly increasing** per stream, even if the clock moves backwards
- Because the ID contains a timestamp, you can query **by time range**

You can supply an explicit ID (it must be greater than the last one), which is handy in tests:

```ts
await redis.xadd("test:s", "1-0", "a", "1");
await redis.xadd("test:s", "2-0", "a", "2");
await redis.xadd("test:s", "2-0", "a", "3");   // ✗ error: ID is not greater than the top item
```

## Writing

```ts
const id = await redis.xadd(STREAM, "*", "type", "created", "orderId", "1001", "totalCents", "4999");
// "1727700000000-0"
```

All values are strings. Helpers keep call sites tidy:

```ts
export type RawEntry = [id: string, fields: string[]];
export interface StreamEntry<T = Record<string, string>> { id: string; data: T }

export function objectToFields(obj: Record<string, string | number | boolean>): string[] {
  return Object.entries(obj).flatMap(([k, v]) => [k, String(v)]);
}

export function fieldsToObject(fields: string[]): Record<string, string> {
  const out: Record<string, string> = {};
  for (let i = 0; i < fields.length; i += 2) out[fields[i]!] = fields[i + 1]!;
  return out;
}

await redis.xadd(STREAM, "*", ...objectToFields({ type: "paid", orderId: "1001" }));
```

### Flat fields or one JSON field?

| | Flat fields | `data` = JSON string |
|---|-------------|----------------------|
| Inspect in Insight or `redis-cli` | Easy | Opaque |
| Nested data | Awkward | Natural |
| Schema validation | Per field | One decode step |
| Size | Slightly smaller | Slightly larger |

A good compromise: a few **flat routing fields** (`type`, `v`, `msgId`) plus a **JSON `data` field** for the payload.

```ts
await redis.xadd(STREAM, "*",
  "type", "order.paid", "v", "1", "msgId", crypto.randomUUID(),
  "data", JSON.stringify({ orderId: "1001", totalCents: 4999 }));
```

### Bounding the stream when writing

Every stream needs a retention rule, or memory grows forever.

```ts
// keep roughly the newest 100,000 entries ("~" = approximate, much cheaper)
await redis.xadd(STREAM, "MAXLEN", "~", 100_000, "*", "type", "click");

// keep roughly the last 7 days (entries with IDs below this are eligible for trimming)
const minId = `${Date.now() - 7 * 86_400_000}-0`;
await redis.xadd(STREAM, "MINID", "~", minId, "*", "type", "click");

// don't create the stream if it doesn't exist (Redis 6.2+)
await redis.xadd(STREAM, "NOMKSTREAM", "*", "type", "click");
```

| Trim option | Meaning |
|-------------|---------|
| `MAXLEN n` | Keep at most `n` entries |
| `MINID id` | Drop entries older than `id` (time-based retention) |
| `~` | **Approximate**: trims in whole internal blocks, so slightly more than the limit may remain. Much faster, and almost always what you want |
| `=` (default) | Exact trimming. Costs more |

You can also trim independently:

```ts
await redis.xtrim(STREAM, "MAXLEN", "~", 100_000);
await redis.xtrim(STREAM, "MINID", "~", `${Date.now() - 86_400_000}-0`);
```

> **Trimming and consumer groups.** Trimming deletes entries **even if a group hasn't read them yet**. If consumers fall behind, they lose data. Size retention for your worst realistic lag, monitor lag ([next lessons](./03_consumer-groups-and-acks.md)), and note that newer Redis releases add options to trim only entries every group has acknowledged. Check the docs for your version.

## Reading by range

```ts
await redis.xlen(STREAM);                                   // number of entries
await redis.xrange(STREAM, "-", "+");                       // everything (careful on big streams)
await redis.xrange(STREAM, "-", "+", "COUNT", 100);         // first 100
await redis.xrevrange(STREAM, "+", "-", "COUNT", 10);       // newest 10 (note: end before start)
```

Reply shape:

```
[
  ["1727700000000-0", ["type", "created", "orderId", "1001"]],
  ["1727700000005-0", ["type", "paid", "orderId", "1001"]],
]
```

```ts
const entries: StreamEntry[] = (await redis.xrange(STREAM, "-", "+", "COUNT", 100) as RawEntry[])
  .map(([id, fields]) => ({ id, data: fieldsToObject(fields) }));
```

### Paging with an exclusive start

Prefix an ID with `(` to exclude it (Redis 6.2+), so each page starts after the previous one:

```ts
async function* pages(stream: string, count = 500) {
  let cursor = "-";
  while (true) {
    const batch = (await redis.xrange(stream, cursor, "+", "COUNT", count)) as RawEntry[];
    if (batch.length === 0) return;
    yield batch;
    cursor = `(${batch[batch.length - 1]![0]}`;       // exclusive: continue AFTER the last ID
  }
}

for await (const batch of pages(STREAM)) { /* process */ }
```

### Querying by time

IDs start with a millisecond timestamp, and you can pass **just the timestamp** as a bound (`start` defaults to sequence 0, `end` to the maximum sequence):

```ts
const lastMinute = await redis.xrange(STREAM, String(Date.now() - 60_000), "+");
const window = await redis.xrange(STREAM, String(fromMs), String(toMs));
```

This turns a stream into a simple time-series store (see [Stream Patterns](./04_stream-patterns.md)).

## Reading new entries: `XREAD`

```ts
// entries after ID "0" (from the beginning), up to 10
const res = await redis.xread("COUNT", 10, "STREAMS", STREAM, "0");
// [[ "shop:stream:orders", [ [id, fields], ... ] ]]   or null when nothing is available

// block up to 5 s waiting for entries newer than "now" (`$` = the stream's current last ID)
const live = await blocker.xread("BLOCK", 5000, "STREAMS", STREAM, "$");
```

| `XREAD` ID argument | Meaning |
|--------------------|---------|
| `0` or `0-0` | From the very beginning |
| `$` | Only entries added **after** this call starts (live tail) |
| A real ID | Entries **after** that ID (exclusive), the way to resume |
| `BLOCK 0` | Wait forever |

> **`$` gotcha.** If you loop with `$` every time, entries added between two calls are **skipped**. Use `$` once for your first call, then pass the **last ID you received**. [Producers and Consumers](./02_producers-and-consumers.md) shows the loop.

`XREAD` can watch several streams: `"STREAMS", "a", "b", lastA, lastB`.

## Housekeeping

```ts
await redis.xdel(STREAM, "1727700000000-0");        // delete one entry (rarely needed)
await redis.xtrim(STREAM, "MAXLEN", "~", 10_000);   // retention
await redis.unlink(STREAM);                          // delete the whole stream
```

`XDEL` marks entries deleted but doesn't shrink memory right away. Prefer trimming for retention.

### Inspecting

```ts
await redis.xinfo("STREAM", STREAM);
// flat array: length, radix-tree-keys, radix-tree-nodes, last-generated-id, groups, first-entry, last-entry ...

await redis.xinfo("STREAM", STREAM, "FULL");        // adds group and consumer detail (can be large)
```

```ts
function toObj(flat: unknown[]): Record<string, unknown> {
  const o: Record<string, unknown> = {};
  for (let i = 0; i < flat.length; i += 2) o[String(flat[i])] = flat[i + 1];
  return o;
}

const info = toObj((await redis.xinfo("STREAM", STREAM)) as unknown[]);
console.log(info.length, info["last-generated-id"]);
```

From the CLI: `XINFO STREAM key`, `XLEN key`, `XRANGE key - + COUNT 5`.

## Memory, persistence and replication

- Streams are normal keys. **RDB/AOF** persist them, and **replicas** copy them
- Replication is **asynchronous**: after a failover, the newest entries and acknowledgements can be missing. Design consumers to be idempotent ([Stream Patterns](./04_stream-patterns.md))
- Memory per entry is the payload plus a small overhead. Measure rather than guess:

```ts
await redis.memory("USAGE", STREAM, "SAMPLES", 0);       // exact (can be slow on huge streams)
```

- Capacity plan: `entries retained × average size`. For example, 1 KB entries retained for 1 million entries is roughly 1 GB
- **Big streams are fine**, but huge payloads aren't. Keep entries small and store blobs elsewhere, referencing them by ID

## Streams in Cluster

A stream is **one key, so one slot, so one node**. A single extremely busy stream can bottleneck its shard. To scale, **split into several stream keys** ([partitioning](./04_stream-patterns.md)). Ordering is guaranteed only **within** a stream.

## Complexity

| Command | Cost |
|---------|------|
| `XADD` | O(1) (plus amortized trimming) |
| `XLEN` | O(1) |
| `XRANGE`, `XREVRANGE` | O(log N + M) |
| `XREAD` | O(M) for M entries returned |
| `XTRIM` | O(N) for N entries removed |
| `XDEL` | O(1) per entry |
| `XINFO STREAM FULL` | O(N), use sparingly |

## Pitfalls

| Pitfall | Fix |
|---------|-----|
| No retention rule | `MAXLEN ~` or `MINID ~` from day one |
| Trimming shorter than your worst consumer lag | Size retention for lag, and alert on it |
| Looping `XREAD` with `$` each time | `$` once, then the last received ID |
| `XRANGE - +` on a huge stream | Page with `COUNT` and an exclusive start |
| Exact `MAXLEN =` on a hot path | Use `~` |
| Large payloads in entries | Store blobs elsewhere, put the reference in the entry |
| Assuming no loss across a failover | Idempotent consumers, replicas plus AOF, `WAIT` for critical writes |
| One giant stream for everything in Cluster | Partition across keys |
| Treating fields as typed | Everything is a string, so convert and validate |

## Key takeaways

- A stream is a persistent, ordered log. Reading does **not** consume entries
- IDs are `ms-seq`, are strictly increasing, and double as timestamps and resume cursors
- Always set retention (`MAXLEN ~` or `MINID ~`), and size it for consumer lag
- Use an exclusive `(` start for paging, and the last ID (not `$`) when tailing

**Next:** [Producers and Consumers](./02_producers-and-consumers.md)
