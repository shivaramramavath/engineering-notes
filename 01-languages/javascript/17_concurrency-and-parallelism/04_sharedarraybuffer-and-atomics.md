# SharedArrayBuffer and Atomics in JavaScript

> True zero-copy shared memory, atomic memory barriers, wait/notify primitives, and cross-origin isolation requirements.

| Metadata | Details |
| :--- | :--- |
| **Level** | Advanced |
| **Prerequisites** | [Buffers and Typed Arrays](file:///e:/Learning/engineering-notes/01-languages/javascript/16_nodejs/07_buffers.md), [Worker Threads](file:///e:/Learning/engineering-notes/01-languages/javascript/17_concurrency-and-parallelism/03_worker-threads.md) |
| **Time to read** | 25 minutes |
| **Status** | Complete |
| **Standard / Runtime** | ECMAScript 2017+, Node.js v12+, Cross-Origin Isolated Browsers |

---

## Table of Contents
1. [What & Why](#1-what--why)
2. [Core Concepts](#2-core-concepts)
3. [How It Works Internally](#3-how-it-works-internally)
4. [Syntax & API Reference](#4-syntax--api-reference)
5. [Practical Examples](#5-practical-examples)
6. [Edge Cases, Pitfalls & Gotchas](#6-edge-cases-pitfalls--gotchas)
7. [Common Mistakes & Anti-Patterns](#7-common-mistakes--anti-patterns)
8. [Performance Characteristics](#8-performance-characteristics)
9. [Engine Internals](#9-engine-internals)
10. [Best Practices & Design Guidelines](#10-best-practices--design-guidelines)
11. [Real-World Case Study](#11-real-world-case-study)
12. [Interview Questions](#12-interview-questions)
13. [Exercises & Challenges](#13-exercises--challenges)

---

## 1. What & Why

In traditional JavaScript concurrency, threads communicate via message-passing (`postMessage`). While safe, passing massive datasets (such as 4K video frame buffers, 3D geometry meshes, or machine learning tensors) incurs significant latency from data cloning or the inconvenience of buffer detaching when using transferables.

**`SharedArrayBuffer`** allows multiple threads (Web Workers or Node.js Worker Threads) to share the exact same physical memory block simultaneously. However, concurrent, unsynchronized mutations by multiple threads introduce data races, torn reads, and undefined state.

The **`Atomics`** namespace provides atomic operations and thread synchronization primitives. It guarantees that memory operations are completed indivisibly without interruption, introduces memory barriers (fences) to prevent CPU instruction reordering, and allows threads to sleep and wake up without burning CPU cycles.

---

## 2. Core Concepts

### Shared Memory Model
A `SharedArrayBuffer` is an `ArrayBuffer` whose backing store can be mapped into multiple V8 isolates simultaneously:
- It cannot be detached or transferred.
- When passed via `postMessage()`, both the sender and receiver point to the **same physical RAM bytes**.
- Changes made by Thread A are immediately visible to Thread B.

### The Problem: Race Conditions & Torn Writes
Without atomic guarantees, simple operations like `view[0]++` compile down to three separate CPU operations:
1. **Read** value from RAM into CPU register.
2. **Increment** register value.
3. **Write** register value back to RAM.
If two threads interleave these steps simultaneously, increments are lost. Furthermore, multi-byte integers can experience "torn writes" where one thread reads half-written bytes from another thread.

### The Solution: `Atomics`
The `Atomics` global object provides static methods that execute read-modify-write sequences as a single, atomic CPU instruction (such as `LOCK CMPXCHG` on x86/x64 architectures).

### Browser Cross-Origin Isolation
Due to the **Spectre** CPU hardware vulnerability, browsers disabled `SharedArrayBuffer` in 2018. It was re-enabled only under **Cross-Origin Isolation**, which requires the following HTTP headers on the hosting server:
```http
Cross-Origin-Opener-Policy: same-origin
Cross-Origin-Embedder-Policy: require-corp
```

---

## 3. How It Works Internally

```
+-------------------------------------------------------------------------+
| PHYSICAL SYSTEM RAM (Heap BackingStore)                                 |
| Address: 0x7FFF12340000 [ SharedArrayBuffer (4096 bytes) ]             |
+-------------------------------------------------------------------------+
             ^                                           ^
             | Memory Map (Pointer)                      | Memory Map (Pointer)
             |                                           |
+---------------------------+               +---------------------------+
| WORKER THREAD 1           |               | WORKER THREAD 2           |
| TypedArray View (Int32)   |               | TypedArray View (Int32)   |
| Atomics.add(view, 0, 1)   |               | Atomics.load(view, 0)     |
|   - Hardware LOCK prefix  |               |   - Hardware Memory Fence |
+---------------------------+               +---------------------------+
```

---

## 4. Syntax & API Reference

### Allocating Shared Memory
```javascript
// Allocate 1024 bytes of shared memory
const sab = new SharedArrayBuffer(1024);

// Create typed array view over the buffer
const int32 = new Int32Array(sab);
```

### Core `Atomics` Operations

| Method | Description | Signature |
| :--- | :--- | :--- |
| `Atomics.load` | Reads value with memory barrier | `Atomics.load(typedArray, index)` |
| `Atomics.store` | Writes value with memory barrier | `Atomics.store(typedArray, index, value)` |
| `Atomics.add` | Adds and returns the *old* value | `Atomics.add(typedArray, index, value)` |
| `Atomics.sub` | Subtracts and returns the *old* value | `Atomics.sub(typedArray, index, value)` |
| `Atomics.exchange` | Stores value and returns *old* value | `Atomics.exchange(typedArray, index, value)` |
| `Atomics.compareExchange` | Stores `replacement` if current equals `expected` | `Atomics.compareExchange(typedArray, index, expected, replacement)` |
| `Atomics.wait` | Puts thread to sleep until notified | `Atomics.wait(typedArray, index, value, [timeout])` |
| `Atomics.notify` | Wakes up sleeping threads | `Atomics.notify(typedArray, index, [count])` |
| `Atomics.waitAsync` | Non-blocking wait returning a Promise | `Atomics.waitAsync(typedArray, index, value, [timeout])` |

*Note: `Atomics.wait()` works exclusively on `Int32Array` or `BigInt64Array` views.*

---

## 5. Practical Examples

### Example 1: Preventing Race Conditions with `Atomics.add`
```javascript
import { Worker, isMainThread, parentPort, workerData } from 'node:worker_threads';

if (isMainThread) {
  const sab = new SharedArrayBuffer(4);
  const view = new Int32Array(sab);

  const NUM_WORKERS = 4;
  const ITERATIONS = 100_000;
  let finished = 0;

  for (let i = 0; i < NUM_WORKERS; i++) {
    const worker = new Worker(new URL(import.meta.url), {
      workerData: { sab, iterations: ITERATIONS },
    });

    worker.on('message', () => {
      finished++;
      if (finished === NUM_WORKERS) {
        console.log(`Final Counter Value: ${view[0]}`);
        console.log(`Expected Value: ${NUM_WORKERS * ITERATIONS}`);
        // With Atomics.add: EXACT MATCH (400000)
        // With standard view[0]++: CORRUPTED (~140000-250000)
      }
    });
  }
} else {
  const { sab, iterations } = workerData;
  const view = new Int32Array(sab);

  for (let i = 0; i < iterations; i++) {
    Atomics.add(view, 0, 1);
  }

  parentPort.postMessage('DONE');
}
```

### Example 2: Thread Synchronization via `Atomics.wait` and `Atomics.notify`
```javascript
import { Worker, isMainThread, parentPort, workerData } from 'node:worker_threads';

const INDEX_FLAG = 0;
const STATE_IDLE = 0;
const STATE_WORK_READY = 1;

if (isMainThread) {
  const sab = new SharedArrayBuffer(16);
  const int32 = new Int32Array(sab);

  const worker = new Worker(new URL(import.meta.url), { workerData: { sab } });

  setTimeout(() => {
    console.log('Main thread: Preparing data and waking up worker...');
    int32[1] = 42; // Data payload
    Atomics.store(int32, INDEX_FLAG, STATE_WORK_READY);
    Atomics.notify(int32, INDEX_FLAG, 1); // Wake up 1 waiting thread
  }, 1000);
} else {
  const int32 = new Int32Array(workerData.sab);

  console.log('Worker thread: Sleeping until work is ready...');
  // Suspends worker execution until int32[0] is no longer STATE_IDLE
  const status = Atomics.wait(int32, INDEX_FLAG, STATE_IDLE);

  console.log(`Worker awakened (status: ${status})! Read payload:`, int32[1]);
}
```

### Example 3: Spin-Free Mutex (Mutual Exclusion Lock)
```javascript
const UNLOCKED = 0;
const LOCKED = 1;

export class AtomicMutex {
  constructor(sharedArray, index = 0) {
    this.view = sharedArray;
    this.index = index;
  }

  lock() {
    while (true) {
      // Attempt to acquire lock if it was UNLOCKED
      if (Atomics.compareExchange(this.view, this.index, UNLOCKED, LOCKED) === UNLOCKED) {
        return; // Lock acquired!
      }
      // If already LOCKED, sleep until notified instead of burning CPU in a spin-loop
      Atomics.wait(this.view, this.index, LOCKED);
    }
  }

  unlock() {
    if (Atomics.exchange(this.view, this.index, UNLOCKED) !== LOCKED) {
      throw new Error('Mutex was not locked by caller');
    }
    // Wake up one sleeping waiter
    Atomics.notify(this.view, this.index, 1);
  }
}
```

### Example 4: Asynchronous Waiting on the Main Thread with `Atomics.waitAsync`
```javascript
// Browser Main Thread or UI thread
const sab = new SharedArrayBuffer(4);
const view = new Int32Array(sab);

async function waitForWorkerReady() {
  console.log('Main thread initiating async wait...');

  // Does NOT block the main UI thread!
  const result = Atomics.waitAsync(view, 0, 0);

  if (result.async) {
    // result.value is a Promise
    const outcome = await result.value;
    console.log('Main thread awakened asynchronously:', outcome); // 'ok' or 'timed-out'
  } else {
    console.log('Value changed immediately without waiting:', result.value);
  }
}

waitForWorkerReady();
```

### Example 5: High-Performance Shared Ring Buffer (SPSC Queue)
```javascript
// Single-Producer Single-Consumer lock-free ring buffer
export class SharedRingBuffer {
  constructor(sab, capacity = 256) {
    this.capacity = capacity;
    // Header: index 0 = writeHead, index 1 = readHead
    this.header = new Int32Array(sab, 0, 2);
    // Data buffer
    this.data = new Uint8Array(sab, 8, capacity);
  }

  push(byte) {
    const write = Atomics.load(this.header, 0);
    const read = Atomics.load(this.header, 1);

    if ((write + 1) % this.capacity === read) {
      return false; // Buffer full
    }

    this.data[write] = byte;
    Atomics.store(this.header, 0, (write + 1) % this.capacity);
    return true;
  }

  pop() {
    const write = Atomics.load(this.header, 0);
    const read = Atomics.load(this.header, 1);

    if (read === write) {
      return null; // Buffer empty
    }

    const byte = this.data[read];
    Atomics.store(this.header, 1, (read + 1) % this.capacity);
    return byte;
  }
}
```

---

## 6. Edge Cases, Pitfalls & Gotchas

1. **`Atomics.wait` on Browser Main Thread**: Calling `Atomics.wait` on the main browser thread throws a `TypeError: Atomics.wait cannot be called in this context`. Browsers strictly forbid synchronous blocking of the UI thread. Use `Atomics.waitAsync` instead.
2. **Type Constraints**: `Atomics` functions only accept integer TypedArrays (`Int8Array`, `Uint8Array`, `Int16Array`, `Uint16Array`, `Int32Array`, `Uint32Array`, `BigInt64Array`, `BigUint64Array`). Passing a `Float32Array` or `Float64Array` throws a `TypeError`.
3. **Memory Reordering**: Modern CPUs and JIT compilers aggressively reorder read/write instructions for performance. Standard typed array writes (`view[0] = 5`) provide no memory barrier guarantees across cores. Always use `Atomics.store()` and `Atomics.load()` when publishing or reading synchronization flags.
4. **Missing Isolation Headers in Browsers**: In evergreen browsers, `window.crossOriginIsolated` is `false` unless `COOP` and `COEP` HTTP response headers are set. Without them, `window.SharedArrayBuffer` is `undefined`.

---

## 7. Common Mistakes & Anti-Patterns

| Anti-Pattern | Why It Breaks | Correct Pattern |
| :--- | :--- | :--- |
| `view[0]++` in concurrent workers | Non-atomic read-modify-write causes lost updates | Use `Atomics.add(view, 0, 1)` |
| Spinning `while (view[0] !== 1) {}` | Saturates CPU core at 100%, starving other threads | Use `Atomics.wait()` and `Atomics.notify()` |
| Calling `Atomics.wait` on `Float64Array` | Throws `TypeError`; floats cannot be atomically manipulated | Use `Int32Array` or `BigInt64Array` |
| Forgetting to serve `COOP` / `COEP` headers | `SharedArrayBuffer` is undefined in browser | Configure server headers for Cross-Origin Isolation |
| Assuming `Atomics.notify()` wakes up all waiters | Default waiter count is 1 if unspecified | Pass explicit count or `Infinity` to wake all |
| Modifying shared memory without memory fences | CPU pipeline instruction reordering reads stale data | Use `Atomics.load()` and `Atomics.store()` |

---

## 8. Performance Characteristics

- **Zero Serialization Latency**: $0.00$ms serialization cost. Both workers read directly from identical RAM addresses.
- **Hardware Instruction Mapping**:
  - `Atomics.add` maps directly to `LOCK XADD` on x86/x64 CPUs.
  - `Atomics.compareExchange` maps to `LOCK CMPXCHG`.
  - Latency of atomic operations is roughly 5–15 nanoseconds (CPU cache line invalidation cost).
- **Wakeup Latency**: `Atomics.notify` wakes a sleeping thread via kernel futex calls in ~10–50 microseconds.

---

## 9. Engine Internals

1. **Linux `futex` and Windows `WaitOnAddress`**:
   - `Atomics.wait()` does not burn CPU cycles. V8 delegates to operating system kernel primitives: `futex` (Fast Userspace Mutex) on Linux, and `WaitOnAddress` on Windows.
   - The thread puts itself to sleep in kernel space until the memory address value changes or a wake signal is issued.
2. **Sequentially Consistent Memory Order**:
   - ECMAScript mandates Sequential Consistency for all `Atomics` operations.
   - V8 injects full memory barriers (such as `MFENCE` on x86 or `DMB ISH` on ARM64) to prevent CPU instruction pipelines from reordering loads and stores across the atomic operation boundary.

---

## 10. Best Practices & Design Guidelines

- **Use Dedicated Indices for Synchronization**: Reserve the first few indices of your `SharedArrayBuffer` (e.g., indices 0–3) exclusively for lock flags and counters, leaving subsequent memory for payload bytes.
- **Keep Critical Sections Minimal**: When holding a lock, execute as few instructions as possible and never perform I/O or sleep inside the locked region.
- **Graceful Fallbacks**: Always verify `typeof SharedArrayBuffer !== 'undefined'` in browser applications to gracefully fall back to transferable `ArrayBuffer` message passing.

---

## 11. Real-World Case Study

### High-Throughput Audio Worklet Ring Buffer
```javascript
// Audio DSP shared memory ring buffer between Web Worker and AudioWorklet
class AudioSharedQueue {
  constructor(sab) {
    this.state = new Int32Array(sab, 0, 4); // [writePos, readPos, capacity, droppedFrames]
    this.buffer = new Float32Array(sab, 16);
    this.capacity = (sab.byteLength - 16) / 4;
    Atomics.store(this.state, 2, this.capacity);
  }

  writeAudioFrames(frames) {
    const write = Atomics.load(this.state, 0);
    const read = Atomics.load(this.state, 1);

    const available = (read - write - 1 + this.capacity) % this.capacity;
    if (frames.length > available) {
      Atomics.add(this.state, 3, 1); // Record dropped buffer frame
      return false;
    }

    for (let i = 0; i < frames.length; i++) {
      this.buffer[(write + i) % this.capacity] = frames[i];
    }

    Atomics.store(this.state, 0, (write + frames.length) % this.capacity);
    return true;
  }
}
```

---

## 12. Interview Questions

<details>
<summary>1. Why was SharedArrayBuffer disabled in browsers and what is required to use it today?</summary>
It was disabled in 2018 due to the Spectre side-channel attack, where high-resolution timers built using concurrent shared memory loops could deduce private CPU memory cache states across origins. To re-enable it, web applications must be Cross-Origin Isolated via <code>Cross-Origin-Opener-Policy: same-origin</code> and <code>Cross-Origin-Embedder-Policy: require-corp</code> headers.
</details>

<details>
<summary>2. What is the difference between Atomics.add() and standard typedArray[0] += 1?</summary>
<code>typedArray[0] += 1</code> consists of separate read, add, and write operations. In a multi-threaded environment, another thread can interleave and mutate the value between the read and write, leading to lost updates. <code>Atomics.add()</code> executes as an atomic, indivisible hardware instruction with a memory barrier.
</details>

<details>
<summary>3. Why does Atomics.wait() throw an exception on the main thread in browsers?</summary>
<code>Atomics.wait()</code> blocks the executing thread synchronously until awakened. Blocking the main thread would freeze the browser UI, stop user input handling, and halt screen paints, causing the page to become completely unresponsive.
</details>

<details>
<summary>4. How does Atomics.compareExchange work?</summary>
It takes a typed array, index, expected value, and replacement value. If the current value at that index equals the expected value, it replaces it with the replacement value atomically. It always returns the <i>original</i> value that was at that index.
</details>

<details>
<summary>5. What underlying OS primitives power Atomics.wait and Atomics.notify?</summary>
On Linux, it uses the <code>futex</code> (Fast Userspace Mutex) system call. On Windows, it uses <code>WaitOnAddress</code> and <code>WakeByAddressSingle</code>. These primitives put threads to sleep in the kernel without burning CPU cycles.
</details>

<details>
<summary>6. Can you use Atomics operations on Float32Array or Float64Array?</summary>
No. The ECMAScript specification restricts <code>Atomics</code> operations strictly to integer TypedArrays (e.g., <code>Int32Array</code>, <code>BigInt64Array</code>). Attempting to use them on floating-point arrays throws a <code>TypeError</code>.
</details>

<details>
<summary>7. What is Atomics.waitAsync and where is it useful?</summary>
<code>Atomics.waitAsync()</code> is a non-blocking alternative to <code>Atomics.wait()</code>. It returns an object containing a Promise that resolves when another thread calls <code>Atomics.notify()</code> or when a timeout expires. It is allowed on the browser main thread.
</details>

<details>
<summary>8. What is memory tearing and how does Atomics prevent it?</summary>
Memory tearing occurs when a thread reads a multi-byte memory location while another thread is halfway through writing it, reading a corrupted mixture of old and new bytes. <code>Atomics.load()</code> and <code>Atomics.store()</code> guarantee indivisible 32-bit or 64-bit aligned reads and writes.
</details>

---

## 13. Exercises & Challenges

### Exercise 1: Build a Ticket Lock (FIFO Fair Mutex)
Implement a fair ticket-lock using `Atomics.add` and `Atomics.wait` on a `SharedArrayBuffer` ensuring that threads acquire the lock in the exact order they requested it (no starvation).

<details>
<summary>View Solution</summary>

```javascript
export class FairTicketLock {
  constructor(sab, offset = 0) {
    // index 0: ticketCounter, index 1: nowServing
    this.view = new Int32Array(sab, offset, 2);
  }

  acquire() {
    // Take next ticket atomically
    const myTicket = Atomics.add(this.view, 0, 1);

    // Wait until it's our turn
    while (true) {
      const serving = Atomics.load(this.view, 1);
      if (serving === myTicket) {
        return; // Our turn!
      }
      Atomics.wait(this.view, 1, serving);
    }
  }

  release() {
    // Advance nowServing and wake all waiting threads
    Atomics.add(this.view, 1, 1);
    Atomics.notify(this.view, 1, Infinity);
  }
}
```
</details>

### Exercise 2: Cross-Thread Atomic Countdown Latch
Implement a `CountdownLatch` synchronization helper using `SharedArrayBuffer` that allows one or more threads to wait until a set of operations performed in other threads completes.

<details>
<summary>View Solution</summary>

```javascript
export class CountdownLatch {
  constructor(sab, count, offset = 0) {
    this.view = new Int32Array(sab, offset, 1);
    Atomics.store(this.view, 0, count);
  }

  countDown() {
    const remaining = Atomics.sub(this.view, 0, 1) - 1;
    if (remaining <= 0) {
      Atomics.notify(this.view, 0, Infinity);
    }
  }

  await() {
    while (true) {
      const current = Atomics.load(this.view, 0);
      if (current <= 0) {
        return;
      }
      Atomics.wait(this.view, 0, current);
    }
  }
}
```
</details>
