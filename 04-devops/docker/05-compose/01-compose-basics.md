# Docker Compose Basics

Docker Compose lets you describe containers, networks, and volumes in a single YAML file, then bring the whole thing up or down with one command.

```
docker-compose.yml   →  declarative description of your setup
docker compose up     →  create and start everything defined in it
docker compose down    →  stop and remove everything it created
```

## Replacing a `docker run` command

Recall this from `02_docker_commands/01_run-ps-stop.md`:

```bash
docker run -d -p 3000:3000 --name my-api -e NODE_ENV=production myapp
```

The same thing, as `docker-compose.yml`:

```yaml
services:
  api:
    build: .
    ports:
      - "3000:3000"
    environment:
      - NODE_ENV=production
```

```bash
docker compose up
```

---

## Anatomy of a `docker-compose.yml`

```yaml
services:
  api: # service name — also used for networking/DNS
    build: . # build from a Dockerfile in the current folder
    # image: myapp:latest         # or use a pre-built image instead of building
    ports:
      - "3000:3000" # same syntax as docker run -p
    environment:
      - NODE_ENV=production # same as docker run -e
    volumes:
      - ./src:/app/src # same as docker run -v
    restart: unless-stopped # restart policy
```

| Key           | Equivalent `docker run` flag                                                              |
| ------------- | ----------------------------------------------------------------------------------------- |
| `image`       | The image name you'd pass to `run`                                                        |
| `build`       | `docker build`, run automatically before starting                                         |
| `ports`       | `-p`                                                                                      |
| `environment` | `-e`                                                                                      |
| `volumes`     | `-v` / `--mount`                                                                          |
| `restart`     | `--restart`                                                                               |
| `depends_on`  | (no direct equivalent — controls startup order, covered in `02_compose-multi-service.md`) |

---

## `env_file` instead of listing every variable

```yaml
services:
  api:
    build: .
    env_file:
      - .env
```

Loads all variables from a `.env` file, rather than listing them individually under `environment:`. Ties directly into the `dotenv`/`envalid` setup from earlier in this documentation set — the app inside the container still reads `process.env` normally; Compose is just how the variables get there.

---

## Core commands

### Start everything

```bash
docker compose up
```

Runs in the foreground, streaming logs from every service. Stop with `Ctrl+C`.

```bash
docker compose up -d
```

`-d` — detached, runs in the background (same idea as `docker run -d`).

### Rebuild before starting

```bash
docker compose up --build
```

Forces a rebuild of any service with a `build:` key, rather than reusing a previously built image.

### Stop and remove everything

```bash
docker compose down
```

Stops and removes the containers and networks Compose created. **Volumes are kept by default** — add `-v` to remove those too:

```bash
docker compose down -v
```

### See what's running

```bash
docker compose ps
```

```
NAME            IMAGE     STATUS         PORTS
myapp-api-1     myapp     Up 2 minutes   0.0.0.0:3000->3000/tcp
```

### View logs

```bash
docker compose logs
docker compose logs -f api     # follow just one service's logs
```

### Run a one-off command in a service

```bash
docker compose exec api sh
```

Same idea as `docker exec`, but referring to the service by its Compose name instead of a container name/ID.

---

## Where the project name comes from

Compose prefixes container and network names with a **project name**, by default the folder name containing `docker-compose.yml`:

```
myapp-api-1
myapp_default   (the auto-created network)
```

Override it explicitly if needed:

```bash
docker compose -p my-project up
```

or via a `name:` key at the top of the compose file.

---

## Compose file version note

Modern Compose files generally don't need a `version:` key at the top — the older `version: "3.8"` style is now largely unnecessary with current Docker Compose (the "Compose Specification"). If you see it in older tutorials, it's safe to omit in new files.

## Quick summary

- `docker-compose.yml` maps closely onto `docker run` flags: `ports` ↔ `-p`, `environment` ↔ `-e`, `volumes` ↔ `-v`
- `docker compose up` (add `-d` to detach) creates and starts everything; `docker compose down` tears it down (add `-v` to also remove volumes)
- `docker compose ps`/`logs`/`exec` mirror their `docker` equivalents, scoped to services defined in the file
- `env_file` is the common way to feed a container the same `.env` file your app already uses locally

## Next

**`02_compose-multi-service.md`** covers the real payoff — running several services together, with automatic networking between them.
