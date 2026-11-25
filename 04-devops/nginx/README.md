# NGINX Production Mastery Curriculum

> **Area:** `04-devops`  
> **Track:** `nginx`  
> **Target Version:** NGINX 1.30+ (Stable) / 1.31+ (Mainline)  
> **OpenSSL Baseline:** OpenSSL 3.0+ / TLS 1.3  
> **Curriculum Scope:** 9 Modules | 73 Concept, Project & Cheatsheet Notes | 9 Module READMEs | 1 Track Root README (83 Markdown Files)  
> **Last Verified:** 2026-11

---

## 1. Track Overview & Architecture

NGINX is the high-performance event-driven web server, reverse proxy, load balancer, and API gateway powering over a third of the world's busiest websites. Unlike traditional process-per-request or thread-per-request web servers (such as Apache MPM Prefork/Worker), NGINX is built from the ground up around non-blocking, asynchronous, event-driven I/O multiplexing (`epoll` on Linux, `kqueue` on BSD/macOS).

This curriculum provides an exhaustive, production-grade guide to NGINX: from single-server static file hosting and reverse proxying to high-availability multi-cloud clusters, zero-downtime binary upgrades, HTTP/3 QUIC deployments, custom njs scripting, and Kubernetes Gateway API architectures.

```
                              ┌─────────────────────────────────────────┐
                              │            Client Requests              │
                              │       HTTP/1.1, HTTP/2, HTTP/3 (QUIC)   │
                              └────────────────────┬────────────────────┘
                                                   │
                                                   ▼
┌─────────────────────────────────────────────────────────────────────────────────────────────────┐
│ NGINX Production Edge Gateway                                                                   │
│                                                                                                 │
│  ┌───────────────────────┐   SIGHUP    ┌─────────────────────────┐   epoll   ┌───────────────┐  │
│  │     Master Process    │────────────►│ 4x Worker Processes     │──────────►│ Non-blocking  │  │
│  │ (Privileged, Signals) │             │ (Unprivileged, Event-Lp)│           │ Event Loop    │  │
│  └───────────────────────┘             └─────────────────────────┘           └───────────────┘  │
│             │                                       │                                           │
│             ▼                                       ▼                                           │
│  ┌───────────────────────┐             ┌─────────────────────────┐                              │
│  │ Configuration Parser  │             │ Shared Memory Zones     │                              │
│  │ & Socket Allocations  │             │ (limit_req, proxy_cache)│                              │
│  └───────────────────────┘             └─────────────────────────┘                              │
└──────────────────────────────────────────────┬──────────────────────────────────────────────────┘
                                               │
                                               ▼
              ┌────────────────────────────────┴────────────────────────────────┐
              │                                                                 │
              ▼                                                                 ▼
┌───────────────────────────┐                                     ┌───────────────────────────┐
│ Static Web Content / SPAs │                                     │ Upstream Backend Clusters │
│ React / Next.js / Vue     │                                     │ Node.js, NestJS, FastAPI, │
│ /var/www/dist             │                                     │ Spring Boot, Go, Python   │
└───────────────────────────┘                                     └───────────────────────────┘
```

---

## 2. Curriculum Module Catalog

The curriculum is structured across 9 cohesive modules. Every concept note includes runnable, copy-pasteable configuration files, ASCII diagrams, a pitfall resolution table ($\ge 6$ rows), 8 interview Q&As, and 6 hands-on practice exercises.

### [Module 01: Core Fundamentals](./01-fundamentals/README.md)
*Master NGINX installation, configuration hierarchy, directive contexts, location matching algorithms, static file delivery, and process signals.*
* [01-installation.md](./01-fundamentals/01-installation.md): Package repository installation (APT/YUM), source builds, OpenSSL 3.0+ compilation, and directory layouts.
* [02-config-structure.md](./01-fundamentals/02-config-structure.md): Configuration hierarchy (`main`, `http`, `server`, `location`, `upstream`), context inheritance rules, and the `include` directive.
* [03-server-and-location.md](./01-fundamentals/03-server-and-location.md): Strict location matching precedence (`=`, `^~`, `~`, `~*`, prefix), longest prefix rules, and the "If is Evil" anti-pattern.
* [04-static-files.md](./01-fundamentals/04-static-files.md): High-speed static delivery, `root` vs `alias` path traps, MIME types (`mime.types`), directory indexing (`autoindex`), and SPA routing fallbacks.
* [05-directives-reference-by-context.md](./01-fundamentals/05-directives-reference-by-context.md): Exhaustive matrix of standard NGINX directives mapped to their permissible contexts and default behaviors.
* [06-command-line-and-signals.md](./01-fundamentals/06-command-line-and-signals.md): Master-Worker signal architecture (`HUP`, `QUIT`, `TERM`, `USR1`, `USR2`, `WINCH`) and CLI verification flags (`-t`, `-T`, `-V`).
* [07-default-server-and-catch-all-hosts.md](./01-fundamentals/07-default-server-and-catch-all-hosts.md): Default server configuration, SSL certificate fallback, dropping malicious scanner probes via HTTP 444, and preventing domain fronting.

### [Module 02: Core Features & Protocols](./02-core-features/README.md)
*Configure production reverse proxying, high-performance logging, TLS/HTTPS, WebSockets, HTTP/2, HTTP/3 QUIC, compression, CORS, and ACME automation.*
* [01-logging.md](./02-core-features/01-logging.md): High-performance logging, custom JSON log formats, buffering (`buffer=32k flush=5s`), conditional logging, and logrotate mechanics.
* [02-reverse-proxy.md](./02-core-features/02-reverse-proxy.md): Reverse proxying mechanics, `proxy_pass` trailing slash URI replacement rules, request header mutation (`proxy_set_header`), and buffer tuning.
* [03-tls-https.md](./02-core-features/03-tls-https.md): Modern SSL/TLS termination, Mozilla Modern ciphers, TLS 1.3 0-RTT tradeoffs, OCSP Stapling, and mutual TLS (mTLS) authentication.
* [04-variables-and-rewrites.md](./02-core-features/04-variables-and-rewrites.md): Built-in runtime variables, declarative `map` blocks, URL rewrite flags (`last`, `break`, `redirect`, `permanent`), and replacing dangerous `if` statements.
* [05-websockets-and-sse-proxying.md](./02-core-features/05-websockets-and-sse-proxying.md): Real-time bidirectional streaming, hop-by-hop `Upgrade` headers, Server-Sent Events (SSE) unbuffered streaming, and long-lived timeout tuning.
* [06-http2-and-http3-quic.md](./02-core-features/06-http2-and-http3-quic.md): Modern standalone `http2 on;` syntax (1.25.1+), HTTP/3 QUIC over UDP port 443 (`quic reuseport`), 0-RTT handshakes, and `Alt-Svc` headers.
* [07-compression-gzip-and-brotli.md](./02-core-features/07-compression-gzip-and-brotli.md): Wire compression tuning, `gzip_static` pre-compression, dynamic Google Brotli module, BREACH attack mitigation, and MIME-type filtering.
* [08-headers-cors-and-security-headers.md](./02-core-features/08-headers-cors-and-security-headers.md): Security headers (HSTS preload, CSP, X-Frame-Options), resolving the `add_header` inheritance trap, and multi-origin CORS preflight architectures.
* [09-error-handling-and-custom-pages.md](./02-core-features/09-error-handling-and-custom-pages.md): Intercepting upstream failures (`proxy_intercept_errors`), branded custom error pages, internal named location handlers (`@error`), and JSON error payloads.
* [10-redirects-and-canonical-urls.md](./02-core-features/10-redirects-and-canonical-urls.md): Canonical domain normalization (HTTP to HTTPS, apex to `www`), trailing slash standardization, and status code selection (301 vs 308).
* [11-acme-and-certbot-automation.md](./02-core-features/11-acme-and-certbot-automation.md): Automated Let's Encrypt certificates, webroot ACME challenge passthrough, DNS-01 validation for wildcards, and systemd renewal timers.
* [12-njs-and-extensibility.md](./02-core-features/12-njs-and-extensibility.md): Custom scripting with NGINX JavaScript (njs / QuickJS engine), JWT signature validation, dynamic routing, and comparing njs vs OpenResty (LuaJIT).

### [Module 03: Traffic Management, Caching & Upstreams](./03-traffic-management/README.md)
*Master enterprise load balancing, edge caching, throttling, single sign-on auth gateways, and deployment patterns.*
* [01-caching.md](./03-traffic-management/01-caching.md): High-throughput edge caching, `proxy_cache_path` layout, microcaching (1s TTL), Cache Lock/Background Update stampede defense, and selective cache bypassing.
* [02-load-balancing.md](./03-traffic-management/02-load-balancing.md): Upstream load balancing algorithms (Round Robin, Least Connected, IP Hash, Hash), `keepalive` connection pooling, and failover parameters.
* [03-rate-limiting.md](./03-traffic-management/03-rate-limiting.md): Leaky bucket rate limiting (`limit_req_zone`), fine-grained burst accommodation (`burst=N nodelay/delay`), multi-tier API rate limits, and client IP whitelisting.
* [04-auth-request-and-sso-gateways.md](./03-traffic-management/04-auth-request-and-sso-gateways.md): Centralized perimeter authentication (`auth_request`), OAuth2/OIDC integration (Keycloak/Authelia), and variable extraction from auth subrequests.
* [05-geo-and-map-based-routing.md](./03-traffic-management/05-geo-and-map-based-routing.md): GeoIP2 country routing, MaxMind database integration, CIDR-based client classification (`geo`), and clean routing logic via `map`.
* [06-canary-and-blue-green-deployments.md](./03-traffic-management/06-canary-and-blue-green-deployments.md): Zero-downtime deployment patterns, canary traffic splitting via `split_clients`, cookie/header-based beta tester routing, and instant rollback strategies.
* [07-connection-limiting-and-concurrency.md](./03-traffic-management/07-connection-limiting-and-concurrency.md): Concurrent connection throttling (`limit_conn`), per-IP and per-server boundaries, download bandwidth throttling (`limit_rate`), and slow-client defense.
* [08-circuit-breaking-and-upstream-health-checks.md](./03-traffic-management/08-circuit-breaking-and-upstream-health-checks.md): Passive health checking (`max_fails`, `fail_timeout`), NGINX Plus active synthetic probes, custom error intercept circuit breaking, and graceful degradation.
* [09-sendfile-and-large-file-delivery.md](./03-traffic-management/09-sendfile-and-large-file-delivery.md): Zero-copy kernel file streaming (`sendfile`, `tcp_nopush`, `tcp_nodelay`), large file slicing (`slice` module), byte-range requests, and direct I/O tuning.

### [Module 04: Internals & Architecture](./04-internals/README.md)
*Explore the low-level engine powering NGINX: process architecture, request execution phases, memory allocation, and source code.*
* [01-architecture-master-worker-and-event-loop.md](./04-internals/01-architecture-master-worker-and-event-loop.md): Process hierarchy, POSIX signal handling, asynchronous non-blocking event loops, `epoll` edge-triggered mechanics, and CPU worker pinning (`worker_cpu_affinity`).
* [02-request-lifecycle-and-phases.md](./04-internals/02-request-lifecycle-and-phases.md): The 11 internal request execution phases (`NGX_HTTP_POST_READ_PHASE` through `NGX_HTTP_LOG_PHASE`), phase handler execution rules, and why directive placement defies file line order.
* [03-config-parsing-and-reload-mechanics.md](./04-internals/03-config-parsing-and-reload-mechanics.md): Lexical analysis, context block nesting trees, compile-time vs runtime structures, and the exact step-by-step kernel and memory sequence of a graceful `SIGHUP` reload.
* [04-memory-buffers-and-shared-zones.md](./04-internals/04-memory-buffers-and-shared-zones.md): Low-level memory management, memory pools (`ngx_pool_t`), slab allocators (`ngx_slab_pool_t`), red-black trees for rate limiting, and buffer overflow prevention.
* [05-modules-and-dynamic-modules.md](./04-internals/05-modules-and-dynamic-modules.md): C module architecture (`ngx_module_t`), core vs HTTP vs stream modules, dynamic module compilation (`load_module`), and ABI compatibility constraints.
* [06-reading-nginx-source-code.md](./04-internals/06-reading-nginx-source-code.md): Navigating the official C codebase (`src/core`, `src/event`, `src/http`), data structures (`ngx_str_t`, `ngx_array_t`, `ngx_list_t`), and writing a minimal hello-world C filter module.

### [Module 05: Security Hardening & Performance Engineering](./05-security-performance/README.md)
*Bulletproof NGINX against cyberattacks and optimize operating system kernels for millions of concurrent connections.*
* [01-performance-tuning.md](./05-security-performance/01-performance-tuning.md): Linux kernel optimization (`sysctl`), file descriptor limits (`nofile`), socket buffers, TCP backlog queue sizing, and NGINX worker buffer sizing.
* [02-security-hardening.md](./05-security-performance/02-security-hardening.md): Server token masking (`server_tokens off;`), unprivileged runtime user, directory traversal defenses, buffer overflow protections, and disabling insecure HTTP verbs.
* [03-waf-and-modsecurity-overview.md](./05-security-performance/03-waf-and-modsecurity-overview.md): Web Application Firewall integration, ModSecurity v3 connector, OWASP Core Rule Set (CRS) v4, Coraza WAF alternative, and false-positive tuning.
* [04-ddos-mitigation-patterns.md](./05-security-performance/04-ddos-mitigation-patterns.md): Layer 7 DDoS mitigation, Slowloris defense via tight timeout windows, TCP SYN-flood mitigation via `syncookies`, connection concurrency limits, and automatic fail2ban / iptables blacklisting.
* [05-real-ip-and-trusted-proxies.md](./05-security-performance/05-real-ip-and-trusted-proxies.md): The `http_realip_module`, trusted proxy networks (`set_real_ip_from`), recursive header unwinding (`real_ip_recursive on;`), and stopping `X-Forwarded-For` spoofing behind CDNs and ALBs.
* [06-benchmarking-methodologies.md](./05-security-performance/06-benchmarking-methodologies.md): Accurate load testing, preventing client-side ephemeral port exhaustion, tool comparisons (wrk, vegeta, k6, ab), and latency percentile analysis (p95, p99).
* [07-common-vulnerabilities-in-configs.md](./05-security-performance/07-common-vulnerabilities-in-configs.md): Top NGINX misconfiguration security flaws: missing trailing slashes on `alias` (off-by-slash path traversal), unsafe `add_header` overwriting, CRLF injection via `$uri`, and insecure regex matching.

### [Module 06: Production Operations, Cloud & Kubernetes](./06-production-cloud-kubernetes/README.md)
*Deploy and operate NGINX across Docker, Kubernetes, systemd, multi-datacenter HA clusters, and global Content Delivery Networks.*
* [01-docker-and-containers.md](./06-production-cloud-kubernetes/01-docker-and-containers.md): Official container images (alpine vs debian), PID 1 signal forwarding (`daemon off;`), dynamic environment variable templating (`envsubst`), and unprivileged non-root containers.
* [02-kubernetes-ingress-and-gateway-api.md](./06-production-cloud-kubernetes/02-kubernetes-ingress-and-gateway-api.md): Ingress controller architecture, the critical **March 2026 EOL retirement of community `ingress-nginx`**, and the modern migration to Kubernetes Gateway API (`gateway.networking.k8s.io`) and NGINX Gateway Fabric.
* [03-operations-zero-downtime-reloads-and-ci.md](./06-production-cloud-kubernetes/03-operations-zero-downtime-reloads-and-ci.md): Zero-downtime reload mechanics, automated CI/CD config linting pipelines (`nginx -t -c`, GitHub Actions), and automated rollback strategies.
* [04-troubleshooting-playbooks.md](./06-production-cloud-kubernetes/04-troubleshooting-playbooks.md): Emergency on-call playbooks for 502 Bad Gateway, 504 Gateway Timeout, 403 Forbidden, 413 Payload Too Large, 400 Bad Request, and memory leaks.
* [05-monitoring-metrics-and-observability.md](./06-production-cloud-kubernetes/05-monitoring-metrics-and-observability.md): Open-source observability with `stub_status`, Prometheus `nginx-prometheus-exporter`, Grafana dashboards, OpenTelemetry distributed tracing, and structured JSON access log analysis.
* [06-high-availability-and-vrrp.md](./06-production-cloud-kubernetes/06-high-availability-and-vrrp.md): Active-Passive high availability, Virtual Router Redundancy Protocol (VRRP) with Keepalived, Virtual IP (VIP) failover, health tracking scripts, and split-brain prevention.
* [07-cdn-and-edge-integration.md](./06-production-cloud-kubernetes/07-cdn-and-edge-integration.md): Multi-tier edge architectures, Cloudflare / AWS CloudFront caching tiers, Origin Shielding, surrogate control headers (`Surrogate-Control`, `X-Accel-Expires`), and stale cache purging.
* [08-nginx-vs-alternatives.md](./06-production-cloud-kubernetes/08-nginx-vs-alternatives.md): Exhaustive technical trade-off matrix comparing NGINX vs Apache (httpd), HAProxy, Traefik, Caddy, Envoy Proxy, and Cloud Load Balancers (AWS ALB).

### [Module 07: Production Reference Projects](./07-projects/README.md)
*Eight complete, end-to-end production architecture reference designs adhering strictly to the 5-section engineering template.*
* [01-secure-gateway-stack.md](./07-projects/01-secure-gateway-stack.md): Multi-site hardened reverse proxy, A+ SSL Labs rating, modern ciphers, automated rate limiting, and comprehensive security headers.
* [02-spa-with-api-proxy.md](./07-projects/02-spa-with-api-proxy.md): Production Single Page Application hosting (React/Vite/Vue) with static asset immutability, client-side routing fallback, and unified backend `/api/` reverse proxying.
* [03-static-site-caching-and-compression.md](./07-projects/03-static-site-caching-and-compression.md): High-throughput static publishing platform with pre-compressed gzip/brotli, microcaching, zero-copy kernel streaming, and Cache-Control headers.
* [04-load-balanced-backend-cluster.md](./07-projects/04-load-balanced-backend-cluster.md): Resilient multi-node application cluster with least-connection balancing, upstream connection pooling (`keepalive 64`), and graceful failover.
* [05-websocket-and-sse-realtime-proxy.md](./07-projects/05-websocket-and-sse-realtime-proxy.md): Scalable real-time messaging gateway supporting persistent WebSocket connections and unbuffered Server-Sent Events (SSE).
* [06-rate-limited-public-api-gateway.md](./07-projects/06-rate-limited-public-api-gateway.md): Public-facing API gateway with multi-tier API key throttling, login brute-force defense, CIDR whitelisting, and custom JSON 429 errors.
* [07-tls-termination-and-certbot-automation.md](./07-projects/07-tls-termination-and-certbot-automation.md): Fully automated enterprise TLS termination with Certbot webroot validation, dual-certificate ECDSA/RSA fallback, OCSP Stapling, and systemd renewal timers.
* [08-canary-release-and-traffic-splitting.md](./07-projects/08-canary-release-and-traffic-splitting.md): Advanced traffic routing infrastructure supporting weighted canary rollouts via `split_clients`, beta cookie stickiness, and instant zero-downtime rollbacks.

### [Module 08: Technical Interview Preparation](./08-interview/README.md)
*Deep-dive Q&A and scenario-based interview guides covering beginner, intermediate, and staff/principal-level engineering evaluations.*
* [01-fundamentals-qa.md](./08-interview/01-fundamentals-qa.md): Core concepts, architecture, static files, and configuration syntax.
* [02-location-matching-and-rewrites-qa.md](./08-interview/02-location-matching-and-rewrites-qa.md): Complex location precedence traps, rewrite flags, and replacing `if` statements.
* [03-reverse-proxy-and-load-balancing-qa.md](./08-interview/03-reverse-proxy-and-load-balancing-qa.md): Proxy buffering, trailing slashes, keepalive pooling, and balancing algorithms.
* [04-tls-and-security-qa.md](./08-interview/04-tls-and-security-qa.md): Modern TLS handshakes, OCSP stapling, HTTP/3 QUIC, DDoS mitigation, and hardening.
* [05-performance-and-internals-qa.md](./08-interview/05-performance-and-internals-qa.md): Epoll event loops, non-blocking I/O, worker affinity, request execution phases, and kernel tuning.
* [06-production-troubleshooting-scenarios.md](./08-interview/06-production-troubleshooting-scenarios.md): Real-world on-call war stories and debugging scenarios for 502s, 504s, reload failures, and CPU spikes.
* [07-nginx-vs-alternatives-qa.md](./08-interview/07-nginx-vs-alternatives-qa.md): Architectural comparisons between NGINX, Envoy, HAProxy, Traefik, Caddy, and Apache.
* [08-behavioural-and-architecture-discussion.md](./08-interview/08-behavioural-and-architecture-discussion.md): Staff/Principal-level system design trade-offs, capacity planning, and incident post-mortems.

### [Module 09: Quick Reference Cheatsheets](./09-cheatsheets/README.md)
*High-density production cheatsheets designed for day-to-day operations and emergency incident triage.*
* [01-directives-quick-reference.md](./09-cheatsheets/01-directives-quick-reference.md): Core contexts, inheritance rules, and parameter syntaxes.
* [02-location-matching-table.md](./09-cheatsheets/02-location-matching-table.md): Precedence matrices, matching flags (`=`, `^~`, `~`, `~*`, prefix), and trap resolutions.
* [03-proxy-and-headers-snippets.md](./09-cheatsheets/03-proxy-and-headers-snippets.md): `proxy_pass` URI replacement rules, WebSocket upgrades, keepalive pools, and Real IP headers.
* [04-tls-and-ssl-snippets.md](./09-cheatsheets/04-tls-and-ssl-snippets.md): Mozilla Modern ciphers, TLS 1.3/1.2, HTTP/2, HTTP/3 QUIC, and OCSP Stapling.
* [05-caching-and-rate-limiting-snippets.md](./09-cheatsheets/05-caching-and-rate-limiting-snippets.md): Microcaching (1s TTL), Cache Lock, `limit_req` (burst/nodelay/delay), and `limit_conn`.
* [06-security-headers-snippets.md](./09-cheatsheets/06-security-headers-snippets.md): HSTS preload, strict CSP profiles, Permissions-Policy, CORS preflight, and solving header inheritance.
* [07-http-error-codes-and-fixes.md](./09-cheatsheets/07-http-error-codes-and-fixes.md): Diagnostic lookup for 301 loops, 400, 403, 404, 413, 414, 421, 429, 444, 500, 502, and 504 errors.
* [08-cli-commands-and-signals.md](./09-cheatsheets/08-cli-commands-and-signals.md): CLI flags (`-t`, `-T`, `-V`), UNIX signals (`HUP`, `QUIT`, `USR1`, `USR2`), zero-downtime binary upgrades, and socket tracing.

---

## 3. Cross-Track Integration Map

NGINX sits at the crossroads of infrastructure, networking, and backend application runtimes. Use these cross-links to navigate related topics in this repository:

| Related Area | Track & Module | Integration Point |
| :--- | :--- | :--- |
| **04-devops** | `docker/` | Containerizing NGINX, non-root execution, Docker Compose stacks. |
| **04-devops** | `linux/` | Linux kernel tuning (`sysctl`), file descriptor limits (`limits.conf`), and systemd service management. |
| **04-devops** | `aws/` | Integrating NGINX behind AWS Application Load Balancers (ALB) and CloudFront CDN. |
| **04-devops** | `git-github/` | Automated configuration testing and CI/CD validation pipelines (`nginx -t -c`). |
| **03-backend**| `nodejs/` | Reverse proxying Express/Fastify apps, handling WebSocket connections (`ws`), socket pooling. |
| **03-backend**| `nestjs/` | Edge proxying NestJS microservices, JWT authentication passthrough, API rate limiting. |
| **03-backend**| `spring-boot/` | Load balancing Spring Boot clusters, SSL offloading, sticky sessions. |
| **03-backend**| `fastapi/` | Proxying Uvicorn/Gunicorn ASGI servers, Unix domain socket communication, Server-Sent Events. |
| **07-system-design** | Multi-Tier Architecture | Layer 7 routing, CDN caching layers, API gateways, and perimeter defense. |

---

## 4. Key Architectural Standards & Invariants

To prevent standard production outages, all configurations in this track adhere to the following architectural invariants:

1. **Standalone `http2 on;` Syntax:** Under NGINX 1.25.1+, `listen ... http2;` is deprecated. Use standalone `http2 on;` inside the `server {}` context.
2. **HTTP/3 UDP Port 443:** HTTP/3 QUIC operates over UDP with `listen 443 quic reuseport;`, requiring the `Alt-Svc: h3=":443"; ma=86400` response header and OpenSSL 3.0+ / BoringSSL / quictls.
3. **The `add_header` / `proxy_set_header` Scope Rule:** If a child block (`server` or `location`) declares even one `add_header` or `proxy_set_header`, it completely ceases inheriting any headers declared in parent blocks. Modular snippet files (`include snippets/headers.conf;`) and the `always` flag are mandatory.
4. **The "If is Evil" Law:** Inside `location` blocks, the `if` directive creates hidden internal locations where content phase handlers fail. Only `return` and `rewrite ... last;` are safe inside `location if`. All complex conditional logic is moved to declarative `map` blocks in the `http {}` context.
5. **Upstream Keepalive Pairing:** Declaring `keepalive 64;` in an `upstream` block **does nothing** unless the proxy location explicitly configures `proxy_http_version 1.1;` and `proxy_set_header Connection "";` to prevent HTTP/1.0 close headers.
6. **Real IP Recursive Verification:** When operating behind multi-proxy chains (e.g. Cloudflare -> AWS ALB -> NGINX), `real_ip_recursive on;` is mandatory to unwind untrusted headers from the rightmost verified IP address and prevent client IP spoofing.
7. **Kubernetes Gateway API Evolution:** The community `kubernetes/ingress-nginx` controller enters official End-of-Life (EOL) in March 2026. Production Kubernetes architectures must transition to the Kubernetes Gateway API (`gateway.networking.k8s.io`) and NGINX Gateway Fabric.
