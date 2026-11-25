---
title: "Interview Prep: Production Troubleshooting Scenarios & Outage Playbooks"
description: "Master technical interview incident scenarios and debugging playbooks: 502/504 outage triage, ephemeral port exhaustion, memory leaks, and low-level kernel diagnostics."
module: "08-interview"
topic: "06-production-troubleshooting-scenarios"
status: "completed"
tags: ["nginx", "interview", "troubleshooting", "incidents", "outages", "playbooks", "502", "504", "qa"]
prerequisites: ["06-production/04-troubleshooting-playbooks.md", "06-production/05-monitoring-metrics-and-observability.md"]
estimated_read_time: "35 min"
---

# Interview Prep: Production Troubleshooting Scenarios & Outage Playbooks

```
+-------------------------------------------------------------------------------+
| System Incident Response Triage Framework                                     |
|                                                                               |
|  Alert: P1 / P2 Customer-Facing Production Outage                             |
|        │                                                                      |
|        ▼                                                                      |
|  Phase 1: Triage Blast Radius & Health                                        |
|        │  Is NGINX alive? `systemctl status nginx`                            |
|        │  Are ports listening? `sudo ss -tulpn | grep -E '80|443'`            |
|        ▼                                                                      |
|  Phase 2: Error Log Signature Extraction (`/var/log/nginx/error.log`)         |
|        ├─ (111: Connection refused)       --> Backend Down / Port Mismatch    |
|        ├─ (110: Connection timed out)     --> Backend Hung / DB Deadlock      |
|        ├─ (13: Permission denied)         --> Parent Directory Mode / SELinux |
|        └─ (24: Too many open files)       --> Process NOFILE Limit Exceeded   |
|        │                                                                      |
|        ▼                                                                      |
|  Phase 3: Isolate Upstream vs Proxy                                           |
|        │  Inspect timing variables: `$request_time` vs `$upstream_response_time`|
|        ▼                                                                      |
|  Phase 4: Low-Level Tracing                                                   |
|        ├─ System calls: `strace -p <pid> -s 512 -T`                           |
|        └─ Packets:      `tcpdump -i any -nn -s 0 port 8080`                   |
+-------------------------------------------------------------------------------+
```

---

## Table of Contents
1. [Scenario 1: 502 Bad Gateway During Rolling Deployments](#1-scenario-1-502-bad-gateway-during-rolling-deployments)
2. [Scenario 2: 504 Gateway Timeout on Checkout API](#2-scenario-2-504-gateway-timeout-on-checkout-api)
3. [Scenario 3: Ephemeral Port Exhaustion Under Load](#3-scenario-3-ephemeral-port-exhaustion-under-load)
4. [Scenario 4: 403 Forbidden with Correct File Permissions](#4-scenario-4-403-forbidden-with-correct-file-permissions)
5. [Scenario 5: Mysterious Traffic Dropping Every 60 Seconds on WebSockets](#5-scenario-5-mysterious-traffic-dropping-every-60-seconds-on-websockets)
6. [Scenario 6: Information Disclosure Leak of Environment Files](#6-scenario-6-information-disclosure-leak-of-environment-files)
7. [Scenario 7: Corrupted JSON Logs Crashing Logging Pipelines](#7-scenario-7-corrupted-json-logs-crashing-logging-pipelines)
8. [Summary & Incident Triage Rules](#8-summary--incident-triage-rules)

---

## 1. Scenario 1: 502 Bad Gateway During Rolling Deployments

### The Incident
During a continuous deployment of a backend microservice across a cluster of 5 nodes, the monitoring dashboard alerts on a sudden spike in `502 Bad Gateway` errors affecting 3% of user traffic.

### Root Cause Analysis
1. Inspect `/var/log/nginx/error.log`:
   ```text
   connect() failed (111: Connection refused) while connecting to upstream, upstream: "http://10.0.1.12:8080/v1/orders"
   ```
2. The orchestrator stopped the container on `10.0.1.12:8080` to upgrade it.
3. Because NGINX was not configured with circuit breaking or retry policies, any client request dispatched to `10.0.1.12` during the 5-second restart window received an immediate connection refusal and threw a 502 error to the user.

### Production Remediation
Update the `upstream` and `location` blocks to enable passive circuit breaking and automatic retries:

```nginx
upstream backend_cluster {
    least_conn;
    server 10.0.1.11:8080 max_fails=2 fail_timeout=10s;
    server 10.0.1.12:8080 max_fails=2 fail_timeout=10s;
    server 10.0.1.13:8080 max_fails=2 fail_timeout=10s;
    server 10.0.1.14:8080 max_fails=2 fail_timeout=10s;
    server 10.0.1.15:8080 max_fails=2 fail_timeout=10s;
    keepalive 64;
}

server {
    location / {
        proxy_pass http://backend_cluster;
        proxy_http_version 1.1;
        proxy_set_header Connection "";

        # Failover retries: Catch connection refusals and 502s instantly
        proxy_connect_timeout 1s;
        proxy_next_upstream error timeout http_502 http_503;
        proxy_next_upstream_tries 3;
        proxy_next_upstream_timeout 4s;
    }
}
```
*Result*: When a node shuts down, NGINX catches the error within 1 second and transparently retries the request on another active node.

---

## 2. Scenario 2: 504 Gateway Timeout on Checkout API

### The Incident
Users report that clicking "Place Order" spins for 60 seconds and then displays `504 Gateway Timeout`. The API gateway team claims NGINX is failing.

### Root Cause Analysis
1. Inspect NGINX access log timing fields:
   ```json
   {"request":"POST /v1/checkout","status":504,"request_time":60.002,"upstream_connect_time":"0.001","upstream_response_time":"60.002"}
   ```
2. Notice:
   - `upstream_connect_time` is `0.001` (1 millisecond). The TCP connection to the backend succeeded instantly.
   - `upstream_response_time` is `60.002` seconds (matching the default `proxy_read_timeout 60s;`).
3. This mathematically proves that NGINX is **NOT** at fault: NGINX connected immediately, but the backend application service hung and failed to emit response bytes within 60 seconds.
4. Deep inspection of the backend database reveals an unindexed query locking the `orders` table.

### Production Remediation
1. Fix the backend database index and deadlock.
2. For batch operations that legitimately take longer than 60 seconds, increase read timeouts on that specific location:
   ```nginx
   location /v1/checkout/batch {
       proxy_pass http://backend_cluster;
       proxy_read_timeout 180s;
   }
   ```

---

## 3. Scenario 3: Ephemeral Port Exhaustion Under Load

### The Incident
Under a synthetic load benchmark of 15,000 req/sec, NGINX begins throwing thousands of `502 Bad Gateway` errors. Worker CPU usage is under 20%.

### Root Cause Analysis
1. Inspect `/var/log/nginx/error.log`:
   ```text
   connect() failed (99: Cannot assign requested address) while connecting to upstream
   ```
2. Run `ss -s` on the host:
   ```text
   TCP: 62450 (estab 120, closed 61000, orphaned 0, timewait 61000)
   ```
3. Over 61,000 sockets are sitting in the Linux kernel `TIME_WAIT` state!
4. NGINX was proxying using default HTTP/1.0 without upstream keepalives, establishing and closing a fresh TCP connection for every single request. The operating system ran out of ephemeral outbound ports.

### Production Remediation
1. Enable persistent upstream keepalive pooling in NGINX:
   ```nginx
   upstream backend_pool {
       server 10.0.1.10:8080;
       keepalive 256;
   }

   server {
       location / {
           proxy_pass http://backend_pool;
           proxy_http_version 1.1;
           proxy_set_header Connection "";
       }
   }
   ```
2. Tune Linux kernel sysctl to reuse `TIME_WAIT` sockets:
   ```ini
   # /etc/sysctl.d/99-network.conf
   net.ipv4.tcp_tw_reuse = 1
   net.ipv4.ip_local_port_range = 1024 65535
   net.ipv4.tcp_fin_timeout = 15
   ```
*Result*: Sockets drop from 61,000 down to 256 persistent connections, handling 50,000+ req/sec effortlessly.

---

## 4. Scenario 4: 403 Forbidden with Correct File Permissions

### The Incident
A new static documentation portal deployed to `/var/www/docs` returns `403 Forbidden` on every page. The files are verified to be owned by `www-data:www-data` with mode `0644`.

### Root Cause Analysis
1. Inspect `/var/log/nginx/error.log`:
   ```text
   "/var/www/docs/index.html" is forbidden (13: Permission denied)
   ```
2. Audit directory traversal with `namei -l /var/www/docs/index.html`:
   ```text
   drwxr-xr-x root root /
   drwxr-xr-x root root var
   drwx------ root root www      <--- Culprit! Mode 700!
   drwxr-xr-x www-data www-data docs
   -rw-r--r-- www-data www-data index.html
   ```
3. Even though `index.html` is readable, the parent directory `/var/www` was created with mode `700`, preventing the `www-data` user from traversing into the folder.

### Production Remediation
Grant execute permissions along the parent chain:
```bash
sudo chmod 755 /var/www
```

---

## 5. Scenario 5: Mysterious Traffic Dropping Every 60 Seconds on WebSockets

### The Incident
Customer support reports that users on a live trading dashboard are disconnected from the live price ticker precisely every 60 seconds.

### Root Cause Analysis
1. Inspect NGINX configuration for WebSocket proxying.
2. `proxy_read_timeout` was omitted, causing NGINX to default to **60 seconds**.
3. During periods where the backend published no price updates for 60 seconds, NGINX's internal timer expired, sending a `TCP FIN` packet to terminate the socket.

### Production Remediation
```nginx
location /ws/ {
    proxy_pass http://websocket_backend;
    proxy_http_version 1.1;
    proxy_set_header Upgrade $http_upgrade;
    proxy_set_header Connection "upgrade";

    # Extend idle timeout to 1 hour
    proxy_read_timeout 3600s;
    proxy_send_timeout 3600s;
}
```
*Application Best Practice*: Configure the backend WebSocket library to emit heartbeat ping/pong frames every 25 seconds.

---

## 6. Scenario 6: Information Disclosure Leak of Environment Files

### The Incident
A penetration test reveals that downloading `https://example.com/.env` returns the production database credentials and API secrets.

### Root Cause Analysis
The server block configured static file hosting using `root /var/www/app;` where `.env` and `.git` resided in the webroot, without explicit shielding rules.

### Production Remediation
Add a strict path-shielding snippet before any catch-all location:

```nginx
# Deny all hidden files (except ACME challenge)
location ~ /\.(?!well-known) {
    deny all;
    access_log off;
    log_not_found off;
    return 404; # Return 404 instead of 403 to prevent reconnaissance
}

# Deny sensitive file extensions explicitly
location ~* \.(env|git|bak|sql|yaml|yml|config|ini)$ {
    deny all;
    access_log off;
    log_not_found off;
    return 404;
}
```

---

## 7. Scenario 7: Corrupted JSON Logs Crashing Logging Pipelines

### The Incident
The centralized Elasticsearch/Logstash log forwarding cluster crashes with JSON parsing exceptions originating from NGINX access logs.

### Root Cause Analysis
1. Inspect the raw log line:
   ```text
   {"client_ip":"198.51.100.1","user_agent":"Mozilla/5.0 "CustomBot"/1.0"}
   ```
2. A client submitted a `User-Agent` string containing unescaped double quotes (`"CustomBot"`).
3. The NGINX `log_format` was defined without the `escape=json` modifier, printing the literal quote characters and invalidating the JSON syntax of the line.

### Production Remediation
Update the `log_format` definition to enforce native JSON character escaping:

```nginx
# Notice `escape=json`!
log_format json_production escape=json '{'
    '"time_local":"$time_iso8601",'
    '"client_ip":"$remote_addr",'
    '"user_agent":"$http_user_agent"'
'}';
```

---

## 8. Summary & Incident Triage Rules

```
+--------------------------------------------------------------------------------+
| Golden Rules for Live NGINX Incident Triage                                    |
|                                                                                |
| 1. Never Restart Blindly: Use `systemctl reload` or inspect logs first.        |
| 2. Compare Timings: $request_time vs $upstream_response_time isolates fault.   |
| 3. Read Errno Numbers: (111) = Backend down; (110) = Backend hung; (13) = Perm.|
| 4. Audit Traversal: `namei -l /path` verifies execute bits on parent folders.  |
| 5. Keep Upstream Keepalives: `Connection ""` prevents TIME_WAIT port depletion.|
+--------------------------------------------------------------------------------+
```

---

[← Previous Topic: 05-performance-and-internals-qa.md](05-performance-and-internals-qa.md) | [Back to Module Index](README.md) | [Next Topic: 07-nginx-vs-alternatives-qa.md →](07-nginx-vs-alternatives-qa.md)
