---
title: "Request Logging and Correlation IDs in NestJS"
description: "Distributed tracing using correlation IDs, AsyncLocalStorage, and request context propagation across NestJS microservices"
tags: [nestjs, observability, correlation-id, async-local-storage, distributed-tracing]
nestjs_version: "11.x"
node_version: ">=20.0.0"
express_version: "5.x"
---

# Request Logging and Correlation IDs in NestJS

## Conceptual Overview
When an HTTP request triggers an internal chain of calls across microservices, databases, and message brokers, debugging failures requires a shared identifier connecting all log statements: the **Correlation ID** (often transmitted as `X-Correlation-ID` or `X-Request-ID`).

Using Node.js `AsyncLocalStorage`, NestJS backends can preserve and access the active correlation ID anywhere in the call stack without passing it explicitly through service method parameters.

---

## Implementation Details & NestJS APIs

### 1. Correlation Context with AsyncLocalStorage

```typescript
// src/common/context/request-context.ts
import { AsyncLocalStorage } from 'node:async_hooks';

export interface RequestStore {
  correlationId: string;
  userId?: string;
}

export const requestContext = new AsyncLocalStorage<RequestStore>();
```

```typescript
// src/common/middleware/correlation.middleware.ts
import { Injectable, NestMiddleware } from '@nestjs/common';
import { Request, Response, NextFunction } from 'express';
import { randomUUID } from 'node:crypto';
import { requestContext } from '../context/request-context';

@Injectable()
export class CorrelationMiddleware implements NestMiddleware {
  use(req: Request, res: Response, next: NextFunction) {
    const correlationId = (req.headers['x-correlation-id'] as string) || randomUUID();

    // Attach to response headers so client receives it
    res.setHeader('x-correlation-id', correlationId);

    // Run downstream execution within AsyncLocalStorage scope
    requestContext.run({ correlationId }, () => {
      next();
    });
  }
}
```

---

## Key Takeaways & Cheat Sheet

- Assign an `X-Correlation-ID` to every inbound request if not provided by the API gateway.
- Use `AsyncLocalStorage` to store the correlation ID across asynchronous microtasks.
- Echo the correlation ID back in the HTTP response headers.

---

## Cross-References
- [02-structured-logging-with-pino.md](./02-structured-logging-with-pino.md) - Pino integration
- [06-tracing.md](./06-tracing.md) - OpenTelemetry distributed tracing
