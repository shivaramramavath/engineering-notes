---
title: "Canary and Blue-Green Deployments in NGINX"
description: "Production guide to continuous delivery routing: split_clients statistical canary releases, blue-green cutovers, cookie-based targeting, and shadow traffic mirroring with ngx_http_mirror_module."
module: "03-traffic-management"
track: "devops-nginx"
order: 6
difficulty: "Advanced"
prerequisites:
  - "01-fundamentals/03-server-and-location"
  - "02-core-features/04-variables-and-rewrites"
  - "03-traffic-management/02-load-balancing"
tags:
  - nginx
  - canary-deployment
  - blue-green
  - split-clients
  - traffic-mirroring
  - continuous-delivery
  - zero-downtime
last_updated: "2026-03-30"
---

# Canary and Blue-Green Deployments in NGINX

## Table of Contents
- [1. What & Why](#1-what--why)
- [2. Core Concepts](#2-core-concepts)
- [3. How It Works Internally](#3-how-it-works-internally)
- [4. Syntax & Directives Reference](#4-syntax--directives-reference)
- [5. Practical Implementation Examples](#5-practical-implementation-examples)
  - [Example 1: The `split_clients` Deterministic Canary (10% Traffic Rollout)](#example-1-the-split_clients-deterministic-canary-10-traffic-rollout)
  - [Example 2: Instant Blue-Green Production Cutover](#example-2-instant-blue-green-production-cutover)
  - [Example 3: Targeted Canary via Cookies and Developer Headers](#example-3-targeted-canary-via-cookies-and-developer-headers)
  - [Example 4: Production Traffic Shadowing / Dark Launching (`mirror`)](#example-4-production-traffic-shadowing--dark-launching-mirror)
  - [Example 5: Automated Canary Promotion with Prometheus Metrics Feedback](#example-5-automated-canary-promotion-with-prometheus-metrics-feedback)
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

Deploying new software releases directly to 100% of production users simultaneously (the "Big Bang" deployment) is one of the highest-risk operations in software engineering. Even with rigorous unit and integration testing, unanticipated race conditions, memory leaks, database query regressions, and edge-case bugs only manifest under real-world production load.

To mitigate release risk, modern DevOps teams employ progressive delivery patterns:
1. **Canary Deployments**: Route a tiny fraction of real production traffic (e.g., 2% to 10%) to the new release ("canary"). Engineers observe error rates, latency metrics, and APM telemetry. If metrics remain healthy, traffic is incrementally promoted to 100%; if errors spike, traffic is instantly rolled back to 0%.
2. **Blue-Green Deployments**: Maintain two identical production environments ("Blue" active, "Green" idle). Deploy the new release to Green, run sanity smoke tests, and execute an instantaneous cutover at the NGINX edge. If an issue arises, revert back to Blue with a single configuration reload.
3. **Dark Launching / Traffic Mirroring**: Duplicate live incoming user traffic asynchronously to a test cluster using the `mirror` module. The test service processes real traffic without ever sending responses to end users, exposing bugs before launch.

---

## 2. Core Concepts

### The `split_clients` Directive
The `ngx_http_split_clients_module` enables deterministic, statistical percentage-based traffic splitting.
```nginx
split_clients "${remote_addr}${http_user_agent}" $app_version {
    10%     canary_pool;
    *       production_pool;
}
```
**Why it is deterministic**:
`split_clients` computes a 32-bit MurmurHash2 hash of the input string and maps the resulting integer into a 0 to 4,294,967,295 range (0% to 100%). As long as a user connects from the same IP address and User-Agent, **they will always consistently receive the same version**. A user will not toggle between the old and new UI on subsequent page refreshes.

### Cookie and Header Overrides
While percentage splitting works well for public users, internal QA teams, developers, and beta testers need to explicitly opt-in or test the canary release on demand. By combining `map` and `split_clients`, NGINX evaluates overrides first:
1. Check for `Cookie: version=canary`
2. Check for `X-Canary-Test: true`
3. Fall back to the statistical `split_clients` distribution

### Traffic Shadowing (`ngx_http_mirror_module`)
The `mirror` module creates an exact clone of the incoming HTTP request (headers, method, and body) and sends it to a designated mirror upstream as an asynchronous background subrequest.
- **Client Impact**: Zero latency overhead. NGINX does not wait for the mirror backend to respond.
- **Response Handling**: The response from the mirrored backend is completely ignored and discarded by NGINX.
- **Use Case**: Stress testing new database schemas, testing breaking API rewrites, and profiling Go/Rust rewrites under real production traffic without risking customer data.

---

## 3. How It Works Internally

### MurmurHash2 Partitioning in `split_clients`

```
Input String: "${remote_addr}${http_user_agent}"
              ("203.0.113.42Mozilla/5.0 ...")
                       |
                       v
            MurmurHash2 Algorithm
                       |
                       v
         32-bit Unsigned Integer: 3,435,973,836
                       |
                       v
         Percentage Range: (3,435,973,836 / 4,294,967,295) = 80.0%
                       |
      +----------------+----------------+
      |                                 |
  0% to 10% (Canary)             10.1% to 100% (Production)
                                        |
                                        v
                            Matches Production Cluster!
```

### Traffic Mirroring Dataflow

```
Public Client
      |
      | 1. HTTP POST /orders (Body: {"item": "shoes"})
      v
[NGINX Reverse Proxy]
      |
      +-----------------------------------------+
      |                                         |
 2. Primary Request (Synchronous)          3. Mirrored Subrequest (Async)
      |                                         |
      v                                         v
[Production Cluster (Blue)]                [Canary / Shadow Cluster]
      | (Processes Order & DB)                  | (Processes Shadow Test)
      v                                         |
 HTTP 201 Created                               v
      |                                    Response DISCARDED by NGINX
      +---------------------+                   (Zero client impact)
                            |
                            v
             Client Receives: HTTP 201 Created
```

---

## 4. Syntax & Directives Reference

### `ngx_http_split_clients_module`
| Directive | Context | Default | Purpose / Details |
| :--- | :--- | :--- | :--- |
| `split_clients string $variable { percent% value; ... * value; }` | `http` | None | Maps MurmurHash2 of string into percentage-based values. `*` acts as catch-all. |

### `ngx_http_mirror_module`
| Directive | Context | Default | Purpose / Details |
| :--- | :--- | :--- | :--- |
| `mirror uri\|off` | `http`, `server`, `location` | `off` | Enqueues mirror subrequests to specified internal URI. |
| `mirror_request_body on\|off` | `http`, `server`, `location` | `on` | Controls whether the client request body is forwarded to the mirror. |

---

## 5. Practical Implementation Examples

### Example 1: The `split_clients` Deterministic Canary (10% Traffic Rollout)

Directs exactly 10% of global traffic to the `canary_upstream` while keeping 90% on the stable `production_upstream`.

```nginx
# /etc/nginx/conf.d/canary_split.conf

# MurmurHash2 of client IP guarantees sticky routing for repeat visits
split_clients "${remote_addr}" $upstream_variant {
    10%     canary_pool;
    *       production_pool;
}

upstream production_pool {
    server 10.0.1.10:8000 max_fails=2 fail_timeout=10s;
    server 10.0.1.11:8000 max_fails=2 fail_timeout=10s;
    keepalive 32;
}

upstream canary_pool {
    server 10.0.2.10:8000 max_fails=2 fail_timeout=10s;
    keepalive 16;
}

server {
    listen 80;
    server_name api.example.com;

    location / {
        proxy_pass http://$upstream_variant;
        proxy_set_header Host $host;
        proxy_set_header X-Real-IP $remote_addr;

        # Keepalive headers
        proxy_http_version 1.1;
        proxy_set_header Connection "";

        # Observability header
        add_header X-Deployment-Variant $upstream_variant always;
    }
}
```

### Example 2: Instant Blue-Green Production Cutover

Execute an instantaneous zero-downtime cutover between Blue and Green clusters via an included snippet file and an `nginx -s reload`.

#### Upstream Definitions (`/etc/nginx/conf.d/blue_green_clusters.conf`):
```nginx
upstream cluster_blue {
    server 10.0.10.1:8080;
    server 10.0.10.2:8080;
    keepalive 32;
}

upstream cluster_green {
    server 10.0.20.1:8080;
    server 10.0.20.2:8080;
    keepalive 32;
}
```

#### Active Target Snippet (`/etc/nginx/snippets/active_deployment.conf`):
```nginx
# Single line defining current active production environment:
set $active_cluster "cluster_blue";
```

#### Server Virtual Host (`/etc/nginx/conf.d/app.conf`):
```nginx
server {
    listen 80;
    server_name portal.example.com;

    location / {
        # Read current active deployment target
        include /etc/nginx/snippets/active_deployment.conf;

        proxy_pass http://$active_cluster;
        proxy_set_header Host $host;
        proxy_http_version 1.1;
        proxy_set_header Connection "";

        add_header X-Served-By-Cluster $active_cluster always;
    }
}
```

#### The Zero-Downtime Cutover Script:
```bash
# To switch instantly from Blue to Green:
echo 'set $active_cluster "cluster_green";' > /etc/nginx/snippets/active_deployment.conf
nginx -t && nginx -s reload

# To rollback instantly:
echo 'set $active_cluster "cluster_blue";' > /etc/nginx/snippets/active_deployment.conf
nginx -t && nginx -s reload
```

### Example 3: Targeted Canary via Cookies and Developer Headers

Supports prioritized routing: QA testing headers first, opt-in cookies second, falling back to 5% random statistical rollout.

```nginx
# /etc/nginx/conf.d/multi_tiered_canary.conf

# 1. Statistical 5% split
split_clients "${remote_addr}${http_user_agent}" $random_variant {
    5%      canary_pool;
    *       production_pool;
}

# 2. Check Developer Header Override
map $http_x_canary $header_override {
    "always" canary_pool;
    "never"  production_pool;
    default  "";
}

# 3. Check Cookie Override
map $cookie_beta_tester $cookie_override {
    "true"   canary_pool;
    "false"  production_pool;
    default  "";
}

# 4. Resolve Final Upstream Target with Precedence:
# Header > Cookie > Statistical Random
map "$header_override:$cookie_override" $final_backend {
    "~^canary_pool"      canary_pool;
    "~^production_pool"  production_pool;
    "~^:canary_pool"     canary_pool;
    "~^:production_pool" production_pool;
    default              $random_variant;
}

server {
    listen 80;
    server_name app.example.com;

    location / {
        proxy_pass http://$final_backend;
        proxy_set_header Host $host;

        add_header X-Assigned-Backend $final_backend always;
    }
}
```

### Example 4: Production Traffic Shadowing / Dark Launching (`mirror`)

Clones 100% of live production traffic to a new experimental Go microservice cluster without risking production latency or error rates.

```nginx
# /etc/nginx/conf.d/traffic_mirroring.conf

upstream prod_python_cluster {
    server 10.0.1.10:8000;
}

upstream shadow_go_cluster {
    server 10.0.2.10:9000;
}

server {
    listen 80;
    server_name checkout.example.com;

    location / {
        # 1. Mirror traffic asynchronously to internal location
        mirror /mirror_traffic;
        mirror_request_body on; # Clone entire POST body

        # 2. Primary request served synchronously by production
        proxy_pass http://prod_python_cluster;
        proxy_set_header Host $host;
    }

    # 3. Internal Mirror Handler
    location = /mirror_traffic {
        internal; # Block external access

        proxy_pass http://shadow_go_cluster$request_uri;
        proxy_set_header Host $host;
        proxy_set_header X-Shadow-Traffic "true";

        # Low timeouts so slow shadow nodes never consume NGINX worker buffers
        proxy_connect_timeout 1s;
        proxy_read_timeout 2s;
    }
}
```

### Example 5: Automated Canary Promotion with Prometheus Metrics Feedback

A sample configuration structured for automated canary operators (such as Flagger or Argo Rollouts).

```nginx
# /etc/nginx/conf.d/flagger_canary.conf

# Weighted Upstream Canary Pattern
upstream release_cluster {
    # Primary Stable Deployment (90% Weight)
    server 10.0.1.10:8000 weight=90;

    # Canary Deployment (10% Weight)
    server 10.0.2.10:8000 weight=10;

    keepalive 32;
}

server {
    listen 80;
    server_name service.example.com;

    location / {
        proxy_pass http://release_cluster;
        proxy_set_header Host $host;
        proxy_http_version 1.1;
        proxy_set_header Connection "";

        # Send custom timing and status to access log for Prometheus scraper
        add_header X-Upstream-Addr $upstream_addr always;
        add_header X-Upstream-Response-Time $upstream_response_time always;
    }
}
```

---

## 6. Production Best Practices

1. **Hash on Stable Client Attributes in `split_clients`**: Always hash on `${remote_addr}` or a stable authenticated cookie (e.g., `${cookie_user_id}`). Hashing on `$request_uri` or random variables causes the same user to bounce unpredictably between canary and production on every click.
2. **Always Provide Developer Override Headers**: Never deploy a canary that relies solely on percentage splitting. Always configure a header bypass (`X-Canary: always`) so QA and SRE teams can run integration smoke tests against the canary on demand.
3. **Set Aggressive Timeouts on Shadow Mirrors**: Mirrored subrequests consume NGINX internal buffers until completed. If the shadow service hangs or encounters an infinite loop, NGINX memory buffers fill up. Always set `proxy_connect_timeout 1s;` and `proxy_read_timeout 2s;` in the mirror location.
4. **Never Shadow Non-Idempotent External APIs**: If your backend calls external third-party payment gateways (Stripe, PayPal) or sends transactional emails (SendGrid), traffic shadowing will duplicate live credit card charges and customer emails unless the shadow cluster is explicitly sandboxed.
5. **Always Expose the Assigned Variant in Headers**: Output `add_header X-Deployment-Variant $upstream_variant always;`. When troubleshooting customer bug reports, the header confirms instantly whether the user hit the canary or production.
6. **Use Atomic File Swaps for Blue-Green Switches**: In blue-green pipelines, write the new configuration snippet to a temporary file (`active.tmp`) and use `mv active.tmp active.conf && nginx -s reload` to prevent race conditions during file writing.

---

## 7. Common Pitfalls & Anti-Patterns

| Anti-Pattern | Why It Fails | Correct Approach |
| :--- | :--- | :--- |
| **Using `split_clients $request_uri ...`** | Every different page requested by the same user hashes differently; user gets Canary on `/home` and Production on `/profile`, breaking session continuity. | Hash on stable identifiers: `${remote_addr}` or `${cookie_session_id}`. |
| **Omitting `*` catch-all in `split_clients`** | If percentages do not sum to 100% and `*` is missing, unmapped requests evaluate to an empty string, causing `proxy_pass http://;` to crash with HTTP 500. | Always end `split_clients` with `* fallback_value;`. |
| **Traffic shadowing without database isolation** | The shadow service processes duplicate `POST /orders`, creating duplicate database rows and corrupting production data. | Ensure shadow services connect to an isolated, mock, or read-only database replica. |
| **Using `weight` for stateful canary releases** | Upstream server weights distribute connections randomly on every request; clients switch back and forth between versions on repeat requests. | Use `split_clients` with cookie/IP hashing to guarantee sticky version assignment. |
| **Omitting `internal;` on the mirror location** | External clients can directly browse to `http://example.com/mirror_traffic`, bypassing authentication and querying shadow backends directly. | Mark the mirror location as `internal;`. |
| **Hard restart during Blue-Green cutover** | Running `systemctl restart nginx` to switch versions drops hundreds of active client connections and WebSockets in flight. | Use graceful reload: `nginx -t && nginx -s reload`. |

---

## 8. Performance, Security & Troubleshooting

### Diagnostic CLI Commands

```bash
# 1. Test Statistical Distribution Across 100 Simulated Client IPs
for i in {1..100}; do
  curl -s -I -H "X-Forwarded-For: 192.168.1.$i" http://api.example.com/ | grep -i x-deployment-variant
done | sort | uniq -c

# Expected output for 10% canary:
#  90 X-Deployment-Variant: production_pool
#  10 X-Deployment-Variant: canary_pool

# 2. Test Developer Header Override (Force Canary)
curl -s -I -H "X-Canary: always" http://api.example.com/ | grep -i x-deployment-variant
# Must return: X-Deployment-Variant: canary_pool

# 3. Test Cookie Override (Force Production)
curl -s -I --cookie "beta_tester=false" http://api.example.com/ | grep -i x-deployment-variant
# Must return: X-Deployment-Variant: production_pool

# 4. Verify Traffic Mirroring Execution in Access Logs
tail -f /var/log/nginx/access.log | grep -E "prod_python|shadow_go"
```

### MurmurHash2 Sizing and Performance
`split_clients` evaluates the MurmurHash2 algorithm in less than **20 nanoseconds per request**, adding zero measurable latency to the request processing pipeline.

---

## 9. Architectural Comparison Table

| Strategy | Routing Mechanism | Stickiness | Rollback Speed | Safety / Isolation |
| :--- | :--- | :--- | :--- | :--- |
| **Statistical Canary** | `split_clients` (0%–100%) | **Yes** (by IP/Cookie) | Fast (Config reload) | Low (Real users hit canary) |
| **Targeted Canary** | Header / Cookie match | **Yes** (by Token) | Instant | High (Only QA / Opt-in hit) |
| **Blue-Green Switch** | Upstream variable swap | N/A (100% cutover) | Instant (Config reload) | High (Full test before cut) |
| **Traffic Shadowing** | `mirror` directive | N/A (Cloned traffic) | Instant (Remove directive) | **Maximum** (Zero user impact) |

---

## 10. Technical Interview Questions

<details>
<summary><strong>Q1: Why is split_clients preferred over weighted upstream servers for canary deployments?</strong></summary>

**Answer:**
When using upstream server weights:
```nginx
upstream backend {
    server prod:8000 weight=9;
    server canary:8000 weight=1;
}
```
The load-balancing algorithm distributes requests probabilistically on a **per-request** basis. If a user loads an initial web page, request 1 (HTML) might hit Production, request 2 (`app.js`) might hit Canary, and request 3 (`style.css`) might hit Production. This version mismatch causes broken frontend rendering, JavaScript runtime exceptions, and erratic user experiences.

In contrast, `split_clients "${remote_addr}" $backend`:
Computes a hash of the client's IP (or session cookie). That specific client will consistently map to the **exact same version** on every single request, providing a stable, unified user experience.
</details>

<details>
<summary><strong>Q2: How does the MurmurHash2 algorithm work inside split_clients?</strong></summary>

**Answer:**
When NGINX parses `split_clients`:
1. It reads the interpolated string value (e.g., `$remote_addr$http_user_agent`).
2. It computes a 32-bit MurmurHash2 checksum of the string, yielding an integer between `0` and `4,294,967,295` ($2^{32} - 1$).
3. It divides this integer by $4,294,967,295$ to produce a uniform float between $0.0$ and $1.0$ (0% to 100%).
4. NGINX compares this value against the configured percentage brackets sequentially:
   - `10% canary;`: Matched if the hash falls within the first 10% of the range.
   - `* production;`: Matches all remaining hashes.
Because MurmurHash2 provides excellent avalanche properties and uniform distribution, real-world traffic divides with mathematical precision.
</details>

<details>
<summary><strong>Q3: What are the dangers of using the mirror directive with non-idempotent HTTP methods (POST, PUT, DELETE)?</strong></summary>

**Answer:**
The `mirror` directive creates an exact duplicate of the incoming HTTP request—including the HTTP method and payload body—and transmits it to the shadow backend.

If a production client sends:
```http
POST /api/v1/orders/checkout HTTP/1.1
{"cart_id": 999, "amount": 150.00}
```
Both the production cluster and the shadow cluster will process the order. If the shadow service connects to the production database or calls external payment APIs (like Stripe), the customer will be charged twice and two physical orders will be created in the warehouse. 

Traffic mirroring must only be used if:
1. The shadowed endpoints are strictly read-only (`GET`/`HEAD`), or
2. The shadow backend is completely decoupled from real databases and uses mock payment gateways.
</details>

<details>
<summary><strong>Q4: How do you achieve a zero-downtime Blue-Green cutover in NGINX?</strong></summary>

**Answer:**
1. Define both environments in `upstream` blocks (`cluster_blue` and `cluster_green`).
2. Point `proxy_pass http://$active_cluster;` to a variable populated from an included snippet file (`include /etc/nginx/snippets/active.conf;`).
3. Deploy and verify the new release on Green while Blue handles 100% of live traffic.
4. Execute the cutover: update the snippet file to `set $active_cluster "cluster_green";`.
5. Run `nginx -t && nginx -s reload`.
The master process verifies syntax and spawns new worker processes that route all new incoming connections to Green. Existing active connections on Blue are cleanly drained by old worker processes before terminating, achieving a seamless zero-downtime cutover.
</details>

<details>
<summary><strong>Q5: What happens if the mirrored backend behind the mirror directive crashes or is slow?</strong></summary>

**Answer:**
The `mirror` module is designed to be completely non-blocking and isolated from the primary request:
- NGINX sends the mirror request as an asynchronous subrequest.
- It completely ignores the HTTP status code and response body returned by the mirror backend.
- If the mirror backend crashes (500), rejects connections (RST), or drops packets, the client receives their normal production response unaffected.

However, if the mirror backend is excessively slow and times out, NGINX must maintain the subrequest's in-memory socket buffers until the timeout expires. To prevent slow mirror nodes from exhausting NGINX worker buffers under heavy traffic, you must always set aggressive timeouts (`proxy_connect_timeout 1s; proxy_read_timeout 2s;`) in the mirror location.
</details>

<details>
<summary><strong>Q6: Why must the mirror location block always be marked internal;?</strong></summary>

**Answer:**
If the location handling the mirror proxying (e.g., `location = /mirror_traffic`) is not marked `internal;`, any external user or malicious crawler can send direct HTTP requests to `http://example.com/mirror_traffic`. 

This would allow external users to bypass frontend authentication, send un-sanitized requests directly to the experimental shadow cluster, and potentially trigger unintended backend execution. The `internal;` directive guarantees that the location can only be reached via internal NGINX subrequests generated by the `mirror` directive.
</details>

<details>
<summary><strong>Q7: How can you implement a 3-way canary split (e.g., 80% v1, 15% v2, 5% v3)?</strong></summary>

**Answer:**
You declare multiple cumulative percentage brackets inside the `split_clients` block:
```nginx
split_clients "${remote_addr}" $app_tier {
    80%     v1_stable;
    15%     v2_beta;
    *       v3_alpha; # Catch-all receives remaining 5%
}
```
NGINX maps hashes from 0% to 80% to `v1_stable`, 80.0001% to 95% to `v2_beta`, and the remaining 5% to `v3_alpha`.
</details>

<details>
<summary><strong>Q8: What is the purpose of mirror_request_body off?</strong></summary>

**Answer:**
By default, `mirror_request_body` is `on`, meaning NGINX buffers and transmits the full client request body to the shadow backend. 

Setting `mirror_request_body off;` instructs NGINX to clone and forward **only the HTTP request headers and URL**, discarding the body. This is useful when load testing URL routing, authentication headers, or query parsing on a shadow cluster without paying the network bandwidth and memory buffer cost of transmitting multi-megabyte payloads.
</details>

---

## 11. Hands-On Practice Exercises

<details>
<summary><strong>Exercise 1: Configure a 20% Statistical Canary with split_clients</strong></summary>

**Goal:** Configure NGINX to route 20% of traffic to port 8002 and 80% to port 8001 based on client IP.

**Solution:**
```nginx
split_clients "${remote_addr}" $target_backend {
    20%     canary_upstream;
    *       prod_upstream;
}

upstream prod_upstream { server 127.0.0.1:8001; }
upstream canary_upstream { server 127.0.0.1:8002; }

server {
    listen 80;
    server_name canary.local;

    location / {
        proxy_pass http://$target_backend;
        add_header X-Backend $target_backend always;
    }
}
```
</details>

<details>
<summary><strong>Exercise 2: Implement Instant Blue-Green Switch via Include</strong></summary>

**Goal:** Create a blue-green configuration and switch active traffic using a single shell command.

**Solution:**
1. Create snippet `/tmp/active_env.conf`:
   ```nginx
   set $active_pool "http://127.0.0.1:9001"; # Blue
   ```
2. Configure NGINX:
   ```nginx
   server {
       listen 80;
       server_name bg.local;
       location / {
           include /tmp/active_env.conf;
           proxy_pass $active_pool;
       }
   }
   ```
3. Test Blue: `curl http://bg.local/`.
4. Switch to Green:
   ```bash
   echo 'set $active_pool "http://127.0.0.1:9002";' > /tmp/active_env.conf
   nginx -s reload
   ```
5. Test Green: `curl http://bg.local/`.
</details>

<details>
<summary><strong>Exercise 3: Configure Canary Opt-In via Cookie</strong></summary>

**Goal:** Allow users with cookie `canary=true` to access the canary pool, while all other users hit production.

**Solution:**
```nginx
map $cookie_canary $selected_pool {
    "true"  canary_cluster;
    default prod_cluster;
}

upstream prod_cluster { server 127.0.0.1:8080; }
upstream canary_cluster { server 127.0.0.1:8081; }

server {
    listen 80;
    location / {
        proxy_pass http://$selected_pool;
        add_header X-Pool $selected_pool always;
    }
}
```
Test with `curl --cookie "canary=true" http://localhost/`.
</details>

<details>
<summary><strong>Exercise 4: Setup Traffic Shadowing with the mirror Directive</strong></summary>

**Goal:** Clone live GET requests from `/api/` and send a shadow copy to a test server on port `9999`.

**Solution:**
```nginx
server {
    listen 80;
    server_name mirror.local;

    location /api/ {
        mirror /mirror_backend;
        proxy_pass http://127.0.0.1:8000;
    }

    location = /mirror_backend {
        internal;
        proxy_pass http://127.0.0.1:9999$request_uri;
        proxy_connect_timeout 1s;
        proxy_read_timeout 1s;
    }
}
```
</details>

<details>
<summary><strong>Exercise 5: Build a Multi-Stage Fallback Map</strong></summary>

**Goal:** Implement priority routing: Header override takes precedence over Cookie, which takes precedence over 10% statistical canary.

**Solution:**
```nginx
split_clients "${remote_addr}" $stat_variant {
    10%     canary;
    *       prod;
}

map $http_x_canary $override_variant {
    "true"  canary;
    "false" prod;
    default "";
}

map "$override_variant:$cookie_canary" $final_env {
    "~^canary"      canary;
    "~^prod"        prod;
    "~^:true"       canary;
    "~^:false"      prod;
    default         $stat_variant;
}
```
</details>

<details>
<summary><strong>Exercise 6: Verify Deterministic IP Stickiness</strong></summary>

**Goal:** Verify that a client IP consistently maps to the same canary tier across 50 consecutive requests.

**Solution:**
```bash
# Query endpoint 50 times from the same source IP:
for i in {1..50}; do
  curl -s -I http://localhost/ | grep -i x-backend
done | sort | uniq -c
# Output MUST show 50 identical hits for either canary or prod, never mixed!
```
</details>

---

## 12. Summary & Quick Reference

```nginx
# Production Canary & Blue-Green Routing Cheat Sheet

# 1. Deterministic Percentage Splitting (http context)
split_clients "${remote_addr}" $canary_target {
    10%     canary_cluster;
    *       prod_cluster;
}

# 2. Developer Override Mapping
map $http_x_canary $target_upstream {
    "true"  canary_cluster;
    "false" prod_cluster;
    default $canary_target;
}

# 3. Traffic Shadowing (Dark Launch)
server {
    location /checkout {
        mirror /shadow_sink;
        mirror_request_body on;
        proxy_pass http://prod_cluster;
    }

    location = /shadow_sink {
        internal;
        proxy_pass http://shadow_cluster$request_uri;
        proxy_connect_timeout 1s;
        proxy_read_timeout 2s;
    }
}

# 4. Instant Blue-Green Switch
# In location: include /etc/nginx/snippets/active_env.conf;
# Update file: echo 'set $active "green";' > /etc/nginx/snippets/active_env.conf
# Reload: nginx -t && nginx -s reload
```

---

## 13. Further Reading & Official Documentation
- [Official NGINX ngx_http_split_clients_module Documentation](https://nginx.org/en/docs/http/ngx_http_split_clients_module.html)
- [Official NGINX ngx_http_mirror_module Documentation](https://nginx.org/en/docs/http/ngx_http_mirror_module.html)
- [A/B Testing and Canary Deployments with NGINX (NGINX Blog)](https://www.nginx.com/blog/smart-canary-deployments-with-nginx/)
- [MurmurHash2 Algorithm Specification](https://en.wikipedia.org/wiki/MurmurHash)

---

[← Previous: GeoIP & Map-Based Routing](05-geo-and-map-based-routing.md) | [Module Overview](README.md) | [Next: Connection Limiting and Concurrency →](07-connection-limiting-and-concurrency.md)
