# Dockerfile Basics

A Dockerfile is a plain text file of instructions, executed top to bottom, that describes how to build an image. This file covers the instructions you'll use in almost every Dockerfile.

```dockerfile
FROM node:20-alpine
WORKDIR /app
COPY package*.json ./
RUN npm install
COPY . .
EXPOSE 3000
ENV NODE_ENV=production
CMD ["node", "server.js"]
```

---

## `FROM` — the base image

Every Dockerfile starts with `FROM`, choosing an existing image to build on top of.

```dockerfile
FROM node:20-alpine
```

- `node:20-alpine` — Node 20, on Alpine Linux (a minimal distro, much smaller than the default Debian-based image)
- Always pin a specific version tag rather than `node:latest`, for the same reason covered in `01_docker_fundamentals/02_images-containers.md`

---

## `WORKDIR` — set the working directory

```dockerfile
WORKDIR /app
```

Sets the directory that subsequent instructions (`COPY`, `RUN`, `CMD`) operate from, and creates it if it doesn't exist. Equivalent to running `mkdir -p /app && cd /app`, but persists for every instruction after it.

---

## `COPY` — copy files into the image

```dockerfile
COPY package.json .
COPY . .
COPY src/ ./src/
```

Copies files from the **build context** (your project folder, or wherever you pointed `docker build` at) into the image. The first argument is the source (relative to the context), the second is the destination inside the image.

### `COPY` vs `ADD`

`ADD` does everything `COPY` does, plus it can auto-extract tar archives and fetch URLs. In practice, prefer `COPY` for anything ordinary — it's more predictable, and `ADD`'s extra behavior is rarely what you actually want. Reach for `ADD` only when you specifically need archive extraction.

---

## `RUN` — execute a command during the build

```dockerfile
RUN npm install
RUN apt-get update && apt-get install -y curl
```

Executes a command **while building the image**, and the result (e.g. installed packages, generated files) becomes part of the image. Each `RUN` typically creates a new layer (more in `02_layers-caching.md`).

### Combine related commands into one `RUN`

```dockerfile
RUN apt-get update && \
    apt-get install -y curl git && \
    rm -rf /var/lib/apt/lists/*
```

Chaining with `&&` keeps it to a single layer and lets you clean up temporary files (like package manager caches) within the same layer, rather than leaving them baked into an earlier one where cleanup can't shrink it.

---

## `CMD` — the default command when the container starts

```dockerfile
CMD ["node", "server.js"]
```

Runs when a container is started from the image — this is what actually keeps the container alive (or, for a one-shot task, runs and exits).

### Exec form vs shell form

```dockerfile
CMD ["node", "server.js"]     # exec form — preferred
CMD node server.js             # shell form
```

The exec form (JSON array) runs the process directly as PID 1, so it properly receives signals like `SIGTERM` from `docker stop`. The shell form wraps it in `/bin/sh -c`, which can swallow those signals — prefer exec form unless you specifically need shell features (variable expansion, piping).

### `CMD` can be overridden

```bash
docker run myapp                 # runs the Dockerfile's CMD
docker run myapp npm test         # overrides CMD entirely, runs this instead
```

Only the **last** `CMD` in a Dockerfile takes effect — it's a default, not a list of commands to run.

---

## `EXPOSE` — document a listening port

```dockerfile
EXPOSE 3000
```

This is **documentation, not enforcement** — it doesn't actually publish the port. You still need `-p` on `docker run` to map it to the host:

```bash
docker run -p 3000:3000 myapp
```

`EXPOSE` mainly helps humans (and tools like `docker network` linking) understand what the container listens on.

---

## `ENV` — set environment variables

```dockerfile
ENV NODE_ENV=production
ENV PORT=3000
```

Sets environment variables available both during the rest of the build **and** inside the running container — unlike `ARG` (covered in `02_git_commands` build file), which is build-time only.

```bash
docker run myapp env
```

```
NODE_ENV=production
PORT=3000
...
```

These can still be overridden at `run` time:

```bash
docker run -e NODE_ENV=development myapp
```

---

## A complete, annotated example

```dockerfile
FROM node:20-alpine        # base image, pinned version

WORKDIR /app                # everything below happens in /app

COPY package*.json ./       # copy just the manifest first (see 02_layers-caching.md for why)
RUN npm install              # install dependencies — cached unless package.json changes

COPY . .                     # now copy the rest of the source code

ENV NODE_ENV=production
EXPOSE 3000

CMD ["node", "server.js"]    # default command when the container starts
```

## Quick summary

| Instruction | Purpose                                                         |
| ----------- | --------------------------------------------------------------- |
| `FROM`      | Choose the base image to build on                               |
| `WORKDIR`   | Set the working directory for following instructions            |
| `COPY`      | Copy files from the build context into the image                |
| `RUN`       | Execute a command during the build (installs, setup)            |
| `CMD`       | Default command when a container starts (overridable)           |
| `EXPOSE`    | Document the port the container listens on (doesn't publish it) |
| `ENV`       | Set environment variables for build and runtime                 |

## Next

**`02_layers-caching.md`** explains why the order of these instructions matters — specifically, why `COPY package*.json` happens before `COPY . .` in the example above.
