# Async Processing

Moving slow, unreliable, or scheduled work out of the request cycle — and connecting parts of your system without tight coupling.

## The problem: work that doesn't belong in a request

```js
// ❌ everything inline: the user waits for all of it, and any failure ruins the request
app.post("/signup", async (req, res) => {
  const user = await createUser(req.body);          //  30 ms
  await sendWelcomeEmail(user);                      //  800 ms (third-party API, can fail)
  await generateAvatar(user);                        //  2 s   (CPU-heavy)
  await syncToCrm(user);                             //  1.5 s (rate limited, flaky)
  res.status(201).json(user);                        // user stares at a spinner for ~4 seconds
});
```

Problems with doing it inline:

- **Latency:** the client waits for work it doesn't need the result of.
- **Fragility:** if the email provider is down, signup fails, even though the user was created.
- **Coupling:** the signup handler knows about email, images, and the CRM.
- **No retries:** a transient failure is lost, or surfaces as a `500` the user can't fix.
- **Load spikes:** 1,000 signups in a minute means 1,000 simultaneous email/CRM calls.
- **Event loop risk:** CPU-heavy work blocks every other request (`15-performance/01-event-loop-performance.md`).

The fix is to do the *essential* part now, record that the rest **needs to happen**, and let something else do it reliably:

```js
// ✅ do the essential work, enqueue the rest, respond immediately
app.post("/signup", async (req, res) => {
  const user = await createUser(req.body);
  await jobs.add("send-welcome-email", { userId: user.id });
  await jobs.add("generate-avatar", { userId: user.id });
  await jobs.add("sync-crm", { userId: user.id });
  res.status(201).json(user);                        // ~40 ms
});
```

Each job runs in a separate **worker** process, with retries, rate limits, and visibility into failures.

---

## The building blocks

```
┌──────────┐   add job    ┌───────────────┐   pull job   ┌──────────┐
│ Producer │ ───────────▶ │ Queue / Broker│ ───────────▶ │ Worker   │
│ (your API)│             │ (Redis, Kafka)│              │ (process)│
└──────────┘              └───────────────┘              └────┬─────┘
                                                              │ fail N times
                                                              ▼
                                                        ┌──────────┐
                                                        │ Dead-    │
                                                        │ letter   │
                                                        └──────────┘
```

| Concept | Meaning |
|---|---|
| **Producer** | Code that creates work (your API handler) |
| **Queue / broker** | Durable storage that holds work until a worker takes it |
| **Job / message** | One unit of work: a name plus a small JSON payload |
| **Worker / consumer** | A process that pulls jobs and executes them |
| **Retry / backoff** | Re-running a failed job, with increasing delays |
| **Dead-letter queue (DLQ)** | Where jobs go after exhausting retries, for inspection |
| **Scheduler** | Creates jobs on a timetable (cron) |

---

## Queue vs event stream: two different tools

| | Job queue (BullMQ, SQS, RabbitMQ) | Event stream (Kafka) |
|---|---|---|
| Mental model | A **to-do list**: each job is done once, then removed | A **log**: an append-only history that many readers replay |
| Consumers | Competing: each job goes to **one** worker | Independent groups: each group reads **every** event |
| After processing | Job is removed (or kept briefly) | Event stays until retention expires |
| Replay history | No | Yes: rewind offsets |
| Ordering | Best effort (per queue) | Strict **per partition** |
| Throughput | Thousands/sec | Hundreds of thousands+/sec |
| Typical use | Emails, image processing, webhooks, reports | Event-driven architecture, analytics, data pipelines, audit logs |
| Operational weight | Low (just Redis) | High (cluster to run) |

Rule of thumb: **"do this task"** → queue. **"this thing happened"** and several systems care → event stream.

---

## What's in this section

| File | What you learn |
|---|---|
| `01-queues-and-bullmq.md` | BullMQ fundamentals: queues, workers, job options, delays, priorities, flows, rate limits |
| `02-workers-retry-dlq.md` | Making workers reliable: retries, backoff, idempotency, dead-letter queues, graceful shutdown, monitoring |
| `03-scheduled-jobs.md` | Cron-style work: `node-cron`, BullMQ schedulers, avoiding duplicate runs across instances |
| `04-kafka.md` | Kafka concepts and KafkaJS: topics, partitions, consumer groups, ordering, delivery guarantees |

Read in order: `01` and `02` are the bread and butter for most applications; `03` covers timers; `04` is the heavier tool for event-driven systems.

---

## Prerequisites

- `07-databases/redis/01-basics-and-data-structures.md`: BullMQ stores everything in Redis
- `03-javascript-for-node/01-callbacks-promises-async-await.md`: workers are async functions
- `03-javascript-for-node/02-event-loop.md`: why blocking work hurts
- `09-api-development/06-idempotency.md`: queues deliver **at least once**, so handlers must tolerate duplicates
- `10-architecture/05-modular-monolith-vs-microservices.md`: queues and events are how modules and services communicate loosely
- `16-production/02-graceful-shutdown-and-health-checks.md`: workers must shut down cleanly

---

## The one fact that shapes everything

> **Distributed systems give you *at-least-once* delivery, not exactly-once.**

A worker can finish a job and crash before acknowledging it; the broker then hands the job to another worker. So:

1. **Every job handler must be idempotent:** running it twice must be safe.
2. **Jobs carry IDs, not data snapshots:** `{ userId: 42 }`, not the whole user object (which goes stale in the queue).
3. **Assume jobs run late, out of order, and more than once.**

Every file in this section returns to this idea.

---

## When to use background jobs

✅ Good candidates:

- Sending email, SMS, push notifications
- Image/video processing, PDF generation, report exports
- Calling slow or flaky third-party APIs (CRM sync, payment reconciliation)
- Webhook delivery with retries (`09-api-development/07-webhooks.md`)
- Bulk imports, data migrations, cleanup tasks
- Anything the user doesn't need to wait for

❌ Poor candidates:

- Work whose result the response depends on (the caller needs the answer now)
- Tiny, fast, reliable operations: a queue adds overhead and complexity for nothing
- Operations that must be atomic with your database write and can't tolerate eventual consistency. Use the **outbox pattern** (below).

### Telling the client what happened

When a request triggers background work, respond honestly with `202 Accepted` and a way to check on it (`09-api-development/01-rest-api-design.md`):

```js
app.post("/reports", requireAuth, async (req, res) => {
  const job = await reportQueue.add("generate", { userId: req.user.id, params: req.body });
  res
    .status(202)
    .location(`/api/v1/reports/jobs/${job.id}`)
    .json({ data: { jobId: job.id, status: "queued" } });
});

app.get("/reports/jobs/:id", requireAuth, async (req, res) => {
  const job = await reportQueue.getJob(req.params.id);
  if (!job) return res.sendStatus(404);
  res.json({ data: { jobId: job.id, status: await job.getState(), result: job.returnvalue } });
});
```

For results pushed to the browser, combine with WebSockets (`12-realtime/`).

---

## The outbox pattern (preview)

The classic dual-write bug:

```js
await db.query("INSERT INTO orders ...");        // ✅ committed
await queue.add("send-confirmation", { ... });    // 💥 Redis is down, job lost, customer never emailed
```

The database and Redis can't share a transaction. Two safe approaches:

1. **Outbox table:** in the *same* DB transaction as the business change, insert a row into an `outbox` table. A small relay process reads unsent rows and enqueues them, marking each sent. The business change and the intent to act are atomic.
2. **Make the job discoverable from state:** a periodic job scans for "orders with no confirmation sent" and enqueues them. Less elegant, more robust to loss.

```js
await withTransaction(async (tx) => {
  await tx.query("INSERT INTO orders ...");
  await tx.query(
    "INSERT INTO outbox (id, type, payload) VALUES ($1, 'order.placed', $2)",
    [randomUUID(), JSON.stringify({ orderId })]
  );
});
// a relay worker later: SELECT ... FROM outbox WHERE sent_at IS NULL → enqueue → mark sent
```

It's covered where it matters: `02-workers-retry-dlq.md` and `04-kafka.md`.

---

## Principles

1. **Respond fast, work later:** do only what the caller needs to see inline.
2. **Design every job as idempotent:** duplicates are a certainty, not an edge case.
3. **Small payloads, stable IDs:** reload fresh data inside the job.
4. **Always configure retries with backoff,** and always decide what happens after the last failure.
5. **Keep failures visible:** a dead-letter queue nobody watches is just a silent data-loss mechanism.
6. **Run workers separately from the web server** so a job spike can't starve your API.
7. **Shut down gracefully:** finish in-flight jobs before exiting.
8. **Measure:** queue depth, job latency, failure rate, oldest waiting job.

## Next

**`01-queues-and-bullmq.md`** introduces BullMQ, the most popular Redis-backed job queue for Node.js, and builds up from a single job to delays, priorities, rate limits, and job flows.
