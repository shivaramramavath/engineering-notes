# Runnable, Callable and Future

These three types separate **what to run** from **how it runs** and **how you get the outcome**:

- `Runnable`: a task that returns nothing.
- `Callable<V>`: a task that **returns a value** and may **throw a checked exception**.
- `Future<V>`: a handle to the **result of a task that may not have finished yet**.

**Prerequisites:** [Threads and Lifecycle](00_threads-and-lifecycle.md), [Functional Interfaces](../09-functional-java/01_functional-interfaces.md).

---

## 1. `Runnable` vs `Callable`

```java
@FunctionalInterface public interface Runnable    { void run(); }
@FunctionalInterface public interface Callable<V> { V call() throws Exception; }
```

| | `Runnable` | `Callable<V>` |
|---|---|---|
| Returns a value | No | Yes (`V`) |
| Checked exceptions | Can't throw | Can throw `Exception` |
| Works with `new Thread(...)` | Yes | No (needs an executor or `FutureTask`) |

```java
Runnable r = () -> System.out.println("side effect only");

Callable<Integer> c = () -> {
    Thread.sleep(100);                 // checked exception allowed here
    return 42;
};
```

When you submit a lambda to an executor, the compiler picks by shape: a lambda that **returns a value** becomes a `Callable`, and one that doesn't becomes a `Runnable`.

```java
executor.submit(() -> compute());          // returns a value → Callable → Future<Integer>
executor.submit(() -> System.out.println("x"));   // no value → Runnable → Future<?>
```

---

## 2. `Future`: the result of work in progress

You submit a task to an `ExecutorService` ([note 10](10_executors-and-thread-pools.md)) and immediately get back a `Future`:

```java
ExecutorService pool = Executors.newFixedThreadPool(4);

Future<Integer> future = pool.submit(() -> slowComputation());

doSomethingElse();                        // keep working while the task runs

Integer result = future.get();            // blocks until the result is ready
```

### The methods

| Method | Behavior |
|---|---|
| `get()` | Blocks until done. Returns the result, or throws |
| `get(timeout, unit)` | Same, but throws `TimeoutException` if not done in time. **The task keeps running** |
| `isDone()` | `true` if finished normally, exceptionally, or cancelled |
| `cancel(mayInterruptIfRunning)` | Attempts to cancel. Returns `false` if it already completed |
| `isCancelled()` | `true` if cancelled before completion |
| `resultNow()`, `exceptionNow()`, `state()` (Java 19+) | Non-blocking inspection of a *completed* future |

### What `get()` can throw

```java
try {
    int value = future.get(2, TimeUnit.SECONDS);
} catch (ExecutionException e) {
    Throwable real = e.getCause();            // the exception the task threw
} catch (TimeoutException e) {
    future.cancel(true);                      // give up on it
} catch (InterruptedException e) {
    Thread.currentThread().interrupt();       // the WAITING thread was interrupted
} catch (CancellationException e) {
    // the task was cancelled
}
```

- **`ExecutionException`** wraps whatever the task threw. Always look at `getCause()`.
- **`TimeoutException`** doesn't stop the task. Call `cancel(true)` if you no longer want it.
- **`InterruptedException`** is about the *calling* thread. Handle it as in [note 00](00_threads-and-lifecycle.md#4-interruption-the-cooperative-way-to-stop).

### Cancellation is cooperative

`cancel(true)` interrupts the worker thread. If the task is in a blocking call that responds to interrupts, it stops. A task in a tight CPU loop that never checks `Thread.currentThread().isInterrupted()` will **keep running**: the `Future` reports "cancelled" but the work continues. Write long-running tasks to check for interruption.

---

## 3. Running several tasks

```java
List<Callable<String>> jobs = List.of(() -> fetch("a"), () -> fetch("b"), () -> fetch("c"));

List<Future<String>> futures = pool.invokeAll(jobs);     // waits for ALL to finish
for (Future<String> f : futures) {
    System.out.println(f.get());                         // won't block: they're done
}

String fastest = pool.invokeAny(jobs);                   // first SUCCESSFUL result; cancels the rest
```

`invokeAll` also has a timeout form. Tasks not finished by then are cancelled.

Submit-then-collect pattern for processing results as they arrive: use an `ExecutorCompletionService`, which gives you futures in **completion order**:

```java
CompletionService<String> cs = new ExecutorCompletionService<>(pool);
jobs.forEach(cs::submit);
for (int i = 0; i < jobs.size(); i++) {
    String r = cs.take().get();           // whichever finishes next
}
```

---

## 4. `FutureTask`: when you need a future without an executor

`FutureTask<V>` is both a `Runnable` and a `Future<V>`. It's the building block executors use, and it can be handed to a plain `Thread`:

```java
FutureTask<Integer> task = new FutureTask<>(() -> 6 * 7);
new Thread(task).start();
int answer = task.get();                  // 42
```

---

## 5. Limits of `Future`

`Future` is deliberately simple, and the simplicity shows quickly:

- **Blocking only.** There's no callback for "when it's done", so you either `get()` (block a thread) or poll `isDone()`.
- **No composition.** You can't say "when A finishes, run B with its result" or "combine A and B".
- **No built-in exception handling or fallback.**
- **No manual completion**, so you can't create a future and complete it later from another thread.

[`CompletableFuture`](12_completablefuture.md) solves all of these and is the default choice for asynchronous pipelines. With virtual threads, plain blocking code with `Future.get()` is also fine ([note 13](13_virtual-threads-and-structured-concurrency.md)).

---

## Common mistakes

| Mistake | Fix |
|---|---|
| Calling `get()` right after `submit()` in a loop (serializes everything) | Submit all tasks first, then collect |
| Calling `get()` with no timeout on anything that could hang | Use a timeout |
| Ignoring `ExecutionException`'s cause | Unwrap with `getCause()` |
| Discarding the `Future` from `submit` | The task's exception is **silently lost**: you must `get()` it (or use `execute`, which routes to the uncaught-exception handler) |
| Assuming `cancel(true)` always stops the task | Only if the task responds to interruption |
| Calling `get()` from a pool task that waits on another task in the *same* pool | Pool starvation deadlock ([note 14](14_deadlock-livelock-starvation.md)) |
| Treating `TimeoutException` as cancellation | Call `cancel` yourself |
| Using `Runnable` when you need the result or checked exceptions | Use `Callable` |

### Debugging

- A swallowed exception: tasks passed to `submit` store failures in the `Future`. If nobody calls `get()`, nobody ever sees them. Log inside the task, or always inspect the future.
- Program hangs in `get()`: take a thread dump ([Troubleshooting Playbook](../22-production-engineering/05_troubleshooting-playbook.md)) and see what the worker is doing (or whether the pool is exhausted).
- `CancellationException` unexpectedly → something called `cancel`, often `invokeAny`/timeout logic or `shutdownNow`.

---

## Quick Summary

- `Runnable` = no result, no checked exceptions. `Callable<V>` = result + checked exceptions. `Future<V>` = handle to a result that arrives later.
- `get()` blocks and throws `ExecutionException` (task failed; use `getCause()`), `TimeoutException`, `InterruptedException`, or `CancellationException`.
- `cancel(true)` interrupts, and the task must cooperate.
- `invokeAll` waits for all; `invokeAny` returns the first success; `ExecutorCompletionService` gives completion order.
- A discarded `Future` hides exceptions.
- For chaining, combining, and callbacks, use [`CompletableFuture`](12_completablefuture.md).

**Next:** [Thread Safety](02_thread-safety.md)