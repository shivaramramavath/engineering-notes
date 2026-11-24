---
title: "Error Tracking and Alerting with Sentry in NestJS"
description: "Automated exception capture, stack trace symbolication, Sentry integration, and SLA-based alerting policies"
tags: [nestjs, observability, sentry, error-tracking, alerting, exceptions]
nestjs_version: "11.x"
node_version: ">=20.0.0"
express_version: "5.x"
---

# Error Tracking and Alerting with Sentry in NestJS

## Conceptual Overview
Catching and alerting on unhandled exceptions in real time prevents prolonged outages. Tools like **Sentry** capture exception stack traces, HTTP context, authenticated user IDs, and environment metadata, aggregating identical errors into manageable issues.

In NestJS, Sentry integrates cleanly via a global **Exception Filter** or Interceptor.

---

## Implementation Details & NestJS APIs

### 1. Sentry Global Exception Filter

```typescript
// src/common/filters/sentry.filter.ts
import {
  Catch,
  ArgumentsHost,
  HttpException,
  HttpStatus,
  Logger,
} from '@nestjs/common';
import { BaseExceptionFilter } from '@nestjs/core';
import * as Sentry from '@sentry/node';

@Catch()
export class SentryExceptionFilter extends BaseExceptionFilter {
  private readonly logger = new Logger(SentryExceptionFilter.name);

  catch(exception: unknown, host: ArgumentsHost) {
    // Only capture unexpected server errors (500), ignore expected 4xx validation errors
    const isHttp = exception instanceof HttpException;
    const status = isHttp ? exception.getStatus() : HttpStatus.INTERNAL_SERVER_ERROR;

    if (status >= 500) {
      Sentry.captureException(exception);
      this.logger.error(`Exception forwarded to Sentry: ${exception}`);
    }

    super.catch(exception, host);
  }
}
```

---

## Key Takeaways & Cheat Sheet

- Forward only 5xx server errors to Sentry; do not flood alerting systems with 4xx user validation errors.
- Attach user ID and correlation ID to Sentry scopes for fast debugging.
- Define actionable alert policies with clear escalation paths (PagerDuty / Opsgenie).

---

## Cross-References
- [01-logging-fundamentals.md](./01-logging-fundamentals.md) - Logging basics
- [09-production-error-handling.md](../04-deployment/09-production-error-handling.md) - Production error handling
