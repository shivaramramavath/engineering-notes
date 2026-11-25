---
title: "ACME and Certbot Automation in NGINX"
description: "Production guide to automating TLS certificate issuance, HTTP-01 webroot challenges, DNS-01 wildcards, zero-downtime renewals, and Certbot deploy hooks in NGINX."
module: "02-core-features"
track: "devops-nginx"
order: 11
difficulty: "Intermediate to Advanced"
prerequisites:
  - "01-fundamentals/03-server-and-location"
  - "02-core-features/03-tls-https"
tags:
  - nginx
  - acme
  - certbot
  - lets-encrypt
  - ssl-automation
  - zero-downtime
  - tls
last_updated: "2026-03-30"
---

# ACME and Certbot Automation in NGINX

## Table of Contents
- [1. What & Why](#1-what--why)
- [2. Core Concepts](#2-core-concepts)
- [3. How It Works Internally](#3-how-it-works-internally)
- [4. Syntax & Challenge Configuration](#4-syntax--challenge-configuration)
- [5. Practical Implementation Examples](#5-practical-implementation-examples)
  - [Example 1: The Production Webroot HTTP-01 Challenge Snippet](#example-1-the-production-webroot-http-01-challenge-snippet)
  - [Example 2: Complete Bootstrap: Zero to Working HTTPS with Certbot](#example-2-complete-bootstrap-zero-to-working-https-with-certbot)
  - [Example 3: Multi-Domain SAN (Subject Alternative Name) Certificate Setup](#example-3-multi-domain-san-subject-alternative-name-certificate-setup)
  - [Example 4: DNS-01 Challenge for Wildcard Certificates (`*.example.com`)](#example-4-dns-01-challenge-for-wildcard-certificates-examplecom)
  - [Example 5: Systemd Timer Automation with Graceful Reload Hooks](#example-5-systemd-timer-automation-with-graceful-reload-hooks)
- [6. Production Best Practices](#6-production-best-practices)
- [7. Common Pitfalls & Anti-Patterns](#7-common-pitfalls--anti-patterns)
- [8. Performance, Security & Troubleshooting](#8-performance-security--troubleshooting)
- [9. Architectural Comparison Table](#9-architectural-comparison-table)
- [10. Technical Interview Questions](#10-technical-interview-questions)
- [11. Hands-On Practice Exercises](#11-hands-on-practice-exercises)
- [12. Summary & Quick Reference](#12-summary--quick-reference)
- [13. Further Reading & Official Documentation](#13-further-reading--official-documentation)

---

## 1. What & Why

Before the creation of the Automatic Certificate Management Environment (ACME protocol, RFC 8555) and free automated Certificate Authorities like Let's Encrypt and ZeroSSL, provisioning TLS certificates was a manual, error-prone, and expensive operational bottleneck. Certificates had 1-to-2 year lifespans, were renewed manually via CSR copy-pasting, and unexpected expirations frequently caused massive customer-facing outages.

Today, standard industry best practice mandates **short-lived certificates (90 days or less)** paired with **fully automated issuance and renewal pipelines**. 

In NGINX environments:
1. **Certbot** (or lightweight alternatives like Acme.sh and Lego) negotiates validation challenges with the ACME CA.
2. NGINX serves the **HTTP-01 challenge token** over plain HTTP port 80 via a dedicated `.well-known/acme-challenge/` webroot path.
3. Upon successful renewal, automated hooks send a graceful `reload` signal (`SIGHUP`) to NGINX, updating active cryptographic contexts with **zero dropped client connections**.

---

## 2. Core Concepts

### The ACME Validation Challenges
The ACME protocol defines three primary challenge types to prove that an applicant actually controls the domain name requested:

1. **HTTP-01 Challenge**:
   - The CA provides a unique challenge token and cryptographic key authorization.
   - The ACME client places this token at:
     `http://<YOUR_DOMAIN>/.well-known/acme-challenge/<TOKEN>`
   - The CA makes an outbound HTTP GET request to port 80 of your domain to verify the token file.
   - **Advantages**: Simple, reliable, requires zero DNS API credentials.
   - **Limitations**: Cannot issue wildcard certificates (`*.example.com`); requires port 80 to be publicly accessible.

2. **DNS-01 Challenge**:
   - The ACME client creates a specific TXT record in your domain's DNS zone:
     `_acme-challenge.<YOUR_DOMAIN> IN TXT "<HASH>"`
   - The CA queries authoritative DNS servers to verify the record.
   - **Advantages**: Supports wildcard certificates (`*.example.com`); works for internal, private, or firewalled servers.
   - **Limitations**: Requires storing sensitive DNS API credentials on the server or using delegated CNAME zones.

3. **TLS-ALPN-01 Challenge**:
   - Validation occurs over TLS port 443 using a custom ALPN protocol (`acme-tls/1`).
   - Primarily used by standalone proxies (Caddy, Traefik); rarely used directly in NGINX.

### Certbot Plugin Modes: `--webroot` vs `--nginx` vs `--standalone`
- **`--webroot` (Recommended for Production)**: Certbot does *not* touch, edit, or modify your NGINX configuration files. It simply writes challenge files to a designated directory on disk (e.g., `/var/www/certbot`), which NGINX serves statically. Zero risk of config corruption.
- **`--nginx`**: Certbot automatically parses and rewrites your NGINX config files. While convenient for quick demos, it can introduce syntax errors, unexpected directive re-ordering, and fragile automation in enterprise CI/CD.
- **`--standalone`**: Certbot runs its own temporary web server on port 80. Requires stopping NGINX before renewal, causing planned downtime. Unacceptable for production.

---

## 3. How It Works Internally

### The Complete HTTP-01 Challenge and Renewal Lifecycle

```
[Certbot Client on Host]                 [Let's Encrypt ACME Server]                  [Public Web Client]
          |                                            |                                       |
 1. Request Certificate                                |                                       |
    (Domain: api.example.com) ------------------------>|                                       |
          |<-- 2. Issue Challenge Token & URL ---------|                                       |
          |                                            |                                       |
 3. Writes Token File to disk:                         |                                       |
    /var/www/certbot/.well-known/                      |                                       |
    acme-challenge/<TOKEN>                             |                                       |
          |                                            |                                       |
          |-- 4. Notify CA ready --------------------->|                                       |
          |                                            |                                       |
          |                                  5. Outbound Validation Probe                      |
          |                                     HTTP GET api.example.com:80                    |
          |                                     /.well-known/acme-challenge/<TOKEN>            |
          |                                            |                                       |
          |        +-----------------------------------+                                       |
          |        |                                                                           |
          |        v                                                                           |
   +--------------+------------------------------------+                                       |
   | NGINX Edge (Port 80)                              |                                       |
   | Location: ^~ /.well-known/acme-challenge/         |                                       |
   | Reads: /var/www/certbot/... (static file)         |                                       |
   | Returns: HTTP 200 OK with token                   |                                       |
   +--------------+------------------------------------+                                       |
          |       |                                                                            |
          |<------+ 6. CA verifies signature & token                                           |
          |                                            |                                       |
 7. Certificate & Full Chain Signed                    |                                       |
    Downloaded to:                                     |                                       |
    /etc/letsencrypt/live/api.example.com/             |                                       |
          |                                            |                                       |
 8. Deploy Hook Executes:                              |                                       |
    systemctl reload nginx                             |                                       |
          |                                            |                                       |
   +------v--------------------------------------------+                                       |
   | NGINX Master reads new certs, spawns new workers  |                                       |
   | Old workers drain connections and terminate       |                                       |
   +---------------------------------------------------+                                       |
                                                                                               |
 9. Subsequent HTTPS Requests served with new certificate =====================================>|
```

---

## 4. Syntax & Challenge Configuration

### The Universal ACME Challenge Snippet
To enable Certbot to validate any domain across multiple virtual hosts cleanly, create a reusable snippet:

```nginx
# /etc/nginx/snippets/acme_challenge.conf

location ^~ /.well-known/acme-challenge/ {
    # Specify the directory where Certbot writes challenge files
    root /var/www/certbot;
    
    # Ensure raw plain text delivery without redirects
    default_type "text/plain";
    
    # Allow access from any source
    allow all;
    
    # Disable caching for challenge responses
    expires -1;
    add_header Cache-Control "no-store, no-cache, must-revalidate" always;
}
```

---

## 5. Practical Implementation Examples

### Example 1: The Production Webroot HTTP-01 Challenge Snippet

A clean, modular HTTP port 80 virtual host that routes ACME challenge requests to the webroot directory while redirecting all normal client traffic to HTTPS.

```nginx
# /etc/nginx/conf.d/http_gateway.conf

server {
    listen 80 default_server;
    listen [::]:80 default_server;
    server_name _;

    # 1. ACME Challenge Path (Takes precedence over redirect!)
    include /etc/nginx/snippets/acme_challenge.conf;

    # 2. Redirect all other traffic permanently to HTTPS
    location / {
        return 301 https://$host$request_uri;
    }
}
```

### Example 2: Complete Bootstrap: Zero to Working HTTPS with Certbot

A step-by-step procedure to issue certificates for a brand-new domain from scratch without causing chicken-and-egg configuration errors.

#### Step 1: Ensure Webroot Directory Exists
```bash
mkdir -p /var/www/certbot/.well-known/acme-challenge
chown -R www-data:www-data /var/www/certbot
chmod -R 755 /var/www/certbot
```

#### Step 2: Configure Port 80 Bootstrap in NGINX
```nginx
# /etc/nginx/conf.d/portal.conf
server {
    listen 80;
    server_name portal.example.com;

    include /etc/nginx/snippets/acme_challenge.conf;

    location / {
        return 200 "Bootstrap in progress...\n";
    }
}
```
Reload NGINX: `nginx -t && nginx -s reload`.

#### Step 3: Run Certbot to Issue Certificate
```bash
certbot certonly --webroot \
  -w /var/www/certbot \
  -d portal.example.com \
  --email admin@example.com \
  --agree-tos \
  --no-eff-email \
  --non-interactive
```

#### Step 4: Add the HTTPS Server Block
Now that `/etc/letsencrypt/live/portal.example.com/` exists, configure the full production HTTPS virtual host:

```nginx
# /etc/nginx/conf.d/portal.conf
server {
    listen 80;
    server_name portal.example.com;

    include /etc/nginx/snippets/acme_challenge.conf;

    location / {
        return 301 https://$host$request_uri;
    }
}

server {
    listen 443 ssl;
    http2 on;
    server_name portal.example.com;

    ssl_certificate     /etc/letsencrypt/live/portal.example.com/fullchain.pem;
    ssl_certificate_key /etc/letsencrypt/live/portal.example.com/privkey.pem;

    ssl_protocols       TLSv1.2 TLSv1.3;
    ssl_ciphers         HIGH:!aNULL:!MD5;

    location / {
        proxy_pass http://127.0.0.1:3000;
        proxy_set_header Host $host;
    }
}
```
Reload NGINX: `nginx -t && nginx -s reload`.

### Example 3: Multi-Domain SAN (Subject Alternative Name) Certificate Setup

Issuing a single unified certificate covering multiple domains and subdomains (`example.com`, `www.example.com`, `api.example.com`).

```bash
certbot certonly --webroot \
  -w /var/www/certbot \
  -d example.com \
  -d www.example.com \
  -d api.example.com \
  --email security@example.com \
  --agree-tos \
  --non-interactive
```

```nginx
# In NGINX server block:
server {
    listen 443 ssl;
    http2 on;
    server_name example.com www.example.com api.example.com;

    # Certbot stores the bundle under the FIRST domain specified in -d
    ssl_certificate     /etc/letsencrypt/live/example.com/fullchain.pem;
    ssl_certificate_key /etc/letsencrypt/live/example.com/privkey.pem;

    location / {
        proxy_pass http://backend_pool;
    }
}
```

### Example 4: DNS-01 Challenge for Wildcard Certificates (`*.example.com`)

Wildcard certificates require DNS-01 validation. This example uses Cloudflare DNS plugin.

```bash
# 1. Install Certbot Cloudflare DNS plugin
apt-get install python3-certbot-dns-cloudflare

# 2. Configure Cloudflare API credentials (/etc/letsencrypt/cloudflare.ini)
# Permissions: Zone.DNS:Edit
cat << 'EOF' > /etc/letsencrypt/cloudflare.ini
dns_cloudflare_api_token = 0123456789abcdef0123456789abcdef01234567
EOF
chmod 600 /etc/letsencrypt/cloudflare.ini

# 3. Request Wildcard and Apex Certificate
certbot certonly \
  --dns-cloudflare \
  --dns-cloudflare-credentials /etc/letsencrypt/cloudflare.ini \
  -d example.com \
  -d "*.example.com" \
  --email admin@example.com \
  --agree-tos \
  --non-interactive
```

```nginx
# Single certificate terminates any subdomain!
server {
    listen 443 ssl;
    http2 on;
    server_name *.example.com;

    ssl_certificate     /etc/letsencrypt/live/example.com/fullchain.pem;
    ssl_certificate_key /etc/letsencrypt/live/example.com/privkey.pem;

    location / {
        proxy_pass http://wildcard_backend;
    }
}
```

### Example 5: Systemd Timer Automation with Graceful Reload Hooks

Configuring automatic zero-downtime certificate renewal via systemd timers with automated validation and NGINX reload hooks.

#### Certbot Renewal Configuration Hook:
Edit `/etc/letsencrypt/renewal-hooks/deploy/reload-nginx.sh`:
```bash
#!/bin/bash
# Automatically invoked ONLY when a certificate is successfully renewed
set -e

# Test configuration syntax before reloading
if /usr/sbin/nginx -t > /dev/null 2>&1; then
    /bin/systemctl reload nginx
    echo "[$(date -u)] NGINX reloaded successfully with renewed certificates" >> /var/log/certbot-reload.log
else
    echo "[$(date -u)] ERROR: NGINX configuration test failed! Reload aborted." >> /var/log/certbot-reload.log
    exit 1
fi
```
Make executable: `chmod +x /etc/letsencrypt/renewal-hooks/deploy/reload-nginx.sh`.

#### Verify Systemd Timer Status:
```bash
# Check that certbot timer is active and enabled
systemctl status certbot.timer

# Inspect timer trigger schedule
systemctl list-timers | grep certbot
```

---

## 6. Production Best Practices

1. **Use `--webroot` Exclusively**: Never use `--nginx` or `--standalone` in production. `--webroot` maintains complete separation of concerns: NGINX remains 100% in control of its configuration files, and Certbot merely writes static validation tokens to disk.
2. **Never Redirect the ACME Challenge**: Ensure the `location ^~ /.well-known/acme-challenge/` uses the `^~` prefix priority match. This guarantees that global HTTP-to-HTTPS redirects do not intercept or mutate token challenge requests.
3. **Always Use `fullchain.pem`**: In your NGINX `ssl_certificate` directive, always point to `fullchain.pem`, never `cert.pem`. `fullchain.pem` contains both the domain leaf certificate and the Let's Encrypt intermediate certificate.
4. **Always Test with `--dry-run`**: Let's Encrypt enforces strict rate limits (e.g., 5 failures per hour per domain, 50 duplicate certs per week). Always test new issuance commands using `--dry-run` against the Let's Encrypt Staging API before requesting production certificates.
5. **Set Proper File Permissions on Private Keys**: Ensure `/etc/letsencrypt/live/` and `/etc/letsencrypt/archive/` directories are restricted to `root:root` with `0700` and `0600` permissions. The NGINX master process reads keys as `root` before dropping privileges.
6. **Deploy Graceful Reloads via Deploy Hooks**: Never run `systemctl restart nginx` in renewal scripts. Always use `systemctl reload nginx` (or `nginx -s reload`) inside `/etc/letsencrypt/renewal-hooks/deploy/`.

---

## 7. Common Pitfalls & Anti-Patterns

| Anti-Pattern | Why It Fails | Correct Approach |
| :--- | :--- | :--- |
| **Using `ssl_certificate .../cert.pem`** | Omits the intermediate CA certificate; desktop browsers appear to work, but mobile clients and curl fail with `SEC_ERROR_UNKNOWN_ISSUER`. | Always configure `ssl_certificate /.../fullchain.pem;`. |
| **Blocking Port 80 at the Firewall** | Engineers assume HTTPS only needs port 443; the Let's Encrypt ACME server cannot reach port 80 to validate HTTP-01 challenges, causing renewal failure. | Always leave TCP port 80 open on edge firewalls to serve ACME challenges and 301 redirects. |
| **Using `certbot --nginx` in GitOps / IaC** | Certbot injects auto-generated directives directly into `/etc/nginx/conf.d/*.conf`, causing merge conflicts with Terraform, Ansible, or Git. | Use `--webroot` mode so Certbot never touches NGINX configuration files. |
| **Hitting Let's Encrypt Production Rate Limits** | Repeatedly debugging failed issuance against the production API triggers a 48-hour IP/domain block. | Always test configurations using `--dry-run` or `--test-cert` against the ACME Staging environment. |
| **Using `--standalone` for renewals** | Port 80 is bound by NGINX; Certbot fails to bind to port 80 unless NGINX is stopped, causing unplanned downtime. | Use `--webroot` to let NGINX serve the challenge token concurrently while handling traffic. |
| **Omitting deploy hook for NGINX reload** | Certbot renews certificates on disk, but NGINX keeps running with old in-memory certificates until the certificate expires and breaks the site. | Add a reload script in `/etc/letsencrypt/renewal-hooks/deploy/` to reload NGINX on successful renewal. |

---

## 8. Performance, Security & Troubleshooting

### Diagnostic CLI Commands

```bash
# 1. Test Renewal Pipeline Without Affecting Certificates (--dry-run)
certbot renew --dry-run

# 2. Inspect Expiration Dates of All Managed Certificates
certbot certificates

# 3. Test HTTP-01 Challenge File Reachability from External Machine
echo "test-token-12345" > /var/www/certbot/.well-known/acme-challenge/test-token
curl -i http://api.example.com/.well-known/acme-challenge/test-token
# Must return: HTTP/1.1 200 OK with "test-token-12345"
rm /var/www/certbot/.well-known/acme-challenge/test-token

# 4. Check Certbot Systemd Timer Execution History
journalctl -u certbot.timer
journalctl -u certbot.service --no-pager -n 50

# 5. Verify Remote Certificate Expiration Date via OpenSSL
echo | openssl s_client -connect api.example.com:443 -servername api.example.com 2>/dev/null | openssl x509 -noout -dates
```

### Rate Limits Reference (Let's Encrypt)
- **Failed Validation Limit**: 5 failures per account, per hostname, per hour.
- **Certificates per Registered Domain**: 50 certificates per week.
- **Duplicate Certificate Limit**: 5 renewals of the exact same set of hostnames per week.

---

## 9. Architectural Comparison Table

| Feature / Metric | Certbot `--webroot` | Certbot `--nginx` | Acme.sh / Shell ACME | Cloudflare Origin Certs |
| :--- | :--- | :--- | :--- | :--- |
| **NGINX Config Modification** | **None** (Untouched) | Modifies `.conf` files directly | **None** | Manual installation |
| **Downtime on Renewal** | Zero | Zero | Zero | Zero |
| **Challenge Types** | HTTP-01 | HTTP-01 | HTTP-01, DNS-01 | None (Proprietary CA) |
| **Dependencies** | Python, Certbot package | Python, Certbot package | Pure POSIX shell, curl/openssl | None |
| **GitOps / IaC Safe** | **Yes** | No (Overwrites configs) | Yes | Yes |
| **Wildcard Support** | No (Use DNS-01) | No (Use DNS-01) | Yes (via DNS APIs) | Yes |

---

## 10. Technical Interview Questions

<details>
<summary><strong>Q1: Why is the --webroot mode strongly preferred over the --nginx mode in production environments?</strong></summary>

**Answer:**
When using `certbot --nginx`, Certbot attempts to parse, tokenize, and programmatically alter your NGINX configuration files on disk to insert `listen 443 ssl` directives and rewrite location blocks. In production environments:
1. In complex multi-file configurations or environments managed by Infrastructure as Code (Terraform, Ansible, Puppet), Certbot's auto-generated edits cause merge conflicts and syntax corruption.
2. Subsequent CI/CD deployments or automated Git pulls will overwrite Certbot's changes or fail to apply.

In contrast, `certbot --webroot` treats NGINX as completely immutable. Certbot simply writes temporary challenge tokens to a standard directory on disk (`/var/www/certbot`), which NGINX serves statically. This adheres to the principle of least privilege and zero configuration interference.
</details>

<details>
<summary><strong>Q2: Why does NGINX need the ^~ modifier in location ^~ /.well-known/acme-challenge/?</strong></summary>

**Answer:**
In NGINX location matching logic, standard prefix locations (e.g., `location /`) have lower priority than regular expression locations (e.g., `location ~* \.(php|jpg|css)$`). 

If your configuration has regex locations or a catch-all redirection like `location / { return 301 https://...; }`, an ACME challenge request to `/.well-known/acme-challenge/token.txt` could inadvertently match a regex rule or trigger an unnecessary redirection. 

The `^~` modifier acts as a **priority prefix match**: if NGINX matches this prefix, it **immediately stops searching for regular expressions** and executes this location block. This guarantees ACME tokens are always served directly from the webroot.
</details>

<details>
<summary><strong>Q3: Why must port 80 remain open on edge firewalls even if a website exclusively uses HTTPS?</strong></summary>

**Answer:**
Port 80 must remain open for two fundamental reasons:
1. **HTTP-01 ACME Validation**: The Let's Encrypt validation servers verify domain control by connecting to port 80 over plain HTTP. Blocking port 80 prevents ACME certificate issuance and automated renewal, causing certificates to expire.
2. **User Experience**: Ordinary human users frequently type bare domain names (e.g., `example.com`) without the `https://` prefix into browser address bars. Browsers default to port 80 first. Port 80 must be listening to immediately return a `301 Moved Permanently` redirecting the user to `https://`.
</details>

<details>
<summary><strong>Q4: What is the difference between cert.pem, privkey.pem, chain.pem, and fullchain.pem in Let's Encrypt?</strong></summary>

**Answer:**
- `cert.pem`: The domain's leaf certificate only. It does not contain intermediate certificates. (Should *never* be used in `ssl_certificate`).
- `privkey.pem`: The private cryptographic key corresponding to the public key in the certificate. Configured in `ssl_certificate_key`.
- `chain.pem`: The intermediate CA certificate(s) issued by the Root CA (e.g., Let's Encrypt R3 intermediate). Used in `ssl_trusted_certificate` for OCSP stapling.
- `fullchain.pem`: The concatenated bundle containing `cert.pem` followed immediately by `chain.pem`. **Mandatory** for `ssl_certificate` so client browsers receive the complete chain of trust.
</details>

<details>
<summary><strong>Q5: Why can't wildcard certificates (*.example.com) be validated using the HTTP-01 challenge?</strong></summary>

**Answer:**
The HTTP-01 challenge requires placing a file at a specific IP address that resolves for the requested hostname. However, a wildcard certificate (`*.example.com`) applies to an infinite number of arbitrary subdomains (e.g., `app.example.com`, `mail.example.com`, `internal.dev.example.com`), which may resolve to completely different IP addresses, cloud load balancers, or server clusters. 

To prove authoritative ownership of an entire namespace, the ACME specification requires the **DNS-01 challenge**, which proves control over the domain's authoritative DNS zone by publishing a TXT record.
</details>

<details>
<summary><strong>Q6: How do you prevent certificate renewal from causing downtime when NGINX reloads?</strong></summary>

**Answer:**
Use the `systemctl reload nginx` (or `nginx -s reload`) command within a renewal deploy hook. 

Unlike a hard restart (`systemctl restart nginx`), a graceful reload:
1. Validates configuration syntax (`nginx -t`).
2. Instructs the master process to open the renewed certificate and private key files.
3. Spawns new worker processes loaded with the new certificates to handle incoming traffic.
4. Sends a `SIGQUIT` signal to old worker processes, allowing them to finish processing active in-flight requests and long-lived WebSockets before gracefully shutting down.
Zero client packets or connections are dropped.
</details>

<details>
<summary><strong>Q7: What is the purpose of the CAA (Certificate Authority Authorization) DNS record?</strong></summary>

**Answer:**
A CAA record (RFC 6844) is an explicit security policy published in your DNS zone that dictates which Certificate Authorities are allowed to issue certificates for your domain:
```dns
example.com. IN CAA 0 issue "letsencrypt.org"
```
Before issuing a certificate, all compliant CAs (including Let's Encrypt, DigiCert, Sectigo) are legally required to query your domain's CAA records. If a CAA record is present and does not list `letsencrypt.org`, Certbot's request will be rejected. This prevents rogue or unauthorized CAs from issuing fraudulent certificates for your infrastructure.
</details>

<details>
<summary><strong>Q8: What happens if a server has multiple virtual hosts sharing a single webroot directory for ACME challenges?</strong></summary>

**Answer:**
Sharing a single webroot directory (e.g., `/var/www/certbot`) across dozens or hundreds of virtual hosts is completely safe and considered an industry best practice. 

Because ACME challenge tokens are ephemeral, randomly generated 43-character cryptographic strings with negligible collision probability, Certbot can write challenge files for `domain-a.com` and `domain-b.com` into the same directory simultaneously. As long as every virtual host's port 80 includes the ACME snippet pointing to `/var/www/certbot`, validation succeeds across all domains without configuration duplication.
</details>

---

## 11. Hands-On Practice Exercises

<details>
<summary><strong>Exercise 1: Create a Reusable ACME Snippet and Shared Webroot Directory</strong></summary>

**Goal:** Create `/var/www/certbot`, set proper permissions, and create an NGINX snippet `/etc/nginx/snippets/acme_challenge.conf`.

**Solution:**
```bash
# 1. Setup filesystem
mkdir -p /var/www/certbot/.well-known/acme-challenge
chmod -R 755 /var/www/certbot

# 2. Create snippet
cat << 'EOF' > /etc/nginx/snippets/acme_challenge.conf
location ^~ /.well-known/acme-challenge/ {
    root /var/www/certbot;
    default_type "text/plain";
    allow all;
    expires -1;
}
EOF
```
</details>

<details>
<summary><strong>Exercise 2: Bootstrap an Initial Certificate with Certbot Dry-Run</strong></summary>

**Goal:** Test certificate issuance for `test.example.com` against the Let's Encrypt Staging API without hitting production rate limits.

**Solution:**
```bash
certbot certonly --webroot \
  -w /var/www/certbot \
  -d test.example.com \
  --dry-run \
  --email admin@example.com \
  --agree-tos \
  --non-interactive
```
Verify that Certbot outputs `The dry run was successful`.
</details>

<details>
<summary><strong>Exercise 3: Configure Automated NGINX Reload Hook</strong></summary>

**Goal:** Create a deploy hook script in `/etc/letsencrypt/renewal-hooks/deploy/` that tests configuration syntax and reloads NGINX only when certificates renew.

**Solution:**
```bash
cat << 'EOF' > /etc/letsencrypt/renewal-hooks/deploy/01-reload-nginx.sh
#!/bin/sh
nginx -t && systemctl reload nginx
EOF

chmod +x /etc/letsencrypt/renewal-hooks/deploy/01-reload-nginx.sh
```
</details>

<details>
<summary><strong>Exercise 4: Test Token Accessibility from External Host</strong></summary>

**Goal:** Manually create a mock token file and verify it is accessible over plain HTTP port 80 without triggering an HTTPS redirect.

**Solution:**
```bash
# On server:
echo "challenge-test-payload-12345" > /var/www/certbot/.well-known/acme-challenge/ping

# From client:
curl -i http://example.com/.well-known/acme-challenge/ping
# Must return: HTTP/1.1 200 OK and "challenge-test-payload-12345"
```
</details>

<details>
<summary><strong>Exercise 5: Inspect Local Certificate Expiration via OpenSSL</strong></summary>

**Goal:** Query the local certificate file directly from the command line to verify validity dates and SAN hostnames.

**Solution:**
```bash
openssl x509 -in /etc/letsencrypt/live/example.com/fullchain.pem -noout -text | grep -E "Not After|DNS:"
```
</details>

<details>
<summary><strong>Exercise 6: Diagnose a Failed HTTP-01 Validation</strong></summary>

**Goal:** Troubleshoot an ACME validation failure where Let's Encrypt reports "Connection refused" or "404 Not Found".

**Solution:**
1. Check if port 80 is listening: `ss -tulpn | grep :80`.
2. Check if the firewall is blocking port 80: `ufw status` or `iptables -L -n`.
3. Verify directory permissions: ensure user `www-data` or `nginx` can read `/var/www/certbot`.
4. Inspect NGINX error logs: `tail -f /var/log/nginx/error.log` while running `certbot renew --dry-run`.
</details>

---

## 12. Summary & Quick Reference

```bash
# Certbot Production Quick Reference

# 1. Standard Webroot Issuance
certbot certonly --webroot -w /var/www/certbot \
  -d example.com -d www.example.com \
  --email admin@example.com --agree-tos --non-interactive

# 2. Test Issuance Without Rate Limits
certbot certonly --webroot -w /var/www/certbot -d example.com --dry-run

# 3. Check All Certificates & Days Remaining
certbot certificates

# 4. Force Manual Renewal for Testing
certbot renew --force-renewal

# 5. Wildcard Certificate via Cloudflare DNS-01
certbot certonly --dns-cloudflare \
  --dns-cloudflare-credentials /etc/letsencrypt/cloudflare.ini \
  -d "*.example.com"
```

```nginx
# NGINX Snippet (/etc/nginx/snippets/acme_challenge.conf)
location ^~ /.well-known/acme-challenge/ {
    root /var/www/certbot;
    default_type "text/plain";
    allow all;
    expires -1;
}
```

---

## 13. Further Reading & Official Documentation
- [Official Certbot Documentation](https://eff-certbot.readthedocs.io/en/stable/)
- [RFC 8555: Automatic Certificate Management Environment (ACME)](https://datatracker.ietf.org/doc/html/rfc8555)
- [Let's Encrypt: Challenge Types & Best Practices](https://letsencrypt.org/docs/challenge-types/)
- [Let's Encrypt: Rate Limits Guide](https://letsencrypt.org/docs/rate-limits/)
- [RFC 6844: DNS Certification Authority Authorization (CAA) Resource Record](https://datatracker.ietf.org/doc/html/rfc6844)

---

[← Previous: Redirects and Canonical URLs](10-redirects-and-canonical-urls.md) | [Module Overview](README.md) | [Next: njs and Extensibility →](12-njs-and-extensibility.md)
