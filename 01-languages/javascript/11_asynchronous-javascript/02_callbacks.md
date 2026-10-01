# Callbacks (Async)

A **callback** is a function you hand to an async operation to be called when it finishes. This is the original async mechanism in JavaScript. (For callbacks in general, see [Functions: Callbacks](../02_functions/05_callbacks.md).)

```js
setTimeout(() => console.log("done"), 1000);

fs.readFile("a.txt", "utf8", (err, text) => {
  if (err) return console.error(err);
  console.log(text);
});

button.addEventListener("click", () => console.log("clicked"));
```

## Error-first convention (Node)

```js
function readConfig(path, callback) {
  fs.readFile(path, "utf8", (err, text) => {
    if (err) return callback(err);                 // 1st argument: error or null
    try {
      callback(null, JSON.parse(text));            // 2nd argument: result
    } catch (parseErr) {
      callback(parseErr);
    }
  });
}

readConfig("app.json", (err, config) => {
  if (err) return handle(err);
  start(config);
});
```

Rules: call the callback **once**, **always asynchronously**, and `return` after calling it with an error.

## Callback hell (pyramid of doom)

```js
getUser(id, (err, user) => {
  if (err) return done(err);
  getOrders(user.id, (err, orders) => {
    if (err) return done(err);
    getItems(orders[0].id, (err, items) => {
      if (err) return done(err);
      done(null, items);
    });
  });
});
```

Problems: deep nesting, repeated error handling, hard sequencing, hard parallelism, hard cancellation.

## Mitigations (before promises)

```js
// Name the steps and flatten
function onUser(err, user) { if (err) return done(err); getOrders(user.id, onOrders); }
function onOrders(err, orders) { if (err) return done(err); getItems(orders[0].id, done); }
getUser(id, onUser);
```

Libraries like `async` provided `waterfall`, `parallel`, `series`. Modern code uses promises.

## Inversion of control (the deeper problem)

When you pass a callback you **trust** the other code to:

- Call it exactly once (not twice, not never)
- Call it with the right arguments
- Call it asynchronously (never synchronously)
- Not swallow errors thrown inside it

Promises encode these guarantees; callbacks do not.

## "Zalgo": sometimes sync, sometimes async

```js
const cache = new Map();
function getData(key, cb) {
  if (cache.has(key)) return cb(null, cache.get(key));     // synchronous!
  fetchData(key, (err, data) => { cache.set(key, data); cb(err, data); });   // asynchronous
}
```

Callers cannot rely on ordering. Fix: always defer.

```js
if (cache.has(key)) return queueMicrotask(() => cb(null, cache.get(key)));
```

## Parallel callbacks by hand

```js
function parallel(tasks, done) {
  const results = [];
  let remaining = tasks.length, failed = false;
  if (!remaining) return done(null, results);
  tasks.forEach((task, i) => task((err, value) => {
    if (failed) return;
    if (err) { failed = true; return done(err); }
    results[i] = value;
    if (--remaining === 0) done(null, results);
  }));
}
```

Easy to get wrong: compare with `Promise.all`.

## Converting callbacks to promises

```js
import { promisify } from "node:util";
const readFileP = promisify(fs.readFile);
const text = await readFileP("a.txt", "utf8");

// manual
const delay = (ms) => new Promise((resolve) => setTimeout(resolve, ms));

function geolocate() {
  return new Promise((resolve, reject) => navigator.geolocation.getCurrentPosition(resolve, reject));
}

// callback API with multiple results: choose a shape
const exists = (path) => new Promise((res) => fs.access(path, (err) => res(!err)));
```

And back, for APIs that need callbacks:

```js
import { callbackify } from "node:util";
const legacy = callbackify(async (id) => loadUser(id));
```

Many Node modules ship a promise API: `node:fs/promises`, `node:timers/promises`, `node:stream/promises`, `node:dns/promises`.

## Where callbacks are still the right tool

| Use | Why |
|-----|-----|
| Event handlers (`click`, `message`) | called **many** times, not once |
| Array methods (`map`, `filter`) | synchronous iteration |
| Observer APIs (`IntersectionObserver`, `MutationObserver`) | repeated notifications |
| Hooks and middleware | caller supplies behavior |
| Streams (`on("data", cb)`) | repeated chunks |

Promises represent **one** future value. For repeated values use events, callbacks, or async iterators.

## `this` and callbacks

```js
class Loader {
  constructor() { this.count = 0; }
  load() { fetchLater(function () { this.count++; }); }      // this is lost
  loadOk() { fetchLater(() => { this.count++; }); }          // arrow keeps this
}
```

## Error handling gotcha

```js
try {
  fs.readFile("missing.txt", (err, data) => { throw new Error("in callback"); });
} catch (e) { /* never runs: the callback runs later, outside this try */ }
```

Errors thrown inside async callbacks escape to the global handler (`uncaughtException` / `window.onerror`).

## Pitfalls

| Pitfall | Why it hurts | Better |
|---------|--------------|--------|
| Ignoring `err` | Silent failures | Always check it first |
| Forgetting `return` after `callback(err)` | Callback runs twice | `return callback(err)` |
| Throwing inside async callbacks | Uncatchable by callers | Catch inside, pass to the callback |
| Mixed sync/async completion | Order bugs | Always async |
| Deep nesting | Unmaintainable | Named functions or promises |
| Calling a callback multiple times | Duplicate effects | Guard with a `called` flag |
| Using callbacks for one-shot async results in new code | Poor composition | Promises / `async` |

## Key takeaways

- Callbacks are the base mechanism but lack guarantees and compose poorly
- Node's convention is error-first, called once, always asynchronously
- Convert callback APIs with `promisify` or `new Promise`
- Keep callbacks for repeated events and hooks

**Next:** [Promises](./03_promises.md)
