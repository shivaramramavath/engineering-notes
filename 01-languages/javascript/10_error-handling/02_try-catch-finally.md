# try, catch, finally

`try...catch...finally` is JavaScript's structured way to handle thrown values.

```js
try {
  riskyOperation();          // code that may throw
} catch (err) {
  handle(err);               // runs only if something was thrown
} finally {
  cleanup();                 // always runs
}
```

## Forms

| Form | Notes |
|------|-------|
| `try { } catch (e) { }` | handle errors |
| `try { } catch { }` | optional catch binding (ES2019) when you do not need the error |
| `try { } finally { }` | cleanup without handling (error keeps propagating) |
| `try { } catch (e) { } finally { }` | all three |

```js
function parse(text) {
  try {
    return JSON.parse(text);
  } catch {
    return null;
  }
}
```

## Execution order

```js
function demo() {
  try {
    console.log("try");
    throw new Error("boom");
    console.log("never");
  } catch (err) {
    console.log("catch", err.message);
    return "from catch";
  } finally {
    console.log("finally");          // runs before the function actually returns
  }
}
demo();   // try, catch boom, finally → "from catch"
```

## `finally` semantics

- Runs after `try` and `catch`, **even with `return`, `break`, `continue` or a thrown error**
- A `return` inside `finally` **overrides** any earlier return or throw (avoid this)
- A `throw` inside `finally` replaces the original error

```js
function tricky() {
  try { return "try"; }
  finally { return "finally"; }      // returns "finally": discouraged
}
```

Good uses:

```js
const conn = await db.connect();
try {
  return await conn.query(sql);
} finally {
  await conn.release();               // always release, success or failure
}

spinner.show();
try { await save(); } finally { spinner.hide(); }

const lock = await acquire();
try { await critical(); } finally { lock.release(); }
```

## The catch parameter

```js
try { ... } catch (err) {
  // err is scoped to the catch block
  // err can be ANY thrown value: Error, string, number, undefined
}
```

Always normalize unknown values:

```js
const toError = (value) => value instanceof Error ? value : new Error(String(value), { cause: value });

try { ... } catch (thrown) {
  const err = toError(thrown);
  log(err);
}
```

## Catching selectively

JavaScript has no typed catch clauses. Check and **rethrow** what you cannot handle.

```js
try {
  await fetchUser(id);
} catch (err) {
  if (err.name === "AbortError") return;              // expected: user canceled
  if (err instanceof NotFoundError) return null;      // expected: missing user
  throw err;                                          // unexpected: let it bubble
}
```

Never swallow errors you do not understand.

## Scope pitfalls

```js
try {
  const value = compute();           // block-scoped
} catch {}
console.log(value);                  // ReferenceError

let value;                           // declare outside when needed after
try { value = compute(); } catch { value = fallback; }
```

## Async errors

`try/catch` catches **synchronous** throws and awaited rejections only.

```js
// works: await turns the rejection into a throw
async function load() {
  try {
    const data = await fetchJson(url);
    return data;
  } catch (err) {
    console.error(err);
  }
}

// does NOT work: callback runs later, outside the try block
try {
  setTimeout(() => { throw new Error("late"); }, 0);
} catch (err) { /* never runs */ }

// does NOT work: promise not awaited
try {
  fetchJson(url);                    // rejection is unhandled
} catch {}

// return vs return await
async function a() { try { return fetchJson(url); } catch {} }         // catch does NOT run for rejection
async function b() { try { return await fetchJson(url); } catch {} }   // catch runs
```

Promise chain style:

```js
fetchJson(url)
  .then(handle)
  .catch((err) => report(err))
  .finally(() => hideSpinner());
```

## Performance

Modern engines optimize `try/catch` well. Throwing is relatively expensive (captures a stack), so do not use exceptions for **ordinary control flow** in hot paths.

```js
// avoid: exceptions as flow control
for (const s of inputs) { try { n = parse(s); } catch { n = 0; } }

// better: validate or return a result
const n = Number.isNaN(Number(s)) ? 0 : Number(s);
```

## Patterns

### Fallback value

```js
const config = (() => { try { return JSON.parse(text); } catch { return {}; } })();
```

### Retry

```js
async function withRetry(fn, { retries = 3, delay = 200 } = {}) {
  let lastError;
  for (let attempt = 0; attempt <= retries; attempt++) {
    try { return await fn(attempt); }
    catch (err) {
      lastError = err;
      if (attempt < retries) await new Promise((r) => setTimeout(r, delay * 2 ** attempt));
    }
  }
  throw new Error(`Failed after ${retries + 1} attempts`, { cause: lastError });
}
```

### Collect multiple errors

```js
const results = await Promise.allSettled(tasks.map((t) => t()));
const failures = results.filter((r) => r.status === "rejected").map((r) => r.reason);
if (failures.length) throw new AggregateError(failures, "Some tasks failed");
```

### Cleanup with resource management (newer)

```js
{
  using file = await openFile(path);     // automatic dispose at scope end (check runtime support)
}
```

## Checking for error conditions without exceptions

| Situation | Non-throwing alternative |
|-----------|--------------------------|
| Parse numbers | `Number.isNaN(Number(s))` |
| Optional values | `?.`, `??` |
| Property existence | `Object.hasOwn`, `in` |
| Safe JSON parse | `try/catch` wrapper once, reuse |
| Search | `find` returns `undefined` |
| URL validity | `URL.canParse(str)` (check support) |

## Pitfalls

| Pitfall | Why it hurts | Better |
|---------|--------------|--------|
| Empty `catch {}` | Silent failures | Handle, log, or rethrow |
| Wrapping huge blocks in `try` | Hides where the error came from | Narrow the `try` to risky calls |
| `return` inside `finally` | Overrides errors and returns | Keep `finally` for cleanup only |
| Forgetting `await` inside `try` | Rejection escapes | `await` (or `return await`) |
| Expecting `try/catch` to catch async callbacks | They run later | Promises, `async/await`, or handle inside the callback |
| Catching everything, then continuing | Corrupt state | Rethrow unexpected errors |
| Using exceptions for normal control flow | Slow, confusing | Return values, validation |
| Assuming `err` is an `Error` | Could be any value | Normalize with `toError` |

## Key takeaways

- `finally` always runs: use it for cleanup, not for returns
- Catch narrowly, handle what you can, rethrow the rest
- `try/catch` only sees synchronous throws and **awaited** rejections
- Do not swallow errors, and do not use exceptions for routine control flow

**Next:** [Custom Errors](./03_custom-errors.md)
