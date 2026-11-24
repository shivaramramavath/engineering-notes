# Module 07: Production Engineering, Observability & Security

> Comprehensive guide to enterprise production readiness: OWASP security hardening, performance optimization, Pino structured logging, OpenTelemetry tracing, Terminus health checks, and containerized deployment.

---

## Sub-Modules & Topics

### 01. Security ([01-security](01-security/README.md))
- **Defense in Depth**: Helmet integration, Content-Security-Policy (CSP), CORS configurations, CSRF protection, and rate limiting with Throttler.
- **OWASP API Security Top 10**: Parameter validation, IDOR prevention, secret management, and timing-safe cryptography.

### 02. Performance ([02-performance](02-performance/README.md))
- **Fastify Platform Adapter**: High-throughput HTTP serving with Fastify 5.
- **Optimization**: Node.js clustering, V8 heap optimization, database query optimization, and load testing with autocannon.

### 03. Observability ([03-observability](03-observability/README.md))
- **Structured Logging**: `nestjs-pino` with correlation ID injection (`X-Request-Id`) and JSON log output.
- **Distributed Tracing & Metrics**: OpenTelemetry instrumentation, Prometheus metrics export, and Terminus health checks.

### 04. Deployment ([04-deployment](04-deployment/README.md))
- **Containers**: Multi-stage Dockerfile with Google Distroless runners and Docker Compose.
- **Kubernetes & Cloud**: Zero-downtime rolling deployments, graceful SIGTERM shutdown with connection draining, and AWS ECS/Fargate deployment.

### 05. Operations ([05-operations](05-operations/README.md))
- **Runbooks & Triage**: Diagnosing memory leaks using V8 heap snapshots, CPU profiling flamegraphs, incident post-mortems, and dynamic config rotation.
