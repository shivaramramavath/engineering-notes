# Custom Errors

Custom error classes give your application **named, structured, recognizable** failures. Callers can handle them precisely instead of matching on message text.

## A minimal custom error

```js
class ValidationError extends Error {
  constructor(message, options) {
    super(message, options);             // options: { cause }
    this.name = "ValidationError";
  }
}

try {
  throw new ValidationError("Email is invalid");
} catch (err) {
  err instanceof ValidationError;        // true
  err instanceof Error;                  // true
  err.name;                              // "ValidationError"
  err.stack;                             // starts with "ValidationError: Email is invalid"
}
```

## Set `name` automatically

```js
class AppError extends Error {
  constructor(message, options) {
    super(message, options);
    this.name = new.target.name;         // subclass name (minification can rename classes)
  }
}

class NotFoundError extends AppError {}
new NotFoundError("User 7").name;        // "NotFoundError"
```

`new.target.name` changes under minification. If names must be stable (logging, comparisons), set them explicitly or use a `code`.

## Add structured data

```js
class HttpError extends Error {
  constructor(status, message, { cause, details } = {}) {
    super(message, { cause });
    this.name = "HttpError";
    this.status = status;
    this.details = details;
  }
  get isClientError() { return this.status >= 400 && this.status < 500; }
  get isServerError() { return this.status >= 500; }
}

throw new HttpError(404, "User not found", { details: { id: 7 } });
```

## Error codes (stable identifiers)

Messages change and get translated; **codes** are for programs.

```js
class AppError extends Error {
  constructor(code, message, { cause, meta, status = 500, expose = false } = {}) {
    super(message, { cause });
    this.name = "AppError";
    this.code = code;              // "USER_NOT_FOUND"
    this.meta = meta;              // safe context for logs
    this.status = status;          // HTTP mapping
    this.expose = expose;          // safe to show to users?
  }
}

throw new AppError("USER_NOT_FOUND", "User 7 not found", { status: 404, expose: true, meta: { id: 7 } });
```

Node uses the same idea (`err.code === "ENOENT"`).

## Class hierarchy example

```js
class AppError extends Error { /* base with code, status, meta */ }

class ValidationError extends AppError {
  constructor(errors, message = "Validation failed") {
    super("VALIDATION_FAILED", message, { status: 422, expose: true });
    this.name = "ValidationError";
    this.errors = errors;          // [{ field: "email", message: "Invalid" }]
  }
}
class NotFoundError extends AppError {
  constructor(resource, id) {
    super("NOT_FOUND", `${resource} ${id} not found`, { status: 404, expose: true, meta: { resource, id } });
    this.name = "NotFoundError";
  }
}
class AuthError extends AppError {
  constructor(message = "Authentication required") {
    super("UNAUTHENTICATED", message, { status: 401, expose: true });
    this.name = "AuthError";
  }
}
class ConflictError extends AppError {}
class TimeoutError extends AppError {}
```

Keep the hierarchy shallow: a base class plus a handful of meaningful categories.

## Handling by type

```js
try {
  await createUser(input);
} catch (err) {
  if (err instanceof ValidationError) return showFieldErrors(err.errors);
  if (err instanceof ConflictError) return showMessage("Email already registered");
  if (err.code === "RATE_LIMITED") return retryLater();
  throw err;                                   // unknown: bubble up
}
```

## Serialization

`JSON.stringify(error)` drops `message`, `name` and `stack` because they are not enumerable.

```js
JSON.stringify(new Error("x"));                // "{}"

class AppError extends Error {
  toJSON() {
    return { name: this.name, code: this.code, message: this.message, meta: this.meta };
    // include stack only in non-production logs
  }
}

const serialize = (err) => ({
  name: err.name, message: err.message, code: err.code, stack: err.stack,
  cause: err.cause instanceof Error ? serialize(err.cause) : err.cause,
});
```

Errors sent through `postMessage` or `structuredClone` keep `name`, `message`, `stack` and `cause`, but lose custom class identity and extra properties (they become plain `Error` objects of the built-in type).

## Checks that survive realms and bundlers

```js
err instanceof NotFoundError;                    // fails across realms or duplicated packages
err?.name === "NotFoundError";                   // works, but names can be minified
err?.code === "NOT_FOUND";                       // most robust

class NotFoundError extends AppError {
  static [Symbol.hasInstance](x) { return x?.code === "NOT_FOUND" || Function.prototype[Symbol.hasInstance].call(this, x); }
}
```

Prefer `code` checks for cross-package and cross-service logic.

## Old-style constructor functions (legacy)

```js
function LegacyError(message) {
  const err = new Error(message);
  Object.setPrototypeOf(err, LegacyError.prototype);
  return err;
}
LegacyError.prototype = Object.create(Error.prototype, { constructor: { value: LegacyError }, name: { value: "LegacyError" } });
```

Use `class ... extends Error`.

## Transpiling to ES5

Extending built-ins breaks under old Babel/TypeScript ES5 targets (`instanceof` fails). Target ES2015+ so native `class` semantics apply.

## Stack trace cleanliness (V8)

```js
class AppError extends Error {
  constructor(message) {
    super(message);
    Error.captureStackTrace?.(this, new.target);   // omit the constructor frames
  }
}
```

## Validation errors that list all problems

```js
function validate(user) {
  const errors = [];
  if (!user.email?.includes("@")) errors.push({ field: "email", message: "Must be a valid email" });
  if ((user.password ?? "").length < 8) errors.push({ field: "password", message: "At least 8 characters" });
  if (errors.length) throw new ValidationError(errors);
  return user;
}
```

## When to create a custom error

| Create one when | Skip it when |
|-----------------|--------------|
| Callers must react differently by failure type | A plain `Error` with a clear message is enough |
| You need structured fields (`code`, `status`, `details`) | Only developers will ever read it |
| The error crosses a boundary (API, module, service) | It is a one-off internal guard |
| You want consistent HTTP/log mapping | You would create dozens of near-identical classes |

## Testing custom errors

```js
import { test, expect } from "vitest";

test("rejects with NotFoundError", async () => {
  await expect(getUser(999)).rejects.toBeInstanceOf(NotFoundError);
  await expect(getUser(999)).rejects.toMatchObject({ code: "NOT_FOUND", status: 404 });
});
```

## Pitfalls

| Pitfall | Why it hurts | Better |
|---------|--------------|--------|
| Not setting `name` | Logs show `Error` | Set it in the constructor |
| Matching on `err.message` | Brittle | `instanceof` or `code` |
| Forgetting `{ cause }` | Root cause lost | Pass and preserve it |
| Putting secrets in `meta`/`message` | Leaks to logs or users | Sanitize; use `expose` flags |
| Deep hierarchies | Hard to maintain | Base class plus `code` |
| Sending `err.stack` to clients | Information disclosure | Return `code` + safe message |
| ES5 transpilation | `instanceof` breaks | Target ES2015+ |
| Expecting `JSON.stringify(err)` to work | `{}` | `toJSON` or a serializer |
| `new.target.name` with minification | Names change | Explicit strings or codes |

## Key takeaways

- Extend `Error`, call `super(message, { cause })`, and set `name`
- Add a stable `code` (and optional `status`, `meta`) for programmatic handling
- Keep the hierarchy small; check by `instanceof` or, across boundaries, by `code`
- Provide `toJSON` or a serializer, since errors do not stringify by default

**Next:** [Error Propagation](./04_error-propagation.md)
