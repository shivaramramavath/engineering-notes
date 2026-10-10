# 04 · BullMQ

A Redis-backed job queue for Node.js. A Next.js route or Server Action adds jobs; a separate worker process runs them with retries, delays, concurrency and schedules.

> Verified against the BullMQ documentation (introduction, retrying failing jobs, going to production, job schedulers, concurrency, auto-removal, graceful shutdown, stop-retrying pattern). BullMQ evolves quickly; check the version you install. The Next.js integration layout (producer module, separate worker entry, hot-reload guard) is a general pattern, not something BullMQ or Next.js documents together.

## 1. What

Three pieces:

| Piece | Role |
|---|---|
| `Queue` | Producer API: `add()` jobs. Used inside Next.js |
| `Worker` | Consumer: runs your processor function for each job. Runs in its own Node process |
| Redis | Stores the queue state; every process connects to the same Redis |

At least one process must run a `Worker`, or jobs just wait. Multiple workers share the load round-robin.

## 2. Why

Over `after()` (see [03](./03-background-jobs.md)) it adds: persistence across restarts, retries with backoff, delayed jobs, schedules, concurrency control, rate limiting, and visibility into failed jobs.

## 3. How

### 3.1 Install and connect

```bash
npm install bullmq
```

BullMQ uses `ioredis` internally. Redis settings that matter in production (from the BullMQ docs):

- Set `maxmemory-policy` to **`noeviction`**. If Redis evicts keys, queue data can be lost.
- Enable persistence (the docs recommend AOF, with one-second fsync being enough for most apps), because Redis is not durable by default.
- For Workers with ioredis, `maxRetriesPerRequest` must be **`null`**. BullMQ sets this by default for Workers and warns if you override it.

```ts
// lib/queue/connection.ts
export const connection = {
  host: process.env.REDIS_HOST ?? '127.0.0.1',
  port: Number(process.env.REDIS_PORT ?? 6379),
}
```

Passing a plain options object lets BullMQ create connections itself. You can also pass an existing `IORedis` instance. Keep the connection options in one module that both the Next.js app and the worker import.

### 3.2 Producer: the queue in Next.js

```ts
// lib/queue/email.ts
import 'server-only'
import { Queue } from 'bullmq'
import { connection } from './connection'

export type EmailJob = { userId: string; template: 'welcome' | 'receipt' }

const globalForQueue = globalThis as unknown as { emailQueue?: Queue<EmailJob> }

export const emailQueue =
  globalForQueue.emailQueue ??
  new Queue<EmailJob>('email', {
    connection,
    defaultJobOptions: {
      attempts: 5,
      backoff: { type: 'exponential', delay: 1000 },
      removeOnComplete: { age: 3600, count: 1000 },
      removeOnFail: { age: 7 * 24 * 3600 },
    },
  })

if (process.env.NODE_ENV !== 'production') globalForQueue.emailQueue = emailQueue

emailQueue.on('error', (err) => console.error('email queue error', err))
```

- The `globalThis` guard prevents creating a new `Queue` (and Redis connection) on every hot reload in development, the same trick used for database clients in [12 · Database](../12-database/README.md).
- `defaultJobOptions` is a Queue option in BullMQ's API; the docs fetched for this note show options per `add()` call, so confirm the option name against the API reference of your version.
- Attach an `error` listener: the docs recommend it on both Queue and Worker, so connection problems are logged instead of unhandled.

Enqueue from a Server Action, after the database commit:

```ts
'use server'
import { emailQueue } from '@/lib/queue/email'

export async function signUp(formData: FormData) {
  const user = await createUser(formData) // transaction committed

  await emailQueue.add(
    'welcome',
    { userId: user.id, template: 'welcome' },
    { jobId: `welcome:${user.id}` } // dedupe key (see 3.6)
  )
}
```

### 3.3 Worker: a separate process

```ts
// worker/index.ts
import { Worker, UnrecoverableError, type Job } from 'bullmq'
import { connection } from '../lib/queue/connection'
import type { EmailJob } from '../lib/queue/email'

const worker = new Worker<EmailJob>(
  'email',
  async (job: Job<EmailJob>) => {
    const user = await getUser(job.data.userId) // load fresh data by id
    if (!user) throw new UnrecoverableError('user not found') // do not retry

    if (await alreadySent(user.id, job.data.template)) return // idempotent

    await sendEmail(user.email, job.data.template)
    await markSent(user.id, job.data.template)
    return { sent: true }
  },
  { connection, concurrency: 10 }
)

worker.on('completed', (job) => console.log('done', job.id))
worker.on('failed', (job, err) => console.error('failed', job?.id, err.message))
worker.on('error', (err) => console.error('worker error', err))

const shutdown = async (signal: string) => {
  console.log(`${signal} received, closing worker`)
  await worker.close() // stops taking jobs, waits for active ones
  process.exit(0)
}
process.on('SIGINT', () => shutdown('SIGINT'))
process.on('SIGTERM', () => shutdown('SIGTERM'))
```

Run it beside the web server:

```json
{
  "scripts": {
    "worker": "tsx worker/index.ts",
    "worker:prod": "node dist/worker/index.js"
  }
}
```

(`tsx` is one way to run TypeScript directly; any runner or a build step works.)

Facts from the docs:

- `concurrency` is per worker instance, default 1. It helps with async work (DB, HTTP). For CPU-heavy work use sandboxed processors, or more processes. Running several workers in separate processes is recommended because it improves availability. It can be changed at runtime: `worker.concurrency = 5`.
- `worker.close()` marks the worker as closing, stops it picking up new jobs and waits for the current ones; it has no timeout of its own. If a worker dies mid-job, that job is marked **stalled** and picked up by another worker (no separate scheduler needed since BullMQ 2.0).
- Listen for `SIGINT` and `SIGTERM`, as in the code above.
- The docs also advise handling `uncaughtException` and `unhandledRejection` so one bad job does not crash the process.

Your worker does not run inside Next.js, so it does not get Next.js features: no `@/` alias unless your runner resolves it, no automatic `.env.local` loading, no `server-only` import resolution (the package throws outside the React server condition). Keep shared logic in plain modules without `server-only`, or have the worker import a different entry. Use relative imports or configure `tsconfig` paths for the runner.

### 3.4 Retries and backoff

```ts
await queue.add('send', payload, {
  attempts: 3,
  backoff: { type: 'exponential', delay: 1000 },
})
```

| Setting | Meaning (from the docs) |
|---|---|
| `attempts` | Total tries; retries are on only when above 1 |
| `backoff.type: 'fixed'` | Wait `delay` ms between tries |
| `backoff.type: 'exponential'` | Wait `2 ^ (attempts - 1) * delay` ms |
| `jitter` (0 to 1) | Randomizes the wait |
| No backoff | Retry immediately |

Custom strategies go in the Worker's `settings.backoffStrategy`; returning `-1` stops retrying and fails the job.

To stop retrying a job that can never succeed (bad input), throw `UnrecoverableError` from the processor (shown above, from the "stop retrying" pattern page).

### 3.5 Delays, priorities and schedules

Delayed job:

```ts
await queue.add('reminder', { userId }, { delay: 60 * 60 * 1000 }) // 1 hour
```

Recurring work uses **Job Schedulers** (v5.16.0 and later; they replace "repeatable jobs"):

```ts
await queue.upsertJobScheduler(
  'nightly-cleanup',
  { pattern: '0 0 3 * * *', tz: 'UTC' }, // 03:00 every day, cron with seconds field
  { name: 'cleanup', data: {}, opts: { attempts: 3 } }
)

await queue.upsertJobScheduler('heartbeat', { every: 60_000 })
```

Behavior from the docs:

- `upsertJobScheduler(id, repeatOptions, template?)`: calling it again with the same ID updates the scheduler instead of duplicating it, so it is safe to run on every deploy.
- `every` is milliseconds; `pattern` is a cron expression.
- The next job is produced only after the previous one starts processing, so a busy queue or low concurrency can slow the real rate.
- Generated jobs get generated IDs; tell them apart by job name.

Call `upsertJobScheduler` from the worker's startup code (or a one-off script), not from a request handler.

### 3.6 Idempotency and de-duplication

BullMQ gives at-least-once processing, so the processor must tolerate repeats (see [03](./03-background-jobs.md)). Two tools:

- A deterministic `jobId` (`welcome:${userId}`) makes `add()` ignore a duplicate while a job with that ID still exists. Removal settings affect how long it exists, so a completed-and-removed job can be added again. Verify this behavior against the BullMQ docs for your version before depending on it.
- A durable "already done" marker in your own database, checked at the top of the processor. This is the safer guarantee.

### 3.7 Retention

By default completed and failed jobs are kept forever, which grows Redis memory. Set removal per job or in defaults:

```ts
{ removeOnComplete: { age: 3600, count: 1000 }, removeOnFail: { age: 24 * 3600 } }
```

`true` removes immediately, a number keeps that many, an object bounds by `age` (seconds) and/or `count`. Removal is lazy: it happens when another job finishes, so old jobs can linger until then. Keep failed jobs longer than completed ones so you can inspect them.

### 3.8 Progress to the browser

Workers can publish progress, and a `QueueEvents` instance emits `waiting`, `active`, `completed`, `failed` and `progress` for a queue. A Next.js [SSE](./01-server-sent-events.md) route can subscribe to `QueueEvents` and forward events for one job to its client:

```ts
import { QueueEvents } from 'bullmq'
const events = new QueueEvents('email', { connection })
events.on('progress', ({ jobId, data }) => { /* forward if jobId matches */ })
```

Create one `QueueEvents` per process and filter by job ID; do not create one per request.

### 3.9 Deployment shape

```text
┌───────────────┐      add()       ┌───────┐      pull       ┌──────────────┐
│ Next.js app   │ ───────────────► │ Redis │ ◄────────────── │ Worker proc. │ x N
│ (web, N inst.)│                  └───────┘                 └──────────────┘
└───────────────┘
```

- Web and worker deploy separately and scale separately. A second `Dockerfile` target or a second service in compose is typical.
- Serverless web hosting still works for producing jobs if the function can reach Redis (a managed Redis with a reachable endpoint), but the worker needs a long-running host.
- Rolling deploys: SIGTERM, `worker.close()`, and a platform grace period longer than your longest job (or short jobs). Jobs interrupted anyway become stalled and are retried, which is why handlers must be idempotent.

### 3.10 Dashboards

Several community dashboards exist (for example Bull Board). Evaluate them separately, and put any admin UI behind authentication (see [11 · Protecting Routes](../11-authentication/04-protecting-routes.md)).

## 4. When

| Use BullMQ | Use something else |
|---|---|
| You already run Redis, jobs need retries/schedules/visibility | No Redis, Postgres only: a Postgres-backed queue or outbox |
| Self-hosted or containerized workers | Fully serverless with no long-running host: a cloud queue or workflow service |
| Moderate to high throughput with concurrency control | Fire-and-forget logging: `after()` |

## 5. Practical

Local setup:

1. `docker run -p 6379:6379 redis` (or any local Redis).
2. Add `lib/queue/connection.ts`, the queue module and `worker/index.ts`.
3. Start `next dev` and `npm run worker` in two terminals.
4. Trigger the action, watch the worker log, then stop Redis or throw in the processor to watch retries.
5. Kill the worker mid-job to see the stalled job picked up again on restart.

## Common mistakes

| Mistake | Effect | Fix |
|---|---|---|
| Running the Worker inside the Next.js server | Dies with the process, runs per instance and per hot reload | Separate process |
| New `Queue` on every hot reload | Redis connection leak | `globalThis` guard |
| Redis with eviction enabled | Lost jobs | `maxmemory-policy noeviction` |
| `maxRetriesPerRequest` not null for workers | Worker connection errors | Keep BullMQ's default |
| No `worker.on('error')` | Unhandled errors from connection loss | Attach listeners on Worker and Queue |
| No `removeOnComplete`/`removeOnFail` | Redis memory grows | Retention options |
| Huge payloads | Slow, stale | Pass IDs |
| Non-idempotent processor | Double side effects after retries or stalls | Idempotency marker |
| Retrying unrecoverable errors | Wasted attempts | `UnrecoverableError` |
| Calling `upsertJobScheduler` on each request | Needless Redis writes | Call at worker start |
| Importing `server-only` modules in the worker | Throws outside Next.js | Plain shared modules |

## Debugging

```text
Jobs stay in "waiting"
  ├─ Is a worker process running?                  no → start it
  ├─ Same Redis host/db and same queue NAME in both? → compare config
  └─ Worker connected? (error listener output)

Jobs fail repeatedly
  ├─ job.failedReason / stacktrace in your dashboard or failed set
  ├─ Backoff too aggressive/none?                  → set attempts + backoff
  └─ Error can never succeed?                      → throw UnrecoverableError

Jobs ran twice
  └─ Normal at-least-once behavior (retry, stall)  → make the processor idempotent

Redis memory growing
  └─ retention options and maxmemory-policy
```

## Quick Summary

- `Queue.add()` in Next.js; `Worker` in a separate long-running process; Redis in the middle.
- Production Redis: AOF persistence, `maxmemory-policy noeviction`; Worker connections need `maxRetriesPerRequest: null`.
- Use `attempts` plus `backoff`, `UnrecoverableError` for hopeless jobs, `delay` for later, `upsertJobScheduler` for recurring work.
- Close the worker on SIGTERM/SIGINT; stalled jobs are re-run by another worker.
- Limit retention and make processors idempotent, because delivery is at least once.

## Next

[16 · Next chapter](../README.md): see the repo root README for the folder name.