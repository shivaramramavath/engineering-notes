# Realtime

Pushing data to clients the moment it changes, instead of waiting for them to ask.

## The problem: HTTP is request → response

Plain HTTP only lets the **client** start a conversation. The server can't say "a new message just arrived" unless the client happens to ask.

```
Client: "Any new messages?"   Server: "No."
Client: "Any new messages?"   Server: "No."
Client: "Any new messages?"   Server: "Yes, one."     ← up to a full interval late, and most requests were wasted
```

Realtime features need the opposite: the **server pushes** when something happens.

- Chat, comments, and live notifications
- Collaborative editing, shared cursors, multiplayer state
- Live dashboards, tickers, order tracking, driver locations
- "Your report is ready" after a background job (`11-async-processing/`)
- Presence ("Sam is online", "3 people viewing this page")

---

## The techniques, from simplest to most capable

| Technique | How it works | Direction | Good for | Weaknesses |
|---|---|---|---|---|
| **Polling** | Client asks every N seconds | Client → server | Low-frequency updates, simplest possible thing | Wasteful, laggy, hammers the server at scale |
| **Long polling** | Client asks; server holds the request open until there's news, then client immediately asks again | Server → client (simulated) | Fallback where nothing else works | Each message costs a full HTTP round trip; awkward state |
| **Server-Sent Events (SSE)** | One long-lived HTTP response; server streams `text/event-stream` | Server → client only | Notifications, live feeds, progress bars, streaming (LLM tokens) | One-way; limited to text; browser connection limits on HTTP/1.1 |
| **WebSocket** | One persistent, full-duplex TCP connection after an HTTP upgrade | Both directions | Chat, games, collaboration, anything interactive | Stateful connections are harder to scale and operate |
| **Socket.IO** | A library on top of WebSocket (with fallbacks) adding rooms, acknowledgements, reconnection | Both directions | Most Node.js realtime apps | Not plain WebSocket: needs the Socket.IO client too |
| **WebTransport / WebRTC** | HTTP/3 streams / peer-to-peer | Both | Low-latency media, games, P2P | Newer, more complex |

### Choosing

```
Does the client need to SEND frequent realtime messages too (chat, game inputs, cursors)?
├── Yes → WebSocket / Socket.IO
└── No: server → client only?
        ├── Yes → SSE is often simpler and sufficient (works over plain HTTP, auto-reconnects)
        └── Rarely updates (every minute+)? → Polling is fine. Don't over-engineer.
```

Many "realtime" features are really **notifications** (server → client only). SSE handles those with far less machinery. Reach for WebSockets when you genuinely need bidirectional, low-latency traffic.

A minimal SSE endpoint, for contrast:

```js
app.get("/events", requireAuth, (req, res) => {
  res.set({
    "Content-Type": "text/event-stream",
    "Cache-Control": "no-cache",
    Connection: "keep-alive",
  });
  res.flushHeaders();

  const send = (event, data) => res.write(`event: ${event}\ndata: ${JSON.stringify(data)}\n\n`);
  send("hello", { ok: true });

  const timer = setInterval(() => res.write(": keep-alive\n\n"), 25_000);   // comment line keeps proxies from closing it
  req.on("close", () => clearInterval(timer));
});
```

```js
// browser
const es = new EventSource("/events");           // reconnects automatically
es.addEventListener("hello", (e) => console.log(JSON.parse(e.data)));
```

---

## What's in this section

| File | What you learn |
|---|---|
| `01-websocket-and-socketio.md` | How WebSockets work, raw `ws`, Socket.IO fundamentals: events, acknowledgements, namespaces, reconnection |
| `02-auth-and-rooms.md` | Authenticating connections, authorizing every event, rooms, presence, validation, abuse protection |
| `03-scaling-with-redis-adapter.md` | Running many instances: sticky sessions, the Redis adapter, emitting from workers, deploys, limits |

Read in order. `01` gets messages flowing, `02` makes it safe, `03` makes it work across more than one server.

---

## Prerequisites

- `02-core-modules/03-http.md` and `02-core-modules/04-events.md`: Socket.IO's API is `EventEmitter`-shaped
- `05-http-web/02-headers-and-content-negotiation.md` and `05-http-web/04-cors.md`: the upgrade handshake and cross-origin rules
- `08-authentication-security/02-jwt-and-tokens.md` and `03-sessions.md`: how you identify a user on a connection
- `07-databases/redis/03-pub-sub.md`: the mechanism behind the Redis adapter
- `11-async-processing/`: how background results get pushed to users

---

## How realtime changes your assumptions

A normal Express request is **stateless and short-lived**: it arrives, you handle it, it's gone. A WebSocket connection is **stateful and long-lived**: it lives for minutes or hours, and your server holds memory for it the entire time.

| Stateless HTTP | Long-lived connections |
|---|---|
| Any instance can serve any request | A specific instance holds each connection |
| Auth checked on every request | Auth checked **once at connect**, then the connection is trusted, so you must re-verify authorization per event, and handle token expiry |
| Load balancer picks freely | May need **sticky sessions** |
| Deploy: drain in seconds | Deploy: thousands of clients disconnect and reconnect at once |
| Capacity ≈ requests/second | Capacity ≈ **concurrent connections** (memory, file descriptors) |
| Failure: one request fails | Failure: connection silently dies (mobile networks, sleeping laptops) and you must detect it |
| Scaling: add instances | Scaling: add instances **plus** a way for instances to talk to each other (`03`) |

Everything in this section follows from that table.

---

## A mental model for messages

Three directions of sending, which recur in every realtime library:

```
 ┌───────────┐   emit to ONE client       "tell this user their report is ready"
 │  Server   │──────────────────────────▶ client
 │           │   emit to a GROUP          "tell everyone in room 'project:42' about the new comment"
 │           │──────────────────────────▶ client, client, client
 │           │   emit to EVERYONE         "maintenance in 5 minutes"
 │           │──────────────────────────▶ all clients
 └───────────┘
        ▲
        │   client → server events: "send message", "typing", "move piece"
        │   (always untrusted input: validate and authorize each one)
```

Rooms (`02`) are how you address a group without tracking individual connections yourself.

---

## Design principles

1. **Use the simplest transport that does the job.** SSE or polling beats WebSockets when you only push occasional updates.
2. **Your database is the source of truth, not the socket.** Persist first, then broadcast. A client that was offline must be able to fetch what it missed over normal HTTP.
3. **Treat every incoming event like an HTTP request:** authenticate, authorize, validate, rate limit.
4. **Assume connections drop constantly.** Design for reconnection, replay, and idempotency (`09-api-development/06-idempotency.md`).
5. **Keep messages small and structured:** an event name plus a validated JSON payload. Send IDs and deltas, not whole objects.
6. **Never rely on delivery.** Sockets are best-effort. Anything important also lives in the database and in a "catch up since X" endpoint.
7. **Plan for horizontal scaling from the start,** because the in-memory model breaks the moment you run two instances.
8. **Close connections you don't need.** Idle sockets cost memory.

## Next

**`01-websocket-and-socketio.md`** shows how the WebSocket protocol works under the hood, then builds up a working Socket.IO server and client: events, acknowledgements, namespaces, and reconnection.
