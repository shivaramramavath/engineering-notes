# Module 11: Distributed & Cloud Patterns

When code crosses a network boundary, latency spikes, packets drop, nodes crash, and partial failures become normal. Classic in-process GoF patterns are not enough on their own—distributed systems require **resilience**, **data consistency**, **caching/routing**, and **safe deployment** patterns.

---

## Notes in This Module

| # | Topic | Focus |
| :--- | :--- | :--- |
| 00 | [Module Overview](file:///E:/Learning/engineering-notes/08-solid-design-patterns/11-distributed-and-cloud-patterns/00-overview.md) | Resilience, consistency, edge/cloud topology & reading map |
| 01 | [Circuit Breaker](file:///E:/Learning/engineering-notes/08-solid-design-patterns/11-distributed-and-cloud-patterns/01-circuit-breaker.md) | Failing fast (`CLOSED -> OPEN -> HALF-OPEN`) to stop cascading outages |
| 02 | [Retry with Backoff & Jitter](file:///E:/Learning/engineering-notes/08-solid-design-patterns/11-distributed-and-cloud-patterns/02-retry-with-backoff-and-jitter.md) | Recovering from transient faults without causing thundering herds |
| 03 | [Timeout & Deadline](file:///E:/Learning/engineering-notes/08-solid-design-patterns/11-distributed-and-cloud-patterns/03-timeout-and-deadline.md) | Bounding wait time and propagating remaining request budgets |
| 04 | [Bulkhead](file:///E:/Learning/engineering-notes/08-solid-design-patterns/11-distributed-and-cloud-patterns/04-bulkhead.md) | Isolating thread/connection pools so one slow dependency can't sink the ship |
| 05 | [Rate Limiter & Throttling](file:///E:/Learning/engineering-notes/08-solid-design-patterns/11-distributed-and-cloud-patterns/05-rate-limiter-and-throttling.md) | Token Bucket, Leaky Bucket, Sliding Window, HTTP `429` |
| 06 | [Idempotency Key & Idempotent Consumer](file:///E:/Learning/engineering-notes/08-solid-design-patterns/11-distributed-and-cloud-patterns/06-idempotency-key-and-idempotent-consumer.md) | Making retries safe under at-least-once delivery |
| 07 | [Saga Pattern](file:///E:/Learning/engineering-notes/08-solid-design-patterns/11-distributed-and-cloud-patterns/07-saga-pattern.md) | Distributed workflows via compensating actions (Choreography vs Orchestration) |
| 08 | [Transactional Outbox](file:///E:/Learning/engineering-notes/08-solid-design-patterns/11-distributed-and-cloud-patterns/08-transactional-outbox.md) | Atomic DB write + reliable event publishing without 2PC |
| 09 | [CQRS Read Models](file:///E:/Learning/engineering-notes/08-solid-design-patterns/11-distributed-and-cloud-patterns/09-cqrs-read-models.md) | Materialized projections, lag handling, and read scaling |
| 10 | [Cache-Aside & Write-Through](file:///E:/Learning/engineering-notes/08-solid-design-patterns/11-distributed-and-cloud-patterns/10-cache-aside-and-write-through.md) | Caching topologies, stampede prevention, and invalidation |
| 11 | [API Gateway & BFF](file:///E:/Learning/engineering-notes/08-solid-design-patterns/11-distributed-and-cloud-patterns/11-api-gateway-and-bff.md) | Edge routing, auth offloading, and Backend-for-Frontend aggregation |
| 12 | [Sidecar & Ambassador](file:///E:/Learning/engineering-notes/08-solid-design-patterns/11-distributed-and-cloud-patterns/12-sidecar-and-ambassador.md) | Out-of-process proxies for mTLS, observability, and service mesh |
| 13 | [Strangler Fig Migration](file:///E:/Learning/engineering-notes/08-solid-design-patterns/11-distributed-and-cloud-patterns/13-strangler-fig-migration.md) | Incrementally replacing legacy systems behind a routing proxy |
| 14 | [Anti-Corruption Layer (Distributed)](file:///E:/Learning/engineering-notes/08-solid-design-patterns/11-distributed-and-cloud-patterns/14-anti-corruption-layer.md) | Translating external/legacy contracts at service boundaries |
| 15 | [Service Discovery & Load Balancing](file:///E:/Learning/engineering-notes/08-solid-design-patterns/11-distributed-and-cloud-patterns/15-service-discovery-and-load-balancing.md) | Client-side vs server-side discovery, health-aware routing |
| 16 | [Leader Election & Distributed Lock](file:///E:/Learning/engineering-notes/08-solid-design-patterns/11-distributed-and-cloud-patterns/16-leader-election-and-distributed-lock.md) | Leases, fencing tokens, and why distributed locks are subtle |
| 17 | [Sharding & Partitioning](file:///E:/Learning/engineering-notes/08-solid-design-patterns/11-distributed-and-cloud-patterns/17-sharding-and-partitioning.md) | Consistent hashing, shard keys, and hot-partition mitigation |
| 18 | [Health Check & Graceful Degradation](file:///E:/Learning/engineering-notes/08-solid-design-patterns/11-distributed-and-cloud-patterns/18-health-check-and-graceful-degradation.md) | Liveness vs readiness probes, fallbacks, and load shedding |
| 19 | [Feature Flags & Dark Launch](file:///E:/Learning/engineering-notes/08-solid-design-patterns/11-distributed-and-cloud-patterns/19-feature-flags-and-dark-launch.md) | Decoupling deployment from release and shadow traffic testing |
| 20 | [Blue–Green & Canary Deployments](file:///E:/Learning/engineering-notes/08-solid-design-patterns/11-distributed-and-cloud-patterns/20-blue-green-and-canary.md) | Zero-downtime rollouts and progressive traffic shifting |

---

## Navigation

| Previous | Up | Next |
| :--- | :--- | :--- |
| [Module 10: Architecture](file:///E:/Learning/engineering-notes/08-solid-design-patterns/10-architecture/README.md) | [Track Index](file:///E:/Learning/engineering-notes/08-solid-design-patterns/INDEX.md) | [Module 12: Functional & Modern Idioms](file:///E:/Learning/engineering-notes/08-solid-design-patterns/12-functional-and-modern-idioms/README.md) |
