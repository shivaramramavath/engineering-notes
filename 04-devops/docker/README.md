# Docker & Container Engineering Learning Track

[<- Back to DevOps Area Overview](../README.md) | [Track Progress](./DOCKER_PROGRESS.md) | [Audit Report](./DOCKER_AUDIT.md)

---

## Track Overview

Welcome to the **Docker & Container Engineering Learning Track**. This curriculum is a comprehensive, production-grade learning system designed to take software engineers, DevOps specialists, and Site Reliability Engineers from foundational container concepts to advanced production orchestration, Linux kernel internals, security compliance, and disaster recovery.

Every note in this curriculum follows strict engineering standards: conceptual foundations, internal kernel mechanics, ASCII architectural diagrams, verified CLI commands, practical multi-service examples, best practices, troubleshooting matrices, technical interview preparation Q&As, and hands-on practice exercises with complete solutions.

---

## Architectural Blueprint

```
+----------------------------------------------------------------------------------------------------+
|                                  DOCKER TRACK ARCHITECTURAL MAP                                     |
|                                                                                                    |
|  [ 00-SETUP ] -----------------> [ 01-FUNDAMENTALS ] --------------> [ 02-COMMANDS ]               |
|  - Engine Installation           - Namespaces (PID/NET/MNT)           - Container Lifecycle        |
|  - Linux/macOS/Windows           - cgroups v1 & v2                    - Image Operations           |
|  - Rootless Docker               - UnionFS (overlay2)                 - System Pruning & Formats   |
|            |                                                                    |                  |
|            v                                                                    v                  |
|  [ 03-DOCKERFILE ] ------------> [ 04-VOLUMES-AND-NETWORKING ] ----> [ 05-COMPOSE ]                |
|  - Multi-Stage Builds            - Named Volumes & Bind Mounts        - Compose Spec Blueprint     |
|  - BuildKit Caching              - Bridge, Host & Overlay Networks    - depends_on healthy         |
|  - Multi-Platform Buildx         - iptables & Port Forwarding         - Compose Watch DevEx        |
|            |                                                                    |                  |
|            v                                                                    v                  |
|  [ 06-PRODUCTION ] ------------> [ 07-PROJECTS ] ------------------> [ 08-INTERVIEW ]              |
|  - Hardening & Security          - 8 End-to-End Reference Stacks      - 125 In-Depth Q&As          |
|  - Vulnerability Scanning        - Node, Python, Spring, NGINX        - 10 Incident Scenarios      |
|  - Image Signing (Cosign)        - Minimal Go, Compose Watch          - STAR Method Behavioral     |
|  - cgroups Resource Limits       - DevSecOps Pipeline, Registry                                    |
|            |                                                                                       |
|            +-------------------> [ 09-CHEATSHEETS ] -----------------------------------------------+
|                                  - CLI, Dockerfile, Compose, Net, Security, Debugging Flowcharts   |
+----------------------------------------------------------------------------------------------------+
```

---

## Curriculum Modules Summary (94 Notes + 10 Module Guides)

| Module | Title | Notes | Focus Areas |
|:-------|:------|:-----:|:------------|
| **[00-setup](./00-setup/README.md)** | Setup and Installation | 6 | Linux Engine install, Docker Desktop (Mac/Win), Rootless Docker, CLI completion, validation, and post-install configuration. |
| **[01-fundamentals](./01-fundamentals/README.md)** | Core Fundamentals | 9 | Docker vs VMs, Images vs Containers, Architecture (`dockerd`/`containerd`/`runc`), OCI standards, Linux namespaces, cgroups v1/v2, UnionFS (`overlay2`), process lifecycles. |
| **[02-commands](./02-commands/README.md)** | Essential CLI Commands | 10 | Container lifecycle (`run`, `stop`, `rm`), inspection & logging (`logs`, `inspect`, `stats`), image ops, interactive exec, cleanup & pruning, Go templates formatting. |
| **[03-dockerfile](./03-dockerfile/README.md)** | Dockerfile Deep Dive | 12 | Instruction reference, layer caching mechanics, multi-stage builds, `.dockerignore`, BuildKit & Buildx, multi-platform images, base image selection (Alpine vs Distroless), Hadolint linting. |
| **[04-volumes-and-networking](./04-volumes-and-networking/README.md)** | Storage and Networking | 9 | Named volumes, host bind mounts, `tmpfs`, volume backups, network drivers (bridge, host, overlay, macvlan), embedded DNS (`127.0.0.11`), iptables firewall chains, troubleshooting. |
| **[05-compose](./05-compose/README.md)** | Docker Compose v2 | 10 | Compose Spec, multi-service topologies, environment interpolation, `depends_on: condition: service_healthy`, profiles, multi-file merging, Compose Watch (`develop.watch`), testing workflows. |
| **[06-production](./06-production/README.md)** | Production Engineering & Security | 14 | Secrets management, image optimization, CI/CD pipelines, container hardening (non-root, capabilities, seccomp), Trivy scanning, Cosign signing, logging drivers, cgroups OOM handling, daemon tuning, Swarm, and K8s translation. |
| **[07-projects](./07-projects/README.md)** | Production Reference Projects | 8 | 8 complete reference architectures: Node.js/Postgres/Redis, Python FastAPI/Celery, Spring Boot/MySQL, NGINX TLS Proxy, Minimal Go Distroless, Compose Watch DevEx, DevSecOps Pipeline, Local Registry. |
| **[08-interview](./08-interview/README.md)** | Technical Interview Preparation | 8 | 125 deep technical Q&As across basics, Dockerfile, networking, compose, security, 10 complex production incident triage scenarios, architectural trade-offs, and STAR behavioral scenarios. |
| **[09-cheatsheets](./09-cheatsheets/README.md)** | Quick Reference Cheatsheets | 8 | Rapid lookup cheatsheets: CLI commands, Dockerfile syntax, Compose specification, networking & storage, Buildx & BuildKit, production security checklist, debugging flowcharts, top 25 errors & fixes. |

---

## Cross-Curriculum Integration

This Docker track integrates directly with surrounding engineering tracks across this repository:
- **[04-devops/linux](../linux/README.md)**: Deepens knowledge of underlying Linux kernel namespaces, cgroups v2, and netfilter iptables rules.
- **[04-devops/nginx](../nginx/README.md)**: Details production reverse proxy configurations, TLS termination, and upstream load balancing for container fleets.
- **[04-devops/aws](../aws/README.md)**: Translates local containerized applications into cloud-managed container services (Amazon ECS, EKS, and AWS Fargate).
- **[04-devops/git-github](../git-github/README.md)**: Explores automated GitHub Actions CI/CD workflows, image build matrixing, and container registry publishing.
- **[01-languages/](../../01-languages/) & [03-backend/](../../03-backend/)**: Provides optimized Dockerfile templates for Node.js, Python FastAPI, Java Spring Boot, and Go backends.

---

## Governance & Tracking Documents

- **[DOCKER_FACTS.md](./DOCKER_FACTS.md)**: Technical facts and architectural assumptions.
- **[DOCKER_AUDIT.md](./DOCKER_AUDIT.md)**: Pre-reorganization audit and legacy gap analysis.
- **[DOCKER_COMPLETION_PLAN.md](./DOCKER_COMPLETION_PLAN.md)**: 10-module curriculum blueprint and delivery milestones.
- **[DOCKER_DECISIONS.md](./DOCKER_DECISIONS.md)**: Architecture Decision Records (ADRs) for naming conventions and tooling choices.
- **[DOCKER_CHANGELOG.md](./DOCKER_CHANGELOG.md)**: Detailed changelog of all authored notes and project assets.
- **[DOCKER_PROGRESS.md](./DOCKER_PROGRESS.md)**: Real-time progress dashboard tracking 100% completion.
- **[DOCKER_FINAL_REPORT.md](./DOCKER_FINAL_REPORT.md)**: Comprehensive final curriculum verification and delivery report.

---

[<- Back to DevOps Area Overview](../README.md) | [Track Progress](./DOCKER_PROGRESS.md) | [Audit Report](./DOCKER_AUDIT.md)
