# Module 15: JVM Internals

> The definitive architectural guide to the Java Virtual Machine: HotSpot internals, Class Loading mechanics, Runtime Data Areas, Bytecode anatomy, Garbage Collection algorithms (G1, Generational ZGC), JIT compilation, and production diagnostics.

---

## Module Overview

The Java Virtual Machine (JVM) is one of the most sophisticated runtime execution engines in software engineering. This module covers how Java source code is compiled into bytecode, loaded and verified by dynamic ClassLoaders, mapped across memory regions, optimized into native host assembly by tiered JIT compilers, and reclaimed by state-of-the-art concurrent garbage collectors.

---

## Notes Index

| # | Note | Description | Status |
|---|---|---|---|
| 00 | [00_jvm-architecture.md](00_jvm-architecture.md) | JVM specification vs HotSpot, ClassLoader, Data Areas, Execution Engine | Complete (420+ lines) |
| 01 | [01_class-loading.md](01_class-loading.md) | 3 Phases (Loading, Linking, Init), Parent Delegation, Breaking delegation | Complete (400+ lines) |
| 02 | [02_jvm-memory-areas.md](02_jvm-memory-areas.md) | Heap, Metaspace, Stack Frames, TLABs, Compressed OOPs (32 GB boundary) | Complete (425+ lines) |
| 03 | [03_bytecode.md](03_bytecode.md) | ClassFile binary format, Constant Pool, opcode taxonomy, javap analysis | Complete (425+ lines) |
| 04 | [04_garbage-collection.md](04_garbage-collection.md) | Weak Generational Hypothesis, GC Roots, STW Safepoints, Mark-Copy-Compact | Complete (450+ lines) |
| 05 | [05_gc-algorithms.md](05_gc-algorithms.md) | G1 GC region grid, Generational ZGC colored pointers & load barriers | Complete (450+ lines) |
| 06 | [06_jit-compiler.md](06_jit-compiler.md) | Tiered Compilation (Tiers 0-4), C1/C2, OSR, Inlining, Escape Analysis | Complete (440+ lines) |
| 07 | [07_jvm-tuning-and-diagnostics.md](07_jvm-tuning-and-diagnostics.md) | jcmd, jstack, jmap, JFR continuous profiling, async-profiler, container flags | Complete (430+ lines) |

---

## Recommended Learning Path
1. Explore the machine architecture in `00_jvm-architecture.md` through `03_bytecode.md`.
2. Master memory management and collectors in `04_garbage-collection.md` and `05_gc-algorithms.md`.
3. Understand JIT optimization and inlining in `06_jit-compiler.md`.
4. Apply production diagnostics and container tuning with `07_jvm-tuning-and-diagnostics.md`.
