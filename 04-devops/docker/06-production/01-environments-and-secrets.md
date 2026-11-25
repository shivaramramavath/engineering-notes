# Environment Configuration and Production Secrets Management

```
Track: DevOps & Containerization
Module: 06-production
Prerequisites: 03-dockerfile/06-buildkit-and-buildx.md, 05-compose/04-env-files-and-interpolation.md
Difficulty: Advanced
Estimated Duration: 50 minutes
Target Docker Version: 27.x+
Last Validated: February 2025
```

---

## Table of Contents
1. [What & Why](#1-what--why)
2. [Core Concepts](#2-core-concepts)
3. [The Secrets Threat Model: Where Credentials Leak](#3-the-secrets-threat-model-where-credentials-leak)
4. [Build-Time Secrets vs Runtime Secrets](#4-build-time-secrets-vs-runtime-secrets)
5. [Syntax & Implementation Reference](#5-syntax--implementation-reference)
6. [Practical Implementation & Examples](#6-practical-implementation--examples)
7. [Production Best Practices & Antipatterns](#7-production-best-practices--antipatterns)
8. [Common Pitfalls & Troubleshooting](#8-common-pitfalls--troubleshooting)
9. [Performance Tuning & Resource Optimization](#9-performance-tuning--resource-optimization)
10. [Security Hardening & Threat Model](#10-security-hardening--threat-model)
11. [Architectural Comparison Table: Secrets Delivery Mechanisms](#11-architectural-comparison-table-secrets-delivery-mechanisms)
12. [Real-World Scenario Walkthrough](#12-real-world-scenario-walkthrough)
13. [Self-Assessment & Interview Questions](#13-self-assessment--interview-questions)
14. [Hands-on Practice Exercises](#14-hands-on-practice-exercises)
15. [Summary & Reference Cheat Sheet](#summary--reference-cheat-sheet)

---

## 1. What & Why

In container engineering, **secrets** (database passwords, private API keys, TLS private certificates, OAuth tokens, SSH deploy keys) require fundamentally different handling than non-sensitive configuration values (port numbers, log levels, hostnames).

Exposing secrets inside containers is the leading vector for supply chain breaches:
- **Image Metadata Leaks**: Baking credentials via `ENV` or `ARG` permanently commits them into image layers. Anyone who pulls the image or executes `docker history --no-trunc` can read the plaintext secrets.
- **Process Table & Inspection Leaks**: Passing secrets via runtime environment variables (`-e API_KEY=xyz`) exposes them in `docker inspect <id>`, host `/proc/<PID>/environ`, application crash dumps, and APM tracing logs.
- **Git Repo Exposure**: Committing `.env` files to source code repositories leaks secrets to anyone with read access.

This guide details enterprise architectures for delivering secrets safely at build-time (BuildKit secrets) and runtime (Docker Secrets, Vault, AWS Secrets Manager, and Tmpfs mounts).

---

## 2. Core Concepts

### Build-Time Secrets
Credentials needed *only* during the compilation/packaging phase (e.g., an NPM token to download private packages from Artifactory, or an SSH key to clone private GitHub dependencies).
- **Golden Rule**: Build-time secrets must **never be recorded** in any image layer, build cache, or image metadata.
- **Mechanism**: BuildKit secret mounts (`RUN --mount=type=secret`).

### Runtime Secrets
Credentials needed by the application *while running* (e.g., database connection credentials, Stripe API secret keys).
- **Golden Rule**: Runtime secrets must reside purely in **volatile memory** (`tmpfs` or `/run/secrets/`), never written to disk layers.
- **Mechanism**: Docker Secrets, in-memory volume mounts, or cloud KMS secret sidecars.

---

## 3. The Secrets Threat Model: Where Credentials Leak

```
+-------------------------------------------------------------------------+
|                  WHERE CONTAINER SECRETS ACCIDENTALLY LEAK               |
+-------------------------------------------------------------------------+
| Attack Surface          | How Secret is Exposed                         |
|-------------------------+-----------------------------------------------|
| 1. Dockerfile ENV       | Visible via `docker history --no-trunc`       |
| 2. Dockerfile ARG       | Recorded in image config metadata JSON        |
| 3. Docker CLI `-e`      | Visible via `docker inspect` & host `ps aux`  |
| 4. Linux Kernel ProcFS  | Readable in `/proc/<PID>/environ` by any root |
| 5. Application Crash    | Dumped to stdout/stderr in unhandled stacktrace|
| 6. Git Repository       | Committed in `.env` or configuration YAML     |
+-------------------------------------------------------------------------+
```

---

## 4. Build-Time Secrets vs Runtime Secrets

### Build-Time: BuildKit Secret Mounts
Unlike `ARG` or `COPY`, BuildKit's `--mount=type=secret` mounts the secret file as an in-memory temporary filesystem exclusively for the duration of that single `RUN` instruction:

```dockerfile
# syntax=docker/dockerfile:1.7
FROM node:22-alpine AS builder
WORKDIR /app
COPY package*.json ./

# The secret is available ONLY during 'npm ci'. It leaves 0 bytes in the image layer!
RUN --mount=type=secret,id=npmrc,target=/root/.npmrc \
    npm ci
```

### Runtime: The In-Memory File Convention (`/run/secrets/`)
Instead of passing `DATABASE_PASSWORD=secret`, applications should read secrets from files mounted at `/run/secrets/<secret_name>`:
- The file exists only in container RAM.
- It is invisible to `docker inspect`.
- It does not leak into child process environment tables.

---

## 5. Syntax & Implementation Reference

### CLI BuildKit Secret Syntax

```bash
# 1. Pass a local file as a secret during build
docker buildx build \
  --secret id=npmrc,src=$HOME/.npmrc \
  --secret id=aws_creds,src=./secrets/aws.txt \
  -t my-app:prod .

# 2. Pass environment variable as secret (Docker 24+)
export GITHUB_TOKEN="ghp_xxx"
docker buildx build \
  --secret id=gh_token,env=GITHUB_TOKEN \
  -t my-app:prod .
```

### Docker Compose Runtime Secrets Syntax

```yaml
services:
  api:
    image: my-api:latest
    secrets:
      - db_password
      - stripe_key
    environment:
      # Tell application where to read secrets:
      - DB_PASSWORD_FILE=/run/secrets/db_password
      - STRIPE_KEY_FILE=/run/secrets/stripe_key

secrets:
  db_password:
    file: ./secrets/db_password.txt
  stripe_key:
    file: ./secrets/stripe_key.txt
```

---

## 6. Practical Implementation & Examples

### Example 1: Cloning Private GitHub Repos via BuildKit SSH Forwarding
Building a container that requires cloning a private Git repository without copying SSH private keys into the Docker context.

```dockerfile
# syntax=docker/dockerfile:1.7
FROM golang:1.24-alpine AS builder

RUN apk add --no-cache git openssh-client

# Add github.com public key to known_hosts
RUN mkdir -p -m 0700 ~/.ssh && ssh-keyscan github.com >> ~/.ssh/known_hosts

WORKDIR /src
# Forward host's SSH agent directly into the RUN instruction
RUN --mount=type=ssh \
    git clone git@github.com:mycorp/private-core-library.git .

RUN go build -o /bin/app .
```

Build command forwarding host SSH credentials:
```bash
eval $(ssh-agent) && ssh-add ~/.ssh/id_ed25519
docker buildx build --ssh default .
```

### Example 2: Node.js / Python Dual-Source Secret Loader
Structuring application code to support both legacy environment variables (for local dev) and file-based secrets (for production).

**Python FastAPI Secret Loader (`config.py`):**
```python
import os
from pathlib import Path

def get_secret(secret_name: str, default: str = None) -> str:
    """
    Reads secret from /run/secrets/<secret_name> if present;
    otherwise falls back to environment variable <SECRET_NAME>.
    """
    secret_path = Path(f"/run/secrets/{secret_name}")
    if secret_path.is_file():
        return secret_path.read_text().strip()
    return os.environ.get(secret_name.upper(), default)

# Usage in production:
DB_PASSWORD = get_secret("db_password")
```

### Example 3: Injecting Secrets from AWS Secrets Manager / HashiCorp Vault via Init Script
Using an entrypoint script to fetch dynamic secrets into in-memory `/tmp` before launching PID 1.

```bash
#!/bin/sh
# entrypoint.sh - Runs inside container before application boot
set -e

if [ -n "$AWS_SECRET_ID" ]; then
    echo "Fetching secrets from AWS Secrets Manager..."
    # Retrieve secret payload directly into RAM
    SECRET_JSON=$(aws secretsmanager get-secret-value --secret-id "$AWS_SECRET_ID" --query SecretString --output text)
    export DB_PASSWORD=$(echo "$SECRET_JSON" | jq -r .password)
fi

# Execute main application process as unprivileged user
exec "$@"
```

### Example 4: Hardening PostgreSQL with `POSTGRES_PASSWORD_FILE`
Official database images (Postgres, MySQL, MariaDB, Redis) natively support reading passwords from files.

```yaml
services:
  db:
    image: postgres:17-alpine
    environment:
      POSTGRES_USER: appuser
      POSTGRES_DB: production
      # Natively supported: Instructs Postgres to read password from /run/secrets/
      POSTGRES_PASSWORD_FILE: /run/secrets/db_password
    secrets:
      - db_password

secrets:
  db_password:
    file: ./secrets/db_password.txt
```

---

## 7. Production Best Practices & Antipatterns

### Best Practices
- **Standardize on File-Based Secrets (`_FILE` Convention)**: Adopt the standard convention where application configs accept `*_FILE` paths (e.g. `DB_PASS_FILE=/run/secrets/db_pass`).
- **Always Use BuildKit Secret Mounts for Private Registries**: Never pass npm tokens, PyPI tokens, or Maven passwords as `ARG`.
- **Set Restrictive Permissions on Host Secret Files**: Ensure local secret files are restricted to root/admin ownership: `chmod 600 ./secrets/*`.
- **Rotate Secrets Dynamically**: Decouple secrets from image lifecycles; rotating a database password should only require updating the secret source and restarting the container.

### Anti-Patterns
- **Using `ARG` for Passwords**: Storing `ARG DB_PASS` embeds the secret directly into the OCI image manifest JSON, permanently readable by `docker history`.
- **Multi-Stage Build Secret Leak**: Running `COPY secret.txt .` in Stage 1 and deleting it in Stage 2. If Stage 1 is exported or cached, the secret is permanently stored in the builder cache.
- **Committing `.env` to Version Control**: Committing plaintext production credentials into Git repositories.

---

## 8. Common Pitfalls & Troubleshooting

| Symptom | Root Cause | Diagnosis Command | Permanent Fix |
|:---|:---|:---|:---|
| `docker history` reveals API key in plaintext | Secret was defined using `ENV` or `ARG` in Dockerfile | `docker history <image> --no-trunc` | Replace `ARG`/`ENV` with `RUN --mount=type=secret`. |
| Application crashes with `Permission Denied: /run/secrets/db_pass` | Container runs as non-root user (e.g. UID 10001), but secret file is owned by root with 0400 permissions | `ls -la /run/secrets/` inside container | In Compose, specify `uid: "10001"` and `mode: 0440` under the secret definition. |
| BuildKit secret mount fails with `secret not found: npmrc` | Secret was not passed in CLI build command | Inspect `docker buildx build` arguments | Add `--secret id=npmrc,src=...` to build command. |
| Host `/proc/<PID>/environ` exposes plaintext passwords | Credentials passed as raw environment variables (`-e PASS=...`) | On host: `cat /proc/<PID>/environ` | Migrate to file-based secrets in `/run/secrets/`. |
| Git repo scanner (TruffleHog/GitGuardian) flags secret in repo | Developer committed `.env` file containing live production API keys | `git log -S "password"` | Invalidate secret immediately, rotate credentials, and scrub Git history with `git-filter-repo`. |

---

## 9. Performance Tuning & Resource Optimization

1. **In-Memory Tmpfs Performance**:
   Reading secrets from `/run/secrets/` incurs zero disk I/O; the file resides purely in the Linux kernel VFS page cache.
2. **BuildKit Secret Caching**:
   BuildKit ensures that steps using `--mount=type=secret` can still be cached cleanly based on the cryptographic hash of the input files, without caching the secret content itself.

---

## 10. Security Hardening & Threat Model

```
+--------------------------------------------------------------+
| ATTACK: Extraction of Credentials via Container Core Dump    |
+--------------------------------------------------------------+
| An unhandled SIGSEGV occurs in application process.          |
| Linux kernel writes core dump to host disk.                  |
| Attacker extracts process environment strings from memory.   |
+--------------------------------------------------------------+
| VULNERABLE APPROACH (Environment Variables):                 |
| - `DATABASE_PASSWORD=supersecret` is resident in memory      |
|   environment table for process lifetime.                    |
| - Attacker extracts plaintext password from core dump!       |
+--------------------------------------------------------------+
| SECURE APPROACH (File-Based Secret):                         |
| - Application reads `/run/secrets/db_password` on startup,   |
|   connects to database, and clears the memory buffer.        |
| - Core dump contains ZERO persistent credential strings.     |
+--------------------------------------------------------------+
```

---

## 11. Architectural Comparison Table: Secrets Delivery Mechanisms

| Mechanism | Build vs Runtime | Leaks into Image? | Visible in `docker inspect`? | Visible in `/proc/environ`? | Security Rating |
|:---|:---|:---|:---|:---|:---|
| **Dockerfile `ENV`** | Build & Runtime | **YES (Permanent)** | **YES** | **YES** | **DANGEROUS (0/5)** |
| **Dockerfile `ARG`** | Build Time | **YES (In metadata)**| **YES** | No | **POOR (1/5)** |
| **CLI `-e VAR=val`** | Runtime | No | **YES** | **YES** | **FAIR (2/5)** |
| **Docker Compose `.env`**| Runtime | No | **YES** | **YES** | **FAIR (2/5)** |
| **BuildKit `--mount=secret`**| Build Time | **NO (Zero trace)** | No | No | **OPTIMAL (5/5)** |
| **Docker Secrets (`/run/secrets`)**| Runtime | **NO (RAM only)** | **NO** | **NO** | **OPTIMAL (5/5)** |

---

## 12. Real-World Scenario Walkthrough

### Scenario: Remediation of an NPM Private Package Token Leak

#### Incident
During a corporate SOC2 compliance audit, a security researcher ran:
`docker history --no-trunc mycompany/web-api:latest`
and extracted a valid GitHub Personal Access Token that had full read/write access to the company's internal private repositories.

#### Root Cause
The Dockerfile contained:
```dockerfile
ARG NPM_TOKEN
RUN echo "//npm.pkg.github.com/:_authToken=${NPM_TOKEN}" > ~/.npmrc && \
    npm ci && \
    rm ~/.npmrc
```
Even though `rm ~/.npmrc` was executed in the same instruction, the variable was declared as an `ARG NPM_TOKEN`. Docker recorded the value passed to `--build-arg NPM_TOKEN=ghp_xxx` in the image's root metadata!

#### Remediation
1. Revoked the compromised token immediately.
2. Refactored the Dockerfile to use BuildKit secret mounts:
   ```dockerfile
   # syntax=docker/dockerfile:1.7
   FROM node:22-alpine AS builder
   WORKDIR /app
   COPY package*.json ./
   RUN --mount=type=secret,id=npmrc,target=/root/.npmrc \
       npm ci
   ```
3. Verified via `docker history` that zero token strings existed in any image layer or metadata.

---

## 13. Self-Assessment & Interview Questions

<details>
<summary><strong>1. Why is `RUN echo $SECRET > file && rm file` still a security failure in standard Docker builds?</strong></summary>

**Answer:**
If `$SECRET` was defined via an `ARG` or `ENV` instruction, Docker stores the argument in the image configuration JSON metadata, making it visible to anyone running `docker history --no-trunc` or `docker inspect`. Furthermore, if the creation and deletion happen across different Dockerfile instructions, the file remains permanently preserved in the intermediate image layer.
</details>

<details>
<summary><strong>2. How does BuildKit's `RUN --mount=type=secret` guarantee that secrets are never written to image layers?</strong></summary>

**Answer:**
BuildKit mounts the secret into the container using an ephemeral, in-memory `tmpfs` filesystem that is mounted strictly for the execution of that specific `RUN` command. When the command finishes, BuildKit unmounts the filesystem before committing the layer snapshot to disk. The secret bytes are never recorded in the container layer or the image manifest.
</details>

<details>
<summary><strong>3. Why are file-based secrets (`/run/secrets/`) more secure than environment variables (`-e PASSWORD=xyz`) at runtime?</strong></summary>

**Answer:**
Environment variables are insecure because:
1. They are exposed in plaintext to anyone with Docker access via `docker inspect`.
2. They are readable on the host in `/proc/<PID>/environ`.
3. They are inherited by all child processes spawned by the container.
4. They frequently leak into error logs, crash reports, and APM performance monitoring tools.
File-based secrets reside strictly in volatile memory, avoiding all these exposure vectors.
</details>

<details>
<summary><strong>4. How do you configure Docker Compose to grant an unprivileged container user access to a mounted secret?</strong></summary>

**Answer:**
By default, Docker Compose mounts secrets with root ownership (`UID 0`) and read-only mode `0444`. You can configure explicit ownership and permissions in `compose.yaml`:
```yaml
secrets:
  my_secret:
    file: ./secret.txt
    uid: "10001"
    gid: "10001"
    mode: 0440
```
</details>

<details>
<summary><strong>5. What is the purpose of BuildKit SSH agent forwarding (`--mount=type=ssh`)?</strong></summary>

**Answer:**
It allows a Docker build process to communicate with the host machine's running SSH agent via a mounted Unix socket. This enables cloning private Git repositories or fetching private dependencies during image builds without copying SSH private key files into the build context or Docker image.
</details>

<details>
<summary><strong>6. How can an application developer support both local `.env` development and production `/run/secrets/` cleanly?</strong></summary>

**Answer:**
By writing a helper utility that first checks if the secret file exists at `/run/secrets/<name>`. If the file exists, it reads the secret from the file; if not, it falls back to checking the standard environment variable (e.g. `os.environ.get("DB_PASSWORD")`).
</details>

<details>
<summary><strong>7. What happens if a secret file referenced in `compose.yaml` does not exist on the host?</strong></summary>

**Answer:**
Docker Compose throws a validation error and refuses to start the stack:
`open ./secrets/db_password.txt: no such file or directory`.
This acts as a protective barrier, preventing containers from booting with missing credentials.
</details>

<details>
<summary><strong>8. Can an attacker read `/proc/<PID>/environ` of a container from inside another container?</strong></summary>

**Answer:**
No, because Linux PID namespaces isolate processes between containers. However, an attacker who gains root privileges on the **host machine** can inspect `/proc/<PID>/environ` of any container running on that host.
</details>

---

## 14. Hands-on Practice Exercises

<details>
<summary><strong>Exercise 1: Inspecting Plaintext Secret Leaks in Image History</strong></summary>

**Goal:** Build a container with an `ARG` secret and use `docker history` to extract it.

**Step-by-step Solution:**
1. Create `Dockerfile`:
   ```dockerfile
   FROM alpine:3.21
   ARG API_KEY=initial_secret
   RUN echo "Built with key"
   ```
2. Build with custom arg:
   ```bash
   docker build --build-arg API_KEY=leaked_secret_999 -t leak-demo .
   ```
3. Inspect history:
   ```bash
   docker history --no-trunc leak-demo
   # Observe 'leaked_secret_999' plainly visible in the build log!
   ```
</details>

<details>
<summary><strong>Exercise 2: Secure BuildKit Secret Mounting</strong></summary>

**Goal:** Pass a secret via BuildKit and verify it leaves zero trace in image history.

**Step-by-step Solution:**
1. Create secret file: `echo "secure_token_123" > secret.txt`
2. Create `Dockerfile`:
   ```dockerfile
   # syntax=docker/dockerfile:1.7
   FROM alpine:3.21
   RUN --mount=type=secret,id=token \
       cat /run/secrets/token > /tmp/used_token.txt && rm /tmp/used_token.txt
   ```
3. Build with BuildKit:
   ```bash
   docker buildx build --secret id=token,src=secret.txt -t secure-demo .
   ```
4. Verify with `docker history --no-trunc secure-demo` -> Zero trace of secret!
</details>

<details>
<summary><strong>Exercise 3: Mounting Compose Runtime Secrets</strong></summary>

**Goal:** Deploy a container in Compose reading credentials from `/run/secrets/`.

**Step-by-step Solution:**
1. Create `db_secret.txt`: `echo "prodpassword" > db_secret.txt`
2. Create `compose.yaml`:
   ```yaml
   services:
     app:
       image: alpine
       command: sh -c "cat /run/secrets/db_pass && sleep 3600"
       secrets: [db_pass]
   secrets:
     db_pass:
       file: ./db_secret.txt
   ```
3. Run: `docker compose up -d`
4. Verify secret in logs: `docker compose logs` -> Displays `prodpassword`.
</details>

<details>
<summary><strong>Exercise 4: Setting Permissions for Unprivileged Container Users</strong></summary>

**Goal:** Mount a secret accessible strictly to non-root UID 10001.

**Step-by-step Solution:**
1. Configure `uid: "10001"` and `mode: 0400` under secret in `compose.yaml`.
2. Launch non-root container and verify readability.
</details>

<details>
<summary><strong>Exercise 5: Inspecting Host `/proc/<pid>/environ`</strong></summary>

**Goal:** Observe environment variable leakage from the host perspective.

**Step-by-step Solution:**
1. Run: `docker run -d --name env-leak -e LEAKED_PASSWORD=mysecret alpine sleep 3600`
2. Extract PID: `PID=$(docker inspect --format '{{.State.Pid}}' env-leak)`
3. Inspect on host: `sudo tr '\0' '\n' < /proc/$PID/environ | grep LEAKED`
4. Clean up: `docker rm -f env-leak`.
</details>

<details>
<summary><strong>Exercise 6: Testing Dual-Mode Python Secret Loader</strong></summary>

**Goal:** Verify a Python script loads from file first, falling back to environment variable.

**Step-by-step Solution:**
1. Test using Python snippet from Section 6.2 with and without `/run/secrets/` file.
</details>

---

## 15. Summary & Reference Cheat Sheet

### Golden Rules of Container Secrets
1. **Never use `ENV` or `ARG` for secrets**: They leak permanently into image metadata.
2. **Use BuildKit secrets** (`--mount=type=secret`) for build-time credentials.
3. **Use `/run/secrets/`** for runtime credentials.
4. **Never commit `.env` files** to Git repositories.
5. **Set `chmod 600`** on host credential files.

---

[← Previous: Module 05 Docker Compose](../05-compose/README.md) | [Next: 02-image-optimisation.md →](02-image-optimisation.md)
