# Cheatsheet 10: Docker & Containerization for Node.js Reference Card

> Production reference card for hardened multi-stage Docker builds, Google Distroless runners, and Docker Compose orchestration for Node.js 22 LTS services.

---

### 1. Hardened Multi-Stage `Dockerfile`
```dockerfile
# =======================================================
# Stage 1: Build & Dependencies
# =======================================================
FROM node:22-alpine AS builder

WORKDIR /app

# Copy lockfile first to leverage Docker layer caching
COPY package*.json tsconfig.json ./

# Install ALL dependencies (including devDependencies for build)
RUN npm ci

# Copy source code and build
COPY src/ ./src
RUN npm run build || true

# Prune devDependencies to keep production image minimal
RUN npm prune --omit=dev

# =======================================================
# Stage 2: Distroless Security Hardened Runner
# =======================================================
# Google Distroless has no shell, no package manager, no curl (Zero attack surface)
FROM gcr.io/distroless/nodejs22-debian12:nonroot

WORKDIR /app

# Copy production artifacts
COPY --from=builder /app/package.json ./package.json
COPY --from=builder /app/node_modules ./node_modules
COPY --from=builder /app/dist ./dist 2>/dev/null || true
COPY --from=builder /app/src ./src

# Run as non-root unprivileged user (UID 65532)
USER nonroot

EXPOSE 3000

ENV NODE_ENV=production
ENV PORT=3000

CMD ["src/index.js"]
```

---

### 2. Standard `.dockerignore`
```text
node_modules
dist
.git
.github
.env*
*.log
npm-debug.log*
.DS_Store
coverage
test
docs
```

---

### 3. Production `docker-compose.yml`
```yaml
version: '3.8'

services:
  app:
    build:
      context: .
      dockerfile: Dockerfile
    ports:
      - "3000:3000"
    environment:
      NODE_ENV: production
      PORT: 3000
      DATABASE_URL: postgresql://postgres:secret@postgres:5432/app_db
      REDIS_URL: redis://redis:6379
    deploy:
      resources:
        limits:
          cpus: '1.0'
          memory: 1024M
    depends_on:
      postgres:
        condition: service_healthy
      redis:
        condition: service_started
    restart: unless-stopped

  postgres:
    image: postgres:16-alpine
    environment:
      POSTGRES_DB: app_db
      POSTGRES_USER: postgres
      POSTGRES_PASSWORD: secret
    ports:
      - "5432:5432"
    volumes:
      - pgdata:/var/lib/postgresql/data
    healthcheck:
      test: ["CMD-SHELL", "pg_isready -U postgres -d app_db"]
      interval: 10s
      timeout: 5s
      retries: 5

  redis:
    image: redis:7-alpine
    ports:
      - "6379:6379"
    volumes:
      - redisdata:/data

volumes:
  pgdata:
  redisdata:
```

---

### 4. Container Security Rules
1. **Never run as root**: Always use `USER node` (in Alpine) or `USER nonroot` (in Distroless).
2. **Handle Signals Gracefully**: Node.js running as PID 1 in a container does not pass `SIGINT`/`SIGTERM` to children by default unless using an init system (`--init`) or direct signal listeners.
3. **Set Memory Limits**: Set `--max-old-space-size` to ~75% of container memory limit (e.g. 768MB for a 1GB container) to prevent the OS kernel OOM killer from abruptly terminating the container before V8 can trigger GC.
