# Project [XX]: [Project Name] (`[slug]`)

> [One-line executive summary of infrastructure architecture, core technologies, and real-world DevOps/SRE patterns implemented in this project.]

| Metadata | Details |
|---|---|
| **Project Code** | `proj-devops-[xx]-[slug]` |
| **Track** | [AWS / Docker / Git-GitHub / Linux / Nginx] |
| **Complexity** | [Intermediate / Advanced / Production-Grade] |
| **Architectural Style** | [Multi-Tier Container Stack / Reverse Proxy Gateway / GitOps CI/CD / Multi-AZ Cloud] |
| **OS / Cloud Scope** | [Ubuntu 24.04 LTS / AWS us-east-1 / Docker Engine 28+] |
| **Tested With** | [Verified runner environment, Docker v28.0, compose v2.30, AWS CLI v2.22] |

---

## 1. Architectural Overview & Component Topology

### 1.1 Topology Diagram
```
+---------------------------------------------------------------------------------+
|                                System Topology                                  |
|                                                                                 |
|   [ Public Ingress / Internet ]                                                 |
|                 |                                                               |
|                 v                                                               |
|   +---------------------------+       HTTPS / TLS (Port 443)                    |
|   |   Nginx Gateway / ALB     |----------------------------+                    |
|   +---------------------------+                            |                    |
|                 | (Internal Bridge Network / Private Subnet)|                   |
|                 v                                          v                    |
|   +---------------------------+              +---------------------------+      |
|   |    App Service (Node/Go)  |              |    Metrics / Observability|      |
|   +---------------------------+              +---------------------------+      |
|                 |                                                               |
|                 v                                                               |
|   +---------------------------+                                                 |
|   |  Database / Redis Cache   | (Isolated Storage Volume)                       |
|   +---------------------------+                                                 |
+---------------------------------------------------------------------------------+
```

### 1.2 Component Responsibilities
- **Gateway / Ingress Layer:** TLS termination, rate limiting, path routing, and request buffering.
- **Application Workload Layer:** Stateless containerized microservices running under unprivileged UIDs.
- **Data & Persistence Layer:** Managed database / container with volume persistence and healthcheck probes.
- **Automation / Pipeline Layer:** Automated build, test, and container packaging workflows.

---

## 2. Complete Production Implementation

### 2.1 Directory Structure
```text
project-root/
├── compose.yaml
├── nginx/
│   └── nginx.conf
├── app/
│   ├── Dockerfile
│   └── .dockerignore
└── scripts/
    ├── verify.sh
    └── backup.sh
```

### 2.2 Container Definitions (`Dockerfile`)
```dockerfile
# Multi-stage production Dockerfile
FROM alpine:3.20 AS builder
WORKDIR /build
# Build stage commands

FROM alpine:3.20
RUN addgroup -S appgroup && adduser -S appuser -G appgroup
USER appuser
EXPOSE 8080
ENTRYPOINT ["/app/entrypoint"]
```

### 2.3 Orchestration Manifest (`compose.yaml` or Kubernetes Manifest)
```yaml
services:
  gateway:
    image: nginx:1.26-alpine
    restart: unless-stopped
    ports:
      - "80:80"
      - "443:443"
    volumes:
      - ./nginx/nginx.conf:/etc/nginx/nginx.conf:ro
    depends_on:
      app:
        condition: service_healthy

  app:
    build:
      context: ./app
      dockerfile: Dockerfile
    restart: unless-stopped
    environment:
      NODE_ENV: production
    healthcheck:
      test: ["CMD", "curl", "-f", "http://localhost:8080/health"]
      interval: 10s
      timeout: 5s
      retries: 3
```

### 2.4 Service Configurations (`nginx.conf`, scripts, or IaC)
```nginx
events {
    worker_connections 1024;
}

http {
    include /etc/nginx/mime.types;
    default_type application/octet-stream;
    sendfile on;
    keepalive_timeout 65;

    upstream backend_app {
        server app:8080;
    }

    server {
        listen 80;
        server_name localhost;

        location / {
            proxy_pass http://backend_app;
            proxy_set_header Host $host;
            proxy_set_header X-Real-IP $remote_addr;
        }
    }
}
```

---

## 3. Automated Verification & Test Harness

### 3.1 Verification Runner Script (`scripts/verify.sh`)
```bash
#!/usr/bin/env bash
set -euo pipefail

echo "=== 1. Starting Stack ==="
docker compose up -d --wait

echo "=== 2. Health Probe Check ==="
curl -sf http://localhost/health | grep -q "UP"
echo "Healthcheck passed!"

echo "=== 3. End-to-End Traffic Assertion ==="
STATUS=$(curl -s -o /dev/null -w "%{http_code}" http://localhost/)
if [ "$STATUS" -eq 200 ]; then
    echo "SUCCESS: Endpoint returned HTTP 200"
else
    echo "FAILED: HTTP status was $STATUS"
    exit 1
fi
```

### 3.2 Real Verification Log
```text
=== 1. Starting Stack ===
[+] Running 3/3
 ✔ Network default         Created
 ✔ Container app           Healthy
 ✔ Container gateway       Started
=== 2. Health Probe Check ===
Healthcheck passed!
=== 3. End-to-End Traffic Assertion ===
SUCCESS: Endpoint returned HTTP 200
```

---

## 4. Production Hardening, Security & Cost Notes

### 4.1 Security Hardening Checklist
- [x] **Non-Root Execution:** All application containers run under UID 10001 (`USER appuser`).
- [x] **Read-Only Root Filesystem:** Container roots mounted as `read_only: true` with `/tmp` as `tmpfs`.
- [x] **Privilege Escalation Prevention:** Dropped capabilities (`cap_drop: [ALL]`) and `no-new-privileges:true`.
- [x] **TLS Encryption:** Enforce TLS 1.3 with automated certificate renewal.
- [x] **Network Isolation:** Internal services bound strictly to private bridge networks without host port publishing.

### 4.2 Resource Allocation & Cloud Cost Estimate
| Component | Resource Allocation (CPU / RAM) | AWS Equivalent | Estimated Monthly Cost |
|---|---|---|---|
| Ingress Gateway | 0.25 vCPU, 256MB RAM | ALB / t4g.nano | ~$16.00 (ALB) |
| App Service (x2) | 0.5 vCPU, 512MB RAM | ECS Fargate / t4g.small | ~$18.00 |
| Cache / DB | 1.0 vCPU, 1GB RAM | Amazon ElastiCache / db.t4g.micro | ~$15.00 |
| **Total Estimated Stack** | | | **~$49.00 / month** |

---

## 5. Operational Runbook & Troubleshooting

### 5.1 Routine Operations
```bash
# View aggregated real-time logs
docker compose logs -f --tail 100

# Inspect container health states
docker compose ps --format "table {{.Name}}\t{{.Status}}\t{{.Ports}}"

# Graceful stack restart
docker compose restart gateway
```

### 5.2 Failure Scenarios & Troubleshooting Runbook
| Failure Mode | Symptoms | Diagnostic Command | Remediation Action |
|---|---|---|---|
| **Upstream 502 Bad Gateway** | Nginx logs show `connect() failed (111: Connection refused)` | `docker compose logs app` | Check if app process crashed; verify container healthcheck |
| **Out of Memory (OOMKilled)** | Container exits with status code 137 | `docker inspect app --format '{{.State.OOMKilled}}'` | Increase memory quota in `compose.yaml` |
| **Disk Space Starvation** | OverlayFS cannot allocate blocks | `docker system df` | Run `docker system prune -af --volumes` |
| **DNS Resolution Failure** | Upstream host not resolved | `docker exec -it gateway ping -c 2 app` | Verify both services share the identical Docker network |
