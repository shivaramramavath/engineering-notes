---
title: "System Design: Idempotent Payment Processing Gateway"
description: "Designing a PCI-compliant payment gateway with idempotency keys, dual-writes prevention, and Webhook reconciliations"
tags: [nestjs, system-design, payments, idempotency, stripe]
---

# System Design: Idempotent Payment Processing Gateway

## Architecture
- **Idempotency**: Clients supply `Idempotency-Key` header cached in Redis with a 24-hour TTL.
- **Transactional Outbox**: Payment status transitions saved to database outbox table atomically before publishing.
