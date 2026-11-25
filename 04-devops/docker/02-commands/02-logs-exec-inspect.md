# `docker logs`, `exec` & `inspect`

Once a container is running, these three commands are how you look inside it — its output, a live shell, and its full configuration.

```
docker logs     →  see a container's stdout/stderr output
docker exec     →  run a command inside a running container
docker inspect  →  see a container's (or image's) full configuration as JSON
```

---

## `docker logs` — see what a container has printed

```bash
docker logs my-web
```

Shows everything the container's main process has written to stdout/stderr since it started — this is where `console.log`, print statements, and typical application logs end up.

### Follow logs live (like `tail -f`)

```bash
docker logs -f my-web
```

Keeps streaming new output as it happens. `Ctrl+C` to stop following (doesn't stop the container).

### Only the recent tail

```bash
docker logs --tail 100 my-web
```

### With timestamps

```bash
docker logs -t my-web
```

### Since a specific time

```bash
docker logs --since 10m my-web       # last 10 minutes
docker logs --since 2026-09-20 my-web
```

### Combine flags

```bash
docker logs -f --tail 50 -t my-web
```

---

## `docker exec` — run a command inside a running container

```bash
docker exec my-web ls /app
```

Runs `ls /app` inside the already-running `my-web` container and prints the result — the container keeps running afterward.

### Get an interactive shell

```bash
docker exec -it my-web bash
```

`-it` (interactive + pseudo-terminal) is what makes this feel like a normal terminal session instead of a single one-off command. Use `sh` instead of `bash` for minimal images (like Alpine-based ones) that don't include Bash:

```bash
docker exec -it my-web sh
```

### Run as a specific user

```bash
docker exec -u root -it my-web bash
```

Useful when the container's default user lacks permission for what you're trying to do (e.g. installing a debugging tool).

### `exec` vs `run`

|                         | Effect                                                 |
| ----------------------- | ------------------------------------------------------ |
| `docker run image`      | Creates a **new** container from an image              |
| `docker exec container` | Runs a command inside an **already running** container |

A very common mix-up for beginners: if `my-web` is already running and you want a shell inside it, you want `exec`, not another `run` (which would create a second, separate container).

---

## `docker inspect` — full configuration as JSON

```bash
docker inspect my-web
```

Returns a large JSON document with everything Docker knows about the container: its IP address, mounted volumes, environment variables, port mappings, restart policy, and more.

### Pulling out one specific field

Rather than reading the whole JSON blob, use `--format` (Go template syntax) to extract just what you need:

```bash
docker inspect --format '{{.NetworkSettings.IPAddress}}' my-web
docker inspect --format '{{.State.Status}}' my-web
docker inspect --format '{{.Config.Env}}' my-web
```

### Inspecting an image instead of a container

```bash
docker inspect nginx
```

Works the same way for images — shows layers, exposed ports, default command, environment, etc.

---

## A typical debugging sequence

```bash
docker ps                          # confirm it's running, note the name
docker logs --tail 50 my-web        # check recent output for errors
docker exec -it my-web sh           # poke around inside if logs aren't enough
docker inspect my-web                # check exact config (env vars, mounts, ports)
```

## Quick summary

- `docker logs` shows a container's console output; `-f` follows it live, `--tail`/`--since` filter it
- `docker exec -it <container> sh` (or `bash`) gets you an interactive shell inside a running container
- `exec` runs inside an _existing_ container; `run` always creates a _new_ one — don't confuse them
- `docker inspect` returns full JSON configuration; use `--format` to pull out one field instead of reading the whole thing

## Next

**`03_images.md`** covers managing the images themselves — pulling, listing, tagging, and removing them.
