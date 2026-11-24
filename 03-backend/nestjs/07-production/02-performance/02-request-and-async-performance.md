---
title: "Request and Asynchronous Performance in NestJS"
description: "Optimizing asynchronous code, Promise concurrency, streaming responses, and JSON serialization in high-throughput NestJS APIs"
tags: [nestjs, performance, async, promise-concurrency, streaming, fast-json]
nestjs_version: "11.x"
node_version: ">=20.0.0"
express_version: "5.x"
---

# Request and Asynchronous Performance in NestJS

## Conceptual Overview
In an asynchronous runtime, performance bottlenecks rarely stem from external network speeds—they arise from sequential `await` anti-patterns, unnecessary serialization allocations, and blocking synchronous computations.

Optimizing request performance involves:
- **Promise Concurrency**: Parallelizing independent I/O tasks via `Promise.all()`.
- **Fast Serialization**: Schema-compiled JSON serialization using Fastify or `fast-json-stringify`.
- **Chunked File & Data Streaming**: Piping large payloads directly to network sockets without buffering in RAM.

---

## Core Mechanisms & Architecture

### 1. Sequential vs Parallel Asynchronous Execution

```typescript
// ANTI-PATTERN: Sequential awaits (Total time = 100ms + 100ms + 100ms = 300ms)
const user = await this.usersService.findById(id);
const orders = await this.ordersService.findByUserId(id);
const notifications = await this.notifyService.getUnread(id);

// OPTIMIZED: Parallel execution (Total time = max(100ms, 100ms, 100ms) = 100ms)
const [user, orders, notifications] = await Promise.all([
  this.usersService.findById(id),
  this.ordersService.findByUserId(id),
  this.notifyService.getUnread(id),
]);
```

### 2. High-Performance Streaming
When exporting 100,000 database rows as CSV or JSON, buffering all records in memory causes V8 heap spikes and blocks the event loop. Streaming transmits data continuously:

```typescript
// src/exports/exports.controller.ts
import { Controller, Get, Res } from '@nestjs/common';
import { Response } from 'express';
import { Transform } from 'node:stream';

@Controller('exports')
export class ExportsController {
  constructor(private readonly repo: LargeDataRepository) {}

  @Get('csv')
  async exportCsv(@Res() res: Response) {
    res.setHeader('Content-Type', 'text/csv');
    res.setHeader('Content-Disposition', 'attachment; filename="data.csv"');

    const dbStream = await this.repo.getStream();
    const csvTransform = new Transform({
      objectMode: true,
      transform(chunk, encoding, callback) {
        callback(null, `${chunk.id},${chunk.name},${chunk.created_at}
`);
      },
    });

    dbStream.pipe(csvTransform).pipe(res);
  }
}
```

---

## Key Takeaways & Cheat Sheet

- Parallelize independent async calls with `Promise.all()` or `Promise.allSettled()`.
- Stream large datasets directly to HTTP response sockets; never buffer large arrays in memory.
- Prefer Fastify with compiled JSON schemas when maximum serialization throughput is required.

---

## Cross-References
- [01-performance-fundamentals.md](./01-performance-fundamentals.md) - Golden signals
- [03-database-performance.md](./03-database-performance.md) - Database query tuning
