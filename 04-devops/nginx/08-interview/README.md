# Module 08: Technical Interview Preparation

## Overview

Module 08 of the NGINX curriculum is a comprehensive, senior-to-staff level technical interview preparation guide designed for DevOps engineers, Site Reliability Engineers (SREs), infrastructure architects, and backend systems developers.

This module provides exhaustive, categorized question-and-answer guides, deep architectural breakdowns, diagnostic scenarios, and system design case studies across 8 specialized domains.

---

## Interview Guide Catalog

| # | Note | Domain & Core Focus Areas |
| :--- | :--- | :--- |
| **01** | [Core Fundamentals & Configuration QA](01-fundamentals-qa.md) | Concurrency architecture, master vs worker processes, context hierarchy, array directive inheritance traps, `root` vs `alias` off-by-slash vulnerabilities, zero-downtime `kill -HUP` reload mechanics. |
| **02** | [Location Matching & Rewrites QA](02-location-matching-and-rewrites-qa.md) | Complete priority algorithm (`=` vs `^~` vs regex vs prefix), rewrite flags (`last`, `break`, `redirect`, `permanent`), the "If is Evil" rule, internal redirect cycles, CRLF injection defenses. |
| **03** | [Reverse Proxying & Upstreams QA](03-reverse-proxy-and-load-balancing-qa.md) | Trailing slash URI transformations in `proxy_pass`, upstream keepalive connection pooling (`Connection ""`), load balancing algorithms (`least_conn`, `consistent hash`), passive circuit breaking (`max_fails`), buffer tuning. |
| **04** | [TLS/HTTPS & Security QA](04-tls-and-security-qa.md) | TLS 1.3 protocol advantages, modern cipher suites, Perfect Forward Secrecy, HSTS preloading risks, OCSP stapling mechanics, client security headers (CSP, nosniff), WAF anomaly scoring, Slowloris mitigation. |
| **05** | [Performance & Internals QA](05-performance-and-internals-qa.md) | Non-blocking `epoll` loops, `reuseport` socket sharding, the 11 HTTP processing phases, `ngx_pool_t` arena allocators, shared memory slab buddy allocators, kernel sysctl tuning, Coordinated Omission, Flame Graphs. |
| **06** | [Production Troubleshooting Scenarios](06-production-troubleshooting-scenarios.md) | Step-by-step triage playbooks for 502 Bad Gateway during rolling deploys, 504 Gateway Timeouts, 403 Forbidden with SELinux, ephemeral port exhaustion, WebSocket dropouts, sensitive `.env` leaks, corrupted JSON logs. |
| **07** | [NGINX vs The Alternatives QA](07-nginx-vs-alternatives-qa.md) | Deep comparisons of NGINX vs Envoy Proxy (xDS dynamic APIs, service mesh), HAProxy (L4 performance, TCP queueing), Traefik & Caddy (Go GC pauses, auto-HTTPS), and Kubernetes Ingress vs Gateway API (March 2026 EOL). |
| **08** | [Staff Architecture & Behavioral Discussions](08-behavioural-and-architecture-discussion.md) | Staff-level architectural designs for 1M+ RPS edge tiers, active-active multi-region disaster recovery, cutting cloud NAT/ALB costs by 70%, leading 300-service Gateway API migrations, blameless post-mortems. |

---

## How to Prepare

1. **Self-Assessment**: Review the collapsible `<details><summary>` question blocks. Attempt to explain the model answer out loud before expanding the solution.
2. **Deep Dives**: For any topic where your understanding is theoretical, follow the cross-links to the corresponding curriculum modules in `01-fundamentals` through `06-production`.
3. **Hands-On Practice**: Implement the live technical scenarios in local Docker containers or KinD clusters to build muscle memory.

---

[← Previous Module: 07-projects](../07-projects/README.md) | [Back to Track Index](../README.md) | [Next Module: 09-cheatsheets →](../09-cheatsheets/README.md)
