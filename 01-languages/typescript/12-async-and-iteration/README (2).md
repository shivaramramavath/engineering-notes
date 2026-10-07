# 12 - Async and Iteration

How asynchronous code runs and how to type it: the event loop that schedules everything, promises and `async`/`await`, generic helpers for async functions, iterators and generators (sync and async), and the concurrency patterns you need once code does more than one thing at a time.

TypeScript does not change JavaScript's runtime model. Its contribution is the type information: `Promise<T>`, `Awaited<T>`, `Generator<T, TReturn, TNext>`, `AsyncIterable<T>`. The behavior (ordering, laziness, interleaving) is the same as plain JavaScript, so most of the hard parts here are about understanding what actually happens at runtime.

## Prerequisites

- [02 Functions](../02-functions/README.md): especially [callbacks](../02-functions/02-callbacks.md)
- [06 Generics](../06-generics/README.md): every helper in this section is generic
- [11 Error Handling](../11-error-handling/README.md): rejected promises and `try/catch` around `await`

## Notes in this section

| # | Note | Covers |
|---|---|---|
| 00 | [The event loop](./00-event-loop.md) | Call stack, microtasks vs macrotasks, ordering, blocking, Node specifics |
| 01 | [Promises](./01-promises.md) | States, chaining, `all` / `allSettled` / `race` / `any`, typing, pitfalls |
| 02 | [async/await](./02-async-await.md) | Execution model, sequential vs parallel, loops, errors, top-level `await`, cancellation |
| 03 | [Async generics](./03-async-generics.md) | `Awaited`, `MaybePromise`, wrapper helpers, memoizing, `Deferred`, typed collections |
| 04 | [Iterators and generators](./04-iterators-and-generators.md) | Iteration protocol, `function*`, laziness, async generators, `for await` |
| 05 | [Concurrency patterns](./05-concurrency-patterns.md) | Limits, semaphores, cancellation, stale results, deduplication, debounce, workers |

Read 00 to 02 in order. 03 and 04 can be read independently after that. 05 builds on everything before it.

## Which tool do I need?

| Situation | Reach for |
|---|---|
| Understand why logs print in a surprising order | [event loop](./00-event-loop.md) |
| Wait for several independent calls | `Promise.all` ([01](./01-promises.md)) |
| Collect every result even if some fail | `Promise.allSettled` ([01](./01-promises.md)) |
| Process a long list without overloading a service | concurrency limit ([05](./05-concurrency-patterns.md)) |
| Name the resolved type of an async function | `Awaited<ReturnType<typeof f>>` ([03](./03-async-generics.md)) |
| Write a helper that accepts sync or async callbacks | `MaybePromise<T>` ([03](./03-async-generics.md)) |
| Consume a paginated API as a flat stream | async generator + `for await` ([04](./04-iterators-and-generators.md)) |
| Produce a huge or infinite sequence without building an array | generator ([04](./04-iterators-and-generators.md)) |
| Stop an in-flight request | `AbortController` / `AbortSignal` ([02](./02-async-await.md), [05](./05-concurrency-patterns.md)) |
| Ignore a slow earlier response | latest-wins check or abort-on-new ([05](./05-concurrency-patterns.md)) |
| Share one request among simultaneous callers | cache the in-flight promise ([05](./05-concurrency-patterns.md)) |
| Run CPU-heavy work without freezing everything | worker threads or Web Workers ([00](./00-event-loop.md), [05](./05-concurrency-patterns.md)) |

## Ideas that recur across the section

- **One thread, many things in flight.** Async is concurrent, not parallel. Waiting overlaps, computing does not.
- **Microtasks before macrotasks.** Promise continuations always run before the next timer or I/O callback.
- **`await` is a suspension point.** Code runs uninterrupted until the next `await`, and anything can happen across it.
- **Promises are eager and uncancellable.** Starting is immediate, and stopping needs a cooperating API (`AbortSignal`).
- **Failures must be handled somewhere.** An unhandled rejection is a bug. Floating promises and `forEach(async ...)` are the usual cause.
- **Types follow `await`.** `Awaited<T>` models unwrapping, and `async` functions always return `Promise<T>`.
- **Laziness is a feature.** Generators and async iteration let consumers control the pace.

## Related sections

- [11 Error Handling](../11-error-handling/README.md): rejection handling and retries
- [13 Compiler and tsconfig: target, module, and lib](../13-compiler-and-tsconfig/02-target-module-and-lib.md): `ES2017`+ for native `async`, `downlevelIteration`, which promise and iterator APIs exist
- [17 Design Patterns: typed event emitter](../17-design-patterns/07-typed-event-emitter.md)
- [18 Testing and Debugging](../18-testing-and-debugging/README.md): testing async code and timing
- [19 React and Frontend: server state](../19-react-and-frontend/07-server-state-tanstack-query.md): data fetching that handles races for you
- [20 Node.js Backend](../20-nodejs-backend/README.md): the event loop in server code
- [22 Performance: runtime performance](../22-performance/02-runtime-performance.md)

## Next

[13 Compiler and tsconfig](../13-compiler-and-tsconfig/README.md)
