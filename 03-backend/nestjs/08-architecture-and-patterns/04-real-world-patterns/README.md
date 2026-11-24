# Module 08.4: Real-World Enterprise Patterns

Welcome to the **Real-World Patterns** submodule. This track covers battle-tested architectures required in production SaaS and distributed enterprise platforms.

---

## Submodule Overview

| File | Title | Description |
| :--- | :--- | :--- |
| [01-audit-logging.md](./01-audit-logging.md) | **Audit Logging** | Tamper-evident, compliance-ready audit trails (SOC 2, HIPAA). |
| [02-soft-delete.md](./02-soft-delete.md) | **Soft Delete Pattern** | `deleted_at` timestamps, partial unique indexes, and cascade deletes. |
| [03-transactional-outbox.md](./03-transactional-outbox.md) | **Transactional Outbox** | Solving the dual-write problem with atomic database outboxes. |
| [04-saga.md](./04-saga.md) | **Saga Pattern** | Managing distributed transactions with compensating actions. |
| [05-retry-and-circuit-breaker.md](./05-retry-and-circuit-breaker.md) | **Retry & Circuit Breaker** | Exponential backoff, jitter, and Cockatiel resilience. |
| [06-distributed-lock.md](./06-distributed-lock.md) | **Distributed Lock (Redlock)** | Redis-backed distributed locks and race condition prevention. |
| [07-full-text-search.md](./07-full-text-search.md) | **Full-Text Search** | PostgreSQL tsvector/GIN indexing, Elasticsearch, and MeiliSearch. |
| [08-multi-tenancy.md](./08-multi-tenancy.md) | **Multi-Tenancy Architectures** | SaaS isolation models, PostgreSQL Row-Level Security (RLS). |
