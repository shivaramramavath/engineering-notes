---
title: "Rate Limiting and Brute Force Protection in NestJS"
description: "Architecting rate limiting, sliding window throttlers, Redis-backed multi-instance rate limiting, and bot protection with @nestjs/throttler"
tags: [nestjs, security, rate-limiting, throttler, redis, brute-force]
nestjs_version: "11.x"
node_version: ">=20.0.0"
express_version: "5.x"
---

# Rate Limiting and Brute Force Protection in NestJS

## Conceptual Overview
Unrestricted API endpoints expose applications to denial-of-service (DoS) attacks, automated credential brute forcing, and resource exhaustion. Rate limiting enforces request frequency boundaries based on client identifiers (IP address, authenticated user ID, or API key).

NestJS provides the official `@nestjs/throttler` package, offering:
- Multi-tier throttling (burst vs sustained limits).
- Redis-backed distributed storage for multi-pod Kubernetes clusters.
- Declarative route overrides (`@Throttle()`, `@SkipThrottle()`).
- Custom key generators for IP and tenant-aware rate tracking.

---

## Core Mechanisms & Architecture

### 1. Algorithms: Fixed Window vs Sliding Window
- **Fixed Window**: Tracks request count in discrete time buckets (e.g., 100 requests per 60s). Vulnerable to burst traffic at window boundaries (up to $2	imes$ the limit).
- **Sliding Window Counter**: Interpolates the request rate across overlapping sub-intervals, delivering smooth rate enforcement.

---

## Implementation Details & NestJS APIs

### 1. Multi-Tier Redis Throttler Setup

```typescript
// src/app.module.ts
import { Module } from '@nestjs/common';
import { ThrottlerModule, ThrottlerGuard } from '@nestjs/throttler';
import { ThrottlerStorageRedisService } from '@nest-lab/throttler-storage-redis';
import { APP_GUARD } from '@nestjs/core';

@Module({
  imports: [
    ThrottlerModule.forRootAsync({
      useFactory: () => ({
        throttlers: [
          // 1. Short burst protection
          { name: 'short', ttl: 1000, limit: 10 },
          // 2. Medium sustained protection
          { name: 'medium', ttl: 10000, limit: 50 },
          // 3. Long hourly protection
          { name: 'long', ttl: 3600000, limit: 1000 },
        ],
        storage: new ThrottlerStorageRedisService(process.env.REDIS_URL),
      }),
    }),
  ],
  providers: [
    {
      provide: APP_GUARD,
      useClass: ThrottlerGuard,
    },
  ],
})
export class AppModule {}
```

### 2. Custom Login Endpoint Protection

```typescript
// src/auth/auth.controller.ts
import { Controller, Post, Body, UseGuards } from '@nestjs/common';
import { Throttle, SkipThrottle } from '@nestjs/throttler';

@Controller('auth')
export class AuthController {
  // Strictest limit on login: 5 attempts per 60 seconds
  @Throttle({ default: { limit: 5, ttl: 60000 } })
  @Post('login')
  async login(@Body() dto: LoginDto) {
    return this.authService.login(dto);
  }

  // Exempt healthcheck from throttling
  @SkipThrottle()
  @Get('health')
  health() {
    return { status: 'ok' };
  }
}
```

---

## Production Patterns & Best Practices

- **Proxy Awareness**: Behind reverse proxies (Nginx, AWS ALB), `req.ip` reflects the proxy's IP unless `app.set('trust proxy', 1)` is configured. Failing to configure proxy trust results in all users sharing a single rate-limit bucket!
- **IP + Username Keying for Authentication**: Rate limit login endpoints by combining client IP and target username (`ip:username`) to prevent distributed credential stuffing.

---

## Key Takeaways & Cheat Sheet

- Use `@nestjs/throttler` with Redis storage in multi-instance production environments.
- Protect sensitive endpoints (login, forgot-password, payment) with strict per-minute limits.
- Always configure `app.set('trust proxy', 1)` when behind Nginx or Cloudflare.

---

## Cross-References
- [01-security-fundamentals.md](./01-security-fundamentals.md) - Security overview
- [07-dependency-security.md](./07-dependency-security.md) - Supply chain security
