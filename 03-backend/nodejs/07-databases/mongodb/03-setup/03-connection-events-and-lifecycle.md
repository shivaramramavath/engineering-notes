# Connection Events & Lifecycle

A Mongoose connection isn't just "connected" or "not connected" — it moves through a defined set of states, and emits events at each transition. This file covers listening for those events and shutting down cleanly.

## The connection states

```js
mongoose.connection.readyState;
```

```
0 = disconnected
1 = connected
2 = connecting
3 = disconnecting
```

```js
import mongoose from "mongoose";

const stateNames = ["disconnected", "connected", "connecting", "disconnecting"];
console.log(stateNames[mongoose.connection.readyState]);
```

Useful for a quick synchronous check of the current state — for example, in a health-check endpoint (`13-production/02-monitoring-and-logging.md`).

---

## Listening for connection events

```js
mongoose.connection.on("connected", () => {
  console.log("Mongoose connected to MongoDB");
});

mongoose.connection.on("error", (err) => {
  console.error("Mongoose connection error:", err);
});

mongoose.connection.on("disconnected", () => {
  console.warn("Mongoose disconnected from MongoDB");
});

mongoose.connection.on("reconnected", () => {
  console.log("Mongoose reconnected to MongoDB");
});

await mongoose.connect(process.env.MONGODB_URI);
```

`mongoose.connection` is itself an `EventEmitter` — registering these listeners before calling `connect()` means you catch every transition, including ones that happen automatically later (like a brief network blip causing a `disconnected` followed by a `reconnected`, without your application code doing anything).

### Why `error` matters specifically

Per the general `EventEmitter` behavior covered earlier in this documentation set: an `"error"` event with no listener registered crashes the process. Always attach an `error` listener on `mongoose.connection` — an unhandled connection error is exactly the kind of thing that otherwise takes down your app with a confusing, generic crash.

---

## A realistic startup sequence

```js
// db.js
import mongoose from "mongoose";

export async function connectDB() {
  mongoose.connection.on("error", (err) => {
    console.error("MongoDB connection error:", err);
  });

  mongoose.connection.on("disconnected", () => {
    console.warn(
      "MongoDB disconnected — Mongoose will attempt to reconnect automatically",
    );
  });

  await mongoose.connect(process.env.MONGODB_URI, {
    serverSelectionTimeoutMS: 5000,
  });

  console.log("MongoDB connected");
}
```

```js
// app.js
import { connectDB } from "./db.js";

try {
  await connectDB();
} catch (err) {
  console.error("Failed to connect to MongoDB, exiting:", err);
  process.exit(1); // fail fast at startup rather than serving requests against a broken DB connection
}

app.listen(3000);
```

Failing loudly and exiting if the **initial** connection attempt fails is usually the right call — an app that starts "successfully" but can't actually reach its database is worse than one that fails to start at all and gets restarted by a process manager or orchestrator.

---

## Automatic reconnection

Mongoose (via the underlying driver) automatically attempts to reconnect after an unexpected disconnect — you generally don't need to write manual reconnection logic yourself. Your job is mainly to:

- Log/monitor `disconnected`/`reconnected` events, so you know when it's happening
- Make sure application code handles a transient failure gracefully (e.g. via retry logic at the request level, or simply returning a clear error to the client) rather than assuming every query always succeeds instantly

---

## Graceful shutdown

```js
async function shutdown() {
  console.log("Shutting down gracefully...");
  await mongoose.connection.close();
  console.log("MongoDB connection closed");
  process.exit(0);
}

process.on("SIGTERM", shutdown);
process.on("SIGINT", shutdown);
```

Ties directly into the `SIGTERM`/`SIGINT` handling covered in the Node core-modules documentation earlier in this set — closing the Mongoose connection cleanly on shutdown means in-flight operations get a chance to finish and the connection is released properly, rather than being abruptly cut off when the process exits.

### In a containerized deployment

This matters specifically in Docker/Kubernetes environments: `docker stop` (or a Kubernetes pod termination) sends `SIGTERM` and waits a grace period before force-killing — an app that doesn't handle `SIGTERM` to close its database connection gets forcefully killed instead of shutting down cleanly, on every single deploy or restart.

---

## Checking the connection in a health check

```js
app.get("/health", (req, res) => {
  const isConnected = mongoose.connection.readyState === 1;
  res.status(isConnected ? 200 : 503).json({
    status: isConnected ? "ok" : "database unavailable",
  });
});
```

A simple, real use of `readyState` — letting a load balancer or orchestrator know whether this instance is actually able to serve requests that depend on the database.

## Common mistakes

- **Not attaching an `error` listener on `mongoose.connection`** — an unhandled connection error crashes the process outright, the same `EventEmitter` behavior covered earlier applies here directly.
- **Not exiting the process if the _initial_ connection fails** — letting the app "start" and serve requests it can't actually fulfill is worse than failing fast at startup.
- **Forgetting to close the connection on `SIGTERM`/`SIGINT`** — especially costly in containerized deployments, where this happens on every deploy.
- **Writing manual reconnection loops** — Mongoose/the driver already retries automatically; manual reconnection logic is usually unnecessary and can conflict with the built-in behavior.

## Quick summary

- `mongoose.connection.readyState` (0-3) reflects the current connection state; `mongoose.connection` emits `connected`/`error`/`disconnected`/`reconnected` events
- Always attach an `error` listener — an unhandled one crashes the process
- Fail fast (log and `process.exit(1)`) if the _initial_ connection attempt fails; let automatic reconnection handle later, transient disconnects
- Close the connection gracefully on `SIGTERM`/`SIGINT`, especially important in containerized deployments
- `readyState` is a simple, direct way to power a `/health` endpoint

## Section complete

That covers Mongoose setup end to end — installing it, connecting, and handling the connection's full lifecycle. **`04-schemas`** covers the core concept everything else in Mongoose builds on: defining the shape of your data.
