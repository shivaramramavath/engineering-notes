---
title: "NestJS Production Deployment Readiness Checklist"
description: "Comprehensive 50-point pre-flight deployment checklist covering infrastructure, databases, security, and rollback readiness"
tags: [nestjs, deployment, checklist, production-readiness, devops]
nestjs_version: "11.x"
node_version: ">=20.0.0"
express_version: "5.x"
---

# NestJS Production Deployment Readiness Checklist

## Production Readiness Verification

### 1. Build & Packaging
- [ ] Multi-stage Docker build producing an unprivileged (`USER node`) runtime image.
- [ ] No devDependencies present in final production image (`npm prune --omit=dev`).
- [ ] Signal forwarder (`dumb-init` / `tini`) configured as container entrypoint.
- [ ] Docker image tagged with git commit SHA.

### 2. Runtime & Configuration
- [ ] Environment variables validated at boot time via Zod/Joi schema.
- [ ] `app.enableShutdownHooks()` explicitly invoked in `main.ts`.
- [ ] Database connection pools sized to match CPU cores.
- [ ] Reverse proxy trust enabled (`app.set('trust proxy', 1)`).

### 3. Resilience & Health
- [ ] Kubernetes liveness probe configured to `/health/liveness` (lightweight).
- [ ] Kubernetes readiness probe configured to `/health/readiness` (dependency checks).
- [ ] Horizontal Pod Autoscaler (HPA) configured with min/max replica boundaries.
- [ ] PodDisruptionBudget configured to guarantee minimum quorum during node drains.

### 4. Observability & Support
- [ ] Structured JSON logging emitted to stdout.
- [ ] Request correlation IDs generated and propagated across all services.
- [ ] Prometheus metrics exposed at `/metrics` (internal network only).
- [ ] Automated error alerts configured via Sentry / PagerDuty.
- [ ] Rollback strategy tested and verified.

---

## Cross-References
- [08-security-checklist.md](../01-security/08-security-checklist.md) - Security checklist
- [07-kubernetes.md](./07-kubernetes.md) - Kubernetes manifests
