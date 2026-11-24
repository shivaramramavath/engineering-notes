---
title: "Health Checks and Readiness Probes with Terminus in NestJS"
description: "Configuring Kubernetes liveness, readiness, and startup probes using @nestjs/terminus, TypeORM, Redis, and disk indicators"
tags: [nestjs, observability, health-checks, terminus, kubernetes, liveness, readiness]
nestjs_version: "11.x"
node_version: ">=20.0.0"
express_version: "5.x"
---

# Health Checks and Readiness Probes with Terminus in NestJS

## Conceptual Overview
In containerized environments (Kubernetes, AWS ECS), the orchestrator must monitor application health to make routing and self-healing decisions:
- **Startup Probe**: Verifies that the container has initialized (useful for slow bootstrap processes).
- **Liveness Probe**: Verifies whether the Node.js process is alive. If this fails, Kubernetes restarts the pod.
- **Readiness Probe**: Verifies whether the application is capable of accepting incoming user traffic (e.g., database and Redis connections are healthy). If this fails, Kubernetes removes the pod from load balancer endpoints.

NestJS provides `@nestjs/terminus` to configure robust, standardized health check probes.

---

## Implementation Details & NestJS APIs

### 1. Terminus Health Controller

```typescript
// src/health/health.controller.ts
import { Controller, Get } from '@nestjs/common';
import {
  HealthCheckService,
  HealthCheck,
  TypeOrmHealthIndicator,
  MemoryHealthIndicator,
  DiskHealthIndicator,
} from '@nestjs/terminus';

@Controller('health')
export class HealthController {
  constructor(
    private readonly health: HealthCheckService,
    private readonly db: TypeOrmHealthIndicator,
    private readonly memory: MemoryHealthIndicator,
    private readonly disk: DiskHealthIndicator,
  ) {}

  // Liveness Probe (Lightweight: checks if process is responsive)
  @Get('liveness')
  @HealthCheck()
  checkLiveness() {
    return this.health.check([
      // Ensure V8 heap does not exceed 300MB
      () => this.memory.checkHeap('memory_heap', 300 * 1024 * 1024),
    ]);
  }

  // Readiness Probe (Deep: checks DB connectivity and disk headroom)
  @Get('readiness')
  @HealthCheck()
  checkReadiness() {
    return this.health.check([
      () => this.db.pingCheck('database', { timeout: 1500 }),
      () => this.disk.checkStorage('storage', { thresholdPercent: 0.9, path: '/' }),
    ]);
  }
}
```

---

## Key Takeaways & Cheat Sheet

- Keep **liveness probes** lightweight (do not check external databases in liveness probes; a database outage would cause all pods to restart simultaneously in a crash-loop!).
- Use **readiness probes** to check downstream dependencies (Postgres, Redis).
- Use `@nestjs/terminus` for built-in health indicators.

---

## Cross-References
- [07-kubernetes.md](../04-deployment/07-kubernetes.md) - Kubernetes deployment
- [01-performance-fundamentals.md](../02-performance/01-performance-fundamentals.md) - Golden signals
