# Connection Pooling

`03-setup/02-connecting-to-mongodb.md` introduced the pooling options briefly. This file covers how the pool actually behaves under load, how to size it, and how to diagnose exhaustion — the connection-layer counterpart to everything else in this performance section.

## Why a pool exists

```js
await mongoose.connect(uri);
```

A single TCP connection to MongoDB can only handle one operation at a time. Rather than opening a brand-new connection for every query (slow — a fresh connection involves a TCP handshake and, for Atlas, a TLS handshake too) or sharing exactly one connection across every concurrent request (a severe bottleneck), Mongoose maintains a **pool** of already-open connections, handing one to each operation and returning it to the pool when done.

```
Request 1 ──┐
Request 2 ──┼── pool of open connections ──→ MongoDB
Request 3 ──┘
```

---

## Sizing the pool: `maxPoolSize`

```js
await mongoose.connect(uri, {
  maxPoolSize: 10, // the default
});
```

`maxPoolSize` caps how many connections Mongoose will open at once. If more operations are in flight than there are available connections, additional operations **queue**, waiting for a connection to free up — they don't fail outright, but they do wait, which shows up as added latency.

### How to think about the right number

There's no universal correct value — it depends on your application's concurrency and MongoDB's own connection limits:

- **Too low** — under real concurrent load, operations queue waiting for a free connection, adding latency even though MongoDB itself isn't the bottleneck
- **Too high** — many idle connections consume memory on both the application and the MongoDB server, and a server (especially a shared/smaller Atlas tier) has its own maximum connection limit that many over-provisioned app instances can collectively exceed

A reasonable starting point is the default (`10`), adjusted upward only if you observe queuing under real load (see monitoring below) — not increased preemptively "just in case."

### `minPoolSize` — keeping connections warm

```js
await mongoose.connect(uri, {
  minPoolSize: 2,
});
```

Keeps at least this many connections open even during idle periods, avoiding the latency of establishing a fresh connection when traffic picks up again after a lull.

---

## Multiple app instances multiply the effective pool size

```
Instance 1: maxPoolSize 10  ─┐
Instance 2: maxPoolSize 10  ─┼── up to 40 total connections to MongoDB
Instance 3: maxPoolSize 10  ─┤
Instance 4: maxPoolSize 10  ─┘
```

If your app runs as multiple processes/containers (via `cluster`, PM2, or multiple container replicas — concepts covered in the earlier Node.js sections of this documentation set), **each instance has its own separate pool**. Four instances each configured with `maxPoolSize: 10` can open up to 40 connections total to MongoDB — a detail easy to overlook when sizing the pool per-instance without considering how many instances actually run in production. MongoDB Atlas clusters (and self-hosted deployments) have their own maximum connection limits that this aggregate total needs to respect.

---

## Timeouts related to pooling

```js
await mongoose.connect(uri, {
  maxPoolSize: 10,
  waitQueueTimeoutMS: 5000, // how long an operation waits for a free connection before giving up
  serverSelectionTimeoutMS: 5000, // how long to wait finding a usable server at all (03-setup/02-)
  socketTimeoutMS: 45000, // how long an individual operation can run before timing out
});
```

`waitQueueTimeoutMS` specifically governs pool exhaustion — without it, an operation waiting for a free connection could wait indefinitely if the pool is genuinely saturated; setting it means a slow/stuck situation fails clearly and relatively promptly, rather than hanging.

---

## Diagnosing pool exhaustion

Symptoms: request latency climbs under load even though individual database operations are fast, or you see explicit `MongoTimeoutError`/`waitQueueTimeoutMS`-related errors.

```js
mongoose.connection.on("connectionPoolCreated", () =>
  console.log("pool created"),
);
```

More directly, monitor at the MongoDB level:

- **Atlas** — the built-in metrics dashboard shows current connection count against your cluster's tier limit
- **`db.serverStatus().connections`** in `mongosh` — shows current/available connections on a self-hosted instance

If connection count is consistently near the pool's `maxPoolSize` (or near MongoDB's own connection limit) during normal traffic, that's the signal to either increase the pool (if MongoDB itself has headroom) or investigate _why_ so many connections are needed — often the real issue is queries holding onto a connection longer than necessary (a slow, unindexed query — `01-indexes-in-mongoose.md` — ties up a connection for longer, contributing to exhaustion under load) rather than the pool size itself being wrong.

---

## A common anti-pattern: reconnecting per request

```js
// ❌ never do this — creates and discards connections constantly, defeating pooling entirely
app.get("/users", async (req, res) => {
  await mongoose.connect(uri);
  const users = await User.find();
  res.json(users);
});
```

As established back in `03-setup/02-connecting-to-mongodb.md`, connect exactly once at startup. This bears repeating specifically in the performance context: reconnecting per request doesn't just waste time establishing new connections, it defeats the entire purpose of having a pool in the first place.

---

## Serverless environments: a special case

In a serverless/functions environment (where each invocation may be a fresh process), the "connect once at startup" advice needs adapting — a common pattern is caching the connection across invocations of the same warm function instance, since a genuinely fresh connection per invocation reintroduces the exact anti-pattern above:

```js
let conn = null;

async function getConnection() {
  if (conn) return conn;
  conn = await mongoose.connect(uri, { maxPoolSize: 5 }); // a SMALLER pool per instance, since serverless can scale to many instances
  return conn;
}
```

A smaller `maxPoolSize` per instance is often appropriate here specifically because serverless platforms can spin up many concurrent instances, each with its own pool — the "multiple app instances multiply the effective pool size" consideration above applies with particular force in serverless, where instance count can spike unpredictably.

## Common mistakes

- **Connecting inside a request handler** instead of once at startup — defeats pooling entirely, on top of the latency cost already covered in the setup section.
- **Sizing `maxPoolSize` per instance without accounting for how many instances run in production** — the real total against MongoDB's connection limit is `maxPoolSize × number of instances`.
- **Increasing `maxPoolSize` reflexively when latency is high**, without checking whether the real problem is slow, unindexed queries holding connections longer than necessary.
- **No `waitQueueTimeoutMS`** — a saturated pool can leave operations waiting indefinitely rather than failing clearly.
- **Using a large `maxPoolSize` in a serverless environment** without considering that many concurrent function instances multiply that number further.

## Quick summary

- A connection pool reuses a set of already-open connections rather than opening one per operation — `maxPoolSize` caps how many, `minPoolSize` keeps a baseline warm
- The _effective_ total connections to MongoDB is `maxPoolSize × number of running app instances` — easy to under-account for
- `waitQueueTimeoutMS` prevents an operation from waiting indefinitely when the pool is saturated
- Diagnose exhaustion via Atlas's metrics or `db.serverStatus().connections`, and check whether slow queries (not pool size) are the real cause before just increasing the number
- In serverless environments, cache the connection across warm invocations and size the pool smaller per instance, since instance count itself can scale unpredictably

## Section complete

That covers performance in full — indexes, query optimization with `.lean()`, scalable pagination, and connection pooling. **`15-patterns-and-architecture`** covers higher-level structural patterns: the repository/service pattern, soft delete, and multi-tenancy.
