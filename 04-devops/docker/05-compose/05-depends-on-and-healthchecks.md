# Startup Ordering, Readiness Probing, and Lifecycle Coordination

```
Track: DevOps & Containerization
Module: 05-compose
Prerequisites: 05-compose/01-compose-basics.md, 03-dockerfile/09-healthchecks.md
Difficulty: Intermediate to Advanced
Estimated Duration: 45 minutes
Target Docker Version: 27.x+ (Compose Specification v2.x+)
Last Validated: February 2025
```

---

## Table of Contents
1. [What & Why](#1-what--why)
2. [Core Concepts](#2-core-concepts)
3. [How It Works Internally](#3-how-it-works-internally)
4. [Syntax & Condition Reference](#4-syntax--condition-reference)
5. [Production Healthcheck Recipes for Databases & Brokers](#5-production-healthcheck-recipes-for-databases--brokers)
6. [Practical Implementation: One-Off Init Jobs & Schema Migrations](#6-practical-implementation-one-off-init-jobs--schema-migrations)
7. [Production Best Practices & Antipatterns](#7-production-best-practices--antipatterns)
8. [Common Pitfalls & Troubleshooting](#8-common-pitfalls--troubleshooting)
9. [Performance Tuning & Resource Optimization](#9-performance-tuning--resource-optimization)
10. [Security Hardening & Threat Model](#10-security-hardening--threat-model)
11. [Architectural Comparison Table](#11-architectural-comparison-table)
12. [Real-World Scenario Walkthrough](#12-real-world-scenario-walkthrough)
13. [Self-Assessment & Interview Questions](#13-self-assessment--interview-questions)
14. [Hands-on Practice Exercises](#14-hands-on-practice-exercises)
15. [Summary & Reference Cheat Sheet](#summary--reference-cheat-sheet)

---

## 1. What & Why

In multi-container distributed systems, race conditions during startup are the primary cause of intermittent deployment failures:
- An API web server boots in 1.2 seconds, attempts to query PostgreSQL, and immediately crashes because PostgreSQL requires 8 seconds to run WAL recovery and open its socket.
- A background worker starts processing queue tasks before Redis or RabbitMQ has initialized its exchange bindings.
- An application connects to a database before automated schema migrations (Flyway, Liquibase, Prisma) have executed, triggering missing-table runtime panics.

Docker Compose solves these coordination challenges using **Directed Acyclic Graph (DAG) Dependency Management** paired with **Healthcheck Readiness Probing**.

---

## 2. Core Concepts

### Process Started vs Service Ready
- **`service_started`**: The container process has been forked by the kernel and PID 1 is executing. This does **not** mean the database is ready to accept connections.
- **`service_healthy`**: Docker's active health probe executed inside the container returned exit code `0`. Traffic can now be safely routed to it.
- **`service_completed_successfully`**: The container ran an initialization task (e.g. database migration or seed script) and terminated cleanly with exit status `0`.

### The Three Startup Conditions in Compose v2
```yaml
depends_on:
  db:
    condition: service_healthy              # Waits for healthcheck = 0
  redis:
    condition: service_started              # Waits only for process start
  migration:
    condition: service_completed_successfully # Waits for container exit 0
```

---

## 3. How It Works Internally

### Directed Acyclic Graph (DAG) Execution Flow

When you execute `docker compose up`, Compose builds an internal dependency graph:

```
[database] (Runs Healthcheck: pg_isready)
    |
    v (Transitions to HEALTHY)
[migration] (Runs: prisma migrate deploy)
    |
    v (Exits cleanly: EXIT 0)
[api-server] (Starts process)
    |
    v (Transitions to HEALTHY)
[reverse-proxy] (Binds host port 80/443 & routes traffic)
```

Compose executes services sequentially along dependency edges. If any dependency fails its healthcheck or exits with a non-zero code, Compose **aborts the execution graph**, preventing dependent services from booting into a broken state.

---

## 4. Syntax & Condition Reference

### Short Form (Legacy / Process Start Only)
```yaml
# WARNING: Only waits for process creation! Unsafe for stateful databases!
depends_on:
  - db
```

### Long Form (Modern Compose Specification)
```yaml
depends_on:
  db:
    condition: service_healthy
    restart: true                          # Restart this service if 'db' restarts!
    required: true                         # Build fails if dependency is disabled
  migration-job:
    condition: service_completed_successfully
    required: true
```

---

## 5. Production Healthcheck Recipes for Databases & Brokers

### 1. PostgreSQL
```yaml
db:
  image: postgres:17-alpine
  environment:
    POSTGRES_USER: appuser
    POSTGRES_DB: appdb
  healthcheck:
    test: ["CMD-SHELL", "pg_isready -U appuser -d appdb"]
    interval: 5s
    timeout: 3s
    retries: 5
    start_period: 10s
    start_interval: 2s
```

### 2. MySQL / MariaDB
```yaml
mysql:
  image: mysql:8.4
  environment:
    MYSQL_ROOT_PASSWORD: rootsecretpassword
  healthcheck:
    test: ["CMD", "mysqladmin", "ping", "-h", "localhost", "-u", "root", "-prootsecretpassword"]
    interval: 5s
    timeout: 3s
    retries: 5
    start_period: 15s
```

### 3. Redis
```yaml
redis:
  image: redis:7-alpine
  healthcheck:
    test: ["CMD", "redis-cli", "ping"]
    interval: 3s
    timeout: 2s
    retries: 3
    start_period: 5s
```

### 4. RabbitMQ
```yaml
rabbitmq:
  image: rabbitmq:3.13-management-alpine
  healthcheck:
    test: ["CMD", "rabbitmq-diagnostics", "-q", "ping"]
    interval: 10s
    timeout: 5s
    retries: 5
    start_period: 20s
```

### 5. Web API (Node.js / Python / Go)
```yaml
api:
  image: my-api:latest
  healthcheck:
    test: ["CMD", "curl", "-f", "http://localhost:3000/healthz"]
    interval: 10s
    timeout: 2s
    retries: 3
    start_period: 5s
```

---

## 6. Practical Implementation: One-Off Init Jobs & Schema Migrations

The canonical production pattern for running database schema migrations before launching the web server.

```yaml
# compose.yaml

services:
  # 1. Database
  db:
    image: postgres:17-alpine
    environment:
      POSTGRES_USER: postgres
      POSTGRES_PASSWORD: secretpassword
      POSTGRES_DB: app_db
    volumes:
      - pg_data:/var/lib/postgresql/data
    healthcheck:
      test: ["CMD-SHELL", "pg_isready -U postgres -d app_db"]
      interval: 3s
      timeout: 2s
      retries: 10
      start_period: 5s

  # 2. Ephemeral Migration Runner (Runs once and exits)
  migration:
    image: my-api:latest
    command: ["npm", "run", "db:migrate"]
    environment:
      DATABASE_URL: "postgresql://postgres:secretpassword@db:5432/app_db"
    depends_on:
      db:
        condition: service_healthy
    restart: "no"

  # 3. Web API (Waits for migration to complete successfully!)
  api:
    image: my-api:latest
    command: ["node", "dist/server.js"]
    ports:
      - "8080:8080"
    environment:
      DATABASE_URL: "postgresql://postgres:secretpassword@db:5432/app_db"
    depends_on:
      db:
        condition: service_healthy
      migration:
        condition: service_completed_successfully
    restart: unless-stopped

volumes:
  pg_data:
```

---

## 7. Production Best Practices & Antipatterns

### Best Practices
- **Always Use Long Form `depends_on` for Stateful Services**: Never use short-form `depends_on: [db]`. Always pair with `condition: service_healthy`.
- **Set Realistic `start_period`**: JVM applications (Spring Boot) and heavy databases require 15-45s of warmup before probes should count against failure retries.
- **Leverage Docker 25+ `start_interval`**: Use `start_interval: 2s` to probe frequently during startup without keeping a noisy 2-second interval during steady-state production.
- **Enable `restart: true` on Dependencies**: Setting `restart: true` inside `depends_on` ensures that if a database crashes and restarts, the dependent API automatically restarts to establish fresh connection pools.

### Anti-Patterns
- **Using Brittle Shell Sleep Hacks**: Adding `sleep 15` in Docker entrypoints is fragile and fails under variable CPU load.
- **Deep Healthchecks in Liveness Probes**: A healthcheck should verify local readiness (is the process responsive?). Do not fail local healthchecks because an external payment gateway is down.
- **Circular Dependencies**: Service A depends on Service B, while Service B depends on Service A. Compose will throw a fatal error.

---

## 8. Common Pitfalls & Troubleshooting

| Symptom | Root Cause | Diagnosis Command | Permanent Fix |
|:---|:---|:---|:---|
| Compose throws `circular dependency detected` | Service A depends on Service B, which depends on Service A | Inspect `depends_on` trees in `compose.yaml` | Break cycle by extracting shared init tasks into a dedicated service. |
| API never starts; stays permanently in pending state | Database healthcheck command is failing or never exiting with 0 | `docker compose ps` shows `db (unhealthy)` | Inspect health logs: `docker inspect <db_id> --format '{{json .State.Health}}'`. |
| Migration fails with `connection refused` despite `depends_on: db` | Used short-form `depends_on: [db]` which only waited for process start, not ready state | Inspect startup timestamps | Change to `depends_on: { db: { condition: service_healthy } }`. |
| Compose fails with `dependency failed to complete successfully` | Migration service exited with non-zero exit code (e.g. exit 1) | `docker compose logs migration` | Fix migration SQL script or syntax error. |
| Healthcheck command returns `127` (Command not found) | Probe command (e.g., `curl`) does not exist in target container image | `docker inspect` shows `exec: "curl": executable file not found` | Use language native probe (`node`, `python`, `wget`) or install curl. |
| `depends_on` condition ignored on standalone `docker run` | `depends_on` is a Compose specification directive; standalone Docker CLI has no concept of it | N/A | Execute multi-container stacks via `docker compose up`. |

---

## 9. Performance Tuning & Resource Optimization

1. **Slashing CI Pipeline Startup Times with `start_interval`**:
   In older versions, setting `interval: 30s` meant an API container could wait up to 30s after the database booted before the first healthcheck passed. Adding `start_interval: 1s` allows Compose to detect readiness within 1000ms of database socket initialization, cutting pipeline test durations dramatically.
2. **Ultra-Lightweight Database Probes**:
   Use `pg_isready` for Postgres rather than `SELECT 1;`. `pg_isready` checks socket responsiveness without completing a full authentication handshake or parsing SQL, consuming minimal CPU.

---

## 10. Security Hardening & Threat Model

```
+--------------------------------------------------------------+
| ATTACK: Healthcheck Command Injection                       |
+--------------------------------------------------------------+
| Vulnerable Setup: Healthcheck command uses shell form with   |
| unverified environment variable expansion:                   |
|   test: "curl http://localhost:$PORT/$ENDPOINT"              |
|                                                              |
| If an attacker compromises an environment file and injects:  |
|   ENDPOINT="health; cat /etc/shadow > /tmp/leaked"           |
| The Docker daemon executes the injected commands!            |
|                                                              |
| HARDENING: Always use Exec Form JSON arrays for healthcheck: |
|   test: ["CMD", "curl", "-f", "http://localhost:8080/health"]|
+--------------------------------------------------------------+
```

---

## 11. Architectural Comparison Table

| Coordination Pattern | Accuracy | Latency Overhead | Failure Recovery | Complexity |
|:---|:---|:---|:---|:---|
| **`sleep 15` Shell Hack** | Low (Guesswork) | High (Wasted seconds) | Fails on slow hosts | Brittle |
| **Short `depends_on: [db]`**| Low (Process start only)| Zero | Crashes if DB not ready| Low |
| **`condition: service_healthy`**| **Maximum (Socket verified)**| Low (< 1-2s)| **Deterministic & Auto-heals**| Moderate |
| **`wait-for-it.sh` Wrapper** | High (TCP port open) | Low | Adds wrapper script bloat | Moderate |

---

## 12. Real-World Scenario Walkthrough

### Scenario: Eliminating 502 Bad Gateway Outages During Zero-Downtime Deployments

#### Background
A production microservices cluster executed rolling updates using Docker Compose. On every deployment, end users experienced a 12-second window of `502 Bad Gateway` errors while Nginx attempted to route traffic to the newly booted API container.

#### Root Cause
The API container required 10 seconds to connect to Kafka, load caches, and compile JIT bytecode. Nginx's `depends_on` only checked that the API container process had started, immediately routing production HTTP traffic to an uninitialized process.

#### Solution
1. Implemented Spring Boot Actuator `/actuator/health/readiness`.
2. Added healthcheck to the API service:
   ```yaml
   api:
     image: backend:latest
     healthcheck:
       test: ["CMD", "wget", "-qO-", "http://localhost:8080/actuator/health/readiness"]
       interval: 3s
       timeout: 2s
       retries: 5
       start_period: 12s
       start_interval: 1s
   ```
3. Configured Nginx:
   ```yaml
   proxy:
     image: nginx:alpine
     depends_on:
       api:
         condition: service_healthy
   ```
4. Zero-downtime achieved: Nginx only receives the new container IP once readiness is verified.

---

## 13. Self-Assessment & Interview Questions

<details>
<summary><strong>1. What are the three valid values for `condition:` under `depends_on` in Compose v2?</strong></summary>

**Answer:**
1. `service_started`: Waits for the dependency container's process (PID 1) to start.
2. `service_healthy`: Waits for the dependency container's healthcheck probe to pass (return exit code 0).
3. `service_completed_successfully`: Waits for an ephemeral task container to terminate cleanly with exit code 0.
</details>

<details>
<summary><strong>2. How does `restart: true` inside a `depends_on` block function?</strong></summary>

**Answer:**
When `restart: true` is set, Docker Compose monitors the dependency service. If the dependency service is restarted, updated, or crashes and recovers, Compose automatically restarts the dependent service as well. This is vital for applications whose database connection pools or socket connections do not automatically reconnect after a database restart.
</details>

<details>
<summary><strong>3. Why is `pg_isready` preferred over running `psql -c 'SELECT 1'` for PostgreSQL healthchecks?</strong></summary>

**Answer:**
`pg_isready` is a lightweight C utility designed specifically for connection probing. It checks server status without opening a full database transaction, parsing SQL, or allocating heavy backend session memory. It also avoids logging thousands of audit entries or requiring passwords in connection strings.
</details>

<details>
<summary><strong>4. What happens if a service configured with `condition: service_completed_successfully` exits with code 1?</strong></summary>

**Answer:**
Docker Compose halts the deployment graph immediately. The dependent services will never be started, and `docker compose up` exits with an error status, alerting CI/CD pipelines that an initialization step (such as database migration) failed.
</details>

<details>
<summary><strong>5. How does `start_interval` (Docker 25+) improve container startup times compared to `interval`?</strong></summary>

**Answer:**
`start_interval` specifies how frequently the healthcheck runs *during* the initial `start_period`. Setting a normal `interval: 30s` with `start_interval: 2s` allows Docker to probe every 2 seconds during container boot, transitioning the container to `healthy` within seconds of readiness, while dropping back to the less resource-intensive 30-second interval during normal operation.
</details>

<details>
<summary><strong>6. Can Docker Compose resolve circular dependencies between services?</strong></summary>

**Answer:**
No. Docker Compose constructs a Directed Acyclic Graph (DAG). If Service A depends on Service B, and Service B depends on Service A, Compose detects a cycle and terminates with `circular dependency detected`. Cycles must be eliminated by refactoring dependencies or removing unnecessary `depends_on` links.
</details>

<details>
<summary><strong>7. What happens if a service has no `healthcheck` declared, but another service specifies `condition: service_healthy` on it?</strong></summary>

**Answer:**
Docker Compose throws a configuration validation error and refuses to start:
`service "api" depends on undefined healthcheck of service "db"`.
A healthcheck must be explicitly defined either in the Compose file or in the service's base Dockerfile.
</details>

<details>
<summary><strong>8. Why should you avoid executing deep dependency checks (e.g., pinging third-party APIs) in container healthchecks?</strong></summary>

**Answer:**
A container healthcheck represents the health of that specific container instance. If a deep check fails due to an external network glitch or third-party API outage, Docker will mark the local container unhealthy. This can trigger unnecessary container restarts, causing cascading failover storms across an entire cluster without resolving the external issue.
</details>

---

## 14. Hands-on Practice Exercises

<details>
<summary><strong>Exercise 1: Coordinating Startup with `service_healthy`</strong></summary>

**Goal:** Create a Redis service with a healthcheck and a worker that blocks until Redis is healthy.

**Step-by-step Solution:**
1. Create `compose.yaml`:
   ```yaml
   services:
     redis:
       image: redis:alpine
       healthcheck:
         test: ["CMD", "redis-cli", "ping"]
         interval: 2s
         timeout: 2s
         retries: 3
     worker:
       image: alpine
       command: sh -c "echo 'Redis is ready!' && sleep 3600"
       depends_on:
         redis:
           condition: service_healthy
   ```
2. Run: `docker compose up`
3. Observe console output: `worker` starts only after `redis` reports `(healthy)`.
</details>

<details>
<summary><strong>Exercise 2: Coordinating an Ephemeral Database Migration Job</strong></summary>

**Goal:** Run a mock migration container that seeds a file, and ensure the web service waits for completion.

**Step-by-step Solution:**
1. Create `compose.yaml`:
   ```yaml
   services:
     migrator:
       image: alpine
       command: sh -c "echo 'Migrating tables...' && sleep 3 && exit 0"
       restart: "no"
     web:
       image: alpine
       command: sh -c "echo 'Starting web server...' && sleep 3600"
       depends_on:
         migrator:
           condition: service_completed_successfully
   ```
2. Execute `docker compose up` and watch the sequential execution order.
</details>

<details>
<summary><strong>Exercise 3: Observing Abort on Migration Failure</strong></summary>

**Goal:** Configure a migrator that exits with code 1 and verify the web service never starts.

**Step-by-step Solution:**
1. Change migrator command to `exit 1`.
2. Run `docker compose up`.
3. Notice Compose aborts and `web` is never executed.
</details>

<details>
<summary><strong>Exercise 4: Inspecting Health Logs via `docker inspect`</strong></summary>

**Goal:** View probe exit codes and stdout captured by the Docker daemon.

**Step-by-step Solution:**
1. Query health details:
   ```bash
   docker inspect $(docker compose ps -q redis) --format '{{json .State.Health.Log}}' | jq .
   ```
</details>

<details>
<summary><strong>Exercise 5: Testing `restart: true` Dependency Cascade</strong></summary>

**Goal:** Verify that restarting a database triggers an automated restart of dependent services.

**Step-by-step Solution:**
1. Add `restart: true` to worker's `depends_on.redis`.
2. In terminal: `docker compose restart redis`.
3. Check `docker compose ps` to see `worker` was automatically restarted.
</details>

<details>
<summary><strong>Exercise 6: Breaking a Circular Dependency</strong></summary>

**Goal:** Intentionally create a circular dependency error, observe output, and fix it.

**Step-by-step Solution:**
1. Add `depends_on: [worker]` to redis, and `depends_on: [redis]` to worker.
2. Run `docker compose up` -> Observe `circular dependency detected`.
3. Remove circular reference to resolve.
</details>

---

## 15. Summary & Reference Cheat Sheet

### Syntax Quick Reference
```yaml
depends_on:
  service_name:
    condition: service_healthy             # Wait for healthcheck = 0
    condition: service_completed_successfully # Wait for task container exit 0
    condition: service_started             # Wait only for process start
    restart: true                          # Restart when dependency restarts
```

### Top 4 Golden Rules
1. **Never use `sleep`** to coordinate container startup.
2. **Always pair `depends_on`** with `condition: service_healthy` for databases.
3. **Use `service_completed_successfully`** for migration and database seed containers.
4. **Use `start_interval`** to accelerate startup probing in CI/CD.

---

[← Previous: 04-env-files-and-interpolation.md](04-env-files-and-interpolation.md) | [Next: 06-profiles-and-overrides.md →](06-profiles-and-overrides.md)
