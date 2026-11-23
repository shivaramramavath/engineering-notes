# Microtasks & Macrotasks

> Understanding the distinct lifecycle, execution priority, and scheduling guarantees of microtasks and macrotasks is essential for deterministic asynchronous programming and high-performance UI rendering.

---

## Metadata

| Attribute | Details |
| :--- | :--- |
| **Level** | Intermediate to Advanced |
| **Prerequisites** | [Event Loop](./01_event-loop.md), [Promises](../11_asynchronous-javascript/03_promises.md) |
| **Time to Read** | 22 minutes |
| **Status** | Complete |
| **Standard** | HTML5 Specification / ECMAScript Job Queues / Baseline Widely Available |

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

In JavaScript asynchronous execution, not all scheduled tasks are treated equally. The runtime maintains two primary categories of task queues with fundamentally different scheduling semantics: **Macrotasks** (often referred to simply as *Tasks* in the HTML specification) and **Microtasks** (referred to as *Jobs* in the ECMAScript specification).

Macrotasks represent independent, discrete units of work dispatched by the host environment—such as processing user input, firing timer intervals, or receiving network packets. Microtasks represent immediate follow-up actions that must run before control is returned to the event loop or before the browser paints the next visual frame. Failing to understand the boundary between these two queues leads to race conditions, state desynchronization, and UI freezing caused by accidental microtask starvation.

---

## 2. Core Concepts

### Macrotasks (Tasks)
A macrotask represents a standalone operation. When a macrotask runs, JavaScript executes synchronous code until the call stack is empty. After the stack clears, the runtime immediately inspects and drains the microtask queue before picking up the next macrotask.

Common sources of Macrotasks:
- Parsing HTML / Scripts
- Timers: `setTimeout`, `setInterval`
- Node.js `setImmediate`
- I/O operations (file system, network sockets)
- UI interaction events (`click`, `keypress`, `scroll`)
- MessageChannel callbacks (`postMessage`)

### Microtasks (Jobs)
Microtasks are short, high-priority tasks executed at the end of the current task and after every callback invocation that empties the JavaScript call stack. The engine guarantees that the microtask queue is **drained to completion** (including any microtasks enqueued while draining) before returning to the next macrotask or rendering phase.

Common sources of Microtasks:
- Promise reactions: `.then()`, `.catch()`, `.finally()`
- Explicitly scheduled microtasks: `queueMicrotask(fn)`
- `MutationObserver` mutation record notifications
- Awaited promise resumptions (`await`)
- Node.js `process.nextTick` (processed in a microtask-like sub-phase with even higher priority)

```text
+-------------------------------------------------------------+
|                      Task Processing Model                  |
|                                                             |
|   1. Pick oldest Macrotask from queue                       |
|   2. Execute Macrotask until call stack is EMPTY            |
|   3. Drain ALL Microtasks (Microtask Checkpoint)            |
|        ├── Microtask 1                                      |
|        ├── Microtask 2 (enqueues Microtask 3)               |
|        └── Microtask 3                                      |
|   4. Update rendering (if browser display refresh due)      |
|   5. Repeat loop                                            |
+-------------------------------------------------------------+
```

---

## 3. How It Works Internally

### The Microtask Checkpoint Lifecycle
According to the HTML living standard, a **Microtask Checkpoint** is performed:
1. When the JavaScript execution context stack empties.
2. At the end of each macrotask execution step.
3. Before running user agent visual rendering steps.

```
+--------------------------------------------------------------------------+
|                          Macrotask Starts                                |
|  e.g. click event callback or setTimeout callback executes               |
+--------------------------------------------------------------------------+
                                     |
                                     v
                       [ Call Stack Empties ]
                                     |
                                     v
+--------------------------------------------------------------------------+
|                       MICROTASK CHECKPOINT INITIATED                     |
+--------------------------------------------------------------------------+
                                     |
+---> [ Is Microtask Queue Empty? ] -+
|                    |               |
|              NO    v         YES   v
|       +-------------------+   +------------------------------------------+
|       | Dequeue Microtask |   | Microtask Checkpoint Finished            |
|       | Execute Callback  |   +------------------------------------------+
|       +-------------------+                        |
|                    |                               v
+--------------------+            [ Visual Render Phase / Next Macrotask ]
```

---

## 4. Syntax & API Reference

### Scheduling APIs

```javascript
// 1. Explicit standard microtask (Browser and Node.js)
queueMicrotask(() => {
  // Runs after current synchronous code, before timers and paints
});

// 2. Promise-based microtask
Promise.resolve().then(() => {
  // Equivalent scheduling timing to queueMicrotask
});

// 3. Macrotask via setTimeout
setTimeout(() => {
  // Minimum 0ms delay, executes in future macrotask cycle
}, 0);

// 4. Macrotask via MessageChannel (Zero-delay macrotask in browsers)
const channel = new MessageChannel();
channel.port1.onmessage = () => {
  // Macrotask without the 4ms nested timer clamping penalty
};
channel.port2.postMessage(null);
```

---

## 5. Practical Examples

### Example 1: Interleaving Order Demonstration
```javascript
console.log('1: Script Start');

setTimeout(() => {
  console.log('2: Timeout 1');
  Promise.resolve().then(() => console.log('3: Promise inside Timeout 1'));
}, 0);

setTimeout(() => {
  console.log('4: Timeout 2');
}, 0);

Promise.resolve().then(() => {
  console.log('5: Promise 1');
  queueMicrotask(() => console.log('6: Nested Microtask'));
});

Promise.resolve().then(() => {
  console.log('7: Promise 2');
});

console.log('8: Script End');

// Output:
// 1: Script Start
// 8: Script End
// 5: Promise 1
// 7: Promise 2
// 6: Nested Microtask
// 2: Timeout 1
// 3: Promise inside Timeout 1
// 4: Timeout 2
```

### Example 2: Normalizing Asynchronous Flow with `queueMicrotask`
```javascript
// Avoid Zalgo by ensuring callback always runs asynchronously as a microtask
class SafeDataCache {
  #cache = new Map();

  getData(key, callback) {
    if (this.#cache.has(key)) {
      const cachedValue = this.#cache.get(key);
      // Guarantee asynchronous execution even for cache hits
      queueMicrotask(() => callback(null, cachedValue));
      return;
    }

    fetch(`/api/data/${key}`)
      .then(res => res.json())
      .then(data => {
        this.#cache.set(key, data);
        callback(null, data);
      })
      .catch(err => callback(err, null));
  }
}
```

### Example 3: Batching DOM Operations with Microtasks
```javascript
// Coalesce multiple synchronous state mutations into a single microtask flush
class ReactiveBatcher {
  #pendingUpdates = new Set();
  #isFlushing = false;

  update(property, value) {
    this.#pendingUpdates.add({ property, value });

    if (!this.#isFlushing) {
      this.#isFlushing = true;
      queueMicrotask(() => this.#flush());
    }
  }

  #flush() {
    console.log(`Flushing batch of ${this.#pendingUpdates.size} updates`);
    for (const update of this.#pendingUpdates) {
      document.body.dataset[update.property] = update.value;
    }
    this.#pendingUpdates.clear();
    this.#isFlushing = false;
  }
}
```

### Example 4: UI Event Dispatch vs DispatchEvent
```javascript
const button = document.createElement('button');
button.addEventListener('click', () => {
  Promise.resolve().then(() => console.log('Microtask 1'));
  console.log('Listener 1');
});
button.addEventListener('click', () => {
  Promise.resolve().then(() => console.log('Microtask 2'));
  console.log('Listener 2');
});

// A. Real user click:
// Listener 1 runs -> Call stack empties -> Microtask 1 runs ->
// Listener 2 runs -> Call stack empties -> Microtask 2 runs

// B. Synthetic click via button.click():
// button.click() is synchronous! Both listeners run on the SAME call stack!
// Output: Listener 1 -> Listener 2 -> Microtask 1 -> Microtask 2
```

### Example 5: High-Frequency Telemetry Flush
```javascript
class TelemetryTracker {
  #buffer = [];
  #scheduled = false;

  track(event) {
    this.#buffer.push(event);

    if (!this.#scheduled) {
      this.#scheduled = true;
      // Use queueMicrotask so all sync events in current tick are batched
      queueMicrotask(() => this.#sendBatch());
    }
  }

  #sendBatch() {
    const payload = JSON.stringify(this.#buffer);
    this.#buffer = [];
    this.#scheduled = false;
    navigator.sendBeacon('/telemetry', payload);
  }
}
```

---

## 6. Edge Cases, Pitfalls & Gotchas

1. **Microtask Starvation**: An infinite microtask loop (`function loop() { queueMicrotask(loop); }`) starves macrotasks completely. Unlike infinite sync recursion, it does not throw `RangeError: Maximum call stack size exceeded` because the call stack unwinds between microtasks, resulting in a silent, permanent freeze of timers and UI rendering.
2. **Synthetic vs Native DOM Events**: When a user clicks a button, each event handler runs as a separate macrotask task, with a microtask checkpoint between them. However, when triggered programmatically via `element.click()`, both listeners execute synchronously on the same call stack frame, delaying microtask execution until after *both* listeners return.
3. **`queueMicrotask` Exceptions**: If an error is thrown inside `queueMicrotask`, it does not stop the microtask queue from draining! The engine dispatches an `error` event to `window.onerror` or `process.on('uncaughtException')` and immediately resumes executing the next microtask in the queue.

---

## 7. Common Mistakes & Anti-Patterns

| Mistake | Incorrect Pattern | Correct Solution | Why It Fails |
| :--- | :--- | :--- | :--- |
| **Using `setTimeout(fn, 0)` for Batching** | `setTimeout(flush, 0)` | `queueMicrotask(flush)` | `setTimeout` introduces 1ms–4ms delays and yields to macrotasks and paint. |
| **Recursive Microtask Loops** | Infinite `Promise.resolve().then(recurse)` | Yield with `setTimeout` or `scheduler.yield()` | Starves the event loop, freezing UI renders and network handling. |
| **Expecting Paint Before Microtasks** | Attempting to force paint via microtask | Use `requestAnimationFrame` | The browser rendering pipeline never runs until all microtasks are drained. |
| **Sync/Async Inconsistency** | Immediate callback invocation in some paths | Wrap sync paths in `queueMicrotask` | Creates timing bugs when callers expect deferred execution. |
| **Assuming Order Across Macrotask APIs** | Assuming `setTimeout(0)` fires before `MessageChannel` | Do not rely on relative timing across macrotask sources | Different browsers prioritize macrotask queues differently. |
| **Unhandled Errors in `queueMicrotask`** | Expecting try/catch around `queueMicrotask` call to catch inner error | Handle errors inside the scheduled callback | Scheduled callbacks run on an independent stack frame. |

---

## 8. Performance Characteristics

- **Microtask Execution Speed**: Microtasks are executed directly in V8's native loop without OS system call overhead. Enqueueing and running a microtask takes approximately 20–50 nanoseconds.
- **Macrotask Scheduling Overhead**: Macrotasks require updating host timer heaps, waking up platform pollers, and re-evaluating event queues, taking 1,000–4,000 nanoseconds.
- **Garbage Collection Pressure**: Creating short-lived closures inside high-frequency microtasks can trigger rapid Young Generation garbage collection cycles.

---

## 9. Engine Internals

1. **V8 `MicrotaskQueue` Implementation**: Each V8 `Context` references an internal C++ `MicrotaskQueue` containing a ring buffer of microtask pointers. When `Isolate::RunMicrotasks()` is invoked, V8 processes the ring buffer sequentially until empty.
2. **Suppressing Checkpoints during Script Evaluation**: While a top-level `<script>` is actively evaluating, microtasks do not run until the entire script compilation and execution completes.
3. **Node.js `process.nextTick` Priority**: In Node.js, `process.nextTickQueue` is checked immediately before the standard V8 microtask queue (`Promise` jobs), giving `nextTick` absolute precedence over `queueMicrotask`.

---

## 10. Best Practices & Design Guidelines

1. **Prefer `queueMicrotask()` over `Promise.resolve().then()`**: When explicitly scheduling a microtask without an existing promise, `queueMicrotask()` avoids allocating unused Promise instances.
2. **Use Microtasks for State Batching**: Collect multiple synchronous data mutations and flush them once at the microtask checkpoint.
3. **Use Macrotasks to Break Up CPU Work**: When breaking up long-running tasks to prevent UI jank, schedule chunks via macrotasks (`scheduler.yield()` or `setTimeout`), never microtasks.

---

## 11. Real-World Case Study

### High-Frequency State Batching in a Custom Reactive Framework

A reactive UI library updates components when signals change. If 50 signals mutate synchronously inside an event handler, naive updates would trigger 50 redundant DOM rerenders.

```javascript
class ReactiveEngine {
  #dirtyComponents = new Set();
  #isFlushScheduled = false;

  markDirty(component) {
    this.#dirtyComponents.add(component);

    if (!this.#isFlushScheduled) {
      this.#isFlushScheduled = true;
      // Schedule single microtask flush after current synchronous actions finish
      queueMicrotask(() => this.#flushUpdates());
    }
  }

  #flushUpdates() {
    try {
      console.log(`Re-rendering ${this.#dirtyComponents.size} dirty components`);
      for (const component of this.#dirtyComponents) {
        component.render();
      }
    } finally {
      this.#dirtyComponents.clear();
      this.#isFlushScheduled = false;
    }
  }
}

// Client usage
const engine = new ReactiveEngine();
const view = { render: () => console.log('Component rendered to DOM') };

// Synchronously mutating multiple states
engine.markDirty(view);
engine.markDirty(view);
engine.markDirty(view);

console.log('Synchronous mutations complete');
// Output:
// Synchronous mutations complete
// Re-rendering 1 dirty components
// Component rendered to DOM
```

---

## 12. Interview Questions

<details>
<summary><strong>1. What is the fundamental difference in execution timing between microtasks and macrotasks?</strong></summary>

A single macrotask runs, and immediately upon its completion, the runtime completely empties the entire microtask queue (including any microtasks added while draining) before executing the next macrotask or performing visual rendering.
</details>

<details>
<summary><strong>2. Why doesn't an infinite microtask loop throw a "Maximum call stack size exceeded" error?</strong></summary>

Because the call stack actually empties after each microtask completes. The engine pops the current execution context and pushes the next microtask from the queue. The call stack never grows deeper, but the event loop remains trapped in the microtask queue indefinitely.
</details>

<details>
<summary><strong>3. How does <code>queueMicrotask(fn)</code> differ from <code>setTimeout(fn, 0)</code>?</strong></summary>

`queueMicrotask` schedules work in the microtask queue, running immediately after the current call stack clears, before browser rendering or macrotasks. `setTimeout(fn, 0)` schedules a macrotask that runs in a subsequent event loop iteration after all microtasks and paints.
</details>

<details>
<summary><strong>4. Why do synthetic DOM events (via <code>element.click()</code>) produce different microtask timing than user clicks?</strong></summary>

With a user click, each registered listener is dispatched as an independent task from the browser's event queue with a microtask checkpoint between listeners. With `element.click()`, both listeners execute synchronously within the caller's call stack, deferring microtask processing until the caller's stack finishes.
</details>

<details>
<summary><strong>5. In Node.js, how does <code>process.nextTick()</code> compare to <code>queueMicrotask()</code>?</strong></summary>

Both schedule deferred asynchronous execution before macrotasks. However, Node.js prioritizes `process.nextTick` ahead of the V8 microtask queue: all pending `nextTick` callbacks drain before any `queueMicrotask` or Promise callbacks run.
</details>

<details>
<summary><strong>6. What happens if an uncaught error is thrown inside a microtask?</strong></summary>

The uncaught error is reported to the host environment (triggering `window.onerror` or `process.on('uncaughtException')`), but the engine continues draining any remaining microtasks in the queue.
</details>

<details>
<summary><strong>7. Can a browser paint the screen between two microtasks?</strong></summary>

No. The HTML specification mandates that the microtask queue must be completely drained before the user agent can proceed to the "Update the rendering" phase.
</details>

<details>
<summary><strong>8. When should you use <code>MessageChannel</code> instead of <code>setTimeout(fn, 0)</code>?</strong></summary>

In browsers, nested `setTimeout` calls are clamped to a minimum 4ms delay. `MessageChannel` schedules a macrotask without this clamping delay, yielding control to the event loop with minimum overhead.
</details>

---

## 13. Exercises & Challenges

### Challenge 1: Trace the Execution Order
Analyze the following code and write down the exact sequence of numbers printed to the console:

```javascript
console.log(1);

setTimeout(() => {
  console.log(2);
  queueMicrotask(() => console.log(3));
}, 0);

queueMicrotask(() => {
  console.log(4);
  setTimeout(() => console.log(5), 0);
});

Promise.resolve().then(() => console.log(6));

console.log(7);
```

<details>
<summary>View Solution & Explanation</summary>

**Execution Order:**
```
1
7
4
6
2
3
5
```

**Step Breakdown:**
1. Sync execution: logs `1` and `7`.
2. First macrotask (`setTimeout 2`) is enqueued.
3. Microtask queue has: `[4, 6]`.
4. Stack clears. Microtasks drain:
   - Logs `4`, enqueues macrotask `5`.
   - Logs `6`.
5. Event loop picks next macrotask: `setTimeout 2`. Logs `2`, enqueues microtask `3`.
6. Macrotask completes. Microtask queue drains: logs `3`.
7. Event loop picks next macrotask: `setTimeout 5`. Logs `5`.
</details>

### Challenge 2: Implement a Safe Microtask Coalescer
Write a function `coalesceMicrotask(fn)` that returns a wrapped function. When the wrapped function is called multiple times synchronously with different arguments, it collects all arguments into an array and invokes `fn(allArgs)` exactly once at the end of the tick.

<details>
<summary>View Solution</summary>

```javascript
function coalesceMicrotask(fn) {
  let pendingArgs = [];
  let isScheduled = false;

  return function (...args) {
    pendingArgs.push(...args);

    if (!isScheduled) {
      isScheduled = true;
      queueMicrotask(() => {
        const batch = pendingArgs;
        pendingArgs = [];
        isScheduled = false;
        fn(batch);
      });
    }
  };
}

// Verification:
const batchLog = coalesceMicrotask(items => console.log('Batched:', items));
batchLog(1, 2);
batchLog(3);
batchLog(4, 5);
// Synchronous logs finish, then: "Batched: [1, 2, 3, 4, 5]"
```
</details>
