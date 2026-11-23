# Timeout Pattern & Cancellation in JavaScript

> A production-grade implementation and architectural guide to asynchronous timeouts in JavaScript, addressing hanging promises, cooperative AbortController cancellation, resource cleanup, and Node.js process lifecycle management.

| Metadata | Value |
| :--- | :--- |
| **Level** | Intermediate / Production-Grade |
| **Prerequisites** | [11_asynchronous-javascript/02_promises.md](file:///e:/Learning/engineering-notes/01-languages/javascript/11_asynchronous-javascript/02_promises.md), [11_asynchronous-javascript/05_async-await.md](file:///e:/Learning/engineering-notes/01-languages/javascript/11_asynchronous-javascript/05_async-await.md) |
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

In JavaScript asynchronous programming, promises that never resolve or reject represent a severe operational hazard. A hanging promise can arise from:
- A remote TCP socket failing to close or send packets (half-open socket without TCP keepalive).
- A database lock acquisition waiting indefinitely behind a deadlocked transaction.
- An unresponsive third-party API gateway.

When asynchronous operations hang without bounded deadlines:
- Memory is leaked because the promise, its closure references, and associated socket buffers remain pinned on the V8 heap.
- Connection pools (HTTP agent pools, database connection pools) become exhausted, eventually rejecting all subsequent traffic.
- Upstream callers exceed service-level agreement (SLA) thresholds, cascading failures throughout the microservice mesh.

The **Timeout Pattern** wraps asynchronous operations in deterministic deadlines. If the target operation does not complete within the designated window, the wrapper rejects immediately, aborts underlying I/O, and frees allocated resources.

---

## 2. Core Concepts

### The Naive `Promise.race` Flaw
Many developers attempt timeouts using `Promise.race`:
```javascript
// NAIVE ANTI-PATTERN: DO NOT USE IN PRODUCTION
function timeoutNaive(promise, ms) {
  const timeoutPromise = new Promise((_, reject) =>
    setTimeout(() => reject(new Error('Timed out')), ms)
  );
  return Promise.race([promise, timeoutPromise]);
}
```
**Why this fails in production**:
1. `Promise.race` only resolves or rejects its outer promise when the first input settles. It **does not cancel** the underlying slow promise.
2. The slow promise continues executing in the background, consuming memory, network bandwidth, and database connection slots.
3. The pending `setTimeout` keeps the Node.js event loop active until the timer fires, even if the primary operation finished in 2ms.

### Cooperative Cancellation via `AbortController`
JavaScript does not possess a preemptive thread-kill mechanism (except for terminating an entire Worker Thread). Therefore, timeout cancellation must be **cooperative**:
- The timeout utility creates an `AbortController` and passes its `signal` to the underlying task.
- If the timeout expires first, the controller calls `abort()`.
- The underlying task listens for the abort event (or checks `signal.aborted`) and explicitly halts execution, closes file descriptors, and tears down network sockets.

```
Caller ───► [ withTimeout(taskFn, 5000) ]
                    │
                    ├── Creates AbortController
                    ├── Spawns Timer (5000ms)
                    └── Invokes taskFn(signal)
                              │
            ┌─────────────────┴─────────────────┐
            ▼                                   ▼
   taskFn finishes in 200ms            5000ms Timer expires
            │                                   │
   Clear timer                         controller.abort()
   Return task result                  Reject with TimeoutError
                                       Underlying task aborts network I/O
```

---

## 3. How It Works Internally

Modern JavaScript engines provide native support for deadline signals via `AbortSignal.timeout(ms)`.

```
                    AbortSignal.timeout(ms)
                              │
               Creates internal Timeout Task
                              │
        ┌─────────────────────┴─────────────────────┐
        ▼                                           ▼
Before deadline expires                    Deadline expires (ms)
        │                                           │
Task completes successfully                         │
Dispose signal / listeners                          ▼
                                            Signal transitions:
                                            - signal.aborted = true
                                            - signal.reason = DOMException('TimeoutError')
                                            - Dispatches 'abort' event to listeners
```

When using `AbortSignal.timeout(ms)`, the browser or Node.js runtime handles timer scheduling at the native platform layer, automatically setting `signal.reason` to a standard `DOMException` with the name `"TimeoutError"`.

---

## 4. Syntax & API Reference

### Production `withTimeout` Implementation
```javascript
/**
 * Executes an async task with an enforced cancellation deadline.
 *
 * @template T
 * @param {(signal: AbortSignal) => Promise<T>} task - Async task receiving the cancellation signal.
 * @param {number} ms - Timeout in milliseconds.
 * @param {Object} [options={}] - Configuration options.
 * @param {string} [options.customMessage] - Custom error message on timeout.
 * @param {AbortSignal} [options.parentSignal] - Optional outer cancellation signal.
 * @returns {Promise<T>} Resolves with task output or rejects with TimeoutError/AbortError.
 */
export async function withTimeout(task, ms, options = {}) {
  const { customMessage, parentSignal } = options;

  const controller = new AbortController();
  let timerId = null;

  // Handle parent signal abort if provided
  const onParentAbort = () => {
    controller.abort(parentSignal.reason);
  };

  if (parentSignal) {
    if (parentSignal.aborted) {
      throw parentSignal.reason;
    }
    parentSignal.addEventListener('abort', onParentAbort, { once: true });
  }

  const timeoutPromise = new Promise((_, reject) => {
    timerId = setTimeout(() => {
      const err = new Error(customMessage || `Operation timed out after ${ms}ms`);
      err.name = 'TimeoutError';
      controller.abort(err);
      reject(err);
    }, ms);

    // In Node.js, unref the timer so it doesn't block process exit
    if (typeof timerId?.unref === 'function') {
      timerId.unref();
    }
  });

  try {
    return await Promise.race([task(controller.signal), timeoutPromise]);
  } finally {
    if (timerId !== null) {
      clearTimeout(timerId);
    }
    if (parentSignal) {
      parentSignal.removeEventListener('abort', onParentAbort);
    }
  }
}
```

---

## 5. Practical Examples

### Example 1: Fetch with Native `AbortSignal.timeout`
```javascript
export async function fetchWithDeadline(url, timeoutMs = 5000) {
  try {
    // Native since Node.js 18+ and modern evergreen browsers
    const response = await fetch(url, {
      signal: AbortSignal.timeout(timeoutMs),
    });

    if (!response.ok) {
      throw new Error(`HTTP ${response.status}: ${response.statusText}`);
    }

    return await response.json();
  } catch (err) {
    if (err.name === 'TimeoutError') {
      console.error(`Request to ${url} timed out after ${timeoutMs}ms`);
      throw new Error(`Gateway timeout: Destination did not respond within ${timeoutMs}ms`);
    }
    throw err;
  }
}
```

### Example 2: Database Query with Cooperative Cancellation
```javascript
import { withTimeout } from './withTimeout.js';

export async function executeQueryWithTimeout(dbPool, sqlQuery, params, timeoutMs = 3000) {
  return withTimeout(
    async (signal) => {
      const client = await dbPool.connect();

      // Listen for abort to cancel in-flight backend query
      const onAbort = () => {
        console.warn('Aborting backend database query via cancel PID...');
        client.query('SELECT pg_cancel_backend($1)', [client.processID]).catch(console.error);
      };

      signal.addEventListener('abort', onAbort, { once: true });

      try {
        const result = await client.query(sqlQuery, params);
        return result.rows;
      } finally {
        signal.removeEventListener('abort', onAbort);
        client.release();
      }
    },
    timeoutMs,
    { customMessage: `Database query exceeded SLA limit of ${timeoutMs}ms` }
  );
}
```

### Example 3: Worker Thread Execution with Hard Kill on Timeout
```javascript
import { Worker } from 'node:worker_threads';

export function runWorkerWithHardTimeout(workerScriptPath, workerData, timeoutMs = 5000) {
  return new Promise((resolve, reject) => {
    const worker = new Worker(workerScriptPath, { workerData });
    let isSettled = false;

    const timer = setTimeout(() => {
      if (!isSettled) {
        isSettled = true;
        console.error(`Worker thread exceeded ${timeoutMs}ms. Terminating V8 thread isolate...`);
        worker.terminate().then(() => {
          const err = new Error(`Worker execution timed out after ${timeoutMs}ms`);
          err.name = 'TimeoutError';
          reject(err);
        });
      }
    }, timeoutMs);

    worker.on('message', (msg) => {
      if (!isSettled) {
        isSettled = true;
        clearTimeout(timer);
        resolve(msg);
      }
    });

    worker.on('error', (err) => {
      if (!isSettled) {
        isSettled = true;
        clearTimeout(timer);
        reject(err);
      }
    });
  });
}
```

### Example 4: Cascading Cumulative SLA Deadline across Multi-Step Pipeline
```javascript
export async function executePipelineWithGlobalSLA(steps, globalDeadlineMs) {
  const startTime = Date.now();
  const globalSignal = AbortSignal.timeout(globalDeadlineMs);
  const results = [];

  for (let i = 0; i < steps.length; i++) {
    const elapsed = Date.now() - startTime;
    const remainingTime = globalDeadlineMs - elapsed;

    if (remainingTime <= 0 || globalSignal.aborted) {
      throw new Error(`Pipeline aborted: Cumulative SLA expired before step ${i + 1}`);
    }

    console.log(`Executing step ${i + 1}/${steps.length} (Remaining SLA: ${remainingTime}ms)`);
    const stepResult = await steps[i](globalSignal);
    results.push(stepResult);
  }

  return results;
}
```

### Example 5: WebSocket Handshake Timeout Guard
```javascript
export function connectWebSocketWithTimeout(url, timeoutMs = 4000) {
  return new Promise((resolve, reject) => {
    const ws = new WebSocket(url);
    let resolved = false;

    const timer = setTimeout(() => {
      if (!resolved) {
        resolved = true;
        ws.close(1000, 'Handshake timeout');
        const err = new Error(`WebSocket connection to ${url} timed out after ${timeoutMs}ms`);
        err.name = 'TimeoutError';
        reject(err);
      }
    }, timeoutMs);

    ws.onopen = () => {
      if (!resolved) {
        resolved = true;
        clearTimeout(timer);
        resolve(ws);
      }
    };

    ws.onerror = (event) => {
      if (!resolved) {
        resolved = true;
        clearTimeout(timer);
        reject(new Error(`WebSocket connection failed: ${event.message || 'Unknown error'}`));
      }
    };
  });
}
```

---

## 6. Edge Cases, Pitfalls & Gotchas

1. **Synchronous CPU Loops Cannot Be Aborted**:
   JavaScript runs on a single main thread. If a function executes an infinite synchronous loop (`while(true) {}`), no timer callback, promise resolution, or `AbortSignal` event can run until the loop finishes. Timeouts only function across **asynchronous yield boundaries**.
2. **Zombie Promise Resource Leaks**:
   Using `Promise.race` without passing an `AbortSignal` leaves the underlying async promise running. If that promise holds an active TCP connection or continues allocating memory, your application will suffer progressive resource leakage.
3. **Timer Unref in Node.js Services**:
   In long-running Node.js CLI tools or test suites, active `setTimeout` timers keep the Node.js event loop alive. Always call `timer.unref()` on timeout timers so they do not artificially prolong process life if all other work has completed.
4. **Unhandled Rejections after Timeout**:
   If a task is timed out and subsequently throws an error in the background, ensure its error is caught or attached to an empty rejection handler; otherwise, it may trigger an unhandled promise rejection.

---

## 7. Common Mistakes & Anti-Patterns

| Anti-Pattern | Operational Risk | Production Fix |
| :--- | :--- | :--- |
| `Promise.race` without `AbortController` | Underlying network request continues in background, leaking resources. | Pass `controller.signal` into underlying task and abort on timeout. |
| Forgetting `clearTimeout` on success | Memory leak and delayed timer execution firing against completed tasks. | Always clear the timer in a `finally` block or on resolution. |
| Expecting timeouts to cancel CPU work | Synchronous computation freezes the thread; timeout never fires. | Offload CPU work to `Worker` threads and use `worker.terminate()`. |
| Swallowing the `TimeoutError` identity | Upstream callers cannot distinguish between network failure and timeout. | Preserve `err.name = 'TimeoutError'` for precise error handling. |
| Missing `timer.unref()` in Node CLI apps | Test runners or scripts hang for $N$ seconds waiting for timers to expire. | Invoke `timer.unref()` on timeout timers. |
| Hardcoding identical timeouts across tiers | Cascading timeouts fire at the same time, obscuring the root cause. | Configure hierarchical timeouts (outer layers have larger timeouts than inner). |

---

## 8. Performance Characteristics

- **Memory Overhead**: An `AbortController` and accompanying `setTimeout` cost approximately 160 bytes of memory on the V8 heap.
- **Timer Resolution**: Modern operating systems and libuv schedule timers with millisecond precision ($1\text{ms}$ to $4\text{ms}$ jitter depending on OS tick rate).
- **Leak Prevention**: Using `signal.addEventListener('abort', fn, { once: true })` combined with explicit listener removal in `finally` blocks ensures immediate GC collection of closures.

---

## 9. Engine Internals

### V8 Event Loop & `AbortSignal.timeout`
1. **Blink / libuv Native Integration**: Unlike custom JS-level timers, `AbortSignal.timeout(ms)` is instantiated directly within the C++ embedding layer.
2. **Macrotask Execution**: When the native timer triggers, it mutates the C++ signal state and pushes an abort task to the macrotask queue.
3. **Microtask Cascade**: When the abort event listener executes in JavaScript, it rejects the associated promise. V8 then processes the rejection during the subsequent microtask checkpoint, guaranteeing deterministic rejection ordering before any following macrotask.

---

## 10. Best Practices & Design Guidelines

1. **Pass Signals Down the Entire Call Stack**: Ensure every asynchronous helper accepts an optional `AbortSignal` and forwards it to underlying I/O.
2. **Hierarchical Timeouts**: Set timeouts strictly shorter as you move deeper into the call stack:
   $$\text{Edge Gateway (10s)} > \text{API Service (6s)} > \text{Database Query (2s)}$$
3. **Use Specific Error Types**: Always verify `err.name === 'TimeoutError'` or `err.name === 'AbortError'` in error boundary logic.
4. **Clean Up State in `finally`**: Ensure connections, file handles, and timers are released unconditionally.

---

## 11. Real-World Case Study

### API Gateway Connection Pool Starvation Post-Mortem
**Incident**: An e-commerce API gateway began rejecting 100% of user checkout requests during a holiday flash sale, reporting `HTTP 500: Connection pool exhausted`.
**Root Cause**:
- The gateway contacted an external address-validation service during checkout.
- The external service suffered a silent TCP deadlock: connections remained open, but no bytes were returned.
- The gateway's HTTP client had no timeout configured. Over 10 minutes, all 2,000 HTTP agent sockets became tied up waiting for the dead service.
- New checkout requests could not obtain a socket from the pool and immediately failed.

**Remediation**:
- Configured a mandatory 2,500ms timeout on the external address service using `withTimeout()` and `signal: AbortSignal.timeout(2500)`.
- Applied a fallback strategy: if address validation times out, the order is marked `address_unverified` and queued for asynchronous verification rather than blocking checkout.
- Added connection pool metrics and automated health checks to the gateway.

---

## 12. Interview Questions

<details>
<summary>1. Why is <code>Promise.race([task, timeoutPromise])</code> insufficient for production timeouts?</summary>

`Promise.race` only determines which promise resolves or rejects the outer race promise first. It has no mechanism to cancel or stop the losing promise. If `task` is an HTTP request, database query, or file stream, it will continue executing in the background, consuming CPU, network bandwidth, memory, and database connection pool slots. Production timeouts must use an `AbortController` to cooperatively signal cancellation to the underlying task.
</details>

<details>
<summary>2. How does <code>AbortSignal.timeout(ms)</code> differ from creating a manual <code>AbortController</code> with <code>setTimeout</code>?</summary>

`AbortSignal.timeout(ms)` is a native Web API (standardized in ECMAScript/HTML spec and implemented in Node.js 18+ and all evergreen browsers). It manages the timer internally in C++, automatically sets `signal.aborted = true` and `signal.reason = DOMException('TimeoutError')`, and eliminates the boilerplate of creating a controller and manually managing `clearTimeout` handles.
</details>

<details>
<summary>3. Can an asynchronous timeout preempt a long-running synchronous JavaScript calculation?</summary>

No. Because JavaScript executes on a single-threaded event loop, synchronous code runs to completion without interruption. As long as a synchronous loop (`for`, `while`) is running, the V8 thread is occupied and cannot process timer callbacks or abort events. To timeout CPU-bound work, the work must either periodically yield to the event loop using `await new Promise(r => setTimeout(r, 0))` or run inside a separate `Worker` thread that can be terminated via `worker.terminate()`.
</details>

<details>
<summary>4. What is the role of <code>timer.unref()</code> in Node.js timeout implementations?</summary>

In Node.js, an active timer returned by `setTimeout` keeps the event loop alive, preventing the Node.js process from exiting even if all other work has completed. Calling `timer.unref()` tells libuv not to treat this timer as an active handle in its ref count. If no other active handles remain, the process exits cleanly without waiting for the timeout timer to fire.
</details>

<details>
<summary>5. How should an application distinguish between a client-side cancellation and an SLA timeout?</summary>

By checking the `name` property of the rejection error or `signal.reason`:
- Timeouts typically set `err.name = 'TimeoutError'`.
- User-initiated cancellations (e.g. clicking a cancel button or navigating away) set `err.name = 'AbortError'`.
This allows the application to show appropriate messaging (e.g., "Request timed out, please retry" vs. "Action canceled").
</details>

<details>
<summary>6. What happens if multiple timeouts are applied at different layers of a distributed architecture?</summary>

Timeouts should follow a hierarchical "onion" structure where outer layers have longer timeouts than inner layers (e.g. Gateway 10s $\to$ Microservice 5s $\to$ Database 2s). If inner timeouts are longer than outer timeouts, the gateway will time out and return an error to the user while the backend database continues doing expensive computation for a request whose client has already disconnected.
</details>

<details>
<summary>7. How do you handle resource cleanup when an operation times out?</summary>

Resource cleanup must be implemented inside the async task using a `try...finally` block or by listening to `signal.addEventListener('abort', cleanup)`. When abort fires or the timeout triggers, the task must explicitly close database connections (`client.release()`), destroy network sockets (`socket.destroy()`), or delete temporary files.
</details>

<details>
<summary>8. How do you combine a user-provided cancellation signal with a timeout signal?</summary>

Using `AbortSignal.any([userSignal, AbortSignal.timeout(ms)])`. Introduced in ECMAScript 2024 / Node.js 20+, `AbortSignal.any()` returns a combined signal that aborts as soon as *either* the user signal aborts or the timeout deadline expires, whichever happens first.
</details>

---

## 13. Exercises & Challenges

### Exercise 1: Build a Timeout Wrapper for Arbitrary Promises
**Specification**: Write a function `timeout(promise, ms)` that rejects with a `TimeoutError` if `promise` does not resolve within `ms` milliseconds, and clears the timer if the promise resolves first.

<details>
<summary>Solution</summary>

```javascript
export function timeout(promise, ms) {
  let timer;

  const timeoutPromise = new Promise((_, reject) => {
    timer = setTimeout(() => {
      const err = new Error(`Promise timed out after ${ms}ms`);
      err.name = 'TimeoutError';
      reject(err);
    }, ms);
  });

  return Promise.race([
    promise.finally(() => clearTimeout(timer)),
    timeoutPromise,
  ]);
}
```
</details>

### Exercise 2: Combine User AbortSignal with Timeout via `AbortSignal.any`
**Specification**: Write a function `fetchWithTimeoutAndSignal(url, userSignal, timeoutMs)` that initiates a fetch request that can be canceled by either the user signal or a timeout deadline.

<details>
<summary>Solution</summary>

```javascript
export async function fetchWithTimeoutAndSignal(url, userSignal, timeoutMs) {
  const timeoutSignal = AbortSignal.timeout(timeoutMs);

  // Combine signals using native AbortSignal.any (or manual fallback)
  const combinedSignal = typeof AbortSignal.any === 'function'
    ? AbortSignal.any([userSignal, timeoutSignal].filter(Boolean))
    : timeoutSignal;

  try {
    const res = await fetch(url, { signal: combinedSignal });
    if (!res.ok) throw new Error(`HTTP ${res.status}`);
    return await res.json();
  } catch (err) {
    if (err.name === 'TimeoutError') {
      throw new Error(`Request to ${url} exceeded timeout limit of ${timeoutMs}ms`);
    }
    throw err;
  }
}
```
</details>
