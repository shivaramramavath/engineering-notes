---
title: "NestJS Interview: Scenario-Based and Real-World Questions"
description: "Practical scenarios: migrating Express to NestJS, multi-tenancy, and handling distributed transaction failures"
tags: [nestjs, interview, scenarios, migration, multi-tenancy]
---

# NestJS Interview: Scenario-Based and Real-World Questions

### Scenario: Migrating an existing 50-endpoint Express app to NestJS
**Approach**:
1. Bootstrap NestJS with `ExpressAdapter`.
2. Mount the existing legacy Express router on the underlying Express instance via `app.getHttpAdapter().getInstance().use('/legacy', legacyRouter)`.
3. Incrementally migrate endpoints module-by-module to native NestJS controllers and services (Strangler Fig pattern).
