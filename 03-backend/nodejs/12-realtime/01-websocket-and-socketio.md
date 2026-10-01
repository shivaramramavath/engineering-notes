# WebSocket & Socket.IO

How a persistent two-way connection works, how to use raw WebSockets with `ws`, and how Socket.IO builds events, acknowledgements, namespaces, and reconnection on top.

## How WebSockets work

A WebSocket starts life as an ordinary HTTP request that asks to be **upgraded**:

```
Client                                               Server
  │  GET /chat HTTP/1.1                                 │
  │  Upgrade: websocket                                 │
  │  Connection: Upgrade                                │
  │  Sec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==        │
  │  Sec-WebSocket-Version: 13                          │
  │  Origin: https://app.example.com                    │
  │ ───────────────────────────────────────────────────▶│
  │                                                     │
  │  HTTP/1.1 101 Switching Protocols                   │
  │  Upgrade: websocket                                 │
  │  Sec-WebSocket-Accept: s3pPLMBiTxaQ9kYGzzhZRbK+xOo= │
  │ ◀───────────────────────────────────────────────────│
  │                                                     │
  │ ◀═══════ full-duplex frames in both directions ════▶│  (same TCP connection, no more HTTP)
```

After `101 Switching Protocols`, the connection is no longer HTTP. Both sides can send **frames** (text, binary, ping, pong, close) at any time with just a few bytes of overhead each, rather than full HTTP headers per message.

Key facts:

| Fact | Consequence |
|---|---|
| Starts as HTTP, so cookies, `Origin`, and query strings arrive in the handshake | The **handshake is your only chance to authenticate with standard HTTP mechanisms** (`02-auth-and-rooms.md`) |
| `ws://` is plaintext, `wss://` is TLS | **Always use `wss://`** in production |
| Browser `WebSocket` API **cannot set custom headers** | No `Authorization: Bearer ...` header from browser JS; use cookies, a short-lived ticket, or auth after connect |
| Not subject to CORS | Any website can try to open a socket to your server using the victim's cookies (**Cross-Site WebSocket Hijacking**). You must check `Origin` yourself |
| Messages are framed, not streams | You send discrete messages (text or binary), and the library delivers them whole |
| Has built-in ping/pong frames | Used to detect dead connections (below) |
| Close frames carry a code | `1000` normal, `1001` going away, `1006` abnormal (no close frame), `1008` policy violation, `1009` message too big, `1011` server error |

### Connections die silently

Mobile networks switch, laptops sleep, NAT tables expire, and proxies kill idle connections, usually **without either side being told**. A TCP connection can look open for minutes after the other end is gone. Hence **heartbeats**: periodically send a ping and expect a pong, and drop the connection if nothing comes back.

---

## Raw WebSockets with `ws`

`ws` is the de-facto Node.js WebSocket library: small, fast, spec-compliant, and no extras.

```bash
npm install ws
```

### A server

```js
import { createServer } from "node:http";
import { WebSocketServer, WebSocket } from "ws";

const server = createServer();                        // you can attach to an Express app's server too
const wss = new WebSocketServer({ server, path: "/ws", maxPayload: 64 * 1024 });   // reject frames > 64 KB

wss.on("connection", (ws, req) => {
  console.log("client connected from", req.socket.remoteAddress);
  ws.isAlive = true;

  ws.on("pong", () => { ws.isAlive = true; });        // reply to our heartbeat ping

  ws.on("message", (data, isBinary) => {
    let msg;
    try {
      msg = JSON.parse(data.toString());              // `data` is a Buffer
    } catch {
      return ws.close(1003, "Invalid JSON");          // 1003: unsupported data
    }

    if (msg.type === "chat") {
      // broadcast to everyone else
      for (const client of wss.clients) {
        if (client !== ws && client.readyState === WebSocket.OPEN) {
          client.send(JSON.stringify({ type: "chat", text: msg.text }));
        }
      }
    }
  });

  ws.on("close", (code, reason) => console.log("closed", code, reason.toString()));
  ws.on("error", (err) => console.error("socket error", err));       // ALWAYS handle: unhandled 'error' crashes the process

  ws.send(JSON.stringify({ type: "welcome" }));
});

// Heartbeat: every 30s, terminate connections that didn't answer the previous ping
const interval = setInterval(() => {
  for (const ws of wss.clients) {
    if (!ws.isAlive) { ws.terminate(); continue; }
    ws.isAlive = false;
    ws.ping();
  }
}, 30_000);

wss.on("close", () => clearInterval(interval));

server.listen(3000);
```

### A browser client

```js
const ws = new WebSocket("wss://api.example.com/ws");

ws.addEventListener("open", () => ws.send(JSON.stringify({ type: "chat", text: "hello" })));
ws.addEventListener("message", (e) => console.log(JSON.parse(e.data)));
ws.addEventListener("close", (e) => console.log("closed", e.code));
ws.addEventListener("error", (e) => console.error("error", e));
```

Notice what you'd have to build yourself with the raw API:

- **Reconnection** with backoff, and re-subscribing after reconnect
- **Event names and routing** (`msg.type` switch statements)
- **Acknowledgements** ("did the server get this?")
- **Rooms / groups** and targeted broadcasts
- **Multi-instance broadcasting**
- **Fallbacks** for networks that block WebSockets

That's exactly the gap Socket.IO fills.

### Backpressure with raw `ws`

`ws.send()` doesn't block: it queues. If a client is slow and you keep sending, memory grows unboundedly. Check `ws.bufferedAmount` and skip, drop, or disconnect slow consumers:

```js
if (client.bufferedAmount > 1_000_000) {       // > 1 MB queued for this client
  client.terminate();                           // or skip non-critical updates (e.g., cursor positions)
  continue;
}
client.send(payload);
```

---

## Socket.IO

Socket.IO is a **realtime framework**, not just a WebSocket wrapper. It uses WebSocket when possible (and falls back to HTTP long polling), and adds the features above.

> ⚠️ A Socket.IO server is **not** a plain WebSocket server. A raw `new WebSocket(...)` client can't talk to it, and a Socket.IO client can't talk to a plain `ws` server. Use the matching pair.

```bash
npm install socket.io               # server
npm install socket.io-client        # client (or load it from a CDN in the browser)
```

### A minimal server with Express

```js
// server.js
import express from "express";
import { createServer } from "node:http";
import { Server } from "socket.io";

const app = express();
const httpServer = createServer(app);        // Socket.IO needs the HTTP server, not just the Express app

const io = new Server(httpServer, {
  cors: {
    origin: ["https://app.example.com"],     // exact origins, never "*" together with credentials
    credentials: true,
  },
  maxHttpBufferSize: 1e5,                    // 100 KB max message size (default 1 MB)
  pingInterval: 25_000,                      // heartbeat: server pings every 25 s
  pingTimeout: 20_000,                       // ...and drops the client if no pong within 20 s
});

io.on("connection", (socket) => {
  console.log("connected:", socket.id);

  socket.on("disconnect", (reason) => console.log("disconnected:", socket.id, reason));
});

app.get("/health", (req, res) => res.sendStatus(200));

httpServer.listen(3000);
```

### A client

```js
import { io } from "socket.io-client";

const socket = io("https://api.example.com", {
  withCredentials: true,                     // send cookies cross-origin
  // transports: ["websocket"],              // skip polling (see 03-scaling-with-redis-adapter.md)
});

socket.on("connect", () => console.log("connected as", socket.id));
socket.on("disconnect", (reason) => console.log("disconnected:", reason));
socket.on("connect_error", (err) => console.error("connection failed:", err.message));
```

The client **reconnects automatically** with exponential backoff and jitter. By default it retries forever.

---

## Events: the core API

Socket.IO's messaging model is named events with arguments, just like `EventEmitter` (`02-core-modules/04-events.md`).

```js
// server
io.on("connection", (socket) => {
  // listen for an event from THIS client
  socket.on("chat:send", (payload) => {
    console.log(payload);                    // { room: "general", text: "hi" }
  });

  // send to THIS client only
  socket.emit("chat:welcome", { message: "Hello!" });
});
```

```js
// client
socket.emit("chat:send", { room: "general", text: "hi" });
socket.on("chat:welcome", (data) => console.log(data.message));
```

Events can carry **any JSON-serializable arguments** (and `Buffer`/`ArrayBuffer` for binary).

### The three ways to address recipients

```js
io.on("connection", (socket) => {
  socket.emit("evt", data);                   // 1. back to the sender only

  socket.broadcast.emit("evt", data);         // 2. everyone EXCEPT the sender

  io.emit("evt", data);                       // 3. everyone, including the sender
});
```

Combined with rooms (`02-auth-and-rooms.md`):

```js
io.to("project:42").emit("comment:added", comment);           // everyone in a room
socket.to("project:42").emit("typing", { userId });            // everyone in the room except the sender
io.to("user:7").emit("notification", n);                       // all of one user's devices
io.to("room1").to("room2").emit("evt", data);                  // union of rooms
io.except("banned").emit("evt", data);                         // everyone except a room
```

### Naming events

Namespace event names with a colon for readability: `chat:send`, `chat:message`, `presence:update`, `order:status`. Avoid the reserved names `connect`, `connection`, `disconnect`, `disconnecting`, `connect_error`, `error`, and `newListener`/`removeListener`.

---

## Acknowledgements: request/response over a socket

Plain events are fire-and-forget. Add a **callback as the last argument** to get an acknowledgement.

```js
// server: respond via the callback
socket.on("chat:send", async (payload, callback) => {
  try {
    const message = await messageService.create({ userId: socket.data.userId, ...payload });
    callback({ ok: true, id: message.id });
  } catch (err) {
    callback({ ok: false, error: "Could not send message" });
  }
});
```

```js
// client: pass a callback
socket.emit("chat:send", { room: "general", text: "hi" }, (response) => {
  if (response.ok) markDelivered(response.id);
  else showError(response.error);
});
```

With a **timeout**, so a lost ack doesn't leave the UI waiting forever:

```js
// client
try {
  const response = await socket.timeout(5000).emitWithAck("chat:send", { room: "general", text: "hi" });
  // response is whatever the server's callback sent
} catch (err) {
  // timed out: the server didn't answer in 5 s (it may or may not have processed it!)
}
```

Server-to-client acknowledgements work the same way (`socket.timeout(5000).emit("evt", data, (err, response) => {})`), useful for confirming that a client actually received something.

### An ack is not a transaction

If the ack times out, you **don't know** whether the server processed the message. Treat sends as at-least-once: include a **client-generated message ID** and make the server dedupe on it (`09-api-development/06-idempotency.md`):

```js
const clientMsgId = crypto.randomUUID();
socket.timeout(5000).emit("chat:send", { clientMsgId, text }, (err, res) => {
  if (err) retryLater(clientMsgId);           // safe to resend: the server ignores duplicates
});
```

Also make server handlers **always call the callback** (including on errors), and guard against clients that omit it: `if (typeof callback === "function") callback(...)`.

---

## Namespaces

A **namespace** is a separate communication channel multiplexed over the same connection. Each namespace has its own events, middleware, and rooms.

```js
// server
const chat = io.of("/chat");
const admin = io.of("/admin");

chat.on("connection", (socket) => { /* chat features */ });

admin.use(requireAdminSocket);                 // namespace-level middleware (02-auth-and-rooms.md)
admin.on("connection", (socket) => { /* admin dashboard feed */ });
```

```js
// client
const chatSocket = io("https://api.example.com/chat");
const adminSocket = io("https://api.example.com/admin");
```

Use namespaces to separate **concerns with different auth rules or lifecycles** (public notifications vs admin tooling). Don't use them for per-user or per-room grouping: that's what **rooms** are for. (`/` is the default namespace.)

---

## Connection lifecycle and reconnection

### Client-side states

```js
socket.on("connect",    () => { /* connected (also fires after every reconnect) */ });
socket.on("disconnect", (reason) => { /* ... */ });
socket.on("connect_error", (err) => { /* handshake or middleware rejected, or the server is unreachable */ });

socket.io.on("reconnect_attempt", (n) => { /* ... */ });
socket.io.on("reconnect", (n) => { /* ... */ });
```

Disconnect `reason` values to know:

| Reason | Meaning | Client auto-reconnects? |
|---|---|---|
| `io server disconnect` | The server called `socket.disconnect()` | **No**: you must call `socket.connect()` yourself |
| `io client disconnect` | The client called `socket.disconnect()` | No |
| `ping timeout` | Server stopped answering heartbeats | Yes |
| `transport close` / `transport error` | Network died / connection error | Yes |

### What does NOT survive a reconnect

A reconnect gives the client a **new `socket.id`** and a **fresh server-side socket**:

- It is **no longer in any rooms**, so you must rejoin on every `connect`.
- Any server-side state attached to the old socket is gone.
- Events emitted while it was offline were not delivered (unless you enable state recovery or replay them yourself).

```js
// client: re-establish subscriptions on EVERY connect, not just the first
socket.on("connect", () => {
  socket.emit("rooms:join", currentRoomIds);
  fetchMissedMessages(lastSeenMessageId);      // catch up over plain HTTP
});
```

```js
// server: rejoin rooms based on identity, not on the old socket
io.on("connection", async (socket) => {
  socket.join(`user:${socket.data.userId}`);                   // derive from the authenticated user
  for (const projectId of await projectIdsFor(socket.data.userId)) socket.join(`project:${projectId}`);
});
```

### Messages sent while offline

Socket.IO clients **buffer** events emitted while disconnected and flush them on reconnect (this is a default). That's convenient but can surprise you: a user's "delete" click from five minutes ago fires when they reconnect. For actions that shouldn't be replayed late, use `socket.volatile.emit(...)` (dropped if not connected) or check `socket.connected` first.

### Connection state recovery

Socket.IO 4.6+ can restore a client's rooms and replay missed events after a *short* disconnection:

```js
const io = new Server(httpServer, {
  connectionStateRecovery: {
    maxDisconnectionDuration: 2 * 60 * 1000,      // keep state for 2 minutes
    skipMiddlewares: true,                         // don't re-run auth middleware on recovery (see below)
  },
});

io.on("connection", (socket) => {
  if (socket.recovered) {
    // rooms and missed events were restored automatically
  } else {
    // new or unrecoverable session: do your normal setup (join rooms, send initial state)
  }
});
```

Limits: it works with the default in-memory adapter and the Redis **Streams** adapter (not the classic Redis Pub/Sub adapter, see `03-scaling-with-redis-adapter.md`), only for brief gaps, and is not guaranteed. **Your database remains the source of truth**: always keep a "give me everything since message X" endpoint as the real catch-up path.

### Reconnection tuning (client)

```js
const socket = io(url, {
  reconnection: true,
  reconnectionAttempts: Infinity,
  reconnectionDelay: 1000,          // first retry after ~1s
  reconnectionDelayMax: 30_000,     // back off up to 30s
  randomizationFactor: 0.5,         // jitter: avoids thundering herd when a server restarts
});
```

Jitter matters: after a deploy, thousands of clients reconnect at once. Randomized delays spread the load (`03-scaling-with-redis-adapter.md`).

---

## Per-socket data and utility methods

```js
io.on("connection", async (socket) => {
  socket.data.userId = "u_7";                       // arbitrary per-socket state (works across instances with the adapter)

  socket.id;                                         // unique per connection
  socket.rooms;                                      // Set: includes its own id as a private room
  socket.handshake.headers;                          // headers from the upgrade request
  socket.handshake.auth;                             // the client's `auth` payload
  socket.handshake.address;                          // client IP (behind a proxy: configure it, see 03)

  const sockets = await io.in("project:42").fetchSockets();       // sockets in a room (works across instances)
  const count = (await io.in("project:42").fetchSockets()).length;

  socket.disconnect();                               // kick a client
  io.in("user:7").disconnectSockets();               // kick all of a user's connections (e.g. on logout / ban)
});
```

Use `socket.data` instead of attaching custom properties to the socket object: it's the supported, cluster-aware place for per-connection state.

---

## Where Socket.IO fits in an Express app

Keep realtime code organized like the rest of the app (`10-architecture/`):

```
src/
├── realtime/
│   ├── index.js              ← createRealtime(httpServer, deps): builds the Server, registers middleware
│   ├── middleware/
│   │   └── auth.js           ← socket authentication (02-auth-and-rooms.md)
│   ├── handlers/
│   │   ├── chat.handlers.js  ← registerChatHandlers(io, socket, deps)
│   │   └── presence.handlers.js
│   └── emitter.js            ← how services push events out
├── modules/...
├── app.js
└── server.js
```

```js
// realtime/index.js
import { Server } from "socket.io";
import { authenticateSocket } from "./middleware/auth.js";
import { registerChatHandlers } from "./handlers/chat.handlers.js";
import { registerPresenceHandlers } from "./handlers/presence.handlers.js";

export function createRealtime(httpServer, deps) {
  const io = new Server(httpServer, { cors: deps.corsOptions, maxHttpBufferSize: 1e5 });

  io.use(authenticateSocket(deps));

  io.on("connection", (socket) => {
    registerChatHandlers(io, socket, deps);
    registerPresenceHandlers(io, socket, deps);
  });

  return io;
}
```

```js
// realtime/handlers/chat.handlers.js: a handler module, testable with fakes
export function registerChatHandlers(io, socket, { chatService, logger }) {
  socket.on("chat:send", async (payload, callback) => {
    try {
      const message = await chatService.send(socket.data.userId, payload);   // persist FIRST
      io.to(`room:${message.roomId}`).emit("chat:message", message);          // then broadcast
      callback?.({ ok: true, id: message.id });
    } catch (err) {
      logger.warn({ err }, "chat:send failed");
      callback?.({ ok: false, error: "Could not send" });
    }
  });
}
```

```js
// server.js
const httpServer = createServer(app);
const io = createRealtime(httpServer, deps);
httpServer.listen(3000);
```

### Emitting from your REST endpoints and services

Realtime and HTTP often cooperate: an HTTP request changes data, then clients are notified.

```js
// A service shouldn't import Socket.IO directly: inject a small "publisher" port (10-architecture/04-dependency-injection.md)
export function makeCommentService({ commentRepository, realtime }) {
  return {
    async add(userId, projectId, text) {
      const comment = await commentRepository.create({ userId, projectId, text });   // 1. persist
      realtime.toRoom(`project:${projectId}`, "comment:added", comment);              // 2. notify
      return comment;
    },
  };
}

// realtime/emitter.js
export const makeRealtimePublisher = (io) => ({
  toRoom: (room, event, payload) => io.to(room).emit(event, payload),
  toUser: (userId, event, payload) => io.to(`user:${userId}`).emit(event, payload),
});
```

In tests, inject a fake publisher that records calls. And when the code that wants to emit runs in a **different process** (a BullMQ worker), you need the Redis emitter (`03-scaling-with-redis-adapter.md`).

---

## Message design

```js
// ✅ small, structured, versionable
socket.emit("order:status", { orderId: "ord_42", status: "shipped", at: "2026-09-30T10:15:00Z" });

// ❌ entire database rows, or huge arrays, on every change
io.emit("orders", await Order.find());
```

- Send **deltas and IDs**, and let clients fetch the rest over HTTP if needed.
- **Throttle/coalesce** high-frequency data (cursor positions, sensor readings): send at most N per second, or `volatile` emit so stale updates are dropped.
- **Include timestamps or sequence numbers** where order matters, since events can arrive out of order across reconnects.
- Make payloads **validated** on the way in (`02-auth-and-rooms.md`) and **versioned** if multiple client versions will be live.
- Keep binary payloads small; don't push large files through sockets (upload over HTTP, send a reference).

---

## Testing

```js
import { createServer } from "node:http";
import { Server } from "socket.io";
import { io as Client } from "socket.io-client";

describe("chat", () => {
  let httpServer, io, port;

  beforeAll((done) => {
    httpServer = createServer();
    io = new Server(httpServer);
    registerChatHandlers(io, /* needs per-socket wiring */);   // or: io.on("connection", (s) => registerChatHandlers(io, s, fakeDeps))
    httpServer.listen(() => { port = httpServer.address().port; done(); });
  });

  afterAll(() => { io.close(); httpServer.close(); });

  test("echoes an acknowledged message", async () => {
    const client = Client(`http://localhost:${port}`);
    await new Promise((r) => client.on("connect", r));

    const res = await client.timeout(2000).emitWithAck("chat:send", { roomId: "r1", text: "hi" });

    expect(res.ok).toBe(true);
    client.close();
  });
});
```

- Listen on **port `0`** (random free port) to avoid collisions.
- Always close clients, the `io` server, and the HTTP server, or Jest will hang with open handles.
- Test handlers in isolation with fake sockets for pure logic, and use real clients for protocol-level behavior (`13-testing/02-api-testing-and-mocking.md`).

---

## Raw WebSocket vs Socket.IO: how to choose

| | Raw `ws` | Socket.IO |
|---|---|---|
| Protocol | Standard WebSocket: any client in any language | Custom protocol on top: needs Socket.IO clients |
| Features | Bare messages | Events, acks, rooms, namespaces, reconnection, fallbacks, adapters |
| Reconnection/heartbeat | You write it | Built in |
| Multi-instance broadcast | You write it | Adapter (Redis, etc.) |
| Overhead | Minimal | Slightly more per message |
| Best for | Interop with non-JS clients, minimal footprint, custom protocols, IoT devices | Web apps where you control both ends and want productivity |

If third parties or non-JS clients must connect using the standard protocol, use `ws` (or `uWebSockets.js` for extreme scale). For most browser/app products you own end to end, Socket.IO saves a lot of work.

---

## Common mistakes

```js
// ❌ attaching Socket.IO to the Express app instead of the HTTP server
const io = new Server(app);                 // doesn't work: pass `httpServer`; call httpServer.listen, not app.listen
app.listen(3000);                           // ❌ then the Socket.IO server never receives connections

// ❌ using `cors: { origin: "*" }` with credentials → rejected by browsers, and unsafe anyway
// ❌ trusting the client: using `payload.userId` instead of the authenticated socket.data.userId
// ❌ broadcasting before persisting → clients see data that was never saved
// ❌ forgetting to rejoin rooms after reconnect
// ❌ registering handlers inside other handlers (a new listener on every event → memory leak, duplicate handling)
io.on("connection", (socket) => {
  socket.on("join", () => { socket.on("msg", ...); });    // ❌ stacks a new "msg" listener on each "join"
});

// ❌ not handling the ack callback being absent: callback is not a function → TypeError crashes the handler
// ❌ unhandled errors in async handlers (they don't reach Express error middleware) → wrap in try/catch
// ❌ unbounded payloads / no maxHttpBufferSize
// ❌ keeping per-socket state in module-level objects keyed by socket.id and never cleaning it up on disconnect
// ❌ assuming one connection per user: users have multiple tabs and devices
// ❌ sending the full dataset on every change
```

## Checklist

- [ ] `wss://` in production; Socket.IO attached to the **HTTP server**
- [ ] Explicit CORS origins; `maxHttpBufferSize` set to what you actually need
- [ ] Heartbeats configured (built into Socket.IO; manual with `ws`)
- [ ] Every event handler has try/catch, always acknowledges, and tolerates a missing callback
- [ ] Persist first, then broadcast; a plain HTTP endpoint exists to catch up on missed data
- [ ] Rooms rejoined on every `connect`; no reliance on `socket.id` for identity
- [ ] Per-connection state in `socket.data`, cleaned up on `disconnect`
- [ ] Client sends IDs for idempotency; server dedupes retries
- [ ] Realtime code organized in modules, with an injectable publisher for services

## Next

**`02-auth-and-rooms.md`** makes all of this safe: authenticating the handshake (cookies, JWT, tickets), authorizing every event, organizing users into rooms, tracking presence, validating payloads, and rate limiting socket events.
