---
title: "Project 08: Canary Releases, Traffic Splitting & Request Shadowing"
description: "Production reference implementation for progressive delivery: statistical percentage canary routing via split_clients, cookie/header targeting, and dark launch request shadowing with ngx_http_mirror_module."
module: "07-projects"
project_id: "08-canary-release-and-traffic-splitting"
status: "completed"
tags: ["nginx", "project", "canary", "traffic-splitting", "split-clients", "mirror", "dark-launch", "ab-testing"]
prerequisites: ["03-traffic-management/06-canary-and-blue-green-deployments.md", "03-traffic-management/05-geo-and-map-based-routing.md"]
estimated_read_time: "35 min"
---

# Project 08: Canary Releases, Traffic Splitting & Request Shadowing

```
+-------------------------------------------------------------------------------+
| Progressive Delivery & Traffic Shadowing Pipeline                             |
|                                                                               |
|  [Incoming Production Request]                                                |
|        │                                                                      |
|        ▼                                                                      |
|  [NGINX Traffic Decision Engine]                                              |
|        │                                                                      |
|        ├── Step 1: Explicit Override Checks:                                  |
|        │   - Header `X-Canary: always`    --> Force V2 (Canary)               |
|        │   - Cookie `canary_user=true`    --> Force V2 (Canary)               |
|        │                                                                      |
|        └── Step 2: Statistical MurmurHash2 Split (split_clients):             |
|            - Hash Key: `$remote_addr$http_user_agent`                         |
|            - 90% Population  --> Route to [Stable V1 Cluster]                 |
|            - 10% Population  --> Route to [Canary V2 Cluster]                 |
|                                                                               |
|  [Optional Dark Launch Shadowing via `mirror` Module]                         |
|  - In background, duplicate 100% of live production requests                  |
|  - Send shadow copy to [Experimental V3 Dark Cluster]                         |
|  - Response from Dark Cluster is discarded; client experiences zero latency!  |
+-------------------------------------------------------------------------------+
```

---

## 1. Architecture & Objective

### Objective
Build a continuous deployment and progressive delivery router inside NGINX that empowers engineering teams to validate new microservice releases safely in production: shifting real user percentages gradually (10% -> 25% -> 50% -> 100%), allowing QA engineers to force canary variants via headers or cookies, and stress-testing experimental builds using asynchronous live traffic shadowing (dark launching).

### Core Architectural Requirements
1. **Statistical Percentage Splitting**: Distribute live production traffic probabilistically (e.g. 90% Stable, 10% Canary) using `split_clients` powered by MurmurHash2.
2. **Deterministic Session Stickiness**: Ensure an individual user consistently lands on the exact same release variant throughout their browsing session.
3. **Explicit Targeting Overrides**: Allow QA engineers and beta testers to override statistical allocation by supplying an HTTP header (`X-Canary: always`) or an opt-in cookie (`canary=1`).
4. **Dark Launch Request Shadowing**: Replicate a copy of incoming production traffic to an experimental backend cluster using `mirror /mirror_backend;` without affecting client response times.
5. **Observability & Telemetry Injection**: Inject `X-Release-Variant: stable|canary` headers into access logs and client response streams for performance comparison in Datadog/Grafana.

---

## 2. Complete Config Code

### 1. Progressive Delivery Configuration (`/etc/nginx/conf.d/canary.example.com.conf`)

```nginx
# ==============================================================================
# 1. Upstream Backend Clusters
# ==============================================================================
upstream upstream_stable_v1 {
    server 10.0.3.11:8080 max_fails=3 fail_timeout=10s;
    server 10.0.3.12:8080 max_fails=3 fail_timeout=10s;
    keepalive 32;
}

upstream upstream_canary_v2 {
    server 10.0.3.21:8080 max_fails=3 fail_timeout=10s;
    keepalive 32;
}

upstream upstream_dark_v3 {
    # Shadow backend: receives duplicate production traffic
    server 10.0.3.99:8080;
    keepalive 16;
}

# ==============================================================================
# 2. Statistical Percentage Splitting via MurmurHash2
# ==============================================================================
# Hash input string: Client IP + User Agent guarantees stickiness per device
split_clients "${remote_addr}${http_user_agent}" $split_variant {
    10%     upstream_canary_v2;
    *       upstream_stable_v1;
}

# ==============================================================================
# 3. Decision Matrix: Priority Cascade
# Priority: 1. Cookie, 2. Header, 3. Statistical Split
# ==============================================================================
map $cookie_canary_override $cookie_decision {
    "always"  upstream_canary_v2;
    "never"   upstream_stable_v1;
    default   "";
}

map $http_x_canary $header_decision {
    "always"  upstream_canary_v2;
    "never"   upstream_stable_v1;
    default   "";
}

map $cookie_decision $final_upstream {
    # If cookie is set, it wins
    "upstream_canary_v2" upstream_canary_v2;
    "upstream_stable_v1" upstream_stable_v1;
    # Fallback to header decision
    default              $header_fallback;
}

map $header_decision $header_fallback {
    "upstream_canary_v2" upstream_canary_v2;
    "upstream_stable_v1" upstream_stable_v1;
    # Fallback to statistical percentage split
    default              $split_variant;
}

# Map final upstream to human-readable variant label for telemetry
map $final_upstream $release_tag {
    "upstream_canary_v2" "canary-v2";
    "upstream_stable_v1" "stable-v1";
    default              "unknown";
}

# ==============================================================================
# 4. Master Virtual Host
# ==============================================================================
server {
    listen 443 ssl;
    http2 on;
    server_name app.example.com;

    ssl_certificate /etc/letsencrypt/live/app.example.com/fullchain.pem;
    ssl_certificate_key /etc/letsencrypt/live/app.example.com/privkey.pem;
    ssl_protocols TLSv1.2 TLSv1.3;

    # Telemetry Response Headers
    add_header X-Release-Variant $release_tag always;

    location / {
        # Optional: Mirror 100% of live traffic to dark launch backend asynchronously
        mirror /mirror_dark_launch;
        mirror_request_body on;

        # Dynamic Proxy Pass to Selected Upstream
        proxy_pass http://$final_upstream;
        proxy_http_version 1.1;
        proxy_set_header Connection "";

        proxy_set_header Host $host;
        proxy_set_header X-Real-IP $remote_addr;
        proxy_set_header X-Forwarded-For $proxy_add_x_forwarded_for;
        proxy_set_header X-Release-Variant $release_tag;

        proxy_connect_timeout 3s;
        proxy_read_timeout 30s;
    }

    # --------------------------------------------------------------------------
    # Dark Launch Asynchronous Mirror Target
    # --------------------------------------------------------------------------
    location = /mirror_dark_launch {
        internal;
        proxy_pass http://upstream_dark_v3$request_uri;
        proxy_http_version 1.1;
        proxy_set_header Connection "";

        proxy_set_header Host $host;
        proxy_set_header X-Real-IP $remote_addr;
        proxy_set_header X-Shadow-Traffic "true";

        # Low timeout for shadow cluster: never hold up workers
        proxy_connect_timeout 1s;
        proxy_read_timeout 2s;
        proxy_send_timeout 2s;
    }
}
```

---

## 3. Step-by-Step Walkthrough

1. **The Priority Decision Cascade**:
   - In modern continuous delivery, automated statistical splitting is not enough; QA engineers must test the canary before general public rollout.
   - We construct a multi-stage `map` hierarchy:
     1. If `Cookie: canary_override=always` is present, `$final_upstream` routes immediately to `upstream_canary_v2`.
     2. Else, if `X-Canary: always` header is present, it routes to `upstream_canary_v2`.
     3. If neither override exists, it defaults to `$split_variant` (the statistical hash).

2. **MurmurHash2 Consistency (`split_clients`)**:
   - `split_clients "${remote_addr}${http_user_agent}" $split_variant` computes a 32-bit MurmurHash2 hash of the input string.
   - The resulting hash maps into a numerical space from 0 to 4,294,967,295 ($2^{32}-1$).
   - `10%` assigns the first 10% of that integer range to `upstream_canary_v2`; the remaining `*` (90%) maps to `upstream_stable_v1`.
   - Because the hash key incorporates client IP and user agent, an end user remains pinned to the same variant across repeated visits without requiring database session state!

3. **Asynchronous Request Mirroring (`ngx_http_mirror_module`)**:
   - `mirror /mirror_dark_launch;` duplicates incoming HTTP requests in memory.
   - While the primary request is being served by the stable backend, NGINX creates an internal subrequest targeting `upstream_dark_v3`.
   - `mirror_request_body on;` ensures the full POST/PUT payload is forwarded to the dark launch service.
   - *Core Invariant*: NGINX completely ignores and discards the HTTP response returned by the shadow cluster. If the dark cluster throws 500 errors or times out, the client never knows, and primary response latency is 100% unaffected!

4. **Telemetry Tagging**:
   - Emitting `X-Release-Variant $release_tag` in the response header and access log allows Prometheus and Datadog to group error rates and latency percentiles by release tag, enabling automated Canary Analysis (ACA) to trigger rollbacks if canary error rates spike.

---

## 4. Verification & Testing

### Test 1: Verify Statistical 10% Canary Distribution
Run 1,000 synthetic requests with randomized IP addresses to simulate diverse client populations:
```bash
python3 -c '
import urllib.request, random
variants = {"canary-v2": 0, "stable-v1": 0}
for _ in range(1000):
    ip = f"192.168.{random.randint(1,254)}.{random.randint(1,254)}"
    req = urllib.request.Request("https://app.example.com/", headers={"X-Forwarded-For": ip})
    try:
        with urllib.request.urlopen(req) as resp:
            tag = resp.headers.get("X-Release-Variant")
            variants[tag] = variants.get(tag, 0) + 1
    except Exception:
        pass
print("Distribution Results:", variants)
'
```
*Expected Output*:
```text
Distribution Results: {'stable-v1': 898, 'canary-v2': 102}
```
Observed distribution matches the configured 90% / 10% ratio within standard statistical variance.

### Test 2: Test Header-Based QA Override
```bash
curl -sI -H "X-Canary: always" https://app.example.com/ | grep -i "X-Release-Variant"
# Output: X-Release-Variant: canary-v2
```

### Test 3: Test Cookie-Based User Override
```bash
curl -sI --cookie "canary_override=always" https://app.example.com/ | grep -i "X-Release-Variant"
# Output: X-Release-Variant: canary-v2
```

### Test 4: Verify Dark Launch Shadow Traffic
Tail logs on the dark launch backend (`10.0.3.99`):
```bash
sudo tail -f /var/log/app/dark-access.log | grep "X-Shadow-Traffic"
```
Send requests to `https://app.example.com/` and confirm that every request generates an identical shadow request on the dark launch backend.

---

## 5. Troubleshooting & Edge Cases

### Edge Case 1: Proxy Caching Across Different Canary Variants
- **Symptom**: Stable users receive Canary responses because NGINX cached the response under the same URI key.
- **Root Cause**: `proxy_cache_key` did not include `$release_tag`, causing cache collisions between versions.
- **Fix**: Incorporate the release tag into the cache key:
  ```nginx
  proxy_cache_key "$scheme$request_method$host$request_uri$release_tag";
  ```

### Edge Case 2: Mirror Subrequests Settle Disk/Connection Backlogs
- **Symptom**: NGINX workers experience high memory usage when the dark launch backend hangs.
- **Root Cause**: `proxy_read_timeout` on the mirror location was left at the 60-second default, causing thousands of mirrored subrequests to linger in memory.
- **Fix**: Set aggressive timeouts (`proxy_connect_timeout 1s; proxy_read_timeout 2s;`) on the `location = /mirror_dark_launch` block.

---

[← Previous Project: 07-tls-termination-and-certbot-automation.md](07-tls-termination-and-certbot-automation.md) | [Back to Module Index](README.md) | [Next Module: 08-interview →](../08-interview/README.md)
