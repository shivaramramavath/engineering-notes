# Module 07: Production Reference Projects

[<- Back to Docker Track Overview](../README.md) | [Next Module: 08-interview ->](../08-interview/README.md)

---

## Overview

Theoretical understanding of Docker commands and Dockerfile syntax is only the first step. True container mastery requires building, hardening, testing, and debugging realistic end-to-end multi-service architectures.

This module provides **8 complete, end-to-end production reference projects**. Every project follows a strict 5-section engineering standard:
1. **Overview & Architecture**: ASCII architecture diagrams, component breakdown, design trade-offs.
2. **Directory Structure & File Listing**: Explicit project layout.
3. **Complete Implementation**: Complete configuration files, Dockerfiles, Compose manifests, and application source code inside markdown code fences.
4. **Step-by-Step Verification & Testing**: Verifiable CLI commands, test assertions, failure injection, and teardown.
5. **Production Considerations**: Security hardening, resource budgeting, zero-downtime updates, and observability.

---

## Project Catalog

| # | Project | Stack / Technologies | Key Architectural Patterns |
|:--|:--------|:---------------------|:---------------------------|
| **[01](./01-node-api-with-postgres-and-redis.md)** | Production Node.js API with PostgreSQL and Redis | Node.js 20, TypeScript, PostgreSQL 16, Redis 7 | Multi-stage distroless build, network isolation (`internal: true`), healthcheck sequencing (`depends_on: condition: service_healthy`), cache-aside pattern. |
| **[02](./02-python-fastapi-stack.md)** | Production Python FastAPI Stack with Celery and Redis | Python 3.12, FastAPI, Uvicorn, Celery, Redis | Virtualenv caching, distributed asynchronous worker queue, non-root user, polling API task status. |
| **[03](./03-spring-boot-with-database.md)** | Production Java Spring Boot with MySQL | Java 21, Spring Boot 3.3, MySQL 8.4 LTS, Actuator | Layered JAR extraction (`layertools`), JVM container ergonomics (`MaxRAMPercentage`), deep health probing. |
| **[04](./04-nginx-reverse-proxy-with-tls-local.md)** | NGINX Reverse Proxy with TLS and Rate Limiting | NGINX 1.27, OpenSSL, HTTP/2, Static SPA | TLS termination, rate limiting (`limit_req_zone`), enterprise security headers, path-based microservice routing. |
| **[05](./05-multi-stage-minimal-go-image.md)** | Minimal Multi-Stage Go Microservice with Distroless | Golang 1.22, Distroless Static, Scratch, Trivy | CGO-free static compilation, symbol stripping (`-ldflags="-s -w"`), < 15MB total image size, zero CVEs. |
| **[06](./06-full-dev-environment-with-compose-watch.md)** | Full-Stack Dev Environment with Compose Watch | React 18, Vite, Node.js Express, PostgreSQL 16 | Compose Watch (`sync`, `sync+restart`, `rebuild`), instant browser HMR, elimination of bind mount latency. |
| **[07](./07-secure-hardened-image-pipeline.md)** | Secure Hardened Container Pipeline | Hadolint, BuildKit, Trivy, Syft, Cosign, GitHub Actions | DevSecOps automation: Dockerfile linting, gated CVE vulnerability scanning, CycloneDX SBOM, Cosign keyless signing. |
| **[08](./08-local-registry-and-mirroring.md)** | Private Container Registry with Pull-Through Cache | Docker Distribution (Registry v2), htpasswd, TLS, Web UI | Self-hosted secure registry, Docker Hub pull-through cache mirror, credential authentication, web catalog. |

---

## How to Use These Reference Projects

Each reference project is completely self-contained within its respective markdown document. You can extract and execute the files directly on any host machine running Docker Engine 24+ and Docker Compose v2.20+.

```bash
# General Execution Workflow for Any Project:
cd 04-devops/docker/07-projects/
# Read the project note, create the directory structure, copy file contents, and run:
docker compose up -d --build
```

---

[<- Back to Docker Track Overview](../README.md) | [Next Module: 08-interview ->](../08-interview/README.md)
