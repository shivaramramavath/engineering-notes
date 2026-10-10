# 01 · Server-Sent Events (SSE)

One-way, server-to-browser streaming over a normal HTTP response. A Route Handler returns a `ReadableStream`, and the browser's `EventSource` reads it and reconnects by itself.

> Verified against the Next.js 16.4 Streaming guide and the `route.js` reference (Route Handlers can stream with the Web Streams API; the guide names Server-Sent Events as a use case). The SSE wire format, `EventSource` behavior and the Redis fan-out pattern are web-platform and general knowledge, labelled as such.

## 1. What

SSE is a long-lived `GET` response with `Content-Type: text/event-stream`. The server writes text frames; the browser parses them into events.

```text
event: price
data: {"symbol":"ACME","value":101.2}
id: 42

: keep-alive comment

data: plain message with no event name
```

Rules of the format (web standard): fields are `event:`, `data:`, `id:`, `retry:`; a blank line ends a frame; a line starting with `:` is a comment, useful as a keep-alive.

## 2. Why

| Need | SSE | WebSocket |
|---|---|---|
| Server pushes updates (notifications, progress, live feed) | Fits | Works, heavier |
| Client sends frequent messages back | No (use normal `fetch`/Actions for that) | Fits |
| Works through ordinary HTTP infrastructure | Yes | Needs upgrade support |
| Automatic reconnect | Built in | You write it |
| Runs inside a Route Handler | Yes | Not natively (see [02](./02-websockets.md)) |

If data only flows server to client, SSE is usually the simplest real-time option.

## 3. How

### 3.1 The route handler

```ts
// app/api/events/route.ts
export const dynamic = 'force-dynamic' // see note below

export async function GET(request: Request) {
  const encoder = new TextEncoder()
  let interval: ReturnType<typeof setInterval>

  const stream = new ReadableStream({
    start(controller) {
      const send = (event: string, data: unknown, id?: string) => {
        let frame = `event: ${event}\n`
        if (id) frame += `id: ${id}\n`
        frame += `data: ${JSON.stringify(data)}\n\n`
        controller.enqueue(encoder.encode(frame))
      }

      send('hello', { ok: true })

      interval = setInterval(() => {
        send('tick', { at: Date.now() })
      }, 1000)

      // Keep proxies from closing an idle connection (general practice)
      const ping = setInterval(() => {
        controller.enqueue(encoder.encode(': ping\n\n'))
      }, 15000)

      // Client went away: stop work and close
      request.signal.addEventListener('abort', () => {
        clearInterval(interval)
        clearInterval(ping)
        controller.close()
      })
    },
    cancel() {
      clearInterval(interval)
    },
  })

  return new Response(stream, {
    headers: {
      'Content-Type': 'text/event-stream; charset=utf-8',
      'Cache-Control': 'no-cache, no-transform',
      Connection: 'keep-alive',
      'X-Accel-Buffering': 'no',
    },
  })
}
```

Points to check against your version:

- Handlers return a standard `Response` whose body is a `ReadableStream`; this is the pattern in the Next.js streaming guide.
- `GET` handlers are dynamic by default since v15 (route reference version history), so `dynamic` is belt and braces. Under Cache Components, avoid caching a streaming route.
- `request.signal` is the Web `Request` abort signal; it is how you detect that the client disconnected. It is not shown in the route reference, so confirm cleanup works in your deployment by logging in the abort handler.
- `Cache-Control: no-cache, no-transform` and `X-Accel-Buffering: no` are common practice to stop proxies from caching or buffering the stream. The Next.js guide documents the `X-Accel-Buffering` header for Nginx.
- `Connection: keep-alive` is meaningful on HTTP/1.1 only; HTTP/2 ignores it.

### 3.2 Why buffering is the main enemy

The Next.js streaming guide lists what can break streaming even when your code is right:

| Layer | Problem | Fix |
|---|---|---|
| Nginx and similar | Buffers responses | `X-Accel-Buffering: no` |
| CDN | May buffer whole responses | Check provider streaming support |
| Serverless | Some do not stream (AWS Lambda needs response streaming mode enabled) | Use a platform that supports it, or a long-running server |
| Compression | Gzip/Brotli can hold chunks back | Make sure the layer flushes, or exclude this route |
| Safari/WebKit | Buffers until 1024 bytes | Matters only for tiny demos |
| `curl` | Buffers lines | `curl -N` and send newlines |

Test with `curl -N http://localhost:3000/api/events` first, then through your real proxy.

### 3.3 The browser side

```tsx
'use client'

import { useEffect, useState } from 'react'

export function LiveTicks() {
  const [ticks, setTicks] = useState<number[]>([])

  useEffect(() => {
    const es = new EventSource('/api/events')

    es.addEventListener('tick', (e) => {
      const { at } = JSON.parse((e as MessageEvent).data)
      setTicks((t) => [...t.slice(-19), at])
    })

    es.onerror = () => {
      // EventSource retries automatically; only log or show a status here
    }

    return () => es.close() // important: close on unmount
  }, [])

  return <ul>{ticks.map((t) => <li key={t}>{t}</li>)}</ul>
}
```

Browser behavior (web standard):

- `EventSource` reconnects after a drop, waiting about 3 seconds unless the server sends `retry: <ms>`.
- On reconnect it sends a `Last-Event-ID` header with the last `id:` it saw, so the server can resume.
- It only does `GET`, cannot set custom headers, and sends cookies for same-origin requests. For auth, rely on the session cookie (see [11 · Authentication](../11-authentication/README.md)) and validate it in the handler.
- Each tab opens its own connection. Browsers limit concurrent HTTP/1.1 connections per host (small number), which HTTP/2 largely removes.

### 3.4 Authenticate and authorize in the handler

```ts
import { verifySession } from '@/lib/dal'

export async function GET(request: Request) {
  const session = await verifySession() // throws or redirects if invalid
  // only stream this user's events
}
```

Auth runs once, at connect. A session that expires mid-stream is not re-checked unless you do it yourself; end long streams on a timer and let the client reconnect (which re-authenticates).

### 3.5 Fan-out across many instances

An in-memory `Set` of connections only sees clients connected to **this** process. With several instances or serverless functions, publish events through a shared channel (Redis pub/sub, Postgres `LISTEN/NOTIFY`, a queue) and have each instance forward them to its own clients. General pattern:

```text
worker / action ──publish──► Redis channel ──► every instance ──► its SSE clients
```

A BullMQ worker, covered in [04](./04-bullmq.md), is a natural publisher of progress events.

### 3.6 Resume with `id`

```ts
const lastId = request.headers.get('last-event-id')
// replay events with id > lastId from your store, then continue live
```

This only works if you keep a short event history; otherwise clients simply miss events during a disconnect.

### 3.7 Duration limits

Hosts cap how long a function may run (`maxDuration` segment config, platform limits). A stream that outlives it is closed, and `EventSource` reconnects. Design for reconnects, do not treat the connection as permanent.

## 4. When

| Use SSE for | Avoid it for |
|---|---|
| Notifications, live dashboards, job progress, log tailing | Chat with client-to-server traffic at high rate |
| Streaming LLM text to the client (also see the AI SDK mentioned in the route reference) | Binary data |
| Anything where a poll every few seconds would work but you want it instant | Environments that cannot hold long responses |

Polling (or TanStack Query `refetchInterval`, see [10 · State Management](../10-state-management/README.md)) is still the right call when updates are rare and simplicity wins.

## 5. Practical

Progress bar for a background job:

1. Client posts a job request; Server Action or handler enqueues it and returns a `jobId`.
2. Client opens `new EventSource('/api/jobs/' + jobId + '/events')`.
3. Worker publishes `progress` and `done` events on a Redis channel named for the job.
4. The SSE handler subscribes to that channel and forwards frames; on `done` it closes the stream.
5. Client calls `es.close()` on `done`, otherwise `EventSource` would reconnect forever.

## Common mistakes

| Mistake | Effect | Fix |
|---|---|---|
| Missing `\n\n` after a frame | Client never fires the event | End every frame with a blank line |
| Never closing `EventSource` | Reconnect loop and leaked connections | `es.close()` in cleanup and on `done` |
| No cleanup on abort | Timers and subscriptions leak per client | `request.signal` abort handler and `cancel()` |
| Proxy or CDN buffering | Events arrive in bursts | Disable buffering, test behind real infra |
| In-memory subscriber list on multi-instance deploy | Clients miss events | Shared pub/sub |
| Sending secrets or other users' data on a shared channel | Data leak | Per-user channels and authorization at connect |
| Cached route | Same stream replayed | Dynamic route, `no-cache` |
| Putting `JSON.stringify` output containing newlines in `data:` | Broken frames | `JSON.stringify` has no raw newlines; for plain text, one `data:` line per line |

## Debugging

```text
No events in browser
  ├─ curl -N the route: do frames print live?          no → handler/stream bug
  ├─ Frames print in bursts through the proxy?          → buffering (headers, compression, CDN)
  ├─ DevTools Network → EventStream tab: frames listed? no → wrong Content-Type
  └─ Reconnects every few seconds?                      → server closes early (duration cap, error), or stream ends
```

## Quick Summary

- SSE = a streaming `GET` response of `text/event-stream` frames from a Route Handler.
- Clean up on `request.signal` abort; close `EventSource` on the client.
- Buffering by proxies, CDNs, compression or serverless is the usual failure.
- Authenticate at connect; fan out across instances with a shared channel.
- Use WebSockets only when the client must send a lot back.

## Next

[02 · WebSockets](./02-websockets.md)