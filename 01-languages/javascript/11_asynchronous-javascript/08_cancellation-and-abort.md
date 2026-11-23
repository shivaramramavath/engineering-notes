# Cancellation & AbortController

> The `AbortController` and `AbortSignal` interfaces provide the standard, unified cancellation primitive across modern JavaScript runtimes, enabling graceful cooperative task termination, timeout management, and resource teardown.

---

## Metadata

| Attribute | Details |
| :--- | :--- |
| **Level** | Intermediate to Advanced |
| **Prerequisites** | [Promises](./03_promises.md), [Async/Await](./05_async-await.md), [Event Loop](../12_event-loop/01_event-loop.md) |
| **Time to Read** | 22 minutes |
| **Status** | Complete |
| **Standard** | WHATWG DOM Standard / ECMAScript Integration / Node.js 15+ / Baseline Widely Available |

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

JavaScript Promises are inherently non-cancellable once constructed. Once an asynchronous operation—such as a network request, database query, file write, or timer—begins, the runtime offers no built-in syntax on the `Promise` object itself to halt execution mid-flight. Historically, developers invented non-standard patterns: boolean `isCancelled` flags, wrapper classes, or leaking orphaned requests that continued to consume CPU, socket connections, and memory long after the user navigated away.

To solve this across all runtimes, the WHATWG defined **`AbortController`** and **`AbortSignal`**. Originally created for the Fetch API, `AbortController` is now the universal standard cancellation mechanism across Web APIs, Node.js (`fs`, `child_process`, `events`, `http`), and popular libraries (Axios, Redux Toolkit, React Query).

Cancellation in JavaScript is **cooperative**: the caller signals an intent to cancel, and the asynchronous task periodically checks this signal or listens for its abort event to perform teardown and release resources.

---

## 2. Core Concepts

### `AbortController` vs `AbortSignal`
The separation of concerns between controller and signal follows the capability security model:
- **`AbortController`**: The owner/producer of the cancellation capability. It holds the `abort(reason)` method and must remain private to the caller who initiates the work.
- **`AbortSignal`**: A read-only event emitter object passed downstream to consumers. Observers can check `signal.aborted` or register an event listener on `'abort'`.

```text
+-----------------------+                    owns / controls
|    AbortController    | ------------------------------------+
+-----------------------+                                     |
| .abort([reason])      |                                     v
+-----------------------+                          +--------------------+
                                                   |    AbortSignal     |
                                                   +--------------------+
                                                   | .aborted: boolean  |
                                                   | .reason: any       |
                                                   | onabort event      |
                                                   +--------------------+
                                                             |
                                           Passed into worker/fetch/timer
                                                             v
                                                   +--------------------+
                                                   |  Async Operation   |
                                                   +--------------------+
```

### Modern Static Factory Methods
Modern JavaScript (ES2022+ / Node 17.3+) provides high-performance static constructors:
- **`AbortSignal.timeout(ms)`**: Automatically aborts after `ms` milliseconds with a `TimeoutError` DOMException.
- **`AbortSignal.any(signals)`**: Combines multiple signals into a single composite signal that triggers when *any* of the input signals aborts (ideal for combining user-driven aborts with operational timeouts).

---

## 3. How It Works Internally

When an `AbortController` triggers `abort(reason)`, the following sequence occurs synchronously:

```
 Caller triggers: controller.abort(customReason)
                      |
                      v
 [1] Check if controller.signal.aborted === true
     If already aborted, NO-OP (idempotent). Exit.
                      |
                      v
 [2] Set signal.aborted = true
     Set signal.reason = customReason ?? new DOMException('This operation was aborted', 'AbortError')
                      |
                      v
 [3] Dispatch 'abort' event synchronously to all registered signal listeners
                      |
     +----------------+----------------+
     |                                 |
     v                                 v
 Listener A: Socket abort          Listener B: Clear timer
 Close active TCP stream           ClearTimeout(timerId)
 Reject promise with AbortError    Reject promise with AbortError
```

Crucially, **abort listener dispatch is synchronous**: when `abort()` is invoked, all attached event listeners execute before `abort()` returns.

---

## 4. Syntax & API Reference

### Initializing & Aborting
```javascript
const controller = new AbortController();
const { signal } = controller;

// Abort with default DOMException("AbortError")
controller.abort();

// Abort with explicit custom reason
controller.abort(new Error('User closed the modal'));
```

### Signal Inspection & Event Handling
```javascript
// 1. Synchronous polling check
if (signal.aborted) {
  throw signal.reason;
}

// 2. Event listener with automatic cleanup
signal.addEventListener('abort', () => {
  console.log(`Operation halted due to: ${signal.reason}`);
}, { once: true });

// 3. Convenience method: signal.throwIfAborted()
signal.throwIfAborted(); // Throws signal.reason immediately if aborted
```

---

## 5. Practical Examples

### Example 1: Fetch with Manual and Timeout Cancellation
```javascript
async function fetchUserWithTimeout(userId, timeoutMs = 5000) {
  const timeoutSignal = AbortSignal.timeout(timeoutMs);

  try {
    const response = await fetch(`https://api.example.com/users/${userId}`, {
      signal: timeoutSignal,
    });
    return await response.json();
  } catch (error) {
    if (error.name === 'TimeoutError') {
      console.error(`Request timed out after ${timeoutMs}ms`);
    } else if (error.name === 'AbortError') {
      console.warn('Request was cancelled by caller');
    } else {
      console.error('Network error:', error);
    }
    throw error;
  }
}
```

### Example 2: Cancellable Delay / Sleep Function
```javascript
function sleep(ms, signal) {
  return new Promise((resolve, reject) => {
    if (signal?.aborted) {
      return reject(signal.reason);
    }

    let timer = null;

    const onAbort = () => {
      clearTimeout(timer);
      reject(signal.reason);
    };

    timer = setTimeout(() => {
      signal?.removeEventListener('abort', onAbort);
      resolve();
    }, ms);

    signal?.addEventListener('abort', onAbort, { once: true });
  });
}

// Usage
const ac = new AbortController();
setTimeout(() => ac.abort(new Error('Cancelled early!')), 100);

try {
  await sleep(1000, ac.signal);
} catch (err) {
  console.log(err.message); // "Cancelled early!"
}
```

### Example 3: Combining User Cancel with Timeout via `AbortSignal.any()`
```javascript
async function downloadLargeFile(url, userSignal) {
  // Hard limit 30-second timeout combined with user abort
  const timeoutSignal = AbortSignal.timeout(30_000);
  const combinedSignal = AbortSignal.any([userSignal, timeoutSignal]);

  const response = await fetch(url, { signal: combinedSignal });
  if (!response.ok) throw new Error(`HTTP ${response.status}`);

  const reader = response.body.getReader();
  const chunks = [];

  try {
    while (true) {
      combinedSignal.throwIfAborted();
      const { done, value } = await reader.read();
      if (done) break;
      chunks.push(value);
    }
    return chunks;
  } finally {
    reader.releaseLock();
  }
}
```

### Example 4: Cancellable Node.js File Stream Copy
```javascript
import fs from 'node:fs';
import { pipeline } from 'node:stream/promises';

async function copyFileWithAbort(sourcePath, destPath, signal) {
  const readStream = fs.createReadStream(sourcePath);
  const writeStream = fs.createWriteStream(destPath);

  try {
    await pipeline(readStream, writeStream, { signal });
    console.log('Copy succeeded');
  } catch (err) {
    if (signal.aborted) {
      console.error('File copy aborted. Removing partial file...');
      await fs.promises.unlink(destPath).catch(() => {});
    }
    throw err;
  }
}
```

### Example 5: Custom Long-Running Background Task
```javascript
async function processBatchItems(items, signal, onItemProcessed) {
  for (let i = 0; i < items.length; i++) {
    // Check abort status before starting every item
    signal.throwIfAborted();

    await processSingleItem(items[i]);
    onItemProcessed(i + 1, items.length);

    // Yield control to event loop to allow incoming abort events to be received
    await new Promise(resolve => setTimeout(resolve, 0));
  }
}

async function processSingleItem(item) {
  // Heavy computation or remote update
  return new Promise(r => setTimeout(r, 50));
}
```

---

## 6. Edge Cases, Pitfalls & Gotchas

1. **Memory Leaks with Event Listeners**: Attaching `signal.addEventListener('abort', ...)` without removing it or without `{ once: true }` retains references in memory if the signal outlives the short-lived asynchronous task. Always clean up listeners.
2. **Synchronous Execution of Listeners**: When calling `controller.abort()`, all attached listeners execute immediately on the same call stack before the next line of JavaScript runs.
3. **Double Abort Is Idempotent**: Calling `controller.abort()` multiple times only honors the first reason passed; subsequent calls are silent no-ops.
4. **Already Aborted Signals**: Always check `if (signal.aborted)` at the start of your function. If the signal was already aborted before invocation, listeners added afterward will never fire.

---

## 7. Common Mistakes & Anti-Patterns

| Mistake | Incorrect Pattern | Correct Solution | Why It Fails |
| :--- | :--- | :--- | :--- |
| **Leaking Listeners** | `signal.addEventListener('abort', cleanup)` without teardown | Use `{ once: true }` or `removeEventListener` in `finally` | Prevents garbage collection of async closures. |
| **Assuming Async Abort** | Expecting code after `abort()` to run before handlers | Treat `controller.abort()` as a synchronous dispatch | Causes unexpected race conditions if code assumes deferred cancellation. |
| **Swallowing Abort Errors** | `catch (e) { return null; }` | Rethrow if `e.name === 'AbortError'` | Hides intentional cancellations from parent orchestrators. |
| **Ignoring Pre-aborted Signals** | Only listening for `'abort'` event | Check `signal.throwIfAborted()` immediately | If signal is already aborted on entry, listener never triggers. |
| **Passing Controller Instead of Signal** | Passing entire `AbortController` into child worker | Pass only `controller.signal` | Violates encapsulation; child could inadvertently trigger abort. |
| **Manual Flag Hacks** | `let isCancelled = false` | Use standard `AbortController` and `AbortSignal` | Incompatible with native APIs (`fetch`, `streams`, `fs`). |

---

## 8. Performance Characteristics

- **Zero-Allocation when Unused**: Modern engines optimize `AbortSignal` instances such that no event queues or internal memory structures are allocated until an event listener is explicitly attached or the signal is passed across a C++ host boundary.
- **Microsecond Signal Dispatch**: Synchronous listener iteration across an `AbortSignal` takes $< 2\mu\text{s}$ for typical listener counts ($< 10$).
- **`AbortSignal.any()` Resource Chaining**: `AbortSignal.any()` internally binds native listeners to all child signals. When one fires, it automatically unregisters itself from all remaining signals to prevent memory leaks.

---

## 9. Engine Internals

In V8 and Chromium/Node C++ layers:
1. **EventTarget Hierarchy**: `AbortSignal` inherits from the native C++ `EventTarget` interface.
2. **DOMException Mapping**: When `abort()` is invoked without arguments, V8 allocates a native `DOMException` with code `ABORT_ERR` (name `"AbortError"`).
3. **Signal Teardown in Host APIs**: When `fetch()` receives an `AbortSignal`, the browser networking process binds a C++ callback to the signal's native observer. When signaled, the HTTP network socket receives an immediate TCP RST/FIN frame, tearing down downstream socket buffers without waiting for application JavaScript microtasks.

---

## 10. Best Practices & Design Guidelines

1. **Pass `signal` as an optional parameter**: All async utility functions should accept `{ signal }` in their options object.
2. **Use `signal.throwIfAborted()`**: Place `signal.throwIfAborted()` before expensive computation checkpoints.
3. **Distinguish Timeout vs User Abort**: Inspect `error.name === 'TimeoutError'` vs `error.name === 'AbortError'` to provide accurate UI feedback.
4. **Always clean up in `finally`**: Ensure network locks, timers, and partial disk files are cleared regardless of whether completion was normal or aborted.

---

## 11. Real-World Case Study

### Resilient Autocomplete Search with Debounce and Request Preemption

An e-commerce search bar triggers live API requests as the user types. Stale, slow responses from earlier keystrokes must be aborted immediately when new keystrokes occur to eliminate network waste and race conditions.

```javascript
class AutocompleteSearch {
  #currentController = null;
  #endpoint;

  constructor(endpoint) {
    this.#endpoint = endpoint;
  }

  async search(query) {
    // 1. Preempt any ongoing in-flight request
    if (this.#currentController) {
      this.#currentController.abort(new DOMException('New keystroke entered', 'PreemptedError'));
    }

    // 2. Create fresh controller for current query
    this.#currentController = new AbortController();
    const { signal } = this.#currentController;

    try {
      // 3. Combine with a strict 3-second network timeout
      const combinedSignal = AbortSignal.any([
        signal,
        AbortSignal.timeout(3000),
      ]);

      const res = await fetch(`${this.#endpoint}?q=${encodeURIComponent(query)}`, {
        signal: combinedSignal,
      });

      if (!res.ok) throw new Error(`HTTP Error: ${res.status}`);
      const data = await res.json();
      return data.results;
    } catch (err) {
      if (err.name === 'PreemptedError' || err.name === 'AbortError') {
        console.info(`Search for "${query}" preempted by newer request.`);
        return null; // Gracefully ignore aborted queries
      }
      if (err.name === 'TimeoutError') {
        console.warn(`Search for "${query}" timed out.`);
        throw new Error('Search server is currently unresponsive.');
      }
      throw err;
    } finally {
      if (this.#currentController?.signal === signal) {
        this.#currentController = null;
      }
    }
  }
}
```

---

## 12. Interview Questions

<details>
<summary><strong>1. What is the difference between AbortController and AbortSignal?</strong></summary>

`AbortController` represents the controller/producer side capable of invoking `.abort(reason)`. `AbortSignal` is the read-only consumer side passed to tasks to monitor cancellation status via `.aborted`, `.reason`, and the `'abort'` event.
</details>

<details>
<summary><strong>2. Is <code>controller.abort()</code> synchronous or asynchronous?</strong></summary>

It is **synchronous**. All event listeners registered on `signal.addEventListener('abort', ...)` are executed immediately on the current call stack before `controller.abort()` returns.
</details>

<details>
<summary><strong>3. How does <code>AbortSignal.timeout(ms)</code> differ from <code>setTimeout</code>?</strong></summary>

`AbortSignal.timeout(ms)` produces a native `AbortSignal` that aborts automatically with a `TimeoutError` after `ms` milliseconds. Unlike manual `setTimeout`, it integrates directly into standard APIs (`fetch`, streams) and does not keep the Node.js event loop active if unreferenced.
</details>

<details>
<summary><strong>4. What does <code>AbortSignal.any()</code> do?</strong></summary>

It takes an iterable of `AbortSignal` objects and returns a new signal that aborts as soon as **any** of the source signals aborts. The resulting signal's reason matches the reason of whichever source signal aborted first.
</details>

<details>
<summary><strong>5. What error is thrown when a fetch request is cancelled?</strong></summary>

By default, it throws a `DOMException` with the name `"AbortError"`. If aborted via `AbortSignal.timeout()`, it throws a `DOMException` with the name `"TimeoutError"`.
</details>

<details>
<summary><strong>6. Why is it important to pass <code>{ once: true }</code> when listening to <code>abort</code>?</strong></summary>

Because an abort event fires at most once in a signal's lifecycle. Using `{ once: true }` guarantees that the listener function is automatically detached from the signal, mitigating memory leak risks.
</details>

<details>
<summary><strong>7. What does <code>signal.throwIfAborted()</code> do?</strong></summary>

It checks if `signal.aborted` is true; if so, it immediately throws `signal.reason`. If false, it does nothing.
</details>

<details>
<summary><strong>8. Can an aborted <code>AbortController</code> be reset or reused?</strong></summary>

No. Once an `AbortController` has aborted, its signal remains permanently in the aborted state. To perform a new cancellable operation, you must instantiate a new `AbortController`.
</details>

---

## 13. Exercises & Challenges

### Challenge 1: Cancellable Promise Wrapper
Create a utility function `withAbort(promise, signal)` that wraps any standard promise and rejects immediately if the provided `AbortSignal` aborts.

<details>
<summary>View Solution</summary>

```javascript
function withAbort(promise, signal) {
  if (!signal) return promise;
  if (signal.aborted) return Promise.reject(signal.reason);

  return new Promise((resolve, reject) => {
    const onAbort = () => reject(signal.reason);
    signal.addEventListener('abort', onAbort, { once: true });

    promise.then(
      val => {
        signal.removeEventListener('abort', onAbort);
        resolve(val);
      },
      err => {
        signal.removeEventListener('abort', onAbort);
        reject(err);
      }
    );
  });
}
```
</details>

### Challenge 2: Implement a Retry Mechanism with Abort Support
Write an async function `fetchWithRetry(url, options = {}, retries = 3, backoffMs = 500)` that retries failed network requests but aborts immediately without delay if `options.signal` triggers.

<details>
<summary>View Solution</summary>

```javascript
async function fetchWithRetry(url, options = {}, retries = 3, backoffMs = 500) {
  const { signal, ...fetchOpts } = options;

  for (let attempt = 0; attempt <= retries; attempt++) {
    signal?.throwIfAborted();

    try {
      const response = await fetch(url, { ...fetchOpts, signal });
      if (response.ok) return response;
      throw new Error(`HTTP ${response.status}`);
    } catch (err) {
      if (signal?.aborted || attempt === retries) throw err;

      const delay = backoffMs * (2 ** attempt);
      await new Promise((resolve, reject) => {
        const timer = setTimeout(resolve, delay);
        signal?.addEventListener('abort', () => {
          clearTimeout(timer);
          reject(signal.reason);
        }, { once: true });
      });
    }
  }
}
```
</details>
