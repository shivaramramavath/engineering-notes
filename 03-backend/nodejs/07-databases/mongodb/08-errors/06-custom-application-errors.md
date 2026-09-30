# Custom Application Errors

Wrapping Mongoose's raw errors in your own error classes, so the rest of your codebase — service functions, background jobs, anything that isn't directly an Express route — doesn't need to know Mongoose-specific error shapes at all.

## Why wrap errors at all?

```js
// service function — knows about Mongoose's error.code === 11000 directly
export async function registerUser(data) {
  try {
    return await User.create(data);
  } catch (err) {
    if (err.code === 11000) {
      throw err; // the CALLER now has to know Mongoose-specific details too
    }
    throw err;
  }
}
```

```js
// somewhere else entirely, maybe a background job, maybe a CLI script, using the same service
try {
  await registerUser(data);
} catch (err) {
  if (err.code === 11000) {
    // this caller ALSO needs to know Mongoose's specific error shape
    // ...
  }
}
```

Every consumer of `registerUser` needs to independently know that a duplicate email surfaces as `err.code === 11000` — a Mongoose-specific detail leaking into every corner of the codebase that ever calls this function, and something that would break if you ever swapped the underlying data layer.

---

## Defining your own error classes

```js
// errors/AppError.js
export class AppError extends Error {
  constructor(message, statusCode) {
    super(message);
    this.name = this.constructor.name;
    this.statusCode = statusCode;
    this.isOperational = true; // per 03-javascript-for-node/05-error-handling.md — an expected, not-a-bug error
  }
}

export class NotFoundError extends AppError {
  constructor(message = "Resource not found") {
    super(message, 404);
  }
}

export class ConflictError extends AppError {
  constructor(message = "Resource already exists") {
    super(message, 409);
  }
}

export class ValidationFailedError extends AppError {
  constructor(message = "Invalid input", details = {}) {
    super(message, 400);
    this.details = details;
  }
}
```

These carry no Mongoose-specific knowledge at all — they're plain, meaningful application concepts (something wasn't found, something conflicts, input was invalid) that make sense regardless of what database or ODM sits underneath.

---

## Translating Mongoose errors at the boundary

```js
// services/userService.js
import { ConflictError, ValidationFailedError } from "../errors/AppError.js";

export async function registerUser({ name, email, password }) {
  try {
    return await User.create({
      name,
      email,
      password: await hashPassword(password),
    });
  } catch (err) {
    if (err.code === 11000) {
      const field = Object.keys(err.keyValue)[0];
      throw new ConflictError(`${field} is already registered`);
    }
    if (err.name === "ValidationError") {
      const details = Object.fromEntries(
        Object.entries(err.errors).map(([field, e]) => [field, e.message]),
      );
      throw new ValidationFailedError("Invalid signup data", details);
    }
    throw err; // something genuinely unexpected — let it propagate as-is
  }
}
```

The translation from Mongoose's raw errors into your own classes happens **exactly once**, right at the boundary where the database is actually touched — every caller of `registerUser` from this point on only ever needs to know about `ConflictError`/`ValidationFailedError`, regardless of what's actually happening underneath.

---

## Consuming the translated errors, anywhere

```js
// Express route — translates to an HTTP response
app.post("/signup", async (req, res, next) => {
  try {
    const user = await registerUser(req.body);
    res.status(201).json(user);
  } catch (err) {
    next(err);
  }
});
```

```js
// centralized error handler — now works generically, no Mongoose knowledge needed
app.use((err, req, res, next) => {
  if (err instanceof AppError) {
    return res.status(err.statusCode).json({
      error: err.message,
      ...(err.details && { details: err.details }),
    });
  }
  console.error("Unexpected error:", err);
  res.status(500).json({ error: "Internal server error" });
});
```

```js
// a background job — no HTTP at all, but still handles the same error meaningfully
try {
  await registerUser(importedRecord);
} catch (err) {
  if (err instanceof ConflictError) {
    console.log(`Skipping already-registered user: ${importedRecord.email}`);
  } else {
    throw err;
  }
}
```

The exact same `ConflictError` is handled correctly by both an Express route (mapped to a `409` response) and a completely unrelated background job (logged and skipped) — neither needs to know it originated from a MongoDB duplicate-key error at all.

---

## The error-handling middleware gets simpler and more generic

```js
app.use((err, req, res, next) => {
  if (err instanceof AppError) {
    return res.status(err.statusCode).json({ error: err.message });
  }
  console.error(err);
  res.status(500).json({ error: "Internal server error" });
});
```

Compare this to `05-turning-errors-into-friendly-responses.md`'s version, which checked `err.name`/`err.code` for Mongoose-specific shapes directly — once errors are translated at the service boundary, the centralized handler only ever needs to understand your own `AppError` hierarchy, making it entirely independent of whatever data layer sits underneath.

---

## Where to draw this boundary

```
Mongoose-specific errors  →  translated at the service/repository layer  →  your own AppError classes  →  everywhere else
```

The translation point matters: do it in the service/repository layer (`11-patterns-and-architecture/01-repository-and-service-pattern.md`), not scattered across every individual route handler. This is the same principle as `10-architecture`'s general layering guidance from earlier in this documentation set — the database-specific details stay contained in the layer that actually talks to the database, and everything above it works with clean, generic concepts.

## Common mistakes

- **Letting raw Mongoose errors (`err.code === 11000`, `err.name === "ValidationError"`) leak into route handlers, service callers, and background jobs alike** — ties your entire codebase to Mongoose-specific error shapes, making a future change to the data layer far more invasive.
- **Wrapping every error in a generic class with no useful distinction** — defeats the purpose; keep a small, meaningful hierarchy (`NotFoundError`, `ConflictError`, `ValidationFailedError`, etc.) that maps cleanly to real situations.
- **Re-throwing an unrecognized Mongoose error as a generic `AppError`** — losing the original stack trace/details; only translate errors you specifically recognize and know how to represent meaningfully, and let anything else propagate as-is for proper logging.
- **Duplicating the same try/catch-and-translate block across many service functions** — consider a small shared helper if the same Mongoose-error-to-AppError mapping repeats often enough to warrant it.

## Quick summary

- Define a small hierarchy of application-specific error classes (`NotFoundError`, `ConflictError`, `ValidationFailedError`) with no Mongoose-specific knowledge
- Translate Mongoose's raw errors into these classes exactly once, at the service/repository boundary where the database is actually touched
- Every consumer — Express routes, background jobs, CLI scripts — then only needs to understand your own error classes, not Mongoose's error shapes
- This keeps your centralized error-handling middleware simple and generic, and makes the codebase more resilient to ever changing the underlying data layer

## Section complete

That closes out error handling in full depth — every Mongoose error type, reading `ValidationError` and `CastError` structures, identifying duplicate-key fields, mapping everything to clean responses, and wrapping it all in your own error classes. **`09-validation`** goes deeper on preventing bad data in the first place.
