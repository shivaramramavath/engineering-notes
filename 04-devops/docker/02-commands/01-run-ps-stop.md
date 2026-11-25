# `docker run`, `ps`, `stop`, `start` & `restart`

The core container lifecycle: create and start a container, see what's running, and stop/start it again.

```
docker run      →  create AND start a new container from an image
docker ps       →  list containers
docker stop     →  gracefully stop a running container
docker start    →  start an existing, stopped container
docker restart  →  stop + start in one command
```

---

## `docker run` — create and start a container

```bash
docker run nginx
```

This does several things in one command:

1. Checks if the `nginx` image exists locally — pulls it from Docker Hub if not
2. Creates a new container from that image
3. Starts it

### Common flags

```bash
docker run -d nginx
```

`-d` (detached) — runs in the background, returns your terminal immediately. Without it, your terminal is attached to the container's output and blocked until it stops.

```bash
docker run -p 8080:80 nginx
```

`-p <host>:<container>` — maps a port on your machine to a port inside the container. Requests to `localhost:8080` reach port `80` inside the container.

```bash
docker run --name my-web nginx
```

`--name` — gives the container a memorable name instead of a random one (e.g. `epic_turing`). Referenced in later commands instead of the container ID.

```bash
docker run -e NODE_ENV=production myapp
```

`-e` — sets an environment variable inside the container. Repeat `-e` for multiple variables, or use `--env-file .env` to load several at once.

```bash
docker run -v mydata:/app/data myapp
```

`-v` — mounts a volume (covered fully in `04_volumes_networking`).

```bash
docker run -it ubuntu bash
```

`-it` — interactive terminal, combining `-i` (keep STDIN open) and `-t` (allocate a pseudo-terminal). Use this to get a shell inside a container, e.g. for exploring an image.

### Putting several together

```bash
docker run -d -p 3000:3000 --name my-api -e NODE_ENV=production myapp
```

---

## `docker ps` — list containers

```bash
docker ps
```

Shows **running** containers only:

```
CONTAINER ID   IMAGE   COMMAND    STATUS         PORTS                  NAMES
a1b2c3d4e5f6   nginx   "nginx"    Up 2 minutes   0.0.0.0:8080->80/tcp   my-web
```

### Show all containers, including stopped ones

```bash
docker ps -a
```

Stopped containers still exist on disk (their filesystem, logs, etc.) until you explicitly remove them — see `05_cleanup.md`.

### Just the IDs (useful for scripting)

```bash
docker ps -q
```

---

## `docker stop` — gracefully stop a container

```bash
docker stop my-web
```

Sends a `SIGTERM` signal, giving the process inside a chance to shut down cleanly, then force-kills it after a grace period (10 seconds by default) if it hasn't exited.

```bash
docker stop -t 30 my-web
```

`-t` changes the grace period, in seconds — useful for apps that need longer to finish in-flight work before shutting down.

You can reference a container by name or by ID (a short prefix of the ID is usually enough):

```bash
docker stop a1b2c3
```

---

## `docker start` — start a stopped container

```bash
docker start my-web
```

Restarts a container that was previously stopped, **using the same configuration it was created with** (ports, volumes, env vars) — you don't need to repeat all the `run` flags.

```bash
docker start -a my-web
```

`-a` (attach) — reattaches your terminal to the container's output, similar to running without `-d` originally.

---

## `docker restart` — stop and start in one step

```bash
docker restart my-web
```

Useful after changing a mounted config file or when a container seems stuck — equivalent to `docker stop` followed by `docker start`.

---

## `run` vs `start`: the key distinction

|                            | Creates a new container?    | Use when                                                                          |
| -------------------------- | --------------------------- | --------------------------------------------------------------------------------- |
| `docker run <image>`       | Yes, every time             | You want a fresh container, or one doesn't exist yet                              |
| `docker start <container>` | No — reuses an existing one | The container already exists (even if stopped) and you just want it running again |

Running `docker run` repeatedly creates a **new container each time** (unless you use `--rm` to auto-remove it on exit) — it's easy to accidentally pile up dozens of stopped containers this way. See `05_cleanup.md`.

## Quick summary

- `docker run` creates and starts a new container — `-d`, `-p`, `--name`, `-e`, `-v`, `-it` are the flags you'll use constantly
- `docker ps` shows running containers; `docker ps -a` shows everything, including stopped ones
- `docker stop`/`start`/`restart` control an _existing_ container's running state — they don't create a new one
- Reference containers by name or ID (a short prefix is fine)

## Next

**`02_logs-exec-inspect.md`** covers looking inside a container while it's running — logs, running a command inside it, and inspecting its configuration.
