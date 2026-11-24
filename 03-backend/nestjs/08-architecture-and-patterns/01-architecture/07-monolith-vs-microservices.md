---
title: "Monolith vs Microservices: Architectural Decision Framework"
description: "Evaluating tradeoffs, distributed monolith anti-patterns, organizational alignment (Conway's Law), and Strangler Fig migration"
tags: [nestjs, architecture, monolith, microservices, tradeoffs, strangler-fig]
nestjs_version: "11.x"
node_version: ">=20.0.0"
express_version: "5.x"
---

# Monolith vs Microservices: Architectural Decision Framework

## Conceptual Overview
Choosing between a monolith and microservices is one of the most consequential architectural decisions. Microservices introduce distributed systems fallacies: network latency, partial failures, distributed transactions, and operational complexity.

---

## Tradeoff Matrix

| Dimension | Modular Monolith | Microservices Architecture |
| :--- | :--- | :--- |
| **Operational Overhead** | Low (Single CI/CD, single container) | High (Kubernetes, service mesh, API gateway) |
| **Deployment Independence** | Tightly coupled release cycles | Independent releases per service |
| **Transaction Integrity** | ACID database transactions | Eventual consistency & Sagas |
| **Latency** | In-process memory calls ($< 0.01\text{ms}$) | Network RPC serialization ($2-50\text{ms}$) |
| **Team Scaling** | Requires strict modular boundaries | Natural alignment with Conway's Law |

---

## Key Takeaways & Cheat Sheet

- Start with a **Modular Monolith** until team size and domain boundaries necessitate independent deployments.
- Avoid the **Distributed Monolith** (microservices that must be deployed together and share databases).
- Migrate incrementally using the **Strangler Fig Pattern**.

---

## Cross-References
- [02-modular-monolith.md](./02-modular-monolith.md) - Modular monolith
- [01-microservices-architecture.md](../../05-advanced/06-microservices/01-microservices-architecture.md) - Microservices architecture
