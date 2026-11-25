---
title: "Upstream Load Balancing in NGINX"
description: "Mastering NGINX upstream load balancing algorithms (Round Robin, Least Connections, IP Hash, Consistent Hashing), keepalive connection pooling, dynamic DNS, and failover."
module: "03-traffic-management"
track: "devops-nginx"
order: 2
difficulty: "Intermediate to Advanced"
prerequisites:
  - "01-fundamentals/03-server-and-location"
  - "02-core-features/02-reverse-proxy"
tags:
  - nginx
  - load-balancing
  - upstream
  - least-conn
  - ip-hash
  - consistent-hashing
  - keepalive
last_updated: "2026-03-30"
---

# Upstream Load Balancing in NGINX

## Table of Contents
- [1. What & Why](#1-what--why)
- [2. Core Concepts](#2-core-concepts)
- [3. How It Works Internally](#3-how-it-works-internally)
- [4. Syntax & Directives Reference](#4-syntax--directives-reference)
- [5. Practical Implementation Examples](#5-practical-implementation-examples)
  - [Example 1: Weighted Round-Robin with Passive Health Checks](#example-1-weighted-round-robin-with-passive-health-checks)
  - [Example 2: Least Connections with Dedicated Backup and Down Nodes](#example-2-least-connections-with-dedicated-backup-and-down-nodes)
  - [Example 3: Ketama Consistent Hashing (`hash ... consistent`)](#example-3-ketama-consistent-hashing-hash--consistent)
  - [Example 4: Upstream Persistent Connection Pooling (`keepalive`)](#example-4-upstream-persistent-connection-pooling-keepalive)
  - [Example 5: Dynamic DNS Resolution in Cloud Auto-Scaling Upstreams](#example-5-dynamic-dns-resolution-in-cloud-auto-scaling-upstreams)
- [6. Production Best Practices](#6-production-best-practices)
- [7. Common Pitfalls & Anti-Patterns](#7-common-pitfalls--anti-patterns)
- [8. Performance, Security & Troubleshooting](#8-performance-security--troubleshooting)
- [9. Architectural Comparison Table](#9-architectural-comparison-table)
- [10. Technical Interview Questions](#10-technical-interview-questions)
- [11. Hands-On Practice Exercises](#11-hands-on-practice-exercises)
- [12. Summary & Quick Reference](#12-summary--quick-reference)
- [13. Further Reading & Official Documentation](#13-further-reading--official-documentation)

---

## 1. What & Why

In scalable backend architectures, no single application server instance can handle all incoming traffic or survive unexpected hardware failures. High-availability systems distribute client requests across horizontal pools of application instances.

NGINX serves as an ultra-high-throughput **Application Load Balancer (Layer 7 Reverse Proxy)**. Operating directly between public clients and backend clusters, NGINX:
1. **Evenly Distributes Load**: Distributes millions of concurrent requests across backend pools using sophisticated algorithms (Round Robin, Least Connections, Consistent Hashing).
2. **Eliminates Backend TCP Handshake Overhead**: Reuses established persistent connections to backend servers via upstream connection pooling (`keepalive`), slashing CPU usage and latency.
3. **Provides Automated Failover**: Detects unresponsive or crashed backend nodes in real time, automatically routing traffic to healthy standby nodes without user disruption.
4. **Enables Zero-Downtime Deployments**: Facilitates gradual rolling deployments, blue-green switches, and maintenance isolation via server state parameters (`down`, `backup`).

---

## 2. Core Concepts

### The Load Balancing Algorithms
NGINX supports multiple load-balancing algorithms within the `upstream {}` block:

1. **Round Robin (Default)**:
   - Requests are distributed sequentially across servers in the pool.
   - Supports weighting (`weight=N`): a server with `weight=3` receives three times as many requests as a server with `weight=1`.

2. **Least Connections (`least_conn`)**:
   - Requests are routed to the server with the lowest number of active client connections.
   - Ideal for long-running transactions, file uploads, WebSocket pools, or applications with unpredictable request processing times.

3. **IP Hash (`ip_hash`)**:
   - Computes a hash of the first three octets of the client's IPv4 address (or full IPv6 address).
   - Guarantees that requests from the same client IP always reach the same backend server (session stickiness) as long as that server is available.

4. **Generic Hash & Consistent Hashing (`hash <key> [consistent]`)**:
   - Routes requests based on an arbitrary string key (e.g., `$request_uri`, `$arg_user_id`, `$http_authorization`).
   - Adding the `consistent` parameter enables **Ketama Consistent Hashing**: when a server is added or removed from the upstream pool, only a tiny fraction of keys are remapped, preserving cache hit ratios across upstream caching proxies.

5. **Random with Two Choices (`random [two [method]]`)**:
   - Selects two servers at random and picks the one with fewer connections (Power of Two Random Choices algorithm). Avoids stampeding the least-connected server under high concurrency.

### Server States and Passive Health Checks
Open-source NGINX uses **passive health checks**:
- `max_fails=number`: Number of failed communication attempts within `fail_timeout` before NGINX considers the server unavailable. (Default: `1`).
- `fail_timeout=time`: The duration during which `max_fails` errors must occur to mark the server down, and the duration the server is kept offline before NGINX tests it again. (Default: `10s`).
- `backup`: Marks the server as a standby fallback. It receives zero traffic unless all primary servers fail.
- `down`: Marks the server as permanently offline. Useful for draining connections before maintenance.
- `max_conns=number`: Caps the simultaneous active connections allowed on that backend node to prevent overloading.

### Upstream Connection Pooling (`keepalive`)
By default, NGINX closes its TCP connection to the upstream server after completing each HTTP request (`Connection: close`). Under high traffic (10,000 req/sec), repeatedly opening and closing TCP sockets exhausts Linux ephemeral ports (`TIME_WAIT` socket exhaustion) and adds 1-RTT latency per request. 

The `keepalive` directive maintains a pool of pre-warmed idle TCP connections to the backend, reusing them across subsequent client requests.

---

## 3. How It Works Internally

### Upstream Request Dispatch and Fallback Flow

```
Client Request arrives at NGINX
               |
               v
Select Upstream Server based on Algorithm
(e.g., Least Connections or Weighted Round-Robin)
               |
               +-----------------------------+
               | Selected Node: 10.0.1.10    |
               +--------------+--------------+
                              |
                     Attempt TCP Connect
                             / \
                       OK   /   \ FAILED (Timeout / RST)
                           /     \
                          /       v
                         /   Increment fails counter on 10.0.1.10
                        /    Is fails >= max_fails within fail_timeout?
                       /         /         \
                      /        YES          NO
                     /          |            |
                    /     Mark 10.0.1.10     |
                   /      DOWN for timeout   |
                  /             |            |
                 /              +-----+------+
                /                     |
               /                      v
              /           Select NEXT Healthy Node in Pool
             /            (via proxy_next_upstream)
            /                         |
+----------v---------------+   +------v-------------------+
| Send HTTP Request Body   |   | Selected Node: 10.0.1.11 |
| Stream response to client|   +--------------------------+
+--------------------------+
```

---

## 4. Syntax & Directives Reference

| Directive | Context | Default | Purpose / Details |
| :--- | :--- | :--- | :--- |
| `upstream name { ... }` | `http` | None | Declares a named pool of upstream backend servers. |
| `server address [parameters]` | `upstream` | None | Defines backend IP, hostname, or Unix socket path with tuning flags. |
| `least_conn` | `upstream` | None | Enables Least Connections load-balancing algorithm. |
| `ip_hash` | `upstream` | None | Enables IP Hash load-balancing algorithm. |
| `hash key [consistent]` | `upstream` | None | Enables generic or Ketama consistent hashing based on a variable key. |
| `random [two [method]]` | `upstream` | None | Enables random load balancing (optionally choosing between two). |
| `keepalive connections` | `upstream` | None | Sets maximum number of idle keepalive connections per worker in pool. |
| `keepalive_requests number`| `upstream` | `1000` (1.19+) | Maximum number of requests handled over a single keepalive connection. |
| `keepalive_time time` | `upstream` | `1h` | Maximum time a persistent keepalive connection may remain open. |
| `keepalive_timeout time` | `upstream` | `60s` | Idle timeout for keepalive connections to upstream servers. |
| `proxy_next_upstream condition ...` | `http`, `server`, `location` | `error timeout` | Conditions under which NGINX retries the request on the next server. |
| `proxy_next_upstream_timeout time` | `http`, `server`, `location` | `0` (No limit) | Total time allowed for finding a responsive upstream server. |
| `proxy_next_upstream_tries number` | `http`, `server`, `location` | `0` (No limit) | Maximum number of upstream servers to try before returning error. |

---

## 5. Practical Implementation Examples

### Example 1: Weighted Round-Robin with Passive Health Checks

Distributing traffic proportionally across machines with different CPU capacities, with passive health check parameters.

```nginx
# /etc/nginx/conf.d/weighted_upstream.conf

upstream backend_pool {
    # Server 1: Heavyweight bare-metal server (Receives 50% of traffic)
    server 10.0.1.10:8000 weight=5 max_fails=3 fail_timeout=15s;

    # Server 2: Standard cloud VM (Receives 30% of traffic)
    server 10.0.1.11:8000 weight=3 max_fails=3 fail_timeout=15s;

    # Server 3: Small cloud VM (Receives 20% of traffic)
    server 10.0.1.12:8000 weight=2 max_fails=3 fail_timeout=15s;

    # Keepalive pool
    keepalive 32;
}

server {
    listen 80;
    server_name app.example.com;

    location / {
        proxy_pass http://backend_pool;
        proxy_set_header Host $host;
        proxy_set_header X-Real-IP $remote_addr;

        # Retry on next server if backend fails or returns 502/503/504
        proxy_next_upstream error timeout http_502 http_503 http_504;
        proxy_next_upstream_tries 3;
        proxy_next_upstream_timeout 10s;

        # Keepalive mandatory headers
        proxy_http_version 1.1;
        proxy_set_header Connection "";
    }
}
```

### Example 2: Least Connections with Dedicated Backup and Down Nodes

Directs traffic to instances with the fewest active connections, maintains an emergency backup server, and isolates a node for maintenance.

```nginx
# /etc/nginx/conf.d/least_conn_cluster.conf

upstream processing_nodes {
    least_conn;

    # Active Primary Nodes (Capped at 500 connections each)
    server 10.0.2.10:8080 max_conns=500 max_fails=2 fail_timeout=10s;
    server 10.0.2.11:8080 max_conns=500 max_fails=2 fail_timeout=10s;
    server 10.0.2.12:8080 max_conns=500 max_fails=2 fail_timeout=10s;

    # Emergency Standby Server (Only receives traffic if ALL primaries fail)
    server 10.0.2.99:8080 backup;

    # Maintenance Node (Zero traffic sent; drained cleanly)
    server 10.0.2.13:8080 down;
}

server {
    listen 80;
    server_name jobs.example.com;

    location / {
        proxy_pass http://processing_nodes;
        proxy_set_header Host $host;
        proxy_connect_timeout 3s;
        proxy_read_timeout 60s;
    }
}
```

### Example 3: Ketama Consistent Hashing (`hash ... consistent`)

Maintains high cache hit ratios by routing requests for the same URI or user ID to the same backend caching node.

```nginx
# /etc/nginx/conf.d/consistent_hashing.conf

upstream cache_cluster {
    # Hash on request URI using Ketama algorithm
    hash $request_uri consistent;

    server 10.0.3.10:8080;
    server 10.0.3.11:8080;
    server 10.0.3.12:8080;
    server 10.0.3.13:8080;
}

server {
    listen 80;
    server_name images.example.com;

    location / {
        proxy_pass http://cache_cluster;
        proxy_set_header Host $host;
    }
}
```

### Example 4: Upstream Persistent Connection Pooling (`keepalive`)

The gold-standard enterprise keepalive configuration that prevents Linux ephemeral port exhaustion and TCP handshake latency.

```nginx
# /etc/nginx/conf.d/keepalive_pool.conf

upstream api_workers {
    server 127.0.0.1:5001;
    server 127.0.0.1:5002;
    server 127.0.0.1:5003;
    server 127.0.0.1:5004;

    # Keep up to 64 idle connections open PER NGINX WORKER PROCESS
    keepalive 64;

    # Rotate keepalive sockets after 5,000 requests to avoid memory bloat
    keepalive_requests 5000;
    keepalive_timeout 60s;
}

server {
    listen 80;
    server_name api.example.com;

    location / {
        proxy_pass http://api_workers;

        # MANDATORY FOR KEEPALIVE TO WORK:
        # 1. Switch to HTTP/1.1 (HTTP/1.0 closes connections by default)
        proxy_http_version 1.1;

        # 2. Clear Connection header (removes default "Connection: close")
        proxy_set_header Connection "";

        proxy_set_header Host $host;
        proxy_set_header X-Forwarded-For $proxy_add_x_forwarded_for;
    }
}
```

### Example 5: Dynamic DNS Resolution in Cloud Auto-Scaling Upstreams

In AWS ECS, Kubernetes, or Docker Swarm, backend IP addresses change frequently as containers scale. Standard NGINX resolves domain names in `upstream {}` **only once at startup**. This example uses a DNS `resolver` and a variable in `proxy_pass` to re-resolve hostnames dynamically.

```nginx
# /etc/nginx/conf.d/dynamic_dns.conf

server {
    listen 80;
    server_name cloud.example.com;

    # AWS Internal VPC DNS Resolver (10.0.0.2 / 169.254.169.253)
    # Valid for 10 seconds; drops IPv6 lookups
    resolver 10.0.0.2 valid=10s ipv6=off;
    resolver_timeout 3s;

    # Crucial: Setting the upstream target in a variable forces
    # NGINX to re-resolve the domain name at runtime using the resolver!
    set $dynamic_backend "http://ecs-service.internal.corp:8080";

    location / {
        proxy_pass $dynamic_backend;
        proxy_set_header Host $host;
        proxy_set_header X-Real-IP $remote_addr;
    }
}
```

---

## 6. Production Best Practices

1. **Always Configure Keepalive Pools**: In any high-throughput reverse proxy setup, always declare `keepalive N;` in `upstream {}`, paired with `proxy_http_version 1.1;` and `proxy_set_header Connection "";`.
2. **Configure `proxy_next_upstream` Carefully**: By default, NGINX retries on `error` and `timeout`. If a request is non-idempotent (e.g., `POST /payments`), retrying on a timeout could result in duplicate charges. Explicitly configure `proxy_next_upstream error timeout non_idempotent;` only when the backend supports idempotent transactions.
3. **Always Set `proxy_next_upstream_tries` and `timeout`**: Prevent requests from cascading indefinitely through dead servers by capping retry attempts (`proxy_next_upstream_tries 3;` and `proxy_next_upstream_timeout 5s;`).
4. **Use Unix Domain Sockets for Local Upstreams**: If NGINX and the backend application run on the same physical host or container (e.g., NGINX + Gunicorn / PHP-FPM), use Unix domain sockets (`server unix:/run/app.sock;`). They bypass the TCP/IP stack completely, saving CPU and avoiding port exhaustion.
5. **Set `max_conns` to Prevent Overload**: If your application runtime has a known concurrency ceiling (e.g., Node.js single thread saturates at 200 concurrent requests), configure `max_conns=200` to prevent NGINX from queuing excess requests on saturated instances.
6. **Use Variable-Based `proxy_pass` in Cloud Environments**: When targeting dynamic cloud endpoints (AWS ALB, Kubernetes services), set a variable (`set $upstream "http://...";`) alongside `resolver` to prevent stale IP resolution outages.

---

## 7. Common Pitfalls & Anti-Patterns

| Anti-Pattern | Why It Fails | Correct Approach |
| :--- | :--- | :--- |
| **Omitting `proxy_set_header Connection ""` with keepalive** | NGINX sends `Connection: close` by default; upstream backend closes the socket immediately; keepalive pool remains empty. | Always pair `keepalive` with `proxy_http_version 1.1;` and `proxy_set_header Connection "";`. |
| **Using static hostname in `upstream` with dynamic cloud IPs** | NGINX resolves the hostname once during startup; when auto-scaling replaces containers, NGINX routes traffic to dead IPs, returning 502s. | Use `resolver` with variable `set $target "http://service.local"; proxy_pass $target;`. |
| **Retrying `POST` requests without idempotency** | If a backend processes a payment but drops the connection before sending HTTP 200, NGINX retries on the next server, executing a double charge. | Avoid adding `non_idempotent` to `proxy_next_upstream` on transactional routes. |
| **Setting `keepalive` number too high** | Causes NGINX to hold thousands of idle sockets open to backends, exhausting backend database or worker thread pools. | Set `keepalive` to an optimal ratio (e.g., 32 to 128 connections per worker). |
| **Using `ip_hash` behind a CDN / Cloudflare** | All client traffic originates from a few CDN edge IPs; `ip_hash` routes 100% of traffic to a single backend node, overloading it. | Use `hash $http_cf_connecting_ip consistent;` or `hash $http_x_forwarded_for consistent;`. |
| **Forgetting `backup` flag syntax** | Putting `backup` on a server with `ip_hash` or `least_conn` in older versions caused syntax errors. | In modern NGINX, `backup` works with standard round-robin and least connections. |

---

## 8. Performance, Security & Troubleshooting

### Diagnostic CLI Commands

```bash
# 1. Inspect Upstream Response Headers and Server Assignment
curl -s -i http://app.example.com/ | grep -E -i "x-upstream|http/"

# 2. Monitor Active Persistent Connections to Upstreams
ss -s
ss -t -a | grep -E "5001|5002|5003" | awk '{print $1}' | sort | uniq -c

# 3. Check for Upstream Connection Failures in Error Log
tail -f /var/log/nginx/error.log | grep -E "no live upstreams|connect\(\) failed|upstream timed out"

# 4. Benchmark Throughput with Keepalive vs Without Keepalive
# (Observe dramatic reduction in TIME_WAIT sockets with keepalive enabled)
ab -n 10000 -c 100 http://app.example.com/
```

### Keepalive Sizing Formula
To determine the optimal `keepalive` pool size:
$$\text{Keepalive Sockets} = \frac{\text{Peak Requests Per Second} \times \text{Average Upstream Response Time (seconds)}}{\text{Number of NGINX Worker Processes}}$$
*Example:* 10,000 req/sec with 0.02s response time across 4 workers:
$$\frac{10000 \times 0.02}{4} = \frac{200}{4} = 50\text{ keepalive connections per worker.}$$

---

## 9. Architectural Comparison Table

| Algorithm | Distribution Logic | Session Stickiness | Failover Handling | Best Use Case |
| :--- | :--- | :--- | :--- | :--- |
| **Round Robin** | Sequential with weights | No | Automatic | Uniform stateless APIs |
| **Least Connections** | Fewest active sockets | No | Automatic | Long requests, WebSockets, file uploads |
| **IP Hash** | Client IP first 3 octets | **Yes** (by IP) | Automatic | Stateful legacy session applications |
| **Generic Consistent Hash**| Ketama hash ring on key | **Yes** (by Key) | Minimal key churn | Upstream caching layers (Varnish, Redis) |
| **Random (Two Choices)** | Compares 2 random nodes | No | Automatic | Ultra-high concurrency microservices |

---

## 10. Technical Interview Questions

<details>
<summary><strong>Q1: Why does upstream keepalive fail to work if you only declare keepalive 32 in the upstream block?</strong></summary>

**Answer:**
Declaring `keepalive 32;` inside the `upstream {}` block only allocates the memory buffer for 32 idle connections. By default, NGINX's reverse proxy module forwards requests to upstream servers using **HTTP/1.0** and automatically sets the header:
```http
Connection: close
```
When the upstream server finishes sending the response, it obeys the `Connection: close` header and immediately terminates the underlying TCP socket. As a result, the keepalive pool remains empty.

To make keepalive work, you must configure two additional directives inside the `location` block:
```nginx
proxy_http_version 1.1;
proxy_set_header Connection "";
```
This forces HTTP/1.1 and removes the `Connection: close` header, allowing the upstream backend to keep the socket open and return it to NGINX's idle keepalive pool.
</details>

<details>
<summary><strong>Q2: What is the difference between standard Round Robin and Least Connections, and when should you choose Least Connections?</strong></summary>

**Answer:**
- **Round Robin**: Distributes incoming requests sequentially (or proportionally by weight) across all available servers without considering how long each server takes to process requests.
- **Least Connections (`least_conn`)**: Dynamically monitors the number of active, in-flight TCP connections on each backend server and routes each new request to the server with the fewest active connections.

**When to choose Least Connections**: Round Robin works well when all requests have uniform processing times (e.g., static assets or fast Redis reads). However, if your application processes heterogeneous workloads (e.g., some requests take 5ms for a status check, while others take 3,000ms for PDF report generation or video encoding), Round Robin will pile new requests onto a server already saturated by slow jobs. Least Connections prevents server saturation by diverting traffic away from busy instances.
</details>

<details>
<summary><strong>Q3: Explain how Ketama Consistent Hashing works and why it is superior to a simple modulo hash.</strong></summary>

**Answer:**
In a simple modulo hash (`server_index = hash(key) % N` where N is the number of servers):
If you have 4 cache servers and 1 server fails (N changes from 4 to 3), the modulo value changes for almost **100% of all keys**. This triggers a massive, simultaneous cache wipeout across the entire cluster, crashing backend databases.

**Ketama Consistent Hashing** (`hash $request_uri consistent;`) maps both the servers and the request keys onto a virtual 360-degree circular hash ring (continuum):
1. Keys are mapped clockwise to the nearest server on the ring.
2. If a server is added or removed, **only the keys that were mapped directly to that specific server are reallocated** (an average of $1/N$ of keys).
3. The remaining $N-1$ servers keep 100% of their existing cached keys intact, preventing cache stampedes.
</details>

<details>
<summary><strong>Q4: Why does open-source NGINX fail to resolve dynamic IP changes in cloud auto-scaling groups, and how do you fix it?</strong></summary>

**Answer:**
When an upstream server is declared with a domain name in a standard `upstream {}` block:
```nginx
upstream backend {
    server my-ecs-alb.amazonaws.com;
}
```
Open-source NGINX resolves the DNS hostname **only once at startup or reload**. In cloud environments (AWS ALB, ECS, Kubernetes), IP addresses change frequently as containers scale and migrate. When an IP changes, NGINX continues sending traffic to the old, terminated IP address, causing `502 Bad Gateway` errors until NGINX is manually reloaded.

**The Fix**:
Assign the target URL to a variable and configure a DNS `resolver`:
```nginx
resolver 10.0.0.2 valid=10s;
set $backend "http://my-ecs-alb.amazonaws.com";
location / {
    proxy_pass $backend;
}
```
When `proxy_pass` targets a variable, NGINX forces a runtime DNS lookup using the specified `resolver`, honoring the `valid` TTL.
</details>

<details>
<summary><strong>Q5: What are the risks of including non_idempotent in proxy_next_upstream?</strong></summary>

**Answer:**
Under RFC 9110, HTTP `GET`, `HEAD`, `PUT`, and `DELETE` methods are defined as idempotent (executing them multiple times produces the same outcome). However, `POST` and `LOCK` requests are **non-idempotent** (e.g., submitting an order or executing a credit card charge).

If `non_idempotent` is enabled in `proxy_next_upstream` and a backend processes a credit card payment but experiences a network glitch before transmitting the final HTTP 200 response, NGINX will treat the timeout as an error and retry the exact same `POST` request on a second backend server. This results in the customer being billed twice. Therefore, `non_idempotent` should only be enabled if your application implements cryptographic idempotency keys.
</details>

<details>
<summary><strong>Q6: What is the purpose of the backup parameter in the server directive?</strong></summary>

**Answer:**
The `backup` parameter marks a server as an emergency standby node. As long as at least one primary server in the upstream pool is healthy and reachable, the `backup` server receives zero requests. 

If all primary servers fail (exceeding `max_fails`), NGINX automatically diverts 100% of incoming traffic to the `backup` servers. Once a primary server recovers and passes its `fail_timeout` probation, NGINX automatically shifts traffic back to the primary and resumes standby mode on the backup.
</details>

<details>
<summary><strong>Q7: How does ip_hash behave when deployed behind a CDN like Cloudflare or Akamai?</strong></summary>

**Answer:**
The `ip_hash` directive computes its hash using the source IP of the direct TCP connection (`$remote_addr`). When NGINX is placed behind a CDN or reverse proxy, all incoming TCP connections originate from a small pool of CDN edge IP addresses rather than the real end users. 

Consequently, `ip_hash` hashes the CDN's IP, directing all requests from thousands of unique users to a single backend node, severely overloading it while leaving other backend servers idle. 

To fix this behind a CDN, do not use `ip_hash`. Instead, use generic consistent hashing on the real client IP header:
```nginx
hash $http_cf_connecting_ip consistent;
# Or: hash $http_x_forwarded_for consistent;
```
</details>

<details>
<summary><strong>Q8: What is the difference between max_fails and max_conns?</strong></summary>

**Answer:**
- `max_fails`: Governs **health checks**. It specifies the maximum number of consecutive unsuccessful connection attempts or timeouts that can occur within `fail_timeout` before NGINX marks the server as offline.
- `max_conns`: Governs **capacity capping**. It sets the absolute maximum number of simultaneous, concurrent active connections NGINX is allowed to open to that specific backend server. If the limit is reached, NGINX queues incoming requests or rejects them to prevent server crashes.
</details>

---

## 11. Hands-On Practice Exercises

<details>
<summary><strong>Exercise 1: Configure Weighted Round-Robin Across Two Local Ports</strong></summary>

**Goal:** Configure an upstream pool balancing between `127.0.0.1:8001` (weight 2) and `127.0.0.1:8002` (weight 1).

**Solution:**
```nginx
upstream local_cluster {
    server 127.0.0.1:8001 weight=2;
    server 127.0.0.1:8002 weight=1;
}

server {
    listen 80;
    server_name balance.local;

    location / {
        proxy_pass http://local_cluster;
        proxy_set_header Host $host;
    }
}
```
</details>

<details>
<summary><strong>Exercise 2: Setup Upstream Keepalive Connection Pooling</strong></summary>

**Goal:** Configure a 32-connection keepalive pool and verify that NGINX forwards requests using HTTP/1.1 with an empty Connection header.

**Solution:**
```nginx
upstream keepalive_cluster {
    server 127.0.0.1:9001;
    server 127.0.0.1:9002;
    keepalive 32;
}

server {
    listen 80;
    server_name keepalive.local;

    location / {
        proxy_pass http://keepalive_cluster;
        proxy_http_version 1.1;
        proxy_set_header Connection "";
        proxy_set_header Host $host;
    }
}
```
</details>

<details>
<summary><strong>Exercise 3: Configure Least Connections with a Standby Backup</strong></summary>

**Goal:** Implement Least Connections across two primary nodes, with a fallback node that only triggers if both primaries are down.

**Solution:**
```nginx
upstream worker_pool {
    least_conn;
    server 127.0.0.1:7001 max_fails=2 fail_timeout=5s;
    server 127.0.0.1:7002 max_fails=2 fail_timeout=5s;
    server 127.0.0.1:7003 backup;
}

server {
    listen 80;
    server_name workers.local;

    location / {
        proxy_pass http://worker_pool;
        proxy_next_upstream error timeout http_502;
    }
}
```
</details>

<details>
<summary><strong>Exercise 4: Implement Consistent Hashing on User-ID Header</strong></summary>

**Goal:** Route incoming requests with the same `X-User-ID` header to the same backend server using Ketama consistent hashing.

**Solution:**
```nginx
upstream user_sharded_cluster {
    hash $http_x_user_id consistent;
    server 127.0.0.1:8081;
    server 127.0.0.1:8082;
    server 127.0.0.1:8083;
}

server {
    listen 80;
    server_name sharded.local;

    location / {
        proxy_pass http://user_sharded_cluster;
        proxy_set_header Host $host;
    }
}
```
</details>

<details>
<summary><strong>Exercise 5: Configure Dynamic DNS Resolution for Auto-Scaled Upstreams</strong></summary>

**Goal:** Configure NGINX to re-resolve an upstream hostname every 5 seconds using Google DNS (`8.8.8.8`).

**Solution:**
```nginx
server {
    listen 80;
    server_name dyn.local;

    resolver 8.8.8.8 valid=5s;
    set $dynamic_target "http://api.external-service.net";

    location / {
        proxy_pass $dynamic_target;
        proxy_set_header Host $host;
    }
}
```
</details>

<details>
<summary><strong>Exercise 6: Test Graceful Draining of a Backend with the down Flag</strong></summary>

**Goal:** Mark an active node as `down` without reloading or dropping in-flight connections.

**Solution:**
1. In `upstream {}`: change `server 10.0.1.10:8000;` to `server 10.0.1.10:8000 down;`.
2. Reload NGINX: `nginx -t && nginx -s reload`.
3. Verify via access logs that zero new requests are sent to port 8000.
</details>

---

## 12. Summary & Quick Reference

```nginx
# Production Upstream Load Balancing Cheat Sheet

upstream backend_production {
    # Algorithm Selection (Default is Weighted Round-Robin)
    least_conn; # Or: ip_hash; Or: hash $request_uri consistent;

    # Backend Nodes
    server 10.0.1.10:8080 weight=3 max_conns=500 max_fails=3 fail_timeout=10s;
    server 10.0.1.11:8080 weight=2 max_conns=500 max_fails=3 fail_timeout=10s;

    # Standby Backup Node
    server 10.0.1.99:8080 backup;

    # Drained Maintenance Node
    server 10.0.1.12:8080 down;

    # Connection Pooling
    keepalive 64;
    keepalive_requests 5000;
    keepalive_timeout 60s;
}

server {
    location / {
        proxy_pass http://backend_production;

        # Keepalive Requirements
        proxy_http_version 1.1;
        proxy_set_header Connection "";

        # Resiliency & Retries
        proxy_next_upstream error timeout http_502 http_503 http_504;
        proxy_next_upstream_tries 3;
        proxy_next_upstream_timeout 5s;
    }
}
```

---

## 13. Further Reading & Official Documentation
- [Official NGINX ngx_http_upstream_module Documentation](https://nginx.org/en/docs/http/ngx_http_upstream_module.html)
- [Load Balancing with NGINX and NGINX Plus](https://docs.nginx.com/nginx/admin-guide/load-balancer/http-load-balancer/)
- [Using NGINX as an Application Gateway (F5 Technical Paper)](https://www.f5.com/company/blog/nginx/load-balancing-nginx)
- [Ketama Consistent Hashing Algorithm Overview](https://www.last.fm/user/RJ/journal/2007/04/10/rz_libketama_-_consistent_hashing_algo_for_memcache_clients)

---

[← Previous: HTTP Caching & Microcaching](01-caching.md) | [Module Overview](README.md) | [Next: Rate Limiting & Traffic Shaping →](03-rate-limiting.md)
