# Production Error Handling

In production you cannot watch the console. Errors must be **caught globally, logged with context, monitored, and communicated safely** to users, and the process must stay healthy or fail in a controlled way.

## Goals

| Goal | How |
|------|-----|
| No silent failures | global handlers, no empty `catch` |
| Fast diagnosis | structured logs, request IDs, stack traces, source maps |
| Safe user experience | friendly messages, no internals leaked |
| Stability | validate input, time out, retry smartly, shut down gracefully |
| Learning | alerting, dashboards, release tracking |

## Global handlers: browser

```js
// Uncaught synchronous errors and script load errors
window.addEventListener("error", (event) => {
  report({ type: "error", message: event.message, source: event.filename, line: event.lineno, col: event.colno, error: event.error });
});

// Promise rejections nobody handled
window.addEventListener("unhandledrejection", (event) => {
  report({ type: "unhandledrejection", reason: event.reason });
  // event.preventDefault();   // suppress the default console error if you handled it
});

// Resource failures (img, script) are captured with useCapture = true
window.addEventListener("error", (e) => { if (e.target !== window) report({ type: "resource", src: e.target.src }); }, true);
```

`error` events for cross-origin scripts show only "Script error." unless the script is served with CORS and loaded with `crossorigin`.

## Global handlers: Node.js

```js
process.on("unhandledRejection", (reason, promise) => {
  logger.error({ err: reason }, "unhandledRejection");
  // Node 15+ terminates by default (--unhandled-rejections=throw): treat it as fatal
  throw reason;
});

process.on("uncaughtException", (err, origin) => {
  logger.fatal({ err, origin }, "uncaughtException");
  shutdown(1);            // the process may be in an undefined state: exit after cleanup
});

process.on("warning", (w) => logger.warn(w));
```

Do **not** keep running after `uncaughtException`. State may be corrupt. Log, flush, exit, and let a supervisor (systemd, Docker, Kubernetes, PM2) restart.

## Graceful shutdown

```js
let shuttingDown = false;

async function shutdown(code = 0) {
  if (shuttingDown) return;
  shuttingDown = true;
  logger.info("shutting down");
  const timer = setTimeout(() => process.exit(1), 10_000).unref();   // hard deadline
  try {
    await new Promise((resolve) => server.close(resolve));            // stop accepting, finish in-flight
    await db.end();
    await flushLogsAndMetrics();
  } finally {
    clearTimeout(timer);
    process.exit(code);
  }
}

process.on("SIGTERM", () => shutdown(0));
process.on("SIGINT", () => shutdown(0));
```

## Express-style central error middleware

```js
const asyncHandler = (fn) => (req, res, next) => Promise.resolve(fn(req, res, next)).catch(next);

app.get("/users/:id", asyncHandler(async (req, res) => {
  const user = await users.find(req.params.id);
  if (!user) throw new NotFoundError("User", req.params.id);
  res.json(user);
}));

app.use((req, res) => res.status(404).json({ error: { code: "NOT_FOUND" } }));

app.use((err, req, res, next) => {
  const known = err instanceof AppError;
  const status = known ? err.status : 500;
  logger[status >= 500 ? "error" : "warn"]({ err, reqId: req.id, path: req.path, userId: req.user?.id });
  res.status(status).json({
    error: {
      code: known ? err.code : "INTERNAL_ERROR",
      message: known && err.expose ? err.message : "Something went wrong",
      requestId: req.id,
    },
  });
});
```

Express 5 forwards rejected promises from async handlers automatically; Express 4 needs a wrapper like `asyncHandler`.

## Structured logging

```js
logger.error({
  err,                                   // serializer outputs name, message, stack, cause chain
  requestId: "9f8a7c",
  userId: 42,
  route: "POST /orders",
  durationMs: 184,
  release: process.env.APP_VERSION,
}, "order creation failed");
```

| Practice | Why |
|----------|-----|
| JSON logs (pino, winston) | searchable, machine-readable |
| Log levels: `fatal`, `error`, `warn`, `info`, `debug` | filter noise |
| Request/correlation IDs propagated across services | trace one request end to end |
| Include release/version and environment | match errors to deploys |
| **Redact** secrets and PII (`password`, `token`, `authorization`, emails, cards) | security and privacy |
| Sample or rate-limit noisy errors | control cost |
| Log once per failure, at the boundary | avoid duplicates |

## Monitoring services

Tools such as **Sentry**, Datadog, Rollbar, Honeybadger, Bugsnag or OpenTelemetry collect errors with stack traces, breadcrumbs, release info and user context.

```js
Sentry.init({ dsn, release: APP_VERSION, environment: "production", tracesSampleRate: 0.1 });

try { await work(); } catch (err) {
  Sentry.captureException(err, { tags: { feature: "checkout" }, extra: { orderId } });
  throw err;
}
```

Upload **source maps** during the build (and keep them private) so minified stack traces are readable.

## Safe messages for users

| Show users | Never show users |
|------------|------------------|
| What happened in plain language | Stack traces |
| What they can do next (retry, contact support) | SQL, file paths, internal hostnames |
| A request ID for support | Secrets or tokens |
| Field-level validation messages | Raw exception messages from libraries |

```js
const userMessage = (err) => err instanceof AppError && err.expose ? err.message : "Something went wrong. Please try again.";
```

## Frontend resilience

- **Error boundaries** (React) or equivalent isolate component failures and show fallbacks
- Wrap event handlers and async actions; update UI state (`error`, `loading`, `retry`)
- Offer **retry** actions and preserve user input
- Degrade gracefully: missing optional data should not break the page
- Handle offline: detect network errors and queue or notify

```jsx
class ErrorBoundary extends React.Component {
  state = { error: null };
  static getDerivedStateFromError(error) { return { error }; }
  componentDidCatch(error, info) { report(error, info.componentStack); }
  render() { return this.state.error ? <Fallback onRetry={() => this.setState({ error: null })} /> : this.props.children; }
}
```

## Defensive practices

| Practice | Detail |
|----------|--------|
| **Validate input at the edge** | schema validation (Zod, Valibot, Ajv) and reject early with `ValidationError` |
| **Timeouts everywhere** | `AbortSignal.timeout`, DB/query timeouts, server request timeouts |
| **Retries with backoff and jitter** | only for idempotent/transient failures |
| **Circuit breakers / bulkheads** | isolate failing dependencies |
| **Idempotency keys** | safe retries for payments and writes |
| **Rate limiting and backpressure** | prevent overload |
| **Health checks** | `/healthz` (liveness), `/readyz` (readiness) |
| **Feature flags and kill switches** | disable failing features fast |
| **Invariants as assertions** | fail loudly on impossible states |

```js
import assert from "node:assert/strict";
assert(Array.isArray(items), "items must be an array");
```

## Alerts and on-call hygiene

- Alert on **symptoms** (error rate, latency, failed checkouts) more than on single exceptions
- Group errors by fingerprint; track **new** vs **regressed** issues
- Link errors to releases; roll back quickly
- Write runbooks for common failures
- Review top errors regularly and delete noise (known harmless errors) via filters

## Testing error paths

```js
test("returns 404 for unknown users", async () => {
  const res = await request(app).get("/users/999");
  expect(res.status).toBe(404);
  expect(res.body.error.code).toBe("NOT_FOUND");
  expect(JSON.stringify(res.body)).not.toMatch(/stack|at .*\(/);   // nothing leaked
});

test("retries transient failures", async () => {
  const fn = vi.fn().mockRejectedValueOnce(new Error("503")).mockResolvedValue("ok");
  expect(await withRetry(fn)).toBe("ok");
  expect(fn).toHaveBeenCalledTimes(2);
});
```

Also test timeouts, malformed input, dependency outages (mocked), and graceful shutdown.

## Checklist

1. Global handlers registered (browser `error`/`unhandledrejection`, Node `uncaughtException`/`unhandledRejection`)
2. Central HTTP error handler that maps errors to status and safe bodies
3. Structured, redacted logs with request IDs
4. Monitoring with source maps and release tracking
5. Timeouts, retries (safe ones), and graceful shutdown
6. User-friendly messages, no leaked internals
7. Alerts on meaningful signals and a path to rollback

## Pitfalls

| Pitfall | Why it hurts | Better |
|---------|--------------|--------|
| Continuing after `uncaughtException` | Corrupt state | Log, cleanup, exit, restart |
| Logging sensitive data | Privacy and security incidents | Redaction |
| Showing raw errors to users | Leaks and confusion | Safe messages + request ID |
| No unhandled-rejection handling | Silent failures or crashes | Handlers and lint rules (`no-floating-promises`) |
| Minified stacks without source maps | Undebuggable | Upload private source maps |
| Retrying everything | Duplicate charges, overload | Idempotency + backoff |
| Alerting on every exception | Alert fatigue | Symptom- and rate-based alerts |
| No request correlation | Cannot trace failures | Request/trace IDs |
| Swallowing errors to keep logs clean | Hidden outages | Fix or filter deliberately |
| No timeouts | Hung requests exhaust resources | Timeouts on all I/O |

## Key takeaways

- Register global handlers, but treat `uncaughtException` as fatal: clean up and exit
- Centralize HTTP error handling: map known errors to codes, hide the rest
- Log once, structured and redacted, with request IDs; send errors to a monitoring service with source maps
- Add timeouts, safe retries, validation and graceful shutdown
- Show users helpful, safe messages and offer recovery paths

**Next:** [Asynchronous JavaScript](../11_asynchronous-javascript/00_README.md)
