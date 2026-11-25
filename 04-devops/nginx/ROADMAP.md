# NGINX Curriculum Roadmap & Learning Pathways

> **Area:** `04-devops`  
> **Track:** `nginx`  
> **Target Version:** NGINX 1.30+ (Stable) / 1.31+ (Mainline)  
> **Status:** All 9 Modules Completed (83 Markdown Files)  
> **Last Updated:** 2026-11

---

## 1. Complete Curriculum Roadmap

The NGINX mastery track is structured into 9 comprehensive stages designed to take an engineer from initial socket binding to Staff-level production architecture, high-availability multi-cloud clustering, and kernel-level performance tuning.

```
┌────────────────────────┐      ┌────────────────────────┐      ┌────────────────────────┐
│ 01. Core Fundamentals  │─────►│ 02. Core Features      │─────►│ 03. Traffic Management │
│ Config, Location, Root │      │ Proxy, TLS, HTTP/2 & 3 │      │ Caching, Balancing, Lim│
└────────────────────────┘      └────────────────────────┘      └────────────────────────┘
            │                                                                │
            ▼                                                                ▼
┌────────────────────────┐      ┌────────────────────────┐      ┌────────────────────────┐
│ 04. Low-Level Internals│◄─────┤ 05. Security & Perf    │◄─────┤ 06. Production & Cloud │
│ Epoll, Phases, Memory  │      │ Hardening, Kernel, WAF │      │ Docker, K8s, CI/CD, HA │
└────────────────────────┘      └────────────────────────┘      └────────────────────────┘
            │                                                                │
            ▼                                                                ▼
┌────────────────────────┐      ┌────────────────────────┐      ┌────────────────────────┐
│ 07. Reference Projects │─────►│ 08. Interview Mastery  │─────►│ 09. Quick Cheatsheets  │
│ 8 Full Production Staks│      │ Tech Scenarios & Q&A   │      │ Incident Triage & Shts │
└────────────────────────┘      └────────────────────────┘      └────────────────────────┘
```

---

## 2. Stage Breakdown & Study Guides

| Stage | Module Focus | Core Deliverables & Key Documents | Recommended Study Time |
| :---: | :--- | :--- | :---: |
| **01** | **Core Fundamentals** | [01-fundamentals/README.md](./01-fundamentals/README.md)<br>• Directives, Contexts, Inheritance<br>• Location matching algorithm (`=`, `^~`, `~`, prefix)<br>• Static files, `root` vs `alias`<br>• Process signals (`HUP`, `QUIT`, `USR2`) | 4 - 6 Hours |
| **02** | **Core Features & Protocols** | [02-core-features/README.md](./02-core-features/README.md)<br>• Reverse proxying & trailing slashes<br>• Modern TLS 1.3/1.2 & OCSP Stapling<br>• Declarative `map` & avoiding `if`<br>• Standalone `http2 on;` & HTTP/3 QUIC<br>• Security headers & CORS preflight | 6 - 8 Hours |
| **03** | **Traffic Management & Upstreams** | [03-traffic-management/README.md](./03-traffic-management/README.md)<br>• High-throughput caching & Microcaching (1s)<br>• Upstream algorithms & `keepalive` pooling<br>• Leaky-bucket rate limiting (`burst`/`nodelay`/`delay`)<br>• Centralized SSO via `auth_request`<br>• Zero-copy kernel streaming (`sendfile`) | 6 - 8 Hours |
| **04** | **Internals & Architecture** | [04-internals/README.md](./04-internals/README.md)<br>• Epoll event loops & non-blocking I/O<br>• The 11 HTTP Request Phases<br>• Memory pools & Shared Zone slab allocators<br>• Dynamic C module architecture | 5 - 7 Hours |
| **05** | **Security Hardening & Performance** | [05-security-performance/README.md](./05-security-performance/README.md)<br>• Linux kernel tuning (`sysctl`, `limits.conf`)<br>• Token masking & unprivileged non-root runtime<br>• Real IP unwinding behind CDNs/ALBs<br>• OWASP ModSecurity / Coraza WAF<br>• Anti-DDoS Slowloris mitigation | 5 - 7 Hours |
| **06** | **Production Operations & Kubernetes** | [06-production-cloud-kubernetes/README.md](./06-production-cloud-kubernetes/README.md)<br>• Docker containers & non-root templating<br>• **March 2026 EOL of community ingress-nginx**<br>• Kubernetes Gateway API migration<br>• Zero-downtime CI/CD reloads<br>• Keepalived VRRP High Availability | 6 - 8 Hours |
| **07** | **Production Reference Projects** | [07-projects/README.md](./07-projects/README.md)<br>• 8 Complete end-to-end architectures<br>• Hardened Gateway, SPA Proxy, Cluster, WebSockets, Public API Gateway, TLS Automation, Canary Splitting | 10 - 15 Hours |
| **08** | **Technical Interview Preparation** | [08-interview/README.md](./08-interview/README.md)<br>• 8 Detailed Q&A modules<br>• Fundamentals, Location traps, Reverse Proxying, TLS, Epoll, Troubleshooting, Architecture | 6 - 8 Hours |
| **09** | **Quick Reference Cheatsheets** | [09-cheatsheets/README.md](./09-cheatsheets/README.md)<br>• Fast diagnostic lookups for daily operations<br>• Error codes, signals, headers, SSL ciphers | Reference / Ongoing |

---

## 3. Knowledge Checkpoints (Self-Assessment)

Before considering a stage mastered, verify that you can execute the following tasks without referencing documentation:

### Checkpoint 1: Foundations & Architecture
- [x] Predict with 100% accuracy which `location` block matches any incoming URI given exact (`=`), priority prefix (`^~`), regular expression (`~`/`~*`), and standard prefix locations.
- [x] Explain why the directive `alias /var/www/data/;` breaks if the enclosing `location /images/` lacks a trailing slash.
- [x] Describe the step-by-step memory and kernel sequence when sending a `SIGHUP` signal to an NGINX master process.

### Checkpoint 2: Protocols & Networking
- [x] Explain the exact syntax difference for HTTP/2 in NGINX 1.25.1+ (`http2 on;`) versus legacy versions (`listen 443 ssl http2;`).
- [x] Configure HTTP/3 QUIC over UDP port 443 with `quic reuseport` and provide the mandatory `Alt-Svc` discovery header.
- [x] Configure zero-copy file transfer using `sendfile`, `tcp_nopush`, and `tcp_nodelay`.

### Checkpoint 3: Proxying, Caching & Resilience
- [x] Explain the trailing slash URI replacement rules in `proxy_pass http://backend;` versus `proxy_pass http://backend/;`.
- [x] Explain why declaring `keepalive 64;` in an upstream block fails without `proxy_http_version 1.1;` and `proxy_set_header Connection "";`.
- [x] Implement a 1-second microcache with `proxy_cache_lock` and `proxy_cache_use_stale` to stop backend cache stampedes during traffic surges.
- [x] Configure `limit_req` with `burst=30 delay=10` and explain the exact delay applied to client requests.

### Checkpoint 4: Security & Edge Operations
- [x] Explain the `add_header` inheritance trap and prove how adding one header in a `location` wipes all inherited headers from `http` and `server`.
- [x] Unwind real client IP addresses across multi-proxy layers using `set_real_ip_from` and `real_ip_recursive on;`.
- [x] Explain the architecture of the Kubernetes Gateway API and describe the operational migration path away from the retired community `kubernetes/ingress-nginx` controller (EOL March 2026).

---

## 4. Fast-Track Learning Pathways

### Path A: "I need to deploy a production app this week"
1. Read [01-fundamentals/03-server-and-location.md](./01-fundamentals/03-server-and-location.md) and [04-static-files.md](./01-fundamentals/04-static-files.md).
2. Read [02-core-features/02-reverse-proxy.md](./02-core-features/02-reverse-proxy.md) and [03-tls-https.md](./02-core-features/03-tls-https.md).
3. Review reference project [07-projects/02-spa-with-api-proxy.md](./07-projects/02-spa-with-api-proxy.md).
4. Apply hardening from [05-security-performance/02-security-hardening.md](./05-security-performance/02-security-hardening.md).

### Path B: "Active production outage / triage right now"
1. Scan [09-cheatsheets/07-http-error-codes-and-fixes.md](./09-cheatsheets/07-http-error-codes-and-fixes.md) for root-cause diagnosis of 502, 504, 403, or 413.
2. Review the incident playbooks in [06-production-cloud-kubernetes/04-troubleshooting-playbooks.md](./06-production-cloud-kubernetes/04-troubleshooting-playbooks.md).
3. Check process signals and socket diagnostics in [09-cheatsheets/08-cli-commands-and-signals.md](./09-cheatsheets/08-cli-commands-and-signals.md).

### Path C: "Senior / Staff Technical Interview Prep"
1. Review all 8 interview guides in [08-interview/README.md](./08-interview/README.md).
2. Study the low-level internals in [04-internals/01-architecture-master-worker-and-event-loop.md](./04-internals/01-architecture-master-worker-and-event-loop.md) and [02-request-lifecycle-and-phases.md](./04-internals/02-request-lifecycle-and-phases.md).
3. Be prepared to defend the architecture and tradeoffs in [07-projects/01-secure-gateway-stack.md](./07-projects/01-secure-gateway-stack.md) and [07-projects/08-canary-release-and-traffic-splitting.md](./07-projects/08-canary-release-and-traffic-splitting.md).
