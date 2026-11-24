---
title: "Distributed Locks with Redis (Redlock) in NestJS"
description: "Preventing race conditions across multi-pod NestJS clusters: Redlock algorithm, lease timeouts, and leader election"
tags: [nestjs, patterns, distributed-lock, redis, redlock, race-conditions]
nestjs_version: "11.x"
node_version: ">=20.0.0"
express_version: "5.x"
---

# Distributed Locks with Redis (Redlock) in NestJS

## Conceptual Overview
When multiple NestJS application pods run concurrently, local in-memory mutexes cannot prevent race conditions. Operations like processing a single payment, running scheduled cron jobs, or updating inventory require **Distributed Locks**.

The **Redlock Algorithm** in Redis provides multi-instance distributed mutual exclusion with TTL lease timeouts to prevent deadlocks if a node crashes while holding a lock.

---

## Implementation Details: Redlock Decorator

```typescript
// src/common/decorators/distributed-lock.decorator.ts
import Redlock from 'redlock';
import { redisClient } from '../redis/client';

const redlock = new Redlock([redisClient], { retryCount: 3, retryDelay: 200 });

export function DistributedLock(resourceKey: string, ttlMs: number = 5000) {
  return function (target: any, propertyKey: string, descriptor: PropertyDescriptor) {
    const originalMethod = descriptor.value;

    descriptor.value = async function (...args: any[]) {
      const lock = await redlock.acquire([resourceKey], ttlMs);
      try {
        return await originalMethod.apply(this, args);
      } finally {
        await lock.release();
      }
    };
  };
}
```

---

## Key Takeaways & Cheat Sheet

- Use Redis-backed distributed locks to serialize critical multi-pod operations.
- Always configure a TTL lease to prevent permanent deadlocks on container crashes.
- Ensure lock release occurs in a `finally` block.

---

## Cross-References
- [05-retry-and-circuit-breaker.md](./05-retry-and-circuit-breaker.md) - Resilience
- [08-multi-tenancy.md](./08-multi-tenancy.md) - Multi-tenancy
