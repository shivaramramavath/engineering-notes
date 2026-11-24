---
title: "NestJS Production Security Checklist"
description: "Comprehensive 50-point pre-flight security checklist for hardening NestJS backends before production deployment"
tags: [nestjs, security, checklist, audit, production-readiness]
nestjs_version: "11.x"
node_version: ">=20.0.0"
express_version: "5.x"
---

# NestJS Production Security Checklist

## Conceptual Overview
This authoritative checklist consolidates essential security verifications across configuration, authentication, transport, data handling, and observability.

---

## Production Security Audit Checklist

### 1. HTTP & Network Transport
- [ ] HTTPS enforced across all endpoints; plain HTTP redirected via reverse proxy.
- [ ] Helmet headers registered with custom Content Security Policy (CSP).
- [ ] HSTS configured with `maxAge: 31536000; includeSubDomains; preload`.
- [ ] CORS configured with explicit origin allowlist; no wildcard `*` with credentials.
- [ ] `X-Powered-By` header disabled.
- [ ] Reverse proxy trust configured (`app.set('trust proxy', 1)`).

### 2. Authentication & Authorization
- [ ] Passwords hashed using Argon2id or bcrypt (cost factor $\ge 12$).
- [ ] JWT tokens signed with strong asymmetric keys (RS256/ES256) or 256-bit secrets.
- [ ] Short token lifetimes enforced (Access token $\le 15$m, Refresh token $\le 7$d).
- [ ] Refresh tokens stored hashed in the database with revocation support.
- [ ] Global authentication guard configured via `APP_GUARD` with `@Public()` exemptions.
- [ ] Role / Permission checks enforced on all state-changing endpoints.

### 3. Input Validation & Data Protection
- [ ] `ValidationPipe` enabled globally with `whitelist: true` and `forbidNonWhitelisted: true`.
- [ ] All database queries parameterized; zero raw SQL string concatenation.
- [ ] Rate limiting enabled globally via `@nestjs/throttler` with Redis backing.
- [ ] Strict login endpoint throttling (e.g., max 5 attempts/minute).
- [ ] Sensitive response fields stripped via `@Exclude()` and `ClassSerializerInterceptor`.

### 4. Infrastructure & Supply Chain
- [ ] Docker container runs as non-root user (`USER node`).
- [ ] `npm audit` integrated into CI/CD pipeline blocking builds on high/critical CVEs.
- [ ] Secrets injected via environment variables or secret managers, never committed to git.
- [ ] Graceful shutdown hooks enabled (`app.enableShutdownHooks()`).

---

## Cross-References
- [01-security-fundamentals.md](./01-security-fundamentals.md) - Security architecture
- [10-production-checklist.md](../04-deployment/10-production-checklist.md) - Deployment checklist
