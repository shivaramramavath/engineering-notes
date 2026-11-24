---
title: "Memory and CPU Optimization in NestJS: V8 Profiling and Leak Detection"
description: "Diagnosing V8 heap allocations, memory leaks, garbage collection pauses, and CPU profiling using clinic.js and Chrome DevTools"
tags: [nestjs, performance, memory-leaks, v8, profiling, clinicjs, flamegraphs]
nestjs_version: "11.x"
node_version: ">=20.0.0"
express_version: "5.x"
---

# Memory and CPU Optimization in NestJS: V8 Profiling and Leak Detection

## Conceptual Overview
Node.js memory management is handled automatically by the V8 Garbage Collector (Orinoco / Scavenger / Major Mark-Sweep). However, lingering object references in global variables, unbounded caches, unclosed event listeners, and closure scopes prevent V8 from reclaiming memory, culminating in process crashes via `FATAL ERROR: Ineffective mark-compacts near heap limit Allocation failed - JavaScript heap out of memory`.

This guide details methodologies to detect, isolate, and eliminate memory leaks and CPU hotspots in NestJS applications.

---

## Core Mechanisms & Architecture

### 1. Common Memory Leak Sources in NestJS
- **Unbounded In-Memory Collections**: Storing user sessions or rate limits in raw JavaScript `Map` or `Set` objects without TTL or LRU eviction policies.
- **Dangling Event Listeners**: Attaching `emitter.on('event', ...)` inside request handlers without removing listeners upon response completion.
- **Request Scope Bubble-Up**: Singletons inadvertently becoming request-scoped, creating thousands of short-lived objects that overload the Scavenger GC.
- **Unclosed Streams & Timers**: Active `setInterval()` references holding closures in memory indefinitely.

---

## Implementation Details & NestJS APIs

### 1. Programmatic Heap Snapshot Generation

```typescript
// src/diagnostics/heap-profiler.service.ts
import { Injectable, Logger } from '@nestjs/common';
import { writeHeapSnapshot } from 'node:v8';
import { join } from 'node:path';

@Injectable()
export class HeapProfilerService {
  private readonly logger = new Logger(HeapProfilerService.name);

  takeSnapshot(label: string = 'manual'): string {
    const filename = `heap-${label}-${Date.now()}.heapsnapshot`;
    const filepath = join(process.cwd(), 'snapshots', filename);

    const savedPath = writeHeapSnapshot(filepath);
    this.logger.warn(`Heap snapshot generated: ${savedPath}`);
    return savedPath;
  }
}
```

---

## Key Takeaways & Cheat Sheet

- Never use raw in-memory Maps for caching; use Redis or bounded LRU caches (`lru-cache`).
- Avoid CPU-bound work on the V8 main thread; offload to Worker Threads.
- Use `clinic doctor` and `clinic flame` to isolate CPU hotspots and I/O bottlenecks.

---

## Cross-References
- [05-clustering-and-worker-processes.md](./05-clustering-and-worker-processes.md) - Worker threads
- [01-performance-fundamentals.md](./01-performance-fundamentals.md) - Performance metrics
