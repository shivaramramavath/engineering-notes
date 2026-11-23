# The Node.js Event Loop & libuv Architecture

> The Node.js event loop is implemented by the C library libuv, coordinating single-threaded JavaScript execution across six distinct operational phases, microtask checkpoints, and non-blocking OS polling.

---

## Metadata

| Attribute | Details |
| :--- | :--- |
| **Level** | Advanced |
| **Prerequisites** | [Event Loop Foundations](../12_event-loop/01_event-loop.md), [The Node.js Runtime](./01_node-runtime.md) |
| **Time to Read** | 25 minutes |
| **Status** | Complete |
| **Standard** | Node.js v22 LTS / libuv Event Loop Architecture Specification |

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

While web browser event loops are governed by the WHATWG HTML Living Standard (interleaving tasks, microtasks, and V-Sync visual rendering), the **Node.js Event Loop** is implemented directly in C by **`libuv`**. Node.js has no DOM, no CSSOM, and no visual screen rendering phase. Instead, its event loop is engineered specifically for scalable network I/O, file descriptor multiplexing, process signals, and high-frequency backend timers.

Many backend performance bottlenecks—such as unexplained HTTP request timeouts, unhandled promise rejection crashes, and socket buffer congestions—stem from misunderstandings of how libuv transitions between its six internal loop phases and how `process.nextTick` interacts with the V8 microtask queue.

---

## 2. Core Concepts

### The Six Phases of the libuv Event Loop
Every iteration of the Node.js event loop traverses six explicit operational phases in strict order:

```text
   ┌─────────────────────────────────────────────────────────────┐
┌─>│                      1. TIMERS                              │
│  │   Executes callbacks scheduled by setTimeout & setInterval   │
│  └──────────────────────────────┬──────────────────────────────┘
│                                 v
│            [ Microtask Drain: nextTick -> Promise Jobs ]
│                                 │
│  ┌──────────────────────────────┴──────────────────────────────┐
│  │                  2. PENDING CALLBACKS                       │
│  │   Executes deferred I/O callbacks (e.g. TCP connection errors)
│  └──────────────────────────────┬──────────────────────────────┘
│                                 v
│            [ Microtask Drain: nextTick -> Promise Jobs ]
│                                 │
│  ┌──────────────────────────────┴──────────────────────────────┐
│  │                     3. IDLE, PREPARE                        │
│  │   Used only internally by libuv for loop state management   │
│  └──────────────────────────────┬──────────────────────────────┘
│                                 v
│  ┌──────────────────────────────┴──────────────────────────────┐
│  │                         4. POLL                             │
│  │   Retrieves new I/O events; blocks/sleeps if no tasks ready │
│  └──────────────────────────────┬──────────────────────────────┘
│                                 v
│            [ Microtask Drain: nextTick -> Promise Jobs ]
│                                 │
│  ┌──────────────────────────────┴──────────────────────────────┐
│  │                         5. CHECK                            │
│  │   Executes callbacks scheduled by setImmediate()            │
│  └──────────────────────────────┬──────────────────────────────┘
│                                 v
│            [ Microtask Drain: nextTick -> Promise Jobs ]
│                                 │
│  ┌──────────────────────────────┴──────────────────────────────┐
│  │                     6. CLOSE CALLBACKS                      │
│  │   Executes socket/handle close events: socket.on('close')   │
│  └──────────────────────────────┬──────────────────────────────┘
│                                 v
│            [ Microtask Drain: nextTick -> Promise Jobs ]
└─────────────────────────────────┘
```

1. **Timers**: Evaluates min-heaps of active `setTimeout()` and `setInterval()` callbacks whose thresholds have expired.
2. **Pending Callbacks**: Executes deferred I/O callbacks from previous loop iterations (e.g., specific operating system socket errors like `ECONNREFUSED`).
3. **Idle, Prepare**: Used purely internally by libuv to prepare subsystem states before polling.
4. **Poll**: Retrieves new I/O events (epoll/kqueue/IOCP). If no timers or immediate tasks exist, the loop will block and sleep here waiting for incoming network traffic.
5. **Check**: Executes callbacks scheduled specifically via `setImmediate()`.
6. **Close Callbacks**: Executes cleanup events, such as `socket.on('close', ...)`.

---

## 3. How It Works Internally

### Node 11+ Per-Callback Microtask Drainage
In Node.js 10 and earlier, microtasks were drained only at the boundary between phases. Since Node.js 11, the runtime performs a **Microtask Checkpoint after every individual callback**, exactly matching browser behavior:

```
 Inside Poll Phase (Two I/O callbacks are ready: CB1, CB2)
                     │
                     ▼
 [ Execute CB1 ] ──> Schedules process.nextTick() & Promise.then()
                     │
                     ▼
 [ Call Stack Empties ]
                     │
                     ▼
 [ DRAIN NEXT TICK QUEUE ] ──> Drains all scheduled nextTicks
                     │
                     ▼
 [ DRAIN V8 MICROTASK QUEUE ] ──> Drains all Promise microtasks
                     │
                     ▼
 [ Execute CB2 ] ──> Process repeats!
```

---

## 4. Syntax & API Reference

### Phase Scheduling Primitives

```javascript
// 1. Pre-microtask Tick (Runs before V8 microtasks)
process.nextTick(() => {
  console.log('Highest priority async callback');
});

// 2. Standard V8 Microtasks
queueMicrotask(() => console.log('Standard Microtask'));
Promise.resolve().then(() => console.log('Promise Microtask'));

// 3. Timers Phase
const timer = setTimeout(() => console.log('Timers phase'), 0);

// 4. Check Phase (libuv Check)
const immediate = setImmediate(() => console.log('Check phase immediate'));

// Timer control methods (Node.js Timeout object)
timer.ref();   // Keeps process alive if active
timer.unref(); // Allows process to terminate even if pending
immediate.ref();
immediate.unref();
```

---

## 5. Practical Examples

### Example 1: The Canonical Node.js Execution Order Puzzle
```javascript
import fs from 'node:fs';

console.log('1: Sync script start');

setTimeout(() => console.log('2: Timers (setTimeout 0)'), 0);
setImmediate(() => console.log('3: Check (setImmediate)'));

process.nextTick(() => {
  console.log('4: process.nextTick 1');
  process.nextTick(() => console.log('5: nested nextTick'));
});

Promise.resolve().then(() => {
  console.log('6: Promise.then');
});

queueMicrotask(() => console.log('7: queueMicrotask'));

console.log('8: Sync script end');

// Output Sequence:
// 1: Sync script start
// 8: Sync script end
// 4: process.nextTick 1
// 5: nested nextTick
// 6: Promise.then
// 7: queueMicrotask
// (Then 2 or 3 depending on top-level loop entry timer precision)
```

### Example 2: Deterministic Ordering Inside I/O Callbacks
```javascript
import fs from 'node:fs';

// Inside any I/O callback, setImmediate is GUARANTEED to execute before setTimeout(0)!
fs.readFile(new URL(import.meta.url), () => {
  setTimeout(() => {
    console.log('Timers phase: setTimeout');
  }, 0);

  setImmediate(() => {
    console.log('Check phase: setImmediate (Always first inside I/O!)');
  });
});

// Output:
// Check phase: setImmediate (Always first inside I/O!)
// Timers phase: setTimeout
// Explanation: The I/O callback completes in the Poll phase.
// The very next phase in the loop cycle is Check (setImmediate), before Timers!
```

### Example 3: Event Loop Starvation via `process.nextTick`
```javascript
function starveLoop() {
  let count = 0;

  function recursiveTick() {
    count++;
    if (count % 100_000 === 0) {
      console.log(`Processed ${count} nextTicks...`);
    }
    // Starves all I/O and timers!
    process.nextTick(recursiveTick);
  }

  // This timer will NEVER fire because nextTick queue never empties!
  setTimeout(() => {
    console.log('Timer fired!');
  }, 10);

  recursiveTick();
}
// starveLoop(); // Warning: Will freeze the Node.js process!
```

### Example 4: Deferring Execution to Allow Listener Attachment
```javascript
import { EventEmitter } from 'node:events';

class AsyncResource extends EventEmitter {
  constructor() {
    super();

    // Use process.nextTick to emit event after the constructor returns,
    // allowing the caller to attach .on('ready') in the same synchronous tick!
    process.nextTick(() => {
      this.emit('ready', { timestamp: Date.now() });
    });
  }
}

// Caller usage:
const resource = new AsyncResource();

// Listener is attached synchronously before nextTick drains!
resource.on('ready', (data) => {
  console.log('Resource ready event received successfully:', data);
});
```

### Example 5: Yielding to the Poll Phase via `setImmediate`
```javascript
import { setImmediate } from 'node:timers/promises';

// Process a million items without blocking incoming HTTP requests
async function processLargeArrayNonBlocking(items, batchSize = 1000) {
  for (let i = 0; i < items.length; i += batchSize) {
    const chunk = items.slice(i, i + batchSize);

    for (const item of chunk) {
      computeItem(item);
    }

    // Yield to the libuv event loop Check phase, allowing I/O polling to occur
    await setImmediate();
  }
}

function computeItem(item) { /* Heavy math */ }
```

---

## 6. Edge Cases, Pitfalls & Gotchas

1. **Top-Level `setTimeout(fn, 0)` vs `setImmediate(fn)` Non-Determinism**: At the top level of a script, whether `setTimeout(0)` or `setImmediate()` runs first depends on how fast the process initialized and entered the libuv loop. If process startup took $> 1\text{ms}$, the timer is already expired and runs first; if $< 1\text{ms}$, libuv advances to the Check phase and runs `setImmediate` first.
2. **Infinite `process.nextTick` Starvation**: Unlike promises or timers, recursive `process.nextTick` calls do not yield to the event loop. The process stays locked in the nextTick queue, starving incoming network sockets and triggering server health check timeouts.
3. **Timer Expiration Clamping in Node**: In Node.js, `setTimeout(fn, 0)` is internally normalized to `setTimeout(fn, 1)`. Because timers cannot have a delay of 0ms in libuv's min-heap, precision is bounded by the host clock.
4. **Unhandled Rejection Tracking Across Ticks**: An unhandled promise rejection that is caught in a subsequent tick (e.g. `setTimeout(() => p.catch(...), 100)`) will still emit a `process.on('rejectionHandled')` event after initially triggering `unhandledRejection`.

---

## 7. Common Mistakes & Anti-Patterns

| Mistake | Incorrect Pattern | Correct Solution | Why It Fails |
| :--- | :--- | :--- | :--- |
| **Recursive `process.nextTick`** | `function loop() { process.nextTick(loop); }`| Use `setImmediate(loop)` | Completely starves all I/O phases, freezing the process. |
| **Assuming `setTimeout(0)` is Immediate**| Relying on `setTimeout(0)` for zero delay | Use `process.nextTick` or `setImmediate` | `setTimeout(0)` is clamped to 1ms and incurs timer min-heap overhead. |
| **Relying on Top-Level Timer Order**| Expecting `setTimeout(0)` before `setImmediate` | Enclose in I/O callback or explicit sequence | Top-level timing is non-deterministic. |
| **Blocking the Poll Phase** | Long-running sync loops (`for` 10M iterations) | Chunk loop and yield with `await setImmediate()`| Blocks libuv from polling OS network sockets. |
| **Synchronous Event Emission in Constructor**| `constructor() { super(); this.emit('ready'); }` | Emit inside `process.nextTick()` | Caller has not had the opportunity to attach listener yet. |
| **Ignoring Unreferenced Timers in Tests**| Background polling keeping Jest/Mocha alive | Call `timer.unref()` | Node refuses to exit because active handles remain in libuv. |

---

## 8. Performance Characteristics

- **Queue Resolution Costs**:
  - `process.nextTick`: $\approx 10\text{ns}$ (internal Node array queue).
  - `queueMicrotask`: $\approx 35\text{ns}$ (V8 C++ microtask ring buffer).
  - `setImmediate`: $\approx 700\text{ns}$ (libuv check handle iteration).
  - `setTimeout(0)`: $\approx 1,200\text{ns} - 3,500\text{ns}$ (min-heap timer management).
- **libuv Poll Blocking**: If no timers are pending and no immediate callbacks exist, the poll phase blocks the thread via `epoll_wait()` or `kevent()`, using **0% CPU** until a network packet or filesystem event arrives.

---

## 9. Engine Internals

1. **`uv_run` Loop Function**: The core of libuv is the `uv_run(loop, UV_RUN_DEFAULT)` C function in `src/unix/core.c`. It contains a `while (r != 0 && loop->stop_flag == 0)` loop executing the six phase functions sequentially.
2. **`TickInfo` and `_tickCallback`**: Node maintains an internal C++ `TickInfo` array. When returning from C++ native layer to JavaScript, the engine checks `tick_info[kHasScheduled]` and invokes `process._tickCallback()`.

---

## 10. Best Practices & Design Guidelines

1. **Use `setImmediate()` for Chunking CPU-Bound Work**: `setImmediate()` yields to the event loop, allowing I/O polling to occur between iterations.
2. **Use `process.nextTick()` for Immediate Event Emission**: Use `nextTick` to emit events right after object instantiation but before any I/O can interleave.
3. **Prefer Standard `queueMicrotask()` in Isomorphic Libraries**: When code must run across both Node.js and browsers, avoid `process.nextTick` in favor of `queueMicrotask()`.
4. **Call `unref()` on Background Timers**: Ensure internal metrics timers and heartbeats do not prevent process termination when work completes.

---

## 11. Real-World Case Study

### High-Throughput Batch Stream Consumer with Backpressure Yielding

A Kafka stream consumer in Node.js processes 50,000 messages per second. Heavy synchronous parsing caused the event loop to lag by 800ms, triggering Kubernetes liveness probe failures and pod restarts.

```javascript
import { setImmediate } from 'node:timers/promises';

class KafkaBatchProcessor {
  #maxBatchSize = 500;
  #maxExecutionBudgetMs = 12;

  async processIncomingBatch(messages) {
    let lastYieldTime = performance.now();

    for (let i = 0; i < messages.length; i++) {
      this.#parseAndSave(messages[i]);

      // Check if current processing slice exceeded our 12ms budget
      const elapsed = performance.now() - lastYieldTime;
      if (elapsed > this.#maxExecutionBudgetMs) {
        // Yield execution to libuv Check phase!
        // This allows the event loop to poll incoming TCP packets,
        // execute Kubernetes HTTP health checks, and prevent pod eviction!
        await setImmediate();
        lastYieldTime = performance.now();
      }
    }
  }

  #parseAndSave(message) {
    // Deserialize and transform
    const parsed = JSON.parse(message.value);
    // Write to in-memory staging buffer...
  }
}
```

---

## 12. Interview Questions

<details>
<summary><strong>1. What are the six phases of the Node.js libuv event loop in order?</strong></summary>

1. **Timers**: Executes callbacks scheduled by `setTimeout()` and `setInterval()`.
2. **Pending Callbacks**: Executes deferred I/O callbacks from previous loop ticks.
3. **Idle, Prepare**: Used internally by libuv.
4. **Poll**: Retrieves new I/O events (epoll/kqueue); blocks if no other work is pending.
5. **Check**: Executes callbacks scheduled by `setImmediate()`.
6. **Close Callbacks**: Executes handle close callbacks (e.g. `socket.on('close')`).
</details>

<details>
<summary><strong>2. In Node.js, what is the execution priority between <code>process.nextTick()</code> and Promise microtasks?</strong></summary>

`process.nextTick()` has absolute priority over Promise microtasks. When the call stack empties, Node.js completely drains the `process.nextTick` queue *before* executing any Promise microtasks or `queueMicrotask()` callbacks.
</details>

<details>
<summary><strong>3. Why is the order of <code>setTimeout(fn, 0)</code> and <code>setImmediate(fn)</code> non-deterministic at the top level?</strong></summary>

At the top level of a script, execution order depends on how fast process startup and script compilation took. If the process took $> 1\text{ms}$ to enter the libuv loop, the timer has already expired and executes in the Timers phase. If it took $< 1\text{ms}$, the timer is not yet ready, and libuv proceeds to the Poll and Check phases, executing `setImmediate` first.
</details>

<details>
<summary><strong>4. Why is <code>setImmediate</code> guaranteed to run before <code>setTimeout(fn, 0)</code> inside an I/O callback?</strong></summary>

I/O callbacks execute during the libuv "Poll" phase. After the Poll phase completes, the event loop strictly transitions to the "Check" phase (`setImmediate`) before circling back around to the "Timers" phase.
</details>

<details>
<summary><strong>5. What happens if <code>process.nextTick()</code> is called recursively?</strong></summary>

The event loop becomes completely starved. Because the nextTick queue is drained to completion before the event loop can proceed to any phase, the loop will never reach the Timers or Poll phases, permanently blocking I/O and freezing the process.
</details>

<details>
<summary><strong>6. What does <code>timer.unref()</code> do in Node.js?</strong></summary>

It tells libuv that this timer should not keep the event loop active. If all other work finishes and only unreferenced timers remain, the Node.js process terminates cleanly without waiting for the timer to fire.
</details>

<details>
<summary><strong>7. How did Node.js 11 change microtask execution relative to loop phases?</strong></summary>

In Node.js 10 and earlier, microtasks were only drained at the boundary between phases. Since Node.js 11, microtasks drain immediately after *every individual callback* within a phase, bringing Node into complete alignment with browser standards.
</details>

<details>
<summary><strong>8. When should you use <code>process.nextTick()</code> instead of <code>setImmediate()</code>?</strong></summary>

Use `process.nextTick()` when you need to execute code immediately after the current synchronous function returns, but before any other I/O, timers, or microtasks execute (e.g. allowing event listeners to attach in constructors). Use `setImmediate()` to yield to the event loop and allow I/O to be polled.
</details>

---

## 13. Exercises & Challenges

### Challenge 1: Predict the Full Execution Order
Trace the output of this Node.js script:

```javascript
import fs from 'node:fs';

setImmediate(() => console.log('Immediate 1'));

Promise.resolve().then(() => console.log('Promise 1'));

process.nextTick(() => {
  console.log('Tick 1');
  process.nextTick(() => console.log('Tick 2'));
});

setTimeout(() => console.log('Timeout 1'), 0);

queueMicrotask(() => console.log('QueueMicrotask 1'));

console.log('Sync 1');
```

<details>
<summary>View Solution & Explanation</summary>

**Output Sequence:**
```
Sync 1
Tick 1
Tick 2
Promise 1
QueueMicrotask 1
(Timeout 1 or Immediate 1 depending on startup jitter)
```

**Step Breakdown:**
1. Synchronous statement: logs `Sync 1`.
2. Stack clears. NextTick queue drains: logs `Tick 1`, enqueues `Tick 2`, logs `Tick 2`.
3. V8 Microtask queue drains: logs `Promise 1`, logs `QueueMicrotask 1`.
4. Event loop starts: Timers and Check phases execute based on clock jitter.
</details>

### Challenge 2: Implement an Async Safe Emitter
Create an EventEmitter subclass `SafeAsyncEmitter` where all `emit(event, ...args)` calls are deferred via `process.nextTick()`, guaranteeing that listeners attached immediately after construction will never miss events emitted in the constructor.

<details>
<summary>View Solution</summary>

```javascript
import { EventEmitter } from 'node:events';

export class SafeAsyncEmitter extends EventEmitter {
  emitAsync(event, ...args) {
    process.nextTick(() => {
      super.emit(event, ...args);
    });
  }
}

// Verification:
const emitter = new SafeAsyncEmitter();
emitter.emitAsync('start', { id: 1 }); // Emitted before listener is attached!

// Listener attached synchronously:
emitter.on('start', (data) => {
  console.log('Received start event safely:', data.id); // Successfully fires!
});
```
</details>
