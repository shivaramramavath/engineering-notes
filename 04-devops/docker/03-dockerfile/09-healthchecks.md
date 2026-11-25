# Container Healthchecks: HEALTHCHECK Instruction and Runtime Probing

```
Track: DevOps & Containerization
Module: 03-dockerfile
Prerequisites: 03-dockerfile/01-dockerfile-basics.md, 02-commands/01-run-ps-stop.md
Difficulty: Intermediate
Estimated Duration: 40 minutes
Target Docker Version: 27.x+
Last Validated: February 2025
```

---

## Table of Contents
1. [What & Why](#1-what--why)
2. [Core Concepts](#2-core-concepts)
3. [How It Works Internally](#3-how-it-works-internally)
4. [Syntax & Directive Reference](#4-syntax--directive-reference)
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

By default, Docker monitors only whether a container's root process (PID 1) is alive. If PID 1 enters a deadlocked state, an infinite loop, runs out of database connections, or encounters a stalled event loop, Docker continues to report the container status as `Up (healthy)` because the process has not terminated.

The `HEALTHCHECK` instruction allows developers to define an active, in-container probing mechanism that validates whether the application is genuinely capable of handling user traffic.
- **Automated Failure Detection**: Surfaces internal deadlocks, socket exhaustion, or upstream connection loss.
- **Orchestration Integration**: Enables Docker Compose and Docker Swarm to delay routing traffic until a service is fully initialized (`condition: service_healthy`), and automatically trigger container restarts or failovers when unhealthy.
- **Graceful Startup Handling**: Distinguishes between slow initial warmup phases (e.g., Spring Boot JVM cold start, database schema migration) and true runtime degradation.

---

## 2. Core Concepts

### Health States
A container with a defined healthcheck traverses three distinct lifecycle states:
1. `starting`: Initial state upon container boot while the grace period or first probe runs.
2. `healthy`: The probe command exited with return code `0`.
3. `unhealthy`: The probe command failed consecutive times matching or exceeding the configured `retries` threshold.

### Exit Codes
The probing command executed by Docker must follow standard POSIX exit status conventions:
- **`0`**: Success — container is healthy and ready for traffic.
- **`1`**: Failure — container is unhealthy or failing validation.
- **`2`**: Reserved — do not use this exit code in custom scripts.

### Healthcheck Timing Parameters
- **`--interval=<duration>`**: Time between probe executions (default: `30s`).
- **`--timeout=<duration>`**: Maximum time allowed for a single probe command to execute before being counted as a failure (default: `30s`).
- **`--start-period=<duration>`**: Initialization grace period during which failed probes do not count toward the `retries` limit (default: `0s`).
- **`--start-interval=<duration>`**: (Introduced in Docker 25.0) Time between probe executions *during* the `start-period`, allowing rapid detection of successful boots without waiting for the full runtime interval (default: `5s`).
- **`--retries=<count>`**: Number of consecutive probe failures required to mark the container `unhealthy` (default: `3`).

---

## 3. How It Works Internally

### Probing Lifecycle & State Machine

```
                      +-------------------+
                      |   docker run      |
                      +---------+---------+
                                |
                                v
                      +-------------------+
                      | State: STARTING   |
                      +---------+---------+
                                |
             +------------------+------------------+
             |                                     |
    Probe returns 0                       Probe returns 1 (failure)
             |                                     |
             v                                     v
+------------------------+             +------------------------+
|    State: HEALTHY      |<------------| Still in start-period? |
+-----------+------------+   Probe = 0 +-----------+------------+
            |                              | Yes           | No
            | Probe fails                  v               v
            | consecutive              Stay in        Fail count++
            | >= retries               STARTING       Consecutive >= retries?
            v                                         |
+------------------------+                            v
|   State: UNHEALTHY     |                     +------------------------+
+------------------------+                     |   State: UNHEALTHY     |
                                               +------------------------+
```

### Daemon Execution Architecture
The Docker daemon (`dockerd`) runs a timer per active container. When the timer triggers:
1. The daemon executes `containerd-shim` -> `runc exec` inside the container namespaces (mount, network, PID).
2. The specified command runs as the container's declared `USER` (or root if omitted).
3. The daemon captures the exit code and up to 2KB of standard output/error, storing it in the container inspect metadata under `.State.Health.Log`.

---

## 4. Syntax & Directive Reference

### Dockerfile Syntax

```dockerfile
# Option 1: Custom Healthcheck Probe
HEALTHCHECK [OPTIONS] CMD command

# Examples:
HEALTHCHECK --interval=10s --timeout=3s --start-period=30s --retries=3 \
    CMD curl -f http://localhost:8080/healthz || exit 1

# Exec form (avoids subshell, recommended for Distroless with custom probe binary):
HEALTHCHECK --interval=15s --timeout=5s \
    CMD ["/app/healthcheck-binary"]

# Option 2: Disable Healthcheck Inherited from Base Image
HEALTHCHECK NONE
```

### Docker Run CLI Overrides

```bash
docker run -d \
  --name api-service \
  --health-cmd="curl -f http://localhost:3000/livez || exit 1" \
  --health-interval=10s \
  --health-timeout=2s \
  --health-retries=3 \
  --health-start-period=15s \
  --health-start-interval=2s \
  my-api:v1
```

### Inspecting Health Status

```bash
# Get brief status (healthy, unhealthy, starting)
docker inspect --format='{{.State.Health.Status}}' <container_id>

# Get full log of recent probe runs, exit codes, and outputs
docker inspect --format='{{json .State.Health}}' <container_id> | jq .
```

---

## 5. Practical Implementation & Examples

### Example 1: Web API Healthcheck using `curl`
Checking an HTTP status code with fail-on-error (`-f`) and silent output (`-s`).

```dockerfile
FROM node:22-bookworm-slim
WORKDIR /app
RUN apt-get update && apt-get install -y --no-install-recommends \
    curl \
 && rm -rf /var/lib/apt/lists/*

COPY package*.json ./
RUN npm ci --omit=dev
COPY . .

USER node
EXPOSE 3000

HEALTHCHECK --interval=15s --timeout=3s --start-period=10s --retries=3 \
    CMD curl -f -s http://127.0.0.1:3000/health || exit 1

CMD ["node", "index.js"]
```

### Example 2: Lightweight Native Healthcheck without `curl` or `wget`
In minimal images (Debian slim or Alpine), avoid installing `curl` just for healthchecks by using native scripting language capabilities.

**Node.js native probe:**
```dockerfile
HEALTHCHECK --interval=10s --timeout=2s --retries=3 \
    CMD node -e "require('http').get('http://127.0.0.1:3000/health', (r) => { process.exit(r.statusCode === 200 ? 0 : 1); }).on('error', () => process.exit(1));"
```

**Python native probe:**
```dockerfile
HEALTHCHECK --interval=10s --timeout=2s --retries=3 \
    CMD python3 -c "import urllib.request, sys; sys.exit(0 if urllib.request.urlopen('http://127.0.0.1:8000/health').getcode() == 200 else 1)"
```

### Example 3: Compiled Go Healthcheck Binary in Distroless / Scratch
Since Distroless contains neither `curl`, `bash`, nor `node`, compile a secondary tiny Go probe binary into the image.

```dockerfile
# Builder Stage
FROM golang:1.24-alpine AS builder
WORKDIR /src
COPY . .
RUN CGO_ENABLED=0 go build -ldflags="-s -w" -o /bin/server ./cmd/server
RUN CGO_ENABLED=0 go build -ldflags="-s -w" -o /bin/probe ./cmd/healthcheck

# Runtime Stage (Distroless)
FROM gcr.io/distroless/static-debian12:nonroot
WORKDIR /app
COPY --from=builder /bin/server /app/server
COPY --from=builder /bin/probe /app/probe

USER nonroot
EXPOSE 8080

HEALTHCHECK --interval=5s --timeout=2s --start-period=5s --retries=2 \
    CMD ["/app/probe"]

ENTRYPOINT ["/app/server"]
```

Where `cmd/healthcheck/main.go` is:
```go
package main
import (
    "net/http"
    "os"
)
func main() {
    resp, err := http.Get("http://127.0.0.1:8080/healthz")
    if err != nil || resp.StatusCode != http.StatusOK {
        os.Exit(1)
    }
    os.Exit(0)
}
```

### Example 4: Database Ready Probe (PostgreSQL & Redis)
Validating TCP and protocol-level availability.

**PostgreSQL Custom Probe:**
```dockerfile
HEALTHCHECK --interval=5s --timeout=3s --retries=5 \
    CMD pg_isready -U postgres -d mydb || exit 1
```

**Redis Custom Probe:**
```dockerfile
HEALTHCHECK --interval=5s --timeout=2s --retries=3 \
    CMD redis-cli ping | grep PONG || exit 1
```

### Example 5: Docker Compose Orchestration with `condition: service_healthy`
Preventing downstream web application from booting until the database is genuinely ready to accept connections.

```yaml
services:
  db:
    image: postgres:17-alpine
    environment:
      POSTGRES_DB: orders
      POSTGRES_PASSWORD: secretpassword
    healthcheck:
      test: ["CMD-SHELL", "pg_isready -U postgres -d orders"]
      interval: 5s
      timeout: 3s
      retries: 5
      start_period: 10s

  api:
    build: .
    ports:
      - "8080:8080"
    depends_on:
      db:
        condition: service_healthy
    restart: on-failure
```

---

## 6. Production Best Practices & Antipatterns

### Best Practices
- **Implement Deep vs Shallow Probes Appropriately**:
  - A *liveness probe* (container healthcheck) should only check local application state (process running, memory ok, event loop not blocked).
  - Do **not** fail local container health if an external third-party API is down, as restarting the container will not heal the third-party outage and will cause cascading restart storms.
- **Set Realistic `start-period`**:
  - JVM applications (Spring Boot) often require 20-45 seconds of initial JIT compilation and database migration. Without `start-period: 60s`, Docker marks the container unhealthy before it finishes loading.
- **Use `127.0.0.1` or `localhost`**:
  - Always probe `http://127.0.0.1:<port>`. Probing via public DNS or bridge hostnames introduces dependency on external DNS resolvers.
- **Keep Health Probes Ultra-Lightweight**:
  - Probes run repeatedly across all containers every 5-30s. If a health endpoint performs full database table scans or complex crypto, it will consume significant CPU cycles.

### Anti-Patterns
- **Forking Heavy Processes Every 5 Seconds**: Running `bash -c "curl ..."` every 3 seconds inside hundreds of containers spawns hundreds of subshells, spiking kernel context switches and fork overhead.
- **Assuming Unhealthy Restarts Containers in Standalone Docker**: Standalone Docker (`docker run`) **never restarts an unhealthy container automatically**. It merely tags the status as `unhealthy`. Automated restarts require Docker Compose (with an external watcher), Docker Swarm, or Kubernetes.
- **Ignoring Healthcheck Output Limits**: Docker stores only the last 2048 bytes of probe output. Verbose probes pollute disk logs.

---

## 7. Common Pitfalls & Troubleshooting

| Symptom | Root Cause | Diagnosis Command | Permanent Fix |
|:---|:---|:---|:---|
| Container stays in `(unhealthy)` state continuously | Health command returns non-zero exit code or fails with command not found | `docker inspect --format='{{json .State.Health}}' <id>` | Inspect `.Output` field; ensure `curl` or probe binary is installed. |
| Container marked `unhealthy` during application boot | `start-period` is too short; probes fail before JVM/framework finishes initialization | Compare boot timestamp against first failed probe timestamp | Increase `--start-period=45s` and use `--start-interval=3s`. |
| `docker run` reports container `unhealthy`, but container never restarts | Standalone Docker daemon has no built-in auto-restart for unhealthy state | Check `docker ps` status | Use Docker Swarm, Kubernetes, or run an auto-healer sidecar like `autoheal`. |
| Heavy CPU utilization on idle host | Dozens of containers running healthchecks with `--interval=1s` | `docker stats` and host `top` | Increase `--interval` to `15s` or `30s` in production. |
| Compose `depends_on` fails with syntax error | Used simple list syntax instead of dict syntax for `condition: service_healthy` | Inspect `docker compose up` error | Configure `depends_on: { service: { condition: service_healthy } }`. |
| Distroless container reports `/bin/sh: not found` during healthcheck | Healthcheck specified in shell form `CMD curl ...` inside image with no shell | Check inspect output: `OCI runtime exec failed` | Use Exec form: `CMD ["/app/probe"]`. |

---

## 8. Performance Tuning & Resource Optimization

1. **Leverage Docker 25+ `--start-interval`**:
   - In older versions, setting `--interval=30s` meant that even with `--start-period=60s`, a fast-booting service (booting in 2s) would wait up to 30s before the first probe ran to transition to `healthy`.
   - With `--start-interval=2s`, Docker probes every 2s until the first pass, then drops back to the 30s interval, slashing CI/CD test pipeline runtimes.
2. **Dedicated Health Endpoint in Code**:
   - Implement `/healthz` returning HTTP 200 with raw bytes `OK`. Bypass expensive authentication middleware, session lookups, and serialization.

---

## 9. Security Hardening & Threat Model

- **Avoid Exposing Health Endpoints Publicly**:
  - Detailed `/health` endpoints often expose internal versions, memory stats, database hostnames, and queue depths. Ensure reverse proxies (Nginx, Traefik) block external access to `/health` or bind health probes exclusively to `127.0.0.1`.
- **Do Not Run Healthchecks as Root**:
  - The healthcheck command executes under the container's current `USER`. If your container runs as an unprivileged user, ensure the healthcheck binary or probe script does not attempt privileged operations.

---

## 10. Architectural Comparison Table

| Probing Approach | Footprint Overhead | Tooling Dependency | Accuracy | Shell Required? |
|:---|:---|:---|:---|:---|
| `curl -f` | ~10 MB package overhead | Requires `curl` & `libcurl` | High (HTTP level) | Yes (unless exec form)|
| `wget -q -O -` | ~1.5 MB (BusyBox) | Requires BusyBox/wget | Medium | Yes |
| Language Native (`node`, `python`)| 0 MB extra | Language runtime | High | No |
| Static Go Binary (`/app/probe`) | ~1.5 MB static file | None (Zero dependencies)| Maximum | **No (Works in scratch)**|
| TCP Port Check (`nc` / `/dev/tcp`)| ~1 MB | Netcat or Bash | Low (socket open only)| Yes |

---

## 11. Real-World Scenario Walkthrough

### Scenario: Cascading Deployment Failure in Microservice Cluster

#### Problem Statement
An e-commerce payments service containerized with Spring Boot was deployed to production. The continuous deployment pipeline pushed 5 new containers. The load balancer immediately routed 20% of traffic to the new instances. However, Spring Boot takes 28 seconds to initialize Hibernate and Liquibase migrations. All incoming customer transactions routed to the new containers crashed with `HTTP 502 Bad Gateway`, causing $40,000 in failed transactions.

#### Root Cause Analysis
The image Dockerfile had no `HEALTHCHECK`. Docker immediately reported `Up 2 seconds` upon process execution, signaling to the reverse proxy that the container was ready.

#### Remediation
1. Implemented Spring Boot Actuator `/actuator/health/readiness`.
2. Updated Dockerfile:
   ```dockerfile
   HEALTHCHECK --interval=10s --timeout=3s --start-period=35s --start-interval=2s --retries=3 \
       CMD wget -q --spider http://127.0.0.1:8080/actuator/health/readiness || exit 1
   ```
3. Configured orchestration rolling update policy to wait for `healthy` status before terminating old instances.
4. Result: Zero dropped requests during subsequent deployments.

---

## 12. Self-Assessment & Interview Questions

<details>
<summary><strong>1. What happens when a container's HEALTHCHECK fails in standard standalone Docker (`docker run`)? Does Docker restart it?</strong></summary>

**Answer:**
No. Standalone Docker (`dockerd`) does **not** terminate or restart an unhealthy container. It only updates the container metadata status to `unhealthy`, which can be viewed in `docker ps` and `docker inspect`. Restarting or replacing unhealthy containers requires an external orchestrator (such as Docker Swarm, Kubernetes, AWS ECS) or a monitoring container (such as `autoheal`).
</details>

<details>
<summary><strong>2. Explain the purpose and behavior of `--start-period` and `--start-interval`.</strong></summary>

**Answer:**
`--start-period` provides initialization time for containers that need time to bootstrap (e.g., loading caches, database migrations). Failures during this window do not count toward the maximum `retries` count. However, if a probe succeeds during `start-period`, the container immediately becomes `healthy`. `--start-interval` (Docker 25+) sets a faster probe frequency specifically during the `start-period` so healthy status is reached as fast as possible.
</details>

<details>
<summary><strong>3. How does the `HEALTHCHECK NONE` directive work, and when should it be used?</strong></summary>

**Answer:**
`HEALTHCHECK NONE` is used in a downstream Dockerfile to explicitly disable or remove a `HEALTHCHECK` inherited from a parent base image. For instance, if an official base image defines an HTTP healthcheck on port 80, but your application acts solely as a background queue worker without an HTTP server, `HEALTHCHECK NONE` prevents the container from being falsely flagged as unhealthy.
</details>

<details>
<summary><strong>4. Why is executing `curl -f` preferable to standard `curl` in healthcheck commands?</strong></summary>

**Answer:**
By default, standard `curl http://localhost:8080/error` returns an exit code of `0` even if the server responds with `HTTP 500 Internal Server Error`, because the HTTP transfer itself completed successfully. The `-f` (`--fail`) flag forces curl to return exit code `22` (non-zero) on server HTTP errors (4xx/5xx), ensuring Docker correctly registers the failure.
</details>

<details>
<summary><strong>5. What is the difference between shallow health checks (liveness) and deep health checks (readiness)? Which should be placed in a Dockerfile?</strong></summary>

**Answer:**
A shallow check (liveness) verifies that the container process itself is responsive (event loop healthy, memory allocated, internal threads alive). A deep check verifies end-to-end functionality, including database connectivity, caches, and third-party APIs. A Dockerfile `HEALTHCHECK` should almost always be a shallow/liveness probe. If a deep check fails due to an external database blip, restarting the container does not fix the database and can lead to thundering herd failures across an entire fleet.
</details>

<details>
<summary><strong>6. How can you define a healthcheck inside a Distroless or `scratch` container where no shell (`/bin/sh`) or `curl` exists?</strong></summary>

**Answer:**
Use the **Exec form** (`CMD ["/executable", "args..."]`) with a dedicated statically compiled healthcheck binary (written in Go, Rust, or C) compiled during the multi-stage build and copied into the final image. This completely avoids the need for a shell interpreter or external networking tools like `curl`.
</details>

<details>
<summary><strong>7. How does Docker Compose make use of healthchecks in multi-service configurations?</strong></summary>

**Answer:**
Through the `depends_on` attribute with `condition: service_healthy`. Instead of starting dependent services immediately after the prerequisite container process starts, Docker Compose blocks the startup of dependent services until the prerequisite service's healthcheck explicitly transitions to the `healthy` state.
</details>

<details>
<summary><strong>8. Where does Docker store healthcheck failure logs, and how can an engineer retrieve the exact error message?</strong></summary>

**Answer:**
Docker stores the exit code and up to 2048 bytes of output in the container's runtime state. It can be retrieved using:
`docker inspect --format='{{json .State.Health.Log}}' <container_id> | jq .`
</details>

---

## 13. Hands-on Practice Exercises

<details>
<summary><strong>Exercise 1: Creating an HTTP Healthcheck with Failover Simulation</strong></summary>

**Goal:** Build a Node.js container with a toggleable health route, observe the transition from `starting` to `healthy` to `unhealthy`.

**Step-by-step Solution:**
1. Create `app.js`:
   ```javascript
   const http = require('http');
   let healthy = true;
   const server = http.createServer((req, res) => {
       if (req.url === '/health') {
           res.writeHead(healthy ? 200 : 500);
           res.end(healthy ? 'OK' : 'FAIL');
       } else if (req.url === '/break') {
           healthy = false;
           res.end('Health broken');
       } else {
           res.end('Hello World');
       }
   });
   server.listen(3000, () => console.log('Listening on 3000'));
   ```
2. Create `Dockerfile`:
   ```dockerfile
   FROM node:22-alpine
   WORKDIR /app
   COPY app.js .
   HEALTHCHECK --interval=3s --timeout=1s --retries=2 \
       CMD wget -q --spider http://127.0.0.1:3000/health || exit 1
   CMD ["node", "app.js"]
   ```
3. Build and run:
   ```bash
   docker build -t health-toggle-demo .
   docker run -d --name test-health -p 3000:3000 health-toggle-demo
   ```
4. Check status transition:
   ```bash
   docker ps # Observe "starting" -> "healthy"
   curl http://localhost:3000/break
   sleep 7
   docker ps # Observe "unhealthy"
   ```
</details>

<details>
<summary><strong>Exercise 2: Docker Compose Order Enforcement with `service_healthy`</strong></summary>

**Goal:** Configure a Redis database and a Node.js worker such that the worker will refuse to boot until Redis passes its healthcheck.

**Step-by-step Solution:**
1. Create `docker-compose.yml`:
   ```yaml
   services:
     redis:
       image: redis:7-alpine
       healthcheck:
         test: ["CMD", "redis-cli", "ping"]
         interval: 2s
         timeout: 2s
         retries: 3
     worker:
       image: alpine:3.21
       depends_on:
         redis:
           condition: service_healthy
       command: ["sh", "-c", "echo 'Redis is healthy! Connecting...' && sleep 3600"]
   ```
2. Run `docker compose up` and observe startup sequence in the console output.
</details>

<details>
<summary><strong>Exercise 3: Native Python Health Probe without Dependencies</strong></summary>

**Goal:** Create a lightweight Python FastAPI image that runs an in-container Python one-liner probe without installing `curl`.

**Step-by-step Solution:**
1. Create `Dockerfile`:
   ```dockerfile
   FROM python:3.12-slim-bookworm
   WORKDIR /app
   RUN pip install --no-cache-dir fastapi uvicorn
   RUN echo "from fastapi import FastAPI\napp = FastAPI()\n@app.get('/health')\ndef h(): return {'status':'ok'}" > main.py
   
   HEALTHCHECK --interval=5s --timeout=2s --retries=2 \
       CMD python3 -c "import urllib.request; urllib.request.urlopen('http://127.0.0.1:8000/health')" || exit 1

   CMD ["uvicorn", "main:app", "--host", "0.0.0.0", "--port", "8000"]
   ```
2. Build and verify health status via `docker inspect`.
</details>

<details>
<summary><strong>Exercise 4: Overriding Base Image Healthcheck to None</strong></summary>

**Goal:** Pull an image that defines a default healthcheck and build a child image that disables it completely.

**Step-by-step Solution:**
1. Create `Dockerfile`:
   ```dockerfile
   FROM elasticsearch:8.17.0
   HEALTHCHECK NONE
   ```
2. Build and run:
   ```bash
   docker build -t es-no-health .
   docker run -d --name test-no-health es-no-health
   docker inspect --format='{{.State.Health}}' test-no-health
   # Returns <nil>
   ```
</details>

<details>
<summary><strong>Exercise 5: Inspecting Probe Failure Logs via CLI</strong></summary>

**Goal:** Write a failing healthcheck command and use `jq` to parse the exact error output captured by the Docker daemon.

**Step-by-step Solution:**
1. Run container with intentionally failing probe:
   ```bash
   docker run -d --name fail-probe \
     --health-cmd="cat /nonexistent/file || exit 1" \
     --health-interval=2s \
     --health-retries=1 \
     alpine:3.21 sleep 300
   ```
2. Wait 4 seconds, then query the health log:
   ```bash
   docker inspect --format='{{json .State.Health.Log}}' fail-probe | jq '.[-1]'
   ```
3. Observe output:
   ```json
   {
     "Start": "...",
     "End": "...",
     "ExitCode": 1,
     "Output": "cat: can't open '/nonexistent/file': No such file or directory\n"
   }
   ```
</details>

<details>
<summary><strong>Exercise 6: Micro Go Probe for Distroless Container</strong></summary>

**Goal:** Compile a 1MB Go binary dedicated exclusively to checking an HTTP endpoint and verify it executes inside Distroless nonroot.

**Step-by-step Solution:**
1. Create `probe.go`:
   ```go
   package main
   import ("net/http"; "os")
   func main() {
       r, err := http.Get("http://127.0.0.1:8080/health")
       if err != nil || r.StatusCode != 200 { os.Exit(1) }
       os.Exit(0)
   }
   ```
2. Multi-stage Dockerfile:
   ```dockerfile
   FROM golang:1.24-alpine AS builder
   WORKDIR /app
   COPY probe.go .
   RUN CGO_ENABLED=0 go build -ldflags="-s -w" -o probe probe.go

   FROM gcr.io/distroless/static-debian12:nonroot
   COPY --from=builder /app/probe /probe
   USER nonroot
   HEALTHCHECK CMD ["/probe"]
   ENTRYPOINT ["/probe"]
   ```
3. Build and test execution.
</details>

---

## Summary & Reference Cheat Sheet

### Syntax Quick Reference
```dockerfile
HEALTHCHECK [OPTIONS] CMD command
# Options:
--interval=30s      # Time between probes
--timeout=30s       # Max probe duration
--start-period=0s   # Grace period on startup
--start-interval=5s # Fast probing during startup (Docker 25+)
--retries=3         # Failures before marking unhealthy
```

### Exit Code Rules
- `0` = Healthy
- `1` = Unhealthy
- `2` = Reserved (avoid)

### Docker Compose Reference
```yaml
depends_on:
  service_name:
    condition: service_healthy
```

---

[← Previous: 08-base-image-choice.md](08-base-image-choice.md) | [Next: 10-language-recipes.md →](10-language-recipes.md)
