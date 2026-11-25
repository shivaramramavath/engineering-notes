# Module 02: Core Features & Protocols

## Overview

Module 02 of the NGINX curriculum covers the core operational capabilities, protocol implementations, and edge features that transform NGINX from a basic web server into an enterprise-grade reverse proxy, API gateway, and edge security boundary.

This module provides exhaustive, production-tested guides covering modern TLS 1.3/1.2 termination, HTTP/2 and HTTP/3 (QUIC over UDP), high-performance URI rewrites with `map`, real-time WebSockets and Server-Sent Events (SSE), compression algorithms (Gzip, Brotli, static pre-compression), security headers and CORS, error handling, canonical redirections, automated ACME/Certbot PKI, and native edge JavaScript programming with `njs` and QuickJS.

---

## Architectural Protocol & Feature Pipeline

```
                               PUBLIC INTERNET CLIENTS
                                          │
       ┌──────────────────────────────────┴──────────────────────────────────┐
       │                                                                     │
       ▼ (TCP Port 80)                                                       ▼ (TCP / UDP Port 443)
┌───────────────────────────────┐                     ┌───────────────────────────────────────────────┐
│     HTTP Listener (Port 80)   │                     │        HTTPS Dual-Stack Listener (Port 443)   │
│ - ACME HTTP-01 Challenges     │                     │ - TLS 1.3 / 1.2 Termination (ECDSA & RSA)    │
│   (/.well-known/acme-challenge)                     │ - Modern HTTP/2 Multiplexing (http2 on;)      │
│ - Canonical 301 HTTPS Redir   │                     │ - HTTP/3 QUIC over UDP (listen 443 quic)      │
│   (return 301 https://$host...)                     │ - Alt-Svc Advertisement (h3=":443")           │
└──────────────┬────────────────┘                     └───────────────────────┬───────────────────────┘
               │                                                              │
               └──────────────────────────────┬───────────────────────────────┘
                                              │
                                              ▼
                             ┌─────────────────────────────────┐
                             │    Request Parsing & Routing    │
                             │ - Variables ($host, $uri, etc.) │
                             │ - map {} $var $target (O(1))    │
                             │ - Safe Rewrites (break vs last) │
                             └────────────────┬────────────────┘
                                              │
                     ┌────────────────────────┴────────────────────────┐
                     ▼                                                 ▼
      ┌─────────────────────────────┐                   ┌─────────────────────────────┐
      │     Static Asset Pipeline   │                   │    Reverse Proxy Gateway    │
      │ - brotli_static / gzip_static│                   │ - proxy_pass to Upstream    │
      │ - sendfile / tcp_nopush     │                   │ - WebSocket (HTTP 101 Map)  │
      │ - Cache-Control & Security  │                   │ - Unbuffered SSE Streaming  │
      └──────────────┬──────────────┘                   │ - proxy_intercept_errors    │
                     │                                  └──────────────┬──────────────┘
                     │                                                 │
                     └────────────────────────┬────────────────────────┘
                                              │
                                              ▼
                             ┌─────────────────────────────────┐
                             │       Outgoing Filter Chain     │
                             │ - Gzip / Brotli dynamic encode  │
                             │ - Enterprise Security Headers   │
                             │   (CSP, HSTS, X-Frame-Options)  │
                             │ - Dynamic Multi-Origin CORS     │
                             │ - njs Body & Header Filters     │
                             └────────────────┬────────────────┘
                                              │
                                              ▼
                                   STRUCTURED ACCESS LOG
                            (JSON format + upstream latency)
```

---

## Notes Catalog

| Note | Title | Difficulty | Key Topics & Directives |
| :--- | :--- | :--- | :--- |
| [01-logging.md](01-logging.md) | Structured & High-Performance Logging | Intermediate | `log_format`, JSON logging, syslog streaming, buffer tuning, `$request_id`, variable redaction |
| [02-reverse-proxy.md](02-reverse-proxy.md) | Reverse Proxying & Upstream Routing | Intermediate | `proxy_pass`, `proxy_set_header`, URI slash semantics, buffer architectures, keepalive pools |
| [03-tls-https.md](03-tls-https.md) | TLS & HTTPS Termination | Intermediate to Advanced | Modern TLS 1.3/1.2, ECDSA/RSA dual certs, OCSP stapling, session tickets, mTLS, PFS |
| [04-variables-and-rewrites.md](04-variables-and-rewrites.md) | Variables, Rewrites & Map Directive | Intermediate to Advanced | `$host`, `$request_uri`, `map` hash engine, "If is Evil" mechanics, `break` vs `last` |
| [05-websockets-and-sse-proxying.md](05-websockets-and-sse-proxying.md) | WebSocket & SSE Real-Time Proxying | Intermediate to Advanced | HTTP 101 upgrade map, `proxy_buffering off`, SSE streaming, timeout tuning, 100k sockets |
| [06-http2-and-http3-quic.md](06-http2-and-http3-quic.md) | HTTP/2 & HTTP/3 (QUIC over UDP) | Advanced | Modern `http2 on;`, `quic reuseport`, UDP 443, `Alt-Svc`, Rapid Reset DoS defense, QPACK |
| [07-compression-gzip-and-brotli.md](07-compression-gzip-and-brotli.md) | HTTP Compression: Gzip & Brotli | Intermediate to Advanced | Dynamic Gzip/Brotli, `gzip_static`, `brotli_static`, zero-copy sendfile, BREACH defense |
| [08-headers-cors-and-security-headers.md](08-headers-cors-and-security-headers.md) | Headers, CORS & Security Headers | Intermediate to Advanced | `add_header` inheritance trap, CSP, HSTS, dynamic multi-origin CORS, preflight OPTIONS |
| [09-error-handling-and-custom-pages.md](09-error-handling-and-custom-pages.md) | Error Handling & Custom Pages | Intermediate | `error_page`, `proxy_intercept_errors`, `internal;` protection, JSON APIs, maintenance mode |
| [10-redirects-and-canonical-urls.md](10-redirects-and-canonical-urls.md) | Redirects & Canonical URLs | Intermediate | 301 vs 308 permanent redirects, www consolidation, trailing slash rules, `port_in_redirect` |
| [11-acme-and-certbot-automation.md](11-acme-and-certbot-automation.md) | ACME & Certbot PKI Automation | Intermediate to Advanced | HTTP-01 webroot challenges, DNS-01 wildcards, zero-downtime reload deploy hooks, systemd |
| [12-njs-and-extensibility.md](12-njs-and-extensibility.md) | njs & Edge Extensibility | Advanced | njs per-request VM, QuickJS ES2023, JWT validation, `ngx.fetch()`, `js_body_filter` |

---

## Key Architectural Principles

1. **The Modern Protocol Baseline**: Always employ standalone `http2 on;` and dual-stack HTTP/3 QUIC over UDP port 443 with `reuseport`. Never use deprecated `listen ... http2;`.
2. **Declarative State vs Procedural Traps**: Never use `if` inside `location` blocks for anything other than `return` or `rewrite ... last;`. Leverage the `map` directive in the `http {}` context for high-performance, constant-time $O(1)$ routing decisions.
3. **The Inheritance Rule**: Remember that `add_header` completely discards all parent-level headers if even a single `add_header` is defined locally in a `location` block. Always use modular include snippets (`include /etc/nginx/snippets/security.conf;`) to preserve enterprise security postures.
4. **Real-Time Buffer Discipline**: Standard request-response traffic benefits from `proxy_buffering on;`, but real-time Server-Sent Events (SSE) and AI token streams require `proxy_buffering off;` and `proxy_cache off;` to prevent blocking.
5. **Zero-Downtime Operations**: Rotate certificates, update configurations, and deploy new workers using graceful reloads (`nginx -t && nginx -s reload`). Reserve binary upgrades (`kill -USR2`) for socket-level parameter changes like `reuseport`.

---

[← Previous Module: Core Fundamentals](../01-fundamentals/README.md) | [Track Overview](../README.md) | [Next Module: Traffic Management, Caching & Upstreams →](../03-traffic-management/README.md)
