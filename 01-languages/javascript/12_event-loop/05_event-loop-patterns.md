# Event Loop Performance Patterns

> Practical engineering patterns to maintain responsive user interfaces and high-throughput servers by breaking up Long Tasks, prioritizing interactive input, and cooperatively yielding to the event loop.

---

## Metadata

| Attribute | Details |
| :--- | :--- |
| **Level** | Advanced |
| **Prerequisites** | [The Event Loop](./01_event-loop.md), [Microtasks & Macrotasks](./02_microtasks-and-macrotasks.md), [Task Ordering](./04_task-ordering.md) |
| **Time to Read** | 24 minutes |
| **Status** | Complete |
| **Standard** | Prioritized Task Scheduling API (`scheduler.yield`, `scheduler.postTask`) / HTML5 / Baseline Widely Available |

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

Because JavaScript runs on a single main thread, long-running synchronous code directly degrades application responsiveness. In the browser, any task executing for more than **50 milliseconds** is classified as a **Long Task**. Long Tasks block user inputs (clicks, typing, gestures), delay visual animations (causing dropped frames and jank), and severely damage Google Core Web Vitals—specifically **INP (Interaction to Next Paint)** and **TBT (Total Blocking Time)**.

In Node.js, CPU-bound operations (such as synchronous encryption, large JSON parsing, or regex evaluation) starve the `libuv` event loop, delaying incoming HTTP connections and health checks, leading to cascading request timeouts.

Event loop performance patterns provide structured mechanisms to cooperatively slice CPU-intensive computation into bite-sized chunks, yielding execution back to the event loop so high-priority user interactions and I/O can be serviced immediately.

---

## 2. Core Concepts

### The 50ms Long Task Threshold
Display monitors refresh at 60Hz ($16.6\text{ms}$ per frame) or 120Hz ($8.3\text{ms}$ per frame). When user input arrives during a long task, the browser cannot dispatch the input event or re-render until the current task finishes.

```text
User Click Arrives Here
       │
       ▼
[============== Long Task: 180ms Execution ==============] ──> [ Paint ]
 ▲                                                       ▲
 │                                                       │
 Task Begins                                             User sees visual feedback
 (Input delayed by 180ms! Severe INP penalty)
```

### Cooperative Chunking vs Preemption
JavaScript lacks native thread preemption on the main thread: the engine cannot forcibly interrupt a running `for` loop. The developer must design **cooperative yielding points**:
1. Check elapsed execution time against a frame budget (e.g., $10\text{ms} - 16\text{ms}$).
2. If the budget is exceeded, yield to the event loop.
3. Resume execution in the next tick.

---

## 3. How It Works Internally

### Slicing Computation via `scheduler.yield()`

When using modern cooperative scheduling:

```
+-------------------------------------------------------------+
|               Chunk 1 (Runs for ~10ms)                      |
+-------------------------------------------------------------+
                               |
                               v
               [ await scheduler.yield() ]
                               |
                               v
+-------------------------------------------------------------+
|                     EVENT LOOP BREAK                        |
|  - Process urgent user clicks & keyboard events             |
|  - Calculate Layout & Paint Frame to Screen                 |
+-------------------------------------------------------------+
                               |
                               v
+-------------------------------------------------------------+
|               Chunk 2 (Runs for ~10ms)                      |
+-------------------------------------------------------------+
```

Unlike legacy `setTimeout(fn, 0)`, `scheduler.yield()`:
1. Does not suffer from the 4ms nested timer clamp.
2. Continues at the front of the prioritized task queue, preventing background tasks from jumping ahead.

---

## 4. Syntax & API Reference

### Modern Prioritized Scheduling APIs

```javascript
// 1. Modern cooperative yield (Chrome 129+, Baseline Newly Available)
await scheduler.yield();

// 2. Prioritized Task Scheduling via scheduler.postTask
scheduler.postTask(() => {
  console.log('User-blocking task');
}, { priority: 'user-blocking' }); // Priorities: 'user-blocking' | 'user-visible' | 'background'

// 3. User input inspection (Chromium)
if (navigator.scheduling?.isInputPending?.()) {
  // Urgent input is waiting in the OS queue! Yield immediately!
}

// 4. Cross-runtime Yield Fallback
function yieldToEventLoop() {
  if (globalThis.scheduler?.yield) {
    return scheduler.yield();
  }
  return new Promise(resolve => setTimeout(resolve, 0));
}
```

---

## 5. Practical Examples

### Example 1: Budget-Aware Time-Slicing Loop
```javascript
async function processLargeCollection(items, processItem, maxBudgetMs = 12) {
  let lastYieldTime = performance.now();

  for (let i = 0; i < items.length; i++) {
    processItem(items[i], i);

    // Check if current chunk has exceeded our frame budget
    if (performance.now() - lastYieldTime > maxBudgetMs) {
      await yieldToEventLoop();
      lastYieldTime = performance.now();
    }
  }
}

function yieldToEventLoop() {
  if (globalThis.scheduler?.yield) return scheduler.yield();
  return new Promise(resolve => setTimeout(resolve, 0));
}
```

### Example 2: Responsive Input-Aware Yielding with `isInputPending`
```javascript
async function searchThroughMillions(dataset, filterQuery) {
  const matches = [];

  for (let i = 0; i < dataset.length; i++) {
    if (dataset[i].includes(filterQuery)) {
      matches.push(dataset[i]);
    }

    // Every 500 items, check if the user is typing or clicking
    if (i % 500 === 0) {
      const hasPendingInput = navigator.scheduling?.isInputPending?.({ includeContinuous: true });
      if (hasPendingInput) {
        // Yield immediately to let browser service the user interaction
        await yieldToEventLoop();
      }
    }
  }

  return matches;
}
```

### Example 3: Prioritized Task Queuing via `scheduler.postTask`
```javascript
class DashboardDataLoader {
  loadData() {
    // 1. Critical user-facing chart data
    scheduler.postTask(() => this.renderPrimaryChart(), {
      priority: 'user-blocking',
    });

    // 2. Secondary sidebar data
    scheduler.postTask(() => this.renderSidebarStats(), {
      priority: 'user-visible',
    });

    // 3. Background pre-fetching and analytics logging
    scheduler.postTask(() => this.sendAnalytics(), {
      priority: 'background',
    });
  }

  renderPrimaryChart() { console.log('Primary chart rendered'); }
  renderSidebarStats() { console.log('Sidebar stats rendered'); }
  sendAnalytics() { console.log('Analytics sent'); }
}
```

### Example 4: Progressive DOM Insertion via `requestAnimationFrame`
```javascript
function render10000Items(container, items, chunkSize = 100) {
  let index = 0;

  function renderChunk() {
    const fragment = document.createDocumentFragment();
    const limit = Math.min(index + chunkSize, items.length);

    while (index < limit) {
      const div = document.createElement('div');
      div.textContent = items[index];
      fragment.appendChild(div);
      index++;
    }

    container.appendChild(fragment);

    if (index < items.length) {
      // Schedule next visual chunk aligned to next monitor refresh
      requestAnimationFrame(renderChunk);
    }
  }

  requestAnimationFrame(renderChunk);
}
```

### Example 5: Node.js Non-Blocking Batch JSON Serializer
```javascript
import { setImmediate } from 'node:timers/promises';

async function streamLargeJsonExport(records, writeStream) {
  writeStream.write('[\n');

  for (let i = 0; i < records.length; i++) {
    const jsonStr = JSON.stringify(records[i]);
    const isLast = i === records.length - 1;
    writeStream.write(jsonStr + (isLast ? '\n' : ',\n'));

    // Yield every 500 items to keep HTTP server responsive to incoming connections
    if (i % 500 === 0) {
      await setImmediate();
    }
  }

  writeStream.write(']\n');
  writeStream.end();
}
```

---

## 6. Edge Cases, Pitfalls & Gotchas

1. **State Inconsistency Across Yield Boundaries**: Yielding to the event loop allows external events and user interactions to interleave! If global or component state changes while your loop is paused, subsequent iterations may operate on stale or mutated state.
2. **`scheduler.yield()` Support**: While shipping in Chromium and modern Node.js, older browsers require polyfills using `MessageChannel` or `setTimeout`.
3. **Microtask Yield Trap**: Using `await Promise.resolve()` or `queueMicrotask()` to yield **does not yield to the event loop**! It keeps execution inside the current task before rendering or I/O can run.

---

## 7. Common Mistakes & Anti-Patterns

| Mistake | Incorrect Pattern | Correct Solution | Why It Fails |
| :--- | :--- | :--- | :--- |
| **Yielding with Microtasks** | `await Promise.resolve()` | `await scheduler.yield()` or `setTimeout(0)` | Microtasks drain immediately without yielding to UI paint or macrotasks. |
| **Fixed Loop Chunk Size** | Always chunking by fixed 1000 items | Measure elapsed time with `performance.now()` | Processing time varies wildly based on data complexity and device CPU. |
| **Unprotected State across Yields** | Modifying shared object across yield points | Capture snapshots or use transactional locks | Race conditions occur when concurrent user actions alter state mid-computation. |
| **Excessive Yielding Overhead** | Yielding on every single loop iteration | Yield only after budget (e.g. 10ms–15ms) | Macrotask switching overhead exceeds useful computation time. |
| **Blocking Server via Sync Crypto** | `crypto.pbkdf2Sync(...)` on Node main thread | Use async `crypto.pbkdf2` with worker pool | Freezes all incoming HTTP requests for all clients. |
| **Ignoring Task Priorities** | Running background analytics at default priority | Use `scheduler.postTask({ priority: 'background' })` | Background sync delays urgent interactive user responses. |

---

## 8. Performance Characteristics

- **INP / TBT Optimization**: Slicing tasks to stay below 50ms eliminates Total Blocking Time (TBT) and ensures Interaction to Next Paint (INP) stays within the "Good" threshold ($< 200\text{ms}$).
- **Yield Overhead Benchmark**:
  - `scheduler.yield()`: $\approx 0.05\text{ms}$ scheduling overhead.
  - `MessageChannel`: $\approx 0.1\text{ms}$ scheduling overhead.
  - `setTimeout(0)`: $\approx 1.5\text{ms} - 4.5\text{ms}$ overhead due to timer clamping.

---

## 9. Engine Internals

1. **Blink / Chromium Task Scheduling**: Chromium categorizes tasks into internal priority bands: `kHighPriority` (compositor, input), `kNormalPriority`, and `kBestEffortPriority` (analytics). `scheduler.postTask()` maps JavaScript callbacks directly into these native C++ task queues.
2. **`scheduler.yield()` Continuation Queue**: When yielding with `scheduler.yield()`, Chromium creates an internal task continuation that preserves the caller's inherited task priority, placing it behind user input tasks but ahead of background macrotasks.

---

## 10. Best Practices & Design Guidelines

1. **Target 50ms Maximum Task Duration**: Ensure no synchronous block runs longer than 50ms; ideally target 10ms–16ms for 60 FPS environments.
2. **Combine Time Budgets with Yielding**: Track execution time using `performance.now()` and yield only when the frame budget expires.
3. **Use Web Workers for True Parallelism**: If heavy computation (e.g. image filters, PDF generation) does not require DOM access, offload it entirely to a Web Worker.
4. **Isolate State across Yields**: Copy or snapshot critical state before yielding to protect against concurrent modifications.

---

## 11. Real-World Case Study

### High-Volume Markdown Parser with Responsive Typing

A collaborative markdown editor parses large 50,000-word documents on every keystroke. Naive synchronous parsing took 350ms, freezing the cursor and creating severe typing lag.

```javascript
class ResponsiveMarkdownEditor {
  #currentJobId = 0;
  #editorElement;

  constructor(editorElement) {
    this.#editorElement = editorElement;
    this.#editorElement.addEventListener('input', () => this.#handleInput());
  }

  async #handleInput() {
    const jobId = ++this.#currentJobId;
    const rawText = this.#editorElement.value;
    const lines = rawText.split('\n');
    const parsedHtmlChunks = [];

    const startTime = performance.now();
    const FRAME_BUDGET_MS = 12;

    for (let i = 0; i < lines.length; i++) {
      // Check if user typed again: cancel stale job immediately
      if (jobId !== this.#currentJobId) {
        return;
      }

      parsedHtmlChunks.push(this.#parseLine(lines[i]));

      // Yield when frame budget is exhausted or input is pending
      const elapsed = performance.now() - startTime;
      const inputPending = navigator.scheduling?.isInputPending?.();

      if (elapsed > FRAME_BUDGET_MS || inputPending) {
        await this.#yield();
      }
    }

    if (jobId === this.#currentJobId) {
      document.getElementById('preview').innerHTML = parsedHtmlChunks.join('');
    }
  }

  #parseLine(line) {
    // Regex markdown transformation
    return `<p>${line.replace(/\*\*(.*?)\*\*/g, '<strong>$1</strong>')}</p>`;
  }

  #yield() {
    if (globalThis.scheduler?.yield) return scheduler.yield();
    return new Promise(resolve => setTimeout(resolve, 0));
  }
}
```

---

## 12. Interview Questions

<details>
<summary><strong>1. What is a "Long Task" in browser performance metrics, and why does it matter?</strong></summary>

A Long Task is any task that takes longer than 50 milliseconds to execute on the main thread. It matters because it blocks user input handling, layout, and rendering, directly degrading Total Blocking Time (TBT) and Interaction to Next Paint (INP).
</details>

<details>
<summary><strong>2. Why doesn't <code>await Promise.resolve()</code> yield to the browser's rendering engine?</strong></summary>

Because Promise resolutions are scheduled in the microtask queue. The browser event loop guarantees that the entire microtask queue drains before moving to rendering or picking up subsequent macrotasks.
</details>

<details>
<summary><strong>3. How does <code>scheduler.yield()</code> differ from <code>setTimeout(fn, 0)</code>?</strong></summary>

`scheduler.yield()` is specifically designed for cooperative time-slicing. It yields to the event loop without the 4ms nested timer clamping penalty, and resumes execution at the appropriate task priority level rather than being pushed to the back of the generic timer queue.
</details>

<details>
<summary><strong>4. What does <code>navigator.scheduling.isInputPending()</code> do?</strong></summary>

It allows JavaScript to check whether user input events (such as clicks or keystrokes) are pending in the browser's OS input queue, enabling code to yield only when an actual user interaction is waiting to be processed.
</details>

<details>
<summary><strong>5. What are the three priorities supported by <code>scheduler.postTask()</code>?</strong></summary>

`'user-blocking'` (highest priority, for immediate interactions), `'user-visible'` (default priority, for rendering visible UI components), and `'background'` (lowest priority, for telemetry and pre-fetching).
</details>

<details>
<summary><strong>6. What is the major hazard of yielding control in the middle of a loop?</strong></summary>

State concurrency. When you yield, user interactions or network callbacks can execute and modify variables, the DOM, or shared application state. Your loop must be resilient against state mutations that occur across yield boundaries.
</details>

<details>
<summary><strong>7. When should you use a Web Worker instead of time-slicing?</strong></summary>

When a heavy CPU task (such as cryptography, image manipulation, or large dataset analysis) does not require direct access to the DOM and takes hundreds of milliseconds or seconds to compute. Web Workers run on completely separate OS threads.
</details>

<details>
<summary><strong>8. How does yielding to the event loop improve Node.js server performance?</strong></summary>

In Node.js, yielding prevents CPU-intensive loops from blocking the libuv event loop, allowing the server to accept new incoming TCP connections, process SSL handshakes, and respond to HTTP requests without latency spikes.
</details>

---

## 13. Exercises & Challenges

### Challenge 1: Implement a Time-Sliced Array Reducer
Write an async function `chunkedReduce(array, reducerFn, initialValue, timeBudgetMs = 10)` that reduces an array while yielding to the event loop whenever computation exceeds `timeBudgetMs`.

<details>
<summary>View Solution</summary>

```javascript
async function chunkedReduce(array, reducerFn, initialValue, timeBudgetMs = 10) {
  let accumulator = initialValue;
  let lastYield = performance.now();

  const yieldFn = () => {
    if (globalThis.scheduler?.yield) return scheduler.yield();
    return new Promise(resolve => setTimeout(resolve, 0));
  };

  for (let i = 0; i < array.length; i++) {
    accumulator = reducerFn(accumulator, array[i], i, array);

    if (performance.now() - lastYield > timeBudgetMs) {
      await yieldFn();
      lastYield = performance.now();
    }
  }

  return accumulator;
}
```
</details>

### Challenge 2: Priority Task Queue with Fallback
Implement a wrapper class `PriorityScheduler` that delegates to `scheduler.postTask()` if supported by the runtime, and falls back to `setTimeout` or `queueMicrotask` according to the requested priority.

<details>
<summary>View Solution</summary>

```javascript
class PriorityScheduler {
  static schedule(task, priority = 'user-visible') {
    if (typeof globalThis.scheduler?.postTask === 'function') {
      return globalThis.scheduler.postTask(task, { priority });
    }

    return new Promise((resolve, reject) => {
      const execute = async () => {
        try {
          resolve(await task());
        } catch (err) {
          reject(err);
        }
      };

      if (priority === 'user-blocking') {
        queueMicrotask(execute);
      } else if (priority === 'background') {
        if (typeof requestIdleCallback !== 'undefined') {
          requestIdleCallback(execute);
        } else {
          setTimeout(execute, 50);
        }
      } else {
        setTimeout(execute, 0);
      }
    });
  }
}
```
</details>
