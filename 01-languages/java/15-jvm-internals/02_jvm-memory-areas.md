# JVM Memory Areas

"Java memory" is not one thing. A running JVM uses several distinct regions, each with its own purpose, size limit, and failure mode. Knowing which region you're looking at is the difference between "I'll raise `-Xmx`" (sometimes right) and finding out your problem was thread stacks, metaspace, or a native buffer.

**Prerequisites:** [JVM Architecture](00_jvm-architecture.md), [Class Loading](01_class-loading.md).

---

## 1. The map

```text
 ┌────────────────────────── OS process memory ──────────────────────────────────┐
 │  ┌──────────────── Java heap  (-Xms / -Xmx) ─────────────────┐               │
 │  │  objects and arrays: young generation + old generation    │  ← managed   │
 │  └────────────────────────────────────────────────────────────┘     by GC     │
 │  ┌── Metaspace ──┐ ┌── Code cache ──┐ ┌── Thread stacks ──┐ ┌── Direct ──┐   │
 │  │ class metadata│ │ JIT-compiled   │ │ one per thread    │ │ buffers    │   │
 │  │ (native mem)  │ │ machine code   │ │ (-Xss)            │ │ (off-heap) │   │
 │  └───────────────┘ └────────────────┘ └───────────────────┘ └────────────┘   │
 │  + GC data structures, symbol tables, native libraries, malloc overhead, ...   │
 └────────────────────────────────────────────────────────────────────────────────┘
```

| Area | Holds | Shared? | Sized by | Fails with |
|---|---|---|---|---|
| **Heap** | Objects, arrays | All threads | `-Xms`, `-Xmx` | `OutOfMemoryError: Java heap space` |
| **Thread stack** | Frames: locals, parameters, return addresses | One per thread | `-Xss` | `StackOverflowError` (too deep), `OutOfMemoryError: unable to create native thread` (too many/OS limit) |
| **Metaspace** | Class metadata, method bytecode | All | `-XX:MaxMetaspaceSize` (unbounded by default) | `OutOfMemoryError: Metaspace` |
| **Code cache** | JIT-compiled code | All | `-XX:ReservedCodeCacheSize` | "CodeCache is full", JIT stops, big slowdown |
| **Direct (off-heap) memory** | NIO direct buffers | All | `-XX:MaxDirectMemorySize` (defaults to max heap size) | `OutOfMemoryError: Direct buffer memory` |
| **PC register / native stacks** | Current instruction / native frames | Per thread | (small) | Rarely an issue |

**The process footprint is larger than `-Xmx`.** Add metaspace, code cache, all thread stacks, direct buffers, and GC bookkeeping. This is why a container can be killed by the OS ("OOMKilled") while the heap looks fine ([section 7](#7-sizing-for-containers)).

---

## 2. The heap

The heap is where **every object and array** lives (the JIT can sometimes avoid allocating, see section 6). It's managed by the garbage collector ([GC](04_garbage-collection.md)), and most collectors organize it by object age:

```text
 Heap
 ├─ Young generation   new objects:  Eden  +  Survivor 0  +  Survivor 1
 └─ Old generation     objects that survived several collections ("tenured")
```

- New objects start in **Eden**. Most die young, and a **minor GC** cleans the young generation cheaply.
- Survivors are copied between survivor spaces, and after enough collections they're **promoted** to the old generation, which is collected less often (and is more expensive to collect).
- Collectors like G1 split the heap into many equal **regions** that play the young/old roles dynamically. ZGC and Shenandoah also manage regions ([GC Algorithms](05_gc-algorithms.md)).

**Allocation is fast.** Each thread gets a **TLAB** (thread-local allocation buffer), a private chunk of Eden where `new` is just a pointer bump with no locking.

### Sizing

```bash
-Xms512m -Xmx2g                      # initial and maximum heap
-XX:MaxRAMPercentage=75              # max heap as % of available RAM (container-aware)
```

- If you don't set `-Xmx`, the default max heap is **a quarter of the available RAM** (`MaxRAMPercentage=25`), and the JVM respects **container memory limits** in modern versions.
- Setting `-Xms` equal to `-Xmx` avoids resize pauses and makes memory use predictable (at the cost of reserving it all).
- A bigger heap isn't automatically better: it means fewer but potentially longer collections for some collectors, and less memory left for everything else.

---

## 3. Thread stacks

Each thread has its own **stack**: a stack of **frames**, one per active method call.

```text
 Thread stack (grows downward as calls nest)
 ┌─────────────────────────┐
 │ frame: compute(int n)   │ ← current method
 │   locals: n, result     │
 │   operand stack, ...    │
 ├─────────────────────────┤
 │ frame: process(Order o) │
 │   locals: o, total      │
 ├─────────────────────────┤
 │ frame: main(String[] a) │
 └─────────────────────────┘
```

- **Primitive locals** live in the frame. **Object locals are references** in the frame pointing to objects on the heap. That's the mechanical reason Java is pass-by-value ([Pass by Value](../02-methods/01_pass-by-value.md)).
- A frame disappears when its method returns, and the objects it referenced stay if something else still references them.
- Stack size is fixed per thread (`-Xss`, commonly 512 KB to 1 MB by default on 64-bit). **Deep or infinite recursion** exhausts it:

```text
Exception in thread "main" java.lang.StackOverflowError
    at Demo.recurse(Demo.java:5)
    at Demo.recurse(Demo.java:5)   ← the same line repeated
```

Fix by finding the missing base case, converting to iteration, or raising `-Xss` only if the depth is legitimate ([Recursion](../02-methods/03_recursion.md)).

- Many threads × big stacks add up: 2,000 platform threads at 1 MB is ~2 GB of *address space* (touched only as used, but still a footprint). **Virtual threads** keep their stacks on the **heap** and grow as needed ([Virtual Threads](../14-concurrency/13_virtual-threads-and-structured-concurrency.md)).

---

## 4. Metaspace and the code cache

**Metaspace** (Java 8+, replacing the old PermGen) holds class metadata: method bytecode, constant pools, field/method layouts. It lives in **native memory**, grows on demand, and is **unbounded by default** (until the machine runs out), so set `-XX:MaxMetaspaceSize` if you want a hard limit. It grows when *many classes are loaded*: big frameworks, generated classes (proxies, lambdas, bytecode-generating libraries), and **class loader leaks** that keep old classes alive ([Class Loading](01_class-loading.md#unloading)).

The **code cache** stores JIT-compiled machine code ([JIT Compiler](06_jit-compiler.md)). If it fills, the JVM logs `CodeCache is full. Compiler has been disabled` and stops compiling new methods, so performance degrades sharply. Raise `-XX:ReservedCodeCacheSize` when you see that message.

---

## 5. Direct memory and native memory

`ByteBuffer.allocateDirect(n)` (and libraries built on it, like Netty and many NIO uses) allocates **off-heap**, outside GC control, avoiding a copy when doing I/O. The `ByteBuffer` object is on the heap, but the bytes aren't. Direct memory is limited by `-XX:MaxDirectMemorySize` and is freed when the buffer object is collected (or explicitly, in libraries that support it). Leaking buffer objects leaks native memory.

The JVM itself also uses **native memory** for GC structures, symbol tables, and the malloc/arena overhead of native libraries. You can see the breakdown with **Native Memory Tracking**:

```bash
java -XX:NativeMemoryTracking=summary -jar app.jar
jcmd <pid> VM.native_memory summary
```

---

## 6. What an object costs

An object on the heap has a **header** plus its fields, padded to a multiple of 8 bytes:

```text
 Classic layout (64-bit, compressed class pointers)          Compact object headers
 ┌───────────────────────────────┐                           ┌───────────────────────────────┐
 │ header: mark word (8 bytes)   │                           │ header: 8 bytes total         │
 │         class pointer (4)     │  = 12 bytes               │ (mark word + class combined)  │
 ├───────────────────────────────┤                           ├───────────────────────────────┤
 │ fields ...                    │                           │ fields ...                    │
 │ padding to 8-byte alignment   │                           │ padding to 8-byte alignment   │
 └───────────────────────────────┘                           └───────────────────────────────┘
```

Example: `class Point { int x; int y; }`

- Classic: 12 header + 8 fields = 20 → padded to **24 bytes**.
- Compact object headers: 8 header + 8 fields = **16 bytes**.

**Compact object headers** became a product feature in Java 25 (opt-in with `-XX:+UseCompactObjectHeaders`) and are **on by default in Java 27** (JEP 534), cutting heap use noticeably for object-heavy programs with no code changes. Disable with `-XX:-UseCompactObjectHeaders` if needed.

Other facts:

- **Compressed ordinary object pointers (compressed oops)** are on by default for heaps up to roughly 32 GB, storing references in 4 bytes instead of 8. Going just past that limit makes references bigger and can *reduce* effective capacity, so a 40 GB heap may hold less than a 31 GB one.
- Arrays add a 4-byte length to the header. Objects of tiny classes cost far more than their data: a `List<Integer>` of a million elements is a million `Integer` objects plus references ([Memory Optimization](../20-performance/02_memory-optimization.md)).
- You can inspect real layouts with the OpenJDK **JOL** tool.
- **Escape analysis** lets the JIT avoid heap allocation for objects that never escape a method (it replaces them with their fields in registers/stack), so `new` doesn't always mean "heap" ([JIT Compiler](06_jit-compiler.md)).
- **Strings** and the string pool live on the heap (since Java 7) ([String Pool and Comparison](../03-strings-and-text/02_string-pool-and-comparison.md)).

---

## 7. Sizing for containers

Everything adds up against one limit:

```text
 container limit  ≥  heap (-Xmx)
                   + metaspace
                   + code cache
                   + thread stacks (threads × -Xss)
                   + direct buffers
                   + GC / JVM native overhead
```

Practical guidance:

- Don't set `-Xmx` equal to the container limit. Leave headroom (a common starting point is 60-75% of the limit for the heap, then verify with NMT and monitoring).
- Prefer `-XX:MaxRAMPercentage` so the heap scales with the container's actual limit.
- If a container is **OOMKilled** but there's no `OutOfMemoryError` in the Java logs, the kernel killed the process because **total** memory exceeded the limit. Look at non-heap memory.
- Set limits on things that otherwise grow unbounded: `MaxMetaspaceSize`, `MaxDirectMemorySize`, thread counts.

---

## 8. `OutOfMemoryError`: which one?

| Message | Meaning | First steps |
|---|---|---|
| `Java heap space` | Heap is full of live objects (a leak, or the workload really needs more) | Heap dump; check for leaks; then consider more heap |
| `GC overhead limit exceeded` | The JVM is spending almost all its time in GC and recovering almost nothing | Same as above: the live set is nearly the whole heap |
| `Metaspace` | Too many classes loaded | Class loader leak (redeploys), runaway class generation; check loader counts |
| `unable to create native thread` | The OS refused a new thread (limit/ memory) | Too many threads; reduce them, use virtual threads/pools, check `ulimit -u` and `-Xss` |
| `Direct buffer memory` | Off-heap buffer limit hit | Leaked or oversized direct buffers; check `MaxDirectMemorySize` |
| `Requested array size exceeds VM limit` / `Java heap space` on a huge allocation | A single enormous array | Fix the data size/logic (streaming) |
| `Compressed class space` | Class metadata area for compressed class pointers is full | Rare; usually the same causes as Metaspace |

### Diagnosing

```bash
# Capture evidence automatically when it happens
java -XX:+HeapDumpOnOutOfMemoryError -XX:HeapDumpPath=/dumps -jar app.jar

# On a live process
jcmd <pid> GC.heap_info                       # heap summary
jcmd <pid> GC.class_histogram                 # instances by class (cheap overview)
jcmd <pid> GC.heap_dump /tmp/heap.hprof       # full dump (pauses the app; large file)
jcmd <pid> VM.native_memory summary           # needs -XX:NativeMemoryTracking
jstat -gcutil <pid> 1000                      # GC/occupancy every second
```

Analyze heap dumps with Eclipse MAT or VisualVM: look at the **dominator tree** and **path to GC roots** to find what's keeping the memory alive ([JVM Diagnostics](../22-production-engineering/04_jvm-diagnostics.md)).

### Typical leak sources (Java "leaks" are objects still reachable but no longer needed)

- `static` collections and caches that only grow (no eviction/TTL)
- Listeners/callbacks registered but never removed
- `ThreadLocal` values not removed on pooled threads ([ThreadLocal](../14-concurrency/09_thread-local.md))
- Unclosed resources (streams, connections, `ExecutorService`s with live threads)
- Class loader leaks after redeploys ([Class Loading](01_class-loading.md))
- Unbounded queues between fast producers and slow consumers ([Concurrency Patterns](../14-concurrency/15_concurrency-patterns.md))

---

## Common mistakes

| Mistake | Fix |
|---|---|
| Assuming `-Xmx` is the total memory of the JVM | Budget for metaspace, stacks, direct memory, and JVM overhead |
| `-Xmx` = container limit | Leave headroom; use `MaxRAMPercentage` |
| "Fixing" `StackOverflowError` by raising `-Xss` | Find the runaway recursion first |
| Raising heap for an `OutOfMemoryError: Metaspace` | It's a different region. Look for class/loader leaks |
| Creating thousands of platform threads | Pools, or virtual threads |
| Ignoring native memory growth (direct buffers, JNI) | NMT and `MaxDirectMemorySize` |
| Assuming a memory leak = forgotten `free` | It's a *reachability* problem: find the reference path |
| Holding huge data in `static` fields "for convenience" | Scope and evict |
| No heap dump on OOM in production | `-XX:+HeapDumpOnOutOfMemoryError` with a path and enough disk |

---

## Quick Summary

- The JVM process has several memory regions: **heap** (objects), **thread stacks** (frames), **metaspace** (class metadata, native), **code cache** (JIT code), **direct/native memory**. The total is more than `-Xmx`.
- Heap is generational (young: Eden + survivors; old). Allocation uses per-thread buffers (TLABs) and is fast.
- Locals/frames are on the stack, objects on the heap, references link them. `StackOverflowError` is about stack depth.
- Object overhead matters: header + fields + padding. **Compact object headers are the default in Java 27**; compressed oops work up to ~32 GB heaps.
- Each `OutOfMemoryError` message names a different region. Capture heap dumps, use `jcmd`, and trace the **path to GC roots**.
- In containers, budget *all* regions against the limit and leave headroom.

**Next:** [Bytecode](03_bytecode.md)