# Error Handling Strategies

The previous notes covered the mechanics: catching, narrowing, custom errors, the `Result` type. This note is about **design**: which errors to expect, where to handle them, what to log, what to show users, and how to keep a system that fails gracefully instead of mysteriously.

**Prerequisites:**
- [Catching and narrowing errors](./00-catching-and-narrowing-errors.md)
- [Custom errors](./01-custom-errors.md)
- [The Result pattern](./02-result-pattern.md)

---

## Two kinds of errors

| | Operational (expected) | Programmer (bugs) |
|---|---|---|
| Examples | invalid input, record not found, network timeout, rate limit, payment declined | `undefined` is not a function, broken invariant, wrong argument type, missing config |
| Cause | the world is messy | the code is wrong |
| Right response | handle it: retry, fall back, return a clear error to the caller | fail loudly, log with context, fix the code |
| Typical tool | `Result` or a specific custom error | an exception that propagates |

Do not handle bugs as if they were expected. Wrapping a `TypeError` into a friendly "something went wrong" and carrying on hides the defect and corrupts state. Do not treat expected failures as crashes either: a 404 is not an exception worth paging someone for.

## Where to handle errors

**Handle an error at the lowest level that can do something meaningful about it. Otherwise let it travel up to a boundary.**

```text
UI / HTTP handler   <- boundary: translate to a response or message, log once
      |
service layer       <- add context, apply business rules, convert to domain errors
      |
repository / client <- convert library errors into your own error types
      |
driver / network    <- throws raw errors
```

- **Low layers** translate and wrap: turn a database driver error into a `NotFoundError` or `ConflictError`, keeping the original as `cause`.
- **Middle layers** add context and decide on retries or fallbacks.
- **The boundary** (HTTP handler, CLI entry point, queue consumer, UI event handler) is where the error ends. It maps to a status code or message, logs it, and returns.

Catching in the middle just to log and rethrow produces duplicate log lines. **Log once, at the place that handles the error.**

## Fail fast, validate at the boundary

Check inputs as soon as they enter the system, and reject bad data immediately rather than letting it flow deep:

```ts
function createUser(raw: unknown): Result<User, ValidationError> {
  const parsed = userSchema.safeParse(raw);   // runtime validation
  if (!parsed.success) return err(new ValidationError("Invalid user", parsed.error.issues));
  return ok(save(parsed.data));
}
```

Inside the core, trust the types. At the edges (HTTP bodies, environment variables, JSON files, third-party APIs), validate. See [trust boundaries](../15-runtime-validation/00-trust-boundaries.md) and [schema validation](../15-runtime-validation/01-schema-validation.md).

## Translating errors for HTTP APIs

Map your error types to responses in **one place**, not in every handler:

```ts
// Express error-handling middleware: four parameters
app.use((err: unknown, req: Request, res: Response, next: NextFunction) => {
  if (err instanceof AppError) {
    return res.status(toStatus(err)).json({ error: { code: err.code, message: err.message } });
  }
  logger.error({ err }, "Unhandled error");
  res.status(500).json({ error: { code: "INTERNAL", message: "Internal server error" } });
});
```

Principles:

- Expected errors get specific status codes and safe messages.
- Unknown errors become a generic `500`. The details go to logs, **never** to the client (no stack traces, SQL, or file paths).
- Express 5 forwards rejected promises from async handlers to the error middleware automatically. In Express 4 you need a wrapper or `next(err)`, or the rejection goes unhandled.

More in [error response types](../16-type-safe-apis/04-error-response-types.md), [Express](../20-nodejs-backend/02-express.md), and [middleware](../20-nodejs-backend/03-middleware.md).

## Retries and transient failures

Some failures are temporary: network blips, `503`, lock contention. Retrying with backoff helps, but only when it is safe.

```ts
const sleep = (ms: number) => new Promise((r) => setTimeout(r, ms));

async function retry<T>(fn: () => Promise<T>, attempts = 3, baseMs = 200): Promise<T> {
  let lastError: unknown;
  for (let i = 0; i < attempts; i++) {
    try {
      return await fn();
    } catch (e) {
      lastError = e;
      if (i < attempts - 1) await sleep(baseMs * 2 ** i);   // 200, 400, 800 ms
    }
  }
  throw lastError;
}
```

Rules of thumb:

- **Retry only transient errors** (timeouts, connection resets, `429`, `503`), not validation errors or `404`s. Filter inside the `catch`.
- **Retry only idempotent operations,** or make them idempotent with a key. Retrying "charge the card" can double-charge.
- **Cap attempts and add jitter** so many clients do not retry in lockstep.
- **Set timeouts.** A hung request that never fails cannot be retried. `AbortSignal.timeout(ms)` works with `fetch`.

## Cleanup

Release resources whether or not the work succeeded:

```ts
const conn = await pool.connect();
try {
  await conn.query(sql);
} finally {
  conn.release();     // always runs
}
```

TypeScript 5.2 added `using` declarations for deterministic disposal of objects that implement `Symbol.dispose`. It needs runtime support and the right `lib` setting, so check your target before relying on it.

## Global safety nets

Last-resort handlers exist to **log and exit cleanly**, not to continue as if nothing happened.

**Node.js:**

```ts
process.on("unhandledRejection", (reason) => {
  logger.error({ reason }, "Unhandled promise rejection");
  process.exitCode = 1;
  // let the process supervisor restart the service
});

process.on("uncaughtException", (err) => {
  logger.fatal({ err }, "Uncaught exception");
  process.exit(1);   // state may be corrupt: do not keep running
});
```

After an uncaught exception, the process may be in an inconsistent state. Log and exit, and let a supervisor (systemd, Kubernetes, PM2) restart it. Newer Node versions already crash on unhandled rejections by default.

**Browser and React:**

```ts
window.addEventListener("unhandledrejection", (e) => report(e.reason));
window.addEventListener("error", (e) => report(e.error));
```

React **error boundaries** catch rendering errors in their subtree. They must be class components (using `getDerivedStateFromError` / `componentDidCatch`) or provided by a library. They do **not** catch errors in event handlers, async code, or server-side rendering, so those still need `try/catch`. See [React and frontend](../19-react-and-frontend/README.md).

## What to log

- **A stable identifier or code**, the message, and the stack.
- **The `cause` chain.** Do not lose the original.
- **Context:** request id, user id (where policy allows), operation, relevant ids.
- **Do not log secrets or personal data:** tokens, passwords, full card numbers.
- **One entry per error,** at the handling boundary.

Structured logs (JSON with fields) are far easier to search than interpolated strings. See [logging and observability](../21-production-tooling/06-logging-and-observability.md).

## What to show users

- A **clear, actionable message** for expected errors ("That email is already registered").
- A **generic message plus a reference id** for unexpected ones ("Something went wrong. Reference: 7f3a"). The id lets support find the log entry.
- Never raw error text, stack traces, or internal identifiers.

## Putting it together

A common, workable convention for a backend:

1. **Validate** input at the boundary with a schema. Invalid input returns a validation error.
2. **Domain and service code** returns `Result` for expected business failures, or throws specific `AppError` subclasses. Pick one style per layer and stay consistent.
3. **Infrastructure code** converts library errors into your error types, preserving `cause`.
4. **Programmer errors** are left to propagate.
5. **A single error handler** at the top maps `AppError` to responses, logs once, and returns a generic `500` for everything else.
6. **Global handlers** log and exit for the truly unhandled.

## Common mistakes

- **Swallowing errors** (`catch {}`) or logging them and carrying on as if nothing happened.
- **Logging and rethrowing at every layer,** producing the same stack five times.
- **Catching too broadly** and hiding bugs behind a default value.
- **Leaking internals to clients** in error messages.
- **Retrying non-idempotent or permanent failures.**
- **No timeouts,** so failures never surface.
- **Mixing styles inconsistently** (some functions throw, some return `Result`, no convention).
- **Continuing after `uncaughtException`.**

## Debugging

- Make sure each error is logged exactly once with its full `cause` chain and a request id.
- If failures are invisible, look for `catch {}` blocks, missing `await`, and unhandled rejections (the lint rule `@typescript-eslint/no-floating-promises` helps).
- If a retry loop makes things worse, check for non-idempotent operations and missing backoff.
- Reproduce with the real error shape: log `e` as received before you assume its type.
- Use source maps in production traces ([debugging](../18-testing-and-debugging/05-debugging.md)).

## Quick summary

- Separate **expected (operational)** errors from **bugs**. Handle the first, surface the second.
- Handle errors at the lowest level that can act. Otherwise let them reach a boundary. Log once.
- Validate at the edges, then trust the types inside.
- Map errors to responses in one central handler. Never leak internals.
- Retry only transient, idempotent failures, with backoff and timeouts.
- Use `finally` for cleanup, and global handlers to log and exit, not to carry on.

**Next:** [12 Async and Iteration](../12-async-and-iteration/README.md)