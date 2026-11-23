# JavaScript Promise Combinators: all, allSettled, race & any

> Comprehensive architectural guide to ECMAScript Promise combinators (`Promise.all`, `Promise.allSettled`, `Promise.race`, `Promise.any`), short-circuiting semantics, AggregateError, and concurrency control.

| Metadata | Details |
| :--- | :--- |
| **Level** | Core / Architecture |
| **Prerequisites** | [11_asynchronous-javascript/03_promises.md](03_promises.md), [10_error-handling/01_errors.md](../10_error-handling/01_errors.md) |
| **Time to Read** | 25 minutes |
| **Status** | Complete |
| **ECMAScript Baseline** | ES2015 (all, race) / ES2020 (allSettled) / ES2021 (any, AggregateError) |

---

## Table of Contents
1. [What & Why](#1-what--why)
2. [Core Concepts: The 4 Concurrency Combinators](#2-core-concepts-the-4-concurrency-combinators)
3. [How It Works: Engine Short-Circuiting Mechanics](#3-how-it-works-engine-short-circuiting-mechanics)
4. [Syntax & Comparative Reference Matrix](#4-syntax--comparative-reference-matrix)
5. [Practical Examples](#5-practical-examples)
6. [Edge Cases, Pitfalls & Gotchas](#6-edge-cases-pitfalls--gotchas)
7. [Common Mistakes & Anti-Patterns](#7-common-mistakes--anti-patterns)
8. [Performance Characteristics & Concurrency Throttling](#8-performance-characteristics--concurrency-throttling)
9. [Engine Internals: V8 Builtin Implementations](#9-engine-internals-v8-builtin-implementations)
10. [Best Practices & Design Guidelines](#10-best-practices--design-guidelines)
11. [Real-World Case Study: Multi-Region Mirror Redundancy & Resilient Batch Sync](#11-real-world-case-study-multi-region-mirror-redundancy--resilient-batch-sync)
12. [Interview Questions & Deep Dives](#12-interview-questions--deep-dives)
13. [Exercises & Challenges](#13-exercises--challenges)

---

## 1. What & Why

In asynchronous programming, coordinating multiple independent tasks is a frequent requirement:
- Loading a user profile, notification count, and cart items simultaneously.
- Ping testing three different CDN mirrors and adopting the fastest response.
- Executing 500 batch operations and reporting which ones succeeded and which failed.

Executing these operations sequentially using chained `await` statements is an architectural disaster:
```javascript
// CATASTROPHIC SEQUENTIAL EXECUTION: Total time = 100ms + 150ms + 200ms = 450ms!
const user = await fetchUser();       // 100ms
const cart = await fetchCart();       // 150ms
const orders = await fetchOrders();   // 200ms
```

To coordinate concurrent tasks without blocking, ECMAScript provides **four standardized Promise Combinators**:
1. **`Promise.all` (ES2015):** All-or-nothing (fail-fast on first rejection).
2. **`Promise.race` (ES2015):** First-to-settle (settles as soon as the first promise fulfills OR rejects).
3. **`Promise.allSettled` (ES2020):** Complete observation (waits for all to finish, never short-circuits).
4. **`Promise.any` (ES2021):** First-to-succeed (fulfills on first success, rejects with `AggregateError` only if all fail).

---

## 2. Core Concepts: The 4 Concurrency Combinators

```
+-----------------------------------------------------------------------------------------+
|                               The Promise Combinator Quadrant                           |
+-----------------------------------------------------------------------------------------+
|                                    |                                                    |
|         ALL MUST PARTICIPATE       |                 FIRST TO FINISH WINS               |
|                                    |                                                    |
+------------------------------------+----------------------------------------------------+
|                                    |                                                    |
|  Promise.all()                     |  Promise.race()                                    |
|  - Fulfills when ALL fulfill       |  - Settles as soon as the FIRST settles            |
|  - Short-circuits on 1st REJECT    |  - Can fulfill OR reject based on first arrival    |
|                                    |                                                    |
+------------------------------------+----------------------------------------------------+
|                                    |                                                    |
|  Promise.allSettled()              |  Promise.any()                                     |
|  - Waits for ALL to settle         |  - Fulfills as soon as the FIRST FULFILLS          |
|  - NEVER short-circuits            |  - Short-circuits on 1st SUCCESS                   |
|  - Returns status array            |  - Rejects with AggregateError if ALL fail         |
|                                    |                                                    |
+------------------------------------+----------------------------------------------------+
```

---

## 3. How It Works: Engine Short-Circuiting Mechanics

### 3.1 `Promise.all` Short-Circuiting
`Promise.all` maintains an internal countdown counter initialized to the number of input promises.
- As each promise fulfills, the engine stores its result in an output array at its corresponding index.
- If **any** promise rejects, `Promise.all` immediately rejects with that error.
- **Critical Invariant:** Short-circuiting does **NOT** cancel the remaining pending promises! The other promises continue running in the background.

### 3.2 `Promise.any` Aggregate Rejection
`Promise.any` inverts the logic of `Promise.all`:
- It fulfills immediately when the **first** promise fulfills, ignoring any prior or subsequent rejections.
- It maintains an internal rejection counter. Only when the rejection counter reaches the total number of promises does it reject, creating an `AggregateError` containing all rejection errors.

---

## 4. Syntax & Comparative Reference Matrix

| Combinator | Baseline | Empty Input `([])` Behavior | Fulfill Condition | Reject Condition |
| :--- | :--- | :--- | :--- | :--- |
| **`Promise.all`** | ES2015 | Fulfills synchronously with `[]` | When **all** fulfill | When **first** rejects |
| **`Promise.allSettled`** | ES2020 | Fulfills synchronously with `[]` | When **all** settle | **Never** rejects |
| **`Promise.race`** | ES2015 | **Remains pending forever!** | When **first** settles (if fulfilled) | When **first** settles (if rejected) |
| **`Promise.any`** | ES2021 | **Rejects synchronously** with `AggregateError` | When **first** fulfills | When **all** reject |

---

## 5. Practical Examples

### Example 1: Parallel Dashboard Fetching via `Promise.all`
```javascript
async function loadUserDashboard(userId) {
  const startTime = Date.now();

  try {
    // All 3 requests execute concurrently in parallel:
    const [profile, orders, notifications] = await Promise.all([
      fetch(`/api/users/${userId}`).then(r => r.json()),
      fetch(`/api/users/${userId}/orders`).then(r => r.json()),
      fetch(`/api/users/${userId}/notifications`).then(r => r.json())
    ]);

    console.log(`Loaded dashboard concurrently in ${Date.now() - startTime}ms`);
    return { profile, orders, notifications };
  } catch (err) {
    console.error("Dashboard failed to load (one or more sub-requests failed):", err.message);
    throw err;
  }
}
```

### Example 2: Resilient Batch Syncing via `Promise.allSettled`
When processing a batch of records (e.g. sending 1,000 push notifications), you do not want a single failure to abort the entire batch:
```javascript
async function sendBatchNotifications(users, message) {
  const tasks = users.map(user => sendPushNotification(user.id, message));
  const results = await Promise.allSettled(tasks);

  const successful = [];
  const failed = [];

  results.forEach((result, idx) => {
    if (result.status === "fulfilled") {
      successful.push({ userId: users[idx].id, response: result.value });
    } else {
      failed.push({ userId: users[idx].id, error: result.reason.message });
    }
  });

  console.log(`Sent: ${successful.length} OK, ${failed.length} Failed.`);
  return { successful, failed };
}

const mockUsers = [{ id: 1 }, { id: 2 }, { id: 3 }];
const sendPushNotification = (id) => id === 2 ? Promise.reject(new Error("Token expired")) : Promise.resolve("OK");

sendBatchNotifications(mockUsers, "System Update Available").then(console.log);
```

### Example 3: Fastest Mirror Fallback via `Promise.any`
Fetch an asset from three geographically distributed CDNs, picking whichever delivers data first:
```javascript
async function fetchFromFastestCdn(filename) {
  const mirrors = [
    "https://us-east.cdn.com/",
    "https://eu-central.cdn.com/",
    "https://ap-southeast.cdn.com/"
  ];

  const requests = mirrors.map(host => {
    return fetch(host + filename).then(res => {
      if (!res.ok) throw new Error(`HTTP ${res.status} on ${host}`);
      return res.text();
    });
  });

  try {
    // Fulfills with the fastest successful response:
    const content = await Promise.any(requests);
    console.log("Successfully fetched from fastest mirror!");
    return content;
  } catch (err) {
    if (err instanceof AggregateError) {
      console.error("All CDN mirrors failed:");
      err.errors.forEach(e => console.error(" - Mirror error:", e.message));
    }
    throw err;
  }
}
```

### Example 4: Request Timeout Race via `Promise.race`
```javascript
function timeoutPromise(ms) {
  return new Promise((_, reject) => {
    setTimeout(() => reject(new Error(`Timeout exceeded after ${ms}ms`)), ms);
  });
}

async function requestWithHardTimeout(asyncOperation, timeoutMs = 3000) {
  // Whichever promise settles first wins the race:
  return Promise.race([
    asyncOperation(),
    timeoutPromise(timeoutMs)
  ]);
}

// Verification:
const slowTask = () => new Promise(res => setTimeout(() => res("Done"), 5000));
requestWithHardTimeout(slowTask, 1000).catch(err => {
  console.log("Caught:", err.message); // "Caught: Timeout exceeded after 1000ms"
});
```

### Example 5: Implementing `Promise.all` Polyfill from Scratch
```javascript
function promiseAllPolyfill(promises) {
  return new Promise((resolve, reject) => {
    if (!Array.isArray(promises)) {
      return reject(new TypeError("Argument must be an array"));
    }

    const total = promises.length;
    if (total === 0) return resolve([]);

    const results = new Array(total);
    let resolvedCount = 0;

    promises.forEach((p, index) => {
      // Promise.resolve wraps primitives in promises:
      Promise.resolve(p).then(
        value => {
          results[index] = value; // Maintain index ordering
          resolvedCount++;
          if (resolvedCount === total) {
            resolve(results);
          }
        },
        error => {
          reject(error); // Short-circuit on first failure!
        }
      );
    });
  });
}

promiseAllPolyfill([Promise.resolve(1), 2, Promise.resolve(3)]).then(console.log); // [1, 2, 3]
```

---

## 6. Edge Cases, Pitfalls & Gotchas

### 6.1 `Promise.race([])` Remains Pending Forever
If an empty iterable is passed to `Promise.race([])`, the resulting promise **never settles**. It remains pending permanently:
```javascript
const race = Promise.race([]);
console.log(race); // Promise { <pending> } (Will NEVER settle!)
```

### 6.2 `Promise.any([])` Rejects Immediately
In contrast, passing an empty iterable to `Promise.any([])` immediately rejects with an `AggregateError`:
```javascript
Promise.any([]).catch(err => {
  console.log(err instanceof AggregateError); // true
  console.log(err.errors); // [] (Empty error array)
});
```

### 6.3 Short-Circuiting Does Not Cancel Remaining Promises
When `Promise.all([p1, p2, p3])` rejects because `p2` failed, **`p1` and `p3` continue running in the background**. If `p1` or `p3` subsequently reject, their rejections might cause unhandled rejection errors if not guarded!
To actually cancel background requests, use `AbortController`.

---

## 7. Common Mistakes & Anti-Patterns

| Anti-Pattern | Bad Practice | Idiomatic Solution | Reason |
| :--- | :--- | :--- | :--- |
| **Using `all` when partial success is allowed**| `Promise.all(batchOperations)` | `Promise.allSettled(batchOperations)` | A single failure in `Promise.all` causes the entire batch to fail. |
| **Passing Empty Array to `race`** | `Promise.race(dynamicArray)` | Guard: `if (arr.length === 0) return;` | `Promise.race([])` hangs pending forever, causing memory leaks. |
| **Unthrottled `Promise.all` on 100,000 items**| `Promise.all(hugeList.map(fn))` | Concurrency limiter / p-limit | Exhausts network sockets, memory, and database connections. |
| **Assuming `race` aborts losing tasks** | Relying on `race` for cancellation | Use `AbortController` | Losing tasks continue executing and consuming server resources. |
| **Iterating with `forEach` + `async`** | `arr.forEach(async x => await x())` | `await Promise.all(arr.map(fn))` | `forEach` does not await promises; loops finish prematurely. |
| **Using `any` without checking AggregateError**| Assuming error is a single Error | Inspect `err.errors` array | `Promise.any` wraps multiple rejections in `AggregateError`. |

---

## 8. Performance Characteristics & Concurrency Throttling

### 8.1 The Unbounded Concurrency Trap
If you execute `Promise.all(urls.map(fetch))` on 10,000 URLs, your application will simultaneously open 10,000 sockets. This triggers `EMFILE: too many open files`, exhausts heap memory, and trips remote server rate limits.

### 8.2 Implementing Concurrency Throttling (p-limit Pattern)
```javascript
function createConcurrencyPool(limit) {
  const queue = [];
  let active = 0;

  function runNext() {
    if (active >= limit || queue.length === 0) return;
    
    active++;
    const { taskFn, resolve, reject } = queue.shift();

    Promise.resolve(taskFn())
      .then(resolve, reject)
      .finally(() => {
        active--;
        runNext();
      });
  }

  return function (taskFn) {
    return new Promise((resolve, reject) => {
      queue.push({ taskFn, resolve, reject });
      runNext();
    });
  };
}

// Verification: Run 10 tasks with concurrency limit = 2:
const limit = createConcurrencyPool(2);
const tasks = Array.from({ length: 5 }, (_, i) => () => {
  console.log(`Starting task #${i + 1}`);
  return new Promise(res => setTimeout(() => {
    console.log(`Finished task #${i + 1}`);
    res(i + 1);
  }, 100));
});

Promise.all(tasks.map(t => limit(t))).then(console.log);
```

---

## 9. Engine Internals: V8 Builtin Implementations

Inside V8, Promise combinators are implemented as high-performance internal built-ins written in Torque and C++:
- `v8::internal::Builtin::PromiseAll`
- `v8::internal::Builtin::PromiseAllSettled`
- `v8::internal::Builtin::PromiseAny`
- `v8::internal::Builtin::PromiseRace`

```
 User calls: Promise.all(iterable)
                   |
                   v
 V8 Builtin::PromiseAll
                   |
                   v
 Allocates internal context holding:
  - count (remaining unresolved counter)
  - values (fixed array for storing results)
  - has_error (boolean flag)
                   |
                   v
 For each item, generates microtask closure resolving directly into V8 fixed array slot
```
Because results are inserted directly into indexed C++ array slots, V8 avoids dynamic array reallocations and provides maximum performance.

---

## 10. Best Practices & Design Guidelines

1. **Use `Promise.all` for Strictly Dependent Operations:** Use `Promise.all` when all data is mandatory (e.g. assembling an invoice that requires both user address and payment token).
2. **Use `Promise.allSettled` for Bulk Operations:** Use `Promise.allSettled` for batch updates, email sending, and cache invalidation.
3. **Always Throttle High-Volume Tasks:** Never pass unconstrained arrays to `Promise.all`. Cap concurrency with a pool limiter.
4. **Use `AbortController` Alongside `Promise.race`:** If racing network requests against a timeout, abort the losing network request via its `AbortSignal`.

---

## 11. Real-World Case Study: Multi-Region Mirror Redundancy & Resilient Batch Sync

### Problem
A video-streaming platform needed to authenticate users against global auth servers. In high-latency regions (Australia, South America), single-endpoint requests took 1,200ms. If the primary US server suffered an outage, the whole app went down.

### Solution
Deploy a dual-combinator strategy:
1. `Promise.any` to query US, EU, and APAC auth endpoints concurrently, fulfilling on the fastest mirror.
2. `Promise.allSettled` in the background to synchronize local user telemetry across all secondary mirrors without blocking the user:

```javascript
class ResilientAuthGateway {
  static ENDPOINTS = [
    "https://us.auth.service.com",
    "https://eu.auth.service.com",
    "https://apac.auth.service.com"
  ];

  static async authenticateUser(credentials) {
    // 1. Race all auth mirrors; adopt whichever validates credentials first:
    const authRequests = this.ENDPOINTS.map(url => {
      return fetch(`${url}/verify`, { method: "POST", body: JSON.stringify(credentials) })
        .then(res => {
          if (!res.ok) throw new Error(`Mirror ${url} returned ${res.status}`);
          return res.json();
        });
    });

    try {
      const userSession = await Promise.any(authRequests);
      console.log(`[Auth Gateway] Successfully authenticated via fastest mirror.`);
      
      // 2. Background sync to all regions (never blocks login):
      this.syncTelemetryAsync(userSession.userId);
      return userSession;
    } catch (err) {
      if (err instanceof AggregateError) {
        console.error("[FATAL] All global auth mirrors failed verification!");
      }
      throw err;
    }
  }

  static syncTelemetryAsync(userId) {
    const syncRequests = this.ENDPOINTS.map(url => {
      return fetch(`${url}/telemetry`, { method: "POST", body: JSON.stringify({ userId }) });
    });

    // allSettled guarantees background logging never crashes the process:
    Promise.allSettled(syncRequests).then(results => {
      const successfulMirrors = results.filter(r => r.status === "fulfilled").length;
      console.log(`[Telemetry] Synced session to ${successfulMirrors}/${this.ENDPOINTS.length} mirrors.`);
    });
  }
}
```

---

## 12. Interview Questions & Deep Dives

<details>
<summary><strong>Q1: What is the primary difference between <code>Promise.all()</code> and <code>Promise.allSettled()</code>?</strong></summary>

`Promise.all()` is all-or-nothing: if any single promise rejects, it immediately short-circuits and rejects with that error, discarding other results. `Promise.allSettled()` never short-circuits: it waits for every input promise to either fulfill or reject, returning an array of descriptor objects: `{ status: "fulfilled", value }` or `{ status: "rejected", reason }`.
</details>

<details>
<summary><strong>Q2: How does <code>Promise.any()</code> differ from <code>Promise.race()</code>?</strong></summary>

`Promise.race()` settles as soon as the **first** promise settles (either fulfilling OR rejecting). `Promise.any()` fulfills as soon as the **first** promise **fulfills** (it ignores rejections). It only rejects if **all** input promises reject, throwing an `AggregateError`.
</details>

<details>
<summary><strong>Q3: What happens if an empty array <code>[]</code> is passed to each of the four combinators?</strong></summary>

- `Promise.all([])`: Fulfills synchronously with `[]`.
- `Promise.allSettled([])`: Fulfills synchronously with `[]`.
- `Promise.race([])`: **Remains pending forever** (never settles).
- `Promise.any([])`: **Rejects synchronously** with `AggregateError`.
</details>

<details>
<summary><strong>Q4: If one promise rejects in <code>Promise.all([p1, p2, p3])</code>, are the other running operations aborted?</strong></summary>

No. `Promise.all` short-circuits its own returned promise, but JavaScript does not cancel the underlying operations of the remaining promises. They continue executing in the background. To actually cancel in-flight network requests, an explicit `AbortController` must be passed to each operation.
</details>

<details>
<summary><strong>Q5: What is an <code>AggregateError</code> and which combinator produces it?</strong></summary>

`AggregateError` is a native error subclass standardized in ES2021. It represents a single error that bundles multiple underlying errors in its `.errors` array property. It is produced by `Promise.any()` when every promise in the input iterable rejects.
</details>

<details>
<summary><strong>Q6: Why is calling <code>Promise.all()</code> on 10,000 items dangerous, and how do you fix it?</strong></summary>

Passing an unbounded list of tasks to `Promise.all()` attempts to execute all 10,000 tasks concurrently, which exhausts operating system socket descriptors, overloads remote databases, and risks heap out-of-memory crashes. The fix is to use a concurrency limiter (like the `p-limit` pattern) to process tasks in batches of $N$ (e.g. 5–10 concurrent workers).
</details>

<details>
<summary><strong>Q7: How does <code>Promise.all()</code> preserve the order of results if promises settle out of order?</strong></summary>

`Promise.all` allocates an output array sized to the length of the input iterable. As each promise resolves, its result is stored at its **original input index** (`results[index] = val`), not at the order of completion, ensuring the returned array strictly matches the input sequence.
</details>

<details>
<summary><strong>Q8: Can non-promise values be passed to Promise combinators?</strong></summary>

Yes. All four combinators wrap each element of the input iterable using `Promise.resolve(element)`. Primitive values (numbers, strings) are treated as immediately fulfilled promises.
</details>

---

## 13. Exercises & Challenges

### Exercise 1: Implement `Promise.any` Polyfill
Write a standalone polyfill for `Promise.any(promises)` that rejects with an `AggregateError` if all promises fail.

<details>
<summary><strong>Solution</strong></summary>

```javascript
function promiseAnyPolyfill(promises) {
  return new Promise((resolve, reject) => {
    if (!Array.isArray(promises)) {
      return reject(new TypeError("Argument must be an array"));
    }

    const total = promises.length;
    if (total === 0) {
      return reject(new AggregateError([], "All promises were rejected"));
    }

    const errors = new Array(total);
    let rejectedCount = 0;

    promises.forEach((p, index) => {
      Promise.resolve(p).then(
        value => resolve(value), // Fulfill on first success!
        error => {
          errors[index] = error;
          rejectedCount++;
          if (rejectedCount === total) {
            reject(new AggregateError(errors, "All promises were rejected"));
          }
        }
      );
    });
  });
}

// Verification:
promiseAnyPolyfill([
  Promise.reject("Err 1"),
  Promise.resolve("Success!"),
  Promise.reject("Err 2")
]).then(console.log); // "Success!"
```
</details>

### Exercise 2: Implementing `Promise.allSettled` Polyfill
Implement `promiseAllSettledPolyfill(promises)` using standard `Promise.all`.

<details>
<summary><strong>Solution</strong></summary>

```javascript
function promiseAllSettledPolyfill(promises) {
  return Promise.all(
    promises.map(p => {
      return Promise.resolve(p).then(
        value => ({ status: "fulfilled", value }),
        reason => ({ status: "rejected", reason })
      );
    })
  );
}

promiseAllSettledPolyfill([Promise.resolve(42), Promise.reject("Boom")]).then(console.log);
// [
//   { status: 'fulfilled', value: 42 },
//   { status: 'rejected', reason: 'Boom' }
// ]
```
</details>

### Exercise 3: First Successful or Timeout Race
Implement a function `firstSuccessfulWithin(promises, timeoutMs)` that returns the first successful promise within the timeout, or rejects with a timeout error if no promise succeeds in time.

<details>
<summary><strong>Solution</strong></summary>

```javascript
function firstSuccessfulWithin(promises, timeoutMs) {
  const timeout = new Promise((_, reject) => {
    setTimeout(() => reject(new Error(`Timeout of ${timeoutMs}ms exceeded without success`)), timeoutMs);
  });

  // Race timeout against Promise.any:
  return Promise.race([
    Promise.any(promises),
    timeout
  ]);
}

const slowOk = new Promise(r => setTimeout(() => r("Slow OK"), 500));
const fastFail = Promise.reject("Failed fast");

firstSuccessfulWithin([slowOk, fastFail], 1000).then(console.log); // "Slow OK"
```
</details>
