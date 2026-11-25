# Volumes & Bind Mounts

By default, any data written inside a container lives only in that container's writable layer — and is deleted the moment the container is removed. Volumes and bind mounts are the two ways to keep data around independently of a container's lifecycle.

```
Named volume  →  Docker-managed storage, best for persistent app data (databases, uploads)
Bind mount    →  a specific folder on your host machine, mapped into the container
```

---

## The problem, illustrated

```bash
docker run -d --name db postgres
# ... data gets written into the database ...
docker rm -f db
docker run -d --name db postgres
# the new container starts with a completely empty database — the old data is gone
```

Without a volume, every fresh container starts from scratch. This is intentional (see `01_docker_fundamentals/02_images-containers.md` on ephemeral writable layers) — it's what makes containers predictable and disposable — but it means anything you actually need to persist has to live somewhere else.

---

## Named volumes

Storage that Docker creates and manages for you, living outside any single container's writable layer.

```bash
docker volume create mydata
docker run -d --name db -v mydata:/var/lib/postgresql/data postgres
```

`-v mydata:/var/lib/postgresql/data` — mounts the volume `mydata` at that path inside the container. Docker creates the volume automatically if it doesn't already exist, so the explicit `docker volume create` step above is often skipped in practice.

### Now the data survives container removal

```bash
docker rm -f db
docker run -d --name db -v mydata:/var/lib/postgresql/data postgres
# same data is still there — the volume, not the container, holds it
```

### Managing volumes

```bash
docker volume ls                  # list all volumes
docker volume inspect mydata       # see where it actually lives on disk, and other metadata
docker volume rm mydata            # delete a volume (only if unused)
docker volume prune                # remove all unused volumes
```

### Where volumes actually live

Docker stores named volumes in a location it manages itself (typically under `/var/lib/docker/volumes/` on Linux, or inside Docker Desktop's VM on Windows/macOS) — you generally don't need to touch this directly, which is exactly the point: Docker handles the storage details so you don't have to.

---

## Bind mounts

Maps a specific, existing folder from your host machine directly into the container.

```bash
docker run -d -v $(pwd)/src:/app/src myapp
```

Unlike a named volume, you're pointing at an exact path you chose — Docker doesn't manage or abstract it.

### The classic use case: live development

```bash
docker run -d -p 3000:3000 -v $(pwd):/app myapp
```

Mounting your entire project folder into the container means edits made on your host machine (in your normal editor) are immediately visible inside the container — combined with a file-watcher/dev-server inside the container, this gives live-reload development without rebuilding the image on every change.

### Read-only bind mounts

```bash
docker run -v $(pwd)/config.json:/app/config.json:ro myapp
```

`:ro` mounts it read-only — the container can read the file but not modify it. Useful for config files you don't want a container able to alter.

---

## Named volume vs bind mount

|                                         | Named volume                                            | Bind mount                                               |
| --------------------------------------- | ------------------------------------------------------- | -------------------------------------------------------- |
| Managed by                              | Docker                                                  | You (an exact host path)                                 |
| Best for                                | Persistent application data (databases, uploaded files) | Sharing source code during development, injecting config |
| Portable across machines                | Yes — no host-specific path required                    | No — tied to a specific host path                        |
| Visible/editable from the host directly | Indirectly (via `docker volume inspect`)                | Yes, directly, like any normal folder                    |

A common rule of thumb: **named volumes for data your app owns and manages** (a database's files); **bind mounts for things you, the developer, are actively editing** (source code) or providing (a specific config file).

---

## The newer `--mount` syntax

`-v`/`--volume` is the classic shorthand; `--mount` is more explicit and is generally recommended for clarity in scripts, though both do the same job:

```bash
docker run -d \
  --mount type=volume,source=mydata,target=/var/lib/postgresql/data \
  postgres

docker run -d \
  --mount type=bind,source=$(pwd)/src,target=/app/src \
  myapp
```

`--mount` requires you to spell out `type=`, `source=`, `target=` explicitly, which makes it harder to get subtly wrong compared to `-v`'s compact but sometimes ambiguous `host:container` shorthand.

---

## Anonymous volumes (a quick mention)

```bash
docker run -v /app/data myapp
```

A volume with no name specified on the host side — Docker creates one with a random ID. These persist as long as the volume isn't removed, but since there's no memorable name, they're easy to lose track of. Prefer named volumes for anything you intend to reuse deliberately.

## Quick summary

- Without a volume or bind mount, a container's data is deleted along with the container — this is the default, not a bug
- Named volumes: Docker-managed, portable, best for data your application owns (databases, uploads)
- Bind mounts: map an exact host folder in, best for live development and injecting config files
- `:ro` makes a mount read-only
- `--mount` is the more explicit modern syntax; `-v` is the common shorthand

## Next

**`02_networking.md`** covers the other half of a container's connection to the outside world: how containers reach each other and how ports get exposed to the host.
