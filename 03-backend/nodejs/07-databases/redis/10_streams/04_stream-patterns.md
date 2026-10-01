# Stream Patterns

The previous lessons covered the mechanics. This one is a catalog of **patterns that solve real problems**, plus the operational habits that keep stream-based systems healthy.

## Pattern index

| Pattern | Problem it solves |
|---------|-------------------|
| [Work queue](#work-queue) | Share jobs among workers with retries |
| [Fan-out to services](#fan-out-to-services) | Several services react to the same events |
| [Idempotent consumers](#idempotent-consumers) | Safe handling of duplicates |
| [Outbox relay](#outbox-relay) | Reliably publish events after a database commit |
| [Partitioned streams](#partitioned-streams) | Scale throughput and keep per-key order |
| [Dead-letter and replay](#dead-letter-and-replay) | Recover from poison entries |
| [Change feed / rebuildable views](#change-feed-and-rebuildable-views) | Caches and indexes that follow the source |
| [Time-series ingestion](#time-series-ingestion) | Metrics and activity logs with windows |
| [Lag-aware producers](#lag-aware-producers) | Protect consumers from overload |
| [Realtime delivery](#realtime-delivery) | Push stream events to browsers |

## Work queue

One stream, one group, N workers: the pattern from [Consumer Groups and Acks](./03_consumer-groups-and-acks.md).

```ts
// producer
await producer.publish("thumbnail.generate", { imageId: "img_91" });

// worker (any number of instances)
const worker = new GroupWorker(blocker, redis, async ({ data }) => {
  const job = JSON.parse(data.data);
  await generateThumbnail(job.imageId);
}, { stream: "shop:stream:jobs", group: "thumbnailers", consumer: consumerName(), concurrency: 4 });
```

| Strength | Limit |
|----------|-------|
| Durable, acknowledged, replayable, no extra dependency | No delayed jobs, priorities, rate limiting, or UI out of the box |
| Observable with `XINFO` | Retry backoff is DIY |

If you need **delays, priorities, rate limits, retries with backoff and a dashboard**, use [BullMQ](../14_queues-and-workers/README.md). It is built for exactly that. Use raw streams when you want a lean durable queue, an event log, or replay.

## Fan-out to services

Give each interested service **its own group** on a shared stream:

```ts
for (const group of ["billing", "email", "analytics"]) {
  await ensureGroup(redis, "shop:stream:orders", group, "0");
}
```

Each service scales independently, fails independently, and can be added later with `0` to **catch up on history**. Compared with Pub/Sub, entries are durable and acknowledged.

Guidelines:

- Name groups after the **consuming service**, not the stream
- One stream per **event family** (`orders`, `users`), with a `type` field for routing, rather than one stream per event type
- Version payloads (`v`), because producers and consumers deploy separately

## Idempotent consumers

Delivery is at-least-once, and producers may retry, so **duplicates will happen**. The fix is in the handler.

### Option 1: make the operation naturally idempotent

Prefer this when possible: `SET` instead of `INCR`, upserts instead of inserts, `ZADD` with a stable score.

```ts
await db.orders.upsert({ id: order.id, status: "paid" });        // safe to run twice
```

### Option 2: deduplicate on `msgId`

When the operation can't be made idempotent (send an email, charge a card), record that you did it:

```ts
export async function processOnce(
  redis: Redis,
  msgId: string,
  work: () => Promise<void>,
  opts = { doneTtlSec: 7 * 86_400, lockMs: 60_000 }
): Promise<"done" | "duplicate" | "in-progress"> {
  const doneKey = `shop:done:${msgId}`;
  const lockKey = `shop:lock:msg:${msgId}`;

  if (await redis.exists(doneKey)) return "duplicate";                       // already finished

  const locked = await redis.set(lockKey, "1", "PX", opts.lockMs, "NX");     // someone else is working on it
  if (locked !== "OK") return "in-progress";

  try {
    await work();
    await redis.set(doneKey, "1", "EX", opts.doneTtlSec);                    // remember success
    return "done";
  } finally {
    await redis.unlink(lockKey);
  }
}
```

```ts
const handler = async ({ data }: StreamEntry) => {
  const result = await processOnce(redis, data.msgId!, () => sendReceipt(JSON.parse(data.data)));
  // "duplicate" and "in-progress" are both fine to acknowledge
};
```

The `doneKey` TTL must be **longer than the redelivery window** (retention plus retry time), or very old duplicates slip through.

Even this has a gap: a crash **after** `work()` and **before** setting `doneKey` repeats the work. For external side effects, pass the `msgId` as the provider's **idempotency key** (payment APIs and email providers support this), which closes the gap.

### Option 3: record the result with the side effect

When the side effect is a database write, store the `msgId` in the **same transaction**, with a unique constraint. A duplicate then fails the insert and is skipped. This is the strongest form.

## Outbox relay

**Problem:** you commit to the database, then publish an event, and a crash in between loses the event (or publishes an event for a transaction that rolled back).

**Solution:** write the event to an **outbox table in the same transaction**, then relay it to the stream.

```
BEGIN
  INSERT order ...
  INSERT outbox (id, type, payload) ...      ← same transaction
COMMIT

relay loop:  read unsent outbox rows ─► XADD (msgId = outbox id) ─► mark sent
```

```ts
async function relayOnce(batchSize = 100) {
  const rows = await db.outbox.fetchUnsent(batchSize);          // ORDER BY id, FOR UPDATE SKIP LOCKED if several relays
  for (const row of rows) {
    await producer.publish(row.type, row.payload, row.id);      // msgId = outbox row id (stable)
    await db.outbox.markSent(row.id);
  }
  return rows.length;
}

async function relayLoop() {
  while (running) {
    const n = await relayOnce();
    if (n === 0) await sleep(250);
  }
}
```

A crash between `publish` and `markSent` **re-publishes** the row on the next run, creating a duplicate with the **same `msgId`**, which consumers drop ([idempotent consumers](#idempotent-consumers)). The result is **effectively-once** processing from at-least-once pieces.

## Partitioned streams

A single stream is one key on one node, and per-entity ordering requires **one consumer** for it. To scale, split into **N streams** by a key, so each entity's events stay ordered within one partition:

```ts
import { createHash } from "node:crypto";

const PARTITIONS = 8;

export function partitionOf(key: string, n = PARTITIONS): number {
  const h = createHash("md5").update(key).digest().readUInt32BE(0);
  return h % n;
}

export const streamFor = (key: string) => `shop:stream:orders:p${partitionOf(key)}`;

// producer: same order id always lands in the same partition
await redis.xadd(streamFor(order.id), "MAXLEN", "~", 100_000, "*", "type", "order.paid", "data", JSON.stringify(order));
```

Consumers:

```ts
// each partition has its own group and (at least) one consumer, so order within a partition is preserved
for (let p = 0; p < PARTITIONS; p++) {
  const w = new GroupWorker(blockerFor(p), redis, handler, {
    stream: `shop:stream:orders:p${p}`, group: "fulfilment", consumer: `${hostname}-${p}`, concurrency: 1,
  });
  await w.start();
}
```

| Consideration | Guidance |
|---------|----------|
| Ordering | Guaranteed **per partition** (so per key), not globally |
| Changing `PARTITIONS` | Reshuffles keys, so plan capacity ahead (use more than you need now, like 16 or 32) |
| Redis Cluster | Different stream keys hash to different slots, so load spreads across shards naturally |
| Blocking connections | One per partition loop, or read several partitions in one `XREADGROUP` call (`STREAMS p0 p1 p2 > > >`) |
| Assignment | Spread partitions across instances (static assignment, or a lease in Redis) |

Start with **one stream** and partition only when throughput or ordering requires it.

## Dead-letter and replay

Covered in [Consumer Groups and Acks](./03_consumer-groups-and-acks.md#dead-letter-handling). Operational habits:

- A small **CLI or admin endpoint** to list, inspect and replay DLQ entries
- Replay with the **same `msgId`** so deduplication still protects you
- Record **why** an entry failed (error message in the DLQ entry), not just that it did
- Alert when the DLQ is non-empty for longer than your SLA

## Change feed and rebuildable views

Treat the stream as the **log of changes**, and build derived data (search indexes, caches, read models) from it:

```ts
// a projector with its own group
const projector = new GroupWorker(blocker, redis, async ({ data }) => {
  const change = JSON.parse(data.data);
  if (data.type === "product.updated") await search.index(change.productId);
  if (data.type === "product.deleted") await search.remove(change.productId);
}, { stream: "shop:stream:catalog", group: "search-indexer", consumer: consumerName() });
```

- A new view starts with a **new group from `0`** and rebuilds from the retained history
- Retention bounds how far back a rebuild can go. For full rebuilds, **re-read the source of truth**, then switch to the stream
- Keep projectors **idempotent** (upserts and deletes by ID)

## Time-series ingestion

Streams make a simple, bounded store for metrics and activity:

```ts
// ingest (bounded to roughly 1 million entries)
await redis.xadd("shop:stream:metrics:api", "MAXLEN", "~", 1_000_000, "*",
  "route", "/api/products", "ms", "42", "status", "200");

// query a window: pass timestamps as partial IDs
const from = Date.now() - 5 * 60_000;
const rows = (await redis.xrange("shop:stream:metrics:api", String(from), "+", "COUNT", 10_000)) as RawEntry[];

const durations = rows.map(([, f]) => Number(fieldsToObject(f).ms));
const avg = durations.reduce((a, b) => a + b, 0) / (durations.length || 1);
```

| Good for | Not for |
|----------|---------|
| Recent windows, dashboards over minutes or hours, "last N events" | Long retention, heavy aggregation, downsampling (use a time-series database or the Redis time-series module) |

Roll up periodically (a consumer that aggregates per minute into hashes or sorted sets), and let the raw stream expire by trimming.

## Lag-aware producers

If consumers fall behind, you can **slow producers** rather than lose entries to trimming:

```ts
async function backlog(stream: string, group: string): Promise<number> {
  const groups = (await redis.xinfo("GROUPS", stream)) as unknown[][];
  const g = groups.map(toObj).find((x) => x.name === group);
  return Number(g?.lag ?? 0) + Number(g?.pending ?? 0);          // lag needs Redis 7.0+
}

async function publishWithBackpressure(entry: () => Promise<string>, stream: string, group: string, max = 50_000) {
  while ((await backlog(stream, group)) > max) await sleep(200);
  return entry();
}
```

Cheaper variants: compare `XLEN` to a threshold, or cache the backlog for a second. For user-facing requests, **reject** with `429` or `503` instead of waiting.

## Realtime delivery

Push entries to browsers with **SSE and resume via `Last-Event-ID`** ([Producers and Consumers](./02_producers-and-consumers.md#fan-out-to-browsers-server-sent-events-with-resume)), or relay them through Socket.IO with the Redis emitter ([Socket.IO with Redis](../09_pub-sub/03_socketio-with-redis.md)):

```ts
// a worker turns durable events into live pushes
new GroupWorker(blocker, redis, async ({ data }) => {
  const evt = JSON.parse(data.data);
  emitter.to(`user:${evt.userId}`).emit("notification", evt);     // best effort live push
}, { stream: "shop:stream:notifications", group: "realtime", consumer: consumerName() });
```

The stream is the **durable record**, and the live push is **best effort**. Clients that missed a push fetch from the stream or database using their last seen ID.

## Streams, BullMQ, Pub/Sub or Kafka

| Need | Streams | BullMQ | Pub/Sub | Kafka and similar |
|------|---------|--------|---------|-------------------|
| Durable | Yes | Yes | **No** | Yes |
| Acknowledgements and retries | Manual (groups) | **Built in** | No | Offsets |
| Delays, priorities, rate limits | DIY | **Built in** | No | DIY |
| Multiple services each seeing every event | **Yes** (groups) | Awkward | Yes (live only) | **Yes** |
| Replay history | Yes (bounded by memory) | No | No | **Yes (disk, long retention)** |
| Throughput ceiling | Single-node per stream, partition for more | Single Redis | Single Redis | **Very high** |
| Operational weight | Low (you already run Redis) | Low | Lowest | **High** |
| Data size | Memory-bound | Memory-bound | None | Disk-bound |

Rules of thumb:

- **Jobs with delays, retries and a dashboard**: BullMQ
- **Durable events across a few services, moderate volume**: Streams
- **Ephemeral notifications**: Pub/Sub
- **Long retention, huge volume, many teams and consumers**: a dedicated log system

## Operations checklist

**Design**

- [ ] Every stream has a retention rule (`MAXLEN ~` or `MINID ~`), sized for worst-case consumer lag
- [ ] Every entry has `msgId`, `type`, `v`
- [ ] Every consumer is idempotent
- [ ] Group start IDs chosen on purpose (`$` or `0`)
- [ ] A DLQ exists, and someone owns it

**Reliability**

- [ ] Workers acknowledge only after the work is durable
- [ ] Reclaim loop with `minIdleMs` longer than the slowest handler
- [ ] `maxDeliveries` and poison-entry handling
- [ ] Graceful shutdown stops reads and finishes in-flight entries
- [ ] Persistence configured (AOF `everysec` plus replicas) if entries matter, and failover duplicates are tolerated

**Observability**

- [ ] Lag, pending, oldest pending age, DLQ size, stream length, per group
- [ ] Alerts on lag approaching retention, a growing DLQ, and idle consumers
- [ ] Logs carry `msgId`, entry ID and attempt count

**Performance**

- [ ] Blocking reads on dedicated connections
- [ ] Batched reads (`COUNT`) and pipelined producers where it helps
- [ ] Hot streams partitioned across keys
- [ ] Small entries (references, not blobs)

## Testing patterns

- **Unique stream and group per test**, cleaned up with `unlink`
- **Explicit IDs** for deterministic ordering assertions
- A **crash test**: handle without acknowledging, then verify reclaim and idempotent reprocessing
- A **poison test**: a handler that always throws ends in the DLQ after `maxDeliveries` (use a small `minIdleMs`)
- A **duplicate test**: publish the same `msgId` twice, and assert the side effect happened once
- Run these against a real Redis (Testcontainers, see `18_testing-and-debugging`)

## Pitfalls

| Pitfall | Fix |
|---------|-----|
| Treating streams as exactly-once | At-least-once plus idempotency |
| Outbox row marked sent before publishing | Publish, then mark sent (and deduplicate) |
| `msgId` generated fresh on every retry | Derive it from the business event or outbox ID |
| Partition count that must change later | Over-provision partitions up front |
| Dashboards without lag-versus-retention alerts | Alert on the ratio |
| Replaying a DLQ with new `msgId`s | Keep the original |
| Using streams as a long-term event store | Memory is finite, so archive to durable storage |
| Complex retry and scheduling logic reinvented on streams | Use BullMQ for that |
| Many tiny streams, one per user, forever | Bound cardinality, expire idle streams |

## Key takeaways

- Streams cover work queues, durable fan-out, change feeds and bounded time series, with **at-least-once** delivery
- **Idempotency** (a stable `msgId` plus deduplication) turns at-least-once into effectively-once
- The **outbox** pattern closes the gap between database commits and published events
- Partition across stream keys for throughput and per-key ordering
- Operate by **lag, pending, DLQ and retention**, and choose BullMQ or Kafka when their features are what you need

**Next module:** [11_distributed-locks](../11_distributed-locks/README.md)
