---
title: "HTTP Security Headers and CORS in NestJS"
description: "Configuring Helmet, Content Security Policy (CSP), Strict-Transport-Security (HSTS), and enterprise Cross-Origin Resource Sharing in NestJS"
tags: [nestjs, security, helmet, cors, hsts, csp, headers]
nestjs_version: "11.x"
node_version: ">=20.0.0"
express_version: "5.x"
---

# HTTP Security Headers and CORS in NestJS

## Conceptual Overview
Web browsers rely on HTTP response headers to enforce security boundaries. Without appropriate security headers, client browsers are susceptible to cross-site scripting (XSS), clickjacking, MIME-type sniffing, and cross-origin data theft.

In NestJS, HTTP security header governance is achieved using two core mechanisms:
1. **Helmet**: Sets 15+ standardized HTTP headers to protect against well-known web attack vectors.
2. **CORS (Cross-Origin Resource Sharing)**: Controls which external web origins are permitted to make credentialed requests to your API.

---

## Core Mechanisms & Architecture

### 1. Essential Security Headers
- **Content-Security-Policy (CSP)**: Restricts scripts, styles, images, and frame origins.
- **Strict-Transport-Security (HSTS)**: Forces browsers to communicate exclusively over HTTPS:
  `max-age=31536000; includeSubDomains; preload`
- **X-Frame-Options**: Prevents clickjacking by disabling embedding in `<iframe>` (`DENY` or `SAMEORIGIN`).
- **X-Content-Type-Options**: Prevents MIME-type sniffing (`nosniff`).
- **Referrer-Policy**: Controls how much referrer information is leaked (`strict-origin-when-cross-origin`).

---

## Implementation Details & NestJS APIs

### 1. Helmet Configuration in `main.ts`

```typescript
// src/main.ts
import { NestFactory } from '@nestjs/core';
import { AppModule } from './app.module';
import helmet from 'helmet';

async function bootstrap() {
  const app = await NestFactory.create(AppModule);

  app.use(
    helmet({
      contentSecurityPolicy: {
        directives: {
          defaultSrc: ["'self'"],
          scriptSrc: ["'self'"],
          styleSrc: ["'self'", "'unsafe-inline'"],
          imgSrc: ["'self'", 'data:', 'https://images.yourcdn.com'],
          connectSrc: ["'self'", 'https://api.yourdomain.com'],
          objectSrc: ["'none'"],
          upgradeInsecureRequests: [],
        },
      },
      crossOriginEmbedderPolicy: true,
      crossOriginOpenerPolicy: { policy: 'same-origin' },
      crossOriginResourcePolicy: { policy: 'same-site' },
      hsts: {
        maxAge: 31536000,
        includeSubDomains: true,
        preload: true,
      },
      frameguard: { action: 'deny' },
      noSniff: true,
    }),
  );

  // 2. Production CORS Policy
  app.enableCors({
    origin: (origin, callback) => {
      const allowedOrigins = [
        'https://app.yourdomain.com',
        'https://admin.yourdomain.com',
      ];
      // Allow requests with no origin (mobile apps, curl) or matched origins
      if (!origin || allowedOrigins.includes(origin)) {
        callback(null, true);
      } else {
        callback(new Error('Blocked by CORS policy'));
      }
    },
    methods: ['GET', 'POST', 'PUT', 'PATCH', 'DELETE', 'OPTIONS'],
    allowedHeaders: ['Content-Type', 'Authorization', 'X-Requested-With', 'X-Correlation-ID'],
    exposedHeaders: ['X-Total-Count', 'X-RateLimit-Remaining'],
    credentials: true,
    maxAge: 86400, // Preflight cache 24h
  });

  await app.listen(3000);
}
bootstrap();
```

---

## Production Patterns & Best Practices

- **Never Use `origin: '*'` with `credentials: true`**: Modern browsers reject requests where wildcard origins are paired with cookie/credential authorization.
- **Preflight Cache Optimization (`maxAge: 86400`)**: Setting `maxAge` instructs browsers to cache HTTP `OPTIONS` preflight checks for 24 hours, eliminating 50% of round-trip network latency on cross-origin requests.

---

## Key Takeaways & Cheat Sheet

- Integrate `helmet()` at the top of `main.ts` before any route handling.
- Configure strict Content Security Policy (CSP) directives.
- Use explicit allowlists for CORS origins; never use `*` for authenticated APIs.
- Cache CORS preflight responses with `maxAge: 86400`.

---

## Cross-References
- [01-security-fundamentals.md](./01-security-fundamentals.md) - Security architecture overview
- [03-csrf.md](./03-csrf.md) - Cross-Site Request Forgery defenses
