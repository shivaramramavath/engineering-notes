# Notification System

A notification system turns events ("Ada commented on your post") into messages users actually see: an **in-app inbox** with an unread badge, a **real-time** pop-up for connected users, and **email or push** for those who are away. Redis fits each part: sorted sets for the inbox, sets for unread state, Pub/Sub for live delivery and queues for slow channels.

```
              event ("comment.created")
                        │
                        ▼
              ┌─────────────────────┐
              │ Notification service│── dedup, preferences, cooldown, digest
              └──────────┬──────────┘
        ┌────────────────┼──────────────────────┐
        ▼                ▼                      ▼
   Inbox (store)    Real-time push        Channel queues
   ZSET + HASH      Pub/Sub ► WebSocket   BullMQ ► email / push / SMS workers
   + unread SET     (best effort)         (retries, backoff, dead letters)
```

## Requirements

| Requirement | Approach |
|-------------|----------|
| Per-user inbox, newest first, paginated | Sorted set scored by time |
| Unread badge count, mark read, mark all read | Set of unread IDs |
| Live delivery to open tabs and apps | Pub/Sub to gateway nodes |
| Email and push with retries | Queue with backoff and dead letters |
| Respect preferences and quiet hours | Preferences hash checked before fan-out |
| No spam: collapse bursts, avoid repeats | Cooldowns, digests, dedup |
| Never slow down the request that caused it | Do channel delivery asynchronously |

## Data model

Use a **hash tag** (`{userId}`) so all of one user's keys live in the same Cluster slot. That lets Lua scripts and `MULTI` touch them together.

```
n:{uid}:items    HASH   notificationId ► JSON {id, type, title, body, data, createdAt}
n:{uid}:inbox    ZSET   member = notificationId, score = createdAt (ms)
n:{uid}:unread   SET    notificationIds not yet read
n:prefs:{uid}    HASH   channel or channel:type ► "0" (disabled). Missing means enabled
n:seq            STRING global counter for IDs
n:evt:{eventId}  STRING dedup marker for the source event (TTL)
```

Why this split:

- `inbox` gives ordered, paginated listing in O(log N + M)
- `items` holds bodies so the sorted set stays small and fast
- `unread` gives an **exact** count (`SCARD`) and exact per-item read state, which a plain counter cannot (counters drift on retries and double marks)
- Everything per user expires together after a period of inactivity

## Creating a notification atomically

Adding to three structures and trimming must be one atomic step, or a crash leaves an inbox entry with no body. Lua does it in one go (see [Lua Scripts](../06_advanced-commands/03_lua-scripts.md)).

```ts
// KEYS: items, inbox, unread | ARGV: id, createdAtMs, json, maxItems, ttlSeconds
const CREATE = `
redis.call("HSET", KEYS[1], ARGV[1], ARGV[3])
redis.call("ZADD", KEYS[2], ARGV[2], ARGV[1])
redis.call("SADD", KEYS[3], ARGV[1])

local over = redis.call("ZCARD", KEYS[2]) - tonumber(ARGV[4])
if over > 0 then
  local old = redis.call("ZRANGE", KEYS[2], 0, over - 1)
  redis.call("ZREMRANGEBYRANK", KEYS[2], 0, over - 1)
  redis.call("HDEL", KEYS[1], unpack(old))
  redis.call("SREM", KEYS[3], unpack(old))
end

for i = 1, 3 do redis.call("EXPIRE", KEYS[i], ARGV[5]) end
return redis.call("SCARD", KEYS[3])
`;
```

The script returns the new unread count, which you can send straight to the client for the badge.

## Service implementation

```ts
import Redis from "ioredis";

export interface Notification {
  id: string;
  userId: string;
  type: string;                    // "comment" | "follow" | "order.shipped" ...
  title: string;
  body?: string;
  data?: Record<string, unknown>;  // deep link, entity IDs
  createdAt: number;
}

type Scripted = Redis & {
  notifCreate(items: string, inbox: string, unread: string, id: string, ts: number, json: string, max: number, ttl: number): Promise<number>;
};

const keys = (uid: string) => ({
  items: `n:{${uid}}:items`,
  inbox: `n:{${uid}}:inbox`,
  unread: `n:{${uid}}:unread`,
});

const MAX_ITEMS = 500;
const INACTIVITY_TTL = 90 * 86_400;

export class NotificationService {
  private r: Scripted;

  constructor(private redis: Redis, private deps: { channels: ChannelDispatcher }) {
    redis.defineCommand("notifCreate", { numberOfKeys: 3, lua: CREATE });
    this.r = redis as Scripted;
  }

  /** Returns null when the source event was already processed. */
  async create(input: Omit<Notification, "id" | "createdAt">, eventId?: string): Promise<Notification | null> {
    if (eventId) {
      const first = await this.redis.set(`n:evt:${eventId}`, "1", "EX", 86_400, "NX");
      if (first !== "OK") return null;                           // duplicate event: ignore
    }

    const n: Notification = { ...input, id: String(await this.redis.incr("n:seq")), createdAt: Date.now() };
    const k = keys(n.userId);
    const unread = await this.r.notifCreate(k.items, k.inbox, k.unread, n.id, n.createdAt, JSON.stringify(n), MAX_ITEMS, INACTIVITY_TTL);

    await this.pushRealtime(n, unread);                          // best effort
    await this.deps.channels.dispatch(n);                        // enqueue email/push, never send inline
    return n;
  }

  /** Cursor pagination by time: pass the createdAt of the last item you received. */
  async list(userId: string, opts: { before?: number; limit?: number } = {}) {
    const { items, inbox, unread } = keys(userId);
    const limit = Math.min(opts.limit ?? 20, 100);

    const ids = opts.before
      ? await this.redis.zrange(inbox, `(${opts.before}`, "-inf", "BYSCORE", "REV", "LIMIT", 0, limit)
      : await this.redis.zrange(inbox, 0, limit - 1, "REV");
    if (ids.length === 0) return [];

    const [raw, isUnread] = await Promise.all([
      this.redis.hmget(items, ...ids),
      this.redis.smismember(unread, ...ids),                     // SMISMEMBER: Redis 6.2+
    ]);

    return ids.flatMap((id, i) =>
      raw[i] ? [{ ...(JSON.parse(raw[i]!) as Notification), read: isUnread[i] === 0 }] : [],
    );
  }

  async unreadCount(userId: string) {
    return this.redis.scard(keys(userId).unread);
  }

  async markRead(userId: string, ids: string[]) {
    const { unread } = keys(userId);
    if (ids.length) await this.redis.srem(unread, ...ids);
    return this.redis.scard(unread);
  }

  async markAllRead(userId: string) {
    await this.redis.del(keys(userId).unread);
    return 0;
  }

  async remove(userId: string, id: string) {
    const k = keys(userId);
    await this.redis.multi().hdel(k.items, id).zrem(k.inbox, id).srem(k.unread, id).exec();   // same slot thanks to {uid}
  }

  private async pushRealtime(n: Notification, unread: number) {
    await this.redis.publish("n:live", JSON.stringify({ userId: n.userId, notification: n, unread }));
  }
}
```

Notes:

- Time-based cursors (`before`) are stable while new items arrive, unlike offsets. Two items created in the same millisecond can share a score, so for very high rates use a score that includes a sequence suffix, or paginate by ID
- If an item was trimmed from `items` but a race left its ID in `inbox`, the `flatMap` skips it instead of crashing

## Real-time delivery

Each gateway node (the process holding WebSocket or SSE connections) subscribes once and forwards messages to the sockets it owns.

```ts
const sub = redis.duplicate();
await sub.subscribe("n:live");

sub.on("message", (_channel, message) => {
  const { userId, notification, unread } = JSON.parse(message);
  for (const socket of socketsByUser.get(userId) ?? []) {
    socket.send(JSON.stringify({ type: "notification", notification, unread }));
  }
});
```

With Socket.IO, put each user in a room (`user:{id}`) and use the Redis adapter, or the Redis emitter package to emit from non-socket services (see [Socket.IO with Redis](../09_pub-sub/03_socketio-with-redis.md)).

Pub/Sub is **at-most-once**: a message published while a client is disconnected is gone. That is fine because the **inbox is the source of truth**:

- On connect or reconnect, the client calls `GET /notifications?since=...` (or just the unread count) and merges
- Real-time pushes are a latency optimization, not a delivery guarantee

At scale, delivering every notification to every gateway wastes bandwidth. Alternatives: per-user channels subscribed on connect and unsubscribed on disconnect, or sharded Pub/Sub in Cluster (Redis 7).

## Channel fan-out: email, push, SMS

Slow, flaky external providers belong in a queue, not in the request path.

```ts
import { Queue } from "bullmq";

const channelQueue = new Queue("notify-channels", { connection: { host: "redis", port: 6379 } });

class ChannelDispatcher {
  constructor(private redis: Redis) {}

  async dispatch(n: Notification) {
    const prefs = await this.redis.hgetall(`n:prefs:${n.userId}`);
    const enabled = (channel: string) => prefs[channel] !== "0" && prefs[`${channel}:${n.type}`] !== "0";

    for (const channel of ["email", "push"]) {
      if (!enabled(channel)) continue;
      if (!(await this.allowedNow(n, channel))) continue;                       // cooldown or digest

      await channelQueue.add(channel, { notificationId: n.id, userId: n.userId }, {
        jobId: `${channel}-${n.id}`,                                            // one job per notification and channel (no colons)
        attempts: 5,
        backoff: { type: "exponential", delay: 5_000 },
        removeOnComplete: 1_000,
        removeOnFail: 5_000,
      });
    }
  }

  private async allowedNow(_n: Notification, _channel: string) { return true; } // see "Taming noise"
}
```

The worker loads the notification, renders it and calls the provider. At-least-once delivery means a job can run twice, so make sending idempotent: pass the notification ID as the provider's idempotency key, or record "sent via email" before and after:

```ts
// worker for "email" jobs
async function sendEmail({ notificationId, userId }: { notificationId: string; userId: string }) {
  if ((await redis.set(`n:sent:email:${notificationId}`, "1", "EX", 7 * 86_400, "NX")) !== "OK") return;
  try {
    await emailProvider.send(await buildEmail(userId, notificationId), { idempotencyKey: notificationId });
  } catch (err) {
    await redis.del(`n:sent:email:${notificationId}`);       // allow the retry
    throw err;                                               // BullMQ retries with backoff, then dead-letters
  }
}
```

Retry, backoff and dead-letter details are in [Retries and Dead Letter Queue](../14_queues-and-workers/03_retries-and-dead-letter-queue.md).

## Preferences

```ts
await redis.hset("n:prefs:42", { email: "0" });              // no email at all
await redis.hset("n:prefs:42", { "push:marketing": "0" });   // no marketing push
```

Default to **enabled**, store only opt-outs, and always honor required messages (security alerts, receipts) regardless of preference. Check quiet hours in the worker so delayed sends respect the user's time zone.

## Taming noise

Users abandon products that over-notify. Combine these:

### Cooldown per user, type and entity

```ts
async function cooldown(userId: string, type: string, entityId: string, seconds = 600) {
  return (await redis.set(`n:cool:${userId}:${type}:${entityId}`, "1", "EX", seconds, "NX")) === "OK";
}
// "5 new comments on your post" should email once per 10 minutes, not five times
```

### Collapse similar events ("Ada and 4 others liked your post")

Keep a counter and a short list of actors for the entity, and update the **same** notification instead of creating a new one:

```ts
const groupKey = `n:group:${userId}:like:${postId}`;
const [[, count]] = (await redis.multi()
  .hincrby(groupKey, "count", 1)
  .hsetnx(groupKey, "first", actorName)
  .expire(groupKey, 3600)
  .exec())! as any;
// first like: create a notification. Later likes: update its title with the count (HSET into items)
```

### Digest instead of instant

```ts
async function addToDigest(userId: string, item: object) {
  await redis.rpush(`n:digest:${userId}`, JSON.stringify(item));
  // schedule exactly one flush per window
  if ((await redis.set(`n:digest:sched:${userId}`, "1", "EX", 900, "NX")) === "OK") {
    await digestQueue.add("flush", { userId }, { delay: 15 * 60_000 });
  }
}

async function flushDigest({ userId }: { userId: string }) {
  const [[, raw]] = (await redis.multi().lrange(`n:digest:${userId}`, 0, -1).del(`n:digest:${userId}`).exec())! as any;
  if (raw.length) await emailProvider.sendDigest(userId, raw.map((s: string) => JSON.parse(s)));
}
```

The `SET NX` guard schedules one flush per window. Reading and deleting inside `MULTI` means items added during the flush are not lost or sent twice. See [Delayed Jobs](../14_queues-and-workers/02_delayed-jobs.md).

### Dedup the source event

Event buses redeliver, so pass the event ID to `create(..., eventId)` as shown above. See [Request Deduplication](./02_request-deduplication.md).

## Broadcast to many users

Writing one copy per follower (**fan-out on write**) is simple but explodes for an account with millions of followers.

| Audience | Strategy |
|----------|----------|
| Up to thousands | Fan-out on write, one inbox entry per user, in batches via a queue |
| Huge (announcements, celebrity posts) | **Fan-out on read**: store one global notification and a per-user "last seen" pointer. Merge at read time |
| Segments | Resolve the audience in batches, enqueue one job per chunk of users |

Never loop over millions of users inside a request or a single job. Chunk the work and make each chunk idempotent.

## HTTP API

```ts
app.get("/notifications", auth, async (req, res) => {
  const before = req.query.before ? Number(req.query.before) : undefined;
  res.json(await notifications.list(req.user.id, { before, limit: 20 }));
});

app.get("/notifications/unread-count", auth, async (req, res) => {
  res.json({ unread: await notifications.unreadCount(req.user.id) });
});

app.post("/notifications/read", auth, async (req, res) => {
  const ids: string[] = Array.isArray(req.body.ids) ? req.body.ids.slice(0, 100).map(String) : [];
  res.json({ unread: await notifications.markRead(req.user.id, ids) });
});

app.post("/notifications/read-all", auth, async (req, res) => {
  res.json({ unread: await notifications.markAllRead(req.user.id) });
});
```

The user ID always comes from the **authenticated session**, never from the request body or URL, or users could read each other's inbox.

## Failure modes

| Failure | Effect | Handling |
|---------|--------|----------|
| Redis down during `create` | Notification lost | Retry from the event source (the event bus or an outbox), keep creation idempotent |
| Pub/Sub message missed | No live pop-up | Client refetches the inbox on reconnect and periodically |
| Provider outage | Email or push delayed | Queue retries with backoff, alert on dead-letter growth |
| Worker crash mid-job | Job redelivered | Idempotent sends with a marker or provider key |
| Trimmed inbox | Old items disappear | Document the limit, archive to a database if the history matters |
| Duplicate events | Duplicate notifications | Dedup by event ID |
| Failover loses recent writes | A few recent notifications missing | AOF, replicas, and a durable event source for important ones |

## Monitoring

- Queue depth, oldest job age and dead-letter count for each channel
- Delivery latency from event to inbox, and to provider acceptance
- Provider error rates, bounces and unsubscribes
- Inbox sizes (`ZCARD`) for the largest users, to spot runaway producers
- Notification volume per type, to catch a bug that floods users

## Testing

```ts
it("adds to inbox and reports the unread count", async () => {
  await svc.create({ userId: "u1", type: "comment", title: "Hi" });
  await svc.create({ userId: "u1", type: "comment", title: "Again" });
  expect(await svc.unreadCount("u1")).toBe(2);
  const list = await svc.list("u1");
  expect(list.map((n) => n.title)).toEqual(["Again", "Hi"]);       // newest first
  expect(list.every((n) => !n.read)).toBe(true);
});

it("marks read and keeps counts exact", async () => {
  const a = (await svc.create({ userId: "u1", type: "t", title: "A" }))!;
  await svc.markRead("u1", [a.id, a.id]);                           // double mark is harmless
  expect(await svc.unreadCount("u1")).toBe(0);
});

it("ignores duplicate source events", async () => {
  await svc.create({ userId: "u1", type: "t", title: "X" }, "evt-1");
  expect(await svc.create({ userId: "u1", type: "t", title: "X" }, "evt-1")).toBeNull();
  expect(await svc.unreadCount("u1")).toBe(1);
});

it("trims the inbox to the maximum and removes bodies too", async () => {
  for (let i = 0; i < MAX_ITEMS + 5; i++) await svc.create({ userId: "u2", type: "t", title: `n${i}` });
  expect(await t.redis.zcard("n:{u2}:inbox")).toBe(MAX_ITEMS);
  expect(await t.redis.hlen("n:{u2}:items")).toBe(MAX_ITEMS);
});

it("paginates with a time cursor", async () => {
  for (let i = 0; i < 5; i++) { await svc.create({ userId: "u3", type: "t", title: `n${i}` }); await sleep(2); }
  const page1 = await svc.list("u3", { limit: 2 });
  const page2 = await svc.list("u3", { limit: 2, before: page1[1].createdAt });
  expect(page2.map((n) => n.title)).toEqual(["n2", "n1"]);
});
```

Test the real-time path with a subscriber connection (publish after `await sub.subscribe(...)`), and test workers by running the handler function directly with a fake provider. Use the helpers from [Testing with Redis](../18_testing-and-debugging/01_testing-with-redis.md). When using `keyPrefix` in tests, remember that hash-tagged keys still work, but scripts receive prefixed `KEYS` automatically.

## Pitfalls

| Pitfall | Fix |
|---------|-----|
| Sending email inside the request handler | Enqueue and respond immediately |
| Plain counter for unread | A set of unread IDs gives exact counts and per-item state |
| Inbox entry and body written separately | One Lua script or `MULTI` in the same slot |
| Keys for one user in different Cluster slots | Hash tag the user ID: `n:{uid}:...` |
| Relying on Pub/Sub for delivery | Inbox is the truth, live push is best effort |
| Offset pagination while new items arrive | Time or ID cursors |
| Unbounded inbox | Trim to a maximum, expire on inactivity |
| Duplicate notifications from redelivered events | Dedup on event ID |
| Duplicate emails from retried jobs | Idempotent send marker or provider idempotency key |
| `jobId` with a colon in BullMQ | Use dashes (`email-123`) |
| No cooldown, collapse or digest | Throttle by user, type and entity |
| Loop over all followers in one job | Chunk and enqueue per chunk |
| User ID taken from the request | Always from the authenticated session |
| Ignoring preferences for required messages | Separate "required" types that bypass opt-outs |

## Key takeaways

- Store notifications once (hash plus sorted set plus unread set) and deliver them through several channels
- Make creation atomic with Lua, hash-tag per-user keys for Cluster and bound the inbox
- Use Pub/Sub for instant delivery but treat the inbox as the source of truth
- Push slow channels through queues with retries, backoff, dead letters and idempotent sends
- Protect users from noise with preferences, cooldowns, collapsing and digests
- Dedup events, authenticate every call and monitor queue depth and delivery latency

**Previous:** [Leaderboard](./03_leaderboard.md) | **Next:** [Feature Flags](./05_feature-flags.md)
