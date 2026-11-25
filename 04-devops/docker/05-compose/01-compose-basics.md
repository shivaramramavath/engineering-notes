# Docker Compose v2: Declarative Multi-Container Orchestration

```
Track: DevOps & Containerization
Module: 05-compose
Prerequisites: 02-commands/01-run-ps-stop.md, 04-volumes-and-networking/01-volumes-and-bind-mounts.md, 04-volumes-and-networking/04-networking.md
Difficulty: Intermediate
Estimated Duration: 45 minutes
Target Docker Version: 27.x+ (Compose Specification v2.x+)
Last Validated: February 2025
```

---

## Table of Contents
1. [What & Why](#1-what--why)
2. [Core Concepts](#2-core-concepts)
3. [How It Works Internally](#3-how-it-works-internally)
4. [Syntax & Manifest Anatomy](#4-syntax--manifest-anatomy)
5. [Practical Implementation & Examples](#5-practical-implementation--examples)
6. [Production Best Practices & Antipatterns](#6-production-best-practices--antipatterns)
7. [Common Pitfalls & Troubleshooting](#7-common-pitfalls--troubleshooting)
8. [Performance Tuning & Resource Optimization](#8-performance-tuning--resource-optimization)
9. [Security Hardening & Threat Model](#9-security-hardening--threat-model)
10. [Architectural Comparison Table](#10-architectural-comparison-table)
11. [Real-World Scenario Walkthrough](#11-real-world-scenario-walkthrough)
12. [Self-Assessment & Interview Questions](#12-self-assessment--interview-questions)
13. [Hands-on Practice Exercises](#13-hands-on-practice-exercises)
14. [Summary & Reference Cheat Sheet](#summary--reference-cheat-sheet)

---

## 1. What & Why

Running multi-tier container architectures using standalone `docker run` commands quickly becomes untenable:
- Shell scripts containing 10-line `docker run` commands with complex port mappings, volume mounts, and network connections are error-prone, fragile, and non-declarative.
- Ordering container boot sequences manually (e.g. ensuring Postgres is ready before the API boots) requires brittle `sleep` commands.
- Tearing down an environment requires hunting down and deleting containers, networks, and volumes individually.

**Docker Compose** is the official declarative specification and orchestration tool for defining and running multi-container applications. With a single YAML file (`compose.yaml` or `docker-compose.yml`), developers define the desired state of their services, volumes, networks, and secrets, and provision the entire stack with a single command:
```bash
docker compose up -d
```

---

## 2. Core Concepts

### Compose Specification & The V2 Engine
- **Compose v1 (Legacy Python CLI)**: Invoked as `docker-compose` (with a hyphen). Deprecated in July 2023. Slow execution, high memory overhead, and versioned schemas (`version: '3.8'`).
- **Compose v2 (Modern Go CLI Plugin)**: Invoked as `docker compose` (with a space). Written in Go, integrated directly into the Docker CLI, conforms to the open **Compose Specification**, and does **not require a `version:` top-level attribute**.

### The 4 Top-Level Manifest Keys
1. **`services`**: Defines the compute containers to run (e.g., `web`, `api`, `db`, `worker`). Each service specifies its image, build context, environment variables, ports, and restart policy.
2. **`networks`**: Defines custom networks (bridges, overlays) that isolate or connect services. If omitted, Compose automatically creates a default user-defined bridge network.
3. **`volumes`**: Declares persistent named volumes shared across services or retained across lifecycles.
4. **`secrets` / `configs`**: Injects sensitive credentials or configuration files without baking them into images or environment variables.

### Project Name & Namespacing
Compose prefixes every container, network, and volume with the **project name** (derived from the directory name or `--project-name` / `-p` flag):
- Directory `my-app` produces container: `my-app-api-1`
- Network: `my-app_default`
- Volume: `my-app_pgdata`
This guarantees complete namespace isolation between different repositories or environments running on the same host.

---

## 3. How It Works Internally

### Compose Architecture & Execution Pipeline

```
+-------------------------------------------------------------+
|               compose.yaml / docker-compose.yml             |
+------------------------------+------------------------------+
                               |
                               v
+-------------------------------------------------------------+
|                    Docker Compose Engine                    |
|  1. Parses YAML against Compose Specification               |
|  2. Interpolates environment variables (.env)               |
|  3. Constructs Directed Acyclic Graph (DAG) of dependencies |
+------------------------------+------------------------------+
                               |
                               v
+-------------------------------------------------------------+
|                      Docker Engine API                      |
|  Calls Docker Daemon via Unix socket (/var/run/docker.sock): |
|  - POST /networks/create       (Creates project_default)    |
|  - POST /volumes/create        (Creates project_volumes)    |
|  - POST /build                 (Builds images via BuildKit) |
|  - POST /containers/create     (Provisions containers)      |
|  - POST /containers/<id>/start (Starts containers in DAG)   |
+-------------------------------------------------------------+
```

---

## 4. Syntax & Manifest Anatomy

### Modern Compose Specification Structure

```yaml
# Note: 'version:' attribute is obsolete and omitted in Compose v2+

services:
  api:
    build:
      context: .
      dockerfile: Dockerfile
      target: runtime
    image: mycompany/api:1.0.0
    container_name: custom-api-name # Optional: overrides project-service-N naming
    restart: unless-stopped
    ports:
      - "127.0.0.1:8080:8080" # Host:Container port mapping
    environment:
      - NODE_ENV=production
      - DB_HOST=db
      - DB_PORT=5432
    env_file:
      - .env.production
    volumes:
      - app_uploads:/app/uploads
      - ./config.json:/app/config.json:ro
    networks:
      - backend-net
    depends_on:
      db:
        condition: service_healthy

  db:
    image: postgres:17-alpine
    restart: always
    environment:
      POSTGRES_DB: orders
      POSTGRES_USER: postgres
      POSTGRES_PASSWORD: ${DB_PASSWORD:-default_secret}
    volumes:
      - db_data:/var/lib/postgresql/data
    networks:
      - backend-net
    healthcheck:
      test: ["CMD-SHELL", "pg_isready -U postgres -d orders"]
      interval: 5s
      timeout: 3s
      retries: 5
      start_period: 10s

networks:
  backend-net:
    driver: bridge

volumes:
  db_data:
  app_uploads:
```

---

## 5. Practical Implementation & Examples

### Example 1: Essential Compose Lifecycle Commands
The day-to-day command set required for managing Compose stacks.

```bash
# 1. Start all services in the background (detached mode)
docker compose up -d

# 2. Force rebuild of all custom images before starting
docker compose up -d --build

# 3. View running services and health status
docker compose ps

# 4. Stream aggregated logs across all services
docker compose logs -f

# 5. Stream logs for a specific service only
docker compose logs -f api

# 6. Execute an interactive command inside a running service
docker compose exec api sh

# 7. Run a one-off task container (e.g. database migrations)
docker compose run --rm api npm run db:migrate

# 8. Stop services without deleting containers or networks
docker compose stop

# 9. Stop and remove containers, networks, and orphan containers
docker compose down --remove-orphans

# 10. Complete teardown including NAMED VOLUMES (CAUTION: Deletes data!)
docker compose down -v
```

### Example 2: Minimal Static Web Server with Nginx
A simple single-service Compose file mapping host files.

```yaml
services:
  web:
    image: nginx:1.27-alpine
    ports:
      - "8080:80"
    volumes:
      - ./public:/usr/share/nginx/html:ro
    restart: always
```

### Example 3: Rebuilding on Code Changes with `--build` and `--no-deps`
Recompiling a single service without disrupting or restarting the database or Redis cache.

```bash
# Rebuild only the 'api' service and recreate its container
docker compose up -d --no-deps --build api
```

### Example 4: Scaling Stateless Workers Dynamically
Scaling a queue worker service to 4 replicas on the fly.

```bash
docker compose up -d --scale worker=4
```
> **Note**: Services with hardcoded host ports (`ports: - "8080:80"`) cannot be scaled beyond 1 replica due to port collisions on the host!

### Example 5: Validating and Inspecting Merged Configuration
Validating Compose syntax and viewing the fully interpolated configuration.

```bash
# Verify YAML syntax and print resolved configuration
docker compose config

# Output only the list of service names
docker compose config --services
```

---

## 6. Production Best Practices & Antipatterns

### Best Practices
- **Omit the `version:` Top-Level Tag**: Modern Compose v2 ignores `version: "3.8"` and warns against it. Follow the Compose Specification.
- **Always Explicitly Name Volumes**: Declare named volumes under the top-level `volumes:` key so data survives `docker compose down`.
- **Use `--remove-orphans` on Deployment**: Prevent rogue containers from abandoned services continuing to run when services are renamed or removed from YAML.
- **Bind Database Ports to `127.0.0.1`**: In `ports:`, never expose databases to `0.0.0.0` unless intentionally public.
- **Use `.env` for Environment Separation**: Separate secrets and environmental configuration from the structural Compose manifest.

### Anti-Patterns
- **Using `docker compose down -v` in Production**: The `-v` flag unconditionally deletes all declared persistent volumes, obliterating database state.
- **Invoking Legacy `docker-compose` (Hyphenated)**: Compose v1 is unmaintained and lacks support for modern BuildKit secrets, Compose Watch, and healthcheck conditions.
- **Hardcoding Passwords in Manifests**: Never commit plaintext API keys or database passwords to `compose.yaml`; use variable interpolation (`${SECRET_KEY}`).

---

## 7. Common Pitfalls & Troubleshooting

| Symptom | Root Cause | Diagnosis Command | Permanent Fix |
|:---|:---|:---|:---|
| `docker compose up` fails with `port is already allocated` | Another host process or previous container is using the specified host port | `docker compose ps` and `lsof -i :<port>` | Change host port mapping or terminate conflicting process. |
| Application crashes on boot with `ECONNREFUSED db:5432` | Application booted before database finished initialization | Inspect logs: `docker compose logs db` | Add `depends_on: { db: { condition: service_healthy } }`. |
| Code changes on host not reflected in container | Container running cached image without bind mount, or image wasn't rebuilt | `docker compose logs api` | Run `docker compose up -d --build` or configure a bind mount. |
| `docker compose down` leaves old containers running | Services were renamed in YAML, leaving "orphan" containers running | `docker ps` | Run `docker compose down --remove-orphans`. |
| Compose creates volumes with unexpected project prefixes | Docker Compose uses directory name as default project namespace | `docker volume ls` | Pass explicit project name via `-p <name>` or set `COMPOSE_PROJECT_NAME`. |
| Scaling a service fails with `port already allocated` | Scaled service defines hardcoded host port (e.g. `80:80`) | Inspect `ports:` definition | Remove host port mapping or use reverse proxy load balancer. |

---

## 8. Performance Tuning & Resource Optimization

1. **Parallel Building and Pulling**:
   Compose v2 automatically pulls images and builds layers concurrently across all services using BuildKit, cutting startup times by up to 60% compared to sequential scripts.
2. **Resource Limits in Compose**:
   Prevent runaway memory leaks from crashing the host by setting resource reservations and limits directly in the Compose file:
   ```yaml
   services:
     api:
       image: my-api
       deploy:
         resources:
           limits:
             cpus: '2.0'
             memory: 1024M
           reservations:
             memory: 256M
   ```

---

## 9. Security Hardening & Threat Model

```
+--------------------------------------------------------------+
| ATTACK: Accidental Exposure of Production Database Ports     |
+--------------------------------------------------------------+
| In compose.yaml:                                             |
|   db:                                                        |
|     ports:                                                   |
|       - "5432:5432"  <-- BINDS TO 0.0.0.0 ON ALL INTERFACES! |
|                                                              |
| Attacker scans host public IP -> Connects to PostgreSQL!     |
+--------------------------------------------------------------+
| HARDENED CONFIGURATION:                                      |
| Option A (Best): Remove `ports:` completely! Backend API     |
|                  talks to db via internal Docker network.    |
| Option B: Bind to localhost: `127.0.0.1:5432:5432`           |
+--------------------------------------------------------------+
```

---

## 10. Architectural Comparison Table

| Feature / Trait | Standalone `docker run` | Docker Compose v2 | Kubernetes (k8s) |
|:---|:---|:---|:---|
| **Configuration** | Imperative CLI flags | Declarative YAML | Declarative YAML / Helm |
| **Multi-Container Apps**| Manual shell scripts | **Native & Coordinated** | **Native & Clustered** |
| **Target Scale** | Single Host / Developer | Single Host / Dev / Small Prod | Multi-Node Large Clusters |
| **Auto-Healing & Scheduling**| Basic restart policies | Basic restart policies | Advanced scheduling, replica sets |
| **Complexity** | Low | Low to Moderate | High |

---

## 11. Real-World Scenario Walkthrough

### Scenario: Modernizing a Legacy Multi-Script Deployment

#### The Legacy Problem
A development team maintained a 150-line Bash script (`deploy.sh`) containing four consecutive `docker run` commands with tangled network creation, volume creation, and `sleep 15` delays to allow MySQL to boot before launching their Rails app. New developers routinely encountered boot race conditions and port conflicts.

#### The Modernization Plan
1. Transcribed all `docker run` parameters into a clean `compose.yaml`.
2. Replaced `sleep 15` with a native MySQL `healthcheck` and `depends_on: { condition: service_healthy }`.
3. Created an isolated `backend` network.
4. Added an automated `.env` template (`.env.example`).
5. Replaced `deploy.sh` with `docker compose up -d`.

#### Results
- Onboarding time for new engineers dropped from 4 hours to **2 minutes**.
- Zero startup race condition failures in CI/CD pipeline runs.

---

## 12. Self-Assessment & Interview Questions

<details>
<summary><strong>1. What is the fundamental difference between `docker-compose` and `docker compose`?</strong></summary>

**Answer:**
`docker-compose` (with a hyphen) is Compose v1, a standalone Python program that is deprecated and unmaintained. `docker compose` (with a space) is Compose v2, a Go-based CLI plugin fully integrated into the Docker engine. Compose v2 is significantly faster, supports BuildKit natively, parses modern Compose Specification YAML without requiring a `version:` tag, and supports features like Compose Watch.
</details>

<details>
<summary><strong>2. How does Docker Compose determine the names of the containers, networks, and volumes it provisions?</strong></summary>

**Answer:**
Compose namespaces all objects using the **Project Name** (which defaults to the lowercase name of the directory containing the `compose.yaml` file, unless overridden by `-p` or `COMPOSE_PROJECT_NAME`). Container names follow the convention `<project>_<service>_<index>` (e.g. `myapp_web_1`), while networks and volumes follow `<project>_<name>` (e.g. `myapp_default`, `myapp_dbdata`).
</details>

<details>
<summary><strong>3. What happens when you run `docker compose down` versus `docker compose down -v`?</strong></summary>

**Answer:**
`docker compose down` stops running containers, removes the containers, and deletes the project's internal virtual networks. Persistent named volumes declared under `volumes:` are preserved intact. Adding the `-v` (or `--volumes`) flag instructs Compose to also delete all named volumes defined in the manifest, permanently destroying database and persistent application data.
</details>

<details>
<summary><strong>4. Why can't a service with `ports: - "8080:80"` be scaled to multiple replicas using `--scale`?</strong></summary>

**Answer:**
Scaling a service launches multiple container instances of that service. If the service defines a static host port mapping (`8080:80`), all replica containers will attempt to bind to host port 8080 simultaneously. The first container will bind successfully, while all subsequent containers will crash with `bind: address already in use`. To scale web services, omit the host port mapping (or use dynamic port publishing) and place a reverse proxy (like Nginx or Traefik) in front of the scaled replicas.
</details>

<details>
<summary><strong>5. What is the purpose of `docker compose run --rm <service> <command>`?</strong></summary>

**Answer:**
`docker compose run` starts a one-off task container using the specified service's image, environment, and volumes, but executes an alternate command (e.g., database schema migrations or test suites) instead of the default service command. The `--rm` flag guarantees that the one-off container is automatically deleted upon process exit.
</details>

<details>
<summary><strong>6. What does `docker compose up --remove-orphans` do, and when should it be used?</strong></summary>

**Answer:**
When services are deleted or renamed in `compose.yaml`, previous containers created for those services remain on the host as "orphans" (containers tagged with the project label but no longer present in the manifest). `--remove-orphans` instructs Compose to detect and permanently remove these defunct containers during deployment.
</details>

<details>
<summary><strong>7. How do you validate a Compose YAML file without actually launching containers?</strong></summary>

**Answer:**
Run `docker compose config`. This command parses the YAML, validates syntax against the Compose specification, interpolates all environment variables, and prints the fully resolved execution configuration. If syntax errors exist, it throws an immediate error with line numbers.
</details>

<details>
<summary><strong>8. Why should you avoid defining the top-level `version:` tag in modern `compose.yaml` files?</strong></summary>

**Answer:**
The Compose Specification deprecated the top-level `version:` attribute (e.g., `version: '3.8'`). Compose v2 automatically merges features from legacy 2.x and 3.x specifications into a single rolling specification. Declaring `version:` is unnecessary, triggers deprecation warnings in newer Compose releases, and can restrict access to modern features.
</details>

---

## 13. Hands-on Practice Exercises

<details>
<summary><strong>Exercise 1: Creating and Deploying a Dual-Service Stack</strong></summary>

**Goal:** Create a `compose.yaml` running Nginx serving an HTML file alongside a Redis cache.

**Step-by-step Solution:**
1. Create `compose.yaml`:
   ```yaml
   services:
     web:
       image: nginx:alpine
       ports:
         - "8088:80"
     cache:
       image: redis:alpine
   ```
2. Start stack:
   ```bash
   docker compose up -d
   ```
3. Verify status:
   ```bash
   docker compose ps
   curl http://localhost:8088
   ```
4. Tear down:
   ```bash
   docker compose down
   ```
</details>

<details>
<summary><strong>Exercise 2: Inspecting Project Namespacing</strong></summary>

**Goal:** Launch a stack with an explicit project name and inspect container prefixes.

**Step-by-step Solution:**
1. Launch with `-p`:
   ```bash
   docker compose -p enterprise-prod up -d
   ```
2. Inspect container names:
   ```bash
   docker ps --filter "label=com.docker.compose.project=enterprise-prod"
   # Container names start with: enterprise-prod-web-1
   ```
3. Tear down: `docker compose -p enterprise-prod down`.
</details>

<details>
<summary><strong>Exercise 3: Executing One-Off Database Commands</strong></summary>

**Goal:** Use `docker compose run` to query Redis without attaching to the primary container.

**Step-by-step Solution:**
1. Run stack in background: `docker compose up -d`
2. Run one-off CLI command:
   ```bash
   docker compose run --rm cache redis-cli -h cache ping
   # Outputs: PONG
   ```
</details>

<details>
<summary><strong>Exercise 4: Live Log Streaming with Filtering</strong></summary>

**Goal:** Filter and tail logs for a specific service with timestamps.

**Step-by-step Solution:**
1. Stream logs:
   ```bash
   docker compose logs -f -t web
   ```
</details>

<details>
<summary><strong>Exercise 5: Rebuilding Service Code Without Downtime</strong></summary>

**Goal:** Rebuild a single service while preserving the running database.

**Step-by-step Solution:**
1. Rebuild and replace:
   ```bash
   docker compose up -d --no-deps --build web
   ```
</details>

<details>
<summary><strong>Exercise 6: Validating Schema with `docker compose config`</strong></summary>

**Goal:** Catch indentation and syntax errors using the CLI linter.

**Step-by-step Solution:**
1. Validate file:
   ```bash
   docker compose config -q && echo "Valid YAML"
   ```
</details>

---

## 14. Summary & Reference Cheat Sheet

### Essential CLI Commands
```bash
docker compose up -d              # Start in background
docker compose up -d --build      # Rebuild images & start
docker compose ps                 # List service status
docker compose logs -f [service]  # Tail logs
docker compose exec <svc> sh      # Interactive shell
docker compose run --rm <svc> cmd # One-off task container
docker compose stop               # Pause containers
docker compose down               # Remove containers & networks
docker compose down -v            # Remove containers, networks & VOLUMES
docker compose config             # Validate YAML
```

### Golden Rules
1. **Never use `docker-compose`**: Use modern `docker compose`.
2. **Never use `version:` in YAML**: It is obsolete.
3. **Never run `docker compose down -v` in production**: Destroys data!
4. **Always bind internal databases to `127.0.0.1`** if publishing ports.

---

[← Previous Module: 04-volumes-and-networking](../04-volumes-and-networking/README.md) | [Next: 02-compose-multi-service.md →](02-compose-multi-service.md)
