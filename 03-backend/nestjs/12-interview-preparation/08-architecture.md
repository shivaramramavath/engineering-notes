---
title: "NestJS Interview: Enterprise Architecture and DDD"
description: "Questions on Modular Monoliths, Clean Architecture, Hexagonal architecture, and CQRS in NestJS"
tags: [nestjs, interview, architecture, ddd, clean-architecture]
---

# NestJS Interview: Enterprise Architecture and DDD

### Q1: What is the advantage of Hexagonal (Ports and Adapters) architecture in NestJS?
**Answer**: It ensures core enterprise business logic remains 100% pure TypeScript with zero dependencies on NestJS decorators, database drivers, or web engines. Adapters wrap external technologies, making migrations and testing trivial.
