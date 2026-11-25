# Module 05: Docker Compose v2

```
Track: DevOps & Containerization
Module: 05-compose
Prerequisites: 01-fundamentals, 02-commands, 03-dockerfile, 04-volumes-and-networking
Difficulty: Intermediate to Advanced
Estimated Duration: 8 hours
Target Docker Version: 27.x+ (Compose Specification v2.x+)
Last Validated: February 2025
```

---

## Overview

Modern software applications are distributed systems composed of multiple interacting services: APIs, databases, caches, queues, and reverse proxies. Managing these interconnected containers using individual, manual `docker run` commands is error-prone, fragile, and unmaintainable.

**Docker Compose** is the official declarative orchestration specification and toolchain for defining and managing multi-container architectures. This module delivers an exhaustive, enterprise-grade guide to **Docker Compose v2** (the modern Go-based CLI plugin), covering the unified Compose specification, multi-tier network segregation, deterministic startup coordination via healthchecks, environment variable precedence, configuration overrides, native file-syncing with Compose Watch, automated testing pipelines, and production hardening.

---

## Module Roadmap

```
05-compose/
├── 01-compose-basics.md                  # Compose v2 CLI, YAML anatomy, lifecycle commands, namespacing
├── 02-compose-multi-service.md           # Enterprise multi-tier architecture, network segregation, service discovery
├── 03-compose-file-reference.md          # Comprehensive directive-by-directive reference of Compose specification
├── 04-env-files-and-interpolation.md     # Variable precedence hierarchy, POSIX string interpolation, .env rules
├── 05-depends-on-and-healthchecks.md     # Directed Acyclic Graph (DAG) startup ordering, service_healthy, migrations
├── 06-profiles-and-overrides.md          # Selective service activation with profiles, multi-file YAML overrides
├── 07-compose-watch-and-dev-workflows.md # Compose Watch: sync, sync+restart, rebuild, inner dev loop acceleration
├── 08-compose-for-testing.md             # Ephemeral CI test environments, --abort-on-container-exit, --exit-code-from
├── 09-compose-in-production-limits.md    # Production feasibility, systemd supervision, zero-downtime blue/green
└── 10-migrating-from-compose-v1.md       # EOL migration playbook, Python to Go, hyphen naming, compatibility mode
```

---

## Detailed Notes Summary

| # | Note | Core Focus | Target Skills |
|:---|:---|:---|:---|
| **01** | [Compose Basics](01-compose-basics.md) | Compose v2 fundamentals | Define services, networks, volumes; master up, down, ps, exec |
| **02** | [Multi-Service Architecture](02-compose-multi-service.md) | Multi-tier topology | Segregate public vs internal networks, dual-homed gateways |
| **03** | [Compose File Reference](03-compose-file-reference.md) | Exhaustive schema reference | Master build, deploy limits, secrets, configs, long volume syntax |
| **04** | [Env Files & Interpolation](04-env-files-and-interpolation.md) | Environment precedence | Master the 7-tier evaluation hierarchy, escape literal $$, .env rules |
| **05** | [Depends-On & Healthchecks](05-depends-on-and-healthchecks.md) | Startup coordination | Eliminate race conditions with service_healthy & migration tasks |
| **06** | [Profiles and Overrides](06-profiles-and-overrides.md) | Configuration layering | Isolate dev vs prod with multi-file overrides & debug profiles |
| **07** | [Compose Watch](07-compose-watch-and-dev-workflows.md) | Fast inner dev loop | Hot-reload code with sub-50ms sync; auto-rebuild on manifest edit |
| **08** | [Compose for Testing](08-compose-for-testing.md) | Ephemeral CI test stacks | Run automated integration tests with --exit-code-from & tmpfs DBs |
| **09** | [Production Feasibility](09-compose-in-production-limits.md) | Production hardening | Supervise with systemd, log rotation, zero-downtime blue/green |
| **10** | [Migrating from v1 to v2](10-migrating-from-compose-v1.md) | Legacy migration playbook | Retire docker-compose v1, fix underscore naming, drop version tag |

---

## Learning Objectives

After completing this module, you will be able to:
1. Declaratively define, build, and orchestrate complex multi-container distributed architectures.
2. Design secure, multi-tier network topologies isolating databases from public web tiers.
3. Configure deterministic container boot sequences using active healthcheck probes (`condition: service_healthy`).
4. Navigate and control environment variable resolution across host shells, `.env` files, and container runtimes.
5. Reuse base configurations across development, staging, and production using multi-file overrides and profiles.
6. Accelerate local developer iteration speeds using native file synchronization via Compose Watch (`develop.watch`).
7. Implement robust, ephemeral integration testing pipelines in CI/CD using `--exit-code-from` and `--abort-on-container-exit`.
8. Safely operate single-node production workloads using Linux `systemd` supervision and zero-downtime Blue/Green deployments.
9. Migrate legacy Python `docker-compose` v1 setups to modern Go-based Compose v2.

---

[← Previous Module: 04-volumes-and-networking](../04-volumes-and-networking/README.md) | [Next Module: 06-production →](../06-production/README.md)
