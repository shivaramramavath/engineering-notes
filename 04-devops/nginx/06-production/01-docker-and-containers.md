---
title: "NGINX in Docker & Containerized Production Workloads"
description: "Comprehensive guide to running NGINX in containers: official images, multi-stage Dockerfiles, non-root execution (nginx-unprivileged), PID 1 signal forwarding, environment substitution, and health checks."
module: "06-production"
topic: "01-docker-and-containers"
status: "completed"
tags: ["nginx", "docker", "containers", "dockerfile", "unprivileged", "pid1", "signals", "envsubst", "healthcheck"]
prerequisites: ["01-fundamentals/01-installation.md", "04-internals/01-architecture-master-worker-and-event-loop.md"]
estimated_read_time: "35 min"
---

# NGINX in Docker & Containerized Production Workloads

```
+-------------------------------------------------------------------------------+
| Containerized NGINX Architecture & Process Tree                               |
|                                                                               |
|  [Docker / OCI Host Engine]                                                   |
|        │  docker stop --time 30 <container_id>                                |
|        ▼ (Sends SIGQUIT to PID 1)                                             |
|  [Container Namespace (PID 1: nginx master)]                                  |
|        │  daemon off; (Runs in foreground; captures STDOUT/STDERR)            |
|        │  Non-root: UID 101 (nginxinc/nginx-unprivileged)                      |
|        │  Listens on unprivileged ports (8080, 8443)                          |
|        ▼                                                                      |
|  [NGINX Worker Processes (Children)]                                          |
|        ├── Worker 1 (epoll loop, non-blocking I/O)                            |
|        └── Worker 2 (epoll loop, non-blocking I/O)                            |
|        │                                                                      |
|        ▼                                                                      |
|  [File & Logging Streams]                                                     |
|        ├─ /var/log/nginx/access.log  --> Symlink to /dev/stdout               |
|        ├─ /var/log/nginx/error.log   --> Symlink to /dev/stderr               |
|        └─ /tmp/, /var/cache/nginx/   --> tmpfs mounts for temp buffers        |
+-------------------------------------------------------------------------------+
```

---

## Table of Contents
1. [What & Why](#1-what--why)
2. [Core Concepts: Container Runtime Differences](#2-core-concepts-container-runtime-differences)
3. [How It Works Internally](#3-how-it-works-internally)
4. [Syntax & Container Directives Reference](#4-syntax--container-directives-reference)
5. [Practical Implementation Examples](#5-practical-implementation-examples)
6. [Dynamic Configuration via Environment Variables & Templates](#6-dynamic-configuration-via-environment-variables--templates)
7. [Common Pitfalls & Anti-Patterns](#7-common-pitfalls--anti-patterns)
8. [Security, Non-Root Hardening & Read-Only Filesystems](#8-security-non-root-hardening--read-only-filesystems)
9. [Architectural Comparison: Bare-Metal / VM vs Dockerized NGINX](#9-architectural-comparison-bare-metal--vm-vs-dockerized-nginx)
10. [Technical Interview Questions & Scenarios](#10-technical-interview-questions--scenarios)
11. [Hands-On Practice Exercises](#11-hands-on-practice-exercises)
12. [Summary & Quick Reference](#12-summary--quick-reference)
13. [Further Reading](#13-further-reading)

---

## 1. What & Why

Running NGINX inside Docker containers has become the predominant deployment model across microservices, Kubernetes pods, and edge container runtimes. However, containerizing NGINX is not as simple as dropping a configuration file into an image:
- **Foreground Daemon Execution**: By default, NGINX forks into the background as a traditional UNIX daemon. In Docker, if PID 1 exits, the container immediately terminates (`Exited (0)`).
- **Log Routing to Container Streams**: Containers must forward logs to `STDOUT` and `STDERR` so that Docker, containerd, and log collectors (Fluentbit, Promtail, Vector) can capture them without disk accumulation.
- **Graceful Termination Signals**: Docker sends `SIGTERM` by default, which causes NGINX to terminate connections violently. NGINX requires `SIGQUIT` for graceful drain.
- **Root vs Unprivileged Execution**: Standard images run the master process as `root` to bind ports 80 and 443, violating Kubernetes security policies (Pod Security Standards: `restricted`).

Addressing these factors ensures zero-downtime rolling deploys, compliance with container security benchmarks, and seamless log ingestion.

---

## 2. Core Concepts: Container Runtime Differences

### Image Variants & Base Distributions
The official Docker Hub library provides multiple NGINX image tags:

| Image Tag Family | Base OS | Compressed Size | Recommended Use Case |
| :--- | :--- | :--- | :--- |
| `nginx:stable-alpine` | Alpine Linux (musl) | ~10 MB | Production default (minimal CVE surface, small footprint) |
| `nginx:stable` | Debian Bookworm (glibc)| ~55 MB | Production where glibc or dynamic C modules are required |
| `nginx:mainline-alpine`| Alpine Linux | ~12 MB | Workloads needing HTTP/3 QUIC or bleeding-edge features |
| `nginxinc/nginx-unprivileged`| Debian / Alpine | ~18 MB | Hardened Kubernetes clusters (runs as UID 101 `nginx`) |

---

## 3. How It Works Internally

### The PID 1 Signal Forwarding & Graceful Shutdown Sequence

```
  Step 1: Host Orchestrator issues shutdown
          docker stop --time=30 my-nginx-container
          │
          ▼
  Step 2: Docker Engine sends SIGQUIT (configured via STOPSIGNAL)
          │
          ▼
  Step 3: NGINX Master Process (PID 1) receives SIGQUIT
          ├─ Master stops listening on port 80/8080 (closes socket listen queue)
          ├─ Master sends SIGQUIT to all worker processes
          ▼
  Step 4: Worker Processes Enter Graceful Drain
          ├─ Accept NO new incoming connections
          ├─ Continue processing active requests to completion
          ├─ Send final bytes and close client sockets
          ▼
  Step 5: Workers terminate cleanly -> Master exits (code 0) -> Container stops
```

If the Dockerfile omits `STOPSIGNAL SIGQUIT`, the Docker daemon defaults to `SIGTERM`. When NGINX receives `SIGTERM`, it executes a **fast shutdown**, immediately dropping active client connections, aborting in-flight database uploads, and throwing `502 Bad Gateway` errors to upstream callers!

---

## 4. Syntax & Container Directives Reference

### Mandatory Directives for Containerized NGINX
- `daemon off;`: Enforces foreground execution. Must be specified in the main configuration context or passed via CLI argument (`nginx -g 'daemon off;'`).
- `error_log /dev/stderr warn;`: Streams diagnostic logs directly to standard error.
- `access_log /dev/stdout main;`: Streams HTTP transaction logs directly to standard output.
- `pid /tmp/nginx.pid;`: Required for non-root containers where `/var/run/` is read-only or owned by root.

### Official Entrypoint Hooks (`/docker-entrypoint.d/`)
The official NGINX entrypoint script executes executable scripts found in `/docker-entrypoint.d/` alphabetically before starting NGINX:
- `10-listen-on-ipv6-by-default.sh`: Enables IPv6 if supported.
- `20-envsubst-on-templates.sh`: Automatically processes `*.template` files located in `/etc/nginx/templates/` and outputs parsed `.conf` files to `/etc/nginx/conf.d/`.
- `30-tune-worker-processes.sh`: Auto-tunes `worker_processes` based on container CPU quotas.

---

## 5. Practical Implementation Examples

### Example 1: Multi-Stage Production Dockerfile for a React/Next SPA

Build static web assets and serve them using a lightweight, hardened Alpine image:

```dockerfile
# Stage 1: Build Application Artifacts
FROM node:22-alpine AS builder
WORKDIR /app

# Cache dependencies
COPY package.json package-lock.json ./
RUN npm ci

COPY . .
RUN npm run build

# Stage 2: Production NGINX Serving Container
FROM nginx:1.26-alpine

# Set graceful termination signal
STOPSIGNAL SIGQUIT

# Remove default boilerplate configuration
RUN rm /etc/nginx/conf.d/default.conf

# Copy custom hardened virtual host configuration
COPY nginx/prod.conf /etc/nginx/conf.d/app.conf

# Copy compiled static assets from builder stage
COPY --from=builder /app/dist /usr/share/nginx/html

# Expose HTTP port
EXPOSE 80

# Health check to ensure worker responsiveness
HEALTHCHECK --interval=15s --timeout=3s --retries=3 --start-period=5s \
  CMD wget -q --spider http://127.0.0.1/health || exit 1

# Run in foreground
CMD ["nginx", "-g", "daemon off;"]
```

### Example 2: Non-Root Production Dockerfile (`nginxinc/nginx-unprivileged`)

Enforces complete rootless execution for strict Kubernetes environments:

```dockerfile
FROM nginxinc/nginx-unprivileged:1.26-alpine

STOPSIGNAL SIGQUIT

# Runs as UID 101 (nginx) out of the box
USER 101

# Copy custom configuration
COPY --chown=101:101 nginx/unprivileged.conf /etc/nginx/conf.d/default.conf
COPY --chown=101:101 ./static /usr/share/nginx/html

# Non-root cannot bind port 80; uses port 8080
EXPOSE 8080

HEALTHCHECK --interval=10s --timeout=2s --start-period=3s \
  CMD wget -q -O - http://127.0.0.1:8080/health || exit 1

CMD ["nginx", "-g", "daemon off;"]
```

Corresponding `unprivileged.conf`:
```nginx
# /etc/nginx/conf.d/default.conf
server {
    listen 8080 default_server;
    server_name _;

    root /usr/share/nginx/html;
    index index.html;

    location /health {
        access_log off;
        return 200 "healthy\n";
    }

    location / {
        try_files $uri $uri/ /index.html;
    }
}
```

### Example 3: Production Docker Compose Integration

A complete microservices stack with reverse proxying, custom networks, and resource caps:

```yaml
# docker-compose.yml
version: '3.8'

services:
  gateway:
    image: nginxinc/nginx-unprivileged:1.26-alpine
    container_name: edge-gateway
    restart: always
    ports:
      - "80:8080"
      - "443:8443"
    environment:
      - API_UPSTREAM_HOST=api-backend
      - API_UPSTREAM_PORT=3000
    volumes:
      - ./nginx/templates:/etc/nginx/templates:ro
      - ./certs:/etc/nginx/certs:ro
    deploy:
      resources:
        limits:
          cpus: '2.0'
          memory: 512M
        reservations:
          cpus: '0.5'
          memory: 128M
    depends_on:
      - api-backend
    networks:
      - app-tier

  api-backend:
    image: node:22-alpine
    container_name: api-backend
    restart: on-failure
    expose:
      - "3000"
    networks:
      - app-tier

networks:
  app-tier:
    driver: bridge
```

---

## 6. Dynamic Configuration via Environment Variables & Templates

Standard NGINX configuration syntax does NOT support reading environment variables natively at runtime (except via Lua or njs). The official NGINX Docker image provides automatic template substitution via `envsubst`:

### Step 1: Create the Template File (`/etc/nginx/templates/default.conf.template`)
```nginx
# /etc/nginx/templates/default.conf.template
upstream backend_cluster {
    server ${BACKEND_HOST}:${BACKEND_PORT};
    keepalive 32;
}

server {
    listen 8080;
    server_name ${DOMAIN_NAME};

    location / {
        proxy_pass http://backend_cluster;
        proxy_http_version 1.1;
        proxy_set_header Connection "";
        proxy_set_header Host $host;
        proxy_set_header X-Real-IP $remote_addr;
    }

    location /health {
        return 200 "OK\n";
    }
}
```

### Step 2: Pass Environment Variables via Docker Run
```bash
docker run -d \
  -p 80:8080 \
  -e BACKEND_HOST=api.internal.svc \
  -e BACKEND_PORT=4000 \
  -e DOMAIN_NAME=api.example.com \
  -v "$PWD/templates:/etc/nginx/templates:ro" \
  nginxinc/nginx-unprivileged:1.26-alpine
```
*Behind the scenes*: Before starting NGINX, `/docker-entrypoint.d/20-envsubst-on-templates.sh` reads `default.conf.template`, replaces `${BACKEND_HOST}`, `${BACKEND_PORT}`, and `${DOMAIN_NAME}` with their environment values, and writes the evaluated `/etc/nginx/conf.d/default.conf`.

*Security Tip*: Explicitly declare `NGINX_ENVSUBST_TEMPLATE_SUFFIX=.template` and pass `NGINX_ENVSUBST_OUTPUT_DIR=/etc/nginx/conf.d` to restrict substitution and prevent accidental corruption of NGINX variables like `$host` and `$uri`.

---

## 7. Common Pitfalls & Anti-Patterns

| Anti-Pattern / Mistake | Root Cause | Observable Production Impact | Correct Solution |
| :--- | :--- | :--- | :--- |
| Default `STOPSIGNAL` (`SIGTERM`) | Docker stops container with `SIGTERM` instead of `SIGQUIT` | Dropped TCP sockets; clients receive abrupt 502s during rolling deploys | Declare `STOPSIGNAL SIGQUIT` in Dockerfile |
| Running `daemon on;` | Master process forks child and exits immediately | Container exits instantly with code 0 (`Exited (0)`) | Always set `daemon off;` in container CMD or nginx.conf |
| Logging to Regular Files in Containers | NGINX writes access logs to disk files inside the container | Container overlay filesystem fills to 100%; `docker logs` is empty | Symlink log files to `/dev/stdout` and `/dev/stderr` |
| Variable Mangling via `envsubst` | Running bare `envsubst` replaces NGINX variables (`$host`, `$request_uri`) with empty strings | Broken proxying; configuration syntax corruption | Pass explicit variable list: `envsubst '$HOST $PORT' < in > out` |
| Single File Bind-Mount Inode Desync | Mounting a single file (`-v ./nginx.conf:/etc/nginx/nginx.conf`) and editing on host | Host text editor creates new inode; container retains old stale config | Mount the entire configuration directory instead of single files |
| Running as Root on Port 80 in Kubernetes | Violates CIS Docker and Pod Security Standards (`RunAsNonRoot`) | Pod fails admission control in hardened clusters | Use `nginxinc/nginx-unprivileged` listening on 8080 |

---

## 8. Security, Non-Root Hardening & Read-Only Filesystems

Modern zero-trust container runtimes enforce **read-only root filesystems** (`readOnlyRootFilesystem: true`). NGINX requires write access to specific temporary directories to buffer client request bodies, fastcgi buffers, and write its PID file.

### Hardened Read-Only Docker Run Configuration
```bash
docker run -d \
  --name secure-nginx \
  --read-only \
  --cap-drop ALL \
  --security-opt no-new-privileges:true \
  --user 101:101 \
  -p 8080:8080 \
  --tmpfs /tmp:rw,noexec,nosuid,size=64m \
  --tmpfs /var/cache/nginx:rw,noexec,nosuid,size=128m \
  --tmpfs /var/run:rw,noexec,nosuid,size=10m \
  nginxinc/nginx-unprivileged:1.26-alpine
```

### Required `nginx.conf` Adjustments for Read-Only Runtimes
```nginx
# /etc/nginx/nginx.conf
pid /tmp/nginx.pid;

http {
    # Direct temporary buffer paths to writable tmpfs volumes
    client_body_temp_path /tmp/client_temp;
    proxy_temp_path       /tmp/proxy_temp;
    fastcgi_temp_path     /tmp/fastcgi_temp;
    uwsgi_temp_path       /tmp/uwsgi_temp;
    scgi_temp_path        /tmp/scgi_temp;
}
```

---

## 9. Architectural Comparison: Bare-Metal / VM vs Dockerized NGINX

| Operational Dimension | Bare-Metal / Virtual Machine | Docker / Containerized NGINX |
| :--- | :--- | :--- |
| **Process Daemonization**| Runs in background as systemd service (`daemon on;`)| Runs in foreground as PID 1 (`daemon off;`)|
| **Log Management** | `logrotate` rotates disk files in `/var/log/nginx/` | Streamed to `/dev/stdout` and `/dev/stderr` |
| **Config Updates** | `systemctl reload nginx` (in-place `kill -HUP`) | Rolling container replacement or mounted reload |
| **Resource Allocation** | System-wide kernel limits (`sysctl.conf`) | Cgroups limits (`cpu.shares`, `memory.limit_in_bytes`)|
| **User Privileges** | Master runs as `root`, workers as `www-data` | Master and workers both run as UID 101 (`unprivileged`) |
| **Port Binding** | Privileged ports 80 and 443 | Unprivileged ports (8080, 8443) mapped to host ports |

---

## 10. Technical Interview Questions & Scenarios

<details>
<summary><strong>1. Why is `STOPSIGNAL SIGQUIT` essential in an NGINX Dockerfile?</strong></summary>

By default, Docker sends `SIGTERM` when stopping a container (`docker stop`). When the NGINX master process receives `SIGTERM`, it executes a quick shutdown, immediately closing listening sockets and forcefully terminating active worker processes. Any requests currently in-flight are abruptly severed, throwing HTTP 502 errors to clients. In contrast, `STOPSIGNAL SIGQUIT` instructs Docker to send `SIGQUIT`. NGINX handles `SIGQUIT` by executing a **graceful shutdown**: it closes listening sockets to reject new connections, allows worker processes to finish serving existing client requests to completion, and only exits once all in-flight transactions are done.
</details>

<details>
<summary><strong>2. Why does an NGINX container exit immediately if `daemon off;` is omitted?</strong></summary>

Docker containers remain in the `RUNNING` state only as long as the initial process (PID 1) defined in `ENTRYPOINT` or `CMD` continues executing. By default, NGINX is engineered as a traditional daemon: the master process forks child worker processes and the parent exits, returning control to the shell. In Docker, when that initial foreground process exits, the Docker container engine detects that PID 1 has terminated and immediately kills the container namespace. Configuring `daemon off;` forces the NGINX master process to remain running in the foreground as PID 1.
</details>

<details>
<summary><strong>3. How does the official NGINX image prevent environment variable substitution from corrupting internal NGINX variables like `$host`?</strong></summary>

The official entrypoint script (`20-envsubst-on-templates.sh`) uses `envsubst`. If standard `envsubst` is run without arguments, it replaces *all* strings starting with `$`—including internal NGINX variables like `$host`, `$uri`, and `$remote_addr`—with empty strings, corrupting the configuration. To avoid this, the entrypoint script dynamically queries the environment to extract the names of explicitly defined environment variables and passes them as a whitelist to `envsubst` (e.g. `envsubst '$API_HOST $API_PORT' < template > output`), leaving native NGINX variables completely untouched.
</details>

<details>
<summary><strong>4. How do you implement a non-root NGINX container that can still bind to port 80?</strong></summary>

Linux kernels restrict binding to ports below 1024 to privileged users (`root`). To run as a non-root user (e.g. UID 101) while binding port 80:
1. Grant the Linux capability `CAP_NET_BIND_SERVICE` to the NGINX binary:
   ```bash
   setcap 'cap_net_bind_service=+ep' /usr/sbin/nginx
   ```
2. Or, configure Docker/Kubernetes sysctl to lower the unprivileged port ceiling:
   ```yaml
   securityContext:
     sysctls:
       - name: net.ipv4.ip_unprivileged_port_start
         value: "0"
   ```
3. The modern standard practice is simply to configure NGINX to listen on port 8080 and use Docker port mapping (`-p 80:8080`) or Kubernetes Service port mapping (`port: 80, targetPort: 8080`).
</details>

<details>
<summary><strong>5. How do you perform a zero-downtime configuration reload in a running Docker container?</strong></summary>

Execute an `nginx -s reload` signal directly inside the running container:
```bash
docker exec -it <container_name> nginx -t && docker exec -it <container_name> nginx -s reload
```
The master process validates the new configuration and forks new workers running the new configuration while old workers gracefully drain active connections.
</details>

<details>
<summary><strong>6. What happens if you run an NGINX container with `--read-only` without specifying `--tmpfs` mounts?</strong></summary>

NGINX will fail on startup or crash upon receiving the first client POST request. Even when serving static files, NGINX attempts to write its process ID file (`/var/run/nginx.pid` or `/run/nginx.pid`) and create buffer directories for client request bodies (`/var/cache/nginx/client_temp`). On a read-only filesystem, these write syscalls fail with `EROFS (Read-only file system)`, causing worker panics. You must mount writable `tmpfs` volumes at `/tmp`, `/var/cache/nginx`, and `/var/run`.
</details>

<details>
<summary><strong>7. Why should you avoid `nginx:latest` in production Dockerfiles?</strong></summary>

The `latest` tag floats and tracks the newest mainline development releases. A rebuild in CI/CD may unexpectedly pull a major or minor version bump introducing breaking configuration changes, deprecated directives, or binary incompatibilities with existing dynamic modules. Production Dockerfiles should pin explicit version tags (e.g. `nginx:1.26.2-alpine`) and manage upgrades through deterministic pull requests.
</details>

<details>
<summary><strong>8. What is the purpose of the `HEALTHCHECK` instruction in an NGINX Dockerfile?</strong></summary>

The `HEALTHCHECK` instruction informs Docker and container orchestrators (Docker Swarm, Kubernetes, Nomad) how to test whether the NGINX service is genuinely operational. While the master process might remain alive as PID 1, workers could be deadlocked or unable to connect to upstreams. An HTTP probe (e.g. `wget -q --spider http://localhost:8080/health`) verifies that the event loop is active and returning valid responses. If probes fail, the orchestrator marks the container `unhealthy` and automates pod replacement.
</details>

---

## 11. Hands-On Practice Exercises

<details>
<summary><strong>Exercise 1: Build and Run a Non-Root NGINX Container</strong></summary>

**Goal**: Verify rootless execution and port binding.
1. Create `Dockerfile` based on `nginxinc/nginx-unprivileged:1.26-alpine`.
2. Add a simple `index.html` returning `"Hello from rootless container"`.
3. Build and run:
```bash
docker build -t rootless-nginx .
docker run -d --name test-rootless -p 8080:8080 rootless-nginx
```
4. Verify user ID inside container:
```bash
docker exec -it test-rootless id
# Output should show: uid=101(nginx) gid=101(nginx)
```
</details>

<details>
<summary><strong>Exercise 2: Test Graceful Shutdown (`SIGQUIT`) vs Fast Shutdown (`SIGTERM`)</strong></summary>

**Goal**: Observe connection dropping during ungraceful container termination.
1. Launch an NGINX container with a slow response endpoint (e.g. `location /slow { echo_sleep 10; echo "done"; }` or proxying a slow mock).
2. Start a curl request in background: `curl http://localhost:8080/slow`.
3. In terminal 2, issue `docker kill -s SIGTERM <container_id>`. Observe the curl request immediately terminating with `curl: (52) Empty reply from server`.
4. Repeat using `docker kill -s SIGQUIT <container_id>` and observe curl completing successfully before the container exits.
</details>

<details>
<summary><strong>Exercise 3: Test Dynamic Environment Variable Templating</strong></summary>

**Goal**: Use `20-envsubst-on-templates.sh` to configure upstreams dynamically.
1. Create `templates/default.conf.template` containing `${PROXY_UPSTREAM}`.
2. Launch container:
```bash
docker run -d -p 8080:8080 \
  -e PROXY_UPSTREAM=example.com \
  -v "$PWD/templates:/etc/nginx/templates:ro" \
  nginxinc/nginx-unprivileged:1.26-alpine
```
3. Inspect generated configuration:
```bash
docker exec -it <container_id> cat /etc/nginx/conf.d/default.conf
```
4. Confirm `${PROXY_UPSTREAM}` was replaced by `example.com`.
</details>

<details>
<summary><strong>Exercise 4: Deploy with Completely Read-Only Root Filesystem</strong></summary>

**Goal**: Verify a hardened zero-write container deployment.
1. Run the container with `--read-only` and the specific `--tmpfs` mounts listed in Section 8.
2. Verify NGINX starts cleanly and serves HTTP requests.
3. Test writing to root: `docker exec -it <container_id> touch /test` -> Confirm `Read-only file system` error.
</details>

<details>
<summary><strong>Exercise 5: Live Hot-Reload Without Container Restart</strong></summary>

**Goal**: Update configuration without restarting or dropping connections.
1. Mount a host directory to `/etc/nginx/conf.d`.
2. Edit a virtual host configuration file on the host.
3. Run: `docker exec <container_id> nginx -t && docker exec <container_id> nginx -s reload`.
4. Inspect `docker logs <container_id>` and verify that graceful reload was logged.
</details>

<details>
<summary><strong>Exercise 6: Author Docker Compose Healthcheck and Dependency Gating</strong></summary>

**Goal**: Prevent frontend containers from starting until NGINX reports healthy.
1. Implement the Docker Compose stack from Example 3.
2. Add a `healthcheck` stanza to the gateway service.
3. Launch with `docker-compose up -d` and inspect status via `docker-compose ps`.
</details>

---

## 12. Summary & Quick Reference

```
+--------------------------------------------------------------------------------+
| Containerized NGINX Directive & Command Cheat Sheet                            |
|                                                                                |
| Directive / Parameter         | Location        | Purpose                      |
| :---------------------------- | :-------------- | :--------------------------- |
| daemon off;                   | nginx.conf/CMD  | Keeps container PID 1 alive  |
| STOPSIGNAL SIGQUIT            | Dockerfile      | Enforces graceful drain      |
| access_log /dev/stdout;       | nginx.conf      | Routes logs to container CLI |
| error_log /dev/stderr warn;   | nginx.conf      | Routes errors to container   |
| pid /tmp/nginx.pid;           | nginx.conf      | Allows non-root execution    |
| /etc/nginx/templates/*.template| Directory       | Auto-processed by envsubst   |
| docker exec <id> nginx -s reload| Host CLI      | Zero-downtime hot reload     |
+--------------------------------------------------------------------------------+
```

---

## 13. Further Reading

- [Official NGINX Docker Hub Repository](https://hub.docker.com/_/nginx)
- [NGINX Unprivileged Docker Images](https://github.com/nginxinc/docker-nginx-unprivileged)
- [Docker Best Practices: Understanding PID 1 and Signal Handling](https://docs.docker.com/develop/develop-images/dockerfile_best-practices/)
- [Next Concept: Kubernetes Ingress Controller & Gateway API](02-kubernetes-ingress-and-gateway-api.md)

---

[← Back to Module Index](README.md) | [Next Note: 02-kubernetes-ingress-and-gateway-api.md →](02-kubernetes-ingress-and-gateway-api.md)
