# Proven Pattern Combinations & Composites

| Domain Scenario | Proven Pattern Stack |
| :--- | :--- |
| **Clean Backend Service** | Hexagonal Ports & Adapters + Repository + Unit of Work + DTO + Service Layer |
| **Resilient Microservice Client** | Gateway (ACL) + Timeout + Circuit Breaker + Retry (Backoff + Jitter) + Bulkhead |
| **Reliable Event Publishing** | Aggregate + Domain Events + Transactional Outbox + Idempotent Consumer |
| **Extensible SaaS / Engine** | Microkernel + Plugin Registry + Strategy + Facade |
| **Undoable Editor / Workflow** | Command + Memento (or Immutable Snapshots) + State Machine |
| **High-Read Scale API** | CQRS Read Models + Cache-Aside (with TTL Jitter + Singleflight) |
