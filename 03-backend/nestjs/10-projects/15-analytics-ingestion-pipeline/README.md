# Project 15: High-Throughput Analytics Ingestion Pipeline (`15-analytics-ingestion-pipeline`)

> Real-time event ingestion engine processing 100,000 events/second: Kafka microservice transporter, batch buffering, and PostgreSQL partitioned time-series storage.

| Metadata | Details |
| :--- | :--- |
| **Project Code** | `proj-nest-15-analytics-ingestion` |
| **Streaming** | Apache Kafka (`@nestjs/microservices`) |
| **Storage** | PostgreSQL Declarative Table Partitioning (Monthly Range) |
| **Throughput** | 100,000+ events/sec |
| **Complexity Level** | High-Scale Distributed Reference |

---

## 1. Kafka Batch Event Ingestion Transporter

```typescript
// src/analytics/analytics.controller.ts
import { Controller } from '@nestjs/common';
import { EventPattern, Payload, Ctx, KafkaContext } from '@nestjs/microservices';
import { AnalyticsService } from './analytics.service';

@Controller()
export class AnalyticsController {
  constructor(private readonly analyticsService: AnalyticsService) {}

  @EventPattern('telemetry.events')
  async handleTelemetry(@Payload() event: any, @Ctx() context: KafkaContext) {
    const rawMessage = context.getMessage();
    // Buffer for micro-batch insertion into PostgreSQL
    await this.analyticsService.bufferEvent(event);
  }
}
```
