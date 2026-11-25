# Multi-Service Compose Setups

The real payoff of Compose: defining an app, a database, and a cache together, with networking and startup order handled automatically instead of manually creating networks and typing multiple `docker run` commands.

## A complete example: app + database + cache

```yaml
services:
  app:
    build: .
    ports:
      - "3000:3000"
    environment:
      - DATABASE_URL=postgresql://user:pass@db:5432/mydb
      - REDIS_URL=redis://cache:6379
    depends_on:
      - db
      - cache

  db:
    image: postgres:16
    environment:
      - POSTGRES_USER=user
      - POSTGRES_PASSWORD=pass
      - POSTGRES_DB=mydb
    volumes:
      - db-data:/var/lib/postgresql/data

  cache:
    image: redis:7-alpine

volumes:
  db-data:
```

```bash
docker compose up
```

That's an entire three-service stack, with persistent database storage and inter-service networking, in about 20 lines.

---

## Services reach each other by name — automatically

Notice `DATABASE_URL` uses the hostname `db`, and `REDIS_URL` uses `cache` — not `localhost`, not an IP address. This is because Compose automatically:

1. Creates a dedicated network for the whole file (recall `04_volumes_networking/02_networking.md` — a custom network, not the default bridge)
2. Attaches every service to it
3. Makes each service reachable by its **service name** as a hostname, via Docker's built-in DNS

You get all the benefits of the manual `docker network create` + `--network` setup from the previous section, for free, just by defining services in the same file.

---

## `depends_on`: startup order, not readiness

```yaml
app:
  depends_on:
    - db
    - cache
```

This guarantees `db` and `cache` are **started** before `app` — but "started" isn't the same as "ready to accept connections." Postgres, for example, takes a moment after its process starts before it's actually ready to handle queries.

### The problem this causes

```yaml
app:
  depends_on:
    - db
```

`app` might start and immediately try to connect to `db` before Postgres has finished its own startup — resulting in a connection error on the very first attempt, even though `depends_on` was satisfied.

### Fixing it with healthchecks

```yaml
services:
  app:
    build: .
    depends_on:
      db:
        condition: service_healthy

  db:
    image: postgres:16
    environment:
      - POSTGRES_PASSWORD=pass
    healthcheck:
      test: ["CMD-SHELL", "pg_isready -U postgres"]
      interval: 5s
      timeout: 5s
      retries: 5
```

Now `app` waits until `db`'s healthcheck actually passes, not just until the container process starts. This is the standard fix for the "app crashes on startup because the database wasn't ready yet" problem.

### The application-level fallback

Even with healthchecks, well-written apps should still retry a failed initial database connection a few times before giving up — healthchecks reduce the problem, but network hiccups and edge cases can still occur, and retry logic is cheap insurance.

---

## Named volumes in Compose

```yaml
services:
  db:
    image: postgres:16
    volumes:
      - db-data:/var/lib/postgresql/data

volumes:
  db-data:
```

Declaring `db-data:` under the top-level `volumes:` key (even with nothing else specified) creates a named volume Compose manages — same concept as `docker volume create` from `04_volumes_networking/01_volumes-bind-mounts.md`, just declared alongside everything else.

---

## Bind mounts for live development

```yaml
services:
  app:
    build: .
    volumes:
      - ./src:/app/src # bind mount — live code changes
      - /app/node_modules # anonymous volume — protects container's own node_modules
    ports:
      - "3000:3000"
```

The second line is a common trick: it prevents the bind-mounted host folder from accidentally overwriting the container's own installed `node_modules` (which might differ from the host's, e.g. due to OS-specific native dependencies).

---

## Scaling a service

```bash
docker compose up --scale app=3
```

Runs three instances of the `app` service. Note: this conflicts with a fixed `ports:` mapping (you can't bind three containers to the same host port), so scaled services are typically used behind a load balancer rather than with direct host port mapping.

---

## Environment-specific overrides

A common pattern: a base `docker-compose.yml` plus an override file for different environments.

```bash
docker compose -f docker-compose.yml -f docker-compose.dev.yml up
```

`docker-compose.dev.yml` might add bind mounts and a debug port that shouldn't exist in production, without duplicating the entire base file.

---

## Quick summary

- Services in the same Compose file can reach each other by **service name** — Compose sets up the network automatically
- `depends_on` controls start order only, not whether a dependency is actually ready — pair it with `condition: service_healthy` and a `healthcheck` for anything that takes time to become ready (databases especially)
- Declare named volumes under a top-level `volumes:` key for persistent data
- An anonymous volume on `node_modules` alongside a source bind mount avoids host/container dependency clashes
- Multiple `-f` files let you layer environment-specific overrides on a shared base

## Section complete

That covers running a realistic multi-container application locally with Compose. **`06_production`** takes this further — environment/secrets handling, shrinking images for deployment, and building images as part of CI.
