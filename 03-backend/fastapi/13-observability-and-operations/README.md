# Module 13: Observability & Production Operations

Welcome to **Module 13: Observability & Production Operations**. This module covers the telemetry stack required to run mission-critical FastAPI microservices: structured JSON logging, correlation IDs, OpenTelemetry distributed tracing, Prometheus instrumentation, and health probes.

---

## Module Overview

| File | Title | Description |
| :--- | :--- | :--- |
| [01-structured-logging-with-structlog-and-loguru.md](./01-structured-logging-with-structlog-and-loguru.md) | **Structured Logging** | JSON logging pipelines using `structlog` and `loguru` for ELK/Datadog. |
| [02-request-logging-and-correlation-ids.md](./02-request-logging-and-correlation-ids.md) | **Correlation IDs** | `asgi-correlation-id`, `X-Request-ID` propagation, and contextvars. |
| [03-opentelemetry-tracing-metrics-logs.md](./03-opentelemetry-tracing-metrics-logs.md) | **OpenTelemetry Tracing** | Auto and manual OTel spans, trace context propagation across HTTP services. |
| [04-prometheus-and-grafana.md](./04-prometheus-and-grafana.md) | **Prometheus & Grafana** | Exporting RED metrics (Rate, Errors, Duration) via `prometheus-fastapi-instrumentator`. |
| [05-health-checks-and-readiness.md](./05-health-checks-and-readiness.md) | **Health Checks & Probes** | Deep health checks, liveness vs readiness probes for Kubernetes. |
| [06-error-tracking-and-alerting.md](./06-error-tracking-and-alerting.md) | **Error Tracking (Sentry)** | Sentry integration, exception captures, user context, and breadcrumbs. |
| [07-profiling-and-debugging-in-production.md](./07-profiling-and-debugging-in-production.md)| **Production Profiling** | CPU profiling with `py-spy`, async profiling with `yappi`, and flamegraphs. |
| [08-memory-leak-diagnosis.md](./08-memory-leak-diagnosis.md) | **Memory Leak Diagnosis** | Diagnosing object accumulation, `tracemalloc`, and circular reference leaks. |
| [09-slos-and-alerting.md](./09-slos-and-alerting.md) | **SLOs & Alerting Rules** | Service level objectives, error budgets, and Prometheus alert expressions. |
