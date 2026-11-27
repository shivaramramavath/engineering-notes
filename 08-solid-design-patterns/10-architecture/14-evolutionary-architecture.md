# Evolutionary Architecture

| Field | Value |
| :--- | :--- |
| **Category** | Software Architecture (Changeability & Migration) |
| **Origin** | Ford, Parsons, Kua (*Building Evolutionary Architectures*, 2017) |
| **Difficulty** | Intermediate → Advanced |
| **Prerequisite(s)** | [ADRs](file:///E:/Learning/engineering-notes/08-solid-design-patterns/10-architecture/12-architecture-decision-records.md), [Architecture Fitness Functions](file:///E:/Learning/engineering-notes/08-solid-design-patterns/10-architecture/13-architecture-fitness-functions.md) |
| **Status** | Complete |

> **TL;DR**: An **Evolutionary Architecture** supports **guided, incremental change** across multiple dimensions—deferring irreversible decisions to the **Last Responsible Moment**, migrating legacy systems via the **Strangler Fig** pattern, and guarding invariants with **Fitness Functions**.

---

## 1. Core Principles

1. **Last Responsible Moment**: Don't pick Kubernetes, Kafka, and 12 microservices on Day 1 before you know your scale. Keep boundaries clean (Modular Monolith + Ports) so you *can* split later when data justifies it.
2. **Incremental Change (Strangler Fig Pattern)**: Never do a "Big Bang Rewrite" (which takes 2 years and usually fails). Instead, place a routing Proxy/Gateway in front of the legacy system and migrate **one vertical slice at a time** until the legacy system is completely replaced.
3. **Guided by Fitness Functions**: Automated tests ensure incremental changes don't break performance, security, or module boundaries.
4. **Expand-and-Contract (Parallel Change)**: For zero-downtime DB/API migrations:
   - *Expand*: Add new column/endpoint alongside old one; dual-write.
   - *Migrate*: Backfill & switch readers to new column.
   - *Contract*: Remove old column/endpoint once traffic is 0%.

---

## 2. Strangler Fig Migration Flow

```mermaid
flowchart LR
    Client([Traffic]) --> Proxy["Routing Proxy / API Gateway"]
    Proxy -->|/orders (migrated)| NewSvc["New Orders Service"]
    Proxy -->|/billing, /users (legacy)| Legacy["Legacy Monolith"]
```

---

## 3. Anti-Patterns & Common Mistakes

| Mistake | Why It Fails | Fix |
| :--- | :--- | :--- |
| Attempting a 18-month "Big Bang Rewrite" from scratch | Business requirements move faster than the rewrite; two systems diverge | Migrate incrementally via **Strangler Fig** |
| Renaming/dropping DB columns in one deploy | Breaks running old app instances during rolling deploys | Use **Expand-and-Contract** across 2–3 safe deploys |
| Prematurely locking into heavy infrastructure "just in case" | Slows velocity and cements wrong boundaries | Defer to the **Last Responsible Moment** |

---

## 4. Interview Questions

<details>
<summary>1. What is the Strangler Fig pattern?</summary>

An incremental migration strategy where a routing layer intercepts requests to a legacy system and gradually shifts individual routes/capabilities to new services until the legacy system can be retired safely.

</details>

<details>
<summary>2. Explain the Expand-and-Contract pattern for zero-downtime schema changes.</summary>

1) **Expand**: Add the new column/field as nullable and dual-write both old and new. 2) **Migrate**: Backfill old rows and switch reads to the new column. 3) **Contract**: Drop the old column once no code references it.

</details>

<details>
<summary>3. What does "Last Responsible Moment" mean in architecture?</summary>

Waiting to make an irreversible or costly decision until the cost of *not* deciding exceeds the value of waiting for more real-world information.

</details>

---

## 5. Cheat Sheet

- **Migrate safely**: Strangler Fig (services) + Expand-and-Contract (schemas/APIs).
- **Guard**: Fitness Functions + ADRs.
- **Avoid**: Big Bang Rewrites & premature distributed complexity.

---

## 6. Navigation

| Previous | Up | Next |
| :--- | :--- | :--- |
| [Architecture Fitness Functions](file:///E:/Learning/engineering-notes/08-solid-design-patterns/10-architecture/13-architecture-fitness-functions.md) | [Module 10](file:///E:/Learning/engineering-notes/08-solid-design-patterns/10-architecture/README.md) | [Module 11: Distributed & Cloud Patterns](file:///E:/Learning/engineering-notes/08-solid-design-patterns/11-distributed-and-cloud-patterns/README.md) |
