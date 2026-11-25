# NGINX Learning Track: Comprehensive Structural & Technical Audit

> **Audit Status**: Complete  
> **Auditor**: Antigravity NGINX Curriculum Engineer & Technical Reviewer  
> **Date**: November 2026  
> **Target Scope**: `04-devops/nginx/` (24 existing files, 8 subdirectories)

---

## 1. Executive Summary

An exhaustive automated and technical analysis of `04-devops/nginx/` reveals significant architectural deficiencies, outdated directives, and substantial curriculum gaps:

1. **Unnumbered Files & Flat Naming Violations**: Every file within the module subdirectories is unnumbered (e.g. `installation.md`, `config-structure.md`), violating the repository-wide `NN-kebab-case.md` specification.
2. **Missing Per-Module READMEs**: None of the 8 module directories contain a `README.md` file. Navigation, learning objectives, and catalog overviews are absent.
3. **Template Non-Conformance**: Almost all concept notes (14 out of 22) are stubs under 140 lines. None of the existing notes implement the mandatory 13-section DevOps concept schema (metadata table, TOC, internal packet/event loop diagrams, 5 progressive examples, 6+ common mistakes table, 8 interview questions in `<details>`, 6 practice exercises with solutions, cheat sheet, and further reading links).
4. **Outdated Directives & Security Risks**:
   - Multiple notes (`tls-https.md`, `performance-tuning.md`, `secure-gateway-stack.md`, `cheatsheet.md`) use the legacy, deprecated `listen 443 ssl http2;` parameter instead of the modern standalone `http2 on;` directive introduced in NGINX 1.25.1+.
   - Missing explicit HTTP/3 (QUIC) configuration, `quic reuseport` UDP socket semantics, and `Alt-Svc` headers.
   - Kubernetes documentation (`docker-kubernetes.md`) fails to account for the official **March 2026 End-of-Life (EOL) retirement** of the `kubernetes/ingress-nginx` project and its successor, the **Kubernetes Gateway API** (NGINX Gateway Fabric).
   - Insecure alias traversal patterns ("off-by-slash") are insufficiently warned or present in examples.
5. **Reference Directory Anti-pattern**: `08-reference/` contains loose files (`cheatsheet.md` and `interview-questions.md`). Under `04-devops/CONVENTIONS.md`, reference content must be partitioned into numbered modules: `08-interview/` (comprehensive categorized Q&A banks) and `09-cheatsheets/` (task-oriented syntax sheets).

---

## 2. Granular Inventory & Assessment Table

| Existing File Path | Lines | Template Status | Technical & Architectural Findings | Recommended Action |
| :--- | :---: | :---: | :--- | :--- |
| `README.md` | 72 | Stub | Outdated track summary; lacks modern roadmap, metrics, and module links | Rewrite & expand to track overview standard |
| `ROADMAP.md` | 101 | Stub | Separate unaligned roadmap file; duplicates README topics | Merge into track README / ROADMAP |
| `01-fundamentals/installation.md` | 73 | Stub | Minimal package manager mentions; no mainline vs stable comparison, build from source, or signature verification | Rename to `01-installation.md`, rewrite to 13 sections |
| `01-fundamentals/config-structure.md` | 101 | Stub | Lacks context inheritance rules, `include` wildcards, and signals | Rename to `02-config-structure.md`, rewrite to 13 sections |
| `01-fundamentals/server-and-location.md`| 93 | Stub | Superficially covers location matching; misses priority tiers (`=`, `^~`, `~*`, prefix), named locations, and `try_files` edge cases | Rename to `03-server-and-location.md`, rewrite to 13 sections |
| `01-fundamentals/static-files.md` | 101 | Stub | Thin coverage of `root` vs `alias`; misses SPA fallback routing, `open_file_cache`, and MIME handling | Rename to `04-static-files.md`, rewrite to 13 sections |
| `02-core-features/logging.md` | 103 | Stub | Lacks JSON structured logging formats, conditional access logging, and high-performance buffered logging | Rename to `01-logging.md`, rewrite to 13 sections |
| `02-core-features/reverse-proxy.md` | 107 | Stub | Missing WebSocket upgrade map, SSE chunked buffering, real IP headers (`X-Forwarded-*`), and timeout architecture | Rename to `02-reverse-proxy.md`, rewrite to 13 sections |
| `02-core-features/tls-https.md` | 106 | Stub | Uses deprecated `listen ... http2;`; missing OCSP stapling, modern TLS 1.3 ciphers, and session tickets | Rename to `03-tls-https.md`, rewrite to 13 sections |
| `02-core-features/variables-and-rewrites.md` | 117 | Stub | Fails to comprehensively document "If is Evil"; lacks `map` block evaluation mechanics and `return` vs `rewrite` rules | Rename to `04-variables-and-rewrites.md`, rewrite to 13 sections |
| `03-traffic-management/caching.md` | 127 | Stub | Missing cache revalidation, `proxy_cache_use_stale`, cache purging mechanics, and `proxy_cache_lock` | Rename to `01-caching.md`, rewrite to 13 sections |
| `03-traffic-management/load-balancing.md` | 132 | Stub | Missing upstream keepalive tuning, session persistence options, and active vs passive health checks | Rename to `02-load-balancing.md`, rewrite to 13 sections |
| `03-traffic-management/rate-limiting.md` | 137 | Stub | Missing two-stage rate limiting, `burst` with `nodelay` vs `delay`, and connection limiting (`limit_conn`) | Rename to `03-rate-limiting.md`, rewrite to 13 sections |
| `04-internals/architecture.md` | 123 | Stub | Missing epoll / kqueue event loops, non-blocking asynchronous state machines, and master/worker IPC | Rename to `01-architecture.md`, rewrite to 13 sections |
| `04-internals/request-lifecycle.md` | 131 | Stub | Does not detail the 11 internal NGINX HTTP processing phases | Rename to `02-request-lifecycle.md`, rewrite to 13 sections |
| `05-security-performance/performance-tuning.md` | 210 | Partial | Outdated HTTP/2 syntax; missing TCP tuning (`tcp_nopush`, `tcp_nodelay`), backlog queues, and worker CPU affinity | Rename to `01-performance-tuning.md`, rewrite to 13 sections |
| `05-security-performance/security-hardening.md` | 221 | Partial | Lacks comprehensive CSP headers, alias off-by-slash traversal examples, and ModSecurity/Coraza integration | Rename to `02-security-hardening.md`, rewrite to 13 sections |
| `06-production/docker-kubernetes.md` | 309 | Partial | Does not document `kubernetes/ingress-nginx` EOL retirement (March 2026) or Kubernetes Gateway API | Split/Rename to `01-docker-and-containers.md` & `02-kubernetes-ingress-and-gateway-api.md` |
| `06-production/operations.md` | 229 | Partial | Lacks zero-downtime binary upgrade procedure (`USR2` -> `WINCH` -> `QUIT`), CI validation pipelines | Rename to `03-operations.md`, rewrite to 13 sections |
| `06-production/troubleshooting.md` | 201 | Partial | Playbooks lack step-by-step diagnostic trees for 502 Bad Gateway, 504 Gateway Timeout, and 413 Payload Too Large | Rename to `04-troubleshooting.md`, rewrite to 13 sections |
| `07-projects/secure-gateway-stack.md` | 526 | Partial | Monolithic configuration file; uses deprecated `listen ... http2;`; lacks modular test harness | Restructure into numbered project format |
| `07-projects/spa-with-api-proxy.md` | 305 | Partial | Lacks production error handling, compression configuration, and security headers | Restructure into numbered project format |
| `08-reference/cheatsheet.md` | 414 | Partial | Unnumbered reference file; mixes directives, commands, and snippets | Split into dedicated sheets in `09-cheatsheets/` |
| `08-reference/interview-questions.md` | 352 | Partial | Unnumbered reference file; lacks categorized scenarios and code debugging challenges | Split into categorized Q&A notes in `08-interview/` |

---

## 3. Curriculum Gaps Identified

The following mandatory enterprise topics are completely missing from the existing track:

1. **Protocol Modernization**:
   - HTTP/3 (QUIC) full implementation with `ngx_http_v3_module`, UDP listeners, and `reuseport`.
   - Standalone `http2 on;` syntax transition.
2. **Advanced Proxy & Application Protocols**:
   - WebSocket proxying (`Upgrade` and `Connection` hop-by-hop header handling via `map`).
   - Server-Sent Events (SSE) and streaming proxying (`proxy_buffering off;`).
   - gRPC proxying (`grpc_pass`) and bidirectional streaming.
3. **Advanced Traffic Routing & Gateway Features**:
   - `auth_request` subrequest authentication (SSO, OAuth2 Proxy, Authelia integration).
   - Layer 4 TCP/UDP proxying via the `stream {}` module.
   - GeoIP2 and map-based traffic routing.
   - Canary releases and A/B testing via `split_clients`.
   - Request mirroring via `mirror`.
4. **Security & WAF**:
   - Deep dive into NGINX configuration vulnerabilities: alias traversal (off-by-slash), CRLF injection, unsafe variable usage in rewrites.
   - Web Application Firewalls (ModSecurity / OWASP Core Rule Set / Coraza).
5. **Modern Kubernetes Architecture**:
   - The end-of-life retirement of `kubernetes/ingress-nginx` (March 2026).
   - Migration to the Kubernetes Gateway API using **NGINX Gateway Fabric**.
6. **Extensibility & Scripting**:
   - NGINX JavaScript (njs) using the modern QuickJS engine (`js_engine qjs;`).
7. **Production Projects**:
   - Expanding from 2 basic projects to 8 end-to-end reference architectures with complete, runnable code blocks.

---

## 4. Renaming & Directory Restructuring Plan

1. **Number all concept notes** with two-digit prefixes (`01-`, `02-`, etc.).
2. **Add a mandatory `README.md`** in each of the 8 module folders.
3. **Migrate `08-reference/`** into:
   - `08-interview/` (8 categorized technical interview notes).
   - `09-cheatsheets/` (8 high-density reference sheets).
4. **Expand `07-projects/`** to include 8 comprehensive reference projects.
