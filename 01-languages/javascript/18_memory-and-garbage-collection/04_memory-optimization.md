# Memory Optimization

Optimizing memory means **holding less** and **allocating less**. Holding less lowers peak usage and the risk of out-of-memory crashes. Allocating less reduces garbage collection work, which smooths out latency. Always **measure first**: most code does not need these techniques, and some of them trade readability for speed.

## Measure before optimizing

1. Define the goal: lower peak memory, fewer GC pauses, or higher throughput
2. Profile with heap snapshots and allocation sampling (see [Memory Leaks](./03_memory-leaks.md))
3. Change one thing, then re-measure with realistic data

```js
const mb = (n) => (n / 1048576).toFixed(1);

function snapshot(label) {
  const m = process.memoryUsage();
  console.log(label, 'heapUsed', mb(m.heapUsed), 'rss', mb(m.rss), 'external', mb(m.external));
}

snapshot('before');
runWorkload();
snapshot('after');
```

Run with `node --expose-gc` and call `global.gc()` before reading numbers in experiments, so garbage does not blur results.

## 1. Do not hold more than you need

### Stream instead of loading

```js
// Whole file in memory
const text = await fs.readFile('big.csv', 'utf8');
const rows = text.split('\n');

// Constant memory
import readline from 'node:readline';
import { createReadStream } from 'node:fs';

const rl = readline.createInterface({ input: createReadStream('big.csv'), crlfDelay: Infinity });
let count = 0;
for await (const line of rl) count++;
```

See [Streams](../16_nodejs/06_streams.md). The same idea applies to HTTP bodies, database cursors, and paginated APIs: process each piece, then let it go.

### Pagination and lazy evaluation

```js
// Eager: builds three large intermediate arrays
const result = bigList.map(f).filter(g).slice(0, 10);

// Lazy with generators: processes one item at a time, stops early
function* lazyMap(it, f) { for (const x of it) yield f(x); }
function* lazyFilter(it, p) { for (const x of it) if (p(x)) yield x; }
function* take(it, n) { let i = 0; for (const x of it) { if (i++ >= n) return; yield x; } }

const first10 = [...take(lazyFilter(lazyMap(bigList, f), g), 10)];
```

Modern runtimes also provide **iterator helpers** (`iterator.map().filter().take().toArray()`) where supported.

### Keep only the fields you need

```js
// Keeps every field of every record
const users = rows.map((r) => r);

// Keeps just what the next step needs
const ids = rows.map((r) => r.id);
```

Retaining a small part of a large object (a slice of a big string, one field from a huge response) can keep the whole parent alive. Copy the small part out so the parent becomes collectable.

### Release references

```js
let payload = await loadHuge();
const summary = summarize(payload);
payload = null;                    // allows collection if nothing else references it

map.delete(key);                   // remove entries when done
arr.length = 0;                    // clear a large array in place
```

Local variables disappear when a function returns, so explicit nulling mostly matters for long-lived variables (module scope, class fields, closures).

## 2. Choose compact data structures

### Typed arrays for numbers

```js
// ~16 bytes or more per element, boxed values possible, GC work
const values = new Array(1_000_000).fill(0).map(() => Math.random());

// 8 MB contiguous, no per-element overhead, not scanned by the GC
const values2 = new Float64Array(1_000_000);
```

| Typed array | Bytes per element | Range |
|-------------|-------------------|-------|
| `Uint8Array` / `Int8Array` | 1 | 0–255 / −128–127 |
| `Uint16Array` / `Int16Array` | 2 | 0–65,535 / −32,768–32,767 |
| `Uint32Array` / `Int32Array` | 4 | up to about 4.29 billion |
| `Float32Array` | 4 | about 7 digits of precision |
| `Float64Array` | 8 | ordinary JS number |
| `BigInt64Array` / `BigUint64Array` | 8 | 64-bit integers |

Pick the smallest type that fits the data. See [Typed Arrays and ArrayBuffer](../09_built-in-objects/10_typed-arrays-and-arraybuffer.md).

### Struct of arrays instead of array of objects

```js
// Array of objects: one heap object per particle
const particles = Array.from({ length: 1e6 }, () => ({ x: 0, y: 0, vx: 1, vy: 1 }));

// Struct of arrays: four typed arrays, cache-friendly, almost no GC load
const N = 1e6;
const x = new Float32Array(N), y = new Float32Array(N);
const vx = new Float32Array(N), vy = new Float32Array(N);

for (let i = 0; i < N; i++) {
  x[i] += vx[i];
  y[i] += vy[i];
}
```

This is worthwhile for millions of homogeneous records (simulation, graphics, analytics), and not for ordinary application data.

### Bit sets and packing

```js
// 1 million booleans as a plain array: ~8 MB or more
const flags = new Array(1e6).fill(false);

// As a bit set: 125 KB
const bits = new Uint8Array(Math.ceil(1e6 / 8));
const set = (i) => (bits[i >> 3] |= 1 << (i & 7));
const has = (i) => (bits[i >> 3] & (1 << (i & 7))) !== 0;
```

### `Map` and `Set` for dynamic keys

Objects with many added and deleted keys fall into slow dictionary mode. `Map` is designed for that:

```js
const counts = new Map();
for (const w of words) counts.set(w, (counts.get(w) ?? 0) + 1);
```

### Consistent object shapes

```js
class Point {
  constructor(x, y) { this.x = x; this.y = y; }     // same fields, same order, always
}

// Initialize every field in the constructor, even if the value is null for now
class Node {
  constructor(value) { this.value = value; this.next = null; this.prev = null; }
}
```

Avoid adding properties later, deleting them, or creating the same logical object with different key orders (see [Memory Model](./01_memory-model.md)).

### Arrays

- Keep arrays homogeneous (all small integers, or all objects)
- Avoid `new Array(n)` for holey arrays when you will fill it anyway: use `Array.from({ length: n }, fn)` or `push`
- Prefer `Array.prototype.push` over assigning far beyond the current length (sparse arrays become dictionary-mode)

### Strings

- Build large text with an array and `join`, or with a `Buffer`/stream, when assembling very large outputs
- For many repeated string values (status names, categories) store small integers or interned constants
- Avoid keeping giant strings just to read a few characters

```js
const parts = [];
for (const row of rows) parts.push(format(row));
const output = parts.join('\n');
```

(`+=` on strings is fine in modern engines for moderate sizes thanks to ropes.)

## 3. Allocate less in hot paths

Short-lived objects are cheap in V8 (the young generation is fast), but allocation in tight loops still adds up.

### Reuse objects and buffers

```js
// Allocates a new array every call
function addVec(a, b) { return [a[0] + b[0], a[1] + b[1]]; }

// Writes into a provided output
function addVecInto(out, a, b) {
  out[0] = a[0] + b[0];
  out[1] = a[1] + b[1];
  return out;
}

const tmp = new Float64Array(2);
for (let i = 0; i < 1e7; i++) addVecInto(tmp, p, q);
```

```js
// Reuse one buffer for many reads
const buf = Buffer.allocUnsafe(64 * 1024);
const fh = await fs.open('data.bin');
while (true) {
  const { bytesRead } = await fh.read(buf, 0, buf.length);
  if (bytesRead === 0) break;
  consume(buf.subarray(0, bytesRead));            // view, no copy
}
await fh.close();
```

### Avoid creating closures and temporary arrays in loops

```js
// New callback function each iteration
for (const row of rows) {
  items.forEach((it) => handle(row, it));
}

// Hoist or use a plain loop
for (const row of rows) {
  for (const it of items) handle(row, it);
}
```

```js
// Spread creates a copy each time
let acc = {};
for (const [k, v] of entries) acc = { ...acc, [k]: v };     // O(n²) time and lots of garbage

// Mutate the accumulator
const acc2 = {};
for (const [k, v] of entries) acc2[k] = v;
// or: Object.fromEntries(entries)
```

### Prefer in-place or non-copying operations when safe

| Copying | Non-copying / in-place |
|---------|------------------------|
| `arr.slice()`, `[...arr]` | Reuse the array |
| `arr.toSorted()`, `toReversed()` | `arr.sort()`, `arr.reverse()` when you own the array |
| `buf.slice` (copy on some types) / `Buffer.from(buf)` | `buf.subarray()` (view) |
| `str.split().map().join()` over huge text | Streaming parse |
| `JSON.parse(JSON.stringify(x))` | `structuredClone` only when needed; usually avoid cloning |

Immutable style is clearer; use in-place operations only where profiling shows allocation hurts.

### Object pooling

Reuse expensive-to-create or frequently created objects instead of allocating and discarding.

```js
class Pool {
  #free = [];
  constructor(create, reset, size = 0) {
    this.create = create;
    this.reset = reset;
    for (let i = 0; i < size; i++) this.#free.push(create());
  }

  acquire() {
    return this.#free.pop() ?? this.create();
  }

  release(obj) {
    this.reset(obj);
    this.#free.push(obj);
  }
}

const bullets = new Pool(
  () => ({ x: 0, y: 0, active: false }),
  (b) => { b.x = b.y = 0; b.active = false; },
  100,
);

const b = bullets.acquire();
// ... use ...
bullets.release(b);
```

Pooling helps in games, animation loops, and parsers that create millions of short-lived objects. It does **not** help for ordinary code: the young generation already makes short-lived allocation cheap, and pools add bugs (forgetting to reset, using an object after release). Use pools for large buffers, connections, and measured hot spots.

## 4. Work with the garbage collector

- **Short-lived is cheap, long-lived is costly.** Objects that survive promotion into the old generation make major GCs larger. Avoid creating medium-lived garbage (objects that live just long enough to be promoted, then die)
- **Large heaps cost more per major GC.** Splitting work, streaming, or offloading to disk or a cache service can help
- **Tune the young generation (Node)** when scavenges are frequent: `node --max-semi-space-size=32 app.js` (MB) can reduce minor GC count at the cost of memory. Measure before and after
- **Set the old-space limit deliberately**: `--max-old-space-size=<MB>`, and keep it below the container's memory limit with headroom for off-heap memory (buffers, native modules). The default depends on system memory and version
- **Do not call `global.gc()` in production**

## 5. Weak references for optional data

```js
// Metadata tied to an object's lifetime, without preventing its collection
const metadata = new WeakMap();
metadata.set(domNode, { observed: true });

// Optional cache that may be dropped when memory is needed
const cache = new Map();                 // key → WeakRef(value)
function cached(key, compute) {
  const hit = cache.get(key)?.deref();
  if (hit) return hit;
  const value = compute();
  cache.set(key, new WeakRef(value));
  return value;
}
```

Because `WeakRef` targets are collected nondeterministically, use them only when recomputing is acceptable. Combine with a size cap or an LRU for predictable behavior.

## 6. Off-heap and shared memory

- `Buffer` and `ArrayBuffer` data lives outside the V8 heap, so large binary data does not bloat `heapUsed` or slow heap scans
- `SharedArrayBuffer` lets worker threads use one copy of the data rather than one copy per thread (see [SharedArrayBuffer and Atomics](../17_concurrency-and-parallelism/04_sharedarraybuffer-and-atomics.md))
- Transfer (`postMessage(..., [buffer])`) moves large buffers between threads without copying
- For huge datasets consider memory-mapped or database-backed approaches (SQLite, columnar formats such as Apache Arrow) so the whole dataset need not live in the JS heap

## 7. Frontend-specific habits

| Habit | Why |
|-------|-----|
| Virtualize long lists (render only visible rows) | Far fewer DOM nodes and component instances |
| Lazy-load images, routes, and modules | Smaller working set at startup |
| Use `ImageBitmap`/`OffscreenCanvas` and release them (`bitmap.close()`) | Decoded images are large |
| Revoke object URLs: `URL.revokeObjectURL(url)` | Blob URLs hold their data alive |
| Remove listeners and observers on unmount; `observer.disconnect()` | Prevents leaks |
| Avoid storing large data in global state if it is only needed on one screen | Keep it local so it is collectable |
| Debounce and batch updates | Fewer transient objects and re-renders |
| Use `structuredClone` or immutable updates sparingly on huge state | Cloning large trees creates big garbage |

See [Browser Performance](../19_performance/03_browser-performance.md).

## 8. Node-specific habits

- Stream request and response bodies; set body size limits
- Use `pipeline` so streams are destroyed on error
- Reuse HTTP agents (keep-alive) and database pools instead of creating clients per request
- Avoid `JSON.stringify` on very large objects in request paths; stream NDJSON or chunk the output
- Avoid reading entire logs or archives; use streaming parsers
- Monitor `process.memoryUsage()` (`rss`, `heapUsed`, `external`, `arrayBuffers`) and event-loop delay
- In containers, set the heap limit relative to the memory limit (for example about 75% of the limit) so Node fails with a clear error before the OS kills it

See [Node Performance](../19_performance/04_node-performance.md).

## Quick wins checklist

| Question | If yes |
|----------|--------|
| Am I reading a whole file or response into memory? | Stream it |
| Do I build intermediate arrays I immediately discard? | Use a loop or generators |
| Is a large collection growing without bound? | Cap it, expire entries, or use a `WeakMap` |
| Are there millions of small objects with the same fields? | Typed arrays (struct of arrays) |
| Am I creating objects, closures, or arrays inside a hot loop? | Hoist or reuse |
| Am I using an object as a changing dictionary? | Use a `Map` |
| Am I keeping a small slice of something huge? | Copy the slice out and drop the original |
| Are listeners, timers, or observers cleaned up? | Pair each setup with a teardown |
| Is the heap limit sized for the deployment? | Set `--max-old-space-size` deliberately |

## Pitfalls

| Pitfall | Why it hurts | Better |
|---------|--------------|--------|
| Optimizing without measuring | Wasted effort, harder code, sometimes slower | Profile first, then re-measure |
| Micro-optimizing short-lived allocations | The young generation already handles them well | Focus on retained memory and hot loops |
| Object pools everywhere | Bugs (stale state, use after release), no gain | Pool only measured hot spots and big buffers |
| Using `Array.prototype.splice`/`shift` in loops on big arrays | O(n) moves, extra work | Index with a pointer, or use a queue/deque |
| Spreading accumulators (`{...acc}`, `[...acc]`) in loops | Quadratic time and garbage | Mutate a local accumulator |
| Typed arrays for tiny or mixed data | Awkward code, no benefit | Use them for large numeric data |
| Raising the heap limit to "fix" growth | Delays the crash, hides a leak | Find and fix the retention |
| Forgetting off-heap memory in container limits | OOM kill despite a healthy heap | Leave headroom for buffers and native memory |
| Relying on `WeakRef` caches for predictable performance | Collection timing varies | Add LRU or size limits |
| Cloning large state frequently | Large garbage spikes | Structural sharing or targeted updates |

## Key takeaways

- Reduce **retained** memory (what you hold) and **allocation rate** (what you create)
- Stream, paginate, and use generators instead of materializing large collections
- Typed arrays and structs-of-arrays are the most compact way to store large numeric data
- Keep object shapes and array element types consistent; use `Map` for dynamic keys
- Reuse buffers and objects in measured hot paths; avoid pools elsewhere
- Let the GC work: no `global.gc()`, and tune heap flags only with measurements
- Combine `WeakMap`/`WeakRef`, bounded caches, and explicit cleanup to avoid leaks
- Always profile before and after

**Next:** [Performance](../19_performance/00_README.md)