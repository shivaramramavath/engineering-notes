# SharedArrayBuffer and Atomics

Normally threads share nothing and communicate by copying messages. A `SharedArrayBuffer` (SAB) is a block of memory that **several threads can read and write at the same time**, with no copying. That speed comes with the classic problems of shared-memory programming: **race conditions**. The `Atomics` object provides the tools to avoid them.

## SharedArrayBuffer

```js
const sab = new SharedArrayBuffer(16);       // 16 bytes, zero-filled
const view = new Int32Array(sab);            // typed array view over it (4 ints)

view[0] = 42;
```

Pass it to a worker. The **same memory** is visible on both sides:

```js
// main.mjs (Node)
import { Worker } from 'node:worker_threads';

const sab = new SharedArrayBuffer(4);
const counter = new Int32Array(sab);

const worker = new Worker(new URL('./worker.mjs', import.meta.url), { workerData: { sab } });
worker.on('exit', () => console.log('counter =', counter[0]));
```

```js
// worker.mjs
import { workerData } from 'node:worker_threads';
const counter = new Int32Array(workerData.sab);
counter[0] = 123;
```

In browsers, `worker.postMessage({ sab })` shares it (do **not** list it in the transfer array: shared buffers are shared, not moved).

| | `ArrayBuffer` | `SharedArrayBuffer` |
|---|---------------|---------------------|
| Sent with `postMessage` | Copied, or transferred (moved) | **Shared** (same memory) |
| Resizable | Yes (`resizable`) | Growable (`growable`, `maxByteLength`) |
| Access from several threads | One at a time | Simultaneously |
| Needs `Atomics` for safety | No | Yes |

Only the raw bytes are shared. Typed arrays (`Int32Array`, `Float64Array`, ...) are per-thread views over it. Objects, strings, and functions cannot live in a SAB; you encode data as numbers and bytes.

### Browser requirement: cross-origin isolation

Browsers expose `SharedArrayBuffer` only on **cross-origin isolated** pages (served with these headers):

```
Cross-Origin-Opener-Policy: same-origin
Cross-Origin-Embedder-Policy: require-corp
```

Check at runtime: `self.crossOriginIsolated`. Node has no such restriction.

## Race conditions

`counter[0]++` looks like one step but is **three**: read, add, write. Two threads can interleave:

```
Thread A reads 5
Thread B reads 5
Thread A writes 6
Thread B writes 6     ← one increment lost; the result should be 7
```

```js
// worker.mjs: 4 workers each run this
for (let i = 0; i < 100_000; i++) {
  counter[0]++;              // NOT atomic: updates get lost
}
// Final counter is often well below 400000
```

Fix with `Atomics`:

```js
for (let i = 0; i < 100_000; i++) {
  Atomics.add(counter, 0, 1);       // atomic read-modify-write
}
// Final counter is exactly 400000
```

## The Atomics API

`Atomics` works on integer typed arrays (`Int8Array`, `Uint8Array`, `Int16Array`, `Uint16Array`, `Int32Array`, `Uint32Array`, `BigInt64Array`, `BigUint64Array`) backed by a `SharedArrayBuffer` (some operations also accept non-shared arrays). All of them are indivisible: no other thread can observe a half-done operation.

| Function | Effect | Returns |
|----------|--------|---------|
| `Atomics.load(ta, i)` | Read | Value |
| `Atomics.store(ta, i, v)` | Write | `v` |
| `Atomics.add(ta, i, v)` | `ta[i] += v` | **Old** value |
| `Atomics.sub(ta, i, v)` | `ta[i] -= v` | Old value |
| `Atomics.and / or / xor(ta, i, v)` | Bitwise operation | Old value |
| `Atomics.exchange(ta, i, v)` | Set, return the previous value | Old value |
| `Atomics.compareExchange(ta, i, expected, v)` | Set to `v` only if `ta[i] === expected` | Old value |
| `Atomics.wait(ta, i, expected, timeout?)` | Block while `ta[i] === expected` | `'ok'`, `'not-equal'`, `'timed-out'` |
| `Atomics.waitAsync(ta, i, expected, timeout?)` | Non-blocking `wait` (returns a promise-like) | `{ async, value }` |
| `Atomics.notify(ta, i, count?)` | Wake waiting threads | Number woken |
| `Atomics.isLockFree(size)` | Whether operations of that byte size are lock-free | Boolean |

```js
const a = new Int32Array(new SharedArrayBuffer(8));

Atomics.store(a, 0, 10);
Atomics.add(a, 0, 5);                        // returns 10, a[0] is now 15
Atomics.compareExchange(a, 0, 15, 99);       // a[0] was 15 → set to 99, returns 15
Atomics.compareExchange(a, 0, 15, 1);        // a[0] is 99, not 15 → unchanged, returns 99
Atomics.load(a, 0);                          // 99
```

`compareExchange` (compare-and-swap, CAS) is the building block for locks and lock-free structures.

### Memory ordering

Atomics operations are **sequentially consistent**: they act as barriers, so ordinary reads and writes before an `Atomics` call are visible to other threads after they observe that call's effect. Plain (non-atomic) accesses to shared memory have no such guarantees and may be reordered or seen late.

```js
// Producer
data[0] = 123;                               // plain write
Atomics.store(flag, 0, 1);                   // publish: the data write is visible before the flag

// Consumer
while (Atomics.load(flag, 0) === 0) {}       // spin (simple but wasteful: see wait/notify below)
console.log(data[0]);                        // 123, guaranteed
```

## wait and notify: efficient blocking

Spinning in a loop burns CPU. `Atomics.wait` puts the thread to **sleep** until another thread calls `Atomics.notify` on the same location.

```js
// worker: wait until the main thread sets index 0 to something other than 0
const result = Atomics.wait(shared, 0, 0);          // sleeps while shared[0] === 0
console.log(result);                                // 'ok' once woken

// another thread
Atomics.store(shared, 0, 1);
Atomics.notify(shared, 0, 1);                       // wake up one waiter
```

**Important:** `Atomics.wait` blocks the thread, so it is **not allowed on the browser's main thread** (it throws). Use it in workers. Use `Atomics.waitAsync` where a non-blocking wait is needed (check availability).

```js
const { async, value } = Atomics.waitAsync(shared, 0, 0);
if (async) {
  value.then((r) => console.log('woken:', r));      // promise resolves with 'ok' or 'timed-out'
}
```

In Node, the main thread may use `Atomics.wait`, but doing so blocks its event loop.

## A mutex (lock)

Use one `Int32Array` slot: `0` = unlocked, `1` = locked.

```js
const UNLOCKED = 0;
const LOCKED = 1;

class Mutex {
  constructor(sharedInt32Array, index = 0) {
    this.a = sharedInt32Array;
    this.i = index;
  }

  lock() {
    while (Atomics.compareExchange(this.a, this.i, UNLOCKED, LOCKED) !== UNLOCKED) {
      Atomics.wait(this.a, this.i, LOCKED);        // sleep while locked
    }
  }

  unlock() {
    Atomics.store(this.a, this.i, UNLOCKED);
    Atomics.notify(this.a, this.i, 1);             // wake one waiter
  }
}

// Every thread builds its own Mutex over the same SharedArrayBuffer
const lockMem = new Int32Array(sab, 0, 1);
const mutex = new Mutex(lockMem);

mutex.lock();
try {
  // critical section: only one thread at a time
  sharedData[0] += 1;                              // plain ops are safe inside the lock
} finally {
  mutex.unlock();
}
```

Always unlock in `finally`. Forgetting to unlock deadlocks every other thread.

## Producer and consumer with a ring buffer (sketch)

A single-producer, single-consumer queue can avoid locks entirely: the producer owns the write index, the consumer owns the read index, and both are updated atomically.

```js
const CAPACITY = 1024;
const sab = new SharedArrayBuffer(8 + CAPACITY * 4);
const idx = new Int32Array(sab, 0, 2);               // [0] = readIndex, [1] = writeIndex
const data = new Int32Array(sab, 8, CAPACITY);

function push(value) {                               // producer thread only
  const w = Atomics.load(idx, 1);
  const r = Atomics.load(idx, 0);
  if ((w + 1) % CAPACITY === r) return false;        // full
  data[w] = value;
  Atomics.store(idx, 1, (w + 1) % CAPACITY);         // publish after writing the data
  Atomics.notify(idx, 1);
  return true;
}

function pop() {                                     // consumer thread only
  const r = Atomics.load(idx, 0);
  const w = Atomics.load(idx, 1);
  if (r === w) return undefined;                     // empty
  const value = data[r];
  Atomics.store(idx, 0, (r + 1) % CAPACITY);
  return value;
}
```

This is a teaching sketch. For production, use a tested library.

## Typical uses

| Use case | Pattern |
|----------|---------|
| Shared counters and statistics | `Atomics.add` |
| Cancellation flag checked by a long task | `Atomics.load(flag, 0)` each iteration; main sets it with `store` |
| Progress reporting without message overhead | Worker `store`s a progress number; UI reads with `load` |
| Large datasets processed by several threads | Each thread works on its own slice of a shared typed array (no locking needed if slices do not overlap) |
| Locks and semaphores between workers | `compareExchange` plus `wait` / `notify` |
| WebAssembly threads | Wasm memory can be shared (`new WebAssembly.Memory({ shared: true, ... })`) |

### Splitting work without locks

```js
// main: 4 workers, each handles a disjoint slice
const N = 4_000_000;
const sab = new SharedArrayBuffer(N * Float64Array.BYTES_PER_ELEMENT);
const data = new Float64Array(sab);

const chunk = N / 4;
for (let w = 0; w < 4; w++) {
  new Worker(new URL('./slice.mjs', import.meta.url), {
    workerData: { sab, start: w * chunk, end: (w + 1) * chunk },
  });
}
```

```js
// slice.mjs: no locks needed because slices never overlap
import { workerData } from 'node:worker_threads';
const { sab, start, end } = workerData;
const data = new Float64Array(sab);
for (let i = start; i < end; i++) data[i] = Math.sqrt(i);
```

## Pitfalls

| Pitfall | Why it hurts | Better |
|---------|--------------|--------|
| `shared[i]++` or `+=` on shared memory | Not atomic: lost updates | `Atomics.add` |
| Plain reads of a flag another thread writes | May see stale values or reordering | `Atomics.load` / `store` |
| `Atomics.wait` on the browser main thread | Throws (would freeze the page) | Use it in workers, or `waitAsync` |
| Forgetting to `unlock` (no `finally`) | Deadlock | `try/finally` |
| Spin loops waiting for a value | Wastes CPU | `Atomics.wait` / `notify` |
| Storing objects or strings in a SAB | Not possible | Encode as numbers/bytes; send structured data by message |
| Assuming float or non-integer atomics | Atomics support integer arrays only | Use integer representations or a lock |
| Putting a `SharedArrayBuffer` in the transfer list | It is shared, not transferred | Just include it in the message or `workerData` |
| Missing COOP/COEP headers in browsers | `SharedArrayBuffer` is undefined | Serve with isolation headers |
| Overlapping slices without synchronization | Data races | Disjoint ranges, or locks |
| Complex lock-free code without testing | Subtle, hard-to-reproduce bugs | Prefer messages; use proven libraries |

## Key takeaways

- A `SharedArrayBuffer` is memory visible to several threads at once; typed arrays are per-thread views of it
- Non-atomic reads and writes on shared memory cause race conditions
- `Atomics.add`, `compareExchange`, `load`, and `store` give indivisible operations and ordering guarantees
- `Atomics.wait` / `notify` provide efficient sleeping and waking; `wait` is forbidden on the browser main thread
- Build locks from `compareExchange` plus `wait` / `notify`, and always release in `finally`
- Browsers need cross-origin isolation (COOP and COEP) for `SharedArrayBuffer`
- Reach for messages first; use shared memory when copying is too slow

**Next:** [Concurrency Control](./05_concurrency-control.md)
