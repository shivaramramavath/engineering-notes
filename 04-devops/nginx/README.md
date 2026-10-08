# nginx Mastery

A structured, practical guide to nginx: from first install to production operations, internals, and interview preparation. It works two ways:

- **Learning:** follow the numbered folders in order.
- **Reference:** jump straight to a topic using the index below, or use the [cheat sheet](08-reference/cheatsheet.md).

New here? Start with the [ROADMAP](ROADMAP.md).

## Who This Is For
- Beginners who want to understand nginx properly, not just copy configs.
- Developers who deploy apps behind nginx and want to debug and tune it.
- Engineers preparing for DevOps, SRE, or backend interviews.

## Repository Map

| Folder | What you will learn |
|---|---|
| [01-fundamentals](01-fundamentals/) | Install, config structure, `server`/`location` matching, static files |
| [02-core-features](02-core-features/) | Reverse proxy, TLS/HTTPS, variables and rewrites, logging |
| [03-traffic-management](03-traffic-management/) | Load balancing, caching, rate limiting |
| [04-internals](04-internals/) | Process architecture, the request lifecycle |
| [05-security-performance](05-security-performance/) | Hardening and tuning |
| [06-production](06-production/) | Operations, troubleshooting, Docker and Kubernetes |
| [07-projects](07-projects/) | Two end-to-end builds that combine everything |
| [08-reference](08-reference/) | Cheat sheet and interview questions |

## Find a Concept Fast

| I want to... | Go to |
|---|---|
| Install nginx or find config file locations | [installation.md](01-fundamentals/installation.md) |
| Understand contexts and why a directive "isn't working" | [config-structure.md](01-fundamentals/config-structure.md) |
| Know which `server` or `location` handles a request | [server-and-location.md](01-fundamentals/server-and-location.md) |
| Serve files, `root` vs `alias`, SPA fallback | [static-files.md](01-fundamentals/static-files.md) |
| Proxy to an app, WebSockets, trailing-slash `proxy_pass` | [reverse-proxy.md](02-core-features/reverse-proxy.md) |
| Set up HTTPS, Let's Encrypt, HSTS | [tls-https.md](02-core-features/tls-https.md) |
| Redirect, rewrite, use `map`, avoid `if` | [variables-and-rewrites.md](02-core-features/variables-and-rewrites.md) |
| Customize logs, JSON logs, read error messages | [logging.md](02-core-features/logging.md) |
| Balance load, failover, keepalive to backends | [load-balancing.md](03-traffic-management/load-balancing.md) |
| Cache responses safely, stale serving | [caching.md](03-traffic-management/caching.md) |
| Limit requests or connections, `burst`/`nodelay` | [rate-limiting.md](03-traffic-management/rate-limiting.md) |
| Understand workers, the event loop, reload and signals | [architecture.md](04-internals/architecture.md) |
| Understand phases, internal redirects, directive order | [request-lifecycle.md](04-internals/request-lifecycle.md) |
| Harden a server, headers, access control | [security-hardening.md](05-security-performance/security-hardening.md) |
| Speed things up: compression, buffers, keepalive, OS tuning | [performance-tuning.md](05-security-performance/performance-tuning.md) |
| Reload safely, rotate logs, monitor, upgrade | [operations.md](06-production/operations.md) |
| Fix 502/504/403/404 and startup errors | [troubleshooting.md](06-production/troubleshooting.md) |
| Run in Docker or Kubernetes (and the ingress-nginx retirement) | [docker-kubernetes.md](06-production/docker-kubernetes.md) |
| See a full SPA + API setup | [spa-with-api-proxy.md](07-projects/spa-with-api-proxy.md) |
| See a multi-site hardened gateway | [secure-gateway-stack.md](07-projects/secure-gateway-stack.md) |
| Look up a command or snippet | [cheatsheet.md](08-reference/cheatsheet.md) |
| Practice interview questions | [interview-questions.md](08-reference/interview-questions.md) |

## How the Files Are Written
Each topic file follows its subject rather than a fixed template, but you will commonly find: concept and prerequisites, how it works, syntax with examples, common mistakes, debugging, performance and security notes, and links to related topics.

## Conventions
- Example domains use `example.com`, example IPs use documentation ranges (`203.0.113.0/24`) or private ranges (`10.0.0.0/8`). Replace them with your own.
- Paths assume a Debian/Ubuntu or RHEL-style package install. Confirm yours with `nginx -V`.
- Always run `sudo nginx -t` before reloading.
- Examples are written for modern nginx. Where a directive depends on a version (for example `http2 on;` needs 1.25.1+, `ssl_reject_handshake` needs 1.19.4+), the file says so.
- Features that exist only in NGINX Plus (active health checks, sticky cookies, cache purge) are labeled as such.

## Verify Before You Rely On It
Configs here are explanatory examples and have not been run against every nginx version or distribution. Test in staging, run `nginx -t`, and check the [official nginx documentation](https://nginx.org/en/docs/) for the version you deploy. The Kubernetes ingress-nginx retirement, HTTP/3 support, and certificate-authority OCSP behavior change over time, so confirm current status before acting on them.

## Contributing
- Keep filenames short and descriptive; number folders only where order matters.
- Add a topic to the existing folder it belongs to before creating a new one.
- Avoid duplicating explanations. Link to the file that owns the concept.
- Update the index tables in this README and in the ROADMAP when you add or move files.
