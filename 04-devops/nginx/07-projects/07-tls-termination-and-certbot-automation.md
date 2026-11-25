---
title: "Project 07: Automated Zero-Downtime TLS Edge with Let's Encrypt & Certbot"
description: "Production reference implementation for automated TLS 1.3 certificate lifecycle management: ACME HTTP-01 webroot challenge routing, automated systemd renewal hooks, OCSP stapling, and Mozilla Modern SSL profile."
module: "07-projects"
project_id: "07-tls-termination-and-certbot-automation"
status: "completed"
tags: ["nginx", "project", "tls", "https", "certbot", "letsencrypt", "acme", "ocsp-stapling", "ssl-automation"]
prerequisites: ["02-core-features/03-tls-https.md", "02-core-features/11-acme-and-certbot-automation.md"]
estimated_read_time: "35 min"
---

# Project 07: Automated Zero-Downtime TLS Edge with Let's Encrypt & Certbot

```
+-------------------------------------------------------------------------------+
| Automated ACME HTTP-01 Renewal & Zero-Downtime Reload Cycle                   |
|                                                                               |
|  [Let's Encrypt Validation Server]                                            |
|        │                                                                      |
|        ▼ HTTP GET /.well-known/acme-challenge/<TOKEN> (Port 80)               |
|  [NGINX Port 80 Listener]                                                     |
|        │                                                                      |
|        ├── Location: /.well-known/acme-challenge/                             |
|        │   Serves token directly from shared webroot: `/var/www/certbot`      |
|        │   Validates domain control instantly!                                |
|        │                                                                      |
|        └── Location: / (All other requests)                                   |
|            Redirects: return 301 https://$host$request_uri;                   |
|                                                                               |
|  [Automated Renewal Lifecycle (Systemd Timer: Twice Daily)]                   |
|  1. Certbot runs: checks certificate expiration (<30 days remaining).         |
|  2. Solves HTTP-01 challenge via `/var/www/certbot`.                          |
|  3. Writes fresh `fullchain.pem` and `privkey.pem`.                          |
|  4. Executes Deploy Hook: `systemctl reload nginx` (kill -HUP).              |
|  5. NGINX Master loads new certs in memory with ZERO DROPPED PACKETS!        |
+-------------------------------------------------------------------------------+
```

---

## 1. Architecture & Objective

### Objective
Deploy an automated, self-healing TLS edge termination tier using NGINX and Certbot that provisions, validates, and automatically renews Let's Encrypt certificates without manual operator intervention, service downtime, or port contention.

### Core Architectural Requirements
1. **Shared Webroot ACME Routing**: Handle Let's Encrypt HTTP-01 validation challenges on port 80 via a dedicated shared directory (`/var/www/certbot`), avoiding fragile standalone proxy shutdowns.
2. **Global Canonical HTTPS Redirection**: Redirect all non-ACME port 80 traffic to HTTPS using canonical permanent 301 redirects.
3. **Mozilla Modern SSL Profile**: Enforce strict TLS 1.3 and TLS 1.2 with Forward Secrecy ciphers, disabled session tickets, and a 2-year HSTS preload policy.
4. **OCSP Stapling**: Enable Online Certificate Status Protocol (OCSP) stapling to accelerate client TLS handshakes by eliminating real-time CA revocation checks.
5. **Zero-Downtime Renewal Hook**: Automate certificate reloading via `systemctl reload nginx` directly inside Certbot's `renewal-hooks/deploy` directory.

---

## 2. Complete Config Code

### 1. Shared ACME Challenge Snippet (`/etc/nginx/snippets/acme-challenge.conf`)

```nginx
# Handle Let's Encrypt ACME HTTP-01 challenge tokens
location ^~ /.well-known/acme-challenge/ {
    default_type "text/plain";
    root /var/www/certbot;
    allow all;
}
```

---

### 2. Standard SSL Configuration Snippet (`/etc/nginx/snippets/ssl-params.conf`)

```nginx
# Mozilla Modern TLS Profile
ssl_protocols TLSv1.2 TLSv1.3;
ssl_ciphers ECDHE-ECDSA-AES128-GCM-SHA256:ECDHE-RSA-AES128-GCM-SHA256:ECDHE-ECDSA-AES256-GCM-SHA384:ECDHE-RSA-AES256-GCM-SHA384:ECDHE-ECDSA-CHACHA20-POLY1305:ECDHE-RSA-CHACHA20-POLY1305;
ssl_prefer_server_ciphers off;

# Session Cache Optimization
ssl_session_cache shared:SSL:20m;
ssl_session_timeout 1d;
ssl_session_tickets off; # Preserves Perfect Forward Secrecy

# OCSP Stapling (Validates certificate revocation status in advance)
ssl_stapling on;
ssl_stapling_verify on;
resolver 1.1.1.1 8.8.8.8 valid=300s;
resolver_timeout 5s;

# Security Headers (HSTS with Preload)
add_header Strict-Transport-Security "max-age=63072000; includeSubDomains; preload" always;
add_header X-Content-Type-Options "nosniff" always;
add_header X-Frame-Options "DENY" always;
```

---

### 3. Production Virtual Host Configuration (`/etc/nginx/sites-available/secure.example.com.conf`)

```nginx
# HTTP (Port 80): ACME Validation + Canonical Redirect
server {
    listen 80;
    listen [::]:80;
    server_name secure.example.com;

    # Include shared ACME challenge handler
    include /etc/nginx/snippets/acme-challenge.conf;

    # Redirect all other traffic to HTTPS
    location / {
        return 301 https://$host$request_uri;
    }
}

# HTTPS (Port 443): Hardened TLS Termination
server {
    listen 443 ssl;
    listen [::]:443 ssl;
    http2 on;
    server_name secure.example.com;

    # Certificate Keypair Paths
    ssl_certificate /etc/letsencrypt/live/secure.example.com/fullchain.pem;
    ssl_certificate_key /etc/letsencrypt/live/secure.example.com/privkey.pem;
    ssl_trusted_certificate /etc/letsencrypt/live/secure.example.com/chain.pem; # For OCSP Stapling

    # Include Tuned SSL Parameters & Ciphers
    include /etc/nginx/snippets/ssl-params.conf;

    root /var/www/html;
    index index.html;

    location / {
        try_files $uri $uri/ =404;
    }
}
```

---

### 4. Certbot Automated Renewal Deploy Hook

Create an automated hook script executed by Certbot immediately after successful renewal:

```bash
# /etc/letsencrypt/renewal-hooks/deploy/reload-nginx.sh
#!/usr/bin/env bash
set -euo pipefail

echo "==> Certbot renewal detected. Validating NGINX syntax..."
if /usr/sbin/nginx -t; then
    echo "==> Syntax valid. Reloading NGINX gracefully..."
    /bin/systemctl reload nginx
    echo "[+] NGINX reloaded successfully with new certificate."
else
    echo "[-] ERROR: NGINX syntax invalid! Aborting reload." >&2
    exit 1
fi
```
Set executable permissions:
```bash
sudo chmod +x /etc/letsencrypt/renewal-hooks/deploy/reload-nginx.sh
```

---

## 3. Step-by-Step Walkthrough

1. **The Initial Bootstrap Dilemma (Chicken-and-Egg Problem)**:
   - When deploying a new server, NGINX cannot start if `ssl_certificate` paths point to non-existent Let's Encrypt files!
   - *Solution*:
     1. Create the ACME webroot directory: `sudo mkdir -p /var/www/certbot && sudo chown -R www-data:www-data /var/www/certbot`.
     2. Temporarily comment out the `listen 443 ssl` server block.
     3. Start NGINX with port 80 only.
     4. Request initial certificates via Certbot:
        ```bash
        sudo certbot certonly --webroot -w /var/www/certbot -d secure.example.com --non-interactive --agree-tos -m admin@example.com
        ```
     5. Uncomment the port 443 block and reload NGINX (`sudo nginx -s reload`).

2. **Webroot vs Standalone Mode**:
   - `certbot --standalone` spins up its own temporary HTTP server on port 80, which requires forcefully stopping NGINX (`systemctl stop nginx`), creating minutes of downtime during renewals!
   - `certbot --webroot` writes validation challenge files directly into `/var/www/certbot/.well-known/acme-challenge/`.
   - NGINX serves the files while continuing to handle production traffic uninterrupted.

3. **OCSP Stapling Explained**:
   - When a browser connects via TLS, it must verify that the server's certificate has not been revoked.
   - Without stapling, the browser must pause and query Let's Encrypt's OCSP responder over the internet, adding 50–200ms to connection setup.
   - `ssl_stapling on;` instructs NGINX to periodically contact the OCSP server in the background, cache the cryptographically signed revocation assertion, and "staple" it into the initial TLS `ServerHello` handshake.
   - `ssl_trusted_certificate` provides the intermediate CA bundle needed to verify the stapled response.

4. **Automated Zero-Downtime Reload**:
   - Let's Encrypt certificates expire every 90 days.
   - The systemd timer (`certbot.timer`) runs twice daily. When expiration drops below 30 days, Certbot renews the files on disk and immediately executes `/etc/letsencrypt/renewal-hooks/deploy/reload-nginx.sh`.
   - NGINX master reads the new certificate files via `kill -HUP` and transitions workers smoothly without severing a single active TCP socket.

---

## 4. Verification & Testing

### Test 1: Test ACME Challenge Path Reachability
```bash
# Create a dummy verification token
echo "acme-test-token-12345" | sudo tee /var/www/certbot/test-token

# Query via HTTP (Port 80)
curl -i http://secure.example.com/.well-known/acme-challenge/test-token
```
*Expected Output*:
```http
HTTP/1.1 200 OK
content-type: text/plain

acme-test-token-12345
```
Notice that the request is **NOT redirected** to HTTPS; it returns HTTP 200 directly!

### Test 2: Test Automated Certbot Dry-Run Renewal
Test the entire renewal pipeline without updating live certificates:
```bash
sudo certbot renew --dry-run
```
*Expected Output*:
```text
- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
Processing /etc/letsencrypt/renewal/secure.example.com.conf
- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
Simulating renewal of an existing certificate for secure.example.com
The dry run was successful.
Running deploy-hook command: /etc/letsencrypt/renewal-hooks/deploy/reload-nginx.sh
==> Certbot renewal detected. Validating NGINX syntax...
nginx: configuration file /etc/nginx/nginx.conf test is successful
[+] NGINX reloaded successfully with new certificate.
```

### Test 3: Verify OCSP Stapling via OpenSSL
```bash
openssl s_client -connect secure.example.com:443 -status -servername secure.example.com < /dev/null | grep -A 17 "OCSP response:"
```
*Expected Output*:
```text
OCSP Response Data:
    OCSP Response Status: successful (0x0)
    Response Type: Basic OCSP Response
    Certificate Status: good
```

---

## 5. Troubleshooting & Edge Cases

### Edge Case 1: NGINX Fails to Start After Reboot (Missing Cert Files)
- **Symptom**: `nginx.service` fails on boot with `BIO_new_file() failed (SSL: error:02001002:system library:fopen:No such file or directory)`.
- **Root Cause**: Let's Encrypt directory was deleted, unmounted, or certificate symlinks are broken.
- **Fix**: Point `ssl_certificate` temporarily to dummy self-signed certificates, start NGINX, and run Certbot to restore live certificates.

### Edge Case 2: CAA Record Blocks Let's Encrypt Issuance
- **Symptom**: Certbot fails with `CAA record does not permit issuer`.
- **Root Cause**: The DNS zone contains a Certificate Authority Authorization (CAA) record that restricts certificate issuance to DigiCert or Sectigo.
- **Fix**: Add a DNS CAA record permitting Let's Encrypt:
  ```dns
  example.com. IN CAA 0 issue "letsencrypt.org"
  ```

---

[← Previous Project: 06-rate-limited-public-api-gateway.md](06-rate-limited-public-api-gateway.md) | [Back to Module Index](README.md) | [Next Project: 08-canary-release-and-traffic-splitting.md →](08-canary-release-and-traffic-splitting.md)
