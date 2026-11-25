---
title: "Cheatsheet: Reverse Proxy, Upstreams & Headers Snippets"
description: "Production copy-pasteable snippets for NGINX reverse proxying, WebSockets, gRPC, keepalives, upstream headers, FastCGI PHP-FPM, and buffer configurations."
module: "09-cheatsheets"
topic: "03-proxy-and-headers-snippets"
status: "completed"
tags: ["nginx", "cheatsheet", "snippets", "proxy", "headers", "websockets", "grpc", "fastcgi"]
prerequisites: ["02-core-features/02-reverse-proxy.md", "03-traffic-management/02-load-balancing.md"]
estimated_read_time: "20 min"
---

# Cheatsheet: Reverse Proxy, Upstreams & Headers Snippets

```
+-------------------------------------------------------------------------------+
| Production Reverse Proxy Modular Directory Layout                             |
|                                                                               |
|  /etc/nginx/                                                                  |
|  ├── nginx.conf                                                               |
|  ├── snippets/                                                                |
|  │   ├── proxy-params.conf       (Standard proxy headers & timeouts)          |
|  │   ├── websocket-params.conf   (Hop-by-hop upgrade headers)                 |
|  │   └── fastcgi-php.conf        (Optimized FastCGI parameters)               |
|  └── conf.d/                                                                  |
|      ├── upstreams.conf          (Backend clusters with keepalives)           |
|      └── sites.conf              (Virtual hosts including snippets)           |
+-------------------------------------------------------------------------------+
```

---

## 1. Standard Production Reverse Proxy Snippet (`proxy-params.conf`)

Save to `/etc/nginx/snippets/proxy-params.conf`:

```nginx
# /etc/nginx/snippets/proxy-params.conf

# Protocol and Persistent Keepalive Reuse
proxy_http_version 1.1;
proxy_set_header Connection "";

# Client Identity Preservation
proxy_set_header Host $host;
proxy_set_header X-Real-IP $remote_addr;
proxy_set_header X-Forwarded-For $proxy_add_x_forwarded_for;
proxy_set_header X-Forwarded-Proto $scheme;
proxy_set_header X-Forwarded-Host $host;
proxy_set_header X-Forwarded-Port $server_port;

# Hide Upstream Stack Fingerprints
proxy_hide_header X-Powered-By;
proxy_hide_header Server;
proxy_hide_header X-AspNet-Version;

# Resilient Timeouts
proxy_connect_timeout 5s;
proxy_send_timeout 30s;
proxy_read_timeout 30s;

# Upstream Buffer Optimization
proxy_buffering on;
proxy_buffer_size 8k;
proxy_buffers 16 32k;
proxy_busy_buffers_size 64k;
proxy_temp_file_write_size 64k;

# Transparent Retries
proxy_next_upstream error timeout http_502 http_503;
proxy_next_upstream_tries 3;
```

---

## 2. WebSocket & Real-Time Upgrade Snippet

Save mapping in `http {}` context in `/etc/nginx/conf.d/maps.conf`:
```nginx
map $http_upgrade $connection_upgrade {
    default upgrade;
    ''      close;
}
```

Include inside your WebSocket location block:
```nginx
location /ws/ {
    proxy_pass http://websocket_cluster;
    proxy_http_version 1.1;

    # Hop-by-Hop Protocol Switching Headers
    proxy_set_header Upgrade $http_upgrade;
    proxy_set_header Connection $connection_upgrade;

    proxy_set_header Host $host;
    proxy_set_header X-Real-IP $remote_addr;
    proxy_set_header X-Forwarded-For $proxy_add_x_forwarded_for;

    # Disable Buffering for Instant Messaging
    proxy_buffering off;

    # Extended Idle Timeouts
    proxy_read_timeout 3600s;
    proxy_send_timeout 3600s;
}
```

---

## 3. Native gRPC Bidirectional Proxying Snippet

For modern gRPC microservices over HTTP/2:

```nginx
server {
    listen 50051 ssl;
    http2 on;
    server_name grpc.example.com;

    ssl_certificate /etc/letsencrypt/live/grpc.example.com/fullchain.pem;
    ssl_certificate_key /etc/letsencrypt/live/grpc.example.com/privkey.pem;

    location / {
        # Forward directly to gRPC backend
        grpc_pass grpc://127.0.0.1:9000;

        # For TLS-encrypted gRPC backends:
        # grpc_pass grpcs://127.0.0.1:9443;

        grpc_set_header Host $host;
        grpc_set_header X-Real-IP $remote_addr;

        grpc_read_timeout 60s;
        grpc_send_timeout 60s;
    }
}
```

---

## 4. Server-Sent Events (SSE) & LLM Token Streaming Snippet

Essential for ChatGPT, Claude, and local Ollama streaming responses:

```nginx
location /api/v1/generate {
    proxy_pass http://llm_backend;
    proxy_http_version 1.1;
    proxy_set_header Connection "";

    proxy_set_header Host $host;
    proxy_set_header X-Real-IP $remote_addr;
    proxy_set_header X-Forwarded-For $proxy_add_x_forwarded_for;

    # CRITICAL: Turn OFF Buffering & Caching
    proxy_buffering off;
    proxy_cache off;
    chunked_transfer_encoding on;

    # Tell Cloudflare & intermediate CDNs to disable buffering
    proxy_set_header X-Accel-Buffering "no";
    add_header Cache-Control "no-cache, no-transform" always;

    # Allow model inference to run up to 10 minutes
    proxy_read_timeout 600s;
}
```

---

## 5. Optimized FastCGI PHP-FPM Snippet

Save to `/etc/nginx/snippets/fastcgi-php.conf`:

```nginx
# /etc/nginx/snippets/fastcgi-php.conf

location ~ \.php$ {
    try_files $uri =404;

    fastcgi_split_path_info ^(.+\.php)(/.+)$;
    fastcgi_pass unix:/run/php/php8.3-fpm.sock;
    fastcgi_index index.php;
    include fastcgi_params;

    fastcgi_param SCRIPT_FILENAME $document_root$fastcgi_script_name;
    fastcgi_param PATH_INFO $fastcgi_path_info;

    # Buffering
    fastcgi_buffer_size 16k;
    fastcgi_buffers 16 16k;
    fastcgi_busy_buffers_size 32k;

    # Timeouts
    fastcgi_connect_timeout 5s;
    fastcgi_send_timeout 60s;
    fastcgi_read_timeout 60s;
}
```

---

## 6. Upstream Cluster with Keepalive Pooling

```nginx
upstream backend_cluster {
    least_conn;

    server 10.0.1.11:8080 weight=5 max_fails=3 fail_timeout=10s;
    server 10.0.1.12:8080 weight=5 max_fails=3 fail_timeout=10s;
    server 10.0.1.99:8080 backup; # Standby failover node

    # Keepalive pool: 64 persistent connections per worker
    keepalive 64;
    keepalive_requests 5000;
    keepalive_timeout 60s;
}
```

---

[← Previous Cheatsheet: 02-location-matching-table.md](02-location-matching-table.md) | [Back to Module Index](README.md) | [Next Cheatsheet: 04-tls-and-ssl-snippets.md →](04-tls-and-ssl-snippets.md)
