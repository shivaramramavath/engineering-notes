# 15 · Advanced Features

Features beyond a page that renders from a database: serving several languages, pushing live updates to the browser, and running work outside the request. Each note ends with the point where the feature stops being enough and what to use next.

> Verified against the Next.js 16.4 documentation (Internationalization, Streaming, Route Handlers, `after`, Custom Server) and the BullMQ documentation. WebSocket hosting guidance comes from third-party guides and platform limits change often; those parts are labelled in the notes.

## Start here: what do you need?

```text
Same site in several languages ───────────────► 00-i18n
Server pushes live updates (one way) ─────────► 01-server-sent-events
Two-way realtime (chat, collaboration) ───────► 02-websockets
Work after the response / on a schedule ──────► 03-background-jobs
Durable jobs with retries and workers ────────► 04-bullmq
```

## Reading order

| # | Note | Covers |
|---|---|---|
| 00 | [Internationalization](./00-i18n.md) | `app/[lang]`, locale redirect in `proxy.ts`, dictionaries, `next/root-params`, static params, `alternates` |
| 01 | [Server-Sent Events](./01-server-sent-events.md) | Streaming Route Handlers, `EventSource`, buffering pitfalls, fan-out, resume |
| 02 | [WebSockets](./02-websockets.md) | Why Route Handlers do not host them, custom server with `ws`, separate service, auth on upgrade |
| 03 | [Background Jobs](./03-background-jobs.md) | `after()`, cron endpoints, when you need a queue, idempotency, outbox |
| 04 | [BullMQ](./04-bullmq.md) | Queue and Worker, retries, backoff, schedulers, retention, deployment |

## How the pieces fit

```text
Browser ──HTTP──► Next.js ──add job──► Redis ◄── Worker process
   ▲                 │                              │
   │ SSE / WebSocket │ publish progress             │ events
   └─────────────────┴──────────────────────────────┘
```

A request enqueues a job and returns immediately. The worker does the slow part and publishes progress. A stream (SSE or WebSocket) shows it in the browser. i18n is orthogonal: it shapes every route and every message.

## The rules to remember

1. **Every special file lives under `app/[lang]`** when you localize with a dynamic segment, and the locale list must be one shared constant.
2. **Dictionaries are server-only data.** Pass translated strings to Client Components as props.
3. **Prefer SSE to WebSockets** when data flows one way; it works inside a normal Route Handler and reconnects by itself.
4. **Streaming is only as good as the layer that buffers it:** proxies, CDNs, compression and serverless platforms can all defeat it.
5. **Route Handlers do not host WebSockets portably.** Choose a custom server, a separate WebSocket service or a managed provider on purpose.
6. **`after()` is best effort.** No persistence, no retries; do not use it for work that must not be lost.
7. **Queues deliver at least once:** make processors idempotent and enqueue after the database commit.
8. **Workers run in their own process,** never inside the Next.js server or a serverless function.
9. **Limit job retention and set Redis to `noeviction`** so the queue neither grows without bound nor loses data.
10. **Authenticate at connect time** for every long-lived connection, and authorize each action by that identity.

## Next

[16 · Next chapter](../README.md): see the repo root README for the folder name.