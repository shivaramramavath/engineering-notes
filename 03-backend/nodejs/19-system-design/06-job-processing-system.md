# Job Processing System

A platform for running work **outside the request/response cycle**: sending emails, resizing images, generating reports, syncing with third-party APIs, nightly cleanups. Nearly every system in this folder depends on one — the notification workers, upload processors, and click analytics are all job processors. This file designs the general-purpose version, building on `11-async-processing/` (BullMQ, workers, retries, DLQ, scheduled jobs).

## Requirements

**Functional**
- Producers submit jobs (with a payload) and get an immediate acknowledgement
- Workers execute jobs asynchronously
- Support **delayed** and **scheduled/recurring** jobs, and **priorities**
- Automatic **retries** with backoff; permanently failing jobs are preserved for inspection
- Visibility: job status, history, queue depth, failures

**Non-functional**
- **Durable** — accepted jobs must survive restarts and crashes
- **At-least-once execution** — a job runs at least once, never silently lost
- **Scalable** — add workers to increase throughput
- **Isolated** — a flood of low-priority jobs must not starve urgent ones
- **Observable** and operable (pause, drain, replay)

**Out of scope:** multi-step workflow orchestration with complex branching (mention as an extension), exactly-once execution (not achievable in general — we design for idempotency instead).

## Scale estimate

Assumptions: 20 million jobs/day, bursts up to 20× average, jobs range from 50 ms to 2 minutes.

| Quantity | Calculation | Result |
|----------|-------------|--------|
| Average rate | 20M ÷ 86,400 | ~230 jobs/sec |
| Burst | ~20× | ~4,600 jobs/sec |
| Workers needed | rate × avg duration (Little's law): e.g. 230/s × 1 s average | ~230 concurrent job slots on average |

Little's law is a useful sizing tool: **concurrency ≈ arrival rate × average processing time**. At bursts, the queue absorbs the excess and workers catch up, as long as average capacity exceeds average arrivals.

## High-level design

```
 Producers                    ┌───────────────────────────┐
 (API, cron, other  ───────►  │   Queue broker (Redis /   │
  services)                   │   BullMQ, or SQS, etc.)   │
                              │                           │
                              │  critical │ default │ bulk│   separate queues / priorities
                              └──────┬────┴────┬────┴──┬──┘
                                     ▼         ▼       ▼
                              ┌────────┐ ┌────────┐ ┌────────┐
                              │workers │ │workers │ │workers │   scaled independently
                              └───┬────┘ └───┬────┘ └───┬────┘
                                  │          │          │
                  success ◄───────┴──────────┴──────────┘
                  failure ──► retry w/ backoff ──► (after max attempts) ──► Dead-letter queue
                                  │
                                  ▼
                        results / status store, metrics, logs
```

## Choosing the queue technology

| Option | Strengths | Watch out for |
|--------|-----------|---------------|
| **Redis + BullMQ** | Rich features (delays, priorities, rate limits, retries, repeatable jobs), great Node.js fit, simple to run | Redis persistence/durability must be configured; memory-bound; Redis is now critical infrastructure |
| **Cloud queues (SQS, Pub/Sub)** | Managed, highly durable, scales automatically | Fewer built-in features; delays/priorities/ordering limited or require extra design |
| **Database-backed queue** (`SELECT ... FOR UPDATE SKIP LOCKED`) | No new infrastructure; transactional with your data | Doesn't scale as far; polling load on the DB |
| **Kafka** | Huge throughput, replayable log, ordering per partition | A log, not a task queue — retries, delays, and per-job state need extra work (`11-async-processing/04-kafka.md`) |

For most Node.js systems, **BullMQ on Redis** is a strong default. Pick a managed cloud queue when durability and zero operations matter more than features.

## Producing a job

```js
import { Queue } from "bullmq";

const emailQueue = new Queue("email", { connection });

await emailQueue.add(
  "welcome",                                   // job name
  { userId: "u_42" },                          // payload: small, serializable
  {
    jobId: `welcome:u_42`,                     // dedupe: same id won't be added twice while it exists
    attempts: 5,
    backoff: { type: "exponential", delay: 1000 },
    removeOnComplete: { age: 3600, count: 1000 },   // keep history bounded
    removeOnFail: { age: 7 * 24 * 3600 },           // keep failures a week for debugging
  }
);
```

**Payload rules:**
- Store **identifiers, not whole objects** — `{ userId }`, not the user's full record. The data may change before the job runs, and big payloads bloat Redis. The worker fetches fresh data.
- **Never put secrets** (passwords, tokens) in payloads — they sit in the broker and show up in dashboards and logs.
- Keep payloads small and **JSON-serializable**; version the shape if it will evolve while old jobs are still queued.

## Consuming jobs: workers

```js
import { Worker } from "bullmq";

const worker = new Worker(
  "email",
  async (job) => {
    switch (job.name) {
      case "welcome": return sendWelcomeEmail(job.data.userId);
      default: throw new Error(`Unknown job: ${job.name}`);
    }
  },
  { connection, concurrency: 10 }
);

worker.on("failed", (job, err) => logger.error({ jobId: job?.id, err }, "job failed"));
worker.on("error", (err) => logger.error({ err }, "worker error"));   // don't let this go unhandled
```

**Run workers as separate processes (or containers) from the API.** CPU-heavy jobs then can't block the web server's event loop, and you scale and deploy them independently (`15-performance/01-event-loop-performance.md`). For CPU-bound work inside a worker, use `worker_threads` or a child process (`02-core-modules/10-cluster-and-worker-threads.md`).

**Choosing `concurrency`:** for I/O-bound jobs (HTTP calls, DB queries) raise it substantially; for CPU-bound jobs keep it near the core count. Add instances for more throughput.

## The central guarantee: at-least-once, so jobs must be idempotent

When a worker takes a job it holds a **lock/lease** that it renews while working. If the worker crashes, the lease expires and the job is handed to another worker. So a job can run **more than once**: crash after doing the work but before reporting success, or a job that stalls past its lock and gets re-queued while the original is still running.

Therefore **every job must be safe to run twice.** Techniques:

```js
async function chargeOrderJob(job) {
  const { orderId } = job.data;

  // 1. Check durable state first — skip if already done
  const order = await orders.get(orderId);
  if (order.paymentStatus === "paid") return;

  // 2. Use an idempotency key with the downstream API
  await paymentProvider.charge({
    amount: order.totalCents,
    idempotencyKey: `charge:${orderId}`,           // provider ignores a repeat
  });

  // 3. Record completion
  await orders.markPaid(orderId);
}
```

- Prefer **"set state to X"** operations over **"increment/append"** ones
- Use **unique constraints** so a duplicate insert fails harmlessly
- Pass **idempotency keys** to providers that support them (`09-api-development/06-idempotency.md`)
- Make long jobs **checkpoint progress** so a retry resumes rather than restarts

## Retries and failure handling

```
attempt 1 fails ──► wait 1s ──► attempt 2 fails ──► wait 2s ──► attempt 3 ... ──► attempt N fails
                                                                                       │
                                                                                       ▼
                                                                              Dead-letter queue
```

- **Exponential backoff with jitter** — retries spread out instead of hammering a recovering dependency all at once
- **Cap attempts** — otherwise a bad job loops forever
- **Distinguish transient from permanent errors.** Network timeout or 503 → retry. Validation error or "record not found" → retrying won't help; fail immediately (BullMQ supports an unrecoverable-error signal for this — check your version's docs)
- **Dead-letter queue (DLQ):** exhausted jobs are parked, not deleted. Alert on DLQ growth, inspect failures, fix the bug, then **replay** (`11-async-processing/02-workers-retry-dlq.md`)
- **Poison messages** (a job that crashes the worker every time) are exactly what capped attempts + DLQ protect against

## Delays, schedules, and recurring jobs

```js
// Delayed: run once, 15 minutes from now
await emailQueue.add("abandoned-cart", { cartId }, { delay: 15 * 60 * 1000 });

// Recurring: every day at 02:00 server time (cron syntax)
await reportQueue.upsertJobScheduler?.("nightly-report", { pattern: "0 2 * * *" }, { name: "generate-report" });
// (older BullMQ versions use `add(..., { repeat: { pattern: ... } })` — check the docs for your version)
```

**The multi-instance trap:** if you schedule with `setInterval` or `node-cron` inside your API, *every* instance runs it — your nightly report fires N times. Registering the schedule **in the queue** (above) means it's created once and executed by **one** worker (`11-async-processing/03-scheduled-jobs.md`). Still make scheduled jobs idempotent: a missed or doubled run shouldn't corrupt data.

Think about **time zones and daylight saving** for user-facing schedules, and what should happen if the system was down when a run was due (skip it, or run it late?).

## Priorities, isolation, and fairness

- **Separate queues by workload**, not just priorities: `critical`, `default`, `bulk`. Give critical dedicated workers so a bulk import can't starve password resets.
- **Priority levels** within a queue help, but a huge backlog of low priority can still hurt; separate queues give stronger isolation.
- **Rate limit** queues that call rate-limited third parties (`limiter` option on the worker).
- **Per-tenant fairness:** in multi-tenant systems, one customer enqueuing a million jobs shouldn't delay everyone else — use per-tenant queues, quotas, or round-robin grouping.

## Job dependencies and workflows (extension)

Some work is a pipeline: *upload → scan → thumbnail → notify*. Options:

- A **flow/parent–child** feature (BullMQ Flows) where a parent completes after its children
- Each job **enqueues the next** on success (simple chains)
- A dedicated **workflow engine** (Temporal, Step Functions) when you need long-running, multi-step processes with branching, compensation, and strong visibility

Prefer the simplest option that fits; reach for an engine when chains grow branches, human approval steps, or days-long waits.

## Graceful shutdown

Deploys and autoscaling stop workers mid-flight. Handle `SIGTERM` by **finishing current jobs and taking no new ones** (`16-production/02-graceful-shutdown-and-health-checks.md`):

```js
process.on("SIGTERM", async () => {
  logger.info("Shutting down worker...");
  await worker.close();          // stop fetching; wait for in-progress jobs to finish
  await connection.quit();
  process.exit(0);
});
```

Give the container enough termination grace time for your longest typical job, or make long jobs short/chunked. Jobs interrupted anyway are recovered by the lease-expiry mechanism — another reason idempotency is non-negotiable.

## Observability and operations

You can't run what you can't see (`14-logging-observability/`). Track:

| Metric | Why it matters |
|--------|----------------|
| **Queue depth** (waiting jobs) | Growing = workers can't keep up |
| **Job latency** (enqueue → start) | What users actually feel |
| **Processing time** (p50/p95/p99) | Spot slow jobs and regressions |
| **Failure rate / retry count** | Health of jobs and dependencies |
| **DLQ size** | Unresolved failures — alert on any growth |
| **Oldest waiting job age** | Detects a stuck queue even if depth looks small |
| **Worker count / utilization** | Capacity planning and autoscaling |

- **Correlation IDs:** put the originating request's ID in the job payload and log it in the worker, so a trace spans API → queue → worker (`14-logging-observability/02-correlation-id.md`)
- **Dashboards:** expose metrics to Prometheus/Grafana (`14-logging-observability/03-metrics-and-prometheus.md`, `05-grafana.md`); a queue UI such as Bull Board helps for inspection
- **Autoscale on queue depth or latency**, not just CPU — an idle-looking worker pool with a growing queue means jobs are waiting on I/O
- **Operations:** the ability to pause a queue, drain it, retry a failed job, and bulk-replay the DLQ

## Redis durability and failure

If Redis *is* your job queue, its configuration is your durability guarantee:

- Enable **persistence** (AOF with a sensible fsync policy) and **replication/failover** — default in-memory-only Redis can lose queued jobs on a crash
- Set the eviction policy to **`noeviction`** for queue Redis — an eviction policy that deletes keys under memory pressure can silently drop jobs
- Use a **separate Redis** for queues vs cache, since cache workloads *want* eviction
- If the broker is down, producers need a fallback. The **outbox pattern** — writing the job request in the same database transaction as the business change, then relaying it to the queue — keeps the two consistent and survives broker outages (`04-notification-system.md`)

## Trade-offs summary

| Decision | Trade-off |
|----------|-----------|
| At-least-once delivery | Never lose work vs must write idempotent jobs |
| Redis-backed queue | Fast, feature-rich vs memory-bound, durability needs care |
| Separate queues per workload | Isolation vs more to operate |
| Many retries | Resilience vs delayed detection of real bugs and added load on failing dependencies |
| Recurring jobs in the queue | Single execution vs dependence on queue infrastructure |
| Small payloads (IDs only) | Fresh data and small broker vs extra fetch in the worker |

## Common mistakes

- **Non-idempotent jobs** — retries and re-deliveries double-charge, double-email, double-insert.
- **Running cron inside every API instance** — the job fires once per instance.
- **Fat payloads or secrets in the payload** — bloated broker, leaked credentials.
- **CPU-heavy jobs in the web process** — blocks the event loop for all users.
- **Retrying forever or retrying permanent errors** — wasted capacity, hidden bugs.
- **No DLQ, or a DLQ nobody watches** — failures disappear quietly.
- **A single queue for everything** — a bulk backlog delays critical work.
- **Redis with an evicting memory policy or no persistence for queues** — silent job loss.
- **Ignoring graceful shutdown** — jobs interrupted on every deploy.
- **Not setting `removeOnComplete`/`removeOnFail`** — history grows until Redis runs out of memory.
- **Scaling on CPU alone** — queue latency climbs while workers idle on I/O.
- **Unhandled `error` events on workers/queues** — can crash the process.

## Quick summary

- A job platform = **producers → durable queue → workers**, with retries, a DLQ, schedules, priorities, and observability
- Delivery is **at-least-once**, so **every job must be idempotent**
- Pass **IDs in small payloads**; never secrets
- **Retry transient failures with exponential backoff + jitter**, fail permanent ones fast, park exhausted jobs in a **DLQ** and alert on it
- Register recurring jobs **in the queue**, not in each app instance
- **Isolate workloads** with separate queues; run workers as separate, independently scaled processes
- Handle **SIGTERM** gracefully; configure Redis for **persistence and no eviction**
- Size with **Little's law**; autoscale on queue depth and latency

## Next

**`21-interview/00-README.md`** (or `20-projects/`, depending on the order you want to build) puts all of this to use — projects to practice on, and interview questions that test the concepts from every folder.
