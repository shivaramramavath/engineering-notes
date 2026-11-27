# Linux System Monitoring: Prometheus Node Exporter and Kernel Metrics

| Metadata | Details |
| :--- | :--- |
| **Topic** | Linux Monitoring Infrastructure, Prometheus Node Exporter, Kernel Metrics Extraction, Alert Rules |
| **Module** | `05-production` |
| **Track** | `04-devops/linux` |
| **Prerequisites** | `03-system/04-systemd-and-services.md`, `04-networking/08-ss-and-port-troubleshooting.md`, `05-production/05-troubleshooting.md` |
| **Target Audience** | SREs, DevOps Engineers, Monitoring Engineers, Systems Administrators |
| **Target Distribution** | Ubuntu 24.04 LTS, Rocky Linux / RHEL 9.4 |
| **Key Tools / Ports** | `node_exporter`, Prometheus, `/metrics` (Port 9100), `/proc/`, `/sys/`, PromQL |

---

## Table of Contents
1. [What & Why](#what--why)
2. [Core Concepts](#core-concepts)
3. [Internal Working](#internal-working)
4. [Syntax & CLI Tools](#syntax--cli-tools)
5. [Progressive Practical Examples](#progressive-practical-examples)
6. [Best Practices](#best-practices)
7. [Common Mistakes & Gotchas](#common-mistakes--gotchas)
8. [Performance, Security & Cost Implications](#performance-security--cost-implications)
9. [Comparison Table / Alternatives](#comparison-table--alternatives)
10. [Real-World Troubleshooting & Interview Q&A](#real-world-troubleshooting--interview-qa)
11. [Hands-On Practice Exercises](#hands-on-practice-exercises)
12. [Summary Cheat Sheet & Further Reading](#summary-cheat-sheet--further-reading)

---

## What & Why

In production operations, you cannot manage what you do not measure. Without real-time telemetry:
- Disks fill to 100% capacity unannounced, crashing databases and corrupting transactions.
- Memory leaks trigger silent OOM killer massacres during off-peak hours.
- Network interfaces drop packets under silent bandwidth throttling, degrading customer API latency without generating application errors.

Modern cloud-native Linux monitoring standardizes on the **pull-based metrics paradigm** pioneered by **Prometheus**. At the foundation of every Linux telemetry stack sits **Node Exporter**, an official, lightweight daemon written in Go.
Node Exporter reads low-level hardware, memory, disk, network, and process metrics directly from the Linux `/proc` and `/sys` virtual filesystems and exposes them as clean, human-readable Prometheus text-formatted metrics over HTTP on port `9100`.

Understanding how Node Exporter extracts kernel counters, how to author custom metrics via the **Textfile Collector**, and how to write mathematically sound **PromQL alert rules** is the cornerstone of proactive site reliability.

---

## Core Concepts

### Pull vs Push Telemetry Models
- **Push Model (Legacy Nagios, Zabbix, StatsD)**: Every host or container runs an agent that establishes outbound connections and pushes metrics to a central monitoring server. Risks overwhelming the monitoring backend during traffic spikes and makes it difficult to detect dead nodes (is the server dead, or did the network just stutter?).
- **Pull Model (Prometheus Standard)**: Central Prometheus servers periodically scrape targets over HTTP (`GET http://host:9100/metrics`) at a fixed scrape interval (e.g. every 15s). Prometheus controls the ingestion rate, and if a target fails to respond, Prometheus immediately flags the target as `UP = 0` (immediate host down detection).

### Prometheus Metric Types
1. **Counter**: A monotonically increasing cumulative metric that only increases or resets to zero on system reboot (e.g., `node_network_receive_bytes_total`, `node_cpu_seconds_total`). Analyzed via `rate()` or `irate()`.
2. **Gauge**: A single numerical value that can arbitrarily go up or down, representing instantaneous snapshots of state (e.g., `node_memory_MemAvailable_bytes`, `node_load1`).
3. **Histogram**: Samples observations (usually request durations or payload sizes) and counts them in configurable bucket intervals.

### Essential Node Exporter Collectors
Node Exporter includes modular collectors enabled by default:
- **`cpu`**: Exposes seconds spent by each CPU core in each state (`user`, `system`, `idle`, `iowait`, `steal`). Metric: `node_cpu_seconds_total`.
- **`meminfo`**: Exposes `/proc/meminfo` fields (Free, Available, Buffers, Cached, Swap). Metric: `node_memory_MemAvailable_bytes`.
- **`diskstats`**: Exposes `/proc/diskstats` (IOPS, bytes read/written, time spent doing I/Os). Metric: `node_disk_read_bytes_total`.
- **`filesystem`**: Exposes mounted disk space and inode utilization. Metric: `node_filesystem_free_bytes`, `node_filesystem_size_bytes`.
- **`netdev`**: Exposes network interface counters (`rx_bytes`, `tx_bytes`, `rx_drop`, `tx_errs`). Metric: `node_network_receive_bytes_total`.
- **`textfile`**: Reads custom `.prom` metrics files written to a designated local directory by cron jobs or bash scripts.

---

## Internal Working

```
+-----------------------------------------------------------------------------------------+
|                                    Prometheus Server                                    |
|   1. Scrapes target every 15s: GET http://srv-prod-01:9100/metrics                      |
+----------------------------------------------------+------------------------------------+
                                                     | HTTP GET Port 9100
                                                     v
+-----------------------------------------------------------------------------------------+
|                                 Node Exporter Daemon (Go)                               |
|   - Listens on 0.0.0.0:9100 (or internal IP)                                           |
|   - Dispatches scrape request across active collectors concurrently                     |
+----------------------------------------------------+------------------------------------+
                                                     |
         +-------------------+-----------------------+-------------------+
         |                   |                       |                   |
         v                   v                       v                   v
+-----------------+ +-----------------+ +-----------------+ +-----------------------------+
|  cpu collector  | | meminfo collect | | diskstats coll  | |      textfile collector     |
| /proc/stat      | | /proc/meminfo   | | /proc/diskstats | | /var/lib/node_exporter/text |
+--------+--------+ +--------+--------+ +--------+--------+ +--------------+--------------+
         |                   |                   |                         |
         +-------------------+-------------------+-------------------------+
                                                     |
                                                     v
+-----------------------------------------------------------------------------------------+
|                               Prometheus Exposition Format (HTTP 200)                   |
|   # HELP node_cpu_seconds_total Seconds the CPUs spent in each mode.                    |
|   # TYPE node_cpu_seconds_total counter                                                 |
|   node_cpu_seconds_total{cpu="0",mode="idle"} 1248102.45                                |
|   node_cpu_seconds_total{cpu="0",mode="system"} 48102.12                                |
|   node_memory_MemAvailable_bytes 16421048320                                            |
+-----------------------------------------------------------------------------------------+
```

### The Rate Formula: Why You Never Graph Raw Counters
In Prometheus, counters like `node_cpu_seconds_total` continually tick upwards:
$$\text{At 10:00:00} \rightarrow 100,000 \quad | \quad \text{At 10:01:00} \rightarrow 100,058$$
Graphing the raw counter produces an upward-sloping line that is useless for operational monitoring.
To calculate actual CPU percentage utilization across a 1-minute window, PromQL computes the per-second rate of change using `rate()`:
$$\text{CPU Utilization \%} = \left( 1 - \text{avg by (instance)} \left( \text{rate}(\text{node\_cpu\_seconds\_total}\{\text{mode}="idle"\}[1m]) \right) \right) \times 100$$

---

## Syntax & CLI Tools

### Running and Testing Node Exporter
```bash
# Query the live metrics endpoint of a local Node Exporter instance
curl -s http://localhost:9100/metrics | head -n 30

# Filter for specific metric families using curl and grep
curl -s http://localhost:9100/metrics | grep -E "^node_memory_MemAvailable_bytes"
curl -s http://localhost:9100/metrics | grep -E "^node_filesystem_free_bytes"

# Launch Node Exporter with custom textfile collector directory and disabled collectors
./node_exporter \
  --web.listen-address="127.0.0.1:9100" \
  --collector.textfile.directory="/var/lib/node_exporter/textfile_collector" \
  --no-collector.infiniband \
  --no-collector.xfs
```

---

## Progressive Practical Examples

### Example 1: Installing and Managing Node Exporter as a Hardened Systemd Service
Production deployment of Node Exporter on Ubuntu 24.04 / Rocky Linux 9 using an unprivileged service user.

```bash
#!/usr/bin/env bash
set -euo pipefail

VERSION="1.8.1"
ARCH="amd64"

echo "==> 1. Creating dedicated unprivileged system user..."
sudo useradd --system --no-create-home --shell /bin/false node_exporter || true

echo "==> 2. Downloading official binary release..."
cd /tmp
curl -sSLO "https://github.com/prometheus/node_exporter/releases/download/v${VERSION}/node_exporter-${VERSION}.linux-${ARCH}.tar.gz"
tar -xzf "node_exporter-${VERSION}.linux-${ARCH}.tar.gz"

sudo cp "node_exporter-${VERSION}.linux-${ARCH}/node_exporter" /usr/local/bin/
sudo chown node_exporter:node_exporter /usr/local/bin/node_exporter
rm -rf "node_exporter-${VERSION}*"

echo "==> 3. Setting up textfile collector directory..."
sudo mkdir -p /var/lib/node_exporter/textfile_collector
sudo chown -R node_exporter:node_exporter /var/lib/node_exporter

echo "==> 4. Creating hardened systemd unit file..."
sudo tee /etc/systemd/system/node_exporter.service << 'EOF'
[Unit]
Description=Prometheus Node Exporter
Documentation=https://github.com/prometheus/node_exporter
After=network-online.target
Wants=network-online.target

[Service]
User=node_exporter
Group=node_exporter
Type=simple
ExecStart=/usr/local/bin/node_exporter \
    --web.listen-address=0.0.0.0:9100 \
    --collector.textfile.directory=/var/lib/node_exporter/textfile_collector

Restart=always
RestartSec=5

# Systemd Hardening Sandboxing
ProtectSystem=strict
ProtectHome=yes
NoNewPrivileges=yes
PrivateTmp=yes
ProtectKernelModules=yes
ProtectKernelTunables=yes
ProtectControlGroups=yes
ReadWritePaths=/var/lib/node_exporter/textfile_collector

[Install]
WantedBy=multi-user.target
EOF

sudo systemctl daemon-reload
sudo systemctl enable --now node_exporter

echo "==> 5. Verifying service is operational:"
curl -s http://localhost:9100/metrics | grep "node_exporter_build_info"
```

### Example 2: Authoring Custom Metrics with the Textfile Collector
Expose custom business or operational metrics (e.g. pending security updates or backup age) via a Bash cron script.

```bash
#!/usr/bin/env bash
set -euo pipefail

TEXTFILE_DIR="/var/lib/node_exporter/textfile_collector"
TEMP_FILE="${TEXTFILE_DIR}/custom_maintenance.prom.tmp"
FINAL_FILE="${TEXTFILE_DIR}/custom_maintenance.prom"

# 1. Count pending security package updates on Ubuntu
SECURITY_UPDATES=$(/usr/lib/update-notifier/apt-check 2>&1 | cut -d';' -f2 || echo 0)

# 2. Check age of last successful database backup (seconds since epoch)
LAST_BACKUP_EPOCH=$(stat -c %Y /mnt/backups/latest_db.tar.zst 2>/dev/null || echo 0)
CURRENT_EPOCH=$(date +%s)
BACKUP_AGE_SECONDS=$(( CURRENT_EPOCH - LAST_BACKUP_EPOCH ))

# 3. Write metrics atomically in Prometheus text format
cat << EOF > "${TEMP_FILE}"
# HELP node_pending_security_updates Total number of uninstalled security updates.
# TYPE node_pending_security_updates gauge
node_pending_security_updates ${SECURITY_UPDATES}

# HELP node_last_backup_age_seconds Time in seconds since last successful backup.
# TYPE node_last_backup_age_seconds gauge
node_last_backup_age_seconds ${BACKUP_AGE_SECONDS}
EOF

# Atomically rename to prevent Prometheus scraping a half-written file!
mv "${TEMP_FILE}" "${FINAL_FILE}"
```

```bash
# Query endpoint to verify custom metrics appear in Node Exporter output:
curl -s http://localhost:9100/metrics | grep "node_pending_security_updates"
```

### Example 3: Production PromQL Alert Rules for Linux Infrastructure
Real-world alerting rules defined in Prometheus (`alerts.yml`).

```yaml
groups:
  - name: linux_system_alerts
    rules:
      # 1. Host Down Alert
      - alert: HostDown
        expr: up{job="node_exporter"} == 0
        for: 2m
        labels:
          severity: critical
        annotations:
          summary: "Host {{ $labels.instance }} is down"
          description: "Target has failed to respond to Prometheus scrapes for more than 2 minutes."

      # 2. High CPU Saturation
      - alert: HostHighCpuLoad
        expr: (1 - avg by(instance) (rate(node_cpu_seconds_total{mode="idle"}[5m]))) * 100 > 90
        for: 10m
        labels:
          severity: warning
        annotations:
          summary: "High CPU usage on {{ $labels.instance }}"
          description: "CPU utilization has exceeded 90% for 10 minutes (current: {{ $value | printf \"%.2f\" }}%)."

      # 3. Low Available Memory (< 10% available)
      - alert: HostLowMemory
        expr: (node_memory_MemAvailable_bytes / node_memory_MemTotal_bytes) * 100 < 10
        for: 5m
        labels:
          severity: critical
        annotations:
          summary: "Low memory on {{ $labels.instance }}"
          description: "Available memory has dropped below 10% (current: {{ $value | printf \"%.2f\" }}%). Risk of OOM killer."

      # 4. Disk Space Exhaustion (< 15% free)
      - alert: HostDiskFilling
        expr: (node_filesystem_avail_bytes{fstype!~"tmpfs|overlay"} / node_filesystem_size_bytes{fstype!~"tmpfs|overlay"}) * 100 < 15
        for: 5m
        labels:
          severity: warning
        annotations:
          summary: "Disk {{ $labels.mountpoint }} running low on space"
          description: "Filesystem on {{ $labels.device }} has less than 15% free space remaining."

      # 5. Disk Will Fill Within 6 Hours (Linear Predictive Model)
      - alert: HostDiskWillFillIn6Hours
        expr: predict_linear(node_filesystem_free_bytes{fstype!~"tmpfs"}[4h], 6 * 3600) < 0
        for: 15m
        labels:
          severity: critical
        annotations:
          summary: "Disk {{ $labels.mountpoint }} predicted to fill within 6 hours"
          description: "Based on current write trends over the last 4 hours, disk space will be completely exhausted."
```

### Example 4: Scraping and Testing PromQL Formulas Locally
How SREs validate complex PromQL queries from the command line using `curl`.

```bash
# Query Prometheus HTTP API to compute current 5-minute CPU utilization rate:
curl -s -G "http://prometheus.corp.internal:9090/api/v1/query" \
  --data-urlencode 'query=(1 - avg by(instance) (rate(node_cpu_seconds_total{mode="idle"}[5m]))) * 100' \
  | jq '.data.result[] | {instance: .metric.instance, cpu_pct: .value[1]}'
```

### Example 5: Securing Node Exporter with TLS and Basic Authentication
By default, Node Exporter serves plaintext HTTP without authentication. Secure the endpoint using Node Exporter's built-in web configuration.

```yaml
# /etc/node_exporter/web-config.yml
tls_server_config:
  cert_file: /etc/node_exporter/server.crt
  key_file: /etc/node_exporter/server.key

basic_auth_users:
  # Generated using: htpasswd -B -n -b prometheus_scraper "StrongPass123"
  prometheus_scraper: $2y$10$w...
```

```bash
# Start node_exporter passing the config file:
node_exporter --web.config.file=/etc/node_exporter/web-config.yml
```

---

## Best Practices

1. **Always Use `node_memory_MemAvailable_bytes` for Memory Alerts**: Never write alert rules based on `node_memory_MemFree_bytes`. In Linux, memory used by disk caches (`buff/cache`) is instantly reclaimable. An alert on `MemFree < 10%` will fire false positives 100% of the time on healthy production servers.
2. **Exclude Virtual and Container Filesystems in Disk Alerts**: When alerting on disk space, always filter out ephemeral and pseudo-filesystems using regex: `node_filesystem_avail_bytes{fstype!~"tmpfs|overlay|squashfs"}`. Failing to filter `overlay` results in duplicate spam alerts across every running Docker container.
3. **Use `predict_linear()` for Proactive Disk Exhaustion**: A disk suddenly filling at 5GB per minute might take hours to reach a static 85% threshold, but will crash the server minutes later. Use `predict_linear(node_filesystem_free_bytes[4h], 6 * 3600) < 0` to catch fast-filling disks hours before they fail.
4. **Always Write Textfile Collector Files Atomically**: When scripting custom metrics, never write directly into `/var/lib/node_exporter/textfile_collector/metric.prom`. If Prometheus scrapes the node while your script is mid-write, Node Exporter returns malformed syntax errors. Always write to a `.tmp` file and atomically rename (`mv`) over the target file.
5. **Restrict Port 9100 via Host Firewall**: Node Exporter exposes detailed system internal metrics (mount points, active network interfaces, CPU cores). Restrict incoming traffic on port 9100 strictly to authorized Prometheus scraper IP addresses using `nftables` or cloud security groups.
6. **Normalize CPU Usage by Core Count**: `rate(node_cpu_seconds_total[5m])` calculates per-second time across each core. To calculate true total system percentage, always aggregate with `avg by (instance)` or divide by `count(node_cpu_seconds_total{mode="idle"})`.

---

## Common Mistakes & Gotchas

| Mistake | Why It Fails | Correct Pattern |
| :--- | :--- | :--- |
| Alerting on `node_memory_MemFree_bytes` | Fires false alarms continuously because Linux naturally uses free RAM for page cache. | Alert on `node_memory_MemAvailable_bytes / node_memory_MemTotal_bytes < 0.10`. |
| Writing non-atomic files in textfile collector | Prometheus scrapes half-written files, causing parser failures and metrics dropouts. | Write to `.tmp` file first and use atomic `mv` to finalize. |
| Using `irate()` in alert rules | `irate()` calculates rate over the last 2 data points; brief transient spikes trigger false alerts. | Use `rate()` over a smoothed window (e.g. `rate(...[5m])`) for alerting. |
| Leaving port 9100 open to the public internet | Exposes complete infrastructure hardware layout, private IPs, and host configurations to scanners. | Restrict port 9100 to Prometheus servers via firewall (`nftables` / `ufw`). |
| Missing `fstype` exclusions in disk alerts | Triggers false alerts on read-only snap packages (`squashfs`) or RAM disks (`tmpfs`). | Add `{fstype!~"tmpfs|squashfs|overlay"}` filter. |
| Summing rates across counter resets incorrectly | Using `sum(rate(...))` is correct; using `rate(sum(...))` miscalculates counter resets and corrupts data. | **Always calculate `rate()` first, then apply `sum()` or `avg()`**: `sum(rate(...))`. |

---

## Performance, Security & Cost Implications

### Performance
- **Low Footprint**: Node Exporter is written in Go and uses non-blocking read calls on `/proc` files. A standard scrape consumes **less than 15ms of CPU time and ~20MB of RAM**, making it safe for micro-instances and massive bare-metal nodes alike.
- **Scrape Interval vs Storage**: Scraping every 1 second generates 15x more time-series data in Prometheus than scraping every 15 seconds. For standard infrastructure OS metrics, a **15s to 30s scrape interval** provides optimal resolution without saturating time-series database (TSDB) storage.

### Security
- **Least Privilege Execution**: Node Exporter requires **zero root privileges**. All metrics in `/proc` and `/sys` are world-readable. Always run Node Exporter under an unprivileged `node_exporter` system account sandboxed with systemd protections (`ProtectSystem=strict`, `NoNewPrivileges=yes`).

### Cost
- In managed cloud Prometheus platforms (AWS Managed Prometheus, Grafana Cloud, Datadog), bills scale per ingested metric sample. Disabling unnecessary or noisy Node Exporter collectors (e.g. `--no-collector.bcache`, `--no-collector.infiniband`) reduces ingested metric counts by **30%–50%**, directly lowering monitoring bills.

---

## Comparison Table / Alternatives

| Feature / Agent | Prometheus Node Exporter | Telegraf (InfluxData) | Vector (Datadog) | Datadog Agent |
| :--- | :--- | :--- | :--- | :--- |
| **Model** | Pull (HTTP `/metrics`) | Push & Pull | Push & Pull (Event stream) | Push (Proprietary) |
| **Ecosystem** | Prometheus / Grafana / VictoriaMetrics | InfluxDB / TIG Stack | Kafka / ClickHouse / S3 | Datadog Cloud Platform |
| **Language** | Go (Official Prometheus) | Go | Rust (Ultra-fast) | Python / Go |
| **Memory Footprint**| ~20 MB | ~50 MB | ~15 MB | ~150 MB–300 MB |
| **Custom Metrics** | Textfile Collector (`.prom`) | Exec Plugin / Script | Custom Lua/VRL scripts | Custom Python checks |
| **Best Used For** | Cloud-native Linux telemetry | Multi-database metrics | High-throughput log & metric routing | Full-stack APM SaaS |

---

## Real-World Troubleshooting & Interview Q&A

### Q1: Write the PromQL query to calculate overall percentage CPU utilization across an entire Linux node over a 5-minute window.
<details>
<summary>View Solution</summary>

```promql
(1 - avg by(instance) (rate(node_cpu_seconds_total{mode="idle"}[5m]))) * 100
```
**Breakdown**:
1. `node_cpu_seconds_total{mode="idle"}[5m]`: Samples the idle CPU seconds counter over 5 minutes.
2. `rate(...[5m])`: Computes per-second idle time per core.
3. `avg by(instance)(...)`: Averages the idle rate across all CPU cores on the host.
4. `(1 - ...)`: Inverts idle time to derive active utilization.
5. `* 100`: Converts to a human-readable percentage (0–100%).
</details>

### Q2: Why is `sum(rate(node_cpu_seconds_total[5m]))` correct, but `rate(sum(node_cpu_seconds_total)[5m])` completely wrong?
<details>
<summary>View Solution</summary>

**Rule of Thumb in Prometheus**: Always apply `rate()` to raw counters **before** applying aggregation functions like `sum()` or `avg()`.

**Why**:
Counters periodically reset to zero (e.g., when a host reboots or a service restarts).
- `rate(node_cpu_seconds_total[5m])` tracks individual counter series and automatically detects resets, compensating for the drop cleanly.
- If you run `sum(node_cpu_seconds_total)` first, you combine multiple counters into an untyped gauge. If one CPU core resets or goes offline, the `sum()` value suddenly drops, causing `rate()` to misinterpret the drop as a catastrophic spike or anomalous negative value.
</details>

### Q3: How do you calculate percentage disk space used on the root partition (`/`) in PromQL?
<details>
<summary>View Solution</summary>

```promql
(1 - (node_filesystem_avail_bytes{mountpoint="/"} / node_filesystem_size_bytes{mountpoint="/"})) * 100
```
*Note*: Use `node_filesystem_avail_bytes` rather than `node_filesystem_free_bytes` because `avail_bytes` reflects the actual storage usable by unprivileged processes (accounting for the 5% root reservation blocks in ext4).
</details>

### Q4: What does the `node_exporter_build_info` metric do?
<details>
<summary>View Solution</summary>

`node_exporter_build_info` is an informational gauge with a constant value of `1`. It carries version and build metadata in its labels:
```text
node_exporter_build_info{branch="HEAD",goversion="go1.22.4",revision="...",version="1.8.1"} 1
```
It is widely used in Grafana dashboards and Prometheus queries via label joining (`on(instance) group_left(version)`) to display operating system versions and agent software versions in tabular reports.
</details>

### Q5: How do you identify disk I/O wait percentage in PromQL?
<details>
<summary>View Solution</summary>

```promql
avg by(instance) (rate(node_cpu_seconds_total{mode="iowait"}[5m])) * 100
```
This isolates the percentage of CPU time spent stalled on storage I/O operations.
</details>

### Q6: How does the Prometheus `predict_linear()` function work for proactive disk alerting?
<details>
<summary>View Solution</summary>

`predict_linear(v[range], t)` performs a simple linear regression calculation over the historical time series `v` across the given `range` window, and predicts what the value will be `t` seconds into the future.
```promql
predict_linear(node_filesystem_free_bytes{mountpoint="/"}[4h], 8 * 3600) < 0
```
This evaluates the write slope over the last 4 hours and alerts if the projected free space will drop below 0 bytes in the next 8 hours.
</details>

### Q7: Why should you avoid scraping Node Exporter through a reverse proxy or load balancer?
<details>
<summary>View Solution</summary>

Prometheus targets should be scraped directly host-to-host:
1. Scraping through a load balancer scrambles target identities: Prometheus would scrape different backend nodes alternately on every pull, corrupting counter series continuity.
2. Load balancers introduce intermediary timeout delays and single points of failure.
3. If an individual node goes down, a load balancer removes it from rotation, causing Prometheus to lose visibility rather than immediately firing a `HostDown` alert.
</details>

### Q8: How can you measure network interface bandwidth utilization in PromQL?
<details>
<summary>View Solution</summary>

- **Ingress Bandwidth (bits/sec)**:
  ```promql
  rate(node_network_receive_bytes_total{device="eth0"}[1m]) * 8
  ```
- **Egress Bandwidth (bits/sec)**:
  ```promql
  rate(node_network_transmit_bytes_total{device="eth0"}[1m]) * 8
  ```
Multiplying by 8 converts bytes per second into standard networking bits per second (bps).
</details>

---

## Hands-On Practice Exercises

### Exercise 1: Querying Raw Metrics via Curl
**Task**: If Node Exporter is running on your machine, query port 9100. If not, inspect raw kernel metrics directly from `/proc/stat` and `/proc/meminfo`.
<details>
<summary>View Solution</summary>

```bash
# Query Node Exporter if active
if curl -s http://localhost:9100/metrics &>/dev/null; then
    curl -s http://localhost:9100/metrics | grep -E "node_cpu_seconds_total|node_memory_MemAvailable" | head -n 15
else
    echo "Node exporter not active. Reading directly from kernel:"
    head -n 5 /proc/stat
    head -n 5 /proc/meminfo
fi
```
</details>

### Exercise 2: Simulating Custom Metrics Generation for the Textfile Collector
**Task**: Write a bash script that captures system uptime in days and root disk usage percentage, formatting them as Prometheus textfile metrics.
<details>
<summary>View Solution</summary>

```bash
#!/usr/bin/env bash
set -euo pipefail

OUTFILE="/tmp/custom_node_metrics.prom"

# 1. Calculate uptime days
UPTIME_DAYS=$(awk '{print $1/86400}' /proc/uptime)

# 2. Calculate root disk usage percentage
ROOT_DISK_USAGE=$(df / | awk 'NR==2 {print substr($5, 1, length($5)-1)}')

# 3. Format output
cat << EOF > "${OUTFILE}"
# HELP custom_system_uptime_days System uptime measured in decimal days.
# TYPE custom_system_uptime_days gauge
custom_system_uptime_days ${UPTIME_DAYS}

# HELP custom_root_disk_usage_percent Percentage of root partition used.
# TYPE custom_root_disk_usage_percent gauge
custom_root_disk_usage_percent ${ROOT_DISK_USAGE}
EOF

cat "${OUTFILE}"
rm -f "${OUTFILE}"
```
</details>

### Exercise 3: Validating PromQL Alert Expression Logic
**Task**: Given total memory 16GB and available memory 1.2GB, calculate whether the alert expression `(MemAvailable / MemTotal) * 100 < 10` would fire.
<details>
<summary>View Solution</summary>

$$\text{Ratio} = \frac{1.2}{16} \times 100 = 7.5\%$$
Because $7.5\% < 10\%$, the alert condition evaluates to **TRUE** and triggers an alert.
</details>

### Exercise 4: Inspecting Local Network Drop Counters
**Task**: Extract network interface drop and error counters using `/proc/net/dev`.
<details>
<summary>View Solution</summary>

```bash
cat /proc/net/dev | awk 'NR>2 {print "Interface:", $1, "RX-Drops:", $5, "TX-Drops:", $13}'
```
</details>

### Exercise 5: Profiling Prometheus Exposition Format
**Task**: Verify that Prometheus exposition format mandates `# HELP` and `# TYPE` annotations for every metric family.
<details>
<summary>View Solution</summary>

```text
# HELP <metric_name> <Human-readable description of what this metric measures>
# TYPE <metric_name> <counter|gauge|histogram|summary>
<metric_name>{label_name="label_value"} <numeric_value>
```
</details>

### Exercise 6: Verifying Systemd Sandbox Security of Node Exporter
**Task**: Inspect the security sandbox score of `node_exporter.service` using `systemd-analyze security`.
<details>
<summary>View Solution</summary>

```bash
if systemctl is-active node_exporter &>/dev/null; then
    systemd-analyze security node_exporter.service
else
    echo "Service not installed; skipping test."
fi
```
</details>

---

## Summary Cheat Sheet & Further Reading

### Command Reference
| Operation | Command Example |
| :--- | :--- |
| **Scrape Node Exporter** | `curl -s http://localhost:9100/metrics` |
| **Check Build Info** | `curl -s http://localhost:9100/metrics \| grep node_exporter_build_info` |
| **CPU Utilization (PromQL)** | `(1 - avg by(instance) (rate(node_cpu_seconds_total{mode="idle"}[5m]))) * 100` |
| **Available Memory (PromQL)**| `node_memory_MemAvailable_bytes` |
| **Disk Space Usage %** | `(1 - (node_filesystem_avail_bytes / node_filesystem_size_bytes)) * 100` |
| **Network Egress bps** | `rate(node_network_transmit_bytes_total[1m]) * 8` |
| **System Uptime Metric** | `node_time_seconds - node_boot_time_seconds` |
| **Predict Disk Exhaustion** | `predict_linear(node_filesystem_free_bytes[4h], 6 * 3600) < 0` |

### Further Reading
- [Prometheus Official Documentation](https://prometheus.io/docs/introduction/overview/)
- [Prometheus Node Exporter GitHub Repository](https://github.com/prometheus/node_exporter)
- [PromQL Querying Language Guide](https://prometheus.io/docs/prometheus/latest/querying/basics/)
- [Robust Perception: Prometheus Best Practices & Guides](https://www.robustperception.io/blog/)
