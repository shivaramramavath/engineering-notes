# Module 03: Dockerfile Deep Dive

```
Track: DevOps & Containerization
Module: 03-dockerfile
Prerequisites: 00-setup, 01-fundamentals, 02-commands
Difficulty: Intermediate to Advanced
Estimated Duration: 8 hours
Target Docker Version: 27.x+
Last Validated: February 2025
```

---

## Overview

A `Dockerfile` is the declarative blueprint for building immutable container images. While simple Dockerfiles are trivial to write, crafting production-grade container images requires deep mastery of image layer caching, multi-stage compilation patterns, unprivileged process isolation, security linter compliance, and supply chain reproducibility.

This module provides exhaustive coverage of container image authoring from fundamental instruction mechanics to enterprise-level BuildKit cache optimization and multi-platform compilation.

---

## Module Roadmap

```
03-dockerfile/
├── 01-dockerfile-basics.md                # Dockerfile syntax, parser directives, and execution model
├── 02-instructions-reference.md           # Exhaustive reference of every Dockerfile instruction
├── 03-layers-and-caching.md               # Union filesystem layers, cache invalidation, and optimization
├── 04-multi-stage-builds.md               # Multi-stage compilation patterns, artifact pruning, targets
├── 05-dockerignore.md                     # Build context pruning, exclusion rules, performance gains
├── 06-buildkit-and-buildx.md              # BuildKit architecture, cache mounts, secret mounts, ssh mounts
├── 07-multi-platform-images.md            # Cross-compilation, QEMU emulation, OCI manifest lists (arm64/amd64)
├── 08-base-image-choice.md                # Alpine vs Debian-slim vs Distroless vs Scratch comparison
├── 09-healthchecks.md                     # In-container healthcheck probing and orchestration integration
├── 10-language-recipes.md                 # Production multi-stage recipes for Node, Python, Go, Java, Rust
├── 11-dockerfile-linting-with-hadolint.md # Static analysis, DL/SC rule enforcement, CI integration
└── 12-reproducible-builds-and-pinning.md  # Hermetic builds, SOURCE_DATE_EPOCH, SHA256 digest pinning
```

---

## Detailed Notes Summary

| # | Note | Core Focus | Target Skills |
|:---|:---|:---|:---|
| **01** | [Dockerfile Basics](01-dockerfile-basics.md) | Syntax, anatomy, parser directives | Structure clean Dockerfiles, understand context transfer |
| **02** | [Instructions Reference](02-instructions-reference.md) | Comprehensive command reference | Master ENTRYPOINT vs CMD, RUN vs COPY, USER, EXPOSE |
| **03** | [Layers & Caching](03-layers-and-caching.md) | Layer hashing, cache invalidation | Order instructions to achieve 90%+ build cache hit rate |
| **04** | [Multi-Stage Builds](04-multi-stage-builds.md) | Build/runtime stage separation | Eliminate compiler bloat, reduce image sizes by 80%+ |
| **05** | [Dockerignore](05-dockerignore.md) | Build context exclusion rules | Avoid sending git repos, secrets, and local artifacts |
| **06** | [BuildKit & Buildx](06-buildkit-and-buildx.md) | Advanced builder backend | Mount package caches, inject build secrets securely |
| **07** | [Multi-Platform Images](07-multi-platform-images.md) | Multi-arch images (ARM64/AMD64) | Cross-compile natively using Buildx and OCI manifest lists |
| **08** | [Base Image Choice](08-base-image-choice.md) | Alpine vs Slim vs Distroless | Select optimal base image considering glibc vs musl and CVEs |
| **09** | [Healthchecks](09-healthchecks.md) | Container liveness & readiness | Author robust probes with intervals, retries, start-periods |
| **10** | [Language Recipes](10-language-recipes.md) | Hardened stacks for 5 languages | Deploy Node, Python, Go, Spring Boot, and Rust cleanly |
| **11** | [Hadolint Linting](11-dockerfile-linting-with-hadolint.md) | Static AST code analysis | Catch bad practices and ShellCheck issues in CI gates |
| **12** | [Reproducible Builds](12-reproducible-builds-and-pinning.md) | Hermetic builds & digest pinning | Lock SHA256 digests and normalize timestamps for SLSA |

---

## Learning Objectives

After completing this module, you will be able to:
1. Author clean, maintainable, and cache-optimized Dockerfiles for any tech stack.
2. Structure multi-stage builds separating compilers and SDKs from lean runtime images.
3. Accelerate CI/CD pipelines using BuildKit cache mounts (`--mount=type=cache`).
4. Inject credentials safely into build steps with `--mount=type=secret` without leaking them into image layers.
5. Cross-compile multi-architecture images supporting Apple Silicon, AWS Graviton, and Intel/AMD x86_64.
6. Make informed architectural trade-offs between Alpine Linux, Debian Slim, Google Distroless, and `scratch`.
7. Define POSIX-compliant healthcheck probes that integrate seamlessly with container orchestrators.
8. Enforce automated static analysis using Hadolint in pre-commit hooks and GitHub Actions.
9. Attain SLSA-grade reproducible builds by locking immutable SHA256 digests and setting `SOURCE_DATE_EPOCH`.

---

[← Previous Module: 02-commands](../02-commands/README.md) | [Next Module: 04-volumes-and-networking →](../04-volumes-and-networking/README.md)
