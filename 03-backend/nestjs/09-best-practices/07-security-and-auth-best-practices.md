---
title: "Security and Authentication Best Practices in NestJS"
description: "Hardening authentication, tokens, password hashing, and API authorization policies in NestJS"
tags: [nestjs, best-practices, security, authentication, jwt, rbac]
nestjs_version: "11.x"
node_version: ">=20.0.0"
express_version: "5.x"
---

# Security and Authentication Best Practices in NestJS

## Core Principles
1. **Short-Lived Access Tokens**: Keep access token expiration $\le 15$ minutes. Use refresh tokens with rotation and revocation.
2. **Hash Passwords with Argon2id**: Use memory-hard password hashing algorithms.
3. **Deny by Default**: Apply authentication guards globally via `APP_GUARD` and explicitly exempt public routes using `@Public()`.

---

## Cross-References
- [01-security-fundamentals.md](../../07-production/01-security/01-security-fundamentals.md) - Security overview
