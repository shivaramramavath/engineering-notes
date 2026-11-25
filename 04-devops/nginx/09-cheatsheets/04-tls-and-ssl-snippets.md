---
title: "Cheatsheet: TLS/SSL Hardening & Cryptographic Snippets"
description: "Production copy-pasteable snippets for Mozilla Modern and Intermediate TLS profiles, Let's Encrypt webroot routing, OCSP stapling, and mutual TLS (mTLS)."
module: "09-cheatsheets"
topic: "04-tls-and-ssl-snippets"
status: "completed"
tags: ["nginx", "cheatsheet", "tls", "ssl", "snippets", "hsts", "ocsp-stapling", "mtls"]
prerequisites: ["02-core-features/03-tls-https.md", "07-projects/07-tls-termination-and-certbot-automation.md"]
estimated_read_time: "20 min"
---

# Cheatsheet: TLS/SSL Hardening & Cryptographic Snippets

```
+-------------------------------------------------------------------------------+
| TLS Security Profiles: Modern vs Intermediate                                 |
|                                                                               |
|  Profile         | Protocols   | Forward Secrecy | Client Compatibility        |
|  :-------------- | :---------- | :-------------- | :-------------------------- |
|  **Modern**      | TLSv1.3 only| Always          | Modern browsers (2020+)     |
|  **Intermediate**| TLSv1.2/1.3 | Always (ECDHE)  | General Production (99.9%)  |
|  **Deprecated**  | TLSv1.0/1.1 | Weak / None     | INSECURE! Never use in prod |
+-------------------------------------------------------------------------------+
```

---

## 1. Mozilla Intermediate TLS Profile (Recommended Production Default)

Provides Grade A+ security on SSL Labs while supporting 99.9% of modern clients and mobile devices:

```nginx
# /etc/nginx/snippets/ssl-intermediate.conf

# Protocols
ssl_protocols TLSv1.2 TLSv1.3;

# Cipher Suites (Prioritizes Authenticated Encryption with Associated Data)
ssl_ciphers ECDHE-ECDSA-AES128-GCM-SHA256:ECDHE-RSA-AES128-GCM-SHA256:ECDHE-ECDSA-AES256-GCM-SHA384:ECDHE-RSA-AES256-GCM-SHA384:ECDHE-ECDSA-CHACHA20-POLY1305:ECDHE-RSA-CHACHA20-POLY1305:DHE-RSA-AES128-GCM-SHA256:DHE-RSA-AES256-GCM-SHA384;
ssl_prefer_server_ciphers off;

# Session Management
ssl_session_cache shared:SSL:20m;
ssl_session_timeout 1d;
ssl_session_tickets off; # Preserves Perfect Forward Secrecy

# OCSP Stapling
ssl_stapling on;
ssl_stapling_verify on;
resolver 1.1.1.1 8.8.8.8 valid=300s;
resolver_timeout 5s;

# Security Headers (HSTS with Preloading)
add_header Strict-Transport-Security "max-age=63072000; includeSubDomains; preload" always;
```

---

## 2. Mozilla Modern TLS Profile (Maximum Security / Internal APIs)

Requires modern clients supporting TLS 1.3 exclusively (no legacy TLS 1.2):

```nginx
# /etc/nginx/snippets/ssl-modern.conf

# Protocol (TLS 1.3 Exclusively)
ssl_protocols TLSv1.3;

# Session Management
ssl_session_cache shared:SSL:20m;
ssl_session_timeout 1d;
ssl_session_tickets off;

# OCSP Stapling
ssl_stapling on;
ssl_stapling_verify on;
resolver 1.1.1.1 8.8.8.8 valid=300s;
resolver_timeout 5s;

# Security Headers
add_header Strict-Transport-Security "max-age=63072000; includeSubDomains; preload" always;
```

---

## 3. Let's Encrypt Shared Webroot ACME Snippet

Save to `/etc/nginx/snippets/acme-challenge.conf`:

```nginx
# /etc/nginx/snippets/acme-challenge.conf

# Intercepts ACME challenge tokens on Port 80 before HTTPS redirect
location ^~ /.well-known/acme-challenge/ {
    default_type "text/plain";
    root /var/www/certbot;
    allow all;
}
```

Usage in Port 80 server block:
```nginx
server {
    listen 80;
    server_name example.com www.example.com;

    include /etc/nginx/snippets/acme-challenge.conf;

    location / {
        return 301 https://$host$request_uri;
    }
}
```

---

## 4. Mutual TLS (mTLS) Client Certificate Authentication Snippet

Forces incoming clients (or CDN edge nodes) to present a valid X.509 client certificate:

```nginx
server {
    listen 443 ssl;
    server_name internal.example.com;

    # Server TLS Certificate
    ssl_certificate /etc/nginx/ssl/server.crt;
    ssl_certificate_key /etc/nginx/ssl/server.key;

    # Client CA Validation Bundle
    ssl_client_certificate /etc/nginx/certs/internal-ca.crt;
    ssl_verify_client on; # Set to 'optional' if only specific endpoints require cert
    ssl_verify_depth 2;

    location / {
        proxy_pass http://backend_pool;
        # Pass client certificate identity upstream to application
        proxy_set_header X-Client-DN $ssl_client_s_dn;
        proxy_set_header X-Client-Verify $ssl_client_verify;
    }
}
```

---

## 5. Generating Self-Signed Fallback Certificates for Default Catch-All

Generate a 10-year dummy keypair so the `default_server` can drop HTTPS scans with HTTP 444 without referencing customer production certificates:

```bash
# Generate dummy fallback keypair
sudo mkdir -p /etc/nginx/ssl
sudo openssl req -x509 -nodes -days 3650 -newkey rsa:2048 \
  -keyout /etc/nginx/ssl/dummy-fallback.key \
  -out /etc/nginx/ssl/dummy-fallback.crt \
  -subj "/CN=untrusted-catchall"

# Set strict permissions
sudo chmod 600 /etc/nginx/ssl/dummy-fallback.key
sudo chmod 644 /etc/nginx/ssl/dummy-fallback.crt
```

Catch-all server block in `/etc/nginx/conf.d/00-default.conf`:
```nginx
server {
    listen 443 ssl default_server;
    listen [::]:443 ssl default_server;
    server_name _;

    ssl_certificate /etc/nginx/ssl/dummy-fallback.crt;
    ssl_certificate_key /etc/nginx/ssl/dummy-fallback.key;
    ssl_protocols TLSv1.2 TLSv1.3;

    return 444; # Drop connection immediately
}
```

---

[← Previous Cheatsheet: 03-proxy-and-headers-snippets.md](03-proxy-and-headers-snippets.md) | [Back to Module Index](README.md) | [Next Cheatsheet: 05-caching-and-rate-limiting-snippets.md →](05-caching-and-rate-limiting-snippets.md)
