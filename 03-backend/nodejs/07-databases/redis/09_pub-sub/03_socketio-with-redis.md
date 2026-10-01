# Socket.IO with Redis

A single Node.js process holds its WebSocket connections in memory. The moment you run **two instances** behind a load balancer, a client connected to instance A can't receive something emitted by instance B. The Redis adapter solves that by relaying broadcasts between instances.

This lesson targets **Socket.IO v4**. Check the adapter documentation for option names in your installed versions.

## The two problems of scaling Socket.IO

| Problem | Symptom | Solution |
|---------|---------|----------|
| **Cross-instance broadcast** | `io.to("room").emit(...)` reaches only clients on the same instance | A Redis **adapter** |
| **Connection affinity** | Handshake errors (`400`, "Session ID unknown") with HTTP long-polling | **Sticky sessions**, or WebSocket-only transport |

You need to solve both.

## How the Redis adapter works

```
Client ─► Instance A ─┐                       ┌─► Instance A ─► its local sockets
                      ├─ PUBLISH ─► Redis ────┤
Client ─► Instance B ─┘                       └─► Instance B ─► its local sockets
```

When any instance broadcasts, the adapter **publishes** the packet to Redis. Every instance is **subscribed**, receives it, and delivers it to the matching sockets it holds locally. It uses the Pub/Sub rules from this module, so it inherits **at-most-once** delivery.

## Setup with ioredis

```bash
npm install socket.io @socket.io/redis-adapter ioredis
```

```ts
// src/realtime/server.ts
import type { Server as HttpServer } from "node:http";
import { Server } from "socket.io";
import { createAdapter } from "@socket.io/redis-adapter";
import { createClient } from "../redis/connection.js";

export function createRealtime(httpServer: HttpServer) {
  const pubClient = createClient("sio-pub");
  const subClient = createClient("sio-sub", "subscriber");

  const io = new Server(httpServer, {
    adapter: createAdapter(pubClient, subClient, { key: "shop:sio" }),   // channel prefix
    cors: { origin: process.env.WEB_ORIGIN, credentials: true },
  });

  return { io, pubClient, subClient };
}
```

Notes:

- Two **dedicated** connections per instance (one publishes, one subscribes), not your commands client
- Always attach `error` listeners (the connection factory does this) or Redis hiccups are silent
- The `key` option namespaces the adapter's channels, which helps when several apps share a Redis
- With `ioredis` Cluster, use the **sharded adapter** (`createShardedAdapter`, needs Redis 7.0+) to avoid broadcasting every packet to every node

## Sticky sessions

Socket.IO connects with **HTTP long-polling first**, then upgrades to WebSocket. The handshake spans several HTTP requests, which must reach the **same instance**.

| Option | How | Trade-off |
|--------|-----|-----------|
| **Sticky load balancing** | Cookie or IP-hash affinity on the LB or ingress (for example nginx ingress `nginx.ingress.kubernetes.io/affinity: "cookie"`) | Works with fallback transports. Needs LB configuration |
| **WebSocket only** | Client: `io(url, { transports: ["websocket"] })` | No stickiness needed, but no long-polling fallback for restrictive networks |
| **Node `cluster` module** | `@socket.io/sticky` | For multiple workers on one machine |

Without one of these, you'll see random connection failures that get worse as you add instances.

## A server with auth, rooms and cross-instance emits

```ts
import { createRealtime } from "./server.js";

const { io } = createRealtime(httpServer);

// authenticate the handshake
io.use(async (socket, next) => {
  try {
    socket.data.user = await verifyToken(socket.handshake.auth.token);   // your JWT or session check
    next();
  } catch {
    next(new Error("unauthorized"));
  }
});

io.on("connection", async (socket) => {
  const { id: userId } = socket.data.user;

  await socket.join(`user:${userId}`);           // personal room: reach a user on ANY instance
  await presence.connect(userId, socket.id);

  socket.on("chat:join", async (roomId: string, ack?: (r: { ok: boolean }) => void) => {
    if (!(await canJoin(userId, roomId))) return ack?.({ ok: false });
    await socket.join(`room:${roomId}`);
    ack?.({ ok: true });
  });

  socket.on("chat:send", async (msg: { roomId: string; text: string }, ack?: (r: unknown) => void) => {
    const saved = await messages.save({ roomId: msg.roomId, userId, text: msg.text });   // durable FIRST
    io.to(`room:${msg.roomId}`).emit("chat:message", saved);                              // then fan out, any instance
    ack?.({ ok: true, id: saved.id });
  });

  socket.on("heartbeat", () => presence.touch(userId, socket.id));
  socket.on("disconnect", () => presence.disconnect(userId, socket.id));
});
```

Patterns worth copying:

- **Personal rooms** (`user:{id}`) let you send to a user without knowing which instance holds their socket
- **Authorize room joins** server-side, never trust a client-supplied room name
- **Persist, then emit.** The database is the source of truth, and Socket.IO is the delivery channel
- Use **acknowledgements** (`ack`) so the sender knows the server handled the message

With the adapter, these work **across all instances**:

```ts
io.to("room:42").emit("event", payload);              // everyone in the room
io.in("room:42").socketsJoin("room:vip");             // move sockets between rooms
io.in(`user:${id}`).disconnectSockets();              // force logout everywhere
const sockets = await io.in("room:42").fetchSockets(); // RemoteSocket objects from all instances
io.serverSideEmit("config:reload", { v: 7 });         // instance-to-instance messages
```

## Emitting from outside the socket servers

API handlers or background workers often need to push an update **without** running a Socket.IO server. Use the emitter, which publishes in the adapter's format directly to Redis:

```bash
npm install @socket.io/redis-emitter
```

```ts
import { Emitter } from "@socket.io/redis-emitter";

const emitter = new Emitter(redis, { key: "shop:sio" });   // same key as the adapter

// from a worker or REST handler
emitter.to(`user:${userId}`).emit("notification", { id, text });
emitter.to("room:42").emit("chat:message", saved);
```

Use the same `key` as the server's adapter. This is the clean way to deliver the results of background jobs ([queues](../14_queues-and-workers/README.md)) to browsers.

## Presence

Tracking who is online across instances needs shared state with **expiry**, because crashes skip `disconnect` handlers.

```ts
// src/realtime/presence.ts
import type { Redis } from "ioredis";
import { keys } from "../redis/keys.js";     // add presenceOnline() and presenceSockets(userId) to the registry

export class Presence {
  constructor(private redis: Redis, private ttlSec = 90) {}

  async connect(userId: string, socketId: string) {
    await this.redis.multi()
      .sadd(keys.presenceSockets(userId), socketId)
      .expire(keys.presenceSockets(userId), this.ttlSec)
      .zadd(keys.presenceOnline(), Date.now(), userId)
      .exec();
  }

  async touch(userId: string, socketId: string) {          // call on each heartbeat
    await this.connect(userId, socketId);
  }

  async disconnect(userId: string, socketId: string) {
    await this.redis.srem(keys.presenceSockets(userId), socketId);
    if ((await this.redis.scard(keys.presenceSockets(userId))) === 0) {
      await this.redis.zrem(keys.presenceOnline(), userId);   // last tab closed
    }
  }

  async isOnline(userId: string) {
    return (await this.redis.zscore(keys.presenceOnline(), userId)) !== null;
  }

  /** Run on a timer: drop users whose heartbeat stopped (crashed instances). */
  async sweep(staleMs = this.ttlSec * 1000) {
    return this.redis.zremrangebyscore(keys.presenceOnline(), 0, Date.now() - staleMs);
  }
}
```

How it stays correct:

| Event | Effect |
|-------|--------|
| Connect or heartbeat | Socket added to the user's set, TTL refreshed, `lastSeen` updated |
| Clean disconnect | Socket removed, and the user leaves the online set when none remain |
| Instance crash | No `disconnect` fires. The heartbeat stops, the set expires, and `sweep()` removes the stale user |
| Multiple tabs or devices | One user, several socket IDs |

The disconnect path is a read-then-write and can race with a reconnect. For strict correctness use a Lua script. Presence is usually tolerant of that, because the next heartbeat repairs it. Sorted sets with timestamp scores are the right structure here (see [Sorted Sets](../04_data-structures/05_sorted-sets.md#6-presence-with-expiry)).

## Reliability: what Pub/Sub means for chat

The classic adapter inherits **at-most-once** delivery, so a client or instance briefly cut off from Redis misses messages. Handle it in the **protocol**, not the transport:

1. **Every message has a monotonically increasing id** (from the database or a Redis `INCR`)
2. The client remembers the **last id** it saw per room
3. On reconnect, the client calls a REST endpoint: "give me messages after id N"
4. The server returns them from the database, and the client merges and de-duplicates by id

That gives you correct history regardless of transport hiccups, and it works with every adapter.

### The Streams adapter and connection state recovery

For fewer gaps, use `@socket.io/redis-streams-adapter`. It stores broadcast packets in a **Redis Stream**, so instances that reconnect can catch up, and it supports Socket.IO's **connection state recovery** (restoring a client's rooms and replaying missed events after a short disconnection):

```ts
import { Server } from "socket.io";
import { createAdapter } from "@socket.io/redis-streams-adapter";

const io = new Server(httpServer, {
  adapter: createAdapter(redisClient),                          // a single client, not a pub/sub pair
  connectionStateRecovery: { maxDisconnectionDuration: 2 * 60 * 1000 },
});
```

| | Pub/Sub adapter | Streams adapter |
|---|-----------------|-----------------|
| Delivery | At-most-once | Replayable from the stream |
| Connection state recovery | **Not supported** | Supported |
| Memory | None | Stream retention in Redis |
| Complexity | Lowest | Slightly higher |

Check the adapter docs for exact options and client compatibility. Even with recovery, keep the "fetch after last id" fallback for longer outages.

## Scaling behavior and limits

- Every broadcast reaches **every instance** (the adapter is a fan-out), so cost grows with `instances × message rate`. Sharded adapters and room-scoped subscriptions reduce that
- Keep payloads **small**: send IDs and let the client fetch details if needed
- Per-instance memory is dominated by open sockets, so size instances by connection count
- Connections to Redis are small and fixed (**2 per instance**), unlike sockets
- Throttle chatty events (typing indicators, cursor positions) client-side and server-side

### Rate limiting socket events

Reuse the atomic limiter from [Lua Scripts](../06_advanced-commands/03_lua-scripts.md):

```ts
io.on("connection", (socket) => {
  socket.use(async ([event], next) => {
    const [allowed] = await redis.rateLimit(keys.rateLimit("sock", socket.data.user.id), 30, 10);   // 30 events / 10 s
    if (allowed === 1) return next();
    next(new Error("rate_limited"));
  });
});
```

## Deployments and shutdown

Rolling restarts disconnect every client on the instance. They all reconnect at once, which can stampede the other instances.

- Keep Socket.IO's **reconnection backoff and randomization** on the client (enabled by default)
- On `SIGTERM`: fail readiness (so the LB stops sending new connections), call `io.close()` (disconnects clients, who then reconnect elsewhere), **then** close the adapter's Redis clients
- Roll out gradually, so only a fraction of clients reconnect at a time

```ts
async function shutdown() {
  shuttingDown = true;                  // readiness now 503
  await sleep(5_000);                   // let the load balancer notice
  await new Promise<void>((r) => io.close(() => r()));
  await Promise.all([pubClient.quit(), subClient.quit()].map((p) => p.catch(() => {})));
}
```

## Testing across instances

Run two real servers on different ports sharing one Redis, and connect a client to each:

```ts
it("delivers a message across instances", async () => {
  const a = await startServer(4001);              // each calls createRealtime() with the same Redis
  const b = await startServer(4002);

  const clientA = ioClient("http://localhost:4001", { transports: ["websocket"], auth: { token } });
  const clientB = ioClient("http://localhost:4002", { transports: ["websocket"], auth: { token: token2 } });

  await Promise.all([once(clientA, "connect"), once(clientB, "connect")]);
  await ack(clientA, "chat:join", "42");
  await ack(clientB, "chat:join", "42");

  const received = once(clientB, "chat:message");
  clientA.emit("chat:send", { roomId: "42", text: "hi" });

  expect((await received)[0].text).toBe("hi");
});
```

This single test catches missing adapters, wrong keys, and room misconfiguration.

## Pitfalls

| Pitfall | Fix |
|---------|-----|
| Adapter added but no sticky sessions | Sticky LB, or `transports: ["websocket"]` |
| Using the commands client for the adapter | Dedicated pub and sub connections |
| Different `key` in adapter and emitter | Same key everywhere |
| Trusting client-supplied room names | Authorize joins on the server |
| Emitting before persisting | Persist, then emit |
| Expecting guaranteed delivery from the Pub/Sub adapter | Message ids plus a "fetch after id" endpoint, or the Streams adapter |
| Presence relying only on `disconnect` events | TTL, heartbeats and a sweeper |
| Broadcasting large payloads to everyone | Send IDs, fetch details |
| All clients reconnecting at once after a deploy | Gradual rollout, client backoff |
| No `error` listeners on adapter clients | Attach them (the factory does) |
| Cluster Redis with the classic adapter at high volume | Sharded adapter (Redis 7+) |

## Key takeaways

- Scaling Socket.IO needs the **Redis adapter** (cross-instance broadcast) **and** sticky sessions (or WebSocket-only)
- Use the **emitter** to push from workers and API handlers
- Pub/Sub means at-most-once, so give messages ids and let clients **catch up from the database**, or use the Streams adapter for recovery
- Track presence with heartbeats, TTLs and a sorted set, since crashes skip `disconnect`
- Persist first, then emit, and shut down in order

**Next module:** [10_streams](../10_streams/README.md)
