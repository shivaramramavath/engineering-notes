# Module 12: Asynchronous Programming and Iteration in TypeScript

Welcome to **Module 12: Asynchronous Programming and Iteration in TypeScript**. This module provides a complete, modern reference for managing asynchronous concurrency, event loop mechanics, streaming iterations, deterministic resource cleanup, and cancellation tokens.

---

## Module Overview

| Note | Title | Level | Core Topics |
| :--- | :--- | :--- | :--- |
| **00** | [Event Loop and Microtask Mechanics](./00-event-loop.md) | Intermediate | Microtasks vs macrotasks, starvation, timer typings (`NodeJS.Timeout` vs `number`), tick scheduling |
| **01** | [Promises and Combinators](./01-promises.md) | Intermediate | `Promise<T>`, executor typing, `Promise.all`, `Promise.allSettled`, `Promise.race`, `Promise.any` |
| **02** | [Async/Await and Awaited](./02-async-await.md) | Intermediate | `async/await`, `Awaited<T>` recursive unwrapping, top-level await, unhandled rejections |
| **03** | [Async Generics and Utilities](./03-async-generics.md) | Advanced | Generic higher-order async functions, in-flight deduplication, semaphores, batched queues |
| **04** | [Iterators and Generators](./04-iterators-and-generators.md) | Advanced | `Generator<TYield, TReturn, TNext>`, `AsyncIterable<T>`, `for-await-of`, coroutine state machines |
| **05** | [Concurrency Patterns and Workers](./05-concurrency-patterns.md) | Advanced | Worker threads, message protocols, structured clone, thread pools, `SharedArrayBuffer`, `Atomics` |
| **06** | [Explicit Resource Management](./06-explicit-resource-management-and-using.md) | Advanced | `using`, `await using`, `Disposable`, `AsyncDisposable`, `Symbol.dispose`, `DisposableStack` |
| **07** | [AbortSignal and Cancellation](./07-abortsignal-and-cancellation-tokens.md) | Advanced | `AbortController`, `AbortSignal.timeout`, `AbortSignal.any`, `throwIfAborted`, cooperative cancellation |

---

## Learning Path

```
                   +----------------------------------+
                   | 00-event-loop                    |
                   | (Call stack, microtasks, timers) |
                   +----------------+-----------------+
                                    |
                                    v
                   +----------------------------------+
                   | 01-promises                      |
                   | (Promise<T>, combinators)        |
                   +----------------+-----------------+
                                    |
                                    v
                   +----------------------------------+
                   | 02-async-await                   |
                   | (Awaited<T>, top-level await)    |
                   +----------------+-----------------+
                                    |
               +--------------------+--------------------+
               |                                         |
               v                                         v
+----------------------------------+   +----------------------------------+
| 03-async-generics                |   | 04-iterators-and-generators      |
| (Deduplication, semaphores)      |   | (Async generators, streams)      |
+----------------+-----------------+   +-----------------+----------------+
                 |                                       |
                 +-------------------+-------------------+
                                     |
                                     v
                   +----------------------------------+
                   | 05-concurrency-patterns          |
                   | (Worker threads, thread pools)   |
                   +-----------------+-----------------+
                                     |
               +---------------------+---------------------+
               |                                           |
               v                                           v
+------------------------------------+   +------------------------------------+
| 06-explicit-resource-management    |   | 07-abortsignal-and-cancellation    |
| (using, await using, disposables)  |   | (AbortController, signals, timeout)|
+------------------------------------+   +------------------------------------+
```

---

## Key Best Practices
1. **Never Float Promises**: Always `await` or attach `.catch()` to avoid unhandled rejections.
2. **Deduplicate Concurrent Requests**: Cache in-flight promises to eliminate cache stampedes on backend services.
3. **Use Explicit Resource Management**: Adopt `using` and `await using` (TS 5.2+) for deterministic cleanup of handles and connections.
4. **Cooperative Cancellation**: Pass `AbortSignal` through all asynchronous API layers and call `signal.throwIfAborted()` periodically.
5. **Offload Heavy CPU Work**: Never execute long synchronous algorithms on the main event loop thread; offload to a typed `WorkerPool`.
