# JavaScript Sync vs Async: Single-Threaded Model, Non-Blocking I/O & Event Offloading

> Comprehensive technical reference on ECMAScript synchronous run-to-completion semantics, asynchronous non-blocking I/O, Web APIs, Node.js libuv event offloading, and event loop starvation.

| Metadata | Details |
| :--- | :--- |
| **Level** | Core / Engine Architecture |
| **Prerequisites** | [04_scope-and-execution/03_execution-context-and-call-stack.md](../04_scope-and-execution/03_execution-context-and-call-stack.md) |
| **Time to Read** | 25 minutes |
| **Status** | Complete |
| **ECMAScript Baseline** | ES5 / ES2015 (Promises) / HTML & Node.js Event Loop Standard |

---

## Table of Contents
1. [What & Why](#1-what--why)
2. [Core Concepts: Single-Threaded Execution & Non-Blocking I/O](#2-core-concepts-single-threaded-execution--non-blocking-io)
3. [How It Works: Engine Architecture & Host Environment Offloading](#3-how-it-works-engine-architecture--host-environment-offloading)
4. [Syntax & Operational Comparisons](#4-syntax--operational-comparisons)
5. [Practical Examples](#5-practical-examples)
6. [Edge Cases, Pitfalls & Gotchas](#6-edge-cases-pitfalls--gotchas)
7. [Common Mistakes & Anti-Patterns](#7-common-mistakes--anti-patterns)
8. [Performance Characteristics: Event Loop Starvation](#8-performance-characteristics-event-loop-starvation)
9. [Engine Internals: V8 Isolate & Libuv Thread Pool](#9-engine-internals-v8-isolate--libuv-thread-pool)
10. [Best Practices & Design Guidelines](#10-best-practices--design-guidelines)
11. [Real-World Case Study: Diagnosing & Remediating Event Loop Starvation in Node.js](#11-real-world-case-study-diagnosing--remediating-event-loop-starvation-in-nodejs)
12. [Interview Questions & Deep Dives](#12-interview-questions--deep-dives)
13. [Exercises & Challenges](#13-exercises--challenges)

---

## 1. What & Why

JavaScript is fundamentally a **single-threaded, non-blocking, asynchronous concurrent programming language**. It possesses exactly **one call stack** and **one memory heap** per execution realm (isolate).

In traditional multi-threaded programming environments (like Java, C++, or Go), a slow operation (such as waiting for a database query, reading a 1 GB file from disk, or requesting data from an external HTTP server) simply blocks the executing OS thread. The operating system pauses the thread and switches to another thread:
```c
// Blocking Synchronous I/O in C:
int bytes = read(socket_fd, buffer, 1024); // Thread sleeps until network packets arrive!
```

If JavaScript behaved this way:
- In the browser, the entire user interface would **freeze completely**. Buttons would not respond to clicks, animations would halt, and the browser would display a "Page Unresponsive" dialog.
- In Node.js, the server would handle only **one single HTTP request at a time**. All other concurrent incoming requests would be queued until the active request completed, crippling throughput.

To achieve massive concurrency without the complexities of thread synchronization, race conditions, and deadlocks, JavaScript employs **Non-Blocking Asynchronous I/O**. Heavy operations are offloaded to host environment background threads (browser Web APIs or Node.js libuv), while the JavaScript call stack remains unblocked and ready to process user interactions and incoming requests.

---

## 2. Core Concepts: Single-Threaded Execution & Non-Blocking I/O

### 2.1 The Run-to-Completion Invariant
Every synchronous JavaScript function executes under the **Run-to-Completion** guarantee:
Once a function begins executing on the call stack, no other JavaScript code (no timers, no click handlers, no network responses) can preempt or interrupt it until the function returns and its stack frame is popped.

```javascript
function heavyCompute() {
  const start = Date.now();
  while (Date.now() - start < 3000) {
    // Blocks the entire call stack for 3 full seconds!
  }
}

setTimeout(() => console.log("Timer fired!"), 100);
heavyCompute();
// "Timer fired!" cannot execute until after heavyCompute() completes,
// even though the timer delay was only 100ms!
```

### 2.2 Synchronous vs Asynchronous Operations
- **Synchronous:** The caller waits until the operation produces a result before proceeding to the next line of code. Execution is strictly sequential.
- **Asynchronous:** The operation initiates a task, registers a continuation (callback, Promise, or async/await), and returns control **immediately** to the caller. The result is delivered on a future tick of the event loop.

---

## 3. How It Works: Engine Architecture & Host Environment Offloading

JavaScript itself (the V8 engine) does not have native networking, timer, or disk access capabilities. Those capabilities are provided by the **Host Environment** (the Browser or Node.js runtime):

```
+-----------------------------------------------------------------------------------------+
|                                    JavaScript Engine (V8)                               |
|                                                                                         |
|       +-----------------------------------+     +-----------------------------------+   |
|       |         Memory Heap               |     |            Call Stack             |   |
|       | Objects, Closures, Variables      |     | Active Function Execution Frames  |   |
|       +-----------------------------------+     +-----------------------------------+   |
+--------------------------------------------------------------------|--------------------+
                                                                     | Offload Async API
                                                                     v
+-----------------------------------------------------------------------------------------+
|                        Host Environment (Browser Web APIs / Node.js libuv)              |
|                                                                                         |
|  - Timers (setTimeout, setInterval)   - Network Sockets (fetch, http, WebSockets)       |
|  - File System I/O (fs.readFile)       - Operating System Polling (epoll, kqueue, IOCP) |
+--------------------------------------------------------------------|--------------------+
                                                                     | Event Complete
                                                                     v
+-----------------------------------------------------------------------------------------+
|                                      Task Queues                                        |
|                                                                                         |
|  Microtask Queue (High Priority): Promise .then/catch, queueMicrotask, MutationObserver |
|  Macrotask Queue (Standard Priority): setTimeout, setInterval, setImmediate, I/O events  |
+--------------------------------------------------------------------|--------------------+
                                                                     | Dequeue onto Stack
                                                                     v
                                                            [Event Loop Tick]
```

### The Async Offloading Lifecycle:
1. `fetch("https://api.com/data")` is called on the Call Stack.
2. V8 hands the network request off to the host environment's C++ network layer (Chromium network stack or libuv).
3. The Call Stack pops `fetch()` immediately. The thread continues executing the remaining lines of JavaScript code.
4. When the network response packets arrive, the host environment places the resolution callback into the **Microtask Queue**.
5. When the Call Stack becomes completely empty, the **Event Loop** dequeues the callback and pushes it onto the Call Stack for execution.

---

## 4. Syntax & Operational Comparisons

| Operation Domain | Synchronous (Blocking) API | Asynchronous (Non-Blocking) API |
| :--- | :--- | :--- |
| **Timers / Delay** | `while(Date.now() < target)` (Freezes thread) | `setTimeout(cb, ms)` or `await delay(ms)` |
| **Node.js File I/O** | `fs.readFileSync(path)` | `fs.promises.readFile(path)` |
| **HTTP Requests** | `XMLHttpRequest` (with `async: false` - deprecated) | `fetch(url)` or `axios.get(url)` |
| **Encryption / Hash**| `crypto.pbkdf2Sync(...)` | `crypto.pbkdf2(...)` (Offloaded to libuv threads) |
| **JSON Operations** | `JSON.parse(str)` (Always sync!) | Web Worker streaming JSON / chunked parser |

---

## 5. Practical Examples

### Example 1: Non-Blocking Sleep Utility vs Blocking Busy-Wait
```javascript
// DANGEROUS: Synchronous busy-wait freezes the entire application
function busyWaitBlocking(ms) {
  const end = Date.now() + ms;
  while (Date.now() < end) {
    // 100% CPU burn! Completely locks the thread!
  }
}

// IDIOMATIC: Asynchronous non-blocking sleep
function sleep(ms) {
  return new Promise(resolve => setTimeout(resolve, ms));
}

async function demonstrateConcurrency() {
  console.log("Start async sleep...");
  
  // While sleeping for 500ms, the event loop can process other events!
  const taskA = sleep(500).then(() => console.log("Task A finished after 500ms"));
  const taskB = sleep(200).then(() => console.log("Task B finished after 200ms"));

  await Promise.all([taskA, taskB]);
  console.log("All concurrent tasks finished!");
}

demonstrateConcurrency();
// Outputs:
// Start async sleep...
// Task B finished after 200ms
// Task A finished after 500ms
// All concurrent tasks finished!
```

### Example 2: Chunking CPU-Intensive Loops to Prevent Event Loop Starvation
If you must calculate a massive computation (e.g. processing 10,000,000 numbers), running it in a single synchronous loop freezes the UI:
```javascript
// Chunking heavy work across event loop ticks using setTimeout(..., 0) or setImmediate:
function processLargeDatasetChunked(items, chunkSize = 1000, onProgress) {
  return new Promise((resolve) => {
    let index = 0;
    const total = items.length;
    let sum = 0;

    function processNextChunk() {
      const chunkEnd = Math.min(index + chunkSize, total);
      
      // Execute a small batch synchronously:
      for (; index < chunkEnd; index++) {
        sum += items[index];
      }

      onProgress(index / total);

      if (index < total) {
        // Yield execution to the event loop so I/O and UI rendering can occur!
        setTimeout(processNextChunk, 0); // In Node.js: setImmediate(processNextChunk)
      } else {
        resolve(sum);
      }
    }

    processNextChunk();
  });
}

const largeArray = new Array(50_000).fill(1);
processLargeDatasetChunked(largeArray, 10_000, (pct) => {
  console.log(`Progress: ${(pct * 100).toFixed(0)}%`);
}).then(total => console.log("Finished sum:", total));
```

### Example 3: The "Sync Inside Async" Gotcha
A function marked with the `async` keyword does **not** run on a background thread; its body executes synchronously until the first `await` is encountered!
```javascript
async function synchronousStart() {
  console.log("1. Inside async function - SYNC");
  
  // This loop runs synchronously on the main thread!
  for (let i = 0; i < 1e8; i++) {} 
  console.log("2. Still synchronous before await");

  await Promise.resolve(); // Yields control here!
  console.log("4. After await - ASYNC microtask");
}

console.log("0. Before call");
synchronousStart();
console.log("3. Outside function - after call returned promise");

// Output Order:
// 0. Before call
// 1. Inside async function - SYNC
// 2. Still synchronous before await
// 3. Outside function - after call returned promise
// 4. After await - ASYNC microtask
```

### Example 4: Measuring Event Loop Lag (Latency Profiling)
We can measure how severely the synchronous call stack is delaying asynchronous execution by checking timer drift:
```javascript
class EventLoopLagMonitor {
  constructor(sampleIntervalMs = 500) {
    this.sampleIntervalMs = sampleIntervalMs;
    this.timer = null;
  }

  start(onLagDetected) {
    let expectedTime = Date.now() + this.sampleIntervalMs;

    this.timer = setInterval(() => {
      const now = Date.now();
      const lagMs = now - expectedTime;
      expectedTime = now + this.sampleIntervalMs;

      // If lag exceeds 20ms, synchronous work is blocking the event loop:
      if (lagMs > 20) {
        onLagDetected(lagMs);
      }
    }, this.sampleIntervalMs);
  }

  stop() {
    clearInterval(this.timer);
  }
}

const monitor = new EventLoopLagMonitor(200);
monitor.start((lag) => console.warn(`[WARNING] Event loop blocked by ${lag}ms!`));
```

### Example 5: High-Concurrency Network Requests (Sync vs Async Throughput)
```javascript
// Synchronous sequential model (Worst): Total time = Sum of all request times
// Asynchronous concurrent model (Optimal): Total time = Max time of slowest request

async function fetchConcurrentMetrics(urls) {
  const startTime = Date.now();

  // All network requests are dispatched in parallel:
  const promises = urls.map(url => {
    // Emulated network latency (100ms - 300ms)
    const delay = 100 + Math.random() * 200;
    return new Promise(res => setTimeout(() => res({ url, time: delay }), delay));
  });

  const results = await Promise.all(promises);
  const totalElapsed = Date.now() - startTime;

  console.log(`Fetched ${results.length} endpoints concurrently in ${totalElapsed}ms.`);
  return results;
}

fetchConcurrentMetrics(["/api/users", "/api/orders", "/api/inventory"]);
```

---

## 6. Edge Cases, Pitfalls & Gotchas

### 6.1 `JSON.parse()` on Huge Payloads Blocks the Thread
`JSON.parse()` and `JSON.stringify()` are **100% synchronous**. Parsing a 50 MB JSON file on the main thread takes 100–300 ms, freezing all incoming HTTP requests during that time:
```javascript
// Freezes the Node.js event loop:
const hugeData = JSON.parse(raw50MbString); 
```
**Fix:** In Node.js, offload massive JSON parsing tasks to a Worker Thread via `worker_threads`.

### 6.2 Regex Catastrophic Backtracking (ReDoS)
A regular expression with nested quantifiers executing against a malicious string blocks the single JavaScript thread indefinitely, leading to a complete service outage.

### 6.3 Calling Synchronous File APIs in Node.js HTTP Routes
Using `fs.readFileSync()` inside an Express or Fastify route handler forces all concurrent users to wait while the physical disk spins and reads data. Always use `fs.promises.readFile()`.

---

## 7. Common Mistakes & Anti-Patterns

| Anti-Pattern | Bad Practice | Idiomatic Solution | Reason |
| :--- | :--- | :--- | :--- |
| **Using `Sync` Methods in Servers** | `fs.readFileSync(path)` in Express routes | `await fs.promises.readFile(path)` | Blocks the single thread, preventing all other users from being served. |
| **Busy-Waiting Loops** | `while(Date.now() < t) {}` | `await new Promise(r => setTimeout(r, ms))` | Spikes CPU to 100% and halts all asynchronous event processing. |
| **Assuming `async` Runs on Threads** | Assuming CPU math inside `async` is offloaded | Use `Worker` threads for heavy CPU math | `async` functions execute on the main thread until the first `await`. |
| **Sequencing Independent Async Calls** | `await fetchA(); await fetchB();` | `await Promise.all([fetchA(), fetchB()])` | Doubles the total latency by waiting for independent tasks sequentially. |
| **Ignoring Event Loop Lag** | Deploying CPU-heavy Node apps without monitoring | Track event loop delay via `perf_hooks` | Causes hidden HTTP latency spikes under heavy production traffic. |
| **Unbounded Microtask Queuing** | Infinite `queueMicrotask()` recursive loop | Use `setTimeout` or `setImmediate` | Microtasks starve macrotasks, preventing I/O and rendering completely. |

---

## 8. Performance Characteristics: Event Loop Starvation

1. **The Single-Thread Bottleneck:**
   - A single Node.js instance can easily handle **50,000+ concurrent idle connections** (e.g. WebSockets) because idle connections consume zero CPU time.
   - However, a single synchronous CPU operation taking 1,000 ms will delay all 50,000 users by 1 full second.
2. **Context Switching vs Event Loop:**
   - Multi-threaded servers (e.g., Apache) create a thread per request, incurring heavy OS context switching overhead (~1–2 KB stack memory and CPU cache invalidations).
   - Node.js event-driven non-blocking I/O uses zero context switching, achieving higher throughput for I/O-bound workloads.

---

## 9. Engine Internals: V8 Isolate & Libuv Thread Pool

Inside Node.js, the runtime is split between V8 and **libuv**:

```
+-----------------------------------------------------------------------------------------+
|                                    Node.js Architecture                                 |
+-----------------------------------------------------------------------------------------+
| JavaScript Layer: V8 Engine (Single Main Execution Thread)                             |
+-----------------------------------------------------------------------------------------+
| libuv C++ Layer:                                                                        |
|                                                                                         |
|  1. Non-Blocking System Calls (Epoll / Kqueue / Event Ports):                           |
|     - TCP/UDP Network Sockets, Timers, HTTP Connections                                 |
|     - Handled directly by OS kernel events with ZERO thread allocation.                 |
|                                                                                         |
|  2. Libuv Thread Pool (Default size: 4 threads, configurable via UV_THREADPOOL_SIZE):   |
|     - File System I/O (`fs.*`)                                                          |
|     - DNS Resolution (`dns.lookup`)                                                     |
|     - Cryptographic hashing / PBKDF2 (`crypto.*`)                                       |
|     - Compression (`zlib.*`)                                                            |
+-----------------------------------------------------------------------------------------+
```

### Key Engine Takeaway:
Network requests in Node.js do **not** consume threads in the libuv thread pool; they are managed directly by operating system kernel multiplexers (`epoll` on Linux, `kqueue` on macOS, `IOCP` on Windows). Only file I/O, DNS lookup, and crypto tasks consume thread pool workers.

---

## 10. Best Practices & Design Guidelines

1. **Rule of Thumb: "Don't Block the Event Loop":** Never execute long-running synchronous calculations, sync file operations, or complex synchronous regexes on the main thread.
2. **Offload Heavy Computation:** Use `worker_threads` in Node.js or `Web Workers` in the browser for CPU-bound tasks (image processing, encryption, large data crunching).
3. **Partition Long Tasks:** If work cannot be moved to a worker, chunk it into smaller slices and yield control periodically via `setTimeout(..., 0)` or `scheduler.yield()` (Browser).
4. **Always Parallelize Independent I/O:** Combine independent network requests using `Promise.all()` or `Promise.allSettled()` to maximize network throughput.

---

## 11. Real-World Case Study: Diagnosing & Remediating Event Loop Starvation in Node.js

### Problem
A SaaS document generation API server handled 2,000 requests per second. Periodically, the server's HTTP response latency spiked from 15ms to 8,500ms, causing Kubernetes health check probes to fail and restarting the container. APM metrics showed 0% database wait time.

### Root Cause Analysis:
Investigation revealed that when users exported PDF documents, the code executed synchronous Markdown sanitization and synchronous image resizing on the main thread:
```javascript
// Culprit:
const html = markdownParser.parseSync(massiveDoc); // Took 600ms of synchronous CPU time!
```
During those 600ms, the main thread was completely blocked, unable to respond to incoming HTTP requests or Kubernetes `/healthz` liveness pings.

### Solution:
1. Offload Markdown parsing and PDF compilation to a dedicated worker thread pool using Node.js `worker_threads`:
```javascript
// worker-pool-service.js
const { Worker } = require("worker_threads");

function runInWorker(taskData) {
  return new Promise((resolve, reject) => {
    const worker = new Worker("./pdf-worker.js", { workerData: taskData });
    worker.on("message", resolve);
    worker.on("error", reject);
    worker.on("exit", (code) => {
      if (code !== 0) reject(new Error(`Worker stopped with exit code ${code}`));
    });
  });
}

// Route Handler:
app.post("/generate-pdf", async (req, res, next) => {
  try {
    // Main event loop is NEVER blocked; worker thread handles CPU compilation:
    const pdfBuffer = await runInWorker(req.body);
    res.type("application/pdf").send(pdfBuffer);
  } catch (err) {
    next(err);
  }
});
```
2. **Result:** P99 API response times dropped from 8,500ms to 22ms. Container restarts ceased entirely.

---

## 12. Interview Questions & Deep Dives

<details>
<summary><strong>Q1: If JavaScript is single-threaded, how does it handle thousands of concurrent network requests?</strong></summary>

JavaScript's execution engine (V8) is single-threaded, but the host environment (Node.js or the browser) is multi-threaded. Network operations are offloaded to operating system kernel mechanisms (e.g., `epoll` on Linux, `kqueue` on macOS) that notify the runtime when network packets arrive. The single JavaScript thread only executes the lightweight completion callbacks, allowing it to handle tens of thousands of concurrent I/O operations without thread allocation overhead.
</details>

<details>
<summary><strong>Q2: What is the "Run-to-Completion" rule in JavaScript?</strong></summary>

The Run-to-Completion invariant guarantees that once a synchronous function begins executing on the call stack, it runs until it finishes completely before any other code (such as timer callbacks, I/O events, or microtasks) can run. Functions cannot be preempted or interrupted mid-execution by the runtime.
</details>

<details>
<summary><strong>Q3: Does an <code>async</code> function execute on a background thread?</strong></summary>

No. `async` functions execute on the exact same single JavaScript main thread. Code inside an `async` function runs synchronously up to the first `await` expression. When an `await` is reached, execution suspends, yielding control back to the event loop, and the remainder of the function resumes later as a microtask.
</details>

<details>
<summary><strong>Q4: What operations utilize the libuv thread pool in Node.js?</strong></summary>

By default, the libuv thread pool (size 4) is used for operations that operating systems cannot perform asynchronously via non-blocking sockets:
1. File system calls (`fs.*`)
2. DNS lookups (`dns.lookup`)
3. Asynchronous cryptography (`crypto.pbkdf2`, `crypto.randomBytes`)
4. Compression (`zlib.*`)
Network I/O does NOT use the thread pool.
</details>

<details>
<summary><strong>Q5: What is "Event Loop Starvation" and how does it occur?</strong></summary>

Event Loop Starvation occurs when synchronous CPU-heavy code runs on the call stack for an extended period, or when a recursive microtask loop continuously enqueues tasks without yielding. Because the call stack or microtask queue never empties, the event loop cannot advance to process macrotasks, I/O events, or UI rendering.
</details>

<details>
<summary><strong>Q6: How can you break up a long-running synchronous computation to prevent blocking?</strong></summary>

You can partition the work into smaller chunks and yield execution to the event loop periodically using `setTimeout(nextChunk, 0)` in browsers, `setImmediate(nextChunk)` in Node.js, or the modern browser `scheduler.yield()` API.
</details>

<details>
<summary><strong>Q7: Why is <code>fs.readFileSync()</code> dangerous in production Node.js servers?</strong></summary>

`fs.readFileSync()` executes synchronously on the single JavaScript thread, freezing the event loop while waiting for disk hardware. During that read, no incoming HTTP requests can be accepted, no existing connections can receive data, and no timer callbacks can fire, degrading server throughput.
</details>

<details>
<summary><strong>Q8: What is the difference between CPU-bound tasks and I/O-bound tasks in JavaScript architecture?</strong></summary>

- **I/O-bound tasks** involve waiting for external devices (disk, network, database). JavaScript excels at these because they are offloaded to OS non-blocking primitives without consuming CPU.
- **CPU-bound tasks** involve intensive local computations (video encoding, cryptography, image processing, complex algorithms). These consume the single thread directly and must be offloaded to `worker_threads` or separate worker services.
</details>

---

## 13. Exercises & Challenges

### Exercise 1: Asynchronous Debounced Executor
Create a function `createDebouncedAsync(fn, delayMs)` that debounces asynchronous operations, ensuring only the latest invocation executes after the specified delay.

<details>
<summary><strong>Solution</strong></summary>

```javascript
function createDebouncedAsync(fn, delayMs) {
  let timerId = null;

  return function (...args) {
    return new Promise((resolve, reject) => {
      if (timerId !== null) {
        clearTimeout(timerId);
      }

      timerId = setTimeout(async () => {
        try {
          const result = await fn(...args);
          resolve(result);
        } catch (err) {
          reject(err);
        }
      }, delayMs);
    });
  };
}

const searchApi = createDebouncedAsync(async (query) => {
  console.log("Executing search for:", query);
  return `Results for ${query}`;
}, 100);

searchApi("j");
searchApi("java");
searchApi("javascript").then(console.log);
// Output after 100ms:
// Executing search for: javascript
// Results for javascript
```
</details>

### Exercise 2: Cooperative Array Processor with `scheduler.yield` Emulation
Implement an asynchronous array iterator `processCooperative(array, taskFn, maxTimeBudgetMs)` that processes items continuously but yields control to the event loop whenever synchronous execution exceeds `maxTimeBudgetMs`.

<details>
<summary><strong>Solution</strong></summary>

```javascript
async function processCooperative(items, taskFn, maxTimeBudgetMs = 16) {
  const results = [];
  let lastYieldTime = performance.now();

  for (let i = 0; i < items.length; i++) {
    results.push(taskFn(items[i], i));

    // Check if time budget exceeded:
    if (performance.now() - lastYieldTime > maxTimeBudgetMs) {
      // Yield execution to allow event loop tick:
      await new Promise(resolve => setTimeout(resolve, 0));
      lastYieldTime = performance.now();
    }
  }

  return results;
}

const items = Array.from({ length: 100_000 }, (_, i) => i);
processCooperative(items, x => x * 2).then(res => {
  console.log("Processed", res.length, "items cooperatively without freezing.");
});
```
</details>

### Exercise 3: Simulating Thread-Pool Queue Overload in Node.js
Write a script demonstrating how running 8 heavy cryptographic hashing operations concurrently saturates the default 4-thread libuv pool.

<details>
<summary><strong>Solution</strong></summary>

```javascript
const crypto = require("crypto");

function benchmarkCryptoThreads() {
  const start = Date.now();
  const iterations = 8;
  let completed = 0;

  for (let i = 1; i <= iterations; i++) {
    crypto.pbkdf2("secret", "salt", 100_000, 512, "sha512", () => {
      completed++;
      console.log(`Task #${i} finished in ${Date.now() - start}ms`);
      if (completed === iterations) {
        console.log(`All 8 tasks completed in ${Date.now() - start}ms`);
      }
    });
  }
}

benchmarkCryptoThreads();
// Observation: Because the default UV_THREADPOOL_SIZE is 4, tasks 1-4 finish first (~150ms),
// and tasks 5-8 finish in a second batch (~300ms)!
```
</details>
