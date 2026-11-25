# HTTP Error Codes & Rapid Fixes Cheatsheet

> **Cheatsheet ID:** `NGX-SHT-007`  
> **Topic:** Comprehensive Diagnostic Lookup for NGINX HTTP Status Codes, Error Log Signatures, and Directives  
> **Target Version:** NGINX 1.30+ (Stable) / 1.31+ (Mainline)  
> **Related Concept Notes:** [09-error-handling-and-custom-pages.md](../02-core-features/09-error-handling-and-custom-pages.md), [04-troubleshooting-playbooks.md](../06-production-cloud-kubernetes/04-troubleshooting-playbooks.md)

---

## 1. Quick Triage Decision Flowchart

```
Client Reports Error Code
 ├── 301/302 Infinite Loop ──► Check SSL offloader (X-Forwarded-Proto) & URI trailing slashes
 ├── 400 Bad Request ───────► Check large_client_header_buffers & HTTP/2 malformed frames
 ├── 403 Forbidden ─────────► Check OS file permissions, directory indexing (autoindex), allow/deny
 ├── 404 Not Found ─────────► Check root vs alias path concatenation & try_files fallback
 ├── 413 Payload Too Large ─► Check client_max_body_size (default is only 1M)
 ├── 414 URI Too Long ──────► Check large_client_header_buffers
 ├── 421 Misdirected Req ───► HTTP/2 connection reuse across certificates without SNI match
 ├── 429 Too Many Requests ─► limit_req / limit_conn zone exhausted
 ├── 444 No Response ───────► Intentional drop of malicious host headers or scanner probes
 ├── 500 Internal Error ────► rewrite cycle or internal redirect loop in try_files
 ├── 502 Bad Gateway ───────► Upstream down, listening on wrong port, or connection refused
 └── 504 Gateway Timeout ───► Upstream slow query, proxy_read_timeout exceeded
```

---

## 2. Status Code Diagnostic Catalog

### 301/302 Redirect Loop (`ERR_TOO_MANY_REDIRECTS`)
* **Symptom:** Browser fails with `ERR_TOO_MANY_REDIRECTS`.
* **Root Cause 1:** NGINX is behind an SSL-terminating Cloud Load Balancer (AWS ALB, Cloudflare). NGINX redirects HTTP to HTTPS, but ALB talks to NGINX over HTTP without NGINX checking `X-Forwarded-Proto`.
* **Root Cause 2:** Trailing slash mismatch between application and NGINX rewrites.
* **Fix Directive:**
  ```nginx
  # Fix 1: Check upstream protocol header
  if ($http_x_forwarded_proto = "http") {
      return 301 https://$host$request_uri;
  }
  ```
* **CLI Test:** `curl -IL http://example.com`

---

### 400 Bad Request
* **Symptom:** Client receives 400 when sending large cookies, tokens, or malformed HTTP requests.
* **Error Log Signature:** `client sent invalid request while reading client request line` or `client sent too long header line`.
* **Fix Directive:**
  ```nginx
  # Increase header buffer capacity for massive JWTs / cookies
  large_client_header_buffers 4 16k; # (default is 4 8k)
  client_header_buffer_size 4k;
  ```
* **CLI Test:** `curl -v -H "Cookie: $(python3 -c 'print("a"*12000)')" https://example.com/`

---

### 403 Forbidden
* **Symptom:** Access denied to static files or URLs.
* **Error Log Signature:**
  * `directory index of "/var/www/html/" is forbidden` (Missing `index.html` and `autoindex off;`)
  * `*1 open() "/var/www/html/test.html" failed (13: Permission denied)`
* **Fix Directive / Action:**
  ```bash
  # Fix OS permissions: NGINX user (nginx or www-data) requires read access to files + execute on directories
  chown -R www-data:www-data /var/www/html
  find /var/www/html -type d -exec chmod 755 {} +
  find /var/www/html -type f -exec chmod 644 {} +
  ```
  ```nginx
  # If directory listing is actually intended:
  autoindex on;
  ```

---

### 404 Not Found
* **Symptom:** Request returns 404 despite file existing on disk.
* **Root Cause:** Misunderstanding `root` vs `alias`.
  * `root /data; location /images/` -> `/data/images/pic.jpg`
  * `alias /data; location /images/` -> `/data/pic.jpg`
* **Fix Directive:**
  ```nginx
  # SPA Fallback Fix (React / Vue / Angular):
  location / {
      root /var/www/app/dist;
      index index.html;
      try_files $uri $uri/ /index.html;
  }
  ```

---

### 413 Payload Too Large (`413 Request Entity Too Large`)
* **Symptom:** File uploads fail immediately with HTTP 413.
* **Error Log Signature:** `client intended to send too large body: NNNNNNN bytes`.
* **Root Cause:** Default `client_max_body_size` is 1MB.
* **Fix Directive:**
  ```nginx
  # In http, server, or location block:
  client_max_body_size 50M;
  ```
* **CLI Test:** `curl -v -F "file=@largefile.zip" https://example.com/upload`

---

### 414 URI Too Long
* **Symptom:** Massive query string parameter requests fail.
* **Error Log Signature:** `client sent too long URI`.
* **Fix Directive:**
  ```nginx
  large_client_header_buffers 4 32k;
  ```

---

### 421 Misdirected Request
* **Symptom:** Client connects to Domain A, but request is sent for Domain B and rejected.
* **Root Cause:** HTTP/2 connection reuse (coalescing). If Domain A and Domain B resolve to the same IP and share an SSL certificate (e.g. wildcard or SAN), HTTP/2 clients reuse the TLS connection. If NGINX server blocks have mismatched auth/SSL, 421 is returned.
* **Fix Directive:**
  ```nginx
  # In NGINX 1.25.1+, ensure http2 is isolated per server or disable coalescing if configs conflict:
  http2 on;
  # Alternatively, separate SSL certificates onto dedicated IPs or use distinct SAN certs
  ```

---

### 429 Too Many Requests
* **Symptom:** API consumers or web browsers receive 429 throttled response.
* **Error Log Signature:** `limiting requests, excess: X.XXX by zone "name"`.
* **Fix Directive:**
  ```nginx
  # Tune rate limiting parameters to accommodate bursty clients:
  limit_req zone=api_limit burst=30 nodelay;
  ```

---

### 444 Connection Closed Without Response
* **Symptom:** Connection terminated abruptly with TCP RST; no HTTP response sent.
* **Purpose:** Special non-standard NGINX return code used to terminate malicious scanners and unrecognized `Host` headers with zero memory/bandwidth overhead.
* **Example Usage:**
  ```nginx
  # Default catch-all server
  server {
      listen 80 default_server;
      listen 443 ssl default_server;
      ssl_certificate     /etc/ssl/dummy.crt;
      ssl_certificate_key /etc/ssl/dummy.key;
      server_name _;
      return 444;
  }
  ```

---

### 500 Internal Server Error
* **Symptom:** Server throws generic 500 error.
* **Error Log Signature:**
  * `rewrite or internal redirection cycle while internally redirecting to "/index.html"`
* **Root Cause:** `try_files` or `rewrite` directives infinite loop without reaching a static file or final terminal handler.
* **Fix Directive:**
  ```nginx
  # Broken:
  try_files $uri $uri/ /index.html; # if index.html itself does not exist in root!
  # Fixed:
  # Verify root path exists and index.html is present in that root.
  ```

---

### 502 Bad Gateway
* **Symptom:** NGINX returns 502 when proxying to an upstream application.
* **Error Log Signatures & Fixes:**
  1. `connect() failed (111: Connection refused) while connecting to upstream`
     * *Cause:* Upstream app (Node, FastAPI, Gunicorn, Spring Boot) is stopped or listening on wrong port/interface (e.g., listening on `127.0.0.1` while NGINX is in Docker container connecting to `localhost`).
     * *Fix:* Start app: `systemctl start myapp` or check container bridge IP.
  2. `connect() failed (13: Permission denied) while connecting to upstream`
     * *Cause:* SELinux is blocking NGINX from opening network sockets.
     * *Fix:* `setsebool -P httpd_can_network_connect 1`
  3. `connect() to unix:/run/app.sock failed (2: No such file or directory)`
     * *Cause:* Unix domain socket not created or path typo.
     * *Fix:* Check socket creation in app configuration.
  4. `connect() to unix:/run/app.sock failed (13: Permission denied)`
     * *Cause:* Socket permissions too restrictive.
     * *Fix:* Set socket chmod to 660 with group ownership `www-data`.
  5. `upstream sent too big header while reading response header from upstream`
     * *Cause:* Upstream response contains massive headers or large cookies exceeding buffer.
     * *Fix:*
       ```nginx
       proxy_buffer_size 128k;
       proxy_buffers 4 256k;
       proxy_busy_buffers_size 256k;
       ```

---

### 504 Gateway Timeout
* **Symptom:** Request hangs for 60 seconds (default) before failing with 504.
* **Error Log Signature:** `upstream timed out (110: Connection timed out) while reading response header from upstream`.
* **Root Cause:** Upstream database query, external API call, or worker thread blocked and failed to reply within `proxy_read_timeout`.
* **Fix Directive:**
  ```nginx
  location /api/long-running-export/ {
      proxy_pass http://backend;
      proxy_connect_timeout 60s;
      proxy_send_timeout    300s;
      proxy_read_timeout    300s;
  }
  ```

---

## 3. Fast Diagnostic Shell Script

Run this command directly on any Linux host running NGINX:

```bash
# Display the last 30 NGINX errors with timestamps and root causes:
sudo tail -n 30 /var/log/nginx/error.log

# Scan for top HTTP response codes in the last 10,000 requests:
tail -n 10000 /var/log/nginx/access.log | awk '{print $9}' | sort | uniq -c | sort -nr
```
