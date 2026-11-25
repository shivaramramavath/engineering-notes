# Docker Track Changelog

All notable changes, restructuring, file movements, and additions to the Docker curriculum track are documented in this file.

---

## [2.0.0] - 2026-11-25 - Complete 100% Curriculum Delivery

### Major Overhaul & Restructuring
- Restructured entire Docker track from 7 legacy snake_case directories (20 partial notes) into a complete 10-module curriculum conforming to standard `NN-kebab-case`:
  - `00_setup` $\rightarrow$ `00-setup` (6 notes + README)
  - `01_docker_fundamentals` $\rightarrow$ `01-fundamentals` (9 notes + README)
  - `02_docker_commands` $\rightarrow$ `02-commands` (10 notes + README)
  - `03_dockerfile` $\rightarrow$ `03-dockerfile` (12 notes + README)
  - `04_volumes_networking` $\rightarrow$ `04-volumes-and-networking` (9 notes + README)
  - `05_docker_compose` $\rightarrow$ `05-compose` (10 notes + README)
  - `06_production` $\rightarrow$ `06-production` (14 notes + README)
  - `07-projects` (8 reference projects + README)
  - `08-interview` (8 interview guides + README)
  - `09-cheatsheets` (8 quick reference guides + README)
- Created track root `README.md` and updated `04-devops/` area root files (`README.md`, `INDEX.md`, `ROADMAP.md`, `GLOSSARY.md`).
- Authored 94 deep concept and project notes + 10 module overview READMEs + 1 track root README (total 105 markdown files).
- Strict 13-section concept note template compliance across all concept notes: Metadata, TOC, What & Why, Core Concepts, Internal Mechanics with ASCII diagrams, Syntax & Usage, 5+ Practical Examples, Best Practices, 6+ Troubleshooting rows, Performance/Security/Cost, Comparison Tables, 8 Collapsible Interview Questions, 6 Collapsible Practice Exercises, Summary Cheat Sheet, and Further Reading.

### Governance Documents Created
- `DOCKER_FACTS.md`: Technical facts, version baselines (Docker 26.1+, Compose v2.27+, BuildKit 0.13+), and deprecation matrices.
- `DOCKER_AUDIT.md`: Pre-reorganization audit and gap analysis of legacy notes.
- `DOCKER_COMPLETION_PLAN.md`: Complete 10-module curriculum roadmap.
- `DOCKER_DECISIONS.md`: Architecture Decision Records (ADRs 001–007) documenting conventions and syntax choices.
- `DOCKER_PROGRESS.md`: Real-time dashboard tracking 100% completion across all 94 notes.
- `DOCKER_FINAL_REPORT.md`: Comprehensive final curriculum delivery and verification report.

### Modules Delivered
- **Module 00: Setup & Installation**: 6 notes + README covering Linux engine, Docker Desktop (Mac/Win), rootless daemon, alternatives (Podman/containerd), remote contexts, and first container walkthrough.
- **Module 01: Core Fundamentals**: 9 notes + README covering Docker vs VMs, images vs containers, daemon architecture, OCI standards, Linux namespaces, cgroups v1/v2, UnionFS (`overlay2`), container lifecycle states, and PID 1 signal reaping.
- **Module 02: Essential Commands**: 10 notes + README covering container lifecycle (`run`, `stop`, `rm`), inspection & logging (`logs`, `inspect`, `stats`), image management, build commands, cleanup, resource limits, restart policies, exec patterns, daemon events, and Go template formatting.
- **Module 03: Dockerfile Deep Dive**: 12 notes + README covering instruction reference, layer caching mechanics, multi-stage builds, `.dockerignore`, BuildKit & Buildx, multi-platform images, base image selection (Alpine vs Distroless vs Debian-slim), healthchecks, language recipes (Node, Python, Java, Go, Rust), Hadolint linting, and reproducible pinning.
- **Module 04: Storage and Networking**: 9 notes + README covering named volumes, bind mounts, `tmpfs`, volume backups, network drivers (bridge, host, overlay, macvlan), embedded DNS (`127.0.0.11`), iptables firewall chains, container-to-host communication, IPv6, and network troubleshooting.
- **Module 05: Docker Compose v2**: 10 notes + README covering Compose Spec blueprint, multi-service topologies, environment interpolation, `depends_on: condition: service_healthy`, profiles, multi-file merging, Compose Watch (`develop.watch`), testing workflows, production boundaries, and Compose v1 migration.
- **Module 06: Production Engineering & Security**: 14 notes + README covering secrets management, image optimization, CI/CD pipelines, container hardening (non-root, capabilities, seccomp), Trivy scanning, Cosign signing, registries, tagging strategies, logging drivers, cgroups OOM handling, daemon tuning, Swarm, and K8s translation.
- **Module 07: Production Reference Projects**: 8 complete reference projects + README: Node/Postgres/Redis, Python FastAPI/Celery, Spring Boot/MySQL, NGINX TLS Proxy, Minimal Go Distroless, Compose Watch DevEx, DevSecOps Pipeline, and Local Authenticated Registry.
- **Module 08: Technical Interview Preparation**: 8 interview guides + README containing 125 deep technical Q&As, 10 complex incident triage scenarios, architectural trade-offs, and STAR behavioral scenarios.
- **Module 09: Quick Reference Cheatsheets**: 8 rapid lookup guides + README covering CLI commands, Dockerfile syntax, Compose specification, networking & storage, Buildx & BuildKit, production security checklist, debugging flowcharts, and top 25 errors & fixes.

---

[<- Back to Docker Track Overview](./README.md)
