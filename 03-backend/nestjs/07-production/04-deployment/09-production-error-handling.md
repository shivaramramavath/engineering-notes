---
title: "Production Error Handling and Problem Details in NestJS"
description: "RFC 7807 / RFC 9457 Problem Details formatting, database error masking, and enterprise exception filters in NestJS"
tags: [nestjs, deployment, error-handling, rfc7807, exception-filters, masking]
nestjs_version: "11.x"
node_version: ">=20.0.0"
express_version: "5.x"
---

# Production Error Handling and Problem Details in NestJS

## Conceptual Overview
Exposing raw database errors (e.g. `duplicate key value violates unique constraint "users_email_key"`) or Node.js stack traces leaks internal schema details to attackers.

Production error handling requires:
1. Masking internal infrastructure failures behind generic HTTP 500 messages.
2. Emitting standardized **RFC 7807 / RFC 9457 Problem Details** for consistent API error consumption.
3. Attaching correlation IDs to all error responses.

---

## Implementation Details & NestJS APIs

### 1. RFC 7807 Global Exception Filter

```typescript
// src/common/filters/problem-details.filter.ts
import {
  ExceptionFilter,
  Catch,
  ArgumentsHost,
  HttpException,
  HttpStatus,
  Logger,
} from '@nestjs/common';
import { Request, Response } from 'express';

@Catch()
export class ProblemDetailsFilter implements ExceptionFilter {
  private readonly logger = new Logger(ProblemDetailsFilter.name);

  catch(exception: unknown, host: ArgumentsHost) {
    const ctx = host.switchToHttp();
    const response = ctx.getResponse<Response>();
    const request = ctx.getRequest<Request>();

    const isHttp = exception instanceof HttpException;
    const status = isHttp ? exception.getStatus() : HttpStatus.INTERNAL_SERVER_ERROR;
    const correlationId = (request.headers['x-correlation-id'] as string) || 'none';

    // Mask internal error messages for 5xx
    const detail =
      status >= 500
        ? 'An unexpected internal server error occurred.'
        : isHttp
          ? (exception.getResponse() as any)?.message || exception.message
          : 'Invalid request.';

    if (status >= 500) {
      this.logger.error(`[${correlationId}] Unhandled Error: ${exception}`, (exception as any)?.stack);
    }

    response.status(status).contentType('application/problem+json').json({
      type: `https://httpstatuses.com/${status}`,
      title: HttpStatus[status],
      status,
      detail,
      instance: request.originalUrl,
      correlationId,
      timestamp: new Date().toISOString(),
    });
  }
}
```

---

## Key Takeaways & Cheat Sheet

- Never leak database error messages or stack traces to clients.
- Format errors conforming to RFC 7807 (`application/problem+json`).
- Include `correlationId` in error responses for rapid support ticket investigation.

---

## Cross-References
- [04-request-lifecycle-internals.md](../../06-internals/04-request-lifecycle-internals.md) - Filter lifecycle
- [07-error-tracking-and-alerting.md](../03-observability/07-error-tracking-and-alerting.md) - Sentry tracking
