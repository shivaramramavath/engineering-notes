<!--
================================================================================
Metadata:
  Title: Reference Project 04 - NGINX Reverse Proxy with TLS and Rate Limiting
  Track: Docker / DevOps
  Difficulty: Intermediate
  Estimated Time: 55 minutes
  Prerequisites: 04-volumes-and-networking/06-ports-publishing-and-firewalls.md, 05-compose/02-compose-multi-service.md
  Key Terminology: NGINX, TLS/SSL Termination, mkcert, Rate Limiting, HTTP/2, Security Headers, Upstream Proxy
================================================================================
-->

# Reference Project 04: NGINX Reverse Proxy with TLS and Rate Limiting

[<- 03-spring-boot-with-database.md](./03-spring-boot-with-database.md) | [Back to Projects Overview](./README.md) | [Next Project: 05-multi-stage-minimal-go-image.md ->](./05-multi-stage-minimal-go-image.md)

---

## 1. Overview & Architecture

Exposing application runtimes (such as Node.js, Python, or Go) directly to the public internet presents security and performance risks. Production deployments utilize **NGINX** as an edge reverse proxy to:
1. Terminate TLS/SSL centrally, offloading cryptographic overhead from application containers.
2. Enforce enterprise HTTP security headers (HSTS, Content-Security-Policy, X-Frame-Options).
3. Protect against Distributed Denial of Service (DDoS) and brute-force attacks via `limit_req` rate limiting.
4. Route traffic dynamically across internal microservices (`/api` vs `/`).

### Architectural Diagram

```
+-----------------------------------------------------------------------------------+
|                                  HOST MACHINE                                     |
|                                                                                   |
|    Browser / HTTPS Client (https://localhost:8443)                                |
|           |                                                                       |
|           | Port 8443 (TLS Encrypted)                                             |
|           v                                                                       |
|  +-----------------------------------------------------------------------------+  |
|  |                          DOCKER NETWORK: edge-net                           |  |
|  |                                                                             |  |
|  |   +---------------------------------------------------------------------+   |  |
|  |   |                       SERVICE: reverse-proxy                        |   |  |
|  |   |  Image: nginx:1.27-alpine                                           |   |  |
|  |   |  Terminates TLS (certs mounted read-only)                           |   |  |
|  |   |  Enforces HTTP/2, Rate Limiting (10 req/s), Security Headers        |   |  |
|  |   +-----------------+---------------------------------+-----------------+   |  |
|  |                     |                                 |                     |  |
|  |         Proxy Pass: /api/*                Proxy Pass: /* (Static/SPA)       |  |
|  |                     v                                 v                     |  |
|  |   +--------------------------+               +--------------------------+   |  |
|  |   |   SERVICE: api-backend   |               |   SERVICE: web-frontend  |   |  |
|  |   |   Node.js / Express API  |               |   Static HTML / SPA      |   |  |
|  |   |   Port 3000 (Internal)   |               |   Port 80 (Internal)     |   |  |
|  |   +--------------------------+               +--------------------------+   |  |
|  +-----------------------------------------------------------------------------+  |
+-----------------------------------------------------------------------------------+
```

---

## 2. Directory Structure & File Listing

```
nginx-tls-reverse-proxy/
├── certs/
│   ├── generate-certs.sh
│   ├── server.crt
│   └── server.key
├── compose.yaml
├── nginx/
│   ├── conf.d/
│   │   └── default.conf
│   └── nginx.conf
└── web/
    └── index.html
```

---

## 3. Complete Implementation

### File: `certs/generate-certs.sh`

```bash
#!/usr/bin/env sh
set -e

mkdir -p certs
# Generate a self-signed RSA 4096-bit certificate valid for localhost
openssl req -x509 -nodes -days 365 -newkey rsa:4096 \
  -keyout certs/server.key \
  -out certs/server.crt \
  -subj "/C=US/ST=DevOps/L=Local/O=Engineering/OU=Architecture/CN=localhost" \
  -addext "subjectAltName=DNS:localhost,IP:127.0.0.1"

chmod 600 certs/server.key
chmod 644 certs/server.crt
echo "TLS Certificates generated successfully in ./certs/"
```

### File: `nginx/nginx.conf`

```nginx
user  nginx;
worker_processes  auto;

error_log  /var/log/nginx/error.log warn;
pid        /var/run/nginx.pid;

events {
    worker_connections  1024;
    multi_accept        on;
}

http {
    include       /etc/nginx/mime.types;
    default_type  application/octet-stream;

    log_format  main  '$remote_addr - $remote_user [$time_local] "$request" '
                      '$status $body_bytes_sent "$http_referer" '
                      '"$http_user_agent" "$http_x_forwarded_for"';

    access_log  /var/log/nginx/access.log  main;

    sendfile        on;
    tcp_nopush      on;
    tcp_nodelay     on;
    keepalive_timeout  65;
    types_hash_max_size 2048;
    server_tokens   off;

    # Gzip Compression
    gzip  on;
    gzip_vary on;
    gzip_proxied any;
    gzip_comp_level 6;
    gzip_types text/plain text/css application/json application/javascript text/xml;

    # Rate Limiting: 10 requests/second per IP with 10MB memory zone
    limit_req_zone $binary_remote_addr zone=api_limit:10m rate=10r/s;
    limit_req_status 429;

    include /etc/nginx/conf.d/*.conf;
}
```

### File: `nginx/conf.d/default.conf`

```nginx
# Upstream Definitions
upstream backend_api {
    server api-backend:3000;
    keepalive 32;
}

upstream frontend_web {
    server web-frontend:80;
    keepalive 16;
}

# Redirect HTTP (Port 80) to HTTPS (Port 443)
server {
    listen 80;
    server_name localhost;
    return 301 https://$host:8443$request_uri;
}

# HTTPS Server (Port 443 mapped to 8443 on host)
server {
    listen 443 ssl;
    http2 on;
    server_name localhost;

    # SSL Certificate Paths
    ssl_certificate /etc/nginx/certs/server.crt;
    ssl_certificate_key /etc/nginx/certs/server.key;

    # Hardened SSL Parameters
    ssl_protocols TLSv1.2 TLSv1.3;
    ssl_ciphers HIGH:!aNULL:!MD5:!3DES;
    ssl_prefer_server_ciphers on;
    ssl_session_cache shared:SSL:10m;
    ssl_session_timeout 1d;

    # Security Headers
    add_header X-Frame-Options "DENY" always;
    add_header X-Content-Type-Options "nosniff" always;
    add_header X-XSS-Protection "1; mode=block" always;
    add_header Strict-Transport-Security "max-age=31536000; includeSubDomains" always;
    add_header Referrer-Policy "strict-origin-when-cross-origin" always;

    # Upstream Routing: Backend API
    location /api/ {
        # Enforce Rate Limiting: burst 5, reject if exceeded
        limit_req zone=api_limit burst=5 nodelay;

        proxy_pass http://backend_api/;
        proxy_http_version 1.1;
        proxy_set_header Connection "";
        proxy_set_header Host $host;
        proxy_set_header X-Real-IP $remote_addr;
        proxy_set_header X-Forwarded-For $proxy_add_x_forwarded_for;
        proxy_set_header X-Forwarded-Proto https;

        proxy_connect_timeout 5s;
        proxy_read_timeout 60s;
    }

    # Upstream Routing: Static Frontend
    location / {
        proxy_pass http://frontend_web;
        proxy_http_version 1.1;
        proxy_set_header Connection "";
        proxy_set_header Host $host;
        proxy_set_header X-Real-IP $remote_addr;
        proxy_set_header X-Forwarded-For $proxy_add_x_forwarded_for;
    }

    # NGINX Stub Status Endpoint (Protected)
    location /nginx_status {
        stub_status;
        allow 127.0.0.1;
        allow 172.16.0.0/12;
        deny all;
    }
}
```

### File: `web/index.html`

```html
<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <title>Production NGINX Reverse Proxy</title>
    <style>
        body { font-family: sans-serif; text-align: center; margin-top: 100px; background: #0f172a; color: #f8fafc; }
        .card { border: 1px solid #334155; padding: 40px; display: inline-block; border-radius: 8px; background: #1e293b; }
        h1 { color: #38bdf8; }
    </style>
</head>
<body>
    <div class="card">
        <h1>NGINX Reverse Proxy Demo</h1>
        <p>Served via HTTP/2 and TLS from Docker!</p>
    </div>
</body>
</html>
```

### File: `compose.yaml`

```yaml
version: "3.8"

services:
  reverse-proxy:
    image: nginx:1.27-alpine
    restart: unless-stopped
    ports:
      - "8080:80"
      - "8443:443"
    volumes:
      - ./nginx/nginx.conf:/etc/nginx/nginx.conf:ro
      - ./nginx/conf.d:/etc/nginx/conf.d:ro
      - ./certs:/etc/nginx/certs:ro
    depends_on:
      - api-backend
      - web-frontend
    networks:
      - edge-net
    healthcheck:
      test: ["CMD-SHELL", "wget -q --spider --no-check-certificate https://localhost/nginx_status || exit 1"]
      interval: 10s
      timeout: 3s
      retries: 3
    deploy:
      resources:
        limits:
          cpus: "1.0"
          memory: 256M

  web-frontend:
    image: nginx:1.27-alpine
    restart: unless-stopped
    volumes:
      - ./web:/usr/share/nginx/html:ro
    networks:
      - edge-net
    deploy:
      resources:
        limits:
          cpus: "0.5"
          memory: 128M

  api-backend:
    image: hashicorp/http-echo:0.2.3
    restart: unless-stopped
    command: ["-text", "Hello from internal API backend microservice!", "-listen", ":3000"]
    networks:
      - edge-net
    deploy:
      resources:
        limits:
          cpus: "0.5"
          memory: 128M

networks:
  edge-net:
    driver: bridge
```

---

## 4. Step-by-Step Verification & Testing

### Step 1: Generate TLS Certificates & Launch

```bash
mkdir -p nginx-tls-reverse-proxy/certs nginx-tls-reverse-proxy/nginx/conf.d nginx-tls-reverse-proxy/web
cd nginx-tls-reverse-proxy

# Make generator executable and run
chmod +x certs/generate-certs.sh
./certs/generate-certs.sh

# Start Stack
docker compose up -d
docker compose ps
```

### Step 2: Test HTTPS & HTTP/2

```bash
# Query NGINX via HTTPS (use -k for self-signed certificates)
curl -k -I https://localhost:8443/
```

*Expected Headers Output*:
```http
HTTP/2 200 
server: nginx
date: Wed, 25 Nov 2026 09:00:00 GMT
content-type: text/html
x-frame-options: DENY
x-content-type-options: nosniff
x-xss-protection: 1; mode=block
strict-transport-security: max-age=31536000; includeSubDomains
```

### Step 3: Test Upstream API Routing

```bash
curl -k https://localhost:8443/api/
```
*Output*:
```
Hello from internal API backend microservice!
```

### Step 4: Test Rate Limiting Trigger (HTTP 429)

Send a burst of 20 concurrent requests to the `/api/` endpoint:
```bash
for i in {1..20}; do
  curl -s -o /dev/null -w "%{http_code}\n" -k https://localhost:8443/api/ &
done
wait
```

*Expected Output*:
```
200
200
200
200
200
429
429
429
...
```
*Requests exceeding the burst rate limit are blocked immediately with `HTTP 429 Too Many Requests`!*

### Step 5: Teardown

```bash
docker compose down
```

---

## 5. Production Considerations

1. **Let's Encrypt / Certbot Automation**: In public environments, mount a shared volume for `/etc/letsencrypt` and run an automated `certbot/certbot` sidecar container to renew certificates every 60 days.
2. **Buffer Tuning**: Configure `client_body_buffer_size 128k` and `proxy_buffers 8 16k` to handle payload buffering in memory rather than spilling to disk.
3. **Internal Only Backend**: Notice that `api-backend` and `web-frontend` expose **no host ports**. They can only be reached through NGINX.

---

[<- 03-spring-boot-with-database.md](./03-spring-boot-with-database.md) | [Back to Projects Overview](./README.md) | [Next Project: 05-multi-stage-minimal-go-image.md ->](./05-multi-stage-minimal-go-image.md)
