# Module 03: Traffic Management, Caching & Upstreams

## Overview

Module 03 of the NGINX curriculum covers the advanced traffic routing, caching, concurrency control, and reliability engineering mechanisms that enable NGINX to govern production traffic at massive scale.

This module details reverse proxy HTTP caching and microcaching, upstream load-balancing algorithms, token/leaky-bucket rate limiting, edge identity gateways (`auth_request`), CIDR/GeoIP2 traffic steering, canary and blue-green continuous delivery patterns, connection limiting, passive circuit breaking with failover retries, and high-throughput zero-copy static delivery with `sendfile`, `aio threads`, and `directio`.

---

## Architectural Traffic Governance Pipeline

```
                               PUBLIC INTERNET CLIENTS
                                          │
                                          ▼
┌─────────────────────────────────────────────────────────────────────────────┐
│                       Edge Concurrency & Rate Limiting                      │
│ - limit_conn (Per-IP & Per-Server active socket caps)                       │
│ - limit_req  (Leaky bucket traffic shaping: rate, burst, nodelay, delay=N)  │
│ - Slowloris Mitigation (Tuned client timeouts, reset_timedout_connection)   │
└─────────────────────────────────────┬───────────────────────────────────────┘
                                      │
                                      ▼
┌─────────────────────────────────────────────────────────────────────────────┐
│                       Routing & Geographic Steering                         │
│ - geo (Radix tree CIDR matching for internal/VPC classification)            │
│ - geoip2 (MaxMind .mmdb country, continent, and city lookup)                │
│ - split_clients (MurmurHash2 statistical percentage canary deployments)     │
│ - map (Multi-variable composite routing, GDPR data sovereignty)             │
└─────────────────────────────────────┬───────────────────────────────────────┘
                                      │
                                      ▼
┌─────────────────────────────────────────────────────────────────────────────┐
│                       Zero-Trust Edge Identity Gateway                      │
│ - auth_request /auth_check (Internal asynchronous subrequests)              │
│ - OAuth2-Proxy / Keycloak / Authelia Single Sign-On                         │
│ - auth_request_set (Capture user claims and inject upstream headers)        │
│ - Subrequest Caching (60s auth decision caching)                            │
└─────────────────────────────────────┬───────────────────────────────────────┘
                                      │
                                      ▼
┌─────────────────────────────────────────────────────────────────────────────┐
│                       HTTP Reverse Proxy Caching Layer                      │
│ - proxy_cache_path (Shared memory keys_zone + atomic disk cache)            │
│ - Microcaching (1-second dynamic caching under extreme load)                │
│ - proxy_cache_lock (Thundering herd / cache stampede defense)               │
│ - proxy_cache_use_stale (Serve stale while background update runs)          │
│ - slice (Byte-range media chunk caching)                                    │
└─────────────────────────────────────┬───────────────────────────────────────┘
                                      │
                                      ▼
┌─────────────────────────────────────────────────────────────────────────────┐
│                       Upstream Load Balancing & Resiliency                  │
│ - Algorithms: Round Robin, least_conn, ip_hash, hash ... consistent         │
│ - Passive Circuit Breaking (max_fails, fail_timeout)                        │
│ - proxy_next_upstream (Capped failover retries across nodes)                │
│ - keepalive (Pre-warmed persistent connection pooling to backends)          │
│ - Emergency Standby Nodes (server ... backup)                               │
└─────────────────────────────────────┬───────────────────────────────────────┘
                                      │
                                      ▼
                             BACKEND MICROSERVICES
                   (Node.js, Spring Boot, FastAPI, Go, Storage)
```

---

## Notes Catalog

| Note | Title | Difficulty | Key Topics & Directives |
| :--- | :--- | :--- | :--- |
| [01-caching.md](01-caching.md) | HTTP Caching & Microcaching | Intermediate to Advanced | `proxy_cache_path`, `keys_zone`, microcaching, `proxy_cache_lock`, `proxy_cache_use_stale`, slice ranges |
| [02-load-balancing.md](02-load-balancing.md) | Upstream Load Balancing | Intermediate to Advanced | Round Robin, `least_conn`, `ip_hash`, Ketama `consistent`, `keepalive` pools, dynamic DNS resolution |
| [03-rate-limiting.md](03-rate-limiting.md) | Rate Limiting & Traffic Shaping | Intermediate to Advanced | Leaky bucket, `limit_req_zone`, `burst`, `nodelay`, `delay=N`, HTTP 429 status, `geo`/`map` whitelisting |
| [04-auth-request-and-sso-gateways.md](04-auth-request-and-sso-gateways.md) | Auth Request & SSO Gateways | Advanced | `auth_request`, subrequests, OAuth2-Proxy, Keycloak, `auth_request_set`, auth decision caching |
| [05-geo-and-map-based-routing.md](05-geo-and-map-based-routing.md) | GeoIP & Map-Based Routing | Intermediate to Advanced | `geo` CIDR radix trees, MaxMind GeoIP2 `.mmdb`, GDPR data routing, geofencing (HTTP 451), composite maps |
| [06-canary-and-blue-green-deployments.md](06-canary-and-blue-green-deployments.md) | Canary & Blue-Green Deployments | Advanced | `split_clients` MurmurHash2 canary, blue-green cutover scripts, cookie overrides, `mirror` shadow traffic |
| [07-connection-limiting-and-concurrency.md](07-connection-limiting-and-concurrency.md) | Connection Limiting & Concurrency | Intermediate to Advanced | `limit_conn`, active socket caps, `limit_rate` bandwidth throttling, `limit_rate_after`, Slowloris defense |
| [08-circuit-breaking-and-upstream-health-checks.md](08-circuit-breaking-and-upstream-health-checks.md) | Circuit Breaking & Health Checks | Advanced | Passive health checks (`max_fails`, `fail_timeout`), circuit breaker state machine, `proxy_next_upstream`, backup nodes |
| [09-sendfile-and-large-file-delivery.md](09-sendfile-and-large-file-delivery.md) | Sendfile, Direct I/O & Large Files | Advanced | Kernel zero-copy `sendfile()`, `tcp_nopush`, `tcp_nodelay`, `aio threads` pools, `directio` page cache bypass |

---

## Key Architectural Principles

1. **Defense in Depth**: Protect backend services through layered governance: restrict concurrency with `limit_conn`, shape request arrival bursts with `limit_req`, and enforce authentication at the edge with `auth_request`.
2. **Preventing Cascading Failures**: Never allow failover retries to run unbounded. Always configure `proxy_next_upstream_tries` and `proxy_next_upstream_timeout` alongside `proxy_connect_timeout 2s;`.
3. **Thundering Herd Protection**: High-traffic dynamic endpoints must always configure `proxy_cache_lock on;` and `proxy_cache_use_stale updating;` to ensure that cache expirations never trigger backend resource exhaustion.
4. **Connection Pooling Invariant**: Upstream `keepalive` reduces network latency and CPU usage by over 60%, but strictly requires `proxy_http_version 1.1;` and `proxy_set_header Connection "";` to prevent premature socket termination.
5. **Zero-Copy Line-Rate Throughput**: When serving static media, pair `sendfile on;` with `tcp_nopush on;` and `sendfile_max_chunk 1m;`, offloading blocking reads to background thread pools with `aio threads=default;`.

---

[← Previous Module: Core Features & Protocols](../02-core-features/README.md) | [Track Overview](../README.md) | [Next Module: Internals & Architecture →](../04-internals/README.md)
