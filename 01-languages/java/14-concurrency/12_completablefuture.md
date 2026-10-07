# CompletableFuture

`CompletableFuture<T>` is a `Future` you can **compose**. Instead of blocking on `get()`, you describe *what should happen next* when the result arrives: transform it, combine it with another result, recover from errors, apply a timeout. The result is an asynchronous pipeline that reads like a stream pipeline.

```java
CompletableFuture.supplyAsync(() -> fetchUser(42))             // start async
                 .thenApply(User::email)                       // transform the result
                 .thenAccept(email -> send(email))             // consume it
                 .exceptionally(ex -> { log.error("failed", ex); return null; });
```

It also implements `CompletionStage`, the interface that defines the chaining API. It **can be completed manually**, which makes it the bridge between callback-style APIs and futures.

**Prerequisites:** [Runnable, Callable and Future](01_runnable-callable-and-future.md), [Executors and Thread Pools](10_executors-and-thread-pools.md), [Lambda Expressions](../09-functional-java/00_lambda-expressions.md).

---

## 1. Creating futures

```java
CompletableFuture<String> a = CompletableFuture.supplyAsync(() -> "result");          // returns a value
CompletableFuture<Void>   b = CompletableFuture.runAsync(() -> doSideEffect());        // no value

ExecutorService io = Executors.newFixedThreadPool(16);
CompletableFuture<String> c = CompletableFuture.supplyAsync(() -> slowCall(), io);     // YOUR executor

CompletableFuture<String> done   = CompletableFuture.completedFuture("x");             // already complete
CompletableFuture<String> failed = CompletableFuture.failedFuture(new IOException());  // Java 9+

CompletableFuture<String> manual = new CompletableFuture<>();                          // complete it yourself
manual.complete("value");              // or manual.completeExceptionally(ex)
```

**Which thread runs `supplyAsync`/`runAsync`?** Without an executor argument, the **common `ForkJoinPool`** ([note 11](11_fork-join-and-parallelism.md)). That pool is sized for CPU work and shared by the whole JVM, so **always pass your own executor for blocking calls** (HTTP, JDBC, file I/O). Otherwise you can starve every parallel stream and async task in the process. With Java 21+, `Executors.newVirtualThreadPerTaskExecutor()` is a good choice for blocking tasks ([note 13](13_virtual-threads-and-structured-concurrency.md)).

---

## 2. Transforming and chaining

| Method | Input → output | Analogy |
|---|---|---|
| `thenApply(fn)` | `T → U` | `Stream.map` |
| `thenAccept(consumer)` | `T → void` | `forEach` |
| `thenRun(runnable)` | ignores the result | |
| `thenCompose(fn)` | `T → CompletableFuture<U>` | `flatMap` |
| `thenCombine(other, fn)` | `(T, U) → V` | Wait for **both**, then merge |

```java
CompletableFuture<Integer> len = supplyAsync(() -> "hello").thenApply(String::length);   // 5
```

### `thenApply` vs `thenCompose`

If your function itself returns a `CompletableFuture`, use `thenCompose`, otherwise you get a nested future:

```java
// findInvoice(...) returns CompletableFuture<Invoice>
var bad = findUser(id).thenApply(u -> findInvoice(u.accountId()));                              // CompletableFuture<CompletableFuture<Invoice>>: nested
CompletableFuture<Invoice> good = findUser(id).thenCompose(u -> findInvoice(u.accountId()));    // flattened
```

### Combining independent results

```java
CompletableFuture<User>        userF   = supplyAsync(() -> users.find(id), io);
CompletableFuture<List<Order>> ordersF = supplyAsync(() -> orders.forUser(id), io);   // runs in parallel with userF

CompletableFuture<Dashboard> dashboard = userF.thenCombine(ordersF, Dashboard::new);
```

Both calls start immediately and run concurrently. `thenCombine` waits for both. (Chaining with `thenCompose` instead would run them *sequentially*.)

### The `Async` variants

Every stage method has `xxxAsync` forms (with or without an executor):

```java
.thenApply(fn)                // runs on whichever thread completes the previous stage (or the caller, if it's already done)
.thenApplyAsync(fn)           // submitted to the common pool
.thenApplyAsync(fn, executor) // submitted to YOUR executor
```

Use the non-async forms for short, non-blocking transforms, and an `Async` form with an executor when the step is slow or blocking.

---

## 3. Many futures at once

```java
List<CompletableFuture<Price>> futures = ids.stream()
        .map(id -> supplyAsync(() -> priceService.get(id), io))
        .toList();                                                  // start them all first

CompletableFuture<List<Price>> all = CompletableFuture
        .allOf(futures.toArray(CompletableFuture[]::new))           // completes when ALL are done
        .thenApply(v -> futures.stream().map(CompletableFuture::join).toList());   // join won't block: all complete
```

- `allOf(...)` returns `CompletableFuture<Void>`, so you collect the values yourself, as above.
- `anyOf(...)` completes with the result of **whichever finishes first** (as `Object`, so cast it).
- If any input fails, `allOf` completes exceptionally (after all inputs finish).

Don't create-and-join in one stream pipeline (`.map(supplyAsync...).map(CompletableFuture::join)`): the stream is lazy and processes one element at a time, so you'd run everything **sequentially**. Collect to a list first.

---

## 4. Error handling

Exceptions flow down the chain: if a stage fails, dependent stages are skipped until something handles the error.

```java
CompletableFuture<String> f = supplyAsync(() -> riskyCall());

f.exceptionally(ex -> "fallback");                          // recover: value or rethrow

f.handle((value, ex) -> ex == null ? value : "fallback");   // sees BOTH success and failure; returns a new value

f.whenComplete((value, ex) -> log(value, ex));              // side effect; does NOT change the result
```

| Method | Runs on... | Can change result? |
|---|---|---|
| `exceptionally(fn)` | failure only | Yes: supplies a replacement value |
| `handle(bifn)` | success **or** failure | Yes |
| `whenComplete(biconsumer)` | success **or** failure | No (the original outcome is preserved) |

### Exception wrapping

Failures often arrive **wrapped in a `CompletionException`**:

```java
try {
    f.join();                       // throws CompletionException (unchecked)
} catch (CompletionException e) {
    Throwable cause = e.getCause(); // the real exception
}

f.get();                            // throws ExecutionException (checked) with the cause
```

Inside `exceptionally`/`handle`, the `Throwable` may be the raw exception or a `CompletionException` wrapping it, depending on whether it came from the same stage or an earlier one. Unwrap defensively:

```java
Throwable root = (ex instanceof CompletionException && ex.getCause() != null) ? ex.getCause() : ex;
```

Java 12 adds `exceptionallyCompose` for recovering with another asynchronous call.

**An unobserved failure is a silent failure.** If nobody calls `join`/`get` or attaches `exceptionally`/`handle`/`whenComplete`, the exception disappears. Always end pipelines you don't wait on with a handler that logs.

---

## 5. Timeouts (Java 9+)

```java
supplyAsync(() -> slowCall(), io)
    .orTimeout(2, TimeUnit.SECONDS);                       // fails with TimeoutException if not done in 2 s

supplyAsync(() -> slowCall(), io)
    .completeOnTimeout("default", 2, TimeUnit.SECONDS);    // completes with a fallback value instead
```

A timeout completes the **future**, but does **not stop the underlying work**. The slow call keeps running on its thread. Design the task itself to be time-limited (client timeouts), or cancel it yourself.

`CompletableFuture.delayedExecutor(delay, unit)` gives an executor that starts tasks after a delay (useful for retries with backoff, [Retry and Backoff](../25-real-world-patterns/02_retry-and-backoff.md)).

---

## 6. Threads, blocking, and cancellation

**Which thread runs a callback?**

- Non-async stage on an **incomplete** future → the thread that *completes* it.
- Non-async stage on an **already complete** future → the thread calling `thenApply` (immediately).
- `*Async` stage → a thread from the common pool or the executor you pass.

This is why heavy work in a plain `thenApply` can unexpectedly run on your **caller's** or a **pool's** critical thread. Be deliberate.

**Blocking at the end:** `join()`/`get()` block the calling thread. Fine in `main`, a test, or a virtual thread. Avoid inside event-loop or request-handling threads where you can't afford to block, and never inside the common pool's own tasks.

**Cancellation:** `cancel(true)` completes the future with a `CancellationException`, but it does **not interrupt** the running task, because `CompletableFuture` has no handle on the worker thread. Stages downstream are cancelled, while the work already running continues.

**Context:** thread locals and MDC context don't follow you into async stages ([ThreadLocal](09_thread-local.md#4-thread-locals-and-asynchronous-code)).

---

## 7. A realistic pipeline

```java
Dashboard loadDashboard(long userId) {
    CompletableFuture<User> userF = supplyAsync(() -> users.find(userId), io)
            .orTimeout(1, TimeUnit.SECONDS);                       // required data: let failure propagate

    CompletableFuture<List<Order>> ordersF = supplyAsync(() -> orders.recent(userId), io)
            .completeOnTimeout(List.of(), 500, TimeUnit.MILLISECONDS)   // optional data: degrade gracefully
            .exceptionally(ex -> {
                log.warn("orders unavailable", ex);
                return List.of();
            });

    return userF.thenCombine(ordersF, Dashboard::new).join();     // block once, at the edge
}
```

Required vs optional dependencies are made explicit, calls run in parallel, and failure behavior is visible in the code.

---

## 8. CompletableFuture and virtual threads

`CompletableFuture` shines for **fan-out/fan-in, timeouts, and fallbacks**, but chained callbacks make stack traces and debugging harder. With virtual threads you can often write plain **blocking** code and run each request or subtask on its own virtual thread, which is simpler to read, debug, and profile ([note 13](13_virtual-threads-and-structured-concurrency.md)). Use `CompletableFuture` when you truly need composition (several independent calls with different fallbacks) or you're exposing an async API.

---

## Common mistakes

| Mistake | Fix |
|---|---|
| Blocking calls in `supplyAsync` without an executor | Pass an executor sized for I/O (or virtual threads) |
| `thenApply` with a function that returns a `CompletableFuture` | `thenCompose` |
| `stream.map(supplyAsync).map(join)` in one pipeline (runs sequentially) | Collect futures into a list first, then join |
| Never observing failures | End with `exceptionally`/`handle`/`whenComplete`, or `join` |
| Assuming `orTimeout` stops the task | It only completes the future. Time-limit the work itself |
| Assuming `cancel(true)` interrupts | It doesn't. Use cooperative cancellation |
| Heavy work in non-async callbacks (runs on an unexpected thread) | Use `thenApplyAsync(fn, executor)` |
| Calling `join()` inside tasks running on the common pool | Pool starvation risk: restructure |
| Catching `ExecutionException` around `join()` | `join` throws `CompletionException` (unchecked) |
| Forgetting `allOf` returns `Void` | Collect the individual results yourself |
| Losing request context (MDC, security) across async hops | Capture and propagate it explicitly |
| Chains so long nobody can follow them | Extract named methods, or use blocking code + virtual threads |

### Debugging

- A pipeline "does nothing" or a failure never shows → no terminal handler/`join` observed it. Add `whenComplete` logging.
- Stack traces end in `CompletableFuture$AsyncSupply.run` with no sign of your caller → expected, since the original call site isn't on this thread's stack. Log correlation IDs and add context to exceptions.
- Sluggish app while many async calls run → the common pool is saturated with blocking work: check thread dumps for `ForkJoinPool.commonPool-worker-*` stuck in I/O.
- Timing: log timestamps and thread names at stage boundaries (`Thread.currentThread().getName()`) to see where time goes and which thread runs what.
- Unexpected `CompletionException` → look at `getCause()`.

---

## Quick Summary

- `CompletableFuture` = a `Future` with **non-blocking composition**: `thenApply` (map), `thenCompose` (flatMap), `thenCombine` (merge two), `allOf`/`anyOf` (many).
- **Pass an executor** for blocking work. The default is the shared common pool.
- Handle errors with `exceptionally` (recover), `handle` (both paths), `whenComplete` (observe). Failures are often wrapped in `CompletionException`. Unobserved failures vanish.
- `orTimeout` / `completeOnTimeout` complete the *future*, not the *work*. `cancel` doesn't interrupt.
- Start all futures **before** joining any, or you'll serialize them.
- With virtual threads, simple blocking code is often clearer. Keep `CompletableFuture` for real composition.

**Next:** [Virtual Threads and Structured Concurrency](13_virtual-threads-and-structured-concurrency.md)