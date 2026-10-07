# JIT Compiler

Java code starts out *slow* and gets *fast*. Bytecode begins life in an **interpreter**, and the JVM watches which code runs most. The hottest methods and loops are compiled **just in time** (JIT) into optimized machine code, using information gathered while your program runs. That's why a warmed-up Java service can rival C++ in throughput, and why the first seconds after startup look worse.

Understanding this explains warm-up, broken microbenchmarks, "why is the first request slow", and why many manual micro-optimizations do nothing.

**Prerequisites:** [JVM Architecture](00_jvm-architecture.md), [Bytecode](03_bytecode.md).

---

## 1. Interpreting vs compiling

| | Interpreter | JIT-compiled code |
|---|---|---|
| Startup | Immediate | Needs compile time |
| Speed | Slow (decode each bytecode every time) | Fast (native machine code) |
| Information | Collects **profiles** (which branches, which types) | Uses the profiles to optimize |
| Memory | Small | Compiled code lives in the **code cache** |

The JVM interprets first so the program starts quickly, then compiles only what's **hot**. Compiling everything up front would waste time on code that runs once.

---

## 2. Tiered compilation in HotSpot

HotSpot has **two** JIT compilers and a ladder of tiers:

```text
 Level 0   Interpreter            collects invocation/loop counters and profiles
   │
 Level 1-3 C1 compiler            fast to compile, modest optimization
   │          1: simple, no profiling      2: limited profiling      3: full profiling
   │
 Level 4   C2 compiler            slow to compile, aggressive optimization, uses profile data
```

- **C1** (client compiler) compiles quickly, giving a speedup early. At levels 2-3 it adds profiling code.
- **C2** (server compiler) takes longer but produces much faster code for the **hottest** methods, using the profiles collected earlier.
- A method typically goes **interpreter → C1 (with profiling) → C2**. If C2 is busy or the method isn't hot enough, it may stay at a lower tier.

### What makes code "hot"?

The JVM counts **method invocations** and **loop back-edges**. When counters pass thresholds, the method is queued for compilation on background **compiler threads**, so your program keeps running meanwhile.

### On-stack replacement (OSR)

A method with a long-running loop (such as `main` with one giant loop) might never be *called* again, so it can't be swapped to compiled code at the next call. **OSR** compiles the loop and **switches execution to the compiled version in the middle of the running loop**. It matters for benchmarks written as one big `main`.

---

## 3. What the JIT optimizes

The JIT's power comes from **inlining** plus what inlining unlocks:

| Optimization | What it does |
|---|---|
| **Inlining** | Replaces a call with the callee's body, removing call overhead and enabling everything below. Small methods (getters, setters, tiny helpers) effectively cost nothing |
| **Devirtualization** | Turns `invokevirtual`/`invokeinterface` into a direct call when only one implementation has been seen (*monomorphic*) or can exist (class hierarchy analysis), guarded by a cheap check |
| **Escape analysis** | If an object never escapes the method, the JIT can avoid allocating it: its fields live in registers (**scalar replacement**), and locks on it can be removed |
| **Lock elision / coarsening** | Removes `synchronized` on non-shared objects, merges adjacent lock regions |
| **Loop optimizations** | Unrolling, hoisting invariant computations, eliminating bounds checks, sometimes SIMD vectorization |
| **Dead code / constant folding** | Removes unreachable or useless computation, precomputes constants |
| **Intrinsics** | Replaces certain methods (`Math.sqrt`, `System.arraycopy`, `String` operations, CRC32, ...) with hand-tuned machine code |

```java
// Source
double area(Shape s) { return s.area() * 2; }
// If the profile says `s` has always been a Circle, C2 inlines Circle.area() behind a type check.
// If an object created in a loop never escapes, no allocation happens at all.
```

### Speculation and deoptimization

C2 optimizes based on **assumptions drawn from the profile** ("this branch is never taken", "this call site only ever sees `Circle`", "this class has no subclasses loaded"). It inserts cheap guards. If an assumption is violated later (a new subclass loads, an unexpected type shows up), the JVM **deoptimizes**: it throws away the compiled code, falls back to the interpreter, and recompiles with new information.

This is normal and healthy. It means **peak performance depends on stable, predictable behavior**, and that a sudden change in input patterns can cause a temporary slowdown.

---

## 4. Warm-up and its consequences

```text
 throughput
    │                          ╭───────────────  peak (C2-optimized)
    │                  ╭──────╯
    │           ╭─────╯  ← C1 + profiling, deopts, recompiles
    │  ╭───────╯
    │ ╭╯  interpreter
    └─┴──────────────────────────────────────────► time since start
```

Consequences:

- **First requests are slow.** A freshly started instance serves its first requests mostly in the interpreter or in C1 code. Services behind load balancers often need a **warm-up period** (synthetic requests, gradual traffic ramp-up) before taking full load.
- **Short-lived programs never reach peak speed.** CLI tools and tiny jobs mostly run in the interpreter/C1, so startup cost dominates.
- **Benchmarks must warm up.** A naive `System.nanoTime()` loop measures interpreter + compilation time. It also lets dead-code elimination delete work whose result isn't used. Use **JMH**, which handles warm-up, forks, and consuming results ([Benchmarking with JMH](../20-performance/01_benchmarking-with-jmh.md)).
- **CPU spikes at startup** are normal: the compiler threads are busy.
- **Autoscaling and deploys** that add cold JVMs under load can cause latency spikes. Warm instances before shifting traffic.

### Faster start and warm-up options

| Technique | Effect |
|---|---|
| **Class Data Sharing (CDS / AppCDS)** | Faster class loading ([Class Loading](01_class-loading.md#6-startup-cost-and-class-data-sharing)) |
| **Ahead-of-time cache (Project Leyden)**, introduced in Java 24 and extended in later releases | Stores loaded/linked classes (and, in newer versions, profile data and objects) from a training run so later runs start faster and reach peak sooner |
| `-XX:TieredStopAtLevel=1` | **C1 only**: very quick compile, lower peak speed. Good for short-lived tools and low-memory environments |
| **GraalVM native image** (separate product) | Compiles ahead of time to a native binary: fastest startup and lowest footprint, but a closed-world model (limited reflection and dynamic loading) and usually lower peak throughput than a warmed-up HotSpot |
| Warm-up traffic / readiness gating | Operational fix for services |

---

## 5. Seeing what the JIT does

```bash
java -XX:+PrintCompilation -jar app.jar
```

```text
    142   85       3       com.acme.Order::total (24 bytes)
    143   86       4       com.acme.Order::total (24 bytes)
    210   85       3       com.acme.Order::total (24 bytes)   made not entrant
```

Columns: timestamp (ms), compile ID, **tier** (3 = C1 with profiling, 4 = C2), method, bytecode size. **"made not entrant"** means compiled code was retired (replaced by better code, or deoptimized).

```bash
java -XX:+UnlockDiagnosticVMOptions -XX:+PrintInlining -XX:+PrintCompilation ...   # inlining decisions
jcmd <pid> Compiler.codecache                                                      # code cache usage
jcmd <pid> Compiler.queue                                                          # methods waiting to compile
```

JFR also records compilation and deoptimization events, and **JITWatch** is a tool for visualizing compile logs. These are investigative tools. You don't need them for everyday development.

### Flags worth recognizing

| Flag | Effect |
|---|---|
| `-Xint` | Interpreter only (very slow, a debugging aid) |
| `-XX:TieredStopAtLevel=1` | C1 only |
| `-XX:ReservedCodeCacheSize=` | Code cache size (raise it on "CodeCache is full") |
| `-XX:CICompilerCount=` | Number of compiler threads |
| `-XX:-DontCompileHugeMethods` | Allow compiling methods larger than 8,000 bytes of bytecode |
| `-XX:MaxInlineSize` / `-XX:FreqInlineSize` | Inlining size limits (bytecode bytes) for cold / hot call sites |

Don't set these by folklore. Defaults are well tuned, and flags change between JDK versions.

---

## 6. Writing JIT-friendly code (without overthinking it)

The most valuable advice is *don't fight the JIT*:

- **Write clear code with small methods.** Small methods inline. Huge methods may not compile at all: **methods over 8,000 bytes of bytecode are not JIT-compiled by default**, which is a real cliff for giant generated methods.
- **Keep hot call sites predictable.** A call site that sees one or two receiver types stays fast. A **megamorphic** call site (many implementations flowing through the same interface call in a hot loop) can't be inlined and costs more.
- **Avoid unnecessary allocation in hot paths**, but small short-lived objects are often optimized away by escape analysis, so verify with a profiler before contorting code ([Memory Optimization](../20-performance/02_memory-optimization.md)).
- **Use primitives and simple loops** for numeric code (`int` counters, arrays) to enable bounds-check elimination and vectorization.
- **Don't trust micro-benchmarks you wrote by hand**, and don't "optimize" without a profiler ([Profiling](../20-performance/00_profiling.md)).
- **`final` rarely makes code faster.** The JIT already knows a method is effectively final if no subclass overrides it. Use `final` for design reasons.
- **Don't try to out-trick the compiler** with manual inlining or loop unrolling. It usually makes the code worse and harder to read.

---

## 7. Misconceptions

| Belief | Reality |
|---|---|
| "Java is slow because it's interpreted" | Only at the start. Hot code is compiled to optimized machine code |
| "`javac` optimizes my code" | `javac` does very little. The JIT does the optimizing |
| "Compiled code is fixed once generated" | It can be deoptimized and recompiled as behavior changes |
| "My loop of 1,000 calls proves method A is faster than B" | Without warm-up, dead-code handling, and forks, you're measuring noise. Use JMH |
| "Fewer bytecode instructions = faster" | Inlining and the profile matter far more |
| "`final` methods/classes are faster" | Mostly no, since the JIT does class-hierarchy analysis |
| "The first 10 seconds are representative" | They include interpretation, compilation, and class loading |

---

## Common mistakes

| Mistake | Fix |
|---|---|
| Benchmarking with a hand-rolled timing loop | JMH |
| Declaring a service "slow" from cold-start measurements | Warm up and measure steady state (and separately measure startup) |
| Sending full traffic to a freshly started instance | Warm-up requests, gradual ramp, readiness gates |
| Giant generated methods | Split them. Methods over 8,000 bytes of bytecode aren't compiled |
| Ignoring "CodeCache is full" warnings | Raise `ReservedCodeCacheSize`, and find what generates so much code |
| Using `-Xint`/odd JIT flags in production | Leave defaults unless measured |
| Writing megamorphic hot loops without checking | Profile. Reduce polymorphism on the hottest path if it matters |
| Contorting code for imagined JIT effects | Write clear code, then profile |

### Debugging

- **Slow right after deploy, fine later** → warm-up. Compare early vs steady-state, and consider CDS/AOT cache or warm-up traffic.
- **High CPU from `C2 CompilerThread`** → normal during warm-up. If it persists, look for constant deoptimization (`-XX:+PrintCompilation` showing repeated "made not entrant") or very large amounts of generated code.
- **Performance cliff for one method** → check its bytecode size (`javap -c`), and whether it was compiled at all (`PrintCompilation`).
- **Performance changes after adding a new implementation of an interface** → a hot call site went from monomorphic to megamorphic.
- **A "fast" microbenchmark whose result is suspiciously constant-time** → dead-code elimination or constant folding removed the work. Use JMH's `Blackhole` and state objects.

---

## Quick Summary

- The JVM **interprets first**, profiles as it goes, then **JIT-compiles hot code**: **C1** (fast, early) and **C2** (aggressive, later) via **tiered compilation**. **OSR** swaps long-running loops into compiled code mid-flight.
- Optimizations center on **inlining**: devirtualization, escape analysis (avoiding allocation and locks), loop optimization, and intrinsics.
- C2 **speculates** from profiles and **deoptimizes** if assumptions break, so steady, predictable code runs fastest.
- **Warm-up is real:** early requests are slower, short programs never peak, benchmarks need JMH, and new instances need warm-up before full traffic.
- Faster start: **CDS/AppCDS**, the Leyden **AOT cache** (Java 24+), `TieredStopAtLevel=1`, or native image.
- Write clear code with small methods, profile before optimizing, and don't try to outsmart the JIT.

**Next module:** [JDBC and Databases](../16-jdbc-and-databases/README.md)