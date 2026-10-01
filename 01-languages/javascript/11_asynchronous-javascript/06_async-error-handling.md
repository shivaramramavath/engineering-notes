# Async Error Handling

Errors in asynchronous code do not travel up the call stack the way synchronous exceptions do. This file collects the rules and traps specific to **promises and `async`/`await`**. (General error design is in [Error Handling](../10_error-handling/00_README.md).)

## Rule 1: rejections need handlers

A rejected promise nobody handles becomes an **unhandled rejection**.

```js
async function risky() { throw new Error("boom"); }

risky();                          // floating promise: unhandled rejection
await risky();                    // throws here: catchable
risky().catch(report);            // handled
```

Every promise should be one of: **awaited**, **returned**, or **explicitly handled**.

## try/catch with await

```js
async function loadUser(id) {
  try {
    const res = await fetch(`/api/users/${id}`);
    if (!res.ok) throw new HttpError(res.status, res.statusText);   // fetch only rejects on network errors
    return await res.json();
  } catch (err) {
    if (err.name === "AbortError") return null;       // expected
    throw new Error(`Cannot load user ${id}`, { cause: err });
  }
}
```

`await` converts rejections into exceptions, so `try/catch/finally` work normally.

## Promise-chain style

```js
fetchUser(id)
  .then(render)
  .catch((err) => showError(err))     // handles errors from fetchUser AND render
  .finally(() => hideSpinner());
```

A `.catch` placed in the middle **recovers** the chain; later `.then`s run with the recovered value.

## What try/catch does NOT catch

```js
// 1. un-awaited promises
try { fetchUser(); } catch { }                 // rejection escapes

// 2. callbacks that run later
try { setTimeout(() => { throw new Error("late"); }); } catch { }

// 3. events
try { emitter.emit("data"); } catch { }        // sync listeners yes; async listeners no

// 4. inside non-awaited Promise.all results
try { Promise.all([a(), b()]); } catch { }     // not awaited
```

Fix: `await` the promise, or handle inside the callback.

## Floating promises

```js
async function handler(req) {
  logAnalytics(req);                // returns a promise, nobody awaits it: failures vanish
  await doWork(req);
}

// intentional fire-and-forget with handling
logAnalytics(req).catch((err) => logger.warn({ err }, "analytics failed"));
void logAnalytics(req).catch(noop);
```

Enable lint rules: `@typescript-eslint/no-floating-promises` and `no-misused-promises`.

## Async functions in places that ignore promises

| Place | Problem | Solution |
|-------|---------|----------|
| `addEventListener("click", async () => {})` | Errors become unhandled rejections | `try/catch` inside |
| `array.forEach(async ...)` | Not awaited | `for...of` / `Promise.all` |
| `new Promise(async (resolve) => {...})` | Throws inside are lost | Don't use an async executor |
| `setTimeout(async () => {})` | Unhandled | `.catch` inside |
| `process.on("x", async ...)` | Unhandled | catch inside |
| Express 4 route `async (req, res) => {}` | Rejection not forwarded | wrapper that calls `next(err)` (built in on Express 5) |

```js
// anti-pattern
new Promise(async (resolve, reject) => {
  const data = await load();        // if this throws, the outer promise never settles
  resolve(data);
});

// correct: just use the async function
const data = await load();
```

## Unhandled rejection events

```js
// Browser
window.addEventListener("unhandledrejection", (e) => { report(e.reason); e.preventDefault(); });
window.addEventListener("rejectionhandled", (e) => { /* handled later */ });

// Node (15+: crashes by default)
process.on("unhandledRejection", (reason) => { logger.error({ err: reason }); throw reason; });
```

Use these as a **safety net** for logging, not as the primary handling mechanism.

## Parallel errors

```js
// Promise.all: first rejection wins, the rest run on silently
try {
  const [a, b] = await Promise.all([getA(), getB()]);
} catch (err) { /* only the first failure */ }

// Collect all failures
const results = await Promise.allSettled([getA(), getB()]);
const errors = results.filter((r) => r.status === "rejected").map((r) => r.reason);

// Promise.any: AggregateError when all fail
try { await Promise.any([...]); } catch (e) { e.errors.forEach(report); }
```

Avoid unhandled rejections from the "other" promises when you stop awaiting early: `Promise.all` and `race` attach handlers to all inputs, so they are not reported twice, but their work continues. Use `AbortController` to stop it.

## Errors in finally

```js
try { await work(); }
finally { await cleanup(); }     // if cleanup throws, it REPLACES the original error
```

Protect cleanup:

```js
try { await work(); }
finally { await cleanup().catch((e) => logger.warn({ err: e }, "cleanup failed")); }
```

## Rethrow, wrap, translate

```js
catch (err) {
  throw err;                                            // keep the original stack
  throw new ServiceError("lookup failed", { cause: err });   // add context
}
```

Do not log and rethrow at every level; log once at the boundary.

## Timeouts and cancellation errors

```js
try {
  await fetch(url, { signal: AbortSignal.timeout(3000) });
} catch (err) {
  if (err.name === "TimeoutError") return fallback();    // signal timeout
  if (err.name === "AbortError") return;                 // user canceled
  throw err;
}
```

## Retry with error filtering

```js
const retryable = (err) => err.name === "TypeError" /* network */ || [502, 503, 504].includes(err.status);

async function withRetry(fn, { attempts = 3, baseMs = 200 } = {}) {
  for (let i = 0; ; i++) {
    try { return await fn(i); }
    catch (err) {
      if (i >= attempts - 1 || !retryable(err)) throw err;
      await sleep(baseMs * 2 ** i + Math.random() * 100);   // exponential backoff + jitter
    }
  }
}
```

Only retry **idempotent** operations (GET, PUT with a key). See [patterns](./09_async-patterns.md).

## Results instead of exceptions

```js
async function safe(promise) {
  try { return [null, await promise]; } catch (err) { return [err, null]; }
}

const [err, user] = await safe(getUser(1));
if (err) return handle(err);
```

Fine for localized flows; do not hide errors that should bubble.

## Debugging async errors

- Enable **async stack traces** in DevTools; name your functions
- Use `Error.cause` chains in logs
- Add context (ids, parameters) when wrapping
- In Node: `node --trace-warnings`, `--unhandled-rejections=strict|throw|warn`
- Test failure paths explicitly

```js
await expect(loadUser(0)).rejects.toThrow("Cannot load user 0");
```

## Pitfalls

| Pitfall | Why it hurts | Better |
|---------|--------------|--------|
| Not awaiting or handling a promise | Unhandled rejection | `await`, return, or `.catch` |
| `try/catch` around a non-awaited call | Never triggers | `await` inside the try |
| `return promise` inside try/catch | catch skipped | `return await` |
| `async` executor in `new Promise` | Lost errors, hanging promise | Plain async function |
| Treating HTTP 4xx/5xx as success (`fetch`) | Silent wrong data | Check `res.ok` |
| `Promise.all` with ignored extra failures | Missing diagnostics | `allSettled`, logging |
| `finally` throwing | Masks the real error | Guard cleanup |
| Logging at every layer | Duplicate noise | Log at the boundary |
| Retrying non-idempotent calls | Duplicate side effects | Idempotency keys |
| Using global handlers as the primary strategy | Lost context | Handle where you have context |

## Key takeaways

- Every promise must be awaited, returned, or explicitly handled
- `try/catch` only catches awaited rejections: mind `return await` and callbacks
- Async callbacks in event handlers and `forEach` need their own error handling
- Use `allSettled`, `AggregateError`, `cause`, and abort/timeout errors deliberately

**Next:** [Async Iterators and Generators](./07_async-iterators-and-generators.md)
