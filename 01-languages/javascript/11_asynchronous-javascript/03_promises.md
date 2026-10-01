# Promises

A **promise** is an object representing the **eventual result** (or failure) of an asynchronous operation. It is a placeholder you can attach handlers to.

```
pending ──► fulfilled (with a value)
        └─► rejected  (with a reason)       settled = fulfilled or rejected, and it never changes again
```

## Creating a promise

```js
const wait = (ms) => new Promise((resolve, reject) => {
  if (ms < 0) return reject(new RangeError("ms must be >= 0"));
  setTimeout(resolve, ms);
});

new Promise((resolve, reject) => {
  // executor runs SYNCHRONOUSLY, immediately
  resolve("value");          // fulfill
  reject(new Error("x"));    // ignored: already settled
});
```

- Calling `resolve`/`reject` more than once has no effect
- A throw inside the executor rejects the promise
- `resolve(otherPromise)` makes this promise **follow** the other one

```js
Promise.resolve(42);                 // already fulfilled
Promise.reject(new Error("no"));     // already rejected
Promise.resolve(promise);            // returns the same promise if it is a native promise
```

## Consuming: `then`, `catch`, `finally`

```js
fetch("/api/user")
  .then((res) => res.json())              // runs on fulfillment
  .then((user) => console.log(user.name))
  .catch((err) => console.error(err))     // runs on rejection from ANY step above
  .finally(() => hideSpinner());          // runs either way, receives no argument
```

| Method | Runs when | Return value effect |
|--------|-----------|--------------------|
| `then(onOk, onFail?)` | fulfilled (or rejected via second arg) | new promise of the handler's result |
| `catch(onFail)` | rejected | same as `then(undefined, onFail)` |
| `finally(fn)` | settled | passes through the original result unless `fn` throws or returns a rejected promise |

## Chaining rules

`then` always returns a **new promise**, resolved with whatever the handler returns:

| Handler returns | Next promise |
|-----------------|--------------|
| a value | fulfilled with that value |
| a promise | **adopts** that promise's outcome |
| nothing | fulfilled with `undefined` |
| throws | rejected with the thrown value |

```js
Promise.resolve(1)
  .then((n) => n + 1)                       // 2
  .then((n) => Promise.resolve(n * 10))     // 20 (flattened)
  .then((n) => { throw new Error(`bad ${n}`); })
  .then(() => console.log("skipped"))
  .catch((err) => err.message)              // recovers: "bad 20"
  .then((msg) => console.log(msg));
```

`catch` **recovers**: whatever it returns becomes the new fulfilled value. Rethrow to keep failing.

## Handlers always run asynchronously

```js
console.log("1");
Promise.resolve().then(() => console.log("3"));
console.log("2");
// 1, 2, 3
```

Handlers are queued as **microtasks**, so they run after the current synchronous code but before timers and I/O.

## Common mistake: forgetting to return

```js
getUser()
  .then((user) => { getOrders(user.id); })      // BUG: promise not returned
  .then((orders) => console.log(orders));       // undefined, and errors in getOrders are lost

getUser()
  .then((user) => getOrders(user.id))           // returns the promise
  .then((orders) => console.log(orders));
```

## Common mistake: nesting (promise hell)

```js
getUser().then((user) => {
  getOrders(user.id).then((orders) => {         // nested: loses flat chain benefits
    ...
  });
});
```

Flatten by returning. Nest only when you need access to earlier values (or use `async`/`await`).

## Thenables

Any object with a `then` method is treated as a promise by `resolve` and `await`.

```js
const thenable = { then(resolve) { resolve("from thenable"); } };
await thenable;                       // "from thenable"
Promise.resolve(thenable);
```

Libraries (older jQuery, Mongoose) return thenables; wrap with `Promise.resolve()` to normalize.

## Static helpers

| Method | Use |
|--------|-----|
| `Promise.resolve(x)` / `Promise.reject(e)` | create settled promises |
| `Promise.all` / `allSettled` / `race` / `any` | combine promises (next file) |
| `Promise.withResolvers()` | create a promise with exposed `resolve`/`reject` (ES2024) |
| `Promise.try(fn)` | start a chain from a function that may throw or return sync/async (ES2025) |

```js
Promise.resolve().then(maybeSyncOrAsync);       // old way to unify
Promise.try(() => maybeSyncOrAsync());          // newer, runs synchronously and catches throws (check support)
```

## Promisifying

```js
const readFileP = (path) => new Promise((resolve, reject) => {
  fs.readFile(path, "utf8", (err, data) => (err ? reject(err) : resolve(data)));
});
```

Prefer built-in promise APIs (`fs/promises`, `fetch`, `timers/promises`).

## Sharing results

A promise runs its executor **once**; every `then` gets the same settled value. Great for caching in-flight work:

```js
let configPromise;
const getConfig = () => (configPromise ??= fetch("/config").then((r) => r.json()));
```

## Unhandled rejections

A rejected promise with no handler eventually triggers:

- Browser: `unhandledrejection` event on `window`
- Node 15+: process **crash** by default (`unhandledRejection`)

```js
Promise.reject(new Error("nobody listens"));        // unhandled
const p = Promise.reject(new Error("late"));
setTimeout(() => p.catch(() => {}), 0);              // attaching too late may still be reported
```

Always handle rejections: chain `.catch`, use `try/catch` with `await`, or return the promise to a caller who will.

## Promises vs callbacks

| | Callbacks | Promises |
|---|-----------|----------|
| Called | possibly multiple/zero times | settled exactly once |
| Timing | maybe sync | always async |
| Error flow | manual `err` checks | automatic propagation down the chain |
| Composition | manual | `then` chaining, `all`, `race`, ... |
| Cancellation | custom | not built in: use `AbortSignal` |
| Repeated values | natural | not suitable (use events or async iterators) |

## Promises are eager

```js
const p = new Promise((res) => { console.log("started"); res(); });   // logs immediately
```

To delay work, wrap in a function: `const task = () => fetch(url);`.

## Promises cannot be canceled

Once started, work continues. To stop it, pass an `AbortSignal` into the operation (see [Cancellation](./08_cancellation-and-abort.md)). Ignoring a result is not canceling.

## Pitfalls

| Pitfall | Why it hurts | Better |
|---------|--------------|--------|
| Forgetting to `return` inside `.then` | Broken chain, lost errors | Return the promise |
| No `.catch` at the end | Unhandled rejection | Terminate chains with `.catch` |
| Throwing in executor after `resolve` | Silently ignored | Be aware settled promises ignore later calls |
| Creating promises inside loops without collecting them | Floating work | `Promise.all(array.map(...))` |
| `new Promise` around something already a promise (anti-pattern) | Verbose, error-prone | Return the existing promise |
| Using promises for repeated events | Settles once | Events / async iterators |
| Assuming handlers run synchronously | Order surprises | Treat `then` as always async |
| Expecting `finally` to receive a value | It gets none | Use `then` for values |

## Key takeaways

- A promise settles once to fulfilled or rejected; handlers run asynchronously as microtasks
- `then` returns a new promise; returned promises are flattened; `catch` recovers
- Always return promises from handlers and end chains with error handling
- Promises are eager and cannot be canceled: use `AbortSignal` for cancellation

**Next:** [Promise Combinators](./04_promise-combinators.md)
