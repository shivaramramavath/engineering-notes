# Asynchronous Concurrency Control & Queuing in JavaScript

> A production-grade implementation and architectural guide to asynchronous concurrency control in JavaScript, managing promise pools, backpressure, task priority queues, and file descriptor limits.

| Metadata | Value |
| :--- | :--- |
| **Level** | Advanced / Production-Grade |
| **Prerequisites** | [11_asynchronous-javascript/02_promises.md](file:///e:/Learning/engineering-notes/01-languages/javascript/11_asynchronous-javascript/02_promises.md), [17_concurrency-and-parallelism/05_concurrency-control.md](file:///e:/Learning/engineering-notes/01-languages/javascript/17_concurrency-and-parallelism/05_concurrency-control.md) |
| **Standard Baseline** | ECMAScript 2024 / Node.js 20+ LTS / Browser Evergreen |
| **Status** | Production Ready |

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

In JavaScript, asynchronous operations are lightweight and non-blocking. It is trivial to spawn thousands of concurrent tasks using `Promise.all`:
```javascript
// DANGEROUS IN PRODUCTION: Spawns 10,000 simultaneous sockets
await Promise.all(urls.map(url => fetch(url)));
```

However, executing thousands of asynchronous I/O operations simultaneously leads to immediate operational collapse:
- **Operating System Socket Exhaustion**: Crashing with `EMFILE` or `ENFILE` (Too many open files/sockets).
- **Downstream Overload**: Overwhelming relational databases (exhausting connection pools) or exceeding API rate limits (HTTP 429).
- **V8 Heap Memory Blowout**: Buffering thousands of in-flight responses simultaneously consumes gigabytes of heap memory, triggering Garbage Collection freezes and `Out Of Memory` crashes.

**Concurrency Control** constrains the number of concurrently executing asynchronous operations to a fixed threshold ($N$), queuing excess tasks until an active slot frees up.

---

## 2. Core Concepts

### Concurrency Limit vs. Rate Limit
- **Concurrency Limit ($N$ in-flight)**: Restricts the number of tasks executing *at the same instant* (e.g., maximum 5 simultaneous database queries). When a task finishes, the next queued task runs immediately.
- **Rate Limit ($N$ per time unit)**: Restricts the volume of tasks initiated *per unit of time* (e.g., maximum 10 requests per second), regardless of how fast they complete.

```
Incoming Tasks (10 items): [ T1, T2, T3, T4, T5, T6, T7, T8, T9, T10 ]
Concurrency Ceiling: 3

Active Slots:
Slot 1: [=== T1 ===] ──► [=== T4 ===] ──► [=== T7 ===] ──► [=== T10 ===]
Slot 2: [====== T2 ======] ──► [=== T5 ===] ──► [=== T8 ===]
Slot 3: [= T3 =] ──► [====== T6 ======] ──► [= T9 =]
```

### Backpressure & Queuing
When tasks arrive faster than the concurrency pool can process them, excess tasks are stored in an in-memory queue. Production queues require:
1. **FIFO / Priority Ordering**: Servicing high-priority operations before background batch jobs.
2. **Backpressure Limits**: Rejecting or pausing producers when queue length exceeds safety thresholds.
3. **Cancellation Support**: Evicting tasks from the queue before they ever execute if an `AbortSignal` fires.

---

## 3. How It Works Internally

The concurrency limiter maintains an internal FIFO queue of deferred generator functions and an `activeCount` integer.

```
                    task(args) added to pool
                               │
                      activeCount < limit?
                               │
             ┌─────────────────┴─────────────────┐
            YES                                  NO
             │                                   │
      activeCount++                       Push to waitingQueue
             │                                   │
      Execute task()                             │
             │                                   ▼
      Task settles (resolve/reject)      (Wait for slot)
             │                                   ▲
      activeCount--                              │
             │                                   │
      Items in waitingQueue? ── YES ── Dequeue next task
             │ NO
          [ Idle ]
```

Each queued task returns a detached Promise. When an active slot becomes available, the next queued runner is shifted from the queue and invoked, resolving the caller's outer promise.

---

## 4. Syntax & API Reference

### Production-Grade Concurrency Limiter (`pLimit`)
```javascript
/**
 * Creates a concurrency-limiting function.
 *
 * @param {number} concurrency - Maximum number of concurrent tasks (must be >= 1).
 * @returns {<T>(fn: () => Promise<T>) => Promise<T>} Limiter function.
 */
export function pLimit(concurrency) {
  if (!Number.isInteger(concurrency) || concurrency < 1) {
    throw new TypeError('Concurrency must be an integer greater than or equal to 1');
  }

  const queue = [];
  let activeCount = 0;

  function next() {
    activeCount--;

    if (queue.length > 0) {
      const nextTask = queue.shift();
      nextTask();
    }
  }

  function run(fn, resolve, reject, args) {
    activeCount++;

    const taskPromise = Promise.resolve().then(() => fn(...args));

    taskPromise
      .then(resolve, reject)
      .finally(() => {
        next();
      });
  }

  function enqueue(fn, resolve, reject, args) {
    queue.push(run.bind(null, fn, resolve, reject, args));

    (async () => {
      // Yield to microtask queue before starting to allow batch sync enqueueing
      await Promise.resolve();

      if (activeCount < concurrency && queue.length > 0) {
        queue.shift()();
      }
    })();
  }

  const generator = (fn, ...args) =>
    new Promise((resolve, reject) => {
      enqueue(fn, resolve, reject, args);
    });

  Object.defineProperties(generator, {
    activeCount: { get: () => activeCount },
    pendingCount: { get: () => queue.length },
    clearQueue: { value: () => { queue.length = 0; } },
  });

  return generator;
}
```

---

## 5. Practical Examples

### Example 1: Web Scraping Pipeline Limiting Concurrency to 5
```javascript
import { pLimit } from './pLimit.js';

export async function scrapeWebsites(urls, maxConcurrent = 5) {
  const limit = pLimit(maxConcurrent);

  console.log(`Starting crawl of ${urls.length} pages with concurrency ${maxConcurrent}...`);

  const crawlTasks = urls.map((url, idx) =>
    limit(async () => {
      console.log(`[Start ${idx + 1}/${urls.length}] Fetching ${url} (Active: ${limit.activeCount}, Pending: ${limit.pendingCount})`);
      const response = await fetch(url);
      const text = await response.text();
      return { url, length: text.length, status: response.status };
    })
  );

  return Promise.all(crawlTasks);
}
```

### Example 2: Batch Image Resizing Constrained to CPU Cores
```javascript
import os from 'node:os';
import { pLimit } from './pLimit.js';

export async function processImageBatch(imageFiles, resizeFunction) {
  // Optimal CPU concurrency: number of logical processor cores
  const cpuLimit = pLimit(os.cpus().length);

  const results = await Promise.allSettled(
    imageFiles.map((file) =>
      cpuLimit(async () => {
        return await resizeFunction(file, 800, 600);
      })
    )
  );

  const succeeded = results.filter((r) => r.status === 'fulfilled').map((r) => r.value);
  const failed = results.filter((r) => r.status === 'rejected').map((r) => r.reason);

  return { succeeded, failed };
}
```

### Example 3: Priority Asynchronous Queue with Cancellation
```javascript
export class PriorityAsyncQueue {
  constructor(concurrency = 2) {
    this.concurrency = concurrency;
    this.activeCount = 0;
    this.queue = []; // Array of { task, priority, resolve, reject, signal }
  }

  enqueue(task, { priority = 0, signal = null } = {}) {
    return new Promise((resolve, reject) => {
      if (signal?.aborted) {
        return reject(signal.reason ?? new Error('Aborted'));
      }

      const item = { task, priority, resolve, reject, signal };

      if (signal) {
        signal.addEventListener(
          'abort',
          () => {
            const idx = this.queue.indexOf(item);
            if (idx !== -1) {
              this.queue.splice(idx, 1);
              reject(signal.reason ?? new Error('Aborted before execution'));
            }
          },
          { once: true }
        );
      }

      this.queue.push(item);
      // Higher priority sorts first
      this.queue.sort((a, b) => b.priority - a.priority);
      this.dispatch();
    });
  }

  dispatch() {
    if (this.activeCount >= this.concurrency || this.queue.length === 0) {
      return;
    }

    const { task, resolve, reject, signal } = this.queue.shift();

    if (signal?.aborted) {
      this.dispatch();
      return;
    }

    this.activeCount++;

    Promise.resolve()
      .then(() => task())
      .then(resolve, reject)
      .finally(() => {
        this.activeCount--;
        this.dispatch();
      });
  }
}
```

### Example 4: Database Bulk Upsert with Backpressure
```javascript
import { pLimit } from './pLimit.js';

export async function bulkInsertRecords(records, dbClient, batchSize = 100, maxConcurrentBatches = 4) {
  const limit = pLimit(maxConcurrentBatches);
  const batches = [];

  for (let i = 0; i < records.length; i += batchSize) {
    batches.push(records.slice(i, i + batchSize));
  }

  console.log(`Split ${records.length} records into ${batches.length} batches.`);

  const tasks = batches.map((batch, idx) =>
    limit(async () => {
      console.log(`Writing batch ${idx + 1}/${batches.length}...`);
      return await dbClient.query('INSERT INTO audit_log SELECT * FROM json_populate_recordset(null::audit_log, $1)', [
        JSON.stringify(batch),
      ]);
    })
  );

  return Promise.all(tasks);
}
```

### Example 5: Graceful Shutdown Drain Pattern
```javascript
export class DrainablePool {
  constructor(concurrency = 4) {
    this.limit = pLimit(concurrency);
    this.isDraining = false;
  }

  submit(task) {
    if (this.isDraining) {
      throw new Error('Pool is shutting down; rejecting new submissions.');
    }
    return this.limit(task);
  }

  async drain(timeoutMs = 10000) {
    this.isDraining = true;
    console.log(`Draining pool. Active: ${this.limit.activeCount}, Pending: ${this.limit.pendingCount}`);

    const start = Date.now();
    while (this.limit.activeCount > 0 || this.limit.pendingCount > 0) {
      if (Date.now() - start > timeoutMs) {
        this.limit.clearQueue();
        throw new Error(`Drain timed out after ${timeoutMs}ms. Dropped pending tasks.`);
      }
      await new Promise((r) => setTimeout(r, 50));
    }

    console.log('Pool successfully drained.');
  }
}
```

---

## 6. Edge Cases, Pitfalls & Gotchas

1. **Deadlocks via Nested Concurrency Pools**:
   If Task A occupies a concurrency slot in Pool X and awaits Task B, which is submitted to the same Pool X, a deadlock occurs when all slots in Pool X are occupied by tasks awaiting child tasks. Always use separate pools for dependent stages.
2. **Uncaught Task Rejections**:
   If a task inside `pLimit` rejects and the caller does not catch the promise returned by `limit(fn)`, Node.js triggers an unhandled promise rejection.
3. **Array Splice Overhead on Massive Queues**:
   In high-scale queues ($>100,000$ items), using `queue.shift()` is an $O(N)$ operation because it re-indexes the entire array on each dequeue. For large queues, use a doubly-linked list or two-pointer ring buffer for true $O(1)$ dequeuing.
4. **Queue Memory Accumulation**:
   Pushing 1,000,000 tasks into `limit()` buffers all 1,000,000 closure functions and promises in memory immediately. If generation is faster than execution, use **Streams** or async iterators to generate tasks on demand.

---

## 7. Common Mistakes & Anti-Patterns

| Anti-Pattern | Operational Risk | Production Fix |
| :--- | :--- | :--- |
| `Promise.all` over unbounded collections | Crashes runtime with `EMFILE` or runs out of RAM. | Wrap tasks in `pLimit(N)` or process with an Async Queue. |
| Inverting concurrency limit (`N = 1000`) | Provides zero protection against socket starvation. | Size concurrency between 2 and 20 based on downstream capacity. |
| Nested tasks sharing identical limiter | Causes circular deadlock when pool capacity is reached. | Use hierarchical or isolated limiters for parent/child workflows. |
| Unbounded queue ingestion | Memory consumption explodes under high input rates. | Enforce queue backpressure (`if (queue.length > 1000) pause()`). |
| Inability to cancel pending queued tasks | Queued tasks execute uselessly even after client disconnects. | Wire `AbortSignal` to remove items from the queue before dispatch. |
| Using `Array.shift()` on 500,000 queued items | High CPU lag due to repeated $O(N)$ memory array shifts. | Implement a linked list or ring buffer queue. |

---

## 8. Performance Characteristics

- **Throughput Curve**: Increasing concurrency from 1 to 8 typically yields a 6x to 7x throughput increase on I/O operations. Beyond 20–30 concurrent connections to a single host, throughput plateaus and latency degrades due to TCP contention and network buffer saturation.
- **Memory Footprint**: `pLimit` maintains a tiny closure footprint (~80 bytes per queued task).
- **CPU Offload**: Tasks are dispatched strictly via standard microtask scheduling without timer polling loops.

---

## 9. Engine Internals

### Libuv Thread Pool & Socket Limits
- **Libuv UV_THREADPOOL_SIZE**: By default, Node.js libuv allocates 4 worker threads for filesystem and DNS lookups. Setting a filesystem concurrency limiter to 4 aligns with the internal threadpool, preventing queue delays at the libuv C++ layer.
- **V8 Microtask Boundary**: Dequeuing via `Promise.resolve().then(...)` ensures that when one task completes, its successor is scheduled in the same event loop turn during the microtask drain phase, minimizing latency between tasks.

---

## 10. Best Practices & Design Guidelines

1. **Size Concurrency to Bottlenecks**:
   - Filesystem I/O: 4–8 (matches `UV_THREADPOOL_SIZE`).
   - CPU-bound Workers: Number of physical CPU cores (`os.cpus().length`).
   - Downstream APIs: Respect partner rate limits (e.g. 5–10).
   - Relational Database Queries: Match database connection pool size (e.g. 10–20).
2. **Combine with `Promise.allSettled`**: Use `Promise.allSettled` when executing batch tasks so that one failure does not abort remaining items.
3. **Expose Metrics**: Export `activeCount` and `pendingCount` to monitoring systems (Prometheus, Datadog).

---

## 11. Real-World Case Study

### S3 Asset Migration (500,000 Media Files)
**Scenario**: An enterprise media platform needed to migrate 500,000 video files from on-premise storage to AWS S3.
**Initial Implementation**: A script read the file list and executed `Promise.all(files.map(upload))`.
**Result**: The process crashed within 3 seconds with `Error: EMFILE, too many open files` and consumed 3.5GB of heap.

**Redesign Architecture**:
- Implemented `pLimit(25)` to constrain active simultaneous uploads.
- Streamed file paths from disk using an asynchronous generator rather than loading all 500,000 paths into memory at once.
- Added retry with full jitter on individual chunk failures.

**Outcome**:
- The migration completed over 6 hours without a single crash or dropped socket.
- Memory usage remained completely flat at 48MB throughout the entire 500,000-file migration.

---

## 12. Interview Questions

<details>
<summary>1. What is the fundamental difference between Concurrency Limiting and Rate Limiting?</summary>

Concurrency limiting restricts the number of tasks that can be active *simultaneously* at any given point in time (e.g., at most 5 database connections open right now). As soon as one finishes, the next runs immediately. Rate limiting restricts the number of tasks that can be started *per unit of time* (e.g., at most 10 requests per second), regardless of whether previous requests have completed or not.
</details>

<details>
<summary>2. Why does <code>Promise.all(urls.map(fetch))</code> fail when processing thousands of URLs?</summary>

It attempts to initialize all network requests concurrently in the same turn of the event loop. This leads to:
1. `EMFILE` / `ENFILE` errors when the OS runs out of file descriptors / TCP sockets.
2. Exhaustion of the browser or Node.js HTTP connection pool.
3. Extreme memory usage from buffering thousands of unresolved Promises and response bodies on the V8 heap, causing Garbage Collection thrashing and Out of Memory crashes.
</details>

<details>
<summary>3. How does a promise pool avoid memory leaks when processing millions of items?</summary>

By combining concurrency control with **asynchronous generators or streams**. Instead of converting all 1,000,000 items into promises upfront, an async iterator yields items in small batches on demand. Concurrency slots pull new items only when a slot becomes free, keeping active memory usage constant ($O(N)$ where $N$ is the concurrency limit, rather than $O(TotalItems)$).
</details>

<details>
<summary>4. What causes a deadlock in an asynchronous concurrency pool?</summary>

A deadlock occurs when tasks in the pool depend on other tasks submitted to the same pool. For example, if a pool has concurrency 2, and Tasks A and B are active, and Task A awaits the result of Task C submitted to the same pool, Task C cannot run because both slots are filled. Task A is blocked waiting for C, and C is blocked waiting for A to finish, resulting in an unrecoverable deadlock.
</details>

<details>
<summary>5. Why is <code>Array.shift()</code> inefficient for large queues, and what is the fix?</summary>

`Array.prototype.shift()` removes the first element and shifts every remaining element in memory down by one index, which is an $O(N)$ operation. For a queue with 100,000 elements, shifting takes significant CPU time. The solution is to use a **Doubly-Linked List** or a **Ring Buffer** (with read and write pointers), both of which perform dequeue operations in $O(1)$ constant time.
</details>

<details>
<summary>6. How can an <code>AbortSignal</code> be integrated into an asynchronous queue?</summary>

When a task is enqueued, pass an `AbortSignal`. Listen for the `'abort'` event on the signal. If it fires while the task is still waiting in the queue, remove the task from the queue array, detach the listener, and immediately reject the returned promise with the abort reason without ever invoking the task function.
</details>

<details>
<summary>7. How does <code>UV_THREADPOOL_SIZE</code> in Node.js relate to concurrency control?</summary>

Node.js uses libuv's internal threadpool to perform asynchronous filesystem I/O, DNS lookups, and crypto hashing. By default, `UV_THREADPOOL_SIZE` is 4. If your JavaScript concurrency limit for disk I/O exceeds 4, the excess requests simply queue inside libuv's C++ thread pool. Aligning filesystem concurrency limits with `UV_THREADPOOL_SIZE` prevents hidden queuing and thread contention.
</details>

<details>
<summary>8. How do you implement a graceful shutdown for an active worker queue?</summary>

1. Set an internal `isDraining = true` flag to reject any new task submissions.
2. Allow currently active and queued tasks to complete.
3. Provide a `drain()` method that returns a Promise resolving when `activeCount === 0 && queue.length === 0`.
4. Enforce a maximum timeout deadline: if draining exceeds $M$ seconds, cancel/clear remaining tasks and reject.
</details>

---

## 13. Exercises & Challenges

### Exercise 1: Implement a Basic Promise Pool
**Specification**: Write `pool(tasks, concurrency)` that executes an array of async functions with a maximum of `concurrency` running simultaneously, returning an array of resolved values in input order.

<details>
<summary>Solution</summary>

```javascript
export async function pool(tasks, concurrency) {
  const results = new Array(tasks.length);
  let currentIndex = 0;

  async function worker() {
    while (currentIndex < tasks.length) {
      const idx = currentIndex++;
      results[idx] = await tasks[idx]();
    }
  }

  const workers = Array.from(
    { length: Math.min(concurrency, tasks.length) },
    () => worker()
  );

  await Promise.all(workers);
  return results;
}
```
</details>

### Exercise 2: Add Per-Task Timeout to Concurrency Pool
**Specification**: Enhance `pLimit` so that individual tasks accept an optional `timeoutMs` option, rejecting the task if it exceeds the deadline while freeing up the concurrency slot for the next queued task.

<details>
<summary>Solution</summary>

```javascript
import { pLimit } from './pLimit.js';

export function pLimitWithTimeout(concurrency) {
  const limit = pLimit(concurrency);

  return function (fn, timeoutMs) {
    if (!timeoutMs) {
      return limit(fn);
    }

    return limit(() => {
      let timer;
      const timeoutPromise = new Promise((_, reject) => {
        timer = setTimeout(() => {
          const err = new Error(`Task exceeded timeout of ${timeoutMs}ms`);
          err.name = 'TimeoutError';
          reject(err);
        }, timeoutMs);
      });

      return Promise.race([
        fn().finally(() => clearTimeout(timer)),
        timeoutPromise,
      ]);
    });
  };
}
```
</details>
