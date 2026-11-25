# Docker Track Decisions Log

This document records architectural, pedagogical, and structural decisions made during the Docker track audit, restructuring, and authoring.

---

## 1. Directory and File Naming Standardisation

- **Decision**: Migrate all `0N_snake_case` directories to `0N-kebab-case`. Specifically:
  - `00_setup` $\rightarrow$ `00-setup`
  - `01_docker_fundamentals` $\rightarrow$ `01-fundamentals`
  - `02_docker_commands` $\rightarrow$ `02-commands`
  - `03_dockerfile` $\rightarrow$ `03-dockerfile`
  - `04_volumes_networking` $\rightarrow$ `04-volumes-and-networking`
  - `05_docker_compose` $\rightarrow$ `05-compose`
  - `06_production` $\rightarrow$ `06-production`
  - Add `07-projects`
  - Add `08-interview`
  - Add `09-cheatsheets`
- **Rationale**: Strict compliance with repository-wide convention (`NN-kebab-case`) observed across Java, TypeScript, Next.js, Node.js, NestJS, Python, FastAPI, Spring Boot, and AWS tracks.
- **Date**: 2026-11-25

---

## 2. Docker CLI and Engine Baseline

- **Decision**: Anchor all syntax and instruction examples on modern Docker Engine (v28/v29+) and Docker CLI (v29.8+). Use `docker compose` (v2 plugin) rather than legacy `docker-compose` v1 (standalone python binary).
- **Rationale**: Docker Compose v1 was deprecated in 2023 and end-of-lifed. Modern Docker CLI uses the sub-command syntax (`docker compose`).
- **Date**: 2026-11-25

---

## 3. Deprecation of the `version:` Key in Compose Files

- **Decision**: Omit the top-level `version:` attribute (e.g. `version: '3.8'`) in all `compose.yaml` files.
- **Rationale**: The Compose Specification standard has rendered the top-level `version:` key obsolete. Modern Docker Compose issues deprecation warnings if `version:` is specified.
- **Date**: 2026-11-25

---

## 4. BuildKit by Default and Multi-Stage Best Practices

- **Decision**: All Dockerfile explanations and builds assume BuildKit (`DOCKER_BUILDKIT=1`, default in modern CLI). Dockerfile syntax uses `# syntax=docker/dockerfile:1` and leverages `--mount=type=cache` and `--mount=type=secret` where appropriate.
- **Rationale**: BuildKit provides parallel stage execution, cache mounts that do not bloat image layers, and secure secret handling without leaking keys into intermediate layers or image history.
- **Date**: 2026-11-25

---

## 5. Strict 13-Section Concept Note Template

- **Decision**: All 86 concept notes across Modules 00-06 and 08-09 strictly adhere to the repository 13-section format:
  1. Title & Metadata Header
  2. Table of Contents
  3. 1. What It Is & Why It Matters
  4. 2. Core Concepts & Terminology
  5. 3. How It Works Internally (with ASCII/mermaid diagrams)
  6. 4. Syntax & Usage
  7. 5. Practical Examples (5 progressively complex, production-grade examples)
  8. 6. Best Practices & Production Guidelines
  9. 7. Common Pitfalls & Anti-Patterns ($\ge 6$ tabulated anti-patterns)
  10. 8. Performance, Security & Cost Implications
  11. 9. Comparison Table
  12. 10. Technical Interview Questions (8 questions in `<details>` blocks)
  13. 11. Hands-On Practice Exercises (6 exercises with solutions in `<details>` blocks)
  14. 12. Summary / Cheat Sheet
  15. 13. Further Reading & References
- **Rationale**: Enforces deep technical rigor and cross-track consistency across the entire DevOps engineering notes repository.
- **Date**: 2026-11-25

---

## 6. Strict 5-Section Reference Project Template

- **Decision**: All 8 projects in Module 07 follow the 5-section reference project standard:
  1. Architecture & Design Overview (with ASCII topology diagram)
  2. Complete Configuration & Fenced File Tree (all Dockerfiles, compose files, and configs inside `.md` code blocks)
  3. Step-by-Step Deployment & Verification Guide
  4. Production Hardening & Security Audit
  5. Troubleshooting & Observability Guide
- **Rationale**: Keeps project files self-contained within markdown while delivering copy-paste ready, production-grade architectures.
- **Date**: 2026-11-25
