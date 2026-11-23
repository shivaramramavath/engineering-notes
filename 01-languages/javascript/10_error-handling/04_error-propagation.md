# JavaScript Error Propagation: Bubbling, Async Boundaries, unhandledRejection & Process Crashes

> Comprehensive technical guide to synchronous and asynchronous error propagation in ECMAScript, Promise rejection chaining, event loop boundaries, Node.js process crash guards, and browser global error telemetry.

| Metadata | Details |
| :--- | :--- |
| **Level** | Core / Architecture |
| **Prerequisites** | [10_error-handling/01_errors.md](01_errors.md), [10_error-handling/02_try-catch-finally.md](02_try-catch-finally.md) |
| **Time to Read** | 26 minutes |
| **Status** | Complete |
| **ECMAScript Baseline** | ES2015 (Promises) / ES2017 (Async/Await) / Node.js Process Standard |

---

## Table of Contents
1. [What & Why](#1-what--why)
2. [Core Concepts: Synchronous Bubbling vs Asynchronous Chains](#2-core-concepts-synchronous-bubbling-vs-asynchronous-chains)
3. [How It Works: Event Loop Boundaries & Microtask Tracking](#3-how-it-works-event-loop-boundaries--microtask-tracking)
4. [Comprehensive Syntax & API Reference](#4-comprehensive-syntax--api-reference)
5. [Practical Examples](#5-practical-examples)
6. [Edge Cases, Pitfalls & Gotchas](#6-edge-cases-pitfalls--gotchas)
7. [Common Mistakes & Anti-Patterns](#7-common-mistakes--anti-patterns)
8. [Performance Characteristics](#8-performance-characteristics)
9. [Engine Internals: V8 PromiseRejectCallback & Node Task Queues](#9-engine-internals-v8-promiserejectcallback--node-task-queues)
10. [Best Practices & Design Guidelines](#10-best-practices--design-guidelines)
11. [Real-World Case Study: Production Node.js Server Graceful Shutdown Pipeline](#11-real-world-case-study-production-nodejs-server-graceful-shutdown-pipeline)
12. [Interview Questions & Deep Dives](#12-interview-questions--deep-dives)
13. [Exercises & Challenges](#13-exercises--challenges)

---

## 1. What & Why

In JavaScript, errors do not remain localized to the line of code that triggered them. Unless caught immediately, an exception **propagates** outwards:
- In **synchronous code**, an error bubbles up through the active call stack frames, unwinding functions until it finds a `try...catch` block.
- In **asynchronous code**, execution yields to the event loop. When a failure occurs inside a callback, Promise, or async function, the original call stack that scheduled the operation has long since vanished.

Failing to properly manage asynchronous error propagation leads to:
1. **Silent Failures:** Promises rejecting with unhandled exceptions that go unnoticed until user complaints arrive.
2. **Zombie Node.js Processes:** Memory leaks, orphaned database connections, and hung HTTP sockets caused by catching errors without cleanup.
3. **Catastrophic Process Crashes:** Node.js terminating abruptly in production due to unhandled promise rejections or uncaught exceptions.

---

## 2. Core Concepts: Synchronous Bubbling vs Asynchronous Chains

### 2.1 Synchronous Stack Bubbling
When a synchronous function throws, V8 unwinds the physical call stack sequentially:

```
[Layer 3: validateInput()]  ---> Throws Error!
              |
              v (Bubbles up)
[Layer 2: parsePayload()]   ---> No catch block; frame popped
              |
              v (Bubbles up)
[Layer 1: requestHandler()] ---> try...catch intercepts error!
```

### 2.2 Asynchronous Propagation Across Event Loop Ticks
When code involves asynchronous timers, I/O, or Promises, errors cannot bubble up the synchronous stack:

```
[Tick 1: Main Thread]
fetchUserData() schedules network I/O
Call stack completely empties!
           |
       (Network fails in background)
           |
[Tick 2: Microtask Queue]
Promise rejects! The stack frame of fetchUserData() NO LONGER EXISTS.
Rejection must propagate down the Promise chain via .catch() or await.
```

---

## 3. How It Works: Event Loop Boundaries & Microtask Tracking

When a Promise is rejected without an immediate rejection handler attached:

```
                +-----------------------------------+
                |        Promise Rejection          |
                |    Promise.reject(new Error())    |
                +-----------------------------------+
                                  |
                                  v
       V8 checks if rejection handler (.catch) is attached
                                  |
            +---------------------+---------------------+
            |                                           |
    Handler Attached                            No Handler Attached
            |                                           |
            v                                           v
    Handler executes in                   Promise queued in V8 Engine
    Microtask Queue                       "Unhandled Rejection List"
                                                        |
                                                        v
                                          Event Loop Tick Finishes
                                                        |
                                          +-------------+-------------+
                                          |                           |
                                      (Node.js)                   (Browser)
                                          |                           |
                                          v                           v
                             process.emit(                 window.dispatchEvent(
                              'unhandledRejection')         'unhandledrejection')
```

### The Unhandled Rejection Lifecycle:
1. If a rejection handler is attached *after* the current microtask turn (e.g. via `setTimeout`), Node.js emits `rejectionHandled` to indicate late recovery.
2. In Node.js v15+, unhandled rejections terminate the process with non-zero exit code `1` by default (mode: `--unhandled-rejections=throw`).

---

## 4. Comprehensive Syntax & API Reference

### 4.1 Node.js Process Crash Guard Events
```javascript
// 1. Uncaught Synchronous Exceptions:
process.on("uncaughtException", (err, origin) => {
  console.error(`[FATAL] Uncaught Exception at: ${origin}. Error:`, err);
  // Mandatory: Clean up and exit process!
  process.exit(1);
});

// 2. Unhandled Asynchronous Promise Rejections:
process.on("unhandledRejection", (reason, promise) => {
  console.error("[FATAL] Unhandled Promise Rejection:", reason);
  // Recommended: Trigger graceful shutdown
  process.exit(1);
});

// 3. Passive Monitoring (Does not prevent crash):
process.on("uncaughtExceptionMonitor", (err, origin) => {
  // Useful for logging metrics to Datadog/CloudWatch before exit
});
```

### 4.2 Browser Global Error Telemetry
```javascript
// Global synchronous runtime errors:
window.addEventListener("error", (event) => {
  console.error("Script error:", event.message, event.filename, event.lineno);
});

// Global unhandled promise rejections:
window.addEventListener("unhandledrejection", (event) => {
  console.error("Unhandled rejection reason:", event.reason);
  event.preventDefault(); // Prevents browser console red error spam
});
```

---

## 5. Practical Examples

### Example 1: Multi-Step Promise Chaining with Mid-Stream Recovery
```javascript
function fetchUserProfile(userId) {
  return Promise.resolve({ id: userId, tier: "FREE" });
}

function fetchUserPreferences(user) {
  // Simulating transient failure for preferences:
  return Promise.reject(new Error("Preferences service unavailable"));
}

function fetchUserDashboard(user, preferences) {
  return `Dashboard for ${user.id} with theme: ${preferences.theme}`;
}

fetchUserProfile(101)
  .then(user => {
    return fetchUserPreferences(user)
      .catch(err => {
        // Intermediate recovery: fall back to default preferences
        console.warn("[Recovered] Could not load preferences; using defaults.");
        return { theme: "light", notifications: true };
      })
      .then(preferences => fetchUserDashboard(user, preferences));
  })
  .then(dashboard => {
    console.log("Rendered:", dashboard);
  })
  .catch(fatalErr => {
    // Only triggers if fetchUserProfile or dashboard rendering throws
    console.error("Fatal workflow failure:", fatalErr.message);
  });
// Output:
// [Recovered] Could not load preferences; using defaults.
// Rendered: Dashboard for 101 with theme: light
```

### Example 2: Legacy Node.js Error-First Callback Adapter
Before Promises, Node.js propagated errors via the error-first callback pattern `callback(err, result)`:
```javascript
// Converting error-first callback into a clean Promise:
function promisify(fn) {
  return function (...args) {
    return new Promise((resolve, reject) => {
      fn(...args, (err, data) => {
        if (err) return reject(err); // Propagate error via Promise rejection
        resolve(data);
      });
    });
  };
}

// Example usage:
const mockAsyncRead = (file, cb) => {
  if (!file) cb(new Error("File missing"), null);
  else cb(null, "File content");
};

const safeRead = promisify(mockAsyncRead);
safeRead("data.txt")
  .then(console.log)
  .catch(err => console.error("Caught:", err.message));
```

### Example 3: Async/Await Error Propagation Across Module Boundaries
```javascript
// Database Layer
async function queryDatabase(sql) {
  throw new Error("Connection lost to DB replica");
}

// Service Layer
async function getUserBalance(userId) {
  // Error bubbles naturally through await without manual try/catch!
  const rows = await queryDatabase(`SELECT balance FROM users WHERE id = ${userId}`);
  return rows[0].balance;
}

// Controller Layer (Boundary)
async function handleApiRequest(req, res) {
  try {
    const balance = await getUserBalance(req.userId);
    res({ status: 200, balance });
  } catch (err) {
    // Intercepts the error that bubbled from queryDatabase:
    console.error(`[API Controller] Request failed:`, err.message);
    res({ status: 500, error: "Internal Server Error" });
  }
}

handleApiRequest({ userId: 42 }, console.log);
```

### Example 4: Late Rejection Handling Bug Demonstration
```javascript
// DANGEROUS: Attaching catch on a later tick triggers unhandledRejection:
const promise = Promise.reject(new Error("Immediate async rejection"));

// Rejection occurs in current microtask tick...
// Attaching catch after 50ms:
setTimeout(() => {
  promise.catch(err => {
    console.log("Late catch handled error:", err.message);
  });
}, 50);

// Node.js will emit:
// 1. 'unhandledRejection' (at end of current tick)
// 2. 'rejectionHandled' (when setTimeout executes)
```

### Example 5: Clean Global Browser Telemetry Forwarder
```javascript
function initializeBrowserErrorTracking(reportingUrl) {
  function sendTelemetry(type, data) {
    const payload = JSON.stringify({ type, ...data, timestamp: Date.now() });
    navigator.sendBeacon(reportingUrl, payload);
  }

  window.addEventListener("error", (event) => {
    sendTelemetry("UNCAUGHT_EXCEPTION", {
      message: event.message,
      filename: event.filename,
      lineno: event.lineno,
      colno: event.colno,
      stack: event.error?.stack
    });
  });

  window.addEventListener("unhandledrejection", (event) => {
    sendTelemetry("UNHANDLED_REJECTION", {
      reason: event.reason?.message || String(event.reason),
      stack: event.reason?.stack
    });
  });
}
```

---

## 6. Edge Cases, Pitfalls & Gotchas

### 6.1 The "Zombie Process" Anti-Pattern in `uncaughtException`
When `uncaughtException` is triggered in Node.js, your application is in an **undefined, corrupt state**:
- File descriptors and locks may be held open.
- Database connection pools might be corrupted.
- Memory structures may be partially updated.
**Never resume normal execution inside `uncaughtException`!** Always log the error, release resources, and invoke `process.exit(1)`.

### 6.2 Errors Thrown Inside Promise `.then()`
If a synchronous error is thrown inside a `.then()` handler, it does **not** crash the process. The Promise mechanism automatically catches it and converts it into a rejected Promise:
```javascript
Promise.resolve(10)
  .then(val => {
    throw new Error("Syntax mistake in then"); // Automatically converted to rejection!
  })
  .catch(err => {
    console.log("Caught cleanly:", err.message); // "Caught cleanly: Syntax mistake in then"
  });
```

### 6.3 Missing `return` in Promise Chains
Forgetting to `return` an asynchronous call inside a `.then()` handler disconnects the promise chain, causing subsequent rejections to bypass the outer `.catch()`:
```javascript
// BROKEN: Missing return
db.getUser(10)
  .then(user => {
    // Missing return! Rejection will escape downstream catch!
    db.updatePreferences(user); 
  })
  .catch(err => console.log("Will NOT catch updatePreferences error!"));

// FIXED:
db.getUser(10)
  .then(user => {
    return db.updatePreferences(user); // Chained properly!
  })
  .catch(err => console.log("Catches all errors!"));
```

---

## 7. Common Mistakes & Anti-Patterns

| Anti-Pattern | Bad Practice | Idiomatic Solution | Reason |
| :--- | :--- | :--- | :--- |
| **Resuming in `uncaughtException`** | `process.on('uncaughtException', () => {})` | Log and call `process.exit(1)` | Process is in an undefined state; leads to corrupted data. |
| **Floating Promises** | `asyncFunction();` without `await` or `.catch()` | `await asyncFunction();` or `.catch(...)` | Unhandled rejections crash modern Node.js runtimes. |
| **Late Catch Attachment** | Attaching `.catch()` inside a later `setTimeout` | Attach `.catch()` immediately on creation | Triggers spurious `unhandledRejection` telemetry alerts. |
| **Swallowing Rejections** | `.catch(err => {})` without logging | Log or re-throw after handling | Hides critical failures from engineers and monitoring tools. |
| **Breaking the Promise Chain** | Omitting `return` inside `.then()` | Always return promises in chains | Disconnects error propagation down the pipeline. |
| **Double Handling Errors** | Catching in helper AND throwing again without cause | Catch only at architectural boundaries | Clutters logs with duplicate stack traces for a single failure. |

---

## 8. Performance Characteristics

1. **Microtask Tracking Overhead:**
   - Every rejected Promise allocates a slot in V8's internal unhandled rejection tracking queue.
   - Attaching `.catch()` immediately allows V8 to resolve the rejection in the microtask queue without promoting it to the global process event layer.
2. **Event Loop Throughput:**
   - Asynchronous error propagation via `async/await` adds negligible CPU overhead (~0.05% in V8) compared to the physical I/O latency of database or network calls.

---

## 9. Engine Internals: V8 PromiseRejectCallback & Node Task Queues

Inside V8, Promise rejections are reported via a C++ callback hook: `v8::Isolate::SetPromiseRejectCallback()`.

```
                  +-----------------------------------+
                  |  V8 Promise Object Enters REJECT  |
                  +-----------------------------------+
                                    |
                                    v
                  Invokes v8::PromiseRejectCallback
                                    |
                  +-----------------+-----------------+
                  |                                   |
           kPromiseRejectWithNoHandler       kPromiseHandlerAddedAfterReject
                  |                                   |
                  v                                   v
        Node.js TaskQueue:                   Node.js TaskQueue:
   Emits 'unhandledRejection'            Emits 'rejectionHandled'
```

- When Node.js boots, it registers its internal C++ handler with V8's `SetPromiseRejectCallback`.
- At the end of every microtask turn, Node.js checks its internal `unhandledRejections` Map.
- If any promise remains unhandled when the event loop transitions to the next phase, Node.js triggers the `unhandledRejection` event.

---

## 10. Best Practices & Design Guidelines

1. **Fail Fast and Restart:** In Node.js, treat unhandled exceptions as fatal. Let the process crash, and rely on orchestrators (Docker, Kubernetes, PM2, systemd) to restart the container cleanly.
2. **Handle Errors at Architectural Boundaries:** Do not wrap every single internal function in `try...catch`. Let errors bubble up to controllers, middleware, or message queue workers.
3. **Always Return Promises in Chains:** Ensure every `.then()` returns a value or a Promise to maintain continuous error propagation.
4. **Attach Global Crash Handlers:** Register `uncaughtException` and `unhandledRejection` listeners to flush pending logs to APM before terminating.

---

## 11. Real-World Case Study: Production Node.js Server Graceful Shutdown Pipeline

### Problem
An e-commerce API server frequently crashed due to unhandled database timeout rejections. When an uncaught exception occurred, Node.js terminated abruptly, dropping 200 in-flight HTTP requests and leaving database connections open.

### Solution
Implement an enterprise Graceful Shutdown Manager that handles `uncaughtException` and `unhandledRejection` by closing HTTP servers and draining database pools before exiting:

```javascript
class GracefulShutdownManager {
  static isShuttingDown = false;

  static initialize(httpServer, dbPool) {
    const handleFatal = async (source, error) => {
      if (this.isShuttingDown) return;
      this.isShuttingDown = true;

      console.error(`[FATAL] Terminating process due to ${source}:`, error);

      // 1. Set a hard termination timeout (in case cleanup hangs):
      const forceExitTimer = setTimeout(() => {
        console.error("[SHUTDOWN] Forceful termination triggered after 10s timeout.");
        process.exit(1);
      }, 10_000);
      forceExitTimer.unref(); // Don't let timeout keep process alive

      // 2. Stop accepting new HTTP requests:
      if (httpServer) {
        console.log("[SHUTDOWN] Closing HTTP server listener...");
        await new Promise(res => httpServer.close(res));
      }

      // 3. Drain and close database pool:
      if (dbPool) {
        console.log("[SHUTDOWN] Draining database connection pool...");
        await dbPool.end();
      }

      console.log("[SHUTDOWN] Cleanup complete. Exiting cleanly.");
      process.exit(1);
    };

    process.on("uncaughtException", (err) => handleFatal("uncaughtException", err));
    process.on("unhandledRejection", (err) => handleFatal("unhandledRejection", err));
  }
}

// Simulated Server & DB Pool:
const mockServer = { close: (cb) => { setTimeout(cb, 50); } };
const mockDb = { end: async () => { await new Promise(r => setTimeout(r, 50)); } };

GracefulShutdownManager.initialize(mockServer, mockDb);

// Simulating unhandled rejection crash trigger:
setTimeout(() => {
  Promise.reject(new Error("Database master connection severed!"));
}, 100);
```

---

## 12. Interview Questions & Deep Dives

<details>
<summary><strong>Q1: Why does modern Node.js crash by default on an unhandled promise rejection?</strong></summary>

Historically, Node.js merely logged a deprecation warning on unhandled promise rejections. However, unhandled rejections represent unexpected asynchronous failures that leave applications in an unmanaged, corrupted state (e.g., hanging sockets or half-written database records). Since Node.js v15, unhandled rejections trigger a process exit with code 1 (`--unhandled-rejections=throw`) to enforce fail-fast reliability.
</details>

<details>
<summary><strong>Q2: Why can't a synchronous <code>try...catch</code> block catch an error thrown inside a Promise without <code>await</code>?</strong></summary>

A Promise executes asynchronously. When code inside a Promise throws or rejects, the synchronous execution context of the enclosing function has already finished executing and its stack frame has been popped off the call stack. The rejection occurs in the microtask queue on a later tick of the event loop.
</details>

<details>
<summary><strong>Q3: What is the difference between <code>process.on('uncaughtException')</code> and <code>process.on('uncaughtExceptionMonitor')</code>?</strong></summary>

`uncaughtException` allows you to intercept the error and execute cleanup before exiting; attaching this listener prevents the default Node.js crash behavior (unless you call `process.exit(1)` explicitly). `uncaughtExceptionMonitor` is a passive listener: it executes before `uncaughtException`, cannot prevent process termination, and is intended strictly for telemetry and log shipping.
</details>

<details>
<summary><strong>Q4: What is the purpose of the <code>window.onunhandledrejection</code> event in browsers?</strong></summary>

It intercepts any Promise rejection that has no `.catch()` handler attached at the end of the microtask queue. It allows frontend applications to log unhandled asynchronous exceptions to monitoring services (like Sentry) and call `event.preventDefault()` to suppress default console error spam.
</details>

<details>
<summary><strong>Q5: What happens when an error is thrown inside a <code>.catch()</code> block in a Promise chain?</strong></summary>

Throwing an error inside a `.catch()` callback converts the Promise back into a rejected state with the new error as its rejection reason. The new error skips any remaining `.then()` handlers and bubbles down to the next subsequent `.catch()` block in the chain.
</details>

<details>
<summary><strong>Q6: Why is calling <code>process.exit(1)</code> mandatory after catching an <code>uncaughtException</code>?</strong></summary>

An uncaught exception indicates that an unexpected, unhandled error bypassed all application safeguards. The JavaScript execution state may be corrupted (e.g., broken prototypes, unclosed file locks, or leaked closures). Resuming execution risks silent data corruption and runaway memory leaks.
</details>

<details>
<summary><strong>Q7: What is the <code>rejectionHandled</code> event in Node.js?</strong></summary>

The `rejectionHandled` event is emitted when a rejection handler (`.catch()`) is attached to a Promise *after* it has already triggered an `unhandledRejection` event. It notifies the runtime that a previously unhandled rejection has now been belatedly handled.
</details>

<details>
<summary><strong>Q8: How does <code>async/await</code> simplify error propagation compared to raw Promise chains?</strong></summary>

`async/await` transforms asynchronous Promise rejection propagation into standard synchronous-style call stack bubbling. An error thrown in an awaited asynchronous function bubbles naturally up through caller functions until it reaches a standard `try...catch` block, matching traditional control flow ergonomics.
</details>

---

## 13. Exercises & Challenges

### Exercise 1: Asynchronous Retry Pipeline with Exponential Backoff
Implement `retryAsync(taskFn, maxRetries, baseDelayMs)` that retries an asynchronous operation upon error, multiplying delay by 2 on each failure, and bubbles the final error if all retries fail.

<details>
<summary><strong>Solution</strong></summary>

```javascript
async function retryAsync(taskFn, maxRetries = 3, baseDelayMs = 100) {
  let attempt = 0;
  while (true) {
    try {
      return await taskFn();
    } catch (err) {
      attempt++;
      if (attempt > maxRetries) {
        throw new Error(`Operation failed after ${maxRetries} retries`, { cause: err });
      }
      const delay = baseDelayMs * (2 ** (attempt - 1));
      console.warn(`[Retry #${attempt}] Failed: "${err.message}". Retrying in ${delay}ms...`);
      await new Promise(res => setTimeout(res, delay));
    }
  }
}

// Verification:
let counter = 0;
retryAsync(async () => {
  counter++;
  if (counter < 3) throw new Error("Connection reset");
  return "SUCCESS";
}).then(console.log);
```
</details>

### Exercise 2: Strict Promise Rejection Guard
Write a wrapper function `guardPromise(promise, onUnhandled)` that guarantees `onUnhandled(err)` is invoked if `promise` rejects and no `.catch()` is attached within 10 milliseconds.

<details>
<summary><strong>Solution</strong></summary>

```javascript
function guardPromise(promise, onUnhandled) {
  let isHandled = false;

  promise.catch(() => {
    isHandled = true;
  });

  setTimeout(() => {
    if (!isHandled) {
      promise.catch(err => onUnhandled(err));
    }
  }, 10);

  return promise;
}
```
</details>

### Exercise 3: Global Error Filter for Test Suites
Implement a test utility `suppressUnhandledRejections(testFn)` that temporarily suppresses unhandled rejection output during a test run and verifies that expected background rejections were emitted.

<details>
<summary><strong>Solution</strong></summary>

```javascript
async function captureUnhandledRejections(asyncBlock) {
  const captured = [];
  const handler = (reason) => {
    captured.push(reason);
  };

  process.on("unhandledRejection", handler);
  try {
    await asyncBlock();
    // Allow event loop tick to register pending rejections:
    await new Promise(res => setTimeout(res, 20));
  } finally {
    process.removeListener("unhandledRejection", handler);
  }

  return captured;
}

// Test usage:
captureUnhandledRejections(async () => {
  Promise.reject(new Error("Simulated background rejection"));
}).then(errors => {
  console.log("Captured background errors:", errors.length); // 1
  console.log("Error message:", errors[0].message); // "Simulated background rejection"
});
```
</details>
