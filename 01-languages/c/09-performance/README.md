# Module 09: Performance & Optimization in C

> Master CPU microarchitecture optimization, cache hierarchy exploitation, SIMD vectorization, hardware performance profiling, branchless programming, memory layout strategies (SoA vs AoS), concurrency tuning, and profile-guided compiler optimizations.

| Metadata | Details |
| :--- | :--- |
| **Module Code** | `09-performance` |
| **Prerequisites** | [02-core-concepts/03-pointers.md](../02-core-concepts/03-pointers.md), [02-core-concepts/06-structs-unions-enums.md](../02-core-concepts/06-structs-unions-enums.md), [05-systems-programming/05-pthreads.md](../05-systems-programming/05-pthreads.md) |
| **Duration** | ~30 hours |
| **Status** | In Progress (Batch 21 Part 3) |
| **Standard Baseline**| C99 / C11 / C17 / POSIX.1-2008 / GCC 9+ / Clang 12+ / x86_64 / ARMv8 |

---

## Overview & Learning Outcomes

C is the language of choice when raw, predictable execution speed and low latency are mission-critical. However, modern CPUs are not simple sequential state machines; they are massively parallel, superscalar, out-of-order execution engines with deep memory cache hierarchies, branch predictors, and vector execution units.

Writing high-performance C requires understanding how source code maps to physical hardware execution:
1. **Memory Hierarchy & Cache Locality**: Exploiting 64-byte CPU cache lines, L1/L2/L3 architectures, hardware prefetchers, and avoiding memory latency stalls via spatial and temporal locality.
2. **Scientific Profiling**: Measuring performance using Linux `perf`, CPU hardware performance monitoring units (PMUs), Callgrind, and flame graphs to identify true execution bottlenecks rather than guessing.
3. **Compiler Optimization Engine**: Leveraging GCC/Clang optimization flags (`-O2`, `-O3`, `-flto`), understanding vectorizer heuristics, loop unrolling, and function inlining trade-offs.
4. **Branch Prediction & Data Layout**: Eliminating pipeline flushes through branchless arithmetic, branch prediction hints (`__builtin_expect`), and sorting data for branch prediction accuracy.
5. **Data-Level Parallelism (SIMD)**: Leveraging SSE, AVX2, AVX-512, and ARM NEON vector extensions to compute multiple data points per single clock cycle.
6. **Data-Oriented Design (SoA vs AoS)**: Structuring data for cache saturation by migrating from Array-of-Structures (AoS) to Structure-of-Arrays (SoA).
7. **Concurrency Performance & False Sharing**: Eliminating multi-core cache line bouncing and false sharing via explicit cache line alignment (`alignas(64)`).
8. **Advanced Build Optimizations**: Employing Profile-Guided Optimization (PGO) and Link-Time Optimization (LTO) to optimize binary layout based on real-world execution profiles.

---

## Module Table of Contents

| Note | Title | Level | Status | Focus Areas |
| :--- | :--- | :--- | :--- | :--- |
| **01** | [Cache and Memory Hierarchy](./01-cache-and-memory-hierarchy.md) | Advanced | Complete | L1/L2/L3 cache lines, spatial/temporal locality, matrix traversal order |
| **02** | [Profiling with Perf, Gprof & Callgrind](./02-profiling-perf-gprof-valgrind-callgrind.md) | Advanced | Complete | PMU hardware counters, sampling profilers, flame graphs, hot spot analysis |
| **03** | [Compiler Optimizations](./03-compiler-optimizations.md) | Advanced | Complete | Optimization flags (`-O2`, `-O3`), vectorization passes, loop unrolling |
| **04** | [Branch Prediction and Data Layout](./04-branch-prediction-and-data-layout.md) | Advanced | Complete | Branch predictors, pipeline bubbles, branchless programming, `likely`/`unlikely` |
| **05** | [SIMD Intrinsics Intro](./05-simd-intrinsics-intro.md) | Advanced | Complete | AVX/SSE/NEON vector types, data parallelism, packed vector math |
| **06** | [Benchmarking Correctly](./06-benchmarking-correctly.md) | Advanced | Planned | Monotonic clocks, warm-up runs, statistical variance, compiler optimization clobbers |
| **07** | [Struct of Arrays vs Array of Structs](./07-struct-of-arrays-vs-array-of-structs.md) | Advanced | Planned | AoS vs SoA memory layout, SIMD vectorization friendliness, cache utilization |
| **08** | [False Sharing and Concurrency Tuning](./08-false-sharing-and-concurrency-performance.md) | Advanced | Planned | Multi-core MESI protocol, cache line bouncing, `alignas(64)` padding |
| **09** | [Link-Time Optimization (LTO) & PGO](./09-link-time-optimization-and-pgo.md) | Advanced | Planned | Cross-TU inlining, feedback-directed optimization (`-fprofile-generate`), branch layout |
| **10** | [Inlining and Code Size Trade-offs](./10-inlining-and-code-size.md) | Advanced | Planned | `inline` vs `__attribute__((always_inline))`, instruction cache pressure, `-Os` |

---

## Suggested Study Order
1. **Hardware Foundations**: Start with [Cache and Memory Hierarchy](./01-cache-and-memory-hierarchy.md) and [Branch Prediction](./04-branch-prediction-and-data-layout.md).
2. **Measurement & Analysis**: Master [Profiling](./02-profiling-perf-gprof-valgrind-callgrind.md) and [Benchmarking Correctly](./06-benchmarking-correctly.md).
3. **Data-Level Parallelism**: Study [SIMD Intrinsics](./05-simd-intrinsics-intro.md) and [Struct of Arrays vs Array of Structs](./07-struct-of-arrays-vs-array-of-structs.md).
4. **Compiler & System Tuning**: Finish with [Compiler Optimizations](./03-compiler-optimizations.md), [False Sharing](./08-false-sharing-and-concurrency-performance.md), [LTO & PGO](./09-link-time-optimization-and-pgo.md), and [Inlining Trade-offs](./10-inlining-and-code-size.md).

---

[← Previous Module: Security & Safety](../08-security-and-safety/README.md) | [C Language Track Index](../README.md) | [Next Module: Design & Patterns](../10-design-and-patterns/README.md)
