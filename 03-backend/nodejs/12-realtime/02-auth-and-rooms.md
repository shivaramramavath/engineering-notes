# Auth & Rooms

Making realtime connections safe — authenticating who connects, authorizing every event, validating input, limiting abuse — and organizing users into rooms and presence.

## Why sockets need their own security thinking

With REST, every request carries credentials and passes through your middleware stack. A socket is different:

```
HTTP:       request ─▶ auth ─▶ authz ─▶ validate ─▶ handler      (every single request)

WebSocket:  handshake ─▶ auth ──────┐
                                    ▼
            event, event, event, event, event ... for hours     (the connection is just "trusted" afterward)
```

Authentication typically happens **once, at connect time**. After that, any message on that connection is assumed to come from the authenticated user, so you must:

1. **Authenticate the handshake** (who is this?)
2. **Check the Origin** (is this a website we trust?), because WebSockets bypass CORS
3. **Authorize every event** (may *this user* do *this action on this resource*?)
4. **Validate every payload** (it's untrusted input, exactly like an HTTP body)
5. **Rate limit events** (one socket can send thousands of messages a second)
6. **Handle long-lived credentials** (tokens expire; users get banned or log out)

Everything in `08-authentication-security/` still applies, with extra care because connections outlive requests.

---

## 1. Authenticating the connection

### The browser limitation

The browser `WebSocket` constructor **cannot set custom headers**, so you can't send `Authorization: Bearer <token>` the way you do with `fetch`. Socket.IO works around this with an `auth` payload sent in the handshake, but the underlying problem still shapes your options.

| Method | How | Notes |
|---|---|---|
| **Session cookie** | Browser sends cookies automatically with the handshake | Easiest for same-site apps with `express-session` (`08-authentication-security/03-sessions.md`). Needs Origin checking (CSWSH, below) |
| **Token in `auth`** | `io(url, { auth: { token } })`: Socket.IO sends it in the handshake body | The standard choice for JWT apps (`08-authentication-security/02-jwt-and-tokens.md`) |
| **Token in query string** | `?token=...` | ⚠️ Avoid: URLs end up in access logs, proxies, and browser history |
| **One-time ticket** | HTTP endpoint issues a short-lived, single-use ticket; client passes it on connect | Strongest for raw WebSockets where you can't use `auth` |
| **First-message auth** | Connect anonymously, send credentials as the first message, drop the connection if invalid or late | Works with any library; the unauthenticated window must be tiny |

### Socket.IO middleware (the main hook)

```js
// realtime/middleware/auth.js
import jwt from "jsonwebtoken";

export const authenticateSocket = ({ userRepository }) => async (socket, next) => {
  try {
    const token = socket.handshake.auth?.token;
    if (!token) return next(new Error("unauthenticated"));

    const payload = jwt.verify(token, process.env.JWT_ACCESS_SECRET, { algorithms: ["HS256"] });

    const user = await userRepository.findById(payload.sub);
    if (!user || user.disabled) return next(new Error("unauthenticated"));

    // Store identity where the SERVER controls it. Never trust data the client sends later.
    socket.data.userId = user.id;
    socket.data.role = user.role;
    socket.data.tokenExp = payload.exp;           // used to handle expiry below

    next();
  } catch (err) {
    // keep the message generic, so it doesn't reveal why (08-authentication-security/01-password-hashing.md)
    next(new Error("unauthenticated"));
  }
};
```

```js
io.use(authenticateSocket(deps));
```

Rejecting with `next(new Error(...))` makes the client emit `connect_error`:

```js
// client
const socket = io(url, { auth: { token: getAccessToken() } });

socket.on("connect_error", async (err) => {
  if (err.message === "unauthenticated") {
    const fresh = await refreshAccessToken();          // via your refresh-token flow
    if (!fresh) return redirectToLogin();
    socket.auth = { token: fresh };                     // update the auth payload...
    socket.connect();                                    // ...and try again
  }
});
```

For auth values that change between reconnects (an access token that rotates), pass a **function** so the freshest token is read on every attempt:

```js
const socket = io(url, {
  auth: (cb) => cb({ token: getAccessToken() }),
});
```

You can attach extra details to the error via `err.data`:

```js
const error = new Error("unauthenticated");
error.data = { code: "token_expired" };            // client reads err.data.code
next(error);
```

### Sharing your Express session

If the app already uses `express-session`, reuse it so sockets see the same logged-in user:

```js
import session from "express-session";

const sessionMiddleware = session({ /* store, secret, cookie options */ });
app.use(sessionMiddleware);

// Socket.IO 4.6+: run the session middleware on the underlying engine for each handshake
io.engine.use(sessionMiddleware);

io.use((socket, next) => {
  const sess = socket.request.session;
  if (!sess?.userId) return next(new Error("unauthenticated"));
  socket.data.userId = sess.userId;
  next();
});
```

Remember that `socket.request.session` is a **snapshot from connect time**. If the user logs out in another tab, this socket still has a copy. See "Session lifetime" below.

### Raw `ws`: authenticate in the `upgrade` handler

With the `ws` library, reject unauthenticated upgrades **before** the socket exists:

```js
import { WebSocketServer } from "ws";

const wss = new WebSocketServer({ noServer: true });

server.on("upgrade", async (req, socket, head) => {
  try {
    const url = new URL(req.url, "http://localhost");
    const ticket = url.searchParams.get("ticket");                   // short-lived, single-use
    const userId = await redis.getDel(`ws-ticket:${ticket}`);        // GETDEL: consume atomically

    if (!userId || !originAllowed(req.headers.origin)) {
      socket.write("HTTP/1.1 401 Unauthorized\r\nConnection: close\r\n\r\n");
      return socket.destroy();
    }

    wss.handleUpgrade(req, socket, head, (ws) => {
      ws.userId = userId;
      wss.emit("connection", ws, req);
    });
  } catch {
    socket.destroy();
  }
});
```

```js
// the ticket endpoint: normal authenticated HTTP, so cookies/headers/CSRF protections all apply
app.post("/api/v1/realtime/ticket", requireAuth, async (req, res) => {
  const ticket = crypto.randomBytes(24).toString("base64url");
  await redis.set(`ws-ticket:${ticket}`, req.user.id, { EX: 30 });    // valid for 30 seconds, one use
  res.json({ data: { ticket } });
});
```

The ticket in the query string is safe-ish *because* it's single-use and expires in seconds, so a leaked URL is worthless.

---

## 2. Cross-Site WebSocket Hijacking: check the Origin

**WebSockets are not subject to CORS.** If your socket authenticates with cookies, any website the user visits can run:

```js
// on evil.example: the victim's browser attaches THEIR cookies for your domain
const ws = new WebSocket("wss://api.yourapp.com/socket");
ws.onmessage = (e) => exfiltrate(e.data);             // reads the victim's private realtime data
ws.onopen = () => ws.send('{"type":"transfer",...}'); // or performs actions as the victim
```

This is **Cross-Site WebSocket Hijacking (CSWSH)**, the WebSocket cousin of CSRF (`08-authentication-security/05-common-vulnerabilities.md`).

Defenses (combine them):

1. **Validate the `Origin` header** during the handshake against an allow-list.
2. Prefer **token-based auth** (`auth` payload, ticket), which an attacker's page can't obtain, over ambient cookies.
3. Use `SameSite=Lax`/`Strict` cookies where possible.

```js
const ALLOWED_ORIGINS = new Set(["https://app.example.com", "https://admin.example.com"]);

const io = new Server(httpServer, {
  cors: { origin: [...ALLOWED_ORIGINS], credentials: true },     // governs the HTTP long-polling transport

  // `cors` does NOT block WebSocket upgrades from other origins: enforce it here
  allowRequest: (req, callback) => {
    const origin = req.headers.origin;
    // Non-browser clients (mobile apps, server-to-server) may send no Origin; decide your policy deliberately
    callback(null, origin ? ALLOWED_ORIGINS.has(origin) : false);
  },
});
```

The `cors` option sets response headers for the polling transport only, and it's a common mistake to think it protects the WebSocket transport. `allowRequest` is the real gate.

---

## 3. Authorizing every event

Authentication says *who*. Authorization says *what they may do*. Do it **per event, on the server, from server-held identity**, exactly like an IDOR-safe REST endpoint (`06-express/06-auth-and-authorization.md`).

```js
// ❌ trusts the client to say who it is and where it may post
socket.on("chat:send", async ({ userId, roomId, text }) => {
  await messageService.create({ userId, roomId, text });
  io.to(`room:${roomId}`).emit("chat:message", { userId, text });
});
```

An attacker simply sends someone else's `userId`, or a `roomId` they were never invited to.

```js
// ✅ identity from socket.data; membership verified from the database
socket.on("chat:send", async (payload, callback) => {
  const parsed = sendSchema.safeParse(payload);                    // validate (below)
  if (!parsed.success) return callback?.({ ok: false, error: "invalid_payload" });

  const { roomId, text } = parsed.data;
  const userId = socket.data.userId;                                // never from the payload

  if (!(await roomRepository.isMember(roomId, userId))) {           // authorize against the source of truth
    return callback?.({ ok: false, error: "forbidden" });
  }

  const message = await messageService.create({ userId, roomId, text });
  io.to(`room:${roomId}`).emit("chat:message", message);
  callback?.({ ok: true, id: message.id });
});
```

Rules:

- **Identity comes from `socket.data`** set by your middleware, never from event payloads.
- **Resource access is checked against the database** (membership, ownership, role), not against "the client says it's in the room."
- Check **both read and write** access: who may *join* a room (to receive) and who may *send* to it.
- A user's permissions can change while connected (removed from a project, role downgraded). For high-stakes actions, re-check on each event rather than caching at connect.

### A reusable per-event guard

```js
// wrap handlers with validation + authentication assumptions in one place
export const guard = ({ schema, authorize }) => (handler) => async (payload, callback) => {
  const ack = typeof callback === "function" ? callback : () => {};
  try {
    const parsed = schema.safeParse(payload);
    if (!parsed.success) return ack({ ok: false, error: "invalid_payload" });

    if (authorize && !(await authorize(parsed.data))) return ack({ ok: false, error: "forbidden" });

    const result = await handler(parsed.data);
    ack({ ok: true, data: result });
  } catch (err) {
    logger.error({ err }, "socket handler failed");
    ack({ ok: false, error: "internal_error" });          // never leak err.message to the client
  }
};
```

---

## 4. Validate every payload

Socket events are **untrusted input**. A client can send anything: wrong types, giant strings, missing fields, extra fields, or objects where strings belong (injection, `08-authentication-security/05-common-vulnerabilities.md`).

```js
import { z } from "zod";

const sendSchema = z.object({
  roomId: z.string().uuid(),
  text: z.string().trim().min(1).max(2000),
  clientMsgId: z.string().uuid().optional(),             // idempotency key from the client
}).strict();                                              // reject unknown fields (mass assignment)
```

Apply it in every handler (or via the `guard` above). More in `09-api-development/04-validation.md`.

Also:

- **Set `maxHttpBufferSize`** (Socket.IO) or `maxPayload` (`ws`) so oversized messages are rejected before parsing.
- **Escape on output.** If clients render message text as HTML, XSS follows: sanitize/escape in the client, and never inject socket data via `innerHTML`.
- **Check event names too.** A catch-all `socket.onAny` that dispatches by name from client input can run handlers you didn't intend, so use explicit allow-listed handlers.

### Socket.IO packet middleware

Run logic before every event on a socket:

```js
io.on("connection", (socket) => {
  socket.use(([event, ...args], next) => {
    if (!ALLOWED_EVENTS.has(event)) return next(new Error("unknown_event"));
    next();
  });

  socket.on("error", (err) => {
    // errors passed to next() in socket.use arrive here
    socket.emit("error:event", { message: err.message });
  });
});
```

---

## 5. Rate limiting events

HTTP rate limiting (`08-authentication-security/06-rate-limiting.md`) doesn't see socket messages, which flow over one long-lived connection. A single client can flood you with thousands of events per second.

### Simple per-socket limiter

```js
function createLimiter({ capacity, refillPerSecond }) {
  let tokens = capacity;
  let last = Date.now();
  return () => {
    const now = Date.now();
    tokens = Math.min(capacity, tokens + ((now - last) / 1000) * refillPerSecond);
    last = now;
    if (tokens < 1) return false;
    tokens -= 1;
    return true;
  };
}

io.on("connection", (socket) => {
  const allow = createLimiter({ capacity: 20, refillPerSecond: 5 });     // burst of 20, sustained 5/s (token bucket)

  socket.use((packet, next) => {
    if (!allow()) return next(new Error("rate_limited"));
    next();
  });
});
```

### Cluster-wide limiting

Per-socket limits don't stop one user opening 50 tabs. For limits by **user or IP across instances**, use Redis (the library `rate-limiter-flexible` has a Redis backend, or reuse the `INCR`/`EXPIRE` pattern from `08-authentication-security/06-rate-limiting.md`):

```js
import { RateLimiterRedis } from "rate-limiter-flexible";

const limiter = new RateLimiterRedis({ storeClient: redisClient, keyPrefix: "rl:ws", points: 30, duration: 10 });

socket.use(async (packet, next) => {
  try {
    await limiter.consume(socket.data.userId);        // keyed by USER, not socket
    next();
  } catch {
    next(new Error("rate_limited"));
  }
});
```

### Limit connections too

```js
io.use(async (socket, next) => {
  const key = `ws:conn:${socket.data.userId}`;
  const count = await redis.incr(key);
  if (count > 10) {                                    // max 10 concurrent connections per user
    await redis.decr(key);
    return next(new Error("too_many_connections"));
  }
  socket.on("disconnect", () => redis.decr(key));       // note: crashes can leave counters stale; give the key a TTL or use a Set
  next();
});
```

Reasonable defaults: cap **concurrent connections per user/IP**, **events per second per user**, **message size**, and **room joins per minute**. Also apply per-IP limits at the proxy (`16-production/04-nginx.md`).

---

## 6. Session and token lifetime on long connections

A connection authenticated with a 15-minute access token may live for 8 hours. Decide what happens when credentials expire or are revoked.

### Disconnect (or re-authenticate) when the token expires

```js
io.on("connection", (socket) => {
  const msUntilExpiry = socket.data.tokenExp * 1000 - Date.now();

  const timer = setTimeout(() => {
    socket.emit("auth:expired");                         // client refreshes its token and reconnects
    socket.disconnect(true);
  }, Math.max(msUntilExpiry, 0));

  socket.on("disconnect", () => clearTimeout(timer));
});
```

```js
// client
socket.on("auth:expired", async () => {
  socket.auth = { token: await refreshAccessToken() };
  socket.connect();                                       // `io server disconnect` does NOT auto-reconnect, so call it yourself
});
```

Alternatively, let the client send a fresh token periodically (`socket.emit("auth:refresh", { token }, ack)`), which the server verifies and swaps into `socket.data`.

### Revocation: logout, password change, ban

Connections must be **actively closed** when access is revoked. Otherwise a banned user's open socket keeps receiving data.

```js
// when a user logs out everywhere, changes password, or is banned (from an HTTP route or service)
io.in(`user:${userId}`).disconnectSockets(true);        // disconnects every connection of that user, across all instances (with an adapter)
```

Pair it with `socket.join(`user:${userId}`)` on connect so every device is addressable by user ID.

### Removing access to a room

```js
// user removed from a project: make all their sockets leave and stop receiving
io.in(`user:${userId}`).socketsLeave(`project:${projectId}`);
```

`socketsLeave`, `socketsJoin`, and `disconnectSockets` work **cluster-wide** when an adapter is configured (`03-scaling-with-redis-adapter.md`).

---

## 7. Rooms

A **room** is a named group of sockets, and is the tool for "send to these people" without tracking connections yourself. Rooms exist **only on the server**; clients can't see or list them. A socket can be in many rooms.

```js
io.on("connection", (socket) => {
  socket.join("project:42");              // add this socket to a room
  socket.leave("project:42");

  io.to("project:42").emit("comment:added", comment);       // everyone in the room (including sender if it's a member)
  socket.to("project:42").emit("typing", { userId });        // everyone in the room EXCEPT this socket
});
```

- Rooms are created implicitly on the first `join` and removed when empty.
- Every socket automatically joins a private room named by its own `socket.id`.
- `socket.rooms` is a `Set` of the rooms it's in.
- On **disconnect**, the socket leaves all rooms automatically.
- On **reconnect**, the new socket is in **no** rooms until you join them again (`01-websocket-and-socketio.md`).

### Room naming conventions

Use a consistent `type:id` scheme so rooms can't collide and are self-documenting:

| Room | Contains | Used for |
|---|---|---|
| `user:{userId}` | All devices/tabs of one user | Personal notifications, "your report is ready", forced logout |
| `project:{projectId}` | Members currently viewing a project | Comments, live edits |
| `chat:{roomId}` | Participants of a conversation | Messages, typing indicators |
| `org:{orgId}` | Everyone in an organization | Announcements |
| `admin` | Admin dashboards | Operational feeds |
| `post:{postId}:viewers` | People viewing one page | Presence counts |

### The most important rooms: per-user rooms

Users have several tabs and devices, and a user's identity is not one socket. A `user:{id}` room gives you "send to this person" in one line:

```js
io.on("connection", (socket) => {
  socket.join(`user:${socket.data.userId}`);
});

// anywhere: a worker finishes, a payment arrives, a friend request comes in
io.to(`user:${userId}`).emit("notification", { type: "report.ready", reportId });
```

### Joining rooms safely: authorize the join

A `join` is a **read permission**. Never let the client name an arbitrary room.

```js
// ❌ any user can eavesdrop on any room
socket.on("room:join", (roomId) => socket.join(roomId));

// ✅ verify membership, and construct the room name server-side
socket.on("room:join", guard({
  schema: z.object({ roomId: z.string().uuid() }),
  authorize: ({ roomId }) => roomRepository.isMember(roomId, socket.data.userId),
})(async ({ roomId }) => {
  socket.join(`chat:${roomId}`);
  return { joined: roomId };
}));
```

Even better for stable memberships: join rooms **automatically on connect**, derived from the database, so the client never asks:

```js
io.on("connection", async (socket) => {
  const userId = socket.data.userId;
  socket.join(`user:${userId}`);

  const projectIds = await projectRepository.idsForUser(userId);
  socket.join(projectIds.map((id) => `project:${id}`));          // join() accepts an array
});
```

(Re-check when memberships change while connected: `socketsJoin` / `socketsLeave` from your HTTP/service layer.)

### Broadcasting patterns

```js
io.to("project:42").emit("evt", data);                   // a room
io.to("user:7").to("user:8").emit("evt", data);          // union of rooms
io.in("project:42").except("user:7").emit("evt", data);  // room minus a user (e.g. skip the author)
socket.broadcast.emit("evt", data);                      // all but this socket
io.of("/admin").to("ops").emit("alert", data);           // room within a namespace
```

### Inspecting rooms

```js
const sockets = await io.in("project:42").fetchSockets();     // [{ id, data, rooms, ... }] (cluster-wide with an adapter)
const online = new Set(sockets.map((s) => s.data.userId));
```

`fetchSockets()` is fine for occasional use, but calling it on every message in a large room is expensive. Maintain counters or presence sets instead (next section).

---

## 8. Presence: who's online

"Online" is a **derived fact**: a user is online if they have at least one live connection. Don't store a boolean per user ("set offline on disconnect"), because it breaks with multiple tabs and crashes.

### Counting connections per user

```js
io.on("connection", async (socket) => {
  const userId = socket.data.userId;
  const key = `presence:user:${userId}`;

  // Redis SET of socket ids for this user: survives multiple tabs and works across instances
  await redis.sAdd(key, socket.id);
  await redis.expire(key, 120);
  if ((await redis.sCard(key)) === 1) {
    io.to("presence:watchers").emit("presence:online", { userId });       // first connection → came online
  }

  socket.on("disconnect", async () => {
    await redis.sRem(key, socket.id);
    if ((await redis.sCard(key)) === 0) {
      io.to("presence:watchers").emit("presence:offline", { userId, at: Date.now() });
    }
  });
});
```

Production presence must also handle:

- **Instance crashes**, which never fire `disconnect`. Give entries a **TTL** and refresh them with a periodic heartbeat from each live socket, so dead entries expire on their own.
- **Flapping:** a mobile client dropping for 3 seconds shouldn't flash "offline → online". Debounce: mark offline only after the user has had no connection for ~10–30 seconds.
- **Scale:** broadcasting every presence change to everyone is expensive. Send presence only to relevant rooms (friends, project members), or let clients **query** a list on demand.

### Per-room presence ("3 people viewing this page")

```js
socket.on("page:view", async ({ pageId }) => {
  socket.join(`page:${pageId}`);
  const viewers = await io.in(`page:${pageId}`).fetchSockets();
  io.to(`page:${pageId}`).emit("page:viewers", { count: new Set(viewers.map((s) => s.data.userId)).size });
});
```

### Typing indicators and other ephemeral signals

Not everything needs persistence or guaranteed delivery. For typing indicators, cursors, and "user is viewing":

```js
socket.on("typing", ({ roomId }) => {
  if (!socket.rooms.has(`chat:${roomId}`)) return;                    // must actually be in the room
  socket.volatile.to(`chat:${roomId}`).emit("typing", { userId: socket.data.userId });
});
```

`volatile` drops the message if the client isn't ready, which is right for data that's stale within a second. Clients should **auto-expire** the indicator after a couple of seconds without a refresh, since you never get a reliable "stopped typing" event.

---

## 9. A complete secured chat handler

```js
// realtime/handlers/chat.handlers.js
import { z } from "zod";

const joinSchema = z.object({ roomId: z.string().uuid() }).strict();
const sendSchema = z.object({
  roomId: z.string().uuid(),
  text: z.string().trim().min(1).max(2000),
  clientMsgId: z.string().uuid(),
}).strict();

export function registerChatHandlers(io, socket, { chatService, roomRepository, logger }) {
  const userId = socket.data.userId;                       // set by auth middleware, never from payloads

  const handle = (schema, fn) => async (payload, callback) => {
    const ack = typeof callback === "function" ? callback : () => {};
    const parsed = schema.safeParse(payload);
    if (!parsed.success) return ack({ ok: false, error: "invalid_payload" });
    try {
      ack({ ok: true, data: await fn(parsed.data) });
    } catch (err) {
      if (err.code === "forbidden") return ack({ ok: false, error: "forbidden" });
      logger.error({ err, userId }, "chat handler failed");
      ack({ ok: false, error: "internal_error" });
    }
  };

  socket.on("chat:join", handle(joinSchema, async ({ roomId }) => {
    if (!(await roomRepository.isMember(roomId, userId))) throw Object.assign(new Error(), { code: "forbidden" });
    await socket.join(`chat:${roomId}`);
    return { roomId, history: await chatService.recent(roomId, 50) };        // initial state over the same channel
  }));

  socket.on("chat:send", handle(sendSchema, async ({ roomId, text, clientMsgId }) => {
    if (!socket.rooms.has(`chat:${roomId}`) || !(await roomRepository.isMember(roomId, userId))) {
      throw Object.assign(new Error(), { code: "forbidden" });                // in the room AND still a member
    }
    const message = await chatService.send({ roomId, userId, text, clientMsgId });   // dedupes on clientMsgId
    io.to(`chat:${roomId}`).emit("chat:message", message);                     // persist FIRST, then broadcast
    return { id: message.id };
  }));
}
```

---

## Testing security

```js
test("rejects connections without a token", async () => {
  const client = Client(url, { reconnection: false });
  const err = await new Promise((r) => client.on("connect_error", r));
  expect(err.message).toBe("unauthenticated");
});

test("rejects a disallowed Origin", async () => {
  const client = Client(url, { extraHeaders: { Origin: "https://evil.example" }, auth: { token: validToken }, reconnection: false });
  await expect(new Promise((res, rej) => { client.on("connect", res); client.on("connect_error", rej); })).rejects.toBeDefined();
});

test("a non-member cannot join or post to a room", async () => {
  const res = await outsider.timeout(2000).emitWithAck("chat:join", { roomId: privateRoomId });
  expect(res).toMatchObject({ ok: false, error: "forbidden" });
});

test("a member's payload cannot impersonate another user", async () => {
  await member.timeout(2000).emitWithAck("chat:send", { roomId, text: "hi", clientMsgId: id, userId: "someone-else" });
  // .strict() schema rejects the extra field → ack { ok: false, error: "invalid_payload" }
});

test("revoked users are disconnected", async () => {
  io.in(`user:${userId}`).disconnectSockets(true);
  await expect(waitForDisconnect(client)).resolves.toBeDefined();
});
```

(`13-testing/02-api-testing-and-mocking.md`.) Note: Node-based test clients can set an `Origin` via `extraHeaders`, but browsers can't spoof it, which is why the check is a defense against hostile *websites*, not against hostile scripts.

---

## Common mistakes

```js
// ❌ authenticating only on the client ("we hide the UI")
// ❌ `cors` option thought to protect the WebSocket transport (it doesn't: use allowRequest / Origin checks)
// ❌ trusting payload.userId / payload.roomId
// ❌ letting clients name arbitrary rooms to join
// ❌ authorizing at connect time only; never revoking on logout / ban / role change
// ❌ JWT in the query string, which ends up in logs
// ❌ no validation → crashes and injection through socket payloads
// ❌ no rate limits → one client floods the server (and every room it can reach)
// ❌ leaking err.message from handlers back to clients
// ❌ presence as a single boolean flipped on connect/disconnect (multi-tab and crash bugs)
// ❌ socket.onAny dispatching to handlers by client-supplied names
// ❌ emitting sensitive data to a broad room "because they're all internal users"
// ❌ forgetting that clients can emit ANY event name with ANY payload, not just the ones your UI uses
```

## Checklist

**Connection**
- [ ] `wss://` only; Origin allow-list enforced with `allowRequest` (and `cors` for polling)
- [ ] Handshake authenticated (session cookie + Origin check, `auth` token, or single-use ticket); no long-lived tokens in URLs
- [ ] Identity stored in `socket.data` by server code; generic auth error messages
- [ ] Limits on concurrent connections per user/IP and on message size

**Events**
- [ ] Every payload validated with a strict schema
- [ ] Every event authorized against the database (membership/ownership/role)
- [ ] Per-user (not just per-socket) rate limits
- [ ] Handlers catch errors, always acknowledge, never leak internals

**Rooms**
- [ ] Room names built server-side (`type:id`), joins authorized (or derived from DB at connect)
- [ ] Per-user rooms (`user:{id}`) for notifications and forced logout
- [ ] Rejoin logic on every reconnect

**Lifetime**
- [ ] Token expiry handled (disconnect + refresh, or in-band refresh)
- [ ] Logout/ban/password change calls `disconnectSockets`; membership changes call `socketsLeave`
- [ ] Presence uses per-user connection sets with TTLs and debouncing

## Next

**`03-scaling-with-redis-adapter.md`** takes everything above beyond a single server: why rooms and broadcasts break with two instances, how sticky sessions and the Redis adapter fix it, how workers emit events, and how to survive deploys and traffic spikes.
