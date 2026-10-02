# Pino & Structured Logging

Why logs should be structured JSON, and how to set up Pino so every log line is fast, safe, and searchable.

## Why `console.log` isn't enough

```js
console.log("User " + user.id + " failed to pay order " + order.id + ": " + err.message);
```

In production this has real problems:

| Problem | Consequence |
|---|---|
| **Unstructured text** | To find "all failures for order 42" you need regexes and luck. Every developer words it differently |
| **No severity** | You can't filter "errors only" or turn debug noise off in production |
| **No context** | Which request? Which user? Which server? Which version? |
| **No timestamp** (or an inconsistent one) | Impossible to correlate with other systems |
| **Errors flattened to strings** | The stack trace and error code are lost or mangled across lines |
| **Synchronous-ish and slow** | `console.log` to a terminal/pipe can block; it's measurably slow at high volume |
| **Secrets leak** | Whatever you concatenate gets logged, including tokens and passwords |

---

## Structured logging

Instead of a sentence, emit a **JSON object per line** (newline-delimited JSON, "NDJSON") with consistent fields:

```json
{"level":"error","time":"2026-10-02T10:15:00.123Z","service":"orders-api","env":"production","version":"1.8.2","requestId":"req_8f3a2c1d","userId":"u_7","orderId":"ord_42","err":{"type":"PaymentError","message":"Card declined","code":"card_declined","stack":"PaymentError: Card declined\n at ..."},"msg":"Payment failed"}
```

Now your log platform can answer precise questions:

```
level=error AND service="orders-api" AND orderId="ord_42"
count by err.code where level=error, last 1h
all logs where requestId="req_8f3a2c1d"           ← the whole story of one request
```

**Rule of thumb:** put the *constant message* in `msg` and the *variable data* in separate fields. That way identical events group together, and you can count them.

```js
// ❌ variable data baked into the message: every line is unique, so nothing groups
logger.error(`Payment failed for order ${order.id}: ${err.message}`);

// ✅ constant message, data as fields
logger.error({ orderId: order.id, err }, "Payment failed");
```

---

## Why Pino

Pino is the standard fast JSON logger for Node.js.

| | Pino | Winston | `console.log` |
|---|---|---|---|
| Output | JSON by default | Configurable (JSON, text) | Text |
| Speed | Very fast (minimal work on the hot path) | Slower (more features in-process) | Moderate, can block |
| Child loggers | ✅ Cheap | ✅ | ❌ |
| Redaction | ✅ Built in | Via formats | ❌ |
| Transports | Run **off the main thread** (worker threads) | In-process | n/a |
| Design | "Log to stdout as JSON; let something else process it" | Many built-in transports | n/a |

Pino's philosophy matches how production systems work: **your app writes JSON lines to stdout, and the platform (Docker, Kubernetes, systemd, a log agent) collects and ships them.** Your app doesn't need to know about Elasticsearch or Loki.

```bash
npm install pino pino-http
npm install -D pino-pretty        # human-readable output for development only
```

---

## Basic usage

```js
// src/config/logger.js
import pino from "pino";

export const logger = pino({
  level: process.env.LOG_LEVEL ?? (process.env.NODE_ENV === "production" ? "info" : "debug"),
  base: {                                               // fields added to EVERY line
    service: "orders-api",
    env: process.env.NODE_ENV,
    version: process.env.APP_VERSION,
  },
  timestamp: pino.stdTimeFunctions.isoTime,             // "time":"2026-10-02T10:15:00.123Z" instead of epoch ms
  formatters: {
    level: (label) => ({ level: label }),               // "level":"info" instead of the number 30
  },
});
```

```js
logger.info("Server started");
logger.info({ port: 3000 }, "Server listening");
logger.warn({ queue: "email", depth: 5400 }, "Queue backlog growing");
logger.error({ err, orderId }, "Payment failed");
```

**The argument order is Pino-specific: object first, message second.** `logger.info("msg", { a: 1 })` does *not* attach `a`.

### Levels

| Level | Number | Use for |
|---|---|---|
| `fatal` | 60 | The process can't continue: about to exit |
| `error` | 50 | An operation failed and needs attention |
| `warn` | 40 | Something unexpected but handled (retry, fallback, deprecated use) |
| `info` | 30 | Normal significant events: startup, request completed, job finished |
| `debug` | 20 | Diagnostic detail for developers |
| `trace` | 10 | Extremely verbose |
| `silent` | n/a | Turn logging off (useful in tests) |

Setting `level: "info"` drops `debug` and `trace` **before they cost anything**. Pick levels deliberately:

- **`error` should mean "someone may need to act."** If errors are routine (a user typed a bad password), they're `info` or `warn`, not `error`. Otherwise alerts on error logs become noise and people ignore them.
- A client's `4xx` is not your server's `error`. Log `4xx` at `info`/`warn` and `5xx` at `error`.
- Make the level configurable by environment variable so you can turn on `debug` in production for a short time without a deploy.

---

## Logging errors properly

Pass the error under the **`err`** key. Pino's built-in serializer turns it into structured fields (type, message, stack, plus custom properties).

```js
try {
  await chargeCard(order);
} catch (err) {
  logger.error({ err, orderId: order.id }, "Payment failed");
}
```

```json
{"level":"error","err":{"type":"PaymentError","message":"Card declined","stack":"PaymentError: Card declined\n    at chargeCard (...)","code":"card_declined"},"orderId":"ord_42","msg":"Payment failed"}
```

```js
// ❌ loses the stack and structure
logger.error("Payment failed: " + err);
logger.error({ error: err.message }, "Payment failed");        // no stack, and a different key than elsewhere

// ❌ the wrong key: Pino only applies the error serializer to `err`
logger.error({ error: err }, "Payment failed");                 // serializes as {} (Error properties aren't enumerable)

// ✅ always `err`
logger.error({ err }, "Payment failed");
```

Log an error **once**, at the layer that handles it (usually the central error handler: `09-api-development/05-error-responses.md`). Logging at every layer as the error bubbles up produces five copies of the same stack trace.

---

## Child loggers: attach context once

`logger.child(bindings)` returns a new logger that adds those fields to every line. Children are cheap.

```js
// per module
const log = logger.child({ module: "orders" });
log.info("Order placed");                             // → {"module":"orders","msg":"Order placed",...}

// per request (this is what pino-http does for you: req.log)
const reqLog = logger.child({ requestId: "req_8f3a2c1d", userId: "u_7" });
reqLog.info({ orderId: "ord_42" }, "Placing order");
reqLog.info("Payment authorized");                    // still carries requestId and userId
```

Create a child **per request/job/connection**, not per log call, and pass it down (or, better, retrieve it automatically from `AsyncLocalStorage`: see `02-correlation-id.md`).

---

## HTTP request logging with `pino-http`

```js
// src/middleware/httpLogger.js
import pinoHttp from "pino-http";
import { randomUUID } from "node:crypto";
import { logger } from "../config/logger.js";

export const httpLogger = pinoHttp({
  logger,

  // one ID per request, honoring an incoming one from the proxy or caller (validate it: see 02)
  genReqId: (req, res) => {
    const id = req.headers["x-request-id"];
    const requestId = typeof id === "string" && /^[\w-]{8,64}$/.test(id) ? id : randomUUID();
    res.setHeader("X-Request-Id", requestId);
    return requestId;
  },

  // pick the level from the outcome
  customLogLevel: (req, res, err) => {
    if (err || res.statusCode >= 500) return "error";
    if (res.statusCode >= 400) return "warn";
    return "info";
  },

  customSuccessMessage: (req, res) => `${req.method} ${req.url} → ${res.statusCode}`,

  // don't spam the logs with health checks and metrics scrapes
  autoLogging: { ignore: (req) => req.url === "/health" || req.url === "/metrics" },

  // control exactly what is logged about the request/response
  serializers: {
    req: (req) => ({ id: req.id, method: req.method, url: req.url, remoteAddress: req.remoteAddress, userAgent: req.headers["user-agent"] }),
    res: (res) => ({ statusCode: res.statusCode }),
  },

  // extra fields on the completion line
  customProps: (req) => ({ userId: req.user?.id }),
});
```

```js
// app.js: register FIRST so every later middleware and handler has req.log
app.use(httpLogger);

app.get("/orders/:id", async (req, res) => {
  req.log.info({ orderId: req.params.id }, "Loading order");     // includes the request id automatically
  // ...
});
```

Each request produces **one completion line** with method, URL, status, and `responseTime` (ms), the raw material for latency and error-rate analysis even before you have metrics:

```json
{"level":"info","time":"...","req":{"id":"req_8f3a2c1d","method":"GET","url":"/api/v1/orders/42"},"res":{"statusCode":200},"responseTime":38,"msg":"GET /api/v1/orders/42 → 200"}
```

Note: `req.user` is set by authentication middleware that runs *after* `httpLogger`, but `customProps` is evaluated when the response finishes, by which time `req.user` is populated.

---

## Never log secrets: redaction

Logs are copied to many systems and read by many people, and they live far longer than you expect. **Treat logs as a data-leak channel.**

Never log: passwords, tokens (`Authorization` headers, JWTs, refresh tokens, API keys), session cookies, full card numbers, government IDs, and unnecessary personal data (`08-authentication-security/`).

Pino can **redact** by path before writing:

```js
export const logger = pino({
  redact: {
    paths: [
      "req.headers.authorization",
      "req.headers.cookie",
      'res.headers["set-cookie"]',
      "*.password",
      "*.passwordHash",
      "*.token",
      "*.refreshToken",
      "*.apiKey",
      "body.cardNumber",
      "user.email",                       // decide deliberately what counts as PII for you
    ],
    censor: "[REDACTED]",                 // or { remove: true } to drop the key entirely
  },
});
```

Rules for redaction:

- Paths are **exact** (with `*` wildcards for one level): `*.password` matches `user.password` but **not** `a.b.password`. Test your redaction.
- Redaction is a **safety net, not a strategy**. The primary defense is not putting secrets in log objects in the first place, so **don't log whole request bodies or whole user objects**: pick fields.
- Don't log request/response **bodies** by default. If you need them for debugging, enable them temporarily, behind a flag, with redaction and size caps.
- Watch for secrets in places you wouldn't expect: URLs (`?token=...`), error messages from libraries that include connection strings, and `err.config` on HTTP client errors (axios errors embed request headers).
- Add a test that logs a sample object and asserts the secret never appears (`13-testing/`).

```js
test("redacts authorization headers", () => {
  const lines = [];
  const log = pino({ redact: ["req.headers.authorization"] }, { write: (s) => lines.push(JSON.parse(s)) });
  log.info({ req: { headers: { authorization: "Bearer secret" } } }, "x");
  expect(lines[0].req.headers.authorization).toBe("[Redacted]");     // Pino's default censor text
});
```

---

## Development vs production output

```js
const isProd = process.env.NODE_ENV === "production";

export const logger = pino({
  level: process.env.LOG_LEVEL ?? (isProd ? "info" : "debug"),
  ...(isProd
    ? {}                                                     // production: raw JSON to stdout (the default)
    : { transport: { target: "pino-pretty", options: { colorize: true, translateTime: "HH:MM:ss", ignore: "pid,hostname" } } }),
});
```

- **Development:** `pino-pretty` turns JSON into readable colored lines. (It runs in a worker thread via Pino's transport system.)
- **Production:** write plain JSON to stdout. Don't pretty-print in production (slower, and unparseable by log platforms).
- **Tests:** `level: "silent"` (or capture lines into an array, as above) so test output stays clean.

Alternatively pipe the same JSON through the CLI: `node server.js | npx pino-pretty`.

---

## Where logs go: the twelve-factor way

**Your app writes to stdout. Something else handles the rest.**

```
Node app ──stdout (JSON lines)──▶ container runtime ──▶ log agent ──▶ aggregation platform
                                                       (Fluent Bit,    (Loki, Elasticsearch,
                                                        Promtail,       CloudWatch, Datadog,
                                                        Vector, or      Splunk...)
                                                        the platform's
                                                        built-in agent)
```

Why this is better than writing files or talking to a log server from the app:

- The app stays simple and doesn't block on a slow or unreachable log backend.
- You can change the destination without changing code.
- Containers/Kubernetes/PaaS already capture stdout.
- No file rotation, disk-full, or permission problems in the app.

If you do write to files (traditional servers), use `pino/file` or `pino.destination("/var/log/app.log")` with **logrotate** doing the rotation, and make sure the logging path can't crash the app when the disk is full.

### Asynchronous destinations and flushing at exit

For maximum throughput, Pino can buffer writes:

```js
const logger = pino(pino.destination({ dest: 1, sync: false, minLength: 4096 }));   // buffered, async to stdout
```

The catch: **buffered logs can be lost if the process dies abruptly**, and the logs right before a crash are the ones you want most. Flush on shutdown:

```js
process.on("SIGTERM", () => {
  logger.info("Shutting down");
  logger.flush();                          // write anything still buffered
  // ...then close servers, DB pools, etc. (16-production/02-graceful-shutdown-and-health-checks.md)
});
```

For most apps the default (synchronous stdout writes in a worker-free setup) is safe and fast enough. Reach for async buffering only when profiling shows logging overhead, and **always flush on `uncaughtException`/`SIGTERM`**.

---

## What to log (and what not to)

### Log these events

- **Startup and shutdown:** version, config summary (not secrets), port, `SIGTERM` received
- **Each request completion:** method, route, status, duration, user ID, request ID (`pino-http` does this)
- **Significant business events:** `order.placed`, `payment.captured`, `user.registered`, with IDs
- **Security events:** login success/failure, password reset requested, permission denied, rate-limit hit, token reuse (`08-authentication-security/`)
- **External calls and their outcomes:** payment provider, email service: target, status, duration
- **Retries, fallbacks, circuit-breaker state changes:** these are the warnings that precede outages
- **Background jobs:** started, finished, failed (with attempt number) (`11-async-processing/02-workers-retry-dlq.md`)
- **Errors:** with the full `err`, once, at the handling layer
- **Slow operations:** queries or calls above a threshold

### Don't

- Secrets, tokens, passwords, card data, unnecessary PII
- Full request/response bodies by default
- Giant payloads (cap sizes; a 5 MB array in every line is expensive)
- High-frequency `debug` noise at `info` level (loops, per-row logs)
- The same error at every layer
- Sentences with embedded data instead of fields
- "Entering function X" / "Leaving function X" tracing (that's what **traces** are for: `04`)

### Choose useful, consistent field names

Agree on names across all services and stick to them:

| Field | Meaning |
|---|---|
| `requestId` | One inbound request (`02-correlation-id.md`) |
| `traceId`, `spanId` | Tracing identifiers (`04`) |
| `userId`, `accountId`, `tenantId` | Who: IDs, never emails or names |
| `orderId`, `jobId`, `eventId` | The entity involved |
| `err` | The error object (always this key) |
| `durationMs` | A duration in milliseconds (include the unit) |
| `service`, `env`, `version` | Where this came from (the `base` fields) |

### A "canonical log line" per unit of work

For important operations, emit **one rich line at the end** summarizing everything, instead of ten scattered lines:

```js
logger.info({
  event: "order.placed",
  orderId, userId, itemCount: items.length, totalCents,
  paymentProvider: "stripe", paymentMs: 212, dbMs: 18,
  outcome: "success",
}, "Order placed");
```

One line per request or job with all the interesting fields is far easier to query and aggregate than reconstructing a story from fragments. It's sometimes called a *wide event*.

---

## Sampling and volume control

Logging every successful request in a high-traffic system can cost more than the system itself.

- **Raise the level** (`info` → `warn`) for noisy components, using a child logger with its own level: `logger.child({ module: "cache" }, { level: "warn" })`.
- **Skip noise:** health checks, metrics scrapes, static assets (`autoLogging.ignore`).
- **Sample** routine success logs, but **never** sample errors:

```js
const shouldLogSuccess = () => Math.random() < 0.1;        // keep ~10% of 2xx request logs
```

- **Set retention** by value: errors and security events longer (30–90 days), debug logs short (days).
- **Alert on volume spikes:** a sudden 10× log rate is itself a signal (an error loop, a retry storm).
- Beware **log loops**: an error handler that logs, which triggers another error, which logs... Rate limit repetitive messages.

---

## Log aggregation and searching

Local `grep` stops working the moment you have more than one instance. Ship logs to a central platform:

| Platform | Notes |
|---|---|
| **Grafana Loki** | Indexes only labels (cheap), stores log lines compressed; queried with LogQL; pairs with Grafana (`05-grafana.md`) |
| **Elasticsearch / OpenSearch (ELK/EFK)** | Powerful full-text search; heavier to run |
| **Cloud-native** (CloudWatch Logs, Google Cloud Logging, Azure Monitor) | Zero infrastructure on that cloud |
| **SaaS** (Datadog, New Relic, Splunk, Better Stack, Papertrail) | Fully managed, priced by volume |

Whatever you choose, make sure you can:

1. **Filter by any JSON field** (`requestId`, `userId`, `level`, `err.code`)
2. **Find all logs for one request** across services in one query
3. **Graph counts** (errors per minute by service)
4. **Alert** on patterns (errors for `payment.*` over a threshold)
5. **Jump** from a log line to its trace and back (`04`, `05`)

A small LogQL taste (Loki):

```
{service="orders-api"} | json | level="error"
{service="orders-api"} | json | requestId="req_8f3a2c1d"
sum by (err_code) (count_over_time({service="orders-api"} | json | level="error" [5m]))
```

### Don't make logs your only monitoring

Logs are great for *what happened in this case* and poor for *how is the system doing overall*. Computing error rates and latency percentiles by parsing logs is slow and expensive; that's what metrics are for (`03`).

---

## Testing and logging

```js
// inject the logger (10-architecture/04-dependency-injection.md): services take `logger` as a dependency
const logs = [];
const logger = pino({ level: "debug" }, { write: (line) => logs.push(JSON.parse(line)) });

await service.placeOrder(input);

expect(logs).toContainEqual(expect.objectContaining({ msg: "Order placed", orderId: expect.any(String) }));
```

- Test **important** log lines (security events, error paths) when other people (alerts, auditors) depend on them.
- Don't assert on every debug line; that's implementation detail.
- Use `level: "silent"` for tests that don't care.

---

## Common mistakes

```js
// ❌ wrong argument order for Pino
logger.info("Order placed", { orderId });            // the object is ignored

// ❌ error under the wrong key (serializes as {})
logger.error({ error: err }, "failed");

// ❌ interpolating data into the message
logger.info(`User ${userId} logged in`);

// ❌ logging whole objects (secrets, PII, huge payloads)
logger.info({ user }, "Login");                      // includes passwordHash, email, address...
logger.info({ req }, "Request");                     // includes headers with Authorization

// ❌ pretty-printing in production; console.log left in the code; multiple logger instances with different formats
// ❌ logging at error for expected client mistakes (404s, validation failures) → alert fatigue
// ❌ the same error logged at every layer
// ❌ no request ID → can't connect lines belonging to one request (02)
// ❌ high-cardinality or giant values in every line
// ❌ relying on logs as the only source of metrics
// ❌ losing buffered logs on crash (async destination without flush)
// ❌ logging in a hot loop without sampling
```

## Checklist

- [ ] One shared Pino instance; JSON to stdout in production; `pino-pretty` for development only
- [ ] `level` configurable via environment variable; sensible default per environment
- [ ] `base` fields: `service`, `env`, `version`; ISO timestamps; string level labels
- [ ] Errors logged as `{ err }`, once, at the handling layer
- [ ] Constant `msg` + data in fields; consistent field names across services
- [ ] `pino-http` (or equivalent) with request IDs, status-based levels, and health checks excluded
- [ ] Redaction configured for auth headers, cookies, tokens, passwords, PII, and tested
- [ ] No bodies, whole objects, or secrets logged by default
- [ ] Logs shipped to a central platform and searchable by `requestId`
- [ ] Retention, sampling, and volume alerts set deliberately
- [ ] Buffered writes flushed on shutdown

## Next

**`02-correlation-id.md`** solves the next problem: your log lines are structured, but a single request produces dozens of them across middleware, services, queues, and other systems. A correlation ID, carried automatically with `AsyncLocalStorage`, ties them all together.
