# Logging, Observability & Design

> **TL;DR**: Design observability in from Day 1 using **Three Pillars** (Structured JSON Logs, Metrics, OpenTelemetry Distributed Traces) tied together by a propagated `traceId` / `correlationId`.

## Rules
- **Structured JSON Logs** (`{ "level": "error", "orderId": "o1", "traceId": "..." }`), never `System.out.println` or unstructured string concatenation.
- **Inject / Wrap Observability at Boundaries**: Use Middleware / Decorators / OpenTelemetry auto-instrumentation so business domain classes aren't cluttered with `logger.info()` on every line.
- **Redact Secrets & PII**: Never log passwords, JWTs, API keys, or credit cards.
