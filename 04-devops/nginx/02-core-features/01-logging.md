<!--
================================================================================
Metadata:
  Title: Logging Architecture: Access Logs, Error Logs, JSON Formats & Buffering
  Track: NGINX & Traffic Engineering / DevOps
  Difficulty: Intermediate
  Estimated Time: 50 minutes
  Prerequisites: 01-fundamentals/02-config-structure.md
  Key Terminology: log_format, access_log, error_log, $request_time, $upstream_response_time, JSON Logging, Syslog, Buffering
================================================================================
-->

# Logging Architecture: Access Logs, Error Logs, JSON Formats & Buffering

[Back to Module Overview](./README.md) | [02-reverse-proxy.md ->](./02-reverse-proxy.md)

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

In distributed cloud architectures, NGINX is the frontline witness to all incoming user traffic, security probes, and backend application latency. Its logging subsystem provides the telemetry required for Site Reliability Engineering (SRE), security incident response, and performance profiling.

However, standard default logging creates severe operational bottlenecks in high-throughput environments:
- **Disk I/O Choke**: By default, NGINX writes access logs synchronously on *every single request*. At 20,000 requests per second, this triggers 20,000 synchronous `write()` system calls, saturating SSD IOPS and causing worker event loops to freeze.
- **Unstructured Legacy Formats**: The default `combined` Apache-style log format is difficult to parse reliably in log aggregators (Elasticsearch, Grafana Loki, Datadog), forcing expensive regular expression extraction.
- **Latency Blindness**: Standard logs record timestamps and status codes, but omit critical microsecond timing metrics (`$request_time`, `$upstream_response_time`, `$upstream_connect_time`), leaving engineers blind to whether latency spikes stem from client connection bottlenecks or slow microservice upstreams.

---

## 2. Core Concepts

### Access Logs vs. Error Logs
1. **Access Log (`access_log`)**:
   - Records completed client HTTP transactions.
   - Executed strictly during the **Log Phase** (`NGX_HTTP_LOG_PHASE`) after the response has been sent to the client.
   - Configurable via the `log_format` directive; supports JSON, custom variables, conditional filters (`if=`), and memory buffering.
2. **Error Log (`error_log`)**:
   - Records operational runtime events, daemon errors, SSL handshake failures, and rewrite rule evaluation.
   - Configured with severity levels: `debug`, `info`, `notice`, `warn`, `error`, `crit`, `alert`, `emerg`.
   - Cannot be formatted arbitrarily; uses fixed internal NGINX formatting.

### Critical Performance & Latency Variables
To measure latency across the proxy lifecycle, NGINX provides granular microsecond-precision variables:

```
[ Client ] ----(1)----> [ NGINX ] ----(2)----> [ Upstream Backend ]
           <---(4)-----           <---(3)-----

- $request_time: Total time (in seconds with millisecond resolution) from
  when NGINX read the first bytes from the client until NGINX finished sending
  the final response bytes back to the client. Includes network latency! (1 + 2 + 3 + 4)

- $upstream_connect_time: Time spent establishing the TCP/TLS connection to the upstream. (2)

- $upstream_header_time: Time between connecting to the upstream and receiving the first
  byte of the response header. (2 + 3a)

- $upstream_response_time: Total time spent communicating with the upstream server
  from connection start to the receipt of the last byte of the response body. (2 + 3)
```

---

## 3. How It Works Internally

### Synchronous vs. Buffered Asynchronous Logging
```
+--------------------------------------------------------------------------+
| SYNCHRONOUS LOGGING (Default: High I/O Overhead)                         |
| Request Completes -> Worker issues write(fd, log_line, len) -> Disk I/O  |
| * Worker blocks on disk if storage queue is saturated!                   |
+--------------------------------------------------------------------------+

+--------------------------------------------------------------------------+
| BUFFERED LOGGING (buffer=64k flush=5s: High Throughput)                  |
| Request Completes -> Worker writes string into in-memory ring buffer.    |
| When buffer reaches 64 KB OR timer hits 5 seconds:                       |
| -> Worker executes single batch write() system call to disk!             |
| * Eliminates 95%+ of write syscall overhead!                             |
+--------------------------------------------------------------------------+
```

---

## 4. Syntax & Usage

### The `log_format` Directive
The `log_format` directive defines a named format schema inside the `http {}` context:

```nginx
http {
    # Custom Structured JSON Format
    log_format json_analytics escape=json
      '{'
        '"timestamp":"$time_iso8601",'
        '"client_ip":"$remote_addr",'
        '"request_method":"$request_method",'
        '"request_uri":"$uri",'
        '"query_string":"$args",'
        '"status":$status,'
        '"body_bytes_sent":$body_bytes_sent,'
        '"request_time":$request_time,'
        '"upstream_response_time":"$upstream_response_time",'
        '"upstream_addr":"$upstream_addr",'
        '"http_referrer":"$http_referer",'
        '"http_user_agent":"$http_user_agent"'
      '}';

    # Attach format to access log with buffering
    access_log /var/log/nginx/access.json.log json_analytics buffer=64k flush=5s;
}
```

### The `error_log` Directive
```nginx
# Syntax: error_log file [level];
# Levels: debug, info, notice, warn, error, crit, alert, emerg

# Production default: log warnings and errors
error_log /var/log/nginx/error.log warn;

# Diagnostic debug log (requires NGINX compiled with --with-debug)
error_log /var/log/nginx/debug.log debug;
```

---

## 5. Practical Examples

### Example 1: Production JSON Logging for ELK / Grafana Loki
```nginx
http {
    # Production-ready JSON logger with escape=json to sanitize malicious payloads
    log_format json_combined escape=json '{'
        '"time_local":"$time_iso8601",'
        '"remote_addr":"$remote_addr",'
        '"remote_user":"$remote_user",'
        '"request":"$request",'
        '"status":$status,'
        '"body_bytes_sent":$body_bytes_sent,'
        '"request_time":$request_time,'
        '"upstream_response_time":"$upstream_response_time",'
        '"upstream_status":"$upstream_status",'
        '"upstream_connect_time":"$upstream_connect_time",'
        '"http_referrer":"$http_referer",'
        '"http_user_agent":"$http_user_agent",'
        '"pipe":"$pipe"'
    '}';

    server {
        listen 80;
        server_name api.internal;

        access_log /var/log/nginx/api_access.json json_combined buffer=32k flush=10s;
        
        location / {
            proxy_pass http://127.0.0.1:8080;
        }
    }
}
```

### Example 2: Conditional Logging to Filter Health Checks
```nginx
http {
    # Map request URI to determine if request is loggable
    # Suppress Kubernetes / ELB health check spam (e.g. /healthz)
    map $request_uri $is_loggable {
        ~^/healthz    0; # Do not log health checks
        ~^/ready      0; # Do not log readiness probes
        default       1; # Log all other application traffic
    }

    server {
        listen 80;
        server_name app.internal;

        # The 'if=' parameter evaluates the mapped variable
        access_log /var/log/nginx/app.log combined if=$is_loggable;

        location /healthz {
            return 200 "healthy\n";
        }

        location / {
            proxy_pass http://app_upstream;
        }
    }
}
```

### Example 3: Streaming Logs Directly to Remote Syslog / Vector
```nginx
http {
    log_format syslog_format '$remote_addr [$time_iso8601] "$request" $status $body_bytes_sent $request_time';

    # Send logs directly over UDP port 514 to a remote log collector without touching local disk!
    access_log syslog:server=10.0.0.50:514,facility=local7,tag=nginx,severity=info syslog_format;
    
    # Error logs can also stream to syslog
    error_log syslog:server=10.0.0.50:514,facility=local7,tag=nginx_err warn;
}
```

### Example 4: Debugging Specific Client IPs with `debug_connection`
```nginx
# You do not need to enable verbose debug logging for all clients (which generates gigabytes of logs)
# Enable debug logs ONLY for your developer IP:
events {
    worker_connections 1024;
    # Enables debug logs strictly for connections from 203.0.113.45
    debug_connection 203.0.113.45;
}

http {
    error_log /var/log/nginx/error.log debug;
}
```

### Example 5: High-Performance Gzip-Compressed Log Writes
```nginx
http {
    log_format main '$remote_addr - $request - $status';

    # NGINX compresses log data in memory before writing to disk (saves 85%+ disk space)
    access_log /var/log/nginx/archive.log.gz main gzip=1 buffer=64k flush=1m;
}
```

---

## 6. Best Practices

- **Always Use `escape=json` When Logging JSON**: If a client sends a malicious User-Agent containing double quotes or raw control characters, standard string concatenation breaks JSON syntax in log collectors. `escape=json` automatically escapes characters to guarantee valid JSON.
- **Enable Buffering on High-Traffic Nodes**: Add `buffer=64k flush=5s` to `access_log`. This buffers log writes in RAM and issues single vectorized writes, reducing disk I/O wait (`%wa`) significantly.
- **Filter Out High-Frequency Health Checks**: Internal load balancers (AWS ALB, Kubernetes kubelet) query `/healthz` multiple times per second per node. Use `map ... $is_loggable` with `if=$is_loggable` to keep logs clean and reduce storage bills.
- **Record Both `$request_time` and `$upstream_response_time`**: When troubleshooting latency spikes, comparing these two variables reveals whether the slowdown is caused by slow database queries on the backend (`$upstream_response_time`) or slow mobile client connections (`$request_time`).
- **Never Log Sensitive Query Parameters or Authorization Headers**: Strip or mask passwords, session tokens, and Bearer tokens from request logs to comply with PCI-DSS and GDPR.

---

## 7. Common Mistakes & Troubleshooting

| Mistake / Issue | Why It Fails | Practical Remedy |
| :--- | :--- | :--- |
| Disabling access logs completely on static assets without error tracking | When assets 404 or fail permission checks, engineers have zero visibility. | Keep error logging active: `access_log off; log_not_found on;`. |
| Writing invalid JSON due to unescaped strings | Malicious or non-standard user agents inject raw quotes into JSON fields. | Use `escape=json` on the `log_format` definition line. |
| Forgetting `log_format` is restricted to `http {}` | Declaring `log_format` inside `server {}` or `location {}` causes syntax failure. | Place all `log_format` definitions in `http {}` context. |
| Disk fills up because log rotation failed | `logrotate` moved log files, but failed to signal NGINX with `kill -USR1`. | Ensure logrotate postrotate script sends `kill -USR1 $(cat /run/nginx.pid)`. |
| Comparing `$request_time` and `$upstream_response_time` incorrectly | Believing `$request_time` measures only NGINX processing time. | `$request_time` includes reading the client request and sending response over network. |
| Missing logs when using `buffer=` | Expecting immediate log entries during testing while buffer has not filled. | Lower buffer size for testing or wait for the `flush=` timer interval. |

---

## 8. Performance, Security & Cost Implications

- **Performance**: Synchronous disk logging on busy multi-core systems forces worker processes to stall on filesystem journal locks. Using `buffer=64k flush=5s` reduces system call overhead by up to 90%, boosting server request throughput.
- **Security**: Access logs frequently become liability vectors if they store PII (Personally Identifiable Information) like full IP addresses, email parameters in query strings, or Bearer tokens in headers. Use `map` blocks to hash or truncate IPs for GDPR compliance.
- **Cost**: Unconstrained logging of high-frequency health checks in cloud environments inflates CloudWatch, Datadog, or Loki ingestion bills. Suppressing health check logging saves thousands of dollars monthly in enterprise logging costs.

---

## 9. Comparison Table

| Logging Architecture | Delivery Guarantee | CPU Overhead | Disk I/O Overhead | Best Used For |
| :--- | :--- | :--- | :--- | :--- |
| **Standard Synchronous** | Immediate (on request close) | Low | **Very High** (syscall per req) | Low-traffic dev boxes |
| **Buffered Disk (`buffer=64k`)**| Delayed up to `flush` timer | Low | **Very Low** (batched writes) | Production high-traffic nodes |
| **Direct Syslog over UDP** | Best-effort (can drop packets) | Extremely Low | **Zero** (no disk touch) | Cloud-native ephemeral nodes |
| **Direct Syslog over TCP** | Guaranteed | Moderate | **Zero** | Centralized SIEM audit compliance |

---

## 10. Real-World Interview Questions

<details>
<summary><strong>1. What is the exact technical difference between `$request_time` and `$upstream_response_time`?</strong></summary>

**Answer**:
- **`$request_time`**: Measures the total time elapsed in seconds (with millisecond resolution) from when NGINX read the first bytes of the client's request headers until NGINX completed transmitting the final bytes of the response body to the client and closed the socket. It includes client network transit latency, TCP window waits, and client-side processing pauses.
- **`$upstream_response_time`**: Measures the time elapsed between NGINX initiating the connection to the upstream backend server and NGINX receiving the final byte of the response body from that upstream.

*Diagnostic Rule*:
If `$request_time` is 5.0 seconds, but `$upstream_response_time` is only 0.05 seconds, the backend is performing fast, but the client is on a high-latency connection or slow mobile network.
If both `$request_time` and `$upstream_response_time` are 5.0 seconds, the backend application/database is the bottleneck.
</details>

<details>
<summary><strong>2. How does buffered logging (`buffer=64k flush=5s`) improve server performance? What is the trade-off?</strong></summary>

**Answer**:
Under standard logging, NGINX executes a synchronous `write()` system call on every completed request. This requires context switching into kernel mode, updating filesystem metadata, and acquiring filesystem locks. Under high concurrency, worker processes spend significant time waiting in I/O queues.

With buffered logging, NGINX appends serialized log strings into an in-memory memory buffer. A single `write()` system call is issued only when the buffer fills to the specified size (e.g., 64 KB) or when the `flush` timer expires (e.g., every 5 seconds). This consolidates thousands of system calls into a single batch write.

*Trade-off*: Log entries are delayed by up to the flush interval. If the server experiences a sudden kernel panic or power failure, up to 64 KB of unwritten log data residing in RAM may be lost.
</details>

<details>
<summary><strong>3. Why should you always use `escape=json` when declaring a JSON `log_format`?</strong></summary>

**Answer**:
In HTTP requests, client-supplied headers (such as `User-Agent`, `Referer`, or URI query parameters) can contain arbitrary characters, including double quotes (`"`), newlines (`\n`), backslashes (`\`), and control characters.

If you construct a JSON string without `escape=json`:
An attacker sending `User-Agent: "test", "injected": "payload"` will generate a syntactically invalid or hijacked JSON log entry. Downstream log ingestion pipelines (Logstash, Fluentd, Vector) will crash or fail to parse the malformed JSON. `escape=json` ensures all characters are properly escaped in accordance with the JSON specification (RFC 8259).
</details>

<details>
<summary><strong>4. How can you completely suppress access logs for specific locations (like `/healthz`) without disabling error logging?</strong></summary>

**Answer**:
Inside the target location, set:
```nginx
location /healthz {
    access_log off;
    # Optional: suppress 404 logging if probing missing assets
    log_not_found off;
    return 200 "healthy\n";
}
```
Setting `access_log off;` disables request logging for that location, but leaves `error_log` completely operational. If an internal error occurs (such as a 500 error or permission failure), it will still be recorded in `error.log`.
</details>

<details>
<summary><strong>5. How does NGINX handle multiple `upstream` servers when logging `$upstream_response_time`?</strong></summary>

**Answer**:
If NGINX attempts failover across multiple upstream servers (for instance, the first server returns a 502 and NGINX retries a second server via `proxy_next_upstream`), the `$upstream_response_time` variable stores a **comma-separated list of values**:
`0.050, 0.120`
The first value represents the time spent on the failed upstream; the second represents the time spent on the successful backup upstream. If the request was served directly from cache, the value is recorded as `-`.
</details>

<details>
<summary><strong>6. What signal must be sent to NGINX during log rotation, and what does the master process do upon receiving it?</strong></summary>

**Answer**:
The administrator or `logrotate` daemon sends the **`SIGUSR1`** signal to the master process:
```bash
kill -USR1 $(cat /run/nginx.pid)
```
Upon receiving `SIGUSR1`:
1. The master process re-opens all files specified in `access_log` and `error_log` directives using their configured file paths.
2. It closes the previous file descriptors and passes the newly opened file descriptors to all active worker processes.
3. The worker processes seamlessly start writing subsequent log lines to the new files without dropping any in-flight logs.
</details>

<details>
<summary><strong>7. What happens if the filesystem holding `/var/log/nginx` runs out of disk space?</strong></summary>

**Answer**:
When disk space is exhausted:
1. NGINX worker `write()` system calls fail with `ENOSPC (No space left on device)`.
2. NGINX will continue attempting to serve HTTP requests, but will log errors to the console or syslog if available.
3. If proxy buffering is enabled, client file uploads and large upstream responses that exceed memory buffers will fail because temporary disk buffer files cannot be created.
4. The system may lock up as OS daemons fail to write PID and state files.

*Remediation*: Mount `/var/log` on a separate disk volume, enable log rotation with compression, and implement buffered logging with alerts at 80% disk capacity.
</details>

<details>
<summary><strong>8. Can NGINX stream logs directly to standard output (`/dev/stdout`) and standard error (`/dev/stderr`)? When is this used?</strong></summary>

**Answer**:
Yes:
```nginx
access_log /dev/stdout main;
error_log /dev/stderr warn;
```
This is the standard architectural pattern for **Docker containers and Kubernetes pods**. In containerized environments, processes should not manage internal log files on local container storage. Streaming logs to stdout and stderr allows the container runtime (Docker daemon, containerd) to capture the streams and forward them to Kubernetes logging drivers (FluentBit, Promtail).
</details>

---

## 11. Practice Exercises

<details>
<summary><strong>Exercise 1 (Easy): Create a JSON Log Format</strong></summary>

**Task**: Author an NGINX `log_format` named `simple_json` inside the `http {}` context that records client IP, request line, and HTTP status code in valid JSON.

**Solution**:
```nginx
http {
    log_format simple_json escape=json
      '{"ip":"$remote_addr","request":"$request","status":$status}';
}
```
</details>

<details>
<summary><strong>Exercise 2 (Easy): Disable Access Logs on Favicon & Robots</strong></summary>

**Task**: Write the location blocks to disable access logs and suppress missing file error logs for `/favicon.ico` and `/robots.txt`.

**Solution**:
```nginx
location = /favicon.ico {
    access_log off;
    log_not_found off;
}

location = /robots.txt {
    access_log off;
    log_not_found off;
}
```
</details>

<details>
<summary><strong>Exercise 3 (Medium): Implement Conditional Error-Only Logging</strong></summary>

**Task**: Configure NGINX using a `map` block so that access logs are recorded ONLY when the HTTP status code is 4xx or 5xx, completely ignoring 2xx and 3xx responses.

**Solution**:
```nginx
http {
    map $status $log_errors_only {
        ~^[23]   0; # Ignore 2xx and 3xx
        default  1; # Log 4xx and 5xx
    }

    server {
        listen 80;
        server_name app.internal;
        access_log /var/log/nginx/errors_access.log combined if=$log_errors_only;
    }
}
```
</details>

<details>
<summary><strong>Exercise 4 (Medium): Configure High-Throughput Buffered Logging</strong></summary>

**Task**: Apply buffered logging to an access log with a 128 KB buffer and a 15-second flush interval.

**Solution**:
```nginx
access_log /var/log/nginx/high_perf.log combined buffer=128k flush=15s;
```
</details>

<details>
<summary><strong>Exercise 5 (Hard): Anonymize Client IP Addresses for GDPR Compliance</strong></summary>

**Task**: Use NGINX regex mapping to mask the last octet of IPv4 client addresses (e.g., convert `192.168.1.50` to `192.168.1.0`) before writing to the access log.

**Solution**:
```nginx
http {
    # Extract subnet and replace host octet with zero
    map $remote_addr $anonymized_ip {
        ~^(?P<subnet>\d+\.\d+\.\d+)\.\d+$  "$subnet.0";
        ~^(?P<subnet>[0-9a-fA-F:]+):[0-9a-fA-F]+$ "$subnet:0000";
        default                             "0.0.0.0";
    }

    log_format gdpr_format '$anonymized_ip - [$time_local] "$request" $status';

    server {
        listen 80;
        access_log /var/log/nginx/gdpr.log gdpr_format;
    }
}
```
</details>

<details>
<summary><strong>Exercise 6 (Hard): Log Upstream Latency Distribution</strong></summary>

**Task**: Configure a JSON log format that captures `$request_time`, `$upstream_connect_time`, `$upstream_header_time`, and `$upstream_response_time`, paired with a python script to parse the output and compute average upstream latency.

**Solution**:
```nginx
# 1. NGINX Configuration
log_format perf_metrics escape=json '{'
    '"req_time":$request_time,'
    '"up_conn":"$upstream_connect_time",'
    '"up_resp":"$upstream_response_time"'
'}';

server {
    listen 80;
    access_log /var/log/nginx/perf.json perf_metrics buffer=16k flush=5s;
}
```

```python
# 2. Python Latency Profiler
import json, sys

total_req, total_up = 0.0, 0.0
count = 0

with open("/var/log/nginx/perf.json") as f:
    for line in f:
        data = json.loads(line)
        try:
            total_req += float(data["req_time"])
            if data["up_resp"] != "-":
                total_up += float(data["up_resp"].split(",")[0])
            count += 1
        except (ValueError, KeyError):
            continue

if count > 0:
    print(f"Total Requests: {count}")
    print(f"Avg Request Time: {total_req/count:.4f}s")
    print(f"Avg Upstream Response Time: {total_up/count:.4f}s")
```
</details>

---

## 12. Summary & Quick Reference Cheat Sheet

| Directive / Parameter | Example | Purpose |
| :--- | :--- | :--- |
| **`log_format`** | `log_format my_fmt escape=json '{...}';` | Defines custom log format schema |
| **`access_log`** | `access_log /path my_fmt;` | Enables access logging with format |
| **`buffer=`** | `access_log /path fmt buffer=64k;` | Batches log writes in memory |
| **`flush=`** | `access_log /path fmt flush=5s;` | Flushes buffer after timer expires |
| **`if=`** | `access_log /path fmt if=$is_loggable;` | Conditionally logs based on variable |
| **`error_log`** | `error_log /path warn;` | Configures error logging severity |
| **`$request_time`** | Variable in `log_format` | Full client-to-client transaction time |
| **`$upstream_response_time`**| Variable in `log_format` | Backend upstream transaction time |

---

## 13. Further Reading & Official Documentation

- [Official NGINX Documentation: ngx_http_log_module](https://nginx.org/en/docs/http/ngx_http_log_module.html)
- [Official NGINX Documentation: Core Error Log Directive](https://nginx.org/en/docs/ngx_core_module.html#error_log)
- [Official NGINX Documentation: Using NGINX Logging for Performance Profiling](https://www.nginx.com/blog/using-nginx-logging-for-application-performance-monitoring/)
- [Grafana Loki Documentation: Ingesting NGINX JSON Logs](https://grafana.com/docs/loki/latest/)
- [Syslog Protocol RFC 5424](https://www.rfc-editor.org/rfc/rfc5424.html)

---

[Back to Module Overview](./README.md) | [02-reverse-proxy.md ->](./02-reverse-proxy.md)
