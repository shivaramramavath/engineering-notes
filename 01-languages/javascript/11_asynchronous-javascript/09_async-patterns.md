# Async Patterns

Reusable recipes built from promises, `async`/`await`, and `AbortSignal`. Copy, adapt, or reach for a library (`p-limit`, `p-retry`, `p-queue`, `p-map`) when needs grow.

## 1. Sleep and delay

```js
const sleep = (ms, { signal } = {}) => new Promise((resolve, reject) => {
  if (signal?.aborted) return reject(signal.reason);
  const id = setTimeout(resolve, ms);
  signal?.addEventListener("abort", () => { clearTimeout(id); reject(signal.reason); }, { once: true });
});

// Node built in
import { setTimeout as sleepNode } from "node:timers/promises";
await sleepNode(500);
```

## 2. Timeout

```js
// preferred: cancels the underlying work
const res = await fetch(url, { signal: AbortSignal.timeout(5000) });

// for APIs without signals: stop waiting (work continues)
const withTimeout = (promise, ms, message = "Timed out") => {
  let id;
  const timer = new Promise((_, reject) => { id = setTimeout(() => reject(new Error(message)), ms); });
  return Promise.race([promise, timer]).finally(() => clearTimeout(id));
};
```

## 3. Retry with exponential backoff and jitter

```js
async function retry(fn, { retries = 3, baseMs = 200, maxMs = 5000, shouldRetry = () => true, signal } = {}) {
  for (let attempt = 0; ; attempt++) {
    try {
      return await fn({ attempt, signal });
    } catch (err) {
      if (attempt >= retries || signal?.aborted || !shouldRetry(err)) throw err;
      const exp = Math.min(maxMs, baseMs * 2 ** attempt);
      const delay = Math.random() * exp;                       // full jitter
      await sleep(delay, { signal });
    }
  }
}

const data = await retry(() => fetchJson(url), {
  shouldRetry: (e) => e.status >= 500 || e.name === "TypeError",
});
```

Retry only **idempotent** operations or use idempotency keys. Never retry validation errors (4xx).

## 4. Concurrency limit (pool)

Run many tasks with at most `limit` in flight.

```js
async function mapLimit(items, limit, fn) {
  const results = new Array(items.length);
  let next = 0;

  async function worker() {
    while (true) {
      const i = next++;
      if (i >= items.length) return;
      results[i] = await fn(items[i], i);
    }
  }

  await Promise.all(Array.from({ length: Math.min(limit, items.length) }, worker));
  return results;                                              // input order preserved
}

const pages = await mapLimit(urls, 5, (u) => fetch(u).then((r) => r.text()));
```

First failure rejects the whole group; use `try/catch` inside `fn` or `allSettled` semantics if needed.

## 5. Task queue

```js
class Queue {
  #concurrency; #running = 0; #pending = [];
  constructor(concurrency = 2) { this.#concurrency = concurrency; }

  add(task) {
    return new Promise((resolve, reject) => {
      this.#pending.push({ task, resolve, reject });
      this.#drain();
    });
  }

  #drain() {
    while (this.#running < this.#concurrency && this.#pending.length) {
      const { task, resolve, reject } = this.#pending.shift();
      this.#running++;
      task().then(resolve, reject).finally(() => { this.#running--; this.#drain(); });
    }
  }
  get size() { return this.#pending.length; }
}

const q = new Queue(3);
const results = await Promise.all(files.map((f) => q.add(() => upload(f))));
```

## 6. Mutex and semaphore

```js
class Mutex {
  #tail = Promise.resolve();
  lock() {
    let release;
    const next = new Promise((r) => (release = r));
    const acquired = this.#tail.then(() => release);
    this.#tail = this.#tail.then(() => next);
    return acquired;                                          // resolves to the release function
  }
  async run(fn) { const release = await this.lock(); try { return await fn(); } finally { release(); } }
}

const mutex = new Mutex();
await mutex.run(async () => { const v = await read(); await write(v + 1); });   // no lost update
```

Needed because `await` lets other code interleave between a read and a write.

## 7. Deduplicate in-flight requests

```js
const inflight = new Map();

function dedupe(key, fn) {
  if (inflight.has(key)) return inflight.get(key);
  const p = fn().finally(() => inflight.delete(key));
  inflight.set(key, p);
  return p;
}

const getUser = (id) => dedupe(`user:${id}`, () => fetchUser(id));
// ten simultaneous getUser(1) calls make ONE request
```

## 8. Memoize async functions

```js
function memoizeAsync(fn, { ttlMs = Infinity } = {}) {
  const cache = new Map();
  return (key) => {
    const hit = cache.get(key);
    if (hit && hit.expires > Date.now()) return hit.promise;
    const promise = fn(key);
    cache.set(key, { promise, expires: Date.now() + ttlMs });
    promise.catch(() => cache.delete(key));                   // do not cache failures
    return promise;
  };
}
```

## 9. Batch (coalesce) calls

```js
function createBatcher(loadMany, { waitMs = 5 } = {}) {
  let queue = [];
  let timer;
  return (key) => new Promise((resolve, reject) => {
    queue.push({ key, resolve, reject });
    timer ??= setTimeout(async () => {
      const batch = queue; queue = []; timer = undefined;
      try {
        const values = await loadMany(batch.map((b) => b.key));       // returns array aligned with keys
        batch.forEach((b, i) => b.resolve(values[i]));
      } catch (err) { batch.forEach((b) => b.reject(err)); }
    }, waitMs);
  });
}

const getUserBatched = createBatcher((ids) => fetchUsers(ids));
const [u1, u2] = await Promise.all([getUserBatched(1), getUserBatched(2)]);   // one request
```

This is the idea behind DataLoader.

## 10. Polling

```js
async function poll(fn, { intervalMs = 1000, timeoutMs = 30_000, until = Boolean, signal } = {}) {
  const deadline = Date.now() + timeoutMs;
  while (true) {
    signal?.throwIfAborted();
    const result = await fn();
    if (until(result)) return result;
    if (Date.now() + intervalMs > deadline) throw new Error("poll timed out");
    await sleep(intervalMs, { signal });
  }
}

const job = await poll(() => getJob(id), { until: (j) => j.status === "done" });
```

Use recursive/looped `await sleep` instead of `setInterval` so runs never overlap.

## 11. Debounce and throttle (async-aware)

```js
function debounceAsync(fn, ms) {
  let timer, latest = 0;
  return (...args) => new Promise((resolve, reject) => {
    clearTimeout(timer);
    const mine = ++latest;
    timer = setTimeout(async () => {
      try { const value = await fn(...args); if (mine === latest) resolve(value); }   // stale results dropped
      catch (err) { if (mine === latest) reject(err); }
    }, ms);
  });
}
```

Superseded calls never settle in this version: document it, or resolve them with `undefined`.

## 12. Event to promise

```js
const once = (target, name, { signal } = {}) => new Promise((resolve, reject) => {
  target.addEventListener(name, resolve, { once: true, signal });
  signal?.addEventListener("abort", () => reject(signal.reason), { once: true });
});

await once(img, "load");
await once(button, "click", { signal: AbortSignal.timeout(10_000) });
// Node: const [value] = await events.once(emitter, "ready");
```

## 13. Pipeline / waterfall

```js
const pipeline = (...steps) => (input) => steps.reduce((p, step) => p.then(step), Promise.resolve(input));

const process = pipeline(readFile, parseCsv, validate, save);
await process("data.csv");
```

Or simply with `await` in order; use `pipeline` when steps are data (configurable).

## 14. Race with fallback (hedged requests)

```js
async function hedged(primary, secondary, delayMs = 200) {
  const ac = new AbortController();
  const first = primary({ signal: ac.signal });
  const second = sleep(delayMs).then(() => secondary({ signal: ac.signal }));
  try { return await Promise.any([first, second]); }
  finally { ac.abort(); }
}
```

Reduces tail latency by starting a second request if the first is slow.

## 15. Lazy / cached initialization

```js
let dbPromise;
const getDb = () => (dbPromise ??= connect().catch((e) => { dbPromise = undefined; throw e; }));
// first call connects; concurrent callers share the promise; failure allows retry
```

## 16. Graceful shutdown of async work

```js
const active = new Set();
function track(promise) { active.add(promise); promise.finally(() => active.delete(promise)); return promise; }

async function shutdown() {
  server.close();
  await Promise.allSettled([...active]);                      // wait for in-flight tasks
}
```

## 17. Run in order, one at a time (serial queue)

```js
let chain = Promise.resolve();
const serial = (fn) => (chain = chain.then(fn, fn));          // each task waits for the previous
serial(() => save(1)); serial(() => save(2));
```

## 18. Parallel with error tolerance

```js
async function allOrReport(tasks) {
  const results = await Promise.allSettled(tasks.map((t) => t()));
  return {
    values: results.flatMap((r) => (r.status === "fulfilled" ? [r.value] : [])),
    errors: results.flatMap((r) => (r.status === "rejected" ? [r.reason] : [])),
  };
}
```

## Race conditions to watch for

| Situation | Problem | Fix |
|-----------|---------|-----|
| Read-modify-write around `await` | Lost updates | Mutex, atomic DB operations |
| Out-of-order responses | Stale data overwrites fresh | Abort previous / compare request ids |
| Double-submit | Duplicate side effects | Disable UI, idempotency keys, dedupe |
| Check-then-act (`if (!exists) create`) | Two callers both create | Unique constraints, locks |
| Shared mutable state across concurrent tasks | Interleaving bugs | Pass data explicitly, immutability |

```js
// lost update
let counter = 0;
async function bump() { const v = counter; await sleep(10); counter = v + 1; }
await Promise.all([bump(), bump()]);      // counter === 1, not 2
```

## Choosing a pattern

| Need | Pattern |
|------|---------|
| Stop slow work | Timeout via `AbortSignal` |
| Flaky dependency | Retry with backoff + jitter, circuit breaker |
| Thousands of tasks | `mapLimit` / queue |
| Same request made repeatedly | Dedupe / memoize |
| Many small lookups | Batcher |
| Wait for a state change | Polling or events |
| Prevent interleaving | Mutex / serial queue |
| Reduce latency | Hedged requests, `Promise.any` |

## Pitfalls

| Pitfall | Why it hurts | Better |
|---------|--------------|--------|
| Unlimited `Promise.all(items.map(...))` | Overload, rate limits | `mapLimit` |
| Retrying without backoff/jitter | Thundering herd | Exponential backoff + jitter |
| Caching rejected promises | Permanent failure | Delete on rejection |
| `setInterval` with async work | Overlapping runs | Looped `await sleep` |
| Retrying non-idempotent operations | Duplicates | Idempotency keys |
| Timeouts that do not cancel work | Leaks | Pass `AbortSignal` |
| Forgetting to clear timers in `finally` | Keeps process alive, wasted callbacks | `clearTimeout` |
| Unbounded queues | Memory growth | Max size, backpressure |
| Ignoring shutdown | Lost in-flight work | Track and await active tasks |

## Key takeaways

- Wrap common needs in small helpers: `sleep`, `timeout`, `retry`, `mapLimit`, `dedupe`
- Bound concurrency, back off with jitter, and make work cancelable with signals
- Share in-flight promises to avoid duplicate work; do not cache failures
- Watch for interleaving races around `await` and protect critical sections

**Next:** [Event Loop](../12_event-loop/00_README.md)
