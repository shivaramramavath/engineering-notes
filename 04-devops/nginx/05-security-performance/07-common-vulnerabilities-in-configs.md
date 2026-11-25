---
title: "Common NGINX Configuration Vulnerabilities & Misconfiguration Auditing"
description: "In-depth security analysis of prevalent NGINX configuration flaws, including off-by-slash alias path traversal, CRLF header injection, SSRF, variable scope traps, and automated auditing with GIXY."
module: "05-security-performance"
topic: "07-common-vulnerabilities-in-configs"
status: "completed"
tags: ["nginx", "security", "vulnerabilities", "misconfigurations", "alias-traversal", "crlf-injection", "ssrf", "gixy", "audit"]
prerequisites: ["05-security-performance/02-security-hardening.md", "01-fundamentals/03-server-and-location.md"]
estimated_read_time: "35 min"
---

# Common NGINX Configuration Vulnerabilities & Misconfiguration Auditing

```
+-------------------------------------------------------------------------------+
| Anatomical Breakdown of Prevalent Configuration Exploits                      |
|                                                                               |
|  1. Off-By-Slash Alias Path Traversal                                         |
|     location /files { alias /var/www/files/; }                                |
|     Attacker GET /files../secret.txt  ==> Resolves to: /var/www/secret.txt!   |
|                                                                               |
|  2. CRLF Injection via Unencoded Variables in Redirects                       |
|     return 302 https://$host$uri;                                            |
|     Attacker GET /%0d%0aSet-Cookie:admin=1  ==> Injects arbitrary headers!    |
|                                                                               |
|  3. Server-Side Request Forgery (SSRF) via Dynamic Proxying                   |
|     location /proxy { proxy_pass http://$arg_target; }                        |
|     Attacker GET /proxy?target=169.254.169.254/latest/meta-data/credentials  |
|                                                                               |
|  4. Insecure Host Header Routing & Cache Poisoning                            |
|     proxy_set_header Host $http_host;                                         |
|     Allows cache poisoning if Host header is unsanitized!                     |
+-------------------------------------------------------------------------------+
```

---

## Table of Contents
1. [What & Why](#1-what--why)
2. [Vulnerability 1: Off-by-Slash Alias Path Traversal](#2-vulnerability-1-off-by-slash-alias-path-traversal)
3. [Vulnerability 2: CRLF Injection via `$uri` in Redirects](#3-vulnerability-2-crlf-injection-via-uri-in-redirects)
4. [Vulnerability 3: SSRF via Unsanitized Dynamic `proxy_pass`](#4-vulnerability-3-ssrf-via-unsanitized-dynamic-proxy_pass)
5. [Vulnerability 4: Host Header Injection & Web Cache Poisoning](#5-vulnerability-4-host-header-injection--web-cache-poisoning)
6. [Vulnerability 5: Variable Scoping Traps & The "If is Evil" Pitfall](#6-vulnerability-5-variable-scoping-traps--the-if-is-evil-pitfall)
7. [Vulnerability 6: Missing Root / Default Root Information Disclosure](#7-vulnerability-6-missing-root--default-root-information-disclosure)
8. [Automated Security Auditing with GIXY & Static Linters](#8-automated-security-auditing-with-gixy--static-linters)
9. [Architectural Comparison: Vulnerable vs Remedied Directives](#9-architectural-comparison-vulnerable-vs-remedied-directives)
10. [Technical Interview Questions & Scenarios](#10-technical-interview-questions--scenarios)
11. [Hands-On Practice Exercises](#11-hands-on-practice-exercises)
12. [Summary & Quick Reference](#12-summary--quick-reference)
13. [Further Reading](#13-further-reading)

---

## 1. What & Why

While the NGINX C core is renowned for its stability and security, real-world compromises against NGINX infrastructures almost never stem from memory corruption bugs in NGINX itself. Instead, the overwhelming majority of breaches arise from **semantic misconfigurations in configuration files**:
- **Subtle Syntax Nuances**: Missing a single trailing slash (`/`) on an `alias` directive transforms a benign static file location into an arbitrary filesystem traversal vulnerability.
- **Variable Normalization Differences**: Using `$uri` instead of `$request_uri` in HTTP redirects introduces CRLF injection that enables Cross-Site Scripting (XSS) and HTTP response splitting.
- **Dynamic Variable Proxying**: Passing client-supplied query parameters directly into `proxy_pass` opens the internal network to Server-Side Request Forgery (SSRF) and cloud metadata exfiltration.

Understanding these subtle behavioral landmines is essential for DevSecOps engineers auditing, hardening, and operating production reverse proxies.

---

## 2. Vulnerability 1: Off-by-Slash Alias Path Traversal

### The Vulnerable Configuration
```nginx
# VULNERABLE: Notice location does NOT end with a slash, but alias DOES
location /static {
    alias /var/www/app/static/;
}
```

### The Attack Mechanics
When NGINX matches `location /static`, it replaces the prefix `/static` in the request URI with `/var/www/app/static/`:
1. Attacker sends: `GET /static../private/secret.key HTTP/1.1`
2. NGINX strips `/static` and appends the remainder (`../private/secret.key`) to the alias path.
3. Computed filesystem path:
   $$\text{Path} = \text{/var/www/app/static/} + \text{../private/secret.key} = \text{/var/www/app/private/secret.key}$$
4. The attacker escapes the `/static/` directory and reads arbitrary files from the parent directory!

### The Secure Fix
Always ensure that if the `alias` directive ends with a slash, the `location` path **must also end with a trailing slash**:
```nginx
# SECURE: Both location and alias have matching trailing slashes
location /static/ {
    alias /var/www/app/static/;
}
```
*Better Alternative*: Whenever possible, prefer `root` over `alias`. The `root` directive appends the full request path to the root folder, rendering traversal impossible.

---

## 3. Vulnerability 2: CRLF Injection via `$uri` in Redirects

### The Vulnerable Configuration
```nginx
# VULNERABLE: Uses normalized $uri in redirection
server {
    listen 80;
    server_name example.com;
    return 301 https://$host$uri;
}
```

### The Attack Mechanics
- `$uri`: NGINX normalizes this variable. It decodes URL-encoded characters (like `%0d%0a`) and strips query strings.
- `$request_uri`: Contains the **raw, unparsed, un-decoded** original request line exactly as sent by the client, including query arguments.

If an attacker sends an encoded CRLF sequence (`%0d%0a` represents `\r\n` in HTTP):
```http
GET /%0d%0aSet-Cookie:%20session_id=attacker_controlled;%20Domain=example.com HTTP/1.1
Host: example.com
```
Because `$uri` decodes `%0d%0a` into literal carriage return and line feed characters, the resulting HTTP response header emitted by NGINX splits into two headers:
```http
HTTP/1.1 301 Moved Permanently
Location: https://example.com/
Set-Cookie: session_id=attacker_controlled; Domain=example.com
```
The attacker has injected an arbitrary HTTP header, enabling Session Fixation, XSS, and cache poisoning!

### The Secure Fix
Always use `$request_uri` in redirects, which preserves URL-encoding and prevents CRLF header injection:
```nginx
# SECURE: Uses raw, un-decoded $request_uri
server {
    listen 80;
    server_name example.com;
    return 301 https://$host$request_uri;
}
```

---

## 4. Vulnerability 3: SSRF via Unsanitized Dynamic `proxy_pass`

### The Vulnerable Configuration
Developers building image resizing proxies or webhook forwarders sometimes construct upstream URLs dynamically:
```nginx
# VULNERABLE: Arbitrary user-controlled proxy target
location /fetch {
    resolver 1.1.1.1;
    proxy_pass http://$arg_url;
}
```

### The Attack Mechanics
An attacker uses your NGINX proxy to probe internal private networks or exfiltrate cloud infrastructure credentials:
```bash
# Steal AWS EC2 Instance Identity Metadata
curl "http://public-nginx.com/fetch?url=169.254.169.254/latest/meta-data/iam/security-credentials/"

# Probe internal Kubernetes cluster services
curl "http://public-nginx.com/fetch?url=kubernetes.default.svc.cluster.local:443"
```

### The Secure Fix
1. Whitelist permitted domains using a `map` block in the `http {}` context.
2. Explicitly deny internal IP addresses and cloud metadata ranges (169.254.169.254, 10.0.0.0/8, 172.16.0.0/12, 192.168.0.0/16, 127.0.0.0/8):

```nginx
# SECURE: Domain whitelisting for dynamic proxies
map $arg_url $allowed_target {
    default                             0;
    ~^https://assets\.partner\.com/     1;
    ~^https://images\.cdn\.com/         1;
}

server {
    location /fetch {
        if ($allowed_target = 0) {
            return 403 "Forbidden Proxy Destination\n";
        }

        resolver 1.1.1.1 valid=30s ipv6=off;
        proxy_pass $arg_url;
    }
}
```

---

## 5. Vulnerability 4: Host Header Injection & Web Cache Poisoning

### The Vulnerable Configuration
```nginx
# VULNERABLE: Blindly forward client-supplied Host header
location / {
    proxy_pass http://backend_pool;
    proxy_set_header Host $http_host;
    proxy_cache my_cache;
}
```

### The Attack Mechanics
1. `$http_host` contains whatever string the client passed in the `Host` request header, without validation.
2. Attacker sends:
   ```http
   GET /reset-password HTTP/1.1
   Host: attacker-controlled.com
   ```
3. The upstream backend generates a password reset email using the host header:
   `https://attacker-controlled.com/reset?token=xyz`
4. If NGINX caches this response, subsequent legitimate users clicking "Forgot Password" receive the poisoned cached response containing the attacker's domain!

### The Secure Fix
- Use `$host`, which prioritizes the validated server name or matches the primary host:
```nginx
# SECURE: Uses sanitized $host
proxy_set_header Host $host;
```
- Define an explicit catch-all `default_server` returning `444` to reject requests with invalid or unrecognized `Host` headers.

---

## 6. Vulnerability 5: Variable Scoping Traps & The "If is Evil" Pitfall

In NGINX, `set` directives and `if` blocks execute strictly during the `REWRITE` phase. They do not execute procedurally in the order they appear inside a location block!

### The Trap
```nginx
# FLAWED LOGIC: Attempting to override variables inside 'if'
location /download/ {
    set $secure_path "public";

    if ($cookie_auth) {
        set $secure_path "authenticated";
    }

    # Because 'if' creates an internal location, directives below
    # may fail to execute or behave unpredictably!
    proxy_pass http://storage/$secure_path;
}
```
Inside a `location` block, an `if` directive creates an invisible, nested, internal location. Many standard directives (such as `proxy_pass`, `try_files`, or `add_header`) fail to inherit properly, resulting in subtle 404s, infinite internal redirect loops, or silent security bypasses.

### The Solution: Use `map` in `http {}`
Replace procedural conditional logic with declarative hash maps:
```nginx
http {
    map $cookie_auth $secure_path {
        default   "public";
        "~^.+"    "authenticated";
    }

    server {
        location /download/ {
            proxy_pass http://storage/$secure_path;
        }
    }
}
```

---

## 7. Vulnerability 6: Missing Root / Default Root Information Disclosure

### The Vulnerable Configuration
```nginx
server {
    listen 80;
    server_name example.com;

    # Notice: NO root directive in server block!

    location /app {
        proxy_pass http://127.0.0.1:3000;
    }
}
```
If a request is received for `/etc/nginx/nginx.conf` or `/var/log/`, and the server block lacks a `root` directive, NGINX falls back to the compiled default root (often `/usr/share/nginx/html` or `/etc/nginx` on some distributions). Depending on the platform, an unhandled root path can expose system files.

### The Secure Fix
Always define an explicit `root` or `return 404` at the `server {}` level:
```nginx
server {
    listen 80;
    server_name example.com;
    root /var/empty; # Explicit non-existent empty directory

    location / {
        return 404;
    }

    location /app {
        proxy_pass http://127.0.0.1:3000;
    }
}
```

---

## 8. Automated Security Auditing with GIXY & Static Linters

Manual code review inevitably misses edge cases. Use automated static analysis tools in CI/CD pipelines to audit NGINX configurations before deployment:

### Tooling 1: GIXY (Static Analysis Tool for NGINX)
GIXY scans configurations for known security anti-patterns:
- Alias path traversal
- SSRF in `proxy_pass`
- HTTP response splitting / CRLF injection
- Permissive CORS wildcards
- Mutually exclusive directives

```bash
# Install GIXY (Python based)
pip install gixy

# Scan configuration tree
gixy /etc/nginx/nginx.conf
```

Example GIXY Output flagging an off-by-slash traversal:
```text
====================
Problem: [alias_traversal] Path traversal via misconfigured alias.
Issue file: /etc/nginx/conf.d/static.conf:14
Summary: The alias directive is used without a trailing slash inside a location with a trailing slash.
====================
```

### Tooling 2: Nginx-Pwner & Semgrep Rules
Integrate Semgrep rules in your GitHub Actions workflows to block commits containing insecure `$uri` redirects or missing slashes:
```yaml
# .github/workflows/nginx-audit.yml
name: NGINX Security Linter
on: [push, pull_request]

jobs:
  audit:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v4
      - name: Run Gixy Security Scan
        run: |
          docker run --rm -v $(pwd):/etc/nginx yandex/gixy /etc/nginx/nginx.conf
```

---

## 9. Architectural Comparison: Vulnerable vs Remedied Directives

| Vulnerability Vector | Insecure Syntax | Secure Syntax | Exploit Risk |
| :--- | :--- | :--- | :--- |
| **Alias Traversal** | `location /img { alias /www/img/; }` | `location /img/ { alias /www/img/; }` | Arbitrary local file read |
| **CRLF Injection** | `return 301 https://$host$uri;` | `return 301 https://$host$request_uri;` | Header injection, XSS |
| **Host Poisoning** | `proxy_set_header Host $http_host;` | `proxy_set_header Host $host;` | Cache poisoning, phishing |
| **Dynamic SSRF** | `proxy_pass http://$arg_url;` | `map` domain validation + private CIDR deny | Cloud metadata exfiltration |
| **Catch-All Routing**| Missing `default_server` | `listen 80 default_server; return 444;` | Direct IP scanning, SNI bypass |
| **Header Loss** | Isolated `add_header` in child block | Re-include shared snippet across all blocks | Vanishing CSP / HSTS |

---

## 10. Technical Interview Questions & Scenarios

<details>
<summary><strong>1. Explain the root cause of the "off-by-slash" alias path traversal bug in NGINX.</strong></summary>

When an `alias` directive is used, NGINX performs a string replacement. If the `location` directive does not have a trailing slash (e.g. `location /static`), but the `alias` directive specifies a directory (e.g. `alias /var/www/static/;`), NGINX drops `/static` from the incoming URI and appends whatever characters remain to the alias prefix. When a client requests `/static../secret.txt`, the remaining portion is `../secret.txt`. Appended to `/var/www/static/`, the resulting path evaluates to `/var/www/static/../secret.txt`, which the filesystem normalizes to `/var/www/secret.txt`. This enables an attacker to traverse out of the intended directory.
</details>

<details>
<summary><strong>2. Why does using `$uri` in redirects cause CRLF injection while `$request_uri` is safe?</strong></summary>

`$uri` is a normalized variable. During request processing, NGINX decodes percent-encoded characters in `$uri` (converting `%0d%0a` into raw byte values `\r\n`). If `$uri` is used in a redirect (`return 301 https://$host$uri;`), the raw carriage return and line feed characters are written directly into the HTTP response stream, creating new header lines. In contrast, `$request_uri` is the completely unparsed, un-decoded original string sent by the client. Percent-encoded sequences remain `%0d%0a`, preventing HTTP header splitting.
</details>

<details>
<summary><strong>3. Why is it dangerous to specify a DNS resolver and use a variable in `proxy_pass` without domain validation?</strong></summary>

When `proxy_pass` contains a variable (e.g. `proxy_pass http://$backend_host;`), NGINX does not resolve the hostname during configuration compilation; instead, it dynamically queries the configured `resolver` at runtime for every request. If `$backend_host` is derived from an untrusted client header or query parameter, the attacker can supply internal IP addresses (e.g., `169.254.169.254` for AWS metadata or `10.0.0.1` for internal microservices), turning NGINX into an open SSRF proxy.
</details>

<details>
<summary><strong>4. How can the "add_header inheritance rule" create a security vulnerability?</strong></summary>

NGINX array directives (such as `add_header`) do not merge between configuration levels. If you define critical security headers (Content-Security-Policy, HSTS, X-Frame-Options) in the `http {}` or `server {}` block, and then define a single local header (e.g., `add_header Cache-Control "no-cache";`) inside a sensitive `location /api/` block, NGINX completely wipes out all inherited headers for that location. As a result, the sensitive endpoint is served with zero security headers.
</details>

<details>
<summary><strong>5. What is the difference between `proxy_pass http://upstream;` and `proxy_pass http://upstream/;` (with a trailing slash)?</strong></summary>

The presence of a trailing slash alters how NGINX constructs the request URI forwarded to the upstream:
- Without trailing slash (`proxy_pass http://upstream;`): NGINX passes the incoming client URI untouched to the backend.
- With trailing slash (`proxy_pass http://upstream/;`): NGINX performs URI normalization and replacement. It strips the matched `location` prefix from the client URI and appends the remainder to `/`. If the location was regex-based, adding a trailing slash is a syntax error.
</details>

<details>
<summary><strong>6. Why is `if ($http_user_agent ~* "badbot") { return 403; }` safe, while other `if` directives are discouraged?</strong></summary>

Using `if` inside a `location` block is considered unsafe ("If is Evil") because it creates an internal nested location that fails to inherit content-phase handlers properly. However, using `return` or `rewrite ... last;` inside an `if` block is safe because both directives immediately terminate further request processing and redirect execution, preventing the request from falling into an uninitialized phase handler state.
</details>

<details>
<summary><strong>7. How can an attacker abuse `proxy_set_header X-Forwarded-For $http_x_forwarded_for;`?</strong></summary>

By using `$http_x_forwarded_for`, NGINX blindly forwards the incoming client-supplied header without validation. If the backend trusts this header to determine user IP addresses for authentication or access control, the client can forge any arbitrary IP. Instead, NGINX should use `$proxy_add_x_forwarded_for`, which appends the real TCP socket IP (`$remote_addr`) to the chain, or enforce `ngx_http_realip_module` with strict `set_real_ip_from` subnets.
</details>

<details>
<summary><strong>8. What security issue arises from defining `root` inside multiple `location` blocks instead of the `server` block?</strong></summary>

Defining `root` inside individual `location` blocks increases configuration drift and leaves unhandled URI paths unmapped. If a request arrives that does not match any specific location block, NGINX falls back to its compiled default root directory, potentially serving distribution configuration files, documentation, or internal artifacts. The best practice is to declare an authoritative `root` at the `server` level and override it only where strictly necessary.
</details>

---

## 11. Hands-On Practice Exercises

<details>
<summary><strong>Exercise 1: Reproduce and Exploit the Off-by-Slash Alias Traversal</strong></summary>

**Goal**: Demonstrate how omitting a trailing slash exposes sibling directories.
1. Create directories:
```bash
mkdir -p /var/www/test/public /var/www/test/private
echo "Public content" > /var/www/test/public/index.html
echo "SECRET_API_KEY=9999" > /var/www/test/private/secret.env
```
2. In `nginx.conf`, set:
```nginx
location /public {
    alias /var/www/test/public/;
}
```
3. Exploit via curl:
```bash
curl http://localhost/public../private/secret.env
```
4. Verify the secret file contents are displayed.
5. Fix the configuration by adding a trailing slash (`location /public/`) and verify curl now returns 404.
</details>

<details>
<summary><strong>Exercise 2: Demonstrate CRLF Header Injection via `$uri`</strong></summary>

**Goal**: Observe arbitrary header injection in HTTP responses.
1. Configure an insecure redirect:
```nginx
server {
    listen 8080;
    return 301 http://$host$uri;
}
```
2. Issue a crafted request:
```bash
curl -v "http://localhost:8080/%0d%0aInjected-Header:%20Compromised"
```
3. Observe `Injected-Header: Compromised` appearing as an independent response header.
4. Replace `$uri` with `$request_uri`, reload, and observe that the sequence remains URL-encoded.
</details>

<details>
<summary><strong>Exercise 3: Run GIXY Static Analysis on Live Configurations</strong></summary>

**Goal**: Automate misconfiguration detection.
1. Install GIXY: `pip install gixy`.
2. Run GIXY against `/etc/nginx/nginx.conf`.
3. Review the reported security issues and verify how many relate to alias traversal or response splitting.
</details>

<details>
<summary><strong>Exercise 4: Remediate Insecure Dynamic SSRF Proxying</strong></summary>

**Goal**: Secure a dynamic webhook endpoint.
1. Configure a vulnerable location:
```nginx
location /proxy {
    resolver 8.8.8.8;
    proxy_pass http://$arg_dest;
}
```
2. Refactor the block using the `map` whitelist approach from Section 4.
3. Verify that requests targeting `169.254.169.254` or `127.0.0.1` are blocked with HTTP 403.
</details>

<details>
<summary><strong>Exercise 5: Audit Header Inheritance Across Nested Locations</strong></summary>

**Goal**: Verify that security headers are not dropped.
1. Add `add_header X-Global-Secure "1";` in `server {}`.
2. In `location /api/`, add `add_header X-Local-API "1";`.
3. Curl `/api/` and inspect response headers. Notice `X-Global-Secure` is missing!
4. Refactor using an included snippet file and confirm both headers appear.
</details>

<details>
<summary><strong>Exercise 6: Enforce an Empty Default Root</strong></summary>

**Goal**: Prevent accidental filesystem disclosure on unhandled paths.
1. Set `root /var/empty;` in the `server` block.
2. Ensure `/var/empty` exists and has permissions `000` or `555`.
3. Request non-existent paths and confirm NGINX returns 404 without reading sensitive parent files.
</details>

---

## 12. Summary & Quick Reference

```
+--------------------------------------------------------------------------------+
| NGINX Configuration Security Remediation Matrix                                |
|                                                                                |
| Pattern                       | Vulnerable Syntax     | Remediated Syntax      |
| :---------------------------- | :-------------------- | :--------------------- |
| Alias Traversal               | location /dir         | location /dir/         |
|                               | alias /www/dir/;      | alias /www/dir/;       |
| Redirect Variable             | return 301 ..$uri;    | return 301 ..$request_uri;|
| Proxy Host Header             | $http_host            | $host                  |
| Header Inheritance            | Single add_header     | include snippets/..    |
| Dynamic Proxy                 | proxy_pass $arg_url;  | map-based whitelist    |
| Catch-All Server              | (Omitted)             | server_name _; ret 444;|
+--------------------------------------------------------------------------------+
```

---

## 13. Further Reading

- [Yandex GIXY: NGINX Configuration Static Analyzer](https://github.com/yandex/gixy)
- [Detecting NGINX Misconfigurations: PortSwigger Web Security](https://portswigger.net/research)
- [Bhaveesh Naik: Common NGINX Misconfigurations](https://github.com/streaak/key-nginx-misconfigurations)
- [Next Module: Production Operations, Cloud & Kubernetes](../06-production-operations/README.md)

---

[← Previous Note: 06-benchmarking-methodologies.md](06-benchmarking-methodologies.md) | [Back to Module Index](README.md) | [Next Module: 06-production-operations →](../06-production-operations/README.md)
