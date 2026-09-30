# Error Handling in Express

Express-specific error handling mechanics: centralized error middleware, and correctly propagating errors from synchronous code, async code, and middleware. This builds directly on the general JavaScript error handling covered in `03-javascript-for-node/05-error-handling.md`.

## The centralized error handler

```js
app.use((err, req, res, next) => {
  console.error(err);

  const statusCode = err.statusCode || 500;
  res.status(statusCode).json({
    error: err.message || "Internal Server Error",
  });
});
```

Express identifies this as error-handling middleware specifically because it takes **four** parameters. It must be registered **after** all routes and other middleware — Express only reaches it when something explicitly calls `next(err)`, or when a synchronous error is thrown inside a regular (non-async) route handler.

---

## How an error reaches the handler

### Synchronous errors — caught automatically

```js
app.get("/users/:id", (req, res) => {
  if (!isValidId(req.params.id)) {
    throw new Error("Invalid ID"); // Express catches this automatically
  }
  res.json(getUser(req.params.id));
});
```

Express wraps synchronous route handlers so a thrown error is automatically caught and routed to your error-handling middleware — no `try/catch` needed for purely synchronous code.

### Async errors — need explicit handling (Express 4)

```js
// ❌ a rejected promise here does NOT reach the error handler on Express 4
app.get("/users/:id", async (req, res) => {
  const user = await db.findUser(req.params.id); // throws if the DB call fails
  res.json(user);
});
```

```js
// ✅ catch it and forward explicitly
app.get("/users/:id", async (req, res, next) => {
  try {
    const user = await db.findUser(req.params.id);
    res.json(user);
  } catch (err) {
    next(err);
  }
});
```

This is the single most important Express-specific error-handling rule on Express 4 and earlier: **`next(err)` is what routes an error to your centralized handler; a thrown error inside an `async` function does not automatically get there on its own.**

### A wrapper to avoid repeating `try/catch` everywhere

```js
const asyncHandler = (fn) => (req, res, next) => {
  Promise.resolve(fn(req, res, next)).catch(next);
};

app.get(
  "/users/:id",
  asyncHandler(async (req, res) => {
    const user = await db.findUser(req.params.id);
    res.json(user);
  }),
);
```

Every route wrapped this way automatically forwards a rejected promise to `next()`, without repeating the `try/catch` boilerplate in every single handler.

### Express 5: this is handled automatically

Express 5 detects when a route handler returns a rejected promise and automatically calls `next(err)` for you — the manual wrapper above becomes unnecessary. Worth checking which major version a given project is actually on, since a lot of existing Express code and tutorials still assume Express 4's manual-forwarding behavior.

---

## Custom error classes, applied to Express

Building directly on `03-javascript-for-node/05-error-handling.md`'s custom error classes:

```js
class AppError extends Error {
  constructor(message, statusCode) {
    super(message);
    this.statusCode = statusCode;
    this.isOperational = true; // distinguishes expected errors from real bugs
  }
}

class NotFoundError extends AppError {
  constructor(message = "Resource not found") {
    super(message, 404);
  }
}

class ValidationError extends AppError {
  constructor(message = "Invalid input") {
    super(message, 400);
  }
}
```

```js
app.get(
  "/users/:id",
  asyncHandler(async (req, res) => {
    const user = await db.findUser(req.params.id);
    if (!user) throw new NotFoundError(`User ${req.params.id} not found`);
    res.json(user);
  }),
);
```

```js
app.use((err, req, res, next) => {
  const statusCode = err.statusCode || 500;
  res.status(statusCode).json({
    error: err.message,
    ...(process.env.NODE_ENV !== "production" && { stack: err.stack }),
  });
});
```

Because every custom error carries its own `statusCode`, the centralized handler needs no `if/else` chain over error types — it just reads `err.statusCode` (falling back to `500` for anything unexpected) and responds accordingly. This is exactly why custom error classes pay off in an Express app specifically.

---

## Hiding internals in production

```js
app.use((err, req, res, next) => {
  const statusCode = err.statusCode || 500;

  const message = err.isOperational ? err.message : "Something went wrong"; // don't leak internal details for unexpected errors

  res.status(statusCode).json({ error: message });

  if (!err.isOperational) {
    console.error("Unexpected error:", err); // log the real details server-side
  }
});
```

The `isOperational` flag (from `03-javascript-for-node/05-error-handling.md`'s operational-vs-programmer-error distinction) lets the handler safely show a real error message for expected failures (`NotFoundError`, `ValidationError`) while hiding implementation details for unexpected bugs — you don't want a stack trace or a raw database error message reaching an API client in production.

---

## 404 handling: routes that don't match anything

```js
// after all real routes, before the error handler
app.use((req, res, next) => {
  res.status(404).json({ error: "Route not found" });
});

app.use((err, req, res, next) => {
  // handles everything else
});
```

A request to a URL with no matching route never triggers the error-handling middleware at all — it just falls through every route with nothing catching it. A catch-all middleware placed after every real route (but before the error handler) is the standard way to return a proper `404` JSON response instead of Express's default HTML error page.

---

## Handling errors thrown inside other middleware

```js
app.use((req, res, next) => {
  try {
    const decoded = verifyToken(req.headers.authorization);
    req.user = decoded;
    next();
  } catch (err) {
    next(err); // same rule applies to any middleware, not just route handlers
  }
});
```

The synchronous-vs-async distinction, and the `next(err)` rule, apply identically to middleware as they do to route handlers — an auth middleware that throws synchronously is caught automatically; one doing `await` needs the same explicit `try/catch` + `next(err)`.

## Common mistakes

- **Assuming Express 4 automatically catches async errors** — it doesn't; forgetting `try/catch` + `next(err)` (or `asyncHandler`) means a rejected promise in a route handler becomes an unhandled rejection instead of a clean error response.
- **Registering the error handler before other routes** — it must be last, since Express only reaches it via `next(err)` from something registered before it.
- **Forgetting the four-parameter signature** on error middleware — `(err, req, res, next)` is what makes Express recognize it as an error handler at all; three parameters makes it a normal (non-error) middleware instead.
- **Leaking internal error details/stack traces to clients in production** — use the `isOperational` distinction to only expose messages for genuinely expected errors.
- **No catch-all 404 handler** — leaves Express's default (unstyled HTML) 404 page in an otherwise JSON API.

## Quick summary

- Centralized error middleware needs exactly four parameters: `(err, req, res, next)`, registered after everything else
- Synchronous errors in route handlers are caught automatically; async errors (Express 4) need explicit `try/catch` + `next(err)`, or a wrapper
- Custom error classes with a `statusCode` property let the centralized handler respond correctly without branching on error type
- An `isOperational` flag lets you show real messages for expected errors while hiding internals for genuine bugs
- A catch-all middleware before the error handler gives a proper JSON `404` for unmatched routes

## Next

**`05-validation.md`** covers validating request input — often the first thing that throws a `ValidationError` like the ones shown here.
