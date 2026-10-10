# 03 · Background Jobs

Work that should not block a request: sending email, resizing images, syncing with third parties, nightly cleanups. This note covers what Next.js gives you (`after`), where it stops, and when you need a real queue.

> `after` is verified against the Next.js 16.4 docs. The job-system concepts (at-least-once delivery, idempotency, outbox pattern, cron options) are general engineering knowledge and labelled as such. The durable-queue implementation is in [04 · BullMQ](./04-bullmq.md).

## 1. What

A background job runs outside the request/response cycle. Three levels, from lightest to heaviest:

| Level | Mechanism | Survives restart? | Retries? |
|---|---|---|---|
| Post-response work | `after()` from `next/server` | No | No |
| Scheduled call | Cron hits a Route Handler | Depends on the scheduler | Depends |
| Durable queue | Job stored in Redis/DB, processed by a worker | Yes | Yes |

## 2. Why

- Responses stay fast: the user does not wait for the email provider.
- Slow or flaky work gets retried instead of failing the request.
- Spikes are absorbed by a queue instead of overloading a downstream service.
- Cost and risk are isolated: a crash in a worker does not take down page rendering.

## 3. How

### 3.1 `after`: do it after the response

```tsx
// app/api/orders/route.ts
import { after } from 'next/server'

export async function POST(request: Request) {
  const order = await createOrder(await request.json())

  after(async () => {
    await sendReceiptEmail(order.id)
    await trackEvent('order_created', { id: order.id })
  })

  return Response.json({ id: order.id }, { status: 201 })
}
```

Facts from the docs:

- It schedules work to run after the response (or prerender) finishes. Intended for logging, analytics and other side effects.
- Usable in Server Components (including `generateMetadata`), Server Functions, Route Handlers and Proxy.
- It is **not** a request-time API; calling it does not make a route dynamic. In a static page, the callback runs at build time or on revalidation.
- It runs even if the response failed, including when an error is thrown or `notFound()` / `redirect()` is called.
- It runs for at most the platform's default or configured `maxDuration`.
- Stable since v15.1.0. Supported on Node.js servers and Docker; not on static export; platform-specific for adapters. Serverless platforms need a `waitUntil` implementation, which Vercel provides.

Request data rules:

| Where | `cookies()` / `headers()` inside the callback |
|---|---|
| Route Handler, Server Function | Allowed |
| Server Component, layout, `generateMetadata` | **Throws**; read the values first and close over them |

```tsx
export default async function Page() {
  const sessionId = (await cookies()).get('session-id')?.value ?? 'anonymous'
  after(() => logUserAction({ sessionId })) // uses the value read above
  return <h1>Hi</h1>
}
```

Under Cache Components, read request data in a component wrapped in `<Suspense>`, then call `after` there.

Server Actions work the same way as Route Handlers:

```ts
'use server'
import { after } from 'next/server'

export async function subscribe(formData: FormData) {
  const email = String(formData.get('email'))
  await saveSubscriber(email)
  after(() => sendWelcomeEmail(email))
}
```

### 3.2 What `after` does not give you

`after` has no queue, no persistence and no retry. If the process dies, the duration cap is hit, or the callback throws, the work is lost, and nobody is told unless you catch and log. Use it for work where losing an occasional run is acceptable: analytics, cache warming, logging, best-effort notifications.

Always catch inside the callback so failures are visible:

```ts
after(async () => {
  try {
    await sendReceiptEmail(order.id)
  } catch (err) {
    logger.error({ err, orderId: order.id }, 'receipt email failed')
  }
})
```

### 3.3 Cron-triggered Route Handlers

For recurring work on a platform with a scheduler (host cron feature, GitHub Actions, an external service), the scheduler calls an endpoint:

```ts
// app/api/cron/cleanup/route.ts
export async function GET(request: Request) {
  const auth = request.headers.get('authorization')
  if (auth !== `Bearer ${process.env.CRON_SECRET}`) {
    return new Response('Unauthorized', { status: 401 })
  }

  await deleteExpiredSessions()
  return Response.json({ ok: true })
}
```

Always authenticate such endpoints; they are public URLs. Work must finish within the platform's function duration; for longer work, the endpoint should only enqueue a job. Cron semantics (retries, overlapping runs, missed runs) belong to the scheduler, so read its docs. (General practice.)

### 3.4 When you need a durable queue

Choose a real queue when any of these is true:

- The work must not be lost (payments follow-up, user-visible emails).
- It takes longer than a request or function duration allows.
- It needs retries with backoff, delays, priorities or rate limiting.
- You need to see state: waiting, active, failed, completed.
- Throughput must be controlled (concurrency limits).

Options (general knowledge): a Redis-backed library like BullMQ (next note), a Postgres-backed queue (pg-boss or an outbox table), a cloud queue (SQS, see your AWS chapter if you have one), or a workflow service from your host. Pick based on what you already operate.

### 3.5 Design rules for any job system (general)

1. **Assume at-least-once.** A job can run twice (retry after a timeout, worker killed mid-job). Make handlers **idempotent**: sending the same receipt twice must not charge twice. Use an idempotency key (order ID) and check "already done" before acting.
2. **Small payloads.** Put IDs in the job, not whole records. The worker loads fresh data, which also avoids acting on stale data.
3. **Enqueue after the commit.** If you enqueue inside a database transaction that later rolls back, the job refers to nothing; if you enqueue before commit, the worker may run before the row is visible. Commit first, then enqueue. For stronger guarantees, write the job to an **outbox table** in the same transaction and have a relay move it to the queue (see [12 · Transactions](../12-database/05-transactions.md)).
4. **Separate process for workers.** Do not run long-lived workers inside the Next.js server process, especially on serverless; see [04](./04-bullmq.md).
5. **Observe failures.** Failed jobs need alerts or a dashboard, and a retry limit so poison jobs do not loop forever.
6. **Handle secrets and env in the worker deliberately.** It is a separate process: it does not inherit Next.js `.env` loading unless you set it up.

### 3.5b The shape of the whole thing

```text
Request ──► Route Handler / Server Action
              │ 1. do the DB write (commit)
              │ 2. enqueue job { orderId }
              ▼
            response 202/200 returns immediately
Queue (Redis/DB) ◄── Worker process (separate) ──► email API, image processing, ...
```

## 4. When

| Situation | Choice |
|---|---|
| Analytics event, log line, cache refresh after a mutation | `after` |
| Nightly cleanup, hourly sync | Cron to a Route Handler, or a queue scheduler |
| Email that must be sent, with retries | Durable queue |
| Video processing or any job over the function time limit | Durable queue plus a worker on a long-running host |
| Fan-out to many items | Queue (one job per item or a flow) |
| User-visible progress | Queue + [SSE](./01-server-sent-events.md) |

## 5. Practical

Pattern: signup sends a welcome email.

1. Server Action validates input and inserts the user (transaction commits).
2. `after()` is **not** enough if the email matters; enqueue `{ userId }` into the `email` queue.
3. Worker loads the user, sends the email, marks `welcomeSentAt` so a retry skips it.
4. If all retries fail, the job stays in the failed set and an alert fires.

## Common mistakes

| Mistake | Effect | Fix |
|---|---|---|
| Using `after` for must-not-lose work | Silent loss on crash or timeout | Durable queue |
| Calling `cookies()`/`headers()` inside `after` in a Server Component | Runtime error | Read first, close over values |
| No try/catch inside `after` | Errors vanish | Catch and log |
| Non-idempotent handler with retries | Double emails or double charges | Idempotency key, "done" marker |
| Enqueue before DB commit | Worker finds no row | Commit, then enqueue (or outbox) |
| Big objects in job payload | Stale data, memory use | Pass IDs |
| Unauthenticated cron endpoint | Anyone can trigger it | Secret header check |
| Workers inside serverless functions | Killed at duration limit | Separate long-running process |
| No retry cap | Poison job loops forever | `attempts` limit and a failed-job alert |

## Debugging

```text
Job did not run
  ├─ after(): was the response path reached? process alive long enough? check logs inside the callback
  ├─ Cron: did the scheduler call the URL? 401 from the secret check?
  ├─ Queue: job in waiting/delayed/failed? worker running and connected to the same Redis?
  └─ Ran twice → handler not idempotent; at-least-once is normal
```

## Quick Summary

- `after()` runs code after the response; it is best effort with no retry or persistence.
- Request APIs inside `after` work in Route Handlers and Server Functions, but must be read beforehand in Server Components.
- Cron endpoints must be authenticated and short; long work belongs in a queue.
- Use a durable queue when work must not be lost, needs retries, or is long.
- Design handlers to be idempotent, enqueue after commit, and run workers in their own process.

## Next

[04 · BullMQ](./04-bullmq.md)