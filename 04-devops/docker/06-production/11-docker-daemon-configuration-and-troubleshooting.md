<!--
================================================================================
Metadata:
  Title: Docker Daemon Configuration and Troubleshooting
  Topic: Docker Production Engineering
  Difficulty: Advanced
  Estimated Time: 50 minutes
  Prerequisites: 00-setup/01-docker-engine-linux.md, 01-fundamentals/03-docker-architecture.md
  Key Terminology: dockerd, daemon.json, SIGHUP live-restore, data-root, systemd, journalctl, overlay2, docker.socket
================================================================================
-->

# Docker Daemon Configuration and Troubleshooting

[<- 10-resource-management-and-oom.md](./10-resource-management-and-oom.md) | [Back to Module Overview](./README.md) | [12-swarm-overview-and-status.md ->](./12-swarm-overview-and-status.md)

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

The Docker daemon (`dockerd`) is the persistent background process that orchestrates container lifecycles, manages image layer storage, builds software, controls networking bridges, and enforces security policies. In development environments, default daemon settings are typically sufficient. However, in production clusters:
- Storing images and container layers in the root partition (`/var/lib/docker`) risks filling the OS disk, crashing the host kernel.
- Restarting the Docker daemon to update a setting tears down all running production containers unless `live-restore` is configured.
- Default bridge network subnets (`172.17.0.0/16`) frequently collide with corporate internal VPNs and VPC CIDR blocks.
- Unrestricted log files without rotation exhaust filesystem inodes.

Mastering `/etc/docker/daemon.json` and systemd integration is fundamental for enterprise Linux systems engineers and site reliability engineers.

---

## 2. Core Concepts

### `/etc/docker/daemon.json`
The primary declarative configuration file for the Docker daemon on Linux. Any flag supported by the `dockerd` CLI binary can be declared inside `daemon.json`.
> **Rule of Thumb**: Never mix systemd CLI flags (`dockerd -H ...`) with conflicting keys in `daemon.json` (`"hosts": [...]`). If a parameter is defined in both places, `dockerd` will fail to start with a configuration collision error.

### Live-Restore
By default, when `dockerd` stops or restarts (e.g., during unattended security package upgrades), it sends SIGTERM/SIGKILL to all running containers. Enabling `"live-restore": true` instructs containerd to keep containers running and serving traffic while the daemon process restarts.

### SIGHUP Dynamic Reloading
Certain configuration settings (such as logging levels, max concurrent downloads, and registry mirrors) can be reloaded without restarting the daemon process by sending a `SIGHUP` signal (`kill -SIGHUP $(pidof dockerd)` or `systemctl reload docker`).

### `data-root` Migration
Specifies the path where Docker stores image layers, container root filesystems, volumes, and network databases (default `/var/lib/docker`). In production, this directory must reside on a dedicated high-performance, expandable volume (e.g., AWS EBS, RAID 10 NVMe).

### Systemd Integration and Socket Activation
On modern Linux distributions, Docker uses systemd socket activation via `docker.socket` listening on `/var/run/docker.sock`. When a client connects, systemd spawns `docker.service` if it is not already running.

---

## 3. How It Works Internally

```
+-----------------------------------------------------------------------------------+
|                               SYSTEMD SUBSYSTEM                                   |
|                                                                                   |
|  +--------------------+        activates        +------------------------------+  |
|  |   docker.socket    | ----------------------> |        docker.service        |  |
|  | /var/run/docker.sock|                         | ExecStart=/usr/bin/dockerd   |  |
|  +--------------------+                         +--------------+---------------+  |
+----------------------------------------------------------------|------------------+
                                                                 v
+-----------------------------------------------------------------------------------+
|                                DOCKER DAEMON (dockerd)                            |
|                                                                                   |
|  1. Parses /etc/docker/daemon.json                                                |
|  2. Initializes Storage Driver (overlay2) at data-root (/data/docker)            |
|  3. Initializes Network Controller (bridge, overlay, IPAM pools)                 |
|  4. Connects via gRPC to Containerd (/run/containerd/containerd.sock)             |
|                                                                                   |
|                   +--------------------------------------------+                  |
|                   |           CONTAINERD (Runtime)             |                  |
|                   |  Manages runc OCI containers and shims     |                  |
|                   +---------------------+----------------------+                  |
+-----------------------------------------|-----------------------------------------+
                                          |
                        +-----------------+-----------------+
                        | (When live-restore is enabled)    |
                        v                                   v
             +---------------------+             +---------------------+
             |  containerd-shim-runc|             |  containerd-shim-runc|
             |      PID: 10452     |             |      PID: 10498     |
             |   API Container     |             |   DB Container      |
             |  (Continues running |             |  (Continues running |
             |   during dockerd    |             |   during dockerd    |
             |      restart)       |             |      restart)       |
             +---------------------+             +---------------------+
```

---

## 4. Syntax & Usage

### Comprehensive Production `daemon.json` Reference

```json
{
  "data-root": "/data/docker",
  "storage-driver": "overlay2",
  "storage-opts": [
    "overlay2.override_kernel_check=true"
  ],
  "live-restore": true,
  "log-driver": "json-file",
  "log-opts": {
    "max-size": "50m",
    "max-file": "5",
    "compress": "true"
  },
  "default-ulimits": {
    "nofile": {
      "Name": "nofile",
      "Hard": 65536,
      "Soft": 65536
    },
    "nproc": {
      "Name": "nproc",
      "Hard": 4096,
      "Soft": 4096
    }
  },
  "default-address-pools": [
    {
      "base": "10.200.0.0/16",
      "size": 24
    }
  ],
  "bip": "10.200.254.1/24",
  "dns": [
    "1.1.1.1",
    "8.8.8.8"
  ],
  "dns-opts": [
    "timeout:2",
    "attempts:3"
  ],
  "max-concurrent-downloads": 10,
  "max-concurrent-uploads": 5,
  "metrics-addr": "127.0.0.1:9323",
  "experimental": true,
  "features": {
    "containerd-snapshotter": true
  },
  "userland-proxy": false,
  "icc": false,
  "no-new-privileges": true
}
```

---

## 5. Practical Examples

### Example 1: Migrating `data-root` to an External NVMe Mount

```bash
# 1. Stop Docker Engine and Sockets
sudo systemctl stop docker.service
sudo systemctl stop docker.socket

# 2. Verify new mount point is mounted and ready
df -h /data

# 3. Synchronize existing /var/lib/docker data preserving all permissions and xattrs
sudo rsync -aP /var/lib/docker/ /data/docker/

# 4. Backup old directory
sudo mv /var/lib/docker /var/lib/docker.old

# 5. Update /etc/docker/daemon.json
sudo tee /etc/docker/daemon.json <<'EOF'
{
  "data-root": "/data/docker",
  "live-restore": true,
  "log-driver": "json-file",
  "log-opts": {
    "max-size": "20m",
    "max-file": "3"
  }
}
EOF

# 6. Start Docker and verify root
sudo systemctl start docker
docker info --format 'Docker Root Dir: {{.DockerRootDir}}'
# Output should verify: Docker Root Dir: /data/docker

# 7. Once validated, remove backup
sudo rm -rf /var/lib/docker.old
```

### Example 2: Non-Disruptive Daemon Configuration Reload (SIGHUP)

```bash
# Verify current logging level
docker info --format 'Logging Level: {{.LoggingLevel}}'

# Update /etc/docker/daemon.json to enable debug logging
sudo jq '. + {"debug": true, "log-level": "debug"}' /etc/docker/daemon.json > /tmp/daemon.json \
  && sudo mv /tmp/daemon.json /etc/docker/daemon.json

# Send SIGHUP to dockerd without stopping containers
sudo systemctl reload docker

# Check journal logs for confirmation
sudo journalctl -u docker -n 10 --no-pager
# Output indicates: "Reloaded configuration: debug=true, log-level=debug"
```

### Example 3: Exposing Prometheus Metrics from Docker Daemon

```json
// /etc/docker/daemon.json
{
  "metrics-addr": "0.0.0.0:9323",
  "experimental": true
}
```

```bash
# Reload daemon
sudo systemctl restart docker

# Test metrics endpoint
curl -s http://localhost:9323/metrics | head -n 25

# Prometheus Scrape Config snippet:
# - job_name: 'dockerd'
#   static_configs:
#     - targets: ['127.0.0.1:9323']
```

### Example 4: Custom Systemd Override for Corporate Proxy or Custom Socket

```bash
# Create systemd drop-in directory
sudo mkdir -p /etc/systemd/system/docker.service.d

# Configure HTTP/HTTPS proxy for image pulling through corporate firewall
sudo tee /etc/systemd/system/docker.service.d/http-proxy.conf <<'EOF'
[Service]
Environment="HTTP_PROXY=http://proxy.corp.internal:8080"
Environment="HTTPS_PROXY=http://proxy.corp.internal:8080"
Environment="NO_PROXY=localhost,127.0.0.1,10.0.0.0/8,registry.corp.internal"
EOF

# Reload systemd and restart service
sudo systemctl daemon-reload
sudo systemctl restart docker

# Verify environment applied
sudo systemctl show --property=Environment docker
```

### Example 5: Resolving Bridge Network Collisions with `default-address-pools`

If corporate networks use `172.17.0.0/16`, Docker's default bridge will collide, dropping corporate VPN connectivity:

```json
// /etc/docker/daemon.json
{
  "bip": "192.168.100.1/24",
  "default-address-pools": [
    {
      "base": "192.168.101.0/24",
      "size": 26
    },
    {
      "base": "192.168.102.0/24",
      "size": 26
    }
  ]
}
```

---

## 6. Best Practices

1. **Always Enable `live-restore`**: In production, uninterrupted service during patch cycles is critical. Set `"live-restore": true`.
2. **Enforce Global Log Rotation**: Without global log limits in `daemon.json`, a chatty container will silently fill the disk partition with raw JSON text.
3. **Disable Userland Proxy (`"userland-proxy": false`)**: Docker's `docker-proxy` binary consumes host memory and degrades performance. Using kernel NAT (`iptables` / `nftables`) directly increases packet throughput and reduces latency.
4. **Disable Inter-Container Communication on Default Bridge (`"icc": false`)**: Improves security by preventing containers on the default bridge network from communicating unless explicitly linked.
5. **Set High Ulimits Globally**: Modern containerized databases and web gateways easily exhaust the default file descriptor limit of 1024. Set `nofile: 65536`.

---

## 7. Common Mistakes & Troubleshooting

| Symptom / Error | Root Cause | Diagnostic Steps | Resolution |
|:----------------|:-----------|:-----------------|:-----------|
| **`failed to start daemon: Error initializing network controller: list bridge addresses failed`** | `bip` or default bridge conflicts with existing host route | Run `ip route show` and inspect existing interface IPs | Change `"bip"` in `daemon.json` to an unallocated CIDR. |
| **`unable to configure the Docker daemon with file /etc/docker/daemon.json: the following directives are specified both as a flag and in the configuration file`** | Duplicate key between systemd `ExecStart` and `daemon.json` | `cat /lib/systemd/system/docker.service \| grep ExecStart` | Remove duplicate CLI flag from systemd or key from `daemon.json`. |
| **`dockerd: failed to start daemon: error while opening volume store metadata database: file is locked`** | Orphaned `dockerd` process running or unclean shutdown | Check `ps aux \| grep dockerd` | Terminate orphaned process (`kill -9`) or remove lockfile in `data-root`. |
| **`permission denied while trying to connect to the Docker daemon socket`** | User is not in `docker` group or socket permissions wrong | Run `id` and check `ls -la /var/run/docker.sock` | Add user: `sudo usermod -aG docker $USER` and log in again. |
| **Containers killed during `systemctl restart docker` despite live-restore** | `live-restore` does not work with Docker Swarm mode | Check `docker info --format '{{.Swarm.LocalNodeState}}'` | Swarm routing mesh requires active daemon supervision; use Rolling Updates. |
| **`no space left on device` but `df -h` shows disk space available** | Inode exhaustion in `data-root` due to millions of small layers | Run `df -i` on filesystem | Prune unused images and containers (`docker system prune -a --volumes`). |

---

## 8. Performance, Security & Cost Implications

### Performance
Setting `"userland-proxy": false` replaces userspace proxy routing with native kernel Netfilter/iptables packet forwarding, cutting connection latency by 30–50% under high concurrency.

### Security
Configuring `"no-new-privileges": true` in `daemon.json` ensures that no container process can gain additional privileges via setuid/setgid binaries, dramatically mitigating container breakout exploits.

### Cost
Configuring `"data-root"` on dynamically resizable cloud storage (e.g., EBS GP3) prevents over-provisioning large root operating system disks across thousands of worker nodes.

---

## 9. Comparison Table

| Mechanism | Systemd CLI Flags | `/etc/docker/daemon.json` |
|:----------|:------------------|:--------------------------|
| **Primary Location** | `/lib/systemd/system/docker.service` | `/etc/docker/daemon.json` |
| **Dynamic Reload (SIGHUP)** | No (Requires full daemon restart) | Yes (for supported parameters) |
| **Configuration Style** | Procedural shell arguments | Declarative structured JSON |
| **Version Control Suitability**| Poor (Managed by OS package manager)| Excellent (GitOps / Ansible friendly) |
| **Precedence** | Collides if duplicated; flags take priority | Fails to start if duplicate flag present |

---

## 10. Real-World Interview Questions

<details>
<summary><strong>Q1: What happens under the hood when you configure "live-restore": true in daemon.json?</strong></summary>

When `live-restore` is enabled, `dockerd` decouples its process lifecycle from `containerd` and `containerd-shim-runc`. When `dockerd` shuts down, it disconnects from containerd's gRPC socket but leaves containerd and the container shims intact. The running container processes retain their open file descriptors, network namespaces, and cgroups without disruption. When `dockerd` starts back up, it reconnects to the containerd socket, re-attaches to the existing container IDs, and synchronizes state seamlessly.
</details>

<details>
<summary><strong>Q2: Why does dockerd fail to start when both -H fd:// and "hosts": ["fd://"] are defined?</strong></summary>

Docker's configuration parser explicitly disallows duplicate or overlapping parameters defined in both CLI flags and `daemon.json` to prevent configuration ambiguity. If the systemd unit file defines `ExecStart=/usr/bin/dockerd -H fd://` and `daemon.json` defines `"hosts": ["fd://"]`, the startup validation logic triggers a fatal error: `the following directives are specified both as a flag and in the configuration file: hosts`. To fix it, either remove the key from `daemon.json` or strip the `-H` flag from systemd using an override file.
</details>

<details>
<summary><strong>Q3: How do you safely migrate Docker's data directory (/var/lib/docker) without losing images or data?</strong></summary>

1. Stop both `docker.service` and `docker.socket` (`sudo systemctl stop docker.socket docker.service`).
2. Verify no processes are writing to `/var/lib/docker`.
3. Use `rsync -aP` (archive mode preserving permissions, symlinks, timestamps, and extended attributes) to copy `/var/lib/docker/` to the target mount.
4. Rename `/var/lib/docker` to `/var/lib/docker.old`.
5. Update `daemon.json` with `"data-root": "/new/path"`.
6. Start `docker.service`. Verify running images with `docker images` and inspect root directory with `docker info`.
7. Once verified, delete `/var/lib/docker.old`.
</details>

<details>
<summary><strong>Q4: Explain how Docker manages Linux iptables rules and what happens if iptables is set to false in daemon.json.</strong></summary>

By default (`"iptables": true`), `dockerd` injects custom chains (`DOCKER`, `DOCKER-USER`, `DOCKER-ISOLATION-STAGE-1`) into the `nat` and `filter` tables to handle port publishing and network isolation. If you set `"iptables": false`, Docker completely disables automatic firewall rule management. As a result, `--publish` (`-p 80:80`) will no longer route traffic from the host interface to the container automatically; the system administrator becomes entirely responsible for manually defining all prerouting and forwarding iptables rules.
</details>

<details>
<summary><strong>Q5: What is the purpose of the DOCKER-USER iptables chain?</strong></summary>

When Docker starts, it clears and rebuilds its internal `DOCKER` chain. Any manual iptables rules added to `INPUT` or `DOCKER` are overwritten whenever Docker restarts. To allow administrators to insert custom firewall rules (such as IP whitelisting or blocking malicious subnets) without having them wiped by Docker, Docker directs incoming traffic through the `DOCKER-USER` chain before any internal routing rules are evaluated.
</details>

<details>
<summary><strong>Q6: How do you troubleshoot a Docker daemon that fails to start with no helpful output in the terminal?</strong></summary>

1. Check systemd unit status: `systemctl status docker.service`.
2. Inspect detailed systemd logs: `journalctl -u docker.service -e --no-pager`.
3. If logs are still vague, stop the service and run `dockerd` manually in foreground debug mode: `sudo dockerd --debug`.
4. Validate that `/etc/docker/daemon.json` contains valid JSON syntax using `jq . /etc/docker/daemon.json`.
5. Check disk space and inodes (`df -h`, `df -i`).
</details>

<details>
<summary><strong>Q7: Can you reload daemon.json configuration changes without stopping running containers? How?</strong></summary>

Yes, for supported parameters (such as `log-level`, `debug`, `labels`, `registry-mirrors`, and `insecure-registries`), you can send a `SIGHUP` signal to `dockerd` using `sudo systemctl reload docker` or `sudo kill -SIGHUP $(pidof dockerd)`. Docker will re-read the configuration file and apply supported changes dynamically without restarting containers. Unsupported changes (like `storage-driver` or `data-root`) require a full daemon restart.
</details>

<details>
<summary><strong>Q8: What is the difference between docker.socket and docker.service in systemd?</strong></summary>

`docker.socket` manages the listening UNIX domain socket at `/var/run/docker.sock`. Under systemd socket activation, the socket is bound on host boot even if `dockerd` has not yet started. When any process (like the `docker` CLI) sends a packet to the socket, systemd automatically starts `docker.service`. This improves boot times and allows socket access to queue requests while the daemon is initializing.
</details>

---

## 11. Practice Exercises

<details>
<summary><strong>Exercise 1: Validate daemon.json syntax prior to reload</strong></summary>

**Task**: Write a shell command to safely validate the JSON syntax of `/etc/docker/daemon.json` before applying it.

**Solution**:
```bash
if jq empty /etc/docker/daemon.json 2>/dev/null; then
  echo "Valid JSON configuration."
else
  echo "ERROR: Invalid JSON syntax in /etc/docker/daemon.json" >&2
  exit 1
fi
```
</details>

<details>
<summary><strong>Exercise 2: Configure and verify log rotation globally</strong></summary>

**Task**: Configure `daemon.json` to enforce 10MB max log size with 3 rotated files, reload the daemon, and inspect the effective logging options of a newly launched container.

**Solution**:
```bash
# 1. Update /etc/docker/daemon.json
sudo jq '. + {"log-driver": "json-file", "log-opts": {"max-size": "10m", "max-file": "3"}}' /etc/docker/daemon.json | sudo tee /etc/docker/daemon.json.new
sudo mv /etc/docker/daemon.json.new /etc/docker/daemon.json

# 2. Reload daemon
sudo systemctl reload docker

# 3. Start test container
docker run -d --name log-check alpine sleep 60

# 4. Verify effective HostConfig LogConfig
docker inspect log-check --format 'Driver: {{.HostConfig.LogConfig.Type}} | Opts: {{json .HostConfig.LogConfig.Config}}'
docker rm -f log-check
```
</details>

<details>
<summary><strong>Exercise 3: Extract dockerd diagnostic logs for the last 15 minutes</strong></summary>

**Task**: Write a journalctl command to extract all Docker daemon error logs recorded in the past 15 minutes.

**Solution**:
```bash
sudo journalctl -u docker.service --since "15 minutes ago" -p err..emerg --no-pager
```
</details>

<details>
<summary><strong>Exercise 4: Systemd override for private insecure registry</strong></summary>

**Task**: Add `10.50.0.10:5000` as an insecure registry in `daemon.json` and reload the daemon without restarting containers.

**Solution**:
```bash
sudo jq '. + {"insecure-registries": ["10.50.0.10:5000"]}' /etc/docker/daemon.json | sudo tee /etc/docker/daemon.json.tmp
sudo mv /etc/docker/daemon.json.tmp /etc/docker/daemon.json
sudo systemctl reload docker
docker info --format 'Insecure Registries: {{.RegistryConfig.IndexConfigs}}'
```
</details>

<details>
<summary><strong>Exercise 5: Inspect systemd unit drop-in overrides</strong></summary>

**Task**: Verify all loaded systemd configuration drop-in files for `docker.service`.

**Solution**:
```bash
systemctl cat docker.service
```
</details>

<details>
<summary><strong>Exercise 6: Test foreground debug execution</strong></summary>

**Task**: Safely stop systemd Docker service and launch `dockerd` directly in terminal foreground to capture real-time debug output.

**Solution**:
```bash
# Stop systemd service
sudo systemctl stop docker.service docker.socket

# Run manually in foreground
sudo dockerd --debug

# Press Ctrl+C when complete, then restore systemd service
sudo systemctl start docker.socket docker.service
```
</details>

---

## 12. Summary & Quick Reference Cheat Sheet

### Common Daemon Commands

```bash
# Service Control
sudo systemctl start docker          # Start daemon and socket
sudo systemctl stop docker           # Stop daemon (terminates containers if no live-restore)
sudo systemctl restart docker        # Full process restart
sudo systemctl reload docker         # SIGHUP dynamic config reload without container restart
sudo systemctl status docker         # Check daemon runtime status

# Logs & Diagnostics
sudo journalctl -u docker -f         # Follow live daemon logs
sudo journalctl -u docker -e         # Jump to end of journal
sudo dockerd --debug                 # Run manually in foreground for deep diagnostics
docker info                          # Query active daemon configuration state
```

### Essential `daemon.json` Production Settings
- `"live-restore": true`: Prevents container teardown on daemon restart.
- `"data-root": "/path"`: Moves Docker storage off the OS root partition.
- `"log-driver": "json-file"` + `"max-size": "50m"`: Prevents log files from exhausting host disk.
- `"userland-proxy": false`: Bypasses userspace proxy in favor of native kernel Netfilter.
- `"no-new-privileges": true`: Disallows privilege escalation inside containers.

---

## 13. Further Reading & Official Documentation

- [Docker Documentation: Daemon Configuration File (daemon.json)](https://docs.docker.com/engine/reference/commandline/dockerd/#daemon-configuration-file)
- [Docker Documentation: Keep containers alive during daemon downtime (Live Restore)](https://docs.docker.com/engine/admin/live-restore/)
- [Docker Documentation: Collect Docker metrics with Prometheus](https://docs.docker.com/engine/admin/prometheus/)
- [Systemd Socket Activation Documentation](https://www.freedesktop.org/software/systemd/man/systemd.socket.html)
- [Docker Networking: Packet filtering and firewalls](https://docs.docker.com/network/packet-filtering-firewalls/)

---

[<- 10-resource-management-and-oom.md](./10-resource-management-and-oom.md) | [Back to Module Overview](./README.md) | [12-swarm-overview-and-status.md ->](./12-swarm-overview-and-status.md)
