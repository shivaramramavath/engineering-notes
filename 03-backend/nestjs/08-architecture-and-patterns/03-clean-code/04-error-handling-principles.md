---
title: "Error Handling Principles in NestJS"
description: "Designing domain exception hierarchies, fail-fast patterns, and preventing swallowed rejections in NestJS"
tags: [nestjs, clean-code, error-handling, exceptions, fail-fast]
nestjs_version: "11.x"
node_version: ">=20.0.0"
express_version: "5.x"
---

# Error Handling Principles in NestJS

## Conceptual Overview
Clean error handling guarantees system predictability. Catching errors and doing nothing (swallowing exceptions) is the single most destructive anti-pattern in backend systems.

Core rules:
1. **Never swallow exceptions**: If an error occurs, either handle it with a safe fallback, or rethrow it.
2. **Domain Exceptions vs HTTP Exceptions**: Domain logic should throw pure domain errors (`InsufficientFundsException`), not HTTP-specific exceptions (`HttpException(400)`). The presentation layer maps domain errors to HTTP status codes.
3. **Fail Fast**: Validate preconditions at the start of a function and abort immediately if invalid.

---

## Key Takeaways & Cheat Sheet

- Domain layers should throw domain-specific exceptions, never `HttpException`.
- Map domain exceptions to HTTP codes inside NestJS Exception Filters.
- Fail fast: validate arguments early before running heavy operations.

---

## Cross-References
- [09-production-error-handling.md](../../07-production/04-deployment/09-production-error-handling.md) - RFC 7807 problem details
- [07-error-tracking-and-alerting.md](../../07-production/03-observability/07-error-tracking-and-alerting.md) - Sentry alerting
