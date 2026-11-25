---
title: "Production Troubleshooting Playbooks, Error Log Dissection & Debugging"
description: "Master troubleshooting guide for production NGINX outages: root-cause triage for 502, 504, 403, 413, and 421 errors, error log anatomy, strace/tcpdump diagnostic workflows, and gdb inspection."
module: "06-production"
topic: "04-troubleshooting-playbooks"
status: "completed"
tags: ["nginx", "troubleshooting", "playbooks", "502-bad-gateway", "504-gateway-timeout", "strace", "tcpdump", "error-log", "debugging"]
prerequisites: ["04-internals/02-request-lifecycle-and-phases.md", "06-production/03-operations-zero-downtime-reloads-and-ci.md"]
estimated_read_time: "40 min"
---

# Production Troubleshooting Playbooks, Error Log Dissection & Debugging

```
+-------------------------------------------------------------------------------+
| Production Outage Triage Decision Tree                                       |
|                                                                               |
|  Alert: Elevated Error Rates / Production Incident                           |
|        │                                                                      |
|        ▼                                                                      |
|  Step 1: Check Process State & Socket Binding                                 |
|        │  systemctl status nginx && sudo ss -tulpn | grep -E '80|443'         |
|        ├─ NOT RUNNING: Check journalctl -u nginx / verify syntax `nginx -t`  |
|        ▼                                                                      |
|  Step 2: Dissect Error Log Signatures (`/var/log/nginx/error.log`)            |
|        ├─ "connect() failed (111: Connection refused)"    --> [Playbook: 502] |
|        ├─ "upstream timed out (110: Connection timed out)" --> [Playbook: 504] |
|        ├─ "Permission denied (13: Permission denied)"     --> [Playbook: 403] |
|        ├─ "client intended to send too large body"        --> [Playbook: 413] |
|        └─ "worker_connections are not enough"             --> [Playbook: 500] |
|        │                                                                      |
|        ▼                                                                      |
|  Step 3: Deep Protocol Tracing                                                |
|        ├─ Socket / Syscall Stalls: `sudo strace -p <worker_pid> -s 512 -T`   |
|        └─ Upstream Packet Loss:    `sudo tcpdump -i eth0 -nn -s 0 port 8080`  |
+-------------------------------------------------------------------------------+
```

---

## Table of Contents
1. [What & Why](#1-what--why)
2. [Anatomy of the NGINX Error Log](#2-anatomy-of-the-nginx-error-log)
3. [Diagnostic Workflow & Outside-In Triage](#3-diagnostic-workflow--outside-in-triage)
4. [Playbook 1: Resolving 502 Bad Gateway Outages](#4-playbook-1-resolving-502-bad-gateway-outages)
5. [Playbook 2: Resolving 504 Gateway Timeout Outages](#5-playbook-2-resolving-504-gateway-timeout-outages)
6. [Playbook 3: Resolving 403 Forbidden & Permission Denied Errors](#6-playbook-3-resolving-403-forbidden--permission-denied-errors)
7. [Playbook 4: Resolving 413, 414, 421 & SSL Misdirected Errors](#7-playbook-4-resolving-413-414-421--ssl-misdirected-errors)
8. [Low-Level Deep Diagnostic Tooling: strace, tcpdump & gdb](#8-low-level-deep-diagnostic-tooling-strace-tcpdump--gdb)
9. [Common Pitfalls & Anti-Patterns During Incident Response](#9-common-pitfalls--anti-patterns-during-incident-response)
10. [Technical Interview Questions & Scenarios](#10-technical-interview-questions--scenarios)
11. [Hands-On Practice Exercises](#11-hands-on-practice-exercises)
12. [Summary & Quick Reference](#12-summary--quick-reference)
13. [Further Reading](#13-further-reading)

---

## 1. What & Why

During an active production incident, guessing configuration changes or randomly restarting services creates extended outages and destroys forensic evidence:
- **Misattributed Blame**: Teams frequently blame NGINX for `502 Bad Gateway` or `504 Gateway Timeout` errors when NGINX is simply the messenger reporting that the backend Node.js, Python, or Java service crashed or deadlocked.
- **Silent Root Causes**: Errors like `13: Permission denied` on Unix domain sockets or static directories stem from Linux directory traversal permissions (`+x`) or SELinux policies, not NGINX configuration syntax.
- **Resource Exhaustion Signals**: Errors like `worker_connections are not enough` or `Too many open files` point to host operating system resource ceilings.

A disciplined troubleshooting playbook systematically maps observable HTTP symptoms and kernel error numbers (`errno`) to actionable remediations.

---

## 2. Anatomy of the NGINX Error Log

Every line in `/var/log/nginx/error.log` follows a standardized diagnostic structure:

```text
2026/11/25 14:15:22 [error] 12345#12345: *67891 connect() failed (111: Connection refused) while connecting to upstream, client: 198.51.100.42, server: api.example.com, request: "GET /v1/checkout HTTP/1.1", upstream: "http://10.0.1.15:8080/v1/checkout", host: "api.example.com"
```

### Field Breakdown:
1. `2026/11/25 14:15:22`: Timestamp of the event.
2. `[error]`: Severity level (`emerg`, `alert`, `crit`, `error`, `warn`, `notice`, `info`, `debug`).
3. `12345#12345`: `PID#TID` (Process ID and Thread ID of the worker).
4. `*67891`: Unique connection counter. All log entries sharing `*67891` belong to the exact same client transaction!
5. `connect() failed (111: Connection refused)`: The operating system syscall that failed, along with the Linux kernel `errno` and descriptive text.
6. `while connecting to upstream`: The internal NGINX phase handler active during the failure.
7. `client: 198.51.100.42`: Real client IP address.
8. `server: api.example.com`: The `server_name` block that matched the transaction.
9. `request: "GET /v1/checkout HTTP/1.1"`: Exact HTTP request line.
10. `upstream: "http://10.0.1.15:8080/..."`: Destination upstream endpoint that caused the error.

---

## 3. Diagnostic Workflow & Outside-In Triage

Follow this 6-step triage pipeline during an active incident:

```
  1. Is NGINX running and listening?
     sudo systemctl status nginx
     sudo ss -tulpn | grep -E ':80|:443'

  2. Is the configuration valid?
     sudo nginx -t

  3. What is the effective running configuration?
     sudo nginx -T | less (Inspects all includes in order)

  4. What does the error log report?
     sudo tail -f /var/log/nginx/error.log | grep -E '\[error\]|\[crit\]'

  5. Reproduce with curl (bypassing browser cache/extensions):
     curl -vI -H "Host: api.example.com" https://127.0.0.1/path

  6. Isolate NGINX from Upstream:
     curl -v http://10.0.1.15:8080/path (Call backend directly)
```

---

## 4. Playbook 1: Resolving 502 Bad Gateway Outages

An HTTP 502 error indicates that NGINX successfully received the request, but while acting as a gateway or proxy, received an invalid response or was unable to establish a connection to the upstream server.

### Symptom 1: `connect() failed (111: Connection refused)`
- **Meaning**: NGINX attempted to open a TCP socket to the upstream IP and port, but the host operating system kernel sent an immediate `TCP RST` packet.
- **Root Causes**:
  1. The backend application process (Node.js, Gunicorn, Spring Boot) crashed or is not running.
  2. The application is listening on `127.0.0.1`, but NGINX is connecting to the external IP `10.0.1.15` (or vice versa).
  3. The application is running in Docker and listening on `localhost` inside its container rather than `0.0.0.0`.
- **Fix**: Check backend service status (`systemctl status my-app` or `docker ps`), verify listening interfaces with `ss -tulpn`, and ensure the `proxy_pass` port matches the application's bound port.

---

### Symptom 2: `connect() failed (13: Permission denied) while connecting to upstream`
- **Meaning**: NGINX attempted to connect to a Unix domain socket (e.g. `unix:/run/gunicorn.sock`), but the worker process user (`www-data` or `nginx`) lacks filesystem permissions to read/write the socket file.
- **Fix**:
  1. Check permissions: `ls -l /run/gunicorn.sock`.
  2. Ensure the socket is owned by `www-data:www-data` or has mode `0660`.
  3. On RHEL/CentOS, check SELinux: `sudo getsebool httpd_can_network_connect`. Enable if disabled:
     ```bash
     sudo setsebool -P httpd_can_network_connect 1
     ```

---

### Symptom 3: `upstream prematurely closed connection while reading response header`
- **Meaning**: The TCP connection succeeded, but the upstream application crashed, was killed by the OS Out-of-Memory (OOM) killer, or terminated the connection while generating the HTTP response.
- **Fix**:
  1. Inspect Linux kernel ring buffer for OOM kills: `dmesg -T | grep -i oom`.
  2. Check application logs for fatal stack traces or unhandled exceptions.
  3. Check backend memory limits in Docker or Kubernetes (`cgroups`).

---

### Symptom 4: `SSL_do_handshake() failed`
- **Meaning**: NGINX was proxying to an HTTPS upstream (`proxy_pass https://...`), but the TLS handshake between NGINX and the backend failed.
- **Root Causes**: Upstream certificate expired, self-signed certificate rejected, or SNI hostname mismatch.
- **Fix**:
  ```nginx
  location / {
      proxy_pass https://internal_backend;
      proxy_ssl_server_name on; # Passes Host in SNI to upstream
      proxy_ssl_name $host;
      # If using self-signed internal certs:
      # proxy_ssl_verify off;
  }
  ```

---

## 5. Playbook 2: Resolving 504 Gateway Timeout Outages

An HTTP 504 error indicates that the upstream server accepted the TCP connection, but failed to return an HTTP response within the configured timeout window.

### Symptom: `upstream timed out (110: Connection timed out)`
- **Root Cause**: The backend application is deadlocked, blocked on a slow database query, frozen in garbage collection, or experiencing thread pool starvation.
- **Default Timeout**: NGINX defaults `proxy_read_timeout` to **60 seconds**.

### Diagnostic Steps:
1. Inspect `$upstream_response_time` in NGINX access logs:
   ```text
   rt=60.002 uct="0.001" uht="60.002" urt="60.002"
   ```
   Notice that `uct` (connect time) was 1ms, but `uht` (header time) took 60 seconds. This proves the network and TCP socket were fast, but the application took longer than 60 seconds to process the request!
2. Inspect database query logs (PostgreSQL `pg_stat_activity` or MySQL `SHOW PROCESSLIST`) for locked tables or slow queries.

### Emergency Remediation:
If long-running batch endpoints legitimately require more time, increase timeouts specifically for that location:
```nginx
location /api/reports/generate {
    proxy_pass http://report_backend;
    proxy_connect_timeout 10s;
    proxy_send_timeout 300s;
    proxy_read_timeout 300s;
}
```

---

## 6. Playbook 3: Resolving 403 Forbidden & Permission Denied Errors

HTTP 403 indicates that NGINX successfully parsed the request and located the file or directory, but was refused permission to access it.

### Symptom 1: Directory Traversal Permission Denied (`errno 13`)
- **Error Log**: `"/var/www/site/index.html" is forbidden (13: Permission denied)`
- **The Golden Rule of Linux Permissions**: For NGINX worker user `www-data` to read a file, **every single parent directory in the path must have execute (`+x`) permissions!**
- **Diagnosis**: Use `namei -l` to audit permissions along the entire chain:
  ```bash
  namei -l /var/www/site/index.html
  ```
  Example output revealing a locked parent folder:
  ```text
  drwxr-xr-x root root /
  drwxr-xr-x root root var
  drwx------ root root www        <--- Culprit! Mode 700 blocks www-data!
  drwxr-xr-x user user site
  -rw-r--r-- user user index.html
  ```
- **Fix**: Grant directory traversal permissions:
  ```bash
  sudo chmod 755 /var/www
  ```

---

### Symptom 2: Directory Index Forbidden by Rule
- **Error Log**: `directory index of "/var/www/site/" is forbidden`
- **Root Cause**: A client requested a directory path (`http://example.com/images/`), but the directory does NOT contain an `index.html` file, and `autoindex` is set to `off` (the secure default).
- **Fix**: Either provide an `index.html` file in that folder, or enable directory browsing explicitly if intended:
  ```nginx
  location /images/ {
      autoindex on;
  }
  ```

---

### Symptom 3: SELinux Denials on RHEL/CentOS/Fedora
- **Error Log**: `(13: Permission denied)` even though Unix permissions are `777`.
- **Root Cause**: SELinux security context prevents NGINX from reading files labeled with standard user contexts (`user_home_t` or `default_t`).
- **Diagnosis**: Inspect SELinux audit logs:
  ```bash
  sudo ausearch -m avc -ts recent
  ```
- **Fix**: Apply the standard web server context (`httpd_sys_content_t`):
  ```bash
  sudo chcon -R -t httpd_sys_content_t /var/www/site/
  # Make context persistent across reboots:
  sudo semanage fcontext -a -t httpd_sys_content_t "/var/www/site(/.*)?"
  sudo restorecon -Rv /var/www/site/
  ```

---

## 7. Playbook 4: Resolving 413, 414, 421 & SSL Misdirected Errors

### 1. HTTP 413 Request Entity Too Large
- **Error Log**: `client intended to send too large body: 15728640 bytes`
- **Root Cause**: Incoming POST/PUT request body exceeds `client_max_body_size` (default is **1MB**).
- **Fix**: Raise the limit in `http`, `server`, or `location`:
  ```nginx
  location /upload/ {
      client_max_body_size 50m;
  }
  ```

### 2. HTTP 414 Request-URI Too Large & HTTP 431 Headers Too Large
- **Root Cause**: OAuth authentication callbacks, SAML tokens, or tracking scripts appending massive query strings exceeding buffer limits.
- **Fix**: Increase header buffer sizes in the `http {}` context:
  ```nginx
  http {
      client_header_buffer_size 4k;
      large_client_header_buffers 4 32k;
  }
  ```

### 3. HTTP 421 Misdirected Request
- **Root Cause**: Occurs primarily under HTTP/2. An HTTP/2 client reuses an existing TLS connection for a different domain because both domains resolve to the same IP address and share a wildcard or multi-domain SAN certificate, but NGINX is configured to require different client certificates or incompatible virtual host settings.
- **Fix**: Disable connection reuse across mismatched hostnames or ensure certificates and TLS settings are unified across virtual hosts.

---

## 8. Low-Level Deep Diagnostic Tooling: strace, tcpdump & gdb

When logs do not reveal why NGINX is hanging, crashing, or misbehaving, drop to kernel syscall and network packet inspection:

### Tooling 1: System Call Tracing with `strace`
Trace active worker processes to see exactly which system call is blocking:

```bash
# Attach to an active NGINX worker process
WORKER_PID=$(pgrep -f "nginx: worker" | head -n1)

# Trace network syscalls with timestamps and print strings up to 512 bytes
sudo strace -p "$WORKER_PID" -f -s 512 -T -e trace=network,file
```

*Common Findings*:
- Stalled on `connect()`: Network firewall or security group dropping SYN packets silently.
- Stalled on `openat()`: Worker blocked on slow NFS / CIFS network storage.
- Flooded with `accept4() = -1 EAGAIN`: High-traffic multi-accept polling (normal non-blocking behavior).

---

### Tooling 2: Network Packet Capture with `tcpdump`
Verify whether packets are reaching the upstream backend:

```bash
# Capture HTTP traffic between NGINX and local backend on port 8080
sudo tcpdump -i any -nn -s 0 -A 'tcp port 8080' -w /tmp/upstream_traffic.pcap
```

Analyze with `tshark` or Wireshark to inspect TCP handshake latency, reset flags, and HTTP status codes directly off the wire.

---

### Tooling 3: Post-Mortem Core Dump Analysis with `gdb`
If NGINX workers crash with `SIGSEGV` (Segmentation Fault) or `SIGBUS`:

1. Enable core dumps in `nginx.conf`:
   ```nginx
   # /etc/nginx/nginx.conf (main context)
   working_directory /tmp/cores;
   worker_rlimit_core 500m;
   ```
2. Set kernel core dump pattern:
   ```bash
   sudo mkdir -p /tmp/cores && sudo chmod 777 /tmp/cores
   sudo sysctl -w kernel.core_pattern=/tmp/cores/core.%e.%p
   ```
3. When a crash occurs, open the core dump in GDB:
   ```bash
   sudo gdb /usr/sbin/nginx /tmp/cores/core.nginx.12345
   (gdb) bt full
   # Prints the complete stack trace and function where the crash happened!
   ```

---

## 9. Common Pitfalls & Anti-Patterns During Incident Response

| Anti-Pattern / Mistake | Root Cause | Observable Production Impact | Correct Solution |
| :--- | :--- | :--- | :--- |
| Blind Service Restarts | Running `systemctl restart nginx` without checking logs | In-flight transactions dropped; does not fix underlying bug | Follow diagnostic workflow; reload only after `nginx -t` |
| Setting `chmod 777` on Files | Attempting to fix 403 Forbidden by opening all permissions | Critical security vulnerability; allows world modification | Audit path with `namei -l`; apply `755` to dirs and `644` to files |
| Increasing Timeouts for All 504s | Raising `proxy_read_timeout` to 600s globally | Backends exhaust thread pools faster; cascading cluster failure | Fix backend query bottlenecks; increase timeouts only on batch routes |
| Disabling SELinux Permanently | Running `setenforce 0` to fix permissions | Breaks compliance (PCI-DSS); leaves host vulnerable | Fix SELinux file contexts with `restorecon` and `semanage` |
| Ignoring `*connection` ID in Logs | Treating error log entries as unrelated individual events | Inability to trace multi-step request failures | Correlate all log lines sharing the same `*N` counter |
| Changing Multiple Directives Simultaneously | Modifying buffers, timeouts, and workers in a single reload | Impossible to determine which change fixed or worsened the issue | Change ONE parameter at a time, measure, and verify |

---

## 10. Technical Interview Questions & Scenarios

<details>
<summary><strong>1. What is the fundamental technical difference between an HTTP 502 Bad Gateway and an HTTP 504 Gateway Timeout?</strong></summary>

- **502 Bad Gateway**: NGINX received an **invalid, unexpected, or non-HTTP response**, or failed to establish a TCP connection with the upstream entirely (e.g. `Connection refused`, upstream crashed mid-transfer, or invalid HTTP headers received). The upstream either never accepted the connection or died while processing.
- **504 Gateway Timeout**: The upstream successfully accepted the TCP connection, but **failed to transmit a response within the configured timeout window** (`proxy_read_timeout`, default 60s). The backend is still alive and working, but running too slowly to meet the gateway's deadline.
</details>

<details>
<summary><strong>2. How do you troubleshoot an NGINX server returning HTTP 403 Forbidden on static assets when file permissions are `chmod 644` and owned by `www-data`?</strong></summary>

1. Check parent directory permissions using `namei -l /path/to/file.html`. Every parent directory must have execute (`+x`) permissions for `www-data` to traverse the tree. If any parent directory lacks `+x`, NGINX receives `EACCES (13: Permission denied)`.
2. Check SELinux status (`sestatus`). If enforcing, verify whether the file context is `httpd_sys_content_t` via `ls -Z`.
3. Check if the location matches a directory and lacks an `index` file with `autoindex off;`.
4. Inspect NGINX configuration for explicit `deny all;` or IP access restrictions.
</details>

<details>
<summary><strong>3. How do you distinguish whether an HTTP 504 timeout is caused by network latency vs backend application slowness?</strong></summary>

Inspect the high-precision upstream timing variables in the NGINX access log:
- `$upstream_connect_time` (`uct`): Time spent establishing the TCP connection to the backend.
- `$upstream_header_time` (`uht`): Time spent waiting for the backend to send the first byte of HTTP response headers.
- `$upstream_response_time` (`urt`): Total time to receive the complete response.
If `uct` is high (e.g. > 1000ms), the issue is network congestion, firewall drops, or routing issues. If `uct` is small (1ms) but `uht` is 60,000ms, the network is perfect, and the backend application code/database is entirely responsible for the timeout.
</details>

<details>
<summary><strong>4. What does `worker_connections are not enough` indicate in `error.log`, and what three settings must be checked?</strong></summary>

It indicates that an NGINX worker process reached its maximum simultaneous connection limit. You must check:
1. `worker_connections` in `events {}` (increase e.g. to 16384 or 65535).
2. `worker_rlimit_nofile` in `nginx.conf` (must be at least double `worker_connections`).
3. OS-level limits in `/etc/security/limits.conf` and systemd `LimitNOFILE=1048576`.
</details>

<details>
<summary><strong>5. How does `strace` help diagnose an NGINX worker process that appears hung at 100% CPU?</strong></summary>

Attaching `strace -p <pid>` reveals what system calls the worker is making. If `strace` outputs thousands of calls per second to `epoll_wait` returning immediately with zero events, the worker is stuck in an active event loop busy-wait. If `strace` outputs zero lines, the worker is executing user-space code without making kernel syscalls, indicating an infinite loop in a regex (`ngx_http_regex_exec`), an in-memory lock spinlock contention, or heavy TLS cryptographic computation.
</details>

<details>
<summary><strong>6. What causes `104: Connection reset by peer` while reading response from upstream?</strong></summary>

This error indicates that the upstream server abruptly terminated the TCP connection by sending a TCP RST packet instead of an orderly FIN/ACK closure. Common causes include:
1. The backend application process crashed (SIGSEGV or uncaught exception) while writing response bytes.
2. The backend OS kernel terminated the application via the Out-of-Memory (OOM) killer.
3. An intermediate firewall or stateful NAT gateway timed out the connection and dropped the state table entry.
</details>

<details>
<summary><strong>7. How do you verify whether NGINX is stripping custom HTTP headers sent by clients?</strong></summary>

By default, NGINX silently drops any HTTP request header containing an **underscore** (`_`) in its name (e.g. `X_Auth_Token` or `user_id`). To verify, test with `curl -H "X_Test_Header: 123" http://localhost/`. If headers are dropped, either rename the headers to use hyphens (`X-Auth-Token`) or configure `underscores_in_headers on;` in the `http {}` context.
</details>

<details>
<summary><strong>8. What is the role of `proxy_next_upstream` during 502/504 outages?</strong></summary>

The `proxy_next_upstream` directive defines under what conditions NGINX will automatically retry a failed request on another upstream server in the cluster. Configuring `proxy_next_upstream error timeout http_502 http_503;` ensures that if one backend node crashes, NGINX immediately reroutes the client request to an alternate healthy backend node transparently, preventing user-visible 502 errors.
</details>

---

## 11. Hands-On Practice Exercises

<details>
<summary><strong>Exercise 1: Reproduce and Resolve a 502 Connection Refused Error</strong></summary>

**Goal**: Trace a dead upstream backend.
1. Configure `proxy_pass http://127.0.0.1:9999;` on a test location.
2. Issue request: `curl -i http://localhost/test`.
3. Verify NGINX returns `HTTP/1.1 502 Bad Gateway`.
4. Inspect `/var/log/nginx/error.log` and verify the exact `(111: Connection refused)` signature.
5. Start a mock server on port 9999 (`python3 -m http.server 9999`) and confirm the request succeeds.
</details>

<details>
<summary><strong>Exercise 2: Audit Permission Traversal Using `namei`</strong></summary>

**Goal**: Trace directory permissions along a filesystem path.
1. Create a nested directory: `sudo mkdir -p /tmp/nginx-test/site`.
2. Restrict parent directory: `sudo chmod 700 /tmp/nginx-test`.
3. Set location root to `/tmp/nginx-test/site` and curl the endpoint. Confirm HTTP 403.
4. Run `namei -l /tmp/nginx-test/site/index.html` and identify the exact blocked directory.
5. Fix with `sudo chmod 755 /tmp/nginx-test` and re-test.
</details>

<details>
<summary><strong>Exercise 3: Simulate and Diagnose a 504 Gateway Timeout</strong></summary>

**Goal**: Measure upstream response times during slow backend processing.
1. Configure upstream proxy with `proxy_read_timeout 5s;`.
2. Start a backend script that sleeps for 8 seconds before returning a response.
3. Send request: `curl -i http://localhost/slow`.
4. Confirm NGINX returns `HTTP/1.1 504 Gateway Timeout` after exactly 5 seconds.
5. Check access log and confirm `urt="5.001"`.
</details>

<details>
<summary><strong>Exercise 4: Trace Live Worker System Calls with `strace`</strong></summary>

**Goal**: Observe NGINX non-blocking socket handling in real-time.
1. Identify a worker PID: `pgrep -f "nginx: worker" | head -n1`.
2. Run `sudo strace -p <PID> -e trace=epoll_wait,accept4,read,write`.
3. In another terminal, run `curl http://localhost/`.
4. Inspect the syscall trace: observe `epoll_wait` unblocking, `read()` reading the request, and `write()` sending response bytes.
</details>

<details>
<summary><strong>Exercise 5: Capture Upstream HTTP Packets with `tcpdump`</strong></summary>

**Goal**: Inspect raw HTTP payloads between NGINX and backends.
1. Capture packets on loopback:
```bash
sudo tcpdump -i lo -nn -A -s 0 'tcp port 3000'
```
2. Trigger an application request through NGINX.
3. Inspect the forwarded HTTP headers (`X-Real-IP`, `Host`, `Connection`) in the tcpdump output.
</details>

<details>
<summary><strong>Exercise 6: Diagnose SELinux File Context Restrictions</strong></summary>

**Goal**: Remediate SELinux permission blocks on RHEL/CentOS.
1. Check current enforcement status: `getenforce`.
2. Inspect audit log for web server denials: `sudo ausearch -m avc -c nginx`.
3. Apply correct context: `sudo chcon -R -t httpd_sys_content_t /var/www/html`.
</details>

---

## 12. Summary & Quick Reference

```
+--------------------------------------------------------------------------------+
| NGINX Troubleshooting Quick Reference                                          |
|                                                                                |
| Status Code | Error Log Key Phrase             | Most Likely Cause             |
| :---------- | :------------------------------- | :---------------------------- |
| **502**     | Connection refused (111)         | Backend down / wrong port     |
| **502**     | Permission denied (13)           | Socket file permissions / SELinux|
| **502**     | Prematurely closed connection    | Backend crashed (OOM / panic) |
| **504**     | Upstream timed out (110)         | Backend slow / DB deadlock    |
| **403**     | Forbidden (13: Permission denied)| Parent dir missing +x / SELinux|
| **403**     | Directory index is forbidden     | Missing index.html            |
| **413**     | Too large body                   | Body > client_max_body_size   |
| **414/431** | Request line / header too large  | Large query strings / cookies  |
| **500**     | worker_connections not enough    | High concurrency ceiling hit  |
+--------------------------------------------------------------------------------+
```

---

## 13. Further Reading

- [NGINX Official Docs: Troubleshooting NGINX](https://docs.nginx.com/nginx/admin-guide/monitoring/troubleshooting/)
- [Linux Man Pages: errno(3) System Error Numbers](https://man7.org/linux/man-pages/man3/errno.3.html)
- [Brendan Gregg: Linux Systems Performance Diagnostics](http://www.brendangregg.com/)
- [Next Concept: Monitoring Metrics, Prometheus & Observability](05-monitoring-metrics-and-observability.md)

---

[← Previous Note: 03-operations-zero-downtime-reloads-and-ci.md](03-operations-zero-downtime-reloads-and-ci.md) | [Back to Module Index](README.md) | [Next Note: 05-monitoring-metrics-and-observability.md →](05-monitoring-metrics-and-observability.md)
