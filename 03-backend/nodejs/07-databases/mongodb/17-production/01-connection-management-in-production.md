# Connection Management in Production

`03-setup/03-connection-events-and-lifecycle.md` covered the connection state machine and graceful shutdown in general. This file covers what changes once that code is actually running in a production deployment — restarts, network instability, and the specific constraints of containerized/orchestrated environments.

## Graceful shutdown, revisited for production

```js
async function shutdown(signal) {
  console.log(`Received ${signal}, shutting down gracefully...`);

  server.close(() => {
    console.log("HTTP server closed");
  });

  await mongoose.connection.close();
  console.log("MongoDB connection closed");

  process.exit(0);
}

process.on("SIGTERM", () => shutdown("SIGTERM"));
process.on("SIGINT", () => shutdown("SIGINT"));
```

As covered in the Node.js/Docker sections of this documentation set: `SIGTERM` is what a container orchestrator (Kubernetes, ECS) or `docker stop` sends on every single deploy and scale-down event — closing the HTTP server (stop accepting new requests, finish in-flight ones) _before_ closing the MongoDB connection matters, since an in-flight request that's still running needs the database connection to still be open to finish correctly.

### A timeout for shutdown that's taking too long

```js
async function shutdown(signal) {
  console.log(`Received ${signal}, shutting down...`);

  const forceExitTimer = setTimeout(() => {
    console.error("Forced shutdown after timeout");
    process.exit(1);
  }, 10000);

  server.close();
  await mongoose.connection.close();
  clearTimeout(forceExitTimer);
  process.exit(0);
}
```

A safety net: if shutdown hangs for some reason (a stuck in-flight request, a connection that won't close cleanly), force-exit after a bounded time rather than hanging indefinitely and eventually being force-killed by the orchestrator anyway (`SIGKILL`, which can't be caught at all).

---

## Handling connection loss without crashing

```js
mongoose.connection.on("disconnected", () => {
  console.warn("MongoDB disconnected");
});

mongoose.connection.on("reconnected", () => {
  console.log("MongoDB reconnected");
});

mongoose.connection.on("error", (err) => {
  console.error("MongoDB connection error:", err);
  // do NOT process.exit() here for every transient error — let automatic reconnection attempt first
});
```

As established in `03-setup/03-connection-events-and-lifecycle.md`, Mongoose/the driver automatically attempts to reconnect after an unexpected disconnect. In production specifically, resist the urge to crash the process on every connection blip — a brief network interruption between your app and MongoDB (entirely normal, especially across availability zones or during a database failover) shouldn't take down a healthy application; let the automatic reconnection do its job, and only fail hard on the **initial** connection attempt at startup, as covered previously.

---

## Retryable writes

```js
await mongoose.connect(uri, {
  retryWrites: true, // the default in modern MongoDB drivers/Atlas connection strings
});
```

`retryWrites` (on by default for Atlas connection strings, and generally recommended) automatically retries a write operation once if it fails due to a transient network issue or a replica set failover — this is a driver-level retry for a **single** operation, distinct from the application-level `TransientTransactionError` retry logic covered for multi-operation transactions in `13-transactions/02-transaction-patterns.md`.

---

## Handling a replica set failover

MongoDB replica sets (required for transactions, `13-transactions/00-README.md`) occasionally undergo an election — the primary node changes, often due to planned maintenance on Atlas. During this brief window, write operations may fail temporarily.

```js
mongoose.connection.on("error", (err) => {
  if (err.message.includes("not master")) {
    console.warn("Primary changed — this should resolve automatically");
  }
});
```

`retryWrites: true` handles most of this transparently for single-document writes; multi-operation transactions should use `withTransaction()`'s built-in retry (`13-transactions/`), which is specifically designed to handle exactly this kind of transient failure.

---

## Multiple app instances, one MongoDB deployment

As covered in `14-performance/04-connection-pooling.md`: each running instance of your application maintains its own connection pool. In production, this means the total connections your MongoDB deployment sees is `pool size × number of instances` — worth confirming this stays comfortably under your MongoDB deployment's connection limit as you scale instance count up or down (e.g. via autoscaling).

---

## Environment-specific connection configuration

```js
const isProduction = process.env.NODE_ENV === "production";

await mongoose.connect(process.env.MONGODB_URI, {
  maxPoolSize: isProduction ? 20 : 5,
  serverSelectionTimeoutMS: isProduction ? 10000 : 5000,
  autoIndex: !isProduction, // per 04-schemas/07-indexes-in-schemas.md
});
```

A production connection often warrants different settings than a local development one — a larger pool (more real concurrent traffic), `autoIndex` disabled (deliberate index management instead), and possibly a longer server-selection timeout (production network paths, especially across regions, can have more latency than a local instance).

## Common mistakes

- **Crashing the process on every transient connection error** rather than letting automatic reconnection attempt first — turns a brief, normal network blip into an unnecessary full restart.
- **Closing the database connection before the HTTP server during shutdown** — can cut off in-flight requests that still need the database.
- **No shutdown timeout/force-exit fallback** — a hung shutdown can leave a container/process lingering until forcefully killed by the orchestrator anyway.
- **Not accounting for pool size × instance count** when scaling instance count up, especially with autoscaling — can unexpectedly approach or exceed MongoDB's connection limit.
- **Leaving `autoIndex: true` in production** — covered previously, worth repeating here as part of the production-specific connection configuration.

## Quick summary

- Close the HTTP server before the MongoDB connection during graceful shutdown, with a force-exit timeout as a safety net
- Don't crash on every transient connection error — automatic reconnection handles most brief network issues; only fail hard on the _initial_ connection attempt
- `retryWrites: true` (the default for Atlas) handles transient single-write failures automatically; multi-operation transactions rely on `withTransaction()`'s own retry logic instead
- Production connection settings (pool size, timeouts, `autoIndex`) often deliberately differ from development defaults

## Next

**`02-monitoring-and-logging.md`** covers what to actually watch once the application is running in production.
