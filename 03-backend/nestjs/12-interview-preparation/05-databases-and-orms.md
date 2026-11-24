---
title: "NestJS Interview: Databases and ORMs"
description: "Questions on TypeORM, Prisma, MikroORM, Drizzle, connection pooling, and solving the N+1 problem"
tags: [nestjs, interview, databases, typeorm, prisma, n-plus-one]
---

# NestJS Interview: Databases and ORMs

### Q1: How do you solve the N+1 query problem in NestJS?
**Answer**:
1. In SQL / REST: Use eager joins (`relations: ['items']`) or query builder joins to fetch child records in a single query.
2. In GraphQL: Use request-scoped **DataLoader** instances that batch individual foreign key queries across the tick into a single `WHERE id IN (...)` SQL query.
