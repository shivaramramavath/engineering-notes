# Queue Fundamentals

## Why queues

| Without a queue | With a queue |
|-----------------|--------------|
| The request waits for the email, the PDF, the image resize | The request enqueues a job and returns `202` in milliseconds |
| A spike of traffic overloads the slow dependency | The queue absorbs the spike, workers drain it at a safe rate |
| A transient failure fails the user's request | The job retries in the background |
| Work is lost if the process restarts mid-task | The job is durable in Redis and picked up again |

Typical jobs: sending email or SMS, generating reports and exports, image and video processing, webhooks to third parties, syncing data, cleanup and housekeeping.

## Anatomy of a job

```ts
{
  id: "welcome:42",              // unique, also the idempotency key
  name: "send-welcome-email",    // which handler
  data: { userId: 42 },          // the payload: small, serializable
  attempts: 1,                   // how many times it has been claimed
  maxAttempts: 5,
  state: "active",               // waiting | active | delayed | completed | failed
  createdAt, startedAt, lastError, ...
}
```

Payload design:

- Put **IDs, not objects**, in the payload (`{ userId: 42 }`, not the whole user). The worker loads fresh data, so it can't act on stale copies
- Keep it **small** (kilobytes). Store big blobs elsewhere and pass a reference
- Make it **JSON-serializable** and **versioned** if the shape will change
- Give every job a **stable ID** when duplicates would be harmful (`welcome:42`), so adding it twice does nothing

## Choosing a Redis structure

| Structure | Strength | Weakness |
|-----------|----------|----------|
| **List** (`LPUSH` and `BRPOP`) | Simplest, very fast | A popped job is gone if the worker dies (at-most-once) |
| **List + processing list** (`LMOVE`) | At-least-once | Needs a reaper, and no delays or priorities |
| **Sorted set** | Delays and priorities (score = time or priority) | Poll-based, needs a promoter |
| **Stream + consumer group** | Durable, acknowledged, replayable ([Streams](../10_streams/README.md)) | No built-in delays or retry backoff |
| **Lists + sorted sets + hashes + Lua** (BullMQ's approach) | The full feature set | Complex to build correctly, so use the library |

The queue below combines a **list** (waiting), a **sorted set** (active leases, delayed, dead) and a **hash per job**, with every transition done in Lua.

## Data layout

All keys of one queue share a **hash tag**, so they live in one Cluster slot and scripts can touch them together:

```
shop:{q:emails}:wait       list     job ids, FIFO (LPUSH in, RPOP out)
shop:{q:emails}:active     zset     job id → lease deadline (ms)
shop:{q:emails}:delayed    zset     job id → run-at time (ms)      (lesson 02)
shop:{q:emails}:dead       zset     job id → failed-at time (ms)   (lesson 03)
shop:{q:emails}:job:<id>   hash     name, data, attempts, state, worker, ...
```

```ts
// src/queue/keys.ts
import { KeyBuilder } from "../redis/key-builder.js";
const K = KeyBuilder.create("shop");

export function queueKeys(name: string) {
  const t = (s: string) => K.tagged(["q", name], s);
  return { wait: t("wait"), active: t("active"), delayed: t("delayed"), dead: t("dead"), jobPrefix: t("job") + ":" };
}
```

The scripts receive the `jobPrefix` as an argument and build per-job keys inside the script. They share the hash tag, so they are in the same slot. (BullMQ does the same.) This is the one deliberate bend of the "declare every key" rule from [Lua Scripts](../06_advanced-commands/03_lua-scripts.md#rules-and-limits).

## The core scripts

### Enqueue

```lua
-- enqueue.lua
-- KEYS[1] = wait, KEYS[2] = delayed
-- ARGV: 1 jobPrefix, 2 id, 3 name, 4 data (json), 5 maxAttempts, 6 delayMs (0 = now)
local jobKey = ARGV[1] .. ARGV[2]
if redis.call("EXISTS", jobKey) == 1 then return 0 end              -- duplicate job id: ignore

local t = redis.call("TIME")
local now = tonumber(t[1]) * 1000 + math.floor(tonumber(t[2]) / 1000)

redis.call("HSET", jobKey, "name", ARGV[3], "data", ARGV[4], "attempts", 0, "stalled", 0,
           "maxAttempts", ARGV[5], "createdAt", now)

local delay = tonumber(ARGV[6])
if delay > 0 then
  redis.call("HSET", jobKey, "state", "delayed")
  redis.call("ZADD", KEYS[2], now + delay, ARGV[2])                 -- lesson 02
else
  redis.call("HSET", jobKey, "state", "waiting")
  redis.call("LPUSH", KEYS[1], ARGV[2])
end
return 1
```

Because creation, state and queue insertion happen in one script, there's no moment where a job id is in the list without its hash, or the reverse.

### Claim (take a job with a lease)

```lua
-- claim.lua
-- KEYS[1] = wait, KEYS[2] = active
-- ARGV: 1 jobPrefix, 2 leaseMs, 3 workerId
local t = redis.call("TIME")
local now = tonumber(t[1]) * 1000 + math.floor(tonumber(t[2]) / 1000)

while true do
  local id = redis.call("RPOP", KEYS[1])
  if not id then return false end                                    -- queue empty

  local jobKey = ARGV[1] .. id
  if redis.call("EXISTS", jobKey) == 1 then                          -- skip ids whose job was removed
    redis.call("ZADD", KEYS[2], now + tonumber(ARGV[2]), id)         -- the LEASE
    local attempts = redis.call("HINCRBY", jobKey, "attempts", 1)
    redis.call("HSET", jobKey, "state", "active", "worker", ARGV[3], "startedAt", now)
    return { id,
             redis.call("HGET", jobKey, "name"),
             redis.call("HGET", jobKey, "data"),
             attempts,
             redis.call("HGET", jobKey, "maxAttempts") }
  end
end
```

The pop and the lease creation are **atomic**. There's no window where a job is "claimed but has no lease", which is the weakness of `BRPOP` followed by a separate `ZADD`.

### Acknowledge, and extend the lease

```lua
-- ack.lua
-- KEYS[1] = active
-- ARGV: 1 jobPrefix, 2 id, 3 workerId, 4 keepCompletedSec (0 = delete the job)
local jobKey = ARGV[1] .. ARGV[2]
if redis.call("HGET", jobKey, "worker") ~= ARGV[3] then return 0 end      -- someone else owns it now
if redis.call("ZREM", KEYS[1], ARGV[2]) == 0 then return 0 end            -- the lease was already reaped

if tonumber(ARGV[4]) <= 0 then
  redis.call("DEL", jobKey)
else
  local t = redis.call("TIME")
  redis.call("HSET", jobKey, "state", "completed", "finishedAt", tonumber(t[1]) * 1000)
  redis.call("EXPIRE", jobKey, tonumber(ARGV[4]))                          -- keep briefly for inspection, then vanish
end
return 1
```

```lua
-- extend.lua  (heartbeat)
-- KEYS[1] = active; ARGV: 1 jobPrefix, 2 id, 3 workerId, 4 leaseMs
if redis.call("HGET", ARGV[1] .. ARGV[2], "worker") ~= ARGV[3] then return 0 end
if redis.call("ZSCORE", KEYS[1], ARGV[2]) == false then return 0 end
local t = redis.call("TIME")
local now = tonumber(t[1]) * 1000 + math.floor(tonumber(t[2]) / 1000)
redis.call("ZADD", KEYS[1], "XX", now + tonumber(ARGV[4]), ARGV[2])
return 1
```

Both check the **worker ID**. Without that, a slow worker whose lease was reaped and whose job was re-claimed by another worker could acknowledge **the other worker's** attempt. This is the same ownership rule as [lock release](../11_distributed-locks/01_locking-fundamentals.md#release-compare-the-token-then-delete).

### The stalled-job reaper

A worker that crashes never acknowledges, so its lease simply **expires**. The reaper finds expired leases and puts the jobs back:

```lua
-- stalled.lua
-- KEYS[1] = wait, KEYS[2] = active, KEYS[3] = dead
-- ARGV: 1 jobPrefix, 2 batch limit, 3 maxStalled
local t = redis.call("TIME")
local now = tonumber(t[1]) * 1000 + math.floor(tonumber(t[2]) / 1000)

local ids = redis.call("ZRANGEBYSCORE", KEYS[2], "-inf", now, "LIMIT", 0, tonumber(ARGV[2]))
local requeued = 0

for _, id in ipairs(ids) do
  if redis.call("ZREM", KEYS[2], id) == 1 then                           -- exactly one reaper wins each job
    local jobKey = ARGV[1] .. id
    if redis.call("EXISTS", jobKey) == 1 then
      local stalled = redis.call("HINCRBY", jobKey, "stalled", 1)
      if stalled > tonumber(ARGV[3]) then
        redis.call("HSET", jobKey, "state", "failed", "lastError", "stalled too many times", "failedAt", now)
        redis.call("ZADD", KEYS[3], now, id)                             -- a crash loop ends in the dead set
      else
        redis.call("HSET", jobKey, "state", "waiting")
        redis.call("RPUSH", KEYS[1], id)                                 -- the consuming end: retried first
        requeued = requeued + 1
      end
    end
  end
end
return requeued
```

Several workers can run the reaper at once. `ZREM` returns `1` for only one of them, so each stalled job is requeued exactly once, and no leader election is needed.

## The producer

```ts
// src/queue/job-queue.ts
import { randomUUID } from "node:crypto";
import type { Redis } from "ioredis";
import { queueKeys } from "./keys.js";

export interface AddOptions {
  jobId?: string;          // dedupe key: adding an existing id is a no-op
  maxAttempts?: number;    // default 5
  delayMs?: number;        // lesson 02
}

export class JobQueue {
  readonly k;
  constructor(private redis: Redis, readonly name: string) { this.k = queueKeys(name); }

  async add<T>(jobName: string, data: T, o: AddOptions = {}) {
    const id = o.jobId ?? randomUUID();
    const created = await this.redis.qEnqueue(
      this.k.wait, this.k.delayed, this.k.jobPrefix,
      id, jobName, JSON.stringify(data), o.maxAttempts ?? 5, o.delayMs ?? 0
    );
    return { id, duplicate: created === 0 };
  }

  async counts() {
    const [wait, active, delayed, dead] = await Promise.all([
      this.redis.llen(this.k.wait),
      this.redis.zcard(this.k.active),
      this.redis.zcard(this.k.delayed),
      this.redis.zcard(this.k.dead),
    ]);
    return { wait, active, delayed, dead };
  }
}
```

Register the scripts and their types once:

```ts
declare module "ioredis" {
  interface RedisCommander<Context> {
    qEnqueue(wait: string, delayed: string, jobPrefix: string, id: string, name: string, data: string, maxAttempts: number, delayMs: number): Promise<number>;
    qClaim(wait: string, active: string, jobPrefix: string, leaseMs: number, workerId: string): Promise<[string, string, string, number, string] | null>;
    qAck(active: string, jobPrefix: string, id: string, workerId: string, keepSec: number): Promise<number>;
    qExtend(active: string, jobPrefix: string, id: string, workerId: string, leaseMs: number): Promise<number>;
    qStalled(wait: string, active: string, dead: string, jobPrefix: string, limit: number, maxStalled: number): Promise<number>;
  }
}

export function registerQueueCommands(redis: Redis) {
  redis.defineCommand("qEnqueue", { numberOfKeys: 2, lua: lua("enqueue") });
  redis.defineCommand("qClaim",   { numberOfKeys: 2, lua: lua("claim") });
  redis.defineCommand("qAck",     { numberOfKeys: 1, lua: lua("ack") });
  redis.defineCommand("qExtend",  { numberOfKeys: 1, lua: lua("extend") });
  redis.defineCommand("qStalled", { numberOfKeys: 3, lua: lua("stalled") });
}
```

(See [Typed Redis Client](../08_nodejs-integration/04_typed-redis-client.md#typing-custom-commands-lua) for the typing notes.)

### Idempotent adds

```ts
await queue.add("send-welcome", { userId: 42 }, { jobId: "welcome:42" });
await queue.add("send-welcome", { userId: 42 }, { jobId: "welcome:42" });   // { duplicate: true }, nothing enqueued
```

A custom `jobId` deduplicates **for as long as the job's hash exists** (waiting, active, delayed, dead, and completed jobs for `keepCompletedSec`). Once it is deleted, the same id can be added again. Choose the retention deliberately.

## The worker

A worker runs several concurrent loops, each doing: **claim, heartbeat while processing, acknowledge or fail**.

```ts
// src/queue/job-worker.ts
import os from "node:os";
import { randomUUID } from "node:crypto";
import type { Redis } from "ioredis";
import { queueKeys } from "./keys.js";

export interface JobContext<T = unknown> {
  id: string;
  name: string;
  data: T;
  attempt: number;
  maxAttempts: number;
  signal: AbortSignal;          // aborted on timeout, lease loss, or forced shutdown
}

export interface WorkerOptions {
  concurrency?: number;         // default 1
  leaseMs?: number;             // default 30_000
  pollMs?: number;              // idle poll interval, default 250
  jobTimeoutMs?: number;        // 0 = none
  keepCompletedSec?: number;    // default 0 (delete)
  reapEveryMs?: number;         // default 10_000
  maxStalled?: number;          // default 2
  shutdownGraceMs?: number;     // default 30_000
}

const sleep = (ms: number) => new Promise((r) => setTimeout(r, ms));

export class JobWorker<T = unknown> {
  private k;
  private running = false;
  private loops: Promise<void>[] = [];
  private hardStop = new AbortController();
  readonly workerId = `${os.hostname()}-${process.pid}-${randomUUID().slice(0, 8)}`;

  constructor(
    private redis: Redis,                                       // normal commands (polling needs no blocking connection)
    private queueName: string,
    private handler: (job: JobContext<T>) => Promise<void>,
    private o: WorkerOptions = {},
    private log: { info: (...a: any[]) => void; warn: (...a: any[]) => void; error: (...a: any[]) => void } = console
  ) {
    this.k = queueKeys(queueName);
  }

  start() {
    this.running = true;
    const n = this.o.concurrency ?? 1;
    for (let i = 0; i < n; i++) this.loops.push(this.consumeLoop());
    this.loops.push(this.reapLoop());
  }

  /** Stop claiming, let running jobs finish, abort the rest after the grace period. */
  async stop() {
    this.running = false;
    const timer = setTimeout(() => this.hardStop.abort(new Error("shutdown grace period exceeded")), this.o.shutdownGraceMs ?? 30_000);
    await Promise.allSettled(this.loops);
    clearTimeout(timer);
  }

  private async consumeLoop() {
    let idle = this.o.pollMs ?? 250;
    while (this.running) {
      try {
        const claimed = await this.redis.qClaim(this.k.wait, this.k.active, this.k.jobPrefix, this.o.leaseMs ?? 30_000, this.workerId);
        if (!claimed) {
          await sleep(idle + Math.random() * idle);             // jitter, so idle workers don't poll in lockstep
          idle = Math.min(idle * 1.5, 2_000);                   // back off while empty (bounded)
          continue;
        }
        idle = this.o.pollMs ?? 250;
        await this.runJob(claimed);
      } catch (err) {
        this.log.error({ err }, "worker loop error");
        await sleep(1_000);
      }
    }
  }

  private async runJob([id, name, rawData, attempt, maxAttempts]: [string, string, string, number, string]) {
    const leaseMs = this.o.leaseMs ?? 30_000;
    const leaseLost = new AbortController();

    // heartbeat: keep the lease alive while the handler works
    const hb = setInterval(async () => {
      try {
        const ok = await this.redis.qExtend(this.k.active, this.k.jobPrefix, id, this.workerId, leaseMs);
        if (ok !== 1) leaseLost.abort(new Error("lease lost"));
      } catch { /* a transient Redis error: the next beat retries */ }
    }, Math.max(100, Math.floor(leaseMs / 3)));

    const signals = [leaseLost.signal, this.hardStop.signal];
    if (this.o.jobTimeoutMs) signals.push(AbortSignal.timeout(this.o.jobTimeoutMs));
    const signal = AbortSignal.any(signals);                    // Node 20.3+

    try {
      await this.handler({ id, name, data: JSON.parse(rawData), attempt, maxAttempts: Number(maxAttempts), signal });
      const acked = await this.redis.qAck(this.k.active, this.k.jobPrefix, id, this.workerId, this.o.keepCompletedSec ?? 0);
      if (acked !== 1) this.log.warn({ id }, "job finished but its lease was lost, it may run again");
    } catch (err) {
      // Lesson 03 adds explicit fail() with backoff and a dead-letter queue.
      // For now: leave the job active. Its lease expires and the reaper retries it.
      this.log.error({ err, id, attempt }, "job failed, the reaper will retry after the lease expires");
    } finally {
      clearInterval(hb);
    }
  }

  private async reapLoop() {
    const every = this.o.reapEveryMs ?? 10_000;
    while (this.running) {
      for (let waited = 0; this.running && waited < every; waited += 250) await sleep(250);
      if (!this.running) break;
      try {
        const n = await this.redis.qStalled(this.k.wait, this.k.active, this.k.dead, this.k.jobPrefix, 100, this.o.maxStalled ?? 2);
        if (n > 0) this.log.warn({ requeued: n }, "requeued stalled jobs");
      } catch (err) {
        this.log.error({ err }, "reaper failed");
      }
    }
  }
}
```

Use it:

```ts
const worker = new JobWorker<{ userId: number }>(redis, "emails", async ({ data, id, signal }) => {
  const user = await users.findById(data.userId);
  await mailer.send(user.email, "Welcome!", { idempotencyKey: id, signal });    // the job id doubles as the idempotency key
}, { concurrency: 5, leaseMs: 30_000, jobTimeoutMs: 60_000 });

worker.start();
process.on("SIGTERM", () => worker.stop());
```

Design points:

| Choice | Reason |
|--------|--------|
| **Lease** with heartbeat | A crashed worker's jobs return automatically, and a slow-but-alive worker keeps its job |
| **Ownership check** on ack and extend | A reaped, re-claimed job can't be acknowledged by the old worker |
| **Polling with backoff and jitter** | Claim is a single atomic Lua call. A blocking pop can't be combined with the lease atomically, so we poll, bounded to roughly 2 s idle |
| Concurrency as N loops in one process | Simple, and bounded by `concurrency` |
| `AbortSignal` passed to the handler | Timeouts, lease loss and shutdown can cancel in-flight I/O |
| Handler error leaves the job to the reaper | A deliberately simple retry. Lesson 03 replaces it with backoff and a DLQ |
| Reaper runs in every worker | Safe: `ZREM` makes each stalled job get requeued once |

### Blocking versus polling

`BRPOP` gives lower idle latency, at the cost of atomicity with the lease. A common refinement keeps Lua for the claim and uses a **wake-up signal**: producers `LPUSH` a marker onto a tiny `wake` list, and idle workers `BRPOP` that marker (with a timeout) instead of sleeping. BullMQ does something similar internally. The polling version above is easier to get right and fine for most workloads.

## Concurrency, ordering and fairness

- **Concurrency** is per worker process (`concurrency`), so total parallelism is `processes × concurrency`. Size it to the **downstream** bottleneck (database pool, API limits), not to your CPU count
- **Ordering** is roughly FIFO but **not guaranteed**: retries, several workers and delays reorder. If order matters for one entity, route that entity to a single partition or queue with concurrency 1, or serialize with a [lock](../11_distributed-locks/README.md)
- **Fairness**: one noisy producer can fill a shared queue. Use separate queues per tenant or priority class, or rate limit at enqueue time ([Rate Limiting](../12_rate-limiting/README.md))

## Graceful shutdown

```
SIGTERM → stop claiming → wait for active jobs (up to the grace period) → abort the rest → exit
```

Jobs aborted by the hard stop (or killed outright) keep their lease and are **recovered by the reaper** after it expires. Keep `shutdownGraceMs` shorter than your platform's termination grace period ([Graceful Shutdown](../03_ioredis-basics/06_graceful-shutdown.md)).

## Observability

| Metric | Signal |
|--------|--------|
| `wait` length (queue depth) | Producers outpace workers |
| **Age of the oldest waiting job** | The user-visible delay (store `createdAt`, compare) |
| `active` count versus `concurrency × workers` | Saturation |
| Jobs completed, failed, retried per minute | Throughput and health |
| Processing duration per job name | Regressions |
| `stalled` requeues | Crashes, CPU-blocked workers, leases too short |
| `dead` count | Jobs needing attention (lesson 03) |

```ts
const oldest = await redis.lindex(k.wait, -1);                            // the next job to be claimed
const createdAt = oldest ? Number(await redis.hget(k.jobPrefix + oldest, "createdAt")) : null;
const waitingForMs = createdAt ? Date.now() - createdAt : 0;
```

Alert on **queue age**, not just depth, because a deep queue that drains quickly is healthy.

## Testing

```ts
it("never gives one job to two workers", async () => {
  const q = new JobQueue(redis, "t1");
  for (let i = 0; i < 100; i++) await q.add("n", { i }, { jobId: `j${i}` });

  const seen = new Map<string, number>();
  const workers = Array.from({ length: 5 }, () =>
    new JobWorker(redis, "t1", async ({ id }) => { seen.set(id, (seen.get(id) ?? 0) + 1); }, { concurrency: 4, pollMs: 5 })
  );
  workers.forEach((w) => w.start());
  await waitFor(async () => (await q.counts()).wait === 0 && (await q.counts()).active === 0, 5_000);
  await Promise.all(workers.map((w) => w.stop()));

  expect(seen.size).toBe(100);
  expect([...seen.values()].every((n) => n === 1)).toBe(true);
});

it("recovers a job from a worker that died", async () => {
  const q = new JobQueue(redis, "t2");
  await q.add("n", {}, { jobId: "x" });

  const k = queueKeys("t2");
  await redis.qClaim(k.wait, k.active, k.jobPrefix, 50, "ghost-worker");     // claimed, and never acknowledged
  await sleep(120);                                                          // the lease expires
  expect(await redis.qStalled(k.wait, k.active, k.dead, k.jobPrefix, 10, 2)).toBe(1);
  expect(await redis.llen(k.wait)).toBe(1);                                  // back in the queue
});

it("rejects an ack from a worker that lost the job", async () => {
  const k = queueKeys("t3");
  const q = new JobQueue(redis, "t3");
  await q.add("n", {}, { jobId: "y" });
  await redis.qClaim(k.wait, k.active, k.jobPrefix, 50, "A");
  await sleep(120);
  await redis.qStalled(k.wait, k.active, k.dead, k.jobPrefix, 10, 2);
  await redis.qClaim(k.wait, k.active, k.jobPrefix, 5_000, "B");
  expect(await redis.qAck(k.active, k.jobPrefix, "y", "A", 0)).toBe(0);      // A's late ack is ignored
  expect(await redis.qAck(k.active, k.jobPrefix, "y", "B", 0)).toBe(1);
});
```

Use a real Redis and a unique queue name per test.

## When to stop here and use BullMQ

This queue is good for learning and for modest needs. The moment you want **priorities, rate limiting, parent/child flows, a dashboard, repeatable schedules or battle-tested edge cases**, switch to [BullMQ](./04_bullmq.md), which implements these ideas, and many more, with years of production hardening.

## Pitfalls

| Pitfall | Fix |
|---------|-----|
| `BRPOP` then process, with no lease | A lease plus acknowledgement (at-least-once) |
| Claim and lease in two commands | One Lua script |
| Acknowledging without an ownership check | Compare the worker ID |
| No heartbeat for long jobs | Extend the lease at one third of its length |
| Lease shorter than the longest GC pause or stall | Longer leases, and idempotent handlers |
| CPU-heavy work blocking the heartbeat | Worker threads or a separate process |
| Large payloads in the job hash | IDs and references |
| Assuming exactly-once | Idempotent handlers, with the job id as the idempotency key |
| Unlimited job retention | `keepCompletedSec`, and a cap on failed jobs |
| Watching queue depth only | Alert on the **age** of the oldest job |
| Everything in one queue | Separate queues by priority, tenant or workload |

## Key takeaways

- A reliable queue is **claim with a lease, acknowledge after success, reap expired leases**, all as atomic Lua transitions
- Delivery is **at-least-once**, so handlers must be idempotent, and the job id is a natural idempotency key
- Check **ownership** on every worker-side transition, and heartbeat long jobs
- Size concurrency to downstream limits, alert on queue age, and shut down gracefully

**Next:** [Delayed Jobs](./02_delayed-jobs.md)
