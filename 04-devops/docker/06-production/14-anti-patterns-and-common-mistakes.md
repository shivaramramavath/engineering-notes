<!--
================================================================================
Metadata:
  Title: Docker Production Anti-Patterns and Common Mistakes
  Topic: Docker Production Engineering
  Difficulty: Intermediate to Advanced
  Estimated Time: 50 minutes
  Prerequisites: 03-dockerfile/01-dockerfile-basics.md, 04-volumes-and-networking/01-volumes-and-bind-mounts.md, 06-production/04-image-security.md
  Key Terminology: Anti-pattern, PID 1 Zombie Reaping, Root Daemon, Secret Baking, Ephemeral State, Writable Layer Bloat, Socket Mount Vulnerability
================================================================================
-->

# Docker Production Anti-Patterns and Common Mistakes

[<- 13-from-docker-to-kubernetes.md](./13-from-docker-to-kubernetes.md) | [Back to Module Overview](./README.md) | [Next Module: 07-projects/01-node-api-with-postgres-and-redis.md ->](../07-projects/01-node-api-with-postgres-and-redis.md)

---

## Table of Contents
- [1. What & Why](#1-what--why)
- [2. Taxonomy of Docker Anti-Patterns](#2-taxonomy-of-docker-anti-patterns)
- [3. Deep-Dive: The Top 20 Production Anti-Patterns](#3-deep-dive-the-top-20-production-anti-patterns)
- [4. Code Comparisons: Anti-Pattern vs. Production Standard](#4-code-comparisons-anti-pattern-vs-production-standard)
- [5. Practical Production Auditing](#5-practical-production-auditing)
- [6. Best Practices Checklist](#6-best-practices-checklist)
- [7. Common Mistakes & Troubleshooting](#7-common-mistakes--troubleshooting)
- [8. Performance, Security & Cost Implications](#8-performance-security--cost-implications)
- [9. Anti-Pattern Severity Matrix](#9-anti-pattern-severity-matrix)
- [10. Real-World Interview Questions](#10-real-world-interview-questions)
- [11. Practice Exercises](#11-practice-exercises)
- [12. Summary & Quick Reference Cheat Sheet](#12-summary--quick-reference-cheat-sheet)
- [13. Further Reading & Official Documentation](#13-further-reading--official-documentation)

---

## 1. What & Why

Docker's low barrier to entry makes it easy to assemble a functional container that runs smoothly on a developer workstation. However, development conveniences often become critical vulnerabilities and operational hazards when transitioned directly to production environments:
1. Containers running as `root` turn simple remote code execution (RCE) flaws into catastrophic host-level privilege escalation.
2. Shell form `ENTRYPOINT` strings swallow POSIX `SIGTERM` signals, forcing Docker to forcibly kill containers after 10-second timeouts and corrupting inflight database transactions.
3. Unrotated container stdout logs silently consume 100% of host filesystem storage.
4. Hardcoding secrets in intermediate build layers permanently exposes API tokens across public registries.

Cataloging and systematically eliminating these 20 anti-patterns is essential for building resilient, secure, and production-grade container infrastructure.

---

## 2. Taxonomy of Docker Anti-Patterns

```
                                  DOCKER ANTI-PATTERNS
                                           |
         +-----------------+---------------+----------------+-----------------+
         |                 |                                |                 |
         v                 v                                v                 v
    [ BUILD TIME ]    [ RUNTIME & LIFECYCLE ]          [ STORAGE & DATA ] [ SECURITY & NET ]
    - Root execution  - PID 1 signal swallower         - State in root fs - Socket exposure
    - Secret baking   - Missing resource limits        - Root mount leaks - Exposed ports
    - Latest tags     - Monolithic VM simulation       - Host ownership   - Host net abuse
    - Cache pollution - Unmonitored exit codes           mismatch         - Flat default net
    - Missing ignore  - Log overflow exhaustion
```

---

## 3. Deep-Dive: The Top 20 Production Anti-Patterns

### Category A: Image Build & Dockerfile Anti-Patterns

#### 1. Running as Root User (Default UID 0)
- **Problem**: Omitting `USER <non-root>` leaves container processes running as `root`. If an attacker exploits an application vulnerability (e.g., path traversal or remote shell), they run as root on the host kernel.
- **Remediation**: Explicitly create and switch to an unprivileged system user (`USER 10001:10001`).

#### 2. Using the `:latest` Floating Tag
- **Problem**: Deploying `:latest` makes builds and rollbacks completely non-deterministic. A rebuild pulls an untested upstream version without notice, breaking reproducible deployments.
- **Remediation**: Pin exact semantic versions (`node:20.12.2-alpine3.19`) or immutable image digests (`@sha256:...`).

#### 3. Baking Credentials and Secrets into Images
- **Problem**: Passing tokens via `ENV` or basic `ARG` stores plaintext credentials inside intermediate image layer metadata (`docker history`).
- **Remediation**: Use BuildKit secret mounts (`RUN --mount=type=secret,id=mysecret`).

#### 4. Package Manager Cache Retention
- **Problem**: Running `apt-get install` without cleaning `/var/lib/apt/lists/*` bloats the final image size by 100MB–300MB per layer.
- **Remediation**: Chain installation and cache removal in a single `RUN` instruction (`apt-get clean && rm -rf /var/lib/apt/lists/*`).

#### 5. Missing `.dockerignore` File
- **Problem**: `COPY . .` blindly uploads `.git` history, local `.env` files, SSH keys, build artifacts, and `node_modules` into the build context, dramatically slowing builds and leaking credentials.
- **Remediation**: Maintain a strict `.dockerignore` file matching `.gitignore` plus `.env*`, `.git`, and build caches.

#### 6. Using `ADD` Instead of `COPY`
- **Problem**: `ADD` silently extracts tarball archives and fetches remote URLs without hash verification.
- **Remediation**: Standardize on `COPY` for local files. If remote archives are needed, fetch and verify them explicitly using `curl` and `sha256sum` in a `RUN` instruction.

### Category B: Process Lifecycle & Runtime Anti-Patterns

#### 7. Swallowing POSIX Signals (Shell Form vs Exec Form)
- **Problem**: Writing `ENTRYPOINT "npm start"` runs the app as a child of `/bin/sh -c`. Shell processes do not forward `SIGTERM` to child processes. Docker waits 10 seconds, then forcibly kills the container with `SIGKILL` (exit code 137), corrupting state.
- **Remediation**: Always use JSON exec form: `ENTRYPOINT ["npm", "start"]` or use an init system like `tini` (`--init`).

#### 8. Treating Containers as Persistent Virtual Machines
- **Problem**: Running `sshd` inside containers, manually logging in with SSH to patch packages, and relying on in-place file edits.
- **Remediation**: Enforce container immutability. If code or dependencies change, build a new image and deploy a replacement container.

#### 9. Monolithic Multi-Process Containers
- **Problem**: Spawning NGINX, PHP-FPM, Redis, and Cron inside a single container using Supervisord. If one daemon fails, Docker cannot detect unhealthy state, and resource monitoring becomes impossible.
- **Remediation**: Decompose processes into dedicated single-purpose containers orchestrated via Docker Compose or Kubernetes.

#### 10. Omitting CPU and Memory Resource Limits
- **Problem**: Without `-m` and `--cpus`, a single runaway container with a memory leak exhausts host RAM, triggering the host Linux OOM killer to terminate critical host daemons.
- **Remediation**: Always define explicit resource limits (`limits.memory`, `limits.cpus`).

#### 11. Uncontrolled Container Logging (Disk Exhaustion)
- **Problem**: Container stdout/stderr defaults to unrestricted JSON files. High-throughput logs fill the host root disk partition, crashing the machine.
- **Remediation**: Enforce log rotation globally in `/etc/docker/daemon.json` (`max-size: 50m`, `max-file: 3`).

### Category C: Storage & Data Anti-Patterns

#### 12. Writing Persistent Application State to the Container Writable Layer
- **Problem**: Writing database data or uploaded files to `/var/lib/postgresql/data` inside the writable overlay layer degrades I/O performance and results in catastrophic data loss when the container is deleted.
- **Remediation**: Always mount dedicated Docker Volumes or external persistent storage for stateful directories.

#### 13. Hardcoded Absolute Host Bind Mounts
- **Problem**: Hardcoding `/home/john/data:/data` in production compose files breaks immediately when deployed across different developer machines or CI/CD worker nodes.
- **Remediation**: Use named Docker volumes (`data-volume:/data`) or project-relative paths (`./data:/data`).

#### 14. Host Volume File Permission Mismatches
- **Problem**: Container runs as unprivileged user `node` (UID 1000), but host directory is owned by `root` (UID 0), causing `EACCES: permission denied` crashes on file writes.
- **Remediation**: Explicitly align host directory permissions (`chown -R 1000:1000 ./data`) or manage ownership via entrypoint fix-up scripts.

### Category D: Security & Networking Anti-Patterns

#### 15. Mounting the Host Docker Socket (`/var/run/docker.sock`)
- **Problem**: Mounting `/var/run/docker.sock` into an untrusted application container gives that container root-level control over the host daemon, allowing full host root compromise.
- **Remediation**: Never mount the Docker socket into public-facing containers. Use rootless Docker or isolated CI agents.

#### 16. Indiscriminate Port Publishing (`-p 5432:5432`)
- **Problem**: Publishing internal database and cache ports to `0.0.0.0` exposes databases directly to the public internet, bypassing host firewalls because Docker injects iptables rules directly.
- **Remediation**: Never publish database ports publicly; communicate strictly across internal isolated user-defined Docker networks.

#### 17. Relying on the Default `bridge` Network for Production
- **Problem**: The default `bridge` network disables automatic container DNS resolution by service name and allows all unlinked containers to communicate unrestricted.
- **Remediation**: Always create custom user-defined bridge networks (`docker network create` or Compose networks).

#### 18. Abusing `--net=host`
- **Problem**: Using `--net=host` eliminates network isolation, allows port collisions, and grants the container full access to local host loopback services (such as cloud metadata services at `169.254.169.254`).
- **Remediation**: Use standard bridge or overlay networks with explicit port mapping.

#### 19. Running Containers with `--privileged` Flag
- **Problem**: `--privileged` grants full Linux capabilities and disables device cgroup restrictions, allowing an attacker to escape the container and mount physical host drives directly.
- **Remediation**: Use granular Linux capabilities: `--cap-drop=ALL --cap-add=NET_BIND_SERVICE`.

#### 20. Over-relying on Unconstrained `restart: always`
- **Problem**: If an application has a fatal configuration bug or failing migration, `restart: always` enters a high-speed spin loop, burning 100% host CPU and filling log files with gigabytes of crash dumps.
- **Remediation**: Use `restart: on-failure:5` or orchestrators that implement exponential backoff.

---

## 4. Code Comparisons: Anti-Pattern vs. Production Standard

### Anti-Pattern 1: Flawed vs. Production Dockerfile

```dockerfile
# -------------------------------------------------------------
# DANGEROUS ANTI-PATTERN DOCKERFILE
# -------------------------------------------------------------
FROM node:latest                 # Anti-Pattern: Floating tag

WORKDIR /app
ADD . /app                      # Anti-Pattern: ADD instead of COPY, context leak

RUN npm install                 # Anti-Pattern: Retains devDeps and build cache
RUN apt-get update && apt-get install -y curl  # Anti-Pattern: apt cache retained

ENV DB_PASSWORD="SuperSecret123" # Anti-Pattern: Hardcoded secret in layer

# Anti-Pattern: Runs as default root user
EXPOSE 8080
ENTRYPOINT npm start            # Anti-Pattern: Shell form swallows SIGTERM!
```

```dockerfile
# -------------------------------------------------------------
# PRODUCTION-GRADE HARDENED DOCKERFILE
# -------------------------------------------------------------
# syntax=docker/dockerfile:1.7
FROM node:20.12.2-alpine3.19 AS builder

WORKDIR /app
COPY package*.json ./
RUN --mount=type=cache,target=/root/.npm \
    npm ci --include=dev

COPY . .
RUN npm run build && npm prune --production

# Final Runtime Stage
FROM node:20.12.2-alpine3.19 AS runtime

WORKDIR /app
ENV NODE_ENV=production

# Security: Dedicated system user and group
RUN addgroup -g 10001 -S appgroup && \
    adduser -u 10001 -S appuser -G appgroup

# Copy only production artifacts
COPY --from=builder --chown=appuser:appgroup /app/dist ./dist
COPY --from=builder --chown=appuser:appgroup /app/node_modules ./node_modules
COPY --from=builder --chown=appuser:appgroup /app/package.json ./package.json

USER 10001:10001

HEALTHCHECK --interval=30s --timeout=5s --start-period=10s --retries=3 \
  CMD wget -qO- http://127.0.0.1:8080/health || exit 1

EXPOSE 8080
# Correct: JSON exec form forwards POSIX signals directly
ENTRYPOINT ["node", "dist/index.js"]
```

---

## 5. Practical Production Auditing

### Audit Script: Detecting Critical Anti-Patterns Locally

```bash
#!/usr/bin/env bash
set -euo pipefail

echo "=== DOCKER SECURITY & PRODUCTION AUDIT ==="

# 1. Check for containers running as root (UID 0)
echo "[*] Checking for containers running as root (UID 0)..."
docker ps -q | while read -r cid; do
  user=$(docker inspect --format '{{.Config.User}}' "$cid")
  name=$(docker inspect --format '{{.Name}}' "$cid")
  if [ -z "$user" ] || [ "$user" = "0" ] || [ "$user" = "root" ]; then
    echo "  [!] WARNING: Container $name ($cid) is running as ROOT!"
  fi
done

# 2. Check for containers without memory limits
echo "[*] Checking for unbounded container memory..."
docker ps -q | while read -r cid; do
  mem=$(docker inspect --format '{{.HostConfig.Memory}}' "$cid")
  name=$(docker inspect --format '{{.Name}}' "$cid")
  if [ "$mem" = "0" ]; then
    echo "  [!] WARNING: Container $name has NO MEMORY LIMIT configured!"
  fi
done

# 3. Check for exposed Docker socket mounts
echo "[*] Checking for dangerous /var/run/docker.sock mounts..."
docker ps -q | while read -r cid; do
  socket_mount=$(docker inspect --format '{{range .Mounts}}{{if eq .Source "/var/run/docker.sock"}}{{.Source}}{{end}}{{end}}' "$cid")
  name=$(docker inspect --format '{{.Name}}' "$cid")
  if [ -n "$socket_mount" ]; then
    echo "  [CRITICAL] Container $name has direct access to /var/run/docker.sock!"
  fi
done

echo "=== AUDIT COMPLETE ==="
```

---

## 6. Best Practices Checklist

- [ ] **Non-root user configured**: `USER 10001:10001` declared in all runtime images.
- [ ] **Immutable tags pinned**: No `:latest` or broad release tags in production manifests.
- [ ] **Signal forwarding verified**: JSON exec form used for `ENTRYPOINT` and `CMD`.
- [ ] **Hard resource limits set**: `--memory` and `--cpus` defined for every container.
- [ ] **Log rotation enforced**: Daemon-level `max-size` and `max-file` configured.
- [ ] **Secrets decoupled**: Zero secrets in Dockerfiles, git repos, or image layers.
- [ ] **Internal networking isolated**: Databases and backend stores never expose public ports.
- [ ] **Docker socket protected**: `/var/run/docker.sock` forbidden in public application containers.
- [ ] **Persistent storage mounted**: Writable container layer used strictly for ephemeral tmp data.
- [ ] **Health checks declared**: Containers include verified health check commands.

---

## 7. Common Mistakes & Troubleshooting

| Symptom / Incident | Root Cause Anti-Pattern | Diagnostic Step | Immediate Fix |
|:-------------------|:------------------------|:----------------|:--------------|
| **Database container data lost after upgrade** | Stored data in writable container layer (Anti-Pattern #12) | Inspect container mounts: `docker inspect <id> --format '{{.Mounts}}'` | Create named volume and migrate data before replacing container. |
| **Container takes 10s to stop; DB corrupted** | Shell form ENTRYPOINT swallowing SIGTERM (Anti-Pattern #7) | Run `docker stop -t 1 <id>` and measure delay | Convert ENTRYPOINT to JSON exec array syntax `["executable", "param"]`. |
| **Disk 100% full; host crashed** | Unrotated JSON log driver (Anti-Pattern #11) | Run `du -sh /var/lib/docker/containers/*/*-json.log` | Configure log options in `/etc/docker/daemon.json` and truncate log files. |
| **Attacker gained root shell on host node** | App exploited while running as root with Docker socket mounted | Review container run flags and user mapping | Strip socket mount, drop capabilities, and enforce non-root user. |
| **Production build failed randomly on Monday** | Base image floating tag updated with breaking changes | Check `docker history` for upstream digest change | Pin immutable digest (`image@sha256:...`) in Dockerfile `FROM`. |
| **API can reach DB by IP but not by hostname** | Running on default bridge network (Anti-Pattern #17) | Run `docker inspect <id> --format '{{.NetworkSettings.Networks}}'` | Create a custom bridge network and connect all service containers to it. |

---

## 8. Performance, Security & Cost Implications

### Performance
Writing persistent data into the container writable layer forces every filesystem write through the storage driver union filesystem (`overlay2` copy-up mechanism). Using dedicated Docker volumes or bind mounts bypasses the union filesystem entirely, achieving native host disk I/O throughput.

### Security
Addressing these 20 anti-patterns eliminates 95% of container security incidents:
- Non-root user prevents container breakout vulnerabilities from inheriting host root access.
- Restricting `/var/run/docker.sock` closes the most common container privilege escalation vector.
- Dropping Linux capabilities (`--cap-drop=ALL`) prevents compromised containers from altering host networking, inspecting host processes, or loading kernel modules.

### Cost
Eliminating bloated build caches, dangling layers, and large uncompressed images reduces AWS ECR/GCR registry storage and egress transfer bandwidth costs by up to 80%.

---

## 9. Anti-Pattern Severity Matrix

| Anti-Pattern | Security Risk | Availability Risk | Operational Pain | Recommended Priority |
|:-------------|:-------------|:-------------------|:-----------------|:---------------------|
| **Mounting `/var/run/docker.sock`** | **CRITICAL** | High | Low | **P0 (Immediate)** |
| **Running as Root (`USER 0`)** | **CRITICAL** | Medium | Low | **P0 (Immediate)** |
| **Hardcoded Secrets in Layers** | **CRITICAL** | Low | High | **P0 (Immediate)** |
| **No Memory / CPU Limits** | Medium | **CRITICAL** | High | **P1 (High)** |
| **Missing Log Rotation** | Low | **CRITICAL** | High | **P1 (High)** |
| **Shell Form Signal Swallowing** | Low | High | High | **P1 (High)** |
| **Exposing Internal DB Ports** | **CRITICAL** | Medium | Medium | **P1 (High)** |
| **Using `:latest` Tags** | Low | High | High | **P2 (Medium)** |
| **Missing `.dockerignore`** | Medium | Low | Medium | **P2 (Medium)** |

---

## 10. Real-World Interview Questions

<details>
<summary><strong>Q1: Why is mounting `/var/run/docker.sock` inside a container considered a critical security vulnerability?</strong></summary>

The Docker daemon socket (`/var/run/docker.sock`) is the control plane API for the entire Docker Engine. When a container has access to this socket, an attacker inside that container can issue commands to the host Docker daemon. Specifically, the attacker can execute:
```bash
docker run -v /:/host-root --privileged -it alpine chroot /host-root
```
This spawns a new privileged container mounting the host's actual root filesystem (`/`) and executes `chroot`, giving the attacker complete, unrestricted root access to the host operating system, bypassing all container isolation boundaries.
</details>

<details>
<summary><strong>Q2: Explain the technical difference between `ENTRYPOINT ["npm", "start"]` and `ENTRYPOINT npm start`.</strong></summary>

- `ENTRYPOINT ["npm", "start"]` (Exec Form): The binary `/usr/local/bin/npm` is spawned directly as PID 1 inside the container process namespace. When `docker stop` sends `SIGTERM`, npm receives the signal directly and can notify connected clients, finish inflight requests, close DB pools, and exit cleanly.
- `ENTRYPOINT npm start` (Shell Form): Docker invokes `/bin/sh -c "npm start"`. `/bin/sh` runs as PID 1, and npm runs as a child process (PID 2). By default, standard POSIX shells do not forward signals to child processes. When `docker stop` sends `SIGTERM`, `/bin/sh` ignores it. Docker waits for the 10-second timeout and forcibly executes `SIGKILL` (signal 9), abruptly aborting active processes.
</details>

<details>
<summary><strong>Q3: Why does Docker bypass host UFW / iptables firewall rules when you publish a port with `-p 8080:80`?</strong></summary>

When Docker initializes, it creates a custom iptables chain named `PREROUTING` in the `nat` table that redirects incoming traffic directly to container IP addresses before standard host filtering chains (like UFW's `ufw-user-input` in the `filter` table) are evaluated. Consequently, even if UFW is configured to deny port 8080 from the public internet, Docker's NAT prerouting rules forward the packets directly into the container. To prevent this, administrators must inject filtering rules into Docker's dedicated `DOCKER-USER` chain or bind ports strictly to localhost (`127.0.0.1:8080:80`).
</details>

<details>
<summary><strong>Q4: Why should you avoid running multiple independent daemons (e.g., NGINX and MySQL) inside one container?</strong></summary>

1. **Failure Masking**: Docker monitors PID 1. If MySQL crashes inside a multi-process container but the supervisor process (PID 1) remains running, Docker considers the container healthy, masking service failure.
2. **Resource Accounting**: Control group limits apply to the container as a single unit. It is impossible to isolate or throttle MySQL without starving NGINX.
3. **Log Stream Confusion**: Stdout and stderr from multiple applications intertwine in a single stream, complicating parsing and log ingestion.
4. **Lifecycle Decoupling**: Upgrading NGINX requires tearing down and restarting the database process.
</details>

<details>
<summary><strong>Q5: What are zombie processes and how can PID 1 inside a container cause host resource exhaustion?</strong></summary>

In UNIX, when a child process terminates, its process table entry remains as a "zombie" until its parent calls `wait()` or `waitpid()` to read its exit code. If the parent terminates first, orphaned processes are reparented to PID 1, which must reap them. Standard application runtimes (such as Node.js, Python, or Go) were never designed to act as init systems and often fail to reap orphaned children. Over time, zombie processes accumulate inside the container until the Linux kernel's process limit (`kernel.pid_max`) is reached, preventing the host from spawning any new processes. This is solved by using an init wrapper like `tini` (`docker run --init`).
</details>

<details>
<summary><strong>Q6: How can BuildKit secret mounts prevent credential leaks in Docker images?</strong></summary>

Traditional methods used `ARG GITHUB_TOKEN` and passed secrets during build. Even if deleted in a subsequent line (`RUN rm -f token.txt`), the secret remains permanently accessible in the intermediate layer filesystem history. BuildKit provides `RUN --mount=type=secret,id=token`, which mounts the secret into a temporary in-memory filesystem during that specific execution step. The secret is never committed to the layer metadata or saved in the final image tarball.
</details>

<details>
<summary><strong>Q7: What happens when an unprivileged container user writes to a host bind mount owned by root?</strong></summary>

Linux file permissions are enforced by UID/GID numbers inside the kernel, not username strings. If the host directory is owned by UID 0 (root) with permissions `0755` (`rwxr-xr-x`), and the container runs as `USER appuser` (UID 10001), write attempts fail with `EACCES: Permission denied`. The container process crashes unless the host directory ownership is updated (`chown -R 10001:10001 /host/dir`) or Docker User Namespaces (`userns-remap`) are enabled.
</details>

<details>
<summary><strong>Q8: What is the risk of using `restart: always` on an unmonitored production container?</strong></summary>

If a fatal application defect occurs (such as an invalid database migration or missing environment variable), `restart: always` causes the container to immediately crash, exit, and restart in an infinite loop. This generates extreme CPU context switching, saturates Docker daemon event streams, fills host disk partitions with repetitive crash logs, and can mask system degradation if alerts are not triggered on crash counts.
</details>

---

## 11. Practice Exercises

<details>
<summary><strong>Exercise 1: Audit a Dockerfile with Hadolint</strong></summary>

**Task**: Run `hadolint` against a flawed Dockerfile using Docker and inspect the reported rule violations.

**Solution**:
```bash
# 1. Create flawed Dockerfile
cat <<'EOF' > Dockerfile.bad
FROM ubuntu:latest
RUN apt-get update
RUN apt-get install -y python3
ADD app.tar.gz /app
WORKDIR /app
CMD python3 app.py
EOF

# 2. Run Hadolint via container
docker run --rm -i hadolint/hadolint < Dockerfile.bad

# Expected warnings:
# DL3006: Always tag the version of an image explicitly (ubuntu:latest)
# DL3008: Pin versions in apt get install
# DL3009: Delete the apt-get lists after installing
# DL3020: Use COPY instead of ADD
# DL3025: Use arguments JSON notation for CMD and ENTRYPOINT
```
</details>

<details>
<summary><strong>Exercise 2: Demonstrate Signal Forwarding with Tini</strong></summary>

**Task**: Run a container with and without `--init` to demonstrate clean signal termination.

**Solution**:
```bash
# Test 1: Run with --init (Tini init system injected as PID 1)
docker run -d --name init-test --init alpine sleep 1000

# Inspect PID 1 inside container
docker exec init-test ps aux
# Output confirms: PID 1 is "docker-init"

# Send graceful stop and measure time (stops instantaneously < 0.5s)
time docker stop init-test
docker rm init-test
```
</details>

<details>
<summary><strong>Exercise 3: Remediate Host Port Binding Exposure</strong></summary>

**Task**: Reconfigure a PostgreSQL container command that currently publishes port 5432 publicly to bind strictly to localhost `127.0.0.1`.

**Solution**:
```bash
# Insecure: binds to 0.0.0.0:5432 across public interfaces
# docker run -d -p 5432:5432 postgres:16-alpine

# Hardened: binds strictly to localhost loopback
docker run -d --name secure-postgres -p 127.0.0.1:5432:5432 \
  -e POSTGRES_PASSWORD=securepass postgres:16-alpine

# Verify binding IP
docker port secure-postgres
# Output: 5432/tcp -> 127.0.0.1:5432
docker rm -f secure-postgres
```
</details>

<details>
<summary><strong>Exercise 4: Prevent Writable Layer Data Accumulation</strong></summary>

**Task**: Run a temporary batch processing container with a read-only root filesystem and an in-memory `tmpfs` volume for `/tmp`.

**Solution**:
```bash
docker run --rm \
  --read-only \
  --tmpfs /tmp:rw,noexec,nosuid,size=64m \
  alpine sh -c "echo 'Ephemeral test' > /tmp/test.txt && cat /tmp/test.txt"
```
</details>

<details>
<summary><strong>Exercise 5: Configure Exponential Restart Backoff in Compose</strong></summary>

**Task**: Write a Compose snippet that limits restart attempts on failure rather than using unconstrained `restart: always`.

**Solution**:
```yaml
services:
  worker:
    image: company/worker:v1.0.0
    deploy:
      restart_policy:
        condition: on-failure
        delay: 5s
        max_attempts: 3
        window: 120s
```
</details>

<details>
<summary><strong>Exercise 6: Detect and Prune Dangling Resources</strong></summary>

**Task**: Write a one-liner to calculate total disk space consumed by dangling images and prune them safely.

**Solution**:
```bash
# Calculate space used by dangling images
docker images -f "dangling=true"

# Remove all dangling images without deleting named tags
docker image prune -f
```
</details>

---

## 12. Summary & Quick Reference Cheat Sheet

### Golden Rules of Container Production Engineering
1. **Never Run as Root**: Always define `USER <non-root-uid>`.
2. **Never Use `:latest`**: Pin exact immutable versions and hashes.
3. **Use JSON Exec Form**: `ENTRYPOINT ["bin", "arg"]` ensures POSIX signals propagate.
4. **Isolate Secrets**: Build secrets belong in BuildKit mounts; runtime secrets belong in Docker Secrets or external Vaults.
5. **Always Set Limits**: `--memory`, `--cpus`, and `--pids-limit` prevent host starvation.
6. **Enforce Log Rotation**: Unbounded JSON logs crash servers; configure `max-size: 50m`.
7. **Protect the Docker Socket**: Never mount `/var/run/docker.sock` to public web services.
8. **Keep Persistent Data on Volumes**: Never rely on container writable overlay layers for state.
9. **Never Expose Internal DB Ports**: Communicate across private user-defined bridge/overlay networks.
10. **One Concern per Container**: Decouple reverse proxies, application logic, and databases.

---

## 13. Further Reading & Official Documentation

- [Docker Documentation: Best practices for writing Dockerfiles](https://docs.docker.com/develop/develop-images/dockerfile_best-practices/)
- [CIS Docker Benchmark: Security guidance for Docker Engine](https://www.cisecurity.org/benchmark/docker)
- [Hadolint: Haskell Dockerfile Linter](https://github.com/hadolint/hadolint)
- [OWASP Docker Top 10 Security Risks](https://owasp.org/www-project-docker-top-10/)
- [Docker Documentation: Configure container logging](https://docs.docker.com/config/containers/logging/configure/)

---

[<- 13-from-docker-to-kubernetes.md](./13-from-docker-to-kubernetes.md) | [Back to Module Overview](./README.md) | [Next Module: 07-projects/01-node-api-with-postgres-and-redis.md ->](../07-projects/01-node-api-with-postgres-and-redis.md)
