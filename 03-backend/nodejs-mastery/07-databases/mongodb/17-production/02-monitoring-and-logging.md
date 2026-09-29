# Monitoring & Logging

What to actually watch once a Mongoose-backed application is running in production — the specific signals that catch problems early, and how to log database operations usefully without drowning in noise.

## What to actually monitor

### 1. Slow queries

```js
mongoose.set("debug", (collectionName, method, ...args) => {
  const start = Date.now();
  // ...
});
```

More practically, use MongoDB's own slow-query logging (via Atlas's Performance Advisor, or `db.setProfilingLevel()` on a self-hosted instance) rather than trying to time every query in application code. Atlas surfaces slow queries directly in its monitoring dashboard, often with an automatic suggestion for a missing index — directly connecting back to `14-performance/01-indexes-in-mongoose.md`'s `explain()` workflow, but observed from real production traffic rather than a hypothesis.

### 2. Connection pool health

```js
mongoose.connection.on("connectionPoolCreated", (event) => {
  /* ... */
});
```

As covered in `14-performance/04-connection-pooling.md`, watch current connection count against `maxPoolSize` and against MongoDB's own connection limit — Atlas's dashboard shows this directly; a self-hosted deployment can check via `db.serverStatus().connections` in `mongosh`.

### 3. Error rates by type

```js
app.use((err, req, res, next) => {
  if (err instanceof AppError) {
    logger.warn(
      { errorType: err.constructor.name, message: err.message },
      "Handled application error",
    );
    return res.status(err.statusCode).json({ error: err.message });
  }
  logger.error({ err }, "Unhandled error");
  res.status(500).json({ error: "Internal server error" });
});
```

Building on `08-errors/06-custom-application-errors.md`'s error classes: log **expected** errors (`ConflictError`, `NotFoundError`) at a lower severity (`warn`, or not at all beyond a metric increment) than genuinely **unexpected** ones (`error`, with full stack trace and alerting). A spike in `ConflictError` (duplicate emails) is a normal part of running a signup flow, not an incident; a spike in unrecognized `500`s is.

### 4. Replica set / connection state changes

```js
mongoose.connection.on("disconnected", () => {
  logger.warn("MongoDB disconnected");
});
mongoose.connection.on("reconnected", () => {
  logger.info("MongoDB reconnected");
});
```

Logging these (per `01-connection-management-in-production.md`) means a pattern of frequent disconnects — even if each one auto-recovers — is visible in your logs/metrics rather than silently happening over and over, since frequent reconnection cycles can indicate a real underlying network or infrastructure issue worth investigating even though no individual request necessarily fails.

---

## Structured logging around database operations

Tying back to the earlier Node.js documentation's Pino coverage — logging database-adjacent events with structured fields, not string concatenation:

```js
logger.info(
  { operation: "createUser", userId: user._id.toString(), durationMs: 45 },
  "User created",
);
```

```js
logger.error(
  { operation: "createOrder", err, orderId: attemptedOrderId },
  "Order creation failed",
);
```

Structured fields (`operation`, `durationMs`, relevant IDs) make it possible to search/filter/aggregate logs meaningfully in a log platform — "show me every failed `createOrder` operation in the last hour" is a real, answerable query against structured logs, and effectively impossible against a pile of unstructured string messages.

---

## Correlating a request across logs

```js
app.use((req, res, next) => {
  req.requestId = crypto.randomUUID();
  next();
});

app.post("/orders", async (req, res, next) => {
  try {
    const order = await createOrder(req.body);
    logger.info(
      { requestId: req.requestId, orderId: order._id },
      "Order created",
    );
    res.status(201).json(order);
  } catch (err) {
    logger.error({ requestId: req.requestId, err }, "Order creation failed");
    next(err);
  }
});
```

A request ID (or correlation ID) threaded through every log line for a given request means you can find every log entry related to one specific failed request, even across multiple service calls/log statements — invaluable when debugging a specific customer-reported issue rather than a general pattern.

---

## What NOT to log

```js
// ❌ never log full documents that might contain sensitive fields
logger.info({ user }, "User created"); // could include the password hash, even if select: false normally hides it in queries
```

```js
// ✅ log only what's needed, explicitly
logger.info({ userId: user._id, email: user.email }, "User created");
```

Per the earlier Node.js documentation's Pino coverage — never log raw documents wholesale, since a `select: false` field can still be present on an in-memory document object that was explicitly queried with `.select("+password")` somewhere upstream; logging is a common, easy-to-miss way sensitive data leaks into a log platform that many more people have access to than the production database itself.

---

## Alerting: what actually deserves a page

| Signal                                                                              | Alert?                                             |
| ----------------------------------------------------------------------------------- | -------------------------------------------------- |
| A single `ConflictError` (duplicate email)                                          | No — normal application behavior                   |
| A sustained spike in unhandled `500` errors                                         | Yes                                                |
| Connection pool consistently near `maxPoolSize`                                     | Yes — investigate before it becomes an outage      |
| MongoDB connection lost and not recovering after a reasonable window                | Yes                                                |
| A slow query newly appearing in the slow-query log                                  | Worth reviewing, not necessarily an immediate page |
| Disk/memory usage on the MongoDB deployment approaching a limit (via Atlas metrics) | Yes                                                |

The general principle: alert on things that indicate an actual or imminent problem for users, not on every error — an alerting system that pages for expected, handled errors trains people to ignore alerts, which is worse than no alerting at all.

## Common mistakes

- **Logging every error at the same severity** — burying genuinely urgent unhandled errors among routine, expected ones like duplicate-key conflicts.
- **Logging full documents**, risking sensitive fields ending up in a log platform.
- **No request/correlation ID** — makes debugging a specific reported issue much harder than it needs to be.
- **Alerting on every error** rather than genuinely actionable signals — leads to alert fatigue and ignored pages.
- **Not watching connection pool metrics until an outage already happened** — this is exactly the kind of leading indicator worth monitoring proactively (per `14-performance/04-connection-pooling.md`).

## Quick summary

- Watch slow queries (via Atlas's Performance Advisor or self-hosted profiling), connection pool health, error rates by type, and connection state changes
- Use structured logging (fields, not string concatenation) so logs are actually searchable/aggregable
- Thread a request/correlation ID through logs to trace a specific failing request across multiple log lines
- Never log full documents that might contain sensitive fields — log specific, chosen fields instead
- Reserve alerting for genuinely actionable signals; expected, handled errors shouldn't page anyone

## Next

**`03-migrations.md`** covers changing a schema safely once real, live data already exists.
