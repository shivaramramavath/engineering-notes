---
title: "Production Docker Multi-Stage Builds for NestJS"
description: "Authoring minimal, secure, production-hardened Docker multi-stage images for NestJS with Alpine/Distroless and non-root users"
tags: [nestjs, deployment, docker, multi-stage, security, distroless, alpine]
nestjs_version: "11.x"
node_version: ">=20.0.0"
express_version: "5.x"
---

# Production Docker Multi-Stage Builds for NestJS

## Conceptual Overview
Containerizing NestJS applications for production requires optimizing for:
- **Image Size**: Keeping image footprints small to accelerate container registry pushes and Kubernetes pod pull times.
- **Security**: Running processes as non-root users and eliminating unnecessary system build tools (`python`, `make`, `gcc`).
- **Layer Caching**: Structuring `Dockerfile` directives so dependency caching avoids reinstalling `node_modules` on every code change.
- **PID 1 Signal Handling**: Using lightweight init systems (`tini` / `dumb-init`) to reap zombie processes and forward signals properly.

---

## Production Dockerfile Implementation

```dockerfile
# ==========================================
# STAGE 1: Dependency & Build Stage
# ==========================================
FROM node:22-alpine AS builder

WORKDIR /usr/src/app

# Install build dependencies
COPY package*.json ./
RUN npm ci

# Copy application source code
COPY . .

# Build production distribution
RUN npm run build

# Prune development dependencies
RUN npm prune --omit=dev

# ==========================================
# STAGE 2: Production Runtime Stage
# ==========================================
FROM node:22-alpine AS runner

WORKDIR /usr/src/app

# Install dumb-init to handle PID 1 signal forwarding
RUN apk add --no-cache dumb-init

# Set production environment
ENV NODE_ENV=production

# Copy non-dev dependencies and compiled output from builder
COPY --chown=node:node --from=builder /usr/src/app/node_modules ./node_modules
COPY --chown=node:node --from=builder /usr/src/app/dist ./dist
COPY --chown=node:node --from=builder /usr/src/app/package.json ./package.json

# Run as unprivileged non-root user
USER node

# Expose HTTP port
EXPOSE 3000

# Execute via dumb-init
ENTRYPOINT ["/usr/bin/dumb-init", "--"]
CMD ["node", "dist/main.js"]
```

---

## Key Takeaways & Cheat Sheet

- Use multi-stage builds to discard TypeScript source code, dev dependencies, and compilers from the final image.
- Always switch to `USER node` to prevent root privilege escalation inside containers.
- Use `dumb-init` or `tini` as entrypoint for proper signal handling.

---

## Cross-References
- [02-graceful-shutdown-and-process-management.md](./02-graceful-shutdown-and-process-management.md) - Signal handling
- [07-kubernetes.md](./07-kubernetes.md) - Kubernetes pod specs
