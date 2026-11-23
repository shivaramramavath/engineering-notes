# Asynchronous Design Patterns

> Production asynchronous JavaScript relies on structured architectural patterns to control concurrency, throttle high-frequency I/O, deduplicate in-flight operations, and build resilient distributed pipelines.

---

## Metadata

| Attribute | Details |
| :--- | :--- |
| **Level** | Advanced |
| **Prerequisites** | [Promises](./03_promises.md), [Promise Combinators](./04_promise-combinators.md), [Async/Await](./05_async-await.md), [Cancellation & Abort](./08_cancellation-and-abort.md) |
| **Time to Read** | 25 minutes |
| **Status** | Complete |
| **Standard** | ECMAScript 2024 / Node.js 20+ LTS / Baseline Widely Available |

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

In modern client and server architectures, naively firing asynchronous operations leads directly to system instability. Unchecked concurrency (`Promise.all` across thousands of items) exhausts database connection pools, triggers HTTP 429 (Too Many Requests) rate limits, causes socket exhaustion (`EMFILE`), and spikes heap memory. Conversely, strictly serializing tasks via sequential `for...of` loops forfeits the performance benefits of non-blocking I/O.

Asynchronous design patterns provide controlled coordination abstractions:
- **Concurrency Limiting (Throttling)**: Regulating active asynchronous operations to a fixed pool size.
- **Async Queue / Pipeline**: Processing sequential tasks with prioritized scheduling and error isolation.
- **Promise Memoization & In-Flight Request Deduplication**: Reusing pending promises to prevent cache stampedes (thundering herd problem).
- **Retry with Exponential Backoff & Jitter**: Tolerating transient network faults without overwhelming recovered services.

---

## 2. Core Concepts

### Concurrency Pools vs Queues
A **Concurrency Pool** limits the number of simultaneously active asynchronous operations to $N$. When a slot opens, the next pending task starts.
An **Async Queue** extends the pool concept by adding FIFO/priority scheduling, task pausing, and drainage event hooks.

```text
Incoming Tasks: [T1, T2, T3, T4, T5, T6]
Capacity Limit: N = 2

Active Slots:
 Slot 1: [ T1 (Running...) ] -> Resolves -> Next: [ T3 (Running...) ]
 Slot 2: [ T2 (Running...) ] -> Resolves -> Next: [ T4 (Running...) ]

Pending FIFO Buffer: [ T5, T6 ]
```

### In-Flight Request Deduplication (Single-Flight Pattern)
When multiple callers request identical resources simultaneously (e.g., five components fetching `/api/user/profile` on render), deduplication catches subsequent callers and binds them to the single active promise before caching the settled result.

```text
Request A (/user/1) ----+
Request B (/user/1) ----+---> [ In-Flight Cache ] ---> Single Network Request
Request C (/user/1) ----+          |
                                   v
                          All 3 resolve simultaneously
```

---

## 3. How It Works Internally

### Concurrency Limiter State Machine
The concurrency pool manages an active counter alongside a FIFO queue of task dispatchers.

```
 User calls: limit(() => fetchTask())
                   |
                   v
    +------------------------------+
    | Is activeCount < concurrency?|
    +------------------------------+
             /            \
       YES  /              \ NO
           v                v
 +-------------------+  +--------------------------------------------+
 | activeCount++     |  | Enqueue deferred task into pending FIFO    |
 | Execute task()    |  +--------------------------------------------+
 +-------------------+                                ^
           |                                          |
           v                                          |
 +-------------------+                                |
 | Task settles      |                                |
 | activeCount--     |                                |
 +-------------------+                                |
           |                                          |
           v                                          |
 +-----------------------------------+                |
 | Is pending FIFO empty?            |                |
 +-----------------------------------+                |
             /            \                           |
       YES  /              \ NO                       |
           v                +-------------------------+
       [ Idle ]             Dequeue next task & run
```

---

## 4. Syntax & API Reference

### Pattern Implementations Overview
Modern JavaScript does not ship with built-in concurrency limiters in `Promise`, but standard patterns are constructed using native promises:

```javascript
// Concurrency limiting signature
const limit = pLimit(concurrency);
const results = await Promise.all(items.map(item => limit(() => processItem(item))));

// Single-flight deduplication signature
const getDeduped = singleFlight(keyFn, asyncFn);
```

---

## 5. Practical Examples

### Example 1: High-Performance Concurrency Limiter (p-limit Pattern)
```javascript
function pLimit(concurrency) {
  const queue = [];
  let activeCount = 0;

  const next = () => {
    activeCount--;
    if (queue.length > 0) {
      const task = queue.shift();
      task();
    }
  };

  const run = async (fn, resolve, reject, args) => {
    activeCount++;
    try {
      const result = await fn(...args);
      resolve(result);
    } catch (err) {
      reject(err);
    } finally {
      next();
    }
  };

  return function enqueue(fn, ...args) {
    return new Promise((resolve, reject) => {
      const task = () => run(fn, resolve, reject, args);
      if (activeCount < concurrency) {
        task();
      } else {
        queue.push(task);
      }
    });
  };
}

// Usage: Process 100 urls with max 5 concurrent requests
const limit = pLimit(5);
const urls = Array.from({ length: 50 }, (_, i) => `https://api.example.com/item/${i}`);
const promises = urls.map(url => limit(async () => {
  const res = await fetch(url);
  return res.json();
}));
const results = await Promise.all(promises);
```

### Example 2: In-Flight Promise Deduplication (Single-Flight)
```javascript
function singleFlight(fn) {
  const inFlight = new Map();

  return async function (...args) {
    const key = JSON.stringify(args);

    if (inFlight.has(key)) {
      return inFlight.get(key); // Return existing active promise
    }

    const promise = (async () => {
      try {
        return await fn(...args);
      } finally {
        inFlight.delete(key); // Clean up immediately when settled
      }
    })();

    inFlight.set(key, promise);
    return promise;
  };
}

// Usage
const fetchUserProfile = singleFlight(async (userId) => {
  console.log(`Executing real network fetch for user ${userId}`);
  const res = await fetch(`https://api.example.com/users/${userId}`);
  return res.json();
});

// Three calls in parallel execute only ONE network request
const [u1, u2, u3] = await Promise.all([
  fetchUserProfile(42),
  fetchUserProfile(42),
  fetchUserProfile(42),
]);
```

### Example 3: Async Queue with Priority Scheduling
```javascript
class PriorityAsyncQueue {
  #queue = [];
  #concurrency;
  #running = 0;

  constructor(concurrency = 2) {
    this.#concurrency = concurrency;
  }

  push(task, priority = 0) {
    return new Promise((resolve, reject) => {
      this.#queue.push({ task, priority, resolve, reject });
      this.#queue.sort((a, b) => b.priority - a.priority); // High priority first
      this.#pump();
    });
  }

  async #pump() {
    if (this.#running >= this.#concurrency || this.#queue.length === 0) {
      return;
    }

    this.#running++;
    const { task, resolve, reject } = this.#queue.shift();

    try {
      const result = await task();
      resolve(result);
    } catch (err) {
      reject(err);
    } finally {
      this.#running--;
      this.#pump();
    }
  }
}
```

### Example 4: Exponential Backoff with Decorrelated Full Jitter
```javascript
async function retryWithJitter(fn, maxRetries = 4, baseDelay = 300, maxDelay = 10000) {
  for (let attempt = 0; attempt <= maxRetries; attempt++) {
    try {
      return await fn();
    } catch (error) {
      if (attempt === maxRetries) throw error;

      // Full Jitter: Sleep = rand(0, min(maxDelay, baseDelay * 2^attempt))
      const exponentialCap = Math.min(maxDelay, baseDelay * 2 ** attempt);
      const jitterDelay = Math.floor(Math.random() * exponentialCap);

      console.warn(`Attempt ${attempt + 1} failed. Retrying in ${jitterDelay}ms...`);
      await new Promise(resolve => setTimeout(resolve, jitterDelay));
    }
  }
}
```

### Example 5: Async Barrier / Countdown Latch
```javascript
class AsyncCountdownLatch {
  #count;
  #promise;
  #resolve;

  constructor(count) {
    this.#count = count;
    this.#promise = new Promise(resolve => {
      this.#resolve = resolve;
    });
    if (count <= 0) this.#resolve();
  }

  countDown() {
    this.#count--;
    if (this.#count <= 0) {
      this.#resolve();
    }
  }

  wait() {
    return this.#promise;
  }
}
```

---

## 6. Edge Cases, Pitfalls & Gotchas

1. **Memory Growth in Queues**: If tasks are enqueued significantly faster than the concurrency pool drains them, the pending array grows indefinitely, causing Node.js heap out-of-memory errors. Implement queue backpressure or bounded capacities.
2. **Key Collisions in Single-Flight**: Naive `JSON.stringify(args)` ignores object key ordering (`{ a: 1, b: 2 }` vs `{ b: 2, a: 1 }`), causing cache misses or collisions. Use deterministic key hashing.
3. **Promise Caching Error Leaks**: In single-flight deduplication, make sure to delete rejected promises from the in-flight map immediately. If an error is permanently cached, all subsequent callers will continuously fail.
4. **Deadlocks in Hierarchical Limiters**: If task $A$ consumes a concurrency slot and awaits task $B$ within the same concurrency pool, and no free slots exist, the system enters an unrecoverable deadlock.

---

## 7. Common Mistakes & Anti-Patterns

| Mistake | Incorrect Pattern | Correct Solution | Why It Fails |
| :--- | :--- | :--- | :--- |
| **Unbounded `Promise.all`** | `await Promise.all(10000_items.map(fn))` | Use concurrency limiter (`pLimit(10)`) | Exhausts network sockets, RAM, and triggers rate limits. |
| **Sequential Bottleneck** | `for (const x of list) await fn(x)` | Limit concurrency to $N > 1$ | Unnecessarily serializes independent operations. |
| **Persistent Single-Flight Errors** | Memoizing rejected promise without removal | Remove key in `finally` block | Future queries fail forever after one transient glitch. |
| **Fixed Delay Retries** | Retrying every 100ms synchronously | Exponential backoff + randomized jitter | Creates thundering herd synchronization spikes. |
| **Shared Global Limiter Deadlock** | Parent task enqueues child in same limiter | Separate pools for independent task tiers | Starves child tasks, halting system progress. |
| **Ignoring Cancellation in Limiters** | Limiter runs task even if caller aborted | Check `signal.throwIfAborted()` before launch | Wastes concurrency slots on discarded requests. |

---

## 8. Performance Characteristics

- **Throughput vs Latency**: Limiting concurrency to an optimal pool size (e.g., 8–16 for HTTP/1.1 or 32–64 for HTTP/2) increases system throughput by preventing TCP connection thrashing and server queue saturation.
- **Microtask Footprint**: Using native promise chains for concurrency pool pumps creates minimal object allocations: one closure and one Promise per queued task.
- **Memory Overhead**: In-flight deduplication reduces peak memory usage by avoiding duplicate buffers and deserialized JSON structures across concurrent clients.

---

## 9. Engine Internals

1. **FIFO Queue Resolution**: A concurrency limiter's queue operates purely in user-space JavaScript memory. When `resolve()` is triggered inside `next()`, the corresponding microtask enters V8's microtask queue.
2. **Garbage Collection of Closures**: In single-flight implementations, closures retain arguments until the promise settles. Immediate deletion in `finally` ensures that response payloads are eligible for Young Generation (Scavenge) GC without surviving into Old Generation space.

---

## 10. Best Practices & Design Guidelines

1. **Size Concurrency to the Bottleneck**: Match concurrency limits to external resource constraints: database pool size for DB tasks, target API rate limits for web crawlers.
2. **Combine Limiters with AbortSignal**: Allow callers to cancel queued tasks before they start execution.
3. **Always Add Jitter to Retries**: Avoid standard exponential backoff without randomness; jitter is required to desynchronize concurrent clients.
4. **Isolate Queues by Priority**: Keep user-facing interactive requests separate from background bulk syncing queues.

---

## 11. Real-World Case Study

### Resilient Batch Asset Downloader with Rate Limiting and Circuit Breaker

An automated ingestion worker downloads thousands of media assets from third-party S3 buckets while respecting strict rate limits, retrying transient 503s with jitter, and skipping stale jobs.

```javascript
import fs from 'node:fs';
import { pipeline } from 'node:stream/promises';

class AssetDownloader {
  #limit;

  constructor(maxConcurrent = 4) {
    this.#limit = pLimit(maxConcurrent);
  }

  async downloadBatch(assets, destDir, abortSignal) {
    const tasks = assets.map(asset => {
      return this.#limit(async () => {
        abortSignal?.throwIfAborted();
        return this.#downloadWithRetry(asset, destDir, abortSignal);
      });
    });

    return Promise.allSettled(tasks);
  }

  async #downloadWithRetry(asset, destDir, signal) {
    return retryWithJitter(async () => {
      signal?.throwIfAborted();
      const targetPath = `${destDir}/${asset.id}.dat`;

      const response = await fetch(asset.url, { signal });
      if (!response.ok) {
        throw new Error(`Failed to download ${asset.id}: HTTP ${response.status}`);
      }

      const fileStream = fs.createWriteStream(targetPath);
      await pipeline(response.body, fileStream, { signal });
      return { id: asset.id, status: 'success', path: targetPath };
    }, 3, 500, 4000);
  }
}
```

---

## 12. Interview Questions

<details>
<summary><strong>1. Why is <code>Promise.all()</code> dangerous when applied to large collections?</strong></summary>

`Promise.all` invokes all asynchronous operations simultaneously upfront. When applied to thousands of items, it creates thousands of concurrent network connections or open file descriptors, leading to `EMFILE` errors, HTTP 429 rate limits, and memory exhaustion.
</details>

<details>
<summary><strong>2. What is the difference between concurrency limiting and debouncing?</strong></summary>

Concurrency limiting restricts the number of *parallel executing operations* at any given point in time. Debouncing delays the execution of a function until a specified period of inactivity has elapsed, discarding intermediate invocations.
</details>

<details>
<summary><strong>3. What is the single-flight (request deduplication) pattern?</strong></summary>

It intercepts simultaneous identical requests and attaches all callers to the same underlying in-flight Promise. When the operation completes, all callers receive the result while executing the expensive underlying work only once.
</details>

<details>
<summary><strong>4. Why is randomized jitter necessary in exponential backoff?</strong></summary>

Without jitter, multiple clients that fail at the same moment will retry at identical intervals ($200\text{ms}, 400\text{ms}, 800\text{ms}$), hitting the server in synchronized waves. Adding random jitter spreads requests out smoothly across the timeline.
</details>

<details>
<summary><strong>5. How can deadlocks occur with concurrency pools?</strong></summary>

If an enqueued task acquires a slot and then waits for another task that is queued behind it in the *same* concurrency pool, neither can proceed, causing a permanent deadlock.
</details>

<details>
<summary><strong>6. How do you implement cancellation in an async queue?</strong></summary>

By integrating `AbortSignal`. When a task is queued, register an abort listener. If aborted before execution starts, remove it from the internal FIFO buffer and reject its promise immediately without consuming a concurrency slot.
</details>

<details>
<summary><strong>7. When should you use <code>Promise.allSettled()</code> instead of <code>Promise.all()</code> in batch processing?</strong></summary>

When operations are independent and partial success is acceptable. `Promise.all` aborts upon the first rejection, whereas `Promise.allSettled` waits for all operations to finish and returns detailed outcomes for each.
</details>

<details>
<summary><strong>8. What is the difference between push-based streaming and pull-based async queues?</strong></summary>

Push-based queues dispatch tasks as soon as producers generate them, requiring explicit pause/resume hooks to prevent overflow. Pull-based systems consume tasks only when the worker has available capacity, naturally preventing backpressure issues.
</details>

---

## 13. Exercises & Challenges

### Challenge 1: Implement a Concurrency Pool with Abort Signal Support
Enhance the basic `pLimit` function so that individual tasks accept an optional `AbortSignal`. If the signal aborts while the task is still waiting in the queue, remove it immediately from the queue without executing it.

<details>
<summary>View Solution</summary>

```javascript
function pLimitWithAbort(concurrency) {
  const queue = [];
  let active = 0;

  const next = () => {
    active--;
    if (queue.length > 0) {
      const task = queue.shift();
      task();
    }
  };

  return function enqueue(fn, signal) {
    return new Promise((resolve, reject) => {
      if (signal?.aborted) {
        return reject(signal.reason);
      }

      let taskWrapper;

      const onAbort = () => {
        const index = queue.indexOf(taskWrapper);
        if (index !== -1) {
          queue.splice(index, 1);
        }
        reject(signal.reason);
      };

      taskWrapper = async () => {
        signal?.removeEventListener('abort', onAbort);
        active++;
        try {
          const result = await fn();
          resolve(result);
        } catch (err) {
          reject(err);
        } finally {
          next();
        }
      };

      signal?.addEventListener('abort', onAbort, { once: true });

      if (active < concurrency) {
        taskWrapper();
      } else {
        queue.push(taskWrapper);
      }
    });
  };
}
```
</details>

### Challenge 2: Implement an Async Memoize with TTL (Time To Live)
Write an async memoizer `memoizeWithTTL(fn, ttlMs)` that caches results for `ttlMs` milliseconds and deduplicates simultaneous in-flight calls.

<details>
<summary>View Solution</summary>

```javascript
function memoizeWithTTL(fn, ttlMs) {
  const cache = new Map(); // key -> { value, expiry }
  const inFlight = new Map(); // key -> Promise

  return async function (...args) {
    const key = JSON.stringify(args);
    const now = Date.now();

    if (cache.has(key)) {
      const entry = cache.get(key);
      if (now < entry.expiry) {
        return entry.value;
      }
      cache.delete(key);
    }

    if (inFlight.has(key)) {
      return inFlight.get(key);
    }

    const promise = (async () => {
      try {
        const result = await fn(...args);
        cache.set(key, { value: result, expiry: Date.now() + ttlMs });
        return result;
      } finally {
        inFlight.delete(key);
      }
    })();

    inFlight.set(key, promise);
    return promise;
  };
}
```
</details>
