# NGINX Learning Track: Changelog

This document tracks all architectural modifications, note rewrites, file renames, deprecated directive removals, and technical corrections executed within `04-devops/nginx/`.

---

## [Unreleased] - November 2026

### Initial Audit & Foundation Setup
- Created `NGINX_FACTS.md` establishing verified version baselines (1.30 stable, 1.31 mainline, Ubuntu 24.04 1.24.x), HTTP/2 deprecation notices, HTTP/3 QUIC syntax, NGINX Plus distinctions, and Kubernetes ingress-nginx March 2026 EOL.
- Created `NGINX_AUDIT.md` documenting structural non-conformance of 24 existing files.
- Created `NGINX_COMPLETION_PLAN.md` detailing the 83-file target curriculum and rename map.
- Created `NGINX_DECISIONS.md` recording ADRs 001 through 006.
- Created `NGINX_PROGRESS.md` establishing checklist tracking.

### Renames & Directory Migrations (Pending Git Execution)
- `01-fundamentals/installation.md` -> `01-fundamentals/01-installation.md`
- `01-fundamentals/config-structure.md` -> `01-fundamentals/02-config-structure.md`
- `01-fundamentals/server-and-location.md` -> `01-fundamentals/03-server-and-location.md`
- `01-fundamentals/static-files.md` -> `01-fundamentals/04-static-files.md`
- `02-core-features/logging.md` -> `02-core-features/01-logging.md`
- `02-core-features/reverse-proxy.md` -> `02-core-features/02-reverse-proxy.md`
- `02-core-features/tls-https.md` -> `02-core-features/03-tls-https.md`
- `02-core-features/variables-and-rewrites.md` -> `02-core-features/04-variables-and-rewrites.md`
- `03-traffic-management/caching.md` -> `03-traffic-management/01-caching.md`
- `03-traffic-management/load-balancing.md` -> `03-traffic-management/02-load-balancing.md`
- `03-traffic-management/rate-limiting.md` -> `03-traffic-management/03-rate-limiting.md`
- `04-internals/architecture.md` -> `04-internals/01-architecture-master-worker-and-event-loop.md`
- `04-internals/request-lifecycle.md` -> `04-internals/02-request-lifecycle-and-phases.md`
- `05-security-performance/performance-tuning.md` -> `05-security-performance/01-performance-tuning.md`
- `05-security-performance/security-hardening.md` -> `05-security-performance/02-security-hardening.md`
- `06-production/docker-kubernetes.md` -> `06-production/01-docker-and-containers.md`
- `06-production/operations.md` -> `06-production/03-operations-zero-downtime-reloads-and-ci.md`
- `06-production/troubleshooting.md` -> `06-production/04-troubleshooting-playbooks.md`
- `07-projects/secure-gateway-stack.md` -> `07-projects/01-secure-gateway-stack.md`
- `07-projects/spa-with-api-proxy.md` -> `07-projects/02-spa-with-api-proxy.md`
- `08-reference/interview-questions.md` -> `08-interview/01-fundamentals-qa.md`
- `08-reference/cheatsheet.md` -> `09-cheatsheets/01-directives-quick-reference.md`
