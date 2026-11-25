# 04-DevOps Master Curriculum Index

This index categorizes all engineering notes across the five DevOps tracks: **Linux**, **Git & GitHub Actions**, **Docker**, **Nginx**, and **AWS**.

---

## 1. Linux Systems Engineering

### 1.1 Fundamentals
- [01-getting-started.md](file:///04-devops/linux/01-fundamentals/01-getting-started.md): Linux distributions, kernel vs userspace, shell prompt architecture.
- [02-filesystem-and-navigation.md](file:///04-devops/linux/01-fundamentals/02-filesystem-and-navigation.md): Filesystem Hierarchy Standard (FHS), inode structure, path traversal.
- [03-working-with-files.md](file:///04-devops/linux/01-fundamentals/03-working-with-files.md): File manipulation, file descriptors, stdout/stderr redirection, pipes.
- [04-users-and-groups.md](file:///04-devops/linux/01-fundamentals/04-users-and-groups.md): `/etc/passwd`, `/etc/shadow`, UID/GID allocation, sudoers privileges.
- [05-permissions.md](file:///04-devops/linux/01-fundamentals/05-permissions.md): Octal permissions, SUID, SGID, sticky bit, POSIX ACLs.

### 1.2 Shell & Text Processing
- [01-shell-basics.md](file:///04-devops/linux/02-shell/01-shell-basics.md): Bash environment variables, aliases, subshells, job control.
- [02-text-processing.md](file:///04-devops/linux/02-shell/02-text-processing.md): Stream manipulation with `grep`, `sed`, `awk`, `cut`, `sort`, `uniq`.
- [03-bash-scripting.md](file:///04-devops/linux/02-shell/03-bash-scripting.md): Strict mode (`set -euo pipefail`), conditionals, loops, functions, traps.
- [04-vim-and-tmux.md](file:///04-devops/linux/02-shell/04-vim-and-tmux.md): Terminal multiplexing with tmux, persistent sessions, modal editing with Vim.

### 1.3 System Administration
- [01-processes-and-signals.md](file:///04-devops/linux/03-system/01-processes-and-signals.md): Process table, state lifecycle, POSIX signals (SIGTERM, SIGKILL, SIGHUP), `/proc`.
- [02-package-management.md](file:///04-devops/linux/03-system/02-package-management.md): Debian `apt`/dpkg and RHEL `dnf`/rpm architecture and repository configuration.
- [03-systemd-and-services.md](file:///04-devops/linux/03-system/03-systemd-and-services.md): Unit files, target dependencies, service supervision, journalctl logging.
- [04-cron-and-timers.md](file:///04-devops/linux/03-system/04-cron-and-timers.md): Crontab scheduling vs modern systemd monotonic and calendar timers.
- [05-storage-and-filesystems.md](file:///04-devops/linux/03-system/05-storage-and-filesystems.md): Block devices, partitioning, ext4, XFS, mount options, LVM volume groups.

### 1.4 Networking & Security
- [01-networking-basics.md](file:///04-devops/linux/04-networking/01-networking-basics.md): Socket inspection (`ss`, `ip`), routing tables, DNS resolution (`/etc/resolv.conf`).
- [02-ssh.md](file:///04-devops/linux/04-networking/02-ssh.md): OpenSSH hardening, ed25519 key generation, ssh-agent, jump hosts, tunneling.
- [03-firewall.md](file:///04-devops/linux/04-networking/03-firewall.md): Linux packet filtering via Netfilter, `nftables`, UFW, and firewalld.
- [01-server-hardening.md](file:///04-devops/linux/05-production/01-server-hardening.md): CIS benchmark compliance, fail2ban, disabling root SSH, AppArmor / SELinux.
- [02-troubleshooting.md](file:///04-devops/linux/05-production/02-troubleshooting.md): Systematic outage diagnostics, dmesg kernel rings, network connection debugging.
- [03-performance.md](file:///04-devops/linux/05-production/03-performance.md): CPU/memory profiling (`top`, `vmstat`, `iostat`, `sar`), load average analysis.
- [01-namespaces-and-cgroups.md](file:///04-devops/linux/06-advanced/01-namespaces-and-cgroups.md): Linux container primitives: PID, NET, MNT namespaces and cgroup v2 controllers.
- [02-advanced-debugging.md](file:///04-devops/linux/06-advanced/02-advanced-debugging.md): Syscall interception with `strace`, library tracing with `ltrace`, eBPF overview.

---

## 2. Git & GitHub Actions

### 2.1 Track Architecture & Setup
- [README.md](file:///04-devops/git-github/README.md): Master Git & GitHub track blueprint, architecture map, and module roadmap.
- [00-setup/](file:///04-devops/git-github/00-setup/README.md): Installation on Linux/macOS/Windows, SSH keys, GPG/SSH commit signing, `.gitconfig` hierarchy, `.gitignore`, and `.gitattributes`.
- [01-fundamentals/](file:///04-devops/git-github/01-fundamentals/README.md): Git vs GitHub, repositories, three trees (Working Tree, Index, HEAD), commit objects, object database, refs, packfiles, SHA-1 vs SHA-256.

### 2.2 Core Operations & History Rewriting
- [02-commands/](file:///04-devops/git-github/02-commands/README.md): Essential commands: init, clone, status, diff, log, add, commit, undoing, clean, branching, merging, conflicts, remotes, stash, tags, patches, blame, bisect, submodules, worktrees, sparse-checkout, LFS, hooks, and aliases.
- [03-history-rewriting/](file:///04-devops/git-github/03-history-rewriting/README.md): History surgery: amend, interactive rebase, cherry-pick, `git reflog`, `git-filter-repo` secret purging, shared history safety, reset vs revert vs restore, lost work recovery playbook.

### 2.3 CI/CD, Collaboration & Security
- [04-github-actions/](file:///04-devops/git-github/04-github-actions/README.md): Complete CI/CD reference: syntax, events, jobs/steps, runners, variables/secrets, expressions, caching, matrix, permissions, environments, reusable workflows, custom actions, pipelines, Docker CI/CD, hardening, debugging, releases, OIDC cloud auth, ARC runners, cost optimization, local testing (`act`), monorepo path filters, and artifact provenance.
- [05-github-collaboration/](file:///04-devops/git-github/05-github-collaboration/README.md): PR lifecycle, code review, issues, milestones, Projects (v2), branch protection & modern Rulesets, `CODEOWNERS`, YAML issue forms, Discussions, forking, GitHub Pages, GHCR packages, and enterprise RBAC.
- [06-workflows-and-strategies/](file:///04-devops/git-github/06-workflows-and-strategies/README.md): Trunk-Based Development (<24h), GitFlow vs GitHub Flow, release branching, hotfixes, Conventional Commits, Semantic Versioning, Feature Flags, dark launching, monorepo vs polyrepo, environment branching anti-patterns, and team operational playbooks.
- [07-github-security/](file:///04-devops/git-github/07-github-security/README.md): Enterprise security: Secret scanning, pre-receive push protection, Dependabot SCA, CodeQL semantic SAST, signed commits & Vigilant Mode, `SECURITY.md`, Private Vulnerability Reporting, SBOM (SPDX/CycloneDX), Sigstore attestations, least-privilege tokens, OIDC, and leaked secret runbook.

### 2.4 Developer Tools, Projects, Interview & Cheatsheets
- [08-gh-cli-and-api/](file:///04-devops/git-github/08-gh-cli-and-api/README.md): GitHub CLI (`gh`), `--json` and `--jq` formatting, REST (v3) vs GraphQL (v4) APIs, cursor pagination, rate limits, webhooks with HMAC validation, GitHub Apps architecture, and `gh` automation in Actions.
- [09-projects/](file:///04-devops/git-github/09-projects/README.md): 8 production reference projects: Conflict resolution lab, rebase & recovery lab, complete Node CI pipeline, multi-arch Docker buildx to GHCR, release automation with conventional commits, monorepo selective CI, reusable workflow library, and zero-secret OIDC cloud deployment.
- [10-interview/](file:///04-devops/git-github/10-interview/README.md): 140+ technical interview questions and case studies across fundamentals, branching/merging, internals & data structures, 15 troubleshooting scenarios, collaboration governance, Actions CI/CD architecture, security engineering, and 12 Staff-level behavioral case studies.
- [11-cheatsheets/](file:///04-devops/git-github/11-cheatsheets/README.md): Rapid reference guides: Git commands, undo & recovery matrix, branching models, Actions syntax, GitHub CLI (`gh`), `.gitconfig` baseline & aliases, and top 20 common errors & fixes.

---

## 3. Docker Containers

### 3.1 Setup & Fundamentals
- [README.md](file:///04-devops/docker/README.md): Master Docker track blueprint, architecture map, and module roadmap.
- [00-setup/](file:///04-devops/docker/00-setup/README.md): Engine installation on Linux, macOS, and Windows, rootless Docker, contexts, and first container.
- [01-fundamentals/](file:///04-devops/docker/01-fundamentals/README.md): Containers vs VMs, images vs containers, daemon architecture, OCI standards, Linux namespaces, cgroups v1/v2, UnionFS (`overlay2`), lifecycle states, and PID 1 signal handling.

### 3.2 Commands & Dockerfile Deep Dive
- [02-commands/](file:///04-devops/docker/02-commands/README.md): CLI lifecycle (`run`, `stop`, `rm`), logs, inspect, stats, image management, build flags, cleanup, restart policies, and Go templates.
- [03-dockerfile/](file:///04-devops/docker/03-dockerfile/README.md): Instruction reference, layer caching mechanics, multi-stage builds, `.dockerignore`, BuildKit/Buildx, multi-platform images, base image choice (Alpine vs Distroless), health checks, language recipes, Hadolint, and pinning.

### 3.3 Storage, Networking & Compose v2
- [04-volumes-and-networking/](file:///04-devops/docker/04-volumes-and-networking/README.md): Named volumes, bind mounts, `tmpfs`, volume backups, network drivers (bridge, host, overlay, macvlan), embedded DNS (`127.0.0.11`), iptables firewall bypass, and troubleshooting.
- [05-compose/](file:///04-devops/docker/05-compose/README.md): Compose Spec blueprint, multi-service topologies, environment interpolation, `depends_on: condition: service_healthy`, profiles, multi-file merging, Compose Watch (`develop.watch`), and testing workflows.

### 3.4 Production, Projects, Interview & Cheatsheets
- [06-production/](file:///04-devops/docker/06-production/README.md): Secrets management, image optimization, CI/CD pipelines, container hardening (non-root, capabilities, seccomp), Trivy scanning, Cosign signing, registries, tagging, logging drivers, cgroups OOM handling, daemon tuning, Swarm, and K8s translation.
- [07-projects/](file:///04-devops/docker/07-projects/README.md): 8 production reference projects (Node/Postgres/Redis, Python FastAPI/Celery, Spring Boot/MySQL, NGINX TLS Proxy, Minimal Go, Compose Watch DevEx, DevSecOps Pipeline, Local Registry).
- [08-interview/](file:///04-devops/docker/08-interview/README.md): 125 deep technical Q&As across 5 domains, 10 complex incident triage scenarios, architectural trade-offs, and STAR behavioral scenarios.
- [09-cheatsheets/](file:///04-devops/docker/09-cheatsheets/README.md): Rapid reference guides for CLI commands, Dockerfile syntax, Compose specification, networking & storage, Buildx & BuildKit, security checklist, debugging flowcharts, and top 25 errors & fixes.

---

## 4. Nginx Web Gateway

### 4.1 Track Overview & Modules
- [README.md](file:///04-devops/nginx/README.md): Master NGINX track architecture, module catalog, and engineering invariants.
- [ROADMAP.md](file:///04-devops/nginx/ROADMAP.md): 9-stage learning roadmap, checkpoints, and fast-track learning pathways.

### 4.2 Module 01: Core Fundamentals
- [01-installation.md](file:///04-devops/nginx/01-fundamentals/01-installation.md): Package installation (APT/YUM), source builds, OpenSSL 3.0+ flags, directory trees.
- [02-config-structure.md](file:///04-devops/nginx/01-fundamentals/02-config-structure.md): Directives, context hierarchy (`main`, `http`, `server`, `location`), inheritance rules.
- [03-server-and-location.md](file:///04-devops/nginx/01-fundamentals/03-server-and-location.md): Strict location precedence (`=`, `^~`, `~`, `~*`, prefix) and the "If is Evil" anti-pattern.
- [04-static-files.md](file:///04-devops/nginx/01-fundamentals/04-static-files.md): Static file serving, `root` vs `alias` traps, MIME types, SPA fallbacks.
- [05-directives-reference-by-context.md](file:///04-devops/nginx/01-fundamentals/05-directives-reference-by-context.md): Comprehensive context matrix of NGINX directives.
- [06-command-line-and-signals.md](file:///04-devops/nginx/01-fundamentals/06-command-line-and-signals.md): Master-Worker signals (`HUP`, `QUIT`, `USR1`, `USR2`) and CLI flags (`-t`, `-T`).
- [07-default-server-and-catch-all-hosts.md](file:///04-devops/nginx/01-fundamentals/07-default-server-and-catch-all-hosts.md): Default server, SSL catch-all fallback, HTTP 444 probe drops.

### 4.3 Module 02: Core Features & Protocols
- [01-logging.md](file:///04-devops/nginx/02-core-features/01-logging.md): High-speed logging, JSON formats, upstream response timing, buffer flushing.
- [02-reverse-proxy.md](file:///04-devops/nginx/02-core-features/02-reverse-proxy.md): Reverse proxying (`proxy_pass`), trailing slashes, header mutations, buffer sizing.
- [03-tls-https.md](file:///04-devops/nginx/02-core-features/03-tls-https.md): Modern TLS 1.3/1.2, Mozilla Modern ciphers, OCSP Stapling, mTLS.
- [04-variables-and-rewrites.md](file:///04-devops/nginx/02-core-features/04-variables-and-rewrites.md): Built-in variables, declarative `map` blocks, rewrite flags (`last`, `break`).
- [05-websockets-and-sse-proxying.md](file:///04-devops/nginx/02-core-features/05-websockets-and-sse-proxying.md): Real-time WebSockets, hop-by-hop headers, unbuffered SSE streaming.
- [06-http2-and-http3-quic.md](file:///04-devops/nginx/02-core-features/06-http2-and-http3-quic.md): Modern standalone `http2 on;`, HTTP/3 QUIC over UDP port 443 (`quic reuseport`).
- [07-compression-gzip-and-brotli.md](file:///04-devops/nginx/02-core-features/07-compression-gzip-and-brotli.md): Wire compression, `gzip_static`, dynamic Brotli, BREACH defense.
- [08-headers-cors-and-security-headers.md](file:///04-devops/nginx/02-core-features/08-headers-cors-and-security-headers.md): HSTS preload, CSP, fixing `add_header` inheritance, CORS preflight.
- [09-error-handling-and-custom-pages.md](file:///04-devops/nginx/02-core-features/09-error-handling-and-custom-pages.md): Intercepting backend errors, branded error pages, JSON payloads.
- [10-redirects-and-canonical-urls.md](file:///04-devops/nginx/02-core-features/10-redirects-and-canonical-urls.md): HTTPS redirects, apex to `www`, trailing slash normalization.
- [11-acme-and-certbot-automation.md](file:///04-devops/nginx/02-core-features/11-acme-and-certbot-automation.md): Automated Let's Encrypt certificates, webroot challenges, renewal timers.
- [12-njs-and-extensibility.md](file:///04-devops/nginx/02-core-features/12-njs-and-extensibility.md): NGINX JavaScript (njs / QuickJS), JWT validation, dynamic routing.

### 4.4 Module 03: Traffic Management, Caching & Upstreams
- [01-caching.md](file:///04-devops/nginx/03-traffic-management/01-caching.md): High-throughput edge caching, 1s microcaching, Cache Lock stampede defense.
- [02-load-balancing.md](file:///04-devops/nginx/03-traffic-management/02-load-balancing.md): Load balancing algorithms, upstream `keepalive` connection pooling.
- [03-rate-limiting.md](file:///04-devops/nginx/03-traffic-management/03-rate-limiting.md): Leaky bucket rate limiting (`limit_req_zone`, `burst`, `nodelay`, `delay`).
- [04-auth-request-and-sso-gateways.md](file:///04-devops/nginx/03-traffic-management/04-auth-request-and-sso-gateways.md): Perimeter SSO authentication, OAuth2/OIDC integration.
- [05-geo-and-map-based-routing.md](file:///04-devops/nginx/03-traffic-management/05-geo-and-map-based-routing.md): GeoIP2 country routing, CIDR classification, clean `map` logic.
- [06-canary-and-blue-green-deployments.md](file:///04-devops/nginx/03-traffic-management/06-canary-and-blue-green-deployments.md): Canary traffic splitting via `split_clients`, cookie/header stickiness.
- [07-connection-limiting-and-concurrency.md](file:///04-devops/nginx/03-traffic-management/07-connection-limiting-and-concurrency.md): Concurrency limits (`limit_conn`), bandwidth rate limits (`limit_rate`).
- [08-circuit-breaking-and-upstream-health-checks.md](file:///04-devops/nginx/03-traffic-management/08-circuit-breaking-and-upstream-health-checks.md): Passive health checks, active probes, graceful degradation.
- [09-sendfile-and-large-file-delivery.md](file:///04-devops/nginx/03-traffic-management/09-sendfile-and-large-file-delivery.md): Zero-copy kernel streaming (`sendfile`), large file slicing.

### 4.5 Module 04: Internals & Architecture
- [01-architecture-master-worker-and-event-loop.md](file:///04-devops/nginx/04-internals/01-architecture-master-worker-and-event-loop.md): Master-Worker architecture, POSIX signals, non-blocking `epoll` event loops.
- [02-request-lifecycle-and-phases.md](file:///04-devops/nginx/04-internals/02-request-lifecycle-and-phases.md): The 11 HTTP Request Execution Phases from POST_READ to LOG.
- [03-config-parsing-and-reload-mechanics.md](file:///04-devops/nginx/04-internals/03-config-parsing-and-reload-mechanics.md): Config AST trees, atomic zero-downtime `SIGHUP` reload sequence.
- [04-memory-buffers-and-shared-zones.md](file:///04-devops/nginx/04-internals/04-memory-buffers-and-shared-zones.md): Memory pools (`ngx_pool_t`) and slab allocators (`ngx_slab_pool_t`).
- [05-modules-and-dynamic-modules.md](file:///04-devops/nginx/04-internals/05-modules-and-dynamic-modules.md): C module architecture (`ngx_module_t`) and dynamic module loading.
- [06-reading-nginx-source-code.md](file:///04-devops/nginx/04-internals/06-reading-nginx-source-code.md): C source tree navigation (`src/core`, `src/http`), writing a C filter module.

### 4.6 Module 05: Security Hardening & Performance
- [01-performance-tuning.md](file:///04-devops/nginx/05-security-performance/01-performance-tuning.md): Linux kernel tuning (`sysctl`), worker affinity, file descriptor limits.
- [02-security-hardening.md](file:///04-devops/nginx/05-security-performance/02-security-hardening.md): Server token masking, unprivileged user, directory traversal defenses.
- [03-waf-and-modsecurity-overview.md](file:///04-devops/nginx/05-security-performance/03-waf-and-modsecurity-overview.md): ModSecurity v3 and OWASP Core Rule Set (CRS) v4.
- [04-ddos-mitigation-patterns.md](file:///04-devops/nginx/05-security-performance/04-ddos-mitigation-patterns.md): Layer 7 anti-DDoS, Slowloris defense, TCP syncookies, connection capping.
- [05-real-ip-and-trusted-proxies.md](file:///04-devops/nginx/05-security-performance/05-real-ip-and-trusted-proxies.md): Real IP module, recursive header unwinding behind CDNs and ALBs.
- [06-benchmarking-methodologies.md](file:///04-devops/nginx/05-security-performance/06-benchmarking-methodologies.md): Load testing methodologies (wrk, vegeta, k6), latency percentiles.
- [07-common-vulnerabilities-in-configs.md](file:///04-devops/nginx/05-security-performance/07-common-vulnerabilities-in-configs.md): Top config security flaws: off-by-slash `alias` traversal, CRLF injection.

### 4.7 Module 06: Production Operations, Cloud & Kubernetes
- [01-docker-and-containers.md](file:///04-devops/nginx/06-production-cloud-kubernetes/01-docker-and-containers.md): Containers, non-root execution, `envsubst` templating.
- [02-kubernetes-ingress-and-gateway-api.md](file:///04-devops/nginx/06-production-cloud-kubernetes/02-kubernetes-ingress-and-gateway-api.md): March 2026 EOL of `ingress-nginx` and Kubernetes Gateway API migration.
- [03-operations-zero-downtime-reloads-and-ci.md](file:///04-devops/nginx/06-production-cloud-kubernetes/03-operations-zero-downtime-reloads-and-ci.md): Zero-downtime reloads, automated CI/CD config linting pipelines.
- [04-troubleshooting-playbooks.md](file:///04-devops/nginx/06-production-cloud-kubernetes/04-troubleshooting-playbooks.md): On-call playbooks for 502, 504, 403, 413, 400, and memory leaks.
- [05-monitoring-metrics-and-observability.md](file:///04-devops/nginx/06-production-cloud-kubernetes/05-monitoring-metrics-and-observability.md): Prometheus exporter, Grafana dashboards, OpenTelemetry distributed tracing.
- [06-high-availability-and-vrrp.md](file:///04-devops/nginx/06-production-cloud-kubernetes/06-high-availability-and-vrrp.md): Active-Passive HA with Keepalived VRRP and Virtual IPs.
- [07-cdn-and-edge-integration.md](file:///04-devops/nginx/06-production-cloud-kubernetes/07-cdn-and-edge-integration.md): Multi-tier CDN integration, Cloudflare / CloudFront caching tiers.
- [08-nginx-vs-alternatives.md](file:///04-devops/nginx/06-production-cloud-kubernetes/08-nginx-vs-alternatives.md): Comprehensive comparison: NGINX vs HAProxy, Traefik, Caddy, Envoy, Apache.

### 4.8 Module 07: Production Reference Projects
- [01-secure-gateway-stack.md](file:///04-devops/nginx/07-projects/01-secure-gateway-stack.md): Multi-site hardened reverse proxy with A+ SSL Labs rating and rate limiting.
- [02-spa-with-api-proxy.md](file:///04-devops/nginx/07-projects/02-spa-with-api-proxy.md): Production SPA hosting (React/Vue) with static asset immutability and API proxy.
- [03-static-site-caching-and-compression.md](file:///04-devops/nginx/07-projects/03-static-site-caching-and-compression.md): High-throughput static platform with pre-compressed Brotli/Gzip.
- [04-load-balanced-backend-cluster.md](file:///04-devops/nginx/07-projects/04-load-balanced-backend-cluster.md): Resilient multi-node cluster with least-conn and `keepalive 64` pools.
- [05-websocket-and-sse-realtime-proxy.md](file:///04-devops/nginx/07-projects/05-websocket-and-sse-realtime-proxy.md): Real-time messaging gateway for persistent WebSockets and unbuffered SSE.
- [06-rate-limited-public-api-gateway.md](file:///04-devops/nginx/07-projects/06-rate-limited-public-api-gateway.md): Public API gateway with tiered rate limits and brute-force defenses.
- [07-tls-termination-and-certbot-automation.md](file:///04-devops/nginx/07-projects/07-tls-termination-and-certbot-automation.md): Automated enterprise TLS with dual ECDSA/RSA fallback and systemd timers.
- [08-canary-release-and-traffic-splitting.md](file:///04-devops/nginx/07-projects/08-canary-release-and-traffic-splitting.md): Canary traffic splitting via `split_clients` with beta tester stickiness.

### 4.9 Module 08: Technical Interview Preparation
- [01-fundamentals-qa.md](file:///04-devops/nginx/08-interview/01-fundamentals-qa.md): Core concepts, process model, static files, and syntax.
- [02-location-matching-and-rewrites-qa.md](file:///04-devops/nginx/08-interview/02-location-matching-and-rewrites-qa.md): Location precedence traps, rewrite flags, declarative `map`.
- [03-reverse-proxy-and-load-balancing-qa.md](file:///04-devops/nginx/08-interview/03-reverse-proxy-and-load-balancing-qa.md): Proxy buffering, trailing slash differences, keepalive pools.
- [04-tls-and-security-qa.md](file:///04-devops/nginx/08-interview/04-tls-and-security-qa.md): Modern TLS, OCSP stapling, HTTP/3 QUIC, DDoS mitigation.
- [05-performance-and-internals-qa.md](file:///04-devops/nginx/08-interview/05-performance-and-internals-qa.md): Epoll event loops, non-blocking I/O, worker affinity, request phases.
- [06-production-troubleshooting-scenarios.md](file:///04-devops/nginx/08-interview/06-production-troubleshooting-scenarios.md): Real-world triage scenarios for 502, 504, reload failures, CPU spikes.
- [07-nginx-vs-alternatives-qa.md](file:///04-devops/nginx/08-interview/07-nginx-vs-alternatives-qa.md): Architectural trade-offs: NGINX vs Envoy, HAProxy, Traefik, Caddy.
- [08-behavioural-and-architecture-discussion.md](file:///04-devops/nginx/08-interview/08-behavioural-and-architecture-discussion.md): Staff/Principal-level system design trade-offs and capacity planning.

### 4.10 Module 09: Quick Reference Cheatsheets
- [01-directives-quick-reference.md](file:///04-devops/nginx/09-cheatsheets/01-directives-quick-reference.md): Core contexts, inheritance rules, syntax summaries.
- [02-location-matching-table.md](file:///04-devops/nginx/09-cheatsheets/02-location-matching-table.md): Precedence matrices, matching flags (`=`, `^~`, `~`, prefix), trap resolutions.
- [03-proxy-and-headers-snippets.md](file:///04-devops/nginx/09-cheatsheets/03-proxy-and-headers-snippets.md): `proxy_pass` rules, WebSocket upgrades, keepalive pools, Real IP headers.
- [04-tls-and-ssl-snippets.md](file:///04-devops/nginx/09-cheatsheets/04-tls-and-ssl-snippets.md): Mozilla Modern ciphers, TLS 1.3/1.2, HTTP/2, HTTP/3 QUIC.
- [05-caching-and-rate-limiting-snippets.md](file:///04-devops/nginx/09-cheatsheets/05-caching-and-rate-limiting-snippets.md): Microcaching (1s TTL), Cache Lock, `limit_req`, `limit_conn`.
- [06-security-headers-snippets.md](file:///04-devops/nginx/09-cheatsheets/06-security-headers-snippets.md): HSTS preload, strict CSP, Permissions-Policy, CORS preflight.
- [07-http-error-codes-and-fixes.md](file:///04-devops/nginx/09-cheatsheets/07-http-error-codes-and-fixes.md): Diagnostic lookup for 301 loops, 400, 403, 404, 413, 429, 444, 500, 502, 504.
- [08-cli-commands-and-signals.md](file:///04-devops/nginx/09-cheatsheets/08-cli-commands-and-signals.md): CLI flags (`-t`, `-T`, `-V`), UNIX signals, zero-downtime upgrades, socket tracing.

---

## 5. Amazon Web Services (AWS)

### 5.1 Module 01: Foundations
- [01-what-is-aws.md](file:///04-devops/aws/01-foundations/01-what-is-aws.md): Cloud computing models, AWS shared responsibility model, global infrastructure.
- [02-accounts-regions-and-billing.md](file:///04-devops/aws/01-foundations/02-accounts-regions-and-billing.md): AWS Organizations, multi-account strategy, Regions, Availability Zones.
- [03-iam.md](file:///04-devops/aws/01-foundations/03-iam.md): IAM users, groups, roles, policy evaluation logic, least privilege enforcement.
- [04-aws-cli.md](file:///04-devops/aws/01-foundations/04-aws-cli.md): AWS CLI v2 configuration, credentials profiles, JMESPath querying.
- [05-cloudshell-and-consoles.md](file:///04-devops/aws/01-foundations/05-cloudshell-and-consoles.md): CloudShell browser environment, console navigation, management tools.
- [06-billing-and-budgets.md](file:///04-devops/aws/01-foundations/06-billing-and-budgets.md): AWS Budgets, anomaly detection, threshold alerts.
- [07-iam-deep-dive.md](file:///04-devops/aws/01-foundations/07-iam-deep-dive.md): Permissions boundaries, session policies, advanced conditions.
- [08-iam-roles-and-cross-account.md](file:///04-devops/aws/01-foundations/08-iam-roles-and-cross-account.md): Cross-account role assumption, external ID, confused deputy defense.
- [09-iam-best-practices.md](file:///04-devops/aws/01-foundations/09-iam-best-practices.md): Least privilege auditing, IAM Access Analyzer, credential rotation.
- [10-cloudtrail-basics.md](file:///04-devops/aws/01-foundations/10-cloudtrail-basics.md): Management and data event auditing, integrity validation.
- [11-aws-pricing-models.md](file:///04-devops/aws/01-foundations/11-aws-pricing-models.md): On-Demand, Savings Plans, Reserved Instances, Spot capacity.
- [12-cost-explorer-and-reports.md](file:///04-devops/aws/01-foundations/12-cost-explorer-and-reports.md): Cost Explorer analysis, Cost and Usage Reports (CUR).

### 5.2 Module 02: Compute & Containers
- [01-ec2.md](file:///04-devops/aws/02-compute/01-ec2.md): Elastic Compute Cloud: instance families, Graviton, EBS storage, security groups.
- [02-lambda.md](file:///04-devops/aws/02-compute/02-lambda.md): Serverless functions: execution environments, concurrency, cold starts, event sources.
- [03-ecs-fargate.md](file:///04-devops/aws/02-compute/03-ecs-fargate.md): Elastic Container Service: task definitions, services, Fargate serverless compute.
- [04-eks-overview.md](file:///04-devops/aws/02-compute/04-eks-overview.md): Elastic Kubernetes Service: control plane, managed node groups, IRSA.
- [05-app-runner.md](file:///04-devops/aws/02-compute/05-app-runner.md): Fully managed container application service for web endpoints.
- [06-compute-decision-matrix.md](file:///04-devops/aws/02-compute/06-compute-decision-matrix.md): Systematic selection matrix across EC2, Lambda, Fargate, EKS, and App Runner.
- [07-ec2-deep-dive.md](file:///04-devops/aws/02-compute/07-ec2-deep-dive.md): Nitro architecture, placement groups, network optimization, EBS-optimized instances.
- [08-ami-and-launch-templates.md](file:///04-devops/aws/02-compute/08-ami-and-launch-templates.md): Golden AMI pipelines, Image Builder, versioned Launch Templates.
- [09-auto-scaling-groups.md](file:///04-devops/aws/02-compute/09-auto-scaling-groups.md): Dynamic, scheduled, predictive scaling, Target Tracking policies.
- [10-systems-manager.md](file:///04-devops/aws/02-compute/10-systems-manager.md): SSM Session Manager, Run Command, Patch Manager, State Manager.

### 5.3 Module 03: Storage & Databases
- [01-s3.md](file:///04-devops/aws/03-storage-and-databases/01-s3.md): Simple Storage Service: buckets, storage classes, lifecycle policies, encryption.
- [02-dynamodb.md](file:///04-devops/aws/03-storage-and-databases/02-dynamodb.md): Managed NoSQL: partition keys, sort keys, secondary indexes (GSI/LSI), TTL.
- [03-rds-and-aurora.md](file:///04-devops/aws/03-storage-and-databases/03-rds-and-aurora.md): Relational Database Service & Amazon Aurora architecture, multi-AZ failover.
- [04-elasticache-redis.md](file:///04-devops/aws/03-storage-and-databases/04-elasticache-redis.md): In-memory caching, sub-millisecond data stores, cluster mode.
- [05-ebs-deep-dive.md](file:///04-devops/aws/03-storage-and-databases/05-ebs-deep-dive.md): EBS gp3 vs io2 Block Express, fast snapshot restore, multi-attach.
- [06-efs-and-shared-storage.md](file:///04-devops/aws/03-storage-and-databases/06-efs-and-shared-storage.md): Serverless POSIX NFS shared storage across EC2, Fargate, and Lambda.
- [07-opensearch-overview.md](file:///04-devops/aws/03-storage-and-databases/07-opensearch-overview.md): Distributed search, log analytics, dashboard visualization.
- [08-database-selection-guide.md](file:///04-devops/aws/03-storage-and-databases/08-database-selection-guide.md): Relational vs NoSQL vs In-Memory vs Graph selection matrix.
- [09-redshift-overview.md](file:///04-devops/aws/03-storage-and-databases/09-redshift-overview.md): Cloud data warehousing, columnar storage, Redshift Serverless.
- [10-aws-backup.md](file:///04-devops/aws/03-storage-and-databases/10-aws-backup.md): Centralized cross-service backup policies, cross-region vaults.
- [11-database-migration-dms.md](file:///04-devops/aws/03-storage-and-databases/11-database-migration-dms.md): Continuous CDC replication, homogeneous and heterogeneous migrations.

### 5.4 Module 04: Networking & Content Delivery
- [01-vpc.md](file:///04-devops/aws/04-networking/01-vpc.md): Virtual Private Cloud: CIDR planning, public/private subnets, NAT Gateways, route tables.
- [02-vpc-deep-dive.md](file:///04-devops/aws/04-networking/02-vpc-deep-dive.md): VPC peering, secondary CIDRs, flow logs, prefix lists, DNS hostnames.
- [03-security-groups-and-nacls.md](file:///04-devops/aws/04-networking/03-security-groups-and-nacls.md): Stateful vs stateless firewalling, ephemeral port routing.
- [04-vpc-peering-transit-gateway-and-privatelink.md](file:///04-devops/aws/04-networking/04-vpc-peering-transit-gateway-and-privatelink.md): Hub-and-spoke transit networks and PrivateLink micro-segmentation.
- [05-route53-and-dns-patterns.md](file:///04-devops/aws/04-networking/05-route53-and-dns-patterns.md): Routing policies, health check failover, private hosted zones.
- [06-cloudfront.md](file:///04-devops/aws/04-networking/06-cloudfront.md): Global CDN, Origin Access Control (OAC), cache behaviors, edge functions.
- [07-load-balancing.md](file:///04-devops/aws/04-networking/07-load-balancing.md): ALB vs NLB vs GLWB, cross-zone load balancing, mTLS.
- [08-api-gateway.md](file:///04-devops/aws/04-networking/08-api-gateway.md): HTTP APIs vs REST APIs, WebSocket APIs, request throttling.
- [09-vpn-and-direct-connect-overview.md](file:///04-devops/aws/04-networking/09-vpn-and-direct-connect-overview.md): Hybrid cloud connectivity, IPsec tunnels, dedicated telco circuits.
- [10-waf-shield-and-network-security.md](file:///04-devops/aws/04-networking/10-waf-shield-and-network-security.md): Layer 7 WAF WebACLs, Layer 3/4 Shield DDoS protection.

### 5.5 Module 05: Application Integration & Messaging
- [01-sqs.md](file:///04-devops/aws/05-app-services/01-sqs.md): Simple Queue Service: standard vs FIFO queues, dead-letter queues (DLQ), visibility timeout.
- [02-sns-and-eventbridge.md](file:///04-devops/aws/05-app-services/02-sns-and-eventbridge.md): Pub/Sub messaging with SNS and event-driven architectures with EventBridge.
- [03-kinesis-and-msk-overview.md](file:///04-devops/aws/05-app-services/03-kinesis-and-msk-overview.md): Real-time streaming data ingestion with Kinesis and Managed Kafka.
- [04-step-functions.md](file:///04-devops/aws/05-app-services/04-step-functions.md): Serverless visual workflow orchestration, Standard vs Express, Saga pattern.
- [05-ses-email.md](file:///04-devops/aws/05-app-services/05-ses-email.md): Simple Email Service: DKIM, SPF verification, transactional email delivery.
- [06-cognito.md](file:///04-devops/aws/05-app-services/06-cognito.md): User Pools, Identity Pools, JWT token verification, social federation.
- [07-appsync-overview.md](file:///04-devops/aws/05-app-services/07-appsync-overview.md): Managed serverless GraphQL APIs with real-time WebSocket subscriptions.
- [08-bedrock.md](file:///04-devops/aws/05-app-services/08-bedrock.md): Serverless generative AI: foundation model APIs, RAG knowledge bases, guardrails.
- [09-event-driven-patterns-on-aws.md](file:///04-devops/aws/05-app-services/09-event-driven-patterns-on-aws.md): Choreography vs orchestration, event routing, transactional outbox.

### 5.6 Module 06: Cloud Operations, Security & IaC
- [01-infrastructure-as-code.md](file:///04-devops/aws/06-operations/01-infrastructure-as-code.md): Declarative cloud infrastructure paradigms, GitOps, drift detection.
- [02-cloudformation.md](file:///04-devops/aws/06-operations/02-cloudformation.md): Native declarative templates, intrinsic functions, nested stacks, StackSets.
- [03-cdk.md](file:///04-devops/aws/06-operations/03-cdk.md): AWS Cloud Development Kit (CDK v2), L1/L2/L3 constructs, cdk-nag.
- [04-terraform-on-aws.md](file:///04-devops/aws/06-operations/04-terraform-on-aws.md): Terraform & OpenTofu on AWS, S3/DynamoDB remote state locking.
- [05-ci-cd-to-aws.md](file:///04-devops/aws/06-operations/05-ci-cd-to-aws.md): GitHub Actions OIDC federation, CodeDeploy Blue/Green deployments.
- [06-security-and-secrets.md](file:///04-devops/aws/06-operations/06-security-and-secrets.md): AWS Secrets Manager, Systems Manager Parameter Store, AWS KMS encryption keys.
- [07-security-services.md](file:///04-devops/aws/06-operations/07-security-services.md): Amazon GuardDuty, AWS Security Hub, Amazon Inspector, Amazon Macie.
- [08-observability.md](file:///04-devops/aws/06-operations/08-observability.md): CloudWatch logs, metric alarms, X-Ray distributed tracing, Container Insights.
- [09-reliability-and-cost.md](file:///04-devops/aws/06-operations/09-reliability-and-cost.md): AWS Well-Architected Framework, cost optimization, AWS Budgets, Cost Explorer.
- [10-cost-optimisation-and-finops.md](file:///04-devops/aws/06-operations/10-cost-optimisation-and-finops.md): FinOps principles, cost allocation tagging, rightsizing, Savings Plans.
- [11-multi-account-strategy.md](file:///04-devops/aws/06-operations/11-multi-account-strategy.md): AWS Organizations, Control Tower, Service Control Policies (SCPs).
- [12-compliance-and-auditing-basics.md](file:///04-devops/aws/06-operations/12-compliance-and-auditing-basics.md): Continuous compliance auditing, AWS Config, Audit Manager, CloudTrail.

### 5.7 Module 07: Enterprise Architecture Patterns
- [01-well-architected-pillars-in-practice.md](file:///04-devops/aws/07-architecture-patterns/01-well-architected-pillars-in-practice.md): 6 Well-Architected pillars, HRI assessments, risk remediation.
- [02-three-tier-web-app.md](file:///04-devops/aws/07-architecture-patterns/02-three-tier-web-app.md): Multi-AZ decoupled public, private, and isolated database architecture.
- [03-serverless-api-pattern.md](file:///04-devops/aws/07-architecture-patterns/03-serverless-api-pattern.md): API Gateway + Lambda ARM64 + DynamoDB Single-Table pattern.
- [04-event-driven-architecture.md](file:///04-devops/aws/07-architecture-patterns/04-event-driven-architecture.md): EventBridge event buses, SQS buffering, DLQ handling, Step Functions sagas.
- [05-containers-on-aws-decision-guide.md](file:///04-devops/aws/07-architecture-patterns/05-containers-on-aws-decision-guide.md): Comprehensive evaluation of App Runner, Fargate, ECS EC2, and EKS.
- [06-multi-region-and-dr-strategies.md](file:///04-devops/aws/07-architecture-patterns/06-multi-region-and-dr-strategies.md): RTO/RPO calculation and the 4 DR tiers from backup to active-active.
- [07-data-lake-pattern.md](file:///04-devops/aws/07-architecture-patterns/07-data-lake-pattern.md): S3 Medallion Lakehouse (Bronze/Silver/Gold), Glue Catalog, Athena, Lake Formation.
- [08-zero-trust-and-landing-zone.md](file:///04-devops/aws/07-architecture-patterns/08-zero-trust-and-landing-zone.md): AWS Verified Access, PrivateLink micro-segmentation, Control Tower guardrails.
- [09-migration-strategies.md](file:///04-devops/aws/07-architecture-patterns/09-migration-strategies.md): The 7 Rs of cloud migration, AWS MGN block replication, AWS DMS CDC.
- [10-choosing-the-right-service.md](file:///04-devops/aws/07-architecture-patterns/10-choosing-the-right-service.md): Multi-dimensional architectural decision trees and flowcharts.

### 5.8 Module 08: Production Reference Projects
- [01-static-site-on-s3-cloudfront.md](file:///04-devops/aws/08-projects/01-static-site-on-s3-cloudfront.md): Production Static Site on S3 + CloudFront with OAC and AWS WAF.
- [02-serverless-rest-api.md](file:///04-devops/aws/08-projects/02-serverless-rest-api.md): Serverless REST API with API Gateway HTTP API, Lambda ARM64, and DynamoDB.
- [03-containerised-app-on-ecs-fargate-with-alb.md](file:///04-devops/aws/08-projects/03-containerised-app-on-ecs-fargate-with-alb.md): Microservice on ECS Fargate with ALB and Auto Scaling.
- [04-three-tier-vpc-with-rds.md](file:///04-devops/aws/08-projects/04-three-tier-vpc-with-rds.md): Enterprise Three-Tier VPC with Amazon Aurora PostgreSQL Multi-AZ.
- [05-event-driven-order-pipeline.md](file:///04-devops/aws/08-projects/05-event-driven-order-pipeline.md): Event-Driven Order Pipeline with EventBridge, SQS, and DLQ.
- [06-secure-ci-cd-with-github-oidc.md](file:///04-devops/aws/08-projects/06-secure-ci-cd-with-github-oidc.md): Zero-static-key continuous deployment with GitHub Actions and OIDC.
- [07-terraform-landing-zone-basics.md](file:///04-devops/aws/08-projects/07-terraform-landing-zone-basics.md): Terraform Landing Zone with AWS Organizations and SCPs.
- [08-observability-stack-on-cloudwatch.md](file:///04-devops/aws/08-projects/08-observability-stack-on-cloudwatch.md): Enterprise Observability Stack on CloudWatch with Composite Alarms.

---

## 6. Interview Preparation & Quick Reference

### 6.1 Technical Interview Preparation
- [01-aws-fundamentals-qa.md](file:///04-devops/aws/09-interview/01-aws-fundamentals-qa.md): AWS Fundamentals, Regions, AZs, Shared Responsibility, and Billing.
- [02-iam-and-security-qa.md](file:///04-devops/aws/09-interview/02-iam-and-security-qa.md): IAM Evaluation Logic, SCPs, Envelope Encryption, KMS, GuardDuty, and WAF.
- [03-networking-qa.md](file:///04-devops/aws/09-interview/03-networking-qa.md): VPC Subnet Math, Security Groups vs NACLs, Transit Gateway, and PrivateLink.
- [04-compute-and-containers-qa.md](file:///04-devops/aws/09-interview/04-compute-and-containers-qa.md): EC2 Lifecycle, Spot, SSM, Lambda Cold Starts, ECS Fargate, and EKS.
- [05-storage-and-databases-qa.md](file:///04-devops/aws/09-interview/05-storage-and-databases-qa.md): S3 Strong Consistency, DynamoDB Partitioning, RDS Multi-AZ, and Aurora.
- [06-serverless-and-messaging-qa.md](file:///04-devops/aws/09-interview/06-serverless-and-messaging-qa.md): SQS Standard vs FIFO, Long Polling, EventBridge, Step Functions Sagas.
- [07-architecture-and-scenario-questions.md](file:///04-devops/aws/09-interview/07-architecture-and-scenario-questions.md): System Design Scenarios: 100k req/s API, Active-Active DR, Lakehouse.
- [08-cost-and-reliability-qa.md](file:///04-devops/aws/09-interview/08-cost-and-reliability-qa.md): FinOps, Cost Allocation Tags, Compute Savings Plans, Well-Architected HRIs.
- [09-behavioural-and-project-discussion.md](file:///04-devops/aws/09-interview/09-behavioural-and-project-discussion.md): Amazon Leadership Principles, STAR Framework, Blameless Post-Mortems.
- [Linux Technical Interview Q&A](file:///04-devops/linux/reference/interview.md)
- [Nginx Gateway Technical Interview Q&A](file:///04-devops/nginx/08-reference/interview-questions.md)

### 6.2 Quick Reference Cheatsheets
- [01-cli-commands.md](file:///04-devops/aws/10-cheatsheets/01-cli-commands.md): AWS CLI v2 essential commands, JMESPath queries, SSM shell, and S3 sync.
- [02-iam-policy-snippets.md](file:///04-devops/aws/10-cheatsheets/02-iam-policy-snippets.md): Trust policies, S3 HTTPS/OAC policies, Organizations SCPs.
- [03-vpc-cidr-and-routing.md](file:///04-devops/aws/10-cheatsheets/03-vpc-cidr-and-routing.md): Subnet math (/16 to /28), AWS 5 reserved IPs, 3-tier VPC blueprint.
- [04-s3-and-dynamodb.md](file:///04-devops/aws/10-cheatsheets/04-s3-and-dynamodb.md): S3 lifecycle syntax, DynamoDB RCU/WCU calculation formulas, Boto3 SDK.
- [05-lambda-and-api-gateway.md](file:///04-devops/aws/10-cheatsheets/05-lambda-and-api-gateway.md): Memory-to-vCPU scaling table, REPORT log line format, error code mapping.
- [06-ecs-eks-quick-ref.md](file:///04-devops/aws/10-cheatsheets/06-ecs-eks-quick-ref.md): ECS Fargate task definitions, exit codes (137 OOM), Karpenter manifests.
- [07-cloudwatch-queries.md](file:///04-devops/aws/10-cheatsheets/07-cloudwatch-queries.md): 10 production Logs Insights queries: top errors, cold starts, VPC flow.
- [08-terraform-and-cdk-snippets.md](file:///04-devops/aws/10-cheatsheets/08-terraform-and-cdk-snippets.md): S3 state backend locking, provider default tags, CDK L2 constructs.
- [09-service-chooser.md](file:///04-devops/aws/10-cheatsheets/09-service-chooser.md): "Which service for what" master table, symptom-to-cause diagnostic table.
- [10-limits-and-defaults.md](file:///04-devops/aws/10-cheatsheets/10-limits-and-defaults.md): Hard vs soft platform limits across compute, storage, networking, messaging.
- [Linux Commands & Sysadmin Cheatsheet](file:///04-devops/linux/reference/cheatsheet.md)
- [GitHub Actions Workflow Cheatsheet](file:///04-devops/git-github/04_github_actions/CHEATSHEET.md)
- [Nginx Directives Cheatsheet](file:///04-devops/nginx/08-reference/cheatsheet.md)
