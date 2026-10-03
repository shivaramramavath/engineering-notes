# Memory Leaks

A **memory leak** is memory that is still **reachable** but no longer **needed**. The garbage collector cannot free it, so usage grows until the tab slows or the process crashes. Leaks are rarely one big mistake; they are usually a small reference kept in the wrong place.

## Recognizing a leak

| Symptom | Where |
|---------|-------|
| Memory climbs steadily and never returns to a baseline after GC | Node `heapUsed`/`rss`, browser Task Manager |
| Sawtooth pattern whose **floor** keeps rising | DevTools Performance memory graph |
| Process killed (`OOMKilled`) or `FATAL ERROR: ... heap out of memory` | Node, containers |
| Page gets slower the longer it is open | Single-page apps |
| Detached DOM nodes accumulate | DevTools heap snapshot |

Growth alone is not a leak: caches fill, JIT warms up, and the heap grows before GC runs. A leak is **growth that survives garbage collection** under a steady workload.

## Common leak patterns

### 1. Accidental globals

```js
function setup() {
  cache = new Array(1e6).fill(0);       // sloppy mode: creates a global (no declaration)
}
```

Fix: use `'use strict'` (ES modules are strict by default), declare variables with `const`/`let`, and avoid attaching large data to `globalThis`/`window`.

### 2. Unbounded caches and collections

```js
const cache = new Map();

function getUser(id) {
  if (!cache.has(id)) cache.set(id, loadUser(id));    // grows forever
  return cache.get(id);
}
```

Fixes:

- Bound the size (LRU) and expire entries (TTL)
- Key by object in a `WeakMap` if values should die with the key
- Cache only what is hot

```js
class LRU {
  #max; #map = new Map();
  constructor(max = 500) { this.#max = max; }

  get(key) {
    if (!this.#map.has(key)) return undefined;
    const v = this.#map.get(key);
    this.#map.delete(key);
    this.#map.set(key, v);                 // refresh recency
    return v;
  }

  set(key, value) {
    this.#map.delete(key);
    this.#map.set(key, value);
    if (this.#map.size > this.#max) {
      this.#map.delete(this.#map.keys().next().value);   // evict the oldest
    }
  }
}
```

See [Caching](../23_real-world-patterns/08_caching.md).

### 3. Forgotten timers

```js
function start() {
  const data = loadBigData();
  setInterval(() => report(data), 1000);   // runs forever; keeps `data` alive
}
```

Fix: keep the handle and clear it when the work is done.

```js
const id = setInterval(tick, 1000);
// later
clearInterval(id);

// Node: let the process exit even if the timer is pending
setInterval(tick, 1000).unref();
```

### 4. Event listeners that are never removed

```js
function mount(el) {
  window.addEventListener('resize', () => layout(el));   // never removed; keeps `el` alive
}
```

Fixes:

```js
// Remove explicitly
const onResize = () => layout(el);
window.addEventListener('resize', onResize);
// on cleanup
window.removeEventListener('resize', onResize);

// Or tie many listeners to one AbortController
const ac = new AbortController();
window.addEventListener('resize', onResize, { signal: ac.signal });
window.addEventListener('scroll', onScroll, { signal: ac.signal });
// on cleanup
ac.abort();                                    // removes all of them
```

In Node, `EventEmitter` listeners leak the same way. The warning `MaxListenersExceededWarning: Possible EventEmitter memory leak detected` points directly at this (see [Events](../16_nodejs/05_events.md)).

```js
// Leak: a new listener per request
server.on('request', (req) => {
  config.on('change', () => update(req));
});
```

### 5. Detached DOM nodes

```js
const cache = [];

function render() {
  const node = document.createElement('div');
  document.body.appendChild(node);
  cache.push(node);                    // JS still references it
  document.body.removeChild(node);     // removed from the page, but kept alive in memory
}
```

A removed element that is still referenced from JavaScript (arrays, maps, closures, listeners) stays in memory with its **entire subtree**. In DevTools heap snapshots, filter by **"Detached"**.

Fix: drop references when removing elements; use `WeakMap`/`WeakRef` for element-keyed metadata.

### 6. Closures holding large data

```js
function process() {
  const huge = new Array(1e7).fill('x');
  const summary = huge.length;

  return function () {
    return summary;                    // may keep the shared closure context alive
  };
}
```

V8 retains only variables **captured by some closure** in a scope, but if any inner function captures `huge`, all closures created in that scope keep it alive. Avoid capturing big values; copy what you need into a small variable and let the large one go out of scope.

```js
function process() {
  const huge = load();
  const summary = summarize(huge);
  return () => summary;                // closure captures only the small value
}
```

See [Closure Pitfalls](../06_closures/03_closure-pitfalls.md).

### 7. Promises that never settle, and long chains

```js
const pending = new Map();

function request(id) {
  return new Promise((resolve) => {
    pending.set(id, resolve);          // never resolved or deleted if the reply is lost
  });
}
```

Always remove entries on completion **and** on timeout or failure. Add timeouts so promises settle.

### 8. Growing logs, buffers, and queues

```js
const history = [];
socket.on('message', (m) => history.push(m));     // unbounded
```

Fix: use a ring buffer or cap the length; drop old entries; apply backpressure (see [Concurrency Control](../17_concurrency-and-parallelism/05_concurrency-control.md)).

### 9. Reading whole files or responses into memory

```js
const data = await fs.readFile('10gb.log');      // may crash or force huge GCs
```

Fix: stream (see [Streams](../16_nodejs/06_streams.md)).

### 10. Third-party resources and native memory

- Database connections, sockets, file handles not closed
- Worker threads or child processes never terminated
- `Buffer` and `ArrayBuffer` retained by references (memory is off-heap, so `heapUsed` can look flat while `rss` climbs)
- Libraries that keep internal registries (loggers, metrics, ORMs)

Use `try/finally`, `stream.pipeline`, and explicit `close()`/`destroy()`.

### 11. `console.log` and debugging aids

In browsers with DevTools open, objects passed to `console.log` can be retained for inspection. In Node, large logs buffered while stdout is slow (for example piped to a slow consumer) can pile up in memory.

## Finding leaks: workflow

1. **Reproduce** under steady, repeatable load (a script, an automated test, or repeated UI actions)
2. **Confirm growth survives GC**: trigger GC and compare the post-GC baseline over time
3. **Take heap snapshots** at intervals and **compare** them
4. **Find the retainer path**: what chain of references keeps the objects alive
5. **Fix, then re-measure** to verify the baseline stays flat

## Browser: Chrome DevTools

**Memory** panel:

| Tool | Use |
|------|-----|
| **Heap snapshot** | Full object graph at a point in time; compare two snapshots to see what grew |
| **Allocation instrumentation on timeline** | Blue bars show allocations; those that remain blue after GC are retained |
| **Allocation sampling** | Low-overhead view of where allocations happen (by function) |

Steps for a three-snapshot comparison:

1. Do the action once to warm up caches, then take **Snapshot 1** (click the trash can icon to force GC first)
2. Repeat the suspicious action several times (for example open and close a dialog 10 times)
3. Force GC, take **Snapshot 2**
4. In Snapshot 2 choose **Comparison** with Snapshot 1 and sort by **# Delta** or **Size Delta**
5. Expand suspicious constructors and read the **Retainers** panel at the bottom: it shows the path from a root to the object

Useful filters: **Detached** (detached DOM trees), and class names of your own types. Compare **Shallow size** (the object itself) with **Retained size** (everything freed if it were removed).

The **Performance** panel with the **Memory** checkbox shows JS heap, DOM nodes, and listener counts over time; ever-increasing node and listener counts hint at leaks.

## Node.js

### Quick monitoring

```js
setInterval(() => {
  const { heapUsed, rss, external, arrayBuffers } = process.memoryUsage();
  console.log({ heapUsed: mb(heapUsed), rss: mb(rss), external: mb(external), arrayBuffers: mb(arrayBuffers) });
}, 5000).unref();

const mb = (n) => (n / 1048576).toFixed(1) + 'MB';
```

Rising `heapUsed` after GCs suggests JS objects leaking; rising `rss` or `external` with flat `heapUsed` suggests native memory, buffers, or fragmentation.

### Heap snapshots

```js
import v8 from 'node:v8';

const file = v8.writeHeapSnapshot();         // writes Heap-<timestamp>.heapsnapshot, blocks while writing
console.log('snapshot saved to', file);
```

```bash
node --heapsnapshot-signal=SIGUSR2 app.js    # then: kill -USR2 <pid>   (Linux/macOS)
node --heapsnapshot-near-heap-limit=3 app.js # snapshots just before an out-of-memory crash
node --inspect app.js                        # attach Chrome DevTools: chrome://inspect
```

Load `.heapsnapshot` files in Chrome DevTools (Memory → Load) and use the same comparison workflow. Taking a snapshot **pauses** the process and uses a lot of memory (roughly the heap size), so be careful in production; do it on one instance, away from peak traffic.

### Other tools

| Tool | Purpose |
|------|---------|
| `node --inspect` + DevTools | Live snapshots and allocation sampling |
| `node --trace-gc` | See GC frequency and how much each collects |
| `v8.getHeapStatistics()` | Heap limit, used, available |
| `process.report.writeReport()` | Diagnostic report including memory info |
| `clinic.js` (`clinic heapprofiler`, `clinic doctor`) | Guided profiling |
| `memlab` (Meta) | Automated leak detection for browser apps |
| APM tools (Datadog, New Relic, Sentry) | Track memory over time in production |

## Testing for leaks

```js
// A rough test: run the operation many times, force GC (node --expose-gc), compare heap
import assert from 'node:assert/strict';

async function measure(fn, iterations = 1000) {
  for (let i = 0; i < 100; i++) await fn();           // warm-up
  global.gc();
  const before = process.memoryUsage().heapUsed;

  for (let i = 0; i < iterations; i++) await fn();
  global.gc();
  const after = process.memoryUsage().heapUsed;

  return after - before;
}

const growth = await measure(() => handleRequest(sampleRequest));
assert.ok(growth < 5 * 1024 * 1024, `heap grew by ${growth} bytes`);   // choose a threshold from baseline noise
```

`FinalizationRegistry` or `WeakRef` can verify that an object **is** collected in a test: create it, drop references, force GC, and check that `deref()` returns `undefined`. Treat such tests as heuristics, since GC timing is not guaranteed.

## Prevention checklist

| Area | Habit |
|------|-------|
| Variables | Strict mode, declare everything, avoid long-lived globals |
| Caches | Set max size and TTL; consider `WeakMap` |
| Timers | Every `setInterval` has a `clearInterval`; use `unref()` in Node for background timers |
| Listeners | Pair `add` with `remove`, or use `AbortController`; watch for `MaxListenersExceededWarning` |
| DOM | Null out references when removing nodes; avoid storing nodes in long-lived structures |
| Closures | Capture small values, not whole objects |
| Collections | Delete entries when done; cap queues and histories |
| Promises | Add timeouts; remove pending entries on any outcome |
| I/O | Stream large data; always `close`/`destroy`/`pipeline` |
| Components (frameworks) | Clean up in `useEffect` return functions, `onUnmounted`, `ngOnDestroy`, etc. |
| Monitoring | Track `heapUsed`, `rss`, GC time, and event-loop delay in production |

## Pitfalls

| Pitfall | Why it hurts | Better |
|---------|--------------|--------|
| Calling growth a leak without checking post-GC | Warm-up and lazy GC look like leaks | Compare after forced GC, over time |
| Taking one snapshot | Cannot tell what grew | Compare snapshots |
| Fixing the biggest object instead of the retainer | The cause is the reference path | Read the **Retainers** panel |
| Snapshots in production at peak load | Pauses and memory doubling | One canary instance, off-peak |
| Watching only `heapUsed` | Misses buffers and native memory | Track `rss`, `external`, `arrayBuffers` |
| "Fixing" with `global.gc()` or restarts | Hides the problem | Find the retainer |
| Clearing a reference but keeping another (array, map, closure, listener) | Object stays reachable | Remove **every** path from a root |
| Assuming `WeakRef` fixes retention | The strong reference elsewhere still exists | Remove strong references |

## Key takeaways

- A leak is memory that stays reachable but is never used again: GC cannot help, because it works only on unreachable objects
- Common causes: unbounded caches, forgotten timers and listeners, detached DOM nodes, closures capturing big data, unsettled promises, unbounded queues, unclosed resources
- Confirm a leak by showing growth **after GC** under steady load
- Use heap snapshot comparison and read the **retainer path** to find who holds the object
- In Node, also watch off-heap memory (`external`, `arrayBuffers`, `rss`)
- Prevent leaks with bounded data structures, explicit cleanup, `AbortController`, streaming, and production monitoring

**Next:** [Memory Optimization](./04_memory-optimization.md)