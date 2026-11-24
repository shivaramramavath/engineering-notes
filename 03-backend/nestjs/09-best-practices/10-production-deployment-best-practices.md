---
title: "Production Deployment Best Practices in NestJS"
description: "Pre-flight deployment verifications: container optimization, zero-downtime rollouts, and disaster recovery"
tags: [nestjs, best-practices, deployment, zero-downtime, containers, kubernetes]
nestjs_version: "11.x"
node_version: ">=20.0.0"
express_version: "5.x"
---

# Production Deployment Best Practices in NestJS

## Core Principles
1. **Run as Non-Root**: Ensure Docker containers execute under `USER node`.
2. **Implement Graceful Shutdown**: Call `app.enableShutdownHooks()` and allow time for in-flight requests to complete.
3. **Automate Rollbacks**: Configure automated health probe checks that trigger rollbacks if error thresholds are exceeded.

---

## Cross-References
- [10-production-checklist.md](../../07-production/04-deployment/10-production-checklist.md) - Pre-flight checklist
