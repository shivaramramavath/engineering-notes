# Concurrency Patterns

The earlier notes covered the *tools*. This one covers the *designs* that use them: recurring shapes of concurrent programs that are known to work. Most of them share one theme: **reduce what is shared, and make the sharing explicit and bounded**.

**Prerequisites:** the rest of this module, especially [Thread Safety](02_thread-safety.md), [Concurrent Collections](08_concurrent-collections.md), [Executors](10_executors-and-thread-pools.md), and [Deadlock, Livelock, Starvation](14_deadlock-livelock-starvation.md).

---

## Pattern overview

| Pattern | Problem it solves | Main tools |
|---|---|---|
| Immutable snapshot + atomic swap | Many readers, rare updates, no locks on reads | `volatile` / `AtomicReference` + immutable objects |
| Thread confinement / single writer | Shared mutable state without locks | Single-thread executor, queue |
| Producer-consumer | Decouple producing from processing | `BlockingQueue` |
| Pipeline | Multi-stage processing in parallel | Queues + one pool per stage |
| Fan-out / fan-in | Run independent calls in parallel, combine | `CompletableFuture`, structured scope, virtual threads |
| Bulkhead | One slow dependency must not sink everything | Separate pools/semaphores |
| Backpressure | Fast producers overwhelm slow consumers | Bounded queues, `CallerRunsPolicy`, semaphores |
| Timeout + deadline | Never wait forever | `tryLock`, `get(timeout)`, `orTimeout` |
| Graceful shutdown | Stop without losing or corrupting work | `shutdown`, poison pill, interrupts, hooks |
| Safe lazy initialization | Create an expensive object once | Holder idiom, `volatile` DCL, `computeIfAbsent` |

---

## 1. Immutable snapshot + atomic swap

Readers need a consistent view; updates are rare. Make the shared state **immutable**, and replace the whole thing with one atomic reference write.

```java
record Config(String host, int port, Duration timeout) {}

private volatile Config config = loadConfig();              // or AtomicReference<Config>

void reload() { config = loadConfig(); }                    // writers publish a NEW object

void call() {
    Config c = config;                                      // read ONCE, then use the local copy
    connect(c.host(), c.port());                            // always a consistent host+port pair
}
```

No locks, no torn reads, no visibility bugs ([note 04](04_memory-model-and-volatile.md)). For read-modify-write from several writers, use `AtomicReference.updateAndGet` ([note 06](06_atomic-classes.md)). It scales to many readers and works for routing tables, feature flags, and caches of derived data.

---

## 2. Thread confinement and the single-writer principle

If one thread owns a piece of state, nothing needs locking. Other threads **send it requests** instead of touching the state.

```java
class Ledger {
    private final ExecutorService owner = Executors.newSingleThreadExecutor(
            Thread.ofPlatform().name("ledger-owner").factory());
    private long balance;                                    // touched ONLY by the owner thread

    CompletableFuture<Long> deposit(long amount) {
        return CompletableFuture.supplyAsync(() -> balance += amount, owner);
    }
    CompletableFuture<Long> balance() {
        return CompletableFuture.supplyAsync(() -> balance, owner);
    }
}
```

All operations run one at a time on `ledger-owner`, in submission order. This is the essence of the **actor model** and of event-loop designs. Trade-offs: the owner thread is a throughput limit and a single point of blocking (never do slow I/O on it), but correctness is easy to reason about.

---

## 3. Producer-consumer

Producers put work on a **bounded `BlockingQueue`**, and consumers take from it. The queue handles waiting, visibility, and handoff ([note 08](08_concurrent-collections.md) has full code with a poison pill).

Why it matters: it decouples *rate* and *failure* of producers from consumers, smooths bursts, and lets you scale consumers independently. The bound is what gives you backpressure (section 6).

---

## 4. Pipeline

Split a job into stages, each with its own worker(s), connected by queues. Stages run concurrently on different items:

```text
  read ──queue──► parse ──queue──► enrich ──queue──► write
 (1 thread)      (4 threads)      (8 threads, I/O)   (1 thread)
```

```java
BlockingQueue<String> raw    = new ArrayBlockingQueue<>(1_000);
BlockingQueue<Record> parsed = new ArrayBlockingQueue<>(1_000);

pool.submit(() -> { for (String line : lines) raw.put(line); raw.put(EOF); return null; });
pool.submit(() -> {
    for (String l = raw.take(); !l.equals(EOF); l = raw.take()) parsed.put(parse(l));
    parsed.put(Record.END);
    return null;
});
pool.submit(() -> { for (Record r = parsed.take(); r != Record.END; r = parsed.take()) save(r); return null; });
```

Size each stage to its bottleneck (CPU-bound stages by cores, I/O stages higher). Bounded queues stop a fast stage from flooding a slow one. On Java 21+, virtual threads can make I/O-heavy stages much simpler ([note 13](13_virtual-threads-and-structured-concurrency.md)).

---

## 5. Fan-out / fan-in

Start independent calls in parallel, then combine, with explicit handling for required and optional parts:

```java
CompletableFuture<User>        user   = supplyAsync(() -> users.find(id), io).orTimeout(1, SECONDS);
CompletableFuture<List<Offer>> offers = supplyAsync(() -> offerSvc.forUser(id), io)
        .completeOnTimeout(List.of(), 300, MILLISECONDS);        // optional: degrade gracefully

Page page = user.thenCombine(offers, Page::new).join();
```

See [CompletableFuture](12_completablefuture.md) for details and [Virtual Threads](13_virtual-threads-and-structured-concurrency.md) for the structured-scope form with automatic cancellation of siblings.

---

## 6. Backpressure: say "slow down" instead of falling over

When producers outpace consumers, something has to give. Without a limit, the "something" is memory (`OutOfMemoryError`). With a limit, the producer slows down or the work is rejected, which is a **decision you make**.

| Mechanism | Behavior when full |
|---|---|
| Bounded `BlockingQueue` + `put` | Producer **blocks** until space is free |
| `queue.offer(item, timeout)` | Producer waits a bit, then gets `false` and can shed load |
| `ThreadPoolExecutor` + bounded queue + `CallerRunsPolicy` | The submitting thread does the work itself (slows the caller) |
| `Semaphore` around the resource | Excess callers wait or fail fast with `tryAcquire` |
| HTTP `429` / queue length limits at the edge | Reject early, with a clear error to clients |

Rule: **every queue and every pool needs a bound and a documented overflow behavior.** See [Rate Limiting](../25-real-world-patterns/03_rate-limiting.md).

---

## 7. Bulkhead: isolate failure domains

Ship compartments keep one leak from sinking the ship. Likewise, give each dependency its **own pool or semaphore**, so a slow service can only exhaust *its* resources:

```java
ExecutorService paymentPool   = boundedPool("payments", 10, 50);
ExecutorService inventoryPool = boundedPool("inventory", 20, 200);
ExecutorService emailPool     = boundedPool("email", 2, 1_000);       // low-priority, can queue a lot

// If the payment gateway hangs, only paymentPool saturates. Inventory and email keep working.
```

With a single shared pool, one hung dependency can occupy every thread and stop unrelated features (a form of starvation, [note 14](14_deadlock-livelock-starvation.md)). Combine with timeouts and a circuit breaker ([Circuit Breaker and Resilience](../25-real-world-patterns/06_circuit-breaker-and-resilience.md)).

---

## 8. Timeouts and deadlines everywhere

Any call that can block should have a timeout, and the *overall* request should carry a **deadline** that subcalls respect:

```java
Instant deadline = Instant.now().plusSeconds(2);

Duration remaining = Duration.between(Instant.now(), deadline);
if (remaining.isNegative()) throw new TimeoutException("deadline exceeded");
future.get(remaining.toMillis(), TimeUnit.MILLISECONDS);
```

Per-call timeouts that each allow 2 s can add up to far more than the user is willing to wait. Pass the remaining budget downward. Timeouts only abandon the *wait*, so make the underlying work cancellable too (client timeouts, interruption).

---

## 9. Graceful shutdown

Stopping a concurrent system badly loses work or leaves things half-written. A reliable sequence:

```text
1. Stop accepting new work           (close the listener / flip a flag / shutdown() the executor)
2. Let in-flight work finish         (awaitTermination with a deadline)
3. Interrupt stragglers              (shutdownNow(); tasks must respond to interrupts)
4. Flush and close resources         (queues drained, files, connections)
```

```java
Runtime.getRuntime().addShutdownHook(new Thread(() -> {
    acceptor.stop();                                         // 1
    workers.shutdown();
    try {
        if (!workers.awaitTermination(20, TimeUnit.SECONDS)) {   // 2
            workers.shutdownNow();                               // 3
        }
    } catch (InterruptedException e) {
        workers.shutdownNow();
        Thread.currentThread().interrupt();
    }
}, "shutdown-hook"));
```

Details: use **poison pills** to end consumers that wait on queues ([note 08](08_concurrent-collections.md)); make tasks **idempotent** so work interrupted mid-way can safely be re-run ([Idempotency](../25-real-world-patterns/04_idempotency.md)); keep shutdown hooks short and don't start threads that need other hooks to finish. Frameworks normally provide lifecycle callbacks that are better than raw hooks.

---

## 10. Safe lazy initialization and memoization

```java
// Holder idiom: lazy, thread-safe, lock-free after init
class Services {
    private static class Holder { static final HeavyService INSTANCE = new HeavyService(); }
    static HeavyService get() { return Holder.INSTANCE; }
}

// Per-key, compute once, safely shared
private final ConcurrentHashMap<String, Report> reports = new ConcurrentHashMap<>();
Report report(String key) { return reports.computeIfAbsent(key, this::generate); }   // keep generate() short and non-recursive
```

More in [Thread Safety](02_thread-safety.md#lazy-initialization), [Memory Model](04_memory-model-and-volatile.md#5-double-checked-locking), [Singleton](../24-design-patterns/01-creational/00_singleton.md). For expensive computations that must run once with waiting callers, store a `CompletableFuture` (or `FutureTask`) in the map instead of the value, so concurrent callers share the in-flight computation.

---

## 11. Choosing a design: questions to ask

1. **Can this state be immutable or confined to one thread?** If yes, stop. No locking needed.
2. **Is the work waiting or computing?** Waiting → virtual threads or a larger I/O pool. Computing → pool sized to cores.
3. **What is bounded?** Queue sizes, thread counts, in-flight requests, and downstream concurrency should all have limits.
4. **What happens on overload, on timeout, on failure, on shutdown?** If you can't answer, the design isn't finished.
5. **How will I observe it?** Named threads, queue-length and wait-time metrics, and the ability to take a thread dump.

---

## Common mistakes

| Mistake | Fix |
|---|---|
| Unbounded queues and pools "for simplicity" | Bound them and choose an overflow policy |
| One shared pool for all dependencies | Bulkheads: pools or semaphores per dependency |
| Reading a `volatile` snapshot field multiple times inside one operation | Copy it to a local once |
| Doing slow I/O on a confined/single-writer thread | Keep the owner thread's work small and non-blocking |
| No timeouts on remote calls and waits | Timeouts and an overall deadline |
| Shutting down with `System.exit` or `shutdownNow` only | Graceful sequence with a deadline, then force |
| Non-idempotent tasks with retries or shutdown re-runs | Make tasks idempotent |
| Poison pill sent once to several consumers | One per consumer (or re-queue it) |
| Inventing a custom concurrency primitive | Use the `java.util.concurrent` building blocks |
| Patterns applied without metrics | Export queue size, pool saturation, rejection counts, and latency |

### Debugging

- **Latency spikes with idle CPU** → queueing or a slow dependency. Check queue lengths, pool active counts, and downstream latency.
- **Memory growth** → an unbounded queue or unbounded task creation somewhere. Find the structure that only grows.
- **Hang on shutdown** → non-daemon threads still running, consumers waiting for a pill that never came, or tasks ignoring interrupts. Take a thread dump.
- **One feature taking everything down** → missing bulkhead. Look at thread dumps for one pool's threads all stuck on the same call.

---

## Quick Summary

- Prefer designs that **avoid shared mutable state**: immutable snapshots, confinement, single writer, message passing.
- Connect stages with **bounded queues**: they give you producer-consumer, pipelines, and backpressure at once.
- **Bulkhead** by dependency, **time out** every wait, and carry a **deadline** across calls.
- Fan out independent calls with `CompletableFuture`, virtual threads, or structured scopes, with clear required vs optional handling.
- Shut down in four steps: stop intake, drain, interrupt, close, with idempotent work.
- Lazily initialize with the holder idiom or `computeIfAbsent`, and observe everything: names, queue metrics, thread dumps.

**Next module:** [JVM Internals](../15-jvm-internals/README.md)