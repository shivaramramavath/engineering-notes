# ThreadLocal

A `ThreadLocal<T>` gives **each thread its own independent copy** of a variable. Because nothing is shared between threads, there's nothing to synchronize: it is *thread confinement* ([Thread Safety](02_thread-safety.md)) packaged as a class.

```java
private static final ThreadLocal<Integer> COUNTER = ThreadLocal.withInitial(() -> 0);

COUNTER.set(COUNTER.get() + 1);     // affects only the current thread's copy
```

It's used quietly by many frameworks: logging context (SLF4J's MDC), security context, transaction context, and per-request data. Understanding it also explains a famous class of production bugs (leaks and data bleeding between requests) and why `ScopedValue` was introduced.

**Prerequisites:** [Thread Safety](02_thread-safety.md), [Executors and Thread Pools](10_executors-and-thread-pools.md) (for the pool-related pitfalls).

---

## 1. API

```java
ThreadLocal<String> userId = new ThreadLocal<>();            // initial value: null
ThreadLocal<List<String>> buf = ThreadLocal.withInitial(ArrayList::new);   // per-thread initial value

userId.set("u-42");        // set for the CURRENT thread only
String id = userId.get();  // read the current thread's value (or the initial value)
userId.remove();           // delete the current thread's value: do this when you're done
```

Make `ThreadLocal` fields `private static final`. The `ThreadLocal` object is just a *key*; each `Thread` holds a map from keys to its own values.

```text
Thread A ── map: { USER_ID → "alice", BUF → [..] }
Thread B ── map: { USER_ID → "bob",   BUF → [..] }      same ThreadLocal keys, separate values
```

---

## 2. Legitimate uses

### Per-thread instances of non-thread-safe helpers

Expensive or non-thread-safe objects can be given one-per-thread instead of locking:

```java
ThreadLocalRandom.current().nextInt(100);        // JDK's built-in per-thread RNG: no contention
```

(The old `ThreadLocal<SimpleDateFormat>` trick is obsolete: use immutable, thread-safe `DateTimeFormatter` ([Formatting and Parsing](../10-date-and-time/04_date-time-formatting-and-parsing.md)).) **Never share the `ThreadLocalRandom.current()` instance across threads**: call `current()` where you use it.

### Request-scoped context

Carrying "who is the current user" or "what is the request ID" through many layers without adding a parameter to every method:

```java
final class RequestContext {
    private static final ThreadLocal<String> USER_ID = new ThreadLocal<>();

    static void set(String id)  { USER_ID.set(id); }
    static String get()         { return USER_ID.get(); }
    static void clear()         { USER_ID.remove(); }
}

// Entry point, such as a servlet filter
void handle(Request req) {
    RequestContext.set(req.userId());
    try {
        service.process(req);               // deep code can call RequestContext.get()
    } finally {
        RequestContext.clear();             // ALWAYS clean up
    }
}
```

Logging frameworks use the same technique to attach request IDs to every log line ([Logging](../22-production-engineering/00_logging.md)).

---

## 3. The big pitfall: thread pools and leaks

Pool threads **live for a long time and are reused for many tasks**. A `ThreadLocal` value set by one task and never removed is still there when the *next, unrelated task* runs on the same thread.

```java
// Task 1 on worker-3
RequestContext.set("alice");     // ...and forgets to clear

// Later, Task 2 on worker-3 (a different user's request)
RequestContext.get();            // "alice"   ← wrong user's data: a security bug, not just a memory one
```

Problems this causes:

- **Data bleeding between requests**: stale user, tenant, or transaction state. This can be a security incident.
- **Memory leaks**: the value stays reachable as long as the thread lives. With app servers, values that reference classes from a web application's class loader can prevent that class loader from ever being unloaded on redeploy (a classic "class loader leak").

The rules:

1. **Always `remove()` in a `finally`** when you set a value on a thread you don't own (pool, server, framework).
2. Don't store large objects.
3. Don't use `set(null)` as a substitute for `remove()`: the entry stays in the thread's map.

---

## 4. Thread locals and asynchronous code

A `ThreadLocal` belongs to one thread. As soon as work hops to another thread, the value is **gone**:

```java
RequestContext.set("alice");
CompletableFuture.supplyAsync(() -> RequestContext.get());   // runs on a pool thread → null (or someone else's value)
```

Executors and `CompletableFuture` do **not** propagate thread locals. You must capture the value and re-establish it inside the task, or use your framework's context-propagation support (for example task decorators and context-propagation libraries).

`InheritableThreadLocal` copies the parent's value into a **newly created** child thread, but pooled threads are created once, so it doesn't help with executors and can leak stale values. Avoid it.

---

## 5. Virtual threads

Virtual threads support thread locals, but there can be **millions** of them, each with its own copy of every thread local value. Heavy per-thread caches (a 1 MB buffer in a `ThreadLocal`, say) that were fine with 200 pool threads become a memory problem. For virtual threads:

- Don't use thread locals as a cache of expensive objects; use a proper pool or shared thread-safe structure.
- Prefer `ScopedValue` for passing context ([section 6](#6-scopedvalue-the-modern-alternative)).
- See [Virtual Threads](13_virtual-threads-and-structured-concurrency.md).

---

## 6. `ScopedValue`: the modern alternative

`ScopedValue` (final in **Java 25**) shares data **safely within a bounded scope** instead of "until someone remembers to clean up":

```java
private static final ScopedValue<String> USER_ID = ScopedValue.newInstance();

void handle(Request req) {
    ScopedValue.where(USER_ID, req.userId())          // bind for the duration of the run(...)
               .run(() -> service.process(req));
}

// Anywhere deeper in the call chain
String id = USER_ID.get();                            // throws NoSuchElementException if not bound
USER_ID.isBound();  USER_ID.orElse("anonymous");
```

How it differs from `ThreadLocal`:

| | `ThreadLocal` | `ScopedValue` |
|---|---|---|
| Lifetime | Until `remove()` (or thread dies) | **Exactly the `run`/`call` scope**, with automatic cleanup |
| Mutability | Anyone can `set` anytime | **Immutable binding** (nested scopes may *rebind*, but nothing mutates the current one) |
| Leaks | Easy to cause | Structurally impossible |
| Cost with many threads | Copy per thread | Cheap; designed for virtual threads |
| Inheritance by child threads | `InheritableThreadLocal` copies | Automatically visible to threads forked in a structured scope |

Use it for **read-only context** (user, request ID, tenant, deadlines). For mutable per-thread state you still use a `ThreadLocal`, though that's a smell. Structured scopes that fork subtasks are still a preview API ([note 13](13_virtual-threads-and-structured-concurrency.md)).

---

## 7. When *not* to use thread locals

- As a **hidden global**: methods that behave differently depending on invisible state are hard to read and test. If a value is a real input, pass it as a parameter.
- To pass data between **tasks or threads**: use queues, futures, or arguments.
- For **caching expensive objects** with virtual threads or large pools. Use a pool or thread-safe shared object.
- When an immutable shared object or a plain local variable would do.

---

## Common mistakes

| Mistake | Fix |
|---|---|
| Not calling `remove()` on pooled/server threads | `try { set(...); ... } finally { remove(); }` |
| Storing big objects in a thread local | Keep values small, or avoid |
| Expecting values to follow work onto another thread (`CompletableFuture`, executor) | Capture and re-set, or use context propagation |
| `ThreadLocal` as a global variable to avoid passing parameters | Pass the parameter explicitly, or use `ScopedValue` for true read-only context |
| `ThreadLocal<SimpleDateFormat>` in new code | `DateTimeFormatter` |
| Non-`static` `ThreadLocal` fields (one key per instance → many entries) | `private static final` |
| Using `InheritableThreadLocal` with thread pools | Don't. Pass the value explicitly |
| Sharing `ThreadLocalRandom.current()` across threads | Call `current()` in each thread |
| `set(null)` instead of `remove()` | `remove()` |

### Debugging

- Wrong user/tenant data appearing in logs or responses → look for a thread local that isn't cleared in `finally` (or one cleared too early).
- `null` from `get()` inside an async callback → the work ran on a different thread than the one that set it.
- Heap dumps showing `ThreadLocalMap$Entry` retaining large objects or class loaders → leaked thread-local values. Find the `ThreadLocal` key and where it's set ([JVM Diagnostics](../22-production-engineering/04_jvm-diagnostics.md)).
- Context "disappears" after switching to virtual threads or reactive code → check how your framework propagates context.

---

## Quick Summary

- `ThreadLocal<T>` = a separate value per thread: no sharing, so no locking.
- Good for per-thread helpers (`ThreadLocalRandom`) and request context (logging MDC, security context).
- **Always `remove()` in `finally`** on pooled threads, or you leak memory and bleed data between requests.
- Thread locals don't cross to other threads: executors, `CompletableFuture`, and `InheritableThreadLocal` don't solve it. Propagate explicitly.
- With virtual threads, avoid heavy thread-local state and prefer **`ScopedValue`** (final in Java 25): immutable, scoped, auto-cleaned.
- If a value is a real input, pass it as a parameter.

**Next:** [Executors and Thread Pools](10_executors-and-thread-pools.md)