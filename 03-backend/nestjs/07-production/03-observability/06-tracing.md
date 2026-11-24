---
title: "Distributed Tracing with OpenTelemetry in NestJS"
description: "End-to-end distributed tracing across HTTP, microservices, and databases using OpenTelemetry (OTel) and Jaeger"
tags: [nestjs, observability, tracing, opentelemetry, otel, jaeger, spans]
nestjs_version: "11.x"
node_version: ">=20.0.0"
express_version: "5.x"
---

# Distributed Tracing with OpenTelemetry in NestJS

## Conceptual Overview
In microservice architectures, a single user request can propagate across dozens of independent services. Distributed tracing tracks the complete lifecycle of a request as it crosses process boundaries, measuring exact durations for every nested database query, cache fetch, and RPC call.

**OpenTelemetry (OTel)** is the vendor-neutral industry standard for distributed telemetry:
- **Tracer**: Creates and manages traces.
- **Span**: A single unit of work (e.g., executing a SQL query or HTTP call).
- **Trace Context Propagation**: Transmitting trace headers (`traceparent`, `tracestate`) across HTTP and messaging protocols (W3C Trace Context).

---

## Implementation Details & NestJS APIs

### 1. OpenTelemetry Initialization Script

OpenTelemetry must initialize **before** any application modules are loaded:

```typescript
// src/tracer.ts
import { NodeSDK } from '@opentelemetry/sdk-node';
import { getNodeAutoInstrumentations } from '@opentelemetry/auto-instrumentations-node';
import { OTLPTraceExporter } from '@opentelemetry/exporter-trace-otlp-http';
import { Resource } from '@opentelemetry/resources';
import { ATTR_SERVICE_NAME, ATTR_SERVICE_VERSION } from '@opentelemetry/semantic-conventions';

const sdk = new NodeSDK({
  resource: new Resource({
    [ATTR_SERVICE_NAME]: 'orders-service',
    [ATTR_SERVICE_VERSION]: '1.0.0',
  }),
  traceExporter: new OTLPTraceExporter({
    url: process.env.OTEL_EXPORTER_OTLP_ENDPOINT || 'http://localhost:4318/v1/traces',
  }),
  instrumentations: [getNodeAutoInstrumentations()],
});

sdk.start();

process.on('SIGTERM', () => {
  sdk.shutdown().finally(() => process.exit(0));
});
```

Execute via Node:
```bash
node --require ./dist/tracer.js ./dist/main.js
```

---

## Key Takeaways & Cheat Sheet

- Initialize OpenTelemetry before bootstrapping NestJS via `--require tracer.js`.
- Use W3C Trace Context headers (`traceparent`) for cross-service propagation.
- Export traces to OTLP collectors (Jaeger, Datadog, Honeycomb).

---

## Cross-References
- [03-request-logging-and-correlation-id.md](./03-request-logging-and-correlation-id.md) - Correlation IDs
- [05-metrics.md](./05-metrics.md) - Prometheus metrics
