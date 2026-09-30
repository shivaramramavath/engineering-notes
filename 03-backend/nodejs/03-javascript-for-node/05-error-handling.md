# Error Handling

Errors in Node show up in several different shapes depending on whether the failing code is synchronous, callback-based, or Promise-based — and each shape needs handling differently. This file covers the general-purpose JavaScript/Node mechanics; `06-express/04-error-handling.md` covers the Express-specific layer built on top of this, and `09-api-development/05-error-responses.md` covers the API response _format_ for errors.

## `try`/`catch` — synchronous errors

```js
try {
  const data = JSON.parse(invalidJson);
} catch (err) {
  console.error("Failed to parse JSON:", err.message);
}
```

`try/catch` only catches errors thrown **synchronously**, within the `try` block itself — it does not catch errors from an asynchronous callback scheduled inside it.

```js
// ❌ this catch block never runs, even if the callback throws
try {
  setTimeout(() => {
    throw new Error("Oops");
  }, 100);
} catch (err) {
  console.log("This never runs");
}
```

By the time the `setTimeout` callback runs, the surrounding `try` block has already finished executing — there's no `try` "still open" to catch it.

---

## Errors in callbacks: the error-first convention

```js
import fs from "node:fs";

fs.readFile("file.txt", "utf-8", (err, data) => {
  if (err) {
    console.error("Read failed:", err.message);
    return;
  }
  console.log(data);
});
```

Node's convention (`03-javascript-for-node/01-callbacks-promises-async-await.md`) puts the error as the callback's first argument — there's no automatic propagation here; **you** must check `err` and decide what to do, every single time. Forgetting the check is a classic bug: the callback proceeds as if it succeeded, using `data` that's actually `undefined`.

---

## Errors in Promises

```js
fetchUser(id)
  .then((user) => console.log(user))
  .catch((err) => console.error("Failed to fetch user:", err));
```

A single `.catch()` at the end of a chain catches a rejection from **any** step earlier in that chain — a real improvement over needing an error check after every individual callback.

### `try`/`catch` with `async`/`await`

```js
async function loadUser(id) {
  try {
    const user = await fetchUser(id);
    return user;
  } catch (err) {
    console.error("Failed to fetch user:", err);
    throw err; // often worth re-throwing, so the caller also knows it failed
  }
}
```

Since `await` unwraps a Promise's rejection into a regular `throw`, ordinary `try/catch` works around it exactly like synchronous code — this is one of the biggest ergonomic wins `async`/`await` provides over raw `.then()`/`.catch()` chains.

---

## Custom error classes

```js
class ValidationError extends Error {
  constructor(message, field) {
    super(message);
    this.name = "ValidationError";
    this.field = field;
    this.statusCode = 400;
  }
}

class NotFoundError extends Error {
  constructor(message) {
    super(message);
    this.name = "NotFoundError";
    this.statusCode = 404;
  }
}
```

```js
function getUser(id) {
  const user = db.find(id);
  if (!user) {
    throw new NotFoundError(`User ${id} not found`);
  }
  return user;
}
```

Extending `Error` (using the prototype/`class` mechanism from `04-prototypes.md`) lets you attach meaningful, structured information — a status code, a field name, an error category — that generic `Error` objects don't carry. This is the foundation centralized error-handling middleware (`06-express/04-error-handling.md`) relies on to turn a thrown error into the right HTTP response automatically.

### Distinguishing error types

```js
try {
  getUser(id);
} catch (err) {
  if (err instanceof NotFoundError) {
    // handle specifically
  } else if (err instanceof ValidationError) {
    // handle differently
  } else {
    throw err; // unknown error — don't silently swallow it, let it propagate
  }
}
```

`instanceof` (from `04-prototypes.md`) is the standard way to branch behavior based on error type, relying on the same prototype chain mechanism covered there.

---

## Operational errors vs programmer errors

A useful distinction:

- **Operational errors** — expected failure modes: a network timeout, invalid user input, a not-found record. These should be handled gracefully — caught, logged, turned into a sensible response.
- **Programmer errors** — actual bugs: calling a function with the wrong arguments, a `TypeError` from `undefined.someMethod()`. These indicate the program is in an unknown, potentially unsafe state, and are generally _not_ meant to be caught and quietly continued from — better to let the process fail loudly (see `process.on("uncaughtException", ...)` in `02-core-modules/07-process.md`) than to keep running in a broken state.

```js
// operational — expected, handle gracefully
if (!user) {
  throw new NotFoundError("User not found");
}

// programmer error — a real bug, shouldn't be "handled" as if it were normal
function processOrder(order) {
  return order.items.map(...);   // throws if `order` is undefined — that's a bug upstream, not a valid input case
}
```

---

## Never swallow errors silently

```js
// ❌ the error vanishes — good luck debugging this in production
try {
  riskyOperation();
} catch (err) {
  // empty — or worse, just a comment like "// ignore"
}
```

```js
// ✅ at minimum, log it — ideally, handle it meaningfully or re-throw
try {
  riskyOperation();
} catch (err) {
  logger.error(err, "riskyOperation failed");
  throw err; // or handle it if there's a sensible fallback
}
```

An empty catch block is one of the most common ways a real production bug becomes invisible until much later, when its downstream consequences finally surface somewhere confusing.

## Common mistakes

- **Expecting `try/catch` to catch errors from async callbacks** — it only catches synchronous throws within the block; use `.catch()`/`await` + `try/catch` for async code instead.
- **Forgetting the error-first check in a callback** — proceeding with `data` when `err` was actually set.
- **Swallowing errors with an empty `catch` block** — always at least log, ideally handle or re-throw.
- **Treating every error the same way** — a validation error and a database connection failure usually deserve very different handling; custom error classes plus `instanceof` make that distinction possible.
- **Catching programmer errors and continuing as if nothing happened** — masks real bugs rather than surfacing them.

## Quick summary

- `try/catch` only catches synchronous errors directly in its block — async errors need `.catch()` or `await` + `try/catch`
- Error-first callbacks require an explicit `if (err)` check every time — nothing propagates automatically
- Custom error classes (`extends Error`) let you attach structured metadata and branch on error type with `instanceof`
- Distinguish operational errors (expected, handle gracefully) from programmer errors (bugs, better to fail loudly)
- Never leave a `catch` block empty — log at minimum

## Next

**`06-memory-management.md`** covers how Node's garbage collector works and the patterns that commonly cause memory leaks in long-running processes.
