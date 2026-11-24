---
title: "Cross-Site Request Forgery (CSRF) Defenses in NestJS"
description: "Mitigating CSRF in NestJS APIs using SameSite cookies, double-submit cookie pattern, and custom CSRF guards"
tags: [nestjs, security, csrf, samesite, cookies, double-submit]
nestjs_version: "11.x"
node_version: ">=20.0.0"
express_version: "5.x"
---

# Cross-Site Request Forgery (CSRF) Defenses in NestJS

## Conceptual Overview
Cross-Site Request Forgery (CSRF) is an attack where a malicious website tricks a victim's browser into executing unwanted actions on an authenticated web application. If authentication credentials (session cookies) are transmitted automatically by the browser, the target API cannot distinguish legitimate user interactions from forged cross-site requests.

CSRF defenses depend on your application architecture:
- **Pure Token-Based APIs (Authorization: Bearer <JWT>)**: Inherently immune to CSRF because browsers never automatically attach custom `Authorization` headers on cross-site requests.
- **Cookie-Based Sessions**: Vulnerable to CSRF unless protected by **SameSite cookies** and cryptographic **Anti-CSRF tokens**.

---

## Core Mechanisms & Architecture

### 1. SameSite Cookie Attribute
- `SameSite=Strict`: Cookies are never sent on cross-site requests (including clicking external links).
- `SameSite=Lax` (Modern Browser Default): Cookies are withheld on cross-site state-changing POST/PUT requests, but sent on top-level GET navigations.
- `SameSite=None; Secure`: Cookies are transmitted on all cross-site requests (requires HTTPS).

### 2. Double-Submit Cookie Pattern
1. Upon login, the server sets a random token in a readable cookie (`XSRF-TOKEN`).
2. On every mutating request (POST, PUT, DELETE), the client application reads `XSRF-TOKEN` and submits it in a custom header (`X-XSRF-TOKEN`).
3. The NestJS Guard verifies that the cookie token matches the header token.

---

## Implementation Details & NestJS APIs

### 1. Custom Double-Submit CSRF Guard

```typescript
// src/common/guards/csrf.guard.ts
import {
  Injectable,
  CanActivate,
  ExecutionContext,
  ForbiddenException,
} from '@nestjs/common';
import { Request } from 'express';

@Injectable()
export class CsrfGuard implements CanActivate {
  canActivate(context: ExecutionContext): boolean {
    const request = context.switchToHttp().getRequest<Request>();
    const method = request.method.toUpperCase();

    // Safe idempotent methods do not require CSRF tokens
    if (['GET', 'HEAD', 'OPTIONS'].includes(method)) {
      return true;
    }

    const cookieToken = request.cookies?.['XSRF-TOKEN'];
    const headerToken = request.headers['x-xsrf-token'] as string;

    if (!cookieToken || !headerToken || cookieToken !== headerToken) {
      throw new ForbiddenException('Invalid or missing CSRF token');
    }

    return true;
  }
}
```

---

## Key Takeaways & Cheat Sheet

- APIs authenticated solely via `Authorization: Bearer` headers are naturally immune to CSRF.
- Cookie-authenticated applications must set `SameSite=Lax` or `SameSite=Strict` and `HttpOnly` on session cookies.
- Implement the Double-Submit Cookie pattern for SPA clients consuming cookie-authenticated APIs.

---

## Cross-References
- [02-http-security-headers-and-cors.md](./02-http-security-headers-and-cors.md) - CORS and headers
- [17-cookies-and-sessions.md](../../02-fundamentals/17-cookies-and-sessions.md) - Cookie configuration in NestJS
