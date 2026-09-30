# Error Responses

A consistent, machine-readable error format — and a single place in your codebase that produces it.

## Why error design matters

Clients write code against your errors exactly as they do against your success responses. They show messages to users, branch on error types, retry certain failures, and file bug reports with the details you give them. A good error answers three questions:

1. **What happened?** (a stable, machine-readable `code`)
2. **Why / what should I do?** (a human-readable `message`, plus `details` where relevant)
3. **How do I report it?** (a `requestId` that maps to your logs)

A bad API returns `{"error": "Something went wrong"}` with status `200` for one route, an HTML stack trace for another, and `{"err": {"msg": "..."}}` for a third.

---

## The error format

Pick **one** shape and use it everywhere — validation errors, auth errors, 404s, rate limits, and unexpected crashes.

```jsonc
{
  "error": {
    "code": "validation_error",
    "message": "Request validation failed",
    "details": [
      { "field": "email", "message": "Must be a valid email address" },
      { "field": "password", "message": "Must be at least 8 characters" }
    ],
    "requestId": "req_8f3a2c1d"
  }
}
```

| Field | Purpose | Rules |
|---|---|---|
| `code` | Stable identifier clients can `switch` on | `snake_case`, never changes once published |
| `message` | Human-readable explanation | May be reworded; clients must not parse it |
| `details` | Optional structured extras (field errors, limits, allowed values) | Shape depends on `code` |
| `requestId` | Correlation ID for support and log lookup | Same value as the `X-Request-Id` header |

### Standards you can adopt instead

**RFC 9457 "Problem Details for HTTP APIs"** (which obsoletes RFC 7807) defines a standard shape and media type:

```jsonc
// Content-Type: application/problem+json
{
  "type": "https://api.example.com/errors/validation",
  "title": "Validation failed",
  "status": 422,
  "detail": "The email field is not a valid address.",
  "instance": "/api/v1/users",
  "errors": [{ "field": "email", "message": "Invalid" }]
}
```

It's a good choice if you want an industry standard; a simple custom format like the one above is also perfectly fine. **What matters is consistency.**

---

## HTTP status code + error code

The status code is for generic clients, proxies, and monitoring; the `code` is for your client's logic.

| Status | `code` examples |
|---|---|
| `400` | `invalid_json`, `invalid_cursor`, `invalid_sort` |
| `401` | `unauthenticated`, `token_expired`, `invalid_token` |
| `403` | `forbidden`, `insufficient_role` |
| `404` | `not_found` |
| `409` | `email_taken`, `version_conflict`, `idempotency_key_reused` |
| `413` | `payload_too_large` |
| `415` | `unsupported_media_type` |
| `422` | `validation_error` |
| `429` | `rate_limited` |
| `500` | `internal_error` |
| `503` | `service_unavailable` |

**Never return `200 OK` with an error in the body** (`{ "success": false }`). It breaks monitoring (your dashboards show 100% success), client libraries, retries, and caches. Use the real status code.

A nice touch: distinct codes for the *same* status let clients react differently.

```js
// 401 token_expired   → client silently refreshes the access token and retries
// 401 invalid_token   → client sends the user to the login screen
```

---

## A custom error class

Throw meaningful errors from anywhere in your code; let one handler turn them into responses.

```js
// utils/AppError.js
export class AppError extends Error {
  constructor(status, code, message, details) {
    super(message);
    this.name = "AppError";
    this.status = status;          // HTTP status
    this.code = code;              // machine-readable code
    this.details = details;        // optional extra info
    this.isOperational = true;     // a known, expected failure (not a bug)
    Error.captureStackTrace?.(this, this.constructor);
  }
}

// convenient factories
export const badRequest   = (code, msg, details) => new AppError(400, code, msg, details);
export const unauthorized = (msg = "Authentication required") => new AppError(401, "unauthenticated", msg);
export const forbidden    = (msg = "You do not have access to this resource") => new AppError(403, "forbidden", msg);
export const notFound     = (what = "Resource") => new AppError(404, "not_found", `${what} not found`);
export const conflict     = (code, msg) => new AppError(409, code, msg);
```

Using it:

```js
export async function get(req, res) {
  const post = await Post.findById(req.params.id);
  if (!post) throw notFound("Post");
  res.json({ data: toPostDto(post) });
}

export async function register(req, res) {
  const exists = await User.findOne({ email: req.body.email });
  if (exists) throw conflict("email_taken", "That email is already registered");
  // ...
}
```

### Operational errors vs programmer errors

| | Operational | Programmer bug |
|---|---|---|
| Examples | Invalid input, not found, upstream timeout, DB down | `TypeError`, undefined variable, bad logic |
| Expected? | Yes, part of normal life | No |
| Client sees | Specific message and code | Generic `500` — **never** internals |
| Server does | Usually log at `warn` | Log at `error` with full stack, alert, investigate |

The `isOperational` flag is how the handler tells them apart. See `03-javascript-for-node/05-error-handling.md`.

---

## The central error handler

Express recognizes an error-handling middleware by its **four arguments**. Register it **last**, after all routes.

```js
// middleware/errorHandler.js
import { ZodError } from "zod";
import { AppError } from "../utils/AppError.js";

export function errorHandler(err, req, res, next) {
  // if headers were already sent, delegate to Express's default handler
  if (res.headersSent) return next(err);

  const requestId = req.id;
  let status = 500;
  let body = {
    code: "internal_error",
    message: "Something went wrong on our side",
  };

  if (err instanceof AppError) {
    status = err.status;
    body = { code: err.code, message: err.message, details: err.details };

  } else if (err instanceof ZodError) {
    status = 422;
    body = {
      code: "validation_error",
      message: "Request validation failed",
      details: err.issues.map((i) => ({
        field: i.path.join("."),
        message: i.message,
      })),
    };

  } else if (err.type === "entity.parse.failed") {           // body-parser: invalid JSON
    status = 400;
    body = { code: "invalid_json", message: "Request body is not valid JSON" };

  } else if (err.type === "entity.too.large") {              // body-parser: over size limit
    status = 413;
    body = { code: "payload_too_large", message: "Request body is too large" };

  } else if (err.name === "TokenExpiredError") {             // jsonwebtoken
    status = 401;
    body = { code: "token_expired", message: "Access token expired" };

  } else if (err.name === "JsonWebTokenError") {
    status = 401;
    body = { code: "invalid_token", message: "Invalid access token" };

  } else if (err.code === "23505") {                         // PostgreSQL unique violation
    status = 409;
    body = { code: "duplicate", message: "Resource already exists" };

  } else if (err.code === 11000) {                           // MongoDB duplicate key
    status = 409;
    body = { code: "duplicate", message: "Resource already exists" };
  }

  // log: warn for expected failures, error (with stack) for unexpected ones
  const log = req.log ?? console;
  if (status >= 500) {
    log.error({ err, requestId, path: req.path }, "Unhandled error");
  } else {
    log.warn({ code: body.code, status, requestId, path: req.path }, "Request failed");
  }

  res.status(status).json({ error: { ...body, requestId } });
}
```

```js
// app.js — order matters
app.use("/api/v1", routes);

app.use((req, res, next) => {                               // catch-all 404, AFTER all routes
  next(new AppError(404, "route_not_found", `Cannot ${req.method} ${req.path}`));
});

app.use(errorHandler);                                       // absolutely last
```

### Async errors

- **Express 5** automatically forwards rejected promises from `async` handlers to the error handler, so `throw` just works.
- **Express 4** does not. Wrap handlers, or use `express-async-errors`:

```js
const asyncHandler = (fn) => (req, res, next) => Promise.resolve(fn(req, res, next)).catch(next);

router.get("/:id", asyncHandler(posts.get));
```

Details in `06-express/04-error-handling.md`.

---

## Never leak internals

```jsonc
// ❌ gives attackers a map of your system
{
  "error": "SequelizeDatabaseError: column \"passwrod\" does not exist",
  "stack": "at /app/src/services/user.js:42:11 ..."
}

// ✅ client gets a generic message + requestId; the truth goes to your logs
{ "error": { "code": "internal_error", "message": "Something went wrong on our side", "requestId": "req_8f3a2c1d" } }
```

Things to keep out of responses: stack traces, SQL, file paths, library and version names, environment details, and internal IDs that reveal structure. A detailed message in development is fine:

```js
if (process.env.NODE_ENV !== "production" && status >= 500) {
  body.debug = { stack: err.stack };
}
```

Be equally careful with *auth* errors: "Invalid email or password" rather than "No account with that email" (`08-authentication-security/01-password-hashing.md`).

---

## Request IDs: connect client reports to logs

```js
import { randomUUID } from "node:crypto";

app.use((req, res, next) => {
  req.id = req.get("X-Request-Id") || `req_${randomUUID().slice(0, 8)}`;
  res.set("X-Request-Id", req.id);
  next();
});
```

When a customer emails "I got an error," the `requestId` lets you find the exact log lines and trace. See `14-logging-observability/02-correlation-id.md`.

---

## Special cases

### Rate limiting (429)

```jsonc
// Headers: Retry-After: 30
{ "error": { "code": "rate_limited", "message": "Too many requests", "details": { "retryAfterSeconds": 30 } } }
```

### Validation failures (422)

List **all** field errors at once in `details`, not just the first (`04-validation.md`).

### Conflicts (409)

Explain what conflicted, without leaking data about other users:

```jsonc
{ "error": { "code": "email_taken", "message": "That email is already registered" } }
```

### Upstream failures

When a third-party service fails, translate it: return `502` or `503` with your own code (`payment_provider_unavailable`), not the provider's raw error. Consider `Retry-After` for `503`.

### Partial failures in batch endpoints

Return `207 Multi-Status` (or `200` with per-item results) instead of failing everything when one item is bad:

```jsonc
{
  "results": [
    { "index": 0, "status": 201, "data": { "id": "1" } },
    { "index": 1, "status": 422, "error": { "code": "validation_error", "message": "..." } }
  ]
}
```

---

## Process-level safety net

Some failures never reach Express: rejected promises nobody awaited, exceptions thrown in timers or event emitters.

```js
process.on("unhandledRejection", (reason) => {
  logger.error({ reason }, "Unhandled promise rejection");
  // treat like a bug: shut down gracefully and let the orchestrator restart the process
});

process.on("uncaughtException", (err) => {
  logger.fatal({ err }, "Uncaught exception");
  process.exit(1);          // state is unknown — exit; do not try to keep serving
});
```

After an `uncaughtException`, the process may be in an inconsistent state, so the safe move is to log, finish in-flight work if possible, and **exit**, relying on Docker/Kubernetes/PM2 to restart it (`16-production/02-graceful-shutdown-and-health-checks.md`, `02-core-modules/07-process.md`).

---

## Testing errors

```js
test("returns the standard shape for a missing post", async () => {
  const res = await request(app).get("/api/v1/posts/does-not-exist").set(authHeader);

  expect(res.status).toBe(404);
  expect(res.body.error).toMatchObject({ code: "not_found" });
  expect(res.body.error.requestId).toBeDefined();
});

test("never leaks stack traces", async () => {
  const res = await request(app).get("/api/v1/boom");
  expect(res.status).toBe(500);
  expect(JSON.stringify(res.body)).not.toMatch(/at .*\.js:\d+/);
});
```

---

## Common mistakes

```js
// ❌ 200 OK with { success: false }
// ❌ different error shapes from different routes or middleware
// ❌ error handler registered BEFORE the routes (it never sees their errors)
// ❌ error handler with only 3 params — Express doesn't treat it as an error handler
app.use((err, req, res) => { /* ... */ });        // must be (err, req, res, next)

// ❌ sending a response twice: calling res.json() then throwing
// ❌ leaking err.message from unexpected errors to the client
// ❌ using free-text messages as the contract ("if message includes 'expired'...")
// ❌ logging validation noise at error level and drowning real problems
```

## Next

**`06-idempotency.md`** tackles a different kind of failure: the client *didn't see* your response (timeout, dropped connection), retries, and you must not charge the customer twice.
