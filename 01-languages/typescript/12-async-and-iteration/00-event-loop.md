# Event Loop and Microtask Mechanics in TypeScript

> In-depth exploration of the JavaScript and Node.js event loops, microtasks vs macrotasks, execution order, starvation pitfalls, and type-safe timers and event emitters in TypeScript.

| Metadata | Details |
| :--- | :--- |
| **Category** | Async & Iteration |
| **Difficulty** | Intermediate |
| **Prerequisites** | [01-fundamentals/00-types-and-inference.md](../01-fundamentals/00-types-and-inference.md), [02-functions/02-callbacks.md](../02-functions/02-callbacks.md) |
| **TypeScript Baseline** | TS 4.5+ (`lib: ["ES2022", "DOM"]`), Node 18+ |
| **Tested tsconfig** | `"strict": true, "target": "ES2022", "module": "NodeNext"` |

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
JavaScript runtimes (V8, JavaScriptCore) are single-threaded with a non-blocking I/O event-driven concurrency model. Understanding the mechanics of the event loop—specifically the distinction between the Call Stack, the Microtask Queue (`Promise.then`, `queueMicrotask`, `process.nextTick`), and the Macrotask Queue (`setTimeout`, `setImmediate`, I/O, UI rendering)—is critical for writing correct concurrent TypeScript code.

Without mastering event loop mechanics, developers introduce race conditions, accidental event-loop blocking (starvation), and subtle timing bugs that TypeScript's static type checker cannot catch at compile time.

---

## 2. Core Concepts
- **Call Stack**: Synchronous frame execution following LIFO (Last-In, First-Out).
- **Microtask Queue (Jobs)**: High-priority tasks executed immediately after the current call stack clears, before any macrotasks or UI rendering:
  - `Promise.prototype.then()` / `catch()` / `finally()`
  - `queueMicrotask()`
  - `process.nextTick()` (Node.js specific, runs before standard microtasks)
- **Macrotask Queue (Tasks)**: Standard scheduled operations executed on subsequent turns of the event loop:
  - `setTimeout()` / `setInterval()`
  - `setImmediate()` (Node.js specific)
  - DOM events, Network I/O callbacks
- **Timer Type Ambiguity (`NodeJS.Timeout` vs `number`)**: In Node.js, `setTimeout()` returns a `NodeJS.Timeout` object (which has `.ref()` and `.unref()`). In browser environments, `setTimeout()` returns a `number`. TypeScript projects targeting both environments often encounter type conflicts.
- **Event Loop Starvation**: Continually enqueuing microtasks prevents the event loop from reaching the macrotask queue or I/O polling, causing application freezes.

---

## 3. How It Works Internally

```
                      +-------------------+
                      |    Call Stack     |
                      | (Synchronous code)|
                      +---------+---------+
                                |
             Stack Empty? ------+
                                |
                                v
               +---------------------------------+
               |      Microtask Queue (Jobs)     |
               |  - process.nextTick             |
               |  - Promise.then / queueMicrotask|
               +----------------+----------------+
                                |
        Drain ALL microtasks until queue is empty!
                                |
                                v
               +---------------------------------+
               |         Render Phase            |
               | (Browsers: requestAnimationFrame|
               |  style, layout, paint)          |
               +----------------+----------------+
                                |
                                v
               +---------------------------------+
               |       Macrotask Queue (Tasks)   |
               |  - setTimeout / setInterval     |
               |  - setImmediate (Node)          |
               |  - I/O polling callbacks        |
               +----------------+----------------+
                                |
            Run ONE macrotask, then check microtasks!
```

---

## 4. Syntax & API Reference

```typescript
// 1. Enqueueing microtasks
queueMicrotask(() => {
  // Executes immediately after current synchronous turn
});

// 2. Cross-environment timer handle type
type TimerHandle = ReturnType<typeof setTimeout>;

// 3. Node.js process.nextTick vs standard microtask
if (typeof process !== 'undefined' && typeof process.nextTick === 'function') {
  process.nextTick(() => {
    // Priority microtask in Node.js
  });
}

// 4. Yielding execution back to the event loop
export const yieldToEventLoop = (): Promise<void> =>
  new Promise(resolve => setTimeout(resolve, 0));
```

---

## 5. Practical Examples

### Example 1: Execution Order Verification
```typescript
function traceExecutionOrder(): string[] {
  const log: string[] = [];

  log.push('1: Sync Start');

  setTimeout(() => {
    log.push('5: Macrotask (setTimeout)');
  }, 0);

  Promise.resolve().then(() => {
    log.push('3: Microtask (Promise.then)');
  });

  queueMicrotask(() => {
    log.push('4: Microtask (queueMicrotask)');
  });

  log.push('2: Sync End');

  return log;
}
// Sequence: 1 -> 2 -> 3 -> 4 -> 5
```

### Example 2: Chunked Heavy Computation (Preventing Starvation)
```typescript
async function processLargeBatch<T, R>(
  items: ReadonlyArray<T>,
  transform: (item: T) => R,
  chunkSize = 1000
): Promise<R[]> {
  const results: R[] = [];

  for (let i = 0; i < items.length; i++) {
    results.push(transform(items[i]));

    // Yield control every chunkSize items to allow I/O and timers to run
    if (i > 0 && i % chunkSize === 0) {
      await new Promise(resolve => setTimeout(resolve, 0));
    }
  }

  return results;
}
```

### Example 3: Cross-Environment Timer Abstraction
```typescript
class Heartbeat {
  private timerId: ReturnType<typeof setTimeout> | null = null;

  start(intervalMs: number, onTick: () => void): void {
    this.stop();
    this.timerId = setTimeout(() => {
      onTick();
      this.start(intervalMs, onTick);
    }, intervalMs);

    // If running in Node.js, unref to prevent hanging test runners or processes
    if (this.timerId && typeof (this.timerId as unknown as { unref?: () => void }).unref === 'function') {
      (this.timerId as unknown as { unref: () => void }).unref();
    }
  }

  stop(): void {
    if (this.timerId !== null) {
      clearTimeout(this.timerId);
      this.timerId = null;
    }
  }
}
```

### Example 4: Compiler Failure Case - Type Mismatch on Timer ID
```typescript
// Compiling with @types/node present:
const handle: number = setTimeout(() => {}, 100);
// @ts-expect-error TS2322: Type 'Timeout' is not assignable to type 'number'.
const n: number = handle;

// Correct cross-platform idiom:
const safeHandle: ReturnType<typeof setTimeout> = setTimeout(() => {}, 100);
```

### Example 5: Idiomatic Next-Tick State Flusher
```typescript
export class MicrotaskBatcher<T> {
  private queue: T[] = [];
  private scheduled = false;

  constructor(private readonly flushFn: (batch: ReadonlyArray<T>) => void) {}

  add(item: T): void {
    this.queue.push(item);
    if (!this.scheduled) {
      this.scheduled = true;
      queueMicrotask(() => this.flush());
    }
  }

  private flush(): void {
    this.scheduled = false;
    const batch = this.queue;
    this.queue = [];
    if (batch.length > 0) {
      this.flushFn(batch);
    }
  }
}
```

---

## 6. Edge Cases, Pitfalls & Gotchas
1. **Recursive Microtask Starvation**: Calling `queueMicrotask` or resolving promises recursively in a loop starves the event loop. The runtime never yields to macrotasks, I/O, or rendering, freezing the process completely.
2. **`ReturnType<typeof setTimeout>` in Monorepos**: In fullstack TypeScript projects importing `@types/node`, `setTimeout` globally returns `NodeJS.Timeout`. In browser-only files, this causes type incompatibilities unless `ReturnType<typeof setTimeout>` or `window.setTimeout` is used.
3. **`async` Function Execution**: The synchronous body of an `async` function executes synchronously until the first `await` statement is reached. Only code after `await` is deferred as a microtask.
4. **Node.js `process.nextTick` Priority**: `nextTick` queues run before the standard microtask queue. Excessive `nextTick` usage starves promise resolution.

---

## 7. Common Mistakes & Anti-Patterns

| Anti-Pattern | Consequence | Correction |
| :--- | :--- | :--- |
| `const id: number = setTimeout(...)` | **TS2322**: Fails in Node.js environment | Use `ReturnType<typeof setTimeout>` |
| Recursive `queueMicrotask` loop | Event loop starvation; process freeze | Use `setTimeout(..., 0)` or `setImmediate` |
| Long-running synchronous loops | Blocks entire Node thread / UI | Chunk work and yield with `await setTimeout(0)` |
| Assuming `setTimeout(fn, 0)` runs immediately | Microtasks run first; minimum delay is ~1-4ms | Understand task vs microtask precedence |
| Using `process.nextTick` in universal code | Throws `ReferenceError` in browsers | Use `queueMicrotask` for cross-platform code |

---

## 8. Performance & Type-Checker Complexity
- `queueMicrotask` is substantially lighter than `setTimeout(fn, 0)`. While `setTimeout` requires registering a timer in the runtime timer heap ($\mathcal{O}(\log N)$), `queueMicrotask` simply appends a function pointer to a contiguous array ($\mathcal{O}(1)$).
- TypeScript typing for `setTimeout` is overloaded in `lib.dom.d.ts` and `@types/node/timers.d.ts`. Resolving the return type costs minimal overhead in the type checker.

---

## 9. Compiler Internals
TypeScript defines globals like `setTimeout` and `queueMicrotask` in `lib.dom.d.ts` and `lib.es2020.promise.d.ts`:
```typescript
declare function queueMicrotask(callback: VoidFunction): void;
declare function setTimeout(handler: TimerHandler, timeout?: number, ...arguments: any[]): number;
```
When `@types/node` is added, declaration merging augments the global scope, redefining `setTimeout` to return `NodeJS.Timeout`.

---

## 10. Best Practices & Design Guidelines
- Prefer `queueMicrotask()` over `Promise.resolve().then(...)` for scheduling microtasks.
- Always use `ReturnType<typeof setTimeout>` for timer references.
- Chunk CPU-intensive algorithms into slices and yield to the event loop using `await yieldToEventLoop()`.
- Avoid `process.nextTick()` unless specifically writing low-level Node.js I/O stream handlers.
- In tests, use fake timers (`vitest.useFakeTimers()`) to deterministically advance macrotask and microtask queues.

---

## 11. Real-World Case Study
A financial logging client buffered analytics events and flushed them via HTTP. Developers scheduled flushes using `setTimeout(flush, 0)` on each event. Under high burst volume (10,000 events/sec), the timer heap grew uncontrollably, causing 100% CPU spikes and latency degradation.

**Refactored with Microtask Coalescing:**
```typescript
export class TelemetryBatcher {
  private buffer: string[] = [];
  private isFlushScheduled = false;

  record(event: string): void {
    this.buffer.push(event);
    if (!this.isFlushScheduled) {
      this.isFlushScheduled = true;
      // Microtask coalesces all synchronous event records in the same tick!
      queueMicrotask(() => this.flush());
    }
  }

  private flush(): void {
    this.isFlushScheduled = false;
    const batch = this.buffer;
    this.buffer = [];
    sendTelemetryPayload(batch);
  }
}
function sendTelemetryPayload(_events: string[]) { /* ... */ }
```
Timer heap churn dropped to zero, and events emitted within the same tick were automatically batched into a single transmission.

---

## 12. Interview Questions

<details>
<summary>1. What is the fundamental difference between microtasks and macrotasks?</summary>

Microtasks (promises, `queueMicrotask`, `process.nextTick`) are executed immediately after the currently running script completes, draining the microtask queue completely before the runtime moves on. Macrotasks (`setTimeout`, `setInterval`, `setImmediate`, I/O) are queued for subsequent iterations of the event loop, running at most one task before checking microtasks again.
</details>

<details>
<summary>2. Why does `setTimeout` return a `number` in browsers but an object in Node.js?</summary>

In browsers, the DOM specification defines timers to return an integer ID. In Node.js, `setTimeout` returns a `NodeJS.Timeout` class instance that provides methods like `.ref()` and `.unref()` to control whether the active timer keeps the Node event loop alive.
</details>

<details>
<summary>3. What happens if a microtask enqueues another microtask indefinitely?</summary>

The microtask queue will never empty, which starves the event loop. Macrotasks (like timers), I/O events, and browser rendering will never execute, effectively freezing the application.
</details>

<details>
<summary>4. When does the synchronous portion of an `async` function execute?</summary>

It executes immediately and synchronously up until the expression on the right-hand side of the first `await` is evaluated. Only the resumption of the function is scheduled as a microtask.
</details>

<details>
<summary>5. How should you type a variable that stores a timer ID in a cross-platform codebase?</summary>

Use `ReturnType<typeof setTimeout>`:
```typescript
let timerId: ReturnType<typeof setTimeout> | undefined;
```
</details>

<details>
<summary>6. In what order do `process.nextTick`, `Promise.then`, and `setTimeout(fn, 0)` execute?</summary>

1. Current synchronous script
2. `process.nextTick`
3. `Promise.then` (microtask)
4. `setTimeout(fn, 0)` (macrotask)
</details>

<details>
<summary>7. What does `unref()` do on a Node.js timer?</summary>

It instructs the Node.js event loop not to wait for this timer if it is the only remaining active handle in the event loop, allowing the process to exit naturally.
</details>

<details>
<summary>8. How can you break up a long synchronous loop without blocking user interactions or incoming network requests?</summary>

By periodically awaiting a macrotask yield (`new Promise(r => setTimeout(r, 0))`), giving the engine an opportunity to process queued I/O events and render frames.
</details>

---

## 13. Exercises & Challenges

### Easy
1. **Sleep Helper**: Implement a type-safe `sleep(ms: number): Promise<void>` helper.
   <details>
   <summary>Solution</summary>

   ```typescript
   export function sleep(ms: number): Promise<void> {
     return new Promise(resolve => setTimeout(resolve, ms));
   }
   ```
   </details>

2. **Cross-Platform Timer Handle**: Create a type alias `TimerId` that dynamically reflects the return type of `setTimeout` in whichever environment it compiles in.
   <details>
   <summary>Solution</summary>

   ```typescript
   export type TimerId = ReturnType<typeof setTimeout>;
   ```
   </details>

### Medium
3. **Microtask-Coalesced Debounce**: Implement a function `debounceMicrotask<Args extends unknown[]>(fn: (...args: Args) => void): (...args: Args) => void` that coalesces multiple synchronous invocations into a single execution scheduled on the microtask queue with the latest arguments.
   <details>
   <summary>Solution</summary>

   ```typescript
   export function debounceMicrotask<Args extends unknown[]>(
     fn: (...args: Args) => void
   ): (...args: Args) => void {
     let lastArgs: Args | null = null;
     let scheduled = false;

     return (...args: Args) => {
       lastArgs = args;
       if (!scheduled) {
         scheduled = true;
         queueMicrotask(() => {
           scheduled = false;
           if (lastArgs) {
             fn(...lastArgs);
             lastArgs = null;
           }
         });
       }
     };
   }
   ```
   </details>

4. **Event Loop Lag Detector**: Write a class `EventLoopMonitor` that samples event loop delay every 100ms using `setTimeout` and emits a warning if latency exceeds 50ms.
   <details>
   <summary>Solution</summary>

   ```typescript
   export class EventLoopMonitor {
     private timer: ReturnType<typeof setInterval> | null = null;

     start(thresholdMs = 50): void {
       let expected = Date.now() + 100;
       this.timer = setInterval(() => {
         const now = Date.now();
         const lag = now - expected;
         if (lag > thresholdMs) {
           console.warn(`[EVENT LOOP LAG] Lag detected: ${lag}ms`);
         }
         expected = Date.now() + 100;
       }, 100);
     }

     stop(): void {
       if (this.timer) clearInterval(this.timer);
     }
   }
   ```
   </details>

### Hard
5. **Fair Task Scheduler with Time-Slicing**: Implement a cooperative task scheduler `scheduleWork<T>(tasks: (() => T)[], maxTimePerSliceMs = 16): Promise<T[]>` that processes tasks sequentially, checking `performance.now()` after each task, and yielding to the macrotask queue whenever the time slice is exceeded.
   <details>
   <summary>Solution</summary>

   ```typescript
   export async function scheduleWork<T>(
     tasks: (() => T)[],
     maxTimePerSliceMs = 16
   ): Promise<T[]> {
     const results: T[] = [];
     let sliceStart = performance.now();

     for (let i = 0; i < tasks.length; i++) {
       results.push(tasks[i]());

       if (performance.now() - sliceStart > maxTimePerSliceMs) {
         await new Promise(resolve => setTimeout(resolve, 0));
         sliceStart = performance.now();
       }
     }

     return results;
   }
   ```
   </details>

6. **Priority-Aware Dual Queue Executor**: Implement an executor class with two methods: `enqueueNormal(task: () => Promise<void>)` and `enqueueUrgent(task: () => Promise<void>)` where urgent tasks run on the microtask queue and normal tasks yield to the macrotask queue, with full type safety.
   <details>
   <summary>Solution</summary>

   ```typescript
   export class PriorityExecutor {
     private isRunning = false;

     enqueueUrgent(task: () => Promise<void>): void {
       queueMicrotask(async () => {
         try {
           await task();
         } catch (err) {
           console.error('Urgent task failed:', err);
         }
       });
     }

     enqueueNormal(task: () => Promise<void>): void {
       setTimeout(async () => {
         try {
           await task();
         } catch (err) {
           console.error('Normal task failed:', err);
         }
       }, 0);
     }
   }
   ```
   </details>
