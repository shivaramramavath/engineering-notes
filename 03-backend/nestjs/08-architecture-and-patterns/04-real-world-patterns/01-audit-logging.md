---
title: "Audit Logging Architecture in NestJS"
description: "Architecting tamper-evident, immutable audit trails for compliance (SOC 2, HIPAA, GDPR) using NestJS interceptors and event sourcing"
tags: [nestjs, patterns, audit-logging, compliance, soc2, hipaa, interceptors]
nestjs_version: "11.x"
node_version: ">=20.0.0"
express_version: "5.x"
---

# Audit Logging Architecture in NestJS

## Conceptual Overview
An audit log is an immutable, append-only record of significant business operations and security events. Unlike operational debugging logs, audit logs answer the five Ws for compliance auditors:
- **Who**: Authenticated identity (User ID, API Key, Service Account).
- **What**: Action executed (`UPDATE_PATIENT_RECORD`, `REFUND_TRANSACTION`).
- **When**: High-precision UTC timestamp.
- **Where**: Originating IP, User-Agent, Tenant ID, and resource identifier.
- **Why**: Before-and-after state diff or justification note.

---

## Implementation Details: Declarative Audit Interceptor

```typescript
// src/common/audit/audit.interceptor.ts
import {
  Injectable,
  NestInterceptor,
  ExecutionContext,
  CallHandler,
} from '@nestjs/common';
import { Observable } from 'rxjs';
import { tap } from 'rxjs/operators';
import { AuditService } from './audit.service';
import { Reflector } from '@nestjs/core';

@Injectable()
export class AuditInterceptor implements NestInterceptor {
  constructor(
    private readonly auditService: AuditService,
    private readonly reflector: Reflector,
  ) {}

  intercept(context: ExecutionContext, next: CallHandler): Observable<any> {
    const action = this.reflector.get<string>('AUDIT_ACTION', context.getHandler());
    if (!action) return next.handle();

    const req = context.switchToHttp().getRequest();
    const user = req.user;
    const resourceId = req.params.id;

    return next.handle().pipe(
      tap(async (response) => {
        await this.auditService.record({
          action,
          userId: user?.id || 'anonymous',
          tenantId: user?.tenantId,
          resourceId,
          ip: req.ip,
          userAgent: req.headers['user-agent'],
          timestamp: new Date().toISOString(),
          status: 'SUCCESS',
        });
      }),
    );
  }
}
```

---

## Key Takeaways & Cheat Sheet

- Audit logs must be immutable and append-only.
- Write audit records in the same transaction or push to a durable queue (Kafka/SQS).
- Redact passwords, credit card numbers, and raw encryption keys before logging.

---

## Cross-References
- [03-metadata-and-reflection.md](../../06-internals/03-metadata-and-reflection.md) - Reflector metadata
- [01-logging-fundamentals.md](../../07-production/03-observability/01-logging-fundamentals.md) - Operational logging
