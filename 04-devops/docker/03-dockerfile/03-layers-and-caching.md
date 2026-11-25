# Layers & Caching

Each instruction in a Dockerfile creates a **layer**, and Docker caches those layers to avoid redoing work on rebuilds. Understanding this is the difference between a Dockerfile that rebuilds in 2 seconds and one that reinstalls every dependency on every single change.

## What a layer is

Every `FROM`, `COPY`, and `RUN` instruction produces a new, stacked filesystem layer. An image is really just an ordered stack of these layers, plus metadata:

```dockerfile
FROM node:20-alpine     # layer 1 (actually many layers, from the base image itself)
WORKDIR /app             # layer 2
COPY package.json .      # layer 3
RUN npm install           # layer 4
COPY . .                  # layer 5
```

```bash
docker history myapp
```

```
IMAGE          CREATED         CREATED BY                    SIZE
9c7d5e3a1b2f   2 minutes ago   CMD ["node" "server.js"]       0B
...            2 minutes ago   COPY . .                       45MB
...            3 minutes ago   RUN npm install                 89MB
...            3 minutes ago   COPY package.json .              1KB
...            5 minutes ago   WORKDIR /app                     0B
...            5 minutes ago   FROM node:20-alpine              127MB
```

---

## How the cache works

On a rebuild, Docker checks each instruction **in order**, starting from the top. For each one, it asks: "have I seen this exact instruction, with this exact input, at this exact position before?" If yes, it reuses the cached layer instead of re-running it. The moment one instruction's cache misses, **every instruction after it** must also re-run — even if they wouldn't have changed on their own.

```
FROM node:20-alpine     ✅ cached (unchanged)
WORKDIR /app             ✅ cached (unchanged)
COPY package.json .      ✅ cached (package.json unchanged)
RUN npm install            ✅ cached (since its input, package.json, didn't change)
COPY . .                   ❌ cache miss (some source file changed)
CMD [...]                  ❌ must re-run (everything after a miss re-runs too)
```

This "cache miss cascades downward" behavior is the single most important thing to understand about writing a fast Dockerfile.

---

## The classic optimization: copy dependency manifests first

This is why the example in `01_dockerfile-basics.md` copies `package.json` _before_ the rest of the source code, instead of just doing `COPY . .` once:

### ❌ Slow: reinstalls dependencies on every code change

```dockerfile
FROM node:20-alpine
WORKDIR /app
COPY . .              # any file change invalidates this AND everything after it
RUN npm install         # re-runs on every single code change, even a one-line tweak
CMD ["node", "server.js"]
```

### ✅ Fast: only reinstalls when dependencies actually change

```dockerfile
FROM node:20-alpine
WORKDIR /app
COPY package*.json ./   # only invalidated when package.json/package-lock.json changes
RUN npm install           # cached unless dependencies changed
COPY . .                   # your day-to-day source edits land here instead
CMD ["node", "server.js"]
```

In the second version, editing `server.js` a hundred times in a row never re-triggers `npm install` — only editing `package.json` does. This single reordering is often the biggest build-speed win available in an entire Dockerfile.

---

## General ordering principle

Put instructions that **change rarely** near the top, and instructions that **change often** near the bottom:

```
Changes rarely  →  FROM, system packages, dependency manifests + install
       ↓
Changes often   →  application source code, CMD
```

Same idea applies to installing OS packages:

```dockerfile
# ✅ good: apt-get layer only invalidated if this line changes
RUN apt-get update && apt-get install -y curl

# then copy frequently-changing source code afterward
COPY . .
```

---

## Cache is based on content, not just "did the file change"

Docker doesn't literally re-read your Dockerfile character by character — for `COPY`, it checks the actual checksum of the files being copied. So `COPY package.json .` is only invalidated when `package.json`'s _contents_ change, regardless of timestamps or unrelated file activity elsewhere in the project.

For `RUN`, caching is based on the exact command string plus the state of the preceding layer — meaning if an earlier layer's cache misses, every `RUN` after it re-executes even if the command text itself is identical to last time.

---

## Forcing a full rebuild

```bash
docker build --no-cache -t myapp .
```

Useful when you suspect a stale cached layer (e.g. a base image was updated remotely but your local cache doesn't know), or when debugging a Dockerfile issue and want to rule out caching as the cause.

---

## Quick summary

- Every Dockerfile instruction becomes a layer; Docker caches each one and reuses it when nothing relevant changed
- A cache miss on one instruction forces every instruction **after** it to re-run too — order matters
- Copy dependency manifests and install dependencies _before_ copying the rest of your source code, so routine code changes don't trigger a full reinstall
- Put things that change rarely near the top of the Dockerfile, things that change often near the bottom
- `--no-cache` forces a full rebuild when you need to bypass caching entirely

## Next

**`03_multi-stage-builds.md`** covers using multiple `FROM` stages to keep build-only tools out of your final image, on top of the caching gains from this file.
