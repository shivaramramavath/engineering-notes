---
title: "Diagnosing Memory Leaks and Heap Snapshots in Production"
description: "Capturing, inspecting, and comparing V8 heap snapshots in live production NestJS containers to isolate memory leaks"
tags: [nestjs, operations, memory-leaks, heap-snapshots, v8, chrome-devtools]
nestjs_version: "11.x"
node_version: ">=20.0.0"
express_version: "5.x"
---

# Diagnosing Memory Leaks and Heap Snapshots in Production

## Conceptual Overview
When a NestJS pod displays a sawtooth memory graph where memory climbs steadily without stabilizing, the process is suffering from a memory leak that will eventually trigger an `OOMKilled` event.

Isolating a leak requires:
1. Generating a baseline heap snapshot upon container boot.
2. Generating a second snapshot after servicing 50,000 requests.
3. Comparing snapshots in **Chrome DevTools Memory Profiler** to identify objects with expanding retained sizes.

---

## Implementation Details: On-Demand Heap Snapshot Trigger

```typescript
// src/diagnostics/snapshot.controller.ts
import { Controller, Post, UseGuards, ForbiddenException } from '@nestjs/common';
import { writeHeapSnapshot } from 'node:v8';
import { join } from 'node:path';

@Controller('admin/diagnostics')
export class SnapshotController {
  @Post('heap-snapshot')
  captureSnapshot() {
    // Restrict access to localhost or secure internal VPC
    const filepath = join('/tmp', `heap-${Date.now()}.heapsnapshot`);
    const written = writeHeapSnapshot(filepath);
    return { status: 'success', path: written };
  }
}
```

---

## Key Takeaways & Cheat Sheet

- Use `writeHeapSnapshot` from `node:v8` to dump heap states.
- Inspect the **Comparison** view in Chrome DevTools to see which constructor counts increased.
- Look for large arrays in singleton services, dangling event listeners, or leaked closures.

---

## Cross-References
- [04-memory-and-cpu-optimization.md](../02-performance/04-memory-and-cpu-optimization.md) - Memory optimization
- [03-cpu-profiling-and-flamegraphs.md](./03-cpu-profiling-and-flamegraphs.md) - CPU profiling
