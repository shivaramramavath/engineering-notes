# Module 15: Node.js Performance, Profiling & Optimization

> Measuring event loop health, diagnosing memory leaks, CPU flamegraphs, database tuning, and load testing.

---

## Module Overview

Node.js delivers immense throughput when asynchronous I/O is respected, but synchronous compute or uncollected object references will quickly degrade performance. This module provides actionable methodologies to profile, diagnose, and optimize production Node.js applications.

---

## Contents

| Index | Note | Core Focus |
| :---: | :--- | :--- |
| **01** | [01-event-loop-performance.md](./01-event-loop-performance.md) | `perf_hooks.monitorEventLoopDelay()`, event loop starvation, threadpool sizing. |
| **02** | [02-profiling-and-memory-leaks.md](./02-profiling-and-memory-leaks.md) | CPU flamegraphs with `0x`, `clinic doctor`, V8 GC trace flags, heap snapshots. |
| **03** | [03-database-optimization.md](./03-database-optimization.md) | Connection pool sizing formulas, multi-level caching (L1 In-Memory + L2 Redis). |
| **04** | [04-load-balancing-and-testing.md](./04-load-balancing-and-testing.md) | Autocannon load testing, p99 latency percentiles, Nginx upstream keep-alive. |

---

## Next Steps
Proceed to [16-production](../16-production/README.md) to explore containerization, graceful shutdown, and zero-downtime deployment patterns.
