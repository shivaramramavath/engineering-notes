<!--
================================================================================
Metadata:
  Title: Quick Reference Cheat Sheet - Docker Buildx and BuildKit
  Track: Docker / DevOps
  Difficulty: Intermediate to Advanced
  Estimated Time: 25 minutes
  Key Terminology: BuildKit, Buildx, Multi-platform, Cache Backends, Bake, Attestations, Provenance
================================================================================
-->

# Docker Buildx and BuildKit Quick Reference Cheat Sheet

[<- 04-networking-and-volumes.md](./04-networking-and-volumes.md) | [Back to Cheatsheets Overview](./README.md) | [Next Cheatsheet: 06-security-checklist.md ->](./06-security-checklist.md)

---

## 1. Enabling BuildKit

```bash
# Enable per command:
DOCKER_BUILDKIT=1 docker build -t my-app .

# Enable for current shell session:
export DOCKER_BUILDKIT=1

# Enable permanently across all builds in /etc/docker/daemon.json:
{
  "features": {
    "buildkit": true
  }
}
```

---

## 2. Buildx Builder Instance Management

```bash
# List available builder instances
docker buildx ls

# Create and bootstrap a new multi-platform containerized builder instance
docker buildx create --name multi-builder --driver docker-container --bootstrap --use

# Inspect builder details (active drivers, supported architectures)
docker buildx inspect --bootstrap

# Switch active builder
docker buildx use default
docker buildx use multi-builder

# Stop and remove custom builder
docker buildx stop multi-builder
docker buildx rm multi-builder
```

---

## 3. Multi-Platform Image Compilation

```bash
# Install QEMU multi-architecture emulation binaries
docker run --privileged --rm tonistiigi/binfmt --install all

# Build and push multi-arch image directly to registry (Manifest List)
docker buildx build \
  --platform linux/amd64,linux/arm64,linux/arm/v7 \
  -t my-registry.io/org/app:1.0.0 \
  --push .

# Build multi-platform and load matching host architecture into local Docker daemon
docker buildx build \
  --platform linux/amd64,linux/arm64 \
  --load \
  -t my-app:test .
```

---

## 4. Cache Backends (`--cache-from` & `--cache-to`)

```bash
# 1. Local Filesystem Cache
docker buildx build \
  --cache-to type=local,dest=/tmp/build-cache,mode=max \
  --cache-from type=local,src=/tmp/build-cache \
  -t my-app .

# 2. Remote Registry Cache (Pushed alongside image layers)
docker buildx build \
  --cache-to type=registry,ref=my-registry.io/org/app:cache,mode=max \
  --cache-from type=registry,ref=my-registry.io/org/app:cache \
  --push \
  -t my-registry.io/org/app:latest .

# 3. GitHub Actions Cache (CI/CD Native)
docker buildx build \
  --cache-to type=gha,mode=max \
  --cache-from type=gha \
  -t my-app .
```

---

## 5. Security & Build Attestations

```bash
# Build with BuildKit Secret (Without baking token into layer history)
docker buildx build \
  --secret id=npmrc,src=$HOME/.npmrc \
  -t my-app .

# Build with Host SSH Agent Forwarding (For private git submodules)
docker buildx build \
  --ssh default=$SSH_AUTH_SOCK \
  -t my-app .

# Generate SLSA Provenance and CycloneDX/SPDX SBOM Attestations
docker buildx build \
  --provenance=mode=max \
  --sbom=true \
  --push \
  -t my-registry.io/org/app:v1.0.0 .
```

---

## 6. Buildx Bake (`docker-bake.hcl`)

`docker buildx bake` replaces complex shell scripts with declarative build files:

```hcl
# docker-bake.hcl
group "default" {
  targets = ["api", "worker"]
}

target "base" {
  context = "."
  platforms = ["linux/amd64", "linux/arm64"]
}

target "api" {
  inherits = ["base"]
  dockerfile = "Dockerfile.api"
  tags = ["my-registry.io/org/api:latest"]
  cache-from = ["type=registry,ref=my-registry.io/org/api:cache"]
  cache-to = ["type=registry,ref=my-registry.io/org/api:cache,mode=max"]
}

target "worker" {
  inherits = ["base"]
  dockerfile = "Dockerfile.worker"
  tags = ["my-registry.io/org/worker:latest"]
}
```

```bash
# Execute concurrent multi-target build across all CPU cores:
docker buildx bake --push
```

---

[<- 04-networking-and-volumes.md](./04-networking-and-volumes.md) | [Back to Cheatsheets Overview](./README.md) | [Next Cheatsheet: 06-security-checklist.md ->](./06-security-checklist.md)
