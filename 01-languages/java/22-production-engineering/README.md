# 22 · Production Engineering

Writing Java code that passes unit tests is only the first step; delivering software that runs reliably, securely, and scalably under real-world production load requires enterprise production engineering. This module covers structured asynchronous logging, externalized 12-Factor configuration, layered container packaging, observable distributed tracing and metrics, live JVM diagnostic toolchains, and algorithmic troubleshooting playbooks.

## Contents

| # | Note | Focus |
|---|------|-------|
| 00 | [Production Logging](00_logging.md) | SLF4J, Logback, JSON logs, MDC context propagation, async appenders |
| 01 | [Configuration Management](01_configuration-management.md) | 12-Factor App config, precedence hierarchy, typed records, fail-fast validation |
| 02 | [Packaging and Deployment](02_packaging-and-deployment.md) | Layered Dockerfiles, jlink minimal custom JREs, AppCDS fast startup |
| 03 | [Observability: Metrics, Tracing, Health](03_observability.md) | Micrometer metrics, OpenTelemetry W3C tracing, Kubernetes liveness/readiness |
| 04 | [JVM Diagnostics and Tooling](04_jvm-diagnostics.md) | jcmd, jstack, jmap, jstat, Native Memory Tracking (NMT), crash log analysis |
| 05 | [Production Troubleshooting Playbook](05_troubleshooting-playbook.md) | Runbooks for High CPU, Memory Leaks, Deadlocks, Connection Pool Exhaustion |

---
**Next module:** [Design and Clean Code](../23-design-and-clean-code/README.md)
