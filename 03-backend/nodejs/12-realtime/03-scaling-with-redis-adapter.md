# Scaling with the Redis Adapter

Running realtime across more than one server: why broadcasts break, how sticky sessions and the Redis adapter fix it, how background workers emit events, and how to survive deploys and traffic spikes.

## Why one instance works and two don't

By default, Socket.IO keeps everything **in the memory of one Node.js process**: the list of connected sockets, which rooms they're in, and the logic to broadcast.

```
        Load balancer
         │          │
         ▼          ▼
   ┌──────────┐ ┌──────────┐
   │ Server A │ │ Server B │
   │  Alice   │ │  Bob     │       Alice and Bob are both in room "chat:1"
   │  room    │ │  room    │
   │  chat:1  │ │  chat:1  │
   └──────────┘ └──────────┘

Alice sends a message → Server A runs io.to("chat:1").emit(...)
   → Server A only knows about ITS sockets → Alice receives it. Bob never does. 😬
```

Each server has its own private view of the world. Anything that targets "all sockets in a room" or "all of a user's devices" only reaches the sockets on **the instance that ran the code**. The same goes for `io.emit`, `fetchSockets`, `disconnectSockets`, and `socketsJoin`.

So scaling horizontally (which you need for capacity and availability) requires two things:

1. **Sticky sessions** (or WebSocket-only transport), so a client's requests keep hitting the same server during the handshake
2. **An adapter** that lets servers share broadcasts: the **Redis adapter**

---

## Part 1: Sticky sessions

### The problem

Socket.IO's default connection process starts with **HTTP long-polling** requests and then upgrades to WebSocket. A single logical connection is therefore several HTTP requests, all of which must reach the **same server**, because the session state (an in-memory session ID) lives there.

```
Client ──(1) GET /socket.io/?transport=polling ───▶ Server A   (creates session abc123)
Client ──(2) POST /socket.io/?sid=abc123 ─────────▶ Server B   ❌ "Session ID unknown" (400)
```

A round-robin load balancer would scatter these requests and the connection would fail or flap.

### Fix 1: Make the load balancer sticky

Route the same client to the same server (by cookie or IP hash).

**Nginx** (this is also the right place to configure the WebSocket upgrade headers):

```nginx
upstream socketio_backend {
    ip_hash;                              # simple stickiness by client IP
    server app1:3000;
    server app2:3000;
    server app3:3000;
}

server {
    listen 443 ssl;
    server_name api.example.com;

    location /socket.io/ {
        proxy_pass http://socketio_backend;

        proxy_http_version 1.1;                          # REQUIRED for WebSocket
        proxy_set_header Upgrade $http_upgrade;          # forward the upgrade request
        proxy_set_header Connection "upgrade";

        proxy_set_header Host $host;
        proxy_set_header X-Real-IP $remote_addr;
        proxy_set_header X-Forwarded-For $proxy_add_x_forwarded_for;
        proxy_set_header X-Forwarded-Proto $scheme;

        proxy_read_timeout 120s;                         # MUST exceed pingInterval + pingTimeout, or Nginx kills idle sockets
        proxy_send_timeout 120s;
        proxy_buffering off;
    }
}
```

Notes:

- `ip_hash` is crude: clients behind one corporate NAT or mobile carrier all land on one server (uneven load), and it breaks if clients' IPs change. Cookie-based stickiness is better where available (Nginx Plus `sticky cookie`, HAProxy `cookie`, AWS ALB **stickiness** with an app/LB cookie, Kubernetes ingress-nginx `nginx.ingress.kubernetes.io/affinity: "cookie"`).
- **`proxy_read_timeout`** is the classic gotcha. If it's shorter than the time between heartbeats, Nginx silently closes quiet connections (default 60 s). Keep it above `pingInterval + pingTimeout` (45 s with the settings in `01`).
- Cloud load balancers have **idle timeouts** too (AWS ALB defaults to 60 s). Heartbeats keep connections alive, but check the numbers.
- See `16-production/04-nginx.md` for more on reverse proxy setup.

### Fix 2: WebSocket-only transport (no stickiness needed)

If the client connects **directly via WebSocket** and skips polling, there's just one long-lived connection, so no session state needs to be shared between requests.

```js
// client
const socket = io("https://api.example.com", { transports: ["websocket"] });
```

```js
// server (optionally restrict as well)
const io = new Server(httpServer, { transports: ["websocket"] });
```

Trade-off: you lose the **HTTP long-polling fallback** for the rare networks that block WebSockets (some corporate proxies, old firewalls). In practice WebSocket support is near-universal today, and this is a popular choice for simplicity. Even so, sticky sessions are still helpful for efficient reconnection and are required if you keep polling enabled.

### Fix 3: Node's `cluster` module on one machine

To use all CPU cores on a single host, you can use `cluster` (`02-core-modules/10-cluster-and-worker-threads.md`), with the same stickiness issue, solved by the official helpers:

```bash
npm install @socket.io/sticky @socket.io/cluster-adapter
```

```js
import cluster from "node:cluster";
import { availableParallelism } from "node:os";
import { createServer } from "node:http";
import { Server } from "socket.io";
import { setupMaster, setupWorker } from "@socket.io/sticky";
import { createAdapter, setupPrimary } from "@socket.io/cluster-adapter";

if (cluster.isPrimary) {
  const httpServer = createServer();
  setupMaster(httpServer, { loadBalancingMethod: "least-connection" });
  setupPrimary();                                                       // lets workers broadcast to each other
  httpServer.listen(3000);
  for (let i = 0; i < availableParallelism(); i++) cluster.fork();
} else {
  const httpServer = createServer(app);
  const io = new Server(httpServer);
  io.adapter(createAdapter());
  setupWorker(io);
}
```

This covers **one host**. For multiple hosts, you need an external adapter like Redis.

---

## Part 2: The Redis adapter

The adapter replaces Socket.IO's in-memory "who's in which room and how to broadcast" logic with one that **publishes broadcasts through Redis**, so every server delivers the message to *its own* matching sockets.

```
Alice ──▶ Server A: io.to("chat:1").emit("msg", ...)
                 │
                 │ PUBLISH to Redis channel
                 ▼
            ┌────────┐
            │ Redis  │  (Pub/Sub: 07-databases/redis/03-pub-sub.md)
            └────────┘
             │      │
   SUBSCRIBE │      │ SUBSCRIBE
             ▼      ▼
        Server A   Server B
     delivers to  delivers to
     Alice (local) Bob (local)   ✅
```

### Setup

```bash
npm install @socket.io/redis-adapter redis
```

```js
// realtime/index.js
import { createClient } from "redis";
import { createAdapter } from "@socket.io/redis-adapter";
import { Server } from "socket.io";

export async function createRealtime(httpServer, deps) {
  const pubClient = createClient({ url: process.env.REDIS_URL });
  const subClient = pubClient.duplicate();               // a subscriber connection can't issue normal commands

  pubClient.on("error", (err) => deps.logger.error({ err }, "Redis pub client error"));
  subClient.on("error", (err) => deps.logger.error({ err }, "Redis sub client error"));

  await Promise.all([pubClient.connect(), subClient.connect()]);

  const io = new Server(httpServer, {
    adapter: createAdapter(pubClient, subClient),
    cors: deps.corsOptions,
  });

  // ...middleware and handlers exactly as before: nothing else in your code changes

  return { io, pubClient, subClient };
}
```

That's it. With the adapter installed, all of these now work **across every instance**:

```js
io.to("project:42").emit("comment:added", c);       // reaches room members on any server
io.emit("maintenance", { at });                      // everyone, everywhere
await io.in("user:7").fetchSockets();                // sockets for a user, cluster-wide
io.in("user:7").disconnectSockets(true);             // kick all of a user's connections on every server
io.in("user:7").socketsJoin("project:42");           // add all their sockets to a room, cluster-wide
io.in("user:7").socketsLeave("project:42");
await io.serverSideEmitWithAck?.("ping");            // server-to-server messaging (see below)
```

Your auth, handlers, and room logic from `02-auth-and-rooms.md` need **no changes**: that's the point of the adapter.

### What the adapter does *not* do

| Not handled | What to do |
|---|---|
| **Sticky sessions** | Still required (or WebSocket-only transport), as Part 1 |
| **Persisting messages** | Redis Pub/Sub is fire-and-forget: if a server is briefly disconnected from Redis, it misses broadcasts. Your database remains the source of truth, with a catch-up endpoint |
| **Per-socket state transfer** | A socket's state lives on the server holding its connection. `socket.data` is visible cluster-wide via `fetchSockets()`, but arbitrary in-memory variables are not |
| **Cross-server business logic** | If you keep in-memory maps (`const online = new Map()`), each instance has its own. Put shared state in Redis (presence sets from `02`) |
| **Message ordering guarantees across servers** | Order is best-effort across instances |

### Choosing an adapter

| Adapter | How it works | Notes |
|---|---|---|
| **`@socket.io/redis-adapter`** (classic) | Redis Pub/Sub; one channel per namespace plus per-room channels | Simplest, most common. Pub/Sub is fire-and-forget |
| **Sharded Pub/Sub mode** (`createShardedAdapter`) | Redis 7+ *sharded* Pub/Sub (`SPUBLISH`/`SSUBSCRIBE`) | Scales better with **Redis Cluster**, because messages only go to the shard owning the channel |
| **`@socket.io/redis-streams-adapter`** | Redis **Streams** instead of Pub/Sub | Messages persisted briefly, so it **supports connection state recovery** (`01`) and tolerates brief Redis outages |
| **`@socket.io/postgres-adapter`**, **`mongo-adapter`**, **`cluster-adapter`** | Use PostgreSQL `LISTEN/NOTIFY`, MongoDB change streams, or Node cluster IPC | Useful if you already run that DB and don't want Redis; throughput is lower |

Check each package's docs for current API and compatibility, since adapter options evolve between releases.

```js
// Sharded variant (Redis 7+; good with Redis Cluster)
import { createShardedAdapter } from "@socket.io/redis-adapter";
const io = new Server(httpServer, { adapter: createShardedAdapter(pubClient, subClient, { subscriptionMode: "dynamic" }) });
```

### Use a dedicated Redis for realtime

Pub/Sub traffic can be heavy, and the adapter uses its own connections. Run it on a Redis instance (or logical deployment) **separate from your cache and your BullMQ queues** (`11-async-processing/01-queues-and-bullmq.md`), because queues require `noeviction` while caches want eviction, and a noisy broadcast channel shouldn't slow down job processing. For small systems sharing one Redis is fine; plan the split before you need it.

---

## Part 3: Emitting from outside Socket.IO servers

A BullMQ worker finishes a report. It runs in a **separate process** with no Socket.IO server and no connections. How does it tell the user?

### Option A: The Redis emitter

`@socket.io/redis-emitter` lets any process publish to the same Redis channels the adapter uses, so the Socket.IO servers deliver to their local sockets.

```bash
npm install @socket.io/redis-emitter redis
```

```js
// workers/realtimeEmitter.js: used from a worker, cron job, or any non-socket process
import { createClient } from "redis";
import { Emitter } from "@socket.io/redis-emitter";

const redisClient = createClient({ url: process.env.REDIS_URL });
await redisClient.connect();

export const emitter = new Emitter(redisClient);
```

```js
// workers/reports.worker.js
import { emitter } from "./realtimeEmitter.js";

new Worker("reports", async (job) => {
  const url = await generateReport(job.data);

  // targets `user:<id>` room on whichever Socket.IO server(s) hold that user's sockets
  emitter.to(`user:${job.data.userId}`).emit("report:ready", { jobId: job.id, url });
}, { connection });
```

```js
// namespaces and rooms work as usual
emitter.of("/admin").to("ops").emit("alert", { level: "warn", text: "Queue depth high" });
```

The emitter is **publish-only**: it can emit and manage rooms (`socketsJoin`, `socketsLeave`, `disconnectSockets`), but it can't *receive* events or query who's connected.

### Option B: Publish an event; let the web tier emit

Keep Socket.IO knowledge out of workers entirely. Workers publish a domain event (BullMQ, Kafka, Redis Pub/Sub), and a small subscriber inside the Socket.IO process translates it into emits:

```js
// in the realtime process
const sub = redis.duplicate(); await sub.connect();
await sub.subscribe("domain-events", (raw) => {
  const event = JSON.parse(raw);
  if (event.type === "report.ready") io.to(`user:${event.data.userId}`).emit("report:ready", event.data);
});
```

Better decoupling and a single place for delivery rules, at the cost of one extra hop. It fits event-driven designs (`11-async-processing/04-kafka.md`, `10-architecture/05-modular-monolith-vs-microservices.md`).

### Always: persist, then push

```js
// worker
await notificationRepository.create({ userId, type: "report.ready", payload });   // 1. store (the source of truth)
emitter.to(`user:${userId}`).emit("notification", payload);                        // 2. best-effort push
```

If the user is offline when the push happens, they still find the notification when they next load the app or reconnect and call `GET /notifications?since=...`.

### Server-to-server messages

With an adapter, instances can also message each other (not clients):

```js
io.serverSideEmit("cache:invalidate", { key: "config" });         // all OTHER instances receive it
io.on("cache:invalidate", ({ key }) => localCache.delete(key));
```

Useful for cache invalidation and coordination. For anything important, prefer a real message bus: this is also Pub/Sub, so it's lossy.

---

## Part 4: Deploys and the reconnect storm

A deploy restarts your servers, which **disconnects every client at once**. Thousands of clients then reconnect in the same second to the remaining/new instances: a **thundering herd** that can overload your servers, Redis, and databases (each connection runs auth middleware, DB lookups, room joins, and initial state fetches).

### Graceful shutdown

```js
// server.js
async function shutdown(signal) {
  logger.info({ signal }, "Shutting down realtime server");

  // 1. Stop accepting new HTTP connections (the load balancer should already be draining us)
  httpServer.close();

  // 2. Ask clients to reconnect elsewhere, spread over time to avoid a stampede
  const sockets = await io.fetchSockets();
  for (const s of sockets) {
    setTimeout(() => s.disconnect(true), Math.random() * 20_000);       // jittered over 20s
  }
  await new Promise((r) => setTimeout(r, 22_000));

  // 3. Close Socket.IO, then Redis and the DB
  await io.close();                                                       // also disconnects remaining clients
  await Promise.allSettled([pubClient.quit(), subClient.quit()]);
  process.exit(0);
}

process.on("SIGTERM", () => shutdown("SIGTERM"));
process.on("SIGINT", () => shutdown("SIGINT"));
```

Kubernetes needs `terminationGracePeriodSeconds` longer than the drain (here, >25 s) and a **readiness probe** that fails during shutdown so new connections stop being routed to the dying pod (`16-production/02-graceful-shutdown-and-health-checks.md`).

Because `s.disconnect(true)` triggers `io server disconnect` on the client, which **does not auto-reconnect**, either emit a custom event first (`socket.emit("server:restarting")`, and have the client call `connect()` after a random delay), or close the transport instead so clients treat it as a network drop and auto-reconnect:

```js
// forces a transport-level close: the client sees "transport close" and reconnects with its own backoff + jitter
s.conn.close();
```

### Client-side jitter and caps

```js
const socket = io(url, {
  reconnectionDelay: 1000,
  reconnectionDelayMax: 30_000,
  randomizationFactor: 0.5,
});
```

### Make connect cheap

Every reconnect runs your `connection` handler, so keep it light:

- **Cache** the user lookup (verifying a JWT needs no DB hit; do you really need `findById` on each connect?).
- **Batch** DB queries for room membership instead of one query per room.
- **Defer** heavy initial state to an explicit event/endpoint rather than blasting it inside `connection`.
- Use **connection state recovery** or a `since` cursor so clients fetch only what they missed, not everything (`01-websocket-and-socketio.md`).

### Rolling deploys

Replace instances gradually (rolling update, `maxUnavailable: 1`) so only a fraction of clients reconnect at a time and always find healthy servers. Blue/green deployments move everyone at once, so combine them with the jittered drain above.

---

## Part 5: Capacity and limits

### What limits connections per server

| Resource | Details |
|---|---|
| **File descriptors** | Each connection uses one. Default `ulimit -n` is often 1024: raise it (`LimitNOFILE=1048576` in systemd, `--ulimit nofile=65536:65536` in Docker, `nofile` in Kubernetes via the container runtime/node settings) |
| **Memory** | Each socket costs buffers and per-socket state: typically **tens of KB** idle, more with big rooms/state. 10,000 connections is usually a few hundred MB; measure your own |
| **CPU** | Mostly consumed by *messages*, not idle connections. TLS handshakes spike during reconnect storms |
| **Event loop** | One Node process runs on one core. Broadcasting to a 50,000-member room blocks the loop while serializing |
| **Ephemeral ports (proxy → app)** | A reverse proxy opening many connections to the app can exhaust the ~28k default ephemeral port range per upstream IP/port pair |
| **Load balancer** | Max connections, idle timeouts, rate limits |

A single well-tuned Node.js process commonly handles **tens of thousands** of mostly-idle WebSocket connections; throughput (messages/second) is the real limit. Don't trust rules of thumb: **load test** (`15-performance/04-load-balancing-and-testing.md`) using tools like `artillery` (has a Socket.IO engine), `k6` (WebSocket support), or a custom script with many `socket.io-client`s.

### Broadcast cost

Fan-out is the hidden multiplier:

```
1 message × 10,000 members in the room = 10,000 sends (+ serialization, + Redis fan-out to every instance)
100 messages/second into that room       = 1,000,000 sends/second
```

Reduce it by:

- **Smaller rooms:** split huge channels (sharding, "pages" of viewers).
- **Batching/coalescing:** combine updates into one message every 100–250 ms.
- **Sending deltas**, not state.
- **Throttling** high-frequency data (cursors at 10–20 Hz, not 60+).
- **Using `volatile`** for droppable data.
- **Subscribing only to what's visible:** clients join rooms for the items on screen, and leave when scrolled away.
- Moving **massive broadcast** (live scores to a million viewers) to **SSE/CDN-style fan-out** or a managed realtime service.

### Redis capacity

Each broadcast is one `PUBLISH`; Redis fans it out to every subscribed instance. Watch for:

- **Many instances × many rooms:** with the classic adapter each instance subscribes to channels for the rooms it has members in. That's fine up to a point; the **sharded** adapter reduces overhead on clusters.
- **Large payloads:** copied to every instance whether or not it has local recipients.
- **Redis as a single point of failure:** use a managed Redis with replicas/failover (ElastiCache, Upstash, Redis Cloud, Sentinel/Cluster). If Redis is unavailable, **cross-instance broadcasts stop**, but local connections continue to work, so degrade gracefully and alert.

### Buy vs build

Operating realtime at large scale is a real job (sticky sessions, adapters, reconnect storms, capacity planning). Managed services (**Ably, Pusher, PubNub, AWS AppSync/API Gateway WebSockets, Supabase Realtime, Firebase**) offload the connection layer: you publish via REST/SDK and they hold the connections. Reasonable when realtime is a feature rather than your core product, when you're a small team, or when you need global scale and presence out of the box.

---

## Part 6: Observability

Metrics worth tracking (`14-logging-observability/03-metrics-and-prometheus.md`):

| Metric | How | Alert when |
|---|---|---|
| **Connected clients** (per instance) | `io.engine.clientsCount` | Sudden drops (a deploy or crash), or approaching limits |
| **Connections/sec** (connects & disconnects) | counters in `connection` / `disconnect` | Spikes = reconnect storm |
| **Disconnect reasons** | label by `reason` | Rising `ping timeout` / `transport error` |
| **Events in/out per second** | `socket.onAny` / wrapper around `emit` | Unexpected floods |
| **Handler latency & error rate** | histogram around handlers | p95 regressions, error spikes |
| **Rate-limit rejections** | counter | Abuse |
| **Redis pub/sub latency & errors** | adapter client `error` events, `redis` metrics | Any sustained errors |
| **Event loop lag** | `perf_hooks.monitorEventLoopDelay()` | Lag > ~100 ms, which delays every client |
| **Memory & file descriptors** | process metrics | Steady growth (leak) / near the limit |

```js
import client from "prom-client";

const connected = new client.Gauge({ name: "ws_connected_clients", help: "Connected sockets" });
const disconnects = new client.Counter({ name: "ws_disconnects_total", help: "Disconnects", labelNames: ["reason"] });

io.on("connection", (socket) => {
  connected.inc();
  socket.on("disconnect", (reason) => { connected.dec(); disconnects.inc({ reason }); });
});
```

Logging: include the `socket.id`, `userId`, and a connection-scoped correlation ID in every log line (`14-logging-observability/02-correlation-id.md`). A socket's `id` changes on every reconnect, so log the user ID as well. Don't log message payloads that may hold private content.

### Admin UI (development and ops)

`@socket.io/admin-ui` provides a dashboard showing connected sockets, rooms, and live events. **Protect it heavily** (auth, restricted origins) or disable it in production, since it can inspect and manipulate live connections.

---

## Local multi-instance setup with Docker Compose

Test the real topology locally: two app instances, Redis, and Nginx as a sticky load balancer.

```yaml
# docker-compose.yml
services:
  redis:
    image: redis:7
  app1:
    build: .
    environment: { REDIS_URL: redis://redis:6379, PORT: 3000 }
    depends_on: [redis]
  app2:
    build: .
    environment: { REDIS_URL: redis://redis:6379, PORT: 3000 }
    depends_on: [redis]
  nginx:
    image: nginx:1.27
    ports: ["8080:80"]
    volumes: ["./nginx.conf:/etc/nginx/nginx.conf:ro"]
    depends_on: [app1, app2]
```

Open two browser tabs (or two clients) pointed at `localhost:8080`, deliberately landing on different instances (log `process.env.HOSTNAME` on connect), and verify a message from one reaches the other. Then **kill one instance** and watch clients reconnect. See `16-production/03-docker-and-compose.md`.

### An automated cross-instance test

```js
test("a broadcast on instance A reaches a client connected to instance B", async () => {
  const [ioA, ioB] = await createTwoInstancesWithRedis();          // each: own http server + same Redis adapter
  const clientB = Client(`http://localhost:${ioB.port}`);
  await new Promise((r) => clientB.on("connect", r));
  clientB.emit("join", "room1");                                   // (handler joins the room on instance B)

  const received = new Promise((r) => clientB.on("hello", r));
  ioA.to("room1").emit("hello", { from: "A" });                    // emitted on a DIFFERENT instance

  expect(await received).toEqual({ from: "A" });
});
```

Run this in CI against a Redis service container (`16-production/05-ci-cd.md`) so adapter misconfiguration is caught before production.

---

## Common mistakes

```js
// ❌ scaling to 2+ instances without an adapter → broadcasts reach only local sockets (silent partial delivery)
// ❌ no sticky sessions AND polling enabled → "Session ID unknown", flapping connections
// ❌ Nginx without `proxy_http_version 1.1` + Upgrade headers → WebSockets never upgrade (stuck on polling)
// ❌ proxy_read_timeout (or LB idle timeout) shorter than the heartbeat interval → mysterious disconnects every 60 s
// ❌ in-memory state (Map of online users, counters) shared "across the app" → wrong as soon as there are 2 instances
// ❌ sharing one Redis with eviction enabled between cache and queues/adapter
// ❌ emitting from workers by importing the io instance (they have none): use the emitter or publish an event
// ❌ treating Pub/Sub as durable → relying on it for important notifications without persisting
// ❌ deploying all instances at once → reconnect stampede
// ❌ not raising file-descriptor limits → EMFILE under load
// ❌ broadcasting full state to huge rooms on every change
// ❌ forgetting to attach `error` handlers to the Redis pub/sub clients → process crashes when Redis blips
// ❌ trusting `x-forwarded-for` / socket.handshake.address behind a proxy without configuring trust (rate limits keyed to the proxy's IP)
```

## Checklist

**Topology**
- [ ] Sticky sessions (cookie-based where possible) **or** `transports: ["websocket"]`
- [ ] Redis adapter (classic, sharded, or streams) configured with separate pub/sub clients and `error` handlers
- [ ] Proxy: `proxy_http_version 1.1`, `Upgrade`/`Connection` headers, `proxy_read_timeout` > ping interval + timeout
- [ ] Load balancer idle timeout above heartbeat interval; `X-Forwarded-*` handled for real client IPs
- [ ] Realtime Redis separate from cache/queue Redis (or deliberately shared)

**Application**
- [ ] No cross-request state in process memory; presence and counters in Redis
- [ ] Workers/services emit via `@socket.io/redis-emitter` or domain events, not via the `io` instance
- [ ] Persist first, push second; a catch-up endpoint exists
- [ ] Revocation and membership changes use cluster-wide `disconnectSockets`/`socketsLeave`

**Operations**
- [ ] Graceful shutdown with a jittered client drain; readiness probe fails during shutdown
- [ ] Client reconnection uses backoff + jitter
- [ ] `ulimit -n` raised; memory per connection measured; load tested with realistic messaging
- [ ] Metrics: connected clients, connect/disconnect rate and reasons, handler latency, event-loop lag, Redis health
- [ ] Redis is highly available; degraded-mode behavior when it's down is understood and alerted on
- [ ] Multi-instance behavior covered by an automated test

## Wrap-up

This completes `12-realtime/`. The through-line: **pick the simplest transport that fits** (`00`), **treat the socket as an untrusted, long-lived pipe with its own protocol conveniences** (`01`), **authenticate once but authorize every event and revoke actively** (`02`), and **assume more than one server from the start, with shared state in Redis and the database as the source of truth** (`03`).

## Next

Section **`13-testing/`**: unit and integration testing, API testing and mocking, and test databases with coverage. You've seen testing snippets throughout the course, and this section pulls them into a coherent strategy.
