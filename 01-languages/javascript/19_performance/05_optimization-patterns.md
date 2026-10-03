# Optimization Patterns

Once profiling points at a bottleneck, these patterns are the usual fixes. They apply in the browser and in Node. Each entry says **what it does**, **when to use it**, and **what it costs**. Apply them to measured hot spots, not everywhere.

Related: [Performance Fundamentals](./01_performance-fundamentals.md), [Profiling](./02_profiling.md), [Concurrency Control](../17_concurrency-and-parallelism/05_concurrency-control.md), [Real-World Patterns](../23_real-world-patterns/00_README.md).

## The order of attack

| Order | Strategy | Example |
|-------|----------|---------|
| 1 | **Do less work**: remove it | Delete a redundant request, skip unchanged data |
| 2 | **Do it less often**: cache, memoize, batch, debounce | Cache API results |
| 3 | **Do it better**: improve the algorithm or data structure | O(n²) to O(n) |
| 4 | **Do it later**: defer, lazy-load, prioritize | Load below-the-fold content on demand |
| 5 | **Do it elsewhere**: worker, server, CDN, queue | Offload CPU to a worker thread |
| 6 | **Do it faster**: micro-optimizations | Typed arrays, reuse buffers (last resort) |

Higher items give bigger wins at lower cost.

## 1. Algorithms and data structures

### Use the right structure for lookups

```js
// O(n) per lookup
const user = users.find((u) => u.id === id);

// O(1) per lookup after a one-time O(n) index
const byId = new Map(users.map((u) => [u.id, u]));
const user2 = byId.get(id);
```

| Need | Use |
|------|-----|
| Membership tests | `Set` |
| Key-value lookup with arbitrary or changing keys | `Map` |
| Group by key | `Map` of arrays (or `Object.groupBy` / `Map.groupBy` where available) |
| Queue (FIFO) | Linked list, ring buffer, or a head index (not `Array.shift()` in a hot loop on big arrays) |
| Sorted data with many lookups | Sort once, then binary search |
| Priority ordering | Binary heap |
| Large numeric data | Typed arrays |

### Remove nested scans

```js
// O(n * m)
const matches = orders.filter((o) => customers.some((c) => c.id === o.customerId));

// O(n + m)
const ids = new Set(customers.map((c) => c.id));
const matches2 = orders.filter((o) => ids.has(o.customerId));
```

### Sort and search wisely

```js
// Sorting repeatedly inside a loop
for (const q of queries) results.push(items.sort(cmp)[0]);

// Sort once, or just find the min in O(n)
const best = items.reduce((m, x) => (cmp(x, m) < 0 ? x : m));
```

- Precompute sort keys instead of calling an expensive comparator function repeatedly (decorate-sort-undecorate)
- Use `localeCompare` with a shared `Intl.Collator` for many string comparisons: `const collator = new Intl.Collator(); items.sort((a, b) => collator.compare(a.name, b.name))`
- Use binary search on sorted arrays (O(log n))

### Early exit and short-circuiting

```js
// Scans everything
const hasAdmin = users.filter((u) => u.role === 'admin').length > 0;

// Stops at the first match
const hasAdmin2 = users.some((u) => u.role === 'admin');
```

Check cheap conditions before expensive ones (`a && expensive()`), and return early from loops and functions.

## 2. Caching and memoization

Store results of expensive, repeatable work.

### Memoize pure functions

```js
function memoize(fn, keyFn = (x) => x) {
  const cache = new Map();
  return function (...args) {
    const key = keyFn(...args);
    if (cache.has(key)) return cache.get(key);
    const value = fn.apply(this, args);
    cache.set(key, value);
    return value;
  };
}

const fib = memoize((n) => (n < 2 ? n : fib(n - 1) + fib(n - 2)));
fib(80);       // instant instead of astronomically slow
```

Only memoize **pure** functions (same input, same output, no side effects), and **bound** the cache.

### LRU and TTL

```js
class LRUCache {
  #max; #ttl; #map = new Map();
  constructor({ max = 500, ttlMs = Infinity } = {}) { this.#max = max; this.#ttl = ttlMs; }

  get(key) {
    const e = this.#map.get(key);
    if (!e) return undefined;
    if (e.expires < Date.now()) { this.#map.delete(key); return undefined; }
    this.#map.delete(key); this.#map.set(key, e);          // mark as recently used
    return e.value;
  }

  set(key, value) {
    this.#map.delete(key);
    this.#map.set(key, { value, expires: Date.now() + this.#ttl });
    if (this.#map.size > this.#max) this.#map.delete(this.#map.keys().next().value);
  }
}
```

### Request coalescing (deduplicating in-flight work)

```js
const inflight = new Map();

function dedupe(key, fn) {
  if (inflight.has(key)) return inflight.get(key);
  const p = fn().finally(() => inflight.delete(key));
  inflight.set(key, p);
  return p;
}

// Ten concurrent calls trigger one request
await dedupe(`user:${id}`, () => fetchUser(id));
```

### Stale-while-revalidate

Serve the cached value immediately, refresh it in the background. Users see instant responses and data stays reasonably fresh. Supported in HTTP (`Cache-Control: max-age=60, stale-while-revalidate=600`) and common in client data libraries.

### Cache invalidation: decide up front

| Strategy | Notes |
|----------|-------|
| **TTL** (expire after a time) | Simple; accepts staleness |
| **Event-based** (invalidate on write) | Fresh, but needs discipline |
| **Versioned keys** (`user:42:v7`, fingerprinted filenames) | Never stale; old entries die by eviction |
| **Write-through / write-behind** | Keep the cache and store in sync on writes |

Costs: memory, staleness, and bugs. Guard against **cache stampedes** (many callers recomputing at once when an entry expires) with request coalescing and TTL jitter. See [Memoization](../23_real-world-patterns/05_memoization.md) and [Caching](../23_real-world-patterns/08_caching.md).

## 3. Batching and coalescing

Replace many small operations with fewer larger ones.

| Instead of | Do |
|------------|----|
| One database query per item | One query with `IN (...)` or a join |
| One network request per event | Send events in batches every N items or N ms |
| One DOM update per change | Collect changes and apply once per frame |
| Many small file writes | Buffer and write in chunks |
| One state update per item | Batch updates (frameworks often batch automatically) |

### A micro-batcher

```js
function createBatcher(processBatch, { maxSize = 100, maxWaitMs = 10 } = {}) {
  let queue = [];
  let timer = null;

  async function flush() {
    clearTimeout(timer);
    timer = null;
    const batch = queue;
    queue = [];
    if (!batch.length) return;

    try {
      const results = await processBatch(batch.map((b) => b.item));
      batch.forEach((b, i) => b.resolve(results[i]));
    } catch (err) {
      batch.forEach((b) => b.reject(err));
    }
  }

  return (item) => new Promise((resolve, reject) => {
    queue.push({ item, resolve, reject });
    if (queue.length >= maxSize) flush();
    else timer ??= setTimeout(flush, maxWaitMs);
  });
}

const getUser = createBatcher((ids) => db.users.getMany(ids));
const [a, b, c] = await Promise.all([getUser(1), getUser(2), getUser(3)]);   // one query
```

Batching raises throughput and can add up to `maxWaitMs` latency per item. This is the idea behind **DataLoader**.

### Debounce and throttle

| | Behavior | Use for |
|---|----------|---------|
| **Debounce** | Run once after calls stop for N ms | Search-as-you-type, auto-save, resize end |
| **Throttle** | Run at most once per N ms | Scroll, mousemove, progress updates |

See [Debounce](../23_real-world-patterns/01_debounce.md) and [Throttle](../23_real-world-patterns/02_throttle.md).

## 4. Lazy and deferred work

Do not compute, load, or render until needed.

### Lazy initialization

```js
class Report {
  #data;
  get data() {
    return (this.#data ??= expensiveLoad());        // computed on first access only
  }
}
```

### Lazy loading modules

```js
const { default: Chart } = await import('./chart.js');     // fetched and evaluated only when needed
```

### Lazy evaluation with generators and iterators

```js
function* range(n) { for (let i = 0; i < n; i++) yield i; }

// Pipeline processes one item at a time and stops early
const firstTen = Iterator.from(range(1e9))
  .filter((n) => n % 7 === 0)
  .map((n) => n * 2)
  .take(10)
  .toArray();
```

(Iterator helpers require a recent runtime; otherwise write small generator functions.) See [Iterators and Iterables](../08_modern-javascript/04_iterators-and-iterables.md) and [Generators](../08_modern-javascript/05_generators.md).

### Pagination and virtualization

Load and render only what is visible: paginated APIs, infinite scroll with `IntersectionObserver`, and virtual lists.

### Prefetching and prioritization

Prefetch what the user will probably need next (idle time, on hover), and prioritize work: critical rendering first, analytics and low-value tasks later (`requestIdleCallback`, `scheduler.postTask`, `setImmediate`).

## 5. Parallelism and offloading

| Situation | Tool |
|-----------|------|
| Independent I/O calls | `Promise.all` (with a limit for many items) |
| CPU-bound work in the browser | Web Worker |
| CPU-bound work in Node | `worker_threads` pool |
| Long-running jobs triggered by requests | Job queue with workers |
| Static content and edge logic | CDN / edge functions |
| Heavy numeric work | WebAssembly or native modules |

```js
// Parallel I/O with a cap
const results = await mapLimit(urls, 8, (u) => fetch(u).then((r) => r.json()));
```

See [Concurrency Control](../17_concurrency-and-parallelism/05_concurrency-control.md), [Web Workers](../17_concurrency-and-parallelism/02_web-workers.md), and [Worker Threads](../17_concurrency-and-parallelism/03_worker-threads.md). Avoid parallelizing tiny tasks: messaging and startup overhead can exceed the gain.

## 6. Reducing data and work per request

| Technique | Effect |
|-----------|--------|
| **Select only needed fields** (SQL columns, GraphQL fields, `?fields=`) | Less I/O, serialization, and memory |
| **Pagination / cursors** | Bounded response size |
| **Compression** (Brotli, gzip) | Smaller transfers; costs CPU |
| **Efficient formats** (binary, MessagePack, Protocol Buffers) for high-volume internal traffic | Smaller and faster to parse than JSON |
| **Delta updates** (send only what changed) | Less bandwidth |
| **Conditional requests** (`ETag`, `If-None-Match`, `304`) | Avoid re-sending unchanged data |
| **Precomputation / materialized views** | Move cost from read time to write time |
| **Denormalization** (carefully) | Fewer joins on hot read paths |

## 7. Memory and allocation patterns

Detailed in [Memory Optimization](../18_memory-and-garbage-collection/04_memory-optimization.md). Highlights:

- Stream instead of buffering
- Typed arrays for large numeric data
- Reuse buffers and objects in measured hot loops
- Keep object shapes and array types consistent
- Avoid creating closures, arrays, and objects inside tight loops when a profile shows allocation pressure
- Bound caches, clean up listeners and timers

## 8. Micro-optimizations (last resort)

These rarely matter outside hot paths. Verify with a benchmark on realistic data.

```js
// Cache lengths and repeated property lookups only in verified hot loops
for (let i = 0, n = arr.length; i < n; i++) { /* ... */ }

// Hoist invariant work out of loops
const re = /\d+/g;                       // compile the regex once, not per iteration
for (const s of strings) re.test(s);     // beware lastIndex with the g flag

// Avoid repeated expensive calls
const formatter = new Intl.NumberFormat('en-US');   // construct once
rows.forEach((r) => out.push(formatter.format(r.total)));

// Prefer simple loops over chained array methods on very large arrays in hot paths
let sum = 0;
for (const x of nums) if (x > 0) sum += x * 2;      // one pass, no intermediate arrays
// vs: nums.filter(x => x > 0).map(x => x * 2).reduce((a, b) => a + b, 0)   // three passes, two temp arrays
```

| Micro-habit | Notes |
|-------------|-------|
| Construct `Intl` formatters, `RegExp`, `TextEncoder`/`Decoder`, and `Date` formatters **once** | Construction is expensive; use is cheap |
| Avoid `try/catch` inside the innermost hot loop? | Modern V8 handles it well; measure before bothering |
| Avoid `arguments` leaking and `delete` | Can hurt optimization |
| Prefer `for` / `for...of` to `forEach` in hot loops | Often slightly faster; rarely matters |
| Use `Array.prototype.at()`, `Object.hasOwn`, spread, destructuring freely | Modern engines optimize them well |
| String building: array `join` or `+=` | Both fine; measure for huge outputs |

Resist folklore ("`for` is always faster than `map`", "`const` is faster than `let`"): engines change, and most such claims are false or irrelevant today.

## Applying a pattern: a worked example

**Problem:** an endpoint returning a list of orders with customer names takes 1.8 s (p95).

1. **Profile / trace:** 5 ms CPU, but 1.7 s waiting on the database; 400 sequential queries for customers (**N+1**)
2. **Fix (do less work, batch):** fetch all customer ids in one query with `IN (...)`, build a `Map`, join in memory
3. **Re-measure:** p95 drops to 90 ms
4. **Second pass:** customer data changes rarely, so add a small TTL cache with request coalescing: p95 drops to 35 ms for repeated calls
5. **Guard:** add a p95 latency alert and a load-test script so a regression is caught early
6. **Stop:** the goal (< 200 ms) is met; leave the rest alone

Notice what was **not** done: no micro-optimization, no rewrite, no new framework.

## Pitfalls

| Pitfall | Why it hurts | Better |
|---------|--------------|--------|
| Caching without invalidation or bounds | Stale data, memory growth | TTL, LRU, versioned keys |
| Memoizing impure functions | Wrong results | Memoize only pure functions |
| Batching without a max size or wait | Unbounded latency or memory | `maxSize` and `maxWaitMs` |
| Debounce vs throttle mix-up | Wrong UX (laggy or spammy) | Debounce for "after", throttle for "during" |
| Parallelizing everything | Overhead, overload | Parallelize measured bottlenecks, with limits |
| Precomputing data nobody reads | Wasted work and storage | Precompute hot reads only |
| Micro-optimizing first | Little gain, worse readability | Follow the order of attack |
| Cache stampede on expiry | Spikes of duplicate heavy work | Coalescing, jittered TTLs, stale-while-revalidate |
| Trusting folklore benchmarks | Outdated across engine versions | Measure on your runtime and data |
| Leaving "temporary" optimizations undocumented | Future maintainers break them | Comment *why* and what the measurement was |

## Key takeaways

- Order of attack: remove work, do it less often, improve the algorithm, defer it, offload it, and only then micro-optimize
- Better data structures (`Map`, `Set`, indexes) and early exits eliminate most hot spots
- Cache with bounds, TTLs, and invalidation; coalesce concurrent requests to avoid stampedes
- Batch small operations, and debounce or throttle high-frequency events
- Load, compute, and render lazily; paginate and virtualize
- Offload CPU-bound work to workers or queues, and cap parallel I/O
- Validate each change with a measurement, and stop when the goal is reached

**Next:** [Design Patterns](../20_design-patterns/00_README.md)