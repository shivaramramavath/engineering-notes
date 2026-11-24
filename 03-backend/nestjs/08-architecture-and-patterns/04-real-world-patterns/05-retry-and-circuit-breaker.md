---
title: "Retry and Circuit Breaker Patterns in NestJS"
description: "Resilience engineering: exponential backoff with jitter, Cockatiel/Opossum circuit breakers, and bulkhead isolation in NestJS"
tags: [nestjs, patterns, circuit-breaker, retry, exponential-backoff, jitter, cockatiel]
nestjs_version: "11.x"
node_version: ">=20.0.0"
express_version: "5.x"
---

# Retry and Circuit Breaker Patterns in NestJS

## Conceptual Overview
In distributed systems, downstream network failures, cold starts, and momentary rate-limiting are inevitable. Naive immediate retries flood struggling downstream services in a thundering herd.

Resilient backends implement:
- **Exponential Backoff with Full Jitter**: Randomizing retry intervals to prevent synchronized retry spikes.
- **Circuit Breaker**: Halting outbound requests when failure thresholds are exceeded, failing fast and protecting downstream services.

---

## Implementation Details with Cockatiel

```typescript
// src/common/resilience/resilience.service.ts
import { Injectable } from '@nestjs/common';
import {
  circuitBreaker,
  retry,
  handleAll,
  wrap,
  ConsecutiveBreaker,
  ExponentialBackoff,
} from 'cockatiel';

@Injectable()
export class ResilienceService {
  private readonly retryPolicy = retry(handleAll, {
    maxAttempts: 3,
    backoff: new ExponentialBackoff({ initialDelay: 200, maxDelay: 2000 }),
  });

  private readonly breakerPolicy = circuitBreaker(handleAll, {
    halfOpenAfter: 10 * 1000,
    breaker: new ConsecutiveBreaker(5),
  });

  private readonly resilientPipeline = wrap(this.retryPolicy, this.breakerPolicy);

  async execute<T>(fn: () => Promise<T>): Promise<T> {
    return this.resilientPipeline.execute(fn);
  }
}
```

---

## Key Takeaways & Cheat Sheet

- Always add randomized jitter to exponential backoff retries.
- Use Circuit Breakers to fail fast when downstream services are offline.
- Fallback to cached data or graceful degradation when the circuit is Open.

---

## Cross-References
- [05-ai-architecture.md](../../05-advanced/08-ai-and-llm/05-ai-architecture.md) - LLM failover
- [06-distributed-lock.md](./06-distributed-lock.md) - Distributed lock
