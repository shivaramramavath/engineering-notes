# DevOps Curriculum Baseline Facts & Technical Specifications

| Fact Category | Specification / Standard | Verified Value (Nov 2026) | Source / Reference URL |
|---|---|---|---|
| **Linux Kernel** | Active LTS Versions | Linux 6.12 LTS, Linux 6.6 LTS, Linux 6.1 LTS, Linux 7.3 LTS | https://www.kernel.org/ |
| **Linux Reference Distribution** | Ubuntu LTS Baseline | Ubuntu 24.04 LTS (Noble Numbat, Kernel 6.8/6.11 HWE, systemd 255.4) | https://releases.ubuntu.com/24.04/ |
| **Linux Init & Service Manager** | systemd Standard | systemd v255 / v256 (cgroup v2 unified hierarchy default) | https://systemd.io/ |
| **Container Engine** | Docker Engine | Docker Engine v28 / v29 (containerd v2.0 image store default) | https://docs.docker.com/engine/release-notes/ |
| **Container Orchestration (Local)**| Docker Compose | Docker Compose v2.30+ (`compose.yaml` specification standard) | https://docs.docker.com/compose/ |
| **Container Build Tool** | Docker Buildx & BuildKit | Buildx v0.19+, BuildKit v0.17+ (multi-platform, OCI provenance) | https://github.com/docker/buildx |
| **Container Standard Specs** | OCI Specifications | OCI Image Spec v1.1.0, Runtime Spec v1.2.0 | https://opencontainers.org/ |
| **Version Control System** | Git Core | Git 2.56.x / 2.47+ (default branch `main`, scalar builtin) | https://git-scm.com/ |
| **CI/CD Platform** | GitHub Actions Runner | `ubuntu-24.04` & `ubuntu-22.04` (Node.js 20 actions runtime baseline) | https://github.com/actions/runner-images |
| **Web Server / Reverse Proxy** | Nginx Mainline / Stable | Nginx Mainline 1.31.x / Stable 1.30.x (HTTP/3 QUIC RFC 9000 support) | https://nginx.org/en/download.html |
| **Cloud CLI** | AWS CLI v2 | AWS CLI v2.22+ / v2.24+ (Python 3.12 bundled runtime) | https://docs.aws.amazon.com/cli/latest/ |
| **Cloud Linux OS** | Amazon Linux 2023 | AL2023 (deterministic releases via versioned repositories, dnf) | https://aws.amazon.com/linux/amazon-linux-2023/ |
| **Infrastructure as Code** | OpenTofu & Terraform | OpenTofu 1.8+ (MPL 2.0) / Terraform 1.9+ | https://opentofu.org/ |

---

## 1. Operating System & Linux Invariants
- **cgroup v2 Unified Hierarchy**: Legacy cgroup v1 (`/sys/fs/cgroup/memory`, etc.) is fully deprecated in modern distros (Ubuntu 22.04+, Debian 12+, RHEL 9+). All memory, CPU, and I/O controllers reside in `/sys/fs/cgroup/`.
- **systemd vs SysVinit**: Modern Linux services are managed via systemd unit files (`/etc/systemd/system/*.service`); `service` and `/etc/init.d/` commands are compatibility shims.
- **Package Management**: Debian/Ubuntu uses `apt` (with `.sources` deb822 format in Ubuntu 24.04+), RHEL/Fedora/AL2023 uses `dnf` (replacing legacy `yum`).
- **Security Subsystems**: AppArmor (Ubuntu/Debian default) and SELinux (RHEL/CentOS/Fedora/AL2023 default) govern mandatory access controls (MAC).

---

## 2. Containerization (Docker) Invariants
- **Compose Specification**: File name standard is canonical `compose.yaml` (or `compose.yml`). Legacy `docker-compose.yml` with top-level `version: '3.8'` is deprecated.
- **CLI Syntax**: Space separator (`docker compose up`) is standard. Legacy hyphenated `docker-compose` (v1 Python script) is completely end-of-life.
- **Image Manifests**: Multi-platform images use OCI Index / Docker Manifest Lists supporting `linux/amd64` and `linux/arm64` (Apple Silicon & AWS Graviton).
- **Non-Root Execution**: Production Dockerfiles must create an unprivileged user/group (`USER appuser`) to mitigate container breakout attacks.

---

## 3. Git & GitHub Actions Invariants
- **Default Branch**: Global default branch name is `main` (replacing legacy `master`).
- **Actions Action Runtime**: GitHub Actions deprecates Node 12 and Node 16 runtimes. All modern actions execute on Node 20 (`runs: using: 'node20'`).
- **Token Security**: Default `GITHUB_TOKEN` permissions must be explicitly locked down with `permissions: read-all` or `contents: read` at the workflow root.
- **OIDC Cloud Authentication**: Cloud deployments (AWS, Azure, GCP) must use OpenID Connect (OIDC) with `aws-actions/configure-aws-credentials` rather than hardcoding static IAM user access keys in repository secrets.

---

## 4. Nginx Invariants
- **HTTP/3 & QUIC**: Native QUIC transport protocol support is integrated via `listen 443 quic reuseport;` alongside `http3 on;` in modern mainline/stable builds.
- **TLS Configuration**: TLS 1.0 and 1.1 are deprecated and insecure; modern production configurations enforce TLS 1.2 and TLS 1.3 only with secure cipher suites.
- **Process Architecture**: Event-driven asynchronous master-worker model using Linux `epoll` or FreeBSD `kqueue`.

---

## 5. AWS Cloud Invariants
- **IAM Best Practices**: Root user credentials must never have active access keys. Workloads must use IAM Roles with temporary credentials (STS).
- **Default VPCs**: Standard default VPC exists per region, but production enterprise architectures must provision custom multi-AZ VPCs with public/private subnet segmentation.
- **Compute Architectures**: AWS Graviton (Graviton3 / Graviton4 on `arm64`) provides up to 40% better price-performance compared to x86_64 across EC2, Lambda, and ECS.
