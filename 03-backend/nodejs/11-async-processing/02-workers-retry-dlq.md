# Workers, Retries & Dead-Letter Queues

Making background processing *reliable*: idempotent handlers, sensible retries, handling permanent failures, and never losing work silently.

## Why "it works on my machine" isn't enough for workers

Background jobs fail in ways request handlers don't, and **nobody is watching**. A user who sees a `500` will retry or complain; a job that fails silently just... doesn't happen. A welcome email never sent, an invoice never generated, a refund never issued.

A reliable worker setup answers five questions:

1. **What if a job runs twice?** → idempotency
2. **What if it fails temporarily?** → retries with backoff
3. **What if it will never succeed?** → permanent failures and dead-letter queues
4. **What if the worker is killed mid-job?** → graceful shutdown and stalled-job recovery
5. **How will I know something's wrong?** → monitoring and alerting

---

## 1. Idempotency: the non-negotiable

Queues deliver **at least once**. Duplicates happen when:

- A worker finishes the work but crashes before acknowledging, so the job is retried.
- A job's lock expires during a long pause (blocked event loop) and it's handed to another worker (*stalled job*).
- Your own code enqueues the same job twice (a retried HTTP request, a double-click).
- You manually retry a failed job that had partially succeeded.

**Idempotent = running it N times has the same effect as running it once** (`09-api-development/06-idempotency.md`).

### Techniques

**1. Natural idempotency: set state, don't add to it**

```js
// ❌ not idempotent: each run adds 10 more credits
user.credits += 10;

// ✅ idempotent: the end state is the same however many times it runs
await db.query("UPDATE orders SET status = 'shipped' WHERE id = $1", [orderId]);
```

**2. Claim the work with a unique record**

```js
async function sendWelcomeEmail({ userId }) {
  // the UNIQUE constraint makes "claim" atomic; only one run can win
  const claimed = await db.query(
    `INSERT INTO email_log (user_id, kind) VALUES ($1, 'welcome')
     ON CONFLICT (user_id, kind) DO NOTHING
     RETURNING id`,
    [userId]
  );
  if (claimed.rowCount === 0) return { skipped: "already sent" };

  await mailer.sendWelcome(userId);
}
```

Caveat: if the email call fails *after* the claim, the retry sees "already sent" and skips, losing the email. Fix with a status column:

```sql
-- state machine: pending → sent (or failed); a retry resumes from 'pending'
INSERT INTO email_log (user_id, kind, status) VALUES ($1, 'welcome', 'pending')
ON CONFLICT (user_id, kind) DO NOTHING;
-- then: send → UPDATE email_log SET status = 'sent' WHERE ... AND status = 'pending'
```

**3. Pass an idempotency key downstream**

When the job calls another system (Stripe, an email provider), give *them* a stable key derived from the job, so their side dedupes too:

```js
await stripe.refunds.create(
  { payment_intent: job.data.paymentIntentId },
  { idempotencyKey: `refund-${job.data.refundId}` }      // same key on every retry
);
```

**4. Check-then-act with a lock (when you must)**

```js
const lock = await redis.set(`lock:invoice:${invoiceId}`, job.id, { NX: true, EX: 300 });
if (!lock) throw new Error("Another worker is processing this invoice");   // will be retried later
```

Locks are a last resort: they expire, workers die holding them, and they don't protect against a retry after a crash. A database unique constraint or conditional update is stronger.

**5. Make steps resumable**

For multi-step jobs, record progress so a retry continues instead of repeating:

```js
async function onboard({ userId }, job) {
  const step = job.data.step ?? 0;

  if (step < 1) { await createWorkspace(userId);  await job.updateData({ ...job.data, step: 1 }); }
  if (step < 2) { await sendWelcomeEmail(userId); await job.updateData({ ...job.data, step: 2 }); }
  if (step < 3) { await syncToCrm(userId);        await job.updateData({ ...job.data, step: 3 }); }
}
```

(Each step must itself be idempotent, since a crash between doing the work and recording the step still causes a repeat.)

### Testing idempotency

```js
test("running the job twice sends one email", async () => {
  await sendWelcomeEmail({ userId: "u1" });
  await sendWelcomeEmail({ userId: "u1" });
  expect(mailer.sendWelcome).toHaveBeenCalledTimes(1);
});
```

---

## 2. Retries and backoff

Many failures are **transient**: a network blip, a rate limit, a database failover, a deploy restarting a dependency. Retrying usually fixes them, *if you wait a sensible time between attempts.*

### Configuring retries in BullMQ

```js
await queue.add("sync-crm", { userId }, {
  attempts: 6,                                          // 1 initial try + 5 retries
  backoff: { type: "exponential", delay: 5000 },        // 5s, 10s, 20s, 40s, 80s
});
```

| Backoff type | Delays (delay = 5 s) | Use when |
|---|---|---|
| `fixed` | 5s, 5s, 5s, 5s | Predictable, short-lived issues |
| `exponential` | 5s, 10s, 20s, 40s, 80s | The default choice: backs off pressure on struggling services |

### Why exponential backoff (and jitter)

If a dependency goes down and 10,000 jobs all retry at exactly the same intervals, they hit it in synchronized waves, the **thundering herd**, which can keep it down. Add **jitter** (randomness) to spread retries out:

```js
new Worker("crm", handler, {
  connection,
  settings: {
    backoffStrategy: (attemptsMade, type, err, job) => {
      const base = 5000 * 2 ** (attemptsMade - 1);              // 5s, 10s, 20s, ...
      const capped = Math.min(base, 10 * 60_000);               // never wait more than 10 min
      return Math.round(capped * (0.75 + Math.random() * 0.5)); // ±25% jitter
    },
  },
});

await queue.add("sync-crm", data, { attempts: 8, backoff: { type: "custom" } });
```

### Permanent vs transient failures

Retrying something that can *never* succeed wastes resources and delays the real alert. Classify errors:

| Transient: retry | Permanent: don't retry |
|---|---|
| Network timeout, `ECONNRESET` | Validation error in the payload |
| `429 Too Many Requests` | `404` for a resource that's gone |
| `502`, `503`, `504` | `400`/`401`/`403`/`422` from a third party |
| Deadlock, serialization failure | "Invalid API key" (needs a human) |
| Database connection lost | Bug in your code (`TypeError`) |

BullMQ gives you `UnrecoverableError` to skip remaining retries:

```js
import { Worker, UnrecoverableError } from "bullmq";

new Worker("email", async (job) => {
  const { userId } = job.data;

  const user = await userRepository.findById(userId);
  if (!user) throw new UnrecoverableError(`User ${userId} no longer exists`);   // retrying won't bring them back

  try {
    await mailer.send(user.email, "welcome");
  } catch (err) {
    if (isPermanentMailError(err)) throw new UnrecoverableError(err.message);   // e.g. invalid address
    throw err;                                                                    // transient → BullMQ retries
  }
}, { connection });
```

```js
function isPermanentMailError(err) {
  return [400, 401, 403, 404, 422].includes(err.status);
}
```

A job that throws `UnrecoverableError` moves straight to the failed set.

### Where to put retry logic: job-level or call-level?

| Approach | Good for |
|---|---|
| **Job-level** (BullMQ `attempts`) | Whole-job failures; slow backoff (seconds to hours); survives restarts because state is in Redis |
| **Call-level** (retry a single HTTP request in-process) | Very short blips (a few hundred ms); avoids re-running earlier steps |

Don't stack them blindly: 5 in-process retries × 5 job attempts = 25 calls to a failing service. Use in-process retries sparingly (1–2 quick ones) and let the queue handle the rest.

---

## 3. Dead-letter queues (DLQ)

After the last attempt fails, what happens to the job? In BullMQ it stays in the **failed** set (subject to `removeOnFail`). That works, but:

- Failed jobs live alongside everything else, and are easy to ignore or accidentally clean up.
- There's no clear "needs human attention" inbox.

A **dead-letter queue** is a dedicated queue where exhausted jobs are moved with context, so they can be inspected, fixed, and replayed. (RabbitMQ and SQS have built-in DLQs; BullMQ doesn't, so you build one, which is easy.)

```js
// queues/deadLetter.queue.js
import { Queue } from "bullmq";
export const deadLetterQueue = new Queue("dead-letter", {
  connection,
  defaultJobOptions: { removeOnComplete: false, removeOnFail: false },   // keep everything
});
```

```js
// workers/email.worker.js
const worker = new Worker("email", handler, { connection, concurrency: 5 });

worker.on("failed", async (job, err) => {
  if (!job) return;

  const maxAttempts = job.opts.attempts ?? 1;
  const exhausted =
    job.attemptsMade >= maxAttempts || err instanceof UnrecoverableError;

  if (!exhausted) return;              // BullMQ will retry; nothing to do

  await deadLetterQueue.add(
    "dead-job",
    {
      originalQueue: "email",
      originalName: job.name,
      originalData: job.data,
      originalJobId: job.id,
      error: { message: err.message, stack: err.stack },
      attemptsMade: job.attemptsMade,
      failedAt: new Date().toISOString(),
    },
    { jobId: `dlq-email-${job.id}` }
  );

  logger.error({ jobId: job.id, queue: "email", err }, "Job moved to dead-letter queue");
  metrics.deadLettered.inc({ queue: "email" });
});
```

Note: `attemptsMade` semantics differ slightly between BullMQ versions (whether the count is updated before the `failed` event fires), so test this against the version you run. The `UnrecoverableError` check makes the "exhausted" decision robust either way.

### What to do with dead letters

A DLQ is an **inbox, not a graveyard.** Every dead-lettered job needs an owner:

1. **Alert** when anything arrives (Slack/PagerDuty), or when depth > 0 for too long.
2. **Investigate** using the stored error, payload, and attempt history.
3. **Fix** the root cause (a bug, a missing record, a bad config).
4. **Replay** the jobs back to the original queue once fixed:

```js
// scripts/replay-dead-letters.js
const dead = await deadLetterQueue.getJobs(["waiting", "completed"], 0, 500);

for (const d of dead) {
  const { originalQueue, originalName, originalData } = d.data;
  await getQueue(originalQueue).add(originalName, originalData, {
    jobId: `replay-${d.id}`,                     // avoids replaying twice
  });
  await d.remove();
}
```

5. **Discard** jobs that are no longer relevant, *deliberately*, with a record of why.

Replaying relies on idempotency (`1.` above): a job that failed after doing half its work must be safe to re-run.

---

## 4. Graceful shutdown

Deploys, scaling events, and crashes kill workers. A worker that exits mid-job leaves the job **active** with an expiring lock; BullMQ will detect it as **stalled** after the lock times out and re-queue it, which works but causes delay and duplicates. Shut down cleanly instead:

```js
// worker.js: entry point
import { emailWorker } from "./workers/email.worker.js";
import { reportsWorker } from "./workers/reports.worker.js";
import { logger } from "./config/logger.js";

const workers = [emailWorker, reportsWorker];
let shuttingDown = false;

async function shutdown(signal) {
  if (shuttingDown) return;
  shuttingDown = true;
  logger.info({ signal }, "Shutting down workers");

  // Safety net: if jobs don't finish in time, force exit
  const timer = setTimeout(() => {
    logger.error("Shutdown timed out; forcing exit");
    process.exit(1);
  }, 30_000);
  timer.unref();

  try {
    // close(): stops taking new jobs and waits for active jobs to finish
    await Promise.all(workers.map((w) => w.close()));
    await closeOtherConnections();            // database pool, Redis, queues
    logger.info("Shutdown complete");
    process.exit(0);
  } catch (err) {
    logger.error({ err }, "Error during shutdown");
    process.exit(1);
  }
}

process.on("SIGTERM", () => shutdown("SIGTERM"));    // Docker / Kubernetes stop signal
process.on("SIGINT", () => shutdown("SIGINT"));      // Ctrl+C
```

Important details:

- **`worker.close()` waits for active jobs** to complete. To abandon them immediately (they'll be retried as stalled), use `worker.close(true)`.
- **Match timeouts to reality:** the orchestrator's kill timeout must exceed your longest job, or be prepared for forced kills. Kubernetes: `terminationGracePeriodSeconds`; Docker: `stop_grace_period`.
- **Long jobs** (minutes+) should be *resumable* (see "make steps resumable") so a forced kill loses little work.
- Handle `uncaughtException` and `unhandledRejection` by logging and exiting; let the orchestrator restart the process (`09-api-development/05-error-responses.md`, `16-production/02-graceful-shutdown-and-health-checks.md`).

### Stalled jobs

```js
new Worker("reports", handler, {
  connection,
  lockDuration: 60_000,          // how long a worker "owns" a job before the lock must be renewed
  stalledInterval: 30_000,       // how often BullMQ checks for stalled jobs
  maxStalledCount: 1,            // after this many stalls, the job FAILS instead of being retried again
});
```

A job is stalled when its worker stopped renewing the lock (process killed, event loop blocked). BullMQ re-queues it up to `maxStalledCount` times. If you see frequent stalls, suspect blocked event loops (CPU-heavy or synchronous work in a handler), not bad settings.

---

## 5. Timeouts inside handlers

BullMQ doesn't forcibly kill a hung handler. A job that awaits a promise forever holds its concurrency slot **forever**. Always bound external calls:

```js
new Worker("crm", async (job) => {
  const res = await fetch(url, {
    method: "POST",
    body: JSON.stringify(job.data),
    signal: AbortSignal.timeout(10_000),       // never wait forever on network calls
  });
  if (!res.ok) throw new HttpError(res.status);
}, { connection });
```

For a hard cap on the whole handler:

```js
function withTimeout(promise, ms, label = "operation") {
  let timer;
  const timeout = new Promise((_, reject) => {
    timer = setTimeout(() => reject(new Error(`${label} timed out after ${ms}ms`)), ms);
  });
  return Promise.race([promise, timeout]).finally(() => clearTimeout(timer));
}

new Worker("reports", (job) => withTimeout(generateReport(job.data), 5 * 60_000, "generateReport"), { connection });
```

(The raced promise keeps running in the background; real cancellation requires the work itself to accept an `AbortSignal`.)

---

## 6. Poison messages and payload validation

A **poison job** is one that fails every time (bad data, unexpected shape) and, without a retry cap, would loop forever, consuming workers. Defenses:

- **Always set `attempts`** to a finite number.
- **Validate the payload at the top of the handler:** queue data comes from your own code, but schemas evolve, and an old job may sit in Redis while new code deploys.

```js
import { z } from "zod";

const payloadSchema = z.object({ userId: z.string().uuid() });

new Worker("email", async (job) => {
  const parsed = payloadSchema.safeParse(job.data);
  if (!parsed.success) {
    throw new UnrecoverableError(`Invalid payload: ${parsed.error.message}`);   // retrying can't fix bad data
  }
  await sendWelcomeEmail(parsed.data);
}, { connection });
```

- **Version your job payloads** when changing shape: `{ v: 2, ... }`, so new workers handle old jobs (`09-api-development/02-versioning-and-pagination.md` applies to internal contracts too).
- **Deploy workers that understand both old and new payloads** before producers start sending the new one.

---

## 7. Observability: know when it's broken

### Log every job with context

```js
worker.on("active", (job) => logger.info({ jobId: job.id, name: job.name, attempt: job.attemptsMade + 1 }, "Job started"));
worker.on("completed", (job) => logger.info({ jobId: job.id, ms: Date.now() - job.processedOn }, "Job completed"));
worker.on("failed", (job, err) => logger.warn({ jobId: job?.id, err: err.message, attempt: job?.attemptsMade }, "Job failed"));
```

Carry a **correlation ID** from the originating request into the job, so a single user action can be traced through the API and the workers (`14-logging-observability/02-correlation-id.md`):

```js
await queue.add("welcome", { userId }, { /* ... */ });
// put the request id in the payload or job metadata
await queue.add("welcome", { userId, requestId: req.id });

// in the worker
const log = logger.child({ requestId: job.data.requestId, jobId: job.id });
```

### Metrics worth alerting on

| Metric | Why it matters | Alert when |
|---|---|---|
| **Queue depth** (waiting jobs) | Are workers keeping up? | Growing steadily, or above a threshold |
| **Oldest waiting job age** | The true "latency" of the queue | > your SLA (for example, 5 min for emails) |
| **Failure rate** | Something is broken | Spike vs baseline |
| **DLQ depth** | Work needs human attention | > 0 |
| **Job duration (p50/p95/p99)** | Performance regressions, stalls | Sudden increase |
| **Active jobs / worker utilization** | Capacity planning | Saturated for long periods |
| **Stalled job count** | Blocked event loops, killed workers | Any sustained occurrence |
| **Retry count** | Flaky dependencies | Rising trend |

```js
// Expose queue metrics to Prometheus (14-logging-observability/03-metrics-and-prometheus.md)
import client from "prom-client";

const depth = new client.Gauge({ name: "queue_jobs", help: "Jobs by state", labelNames: ["queue", "state"] });

setInterval(async () => {
  for (const queue of [emailQueue, reportsQueue]) {
    const counts = await queue.getJobCounts("waiting", "active", "delayed", "failed");
    for (const [state, n] of Object.entries(counts)) depth.set({ queue: queue.name, state }, n);
  }
}, 15_000).unref();
```

BullMQ also ships built-in Prometheus support (`queue.exportPrometheusMetrics()`) in recent versions. Check the docs for your version.

### Health checks

Workers have no HTTP port by default, but orchestrators still need liveness signals:

```js
worker.on("error", (err) => { logger.error({ err }, "Worker error"); });

// option: expose a tiny HTTP endpoint that checks the worker's state
import http from "node:http";
http.createServer(async (req, res) => {
  const healthy = worker.isRunning() && !worker.closing;
  res.writeHead(healthy ? 200 : 503).end();
}).listen(8081);
```

---

## 8. The outbox pattern in practice

Recall the dual-write problem (`00-README.md`): you can't atomically commit to PostgreSQL *and* enqueue in Redis. Use the **transactional outbox**:

```sql
CREATE TABLE outbox (
  id          UUID PRIMARY KEY,
  queue       TEXT NOT NULL,
  name        TEXT NOT NULL,
  payload     JSONB NOT NULL,
  created_at  TIMESTAMPTZ NOT NULL DEFAULT now(),
  sent_at     TIMESTAMPTZ
);
CREATE INDEX outbox_unsent ON outbox (created_at) WHERE sent_at IS NULL;
```

```js
// 1. Business change + outbox row in ONE database transaction
await withTransaction(async (tx) => {
  const order = await orderRepository.insert(data, tx);
  await tx.query(
    "INSERT INTO outbox (id, queue, name, payload) VALUES ($1, 'email', 'order-confirmation', $2)",
    [randomUUID(), JSON.stringify({ orderId: order.id })]
  );
});
```

```js
// 2. A relay (a small always-running process or a frequent scheduled job) publishes pending rows
async function relayOutbox() {
  const client = await pool.connect();
  try {
    await client.query("BEGIN");
    const { rows } = await client.query(
      `SELECT * FROM outbox WHERE sent_at IS NULL
       ORDER BY created_at LIMIT 100
       FOR UPDATE SKIP LOCKED`            // multiple relays can run without colliding
    );

    for (const row of rows) {
      await getQueue(row.queue).add(row.name, row.payload, { jobId: `outbox-${row.id}` });   // idempotent add
      await client.query("UPDATE outbox SET sent_at = now() WHERE id = $1", [row.id]);
    }
    await client.query("COMMIT");
  } catch (err) {
    await client.query("ROLLBACK");
    throw err;
  } finally {
    client.release();
  }
}
```

If the relay crashes after enqueueing but before committing, the row is published again on the next run, and the deterministic `jobId` (`outbox-<id>`) prevents a duplicate job. The result is **at-least-once publishing with effective de-duplication**, and **no lost work**. This is the same pattern used to publish to Kafka (`04-kafka.md`). Transactions: `07-databases/postgresql/02-transactions-and-indexing.md`.

Use the outbox when a lost job would be a real problem (payments, order confirmations). For low-stakes notifications, a plain `queue.add` after commit is often acceptable.

---

## Putting it together: a reliable worker template

```js
// workers/email.worker.js
import { Worker, UnrecoverableError } from "bullmq";
import { z } from "zod";

const payload = z.object({ userId: z.string().uuid(), requestId: z.string().optional() });

export function createEmailWorker({ connection, userRepository, mailer, deadLetterQueue, logger }) {
  const worker = new Worker(
    "email",
    async (job) => {
      const parsed = payload.safeParse(job.data);                                   // 1. validate
      if (!parsed.success) throw new UnrecoverableError("Invalid payload");

      const log = logger.child({ jobId: job.id, requestId: parsed.data.requestId });
      const user = await userRepository.findById(parsed.data.userId);               // 2. reload fresh data
      if (!user) throw new UnrecoverableError("User not found");                    // 3. permanent failure

      await mailer.sendWelcome(user.email, {                                        // 4. idempotent, bounded call
        idempotencyKey: `welcome-${user.id}`,
        signal: AbortSignal.timeout(10_000),
      });
      log.info("Welcome email sent");
    },
    {
      connection,
      concurrency: 10,
      limiter: { max: 50, duration: 1000 },                                          // 5. respect provider limits
    }
  );

  worker.on("failed", async (job, err) => {                                          // 6. dead-letter on exhaustion
    if (job && (err instanceof UnrecoverableError || job.attemptsMade >= (job.opts.attempts ?? 1))) {
      await deadLetterQueue.add("dead-job", { originalQueue: "email", originalData: job.data, error: err.message });
    }
  });
  worker.on("error", (err) => logger.error({ err }, "Worker error"));               // 7. never leave 'error' unhandled

  return worker;                                                                     // 8. caller handles graceful shutdown
}
```

---

## Checklist

**Design**
- [ ] Every handler is idempotent and tested by running it twice
- [ ] Payloads contain IDs, not snapshots or secrets; validated at the top of the handler
- [ ] Each job type has an explicit `attempts` and `backoff` (with jitter for high volume)
- [ ] Permanent failures throw `UnrecoverableError`; transient ones propagate
- [ ] External calls have timeouts; idempotency keys are passed downstream

**Failure handling**
- [ ] Exhausted jobs land in a dead-letter queue (or the failed set) with full context
- [ ] Someone is alerted and owns the DLQ; a replay script exists
- [ ] `removeOnComplete`/`removeOnFail` are set so Redis doesn't fill up

**Operations**
- [ ] Workers run separately from the web server and scale independently
- [ ] Graceful shutdown on `SIGTERM`/`SIGINT`, with a timeout that fits your longest job
- [ ] `error` listener attached to every worker
- [ ] Metrics: queue depth, oldest job age, failure rate, DLQ depth, duration
- [ ] Logs include job ID and the originating request ID
- [ ] Redis: `maxmemory-policy noeviction`, persistence enabled

**Consistency**
- [ ] Outbox pattern (or reconciliation job) for work that must not be lost

## Common mistakes

```js
// ❌ catching errors and swallowing them: BullMQ thinks the job succeeded
try { await mailer.send(); } catch (err) { console.error(err); }     // job marked completed!
// ✅ rethrow (or throw UnrecoverableError) so the retry machinery works

// ❌ infinite retries / no attempts limit → poison job loops forever
// ❌ same fixed delay for thousands of retrying jobs → thundering herd
// ❌ retrying permanent errors 10 times, hiding the real alert
// ❌ logging entire payloads (PII, tokens) at info level
// ❌ dead-letter queue with no alert and no owner
// ❌ shutting down with process.exit(0) while jobs are active
// ❌ assuming the job ran only once because "it never failed in testing"
// ❌ long synchronous work in a handler → stalled locks → duplicate runs
// ❌ non-deterministic side effects (random IDs generated inside the handler on each retry) that break dedup
```

## Next

**`03-scheduled-jobs.md`** covers work that happens on a timetable rather than in response to an event: cron syntax, `node-cron`, BullMQ's job schedulers, and, critically, how to stop several app instances from all running the same scheduled task.
