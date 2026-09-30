# Memory Management

Node manages memory automatically via garbage collection — but a long-running server process (unlike a short-lived script) can still leak memory in ways that eventually crash it. This file covers how garbage collection works and the patterns that commonly cause leaks.

## How garbage collection works, briefly

JavaScript's garbage collector frees memory for objects that are no longer **reachable** — meaning nothing in your program can reference them anymore.

```js
let user = { name: "Alice" };
user = null; // the original object is now unreachable — eligible for garbage collection
```

You never manually free memory in JavaScript (unlike C/C++) — the garbage collector runs periodically, identifies unreachable objects, and reclaims their memory. A **memory leak**, in this context, means something is unintentionally keeping an object reachable long after you actually need it — the garbage collector isn't broken, it's correctly keeping alive something you accidentally still have a reference to.

---

## Checking memory usage

```js
console.log(process.memoryUsage());
```

```js
{
  rss: 45678592,        // total memory allocated for the process
  heapTotal: 20000000,   // memory allocated for the JS heap
  heapUsed: 15000000,     // memory actually in use on the heap
  external: 1000000,       // memory used by C++ objects bound to JS (e.g. Buffers)
}
```

Watching `heapUsed` climb steadily over time, without ever coming back down after garbage collection runs, is the classic signature of a leak — normal usage should rise and fall as objects are created and released, not grow indefinitely.

---

## Common leak pattern #1: growing global collections

```js
// ❌ this array grows forever — nothing ever removes old entries
const requestLog = [];

app.use((req, res, next) => {
  requestLog.push({ url: req.url, time: Date.now() });
  next();
});
```

Every request adds an entry that's never removed — over enough requests, `requestLog` consumes unbounded memory. Fix by capping its size, using a proper time-windowed data structure, or (usually the right answer) sending this to a logging system (`14-logging-observability/`) instead of holding it in memory at all.

---

## Common leak pattern #2: forgotten event listeners

```js
// ❌ if this runs repeatedly (e.g. once per request), listeners pile up forever
function handleConnection(socket) {
  emitter.on("update", (data) => {
    socket.send(data);
  });
}
```

Each call adds a new listener to `emitter`, but nothing ever removes it — even after `socket` itself is long gone, the listener (and its closure, see `03-closures.md`) keeps a reference alive, preventing garbage collection. This is also exactly what triggers the `MaxListenersExceededWarning` covered in `02-core-modules/04-events.md`.

```js
// ✅ remove the listener when it's no longer needed
function handleConnection(socket) {
  const onUpdate = (data) => socket.send(data);
  emitter.on("update", onUpdate);

  socket.on("close", () => {
    emitter.off("update", onUpdate);
  });
}
```

---

## Common leak pattern #3: closures holding onto large objects

```js
function processLargeDataset(data) {
  const summary = computeSummary(data); // summary is small

  return function () {
    return summary; // closure only needs `summary`...
    // ...but if `data` (large) is referenced ANYWHERE in this function,
    // even in unreachable code, some engines may keep the whole closure scope alive
  };
}
```

A closure (`03-closures.md`) keeps its entire surrounding scope reachable, not just the specific variables it uses — being deliberate about what a long-lived closure actually needs to reference matters more the larger and longer-lived that closure is.

---

## Common leak pattern #4: caches with no eviction

```js
// ❌ grows forever, one entry per unique user ID ever seen
const userCache = new Map();

function getUser(id) {
  if (!userCache.has(id)) {
    userCache.set(id, fetchUserFromDb(id));
  }
  return userCache.get(id);
}
```

An in-memory cache with no size limit or expiration is, functionally, a memory leak with extra steps. Use a proper LRU cache (evicts the least-recently-used entry once full) or move the cache to Redis (`07-databases/redis/02-caching-and-sessions.md`), which has its own memory limits and eviction policies designed for exactly this.

---

## Common leak pattern #5: timers that are never cleared

```js
// ❌ if this function runs many times, intervals pile up and never stop
function startPolling() {
  setInterval(() => {
    checkForUpdates();
  }, 5000);
}
```

```js
// ✅ keep a reference and clear it when it's no longer needed
function startPolling() {
  const intervalId = setInterval(() => checkForUpdates(), 5000);
  return () => clearInterval(intervalId);
}

const stopPolling = startPolling();
// later:
stopPolling();
```

A running `setInterval` (or an open connection, an open file handle) also keeps the Node process itself alive indefinitely, even with no other pending work — relevant when a script that should exit on its own doesn't (see `process` in `01-fundamentals/01-node-runtime.md`).

---

## Finding leaks: heap snapshots

```bash
node --inspect app.js
```

Then open `chrome://inspect` in Chrome, connect to the process, and use the **Memory** tab to take heap snapshots at two different points in time (e.g. before and after handling many requests) and compare them — objects that grew in count between snapshots, but shouldn't have, point directly at the leak.

```js
import v8 from "node:v8";
v8.writeHeapSnapshot(); // writes a .heapsnapshot file you can load into Chrome DevTools
```

Covered in more operational depth in `15-performance/02-profiling-and-memory-leaks.md`.

## Common mistakes

- **Unbounded in-memory arrays/maps used as ad hoc logs or caches** — always cap size or move to a proper store (a database, Redis, a log platform).
- **Registering event listeners without ever removing them**, especially inside a function that runs repeatedly (once per request, once per connection).
- **Assuming a memory leak is a garbage collector bug** — it almost never is; something in your code is unintentionally keeping an object reachable.
- **Not watching `process.memoryUsage()` (or a proper monitoring tool) in a long-running production service** — a slow leak can go unnoticed for a long time until the process finally crashes from memory exhaustion.

## Quick summary

- Node's garbage collector frees memory for objects nothing can reach anymore — a "leak" means something unintentionally still holds a reference
- Common culprits: unbounded arrays/caches, event listeners that are never removed, closures over large objects, and uncleared timers
- `process.memoryUsage().heapUsed` climbing steadily with no drops over time is the classic warning sign
- Heap snapshots (via `--inspect` + Chrome DevTools) are the standard tool for actually finding a leak's source

## Section complete

That covers the JavaScript mechanics that matter most in Node: async patterns, the event loop, closures, prototypes, error handling, and memory. **`04-npm-ecosystem`** moves on to using npm effectively day to day.
