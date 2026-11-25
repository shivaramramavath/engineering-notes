# Security Hardening

## Concept
A default nginx install is reasonably safe, but real deployments add risk through misconfiguration: leaked files, spoofable headers, permissive proxying, and slow-client abuse. Hardening means reducing what is exposed and limiting what a client can make nginx do.

**Prerequisites:** [tls-https.md](../02-core-features/tls-https.md), [rate-limiting.md](../03-traffic-management/rate-limiting.md), [request-lifecycle.md](../04-internals/request-lifecycle.md)

## Hardening Checklist

| Area | Action |
|---|---|
| Information leakage | `server_tokens off;`, hide backend headers |
| Transport | TLS 1.2+/1.3, HSTS, HTTP to HTTPS redirect |
| Headers | Content-Type, framing, referrer, CSP policies |
| Access control | `allow/deny`, auth, restrict sensitive paths |
| Unknown hosts | Catch-all server that rejects them |
| File exposure | Block dotfiles, backups, source control folders |
| Methods | Allow only what the app needs |
| Request limits | Body size, header size, timeouts |
| Abuse control | `limit_req`, `limit_conn`, fail2ban/WAF |
| Process isolation | Unprivileged worker user, correct file permissions |
| Maintenance | Keep nginx and OpenSSL patched |

## Reduce Information Leakage
```nginx
http {
    server_tokens off;                  # hides version in headers and error pages

    # In proxy locations: strip headers that reveal the backend stack
    proxy_hide_header X-Powered-By;
    proxy_hide_header Server;
}
```
`server_tokens off` hides the version but still sends `Server: nginx`. Removing that header entirely needs a third-party module (headers-more). Hiding the version is mainly about not advertising vulnerable versions. It is not a substitute for patching.

## Security Headers
```nginx
add_header X-Content-Type-Options    "nosniff" always;
add_header Referrer-Policy           "strict-origin-when-cross-origin" always;
add_header X-Frame-Options           "SAMEORIGIN" always;
add_header Permissions-Policy        "geolocation=(), camera=(), microphone=()" always;
add_header Content-Security-Policy   "default-src 'self'; frame-ancestors 'self'" always;
add_header Strict-Transport-Security "max-age=31536000; includeSubDomains" always;
```

| Header | Protects against |
|---|---|
| `X-Content-Type-Options: nosniff` | MIME sniffing of uploaded content as scripts |
| `X-Frame-Options` / CSP `frame-ancestors` | Clickjacking (prefer CSP; keep both for old browsers) |
| `Content-Security-Policy` | XSS and injection. Needs tailoring per app, and a too-strict policy breaks pages |
| `Referrer-Policy` | URL leakage to other sites |
| `Permissions-Policy` | Unwanted browser feature access |
| `Strict-Transport-Security` | Downgrade attacks (see [tls-https.md](../02-core-features/tls-https.md) before enabling) |

Rules:
- Always use `always`, or headers are missing on error responses (404, 502, ...).
- **Inheritance trap:** if a `location` defines any `add_header`, it **drops all** `add_header` lines from `server` and `http`. Put shared headers in a snippet and `include` it in every block that adds its own headers. See [config-structure.md](../01-fundamentals/config-structure.md).
- Roll out CSP in report-only mode first (`Content-Security-Policy-Report-Only`).
- Backends that set the same header cause duplicates. Decide which layer owns each header.

## Access Control

### By IP
```nginx
location /admin/ {
    allow 10.0.0.0/8;
    allow 203.0.113.10;
    deny  all;
}
```
Rules are checked in order; the first match wins. End with `deny all;`.

### Basic authentication
```nginx
location /internal/ {
    auth_basic           "Restricted";
    auth_basic_user_file /etc/nginx/.htpasswd;
}
```
```bash
sudo htpasswd -c /etc/nginx/.htpasswd alice     # from apache2-utils / httpd-tools
```
Basic auth is acceptable only over HTTPS and for low-risk areas. For real user accounts, delegate to the application or an identity provider with `auth_request`.

### Delegating authentication
```nginx
location /private/ {
    auth_request /auth;
    proxy_pass http://app;
}
location = /auth {
    internal;
    proxy_pass http://auth-service/verify;
    proxy_pass_request_body off;
    proxy_set_header Content-Length "";
    proxy_set_header X-Original-URI $request_uri;
}
```
A 2xx from the subrequest allows the request; 401 or 403 rejects it.

### Combining IP and auth
`satisfy any;` lets either pass; `satisfy all;` (default) requires both.

## Reject Unknown Hosts
Stops scanners and Host-header abuse from reaching real sites.
```nginx
server {
    listen 80  default_server;
    listen 443 ssl default_server;
    server_name _;
    ssl_reject_handshake on;      # nginx 1.19.4+: no certificate is shown for unknown names
    return 444;                   # closes the connection without a response
}
```

## Block Sensitive Files
```nginx
# Hidden files and folders (.git, .env, .htpasswd), but keep ACME challenges
location ~ /\.(?!well-known) { deny all; }

# Backup and source artifacts
location ~* \.(bak|old|orig|swp|sql|log|ini|conf)$ { deny all; }
```
The best protection is to never place such files under the web root. Deploy only built artifacts.

## Restrict HTTP Methods
```nginx
location /api/ {
    limit_except GET POST { deny all; }
}
```
Or return `405` in a `map`-based check. Disallow `TRACE` (nginx doesn't support it by default, but backends might).

## Request Limits and Slow-Client Defense
```nginx
client_max_body_size        10m;      # default 1m; set per location for upload endpoints
client_body_timeout         10s;
client_header_timeout       10s;
send_timeout                30s;
keepalive_timeout           30s;
client_header_buffer_size   1k;
large_client_header_buffers 4 8k;
```
These shorten the time a slow or malicious client can hold a connection (Slowloris-style attacks). Because nginx is event-driven, it tolerates slow clients well, but limits still prevent exhaustion of connections and file descriptors. Raise sizes only where needed, such as large cookies or upload routes. Combine with `limit_conn` and `limit_req` ([rate-limiting.md](../03-traffic-management/rate-limiting.md)).

## Path Traversal via `alias`
A classic misconfiguration:
```nginx
location /img {                 # missing trailing slash
    alias /var/www/images/;
}
```
A request for `/img../secret.txt` maps to `/var/www/images/../secret.txt`. Make location and alias slashes match: `location /img/ { alias /var/www/images/; }`. Prefer `root` where possible. See [static-files.md](../01-fundamentals/static-files.md).

## Proxy-Specific Risks
- **Open proxy / SSRF:** never build `proxy_pass` from user-controlled values (`proxy_pass http://$arg_target;`). If you must use variables, validate them with `map` against an allowlist.
- **Forwarded header spoofing:** at the outermost nginx, overwrite rather than append client-supplied values:
  ```nginx
  proxy_set_header X-Forwarded-For $remote_addr;   # edge proxy: do not trust the client's value
  ```
  Behind a CDN or load balancer, trust only its ranges via `set_real_ip_from`.
- **Internal endpoints:** mark them `internal;` so clients cannot call them directly (e.g. `X-Accel-Redirect` targets).
- **Cache leaks:** never cache responses containing `Set-Cookie` or personalized data. See [caching.md](../03-traffic-management/caching.md).
- **Backend ports:** bind to `127.0.0.1` or private networks, and firewall them.
- **Upstream TLS:** enable `proxy_ssl_verify on;` for backends you reach over untrusted networks.

## CORS (If You Set It at nginx)
```nginx
map $http_origin $cors_origin {
    default "";
    "https://app.example.com" $http_origin;
}
add_header Access-Control-Allow-Origin $cors_origin always;
add_header Vary Origin always;
```
Never echo arbitrary `Origin` values or use `*` together with credentials.

## Process and File Permissions
```nginx
user www-data;                      # unprivileged worker user (nginx on RHEL)
```
- Workers run unprivileged, only the master runs as root (to bind ports 80/443).
- Web root readable by the worker user, **not writable**.
- Private keys: `chmod 600`, owned by root.
- Config files: owned by root, not writable by the worker user.
- In containers, prefer unprivileged nginx images or `listen 8080` with a non-root user.

## Abuse Control and WAF
- **fail2ban** can ban repeated offenders by parsing the access/error logs (e.g. many 401/403/404 responses).
- A WAF module (ModSecurity-based or Coraza-based with the OWASP Core Rule Set) adds attack signatures, at a cost in CPU and false positives. Run in detection mode first and tune.
- Upstream protection (CDN, cloud DDoS service) is needed for volumetric attacks. nginx alone cannot absorb them.

## Keep It Patched
- Track nginx security advisories and update through your package repo or the official nginx.org repositories.
- Prefer the **stable** branch for production, and rebuild container images regularly.
- Old protocol features have had issues. Keep HTTP/2 and HTTP/3 modules current, and apply vendor advice on connection and stream limits when advisories appear.

## Common Mistakes
- `add_header` in a `location` silently dropping security headers from `server`.
- Headers missing on error pages because `always` was omitted.
- Exposing `.git`, `.env`, or backup files in the web root.
- Using HSTS before every subdomain supports HTTPS.
- `alias` without matching slashes.
- Trusting `X-Forwarded-For` from clients.
- Leaving a default welcome page or a catch-all that serves a real site.
- Basic-auth credentials sent over plain HTTP.
- Relying on `server_tokens off` as protection.

## Debugging and Verification
```bash
curl -sI https://example.com | grep -iE "server|strict-transport|content-security|x-frame|x-content"
curl -sI https://example.com/.git/config        # expect 403/404
curl -sI -H "Host: unknown.example" http://203.0.113.20/   # expect connection closed (444)
sudo nginx -T | grep -n add_header              # find places that override headers
```
Test TLS with SSL Labs or `testssl.sh`, and headers with securityheaders.com or Mozilla Observatory.

## Related / Next
- [performance-tuning.md](performance-tuning.md): tuning without weakening limits
- [troubleshooting](../06-production/troubleshooting.md): diagnosing 403/413/444 responses
- [rate-limiting.md](../03-traffic-management/rate-limiting.md): brute-force and abuse limits
