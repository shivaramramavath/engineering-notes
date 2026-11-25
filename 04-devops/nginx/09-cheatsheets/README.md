# Module 09: Quick Reference Cheatsheets

> **Track:** NGINX Curriculum  
> **Module ID:** `04-devops/nginx/09-cheatsheets`  
> **Module Status:** Complete (8 of 8 cheatsheets)

---

## 1. Overview & Purpose

Module 09 provides battle-tested, high-density reference guides designed for day-to-day operations, emergency incident triage, on-call troubleshooting, and rapid configuration design. Each cheatsheet condenses hundreds of pages of documentation and production experience into precise syntax blocks, decision matrices, copy-pasteable snippets, and diagnostic shell commands.

---

## 2. Cheatsheets Catalog

| Number | Cheatsheet Title | Target Topics & Highlights | Link |
| :---: | :--- | :--- | :---: |
| **01** | **Directives Quick Reference** | Core contexts (`main`, `http`, `server`, `location`, `upstream`), default values, inheritance rules, and the "If is Evil" pattern. | [01-directives-quick-reference.md](./01-directives-quick-reference.md) |
| **02** | **Location Matching Table** | Exact prefix, regex (`~`, `~*`), non-regex priority (`^~`), order of evaluation, precedence trap matrix, and debugging workflows. | [02-location-matching-table.md](./02-location-matching-table.md) |
| **03** | **Proxy & Headers Snippets** | `proxy_pass` trailing slash URI replacement rules, WebSocket upgrades, keepalive connection pooling, and multi-proxy Real IP headers. | [03-proxy-and-headers-snippets.md](./03-proxy-and-headers-snippets.md) |
| **04** | **TLS & SSL Snippets** | Modern TLS 1.3/1.2 ciphers, HTTP/2 (`http2 on;`), HTTP/3 QUIC over UDP (`quic reuseport`), OCSP Stapling, and mutual TLS (mTLS). | [04-tls-and-ssl-snippets.md](./04-tls-and-ssl-snippets.md) |
| **05** | **Caching & Rate Limiting** | Microcaching (1s TTL), Cache Lock/Background Update, `limit_req` (burst/nodelay/delay), and `limit_conn` anti-DDoS blueprints. | [05-caching-and-rate-limiting-snippets.md](./05-caching-and-rate-limiting-snippets.md) |
| **06** | **Security Headers & CORS** | HSTS preload, strict CSP, permissions-policy, CORS preflight handling via `map`, and solving the `add_header` inheritance trap. | [06-security-headers-snippets.md](./06-security-headers-snippets.md) |
| **07** | **HTTP Error Codes & Fixes** | Rapid lookup for 301 loops, 400, 403, 404 (root vs alias), 413, 414, 421, 429, 444, 500, 502, 504 with exact directive resolutions. | [07-http-error-codes-and-fixes.md](./07-http-error-codes-and-fixes.md) |
| **08** | **CLI Commands & Process Signals** | CLI flags (`-t`, `-T`, `-V`), Master-Worker signals (HUP, QUIT, USR1, USR2, WINCH), zero-downtime upgrades, systemd, and socket tracing. | [08-cli-commands-and-signals.md](./08-cli-commands-and-signals.md) |

---

## 3. Recommended Workflow

1. **For Configuration Drafting:** Start with [01-directives-quick-reference.md](./01-directives-quick-reference.md) and [02-location-matching-table.md](./02-location-matching-table.md).
2. **For Production Deployment:** Combine [03-proxy-and-headers-snippets.md](./03-proxy-and-headers-snippets.md), [04-tls-and-ssl-snippets.md](./04-tls-and-ssl-snippets.md), and [06-security-headers-snippets.md](./06-security-headers-snippets.md).
3. **For High-Traffic Resiliency:** Apply [05-caching-and-rate-limiting-snippets.md](./05-caching-and-rate-limiting-snippets.md).
4. **For Active Incident Triage:** Consult [07-http-error-codes-and-fixes.md](./07-http-error-codes-and-fixes.md) and [08-cli-commands-and-signals.md](./08-cli-commands-and-signals.md).
