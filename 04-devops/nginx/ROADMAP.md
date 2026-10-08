# Roadmap

How to work through this repository. Pick the path that matches your goal, and use the checkpoints to confirm you are ready to move on.

## Learning Order

```
Setup → Fundamentals → Core features → Traffic management
      → Internals → Security & performance → Production → Projects → Reference / interviews
```

| Stage | Files | Time (rough) |
|---|---|---|
| 1. Fundamentals | [installation](01-fundamentals/installation.md) → [config-structure](01-fundamentals/config-structure.md) → [server-and-location](01-fundamentals/server-and-location.md) → [static-files](01-fundamentals/static-files.md) | 3-4 hours |
| 2. Core features | [reverse-proxy](02-core-features/reverse-proxy.md) → [tls-https](02-core-features/tls-https.md) → [variables-and-rewrites](02-core-features/variables-and-rewrites.md) → [logging](02-core-features/logging.md) | 4-5 hours |
| 3. Traffic management | [load-balancing](03-traffic-management/load-balancing.md) → [caching](03-traffic-management/caching.md) → [rate-limiting](03-traffic-management/rate-limiting.md) | 3-4 hours |
| 4. Internals | [architecture](04-internals/architecture.md) → [request-lifecycle](04-internals/request-lifecycle.md) | 2-3 hours |
| 5. Security and performance | [security-hardening](05-security-performance/security-hardening.md) → [performance-tuning](05-security-performance/performance-tuning.md) | 3-4 hours |
| 6. Production | [operations](06-production/operations.md) → [troubleshooting](06-production/troubleshooting.md) → [docker-kubernetes](06-production/docker-kubernetes.md) | 4-5 hours |
| 7. Projects | [spa-with-api-proxy](07-projects/spa-with-api-proxy.md) → [secure-gateway-stack](07-projects/secure-gateway-stack.md) | 4-6 hours (hands-on) |
| 8. Reference | [interview-questions](08-reference/interview-questions.md), with the [cheatsheet](08-reference/cheatsheet.md) as ongoing lookup | Ongoing |

Times are rough estimates for reading and trying the examples on a test server.

## Setup for Hands-On Practice
You learn nginx by changing configs and watching what happens. Use any of:
- A small Linux VM or cloud instance (best for TLS and systemd practice).
- WSL2 or a local Linux machine.
- Docker: `docker run --rm -p 8080:80 nginx:stable`, with your config mounted ([docker-kubernetes](06-production/docker-kubernetes.md)).

Two habits to build from day one: run `sudo nginx -t` before every reload, and keep `sudo tail -f /var/log/nginx/error.log` open in a second terminal.

## Checkpoints
Move on when you can do these without looking.

**After Stage 1**
- [ ] Explain which `location` wins for `/images/a.jpg` given a prefix, a `^~` prefix, and a regex.
- [ ] Explain `root` vs `alias`, and why `add_header` in a `location` removes inherited headers.
- [ ] Serve an SPA with refresh-safe routes.

**After Stage 2**
- [ ] Proxy to an app with correct `Host` and `X-Forwarded-*` headers, and explain the `proxy_pass` trailing-slash difference.
- [ ] Redirect HTTP to HTTPS and get a Let's Encrypt certificate with working renewal.
- [ ] Replace an `if` with `map`, `return`, or `try_files`.

**After Stage 3**
- [ ] Configure three backends with failover, and read `$upstream_addr` in logs to prove it.
- [ ] Cache anonymous responses without leaking personalized ones.
- [ ] Rate limit a login endpoint and explain `burst` and `nodelay`.

**After Stage 4**
- [ ] Describe master and worker roles, and what happens during a reload.
- [ ] Explain why directive order in the file does not control execution order.

**After Stage 5**
- [ ] Apply a hardening checklist, including a catch-all server and security headers on error responses.
- [ ] Find a bottleneck with logged timings before changing any tuning setting.

**After Stage 6**
- [ ] Diagnose a 502 and a 504 from the error log alone.
- [ ] Reload with zero downtime and roll back a bad change.
- [ ] Run nginx in a container and explain why a ConfigMap edit does not reload it.

**After Stage 7**
- [ ] Build both projects from scratch and pass every item in their verification tables.

## Fast Paths

### "I have a bug right now"
1. [troubleshooting](06-production/troubleshooting.md): symptom tables and the diagnostic workflow.
2. [logging](02-core-features/logging.md): read the error log and add upstream timing.
3. [server-and-location](01-fundamentals/server-and-location.md): if the wrong block handled the request.
4. [cheatsheet](08-reference/cheatsheet.md): commands and gotchas.

### "I know the basics, I need to deploy something this week"
1. [reverse-proxy](02-core-features/reverse-proxy.md) and [tls-https](02-core-features/tls-https.md)
2. [spa-with-api-proxy](07-projects/spa-with-api-proxy.md)
3. [security-hardening](05-security-performance/security-hardening.md) and [operations](06-production/operations.md)

### "Interview in a few days"
1. [cheatsheet](08-reference/cheatsheet.md): skim the gotchas.
2. [interview-questions](08-reference/interview-questions.md): do all 🟢 and 🟡 questions aloud, then the 🔴 ones.
3. Read [architecture](04-internals/architecture.md) and [request-lifecycle](04-internals/request-lifecycle.md) for internals questions.
4. Be ready to walk through [secure-gateway-stack](07-projects/secure-gateway-stack.md) as a design discussion.

### "I run nginx in Kubernetes"
1. [docker-kubernetes](06-production/docker-kubernetes.md), starting with the ingress-nginx retirement section.
2. [operations](06-production/operations.md) and [performance-tuning](05-security-performance/performance-tuning.md) for the same principles in containers.

### "I want deep understanding"
Read in order, then re-read [request-lifecycle](04-internals/request-lifecycle.md) and [config-structure](01-fundamentals/config-structure.md) after the projects. Their rules explain most surprising behavior.

## Beyond This Repository
Topics intentionally left out until you need them: the `stream` module (TCP/UDP proxying), njs scripting, OpenResty/Lua, HTTP/3 deployment details, WAF modules, and NGINX Plus features. Add files for them in the folder where they fit (for example `02-core-features/stream-proxy.md`) when you actually use them. See [Contributing](README.md#contributing).

## Suggested Practice Routine
1. Read one file.
2. Reproduce its main examples on a test server.
3. Break something on purpose, then diagnose it using only `nginx -t`, logs, and `curl`.
4. Answer that topic's questions in [interview-questions](08-reference/interview-questions.md) aloud.
5. Add anything you learned that is missing, linking to the file that owns the concept.
