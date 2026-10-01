# Producers and Consumers

This lesson covers streams **without consumer groups**: producers appending entries, and independent readers that each track their own position. Consumer groups, which share work between workers, are in the [next lesson](./03_consumer-groups-and-acks.md).

## Producers

A producer's job is simple: validate, append, bound the stream. The tricky parts are **failure behavior** and **duplicates**.

### A small typed producer

```ts
import { randomUUID } from "node:crypto";
import type { Redis } from "ioredis";

export interface ProducerOptions {
  stream: string;
  maxLen?: number;                          // approximate cap, default 100_000
  source: string;                           // who produced it, for debugging
}

export class StreamProducer<T extends Record<string, unknown>> {
  constructor(private redis: Redis, private o: ProducerOptions) {}

  async publish(type: string, data: T, msgId: string = randomUUID()): Promise<string> {
    return (await this.redis.xadd(
      this.o.stream,
      "MAXLEN", "~", this.o.maxLen ?? 100_000,
      "*",
      "type", type,
      "v", "1",
      "msgId", msgId,
      "src", this.o.source,
      "ts", String(Date.now()),
      "data", JSON.stringify(data)
    )) as string;
  }

  /** Many entries in one round trip. NOT atomic: each XADD is independent. */
  async publishMany(items: { type: string; data: T; msgId?: string }[]): Promise<string[]> {
    const p = this.redis.pipeline();
    for (const it of items) {
      p.xadd(
        this.o.stream, "MAXLEN", "~", this.o.maxLen ?? 100_000, "*",
        "type", it.type, "v", "1", "msgId", it.msgId ?? randomUUID(),
        "src", this.o.source, "ts", String(Date.now()), "data", JSON.stringify(it.data)
      );
    }
    const res = (await p.exec()) ?? [];
    return res.map(([err, id]) => {
      if (err) throw err;
      return id as string;
    });
  }
}
```

Each entry carries:

| Field | Purpose |
|-------|---------|
| `type` | Routing and filtering |
| `v` | Schema version, so old and new consumers can coexist |
| `msgId` | A **business-level unique ID**, the key to deduplication |
| `src`, `ts` | Debugging and rough latency measurement |
| `data` | The payload as JSON |

### Producers can create duplicates

`XADD` is **not idempotent**. If it times out and you retry, the first attempt may have succeeded, so you now have two entries:

```
XADD ... ──timeout──►   (did it land? unknown)
XADD ... (retry)        → possibly a duplicate
```

You have three options:

| Option | How |
|--------|-----|
| **Don't retry** blindly | Fine for metrics and logs |
| **Retry and deduplicate in the consumer** | Include a stable `msgId` on every attempt (the producer above does), and have consumers skip IDs they've processed ([idempotent consumers](./04_stream-patterns.md#idempotent-consumers)) |
| **Deterministic entry IDs** | Supply an explicit ID derived from the business key. A retry then fails with "ID is not greater" instead of duplicating. This only works with strictly increasing IDs |

The `msgId` approach is the most common. Keep the same `msgId` across retries:

```ts
const msgId = randomUUID();
await withRetry(() => producer.publish("order.paid", data, msgId));   // same msgId each attempt
```

### When the stream or Redis is unavailable

Decide per use case:

| Producer | Policy |
|----------|--------|
| Metrics, click logs | Drop, count the drop |
| User actions you must not lose | Write to a **local outbox** (database table) and relay later ([outbox pattern](./04_stream-patterns.md#outbox-relay)) |
| Request path | Short `commandTimeout`, fail fast, respond with `503` or degrade |

### Producer backpressure

`MAXLEN` protects memory, but silently discards the oldest entries. If consumers lag and you'd rather slow producers down than lose data, check lag first (see [monitoring](./03_consumer-groups-and-acks.md#monitoring)):

```ts
async function canAccept(stream: string, maxBacklog = 50_000) {
  return (await redis.xlen(stream)) < maxBacklog;
}
```

## Independent consumers (tailing)

A reader that **tracks its own position** and never acknowledges anything. Every such reader sees **every** entry. This is the right model for:

- Projections and caches that rebuild from the log
- Broadcasting to connected clients (SSE, WebSockets)
- Audit, analytics or search-index feeders
- Anything where a **single consumer** processes everything in order

### The loop

```ts
let lastId = "$";                          // "$" ONCE: only entries from now on

while (running) {
  const res = await blocker.xread("COUNT", 100, "BLOCK", 5000, "STREAMS", STREAM, lastId);
  if (!res) continue;                      // timed out, nothing new

  for (const [, entries] of res as [string, RawEntry[]][]) {
    for (const [id, fields] of entries) {
      await handle({ id, data: fieldsToObject(fields) });
      lastId = id;                         // advance only after handling
    }
  }
}
```

Every call passes the **last ID you handled**, so nothing between calls is missed. Short `BLOCK` timeouts (a few seconds) keep the loop responsive to shutdown.

### Start positions

| Start at | Behavior |
|----------|----------|
| `$` | Only entries created from now on |
| `0` | Replay the entire retained history, then continue live |
| A saved ID | **Resume** exactly where the previous run stopped |

### A reusable reader with checkpoints

To resume after a restart, **persist the last handled ID** somewhere durable: a Redis key for light uses, your database for anything critical (so the checkpoint commits with your side effects).

```ts
export interface ReaderOptions {
  stream: string;
  name: string;                                         // identifies the checkpoint
  startFrom?: "$" | "0";                                // used only when there is no checkpoint
  batch?: number;
  blockMs?: number;
  checkpointKey?: string;
}

export class StreamReader {
  private running = false;
  private done?: Promise<void>;

  constructor(
    private blocker: Redis,                             // dedicated blocking connection
    private redis: Redis,                               // normal commands (checkpoints)
    private handle: (e: StreamEntry) => Promise<void>,
    private o: ReaderOptions,
    private log: { error: (o: unknown, m?: string) => void } = console
  ) {}

  private get ckKey() { return this.o.checkpointKey ?? `${this.o.stream}:cursor:${this.o.name}`; }

  async start() {
    this.running = true;
    this.done = this.loop();
  }

  async stop() {
    this.running = false;
    await this.done;                                    // exits within one blockMs
  }

  private async loop() {
    let lastId = (await this.redis.get(this.ckKey)) ?? this.o.startFrom ?? "$";
    let backoff = 100;

    while (this.running) {
      try {
        const res = await this.blocker.xread(
          "COUNT", this.o.batch ?? 100, "BLOCK", this.o.blockMs ?? 5000, "STREAMS", this.o.stream, lastId
        );
        backoff = 100;
        if (!res) continue;

        for (const [, entries] of res as [string, RawEntry[]][]) {
          for (const [id, fields] of entries) {
            await this.handle({ id, data: fieldsToObject(fields) });   // if this throws, we retry from lastId
            lastId = id;
          }
        }
        await this.redis.set(this.ckKey, lastId);                      // checkpoint after each batch
      } catch (err) {
        this.log.error({ err, stream: this.o.stream, lastId }, "reader error, retrying");
        await new Promise((r) => setTimeout(r, backoff));
        backoff = Math.min(backoff * 2, 5000);
      }
    }
  }
}
```

Behavior to note:

- If `handle` throws, the loop **retries the same entry** from `lastId` (so a poison entry blocks the reader). For per-entry error policy, catch inside `handle`, or use a [consumer group](./03_consumer-groups-and-acks.md) with a dead-letter stream
- The checkpoint is written **after the batch**. A crash mid-batch replays part of that batch, giving **at-least-once** semantics, so handlers should be idempotent
- If the checkpoint ID has been **trimmed** from the stream, `XREAD` simply returns entries after it, so retention shorter than your downtime means silently skipped entries. Monitor the gap (`first-entry` ID vs your checkpoint)
- The blocking connection should come from the `"blocking"` role in your [connection factory](../08_nodejs-integration/01_connection-management.md), with **no `commandTimeout`** shorter than `BLOCK`

### History, then live

Catch up from a checkpoint without blocking, and only then go live. `XREAD` already does this: pass your last ID, and it returns whatever is newer immediately, and blocks only when you're caught up. One loop handles both phases.

### Reading several streams

```ts
const res = await blocker.xread("COUNT", 100, "BLOCK", 5000, "STREAMS", "s:orders", "s:users", lastOrders, lastUsers);
// res = [[ "s:orders", [...] ], [ "s:users", [...] ]]  (only streams that had entries)
```

Keep one last ID per stream.

## Fan-out to browsers: Server-Sent Events with resume

Stream entry IDs make a natural SSE `id:`. When the browser reconnects, it sends `Last-Event-ID`, and you resume from there. Because `XREAD BLOCK` holds a connection, **don't open one per browser**. Run **one shared reader** per process and fan out in memory:

```ts
import { EventEmitter } from "node:events";

// ID comparison (ms-seq) without floating-point surprises
export function cmpId(a: string, b: string): number {
  const [am, as] = a.split("-").map(BigInt) as [bigint, bigint];
  const [bm, bs] = b.split("-").map(BigInt) as [bigint, bigint];
  if (am !== bm) return am < bm ? -1 : 1;
  return as === bs ? 0 : as < bs ? -1 : 1;
}

export class LiveFeed extends EventEmitter {
  private running = false;
  private done?: Promise<void>;
  constructor(private blocker: Redis, private stream: string) { super(); this.setMaxListeners(0); }

  start() { this.running = true; this.done = this.loop(); }
  async stop() { this.running = false; await this.done; }

  private async loop() {
    let last = "$";
    while (this.running) {
      try {
        const res = await this.blocker.xread("COUNT", 100, "BLOCK", 3000, "STREAMS", this.stream, last);
        if (!res) continue;
        for (const [, entries] of res as [string, RawEntry[]][]) {
          for (const [id, fields] of entries) {
            last = id;
            this.emit("entry", { id, data: fieldsToObject(fields) } satisfies StreamEntry);
          }
        }
      } catch {
        await new Promise((r) => setTimeout(r, 500));
      }
    }
  }
}
```

The SSE endpoint replays missed entries from the stream, then switches to live, without gaps or duplicates:

```ts
app.get("/api/events", async (req, res) => {
  res.set({ "Content-Type": "text/event-stream", "Cache-Control": "no-cache", Connection: "keep-alive" });
  res.flushHeaders();

  const send = (e: StreamEntry) => res.write(`id: ${e.id}\nevent: ${e.data.type}\ndata: ${e.data.data}\n\n`);

  // 1. start buffering live entries FIRST so nothing is missed during the replay
  let replaying = true;
  const buffer: StreamEntry[] = [];
  let lastSent = req.header("Last-Event-ID") ?? "$";

  const onEntry = (e: StreamEntry) => {
    if (replaying) buffer.push(e);
    else if (cmpId(e.id, lastSent) > 0) { send(e); lastSent = e.id; }
  };
  feed.on("entry", onEntry);
  req.on("close", () => feed.off("entry", onEntry));

  // 2. replay anything the client missed (non-blocking range read on the normal connection)
  if (lastSent !== "$") {
    const missed = (await redis.xrange(STREAM, `(${lastSent}`, "+", "COUNT", 1000)) as RawEntry[];
    for (const [id, fields] of missed) {
      send({ id, data: fieldsToObject(fields) });
      lastSent = id;
    }
  }

  // 3. flush the buffer (skipping what the replay already sent), then go live
  for (const e of buffer) if (cmpId(e.id, lastSent) > 0) { send(e); lastSent = e.id; }
  replaying = false;
});
```

Why it works:

| Concern | How it is handled |
|---------|-------------------|
| Browser reconnects | `Last-Event-ID` is the stream ID of the last event it saw |
| Gap between replay and live | Live entries are buffered **during** the replay, then flushed |
| Duplicates (an entry seen in both) | `cmpId` skips anything not newer than `lastSent` |
| One Redis blocking connection regardless of client count | A single shared `LiveFeed` |
| Very long offline periods | Replay is capped (`COUNT 1000`) and limited by retention, so the client should do a full refresh if its ID is older than the stream's first entry |

For authenticated or per-user feeds, filter in `onEntry` by a field such as `userId`, or use **one stream per user or tenant** if volume warrants it.

## Graceful shutdown

```ts
async function shutdown() {
  await reader.stop();                    // flips the flag, waits up to one BLOCK interval
  await feed.stop();
  await blocker.quit();                   // then close the blocking connection
}
```

Use a short `BLOCK` (2 to 5 s) so shutdown isn't delayed ([Graceful Shutdown](../03_ioredis-basics/06_graceful-shutdown.md)).

## Choosing between a tailing reader and a consumer group

| Question | Tailing reader | Consumer group |
|----------|----------------|----------------|
| Should several workers **share** the work? | No | **Yes** |
| Must each entry be processed by **exactly one** worker? | No | **Yes** |
| Need acknowledgements and automatic redelivery? | No (manual checkpoint) | **Yes** |
| Every consumer should see **everything**? | **Yes** | One group per consumer |
| Rebuild state by replaying from `0`? | **Yes** | Possible (`XGROUP SETID`) |
| Strictly ordered single consumer? | **Yes** | One worker per stream (or partition) |

## Testing

- Use a **unique stream per test**, and `unlink` it afterwards
- Use **explicit IDs** (`1-0`, `2-0`) for deterministic assertions
- Test resume: process some entries, stop the reader, add more, start a new reader with the same name, and assert it continues from the checkpoint
- Test duplicate delivery by crashing the handler after side effects and before the checkpoint

```ts
it("resumes from its checkpoint", async () => {
  await redis.xadd(S, "1-0", "n", "1");
  await redis.xadd(S, "2-0", "n", "2");
  const seen: string[] = [];

  const r1 = new StreamReader(blocker, redis, async (e) => { seen.push(e.id); }, { stream: S, name: "t", startFrom: "0" });
  await r1.start(); await waitFor(() => seen.length === 2); await r1.stop();

  await redis.xadd(S, "3-0", "n", "3");
  const r2 = new StreamReader(blocker, redis, async (e) => { seen.push(e.id); }, { stream: S, name: "t", startFrom: "0" });
  await r2.start(); await waitFor(() => seen.length === 3); await r2.stop();

  expect(seen).toEqual(["1-0", "2-0", "3-0"]);
});
```

## Pitfalls

| Pitfall | Fix |
|---------|-----|
| Retrying `XADD` without a stable `msgId` | Include one, and deduplicate in consumers |
| `$` on every loop iteration | `$` once, then the last received ID |
| Reader and normal commands sharing one connection | A dedicated blocking connection |
| One blocking connection per browser client | One shared reader and in-memory fan-out |
| Checkpoint written before handling | Handle, then checkpoint |
| A poison entry stalls the reader forever | Catch per entry, or use a group with a DLQ |
| Checkpoint older than the stream's first entry | Alert, and do a full resync |
| Very long `BLOCK` timeouts | 2 to 5 seconds, so shutdown is quick |
| Producers ignoring failure policy | Decide: drop, outbox, or fail fast |

## Key takeaways

- Producers append with a **bounded** `XADD`, carry a stable `msgId`, and decide how to behave when Redis is down
- Independent readers track their own **last ID** and persist a checkpoint to resume
- A shared reader plus in-memory fan-out lets SSE (and similar) scale without one connection per client
- For shared work and acknowledgements, use consumer groups

**Next:** [Consumer Groups and Acks](./03_consumer-groups-and-acks.md)
