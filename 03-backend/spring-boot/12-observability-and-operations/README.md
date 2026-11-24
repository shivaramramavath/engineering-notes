# Module 12: Observability & Production Operations

Welcome to **Module 12: Observability & Production Operations**. This module equips engineers to monitor, diagnose, and maintain production Spring Boot applications: Actuator in depth, Micrometer metrics, OpenTelemetry distributed tracing, native structured JSON logging, Prometheus/Grafana, Kubernetes health probes, and JVM thread/heap dump analysis.

---

## Module Contents

| # | Note | Description | Level | Status |
|---|---|---|---|---|
| 01 | [01-actuator-in-depth.md](file:///03-backend/spring-boot/12-observability-and-operations/01-actuator-in-depth.md) | Actuator architecture, custom endpoints (`@Endpoint`), JMX vs HTTP exposure | Intermediate | Complete |
| 02 | [02-micrometer-metrics.md](file:///03-backend/spring-boot/12-observability-and-operations/02-micrometer-metrics.md) | MeterRegistry, Counters, Timers, Gauges, DistributionSummaries, percentiles | Intermediate | Complete |
| 03 | [03-opentelemetry-tracing-and-logs.md](file:///03-backend/spring-boot/12-observability-and-operations/03-opentelemetry-tracing-and-logs.md) | Exporting spans and traces directly to OpenTelemetry collectors | Advanced | Complete |
| 04 | [04-structured-logging.md](file:///03-backend/spring-boot/12-observability-and-operations/04-structured-logging.md) | Boot 3.4+ native structured logging: ECS, Logstash, GELF, MDC trace injection | Intermediate | Complete |
| 05 | [05-health-probes-and-kubernetes.md](file:///03-backend/spring-boot/12-observability-and-operations/05-health-probes-and-kubernetes.md) | Kubernetes Liveness and Readiness probe groups (`/actuator/health/liveness`) | Intermediate | Complete |
| 06 | [06-prometheus-and-grafana.md](file:///03-backend/spring-boot/12-observability-and-operations/06-prometheus-and-grafana.md) | Scraping `/actuator/prometheus`, designing production Grafana dashboards | Intermediate | Complete |
| 07 | [07-logging-best-practices.md](file:///03-backend/spring-boot/12-observability-and-operations/07-logging-best-practices.md) | Log level discipline, async appenders, preventing log injection and PII leaks | Beginner | Complete |
| 08 | [08-jvm-metrics-and-diagnostics.md](file:///03-backend/spring-boot/12-observability-and-operations/08-jvm-metrics-and-diagnostics.md) | Monitoring JVM memory pools (Eden, Survivor, Tenured, Metaspace) and GC pauses | Advanced | Complete |
| 09 | [09-thread-and-heap-dump-analysis.md](file:///03-backend/spring-boot/12-observability-and-operations/09-thread-and-heap-dump-analysis.md) | Generating and analyzing thread dumps (jstack) and heap dumps (jcmd/MAT) | Advanced | Complete |
| 10 | [10-spring-boot-admin-overview.md](file:///03-backend/spring-boot/12-observability-and-operations/10-spring-boot-admin-overview.md) | Spring Boot Admin server and client setup for multi-instance visual management | Intermediate | Complete |
| 11 | [11-alerting-and-slos.md](file:///03-backend/spring-boot/12-observability-and-operations/11-alerting-and-slos.md) | Defining Service Level Objectives (SLOs), error budgets, and Prometheus alerts | Advanced | Complete |

---

## Next Steps
Proceed to **[Module 13: Performance Tuning & Optimization](file:///03-backend/spring-boot/13-performance/)** to master GraalVM Native Image, Virtual Threads, and HikariCP optimization.
