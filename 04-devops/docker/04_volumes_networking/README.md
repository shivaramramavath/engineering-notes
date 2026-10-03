# 04 — Volumes & Networking

Two things a Dockerfile alone can't solve: making data survive a container being removed, and letting containers talk to each other and the outside world. This section covers both.

## In this section

| File                        | Covers                                                                                    |
| --------------------------- | ----------------------------------------------------------------------------------------- |
| `01_volumes-bind-mounts.md` | Persisting and sharing data — Docker-managed volumes vs bind mounts, and when to use each |
| `02_networking.md`          | Docker's default networks, container-to-container communication, and port mapping         |

## Why these are grouped together

Both are about a container's boundary with the outside world — what data it can see beyond its own writable layer, and what it can talk to over the network. Neither is really about the container's _contents_ (that's the Dockerfile's job) — they're about a running container's relationship to everything around it.

## Why this matters

Recall from `01_docker_fundamentals/02_images-containers.md`: a container's writable layer is deleted when the container is removed. Without a volume, a database container losing its data on `docker rm` isn't a bug — it's the default, expected behavior. Similarly, containers are isolated from each other by default; getting an app container to reach a database container requires understanding Docker's networking model, not just running both and hoping.

## What you should be able to do after this section

- Explain why a container's data disappears on removal unless it's stored somewhere else
- Use a named volume to persist data (e.g. a database) across container restarts and removals
- Use a bind mount to share a local folder into a container (e.g. for live-reloading source code in development)
- Get two containers talking to each other by name, over a shared Docker network
- Map a container's internal port to a port on the host machine

## Next

**`05_docker_compose`** covers defining volumes, networks, and multiple containers together in a single declarative file, instead of typing long `docker run` commands by hand.
