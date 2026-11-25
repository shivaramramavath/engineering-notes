# `docker build`

Builds your own image from a **Dockerfile** — a text file of instructions describing how to assemble it. This file covers the `build` command itself; the Dockerfile's syntax and best practices get their own full section (`03_dockerfile`).

```bash
docker build -t myapp .
```

## What each part means

- `docker build` — build a new image
- `-t myapp` — tag (name) the resulting image `myapp` (defaults to `myapp:latest` if you don't include a version)
- `.` — the **build context**: the folder Docker can access while building (usually the current directory)

## A minimal Dockerfile, to see what's being built

```dockerfile
FROM node:20-alpine
WORKDIR /app
COPY . .
RUN npm install
CMD ["node", "server.js"]
```

```bash
docker build -t myapp .
```

```
[+] Building 12.3s
 => [1/4] FROM node:20-alpine
 => [2/4] WORKDIR /app
 => [3/4] COPY . .
 => [4/4] RUN npm install
 => exporting to image
 => naming to docker.io/library/myapp
```

Each `FROM`/`COPY`/`RUN`/etc. line becomes a **layer** (more in `03_dockerfile/02_layers-caching.md`); the build log shows each one executing in order.

---

## Tagging with a version

```bash
docker build -t myapp:v1.0 .
docker build -t myuser/myapp:v1.0 .    # include your registry username for pushing later
```

You can apply multiple tags to the same build in one command:

```bash
docker build -t myapp:v1.0 -t myapp:latest .
```

---

## The build context matters

```bash
docker build -t myapp .
```

That trailing `.` tells Docker: "everything in this folder is available to `COPY`/`ADD` instructions." Docker sends the **entire contents of that folder** to the daemon before building — including files never referenced in the Dockerfile — which is why a large, cluttered directory (e.g. one with `node_modules` or `.git` inside it) can make builds slow to even start.

```
Sending build context to Docker daemon  482MB
```

Use a `.dockerignore` file to exclude what shouldn't be sent (covered in `03_dockerfile/04_dockerignore.md`) — this is the single biggest lever for keeping builds fast.

### Building from a different Dockerfile or context

```bash
docker build -f Dockerfile.prod -t myapp:prod .
```

`-f` points to a differently-named or located Dockerfile, while `.` still sets the context folder.

```bash
docker build -t myapp ./backend
```

Builds using `./backend` as both the context and the location to look for a `Dockerfile` (unless `-f` overrides that).

---

## Passing build-time variables

```dockerfile
ARG NODE_VERSION=20
FROM node:${NODE_VERSION}-alpine
```

```bash
docker build --build-arg NODE_VERSION=18 -t myapp .
```

`ARG` values are available only during the build, not inside the running container (use `ENV` in the Dockerfile, or `-e` on `docker run`, for that — see `03_dockerfile/01_dockerfile-basics.md`).

---

## Rebuilding without cache

```bash
docker build --no-cache -t myapp .
```

Docker caches each layer and skips re-running unchanged steps on subsequent builds — usually a big speed win, but occasionally you need to force a full rebuild (e.g. to pick up a base image update, or when debugging a caching-related issue).

---

## Checking what got built

```bash
docker images myapp
```

```
REPOSITORY   TAG      IMAGE ID       CREATED         SIZE
myapp        latest   9c7d5e3a1b2f   5 seconds ago   215MB
```

Then run it like any other image:

```bash
docker run -d -p 3000:3000 myapp
```

## Quick summary

- `docker build -t <name> <context>` builds an image from a Dockerfile in that context folder
- The build context (the last argument, usually `.`) is sent to the daemon in full — keep it small with `.dockerignore`
- `-f` picks a specific Dockerfile; `--build-arg` passes build-time variables; `-t` can be repeated for multiple tags
- `--no-cache` forces a full rebuild, ignoring Docker's normal layer caching

## Next

**`05_cleanup.md`** covers removing containers and images you no longer need — including builds that pile up over time.
