---
title: "Structured Logging with Pino in NestJS"
description: "Ultra-fast asynchronous logging, automatic redaction of PII, and HTTP request logging using nestjs-pino"
tags: [nestjs, observability, pino, nestjs-pino, redaction, high-performance]
nestjs_version: "11.x"
node_version: ">=20.0.0"
express_version: "5.x"
---

# Structured Logging with Pino in NestJS

## Conceptual Overview
Standard logging frameworks in Node.js (such as Winston or default console loggers) can block the V8 event loop when formatting strings under intense traffic. **Pino** is an ultra-fast Node.js logger that minimizes CPU overhead by serializing JSON asynchronously.

Using `nestjs-pino`:
- Outperforms standard Winston logging by up to $5	imes$.
- Automatically logs all HTTP request/response lifecycles.
- Automatically redacts sensitive fields (passwords, tokens, credit cards).
- Injects correlation IDs into every downstream log statement.

---

## Implementation Details & NestJS APIs

### 1. Configuring `LoggerModule` with Pino

```typescript
// src/app.module.ts
import { Module } from '@nestjs/common';
import { LoggerModule } from 'nestjs-pino';
import { randomUUID } from 'node:crypto';

@Module({
  imports: [
    LoggerModule.forRoot({
      pinoHttp: {
        level: process.env.NODE_ENV === 'production' ? 'info' : 'debug',
        // Auto-assign correlation ID
        genReqId: (req) => req.headers['x-correlation-id'] || randomUUID(),
        // Redact sensitive headers and body fields
        redact: {
          paths: [
            'req.headers.authorization',
            'req.headers.cookie',
            'req.body.password',
            'req.body.creditCard',
            'res.headers["set-cookie"]',
          ],
          censor: '[REDACTED]',
        },
        transport:
          process.env.NODE_ENV !== 'production'
            ? { target: 'pino-pretty', options: { colorize: true } }
            : undefined,
      },
    }),
  ],
})
export class AppModule {}
```

```typescript
// src/main.ts
import { NestFactory } from '@nestjs/core';
import { AppModule } from './app.module';
import { Logger } from 'nestjs-pino';

async function bootstrap() {
  const app = await NestFactory.create(AppModule, { bufferLogs: true });

  // Use Pino as the global NestJS application logger
  app.useLogger(app.get(Logger));

  await app.listen(3000);
}
bootstrap();
```

---

## Key Takeaways & Cheat Sheet

- `nestjs-pino` is the industry standard for high-throughput, low-overhead Node.js logging.
- Use `redact.paths` to automatically scrub sensitive credentials before serialization.
- Configure `pino-pretty` exclusively for local development; use raw JSON in production.

---

## Cross-References
- [01-logging-fundamentals.md](./01-logging-fundamentals.md) - Logging basics
- [03-request-logging-and-correlation-id.md](./03-request-logging-and-correlation-id.md) - Request correlation
