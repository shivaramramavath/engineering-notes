# Promise Combinators

Static methods that combine many promises into one. All of them accept any **iterable** of promises or plain values (non-promises are treated as already fulfilled).

## Comparison

| Method | Fulfills when | Rejects when | Result |
|--------|---------------|--------------|--------|
| `Promise.all` | **all** fulfill | **first** rejection | array of values (input order) |
| `Promise.allSettled` | **all** settle | never | array of `{ status, value / reason }` |
| `Promise.race` | **first** settles fulfilled | first settles rejected | first outcome |
| `Promise.any` | **first** fulfills | **all** reject (`AggregateError`) | first value |

Empty-input behavior:

| Call | Result |
|------|--------|
| `Promise.all([])` | fulfills with `[]` |
| `Promise.allSettled([])` | fulfills with `[]` |
| `Promise.race([])` | **pending forever** |
| `Promise.any([])` | rejects with `AggregateError` |

## Promise.all: everything or nothing

```js
const [user, orders, settings] = await Promise.all([
  getUser(id),
  getOrders(id),
  getSettings(id),
]);
```

- Runs all operations **in parallel** (they start when the promises are created)
- Rejects immediately on the first rejection; other promises keep running but their results are ignored
- Preserves **input order**, not completion order

```js
const users = await Promise.all(ids.map((id) => fetchUser(id)));   // map to promises, then all
```

Parallel with a failure fallback:

```js
const [a, b] = await Promise.all([
  getA(),
  getB().catch(() => null),            // optional: do not fail the whole group
]);
```

## Promise.allSettled: collect every outcome

```js
const results = await Promise.allSettled([getA(), getB(), getC()]);
// [{ status: "fulfilled", value: 1 }, { status: "rejected", reason: Error }, ...]

const values = results.filter((r) => r.status === "fulfilled").map((r) => r.value);
const errors = results.filter((r) => r.status === "rejected").map((r) => r.reason);
```

Use for batch jobs, dashboards with independent widgets, and cleanup tasks where you want to know everything that happened.

## Promise.race: first to settle

```js
const timeout = (ms) => new Promise((_, reject) => setTimeout(() => reject(new Error("timeout")), ms));
const data = await Promise.race([fetchData(), timeout(3000)]);
```

Caution: the losing promise keeps running. Prefer `AbortSignal.timeout(ms)` so the work actually stops:

```js
const res = await fetch(url, { signal: AbortSignal.timeout(3000) });
```

Also note a rejected loser is handled by `race`, so it will not cause an unhandled rejection.

## Promise.any: first success

```js
try {
  const fastest = await Promise.any([fetch(mirror1), fetch(mirror2), fetch(mirror3)]);
} catch (err) {
  err instanceof AggregateError;       // true: every attempt failed
  err.errors;                          // array of reasons
}
```

Use for redundant sources (mirrors, cache vs network, multiple endpoints).

## Promise.withResolvers (ES2024)

Creates a promise together with its `resolve` and `reject`, avoiding the executor boilerplate.

```js
const { promise, resolve, reject } = Promise.withResolvers();

button.addEventListener("click", () => resolve("clicked"), { once: true });
setTimeout(() => reject(new Error("no click")), 10_000);
await promise;
```

Old pattern (a "deferred"):

```js
let resolve, reject;
const promise = new Promise((res, rej) => { resolve = res; reject = rej; });
```

Use sparingly: exposed resolvers make control flow harder to follow. Good for turning events or callbacks into awaitable values.

## Promise.try (ES2025, check support)

Runs a function and always returns a promise, whether it returns a value, a promise, or throws synchronously.

```js
Promise.try(() => parseConfig(text)).then(use).catch(handle);
```

## Array.fromAsync (ES2024)

Collects an async iterable (or iterable of promises) into an array.

```js
const lines = await Array.fromAsync(readLines(file));
const values = await Array.fromAsync([p1, p2, p3]);
```

## Sequential vs parallel

```js
// parallel: total time = slowest
const results = await Promise.all(items.map(process));

// sequential: total time = sum, preserves order of side effects
const out = [];
for (const item of items) out.push(await process(item));

// sequential reduce (older style)
await items.reduce((p, item) => p.then(() => process(item)), Promise.resolve());
```

Parallel is not always right: it can overload servers and rate limits. See [concurrency limits](./09_async-patterns.md).

## Practical recipes

```js
// Timeout wrapper that also cancels
const withTimeout = (fn, ms) => fn(AbortSignal.timeout(ms));

// Fetch several, tolerate failures
const settled = await Promise.allSettled(urls.map((u) => fetch(u).then((r) => r.json())));
const ok = settled.flatMap((r) => (r.status === "fulfilled" ? [r.value] : []));

// First non-null result
const firstHit = await Promise.any(sources.map(async (s) => {
  const v = await s.lookup(key);
  if (v == null) throw new Error("miss");
  return v;
}));

// Wait for all, then throw aggregate
const rs = await Promise.allSettled(tasks.map((t) => t()));
const failed = rs.filter((r) => r.status === "rejected");
if (failed.length) throw new AggregateError(failed.map((r) => r.reason), "Some tasks failed");
```

## Choosing a combinator

| I need... | Use |
|-----------|-----|
| All results, fail fast | `Promise.all` |
| All outcomes regardless of failure | `Promise.allSettled` |
| The first result, success or failure | `Promise.race` |
| The first **successful** result | `Promise.any` |
| To resolve a promise from outside | `Promise.withResolvers` |
| One result per item with bounded parallelism | a pool (next chapters' patterns) |

## Pitfalls

| Pitfall | Why it hurts | Better |
|---------|--------------|--------|
| `await` inside `map` without `Promise.all` | Array of promises, not values | `await Promise.all(items.map(...))` |
| Starting all work at once for huge lists | Overload, rate limits | Concurrency limit |
| Assuming `Promise.all` cancels the others on failure | They keep running | `AbortController` |
| `race` with a timeout that does not cancel work | Resource leak | `AbortSignal.timeout` |
| `Promise.race([])` | Never settles | Guard empty input |
| Using `all` when one failure should not matter | Whole batch fails | `allSettled` or per-item `.catch` |
| Ignoring `AggregateError` from `any` | Lost reasons | Read `err.errors` |
| Results order assumed by completion time | `all` preserves input order | Sort by timestamps if needed |

## Key takeaways

- `all` = everything or fail fast, `allSettled` = every outcome, `race` = first settled, `any` = first success
- Order of results follows input order for `all` and `allSettled`
- Combinators do not cancel losers: use `AbortSignal`
- Limit concurrency for large batches

**Next:** [async/await](./05_async-await.md)
