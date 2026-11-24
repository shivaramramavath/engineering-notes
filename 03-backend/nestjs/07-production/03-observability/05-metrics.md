---
title: "Metrics and Prometheus Integration in NestJS"
description: "Collecting Golden Signals, custom business metrics, and exposing Prometheus endpoints using nestjs-prometheus"
tags: [nestjs, observability, metrics, prometheus, grafana, counters, histograms]
nestjs_version: "11.x"
node_version: ">=20.0.0"
express_version: "5.x"
---

# Metrics and Prometheus Integration in NestJS

## Conceptual Overview
Metrics represent aggregated numerical timeseries data measuring system performance over time. While logs record individual events, metrics reveal macroscopic trends: request rates, error ratios, latency distributions, and resource saturation.

Integrating Prometheus with NestJS involves:
- Exposing a `/metrics` scrape endpoint.
- Capturing default Node.js runtime metrics (event loop lag, GC duration, active handles, memory).
- Recording custom HTTP and business metrics (Counter, Gauge, Histogram, Summary).

---

## Implementation Details & NestJS APIs

### 1. Prometheus Module Configuration

```typescript
// src/app.module.ts
import { Module } from '@nestjs/common';
import { PrometheusModule, makeCounterProvider, makeHistogramProvider } from '@willsoto/nestjs-prometheus';

@Module({
  imports: [
    PrometheusModule.register({
      path: '/metrics',
      defaultMetrics: { enabled: true },
    }),
  ],
  providers: [
    makeCounterProvider({
      name: 'orders_placed_total',
      help: 'Total number of successfully placed customer orders',
      labelNames: ['plan_type', 'currency'],
    }),
    makeHistogramProvider({
      name: 'http_request_duration_seconds',
      help: 'HTTP request duration in seconds',
      labelNames: ['method', 'route', 'status_code'],
      buckets: [0.005, 0.01, 0.025, 0.05, 0.1, 0.25, 0.5, 1, 2.5, 5],
    }),
  ],
})
export class AppModule {}
```

---

## Key Takeaways & Cheat Sheet

- Collect the Four Golden Signals: Latency (Histogram), Traffic (Counter), Errors (Counter), Saturation (Gauge).
- Scrape metrics via a dedicated `/metrics` endpoint guarded by internal network firewalls.
- Avoid high-cardinality labels (never put user IDs or email addresses in metric labels).

---

## Cross-References
- [01-performance-fundamentals.md](../02-performance/01-performance-fundamentals.md) - Golden signals
- [06-tracing.md](./06-tracing.md) - Distributed tracing
