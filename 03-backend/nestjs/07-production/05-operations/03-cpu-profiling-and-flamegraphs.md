---
title: "CPU Profiling and Flamegraphs in Production NestJS"
description: "Generating V8 CPU profiles, flamegraphs, and diagnosing event loop blocking functions under live traffic"
tags: [nestjs, operations, cpu-profiling, flamegraph, clinicjs, v8-profiler]
nestjs_version: "11.x"
node_version: ">=20.0.0"
express_version: "5.x"
---

# CPU Profiling and Flamegraphs in Production NestJS

## Conceptual Overview
When CPU utilization reaches 100% and event loop lag exceeds 200ms, the main V8 thread is executing synchronous computations. **Flamegraphs** visualize call stacks over time, where horizontal width corresponds directly to CPU time consumed by individual functions.

---

## Generating Flamegraphs with Clinic.js

```bash
# 1. Start NestJS service with clinic flame profiler
npx clinic flame -- node dist/main.js

# 2. In another terminal, generate load with autocannon
npx autocannon -c 50 -d 20 http://localhost:3000/v1/orders

# 3. Stop the process (Ctrl+C). Clinic generates a flamegraph HTML report.
```

---

## Key Takeaways & Cheat Sheet

- Wide bars in a flamegraph represent functions consuming the most CPU time.
- Identify unexpected synchronous work (regex backtracking, bcrypt, large JSON stringifying).
- Offload identified heavy CPU algorithms to Worker Threads.

---

## Cross-References
- [05-clustering-and-worker-processes.md](../02-performance/05-clustering-and-worker-processes.md) - Worker threads
- [02-diagnosing-memory-leaks-and-heap-snapshots.md](./02-diagnosing-memory-leaks-and-heap-snapshots.md) - Memory leaks
