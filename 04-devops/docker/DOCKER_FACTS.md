# Docker Track Technical Facts & Verified Baselines

## 1. Tooling & Platform Baselines (Verified: November 2026)

| Tool / Component | Baseline Version | Status | Source / Verification |
|---|---|---|---|
| **Docker Engine** | 28.x / 29.x LTS | Active Stable | https://docs.docker.com/engine/release-notes/ |
| **Docker CLI** | 29.8+ (API v1.56) | Active Stable | `docker version` output |
| **Docker Compose** | Compose v2 (v2.30+ / Compose Spec) | Default (`docker compose`) | https://docs.docker.com/compose/ |
| **Docker Buildx** | v0.21+ / BuildKit v0.19+ | Default builder | https://docs.docker.com/build/buildx/ |
| **Container Runtimes** | containerd 2.0+, runc 1.2+ | Core OCI Foundation | https://containerd.io/ |
| **Linux Kernel Base** | Kernel 6.8+ / 6.12 LTS | Recommended for cgroup v2 | Ubuntu 24.04 LTS / Debian 12 |

---

## 2. Deprecations, Obsolescence & Prohibitions

| Obsolete Feature / Tool | Status | Replacement / Modern Standard |
|---|---|---|
| `docker-compose` (v1 Python script) | **Deprecated & Unsupported** | `docker compose` (v2 Go CLI plugin) |
| `version:` key in `compose.yaml` | **Deprecated & Obsolete** | Omit `version:` completely; Compose Specification is standard |
| `docker-machine` | **Archived & Dead** | Docker Desktop, Cloud CLI (AWS/Azure/GCP), or Vagrant/Multipass |
| Legacy Docker Builder (`DOCKER_BUILDKIT=0`) | **Deprecated** | BuildKit (`DOCKER_BUILDKIT=1`) is default since Engine 23.0+ |
| `--link` flag in `docker run` | **Legacy / Deprecated** | User-defined bridge networks with embedded DNS |
| `MAINTAINER` instruction in Dockerfile | **Deprecated** | `LABEL maintainer="name <email>"` |
| `devicemapper` storage driver | **Removed** | `overlay2` storage driver |
| Storing secrets in `ENV` / `ARG` | **Security Anti-Pattern** | BuildKit `--mount=type=secret` or Compose `secrets:` |
| Root-owned containers (`USER root`) | **Security Anti-Pattern** | Explicit unprivileged user (`USER appuser:appgroup`) |
| Mounting `/var/run/docker.sock` | **Critical Security Hazard** | Rootless Docker, Docker-in-Docker with TLS, or Kaniko/Buildah in CI |

---

## 3. Docker Hub Rate Limits & Commercial Plans

- **Anonymous Unauthenticated Pulls**: 100 pulls per 6 hours per IP address.
- **Authenticated Docker ID (Free Tier)**: 200 pulls per 6 hours per user account.
- **Docker Pro / Team / Business Plans**: Unlimited pulls.
- *Citation*: [Docker Hub Pricing & Rate Limits Documentation](https://docs.docker.com/docker-hub/download-rate-limit/)

---

## 4. Modern Compose Specification Capabilities

- **`compose.yaml` vs `docker-compose.yml`**: `compose.yaml` is the preferred default filename under the Compose Specification.
- **`watch` (Compose Watch)**: Real-time file sync, rebuild, and container restarts on source changes without full restart cycles.
- **`include`**: Modular compose file composition (`include: [ ../common/compose.yaml ]`) replacing fragile symlinks or script concatenations.
- **`profiles`**: Selectively start service sets (`profiles: ["debug", "testing"]`) with `docker compose --profile debug up`.
- **`develop`**: Development lifecycle hooks for hot-reloading and asset synchronization.

---

## 5. Security & Isolation Primitives

- **Linux Namespaces**: PID (process tree), NET (network interfaces/routing), MNT (filesystem mounts), IPC (shared memory), UTS (hostname), USER (UID/GID mapping).
- **Control Groups (cgroups v2)**: Unified hierarchy controlling memory limits (`--memory`), CPU shares (`--cpus`), and block I/O.
- **Capabilities**: Linux kernel capabilities (`--cap-drop=ALL --cap-add=NET_BIND_SERVICE`).
- **Seccomp & AppArmor**: System call filtering (`default.json` seccomp profile blocks ~44 dangerous syscalls).
- **Rootless Mode**: Running the Docker daemon and containers entirely inside a user namespace without root privileges.
- **containerd Image Store**: Enabling containerd image snapshotters in Docker Engine (`"features": { "containerd-snapshotter": true }`) for direct multi-platform storage and Wasm support.
