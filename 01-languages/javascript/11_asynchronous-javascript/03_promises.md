# JavaScript Promises: States, Microtask Reactions, Chaining & Promise.withResolvers

> Authoritative technical guide to ECMAScript Promises, the Promises/A+ specification, internal slots (`[[PromiseState]]`), reaction jobs in the Microtask Queue, chaining invariants, and ES2024 `Promise.withResolvers`.

| Metadata | Details |
| :--- | :--- |
| **Level** | Core / Engine Architecture |
| **Prerequisites** | [11_asynchronous-javascript/01_sync-vs-async.md](01_sync-vs-async.md), [11_asynchronous-javascript/02_callbacks.md](02_callbacks.md) |
| **Time to Read** | 27 minutes |
| **Status** | Complete |
| **ECMAScript Baseline** | ES2015 (Promise, then, catch) / ES2018 (finally) / ES2024 (Promise.withResolvers) |

---

## Table of Contents
1. [What & Why](#1-what--why)
2. [Core Concepts: States, Invariants & The Executor](#2-core-concepts-states-invariants--the-executor)
3. [How It Works: Engine Microtask Reactions & Internal Slots](#3-how-it-works-engine-microtask-reactions--internal-slots)
4. [Comprehensive Syntax & API Reference](#4-comprehensive-syntax--api-reference)
5. [Practical Examples](#5-practical-examples)
6. [Edge Cases, Pitfalls & Gotchas](#6-edge-cases-pitfalls--gotchas)
7. [Common Mistakes & Anti-Patterns](#7-common-mistakes--anti-patterns)
8. [Performance Characteristics: Microtask Prioritization](#8-performance-characteristics-microtask-prioritization)
9. [Engine Internals: V8 JSPromise & PromiseReactionJob](#9-engine-internals-v8-jspromise--promisereactionjob)
10. [Best Practices & Design Guidelines](#10-best-practices--design-guidelines)
11. [Real-World Case Study: High-Concurrency Request Deduplication & In-Flight Coalescing Cache](#11-real-world-case-study-high-concurrency-request-deduplication--in-flight-coalescing-cache)
12. [Interview Questions & Deep Dives](#12-interview-questions--deep-dives)
13. [Exercises & Challenges](#13-exercises--challenges)

---

## 1. What & Why

Before ECMAScript 2015 (ES6), asynchronous JavaScript relied exclusively on callbacks. As demonstrated in earlier modules, callbacks suffer from Inversion of Control: callers surrender trust to third-party functions regarding when, how often, and how errors are handled.

Standardized in ES2015 (building upon the community **Promises/A+ specification**), a **`Promise`** is an object representing the eventual completion (or failure) of an asynchronous operation and its resulting value.

### The Architectural Guarantees of Promises:
1. **Inverted Control Reclaimed:** Instead of passing your continuation function to a third party, the asynchronous API returns a Promise object to you. You decide how and when to attach `.then()` handlers.
2. **Immutable Settled State:** Once a Promise transitions to fulfilled or rejected, its state and value are permanently frozen. It can never resolve twice or transition between states.
3. **Guaranteed Asynchronous Execution:** Callbacks attached to `.then()` are guaranteed to execute **strictly asynchronously** in the Microtask Queue, eliminating Zalgo bugs forever.
4. **Structured Error Bubbling:** Synchronous exceptions thrown inside any `.then()` handler are automatically converted into rejections and forwarded down the chain.

---

## 2. Core Concepts: States, Invariants & The Executor

### 2.1 The Three Mutually Exclusive States
At any moment, a Promise exists in exactly one of three states:

```
                      +-------------------+
                      |      pending      |
                      | (Initial state)   |
                      +-------------------+
                                |
             +------------------+------------------+
             |                                     |
    resolve(value) invoked                reject(error) invoked
             |                                     |
             v                                     v
+-------------------------+               +-------------------------+
|        fulfilled        |               |        rejected         |
| Result = resolved value |               | Result = rejection err  |
+-------------------------+               +-------------------------+
             \                                     /
              \=======> [SETTLED / RESOLVED] ======/
                       (State is permanently immutable)
```

- **`pending`:** Initial state; operation is actively underway.
- **`fulfilled`:** The operation succeeded; has a permanent result value.
- **`rejected`:** The operation failed; has a permanent rejection reason (error).
- Once a promise is **settled** (either fulfilled or rejected), subsequent calls to `resolve()` or `reject()` are silently ignored.

### 2.2 The Executor Runs Synchronously!
A universal point of confusion:
**The executor function passed to `new Promise((resolve, reject) => { ... })` runs SYNCHRONOUSLY and immediately upon instantiation!**

```javascript
console.log("1. Before constructor");

const p = new Promise((resolve) => {
  console.log("2. Inside executor - SYNCHRONOUS!");
  resolve("Done");
});

console.log("3. After constructor");

p.then(val => console.log("4. Inside .then() - ASYNCHRONOUS microtask:", val));

// Output:
// 1. Before constructor
// 2. Inside executor - SYNCHRONOUS!
// 3. After constructor
// 4. Inside .then() - ASYNCHRONOUS microtask: Done
```

### 2.3 Chaining Returns a Brand New Promise
Every invocation of `.then()`, `.catch()`, or `.finally()` returns a **new Promise**. The fate of the new Promise depends on what the handler returns:
- If the handler returns a primitive or plain object, the new Promise immediately fulfills with that value.
- If the handler returns another Promise (or "thenable"), the new Promise **adopts its state**, waiting for it to settle.
- If the handler throws an error, the new Promise immediately rejects with the thrown error.

---

## 3. How It Works: Engine Microtask Reactions & Internal Slots

Per the ECMA-262 specification, every `Promise` instance contains four internal slots:

| Internal Slot | Type | Description |
| :--- | :--- | :--- |
| `[[PromiseState]]` | `"pending"` \| `"fulfilled"` \| `"rejected"` | Current state of the promise. |
| `[[PromiseResult]]` | `any` | The resolved value or rejection reason. |
| `[[PromiseFulfillReactions]]`| `List<PromiseReaction>` | Queue of success handlers waiting for resolution. |
| `[[PromiseRejectReactions]]` | `List<PromiseReaction>` | Queue of error handlers waiting for rejection. |

```
Step 1: Instantiation & Reaction Registration
const p = new Promise(...);
p.then(handler1);
p.then(handler2);
  |---> p.[[PromiseFulfillReactions]] = [ handler1, handler2 ]

Step 2: Settlement
resolve("data");
  |---> p.[[PromiseState]] = "fulfilled"
  |---> p.[[PromiseResult]] = "data"
  |---> For each reaction in [[PromiseFulfillReactions]]:
        Enqueues a PromiseReactionJob onto the Microtask Queue!
```

---

## 4. Comprehensive Syntax & API Reference

### 4.1 Instance Methods
- `p.then(onFulfilled, onRejected)`: Appends fulfillment and rejection handlers.
- `p.catch(onRejected)`: Syntactic sugar for `p.then(null, onRejected)`.
- `p.finally(onFinally)`: Executes a callback when settled, passing through the original fulfillment value or rejection error untouched (ES2018).

### 4.2 Static Helper Methods
```javascript
// Immediately fulfilled promise:
const fulfilled = Promise.resolve(42);

// Immediately rejected promise:
const rejected = Promise.reject(new Error("Failed"));

// ES2024: Promise.withResolvers()
// Returns { promise, resolve, reject } without nesting inside executor:
const { promise, resolve, reject } = Promise.withResolvers();
resolve("Deferred success!");
promise.then(console.log);
```

---

## 5. Practical Examples

### Example 1: Robust Fetch with Timeout Wrapper
```javascript
function fetchWithTimeout(url, timeoutMs = 5000) {
  return new Promise((resolve, reject) => {
    const timer = setTimeout(() => {
      reject(new Error(`Request timed out after ${timeoutMs}ms for URL: ${url}`));
    }, timeoutMs);

    fetch(url)
      .then(response => {
        clearTimeout(timer);
        resolve(response);
      })
      .catch(err => {
        clearTimeout(timer);
        reject(err);
      });
  });
}
```

### Example 2: Sequential Data Pipeline using `.then()` Chains
```javascript
function processPipeline(userId) {
  return Promise.resolve(userId)
    .then(id => {
      console.log(`Step 1: Fetching user #${id}`);
      return { id, role: "editor", email: "user@domain.com" };
    })
    .then(user => {
      console.log(`Step 2: Checking permissions for ${user.role}`);
      if (user.role !== "admin") {
        throw new Error("AccessDenied: User is not an administrator");
      }
      return user;
    })
    .then(admin => {
      console.log(`Step 3: Loading admin dashboard...`);
      return { ready: true };
    })
    .catch(err => {
      console.error("[Pipeline Error]", err.message);
      return { ready: false, error: err.message }; // Graceful degradation
    });
}

processPipeline(101).then(console.log);
```

### Example 3: Deferred Event Handling via `Promise.withResolvers` (ES2024)
Before ES2024, exposing `resolve` and `reject` outside the executor required clumsy variable lifting. `Promise.withResolvers` provides clean destructuring:
```javascript
class DeferredJobQueue {
  constructor() {
    this.jobs = [];
  }

  enqueueTask(data) {
    // ES2024 native withResolvers:
    const { promise, resolve, reject } = Promise.withResolvers();
    this.jobs.push({ data, resolve, reject });
    return promise;
  }

  processNext() {
    const job = this.jobs.shift();
    if (!job) return;

    try {
      console.log(`Processing job: ${job.data}`);
      job.resolve(`Finished: ${job.data}`);
    } catch (err) {
      job.reject(err);
    }
  }
}

const queue = new DeferredJobQueue();
const resultPromise = queue.enqueueTask("ExportCSV");

resultPromise.then(console.log);
queue.processNext();
// Logs:
// Processing job: ExportCSV
// Finished: ExportCSV
```

### Example 4: Telemetry & Cleanup Invariants with `Promise.prototype.finally`
Unlike `.then()`, `.finally()` does not receive any arguments and passes the original settled outcome forward:
```javascript
let isSpinnerActive = true;

function loadDashboardData() {
  return Promise.resolve("Dashboard JSON Payload")
    .then(data => {
      console.log("Data loaded successfully");
      return data;
    })
    .finally(() => {
      // GUARANTEED: Executes on success AND failure without modifying the payload!
      isSpinnerActive = false;
      console.log("[Cleanup] Loading spinner deactivated.");
    });
}

loadDashboardData().then(payload => {
  console.log("Received payload:", payload);
  console.log("Spinner active?", isSpinnerActive); // false
});
```

### Example 5: Interoperating with Third-Party Thenables
A "thenable" is any object with a `.then()` method. Promises automatically unwrap them:
```javascript
const customThenable = {
  then(resolve, reject) {
    setTimeout(() => resolve("Unwrapped from custom thenable!"), 50);
  }
};

Promise.resolve(customThenable)
  .then(console.log); // "Unwrapped from custom thenable!"
```

---

## 6. Edge Cases, Pitfalls & Gotchas

### 6.1 The "Sync Code in Executor" Freeze
Because the executor function runs synchronously, executing long loops inside `new Promise()` will freeze the call stack before returning the Promise:
```javascript
// DANGEROUS: Blocks before returning the promise!
const p = new Promise((resolve) => {
  for (let i = 0; i < 1e8; i++) {} // Freezes event loop synchronously!
  resolve("Done");
});
```

### 6.2 The `.finally()` Return Trap
Returning a value inside `.finally()` has **no effect**. The original fulfilled value is preserved:
```javascript
Promise.resolve("ORIGINAL")
  .finally(() => {
    return "OVERRIDDEN"; // Ignored!
  })
  .then(console.log); // Prints: "ORIGINAL"
```
*(Exception: If `.finally()` throws an error or returns a rejected promise, the chain rejects with that new error).*

### 6.3 Promise Rejection Swallowing
If an error is thrown inside a `.then()` handler, and no `.catch()` is attached to that specific returned promise, it triggers an `UnhandledPromiseRejection`:
```javascript
const p = Promise.resolve();
p.then(() => {
  throw new Error("Swallowed!"); // Error occurs on the child promise, NOT 'p'!
});
// 'p' remains fulfilled; child promise rejects unhandled!
```

---

## 7. Common Mistakes & Anti-Patterns

| Anti-Pattern | Bad Practice | Idiomatic Solution | Reason |
| :--- | :--- | :--- | :--- |
| **The Explicit Construction Anti-Pattern** | `new Promise((res) => asyncFn().then(res))` | `return asyncFn()` | Redundant Promise wrapping adds unnecessary allocation overhead. |
| **Lifting Resolvers Manually** | Declaring `let res; new Promise(r => res = r)` | Use `Promise.withResolvers()` | Boilerplate-heavy and prone to race condition bugs. |
| **Catching inside Executor** | `new Promise(async (res, rej) => ...)` | Avoid async executors | Errors thrown in async executors cause unhandled promise rejections. |
| **Breaking the Chain** | Calling `.then()` without returning results | `return nextPromise;` inside `.then()` | Disconnects the downstream promise pipeline. |
| **Using `new Promise()` without resolving** | Forgetting to call `resolve()` or `reject()` | Ensure all execution branches settle | Leaves the Promise pending forever, leaking memory. |
| **Expecting `.finally()` to receive value** | `.finally((val) => console.log(val))` | Use `.then()` if value is needed | `.finally` callbacks take zero arguments by specification. |

---

## 8. Performance Characteristics: Microtask Prioritization

1. **Microtasks vs Macrotasks:**
   - Promise resolution callbacks run as **Microtasks**.
   - Timers (`setTimeout`, `setInterval`) and I/O run as **Macrotasks**.
   - When the Call Stack clears, the engine drains the **entire Microtask Queue to completion** before allowing a single macrotask to execute.
2. **Allocation Cost:**
   - A `JSPromise` in V8 occupies $\approx 48\text{ bytes}$.
   - Attaching `.then()` allocates two `PromiseReaction` nodes ($\approx 64\text{ bytes}$).
   - While lightweight, creating millions of Promises in tight loops generates short-lived GC pressure.

---

## 9. Engine Internals: V8 JSPromise & PromiseReactionJob

Inside V8, Promises are implemented in `v8::internal::JSPromise`:

```
                  +-----------------------------------+
                  |      v8::internal::JSPromise      |
                  +-----------------------------------+
                  | flags: (kPending / kFulfilled)    |
                  | reactions_or_result:              |
                  |   - If pending: Points to List    |
                  |     of PromiseReaction objects    |
                  |   - If settled: Points to Result  |
                  +-----------------------------------+
```

### The Microtask Drainage Cycle:
1. When `resolve(val)` is called, V8 sets `flags = kFulfilled` and stores `reactions_or_result = val`.
2. It iterates through the `PromiseReaction` linked list.
3. For each reaction, it allocates a native `PromiseReactionJob` and pushes it to the V8 isolate's **`MicrotaskQueue`**.
4. When JavaScript execution yields, V8 calls `MicrotaskQueue::PerformCheckpoint()`, popping reaction jobs and running handler bytecodes.

---

## 10. Best Practices & Design Guidelines

1. **Always Return Promises from `.then()` Handlers:** Keep chains linear and connected to ensure proper error bubbling.
2. **Never Pass Async Functions to the `new Promise()` Executor:** Write `new Promise((resolve, reject) => { ... })`. Never write `new Promise(async (...) => { ... })`.
3. **Use `Promise.withResolvers()` for Deferred Control:** Standardize on ES2024 `Promise.withResolvers()` instead of manual closure extraction.
4. **Always Terminate Chains with `.catch()`:** Ensure every promise chain has a terminal rejection handler.

---

## 11. Real-World Case Study: High-Concurrency Request Deduplication & In-Flight Coalescing Cache

### Problem
An API gateway queried a user permissions service. When 100 concurrent requests arrived simultaneously for the same user, the gateway dispatched 100 identical HTTP requests to the permissions backend (cache stampede), overloading the database.

### Solution
Implement an **In-Flight Request Coalescing Cache** using Promises. Subsequent requests for the same key share the exact same in-flight Promise instance:

```javascript
class RequestCoalescingCache {
  constructor(fetcherFn) {
    this.fetcherFn = fetcherFn;
    this.inFlight = new Map(); // Maps key -> Promise
  }

  get(key) {
    // If request is already in-flight, return the existing Promise!
    if (this.inFlight.has(key)) {
      console.log(`[Cache Coalesced] Sharing in-flight Promise for key: '${key}'`);
      return this.inFlight.get(key);
    }

    console.log(`[Cache Miss] Dispatched fresh network call for key: '${key}'`);
    const promise = this.fetcherFn(key)
      .finally(() => {
        // Automatically remove from in-flight cache when settled:
        this.inFlight.delete(key);
      });

    this.inFlight.set(key, promise);
    return promise;
  }
}

// Verification:
let networkCallCount = 0;
const mockFetchUserPermissions = async (userId) => {
  networkCallCount++;
  await new Promise(res => setTimeout(res, 50)); // Simulating 50ms latency
  return { userId, roles: ["ADMIN", "BILLING"] };
};

const cache = new RequestCoalescingCache(mockFetchUserPermissions);

// 3 concurrent requests fired simultaneously:
Promise.all([
  cache.get("user_42"),
  cache.get("user_42"),
  cache.get("user_42")
]).then(([res1, res2, res3]) => {
  console.log("All requests received permissions successfully.");
  console.log("Total actual network calls made:", networkCallCount); // EXACTLY 1!
});
```

---

## 12. Interview Questions & Deep Dives

<details>
<summary><strong>Q1: What happens if you call <code>resolve("first")</code> followed immediately by <code>reject(new Error("second"))</code> inside a Promise executor?</strong></summary>

The call to `reject()` is completely ignored. The Promises/A+ specification mandates that a Promise can transition states only once. As soon as `resolve("first")` executes, the internal state becomes permanently `fulfilled`, and any subsequent transition attempts are discarded.
</details>

<details>
<summary><strong>Q2: Why does code inside a <code>new Promise()</code> executor run synchronously, but callbacks passed to <code>.then()</code> run asynchronously?</strong></summary>

The executor function is invoked immediately by the constructor to initiate the asynchronous task. In contrast, `.then()` callbacks represent the completion reaction and are scheduled as **microtasks** per specification. They can only execute after the current call stack has completely cleared.
</details>

<details>
<summary><strong>Q3: What is the purpose of <code>Promise.withResolvers()</code> introduced in ECMAScript 2024?</strong></summary>

`Promise.withResolvers()` provides a clean, non-nested mechanism to create a Promise alongside its `resolve` and `reject` dispatch functions in a single call. It returns `{ promise, resolve, reject }`, eliminating the boilerplate anti-pattern of declaring variables and lifting them from inside an executor closure.
</details>

<details>
<summary><strong>Q4: What is a "thenable" in JavaScript?</strong></summary>

A thenable is any object or function that defines a `.then(onFulfilled, onRejected)` method. Native ECMAScript Promises recognize thenables and automatically unwrap them, enabling interoperability between native Promises, jQuery deferreds, Bluebird promises, and custom asynchronous classes.
</details>

<details>
<summary><strong>Q5: What is the difference between returning a primitive value vs returning a Promise inside a <code>.then()</code> callback?</strong></summary>

Returning a primitive value immediately fulfills the new returned Promise with that value on the next microtask tick. Returning another Promise causes the outer Promise to **adopt its state**: it will remain pending until the inner Promise settles, fulfilling or rejecting with the inner Promise's eventual outcome.
</details>

<details>
<summary><strong>Q6: Why does <code>Promise.prototype.finally()</code> ignore return values?</strong></summary>

`.finally()` is designed strictly for side-effect cleanups (e.g., hiding loading spinners, closing file descriptors) that should not interfere with the pipeline's data flow. Therefore, it passes the original resolved value or rejected error through to the next handler untouched. (Note: if `.finally()` throws an error, the chain rejects with that new error).
</details>

<details>
<summary><strong>Q7: How are Microtasks prioritized relative to standard timers in the Event Loop?</strong></summary>

Microtasks (including Promise reactions and `queueMicrotask`) have strict execution priority over macrotasks (`setTimeout`, `setImmediate`, I/O). The event loop drains the entire microtask queue to completion at the end of every tick before advancing to the next macrotask.
</details>

<details>
<summary><strong>Q8: What is the "Explicit Construction Anti-Pattern"?</strong></summary>

It occurs when a developer unnecessarily wraps an existing Promise inside `new Promise()` (e.g., `new Promise(resolve => fetch(url).then(resolve))`). Instead of wrapping, the developer should simply return the existing Promise directly (`return fetch(url)`), avoiding redundant object allocation and double-wrapping.
</details>

---

## 13. Exercises & Challenges

### Exercise 1: Build a `delay(ms)` Utility Supporting Cancellation
Implement an asynchronous delay utility `delay(ms)` that resolves after `ms` milliseconds but immediately rejects with `"Canceled"` if an optional `AbortSignal` is triggered.

<details>
<summary><strong>Solution</strong></summary>

```javascript
function delay(ms, signal) {
  return new Promise((resolve, reject) => {
    if (signal?.aborted) {
      return reject(new Error("Delay aborted immediately"));
    }

    const timer = setTimeout(() => {
      resolve();
    }, ms);

    signal?.addEventListener("abort", () => {
      clearTimeout(timer);
      reject(new Error("Delay aborted"));
    }, { once: true });
  });
}

// Verification:
const controller = new AbortController();
delay(1000, controller.signal).catch(err => console.log("Caught:", err.message));
controller.abort(); // Rejects immediately without waiting 1000ms!
```
</details>

### Exercise 2: Implementing `Promise.withResolvers` Polyfill
Write a compliant standalone implementation of `withResolvers()` for environments prior to ES2024.

<details>
<summary><strong>Solution</strong></summary>

```javascript
function withResolvers() {
  let resolve;
  let reject;

  const promise = new Promise((res, rej) => {
    resolve = res;
    reject = rej;
  });

  return { promise, resolve, reject };
}

const { promise, resolve } = withResolvers();
promise.then(val => console.log("Resolved:", val));
resolve("Polyfill Success");
```
</details>

### Exercise 3: Sequential Promise Reducer Pipeline
Write a function `runWaterfall(tasks, initialValue)` that executes an array of asynchronous functions sequentially, passing the resolved output of each task as the input to the next.

<details>
<summary><strong>Solution</strong></summary>

```javascript
function runWaterfall(tasks, initialValue) {
  return tasks.reduce((promiseChain, currentTask) => {
    return promiseChain.then(prevResult => currentTask(prevResult));
  }, Promise.resolve(initialValue));
}

const stepA = async (x) => x + 10;
const stepB = async (x) => x * 2;
const stepC = async (x) => `Final Result: ${x}`;

runWaterfall([stepA, stepB, stepC], 5).then(console.log);
// Step A: 5 + 10 = 15
// Step B: 15 * 2 = 30
// Step C: "Final Result: 30"
```
</details>
