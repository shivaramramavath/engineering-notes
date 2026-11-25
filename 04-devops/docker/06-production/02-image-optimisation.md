# Image Optimization

A smaller production image pulls faster, deploys faster, and has a smaller attack surface. This file pulls together techniques from earlier sections into a production-focused checklist, plus a few new ones.

```
Smaller image  →  faster pulls/deploys, less disk/bandwidth, fewer things that can have a vulnerability
```

---

## 1. Choose a minimal base image

```dockerfile
FROM node:20              # ~1GB
FROM node:20-slim          # ~200MB — trimmed Debian
FROM node:20-alpine        # ~130MB — minimal Alpine Linux
```

| Base             | Size     | Trade-off                                                                                               |
| ---------------- | -------- | ------------------------------------------------------------------------------------------------------- |
| Full (`node:20`) | Largest  | Includes lots of tools you likely don't need at runtime                                                 |
| `-slim`          | Smaller  | Debian-based, broader compatibility than Alpine                                                         |
| `-alpine`        | Smallest | Uses `musl` libc instead of `glibc` — occasionally causes compatibility issues with native dependencies |

Alpine is usually the right default for production; fall back to `-slim` if a dependency specifically doesn't work well with `musl` (some native Node addons, for example).

### Distroless images (a further step)

```dockerfile
FROM gcr.io/distroless/nodejs20-debian12
```

"Distroless" images contain only your application and its runtime — no shell, no package manager, nothing else. Smaller and more secure (nothing for an attacker to use if they get code execution), but harder to debug since you can't `docker exec ... sh` into one at all.

---

## 2. Use multi-stage builds

Covered in full in `03_dockerfile/03_multi-stage-builds.md` — the single biggest lever after base image choice. Recap:

```dockerfile
FROM node:20 AS builder
WORKDIR /app
COPY package*.json ./
RUN npm install
COPY . .
RUN npm run build

FROM node:20-alpine
WORKDIR /app
COPY package*.json ./
RUN npm install --omit=dev
COPY --from=builder /app/dist ./dist
CMD ["node", "dist/server.js"]
```

Build tools, dev dependencies, and source files stay in the discarded `builder` stage — the shipped image only has the compiled output and production dependencies.

---

## 3. Install only production dependencies

```dockerfile
RUN npm install --omit=dev
```

or, for a clean, reproducible install matching the lockfile exactly:

```dockerfile
RUN npm ci --omit=dev
```

`npm ci` is generally preferred for production/CI builds over `npm install` — it installs exactly what's in `package-lock.json`, fails if the lockfile and `package.json` are out of sync, and is typically faster.

---

## 4. Clean up in the same layer you create the mess

```dockerfile
# ❌ leaves the apt cache baked into this layer permanently
RUN apt-get update
RUN apt-get install -y curl

# ✅ cleanup happens within the same layer, so it actually reduces size
RUN apt-get update && \
    apt-get install -y --no-install-recommends curl && \
    rm -rf /var/lib/apt/lists/*
```

As covered in `03_dockerfile/02_layers-caching.md`, a layer's size is fixed once created — deleting a file in a _later_ instruction doesn't shrink an _earlier_ layer that already contains it. Cleanup only helps if it happens inside the same `RUN` that created the files.

---

## 5. Keep the build context (and therefore the image) free of junk

Recap of `03_dockerfile/04_dockerignore.md`:

```gitignore
node_modules
.git
dist
*.md
.env
.env.*
```

A broad `COPY . .` combined with a missing `.dockerignore` is one of the most common ways an image ends up unexpectedly large — or contains a secret it shouldn't (see `01_environments-secrets.md`).

---

## 6. Combine and order `RUN` instructions deliberately

Fewer, well-ordered layers usually beat many small ones — both for size (less duplicated intermediate state) and for cache efficiency (see `03_dockerfile/02_layers-caching.md`):

```dockerfile
RUN apt-get update && \
    apt-get install -y --no-install-recommends git curl && \
    rm -rf /var/lib/apt/lists/*
```

---

## 7. Avoid copying things you don't need at runtime

```dockerfile
# ❌ copies test files, docs, config that's irrelevant at runtime
COPY . .

# ✅ copy only what's actually needed to run
COPY dist ./dist
COPY package*.json ./
```

If you're not using a multi-stage build, be deliberate about exactly what `COPY` brings in, rather than defaulting to copying everything.

---

## 8. Check what you actually shipped

```bash
docker images myapp
```

```
REPOSITORY   TAG      SIZE
myapp        v1.0     94MB
```

```bash
docker history myapp
```

Shows the size contributed by each layer — useful for spotting an unexpectedly large step (e.g. a package manager cache that wasn't cleaned up).

Third-party tools like **Dive** (`docker run --rm -it wagoodman/dive myapp`) give a more detailed, interactive breakdown of exactly what's taking up space in each layer, if `docker history` alone isn't enough detail.

---

## Production image checklist

- [ ] Base image is `-alpine`, `-slim`, or distroless — not the full default variant
- [ ] Multi-stage build separates build tools/dev dependencies from the final runtime image
- [ ] Only production dependencies installed in the final stage (`npm ci --omit=dev` or equivalent)
- [ ] Package manager caches/temp files cleaned up **within the same `RUN`** that created them
- [ ] `.dockerignore` excludes `node_modules`, `.git`, docs, and any secrets
- [ ] No secrets baked in via `ENV`/`ARG`/`COPY` (see `01_environments-secrets.md`)
- [ ] `docker history`/`docker images` checked to confirm the final size is what you expect

## Quick summary

- Base image choice (Alpine/slim/distroless) and multi-stage builds are the two biggest levers for image size
- Install only production dependencies in the final stage, using `npm ci --omit=dev` or the equivalent for your package manager
- Clean up temporary files in the same layer that creates them — cleanup in a later layer doesn't shrink an earlier one
- `docker history` (or a tool like Dive) shows exactly where an image's size is coming from

## Next

**`03_docker-in-ci.md`** covers building and pushing an optimized image like this automatically, as part of a CI/CD pipeline.
