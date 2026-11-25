---
title: "Interview Prep: TLS/HTTPS Architecture, Security Hardening & WAF"
description: "Master technical interview questions on NGINX TLS 1.3 termination, cipher tuning, Perfect Forward Secrecy, HSTS, OCSP stapling, security headers, and WAF integration."
module: "08-interview"
topic: "04-tls-and-security-qa"
status: "completed"
tags: ["nginx", "interview", "tls", "https", "security", "hsts", "ocsp-stapling", "waf", "qa"]
prerequisites: ["02-core-features/03-tls-https.md", "05-security-performance/02-security-hardening.md"]
estimated_read_time: "35 min"
---

# Interview Prep: TLS/HTTPS Architecture, Security Hardening & WAF

```
+-------------------------------------------------------------------------------+
| Multi-Tiered TLS & Edge Security Defense Spectrum                             |
|                                                                               |
|  1. Cryptographic Transport Layer (TLS 1.3 / 1.2)                             |
|     - Forward Secrecy: ECDHE ciphers; session tickets disabled                |
|     - Handshake Acceleration: OCSP Stapling (ssl_stapling on)                 |
|     - Session Resumption: In-memory cache (ssl_session_cache shared:SSL:20m)  |
|                                                                               |
|  2. Protocol & Origin Shielding                                               |
|     - Catch-All Rejection: listen 443 default_server; return 444;             |
|     - Authenticated Origin Pulls: mTLS verification via CDN CA                |
|     - HSTS Preloading: max-age=63072000; includeSubDomains; preload           |
|                                                                               |
|  3. Client Security Headers & Content Policy                                 |
|     - Clickjacking defense: X-Frame-Options: DENY                             |
|     - MIME confusion defense: X-Content-Type-Options: nosniff                 |
|     - Script injection mitigation: Content-Security-Policy (CSP)              |
|                                                                               |
|  4. Layer 7 Deep Packet Inspection (WAF)                                      |
|     - ModSecurity v3 / OWASP Coraza: Anomaly scoring vs instant block         |
|     - OWASP Core Rule Set (CRS v3/v4): SQLi, XSS, RCE, LFI virtual patching   |
+-------------------------------------------------------------------------------+
```

---

## Table of Contents
1. [TLS 1.3 & Cryptographic Architecture](#1-tls-13--cryptographic-architecture)
2. [HSTS, OCSP Stapling & Handshake Optimization](#2-hsts-ocsp-stapling--handshake-optimization)
3. [Client Defense Headers & Security Policies](#3-client-defense-headers--security-policies)
4. [Web Application Firewalls & Virtual Patching](#4-web-application-firewalls--virtual-patching)
5. [Live Technical Scenarios & Incident Walkthroughs](#5-live-technical-scenarios--incident-walkthroughs)
6. [Summary & Quick Reference](#6-summary--quick-reference)

---

## 1. TLS 1.3 & Cryptographic Architecture

<details>
<summary><strong>Q1: Why should legacy protocols (SSLv3, TLS 1.0, TLS 1.1) be disabled, and what makes TLS 1.3 fundamentally more secure and faster?</strong></summary>

**Model Answer:**
- **Legacy Vulnerabilities**: SSLv3, TLS 1.0, and TLS 1.1 suffer from severe cryptographic flaws (POODLE, BEAST, CRIME) and rely on broken hashing algorithms (MD5, SHA-1) and weak cipher modes (CBC mode susceptible to padding oracle attacks). Disabling them is required for PCI-DSS compliance.
- **TLS 1.3 Advantages**:
  1. **Speed (1-RTT and 0-RTT)**: TLS 1.3 reduces the handshake from two network round-trips (2-RTT) down to a single round-trip (1-RTT). With 0-RTT session resumption, returning clients can send HTTP requests inside the initial `ClientHello`.
  2. **Security by Design**: Obsolete, insecure algorithms (RSA key transport, CBC ciphers, arbitrary Diffie-Hellman groups) are completely removed from the specification. Only modern Authenticated Encryption with Associated Data (AEAD) ciphers (AES-GCM, ChaCha20-Poly1305) are permitted.
</details>

<details>
<summary><strong>Q2: What is Perfect Forward Secrecy (PFS), and why does `ssl_session_tickets off;` help preserve it?</strong></summary>

**Model Answer:**
- **Perfect Forward Secrecy (PFS)** guarantees that if a server's long-term private key is compromised in the future, past encrypted sessions recorded by adversaries cannot be decrypted. It requires ephemeral key exchanges (like ECDHE).
- **The Session Ticket Risk**: Session tickets (RFC 5077) encrypt TLS state and store it on the client. If an NGINX server uses session tickets and the session ticket encryption key (STEK) is not rotated frequently in memory, an attacker who obtains that static ticket key can retroactively decrypt all past sessions recorded over the wire.
- Disabling session tickets (`ssl_session_tickets off;`) forces clients to use server-side session caching (`ssl_session_cache shared:SSL:20m;`), where session keys exist strictly in volatile RAM and cannot be recovered post-hoc.
</details>

<details>
<summary><strong>Q3: Explain the modern syntax change for HTTP/2 in NGINX 1.25.1+.</strong></summary>

**Model Answer:**
Prior to NGINX 1.25.1, HTTP/2 was enabled as a parameter on the `listen` directive:
```nginx
# DEPRECATED SYNTAX (Emits warning in 1.25.1+)
listen 443 ssl http2;
```
Starting in NGINX 1.25.1+, NGINX deprecated the `http2` listen parameter to cleanly decouple transport protocols (HTTP/2 vs HTTP/3 QUIC) from socket binding. The modern standard syntax is:
```nginx
# MODERN STANDARD SYNTAX
listen 443 ssl;
http2 on;
```
`http2 on;` can be declared in the `http {}` or `server {}` context.
</details>

---

## 2. HSTS, OCSP Stapling & Handshake Optimization

<details>
<summary><strong>Q4: What is HTTP Strict Transport Security (HSTS), and what are the catastrophic risks of improper HSTS preloading?</strong></summary>

**Model Answer:**
HSTS is an HTTP response header that instructs web browsers that the domain must **only** be accessed via HTTPS for a specified duration, converting all `http://` links to `https://` internally before sending packets:
```nginx
add_header Strict-Transport-Security "max-age=63072000; includeSubDomains; preload" always;
```
- **The Preload List**: Browsers (Chrome, Firefox, Safari) maintain a hardcoded "HSTS Preload List". Domains on this list enforce HTTPS out of the box on the very first connection, eliminating SSL-stripping attacks.
- **The Catastrophic Risk**: Preloading is **permanent and irreversible**. If you submit `example.com` with `includeSubDomains; preload`, every single internal subdomain (`legacy.internal.example.com`, `printer.corp.example.com`) will be permanently forced to HTTPS. If any internal subdomain lacks a valid public SSL certificate, it becomes permanently inaccessible to users globally, with zero client-side bypass capability!
</details>

<details>
<summary><strong>Q5: How does OCSP Stapling work and why does it require `ssl_trusted_certificate`?</strong></summary>

**Model Answer:**
When a browser negotiates a TLS connection, it must check if the server's certificate has been revoked:
- **Without Stapling**: The browser halts the handshake and queries the CA's Online Certificate Status Protocol (OCSP) server over the public internet, adding 50–200ms of latency. If the CA server is slow or down, the user experiences hangs.
- **With OCSP Stapling (`ssl_stapling on;`)**: NGINX periodically contacts the CA's OCSP server in the background, caches the cryptographically signed revocation response, and "staples" it into the initial TLS `ServerHello`.
- **`ssl_trusted_certificate`**: To verify that the stapled OCSP response is authentic before sending it to clients, NGINX must validate the CA's signature. `ssl_trusted_certificate` points to the intermediate CA chain bundle required for this validation.
</details>

---

## 3. Client Defense Headers & Security Policies

<details>
<summary><strong>Q6: Explain what `X-Content-Type-Options: nosniff` protects against.</strong></summary>

**Model Answer:**
When a web browser downloads a file, some browsers attempt to "sniff" the file contents to guess its MIME type rather than respecting the `Content-Type` header sent by the server.
- **The Attack**: An attacker uploads an avatar image containing executable JavaScript or HTML into a user profile directory (`avatar.jpg`). When another user views the image, an old browser sniffing the contents detects script tags and executes the image as JavaScript in the user's browser (Cross-Site Scripting).
- Setting `X-Content-Type-Options: nosniff` forces browsers to strictly adhere to the declared `Content-Type` header, disabling MIME-type sniffing completely.
</details>

<details>
<summary><strong>Q7: Compare `X-Frame-Options` and Content Security Policy (CSP) `frame-ancestors`. Why are both often declared?</strong></summary>

**Model Answer:**
Both directives defend against **Clickjacking attacks** (where an attacker embeds your website inside a transparent `<iframe>` on an external domain to trick users into clicking buttons):
- `X-Frame-Options: DENY` (or `SAMEORIGIN`) is the legacy standard. It only supports binary options (block all framing or allow same origin only); it cannot express whitelisting multiple specific partner domains.
- `Content-Security-Policy: frame-ancestors 'self' https://partner.com;` is the modern, flexible W3C standard.
- Both are declared simultaneously in production to provide defense-in-depth: modern browsers enforce CSP `frame-ancestors` (which takes precedence), while legacy browsers fall back to `X-Frame-Options`.
</details>

---

## 4. Web Application Firewalls & Virtual Patching

<details>
<summary><strong>Q8: What is Anomaly Scoring in the OWASP Core Rule Set (CRS), and why is it preferred over immediate blocking in production WAFs?</strong></summary>

**Model Answer:**
- **Immediate Blocking**: A rule immediately issues an HTTP 403 response the moment a single regex pattern matches. This results in unacceptable false positive rates: an innocent blog comment containing words like `select`, `drop`, or `union` will instantly block legitimate users.
- **Anomaly Scoring**: Each matching rule increments an internal risk counter based on severity (Critical: 5, Error: 4, Warning: 3, Notice: 2). Individual rules do not block. At the end of the request inspection phase, an evaluation rule checks if the cumulative score exceeds `inbound_anomaly_score_threshold` (default is 5).
- *Benefit*: A benign transaction triggering a single low-severity warning is allowed through, while an attack exhibiting multiple concurrent suspicious patterns is reliably intercepted.
</details>

---

## 5. Live Technical Scenarios & Incident Walkthroughs

<details>
<summary><strong>Scenario 1: Your company was hit with a Slowloris attack that exhausted all 65,000 NGINX worker connections, dropping customer traffic. Network bandwidth usage was near 0%. What happened and how do you mitigate it?</strong></summary>

**Diagnosis & Remediation:**
Slowloris opens thousands of valid TCP connections and transmits HTTP header bytes at excruciatingly slow intervals (1 byte every 10 seconds). Because connections are never closed, worker connection tables saturate while consuming virtually zero bandwidth.
*Remediation in `nginx.conf`*:
1. Enforce aggressive read timeouts:
   ```nginx
   client_header_timeout 5s;
   client_body_timeout 5s;
   send_timeout 5s;
   ```
2. Enable immediate TCP RST teardown:
   ```nginx
   reset_timedout_connection on;
   ```
3. Cap simultaneous active connections per IP:
   ```nginx
   limit_conn_zone $binary_remote_addr zone=conn_limit:20m;
   limit_conn conn_limit 20;
   ```
4. If an IP exceeds limits, return HTTP 429 and ban persistent offenders in the Linux kernel via `fail2ban` and `ipset`.
</details>

<details>
<summary><strong>Scenario 2: An attacker executes a Web Cache Poisoning attack by injecting `X-Forwarded-Host: evil.com`. Legitimate users clicking links on your site are redirected to the attacker's phishing portal. How do you prevent this?</strong></summary>

**Diagnosis & Remediation:**
The backend application was using the unvalidated `X-Forwarded-Host` or client `Host` header to generate canonical URLs, and NGINX cached the response without including the host header in its `proxy_cache_key`.
*Fix*:
1. Sanitize the Host header forwarded to the backend:
   ```nginx
   proxy_set_header Host $host; # Uses NGINX's validated host
   ```
2. Strip untrusted client forward headers before passing upstream:
   ```nginx
   proxy_set_header X-Forwarded-Host "";
   ```
3. Enforce an explicit catch-all `default_server` returning HTTP 444 so requests with unrecognized host headers are rejected before reaching cache engines.
</details>

---

## 6. Summary & Quick Reference

```
+--------------------------------------------------------------------------------+
| Security & TLS Hardening Quick Reference                                       |
|                                                                                |
| Directive                     | Mandatory Setting / Purpose                    |
| :---------------------------- | :--------------------------------------------- |
| ssl_protocols                 | TLSv1.2 TLSv1.3; (Never allow SSLv3/TLS1.0/1.1)|
| http2 on;                     | Modern HTTP/2 syntax (NGINX 1.25.1+)           |
| ssl_session_tickets off;      | Preserves Perfect Forward Secrecy              |
| ssl_stapling on;              | Accelerates TLS handshakes via OCSP stapling    |
| Strict-Transport-Security     | max-age=63072000; includeSubDomains; preload   |
| X-Content-Type-Options        | nosniff (Prevents MIME sniffing attacks)       |
| return 444;                   | Drops unauthorized host scans immediately      |
| client_header_timeout 5s;     | Defends against Slowloris socket exhaustion    |
+--------------------------------------------------------------------------------+
```

---

[← Previous Topic: 03-reverse-proxy-and-load-balancing-qa.md](03-reverse-proxy-and-load-balancing-qa.md) | [Back to Module Index](README.md) | [Next Topic: 05-performance-and-internals-qa.md →](05-performance-and-internals-qa.md)
