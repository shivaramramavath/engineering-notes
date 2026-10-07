# JVM Architecture

The **Java Virtual Machine (JVM)** is the program that runs your program. It loads compiled classes, manages memory, executes bytecode (interpreting it at first, compiling the hot parts to machine code), collects garbage, and provides threads and a bridge to native code. "Write once, run anywhere" works because you compile to **bytecode**, a portable instruction set, and each platform has a JVM that knows how to run it.

This note is the map. The next notes zoom into each region.

**Prerequisites:** [How Java Works](../00-setup/02_how-java-works.md).

---

## 1. JDK, JRE, JVM

```text
 JDK  (Java Development Kit)       tools to BUILD:  javac, jar, jlink, jdeps, javadoc...
  └─ JRE  (runtime)                libraries to RUN: java.base, java.sql, ...
      └─ JVM                       the engine: class loader, memory, interpreter/JIT, GC
```

- **JVM** is a *specification* (what behavior a conforming machine must have) with several *implementations*: **HotSpot** (OpenJDK, Oracle JDK, Temurin, Corretto, and most others), **OpenJ9** (Eclipse/IBM), and **GraalVM** (HotSpot-based, with the Graal compiler and an ahead-of-time native-image option). These notes describe HotSpot.
- Since Java 11, most vendors ship only a JDK, with no separate JRE download. You install a JDK, or build a minimal custom runtime with `jlink` ([Java Modules](../05-packages-and-modules/02_java-modules.md)).
- The JVM runs **bytecode**, not Java. Kotlin, Scala, Groovy, and Clojure also compile to bytecode and run on the same JVM.

---

## 2. The architecture

```text
 ┌───────────────────────────── JVM process ────────────────────────────────────┐
 │                                                                              │
 │  ┌───────────────────────┐                                                   │
 │  │ Class Loader Subsystem│  load → link (verify, prepare, resolve) → init    │
 │  └───────────┬───────────┘                                                   │
 │              ▼                                                               │
 │  ┌─────────────────────── Runtime Data Areas ────────────────────────────┐   │
 │  │ Shared by all threads:         │ Per thread:                          │   │
 │  │  • Heap (objects)              │  • JVM stack (frames, locals)        │   │
 │  │  • Metaspace (class metadata)  │  • PC register                       │   │
 │  │  • Code cache (JIT output)     │  • Native method stack               │   │
 │  └───────────────────────────────────────────────────────────────────────┘   │
 │              ▲                                                               │
 │  ┌───────────┴───────────┐   ┌────────────────────┐   ┌──────────────────┐   │
 │  │   Execution Engine    │   │ Garbage Collector  │   │ Native interface │   │
 │  │ interpreter + JIT     │   │ (frees the heap)   │   │ JNI / FFM API    │   │
 │  └───────────────────────┘   └────────────────────┘   └────────┬─────────┘   │
 └─────────────────────────────────────────────────────────────────┼────────────┘
                                                                   ▼
                                                      native libraries (C/C++), OS
```

| Component | Job | Note |
|---|---|---|
| **Class loader subsystem** | Finds `.class` files, loads them, verifies and initializes them | [01](01_class-loading.md) |
| **Runtime data areas** | Heap, stacks, class metadata, compiled code | [02](02_jvm-memory-areas.md) |
| **Execution engine** | Runs bytecode: interpreter first, JIT compiler for hot code | [03](03_bytecode.md), [06](06_jit-compiler.md) |
| **Garbage collector** | Reclaims unreachable objects | [04](04_garbage-collection.md), [05](05_gc-algorithms.md) |
| **Native interface** | Calls into native code: JNI (classic) and the Foreign Function & Memory API (final in Java 22) | |

---

## 3. What happens when you run `java Main`

```bash
java -cp out Main
```

1. **The launcher starts the JVM:** it parses options, sizes the heap, picks a GC, sets up memory areas and system threads. On startup the JVM maps a **Class Data Sharing (CDS) archive** of pre-parsed JDK classes (enabled by default since Java 12), which speeds startup.
2. **The main class is loaded** by the application class loader, then **linked** (verified, static storage prepared) and **initialized** (static initializers run).
3. **`Main.main(String[])` is invoked** on the `main` thread. Bytecode runs in the **interpreter**, and the JVM counts how often methods and loops execute.
4. **Hot code is compiled** to machine code by the JIT in background threads, while the program keeps running.
5. **Objects are allocated on the heap**, and the **GC** reclaims them when they become unreachable.
6. **The JVM exits** when `main` and all **non-daemon threads** finish, or when something calls `System.exit`/`Runtime.halt`. **Shutdown hooks** run first on a normal exit ([Concurrency Patterns](../14-concurrency/15_concurrency-patterns.md#9-graceful-shutdown)).

Recent JDKs shorten steps 1-4 with ahead-of-time caching of class loading, linking, and profile data (Project Leyden work, starting with Java 24), covered in [JIT Compiler](06_jit-compiler.md).

---

## 4. Threads inside the JVM

Your program isn't the only thing running. A thread dump (`jcmd <pid> Thread.print`) shows JVM-internal threads next to yours:

| Thread (examples) | Purpose |
|---|---|
| `main` | Runs `main()` |
| GC threads (`G1 Conc#0`, `GC Thread#0`, `ZWorker`...) | Garbage collection |
| `C1/C2 CompilerThread` | JIT compilation in the background |
| `VM Thread` | Executes safepoint operations (some GC phases, deoptimization, thread dumps) |
| `Signal Dispatcher`, `Service Thread`, `Monitor Deflation Thread` | JVM housekeeping |
| `Reference Handler`, `Finalizer`, `Common-Cleaner` | Reference processing and cleanup |

A **safepoint** is a moment when all Java threads are paused at a known-safe point so the VM can do work that needs a stable view of the heap. Short safepoints are routine, and long ones show up as latency spikes.

Java threads map onto **OS threads** (platform threads) or, since Java 21, can be **virtual threads** scheduled by the JVM ([Virtual Threads](../14-concurrency/13_virtual-threads-and-structured-concurrency.md)).

---

## 5. Where does each thing live?

A question that explains many bugs:

| Thing | Lives in |
|---|---|
| Objects and arrays (`new Foo()`) | **Heap** (shared by all threads, managed by the GC) |
| Local variables, parameters, call frames | **Thread stack** (per thread). A local *reference* lives on the stack, and the object it points to on the heap |
| `static` fields | Class metadata/mirror (conceptually per class; the referenced objects are on the heap) |
| Class structure, method bytecode, constant pool | **Metaspace** (native memory) |
| JIT-compiled machine code | **Code cache** (native memory) |
| `ByteBuffer.allocateDirect` buffers, NIO, native libraries | **Native/off-heap memory** |

Details and failure modes in [JVM Memory Areas](02_jvm-memory-areas.md). Because of the last three rows, **a JVM process's memory footprint is always larger than `-Xmx`**.

---

## 6. Tools and flags

| Tool | Use |
|---|---|
| `java` / `javac` | Run / compile |
| `javap` | Disassemble class files ([Bytecode](03_bytecode.md)) |
| `jcmd` | The Swiss-army knife: thread dumps, heap info/dumps, GC, flags, native memory, JFR |
| `jstack`, `jmap`, `jstat` | Thread dump, heap histogram/dump, GC statistics (older, `jcmd` covers most) |
| `jfr` + Java Flight Recorder | Low-overhead profiling and event recording |
| `jlink`, `jdeps` | Build a minimal runtime / analyze dependencies |

JVM options come in three kinds:

```bash
java -Xmx2g -Xms2g -Xss512k ...                 # standard-ish: -X options
java -XX:+UseG1GC -XX:MaxGCPauseMillis=100 ...  # -XX: options: +flag / -flag / name=value
java -Xlog:gc*:file=gc.log:time,uptime ...      # unified logging (Java 9+)
java -XX:+PrintFlagsFinal -version | grep -i gc # show every flag and its effective value
```

`-XX:` options include *product* flags (supported), *diagnostic* flags (need `-XX:+UnlockDiagnosticVMOptions`), and *experimental* flags (need `-XX:+UnlockExperimentalVMOptions`). `JAVA_TOOL_OPTIONS` can inject options into any JVM launch (useful in containers). See the [JVM flags cheatsheet](../29-cheatsheets/07_jvm-flags-and-diagnostics.md) and [JVM Diagnostics](../22-production-engineering/04_jvm-diagnostics.md).

---

## 7. Misconceptions

| Belief | Reality |
|---|---|
| "Java is interpreted" | Interpreted *first*, then hot code is JIT-compiled to machine code ([06](06_jit-compiler.md)) |
| "The JVM and Java language are the same thing" | The JVM runs bytecode. Many languages target it |
| "The JVM uses `-Xmx` memory total" | Heap is only part of the footprint: metaspace, thread stacks, code cache, direct buffers, and GC structures are extra |
| "`new` always allocates on the heap" | The JIT can eliminate allocations (escape analysis) |
| "Garbage collection is just a background thread" | Collectors pause application threads at times (stop-the-world), to different degrees ([05](05_gc-algorithms.md)) |
| "Compiling with `javac` optimizes the code" | `javac` does little optimization. The JIT does the heavy lifting at runtime |
| "All JVMs behave the same" | Defaults, GC choices, and flags differ by vendor, version, and environment |

---

## Quick Summary

- The JVM = **class loader + runtime memory areas + execution engine (interpreter + JIT) + garbage collector + native interface**.
- `java Main`: start the JVM → load/link/initialize `Main` → run `main()` interpreted → JIT-compile hot code → GC reclaims objects → exit when non-daemon threads finish.
- Objects live on the **heap**, locals and frames on **thread stacks**, class metadata in **metaspace**, compiled code in the **code cache**. The process uses more than `-Xmx`.
- The JVM runs its own threads (GC, JIT, VM thread), and **safepoints** are the pauses where it needs a stable view.
- Tools to know: `jcmd`, `javap`, JFR, and `-Xlog`. Flags come as `-X`, `-XX:`, `-Xlog`.

**Next:** [Class Loading](01_class-loading.md)