---
title: "NGINX Security Hardening & Zero-Trust Architecture"
description: "Production guide to hardening NGINX: server tokens, security headers, catch-all server blocks, file permissions, path traversal mitigation, buffer overflow protection, and least privilege execution."
module: "05-security-performance"
topic: "02-security-hardening"
status: "completed"
tags: ["nginx", "security", "hardening", "headers", "zero-trust", "least-privilege", "csp", "hsts", "defensive"]
prerequisites: ["05-security-performance/01-performance-tuning.md", "02-core-features/03-tls-https.md"]
estimated_read_time: "35 min"
---

# NGINX Security Hardening & Zero-Trust Architecture

```
+-----------------------------------------------------------------------------+
| Multi-Layered Edge Defense Perimeter                                        |
|                                                                             |
|  [Untrusted Internet / Attacker]                                            |
|        │                                                                    |
|        ▼                                                                    |
|  [Catch-All Default Server]                                                 |
|        │  listen 80/443 default_server; return 444; (Drop invalid SNI/Host) |
|        ▼                                                                    |
|  [Protocol & TLS Boundary]                                                  |
|        │  TLSv1.2 TLSv1.3 only; HSTS (Strict-Transport-Security);             |
|        │  ssl_prefer_server_ciphers off;                                    |
|        ▼                                                                    |
|  [HTTP Request Sanitization & Size Constraints]                             |
|        │  server_tokens off; client_max_body_size 10m;                      |
|        │  limit_except GET POST { deny all; }                               |
|        ▼                                                                    |
|  [Path Traversal & Sensitive File Shields]                                  |
|        │  location ~ /\.(?!well-known) { deny all; }                        |
|        │  location ~* \.(env|git|bak|sql|yaml|yml)$ { deny all; }           |
|        ▼                                                                    |
|  [Client Defense Headers (CSP, Frame, Sniff)]                               |
|        │  Content-Security-Policy, X-Content-Type-Options: nosniff          |
|        ▼                                                                    |
|  [Hardened OS Sandbox: unprivileged user, read-only rootfs, drop caps]       |
+-----------------------------------------------------------------------------+
```

---

## Table of Contents
1. [What & Why](#1-what--why)
2. [Core Concepts & Defense-in-Depth Model](#2-core-concepts--defense-in-depth-model)
3. [How It Works Internally](#3-how-it-works-internally)
4. [Syntax & Directives Reference](#4-syntax--directives-reference)
5. [Practical Implementation Examples](#5-practical-implementation-examples)
6. [Production Hardening Checklist](#6-production-hardening-checklist)
7. [Common Pitfalls & Anti-Patterns](#7-common-pitfalls--anti-patterns)
8. [Performance & Security Trade-Offs](#8-performance--security-trade-offs)
9. [Architectural Comparison: Default vs Hardened NGINX](#9-architectural-comparison-default-vs-hardened-nginx)
10. [Technical Interview Questions & Scenarios](#10-technical-interview-questions--scenarios)
11. [Hands-On Practice Exercises](#11-hands-on-practice-exercises)
12. [Summary & Quick Reference](#12-summary--quick-reference)
13. [Further Reading](#13-further-reading)

---

## 1. What & Why

NGINX is often the front-line gatekeeper between the public internet and sensitive application clusters. While the NGINX core has an exceptional security track record, default installations expose exploitable attack surfaces:
- **Reconnaissance & Fingerprinting**: Default error pages and headers broadcast `Server: nginx/1.24.0 (Ubuntu)`, allowing automated scanners to match specific known CVEs.
- **Direct IP Attacks & Host Header Injection**: Attackers scanning IPv4/IPv6 address blocks send arbitrary `Host` headers to access internal virtual hosts, trigger password reset poisoning, or exploit cache poisoning.
- **Accidental Information Disclosure**: Permissive location blocks accidentally expose hidden version control folders (`.git`), configuration secrets (`.env`), or database backups (`backup.sql`).
- **Client-Side Attacks (XSS, Clickjacking, MIME Sniffing)**: Omitting strict HTTP response headers leaves browsers vulnerable to script injection and framing attacks.

Security hardening transforms NGINX from a passive proxy into an active security perimeter following the Principle of Least Privilege.

---

## 2. Core Concepts & Defense-in-Depth Model

A robust hardening strategy enforces multiple concentric rings of protection:
1. **Network & Connection Perimeter**: Reject invalid Host/SNI combinations immediately with HTTP 444 (closed connection without response).
2. **TLS Hardening**: Enforce modern ciphers, disable deprecated protocols (SSLv3, TLS 1.0, TLS 1.1), and enforce HSTS with preloading.
3. **Information Hiding**: Strip upstream identity headers (`X-Powered-By`, `Server`, `X-AspNet-Version`) and disable server tokens.
4. **Access Control & Path Shielding**: Explicitly forbid dotfiles, backup files, and restricted HTTP verbs (`PUT`, `DELETE`, `TRACE`, `OPTIONS`) where not required.
5. **Buffer Overflow & Resource Protection**: Cap request header and body sizes to prevent buffer overflow probes and memory exhaustion attacks.
6. **Operating System Isolation**: Run worker processes under an unprivileged user (`www-data` or `nginx`), isolate configurations with read-only permissions, and restrict file execution.

---

## 3. How It Works Internally

### Connection Rejection via Non-Standard HTTP Status 444

```
  Attacker Scans IP: https://198.51.100.4/
  [Host: attacker-controlled.com]
          │
          ▼
  [NGINX Listening Socket]
          │
          ▼
  Find-Config Phase (Server Block Matching)
  - Matches `default_server` because Host does not match any valid `server_name`
          │
          ▼
  `return 444;` Directive Executes
          │
          ▼
  Internal Function: `ngx_http_finalize_request(r, NGX_HTTP_CLOSE)`
  - Drops connection immediately!
  - Sends ZERO bytes to attacker.
  - Releases client socket and returns TCP RST packet.
```

When NGINX encounters `return 444;`, it invokes `ngx_http_finalize_request()` with `NGX_HTTP_CLOSE`. Unlike returning `403 Forbidden` or `404 Not Found` (which requires generating an HTTP response body, consuming bandwidth and advertising an active web server), status 444 closes the TCP socket immediately, starving port scanners of fingerprinting data.

---

## 4. Syntax & Directives Reference

### Information Disclosure Directives
- `server_tokens off;`: Suppresses the NGINX version number on error pages and in the `Server` response header.
- `proxy_hide_header <header_name>;`: Strips specified backend response headers before sending to the client (e.g., `proxy_hide_header X-Powered-By;`).
- `more_clear_headers <header_name>;`: (Requires `headers-more-nginx-module`) Completely strips the `Server` header from responses.

### Request Limiting & Buffer Hardening
- `client_max_body_size <size>;`: Sets the maximum allowed size of the client request body. Returns `413 Request Entity Too Large` if exceeded.
- `client_header_buffer_size <size>;`: Limits the buffer allocated for client request lines and headers.
- `large_client_header_buffers <number> <size>;`: Maximum number and size of buffers for large URI strings and HTTP headers. Prevents memory inflation attacks.

### Access Control Directives
- `limit_except <METHOD> ... { deny all; }`: Restricts access within a location to specified HTTP methods (e.g., only allow `GET` and `POST`).
- `allow <CIDR>;` / `deny all;`: Enforces IP-based firewall rules directly at the NGINX access phase.

---

## 5. Practical Implementation Examples

### Example 1: Catch-All Drop-In Server Block (`default_server`)

Drop all scanners and unauthorized Host header traffic immediately:

```nginx
# /etc/nginx/conf.d/00-default-drop.conf

# Catch-all HTTP (Port 80)
server {
    listen 80 default_server;
    listen [::]:80 default_server;
    server_name _;

    # Drop connection immediately with zero response bytes
    return 444;
}

# Catch-all HTTPS (Port 443)
server {
    listen 443 ssl default_server;
    listen [::]:443 ssl default_server;
    server_name _;

    # Self-signed fallback dummy certificate for invalid SNI
    ssl_certificate /etc/nginx/ssl/dummy-fallback.crt;
    ssl_certificate_key /etc/nginx/ssl/dummy-fallback.key;
    ssl_protocols TLSv1.2 TLSv1.3;

    return 444;
}
```

### Example 2: Production Hardened Security Headers Snippet

Create a reusable security headers file and include it across server blocks:

```nginx
# /etc/nginx/snippets/security-headers.conf

# 1. Prevent MIME Sniffing
add_header X-Content-Type-Options "nosniff" always;

# 2. Prevent Clickjacking (framing)
add_header X-Frame-Options "DENY" always;

# 3. Referrer Information Control
add_header Referrer-Policy "strict-origin-when-cross-origin" always;

# 4. Strict Permissions Policy (Disable unnecessary hardware APIs)
add_header Permissions-Policy "camera=(), microphone=(), geolocation=(), payment=()" always;

# 5. Cross-Origin Policies
add_header Cross-Origin-Opener-Policy "same-origin" always;
add_header Cross-Origin-Resource-Policy "same-origin" always;

# 6. Content Security Policy (Strict Base)
add_header Content-Security-Policy "default-src 'self'; script-src 'self'; style-src 'self' 'unsafe-inline'; img-src 'self' data:; font-src 'self'; connect-src 'self'; frame-ancestors 'none'; base-uri 'self'; form-action 'self';" always;

# 7. HTTP Strict Transport Security (HSTS - 2 Years with Preload)
add_header Strict-Transport-Security "max-age=63072000; includeSubDomains; preload" always;
```

### Example 3: Blocking Sensitive Files, Dotfiles, and Backups

Shield repository artifacts, credentials, and configuration dumps from web access:

```nginx
# /etc/nginx/snippets/block-sensitive-files.conf

# Deny all hidden dotfiles (e.g., .git, .env, .htaccess, .ssh)
# Exception: allow ACME challenge directory for Let's Encrypt
location ~ /\.(?!well-known) {
    deny all;
    access_log off;
    log_not_found off;
    return 404;
}

# Deny access to dangerous extensions
location ~* \.(bak|conf|config|dist|fla|inc|ini|log|psd|sh|sql|swp|yaml|yml|env|git)$ {
    deny all;
    access_log off;
    log_not_found off;
    return 404;
}

# Block package management and build artifacts
location ~* (package\.json|package-lock\.json|yarn\.lock|composer\.json|composer\.lock|Dockerfile|docker-compose\.yml)$ {
    deny all;
    access_log off;
    log_not_found off;
    return 404;
}
```

### Example 4: Hardened Reverse Proxy Server Configuration

A complete secure reverse proxy block for an application backend:

```nginx
# /etc/nginx/conf.d/app.example.com.conf

server {
    listen 80;
    server_name app.example.com;
    # Force canonical HTTPS redirect
    return 301 https://$host$request_uri;
}

server {
    listen 443 ssl;
    http2 on;
    server_name app.example.com;

    # TLS Configuration
    ssl_certificate /etc/letsencrypt/live/app.example.com/fullchain.pem;
    ssl_certificate_key /etc/letsencrypt/live/app.example.com/privkey.pem;
    ssl_protocols TLSv1.2 TLSv1.3;
    ssl_ciphers ECDHE-ECDSA-AES128-GCM-SHA256:ECDHE-RSA-AES128-GCM-SHA256:ECDHE-ECDSA-AES256-GCM-SHA384:ECDHE-RSA-AES256-GCM-SHA384;
    ssl_prefer_server_ciphers off;

    # Security Headers & Sensitive File Shields
    include /etc/nginx/snippets/security-headers.conf;
    include /etc/nginx/snippets/block-sensitive-files.conf;

    # Request Size and Timeout Constraints
    client_max_body_size 5m;
    client_body_buffer_size 128k;
    client_header_buffer_size 1k;
    large_client_header_buffers 4 8k;

    client_body_timeout 10s;
    client_header_timeout 10s;
    send_timeout 10s;

    # Restrict HTTP Verbs Globally
    limit_except GET POST PUT DELETE HEAD OPTIONS {
        deny all;
    }

    location / {
        proxy_pass http://127.0.0.1:3000;
        proxy_http_version 1.1;
        proxy_set_header Connection "";

        # Standard Proxy Headers
        proxy_set_header Host $host;
        proxy_set_header X-Real-IP $remote_addr;
        proxy_set_header X-Forwarded-For $proxy_add_x_forwarded_for;
        proxy_set_header X-Forwarded-Proto https;

        # Hide Backend Fingerprints
        proxy_hide_header X-Powered-By;
        proxy_hide_header Server;
        proxy_hide_header X-AspNet-Version;

        # Re-include security headers (Crucial: child block overrides parent headers!)
        include /etc/nginx/snippets/security-headers.conf;
    }
}
```

### Example 5: Restricting Administrative Paths by IP and Basic Auth

Enforce multi-factor IP whitelisting on internal dashboards:

```nginx
location /admin/ {
    # 1. Allow internal VPC and corporate VPN only
    allow 10.0.0.0/8;
    allow 192.168.1.0/24;
    allow 203.0.113.50/32; # Corporate Office Static IP
    deny all;

    # 2. Enforce Basic Authentication as an additional barrier
    auth_basic "Restricted Administrative Console";
    auth_basic_user_file /etc/nginx/.htpasswd;

    proxy_pass http://127.0.0.1:3000/admin/;
    include /etc/nginx/snippets/security-headers.conf;
}
```

---

## 6. Production Hardening Checklist

- [ ] **Run as Non-Root**: Verify that NGINX master drops worker privileges to `www-data` or `nginx` (`user www-data;`).
- [ ] **File Permissions**: Set `/etc/nginx` permissions to `root:root` with directory permissions `755` and configuration files `644`. Ensure SSL private keys are `chmod 600 root:root`.
- [ ] **Disable Server Version**: Configure `server_tokens off;` in the `http {}` context.
- [ ] **Catch-All Block**: Define explicit `default_server` blocks returning `444` for both HTTP and HTTPS.
- [ ] **Security Headers Applied**: Verify `X-Content-Type-Options`, `X-Frame-Options`, `Referrer-Policy`, `CSP`, and `HSTS` headers via `curl -I`.
- [ ] **Child Block Header Inheritance**: Ensure `add_header` snippets are included inside any `location` block that specifies `add_header`.
- [ ] **Disable Dangerous HTTP Methods**: Restrict methods using `limit_except` or drop `TRACE`, `CONNECT`, `TRACK`.
- [ ] **Block Dotfiles**: Verify that requests to `/.git/config` and `/.env` return `404` or `403`.
- [ ] **Tuned Timeouts**: Enforce tight client header and body timeouts to defend against Slowloris attacks (`client_header_timeout 10s;`).

---

## 7. Common Pitfalls & Anti-Patterns

| Anti-Pattern / Mistake | Root Cause | Observable Security Vulnerability | Correct Solution |
| :--- | :--- | :--- | :--- |
| Missing `default_server` block | NGINX routes unknown hosts to the first parsed virtual host | Attackers reach internal applications via raw IP or arbitrary Host | Explicitly declare a default server with `return 444;` |
| `add_header` Inheritance Blindspot | Child `location` defines one `add_header`, wiping all parent headers | Security headers (CSP, HSTS) vanish entirely on protected endpoints | Include common headers snippet inside every child block |
| Storing Private Keys Readable by Workers | Setting SSL private key permissions to `chmod 644` or `chown www-data` | Compromised worker process can read the master TLS private key | Keep keys `chmod 600 root:root`; master loads keys before dropping privilege |
| Permissive Regex in Dotfile Rule | `location ~ /\. { deny all; }` blocking ACME renewals | Automated Let's Encrypt certbot HTTP-01 challenge fails | Use negative lookahead: `location ~ /\.(?!well-known)` |
| Relying on `server_tokens off` alone | Believing version hiding prevents software vulnerabilities | Vulnerable endpoints still exploitable via known payloads | Treat version hiding as defense-in-depth, not a substitute for patching |
| Unbounded `client_max_body_size` | Setting body size to `0` or `1000m` without rate limits | Denial of Service through disk and memory exhaustion | Set strict per-location body limits (e.g. 2m for APIs, 50m for uploads) |

---

## 8. Performance & Security Trade-Offs

- **HSTS Preloading (`preload`)**: Adding your domain to the Chrome/Firefox HSTS preload list permanently forces browsers to use HTTPS. *Caution*: If your certificate expires or an internal subdomain fails to support HTTPS, users will be locked out with zero bypass capability.
- **Strict Content Security Policy (CSP)**: Completely mitigates XSS by forbidding inline script tags (`'unsafe-inline'`) and `eval()`. However, legacy frontend applications relying on inline scripts or external CDNs will break unless refactored or configured with cryptographic nonces/hashes.
- **Drop Connection (`return 444;`) vs `return 403;`**: Returning 444 saves CPU and bandwidth because NGINX closes the socket immediately without generating HTML error pages.

---

## 9. Architectural Comparison: Default vs Hardened NGINX

| Security Control | Default Distribution Config | Production Hardened Config |
| :--- | :--- | :--- |
| **Server Header** | `Server: nginx/1.24.0 (Ubuntu)` | `Server: nginx` (or omitted via module) |
| **Unknown Host Handling** | Routes to first defined virtual host | Dropped instantly with HTTP 444 |
| **SSL / TLS Version** | TLSv1, TLSv1.1, TLSv1.2, TLSv1.3 | TLSv1.2 and TLSv1.3 exclusively |
| **HSTS** | Disabled | 2 years (`max-age=63072000; preload`) |
| **Dotfiles (`.git`, `.env`)** | Accessible if present in `root` directory | Explicitly blocked with `return 404;` |
| **HTTP Verb Filtering** | All methods accepted (`TRACE`, `DELETE`) | Restricted via `limit_except` |
| **Backend Stack Leakage** | `X-Powered-By: Express` forwarded | Stripped via `proxy_hide_header` |
| **Client Protection Headers** | None | CSP, Frame-Options, nosniff, Referrer |

---

## 10. Technical Interview Questions & Scenarios

<details>
<summary><strong>1. Explain the "add_header inheritance trap" in NGINX and how it leads to security vulnerabilities.</strong></summary>

In NGINX, the `add_header` directive exhibits non-intuitive inheritance behavior: array directives like `add_header` are only inherited from parent configuration blocks (e.g., `http {}` or `server {}`) if and only if the current block contains **no** `add_header` directives of its own. If you define global security headers (HSTS, CSP, X-Frame-Options) in the `http {}` block, and subsequently add a single header like `add_header Cache-Control "no-store";` inside a sensitive `location /api/` block, **all** parent security headers are discarded for that location. The fix is to isolate security headers in a snippet (`snippets/security-headers.conf`) and include it explicitly inside every block that sets custom headers.
</details>

<details>
<summary><strong>2. Why should SSL private key files be owned by `root:root` with `chmod 600`, even though NGINX runs workers as `www-data`?</strong></summary>

NGINX employs a master-worker process architecture. The master process starts with `root` privileges. During initialization (`ngx_init_cycle()`), the master process reads and validates the configuration files, opens listening sockets on privileged ports (80/443), and loads the SSL private keys directly into memory. Only after these initialization steps are complete does the master spawn worker processes and drop worker privileges to the unprivileged user (`www-data`). Because workers inherit the open SSL memory context from the master, they never need direct filesystem read access to private keys. Keeping permissions at `chmod 600 root:root` ensures that even if an attacker achieves Remote Code Execution (RCE) in a worker process, they cannot extract the private key from disk.
</details>

<details>
<summary><strong>3. What is HTTP status code 444, and why is it preferred over HTTP 403 or 400 for malicious scanners?</strong></summary>

HTTP 444 is an NGINX-specific internal status code that instructs the server to close the TCP connection immediately without sending an HTTP response header or body to the client (`NGX_HTTP_CLOSE`). For unauthorized port scanners, direct IP scans, or invalid SNI requests:
1. It sends zero response bytes, saving outbound egress bandwidth.
2. It prevents fingerprinting of the web server software or operating system.
3. It starves automated scanning engines of feedback, forcing connection timeouts.
</details>

<details>
<summary><strong>4. How does an attacker exploit an "off-by-slash" alias misconfiguration, and how do you prevent it?</strong></summary>

When using `alias` inside a location without a trailing slash:
```nginx
location /static {
    alias /var/www/app/static/;
}
```
An attacker requesting `/static../secret.txt` causes NGINX to strip `/static` and append `../secret.txt` to the alias path, resulting in `/var/www/app/static/../secret.txt` which resolves to `/var/www/app/secret.txt`. This allows path traversal and arbitrary file reading outside the intended directory. To prevent it, ensure the location URI and the alias path both end with a trailing slash:
```nginx
location /static/ {
    alias /var/www/app/static/;
}
```
</details>

<details>
<summary><strong>5. How do you defend NGINX against Slowloris and Slow HTTP POST denial-of-service attacks?</strong></summary>

Slowloris attacks open hundreds of connections and send partial HTTP headers or body bytes at slow intervals (e.g., 1 byte every 10 seconds), exhausting worker connection tables. Defenses include:
1. Tightening `client_header_timeout` and `client_body_timeout` (e.g., 10s each).
2. Setting `send_timeout 10s;`.
3. Enabling `reset_timedout_connection on;` to immediately send a TCP RST on expiration.
4. Enforcing connection limits per IP address using `limit_conn_zone $binary_remote_addr zone=addr:10m;` and `limit_conn addr 20;`.
</details>

<details>
<summary><strong>6. Why is it dangerous to use `if` directives inside `location` blocks for access control?</strong></summary>

In NGINX, `if` directives inside `location` blocks do not behave like procedural conditionals. They create an invisible nested internal location. When an `if` condition evaluates to true, request execution is transferred into the internal location where content phase handlers and module directives may not be inherited. This can bypass `proxy_pass`, nullify access controls (`allow`/`deny`), or trigger silent worker crashes (`SIGSEGV`). Safe access control must use `limit_except`, the access module (`allow`/`deny`), or `map` directives in the `http {}` context.
</details>

<details>
<summary><strong>7. What headers should be stripped when proxying to an upstream Node.js or Python backend?</strong></summary>

Upstream application frameworks often emit informational headers that leak implementation details. NGINX should strip:
- `X-Powered-By` (e.g., `Express`, `PHP/8.1`, `ASP.NET`)
- `Server` (e.g., `Kestrel`, `Werkzeug`, `Gunicorn`)
- `X-AspNet-Version` and `X-Runtime`
Use `proxy_hide_header` inside the `location` or `http` context to suppress them before downstream transmission.
</details>

<details>
<summary><strong>8. What is the security advantage of disabling `ssl_session_tickets`?</strong></summary>

Session tickets encrypt TLS session state using a key shared across server instances. If session tickets are enabled and the server does not regularly rotate ticket keys in memory, an attacker who later compromises the session ticket encryption key can retroactively decrypt recorded TLS traffic from past sessions, breaking Perfect Forward Secrecy (PFS). Disabling `ssl_session_tickets off;` forces clients to rely on `ssl_session_cache shared:SSL:10m;`, where session keys reside strictly in volatile server RAM and cannot be decrypted post-hoc.
</details>

---

## 11. Hands-On Practice Exercises

<details>
<summary><strong>Exercise 1: Audit Live Security Headers with `curl`</strong></summary>

**Goal**: Verify all hardening headers are present and identify missing protections.
1. Run a detailed header check against your NGINX endpoint:
```bash
curl -s -D - https://app.example.com -o /dev/null
```
2. Verify the presence of:
   - `strict-transport-security`
   - `x-content-type-options: nosniff`
   - `x-frame-options: DENY`
   - `content-security-policy`
3. Check the `Server` header to ensure the version number is suppressed.
</details>

<details>
<summary><strong>Exercise 2: Test Catch-All Rejection via Raw IP and Invalid Host</strong></summary>

**Goal**: Ensure unauthorized host headers trigger HTTP 444.
1. Issue a request targeting the IP with a bogus Host header:
```bash
curl -v -H "Host: malicious-attacker.com" http://127.0.0.1/
```
2. Confirm the curl output indicates `Empty reply from server` or `Connection reset by peer` (code 444).
</details>

<details>
<summary><strong>Exercise 3: Test Sensitive Dotfile and Extension Blocking</strong></summary>

**Goal**: Validate that path shielding rules block unauthorized file access.
1. Create a dummy test file: `touch /var/www/html/.env /var/www/html/test.bak`.
2. Issue requests:
```bash
curl -I http://localhost/.env
curl -I http://localhost/test.bak
```
3. Verify both requests return HTTP 404 or 403, and that accesses are omitted from access logs if `access_log off;` is active.
</details>

<details>
<summary><strong>Exercise 4: Verify the `add_header` Inheritance Trap</strong></summary>

**Goal**: Reproduce and resolve header loss in child location blocks.
1. In `server {}`, declare `add_header X-Custom-Global "present";`.
2. In `location /api/`, add `add_header X-API-Local "present";`.
3. Curl `/api/` and notice `X-Custom-Global` is missing!
4. Fix by creating a shared snippet and including it in both locations.
</details>

<details>
<summary><strong>Exercise 5: Simulate a Slowloris Attack Against Tuned Timeouts</strong></summary>

**Goal**: Verify NGINX connection termination under slow client conditions.
1. Open a connection with `nc localhost 80`.
2. Send `GET / HTTP/1.1\r\n` and wait 15 seconds without sending the final `\r\n`.
3. Confirm that NGINX terminates the connection with a 408 Request Timeout or TCP RST after `client_header_timeout` elapses.
</details>

<details>
<summary><strong>Exercise 6: Generate and Configure Self-Signed Fallback SSL Certificates</strong></summary>

**Goal**: Equip the `default_server` with dummy certificates for TLS drops.
1. Generate a self-signed fallback keypair:
```bash
sudo openssl req -x509 -nodes -days 3650 -newkey rsa:2048 \
  -keyout /etc/nginx/ssl/dummy-fallback.key \
  -out /etc/nginx/ssl/dummy-fallback.crt \
  -subj "/CN=untrusted-catchall"
```
2. Lock down file permissions:
```bash
sudo chmod 600 /etc/nginx/ssl/dummy-fallback.key
sudo chmod 644 /etc/nginx/ssl/dummy-fallback.crt
```
3. Link into `00-default-drop.conf` and reload NGINX.
</details>

---

## 12. Summary & Quick Reference

```
+--------------------------------------------------------------------------------+
| NGINX Security Directives Reference                                            |
|                                                                                |
| Directive                     | Context         | Purpose                      |
| :---------------------------- | :-------------- | :--------------------------- |
| server_tokens off;            | http, server    | Hides version number         |
| return 444;                   | server          | Closes connection instantly  |
| client_max_body_size 10m;     | http, server, loc| Rejects large request bodies|
| client_header_timeout 10s;    | http, server    | Defends against Slowloris    |
| proxy_hide_header X-Header;   | http, server, loc| Suppresses upstream headers  |
| limit_except GET POST {deny;} | location        | Restricts allowed verbs      |
| ssl_protocols TLSv1.2 TLSv1.3;| http, server    | Eliminates legacy SSL/TLS    |
| ssl_session_tickets off;      | http, server    | Preserves Forward Secrecy    |
+--------------------------------------------------------------------------------+
```

---

## 13. Further Reading

- [OWASP Secure Headers Project](https://owasp.org/www-project-secure-headers/)
- [Mozilla SSL Configuration Generator](https://ssl-config.mozilla.org/)
- [CIS NGINX Benchmark](https://www.cisecurity.org/benchmark/nginx)
- [Next Concept: WAF Integration & ModSecurity Overview](03-waf-and-modsecurity-overview.md)

---

[← Previous Note: 01-performance-tuning.md](01-performance-tuning.md) | [Back to Module Index](README.md) | [Next Note: 03-waf-and-modsecurity-overview.md →](03-waf-and-modsecurity-overview.md)
