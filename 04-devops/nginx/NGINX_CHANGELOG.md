# NGINX Learning Track: Changelog

This document tracks all architectural modifications, note rewrites, file renames, deprecated directive removals, and technical corrections executed within `04-devops/nginx/`.

---

## [1.0.0] - November 2026

### Complete Curriculum Delivery (83 Markdown Files)
- **Total Deliverables**: 73 Notes + 9 Module READMEs + 1 Track Root README = 83 Markdown Files.
- All notes conform strictly to the 13-section concept schema (400-700 lines, runnable configs, ASCII architecture diagrams, $\ge 6$ row pitfalls table, 8 interview Q&As, 6 practice exercises).
- Reference projects conform strictly to the 5-section project schema (Architecture, Complete Code, Walkthrough, Verification, Troubleshooting).

### Module Releases
- **Module 01: Core Fundamentals** (7 notes + `README.md`):
  - Added installation recipes across APT, YUM, and source builds with OpenSSL 3.0+.
  - Comprehensive coverage of directive contexts, location matching algorithms (`=`, `^~`, `~`, `~*`, prefix), `root` vs `alias` traps, directive reference tables, Master-Worker signal handling, and default catch-all servers dropping malicious scans via HTTP 444.
- **Module 02: Core Features & Protocols** (12 notes + `README.md`):
  - Advanced logging with JSON formats, microsecond upstream timing, and buffer flushing.
  - Reverse proxying with trailing slash URI rewrite rules, buffer tuning, and WebSocket upgrades.
  - Modern TLS 1.3/1.2 termination, OCSP Stapling, and mutual TLS (mTLS).
  - Declarative `map` logic to eliminate unsafe `if` statements.
  - Real-time WebSockets & SSE streaming proxying.
  - Standalone `http2 on;` (1.25.1+) and HTTP/3 QUIC over UDP (`quic reuseport`).
  - Wire compression with `gzip_static` and dynamic Google Brotli module.
  - Hardened security headers, solving `add_header` inheritance, and multi-origin CORS preflight.
  - Custom error handling (`proxy_intercept_errors`), canonical URL redirects, automated ACME Certbot renewals, and njs QuickJS scripting.
- **Module 03: Traffic Management, Caching & Upstreams** (9 notes + `README.md`):
  - Edge caching, 1-second microcaching, cache stampede defense (`proxy_cache_lock`).
  - Upstream load balancing algorithms and `keepalive` connection pooling.
  - Leaky-bucket rate limiting (`limit_req_zone`, `burst`, `nodelay`, `delay`).
  - Perimeter authentication via `auth_request` and SSO gateways.
  - GeoIP2 database classification and declarative `map` routing.
  - Canary releases and traffic splitting via `split_clients`.
  - Concurrency connection limiting (`limit_conn`) and bandwidth throttling (`limit_rate`).
  - Circuit breaking, upstream health checks, and zero-copy `sendfile` kernel streaming.
- **Module 04: Internals & Architecture** (6 notes + `README.md`):
  - Master-Worker process hierarchy, POSIX signals, asynchronous non-blocking event loops (`epoll`).
  - Exhaustive walk-through of the 11 HTTP Request Execution Phases.
  - Lexical parsing, configuration tree compilation, and atomic `SIGHUP` reload sequence.
  - Low-level memory management: memory pools (`ngx_pool_t`) and slab allocators (`ngx_slab_pool_t`).
  - Dynamic C modules (`ngx_module_t`) and source code walkthrough (`src/core`, `src/event`, `src/http`).
- **Module 05: Security Hardening & Performance Engineering** (7 notes + `README.md`):
  - Linux kernel parameter tuning (`sysctl`, `limits.conf`, socket backlogs).
  - Production server hardening, token masking, and unprivileged user isolation.
  - Web Application Firewalls: ModSecurity v3 and OWASP CRS v4.
  - Anti-DDoS patterns against Slowloris, SYN floods, and HTTP floods.
  - Real IP recursive unwinding (`real_ip_recursive on;`) behind CDNs and ALBs.
  - Accurate benchmarking methodologies (wrk, vegeta, k6, ab) and latency percentiles.
  - Top configuration vulnerabilities: off-by-slash `alias` traversal, header overwrites, and CRLF injection.
- **Module 06: Production Operations, Cloud & Kubernetes** (8 notes + `README.md`):
  - Docker containers, non-root runtimes, and dynamic config templating.
  - Documentation of the **March 2026 EOL retirement of community `ingress-nginx`** and production migration to Kubernetes Gateway API (`gateway.networking.k8s.io`) and NGINX Gateway Fabric.
  - Zero-downtime reloads and automated CI/CD linting pipelines (`nginx -t -c`).
  - Incident troubleshooting playbooks for 502, 504, 403, 413, 400, and memory leaks.
  - Open-source observability with `stub_status`, Prometheus exporter, and Grafana.
  - Active-Passive High Availability with Keepalived VRRP and Virtual IPs.
  - Multi-tier CDN integration, Cloudflare / CloudFront caching, and surrogate headers.
  - Comprehensive comparison matrix: NGINX vs HAProxy, Traefik, Caddy, Envoy, and Apache.
- **Module 07: Production Reference Projects** (8 projects + `README.md`):
  - Complete runnable architectures for Secure Gateway Stack, SPA + API Proxy, Static Site Caching, Load Balanced Backend Cluster, WebSocket & SSE Realtime Proxy, Rate-Limited Public API Gateway, TLS Termination with Certbot Automation, and Canary Release Traffic Splitting.
- **Module 08: Technical Interview Preparation** (8 notes + `README.md`):
  - Over 60 in-depth technical interview questions across 8 domains: Fundamentals, Location matching, Reverse proxying, TLS/Security, Internals/Epoll, Incident Troubleshooting Scenarios, Alternative Proxies, and Staff-level System Design & Behavioural discussions.
- **Module 09: Quick Reference Cheatsheets** (8 cheatsheets + `README.md`):
  - High-density operational cheatsheets: Directives reference, Location matching table, Proxy & Headers snippets, TLS & SSL snippets, Caching & Rate Limiting snippets, Security Headers & CORS snippets, HTTP Error Codes & Rapid Fixes, and CLI Commands & Process Signals.

### Architecture Invariants Enforced
- Deprecated `listen ... http2;` removed; replaced with modern standalone `http2 on;`.
- HTTP/3 QUIC standard implemented with UDP port 443 `quic reuseport` and `Alt-Svc`.
- `add_header` / `proxy_set_header` inheritance traps resolved with modular snippet files and `always` flags.
- Dangerous `if` statements inside `location` replaced with declarative `map` in `http`.
- Upstream keepalive pools strictly paired with `proxy_http_version 1.1;` and `proxy_set_header Connection "";`.
- Multi-proxy Real IP chains recursively unwound to stop `X-Forwarded-For` spoofing.
- Community `ingress-nginx` retirement documented with Gateway API paths.
