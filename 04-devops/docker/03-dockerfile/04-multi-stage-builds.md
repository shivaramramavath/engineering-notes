# Multi-Stage Builds

A single-stage Dockerfile often ships more than it needs to: build tools, dev dependencies, source files that get compiled away, and caches — all baked into your final production image. Multi-stage builds fix this by using multiple `FROM` instructions, keeping only what the final stage actually needs.

## The problem, illustrated

```dockerfile
FROM node:20
WORKDIR /app
COPY . .
RUN npm install
RUN npm run build          # compiles TypeScript, bundles assets, etc.
CMD ["node", "dist/server.js"]
```

This works, but the final image contains:

- The full `node:20` image (not the smaller `-alpine` variant, if you need build tools that need glibc)
- **All** `node_modules`, including dev-only dependencies (TypeScript, bundlers, test tools) never needed at runtime
- The original, uncompiled source files, now redundant alongside the built `dist/` output

None of that is needed to actually _run_ the app — only to _build_ it.

---

## The fix: separate build and runtime stages

```dockerfile
# ---- Stage 1: build ----
FROM node:20 AS builder
WORKDIR /app
COPY package*.json ./
RUN npm install
COPY . .
RUN npm run build

# ---- Stage 2: runtime ----
FROM node:20-alpine
WORKDIR /app
COPY package*.json ./
RUN npm install --omit=dev
COPY --from=builder /app/dist ./dist
CMD ["node", "dist/server.js"]
```

- **Stage 1 (`builder`)** — has everything needed to build: full Node image, all dependencies, source files, build tools. Named with `AS builder` so it can be referenced later.
- **Stage 2** — starts completely fresh from a minimal `alpine` base, installs only production dependencies, and copies in **just the compiled output** (`COPY --from=builder /app/dist ./dist`) from the first stage.

The first stage is discarded after the build — none of its bulk (dev dependencies, source files, build caches) makes it into the final image. Only the final `FROM` stage becomes the actual image you run and ship.

---

## `COPY --from=<stage>`

This is the key instruction that makes multi-stage builds work — it copies files from an earlier stage instead of from the build context:

```dockerfile
COPY --from=builder /app/dist ./dist
```

You can also copy from a public image directly, without defining your own stage for it:

```dockerfile
COPY --from=nginx:alpine /etc/nginx/nginx.conf /etc/nginx/nginx.conf
```

---

## Referencing stages by number instead of name

Naming stages (`AS builder`) is optional but strongly recommended for readability — without a name, you'd reference stages by index (`--from=0`), which is much harder to follow as a Dockerfile grows:

```dockerfile
FROM node:20 AS deps
FROM node:20 AS builder
FROM node:20-alpine AS runner
```

---

## A common pattern: static site / frontend build

```dockerfile
FROM node:20 AS builder
WORKDIR /app
COPY package*.json ./
RUN npm install
COPY . .
RUN npm run build          # outputs static files to /app/dist

FROM nginx:alpine
COPY --from=builder /app/dist /usr/share/nginx/html
```

The final image is just Nginx serving static files — no Node.js, no `node_modules`, no source code at all in the shipped image. Often shrinks a build from a gigabyte-plus down to tens of megabytes.

---

## Building only up to a specific stage

Useful for running tests or debugging the build stage itself, without needing the final stage to succeed:

```bash
docker build --target builder -t myapp:builder .
```

`--target` stops the build at the named stage instead of running through to the last one.

---

## Why this matters

|                                           | Single-stage                               | Multi-stage                                 |
| ----------------------------------------- | ------------------------------------------ | ------------------------------------------- |
| Final image includes build tools/dev deps | Yes                                        | No                                          |
| Final image includes source code          | Often, redundantly                         | No — only build output                      |
| Typical final image size                  | Larger                                     | Often dramatically smaller                  |
| Smaller image means                       | Slower pulls/pushes, larger attack surface | Faster pulls/pushes, smaller attack surface |

A smaller image isn't just about disk space — fewer packages and tools in the final image also means fewer things that could have a vulnerability, which matters more the closer an image gets to production.

## Quick summary

- Multiple `FROM` instructions define separate build stages in one Dockerfile
- `COPY --from=<stage>` pulls specific files from an earlier stage into the current one
- Only the last stage becomes the final image — everything from earlier stages is discarded unless explicitly copied forward
- The typical pattern: a heavy `builder` stage with all build tools, and a minimal final stage with just the runtime and build output
- `--target <stage>` lets you build (or debug) just one stage instead of the whole pipeline

## Next

**`04_dockerignore.md`** covers `.dockerignore` — keeping the build context itself small, which speeds up every stage of a build, multi-stage or not.
