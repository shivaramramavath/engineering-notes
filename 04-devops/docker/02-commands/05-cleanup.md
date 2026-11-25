# Cleanup: `rm`, `rmi` & `system prune`

Containers and images accumulate fast — every `docker run` without `--rm` leaves a stopped container behind, and every rebuild can leave an old, now-unused image. This file covers reclaiming that disk space.

```
docker rm            →  remove a container
docker rmi           →  remove an image
docker system prune  →  remove a broad set of unused stuff at once
```

---

## `docker rm` — remove a container

```bash
docker rm my-web
```

Only works on a **stopped** container:

```
Error response from daemon: cannot remove container "my-web": container is running
```

Stop it first, or force it:

```bash
docker stop my-web && docker rm my-web
docker rm -f my-web        # stop and remove in one step
```

### Remove all stopped containers at once

```bash
docker container prune
```

```
WARNING! This will remove all stopped containers.
Are you sure you want to continue? [y/N]
```

### Auto-remove on exit

Rather than cleaning up after the fact, prevent the buildup in the first place for throwaway containers:

```bash
docker run --rm nginx
```

`--rm` deletes the container automatically the moment it stops — ideal for one-off commands (tests, scripts, debugging) that don't need to persist.

---

## `docker rmi` — remove an image

```bash
docker rmi myapp:v1
```

Fails if a container (even a stopped one) still references the image:

```
Error response from daemon: conflict: unable to delete ... (must be forced) - image is referenced in multiple repositories
```

Remove the dependent container(s) first, or force it:

```bash
docker rmi -f myapp:v1
```

### Remove dangling images

A "dangling" image is a layer left behind with no tag — usually the previous version of an image after a rebuild reused its name:

```bash
docker images -f dangling=true
docker image prune
```

```
REPOSITORY   TAG       IMAGE ID
<none>       <none>    e3a1b2f9c7d5
```

These serve no purpose once superseded and are safe to remove.

---

## `docker system prune` — clean up broadly

```bash
docker system prune
```

Removes, in one command:

- All stopped containers
- All dangling images
- All unused networks
- All build cache

```
WARNING! This will remove:
  - all stopped containers
  - all networks not used by at least one container
  - all dangling images
  - all dangling build cache

Are you sure you want to continue? [y/N]
```

### Including unused (not just dangling) images

```bash
docker system prune -a
```

`-a` also removes images that aren't dangling but simply aren't used by _any_ container right now — more aggressive, and will require re-pulling/rebuilding anything you actually still needed.

### Including volumes

```bash
docker system prune --volumes
```

Volumes aren't removed by a plain `system prune` by default, since they often hold data you want to keep (see `04_volumes_networking`) — only add `--volumes` when you're sure.

### Skip the confirmation prompt

```bash
docker system prune -f
```

---

## Checking disk usage

```bash
docker system df
```

```
TYPE            TOTAL     ACTIVE    SIZE      RECLAIMABLE
Images          12        3         4.2GB     3.1GB (73%)
Containers      8         2         120MB     95MB (79%)
Local Volumes   4         2         850MB     400MB (47%)
Build Cache     34        0         1.1GB     1.1GB (100%)
```

A quick way to see where space is actually going before deciding how aggressively to clean up.

---

## A sensible cleanup routine

```bash
docker system df               # see what's using space
docker container prune -f      # remove stopped containers
docker image prune -f          # remove dangling images
docker system prune -a -f      # more aggressive: also removes unused (not just dangling) images
```

## Quick summary

| Command                         | Removes                                                              |
| ------------------------------- | -------------------------------------------------------------------- |
| `docker rm <container>`         | One stopped container                                                |
| `docker rm -f <container>`      | Force-stop and remove one container                                  |
| `docker container prune`        | All stopped containers                                               |
| `docker rmi <image>`            | One image (fails if in use)                                          |
| `docker image prune`            | Dangling (untagged) images                                           |
| `docker system prune`           | Stopped containers + dangling images + unused networks + build cache |
| `docker system prune -a`        | Same, plus _all_ unused images, not just dangling ones               |
| `docker system prune --volumes` | Same, plus unused volumes — use carefully                            |

- Use `--rm` on `docker run` for throwaway containers to avoid buildup in the first place
- Check `docker system df` before an aggressive prune, so you know what you're about to reclaim

## Section complete

That covers the day-to-day CLI. **`03_dockerfile`** goes deeper into writing a good Dockerfile — the thing `docker build` actually reads.
