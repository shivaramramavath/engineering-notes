# Module 07: Production Reference Projects

## Overview

Module 07 of the NGINX curriculum provides end-to-end, production-ready reference projects that combine the concepts, features, performance tuning, and operational practices from earlier modules into complete, copy-pasteable architectural stacks.

Every project in this module adheres strictly to the 5-section project schema:
1. **Architecture & Objective**: High-level ASCII topology, concrete operational goals, and design invariants.
2. **Complete Config Code**: Fully working, copy-pasteable configuration files, supporting snippets, and deploy scripts.
3. **Step-by-Step Walkthrough**: Deep architectural dissection of directives, memory zones, headers, and protocol interactions.
4. **Verification & Testing**: Runnable `curl`, `wscat`, and `wrk` verification commands with expected outputs.
5. **Troubleshooting & Edge Cases**: Common failure modes, triage steps, and production remediation playbooks.

---

## Projects Catalog

| # | Project Note | Primary Objective & Architecture |
| :--- | :--- | :--- |
| **01** | [Secure Edge Gateway Stack](01-secure-gateway-stack.md) | Multi-domain reverse proxy gateway with TLS 1.3, tiered rate limiting, 1-second microcaching, `least_conn` load balancing, and defensive catch-all 444 drops. |
| **02** | [SPA with API Reverse Proxy](02-spa-with-api-proxy.md) | Single-origin hosting for React/Vite SPAs and backend APIs without CORS overhead, `try_files` HTML5 pushState routing, zero-cache HTML, and 1-year immutable asset caching. |
| **03** | [Static Site Delivery with Pre-Compression](03-static-site-caching-and-compression.md) | Ultra-high throughput static file delivery using build-time Brotli (`.br`) and Gzip (`.gz`) pre-compression, zero-copy kernel `sendfile`, and `open_file_cache` memory acceleration. |
| **04** | [Resilient Microservice Backend Cluster](04-load-balanced-backend-cluster.md) | High-availability backend cluster featuring `least_conn` distribution, persistent upstream keepalive pooling, passive circuit breaking (`max_fails`), transparent retries, and emergency standby failover. |
| **05** | [Real-Time Streaming Gateway (WebSockets & SSE)](05-websocket-and-sse-realtime-proxy.md) | Persistent real-time communication gateway: hop-by-hop `Upgrade` handling for WebSockets, unbuffered zero-latency token streaming for LLM Server-Sent Events (SSE), and extended keepalive timeouts. |
| **06** | [Public API Gateway with Tiered Limits & CORS](06-rate-limited-public-api-gateway.md) | Developer API gateway with tiered rate limiting by API key (Free vs Premium), two-stage burst smoothing (`delay`), RFC 6585 HTTP 429 JSON errors, and robust edge CORS preflight handling. |
| **07** | [Automated Zero-Downtime TLS Edge with Certbot](07-tls-termination-and-certbot-automation.md) | Automated TLS certificate lifecycle management: ACME HTTP-01 webroot challenge routing (`/var/www/certbot`), zero-downtime renewal reload hooks, OCSP stapling, and Mozilla Modern SSL profile. |
| **08** | [Canary Releases & Request Shadowing](08-canary-release-and-traffic-splitting.md) | Progressive delivery router: statistical percentage canary routing via `split_clients` (MurmurHash2), cookie/header QA targeting overrides, and dark launch request shadowing via `mirror`. |

---

## Architectural Principles Enforced Across Projects

1. **Zero Raw Source Files**: All configurations, deploy scripts, and snippets reside strictly within fenced code blocks in markdown notes for portable engineering documentation.
2. **Defensive Defaults**: Catch-all default servers rejecting unknown Host headers with HTTP 444 are standard across all ingress gateways.
3. **Strict Array Header Inheritance**: Security headers (`add_header`) are safely managed via included snippets to prevent location-level inheritance traps.
4. **Upstream Connection Longevity**: All proxy configurations enforce `proxy_http_version 1.1;` and `proxy_set_header Connection "";` to maintain persistent socket pools.

---

[← Previous Module: 06-production](../06-production/README.md) | [Back to Track Index](../README.md) | [Next Module: 08-interview →](../08-interview/README.md)
