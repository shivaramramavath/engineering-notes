# Threads and Lifecycle

A **thread** is an independent path of execution inside a program. Threads in the same JVM share the heap (objects) but each has its own stack and program counter. That sharing is what makes threads powerful (cheap communication) and dangerous (shared mutable state).

Every Java program starts with at least one thread, `main`, and the JVM runs others for you (garbage collection, JIT compilation, finalization/cleaning).

**Concurrency vs parallelism:** *concurrency* is structuring a program as independently progressing tasks (possibly on one core, time-sliced). *Parallelism* is literally executing at the same instant on several cores. You write concurrent code, and the hardware decides how parallel it gets.

**Prerequisites:** [Methods](../02-methods/README.md), [Exceptions](../06-exceptions-and-debugging/README.md), [Lambda Expressions](../09-functional-java/00_lambda-expressions.md).

---

## 1. Creating and starting a thread

```java
Thread t = new Thread(() -> System.out.println("hello from " + Thread.currentThread().getName()));
t.start();           // asks the JVM to run it concurrently
t.join();            // wait for it to finish
```

The task is a `Runnable` ([Runnable, Callable and Future](01_runnable-callable-and-future.md)); `Thread` is just the mechanism that runs it.

Since Java 21 there are builders for both kinds of thread:

```java
Thread platform = Thread.ofPlatform().name("worker-1").daemon(false).start(task);   // OS-backed thread
Thread virtual  = Thread.ofVirtual().name("v-1").start(task);                       // lightweight: see note 13
Thread quick    = Thread.startVirtualThread(task);
```

Extending `Thread` also works (`class MyThread extends Thread { public void run() {...} }`), but it ties *what* to run to *how* it runs. Prefer passing a `Runnable`.

### `start()` vs `run()`

```java
t.run();      // just a method call: runs on the CURRENT thread, no concurrency!
t.start();    // creates a new thread of execution which then calls run()
t.start();    // second call → IllegalThreadStateException (a thread can start only once)
```

Calling `run()` instead of `start()` is a classic beginner bug: the program works but is silently sequential.

In real code you rarely create threads by hand. You hand tasks to an **executor** ([Executors and Thread Pools](10_executors-and-thread-pools.md)) or use virtual threads ([note 13](13_virtual-threads-and-structured-concurrency.md)).

---

## 2. Thread lifecycle

```text
          start()                       scheduler picks it
  NEW ───────────────► RUNNABLE ◄────────────────────────┐
                        │  │  │                          │
  waiting for a monitor │  │  │ wait()/join()/park()     │ notified / joined / unparked
  lock (synchronized)   │  │  ▼                          │
                        ▼  │  WAITING ───────────────────┤
                    BLOCKED│                              │
                           │ sleep(ms)/wait(ms)/join(ms)  │
                           ▼                              │
                       TIMED_WAITING ─────────────────────┘
        run() returns or throws
  RUNNABLE ───────────────► TERMINATED
```

`Thread.getState()` returns one of `Thread.State`:

| State | Meaning |
|---|---|
| `NEW` | Created, `start()` not yet called |
| `RUNNABLE` | Running **or ready to run** (includes threads blocked in native I/O, as far as the JVM can tell) |
| `BLOCKED` | Waiting to enter a `synchronized` block/method (another thread holds the monitor) |
| `WAITING` | Waiting indefinitely: `Object.wait()`, `Thread.join()`, `LockSupport.park()` (this is what `Lock`s and queues use) |
| `TIMED_WAITING` | Same with a timeout: `sleep(ms)`, `wait(ms)`, `join(ms)` |
| `TERMINATED` | `run()` finished |

Use states when reading thread dumps: many `BLOCKED` threads point at a contended monitor ([Deadlock, Livelock, Starvation](14_deadlock-livelock-starvation.md)).

---

## 3. Working with a running thread

```java
Thread.sleep(500);                  // pause the current thread; does NOT release any locks it holds
Thread.currentThread();             // the thread executing this line
t.join();                           // wait for t to terminate (also gives a happens-before edge, note 04)
t.join(2_000);                      // wait at most 2 s (then check t.isAlive())
t.isAlive();
Thread.onSpinWait();                // hint for busy-wait loops (rarely needed)
```

### Naming and identity

Give threads **meaningful names**: they appear in logs, thread dumps, and profilers. `Thread-7` tells you nothing.

```java
new Thread(task, "invoice-sender-1");
```

`Thread.threadId()` (Java 19+) replaces the deprecated `getId()`.

### Daemon threads

```java
t.setDaemon(true);    // must be set BEFORE start()
```

The JVM exits when **all non-daemon threads** have finished. Daemon threads (like the GC) are abandoned at that point, mid-task, with no cleanup. Use them for background housekeeping, never for work that must complete. A forgotten non-daemon thread (or an un-shut-down thread pool) is the usual reason a program "hangs" at exit. Virtual threads are always daemon threads.

### Priorities

`setPriority(1..10)` is only a hint to the OS scheduler and behaves differently per platform. Don't build correctness on it.

---

## 4. Interruption: the cooperative way to stop

Java has **no safe way to forcibly stop a thread**. `Thread.stop()` was dangerous (it left objects half-updated) and now throws `UnsupportedOperationException`. Instead you *ask* a thread to stop, and the thread *cooperates*.

```java
t.interrupt();        // sets the thread's interrupt flag, and wakes it if it is sleeping/waiting
```

What the target sees:

- If it's blocked in `sleep`, `wait`, `join`, `BlockingQueue.take()`, `Future.get()`, or similar, that method **throws `InterruptedException`** and **clears** the flag.
- If it's running ordinary code, nothing happens except the flag being set; the code must check it.

```java
Runnable worker = () -> {
    while (!Thread.currentThread().isInterrupted()) {      // check the flag in long loops
        doUnitOfWork();
    }
    // clean up and return
};
```

### Handling `InterruptedException` correctly

An interrupt is a **request to stop what you're doing**. Don't swallow it.

```java
// WRONG: the interrupt is lost; callers/threads above never learn about it
try { Thread.sleep(1000); } catch (InterruptedException e) { }

// RIGHT (1): you can propagate it
void pause() throws InterruptedException { Thread.sleep(1000); }

// RIGHT (2): you can't throw it → restore the flag so higher-level code can see it
try {
    Thread.sleep(1000);
} catch (InterruptedException e) {
    Thread.currentThread().interrupt();
    return;                                  // and stop what you're doing
}
```

| Method | Effect on the flag |
|---|---|
| `t.isInterrupted()` | Reads it, leaves it unchanged |
| `Thread.interrupted()` (static) | Reads **and clears** it for the current thread |
| `InterruptedException` thrown | Flag is cleared before throwing |

Thread pools stop tasks with `shutdownNow()` and `Future.cancel(true)`, and both work by interrupting, so code that ignores interrupts can't be stopped ([Executors](10_executors-and-thread-pools.md)).

---

## 5. Exceptions in threads

An uncaught exception **terminates only that thread**. The rest of the program carries on, and the error appears on stderr and may go unnoticed.

```java
Thread t = new Thread(() -> { throw new IllegalStateException("boom"); }, "worker");
t.setUncaughtExceptionHandler((th, ex) -> log.error("Thread {} died", th.getName(), ex));
t.start();

Thread.setDefaultUncaughtExceptionHandler(...);    // JVM-wide fallback
```

You can't catch a thread's exception with a `try`/`catch` around `start()`: that code runs on a different thread. Tasks submitted to executors capture exceptions in the `Future` instead ([Runnable, Callable and Future](01_runnable-callable-and-future.md)).

---

## 6. Cost of platform threads

A classic ("platform") thread is a thin wrapper over an **OS thread**:

- Each reserves a stack (typically about 1 MB of virtual address space by default, configurable with `-Xss`).
- Creating and context-switching them takes microseconds, which is slow at scale.
- A machine handles thousands, not millions.

That's why we use **thread pools** (reuse a bounded set of threads) and, for huge numbers of mostly-blocked tasks, **virtual threads** ([note 13](13_virtual-threads-and-structured-concurrency.md)).

---

## Common mistakes

| Mistake | Fix |
|---|---|
| Calling `run()` instead of `start()` | `start()` |
| Starting a thread twice | Create a new `Thread` |
| Swallowing `InterruptedException` | Propagate it, or restore with `Thread.currentThread().interrupt()` |
| Using `Thread.sleep` to "wait until the other thread is done" | `join`, a latch, or a `Future` ([Synchronizers](07_synchronizers.md)) |
| Expecting `sleep` to release locks | It doesn't. Don't sleep inside `synchronized` |
| Relying on thread priorities | Don't |
| Unnamed threads | Name them |
| Non-daemon background thread stops the JVM from exiting | `setDaemon(true)` or shut it down properly |
| Creating a thread per task in a server | Use an executor or virtual threads |
| Catching exceptions around `start()` | Use an uncaught-exception handler or `Future` |
| Writing `Thread.stop()`-style cancellation | Interruption plus a stop flag ([volatile](04_memory-model-and-volatile.md)) |

### Debugging

- **See what every thread is doing:** `jcmd <pid> Thread.print` (or `jstack <pid>`). Thread names and states make dumps readable ([Troubleshooting Playbook](../22-production-engineering/05_troubleshooting-playbook.md)).
- Program won't exit → thread dump, look for live non-daemon threads (pool workers, timers).
- Thread "disappeared" → look for an uncaught exception on stderr, or install an uncaught-exception handler.
- Output interleaved or out of order → expected; threads give no ordering guarantees without synchronization.

---

## Quick Summary

- A thread is an execution path with its own stack and shared heap. `start()` runs it concurrently, `run()` doesn't.
- States: `NEW → RUNNABLE ⇄ BLOCKED / WAITING / TIMED_WAITING → TERMINATED`.
- Stopping is **cooperative**: `interrupt()` sets a flag or wakes blocking calls with `InterruptedException`. Never swallow it.
- Daemon threads don't keep the JVM alive. Uncaught exceptions only kill their own thread.
- Platform threads are costly OS threads: use pools, or virtual threads for blocking I/O.
- Name threads, and prefer executors over raw `Thread`.

**Next:** [Runnable, Callable and Future](01_runnable-callable-and-future.md)