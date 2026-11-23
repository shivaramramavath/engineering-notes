# Concurrency vs Parallelism in JavaScript

> Architectural comparison of single-threaded I/O concurrency versus multi-core CPU parallelism across browser and Node.js runtimes.

| Metadata | Details |
| :--- | :--- |
| **Level** | Intermediate to Advanced |
| **Prerequisites** | [Event Loop](file:///e:/Learning/engineering-notes/01-languages/javascript/12_event-loop/01_event-loop.md), [Asynchronous JavaScript](file:///e:/Learning/engineering-notes/01-languages/javascript/11_asynchronous-javascript/01_asynchronous-model.md) |
| **Time to read** | 22 minutes |
| **Status** | Complete |
| **Standard / Runtime** | ECMAScript 2015+, Node.js, Evergreen Browsers |

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

In software engineering, the terms *concurrency* and *parallelism* are frequently conflated, yet they address fundamentally different computing bottlenecks:
- **Concurrency** is about **structure**: dealing with many things at once by interleaving their execution steps during wait periods.
- **Parallelism** is about **execution**: doing many things simultaneously at the exact same physical instant across multiple CPU cores.

JavaScript was engineered as a single-threaded runtime optimized for non-blocking I/O concurrency. One call stack cooperatively handles thousands of concurrent socket or network connections. However, when an application encounters computationally intensive tasks—such as image manipulation, cryptographic operations, syntax parsing, or heavy numeric crunching—a single thread becomes a catastrophic bottleneck. Parallelism mechanisms (Web Workers, Node.js Worker Threads, Cluster, and child processes) are required to offload CPU-bound workloads without freezing the main user interface or server event loop.

---

## 2. Core Concepts

### Concurrency Without Parallelism
JavaScript achieves high concurrency on a single physical thread through the **Event Loop** and non-blocking asynchronous APIs. When code initiates an asynchronous I/O request (such as `fetch()` or `fs.promises.readFile()`), the execution yields control back to the call stack while the operating system kernel handles the data transfer. Once complete, the callback or resolved promise microtask is scheduled back onto the event loop. At no point do two lines of JavaScript execute in parallel on the same thread.

### CPU-Bound vs I/O-Bound Workloads
- **I/O-Bound Tasks**: Spend the vast majority of wall-clock time waiting for external hardware or network events (e.g., HTTP requests, database queries, disk reads). They benefit immensely from single-threaded asynchronous concurrency.
- **CPU-Bound Tasks**: Spend execution time saturating the CPU arithmetic logic units (e.g., matrix multiplication, JSON compression, image filtering). Asynchronous wrappers (`async`/`await`) do **not** make CPU-bound work non-blocking; they merely delay when the thread freezes. They demand physical multi-core parallelism.

### Amdahl's Law and Parallel Scalability
The theoretical speedup of an application when using parallel processors is governed by Amdahl's Law:
$$S = \frac{1}{(1 - P) + \frac{P}{N}}$$
Where:
- $S$ is the theoretical speedup factor.
- $P$ is the fraction of code that can be parallelized.
- $N$ is the number of execution units (CPU cores).
Even with infinite processor cores, the maximum speedup is strictly capped by the serial, non-parallelizable portion $(1 - P)$ of the workload.

---

## 3. How It Works Internally

```
CONCURRENCY (Single-Threaded Interleaving)
Thread 1 (Main Stack):
+-------+   wait   +-------+   wait   +-------+
| Task A|--------->| Task B|--------->| Task A|---> Time
+-------+          +-------+          +-------+
    |                  |                  |
    v                  v                  v
[I/O Wait]        [I/O Wait]         [I/O Done]

PARALLELISM (Multi-Core Simultaneous Execution)
Core 0 (Thread 1):
+---------------------------------------------+
| Task A (V8 Isolate 1)                       |---> Time
+---------------------------------------------+

Core 1 (Thread 2):
+---------------------------------------------+
| Task B (V8 Isolate 2)                       |---> Time
+---------------------------------------------+
```

Each parallel worker in JavaScript (Web Worker or `worker_threads`) runs an isolated V8 runtime with its own:
1. Call Stack
2. V8 Memory Heap (no shared pointer references by default)
3. Event Loop and Microtask Queue
4. Garbage Collector instance

Communication between isolates is achieved either through asynchronous message-passing via the **Structured Clone Algorithm** or zero-copy shared memory via **`SharedArrayBuffer`** with atomic synchronization.

---

## 4. Syntax & API Reference

### Comparing Parallelism Options

| Environment | Mechanism | Communication | Memory Model | Creation Cost |
| :--- | :--- | :--- | :--- | :--- |
| **Browser** | Web Workers (`Worker`) | `postMessage` | Isolated heap; `ArrayBuffer` transfer | Moderate (~2-5 MB) |
| **Browser** | Service Workers | `postMessage` | Event-driven lifecycle; isolated | Low/dormant |
| **Node.js** | `worker_threads` | `parentPort.postMessage` | Isolated isolate; `SharedArrayBuffer` | Fast (~1-2 MB) |
| **Node.js** | `child_process` | IPC socket pipes | Separate OS processes | Heavy (~30 MB) |
| **Node.js** | `cluster` | IPC socket passing | Separate OS processes | Heavy (~30 MB) |

---

## 5. Practical Examples

### Example 1: Demonstrating That `async` Does NOT Prevent CPU Freezing
```javascript
// WRONG: Wrapping heavy CPU work in async does NOT make it parallel or non-blocking
function cpuIntensiveFibonacci(n) {
  if (n <= 1) return n;
  return cpuIntensiveFibonacci(n - 1) + cpuIntensiveFibonacci(n - 2);
}

async function fakeAsyncCompute(n) {
  // The event loop is completely FROZEN while this line executes!
  return cpuIntensiveFibonacci(n);
}

// Any timer scheduled will be delayed by the entire computation duration
setTimeout(() => console.log('Timer fired!'), 10);

console.time('compute');
await fakeAsyncCompute(42); // Blocks main thread for ~1.5s
console.timeEnd('compute');
// 'Timer fired!' only logs AFTER fakeAsyncCompute finishes!
```

### Example 2: Concurrent I/O Batching with Concurrency Limits
```javascript
// High concurrency without overwhelming sockets
async function pMap(items, mapper, concurrency = 4) {
  const results = new Array(items.length);
  let currentIndex = 0;

  async function worker() {
    while (currentIndex < items.length) {
      const index = currentIndex++;
      results[index] = await mapper(items[index], index);
    }
  }

  const workers = Array.from({ length: Math.min(concurrency, items.length) }, () => worker());
  await Promise.all(workers);
  return results;
}

// Usage: 100 concurrent downloads limited to 5 active sockets
const urls = Array.from({ length: 100 }, (_, i) => `https://api.example.com/item/${i}`);
const data = await pMap(urls, async (url) => {
  const res = await fetch(url);
  return res.json();
}, 5);
```

### Example 3: Simulating Concurrency vs Parallelism Benchmarks
```javascript
import { performance } from 'node:perf_hooks';

function blockCpu(ms) {
  const start = performance.now();
  while (performance.now() - start < ms) {
    // Busy wait simulating intensive computation
  }
}

// 1. Concurrent I/O simulation (Promise.all)
console.time('Concurrent I/O');
await Promise.all([
  new Promise(r => setTimeout(r, 100)),
  new Promise(r => setTimeout(r, 100)),
  new Promise(r => setTimeout(r, 100)),
]);
console.timeEnd('Concurrent I/O'); // ~100ms total wall time

// 2. Sequential CPU work (Single-threaded)
console.time('Sequential CPU');
blockCpu(100);
blockCpu(100);
blockCpu(100);
console.timeEnd('Sequential CPU'); // ~300ms total wall time
```

### Example 4: Offloading Work to a Web Worker (Browser Parallelism)
```javascript
// main.js
const worker = new Worker(new URL('./worker.js', import.meta.url), { type: 'module' });

function calculatePrimes(max) {
  return new Promise((resolve) => {
    worker.onmessage = (event) => resolve(event.data);
    worker.postMessage({ max });
  });
}

const primes = await calculatePrimes(10_000_000);
console.log('Main thread remained 100% fluid and responsive!');

// worker.js
self.onmessage = (e) => {
  const { max } = e.data;
  const primes = [];
  for (let i = 2; i <= max; i++) {
    let isPrime = true;
    for (let j = 2; j * j <= i; j++) {
      if (i % j === 0) { isPrime = false; break; }
    }
    if (isPrime) primes.push(i);
  }
  self.postMessage(primes);
};
```

### Example 5: Zero-Copy Memory Sharing via Transferable Objects
```javascript
// Allocating 64MB of data
const buffer = new ArrayBuffer(64 * 1024 * 1024);
const u8View = new Uint8Array(buffer);
u8View.fill(42);

console.log('Main thread buffer length before transfer:', buffer.byteLength); // 67108864

// Transfer ownership to worker without cloning bytes
worker.postMessage({ data: buffer }, [buffer]);

// Ownership is completely detached from the main thread!
console.log('Main thread buffer length after transfer:', buffer.byteLength); // 0 (detached)
```

---

## 6. Edge Cases, Pitfalls & Gotchas

1. **Structured Clone Serialization Overhead**: When passing large objects or deep trees via `postMessage()`, the V8 engine serializes the data structure into an intermediate binary buffer and reconstructs it in the target isolate. For multi-megabyte payloads, serialization CPU overhead can exceed the time saved by parallel execution.
2. **Context Switching and Thread Saturation**: Spawning more worker threads than physical CPU cores causes excessive OS thread context switching, cache thrashing, and degraded throughput. Always bound workers to `os.availableParallelism()`.
3. **Deadlocks in Lock-Free Constructs**: When using `SharedArrayBuffer` with `Atomics.wait()`, calling `Atomics.wait()` on the browser main thread throws an exception by spec to prevent permanently hanging the UI rendering pipeline.
4. **Detached Buffers**: Attempting to read or write to an `ArrayBuffer` after it has been transferred in a `postMessage([buffer])` call throws `TypeError: Cannot perform operation on a detached ArrayBuffer`.

---

## 7. Common Mistakes & Anti-Patterns

| Anti-Pattern | Why It Breaks | Correct Pattern |
| :--- | :--- | :--- |
| Wrapping CPU-bound loops in `new Promise()` | Executes synchronously on the current call stack before returning | Offload computation to a Worker Thread |
| Creating a new Worker for every HTTP request | High thread creation and V8 isolate startup latency | Maintain a reusable Worker Pool |
| Transferring DOM nodes to Web Workers | DOM nodes cannot be cloned or transferred across thread boundaries | Send raw data or transfer OffscreenCanvas |
| Unbounded `Promise.all()` over thousands of tasks | Exhausts network socket pools, memory, and database connections | Use a concurrency-limiting mapper |
| Calling `Atomics.wait` on browser main thread | Throws `TypeError` because main thread must never sleep | Execute atomic waits exclusively inside Workers |
| Relying on `async` functions to yield automatically | `async` functions execute synchronously until the first `await` | Insert cooperative micro-yields via `setImmediate` or `scheduler.yield()` |

---

## 8. Performance Characteristics

- **Worker Thread Instantiation Overhead**: Starting a V8 isolate takes 5–20ms and allocates 1–3 MB of baseline RAM.
- **Structured Clone Throughput**: Roughly 100–300 MB/s. Transferring `ArrayBuffer` handles operates in sub-microsecond $O(1)$ constant time regardless of buffer size.
- **Thread Scaling Limits**: CPU-bound tasks achieve near-linear speedups up to the physical core count, after which hyper-threading provides diminishing returns (typically 15–30% additional throughput).

---

## 9. Engine Internals

1. **V8 Isolates**: An isolate represents an independent instance of the V8 engine with its own heap manager, garbage collector, and compilation pipeline. Two isolates cannot touch each other's heap objects directly because their memory spaces are mapped independently.
2. **Transferable Objects Mechanism**: When an `ArrayBuffer` is transferred, V8 clears the pointer in the sender's `BackingStore` struct (setting byte length to 0 and marking the buffer detached) and moves the raw pointer directly into the recipient's `BackingStore` without allocating or copying heap memory.

---

## 10. Best Practices & Design Guidelines

- **Task Granularity Rule**: Only delegate CPU tasks to parallel workers if execution time exceeds 10–20ms. For smaller tasks, structured clone overhead exceeds execution time.
- **Use Cooperative Yielding for Intermediate Tasks**: In environments where worker threads cannot be spawned, chunk long-running loops using `scheduler.yield()` or `setTimeout(..., 0)` to allow the event loop to service render frames and input events.
- **Zero-Copy Architecture**: Prefer `Transferable` typed arrays or `SharedArrayBuffer` for high-frequency data pipelines (audio processing, video streaming, canvas rendering).

---

## 11. Real-World Case Study

### Chunked Cooperative Task Scheduling (Main Thread Yielding)
```javascript
// Breaking a massive 1,000,000 item loop into non-blocking chunks
async function processLargeDatasetConcurrently(items, processItem) {
  const CHUNK_SIZE = 5000;
  let index = 0;

  while (index < items.length) {
    const end = Math.min(index + CHUNK_SIZE, items.length);

    // Process chunk synchronously
    for (; index < end; index++) {
      processItem(items[index]);
    }

    // Cooperative yield: Allow event loop to process user clicks and renders
    if (globalThis.scheduler?.yield) {
      await scheduler.yield();
    } else {
      await new Promise(resolve => setTimeout(resolve, 0));
    }
  }
}
```

---

## 12. Interview Questions

<details>
<summary>1. What is the fundamental difference between concurrency and parallelism?</summary>
Concurrency is the ability to handle multiple tasks by interleaving their execution over time (managing multiple things at once), which can occur on a single CPU core. Parallelism is the simultaneous execution of multiple tasks at the exact same instant across multiple physical CPU cores.
</details>

<details>
<summary>2. Why does wrapping a heavy synchronous loop in an async function fail to prevent UI lag?</summary>
An <code>async</code> function executes completely synchronously on the main thread until it reaches its first <code>await</code> expression. If a function contains a synchronous <code>while</code> loop or heavy recursive algorithm, it monopolizes the call stack and freezes the event loop regardless of whether it is marked <code>async</code>.
</details>

<details>
<summary>3. What is the Structured Clone Algorithm?</summary>
It is the native algorithm used by the browser and Node.js to serialize and duplicate complex JavaScript objects across execution contexts (like Web Workers and IPC). Unlike JSON serialization, it handles circular references, typed arrays, Maps, Sets, RegExps, Blobs, and Dates.
</details>

<details>
<summary>4. What are Transferable Objects?</summary>
Transferable Objects (such as <code>ArrayBuffer</code>, <code>MessagePort</code>, and <code>ImageBitmap</code>) allow zero-copy memory transfer between threads. The memory ownership is detached from the sender and attached to the receiver in $O(1)$ constant time without duplicating underlying bytes.
</details>

<details>
<summary>5. Why is Atomics.wait() disallowed on the main thread in browsers?</summary>
<code>Atomics.wait()</code> blocks the executing thread synchronously until awakened by another thread. Allowing it on the main browser thread would freeze the browser UI, user input processing, and screen rendering, resulting in an unresponsive browser tab.
</details>

<details>
<summary>6. How does Amdahl's Law apply to JavaScript worker pools?</summary>
Amdahl's law demonstrates that the speedup gained from adding parallel worker threads is strictly limited by the portion of the program that must run sequentially (such as data serialization, main thread aggregation, and DOM rendering).
</details>

<details>
<summary>7. What happens to an ArrayBuffer after it has been transferred?</summary>
Its underlying memory buffer is detached. The buffer's <code>byteLength</code> becomes 0, and any subsequent read or write attempt on its typed array views throws a <code>TypeError</code>.
</details>

<details>
<summary>8. When should you choose worker_threads over child_process in Node.js?</summary>
Use <code>worker_threads</code> for CPU-intensive JavaScript tasks where you want low memory footprint (~1-2MB vs ~30MB), fast startup times, and the ability to share memory via <code>SharedArrayBuffer</code>. Use <code>child_process</code> when executing non-Node system binaries or when you require strict operating-system-level process isolation.
</details>

---

## 13. Exercises & Challenges

### Exercise 1: Build a Concurrency Throttler
Implement an asynchronous function `throttleConcurrency(tasks, limit)` that executes an array of asynchronous factory functions `() => Promise<T>`, ensuring that no more than `limit` promises are active concurrently, and returning the aggregated results in their original order.

<details>
<summary>View Solution</summary>

```javascript
async function throttleConcurrency(tasks, limit) {
  const results = new Array(tasks.length);
  let nextTaskIndex = 0;

  async function worker() {
    while (nextTaskIndex < tasks.length) {
      const index = nextTaskIndex++;
      results[index] = await tasks[index]();
    }
  }

  const pool = Array.from({ length: Math.min(limit, tasks.length) }, () => worker());
  await Promise.all(pool);
  return results;
}
```
</details>

### Exercise 2: Benchmarking Structured Clone vs ArrayBuffer Transfer
Write a Node.js or browser benchmark measuring the exact millisecond cost of cloning a 32MB `Uint8Array` versus transferring it across a `MessageChannel`.

<details>
<summary>View Solution</summary>

```javascript
import { MessageChannel } from 'node:worker_threads';
import { performance } from 'node:perf_hooks';

const { port1, port2 } = new MessageChannel();

// Setup receiver
port2.on('message', () => {});

const SIZE = 32 * 1024 * 1024; // 32MB

// Benchmark 1: Structured Clone
const cloneBuffer = new ArrayBuffer(SIZE);
const startClone = performance.now();
port1.postMessage({ buffer: cloneBuffer }); // Cloned by default
const cloneDuration = performance.now() - startClone;
console.log(`Structured Clone took: ${cloneDuration.toFixed(2)}ms`);

// Benchmark 2: Transferable zero-copy
const transferBuffer = new ArrayBuffer(SIZE);
const startTransfer = performance.now();
port1.postMessage({ buffer: transferBuffer }, [transferBuffer]); // Transferred
const transferDuration = performance.now() - startTransfer;
console.log(`Zero-Copy Transfer took: ${transferDuration.toFixed(2)}ms`);

port1.close();
port2.close();
```
</details>
