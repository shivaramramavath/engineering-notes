# NGINX Learning Track: Verified Facts, Baselines & Standards

> **Document Status**: Complete & Verified  
> **Verification Date**: November 2026 (Live Web & Official Documentation)  
> **Scope**: NGINX Open Source, NGINX Plus, Modern Protocols (HTTP/2, HTTP/3 QUIC), Ingress & Gateway API

---

## 1. Version Baselines (As of Late 2026)

| Release Stream | Current Version Series | Release Cadence | Target Environment | Canonical Reference |
| :--- | :--- | :--- | :--- | :--- |
| **Mainline** | `1.31.x` (e.g. `1.31.10`) | Active development, released every 4–6 weeks | Development, edge routing, HTTP/3 testing | [nginx.org/en/download.html](https://nginx.org/en/download.html) |
| **Stable** | `1.30.x` (e.g. `1.30.8`) | Forked annually from mainline (May 2026); critical bug/security fixes only | Enterprise production, financial platforms | [nginx.org/en/download.html](https://nginx.org/en/download.html) |
| **Legacy Stable** | `1.28.x` / `1.26.x` | Reached End-of-Life / maintenance-only | Older LTS distributions | [endoflife.date/nginx](https://endoflife.date/nginx) |
| **Ubuntu 24.04 LTS** | `1.24.0` (Distro package) | Backported security patches by Canonical | Stock `apt install nginx` on Noble Numbat | [packages.ubuntu.com/noble/nginx](https://packages.ubuntu.com/noble/nginx) |

---

## 2. Protocol Directives & Syntax Evolution

### HTTP/2 Syntax Deprecation (NGINX 1.25.1+)
- **Legacy Syntax (Deprecated in 1.25.1+)**:
  ```nginx
  listen 443 ssl http2; # Triggers warning: [warn] the "listen ... http2" directive is deprecated
  ```
- **Modern Syntax (NGINX 1.25.1+)**:
  ```nginx
  listen 443 ssl;
  http2 on; # Standalone directive inside server {} or http {} block
  ```
- **Compatibility Warning**: On older Linux distributions where NGINX is `< 1.25.1` (such as Ubuntu 24.04 stock `1.24.x`), `http2 on;` will throw `unknown directive "http2"`. The legacy `listen 443 ssl http2;` syntax must be used unless the official NGINX PPA/repo is added.
- **Reference**: [nginx.org/en/docs/http/ngx_http_v2_module.html](https://nginx.org/en/docs/http/ngx_http_v2_module.html)

### HTTP/3 (QUIC) Syntax & Requirements
- **Module**: `ngx_http_v3_module` (compiled by default in modern mainline and stable releases with `--with-http_v3_module`).
- **Transport**: Runs over **UDP** port 443 (NOT TCP).
- **Mandatory Directives**:
  ```nginx
  http {
      http3 on;

      server {
          # TCP Listener for HTTP/1.1 and HTTP/2
          listen 443 ssl;
          listen [::]:443 ssl;

          # UDP Listener for HTTP/3 over QUIC (reuseport is mandatory for multi-worker efficiency)
          listen 443 quic reuseport;
          listen [::]:443 quic reuseport;

          # QUIC requires TLS 1.3
          ssl_protocols TLSv1.3;

          # Advertise HTTP/3 availability to browsers
          add_header Alt-Svc 'h3=":443"; ma=86400' always;
      }
  }
  ```
- **Operational Requirements**:
  1. Cloud Security Groups / UFW / iptables must explicitly permit **UDP port 443**.
  2. Modifying `reuseport` requires a full process restart (`systemctl restart nginx`), as socket options cannot be rebound during graceful reload (`nginx -s reload`).
- **Reference**: [nginx.org/en/docs/http/ngx_http_v3_module.html](https://nginx.org/en/docs/http/ngx_http_v3_module.html)

---

## 3. Cryptographic & TLS Baseline Standards

- **Protocols Allowed**: `TLSv1.2 TLSv1.3;` (Default in modern NGINX).
- **Protocols Forbidden**: `SSLv2`, `SSLv3`, `TLSv1.0`, `TLSv1.1` (All deprecated, insecure, and vulnerable to POODLE, BEAST, and SWEET32 attacks).
- **Deprecated Directives**:
  - `ssl on;` was deprecated in NGINX 1.15.0 in favor of `listen ... ssl;`.
- **Modern Security Headers**:
  - `Strict-Transport-Security "max-age=63072000; includeSubDomains; preload" always;`
  - `X-Content-Type-Options "nosniff" always;`
  - `X-Frame-Options "DENY" always;`
  - `Content-Security-Policy "default-src 'self';" always;`
  - `Referrer-Policy "strict-origin-when-cross-origin" always;`

---

## 4. NGINX Open Source vs. NGINX Plus Features

| Feature Capability | NGINX Open Source (OSS) | NGINX Plus (Commercial) | OSS Alternative / Workaround |
| :--- | :--- | :--- | :--- |
| **Passive Health Checks** | Supported (`max_fails`, `fail_timeout`) | Supported | Built-in |
| **Active Dynamic Health Checks** | **Not Supported** (probe background endpoints) | Supported (`health_check` directive) | Third-party module `nginx_upstream_check_module` or external consul/consul-template |
| **Dynamic Upstream Reconfiguration** | **Not Supported** (requires `nginx -s reload`) | Supported via HTTP REST API (`/api/`) | OpenResty Lua or Nginx Gateway Fabric / Consul template reload |
| **Session Persistence (Sticky)** | Basic (`ip_hash`, `hash $cookie_...`) | Advanced (`sticky cookie`, `sticky route`, `sticky learn`) | `hash $cookie_JSESSIONID consistent;` |
| **Live Activity Monitoring Dashboard** | Minimal (`stub_status`: 7 metrics) | Full JSON/HTML Dashboard (over 100 metrics) | `nginx-prometheus-exporter` querying `stub_status` |
| **Native Key-Value Store** | **Not Supported** | Supported (`keyval`, `keyval_zone`) | Redis integration via Lua or njs |
| **Native ACME / Automatic Let's Encrypt**| **Not Supported** (relies on external Certbot) | Third-party integrations | Certbot / `acme.sh` cron or systemd timer |

---

## 5. Modern NGINX JavaScript (njs) Evolution (2025–2026)

- **Engine Modernization**: The legacy custom njs engine has been deprecated in favor of **QuickJS** (`js_engine qjs;`).
- **ECMAScript Standards**: Full **ES2023** compliance, including `async/await`, Promises, `BigInt`, and modern classes.
- **HTTP Client**: Built-in `ngx.fetch()` now supports HTTP forward proxying and strict HTTP/1.1 response framing.
- **Reference**: [nginx.org/en/docs/njs/](https://nginx.org/en/docs/njs/)

---

## 6. Kubernetes Ingress-NGINX Retirement & Gateway API Successor

- **Retirement Status**: The open-source `kubernetes/ingress-nginx` project reached **End-of-Life (EOL) in March 2026**.
- **Current State**: GitHub repository is archived / read-only. No further releases, bug fixes, or CVE security patches will be issued upstream.
- **Architectural Driver for Retirement**: The Ingress spec (`networking.k8s.io/v1`) relied on overloaded vendor annotations (`nginx.ingress.kubernetes.io/*`) and raw snippet injection (`configuration-snippet`), creating intractable privilege escalation vulnerabilities.
- **The Modern Standard**: **Kubernetes Gateway API** (`gateway.networking.k8s.io`).
  - Separates platform infrastructure (`GatewayClass`, `Gateway`) from developer routing (`HTTPRoute`, `GRPCRoute`).
  - Native weighted traffic splitting, header rewrites, and cross-namespace references.
- **NGINX Implementation**: **NGINX Gateway Fabric** (developed by F5/NGINX) is the designated production replacement for ingress-nginx.
- **Migration Tooling**: `ingress2gateway` automated conversion utility.
- **Reference**: [kubernetes.io/blog/2026/ingress-nginx-retirement/](https://kubernetes.io/blog/)

---

## 7. Critical Antipatterns & Configuration Vulnerabilities to Correct

| Antipattern / Misconfiguration | Security / Operational Hazard | Remediation Standard |
| :--- | :--- | :--- |
| **Off-by-slash / Alias Path Traversal** | `location /files { alias /var/www/files/; }` allows reading `/var/www/files_secret/` via `/files../files_secret`. | Ensure trailing slash parity: `location /files/ { alias /var/www/files/; }` or use `root`. |
| **"If is Evil" in Location Blocks** | `if ($var) { rewrite ...; }` inside `location` breaks NGINX phase handler execution, leading to segfaults or bypassed proxy headers. | Use `map {}` blocks or `try_files` outside location directives. |
| **Missing `server_tokens off;`** | Exposes exact NGINX version number in `Server:` header and default error pages, accelerating automated exploit profiling. | Place `server_tokens off;` in the `http {}` context. |
| **Unsafe `proxy_pass` Trailing Slash** | `proxy_pass http://backend/;` alters the request URI; `proxy_pass http://backend;` preserves the incoming URI intact. | Document and deliberately choose URI passing behavior. |
| **Missing `default_server` & Catch-All Host**| Unmatched SNI requests fall through to the first defined virtual host, leaking internal certificates or serving wrong apps. | Create an explicit `default_server` block returning `444` (No Response) or `400`. |
| **Unrestricted `stub_status`** | Leaving `location /stub_status` without `allow 127.0.0.1; deny all;` exposes active connection counts and traffic volume to the public. | Restrict to loopback and Prometheus scraper IPs. |
| **Wildcard CORS with Credentials** | Setting `Access-Control-Allow-Origin: *` while passing `Access-Control-Allow-Credentials: true` is rejected by browsers or causes credential leaks. | Use a dynamic `map $http_origin $cors_origin` whitelist. |
| **Missing Rate Limiting (`limit_req`)** | Leaves API endpoints vulnerable to credential stuffing, brute force, and layer-7 DDoS floods. | Enforce `limit_req_zone` with realistic `burst` and `nodelay` limits. |
| **Misunderstanding Location Priority** | Assuming regex `~` has lower priority than prefix strings, or misunderstanding `^~` (non-regex prefix match). | Follow strict resolution order: Exact (`=`) -> Non-regex prefix (`^~`) -> Regex (`~`/`~*`) -> Longest prefix. |
