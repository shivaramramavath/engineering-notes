# CLI Commands & Process Signals Cheatsheet

> **Cheatsheet ID:** `NGX-SHT-008`  
> **Topic:** NGINX Command-Line Tooling, Process Signal Control, Systemd Operations, and Linux Debugging  
> **Target Version:** NGINX 1.30+ (Stable) / 1.31+ (Mainline)  
> **Related Concept Notes:** [06-command-line-and-signals.md](../01-fundamentals/06-command-line-and-signals.md), [03-config-parsing-and-reload-mechanics.md](../04-internals/03-config-parsing-and-reload-mechanics.md), [03-operations-zero-downtime-reloads-and-ci.md](../06-production-cloud-kubernetes/03-operations-zero-downtime-reloads-and-ci.md)

---

## 1. NGINX Command Line Flags

| Flag | Purpose | Production Use Case | Example Command |
| :--- | :--- | :--- | :--- |
| `-t` | Test configuration syntax | CI/CD pipeline pre-deployment test | `nginx -t` |
| `-T` | Test syntax and dump entire parsed configuration to stdout | Auditing multi-file configurations & debugging includes | `nginx -T > /tmp/full_nginx.conf` |
| `-v` | Display NGINX version | Quick version verification | `nginx -v` |
| `-V` | Display version, compiler flags, and enabled modules | Verifying HTTP/2, HTTP/3 QUIC, TLS 1.3, and dynamic modules | `nginx -V 2>&1 \| tr ' ' '\n'` |
| `-s <signal>` | Send signal to master process | Reloading, stopping, or reopening log files | `nginx -s reload` |
| `-c <file>` | Specify alternate configuration file path | Staging or isolated config testing | `nginx -t -c /etc/nginx/staging.conf` |
| `-p <prefix>` | Set NGINX prefix path (runtime root) | Running unprivileged or custom directory trees | `nginx -p /app/nginx/ -c conf/nginx.conf` |
| `-g <directive>` | Set global directives outside configuration file | Container runtime injection (e.g. foreground daemon mode) | `nginx -g 'daemon off;'` |

---

## 2. UNIX Signals and Process Management

NGINX uses a Master-Worker process architecture. Signals sent to the **Master PID** orchestrate the worker lifecycle.

### Signal Control Matrix

| Signal | Standard Command | Master Behavior | Worker Behavior |
| :--- | :--- | :--- | :--- |
| `SIGHUP` | `nginx -s reload` or `kill -HUP $PID` | Re-reads configuration, creates new shared zones, spawns new workers with new configuration | Gracefully finishes serving current requests, then terminates cleanly |
| `SIGQUIT` | `nginx -s quit` or `kill -QUIT $PID` | Waits for all workers to finish active connections, then removes PID file and exits | Stops accepting new sockets; cleanly completes in-flight requests |
| `SIGTERM` / `SIGINT` | `nginx -s stop` or `kill -TERM $PID` | Immediate fast shutdown | Terminates active client connections immediately |
| `SIGUSR1` | `nginx -s reopen` or `kill -USR1 $PID` | Reopens all log file descriptors (used for logrotate) | Reopens log file descriptors |
| `SIGUSR2` | `kill -USR2 $PID` | Spawns a new NGINX master binary alongside the old one | Keeps serving on old workers while new master initializes |
| `SIGWINCH` | `kill -WINCH $PID` | Gracefully shuts down old workers (used during zero-downtime binary upgrade) | Stops accepting new connections, drains, and exits |

---

## 3. Zero-Downtime Binary Upgrade Workflow

To upgrade NGINX executable binaries without dropping a single active client connection:

```bash
# Step 1: Identify Old Master PID
OLD_PID=$(cat /run/nginx.pid)

# Step 2: Replace the /usr/sbin/nginx binary (via apt/yum or custom build)

# Step 3: Send USR2 to Old Master to execute new binary
kill -USR2 $OLD_PID
# NGINX renames /run/nginx.pid to /run/nginx.pid.oldbin and starts a new master process

# Step 4: Verify new master and new workers are running cleanly
ps aux | grep nginx

# Step 5: Gracefully shut down old workers
kill -WINCH $OLD_PID

# Step 6: Verify traffic is handled smoothly by new workers.
# If satisfied, terminate old master:
kill -QUIT $OLD_PID

# (Rollback option: If new version misbehaves before step 6, restart old workers and kill new master):
# kill -HUP $OLD_PID
# kill -QUIT $(cat /run/nginx.pid)
```

---

## 4. Systemd Service Control

```bash
# Check service status, active master PID, and memory footprint
systemctl status nginx

# Graceful configuration reload (equivalent to nginx -s reload)
sudo systemctl reload nginx

# Restart service (causes momentary TCP socket drop)
sudo systemctl restart nginx

# View real-time systemd service journal logs
sudo journalctl -u nginx -f -n 100

# View only NGINX startup errors
sudo journalctl -u nginx -e --no-pager
```

---

## 5. Network & Kernel Diagnostic Toolkit

### 5.1 Socket & Port Inspection (`ss`)
```bash
# 1. Check listening TCP and UDP sockets (including HTTP/3 QUIC on UDP 443)
ss -tulpn | grep nginx

# 2. Inspect active client connections and established socket queues
ss -tan state established '( dport = :80 or dport = :443 )' | wc -l

# 3. Check TCP Listen Backlog overflow / drops
ss -lnt '( sport = :443 )'
```

### 5.2 File Descriptors & Limits (`lsof` & `/proc`)
```bash
# 1. Count open file descriptors used by NGINX workers
lsof -c nginx | wc -l

# 2. Inspect kernel limits for a running NGINX worker
PID=$(pgrep -f "nginx: worker" | head -n1)
cat /proc/$PID/limits | grep -E "open files|max processes"
```

### 5.3 Tracing & Network Packet Sniffing (`strace` & `tcpdump`)
```bash
# 1. Trace system calls on an active NGINX worker (epoll_wait, read, write)
sudo strace -p <WORKER_PID> -e trace=network,read,write -s 256

# 2. Capture HTTP/3 QUIC UDP packets on port 443
sudo tcpdump -i any udp port 443 -nn -c 20

# 3. Sniff unencrypted HTTP traffic to upstream backend (port 3000)
sudo tcpdump -i lo -A -s 0 'tcp port 3000 and (((ip[2:2] - ((ip[0]&0xf)<<2)) - ((tcp[12:2]&0xf0)>>2)) != 0)'
```

---

## 6. One-Liner Log Rotation Script

If running in minimal containers or systems without `logrotate`:

```bash
#!/bin/bash
# Atomic zero-downtime log rotation
DATE=$(date +%Y%m%d_%H%M%S)
mv /var/log/nginx/access.log /var/log/nginx/access.log.$DATE
mv /var/log/nginx/error.log  /var/log/nginx/error.log.$DATE

# Reopen log files cleanly
nginx -s reopen

# Compress rotated logs in background
gzip /var/log/nginx/*.$DATE &
```
