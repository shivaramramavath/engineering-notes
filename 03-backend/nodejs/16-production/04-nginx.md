# Nginx

A reverse proxy in front of your Node.js app: TLS termination, compression, static files, rate limiting, WebSocket proxying, and load balancing.

## What a reverse proxy does (and why Node shouldn't face the internet alone)

```
                        ┌──────────────────────────────────────────────┐
 Internet ──HTTPS:443──▶│                    Nginx                     │
                        │  TLS termination · compression · static      │
                        │  files · rate limits · request buffering     │
                        │  load balancing · access control             │
                        └───────────┬───────────────┬──────────────────┘
                              HTTP (private network only)
                                    ▼               ▼
                              ┌──────────┐    ┌──────────┐
                              │ Node app │    │ Node app │   ... more instances
                              │  :3000   │    │  :3000   │
                              └──────────┘    └──────────┘
```

A **reverse proxy** accepts connections from clients on behalf of your servers. Node *can* terminate TLS and serve static files itself, but a dedicated proxy does these jobs better, and lets Node concentrate on application logic:

| Job | Why Nginx is better at it |
|---|---|
| **TLS termination** | Optimized crypto, session caching, HTTP/2, easy certificate automation |
| **Slow clients** | Nginx buffers slow uploads/downloads so a client on a bad connection doesn't hold a Node request open (and an event-loop-bound process busy) |
| **Static files** | Served directly from disk with `sendfile`, caching headers, and no JavaScript involved |
| **Compression** | gzip/brotli offloaded from your single-threaded Node process |
| **Load balancing** | Spread requests across instances, with health-aware failover |
| **Protection** | Connection limits, request-rate limits, size limits, blocking paths and bad bots before they reach Node |
| **One entry point** | Route `/api` to the API, `/` to the front-end, `/ws` to the realtime service, on one domain |
| **Zero-downtime config reloads** | `nginx -s reload` applies changes without dropping connections |

### Do you need Nginx?

| Setup | Need Nginx? |
|---|---|
| **AWS ECS/EKS behind an ALB** | Usually **no**: the ALB does TLS, health checks, and load balancing (`06-aws.md`). Add Nginx as a sidecar only for specific needs (static assets, advanced caching, custom rules) |
| **Cloud Run / App Runner / Heroku-style PaaS** | No: the platform provides it |
| **A single VM or bare-metal server** | **Yes**: it's the standard way to get TLS, multiple apps per host, and static serving |
| **Kubernetes** | An **Ingress controller** (often Nginx-based) plays this role |
| **Need a CDN/WAF** | Put **CloudFront/Cloudflare** in front too |

The concepts here (forwarded headers, timeouts, keep-alive, WebSockets, rate limiting) apply to *every* proxy or load balancer, so they're worth knowing even if a managed service runs them for you.

---

## Installing and the file layout

```bash
# Debian/Ubuntu
sudo apt install nginx
# or the official container
docker run -d -p 80:80 -v ./nginx.conf:/etc/nginx/nginx.conf:ro nginx:1.27
```

```
/etc/nginx/
├── nginx.conf                 ← main config: global settings, includes the rest
├── conf.d/*.conf              ← extra config files (common in containers)
├── sites-available/           ← (Debian/Ubuntu) one file per site
├── sites-enabled/             ← symlinks to the sites that are active
└── snippets/                  ← reusable fragments
```

```bash
sudo nginx -t                  # ALWAYS test the config before applying it
sudo nginx -s reload           # apply changes gracefully (no dropped connections)
sudo systemctl status nginx
sudo tail -f /var/log/nginx/error.log
```

Config is built from **directives** inside **contexts** (blocks): `http { server { location { ... } } }`.

| Context | Purpose |
|---|---|
| `http` | Settings for all HTTP traffic |
| `upstream` | A named group of backend servers |
| `server` | A virtual host: one domain/port |
| `location` | Rules for a URL path inside a server |

---

## A basic reverse proxy

```nginx
# /etc/nginx/conf.d/api.conf
upstream orders_api {
    server 127.0.0.1:3000;
    keepalive 32;                                   # reuse connections to Node (needs the proxy_http_version/Connection lines below)
}

server {
    listen 80;
    server_name api.example.com;

    location / {
        proxy_pass http://orders_api;

        proxy_http_version 1.1;                     # required for keep-alive to the upstream and for WebSockets
        proxy_set_header Connection "";             # clear "Connection: close" so upstream keep-alive works

        proxy_set_header Host              $host;                    # the original Host header
        proxy_set_header X-Real-IP         $remote_addr;             # the client's IP
        proxy_set_header X-Forwarded-For   $proxy_add_x_forwarded_for;
        proxy_set_header X-Forwarded-Proto $scheme;                  # http or https: Express needs this for secure cookies
        proxy_set_header X-Request-Id      $request_id;              # a unique ID per request (14-logging-observability/02)
    }
}
```

### Tell Express it's behind a proxy

Without this, Express sees every request as coming from Nginx (`127.0.0.1`) over plain HTTP:

- `req.ip` is the proxy's address → **rate limits and logs are keyed to the wrong IP**
- `req.protocol` is `http` → `secure` cookies aren't set
- `req.secure` is false → HTTPS redirects loop

```js
app.set("trust proxy", 1);          // trust exactly ONE proxy hop (Nginx), so req.ip/protocol come from X-Forwarded-*
```

**Set the number of hops precisely.** `true` trusts *any* `X-Forwarded-For` value, so a client can spoof its IP and bypass rate limits. With a CDN in front of Nginx, you'd have two hops (`2`). See `08-authentication-security/06-rate-limiting.md` and `01-environment-management.md` (`TRUST_PROXY`).

---

## TLS and HTTPS

**Terminate TLS at Nginx**, and keep the Node app on plain HTTP over a private network.

### Free certificates with Let's Encrypt

```bash
sudo apt install certbot python3-certbot-nginx
sudo certbot --nginx -d api.example.com         # obtains a certificate and edits the Nginx config
sudo certbot renew --dry-run                    # certificates last 90 days; certbot installs an auto-renew timer
```

(Behind a cloud load balancer, **AWS Certificate Manager (ACM)** provides free, auto-renewing certificates instead: `06-aws.md`.)

### A production HTTPS server block

```nginx
# redirect all HTTP to HTTPS
server {
    listen 80;
    server_name api.example.com;
    location /.well-known/acme-challenge/ { root /var/www/certbot; }     # for certificate issuance/renewal
    location / { return 301 https://$host$request_uri; }
}

server {
    listen 443 ssl;
    http2 on;                                         # HTTP/2 (Nginx 1.25.1+; older versions use `listen 443 ssl http2;`)
    server_name api.example.com;

    ssl_certificate     /etc/letsencrypt/live/api.example.com/fullchain.pem;
    ssl_certificate_key /etc/letsencrypt/live/api.example.com/privkey.pem;

    ssl_protocols TLSv1.2 TLSv1.3;                    # no SSLv3, TLS 1.0 or 1.1
    ssl_session_cache shared:SSL:10m;                 # session reuse: fewer expensive handshakes
    ssl_session_timeout 1d;
    ssl_session_tickets off;

    # HSTS: tell browsers "HTTPS only" (08-authentication-security/07-helmet.md): start with a short max-age while testing!
    add_header Strict-Transport-Security "max-age=31536000; includeSubDomains" always;

    server_tokens off;                                # don't advertise the Nginx version

    location / {
        proxy_pass http://orders_api;
        # ... the proxy_set_header lines from above ...
    }
}
```

Use **Mozilla's SSL Configuration Generator** for current recommended cipher suites rather than copying old blog posts, and test your result with **SSL Labs** (ssllabs.com/ssltest). Cipher recommendations change over time.

HSTS and security headers (`07-helmet.md`) can be set in Nginx *or* in Node (Helmet). Pick one place to avoid duplicate headers; if Nginx serves static files directly, those responses bypass Helmet, so headers set at Nginx cover everything.

---

## Compression

Compress text responses (JSON, HTML, CSS, JS) to cut bandwidth and latency, and **do it at Nginx instead of in Node** (compression is CPU work that would otherwise block your event loop: `15-performance/01-event-loop-performance.md`).

```nginx
http {
    gzip on;
    gzip_comp_level 5;                                # 1–9; 4–6 balances CPU and size
    gzip_min_length 1024;                             # don't bother with tiny responses
    gzip_vary on;                                     # add "Vary: Accept-Encoding" for caches
    gzip_proxied any;                                 # also compress responses from the upstream
    gzip_types
        application/json application/javascript text/css text/plain
        text/xml application/xml image/svg+xml;
    # text/html is always compressed once gzip is on
}
```

**Brotli** compresses better than gzip but needs the `ngx_brotli` module (not in the default build). If you use compression in Express instead (`compression` middleware), disable one of them so responses aren't compressed twice. **Don't compress already-compressed formats** (JPEG, PNG, MP4, ZIP). See `05-http-web/05-caching-and-compression.md`.

> **Security note:** compressing responses that mix secrets with attacker-influenced content can enable BREACH-style attacks. For typical JSON APIs this is a minor concern; CSRF tokens and per-response randomization mitigate it for HTML forms.

---

## Serving static files

If you have a front-end build, uploads you've stored on disk, or assets, let Nginx serve them without touching Node:

```nginx
server {
    # ...
    root /var/www/app;                                 # the built front-end (index.html, assets/)

    # versioned/hashed assets: cache for a year
    location /assets/ {
        expires 1y;
        add_header Cache-Control "public, max-age=31536000, immutable";
        try_files $uri =404;
    }

    # the single-page app: any unknown path returns index.html (client-side routing)
    location / {
        try_files $uri /index.html;
        add_header Cache-Control "no-cache";           # always revalidate the HTML shell
    }

    # API requests go to Node
    location /api/ {
        proxy_pass http://orders_api;
        # ... proxy headers ...
    }
}
```

Hashed filenames (`app.3f9c2ab.js`) make **immutable** caching safe, since a new deploy produces new names. The HTML shell must stay revalidating so users pick up the new asset names. For global audiences, put a **CDN** (CloudFront, Cloudflare) in front of the static content.

User-uploaded files normally belong in **object storage (S3)** served via a CDN, not on the app server's disk (`06-express/07-file-upload.md`), because containers and instances are disposable.

---

## Rate limiting and connection limits

Nginx can reject abusive traffic **before it reaches Node**, which is cheap and far more efficient than doing it in the app. Use it as a coarse outer defense alongside the application-level, per-user limits (`08-authentication-security/06-rate-limiting.md`).

```nginx
http {
    # define limit zones (shared memory keyed by client IP)
    limit_req_zone  $binary_remote_addr zone=api_limit:10m   rate=20r/s;     # 20 requests/second per IP
    limit_req_zone  $binary_remote_addr zone=login_limit:10m rate=5r/m;      # 5 requests/minute per IP
    limit_conn_zone $binary_remote_addr zone=conn_limit:10m;                 # concurrent connections per IP

    limit_req_status  429;                                                    # default is 503; 429 is the right status
    limit_conn_status 429;

    server {
        # ...
        limit_conn conn_limit 30;                                             # max 30 concurrent connections per IP

        location /api/ {
            limit_req zone=api_limit burst=40 nodelay;                        # allow short bursts of 40 over the rate
            proxy_pass http://orders_api;
        }

        location = /api/v1/auth/login {
            limit_req zone=login_limit burst=5 nodelay;                       # stricter on credential endpoints
            proxy_pass http://orders_api;
        }
    }
}
```

- `rate` is the sustained rate; `burst` is how many extra requests may queue/pass beyond it; `nodelay` serves the burst immediately instead of spacing it out.
- **Behind a CDN or load balancer, `$remote_addr` is the *proxy's* IP**, so everyone shares one bucket. Use the real client IP via the `realip` module:

```nginx
set_real_ip_from 10.0.0.0/8;                  # trust forwarded headers ONLY from your own load balancer's network
real_ip_header   X-Forwarded-For;
real_ip_recursive on;
```

- Remember that per-IP limits punish users behind shared NATs (offices, mobile carriers). Keep the generous ones in Nginx and the precise per-user ones in the app.

### Limit sizes and timeouts

```nginx
client_max_body_size 5m;               # reject big uploads at the edge (match Express's limits: 09-api-development/04-validation.md)
client_body_timeout   15s;             # slow-loris defense: drop clients that trickle the body
client_header_timeout 15s;
send_timeout          30s;
keepalive_timeout     65s;

proxy_connect_timeout 5s;              # how long to wait to connect to Node
proxy_send_timeout    60s;
proxy_read_timeout    60s;             # how long to wait for Node's response before returning 504
```

For a few endpoints (large uploads, exports), override per `location` rather than raising limits globally.

---

## WebSockets (and Server-Sent Events)

WebSockets start as an HTTP **Upgrade** request; Nginx must forward the upgrade headers or the connection silently falls back or fails (`12-realtime/03-scaling-with-redis-adapter.md`).

```nginx
# map the Upgrade header to the right Connection value
map $http_upgrade $connection_upgrade {
    default upgrade;
    ''      close;
}

server {
    # ...
    location /socket.io/ {
        proxy_pass http://orders_api;

        proxy_http_version 1.1;                          # REQUIRED
        proxy_set_header Upgrade    $http_upgrade;
        proxy_set_header Connection $connection_upgrade;

        proxy_set_header Host              $host;
        proxy_set_header X-Forwarded-For   $proxy_add_x_forwarded_for;
        proxy_set_header X-Forwarded-Proto $scheme;

        proxy_read_timeout 120s;                         # MUST exceed the heartbeat interval or idle sockets get killed
        proxy_send_timeout 120s;
        proxy_buffering off;
    }
}
```

The default `proxy_read_timeout` of 60 s closes quiet WebSocket connections, a classic "disconnects every minute" bug. For **Server-Sent Events**, also add `proxy_buffering off;` (and have your app send periodic comment lines as keep-alives).

---

## Load balancing across instances

```nginx
upstream orders_api {
    least_conn;                                       # send each request to the server with the fewest active connections

    server 10.0.1.11:3000 max_fails=3 fail_timeout=30s;
    server 10.0.1.12:3000 max_fails=3 fail_timeout=30s;
    server 10.0.1.13:3000 backup;                     # only used when the others are down

    keepalive 64;
}
```

| Method | Behavior | Use |
|---|---|---|
| **round-robin** (default) | Rotate through servers | Equal servers, similar request costs |
| **`least_conn`** | Fewest active connections | Uneven request durations (a good general default) |
| **`ip_hash`** | Same client IP → same server | Crude stickiness (WebSocket polling, in-memory sessions) |
| **`hash $cookie_x consistent`** | Hash on any key | Cookie- or key-based stickiness |
| **Weights** (`server ... weight=3`) | Proportional share | Mixed-size servers |

**Health checking:** open-source Nginx performs **passive** checks only: after `max_fails` failures within `fail_timeout`, a server is skipped for `fail_timeout`. **Active** health checks (probing `/readyz`) are an Nginx Plus feature, which is one reason cloud load balancers (ALB) or HAProxy are often preferred for the balancing role (`02-graceful-shutdown-and-health-checks.md`). Use `proxy_next_upstream` to retry a failed request on another server, but **only for idempotent requests**; retrying a `POST` that may have succeeded can duplicate work (`09-api-development/06-idempotency.md`):

```nginx
proxy_next_upstream error timeout http_502 http_503 http_504;
proxy_next_upstream_tries 2;
proxy_next_upstream_timeout 5s;
# By default Nginx does not retry non-idempotent methods (POST, PATCH): leave it that way unless you use idempotency keys.
```

### Keep-alive alignment (the 502 gotcha)

When Nginx reuses upstream connections (`keepalive` in the `upstream`), **Node's `server.keepAliveTimeout` must be longer than Nginx's upstream idle timeout**, otherwise Node closes a connection just as Nginx tries to reuse it, producing sporadic `502 Bad Gateway`s. Nginx's `keepalive_timeout` inside the `upstream` block (1.15.3+, default 60 s) is the relevant value:

```nginx
upstream orders_api {
    server 127.0.0.1:3000;
    keepalive 32;
    keepalive_timeout 55s;           # Nginx closes idle upstream connections first
}
```

```js
server.keepAliveTimeout = 65_000;    // Node outlasts Nginx (and any ALB in front)
server.headersTimeout = 66_000;
```

Details in `02-graceful-shutdown-and-health-checks.md`.

---

## Caching

Nginx can cache upstream responses, which is useful for expensive, public, rarely-changing GET endpoints (catalogs, public configuration), and a lifesaver in traffic spikes.

```nginx
http {
    proxy_cache_path /var/cache/nginx levels=1:2 keys_zone=api_cache:10m max_size=1g inactive=10m use_temp_path=off;

    server {
        location /api/v1/catalog/ {
            proxy_cache api_cache;
            proxy_cache_valid 200 1m;                       # cache successful responses for 1 minute
            proxy_cache_key "$scheme$request_method$host$request_uri";
            proxy_cache_lock on;                            # collapse concurrent misses into ONE upstream request (prevents stampedes)
            proxy_cache_use_stale error timeout updating http_500 http_502 http_503;   # serve stale content if the app is struggling
            add_header X-Cache-Status $upstream_cache_status;   # HIT / MISS / STALE: great for debugging
            proxy_pass http://orders_api;
        }
    }
}
```

**Never cache personalized or authenticated responses** with a shared key: User A would receive User B's data. Only cache routes that are identical for everyone, or include the user in the cache key deliberately. By default Nginx won't cache responses that carry `Set-Cookie` or `Cache-Control: private`/`no-store`, and respecting those headers from your app is the safest policy (`05-http-web/05-caching-and-compression.md`).

---

## Security hardening

```nginx
server_tokens off;                                    # hide the version

# block common probing of sensitive paths
location ~ /\.(?!well-known) { deny all; }            # .git, .env, .htaccess, ...
location ~* \.(?:bak|sql|swp|old)$ { deny all; }

# only allow expected HTTP methods
if ($request_method !~ ^(GET|HEAD|POST|PUT|PATCH|DELETE|OPTIONS)$) { return 405; }

# keep operational endpoints off the public internet (14-logging-observability/03)
location = /metrics { allow 10.0.0.0/8; deny all; proxy_pass http://orders_api; }
location = /readyz  { allow 10.0.0.0/8; deny all; proxy_pass http://orders_api; }     # or return a bare 200 publicly and keep detail internal

# security headers (or set them with Helmet in Node: just not both; 08-authentication-security/07-helmet.md)
add_header X-Content-Type-Options "nosniff" always;
add_header X-Frame-Options "DENY" always;
add_header Referrer-Policy "strict-origin-when-cross-origin" always;
```

Notes:

- `add_header` directives **are not inherited** into a `location` that defines its own `add_header`. Repeat the headers or use an `include` snippet (a common gotcha).
- `if` in `location` contexts is fragile in Nginx ("if is evil"). Prefer `map`, `limit_except`, or separate `location` blocks where possible.
- Don't expose the app's raw port to the internet. Bind Node to a private interface, or use security groups so **only the proxy** can reach port 3000.
- Consider a **WAF** (ModSecurity/Coraza, AWS WAF, Cloudflare) for managed rule sets against SQL injection, XSS, and bad bots (`08-authentication-security/05-common-vulnerabilities.md`).

---

## Logging that connects to your observability

```nginx
log_format json_combined escape=json
  '{'
    '"time":"$time_iso8601",'
    '"remote_addr":"$remote_addr",'
    '"request_id":"$request_id",'
    '"method":"$request_method",'
    '"uri":"$request_uri",'
    '"status":$status,'
    '"bytes_sent":$body_bytes_sent,'
    '"request_time":$request_time,'
    '"upstream_time":"$upstream_response_time",'
    '"upstream_addr":"$upstream_addr",'
    '"cache":"$upstream_cache_status",'
    '"user_agent":"$http_user_agent"'
  '}';

access_log /var/log/nginx/access.log json_combined;
error_log  /var/log/nginx/error.log warn;

# don't log health checks
map $request_uri $loggable { /healthz 0; /readyz 0; default 1; }
access_log /var/log/nginx/access.log json_combined if=$loggable;
```

In containers, send logs to stdout/stderr (the official image symlinks `/var/log/nginx/access.log` → `/dev/stdout`) so the platform collects them (`14-logging-observability/01-pino-and-structured-logging.md`). Forwarding `X-Request-Id: $request_id` (shown earlier) and logging it means an Nginx access line and your app's log lines share an ID: set `requestContextMiddleware` to honor the incoming `X-Request-Id` (`14-logging-observability/02-correlation-id.md`). The `$upstream_response_time` vs `$request_time` difference separates slow app responses from slow clients. If `upstream_time` is small but `request_time` is large, the *client* is slow.

---

## Troubleshooting

| Symptom | Likely cause |
|---|---|
| **`502 Bad Gateway`** | Node isn't running/listening, wrong `proxy_pass` port, Node crashed, or the **keep-alive timeout mismatch** above |
| **`504 Gateway Timeout`** | Node took longer than `proxy_read_timeout`: slow handler, blocked event loop, or a dependency timeout |
| **`413 Request Entity Too Large`** | `client_max_body_size` too small for the upload |
| **Everyone has the same IP in logs / rate limits fire for all** | Missing `trust proxy` in Express, or missing `realip` config in Nginx behind a CDN/LB |
| **Redirect loops after enabling HTTPS** | Node doesn't know the request was HTTPS (`X-Forwarded-Proto` + `trust proxy`) and redirects again |
| **Secure cookies not being set** | Same cause: Express thinks the connection is plain HTTP |
| **WebSockets fall back to polling or disconnect every 60 s** | Missing `Upgrade`/`Connection` headers, `proxy_http_version 1.1`, or a too-short `proxy_read_timeout` |
| **CORS errors only in production** | Nginx stripping/duplicating headers, or `Origin`/`Host` not forwarded; check headers with `curl -I` |
| **Duplicate headers** (two `Strict-Transport-Security`) | Set by both Nginx and Helmet: choose one |
| **Config change has no effect** | Forgot `nginx -s reload`, edited the wrong file, or a more specific `location` matches first |
| **`nginx: [emerg] ...` on startup** | Syntax error: run `nginx -t` |

Debug tools:

```bash
nginx -T                                   # dump the full effective configuration
curl -sv https://api.example.com/healthz   # TLS handshake, status, headers
curl -I -H "Origin: https://app.example.com" https://api.example.com/api/v1/posts
```

---

## Nginx alternatives

| Tool | Notes |
|---|---|
| **Caddy** | Automatic HTTPS with a tiny config (`api.example.com { reverse_proxy localhost:3000 }`). Great for small deployments and simplicity |
| **Traefik** | Docker/Kubernetes-native: discovers services from labels, with automatic certificates |
| **HAProxy** | Excellent load balancer with rich active health checks and metrics |
| **AWS ALB / GCP LB / Azure App Gateway** | Managed: TLS, health checks, autoscaling, WAF integration, no servers to patch |
| **Kubernetes Ingress** (ingress-nginx, Traefik, AWS Load Balancer Controller) | The standard entry point in a cluster |
| **Envoy / service meshes** | Advanced traffic management between services |
| **CDN (CloudFront, Cloudflare, Fastly)** | Edge caching, DDoS protection, global TLS: often placed in front of any of the above |

---

## A complete example for a single server

```nginx
# /etc/nginx/nginx.conf (condensed)
worker_processes auto;
events { worker_connections 4096; }

http {
    include       mime.types;
    sendfile      on;
    server_tokens off;

    gzip on; gzip_comp_level 5; gzip_min_length 1024; gzip_vary on;
    gzip_types application/json application/javascript text/css text/plain image/svg+xml;

    map $http_upgrade $connection_upgrade { default upgrade; '' close; }

    limit_req_zone $binary_remote_addr zone=api_limit:10m rate=20r/s;
    limit_req_status 429;

    upstream orders_api {
        least_conn;
        server 127.0.0.1:3000 max_fails=3 fail_timeout=30s;
        server 127.0.0.1:3001 max_fails=3 fail_timeout=30s;
        keepalive 32;
        keepalive_timeout 55s;
    }

    server {
        listen 80;
        server_name api.example.com;
        return 301 https://$host$request_uri;
    }

    server {
        listen 443 ssl;
        http2 on;
        server_name api.example.com;

        ssl_certificate     /etc/letsencrypt/live/api.example.com/fullchain.pem;
        ssl_certificate_key /etc/letsencrypt/live/api.example.com/privkey.pem;
        ssl_protocols TLSv1.2 TLSv1.3;

        add_header Strict-Transport-Security "max-age=31536000; includeSubDomains" always;
        client_max_body_size 5m;

        location / {
            limit_req zone=api_limit burst=40 nodelay;
            proxy_pass http://orders_api;
            proxy_http_version 1.1;
            proxy_set_header Connection "";
            proxy_set_header Host $host;
            proxy_set_header X-Real-IP $remote_addr;
            proxy_set_header X-Forwarded-For $proxy_add_x_forwarded_for;
            proxy_set_header X-Forwarded-Proto $scheme;
            proxy_set_header X-Request-Id $request_id;
            proxy_read_timeout 60s;
        }

        location /socket.io/ {
            proxy_pass http://orders_api;
            proxy_http_version 1.1;
            proxy_set_header Upgrade $http_upgrade;
            proxy_set_header Connection $connection_upgrade;
            proxy_set_header Host $host;
            proxy_set_header X-Forwarded-For $proxy_add_x_forwarded_for;
            proxy_set_header X-Forwarded-Proto $scheme;
            proxy_read_timeout 120s;
            proxy_buffering off;
        }

        location = /metrics { allow 127.0.0.1; deny all; proxy_pass http://orders_api; }
    }
}
```

A sample note: the `/socket.io/` location doesn't use `keepalive` upstream connection reuse (the `Connection` header is overridden for upgrades), and the `upstream` is shared safely between both locations because `keepalive` only applies to requests that send `Connection: ""`.

---

## Common mistakes

```nginx
# ❌ no `trust proxy` in Express → wrong client IPs, rate limits keyed to the proxy, secure cookies broken
# ❌ `trust proxy` = true behind the public internet → clients can spoof X-Forwarded-For
# ❌ missing proxy_set_header Host / X-Forwarded-Proto / X-Forwarded-For
# ❌ WebSocket location without proxy_http_version 1.1 + Upgrade/Connection headers
# ❌ default proxy_read_timeout (60 s) for WebSockets/SSE/long requests
# ❌ Node keepAliveTimeout shorter than the proxy's upstream idle timeout → sporadic 502s
# ❌ forgetting `nginx -t` before reload; editing config without a rollback plan
# ❌ caching authenticated/personalized responses with a shared cache key
# ❌ rate limiting by $remote_addr behind a load balancer without the realip module (everyone shares one bucket)
# ❌ HSTS with a long max-age (or preload) before HTTPS is solid across all subdomains
# ❌ security headers set in both Nginx and Helmet (duplicates); or add_header silently dropped by inheritance rules
# ❌ exposing /metrics, /readyz details, or the Node port publicly
# ❌ serving user uploads from the app server's disk
# ❌ compressing in both Express and Nginx, or compressing already-compressed content
# ❌ retrying non-idempotent requests on another upstream without idempotency keys
# ❌ certificates that expire because renewal was never tested (`certbot renew --dry-run`)
# ❌ running Nginx when a managed load balancer would do the same job with less to operate
```

## Checklist

- [ ] TLS terminated at the proxy (Let's Encrypt/certbot or ACM); HTTP → HTTPS redirect; TLS 1.2+; renewal tested
- [ ] `proxy_http_version 1.1` + `Host`, `X-Forwarded-For`, `X-Forwarded-Proto`, `X-Request-Id` forwarded
- [ ] Express `trust proxy` set to the **exact** number of hops; `realip` configured if a CDN/LB sits in front of Nginx
- [ ] Node `keepAliveTimeout` > proxy upstream idle timeout; `headersTimeout` > `keepAliveTimeout`
- [ ] Timeouts, `client_max_body_size`, and slow-client protections set deliberately (aligned with Express limits)
- [ ] gzip (or brotli) enabled for text types at Nginx, not duplicated in Express
- [ ] Static assets served by Nginx/CDN with immutable caching for hashed files; uploads in object storage
- [ ] Coarse rate limits and connection limits in Nginx; precise per-user limits in the app
- [ ] WebSocket locations with Upgrade headers and long read timeouts; SSE with buffering off
- [ ] Load balancing method chosen; passive health checks; retries only for idempotent requests
- [ ] `server_tokens off`; sensitive paths blocked; `/metrics` and detailed readiness kept internal; app port not publicly reachable
- [ ] JSON access logs including the request ID, health checks excluded, shipped to your log platform
- [ ] `nginx -t` in CI/deploy before every reload

## Next

**`05-ci-cd.md`** automates everything so far: every push is linted, tested, scanned, built into an image, and deployed through staging to production, with a way back when something goes wrong.
