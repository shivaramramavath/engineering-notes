# Callbacks, Inversion of Control & Asynchronous Execution

> An exhaustive technical reference on JavaScript callbacks: synchronous vs asynchronous execution, error-first conventions, the "Pyramid of Doom", the "Zalgo" anti-pattern, `map(parseInt)` arity traps, and promisification.

| Metadata | Details |
| :--- | :--- |
| **Level** | Beginner to Intermediate |
| **Prerequisites** | [01_declarations-and-expressions.md](./01_declarations-and-expressions.md), [03_parameters-and-arguments.md](./03_parameters-and-arguments.md) |
| **Time to Read** | 25 minutes |
| **Status** | Complete |
| **ECMAScript Standard** | ES6 (ES2015) to Current (ES2024/ES2025) |

---

## Table of Contents
1. [What & Why](#1-what--why)
2. [Core Concepts](#2-core-concepts)
3. [How It Works: Call Stack vs. Event Loop Queues](#3-how-it-works-call-stack-vs-event-loop-queues)
4. [Syntax & Callback Conventions Reference](#4-syntax--callback-conventions-reference)
5. [Practical Examples](#5-practical-examples)
6. [Edge Cases & Gotchas](#6-edge-cases--gotchas)
7. [Common Mistakes & Anti-Patterns](#7-common-mistakes--anti-patterns)
8. [Performance Characteristics](#8-performance-characteristics)
9. [Engine Internals: V8 Call Frames & Libuv Event Loop Tasks](#9-engine-internals-v8-call-frames--libuv-event-loop-tasks)
10. [Best Practices & Style Guide](#10-best-practices--style-guide)
11. [Real-World Applications & Case Studies](#11-real-world-applications--case-studies)
12. [Interview Questions & Deep Dives](#12-interview-questions--deep-dives)
13. [Exercises & Challenges](#13-exercises--challenges)

---

## 1. What & Why

A **callback** is a function passed as an argument into another host function, intended to be executed at a later time or under specific conditions.

Callbacks represent **Inversion of Control (IoC)**: instead of code imperatively polling for data or executing linear steps, you supply executable logic to a host API and delegate execution responsibility:
- **Synchronous Callbacks**: Invoked immediately on the current execution stack during the host function's runtime (e.g. `Array.prototype.map`, `Array.prototype.sort`).
- **Asynchronous Callbacks**: Deferred to a future turn of the Event Loop following network I/O, file access, or timer expiration (e.g. `setTimeout`, `fs.readFile`, DOM click handlers).

```
Synchronous Execution (Single Stack Turn):
  Main Code ──► Host Function ──► [ Executes Callback Immediately ] ──► Resumes Main

Asynchronous Execution (Deferred via Event Loop):
  Main Code ──► Schedules Async I/O ──► Main Finishes Stack Frame
                     │
                     ▼ (Network I/O completes)
  Event Loop Task Queue ──► [ Callback Pops onto Fresh Call Stack ] ──► Executes
```

While Promises and `async/await` have modernized asynchronous workflows, callbacks remain the fundamental substrate of event emitters, streams, functional array methods, and low-level Node.js operating system bindings.

---

## 2. Core Concepts

### 2.1 Synchronous vs. Asynchronous Callbacks

| Characteristic | Synchronous Callbacks | Asynchronous Callbacks |
| :--- | :--- | :--- |
| **Execution Timing** | Immediately within the calling stack turn | Deferred to future Event Loop tick |
| **Stack Trace** | Preserves full caller stack trace | Loses initial scheduling caller frame |
| **Error Handling** | Standard `try / catch` works | `try / catch` around scheduler fails! |
| **Blocking Behavior** | Blocks main thread until callback finishes | Non-blocking; frees thread for other tasks |
| **Examples** | `[].map()`, `[].filter()`, `[].sort()` | `setTimeout()`, `addEventListener()`, `fs.readFile()` |

### 2.2 The Node.js Error-First Callback Convention
Node.js standardized the **Node-Style Callback** (or "Error-First Callback"):
```javascript
function callback(err, data) {
  if (err) {
    // Handle error
    return;
  }
  // Process data
}
```
1. The first parameter is reserved for an `Error` object. If the operation succeeded, `err` is `null` or `undefined`.
2. The second (and subsequent) parameters deliver successful payload data.

### 2.3 The "Pyramid of Doom" (Callback Hell)
When multiple dependent asynchronous operations are sequenced using callbacks, code indents horizontally, forming a pyramid:
```javascript
getUser(userId, (err, user) => {
  if (err) return handleError(err);
  getOrders(user.id, (err, orders) => {
    if (err) return handleError(err);
    getOrderDetails(orders[0].id, (err, details) => {
      if (err) return handleError(err);
      // Deeply nested, error-prone, fragile control flow
    });
  });
});
```
This historical anti-pattern led directly to the standardization of ES2015 Promises and ES2017 `async/await`.

### 2.4 The Arity Mismatch Trap: `map(parseInt)`
Passing multi-parameter functions directly into array callbacks is a classic JavaScript trap:
```javascript
["1", "2", "3"].map(parseInt); // Returns: [1, NaN, NaN]!
```
- **Why?**: `Array.prototype.map` passes **3 arguments** to its callback: `(item, index, array)`.
- `parseInt` accepts **2 parameters**: `parseInt(string, radix)`.
- Step 1: `parseInt("1", 0)` -> Radix 0 defaults to base 10 -> `1`.
- Step 2: `parseInt("2", 1)` -> Radix 1 is invalid in base-1 -> `NaN`.
- Step 3: `parseInt("3", 2)` -> Binary base-2 cannot contain the digit '3' -> `NaN`.

---

## 3. How It Works: Call Stack vs. Event Loop Queues

```mermaid
sequenceDiagram
    autonumber
    participant Main as Main Script (Call Stack)
    participant WebAPI as Browser / Libuv Threadpool
    participant Queue as Task Queue (Macrotask)
    participant Loop as Event Loop

    Main->>WebAPI: setTimeout(callback, 50)
    Note over Main: Timer registered in C++ background
    Main->>Main: Continue synchronous execution
    Note over Main: Call Stack becomes EMPTY

    WebAPI-->>Queue: Push callback after 50ms
    Loop->>Main: Check: Is Call Stack empty?
    Loop->>Queue: Dequeue callback
    Queue-->>Main: Push callback to Call Stack
    Main->>Main: Execute callback statements
```

When an asynchronous callback is registered:
1. The JavaScript engine delegates the timer or I/O operation to the host environment (Browser Web APIs or Node.js `libuv`).
2. The current JavaScript function returns, and the call stack unwinds completely.
3. When the background operation completes, the host environment places the callback into the **Task Queue** (Macrotask Queue).
4. The **Event Loop** waits until the Call Stack is completely clear, then dequeues the callback and pushes it onto a **brand-new activation stack frame**.

---

## 4. Syntax & Callback Conventions Reference

### Standard Callback Patterns

```javascript
// 1. Synchronous Predicate Callback
const hasAdmin = users.some(user => user.role === "admin");

// 2. Synchronous Comparator Callback
const sorted = items.sort((a, b) => a.priority - b.priority);

// 3. Node.js Error-First Asynchronous Callback
import fs from "node:fs";
fs.readFile("config.json", "utf8", (err, content) => {
  if (err) {
    console.error("Failed to read file:", err);
    return;
  }
  console.log("File content loaded:", content.length);
});

// 4. DOM Event Listener Callback
button.addEventListener("click", function onClick(event) {
  console.log("Button clicked at coordinates:", event.clientX, event.clientY);
});
```

---

## 5. Practical Examples

### Example 1: Solving the `map(parseInt)` Arity Bug

```javascript
const rawNumbers = ["10", "20", "30"];

// FLAWED:
console.log("Flawed map:", rawNumbers.map(parseInt)); // [10, NaN, NaN]

// SOLUTION 1: Explicit unary arrow function
const clean1 = rawNumbers.map((str) => parseInt(str, 10));
console.log("Explicit arrow:", clean1); // [10, 20, 30]

// SOLUTION 2: Using the Number constructor (accepts only 1 argument)
const clean2 = rawNumbers.map(Number);
console.log("Using Number:", clean2); // [10, 20, 30]

// SOLUTION 3: Unary higher-order function decorator
const unary = (fn) => (arg) => fn(arg);
const clean3 = rawNumbers.map(unary(parseInt));
console.log("Using unary decorator:", clean3); // [10, 20, 30]
```

### Example 2: The "Zalgo" Trap & The `queueMicrotask` Fix

"Releasing Zalgo" refers to an API that executes **synchronously sometimes and asynchronously other times**, introducing unpredictable race conditions:

```javascript
// DANGEROUS: Inconsistent timing based on cache state!
const cache = new Map();

function fetchUserDataZalgo(userId, callback) {
  if (cache.has(userId)) {
    // BUG: Invoked synchronously on cache hit!
    callback(null, cache.get(userId));
    return;
  }
  // Invoked asynchronously on network fetch!
  setTimeout(() => {
    const data = { id: userId, name: "Grace" };
    cache.set(userId, data);
    callback(null, data);
  }, 50);
}

// SAFE ARCHITECTURE: Guarantee 100% Asynchronous Execution via queueMicrotask
function fetchUserDataSafe(userId, callback) {
  if (cache.has(userId)) {
    // Defer callback to microtask queue, ensuring caller always finishes first!
    queueMicrotask(() => {
      callback(null, cache.get(userId));
    });
    return;
  }
  setTimeout(() => {
    const data = { id: userId, name: "Grace" };
    cache.set(userId, data);
    callback(null, data);
  }, 50);
}

// Verification:
let initialized = false;
fetchUserDataSafe("usr_1", (err, data) => {
  console.log("Callback executed, initialized is:", initialized); // Always TRUE!
});
initialized = true; // Synchronous code finishes before callback runs!
```

### Example 3: Promisifying Error-First Callbacks from Scratch

```javascript
/**
 * Polyfill for Node.js util.promisify: converts error-first callbacks into Promises.
 * @param {Function} originalFn
 * @returns {Function}
 */
function promisify(originalFn) {
  return function (...args) {
    return new Promise((resolve, reject) => {
      // Append custom error-first callback to args
      originalFn.call(this, ...args, (err, result) => {
        if (err) {
          reject(err);
        } else {
          resolve(result);
        }
      });
    });
  };
}

// Emulating a callback-based API
function legacyFetch(url, callback) {
  setTimeout(() => {
    if (url.startsWith("https://")) {
      callback(null, { status: 200, body: "Secure Payload" });
    } else {
      callback(new Error("Insecure protocol rejected"));
    }
  }, 20);
}

// Promisified usage:
const modernFetch = promisify(legacyFetch);

modernFetch("https://api.domain.com")
  .then((res) => console.log("Promisified Success:", res.body))
  .catch((err) => console.error("Promisified Error:", err.message));
```

### Example 4: Asynchronous Retry Mechanism with Exponential Backoff

```javascript
/**
 * Retries an asynchronous callback with exponential backoff delay.
 * @param {Function} taskFn - Function accepting (callback)
 * @param {number} retries - Maximum retry attempts
 * @param {number} delayMs - Initial delay in milliseconds
 * @param {Function} finalCallback - Error-first completion callback
 */
function retryWithBackoff(taskFn, retries, delayMs, finalCallback) {
  taskFn((err, data) => {
    if (!err) {
      return finalCallback(null, data);
    }

    if (retries <= 0) {
      return finalCallback(new Error(`All retries exhausted. Last error: ${err.message}`));
    }

    console.warn(`[RETRY] Attempt failed (${err.message}). Retrying in ${delayMs}ms...`);
    setTimeout(() => {
      retryWithBackoff(taskFn, retries - 1, delayMs * 2, finalCallback);
    }, delayMs);
  });
}

// Flaky task simulation
let attempts = 0;
const flakyNetworkCall = (cb) => {
  attempts++;
  if (attempts < 3) {
    cb(new Error("Network Gateway Timeout 504"));
  } else {
    cb(null, "Connected Successfully on Attempt 3!");
  }
};

retryWithBackoff(flakyNetworkCall, 3, 50, (err, res) => {
  if (err) console.error("Failed:", err.message);
  else console.log("Final Outcome:", res);
});
```

### Example 5: Custom Batch Processor with Early Abort

```javascript
/**
 * Synchronous array iteration callback with early termination capability.
 */
function iterateWhile(items, callback) {
  for (let i = 0; i < items.length; i++) {
    const shouldContinue = callback(items[i], i, items);
    if (shouldContinue === false) {
      console.log(`[ABORT] Iteration stopped early at index ${i}`);
      break;
    }
  }
}

const numbers = [10, 20, 30, -5, 40, 50];
iterateWhile(numbers, (num, idx) => {
  if (num < 0) {
    console.warn(`Encountered negative number (${num})`);
    return false; // Halts iteration
  }
  console.log(`Valid item [${idx}]: ${num}`);
});
```

---

## 6. Edge Cases & Gotchas

### 6.1 Uncaught Exceptions Inside Asynchronous Callbacks
Wrapping an asynchronous scheduler in a `try...catch` block **cannot catch errors thrown inside the callback**:
```javascript
try {
  setTimeout(() => {
    throw new Error("Fatal async crash!"); // CRASHES PROCESS!
  }, 10);
} catch (err) {
  // NEVER RUNS! The try/catch block already exited 10ms ago!
  console.log("Caught:", err.message);
}
```
**Defensive Fix**: Asynchronous errors must be caught inside the callback itself or routed through Promises / event listeners.

### 6.2 Context (`this`) Loss When Passing Object Methods
Passing an object method as a callback severs its connection to `this`:
```javascript
const service = {
  name: "AuthService",
  login() {
    console.log(`Logging into ${this.name}`);
  }
};

// DANGER: Method passed by reference; 'this' defaults to globalThis or undefined
setTimeout(service.login, 50); // "Logging into undefined"

// FIX: Wrap in an arrow function or use .bind():
setTimeout(() => service.login(), 50);
```

### 6.3 Invoking a Callback Multiple Times
A buggy asynchronous function might accidentally call its completion callback multiple times, leading to duplicate database inserts or double-free errors:
```javascript
function makeSingleUseCallback(fn) {
  let called = false;
  return function (...args) {
    if (called) {
      console.warn("Defensive guard: Callback invoked more than once!");
      return;
    }
    called = true;
    return Reflect.apply(fn, this, args);
  };
}
```

---

## 7. Common Mistakes & Anti-Patterns

| Anti-Pattern | Why It Fails | Correct Approach |
| :--- | :--- | :--- |
| **Passing multi-argument functions to `map()`** | Secondary parameters (`index`, `array`) corrupt functions like `parseInt`. | Use an explicit arrow function: `(x) => parseInt(x, 10)`. |
| **Releasing Zalgo (Mixed Sync/Async)** | Introduces race conditions when cached data fires synchronously. | Enforce consistent async execution using `queueMicrotask()`. |
| **Using `try...catch` around `setTimeout`** | Call stack unwinds before the callback fires; exceptions are missed. | Catch errors inside the callback itself. |
| **Passing object methods directly as callbacks** | Method loses its `this` binding. | Wrap in an arrow function: `() => obj.method()`. |
| **Forgetting to return after error callback** | Execution falls through and runs success logic after error handling. | Always write `return callback(err);`. |
| **Nesting deep callbacks ("Callback Hell")** | Obscures logic, breaks stack traces, and complicates error handling. | Refactor to Promises or `async/await`. |

---

## 8. Performance Characteristics

In modern V8:
- **Synchronous Inline Callbacks**: When passing callbacks to `map`, `filter`, or `reduce`, TurboFan attempts to inline the callback bytecode into the loop body, eliminating function call overhead entirely.
- **Asynchronous Task Overhead**: Each asynchronous callback requires allocating a Task node in the C++ event loop queue, storing closure contexts, and scheduling an Event Loop turn (~0.1ms to 1.0ms latency).
- **Stack Trace Allocation**: Capturing asynchronous stack traces across deep callback chains incurs memory overhead in debuggers.

---

## 9. Engine Internals: V8 Call Frames & Libuv Event Loop Tasks

When Node.js processes an asynchronous callback via `fs.readFile`:
1. **Delegation**: V8 executes the C++ binding `node::fs::ReadFile`. The request is dispatched to the **Libuv worker thread pool**.
2. **Execution**: The worker thread executes the POSIX `read()` system call asynchronously.
3. **Completion**: When data is ready, Libuv posts a completion notification to the main thread's I/O event loop queue.
4. **V8 Entry**: On the next loop turn, the main thread pulls the event from Libuv and calls `v8::Function::Call()`, pushing the callback onto the call stack.

---

## 10. Best Practices & Style Guide

1. **Always Return on Error Callbacks**: Prevent execution fall-through bugs:
   ```javascript
   if (err) return callback(err);
   ```
2. **Promisify Legacy Callback APIs**: Convert Node.js callback functions using `node:util.promisify` or native Promises.
3. **Never Release Zalgo**: A function must be 100% synchronous or 100% asynchronous; never mix them based on internal caching.
4. **Guard Multi-Arg Callbacks**: Never pass `parseInt` directly into `[].map()`.

---

## 11. Real-World Applications & Case Studies

### Case Study: Node.js Double-Response Outage
A payment gateway service crashed in production during a spike in failed credit card charges:
```javascript
// BUGGY CONTROLLER:
function handlePayment(req, res) {
  processCharge(req.body, (err, transaction) => {
    if (err) {
      res.status(400).json({ error: err.message });
      // MISSING 'return'! Execution continues!
    }
    res.status(200).json({ transactionId: transaction.id });
  });
}
```
- **Symptom**: On payment failure, the server attempted to write to `res` twice, throwing:
  `ERR_HTTP_HEADERS_SENT: Cannot set headers after they are sent to the client`.
- **Impact**: Uncaught exception crashed the Node.js process instance, triggering service restarts across the cluster.
- **Fix**: Added the `return` keyword: `if (err) return res.status(400)...`.

---

## 12. Interview Questions & Deep Dives

<details>
<summary><strong>1. Explain why `["1", "2", "3"].map(parseInt)` returns `[1, NaN, NaN]`.</strong></summary>

**Answer:**
`Array.prototype.map` passes three arguments to its callback on each iteration: `(element, index, array)`. `parseInt` accepts two parameters: `(string, radix)`.
1. Iteration 0: `parseInt("1", 0)`. A radix of `0` defaults to base 10, returning `1`.
2. Iteration 1: `parseInt("2", 1)`. Base 1 is an invalid radix (radix must be between 2 and 36), returning `NaN`.
3. Iteration 2: `parseInt("3", 2)`. In binary base 2, only digits `0` and `1` are valid. Since `"3"` is invalid in binary, parsing fails and returns `NaN`.
</details>

<details>
<summary><strong>2. What does "releasing Zalgo" mean and why is it considered a critical anti-pattern?</strong></summary>

**Answer:**
"Releasing Zalgo" refers to an asynchronous-looking API that executes its callback synchronously under certain conditions (e.g. cache hits) and asynchronously under others (e.g. cache misses). This creates unpredictable execution order bugs because the caller cannot know whether code following the invocation runs before or after the callback. APIs must be consistently synchronous or consistently asynchronous (using `queueMicrotask` to defer synchronous returns).
</details>

<details>
<summary><strong>3. Why can't a standard `try...catch` block catch an error thrown inside `setTimeout`?</strong></summary>

**Answer:**
`setTimeout` is an asynchronous scheduling API. When `setTimeout(fn, 100)` executes, it registers the timer with the host environment and returns immediately. The enclosing `try...catch` block exits and its stack frame is popped from the call stack. When the callback `fn` executes 100ms later, it runs on a brand-new call stack initiated by the Event Loop; the original `try...catch` block no longer exists to intercept the exception.
</details>

<details>
<summary><strong>4. What is the Node.js "Error-First" callback convention?</strong></summary>

**Answer:**
The error-first convention mandates that the first parameter of any asynchronous callback is reserved for an error object (`err`). If the asynchronous operation fails, `err` contains an instance of `Error`; if it succeeds, `err` is `null` or `undefined`, and successful result values are passed in the subsequent arguments (`callback(null, result)`).
</details>

<details>
<summary><strong>5. How does `util.promisify` work in Node.js?</strong></summary>

**Answer:**
`util.promisify(fn)` takes a function that follows the Node.js error-first callback convention and returns a wrapper function returning a Promise. When called, the wrapper passes its own custom callback as the final argument. If the callback receives an error (`err`), the Promise is rejected; if `err` is falsy, the Promise is resolved with the second argument.
</details>

<details>
<summary><strong>6. What is the difference between passing an anonymous function vs. a named function as a callback?</strong></summary>

**Answer:**
Functionally they execute identically, but named callbacks provide:
1. **Descriptive Stack Traces**: Crash reports display the function's name instead of `anonymous`, vastly accelerating debugging.
2. **Reusability**: Named functions can be registered and subsequently unregistered from event listeners (`removeEventListener`).
3. **Self-Referential Recursion**: The function can reference itself cleanly.
</details>

<details>
<summary><strong>7. How can you ensure a callback is only executed once?</strong></summary>

**Answer:**
Wrap the callback in an "once" higher-order function that uses a closure boolean flag:
```javascript
function once(fn) {
  let executed = false;
  return function (...args) {
    if (!executed) {
      executed = true;
      return fn.apply(this, args);
    }
  };
}
```
</details>

<details>
<summary><strong>8. What is the difference between a Microtask and a Macrotask callback?</strong></summary>

**Answer:**
- **Macrotasks** (Task Queue): Callbacks from `setTimeout`, `setInterval`, `setImmediate`, and I/O. The Event Loop processes only **one** macrotask per tick before checking other queues and rendering.
- **Microtasks**: Callbacks from `Promise.then()`, `queueMicrotask()`, and `process.nextTick()`. The Event Loop drains the **entire microtask queue to exhaustion** before moving to the next macrotask or rendering frame.
</details>

---

## 13. Exercises & Challenges

### Exercise 1 (Easy): Safe Array ParseInt
**Task**: Use a clean higher-order unary wrapper or arrow function to parse an array of string numbers into base-10 integers without producing `NaN`.

<details>
<summary><strong>Solution</strong></summary>

```javascript
const stringNumbers = ["10", "20", "30", "40"];

const numbers = stringNumbers.map((s) => parseInt(s, 10));
console.log(numbers); // [10, 20, 30, 40]
```
</details>

### Exercise 2 (Easy): Once Function Wrapper
**Task**: Build a `once(callback)` utility ensuring that no matter how many times the returned function is called, the original `callback` only executes once, returning the original result on subsequent calls.

<details>
<summary><strong>Solution</strong></summary>

```javascript
function once(fn) {
  let executed = false;
  let result;

  return function (...args) {
    if (!executed) {
      executed = true;
      result = Reflect.apply(fn, this, args);
    }
    return result;
  };
}

const initialize = once((msg) => `Initialized with: ${msg}`);
console.log(initialize("Alpha")); // "Initialized with: Alpha"
console.log(initialize("Beta"));  // "Initialized with: Alpha" (Cached!)
```
</details>

### Exercise 3 (Medium): Custom Promisify Function
**Task**: Write a function `myPromisify(errorFirstFn)` that converts any error-first callback-based function into a Promise-returning function without importing `node:util`.

<details>
<summary><strong>Solution</strong></summary>

```javascript
function myPromisify(fn) {
  return function (...args) {
    return new Promise((resolve, reject) => {
      fn.call(this, ...args, (err, data) => {
        if (err) {
          reject(err);
        } else {
          resolve(data);
        }
      });
    });
  };
}

// Verification
const mockAsyncRead = (filename, cb) => {
  filename === "valid.txt" ? cb(null, "File Content") : cb(new Error("File Not Found"));
};

const promisifiedRead = myPromisify(mockAsyncRead);
promisifiedRead("valid.txt").then((data) => console.log("Read:", data));
```
</details>

### Exercise 4 (Medium): Enforce Consistent Asynchrony (De-Zalgo)
**Task**: Wrap a caching function `fetchCached(id, callback)` using `queueMicrotask` so that whether data is served from memory or simulated network, the callback is **always guaranteed** to fire asynchronously.

<details>
<summary><strong>Solution</strong></summary>

```javascript
function createSafeFetcher() {
  const memory = new Map([["usr_100", { name: "Ada Lovelace" }]]);

  return function fetchUser(id, callback) {
    if (memory.has(id)) {
      // Defer to microtask queue: avoids releasing Zalgo!
      queueMicrotask(() => {
        callback(null, memory.get(id));
      });
      return;
    }

    setTimeout(() => {
      const user = { id, name: "Remote User" };
      memory.set(id, user);
      callback(null, user);
    }, 50);
  };
}

const fetchUser = createSafeFetcher();
let state = "before";
fetchUser("usr_100", (err, user) => {
  console.log(`Callback fired. State was: ${state}`); // State was: "after"!
});
state = "after";
```
</details>

### Exercise 5 (Hard): Asynchronous Waterfall Flow Runner
**Task**: Implement a function `waterfall(tasks, finalCallback)` that executes an array of asynchronous error-first functions sequentially, passing the result of each task as the input to the next task.

<details>
<summary><strong>Solution</strong></summary>

```javascript
function waterfall(tasks, finalCallback) {
  if (tasks.length === 0) {
    return finalCallback(null);
  }

  function runTask(taskIndex, inputArgs) {
    if (taskIndex >= tasks.length) {
      return finalCallback(null, ...inputArgs);
    }

    const currentTask = tasks[taskIndex];
    currentTask(...inputArgs, (err, ...outputArgs) => {
      if (err) {
        return finalCallback(err);
      }
      runTask(taskIndex + 1, outputArgs);
    });
  }

  runTask(0, []);
}

// Verification
const stepA = (cb) => cb(null, 5);
const stepB = (num, cb) => cb(null, num * 2);
const stepC = (num, cb) => cb(null, num + 10);

waterfall([stepA, stepB, stepC], (err, finalResult) => {
  console.log("Waterfall Result (5 * 2 + 10):", finalResult); // 20
});
```
</details>

### Exercise 6 (Hard): Event-Driven Callback Registry with Timeout Protection
**Task**: Build a callback registry `AsyncCallbackPool` where asynchronous callbacks can be registered with a correlation ID and a timeout. If the callback is not completed before the timeout, it is automatically invoked with a `TimeoutError`.

<details>
<summary><strong>Solution</strong></summary>

```javascript
class AsyncCallbackPool {
  constructor() {
    this.pool = new Map();
  }

  register(correlationId, callback, timeoutMs = 1000) {
    const timer = setTimeout(() => {
      this.pool.delete(correlationId);
      callback(new Error(`Operation timed out after ${timeoutMs}ms`), null);
    }, timeoutMs);

    this.pool.set(correlationId, { callback, timer });
  }

  resolve(correlationId, err, data) {
    const entry = this.pool.get(correlationId);
    if (!entry) {
      console.warn(`No pending callback for ID: ${correlationId}`);
      return false;
    }

    clearTimeout(entry.timer);
    this.pool.delete(correlationId);
    entry.callback(err, data);
    return true;
  }
}

// Verification
const pool = new AsyncCallbackPool();

pool.register("req_001", (err, data) => {
  if (err) console.error("Req 001 Failed:", err.message);
  else console.log("Req 001 Succeeded:", data);
}, 100);

// Resolve before timeout
pool.resolve("req_001", null, "Payload Received");

// Register another that times out
pool.register("req_002", (err, data) => {
  if (err) console.error("Req 002 Handled:", err.message);
}, 50);
// req_002 will log: Req 002 Handled: Operation timed out after 50ms
```
</details>
