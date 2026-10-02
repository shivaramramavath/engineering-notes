# WebSockets and Server-Sent Events

HTTP request/response is client-initiated. For **live updates** (chat, notifications, dashboards, collaboration, games) you need the server to **push** data. The main options:

| Technique | Direction | Transport | Notes |
|-----------|-----------|-----------|-------|
| **Polling** | client → server repeatedly | plain HTTP | simplest, wasteful, delayed |
| **Long polling** | client waits for the server to answer when data exists | HTTP | works everywhere, higher overhead |
| **Server-Sent Events (SSE)** | server → client (one way) | HTTP stream | built-in reconnect, text only |
| **WebSocket** | both directions | own protocol over one TCP connection | low latency, text and binary |
| **WebTransport** | both directions, streams and datagrams | HTTP/3 (QUIC) | newer, check support |
| **WebRTC data channels** | peer to peer | UDP-based | games, calls, file sharing |

## WebSocket basics

A WebSocket starts as an HTTP request with `Upgrade: websocket`; after the server answers `101 Switching Protocols`, the same TCP connection carries **bidirectional messages**.

```js
const socket = new WebSocket("wss://example.com/ws");        // use wss:// (TLS) in production

socket.addEventListener("open", () => {
  socket.send(JSON.stringify({ type: "subscribe", channel: "prices" }));
});

socket.addEventListener("message", (event) => {
  const msg = JSON.parse(event.data);                        // string, Blob or ArrayBuffer
  handle(msg);
});

socket.addEventListener("close", (event) => {
  console.log(event.code, event.reason, event.wasClean);
});

socket.addEventListener("error", () => { /* details are intentionally hidden; a close event follows */ });
```

### API summary

| Member | Meaning |
|--------|---------|
| `new WebSocket(url, protocols?)` | connect (sub-protocols optional) |
| `readyState` | `0` CONNECTING, `1` OPEN, `2` CLOSING, `3` CLOSED |
| `send(data)` | string, `Blob`, `ArrayBuffer`, typed array (only when OPEN) |
| `close(code?, reason?)` | start the closing handshake |
| `binaryType` | `"blob"` (default) or `"arraybuffer"` |
| `bufferedAmount` | bytes queued but not yet sent (backpressure signal) |
| `protocol`, `extensions`, `url` | negotiated info |

Common close codes: `1000` normal, `1001` going away, `1006` abnormal (no close frame, connection lost; not sendable), `1008` policy violation, `1011` server error, `4000-4999` application-defined.

## Message design

WebSocket gives you a raw message channel: **you define the protocol**.

```js
// envelope with a type and optional id for request/response matching
{ "type": "chat.message", "id": "c1", "data": { "room": "general", "text": "Hi" } }
{ "type": "ack", "replyTo": "c1" }
{ "type": "error", "code": "RATE_LIMITED", "message": "Slow down" }
```

Guidelines:

- Version your message types
- Validate every incoming message (schema validation) on **both** sides
- Include ids for acknowledgments, idempotency and ordering
- Use `binaryType = "arraybuffer"` and a compact format (MessagePack, Protobuf) for high-volume data

## A reconnecting client

Connections drop (mobile networks, deploys, proxies). Always reconnect with **exponential backoff and jitter**, and resubscribe.

```js
class ReconnectingSocket extends EventTarget {
  #url; #socket; #attempt = 0; #closedByUser = false; #queue = []; #heartbeat;

  constructor(url) { super(); this.#url = url; this.#connect(); }

  #connect() {
    this.#socket = new WebSocket(this.#url);

    this.#socket.onopen = () => {
      this.#attempt = 0;
      this.#queue.splice(0).forEach((m) => this.#socket.send(m));        // flush queued messages
      this.#startHeartbeat();
      this.dispatchEvent(new Event("open"));
    };

    this.#socket.onmessage = (e) => {
      const msg = JSON.parse(e.data);
      if (msg.type === "pong") return;
      this.dispatchEvent(new CustomEvent("message", { detail: msg }));
    };

    this.#socket.onclose = () => {
      clearInterval(this.#heartbeat);
      this.dispatchEvent(new Event("close"));
      if (this.#closedByUser) return;
      const delay = Math.min(30_000, 500 * 2 ** this.#attempt++) * (0.5 + Math.random() / 2);   // backoff + jitter
      setTimeout(() => this.#connect(), delay);
    };
  }

  #startHeartbeat() {
    clearInterval(this.#heartbeat);
    this.#heartbeat = setInterval(() => this.#socket.readyState === 1 && this.#socket.send('{"type":"ping"}'), 25_000);
  }

  send(msg) {
    const text = JSON.stringify(msg);
    if (this.#socket.readyState === WebSocket.OPEN) this.#socket.send(text);
    else this.#queue.push(text);
  }

  close() { this.#closedByUser = true; clearInterval(this.#heartbeat); this.#socket.close(1000); }
}
```

Libraries add features: **Socket.IO** (rooms, acks, fallbacks, auto-reconnect; uses its own protocol, so both sides need it), **reconnecting-websocket**, **PartySocket**, **ws** (Node server/client).

## Heartbeats and dead connections

Browsers cannot send protocol-level ping frames and may not notice a silently dead connection for a long time. Use **application-level ping/pong** and close the socket when no reply arrives within a timeout. Servers (`ws`) can send protocol pings and terminate unresponsive clients.

```js
// Node server with `ws`
import { WebSocketServer } from "ws";
const wss = new WebSocketServer({ port: 8080 });

wss.on("connection", (ws, req) => {
  ws.isAlive = true;
  ws.on("pong", () => { ws.isAlive = true; });
  ws.on("message", (data) => {
    const msg = JSON.parse(data);
    for (const client of wss.clients) if (client.readyState === 1) client.send(JSON.stringify(msg));   // broadcast
  });
});

setInterval(() => {
  for (const ws of wss.clients) {
    if (!ws.isAlive) { ws.terminate(); continue; }
    ws.isAlive = false;
    ws.ping();
  }
}, 30_000);
```

## Authentication

The browser WebSocket API **cannot set custom headers**.

| Approach | Notes |
|----------|-------|
| **Cookies** | sent automatically on the upgrade request (same-site); validate on the server; check `Origin` |
| **Short-lived ticket in the URL** | fetch a one-time token over HTTPS, then `new WebSocket(url + "?ticket=...")`; tokens in URLs can be logged, so keep them single-use and short-lived |
| **First message auth** | connect, then send `{ type: "auth", token }`; server ignores everything else until authenticated |
| **Sub-protocol trick** | pass a token via `Sec-WebSocket-Protocol`: works, but non-standard use |

Security checklist:

- Use **`wss://`**
- **Validate the `Origin`** header on the server (WebSockets are not protected by CORS: cross-site hijacking is possible with cookie auth)
- Authenticate and **authorize** each message/subscription
- Rate limit and cap message size
- Validate and sanitize all input; never trust client messages
- Clean up subscriptions when sockets close

## Scaling WebSockets

- Connections are **stateful** and long-lived: each consumes memory and a file descriptor
- Behind load balancers use **sticky sessions** or a pub/sub backbone so any server can reach any client
- Fan out through **Redis Pub/Sub**, NATS, Kafka or managed services (Ably, Pusher, AWS API Gateway WebSockets)
- Plan graceful shutdown: tell clients to reconnect (`1001`/custom code) so deploys do not drop everyone at once
- Watch **backpressure**: check `bufferedAmount` before sending bulk data

```js
function sendSafe(socket, data) {
  if (socket.bufferedAmount > 1_000_000) return false;      // skip or queue: the connection is congested
  socket.send(data);
  return true;
}
```

## Server-Sent Events (SSE)

SSE is a simple, **one-way** (server → client) stream over a normal HTTP response with `Content-Type: text/event-stream`. The browser's `EventSource` handles parsing and **automatic reconnection**.

### Client

```js
const source = new EventSource("/api/stream", { withCredentials: true });   // cookies for cross-origin

source.onopen = () => console.log("connected");
source.onmessage = (e) => console.log("message", e.data, e.lastEventId);      // events without a name
source.addEventListener("price", (e) => update(JSON.parse(e.data)));          // named events
source.onerror = () => { /* the browser retries automatically unless readyState is CLOSED */ };

source.close();                                                                 // stop reconnecting
```

### Wire format

```text
retry: 5000
id: 101
event: price
data: {"symbol":"ACME","value":42.1}

data: first line of a message
data: second line of the same message

: this is a comment, often used as a keep-alive ping

```

- Messages end with a **blank line**
- `id:` sets the **last event ID**; on reconnect the browser sends `Last-Event-ID` so the server can **resume**
- `retry:` sets the reconnect delay (ms)
- Text only (UTF-8): encode binary as Base64

### Server (Node.js, no framework)

```js
import http from "node:http";

http.createServer((req, res) => {
  if (req.url !== "/api/stream") { res.writeHead(404).end(); return; }

  res.writeHead(200, {
    "Content-Type": "text/event-stream",
    "Cache-Control": "no-cache, no-transform",
    Connection: "keep-alive",
    "X-Accel-Buffering": "no",                       // disable proxy buffering (nginx)
  });

  const lastId = Number(req.headers["last-event-id"] ?? 0);
  let id = lastId;
  const send = (event, data) => res.write(`id: ${++id}\nevent: ${event}\ndata: ${JSON.stringify(data)}\n\n`);

  const timer = setInterval(() => send("tick", { time: Date.now() }), 1000);
  const ping = setInterval(() => res.write(": keep-alive\n\n"), 15_000);

  req.on("close", () => { clearInterval(timer); clearInterval(ping); });   // client disconnected
}).listen(3000);
```

### SSE limits and workarounds

| Limitation | Workaround |
|------------|-----------|
| `EventSource` cannot set headers or use `POST` | use cookies, query tickets, or stream with `fetch()` and parse the body (libraries like `@microsoft/fetch-event-source`) |
| Max ~6 connections per origin on HTTP/1.1 (shared across tabs) | use HTTP/2 or HTTP/3 (many streams on one connection) |
| One-way only | send client messages with normal `fetch` calls |
| Proxies/CDNs may buffer or time out idle streams | send comments as keep-alive, disable buffering, set long timeouts |
| Text only | Base64 or JSON |

SSE is also used by many AI chat APIs to stream tokens (`fetch` + `ReadableStream` parsing).

## Streaming with `fetch` (SSE-like, any method/headers)

```js
const res = await fetch("/api/chat", { method: "POST", headers: { "Content-Type": "application/json" }, body: JSON.stringify({ prompt }), signal });
const reader = res.body.pipeThrough(new TextDecoderStream()).getReader();
let buffer = "";
while (true) {
  const { done, value } = await reader.read();
  if (done) break;
  buffer += value;
  let i;
  while ((i = buffer.indexOf("\n\n")) !== -1) {
    const frame = buffer.slice(0, i);
    buffer = buffer.slice(i + 2);
    const data = frame.split("\n").filter((l) => l.startsWith("data:")).map((l) => l.slice(5).trim()).join("\n");
    if (data) onMessage(JSON.parse(data));
  }
}
```

## Choosing the technique

| Need | Choose |
|------|--------|
| Occasional refresh, simple backend | **polling** (with `ETag`/`304`) |
| Server pushes notifications, feeds, progress, streaming text | **SSE** |
| Chat, collaboration, games, low-latency two-way | **WebSocket** |
| Binary streams, many parallel streams, unreliable datagrams | WebTransport (check support) |
| Peer-to-peer media/data | WebRTC |
| Work behind restrictive proxies, simple infra | SSE or long polling |

| | Polling | SSE | WebSocket |
|---|---------|-----|-----------|
| Direction | pull | server → client | both |
| Reconnect | n/a | **automatic** | you build it |
| Binary | yes | no (encode) | yes |
| Custom headers / methods | yes | limited | no (browser) |
| Works through HTTP infra | yes | mostly | usually, needs support for `Upgrade` |
| Complexity | lowest | low | highest |

## Pitfalls

| Pitfall | Why it hurts | Better |
|---------|--------------|--------|
| Using WebSocket when SSE would do | More infra and state | SSE for one-way pushes |
| No reconnect logic | Silent dead connections after network changes | Backoff + jitter + resubscribe |
| No heartbeat | Half-open connections unnoticed | Ping/pong with timeouts |
| Sending before `OPEN` | `InvalidStateError` | Check `readyState` / queue |
| Trusting `Origin` or messages blindly | Cross-site hijacking, injection | Validate origin and payloads |
| Tokens in long-lived URLs | Leaked via logs | Short-lived tickets or first-message auth |
| Unbounded broadcast queues | Memory growth | Backpressure with `bufferedAmount`, drop/coalesce |
| Proxy buffering SSE | Events arrive late or in bursts | Disable buffering, send keep-alives |
| Reconnect stampede after a deploy | Server overload | Jittered backoff, staggered restarts |
| Treating `error` event as informative | No details exposed | Log `close` code and reason |
| Forgetting cleanup on close | Leaked subscriptions/timers | Unsubscribe in `close` handlers |

## Key takeaways

- WebSocket = bidirectional, you design the protocol and reconnection; SSE = simple server push with built-in reconnect and `Last-Event-ID`
- Always use `wss://`, validate `Origin`, authenticate, and validate every message
- Reconnect with exponential backoff and jitter; add heartbeats
- Choose the simplest transport that satisfies the direction and latency needs

**Next:** [Networking Patterns](./06_networking-patterns.md)
