# NGINX Learning Track: Architecture, Completion Plan & Rename Map

> **Plan Status**: Approved & Execution Ready  
> **Auditor**: Antigravity NGINX Curriculum Engineer  
> **Target Scope**: Complete 9-module curriculum, 73 technical notes, 9 module READMEs, 1 track root README (83 files total).

---

## 1. Governance & Rename Map

All existing files are mechanically renamed using `git mv` to establish strict `NN-kebab-case.md` consecutive numbering.

| Existing Source Path | Target Canonical Path | Transformation Type |
| :--- | :--- | :--- |
| `01-fundamentals/installation.md` | `01-fundamentals/01-installation.md` | Renumbered & standard prefix |
| `01-fundamentals/config-structure.md` | `01-fundamentals/02-config-structure.md` | Renumbered & standard prefix |
| `01-fundamentals/server-and-location.md` | `01-fundamentals/03-server-and-location.md` | Renumbered & standard prefix |
| `01-fundamentals/static-files.md` | `01-fundamentals/04-static-files.md` | Renumbered & standard prefix |
| `02-core-features/logging.md` | `02-core-features/01-logging.md` | Renumbered & standard prefix |
| `02-core-features/reverse-proxy.md` | `02-core-features/02-reverse-proxy.md` | Renumbered & standard prefix |
| `02-core-features/tls-https.md` | `02-core-features/03-tls-https.md` | Renumbered & standard prefix |
| `02-core-features/variables-and-rewrites.md` | `02-core-features/04-variables-and-rewrites.md` | Renumbered & standard prefix |
| `03-traffic-management/caching.md` | `03-traffic-management/01-caching.md` | Renumbered & standard prefix |
| `03-traffic-management/load-balancing.md` | `03-traffic-management/02-load-balancing.md` | Renumbered & standard prefix |
| `03-traffic-management/rate-limiting.md` | `03-traffic-management/03-rate-limiting.md` | Renumbered & standard prefix |
| `04-internals/architecture.md` | `04-internals/01-architecture-master-worker-and-event-loop.md` | Renumbered & expanded |
| `04-internals/request-lifecycle.md` | `04-internals/02-request-lifecycle-and-phases.md` | Renumbered & expanded |
| `05-security-performance/performance-tuning.md` | `05-security-performance/01-performance-tuning.md` | Renumbered & standard prefix |
| `05-security-performance/security-hardening.md` | `05-security-performance/02-security-hardening.md` | Renumbered & standard prefix |
| `06-production/docker-kubernetes.md` | `06-production/01-docker-and-containers.md` | Renumbered & split from K8s |
| `06-production/operations.md` | `06-production/03-operations-zero-downtime-reloads-and-ci.md` | Renumbered & expanded |
| `06-production/troubleshooting.md` | `06-production/04-troubleshooting-playbooks.md` | Renumbered & expanded |
| `07-projects/secure-gateway-stack.md` | `07-projects/01-secure-gateway-stack.md` | Renumbered & standard prefix |
| `07-projects/spa-with-api-proxy.md` | `07-projects/02-spa-with-api-proxy.md` | Renumbered & standard prefix |
| `08-reference/interview-questions.md` | `08-interview/01-fundamentals-qa.md` | Transformed into Module 08 |
| `08-reference/cheatsheet.md` | `09-cheatsheets/01-directives-quick-reference.md` | Transformed into Module 09 |

---

## 2. Target Curriculum Specification (83 Files)

### Track Root Documentation
- `04-devops/nginx/README.md`: Track overview, architecture map, mental models, global roadmap.
- `04-devops/nginx/ROADMAP.md`: Merged and harmonized with track root.

### Module 01: Core Fundamentals (7 notes + `README.md`)
- `01-installation.md`: Mainline vs stable, official deb/rpm repos, Docker images, compile from source with flags.
- `02-config-structure.md`: Context hierarchy (`main`, `http`, `server`, `location`), directive inheritance, `include` wildcards.
- `03-server-and-location.md`: Exact `server_name` matching, location priority (`=`, `^~`, `~*`, `~`, prefix), named locations, `try_files`.
- `04-static-files.md`: `root` vs `alias`, `index`, `autoindex`, MIME types (`mime.types`), SPA fallbacks, `open_file_cache`.
- `05-directives-reference-by-context.md`: Comprehensive index of directives allowed in main, events, http, server, and location.
- `06-command-line-and-signals.md`: CLI flags (`-t`, `-T`, `-V`, `-s`), Unix signals (`HUP`, `QUIT`, `TERM`, `USR1`, `USR2`, `WINCH`).
- `07-default-server-and-catch-all-hosts.md`: SNI fallthrough, `default_server` flag, dropping unmatched Host headers with `444`.
- `README.md`: Module overview, learning path, and catalog table.

### Module 02: Core Features & Protocols (12 notes + `README.md`)
- `01-logging.md`: Access/error log formats, JSON structured logs, conditional logging (`if=`), buffered writes, logrotate.
- `02-reverse-proxy.md`: `proxy_pass` URI replacement vs append, buffers, timeouts, headers (`Host`, `X-Forwarded-*`).
- `03-tls-https.md`: Modern TLS 1.3/1.2 baseline, OpenSSL ciphers, OCSP stapling, HSTS, session resumption, mTLS.
- `04-variables-and-rewrites.md`: Built-in variables, `map {}` block evaluation, "If is Evil" rules, `return` vs `rewrite`.
- `05-websockets-and-sse-proxying.md`: `Upgrade` and `Connection` hop-by-hop headers, proxy timeout tuning, SSE unbuffered streams.
- `06-http2-and-http3-quic.md`: Standalone `http2 on;`, HTTP/3 QUIC over UDP (`quic reuseport`), `Alt-Svc` headers, 0-RTT.
- `07-compression-gzip-and-brotli.md`: `gzip` on-the-fly vs `gzip_static`, Brotli module (`ngx_brotli`), MIME filtering, CPU tradeoffs.
- `08-headers-cors-and-security-headers.md`: `add_header` inheritance quirks, dynamic CORS whitelisting, HSTS, CSP, X-Frame-Options.
- `09-error-handling-and-custom-pages.md`: `error_page` directive, internal redirection, fallback to named locations, proxy interception.
- `10-redirects-and-canonical-urls.md`: HTTP-to-HTTPS redirection (301 vs 308), www vs non-www canonicalization, trailing slash redirects.
- `11-acme-and-certbot-automation.md`: Let's Encrypt validation via webroot (`.well-known/acme-challenge/`), Certbot automation, hooks.
- `12-njs-and-extensibility.md`: Modern njs with QuickJS engine (`js_engine qjs;`), ES2023 syntax, `ngx.fetch()`, custom auth logic.
- `README.md`: Module overview, learning path, and catalog table.

### Module 03: Traffic Management, Caching & Upstreams (9 notes + `README.md`)
- `01-caching.md`: `proxy_cache_path`, cache keys, TTL validity, `use_stale`, cache purging, stampede protection (`proxy_cache_lock`).
- `02-load-balancing.md`: Algorithms (round robin, least_conn, ip_hash, generic hash), upstream keepalive, weights, passive failover.
- `03-rate-limiting.md`: `limit_req_zone`, leaky bucket algorithm, `burst`, `nodelay` vs `delay`, `limit_conn_zone`.
- `04-auth-request-and-sso-gateways.md`: `auth_request` subrequest authentication, passing user headers, OAuth2-Proxy / Authelia.
- `05-geo-and-map-based-routing.md`: `geo` directive, MaxMind GeoIP2 integration, CIDR matching, dynamic tenant routing.
- `06-canary-and-blue-green-deployments.md`: Header-based routing, cookie-based routing, weighted upstream canary splits.
- `07-stream-module-tcp-and-udp-proxying.md`: Layer 4 load balancing (`stream {}`), TLS termination for TCP, proxy protocol support.
- `08-mirroring-and-split-clients.md`: Shadow traffic testing via `mirror`, A/B traffic distribution with `split_clients`.
- `09-sendfile-and-large-file-delivery.md`: Kernel zero-copy `sendfile`, `tcp_nopush`, `tcp_nodelay`, slice module for multi-gigabyte video.
- `README.md`: Module overview, learning path, and catalog table.

### Module 04: Internals & Architecture (6 notes + `README.md`)
- `01-architecture-master-worker-and-event-loop.md`: Master process, non-blocking worker pool, event loops (`epoll`/`kqueue`), asynchronous state machine.
- `02-request-lifecycle-and-phases.md`: The 11 HTTP processing phases (post-read, server-rewrite, find-config, rewrite, post-rewrite, pre-access, access, post-access, pre-content, content, log).
- `03-config-parsing-and-reload-mechanics.md`: Parser grammar, AST construction, validation (`nginx -t`), graceful worker migration during SIGHUP.
- `04-memory-buffers-and-shared-zones.md`: Slab allocators, shared memory zones (`shm`), connection pools, buffer limits.
- `05-modules-and-dynamic-modules.md`: Compiled vs dynamic modules (`load_module`), handlers, filters, module development overview.
- `06-reading-nginx-source-code.md`: Navigation guide to the C codebase (`src/core`, `src/http`, `src/os/unix`), key structs (`ngx_cycle_t`, `ngx_http_request_t`).
- `README.md`: Module overview, learning path, and catalog table.

### Module 05: Security Hardening & Performance Engineering (7 notes + `README.md`)
- `01-performance-tuning.md`: `worker_processes auto`, CPU affinity, `worker_connections`, file descriptor limits, socket backlog queues.
- `02-security-hardening.md`: `server_tokens off;`, buffer overflow guards (`client_max_body_size`, buffer sizes), slowloris protection.
- `03-waf-and-modsecurity-overview.md`: Web Application Firewall architecture, ModSecurity v3 with NGINX connector, OWASP Core Rule Set, Coraza WAF.
- `04-ddos-mitigation-patterns.md`: Layer 7 rate-limiting defenses, connection throttling, dropping slow clients, tarpitting.
- `05-real-ip-and-trusted-proxies.md`: `ngx_http_realip_module`, `set_real_ip_from`, `real_ip_header`, PROXY protocol support from AWS ALB / Cloudflare.
- `06-benchmarking-methodologies.md`: Benchmarking tools (`wrk`, `hey`), avoiding client-side saturation, latency percentiles (p99/p99.9).
- `07-common-vulnerabilities-in-configs.md`: Alias traversal (off-by-slash), CRLF injection in rewrites, SSRF via dynamic upstreams, CORS leaks.
- `README.md`: Module overview, learning path, and catalog table.

### Module 06: Production Operations, Cloud & Kubernetes (8 notes + `README.md`)
- `01-docker-and-containers.md`: Production Dockerfiles, non-root user execution, signal handling (PID 1 dumb-init), Docker Compose environments.
- `02-kubernetes-ingress-and-gateway-api.md`: Documentation of `kubernetes/ingress-nginx` EOL (March 2026), Gateway API architecture, NGINX Gateway Fabric.
- `03-operations-zero-downtime-reloads-and-ci.md`: Live binary upgrades without dropped connections (`USR2`/`WINCH`), GitOps validation pipelines (`nginx -t` in CI).
- `04-troubleshooting-playbooks.md`: Step-by-step diagnostic playbooks for 502 Bad Gateway, 504 Gateway Timeout, 413 Request Entity Too Large, and connection resets.
- `05-monitoring-metrics-and-observability.md`: `stub_status` metrics, Prometheus `nginx-prometheus-exporter`, OpenTelemetry tracing, Loki/ELK log aggregation.
- `06-high-availability-and-vrrp.md`: Active-passive clustering with Keepalived, Virtual Router Redundancy Protocol (VRRP), floating virtual IPs.
- `07-cdn-and-edge-integration.md`: Origin shielding, cache control headers (`s-maxage`), AWS CloudFront / Cloudflare integration patterns.
- `08-nginx-vs-alternatives.md`: Technical comparison matrix: NGINX vs Caddy vs Traefik vs HAProxy vs Envoy vs Cloud ALBs.
- `README.md`: Module overview, learning path, and catalog table.

### Module 07: Production Reference Projects (8 projects + `README.md`)
- `01-secure-gateway-stack.md`: Edge reverse proxy with TLS 1.3, rate limiting, and security headers.
- `02-spa-with-api-proxy.md`: React/Vue single-page application static host with Node/Nest API reverse proxy.
- `03-static-site-caching-and-compression.md`: High-throughput static file server with Brotli/gzip and micro-caching.
- `04-load-balanced-backend-cluster.md`: Resilient load-balanced cluster with upstream keepalives and passive health checks.
- `05-websocket-and-sse-realtime-proxy.md`: Real-time chat & telemetry proxy supporting bi-directional WebSockets and SSE.
- `06-rate-limited-public-api-gateway.md`: Public API gateway with multi-tier API key rate limiting and burst handling.
- `07-tls-termination-and-certbot-automation.md`: Automated SSL/TLS lifecycle with Certbot webroot validation and OCSP stapling.
- `08-canary-release-and-traffic-splitting.md`: Zero-downtime canary deployment lab using `split_clients` and cookie pinning.
- `README.md`: Projects catalog, topologies, and test instructions.

### Module 08: Technical Interview Preparation (8 notes + `README.md`)
- `01-fundamentals-qa.md`: Core directives, contexts, configuration grammar, process models.
- `02-location-matching-and-rewrites-qa.md`: Detailed location modifier priority, regex capture groups, rewrite rules.
- `03-reverse-proxy-and-load-balancing-qa.md`: Header forwarding, buffering trade-offs, upstream balancing algorithms.
- `04-tls-and-security-qa.md`: Handshakes, OCSP, session resumption, security headers, rate limiting.
- `05-performance-and-internals-qa.md`: Non-blocking I/O, epoll vs select, worker processes, HTTP phases.
- `06-production-troubleshooting-scenarios.md`: Real-world 502/504 incidents, disk full from proxy buffers, upstream connection exhaustion.
- `07-nginx-vs-alternatives-qa.md`: Architectural trade-offs against Envoy, HAProxy, Caddy, Traefik.
- `08-behavioural-and-architecture-discussion.md`: Designing edge ingress for a 100k RPS global microservice platform.
- `README.md`: Interview roadmap and preparation guide.

### Module 09: Quick Reference Cheatsheets (8 cheatsheets + `README.md`)
- `01-directives-quick-reference.md`: Directives catalog grouped by module and context.
- `02-location-matching-table.md`: Decision tree and lookup matrix for URI resolution.
- `03-proxy-and-headers-snippets.md`: Copy-pasteable proxy header configurations.
- `04-tls-and-ssl-snippets.md`: Modern TLS 1.3 / 1.2 hardened server blocks.
- `05-caching-and-rate-limiting-snippets.md`: Standard cache zones and rate limit blocks.
- `06-security-headers-snippets.md`: Production OWASP security header snippets.
- `07-http-error-codes-and-fixes.md`: HTTP 4xx/5xx diagnostic codes and NGINX causes.
- `08-cli-commands-and-signals.md`: CLI flags and Unix signals quick reference.
- `README.md`: Cheatsheets catalog and index.

---

## 3. Execution Batches

- **Batch 1: Module 01 Core Fundamentals** (7 concept notes + `README.md`)
- **Batch 2: Module 02 Core Features & Protocols** (12 concept notes + `README.md`)
- **Batch 3: Module 03 Traffic Management & Upstreams** (9 concept notes + `README.md`)
- **Batch 4: Module 04 Internals & Module 05 Security/Performance** (13 concept notes + 2 `README.md`s)
- **Batch 5: Module 06 Production Engineering & Kubernetes** (8 concept notes + `README.md`)
- **Batch 6: Module 07 Production Reference Projects** (8 complete projects + `README.md`)
- **Batch 7: Module 08 Interview & Module 09 Cheatsheets** (16 reference notes + 2 `README.md`s)
- **Batch 8: Track Root Integration, Link Validation & Final Report** (`README.md`, `ROADMAP.md`, `tree.txt`, `NGINX_FINAL_REPORT.md`)
