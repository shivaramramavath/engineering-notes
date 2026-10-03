# Error Types

Error handling is where untyped JavaScript gets messy fastest: anything can be thrown (strings, objects, `undefined`), so every `catch` block starts with uncertainty. TypeScript gives you tools to bring order — typed custom error classes, safe narrowing in `catch`, and a centralized handler that knows exactly what it's dealing with. This builds on the Express pattern in `06-express/04-error-handling.md`.

## `catch` gives you `unknown`

```ts
try {
  await doSomething();
} catch (err) {
  err.message; // ❌ Error: 'err' is of type 'unknown'
}
```

With `strict` on (specifically `useUnknownInCatchVariables`), the caught value is `unknown` — and that's **correct**, because JavaScript lets you `throw "oops"` or `throw { code: 1 }`. You must narrow before use:

```ts
catch (err) {
  if (err instanceof Error) {
    console.error(err.message, err.stack);
  } else {
    console.error("Non-Error thrown:", err);
  }
}
```

Don't "fix" this by typing `catch (err: any)`. Narrow instead.

---

## A custom error hierarchy

A base class carrying an HTTP status code and a machine-readable code, with specific subclasses:

```ts
// errors/AppError.ts
export class AppError extends Error {
  constructor(
    message: string,
    public readonly statusCode: number = 500,
    public readonly code: string = "INTERNAL_ERROR",
    public readonly isOperational: boolean = true,
    public readonly details?: unknown
  ) {
    super(message);
    this.name = new.target.name;
    Object.setPrototypeOf(this, new.target.prototype);
    Error.captureStackTrace?.(this, this.constructor);
  }
}

export class NotFoundError extends AppError {
  constructor(resource = "Resource") {
    super(`${resource} not found`, 404, "NOT_FOUND");
  }
}

export class ValidationError extends AppError {
  constructor(details: unknown) {
    super("Validation failed", 400, "VALIDATION_ERROR", true, details);
  }
}

export class UnauthorizedError extends AppError {
  constructor(message = "Unauthorized") {
    super(message, 401, "UNAUTHORIZED");
  }
}

export class ForbiddenError extends AppError {
  constructor(message = "Forbidden") {
    super(message, 403, "FORBIDDEN");
  }
}

export class ConflictError extends AppError {
  constructor(message = "Conflict") {
    super(message, 409, "CONFLICT");
  }
}
```

Details worth understanding:

- **`public readonly` in the constructor** — TypeScript shorthand that declares *and* assigns the property in one go
- **`Object.setPrototypeOf(...)`** — needed when compiling to older targets (below ES2015), where extending built-ins breaks `instanceof`. Harmless on modern targets, so it's a safe habit.
- **`new.target.name`** — sets `err.name` to the actual subclass (`"NotFoundError"`), which makes logs readable
- **`isOperational`** — distinguishes *expected* failures (bad input, missing record) from *bugs* (null dereference). Operational errors get a friendly response; bugs get logged loudly and, depending on severity, may warrant a restart (see `16-production/02-graceful-shutdown-and-health-checks.md`).

Using them from a service (the same `ConflictError` referenced in `06-express/03-controllers.md`):

```ts
export async function registerUser(name: string, email: string) {
  const existing = await User.findOne({ email });
  if (existing) throw new ConflictError("Email already in use");
  return User.create({ name, email });
}
```

The service throws; it knows nothing about HTTP responses. The status code travels *with* the error.

---

## A type guard for `AppError`

`instanceof` already narrows, but a named guard keeps handler code readable and works across module boundaries:

```ts
export function isAppError(err: unknown): err is AppError {
  return err instanceof AppError;
}
```

`err is AppError` is a **type predicate** — when the function returns `true`, TypeScript narrows `err` to `AppError` in that branch.

For errors from other libraries, write guards that check shape:

```ts
interface MongoDuplicateKeyError extends Error {
  code: 11000;
  keyValue: Record<string, unknown>;
}

function isMongoDuplicateKey(err: unknown): err is MongoDuplicateKeyError {
  return err instanceof Error && "code" in err && (err as { code?: unknown }).code === 11000;
}
```

---

## Typed centralized error-handling middleware

```ts
// middleware/errorHandler.ts
import { ErrorRequestHandler } from "express";
import { ZodError } from "zod";
import { isAppError, ValidationError } from "../errors/AppError.js";
import { logger } from "../lib/logger.js";

interface ErrorResponse {
  success: false;
  error: {
    code: string;
    message: string;
    details?: unknown;
    requestId?: string;
  };
}

export const errorHandler: ErrorRequestHandler = (err: unknown, req, res, _next) => {
  // 1. Known, operational errors
  if (isAppError(err)) {
    const body: ErrorResponse = {
      success: false,
      error: {
        code: err.code,
        message: err.message,
        details: err.details,
        requestId: req.requestId,
      },
    };
    return res.status(err.statusCode).json(body);
  }

  // 2. Library errors translated into our own types
  if (err instanceof ZodError) {
    const appErr = new ValidationError(err.flatten());
    return res.status(appErr.statusCode).json({
      success: false,
      error: { code: appErr.code, message: appErr.message, details: appErr.details },
    } satisfies ErrorResponse);
  }

  // 3. Unknown errors = probably bugs: log everything, reveal nothing
  logger.error({ err, requestId: req.requestId }, "Unhandled error");

  const body: ErrorResponse = {
    success: false,
    error: {
      code: "INTERNAL_ERROR",
      message: "Something went wrong",
      requestId: req.requestId,
    },
  };
  res.status(500).json(body);
};
```

Notes:

- The handler takes `err: unknown` and **narrows** it — three clear branches instead of poking at properties on `any`
- Unknown errors never leak `err.message` or stack traces to the client (`08-authentication-security/05-common-vulnerabilities.md`)
- The response shape is an `interface`, so every path returns the same structure (`09-api-development/05-error-responses.md`)
- The `_next` underscore tells linters it's intentionally unused, but it must still be **declared** so Express sees four arguments

Registered **last**:

```ts
app.use("/users", userRouter);
// ...all routes...
app.use(notFoundHandler);   // turns unmatched routes into NotFoundError
app.use(errorHandler);      // must be last
```

---

## Typed results instead of exceptions (optional)

Exceptions suit *exceptional* failures. For expected outcomes in business logic, a discriminated-union result makes failure part of the type so callers can't forget to handle it:

```ts
type Result<T, E = AppError> =
  | { ok: true; value: T }
  | { ok: false; error: E };

async function findUser(id: string): Promise<Result<User, NotFoundError>> {
  const user = await User.findById(id);
  return user ? { ok: true, value: user } : { ok: false, error: new NotFoundError("User") };
}

const result = await findUser(id);
if (!result.ok) return next(result.error);
result.value.name; // narrowed to User
```

Neither style is "right" — throwing plus a central handler is the Express norm and usually enough. Use `Result` where a failure is routine and callers genuinely must branch on it.

---

## Process-level errors

Errors that escape every handler land on `process` events, whose arguments are also loosely typed:

```ts
process.on("unhandledRejection", (reason: unknown) => {
  logger.fatal({ reason }, "Unhandled promise rejection");
  process.exit(1);
});

process.on("uncaughtException", (err: Error) => {
  logger.fatal({ err }, "Uncaught exception");
  process.exit(1);
});
```

After an `uncaughtException` the process state is undefined — log, then exit and let the supervisor (Docker, PM2, Kubernetes) restart it. See `03-javascript-for-node/05-error-handling.md` and `16-production/02-graceful-shutdown-and-health-checks.md`.

---

## Common mistakes

- **`catch (err: any)`** — silences the compiler and invites `err.message` on things that aren't errors. Keep `unknown` and narrow.
- **Throwing strings or plain objects** — no stack trace, can't use `instanceof`. Always throw `Error` (or a subclass).
- **Subclassing `Error` without `Object.setPrototypeOf` on old targets** — `instanceof` mysteriously returns `false`.
- **Dropping the fourth parameter in error middleware** — Express won't treat it as an error handler.
- **Returning raw `err.message` for unknown errors** — leaks internals. Return a generic message and log the details server-side.
- **Putting HTTP concerns (`res.status`) inside services** — throw a typed error and let the central handler translate it.

## Quick summary

- `catch` variables are `unknown` under `strict` — narrow with `instanceof` or type guards
- Build an `AppError` base class carrying `statusCode`, `code`, and an `isOperational` flag; subclass it per failure type
- Type guards (`err is AppError`) keep handler logic readable
- One centralized `ErrorRequestHandler` narrows `unknown` errors and always returns the same typed response shape
- Operational errors → friendly response; unknown errors → log in full, respond generically
- `Result<T, E>` unions are an optional alternative for expected failures

## Next

**`05-production-config.md`** covers the strictest `tsconfig` settings, build pipeline, path aliases, linting, and shipping compiled TypeScript in Docker.
