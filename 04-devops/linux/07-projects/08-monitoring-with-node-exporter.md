# Project 08: Infrastructure Monitoring with Prometheus Node Exporter

| Metadata | Details |
| :--- | :--- |
| **Project** | Production Node Exporter Deployment, Textfile Collector & PromQL Alerting |
| **Module** | `07-projects` — Production Reference Projects & Labs |
| **Track** | `04-devops/linux` |
| **Target OS** | Ubuntu 24.04 LTS, RHEL 9.4 / Rocky Linux 9.4 |
| **Complexity** | Intermediate |
| **Estimated Duration**| 45–60 minutes |

---

## 1. Architecture & Design

### Scenario Context
Operating a fleet of production Linux servers without real-time, low-overhead hardware and kernel metrics is flying blind. Relying on periodic SSH scripts or reactive email alerts results in delayed incident response during CPU saturation, memory exhaustion, or network packet drops.

**Prometheus Node Exporter** is the industry-standard agent for exporting machine-level metrics (CPU modes, virtual memory, disk I/O latency, network throughput, filesystem usage, and systemd service states). This project constructs a production deployment of Node Exporter featuring:
1. **Hardened systemd Service**: Executed under a dedicated unprivileged system account (`node_exporter`) with strict kernel filesystem sandboxing.
2. **Custom Textfile Collector Pipeline**: A lightweight shell automation script that publishes custom Linux metrics (pending security CVE patches, GFS backup ages, zombie process counts) in Prometheus exposition format.
3. **Enterprise Alerting Rules**: Production-tested PromQL alert expressions detecting impending outages.

### Observability Data Flow Architecture
```
+-------------------------------------------------------------------------+
|                  Prometheus Linux Node Telemetry Path                   |
|                                                                         |
|  [ Linux Kernel & Hardware Subsystems ]                                 |
|  (/proc/stat, /proc/meminfo, /proc/diskstats, /sys/class/net)           |
|                                  |                                      |
|                                  v                                      |
|  +-------------------------------------------------------------------+  |
|  | PROMETHEUS NODE EXPORTER (Port 9100)                              |  |
|  | - User: node_exporter (Unprivileged System Account)               |  |
|  | - Sandboxing: ProtectSystem=strict, PrivateTmp=true               |  |
|  | - Collectors: cpu, meminfo, diskstats, filesystem, netdev, systemd |  |
|  |                                                                   |  |
|  | [ Textfile Collector Directory: /var/lib/node_exporter/textfile ]  |
|  |   ^                                                               |  |
|  |   | Periodic Cron Script: custom-metrics.sh                       |  |
|  |   +-- node_security_updates_pending                               |  |
|  |   +-- node_backup_last_success_timestamp                          |  |
|  |   +-- node_processes_zombie_count                                 |  |
|  +-------------------------------------------------------------------+  |
|                                  | HTTP GET /metrics (Scraped every 15s)|
|                                  v                                      |
|  +-------------------------------------------------------------------+  |
|  | PROMETHEUS SERVER / MONITORING CLUSTER                            |  |
|  | - PromQL Evaluation Engine & Grafana Dashboards                  |  |
|  | - Alertmanager: PagerDuty / Slack Notifications                   |  |
|  +-------------------------------------------------------------------+  |
+-------------------------------------------------------------------------+
```

---

## 2. Implementation Artifacts

### Artifact 1: Node Exporter Automated Installer & Hardened Unit (`install-node-exporter.sh`)

```bash
#!/usr/bin/env bash
# ==============================================================================
# Script: install-node-exporter.sh
# Purpose: Deploy and Harden Prometheus Node Exporter with Textfile Collector
# Supported: Ubuntu 24.04 LTS / RHEL 9.4
# ==============================================================================

set -euo pipefail

NODE_EXPORTER_VERSION="1.8.1"
ARCH="linux-amd64"
INSTALL_DIR="/usr/local/bin"
TEXTFILE_DIR="/var/lib/node_exporter/textfile_collector"

log() { echo -e "\033[1;32m[+] $*\033[0m"; }
error() { echo -e "\033[1;31m[-] $*\033[0m" >&2; exit 1; }

[[ "${EUID}" -eq 0 ]] || error "Must run as root."

log "Step 1: Creating unprivileged node_exporter system user..."
if ! id "node_exporter" >/dev/null 2>&1; then
    useradd -r -s /usr/sbin/nologin -d /var/lib/node_exporter node_exporter
fi

mkdir -p "${TEXTFILE_DIR}"
chown -R node_exporter:node_exporter /var/lib/node_exporter
chmod 755 "${TEXTFILE_DIR}"

log "Step 2: Downloading official Node Exporter binary (v${NODE_EXPORTER_VERSION})..."
TARBALL="node_exporter-${NODE_EXPORTER_VERSION}.${ARCH}.tar.gz"
URL="https://github.com/prometheus/node_exporter/releases/download/v${NODE_EXPORTER_VERSION}/${TARBALL}"

cd /tmp
curl -fsSL -O "${URL}"
tar -xzf "${TARBALL}"
cp "node_exporter-${NODE_EXPORTER_VERSION}.${ARCH}/node_exporter" "${INSTALL_DIR}/"
chown root:root "${INSTALL_DIR}/node_exporter"
chmod 755 "${INSTALL_DIR}/node_exporter"
rm -rf "node_exporter-${NODE_EXPORTER_VERSION}.${ARCH}" "${TARBALL}"

log "Step 3: Creating hardened systemd service unit..."
cat << EOF > /etc/systemd/system/node_exporter.service
[Unit]
Description=Prometheus Node Exporter
Documentation=https://github.com/prometheus/node_exporter
After=network.target

[Service]
User=node_exporter
Group=node_exporter
Type=simple
ExecStart=${INSTALL_DIR}/node_exporter \\
  --collector.systemd \\
  --collector.processes \\
  --collector.textfile.directory=${TEXTFILE_DIR} \\
  --web.listen-address=0.0.0.0:9100

Restart=always
RestartSec=5s

# --- Security Sandboxing ---
ProtectSystem=strict
ProtectHome=true
PrivateTmp=true
NoNewPrivileges=true
ProtectKernelTunables=true
ProtectControlGroups=true
MemoryDenyWriteExecute=true
CapabilityBoundingSet=
ReadOnlyPaths=/
ReadWritePaths=${TEXTFILE_DIR}

[Install]
WantedBy=multi-user.target
EOF

log "Step 4: Reloading systemd and enabling node_exporter..."
systemctl daemon-reload
systemctl enable --now node_exporter

log "Node Exporter successfully installed and running on port 9100!"
```

### Artifact 2: Custom Metrics Textfile Generator (`custom-metrics.sh`)

A cron-driven script that checks pending security updates, backup status, and zombie processes, writing metrics atomically via a temporary file.

```bash
#!/usr/bin/env bash
# ==============================================================================
# Script: custom-metrics.sh
# Purpose: Generate custom Linux metrics for Prometheus Node Exporter
# Location: /usr/local/sbin/custom-metrics.sh
# ==============================================================================

set -euo pipefail

METRICS_DIR="/var/lib/node_exporter/textfile_collector"
TEMP_FILE="${METRICS_DIR}/custom_metrics.prom.$$"
FINAL_FILE="${METRICS_DIR}/custom_metrics.prom"

# 1. Count Zombie Processes
ZOMBIE_COUNT=$(ps -eo stat | grep -c "^Z" || true)

# 2. Check Pending Security Updates (Debian/Ubuntu)
PENDING_UPDATES=0
if [[ -f /usr/lib/update-notifier/apt-check ]]; then
    PENDING_UPDATES=$(/usr/lib/update-notifier/apt-check 2>&1 | cut -d';' -f2 || echo 0)
fi

# 3. Check Last Backup Timestamp (Read from backup log or file)
LAST_BACKUP_SEC=0
LATEST_BACKUP=$(ls -t /mnt/backups/daily/*.tar.zst.gpg 2>/dev/null | head -n 1 || true)
if [[ -n "${LATEST_BACKUP}" && -f "${LATEST_BACKUP}" ]]; then
    LAST_BACKUP_SEC=$(stat -c %Y "${LATEST_BACKUP}")
fi

# 4. Write Metrics in Prometheus Format (Atomically)
cat << EOF > "${TEMP_FILE}"
# HELP node_processes_zombie_count Number of zombie processes on host
# TYPE node_processes_zombie_count gauge
node_processes_zombie_count ${ZOMBIE_COUNT}

# HELP node_security_updates_pending Number of unapplied security packages
# TYPE node_security_updates_pending gauge
node_security_updates_pending ${PENDING_UPDATES}

# HELP node_backup_last_success_timestamp Unix timestamp of latest successful daily backup
# TYPE node_backup_last_success_timestamp gauge
node_backup_last_success_timestamp ${LAST_BACKUP_SEC}
EOF

# Atomic swap prevents Prometheus from reading a partially-written file
mv "${TEMP_FILE}" "${FINAL_FILE}"
chmod 644 "${FINAL_FILE}"
```

### Artifact 3: Production Prometheus Alerting Rules (`linux-alerts.yml`)

```yaml
groups:
  - name: LinuxInfrastructureAlerts
    rules:
      # Alert: High CPU Utilization (> 90% for 5 minutes)
      - alert: HostHighCpuLoad
        expr: 100 - (avg by (instance) (rate(node_cpu_seconds_total{mode="idle"}[5m])) * 100) > 90
        for: 5m
        labels:
          severity: warning
        annotations:
          summary: "Host high CPU load (instance {{ $labels.instance }})"
          description: "CPU load is > 90% for 5m. Current value: {{ $value | printf \"%.2f\" }}%"

      # Alert: Low Available Memory (< 10% available for 5 minutes)
      - alert: HostLowMemory
        expr: (node_memory_MemAvailable_bytes / node_memory_MemTotal_bytes) * 100 < 10
        for: 5m
        labels:
          severity: critical
        annotations:
          summary: "Host low available memory (instance {{ $labels.instance }})"
          description: "Available RAM is below 10%. Value: {{ $value | printf \"%.2f\" }}%"

      # Alert: Disk Space Exhaustion (< 10% free space)
      - alert: HostDiskFillingFast
        expr: (node_filesystem_avail_bytes{fstype!~"tmpfs|fuse.*"} / node_filesystem_size_bytes{fstype!~"tmpfs|fuse.*"}) * 100 < 10
        for: 5m
        labels:
          severity: critical
        annotations:
          summary: "Filesystem filling fast on {{ $labels.mountpoint }} ({{ $labels.instance }})"
          description: "Free disk space is below 10%. Value: {{ $value | printf \"%.2f\" }}%"

      # Alert: Zombie Processes Detected (> 5 zombies for 15 minutes)
      - alert: HostZombieProcesses
        expr: node_processes_zombie_count > 5
        for: 15m
        labels:
          severity: warning
        annotations:
          summary: "Zombie processes accumulating on {{ $labels.instance }}"
          description: "Found {{ $value }} zombie processes. Parent process is leaking child tasks."

      # Alert: Stale Backups (> 30 hours since last backup)
      - alert: HostBackupStale
        expr: (time() - node_backup_last_success_timestamp) > 108000
        for: 1h
        labels:
          severity: critical
        annotations:
          summary: "Backup pipeline failed on {{ $labels.instance }}"
          description: "No successful daily backup recorded in over 30 hours."
```

---

## 3. Execution & Verification

### Step-by-Step Test Procedure

1. **Deploy and Run Installer**:
   ```bash
   sudo chmod +x ./install-node-exporter.sh
   sudo ./install-node-exporter.sh
   ```

2. **Verify Systemd Service Status**:
   ```bash
   systemctl status node_exporter.service
   ```

3. **Simulated Verification Output**:
   ```text
   ● node_exporter.service - Prometheus Node Exporter
        Loaded: loaded (/etc/systemd/system/node_exporter.service; enabled; preset: enabled)
        Active: active (running) since Wed 2026-11-26 11:20:01 UTC; 12s ago
      Main PID: 58210 (node_exporter)
         Tasks: 5 (limit: 4680)
        Memory: 14.5M (max: 128M)
        CGroup: /system.slice/node_exporter.service
                └─58210 /usr/local/bin/node_exporter --collector.systemd --collector.processes --collector.textfile.directory=/var/lib/node_exporter/textfile_collector --web.listen-address=0.0.0.0:9100
   ```

4. **Generate and Verify Custom Textfile Metrics**:
   ```bash
   sudo chmod +x ./custom-metrics.sh
   sudo ./custom-metrics.sh
   
   # Query Node Exporter local endpoint directly
   curl -s http://localhost:9100/metrics | grep -E "node_processes_zombie_count|node_security_updates"
   ```
   *Expected Output:*
   ```text
   # HELP node_processes_zombie_count Number of zombie processes on host
   # TYPE node_processes_zombie_count gauge
   node_processes_zombie_count 0
   # HELP node_security_updates_pending Number of unapplied security packages
   # TYPE node_security_updates_pending gauge
   node_security_updates_pending 2
   ```

---

## 4. Failure Modes & Edge Cases

| Failure Mode / Incident | Root Cause | Remediation Runbook |
| :--- | :--- | :--- |
| **Port 9100 Connection Refused** | Service crashed on bad command flag or firewall blocked port. | Run `journalctl -u node_exporter -e`. Check firewall: `sudo ufw allow from <PROMETHEUS_IP> to any port 9100 proto tcp`. |
| **Custom Textfile Metrics Missing** | Textfile directory path mismatch or permission denied. | Verify directory permissions: `ls -ld /var/lib/node_exporter/textfile_collector`. Ensure `chown -R node_exporter:node_exporter` is set. |
| **High CPU Usage by Node Exporter** | Heavy filesystem collectors scanning virtual mounts (e.g., thousands of Docker/Kubernetes container mounts). | Pass `--collector.filesystem.mount-points-exclude="^/(sys|proc|dev|host|etc)($|/)"` to exclude pseudo-filesystems. |
| **`Read-only file system` with Textfile Collector** | Custom metrics script ran as `node_exporter` with `ProtectSystem=strict` without `ReadWritePaths=`. | Ensure `ReadWritePaths=${TEXTFILE_DIR}` is set in the systemd service unit. |

---

## 5. Production Considerations

- **Network Security & TLS**: Never leave Node Exporter exposed unauthenticated to the public internet (`0.0.0.0:9100`). Bind to internal private VPC IPs (`--web.listen-address=10.0.1.50:9100`) or configure TLS and Basic Auth via `--web.config.file=web-config.yml`.
- **Systemd Collector Filtering**: By default, `--collector.systemd` monitors every unit. On busy hosts, restrict monitoring to critical services using `--collector.systemd.unit-include="^(nginx|docker|postgres|ssh).*$"`.
- **Atomic Textfile Writes**: Never append directly (`>>`) to `.prom` files in the textfile directory. An incomplete write can cause Prometheus to fail scrape evaluation with a parse error. Always write to a `.tmp` file on the same filesystem and move it via `mv`.

---
*Module 07 Complete. Proceed to Module 08: Technical Interview Preparation.*
