# The JavaScript Event Loop

> The JavaScript event loop is the foundational concurrency coordinator that powers single-threaded, non-blocking I/O across browsers and server-side runtimes by multiplexing synchronous execution with prioritized task queues.

---

## Metadata

| Attribute | Details |
| :--- | :--- |
| **Level** | Core / Advanced |
| **Prerequisites** | [Sync vs Async](../11_asynchronous-javascript/01_sync-vs-async.md), [Promises](../11_asynchronous-javascript/03_promises.md) |
| **Time to Read** | 25 minutes |
| **Status** | Complete |
| **Standard** | HTML5 Living Standard / Node.js libuv / Baseline Widely Available |

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

JavaScript is architected around a single-threaded execution model: it possesses exactly one call stack and executes one statement at a time. In traditional multi-threaded runtimes (like Java or C++), blocking operations—such as disk reads or network socket queries—pause the invoking thread while waiting for hardware I/O. If JavaScript blocked its single thread on network queries or file access, the entire user interface in browsers would freeze, and Node.js servers would stall for all concurrent clients.

The **Event Loop** is the coordinating mechanism that resolves this fundamental tension. By offloading I/O operations to host platform abstractions (browser Web APIs or Node.js `libuv` worker pools), the JavaScript thread remains free to execute synchronous user logic. When asynchronous tasks finish, their callbacks are placed into queues. The event loop continuously inspects the call stack; whenever the stack empties, it schedules queued callbacks back onto the execution stack.

---

## 2. Core Concepts

### Architectural Building Blocks

1. **The Call Stack**: A LIFO (Last-In-First-Out) data structure that tracks active execution contexts. Every function call pushes a stack frame; returning pops it.
2. **The Memory Heap**: An unstructured memory region where objects, closures, and reference types are allocated.
3. **Host APIs (Web APIs / libuv)**: Thread pools and OS-level asynchronous interfaces (e.g., epoll, kqueue, IOCP) that perform background timers, HTTP networking, and filesystem access without touching the JavaScript main thread.
4. **Macrotask Queue (Task Queue)**: Holds callbacks scheduled by timers (`setTimeout`, `setInterval`), I/O events, message channels, and UI events.
5. **Microtask Queue**: Holds high-priority callbacks scheduled by Promise reactions (`.then()`, `.catch()`, `.finally()`), `queueMicrotask()`, and `MutationObserver`.
6. **Render Pipeline (Browsers)**: Calculates style updates, layout reflows, and paints at display refresh intervals (e.g., 60Hz / 120Hz).

```text
+-------------------------------------------------------------------------+
|                           JavaScript Runtime                            |
|                                                                         |
|   +-------------------+        +-----------------------------------+    |
|   |    Call Stack     |        |            Memory Heap            |    |
|   |  [ Frame 2: bar ] |        |  { user: "Alice", active: true }  |    |
|   |  [ Frame 1: foo ] |        |  [ ArrayBuffer: 4096 bytes ]      |    |
|   +-------------------+        +-----------------------------------+    |
+-------------------------------------------------------------------------+
             |                                             ^
       Dispatches I/O                              Pushes callbacks
             v                                             |
+-------------------------------------------------------------------------+
|                  Host Environment (Browser / libuv)                     |
|   Timers, Network Sockets, File Descriptors, IPC, DOM Event Dispatch    |
+-------------------------------------------------------------------------+
             |
             +-----------------------+-----------------------+
             |                                               |
             v                                               v
+---------------------------+                   +-------------------------+
|     Microtask Queue       |                   |     Macrotask Queue     |
| (Promises, queueMicrotask)|                   | (Timers, I/O, UI, IPC)  |
+---------------------------+                   +-------------------------+
             |                                               |
             +-------------------> [ EVENT LOOP ] <----------+
                                          |
                                          v
                               Executes on Call Stack
```

---

## 3. How It Works Internally

The event loop runs an infinite loop governed by strict phase guarantees.

```
+-----------------------------------------------------------------------+
|                       One Event Loop Iteration                        |
+-----------------------------------------------------------------------+
                                    |
                                    v
            +-----------------------------------------------+
            | 1. Execute oldest Task from Macrotask Queue    |
            |    (Run until Call Stack is completely empty) |
            +-----------------------------------------------+
                                    |
                                    v
            +-----------------------------------------------+
            | 2. Drain Microtask Queue completely           |
            |    (Run all microtasks, including newly       |
            |     enqueued microtasks, until queue is empty)|
            +-----------------------------------------------+
                                    |
                                    v
            +-----------------------------------------------+
            | 3. (Browser) Check if rendering is required    |
            |    - Run requestAnimationFrame callbacks      |
            |    - Recalculate Styles & Layout              |
            |    - Paint Composited Layers to Screen        |
            +-----------------------------------------------+
                                    |
                                    v
            +-----------------------------------------------+
            | 4. Wait / Sleep until next task or I/O event  |
            +-----------------------------------------------+
                                    |
                                    +--- Loop Repeats
```

### Run-to-Completion Semantics
Once a JavaScript function begins executing on the call stack, it cannot be interrupted or preempted by another function or event loop task. It runs completely until it returns or throws. This eliminates shared-memory data race conditions common in multi-threaded environments.

---

## 4. Syntax & API Reference

### Scheduling Primitives

```javascript
// 1. Enqueue high-priority microtask
queueMicrotask(() => {
  // Executes immediately after current call stack empties, BEFORE any timer or paint
});

// 2. Schedule macrotask timer
const timerId = setTimeout(() => {
  // Executes in a future macrotask phase after minimum delay
}, 0);

// 3. Node.js only: Next Tick (runs before standard microtasks)
if (typeof process !== 'undefined' && process.nextTick) {
  process.nextTick(() => {
    // Intercepts call stack before Promise microtask queue drains
  });
}

// 4. Node.js only: Check Phase immediate callback
if (typeof setImmediate !== 'undefined') {
  setImmediate(() => {
    // Executes in libuv 'check' phase immediately after I/O callbacks
  });
}

// 5. Browser only: Animation frame before visual repaint
if (typeof requestAnimationFrame !== 'undefined') {
  requestAnimationFrame((timestamp) => {
    // Runs right before browser layout recalculation and paint
  });
}
```

---

## 5. Practical Examples

### Example 1: Execution Order Verification
```javascript
console.log('1: Sync start');

setTimeout(() => {
  console.log('2: Macrotask (setTimeout)');
}, 0);

Promise.resolve().then(() => {
  console.log('3: Microtask (Promise.then)');
});

queueMicrotask(() => {
  console.log('4: Microtask (queueMicrotask)');
});

console.log('5: Sync end');

// Output:
// 1: Sync start
// 5: Sync end
// 3: Microtask (Promise.then)
// 4: Microtask (queueMicrotask)
// 2: Macrotask (setTimeout)
```

### Example 2: Chunking Synchronous Work to Yield Control to Event Loop
```javascript
// Processing 100,000 items without freezing the UI or blocking server I/O
async function processLargeArrayInChunks(items, chunkSize = 1000) {
  for (let i = 0; i < items.length; i += chunkSize) {
    const chunk = items.slice(i, i + chunkSize);

    // Synchronous work
    for (const item of chunk) {
      heavyCalculation(item);
    }

    // Yield control to event loop to allow UI renders and I/O to flush
    await new Promise(resolve => setTimeout(resolve, 0));
  }
}

function heavyCalculation(item) {
  return item * 2;
}
```

### Example 3: Microtask Starvation Reproduction
```javascript
function starveEventLoop() {
  let counter = 0;

  function recursiveMicrotask() {
    if (counter++ < 1_000_000) {
      queueMicrotask(recursiveMicrotask); // Starves macrotasks completely!
    }
  }

  // This timer will NEVER fire until all 1,000,000 microtasks finish
  setTimeout(() => {
    console.log('Macrotask finally executed!');
  }, 0);

  queueMicrotask(recursiveMicrotask);
}
```

### Example 4: Browser Animation Alignment via `requestAnimationFrame`
```javascript
function smoothWidthTransition(element, targetWidth, durationMs) {
  const startWidth = element.offsetWidth;
  const startTime = performance.now();

  function step(currentTime) {
    const elapsed = currentTime - startTime;
    const progress = Math.min(elapsed / durationMs, 1);

    // Linear interpolation
    element.style.width = `${startWidth + (targetWidth - startWidth) * progress}px`;

    if (progress < 1) {
      requestAnimationFrame(step); // Synchronized to monitor refresh rate
    }
  }

  requestAnimationFrame(step);
}
```

### Example 5: Node.js `setImmediate` vs `setTimeout` Ordering
```javascript
import fs from 'node:fs';

// Inside an I/O callback, setImmediate ALWAYS executes before setTimeout(fn, 0)
fs.readFile(new URL(import.meta.url), () => {
  setTimeout(() => {
    console.log('Timer phase callback');
  }, 0);

  setImmediate(() => {
    console.log('Check phase (setImmediate) callback');
  });
});

// Output:
// Check phase (setImmediate) callback
// Timer phase callback
```

---

## 6. Edge Cases, Pitfalls & Gotchas

1. **Nested Microtasks Starve the Host**: If a microtask enqueues another microtask, the event loop drains the new microtask before returning to macrotasks. Unbounded microtask recursion starves timers, network I/O, and UI painting permanently.
2. **`setTimeout(fn, 0)` Is Not Zero**: The HTML standard mandates a minimum 4ms delay clamp for nested timer depths $\ge 5$. Furthermore, timer callbacks must wait for the current call stack and all pending microtasks to drain before execution.
3. **Top-Level `setTimeout` vs `setImmediate` in Node**: In top-level Node.js modules, the execution order between `setTimeout(fn, 0)` and `setImmediate(fn)` is non-deterministic (bound to process startup CPU jitter). However, inside an I/O callback, `setImmediate` is guaranteed to run first.
4. **Synchronous Error Bypasses Microtask Drain**: If an uncaught exception crashes the call stack, unhandled rejection handlers are dispatched before subsequent queued tasks execute.

---

## 7. Common Mistakes & Anti-Patterns

| Mistake | Incorrect Pattern | Correct Solution | Why It Fails |
| :--- | :--- | :--- | :--- |
| **Blocking the Stack with CPU Work** | Executing heavy JSON parsing or crypto in single loop | Chunk work or delegate to Worker Threads | Freezes the event loop, causing dropped frames and request timeouts. |
| **Using `setTimeout` for Smooth UI** | `setInterval(updateUI, 16)` | `requestAnimationFrame(updateUI)` | Timers drift and fire out-of-sync with hardware V-Sync refresh. |
| **Recursive Microtask Loops** | Infinite `Promise.resolve().then(loop)` | Yield via `setTimeout` or `scheduler.yield()` | Completely starves macrotask queues and browser rendering. |
| **Assuming Timers are Exact** | Relying on `setTimeout(fn, 1000)` for exact clock timing | Calculate delta with `performance.now()` | Timers guarantee minimum wait, not exact execution time. |
| **Zalgo Sync/Async Ambiguity** | Executing callback sync in cache hit, async in miss | Always schedule callback via `queueMicrotask` | Breaks caller invariants and expectation of deferred execution. |
| **Misunderstanding Microtask Priority** | Expecting `setTimeout(0)` to run before resolved Promise | Microtasks always drain before the next macrotask | Inverts execution order expectations and introduces race conditions. |

---

## 8. Performance Characteristics

- **Microtask Queue Drain Cost**: Microtasks execute in a tight internal C++ loop without re-entering the OS event demultiplexer, making them extremely fast ($< 50\text{ns}$ per resolution).
- **Macrotask Context Switch Cost**: Moving between macrotasks requires the runtime to query host timers, inspect I/O poll sets, and evaluate rendering opportunities, incurring higher overhead ($> 1\mu\text{s}$).
- **Rendering Throughput**: Browsers target 16.6ms (60 FPS) or 8.3ms (120 FPS) frame budgets. Any synchronous JavaScript task exceeding 50ms is classified as a Long Task, degrading First Input Delay (FID) and Interaction to Next Paint (INP).

---

## 9. Engine Internals

1. **V8 Microtask Checkpoints**: In V8, microtasks are executed at explicit checkpoints (`Isolate::RunMicrotasks()`). A checkpoint occurs whenever the JavaScript call stack becomes completely empty, or explicitly during host callback boundaries.
2. **libuv Event Loop Phases (Node.js)**:
   - `Timers`: Executes callbacks scheduled by `setTimeout()` and `setInterval()`.
   - `Pending Callbacks`: Executes I/O callbacks deferred from the previous loop iteration.
   - `Idle, Prepare`: Internal libuv coordination phases.
   - `Poll`: Retrieves new I/O events (epoll/kqueue); blocks if no other work is pending.
   - `Check`: Executes `setImmediate()` callbacks.
   - `Close Callbacks`: Executes socket close handlers (`socket.on('close')`).

---

## 10. Best Practices & Design Guidelines

1. **Keep Call Stack Frames Short**: Avoid executing synchronous loops exceeding 16ms on the main thread.
2. **Use `queueMicrotask` for Deferred Execution**: When you need to defer execution until synchronous operations finish but before the next render/task, use `queueMicrotask()` instead of `setTimeout(fn, 0)`.
3. **Use Web Workers / Worker Threads for CPU-Intensive Tasks**: Offload image processing, cryptography, and heavy parsing to secondary threads.
4. **Adopt `scheduler.yield()`**: In modern browsers, use the cooperative scheduling API `scheduler.yield()` to yield control back to the event loop without losing task priority.

---

## 11. Real-World Case Study

### Preventing Main Thread Freezes During Huge DOM Reconciliation

A real-time analytics dashboard receives 10,000 metrics updates per second over WebSocket. Updating DOM elements synchronously froze user scrolling and caused high interaction latency.

```javascript
class NonBlockingRenderer {
  #pendingUpdates = [];
  #isScheduled = false;

  enqueue(update) {
    this.#pendingUpdates.push(update);
    if (!this.#isScheduled) {
      this.#isScheduled = true;
      requestAnimationFrame(() => this.#flushBatch());
    }
  }

  #flushBatch() {
    const startTime = performance.now();
    const FRAME_BUDGET_MS = 10; // Keep well under 16ms budget

    while (this.#pendingUpdates.length > 0) {
      const update = this.#pendingUpdates.shift();
      this.#applyDomUpdate(update);

      // Check if we exhausted the frame budget
      if (performance.now() - startTime > FRAME_BUDGET_MS) {
        break;
      }
    }

    if (this.#pendingUpdates.length > 0) {
      // Re-schedule remaining updates for the next animation frame
      requestAnimationFrame(() => this.#flushBatch());
    } else {
      this.#isScheduled = false;
    }
  }

  #applyDomUpdate(update) {
    const el = document.getElementById(`metric-${update.id}`);
    if (el) el.textContent = update.value;
  }
}
```

---

## 12. Interview Questions

<details>
<summary><strong>1. What is the fundamental difference between the Microtask Queue and the Macrotask Queue?</strong></summary>

The macrotask queue processes one task per event loop cycle, after which the entire microtask queue is drained to completion. Any new microtasks enqueued while draining are also processed in the same cycle before the loop proceeds to rendering or the next macrotask.
</details>

<details>
<summary><strong>2. Why is <code>setTimeout(fn, 0)</code> not executed immediately?</strong></summary>

Because `setTimeout(fn, 0)` schedules a macrotask. It can only execute after the current call stack finishes, all pending microtasks drain completely, and the event loop scheduler picks the timer in the next cycle (with a minimum browser clamp of up to 4ms for nested timers).
</details>

<details>
<summary><strong>3. What happens if a microtask recursively enqueues another microtask indefinitely?</strong></summary>

The event loop becomes starved. The microtask queue will never empty, preventing the event loop from progressing to macrotasks, I/O processing, or visual page rendering. The page or server process effectively freezes.
</details>

<details>
<summary><strong>4. In Node.js, what is the difference between <code>process.nextTick()</code> and <code>setImmediate()</code>?</strong></summary>

`process.nextTick()` callbacks are processed immediately after the current operation completes, before the event loop continues to standard microtasks or the next phase. `setImmediate()` schedules callbacks in the libuv "Check" phase, which runs after I/O polling.
</details>

<details>
<summary><strong>5. When does browser rendering occur relative to the event loop phases?</strong></summary>

Rendering occurs after a macrotask completes and the microtask queue has completely drained, provided the browser determines it is time for a screen refresh (aligned with monitor V-Sync).
</details>

<details>
<summary><strong>6. What is "Run-to-Completion" semantics?</strong></summary>

It means that when a JavaScript function executes, it cannot be interrupted by another task or thread. It runs until it completes all statements, returns, or throws an exception.
</details>

<details>
<summary><strong>7. How does <code>requestAnimationFrame</code> differ from <code>setTimeout(fn, 16)</code>?</strong></summary>

`requestAnimationFrame` is directly synchronized with the browser's display refresh hardware (e.g. 60Hz or 120Hz), firing right before layout and paint. `setTimeout` fires independently of refresh intervals, causing skipped frames and jitter.
</details>

<details>
<summary><strong>8. What is a "Long Task" in browser performance metrics?</strong></summary>

Any task that monopolizes the main thread for more than 50 milliseconds. Long tasks block user input processing, leading to poor Interaction to Next Paint (INP) scores.
</details>

---

## 13. Exercises & Challenges

### Challenge 1: Predict the Exact Execution Order
Predict the console output of the following snippet without running it:

```javascript
console.log('A');

setTimeout(() => console.log('B'), 0);

Promise.resolve()
  .then(() => {
    console.log('C');
    queueMicrotask(() => console.log('D'));
  })
  .then(() => console.log('E'));

console.log('F');
```

<details>
<summary>View Solution & Explanation</summary>

**Output:**
```
A
F
C
D
E
B
```

**Step-by-Step Breakdown:**
1. Synchronous statements execute: `'A'` and `'F'` are logged.
2. `setTimeout` schedules callback `'B'` in the macrotask queue.
3. First promise `.then()` is queued in microtask queue.
4. Call stack empties. Microtask queue drains:
   - Logs `'C'`, enqueues microtask `'D'`, and enqueues second promise chained callback `'E'`.
   - Microtask `'D'` executes: logs `'D'`.
   - Microtask `'E'` executes: logs `'E'`.
5. Microtask queue is now empty.
6. Event loop advances to macrotask queue: logs `'B'`.
</details>

### Challenge 2: Implement a Cooperative Task Scheduler
Write a utility function `yieldToMain()` that uses modern `scheduler.yield()` if available, falling back to `setTimeout(resolve, 0)`, allowing long-running CPU algorithms to periodically yield without starving the event loop.

<details>
<summary>View Solution</summary>

```javascript
function yieldToMain() {
  if (typeof globalThis.scheduler?.yield === 'function') {
    return globalThis.scheduler.yield();
  }
  return new Promise(resolve => setTimeout(resolve, 0));
}

// Usage in heavy computation:
async function processData(dataset) {
  let lastYield = performance.now();
  for (let i = 0; i < dataset.length; i++) {
    dataset[i] = heavyTransform(dataset[i]);

    // Yield if we have run for more than 15ms
    if (performance.now() - lastYield > 15) {
      await yieldToMain();
      lastYield = performance.now();
    }
  }
}
```
</details>
