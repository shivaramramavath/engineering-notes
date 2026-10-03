# 11 · Asynchronous JavaScript

JavaScript runs your code on a **single thread**, yet it handles network calls, timers, files and user input without freezing. It does this by starting work, **continuing**, and handling the result later. This chapter covers every tool for that: callbacks, promises, `async`/`await`, async iteration, cancellation, and the patterns built on them.

```
start work ──► keep running other code ──► result arrives ──► your continuation runs
```

## Reading order

| # | File | You will learn |
|---|------|----------------|
| 1 | [Sync vs Async](./01_sync-vs-async.md) | Blocking, the event loop at a glance, why async exists |
| 2 | [Callbacks](./02_callbacks.md) | Continuation-passing, error-first, callback hell, promisify |
| 3 | [Promises](./03_promises.md) | States, chaining, `then`/`catch`/`finally`, creating promises |
| 4 | [Promise Combinators](./04_promise-combinators.md) | `all`, `allSettled`, `race`, `any`, `withResolvers`, `try` |
| 5 | [async/await](./05_async-await.md) | Syntax, sequential vs parallel, loops, top-level `await` |
| 6 | [Async Error Handling](./06_async-error-handling.md) | `try/catch`, unhandled rejections, floating promises |
| 7 | [Async Iterators and Generators](./07_async-iterators-and-generators.md) | `for await...of`, async generators, streaming data |
| 8 | [Cancellation and Abort](./08_cancellation-and-abort.md) | `AbortController`, timeouts, "latest wins" |
| 9 | [Async Patterns](./09_async-patterns.md) | Retry, timeout, concurrency limits, queues, dedupe, polling |

## Quick comparison

| Style | Looks like | Best for |
|-------|-----------|----------|
| Callbacks | `fn(args, (err, data) => {})` | events, legacy APIs, simple hooks |
| Promises | `fn().then(ok).catch(fail)` | composition, parallelism, library APIs |
| `async`/`await` | `const data = await fn()` | readable sequential logic (the default today) |
| Async iterators | `for await (const x of stream)` | streams, pagination, event sequences |

## Goal

By the end you can read and write any asynchronous JavaScript, run work in parallel safely, cancel it, and avoid the classic race conditions and swallowed errors.

## Prerequisites

- [Callbacks](../02_functions/05_callbacks.md) and [Closures](../06_closures/01_closures.md)
- [Error Handling](../10_error-handling/00_README.md)
- Skim [Event Loop](../12_event-loop/00_README.md) for the full mechanics (this chapter gives the working model)

**Next:** [Sync vs Async](./01_sync-vs-async.md)
