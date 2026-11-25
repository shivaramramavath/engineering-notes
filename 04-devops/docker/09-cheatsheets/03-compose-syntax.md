<!--
================================================================================
Metadata:
  Title: Quick Reference Cheat Sheet - Docker Compose v2 Syntax and CLI
  Track: Docker / DevOps
  Difficulty: All Levels
  Estimated Time: 25 minutes
  Key Terminology: Compose Spec, depends_on, healthcheck, develop.watch, profiles, interpolation
================================================================================
-->

# Docker Compose v2 Quick Reference Cheat Sheet

[<- 02-dockerfile-instructions.md](./02-dockerfile-instructions.md) | [Back to Cheatsheets Overview](./README.md) | [Next Cheatsheet: 04-networking-and-volumes.md ->](./04-networking-and-volumes.md)

---

## 1. Full Compose Specification Blueprint

```yaml
# compose.yaml (Top-level version string is obsolete and omitted)
name: my-project # Optional project namespace

services:
  web-api:
    image: company/web-api:1.0.0
    build:
      context: .
      dockerfile: Dockerfile
      target: runtime
      args:
        VERSION: 1.0.0
    restart: unless-stopped # no | always | on-failure | unless-stopped
    ports:
      - "8080:8080" # Host:Container
      - "127.0.0.1:9090:9090" # Bind to localhost loopback only
    expose:
      - "8080" # Internal to network only
    environment:
      - PORT=8080
      - DB_PASS=${DB_PASSWORD:-default_dev_pass} # Interpolation with fallback
      - REQUIRED_KEY=${API_KEY:?API_KEY must be set!} # Fails if unset
    env_file:
      - .env.local
    depends_on:
      db:
        condition: service_healthy # service_started | service_healthy | service_completed_successfully
    healthcheck:
      test: ["CMD", "curl", "-f", "http://localhost:8080/health"]
      interval: 10s
      timeout: 3s
      retries: 3
      start_period: 15s
    volumes:
      - app_data:/app/data # Named volume
      - ./config:/app/config:ro # Read-only bind mount
    networks:
      - frontend-net
      - backend-net
    secrets:
      - server_cert
    deploy:
      resources:
        limits:
          cpus: "1.5"
          memory: 512M
          pids: 150
        reservations:
          cpus: "0.2"
          memory: 128M
      restart_policy:
        condition: on-failure
        delay: 5s
        max_attempts: 3
    develop:
      watch:
        - action: sync
          path: ./src
          target: /app/src
        - action: rebuild
          path: ./package.json
    stop_grace_period: 30s
    profiles:
      - core

  db:
    image: postgres:16-alpine
    environment:
      POSTGRES_PASSWORD: ${DB_PASSWORD}
    volumes:
      - pg_data:/var/lib/postgresql/data
    networks:
      - backend-net
    healthcheck:
      test: ["CMD-SHELL", "pg_isready -U postgres"]
      interval: 5s
      timeout: 3s
      retries: 5

networks:
  frontend-net:
    driver: bridge
  backend-net:
    driver: bridge
    internal: true # Disables external internet routing

volumes:
  app_data:
  pg_data:

secrets:
  server_cert:
    file: ./certs/server.crt
```

---

## 2. Essential Docker Compose CLI Commands

```bash
# Core Lifecycle
docker compose up -d                        # Start all services in background
docker compose up -d --build                # Force image rebuild before starting
docker compose up -d --scale web-api=3      # Scale service to 3 instances
docker compose down                         # Stop and remove containers and networks
docker compose down -v                      # DANGER: Stop and remove containers + VOLUMES!
docker compose stop                         # Stop containers without removing them
docker compose start                        # Start existing stopped containers
docker compose restart <service>            # Restart a specific service

# Monitoring & Logs
docker compose ps                           # List status of all project containers
docker compose logs -f --tail=50 <service>  # Stream live logs for a service
docker compose top                          # Display running processes across services

# Execution & One-off Tasks
docker compose exec -it <service> sh        # Interactive shell in running service
docker compose run --rm <service> <command> # Run one-off command in isolated container

# Modern Developer Workflow
docker compose watch                        # Start file watcher for Hot Reloading
docker compose config                       # Validate and print merged YAML config
docker compose config --services            # List all service names defined in file

# Profiles
docker compose --profile debug up -d        # Start services tagged with 'debug' profile
```

---

## 3. Variable Interpolation Reference

| Syntax | Description | Example Expression | Result if `PORT` unset |
|:-------|:------------|:-------------------|:-----------------------|
| `${VAR}` | Simple value substitution | `${PORT}` | `""` (Empty string) |
| `${VAR:-default}` | Use default if unset or empty | `${PORT:-8080}` | `"8080"` |
| `${VAR-default}` | Use default only if unset | `${PORT-8080}` | `"8080"` |
| `${VAR:?error}` | Throw fatal error if unset or empty | `${DB_PASS:?Password required!}` | Exits with error |
| `${VAR:+value}` | Use alternative value if set | `${DEBUG:+--verbose}` | `""` (if unset) |
| `$$VAR` | Escape literal dollar sign | `$$VAR` | `"$VAR"` |

---

[<- 02-dockerfile-instructions.md](./02-dockerfile-instructions.md) | [Back to Cheatsheets Overview](./README.md) | [Next Cheatsheet: 04-networking-and-volumes.md ->](./04-networking-and-volumes.md)
