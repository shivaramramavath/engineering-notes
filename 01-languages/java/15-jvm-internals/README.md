# 15 · JVM Internals

You can write Java for years without thinking about the JVM, until the day you hit `OutOfMemoryError`, a 2-second GC pause, a `NoClassDefFoundError` that "can't happen", or a service that is slow for the first minute after every deploy. This module explains **what the JVM does with your program**, so those problems become explainable and fixable instead of mysterious.

It is a mental-model module. You won't tune anything here (that's [Performance](../20-performance/README.md)), but you'll know *what* is being tuned and why.

## Contents

| # | Note | The question it answers |
|---|------|--------------------------|
| 00 | [JVM Architecture](00_jvm-architecture.md) | What are the JVM's parts, and what happens between `java Main` and your first line of code? |
| 01 | [Class Loading](01_class-loading.md) | How do `.class` files become running classes? Why `ClassNotFoundException` vs `NoClassDefFoundError`? |
| 02 | [JVM Memory Areas](02_jvm-memory-areas.md) | Where do objects, locals, and class metadata live, and what are the different `OutOfMemoryError`s? |
| 03 | [Bytecode](03_bytecode.md) | What does `javac` produce, and how do I read it with `javap`? |
| 04 | [Garbage Collection](04_garbage-collection.md) | How does the JVM decide what to free, and what do pauses mean? |
| 05 | [GC Algorithms](05_gc-algorithms.md) | Serial, Parallel, G1, ZGC, Shenandoah: how do they differ, and which one when? |
| 06 | [JIT Compiler](06_jit-compiler.md) | Why is Java slow at first and fast later, and how does it get fast? |

## The big picture

```text
 Foo.java ──javac──► Foo.class (bytecode) ──► ┌──────────────── JVM ────────────────┐
                                              │ Class loading  (01)                 │
                                              │ Memory areas   (02): heap, stacks…  │
                                              │ Execution: interpreter + JIT (06)   │
                                              │ Garbage collector (04, 05)          │
                                              └─────────────────────────────────────┘
                                                        │  (03: what's inside the .class)
                                                     machine code on your CPU
```

## Suggested path

**00 → 01 → 02** is the foundation. After that, read **04 → 05** for memory behavior or **03 → 06** for execution behavior, depending on what you're debugging.

## Symptom → where to look

| Symptom | Start with |
|---|---|
| `OutOfMemoryError: Java heap space` / Metaspace / native thread | [02](02_jvm-memory-areas.md), then [04](04_garbage-collection.md) |
| Long or frequent GC pauses | [04](04_garbage-collection.md), [05](05_gc-algorithms.md) |
| `ClassNotFoundException`, `NoClassDefFoundError`, `NoSuchMethodError` | [01](01_class-loading.md) |
| Slow startup, slow first requests, benchmarks that "speed up" over time | [06](06_jit-compiler.md), [01](01_class-loading.md) |
| High CPU with little real work | [06](06_jit-compiler.md), [04](04_garbage-collection.md) |
| "What does this compile to?" / boxing or string-concat cost | [03](03_bytecode.md) |
| Container gets OOM-killed though the heap looks fine | [02](02_jvm-memory-areas.md) |

## Version snapshot (October 2026)

These notes describe **HotSpot**, the JVM in Oracle JDK and OpenJDK builds, and call out version differences. Two defaults changed recently, and both affect behavior after upgrading:

- **Java 27:** **G1 is the default GC in every environment** (JEP 523). Previously, machines with fewer than 2 CPUs or under about 1.8 GB of memory silently got the Serial collector.
- **Java 27:** **compact object headers are on by default** (JEP 534), shrinking every object's header from 12 to 8 bytes in typical configurations.

JVM options and defaults do change between releases, so verify against your JDK's documentation and `java -XX:+PrintFlagsFinal -version`.

## Prerequisites

[How Java Works](../00-setup/02_how-java-works.md), [Classes and Objects](../04-oop/00_classes-and-objects.md), and [Concurrency basics](../14-concurrency/00_threads-and-lifecycle.md) (for the memory-model connections).

## Related

[GC Tuning](../20-performance/03_gc-tuning.md) · [Memory Optimization](../20-performance/02_memory-optimization.md) · [JVM Diagnostics](../22-production-engineering/04_jvm-diagnostics.md) · [JVM interview questions](../28-interview/08_jvm.md) · [JVM flags cheatsheet](../29-cheatsheets/07_jvm-flags-and-diagnostics.md)

**Next module:** [JDBC and Databases](../16-jdbc-and-databases/README.md)