# AbortSignal and Cancellation Tokens in TypeScript

> Comprehensive guide to cooperative cancellation in TypeScript: `AbortController`, `AbortSignal`, timeout cancellation (`AbortSignal.timeout`), composition (`AbortSignal.any`), `signal.throwIfAborted()`, and propagation through async hierarchies.

| Metadata | Details |
| :--- | :--- |
| **Category** | Async & Iteration |
| **Difficulty** | Advanced |
| **Prerequisites** | [12-async-and-iteration/01-promises.md](./01-promises.md), [12-async-and-iteration/02-async-await.md](./02-async-await.md) |
| **TypeScript Baseline** | TS 4.5+, TS 5.2+ (`AbortSignal.any`) |
| **Tested tsconfig** | `"strict": true, "target": "ES2022", "lib": ["ES2022", "DOM"]` |

---

## Table of Contents
1. [What & Why](#1-what--why)
2. [Core Concepts](#2-core-concepts)
3. [How It Works Internally](#3-how-it-works-internally)
4. [Syntax & API Reference](#4-syntax--api-reference)
5. [Practical Examples](#5-practical-examples)
6. [Edge Cases, Pitfalls & Gotchas](#6-edge-cases-pitfalls--gotchas)
7. [Common Mistakes & Anti-Patterns](#7-common-mistakes--anti-patterns)
8. [Performance & Type-Checker Complexity](#8-performance--type-checker-complexity)
9. [Compiler Internals](#9-compiler-internals)
10. [Best Practices & Design Guidelines](#10-best-practices--design-guidelines)
11. [Real-World Case Study](#11-real-world-case-study)
12. [Interview Questions](#12-interview-questions)
13. [Exercises & Challenges](#13-exercises--challenges)

---

## 1. What & Why
JavaScript promises are fundamentally **non-cancelable**: once initiated, a promise runs to completion regardless of whether the caller still needs the result. In web applications and backend microservices, abandoning obsolete in-flight requests (such as when a user navigates away or when a downstream request times out) wastes network bandwidth, database connections, and server memory.

`AbortController` and `AbortSignal` represent the official Web and Node.js standard for **cooperative cancellation**. TypeScript provides comprehensive typing for cancellation signals, reason objects, and modern static utilities like `AbortSignal.timeout()` and `AbortSignal.any()`.

---

## 2. Core Concepts
- **`AbortController`**: The controller object that owns the cancellation capability via `controller.abort(reason)`.
- **`AbortSignal`**: The read-only signal passed to asynchronous operations (`signal.aborted`, `signal.reason`, and `'abort'` event listener).
- **`signal.throwIfAborted()`**: Standard method that immediately throws `signal.reason` (or `DOMException('Aborted', 'AbortError')`) if the signal has already been aborted.
- **`AbortSignal.timeout(ms)`**: Factory method creating an `AbortSignal` that automatically fires after a specified millisecond duration with a `TimeoutError`.
- **`AbortSignal.any([signal1, signal2])`**: Composes multiple signals into a single unified signal that aborts when *any* of the child signals aborts.
- **Cooperative Cancellation**: Cancellation is cooperative: asynchronous tasks must actively inspect the signal or pass it to standard APIs (like `fetch` or Node `fs/promises`).

---

## 3. How It Works Internally

```
+-------------------------------------------------------------+
|                      AbortController                        |
|                  controller.abort(reason)                   |
+------------------------------+------------------------------+
                               |
                               v
+-------------------------------------------------------------+
|                        AbortSignal                          |
|  - signal.aborted = true                                    |
|  - signal.reason = reason                                   |
|  - emits 'abort' event                                      |
+------------------------------+------------------------------+
                               |
            +------------------+------------------+
            |                                     |
    signal.addEventListener('abort')      signal.throwIfAborted()
            |                                     |
            v                                     v
   Tears down open socket /               Throws signal.reason
   cancels ongoing fetch()                immediately
```

When `controller.abort()` is invoked, the controller sets the internal `[[AbortedFlag]]` to `true`, stores the reason object, and synchronously dispatches the `abort` event to all registered listeners.

---

## 4. Syntax & API Reference

```typescript
// 1. Basic usage with fetch
const controller = new AbortController();
const response = await fetch('https://api.example.com/data', {
  signal: controller.signal,
});

// Cancel the request
controller.abort(new Error('User cancelled navigation'));

// 2. Combining with timeout (TS 5.2+ / Node 18+)
const timeoutSignal = AbortSignal.timeout(5000);

// 3. Composing signals (AbortSignal.any)
const userSignal = controller.signal;
const combinedSignal = AbortSignal.any([timeoutSignal, userSignal]);

// 4. Checking status inside loop
function computeWithSignal(signal: AbortSignal): void {
  for (let i = 0; i < 1_000_000; i++) {
    signal.throwIfAborted(); // Throws if cancelled!
  }
}
```

---

## 5. Practical Examples

### Example 1: Cancellable Sleep Function
```typescript
export function cancellableSleep(ms: number, signal?: AbortSignal): Promise<void> {
  return new Promise((resolve, reject) => {
    if (signal?.aborted) {
      reject(signal.reason ?? new Error('Aborted'));
      return;
    }

    const timer = setTimeout(() => {
      resolve();
      cleanup();
    }, ms);

    const onAbort = () => {
      clearTimeout(timer);
      reject(signal?.reason ?? new Error('Aborted'));
      cleanup();
    };

    const cleanup = () => {
      signal?.removeEventListener('abort', onAbort);
    };

    signal?.addEventListener('abort', onAbort);
  });
}
```

### Example 2: Long-Running Generator with Periodic Cancellation Check
```typescript
async function* cancellableWorker(
  items: string[],
  signal: AbortSignal
): AsyncGenerator<string, void, unknown> {
  for (const item of items) {
    signal.throwIfAborted();
    // Simulate async operation
    await new Promise(r => setTimeout(r, 50));
    yield `Processed: ${item}`;
  }
}
```

### Example 3: Edge Case - Memory Leaks in Event Listeners
```typescript
// Always clean up 'abort' event listeners once an operation settles!
export async function safeCancellableTask<T>(
  action: () => Promise<T>,
  signal?: AbortSignal
): Promise<T> {
  if (signal?.aborted) throw signal.reason;

  let onAbort: () => void;
  const abortPromise = new Promise<never>((_, reject) => {
    onAbort = () => reject(signal!.reason);
    signal?.addEventListener('abort', onAbort, { once: true });
  });

  try {
    return await Promise.race([action(), abortPromise]);
  } finally {
    if (signal && onAbort!) {
      signal.removeEventListener('abort', onAbort);
    }
  }
}
```

### Example 4: Compiler Failure Case - Non-AbortSignal in fetch
```typescript
const fakeSignal = { aborted: true };

// @ts-expect-error TS2322: Type '{ aborted: boolean; }' is not assignable to type 'AbortSignal'.
fetch('https://api.example.com', { signal: fakeSignal });
```

### Example 5: Idiomatic Composite Cancellation Hierarchies
```typescript
export class OperationScope {
  private controller = new AbortController();

  constructor(parentSignal?: AbortSignal) {
    if (parentSignal) {
      parentSignal.addEventListener('abort', () => {
        this.controller.abort(parentSignal.reason);
      }, { once: true });
    }
  }

  get signal(): AbortSignal {
    return this.controller.signal;
  }

  cancel(reason = 'OperationScope cancelled'): void {
    this.controller.abort(new Error(reason));
  }
}
```

---

## 6. Edge Cases, Pitfalls & Gotchas
1. **Listener Memory Leaks**: Attaching an event listener to an `AbortSignal` without removing it (or omitting `{ once: true }`) retains the callback and any closed-over scope in memory until the controller itself is garbage collected.
2. **Synchronous Abortion Before Start**: If `signal.aborted` is already `true` when a function starts, immediately check `signal.throwIfAborted()`. Relying solely on `signal.addEventListener('abort')` will never fire because the abort event was already dispatched in the past.
3. **`signal.reason` Type**: In TypeScript standard definitions, `signal.reason` is `any`. In modern browsers, it defaults to a `DOMException` with name `'AbortError'`, but callers can pass arbitrary objects to `controller.abort(customReason)`.
4. **Unhandled Rejection on Race**: When using `Promise.race([task, abortPromise])`, if the task continues running and later rejects after abortion, it can cause an unhandled rejection unless handled with `.catch()`.

---

## 7. Common Mistakes & Anti-Patterns

| Anti-Pattern | Consequence | Correction |
| :--- | :--- | :--- |
| Forgetting to check `signal.aborted` upfront | Misses already-cancelled signals | Call `signal.throwIfAborted()` at entry |
| Not removing `abort` event listeners | Severe memory leak over long-lived signals | Remove listener in `finally` block or use `{ once: true }` |
| Using `AbortSignal.timeout` without clearing | Leaks timer handles in Node | Let garbage collection or clear handles safely |
| Swallowing abort errors silently | Callers assume task succeeded | Re-throw abort errors or return explicit result |
| Ignoring signal in heavy CPU loops | Loop continues wasting CPU | Call `signal.throwIfAborted()` periodically |

---

## 8. Performance & Type-Checker Complexity
- `AbortSignal` is an event target (`EventTarget`). Creating an `AbortController` allocates a small DOM/V8 object with minimal overhead.
- Checking `signal.aborted` is a single boolean property lookup ($\sim 0.5\text{ns}$). Periodically polling `signal.aborted` inside loops introduces near-zero CPU overhead.

---

## 9. Compiler Internals
In TypeScript's `lib.dom.d.ts`:
```typescript
interface AbortSignal extends EventTarget {
  readonly aborted: boolean;
  readonly reason: any;
  throwIfAborted(): void;
  onabort: ((this: AbortSignal, ev: Event) => any) | null;
}
interface AbortSignalConstructor {
  timeout(milliseconds: number): AbortSignal;
  any(signals: Iterable<AbortSignal>): AbortSignal;
}
```
TypeScript types the static `timeout` and `any` methods when targeting modern ECMAScript standard libraries.

---

## 10. Best Practices & Design Guidelines
- Always accept an optional `signal?: AbortSignal` as the last parameter in asynchronous APIs.
- Check `signal?.throwIfAborted()` at the start of functions and inside long loops.
- Pass signals directly to built-in asynchronous functions (`fetch`, `fs.promises.readFile`).
- Clean up all event listeners registered on external signals using `finally`.
- Differentiate between timeout cancellations and user-initiated cancellations using `error.name === 'TimeoutError'`.

---

## 11. Real-World Case Study
A real-time search auto-complete dropdown fired HTTP queries on every keystroke. Typing quickly resulted in race conditions: an earlier request for "app" completed *after* a later request for "apple", overwriting the search results with stale data.

**Refactored with AbortController:**
```typescript
export class AutocompleteClient {
  private activeController: AbortController | null = null;

  async search(query: string): Promise<string[]> {
    // Abort previous in-flight search!
    if (this.activeController) {
      this.activeController.abort('Stale query replaced');
    }

    this.activeController = new AbortController();
    const { signal } = this.activeController;

    try {
      const res = await fetch(`/api/search?q=${encodeURIComponent(query)}`, { signal });
      return (await res.json()) as string[];
    } catch (err: unknown) {
      if (err instanceof Error && err.name === 'AbortError') {
        return []; // Gracefully ignore aborted queries
      }
      throw err;
    }
  }
}
```
Network traffic was slashed, and search results were guaranteed to reflect only the latest keystroke.

---

## 12. Interview Questions

<details>
<summary>1. What is the purpose of `AbortSignal.throwIfAborted()`?</summary>

It checks if the signal has already been aborted, and if so, immediately throws `signal.reason`. It eliminates the need for manual `if (signal.aborted) throw signal.reason;` boilerplate.
</details>

<details>
<summary>2. What does `AbortSignal.any()` do?</summary>

Introduced in modern runtimes (and TS 5.2+), `AbortSignal.any(signals)` takes an iterable of `AbortSignal`s and returns a single signal that aborts as soon as *any* of the input signals aborts, carrying over the abort reason of the first signal that triggered.
</details>

<details>
<summary>3. Why must you clean up `'abort'` event listeners?</summary>

If an `AbortSignal` is long-lived (e.g. attached to an HTTP server request or global scope), adding event listeners attaches callbacks to that signal. If listeners are not removed when a short-lived child task finishes, the callback and its closed-over scope are leaked in memory.
</details>

<details>
<summary>4. What is the difference between `AbortSignal.timeout(ms)` and manual `setTimeout` with controller?</summary>

`AbortSignal.timeout(ms)` is natively implemented by the runtime, creates an `AbortSignal` directly without requiring manual `AbortController` allocation, and aborts with a specialized `TimeoutError` DOMException.
</details>

<details>
<summary>5. How does `fetch` handle an aborted `AbortSignal`?</summary>

`fetch` immediately halts network socket communication, aborts the HTTP request, and rejects the returned promise with a `DOMException` whose `name` is `'AbortError'`.
</details>

<details>
<summary>6. Can you cancel a promise synchronously from the outside without `AbortSignal`?</summary>

No. Standard JavaScript promises cannot be forcibly cancelled or aborted from the outside. Cancellation must be cooperative via an out-of-band communication channel like `AbortSignal`.
</details>

<details>
<summary>7. What happens if you call `controller.abort()` multiple times?</summary>

Only the first call has an effect. Subsequent calls are no-ops; the `abort` event is only emitted once and `signal.reason` preserves the first abort reason.
</details>

<details>
<summary>8. How do you distinguish between user cancellation and timeout?</summary>

By inspecting the error thrown: timeouts throw a `TimeoutError`, whereas user aborts typically throw an `AbortError` or custom reason passed to `controller.abort(reason)`.
</details>

---

## 13. Exercises & Challenges

### Easy
1. **Signal Checker**: Write a function `assertNotAborted(signal?: AbortSignal): void` that throws if the signal is aborted.
   <details>
   <summary>Solution</summary>

   ```typescript
   export function assertNotAborted(signal?: AbortSignal): void {
     signal?.throwIfAborted();
   }
   ```
   </details>

2. **Timeout Fetcher**: Write a wrapper `fetchWithTimeout(url: string, timeoutMs: number): Promise<Response>` using `AbortSignal.timeout`.
   <details>
   <summary>Solution</summary>

   ```typescript
   export function fetchWithTimeout(url: string, timeoutMs: number): Promise<Response> {
     return fetch(url, { signal: AbortSignal.timeout(timeoutMs) });
   }
   ```
   </details>

### Medium
3. **Cancellable Poll with Exponential Backoff**: Implement `pollUntil<T>(action: () => Promise<T | null>, condition: (val: T) => boolean, signal: AbortSignal): Promise<T>`.
   <details>
   <summary>Solution</summary>

   ```typescript
   export async function pollUntil<T>(
     action: () => Promise<T | null>,
     condition: (val: T) => boolean,
     signal: AbortSignal
   ): Promise<T> {
     let delay = 100;

     while (true) {
       signal.throwIfAborted();
       const result = await action();
       if (result !== null && condition(result)) {
         return result;
       }

       await new Promise<void>((resolve, reject) => {
         const timer = setTimeout(resolve, delay);
         signal.addEventListener('abort', () => {
           clearTimeout(timer);
           reject(signal.reason);
         }, { once: true });
       });

       delay = Math.min(delay * 2, 5000);
     }
   }
   ```
   </details>

4. **Signal Linking Utility**: Implement `linkSignals(parent: AbortSignal, child: AbortController): () => void` that aborts the child when the parent aborts, returning an unlink cleanup function.
   <details>
   <summary>Solution</summary>

   ```typescript
   export function linkSignals(parent: AbortSignal, child: AbortController): () => void {
     if (parent.aborted) {
       child.abort(parent.reason);
       return () => {};
     }

     const onAbort = () => child.abort(parent.reason);
     parent.addEventListener('abort', onAbort, { once: true });
     return () => parent.removeEventListener('abort', onAbort);
   }
   ```
   </details>

### Hard
5. **Cancellable Async Task Queue with Per-Task Signals**: Build a task queue that executes tasks sequentially; each task receives a child `AbortSignal` derived from a global queue controller AND a per-task timeout signal composed via `AbortSignal.any()`.
   <details>
   <summary>Solution</summary>

   ```typescript
   export class CancellableQueue {
     private globalController = new AbortController();

     cancelAll(reason = 'Queue cancelled'): void {
       this.globalController.abort(new Error(reason));
     }

     async runTask<T>(
       task: (signal: AbortSignal) => Promise<T>,
       timeoutMs = 10000
     ): Promise<T> {
       const timeoutSignal = AbortSignal.timeout(timeoutMs);
       const combinedSignal = AbortSignal.any([this.globalController.signal, timeoutSignal]);

       combinedSignal.throwIfAborted();
       return await task(combinedSignal);
     }
   }
   ```
   </details>

6. **Full-Featured Cancellation Token Source**: Implement a C#-style `CancellationTokenSource` class with `.token`, `.cancel()`, `.cancelAfter(ms)`, and child linked token sources, fully type-safe.
   <details>
   <summary>Solution</summary>

   ```typescript
   export class CancellationTokenSource {
     private controller = new AbortController();
     private timer: ReturnType<typeof setTimeout> | null = null;

     get token(): AbortSignal {
       return this.controller.signal;
     }

     cancel(reason?: unknown): void {
       if (this.timer) clearTimeout(this.timer);
       this.controller.abort(reason);
     }

     cancelAfter(ms: number): void {
       if (this.timer) clearTimeout(this.timer);
       this.timer = setTimeout(() => {
         this.controller.abort(new Error(`Cancelled after ${ms}ms`));
       }, ms);
     }

     static createLinkedTokenSource(...signals: AbortSignal[]): CancellationTokenSource {
       const cts = new CancellationTokenSource();
       const combined = AbortSignal.any(signals);
       if (combined.aborted) {
         cts.cancel(combined.reason);
       } else {
         combined.addEventListener('abort', () => cts.cancel(combined.reason), { once: true });
       }
       return cts;
     }
   }
   ```
   </details>
