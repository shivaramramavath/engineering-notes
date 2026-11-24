---
title: "Graceful Shutdown and Process Management in NestJS"
description: "Handling SIGTERM/SIGINT, draining active HTTP connections, closing database pools, and NestJS 11 reversed shutdown order"
tags: [nestjs, deployment, graceful-shutdown, sigterm, lifecycle, process-management]
nestjs_version: "11.x"
node_version: ">=20.0.0"
express_version: "5.x"
---

# Graceful Shutdown and Process Management in NestJS

## Conceptual Overview
When an application container is terminated (e.g. during a Kubernetes rolling update or auto-scaling down event), the host sends a `SIGTERM` signal. If an application exits abruptly, active user HTTP requests are dropped, database transactions remain uncommitted, and message queue consumer acknowledgments are lost.

Graceful shutdown ensures:
1. The HTTP server stops accepting new connections.
2. In-flight requests are allowed to finish within a grace period.
3. Long-running queue workers complete their current jobs.
4. Database connection pools, Redis clients, and microservice transporters disconnect cleanly.

---

## Core Mechanisms & Architecture

### 1. NestJS 11 Reversed Shutdown Order
In NestJS 11, shutdown hooks execute in **reverse topological order** compared to bootstrap:
1. `onModuleDestroy()`
2. `beforeApplicationShutdown(signal)`
3. `onApplicationShutdown(signal)`

This guarantees that consumer services drain before the database connection pools they depend upon are closed.

---

## Implementation Details & NestJS APIs

### 1. Enabling Shutdown Hooks in `main.ts`

```typescript
// src/main.ts
import { NestFactory } from '@nestjs/core';
import { AppModule } from './app.module';

async function bootstrap() {
  const app = await NestFactory.create(AppModule);

  // CRITICAL: Explicitly listen for system termination signals
  app.enableShutdownHooks();

  await app.listen(3000);
}
bootstrap();
```

### 2. Implementing Lifecycle Hooks in Services

```typescript
// src/database/database.service.ts
import { Injectable, OnModuleDestroy, BeforeApplicationShutdown, Logger } from '@nestjs/common';
import { DataSource } from 'typeorm';

@Injectable()
export class DatabaseService implements OnModuleDestroy, BeforeApplicationShutdown {
  private readonly logger = new Logger(DatabaseService.name);

  constructor(private readonly dataSource: DataSource) {}

  beforeApplicationShutdown(signal?: string) {
    this.logger.log(`Received signal [${signal}]. Draining database queries...`);
  }

  async onModuleDestroy() {
    this.logger.log('Closing database connection pool.');
    if (this.dataSource.isInitialized) {
      await this.dataSource.destroy();
    }
  }
}
```

---

## Key Takeaways & Cheat Sheet

- Always call `app.enableShutdownHooks()` in `main.ts`.
- In NestJS 11, shutdown hooks run in reverse dependency order.
- Allow Kubernetes `terminationGracePeriodSeconds` (default 30s) sufficient time for cleanup.

---

## Cross-References
- [01-how-nest-boots.md](../../06-internals/01-how-nest-boots.md) - Bootstrap and lifecycle hooks
- [07-kubernetes.md](./07-kubernetes.md) - Kubernetes lifecycle management
