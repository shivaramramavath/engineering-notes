# Pub/Sub Fundamentals

## The model

| Role | Does |
|------|------|
| **Publisher** | Sends a message to a named channel with `PUBLISH` |
| **Subscriber** | Registers interest with `SUBSCRIBE` (exact channel) or `PSUBSCRIBE` (glob pattern) and receives messages as they arrive |
| **Channel** | Just a name. It is not created, stored or configured. It exists while someone is subscribed |

Key properties:

- **At-most-once delivery.** Each subscriber gets a message zero or one times, never more (barring the duplicate-subscription case below)
- **No persistence.** Messages are not stored. Late subscribers see nothing from the past
- **No acknowledgement.** The publisher learns only *how many* clients received it
- **Global namespace.** Channels are unrelated to databases: a publish on DB 0 reaches subscribers on DB 5
- **Ordering.** Messages from one publisher connection arrive at a subscriber in order

## Commands

| Command | Purpose |
|---------|---------|
| `PUBLISH channel message` | Send. Returns the number of receivers |
| `SUBSCRIBE channel [channel ...]` | Listen on exact channels |
| `PSUBSCRIBE pattern [pattern ...]` | Listen on glob patterns |
| `UNSUBSCRIBE` / `PUNSUBSCRIBE` | Stop listening |
| `PUBSUB CHANNELS [pattern]` | Active channels with at least one subscriber |
| `PUBSUB NUMSUB channel ...` | Subscriber counts per channel |
| `PUBSUB NUMPAT` | Number of active pattern subscriptions |
| `SPUBLISH` / `SSUBSCRIBE` | Sharded Pub/Sub (Redis 7.0+, for Cluster) |

## Your first publisher and subscriber

A subscribing connection **can only run subscription commands** (plus `PING` and `QUIT`), so use two clients:

```ts
import { Redis } from "ioredis";

const pub = new Redis();
const sub = new Redis();                         // dedicated to subscribing

sub.on("message", (channel, message) => {
  console.log(`[${channel}]`, message);
});

const count = await sub.subscribe("news", "alerts");   // resolves once Redis confirms
console.log("subscribed to", count, "channels");

const receivers = await pub.publish("news", "hello");
console.log("delivered to", receivers);                // 1
```

Notes:

- `await sub.subscribe(...)` resolves **after** the server confirms, so publishing afterwards can't race ahead of the subscription
- The `message` handler is registered **once per client**, not per `subscribe` call (see the pitfall below)
- Messages are strings. Use `messageBuffer` for binary payloads

## Patterns

`PSUBSCRIBE` matches channel names with glob syntax (`*`, `?`, `[abc]`):

```ts
await sub.psubscribe("shop:events:*");

sub.on("pmessage", (pattern, channel, message) => {
  console.log(pattern, channel, message);
  // "shop:events:*"  "shop:events:order.created"  "{...}"
});

await pub.publish("shop:events:order.created", JSON.stringify({ id: 1 }));
```

Things to know:

- Pattern matches arrive on **`pmessage`** (three arguments), exact matches on **`message`** (two)
- A client subscribed to both `news` and `n*` receives a message published to `news` **twice** (once per subscription). That is the one way to see duplicates
- Redis checks every active pattern on every publish, so cost grows with the **total number of patterns**. A few patterns are fine. Thousands of per-user patterns are not. Prefer exact channels where possible

### Channel naming

Use the same discipline as keys ([Key Design](../05_key-management/01_key-design.md)):

```
shop:events:order.created
shop:events:user.updated
shop:chat:room:42
shop:cache:invalidate
```

Build them with your [key builder](../08_nodejs-integration/05_redis-key-builder.md), since `keyPrefix` is **not** applied to Pub/Sub channels.

## What can go wrong

| Situation | Result |
|-----------|--------|
| Nobody is subscribed | Message dropped. `PUBLISH` returns `0` |
| Subscriber is disconnected for 2 seconds | Messages in that window are **lost**. After reconnect ioredis resubscribes (`autoResubscribe`), but it can't recover missed messages |
| Redis restarts | All subscriptions end. ioredis resubscribes on reconnect. Messages during the gap are lost |
| Subscriber's handler is slow | Redis buffers output for that client, then **disconnects it** if it exceeds the limit |
| Handler throws | Nothing retries it. The message is gone |
| Publisher crashes after the DB commit but before `PUBLISH` | The event is never sent |

### Slow subscribers and output limits

Redis protects itself from clients that can't keep up. The default is:

```
client-output-buffer-limit pubsub 32mb 8mb 60
```

Meaning: disconnect a Pub/Sub client when its pending output exceeds 32 MB, or stays above 8 MB for 60 seconds. A subscriber that does heavy work **inside** the `message` callback (and so reads slowly) can hit this. Keep handlers quick, and hand work to a queue.

### Detecting that nobody listened

```ts
const receivers = await pub.publish("shop:chat:room:42", payload);
if (receivers === 0) {
  // no live listeners right now. Fine for presence, a problem for anything important
}
```

`receivers` is a hint, not a delivery guarantee. In Cluster it reflects only subscribers connected to the node you published through.

## A robust subscriber wrapper

Raw `sub.on("message")` handlers invite bugs: registering twice duplicates handling, one throwing handler can break others, and channels need reference counting. Wrap it once:

```ts
// src/redis/pubsub.ts
import type { Redis } from "ioredis";

type Handler = (message: string, channel: string) => void | Promise<void>;
type PatternHandler = (message: string, channel: string, pattern: string) => void | Promise<void>;
type Logger = { error: (obj: unknown, msg?: string) => void };

export class PubSub {
  private channels = new Map<string, Set<Handler>>();
  private patterns = new Map<string, Set<PatternHandler>>();

  constructor(private pub: Redis, private sub: Redis, private log: Logger = console) {
    // registered exactly once
    sub.on("message", (channel, message) => {
      for (const h of this.channels.get(channel) ?? []) this.run(() => h(message, channel), channel);
    });
    sub.on("pmessage", (pattern, channel, message) => {
      for (const h of this.patterns.get(pattern) ?? []) this.run(() => h(message, channel, pattern), channel);
    });
  }

  publish(channel: string, message: string): Promise<number> {
    return this.pub.publish(channel, message);
  }

  /** Returns an unsubscribe function. The Redis subscription is reference counted. */
  async subscribe(channel: string, handler: Handler): Promise<() => Promise<void>> {
    let set = this.channels.get(channel);
    if (!set) {
      set = new Set();
      this.channels.set(channel, set);
      await this.sub.subscribe(channel);
    }
    set.add(handler);

    return async () => {
      set!.delete(handler);
      if (set!.size === 0) {
        this.channels.delete(channel);
        await this.sub.unsubscribe(channel);
      }
    };
  }

  async psubscribe(pattern: string, handler: PatternHandler): Promise<() => Promise<void>> {
    let set = this.patterns.get(pattern);
    if (!set) {
      set = new Set();
      this.patterns.set(pattern, set);
      await this.sub.psubscribe(pattern);
    }
    set.add(handler);

    return async () => {
      set!.delete(handler);
      if (set!.size === 0) {
        this.patterns.delete(pattern);
        await this.sub.punsubscribe(pattern);
      }
    };
  }

  async close() {
    await this.sub.unsubscribe().catch(() => {});
    await this.sub.punsubscribe().catch(() => {});
  }

  // one failing handler must not affect the others, and async errors must not go unhandled
  private run(fn: () => void | Promise<void>, ctx: string) {
    Promise.resolve()
      .then(fn)
      .catch((err) => this.log.error({ err, channel: ctx }, "pubsub handler failed"));
  }
}
```

Wire it in your container with two connections ([connection management](../08_nodejs-integration/01_connection-management.md)):

```ts
const pub = connections.create("pubsub-pub");
const sub = connections.create("pubsub-sub", "subscriber");
export const pubsub = new PubSub(pub, sub, logger);
```

Use:

```ts
const off = await pubsub.subscribe("shop:cache:invalidate", (key) => l1.delete(key));
// later
await off();
```

## Message design

Pub/Sub carries strings, so agree on a shape. A small **envelope** pays for itself:

```ts
interface Envelope<T = unknown> {
  id: string;        // unique id (for deduplication and tracing)
  type: string;      // "order.created"
  v: number;         // schema version
  ts: number;        // publish time in ms
  payload: T;
}
```

Guidelines:

- **Keep messages small.** Send IDs and let receivers fetch details if needed
- **Make handlers idempotent.** Even at-most-once systems see replays when you add retries elsewhere
- **Version the schema** (`v`) so rolling deploys can run old and new consumers together
- **Validate on receive** with a schema ([Typed Redis Client](../08_nodejs-integration/04_typed-redis-client.md)), and drop bad messages with a log line
- **Don't put secrets in channels.** Subscribers see everything on that channel, and ACLs can restrict channels (`&pattern`) but the data is plain text

## Pub/Sub vs the alternatives

| | Pub/Sub | Streams | List queue / BullMQ | Node `EventEmitter` |
|---|---------|---------|---------------------|---------------------|
| Persistence | None | Yes | Yes | None |
| Multiple independent consumers | Yes, all get everything | Yes (groups or independent) | Competing consumers | Yes |
| Work sharing | No | Yes (consumer groups) | Yes | No |
| Replay and catch-up | No | Yes | No | No |
| Acknowledgement and retry | No | Yes | Yes | No |
| Crosses processes | Yes | Yes | Yes | **No** |
| Overhead | Lowest | Low | Low to medium | None |

Rule of thumb: **if losing a message is acceptable, use Pub/Sub. If not, use Streams or a queue.**

## Fan-out, not load balancing

If three app instances each subscribe to `shop:events:order.created`, **each** receives every event. That is great for "every instance clears its local cache", and wrong for "send one confirmation email". For the second case, only **one** consumer should act: use a queue or a Stream consumer group.

## Pub/Sub in Cluster

- Classic `PUBLISH` is **broadcast to every node** over the cluster bus, so clients may subscribe on any node. The cost grows with node count and message rate
- **Sharded Pub/Sub** (`SPUBLISH`/`SSUBSCRIBE`, Redis 7.0+) keeps each channel on the shard that owns its slot, which scales much better. Support in ioredis depends on version, so check its documentation for `ssubscribe`
- Connect your subscriber so it follows topology changes (`Cluster` clients handle this)

## Keyspace notifications (optional)

Redis can publish events when keys change, using Pub/Sub channels:

```
CONFIG SET notify-keyspace-events Ex      # E = keyevent channels, x = expired events
```

```ts
await sub.psubscribe("__keyevent@0__:expired");
sub.on("pmessage", (_pattern, _channel, expiredKey) => { /* ... */ });
```

These notifications are best effort (same delivery rules as any Pub/Sub message), and they are **per database** (`@0`). Don't build correctness on them. See [Expiration and TTL](../02_redis-fundamentals/03_expiration-and-ttl.md).

## Testing

```ts
it("delivers to subscribers", async () => {
  const received: string[] = [];
  const off = await pubsub.subscribe("test:chan", (m) => { received.push(m); });

  await pubsub.publish("test:chan", "hi");
  await waitFor(() => received.length === 1);        // poll briefly, don't sleep a fixed time

  expect(received).toEqual(["hi"]);
  await off();
});
```

- Use a **real Redis** for integration tests, because behavior on reconnect and with patterns can't be faked reliably
- Wait with a helper that polls with a timeout instead of `setTimeout(…, 100)`
- `PUBSUB NUMSUB channel` confirms a subscription is active before you publish

## Pitfalls

| Pitfall | Fix |
|---------|-----|
| Using Pub/Sub for must-deliver messages | Streams or a queue |
| Running normal commands on the subscriber connection | Separate publisher and subscriber clients |
| Calling `sub.on("message", …)` every time you subscribe | Register once, dispatch by channel (the wrapper) |
| Subscribed to both a channel and a matching pattern | Expect two deliveries, or avoid the overlap |
| Handler throws and kills processing | Isolate handlers, log, and catch async errors |
| Heavy work inside the handler | Enqueue and return quickly |
| Expecting history after a reconnect | Design for gaps (sequence numbers, resync from the source of truth) |
| Thousands of patterns | Fewer, coarser patterns, or exact channels |
| Assuming `keyPrefix` applies to channels | It doesn't. Build channel names explicitly |
| Sending large payloads | Send IDs, fetch details |

## Key takeaways

- Pub/Sub is **fan-out, fire-and-forget, at-most-once**
- Use two connections, register handlers once, and isolate handler failures
- Keep messages small, versioned and validated
- For durability, work sharing or replay, choose Streams or queues

**Next:** [Event-Driven Node.js](./02_event-driven-nodejs.md)
