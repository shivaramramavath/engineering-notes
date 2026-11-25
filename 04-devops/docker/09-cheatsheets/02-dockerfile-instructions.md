<!--
================================================================================
Metadata:
  Title: Quick Reference Cheat Sheet - Dockerfile Instructions
  Track: Docker / DevOps
  Difficulty: All Levels
  Estimated Time: 25 minutes
  Key Terminology: Dockerfile, BuildKit, Multi-stage, RUN mounts, Exec form, Shell form
================================================================================
-->

# Dockerfile Instructions Quick Reference Cheat Sheet

[<- 01-cli-commands.md](./01-cli-commands.md) | [Back to Cheatsheets Overview](./README.md) | [Next Cheatsheet: 03-compose-syntax.md ->](./03-compose-syntax.md)

---

## 1. Complete Instruction Reference Table

| Instruction | Syntax Example | Layer Created? | Description & Best Practice |
|:------------|:---------------|:---------------|:----------------------------|
| **`FROM`** | `FROM node:20-alpine AS builder` | Yes | Sets base image and initializes new build stage. Always pin specific version tags. |
| **`WORKDIR`** | `WORKDIR /app` | Yes (metadata) | Sets working directory for subsequent instructions. Creates folder if non-existent. |
| **`COPY`** | `COPY --chown=10001:10001 src/ ./src/` | Yes | Copies files/directories from host context. Supports `--from=<stage>` and `--chown`. |
| **`ADD`** | `ADD archive.tar.gz /app/` | Yes | Copies files; auto-extracts local tarballs and fetches URLs. Prefer `COPY` unless extracting. |
| **`RUN`** | `RUN npm ci && npm run build` | Yes | Executes commands in a new layer and commits result. Chain commands using `&&`. |
| **`ENV`** | `ENV NODE_ENV=production PORT=8080` | Yes (metadata) | Sets environment variables that persist in runtime containers and build steps. |
| **`ARG`** | `ARG VERSION=1.0.0` | No | Defines build-time variables passed via `--build-arg`. Not available in runtime containers. |
| **`EXPOSE`** | `EXPOSE 8080/tcp` | No | Informational port declaration metadata. Does not actually publish the port. |
| **`USER`** | `USER 10001:10001` | Yes (metadata) | Sets non-root user UID and GID for following instructions and container execution. |
| **`VOLUME`** | `VOLUME ["/var/lib/data"]` | Yes (metadata) | Declares a managed mount point. Changes made after `VOLUME` in build are discarded! |
| **`ENTRYPOINT`** | `ENTRYPOINT ["node", "dist/index.js"]` | Yes (metadata) | Configures the executable running as PID 1. Always use JSON Exec form. |
| **`CMD`** | `CMD ["--port", "8080"]` | Yes (metadata) | Provides default arguments to `ENTRYPOINT` or default executable. Easily overridden by CLI. |
| **`HEALTHCHECK`**| `HEALTHCHECK --interval=30s CMD curl -f http://localhost:8080/health \|\| exit 1` | Yes (metadata) | Defines command executed inside container to verify runtime health. |
| **`STOPSIGNAL`** | `STOPSIGNAL SIGTERM` | Yes (metadata) | Sets the POSIX signal sent to container to initiate graceful shutdown (default `SIGTERM`). |
| **`SHELL`** | `SHELL ["/bin/bash", "-c"]` | No | Overrides default shell used for shell-form commands (`/bin/sh -c` on Linux). |
| **`LABEL`** | `LABEL org.opencontainers.image.source="..."` | Yes (metadata) | Adds OCI metadata key-value annotations to the image. |

---

## 2. Exec Form vs. Shell Form

Every Dockerfile instruction that executes commands (`RUN`, `CMD`, `ENTRYPOINT`) can be written in two distinct forms:

```dockerfile
# -------------------------------------------------------------
# 1. JSON EXEC FORM (REQUIRED FOR PRODUCTION)
# -------------------------------------------------------------
ENTRYPOINT ["executable", "param1", "param2"]
CMD ["param1", "param2"]

# Mechanics: Spawns the binary directly as PID 1.
# Pros: Receives POSIX signals (SIGTERM) directly for graceful shutdown.
# Cons: Does NOT expand environment variables ($PORT) unless invoking a shell explicitly:
#       ENTRYPOINT ["sh", "-c", "exec app --port $PORT"]

# -------------------------------------------------------------
# 2. SHELL FORM (ANTI-PATTERN FOR ENTRYPOINT)
# -------------------------------------------------------------
ENTRYPOINT executable param1 param2
CMD executable param1 param2

# Mechanics: Docker invokes: /bin/sh -c "executable param1 param2".
# Cons: /bin/sh runs as PID 1; application runs as child process.
#       Shell swalllows SIGTERM, causing 10-second stop delays and SIGKILL exits!
```

---

## 3. BuildKit Advanced Mounts (`RUN --mount`)

Enable BuildKit syntax at top of Dockerfile: `# syntax=docker/dockerfile:1.7`

```dockerfile
# 1. Cache Mount (Accelerates dependency downloads without bloating image)
RUN --mount=type=cache,target=/root/.npm \
    npm ci

RUN --mount=type=cache,target=/root/.cache/pip \
    pip install -r requirements.txt

# 2. Secret Mount (Consumes build credentials without saving into image layers)
RUN --mount=type=secret,id=npmrc,target=/root/.npmrc \
    npm publish

# 3. SSH Mount (Uses host SSH agent to clone private git repositories)
RUN --mount=type=ssh \
    git clone git@github.com:myorg/private-repo.git

# 4. Bind Mount (Mounts host or stage files during execution without COPY)
RUN --mount=type=bind,source=package.json,target=package.json \
    npm install --package-lock-only

# 5. tmpfs Mount (Fast, RAM-backed temporary build directory)
RUN --mount=type=tmpfs,target=/tmp/build \
    make -C /tmp/build
```

---

## 4. Production Golden Template

```dockerfile
# syntax=docker/dockerfile:1.7
# check=error=true

# -------------------------------------------------------------
# Stage 1: Build & Compile
# -------------------------------------------------------------
FROM node:20.12-alpine3.19 AS builder

WORKDIR /build

COPY package*.json ./
RUN --mount=type=cache,target=/root/.npm \
    npm ci

COPY . .
RUN npm run build && npm prune --production

# -------------------------------------------------------------
# Stage 2: Hardened Runtime
# -------------------------------------------------------------
FROM node:20.12-alpine3.19 AS runtime

WORKDIR /app
ENV NODE_ENV=production

# Install curl for healthcheck
RUN apk add --no-cache curl

# Create dedicated non-root user (UID 10001)
RUN addgroup -g 10001 -S appgroup && \
    adduser -u 10001 -S appuser -G appgroup

# Copy compiled artifacts from builder
COPY --from=builder --chown=appuser:appgroup /build/dist ./dist
COPY --from=builder --chown=appuser:appgroup /build/node_modules ./node_modules
COPY --from=builder --chown=appuser:appgroup /build/package.json ./package.json

USER 10001:10001

HEALTHCHECK --interval=30s --timeout=3s --start-period=5s --retries=3 \
  CMD curl -f http://127.0.0.1:8080/healthz || exit 1

EXPOSE 8080

ENTRYPOINT ["node", "dist/index.js"]
```

---

[<- 01-cli-commands.md](./01-cli-commands.md) | [Back to Cheatsheets Overview](./README.md) | [Next Cheatsheet: 03-compose-syntax.md ->](./03-compose-syntax.md)
