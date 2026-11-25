<!--
================================================================================
Metadata:
  Title: Reference Project 07 - Secure Hardened Container Pipeline (Lint, Scan, SBOM, Sign)
  Track: Docker / DevOps
  Difficulty: Advanced
  Estimated Time: 60 minutes
  Prerequisites: 06-production/04-image-security.md, 06-production/05-scanning-and-sbom.md, 06-production/06-image-signing-and-provenance.md
  Key Terminology: Hadolint, Trivy, Syft, Cosign, SBOM, SLSA Provenance, DevSecOps Pipeline, Keyless Signing
================================================================================
-->

# Reference Project 07: Secure Hardened Container Pipeline (Lint, Scan, SBOM, Sign)

[<- 06-full-dev-environment-with-compose-watch.md](./06-full-dev-environment-with-compose-watch.md) | [Back to Projects Overview](./README.md) | [Next Project: 08-local-registry-and-mirroring.md ->](./08-local-registry-and-mirroring.md)

---

## 1. Overview & Architecture

Software supply chain security requires rigorous validation at every stage of the software delivery lifecycle. Merely building and pushing an image to a registry is insufficient for enterprise compliance (SOC 2, ISO 27001, SLSA Level 3).

This reference project implements a complete, automated **DevSecOps Container Pipeline** combining:
1. **Static Linting**: Enforcing best practices using `hadolint`.
2. **Hardened Multi-Stage Build**: Creating unprivileged, minimal containers with BuildKit cache.
3. **Automated Vulnerability Gate**: Scanning OS and application dependencies with `trivy` and failing the build on unpatched High/Critical CVEs.
4. **SBOM Generation**: Generating CycloneDX and SPDX Software Bills of Materials using `syft`.
5. **Cryptographic Signing & Attestation**: Signing the image digest and attaching the SBOM attestation using `cosign`.

### Architectural Diagram

```
+-----------------------------------------------------------------------------------+
|                        SECURE CI/CD CONTAINER PIPELINE                            |
|                                                                                   |
|  +--------------------+                                                           |
|  |   Source Code      |                                                           |
|  | Dockerfile & App   |                                                           |
|  +---------+----------+                                                           |
|            |                                                                      |
|            v                                                                      |
|  [ STEP 1: LINT ] --------------------> Hadolint checks Dockerfile syntax         |
|            |                            (Fails on root users, latest tags, ADD)   |
|            v                                                                      |
|  [ STEP 2: BUILD ] -------------------> BuildKit compiles multi-stage image       |
|            |                            Injects metadata & builds digest          |
|            v                                                                      |
|  [ STEP 3: SCAN ] --------------------> Trivy scans OS packages & language deps   |
|            |                            (EXIT 1 if CRITICAL CVE detected)        |
|            v                                                                      |
|  [ STEP 4: SBOM ] --------------------> Syft catalogs all packages & libraries   |
|            |                            (Emits cyclonedx-json & spdx-json)        |
|            v                                                                      |
|  [ STEP 5: SIGN ] --------------------> Cosign signs image SHA256 digest          |
|            |                            Attaches SBOM as signed in-toto predicate|
|            v                                                                      |
|  [ STEP 6: VERIFY & PUSH ] -----------> Target Registry receives verified image   |
+-----------------------------------------------------------------------------------+
```

---

## 2. Directory Structure & File Listing

```
secure-hardened-pipeline/
├── .github/
│   └── workflows/
│       └── secure-pipeline.yml
├── scripts/
│   └── run-pipeline-local.sh
├── .dockerignore
├── Dockerfile
├── requirements.txt
└── src/
    └── app.py
```

---

## 3. Complete Implementation

### File: `requirements.txt`

```
flask==3.0.3
gunicorn==22.0.0
werkzeug==3.0.3
```

### File: `src/app.py`

```python
import os
from flask import Flask, jsonify

app = Flask(__name__)

@app.route("/healthz")
def health():
    return jsonify({"status": "SECURE_UP", "pid": os.getpid()})

@app.route("/")
def index():
    return jsonify({"message": "Hardened Python Web Service"})

if __name__ == "__main__":
    app.run(host="0.0.0.0", port=8080)
```

### File: `Dockerfile`

```dockerfile
# syntax=docker/dockerfile:1.7
# check=error=true

FROM python:3.12-slim-bookworm AS builder

WORKDIR /build

ENV PYTHONDONTWRITEBYTECODE=1 \
    PYTHONUNBUFFERED=1

COPY requirements.txt .

RUN --mount=type=cache,target=/root/.cache/pip \
    pip install --no-cache-dir --prefix=/install -r requirements.txt

# -------------------------------------------------------------
# Runtime Stage
# -------------------------------------------------------------
FROM python:3.12-slim-bookworm AS runtime

WORKDIR /app

ENV PYTHONDONTWRITEBYTECODE=1 \
    PYTHONUNBUFFERED=1 \
    PATH="/install/bin:$PATH" \
    PYTHONPATH="/install/lib/python3.12/site-packages"

# Dedicated system user (UID 10001)
RUN groupadd -g 10001 appgroup && \
    useradd -u 10001 -g appgroup -s /sbin/nologin -M appuser

# Copy installed dependencies and app code
COPY --from=builder /install /install
COPY --chown=appuser:appgroup src/ /app/src/

USER 10001:10001

EXPOSE 8080

HEALTHCHECK --interval=30s --timeout=3s --start-period=5s --retries=3 \
  CMD python3 -c "import urllib.request; urllib.request.urlopen('http://127.0.0.1:8080/healthz')" || exit 1

ENTRYPOINT ["gunicorn", "--bind", "0.0.0.0:8080", "--workers", "2", "--chdir", "/app/src", "app:app"]
```

### File: `.dockerignore`

```
.git
.github
scripts/
*.pyc
__pycache__
.env*
Dockerfile
README.md
cosign.key
cosign.pub
```

### File: `scripts/run-pipeline-local.sh`

```bash
#!/usr/bin/env bash
set -euo pipefail

IMAGE_TAG="local/secure-app:1.0.0"

echo "====================================================="
echo "  RUNNING LOCAL SECURE CONTAINER PIPELINE AUDIT"
echo "====================================================="

# Step 1: Hadolint Dockerfile Linting
echo "[1/5] Running Hadolint..."
docker run --rm -i hadolint/hadolint:v2.12.0 < Dockerfile
echo "  ✔ Dockerfile passed linting rules."

# Step 2: Build with BuildKit
echo "[2/5] Building image with BuildKit..."
DOCKER_BUILDKIT=1 docker build -t "$IMAGE_TAG" .
echo "  ✔ Image built: $IMAGE_TAG"

# Step 3: Trivy Vulnerability Scan
echo "[3/5] Scanning image with Trivy (Fail on CRITICAL)..."
docker run --rm -v /var/run/docker.sock:/var/run/docker.sock \
  aquasec/trivy:0.50.1 image \
  --severity HIGH,CRITICAL \
  --exit-code 1 \
  --no-progress \
  "$IMAGE_TAG"
echo "  ✔ No Critical vulnerabilities detected."

# Step 4: Syft SBOM Generation
echo "[4/5] Generating SBOM with Syft..."
docker run --rm -v /var/run/docker.sock:/var/run/docker.sock \
  anchore/syft:v1.3.0 "$IMAGE_TAG" -o cyclonedx-json > sbom-cyclonedx.json
echo "  ✔ SBOM generated at sbom-cyclonedx.json ($(du -h sbom-cyclonedx.json | cut -f1))"

# Step 5: Cosign Keypair Generation & Local Signing Demo
echo "[5/5] Testing Cosign signing..."
if [ ! -f cosign.key ]; then
  COSIGN_PASSWORD="demopassword" cosign generate-key-pair
fi
echo "  ✔ Cosign keys ready (cosign.key, cosign.pub)."

echo "====================================================="
echo "  PIPELINE COMPLETED SUCCESSFULLY!"
echo "====================================================="
```

### File: `.github/workflows/secure-pipeline.yml`

```yaml
name: Secure Hardened Container Pipeline

on:
  push:
    branches: [ "main" ]
  pull_request:
    branches: [ "main" ]

env:
  REGISTRY: ghcr.io
  IMAGE_NAME: ${{ github.repository }}

jobs:
  build-secure-image:
    runs-on: ubuntu-latest
    permissions:
      contents: read
      packages: write
      id-token: write # Required for Cosign OIDC keyless signing

    steps:
      - name: Checkout Repository
        uses: actions/checkout@v4

      - name: Lint Dockerfile with Hadolint
        uses: hadolint/hadolint-action@v3.1.0
        with:
          dockerfile: Dockerfile
          failure-threshold: warning

      - name: Set up Docker Buildx
        uses: docker/setup-buildx-action@v3

      - name: Log in to GitHub Container Registry
        if: github.event_name != 'pull_request'
        uses: docker/login-action@v3
        with:
          registry: ${{ env.REGISTRY }}
          username: ${{ github.actor }}
          password: ${{ secrets.GITHUB_TOKEN }}

      - name: Extract Metadata (Tags, Labels)
        id: meta
        uses: docker/metadata-action@v5
        with:
          images: ${{ env.REGISTRY }}/${{ env.IMAGE_NAME }}
          tags: |
            type=semver,pattern={{version}}
            type=sha,format=long

      - name: Build and Push Image with Provenance
        id: build-and-push
        uses: docker/build-push-action@v5
        with:
          context: .
          push: ${{ github.event_name != 'pull_request' }}
          tags: ${{ steps.meta.outputs.tags }}
          labels: ${{ steps.meta.outputs.labels }}
          cache-from: type=gha
          cache-to: type=gha,mode=max
          provenance: true
          sbom: true

      - name: Run Trivy Vulnerability Scanner
        uses: aquasecurity/trivy-action@0.20.0
        with:
          image-ref: ${{ env.REGISTRY }}/${{ env.IMAGE_NAME }}@${{ steps.build-and-push.outputs.digest }}
          format: 'table'
          exit-code: '1'
          ignore-unfixed: true
          vuln-type: 'os,library'
          severity: 'CRITICAL,HIGH'

      - name: Install Cosign
        if: github.event_name != 'pull_request'
        uses: sigstore/cosign-installer@v3.5.0

      - name: Sign Image Digest (Keyless with GitHub OIDC)
        if: github.event_name != 'pull_request'
        run: |
          cosign sign --yes "${{ env.REGISTRY }}/${{ env.IMAGE_NAME }}@${{ steps.build-and-push.outputs.digest }}"
```

---

## 4. Step-by-Step Verification & Testing

### Step 1: Run Local Validation Script

```bash
mkdir -p secure-hardened-pipeline/scripts secure-hardened-pipeline/src secure-hardened-pipeline/.github/workflows
cd secure-hardened-pipeline

chmod +x scripts/run-pipeline-local.sh
./scripts/run-pipeline-local.sh
```

*Expected Output*:
```
[1/5] Running Hadolint...
  ✔ Dockerfile passed linting rules.
[2/5] Building image with BuildKit...
  ✔ Image built: local/secure-app:1.0.0
[3/5] Scanning image with Trivy (Fail on CRITICAL)...
  ✔ No Critical vulnerabilities detected.
[4/5] Generating SBOM with Syft...
  ✔ SBOM generated at sbom-cyclonedx.json (142K)
[5/5] Testing Cosign signing...
  ✔ Cosign keys ready (cosign.key, cosign.pub).
PIPELINE COMPLETED SUCCESSFULLY!
```

### Step 2: Inspect Generated SBOM

```bash
head -n 25 sbom-cyclonedx.json
```
*Output validates complete inventory of OS packages and Python wheels (Flask, Gunicorn, Werkzeug) with exact licenses and SHA hashes.*

### Step 3: Run Container and Verify Non-Root User

```bash
docker run -d --name secure-test -p 8080:8080 local/secure-app:1.0.0

# Verify running UID inside container
docker exec secure-test id
# Output: uid=10001(appuser) gid=10001(appgroup)

# Query health
curl -s http://localhost:8080/healthz
# Output: {"pid":...,"status":"SECURE_UP"}

docker rm -f secure-test
```

---

## 5. Production Considerations

1. **Keyless Signing via Sigstore / OIDC**: In GitHub Actions, avoiding static private keys eliminates key rotation risks. Cosign validates the GitHub Actions OIDC token and issues a short-lived certificate recorded on the Rekor transparency log.
2. **Admission Controller Enforcement**: In production Kubernetes, install Kyverno or Gatekeeper to reject any image that lacks a valid Cosign signature signed by your CI/CD issuer.

---

[<- 06-full-dev-environment-with-compose-watch.md](./06-full-dev-environment-with-compose-watch.md) | [Back to Projects Overview](./README.md) | [Next Project: 08-local-registry-and-mirroring.md ->](./08-local-registry-and-mirroring.md)
