# Architecture & Distributed Patterns Q&A

<details><summary>1. Modular Monolith vs Microservices: how do you decide?</summary>
Start with a **Modular Monolith** (strict vertical module boundaries, separate DB schemas, enforced via CI Fitness Functions). Extract a microservice only when you hit independent scaling/hardware needs, large-team deployment contention, or strict regulatory isolation.
</details>

<details><summary>2. What is the Dual-Write problem and how does Transactional Outbox solve it?</summary>
Updating Postgres and publishing to Kafka are non-atomic—crashing in between loses events. Writing the event to an `outbox` table inside the **exact same ACID transaction** and relaying it via CDC/poller guarantees at-least-once delivery.
</details>

<details><summary>3. Explain Circuit Breaker + Retry + Bulkhead + Timeout together.</summary>
**Timeout** bounds each call; **Retry + Jitter** recovers from brief transient glitches (for idempotent ops); **Circuit Breaker** stops calling a persistently dead service; **Bulkhead** isolates thread/connection pools so one slow service doesn't starve others.
</details>

<details><summary>4. Saga Pattern vs 2-Phase Commit (2PC)?</summary>
2PC locks rows across distributed databases and stalls if the coordinator fails. Saga runs local ACID transactions sequentially and uses idempotent **compensating transactions** in reverse if a later step fails.
</details>

<details><summary>5. Why shouldn't a Kubernetes Liveness probe check PostgreSQL?</summary>
If Postgres has a 10-second hiccup, failing Liveness kills and restarts all app pods simultaneously, turning a brief blip into a prolonged outage. Check DB health in **Readiness**, not Liveness.
</details>
