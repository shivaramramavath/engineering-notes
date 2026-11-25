# Docker Compose Specification: Exhaustive Directive Reference

```
Track: DevOps & Containerization
Module: 05-compose
Prerequisites: 05-compose/01-compose-basics.md, 05-compose/02-compose-multi-service.md
Difficulty: Advanced
Estimated Duration: 55 minutes
Target Docker Version: 27.x+ (Compose Specification v2.x+)
Last Validated: February 2025
```

---

## Table of Contents
1. [What & Why](#1-what--why)
2. [Top-Level Schema Structure](#2-top-level-schema-structure)
3. [Service Configuration Directives](#3-service-configuration-directives)
   - [3.1 Build & Image Definitions](#31-build--image-definitions)
   - [3.2 Runtime Execution & Process Controls](#32-runtime-execution--process-controls)
   - [3.3 Environment & Configuration Management](#33-environment--configuration-management)
   - [3.4 Networking, Ports, and Host Bindings](#34-networking-ports-and-host-bindings)
   - [3.5 Storage: Short vs Long Volume Syntax](#35-storage-short-vs-long-volume-syntax)
   - [3.6 Healthchecks and Dependencies](#36-healthchecks-and-dependencies)
   - [3.7 Deployment, Scaling, and Resource Limits](#37-deployment-scaling-and-resource-limits)
   - [3.8 Security, Capabilities, and Kernel Controls](#38-security-capabilities-and-kernel-controls)
4. [Top-Level Networks, Volumes, Secrets, and Configs](#4-top-level-networks-volumes-secrets-and-configs)
5. [Complete Production Reference Manifest](#5-complete-production-reference-manifest)
6. [Production Best Practices & Antipatterns](#6-production-best-practices--antipatterns)
7. [Common Pitfalls & Troubleshooting](#7-common-pitfalls--troubleshooting)
8. [Performance Tuning & Resource Optimization](#8-performance-tuning--resource-optimization)
9. [Security Hardening & Threat Model](#9-security-hardening--threat-model)
10. [Architectural Comparison: Short vs Long Syntax](#10-architectural-comparison-short-vs-long-syntax)
11. [Self-Assessment & Interview Questions](#11-self-assessment--interview-questions)
12. [Hands-on Practice Exercises](#12-hands-on-practice-exercises)
13. [Summary & Reference Cheat Sheet](#summary--reference-cheat-sheet)

---

## 1. What & Why

The **Compose Specification** ([compose-spec.io](https://compose-spec.io)) is the open, vendor-neutral standard for container orchestration manifests. Unlike simple tutorials that touch only `image` and `ports`, enterprise Compose files must handle complex orchestration requirements:
- Multi-stage build targets and BuildKit cache exports.
- Kernel capability dropping (`cap_drop: [ALL]`) and unprivileged user mappings.
- Fine-grained memory/CPU resource ceilings (`deploy.resources.limits`).
- Encrypted secrets management (`secrets:`) and external configuration files (`configs:`).
- Long-form volume mounting with bind propagation and tmpfs size restrictions.

This guide serves as the definitive reference manual for every core directive in the Compose specification.

---

## 2. Top-Level Schema Structure

A valid Compose specification file is structured under 5 primary top-level keys:

```yaml
# Top-level keys:
services:  # [MANDATORY] Microservice container specifications
networks:  # [OPTIONAL] Custom virtual network definitions
volumes:   # [OPTIONAL] Persistent named volume storage definitions
secrets:   # [OPTIONAL] Sensitive credential definitions (files or external)
configs:   # [OPTIONAL] Non-sensitive configuration file definitions
```

---

## 3. Service Configuration Directives

### 3.1 Build & Image Definitions

#### `image`
Specifies the image to run or the tag to assign when building.
```yaml
image: myregistry.corp.com/team/auth-service:2.4.1
```

#### `build`
Specifies build parameters. Can be a string path or an object for advanced BuildKit controls:
```yaml
build:
  context: ./backend                  # Build directory context
  dockerfile: Dockerfile.prod         # Alternate Dockerfile name
  target: runtime                     # Specific multi-stage target
  args:                               # Build-time variables (ARG)
    APP_VERSION: "2.4.1"
  cache_from:                         # External registry cache sources
    - type=registry,ref=mycorp/api:cache
  cache_to:                           # Push cache back to registry
    - type=registry,ref=mycorp/api:cache,mode=max
  secrets:                            # BuildKit build-time secret injection
    - npmrc
  ssh:                                # BuildKit SSH agent forwarding
    - default
```

---

### 3.2 Runtime Execution & Process Controls

#### `command` and `entrypoint`
Overrides the Dockerfile `CMD` and `ENTRYPOINT`. Always use JSON array format:
```yaml
entrypoint: ["/usr/bin/tini", "--"]
command: ["node", "dist/server.js", "--port", "3000"]
```

#### `container_name`
Assigns a static container name instead of the default `<project>_<service>_<index>`.
*(Caution: Restricts scaling to 1 replica)*.
```yaml
container_name: production-edge-router
```

#### `user`
Declares the UID:GID to execute the process, dropping root privileges:
```yaml
user: "10001:10001"
```

#### `working_dir`
Overrides the default working directory inside the container:
```yaml
working_dir: /var/app
```

#### `restart`
Defines the container restart policy:
- `no`: Never restart (default).
- `always`: Always restart if stopped or host reboots.
- `on-failure[:max-retries]`: Restart only if process exits with non-zero exit code.
- `unless-stopped`: Restart always, unless manually stopped by operator.
```yaml
restart: unless-stopped
```

---

### 3.3 Environment & Configuration Management

#### `environment`
Injects environment variables. Supports map or list syntax:
```yaml
environment:
  NODE_ENV: production
  DATABASE_URL: "postgres://user:pass@db:5432/app"
  DEBUG: "false"
```

#### `env_file`
Loads environment variables from external files:
```yaml
env_file:
  - .env
  - .env.production
```

---

### 3.4 Networking, Ports, and Host Bindings

#### `ports`
Maps host ports to container ports. Supports short or long syntax:
```yaml
# Short syntax:
ports:
  - "127.0.0.1:8080:80"        # Localhost binding (Secure)
  - "443:443"                  # Public binding (0.0.0.0)
  - "53:53/udp"                # UDP protocol

# Long syntax (Rich parameters):
ports:
  - target: 80                 # Port inside container
    published: 8080            # Port on host
    protocol: tcp
    mode: host                 # 'host' or 'ingress'
```

#### `expose`
Documents internal ports without publishing them to the host:
```yaml
expose:
  - "5432"
```

#### `networks`
Assigns service to networks, with optional static IPs and aliases:
```yaml
networks:
  backend-net:
    aliases:
      - primary-db
    ipv4_address: 10.20.0.50
```

#### `extra_hosts`
Injects custom `/etc/hosts` entries:
```yaml
extra_hosts:
  - "host.docker.internal:host-gateway"
  - "corporate-api.internal:192.168.10.5"
```

---

### 3.5 Storage: Short vs Long Volume Syntax

Compose supports two volume declaration syntaxes:

#### Short Syntax: `[source:]target[:mode]`
```yaml
volumes:
  - db_data:/var/lib/postgresql/data       # Named volume
  - ./config:/etc/nginx/conf.d:ro          # Read-only bind mount
  - /tmp/cache                             # Anonymous volume
```

#### Long Syntax (Preferred in Enterprise Specs):
```yaml
volumes:
  - type: volume                           # 'volume', 'bind', 'tmpfs'
    source: db_data
    target: /var/lib/postgresql/data
    read_only: false
    volume:
      nocopy: true                         # Disable automatic image copying

  - type: bind
    source: ./local-source
    target: /app/src
    read_only: true
    bind:
      create_host_path: false              # Fail if host path is missing!

  - type: tmpfs
    target: /run/secrets
    tmpfs:
      size: 64M                            # RAM byte ceiling
      mode: 0700                           # Octal permissions
```

---

### 3.6 Healthchecks and Dependencies

#### `healthcheck`
Defines an active in-container readiness probe:
```yaml
healthcheck:
  test: ["CMD", "curl", "-f", "http://localhost:3000/healthz"]
  interval: 10s
  timeout: 3s
  retries: 3
  start_period: 20s
  start_interval: 2s
```

#### `depends_on`
Controls startup sequence based on process state or health status:
```yaml
depends_on:
  db:
    condition: service_healthy             # Wait for db healthcheck = 0
    restart: true                          # Restart service if db restarts
  redis:
    condition: service_started             # Wait only for process start
```

---

### 3.7 Deployment, Scaling, and Resource Limits

#### `deploy`
Configures production runtime constraints and scaling:
```yaml
deploy:
  replicas: 3                              # Scale to 3 instances
  resources:
    limits:
      cpus: '1.5'                          # Cap at 1.5 CPU cores
      memory: 1024M                        # Hard OOM ceiling
    reservations:
      cpus: '0.25'                         # Minimum guaranteed CPU
      memory: 256M                         # Minimum guaranteed RAM
  restart_policy:
    condition: on-failure
    delay: 5s
    max_attempts: 3
    window: 120s
```

---

### 3.8 Security, Capabilities, and Kernel Controls

#### `cap_drop` and `cap_add`
Enforces least-privilege Linux kernel capabilities:
```yaml
cap_drop:
  - ALL                                    # Drop all 40+ Linux root capabilities
cap_add:
  - NET_BIND_SERVICE                       # Grant strictly ability to bind port 80/443
```

#### `security_opt`
Enforces no-new-privileges and custom AppArmor/SELinux profiles:
```yaml
security_opt:
  - no-new-privileges:true                 # Prevent setuid privilege escalation
  - seccomp:unconfined                     # Or path to custom seccomp profile
```

#### `read_only`
Mounts the container's root filesystem as completely immutable:
```yaml
read_only: true
```

---

## 4. Top-Level Networks, Volumes, Secrets, and Configs

### `networks` Definition
```yaml
networks:
  frontend-net:
    driver: bridge
    ipam:
      driver: default
      config:
        - subnet: 10.100.0.0/16
          gateway: 10.100.0.1
  external-net:
    external: true                         # References pre-existing Docker network
```

### `volumes` Definition
```yaml
volumes:
  db_data:
    driver: local                          # Standard local volume
    labels:
      environment: production
  nfs_data:                                # Remote NFS volume mount
    driver: local
    driver_opts:
      type: "nfs"
      o: "addr=192.168.1.50,rw"
      device: ":/path/to/nfs/share"
```

### `secrets` and `configs` Definitions
```yaml
secrets:
  db_password:
    file: ./secrets/db_password.txt        # Local file loaded as Docker secret
  external_api_token:
    external: true                         # Managed externally

configs:
  nginx_conf:
    file: ./config/nginx.conf
```

---

## 5. Complete Production Reference Manifest

```yaml
# compose.yaml
services:
  api:
    build:
      context: .
      dockerfile: Dockerfile
      target: runtime
    image: mycorp/api:1.2.0
    restart: unless-stopped
    read_only: true
    user: "10001:10001"
    security_opt:
      - no-new-privileges:true
    cap_drop:
      - ALL
    ports:
      - "127.0.0.1:3000:3000"
    environment:
      NODE_ENV: production
      PORT: 3000
      DB_PASSWORD_FILE: /run/secrets/db_password
    secrets:
      - db_password
    volumes:
      - type: tmpfs
        target: /tmp
        tmpfs:
          size: 32M
          mode: 0700
    networks:
      - backend-net
    depends_on:
      db:
        condition: service_healthy
    deploy:
      resources:
        limits:
          cpus: '2.0'
          memory: 1024M
    logging:
      driver: "json-file"
      options:
        max-size: "10m"
        max-file: "3"

  db:
    image: postgres:17-alpine
    restart: always
    environment:
      POSTGRES_DB: app
      POSTGRES_USER: appuser
      POSTGRES_PASSWORD_FILE: /run/secrets/db_password
    secrets:
      - db_password
    volumes:
      - type: volume
        source: pg_data
        target: /var/lib/postgresql/data
    networks:
      - backend-net
    healthcheck:
      test: ["CMD-SHELL", "pg_isready -U appuser -d app"]
      interval: 5s
      timeout: 3s
      retries: 5
      start_period: 10s
    deploy:
      resources:
        limits:
          memory: 2048M

networks:
  backend-net:
    driver: bridge

volumes:
  pg_data:

secrets:
  db_password:
    file: ./secrets/db_password.txt
```

---

## 6. Production Best Practices & Antipatterns

### Best Practices
- **Use Long Syntax for Volumes**: Long syntax (`type: bind, create_host_path: false`) prevents Docker from accidentally creating empty root-owned folders on the host.
- **Always Apply Kernel Capability Dropping**: Set `cap_drop: [ALL]` and add back only what is required.
- **Implement Secrets instead of Environment Passwords**: Mount passwords into `/run/secrets/` to keep them out of process memory dumps and `docker inspect`.
- **Enforce Memory and CPU Limits**: Unbounded containers risk triggering host-level OOM panics.

### Anti-Patterns
- **Using Obsolete `version:` Attribute**: Drop `version: '3.8'` completely.
- **Exposing Internal Ports to `0.0.0.0`**: Avoid `- "5432:5432"`; use `- "127.0.0.1:5432:5432"` or omit ports.
- **Using Anonymous Volumes**: Anonymous volumes become orphaned upon container recreation.

---

## 7. Common Pitfalls & Troubleshooting

| Symptom | Root Cause | Diagnosis Command | Permanent Fix |
|:---|:---|:---|:---|
| Compose throws `yaml: line X: mapping values are not allowed` | Tab character used for indentation instead of 2 spaces | `cat -T compose.yaml` | Replace all tabs with 2 spaces. |
| Container fails to start with `read-only file system` | `read_only: true` set, but app writes to `/tmp` or `/var/run` | `docker compose logs` | Add `tmpfs` mounts for `/tmp` and `/var/run`. |
| Secrets file fails with `open ./secrets/db.txt: no such file` | File path in top-level `secrets:` does not exist on host | `ls -la ./secrets/` | Create secret file before running `docker compose up`. |
| Host port conflict on scaled replicas | Service defined hardcoded host port in `ports:` | `docker compose ps` | Remove host port or use reverse proxy. |
| Memory limit ignored on Linux | Host Linux cgroups v2 swap/memory accounting disabled | `docker info \| grep WARNING` | Enable `cgroup_enable=memory` in Linux kernel boot args. |
| Healthcheck fails with `executable file not found` | Probe command uses `curl` inside minimal image where `curl` is missing | `docker inspect` health log | Use `wget`, language one-liner, or custom probe binary. |

---

## 8. Performance Tuning & Resource Optimization

1. **Avoid Unnecessary Logging Overhead**:
   Configure `json-file` log limits per service to prevent log files from growing to hundreds of gigabytes and exhausting disk I/O.
2. **BuildKit Cache Sharing**:
   Configure `cache_from` and `cache_to` under `build:` to reuse container build layer caches across distributed CI runners.

---

## 9. Security Hardening & Threat Model

```
+--------------------------------------------------------------+
| HARDENED COMPOSE SERVICE CONFIGURATION MATRIX                 |
+--------------------------------------------------------------+
| Security Directive        | Defensive Benefit                |
|---------------------------+----------------------------------|
| user: "10001:10001"       | Drops root UID 0 execution       |
| read_only: true           | Prevents modifying rootfs files  |
| cap_drop: [ALL]           | Drops 40+ Linux kernel syscalls  |
| security_opt:             | Prevents setuid binary breakout  |
|   - no-new-privileges:true|                                  |
| secrets: [db_password]    | In-memory RAM mount (/run/secrets)|
| ports: - "127.0.0.1:..."  | Blocks public internet exposure  |
+--------------------------------------------------------------+
```

---

## 10. Architectural Comparison: Short vs Long Syntax

| Attribute | Short Syntax (`./src:/app:ro`) | Long Syntax (`type: bind, ...`) |
|:---|:---|:---|
| **Verbosity** | Compact, fast to type | Explicit, multi-line YAML |
| **Missing Path Handling** | Silently creates host folder as root | Fails loudly if `create_host_path: false` |
| **Tmpfs Controls** | Limited (no size control) | Fine-grained (`size: 64M`, `mode: 0700`)|
| **Readability** | Obscure flags (`:z`, `:nocopy`) | Key-value pairs (`read_only: true`) |
| **Enterprise Standard**| Prototyping only | **Mandatory for Production** |

---

## 11. Self-Assessment & Interview Questions

<details>
<summary><strong>1. What is the difference between `ports:` and `expose:` in a Compose file?</strong></summary>

**Answer:**
`ports:` actively modifies the host Linux firewall (iptables) to map an external host port to a container port, making it reachable from outside the Docker virtual network. `expose:` merely documents that the container listens on specified ports and makes them reachable *only* to other containers on the same Docker network; it does not publish the ports to the host machine.
</details>

<details>
<summary><strong>2. How does the `secrets:` top-level directive work in Docker Compose v2?</strong></summary>

**Answer:**
`secrets:` defines sensitive data (such as API keys or database passwords) loaded from host files or external stores. When assigned to a service, Compose mounts the secret as an in-memory tmpfs file inside the container at `/run/secrets/<secret_name>` (mode `0444`). This ensures secrets are never baked into container image layers, logged in `docker inspect`, or exposed in process environment tables.
</details>

<details>
<summary><strong>3. Why is `security_opt: ["no-new-privileges:true"]` an essential production directive?</strong></summary>

**Answer:**
`no-new-privileges:true` sets the Linux kernel `PR_SET_NO_NEW_PRIVS` flag on container processes. This prevents child processes from gaining additional privileges via `setuid` or `setgid` binaries (such as `sudo` or `pkexec`), effectively preventing container breakout privilege escalation attacks even if a malicious binary is executed inside the container.
</details>

<details>
<summary><strong>4. What does `nocopy: true` do in a volume declaration?</strong></summary>

**Answer:**
By default, when an empty named volume is mounted to a non-empty container directory, Docker automatically copies existing files from the image into the volume. Setting `volume: { nocopy: true }` disables this automatic copying behavior, which improves mount performance and prevents overwriting pre-existing data in shared storage configurations.
</details>

<details>
<summary><strong>5. How do you enforce strict memory and CPU limits on a container in Compose v2 without running in Swarm mode?</strong></summary>

**Answer:**
Under the service's `deploy.resources.limits` key:
```yaml
deploy:
  resources:
    limits:
      cpus: '2.0'
      memory: 1024M
```
Compose v2 natively respects `deploy.resources.limits` in standalone mode without requiring Swarm.
</details>

<details>
<summary><strong>6. What is the difference between `environment:` and `env_file:`?</strong></summary>

**Answer:**
`environment:` defines environment variables directly inline within the Compose YAML manifest. `env_file:` specifies external text files (e.g. `.env.production`) from which environment variables are parsed. If a variable is defined in both, the inline `environment:` value takes precedence.
</details>

<details>
<summary><strong>7. How does Compose handle build caching with `cache_from` and `cache_to`?</strong></summary>

**Answer:**
When using BuildKit, `cache_from` instructs the builder to pull cached layers from an external image registry rather than building from scratch. `cache_to` pushes the newly computed build cache back to the registry, enabling shared, remote build caching across distributed CI/CD runner pipelines.
</details>

<details>
<summary><strong>8. Why should you avoid using `container_name:` in services intended for horizontal scaling?</strong></summary>

**Answer:**
Container names must be globally unique across a Docker host. If a service defines a static `container_name: api-server`, attempting to scale the service via `docker compose up --scale api=3` will fail because subsequent replica containers will attempt to use the exact same container name and collide.
</details>

---

## 12. Hands-on Practice Exercises

<details>
<summary><strong>Exercise 1: Defining a Long-Syntax Tmpfs Mount with Size Bounds</strong></summary>

**Goal:** Configure a service with an immutable root filesystem and an in-memory 16MB tmpfs mount.

**Step-by-step Solution:**
1. Create `compose.yaml`:
   ```yaml
   services:
     sandbox:
       image: alpine
       command: sleep 3600
       read_only: true
       volumes:
         - type: tmpfs
           target: /tmp
           tmpfs:
             size: 16M
             mode: 0700
   ```
2. Run: `docker compose up -d`
3. Verify `/tmp` size: `docker compose exec sandbox df -h /tmp` (Shows 16M).
</details>

<details>
<summary><strong>Exercise 2: Injecting File Secrets via Compose</strong></summary>

**Goal:** Create a local secret file and mount it into a running container at `/run/secrets/`.

**Step-by-step Solution:**
1. Create secret file: `echo "supersecrettoken" > token.txt`
2. Create manifest:
   ```yaml
   services:
     app:
       image: alpine
       command: sleep 3600
       secrets: [api_token]
   secrets:
     api_token:
       file: ./token.txt
   ```
3. Run and verify: `docker compose up -d && docker compose exec app cat /run/secrets/api_token`
</details>

<details>
<summary><strong>Exercise 3: Enforcing Least Privilege with `cap_drop: [ALL]`</strong></summary>

**Goal:** Launch a container that has all Linux capabilities dropped and verify ping fails.

**Step-by-step Solution:**
1. Create manifest:
   ```yaml
   services:
     restricted:
       image: alpine
       command: sleep 3600
       cap_drop: [ALL]
   ```
2. Test capability drop:
   ```bash
   docker compose up -d
   docker compose exec restricted ping -c 1 8.8.8.8
   # Operation not permitted (RAW socket capability is dropped!)
   ```
</details>

<details>
<summary><strong>Exercise 4: Configuring Resource Ceilings</strong></summary>

**Goal:** Constrain a container to 128MB RAM and verify with `docker stats`.

**Step-by-step Solution:**
1. Configure `deploy.resources.limits.memory: 128M`.
2. Start and check `docker stats --no-stream`.
</details>

<details>
<summary><strong>Exercise 5: Validating Resolved Configuration via CLI</strong></summary>

**Goal:** Run `docker compose config` to inspect fully merged environment variables.

**Step-by-step Solution:**
1. Execute: `docker compose config`
2. Check formatted output.
</details>

<details>
<summary><strong>Exercise 6: Configuring Log Rotation Directives</strong></summary>

**Goal:** Enforce 5MB log rotation on a high-throughput logging service.

**Step-by-step Solution:**
1. Configure service `logging` options:
   ```yaml
   logging:
     driver: "json-file"
     options:
       max-size: "5m"
       max-file: "2"
   ```
2. Verify with `docker inspect`.
</details>

---

## 13. Summary & Reference Cheat Sheet

### Top 8 Enterprise Compose Directives
1. **`read_only: true`**: Immutable root filesystem.
2. **`cap_drop: [ALL]`**: Drops all kernel privileges.
3. **`security_opt: ["no-new-privileges:true"]`**: Prevents setuid escalations.
4. **`user: "10001:10001"`**: Unprivileged UID execution.
5. **`secrets:`**: Mounts credentials in RAM `/run/secrets/`.
6. **`deploy.resources.limits`**: Hard CPU & RAM ceilings.
7. **`healthcheck:`**: Custom readiness probes.
8. **`depends_on: { condition: service_healthy }`**: Deterministic startup ordering.

---

[← Previous: 02-compose-multi-service.md](02-compose-multi-service.md) | [Next: 04-env-files-and-interpolation.md →](04-env-files-and-interpolation.md)
