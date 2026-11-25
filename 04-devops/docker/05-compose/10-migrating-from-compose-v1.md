# Migrating from Docker Compose v1 to Compose v2

```
Track: DevOps & Containerization
Module: 05-compose
Prerequisites: 05-compose/01-compose-basics.md, 05-compose/03-compose-file-reference.md
Difficulty: Intermediate
Estimated Duration: 40 minutes
Target Docker Version: 27.x+ (Compose Specification v2.x+)
Last Validated: February 2025
```

---

## Table of Contents
1. [What & Why](#1-what--why)
2. [Core Concepts](#2-core-concepts)
3. [The End of Life (EOL) Timeline](#3-the-end-of-life-eol-timeline)
4. [Key Architectural Differences](#4-key-architectural-differences)
5. [The Container Naming Breaking Change (Hyphen vs Underscore)](#5-the-container-naming-breaking-change-hyphen-vs-underscore)
6. [Schema Evolution: What to Remove & What to Update](#6-schema-evolution-what-to-remove--what-to-update)
7. [Step-by-Step Enterprise Migration Playbook](#7-step-by-step-enterprise-migration-playbook)
8. [Production Best Practices & Antipatterns](#8-production-best-practices--antipatterns)
9. [Common Pitfalls & Troubleshooting](#9-common-pitfalls--troubleshooting)
10. [Performance Tuning & Resource Optimization](#10-performance-tuning--resource-optimization)
11. [Security Hardening & Threat Model](#11-security-hardening--threat-model)
12. [Architectural Comparison Table: v1 vs v2](#12-architectural-comparison-table-v1-vs-v2)
13. [Real-World Scenario Walkthrough](#13-real-world-scenario-walkthrough)
14. [Self-Assessment & Interview Questions](#14-self-assessment--interview-questions)
15. [Hands-on Practice Exercises](#15-hands-on-practice-exercises)
16. [Summary & Reference Cheat Sheet](#summary--reference-cheat-sheet)

---

## 1. What & Why

For nearly a decade, `docker-compose` (with a hyphen) was the standard orchestration tool for local container environments. Written in Python, it operated as an external CLI binary communicating with the Docker daemon.

In July 2023, Docker officially retired Compose v1. **Compose v1 reached End of Life (EOL)**, receiving zero security patches, bug fixes, or performance updates.

Compose v2 (`docker compose`, with a space) represents a ground-up rewrite in **Go**, fully integrated into the Docker CLI as a native plugin. Transitioning to v2 delivers:
- **Massive Performance Gains**: Up to 60% faster startup and build times due to native Go concurrency and direct BuildKit integration.
- **Modern Specification Support**: Access to modern features like Compose Watch, BuildKit secret mounts, and fine-grained healthcheck dependencies.
- **Unified CLI Tooling**: Consistent CLI flags, error formats, and authentication handling.

---

## 2. Core Concepts

### The CLI Shift
- **Legacy v1**: `docker-compose up -d` (Invokes `/usr/local/bin/docker-compose`, Python runtime).
- **Modern v2**: `docker compose up -d` (Invokes the native Go Docker CLI plugin `docker-compose` under `~/.docker/cli-plugins/`).

### The Filename Standard
While `docker-compose.yml` remains supported for backward compatibility, the official Compose Specification establishes **`compose.yaml`** (or `compose.yml`) as the preferred canonical filename.

### The Obsolete `version:` Attribute
In v1, developers were forced to declare schema version headers (e.g. `version: '3.8'`), which artificially gated access to features. In v2, the `version:` tag is completely obsolete and deprecated.

---

## 3. The End of Life (EOL) Timeline

```
+-------------------------------------------------------------------------+
|                  COMPOSE v1 RETIREMENT TIMELINE                         |
+-------------------------------------------------------------------------+
| April 2022  | Docker Compose v2 reaches General Availability (GA).       |
| October 2022| Docker Desktop transitions default to Compose v2.         |
| July 2023   | Docker Compose v1 REACHES OFFICIAL END-OF-LIFE (EOL).     |
|             | Zero security patches, CVE fixes, or updates provided.    |
| Present     | Compose v1 is completely unsupported. Migration is        |
|             | mandatory for production and enterprise compliance.       |
+-------------------------------------------------------------------------+
```

---

## 4. Key Architectural Differences

```
+-----------------------------------+   +-----------------------------------+
|         COMPOSE v1 (Legacy)       |   |         COMPOSE v2 (Modern)       |
+-----------------------------------+   +-----------------------------------+
| - Language: Python 3              |   | - Language: Go                    |
| - Architecture: Standalone CLI    |   | - Architecture: Native Docker CLI |
|   binary (/usr/bin/docker-compose)|   |   plugin (docker compose ...)     |
| - Concurrency: Sequential / Slow  |   | - Concurrency: High Go Goroutines |
| - Build Engine: Legacy Docker SDK |   | - Build Engine: Native BuildKit   |
| - Naming: project_service_1       |   | - Naming: project-service-1       |
| - File: docker-compose.yml        |   | - File: compose.yaml              |
+-----------------------------------+   +-----------------------------------+
```

---

## 5. The Container Naming Breaking Change (Hyphen vs Underscore)

The most significant breaking change between v1 and v2 is the default container naming separator:
- **Compose v1**: Used **underscores** -> `<project>_<service>_<index>` (e.g., `myapp_web_1`).
- **Compose v2**: Uses **hyphens** -> `<project>-<service>-<index>` (e.g., `myapp-web-1`).

### Why This Breaks Legacy Systems
Legacy automation scripts, monitoring tools, or Nginx configurations that hardcoded container names like `myapp_web_1` fail when migrated to v2.

### The Backward-Compatibility Fix
To force Compose v2 to retain legacy v1 underscore naming, use the `--compatibility` flag or set the environment variable:
```bash
# Via CLI flag:
docker compose --compatibility up -d

# Via environment variable:
export COMPOSE_COMPATIBILITY=true
docker compose up -d
```

---

## 6. Schema Evolution: What to Remove & What to Update

### 1. Remove Top-Level `version:`
```yaml
# DELETE THIS LINE in v2:
version: "3.8"

services:
  ...
```

### 2. Replace Deprecated Flags
- Remove legacy `--link` and replace with user-defined networks.
- Remove obsolete `scale:` within service blocks; use `deploy.replicas` or `docker compose up --scale service=N`.

### 3. Replace Legacy Healthcheck Syntax
Ensure healthchecks use modern JSON array syntax (`test: ["CMD", "curl", ...]`) rather than raw string shell commands.

---

## 7. Step-by-Step Enterprise Migration Playbook

### Phase 1: Audit and Inventory
1. Search all code repositories for legacy invocations:
   ```bash
   grep -rn "docker-compose" .
   ```
2. Search for references to legacy underscore container names:
   ```bash
   grep -rn "_1" .
   ```

### Phase 2: Update CI/CD Pipeline Scripts
1. Replace `docker-compose` with `docker compose`:
   ```bash
   # Old:
   docker-compose up -d
   docker-compose down

   # New:
   docker compose up -d
   docker compose down
   ```
2. Replace `docker-compose.yml` with `compose.yaml`:
   ```bash
   git mv docker-compose.yml compose.yaml
   ```

### Phase 3: Modernize Manifests
1. Delete `version: ...` lines from all Compose files.
2. Run validation check:
   ```bash
   docker compose config
   ```
3. Verify that BuildKit is enabled (`DOCKER_BUILDKIT=1`).

### Phase 4: Deploy and Verify
1. Deploy staging stack using Compose v2.
2. Confirm container discovery, networks, and persistent volume attachments.

---

## 8. Production Best Practices & Antipatterns

### Best Practices
- **Adopt `compose.yaml` as the Default Standard**: Use the new official standard filename.
- **Enable `COMPOSE_COMPATIBILITY=true` Temporarily if Needed**: Use compatibility mode during initial migration to avoid breaking legacy monitoring scripts.
- **Standardize on Modern `depends_on` Conditions**: Upgrade basic dependencies to `condition: service_healthy`.
- **Verify CI Runner Plugin Installation**: Ensure CI runners have the `docker-compose-plugin` package installed.

### Anti-Patterns
- **Creating Alias `alias docker-compose='docker compose'` Without Testing**: While aliases help local developers, hardcoded CI scripts without the alias will fail.
- **Mixing v1 and v2 on the Same Host**: Managing the same stack alternately with v1 and v2 causes container duplication due to the underscore vs hyphen naming discrepancy.
- **Keeping Dead `version:` Tags**: Keeping `version: '3.8'` generates continuous deprecation warnings in modern Docker CLI releases.

---

## 9. Common Pitfalls & Troubleshooting

| Symptom | Root Cause | Diagnosis Command | Permanent Fix |
|:---|:---|:---|:---|
| `docker-compose: command not found` | Compose v1 was uninstalled or omitted from newer OS distributions | `which docker-compose` | Switch command to `docker compose` (with space). |
| Existing containers recreated with new names (`-` instead of `_`) | Compose v2 changed container naming convention from underscore to hyphen | `docker ps` shows both sets of containers | Run `docker compose --compatibility up -d` or migrate external scripts. |
| Compose outputs `version is obsolete` warning | Top-level `version:` tag present in YAML manifest | Run `docker compose config` | Remove the `version:` line from `compose.yaml`. |
| CI build fails with `docker: 'compose' is not a docker command` | Host has old Docker Engine (< 20.10) or missing `docker-compose-plugin` package | `docker info` | Install plugin: `apt-get install docker-compose-plugin`. |
| Script parsing `docker-compose ps -q` fails | Output formatting differences between v1 and v2 | Inspect command output | Update script to use `docker compose ps -q`. |
| Compose v2 fails on legacy `scale:` attribute | `scale:` key inside service block was deprecated | Inspect YAML | Move scaling to `deploy.resources` or use `--scale service=N`. |

---

## 10. Performance Tuning & Resource Optimization

1. **Parallel Execution Engine**:
   Compose v2 leverages Go's native goroutines to execute image builds, network attachments, and volume provisions concurrently. In multi-service stacks (> 8 services), Compose v2 reduces startup latency by 40-60%.
2. **BuildKit Integration**:
   Compose v2 automatically builds custom images using BuildKit by default, enabling layer caching, secret mounts, and cache imports.

---

## 11. Security Hardening & Threat Model

```
+--------------------------------------------------------------+
| VULNERABILITY: Unpatched CVEs in Python Compose v1           |
+--------------------------------------------------------------+
| Compose v1 is built on Python 3 and unmaintained dependencies|
| (PyYAML, urllib3, requests).                                 |
| Because v1 reached EOL in July 2023:                         |
| - Zero security patches are released.                        |
| - High/Critical CVEs in Python libraries remain unpatched.   |
|                                                              |
| COMPLIANCE HAZARD: Fails SOC2, ISO27001, and FedRAMP audits!|
|                                                              |
| MITIGATION: IMMEDIATE MIGRATION TO GO-BASED COMPOSE v2.      |
+--------------------------------------------------------------+
```

---

## 12. Architectural Comparison Table: v1 vs v2

| Metric / Trait | Docker Compose v1 | Docker Compose v2 |
|:---|:---|:---|
| **Underlying Language** | Python | **Go** |
| **Maintenance Status** | **END OF LIFE (Dead)** | **Active / Supported** |
| **CLI Invocation** | `docker-compose` | `docker compose` |
| **Canonical File** | `docker-compose.yml` | `compose.yaml` |
| **Top-Level `version:`** | Mandatory (`version: '3.8'`) | **Obsolete (Omit completely)** |
| **Container Naming** | `project_service_1` | `project-service-1` |
| **BuildKit Support** | Optional (`COMPOSE_DOCKER_CLI_BUILD=1`)| **Default & Native** |
| **Compose Watch** | Not supported | **Supported (v2.22+)** |

---

## 13. Real-World Scenario Walkthrough

### Scenario: Migrating an Enterprise FinTech Monorepo from v1 to v2

#### Background
A payment processing platform maintained a repository with 42 microservices orchestrated locally by a 600-line `docker-compose.yml`. Upgrading to Ubuntu 24.04 broke their CI/CD pipelines because Ubuntu 24.04 completely dropped the legacy `docker-compose` Python package.

#### Migration Steps
1. **Installed Docker Compose Plugin**:
   ```bash
   sudo apt-get update && sudo apt-get install -y docker-compose-plugin
   ```
2. **Updated Canonical Manifest**:
   - Renamed `docker-compose.yml` to `compose.yaml`.
   - Removed `version: "3.9"`.
3. **Fixed Legacy Name Dependencies**:
   A shell script curled `myapp_api_1:3000`. Updated the script to use service DNS: `http://api:3000`.
4. **Updated CI Pipeline Configuration**:
   Changed all GitHub Actions workflow files from `docker-compose` to `docker compose`.

#### Outcome
- CI integration test run times dropped from **6 minutes 40 seconds** to **3 minutes 10 seconds** (over 50% speedup).
- Security scanner cleared all legacy Python CVE flags.

---

## 14. Self-Assessment & Interview Questions

<details>
<summary><strong>1. When did Docker Compose v1 reach official End of Life (EOL)?</strong></summary>

**Answer:**
Docker Compose v1 officially reached End of Life in **July 2023**. It is no longer supported and receives no security updates or bug fixes.
</details>

<details>
<summary><strong>2. What is the breaking change regarding container names between Compose v1 and v2?</strong></summary>

**Answer:**
Compose v1 separated project, service, and instance indices using **underscores** (e.g. `myapp_web_1`). Compose v2 changed the separator to **hyphens** (e.g. `myapp-web-1`). This breaks legacy scripts or reverse proxy configs that hardcode container names with underscores.
</details>

<details>
<summary><strong>3. How can you instruct Compose v2 to retain the legacy underscore naming convention?</strong></summary>

**Answer:**
Pass the `--compatibility` flag during CLI execution:
`docker compose --compatibility up -d`
Or set the environment variable:
`export COMPOSE_COMPATIBILITY=true`
</details>

<details>
<summary><strong>4. Why is the `version:` top-level attribute obsolete in modern Compose files?</strong></summary>

**Answer:**
Under the Compose Specification, Compose v2 unifies the legacy 2.x and 3.x schema formats into a single continuous specification. The top-level `version:` key is ignored by Compose v2 and triggers deprecation warnings; it should be omitted entirely from modern `compose.yaml` manifests.
</details>

<details>
<summary><strong>5. What is the preferred canonical filename for Compose files under the modern specification?</strong></summary>

**Answer:**
**`compose.yaml`** (or `compose.yml`). While `docker-compose.yml` is still supported for backward compatibility, `compose.yaml` is the official modern standard.
</details>

<details>
<summary><strong>6. What performance improvements does Compose v2 offer over Compose v1?</strong></summary>

**Answer:**
Compose v2 is written in Go and integrates directly into the Docker CLI as a native plugin. It executes operations concurrently using Go goroutines, integrates directly with BuildKit for parallel builds and advanced cache mounting, and avoids the memory footprint and Python interpreter startup overhead of Compose v1, resulting in 40-60% faster execution.
</details>

<details>
<summary><strong>7. How do you install Compose v2 on a Linux server that only has the Docker engine installed?</strong></summary>

**Answer:**
Install the official Docker repository plugin package:
`sudo apt-get install docker-compose-plugin`
Or download the static binary and place it in the Docker CLI plugins directory:
`~/.docker/cli-plugins/docker-compose` (with executable permissions `chmod +x`).
</details>

<details>
<summary><strong>8. Does Compose v2 support Docker Swarm deployments using `docker stack deploy`?</strong></summary>

**Answer:**
`docker stack deploy` continues to use the Compose file format for Swarm mode deployments, but it is an engine-level command (`docker stack`), not `docker compose`. Compose v2 focuses on single-node orchestration while remaining compatible with `deploy:` keys used by Swarm.
</details>

---

## 15. Hands-on Practice Exercises

<details>
<summary><strong>Exercise 1: Validating a Clean Modern `compose.yaml` without Version Header</strong></summary>

**Goal:** Create a Compose file omitting `version:`, and verify validation passes without warnings.

**Step-by-step Solution:**
1. Create `compose.yaml`:
   ```yaml
   services:
     hello:
       image: alpine
       command: echo "Modern Compose v2 active"
   ```
2. Validate:
   ```bash
   docker compose config
   ```
3. Verify output contains no deprecation warnings.
</details>

<details>
<summary><strong>Exercise 2: Testing Underscore Compatibility Mode</strong></summary>

**Goal:** Launch a container and verify that `--compatibility` generates underscores in container names.

**Step-by-step Solution:**
1. Run without compatibility:
   ```bash
   docker compose up -d
   docker compose ps
   # Container name has hyphen: <project>-hello-1
   docker compose down
   ```
2. Run with compatibility:
   ```bash
   docker compose --compatibility up -d
   docker compose ps
   # Container name has underscore: <project>_hello_1
   docker compose down
   ```
</details>

<details>
<summary><strong>Exercise 3: Renaming Legacy Files via Git</strong></summary>

**Goal:** Safely rename legacy `docker-compose.yml` to `compose.yaml` while preserving Git history.

**Step-by-step Solution:**
1. Execute:
   ```bash
   git mv docker-compose.yml compose.yaml
   git commit -m "chore: migrate to canonical compose.yaml filename"
   ```
</details>

<details>
<summary><strong>Exercise 4: Checking Compose Plugin Version via CLI</strong></summary>

**Goal:** Verify that your local environment is running Compose v2.

**Step-by-step Solution:**
1. Run:
   ```bash
   docker compose version
   # Example output: Docker Compose version v2.32.0
   ```
</details>

<details>
<summary><strong>Exercise 5: Converting Legacy Shell Script Invocations</strong></summary>

**Goal:** Search and replace `docker-compose` commands across bash scripts.

**Step-by-step Solution:**
1. Run:
   ```bash
   sed -i 's/docker-compose/docker compose/g' deploy.sh
   ```
</details>

<details>
<summary><strong>Exercise 6: Testing BuildKit Native Integration</strong></summary>

**Goal:** Build an image using Compose v2 and verify BuildKit output format.

**Step-by-step Solution:**
1. Run: `docker compose build --progress=plain`
2. Observe modern BuildKit build graph output.
</details>

---

## 16. Summary & Reference Cheat Sheet

### Top 5 Migration Rules
1. **Change CLI**: Replace `docker-compose` with `docker compose`.
2. **Rename Manifest**: Rename `docker-compose.yml` to `compose.yaml`.
3. **Delete `version:`**: Drop the obsolete `version: "3.8"` line.
4. **Use `--compatibility`**: If legacy scripts depend on underscore container names (`app_web_1`).
5. **Install Plugin**: Ensure CI runners have `docker-compose-plugin`.

---

[← Previous: 09-compose-in-production-limits.md](09-compose-in-production-limits.md) | [Next: Module 06 Production Engineering →](../06-production/README.md)
