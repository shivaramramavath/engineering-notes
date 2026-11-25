# Multi-Service Architecture in Docker Compose v2

```
Track: DevOps & Containerization
Module: 05-compose
Prerequisites: 05-compose/01-compose-basics.md, 04-volumes-and-networking/05-user-defined-networks-and-dns.md
Difficulty: Intermediate to Advanced
Estimated Duration: 50 minutes
Target Docker Version: 27.x+
Last Validated: February 2025
```

---

## Table of Contents
1. [What & Why](#1-what--why)
2. [Core Concepts](#2-core-concepts)
3. [How It Works Internally](#3-how-it-works-internally)
4. [Multi-Tier Architecture Design](#4-multi-tier-architecture-design)
5. [Syntax & Implementation Reference](#5-syntax--implementation-reference)
6. [Practical Implementation: Complete Enterprise Stack](#6-practical-implementation-complete-enterprise-stack)
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

Modern production systems rarely consist of a single standalone container. A realistic web application incorporates:
- An edge reverse proxy / load balancer (Nginx / Traefik).
- A public API service (Node.js, FastAPI, Spring Boot).
- An asynchronous queue worker processing long-running jobs (Celery, BullMQ).
- A relational database (PostgreSQL, MySQL).
- An in-memory cache and session store (Redis).
- A message broker (RabbitMQ, Kafka).

Docker Compose shines as the orchestration glue for these multi-service topologies. It enables developers to instantiate a complete, interconnected, enterprise microservice architecture on local workstations or single-node production servers with a single command, eliminating the friction of manual service coordination.

---

## 2. Core Concepts

### Automatic Network Fabric
When Compose starts, it automatically provisions a dedicated user-defined bridge network for the project (e.g., `project_default`).
- Every service defined in the manifest joins this network automatically.
- Every container can reach every other container using its **service name** (e.g., `http://database:5432`, `redis://cache:6379`) via Docker's embedded DNS server (`127.0.0.11`).

### Network Tiering & Segregation
In high-security production designs, placing all containers on a single flat default network is dangerous. Compose enables explicit **network segregation**:
- **`public-net`**: Accessible to reverse proxies and frontend APIs.
- **`internal-net`**: Accessible to APIs, background workers, and internal caches.
- **`data-net`**: Accessible strictly to APIs, workers, and databases (completely unreachable from reverse proxies or external clients).

### Dependency DAG & Readiness Coordination
By default, `depends_on: [db]` only waits until the `db` container process starts. However, relational databases take 5-20 seconds to initialize schemas.
Compose v2 provides deterministic startup coordination via **Healthcheck Conditions**:
```yaml
depends_on:
  db:
    condition: service_healthy
```
The dependent service will physically pause startup until the database healthcheck transitions to `healthy`.

---

## 3. How It Works Internally

### Inter-Service Resolution & Routing Mesh

```
+-------------------------------------------------------------------------+
|                         DOCKER HOST SYSTEM                              |
|                                                                         |
|  Physical Inbound Port: 443 / 80                                        |
|         |                                                               |
|         v                                                               |
|  +-------------------------------------------------------------------+  |
|  | Reverse Proxy (Nginx) [Port: 80, 443]                             |  |
|  | Connected to: frontend-net                                        |  |
|  +-------------------+-----------------------------------------------+  |
|                      |                                                  |
|                      v (http://api:3000)                                |
|  +-------------------------------------------------------------------+  |
|  | Web API (Node / Python) [Internal Port: 3000]                     |  |
|  | Dual-Homed: Connected to frontend-net AND backend-net            |  |
|  +---------+-----------------------------------------+---------------+  |
|            |                                         |                  |
|            v (redis://cache:6379)                    v (amqp://broker)  |
|  +------------------------+           +------------------------------+  |
|  | Redis Cache            |           | RabbitMQ Message Broker      |  |
|  | Connected: backend-net |           | Connected: backend-net       |  |
|  +------------------------+           +--------------+---------------+  |
|                                                      |                  |
|                                                      v (Async jobs)     |
|                                       +------------------------------+  |
|                                       | Background Worker            |  |
|                                       | Dual-Homed: backend & data   |  |
|                                       +--------------+---------------+  |
|                                                      |                  |
|                                                      v (sql:5432)       |
|  +-------------------------------------------------------------------+  |
|  | PostgreSQL Database [Port: 5432]                                  |  |
|  | Connected EXCLUSIVELY to: data-net (Zero external access!)        |  |
|  +-------------------------------------------------------------------+  |
+-------------------------------------------------------------------------+
```

---

## 4. Multi-Tier Architecture Design

### The 3-Tier Enterprise Pattern

1. **Presentation Tier (`proxy`, `frontend`)**:
   - Exposed to host ports (`80`, `443`).
   - Terminates TLS, handles gzip compression, serves static UI assets.
   - Forwards dynamic requests to API service.
2. **Application Tier (`api`, `worker`)**:
   - No public host port bindings.
   - Executes business logic, authentication, and background task queues.
   - Reaches cache, broker, and database via private networks.
3. **Data Tier (`db`, `cache`, `broker`)**:
   - Zero host port exposures (`0.0.0.0`).
   - Backed by persistent named Docker volumes.
   - Isolated in private subnets with strict authentication.

---

## 5. Syntax & Implementation Reference

### Declaring Multi-Tier Networks and Volume Bindings

```yaml
services:
  api:
    image: my-api:latest
    networks:
      - frontend-tier
      - backend-tier
    depends_on:
      db:
        condition: service_healthy
      redis:
        condition: service_healthy

  db:
    image: postgres:17-alpine
    volumes:
      - pg_data:/var/lib/postgresql/data
    networks:
      - backend-tier
    healthcheck:
      test: ["CMD-SHELL", "pg_isready -U postgres -d app_db"]
      interval: 5s
      timeout: 3s
      retries: 5
      start_period: 10s

  redis:
    image: redis:7-alpine
    volumes:
      - redis_data:/data
    networks:
      - backend-tier
    healthcheck:
      test: ["CMD", "redis-cli", "ping"]
      interval: 5s
      timeout: 2s
      retries: 3

networks:
  frontend-tier:
    driver: bridge
  backend-tier:
    driver: bridge

volumes:
  pg_data:
  redis_data:
```

---

## 6. Practical Implementation: Complete Enterprise Stack

Below is a complete, production-grade 5-service application stack featuring an Nginx reverse proxy, Node.js API, Celery/Python worker, Redis cache, and PostgreSQL database with healthcheck coordination and multi-tier network segregation.

```yaml
# compose.yaml

services:
  # -------------------------------------------------------------
  # 1. Edge Reverse Proxy (Public Entrypoint)
  # -------------------------------------------------------------
  proxy:
    image: nginx:1.27-alpine
    restart: always
    ports:
      - "80:80"
      - "443:443"
    volumes:
      - ./nginx/nginx.conf:/etc/nginx/nginx.conf:ro
      - ./nginx/certs:/etc/ssl/certs:ro
    networks:
      - public-net
    depends_on:
      - api

  # -------------------------------------------------------------
  # 2. Application Web API
  # -------------------------------------------------------------
  api:
    build:
      context: ./api
      dockerfile: Dockerfile
    restart: unless-stopped
    environment:
      - NODE_ENV=production
      - PORT=3000
      - DATABASE_URL=postgresql://app_user:${DB_PASSWORD}@db:5432/production_db
      - REDIS_URL=redis://cache:6379/0
    networks:
      - public-net
      - internal-net
    depends_on:
      db:
        condition: service_healthy
      cache:
        condition: service_healthy

  # -------------------------------------------------------------
  # 3. Asynchronous Queue Worker
  # -------------------------------------------------------------
  worker:
    build:
      context: ./worker
      dockerfile: Dockerfile
    restart: unless-stopped
    environment:
      - DATABASE_URL=postgresql://app_user:${DB_PASSWORD}@db:5432/production_db
      - REDIS_URL=redis://cache:6379/0
    networks:
      - internal-net
    depends_on:
      db:
        condition: service_healthy
      cache:
        condition: service_healthy

  # -------------------------------------------------------------
  # 4. In-Memory Cache & Message Broker
  # -------------------------------------------------------------
  cache:
    image: redis:7-alpine
    restart: always
    command: ["redis-server", "--appendonly", "yes"]
    volumes:
      - cache_data:/data
    networks:
      - internal-net
    healthcheck:
      test: ["CMD", "redis-cli", "ping"]
      interval: 5s
      timeout: 2s
      retries: 3

  # -------------------------------------------------------------
  # 5. Persistent Relational Database
  # -------------------------------------------------------------
  db:
    image: postgres:17-alpine
    restart: always
    environment:
      POSTGRES_DB: production_db
      POSTGRES_USER: app_user
      POSTGRES_PASSWORD: ${DB_PASSWORD}
    volumes:
      - db_data:/var/lib/postgresql/data
    networks:
      - internal-net
    healthcheck:
      test: ["CMD-SHELL", "pg_isready -U app_user -d production_db"]
      interval: 5s
      timeout: 3s
      retries: 5
      start_period: 15s

# -------------------------------------------------------------
# Isolated Network Topologies
# -------------------------------------------------------------
networks:
  public-net:
    driver: bridge
  internal-net:
    driver: bridge

# -------------------------------------------------------------
# Persistent Volumes
# -------------------------------------------------------------
volumes:
  db_data:
  cache_data:
```

---

## 7. Production Best Practices & Antipatterns

### Best Practices
- **Segregate Public Facing from Backend Services**: Keep reverse proxies on a public network and databases on an internal network.
- **Always Pair `depends_on` with `condition: service_healthy`**: Never rely on process start alone for stateful databases; define a real readiness probe.
- **Enforce Restart Policies**: Use `restart: unless-stopped` or `restart: always` across all production services to ensure containers recover from node reboots.
- **Centralize Common Configurations with YAML Anchors**: Reuse logging, environment, and network definitions using standard YAML anchors (`&default-logging` / `*default-logging`).
- **Store Persistent Data Only in Named Volumes**: Never let databases or cache write-ahead logs write to anonymous volumes.

### Anti-Patterns
- **Exposing Database Ports to Host**: Adding `ports: - "5432:5432"` on the database container invites external attacks. Remove `ports:` entirely; the API connects via the internal network name `db:5432`.
- **Using Hardcoded Connection Strings**: Avoid hardcoding credentials; use environment variable interpolation (`${DB_PASSWORD}`).
- **Running Database Migrations in the Main API Process**: Concurrently scaling an API service running auto-migrations risks database lock contention. Run migrations via `docker compose run --rm api npm run migrate` as a dedicated deployment step.

---

## 8. Common Pitfalls & Troubleshooting

| Symptom | Root Cause | Diagnosis Command | Permanent Fix |
|:---|:---|:---|:---|
| Nginx fails to start with `host not found in upstream "api"` | Nginx booted before API container had registered its DNS name | `docker compose logs proxy` | Add `depends_on: [api]` to proxy service. |
| API crashes repeatedly with database connection timeout | API booted before Postgres completed table initialization | Check boot order in `docker compose logs` | Add `depends_on: { db: { condition: service_healthy } }`. |
| Proxy cannot ping database | Expected behavior: Proxy and DB do not share any network | `docker compose exec proxy ping db` | Keep them isolated! Route requests via the API tier. |
| Redis data lost after running `docker compose down -v` | `-v` flag deleted the persistent named volume | `docker volume ls` | Avoid `-v` in production; take automated volume backups. |
| Worker container cannot find Redis cache | Worker service missing `internal-net` network declaration | Inspect `worker` networks | Ensure both `worker` and `cache` join `internal-net`. |
| Host disk fills up rapidly from multi-service logging | Default json-file logging driver has no log rotation configured | `docker system df` | Configure `logging` options with `max-size: "10m"` and `max-file: "3"`. |

---

## 9. Performance Tuning & Resource Optimization

1. **Configuring Log Rotation Globally Across All Services**:
   By default, Docker's `json-file` logging driver retains container logs indefinitely until host disk space is exhausted. Configure automated log rotation:
   ```yaml
   x-logging: &default-logging
     driver: "json-file"
     options:
       max-size: "20m"
       max-file: "3"

   services:
     api:
       image: my-api
       logging: *default-logging
     db:
       image: postgres:17
       logging: *default-logging
   ```
2. **Redis In-Memory Footprint Limits**:
   Always cap Redis memory in `compose.yaml` to prevent it from consuming all host RAM:
   ```yaml
   cache:
     image: redis:7-alpine
     command: ["redis-server", "--maxmemory", "512mb", "--maxmemory-policy", "allkeys-lru"]
   ```

---

## 10. Security Hardening & Threat Model

```
+--------------------------------------------------------------+
| ATTACK SCENARIO: Compromise of Public Nginx Reverse Proxy    |
+--------------------------------------------------------------+
| Attacker discovers zero-day vulnerability in Nginx.          |
| Attacker spawns shell inside `proxy` container.              |
| Attacker attempts to connect directly to PostgreSQL database.|
|                                                              |
| WITH MULTI-TIER SEGREGATION:                                 |
| - `proxy` is connected ONLY to `public-net`.                 |
| - `db` is connected ONLY to `internal-net`.                  |
| - Linux kernel drops all packets between proxy and db.       |
|                                                              |
| OUTCOME: ATTACKER CANNOT ACCESS OR DUMP DATABASE RECORDS!   |
+--------------------------------------------------------------+
```

---

## 11. Architectural Comparison Table

| Architecture Pattern | Network Isolation | Scalability | Blast Radius on Breach | Operational Complexity |
|:---|:---|:---|:---|:---|
| **Single Flat Network** | None (All containers share 1 bridge)| High | **Severe (Full lateral access)**| Low |
| **Tiered Multi-Network**| **High (Segmented by tier)** | High | **Contained to tier** | Low to Moderate |
| **Host Network Mode** | **Zero (Shares host stack)** | Low (Port clashes)| Maximum | Low |

---

## 12. Real-World Scenario Walkthrough

### Scenario: Race Condition on Microservice Startup in Continuous Deployment

#### Problem Statement
An automated CI/CD pipeline deployed a 4-service stack (`web`, `api`, `postgres`, `redis`). In 25% of deployments, the API service crashed on boot with:
`Fatal Error: connect ECONNREFUSED 172.22.0.2:5432`.
The operations team added a naive `sleep 10` to the API Dockerfile entrypoint script, but during heavy CI load, Postgres took 18 seconds to initialize, causing builds to fail again.

#### Root Cause Analysis
The `compose.yaml` used basic `depends_on`:
```yaml
depends_on:
  - db
```
Docker started the `db` container process and immediately launched `api`. However, PostgreSQL requires initialization time to run database clustering and startup routines before opening its TCP listening socket.

#### Remediation
1. Implemented a robust healthcheck on PostgreSQL:
   ```yaml
   db:
     image: postgres:17-alpine
     healthcheck:
       test: ["CMD-SHELL", "pg_isready -U postgres"]
       interval: 2s
       timeout: 2s
       retries: 10
       start_period: 5s
   ```
2. Updated API dependency condition:
   ```yaml
   api:
     depends_on:
       db:
         condition: service_healthy
   ```
3. Deployment failure rate dropped from 25% to **0%**.

---

## 13. Self-Assessment & Interview Questions

<details>
<summary><strong>1. Why is `depends_on: [db]` insufficient to prevent an application from crashing during initial database startup?</strong></summary>

**Answer:**
By default, `depends_on` only checks that the dependency container's root process (PID 1) has been created and transitioned to `running`. It does not wait for the application inside that container (e.g. Postgres, MySQL) to finish its internal bootstrapping, create databases, or open its listening TCP socket. To ensure true readiness, `depends_on` must be configured with `condition: service_healthy` coupled with a valid `healthcheck` definition on the target service.
</details>

<details>
<summary><strong>2. How does Docker Compose enable inter-service communication without publishing host ports?</strong></summary>

**Answer:**
Compose automatically creates a user-defined bridge network and attaches all services to it. Docker's embedded DNS server (`127.0.0.11`) resolves the service name (e.g. `db`, `cache`) directly to the container's private virtual IP address on that network. Communication flows securely over the virtual bridge without publishing any ports to the host (`0.0.0.0`).
</details>

<details>
<summary><strong>3. What is a "dual-homed" container in Docker Compose?</strong></summary>

**Answer:**
A dual-homed container is a service that is attached to two distinct Docker networks simultaneously (e.g., connected to both `public-net` and `internal-net`). This service receives two virtual network interfaces and can communicate with services on both networks, effectively acting as an application gateway or bridge between public and private tiers.
</details>

<details>
<summary><strong>4. How can you define common logging or environment configurations once and reuse them across multiple services in `compose.yaml`?</strong></summary>

**Answer:**
Using standard **YAML extension fields and anchors**:
```yaml
x-logging: &default-logging
  driver: "json-file"
  options: { max-size: "10m", max-file: "3" }

services:
  api:
    image: my-api
    logging: *default-logging
  worker:
    image: my-worker
    logging: *default-logging
```
</details>

<details>
<summary><strong>5. What is the security advantage of omitting `ports:` on a database service in Compose?</strong></summary>

**Answer:**
Omitting `ports:` guarantees that the database container does not bind to any host network interfaces (`0.0.0.0`). The database is physically unreachable from the public internet, host LAN, or other host processes, and can only be queried by containers that explicitly share a private Docker network with it.
</details>

<details>
<summary><strong>6. How do you scale a single stateless service in a multi-service Compose stack without scaling the stateful database?</strong></summary>

**Answer:**
Use the `--scale` flag targeting only the desired service:
`docker compose up -d --scale worker=5`
This scales the `worker` service to 5 container instances while keeping `db`, `cache`, and `api` at single instances.
</details>

<details>
<summary><strong>7. How do you execute a database migration script against a multi-service Compose stack without restarting the running API?</strong></summary>

**Answer:**
Use `docker compose run --rm`:
`docker compose run --rm api npm run db:migrate`
This spins up a temporary, isolated replica of the `api` service attached to the same network and environment, executes the migration command, and automatically destroys the temporary container on completion.
</details>

<details>
<summary><strong>8. What happens to named volumes when you update image tags in a multi-service Compose file and run `docker compose up -d`?</strong></summary>

**Answer:**
Docker Compose pulls the new image and recreates the container, but it automatically reattaches the existing named volume. All database files and persistent data remain completely untouched and are mounted into the new container version.
</details>

---

## 14. Hands-on Practice Exercises

<details>
<summary><strong>Exercise 1: Building a 3-Tier Multi-Network Stack</strong></summary>

**Goal:** Create a Compose file with an isolated frontend and backend network, verifying the frontend cannot reach the database.

**Step-by-step Solution:**
1. Create `compose.yaml`:
   ```yaml
   services:
     web:
       image: alpine
       command: sleep 3600
       networks: [frontend]
     api:
       image: alpine
       command: sleep 3600
       networks: [frontend, backend]
     db:
       image: alpine
       command: sleep 3600
       networks: [backend]

   networks:
     frontend:
     backend:
   ```
2. Start stack: `docker compose up -d`
3. Test web -> api: `docker compose exec web ping -c 1 api` (Succeeds)
4. Test web -> db: `docker compose exec web ping -c 1 db` (Fails - Isolated!)
5. Clean up: `docker compose down`
</details>

<details>
<summary><strong>Exercise 2: Implementing Deterministic Startup with `service_healthy`</strong></summary>

**Goal:** Configure a Postgres database with a healthcheck and an API that blocks until the database is ready.

**Step-by-step Solution:**
1. Author manifest with `depends_on: { db: { condition: service_healthy } }`.
2. Run `docker compose up` and watch console logs verify API startup is paused until Postgres reports `healthy`.
</details>

<details>
<summary><strong>Exercise 3: Scaling Workers Dynamically</strong></summary>

**Goal:** Scale a background worker service from 1 to 3 instances and observe DNS round-robin.

**Step-by-step Solution:**
1. Run stack: `docker compose up -d --scale worker=3`
2. Run `docker compose ps` to inspect `worker-1`, `worker-2`, `worker-3`.
3. Query worker hostname from API container: `docker compose exec api nslookup worker` -> 3 IPs returned!
</details>

<details>
<summary><strong>Exercise 4: Running One-Off Database Seeds</strong></summary>

**Goal:** Execute a one-off database seed script without disturbing running services.

**Step-by-step Solution:**
1. Run: `docker compose run --rm db psql -U postgres -c "SELECT current_database();"`
</details>

<details>
<summary><strong>Exercise 5: Inspecting Aggregated Multi-Service Logs</strong></summary>

**Goal:** Stream logs across multiple services with color-coded service prefixes.

**Step-by-step Solution:**
1. Execute: `docker compose logs -f --tail=50 api db`
</details>

<details>
<summary><strong>Exercise 6: Configuring Global Log Rotation via YAML Anchors</strong></summary>

**Goal:** Implement automated 10MB log rotation across all services using YAML anchors.

**Step-by-step Solution:**
1. Configure `x-logging` anchor and apply to all services.
2. Validate syntax: `docker compose config`.
</details>

---

## 15. Summary & Reference Cheat Sheet

### Core Compose Directives for Multi-Service
```yaml
depends_on:
  service_name:
    condition: service_healthy # Wait for health probe
    restart: true              # Restart if dependency updates

networks:
  - public-net                 # Segmented access
  - internal-net

volumes:
  - data_vol:/path             # Persistent storage
```

### 4 Golden Architecture Rules
1. **Never expose database ports** (`ports: - "5432:5432"`) in production.
2. **Always pair `depends_on`** with `condition: service_healthy`.
3. **Segregate tiers** using multiple user-defined networks (`public-net` vs `internal-net`).
4. **Use YAML anchors** (`&anchor` / `*anchor`) to eliminate duplicate logging/env definitions.

---

[← Previous: 01-compose-basics.md](01-compose-basics.md) | [Next: 03-compose-file-reference.md →](03-compose-file-reference.md)
