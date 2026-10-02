# BullMQ

Lessons 01 to 03 built a queue from first principles. **BullMQ** is the production-grade library that implements those ideas (leases, delays, retries, dead jobs, atomic Lua transitions) and adds priorities, rate limiting, flows, schedulers, events and dashboards. It is built on **ioredis** and Redis.

> Option names, defaults and some APIs change between BullMQ releases. This lesson reflects BullMQ v5. Check the [official documentation](https://docs.bullmq.io) for the version you install, especially for job schedulers, global concurrency and flow options.

## What you get versus the DIY queue

| Concept in lessons 01 to 03 | In BullMQ |
|-----------------------------|-----------|
| `add` with `jobId` dedupe | `queue.add(name, data, { jobId })` |
| Lease, heartbeat, reaper | Worker **lock** renewal, `stalledInterval`, `maxStalledCount` |
| Delayed jobs and the promoter | `delay` option, handled automatically |
| `fail` with backoff, attempts | `attempts` and `backoff` options |
| Dead-letter set | **Failed** set (kept by `removeOnFail`) |
| Replay | `job.retry()`, `queue.retryJobs()` |
| Rate-limited dependency | Worker `limiter` |
| Pause flag | `queue.pause()` and `queue.resume()` |
| Recurring jobs | **Job schedulers** (`upsertJobScheduler`) |
| Nothing | Priorities, **flows** (parent/child), events, sandboxed processors, dashboards |

## Install and connect

```bash
npm install bullmq ioredis
```

```ts
// src/queue/connection.ts
export const connection = {
  host: process.env.REDIS_HOST ?? "127.0.0.1",
  port: Number(process.env.REDIS_PORT ?? 6379),
  password: process.env.REDIS_PASSWORD,
  maxRetriesPerRequest: null,        // REQUIRED for workers: BullMQ throws if it is not null
};
```

Rules about connections:

| Rule | Why |
|------|-----|
| Pass **options** (as above), or an ioredis instance | BullMQ creates and duplicates connections itself where it needs a separate blocking one |
| Workers and `QueueEvents` need `maxRetriesPerRequest: null` | Their blocking commands must not time out client-side |
| For **producers inside HTTP handlers**, use a connection with `enableOfflineQueue: false` | `queue.add` **fails fast** during a Redis outage instead of hanging requests |
| Each `Worker` uses extra connections (one blocking) | Budget connections: instances × workers ([Connection Management](../08_nodejs-integration/01_connection-management.md)) |
| Attach an **`error` listener** to queues, workers and events | An unhandled `error` event is silent or fatal |

```ts
const producerConnection = { ...connection, maxRetriesPerRequest: 1, enableOfflineQueue: false };
```

### Redis configuration for BullMQ

- Use **`maxmemory-policy noeviction`**. BullMQ logs a warning otherwise, because evicting its keys corrupts queues ([Memory and Eviction](../02_redis-fundamentals/05_memory-and-eviction.md))
- Use **persistence** (AOF `everysec` plus replicas), because queued jobs are real data ([Persistence](../02_redis-fundamentals/04_persistence.md))
- Keep queues on **their own Redis** (or logical role) if caches share the instance, so cache eviction policies never touch jobs
- In **Cluster**, set a hash-tagged `prefix` so all of a queue's keys share a slot: `prefix: "{shop}"`

## A queue and a worker

```ts
import { Queue, Worker, UnrecoverableError, type Job } from "bullmq";
import { connection } from "./connection.js";

export interface EmailJob { userId: number; template: "welcome" | "receipt" }

export const emailQueue = new Queue<EmailJob>("emails", {
  connection: producerConnection,
  prefix: "{shop}",                                       // optional, recommended for Cluster
  defaultJobOptions: {
    attempts: 5,
    backoff: { type: "exponential", delay: 1_000 },       // 1s, 2s, 4s, 8s ...
    removeOnComplete: { age: 3_600, count: 1_000 },       // keep up to 1000 for an hour
    removeOnFail: { age: 7 * 24 * 3_600 },                // keep failures a week for diagnosis
  },
});

emailQueue.on("error", (err) => logger.error({ err }, "email queue error"));
```

```ts
// producer
await emailQueue.add("send", { userId: 42, template: "welcome" }, {
  jobId: "welcome:42",              // dedupe: the same id while the job exists is ignored
  delay: 5_000,                     // run in 5 seconds
  priority: 1,                      // lower number = higher priority (1 is the highest)
});
```

```ts
// worker (usually its own process)
export const emailWorker = new Worker<EmailJob>(
  "emails",
  async (job: Job<EmailJob>) => {
    const user = await users.findById(job.data.userId);
    if (!user) throw new UnrecoverableError("user deleted");              // do not retry

    await job.updateProgress(10);
    await mailer.send(user.email, job.data.template, { idempotencyKey: job.id });
    await job.updateProgress(100);

    return { sentAt: Date.now() };                                        // stored as job.returnvalue
  },
  {
    connection,
    prefix: "{shop}",
    concurrency: 10,
    limiter: { max: 50, duration: 1_000 },                                // at most 50 jobs per second, across all workers
  }
);

emailWorker.on("completed", (job) => logger.info({ id: job.id }, "email sent"));
emailWorker.on("failed", (job, err) => logger.warn({ id: job?.id, err, attempt: job?.attemptsMade }, "email failed"));
emailWorker.on("error", (err) => logger.error({ err }, "email worker error"));      // required
```

## Job options that matter

| Option | Meaning | Notes |
|--------|---------|-------|
| `attempts` | Total tries | Default is 1 (**no retries** unless you set it) |
| `backoff` | `{ type: "fixed" \| "exponential", delay }` | Add jitter (below) |
| `delay` | Milliseconds before the job becomes runnable | For far-future schedules, keep them in your database |
| `priority` | 1 (highest) and up | Prioritized jobs cost more than FIFO ones, so use sparingly |
| `jobId` | Custom unique id | Dedupe. Reusing an id of an existing job is ignored |
| `lifo` | Process newest first | Rare |
| `removeOnComplete` / `removeOnFail` | `true`, a count, or `{ age, count }` | **Set these.** Otherwise finished jobs accumulate in Redis forever |
| `deduplication` | Time-boxed deduplication (newer versions) | Check the docs for your version |

### Retries and backoff

```ts
// jitter and a cap via a custom strategy, defined on the Worker
new Worker("emails", processor, {
  connection,
  settings: {
    backoffStrategy: (attemptsMade, type, err, job) => {
      if (type === "custom") {
        const exp = Math.min(5 * 60_000, 1_000 * 2 ** (attemptsMade - 1));
        return Math.floor(exp / 2 + Math.random() * (exp / 2));          // equal jitter
      }
      return -1;                                                          // -1 = fall back to the built-in types
    },
  },
});

await emailQueue.add("send", payload, { attempts: 8, backoff: { type: "custom" } });
```

Error handling inside processors:

```ts
import { UnrecoverableError } from "bullmq";

throw new UnrecoverableError("validation failed");     // fail now, no more retries
throw new Error("provider 503");                       // any other error: retried per `attempts` and `backoff`
```

Rate-limit responses from a provider can be honored with the worker's manual rate-limit API (`worker.rateLimit(ms)` followed by throwing the library's rate-limit error), which defers jobs **without consuming attempts**. This is the equivalent of `RetryAfterError` in lesson 03, so see the docs for the exact calls in your version.

## Concurrency and rate limits

| Control | Where | Effect |
|---------|-------|--------|
| `concurrency` | Worker option | Parallel jobs **per worker process** |
| `limiter: { max, duration }` | Worker option | A **queue-wide** rate limit across all workers |
| Global concurrency | `queue.setGlobalConcurrency(n)` in recent versions | A cap across the whole queue, whatever the worker count |
| Several queues | Separate `Queue`s | Isolation: a slow workload can't starve a fast one |

Size concurrency to the **downstream** bottleneck (database pool, partner API limits), not to CPU count.

## Delayed and scheduled jobs

```ts
await queue.add("reminder", { orderId }, { delay: 24 * 3_600_000, jobId: `reminder:${orderId}` });

const job = await queue.getJob(`reminder:${orderId}`);
if (job && (await job.getState()) === "delayed") await job.remove();      // cancel (when still pending)
```

### Recurring jobs: job schedulers

Recent versions replace the old `repeat` option with **job schedulers**: one idempotent definition, created or updated with `upsertJobScheduler`, with no "every instance fires" problem:

```ts
await queue.upsertJobScheduler(
  "daily-digest",                                         // scheduler id, upserted so deploys are idempotent
  { pattern: "0 9 * * *", tz: "Asia/Kolkata" },           // cron pattern and time zone (or { every: 60_000 })
  { name: "send-digest", data: {}, opts: { attempts: 3, removeOnComplete: true } }
);
```

Call it at **startup from every instance**. Because it upserts, running it many times is safe. If you are on an older BullMQ version, use the `repeat` option and check its documentation.

## Flows: parent and child jobs

A **parent** waits until all **children** finish, which is ideal for fan-out and fan-in (fetch many sources, then build a report):

```ts
import { FlowProducer } from "bullmq";

const flow = new FlowProducer({ connection: producerConnection });

await flow.add({
  name: "build-report",
  queueName: "reports",
  data: { reportId: 7 },
  children: [
    { name: "fetch-sales",   queueName: "fetch", data: { source: "sales" } },
    { name: "fetch-returns", queueName: "fetch", data: { source: "returns" } },
  ],
});
```

```ts
// the parent's processor reads its children's return values
new Worker("reports", async (job) => {
  const results = await job.getChildrenValues();            // { "<childKey>": returnvalue, ... }
  return buildReport(job.data.reportId, results);
}, { connection });
```

Flows are powerful, and the failure semantics (what happens to the parent when a child fails) are configurable, so read the flow documentation before relying on them.

## Events and request/response

`Worker` events fire in the **worker's process**. To observe jobs from **anywhere** (an API process, a dashboard), use `QueueEvents`:

```ts
import { QueueEvents } from "bullmq";

const events = new QueueEvents("emails", { connection });
events.on("completed", ({ jobId, returnvalue }) => { /* ... */ });
events.on("failed", ({ jobId, failedReason }) => { /* ... */ });
events.on("progress", ({ jobId, data }) => { /* ... */ });
events.on("error", (err) => logger.error({ err }, "queue events error"));
```

Waiting for a job's result inside a request is possible (`job.waitUntilFinished(events)`), but it ties up the request and reintroduces the coupling queues exist to remove. Prefer **`202 Accepted` plus polling or a push**:

```ts
app.post("/exports", async (req, res) => {
  const job = await exportQueue.add("export", { userId: req.user.id, filters: req.body }, { jobId: `export:${req.user.id}:${hash(req.body)}` });
  res.status(202).location(`/exports/${job.id}`).json({ id: job.id });
});

app.get("/exports/:id", async (req, res) => {
  const job = await exportQueue.getJob(req.params.id);
  if (!job) return res.status(404).end();
  res.json({
    state: await job.getState(),                    // waiting | active | delayed | completed | failed
    progress: job.progress,
    result: job.returnvalue ?? null,
    error: job.failedReason ?? null,
  });
});
```

For live updates, have the worker push the result to the browser through the Socket.IO emitter ([Socket.IO with Redis](../09_pub-sub/03_socketio-with-redis.md#emitting-from-outside-the-socket-servers)).

## Stalled jobs

Workers hold a **lock** on each active job and renew it. If a worker dies or its event loop is blocked past the lock duration, the job is considered **stalled** and moved back to waiting (or failed after too many stalls).

| Option | Meaning | Default |
|--------|---------|---------|
| `lockDuration` | How long a lock lasts before it must be renewed | 30 s |
| `stalledInterval` | How often stalled jobs are checked for | 30 s |
| `maxStalledCount` | Stalls allowed before the job fails | 1 |

Consequences you must plan for:

- A stalled-and-requeued job **may run twice** (the original worker might still be running), so handlers must be idempotent
- **CPU-bound processors block the event loop**, stop lock renewal, and cause false stalls. Use **sandboxed processors** (a separate process or thread) for heavy work:

```ts
import path from "node:path";

new Worker("images", path.join(import.meta.dirname, "image-processor.js"), {
  connection,
  concurrency: 4,
  useWorkerThreads: true,                       // threads instead of child processes
});
```

## Graceful shutdown

```ts
async function shutdown() {
  await emailWorker.close();        // stop taking jobs, wait for active ones to finish
  await emailQueue.close();
  await events.close();
}

process.on("SIGTERM", () => void shutdown());
```

`worker.close()` waits for the active jobs, so keep job durations shorter than your platform's termination grace period, or pass `true` to force-close and let stalled-job recovery pick them up ([Graceful Shutdown](../03_ioredis-basics/06_graceful-shutdown.md)).

## Operating BullMQ

### Dashboard

[Bull Board](https://github.com/felixmosh/bull-board) gives a UI to inspect, retry and clean jobs:

```ts
import { createBullBoard } from "@bull-board/api";
import { BullMQAdapter } from "@bull-board/api/bullMQAdapter";
import { ExpressAdapter } from "@bull-board/express";

const serverAdapter = new ExpressAdapter();
serverAdapter.setBasePath("/admin/queues");

createBullBoard({ queues: [new BullMQAdapter(emailQueue)], serverAdapter });

app.use("/admin/queues", requireAdmin, serverAdapter.getRouter());     // ALWAYS behind authentication
```

Import paths vary by version, so check the Bull Board README. The dashboard can **retry, delete and view payloads**, so protect it like an admin panel.

### Health and metrics

```ts
const counts = await emailQueue.getJobCounts("waiting", "active", "delayed", "failed", "completed");
// { waiting: 12, active: 3, delayed: 40, failed: 1, completed: 900 }
```

| Metric | Why |
|--------|-----|
| `waiting` and **age of the oldest waiting job** | Backlog and user-visible delay |
| `active` versus `concurrency × workers` | Saturation |
| `failed` growth | The dead-letter equivalent (lesson 03) |
| Stalled events (`worker.on("stalled")`) | Crashes, blocked loops, leases too short |
| Job duration (`job.finishedOn - job.processedOn`) | Regressions |
| Redis memory and key counts | Retention misconfiguration |

### Housekeeping

```ts
await emailQueue.clean(24 * 3_600_000, 1_000, "completed");      // grace in ms, batch limit, state
await emailQueue.retryJobs({ state: "failed", count: 100 });      // replay failed jobs gradually (check the options in your version)
await emailQueue.pause();                                         // stop processing, keep accepting jobs
await emailQueue.resume();
await emailQueue.drain();                                         // remove waiting jobs (dangerous)
await emailQueue.obliterate({ force: true });                     // remove EVERYTHING (tests only)
```

Replay with the same rules as before: **fix the cause first, then replay gradually**.

## Typed queue definitions

Centralize queues so payload and result types are checked at every call site:

```ts
// src/queue/queues.ts
import { Queue, Worker, type Processor } from "bullmq";

export interface JobMap {
  emails:  { name: "send";   data: { userId: number; template: string }; result: { sentAt: number } };
  exports: { name: "export"; data: { userId: number; filters: unknown };  result: { url: string } };
}

export function makeQueue<K extends keyof JobMap>(name: K) {
  return new Queue<JobMap[K]["data"], JobMap[K]["result"], JobMap[K]["name"]>(name, { connection: producerConnection, defaultJobOptions });
}

export function makeWorker<K extends keyof JobMap>(name: K, processor: Processor<JobMap[K]["data"], JobMap[K]["result"], JobMap[K]["name"]>, concurrency = 5) {
  return new Worker(name, processor, { connection, concurrency });
}
```

Inject the services your processors need (a repository, a mailer) through the **composition root** ([Redis Service](../08_nodejs-integration/02_redis-service.md#composition-root)), rather than importing singletons, so processors are unit-testable with fakes.

## Testing

- Use a **real Redis** (Testcontainers) and a **unique queue name per test**, and clean up with `obliterate({ force: true })`
- Test processors as **plain functions** with fakes. Test BullMQ wiring (options, retries) in a few integration tests
- Use short `delay`, `backoff` and `lockDuration` values to keep tests fast

```ts
it("retries then succeeds", async () => {
  const name = `t-${randomUUID()}`;
  const queue = new Queue(name, { connection });
  let calls = 0;
  const worker = new Worker(name, async () => { if (++calls < 3) throw new Error("boom"); return "ok"; },
    { connection, settings: { backoffStrategy: () => 20 } });
  const events = new QueueEvents(name, { connection });
  await events.waitUntilReady();

  const job = await queue.add("n", {}, { attempts: 5, backoff: { type: "custom" } });
  await job.waitUntilFinished(events, 5_000);

  expect(calls).toBe(3);
  await Promise.all([worker.close(), events.close(), queue.obliterate({ force: true }).then(() => queue.close())]);
});
```

## BullMQ, Streams, or something else

| Need | BullMQ | Streams (module 10) | Kafka, SQS, RabbitMQ |
|------|--------|---------------------|-----------------------|
| Background jobs with retries, delays, priorities | **Built in** | DIY | Varies |
| Dashboard | **Bull Board** | DIY | Provider tools |
| Several services each reading every event | Awkward | **Natural** (groups) | **Natural** |
| Replay of history | No | **Yes** (memory-bound) | **Yes** (disk) |
| Parent/child workflows | **Flows** | DIY | DIY |
| Operational weight | Low (Redis only) | Low | Higher |
| Scale ceiling | One Redis | One Redis per stream, partition | Very high |

Rule of thumb: **jobs** (do this work, once, with retries) point to BullMQ. **Events** (this happened, several consumers may care, replay may be needed) point to Streams or a broker. Many systems use both.

## Checklist

- [ ] `maxRetriesPerRequest: null` on worker and event connections, and `enableOfflineQueue: false` for HTTP-path producers
- [ ] `error` listeners on every `Queue`, `Worker` and `QueueEvents`
- [ ] `removeOnComplete` and `removeOnFail` set everywhere
- [ ] `attempts`, `backoff` (with jitter) and `UnrecoverableError` for permanent failures
- [ ] Idempotent processors, with the job id as the idempotency key for external calls
- [ ] Concurrency sized to downstream limits, plus a `limiter` where a provider rate limits you
- [ ] CPU-heavy work in sandboxed processors
- [ ] Graceful shutdown (`worker.close()`) and SIGTERM handling
- [ ] Redis: `noeviction`, persistence, a hash-tagged `prefix` in Cluster
- [ ] Dashboard behind authentication, and alerts on queue age, failed count and stalls
- [ ] Job schedulers (not cron-per-instance) for recurring work

## Pitfalls

| Pitfall | Fix |
|---------|-----|
| `Worker` throws about `maxRetriesPerRequest` | Set it to `null` for worker connections |
| Redis fills with completed jobs | `removeOnComplete` and `removeOnFail` |
| No `error` listener on the worker | Attach one, or errors go unnoticed |
| `attempts` left at the default of 1 | Set retries explicitly |
| Retrying permanent failures | `UnrecoverableError` |
| Non-idempotent processors with stalls and retries | Idempotency keys and state checks |
| CPU-bound processor causing false stalls | Sandboxed processors or worker threads |
| `queue.add` hanging in a request during a Redis outage | `enableOfflineQueue: false` and a low retry count on the producer connection |
| Evictable Redis (`allkeys-lru`) under queues | `noeviction`, or a dedicated Redis |
| Reusing a `jobId` and wondering why nothing ran | Existing ids are ignored, and removed jobs free their ids |
| Delaying jobs by weeks inside Redis | Keep long schedules in the database |
| Unprotected Bull Board | Authenticate and authorize |
| Mismatched BullMQ versions across producers and workers | Upgrade them together, and read release notes |

## Key takeaways

- BullMQ is **the lessons you just read, hardened**: leases (locks), delayed jobs, retries and backoff, failed sets, plus priorities, flows, schedulers and dashboards
- Set the essentials: **`maxRetriesPerRequest: null`, `error` listeners, `removeOnComplete`/`removeOnFail`, `attempts` and `backoff`**
- Keep processors **idempotent**, offload CPU work, size concurrency to downstream limits, and close workers gracefully
- Use **Streams** for durable events and replay, and **BullMQ** for jobs

**Next module:** [15_redis-architecture](../15_redis-architecture/README.md)
