# Module 09: Quick Reference Cheatsheets

[<- Back to Docker Track Overview](../README.md) | [Back to Interview Overview ->](../08-interview/README.md)

---

## Overview

When working in fast-paced production operations, incident triage, or continuous integration pipelines, having concise, authoritative syntax references and diagnostic flowcharts is essential.

This module provides **8 modular, battle-tested quick reference cheatsheets** covering CLI flags, Dockerfile instructions, Compose syntax, networking, BuildKit, security checklists, visual debugging decision trees, and top error fixes.

---

## Cheatsheets Catalog

| # | Cheatsheet | Key Topics Covered | Target Audience |
|:--|:-----------|:-------------------|:----------------|
| **[01](./01-cli-commands.md)** | Docker CLI Commands | Container lifecycle (`run`, `exec`, `stop`), inspection, Go template formatting (`--format`), pruning, shell aliases. | All Engineers |
| **[02](./02-dockerfile-instructions.md)** | Dockerfile Instructions | Complete instruction reference (`FROM` through `HEALTHCHECK`), JSON Exec vs Shell form, BuildKit mounts (`type=cache`, `type=secret`). | Developers & DevOps |
| **[03](./03-compose-syntax.md)** | Docker Compose v2 Syntax | Compose Spec blueprint, `depends_on: condition: service_healthy`, variable interpolation, `develop.watch`, profiles. | Developers & DevOps |
| **[04](./04-networking-and-volumes.md)** | Networking & Storage Volumes | Named volumes vs bind mounts vs tmpfs, volume backup/restore one-liners, network drivers, troubleshooting commands. | Systems Engineers & SREs |
| **[05](./05-buildx-and-buildkit.md)** | Docker Buildx & BuildKit | Builder instance management, multi-platform compilation (`--platform`), remote cache backends (`gha`, `s3`, `registry`), bake files. | CI/CD & Build Engineers |
| **[06](./06-security-checklist.md)** | Production Security Checklist | Actionable 4-tier security checklist: Dockerfile hardening, supply chain (Trivy/Cosign), runtime boundaries, CIS benchmark. | Security & SRE Leads |
| **[07](./07-debugging-flowchart.md)** | Production Debugging Flowcharts | ASCII decision trees for: container crash loops, unreachable networks, high latency, failing builds, and disk full outages. | On-Call Engineers & SREs |
| **[08](./08-common-errors-and-fixes.md)** | Common Errors & Fixes | Top 25 Docker error strings, root causes, and immediate 1-line CLI commands to resolve them. | All Engineers |

---

## How to Use These Cheatsheets

- **Bookmark for Incident Triage**: Use **[07-debugging-flowchart.md](./07-debugging-flowchart.md)** and **[08-common-errors-and-fixes.md](./08-common-errors-and-fixes.md)** during active PagerDuty pages to rapidly isolate root causes.
- **Pre-Production Auditing**: Walk through **[06-security-checklist.md](./06-security-checklist.md)** before signing off on production deployments.
- **CI/CD Pipeline Authoring**: Reference **[02-dockerfile-instructions.md](./02-dockerfile-instructions.md)** and **[05-buildx-and-buildkit.md](./05-buildx-and-buildkit.md)** when configuring GitHub Actions or GitLab CI build jobs.

---

[<- Back to Docker Track Overview](../README.md) | [Back to Interview Overview ->](../08-interview/README.md)
