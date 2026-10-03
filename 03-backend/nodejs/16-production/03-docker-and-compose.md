# Docker & Compose

Packaging your app into a small, secure, cache-friendly container image, and running it together with its dependencies using Docker Compose.

## Why containers

A container image bundles your code, the Node.js runtime, system libraries, and dependencies into **one immutable artifact** that runs identically on your laptop, in CI, and in production.

| Problem | How containers help |
|---|---|
| "Works on my machine" (different Node/OS/library versions) | The image *is* the environment |
| Deployment = copy files + install + hope | Deployment = run the image you already tested |
| Rolling back means undoing changes on a server | Rollback = run the previous image |
| Dependencies conflict between apps on a server | Each container is isolated |
| Hard to scale out | Run more copies of the same image |
| Dev/prod drift | Run the same Postgres/Redis versions locally via Compose |

Vocabulary:

| Term | Meaning |
|---|---|
| **Image** | A read-only, layered template: your app + its environment |
| **Container** | A running instance of an image |
| **Dockerfile** | The recipe that builds an image |
| **Registry** | A store for images (Docker Hub, **AWS ECR**, GitHub Container Registry) |
| **Tag** | A label on an image version (`orders-api:3f9c2ab`) |
| **Layer** | One cached step in the build; unchanged layers are reused |
| **Compose** | A tool for defining and running multi-container setups from one YAML file |

---

## A production-ready Dockerfile

```dockerfile
# syntax=docker/dockerfile:1

ARG NODE_VERSION=22

# ---------- 1. Base ----------
FROM node:${NODE_VERSION}-slim AS base
WORKDIR /app

# ---------- 2. Production dependencies only ----------
FROM base AS prod-deps
COPY package.json package-lock.json ./
RUN --mount=type=cache,target=/root/.npm \
    npm ci --omit=dev

# ---------- 3. Build (needs devDependencies; skip this stage for plain JavaScript) ----------
FROM base AS build
COPY package.json package-lock.json ./
RUN --mount=type=cache,target=/root/.npm \
    npm ci
COPY . .
RUN npm run build                       # e.g. tsc → dist/

# ---------- 4. Runtime: the small final image ----------
FROM base AS runtime
ENV NODE_ENV=production
COPY --from=prod-deps --chown=node:node /app/node_modules ./node_modules
COPY --from=build     --chown=node:node /app/dist ./dist
COPY --chown=node:node package.json ./

USER node                               # never run as root
EXPOSE 3000

HEALTHCHECK --interval=15s --timeout=3s --start-period=20s --retries=3 \
  CMD node -e "fetch('http://127.0.0.1:3000/healthz').then(r=>process.exit(r.ok?0:1)).catch(()=>process.exit(1))"

CMD ["node", "dist/server.js"]
```

For a plain JavaScript project with no build step, drop stage 3 and copy `src/` directly:

```dockerfile
FROM base AS runtime
ENV NODE_ENV=production
COPY --from=prod-deps --chown=node:node /app/node_modules ./node_modules
COPY --chown=node:node package.json ./
COPY --chown=node:node src ./src
USER node
EXPOSE 3000
CMD ["node", "src/server.js"]
```

```bash
docker build -t orders-api:dev .
docker run --rm -p 3000:3000 --env-file .env orders-api:dev
```

Now let's go through *why* each decision is made.

---

## Dockerfile best practices, explained

### 1. Choose the base image deliberately

| Image | Size | Notes |
|---|---|---|
| `node:22` (full Debian) | ~1 GB | Includes compilers and tools; convenient for builds, too big to ship |
| **`node:22-slim`** | ~200 MB | Debian without extras. **A solid default**: glibc compatibility with native modules |
| `node:22-alpine` | ~150 MB | Uses **musl libc**; smallest, but some native modules (`bcrypt`, `sharp`, `canvas`, `grpc`, DB drivers) can misbehave or need extra build steps. Check prebuilt-binary support before choosing it |
| **Distroless** (`gcr.io/distroless/nodejs22-debian12`) | ~170 MB | No shell, no package manager: a minimal attack surface; harder to debug (`docker exec` can't open a shell) |
| `node:22-bookworm` | ~350 MB | Debian with common build tools; good for the *build* stage |

```dockerfile
FROM node:22-slim                        # ✅ pin a major (or exact) version
FROM node:latest                         # ❌ changes under you; builds aren't reproducible
```

- **Pin versions:** at least the major (`22`), better the minor (`22.11`), and for maximum reproducibility pin the **digest** (`node:22-slim@sha256:...`) and let Dependabot/Renovate bump it.
- Use an **LTS** release of Node (even-numbered major versions) and match what you run in CI and locally (`.nvmrc` / `engines`).
- Rebuild regularly: base images get security patches.

### 2. Order layers for caching

Docker caches each instruction's result and reuses it **until something above it changes.** Put what changes *rarely* first, and what changes *often* last.

```dockerfile
# ✅ dependencies installed in a layer that only rebuilds when the lockfile changes
COPY package.json package-lock.json ./
RUN npm ci --omit=dev
COPY src ./src                           # code changes every commit, but the install layer above stays cached

# ❌ copying everything first: any code change invalidates the npm install layer → 2 minutes per build
COPY . .
RUN npm ci
```

The BuildKit **cache mount** (`--mount=type=cache,target=/root/.npm`) keeps npm's download cache between builds, so even when the lockfile changes, packages aren't re-downloaded. (BuildKit is the default builder in current Docker.)

### 3. Use `npm ci`, not `npm install`

- **`npm ci`** installs *exactly* what's in `package-lock.json`, fails if the lockfile and `package.json` disagree, and is faster and deterministic.
- **`--omit=dev`** (formerly `--production`) leaves out `devDependencies`, so test frameworks and linters aren't in the image.
- Commit `package-lock.json`.

### 4. Multi-stage builds: build with tools, ship without them

The **build** stage can use compilers, TypeScript, bundlers, and devDependencies. The **runtime** stage copies only the output:

```
Stage "build"   (large: devDeps, source, compiler)  ──copy dist/──▶  Stage "runtime" (small: prod deps + compiled output)
                                                                        ↑ only this becomes the final image
```

Benefits: a smaller image (faster pulls and deploys, lower storage cost) and a smaller attack surface (no compilers or dev tools in production). Build tools and source never ship.

### 5. Don't run as root

```dockerfile
USER node          # the official Node images ship a non-root `node` user (uid 1000)
```

If an attacker exploits your app, running as root inside the container makes escalation far easier. Use `COPY --chown=node:node` so files are readable by that user, and write only to directories it owns (or to mounted volumes/tmpfs). Never need root at runtime: bind ports above 1024 (3000, not 80).

### 6. `NODE_ENV=production` only in the runtime stage

```dockerfile
FROM base AS build
# NODE_ENV NOT set here: `npm ci` would skip devDependencies and the build would fail
RUN npm ci && npm run build

FROM base AS runtime
ENV NODE_ENV=production                   # set where it matters
```

If you set `NODE_ENV=production` globally *before* the build-stage `npm ci`, the devDependencies aren't installed and `tsc` or your bundler can't be found. (`01-environment-management.md`)

### 7. Use the exec form of `CMD`, and run Node directly

```dockerfile
CMD ["node", "dist/server.js"]            # ✅ exec form: Node receives SIGTERM directly

CMD ["npm", "start"]                      # ❌ npm swallows SIGTERM → 10 s hang, then SIGKILL
CMD node dist/server.js                   # ❌ shell form: /bin/sh is PID 1 and may not forward signals
```

Add an init (`docker run --init`, Compose `init: true`, or `tini` in the image) so signals and zombie processes are handled properly. Details in `02-graceful-shutdown-and-health-checks.md`.

### 8. `.dockerignore`: keep junk out of the build context

```gitignore
# .dockerignore
node_modules
npm-debug.log
.git
.github
.env
.env.*
!.env.example
coverage
dist
*.md
docker-compose*.yml
Dockerfile
.dockerignore
test
.vscode
```

Why it matters:

- **Security:** `.env` and secrets must never end up inside an image layer.
- **Speed:** sending a huge `node_modules` or `.git` to the Docker daemon on every build is slow.
- **Correctness:** a host's `node_modules` (built for macOS/Windows) copied into a Linux image breaks native modules. Always install *inside* the image.

### 9. One process per container; log to stdout

- The container runs **one** main process (your Node app). Run workers, schedulers, and the API as **separate containers from the same image** with different commands (`11-async-processing/`).
- Log to **stdout/stderr** (`14-logging-observability/01-pino-and-structured-logging.md`), not to files inside the container. The runtime collects them.
- Don't install SSH or a process manager (PM2) inside the container: the orchestrator handles restarts and scaling.

### 10. Handle memory limits explicitly

Containers usually run with a memory limit (`512Mi`, say). If the Node heap tries to grow past it, the kernel's OOM killer terminates the process with no warning, and no stack trace.

```dockerfile
# 512 MB container → leave headroom for non-heap memory (buffers, native code, stack): heap ≈ 70–75% of the limit
ENV NODE_OPTIONS="--max-old-space-size=384"
```

Recent Node versions are container-aware and derive defaults from the cgroup limit, but it's still wise to **set the heap limit explicitly** and monitor memory (`14-logging-observability/03-metrics-and-prometheus.md`). With a heap limit below the container limit, V8 throws a catchable out-of-memory error and you get a crash log instead of a silent kill. See `15-performance/02-profiling-and-memory-leaks.md`.

### 11. Native modules and the libc question

Packages with compiled code (`bcrypt`, `sharp`, `canvas`, `node-rdkafka`, `sqlite3`) ship **prebuilt binaries** per platform. If the build stage and runtime stage use *different* base images (e.g. build on Debian, run on Alpine), the binaries won't match. Keep both stages on the **same OS family**, or build natively in the target image. If you need compilation tools, install them in the build stage only:

```dockerfile
FROM node:22-slim AS build
RUN apt-get update && apt-get install -y --no-install-recommends python3 make g++ && rm -rf /var/lib/apt/lists/*
```

Building for ARM (AWS Graviton, Apple Silicon) versus x86 produces different images. Use `docker buildx build --platform linux/amd64,linux/arm64` for multi-architecture images, or build for the platform you deploy to.

### 12. Never put secrets in the image

```dockerfile
ENV JWT_SECRET=supersecret              # ❌ visible in `docker history` and in every layer
COPY .env .                             # ❌ ditto
ARG NPM_TOKEN                           # ❌ build args are also recorded in image history
RUN echo "//registry.npmjs.org/:_authToken=${NPM_TOKEN}" > .npmrc && npm ci
```

Runtime secrets come from the platform at start (`01-environment-management.md`). If a **build** needs a secret (a private npm registry token), use a BuildKit secret mount, which is never written to a layer:

```dockerfile
RUN --mount=type=secret,id=npmrc,target=/root/.npmrc \
    npm ci --omit=dev
```

```bash
docker build --secret id=npmrc,src=$HOME/.npmrc -t orders-api .
```

### 13. Image hygiene and security

| Practice | Notes |
|---|---|
| **Scan images** for known vulnerabilities | `trivy image orders-api:tag`, `docker scout cves`, ECR scan-on-push, Snyk. Fail CI on critical findings (`05-ci-cd.md`) |
| **Keep images small** | Fewer packages = fewer CVEs and faster deploys |
| **Update base images regularly** | Automate with Dependabot/Renovate |
| **Read-only root filesystem** | `docker run --read-only --tmpfs /tmp`: an attacker can't drop files; your app must write only to `/tmp` or volumes |
| **Drop capabilities / no new privileges** | `--cap-drop=ALL --security-opt=no-new-privileges` |
| **Sign and attest images** (cosign, SBOM) | Supply-chain security for mature setups |
| **Don't install `curl`/shells you don't need** | Less for an attacker to use (distroless takes this to the limit) |
| **Label images** with source commit and build time | `LABEL org.opencontainers.image.revision=$GIT_SHA` helps tracing a running image back to its source |

---

## Tagging and registries

```bash
# Tag with the immutable Git commit SHA (and optionally a semantic version)
docker build -t 123456789012.dkr.ecr.ap-south-1.amazonaws.com/orders-api:3f9c2ab .
docker push 123456789012.dkr.ecr.ap-south-1.amazonaws.com/orders-api:3f9c2ab
```

| Tagging strategy | Verdict |
|---|---|
| **Git SHA** (`:3f9c2ab`) | ✅ Immutable and traceable: exactly which commit is running |
| **Semver** (`:1.8.2`) | ✅ Good for releases; also keep the SHA |
| **`:latest`** | ❌ A moving target: you can't tell what's deployed, and rollback is ambiguous. Never deploy `latest` to production |
| **Branch names** (`:main`) | ⚠️ Mutable; fine for staging convenience, not for production |

Deploy by **tag or digest**; configure the registry for **immutable tags** where supported (ECR has this setting). Add lifecycle rules so old images are cleaned up. Run vulnerability scanning on push.

---

## Docker Compose: running the whole stack

Your app needs a database, Redis, maybe a queue worker. **Compose** describes all of them in one file so a teammate (or CI) can start the entire stack with one command.

### Local development stack

```yaml
# docker-compose.yml
services:
  api:
    build:
      context: .
      target: runtime                      # the final stage of the Dockerfile
    init: true                             # proper signal handling / zombie reaping (PID 1 problem)
    ports: ["3000:3000"]
    env_file: .env                         # local development values only
    environment:
      DATABASE_URL: postgres://app:app@postgres:5432/app    # `postgres` is the service name = hostname on the Compose network
      REDIS_URL: redis://redis:6379
    depends_on:
      postgres: { condition: service_healthy }              # wait until the DB actually accepts connections
      redis:    { condition: service_healthy }
    restart: unless-stopped

  worker:                                  # same image, different command (11-async-processing/)
    build: { context: ., target: runtime }
    init: true
    command: ["node", "dist/worker.js"]
    env_file: .env
    environment:
      DATABASE_URL: postgres://app:app@postgres:5432/app
      REDIS_URL: redis://redis:6379
    depends_on:
      postgres: { condition: service_healthy }
      redis:    { condition: service_healthy }

  postgres:
    image: postgres:16
    environment:
      POSTGRES_USER: app
      POSTGRES_PASSWORD: app
      POSTGRES_DB: app
    volumes:
      - pgdata:/var/lib/postgresql/data      # named volume: data survives container recreation
    ports: ["5432:5432"]                     # expose to the host for local tools (remove in shared environments)
    healthcheck:
      test: ["CMD-SHELL", "pg_isready -U app -d app"]
      interval: 5s
      timeout: 3s
      retries: 10

  redis:
    image: redis:7
    command: ["redis-server", "--appendonly", "yes"]
    volumes: [redisdata:/data]
    ports: ["6379:6379"]
    healthcheck:
      test: ["CMD", "redis-cli", "ping"]
      interval: 5s
      timeout: 3s
      retries: 10

volumes:
  pgdata:
  redisdata:
```

```bash
docker compose up -d --build        # build and start everything in the background
docker compose ps                    # status and health
docker compose logs -f api           # follow one service's logs
docker compose exec postgres psql -U app -d app    # a shell/psql inside a running container
docker compose run --rm api npm run db:migrate     # one-off task using the app image
docker compose down                  # stop and remove containers (volumes survive)
docker compose down -v               # ...and delete the volumes (data!)
```

### Key Compose concepts

| Concept | Notes |
|---|---|
| **Service names are hostnames** | Inside the Compose network, the app connects to `postgres:5432`, **not** `localhost:5432` (`localhost` in a container is the container itself) |
| **`depends_on` + `condition: service_healthy`** | Plain `depends_on` only waits for the container to *start*, not for Postgres to be ready. Pair it with a healthcheck, and still make your app **retry** its initial connections, since readiness can lag |
| **Named volumes** | Persist database data. Bind mounts (`./src:/app/src`) map host files into the container for development |
| **`ports: ["host:container"]`** | Publishes a port to your machine. Containers talk to each other over the internal network without publishing |
| **`env_file` vs `environment`** | `environment` entries override `env_file`. Use `env_file` for your `.env`; override connection hosts for the Compose network |
| **Profiles** | Optional services: `profiles: ["tools"]`, started with `docker compose --profile tools up` |
| **Compose v2** | The command is `docker compose` (a space), part of Docker itself. The old `docker-compose` binary and the `version:` key in files are legacy |

### Dev mode: live reload with an override file

Compose automatically merges `docker-compose.override.yml` into `docker-compose.yml`, which is handy for developer-only settings:

```yaml
# docker-compose.override.yml: development conveniences (not used in production)
services:
  api:
    build:
      target: dev                            # a dev stage with devDependencies
    command: ["node", "--watch", "src/server.js"]
    volumes:
      - ./src:/app/src                       # edit on the host, the container sees changes instantly
      - /app/node_modules                    # keep the container's node_modules (built for Linux), not the host's
    environment:
      NODE_ENV: development
      LOG_LEVEL: debug
```

The `dev` target referenced above is one more stage in the Dockerfile that keeps devDependencies and doesn't set `NODE_ENV=production`:

```dockerfile
FROM base AS dev
COPY package.json package-lock.json ./
RUN npm ci                              # includes devDependencies
COPY . .
USER node
CMD ["node", "--watch", "src/server.js"]
```

Newer Compose versions also offer **`docker compose watch`** (a `develop.watch` section), which syncs or rebuilds on file changes without bind-mounting. Pick whichever you find more reliable.

A simple alternative many teams prefer: **run only the dependencies in Compose and run the app directly on the host** (`npm run dev`), because it's the fastest feedback loop and the debugger works normally:

```bash
docker compose up -d postgres redis
npm run dev
```

### Compose for tests and CI

```yaml
# docker-compose.test.yml: ephemeral dependencies for integration tests (13-testing/03-test-database-and-coverage.md)
services:
  postgres-test:
    image: postgres:16
    environment: { POSTGRES_USER: test, POSTGRES_PASSWORD: test, POSTGRES_DB: app_test }
    ports: ["5433:5432"]
    tmpfs: ["/var/lib/postgresql/data"]      # RAM-backed: fast and automatically clean
```

### Compose in production?

Compose is excellent for **development, CI, and small single-host deployments** (a side project on one VM). For production at any scale, use a platform that provides rolling deploys, health-based replacement, autoscaling, and secrets management: **ECS/Fargate, Kubernetes, Cloud Run, App Runner** (`06-aws.md`). If you do run Compose in production on a single host:

- Set `restart: unless-stopped` (or `always`).
- Don't publish database ports publicly; keep them on the internal network.
- Use `deploy.resources.limits` (or `mem_limit`/`cpus`) so one service can't starve the host.
- Put secrets in Docker secrets or an external manager, not committed `.env` files.
- Put **Nginx or a managed load balancer in front** for TLS (`04-nginx.md`).
- Accept that deploys will have brief downtime, unless you build blue/green yourself.

### Resource limits and logging

```yaml
services:
  api:
    mem_limit: 512m
    cpus: 1.0
    logging:
      driver: json-file
      options: { max-size: "10m", max-file: "3" }     # prevent logs from filling the disk
    read_only: true                                    # immutable root filesystem
    tmpfs: ["/tmp"]
    cap_drop: ["ALL"]
    security_opt: ["no-new-privileges:true"]
```

---

## Docker troubleshooting cheat sheet

| Symptom | Likely cause |
|---|---|
| `Cannot find module 'x'` in the container, but fine locally | `node_modules` copied from the host, or the package is in `devDependencies` and you built with `--omit=dev` |
| Native module error (`invalid ELF header`, `Error loading shared library`) | Host `node_modules` copied in (add to `.dockerignore`), or build/runtime stages use different OS/libc |
| App unreachable from the browser/host | Listening on `localhost` instead of `0.0.0.0`, or port not published (`-p 3000:3000`) |
| App can't connect to `localhost:5432` inside Compose | Use the service name (`postgres:5432`) |
| `docker stop` takes exactly 10 seconds | `SIGTERM` never reached Node: `CMD ["npm","start"]` or shell form (`02-graceful-shutdown-and-health-checks.md`) |
| Container exits immediately | Check `docker compose logs api`; usually config validation failing (`01-environment-management.md`) |
| `OOMKilled` / exit code 137 | Memory limit exceeded: tune `--max-old-space-size`, find the leak |
| `EACCES: permission denied` writing files | Running as non-root without write access: write to `/tmp` or a volume owned by the user |
| Builds are slow every time | Layer order busts the cache (copying source before installing deps), or no `.dockerignore` |
| Database data vanished | `docker compose down -v` deleted the volume, or no volume was mounted |
| Image is huge | Missing multi-stage build, devDependencies included, big base image, no `.dockerignore`, cache files left in layers (`rm -rf /var/lib/apt/lists/*`) |

Handy commands:

```bash
docker image ls                       # images and sizes
docker history orders-api:dev         # layers and what made them big (and any secrets baked in!)
docker run --rm -it orders-api:dev sh # poke around inside (not available in distroless)
docker stats                          # live CPU/memory per container
docker system df                      # disk usage; `docker system prune` to reclaim
docker inspect --format '{{.State.Health.Status}}' <container>
```

---

## Common mistakes

```dockerfile
# ❌ FROM node:latest                       → irreproducible builds
# ❌ COPY . . before npm ci                 → no layer caching
# ❌ npm install instead of npm ci          → non-deterministic installs
# ❌ copying the host's node_modules / no .dockerignore
# ❌ shipping devDependencies, compilers, tests, and .git in the final image
# ❌ USER root (the default) at runtime
# ❌ CMD ["npm", "start"] or shell-form CMD → signals not delivered
# ❌ ENV NODE_ENV=production before the build stage's npm ci → devDependencies missing, build fails
# ❌ secrets in ENV / ARG / COPY .env       → readable from the image
# ❌ no HEALTHCHECK and no resource limits
# ❌ Alpine with native modules that expect glibc, without verifying
# ❌ different base images for build and runtime stages with native addons
# ❌ deploying the :latest tag
# ❌ app listening on 127.0.0.1 inside the container
# ❌ no heap limit aligned with the container's memory limit → OOM-killed without a trace
# ❌ treating Compose as a production orchestrator for a service that needs zero downtime
# ❌ relying on `depends_on` alone for readiness (no healthchecks, no connection retries)
```

## Checklist

- [ ] Multi-stage Dockerfile; final image has production dependencies and compiled output only
- [ ] Pinned base image (LTS Node, `-slim` unless you've verified Alpine/distroless works); regularly rebuilt
- [ ] `package.json` + lockfile copied first; `npm ci --omit=dev`; BuildKit cache mount
- [ ] `.dockerignore` excludes `node_modules`, `.git`, `.env*`, tests, coverage
- [ ] `USER node`; non-privileged port; `--chown` on copied files
- [ ] `NODE_ENV=production` set in the runtime stage only; `NODE_OPTIONS` heap limit < container memory limit
- [ ] Exec-form `CMD ["node", ...]` plus an init (`init: true` / `--init` / tini); `docker stop` returns quickly
- [ ] `HEALTHCHECK` using Node (no curl dependency)
- [ ] No secrets in the image (verified with `docker history`); build-time secrets via `--mount=type=secret`
- [ ] Images tagged with the Git SHA (never deploy `latest`); scanned for vulnerabilities in CI; pushed to a private registry
- [ ] Compose: service-name hostnames, healthchecks with `condition: service_healthy`, named volumes, resource limits, rotated logs
- [ ] Workers and API run as separate containers from the same image
- [ ] Production runs on an orchestrator, not a hand-run `docker compose up`

## Next

**`04-nginx.md`** puts a reverse proxy in front of your containers: TLS termination, compression, static files, rate limiting, WebSocket proxying, and load balancing across instances.
