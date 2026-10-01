# async/await

`async`/`await` (ES2017) is syntax over promises that lets asynchronous code **read like synchronous code**.

```js
async function loadProfile(id) {
  const res = await fetch(`/api/users/${id}`);
  const user = await res.json();
  return user.name;
}

const name = await loadProfile(1);
```

## The two keywords

| Keyword | Meaning |
|---------|---------|
| `async` before a function | the function **always returns a promise**; its `return` value fulfills it, a `throw` rejects it |
| `await expr` | pauses **this async function** until `expr` settles; yields the fulfilled value or throws the rejection |

```js
async function a() { return 1; }          // Promise<1>
async function b() { throw new Error("x"); }   // Promise rejected with Error
a().then(console.log);
```

`await` works on any value: a promise, a thenable, or a plain value (wrapped with `Promise.resolve`).

```js
const n = await 5;                        // 5, but still yields to the microtask queue
```

## Where you can use them

```js
async function declaration() {}
const expression = async function () {};
const arrow = async () => {};
const obj = { async method() {} };
class C { async method() {} static async create() {} }
async function* asyncGenerator() { yield await getOne(); }

// top-level await (ES modules only)
const config = await loadConfig();
```

`await` is **not allowed** in non-async functions or classic scripts / CommonJS top level (`SyntaxError`). Array callbacks need their own `async`.

## What happens at an `await`

```js
console.log("1");
(async () => {
  console.log("2");
  await null;                     // pause; the rest becomes a microtask continuation
  console.log("4");
})();
console.log("3");
// 1, 2, 3, 4
```

- Code **before** the first `await` runs synchronously
- The caller continues immediately after the `await` suspends the function
- Nothing is blocked: the thread is free to run other tasks

## Sequential vs parallel

```js
// sequential: ~ a + b + c time
const a = await getA();
const b = await getB();
const c = await getC();

// parallel: ~ max(a, b, c) time
const [a2, b2, c2] = await Promise.all([getA(), getB(), getC()]);

// start early, await later
const pA = getA();                 // starts now
const pB = getB();                 // starts now
const a3 = await pA;
const b3 = await pB;
```

Only await sequentially when a step **depends** on the previous result (or order of side effects matters).

## Loops

```js
// sequential: one at a time, in order
for (const id of ids) {
  await save(id);
}

// parallel
await Promise.all(ids.map((id) => save(id)));

// forEach: does NOT wait (callback's promises are ignored)
ids.forEach(async (id) => { await save(id); });   // BUG

// reduce: sequential, older style
await ids.reduce((p, id) => p.then(() => save(id)), Promise.resolve());

// filter with an async predicate
const flags = await Promise.all(items.map(isValid));
const valid = items.filter((_, i) => flags[i]);
```

`map`, `filter`, `forEach`, `reduce` do not understand promises. Use `for...of` or `Promise.all`.

## return vs return await

```js
async function a() {
  try { return fetchJson(); }          // returns the promise: catch does NOT run for its rejection
  catch { return null; }
}
async function b() {
  try { return await fetchJson(); }    // awaited inside try: catch handles rejection
  catch { return null; }
}
```

Outside `try/catch`, `return promise` and `return await promise` behave the same (the latter adds a stack frame to traces, which can help debugging).

## Error handling

```js
async function load() {
  try {
    const data = await riskyFetch();
    return process(data);
  } catch (err) {
    console.error("failed", err);
    throw err;                          // or return a fallback
  } finally {
    hideSpinner();
  }
}

load().catch(handleTopLevel);           // an async function's rejection still needs a handler
```

More in [Async Error Handling](./06_async-error-handling.md).

## Top-level await

In ES modules (`.mjs`, `"type": "module"`, `<script type="module">`):

```js
const response = await fetch("/data.json");
export const data = await response.json();
```

Importers **wait** for the module to finish evaluating, so slow top-level awaits delay startup. Use for config loading and optional dynamic imports; avoid long operations.

```js
const mod = await import("./heavy.js");
```

## Await and `this`

```js
class Api {
  base = "/api";
  async get(path) {
    const res = await fetch(this.base + path);   // `this` preserved across await
    return res.json();
  }
}
```

## Async arrow in event handlers

```js
button.addEventListener("click", async () => {
  button.disabled = true;
  try { await save(); } finally { button.disabled = false; }
});
```

The event system ignores the returned promise, so **handle errors inside**.

## Awaiting multiple things with different handling

```js
const results = await Promise.allSettled([a(), b()]);
const [first, second] = await Promise.all([a().catch(() => null), b()]);
```

## Ordering relative to other tasks

```js
setTimeout(() => console.log("timeout"), 0);
(async () => {
  await Promise.resolve();
  console.log("after await");          // microtask: before the timeout
})();
// after await, timeout
```

## Common patterns

```js
// sleep
const sleep = (ms) => new Promise((r) => setTimeout(r, ms));
await sleep(500);

// timeout
const res = await fetch(url, { signal: AbortSignal.timeout(5000) });

// fallback chain
const value = (await getFromCache(key)) ?? (await getFromDb(key));

// IIFE for scripts without top-level await
(async () => { await main(); })().catch((e) => { console.error(e); process.exitCode = 1; });

// run something on all, but one at a time with a delay
for (const job of jobs) { await run(job); await sleep(100); }
```

## async functions and stack traces

Modern engines keep **async stack traces**, showing awaited callers (`at async main`). Naming functions (instead of anonymous arrows) improves them.

## Performance notes

- Each `await` costs a microtask turn: negligible, but avoid awaiting in tight CPU loops
- Do not `await` values you do not need (`return promise` directly where no handling is needed)
- Parallelize independent work with `Promise.all`
- Starting thousands of promises at once can exhaust memory or rate limits: use a pool

## Pitfalls

| Pitfall | Why it hurts | Better |
|---------|--------------|--------|
| Awaiting independent calls sequentially | Slow | `Promise.all` |
| `forEach(async ...)` | Not awaited | `for...of` or `Promise.all(map)` |
| Forgetting `await` | Promise used as value, unhandled rejections | Lint `no-floating-promises` |
| `await` inside a non-async function | `SyntaxError` | Make it `async` |
| `return promise` inside `try` expecting `catch` | Rejection escapes | `return await` |
| Unhandled rejection from async event handlers | Crashes or silent failure | `try/catch` inside the handler |
| Blocking top-level await in shared modules | Delays all importers | Keep it short or lazy-load |
| Using `async` for purely synchronous functions | Unneeded promise wrapping | Plain function |
| Mixing `.then` chains and `await` randomly | Hard to read | Choose one style per function |

## Key takeaways

- `async` functions return promises; `await` unwraps them and rethrows rejections
- Code before the first `await` is synchronous
- Use `Promise.all` for parallel work and `for...of` for sequential work
- Handle errors with `try/catch` and never leave promises floating

**Next:** [Async Error Handling](./06_async-error-handling.md)
