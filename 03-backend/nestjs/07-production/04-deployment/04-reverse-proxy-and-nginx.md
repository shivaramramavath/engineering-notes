---
title: "Reverse Proxy and Nginx Configuration for NestJS"
description: "Configuring Nginx for SSL termination, load balancing, WebSocket upgrades, rate limiting, and caching in front of NestJS"
tags: [nestjs, deployment, nginx, reverse-proxy, ssl, load-balancing, websockets]
nestjs_version: "11.x"
node_version: ">=20.0.0"
express_version: "5.x"
---

# Reverse Proxy and Nginx Configuration for NestJS

## Conceptual Overview
While NestJS can serve HTTP directly, placing an enterprise reverse proxy like **Nginx** (or an AWS Application Load Balancer / Cloudflare) in front provides critical capabilities:
- **TLS/SSL Termination**: Offloading asymmetric cryptographic handshakes from Node.js CPU cores.
- **Static File Caching**: Serving images, assets, and documentation at wire speed without waking V8.
- **Gzip / Brotli Compression**: Compressing outbound payloads natively in C.
- **WebSocket Connection Upgrades**: Managing persistent HTTP `Upgrade` connections.

---

## Production Nginx Configuration

```nginx
# /etc/nginx/conf.d/nestjs.conf

upstream nestjs_cluster {
    # Least-connection load balancing across multiple NestJS instances
    least_conn;
    server 127.0.0.1:3000 max_fails=3 fail_timeout=10s;
    server 127.0.0.1:3001 max_fails=3 fail_timeout=10s;
    keepalive 64;
}

server {
    listen 80;
    server_name api.yourdomain.com;
    return 301 https://$host$request_uri;
}

server {
    listen 443 ssl http2;
    server_name api.yourdomain.com;

    ssl_certificate /etc/letsencrypt/live/api.yourdomain.com/fullchain.pem;
    ssl_certificate_key /etc/letsencrypt/live/api.yourdomain.com/privkey.pem;
    ssl_protocols TLSv1.2 TLSv1.3;
    ssl_ciphers HIGH:!aNULL:!MD5;

    # Gzip Compression
    gzip on;
    gzip_types text/plain application/json application/javascript text/css;

    # Security buffer sizes
    client_max_body_size 10M;

    location / {
        proxy_pass http://nestjs_cluster;
        proxy_http_version 1.1;

        # WebSocket support
        proxy_set_header Upgrade $http_upgrade;
        proxy_set_header Connection "upgrade";

        # Forwarded Client Headers
        proxy_set_header Host $host;
        proxy_set_header X-Real-IP $remote_addr;
        proxy_set_header X-Forwarded-For $proxy_add_x_forwarded_for;
        proxy_set_header X-Forwarded-Proto $scheme;

        # Timeouts
        proxy_connect_timeout 5s;
        proxy_read_timeout 60s;
        proxy_send_timeout 60s;
    }
}
```

---

## Key Takeaways & Cheat Sheet

- Terminate TLS and compress responses at the Nginx layer.
- Pass `X-Forwarded-For` and `X-Forwarded-Proto` and enable `app.set('trust proxy', 1)` in NestJS.
- Support WebSockets by setting `proxy_set_header Upgrade` and `Connection`.

---

## Cross-References
- [02-http-security-headers-and-cors.md](../01-security/02-http-security-headers-and-cors.md) - Security headers
- [03-docker.md](./03-docker.md) - Docker containerization
