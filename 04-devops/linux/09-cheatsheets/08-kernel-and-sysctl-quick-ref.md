# Linux Kernel Tuning & Sysctl: High-Density Quick Reference

| Metadata | Details |
| :--- | :--- |
| **Topic** | Production Kernel Parameters, Memory Management & Network Stack Tuning |
| **Module** | `09-cheatsheets` — High-Density Linux Production Cheatsheets |
| **Track** | `04-devops/linux` |
| **Target OS** | Ubuntu 24.04 LTS (Kernel 6.8+), RHEL 9.4+ (Kernel 5.14+) |
| **Last Updated**| 2026 |

---

## 1. Applying & Managing Sysctl Parameters

```bash
# === Query Parameters ===
sysctl -a                                   # Dump ALL active kernel parameters
sysctl net.ipv4.tcp_congestion_control      # Query single parameter
sysctl -n vm.swappiness                     # Print ONLY value (clean for scripts)

# === Live Ephemeral Tuning ===
sudo sysctl -w net.core.somaxconn=65535     # Apply immediately (lost on reboot)

# === Persistent Configuration ===
# Place drop-in files inside: /etc/sysctl.d/99-production.conf
sudo sysctl --system                        # Reload and apply ALL configuration files
```

---

## 2. Virtual Memory & Storage Parameters

| Parameter | Default | Production Value | Architectural Purpose |
| :--- | :--- | :--- | :--- |
| `vm.swappiness` | `60` | `10` (Web) / `1` (DB) | Bias towards reclaiming page cache before paging anonymous memory to swap. |
| `vm.dirty_ratio` | `20` | `10` | Percentage of RAM where process blocks and synchronously flushes dirty pages. |
| `vm.dirty_background_ratio` | `10` | `5` | Percentage of RAM where `kswapd` begins asynchronous background flushing. |
| `vm.vfs_cache_pressure` | `100` | `50` | Lower value preserves dentry and inode caches in RAM, speeding up file traversal. |
| `vm.max_map_count` | `65530`| `262144` | Maximum memory-mapped areas per process (MANDATORY for Elasticsearch, Kafka). |
| `vm.overcommit_memory` | `0` | `0` (or `1` for Redis) | `0`: Heuristic check; `1`: Always overcommit; `2`: Strict allocation ceiling. |

---

## 3. Network Connection Queues & Limits

| Parameter | Default | Production Value | Architectural Purpose |
| :--- | :--- | :--- | :--- |
| `net.core.somaxconn` | `4096` | `65535` | Maximum depth of the Accept Listen Queue. Prevents connection timeouts. |
| `net.ipv4.tcp_max_syn_backlog`| `2048` | `65535` | Maximum half-open connections in SYN queue during 3-way handshake. |
| `net.core.netdev_max_backlog` | `1000` | `100000` | Number of incoming packets queued on interface before IP stack processing. |
| `net.ipv4.ip_local_port_range`| `32768 60999` | `1024 65535` | Ephemeral port range for outbound connections (expands pool to ~64,000 ports).|
| `net.ipv4.tcp_max_tw_buckets` | `262144` | `2000000` | Maximum TIME_WAIT sockets allowed before kernel purges them. |

---

## 4. High-Speed TCP Buffer Windows (10GbE / 100GbE)

```ini
# Enlarge maximum socket buffer ceilings (64MB)
net.core.rmem_max = 67108864
net.core.wmem_max = 67108864
net.core.rmem_default = 1048576
net.core.wmem_default = 1048576

# TCP Autotuning: min default max (in bytes)
net.ipv4.tcp_rmem = 4096 87380 67108864
net.ipv4.tcp_wmem = 4096 65536 67108864

# Enable TCP Features
net.ipv4.tcp_window_scaling = 1
net.ipv4.tcp_timestamps = 1
net.ipv4.tcp_sack = 1
```

---

## 5. Modern Congestion Control: Google BBR

```ini
# Load TCP BBR module first: sudo modprobe tcp_bbr

# Set queuing discipline to Fair Queueing (required for BBR pacing)
net.core.default_qdisc = fq

# Enable BBR congestion control
net.ipv4.tcp_congestion_control = bbr

# Keep TCP window open after idle periods (Critical for HTTP/2 & gRPC)
net.ipv4.tcp_slow_start_after_idle = 0
```

---

## 6. Connection Lifecycle & TIME_WAIT Management

```ini
# Enable safe reuse of TIME_WAIT sockets for outbound connections
net.ipv4.tcp_tw_reuse = 1

# Reduce FIN connection timeout (default 60s -> 15s)
net.ipv4.tcp_fin_timeout = 15

# SYN flood protection
net.ipv4.tcp_syncookies = 1
net.ipv4.tcp_synack_retries = 2
```

---

## 7. Kernel Hardening & Security Parameters

```ini
# ASLR: Full Address Space Layout Randomization
kernel.randomize_va_space = 2

# Hide kernel memory pointers from non-root users
kernel.kptr_restrict = 2

# Restrict dmesg kernel log buffer access to root only
kernel.dmesg_restrict = 1

# Restrict ptrace process tracing to direct children only (YAMA)
kernel.yama.ptrace_scope = 1

# Protect symlinks and hardlinks in world-writable directories
fs.protected_symlinks = 1
fs.protected_hardlinks = 1
fs.protected_fifos = 2
fs.protected_regular = 2

# Network Spoofing & Redirection Defenses
net.ipv4.conf.all.rp_filter = 1
net.ipv4.conf.default.rp_filter = 1
net.ipv4.conf.all.accept_redirects = 0
net.ipv4.conf.default.accept_redirects = 0
net.ipv4.conf.all.send_redirects = 0
net.ipv4.conf.default.send_redirects = 0
```

---

## 8. Complete Drop-in Baseline (`/etc/sysctl.d/99-production-baseline.conf`)

```ini
# ====================================================================
# Enterprise Production Sysctl Configuration (Ubuntu 24.04 / RHEL 9.4)
# ====================================================================

# 1. System File Limits
fs.file-max = 2097152
fs.inotify.max_user_watches = 524288

# 2. Virtual Memory
vm.swappiness = 10
vm.dirty_ratio = 10
vm.dirty_background_ratio = 5
vm.vfs_cache_pressure = 50
vm.max_map_count = 262144

# 3. Connection Queues
net.core.somaxconn = 65535
net.ipv4.tcp_max_syn_backlog = 65535
net.core.netdev_max_backlog = 100000
net.ipv4.ip_local_port_range = 1024 65535

# 4. TCP BBR & Buffer Windows
net.core.default_qdisc = fq
net.ipv4.tcp_congestion_control = bbr
net.ipv4.tcp_slow_start_after_idle = 0
net.core.rmem_max = 67108864
net.core.wmem_max = 67108864
net.ipv4.tcp_rmem = 4096 87380 67108864
net.ipv4.tcp_wmem = 4096 65536 67108864

# 5. Socket Reuse
net.ipv4.tcp_tw_reuse = 1
net.ipv4.tcp_fin_timeout = 15
net.ipv4.tcp_syncookies = 1

# 6. Kernel Security
kernel.randomize_va_space = 2
kernel.kptr_restrict = 2
kernel.dmesg_restrict = 1
fs.protected_symlinks = 1
fs.protected_hardlinks = 1
```

---
*Next Cheatsheet: [09-package-managers-compared.md](file:///04-devops/linux/09-cheatsheets/09-package-managers-compared.md)*
