---
title: "NestJS Interview: Production Security and Hardening"
description: "OWASP Top 10 defenses, Helmet, CORS, CSRF, rate limiting, and parameter pollution prevention"
tags: [nestjs, interview, security, owasp, hardening]
---

# NestJS Interview: Production Security and Hardening

### Q1: Why should you configure `ValidationPipe({ whitelist: true, forbidNonWhitelisted: true })` globally?
**Answer**: `whitelist: true` strips unexpected properties, while `forbidNonWhitelisted: true` halts the request with 400 Bad Request if unmapped properties are passed. This prevents **mass assignment vulnerabilities** where an attacker submits malicious payload fields (e.g. `isAdmin: true`).
