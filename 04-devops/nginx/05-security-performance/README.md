# Module 05: Security Hardening & Performance Engineering

## Overview

Module 05 of the NGINX curriculum covers the engineering practices required to transform NGINX into an ultra-resilient, hardened, and high-throughput security edge.

This module details high-concurrency Linux kernel and socket tuning (`somaxconn`, `epoll`, `reuseport`), multi-layered zero-trust security postures, Web Application Firewall (WAF) integration with ModSecurity v3 and the OWASP Core Rule Set, Layer 7 DDoS mitigation and connection throttling, authoritative client IP restoration behind CDNs and load balancers, scientific benchmarking methodologies avoiding Coordinated Omission, and systematic auditing of common configuration vulnerabilities.

---

## Architectural Security & Performance Perimeter

```
                               PUBLIC INTERNET / TRAFFIC
                                           │
                                           ▼
┌─────────────────────────────────────────────────────────────────────────────┐
│                      Operating System & Kernel Hardening                    │
│ - sysctl: somaxconn=65535, tcp_max_syn_backlog=65535, tcp_tw_reuse=1       │
│ - Netfilter: ipset constant-time IP dropping + fail2ban integration         │
│ - Security: unprivileged worker user (www-data), locked file permissions    │
└─────────────────────────────────────┬───────────────────────────────────────┘
                                      │
                                      ▼
┌─────────────────────────────────────────────────────────────────────────────┐
│                      Edge Defense & Catch-All Protection                    │
│ - Catch-All Default Server: listen 80/443 default_server; return 444;       │
│ - Anti-Slowloris Timeouts: client_header_timeout 5s; reset_timedout_conn on │
│ - Connection & Rate Caps: limit_conn 20; limit_req burst=30 nodelay (429)   │
└─────────────────────────────────────┬───────────────────────────────────────┘
                                      │
                                      ▼
┌─────────────────────────────────────────────────────────────────────────────┐
│                      Identity & Real IP Chain Restoration                   │
│ - ngx_http_realip_module: set_real_ip_from CIDRs (Cloudflare, AWS ALB)     │
│ - Recursive Chain Validation: real_ip_recursive on (Prevents IP spoofing)   │
│ - PROXY Protocol v1/v2: Hardware/L4 NLB origin address restoration          │
└─────────────────────────────────────┬───────────────────────────────────────┘
                                      │
                                      ▼
┌─────────────────────────────────────────────────────────────────────────────┐
│                      Deep Packet Inspection (WAF Layer)                     │
│ - ModSecurity v3 (libmodsecurity connector) / OWASP Coraza                  │
│ - OWASP Core Rule Set (CRS v3/v4) Anomaly Scoring Engine                    │
│ - SQLi, XSS, RCE, LFI Detection + Virtual Patching Rules                    │
└─────────────────────────────────────┬───────────────────────────────────────┘
                                      │
                                      ▼
┌─────────────────────────────────────────────────────────────────────────────┐
│                      High-Throughput Content & Upstream Core                │
│ - Zero-Copy: sendfile on; tcp_nopush on; tcp_nodelay on;                    │
│ - Open File Cache: open_file_cache max=100000 inactive=60s;                 │
│ - Connection Pooling: upstream keepalive 256; proxy_http_version 1.1;       │
│ - In-Memory Buffered Logging: access_log buffer=1m flush=5s;                │
└─────────────────────────────────────┬───────────────────────────────────────┘
                                      │
                                      ▼
                             PROTECTED UPSTREAMS
```

---

## Module Notes Catalog

| # | Note | Key Topics Covered |
| :--- | :--- | :--- |
| **01** | [Performance Tuning & Kernel Optimization](01-performance-tuning.md) | High-concurrency Linux kernel `sysctl` parameters, worker affinity pinning, file descriptor limits, socket geometry, zero-copy `sendfile`, keepalive connection reuse. |
| **02** | [Security Hardening & Zero-Trust Architecture](02-security-hardening.md) | `server_tokens off`, catch-all `default_server` (HTTP 444), modern TLS 1.3/1.2 ciphers, HSTS preloading, CSP, path shielding (`.git`, `.env`), least-privilege permissions. |
| **03** | [WAF Integration & ModSecurity Overview](03-waf-and-modsecurity-overview.md) | ModSecurity v3 (`libmodsecurity`), OWASP Core Rule Set (CRS), anomaly scoring, paranoia levels, tuning false positives, targeted exclusions, OWASP Coraza comparison. |
| **04** | [DDoS Mitigation Patterns & Edge Defense](04-ddos-mitigation-patterns.md) | Layer 7 mitigation, HTTP floods, Slowloris, Slow POST, Leaky Bucket rate limiting (`limit_req`), connection caps (`limit_conn`), `fail2ban` + `ipset` kernel drops. |
| **05** | [Real IP Resolution & Trusted Proxies](05-real-ip-and-trusted-proxies.md) | `ngx_http_realip_module`, `set_real_ip_from`, `real_ip_recursive on`, PROXY Protocol v1/v2, AWS ALB/NLB, Cloudflare CIDR synchronization, IP spoofing prevention. |
| **06** | [Benchmarking Methodologies & Profiling](06-benchmarking-methodologies.md) | Open-loop vs closed-loop testing, Gil Tene's Coordinated Omission, `wrk`/`wrk2`, `k6`, HDR latency histograms ($p99/p99.9$), Linux `perf`, Brendan Gregg on-CPU Flame Graphs. |
| **07** | [Common Vulnerabilities & Config Auditing](07-common-vulnerabilities-in-configs.md) | Off-by-slash alias traversal, CRLF header injection in redirects (`$uri` vs `$request_uri`), dynamic SSRF proxying, Host header cache poisoning, static analysis with GIXY. |

---

## Core Mental Models & Principles

1. **Defense in Depth**: Security must be enforced in layers—at the kernel firewall (Netfilter/ipset), socket layer (connection limits), HTTP protocol boundary (catch-all 444), application layer (WAF), and operating system sandbox.
2. **Open-Loop Latency Measurement**: Never trust closed-loop benchmarks like `ab`. Authentic latency measurement requires constant-arrival rate generation (`wrk2`) to account for Coordinated Omission.
3. **Right-to-Left Real IP Validation**: In multi-proxy chains, never trust the leftmost IP in `X-Forwarded-For`. Trust must unwind recursively from the rightmost verified hop to prevent header forgery.
4. **Declarative State Over Procedural Logic**: Avoid procedural `if` blocks inside `location` directives. Use declarative `map` structures in the `http` context for predictable, zero-regression routing.

---

[← Previous Module: 04-internals](../04-internals/README.md) | [Back to Track Index](../README.md) | [Next Module: 06-production-operations →](../06-production-operations/README.md)
