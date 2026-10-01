# Queues & BullMQ

A practical tour of BullMQ, the Redis-backed job queue for Node.js: from your first job to delays, priorities, rate limits, and multi-step flows.

## What BullMQ is

BullMQ stores jobs in **Redis** and gives you:

- Durable queues (jobs survive restarts)
- Workers with configurable concurrency
- Retries with backoff, delayed jobs, priorities
- Repeatable/scheduled jobs (`03-scheduled-jobs.md`)
- Rate limiting, job flows (parent/child), events, and progress reporting

```bash
npm install bullmq
# Redis must be running:  docker run -d -p 6379:6379 redis:7
```

BullMQ is the successor to the older **Bull** library. For new projects, use BullMQ. Alternatives: `pg-boss` (queues in PostgreSQL, handy if you don't want Redis), `Agenda` (MongoDB), AWS SQS, RabbitMQ.

---

## Redis requirements

BullMQ relies on Redis data integrity. Two settings matter in production:

```conf
# redis.conf
maxmemory-policy noeviction     # REQUIRED: Redis must never silently delete queue keys when memory is full
appendonly yes                   # AOF persistence, so jobs survive a Redis restart
```

If Redis uses an eviction policy like `allkeys-lru`, it may delete job data under memory pressure and you'll lose work with no error. BullMQ warns about this at startup. Also consider using a **dedicated Redis instance** for queues, separate from your cache (`07-databases/redis/02-caching-and-sessions.md`), because cache eviction settings conflict with queue requirements.

---

## The three pieces: Queue, Worker, and the connection

```js
// queues/connection.js
export const connection = {
  host: process.env.REDIS_HOST ?? "127.0.0.1",
  port: Number(process.env.REDIS_PORT ?? 6379),
  password: process.env.REDIS_PASSWORD,
  // BullMQ requires this for blocking worker connections (ioredis option)
  // (BullMQ sets it automatically for Workers when you pass plain options)
};
```

```js
// queues/email.queue.js: the PRODUCER side
import { Queue } from "bullmq";
import { connection } from "./connection.js";

export const emailQueue = new Queue("email", { connection });
```

```js
// workers/email.worker.js: the CONSUMER side (runs in a separate process)
import { Worker } from "bullmq";
import { connection } from "../queues/connection.js";

export const emailWorker = new Worker(
  "email",                                   // must match the queue name
  async (job) => {
    const { to, template } = job.data;
    await mailer.send(to, template);
    return { sent: true };                   // becomes job.returnvalue
  },
  { connection, concurrency: 5 }
);

emailWorker.on("completed", (job) => console.log(`✅ ${job.id} done`));
emailWorker.on("failed", (job, err) => console.error(`❌ ${job?.id} failed:`, err.message));
emailWorker.on("error", (err) => console.error("Worker error:", err));   // connection-level errors: ALWAYS listen
```

Adding a job from your API:

```js
await emailQueue.add("welcome", { to: "sam@example.com", template: "welcome" });
```

- `"welcome"` is the **job name** (a label; one worker function can branch on `job.name`).
- The second argument is the **payload**: it must be JSON-serializable.
- The queue name links producer and worker; the job name just distinguishes types within a queue.

### Reuse connections wisely

Each `Queue` and `Worker` opens Redis connections. In an API process, create queues **once** at startup and import them, rather than constructing a `new Queue()` inside a request handler, which leaks connections. Workers need their own dedicated blocking connection; BullMQ handles this when you pass connection options (not a shared ioredis instance, unless you know what you're doing).

---

## Project layout

Keep the web server and workers as separate entry points, sharing queue definitions:

```
src/
├── queues/
│   ├── connection.js
│   ├── email.queue.js
│   └── reports.queue.js
├── workers/
│   ├── email.worker.js
│   ├── reports.worker.js
│   └── index.js           ← worker process entry point
├── jobs/                  ← the actual handler logic (plain functions, easily testable)
│   ├── sendWelcomeEmail.js
│   └── generateReport.js
├── app.js                 ← web server (producers only)
├── server.js
└── worker.js              ← `node src/worker.js` starts workers
```

```json
{
  "scripts": {
    "start": "node src/server.js",
    "start:worker": "node src/worker.js"
  }
}
```

Run them as separate processes or containers (`16-production/03-docker-and-compose.md`):

```yaml
services:
  api:
    command: node src/server.js
    deploy: { replicas: 2 }
  worker:
    command: node src/worker.js
    deploy: { replicas: 3 }       # scale workers independently of the API
```

Keeping the handler as a **plain function** in `jobs/` makes it testable without Redis and reusable from scripts:

```js
// jobs/sendWelcomeEmail.js
export async function sendWelcomeEmail({ userId }, { userRepository, mailer }) {
  const user = await userRepository.findById(userId);     // reload fresh data by ID
  if (!user) return;                                       // user deleted since enqueueing: nothing to do
  await mailer.sendWelcome(user.email, user.name);
}
```

```js
// workers/email.worker.js
new Worker("email", async (job) => {
  switch (job.name) {
    case "welcome": return sendWelcomeEmail(job.data, deps);
    case "password-reset": return sendPasswordReset(job.data, deps);
    default: throw new UnrecoverableError(`Unknown job: ${job.name}`);
  }
}, { connection, concurrency: 5 });
```

---

## Job payloads: pass IDs, not snapshots

```js
// ❌ entire objects: stale by the time the job runs; bloats Redis; may contain secrets
await queue.add("welcome", { user: fullUserObject });

// ✅ identifiers + the minimum needed context
await queue.add("welcome", { userId: user.id });
```

Reasons:

- The job may run seconds, minutes, or hours later; the user might have changed their email or been deleted.
- Redis memory is finite, and large payloads slow everything down.
- Payloads are stored in plain text in Redis and visible in dashboards, so **never put passwords, tokens, or card data in a job**.

For bulky inputs (an uploaded file), store it in object storage (S3) and pass the key.

---

## Job options

Options can be set per job, or as defaults for the whole queue.

```js
await queue.add(
  "send-invoice",
  { invoiceId: "inv_42" },
  {
    attempts: 5,                                   // total tries (including the first)
    backoff: { type: "exponential", delay: 2000 },// wait 2s, 4s, 8s, 16s between tries
    delay: 60_000,                                 // wait 60 s before the first attempt
    priority: 1,                                   // lower number = higher priority (1 is highest)
    jobId: "invoice-inv_42",                       // custom ID: deduplicates (see below)
    removeOnComplete: { age: 3600, count: 1000 },  // keep completed jobs for 1h, max 1000
    removeOnFail: { age: 7 * 24 * 3600 },          // keep failed jobs for 7 days for debugging
    lifo: false,                                   // true = process newest first
  }
);
```

```js
// Sensible queue-wide defaults
export const emailQueue = new Queue("email", {
  connection,
  defaultJobOptions: {
    attempts: 5,
    backoff: { type: "exponential", delay: 2000 },
    removeOnComplete: { age: 3600, count: 1000 },
    removeOnFail: { age: 7 * 24 * 3600 },
  },
});
```

### Always configure cleanup

By default, BullMQ **keeps completed and failed jobs forever**. On a busy queue, Redis memory grows until it fills up. Always set `removeOnComplete` and `removeOnFail` (by `age` and/or `count`).

---

## Delayed jobs

```js
// run in 30 minutes: e.g. "remind the user if they haven't finished onboarding"
await queue.add("onboarding-reminder", { userId }, { delay: 30 * 60 * 1000 });

// cancel it if the user finished in the meantime
const job = await queue.getJob(`onboarding-reminder-${userId}`);
await job?.remove();
```

Combine with a deterministic `jobId` (`onboarding-reminder-${userId}`) so you can find and remove the job later, and so a second "schedule" request doesn't create a duplicate.

Delays are approximate: a delayed job becomes *eligible* at the time, but runs when a worker is free. Don't rely on second-level precision.

---

## Deduplication with job IDs

If you add a job with an ID that already exists in the queue (and hasn't been removed), BullMQ **ignores** the new one.

```js
// only one "sync" for this account can be waiting at a time
await queue.add("sync-crm", { accountId }, { jobId: `sync-crm-${accountId}` });
await queue.add("sync-crm", { accountId }, { jobId: `sync-crm-${accountId}` });   // no-op
```

Gotchas:

- Custom job IDs **cannot contain `:`** (it's reserved by BullMQ's key scheme); use `-` or `_`.
- Once a job completes and is removed, the same ID can be used again. If you keep completed jobs, the old job still occupies the ID, so dedupe applies until it is removed.
- Job IDs that look like integers are reserved for auto-generated IDs; prefix yours with text.

This is a first line of defense against duplicates; real idempotency still belongs in the handler (`02-workers-retry-dlq.md`).

---

## Priorities

```js
await queue.add("export", { userId, size: "huge" }, { priority: 10 });
await queue.add("password-reset", { userId }, { priority: 1 });   // jumps ahead
```

Priority ranges from 1 (highest) to about two million. Prioritized jobs have slightly more overhead than plain FIFO ones. For truly distinct workloads, prefer **separate queues with separate workers**, which gives stronger isolation:

```js
const criticalQueue = new Queue("email-critical", { connection });   // password resets: 10 workers
const bulkQueue = new Queue("email-bulk", { connection });           // newsletters: 2 workers
```

A marketing blast of 500,000 emails should never delay a password reset.

---

## Concurrency and scaling workers

```js
new Worker("images", processImage, { connection, concurrency: 10 });
```

`concurrency` = how many jobs **one worker process** runs simultaneously. For I/O-bound jobs (HTTP calls, database queries, email), async code lets one process handle many at once, so raise it (10–50). For **CPU-bound** jobs (image resizing, PDF rendering, hashing), a high concurrency won't help: the work is on one thread, and will block the event loop (`15-performance/01-event-loop-performance.md`). Options:

1. **Run more worker processes**, each with `concurrency: 1` or 2. Processes scale across cores/machines.
2. Use **sandboxed processors**, where BullMQ runs your handler in a separate process or worker thread:

```js
// workers/images.worker.js
import { Worker } from "bullmq";
import path from "node:path";

new Worker("images", path.join(import.meta.dirname, "processors/resize.js"), {
  connection,
  concurrency: 4,
  useWorkerThreads: true,          // or omit to use child processes
});
```

```js
// workers/processors/resize.js: runs in its own thread/process
export default async function (job) {
  // heavy CPU work is isolated from the worker's event loop
  return await resizeImage(job.data.key);
}
```

(`02-core-modules/10-cluster-and-worker-threads.md` explains the underlying mechanics.)

Scaling = add more worker processes. Redis coordinates, so **each job goes to exactly one worker at a time**.

---

## Rate limiting

Third-party APIs often cap request rates. Enforce it in the worker, across *all* worker instances:

```js
new Worker("crm-sync", syncToCrm, {
  connection,
  limiter: { max: 10, duration: 1000 },     // at most 10 jobs started per second, globally
});
```

When the limit is hit, waiting jobs simply stay queued until the window allows more, with no errors and no lost work. For an API that returns `429 Too Many Requests` with `Retry-After`, you can dynamically pause the queue:

```js
new Worker("crm-sync", async (job) => {
  const res = await crm.push(job.data);

  if (res.status === 429) {
    const waitMs = Number(res.headers.get("Retry-After") ?? 5) * 1000;
    await worker.rateLimit(waitMs);               // pause the whole queue for waitMs
    throw Worker.RateLimitError();                // retry this job without consuming an attempt
  }
}, { connection, limiter: { max: 50, duration: 1000 } });
```

---

## Progress and return values

```js
new Worker("reports", async (job) => {
  const rows = await loadRows(job.data);

  for (let i = 0; i < rows.length; i++) {
    await processRow(rows[i]);
    if (i % 100 === 0) await job.updateProgress(Math.round((i / rows.length) * 100));
  }

  return { downloadUrl: await uploadReport(rows) };     // stored as job.returnvalue
}, { connection });
```

```js
// API: poll for status
const job = await reportQueue.getJob(jobId);
const state = await job.getState();       // waiting | active | completed | failed | delayed | ...
res.json({ state, progress: job.progress, result: job.returnvalue });
```

Keep return values **small** (a URL or ID, not the data itself). They're stored in Redis.

### Reacting to results elsewhere: QueueEvents

Workers emit events for their own jobs; to watch a queue from *another* process (like your API), use `QueueEvents`:

```js
import { QueueEvents } from "bullmq";

const events = new QueueEvents("reports", { connection });

events.on("completed", ({ jobId, returnvalue }) => { /* push to the user via WebSocket */ });
events.on("failed", ({ jobId, failedReason }) => { /* alert */ });
events.on("progress", ({ jobId, data }) => { /* stream progress */ });

// or wait for one job to finish (careful: holds a request open; only for short jobs)
const result = await job.waitUntilFinished(events, 30_000);
```

---

## Flows: parent and child jobs

Some work is a pipeline: *generate 5 thumbnails → then mark the upload complete.* A **flow** models a tree of jobs where a parent runs only after all its children have completed.

```js
import { FlowProducer } from "bullmq";

const flow = new FlowProducer({ connection });

await flow.add({
  name: "finalize-upload",
  queueName: "uploads",
  data: { uploadId: "up_1" },
  children: [
    { name: "thumbnail", queueName: "images", data: { uploadId: "up_1", size: 64 } },
    { name: "thumbnail", queueName: "images", data: { uploadId: "up_1", size: 256 } },
    { name: "virus-scan", queueName: "security", data: { uploadId: "up_1" } },
  ],
});
```

```js
// the parent's worker can read its children's results
new Worker("uploads", async (job) => {
  const results = await job.getChildrenValues();       // { "bull:images:1": {...}, ... }
  await markUploadComplete(job.data.uploadId, results);
}, { connection });
```

If a child fails permanently, the parent stays waiting unless you configure failure handling (`failParentOnFailure`, `ignoreDependencyOnFailure`, `removeDependencyOnFailure`). Flows are powerful but add complexity, and for simple linear steps, one job that runs the steps in order (with checkpoints) is often easier to reason about.

---

## Bulk adds and queue management

```js
// add many jobs in one round trip (far faster than a loop of add() calls)
await queue.addBulk(
  users.map((u) => ({ name: "newsletter", data: { userId: u.id }, opts: { jobId: `nl-2026-09-${u.id}` } }))
);

// inspect
await queue.getJobCounts("waiting", "active", "delayed", "failed", "completed");
await queue.getWaiting(0, 20);
await queue.getFailed(0, 20);

// control
await queue.pause();                  // stop workers picking up new jobs (in-flight ones finish)
await queue.resume();
await queue.drain();                  // remove all WAITING jobs
await queue.clean(3600_000, 1000, "completed");   // remove old completed jobs
await queue.obliterate({ force: true });          // delete the queue entirely (dangerous)
```

Per-job:

```js
const job = await queue.getJob(id);
await job.retry();                    // re-run a failed job
await job.remove();                   // delete it
await job.promote();                  // make a delayed job run now
await job.changePriority({ priority: 1 });
```

---

## Dashboards

Invisible queues are how jobs silently pile up. Mount a dashboard:

```bash
npm install @bull-board/api @bull-board/express
```

```js
import { createBullBoard } from "@bull-board/api";
import { BullMQAdapter } from "@bull-board/api/bullMQAdapter";
import { ExpressAdapter } from "@bull-board/express";

const serverAdapter = new ExpressAdapter();
serverAdapter.setBasePath("/admin/queues");

createBullBoard({
  queues: [new BullMQAdapter(emailQueue), new BullMQAdapter(reportQueue)],
  serverAdapter,
});

// PROTECT IT: it can retry, delete, and inspect job data
app.use("/admin/queues", requireAuth, requireRole("admin"), serverAdapter.getRouter());
```

Alternatives: Taskforce.sh (hosted), Arena (older), or Prometheus metrics (`14-logging-observability/03-metrics-and-prometheus.md`).

---

## Testing

```js
// 1. Unit test the handler as a plain function: no Redis
test("sendWelcomeEmail skips deleted users", async () => {
  const mailer = { sendWelcome: jest.fn() };
  await sendWelcomeEmail({ userId: "gone" }, { userRepository: { findById: async () => null }, mailer });
  expect(mailer.sendWelcome).not.toHaveBeenCalled();
});

// 2. Integration test against a real Redis (docker / Testcontainers)
test("job flows through queue and worker", async () => {
  const queue = new Queue("test-email", { connection });
  const done = new Promise((resolve) => {
    new Worker("test-email", async (job) => job.data.n * 2, { connection })
      .on("completed", (_job, result) => resolve(result));
  });

  await queue.add("double", { n: 21 });
  expect(await done).toBe(42);

  await queue.obliterate({ force: true });
});

// 3. In API tests: replace the queue with a fake via dependency injection
const app = buildApp({ emailQueue: { add: async (name, data) => added.push({ name, data }) } });
```

Use unique queue names (or a separate Redis DB) per test run to avoid cross-talk (`13-testing/`).

---

## Common mistakes

```js
// ❌ creating a Queue (and Redis connections) inside a request handler
app.post("/x", async () => { const q = new Queue("email", { connection }); await q.add(...); });

// ❌ no error listener on the worker → unhandled 'error' events can crash the process
// ❌ no removeOnComplete / removeOnFail → Redis fills up
// ❌ evicting Redis policy (allkeys-lru) on the queue's Redis instance
// ❌ huge payloads or secrets in job data
// ❌ non-idempotent handlers behind at-least-once delivery
// ❌ CPU-heavy work in a worker with high concurrency: blocks the event loop and stalls job locks
// ❌ running workers in the same process as the web server (a job spike starves HTTP)
// ❌ custom jobId containing ":"
// ❌ forgetting that the producer and worker must use the SAME queue name and compatible Redis connection
// ❌ assuming delayed jobs run exactly on time
```

### "Job stalled" warnings

Workers hold a **lock** on active jobs and renew it periodically. If the event loop is blocked too long (heavy synchronous code) or the process dies, the lock expires and BullMQ marks the job **stalled**, re-queuing it for another worker. You may then see duplicate executions. Fixes: don't block the event loop, use sandboxed processors for CPU work, and keep handlers idempotent. (`lockDuration` and `stalledInterval` are tunable, but fixing the root cause is better.)

## Next

**`02-workers-retry-dlq.md`** goes deeper into reliability: designing idempotent handlers, choosing retry and backoff strategies, separating permanent from transient failures, dead-letter queues, and shutting workers down gracefully.
