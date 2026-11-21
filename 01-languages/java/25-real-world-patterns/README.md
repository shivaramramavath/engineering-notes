# Module 25: Real-World Enterprise Patterns in Java

> Production-grade architectural patterns for building resilient, high-throughput, fault-tolerant, and scalable distributed Java services.

---

## Module Overview

Modern enterprise microservices must withstand network partitions, spiky traffic, slow dependencies, and catastrophic database outages. Module 25 presents comprehensive, end-to-end design patterns and production Java implementations for enterprise resilience, distributed data flow, and concurrency governance.

Every concept note provides:
- Mathematical formulations, algorithms, and ASCII architecture diagrams.
- Production Java SE 21/25 LTS code verified against Oracle JDK 25 LTS.
- In-depth edge cases, security mitigations, and common enterprise anti-patterns.
- 8+ interview questions with expandable answers and complete practice exercises.

---

## Topics in this Module

| Topic | Key Focus Areas | Reading Time |
| :--- | :--- | :--- |
| [`00_caching.md`](00_caching.md) | Cache-aside, Read/Write-through, Eviction (W-TinyLFU, LRU), Cache stampede, Multi-tier L1/L2 | 35 min |
| [`01_pagination.md`](01_pagination.md) | Offset pagination cliff, Keyset/Cursor-based pagination, B-Tree seeking, Opaque tokens | 35 min |
| [`02_retry-and-backoff.md`](02_retry-and-backoff.md) | Exponential backoff, Full/Decorrelated Jitter, Transient error filtering, `Retry-After` compliance | 35 min |
| [`03_rate-limiting.md`](03_rate-limiting.md) | Token Bucket, Leaky Bucket, Sliding Window Counter, Lock-free atomic math, Distributed Redis Lua | 35 min |
| [`04_idempotency.md`](04_idempotency.md) | At-least-once delivery, Idempotency keys, SHA-256 payload fingerprinting, Database deduplication | 35 min |
| [`05_background-jobs.md`](05_background-jobs.md) | Task queues, Delayed jobs, Virtual Threads, Dead Letter Queues (DLQ), Graceful shutdown | 35 min |
| [`06_circuit-breaker-and-resilience.md`](06_circuit-breaker-and-resilience.md) | CLOSED/OPEN/HALF-OPEN states, Sliding-window metrics, Bulkheads, Graceful fallbacks | 35 min |

---

## Pattern Selection Matrix

```
Requirement                                         Primary Pattern
-----------------------------------------------------------------------------------------
Reduce database read latency (< 100ns / 1ms)    --> Multi-Tier Caching (L1 Caffeine + L2 Redis)
Paginate 10M+ rows without performance cliff   --> Keyset (Cursor-Based) Pagination
Survive temporary network blips / 503s          --> Exponential Backoff with Full Jitter
Protect API from noisy neighbors / DoS floods   --> Token Bucket / Sliding Window Rate Limiting
Prevent duplicate billing on client retries    --> Idempotency Keys with Deduplication Table
Offload 10-second PDF or image generation       --> Background Job Engine with Virtual Threads
Prevent cascading outages from failing upstream --> Circuit Breaker & Bulkhead Isolation
```
