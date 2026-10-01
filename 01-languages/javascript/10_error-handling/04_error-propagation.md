# Error Propagation

**Propagation** is how an error travels from where it happens to where it is handled. Good design decides **where to catch**, **what to add**, and **when to let it bubble**.

```
deep function throws ──► caller ──► caller's caller ──► ... ──► boundary that handles it
```

## The golden rule

> Handle an error where you have enough context to do something useful about it. Otherwise let it propagate.

| Level | Typical responsibility |
|-------|------------------------|
| Low level (helpers, repositories) | detect problems, throw precise errors, do not log and swallow |
| Mid level (services) | add context, translate errors, retry or fall back |
| Boundaries (HTTP handlers, CLI `main`, UI event handlers, job runners) | catch, log once, respond to the user, decide to exit or continue |

## Let it bubble

```js
function readConfig(path) {
  const text = fs.readFileSync(path, "utf8");      // may throw ENOENT: no handling here
  return JSON.parse(text);                         // may throw SyntaxError: no handling here
}

function main() {
  try {
    const config = readConfig("./config.json");
    start(config);
  } catch (err) {
    console.error("Startup failed:", err);
    process.exitCode = 1;
  }
}
```

Only `main` knows the right reaction (print and exit).

## Add context and keep the cause

```js
async function loadDashboard(userId) {
  try {
    const [profile, orders] = await Promise.all([getProfile(userId), getOrders(userId)]);
    return { profile, orders };
  } catch (err) {
    throw new Error(`Failed to load dashboard for user ${userId}`, { cause: err });
  }
}
```

Each layer adds "what I was trying to do". Logs then read like a story, and the original error is always available on `.cause`.

## Translate errors at boundaries

Do not leak low-level details (SQL errors, file paths) across layers.

```js
async function findUser(id) {
  try {
    return await db.query("SELECT * FROM users WHERE id = $1", [id]);
  } catch (err) {
    if (err.code === "ECONNREFUSED") throw new ServiceUnavailableError("Database unavailable", { cause: err });
    throw new InternalError("User lookup failed", { cause: err });
  }
}

function toHttpStatus(err) {
  if (err instanceof ValidationError) return 422;
  if (err instanceof NotFoundError) return 404;
  if (err instanceof AuthError) return 401;
  return 500;
}
```

## Rethrowing

```js
try {
  await work();
} catch (err) {
  metrics.increment("work.failed");                 // side effect
  throw err;                                        // same error object, stack preserved
}
```

- `throw err` keeps the **original stack** (created at construction time)
- `throw new Error(msg, { cause: err })` adds context but creates a new stack
- Do not log **and** rethrow at every level: you will get the same failure logged many times

## Log once, at the boundary

```js
// bad: duplicate noise
async function a() { try { await b(); } catch (e) { logger.error(e); throw e; } }
async function b() { try { await c(); } catch (e) { logger.error(e); throw e; } }

// good: propagate, log at the top
app.use((err, req, res, next) => {
  logger.error({ err, path: req.path });
  res.status(toHttpStatus(err)).json({ error: { code: err.code ?? "INTERNAL" } });
});
```

## Promise propagation

A rejection propagates through `.then` chains until a `.catch`.

```js
fetchUser(1)
  .then(loadOrders)           // skipped if fetchUser rejects
  .then(render)
  .catch(handleError)         // receives errors from ANY step above
  .finally(cleanup);
```

- Returning a value from `.catch` **recovers** the chain
- Throwing inside `.catch` continues the rejection
- Forgetting `return` inside `.then` breaks the chain and its error handling

```js
p.then(() => { doAsync(); });          // BUG: result not returned, rejection is unhandled
p.then(() => doAsync());               // returns the promise: errors propagate
```

## async/await propagation

`await` rethrows rejections as exceptions, so normal `try/catch` and natural bubbling apply.

```js
async function a() { await b(); }       // if b rejects, a rejects
async function main() {
  try { await a(); } catch (err) { report(err); }
}
```

Every `async` function returns a promise: **an unawaited rejection is unhandled**.

```js
saveLogs();                              // floating promise: if it rejects, nobody hears about it
await saveLogs();                        // or: saveLogs().catch(reportError);
void saveLogs().catch(reportError);      // intentional fire-and-forget
```

## Parallel work

```js
await Promise.all([a(), b()]);              // rejects on FIRST failure (others keep running)
await Promise.allSettled([a(), b()]);        // never rejects: inspect each result
await Promise.any([a(), b()]);               // first success, else AggregateError
await Promise.race([a(), timeout(5000)]);    // first to settle
```

Collect every failure:

```js
const results = await Promise.allSettled(jobs.map(run));
const errors = results.flatMap((r) => (r.status === "rejected" ? [r.reason] : []));
if (errors.length) throw new AggregateError(errors, `${errors.length} jobs failed`);
```

## Callbacks and events

```js
// Node error-first callback
fs.readFile(path, (err, data) => {
  if (err) return callback(err);          // propagate by passing it on
  callback(null, JSON.parse(data));       // a throw here is NOT caught by the caller: wrap it
});

// EventEmitter: emitting "error" with no listener throws
emitter.on("error", handleError);

// Streams: use pipeline so errors propagate and resources are cleaned up
import { pipeline } from "node:stream/promises";
await pipeline(input, transform, output);
```

## Errors as values (Result pattern)

Return failures instead of throwing, for **expected** outcomes.

```js
const ok = (value) => ({ ok: true, value });
const fail = (error) => ({ ok: false, error });

function parseAge(input) {
  const n = Number(input);
  return Number.isInteger(n) && n >= 0 ? ok(n) : fail(new ValidationError([{ field: "age", message: "Must be a non-negative integer" }]));
}

const result = parseAge(form.age);
if (!result.ok) return show(result.error);
use(result.value);
```

| Use exceptions for | Use result values for |
|--------------------|-----------------------|
| Unexpected failures, bugs, I/O faults | Expected outcomes (validation, "not found", parse) |
| Deep call stacks where bubbling is convenient | Local, explicit flow |
| Library boundaries with `Promise` semantics | Functional pipelines (`Either`, see the FP chapter) |

## Retry, fallback and circuit breaking

| Strategy | When | Notes |
|----------|------|-------|
| Retry with backoff | transient faults (network, 503) | cap attempts, add jitter, only for **idempotent** operations |
| Fallback value | optional data (recommendations) | log the degradation |
| Timeout / cancel | slow dependencies | `AbortSignal.timeout(ms)` |
| Circuit breaker | repeatedly failing dependency | fail fast, recover after cool-down |
| Compensation | partial multi-step failure | undo previous steps |

```js
const res = await fetch(url, { signal: AbortSignal.timeout(5000) });   // throws TimeoutError (DOMException)
```

## Cancellation is not an error

```js
try {
  await fetch(url, { signal });
} catch (err) {
  if (err.name === "AbortError") return;           // expected, not a failure to report
  throw err;
}
```

## Cleanup when errors propagate

```js
const handle = await open(path);
try {
  return await process(handle);
} finally {
  await handle.close();                            // runs while the error continues upward
}
```

Preserve the original error if cleanup itself can fail:

```js
try { await work(); }
catch (err) {
  try { await cleanup(); } catch (cleanupErr) { throw new AggregateError([err, cleanupErr], "Work and cleanup failed"); }
  throw err;
}
```

## Where to put `try/catch`: examples

| Place | Do |
|-------|----|
| HTTP handler / route wrapper | Catch all, map to status, log, respond |
| UI event handler | Catch, show a friendly message, report |
| Job / queue consumer | Catch per job, retry or dead-letter, continue the loop |
| Library function | Usually **do not** catch; throw documented errors |
| Utility/helper | Only when you can fully recover (fallback) |

## Pitfalls

| Pitfall | Why it hurts | Better |
|---------|--------------|--------|
| Catch, log and continue as if nothing happened | Corrupt state | Rethrow or fail the operation |
| Logging at every layer | Duplicate noise | Log once at the boundary |
| Floating promises | Silent failures | `await`, `.catch`, or explicit `void` with handler |
| Forgetting `return` in `.then` | Broken chain | Return the promise |
| Translating errors without `cause` | Lost root cause | Always pass `{ cause }` |
| Leaking internal details to callers | Coupling, security | Translate at boundaries |
| Using `Promise.all` and ignoring remaining work | Leaks, partial effects | `allSettled`, cancellation |
| Retrying non-idempotent operations | Duplicate effects | Idempotency keys, only safe retries |
| Treating cancellation as failure | Noisy alerts | Check `AbortError` |

## Key takeaways

- Catch where you can act; otherwise propagate
- Add context with `{ cause }`, translate at layer boundaries, log once at the top
- Promises and async functions propagate rejections: await or handle every promise
- Use result values for expected outcomes and exceptions for the unexpected
- Always clean up with `finally`

**Next:** [Production Error Handling](./05_production-error-handling.md)
