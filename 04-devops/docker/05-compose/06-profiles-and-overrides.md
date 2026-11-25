# Compose Profiles, Multi-File Overrides, and Configuration Inheritance

```
Track: DevOps & Containerization
Module: 05-compose
Prerequisites: 05-compose/01-compose-basics.md, 05-compose/03-compose-file-reference.md
Difficulty: Advanced
Estimated Duration: 50 minutes
Target Docker Version: 27.x+ (Compose Specification v2.x+)
Last Validated: February 2025
```

---

## Table of Contents
1. [What & Why](#1-what--why)
2. [Core Concepts](#2-core-concepts)
3. [Compose Profiles: Selective Service Activation](#3-compose-profiles-selective-service-activation)
4. [Multi-File Overrides: Environment-Specific Layering](#4-multi-file-overrides-environment-specific-layering)
5. [The YAML Merging Algorithm (Rules of Precedence)](#5-the-yaml-merging-algorithm-rules-of-precedence)
6. [Service Reusability with `extends`](#6-service-reusability-with-extends)
7. [Syntax & Implementation Reference](#7-syntax--implementation-reference)
8. [Practical Implementation & Examples](#8-practical-implementation--examples)
9. [Production Best Practices & Antipatterns](#9-production-best-practices--antipatterns)
10. [Common Pitfalls & Troubleshooting](#10-common-pitfalls--troubleshooting)
11. [Performance Tuning & Resource Optimization](#11-performance-tuning--resource-optimization)
12. [Security Hardening & Threat Model](#12-security-hardening--threat-model)
13. [Architectural Comparison Table](#13-architectural-comparison-table)
14. [Real-World Scenario Walkthrough](#14-real-world-scenario-walkthrough)
15. [Self-Assessment & Interview Questions](#15-self-assessment--interview-questions)
16. [Hands-on Practice Exercises](#16-hands-on-practice-exercises)
17. [Summary & Reference Cheat Sheet](#summary--reference-cheat-sheet)

---

## 1. What & Why

Managing container configurations across different stages of the software delivery lifecycle (local developer laptops, automated CI pipelines, staging, production) often leads to duplication:
- Creating separate, standalone `compose-dev.yaml`, `compose-test.yaml`, and `compose-prod.yaml` files duplicates 80% of service declarations, making updates maintenance nightmares.
- Running heavy auxiliary services (monitoring stacks like Prometheus/Grafana, database admin GUIs like pgAdmin, or mocking proxies) on developer laptops wastes gigabytes of RAM when not actively in use.

Docker Compose provides two complementary architectural mechanisms to eliminate configuration duplication:
1. **Compose Profiles**: Assigns services to named profiles so they only boot when explicitly requested via CLI flags or environment variables.
2. **Multi-File Overrides**: Merges a base `compose.yaml` with environment-specific delta files (`compose.override.yaml`, `compose.prod.yaml`) to patch specific settings (ports, volumes, replicas).

---

## 2. Core Concepts

### Profiles (`profiles: [...]`)
By default, any service defined in a Compose file boots on `docker compose up`. Adding `profiles: ["monitoring"]` changes this:
- The service is **ignored** during standard `docker compose up`.
- It boots **only** when the profile is explicitly targeted (e.g. `docker compose --profile monitoring up -d`).

### Automatic vs Explicit Overrides
- **Automatic Override (`compose.override.yaml`)**: If a file named `compose.override.yaml` (or `docker-compose.override.yml`) exists in the project root, Compose **automatically merges it** on top of `compose.yaml` without requiring any CLI flags. Perfect for developer-specific local customizations.
- **Explicit Overrides (`-f file1 -f file2`)**: Compose merges multiple files in the exact order specified on the command line:
  `docker compose -f compose.yaml -f compose.prod.yaml up -d`

---

## 3. Compose Profiles: Selective Service Activation

```
+-------------------------------------------------------------------------+
|                  compose.yaml (Multi-Profile Architecture)              |
+-------------------------------------------------------------------------+
| [api] (No Profile)        | Runs by default in ALL environments        |
| [db]  (No Profile)        | Runs by default in ALL environments        |
| [redis] (No Profile)      | Runs by default in ALL environments        |
|---------------------------+---------------------------------------------|
| [pgadmin]     [debug]     | Runs ONLY with --profile debug              |
| [prometheus]  [metrics]   | Runs ONLY with --profile metrics            |
| [grafana]     [metrics]   | Runs ONLY with --profile metrics            |
| [seed-runner] [tools]     | Runs ONLY with --profile tools              |
+-------------------------------------------------------------------------+
```

### CLI Activation Methods
```bash
# Activate a single profile:
docker compose --profile debug up -d

# Activate multiple profiles:
docker compose --profile debug --profile metrics up -d

# Activate via environment variable:
export COMPOSE_PROFILES=metrics,debug
docker compose up -d
```

---

## 4. Multi-File Overrides: Environment-Specific Layering

### The Canonical File Hierarchy

```
               +-----------------------------------+
               |           compose.yaml            |
               | (Base Architecture: Shared by All)|
               +-----------------+-----------------+
                                 |
        +------------------------+------------------------+
        |                                                 |
        v                                                 v
+-------------------------------+ +-------------------------------+
|    compose.override.yaml      | |       compose.prod.yaml       |
|    (Local Development)        | |         (Production)          |
| - Bind mounts source code     | | - Pulls tagged registry image |
| - Maps public debug ports     | | - Enforces memory limits      |
| - Enables verbose logging     | | - Binds ports to localhost    |
+-------------------------------+ +-------------------------------+
```

---

## 5. The YAML Merging Algorithm (Rules of Precedence)

When Compose merges multiple YAML manifests, it follows strict structural rules:

1. **Scalars (Strings, Numbers, Booleans)**:
   - **Replaced**: The value from the later file replaces the value from the earlier file.
   - Example: `image: api:dev` overridden by `image: api:v1.2` becomes `image: api:v1.2`.
2. **Lists / Arrays (`ports:`, `volumes:`, `expose:`)**:
   - **Appended / Concatenated**: Items from the later file are added to the list from the earlier file.
   - Example: Base defines `ports: ["80:80"]`, override defines `ports: ["443:443"]` -> Result has both ports!
3. **Mappings / Dictionaries (`environment:`, `labels:`, `deploy:`)**:
   - **Merged Key-by-Key**: Keys from the later file are added. If a key already exists, its value is overridden.
   - Example: Base defines `NODE_ENV: dev`, override defines `NODE_ENV: prod` and `DEBUG: false` -> `NODE_ENV` is overridden to `prod` and `DEBUG` is added.

---

## 6. Service Reusability with `extends`

The `extends` directive allows a service to inherit common configurations from another service in the same file or a separate shared file.

```yaml
# common-services.yaml
services:
  base-app:
    restart: unless-stopped
    logging:
      driver: "json-file"
      options: { max-size: "10m", max-file: "3" }
    networks:
      - internal-net

# compose.yaml
services:
  api:
    extends:
      file: common-services.yaml
      service: base-app
    image: my-api:latest
    ports:
      - "8080:8080"
```

---

## 7. Syntax & Implementation Reference

### Profile Definition Syntax

```yaml
services:
  api:
    image: my-api:latest

  pgadmin:
    image: dpage/pgadmin4
    profiles:
      - debug
      - tools
    ports:
      - "5050:80"
    environment:
      PGADMIN_DEFAULT_EMAIL: admin@corp.local
      PGADMIN_DEFAULT_PASSWORD: admin

  jaeger:
    image: jaegertracing/all-in-one
    profiles:
      - tracing
    ports:
      - "16686:16686"
```

---

## 8. Practical Implementation & Examples

### Example 1: Local Development vs Production Multi-File Split

#### 1. Base Manifest (`compose.yaml`)
Contains structure, networks, and persistent volume definitions:
```yaml
# compose.yaml
services:
  web:
    image: mycorp/web-app
    restart: unless-stopped
    networks:
      - app-net
    depends_on:
      db:
        condition: service_healthy

  db:
    image: postgres:17-alpine
    environment:
      POSTGRES_DB: appdb
      POSTGRES_USER: appuser
    volumes:
      - pg_data:/var/lib/postgresql/data
    networks:
      - app-net
    healthcheck:
      test: ["CMD-SHELL", "pg_isready -U appuser -d appdb"]
      interval: 5s
      timeout: 3s
      retries: 5

networks:
  app-net:

volumes:
  pg_data:
```

#### 2. Local Dev Override (`compose.override.yaml` - Auto-loaded!)
Adds local source mounting and exposes database for host debugging:
```yaml
# compose.override.yaml
services:
  web:
    build: .
    ports:
      - "3000:3000"
    volumes:
      - .:/app
    environment:
      NODE_ENV: development
      DEBUG: "true"

  db:
    ports:
      - "127.0.0.1:5432:5432" # Allow developer to connect GUI to database
    environment:
      POSTGRES_PASSWORD: devpassword
```

#### 3. Production Override (`compose.prod.yaml`)
Enforces production image tags, CPU/RAM limits, and removes dev bind mounts:
```yaml
# compose.prod.yaml
services:
  web:
    image: mycorp/web-app:2.5.0
    ports:
      - "80:3000"
    environment:
      NODE_ENV: production
      POSTGRES_PASSWORD: ${DB_PASSWORD}
    deploy:
      resources:
        limits:
          cpus: '2.0'
          memory: 1024M

  db:
    environment:
      POSTGRES_PASSWORD: ${DB_PASSWORD}
    deploy:
      resources:
        limits:
          memory: 2048M
```

Execution Commands:
```bash
# In local development: Just run! (compose.override.yaml auto-merges!)
docker compose up -d

# In production deployment: Explicitly layer base + prod
docker compose -f compose.yaml -f compose.prod.yaml up -d
```

---

## 9. Production Best Practices & Antipatterns

### Best Practices
- **Commit `compose.yaml` and `compose.prod.yaml`**: Keep core architecture under Git version control.
- **Add `compose.override.yaml` to `.gitignore`**: Allow developers to create private, uncommitted local overrides (e.g. personal port mappings, local bind mounts) without polluting Git.
- **Group Heavy Diagnostics in Profiles**: Put administrative tools (Adminer, pgAdmin, Jaeger, Prometheus, Mockoon) under `profiles: ["debug"]` or `profiles: ["tools"]`.
- **Verify Merged Output via `docker compose config`**: Always inspect the final merged configuration (`docker compose -f base.yaml -f override.yaml config`) before deploying to production.

### Anti-Patterns
- **Duplicating Declarations Across Standalone Files**: Copy-pasting the full 100-line Compose file into `dev.yaml`, `test.yaml`, and `prod.yaml` ensures configurations diverge and break over time.
- **Relying on Automatic `compose.override.yaml` in Production**: In production, always pass explicit `-f` flags to prevent rogue local override files from altering production state.
- **Putting Core Services in Profiles**: A service that must run in all environments (like the database or primary API) should never be assigned a profile.

---

## 10. Common Pitfalls & Troubleshooting

| Symptom | Root Cause | Diagnosis Command | Permanent Fix |
|:---|:---|:---|:---|
| Service completely missing when running `docker compose up` | Service has a `profiles:` declaration and the profile was not targeted | Check service definition in YAML | Run with `--profile <name>` or `COMPOSE_PROFILES=<name>`. |
| Port mapping from base file still active despite override | List attributes (like `ports:`) are **appended**, not replaced! | `docker compose config` | Base file should omit port mappings, allowing override files to specify them. |
| Production deployment accidentally loads local dev bind mounts | `compose.override.yaml` was left in the production folder and auto-loaded | `docker compose config` | Pass explicit `-f compose.yaml -f compose.prod.yaml` or delete local override file. |
| `extends` fails with `cannot extend service with depends_on` | The Compose Specification prohibits inheriting `depends_on` or `links` via `extends` | Inspect error message | Define `depends_on` locally within each inheriting service. |
| Environment variables from override file not overriding base | Base file defined variable in inline `environment:`, which has equal precedence; order of files dictates winner | `docker compose config` | Ensure override file is passed **after** the base file in `-f` arguments. |
| Host port collision on multiple profiles | Profile services attempt to bind the same host port (e.g. two admin GUIs binding port 8080) | `docker compose ps` | Map to distinct host ports (e.g. 8081 vs 8082). |

---

## 11. Performance Tuning & Resource Optimization

1. **RAM Preservation on Developer Machines**:
   By assigning heavy telemetry and database GUI containers to `profiles: ["debug"]`, developers save 1-3 GB of host RAM during routine feature development, launching debugging tools only when troubleshooting.
2. **Deterministic CI Pipelines**:
   In automated testing, running `docker compose --profile test up --exit-code-from test-runner` spins up only the application, database, and test suite, keeping CI runner CPU and memory footprints minimal.

---

## 12. Security Hardening & Threat Model

```
+--------------------------------------------------------------+
| ATTACK: Exposure of Administrative GUIs in Production         |
+--------------------------------------------------------------+
| Scenario: Developer adds `adminer` or `pgadmin` to base      |
|           `compose.yaml` without a profile.                  |
| Production runs `docker compose up -d`.                      |
| Adminer boots on public port 8080 with default weak login.   |
| Attacker scans IP -> Gains full visual SQL database access!  |
|                                                              |
| PERMANENT MITIGATION:                                        |
| Tag all administrative utilities with `profiles: ["tools"]`. |
| Core production deployments will NEVER boot tools services.  |
+--------------------------------------------------------------+
```

---

## 13. Architectural Comparison Table

| Feature | Compose Profiles | Multi-File Overrides (`-f`) | `extends` Directive |
|:---|:---|:---|:---|
| **Primary Scope** | Within a single file | Across multiple files | Service-to-service |
| **Mechanism** | Conditional activation | Structural YAML merging | Object-oriented inheritance |
| **Best Used For** | Optional tools (pgAdmin, debug)| Dev vs Staging vs Prod | Sharing base logging/network configs |
| **CLI Flag** | `--profile <name>` | `-f <file1> -f <file2>` | None (Declared in YAML) |

---

## 14. Real-World Scenario Walkthrough

### Scenario: Preventing Local Dev Mounts from Poisoning Production

#### Incident
A deployment to an AWS EC2 production instance used `docker compose up -d`. The application crashed immediately with:
`Error: Cannot find module '/app/src/server.js'`.
The developers were baffled because the production Docker image was fully tested in CI.

#### Root Cause Analysis
The team had stored local developer settings inside `compose.override.yaml` (which mounted `./src:/app/src`). An engineer had accidentally copied the entire repository directory—including `compose.override.yaml`—to the production server.
Because `docker compose up` automatically merges `compose.override.yaml` by default, Docker mounted the production host's empty source directory over the container's pre-compiled `/app/src` directory!

#### Remediation
1. Deleted `compose.override.yaml` from production and added it to `.gitignore`.
2. Updated the production deployment automation script to always use explicit file targeting:
   ```bash
   docker compose -f compose.yaml -f compose.prod.yaml up -d
   ```
3. Verified via `docker compose -f compose.yaml -f compose.prod.yaml config` that zero bind mounts existed in production.

---

## 15. Self-Assessment & Interview Questions

<details>
<summary><strong>1. What happens when a service defines `profiles: ["debug"]` in `compose.yaml` and you run standard `docker compose up`?</strong></summary>

**Answer:**
The service will be completely ignored and will not start. To start a service assigned to a profile, you must explicitly enable the profile using the `--profile debug` CLI flag or set the `COMPOSE_PROFILES=debug` environment variable.
</details>

<details>
<summary><strong>2. How does Docker Compose resolve list attributes (such as `ports:` or `volumes:`) when merging two Compose files?</strong></summary>

**Answer:**
List attributes are **appended (concatenated)**, not replaced. If `compose.yaml` defines `ports: ["80:80"]` and `compose.override.yaml` defines `ports: ["443:443"]`, the merged configuration contains both mappings: `["80:80", "443:443"]`. To prevent unwanted ports in production, omit the port mapping from the base file and define it exclusively in the environment-specific override file.
</details>

<details>
<summary><strong>3. How does the default `compose.override.yaml` file behave, and what is its primary use case?</strong></summary>

**Answer:**
If a file named `compose.override.yaml` (or `docker-compose.override.yml`) exists in the project root, Compose automatically merges it on top of `compose.yaml` whenever any `docker compose` command is run without explicit `-f` flags. Its primary use case is local development customization (such as bind mounting local source code, adding debug environment variables, or mapping local ports) without modifying the base manifest.
</details>

<details>
<summary><strong>4. How do you merge three Compose files in a specific order using the CLI?</strong></summary>

**Answer:**
Pass multiple `-f` flags in the desired order of application:
`docker compose -f base.yaml -f overrides.yaml -f local.yaml up -d`
Compose parses `base.yaml`, applies `overrides.yaml` on top, and finally applies `local.yaml` as the highest-precedence layer.
</details>

<details>
<summary><strong>5. Can a service belong to multiple profiles simultaneously?</strong></summary>

**Answer:**
Yes. A service can declare an array of profiles:
```yaml
profiles: ["debug", "tools", "admin"]
```
The service will boot if *any* of those profiles are active (e.g. `--profile tools`).
</details>

<details>
<summary><strong>6. What limitations exist when using the `extends` directive in Compose?</strong></summary>

**Answer:**
Under the Compose Specification, a service cannot inherit `depends_on` or `links` from another service using `extends`. This prevents circular dependency bugs and complex graph inheritance conflicts across multiple files. Dependency links must always be declared directly within the concrete service definition.
</details>

<details>
<summary><strong>7. How can you preview the final merged YAML document resulting from multiple Compose files and active profiles?</strong></summary>

**Answer:**
Execute `docker compose -f file1.yaml -f file2.yaml --profile <name> config`. This validates the configuration and prints the fully resolved, merged YAML document to stdout.
</details>

<details>
<summary><strong>8. Why should `compose.override.yaml` be added to `.gitignore` in enterprise teams?</strong></summary>

**Answer:**
Because `compose.override.yaml` is designed for developer-specific local customizations (such as personal directory paths, alternative host port bindings, and local secrets). If committed to Git, developers will continuously overwrite each other's local development settings.
</details>

---

## 16. Hands-on Practice Exercises

<details>
<summary><strong>Exercise 1: Implementing a Debug Profile for Redis Commander</strong></summary>

**Goal:** Create a Redis service that runs by default, and a Redis GUI that runs only with `--profile debug`.

**Step-by-step Solution:**
1. Create `compose.yaml`:
   ```yaml
   services:
     cache:
       image: redis:alpine
     cache-gui:
       image: rediscommander/redis-commander:latest
       profiles: [debug]
       ports: ["8081:8081"]
       environment:
         - REDIS_HOSTS=local:cache:6379
   ```
2. Run standard: `docker compose up -d` -> Verify only `cache` is running (`docker compose ps`).
3. Run with profile: `docker compose --profile debug up -d` -> Verify both `cache` and `cache-gui` are running!
4. Clean up: `docker compose --profile debug down`.
</details>

<details>
<summary><strong>Exercise 2: Multi-File Merging Verification</strong></summary>

**Goal:** Merge a base file with a production override and verify using `docker compose config`.

**Step-by-step Solution:**
1. Create `base.yaml`:
   ```yaml
   services:
     api:
       image: my-api:latest
   ```
2. Create `prod.yaml`:
   ```yaml
   services:
     api:
       environment:
         - NODE_ENV=production
   ```
3. Inspect merged output:
   ```bash
   docker compose -f base.yaml -f prod.yaml config
   ```
4. Verify `NODE_ENV: production` is merged into the `api` service.
</details>

<details>
<summary><strong>Exercise 3: Observing Port Appending Behavior</strong></summary>

**Goal:** Observe that list items append rather than overwrite.

**Step-by-step Solution:**
1. Base file with port 80; override file with port 8080.
2. Run `docker compose config` and observe `ports` list contains both `80` and `8080`.
</details>

<details>
<summary><strong>Exercise 4: Using `extends` for Standard Logging</strong></summary>

**Goal:** Reuse logging configuration across two services using `extends`.

**Step-by-step Solution:**
1. Author `common.yaml` and reference it from `compose.yaml` using `extends`.
2. Verify with `docker compose config`.
</details>

<details>
<summary><strong>Exercise 5: Activating Profiles via Environment Variable</strong></summary>

**Goal:** Activate the `tools` profile using `COMPOSE_PROFILES` without CLI flags.

**Step-by-step Solution:**
1. Export variable:
   ```bash
   export COMPOSE_PROFILES=debug
   docker compose ps # Shows profile services
   ```
</details>

<details>
<summary><strong>Exercise 6: Preventing Accidental Production Override Loading</strong></summary>

**Goal:** Create a production script that ignores any local `compose.override.yaml`.

**Step-by-step Solution:**
1. Create script:
   ```bash
   docker compose -f compose.yaml -f compose.prod.yaml up -d
   ```
2. Verify `compose.override.yaml` is completely bypassed.
</details>

---

## 17. Summary & Reference Cheat Sheet

### CLI Quick Reference
```bash
# Profiles:
docker compose --profile <name> up -d
export COMPOSE_PROFILES=profile1,profile2

# Overrides:
docker compose -f compose.yaml -f compose.prod.yaml up -d
docker compose -f compose.yaml -f compose.prod.yaml config
```

### Merging Rules:
- **Scalars** (images, commands): **Replaced** by later files.
- **Lists** (ports, volumes): **Appended** (both entries remain).
- **Mappings** (environment, labels): **Merged** key-by-key.

---

[← Previous: 05-depends-on-and-healthchecks.md](05-depends-on-and-healthchecks.md) | [Next: 07-compose-watch-and-dev-workflows.md →](07-compose-watch-and-dev-workflows.md)
