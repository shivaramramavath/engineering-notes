---
title: "Logging Fundamentals and Structured Logging in NestJS"
description: "Mastering logging architectures, log levels, structured JSON output, and NestJS 11 built-in ConsoleLogger features"
tags: [nestjs, observability, logging, structured-logging, json-logger, log-levels]
nestjs_version: "11.x"
node_version: ">=20.0.0"
express_version: "5.x"
---

# Logging Fundamentals and Structured Logging in NestJS

## Conceptual Overview
In production distributed systems, logs are not merely debugging text printed to a terminal—they are structured event streams ingested by centralized aggregators (Datadog, Elasticsearch, Loki, CloudWatch) for real-time alerting, audit trails, and root-cause analysis.

Unstructured text logging (`console.log('User logged in: ' + userId)`) breaks automated log parsers and complicates querying. Enterprise NestJS backends enforce **Structured JSON Logging**, ensuring every log record is an indexable JSON object with consistent metadata schemas.

```
+-------------------------------------------------------------------------------+
|                       Structured Log Record Anatomy                           |
|                                                                               |
| {                                                                             |
|   "timestamp": "2026-11-24T08:30:00.123Z",                                    |
|   "level": "INFO",                                                            |
|   "context": "OrdersService",                                                 |
|   "message": "Order successfully processed",                                  |
|   "orderId": "ord_8f92a10",                                                   |
|   "customerId": "cust_12345",                                                 |
|   "total": 149.99,                                                            |
|   "correlationId": "b1a7-49f2-89cd",                                          |
|   "durationMs": 42.5                                                          |
| }                                                                             |
+-------------------------------------------------------------------------------+
```

---

## Core Mechanisms & Architecture

### 1. Log Levels Hierarchy
NestJS logging supports standardized levels:
- `FATAL`: Unrecoverable errors requiring immediate engineer intervention (process will exit).
- `ERROR`: Runtime exceptions that prevented an operation from completing.
- `WARN`: Unexpected anomalies or degraded states that did not fail the operation.
- `LOG` / `INFO`: Normal operational milestones (system startup, significant domain events).
- `DEBUG`: Diagnostic information required during development or staging troubleshooting.
- `VERBOSE`: Highly granular execution tracing (internal loops, low-level socket payloads).

### 2. NestJS 11 Built-in `ConsoleLogger` with `json: true`
Starting in NestJS 11, the built-in `ConsoleLogger` natively supports structured JSON formatting without third-party dependencies:

```typescript
// src/main.ts
import { NestFactory } from '@nestjs/core';
import { AppModule } from './app.module';
import { ConsoleLogger } from '@nestjs/common';

async function bootstrap() {
  const app = await NestFactory.create(AppModule, {
    logger: new ConsoleLogger({
      json: true, // Emits machine-readable JSON in NestJS 11
      colors: process.env.NODE_ENV !== 'production',
      prefix: 'MyApp',
    }),
  });

  await app.listen(3000);
}
bootstrap();
```

---

## Implementation Details & NestJS APIs

### 1. Using Contextual Logging in Services

```typescript
// src/orders/orders.service.ts
import { Injectable, Logger } from '@nestjs/common';

@Injectable()
export class OrdersService {
  // Bind class name as logger context
  private readonly logger = new Logger(OrdersService.name);

  async processOrder(orderId: string, amount: number) {
    this.logger.log(`Processing order [${orderId}] for amount $${amount}`);

    try {
      // Order processing business logic...
      this.logger.debug(`Inventory reserved for order [${orderId}]`);
    } catch (error) {
      this.logger.error(
        `Failed to process order [${orderId}]: ${error.message}`,
        error.stack,
      );
      throw error;
    }
  }
}
```

---

## Key Takeaways & Cheat Sheet

- In production, always emit structured JSON to `stdout` / `stderr`.
- Use contextual loggers (`new Logger(Service.name)`) to identify the originating component.
- In NestJS 11, enable native JSON logging using `new ConsoleLogger({ json: true })`.
- Never log sensitive credentials, passwords, or unredacted PII.

---

## Cross-References
- [02-structured-logging-with-pino.md](./02-structured-logging-with-pino.md) - Pino integration
- [03-request-logging-and-correlation-id.md](./03-request-logging-and-correlation-id.md) - Correlation IDs
