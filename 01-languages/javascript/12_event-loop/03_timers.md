# Timers & Scheduling

> JavaScript timers provide temporal scheduling for deferred execution, governed by browser nesting clamps, host timer heaps, monotonic performance clocks, and background tab throttling.

---

## Metadata

| Attribute | Details |
| :--- | :--- |
| **Level** | Intermediate |
| **Prerequisites** | [Event Loop](./01_event-loop.md), [Microtasks & Macrotasks](./02_microtasks-and-macrotasks.md) |
| **Time to Read** | 22 minutes |
| **Status** | Complete |
| **Standard** | WHATWG HTML Standard / Node.js Timer Architecture / Baseline Widely Available |

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

JavaScript environments do not include hardware sleep or synchronous pause instructions on the main thread. Attempting to freeze execution via a busy loop (`while (Date.now() < target)`) locks the CPU at 100%, freezes all user interface rendering, and blocks incoming network I/O. 

To execute code after a delay or at recurring intervals, host environments provide **Timers**: `setTimeout`, `setInterval`, and associated cancellation primitives (`clearTimeout`, `clearInterval`). Rather than pausing the execution thread, timers register an expiration event with the host platform (browser or Node.js runtime) and allow the JavaScript call stack to unwind. Once the timer expires, its callback is scheduled as a macrotask in the event loop.

---

## 2. Core Concepts

### Minimum Delay vs Exact Execution
The delay argument passed to `setTimeout(callback, delayMs)` defines the **minimum time** the runtime will wait before enqueuing the callback onto the macrotask queue. It does **not** guarantee that the callback will execute precisely at `T + delayMs`. If synchronous code or microtasks are running when the timer fires, the callback must wait until the call stack clears.

### The 4ms HTML Nesting Clamp
Under the WHATWG HTML standard, browsers impose an artificial delay clamp: if a timer is nested $\ge 5$ levels deep (via repeated `setTimeout` calls) or if `setInterval` has executed $\ge 5$ times, the minimum delay is clamped to **at least 4 milliseconds**, even if $0\text{ms}$ was specified.

### Timer Drift
Because `setInterval(fn, interval)` schedules each execution relative to when the callback is dispatched rather than when it completes, CPU congestion or heavy synchronous work causes intervals to drift forward in time, accumulating cumulative latency.

```text
Scheduled: |----- 100ms -----|----- 100ms -----|----- 100ms -----|
Actual:    |----- 100ms -----|======== 180ms ========|-- 60ms --|
                               ^ heavy CPU block      ^ queue catch-up
```

---

## 3. How It Works Internally

When code invokes `setTimeout(fn, delay)`:

```
 User calls: setTimeout(callback, 250)
                   |
                   v
 [1] Compute expiration timestamp: now() + 250ms
     Allocate Timer node in Host Timer Heap (Priority Queue / Min-Heap)
                   |
                   v
 [2] JavaScript Call Stack continues executing synchronous code
                   |
                   v
 [3] Host Platform Timer Thread / Event Demultiplexer monitors expiration
                   |
     +-------------+-------------+
     | Has 250ms elapsed?        |
     +---------------------------+
                   |
             YES   v
 [4] Move timer callback from Timer Heap into the Macrotask Queue
                   |
                   v
 [5] Event Loop picks task when Call Stack is empty and Microtasks drained
                   |
                   v
 [6] Execute callback() on the main JavaScript thread
```

---

## 4. Syntax & API Reference

### Core Timer Functions

```javascript
// Schedule one-shot timer
const timerId = setTimeout((arg1, arg2) => {
  console.log(`Executed with: ${arg1}, ${arg2}`);
}, 500, 'paramA', 'paramB');

// Cancel pending timer
clearTimeout(timerId);

// Schedule recurring interval
const intervalId = setInterval(() => {
  console.log('Recurring tick');
}, 1000);

// Cancel interval
clearInterval(intervalId);
```

### Node.js Timer Controls: `ref()` and `unref()`

```javascript
// In Node.js, timers keep the process event loop alive by default.
const backgroundTimer = setTimeout(() => {
  console.log('This will only run if other work keeps Node alive');
}, 10_000);

// Unref: Do not keep the Node.js process alive if this is the only active handle
backgroundTimer.unref();

// Ref: Restore default behavior
backgroundTimer.ref();
```

---

## 5. Practical Examples

### Example 1: Drift-Free Self-Correcting Interval
```javascript
function createAccurateInterval(callback, intervalMs) {
  let expected = performance.now() + intervalMs;
  let timerId = null;
  let isRunning = true;

  function step() {
    if (!isRunning) return;

    const drift = performance.now() - expected;
    callback();

    expected += intervalMs;
    // Calculate adjusted delay to compensate for execution drift
    const nextDelay = Math.max(0, intervalMs - drift);
    timerId = setTimeout(step, nextDelay);
  }

  timerId = setTimeout(step, intervalMs);

  return {
    cancel: () => {
      isRunning = false;
      clearTimeout(timerId);
    },
  };
}

// Usage
const ticker = createAccurateInterval(() => {
  console.log('Accurate tick at:', performance.now());
}, 1000);
```

### Example 2: Debounce Utility with Immediate Trailing Option
```javascript
function debounce(fn, waitMs, immediate = false) {
  let timerId = null;

  return function (...args) {
    const callNow = immediate && !timerId;

    clearTimeout(timerId);

    timerId = setTimeout(() => {
      timerId = null;
      if (!immediate) fn.apply(this, args);
    }, waitMs);

    if (callNow) {
      fn.apply(this, args);
    }
  };
}

// Usage for search input
const onSearch = debounce(query => {
  console.log('Query API:', query);
}, 300);
```

### Example 3: Throttle Utility via Monotonic Clocks
```javascript
function throttle(fn, limitMs) {
  let lastCall = 0;
  let timerId = null;

  return function (...args) {
    const now = performance.now();
    const remaining = limitMs - (now - lastCall);

    if (remaining <= 0) {
      clearTimeout(timerId);
      timerId = null;
      lastCall = now;
      fn.apply(this, args);
    } else if (!timerId) {
      timerId = setTimeout(() => {
        lastCall = performance.now();
        timerId = null;
        fn.apply(this, args);
      }, remaining);
    }
  };
}
```

### Example 4: Timeout Guard for Async Operations
```javascript
async function withTimeout(asyncFn, timeoutMs, timeoutMessage = 'Operation timed out') {
  let timerId;

  const timeoutPromise = new Promise((_, reject) => {
    timerId = setTimeout(() => {
      reject(new Error(timeoutMessage));
    }, timeoutMs);
  });

  try {
    return await Promise.race([asyncFn(), timeoutPromise]);
  } finally {
    clearTimeout(timerId); // Guarantee timer cleanup to prevent memory leaks
  }
}
```

### Example 5: Zero-Delay Yield via `MessageChannel` (Bypassing 4ms Clamp)
```javascript
function yieldToEventLoop() {
  return new Promise(resolve => {
    const channel = new MessageChannel();
    channel.port1.onmessage = () => {
      channel.port1.close();
      channel.port2.close();
      resolve();
    };
    channel.port2.postMessage(null);
  });
}

// Bypasses 4ms clamping penalty in nested loops
async function longRunningWorker() {
  for (let i = 0; i < 1000; i++) {
    // Process unit
    if (i % 10 === 0) {
      await yieldToEventLoop(); // Rapid macrotask yield
    }
  }
}
```

---

## 6. Edge Cases, Pitfalls & Gotchas

1. **Background Tab Throttling**: When a browser tab is inactive, hidden, or minimized, browsers throttle timers aggressively (e.g., to once every 1,000ms or even once per minute) to preserve battery and CPU. Never rely on `setInterval` for mission-critical background timing in web apps.
2. **Timer ID Overflows & Type Differences**: In browsers, `setTimeout` returns an integer identifier (`number`). In Node.js, `setTimeout` returns a `Timeout` object containing `.ref()` and `.unref()` methods. Passing a Node `Timeout` object to standard browser typings can cause runtime confusion if polymorphic code is assumed.
3. **Overlapping `setInterval` Execution**: If an async task inside `setInterval` takes longer to complete than the interval delay, multiple async tasks will execute simultaneously or back-to-back. Use recursive `setTimeout` instead of `setInterval` for async operations.
4. **Max 32-Bit Signed Integer Timeout**: The maximum supported delay for `setTimeout` is $2^{31} - 1 = 2,147,483,647\text{ms}$ (~24.8 days). Supplying a value greater than this causes 32-bit integer overflow in V8, resulting in the timer firing immediately ($0\text{ms}$).

---

## 7. Common Mistakes & Anti-Patterns

| Mistake | Incorrect Pattern | Correct Solution | Why It Fails |
| :--- | :--- | :--- | :--- |
| **Using `setInterval` for Async Tasks** | `setInterval(async () => await fetch(), 1000)` | Use recursive `setTimeout` after await settles | Heavy network delays cause overlapping concurrent executions. |
| **Passing Strings to `setTimeout`** | `setTimeout("doWork()", 1000)` | Pass function reference `setTimeout(doWork, 1000)` | Invokes `eval()`, creating security risks and performance penalties. |
| **Exceeding 32-bit Integer Timeout** | `setTimeout(fn, 30 * 24 * 60 * 60 * 1000)` | Chunk delays into 24-day segments | Integer overflow in V8 causes timer to fire immediately. |
| **Forgetting to `clearTimeout`** | Abandoning timer on component unmount | Clean up active timers in teardown | Retains closures in memory, causing leaks and crashes. |
| **Using `Date.now()` for Benchmarking** | Measuring elapsed time with `Date.now()` | Use `performance.now()` | Wall clock can be adjusted by OS/NTP time sync, skewing deltas. |
| **Assuming Timers Run on Isolated Threads** | Running heavy CPU math inside `setTimeout` | Move heavy computation to Web Worker | Callback executes on the single main JavaScript thread. |

---

## 8. Performance Characteristics

- **Host Timer Storage**: Modern runtimes (V8/libuv and Chromium) organize active timers using a **Min-Heap (Binary Priority Queue)** sorted by expiration time ($O(\log N)$ insertion and deletion, $O(1)$ peek).
- **Tab Throttling Impact**: In modern browsers, inactive tabs throttle timers to $\ge 1000\text{ms}$, reducing CPU wakeups by up to 90%.
- **Clamping Latency**: Recursive `setTimeout` loops in browsers incur a hard minimum 4ms latency penalty after the 4th level of nesting.

---

## 9. Engine Internals

1. **Node.js libuv Timer Wheel / Min-Heap**: Node.js groups timers with identical durations into circular linked lists indexed by duration. This provides $O(1)$ insertion for the most common timer durations (such as 1000ms), falling back to a min-heap only for arbitrary expiration timestamps.
2. **`unref()` Mechanism in Node**: When a timer is created, it increments libuv's active handle reference count. Calling `timer.unref()` decrements the handle counter. When libuv's active handle count reaches zero, the event loop terminates even if unreferenced timers remain pending.

---

## 10. Best Practices & Design Guidelines

1. **Prefer Recursive `setTimeout` over `setInterval`**: Recursive scheduling ensures the next timer delay only starts after the previous operation completes, preventing overlapping executions.
2. **Always Store and Clean Up Timer IDs**: Never fire fire-and-forget intervals; provide cancellation hooks for all persistent timers.
3. **Use `performance.now()` for Elapsed Duration**: Monotonic clocks guarantee that time always moves forward, regardless of system clock updates.
4. **Use Web Workers for Background Accuracy**: If a web application requires un-throttled accurate timing while the tab is in the background (e.g., audio playback or metronome), run the timer inside a Dedicated Web Worker.

---

## 11. Real-World Case Study

### Resilient Polling Engine with Dynamic Jitter and Backoff

A microservice frontend monitors a long-running asynchronous batch processing job on the backend. The poller must handle transient network errors, avoid interval overlap, and throttle down polling frequency over time.

```javascript
class PollingMonitor {
  #url;
  #timerId = null;
  #isPolling = false;

  constructor(url) {
    this.#url = url;
  }

  start(onResult, onError) {
    if (this.#isPolling) return;
    this.#isPolling = true;

    let attempt = 0;
    const baseInterval = 1000;
    const maxInterval = 30000;

    const poll = async () => {
      if (!this.#isPolling) return;

      try {
        const response = await fetch(this.#url);
        if (!response.ok) throw new Error(`HTTP ${response.status}`);
        const data = await response.json();

        onResult(data);

        if (data.status === 'COMPLETE' || data.status === 'FAILED') {
          this.stop();
          return;
        }

        // Successful poll: keep interval standard
        attempt = 0;
        this.#scheduleNext(baseInterval, poll);
      } catch (err) {
        attempt++;
        onError(err);

        // Exponential backoff with random jitter on error
        const backoff = Math.min(maxInterval, baseInterval * 2 ** attempt);
        const jitter = Math.floor(Math.random() * (backoff * 0.2));
        this.#scheduleNext(backoff + jitter, poll);
      }
    };

    poll();
  }

  #scheduleNext(delayMs, callback) {
    if (!this.#isPolling) return;
    this.#timerId = setTimeout(callback, delayMs);
  }

  stop() {
    this.#isPolling = false;
    if (this.#timerId !== null) {
      clearTimeout(this.#timerId);
      this.#timerId = null;
    }
  }
}
```

---

## 12. Interview Questions

<details>
<summary><strong>1. What does the delay parameter in <code>setTimeout(fn, 100)</code> actually guarantee?</strong></summary>

It guarantees that the runtime will wait *at least* 100 milliseconds before adding `fn` to the macrotask queue. It does not guarantee that the function will execute at precisely 100ms, as any running synchronous code or pending microtasks will delay its execution.
</details>

<details>
<summary><strong>2. What is the 4ms clamping rule in browsers?</strong></summary>

Under the WHATWG specification, once a timer has been nested 5 or more times (e.g. recursive `setTimeout` calls) or an interval has executed 5 times, the browser enforces a minimum delay clamp of 4 milliseconds, even if 0ms was requested.
</details>

<details>
<summary><strong>3. What is the difference between <code>Date.now()</code> and <code>performance.now()</code>?</strong></summary>

`Date.now()` reflects the system wall clock (Unix timestamp), which is subject to system clock changes, leap seconds, and NTP synchronization. `performance.now()` returns a high-resolution, monotonic timestamp measured from page navigation start that never steps backward.
</details>

<details>
<summary><strong>4. What happens if you pass a delay larger than 2,147,483,647ms to <code>setTimeout</code>?</strong></summary>

V8 stores timer delays as 32-bit signed integers. Any value exceeding $2^{31} - 1$ overflows to a negative integer, causing the runtime to treat the delay as 0ms, firing the callback immediately.
</details>

<details>
<summary><strong>5. In Node.js, what do <code>timer.unref()</code> and <code>timer.ref()</code> do?</strong></summary>

`timer.unref()` tells the Node.js event loop that this timer should not prevent the process from exiting if it is the only active handle remaining. `timer.ref()` restores the default behavior, keeping the process running until the timer fires.
</details>

<details>
<summary><strong>6. Why should you avoid using <code>setInterval</code> for asynchronous network requests?</strong></summary>

If a network request takes longer to resolve than the specified interval duration, multiple requests will overlap concurrently, potentially exhausting socket limits and causing out-of-order response processing.
</details>

<details>
<summary><strong>7. How do modern browsers handle timers in background or inactive tabs?</strong></summary>

Browsers throttle timers in background tabs to at least 1,000ms (or even suspend them entirely) to conserve battery, CPU cycles, and memory.
</details>

<details>
<summary><strong>8. How can you schedule a macrotask with true zero-delay in the browser without the 4ms clamp?</strong></summary>

Using `MessageChannel` (`port.postMessage(null)`). It schedules an immediate macrotask in the event loop without being subject to the 4ms nested timer clamp.
</details>

---

## 13. Exercises & Challenges

### Challenge 1: Implement an Async Sleep with Cancellation
Write a function `sleep(ms, signal)` that returns a promise that resolves after `ms` milliseconds, but rejects immediately with an `AbortError` if the provided `AbortSignal` is triggered.

<details>
<summary>View Solution</summary>

```javascript
function sleep(ms, signal) {
  return new Promise((resolve, reject) => {
    if (signal?.aborted) {
      return reject(signal.reason);
    }

    let timerId;

    const onAbort = () => {
      clearTimeout(timerId);
      reject(signal.reason);
    };

    timerId = setTimeout(() => {
      signal?.removeEventListener('abort', onAbort);
      resolve();
    }, ms);

    signal?.addEventListener('abort', onAbort, { once: true });
  });
}
```
</details>

### Challenge 2: Implement a Polling Timeout with Max Retries
Write an async function `pollUntil(predicateFn, intervalMs, timeoutMs)` that repeatedly calls `predicateFn` every `intervalMs` until it returns a truthy value, or rejects if `timeoutMs` elapses first.

<details>
<summary>View Solution</summary>

```javascript
async function pollUntil(predicateFn, intervalMs, timeoutMs) {
  const startTime = performance.now();

  while (true) {
    const result = await predicateFn();
    if (result) return result;

    if (performance.now() - startTime >= timeoutMs) {
      throw new Error(`Polling timed out after ${timeoutMs}ms`);
    }

    await new Promise(resolve => setTimeout(resolve, intervalMs));
  }
}
```
</details>
