# 02 · WebSockets

Two-way, persistent connections for chat, collaboration, multiplayer and anything where the client also sends frequent messages. The key fact for Next.js: the framework's Route Handlers are request/response, so a WebSocket server is something you add around it or next to it.

> The custom server API is from the Next.js 16.4 docs. The architecture guidance (separate process, Vercel behavior, auth handoff) comes from third-party guides (Fly.io and websocket.org) and general knowledge; platform limits change quickly, so confirm them with your host. Treat this note as a decision guide more than a recipe.

## 1. What

A WebSocket starts as an HTTP request with `Upgrade: websocket`; after the server accepts, the connection stays open and both sides can send messages at any time.

Next.js has no built-in, portable WebSocket server API for the App Router. A host may offer its own (one guide mentions an experimental upgrade helper on Vercel), but that is platform-specific.

## 2. Why this needs a decision

| Question | Why it matters |
|---|---|
| Where does the socket server run? | Serverless functions are built to start and stop quickly and do not suit long-lived connections |
| How many instances? | A socket pins to one instance; broadcasting needs shared state |
| Who is the user? | The browser `WebSocket` API cannot set custom headers |
| Do you need it at all? | SSE or polling is often enough |

## 3. How

### 3.1 Three architectures

```text
A. Separate WebSocket service        B. Custom Next.js server         C. Managed realtime service
   Next.js (UI + API)                   one Node process:                Next.js (UI + API)
        │ publish                       Next handler + ws                     │ publish via SDK/REST
   WS server ◄── browsers               ◄── browsers                     Ably / Pusher / ... ◄── browsers
   (Redis pub/sub between)
```

| | A. Separate service | B. Custom server | C. Managed service |
|---|---|---|---|
| Next.js can deploy serverless | Yes | No | Yes |
| Scales socket tier independently | Yes | No | Yes (vendor) |
| Next.js restarts drop sockets | No | Yes | No |
| You operate it | Yes | Yes | No |
| Cost and lock-in | Infra only | Infra only | Vendor fees |

A separate WebSocket process is generally the most portable production choice (per the websocket.org guide). Option B is the quickest for a self-hosted app or a prototype.

### 3.2 Option B: custom server with `ws`

The Next.js docs show a custom server that wraps the request handler:

```ts
// server.ts
import { createServer } from 'http'
import next from 'next'

const port = parseInt(process.env.PORT || '3000', 10)
const dev = process.env.NODE_ENV !== 'production'
const app = next({ dev })
const handle = app.getRequestHandler()

app.prepare().then(() => {
  createServer((req, res) => {
    handle(req, res)
  }).listen(port)
})
```

Add a `WebSocketServer` from the `ws` package in `noServer` mode and route the HTTP `upgrade` event yourself:

```ts
// server.ts
import { createServer } from 'http'
import { parse } from 'url'
import next from 'next'
import { WebSocketServer } from 'ws'

const port = parseInt(process.env.PORT || '3000', 10)
const dev = process.env.NODE_ENV !== 'production'
const app = next({ dev })
const handle = app.getRequestHandler()
const upgrade = app.getUpgradeHandler()

app.prepare().then(() => {
  const server = createServer((req, res) => handle(req, res))
  const wss = new WebSocketServer({ noServer: true })

  wss.on('connection', (socket) => {
    socket.on('message', (data) => {
      for (const client of wss.clients) {
        if (client.readyState === client.OPEN) client.send(data.toString())
      }
    })
  })

  server.on('upgrade', (req, socket, head) => {
    const { pathname } = parse(req.url ?? '')

    if (pathname === '/ws') {
      wss.handleUpgrade(req, socket, head, (ws) => wss.emit('connection', ws, req))
    } else {
      upgrade(req, socket, head) // lets Next.js handle its own upgrades (dev HMR)
    }
  })

  server.listen(port)
})
```

The Fly.io guide uses `nextApp.getUpgradeHandler()` for the dev hot-reload path (`/_next/webpack-hmr`); confirm that `getUpgradeHandler` exists in your Next.js version before relying on it, and test that HMR still works in `next dev` after adding your own `upgrade` listener.

Run it by changing the scripts, as the docs say:

```json
{
  "scripts": {
    "dev": "node server.js",
    "build": "next build",
    "start": "NODE_ENV=production node server.js"
  }
}
```

Costs from the docs and the guides:

- `server.js` is not compiled by Next.js, so it must run on your Node version as written (compile or use a runner for TypeScript).
- Standalone output mode does not trace custom server files, so the two cannot be used together.
- Some automatic optimizations are disabled, and Vercel does not run custom servers.
- You own connection limits, memory, health checks and graceful shutdown.

### 3.3 Option A: a separate WebSocket service

A small standalone Node process:

```ts
// ws-server/index.ts
import { WebSocketServer } from 'ws'

const wss = new WebSocketServer({ port: 8080 })

wss.on('connection', (socket, req) => {
  // verify the token here (section 3.5), then register the socket
  socket.on('message', (data) => { /* handle */ })
})
```

Next.js talks to it by publishing to a shared channel (Redis pub/sub, a queue, or an internal HTTP call). Because each connection pins to one instance, every instance subscribes to the channel and forwards messages to its own sockets. This is the same fan-out pattern as in [SSE](./01-server-sent-events.md).

### 3.4 Client

```tsx
'use client'

import { useEffect, useRef, useState } from 'react'

export function Chat({ url }: { url: string }) {
  const [messages, setMessages] = useState<string[]>([])
  const ws = useRef<WebSocket | null>(null)

  useEffect(() => {
    // Created in an effect: WebSocket does not exist during server rendering
    const socket = new WebSocket(url)
    ws.current = socket
    socket.onmessage = (e) => setMessages((m) => [...m, String(e.data)])
    return () => socket.close()
  }, [url])

  return (
    <>
      <ul>{messages.map((m, i) => <li key={i}>{m}</li>)}</ul>
      <button onClick={() => ws.current?.send('hello')}>Send</button>
    </>
  )
}
```

Unlike `EventSource`, the browser `WebSocket` does **not** reconnect on its own. Production code wraps it with reconnect, backoff and re-subscription, or uses a library (Socket.IO, or a vendor SDK). If a connection must survive client-side navigation, create it in a client provider placed in the root layout rather than in each page.

### 3.5 Authentication

The browser `WebSocket` constructor cannot set an `Authorization` header. Common approaches:

1. **Same-origin cookie:** the browser sends cookies on the upgrade request; validate the session in the `upgrade` handler. Also check the `Origin` header to block cross-site WebSocket hijacking.
2. **Short-lived ticket:** an authenticated Next.js route issues a signed token that expires in seconds; the client passes it in the URL (`wss://.../ws?ticket=...`); the socket server verifies it once on connect. The websocket.org guide describes this handoff. A token in a URL can appear in logs, which is why it should be single-use and short-lived.

After connect, authorize every message by the identity established at connect, never by IDs sent in the payload (the IDOR rule from [11 · Authorization](../11-authentication/05-authorization.md)).

### 3.6 Serverless and Vercel

Per the websocket.org guide at the time of writing: WebSockets on Vercel Functions are in beta, connections close at the function's maximum duration, and custom servers are ignored. Platforms change this, so check current docs. The portable answers are a separate WebSocket service or a managed realtime provider.

### 3.7 Production checklist

| Concern | Do |
|---|---|
| Liveness | Ping/pong or app-level heartbeat; drop dead sockets |
| Limits | Max payload size, max connections, per-socket rate limit |
| Shutdown | On SIGTERM, stop accepting, close sockets with a code, let clients reconnect |
| Scale | Redis (or similar) pub/sub between instances |
| Proxy | Forward `Upgrade` and `Connection` headers, raise idle timeouts |
| Validation | Validate every incoming message with a schema library such as Zod |
| Observability | Log connects, disconnects, close codes |

## 4. When

| Need | Choose |
|---|---|
| Server pushes only | [SSE](./01-server-sent-events.md) |
| Rare updates | Polling or `refetchInterval` |
| High-rate two-way traffic, self-hosted, one instance | Custom server with `ws` |
| Two-way traffic at scale or on serverless Next.js | Separate WebSocket service |
| Realtime without running infrastructure | Managed service |

## 5. Practical

1. Start with the requirement: do clients really send frequent messages? If not, use SSE.
2. Prototype with option B locally.
3. Before production, decide the deployment target; if it is serverless, move to A or C.
4. Add auth on connect, heartbeat, reconnect and message validation.

## Common mistakes

| Mistake | Effect | Fix |
|---|---|---|
| Building WebSockets on a serverless-only deployment | Connections drop or never upgrade | Separate service or managed provider |
| In-memory client list with several instances | Messages reach only some users | Pub/sub between instances |
| No reconnect logic on the client | Silent dead UI after a network blip | Reconnect with backoff |
| Trusting user IDs in message payloads | IDOR | Use the identity from connect |
| Not checking `Origin` on upgrade with cookie auth | Cross-site WebSocket hijacking | Allow-list origins |
| Creating the socket during render | `WebSocket is not defined` on the server | Create it in `useEffect` |
| Custom server with `output: 'standalone'` | Custom server not traced | Pick one, or build your own deployment |
| Overriding `upgrade` and breaking dev HMR | Hot reload stops | Forward non-matching upgrades to Next.js |
| No payload limit | Memory abuse | `maxPayload` option on the server |

## Debugging

```text
Socket will not connect
  ├─ Browser DevTools → Network → WS: status 101?        no → upgrade not handled / proxy strips headers
  ├─ Works locally, fails behind proxy/CDN                → forward Upgrade headers, timeouts
  ├─ Closes at a regular interval                         → idle timeout or platform max duration
  ├─ 'WebSocket is not defined'                           → created during SSR
  └─ Messages reach some clients only                     → multiple instances without pub/sub
```

## Quick Summary

- Next.js Route Handlers do not host WebSockets in a portable way; pick an architecture deliberately.
- Self-hosted prototype: custom server plus `ws`, forwarding unmatched upgrades to Next.js.
- Production at scale: a separate WebSocket service or a managed provider, with pub/sub between instances.
- The browser `WebSocket` has no auto-reconnect and no custom headers; plan auth and reconnect.
- If the traffic is one-way, use SSE instead.

## Next

[03 · Background Jobs](./03-background-jobs.md)