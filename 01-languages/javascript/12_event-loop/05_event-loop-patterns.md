# Event Loop Patterns

Practical techniques for **keeping the loop free**: yielding, chunking, batching, scheduling by priority, measuring lag, and offloading CPU work.

## Symptoms of a blocked loop

| Environment | Symptoms |
|-------------|----------|
| Browser | janky scrolling, unresponsive clicks, delayed rendering, high INP |
| Node server | rising latency for **all** requests, timeouts, health checks failing |
| Anywhere | timers firing late, "script is unresponsive" warnings |

Rule of thumb: keep each task under **~50 ms** in the browser. In servers, keep synchronous sections short.

## Pattern 1: Yield to the event loop

```js
// simplest cross-environment yield (macrotask)
const yieldToMain = () => new Promise((resolve) => setTimeout(resolve, 0));

// newer browsers: yields and keeps priority
const yieldNow = () => ("scheduler" in globalThis && scheduler.yield ? scheduler.yield() : yieldToMain());

// Node
import { setImmediate as yieldNode } from "node:timers/promises";
await yieldNode();
```

Microtask-based "yields" (`await null`) do **not** let rendering or other tasks run.

## Pattern 2: Chunk long work

```js
async function processAll(items, handle, { budgetMs = 12 } = {}) {
  let deadline = performance.now() + budgetMs;
  for (const item of items) {
    handle(item);
    if (performance.now() >= deadline) {
      await yieldNow();                          // let input and rendering happen
      deadline = performance.now() + budgetMs;
    }
  }
}
```

A time budget adapts to slow devices better than a fixed chunk size.

```js
// fixed chunk size version
for (let i = 0; i < items.length; i += 500) {
  items.slice(i, i + 500).forEach(handle);
  await yieldNow();
}
```

## Pattern 3: Cancelable chunked work

```js
async function crunch(items, { signal } = {}) {
  for (let i = 0; i < items.length; i++) {
    signal?.throwIfAborted();
    heavy(items[i]);
    if (i % 200 === 0) await yieldNow();
  }
}
```

## Pattern 4: Batch updates with a microtask

Coalesce many synchronous changes into **one** update.

```js
let scheduled = false;
const dirty = new Set();

function markDirty(item) {
  dirty.add(item);
  if (scheduled) return;
  scheduled = true;
  queueMicrotask(() => {
    scheduled = false;
    flush([...dirty]);          // runs once after all synchronous markDirty calls
    dirty.clear();
  });
}
```

This is how many frameworks batch state updates.

## Pattern 5: Batch DOM work per frame

```js
const writes = [];
let raf = 0;

function scheduleWrite(fn) {
  writes.push(fn);
  raf ||= requestAnimationFrame(() => {
    raf = 0;
    const batch = writes.splice(0);
    batch.forEach((w) => w());            // all writes in the same frame
  });
}
```

Avoid **layout thrashing**: do all DOM reads first, then all writes.

```js
const heights = items.map((el) => el.offsetHeight);          // reads
items.forEach((el, i) => { el.style.height = `${heights[i] + 10}px`; });   // writes
```

## Pattern 6: Idle and priority scheduling

```js
// idle work (feature-detect)
const whenIdle = (fn) => ("requestIdleCallback" in window ? requestIdleCallback(fn, { timeout: 2000 }) : setTimeout(fn, 1));

// priorities (newer browsers)
scheduler.postTask(() => track(), { priority: "background" });
scheduler.postTask(() => render(), { priority: "user-blocking" });
```

## Pattern 7: Offload CPU work to a worker

```js
// main.js (browser)
const worker = new Worker(new URL("./hash-worker.js", import.meta.url), { type: "module" });
const result = await new Promise((resolve, reject) => {
  worker.onmessage = (e) => resolve(e.data);
  worker.onerror = reject;
  worker.postMessage(bigData, [bigData.buffer]);        // transfer instead of copy
});
```

```js
// Node: worker_threads
import { Worker } from "node:worker_threads";
const run = (data) => new Promise((resolve, reject) => {
  const w = new Worker(new URL("./task.js", import.meta.url), { workerData: data });
  w.once("message", resolve); w.once("error", reject);
});
```

Use pools (`workerpool`, `piscina`) for repeated tasks. See [Concurrency and Parallelism](../17_concurrency-and-parallelism/00_README.md).

## Pattern 8: Stream instead of buffering

```js
// bad: loads the whole file and blocks while parsing
const data = JSON.parse(fs.readFileSync("big.json", "utf8"));

// better: async and incremental
import { createReadStream } from "node:fs";
import { createInterface } from "node:readline";
for await (const line of createInterface({ input: createReadStream("big.ndjson") })) {
  handle(JSON.parse(line));
}
```

## Pattern 9: Protect servers from slow synchronous work

| Risk | Mitigation |
|------|-----------|
| `*Sync` fs/crypto calls in handlers | async versions (`fs/promises`, `crypto.pbkdf2` callback/promise) |
| Large `JSON.parse`/`stringify` | stream, paginate, limit body size |
| Regex backtracking (ReDoS) | safe patterns, input limits |
| Big loops/sorts | chunk, precompute, or worker |
| Compression/hashing of big buffers | async APIs, workers |

## Pattern 10: Measure event loop lag

```js
// Node
import { monitorEventLoopDelay } from "node:perf_hooks";
const h = monitorEventLoopDelay({ resolution: 20 });
h.enable();
setInterval(() => {
  console.log({ p50: h.percentile(50) / 1e6, p99: h.percentile(99) / 1e6, max: h.max / 1e6 });   // ms
  h.reset();
}, 5000).unref();

// portable "heartbeat" lag check
let last = performance.now();
setInterval(() => {
  const now = performance.now();
  const lag = now - last - 100;               // expected interval 100 ms
  if (lag > 50) console.warn(`event loop lag ${lag.toFixed(0)} ms`);
  last = now;
}, 100);
```

```js
// Browser: long task observer
new PerformanceObserver((list) => {
  for (const e of list.getEntries()) console.warn("long task", e.duration, e.attribution);
}).observe({ type: "longtask", buffered: true });
```

Export these metrics to monitoring and alert on p99 lag.

## Pattern 11: Backpressure and queue limits

Unbounded queues turn a slow consumer into a memory leak.

```js
class BoundedQueue {
  #items = []; #waiters = [];
  constructor(max = 100) { this.max = max; }
  async push(item) {
    while (this.#items.length >= this.max) await new Promise((r) => this.#waiters.push(r));   // wait for space
    this.#items.push(item);
  }
  shift() {
    const item = this.#items.shift();
    this.#waiters.shift()?.();
    return item;
  }
}
```

Streams (`pipeline`, `Readable.from`) and async generators provide backpressure automatically.

## Pattern 12: Rate-limit event handlers

```js
window.addEventListener("scroll", throttle(update, 100), { passive: true });
input.addEventListener("input", debounce(search, 300));
window.addEventListener("resize", () => requestAnimationFrame(layout));
```

`{ passive: true }` tells the browser the handler will not `preventDefault`, so scrolling does not wait for it.

## Pattern 13: Avoid microtask starvation

```js
// bad: processes a large queue in microtasks, blocking timers, I/O and rendering
function drain() { if (queue.length) { work(queue.shift()); queueMicrotask(drain); } }

// good: yield to macrotasks periodically
async function drain() {
  let n = 0;
  while (queue.length) {
    work(queue.shift());
    if (++n % 100 === 0) await yieldNow();
  }
}
```

## Pattern 14: Wait for the loop to settle (tests)

```js
const flushPromises = () => new Promise((resolve) => setTimeout(resolve, 0));    // after all microtasks
await flushPromises();

// Node: wait one full loop turn
import { setImmediate as tick } from "node:timers/promises";
```

Prefer fake timers (`vi.useFakeTimers`) for deterministic tests.

## Decision guide

| Situation | Technique |
|-----------|-----------|
| Long CPU loop in the UI | chunk + `scheduler.yield`, or a Web Worker |
| Long CPU task in a server | `worker_threads`, a job queue, or another service |
| Many small state changes | batch with `queueMicrotask` or `requestAnimationFrame` |
| Non-urgent work | `requestIdleCallback` / `scheduler.postTask` background |
| Huge data | stream, paginate, process incrementally |
| Unknown slowness | measure with the Performance panel, lag histograms, CPU profiles |

## Pitfalls

| Pitfall | Why it hurts | Better |
|---------|--------------|--------|
| "Yielding" with `await null` / promises only | Microtasks do not let rendering or timers run | Macrotask yield (`setTimeout`, `scheduler.yield`) |
| Chunking without a time budget on slow devices | Still janky | Budget by milliseconds |
| Sync fs/crypto/compression in request handlers | Latency spikes for everyone | Async APIs or workers |
| Moving to workers for tiny tasks | Message overhead exceeds gains | Profile first |
| Copying big data to workers | Slow postMessage | Transfer `ArrayBuffer`s |
| Unbounded queues | Memory growth | Limits and backpressure |
| Layout thrashing (read/write interleaving) | Forced reflows | Batch reads then writes |
| Measuring only averages | Hides spikes | Track p95/p99 and max lag |

## Key takeaways

- Keep tasks short; yield with a **macrotask** (not a microtask) so rendering and I/O can run
- Batch bursts of changes with microtasks or `requestAnimationFrame`
- Offload CPU-bound work to workers; stream large data; avoid sync I/O in servers
- Measure: long task observers in browsers, event-loop delay histograms in Node
- Bound your queues and respect backpressure

**Next:** [Modules](../13_modules/00_README.md)
