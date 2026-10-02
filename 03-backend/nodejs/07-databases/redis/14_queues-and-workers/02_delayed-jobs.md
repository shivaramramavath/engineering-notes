# Delayed Jobs

A **delayed job** should run **at a later time**: a reminder in 24 hours, a retry in 30 seconds, "release the reserved stock in 15 minutes unless the order is paid". Redis makes this natural with a **sorted set scored by run time**.

## Options

| Approach | Reliable? | Notes |
|----------|-----------|-------|
| `setTimeout` in the app | **No** | Lost on restart, deploy or crash |
| Redis key expiry notifications | **No** | Best-effort Pub/Sub, may be missed ([Pub/Sub Fundamentals](../09_pub-sub/01_pub-sub-fundamentals.md#keyspace-notifications-optional)) |
| `cron` in every instance | Duplicated | Every instance fires ([run-once pitfalls](../11_distributed-locks/01_locking-fundamentals.md#pattern-run-a-scheduled-job-on-one-instance)) |
| **Sorted set by run-at time + a promoter** | **Yes** | Durable, atomic, scalable (this lesson) |
| **BullMQ delayed jobs** | **Yes** | The same idea, hardened ([BullMQ](./04_bullmq.md)) |
| A database table + poller | Yes | Good for very long delays (weeks, months) |

## The design

```
add(delayMs)  ──►  delayed (zset: job id → run-at ms)
                         │
         promoter, every ~1 s or when the next job is due:
         move every id with score ≤ now  ──►  wait (list)  ──►  workers claim it as usual
```

- Run-at times are **absolute milliseconds**, from the Redis **server clock** (`TIME`), so every app instance agrees
- `ZRANGEBYSCORE delayed -inf <now>` finds due jobs in O(log N + M)
- A **promoter** moves due jobs into the normal waiting list atomically

The enqueue script from [lesson 01](./01_queue-fundamentals.md#enqueue) already contains the delayed branch (`ZADD delayed now+delay id`).

## The promoter script

```lua
-- promote.lua
-- KEYS[1] = delayed, KEYS[2] = wait
-- ARGV: 1 jobPrefix, 2 batch limit
-- returns {movedCount, nextDueMs}  (nextDueMs = 0 when nothing is scheduled)
local t = redis.call("TIME")
local now = tonumber(t[1]) * 1000 + math.floor(tonumber(t[2]) / 1000)

local ids = redis.call("ZRANGEBYSCORE", KEYS[1], "-inf", now, "LIMIT", 0, tonumber(ARGV[2]))
local moved = 0
for _, id in ipairs(ids) do
  if redis.call("ZREM", KEYS[1], id) == 1 then                         -- exactly one promoter wins each job
    local jobKey = ARGV[1] .. id
    if redis.call("EXISTS", jobKey) == 1 then
      redis.call("HSET", jobKey, "state", "waiting")
      redis.call("LPUSH", KEYS[2], id)
      moved = moved + 1
    end
  end
end

local nxt = redis.call("ZRANGE", KEYS[1], 0, 0, "WITHSCORES")           -- the earliest remaining job
local nextDue = 0
if nxt[2] then nextDue = tonumber(nxt[2]) end
return {moved, nextDue}
```

`ZREM` returning `1` for only one caller means **many promoters can run safely at once**: no leader election, and a crashed promoter loses nothing, since the job stays in the sorted set until moved.

## The promoter loop

Each worker process can run one. Sleeping until the **next due time** keeps latency low without hammering Redis:

```ts
declare module "ioredis" {
  interface RedisCommander<Context> {
    qPromote(delayed: string, wait: string, jobPrefix: string, limit: number): Promise<[number, number]>;
  }
}
redis.defineCommand("qPromote", { numberOfKeys: 2, lua: lua("promote") });

// inside JobWorker
private async promoteLoop() {
  const maxSleep = 1_000;
  while (this.running) {
    try {
      const [moved, nextDue] = await this.redis.qPromote(this.k.delayed, this.k.wait, this.k.jobPrefix, 200);
      if (moved === 200) continue;                                       // a full batch: more may be due, go again immediately

      const untilNext = nextDue > 0 ? nextDue - Date.now() : maxSleep;   // client clock only for sleeping, not for deciding
      await sleep(Math.min(Math.max(untilNext, 20), maxSleep) + Math.random() * 50);
    } catch (err) {
      this.log.error({ err }, "promoter failed");
      await sleep(1_000);
    }
  }
}
```

Call `this.loops.push(this.promoteLoop())` in `start()`. The **decision** about what is due is made inside Redis with its own clock. The worker's clock only decides how long to nap.

A new job with an **earlier** run time than the current head might wait up to `maxSleep` (1 second) before being noticed. That is the precision of the system: **about a second**, which suits reminders, retries and expiries. For tighter timing, lower `maxSleep`, or have `add` publish a "wake" message promoters subscribe to.

### Precision and limits

| Aspect | Reality |
|--------|---------|
| Accuracy | Roughly `maxSleep` plus worker pickup time (about a second) |
| Scale | The sorted set handles millions of entries (`ZADD` and `ZREM` are O(log N)) |
| Memory | Each pending job's payload sits in Redis **until it runs**. For schedules weeks or months away, store them in your database and enqueue them **shortly before** they are due |
| Bursts | Many jobs due at the same instant are promoted in batches of 200 per script call, repeatedly |

## Producer API

```ts
await queue.add("send-reminder", { orderId: 9001 }, { jobId: "reminder:9001", delayMs: 24 * 3600_000 });
```

Absolute times are easy to derive:

```ts
const delayMs = Math.max(0, appointment.startsAt.getTime() - 15 * 60_000 - Date.now());
await queue.add("appointment-reminder", { id: appointment.id }, { jobId: `appt-reminder:${appointment.id}`, delayMs });
```

`delayMs` is relative to Redis `TIME` at enqueue. The small difference between your clock and Redis's is irrelevant for delays of seconds or more.

## Cancelling and rescheduling

A delayed job sits in the sorted set, so cancelling is a `ZREM`:

```ts
// JobQueue additions
async cancel(jobId: string): Promise<boolean> {
  const removed = await this.redis.zrem(this.k.delayed, jobId);          // 1 only if it was still pending
  if (removed === 1) await this.redis.unlink(this.k.jobPrefix + jobId);
  return removed === 1;
}

async reschedule(jobId: string, delayMs: number): Promise<boolean> {
  const t = await this.redis.time();                                     // [seconds, microseconds] from Redis
  const now = Number(t[0]) * 1000 + Math.floor(Number(t[1]) / 1000);
  return (await this.redis.zadd(this.k.delayed, "XX", "CH", now + delayMs, jobId)) === 1;   // XX: only if it still exists
}
```

`cancel` returns `false` if the job already moved to waiting or is running, which is the answer you need: **too late to cancel**. Handlers should therefore still **re-check state** before acting.

### Example: release a reservation unless paid

```ts
// when stock is reserved
await queue.add("release-reservation", { orderId }, { jobId: `release:${orderId}`, delayMs: 15 * 60_000 });

// when payment succeeds
const cancelled = await queue.cancel(`release:${orderId}`);
// cancelled === false means the release job is already running or has run. The handler decides what to do:

// the handler, which always checks the current state first (idempotent)
async function releaseReservation({ data }: JobContext<{ orderId: string }>) {
  const order = await orders.findById(data.orderId);
  if (order.status === "paid") return;                                   // paid in the meantime: nothing to do
  await stock.release(order);
}
```

Cancellation is an **optimization**. **The handler's state check is the guarantee.**

### Debounce and "reset the timer"

Run something N seconds after the **last** event:

```ts
// every event pushes the run time forward
await queue.add("flush-user-cache", { userId }, { jobId: `flush:${userId}`, delayMs: 10_000 });   // first time: scheduled
await queue.reschedule(`flush:${userId}`, 10_000);                                                // later events: push it back
```

If `reschedule` returns `false` (already running or done), enqueue a fresh job.

## Recurring jobs

**Don't run `cron` in every instance** and hope: they all fire. Two correct designs:

### 1. Self-rescheduling

When a job finishes, it schedules the next occurrence. A **deterministic job id per occurrence** deduplicates if two instances race:

```ts
async function handleDigest({ data }: JobContext<{ slot: string }>) {
  await sendDigest();

  const next = nextHourSlot(data.slot);                                  // e.g. "2026-10-02T10"
  await queue.add("digest", { slot: next }, {
    jobId: `digest:${next}`,                                             // the same id: a duplicate add is a no-op
    delayMs: Math.max(0, slotStart(next) - Date.now()),
  });
}
```

The weakness: if the chain breaks (a bug, a purge), the schedule **silently stops**. Add a **watchdog** that ensures a future job exists.

### 2. A scheduler that enqueues idempotently

A small loop (in every instance) ensures the **next occurrences exist**, using deterministic ids:

```ts
import { CronExpressionParser } from "cron-parser";                      // API differs across versions, so check yours

async function ensureScheduled(name: string, cronExpr: string, lookahead = 3) {
  const it = CronExpressionParser.parse(cronExpr, { tz: "UTC" });
  for (let i = 0; i < lookahead; i++) {
    const at = it.next().toDate();
    await queue.add(name, { scheduledFor: at.toISOString() }, {
      jobId: `${name}:${at.toISOString()}`,                              // one job per occurrence, across all instances
      delayMs: Math.max(0, at.getTime() - Date.now()),
    });
  }
}

setInterval(() => void ensureScheduled("digest", "0 * * * *").catch(console.error), 60_000);
```

Every instance runs the same loop, and `jobId` makes the **enqueue idempotent**, so exactly one job exists per occurrence. If the loop stops in one instance, others continue. This is the pattern BullMQ's job schedulers implement for you.

Cron details to get right:

- Store and compare **UTC** times. Apply a time zone only when **computing** the next fire time (and be deliberate about DST: some local times don't exist or occur twice)
- Decide what happens to **missed runs** after downtime (run once, run all, skip). Deterministic ids plus `Math.max(0, ...)` above run **overdue** occurrences immediately
- Make the job **idempotent per slot** (`scheduledFor` in the payload), so a duplicate run is harmless

## Retries are delayed jobs

An exponential backoff retry is just "schedule this same job again in N seconds". Lesson 03's `fail` script puts failed jobs into the same `delayed` set, so promotion, cancel and observability all work uniformly.

## Do not use key expiry as a scheduler

Setting `SET reminder:42 ... EX 86400` and listening for the expired event looks neat but:

- Expiry events are **Pub/Sub**, so a disconnected listener **misses them**
- Redis expires keys **lazily and approximately**, so events can be late
- The key is gone when the event arrives, and you must have kept the payload elsewhere

Use the sorted set.

## Observability

| Metric | Why |
|--------|-----|
| `ZCARD delayed` | How much future work is stored |
| **Lateness**: promotion time minus run-at time | The scheduling accuracy you are actually delivering |
| Time until the next due job (`ZRANGE delayed 0 0 WITHSCORES`) | Is the schedule healthy |
| Overdue count: `ZCOUNT delayed -inf <now>` | Promoters stalled if this grows |

```ts
const overdue = await redis.zcount(k.delayed, "-inf", Date.now());
```

Alert if `overdue` stays above zero for more than a few seconds, since it means promoters aren't running.

## Testing

```ts
it("runs a delayed job after its delay, not before", async () => {
  const q = new JobQueue(redis, "d1");
  const ran: number[] = [];
  const w = new JobWorker(redis, "d1", async () => { ran.push(Date.now()); }, { pollMs: 10 });
  const start = Date.now();

  await q.add("n", {}, { delayMs: 300 });
  w.start();

  await sleep(150);
  expect(ran).toHaveLength(0);                                           // not yet
  await waitFor(() => ran.length === 1, 3_000);
  expect(ran[0]! - start).toBeGreaterThanOrEqual(280);                   // tolerance for clock and poll jitter
  await w.stop();
});

it("promotes each due job exactly once with many promoters", async () => {
  const k = queueKeys("d2");
  const q = new JobQueue(redis, "d2");
  for (let i = 0; i < 50; i++) await q.add("n", { i }, { jobId: `j${i}`, delayMs: 50 });
  await sleep(120);

  await Promise.all(Array.from({ length: 8 }, () => redis.qPromote(k.delayed, k.wait, k.jobPrefix, 200)));
  expect(await redis.llen(k.wait)).toBe(50);                             // not 400
});

it("cancel works before the job is due, and reports failure after", async () => {
  const q = new JobQueue(redis, "d3");
  await q.add("n", {}, { jobId: "c", delayMs: 5_000 });
  expect(await q.cancel("c")).toBe(true);
  expect(await q.cancel("c")).toBe(false);
});
```

## Pitfalls

| Pitfall | Fix |
|---------|-----|
| `setTimeout` for anything that must happen | A sorted-set schedule |
| Key-expiry notifications as a scheduler | They are lossy, so use the zset |
| Client clock deciding what is due | Decide inside Redis with `TIME` |
| One promoter as a single point of failure | Every worker promotes, which is safe because of `ZREM` |
| Delays of weeks or months stored in Redis memory | Keep them in the database, and enqueue near the due time |
| Assuming `cancel` always succeeds | It returns `false` once running, so make handlers re-check state |
| `cron` firing in every instance | Self-rescheduling or deterministic ids per occurrence |
| Local time in schedules | Compute in the time zone, store and compare in UTC |
| Self-rescheduling chains with no watchdog | Ensure a future job always exists |
| Not alerting on overdue delayed jobs | Monitor lateness and the overdue count |

## Key takeaways

- A **sorted set scored by run-at time** plus an **atomic promoter** is a durable, scalable delay mechanism
- Decide "due" with **Redis's clock**, and run promoters everywhere. `ZREM` makes it safe
- **Cancel** is a `ZREM`, so make handlers **re-check state** because cancellation can lose the race
- Use **deterministic job ids** to run recurring jobs once per slot across instances
- Retries and delays are the same mechanism

**Next:** [Retries and Dead-Letter Queue](./03_retries-and-dead-letter-queue.md)
