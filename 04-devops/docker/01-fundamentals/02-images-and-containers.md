# Images & Containers

The single most important distinction in Docker: an **image** is a blueprint; a **container** is a running instance of that blueprint. Nearly every Docker command is really an operation on one or the other.

```
Image        →  read-only template (like a class)
Container    →  running instance of an image (like an object)
```

## Images

An image is a packaged, read-only bundle containing everything an application needs to run: the code, a runtime (e.g. Node.js), libraries, environment variables, and configuration.

```bash
docker images
```

```
REPOSITORY   TAG       IMAGE ID       SIZE
nginx        latest    a8758716bb6a   187MB
node         20-alpine 3f4d2c1a9e0b   127MB
```

Key properties of an image:

- **Immutable** — an image never changes once built. Modifying an app means building a new image, not editing an existing one.
- **Layered** — an image is made up of stacked, cacheable layers (covered in `03_dockerfile/02_layers-caching.md`)
- **Shareable** — images are pushed to and pulled from a registry (like Docker Hub), so the exact same image runs identically anywhere

### Where images come from

```bash
docker pull nginx           # download from a registry
docker build -t myapp .     # build one yourself from a Dockerfile
```

---

## Containers

A container is a running (or stopped) **instance** of an image, with its own writable layer on top of the image's read-only layers.

```bash
docker run nginx
```

Key properties of a container:

- **Writable** — while an image is read-only, a container has a thin writable layer on top, where any runtime changes (new files, modified config) go
- **Ephemeral by default** — that writable layer is deleted when the container is removed, unless the data lives in a volume (see `04_volumes_networking`)
- **Isolated** — its own filesystem view, network interface, and process namespace, even though it shares the host's kernel

### One image, many containers

The same image can be used to start many independent containers, each with its own writable layer and runtime state:

```bash
docker run -d --name web-1 -p 8081:80 nginx
docker run -d --name web-2 -p 8082:80 nginx
docker run -d --name web-3 -p 8083:80 nginx
```

Three separate, independent containers — all started from the exact same `nginx` image.

---

## The class/object analogy

If you're familiar with object-oriented programming, this maps almost exactly:

| Programming                         | Docker                         |
| ----------------------------------- | ------------------------------ |
| Class                               | Image                          |
| Object (instance of a class)        | Container                      |
| Creating an object: `new MyClass()` | `docker run myimage`           |
| Many objects from one class         | Many containers from one image |

---

## The container lifecycle

A container moves through a few states over its life:

```
docker run   →  Created + Started (running)
docker stop  →  Stopped (still exists, filesystem intact)
docker start →  Running again
docker rm    →  Removed entirely (writable layer discarded)
```

```bash
docker ps        # running containers
docker ps -a     # all containers, including stopped ones
```

A **stopped** container isn't gone — it still exists on disk with its writable layer intact, which is why `docker start` can bring it back exactly as it was. Only `docker rm` actually deletes it.

---

## Tags: which version of an image

Images are identified by name and an optional **tag** (defaulting to `latest` if omitted):

```bash
docker pull node:20-alpine
docker pull node:18
docker pull node          # implicitly node:latest
```

`latest` is just a conventional tag name, not automatically "the newest version" — it's whatever the image's maintainer chose to tag as `latest`. For anything you depend on in production, pin an explicit version tag rather than relying on `latest`, since it can change out from under you.

## Quick summary

- An image is a read-only, immutable template; a container is a running instance of one, with its own writable layer
- Many containers can run from the same image simultaneously, independent of each other
- Stopping a container doesn't delete it — only `docker rm` does
- Prefer explicit version tags (`node:20-alpine`) over `latest` for anything that matters

## Next

**`03_docker-architecture.md`** covers how the `docker` command you type actually results in a container running — the daemon, the client, and registries.
