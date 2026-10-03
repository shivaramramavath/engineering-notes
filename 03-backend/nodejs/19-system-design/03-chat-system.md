# Chat System

A real-time messaging system — one-to-one and group chats, delivered instantly when the recipient is online, and stored for when they aren't. The hard parts are **long-lived connections**, **fan-out across multiple servers**, **message ordering**, and **reliable delivery**. It builds directly on `12-realtime/` (WebSockets, auth and rooms, Redis adapter).

## Requirements

**Functional**
- One-to-one and group conversations
- Messages delivered in real time to online users
- Message history stored and retrievable (paginated)
- Online/offline presence, typing indicators, read receipts (optional extras)
- Offline users receive messages when they return (and optionally a push notification)

**Non-functional**
- **Low latency** delivery (hundreds of milliseconds)
- **No lost messages** — if the server accepted it, it must eventually arrive
- **Ordering** within a conversation
- Support many concurrent connections; scale horizontally

**Out of scope:** end-to-end encryption, media transcoding, search (mention as extensions).

## Scale estimate

Assumptions: 5 million daily active users, 1 million concurrent connections at peak, each user sends ~40 messages/day.

| Quantity | Calculation | Result |
|----------|-------------|--------|
| Messages/day | 5M × 40 | 200 million |
| Messages/sec (avg) | 200M ÷ 86,400 | ~2,300/sec |
| Messages/sec (peak, ~5×) | | ~12,000/sec |
| Storage/day | 200M × ~200 bytes | ~40 GB/day (~14 TB/year) |
| Connections | 1M concurrent | Memory- and file-descriptor-bound, not CPU-bound |

Takeaways: message volume is moderate, but **holding a million open connections** is the real constraint, and storage grows continuously (so choose a store that handles heavy writes and time-ordered reads).

## Protocol choice

| Option | Notes |
|--------|-------|
| **WebSocket** | Persistent, bidirectional, low overhead — the standard choice |
| Long polling | Fallback for restrictive networks; much heavier |
| Server-Sent Events | One-way (server → client); fine for notifications, not for sending chat |

Socket.IO wraps WebSocket with reconnection, rooms, acknowledgements, and fallbacks (`12-realtime/01-websocket-and-socketio.md`). Sending messages can also go via ordinary `POST` while WebSocket handles delivery — a common hybrid.

## High-level design

```
 Client A ◄──ws──► ┌──────────────┐          ┌──────────────┐ ◄──ws──► Client B
 Client C ◄──ws──► │ Chat server 1│          │ Chat server 2│ ◄──ws──► Client D
                   └──────┬───────┘          └──────┬───────┘
                          │    ┌────────────────┐   │
                          ├───►│ Redis pub/sub  │◄──┤   (cross-server fan-out)
                          │    └────────────────┘   │
                          │    ┌────────────────┐   │
                          ├───►│ Redis presence │◄──┤   (who is online, where)
                          │    └────────────────┘   │
                          ▼                         ▼
                   ┌──────────────────────────────────┐
                   │  Message store (DB)              │
                   └──────────────────────────────────┘
                          │
                          ▼  (offline users)
                   ┌──────────────┐
                   │ Notification │ → push / email  (04-notification-system.md)
                   │   queue      │
                   └──────────────┘
```

## The core problem: users connected to different servers

Alice is connected to server 1; Bob is connected to server 2. When Alice sends a message, server 1 holds the message but can't write to Bob's socket — it doesn't have it.

**Solution:** servers publish messages to a shared channel layer, and every server forwards to its *own* connected clients. With Socket.IO that's the Redis adapter (`12-realtime/03-scaling-with-redis-adapter.md`):

```js
import { Server } from "socket.io";
import { createAdapter } from "@socket.io/redis-adapter";
import { createClient } from "redis";

const pub = createClient({ url: process.env.REDIS_URL });
const sub = pub.duplicate();
await Promise.all([pub.connect(), sub.connect()]);

const io = new Server(httpServer, { adapter: createAdapter(pub, sub) });
```

Now `io.to(room).emit(...)` on any server reaches members connected to any other server.

## Connections: auth and rooms

Authenticate during the **handshake**, not afterward (`12-realtime/02-auth-and-rooms.md`):

```js
io.use(async (socket, next) => {
  try {
    socket.data.user = await verifyToken(socket.handshake.auth.token);
    next();
  } catch {
    next(new Error("unauthorized"));
  }
});

io.on("connection", async (socket) => {
  const userId = socket.data.user.id;

  socket.join(`user:${userId}`);                       // personal room: reach all of a user's devices
  const convIds = await conversations.idsForUser(userId);
  convIds.forEach((id) => socket.join(`conv:${id}`));  // one room per conversation

  socket.on("message:send", (payload, ack) => handleSend(socket, payload, ack));
});
```

- A **room per conversation** makes group fan-out a single `emit`
- A **personal room per user** handles multiple devices (phone + laptop) and lets you push to a user without knowing which server they're on

## Sending a message — persist first, then deliver

```js
async function handleSend(socket, { conversationId, clientMsgId, text }, ack) {
  const senderId = socket.data.user.id;

  // 1. Authorize: is the sender actually in this conversation?
  if (!(await conversations.isMember(conversationId, senderId))) {
    return ack({ ok: false, error: "forbidden" });
  }

  // 2. Persist (idempotent on clientMsgId — safe to retry)
  const message = await messages.insertOnce({
    conversationId,
    senderId,
    clientMsgId,                                       // client-generated unique ID
    text: String(text).slice(0, 4000),                 // validate and bound input
  });

  // 3. Acknowledge to the sender: "the server has it"
  ack({ ok: true, id: message.id, seq: message.seq });

  // 4. Fan out to everyone currently online in the conversation
  io.to(`conv:${conversationId}`).emit("message:new", message);

  // 5. Queue notifications for members who are offline
  await notifyOfflineMembers(conversationId, message);
}
```

Why this order:

- **Persist before delivering** — if the server crashes after delivery but before saving, the message is "seen" yet gone from history. Saving first means a lost *delivery* can be recovered from storage; a lost *message* can't.
- **`clientMsgId` for idempotency** — networks drop acknowledgements. The client retries the same send; the server recognizes the duplicate (unique index on `(conversationId, clientMsgId)`) and returns the original instead of storing it twice (`09-api-development/06-idempotency.md`).
- **Authorize every send** — being connected doesn't mean being allowed in *this* conversation.
- **Validate and bound** payload size and rate (`01-scalable-api-and-rate-limiter.md`) — a socket is an attack surface too.

## Ordering

Wall-clock timestamps are unreliable across servers (clock skew). Give each conversation a **monotonically increasing sequence number** assigned at write time:

```
messages
  conversation_id   -- partition/shard key
  seq               -- 1, 2, 3 ... per conversation
  id, sender_id, text, client_msg_id, created_at
  PRIMARY KEY (conversation_id, seq)
  UNIQUE (conversation_id, client_msg_id)
```

Clients render by `seq`. A client that sees `seq` 41 then 43 knows it **missed 42** and can fetch the gap. Per-conversation ordering is enough — there is no need for a global order across all chats.

## Delivery guarantees

| Guarantee | Meaning | Chat reality |
|-----------|---------|--------------|
| At-most-once | Might lose messages | Unacceptable |
| **At-least-once** | Never lost, possible duplicates | **The practical choice**, paired with client-side de-duplication by `id`/`seq` |
| Exactly-once | Neither lost nor duplicated | Effectively achieved via at-least-once + idempotent handling |

**Catching up after a disconnect:** the client remembers the last `seq` it saw per conversation. On reconnect it asks: "give me everything after `seq` N." The database — not the live socket — is the source of truth. This also handles users who were offline for days.

```js
socket.on("sync", async ({ conversationId, afterSeq }, ack) => {
  const missed = await messages.after(conversationId, afterSeq, { limit: 200 });
  ack(missed);
});
```

## Presence and typing

**Presence** ("online"): store `presence:user:{id}` in Redis with a short TTL that connected clients refresh via heartbeat. If a client vanishes (laptop lid closed), the key expires and the user flips to offline automatically — no reliance on a clean disconnect event.

```js
await redis.set(`presence:${userId}`, "1", { EX: 30 });   // refreshed every ~15 s while connected
```

**Typing indicators** are **ephemeral** — broadcast via the socket, never stored, and throttled client-side. Losing one is harmless.

Presence at large scale gets expensive (every status change fans out to every friend). Common mitigations: only broadcast presence to users currently viewing the relevant chat, and batch updates.

## Storage

Access pattern: *"give me the latest N messages in conversation X"* and *"messages after seq N"* — time-ordered reads within one conversation, heavy append writes.

| Store | Fit |
|-------|-----|
| PostgreSQL / MongoDB | Perfectly adequate to start; index on `(conversation_id, seq)`; partition/shard by `conversation_id` as it grows (`07-databases/`) |
| Wide-column stores (Cassandra, etc.) | Strong at huge append-heavy, partition-keyed workloads — worth it at very large scale |

Choose by the access pattern and scale, not fashion. Hot, recent messages are what's read most; older history can move to cheaper storage.

## Offline users and notifications

When recipients aren't connected, hand off to the notification system rather than doing it inline (`04-notification-system.md`):

```js
async function notifyOfflineMembers(conversationId, message) {
  const members = await conversations.memberIds(conversationId);
  for (const userId of members) {
    if (userId === message.senderId) continue;
    if (!(await redis.exists(`presence:${userId}`))) {
      await notificationQueue.add("push", { userId, conversationId, messageId: message.id });
    }
  }
}
```

(For very large groups, query presence in bulk rather than one call per member.)

## Scaling considerations

- **Connection count dominates.** Each server holds a bounded number of sockets (memory, file descriptors — tune OS limits). Scale by adding servers; assign users to servers via the load balancer.
- **Sticky sessions for the transport.** Socket.IO's long-polling fallback needs requests from one client to reach the same server during the handshake, so configure sticky routing (or force WebSocket-only) (`16-production/04-nginx.md`).
- **Graceful deploys.** Restarting a server drops its connections; clients reconnect with backoff and `sync` the gap. Add jitter to reconnection delays to avoid a **thundering herd** when a server restarts and thousands reconnect simultaneously (`16-production/02-graceful-shutdown-and-health-checks.md`).
- **Large groups.** A message to a 100,000-member channel is a fan-out problem — consider a different model (pull-based / fan-out on read) for huge channels.
- **Redis pub/sub is fire-and-forget.** It doesn't persist or retry. That's fine *because the database is the source of truth* and clients can resync. For stronger guarantees between services, use streams or a broker (`11-async-processing/04-kafka.md`).
- **Limit abuse.** Rate limit messages per user; cap message and payload size.

## Trade-offs summary

| Decision | Trade-off |
|----------|-----------|
| Persist before deliver | Slightly higher latency vs no lost messages |
| At-least-once + idempotency | Duplicate handling logic vs guaranteed delivery |
| Per-conversation `seq` vs global ordering | Simple and scalable vs no cross-chat ordering (not needed) |
| Redis pub/sub for fan-out | Fast and simple vs no durability (DB covers it) |
| Presence via TTL heartbeat | Self-healing vs slightly delayed offline detection |
| Fan-out on write vs read | Fast reads vs write amplification for huge groups |

## Common mistakes

- **Delivering before persisting** — a crash loses a message everyone thought was sent.
- **Trusting wall-clock time for ordering** — clock skew reorders messages; use per-conversation sequence numbers.
- **No idempotency key** — retries create duplicate messages.
- **Relying on the socket as the source of truth** — reconnects lose messages unless clients can sync from storage.
- **Authenticating only at connect time and never authorizing per action** — any connected user could post to any conversation.
- **Keeping presence in server memory** — wrong across multiple servers and lost on restart; use Redis with TTL.
- **Scaling to multiple servers without a pub/sub adapter** — users on different servers can't see each other's messages.
- **Reconnect storms** — no backoff/jitter after a deploy.
- **Unbounded message size or send rate** — easy to abuse.

## Quick summary

- Use **WebSockets** (via Socket.IO); authenticate at handshake and authorize each action
- **Redis pub/sub adapter** lets servers fan messages out to users connected elsewhere
- **Persist first, then deliver**; use a client-generated ID for idempotent retries
- Order with a **per-conversation sequence number**, and let clients **sync from the database** after reconnecting
- Presence = Redis key with TTL + heartbeat; typing indicators are ephemeral
- Offline users are handled through the **notification queue**
- Plan for connection limits, sticky transport, graceful deploys, and reconnect jitter

## Next

**`04-notification-system.md`** designs the system that delivers emails, SMS, and push notifications reliably at scale.
