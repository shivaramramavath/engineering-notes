# Logging Drivers, Non-Blocking Log Delivery, and Prometheus Monitoring

```
Track: DevOps & Containerization
Module: 06-production
Prerequisites: 02-commands/02-logs-exec-inspect.md, 06-production/01-environments-and-secrets.md
Difficulty: Advanced
Estimated Duration: 50 minutes
Target Docker Version: 27.x+
Last Validated: February 2025
```

---

## Table of Contents
1. [What & Why](#1-what--why)
2. [Core Concepts](#2-core-concepts)
3. [The Docker Logging Pipeline Architecture](#3-the-docker-logging-pipeline-architecture)
4. [The Blocking vs Non-Blocking Dilemma](#4-the-blocking-vs-non-blocking-dilemma)
5. [Logging Drivers Deep Dive](#5-logging-drivers-deep-dive)
   - [5.1 json-file with Log Rotation](#51-json-file-with-log-rotation)
   - [5.2 The `local` Driver (High-Performance Ring Buffer)](#52-the-local-driver-high-performance-ring-buffer)
   - [5.3 Remote Drivers (AWS CloudWatch, Fluentd, Loki, Journald)](#53-remote-drivers-aws-cloudwatch-fluentd-loki-journald)
6. [Syntax & Configuration Reference](#6-syntax--configuration-reference)
7. [Container Monitoring Architecture (cAdvisor & Prometheus)](#7-container-monitoring-architecture-cadvisor--prometheus)
8. [Practical Implementation: Production Telemetry Stack](#8-practical-implementation-production-telemetry-stack)
9. [Production Best Practices & Antipatterns](#9-production-best-practices--antipatterns)
10. [Common Pitfalls & Troubleshooting](#10-common-pitfalls--troubleshooting)
11. [Performance Tuning & Resource Optimization](#11-performance-tuning--resource-optimization)
12. [Security Hardening & Threat Model](#12-security-hardening--threat-model)
13. [Architectural Comparison Table: Logging Drivers](#13-architectural-comparison-table-logging-drivers)
14. [Real-World Scenario Walkthrough](#14-real-world-scenario-walkthrough)
15. [Self-Assessment & Interview Questions](#15-self-assessment--interview-questions)
16. [Hands-on Practice Exercises](#16-hands-on-practice-exercises)
17. [Summary & Reference Cheat Sheet](#summary--reference-cheat-sheet)

---

## 1. What & Why

In 12-factor container architecture, an application should never concern itself with routing or writing its own log files to disk. A containerized application writes all events, access logs, and errors unbuffered to standard output (`stdout`) and standard error (`stderr`). The container runtime captures these streams and handles distribution.

However, unmanaged Docker logging is a primary source of production outages:
- **Disk Exhaustion**: By default, Docker's `json-file` driver retains all container logs indefinitely. A chatty microservice can generate 100GB of JSON logs, filling the host root disk (`/`) and crashing all running containers.
- **Application Thread Freezes (Backpressure)**: When logging to remote aggregators (Splunk, CloudWatch, Syslog), network timeouts or aggregator downtime can cause Docker to block the container's standard output pipe. In blocking mode, **the application process freezes completely** waiting for a `write()` syscall to complete!
- **Lack of Real-Time Resource Visibility**: Without cgroup metrics monitoring, teams fail to detect memory leaks until the Linux kernel Out-Of-Memory (OOM) killer abruptly terminates their containers.

---

## 2. Core Concepts

### The 12-Factor Log Rule
Treat logs as event streams. The containerized application logs to `stdout`/`stderr`. The Docker daemon intercepts the file descriptors via `containerd-shim` and multiplexes them to the configured **logging driver**.

### Delivery Modes: Blocking vs Non-Blocking
- **`mode: blocking` (Default)**: If the logging buffer fills up, Docker pauses the container process until logs are flushed to disk or network. A slow logging driver degrades or completely halts application execution.
- **`mode: non-blocking`**: Docker stores log messages in an in-memory ring buffer (`max-buffer-size: 4m`). If the buffer fills up (e.g. during a network blip to CloudWatch), Docker drops oldest log messages to **guarantee the application never freezes**.

---

## 3. The Docker Logging Pipeline Architecture

```
+-------------------------------------------------------------------------+
|                          CONTAINER NAMESPACE                            |
|                                                                         |
|  Application writes to stdout / stderr (File Descriptors 1 & 2)         |
+------------------------------------+------------------------------------+
                                     |
                                     v (FIFO Pipe)
+-------------------------------------------------------------------------+
|                  containerd-shim / Docker Daemon (dockerd)              |
|                                                                         |
|  Log Multiplexer Buffer:                                                |
|  - If mode=non-blocking: In-memory ring buffer (default 4MB)            |
+------------------------------------+------------------------------------+
                                     |
              +----------------------+----------------------+
              |                                             |
              v                                             v
+-----------------------------+               +---------------------------+
| Local Driver (json-file /   |               | Remote Driver (CloudWatch/|
| local ring-buffer)          |               | Fluentd / Loki / Syslog)  |
| - Rotates at max-size       |               | - Streams over TLS socket |
| - Retains max-file archives |               | - Centralized aggregator  |
+-----------------------------+               +---------------------------+
```

---

## 4. The Blocking vs Non-Blocking Dilemma

```
+-------------------------------------------------------------------------+
|                   BLOCKING VS NON-BLOCKING BEHAVIOR                     |
+-------------------------------------------------------------------------+
| Scenario: Remote Logstash or AWS CloudWatch network connection blips.   |
|                                                                         |
| IN BLOCKING MODE:                                                       |
|  1. Output buffer fills up.                                             |
|  2. Docker kernel pipe blocks on write() syscall.                       |
|  3. Application thread hangs.                                           |
|  4. Incoming HTTP requests queue up -> 504 Gateway Timeout!             |
|  5. OUTCOME: TOTAL APPLICATION OUTAGE CAUSED BY LOGGING!                |
+-------------------------------------------------------------------------+
| IN NON-BLOCKING MODE:                                                   |
|  1. In-memory 4MB ring buffer fills up.                                 |
|  2. Oldest log lines dropped (warning logged to daemon).                |
|  3. Application write() returns immediately in < 1 microsecond.         |
|  4. HTTP requests continue processing without interruption!             |
|  5. OUTCOME: APPLICATION REMAINS 100% OPERATIONAL.                      |
+-------------------------------------------------------------------------+
```

---

## 5. Logging Drivers Deep Dive

### 5.1 `json-file` with Log Rotation
The default driver. Stores logs as JSON files on the host disk under `/var/lib/docker/containers/<id>/<id>-json.log`. Supports `docker logs`.
**Mandatory Configuration**: Always set `max-size` and `max-file`:
```json
{
  "log-driver": "json-file",
  "log-opts": {
    "max-size": "20m",
    "max-file": "5"
  }
}
```

### 5.2 The `local` Driver (High-Performance Ring Buffer)
Introduced in Docker 20.10. An optimized binary format designed specifically to prevent host disk exhaustion.
- Performs automatic compression of rotated logs.
- Retains small binary files rather than verbose JSON strings.
- Significantly lower disk I/O and CPU overhead than `json-file`.
- Fully supports `docker logs`.

### 5.3 Remote Drivers (AWS CloudWatch, Fluentd, Loki, Journald)
- **`awslogs`**: Streams logs directly to AWS CloudWatch Logs. (Caveat: Disables `docker logs` CLI unless configured with dual-logging).
- **`journald`**: Routes logs into the host Linux systemd journal, enabling unified log aggregation via `journalctl`.
- **`fluentd` / `loki`**: Forwards structured log lines to enterprise log aggregators (Elasticsearch, Grafana Loki).

---

## 6. Syntax & Configuration Reference

### Global Daemon Hardening: `/etc/docker/daemon.json`
Configuring global non-blocking log rotation across all containers running on the host:

```json
{
  "log-driver": "local",
  "log-opts": {
    "max-size": "25m",
    "max-file": "5",
    "compress": "true",
    "mode": "non-blocking",
    "max-buffer-size": "4m"
  }
}
```

### Per-Service Configuration in Docker Compose

```yaml
services:
  api:
    image: my-api:latest
    logging:
      driver: "json-file"
      options:
        max-size: "10m"
        max-file: "3"
        mode: "non-blocking"
        max-buffer-size: "4m"
```

### Streaming to AWS CloudWatch via Compose

```yaml
services:
  web:
    image: nginx:alpine
    logging:
      driver: "awslogs"
      options:
        awslogs-region: "us-east-1"
        awslogs-group: "/docker/production/web"
        awslogs-create-group: "true"
        awslogs-stream: "instance-01"
        mode: "non-blocking"
```

---

## 7. Container Monitoring Architecture (cAdvisor & Prometheus)

Monitoring containerized environments requires tracking four core cgroup resource metrics:
1. **CPU Usage**: Cumulative CPU jiffies, throttled periods (`nr_throttled`).
2. **Memory Usage**: Current Resident Set Size (RSS), cache memory, swap, and OOM kills.
3. **Network I/O**: Ingress/egress bytes and dropped packets per interface.
4. **Disk I/O**: Read/write bytes and IOPs across storage devices.

```
+-------------------------------------------------------------+
|                      DOCKER HOST SYSTEM                     |
|                                                              |
|  [Container A]   [Container B]   [Container C]              |
|        |               |               |                    |
|        v               v               v                    |
|  +-------------------------------------------------------+  |
|  | Linux Kernel cgroups (/sys/fs/cgroup/memory,cpu)      |  |
|  +---------------------------+---------------------------+  |
|                              |                              |
|                              v                              |
|  +-------------------------------------------------------+  |
|  | Google cAdvisor (Container Advisor)                   |  |
|  | Daemon container scraping cgroups & Docker Engine API |  |
|  | Exposes metrics on: http://host:8080/metrics         |  |
|  +---------------------------+---------------------------+  |
+------------------------------|------------------------------+
                               |
                               v (Scrape every 15s)
+-------------------------------------------------------------+
|                     PROMETHEUS SERVER                       |
|  - Evaluates alerting rules (e.g. Memory > 90%)             |
|  - Stores time-series metrics in TSDB                       |
+------------------------------+------------------------------+
                               |
                               v
+-------------------------------------------------------------+
|                     GRAFANA DASHBOARD                       |
|  - Real-time visualization of container CPU, RAM, and I/O   |
+-------------------------------------------------------------+
```

---

## 8. Practical Implementation: Production Telemetry Stack

A complete Docker Compose stack deploying cAdvisor, Prometheus, and Grafana to monitor all containers on the host.

```yaml
# telemetry-compose.yaml

services:
  # 1. Google cAdvisor (Scrapes host cgroups directly)
  cadvisor:
    image: gcr.io/cadvisor/cadvisor:v0.49.1
    container_name: monitoring-cadvisor
    restart: unless-stopped
    privileged: true
    devices:
      - /dev/kmsg
    volumes:
      - /:/rootfs:ro
      - /var/run:/var/run:ro
      - /sys:/sys:ro
      - /var/lib/docker/:/var/lib/docker:ro
      - /dev/disk/:/dev/disk:ro
    ports:
      - "127.0.0.1:8080:8080"
    networks:
      - monitoring-net

  # 2. Prometheus Time-Series Metrics Database
  prometheus:
    image: prom/prometheus:v2.53.1
    container_name: monitoring-prometheus
    restart: unless-stopped
    volumes:
      - ./prometheus.yml:/etc/prometheus/prometheus.yml:ro
      - prometheus_data:/prometheus
    ports:
      - "127.0.0.1:9090:9090"
    networks:
      - monitoring-net
    depends_on:
      - cadvisor

  # 3. Grafana Dashboard Visualizer
  grafana:
    image: grafana/grafana:11.1.0
    container_name: monitoring-grafana
    restart: unless-stopped
    volumes:
      - grafana_data:/var/lib/grafana
    ports:
      - "127.0.0.1:3000:3000"
    networks:
      - monitoring-net
    depends_on:
      - prometheus

networks:
  monitoring-net:
    driver: bridge

volumes:
  prometheus_data:
  grafana_data:
```

### Prometheus Configuration (`prometheus.yml`):
```yaml
global:
  scrape_interval: 15s

scrape_configs:
  - job_name: 'cadvisor'
    static_configs:
      - targets: ['cadvisor:8080']

  - job_name: 'docker-daemon'
    static_configs:
      - targets: ['172.17.0.1:9323'] # Docker daemon native metrics
```

---

## 9. Production Best Practices & Antipatterns

### Best Practices
- **Always Enable `mode: non-blocking` in Production**: Never allow slow remote logging servers to freeze mission-critical application worker threads.
- **Enforce Global Log Limits in `daemon.json`**: Configure `max-size: "20m"` and `max-file: "3"` on the Docker daemon so even unconfigured containers are bounded.
- **Prefer the `local` Driver for Local Logging**: Use `local` rather than `json-file` to get automated compression and avoid disk write-amplification.
- **Monitor Container CPU Throttling (`container_cpu_cfs_throttled_periods_total`)**: Tracking throttled periods alerts you when CPU limits are set too aggressively, causing latency spikes.

### Anti-Patterns
- **Logging to In-Container Flat Files (`/var/log/app.log`)**: Writing log files inside the container's writable layer consumes ephemeral storage and requires complex logrotate configurations inside the image.
- **Relying on Default Unbounded `json-file`**: Guaranteed to eventually fill host root storage and cause production outages.
- **Running `docker logs` on Multi-Gigabyte Files**: Parsing massive JSON logs consumes gigabytes of host RAM and locks the daemon.

---

## 10. Common Pitfalls & Troubleshooting

| Symptom | Root Cause | Diagnosis Command | Permanent Fix |
|:---|:---|:---|:---|
| Host root disk is 100% full, but `docker system df` shows low image usage | Unrotated JSON log files in `/var/lib/docker/containers/` | `sudo du -sh /var/lib/docker/containers/*` | Enforce `max-size` and `max-file` in `daemon.json` and truncate current logs. |
| Application freezes when logging; resumes when network restored | Remote logging driver (`awslogs`, `syslog`) running in default `mode: blocking` | `strace -p <PID>` shows process blocked on `write(1, ...)` | Set `mode: "non-blocking"` in logging options. |
| `docker logs` fails with `configured logging driver does not support reading` | Remote logging driver (e.g. `awslogs`, `syslog`) used without dual-logging | Check `docker info --format '{{.LoggingDriver}}'` | Enable BuildKit dual-logging (`log-opts: { cache-disabled: "false" }`). |
| cAdvisor reports `permission denied` accessing cgroups | Host running cgroups v2 without proper read-only mounts | Inspect cAdvisor logs | Ensure `/sys:ro` and `/rootfs:ro` are properly bind-mounted. |
| Docker daemon metrics endpoint returns 404 | Prometheus metrics endpoint disabled in Docker daemon | `curl http://localhost:9323/metrics` | Add `"metrics-addr": "127.0.0.1:9323"` and `"experimental": true` to `daemon.json`. |
| Dropped log lines reported during traffic bursts | Non-blocking ring buffer size (`4m`) exhausted during massive burst | Check Docker daemon systemd journal | Increase buffer size: `max-buffer-size: "16m"`. |

---

## 11. Performance Tuning & Resource Optimization

1. **Dual-Logging Architecture**:
   Docker 20.10+ implements **dual-logging**: even when streaming logs to remote drivers (`awslogs` or `fluentd`), Docker maintains a local cache so `docker logs` continues to function without querying the remote cloud service.
2. **Native Daemon Prometheus Endpoint**:
   Enable Docker's internal engine telemetry in `/etc/docker/daemon.json`:
   ```json
   {
     "metrics-addr": "127.0.0.1:9323",
     "experimental": true
   }
   ```
   This exposes engine-level metrics: active container count, storage driver operations, daemon CPU/memory, and BuildKit cache usage directly to Prometheus.

---

## 12. Security Hardening & Threat Model

```
+--------------------------------------------------------------+
| ATTACK: Denial of Service via Log Flooding Disk Exhaustion   |
+--------------------------------------------------------------+
| An attacker sends millions of invalid HTTP requests to API.  |
| Application logs each error to stdout:                       |
|   `Invalid request from IP: ... Stacktrace: ...`             |
|                                                              |
| WITHOUT LOG ROTATION:                                        |
| - Host /var/lib/docker/containers/<id>/ grows by 5GB/hour.   |
| - Host disk hits 100% capacity within 24 hours.              |
| - Linux kernel fails to write to /tmp or journal.            |
| - SYSTEM-WIDE CRASH OF ALL HOST SERVICES!                    |
+--------------------------------------------------------------+
| WITH LOG ROTATION (max-size=20m, max-file=3):                |
| - Log file caps strictly at 60MB total disk space.           |
| - Oldest logs rotated and purged automatically.              |
| - HOST SYSTEM REMAINS 100% OPERATIONAL.                      |
+--------------------------------------------------------------+
```

---

## 13. Architectural Comparison Table: Logging Drivers

| Driver | Storage Location | Supports `docker logs`? | Performance Overhead | Best Used For |
|:---|:---|:---|:---|:---|
| **`json-file`** | Host Disk (Plain JSON) | **Yes** | Moderate | Standard default with rotation |
| **`local`** | Host Disk (Binary Ring Buffer)| **Yes** | **Lowest (Compressed)** | High-throughput local logging |
| **`awslogs`** | AWS CloudWatch Logs | Yes (via cache) | Low (Non-blocking mode)| AWS ECS / EC2 deployments |
| **`journald`** | Host Systemd Journal | **Yes** | Low | Native Linux server integration|
| **`fluentd`** | Remote Fluentd Daemon | No | Moderate | Enterprise ELK / Splunk |
| **`none`** | Discarded completely | No | **Zero** | Batch compute where logs unneeded|

---

## 14. Real-World Scenario Walkthrough

### Scenario: Investigating an Application Lockup Caused by AWS CloudWatch

#### The Outage
A high-throughput payment API running 20 containers on an EC2 instance suddenly locked up at 2:00 PM. All containers stopped responding to HTTP traffic, but `docker ps` reported status `Up 4 days`. Host CPU and RAM sat at 2%.

#### The Investigation
1. Attached `strace` to the running Node.js process:
   ```bash
   sudo strace -p <PID>
   ```
   The trace showed the process blocked indefinitely on:
   `write(1, "Payment processed...", 42) = ? <unavailable>`
2. File descriptor `1` is `stdout`. The container's stdout pipe was completely frozen.
3. The team checked their logging driver:
   `driver: "awslogs"` in default `mode: "blocking"`.
4. At 2:00 PM, AWS CloudWatch in `us-east-1` had experienced an API throttling event. The Docker daemon's buffer filled up, and Docker blocked the container process pipe!

#### The Remediation
1. Updated `/etc/docker/daemon.json` globally:
   ```json
   {
     "log-driver": "awslogs",
     "log-opts": {
       "mode": "non-blocking",
       "max-buffer-size": "16m"
     }
   }
   ```
2. Reloaded daemon: `sudo systemctl restart docker`.
3. During subsequent CloudWatch throttling incidents, the application experienced zero downtime and processed transactions without interruption.

---

## 15. Self-Assessment & Interview Questions

<details>
<summary><strong>1. What is the fundamental difference between `mode: blocking` and `mode: non-blocking` in Docker logging?</strong></summary>

**Answer:**
In `mode: blocking` (the default), if the logging driver's buffer fills up (e.g., due to disk I/O bottlenecks or remote network latency), the Docker daemon pauses the container process until the buffer is flushed. In `mode: non-blocking`, logs are placed in an in-memory ring buffer (default 4MB); if the buffer fills up, Docker drops older log lines to guarantee that the application process is never paused or blocked.
</details>

<details>
<summary><strong>2. Why should applications running in containers log strictly to `stdout` and `stderr` instead of managing internal log files?</strong></summary>

**Answer:**
Logging to `stdout`/`stderr` decouples the application from host infrastructure. It follows the 12-factor application methodology, allowing the container runtime to route logs to any destination (local disk, AWS CloudWatch, Datadog, Splunk, Fluentd) purely via environment or daemon configuration without modifying application source code. Furthermore, logging to internal files balloons the container's writable layer and risks disk exhaustion.
</details>

<details>
<summary><strong>3. How does the `local` logging driver differ from the default `json-file` driver?</strong></summary>

**Answer:**
The `local` driver stores log messages in an optimized binary ring-buffer format with automatic gzip compression for rotated files, consuming significantly less disk space and CPU compared to `json-file` (which writes verbose plaintext JSON strings). It guarantees bounded disk consumption while fully supporting the `docker logs` CLI command.
</details>

<details>
<summary><strong>4. How does Google's cAdvisor collect metrics from Docker containers without running an agent inside each container?</strong></summary>

**Answer:**
cAdvisor runs as a single daemon container on the host with read-only access to `/sys/fs/cgroup`, `/var/run/docker.sock`, and the host root filesystem. It reads resource metrics (CPU jiffies, memory usage, disk I/O, network traffic) directly from the Linux kernel cgroups and network namespace interfaces assigned to each container, aggregating and exposing them via an open Prometheus `/metrics` endpoint.
</details>

<details>
<summary><strong>5. What parameters must be configured to prevent the `json-file` logging driver from filling the host disk?</strong></summary>

**Answer:**
`max-size` (the maximum size a single log file can reach before rotating, e.g. `20m`) and `max-file` (the maximum number of rotated archive files to retain, e.g. `5`). Together, they enforce a hard ceiling on total disk consumption per container:
$$\text{Max Storage} = \text{max-size} \times \text{max-file}$$
</details>

<details>
<summary><strong>6. How can an administrator manually truncate an oversized container JSON log file on a live host without restarting the container?</strong></summary>

**Answer:**
Using shell truncation (`cat /dev/null > file` or `truncate -s 0 file`):
```bash
sudo truncate -s 0 /var/lib/docker/containers/<id>/<id>-json.log
```
Never use `rm` to delete the file, as the running Docker daemon process will hold the deleted file descriptor open, preventing the Linux kernel from releasing the disk space.
</details>

<details>
<summary><strong>7. What Prometheus metric is used to detect whether a container is experiencing CPU throttling due to container CPU limits?</strong></summary>

**Answer:**
`container_cpu_cfs_throttled_periods_total` (provided by cAdvisor). A rapidly increasing throttled periods metric indicates that the container is exhausting its allocated CPU quota (`--cpus` or `cpu_quota`) within the CFS period window, causing the Linux scheduler to pause its execution.
</details>

<details>
<summary><strong>8. What is "Dual-Logging" in modern Docker versions?</strong></summary>

**Answer:**
Dual-logging is an engine feature where Docker automatically maintains a local ring-buffer cache of recent log entries even when a remote logging driver (like `awslogs`, `syslog`, or `fluentd`) is configured. This allows operators to run `docker logs --tail=50` for real-time debugging directly from the terminal without having to log into remote cloud logging dashboards.
</details>

---

## 16. Hands-on Practice Exercises

<details>
<summary><strong>Exercise 1: Configuring Automated Log Rotation on a Chatty Container</strong></summary>

**Goal:** Create a container that spams logs and verify rotation caps disk usage.

**Step-by-step Solution:**
1. Run container with strict log limits:
   ```bash
   docker run -d --name log-spammer \
     --log-driver json-file \
     --log-opt max-size=1m \
     --log-opt max-file=3 \
     alpine sh -c "while true; do echo 'SPAM LOG LINE $(date)' >> /dev/stdout; sleep 0.01; done"
   ```
2. Monitor log directory:
   ```bash
   ID=$(docker inspect --format '{{.Id}}' log-spammer)
   ls -lh /var/lib/docker/containers/$ID/*-json.log*
   ```
3. Verify files rotate (`*-json.log.1`, `*-json.log.2`) and never exceed 3MB total.
4. Clean up: `docker rm -f log-spammer`.
</details>

<details>
<summary><strong>Exercise 2: Testing Non-Blocking Logging Mode</strong></summary>

**Goal:** Launch a container with non-blocking mode and inspect its log options.

**Step-by-step Solution:**
1. Run container:
   ```bash
   docker run -d --name nonblock-test \
     --log-opt mode=non-blocking \
     --log-opt max-buffer-size=2m \
     alpine sleep 3600
   ```
2. Inspect configuration:
   ```bash
   docker inspect nonblock-test --format '{{json .HostConfig.LogConfig.Config}}'
   # Displays mode: non-blocking
   ```
</details>

<details>
<summary><strong>Exercise 3: Deploying and Querying cAdvisor Locally</strong></summary>

**Goal:** Run cAdvisor and query its metrics endpoint via curl.

**Step-by-step Solution:**
1. Launch cAdvisor:
   ```bash
   docker run -d --name cadvisor -p 8085:8080 -v /:/rootfs:ro -v /var/run:/var/run:ro -v /sys:/sys:ro -v /var/lib/docker/:/var/lib/docker:ro gcr.io/cadvisor/cadvisor:latest
   ```
2. Query Prometheus metrics:
   ```bash
   curl -s http://localhost:8085/metrics | grep container_memory_usage_bytes | head -n 5
   ```
3. Clean up: `docker rm -f cadvisor`.
</details>

<details>
<summary><strong>Exercise 4: Enabling the Native Docker Prometheus Metrics Endpoint</strong></summary>

**Goal:** Configure `/etc/docker/daemon.json` to expose Docker engine metrics on port 9323.

**Step-by-step Solution:**
1. Edit `/etc/docker/daemon.json`:
   ```json
   {
     "metrics-addr": "127.0.0.1:9323",
     "experimental": true
   }
   ```
2. Reload daemon: `sudo systemctl reload docker`.
3. Query metrics: `curl http://127.0.0.1:9323/metrics`.
</details>

<details>
<summary><strong>Exercise 5: Safely Truncating a Live Container Log File</strong></summary>

**Goal:** Truncate a live container log file without restarting the process.

**Step-by-step Solution:**
1. Locate log file for an active container.
2. Truncate using `sudo truncate -s 0 <path_to_log>`.
3. Verify file size becomes 0 bytes immediately.
</details>

<details>
<summary><strong>Exercise 6: Inspecting Real-Time Resource Utilization via `docker stats`</strong></summary>

**Goal:** Use the native CLI to monitor container CPU, memory percentage, and network I/O.

**Step-by-step Solution:**
1. Run `docker stats` in a terminal window.
2. Review live ASCII metrics display.
</details>

---

## 17. Summary & Reference Cheat Sheet

### Production `daemon.json` Logging Standard
```json
{
  "log-driver": "local",
  "log-opts": {
    "max-size": "20m",
    "max-file": "5",
    "compress": "true",
    "mode": "non-blocking",
    "max-buffer-size": "4m"
  }
}
```

### Top 4 Rules of Container Observability
1. **Always set `mode: non-blocking`**: Prevent slow logging backpressure from freezing application processes.
2. **Enforce `max-size` and `max-file` globally**: Prevent host disk full crashes.
3. **Log exclusively to `stdout`/`stderr`**: Follow 12-factor streaming principles.
4. **Deploy cAdvisor + Prometheus**: Monitor real-time memory usage and CPU throttling.

---

[← Previous: 08-tagging-and-release-strategies.md](08-tagging-and-release-strategies.md) | [Next: 10-resource-management-and-oom.md →](10-resource-management-and-oom.md)
