# Linux Technical Facts & Standards Baseline

**Date:** 2026-11-25  
**Scope:** Linux Systems Engineering Curriculum Baseline (`04-devops/linux/`)  
**Standard Distributions:** Ubuntu 24.04 LTS (Noble Numbat), Ubuntu 22.04 LTS (Jammy Jellyfish), RHEL 9 / Rocky Linux 9 / AlmaLinux 9, Fedora 40/41  

---

## 1. Operating System & Distribution Baselines

### Ubuntu LTS Series
- **Ubuntu 24.04 LTS (Noble Numbat)**:
  - Release Date: April 25, 2024. Standard Support: April 2029 (5 years); Expanded Security Maintenance (ESM) to 2034.
  - Linux Kernel: 6.8 (HWE updates to 6.11+).
  - Init & System Manager: `systemd 255.4`.
  - Cgroup Hierarchy: Unified **cgroup v2** enabled by default.
  - Network Configuration: Netplan v1.0 with `systemd-networkd` (server) and `NetworkManager` (desktop).
  - Default Firewall: `ufw` frontend powered by `nftables` backend.
  - Security Module: **AppArmor 4.0** enabled in enforcing mode.
  - C Standard Library: `glibc 2.39`.
  - Source: [Ubuntu 24.04 LTS Release Notes](https://discourse.ubuntu.com/t/noble-numbat-release-notes/39890)

- **Ubuntu 22.04 LTS (Jammy Jellyfish)**:
  - Release Date: April 21, 2022. Standard Support: April 2027; ESM to 2032.
  - Linux Kernel: 5.15 LTS (HWE 6.5).
  - Init System: `systemd 249.11`.
  - Cgroup: cgroup v2 default.
  - Source: [Ubuntu 22.04 LTS Release Notes](https://discourse.ubuntu.com/t/jammy-jellyfish-release-notes/24606)

### RHEL Family (Enterprise Linux 9 / Rocky Linux / AlmaLinux)
- **RHEL 9.x / Rocky Linux 9 / AlmaLinux 9**:
  - Kernel: 5.14.0 (Red Hat backported kernel tree).
  - Init System: `systemd 252`.
  - Package Management: `dnf v4` (libdnf); RPM v4.16+. (Fedora 41 defaults to `dnf5`).
  - Network Configuration: `NetworkManager` (via `nmcli`, `nmtui`, `/etc/NetworkManager/system-connections/`). Legacy `network-scripts` completely removed.
  - Default Firewall: `firewalld` using native `nftables` backend.
  - Security Module: **SELinux** in `Enforcing` mode with targeted policy.
  - Source: [Red Hat Enterprise Linux 9 Release Notes](https://docs.redhat.com/en/documentation/red_hat_enterprise_linux/9/)

---

## 2. Kernel & Low-Level Primitives

### Unified Cgroups v2
- cgroup v2 is the standard hierarchy located at `/sys/fs/cgroup`.
- Replaces legacy multi-hierarchy cgroup v1. Eliminates resource attribution deadlocks between memory and block I/O controllers.
- Required by modern container engines (Docker 20.10+, containerd 1.6+, Kubernetes 1.25+, systemd 247+).
- Verified via: `stat -fc %T /sys/fs/cgroup` (returns `cgroup2fs`).

### Linux Kernel Namespaces
The 8 namespaces providing isolation for Linux containers:
1. `pid`: Process IDs and process tree isolation.
2. `net`: Network devices, IP routing tables, port bindings, firewall rules.
3. `mnt`: Filesystem mount points (`pivot_root`, `chroot`).
4. `ipc`: System V IPC, POSIX message queues.
5. `uts`: Hostname and NIS domain name.
6. `user`: UID/GID mappings (rootless containers).
7. `cgroup`: Isolated root cgroup directory.
8. `time`: Monotonic and boot clock offsets (Linux 5.6+).

---

## 3. Deprecated Tools vs Modern Standards

| Deprecated / Obsolete Tool | Modern Replacement | Architectural Justification |
|:---|:---|:---|
| `ifconfig` (net-tools) | `ip address`, `ip link` (`iproute2`) | `ifconfig` cannot handle multi-IP addresses per interface, network namespaces, or modern netlink kernel sockets. |
| `netstat` (net-tools) | `ss` (iproute2) | `netstat` reads slow `/proc/net/` files; `ss` queries kernel Netlink socket structures directly in $O(1)$ speed. |
| `arp` | `ip neighbor` | Integrated directly into `iproute2` suite. |
| `route` | `ip route` | Full support for CIDR, multiple routing tables, metric policies, and namespaces. |
| `iptables-legacy` (xtables) | `nftables` (`nft`), `iptables-nft` | `nftables` uses an in-kernel bytecode VM, reducing ruleset evaluation from $O(N)$ linear scans to $O(1)$ set/map lookups. |
| `service <name> start` | `systemctl start <name>.service` | `service` is a legacy wrapper script; `systemctl` interacts natively with systemd D-Bus API. |
| `/etc/init.d/` scripts | `/etc/systemd/system/*.service` | Systemd declarative unit files enforce process supervision, cgroup isolation, and dependency ordering. |
| System V cron only | `systemd.timer` units | Systemd timers support calendar scheduling, monotonic intervals (`OnBootSec`, `OnUnitActiveSec`), missed-run catchup (`Persistent=true`), and unified logging in `journalctl`. |
| `nslookup` | `dig`, `resolvectl query` | `nslookup` has inconsistent cross-platform resolvers; `dig` and `resolvectl` query modern DNS caches and DNSSEC. |
| `scp` (legacy protocol) | `rsync -avz -e ssh`, `sftp` | Legacy SCP protocol contains severe CVEs and does not support resumption or atomic sync. OpenSSH 9.0+ defaults SCP to SFTP protocol. |

---

## 4. Security & Hardening Baselines

### CIS Benchmark Alignment
- **Root SSH**: `PermitRootLogin no` mandatory in `/etc/ssh/sshd_config`.
- **Authentication**: `PasswordAuthentication no`; mandate asymmetric keys (`ed25519`).
- **File Permissions**:
  - `/etc/shadow`: `0640` or `0600` (root:shadow).
  - `/etc/passwd`: `0644` (root:root).
  - `/etc/sudoers`: `0440` (root:root), validated via `visudo -c`.
- **Dangerous Commands Flagged in Audit**:
  - `chmod 777`: Destroys security boundaries; replace with precise octal masks (`755`, `644`) or POSIX ACLs (`setfacl`).
  - `curl | bash`: Executes unverified remote code; mandate SHA-256 verification and inspection.
  - `kill -9` (SIGKILL): Prevents graceful process shutdown, database transaction commits, and lockfile cleanup; always send `SIGTERM` (15) first.
  - `su -`: Discouraged over audited `sudo -i` with logged command trails.

---

## 5. Modern Packaging & Systems Tooling

- **APT (Debian/Ubuntu)**: Uses `apt` CLI for human interactive use; `apt-get` for deterministic scripts. Configuration in `/etc/apt/sources.list.d/*.sources` (modern deb822 format in Ubuntu 24.04).
- **DNF / RPM (RHEL/Rocky/Fedora)**: `dnf` v4 standard in RHEL 9; `dnf5` in Fedora 41. Modular streams, transaction histories (`dnf history undo`).
- **Modern Shell Ergonomics**:
  - `bash 5.2+`
  - Strict mode: `set -euo pipefail` with `IFS=$'\n\t'`
  - Script linters: `shellcheck`
  - Structured data parsing: `jq` (JSON), `yq` (YAML)
  - Process inspection: `htop`, `btop`, `glances` alongside canonical `top`, `vmstat`, `iostat`, `sar`.
