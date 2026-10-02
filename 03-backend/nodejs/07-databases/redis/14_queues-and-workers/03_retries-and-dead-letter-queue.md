# Retries and Dead-Letter Queue

Things fail. A good queue distinguishes failures that **deserve another try** from ones that **never will succeed**, spaces the retries so they don't pile onto a struggling dependency, and keeps jobs that run out of attempts somewhere a human (or a repair job) can find them.

## Classify the failure first

| Class | Examples | Right response |
|-------|----------|----------------|
| **Transient** | Network blip, `503`, timeout, deadlock, connection pool exhausted | **Retry** with backoff |
| **Rate limited or overloaded** | `429`, "try again later", `Retry-After` | Retry **after the time the server said**, without burning an attempt |
| **Permanent** | Validation error, `400`, `404`, "user deleted", a bug in the handler | **Don't retry**: send to the dead-letter queue now |
| **Poison** | Crashes the process (out-of-memory, native crash) | The job stalls repeatedly, so it must end in the dead set |
| **Stalled** | Worker died or froze mid-job | Recover via the lease reaper ([lesson 01](./01_queue-fundamentals.md#the-stalled-job-reaper)) |

Retrying a permanent failure five times only delays the alert and wastes capacity. **Not** retrying a transient one loses work. Make the handler say which it is:

```ts
export class PermanentError extends Error {}                              // never retry
export class RetryAfterError extends Error {                              // retry later, don't count the attempt
  constructor(public afterMs: number, msg = "retry later") { super(msg); }
}

async function handler({ data, signal }: JobContext<EmailJob>) {
  const res = await mailer.send(data, { signal });

  if (res.status === 429) throw new RetryAfterError(Number(res.headers["retry-after"] ?? 30) * 1000);
  if (res.status >= 400 && res.status < 500) throw new PermanentError(`provider rejected: ${res.status}`);
  if (res.status >= 500) throw new Error(`provider error ${res.status}`);          // transient: default
}
```

**Unknown errors default to retryable** (bounded by `maxAttempts`), and permanent ones must be marked explicitly.

## Backoff

Retry spacing should **grow**, and **randomness** should spread out clients that failed together.

```ts
export function backoffMs(attempt: number, { baseMs = 1_000, capMs = 5 * 60_000 } = {}) {
  const exp = Math.min(capMs, baseMs * 2 ** (attempt - 1));     // 1s, 2s, 4s, 8s ... capped
  return Math.floor(exp / 2 + Math.random() * (exp / 2));       // "equal jitter": half fixed, half random
}
```

| Strategy | Delay | Use |
|----------|-------|-----|
| Fixed | Same each time | Simple internal retries |
| Exponential | Doubles, capped | The default for external dependencies |
| **Exponential with jitter** | Doubles, randomized | **Always**, to prevent synchronized **retry storms** |
| Server-directed | `Retry-After` | Rate limits |
| Custom per error | Your logic | Known recovery times (a deploy window) |

Without jitter, 10,000 jobs that failed at the same moment all retry at the same moment, and fail again together.

Choose `maxAttempts` and the cap so the **total retry window** covers realistic outages: 5 attempts with a 1 s base is only about 15 seconds of coverage, and a downstream outage often lasts minutes. For slow recovery, raise the cap and attempts, or use the **pause** pattern below.

## The `fail` script

On failure, decide atomically: retry (into the delayed set) or give up (into the dead set).

```lua
-- fail.lua
-- KEYS[1] = active, KEYS[2] = delayed, KEYS[3] = dead
-- ARGV: 1 jobPrefix, 2 id, 3 workerId, 4 retryable ("1"/"0"), 5 backoffMs, 6 error text, 7 keepDeadSec (0 = forever)
-- returns {1, "retry"} | {1, "dead"} | {0, "lost"}
local jobKey = ARGV[1] .. ARGV[2]
if redis.call("HGET", jobKey, "worker") ~= ARGV[3] then return {0, "lost"} end     -- not our job any more
if redis.call("ZREM", KEYS[1], ARGV[2]) == 0 then return {0, "lost"} end

local t = redis.call("TIME")
local now = tonumber(t[1]) * 1000 + math.floor(tonumber(t[2]) / 1000)

local attempts = tonumber(redis.call("HGET", jobKey, "attempts"))
local maxAttempts = tonumber(redis.call("HGET", jobKey, "maxAttempts"))
redis.call("HSET", jobKey, "lastError", ARGV[6], "failedAt", now)

if ARGV[4] == "1" and attempts < maxAttempts then
  redis.call("HSET", jobKey, "state", "delayed")
  redis.call("ZADD", KEYS[2], now + tonumber(ARGV[5]), ARGV[2])                     -- the retry is a delayed job (lesson 02)
  return {1, "retry"}
end

redis.call("HSET", jobKey, "state", "failed")
redis.call("ZADD", KEYS[3], now, ARGV[2])                                           -- the dead-letter set
if tonumber(ARGV[7]) > 0 then redis.call("EXPIRE", jobKey, tonumber(ARGV[7])) end
return {1, "dead"}
```

Two companion scripts:

```lua
-- defer.lua   "not now, and don't count this attempt" (429, maintenance windows)
-- KEYS[1] = active, KEYS[2] = delayed; ARGV: 1 jobPrefix, 2 id, 3 workerId, 4 delayMs
local jobKey = ARGV[1] .. ARGV[2]
if redis.call("HGET", jobKey, "worker") ~= ARGV[3] then return 0 end
if redis.call("ZREM", KEYS[1], ARGV[2]) == 0 then return 0 end
local t = redis.call("TIME")
local now = tonumber(t[1]) * 1000 + math.floor(tonumber(t[2]) / 1000)
redis.call("HINCRBY", jobKey, "attempts", -1)                                       -- undo the claim's increment
redis.call("HSET", jobKey, "state", "delayed")
redis.call("ZADD", KEYS[2], now + tonumber(ARGV[4]), ARGV[2])
return 1
```

```lua
-- release.lua  "put it back" during shutdown (no attempt consumed)
-- KEYS[1] = active, KEYS[2] = wait; ARGV: 1 jobPrefix, 2 id, 3 workerId
local jobKey = ARGV[1] .. ARGV[2]
if redis.call("HGET", jobKey, "worker") ~= ARGV[3] then return 0 end
if redis.call("ZREM", KEYS[1], ARGV[2]) == 0 then return 0 end
redis.call("HINCRBY", jobKey, "attempts", -1)
redis.call("HSET", jobKey, "state", "waiting")
redis.call("RPUSH", KEYS[2], ARGV[2])                                               -- the consuming end: runs next
return 1
```

All three check **ownership** first, exactly like `ack` ([lesson 01](./01_queue-fundamentals.md#acknowledge-and-extend-the-lease)).

## Wiring failures into the worker

Replace the `catch` block from lesson 01's `runJob`:

```ts
declare module "ioredis" {
  interface RedisCommander<Context> {
    qFail(active: string, delayed: string, dead: string, jobPrefix: string, id: string, workerId: string,
          retryable: string, backoffMs: number, error: string, keepDeadSec: number): Promise<[number, string]>;
    qDefer(active: string, delayed: string, jobPrefix: string, id: string, workerId: string, delayMs: number): Promise<number>;
    qRelease(active: string, wait: string, jobPrefix: string, id: string, workerId: string): Promise<number>;
  }
}

redis.defineCommand("qFail",    { numberOfKeys: 3, lua: lua("fail") });
redis.defineCommand("qDefer",   { numberOfKeys: 2, lua: lua("defer") });
redis.defineCommand("qRelease", { numberOfKeys: 2, lua: lua("release") });
```

```ts
} catch (err) {
  const k = this.k;

  // 1. we are shutting down and aborted the job: give it back without using an attempt
  if (this.hardStop.signal.aborted) {
    await this.redis.qRelease(k.active, k.wait, k.jobPrefix, id, this.workerId);
    return;
  }

  // 2. the server told us to wait: reschedule without counting this attempt
  if (err instanceof RetryAfterError) {
    await this.redis.qDefer(k.active, k.delayed, k.jobPrefix, id, this.workerId, err.afterMs);
    return;
  }

  // 3. a real failure: retry with backoff, or die
  const retryable = !(err instanceof PermanentError) && !leaseLost.signal.aborted;
  const message = String((err as Error)?.stack ?? err).slice(0, 2_000);              // cap what you store

  const [, outcome] = await this.redis.qFail(
    k.active, k.delayed, k.dead, k.jobPrefix, id, this.workerId,
    retryable ? "1" : "0", backoffMs(attempt, this.o.backoff), message, this.o.keepDeadSec ?? 7 * 86_400
  );

  this.log[outcome === "dead" ? "error" : "warn"]({ id, name, attempt, outcome, err }, `job ${outcome}`);
}
```

What this adds:

| Case | Behavior |
|------|----------|
| Transient error | Delayed retry with backoff and jitter, up to `maxAttempts` |
| `PermanentError` | Straight to the dead set, with no wasted attempts |
| `RetryAfterError` | Rescheduled for the server's time, **attempt not consumed** |
| Shutdown abort | Returned to the queue, **attempt not consumed** |
| Lease lost (another worker may own it) | `qFail` returns `lost`, since the new owner decides |
| Crash, OOM, hang | The lease expires, the reaper requeues, and after `maxStalled` it goes to the dead set |

## Poison jobs

A job that **kills the worker** never reaches `catch`. It stalls, the reaper requeues it, the next worker crashes, and so on. Because the `stalled` counter increments each time and the reaper sends it to the **dead set after `maxStalled`**, a crash loop **ends** instead of repeating forever.

Mitigations: keep `maxStalled` small (1 to 3), run risky work (native libraries, huge files) in a **child process or worker thread** with memory limits, and alert on stalled-job counts.

## Protecting a struggling dependency

If the downstream service is **down**, retrying every job just burns attempts and adds load. Options:

### Pause the queue

```ts
// a simple pause flag, checked by workers before claiming
await redis.set(k.paused, "1", "EX", 300);                                           // auto-resume after 5 minutes

// in consumeLoop
if (await redis.exists(this.k.paused)) { await sleep(1_000); continue; }
```

Trip the pause with a **circuit breaker**: after N consecutive failures from the same dependency, pause for a few minutes ([circuit breaker sketch](../07_caching/04_cache-problems.md#cause-3-redis-is-down-or-slow)). Jobs wait safely in the queue rather than failing.

### Rate limit the dependency

Workers wait for a shared budget before calling the provider:

```ts
await acquire(limiter, { name: "mail-provider", algo: "gcra", limit: 20, windowSec: 1 }, "global");
```

([Limiting outbound calls](../12_rate-limiting/03_distributed-rate-limiter.md#limiting-outbound-calls)). The budget is shared by every worker on every host.

### Cap concurrency

A semaphore ([concurrency limiting](../12_rate-limiting/02_token-and-leaky-bucket.md#concurrency-limiting-a-different-question)) limits how many jobs hit the dependency at once, which helps when failures come from overload.

## The dead-letter queue

The **dead set** holds jobs that exhausted their attempts, failed permanently, or kept stalling. It must be treated as a **work queue for humans and repair jobs**, not a graveyard.

Each dead job keeps its **payload, the last error (with stack), the attempt count and timestamps**, which is everything needed to diagnose and replay.

### Inspect

```ts
async function listDead(limit = 50) {
  const ids = await redis.zrevrange(k.dead, 0, limit - 1);                           // newest first
  const p = redis.pipeline();
  ids.forEach((id) => p.hgetall(k.jobPrefix + id));
  const res = (await p.exec()) ?? [];
  return res.map(([, h], i) => ({ id: ids[i]!, ...(h as Record<string, string>) }))
            .filter((j) => j.name);                                                  // skip hashes that already expired
}
```

Group by `lastError` (first line) to see **which failure is dominating**. Ten thousand dead jobs with one error message are one bug, not ten thousand problems.

### Replay

```lua
-- retry-dead.lua
-- KEYS[1] = dead, KEYS[2] = wait; ARGV: 1 jobPrefix, 2 id
if redis.call("ZREM", KEYS[1], ARGV[2]) == 0 then return 0 end
local jobKey = ARGV[1] .. ARGV[2]
if redis.call("EXISTS", jobKey) == 0 then return 0 end
redis.call("PERSIST", jobKey)                                                       -- it may have had a retention TTL
redis.call("HSET", jobKey, "attempts", 0, "stalled", 0, "state", "waiting")
redis.call("LPUSH", KEYS[2], ARGV[2])
return 1
```

```ts
async function replayDead(ids: string[], ratePerSec = 20) {
  for (const id of ids) {
    await redis.qRetryDead(k.dead, k.wait, k.jobPrefix, id);
    await sleep(1000 / ratePerSec);                                                  // don't re-flood a recovering dependency
  }
}
```

Replay rules:

- **Fix the cause first.** Replaying into the same bug re-kills the jobs
- Replay **gradually** (a rate limit), because a dead backlog replayed all at once is a retry storm
- The job keeps its **original id**, so [idempotency](../10_streams/04_stream-patterns.md#idempotent-consumers) still protects against double effects
- Provide **bulk replay filtered by error text** and **per-job replay** in an admin tool

### Purge and retention

Dead jobs are kept for diagnosis, not forever:

```ts
async function purgeDead(olderThanMs: number) {
  const cutoff = Date.now() - olderThanMs;
  const ids = await redis.zrangebyscore(k.dead, "-inf", cutoff, "LIMIT", 0, 1_000);
  if (!ids.length) return 0;
  await Promise.all(ids.map((id) => redis.unlink(k.jobPrefix + id)));
  await redis.zrem(k.dead, ...ids);
  return ids.length;
}
```

Run it on a schedule, and archive to durable storage first if you must keep a record. The `keepDeadSec` TTL on the job hash prevents leaks even if the cleanup job stops, and dangling ids in the dead set are skipped by `listDead` and removed by `purgeDead`.

### Alert on the dead set

| Signal | Action |
|--------|--------|
| Dead count **increasing** | A new failure mode |
| Oldest dead job **older than your SLA** | Nobody is looking |
| Dead **rate** spike | A dependency outage or a bad deploy |
| One error message dominating | One fix resolves many jobs |

Also consider a **per-queue** dead set (as here) versus a **shared** one. Per-queue is simpler to reason about and replay.

### PII and secrets

Dead jobs keep their **payloads and stack traces**. Don't log or display sensitive fields, truncate stored errors (as above), and apply the same retention and access controls as the original data.

## Idempotency, again

Retries mean a handler can run **after partially succeeding**. Design for it:

```ts
async function handler({ id, data }: JobContext<{ orderId: string }>) {
  const order = await orders.findById(data.orderId);
  if (order.receiptSentAt) return;                                  // already done: a retry is a no-op

  await mailer.send(order.email, receipt(order), { idempotencyKey: `receipt:${id}` });   // the provider dedupes too
  await orders.markReceiptSent(order.id);                           // record success
}
```

The three layers: **check state**, **pass an idempotency key** to external services, and **record completion**.

## Metrics

| Metric | Why |
|--------|-----|
| Attempts per job (histogram) | Many jobs needing 3+ attempts means an unhealthy dependency |
| Retries by error class | Which failures dominate |
| Time from enqueue to success | The user-visible latency, retries included |
| Dead rate and size | Unrecoverable work |
| `RetryAfter` deferrals | Rate-limit pressure |
| Stalled requeues | Crashes or blocked workers |

## Testing

```ts
it("retries a transient failure and then succeeds", async () => {
  const q = new JobQueue(redis, "r1");
  let calls = 0;
  const w = new JobWorker(redis, "r1", async () => { if (++calls < 3) throw new Error("boom"); },
    { pollMs: 5, backoff: { baseMs: 20, capMs: 50 } });
  await q.add("n", {}, { jobId: "a", maxAttempts: 5 });
  w.start();

  await waitFor(async () => (await q.counts()).wait + (await q.counts()).active + (await q.counts()).delayed === 0, 5_000);
  expect(calls).toBe(3);
  expect((await q.counts()).dead).toBe(0);
  await w.stop();
});

it("sends a permanent failure straight to the dead set", async () => {
  const q = new JobQueue(redis, "r2");
  let calls = 0;
  const w = new JobWorker(redis, "r2", async () => { calls++; throw new PermanentError("bad input"); }, { pollMs: 5 });
  await q.add("n", {}, { jobId: "b", maxAttempts: 5 });
  w.start();

  await waitFor(async () => (await q.counts()).dead === 1, 3_000);
  expect(calls).toBe(1);                                              // no retries
  await w.stop();
});

it("kills a crash loop after maxStalled", async () => {
  const k = queueKeys("r3");
  const q = new JobQueue(redis, "r3");
  await q.add("n", {}, { jobId: "c" });

  for (let i = 0; i < 3; i++) {                                       // each "worker" claims and dies
    await redis.qClaim(k.wait, k.active, k.jobPrefix, 20, `ghost-${i}`);
    await sleep(40);
    await redis.qStalled(k.wait, k.active, k.dead, k.jobPrefix, 10, 2);
  }
  expect(await redis.zcard(k.dead)).toBe(1);
});

it("replays a dead job back to waiting", async () => {
  const k = queueKeys("r4");
  /* ...fail a job into the dead set, then: */
  expect(await redis.qRetryDead(k.dead, k.wait, k.jobPrefix, "d")).toBe(1);
  expect(await redis.hget(k.jobPrefix + "d", "attempts")).toBe("0");
});

it("backoff stays within its bounds", () => {
  for (let attempt = 1; attempt <= 12; attempt++) {
    const d = backoffMs(attempt, { baseMs: 100, capMs: 5_000 });
    expect(d).toBeGreaterThanOrEqual(0);
    expect(d).toBeLessThanOrEqual(5_000);
  }
});
```

Use short backoffs in tests so they finish quickly, and keep `waitFor` timeouts generous to avoid flakiness.

## Pitfalls

| Pitfall | Fix |
|---------|-----|
| Retrying everything the same way | Classify: transient, rate-limited, permanent |
| Retrying permanent failures | `PermanentError` goes straight to the dead set |
| Fixed delays with no jitter | Exponential backoff **with jitter** |
| Retry window shorter than a typical outage | Larger cap and attempts, or the pause pattern |
| Burning attempts during a known outage | `RetryAfterError`, a circuit breaker and pausing |
| Non-idempotent handlers | State checks, provider idempotency keys, recorded completion |
| A dead set nobody reads | Alerts on size, age and rate, and an owner |
| Replaying before fixing the cause | Fix, then replay gradually |
| Replaying all at once | A rate-limited replay |
| Storing huge stack traces and sensitive payloads forever | Truncate, and set retention |
| Treating a crash loop as ordinary failure | `stalled` counter and `maxStalled` |
| Same retry policy for every job type | Per-job-type attempts and backoff |

## Key takeaways

- **Classify** failures: retry the transient, defer the rate-limited, fail fast on the permanent
- Use **exponential backoff with jitter**, and make retries just **delayed jobs**
- Make `fail`, `defer` and `release` **atomic and ownership-checked** in Lua
- The **dead-letter set** is an operational tool: inspect, group, fix, **replay gradually**, then purge
- Protect struggling dependencies with pausing, rate limits and concurrency caps
- Everything is **at-least-once**, so handlers must be **idempotent**

**Next:** [BullMQ](./04_bullmq.md)
