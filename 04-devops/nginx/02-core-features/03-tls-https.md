---
title: "TLS and HTTPS Termination in NGINX"
description: "Production guide to TLS 1.3/1.2 configuration, modern cipher suites, OCSP stapling, session resumption, mutual TLS (mTLS), and zero-downtime certificate rotation in NGINX."
module: "02-core-features"
track: "devops-nginx"
order: 3
difficulty: "Intermediate to Advanced"
prerequisites:
  - "01-fundamentals/02-config-structure"
  - "01-fundamentals/03-server-and-location"
  - "02-core-features/02-reverse-proxy"
tags:
  - nginx
  - tls
  - https
  - security
  - ssl
  - mtls
  - ocsp-stapling
last_updated: "2026-03-30"
---

# TLS and HTTPS Termination in NGINX

## Table of Contents
- [1. What & Why](#1-what--why)
- [2. Core Concepts](#2-core-concepts)
- [3. How It Works Internally](#3-how-it-works-internally)
- [4. Syntax & Configuration Directives](#4-syntax--configuration-directives)
- [5. Practical Implementation Examples](#5-practical-implementation-examples)
  - [Example 1: Modern SSL Profile (TLS 1.3 + TLS 1.2 Strict)](#example-1-modern-ssl-profile-tls-13--tls-12-strict)
  - [Example 2: OCSP Stapling with Upstream DNS Resolver](#example-2-ocsp-stapling-with-upstream-dns-resolver)
  - [Example 3: Mutual TLS (mTLS) Client Certificate Authentication](#example-3-mutual-tls-mtls-client-certificate-authentication)
  - [Example 4: Multi-Domain SNI Termination (Dual RSA/ECDSA Certificates)](#example-4-multi-domain-sni-termination-dual-rsaecdsa-certificates)
  - [Example 5: SSL Session Caching, Tickets, and Zero-Downtime Cert Reload](#example-5-ssl-session-caching-tickets-and-zero-downtime-cert-reload)
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

Transport Layer Security (TLS) is the cryptographic protocol underlying HTTPS. In modern web architectures, NGINX almost universally acts as a **TLS Termination Proxy** (or SSL Termination endpoint). When acting as a termination proxy, NGINX decrypts incoming HTTPS traffic from clients on the public edge, offloads compute-heavy cryptographic handshakes, inspects and sanitizes headers, and routes decrypted or re-encrypted HTTP traffic to backend microservices.

### Why Terminate TLS at NGINX?
1. **Cryptographic Offloading**: Handshakes require asymmetric cryptography (ECDHE, RSA), which consumes significant CPU cycles. NGINX utilizes asynchronous OpenSSL engine acceleration, hardware AES-NI instructions, and worker thread affinity far more efficiently than application runtimes (Node.js, Python, Java).
2. **Centralized Certificate Management**: Rather than provisioning, renewing, and binding X.509 certificates inside dozens of backend microservices, certificates are managed in one location with automated ACME tooling (Certbot, Lego).
3. **Advanced Protocol Support**: NGINX acts as the gateway negotiating HTTP/2 and HTTP/3 (QUIC), terminating modern client protocols at the edge while speaking clean HTTP/1.1 or fast HTTP/2 to internal upstream servers.
4. **Enhanced Edge Security**: Enables HTTP Strict Transport Security (HSTS), OCSP stapling, automated certificate verification, client-side certificate authentication (mTLS), and rapid mitigation of TLS protocol vulnerabilities (e.g., revoking outdated TLS 1.0/1.1 protocols and weak ciphers).

---

## 2. Core Concepts

### TLS 1.2 vs. TLS 1.3
- **TLS 1.2 (RFC 5246)**: Requires a 2-round-trip time (2-RTT) handshake before application data can be transmitted. Supports older cipher negotiation and static key exchange algorithms (RSA key exchange is vulnerable to lack of forward secrecy).
- **TLS 1.3 (RFC 8446)**: Reduces the full handshake to 1-RTT (or 0-RTT for session resumption). Mandates Ephemeral Diffie-Hellman (ECDHE/DHE) for guaranteed Perfect Forward Secrecy (PFS), removes insecure ciphers (RC4, DES, 3DES, MD5, SHA-1), and encrypts handshake metadata to prevent eavesdropping on client identity.

### Server Name Indication (SNI)
SNI (RFC 6066) is a TLS extension where the client indicates the target hostname (`Host` header equivalent) in the initial `ClientHello` packet before encryption begins. This allows NGINX to select the correct virtual host (`server {}` block) and present the corresponding X.509 certificate on a single shared IP address.

### Certificate Chains: Leaf, Intermediate, and Root
A valid TLS setup requires the server to present a complete certificate chain:
- **Leaf Certificate**: Issued specifically for your domain (`api.example.com`).
- **Intermediate CA**: Certificates linking your leaf certificate to a trusted root authority.
- **Root CA**: Pre-installed in the operating system or browser trust store.
In NGINX, `ssl_certificate` must point to a file containing the leaf certificate followed immediately by the intermediate certificates (the full chain). Omitting intermediate certificates triggers `SEC_ERROR_UNKNOWN_ISSUER` or untrusted authority warnings on mobile devices and clean systems.

### Perfect Forward Secrecy (PFS)
PFS ensures that past session traffic cannot be decrypted even if the server's private key is compromised in the future. PFS is achieved through ephemeral key exchanges like ECDHE (Elliptic Curve Diffie-Hellman Ephemeral). TLS 1.3 enforces PFS by default; TLS 1.2 requires configuring ECDHE cipher suites.

### OCSP Stapling (RFC 6066)
The Online Certificate Status Protocol (OCSP) queries the certificate authority to check if a certificate has been revoked. Traditional client-side OCSP creates high latency and leaks user browsing habits to the CA. With **OCSP Stapling**, NGINX periodically queries the CA's OCSP responder, caches the cryptographically signed response, and attaches ("staples") it directly to the TLS `CertificateStatus` handshake message.

---

## 3. How It Works Internally

### The TLS 1.3 vs TLS 1.2 Handshake Flow

```
TLS 1.2 Handshake (2-RTT)
Client                                               NGINX Edge
  |                                                      |
  |--- ClientHello (SNI, Ciphers, ClientRandom) -------->|
  |<-- ServerHello, Certificate Chain, ServerKeyExch ----| (1-RTT)
  |<-- ServerHelloDone -----------------------------------|
  |                                                      |
  |--- ClientKeyExchange, [ChangeCipherSpec], Finished ->|
  |<-- [ChangeCipherSpec], Finished ---------------------| (2-RTT)
  |                                                      |
  |<====== Encrypted Application Data (HTTP/2 / HTTPS) =>|

TLS 1.3 Handshake (1-RTT)
Client                                               NGINX Edge
  |                                                      |
  |--- ClientHello (SNI, Supported Groups, KeyShare) ---->|
  |<-- ServerHello, KeyShare, EncryptedExtensions, ------| (1-RTT)
  |<-- Certificate, CertVerify, Finished ----------------|
  |                                                      |
  |--- Finished, Encrypted Application Data ------------>|
  |<====== Encrypted Application Data ==================>|
```

### NGINX SSL Worker Engine Architecture

```
                    +------------------------------------+
                    |        Master Process (root)       |
                    |  Reads & validates private keys    |
                    +-----------------+------------------+
                                      | fork() with shared handles
               +----------------------+----------------------+
               |                                             |
+--------------v---------------+              +--------------v---------------+
|     Worker Process 1         |              |     Worker Process 2         |
|  - epoll event loop          |              |  - epoll event loop          |
|  - OpenSSL TLS state machine |              |  - OpenSSL TLS state machine |
+--------------+---------------+              +--------------+---------------+
               \                                             /
                \                                           /
                 +-----------------------------------------+
                 | Shared Memory Zone: ssl_session_cache   |
                 | (Cache IDs, Session keys, 10MB ~ 40k)   |
                 +-----------------------------------------+
```

1. **Master Startup**: The master process reads the private keys from disk (`ssl_certificate_key`). Once opened, it drops privileges and forks worker processes.
2. **Handshake Handling**: Worker processes negotiate cryptographic parameters using OpenSSL's non-blocking I/O routines within the NGINX event loop.
3. **Session Cache Storage**: When a handshake completes, session state is stored in a shared memory slab (`ssl_session_cache shared:SSL:10m`). Any worker process handling a subsequent connection can validate the session ID without re-running the key exchange.

---

## 4. Syntax & Configuration Directives

| Directive | Context | Default | Purpose / Details |
| :--- | :--- | :--- | :--- |
| `ssl_certificate file` | `http`, `server` | None | Path to PEM-encoded certificate chain (leaf + intermediates). |
| `ssl_certificate_key file` | `http`, `server` | None | Path to matching private key file (PEM format, permissions `0600`). |
| `ssl_protocols [SSLv2] [SSLv3] [TLSv1] [TLSv1.1] [TLSv1.2] [TLSv1.3]` | `http`, `server` | `TLSv1.2 TLSv1.3` (1.23+) | Specifies enabled TLS versions. Always enforce `TLSv1.2 TLSv1.3`. |
| `ssl_ciphers ciphers` | `http`, `server` | `HIGH:!aNULL:!MD5` | Colon-delimited list of OpenSSL cipher strings for TLS 1.2. |
| `ssl_prefer_server_ciphers on\|off` | `http`, `server` | `off` (since 1.21) | When on, server dictates cipher preference over client preference. |
| `ssl_conf_command name value` | `http`, `server` | None | Direct OpenSSL configuration commands (e.g., `Options PrioritizeChaCha`). |
| `ssl_session_cache off\|none\|[builtin[:size]] [shared:name:size]` | `http`, `server` | `none` | Defines session cache. Recommended: `shared:SSL:10m` (~40,000 sessions). |
| `ssl_session_timeout time` | `http`, `server` | `5m` | Lifetime of cached TLS sessions (e.g., `1d` or `4h`). |
| `ssl_session_tickets on\|off` | `http`, `server` | `on` | Enables stateless RFC 5077 tickets. Must be managed carefully in clusters. |
| `ssl_session_ticket_key file` | `http`, `server` | None | Shared 80-byte secret key file to encrypt tickets across multiple NGINX nodes. |
| `ssl_stapling on\|off` | `http`, `server` | `off` | Enables OCSP stapling on TLS handshakes. |
| `ssl_stapling_verify on\|off` | `http`, `server` | `off` | Verifies CA signature on OCSP responses before caching. |
| `ssl_trusted_certificate file` | `http`, `server` | None | Path to root and intermediate CA bundle used to verify OCSP responder. |
| `ssl_dhparam file` | `http`, `server` | None | Path to custom Diffie-Hellman parameters (for non-ECDHE DHE ciphers). |
| `ssl_ecdh_curve curve` | `http`, `server` | `auto` | Curves for ECDHE key exchange (e.g., `prime256v1:secp384r1:X25519`). |
| `ssl_client_certificate file` | `http`, `server` | None | CA certificate bundle used to authenticate incoming client certificates in mTLS. |
| `ssl_verify_client on\|off\|optional\|optional_no_ca` | `http`, `server` | `off` | Enforces or verifies client certificates in mTLS configurations. |
| `ssl_verify_depth number` | `http`, `server` | `1` | Depth of intermediate CA verification in client certificate chains. |

---

## 5. Practical Implementation Examples

### Example 1: Modern SSL Profile (TLS 1.3 + TLS 1.2 Strict)

This configuration achieves an **A+ rating on SSL Labs**, utilizing Mozilla's Intermediate TLS profile with HSTS and secure ciphers.

```nginx
# /etc/nginx/conf.d/tls_production.conf

server {
    listen 80;
    listen [::]:80;
    server_name api.example.com;

    # Canonical HTTP to HTTPS redirection (RFC 7238 Permanent Redirect)
    location / {
        return 301 https://$host$request_uri;
    }
}

server {
    listen 443 ssl;
    listen [::]:443 ssl;
    http2 on; # NGINX 1.25.1+ syntax (use 'listen 443 ssl http2;' on <= 1.24)
    server_name api.example.com;

    # Certificate & Private Key
    ssl_certificate         /etc/ssl/certs/api.example.com/fullchain.pem;
    ssl_certificate_key     /etc/ssl/private/api.example.com/privkey.pem;

    # Supported Protocols (Disable TLS 1.0, 1.1, SSLv3)
    ssl_protocols TLSv1.2 TLSv1.3;

    # Modern Cipher Suites for TLS 1.2
    # Note: TLS 1.3 ciphersuites are configured via OpenSSL internals and are non-negotiable here
    ssl_ciphers ECDHE-ECDSA-AES128-GCM-SHA256:ECDHE-RSA-AES128-GCM-SHA256:ECDHE-ECDSA-AES256-GCM-SHA384:ECDHE-RSA-AES256-GCM-SHA384:ECDHE-ECDSA-CHACHA20-POLY1305:ECDHE-RSA-CHACHA20-POLY1305:DHE-RSA-AES128-GCM-SHA256:DHE-RSA-AES256-GCM-SHA384;
    ssl_prefer_server_ciphers off; # Allow client preference among strong ciphers

    # ECDH Curves (X25519 is preferred for speed and security)
    ssl_ecdh_curve X25519:prime256v1:secp384r1;

    # Session Resumption
    ssl_session_cache shared:SSL:20m;
    ssl_session_timeout 1d;
    ssl_session_tickets off; # Disabled unless ticket keys are distributed across cluster

    # HTTP Strict Transport Security (HSTS) - 2 Years with preload
    add_header Strict-Transport-Security "max-age=63072000; includeSubDomains; preload" always;
    add_header X-Content-Type-Options "nosniff" always;
    add_header X-Frame-Options "DENY" always;

    location / {
        proxy_pass http://127.0.0.1:3000;
        proxy_set_header Host $host;
        proxy_set_header X-Real-IP $remote_addr;
        proxy_set_header X-Forwarded-For $proxy_add_x_forwarded_for;
        proxy_set_header X-Forwarded-Proto https;
    }
}
```

### Example 2: OCSP Stapling with Upstream DNS Resolver

Stapling certificate validity directly into the handshake avoids client DNS lookups to the CA.

```nginx
# /etc/nginx/conf.d/ocsp_stapling.conf

server {
    listen 443 ssl;
    server_name secure.example.com;

    ssl_certificate         /etc/letsencrypt/live/secure.example.com/fullchain.pem;
    ssl_certificate_key     /etc/letsencrypt/live/secure.example.com/privkey.pem;

    # OCSP Stapling Directives
    ssl_stapling on;
    ssl_stapling_verify on;

    # Root + Intermediate certificates bundle for verifying OCSP response
    ssl_trusted_certificate /etc/letsencrypt/live/secure.example.com/chain.pem;

    # Valid DNS resolver required for NGINX worker to query CA OCSP URL
    resolver 1.1.1.1 8.8.8.8 valid=300s ipv6=off;
    resolver_timeout 5s;

    location / {
        root /var/www/html;
        index index.html;
    }
}
```

### Example 3: Mutual TLS (mTLS) Client Certificate Authentication

Restricts administrative or service-to-service endpoints to clients presenting a valid X.509 client certificate signed by the internal enterprise CA.

```nginx
# /etc/nginx/conf.d/mtls_api.conf

server {
    listen 443 ssl;
    server_name internal-api.corp.internal;

    # Server TLS Identity
    ssl_certificate     /etc/ssl/certs/internal_server.crt;
    ssl_certificate_key /etc/ssl/private/internal_server.key;
    ssl_protocols       TLSv1.3 TLSv1.2;

    # Client Certificate Authentication
    ssl_client_certificate /etc/ssl/ca/enterprise_root_and_intermediate_ca.crt;
    ssl_verify_client      on; # Reject connection during handshake if invalid
    ssl_verify_depth       2;  # Verify Client -> Intermediate CA -> Root CA

    # Revocation list (optional CRL)
    # ssl_crl /etc/ssl/ca/crl.pem;

    location /v1/ {
        # Forward client certificate subject DN and fingerprint to backend microservice
        proxy_pass http://backend_internal_cluster;
        proxy_set_header Host $host;
        proxy_set_header X-Client-DN $ssl_client_s_dn;
        proxy_set_header X-Client-Serial $ssl_client_serial;
        proxy_set_header X-Client-Verify $ssl_client_verify;
        proxy_set_header X-Client-Cert-Fingerprint $ssl_client_fingerprint;
    }
}
```

### Example 4: Multi-Domain SNI Termination (Dual RSA/ECDSA Certificates)

Serves high-performance ECDSA certificates to modern clients while gracefully falling back to RSA for legacy clients on the same domain.

```nginx
# /etc/nginx/conf.d/dual_cert_sni.conf

server {
    listen 443 ssl;
    server_name checkout.example.com;

    # 1. Modern ECDSA Certificate (Fast, Small Keys, Low CPU)
    ssl_certificate     /etc/ssl/certs/checkout.ecdsa.fullchain.pem;
    ssl_certificate_key /etc/ssl/private/checkout.ecdsa.key;

    # 2. Legacy RSA Fallback Certificate (Wide Compatibility)
    ssl_certificate     /etc/ssl/certs/checkout.rsa.fullchain.pem;
    ssl_certificate_key /etc/ssl/private/checkout.rsa.key;

    ssl_protocols TLSv1.2 TLSv1.3;
    ssl_ciphers ECDHE-ECDSA-AES128-GCM-SHA256:ECDHE-RSA-AES128-GCM-SHA256:ECDHE-ECDSA-AES256-GCM-SHA384:ECDHE-RSA-AES256-GCM-SHA384;

    location / {
        proxy_pass http://checkout_upstream;
    }
}
```

### Example 5: SSL Session Caching, Tickets, and Zero-Downtime Cert Reload

Configuring multi-worker session caching and automating safe zero-downtime certificate reloads via systemd signals.

```nginx
# /etc/nginx/conf.d/session_resumption.conf

# 10MB cache holds ~40,000 TLS sessions across all workers
ssl_session_cache shared:TLS_CACHE:10m;
ssl_session_timeout 4h;

# In clustered NGINX setups behind L4 load balancers (AWS NLB/Keepalived),
# generate a shared ticket key to permit session resumption across nodes:
#   openssl rand 80 > /etc/nginx/ssl_ticket.key
ssl_session_tickets on;
ssl_session_ticket_key /etc/nginx/ssl_ticket.key;

server {
    listen 443 ssl;
    server_name auth.example.com;

    ssl_certificate     /etc/letsencrypt/live/auth.example.com/fullchain.pem;
    ssl_certificate_key /etc/letsencrypt/live/auth.example.com/privkey.pem;

    location / {
        proxy_pass http://127.0.0.1:8080;
    }
}
```

#### Zero-Downtime Certificate Reload Command
When automated renewal tools (Certbot, Acme.sh) issue new certificates:
```bash
# 1. Test configuration syntax and certificate validity
nginx -t

# 2. Reload NGINX without dropping active client connections
# (Master reads new certs and spawns new workers; old workers drain active requests)
systemctl reload nginx
# Or directly via signal:
nginx -s reload
```

---

## 6. Production Best Practices

1. **Always Supply the Full Chain**: The file configured in `ssl_certificate` must contain the leaf certificate at the top, followed by all intermediate CA certificates. Never configure the private key in this file.
2. **Set File Permissions to 0600**: Private key files (`ssl_certificate_key`) should be owned by `root:root` with permissions `0600` or `0400`. The NGINX master process loads keys as `root` before spawning unprivileged workers (`nginx`/`www-data`).
3. **Disable TLS 1.0 and 1.1 Globally**: Ensure `ssl_protocols TLSv1.2 TLSv1.3;` is declared in the `http {}` context so no virtual host accidentally allows deprecated protocol versions.
4. **Use Shared Session Caches**: Always use `ssl_session_cache shared:NAME:size;` rather than `builtin`. The `builtin` cache is local to an individual worker process, causing high cache misses when epoll distributes requests across workers.
5. **Always Configure a Resolver for OCSP**: OCSP stapling (`ssl_stapling on;`) silently fails if NGINX cannot resolve the CA's OCSP URL. Always set a fast internal or public DNS resolver (`resolver 1.1.1.1 8.8.8.8 valid=300s;`).
6. **Include HSTS with Caution**: The `Strict-Transport-Security` header instructs browsers to never connect over HTTP. Start with a short `max-age` (e.g., `max-age=300`) during initial rollout before committing to `max-age=63072000; includeSubDomains; preload`.

---

## 7. Common Pitfalls & Anti-Patterns

| Anti-Pattern | Why It Fails | Correct Approach |
| :--- | :--- | :--- |
| **Missing Intermediate Cert in `ssl_certificate`** | Desktop browsers cache CA certs and appear to work, but mobile browsers, curl, and automated microservices fail with `UNTRUSTED_ISSUER`. | Concatenate leaf and intermediate: `cat domain.crt intermediate.crt > fullchain.pem` and point `ssl_certificate` to `fullchain.pem`. |
| **`ssl_stapling on;` without `resolver`** | NGINX worker fails to resolve the OCSP responder hostname; the OCSP response is not cached or stapled. | Specify `resolver 1.1.1.1 8.8.8.8 valid=300s;` in the `http` or `server` context. |
| **Permissive `ssl_protocols TLSv1 TLSv1.1`** | Fails PCI-DSS compliance checks and exposes clients to BEAST and POODLE downgrade attacks. | Restrict strictly to `ssl_protocols TLSv1.2 TLSv1.3;`. |
| **Using `listen 443;` without `ssl` parameter** | NGINX attempts to parse incoming TLS client hello packets as plain HTTP, returning `400 Bad Request: The plain HTTP request was sent to HTTPS port`. | Ensure `listen 443 ssl;` is explicitly configured. |
| **Session Tickets enabled in multi-node clusters without shared key** | Client renegotiates session with Node B using a ticket issued by Node A. Node B cannot decrypt the ticket, causing full handshake fallback latency. | Generate and synchronize a shared `ssl_session_ticket_key` across all cluster nodes, or disable tickets and rely on shared caches with IP-hash persistence. |
| **Restarting NGINX (`systemctl restart`) to rotate certs** | Hard restarts terminate existing client connections in flight, dropping active file uploads or long-lived WebSockets. | Use graceful reload: `nginx -t && nginx -s reload`. |

---

## 8. Performance, Security & Troubleshooting

### Diagnostic CLI Commands

```bash
# 1. Verify TLS Handshake, Negotiated Protocol, and Certificate Chain
openssl s_client -connect api.example.com:443 -servername api.example.com -tlsextdebug -status

# 2. Verify OCSP Stapling Status specifically (Look for 'OCSP Response Status: successful')
openssl s_client -connect api.example.com:443 -servername api.example.com -status < /dev/null 2>&1 | grep -A 17 "OCSP response"

# 3. Test for Deprecated TLS 1.0 / 1.1 (Must fail / be refused)
openssl s_client -connect api.example.com:443 -tls1_1

# 4. Inspect Expiration Date of Remote Certificate
echo | openssl s_client -connect api.example.com:443 -servername api.example.com 2>/dev/null | openssl x509 -noout -dates

# 5. Check Local Key and Certificate Match (MD5 hashes must be identical)
openssl x509 -noout -modulus -in /etc/ssl/certs/fullchain.pem | openssl md5
openssl rsa  -noout -modulus -in /etc/ssl/private/privkey.pem  | openssl md5
```

### Performance Tuning Matrix
- **ECDSA vs. RSA**: ECDSA with curve P-256 provides equivalent cryptographic strength to a 3072-bit RSA key, but processes handshakes up to 3x faster with 75% smaller public key transmission size.
- **Session Cache Sizing**: 1 megabyte holds approximately 4,000 TLS sessions. Setting `ssl_session_cache shared:SSL:20m;` reserves 20MB of RAM and holds ~80,000 sessions, reducing CPU handshake overhead by over 80% on repeat client visits.

---

## 9. Architectural Comparison Table

| Feature / Metric | TLS 1.2 | TLS 1.3 | mTLS (Mutual TLS) |
| :--- | :--- | :--- | :--- |
| **Handshake Latency** | 2-RTT (two network round-trips) | 1-RTT (one round-trip) | 1-RTT (TLS 1.3) or 2-RTT (TLS 1.2) + client cert verify |
| **Client Authentication** | Server authenticated only | Server authenticated only | Bi-directional (Server & Client authenticate) |
| **Forward Secrecy (PFS)** | Optional (depends on cipher suite) | Mandatory (ECDHE enforced) | Mandatory (under TLS 1.3) |
| **Vulnerable to Downgrade** | Susceptible if weak ciphers allowed | Cryptographically protected against downgrade | Protected |
| **Typical Use Case** | Legacy client compatibility | Public Internet HTTPS termination | Zero-Trust internal APIs, B2B gateway integrations |
| **CPU Overhead** | Medium (asymmetric key negotiation) | Low (optimized curve mathematics) | High (double certificate chain validation) |

---

## 10. Technical Interview Questions

<details>
<summary><strong>Q1: Why does NGINX require the full certificate chain in the ssl_certificate directive, and what happens if intermediate certificates are omitted?</strong></summary>

**Answer:**
Web browsers trust a pre-installed set of Root Certificate Authorities (Root CAs). Commercial CAs do not sign domain certificates directly with their root keys; instead, they use intermediate signing keys. When a client connects, it must build a cryptographic "chain of trust" from the domain's leaf certificate up to a trusted root in its store. 

If intermediate certificates are omitted from `ssl_certificate`, NGINX only sends the leaf certificate. While some desktop browsers may attempt to fetch missing intermediates via AIA (Authority Information Access) or use cached certificates, mobile browsers, command-line clients (`curl`), and automated APIs will immediately abort the TLS handshake with `SEC_ERROR_UNKNOWN_ISSUER` or `SSL certificate problem: unable to get local issuer certificate`.
</details>

<details>
<summary><strong>Q2: How does Server Name Indication (SNI) work, and how does NGINX route HTTPS requests before decrypting HTTP headers?</strong></summary>

**Answer:**
In standard TLS, the HTTP `Host` header cannot be read until the TLS handshake completes and encryption is established. However, NGINX needs to know which certificate to present *during* the handshake. 

SNI solves this by having the client include the desired server hostname as a plain-text extension in the initial `ClientHello` packet. The NGINX master/worker parses this SNI field, matches it against all configured `server_name` directives listening on that IP/port, selects the matching `server {}` block, and presents that block's `ssl_certificate`.
</details>

<details>
<summary><strong>Q3: What is OCSP Stapling, and why is it preferred over standard client OCSP validation?</strong></summary>

**Answer:**
In standard OCSP, when a client connects to a server, it must pause the TLS handshake and make an independent HTTP request to the CA's OCSP responder to verify that the certificate has not been revoked. This introduces significant latency (often 100-300ms) and compromises client privacy by disclosing browsing activity to the CA.

With **OCSP Stapling** (`ssl_stapling on;`), NGINX periodically queries the CA's OCSP responder in the background, validates the CA's digital signature on the status response (`ssl_stapling_verify on;`), and caches the result. During subsequent client TLS handshakes, NGINX attaches ("staples") this signed timestamped revocation status directly into the `CertificateStatus` message, eliminating client-side latency.
</details>

<details>
<summary><strong>Q4: Explain the difference between TLS Session IDs and TLS Session Tickets.</strong></summary>

**Answer:**
Both mechanisms allow returning clients to resume an existing TLS session without performing an expensive full asymmetric cryptographic handshake:
- **Session ID (RFC 5246)**: State is stored on the server inside a memory cache (`ssl_session_cache shared:SSL:10m`). The server sends a 32-byte Session ID to the client. Upon reconnection, the client presents the ID; if found in server memory, the session is resumed. Limitation: Does not scale across multiple NGINX servers without sticky sessions or complex cache synchronization.
- **Session Tickets (RFC 5077 / TLS 1.3)**: State is stored on the client. The server encrypts the session state using a secret ticket key (`ssl_session_ticket_key`) and gives it to the client. On reconnection, the client sends the encrypted ticket back; the server decrypts it and resumes the session. This is completely stateless on the server and works seamlessly across multi-node clusters that share the same ticket key.
</details>

<details>
<summary><strong>Q5: What are the risks of using ssl_session_tickets on without a defined ssl_session_ticket_key in a clustered environment?</strong></summary>

**Answer:**
When `ssl_session_tickets on;` is set without specifying an explicit key file (`ssl_session_ticket_key`), NGINX generates a random ticket encryption key in memory when the master process starts. 

In a clustered environment (e.g., two NGINX servers behind an AWS NLB):
1. Node A and Node B will have completely different, randomly generated in-memory ticket keys.
2. If a client connects to Node A and receives a ticket, and subsequent requests hit Node B, Node B will fail to decrypt the ticket.
3. Node B will fall back to a full 1-RTT or 2-RTT TLS handshake, negating the performance benefits of resumption and causing erratic latency spikes. The solution is to generate a shared 80-byte key file and distribute it securely to all cluster instances.
</details>

<details>
<summary><strong>Q6: How does NGINX handle zero-downtime certificate rotation without dropping client connections?</strong></summary>

**Answer:**
When certificates are updated on disk, issuing `nginx -s reload` (or `systemctl reload nginx`) triggers the master process to re-parse the configuration and open new certificate/key files. 

The master process then spawns a new generation of worker processes loaded with the new certificates to handle all new incoming connections. Concurrently, the master sends a graceful shutdown signal (`SIGQUIT`) to the old worker processes. Old workers stop accepting new connections on the listen sockets but remain running until all active, in-flight HTTPS requests and persistent WebSockets finish processing before cleanly exiting.
</details>

<details>
<summary><strong>Q7: How do you pass the client's verified identity to a backend upstream service when using mTLS?</strong></summary>

**Answer:**
In an mTLS architecture, NGINX verifies the client certificate against `ssl_client_certificate`. To inform backend microservices of the authenticated client identity, NGINX exposes internal variables that are forwarded via HTTP proxy headers:
```nginx
proxy_set_header X-Client-DN        $ssl_client_s_dn;       # Subject Distinguished Name (e.g., CN=service-a)
proxy_set_header X-Client-Verify    $ssl_client_verify;     # 'SUCCESS', 'FAILED', or 'NONE'
proxy_set_header X-Client-Serial    $ssl_client_serial;     # Serial number of the certificate
proxy_set_header X-Client-Fingerprint $ssl_client_fingerprint; # SHA-1 or SHA-256 fingerprint
```
Backend applications verify that `X-Client-Verify` is `SUCCESS` and parse `X-Client-DN` to enforce role-based access control.
</details>

<details>
<summary><strong>Q8: What is the security implication of setting ssl_prefer_server_ciphers on vs off with modern TLS 1.3?</strong></summary>

**Answer:**
Historically, `ssl_prefer_server_ciphers on;` was considered best practice because older clients supported weak ciphers (e.g., RC4 or 3DES) and the server needed to enforce the strongest available cipher. 

However, in modern configurations (TLS 1.2 + TLS 1.3 with a strictly hardened server cipher suite), setting `ssl_prefer_server_ciphers off;` is recommended by Mozilla. Modern mobile devices (smartphones, IoT devices) often lack hardware acceleration for AES-GCM and prefer ChaCha20-Poly1305 to conserve CPU and battery life, whereas server CPUs have AES-NI hardware instructions. With `ssl_prefer_server_ciphers off;`, the client can choose between AES-GCM and ChaCha20 (both of which are equally secure) based on its hardware acceleration capabilities.
</details>

---

## 11. Hands-On Practice Exercises

<details>
<summary><strong>Exercise 1: Generate Self-Signed Certificates and Configure an HTTPS Virtual Host</strong></summary>

**Goal:** Create a 2048-bit RSA self-signed certificate with an SAN (Subject Alternative Name) extension and configure NGINX to serve HTTPS locally on port 8443.

**Solution:**
```bash
# 1. Generate certificate and private key in one command
openssl req -x509 -nodes -days 365 -newkey rsa:2048 \
  -keyout /tmp/selfsigned.key \
  -out /tmp/selfsigned.crt \
  -subj "/CN=localhost" \
  -addext "subjectAltName=DNS:localhost,IP:127.0.0.1"
```

```nginx
# /etc/nginx/conf.d/exercise1.conf
server {
    listen 8443 ssl;
    server_name localhost;

    ssl_certificate     /tmp/selfsigned.crt;
    ssl_certificate_key /tmp/selfsigned.key;
    ssl_protocols       TLSv1.2 TLSv1.3;

    location / {
        return 200 "HTTPS self-signed handshake successful!\n";
    }
}
```

Verify with:
```bash
curl -k https://localhost:8443
```
</details>

<details>
<summary><strong>Exercise 2: Implement HSTS and Force HTTP to HTTPS Redirection</strong></summary>

**Goal:** Build a dual-server configuration where port 80 permanently redirects all requests to port 443 with query parameters preserved, and configure strict HSTS headers.

**Solution:**
```nginx
server {
    listen 80;
    server_name portal.example.com;
    return 301 https://$host$request_uri;
}

server {
    listen 443 ssl;
    server_name portal.example.com;

    ssl_certificate     /etc/ssl/certs/portal.crt;
    ssl_certificate_key /etc/ssl/private/portal.key;
    ssl_protocols       TLSv1.2 TLSv1.3;

    add_header Strict-Transport-Security "max-age=31536000; includeSubDomains" always;

    location / {
        return 200 "Welcome to secure portal\n";
    }
}
```
</details>

<details>
<summary><strong>Exercise 3: Test and Verify SSL Session Resumption with Session IDs</strong></summary>

**Goal:** Configure a 10MB shared session cache and test with `openssl s_client` that the second connection reuses the established Session-ID without a full handshake.

**Solution:**
```nginx
# Add inside http {} or server {} block:
ssl_session_cache shared:SSL_TEST:10m;
ssl_session_timeout 10m;
ssl_session_tickets off; # Force session ID testing
```

Verify using OpenSSL:
```bash
# First connection: save session state to a file
openssl s_client -connect localhost:8443 -reconnect -no_ticket
# Observe the 'Reused, TLSv1.2, Cipher is ...' output in the terminal
```
</details>

<details>
<summary><strong>Exercise 4: Configure Dual ECDSA and RSA Certificates</strong></summary>

**Goal:** Configure an NGINX virtual host to present both ECDSA and RSA certificates to support both modern and legacy clients.

**Solution:**
```nginx
server {
    listen 443 ssl;
    server_name multi.example.com;

    # Primary: Fast ECDSA
    ssl_certificate     /etc/ssl/certs/multi_ecdsa.crt;
    ssl_certificate_key /etc/ssl/private/multi_ecdsa.key;

    # Fallback: RSA
    ssl_certificate     /etc/ssl/certs/multi_rsa.crt;
    ssl_certificate_key /etc/ssl/private/multi_rsa.key;

    ssl_protocols TLSv1.2 TLSv1.3;
    ssl_ciphers ECDHE-ECDSA-AES128-GCM-SHA256:ECDHE-RSA-AES128-GCM-SHA256:HIGH:!aNULL:!MD5;

    location / {
        return 200 "Dual certificate served!\n";
    }
}
```
</details>

<details>
<summary><strong>Exercise 5: Build a Mutual TLS (mTLS) Protected Endpoint</strong></summary>

**Goal:** Create a self-signed Root CA, issue a client certificate, and configure an NGINX location `/admin` that requires a valid client certificate, rejecting unauthenticated clients with HTTP 403.

**Solution:**
```bash
# 1. Create CA
openssl req -x509 -nodes -days 365 -newkey rsa:2048 -keyout ca.key -out ca.crt -subj "/CN=InternalCA"

# 2. Create Client Cert
openssl req -nodes -newkey rsa:2048 -keyout client.key -out client.csr -subj "/CN=admin-user"
openssl x509 -req -in client.csr -CA ca.crt -CAkey ca.key -CAcreateserial -out client.crt -days 365
```

```nginx
server {
    listen 443 ssl;
    server_name admin.example.com;

    ssl_certificate     /etc/ssl/certs/server.crt;
    ssl_certificate_key /etc/ssl/private/server.key;

    # mTLS Directives
    ssl_client_certificate /path/to/ca.crt;
    ssl_verify_client      optional; # Allow connection, enforce in location

    location /admin {
        if ($ssl_client_verify != SUCCESS) {
            return 403 "Forbidden: Valid Client Certificate Required\n";
        }
        return 200 "Hello Authenticated Admin: $ssl_client_s_dn\n";
    }
}
```
Test with and without certificate:
```bash
# Unauthorized:
curl -k https://admin.example.com/admin
# Authorized:
curl -k --cert client.crt --key client.key https://admin.example.com/admin
```
</details>

<details>
<summary><strong>Exercise 6: Diagnose and Fix a Broken Certificate Chain</strong></summary>

**Goal:** Simulate a broken intermediate certificate chain error and repair the bundle using OpenSSL CLI tools.

**Solution:**
1. Given separate files `domain.crt`, `intermediate.crt`, and `root.crt`.
2. Inspect the chain using OpenSSL:
   ```bash
   openssl verify -CAfile root.crt -untrusted intermediate.crt domain.crt
   ```
3. Assemble the proper NGINX bundle (leaf must be first, followed by intermediate, never root):
   ```bash
   cat domain.crt intermediate.crt > /etc/ssl/certs/bundle_fixed.crt
   ```
4. Update `ssl_certificate /etc/ssl/certs/bundle_fixed.crt;` and run `nginx -t && nginx -s reload`.
</details>

---

## 12. Summary & Quick Reference

```nginx
# Gold Standard NGINX TLS 1.3 / 1.2 Template
server {
    listen 443 ssl;
    listen [::]:443 ssl;
    http2 on;
    server_name example.com;

    # Certificates
    ssl_certificate         /etc/ssl/certs/example.com.fullchain.pem;
    ssl_certificate_key     /etc/ssl/private/example.com.privkey.pem;

    # Protocols & Ciphers
    ssl_protocols           TLSv1.2 TLSv1.3;
    ssl_ciphers             ECDHE-ECDSA-AES128-GCM-SHA256:ECDHE-RSA-AES128-GCM-SHA256:ECDHE-ECDSA-AES256-GCM-SHA384:ECDHE-RSA-AES256-GCM-SHA384:ECDHE-ECDSA-CHACHA20-POLY1305:ECDHE-RSA-CHACHA20-POLY1305;
    ssl_prefer_server_ciphers off;

    # Session Resumption
    ssl_session_cache       shared:SSL:20m;
    ssl_session_timeout     1d;
    ssl_session_tickets     off;

    # OCSP Stapling
    ssl_stapling            on;
    ssl_stapling_verify     on;
    ssl_trusted_certificate /etc/ssl/certs/example.com.chain.pem;
    resolver                1.1.1.1 8.8.8.8 valid=300s;

    # Security Headers
    add_header Strict-Transport-Security "max-age=63072000; includeSubDomains; preload" always;
    add_header X-Content-Type-Options "nosniff" always;
}
```

---

## 13. Further Reading & Official Documentation
- [Official NGINX ngx_http_ssl_module Documentation](https://nginx.org/en/docs/http/ngx_http_ssl_module.html)
- [Mozilla SSL Configuration Generator (Server Side TLS)](https://ssl-config.mozilla.org/)
- [RFC 8446: The Transport Layer Security (TLS) Protocol Version 1.3](https://datatracker.ietf.org/doc/html/rfc8446)
- [RFC 6066: TLS Extensions: Extension Definitions (SNI & OCSP Stapling)](https://datatracker.ietf.org/doc/html/rfc6066)
- [Qualys SSL Labs SSL Server Test](https://www.ssllabs.com/ssltest/)

---

[← Previous: Reverse Proxying & Upstreams](02-reverse-proxy.md) | [Module Overview](README.md) | [Next: Variables and Rewrites →](04-variables-and-rewrites.md)
