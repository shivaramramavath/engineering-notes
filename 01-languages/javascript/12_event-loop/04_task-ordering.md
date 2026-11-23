# Task Ordering & Execution Semantics

> Mastering the precise scheduling sequence between synchronous evaluation, `process.nextTick`, Promise microtasks, libuv phases, and browser rendering loops eliminates non-deterministic timing bugs.

---

## Metadata

| Attribute | Details |
| :--- | :--- |
| **Level** | Advanced |
| **Prerequisites** | [The Event Loop](./01_event-loop.md), [Microtasks & Macrotasks](./02_microtasks-and-macrotasks.md), [Timers](./03_timers.md) |
| **Time to Read** | 24 minutes |
| **Status** | Complete |
| **Standard** | HTML5 Event Loop Processing Model / Node.js libuv Event Loop Specification |

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

Developers frequently encounter asynchronous bugs that manifest only under specific load conditions or runtime environments. A snippet tested in Chromium might exhibit different callback execution order when migrated to a Node.js microservice. These discrepancies stem from subtle differences between the **WHATWG HTML Event Loop specification** (governing web browsers) and the **libuv event loop architecture** (governing Node.js).

Predicting task ordering is not an academic exercise: race conditions in cache population, unhandled promise rejections during stream teardown, and UI rendering flickers occur when developers make incorrect assumptions about which queue drains first. Mastering task ordering provides total control over execution timing.

---

## 2. Core Concepts

### The Hierarchy of Queues

When multiple asynchronous primitives are scheduled concurrently, JavaScript runtimes evaluate them according to a strict priority hierarchy:

```text
Priority 1: Synchronous Call Stack (Current execution frame)
    │
    ▼
Priority 2: Node.js process.nextTick Queue (Node only; drains immediately)
    │
    ▼
Priority 3: V8 Microtask Queue (queueMicrotask, Promise .then/.catch/finally)
    │
    ▼
Priority 4: Browser Animation Callbacks (requestAnimationFrame, browsers only)
    │
    ▼
Priority 5: Macrotask Queue (setTimeout, setInterval, setImmediate, I/O, events)
```

### Browser vs Node.js Event Loop Architecture

| Feature | Browser (HTML5 Standard) | Node.js (libuv) |
| :--- | :--- | :--- |
| **Macrotasks** | Generic task queues (Timers, I/O, UI, MessageChannel) | Explicit phases: Timers, Pending I/O, Poll, Check, Close |
| **Immediate Hook** | None native (`MessageChannel` / `postMessage` fallback) | `setImmediate()` (runs in libuv Check phase) |
| **Next Tick Hook** | None (`queueMicrotask` is the closest standard) | `process.nextTick()` (runs before standard microtasks) |
| **Rendering Phase** | Dedicated V-Sync aligned rendering pipeline | No visual rendering pipeline |
| **Microtask Drain Point**| After *every* macrotask and after call stack empties | After *every* individual callback (Node 11+) |

---

## 3. How It Works Internally

### Node.js libuv Phase Sequence with Microtask Checkpoints

Since Node.js v11, the runtime drains the `process.nextTick` queue and the Promise microtask queue between *every single* callback, matching browser behavior:

```
   ┌─────────────────────────────────────────────────────────────┐
┌─>│                      1. TIMERS PHASE                        │
│  │   Executes callbacks scheduled by setTimeout & setInterval   │
│  └──────────────────────────────┬──────────────────────────────┘
│                                 v
│            [ Microtask Checkpoint: nextTick -> Microtasks ]
│                                 |
│  ┌──────────────────────────────┴──────────────────────────────┐
│  │                  2. PENDING CALLBACKS PHASE                 │
│  │   Executes deferred I/O callbacks from previous loop tick    │
│  └──────────────────────────────┬──────────────────────────────┘
│                                 v
│            [ Microtask Checkpoint: nextTick -> Microtasks ]
│                                 |
│  ┌──────────────────────────────┴──────────────────────────────┐
│  │                     3. IDLE / PREPARE                       │
│  │   Internal libuv coordination phases                        │
│  └──────────────────────────────┬──────────────────────────────┘
│                                 v
│  ┌──────────────────────────────┴──────────────────────────────┐
│  │                       4. POLL PHASE                         │
│  │   Retrieves new I/O events; blocks if no work is ready      │
│  └──────────────────────────────┬──────────────────────────────┘
│                                 v
│            [ Microtask Checkpoint: nextTick -> Microtasks ]
│                                 |
│  ┌──────────────────────────────┴──────────────────────────────┐
│  │                      5. CHECK PHASE                         │
│  │   Executes callbacks scheduled by setImmediate()            │
│  └──────────────────────────────┬──────────────────────────────┘
│                                 v
│            [ Microtask Checkpoint: nextTick -> Microtasks ]
│                                 |
│  ┌──────────────────────────────┴──────────────────────────────┐
│  │                      6. CLOSE PHASE                         │
│  │   Executes close callbacks (e.g. socket.on('close'))         │
│  └──────────────────────────────┬──────────────────────────────┘
│                                 v
│            [ Microtask Checkpoint: nextTick -> Microtasks ]
└─────────────────────────────────┘
```

---

## 4. Syntax & API Reference

### Comparative Scheduling APIs

```javascript
// Universal: Microtask Queue
queueMicrotask(() => console.log('Universal Microtask'));
Promise.resolve().then(() => console.log('Promise Microtask'));

// Universal: Macrotask Timer
setTimeout(() => console.log('Macrotask Timer'), 0);

// Node.js Only: Pre-microtask Next Tick
if (typeof process !== 'undefined' && process.nextTick) {
  process.nextTick(() => console.log('Node Next Tick'));
}

// Node.js Only: Check Phase Immediate
if (typeof setImmediate !== 'undefined') {
  setImmediate(() => console.log('Node Check Phase Immediate'));
}

// Browser Only: Animation Frame
if (typeof requestAnimationFrame !== 'undefined') {
  requestAnimationFrame(() => console.log('Browser Animation Frame'));
}
```

---

## 5. Practical Examples

### Example 1: The Canonical Node.js Full Queue Puzzle
```javascript
console.log('1: Sync 1');

setTimeout(() => console.log('2: setTimeout 0'), 0);
setImmediate(() => console.log('3: setImmediate'));

process.nextTick(() => {
  console.log('4: nextTick 1');
  process.nextTick(() => console.log('5: nested nextTick'));
});

Promise.resolve().then(() => {
  console.log('6: Promise 1');
  process.nextTick(() => console.log('7: nextTick inside Promise'));
});

queueMicrotask(() => console.log('8: queueMicrotask'));

console.log('9: Sync 2');

// Output:
// 1: Sync 1
// 9: Sync 2
// 4: nextTick 1
// 5: nested nextTick
// 6: Promise 1
// 8: queueMicrotask
// 7: nextTick inside Promise
// (2 or 3 depending on timer jitter at top-level)
```

### Example 2: Deterministic `setImmediate` vs `setTimeout` inside I/O
```javascript
import fs from 'node:fs';

// Non-deterministic at top level:
// setTimeout(() => console.log('timeout'), 0);
// setImmediate(() => console.log('immediate'));

// Deterministic inside any I/O callback:
fs.readFile(new URL(import.meta.url), () => {
  setTimeout(() => console.log('1: Timeout inside I/O'), 0);
  setImmediate(() => console.log('2: Immediate inside I/O'));
});

// Output:
// 2: Immediate inside I/O
// 1: Timeout inside I/O
// Explanation: The I/O callback completes in the Poll phase.
// The next phase in the loop is Check (setImmediate), BEFORE Timers!
```

### Example 3: Browser UI vs Microtasks vs Paint
```javascript
const box = document.getElementById('box');

// User triggers visual mutation
box.style.transform = 'translateX(100px)';

// Microtask runs BEFORE browser paint
Promise.resolve().then(() => {
  // Reading layout forces synchronous reflow
  console.log('Offset in microtask:', box.offsetLeft);
});

// Animation frame runs right BEFORE layout/paint
requestAnimationFrame(() => {
  console.log('rAF executed before screen refresh');
});

// Macrotask runs AFTER paint
setTimeout(() => {
  console.log('setTimeout executed after render');
}, 0);
```

### Example 4: Starving the Event Loop via `process.nextTick`
```javascript
function recursiveTick(depth = 0) {
  if (depth > 1000) {
    console.log('Finished recursive nextTick');
    return;
  }
  process.nextTick(() => recursiveTick(depth + 1));
}

// This timer will NOT fire until all 1000 nextTicks have completed!
setTimeout(() => console.log('Timer fired!'), 0);

recursiveTick();
```

### Example 5: Interleaving Between Two Promises
```javascript
Promise.resolve()
  .then(() => console.log('Promise A1'))
  .then(() => console.log('Promise A2'))
  .then(() => console.log('Promise A3'));

Promise.resolve()
  .then(() => console.log('Promise B1'))
  .then(() => console.log('Promise B2'))
  .then(() => console.log('Promise B3'));

// Output alternates microtasks tick-by-tick:
// Promise A1
// Promise B1
// Promise A2
// Promise B2
// Promise A3
// Promise B3
```

---

## 6. Edge Cases, Pitfalls & Gotchas

1. **Top-Level `setTimeout` vs `setImmediate` Jitter in Node**: At top-level script scope, whether `setTimeout(fn, 0)` or `setImmediate(fn)` runs first depends on system CPU load and how long process bootstrap took before entering the libuv loop.
2. **`process.nextTick` Recursion Limits**: In modern Node.js, `process.nextTick` does not enforce an automatic recursion depth limit, meaning an infinite `process.nextTick` loop will permanently starve I/O and timers.
3. **Promise Microtask Interleaving in Async Await**: Awaited expressions create microtask points. If two async functions await simultaneously, they interleave predictably on every `await` boundary.
4. **`queueMicrotask` in Node vs Browser**: In Node, `process.nextTick` runs ahead of `queueMicrotask`. In the browser, `queueMicrotask` and Promise reactions share the identical single microtask queue.

---

## 7. Common Mistakes & Anti-Patterns

| Mistake | Incorrect Pattern | Correct Solution | Why It Fails |
| :--- | :--- | :--- | :--- |
| **Relying on Top-Level Timer Order** | Assuming `setTimeout(fn, 0)` runs before `setImmediate` | Enclose in I/O callback or use explicit sequencing | Top-level timing is non-deterministic due to timer expiration precision. |
| **Using `nextTick` for Heavy Work** | `process.nextTick(heavySyncTask)` | Use worker threads or `setImmediate` | Starves the libuv I/O poll phase, blocking incoming network traffic. |
| **Assuming `rAF` Runs After Paint** | Running state cleanup inside `requestAnimationFrame` | Use `requestAnimationFrame` + `setTimeout(0)` or double `rAF` | `rAF` runs *before* paint, not after. |
| **Mixing Sync and Async Returns** | Calling callback synchronously on cache hit | Wrap synchronous paths in `queueMicrotask` | Breaks caller assumptions about ordering (Zalgo). |
| **Nested `setTimeout` Clamping Surprise** | Rapidly rescheduling tasks via `setTimeout(0)` | Use `setImmediate` (Node) or `MessageChannel` (Browser) | Nested timers get clamped to 4ms after 5 iterations. |
| **Ignoring Microtask Checkpoints between Macrotasks**| Assuming macrotasks batch without microtasks running | Rely on microtasks draining after each macrotask | Microtasks always drain immediately after each individual macrotask. |

---

## 8. Performance Characteristics

- **Queue Dispatch Latency**:
  - `process.nextTick`: $\approx 10\text{ns}$ (internal Node array queue).
  - `queueMicrotask`: $\approx 30\text{ns}$ (V8 C++ microtask ring buffer).
  - `setImmediate`: $\approx 800\text{ns}$ (libuv check handle iteration).
  - `setTimeout(0)`: $\approx 1,200\text{ns} - 4,000\text{ns}$ (min-heap timer management).
- **CPU Starvation**: Microtasks do not yield to the OS event demultiplexer (epoll/kqueue), making excessive microtasks a primary cause of high latency percentiles (p99).

---

## 9. Engine Internals

1. **Node.js TickProcessor**: Node.js implements an internal C++ `TickInfo` struct. When returning from C++ bindings to JavaScript, Node checks `tick_info->has_scheduled_ticks()`. If true, it invokes `_tickCallback()` before letting libuv proceed.
2. **Microtask Queue in V8**: V8 maintains an isolate-wide microtask queue. When the JavaScript call stack height reaches zero, V8 triggers `MicrotasksScope::PerformCheckpoint()`.

---

## 10. Best Practices & Design Guidelines

1. **Use `queueMicrotask` for Cross-Platform Deferral**: When code runs in both browser and Node.js environments, avoid `process.nextTick` or `setImmediate` in favor of standard `queueMicrotask()`.
2. **Use `setImmediate` in Node.js for Breaking Up Long CPU Tasks**: `setImmediate` yields to the libuv event loop, allowing I/O polling to occur between task chunks.
3. **Use `requestAnimationFrame` Exclusively for Visual Changes**: Keep DOM measurements and CSS transitions inside `requestAnimationFrame` callbacks.
4. **Never Create Unbounded `process.nextTick` Recursive Chains**: Always include termination or fallback yielding conditions.

---

## 11. Real-World Case Study

### Eliminating Race Conditions in Node.js Event Emitter Initialization

A database client library emits a `'connect'` event. If the client connects immediately from an in-memory cache, synchronous emission happens before the user has attached an event listener.

```javascript
import { EventEmitter } from 'node:events';

class DatabaseClient extends EventEmitter {
  constructor(options) {
    super();
    this.options = options;
    this.#init();
  }

  #init() {
    if (this.options.useMock) {
      // ANTI-PATTERN: this.emit('connect') here would fail because
      // the caller has not yet attached .on('connect', ...) !

      // SOLUTION: Use process.nextTick to defer the event until
      // after the constructor returns and listeners are registered,
      // but BEFORE any external I/O or timers can interleave!
      process.nextTick(() => {
        this.emit('connect', { mock: true });
      });
      return;
    }

    // Real asynchronous connection
    realNetworkConnect(this.options).then(() => {
      this.emit('connect', { mock: false });
    });
  }
}

// Client usage
const db = new DatabaseClient({ useMock: true });

// Listener attaches safely because nextTick has not fired yet!
db.on('connect', (info) => {
  console.log('Database connected successfully:', info);
});
```

---

## 12. Interview Questions

<details>
<summary><strong>1. Why is the execution order of <code>setTimeout(fn, 0)</code> and <code>setImmediate(fn)</code> non-deterministic at top-level in Node.js?</strong></summary>

When Node.js starts, it compiles and executes the top-level script. Entering the libuv event loop takes a variable fraction of a millisecond. If the loop enters before the OS timer clock advances past 1ms, the timer is not yet ready, so libuv proceeds to the Poll and Check phases, executing `setImmediate` first. If the clock ticked past 1ms during startup, `setTimeout` executes first.
</details>

<details>
<summary><strong>2. Why is <code>setImmediate</code> guaranteed to run before <code>setTimeout(fn, 0)</code> inside an I/O callback?</strong></summary>

I/O callbacks execute during the libuv "Poll" phase. After the Poll phase completes, the event loop strictly transitions to the "Check" phase (`setImmediate`) before circling back around to the "Timers" phase (`setTimeout`).
</details>

<details>
<summary><strong>3. How does <code>process.nextTick()</code> interact with the Promise microtask queue?</strong></summary>

In Node.js, `process.nextTick` callbacks are maintained in an internal queue separate from the V8 microtask queue. When the call stack clears, Node.js completely drains the `process.nextTick` queue *before* draining the Promise microtask queue.
</details>

<details>
<summary><strong>4. What is the difference between Node.js 10 and Node.js 11+ regarding microtask execution?</strong></summary>

In Node.js 10 and earlier, microtasks were only drained between full libuv phases. Since Node.js 11, microtasks drain immediately after *every individual callback* within a phase, bringing Node.js into alignment with browser standards.
</details>

<details>
<summary><strong>5. In browsers, when does <code>requestAnimationFrame</code> execute relative to microtasks and paint?</strong></summary>

`requestAnimationFrame` callbacks execute after macrotasks and microtasks drain, right before the browser calculates layout, style recalculations, and paint.
</details>

<details>
<summary><strong>6. What happens if an error is thrown inside <code>process.nextTick</code>?</strong></summary>

The error bubbles up to `process.on('uncaughtException')`. If unhandled, the Node.js process terminates immediately with a non-zero exit code.
</details>

<details>
<summary><strong>7. How can you schedule a callback to run after the browser has finished painting?</strong></summary>

By nesting `setTimeout(fn, 0)` inside a `requestAnimationFrame(fn)` callback, or by nesting two `requestAnimationFrame` calls.
</details>

<details>
<summary><strong>8. Why is <code>queueMicrotask</code> preferred over <code>process.nextTick</code> in modern Node.js applications?</strong></summary>

Because `queueMicrotask` is a standardized web API supported across all runtimes (browsers, Node.js, Deno, Bun), whereas `process.nextTick` is proprietary to Node.js and can easily cause event loop starvation if misused.
</details>

---

## 13. Exercises & Challenges

### Challenge 1: The Ultimate Task Order Prediction
Predict the complete console output of this script:

```javascript
console.log('1');

setTimeout(() => {
  console.log('2');
  Promise.resolve().then(() => console.log('3'));
}, 0);

new Promise((resolve) => {
  console.log('4');
  resolve();
}).then(() => {
  console.log('5');
  setTimeout(() => console.log('6'), 0);
});

console.log('7');
```

<details>
<summary>View Solution & Explanation</summary>

**Output:**
```
1
4
7
5
2
3
6
```

**Step Breakdown:**
1. Log `1`.
2. Schedule `setTimeout(2)` as macrotask #1.
3. In Promise constructor: log `4` (constructor is synchronous!). `resolve()` schedules `.then(5)` in microtasks.
4. Log `7`.
5. Call stack clears. Microtasks drain: log `5`, schedules `setTimeout(6)` as macrotask #2.
6. Macrotask #1 (`setTimeout 2`) runs: logs `2`, schedules `.then(3)` in microtasks.
7. Microtask drains: logs `3`.
8. Macrotask #2 (`setTimeout 6`) runs: logs `6`.
</details>

### Challenge 2: Implement a Safe Cross-Platform Immediate Dispatcher
Write a utility function `runImmediate(callback)` that uses `setImmediate` in Node.js, `MessageChannel` in browsers, and falls back to `setTimeout(fn, 0)` in environments where neither is available.

<details>
<summary>View Solution</summary>

```javascript
function runImmediate(callback) {
  if (typeof setImmediate === 'function') {
    return setImmediate(callback);
  }

  if (typeof MessageChannel !== 'undefined') {
    const channel = new MessageChannel();
    channel.port1.onmessage = () => {
      channel.port1.close();
      channel.port2.close();
      callback();
    };
    channel.port2.postMessage(null);
    return;
  }

  return setTimeout(callback, 0);
}
```
</details>
