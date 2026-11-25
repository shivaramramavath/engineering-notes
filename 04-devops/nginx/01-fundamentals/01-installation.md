<!--
================================================================================
Metadata:
  Title: NGINX Installation: Package Repositories, Docker & Source Builds
  Track: NGINX & Traffic Engineering / DevOps
  Difficulty: Beginner
  Estimated Time: 45 minutes
  Prerequisites: None
  Key Terminology: Mainline, Stable, PGP Signatures, APT, DNF, Compilation Flags, OpenSSL, Modules
================================================================================
-->

# NGINX Installation: Package Repositories, Docker & Source Builds

[Back to Track Overview](../README.md) | [Back to Module Overview](./README.md) | [02-config-structure.md ->](./02-config-structure.md)

---

## Table of Contents
- [1. What & Why](#1-what--why)
- [2. Core Concepts](#2-core-concepts)
- [3. How It Works Internally](#3-how-it-works-internally)
- [4. Syntax & Usage](#4-syntax--usage)
- [5. Practical Examples](#5-practical-examples)
- [6. Best Practices](#6-best-practices)
- [7. Common Mistakes & Troubleshooting](#7-common-mistakes--troubleshooting)
- [8. Performance, Security & Cost Implications](#8-performance-security--cost-implications)
- [9. Comparison Table](#9-comparison-table)
- [10. Real-World Interview Questions](#10-real-world-interview-questions)
- [11. Practice Exercises](#11-practice-exercises)
- [12. Summary & Quick Reference Cheat Sheet](#12-summary--quick-reference-cheat-sheet)
- [13. Further Reading & Official Documentation](#13-further-reading--official-documentation)

---

## 1. What & Why

NGINX powers over a third of the world's busiest websites, functioning as a high-performance HTTP server, reverse proxy, layer-4 TCP/UDP load balancer, and edge ingress controller.

However, many engineers deploy NGINX simply by running `apt install nginx` without understanding the implications:
- Stock operating system repositories (such as Debian or Ubuntu LTS) freeze software versions for multi-year stability cycles. For instance, Ubuntu 24.04 LTS ships NGINX `1.24.0`, which lacks modern HTTP/3 QUIC transport enhancements, modern njs engine updates, and standalone `http2 on;` syntax support.
- Choosing between **Mainline** (active feature releases) and **Stable** (annual branches with critical backports) dictates whether your infrastructure can leverage modern web standards or remains bounded by legacy protocol constraints.
- Custom security profiles or edge proxy features (such as Brotli compression, GeoIP2, or OpenSSL 3.x with Post-Quantum cryptography) often necessitate building NGINX from source with specific compilation flags.

---

## 2. Core Concepts

### Mainline vs. Stable Release Branches
The NGINX project maintains two active branches:
1. **Mainline Branch (Odd minor versions, e.g., `1.31.x`)**:
   - The primary active development stream.
   - Receives new features, performance enhancements, and regular bug fixes first.
   - Despite the name, NGINX officially recommends the Mainline branch for all general production environments because security patches and protocol evolutions land here immediately.
2. **Stable Branch (Even minor versions, e.g., `1.30.x`)**:
   - Forked annually from Mainline (typically in April or May).
   - Receives **no new features**; only critical security advisories (CVEs) and major bug fixes are backported.
   - Ideal for conservative enterprise environments with strict regulatory freezes and lengthy certification audits.

```
+-------------------------------------------------------------+
| NGINX RELEASE CYCLE TIMELINE                                |
|                                                             |
| Mainline: 1.29.x ----> 1.31.0 ----> 1.31.x (Active Dev)     |
|                          |                                  |
|                   Forked May 2026                           |
|                          v                                  |
| Stable:               1.30.0 -----> 1.30.8 (CVE Fixes Only) |
+-------------------------------------------------------------+
```

### Installation Methodologies
- **Official NGINX Package Repositories (Debian/Ubuntu/RHEL)**: Best balance of modern features, automated security updates via package managers, and signed PGP verification.
- **Docker Container Images**: Ideal for immutable deployments, Kubernetes pods, and local staging pipelines (`nginx:alpine` or `nginx:bookworm`).
- **Compiling from Source**: Mandatory when embedding non-dynamic third-party modules (e.g., specific cryptographic hardware accelerators, custom Lua/C modules).

---

## 3. How It Works Internally

### Source Compilation & Module Binding
Unlike traditional monolithic software, NGINX uses a modular C architecture. When NGINX is compiled from source, the `./configure` script determines which modules are statically compiled into the core binary (`/usr/sbin/nginx`) versus which are compiled as shared dynamic modules (`.so` files placed in `/usr/lib/nginx/modules`):

```
+--------------------------------------------------------------------------+
| COMPILATION & ARCHITECTURE TOPOLOGY                                      |
|                                                                          |
| [ ./configure Flags ]                                                    |
|  ├── --with-http_ssl_module         -> Links libssl/libcrypto (OpenSSL)  |
|  ├── --with-http_v2_module          -> Enables HTTP/2 protocol engine    |
|  ├── --with-http_v3_module          -> Enables HTTP/3 (QUIC/UDP) engine  |
|  ├── --with-threads                 -> Enables Worker Thread Pools (AIO) |
|  └── --add-dynamic-module=/src/...  -> Emits external .so dynamic module |
|            |                                                             |
|            v                                                             |
| [ make & make install ]                                                  |
|  ├── Output Binary: /usr/sbin/nginx (Core Engine + Static Modules)       |
|  ├── Dynamic Modules: /usr/lib/nginx/modules/*.so                       |
|  └── Configuration: /etc/nginx/nginx.conf                                |
+--------------------------------------------------------------------------+
```

During startup, the master process inspects compiled-in modules (`nginx -V`) and loads external dynamic libraries declared via the `load_module` directive in the main context.

---

## 4. Syntax & Usage

### Installing Official NGINX Mainline on Ubuntu / Debian
```bash
# 1. Install prerequisites
sudo apt update
sudo apt install -y curl gnupg2 ca-certificates lsb-release ubuntu-keyring

# 2. Import official NGINX PGP signing key
curl https://nginx.org/keys/nginx_signing.key | gpg --dearmor \
    | sudo tee /usr/share/keyrings/nginx-archive-keyring.gpg >/dev/null

# 3. Verify key fingerprint (must match 573B FD6B 3D8F BC64 1079 A6AB ABF5 BD82 7BD9 BF62)
gpg --dry-run --quiet --no-keyring --import --import-options import-show /usr/share/keyrings/nginx-archive-keyring.gpg

# 4. Set up official APT repository (Mainline branch)
echo "deb [signed-by=/usr/share/keyrings/nginx-archive-keyring.gpg] \
http://nginx.org/packages/mainline/ubuntu `lsb_release -cs` nginx" \
    | sudo tee /etc/apt/sources.list.d/nginx.list

# 5. Pin repository priority over stock Ubuntu repos
cat << 'EOF' | sudo tee /etc/apt/preferences.d/99nginx
Package: *
Pin: origin nginx.org
Pin: release o=nginx
Pin-Priority: 900
EOF

# 6. Install NGINX
sudo apt update
sudo apt install -y nginx
```

### Installing Official NGINX on RHEL 9 / Rocky Linux 9
```bash
# 1. Configure the NGINX repository file
cat << 'EOF' | sudo tee /etc/yum.repos.d/nginx.repo
[nginx-stable]
name=nginx stable repo
baseurl=http://nginx.org/packages/centos/9/$basearch/
gpgcheck=1
enabled=1
gpgkey=https://nginx.org/keys/nginx_signing.key
module_hotfixes=true
EOF

# 2. Install NGINX via DNF
sudo dnf install -y nginx

# 3. Enable and start systemd service
sudo systemctl enable --now nginx
```

### Verifying Compilation Arguments
```bash
# Print version number only
nginx -v
# Output: nginx version: nginx/1.31.10

# Print version, compiler details, and all compiled-in configure flags
nginx -V
```

---

## 5. Practical Examples

### Example 1: Verifying Systemd Service State and Firewall Rules
```bash
# Verify systemd service status
sudo systemctl status nginx --no-pager

# Enable HTTP (80) and HTTPS (443) through UFW firewall
sudo ufw allow 'Nginx Full'
sudo ufw status

# Verify local socket binding (inspecting master process listener)
ss -tulpn | grep nginx
# Output:
# tcp  LISTEN  0  511  0.0.0.0:80   0.0.0.0:*  users:(("nginx",pid=1204,fd=6))
```

### Example 2: Running NGINX via Production-Ready Docker Container
```bash
# Launch a minimal Alpine NGINX container with custom volume mount and strict read-only root
docker run -d \
  --name edge-proxy \
  -p 8080:80 \
  -p 8443:443 \
  -v /var/www/html:/usr/share/nginx/html:ro \
  -v /etc/nginx/custom.conf:/etc/nginx/nginx.conf:ro \
  --read-only \
  --tmpfs /var/cache/nginx:rw,noexec,nosuid,size=100m \
  --tmpfs /var/run:rw,noexec,nosuid,size=10m \
  nginx:1.30-alpine

# Test local endpoint
curl -I http://localhost:8080
# HTTP/1.1 200 OK
# Server: nginx/1.30.8
```

### Example 3: Building NGINX from Source with HTTP/3 & Custom Modules
```bash
# 1. Install build dependencies (Ubuntu 24.04)
sudo apt update && sudo apt install -y \
  build-essential \
  libpcre2-dev \
  zlib1g-dev \
  libssl-dev \
  git \
  curl

# 2. Download and unpack source tarball
cd /tmp
curl -LO https://nginx.org/download/nginx-1.31.10.tar.gz
tar -zxvf nginx-1.31.10.tar.gz && cd nginx-1.31.10

# 3. Configure compilation with HTTP/2, HTTP/3, and SSL
./configure \
  --prefix=/etc/nginx \
  --sbin-path=/usr/sbin/nginx \
  --modules-path=/usr/lib/nginx/modules \
  --conf-path=/etc/nginx/nginx.conf \
  --error-log-path=/var/log/nginx/error.log \
  --http-log-path=/var/log/nginx/access.log \
  --pid-path=/run/nginx.pid \
  --lock-path=/run/lock/subsys/nginx \
  --user=nginx \
  --group=nginx \
  --with-threads \
  --with-file-aio \
  --with-http_ssl_module \
  --with-http_v2_module \
  --with-http_v3_module \
  --with-http_realip_module \
  --with-http_stub_status_module

# 4. Compile and install
make -j$(nproc)
sudo make install

# Verify custom binary
nginx -V
```

### Example 4: Diagnosing Package Priority Collisions (`apt-cache policy`)
```bash
# Problem: Running apt install nginx still pulls outdated Ubuntu distribution package
apt-cache policy nginx

# Sample Issue Output:
# nginx:
#   Installed: 1.24.0-2ubuntu7
#   Candidate: 1.24.0-2ubuntu7
#   Version table:
#      1.31.10-1~noble 900
#         500 http://nginx.org/packages/mainline/ubuntu noble/nginx amd64 Packages
#  *** 1.24.0-2ubuntu7 500
#         500 http://archive.ubuntu.com/ubuntu noble/main amd64 Packages

# Remediation: Ensure pinning preference is set to priority 900+ in /etc/apt/preferences.d/99nginx
sudo apt install -y nginx=1.31.10-1~noble
```

### Example 5: Creating the Hardened `nginx` System User
```bash
# When compiling from source, create the dedicated unprivileged system user
sudo groupadd --system nginx
sudo useradd --system \
  --gid nginx \
  --no-create-home \
  --home-dir /nonexistent \
  --shell /usr/sbin/nologin \
  --comment "NGINX System Service Account" \
  nginx

# Verify user creation
id nginx
# Output: uid=993(nginx) gid=993(nginx) groups=993(nginx)
```

---

## 6. Best Practices

- **Use the Official NGINX Repository**: Avoid default OS distribution repositories for production reverse proxies. Ubuntu LTS freezes NGINX for 5 years; official NGINX mainline/stable repos provide current security fixes and protocol implementations.
- **Run as Unprivileged `nginx` User**: Ensure worker processes run as `nginx` or `www-data`. Never allow worker processes to inherit root execution credentials.
- **Pin Package Versions in CI/CD**: When building container images or deploying with Ansible, pin the exact minor package version (e.g., `nginx=1.30.8*`) to prevent unexpected config syntax changes during automated builds.
- **Always Keep Build Dependencies Out of Production Images**: If compiling from source, use multi-stage Docker builds. Build the binary in a compiler container, then copy only the compiled executable and shared libraries into a clean, minimal runtime image.
- **Verify Signatures on Downloads**: When downloading source archives or binaries directly, always check SHA256 checksums and PGP signatures against the official NGINX key.

---

## 7. Common Mistakes & Troubleshooting

| Mistake / Issue | Why It Fails | Practical Remedy |
| :--- | :--- | :--- |
| Installing via `apt install nginx` on Ubuntu LTS and expecting HTTP/3 | Stock Ubuntu repos ship older NGINX versions (`1.24.x`) compiled without modern HTTP/3 or standalone `http2 on;` support. | Add official NGINX mainline repo (`nginx.org/packages/mainline`). |
| Port 80/443 conflict: `bind() to 0.0.0.0:80 failed (98: Address already in use)` | Another web server (Apache, lighttpd, or an existing instance of NGINX) is already bound to port 80. | Run `sudo ss -tulpn \| grep :80` to identify and stop the conflicting process. |
| `nginx.service failed: Unit nginx.service is masked` | The systemd service was disabled and masked, preventing startup. | Run `sudo systemctl unmask nginx && sudo systemctl enable --now nginx`. |
| GPG error: `The following signatures couldn't be verified because the public key is not available` | The official NGINX PGP signing key was not imported into `/usr/share/keyrings/`. | Re-import the official key: `curl -s https://nginx.org/keys/nginx_signing.key \| gpg --dearmor ...` |
| Compiled binary cannot find OpenSSL libraries at runtime | Built against custom OpenSSL without setting `rpath` or running `ldconfig`. | Run `sudo ldconfig` or pass `-Wl,-rpath=/path/to/openssl/lib` during `./configure`. |
| Missing PID file directory: `nginx: [error] open() "/run/nginx.pid" failed` | The `/run` or `/var/run` directory permissions prevent the unprivileged user from writing the PID, or directory was wiped. | Verify permissions and ensure `systemd` creates the runtime directory via `RuntimeDirectory=nginx`. |

---

## 8. Performance, Security & Cost Implications

- **Performance**: NGINX compiled with `--with-pcre2` and `--with-threads` enables multi-threaded asynchronous I/O (AIO), preventing disk reads from stalling the worker event loop. Package repository builds enable these flags by default.
- **Security**: Official repositories receive security backports within hours of a CVE publication. Outdated distro builds (such as unpatched 1.24.x) remain vulnerable to HTTP/2 Rapid Reset and use-after-free CVEs until backported by downstream OS maintainers.
- **Cost**: Efficient NGINX deployments handle 50,000+ concurrent connections per compute node with under 512 MB of RAM, dramatically reducing cloud compute instance requirements compared to multi-process servers like Apache MPM Prefork.

---

## 9. Comparison Table

| Attribute | Stock OS Repos (Ubuntu/RHEL) | Official NGINX Repos | Source Compilation | Docker (`nginx:alpine`) |
| :--- | :--- | :--- | :--- | :--- |
| **Version Freshness** | Outdated (LTS freeze) | Current (Mainline/Stable) | Any target release | Immediate |
| **Installation Speed** | Instant (`apt install`) | Fast (1 repo command) | Slow (5–15 min build) | Instant (`docker run`) |
| **Custom Modules** | Third-party apt packages | Dynamic modules only | Full custom flexibility| Custom Dockerfile build |
| **Maintenance Overhead**| Handled by OS updates | Handled by OS updates | Manual rebuilds on CVEs | Re-pull container tag |
| **HTTP/3 Support** | Often missing/experimental | Supported out-of-the-box | Requires OpenSSL 3.x | Supported |
| **Best Used For** | Basic development labs | Production Bare-Metal/VMs | Specialized edge WAFs | Containerized Cloud / K8s |

---

## 10. Real-World Interview Questions

<details>
<summary><strong>1. What is the fundamental difference between NGINX Mainline and NGINX Stable branches? Which should you deploy in production?</strong></summary>

**Answer**:
- **Mainline**: The active development branch containing all new features, bug fixes, protocol implementations (such as HTTP/3), and performance enhancements. It is updated every few weeks.
- **Stable**: Forked once a year from Mainline. It receives **no new features**; only critical security patches and major regressions are backported.

*Recommendation*: NGINX officially recommends the **Mainline** branch for almost all production deployments. The name "Stable" refers to configuration and API stability (meaning directives and behavior will not change during its lifecycle), not freedom from software bugs. Mainline is just as stable in terms of reliability, while receiving the latest optimizations and security protections first.
</details>

<details>
<summary><strong>2. Why can running `apt install nginx` on Ubuntu LTS cause operational issues in modern infrastructure?</strong></summary>

**Answer**:
Canonical freezes package versions at the time of an Ubuntu LTS release. For example, Ubuntu 24.04 LTS freezes NGINX at version 1.24.0.
This introduces operational hurdles:
1. Version 1.24.0 lacks support for the modern `http2 on;` directive introduced in 1.25.1.
2. It lacks full production-grade HTTP/3 (QUIC) capabilities.
3. Modern NGINX JavaScript (njs) with the QuickJS engine is unavailable.
4. Bug fixes and performance patches are delayed until Canonical backports them into Ubuntu security channels.

Best practice is to configure the official `nginx.org` package repository to receive current releases directly.
</details>

<details>
<summary><strong>3. How does `nginx -V` differ from `nginx -v`?</strong></summary>

**Answer**:
- `nginx -v` (lowercase): Prints only the version string (e.g., `nginx version: nginx/1.30.8`).
- `nginx -V` (uppercase): Prints the version string, the exact compiler version used (gcc/clang), the architecture, and every single `./configure` argument used during the build.

Running `nginx -V` is essential for diagnosing missing features (e.g., verifying if `--with-http_v3_module` or `--with-http_ssl_module` was included) and checking module search paths (`--modules-path`).
</details>

<details>
<summary><strong>4. What build flags are mandatory when compiling NGINX to support HTTP/3 over QUIC?</strong></summary>

**Answer**:
To build NGINX with HTTP/3 support:
1. `--with-http_v3_module`: Compiles the core QUIC and HTTP/3 transport module.
2. Cryptographic library supporting QUIC: Standard OpenSSL before 3.2 did not support QUIC APIs. You must build against an SSL library with QUIC support (such as BoringSSL, LibreSSL, or OpenSSL 3.2+ with `--with-openssl`).
3. `--with-cc-opt` and `--with-ld-opt`: To link against the custom QUIC-capable TLS library.
</details>

<details>
<summary><strong>5. Explain the purpose of pinning the NGINX package repository in `/etc/apt/preferences.d/`.</strong></summary>

**Answer**:
When both the Ubuntu distribution repository and the official `nginx.org` repository provide packages named `nginx`, the APT resolver compares priority metrics and version numbers.

If Ubuntu packages assign an epoch or higher build number, `apt upgrade` might attempt to replace your official NGINX mainline binary with an older distribution build. Creating a pinning preference file in `/etc/apt/preferences.d/` giving `origin nginx.org` a priority of `900` forces APT to always prefer packages from the official NGINX repository over distribution repositories.
</details>

<details>
<summary><strong>6. What are dynamic modules in NGINX and how are they activated?</strong></summary>

**Answer**:
Dynamic modules are compiled as standalone `.so` shared object libraries instead of being baked directly into the primary NGINX binary.

They are enabled by compiling with `--add-dynamic-module=/path/to/source` and placed into `/usr/lib/nginx/modules/`. To activate a dynamic module in production, you must declare the `load_module` directive at the very top of `nginx.conf` in the **main** context:
```nginx
load_module modules/ngx_http_geoip2_module.so;
```
If `load_module` is omitted, the directives provided by the dynamic module will produce `unknown directive` errors during `nginx -t`.
</details>

<details>
<summary><strong>7. Why should an unprivileged system account like `nginx` be created with `/usr/sbin/nologin`?</strong></summary>

**Answer**:
The `nginx` system account runs worker processes that process untrusted network traffic from the internet. If an attacker achieves Remote Code Execution (RCE) by exploiting a buffer overflow or web application flaw, the attacker's shell executes under the `nginx` UID.

Setting the user's shell to `/usr/sbin/nologin` (or `/bin/false`) and omitting a home directory prevents an attacker from establishing an interactive SSH or terminal login session using that account, limiting the attacker's ability to maintain persistence.
</details>

<details>
<summary><strong>8. How do you verify the integrity of downloaded NGINX packages before installation?</strong></summary>

**Answer**:
1. Check the official PGP key fingerprint (`573B FD6B 3D8F BC64 1079 A6AB ABF5 BD82 7BD9 BF62`) published on `nginx.org/en/pgp_keys.html`.
2. Use `gpg --verify` against the detached signature file (`.asc`) for source tarballs.
3. In Debian/Ubuntu, store the de-armored key in `/usr/share/keyrings/nginx-archive-keyring.gpg` and specify `[signed-by=...]` in the `sources.list.d` entry. APT automatically verifies package checksums against the signed `Release.gpg` index.
</details>

---

## 11. Practice Exercises

<details>
<summary><strong>Exercise 1 (Easy): Query Installed NGINX Build Flags</strong></summary>

**Task**: Execute the CLI command to query your system's NGINX build configuration and extract all enabled protocol modules (`http_ssl`, `http_v2`, `http_v3`).

**Solution**:
```bash
nginx -V 2>&1 | tr ' ' '\n' | grep -E "http_(ssl|v2|v3)_module"
```
</details>

<details>
<summary><strong>Exercise 2 (Easy): Verify Listening Sockets for NGINX</strong></summary>

**Task**: Use `ss` to verify which network interfaces and ports the NGINX master process is actively bound to.

**Solution**:
```bash
sudo ss -tulpn | grep nginx
```
</details>

<details>
<summary><strong>Exercise 3 (Medium): Configure Official NGINX Repository Pinning</strong></summary>

**Task**: Author the exact APT preference file in `/etc/apt/preferences.d/99nginx` that forces APT to prioritize official NGINX packages over Ubuntu default packages.

**Solution**:
```bash
cat << 'EOF' | sudo tee /etc/apt/preferences.d/99nginx
Package: *
Pin: origin nginx.org
Pin: release o=nginx
Pin-Priority: 900
EOF

# Verify priority resolution
apt-cache policy nginx
```
</details>

<details>
<summary><strong>Exercise 4 (Medium): Run Hardened NGINX Container with Read-Only Root</strong></summary>

**Task**: Write a Docker command that launches `nginx:alpine` with a read-only root filesystem, mapping `/var/cache/nginx` and `/var/run` to temporary in-memory tmpfs mounts.

**Solution**:
```bash
docker run -d \
  --name secure-nginx \
  -p 80:80 \
  --read-only \
  --tmpfs /var/cache/nginx:rw,noexec,nosuid,size=50m \
  --tmpfs /var/run:rw,noexec,nosuid,size=10m \
  nginx:alpine
```
</details>

<details>
<summary><strong>Exercise 5 (Hard): Script an Automated Source Build with Custom Compilation Directory</strong></summary>

**Task**: Author a bash script that downloads NGINX 1.30.8, verifies source checksum, compiles with SSL and threads enabled, and outputs the binary to `/tmp/custom_nginx/sbin/nginx`.

**Solution**:
```bash
#!/usr/bin/env bash
set -euo pipefail

WORK_DIR=$(mktemp -d)
trap 'rm -rf "$WORK_DIR"' EXIT
cd "$WORK_DIR"

echo "Downloading NGINX source..."
curl -sLO https://nginx.org/download/nginx-1.30.8.tar.gz
tar -zxf nginx-1.30.8.tar.gz
cd nginx-1.30.8

echo "Configuring custom build..."
./configure \
  --prefix=/tmp/custom_nginx \
  --with-http_ssl_module \
  --with-threads \
  --with-http_v2_module

echo "Compiling..."
make -j$(nproc)
make install

echo "Testing compiled custom binary:"
/tmp/custom_nginx/sbin/nginx -v
```
</details>

<details>
<summary><strong>Exercise 6 (Hard): Audit Systemd Unit Hardening for NGINX</strong></summary>

**Task**: Use `systemd-analyze security` to audit the security posture of the NGINX systemd service and implement unit overrides to improve the security score.

**Solution**:
```bash
# Audit service security score (0 = secure, 10 = exposed)
systemd-analyze security nginx

# Create hardening drop-in override
sudo mkdir -p /etc/systemd/system/nginx.service.d/
cat << 'EOF' | sudo tee /etc/systemd/system/nginx.service.d/override.conf
[Service]
ProtectSystem=strict
ProtectHome=true
PrivateTmp=true
NoNewPrivileges=true
ProtectKernelTunables=true
ProtectControlGroups=true
RestrictNamespaces=true
EOF

sudo systemctl daemon-reload
sudo systemctl restart nginx
systemd-analyze security nginx
```
</details>

---

## 12. Summary & Quick Reference Cheat Sheet

| Task | Command | Description |
| :--- | :--- | :--- |
| **Check Version** | `nginx -v` | Prints version number |
| **Check Build Flags** | `nginx -V` | Detailed compiler and configure arguments |
| **Test Configuration** | `sudo nginx -t` | Tests config syntax and validates includes |
| **Dump Full Config** | `sudo nginx -T` | Dumps complete resolved config to stdout |
| **Reload Config** | `sudo nginx -s reload` | Gracefully reloads config with zero downtime |
| **Verify Port Binding**| `ss -tulpn \| grep nginx`| Displays open TCP/UDP sockets |
| **Check Service State**| `systemctl status nginx` | Systemd service status and recent journal logs |

---

## 13. Further Reading & Official Documentation

- [Official NGINX Installation Guide](https://nginx.org/en/docs/install.html)
- [Official NGINX PGP Signing Keys](https://nginx.org/en/pgp_keys.html)
- [NGINX Configure Options Reference](https://nginx.org/en/docs/configure.html)
- [Docker Official Image: NGINX](https://hub.docker.com/_/nginx)
- [CIS NGINX Benchmark Guidance](https://www.cisecurity.org/benchmark/nginx)

---

[Back to Track Overview](../README.md) | [Back to Module Overview](./README.md) | [02-config-structure.md ->](./02-config-structure.md)
