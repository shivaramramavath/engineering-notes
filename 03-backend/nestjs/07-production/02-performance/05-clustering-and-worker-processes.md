---
title: "Clustering and Worker Processes in NestJS"
description: "Scaling NestJS across multi-core systems using Node.js cluster, Piscina worker thread pools, and PM2 process management"
tags: [nestjs, performance, clustering, worker-threads, multi-core, piscina]
nestjs_version: "11.x"
node_version: ">=20.0.0"
express_version: "5.x"
---

# Clustering and Worker Processes in NestJS

## Conceptual Overview
Because Node.js executes JavaScript on a single thread, a NestJS instance running on an 8-core CPU server utilizes only 12.5% of the machine's processing capacity. To harness modern multi-core infrastructure, applications must scale horizontally across physical CPU cores.

Two distinct architectural approaches exist:
1. **Process-Level Clustering (`cluster` module / PM2)**: Spawning multiple independent Node.js processes sharing the same TCP listening port.
2. **Thread-Level Concurrency (`worker_threads` / Piscina)**: Spawning lightweight threads sharing process memory to execute CPU-intensive computations without blocking the event loop.

---

## Implementation Details & NestJS APIs

### 1. Multi-Core Process Clustering

```typescript
// src/cluster.ts
import cluster from 'node:cluster';
import { availableParallelism } from 'node:os';
import { Logger } from '@nestjs/common';

const logger = new Logger('ClusterManager');

if (cluster.isPrimary) {
  const numCPUs = availableParallelism();
  logger.log(`Primary cluster process ${process.pid} is running. Forking ${numCPUs} workers...`);

  for (let i = 0; i < numCPUs; i++) {
    cluster.fork();
  }

  cluster.on('exit', (worker, code, signal) => {
    logger.warn(`Worker ${worker.process.pid} died (signal: ${signal}). Spawning replacement...`);
    cluster.fork();
  });
} else {
  // Worker process runs standard NestJS bootstrap
  require('./main');
}
```

### 2. Offloading CPU Tasks with Worker Threads (Piscina)

```typescript
// src/heavy-math/math-pool.service.ts
import { Injectable, OnModuleDestroy } from '@nestjs/common';
import Piscina from 'piscina';
import { join } from 'node:path';

@Injectable()
export class MathPoolService implements OnModuleDestroy {
  private readonly pool: Piscina;

  constructor() {
    this.pool = new Piscina({
      filename: join(__dirname, 'worker.js'),
      maxThreads: 4,
    });
  }

  async calculatePrimes(max: number): Promise<number[]> {
    return this.pool.run({ max });
  }

  async onModuleDestroy() {
    await this.pool.destroy();
  }
}
```

---

## Key Takeaways & Cheat Sheet

- Use Node.js clustering or container replication to utilize all CPU cores.
- Offload heavy CPU tasks (PDF generation, cryptographic hashing, image compression) to Worker Threads.
- In Kubernetes environments, scale via single-threaded pods rather than in-pod clustering.

---

## Cross-References
- [04-memory-and-cpu-optimization.md](./04-memory-and-cpu-optimization.md) - CPU optimization
- [07-kubernetes.md](../04-deployment/07-kubernetes.md) - Kubernetes horizontal scaling
