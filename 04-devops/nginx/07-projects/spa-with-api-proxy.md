# Project: SPA with API Proxy

## Goal
Serve a single-page application (React, Vue, Angular, Svelte, ...) and its backend API from **one domain** over HTTPS:

```
                        ┌────────────────────────── nginx ───────────────────────────┐
 Browser ──HTTPS──►     │  /            →  static files (SPA, fallback to index.html) │
 app.example.com        │  /assets/*    →  static files, cached for 1 year           │
                        │  /api/*       →  proxy to Node/Python/Go on 127.0.0.1:3000 │
                        │  /api/auth/*  →  same, with a much stricter rate limit      │
                        └──────────────────────────────────────────────────────────────┘
```

Same-origin hosting means **no CORS configuration**, one certificate, and one place to apply caching, compression, and security headers.

## What You Will Practice

| Skill | Topic file |
|---|---|
| `root`, `try_files`, SPA fallback | [static-files.md](../01-fundamentals/static-files.md) |
| Location matching (`=`, prefix, regex) | [server-and-location.md](../01-fundamentals/server-and-location.md) |
| `proxy_pass`, headers, timeouts | [reverse-proxy.md](../02-core-features/reverse-proxy.md) |
| HTTPS, redirect, Let's Encrypt | [tls-https.md](../02-core-features/tls-https.md) |
| Upstream keepalive | [load-balancing.md](../03-traffic-management/load-balancing.md) |
| Rate limiting login endpoints | [rate-limiting.md](../03-traffic-management/rate-limiting.md) |
| Security headers and the `add_header` trap | [security-hardening.md](../05-security-performance/security-hardening.md) |
| Compression and caching | [performance-tuning.md](../05-security-performance/performance-tuning.md) |
| Zero-downtime deploys | [operations.md](../06-production/operations.md) |

## Prerequisites
- A Linux server with nginx and Certbot installed ([installation.md](../01-fundamentals/installation.md)).
- DNS: `app.example.com` points to the server. Ports 80 and 443 are open.
- A built SPA in a `dist/` folder, containing `index.html` and a `assets/` folder with fingerprinted filenames (Vite, CRA, Angular, and similar tools produce these).
- A backend listening on `127.0.0.1:3000`, serving paths under `/api/`. For a quick stand-in, `python3 -m http.server 3000 --bind 127.0.0.1` is enough to test proxying.

Replace `app.example.com` throughout with your domain.

## Step 1: Directory Layout and Deploy Script

```
/var/www/app/
├── releases/
│   ├── 20261008101500/      # each deploy = a new folder
│   └── 20261009093000/
└── current -> releases/20261009093000     # symlink that nginx serves
```

`deploy.sh`:
```bash
#!/usr/bin/env bash
set -euo pipefail

BASE=/var/www/app
REL="$BASE/releases/$(date +%Y%m%d%H%M%S)"

mkdir -p "$REL"
rsync -a dist/ "$REL"/
chmod -R a+rX "$REL"                       # nginx worker user must be able to read

ln -sfn "$REL" "$BASE/current.tmp"
mv -T "$BASE/current.tmp" "$BASE/current"  # atomic switch, no half-deployed state

# keep the last 5 releases
ls -1dt "$BASE"/releases/* | tail -n +6 | xargs -r rm -rf
```
No nginx reload is needed for new frontend files because nginx resolves the symlink on every request (leave `open_file_cache` off for this site). **Rollback** is pointing `current` at an older release the same way.

Keeping several releases matters: browsers with a cached old `index.html` may still request old hashed assets for a while.

## Step 2: Snippets (Shared Config)

Two small files avoid repeating directives and make the `add_header` inheritance rule easy to handle.

```bash
sudo mkdir -p /etc/nginx/snippets /var/www/certbot
```

`/etc/nginx/snippets/security-headers.conf`:
```nginx
add_header X-Content-Type-Options "nosniff" always;
add_header X-Frame-Options        "SAMEORIGIN" always;
add_header Referrer-Policy        "strict-origin-when-cross-origin" always;
add_header Strict-Transport-Security "max-age=300" always;   # raise to 31536000 once everything works
```
Add a `Content-Security-Policy` suited to your app, ideally starting with `Content-Security-Policy-Report-Only`.

`/etc/nginx/snippets/proxy-params.conf`:
```nginx
proxy_http_version 1.1;
proxy_set_header Host              $host;
proxy_set_header X-Real-IP         $remote_addr;
proxy_set_header X-Forwarded-For   $remote_addr;      # nginx is the edge: don't trust client-sent values
proxy_set_header X-Forwarded-Proto $scheme;
```
Make sure your backend trusts these headers (for example Express `trust proxy`, Django `SECURE_PROXY_SSL_HEADER`) **only** from nginx.

## Step 3: HTTP Server and Certificate

The HTTPS config references certificate files that do not exist yet, so start with HTTP only.

`/etc/nginx/conf.d/app-http.conf`:
```nginx
server {
    listen 80;
    server_name app.example.com;

    location /.well-known/acme-challenge/ {
        root /var/www/certbot;
    }
    location / {
        return 301 https://$host$request_uri;
    }
}
```
```bash
sudo nginx -t && sudo systemctl reload nginx
sudo certbot certonly --webroot -w /var/www/certbot -d app.example.com
```

Automatic renewal needs a reload hook so nginx picks up new certificates:
```bash
sudo tee /etc/letsencrypt/renewal-hooks/deploy/reload-nginx.sh >/dev/null <<'EOF'
#!/bin/sh
systemctl reload nginx
EOF
sudo chmod +x /etc/letsencrypt/renewal-hooks/deploy/reload-nginx.sh
sudo certbot renew --dry-run
```
(Shortcut: `certbot --nginx -d app.example.com` edits your config for you. Writing it by hand, as here, is better for learning and keeps your config predictable.)

## Step 4: The HTTPS Server

`/etc/nginx/conf.d/app-https.conf`:
```nginx
# --- Shared definitions (http context, because conf.d files are included there) ---
upstream api_backend {
    server 127.0.0.1:3000;
    keepalive 16;                       # reuse connections to the backend
}

limit_req_zone $binary_remote_addr zone=api:10m   rate=20r/s;
limit_req_zone $binary_remote_addr zone=login:10m rate=10r/m;
limit_req_status 429;

gzip on;
gzip_comp_level 5;
gzip_min_length 256;
gzip_vary on;
gzip_proxied any;
gzip_types text/css text/javascript application/javascript application/json
           image/svg+xml text/plain;    # also compresses text/html by default

# --- The site ---
server {
    listen 443 ssl;
    http2 on;                           # nginx 1.25.1+; older: listen 443 ssl http2;
    server_name app.example.com;

    ssl_certificate     /etc/letsencrypt/live/app.example.com/fullchain.pem;
    ssl_certificate_key /etc/letsencrypt/live/app.example.com/privkey.pem;
    ssl_protocols       TLSv1.2 TLSv1.3;
    ssl_session_cache   shared:SSL:10m;
    ssl_session_timeout 1d;
    ssl_session_tickets off;

    root  /var/www/app/current;
    index index.html;

    # Applies to every location that does NOT define its own add_header
    include snippets/security-headers.conf;

    # Return JSON, not an nginx error page, when the API is down
    error_page 502 503 504 = @api_down;

    # ---------- API ----------
    location /api/ {
        limit_req zone=api burst=40 nodelay;
        client_max_body_size 5m;
        include snippets/proxy-params.conf;
        proxy_set_header Connection "";           # required for upstream keepalive
        proxy_read_timeout 30s;
        proxy_pass http://api_backend;            # no URI part: /api/x stays /api/x
    }

    # Longer prefix beats /api/, so credential endpoints get a tighter limit
    location /api/auth/ {
        limit_req zone=login burst=5 nodelay;
        include snippets/proxy-params.conf;
        proxy_set_header Connection "";
        proxy_pass http://api_backend;
    }

    # ---------- Static assets (fingerprinted filenames) ----------
    location /assets/ {
        try_files $uri =404;                      # never fall back to index.html here
        access_log off;
        add_header Cache-Control "public, max-age=31536000, immutable" always;
        include snippets/security-headers.conf;   # needed again: this block has its own add_header
    }

    # ---------- App shell: never cached long ----------
    location = /index.html {
        add_header Cache-Control "no-cache" always;
        include snippets/security-headers.conf;
    }

    # ---------- SPA fallback ----------
    location / {
        try_files $uri $uri/ /index.html;
    }

    # ---------- Misc ----------
    location ~ /\.(?!well-known) { deny all; }    # dotfiles

    location = /healthz {
        access_log off;
        default_type text/plain;
        return 200 "ok\n";
    }

    location @api_down {
        default_type application/json;
        add_header Retry-After 5 always;
        include snippets/security-headers.conf;
        return 503 '{"error":"service_unavailable"}';
    }
}
```

Apply it:
```bash
sudo nginx -t && sudo systemctl reload nginx
```

### Why it is built this way
- **`/assets/` returns 404 for missing files** instead of falling back to `index.html`. Otherwise a removed JavaScript chunk returns HTML with status 200, and the browser reports a confusing syntax or MIME error.
- **`index.html` is `no-cache`**, assets are `immutable` for a year. A deploy changes `index.html`, which references new hashed filenames, so users get updates immediately while still caching assets.
- **`/` with `try_files ... /index.html`** makes deep links like `/dashboard/settings` work on refresh. The fallback is an internal redirect, so it lands in `location = /index.html` and gets the `no-cache` header.
- **Security headers are re-included** in `/assets/`, `= /index.html`, and `@api_down` because each of those blocks defines its own `add_header`, which discards the inherited ones ([config-structure.md](../01-fundamentals/config-structure.md)).
- **`/api/auth/` is a separate, longer prefix**, so it wins over `/api/`: slow brute-force attempts without limiting normal API use.
- **No trailing slash on `proxy_pass`**, so the backend receives the full `/api/...` path. If your backend expects paths without `/api`, use `proxy_pass http://api_backend/;` instead ([reverse-proxy.md](../02-core-features/reverse-proxy.md)).
- **Order of regex vs prefix:** the dotfile rule is a regex location. Regexes are checked after prefix matches but win over plain prefixes, so it blocks `/.git/...` anywhere.

## Step 5: Verify

| Check | Command | Expected |
|---|---|---|
| HTTP redirects | `curl -sI http://app.example.com/` | `301` and `Location: https://app.example.com/` |
| App shell | `curl -sI https://app.example.com/` | `200`, `Cache-Control: no-cache` |
| Deep link | `curl -sI https://app.example.com/dashboard/settings` | `200` (served from `index.html`) |
| Asset | `curl -sI https://app.example.com/assets/<real-file>.js` | `200`, long `Cache-Control`, `Content-Encoding: gzip` if you send `-H "Accept-Encoding: gzip"` |
| Missing asset | `curl -sI https://app.example.com/assets/nope.js` | `404` (not 200 with HTML) |
| API proxied | `curl -si https://app.example.com/api/health` | Response from your backend |
| Headers on 404 | `curl -sI https://app.example.com/assets/nope.js \| grep -i x-content-type` | Header present (thanks to `always`) |
| Dotfiles | `curl -sI https://app.example.com/.git/config` | `403` |
| Rate limit | `for i in $(seq 1 30); do curl -s -o /dev/null -w "%{http_code}\n" https://app.example.com/api/auth/login; done \| sort \| uniq -c` | Some `429` responses |
| Backend down | Stop the backend, then `curl -si https://app.example.com/api/x` | `503` with the JSON body |
| TLS | `echo \| openssl s_client -connect app.example.com:443 -servername app.example.com 2>/dev/null \| openssl x509 -noout -dates` | Valid dates |

Then raise HSTS `max-age` from `300` to `31536000` after confirming everything works over HTTPS.

## Optional: WebSocket Endpoint
If the backend also serves WebSockets (for example `/socket/`):
```nginx
# in the http context (e.g. at the top of app-https.conf)
map $http_upgrade $connection_upgrade {
    default upgrade;
    ''      close;
}

# inside the server block
location /socket/ {
    include snippets/proxy-params.conf;
    proxy_set_header Upgrade    $http_upgrade;
    proxy_set_header Connection $connection_upgrade;
    proxy_read_timeout 1h;
    proxy_pass http://api_backend;
}
```

## Common Problems in This Setup

| Symptom | Likely cause | Fix |
|---|---|---|
| Refreshing `/some/route` gives 404 | Missing SPA fallback, or `/` location overridden | `try_files $uri $uri/ /index.html;` in `location /` |
| Blank page after deploy, console shows JS "Unexpected token <" | Old hashed assets deleted while users still reference them, and a fallback returned HTML | Keep several releases, keep `/assets/` returning 404 for missing files |
| Users keep seeing the old app | `index.html` cached | Confirm `Cache-Control: no-cache` on `/index.html`, and that no CDN overrides it |
| API 502 | Backend down or wrong port | `ss -ltnp \| grep 3000`, read `/var/log/nginx/error.log` ([troubleshooting.md](../06-production/troubleshooting.md)) |
| API redirects to `http://` or wrong host | Backend ignores forwarded headers | Configure the backend's trusted-proxy setting |
| Uploads fail with 413 | `client_max_body_size` too small for `/api/` | Raise it for the upload route only |
| Security headers missing on some responses | Location with its own `add_header` | Re-include the snippet there |
| 403 on all files | Worker user cannot read the release directory | `namei -l /var/www/app/current/index.html`, fix permissions |

## Extensions to Try
1. **Cache public API responses:** add `proxy_cache` for `GET /api/public/` using [caching.md](../03-traffic-management/caching.md), and expose `$upstream_cache_status` in a header.
2. **JSON access logs with upstream timing** ([logging.md](../02-core-features/logging.md)), then find your slowest endpoint.
3. **Pre-compressed assets:** build `.gz` files in CI and enable `gzip_static on;`.
4. **Two API instances:** add a second `server` line to `api_backend` and observe failover when you stop one ([load-balancing.md](../03-traffic-management/load-balancing.md)).
5. **Run it in Docker Compose** ([docker-kubernetes.md](../06-production/docker-kubernetes.md)).
6. **Scale up to multiple apps:** continue with [secure-gateway-stack.md](secure-gateway-stack.md).

## Related / Next
- [secure-gateway-stack.md](secure-gateway-stack.md): several apps behind one hardened nginx
- [troubleshooting.md](../06-production/troubleshooting.md): diagnosing problems from this project
