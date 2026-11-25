# Module 06: Production Operations, Cloud & Kubernetes

## Overview

Module 06 of the NGINX curriculum covers the real-world operational lifecycle, cloud-native deployments, container architectures, and high-availability clustering strategies that ensure NGINX runs continuously in production without outages.

This module details Docker containerization patterns (non-root unprivileged containers, PID 1 signal forwarding), the landmark migration from legacy Kubernetes Ingress to the **Kubernetes Gateway API** ahead of the March 2026 community controller retirement, zero-downtime configuration reloads and live binary upgrades on-the-fly (`kill -USR2`), comprehensive troubleshooting playbooks for 502/504/403 errors, Prometheus metrics and OpenTelemetry distributed tracing, Keepalived VRRP active-passive failover clustering, global CDN integration with origin cloaking, and an in-depth architectural comparison against Envoy, HAProxy, Traefik, and Caddy.

---

## Architectural Production Operations Lifecycle

```
                     CODE & CONFIGURATION IN GIT
                                  │
                                  ▼
                CI/CD VALIDATION PIPELINE (GitHub Actions)
                - Ephemeral Docker container syntax test: `nginx -t`
                - Security static analysis: GIXY
                - Deprecated syntax scanner: forbid `listen ... http2;`
                                  │
                                  ▼
            ZERO-DOWNTIME DEPLOYMENT OR LIVE BINARY UPGRADE
       ┌──────────────────────────┴──────────────────────────┐
       ▼                                                     ▼
 CONFIGURATION RELOAD (kill -HUP)              LIVE BINARY UPGRADE (kill -USR2)
 - Reads new config in-place                   - Forks new master with new binary
 - Drains old workers gracefully               - Inherits open socket descriptors
 - Zero dropped client packets                 - Instant atomic rollback via USR2/WINCH
                                  │
                                  ▼
             OBSERVABILITY, METRICS & RUNTIME MONITORING
 - Metrics: `stub_status` + Prometheus Exporter -> Grafana dashboards & alerts
 - Traces: OpenTelemetry `ngx_otel_module` -> W3C `traceparent` propagation
 - Logs: High-precision structured JSON access logs -> Vector / Elasticsearch
                                  │
                                  ▼
             INCIDENT RESPONSE & HIGH-AVAILABILITY CLUSTERING
 - High Availability: Keepalived VRRP floating VIP failover (1-3s failover)
 - Incident Triage: Error log dissection, `strace` syscalls, `tcpdump` packets
 - Edge Security: Origin cloaking, mTLS client certs, CDN shield integration
```

---

## Module Notes Catalog

| # | Note | Key Topics Covered |
| :--- | :--- | :--- |
| **01** | [Docker & Containerized Workloads](01-docker-and-containers.md) | Official image tags, multi-stage Dockerfiles, non-root `nginxinc/nginx-unprivileged` (UID 101), PID 1 signal handling (`SIGQUIT`), environment variable templating (`envsubst`), read-only root filesystems. |
| **02** | [Kubernetes Ingress & Gateway API](02-kubernetes-ingress-and-gateway-api.md) | Legacy Ingress API limitations, community `ingress-nginx` Lua dynamic balancers, **March 2026 EOL retirement**, NGINX Gateway Fabric, `GatewayClass`, `Gateway`, `HTTPRoute`, native canary traffic splitting. |
| **03** | [Operations, Zero-Downtime Reloads & CI](03-operations-zero-downtime-reloads-and-ci.md) | Atomic `nginx -t` deployment pipelines, graceful reloads (`SIGHUP`), live binary upgrades on the fly (`USR2`/`WINCH`), `logrotate` with `SIGUSR1`, hardened systemd service unit overrides. |
| **04** | [Troubleshooting Playbooks & Dissection](04-troubleshooting-playbooks.md) | Anatomy of `error.log` signatures, root-cause triage for 502 Bad Gateway (connection refused, OOM kills), 504 Gateway Timeout, 403 Forbidden (directory traversal, SELinux), 413/421 errors, `strace`, `tcpdump`, `gdb`. |
| **05** | [Monitoring Metrics & Observability](05-monitoring-metrics-and-observability.md) | `ngx_http_stub_status_module` counters (accepts, handled, requests, reading, writing, waiting), `nginx-prometheus-exporter`, OpenTelemetry distributed tracing (`ngx_otel_module`), structured JSON logging, Alertmanager rules. |
| **06** | [High Availability & Keepalived VRRP](06-high-availability-and-vrrp.md) | Active-passive and active-active dual-VIP clustering, Keepalived, Virtual Router Redundancy Protocol (VRRP), Gratuitous ARP (GARP), `ip_nonlocal_bind = 1`, health tracking scripts, split-brain mitigation. |
| **07** | [CDN & Edge Integration](07-cdn-and-edge-integration.md) | Origin cloaking, preventing direct-to-origin bypasses, Authenticated Origin Pulls (mTLS), `Cache-Control` hierarchies (`s-maxage` vs `max-age` vs `stale-while-revalidate`), Origin Shielding, Cloudflare/CloudFront tuning. |
| **08** | [NGINX vs Alternatives (Envoy, HAProxy, Traefik, Caddy)](08-nginx-vs-alternatives.md) | Technical comparison across concurrency architectures (multi-process vs multi-threaded vs goroutines), configuration dynamics (static files vs xDS APIs vs label discovery), latency jitter ($p99.9$), memory footprints. |

---

## Core Mental Models & Principles

1. **Graceful Draining Over Abrupt Termination**: Never issue `SIGTERM` or restart an edge proxy. Always signal `SIGQUIT` for termination and `SIGHUP` for configuration reloads, allowing active client connections to finish.
2. **Decoupled Gateway Roles**: The Kubernetes Gateway API separates cluster networking infrastructure (`Gateway`) from application routing rules (`HTTPRoute`), enabling independent team velocity without configuration sprawl.
3. **Outside-In Incident Triage**: Diagnose production outages from the edge inwards: verify network port binding -> test syntax -> correlate error log signatures -> trace system calls -> isolate upstream backend health.
4. **Origin Cloaking**: A CDN is not a security perimeter if your origin IP is publicly accessible. Enforce firewall whitelisting and mTLS client certificate verification on every edge ingress node.

---

[← Previous Module: 05-security-performance](../05-security-performance/README.md) | [Back to Track Index](../README.md) | [Next Module: 07-projects →](../07-projects/README.md)
