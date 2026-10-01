# Event-Driven Node.js

Pub/Sub becomes useful when it sits behind a small, well-typed **event bus** that your application code can use without caring whether a listener is in the same process or on another server. This lesson builds that bus and covers ordering, failure and design trade-offs.

## What events are good for

| Use | Why Pub/Sub fits |
|-----|------------------|
| Clearing in-process caches on every instance | Fan-out, and a TTL backstop covers misses |
| Pushing UI updates (feeds, presence, typing) | A missed update is replaced by the next |
| Reloading configuration and feature flags | A hint, with periodic refresh as a backstop |
| Decoupling modules inside a monolith | In-process delivery, no network |
| "Something changed, go check" wake-ups | The source of truth stays elsewhere |

What they are **not** good for: guaranteed work (emails, payments, billing). Pair them with a durable mechanism (below).

## Typed events

Define the vocabulary once:

```ts
// src/events/types.ts
export interface AppEvents {
  "user.created":   { userId: string; email: string };
  "user.updated":   { userId: string; fields: string[] };
  "order.paid":     { orderId: string; totalCents: number };
  "cache.invalidate": { key: string };
}

export interface Envelope<T = unknown> {
  id: string;
  type: string;
  v: number;
  ts: number;
  origin: string;        // which instance published it
  seq: number;           // per-origin sequence number, for gap detection
  payload: T;
}
```

## The event bus: local plus remote

One interface, two delivery paths. Listeners on the **same instance** are called directly. Listeners on **other instances** get the event via Redis:

```ts
// src/events/event-bus.ts
import { EventEmitter } from "node:events";
import { randomUUID } from "node:crypto";
import type { PubSub } from "../redis/pubsub.js";
import type { Envelope } from "./types.js";

type Logger = { error: (o: unknown, m?: string) => void; warn: (o: unknown, m?: string) => void };

export class EventBus<E extends Record<string, unknown>> {
  private local = new EventEmitter();
  private origin = randomUUID();
  private seq = 0;
  private lastSeen = new Map<string, number>();         // origin → last seq
  private stop?: () => Promise<void>;

  constructor(
    private pubsub: PubSub,
    private channel = "shop:events",
    private log: Logger = console,
    private onGap: (origin: string, from: number, to: number) => void = () => {}
  ) {
    this.local.setMaxListeners(100);
  }

  /** Begin receiving events published by OTHER instances. */
  async start() {
    this.stop = await this.pubsub.subscribe(this.channel, (raw) => {
      let env: Envelope;
      try { env = JSON.parse(raw); } catch { return this.log.warn({ raw: raw.slice(0, 100) }, "bad event dropped"); }

      if (env.origin === this.origin) return;           // we already delivered our own events locally
      this.detectGap(env);
      this.local.emit(env.type, env.payload, env);
    });
  }

  async close() { await this.stop?.(); }

  on<K extends keyof E & string>(
    type: K,
    handler: (payload: E[K], env: Envelope<E[K]>) => void | Promise<void>
  ): () => void {
    const wrapped = (payload: E[K], env: Envelope<E[K]>) => {
      Promise.resolve()
        .then(() => handler(payload, env))
        .catch((err) => this.log.error({ err, type }, "event handler failed"));   // isolate failures
    };
    this.local.on(type, wrapped);
    return () => this.local.off(type, wrapped);
  }

  async emit<K extends keyof E & string>(type: K, payload: E[K]): Promise<void> {
    const env: Envelope<E[K]> = {
      id: randomUUID(), type, v: 1, ts: Date.now(), origin: this.origin, seq: ++this.seq, payload,
    };

    this.local.emit(type, payload, env);                // same-process listeners, no network
    await this.pubsub.publish(this.channel, JSON.stringify(env));   // other instances
  }

  private detectGap(env: Envelope) {
    const last = this.lastSeen.get(env.origin);
    if (last !== undefined && env.seq > last + 1) this.onGap(env.origin, last + 1, env.seq - 1);
    this.lastSeen.set(env.origin, env.seq);
  }
}
```

Usage:

```ts
const bus = new EventBus<AppEvents>(pubsub, "shop:events", logger, (origin, from, to) => {
  logger.warn({ origin, from, to }, "missed events: resyncing");
  resyncFromSource();                                    // reload from the database or cache
});
await bus.start();

// listen (type-checked payload)
const off = bus.on("cache.invalidate", ({ key }) => l1.delete(key));
bus.on("user.created", async ({ userId }) => { await warmProfile(userId); });

// publish (type-checked)
await bus.emit("cache.invalidate", { key: "shop:cache:product:88:v3" });
await bus.emit("user.created", { userId: "42", email: "a@x.com" });   // ✗ compile error if a field is missing
```

Design points:

| Decision | Reason |
|----------|--------|
| Local listeners are called **directly** | No network hop, and the event still works if Redis is down |
| Remote events skip **our own `origin`** | Without that, each instance would handle its own events twice |
| Handlers are **isolated** | A throwing handler never breaks the others |
| `seq` per origin | Lets receivers **detect gaps** (a reconnect, a Redis restart) and resync |
| One shared channel | Simple. Use per-type channels or patterns if instances care about few types |

### One channel or many?

| | Single channel | One channel per type |
|---|----------------|----------------------|
| Subscriptions | 1 | One per type used |
| Every instance receives | **All** events | Only subscribed types |
| Good for | Few types, small volume | High volume, selective interest |

With a single channel, filtering happens in Node (cheap, but you still pay network and parsing for events you ignore).

## Ordering

- Messages from **one publisher connection** reach a subscriber **in order**
- Across different publishers there is **no global order**
- Your handlers are `async`, so two handlers for consecutive events can **overlap**. If order matters for one entity, serialize per key:

```ts
const tails = new Map<string, Promise<void>>();

function serialized(key: string, fn: () => Promise<void>) {
  const prev = tails.get(key) ?? Promise.resolve();
  const next = prev.then(fn, fn).finally(() => {
    if (tails.get(key) === next) tails.delete(key);          // don't leak entries
  });
  tails.set(key, next);
  return next;
}

bus.on("user.updated", ({ userId }) => serialized(userId, () => refreshProfile(userId)));
```

If you need strict, durable ordering, use a [Stream](../10_streams/README.md).

## Handling gaps and missed messages

Because delivery is at-most-once, design for **eventual repair**:

| Technique | How |
|-----------|-----|
| **TTL backstop** | Cached data expires on its own, so a missed invalidation is bounded (see [Cache Invalidation](../07_caching/03_cache-invalidation.md)) |
| **Gap detection** | The `seq` check above triggers a resync when something was lost |
| **Periodic refresh** | Reload config or flags every N seconds regardless of events |
| **Snapshot plus deltas** | New or reconnecting clients fetch current state from the source of truth, then apply live events |
| **Hint, not data** | Publish "product 88 changed", not the product. Receivers fetch the latest, so a lost or reordered hint is harmless |

The **hint** pattern is the most robust: events say *that* something changed, and the source of truth says *what* it is.

## Making important events durable

When an event **must** result in action, don't depend on Pub/Sub alone.

### Outbox plus relay

```
1. In ONE database transaction: write the data AND an "outbox" row
2. A relay reads unsent outbox rows and publishes them (to a Stream or queue)
3. Mark them sent after a successful publish
```

This closes the gap between "database committed" and "event published" (a crash between them otherwise loses the event). Publish to a **Stream** or **BullMQ** rather than Pub/Sub, so consumers can acknowledge and retry ([Streams](../10_streams/README.md), [Queues](../14_queues-and-workers/README.md)).

### Pub/Sub as the wake-up signal for durable work

Keep the work in a durable place and use Pub/Sub only to cut latency:

```ts
// producer: durable first, signal second
await redis.xadd("shop:jobs", "*", "type", "thumbnail", "imageId", id);
await redis.publish("shop:jobs:wake", "1");

// worker: process on signal, and ALSO on a timer, so a missed signal only delays
bus.on("jobs.wake", () => drain());
setInterval(drain, 30_000);
```

A lost message costs at most 30 seconds, never a job.

## Request and response over Pub/Sub

Possible, rarely wise:

```ts
async function request<T>(channel: string, body: unknown, timeoutMs = 2000): Promise<T> {
  const id = randomUUID();
  const replyChannel = `shop:reply:${id}`;

  return new Promise<T>(async (resolve, reject) => {
    const timer = setTimeout(() => { off().catch(() => {}); reject(new Error("timeout")); }, timeoutMs);
    const off = await pubsub.subscribe(replyChannel, (raw) => {
      clearTimeout(timer);
      off().catch(() => {});
      resolve(JSON.parse(raw));
    });
    await pubsub.publish(channel, JSON.stringify({ id, replyChannel, body }));
  });
}
```

Problems: nobody may be listening (instant silent failure until the timeout), several responders may all answer, and no retry or backpressure. Prefer an HTTP call, gRPC, or a queue with a reply stream.

## Graceful shutdown

```ts
async function shutdown() {
  await bus.close();            // unsubscribe from the shared channel
  await pubsub.close();         // unsubscribe everything
  await connections.close();    // then quit the Redis connections (subscribers first)
}
```

Order: stop **receiving** first, let in-flight handlers finish, then close connections ([Graceful Shutdown](../03_ioredis-basics/06_graceful-shutdown.md)).

## Observability

Track:

- Events **published** and **received** per type (a persistent gap between instances signals loss)
- **Handler failures** per type
- **Gap detections** (each is a reconnect or loss event)
- Publish **receiver count** of `0` where you expected listeners
- Approximate **latency**: `Date.now() - env.ts` (only meaningful if clocks are synchronized, so treat it as a trend)

Log the envelope `id`, `type` and `origin`, not whole payloads.

## Testing event-driven code

**Unit tests**: use an in-memory bus with the same interface:

```ts
export class InMemoryEventBus<E extends Record<string, unknown>> {
  private em = new EventEmitter();
  on<K extends keyof E & string>(type: K, h: (p: E[K]) => void | Promise<void>) { this.em.on(type, h); return () => this.em.off(type, h); }
  async emit<K extends keyof E & string>(type: K, payload: E[K]) { this.em.emit(type, payload); }
  emitted: { type: string; payload: unknown }[] = [];
}
```

**Integration tests**: two `EventBus` instances over one real Redis, asserting cross-instance delivery:

```ts
it("delivers events to another instance, and not twice to the sender", async () => {
  const a = new EventBus<AppEvents>(pubsubA);
  const b = new EventBus<AppEvents>(pubsubB);
  await a.start(); await b.start();

  const onA = vi.fn(); const onB = vi.fn();
  a.on("cache.invalidate", onA);
  b.on("cache.invalidate", onB);

  await a.emit("cache.invalidate", { key: "k" });
  await waitFor(() => onB.mock.calls.length === 1);

  expect(onA).toHaveBeenCalledTimes(1);          // local delivery only
  expect(onB).toHaveBeenCalledTimes(1);          // remote delivery
});
```

Also test **resilience**: kill the subscriber connection mid-test (`sub.disconnect(true)`), publish, reconnect, and verify the gap is detected.

## Choosing the mechanism

| Need | Use |
|------|-----|
| Notify listeners **in the same process** | `EventEmitter` |
| Notify **all instances**, loss is acceptable | Redis Pub/Sub (this bus) |
| Notify **all instances**, must not lose | Streams (each instance as its own consumer) |
| **One worker** per job, retries, delays | BullMQ or Streams consumer groups |
| Replay and audit history | Streams, or Kafka at larger scale |
| Cross-service contracts at scale | A real broker (Kafka, RabbitMQ, SNS/SQS) |

## Pitfalls

| Pitfall | Fix |
|---------|-----|
| Instance handles its own published event twice | Skip your own `origin` on the remote path |
| Throwing handler stops other listeners | Wrap each handler |
| Overlapping async handlers breaking per-entity order | Serialize per key |
| Treating Pub/Sub as a job queue | Durable store plus a wake-up signal |
| Publishing after commit with no safety net | Outbox plus a Stream or queue |
| Putting full objects in events | Publish hints, fetch the latest |
| No gap detection or periodic refresh | `seq` numbers, TTLs, refresh timers |
| Unbounded `EventEmitter` listeners (leak) | Return and call the unsubscribe function |
| Request/response over Pub/Sub | HTTP, gRPC or a queue |

## Key takeaways

- A typed bus hides local versus remote delivery and keeps handlers isolated
- Skip your own origin, number your events, and **design for loss** with TTLs, refresh and resync
- Publish **hints**, and keep the truth in a durable store
- Use Streams or queues for anything that must not be lost

**Next:** [Socket.IO with Redis](./03_socketio-with-redis.md)
