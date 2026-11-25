<!--
================================================================================
Metadata:
  Title: Reference Project 04 - Multi-Arch Docker Build & Push to GHCR
  Track: Git & GitHub / DevOps
  Difficulty: Advanced
  Estimated Time: 55 minutes
  Prerequisites: 04-github-actions/16-docker-ci-cd.md, 07-github-security/06-sbom-and-supply-chain-security.md
  Key Terminology: Docker Buildx, QEMU, Multi-Arch, GHCR (ghcr.io), GHA Cache Backend, Trivy SARIF, OCI Labels
================================================================================
-->

# Reference Project 04: Multi-Arch Docker Build & Push to GHCR

[<- Back to Projects Overview](./README.md) | [Next Project: 05-release-automation-with-conventional-commits.md ->](./05-release-automation-with-conventional-commits.md)

---

## 1. Overview & Architecture

Modern containerized microservices must run reliably across diverse hardware architectures: developer Apple Silicon laptops (`linux/arm64`) and enterprise production cloud Kubernetes nodes (Intel/AMD `linux/amd64`). Publishing multi-architecture OCI images manually is complex and slow without dedicated build infrastructure.

This reference project implements a high-throughput, secure Docker build pipeline utilizing **Docker Buildx**, **QEMU**, **GitHub Actions cache (`type=gha`)**, and the **GitHub Container Registry (`ghcr.io`)**. The pipeline also integrates container vulnerability scanning using **Trivy** to publish SARIF reports directly to GitHub Code Scanning before promoting image tags.

### Pipeline Architecture Diagram

```
+-------------------------------------------------------------------------+
| Push Event (Git Tag: v* or Branch: main)                                |
+-------------------------------------------------------------------------+
                                    |
                                    v
+-------------------------------------------------------------------------+
| 1. Docker Metadata Engine (docker/metadata-action)                      |
|    - Computes semantic tags: :v1.2.3, :latest, :sha-4a3b1c              |
|    - Injects standard OCI annotations (source, commit, description)     |
+-------------------------------------------------------------------------+
                                    |
                                    v
+-------------------------------------------------------------------------+
| 2. Multi-Architecture Buildx Engine (docker/build-push-action)          |
|    - QEMU emulation for linux/amd64 and linux/arm64                     |
|    - GHA Layer Caching: cache-from=type=gha, cache-to=type=gha,mode=max |
|    - Generates multi-arch OCI manifest list                             |
+-------------------------------------------------------------------------+
                                    |
                                    v
+-------------------------------------------------------------------------+
| 3. Security Vulnerability Scan (Trivy)                                  |
|    - Analyzes OS packages & node dependencies for CVEs                  |
|    - Uploads SARIF report to GitHub Code Scanning                       |
+-------------------------------------------------------------------------+
                                    |
                                    v
+-------------------------------------------------------------------------+
| 4. Publish to GitHub Container Registry (ghcr.io/org/repo:tag)          |
+-------------------------------------------------------------------------+
```

---

## 2. Prerequisites & Setup

### Environment Requirements
- **Git**: Version 2.25+
- **Docker**: Local Docker daemon (for local testing)
- **GitHub Permissions**: `packages: write`, `contents: read`, `security-events: write`

### Repository Directory Hierarchy
```text
docker-ghcr-project/
├── .github/
│   └── workflows/
│       └── docker-publish.yml  # Complete container build workflow
├── Dockerfile                  # Hardened multi-stage Dockerfile
├── package.json
└── server.js                   # Minimal Node.js HTTP service
```

---

## 3. Step-by-Step Implementation Walkthrough

### Step 1: Initialize Workspace & Application Code
```bash
mkdir -p ~/git-labs/docker-ghcr-project && cd ~/git-labs/docker-ghcr-project
git init
```

Create `server.js`:
```javascript
cat << 'EOF' > server.js
const http = require('http');
const os = require('os');

const PORT = process.env.PORT || 8080;

const server = http.createServer((req, res) => {
  if (req.url === '/healthz') {
    res.writeHead(200, { 'Content-Type': 'application/json' });
    return res.end(JSON.stringify({ status: 'UP', arch: os.arch() }));
  }

  res.writeHead(200, { 'Content-Type': 'text/plain' });
  res.end(`Hello from Container running on ${os.arch()} architecture!\n`);
});

server.listen(PORT, () => {
  console.log(`Server listening on port ${PORT}`);
});
EOF
```

Create `package.json`:
```json
cat << 'EOF' > package.json
{
  "name": "docker-ghcr-demo",
  "version": "1.0.0",
  "main": "server.js",
  "scripts": {
    "start": "node server.js"
  }
}
EOF
```

### Step 2: Author Hardened Multi-Stage Dockerfile
```dockerfile
cat << 'EOF' > Dockerfile
# Stage 1: Build & Dependency Stage
FROM node:20-alpine AS builder
WORKDIR /app
COPY package*.json ./
# Clean install only production dependencies
RUN npm ci --only=production

# Stage 2: Final Minimal Runtime Image
FROM node:20-alpine AS runner
WORKDIR /app

# Run as non-root user (node:node is UID 1000 in alpine)
USER node

# Copy dependencies and application logic
COPY --chown=node:node --from=builder /app/node_modules ./node_modules
COPY --chown=node:node package.json ./
COPY --chown=node:node server.js ./

ENV NODE_ENV=production
ENV PORT=8080
EXPOSE 8080

HEALTHCHECK --interval=30s --timeout=3s --retries=3 \
  CMD wget --no-verbose --tries=1 --spider http://localhost:8080/healthz || exit 1

CMD ["node", "server.js"]
EOF
```

### Step 3: Author GitHub Actions Workflow (`.github/workflows/docker-publish.yml`)

```yaml
mkdir -p .github/workflows
cat << 'EOF' > .github/workflows/docker-publish.yml
name: "Docker Build & Push to GHCR"

on:
  push:
    branches: ["main"]
    tags: ["v*"]
  pull_request:
    branches: ["main"]

env:
  REGISTRY: ghcr.io
  IMAGE_NAME: ${{ github.repository }}

jobs:
  build-and-push:
    name: "Build & Push Multi-Arch Image"
    runs-on: ubuntu-latest
    permissions:
      contents: read
      packages: write
      security-events: write

    steps:
      - name: "Checkout repository"
        uses: actions/checkout@v4

      - name: "Set up QEMU (ARM64 emulation)"
        uses: docker/setup-qemu-action@v3

      - name: "Set up Docker Buildx"
        uses: docker/setup-buildx-action@v3

      - name: "Log in to GitHub Container Registry"
        if: github.event_name != 'pull_request'
        uses: docker/login-action@v3
        with:
          registry: ${{ env.REGISTRY }}
          username: ${{ github.actor }}
          password: ${{ secrets.GITHUB_TOKEN }}

      - name: "Extract Docker metadata & OCI labels"
        id: meta
        uses: docker/metadata-action@v5
        with:
          images: ${{ env.REGISTRY }}/${{ env.IMAGE_NAME }}
          tags: |
            type=ref,event=branch
            type=semver,pattern={{version}}
            type=semver,pattern={{major}}.{{minor}}
            type=sha,format=short

      - name: "Build and push multi-arch image"
        id: build-push
        uses: docker/build-push-action@v5
        with:
          context: .
          platforms: linux/amd64,linux/arm64
          push: ${{ github.event_name != 'pull_request' }}
          tags: ${{ steps.meta.outputs.tags }}
          labels: ${{ steps.meta.outputs.labels }}
          cache-from: type=gha
          cache-to: type=gha,mode=max

      - name: "Scan image for vulnerabilities with Trivy"
        if: github.event_name != 'pull_request'
        uses: aquasecurity/trivy-action@master
        with:
          image-ref: ${{ env.REGISTRY }}/${{ env.IMAGE_NAME }}:${{ steps.meta.outputs.version || 'main' }}
          format: "sarif"
          output: "trivy-results.sarif"

      - name: "Upload Trivy scan results to GitHub Security"
        if: github.event_name != 'pull_request'
        uses: github/codeql-action/upload-sarif@v3
        with:
          sarif_file: "trivy-results.sarif"
EOF
```

---

## 4. Verification & Testing

### Step 1: Local Multi-Platform Build Verification
Test build locally with Buildx:
```bash
docker buildx build --platform linux/amd64,linux/arm64 . -t test-app:local
```

### Step 2: Push Tag and Verify GHCR Package
```bash
git add .
git commit -m "feat(docker): add multi-arch buildx pipeline to GHCR"
git tag v1.0.0
git push origin main --tags
```

Inspect the container image in terminal:
```bash
# Pull and inspect the multi-arch manifest list
docker manifest inspect ghcr.io/my-org/docker-ghcr-project:v1.0.0
```
Output confirms both architectures:
```json
{
   "schemaVersion": 2,
   "mediaType": "application/vnd.docker.distribution.manifest.list.v2+json",
   "manifests": [
      {
         "mediaType": "application/vnd.docker.distribution.manifest.v2+json",
         "platform": { "architecture": "amd64", "os": "linux" }
      },
      {
         "mediaType": "application/vnd.docker.distribution.manifest.v2+json",
         "platform": { "architecture": "arm64", "os": "linux" }
      }
   ]
}
```

---

## 5. Production Readiness & Troubleshooting

### Production Readiness Checklist
- [x] Multi-stage build isolates build tooling from runtime environment.
- [x] Runs under unprivileged non-root user (`USER node`).
- [x] Docker layer caching via GitHub Actions cache backend (`cache-to: type=gha,mode=max`).
- [x] Standard OCI annotations attached automatically via `docker/metadata-action`.
- [x] Automatic container vulnerability scanning via Trivy before deployment.

### Troubleshooting Table

| Error | Root Cause | Solution |
|:---|:---|:---|
| `denied: installation not allowed to Write organization package` | `GITHUB_TOKEN` lacking write access | Declare `permissions: packages: write` in workflow |
| `exec /bin/sh: exec format error` | Container built for x86 executed on ARM (or vice-versa) | Ensure QEMU is enabled and build targets multi-arch |
| Slow multi-arch builds (>15 mins) | Emulation overhead via QEMU | Use native ARM self-hosted runners or leverage layer caching |

---

[<- Back to Projects Overview](./README.md) | [Next Project: 05-release-automation-with-conventional-commits.md ->](./05-release-automation-with-conventional-commits.md)
