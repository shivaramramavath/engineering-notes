---
title: "Circuit Breaking and Upstream Health Checks in NGINX"
description: "Mastering passive health checks, the circuit breaker pattern, proxy_next_upstream failover, backup clusters, and active health check architectures in NGINX."
module: "03-traffic-management"
track: "devops-nginx"
order: 8
difficulty: "Advanced"
prerequisites:
  - "01-fundamentals/03-server-and-location"
  - "03-traffic-management/01-caching"
  - "03-traffic-management/02-load-balancing"
tags:
  - nginx
  - circuit-breaker
  - health-checks
  - proxy-next-upstream
  - failover
  - high-availability
  - reliability
last_updated: "2026-03-30"
---

# Circuit Breaking and Upstream Health Checks in NGINX

## Table of Contents
- [1. What & Why](#1-what--why)
- [2. Core Concepts](#2-core-concepts)
- [3. How It Works Internally](#3-how-it-works-internally)
- [4. Syntax & Directives Reference](#4-syntax--directives-reference)
- [5. Practical Implementation Examples](#5-practical-implementation-examples)
  - [Example 1: The Canonical Passive Circuit Breaker Configuration](#example-1-the-canonical-passive-circuit-breaker-configuration)
  - [Example 2: Multi-Tiered Failover with Standby Backup Upstreams](#example-2-multi-tiered-failover-with-standby-backup-upstreams)
  - [Example 3: Graceful Degradation via Stale Cache on Circuit Trip](#example-3-graceful-degradation-via-stale-cache-on-circuit-trip)
  - [Example 4: Capping Retries to Prevent Cascading Failures](#example-4-capping-retries-to-prevent-cascading-failures)
  - [Example 5: Emulating Active Probing in Open-Source NGINX via njs](#example-5-emulating-active-probing-in-open-source-nginx-via-njs)
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

In distributed microservices, services fail in unpredictable ways: memory leaks cause gradual thread lockup, databases run out of connections, networks experience transient packet loss, and nodes crash abruptly. 

If a reverse proxy continues to route incoming client traffic to a failing or dead backend server:
1. **User Requests Fail Repeatedly**: Clients experience repeated timeouts and 502 Bad Gateway errors.
2. **Cascading Failure (The Domino Effect)**: As dead nodes fail, retry loops pile excess traffic onto surviving nodes, quickly overloading them and taking down the entire cluster.
3. **Slow Latency Traps**: Clients hang for 60 seconds waiting for connection timeouts before failing.

The **Circuit Breaker pattern** (formalized by Michael Nygard in *Release It!*) protects distributed systems by automatically detecting failures and isolating failing nodes:
- When a service is healthy, the circuit is **CLOSED** (traffic flows normally).
- When failures exceed a threshold, the circuit **TRIPS (OPENS)**: traffic is immediately diverted away from the failing node, giving it time to recover.
- After a recovery period, the circuit enters **HALF-OPEN**: a single test request is routed to test health; if successful, the circuit resets to **CLOSED**.

NGINX implements native, zero-overhead circuit breaking via **Passive Health Checks** (`max_fails` and `fail_timeout`) paired with intelligent retry policies (`proxy_next_upstream`).

---

## 2. Core Concepts

### Passive vs Active Health Checks
- **Passive Health Checks (Open-Source NGINX Standard)**:
  - NGINX does *not* send artificial synthetic probe requests to backends.
  - Instead, NGINX monitors **real live client traffic** passing through the proxy.
  - If a real client request to server A fails (TCP RST, connection timeout, or 502), NGINX increments server A's failure counter.
  - Zero synthetic network overhead; fully responsive to actual client-perceived failures.
- **Active Health Checks (Synthetic Probing)**:
  - NGINX periodically sends dedicated synthetic HTTP requests (e.g., `GET /healthz`) in the background.
  - Native to **NGINX Plus** (`health_check` directive); can be emulated in open-source NGINX via third-party modules or `njs` background timers.

### The Circuit Breaker Triad in NGINX
In NGINX, the circuit breaker pattern maps directly to three configuration parameters:
1. **Failure Threshold (`max_fails`)**: The number of unsuccessful attempts within `fail_timeout` that will cause the circuit to trip OPEN. (Default: `1`).
2. **Recovery Window (`fail_timeout`)**: The duration for which the circuit remains OPEN (the server is excluded from load balancing). After `fail_timeout` elapses, NGINX enters HALF-OPEN state and attempts to route exactly one client request to the node. (Default: `10s`).
3. **Failure Conditions (`proxy_next_upstream`)**: Specifies which HTTP status codes or socket conditions count as a failure (e.g., `error`, `timeout`, `http_502`, `http_503`).

### Standby Circuit Fallbacks (`backup`)
When all primary upstream servers trip their circuits and enter the OPEN state, NGINX needs an emergency fallback. The `backup` directive defines servers that receive zero traffic during normal operations, but immediately activate when all primary nodes are down, serving a degraded read-only UI or maintenance notice.

---

## 3. How It Works Internally

### The Circuit Breaker State Machine

```
               +---------------------------------------------+
               |                 CLOSED                      |
               | - Server is HEALTHY                         |
               | - All normal traffic routed here            |
               | - Consecutive failures < max_fails          |
               +----------------------+----------------------+
                                      |
                     Consecutive failures >= max_fails
                             within fail_timeout
                                      |
                                      v
               +---------------------------------------------+
               |                  OPEN                       |
               | - Server is DEAD / TRIPPED                  |
               | - 0% traffic sent to this node              |
               | - Traffic diverted to surviving/backup nodes|
               +----------------------+----------------------+
                                      |
                        Time >= fail_timeout elapses
                                      |
                                      v
               +---------------------------------------------+
               |                HALF-OPEN                    |
               | - NGINX routes ONE test probe request       |
               +----------------------+----------------------+
                                     / \
                        Test SUCCEEDS   Test FAILS
                            /             \
                           /               \
                          v                 v
               Reset to CLOSED           Remain OPEN for
               (Server fully recovered)  another fail_timeout
```

---

## 4. Syntax & Directives Reference

| Directive | Context | Default | Purpose / Details |
| :--- | :--- | :--- | :--- |
| `max_fails=number` (in `server`) | `upstream` | `1` | Number of failed attempts within `fail_timeout` to mark node down. Set to `0` to disable checks. |
| `fail_timeout=time` (in `server`) | `upstream` | `10s` | Time window to accumulate `max_fails`, and duration node remains in OPEN state. |
| `backup` (in `server`) | `upstream` | None | Marks server as emergency backup; only used when all primaries are OPEN. |
| `down` (in `server`) | `upstream` | None | Permanently marks server as unavailable. |
| `proxy_next_upstream condition ...` | `http`, `server`, `location` | `error timeout` | Conditions triggering immediate failover to the next upstream node. |
| `proxy_next_upstream_tries number` | `http`, `server`, `location` | `0` (Uncapped) | Maximum number of upstream servers NGINX will try before returning error. |
| `proxy_next_upstream_timeout time` | `http`, `server`, `location` | `0` (Uncapped) | Maximum total time allowed for failover retry attempts. |
| `proxy_connect_timeout time` | `http`, `server`, `location` | `60s` | Max time allowed to establish TCP connection with upstream backend. |

---

## 5. Practical Implementation Examples

### Example 1: The Canonical Passive Circuit Breaker Configuration

A hardened 3-node cluster: trips circuit after 3 failures within 10 seconds, retrying failed requests across healthy nodes.

```nginx
# /etc/nginx/conf.d/passive_breaker.conf

upstream payment_api {
    # Tripping rule: 3 failures in 10s trips circuit OPEN for 10s
    server 10.0.1.10:8000 max_fails=3 fail_timeout=10s;
    server 10.0.1.11:8000 max_fails=3 fail_timeout=10s;
    server 10.0.1.12:8000 max_fails=3 fail_timeout=10s;

    keepalive 32;
}

server {
    listen 80;
    server_name pay.example.com;

    location / {
        proxy_pass http://payment_api;
        proxy_set_header Host $host;

        # Tight connection timeout so dead servers are detected instantly
        proxy_connect_timeout 2s;
        proxy_read_timeout 10s;

        # Failover trigger conditions
        proxy_next_upstream error timeout http_502 http_503 http_504;
        proxy_next_upstream_tries 3;
        proxy_next_upstream_timeout 5s;

        # Keepalive headers
        proxy_http_version 1.1;
        proxy_set_header Connection "";

        add_header X-Handled-By $upstream_addr always;
    }
}
```

### Example 2: Multi-Tiered Failover with Standby Backup Upstreams

If all primary microservice instances crash, traffic immediately diverts to a dedicated static backup server serving a graceful degraded maintenance response.

```nginx
# /etc/nginx/conf.d/backup_failover.conf

upstream core_service {
    # Primary Production Nodes
    server 10.0.10.1:8080 max_fails=2 fail_timeout=5s;
    server 10.0.10.2:8080 max_fails=2 fail_timeout=5s;

    # Emergency Standby Server
    # Only invoked when BOTH primaries trip their circuits!
    server 10.0.99.1:8080 backup;
}

server {
    listen 80;
    server_name store.example.com;

    location / {
        proxy_pass http://core_service;
        proxy_set_header Host $host;

        proxy_next_upstream error timeout http_500 http_502 http_503;
        proxy_connect_timeout 1s;

        add_header X-Served-By $upstream_addr always;
    }
}
```

### Example 3: Graceful Degradation via Stale Cache on Circuit Trip

Combines reverse proxy caching with circuit breaking: if all backends fail, NGINX serves the last-known good cached version of the page rather than showing an error.

```nginx
# /etc/nginx/conf.d/stale_circuit_breaker.conf

proxy_cache_path /var/cache/nginx/resilient_cache
                 levels=1:2
                 keys_zone=RESILIENT:20m
                 max_size=2g
                 inactive=24h
                 use_temp_path=off;

upstream catalog_nodes {
    server 10.0.1.10:8000 max_fails=2 fail_timeout=15s;
    server 10.0.1.11:8000 max_fails=2 fail_timeout=15s;
}

server {
    listen 80;
    server_name shop.example.com;

    location / {
        proxy_pass http://catalog_nodes;
        proxy_set_header Host $host;

        proxy_cache RESILIENT;
        proxy_cache_valid 200 5m;

        # If backends trip, serve stale cached content indefinitely!
        proxy_cache_use_stale error timeout invalid_header updating http_500 http_502 http_503 http_504;

        proxy_next_upstream error timeout http_502 http_503;
        proxy_connect_timeout 1s;

        add_header X-Cache-Status $upstream_cache_status always;
        add_header X-Backend-Server $upstream_addr always;
    }
}
```

### Example 4: Capping Retries to Prevent Cascading Failures

Under extreme load, uncapped retries cause retry storms that crush surviving nodes. This example strictly caps failover attempts.

```nginx
# /etc/nginx/conf.d/retry_budget.conf

upstream high_traffic_api {
    server 10.0.1.10:8000 max_fails=3 fail_timeout=10s;
    server 10.0.1.11:8000 max_fails=3 fail_timeout=10s;
    server 10.0.1.12:8000 max_fails=3 fail_timeout=10s;
    server 10.0.1.13:8000 max_fails=3 fail_timeout=10s;
}

server {
    listen 80;
    server_name api.example.com;

    location / {
        proxy_pass http://high_traffic_api;
        proxy_set_header Host $host;

        # Strict retry budget:
        # At most 2 servers tried in total (1 primary attempt + 1 retry)
        proxy_next_upstream_tries 2;

        # Cap total failover time budget to 3 seconds
        proxy_next_upstream_timeout 3s;

        # Connect timeout: Drop dead server in 1 second
        proxy_connect_timeout 1s;

        proxy_next_upstream error timeout http_502 http_504;
    }
}
```

### Example 5: Emulating Active Probing in Open-Source NGINX via njs

Using an asynchronous njs script to probe a backend `/healthz` endpoint on a periodic timer and dynamically adjust an NGINX variable routing target.

#### `/etc/nginx/njs/health_checker.js`:
```javascript
// Background active probe emulator
let nodeHealthy = true;

async function checkHealth() {
    try {
        const res = await ngx.fetch('http://10.0.1.10:8000/healthz', {
            method: 'GET',
            timeout: 1000
        });
        nodeHealthy = (res.status === 200);
    } catch (e) {
        nodeHealthy = false;
    }
}

// Probe every 5 seconds
setInterval(checkHealth, 5000);

function routeTarget(r) {
    return nodeHealthy ? 'primary_cluster' : 'backup_cluster';
}

export default { routeTarget };
```

#### NGINX Configuration:
```nginx
js_import hc from /etc/nginx/njs/health_checker.js;
js_set $active_pool hc.routeTarget;

server {
    listen 80;
    server_name probed.example.com;

    location / {
        proxy_pass http://$active_pool;
        proxy_set_header Host $host;
    }
}
```

---

## 6. Production Best Practices

1. **Always Reduce `proxy_connect_timeout`**: The default `proxy_connect_timeout` is `60s`. If a backend server crashes or is rebooted, a client request will hang for an entire minute before NGINX marks it as failed and retries! In local datacenters or cloud VPCs, TCP connections establish in $<5\text{ms}$. Set `proxy_connect_timeout 1s;` or `2s;`.
2. **Always Constrain Retries (`proxy_next_upstream_tries`)**: Never leave `proxy_next_upstream_tries` at its default `0` (unlimited). If you have 20 servers in a pool and a systemic error occurs (e.g., bad database migration), an incoming request will retry against all 20 servers sequentially, creating a massive retry storm. Cap retries to `2` or `3`.
3. **Never Include `non_idempotent` on Transactional Routes**: Do not configure `proxy_next_upstream ... non_idempotent;` on routes handling payments, order creations, or resource mutations unless your application explicitly handles idempotency keys.
4. **Tune `max_fails` Based on Traffic Volume**: For high-traffic servers (thousands of req/sec), set `max_fails=3` or `5`. Setting `max_fails=1` can cause healthy nodes to be prematurely evicted due to a single transient network glitch.
5. **Always Pair with Keepalive Pools**: Healthy connection pooling ensures that NGINX does not re-run TCP handshakes on every request, reducing connection timeout failures.
6. **Set `fail_timeout` to an Operationally Sane Window**: Set `fail_timeout=10s` to `30s`. Setting it too short (e.g., `1s`) causes NGINX to flood dead servers with probe requests before they finish restarting; setting it too long (e.g., `1h`) leaves recovered servers idle for hours.

---

## 7. Common Pitfalls & Anti-Patterns

| Anti-Pattern | Why It Fails | Correct Approach |
| :--- | :--- | :--- |
| **Default `proxy_connect_timeout 60s;`** | When an upstream server dies, incoming client requests freeze for 60 seconds before failing over to the next node. | Set `proxy_connect_timeout 1s;` or `2s;`. |
| **`max_fails=1` in production** | A single transient TCP retransmission or dropped packet trips the node offline for the entire `fail_timeout` window. | Use `max_fails=3` or `max_fails=5`. |
| **Uncapped retries (`proxy_next_upstream_tries 0;`)** | When backend outage occurs, thousands of client requests cascade across every single server in the pool, crashing all surviving instances. | Set `proxy_next_upstream_tries 2;` or `3;` and `proxy_next_upstream_timeout 5s;`. |
| **Retrying `POST` requests without idempotency** | If a backend processes an order but crashes before transmitting the 200 OK, NGINX retries on server 2, resulting in a duplicate order/charge. | Exclude `non_idempotent` from `proxy_next_upstream`. |
| **Setting `max_fails=0` accidentally** | Setting `max_fails=0` **completely disables health checking**; NGINX will continue routing traffic to dead nodes indefinitely! | Set `max_fails=3` with an explicit `fail_timeout`. |
| **Forgetting to configure backup servers** | When all primary nodes trip their circuits, NGINX has nowhere to route and returns an unformatted raw 502 error to users. | Configure a standby `server ... backup;` serving a graceful maintenance page. |

---

## 8. Performance, Security & Troubleshooting

### Diagnostic CLI Commands

```bash
# 1. Test Failover in Action: Shut Down One Backend Node
# Observe NGINX instantly retrying on Server 2 without dropping the client request:
curl -s -i http://pay.example.com/ | grep -i x-handled-by

# 2. Inspect Error Log for Upstream Eviction Notices
tail -f /var/log/nginx/error.log | grep -E "no live upstreams|connect\(\) failed"
# Sample log:
# 2026/03/30 14:00:00 [error] 1234#1234: *567 connect() failed (111: Connection refused) while connecting to upstream, client: 10.0.0.1, upstream: "http://10.0.1.10:8000/api", host: "pay.example.com"

# 3. Simulate Upstream Delay and Verify Connect Timeout
# Inject 5-second artificial latency on backend; verify NGINX drops connection at 2s:
curl -s -o /dev/null -w 'Total Request Time: %{time_total}s\n' http://pay.example.com/

# 4. Verify Backup Server Activation
# Stop ALL primary backend servers; query NGINX:
curl -s -i http://store.example.com/ | grep -i x-served-by
# Verify response originates from backup node (10.0.99.1)
```

---

## 9. Architectural Comparison Table

| Metric / Dimension | Passive Health Checks (NGINX OSS) | Active Health Checks (NGINX Plus) | External Service Mesh (Envoy/Istio) |
| :--- | :--- | :--- | :--- |
| **Mechanism** | Live client traffic monitoring | Synthetic background probes | Envoy sidecar outlier detection |
| **Synthetic Network Overhead**| **Zero** | Low (Periodic HTTP probes) | High (Continuous bidirectional probes)|
| **Detection Speed** | Instant upon client traffic | Dependent on probe interval (e.g., 5s)| Fast (Configurable outlier ejection) |
| **Zero-Traffic Health Status** | Unknown until client connects | Always known | Always known |
| **Cost / License** | **100% Free / Open Source** | Commercial License required | Open Source (High complexity) |
| **Configuration Complexity** | Very Low (2 directives) | Low | High (Control plane required) |

---

## 10. Technical Interview Questions

<details>
<summary><strong>Q1: Explain how the Circuit Breaker pattern is implemented in open-source NGINX using passive health checks.</strong></summary>

**Answer:**
Open-source NGINX implements the Circuit Breaker pattern via three states:
1. **CLOSED (Healthy)**: During normal operation, the failure counter for an upstream server is below `max_fails`. NGINX distributes traffic across all servers according to the load-balancing algorithm.
2. **OPEN (Tripped)**: If an upstream server experiences `max_fails` unsuccessful communication attempts (as defined by `proxy_next_upstream`) within the `fail_timeout` window, NGINX trips the circuit. The server is marked unavailable and excluded from the load-balancing pool for the entire duration of `fail_timeout`. Zero requests are sent to this node.
3. **HALF-OPEN (Probe)**: Once `fail_timeout` elapses, NGINX transitions the server into a half-open state. It allows **exactly one live client request** to be routed to that server as a test probe. If that probe request succeeds, the failure counter is reset to zero, and the circuit resets to **CLOSED**. If the probe request fails, the circuit immediately returns to **OPEN** for another `fail_timeout` period.
</details>

<details>
<summary><strong>Q2: Why is the default proxy_connect_timeout of 60 seconds dangerous in a production upstream cluster?</strong></summary>

**Answer:**
When a backend server crashes, experiences kernel panic, or is forcibly terminated, the underlying operating system may not cleanly send a TCP FIN packet; instead, packets are silently dropped (blackholed). 

With the default `proxy_connect_timeout 60s;`, any client request routed to that dead server will freeze and hang for **60 full seconds** before NGINX gives up and attempts to fail over via `proxy_next_upstream`. For an e-commerce or financial API, this leads to browser timeouts, frustrated users, and thread pool exhaustion on upstream gateways. 

In modern cloud environments, TCP handshakes take milliseconds. Reducing `proxy_connect_timeout` to `1s` or `2s` ensures dead instances are detected and bypassed almost instantaneously.
</details>

<details>
<summary><strong>Q3: What is the purpose of proxy_next_upstream_tries and proxy_next_upstream_timeout?</strong></summary>

**Answer:**
By default, if an upstream request fails and `proxy_next_upstream` is configured, NGINX will continue retrying the request on subsequent upstream servers until either a server succeeds or all servers in the pool have been exhausted.

In large clusters (e.g., 20 servers), if a fatal database crash occurs that affects all backends, a single incoming request would attempt 20 consecutive retries. Multiplying this by 1,000 incoming requests creates a **retry storm** of 20,000 requests, completely overloading the network and preventing surviving nodes from recovering.
- `proxy_next_upstream_tries number`: Caps the maximum number of upstream servers that can be attempted for a single request (e.g., `proxy_next_upstream_tries 3;`).
- `proxy_next_upstream_timeout time`: Sets a hard time limit on the total retry budget across all servers (e.g., `proxy_next_upstream_timeout 5s;`).
</details>

<details>
<summary><strong>Q4: What happens if max_fails=0 is set on an upstream server?</strong></summary>

**Answer:**
Setting `max_fails=0` **completely disables passive health checking and circuit breaking** for that specific server. 

Even if the server crashes, refuses TCP connections, or times out on 10,000 consecutive requests, NGINX will **never** mark the server as down. NGINX will continue routing traffic to the dead node as normal. While this might be used for testing, it is an anti-pattern in production environments.
</details>

<details>
<summary><strong>Q5: How does the backup directive function during a total primary cluster failure?</strong></summary>

**Answer:**
Servers marked with the `backup` parameter are held in a dormant, standby state. During normal operations, NGINX sends **0% of traffic** to backup nodes. 

However, if all primary servers in the upstream pool fail and enter the OPEN (tripped) state simultaneously, NGINX immediately promotes the backup servers to active status. All incoming requests are routed to the backup servers until at least one primary server passes its half-open probe and recovers. This allows organizations to host lightweight, static fallback maintenance pages or read-only replica gateways.
</details>

<details>
<summary><strong>Q6: Why is proxy_next_upstream non_idempotent dangerous when proxying credit card transactions?</strong></summary>

**Answer:**
Under the HTTP specification, `POST` requests are non-idempotent. If NGINX sends a `POST /checkout` to Backend Server 1, Server 1 may successfully charge the customer's credit card and write the transaction to the database, but experience a network glitch or timeout before transmitting the HTTP 200 response back to NGINX. 

If `non_idempotent` is enabled in `proxy_next_upstream`, NGINX treats the timeout as a failure and immediately retries the exact same `POST` request on Backend Server 2. Server 2 will execute the checkout again, resulting in the customer being billed twice. Therefore, `non_idempotent` should only be used if the backend application implements transactional idempotency tokens.
</details>

<details>
<summary><strong>Q7: What is the difference between active health checks in NGINX Plus and passive health checks in open-source NGINX?</strong></summary>

**Answer:**
- **Passive Health Checks (Open-Source)**: Reactive. NGINX monitors real client requests. A dead server can only be detected when a real client request happens to hit it and fails. If no traffic arrives, NGINX has no knowledge of server health.
- **Active Health Checks (NGINX Plus)**: Proactive. NGINX runs a background timer and generates independent synthetic HTTP probe requests (e.g., `GET /healthz`) at fixed intervals. If a server fails its synthetic probe, NGINX marks it down *before* any real user traffic ever touches it. Furthermore, it supports deep inspection of response headers, status codes, and body regex matching.
</details>

<details>
<summary><strong>Q8: How can proxy_cache_use_stale be used as a circuit breaker fallback?</strong></summary>

**Answer:**
By combining caching with upstream error interception:
```nginx
proxy_cache_use_stale error timeout http_502 http_503 http_504;
```
If all primary backend instances crash or trip their circuit breakers, NGINX intercepts the error and checks its local disk cache. If a cached version of the requested page exists (even if its TTL has expired and it is marked "stale"), NGINX delivers the stale cached content to the user with an `X-Cache-Status: STALE` header. The user receives a functional, fast page rather than an error screen.
</details>

---

## 11. Hands-On Practice Exercises

<details>
<summary><strong>Exercise 1: Configure Passive Circuit Breaking with 2s Connect Timeout</strong></summary>

**Goal:** Configure an upstream with 2 nodes where 2 failures within 5 seconds trips the node offline for 10 seconds.

**Solution:**
```nginx
upstream circuit_pool {
    server 127.0.0.1:8001 max_fails=2 fail_timeout=10s;
    server 127.0.0.1:8002 max_fails=2 fail_timeout=10s;
}

server {
    listen 80;
    server_name breaker.local;

    location / {
        proxy_pass http://circuit_pool;
        proxy_connect_timeout 2s;
        proxy_next_upstream error timeout http_502;
    }
}
```
</details>

<details>
<summary><strong>Exercise 2: Setup Emergency Backup Server</strong></summary>

**Goal:** Configure a primary server on port 9001 and a backup server on port 9002 that only activates when 9001 fails.

**Solution:**
```nginx
upstream backup_demo {
    server 127.0.0.1:9001 max_fails=1 fail_timeout=5s;
    server 127.0.0.1:9002 backup;
}

server {
    listen 80;
    server_name backup.local;

    location / {
        proxy_pass http://backup_demo;
        proxy_connect_timeout 1s;
    }
}
```
Test:
1. Run backend on 9001; verify requests hit 9001.
2. Terminate backend on 9001; verify requests automatically switch to 9002.
</details>

<details>
<summary><strong>Exercise 3: Restrict Retries with proxy_next_upstream_tries</strong></summary>

**Goal:** Limit failover attempts so that NGINX tries at most 2 servers and gives up after 3 seconds total.

**Solution:**
```nginx
location /api/ {
    proxy_pass http://multi_node_cluster;
    proxy_next_upstream error timeout http_500 http_502;
    proxy_next_upstream_tries 2;
    proxy_next_upstream_timeout 3s;
    proxy_connect_timeout 1s;
}
```
</details>

<details>
<summary><strong>Exercise 4: Verify Failover Using curl and Response Headers</strong></summary>

**Goal:** Expose `$upstream_addr` in response headers to observe NGINX retrying across servers in real time.

**Solution:**
```nginx
server {
    listen 80;
    location / {
        proxy_pass http://cluster;
        proxy_next_upstream error timeout http_502;
        add_header X-Attempted-Servers $upstream_addr always;
    }
}
```
When Server 1 fails and Server 2 responds, curl will output:
`X-Attempted-Servers: 127.0.0.1:8001, 127.0.0.1:8002`
</details>

<details>
<summary><strong>Exercise 5: Configure Stale Cache Fallback on Circuit Trip</strong></summary>

**Goal:** Configure NGINX to serve stale cached responses when upstream servers return 502 or 504.

**Solution:**
```nginx
proxy_cache_path /tmp/stale_cache keys_zone=STALE_ZONE:5m;

server {
    listen 80;
    location / {
        proxy_pass http://failing_backend;
        proxy_cache STALE_ZONE;
        proxy_cache_valid 200 1m;
        proxy_cache_use_stale error timeout http_502 http_504;
    }
}
```
</details>

<details>
<summary><strong>Exercise 6: Diagnose Upstream Timeout Failures in Error Log</strong></summary>

**Goal:** Identify circuit breaker tripping events in NGINX error logs.

**Solution:**
1. Stop an upstream server.
2. Run curl traffic through NGINX.
3. Inspect log output:
   ```bash
   grep -E "upstream timed out|no live upstreams" /var/log/nginx/error.log
   ```
</details>

---

## 12. Summary & Quick Reference

```nginx
# Production Circuit Breaker & Health Check Cheat Sheet

upstream resilient_cluster {
    # Passive Health Check Parameters:
    # 3 failures within 10s trips circuit OPEN for 10s
    server 10.0.1.10:8080 max_fails=3 fail_timeout=10s;
    server 10.0.1.11:8080 max_fails=3 fail_timeout=10s;

    # Standby Circuit Breaker Fallback
    server 10.0.99.1:8080 backup;

    keepalive 32;
}

server {
    listen 80;
    server_name api.example.com;

    location / {
        proxy_pass http://resilient_cluster;

        # 1. Fast detection of dead nodes
        proxy_connect_timeout 2s;
        proxy_read_timeout 15s;

        # 2. Trigger conditions for circuit breaking & retries
        proxy_next_upstream error timeout http_502 http_503 http_504;

        # 3. Prevent retry storms
        proxy_next_upstream_tries 2;
        proxy_next_upstream_timeout 4s;

        # 4. Observability header
        add_header X-Upstream-Path $upstream_addr always;
    }
}
```

---

## 13. Further Reading & Official Documentation
- [Official NGINX ngx_http_upstream_module Documentation](https://nginx.org/en/docs/http/ngx_http_upstream_module.html)
- [Official NGINX ngx_http_proxy_module (proxy_next_upstream)](https://nginx.org/en/docs/http/ngx_http_proxy_module.html#proxy_next_upstream)
- [The Circuit Breaker Pattern (Martin Fowler)](https://martinfowler.com/bliki/CircuitBreaker.html)
- [Release It!: Design and Deploy Production-Ready Software (Michael Nygard)](https://pragprog.com/titles/mnee2/release-it-second-edition/)

---

[← Previous: Connection Limiting & Concurrency](07-connection-limiting-and-concurrency.md) | [Module Overview](README.md) | [Next: Sendfile and Large File Delivery →](09-sendfile-and-large-file-delivery.md)
