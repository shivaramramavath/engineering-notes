---
title: "Rate Limiting and Traffic Shaping in NGINX"
description: "Mastering NGINX rate limiting: the leaky bucket algorithm, burst management, nodelay vs delay two-stage traffic shaping, HTTP 429 status codes, and IP whitelisting."
module: "03-traffic-management"
track: "devops-nginx"
order: 3
difficulty: "Intermediate to Advanced"
prerequisites:
  - "01-fundamentals/03-server-and-location"
  - "02-core-features/04-variables-and-rewrites"
tags:
  - nginx
  - rate-limiting
  - traffic-shaping
  - leaky-bucket
  - burst
  - nodelay
  - ddos-mitigation
last_updated: "2026-03-30"
---

# Rate Limiting and Traffic Shaping in NGINX

## Table of Contents
- [1. What & Why](#1-what--why)
- [2. Core Concepts](#2-core-concepts)
- [3. How It Works Internally](#3-how-it-works-internally)
- [4. Syntax & Directives Reference](#4-syntax--directives-reference)
- [5. Practical Implementation Examples](#5-practical-implementation-examples)
  - [Example 1: Strict API Endpoint Rate Limiting (HTTP 429)](#example-1-strict-api-endpoint-rate-limiting-http-429)
  - [Example 2: Burst Allowance with `nodelay` (Real-World Web Apps)](#example-2-burst-allowance-with-nodelay-real-world-web-apps)
  - [Example 3: Two-Stage Traffic Shaping (`delay=N`)](#example-3-two-stage-traffic-shaping-delayn)
  - [Example 4: IP Whitelisting and Dynamic Exemptions with `geo` & `map`](#example-4-ip-whitelisting-and-dynamic-exemptions-with-geo--map)
  - [Example 5: Multi-Tier Layered Rate Limiting (Per-IP + Global API Token)](#example-5-multi-tier-layered-rate-limiting-per-ip--global-api-token)
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

Rate limiting is one of the most critical defensive controls in web security and system reliability engineering. Without rate limiting, public web endpoints are vulnerable to:
1. **Denial of Service (DoS / DDoS)**: Malicious actors or runaway automated scripts flooding endpoints with requests, exhausting CPU and memory.
2. **Credential Stuffing & Brute Force Attacks**: Automated bots attempting millions of password combinations against `/login` or `/api/auth`.
3. **API Scraping & Resource Depletion**: Web crawlers downloading entire databases via unchecked GET requests, consuming bandwidth and database connections.
4. **Service Level Agreement (SLA) Violations**: "Noisy neighbor" tenants monopolizing multitenant microservice capacity.

NGINX implements high-performance, in-memory rate limiting via the **Leaky Bucket algorithm**. Operating directly at the network edge in the pre-access phase, NGINX evaluates millions of client keys with microsecond overhead, dropping or queueing abusive traffic before backend application servers are ever contacted.

---

## 2. Core Concepts

### The Leaky Bucket Algorithm
NGINX models rate limiting as a bucket with a small hole in the bottom:
- **Incoming Requests**: Water poured into the bucket at random intervals and bursty speeds.
- **Leak Rate (`rate=r/s` or `rate=r/m`)**: Water leaves the bucket at a strictly constant, uniform rate.
- **Bucket Capacity (`burst`)**: The maximum volume of water the bucket can hold.
- **Overflow**: If water is poured in faster than the leak rate and the bucket fills completely, excess water overflows the rim and is **immediately discarded** (NGINX returns an HTTP error code).

### Traffic Policing vs Traffic Shaping
- **Traffic Policing (Hard Rejection)**: When a client exceeds the defined rate, excess requests are rejected immediately with an HTTP 429 or 503 status code.
- **Traffic Shaping (Smooth Delay)**: When a client exceeds the baseline rate, excess requests within the `burst` capacity are queued in memory and delayed, allowing them to be processed at the steady leak rate without dropping connections.

### The Evolution of Burst Handling
1. **Default `burst` (Delay Queue)**:
   `limit_req zone=api burst=5;`
   If 5 requests arrive at once, request 1 is processed immediately. Requests 2, 3, 4, and 5 are queued and delayed, executed one by one at intervals dictated by the rate.
2. **Burst with `nodelay` (Immediate Execution)**:
   `limit_req zone=api burst=5 nodelay;`
   All 5 burst requests are processed **immediately without latency**. However, the bucket is now full. Any 6th request arriving before the bucket drains will be rejected.
3. **Two-Stage Rate Limiting (`delay=N`)**:
   `limit_req zone=api burst=10 delay=4;`
   Introduced in NGINX 1.15.7+. The first 4 burst requests are processed immediately with zero delay. Requests 5 through 10 are queued and shaped with increasing latency. Request 11 is rejected.

---

## 3. How It Works Internally

### The Leaky Bucket State Machine in NGINX Shared Memory

```
Client Request arrives at NGINX
               |
               v
Extract Key from Client (e.g., $binary_remote_addr)
               |
               v
Lookup Key in Shared Memory Red-Black Tree (keys_zone)
        /                                     \
    FOUND                                  NOT FOUND
      /                                         \
Calculate time elapsed since last request       Create new Node in rbtree
Determine current virtual bucket level          Set bucket level = 1
      |                                         Forward request to content phase
      v
Is (current_level + 1) > (rate + burst)?
      /                                 \
    YES                                 NO
     |                                   |
OVERFLOW DETECTED                        Update node timestamp & level
     |                                   |
Return HTTP 429 Too Many Requests        Check delay parameter:
(Drop connection instantly)              - If nodelay: Process immediately
                                         - If delay=N: Calculate sleep timer
                                           and park request on timer queue
```

### Why `$binary_remote_addr` is Mandatory
In NGINX rate-limiting configurations, you should almost always use `$binary_remote_addr` as the key rather than `$remote_addr`:
- `$remote_addr`: Stores the IP as an ASCII string (e.g., `"192.168.1.100"` = 15 bytes; IPv6 up to 39 bytes).
- `$binary_remote_addr`: Stores the IP address in its native binary representation (always exactly **4 bytes for IPv4** and **16 bytes for IPv6**).
- Using `$binary_remote_addr` reduces the in-memory red-black tree node size to just 64 bytes. A 10MB shared memory zone can track over **160,000 unique concurrent client IP addresses**.

---

## 4. Syntax & Directives Reference

| Directive | Context | Default | Purpose / Details |
| :--- | :--- | :--- | :--- |
| `limit_req_zone key zone=name:size rate=rate` | `http` | None | Defines the shared memory zone, tracking key, memory size, and baseline leak rate (`r/s` or `r/m`). |
| `limit_req zone=name [burst=number] [nodelay \| delay=number]` | `http`, `server`, `location` | None | Applies rate limiting from a defined zone with optional burst and delay shaping. |
| `limit_req_status code` | `http`, `server`, `location` | `503` | HTTP status code returned when rate limit is exceeded. Recommended: `429`. |
| `limit_req_log_level info\|notice\|warn\|error` | `http`, `server`, `location` | `error` | Severity level for logging rate limit rejections in `error.log`. |
| `limit_req_dry_run on\|off` | `http`, `server`, `location` | `off` | Evaluates limits and logs rejections without actually dropping or delaying requests (testing mode). |

---

## 5. Practical Implementation Examples

### Example 1: Strict API Endpoint Rate Limiting (HTTP 429)

Configures strict rate limiting on an API endpoint, returning an RFC 6585 compliant HTTP 429 Too Many Requests status code.

```nginx
# /etc/nginx/conf.d/strict_api_ratelimit.conf

# 10MB zone tracks ~160,000 IPs; rate capped at 5 requests per second
limit_req_zone $binary_remote_addr zone=api_strict:10m rate=5r/s;

server {
    listen 80;
    server_name api.example.com;

    # Return standard HTTP 429 instead of default 503
    limit_req_status 429;
    limit_req_log_level warn;

    location /v1/checkout {
        # Strict policing: Zero burst allowance
        # Any request exceeding 5 req/sec is immediately rejected with 429
        limit_req zone=api_strict;

        proxy_pass http://checkout_backend;
        proxy_set_header Host $host;
    }
}
```

### Example 2: Burst Allowance with `nodelay` (Real-World Web Apps)

Modern web pages load 20–50 assets (CSS, JS, images) simultaneously when opened. A strict 5r/s limit would break normal browser page loads. Using `burst=20 nodelay` permits instantaneous bursts while enforcing long-term rate limits.

```nginx
# /etc/nginx/conf.d/burst_nodelay.conf

limit_req_zone $binary_remote_addr zone=web_pages:10m rate=10r/s;

server {
    listen 80;
    server_name www.example.com;

    limit_req_status 429;

    location / {
        # Allows a burst of up to 20 requests immediately without delay
        # Long-term consumption cannot exceed 10 requests per second
        limit_req zone=web_pages burst=20 nodelay;

        proxy_pass http://frontend_upstream;
        proxy_set_header Host $host;
    }
}
```

### Example 3: Two-Stage Traffic Shaping (`delay=N`)

Provides the ultimate user experience: initial bursts are processed instantly, moderate surges are smoothed with graduated delays, and extreme floods are rejected.

```nginx
# /etc/nginx/conf.d/two_stage_shaping.conf

limit_req_zone $binary_remote_addr zone=smart_shaping:10m rate=5r/s;

server {
    listen 80;
    server_name app.example.com;

    limit_req_status 429;

    location /search {
        # Total burst allowed = 15 requests
        # Requests 1 to 5: Processed immediately (within rate)
        # Requests 6 to 10: Processed IMMEDIATELY (delay=5 allows 5 instant burst requests)
        # Requests 11 to 15: Delayed and smoothed according to the 5r/s leak rate
        # Requests 16+: Rejected immediately with HTTP 429
        limit_req zone=smart_shaping burst=15 delay=5;

        proxy_pass http://search_backend;
        proxy_set_header Host $host;
    }
}
```

### Example 4: IP Whitelisting and Dynamic Exemptions with `geo` & `map`

Exempt internal corporate subnets, VPN gateways, and health checkers from rate limiting while enforcing strict limits on public traffic.

```nginx
# /etc/nginx/conf.d/selective_ratelimit.conf

# 1. Classify client IP into internal vs external
geo $is_whitelisted {
    default        0;
    127.0.0.1      1; # Localhost
    10.0.0.0/8     1; # Internal VPC
    192.168.1.0/24 1; # Corporate Office
    198.51.100.42  1; # Datadog / Pingdom Monitoring Probe
}

# 2. Map whitelisted IPs to an EMPTY STRING
# If the key in limit_req_zone evaluates to an empty string, NGINX BYPASSES rate limiting!
map $is_whitelisted $limit_key {
    0 $binary_remote_addr; # Public client: Rate limited
    1 "";                  # Whitelisted: NOT rate limited
}

limit_req_zone $limit_key zone=public_api:10m rate=10r/s;

server {
    listen 80;
    server_name api.example.com;

    location / {
        limit_req zone=public_api burst=20 nodelay;
        limit_req_status 429;

        proxy_pass http://backend_pool;
        proxy_set_header Host $host;
    }
}
```

### Example 5: Multi-Tier Layered Rate Limiting (Per-IP + Global API Token)

Apply multiple rate limit zones simultaneously: protects against a single IP flooding the gateway while also capping total tenant consumption across distributed IPs.

```nginx
# /etc/nginx/conf.d/tiered_ratelimit.conf

# Tier 1: Per-IP zone (Max 20 req/sec per individual IP)
limit_req_zone $binary_remote_addr zone=per_ip:10m rate=20r/s;

# Tier 2: Per-API-Key zone (Max 100 req/sec across an entire corporate tenant)
map $http_x_api_key $api_key_limit {
    default $http_x_api_key;
    ""      $binary_remote_addr; # Fallback to IP if no key provided
}
limit_req_zone $api_key_limit zone=tenant_key:20m rate=100r/s;

server {
    listen 443 ssl;
    server_name enterprise.example.com;

    ssl_certificate     /etc/ssl/certs/enterprise.crt;
    ssl_certificate_key /etc/ssl/private/enterprise.key;

    limit_req_status 429;

    location /api/ {
        # Both limits must be satisfied simultaneously!
        limit_req zone=per_ip burst=10 nodelay;
        limit_req zone=tenant_key burst=50 nodelay;

        proxy_pass http://api_upstream;
        proxy_set_header Host $host;
    }
}
```

---

## 6. Production Best Practices

1. **Always Use `$binary_remote_addr`**: Never use `$remote_addr` for the zone key. The binary representation consumes 4x less memory and prevents memory exhaustion under high unique IP cardinality.
2. **Always Override Default Status Code with `limit_req_status 429;`**: By default, NGINX returns `503 Service Unavailable` on rate limit breach. HTTP 503 indicates an unexpected server failure, confusing uptime monitors. RFC 6585 mandates `429 Too Many Requests`.
3. **Always Combine `burst` with `nodelay` for Web Browsers**: Modern web pages initiate dozens of concurrent HTTP asset requests. Rate limits without `burst ... nodelay` will artificially throttle legitimate users.
4. **Use `limit_req_dry_run on;` When Deploying New Limits**: Test new rate-limiting thresholds in production without impacting live users. Dry-run mode evaluates requests and logs rejections to `error.log` while allowing traffic to pass untouched.
5. **Set Proper Log Levels (`limit_req_log_level warn;`)**: Default log level is `error`, which will flood error logs during DDoS attacks and fill server disks. Use `warn` or `notice`.
6. **Bypass Limits via Empty Strings in `map`**: Take advantage of NGINX's design: when a zone key evaluates to an empty string `""`, NGINX completely skips rate limiting for that request.

---

## 7. Common Pitfalls & Anti-Patterns

| Anti-Pattern | Why It Fails | Correct Approach |
| :--- | :--- | :--- |
| **Omitting `burst` completely** | Any two requests arriving within milliseconds of each other trigger an immediate 429 error, destroying user experience. | Always configure a reasonable `burst` buffer (e.g., `burst=10` or `burst=20`). |
| **Omitting `nodelay` on large bursts** | NGINX queues burst requests and introduces artificial multi-second delays, making web pages feel sluggish and unresponsive. | Append `nodelay` (or use `delay=N`) on web locations. |
| **Using `503` instead of `429`** | Client applications and CDN health monitors treat 503 as an infrastructure crash, triggering false emergency alerts. | Explicitly configure `limit_req_status 429;`. |
| **Rate limiting `$remote_addr` behind Cloudflare / CDN** | All incoming traffic shares the CDN's edge IP addresses; the entire global client base shares a single rate limit bucket. | Rate limit by real client IP header: `limit_req_zone $http_cf_connecting_ip zone=...` or use `ngx_http_realip_module`. |
| **Over-allocating `keys_zone` memory** | Sizing a zone at 500MB when only 10,000 active clients connect wastes RAM that could otherwise be used by the OS page cache. | 10MB holds ~160,000 active IPs, which is more than sufficient for 99% of workloads. |
| **Rate limiting health check endpoints** | Cloud load balancers (AWS ALB, K8s kubelet) poll `/healthz` at high frequency; rate limiting health checks marks healthy nodes dead. | Exempt `/healthz` completely or use `map` to bypass health checker IPs. |

---

## 8. Performance, Security & Troubleshooting

### Diagnostic CLI Commands

```bash
# 1. Test Rate Limiting via curl in a Fast Loop
for i in {1..15}; do
  curl -s -o /dev/null -w "%{http_code}\n" http://api.example.com/v1/checkout
done
# Should observe transition from 200 to 429:
# 200
# 200
# 429
# 429

# 2. Inspect Rate Limiting Rejection Logs in Real Time
tail -f /var/log/nginx/error.log | grep "limiting requests"
# Sample log format:
# 2026/03/30 14:00:00 [warn] 1234#1234: *567 limiting requests, excess: 5.200 by zone "api_strict", client: 192.168.1.50

# 3. Simulate High-Concurrency Flood with ApacheBench
ab -n 100 -c 10 http://api.example.com/search

# 4. Inspect Shared Memory Zone Consumption via NGINX Status
# (Available via NGINX Plus API or stub_status)
```

### Understanding the Error Log Line
When NGINX rejects a request, it logs:
`limiting requests, excess: 5.200 by zone "api_strict", client: 1.2.3.4`
- `excess`: The number of requests currently overflowing the rate and burst limit (in thousandths of a request).
- `zone`: The specific shared memory zone that triggered the block.

---

## 9. Architectural Comparison Table

| Strategy | Directives | User Experience | Server Protection | Best Use Case |
| :--- | :--- | :--- | :--- | :--- |
| **Strict Policing** | `limit_req zone=x;` | Harsh (Immediate 429) | Maximum | Brute-force auth gates, payment endpoints |
| **Burst + Nodelay** | `limit_req zone=x burst=20 nodelay;` | Smooth (Fast bursts allowed) | High | Web pages loading multiple CSS/JS assets |
| **Two-Stage Shaping** | `limit_req zone=x burst=15 delay=5;` | Optimal (Fast burst, soft delay) | High | Public search queries, autocomplete APIs |
| **Dry-Run Testing** | `limit_req_dry_run on;` | Zero user impact | None (Observability only) | Calibrating new limits in production |

---

## 10. Technical Interview Questions

<details>
<summary><strong>Q1: How does NGINX's rate limiting algorithm differ from a Token Bucket algorithm?</strong></summary>

**Answer:**
- **Token Bucket**: Tokens are deposited into a bucket at a fixed rate. When a request arrives, it consumes a token. If the bucket has tokens, the request passes immediately; if empty, it is dropped. Token bucket natively allows sudden bursts up to the total capacity of the bucket without delay.
- **Leaky Bucket (NGINX)**: Models requests as water entering a bucket that leaks out at a strictly uniform, constant rate. By default, NGINX enforces this constant leak rate between every single request. To accommodate bursts, NGINX introduces the `burst` and `nodelay` parameters. 

With `burst=N nodelay`, NGINX approximates token bucket behavior: it permits an instantaneous burst of $N$ requests without delay, but strictly caps the sustained rate to the configured leak speed.
</details>

<details>
<summary><strong>Q2: Why should $binary_remote_addr always be used instead of $remote_addr in limit_req_zone?</strong></summary>

**Answer:**
- `$remote_addr` stores the client's IP address as an ASCII string. For IPv4, strings take 7 to 15 bytes; for IPv6, strings take up to 39 bytes.
- `$binary_remote_addr` stores the IP address in its native binary format: exactly **4 bytes for IPv4** and **16 bytes for IPv6**.

In NGINX, rate-limiting states are held in an in-memory red-black tree inside the shared memory zone. Using the 4-byte binary representation reduces node overhead significantly, allowing a 10MB memory zone to track over **160,000 unique client IP addresses**, compared to fewer than 60,000 using ASCII strings.
</details>

<details>
<summary><strong>Q3: Explain the exact behavioral difference between burst=10 and burst=10 nodelay.</strong></summary>

**Answer:**
Suppose a zone has a rate of `1r/s` (one request per second) and receives 10 simultaneous requests from a client at $t=0$:
- **`burst=10` (without nodelay)**:
  - Request 1 is processed immediately at $t=0$.
  - Requests 2 through 10 are placed in a queue. NGINX artificially delays them, executing Request 2 at $t=1\text{s}$, Request 3 at $t=2\text{s}$, ..., and Request 10 at $t=9\text{s}$.
  - The client experiences extreme artificial latency.
- **`burst=10 nodelay`**:
  - All 10 requests are processed **immediately at $t=0$ with zero latency**.
  - However, the bucket is now completely full (10 tokens consumed).
  - Any 11th request arriving before 1 second elapses is immediately rejected with HTTP 429.
</details>

<details>
<summary><strong>Q4: How does two-stage rate limiting work with the delay parameter?</strong></summary>

**Answer:**
Introduced in NGINX 1.15.7, the syntax `limit_req zone=NAME burst=12 delay=4;` provides two-stage traffic shaping:
1. **Stage 1 (Unthrottled Burst)**: The first 4 burst requests (`delay=4`) are processed immediately without any artificial delay.
2. **Stage 2 (Smooth Shaping)**: Requests 5 through 12 are permitted, but NGINX introduces an intentional, progressive delay (sleeping on the event loop timer) so that they leak out at the uniform rate.
3. **Stage 3 (Rejection)**: Any request arriving while the burst queue of 12 is completely full is rejected immediately with HTTP 429.
This creates a smooth degradation curve rather than an abrupt binary cutoff.
</details>

<details>
<summary><strong>Q5: How can you exempt internal office IPs or health checks from rate limiting without duplicating server blocks?</strong></summary>

**Answer:**
Leverage NGINX's rule: **if the key variable in `limit_req_zone` evaluates to an empty string `""`, rate limiting is bypassed**.

Using `geo` and `map`:
```nginx
geo $is_whitelisted {
    default        0;
    10.0.0.0/8     1;
    192.168.1.50   1;
}

map $is_whitelisted $rate_limit_key {
    0 $binary_remote_addr; # Public client: track IP
    1 "";                  # Whitelisted: empty string -> BYPASS!
}

limit_req_zone $rate_limit_key zone=api:10m rate=5r/s;
```
For whitelisted clients, `$rate_limit_key` evaluates to `""`, completely disabling rate limiting for those requests.
</details>

<details>
<summary><strong>Q6: Why is rate limiting based on $binary_remote_addr flawed when deployed behind Cloudflare, and how do you resolve it?</strong></summary>

**Answer:**
When NGINX sits behind Cloudflare, Akamai, or an AWS Application Load Balancer, the TCP connection terminating on NGINX originates from the CDN edge server's IP address, not the client's real IP. 

If `$binary_remote_addr` is used, all thousands of legitimate end users connecting through the same CDN edge node share a single rate-limiting bucket. Once the total traffic across all users exceeds the threshold, NGINX begins blocking legitimate traffic globally.

To resolve it:
1. Use the `ngx_http_realip_module` to restore the real client IP:
   ```nginx
   set_real_ip_from 173.245.48.0/20; # Cloudflare IP ranges
   real_ip_header CF-Connecting-IP;
   ```
2. Once `set_real_ip_from` is configured, NGINX replaces `$remote_addr` and `$binary_remote_addr` with the real client IP address before the rate-limiting phase executes.
</details>

<details>
<summary><strong>Q7: What is the purpose of limit_req_dry_run?</strong></summary>

**Answer:**
`limit_req_dry_run on;` (introduced in NGINX 1.17.1) enables a testing and calibration mode. 

When dry-run mode is enabled:
- NGINX tracks incoming requests in the shared memory zone and evaluates rate-limiting rules.
- If a client exceeds the limit, NGINX logs the excess event in `error.log` (as it would in production).
- However, NGINX **does not reject or delay the request**. The request is forwarded untouched to the backend.
This allows SRE teams to analyze real-world traffic patterns and verify that thresholds are not causing false positives before enforcing strict blocking.
</details>

<details>
<summary><strong>Q8: Can you apply multiple rate limits to the same request?</strong></summary>

**Answer:**
Yes. NGINX evaluates all `limit_req` directives specified in the current context sequentially. 

For example:
```nginx
limit_req zone=per_ip burst=10 nodelay;
limit_req zone=global_api burst=100 nodelay;
```
If a request violates *any* of the configured zones, the request is rejected immediately with the configured status code. The request is only permitted to proceed to the content phase if it satisfies the limits of **every** configured zone simultaneously.
</details>

---

## 11. Hands-On Practice Exercises

<details>
<summary><strong>Exercise 1: Configure a Basic 5 r/s Rate Limit Returning HTTP 429</strong></summary>

**Goal:** Create a rate limit zone for port 80 that caps requests at 5 req/sec with an RFC 6585 compliant HTTP 429 response.

**Solution:**
```nginx
limit_req_zone $binary_remote_addr zone=test_limit:10m rate=5r/s;

server {
    listen 80;
    server_name limit.local;

    limit_req_status 429;

    location / {
        limit_req zone=test_limit;
        return 200 "Request permitted\n";
    }
}
```
Test with a loop:
```bash
for i in {1..8}; do curl -s -o /dev/null -w "%{http_code}\n" http://localhost/; done
```
</details>

<details>
<summary><strong>Exercise 2: Implement Burst with Nodelay</strong></summary>

**Goal:** Modify the configuration to allow a burst of 10 requests immediately without delay.

**Solution:**
```nginx
location / {
    limit_req zone=test_limit burst=10 nodelay;
    return 200 "Burst permitted\n";
}
```
Verify that all 10 requests in a loop return HTTP 200, while the 11th returns 429.
</details>

<details>
<summary><strong>Exercise 3: Test Two-Stage Rate Limiting with delay=5</strong></summary>

**Goal:** Configure a burst of 10 where the first 5 requests are instant, and requests 6 through 10 are delayed.

**Solution:**
```nginx
location /search {
    limit_req zone=test_limit burst=10 delay=5;
    return 200 "Search result\n";
}
```
Observe curl execution timings: the first 5 return in milliseconds, while subsequent requests pause before returning.
</details>

<details>
<summary><strong>Exercise 4: Whitelist Localhost from Rate Limiting</strong></summary>

**Goal:** Use `geo` and `map` to exempt `127.0.0.1` completely from rate limiting.

**Solution:**
```nginx
geo $is_exempt {
    default   0;
    127.0.0.1 1;
}

map $is_exempt $client_key {
    0 $binary_remote_addr;
    1 "";
}

limit_req_zone $client_key zone=exempt_zone:10m rate=2r/s;

server {
    listen 80;
    location / {
        limit_req zone=exempt_zone;
        limit_req_status 429;
        return 200 "OK\n";
    }
}
```
</details>

<details>
<summary><strong>Exercise 5: Rate Limit by Custom API Key Header</strong></summary>

**Goal:** Enforce rate limits per API key (`X-API-Token`) instead of client IP.

**Solution:**
```nginx
limit_req_zone $http_x_api_token zone=token_zone:10m rate=10r/s;

server {
    listen 80;
    location /api/ {
        limit_req zone=token_zone burst=5 nodelay;
        limit_req_status 429;
        proxy_pass http://127.0.0.1:4000;
    }
}
```
</details>

<details>
<summary><strong>Exercise 6: Test Dry-Run Mode and Inspect Error Logs</strong></summary>

**Goal:** Enable `limit_req_dry_run on;`, generate an abusive flood, and verify that HTTP 200 is returned while warnings appear in `error.log`.

**Solution:**
```nginx
location /test-dry-run {
    limit_req zone=test_limit;
    limit_req_dry_run on;
    return 200 "Dry run active\n";
}
```
Run flood and verify:
```bash
tail -n 10 /var/log/nginx/error.log | grep "dry run"
```
</details>

---

## 12. Summary & Quick Reference

```nginx
# Production Rate Limiting Cheat Sheet

# 1. Define Zone in http context
# 10MB memory zone holds ~160,000 IPs at 10 requests per second
limit_req_zone $binary_remote_addr zone=API_ZONE:10m rate=10r/s;

# 2. Server Location Settings
server {
    # Always return HTTP 429 instead of default 503
    limit_req_status 429;
    limit_req_log_level warn;

    # Pattern A: Web Apps (Burst with instant execution)
    location / {
        limit_req zone=API_ZONE burst=20 nodelay;
        proxy_pass http://frontend;
    }

    # Pattern B: Sensitive APIs (Two-stage shaping)
    location /api/ {
        limit_req zone=API_ZONE burst=15 delay=5;
        proxy_pass http://backend;
    }

    # Pattern C: Brute Force Gates (Strict zero-burst policing)
    location /login {
        limit_req zone=API_ZONE;
        proxy_pass http://auth;
    }
}
```

---

## 13. Further Reading & Official Documentation
- [Official NGINX ngx_http_limit_req_module Documentation](https://nginx.org/en/docs/http/ngx_http_limit_req_module.html)
- [Rate Limiting with NGINX and NGINX Plus (NGINX Blog)](https://www.nginx.com/blog/rate-limiting-nginx/)
- [RFC 6585: Additional HTTP Status Codes (429 Too Many Requests)](https://datatracker.ietf.org/doc/html/rfc6585#section-4)
- [RFC 9110: HTTP Semantics (Idempotency and Method Handling)](https://datatracker.ietf.org/doc/html/rfc9110)

---

[← Previous: Upstream Load Balancing](02-load-balancing.md) | [Module Overview](README.md) | [Next: Auth Request and SSO Gateways →](04-auth-request-and-sso-gateways.md)
