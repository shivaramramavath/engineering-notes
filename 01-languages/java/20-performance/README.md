# 20 · Performance and Optimization

High performance is not an accident; it is the predictable outcome of deliberate design, mechanical sympathy with hardware caches, and scientific profiling. This module covers production JVM performance engineering: from low-overhead profiling and microbenchmarking to memory layout optimization, garbage collection tuning, and eliminating common latency anti-patterns.

## Contents

| # | Note | Focus |
|---|------|-------|
| 00 | [JVM Profiling](00_profiling.md) | Async-profiler, JFR, JMC, Flame Graphs, Safepoint Bias, Wall-clock profiling |
| 01 | [Benchmarking with JMH](01_benchmarking-with-jmh.md) | JMH Harness, JIT Dead Code Elimination, Constant Folding, Blackholes |
| 02 | [Memory Optimization](02_memory-optimization.md) | Object headers, Compressed OOPs, False Sharing (@Contended), Off-heap FFM |
| 03 | [Garbage Collection Tuning](03_gc-tuning.md) | G1, Generational ZGC, Allocation vs Promotion rates, Unified GC Logging |
| 04 | [Concurrency Performance](04_concurrency-performance.md) | Lock contention, LongAdder striping, VarHandle, Virtual Thread pinning |
| 05 | [Common Performance Pitfalls](05_common-performance-pitfalls.md) | String concatenation in loops, Autoboxing churn, Regex recompilation |

---
**Next module:** [Security](../21-security/README.md)
