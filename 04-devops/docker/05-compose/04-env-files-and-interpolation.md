# Environment Variables, Precedence Hierarchy, and String Interpolation

```
Track: DevOps & Containerization
Module: 05-compose
Prerequisites: 05-compose/01-compose-basics.md, 05-compose/03-compose-file-reference.md
Difficulty: Intermediate to Advanced
Estimated Duration: 45 minutes
Target Docker Version: 27.x+ (Compose Specification v2.x+)
Last Validated: February 2025
```

---

## Table of Contents
1. [What & Why](#1-what--why)
2. [Core Concepts](#2-core-concepts)
3. [The Two Environment Domains: Host vs Container](#3-the-two-environment-domains-host-vs-container)
4. [The Strict Precedence Hierarchy (Evaluation Order)](#4-the-strict-precedence-hierarchy-evaluation-order)
5. [Interpolation Syntax Reference](#5-interpolation-syntax-reference)
6. [Syntax & Configuration Reference](#6-syntax--configuration-reference)
7. [Practical Implementation & Examples](#7-practical-implementation--examples)
8. [Production Best Practices & Antipatterns](#8-production-best-practices--antipatterns)
9. [Common Pitfalls & Troubleshooting](#9-common-pitfalls--troubleshooting)
10. [Performance Tuning & Resource Optimization](#10-performance-tuning--resource-optimization)
11. [Security Hardening & Threat Model](#11-security-hardening--threat-model)
12. [Architectural Comparison Table](#12-architectural-comparison-table)
13. [Real-World Scenario Walkthrough](#13-real-world-scenario-walkthrough)
14. [Self-Assessment & Interview Questions](#14-self-assessment--interview-questions)
15. [Hands-on Practice Exercises](#15-hands-on-practice-exercises)
16. [Summary & Reference Cheat Sheet](#summary--reference-cheat-sheet)

---

## 1. What & Why

In modern 12-factor application architecture, configuration must be strictly separated from code. Hardcoding configuration values (ports, database credentials, third-party API keys) inside Dockerfiles or Compose manifests breaks environment portability and creates severe security hazards.

Docker Compose provides a dual-tiered environment engine:
1. **Manifest Interpolation**: Replacing variables inside `compose.yaml` (e.g. `image: myapp:${APP_VERSION}`, `ports: - "${PORT}:80"`).
2. **Container Injection**: Passing runtime environment variables into the process running inside the container (e.g. `NODE_ENV=production`).

Understanding the **strict precedence hierarchy** across shell variables, `.env` files, CLI flags, `env_file`, and inline `environment` definitions is essential to prevent unintended configuration drift between development, staging, and production environments.

---

## 2. Core Concepts

### Interpolation vs Container Environment
- **Interpolation Variable**: Evaluated on the **host machine** by the Docker Compose parser *before* communicating with the Docker daemon. Used to dynamically set image tags, port numbers, volume paths, or service names in the YAML manifest.
- **In-Container Environment Variable**: Injected directly into the container's process environment table. The containerized application reads it at runtime via `process.env` (Node), `os.environ` (Python), or `System.getenv()` (Java).

### The Default `.env` File
When `docker compose` executes, it automatically looks for a file named `.env` in the current working directory.
- Variables in `.env` are used **primarily for YAML string interpolation**.
- **Crucial Rule**: Variables in `.env` are **not** automatically injected into container runtimes unless explicitly referenced in `environment:` or `env_file:`.

---

## 3. The Two Environment Domains: Host vs Container

```
+-------------------------------------------------------------------------+
|                              HOST SYSTEM                                |
|                                                                         |
|  1. Host Shell Variables ($APP_VERSION, $PORT)                          |
|  2. Default Project .env File (TAG=2.1, DB_PORT=5432)                   |
|                                                                         |
|                               v (Interpolation)                         |
|  +-------------------------------------------------------------------+  |
|  | Compose Manifest (compose.yaml):                                  |  |
|  |   services:                                                       |  |
|  |     api:                                                          |  |
|  |       image: my-api:${TAG}      <--- Resolves to: my-api:2.1     |  |
|  |       ports:                                                      |  |
|  |         - "${DB_PORT}:80"       <--- Resolves to: 5432:80        |  |
|  |       environment:                                                |  |
|  |         - NODE_ENV=production   <--- IN-CONTAINER RUNTIME INJECTION|
|  +---------------------------------+---------------------------------+  |
+------------------------------------|------------------------------------+
                                     |
                                     v
+-------------------------------------------------------------------------+
|                           CONTAINER PROCESS                             |
|                                                                         |
|  Environment Table:                                                     |
|    NODE_ENV=production                                                  |
|    (Notice: $TAG and $DB_PORT are NOT present unless mapped!)           |
+-------------------------------------------------------------------------+
```

---

## 4. The Strict Precedence Hierarchy (Evaluation Order)

When the exact same variable name (e.g., `DATABASE_URL`) is defined in multiple locations, Docker Compose resolves the final value according to this strict descending order (Highest priority wins):

```
+-------------------------------------------------------------------------+
|               HIGHEST PRIORITY (Overrides all below)                    |
+-------------------------------------------------------------------------+
| 1. Explicit CLI Flags: docker compose run -e VAR=val ...                |
+-------------------------------------------------------------------------+
| 2. Host Shell Environment: export VAR=val (or in current terminal)      |
+-------------------------------------------------------------------------+
| 3. Inline YAML `environment:` Key in compose.yaml                       |
+-------------------------------------------------------------------------+
| 4. External `env_file:` Directives in compose.yaml                      |
+-------------------------------------------------------------------------+
| 5. Project Root `.env` File (Default interpolation file)                |
+-------------------------------------------------------------------------+
| 6. Dockerfile `ENV` Directives (Bake into base image)                   |
+-------------------------------------------------------------------------+
| 7. Fallback Default in Interpolation Syntax: ${VAR:-default}            |
+-------------------------------------------------------------------------+
|               LOWEST PRIORITY                                           |
+-------------------------------------------------------------------------+
```

---

## 5. Interpolation Syntax Reference

Docker Compose supports POSIX shell variable expansion syntax:

| Syntax | Behavior if `VAR` is Set & Non-Empty | Behavior if `VAR` is Unset | Behavior if `VAR` is Set to Empty (`""`) |
|:---|:---|:---|:---|
| **`${VAR}`** | Value of `VAR` | Blank (`""`) + Warning | Blank (`""`) |
| **`${VAR:-default}`**| Value of `VAR` | `default` | `default` |
| **`${VAR-default}`** | Value of `VAR` | `default` | Blank (`""`) |
| **`${VAR:?error}`** | Value of `VAR` | **Exits with custom error** | **Exits with custom error** |
| **`${VAR?error}`** | Value of `VAR` | **Exits with custom error** | Blank (`""`) |
| **`${VAR:+alt}`** | `alt` | Blank (`""`) | Blank (`""`) |
| **`$$VAR`** | Escapes literal `$VAR` (passed unexpanded to container) | N/A | N/A |

---

## 6. Syntax & Configuration Reference

### YAML Variable Interpolation Examples

```yaml
services:
  web:
    # Fails build immediately with error message if $APP_VERSION is unset
    image: mycorp/web:${APP_VERSION:?Error: APP_VERSION must be provided}

    # Uses default port 8080 if $HOST_PORT is empty or unset
    ports:
      - "${HOST_PORT:-8080}:80"

    # Injects multiple external environment files (later files override earlier)
    env_file:
      - .env.base
      - .env.${APP_ENV:-development}

    # Inline overrides: Overrides anything set in env_file!
    environment:
      DATABASE_HOST: db
      # Literal dollar sign escaping for nginx/php/bash inside container
      NGINX_TEMPLATE: "$$host:$$port"
```

---

## 7. Practical Implementation & Examples

### Example 1: Multi-Environment Swapping via `--env-file`
Switching between staging and production environments using external files without modifying `compose.yaml`.

```bash
# 1. Staging deployment:
docker compose --env-file .env.staging up -d

# 2. Production deployment:
docker compose --env-file .env.production up -d
```

Where `.env.staging` contains:
```ini
APP_ENV=staging
IMAGE_TAG=2.4.0-rc1
REPLICAS=1
DB_PORT=5433
```
And `.env.production` contains:
```ini
APP_ENV=production
IMAGE_TAG=2.4.0
REPLICAS=5
DB_PORT=5432
```

### Example 2: Strict Mandatory Variable Validation
Ensuring that critical database passwords are never silently omitted during automated CI/CD deployments.

```yaml
services:
  db:
    image: postgres:17-alpine
    environment:
      POSTGRES_DB: production_db
      POSTGRES_USER: db_admin
      # If DB_PASSWORD is not set in host shell or .env, Compose aborts execution!
      POSTGRES_PASSWORD: ${DB_PASSWORD:?FATAL: DB_PASSWORD environment variable is required!}
```

### Example 3: Escaping Literal Dollar Signs (`$$`)
Preventing Compose from corrupting shell commands or template strings that contain `$` symbols.

```yaml
services:
  cron:
    image: alpine
    command:
      # Use $$ to prevent Compose from trying to interpolate $DATE on host!
      - "sh"
      - "-c"
      - "DATE=$$(date +%Y%m%d) && echo Today is $$DATE"
```

### Example 4: Passing Host Environment Variables Directly Without Values
If a variable exists in the host terminal, you can pass it into the container without repeating its value:

```yaml
services:
  worker:
    image: worker:latest
    environment:
      # If AWS_SECRET_ACCESS_KEY exists in host shell, it is passed directly into container:
      - AWS_SECRET_ACCESS_KEY
      - AWS_REGION
```

### Example 5: Inspecting Resolved Environment Values
Verify how Compose resolved all variables before launching containers:

```bash
docker compose config
```
This prints the fully evaluated, interpolated YAML document, showing the exact values assigned to every parameter.

---

## 8. Production Best Practices & Antipatterns

### Best Practices
- **Commit `.env.example`, Never `.env`**: Always track `.env.example` (containing empty keys and documentation) in Git. Add `.env` and `.env.*` to `.gitignore` to prevent credential leaks.
- **Use Mandatory Interpolation for Secrets**: Always use `${VAR:?error}` for mandatory passwords and API tokens.
- **Escape Literal Dollar Signs with `$$`**: Whenever Nginx variables (`$host`), shell variables (`$PATH`), or PHP variables appear in `compose.yaml`, double the dollar sign (`$$host`).
- **Validate with `docker compose config`**: Always validate merged environment states in CI/CD before initiating deployments.

### Anti-Patterns
- **Assuming `.env` Injects Variables into Containers**: Placing `API_KEY=xyz` in `.env` does **not** expose it inside your container unless declared in `environment:` or `env_file:`.
- **Relying on Unpredictable Shell Inheritance**: Depending on developers having `export DB_URL=...` in their local `.bashrc` leads to "works on my machine" bugs. Standardize on `.env` files.
- **Stashing Passwords in Git Commits**: Never commit `.env` files containing production secrets.

---

## 9. Common Pitfalls & Troubleshooting

| Symptom | Root Cause | Diagnosis Command | Permanent Fix |
|:---|:---|:---|:---|
| `the attribute 'APP_TAG' is not set` warning | Variable missing from host shell and `.env` file | `docker compose config` | Supply default value: `${APP_TAG:-latest}` or add to `.env`. |
| Variable set in `.env` is `undefined` inside container | `.env` variables are only used for YAML interpolation, not automatic container injection | `docker compose exec api env` | Add variable to `environment:` or `env_file:` in YAML. |
| Host shell variable unexpectedly overrides `.env` file | Host shell (`export VAR=...`) has higher precedence than `.env` file | In host: `echo $VAR` | Unset host variable (`unset VAR`) or run `VAR= docker compose up`. |
| Nginx container crashes with `unknown directive "$host"` | Compose parsed `$host` during interpolation and replaced it with blank | Check `docker compose config` | Escape as `$$host` in `compose.yaml`. |
| `env_file` values overridden unexpectedly | Inline `environment:` keys always supersede `env_file` values | Inspect `compose.yaml` | Remove duplicate keys from inline `environment:`. |
| Multiline string in `.env` causes syntax error | Compose `.env` parser requires quotes for multiline values | Inspect `.env` file | Enclose multiline values (like private keys) in double quotes: `"-----BEGIN...-----"`. |

---

## 10. Performance Tuning & Resource Optimization

1. **Lightweight Env Evaluation**:
   Environment variable interpolation in Compose v2 occurs in Go compiled code, completing in < 5ms. However, avoid gigantic `.env` files with thousands of lines, as every string is parsed during manifest validation.
2. **Minimize Process Environment Pollution**:
   Do not dump 500 configuration variables into a container's environment table. Use structured configuration files mounted via `configs:` or volumes for large configuration sets.

---

## 11. Security Hardening & Threat Model

```
+--------------------------------------------------------------+
| ATTACK SCENARIO: Accidental Git Commit of Production Secrets |
+--------------------------------------------------------------+
| Developer authors `.env` containing AWS and DB root secrets. |
| Developer runs `git add . && git commit -m "Update config"`. |
| Repo pushed to GitHub / GitLab public or shared repository.  |
| Automated bots scan GitHub commits -> Credentials leaked!    |
|                                                              |
| PERMANENT DEFENSE:                                           |
| 1. Add `*.env` and `*.env.*` to `.gitignore`.                |
| 2. Commit strictly `.env.example` with dummy values.         |
| 3. Use pre-commit hooks (e.g. `gitleaks`) to block commits.  |
+--------------------------------------------------------------+
```

---

## 12. Architectural Comparison Table

| Mechanism | Scope | Primary Purpose | Git Tracked? | Precedence Rank |
|:---|:---|:---|:---|:---|
| **CLI `-e VAR=val`** | Container Runtime | Ad-hoc overrides | No | **#1 (Highest)** |
| **Host Shell (`export`)** | Interpolation & Container| CI/CD pipeline variables| No | **#2** |
| **Inline `environment:`** | Container Runtime | Explicit static config | **Yes** | **#3** |
| **`env_file:`** | Container Runtime | Tiered app config files | Config files only | **#4** |
| **Project `.env`** | YAML Interpolation Only | Manifest variables/tags | **NEVER** | **#5** |
| **Dockerfile `ENV`** | Image Default | Fallback runtime default| **Yes** | **#6 (Lowest)** |

---

## 13. Real-World Scenario Walkthrough

### Scenario: Fixing Unintended Production Database Overwrites

#### Incident
A DevOps engineer launched `docker compose up -d` on a production server. To their horror, the application connected to a staging database and wiped the staging tables!

#### Investigation
1. The `compose.yaml` had:
   ```yaml
   environment:
     - DATABASE_URL=${DATABASE_URL}
   ```
2. The production server had an accurate `.env` file:
   `DATABASE_URL=postgres://prod_user:secret@prod-db:5432/proddb`
3. However, two hours prior, the engineer had tested a staging script in their terminal:
   `export DATABASE_URL=postgres://staging_user:secret@staging-db:5432/stagingdb`
4. Because **Host Shell environment variables have higher precedence than `.env` files**, Compose evaluated `$DATABASE_URL` from the active terminal shell rather than the production `.env` file!

#### Permanent Solution
1. Explicitly unset shell overrides in deployment automation scripts:
   ```bash
   unset DATABASE_URL
   docker compose --env-file .env.production up -d
   ```
2. Implemented mandatory deployment validation scripts running `docker compose config` to verify target database endpoints prior to container recreation.

---

## 14. Self-Assessment & Interview Questions

<details>
<summary><strong>1. Does defining a variable in the project's root `.env` file automatically expose it inside a container's environment?</strong></summary>

**Answer:**
No. The default `.env` file is used exclusively by Docker Compose for **YAML string interpolation** on the host. To expose a variable from `.env` inside a container, it must be explicitly declared in the service's `environment:` key (e.g. `environment: - API_KEY=${API_KEY}`) or referenced via an `env_file:` directive.
</details>

<details>
<summary><strong>2. If a variable is defined in both the host shell (`export PORT=8000`) and the project `.env` file (`PORT=9000`), which value wins during interpolation?</strong></summary>

**Answer:**
The **host shell environment variable wins** (`8000`). Host shell variables have higher precedence than the default project `.env` file.
</details>

<details>
<summary><strong>3. What is the difference between `${VAR:-default}` and `${VAR-default}`?</strong></summary>

**Answer:**
- `${VAR:-default}`: Evaluates to `default` if `VAR` is **unset OR set to an empty string** (`""`).
- `${VAR-default}`: Evaluates to `default` **only if `VAR` is completely unset**. If `VAR` is set to an empty string (`VAR=""`), it evaluates to the empty string.
</details>

<details>
<summary><strong>4. How does `${VAR:?error_message}` improve CI/CD deployment safety?</strong></summary>

**Answer:**
It acts as a mandatory validation gate. If `VAR` is unset or empty when Compose parses the manifest, Compose aborts immediately with a non-zero exit code and displays the custom error message. This prevents containers from booting with missing secrets, unassigned database passwords, or empty image tags.
</details>

<details>
<summary><strong>5. How do you pass a literal `$` sign into a container command or configuration without triggering Compose interpolation?</strong></summary>

**Answer:**
Escape the dollar sign by doubling it: `$$`. For example, `command: ["sh", "-c", "echo $$PATH"]`. Compose interprets `$$` as a literal `$` and passes `$PATH` to the container shell.
</details>

<details>
<summary><strong>6. If a variable is declared in both `env_file:` and inline in `environment:`, which one takes priority?</strong></summary>

**Answer:**
The inline **`environment:`** definition takes priority and overrides the value defined in `env_file:`.
</details>

<details>
<summary><strong>7. How can you instruct Docker Compose to load a custom environment file instead of the default `.env`?</strong></summary>

**Answer:**
Use the `--env-file` CLI flag:
`docker compose --env-file .env.production up -d`
This replaces the default `.env` file with `.env.production` for manifest interpolation.
</details>

<details>
<summary><strong>8. What command outputs the fully resolved and interpolated Compose YAML configuration for verification?</strong></summary>

**Answer:**
`docker compose config`. It prints the fully merged YAML manifest with all environment variables interpolated, allowing engineers to audit configuration before deployment.
</details>

---

## 15. Hands-on Practice Exercises

<details>
<summary><strong>Exercise 1: Observing Host Shell vs `.env` Precedence</strong></summary>

**Goal:** Prove experimentally that host shell variables override `.env` files.

**Step-by-step Solution:**
1. Create `.env`:
   ```ini
   APP_PORT=9000
   ```
2. Create `compose.yaml`:
   ```yaml
   services:
     demo:
       image: alpine
       command: sh -c "echo Port is $$APP_PORT"
       environment:
         - APP_PORT=${APP_PORT}
   ```
3. Test `.env` default:
   ```bash
   docker compose run --rm demo
   # Outputs: Port is 9000
   ```
4. Override in host shell:
   ```bash
   APP_PORT=3333 docker compose run --rm demo
   # Outputs: Port is 3333 (Shell wins!)
   ```
</details>

<details>
<summary><strong>Exercise 2: Testing Mandatory Error Interpolation (`:?`)</strong></summary>

**Goal:** Configure a mandatory variable and observe build termination when omitted.

**Step-by-step Solution:**
1. Create `compose.yaml`:
   ```yaml
   services:
     secure-app:
       image: alpine
       command: echo ${REQUIRED_SECRET:?FATAL: REQUIRED_SECRET is missing!}
   ```
2. Run without variable:
   ```bash
   docker compose up
   # Throws fatal error and aborts with non-zero exit code!
   ```
3. Run with variable:
   ```bash
   REQUIRED_SECRET=mytoken docker compose up
   # Succeeds!
   ```
</details>

<details>
<summary><strong>Exercise 3: Resolving Literal Dollar Signs</strong></summary>

**Goal:** Pass an unexpanded `$HOSTNAME` to an Alpine subshell.

**Step-by-step Solution:**
1. Author manifest:
   ```yaml
   services:
     host-test:
       image: alpine
       command: sh -c "echo Hostname is $$HOSTNAME"
   ```
2. Run: `docker compose run --rm host-test`
3. Notice container outputs its internal container hostname rather than host machine hostname.
</details>

<details>
<summary><strong>Exercise 4: Tiered `env_file` Inheritance</strong></summary>

**Goal:** Configure two `.env` files where the second overrides keys from the first.

**Step-by-step Solution:**
1. Create `base.env`: `PORT=80` and `DEBUG=false`
2. Create `dev.env`: `DEBUG=true`
3. In `compose.yaml`:
   ```yaml
   services:
     app:
       image: alpine
       command: env
       env_file: [base.env, dev.env]
   ```
4. Verify `DEBUG=true` and `PORT=80` in `docker compose run --rm app`.
</details>

<details>
<summary><strong>Exercise 5: Auditing Interpolated Output via `docker compose config`</strong></summary>

**Goal:** Use `config` to catch unresolved variables.

**Step-by-step Solution:**
1. Run `docker compose config`
2. Inspect the printed YAML to ensure no unintended empty strings exist.
</details>

<details>
<summary><strong>Exercise 6: Sanitizing Repositories with `.gitignore`</strong></summary>

**Goal:** Author an enterprise `.gitignore` that prevents `.env` leaks while committing `.env.example`.

**Step-by-step Solution:**
1. Add to `.gitignore`:
   ```gitignore
   .env
   .env.*
   !.env.example
   ```
2. Commit `.env.example` with documented keys.
</details>

---

## 16. Summary & Reference Cheat Sheet

### Precedence Hierarchy Quick Reference
1. `docker compose run -e VAR=val` (CLI)
2. `export VAR=val` (Host Shell)
3. `environment:` in `compose.yaml` (Inline)
4. `env_file:` in `compose.yaml` (File)
5. `.env` in project directory (Root file)
6. `ENV` in Dockerfile (Image)

### Interpolation Syntax
- `${VAR:-default}`: Fallback if unset or empty
- `${VAR:?error}`: Fail loudly if unset or empty
- `$$VAR`: Literal dollar sign escaping

---

[← Previous: 03-compose-file-reference.md](03-compose-file-reference.md) | [Next: 05-depends-on-and-healthchecks.md →](05-depends-on-and-healthchecks.md)
