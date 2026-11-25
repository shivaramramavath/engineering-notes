# Managing Images: `pull`, `images`, `rmi` & `tag`

Commands for working with images themselves, separate from the containers created from them.

```
docker pull    →  download an image from a registry
docker images  →  list images stored locally
docker rmi     →  remove a local image
docker tag     →  give an image an additional name/tag
```

---

## `docker pull` — download an image

```bash
docker pull nginx
```

Downloads the `nginx:latest` image from Docker Hub (the default registry) without running it.

### Pulling a specific version

```bash
docker pull node:20-alpine
docker pull postgres:16
```

As covered in `01_docker_fundamentals/02_images-containers.md`, prefer an explicit tag over `latest` for anything you depend on — `latest` can change to a different actual version over time.

### Pulling from a different registry

```bash
docker pull ghcr.io/someorg/someimage:tag
```

Docker Hub is the default, but you can pull from any registry by including its hostname in the image reference.

---

## `docker images` — list local images

```bash
docker images
```

```
REPOSITORY   TAG          IMAGE ID       CREATED       SIZE
nginx        latest       a8758716bb6a   2 weeks ago   187MB
node         20-alpine    3f4d2c1a9e0b   3 weeks ago   127MB
myapp        v1.2         9c7d5e3a1b2f   2 hours ago   215MB
```

Every image `docker run` or `docker build` has used ends up here, taking up disk space until removed.

### Just the IDs (useful for scripting)

```bash
docker images -q
```

### Filter by name

```bash
docker images node
```

---

## `docker rmi` — remove an image

```bash
docker rmi nginx:latest
```

("rmi" = remove image)

### Removing by ID

```bash
docker rmi a8758716bb6a
```

### An image in use can't be removed

```
Error response from daemon: conflict: unable to remove repository reference
"nginx:latest" (must force) - container a1b2c3d4e5f6 is using its referenced image
```

Stop and remove any containers using the image first (see `05_cleanup.md`), or force it:

```bash
docker rmi -f nginx:latest
```

Forcing removes the image reference even with dependent containers, but this can leave things in a confusing state — generally prefer removing the containers first.

---

## `docker tag` — give an image another name

```bash
docker tag myapp:latest myuser/myapp:v1.0
```

This doesn't copy or duplicate the image — it just adds a second name pointing at the same underlying image ID. You'll see both listed in `docker images`, but they share the same `IMAGE ID` and disk space.

### Why you need this before pushing

Docker Hub (and most registries) require an image to be tagged with your username/org before it can be pushed:

```bash
docker build -t myapp .                    # local name
docker tag myapp myuser/myapp:v1.0          # add a registry-ready name
docker push myuser/myapp:v1.0               # now it can be pushed
```

Or build with the final name directly:

```bash
docker build -t myuser/myapp:v1.0 .
```

---

## A typical image workflow

```bash
docker pull node:20-alpine          # get a base image
docker images                        # confirm it's there
docker build -t myapp:v1 .           # build your own image (see 04_build.md)
docker tag myapp:v1 myuser/myapp:v1  # prepare it for a registry
docker push myuser/myapp:v1          # share it
docker rmi myapp:v1                  # clean up an old local tag if needed
```

## Quick summary

- `docker pull <image>:<tag>` downloads an image without running it
- `docker images` lists everything stored locally, and how much space it's using
- `docker rmi` removes an image — but not while a container is still using it, unless you `-f` (force)
- `docker tag` adds an additional name to an existing image, without duplicating it — commonly used to prep an image for pushing to a registry

## Next

**`04_build.md`** covers building your own image from a Dockerfile with `docker build`.
