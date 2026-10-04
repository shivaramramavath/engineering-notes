# Event-Driven System

In an event-driven system, services announce **what happened** ("order created") and other services react on their own. The producer does not know who is listening, so new consumers can be added without changing it. Redis Streams provide the backbone: a durable, ordered log with **consumer groups**, acknowledgements and the ability to reclaim work from crashed consumers.

```
 Order service ──XADD──► stream "events:orders" ──┬─ group "inventory"      ► consumer A, consumer B (share the work)
                          (append-only log)       ├─ group "payments"       ► consumer C
                                                  ├─ group "notifications"  ► consumer D
                                                  └─ group "analytics"      ► consumer E
                                                        │ failed too many times
                                                        ▼
                                                 stream "events:orders:dlq"
```

Every **group** receives every event. Inside a group, each event goes to **one** consumer, so you scale a service by adding consumers.

## The problem

| Tight coupling | Event-driven |
|----------------|--------------|
| Order service calls inventory, payments, email and analytics directly | Order service publishes one event |
| One slow or failing dependency fails the whole request | Consumers fail and retry independently |
| Adding a consumer means changing and redeploying the producer | New group, no producer change |
| Hard to replay or audit | The log is the history |

## Choosing the right Redis tool

| Need | Pub/Sub | Lists / BullMQ | **Streams** |
|------|:-------:|:--------------:|:-----------:|
| Fire-and-forget live broadcast | Yes | No | Overkill |
| Messages survive a consumer being offline | No | Yes | Yes |
| Several independent services each get every event | Yes (live only) | No (one queue per consumer) | **Yes** (consumer groups) |
| Work shared among workers with acknowledgements | No | Yes | Yes |
| Retries, delays, rate limits, job UI | No | **Yes** (BullMQ) | Build it yourself |
| Replay history | No | No | **Yes** |
| Per-event ordering | n/a | Per queue | Per stream |

Rules of thumb: **Pub/Sub** for ephemeral, real-time fan-out (cache invalidation, live updates). **BullMQ** for background jobs with retries and delays. **Streams** for domain events that several services consume reliably. If you need very high volume, long retention (weeks or more) or huge partition counts, a dedicated log system is a better fit than Redis. See [Stream Patterns](../10_streams/04_stream-patterns.md).

## Example domain: order processing

| Event | Published by | Reacted to by |
|-------|--------------|---------------|
| `order.created` | Order service | Inventory (reserve stock), Payments (charge), Analytics |
| `payment.succeeded` | Payments | Order service (mark paid), Notifications (receipt) |
| `payment.failed` | Payments | Order service (cancel), Inventory (release), Notifications |
| `order.shipped` | Fulfillment | Notifications, Analytics |

Each service owns its own steps and reacts to events from others. Compensation (release stock, refund) is itself an event. This choreography style avoids a central coordinator, at the cost of needing good monitoring to follow a flow end to end.

## Event design

A consistent envelope makes events routable, traceable and evolvable.

```ts
export interface DomainEvent<T = unknown> {
  id: string;           // unique, used for deduplication
  type: string;         // "order.created"
  v: number;            // schema version of `data`
  ts: number;           // when it happened (ms)
  corr?: string;        // correlation ID, follows a business flow across services
  data: T;
}

export interface OrderCreated {
  orderId: string;
  customerId: string;
  items: { sku: string; qty: number }[];
  totalCents: number;
}
```

Guidelines:

- Name events in the **past tense** (`order.created`): they are facts, not commands
- Include what consumers need (IDs and key facts), not a pointer they must look up in the producer's database. Do not include secrets or unneeded personal data
- Never change the meaning of an existing field. Add new optional fields and bump `v` for breaking changes
- Consumers **ignore unknown fields and unknown event types**

## Publishing

```ts
import { randomUUID } from "node:crypto";
import Redis from "ioredis";

const RETENTION_MS = 7 * 86_400_000;       // keep a week of history

export async function publish<T>(
  redis: Redis,
  stream: string,
  e: { type: string; data: T; v?: number; corr?: string },
): Promise<string> {
  const event: DomainEvent<T> = { id: randomUUID(), ts: Date.now(), v: e.v ?? 1, type: e.type, corr: e.corr, data: e.data };

  await redis.xadd(
    stream,
    "MINID", "~", `${Date.now() - RETENTION_MS}-0`,       // time-based trimming, approximate and cheap
    "*",
    "id", event.id, "type", event.type, "v", String(event.v), "ts", String(event.ts),
    "corr", event.corr ?? "", "data", JSON.stringify(event.data),
  );
  return event.id;
}

await publish(redis, "events:orders", { type: "order.created", data: order, corr: requestId });
```

### Retention versus slow consumers

Trimming deletes entries whether or not every group has processed them. Choose retention **longer than your worst consumer outage**, and alert on consumer lag long before it approaches the retention window. `MAXLEN ~ N` limits by count and `MINID ~ ts` limits by age, and age is usually easier to reason about.

### The publish-after-commit problem

```
BEGIN; INSERT order; COMMIT;  ──►  XADD event  ──► ⚡ crash here: order saved, event never published
```

Publishing inside the request is **two writes to two systems**, and one can succeed without the other. The standard fix is the **transactional outbox**: write the event to an `outbox` table in the same database transaction as the business change, then a relay publishes it.

```ts
// 1) in the business transaction
await db.tx(async (tx) => {
  await tx.query("INSERT INTO orders (id, ...) VALUES ($1, ...)", [order.id]);
  await tx.query("INSERT INTO outbox (id, type, payload) VALUES ($1, $2, $3)", [randomUUID(), "order.created", order]);
});

// 2) relay loop (one or more instances)
async function relayOnce() {
  await db.tx(async (tx) => {
    const { rows } = await tx.query(
      "SELECT id, type, payload FROM outbox WHERE published_at IS NULL ORDER BY seq LIMIT 100 FOR UPDATE SKIP LOCKED",
    );
    for (const r of rows) {
      await publish(redis, "events:orders", { type: r.type, data: r.payload });
      await tx.query("UPDATE outbox SET published_at = now() WHERE id = $1", [r.id]);
    }
  });
}
```

If the relay crashes after `XADD` but before the update, the event is published again. That is fine because **consumers are idempotent**. The outbox converts "maybe lost" into "maybe duplicated", which is much easier to handle.

## Consuming: a reusable consumer

```ts
export interface ConsumerOptions {
  stream: string;
  group: string;
  consumer: string;               // unique per process: `${hostname}-${pid}`
  startId?: string;               // "$" = only new events, "0" = replay history (first creation only)
  batch?: number;
  blockMs?: number;
  claimIdleMs?: number;           // reclaim messages idle this long
  maxDeliveries?: number;         // then dead-letter
  dlqStream?: string;
  dedupTtlSec?: number;
}

type Handler = (event: DomainEvent) => Promise<void>;

const toObject = (flat: string[]) => {
  const o: Record<string, string> = {};
  for (let i = 0; i < flat.length; i += 2) o[flat[i]] = flat[i + 1];
  return o;
};

export class StreamConsumer {
  private o: Required<ConsumerOptions>;
  private reader: Redis;                        // dedicated connection: XREADGROUP BLOCK holds it
  private running = false;
  private loop?: Promise<void>;
  private reclaimTimer?: NodeJS.Timeout;

  constructor(private redis: Redis, opts: ConsumerOptions, private handlers: Record<string, Handler>) {
    this.o = {
      startId: "$", batch: 10, blockMs: 5_000, claimIdleMs: 60_000, maxDeliveries: 5,
      dlqStream: `${opts.stream}:dlq`, dedupTtlSec: 7 * 86_400, ...opts,
    };
    this.reader = redis.duplicate();
  }

  async start() {
    try {
      await this.redis.xgroup("CREATE", this.o.stream, this.o.group, this.o.startId, "MKSTREAM");
    } catch (e) {
      if (!String((e as Error).message).includes("BUSYGROUP")) throw e;    // group already exists: fine
    }
    this.running = true;
    this.loop = this.readLoop();
    this.reclaimTimer = setInterval(() => this.reclaim().catch(console.error), this.o.claimIdleMs / 2);
    this.reclaimTimer.unref();
  }

  async stop() {
    this.running = false;
    if (this.reclaimTimer) clearInterval(this.reclaimTimer);
    this.reader.disconnect();                                              // unblocks the pending read
    await this.loop?.catch(() => {});                                      // lets an in-flight handler finish
  }

  private async readLoop() {
    while (this.running) {
      try {
        const res = (await this.reader.xreadgroup(
          "GROUP", this.o.group, this.o.consumer,
          "COUNT", this.o.batch, "BLOCK", this.o.blockMs,
          "STREAMS", this.o.stream, ">",
        )) as [string, [string, string[]][]][] | null;

        for (const [, entries] of res ?? []) {
          for (const [id, fields] of entries) await this.process(id, fields);
        }
      } catch (err) {
        if (!this.running) return;
        console.error("consumer read error", err);
        await new Promise((r) => setTimeout(r, 1_000));                    // back off, then try again
      }
    }
  }

  /** Take over messages that were delivered but never acknowledged (crashed or stuck consumers). */
  private async reclaim() {
    let cursor = "0-0";
    do {
      const [next, entries] = (await this.redis.xautoclaim(
        this.o.stream, this.o.group, this.o.consumer, this.o.claimIdleMs, cursor, "COUNT", 50,
      )) as [string, [string, string[]][]];
      for (const [id, fields] of entries) await this.process(id, fields);
      cursor = next;
    } while (cursor !== "0-0" && this.running);
  }

  private async process(id: string, fields: string[]) {
    let event: DomainEvent;
    try {
      const raw = toObject(fields);
      event = { id: raw.id, type: raw.type, v: Number(raw.v), ts: Number(raw.ts), corr: raw.corr || undefined, data: JSON.parse(raw.data) };
    } catch {
      return this.deadLetter(id, fields, "unparseable event");              // poison message: do not retry forever
    }

    const handler = this.handlers[event.type];
    if (!handler) return void (await this.redis.xack(this.o.stream, this.o.group, id));   // not for us

    // how many times has this entry been delivered? (1 on the first attempt)
    const pending = (await this.redis.xpending(this.o.stream, this.o.group, id, id, 1)) as [string, string, number, number][];
    const deliveries = pending[0]?.[3] ?? 1;

    try {
      await this.once(event, handler);
      await this.redis.xack(this.o.stream, this.o.group, id);
    } catch (err) {
      if (deliveries >= this.o.maxDeliveries) {
        await this.deadLetter(id, fields, `failed ${deliveries} times: ${(err as Error).message}`);
      }
      // otherwise leave it pending: reclaim() will redeliver after claimIdleMs
    }
  }

  /** Skip events this group has already handled (redelivery after a crash, outbox republish). */
  private async once(event: DomainEvent, handler: Handler) {
    const doneKey = `evt:done:${this.o.group}:${event.id}`;
    if (await this.redis.exists(doneKey)) return;
    await handler(event);
    await this.redis.set(doneKey, "1", "EX", this.o.dedupTtlSec);
  }

  private async deadLetter(id: string, fields: string[], reason: string) {
    await this.redis.multi()
      .xadd(this.o.dlqStream, "*", ...fields, "origId", id, "group", this.o.group, "error", reason.slice(0, 500))
      .xack(this.o.stream, this.o.group, id)
      .exec();
  }
}
```

### Wiring up services

```ts
import os from "node:os";

const consumerName = `${os.hostname()}-${process.pid}`;

const inventory = new StreamConsumer(redis,
  { stream: "events:orders", group: "inventory", consumer: consumerName },
  {
    "order.created": async (e) => reserveStock((e.data as OrderCreated).items, e.id),
    "payment.failed": async (e) => releaseStock((e.data as { orderId: string }).orderId),
  },
);

const notifications = new StreamConsumer(redis,
  { stream: "events:orders", group: "notifications", consumer: consumerName },
  { "order.shipped": async (e) => notifyShipped(e.data as { orderId: string }) },
);

await inventory.start();
await notifications.start();

process.on("SIGTERM", async () => {
  await Promise.all([inventory.stop(), notifications.stop()]);
  await redis.quit();
});
```

To scale a service, run more processes with the **same group** and different consumer names. Redis spreads new events among them. To add a new service, create a **new group**. With `startId: "0"` it first processes the entire retained history, with `"$"` only events from now on.

## Delivery semantics: be honest about them

| Guarantee | Reality with Streams |
|-----------|----------------------|
| Delivered at least once | Yes, if consumers acknowledge only after success and reclaim stuck work |
| Delivered exactly once | **No.** Crashes, reclaims and republished outbox rows cause repeats |
| Ordered | Entries are ordered in the stream. Consumers in a group process in parallel, so completion order can differ |
| Not lost | Within retention, and as durable as your persistence and replication settings (see [Backup and Disaster Recovery](../19_redis-production/03_backup-and-disaster-recovery.md)) |

So the working model is **at-least-once delivery plus idempotent handlers**. The `once` wrapper skips events a group already handled, but a crash between "handler finished" and "marker written" can still run the handler twice. Make the side effect itself safe to repeat:

```ts
async function reserveStock(items: { sku: string; qty: number }[], eventId: string) {
  // unique constraint on (event_id) in the reservations table makes a repeat a no-op
  await db.query("INSERT INTO reservations (event_id, ...) VALUES ($1, ...) ON CONFLICT (event_id) DO NOTHING", [eventId]);
}
```

See [Idempotency](./01_idempotency.md) and [Request Deduplication](./02_request-deduplication.md).

## Ordering

A single stream preserves **append order**, but a group with several consumers handles different entries concurrently, and retries reorder further. If events for one entity must be processed in order (for example `order.created` before `order.cancelled` for the same order):

| Approach | How |
|----------|-----|
| One consumer per group | Simple, limits throughput |
| **Shard streams by entity** | `events:orders:{hash(orderId) % N}`, one consumer per shard, so each order's events stay in one lane |
| Make handlers order-tolerant | Include a version or timestamp, ignore stale events, upsert state |

```ts
import { createHash } from "node:crypto";

const SHARDS = 8;
const streamFor = (orderId: string) =>
  `events:orders:${createHash("md5").update(orderId).digest().readUInt32BE(0) % SHARDS}`;
```

In a Redis Cluster, different shard streams live on different slots, which also spreads load. Consumers subscribe to all shard streams (one reader per shard). Order-tolerant handlers are the most robust option, because they also survive retries and replays.

## Failure handling

| Problem | Mechanism |
|---------|-----------|
| Handler throws | Entry stays pending, retried after `claimIdleMs` |
| Consumer crashes mid-processing | Another consumer `XAUTOCLAIM`s the entry after it has been idle long enough |
| Poison message (always fails) | After `maxDeliveries` it is copied to the dead-letter stream and acknowledged |
| Dependency down for minutes | Retries continue. Use longer idle times or backoff to avoid hammering it |
| Bad deploy processed events wrongly | Fix the code, replay from the stream with `XGROUP SETID` (handlers must be idempotent) |
| Consumer removed for good | Reclaim its pending entries, then `XGROUP DELCONSUMER` |

Inspect and handle dead letters deliberately:

```ts
const dead = await redis.xrange("events:orders:dlq", "-", "+", "COUNT", 20);
// each entry has the original fields plus origId, group and error
// after fixing the cause, republish with publish() and delete the DLQ entry with XDEL
```

Alert on any non-zero dead-letter growth, since each entry is an unprocessed business event.

### Replay

```bash
# reprocess everything a group has already seen (idempotent handlers required)
redis-cli XGROUP SETID events:orders inventory 0

# or from a point in time (stream IDs start with a millisecond timestamp)
redis-cli XGROUP SETID events:orders inventory 1759500000000-0
```

Replay is one of the biggest advantages of a log, and also the reason idempotent consumers are not optional.

## Observability

```ts
export async function groupHealth(redis: Redis, stream: string) {
  const groups = (await redis.xinfo("GROUPS", stream)) as unknown[][];
  return groups.map((g) => {
    const o: Record<string, unknown> = {};
    for (let i = 0; i < g.length; i += 2) o[String(g[i])] = g[i + 1];
    return {
      group: o.name,
      consumers: o.consumers,
      pending: o.pending,              // delivered but not yet acknowledged
      lag: o.lag,                      // entries not yet delivered to the group (Redis 7.0+, can be null)
      lastDelivered: o["last-delivered-id"],
    };
  });
}
```

Track and alert on:

| Metric | Why |
|--------|-----|
| Lag per group | Falling behind: add consumers or fix a slow handler |
| Pending count and oldest pending age (`XPENDING`) | Stuck or crashing handlers |
| Dead-letter stream length | Unprocessed events |
| Handler duration and error rate by event type | Slow or failing logic |
| Stream length versus retention | Trimming might drop unprocessed events |
| Consumers per group (`XINFO CONSUMERS`) | Capacity and crashed workers |
| End-to-end latency (`now - event.ts` at handling time) | User-visible delay |

Carry the `corr` ID through every log line and event so a single order can be traced across services. See [Monitoring and Logging](../19_redis-production/02_monitoring-and-logging.md).

## Schema evolution

| Change | Safe? | How |
|--------|-------|-----|
| Add an optional field | Yes | Consumers ignore unknown fields |
| Remove or rename a field | No | Publish a new version (`v: 2`) and support both until consumers migrate |
| Change a field's meaning or type | No | New event type or version |
| New event type | Yes | Consumers without a handler ignore it |

```ts
const handlers = {
  "order.created": async (e: DomainEvent) => {
    const data = e.v === 1 ? upgradeV1ToV2(e.data as OrderCreatedV1) : (e.data as OrderCreated);
    await handle(data);
  },
};
```

Keep a short contract document per event type, and test consumers against recorded sample events from older versions.

## Testing

```ts
const stream = () => `${t.prefix}events`;   // use unique stream and group names per test

it("delivers an event to every group, once per group", async () => {
  const seenA: string[] = [], seenB: string[] = [];
  const a = new StreamConsumer(t.redis, { stream: stream(), group: "a", consumer: "a1", startId: "0", blockMs: 200 }, { "x.happened": async (e) => void seenA.push(e.id) });
  const b = new StreamConsumer(t.redis, { stream: stream(), group: "b", consumer: "b1", startId: "0", blockMs: 200 }, { "x.happened": async (e) => void seenB.push(e.id) });
  await a.start(); await b.start();

  const id = await publish(t.redis, stream(), { type: "x.happened", data: {} });
  await waitFor(async () => seenA.length + seenB.length, (n) => n === 2);

  expect(seenA).toEqual([id]); expect(seenB).toEqual([id]);
  await a.stop(); await b.stop();
});

it("splits work among consumers of the same group without overlap", async () => {
  // start two consumers in one group, publish 50 events, assert the union is 50 and the intersection is empty
});

it("retries a failing handler and then dead-letters it", async () => {
  let attempts = 0;
  const c = new StreamConsumer(t.redis,
    { stream: stream(), group: "g", consumer: "c1", startId: "0", blockMs: 100, claimIdleMs: 200, maxDeliveries: 3 },
    { "bad.event": async () => { attempts++; throw new Error("boom"); } });
  await c.start();
  await publish(t.redis, stream(), { type: "bad.event", data: {} });

  await waitFor(() => t.redis.xlen(`${stream()}:dlq`), (n) => n === 1, 10_000);
  expect(attempts).toBeGreaterThanOrEqual(3);
  await c.stop();
});

it("does not run a handler twice for the same event ID", async () => {
  // publish the same entry twice (same id field) and assert the handler ran once
});

it("reclaims messages from a consumer that died without acknowledging", async () => {
  // read with XREADGROUP as "dead-consumer", never XACK, then start a live consumer and
  // assert it processes the entry after claimIdleMs
});
```

Also test: unknown event types are acknowledged and ignored, an unparseable entry goes straight to the dead-letter stream, and `stop()` waits for the in-flight handler. Use short `blockMs` and `claimIdleMs` in tests so they run fast, and always stop consumers in `afterEach`. Helpers are in [Testing with Redis](../18_testing-and-debugging/01_testing-with-redis.md).

## Choosing between group start positions

| `startId` | Meaning | Use when |
|-----------|---------|----------|
| `"$"` | Only events added after the group is created | A new service that only cares about the future |
| `"0"` | From the beginning of the retained stream | Backfilling a new read model, or reprocessing |
| An ID | From that point | Resume after a known incident |

`startId` only matters when the group is **created**. An existing group keeps its position, so use `XGROUP SETID` to move it.

## Pitfalls

| Pitfall | Fix |
|---------|-----|
| Using Pub/Sub for events that must not be lost | Streams with consumer groups |
| Reading the stream without a group when work must be shared | `XREADGROUP`, so each entry goes to one consumer in the group |
| Acknowledging before the work succeeds | `XACK` only after the handler finishes |
| No reclaim of idle pending entries | Periodic `XAUTOCLAIM` |
| Retrying poison messages forever | Count deliveries and dead-letter after a limit |
| Blocking reads on the shared client | `redis.duplicate()` for the reader |
| Publishing after the DB commit with no outbox | Transactional outbox plus an idempotent relay |
| Assuming exactly-once | Design for at-least-once with idempotent handlers |
| Non-unique consumer names across processes | Hostname plus PID (or a UUID) |
| Unbounded stream growth | `MINID ~` or `MAXLEN ~` on `XADD` |
| Retention shorter than a consumer outage | Size it from worst-case downtime, and alert on lag |
| Assuming global ordering across consumers | Shard by entity, or write order-tolerant handlers |
| Breaking schema changes without versions | Additive changes, `v` field, support old and new |
| Ignoring the dead-letter stream | Alert on growth, and have a replay procedure |
| Stuffing huge payloads into entries | Store a reference (ID, object key) and keep events small |
| Ending without a graceful `stop()` | Stop reading, finish in-flight work, then quit |

## Key takeaways

- Streams with consumer groups give durable, replayable events where every service gets its own copy and workers within a service share the load
- Use a consistent event envelope with an ID, type, version and correlation ID, and evolve schemas additively
- Publish reliably with a transactional outbox, and expect occasional duplicates
- Acknowledge after success, reclaim idle pending entries, count deliveries and dead-letter poison messages
- Delivery is at-least-once, so make consumers idempotent and handle ordering through sharding or order-tolerant logic
- Watch lag, pending entries, dead letters and retention, and practice replaying events

**Previous:** [Feature Flags](./05_feature-flags.md) | **Next:** [Projects](../21_projects/README.md)
