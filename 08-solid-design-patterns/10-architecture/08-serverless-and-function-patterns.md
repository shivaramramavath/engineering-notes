# Serverless & Function-as-a-Service (FaaS) Patterns

| Field | Value |
| :--- | :--- |
| **Category** | Software Architecture (Cloud-Native) |
| **Origin** | Cloud FaaS (AWS Lambda, Cloud Functions, Cloudflare Workers) |
| **Difficulty** | Intermediate |
| **Prerequisite(s)** | [Dependency Injection in Functional Style](file:///E:/Learning/engineering-notes/08-solid-design-patterns/12-functional-and-modern-idioms/08-dependency-injection-in-functional-style.md), [Event-Driven Architecture](file:///E:/Learning/engineering-notes/08-solid-design-patterns/10-architecture/05-event-driven.md) |
| **Status** | Complete |

> **TL;DR**: Serverless FaaS executes **stateless, event-triggered functions** that scale from zero to thousands automatically—requiring careful attention to **cold starts**, **DB connection pooling**, **idempotency**, and **avoiding "Lambda Pinball"**.

---

## 1. The Problem: Why Does Serverless Exist?

Managing EC2 instances or Kubernetes clusters for spiky, event-driven workloads (image thumbnailing on S3 upload, webhook receivers, nightly cron jobs) wastes idle compute and ops effort. Serverless lets you deploy pure handler functions and pay only per millisecond of execution.

---

## 2. Core Serverless Patterns

1. **Stateless Execution & Externalized State**: Local memory/disk disappears between invocations—store state in DynamoDB/Redis/S3.
2. **Init-Outside-Handler (Warm Reuse)**: Initialize DB clients/SDKs at module scope so warm container invocations reuse them.
3. **Fan-Out / Queue-Based Load Leveling**: Trigger functions via SQS/EventBridge rather than chaining synchronous Lambda-to-Lambda calls.
4. **Functional Core + Thin Handler**: Keep business logic in pure functions ([Functional DI](file:///E:/Learning/engineering-notes/08-solid-design-patterns/12-functional-and-modern-idioms/08-dependency-injection-in-functional-style.md)) so you test locally without AWS SAM/LocalStack.

---

## 3. Anti-Patterns & Common Mistakes

| Mistake | Why It Fails | Fix |
| :--- | :--- | :--- |
| Opening a new PostgreSQL TCP connection inside the handler on every invoke | 1,000 concurrent Lambdas exhaust Postgres `max_connections` | Use RDS Proxy / HTTP-based DBs (DynamoDB, Neon) or reuse outside handler |
| Synchronous Lambda calling Lambda calling Lambda ("Lambda Pinball") | Double-bills idle wait time & multiplies cold starts | Orchestrate with Step Functions or decouple via SQS |
| Storing in-memory rate limits/sessions inside the function | Each container has isolated ephemeral RAM | Use Redis / DynamoDB |
| Heavy frameworks (full Spring Boot + Hibernate) in cold-start-sensitive APIs | 5–10 second cold starts | Use lightweight runtimes, GraalVM Native, or Node/Python |

---

## 4. Interview Questions

<details>
<summary>1. What causes a Serverless cold start, and how do you mitigate it?</summary>

When no warm container exists, the cloud provider must allocate a microVM, download your bundle, and run module initialization. Mitigate with smaller bundles, lightweight runtimes, provisioned concurrency, or GraalVM/edge isolates.

</details>

<details>
<summary>2. Why is connecting 500 Lambdas directly to PostgreSQL dangerous?</summary>

Each Lambda instance opens its own TCP connections, quickly exhausting Postgres's connection limit. Use a connection pooler like AWS RDS Proxy or PgBouncer.

</details>

<details>
<summary>3. Why is the "Lambda-calling-Lambda" pattern an anti-pattern?</summary>

You pay twice for compute while the caller Lambda sits idle waiting, and failures cascade. Use queues (SQS) or workflows (Step Functions).

</details>

<details>
<summary>4. When is Serverless a *bad* fit?</summary>

Steady high-throughput 24/7 workloads (often cheaper on containers/EC2), long-running >15-min jobs, or ultra-low-latency stateful WebSockets.

</details>

---

## 5. Cheat Sheet

- **Stateless + Idempotent** handlers; **init SDKs outside handler**.
- **Pool DB connections** (RDS Proxy) and **avoid sync Lambda-to-Lambda chains**.

---

## 6. Navigation

| Previous | Up | Next |
| :--- | :--- | :--- |
| [CQRS & Event Sourcing](file:///E:/Learning/engineering-notes/08-solid-design-patterns/10-architecture/07-cqrs-and-event-sourcing.md) | [Module 10](file:///E:/Learning/engineering-notes/08-solid-design-patterns/10-architecture/README.md) | [Pipes & Filters](file:///E:/Learning/engineering-notes/08-solid-design-patterns/10-architecture/09-pipes-and-filters.md) |
