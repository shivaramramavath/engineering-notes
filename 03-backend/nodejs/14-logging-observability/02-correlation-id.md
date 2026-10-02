# Correlation ID

Tracking one request through every log line, service, queue, and system it touches — automatically, with `AsyncLocalStorage`.

## The problem: a thousand interleaved log lines

A production server handles many requests at once. Their log lines interleave:

```
10:15:00.101 INFO  Loading order
10:15:00.102 INFO  Loading order
10:15:00.104 INFO  Payment authorized
10:15:00.105 ERROR Card declined
10:15:00.107 INFO  Payment authorized
10:15:00.110 INFO  Sending confirmation email
```

Which "Card declined" belongs to which request? Which user is complaining about it? Without a shared identifier, you can't reconstruct what happened to *one* request, and it only gets worse across services.

A **correlation ID** (also called request ID) is a unique value assigned to a request at the edge of your system and attached to **every** log line, error response, outgoing call, and background job that request causes.

```
{"requestId":"req_8f3a2c1d","msg":"Loading order"}
{"requestId":"req_77b1e0aa","msg":"Loading order"}
{"requestId":"req_8f3a2c1d","msg":"Payment authorized"}
{"requestId":"req_77b1e0aa","err":{"code":"card_declined"},"msg":"Payment failed"}    ← filter by this ID: the whole story
```

Then the support flow becomes simple:

```
Customer: "I got an error at 10:15."
Support:  asks for the requestId shown in the error (09-api-development/05-error-responses.md)
You:      search logs for requestId="req_77b1e0aa" → see every line, in every service, in order
```

---

## Where the ID comes from

```
Browser / mobile app ──▶ CDN / Load balancer ──▶ API gateway ──▶ Your service ──▶ Another service
        │                       │                     │               │                 │
   may send one          may add one           may add one      honor or create   honor it, pass it on
  (X-Request-Id)         (X-Amzn-Trace-Id,      (X-Request-Id)                     (same ID!)
                          X-Cloud-Trace-Context)
```

The rule: **reuse an incoming ID if there is a valid one; otherwise generate one; and always propagate it onward.** The first service in the chain creates it; everyone else passes it along.

### Why validate an incoming ID?

An incoming header is **untrusted input**. If you log it blindly:

- **Log injection / forging:** a client sends `X-Request-Id: abc\n{"level":"info","msg":"admin logged in"}` and fakes log lines (less of a risk with JSON logging, but still can pollute searches or confuse parsers).
- **Huge values** bloat every log line.
- **Collisions:** a malicious client reuses *another* request's ID to muddy your investigation.

So accept only well-formed IDs and otherwise generate your own:

```js
const VALID_ID = /^[A-Za-z0-9_-]{8,64}$/;

function resolveRequestId(header) {
  return typeof header === "string" && VALID_ID.test(header) ? header : `req_${randomUUID()}`;
}
```

For requests from the **public internet**, some teams ignore client-supplied IDs entirely at the edge and always generate their own. Honor incoming IDs from **trusted internal callers** (other services), and treat them as untrusted from outside.

---

## The hard part: getting the ID to every log line

The obvious approach is to pass the logger (or ID) as a parameter through every function:

```js
// ❌ "prop drilling": every function signature carries the logger
async function placeOrder(input, log) {
  const user = await loadUser(input.userId, log);
  await chargeCard(user, input.total, log);
  await sendEmail(user, log);
}
```

It works, but it's noisy, easy to forget, and impossible when the code that logs is deep inside a library or a repository that doesn't take a logger. You want the logger to *know* which request it's serving without being told.

### The wrong fix: a global variable

```js
// ❌ BROKEN under concurrency
let currentRequestId;
app.use((req, res, next) => { currentRequestId = req.id; next(); });
// request A sets it, then awaits... request B overwrites it... A's later logs now carry B's ID
```

Node handles many requests in one process, interleaved at every `await`. A shared variable gets overwritten constantly, which is a *wrong* ID and worse than none.

### The right fix: `AsyncLocalStorage`

`AsyncLocalStorage` (from `node:async_hooks`) gives you a storage slot that follows an **asynchronous execution chain**: everything that happens as a consequence of a given request (promises, `await`s, timers, callbacks, streams) sees *that request's* value, regardless of how many other requests are interleaved. It's the Node equivalent of "thread-local storage."

```js
// src/context.js
import { AsyncLocalStorage } from "node:async_hooks";

const storage = new AsyncLocalStorage();

export const requestContext = {
  run: (context, fn) => storage.run(context, fn),    // everything inside fn (and its async descendants) sees `context`
  get: () => storage.getStore(),                      // undefined outside any request
  set: (key, value) => {                              // add data mid-request (e.g. userId after auth)
    const store = storage.getStore();
    if (store) store[key] = value;
  },
};
```

```js
// src/middleware/requestContext.js
import { randomUUID } from "node:crypto";
import { requestContext } from "../context.js";

const VALID_ID = /^[A-Za-z0-9_-]{8,64}$/;

export function requestContextMiddleware(req, res, next) {
  const incoming = req.get("X-Request-Id");
  const requestId = incoming && VALID_ID.test(incoming) ? incoming : `req_${randomUUID()}`;

  res.setHeader("X-Request-Id", requestId);            // echo it so clients and support can quote it

  requestContext.run({ requestId, startedAt: Date.now() }, () => {
    next();                                            // everything downstream runs INSIDE this context
  });
}
```

Now **any code, anywhere**, can read the current request's ID without being passed anything:

```js
requestContext.get()?.requestId;     // "req_8f3a2c1d" inside any function called while handling this request
```

### Wire it into the logger with `mixin`

Pino's `mixin` option runs on **every log call** and merges extra fields into the line. Point it at the async context:

```js
// src/config/logger.js
import pino from "pino";
import { requestContext } from "../context.js";

export const logger = pino({
  level: process.env.LOG_LEVEL ?? "info",
  base: { service: "orders-api", env: process.env.NODE_ENV },
  mixin() {
    const ctx = requestContext.get();
    return ctx ? { requestId: ctx.requestId, userId: ctx.userId } : {};
  },
});
```

Now **every** `logger.info(...)` anywhere in the codebase (middleware, services, repositories, even helper libraries that receive your logger) automatically includes `requestId` and `userId`, with no parameters passed:

```js
// deep in a repository: knows nothing about HTTP
export async function findOrder(id) {
  logger.debug({ orderId: id }, "Querying order");     // → {"requestId":"req_8f3a2c1d","userId":"u_7","orderId":"42","msg":"Querying order"}
  return db.query("SELECT ...", [id]);
}
```

### Add data as you learn it

```js
// after authentication succeeds
function authenticate(req, res, next) {
  const user = verifyToken(req);
  req.user = user;
  requestContext.set("userId", user.id);               // all later log lines carry userId too
  next();
}
```

### Middleware order matters

```js
app.use(requestContextMiddleware);    // FIRST: so everything after it (logging, auth, handlers) is inside the context
app.use(httpLogger);                  // pino-http: logs request completion
app.use(express.json());
app.use(authenticate);
app.use("/api", routes);
app.use(errorHandler);                // runs inside the same context, so error logs carry the ID too
```

### Combine with `pino-http`

Make `pino-http` reuse the same ID so its completion line matches everything else:

```js
export const httpLogger = pinoHttp({
  logger,
  genReqId: (req) => requestContext.get()?.requestId ?? randomUUID(),
});
```

---

## Put the ID in your API responses

Make it trivially easy for a user or developer to give you the ID:

```js
// header on every response (set in the middleware above)
X-Request-Id: req_8f3a2c1d
```

```js
// and inside every error body (09-api-development/05-error-responses.md)
{ "error": { "code": "internal_error", "message": "Something went wrong on our side", "requestId": "req_8f3a2c1d" } }
```

```js
// error handler
res.status(status).json({ error: { ...body, requestId: requestContext.get()?.requestId } });
```

Show it in your UI's error screens ("Something went wrong. Reference: req_8f3a2c1d"). It turns "it didn't work" support tickets into one-search investigations.

---

## Propagating the ID to everything else

A request's journey doesn't end at your process boundary. Pass the ID on, and restore it where the work resumes.

### Outgoing HTTP calls

```js
// one place to build outbound requests, so no call forgets the header
export async function httpGet(url, options = {}) {
  const requestId = requestContext.get()?.requestId;
  return fetch(url, {
    ...options,
    headers: { ...options.headers, ...(requestId && { "X-Request-Id": requestId }) },
    signal: options.signal ?? AbortSignal.timeout(5000),
  });
}
```

The downstream service's `requestContextMiddleware` picks it up, so both services' logs share the ID. (With OpenTelemetry, the standard `traceparent` header does this automatically: `04-tracing-and-opentelemetry.md`.)

### Background jobs (BullMQ)

The request ends, but the work it triggers continues in a **different process**, possibly much later. Put the ID in the job and restore it in the worker.

```js
// producer: attach the ID when enqueueing (11-async-processing/01-queues-and-bullmq.md)
await emailQueue.add("welcome", {
  userId: user.id,
  _ctx: { requestId: requestContext.get()?.requestId },       // underscore prefix: metadata, not business data
});
```

```js
// worker: re-establish the context for the duration of the job
new Worker("email", async (job) => {
  const parentRequestId = job.data._ctx?.requestId;

  return requestContext.run(
    { requestId: parentRequestId ?? `job_${job.id}`, jobId: job.id, parentRequestId },
    async () => {
      logger.info({ queue: "email", attempt: job.attemptsMade + 1 }, "Job started");   // carries the original requestId
      await sendWelcomeEmail(job.data);
      logger.info("Job finished");
    }
  );
}, { connection });
```

Now searching for the original `requestId` also shows the emails, retries, and failures that request *caused*, even hours later in another process. Keeping the *job's own* ID (`jobId`) as a separate field lets you tell apart the original request and each background attempt.

### Kafka and other message brokers

Use **message headers** (not the payload), since this is metadata about the message, not business data (`11-async-processing/04-kafka.md`):

```js
// producer
await producer.send({ topic: "orders", messages: [{ key: order.id, value, headers: { "x-request-id": requestContext.get()?.requestId ?? "" } }] });

// consumer
await consumer.run({
  eachMessage: async ({ message }) => {
    const requestId = message.headers["x-request-id"]?.toString();
    await requestContext.run({ requestId: requestId || `msg_${randomUUID()}` }, () => handle(message));
  },
});
```

### WebSockets

Socket.IO has no single request, so create a **per-connection** ID at connect time and a **per-event** ID if you want finer granularity (`12-realtime/`):

```js
io.on("connection", (socket) => {
  const connectionId = `conn_${randomUUID()}`;
  socket.use((packet, next) => {
    requestContext.run({ requestId: `ev_${randomUUID()}`, connectionId, userId: socket.data.userId }, next);
  });
});
```

(Verify against your Socket.IO version that context set in packet middleware reaches the event handler: if not, wrap the handler instead, using a small `withContext(handler)` helper.)

### Scheduled jobs and scripts

Every unit of work needs *some* ID, even if nothing triggered it, so wrap cron ticks and scripts:

```js
cron.schedule("0 2 * * *", () =>
  requestContext.run({ requestId: `cron_${randomUUID()}`, job: "nightly-cleanup" }, runNightlyCleanup));
```

### Databases and third-party APIs

- Some databases let you tag queries: a SQL comment (`/* requestId=req_8f3a2c1d */ SELECT ...`) shows up in `pg_stat_activity` and slow query logs, which helps connect a slow query back to a request. (Keep it out of prepared-statement text that you want cached, and sanitize the value.)
- For third-party APIs that support it, send the ID as an **idempotency or tracking** header, and log the provider's own request ID alongside yours:

```js
const res = await fetch(url, { method: "POST", headers: { "Idempotency-Key": key }, body });
logger.info({ provider: "stripe", providerRequestId: res.headers.get("request-id"), status: res.status }, "Provider call");
```

When you contact their support about a failure, *their* ID is what they can look up.

---

## Correlation ID vs trace ID

| | Correlation / request ID | Trace ID (OpenTelemetry) |
|---|---|---|
| Created by | You (a header and middleware) | The tracing SDK |
| Standard | Convention (`X-Request-Id`) | W3C **Trace Context** (`traceparent` header) |
| Gives you | "All log lines for this request" | The above **plus** a timeline of spans (what called what, how long each took) |
| Needs | `AsyncLocalStorage` + a few lines | Instrumentation, a collector, and a trace backend |
| Cost | Nearly free | Moderate (sampling at scale) |

They're **complementary, and converge** once you adopt tracing: you then put `traceId` (and `spanId`) into each log line, and it effectively *becomes* your correlation ID. Many teams run with request IDs first (cheap, immediate value), then add tracing and either keep `requestId` for user-facing support references or switch to `traceId` everywhere. See `04-tracing-and-opentelemetry.md`, which also shows the `trace_id` log-injection that makes logs and traces clickable in Grafana (`05-grafana.md`).

```js
// when tracing is on, enrich the mixin so logs link to traces
import { trace } from "@opentelemetry/api";

mixin() {
  const ctx = requestContext.get();
  const span = trace.getActiveSpan()?.spanContext();
  return { requestId: ctx?.requestId, userId: ctx?.userId, ...(span && { traceId: span.traceId, spanId: span.spanId }) };
}
```

---

## `AsyncLocalStorage`: things to know

### It's efficient, and it's stable

`AsyncLocalStorage` is stable in modern Node versions and has low overhead (recent Node releases improved it considerably). It's what OpenTelemetry's Node context manager, Sentry, and many frameworks use internally.

### `run` vs `enterWith`

```js
storage.run(context, () => { /* context applies here and to async descendants */ });   // ✅ scoped, predictable

storage.enterWith(context);          // ⚠️ sets it for the REST of the current execution chain: easy to leak
```

Prefer `run`. Use `enterWith` only if you really need it, and understand it.

### Context can be lost

The store follows promises, `async/await`, timers, and most Node APIs. It's **lost** when work crosses a boundary that doesn't preserve async context:

- **Callback-based libraries that queue callbacks themselves** (older connection pools, some event emitters that store callbacks and call them from another context): the callback may run in the context of whoever triggered it, not who registered it. Fix with `AsyncResource.bind(callback)` or `AsyncLocalStorage.bind(callback)`.
- **Event emitters:** listeners run in the context of the code that calls `emit()`, not the code that called `on()`.
- **Worker threads and child processes:** a separate runtime; pass the ID explicitly in the message.
- **Queues/brokers:** pass the ID in the message (as above).
- **Native addons** that call back from their own threads.

If a log line has no `requestId`, suspect a context boundary at that point and test it.

### Shared connection pools

A pooled DB connection runs queries for different requests over time. Context follows your `await`ed call, not the connection, so log lines from your code stay correct. It matters only if the *driver itself* logs from internal callbacks.

### Don't store big things

The store is for small metadata (IDs, tenant, user). Don't stash request bodies, DB handles, or large objects there. They're retained for the life of the async chain.

### Testing it

```js
test("every log line in a request carries the request ID", async () => {
  const lines = [];
  const logger = makeLogger({ write: (s) => lines.push(JSON.parse(s)) });
  const app = buildApp({ logger });

  const res = await request(app).get("/api/v1/orders/42").set("X-Request-Id", "req_test_12345678");

  expect(res.headers["x-request-id"]).toBe("req_test_12345678");
  expect(lines.length).toBeGreaterThan(0);
  expect(lines.every((l) => l.requestId === "req_test_12345678")).toBe(true);
});

test("concurrent requests never mix their IDs", async () => {
  await Promise.all(Array.from({ length: 50 }, (_, i) =>
    request(app).get("/api/v1/slow-endpoint").set("X-Request-Id", `req_test_${String(i).padStart(8, "0")}`)));
  // assert each captured line's requestId matches the one for its own request (e.g. echoed in the log's `path`/`n` field)
});

test("generates an ID when the incoming one is malformed", async () => {
  const res = await request(app).get("/health").set("X-Request-Id", "bad id with spaces\nand newline");
  expect(res.headers["x-request-id"]).toMatch(/^req_[0-9a-f-]{36}$/);
});
```

The concurrency test is the important one: it's what catches the "global variable" class of bug.

---

## Putting it together

```js
// app.js
import express from "express";
import { requestContextMiddleware } from "./middleware/requestContext.js";
import { httpLogger } from "./middleware/httpLogger.js";
import { errorHandler } from "./middleware/errorHandler.js";

const app = express();

app.use(requestContextMiddleware);      // 1. create/accept the ID and open the async context
app.use(httpLogger);                    // 2. log request completion with that ID
app.use(express.json());
// ... auth (sets userId in context), routes ...
app.use(errorHandler);                  // 3. logs errors and returns { error: { requestId } }

export default app;
```

Result for one failing request:

```
{"level":"info","requestId":"req_8f3a2c1d","userId":"u_7","orderId":"ord_42","msg":"Placing order"}
{"level":"warn","requestId":"req_8f3a2c1d","userId":"u_7","provider":"stripe","providerRequestId":"req_xyz","status":503,"msg":"Provider call"}
{"level":"error","requestId":"req_8f3a2c1d","userId":"u_7","err":{"code":"provider_unavailable"},"msg":"Request failed"}
{"level":"error","requestId":"req_8f3a2c1d","userId":"u_7","req":{"method":"POST","url":"/api/v1/orders"},"res":{"statusCode":502},"responseTime":2043,"msg":"POST /api/v1/orders → 502"}
...later, in the worker process:
{"level":"warn","requestId":"req_8f3a2c1d","jobId":"58","attempt":2,"msg":"Retrying order confirmation email"}
```

Query `requestId="req_8f3a2c1d"` and the whole story, across the API and the worker, is in one list.

---

## Common mistakes

```js
// ❌ a module-level variable for the "current request": overwritten by concurrent requests → wrong IDs
// ❌ registering the context middleware AFTER the logger/auth/routes → earlier code is outside the context
// ❌ calling next() outside storage.run(): the context isn't active for downstream handlers
requestContext.run(ctx, () => {});    // ❌ forgot to call next() inside
next();

// ❌ trusting an incoming X-Request-Id without validation (length, charset)
// ❌ not returning the ID in error bodies/headers: support can't ask for it
// ❌ generating a NEW ID in each service instead of propagating the incoming one: the chain is broken
// ❌ not passing the ID into queue messages, so background work is orphaned from the request that caused it
// ❌ using enterWith casually: context leaks into unrelated work
// ❌ relying on context across emitters/callback-queues/worker threads without checking it survives
// ❌ different field names across services (requestId vs request_id vs reqId vs correlationId)
// ❌ putting large objects or secrets in the async store
// ❌ no concurrency test: the bug only appears under load
```

## Checklist

- [ ] Middleware first in the chain: accepts a **validated** `X-Request-Id` or generates one, echoes it in the response header, opens `AsyncLocalStorage`
- [ ] Logger `mixin` reads the context so **every** line has `requestId` (and `userId` once known)
- [ ] `pino-http` reuses the same ID
- [ ] Error bodies and error screens include the `requestId`
- [ ] Outbound HTTP, queue jobs, broker messages, and cron/scripts propagate or create an ID
- [ ] Workers/consumers restore the context with `run(...)` for each unit of work
- [ ] Same field name in every service; `traceId`/`spanId` added once tracing is adopted
- [ ] Tests cover: header echo, every line tagged, concurrent requests not mixed, malformed IDs replaced

## Next

**`03-metrics-and-prometheus.md`** moves from individual requests to the big picture: counters, gauges, and histograms exposed to Prometheus, the RED and USE methods for deciding what to measure, PromQL basics, and alert rules.
