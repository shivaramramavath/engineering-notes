# 23 · Real-World Patterns

Small, reusable building blocks you'll write or meet in almost every production JavaScript codebase. Each note builds the pattern from scratch so you understand how it works, then points out where a well-tested library is the better choice in production.

Most of these combine earlier topics: closures, timers, promises, `AbortSignal`, `Map`, and the event loop.

## Learning Order

| # | Note | Problem it solves |
|---|---|---|
| 01 | [Debounce](./01_debounce.md) | Run once after a burst of events stops (search input, autosave) |
| 02 | [Throttle](./02_throttle.md) | Cap how often a handler runs during continuous events (scroll, drag) |
| 03 | [Retry](./03_retry.md) | Recover from transient failures with backoff and jitter |
| 04 | [Timeout](./04_timeout.md) | Stop waiting on operations that may hang; real cancellation with `AbortSignal` |
| 05 | [Memoization](./05_memoization.md) | Cache pure-function results by arguments |
| 06 | [Concurrency Control](./06_concurrency-control.md) | Limit in-flight async work (`mapLimit`, limiters) |
| 07 | [Event Emitter](./07_event-emitter.md) | Decoupled one-to-many notification (pub/sub) |
| 08 | [Caching](./08_caching.md) | TTL + LRU, cache-aside, stampede protection, HTTP caching |
| 09 | [Rate Limiting](./09_rate-limiting.md) | Cap requests per time (token bucket, 429 handling) |

## Prerequisites

- [Closures](../06_closures/README.md): every pattern here keeps state in a closure
- [Asynchronous JavaScript](../11_asynchronous-javascript/README.md): promises, async/await, cancellation
- [Event loop](../12_event-loop/README.md): why timers and microtasks behave as they do
- [Testing](../21_testing/README.md): each note shows how to test the pattern with fake timers

## How the Patterns Fit Together

```text
                 call from user / event
                           │
             ┌─────────────┴─────────────┐
        debounce / throttle        rate limiting (server side)
                           │
                  concurrency control
                           │
        ┌──────────────────┼──────────────────┐
     retry (+ backoff)   timeout (+ abort)   cache / memoize
                           │
                      remote service
```

- A **retry** wrapper should give each attempt its own **timeout**, and stop on abort.
- A **cache** or **memoizeAsync** de-duplicates identical in-flight work; a **concurrency limit** protects what's behind it.
- **Rate limiting** protects a server from clients; **throttle/pacing** keeps a client within a server's limits.
- An **emitter** is the glue for notifying other parts of the app about progress, completion, or failure.

## Suggested Paths

- **Frontend:** 01 → 02 → 04 → 05 → 07
- **Backend / Node:** 03 → 04 → 06 → 08 → 09
- **Interview prep:** debounce vs throttle (implement both), `retry` with backoff/jitter, `Promise.race` timeout and why it doesn't cancel, memoize, promise pool with a concurrency limit, tiny event emitter, LRU cache, token bucket

## Related Topics

- [Concurrency control (model and primitives)](../17_concurrency-and-parallelism/05_concurrency-control.md)
- [Cancellation and abort](../11_asynchronous-javascript/08_cancellation-and-abort.md)
- [Observer pattern](../20_design-patterns/06_observer-pattern.md)
- [Security checklist](../22_security/06_security-checklist.md)
- [Performance optimization patterns](../19_performance/05_optimization-patterns.md)

## Core Principles

1. **Create once, reuse:** wrappers like `debounce`/`throttle` must not be recreated on each call or render.
2. **Always clean up:** clear timers, remove listeners, abort in-flight work.
3. **Bound everything:** attempts, cache size, concurrency, queue length, wait time.
4. **Be explicit about failure:** what's retryable, what's cached, what happens on timeout.
5. **Test with fake time:** deterministic, fast, and no `sleep()` in tests.
