---
title: "System Design: Distributed API Gateway Rate Limiter"
description: "Designing an enterprise sliding-window distributed rate limiter with Redis and @nestjs/throttler"
tags: [nestjs, system-design, rate-limiter, sliding-window, redis]
---

# System Design: Distributed API Gateway Rate Limiter

## Architecture
- **Algorithm**: Redis Sorted Set (ZSET) sliding-window rate limiting.
- **Multi-Tier Throttling**: Burst protection (10 req/s), sustained per-minute, and account-tier hourly quotas.
