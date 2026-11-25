# Docker in CI/CD: DinD, DooD, Remote Caching, and GitHub Actions Pipelines

```
Track: DevOps & Containerization
Module: 06-production
Prerequisites: 03-dockerfile/06-buildkit-and-buildx.md, 03-dockerfile/07-multi-platform-images.md
Difficulty: Advanced
Estimated Duration: 55 minutes
Target Docker Version: 27.x+
Last Validated: February 2025
```

---

## Table of Contents
1. [What & Why](#1-what--why)
2. [Core Concepts](#2-core-concepts)
3. [The Three CI Container Execution Models](#3-the-three-ci-container-execution-models)
   - [3.1 Docker-in-Docker (DinD)](#31-docker-in-docker-dind)
   - [3.2 Docker-outside-of-Docker (DooD / Socket Mount)](#32-docker-outside-of-docker-dood--socket-mount)
   - [3.3 Daemonless Builders (Kaniko & Buildah)](#33-daemonless-builders-kaniko--buildah)
4. [BuildKit Remote Layer Caching in CI](#4-buildkit-remote-layer-caching-in-ci)
5. [Syntax & Configuration Reference](#5-syntax--configuration-reference)
6. [Complete Production GitHub Actions Workflow](#6-complete-production-github-actions-workflow)
7. [GitLab CI Pipeline Example](#7-gitlab-ci-pipeline-example)
8. [Production Best Practices & Antipatterns](#8-production-best-practices--antipatterns)
9. [Common Pitfalls & Troubleshooting](#9-common-pitfalls--troubleshooting)
10. [Performance Tuning & Resource Optimization](#10-performance-tuning--resource-optimization)
11. [Security Hardening & Threat Model](#11-security-hardening--threat-model)
12. [Architectural Comparison Table: CI Execution Models](#12-architectural-comparison-table-ci-execution-models)
13. [Real-World Scenario Walkthrough](#13-real-world-scenario-walkthrough)
14. [Self-Assessment & Interview Questions](#14-self-assessment--interview-questions)
15. [Hands-on Practice Exercises](#15-hands-on-practice-exercises)
16. [Summary & Reference Cheat Sheet](#summary--reference-cheat-sheet)

---

## 1. What & Why

Automating container builds, testing, and registry distribution in Continuous Integration and Continuous Deployment (CI/CD) pipelines is foundational to modern DevOps.

However, running Docker *inside* a CI runner environment introduces unique architectural challenges:
- **How does a container build another container?** (The DinD vs DooD dilemma).
- **Why do CI builds run from scratch without caching?** Standard ephemeral CI runners (GitHub Actions cloud VMs, GitLab shared runners, AWS CodeBuild) discard local disk storage when a job finishes. Without **remote registry cache backends**, every CI run rebuilds base layers, taking 15 minutes instead of 30 seconds.
- **How to manage registry credentials safely** without leaking tokens into build artifacts.
- **How to cross-compile multi-architecture images** (Linux AMD64 + ARM64) within standard x86 CI runners.

---

## 2. Core Concepts

### Ephemeral CI Runners vs Persistent Daemons
In local development, Docker reuses the local machine's persistent layer cache in `/var/lib/docker`.
In cloud CI/CD, runners spin up on clean, empty virtual machines. To achieve fast build times, CI pipelines must stream and synchronize build caches remotely to an OCI registry or cloud cache storage (e.g. GitHub Actions Cache API, AWS S3).

### The BuildKit Cache Backends
- **`type=gha`**: Stores layer caches directly in GitHub Actions Cache storage (zero registry storage costs).
- **`type=registry`**: Pushes an independent cache manifest directly to your container registry (Docker Hub, AWS ECR, GitHub Packages).
- **`mode=max`**: Ensures BuildKit caches *all* intermediate build stages, not just the final runtime image stage.

---

## 3. The Three CI Container Execution Models

```
+-------------------------------------------------------------------------+
|                  CI CONTAINER EXECUTION ARCHITECTURES                   |
+-------------------------------------------------------------------------+
| Mode A: Docker-outside-of-Docker (DooD)                                 |
| CI Container ---> Mounts /var/run/docker.sock ---> Host Docker Daemon   |
+-------------------------------------------------------------------------+
| Mode B: Docker-in-Docker (DinD)                                         |
| CI Container ---> Runs isolated nested dockerd inside (--privileged)    |
+-------------------------------------------------------------------------+
| Mode C: Daemonless Builder (Kaniko / Buildah)                           |
| CI Container ---> Compiles OCI image directly in userspace (No daemon!) |
+-------------------------------------------------------------------------+
```

### 3.1 Docker-in-Docker (DinD)
DinD runs a completely isolated, independent Docker daemon inside the CI container.
- **Requirement**: Must run with `--privileged` kernel flags.
- **Pros**: Complete filesystem and container isolation; clean daemon state per build.
- **Cons**: Severe security risk (privileged container breakout); OverlayFS-on-OverlayFS storage driver performance degradation.

### 3.2 Docker-outside-of-Docker (DooD / Socket Mount)
The CI container mounts the host machine's Docker daemon socket:
`-v /var/run/docker.sock:/var/run/docker.sock`
- **Pros**: Fast; shares the host's layer cache directly; no privileged flag needed.
- **Cons**: Binds to host path namespace! (Host bind mounts like `-v $(pwd):/app` resolve relative to the **host**, not the CI container, causing empty mount bugs); grants full root privileges over the host.

### 3.3 Daemonless Builders (Kaniko & Buildah)
Built specifically for unprivileged Kubernetes clusters.
- Constructs OCI container image layers directly in user space by snapshotting filesystem changes without requiring a background Docker daemon or root kernel privileges.

---

## 4. BuildKit Remote Layer Caching in CI

To avoid rebuilding unchanged layers on ephemeral cloud runners, configure remote caching:

```bash
docker buildx build \
  --cache-to type=registry,ref=mycorp/api:buildcache,mode=max \
  --cache-from type=registry,ref=mycorp/api:buildcache \
  --push -t mycorp/api:v1.2.0 .
```

### Key Parameters:
- `cache-to type=registry`: Exports newly created build cache layers directly to the registry.
- `mode=max`: Caches every intermediate multi-stage build layer (including test and compiler stages), rather than just the final output layers.
- `cache-from type=registry`: Pulls and matches cached layers from the remote repository during subsequent CI runs.

---

## 5. Syntax & Configuration Reference

### Docker Buildx Action Configuration

```yaml
- name: Set up Docker Buildx
  uses: docker/setup-buildx-action@v3
  with:
    driver-opts: |
      image=moby/buildkit:latest
      network=host
```

### Docker Metadata Action (Automated OCI SemVer Tagging)

```yaml
- name: Extract Docker metadata
  id: meta
  uses: docker/metadata-action@v5
  with:
    images: mycorp/web-api
    tags: |
      type=ref,event=branch
      type=ref,event=pr
      type=semver,pattern={{version}}
      type=semver,pattern={{major}}.{{minor}}
      type=sha,format=short
```

---

## 6. Complete Production GitHub Actions Workflow

Below is an enterprise-grade GitHub Actions pipeline featuring:
- Automated authentication with GitHub Container Registry (GHCR).
- Multi-architecture cross-compilation (linux/amd64 and linux/arm64).
- High-speed GitHub Actions Layer Caching (`type=gha`).
- Semantic version tagging via `docker/metadata-action`.
- Vulnerability scanning and SBOM generation.

```yaml
# .github/workflows/docker-build-push.yml
name: Build, Scan, and Push Production Image

on:
  push:
    branches: [main]
    tags: ['v*.*.*']
  pull_request:
    branches: [main]

env:
  REGISTRY: ghcr.io
  IMAGE_NAME: ${{ github.repository }}

jobs:
  build-and-push:
    runs-on: ubuntu-latest
    permissions:
      contents: read
      packages: write
      security-events: write

    steps:
      - name: Checkout Code
        uses: actions/checkout@v4

      - name: Set up QEMU (for Multi-Architecture Emulation)
        uses: docker/setup-qemu-action@v3
        with:
          platforms: linux/amd64,linux/arm64

      - name: Set up Docker Buildx
        uses: docker/setup-buildx-action@v3

      - name: Log in to GitHub Container Registry
        if: github.event_name != 'pull_request'
        uses: docker/login-action@v3
        with:
          registry: ${{ env.REGISTRY }}
          username: ${{ github.actor }}
          password: ${{ secrets.GITHUB_TOKEN }}

      - name: Extract Metadata & Tags
        id: meta
        uses: docker/metadata-action@v5
        with:
          images: ${{ env.REGISTRY }}/${{ env.IMAGE_NAME }}
          tags: |
            type=raw,value=latest,enable=${{ github.ref == 'refs/heads/main' }}
            type=ref,event=branch
            type=semver,pattern={{version}}
            type=sha,format=short

      - name: Build and Push OCI Image
        id: build-push
        uses: docker/build-push-action@v6
        with:
          context: .
          platforms: linux/amd64,linux/arm64
          push: ${{ github.event_name != 'pull_request' }}
          tags: ${{ steps.meta.outputs.tags }}
          labels: ${{ steps.meta.outputs.labels }}
          cache-from: type=gha
          cache-to: type=gha,mode=max
          provenance: true
          sbom: true

      - name: Verify Image Digest
        run: |
          echo "Published image digest: ${{ steps.build-push.outputs.digest }}"
```

---

## 7. GitLab CI Pipeline Example

A production `.gitlab-ci.yml` pipeline utilizing Kaniko for unprivileged rootless builds inside a Kubernetes runner.

```yaml
# .gitlab-ci.yml
stages:
  - build

build_image:
  stage: build
  image:
    name: gcr.io/kaniko-project/executor:v1.23.2-debug
    entrypoint: [""]
  script:
    - mkdir -p /kaniko/.docker
    - echo "{\"auths\":{\"$CI_REGISTRY\":{\"auth\":\"$(printf "%s:%s" "$CI_REGISTRY_USER" "$CI_REGISTRY_PASSWORD" | base64 | tr -d '\n')\"}}}" > /kaniko/.docker/config.json
    - >-
      /kaniko/executor
      --context "${CI_PROJECT_DIR}"
      --dockerfile "${CI_PROJECT_DIR}/Dockerfile"
      --destination "${CI_REGISTRY_IMAGE}:${CI_COMMIT_SHORT_SHA}"
      --destination "${CI_REGISTRY_IMAGE}:latest"
      --cache=true
      --cache-copy-layers=true
  rules:
    - if: $CI_COMMIT_BRANCH == "main"
```

---

## 8. Production Best Practices & Antipatterns

### Best Practices
- **Always Use `mode=max` for CI Remote Caches**: By default, `cache-to` only caches the final stage. Setting `mode=max` caches all builder, dependency, and test stages, maximizing CI cache hits.
- **Use Native Cloud Runners for Multi-Arch when Available**: Emulating ARM64 on x86 via QEMU is 4x-10x slower than running on native ARM64 runners (e.g. GitHub Actions ARM64 runners or AWS Graviton).
- **Tag with Git SHA and SemVer Simultaneously**: Always push both `image:v1.2.0` and `image:sha-a1b2c3d` to enable unambiguous traceability back to Git commits.
- **Never Push from Pull Request Builds**: Only build and scan PRs; execute `--push` only after code is merged into protected main branches.

### Anti-Patterns
- **Relying on `:latest` in CI/CD Deployments**: Deploying `:latest` makes rollback impossible and obscures which commit is running in production.
- **Using Docker-in-Docker with Privileged Mode in Multi-Tenant CI**: Running `--privileged` allows compromised CI jobs to execute container breakouts, gaining full root access over the host CI server.
- **Omitting `type=gha` or Remote Cache**: Re-downloading Node, Python, or Go packages on every single commit slows developer iteration and wastes network egress.

---

## 9. Common Pitfalls & Troubleshooting

| Symptom | Root Cause | Diagnosis Command | Permanent Fix |
|:---|:---|:---|:---|
| Multi-arch build takes 25 minutes in GitHub Actions | QEMU software CPU emulation is extremely slow for CPU-heavy compilation (Rust, C++, Go) | Inspect step timings | Use native ARM64 self-hosted runner, or build AMD64 only on standard runners. |
| DooD build fails with `file not found` during bind mount | Host socket mount resolves paths relative to the **host**, not the CI container | `docker inspect` mount path | Use volumes or pass absolute host working directory via `$HOST_PWD`. |
| CI build cache hits report 0% despite identical code | Missing `cache-from` specification, or remote cache was purged by registry lifecycle | Inspect BuildKit output for `CACHED` | Configure both `cache-from: type=gha` and `cache-to: type=gha,mode=max`. |
| GitHub Actions cache fails with `cache quota exceeded` | GitHub Actions caps cache storage at 10GB per repository | Check repo Settings -> Actions -> Caches | Purge stale branch caches or transition cache backend to `type=registry`. |
| Push to registry fails with `401 Unauthorized` | Registry token expired, lacks `write:packages` scope, or invalid actor name | Inspect `docker login` step output | Verify GITHUB_TOKEN has `packages: write` permission in workflow YAML. |
| Kaniko fails with `error checking push permissions` | Incorrect base64 format in `/kaniko/.docker/config.json` | Check Kaniko debug logs | Ensure JSON is formatted with valid `auth` token. |

---

## 10. Performance Tuning & Resource Optimization

1. **GitHub Actions Cache Optimization**:
   Keep cache sizes under control by isolating caches per branch:
   ```yaml
   cache-from: |
     type=gha,scope=${{ github.ref_name }}
     type=gha,scope=main
   cache-to: type=gha,mode=max,scope=${{ github.ref_name }}
   ```
   This ensures feature branches read from the `main` branch cache, but write to their own dedicated cache.
2. **Layer Pruning via Multi-Stage**:
   Ensure test files and fixtures are placed in an isolated stage that is omitted from the production push target, reducing final pushed layer size.

---

## 11. Security Hardening & Threat Model

```
+--------------------------------------------------------------+
| ATTACK SCENARIO: Privilege Escalation via CI Socket Mount    |
+--------------------------------------------------------------+
| Multi-tenant CI server runs untrusted PR code.               |
| CI job runs with `-v /var/run/docker.sock:/var/run/docker.sock`.|
| Malicious PR script executes:                                |
| `docker run -v /:/host-root -it alpine chroot /host-root`    |
| Attacker extracts SSH keys and AWS credentials of CI host!   |
|                                                              |
| PERMANENT MITIGATION:                                        |
| 1. Never mount docker.sock in untrusted PR workflows.        |
| 2. Use Rootless Kaniko / Buildah for untrusted CI runners.   |
+--------------------------------------------------------------+
```

---

## 12. Architectural Comparison Table: CI Execution Models

| Execution Model | Root Privileges Required? | BuildKit Support? | Performance | Security Rating |
|:---|:---|:---|:---|:---|
| **Docker-outside-of-Docker (DooD)**| **Full Root on Host** | Native | **Fastest** | **Dangerous (1/5)** |
| **Docker-in-Docker (DinD)** | **Requires `--privileged`** | Native | Moderate | **Poor (2/5)** |
| **Native GitHub Actions Runner**| Managed Host VM | **Native Buildx** | **Fast** | **High (4/5)** |
| **Kaniko (Daemonless)** | **Zero (Completely Rootless)**| Partial (Emulated)| Moderate | **Maximum (5/5)** |
| **Buildah (Rootless)** | **Zero (User namespaces)** | OCI Standard | Fast | **Maximum (5/5)** |

---

## 13. Real-World Scenario Walkthrough

### Scenario: Slashing Multi-Arch Build Times from 28 Minutes to 90 Seconds

#### The Bottleneck
A development team built multi-arch images (`linux/amd64,linux/arm64`) for an enterprise Go microservice in GitHub Actions. Because they ran on standard x86 runners, the ARM64 build used QEMU software emulation. Total build time: **28 minutes 12 seconds**. Developers avoided opening PRs due to agonizing CI feedback loops.

#### The Root Cause
1. Go is already a cross-compiler! It does not need QEMU emulation if CGO is disabled.
2. Caching was completely disabled; every run downloaded 1.5GB of dependencies from scratch.

#### The Architectural Solution
1. Configured Go cross-compilation in a multi-stage Dockerfile using `TARGETARCH` build arguments:
   ```dockerfile
   ARG TARGETARCH
   RUN CGO_ENABLED=0 GOARCH=$TARGETARCH go build -o /bin/app .
   ```
2. Implemented GitHub Actions Cache:
   ```yaml
   cache-from: type=gha
   cache-to: type=gha,mode=max
   ```
3. Enabled Buildx parallel compilation.

#### The Outcome
- Multi-arch build duration plummeted from **28 minutes 12 seconds** to **1 minute 24 seconds** (a 20x speedup!).
- QEMU CPU emulation overhead was completely eliminated.

---

## 14. Self-Assessment & Interview Questions

<details>
<summary><strong>1. What is the fundamental difference between Docker-in-Docker (DinD) and Docker-outside-of-Docker (DooD)?</strong></summary>

**Answer:**
DinD executes a nested, independent Docker daemon inside the container itself, requiring the `--privileged` flag and managing its own isolated container and image state. DooD mounts the host machine's Docker Unix socket (`/var/run/docker.sock`) into the container, delegating all operations to the underlying host daemon. DooD shares the host's image cache, but any host path mounts resolve relative to the host machine, not the container.
</details>

<details>
<summary><strong>2. Why is running Docker-in-Docker with `--privileged` considered a major security vulnerability in multi-tenant CI clusters?</strong></summary>

**Answer:**
`--privileged` grants the container access to all host kernel capabilities, devices, and sysfs/procfs interfaces. A malicious actor running arbitrary script code inside a privileged container can easily execute a container breakout, mount the host's root filesystem, read memory, and gain full unauthenticated root access over the host machine.
</details>

<details>
<summary><strong>3. What does `mode=max` do when configuring BuildKit's `cache-to`?</strong></summary>

**Answer:**
By default (`mode=min`), BuildKit only caches the layers belonging to the final output stage of a Dockerfile. Setting `mode=max` instructs BuildKit to cache the layers of *all* intermediate stages (including builders, linters, dependency extractors, and test runners). In multi-stage builds, `mode=max` is mandatory to ensure upstream stages are not unnecessarily rebuilt in CI.
</details>

<details>
<summary><strong>4. How does Kaniko build container images inside Kubernetes without requiring a Docker daemon?</strong></summary>

**Answer:**
Kaniko executes inside an unprivileged user-space container. It parses the Dockerfile, extracts the base image filesystem, executes `RUN` commands directly within its container userspace, takes filesystem snapshots after each instruction by comparing the current directory state against the base layer, and packages the diffs into standard OCI tarball layers pushed directly to the registry.
</details>

<details>
<summary><strong>5. Why is cross-compiling Go or Rust inside Docker using `TARGETARCH` preferred over QEMU emulation?</strong></summary>

**Answer:**
QEMU emulates an entire foreign CPU architecture (e.g. ARM64) instruction-by-instruction in software on an x86 host, which incurs a 4x to 10x CPU performance penalty. Modern compilers like Go and Rust can cross-compile natively for target architectures directly on x86 CPUs at 100% native clock speed. Using Docker build arguments (`TARGETARCH`) lets the native compiler produce foreign architecture binaries without QEMU emulation overhead.
</details>

<details>
<summary><strong>6. What is the purpose of `docker/metadata-action` in GitHub Actions?</strong></summary>

**Answer:**
`docker/metadata-action` automates the extraction and generation of standard OCI tags and labels based on Git events. It dynamically formats semantic version tags (e.g., `v1.2.3`, `1.2`, `1`), branch names, PR numbers, and short Git commit SHAs, injecting standard OpenContainers labels into image manifests.
</details>

<details>
<summary><strong>7. Why should continuous integration pipelines avoid pushing images tagged solely with `:latest`?</strong></summary>

**Answer:**
`:latest` is a mutable pointer. Overwriting `:latest` on every build makes rollbacks impossible, introduces race conditions during deployments, prevents deterministic auditing, and obscures which specific Git commit hash corresponds to the running production container. Production releases should always be tagged with immutable Git SHAs or Semantic Version numbers.
</details>

<details>
<summary><strong>8. How does `provenance: true` and `sbom: true` in `docker/build-push-action` enhance supply chain security?</strong></summary>

**Answer:**
- `provenance: true`: Generates SLSA-compliant build attestations documenting how, where, and from what source commit the image was built, cryptographically signed by the CI runner.
- `sbom: true`: Automatically scans the image during build and attaches a Software Bill of Materials (SBOM) listing all installed OS packages and language dependencies to the image index in the registry.
</details>

---

## 15. Hands-on Practice Exercises

<details>
<summary><strong>Exercise 1: Inspecting Remote Registry Cache Export via CLI</strong></summary>

**Goal:** Build a container using BuildKit and export its cache to a local folder or registry.

**Step-by-step Solution:**
1. Create a dummy Dockerfile:
   ```dockerfile
   FROM alpine:3.21
   RUN apk add --no-cache curl
   ```
2. Build with local directory cache export:
   ```bash
   docker buildx build \
     --cache-to type=local,dest=/tmp/docker-cache,mode=max \
     --cache-from type=local,src=/tmp/docker-cache \
     -t cache-demo .
   ```
3. Inspect `/tmp/docker-cache` to verify layer metadata and tar caches exist.
</details>

<details>
<summary><strong>Exercise 2: Simulating DooD Path Mismatch Error</strong></summary>

**Goal:** Understand why mounting host socket inside a container breaks host bind mounts.

**Step-by-step Solution:**
1. Run a container with socket mount:
   ```bash
   docker run -it --rm -v /var/run/docker.sock:/var/run/docker.sock docker:cli sh
   ```
2. Inside container, create `/tmp/test.txt` and attempt to mount it:
   ```sh
   echo "hello" > /tmp/test.txt
   docker run --rm -v /tmp/test.txt:/test.txt alpine cat /test.txt
   ```
3. Notice error or directory creation: Docker looks for `/tmp/test.txt` on the **host machine**, not inside the CLI container!
</details>

<details>
<summary><strong>Exercise 3: Authoring a Clean Multi-Arch Build Matrix</strong></summary>

**Goal:** Build a native multi-platform image for AMD64 and ARM64.

**Step-by-step Solution:**
1. Verify Buildx builder: `docker buildx ls`
2. Create multi-arch builder:
   ```bash
   docker buildx create --name multi-builder --use
   docker buildx inspect --bootstrap
   ```
3. Build for both platforms:
   ```bash
   docker buildx build --platform linux/amd64,linux/arm64 -t test-multi:v1 .
   ```
</details>

<details>
<summary><strong>Exercise 4: Generating an OCI Metadata Summary</strong></summary>

**Goal:** Inspect OCI labels injected by standard CI tooling.

**Step-by-step Solution:**
1. Inspect image labels:
   ```bash
   docker inspect <image> --format '{{json .Config.Labels}}' | jq .
   ```
</details>

<details>
<summary><strong>Exercise 5: Validating GHA Workflow Syntax via Actionlint</strong></summary>

**Goal:** Lint a GitHub Actions workflow file locally.

**Step-by-step Solution:**
1. Run `actionlint .github/workflows/*.yml` to catch parameter or permission errors before pushing.
</details>

<details>
<summary><strong>Exercise 6: Testing Kaniko Execution Locally via Docker</strong></summary>

**Goal:** Run a mock Kaniko build container locally.

**Step-by-step Solution:**
1. Run Kaniko executor container pointing to local directory:
   ```bash
   docker run --rm -v $(pwd):/workspace \
     gcr.io/kaniko-project/executor:latest \
     --no-push \
     --context dir:///workspace
   ```
</details>

---

## 16. Summary & Reference Cheat Sheet

### Golden Rules of Docker in CI/CD
1. **Always use BuildKit**: Set `DOCKER_BUILDKIT=1`.
2. **Always enable remote caching**: Use `cache-to: type=gha,mode=max` or `type=registry`.
3. **Never push from unmerged PRs**: Build and test in PR; push on merge to `main`.
4. **Tag with Git SHA + SemVer**: Never rely purely on `:latest`.
5. **Avoid `--privileged` DinD in shared runners**: Use daemonless builders (Kaniko/Buildah) for rootless security.
6. **Include SBOM and Provenance**: Enable `provenance: true` and `sbom: true` for supply chain compliance.

---

[← Previous: 02-image-optimisation.md](02-image-optimisation.md) | [Next: 04-image-security.md →](04-image-security.md)
