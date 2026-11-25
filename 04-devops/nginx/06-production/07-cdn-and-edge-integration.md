---
title: "CDN & Edge Integration: Origin Shielding, Cloaking & Cache-Control"
description: "Master engineering guide to integrating NGINX with global Content Delivery Networks (Cloudflare, AWS CloudFront, Fastly): origin cloaking, mTLS authenticated origin pulls, Cache-Control header hierarchies, and origin shielding."
module: "06-production"
topic: "07-cdn-and-edge-integration"
status: "completed"
tags: ["nginx", "cdn", "cloudflare", "cloudfront", "fastly", "origin-shielding", "cloaking", "mtls", "cache-control", "edge"]
prerequisites: ["05-security-performance/05-real-ip-and-trusted-proxies.md", "03-traffic-management/01-caching.md"]
estimated_read_time: "35 min"
---

# CDN & Edge Integration: Origin Shielding, Cloaking & Cache-Control

```
+-------------------------------------------------------------------------------+
| CDN-to-Origin Security & Caching Architecture                                 |
|                                                                               |
|  [Global End Users / Clients]                                                 |
|        │                                                                      |
|        ▼                                                                      |
|  [Edge CDN Points of Presence (PoPs: 300+ Locations)]                         |
|        │  Terminates user TLS, serves cached static assets (max-age, s-maxage) |
|        ▼                                                                      |
|  [CDN Origin Shield PoP (e.g. AWS CloudFront Origin Shield)]                  |
|        │  Deduplicates cache misses across all global PoPs into 1 request     |
|        ▼ (Mutual TLS / Shared Secret Header: X-Origin-Verify)                 |
|  [Network Firewall / Cloud Security Group]                                   |
|        │  Whitelists ONLY CDN published CIDR subnets (Blocks raw public IP)   |
|        ▼                                                                      |
|  [NGINX Origin Gateway]                                                       |
|        ├─ 1. Authenticates CDN mTLS Certificate (`ssl_verify_client on;`)     |
|        ├─ 2. Restores True Client IP (`ngx_http_realip_module`)               |
|        ├─ 3. Emits Fine-Grained `Cache-Control: s-maxage=3600, max-age=60`    |
|        └─ 4. Serves Microcached or Dynamic Backend Responses                  |
+-------------------------------------------------------------------------------+
```

---

## Table of Contents
1. [What & Why](#1-what--why)
2. [Core Concepts: Origin Cloaking & The Direct-to-Origin Threat](#2-core-concepts-origin-cloaking--the-direct-to-origin-threat)
3. [How It Works Internally](#3-how-it-works-internally)
4. [Syntax & Directives Reference](#4-syntax--directives-reference)
5. [Practical Implementation Examples](#5-practical-implementation-examples)
6. [The `Cache-Control` Hierarchy: Browser vs CDN Directives](#6-the-cache-control-hierarchy-browser-vs-cdn-directives)
7. [Mutual TLS (mTLS) Authenticated Origin Pulls](#7-mutual-tls-mtls-authenticated-origin-pulls)
8. [Common Pitfalls & Anti-Patterns](#8-common-pitfalls--anti-patterns)
9. [Architectural Comparison: Direct Hosting vs CDN-Fronted NGINX](#9-architectural-comparison-direct-hosting-vs-cdn-fronted-nginx)
10. [Technical Interview Questions & Scenarios](#10-technical-interview-questions--scenarios)
11. [Hands-On Practice Exercises](#11-hands-on-practice-exercises)
12. [Summary & Quick Reference](#12-summary--quick-reference)
13. [Further Reading](#13-further-reading)

---

## 1. What & Why

Deploying an edge Content Delivery Network (CDN) like Cloudflare, AWS CloudFront, or Fastly in front of NGINX dramatically accelerates static content delivery and mitigates volumetric DDoS attacks. However, naive CDN deployments introduce severe architectural vulnerabilities:
- **Direct-to-Origin Bypass**: If an attacker discovers the real public IP address of your origin NGINX server (via historical DNS records, SSL certificate logs, or outbound webhook callbacks), they can bypass the CDN entirely, attacking the origin with DDoS floods or exploit payloads.
- **Cache Invalidation & Stampedes**: Without **Origin Shielding**, a cache miss across 300 global CDN edge locations results in 300 simultaneous duplicate requests hammering your NGINX origin for the same asset.
- **Cache-Control Confusion**: Failing to distinguish between browser cache lifetimes (`max-age`) and CDN edge cache lifetimes (`s-maxage`) either leaks stale dynamic data to users or over-burdens origin servers with unnecessary revalidations.

Origin cloaking, mTLS authentication, and precise header controls ensure NGINX and the CDN function as a unified, impenetrable edge tier.

---

## 2. Core Concepts: Origin Cloaking & The Direct-to-Origin Threat

### Origin Cloaking
Origin cloaking is the security practice of rendering the origin web server completely inaccessible to the public internet, ensuring that **only traffic originating from the CDN's verified edge servers can establish connections**:
1. **Network Firewall / Cloud Security Groups**: In AWS EC2, GCP, or on-premise firewalls, configure ingress rules to allow TCP ports 80/443 strictly from the CDN's published IP CIDR ranges.
2. **Mutual TLS (Authenticated Origin Pulls)**: NGINX verifies that incoming TLS client connections present a valid X.509 client certificate signed exclusively by the CDN provider's private Certificate Authority (CA).
3. **Shared Secret Verification**: The CDN injects a cryptographically random, pre-shared header (e.g. `X-Origin-Verify: <secret>`) on every forwarded request, which NGINX validates before processing.

---

## 3. How It Works Internally

### Origin Shielding Mechanics

```
  Without Origin Shielding:
  Edge PoP Tokyo   (Cache Miss) ──┐
  Edge PoP London  (Cache Miss) ──┼──> [NGINX Origin] (300 simultaneous backend hits!)
  Edge PoP New York(Cache Miss) ──┘

  With Origin Shielding (e.g. CloudFront Origin Shield / Fastly Shield):
  Edge PoP Tokyo   (Miss) ──┐
  Edge PoP London  (Miss) ──┼──> [Origin Shield PoP] (Deduplicates) ──> [NGINX Origin]
  Edge PoP New York(Miss) ──┘           (1 single cache fetch!)            (1 hit!)
```

When an asset expires, instead of all 300 edge PoPs independently making backend requests to NGINX, edge PoPs forward their misses to a designated **Origin Shield PoP** located geographically close to your NGINX origin datacenter. The shield consolidates identical requests into a single origin transaction and distributes the cached response out to the edge network.

---

## 4. Syntax & Directives Reference

### Cache Control & Header Manipulation
- `add_header Cache-Control "<directives>" always;`: Emits HTTP caching directives downstream to the CDN and browser.
- `proxy_hide_header <header>;`: Strips internal or sensitive upstream headers before responses reach the CDN.
- `proxy_ignore_headers "Cache-Control" "Expires";`: Instructs NGINX's internal cache to ignore backend headers and apply local caching rules.

### Mutual TLS Directives
- `ssl_client_certificate <path_to_ca_bundle>;`: Specifies the CA certificate used to verify incoming client certificates presented by the CDN.
- `ssl_verify_client on | off | optional;`:
  - `on`: Strictly requires clients to present a valid certificate signed by `ssl_client_certificate`. Connections without a valid cert are aborted with SSL handshake failure.

---

## 5. Practical Implementation Examples

### Example 1: Enforcing Authenticated Origin Pulls (mTLS) with Cloudflare

Prevent all non-Cloudflare traffic from completing TLS handshakes at the NGINX origin:

```nginx
# /etc/nginx/conf.d/origin.conf

server {
    listen 443 ssl;
    server_name api.example.com;

    # Origin Server TLS Certificates
    ssl_certificate /etc/letsencrypt/live/api.example.com/fullchain.pem;
    ssl_certificate_key /etc/letsencrypt/live/api.example.com/privkey.pem;

    # 1. Cloudflare Authenticated Origin Pull CA Bundle
    # Downloaded from: https://developers.cloudflare.com/ssl/origin-configuration/authenticated-origin-pull/
    ssl_client_certificate /etc/nginx/certs/cloudflare-origin-pull-ca.pem;
    ssl_verify_client on;

    # 2. Restore Real Client IP from Cloudflare
    include /etc/nginx/snippets/cloudflare-realip.conf;

    location / {
        proxy_pass http://127.0.0.1:3000;
        proxy_set_header Host $host;
        proxy_set_header X-Real-IP $remote_addr;
        proxy_set_header X-Forwarded-For $proxy_add_x_forwarded_for;
    }
}
```

*Result*: Any attacker scanning your public IP with `curl https://198.51.100.25/` receives an immediate TLS error: `curl: (35) error:14094412:SSL routines:ssl3_read_bytes:sslv3 alert bad certificate`.

---

### Example 2: Shared Secret Header Validation (AWS CloudFront / Fastly)

When mTLS is not supported or when using AWS CloudFront Custom Headers:

```nginx
# /etc/nginx/conf.d/cloudfront-origin.conf

# 1. Map incoming secret header to authorized state
map $http_x_origin_verify $origin_access_authorized {
    default                             0;
    "4f8a9b2c-e1d3-4a5f-9876-1234567890ab" 1; # Cryptographically random secret
}

server {
    listen 443 ssl;
    server_name cdn.example.com;

    ssl_certificate /etc/nginx/ssl/server.crt;
    ssl_certificate_key /etc/nginx/ssl/server.key;

    # 2. Reject any request lacking the pre-shared secret header
    if ($origin_access_authorized = 0) {
        return 403 "Direct Origin Access Prohibited\n";
    }

    # 3. Restore true client IP from CloudFront
    set_real_ip_from 10.0.0.0/8; # Or AWS CloudFront CIDRs
    real_ip_header X-Forwarded-For;
    real_ip_recursive on;

    location / {
        proxy_pass http://127.0.0.1:8080;
        proxy_set_header Host $host;
        proxy_set_header X-Real-IP $remote_addr;
    }
}
```

---

## 6. The `Cache-Control` Hierarchy: Browser vs CDN Directives

To maximize CDN offload while keeping browsers responsive to updates, distinguish between browser and surrogate/CDN caching using standard HTTP directives:

```nginx
# /etc/nginx/conf.d/caching-headers.conf

server {
    server_name example.com;

    # 1. Static Immutable Assets (Fingerprinted: app.a8f9c1.js)
    # Cache in both browser and CDN for 1 year; never revalidate
    location ~* \.(?:css|js|woff2|png|jpg|webp)$ {
        add_header Cache-Control "public, max-age=31536000, immutable" always;
        try_files $uri =404;
    }

    # 2. Dynamic API Endpoints (Semi-Static: e.g. Catalog, Leaderboard)
    # Cache on CDN for 10 minutes (s-maxage=600), browser for 0 seconds (no-cache)
    # Allow CDN to serve stale content for 1 hour while updating in background!
    location /api/v1/catalog {
        proxy_pass http://backend_cluster;
        add_header Cache-Control "public, no-cache, s-maxage=600, stale-while-revalidate=3600" always;
    }

    # 3. User-Specific Sensitive Data (Profile, Checkout)
    # Strictly forbid caching at all CDN edge layers and public proxies
    location /api/v1/user/ {
        proxy_pass http://backend_cluster;
        add_header Cache-Control "private, no-store, no-cache, must-revalidate" always;
        add_header Pragma "no-cache" always;
    }
}
```

### Directives Dissection:
- `s-maxage=<seconds>`: Applies **exclusively to public shared caches** (CDNs, reverse proxies). Completely ignored by end-user web browsers!
- `max-age=<seconds>`: Dictates maximum caching lifetime for web browsers.
- `stale-while-revalidate=<seconds>`: Instructs the CDN to serve a stale cached response immediately while asynchronously fetching a fresh copy from the NGINX origin in the background, eliminating latency spikes.
- `immutable`: Tells browsers the file contents will never change as long as the filename remains the same, eliminating conditional `304 Not Modified` round-trip revalidations.

---

## 7. Mutual TLS (mTLS) Authenticated Origin Pulls

### The End-to-End Cryptographic Chain

```
  [End User]
      │
      ▼ TLS Session 1 (Public Cert, e.g. Let's Encrypt / DigiCert)
  [Cloudflare / CDN Edge]
      │
      ▼ TLS Session 2 (mTLS Connection to Origin)
      ├─ CDN presents Client Certificate signed by Cloudflare CA
      ├─ NGINX Origin verifies cert against `ssl_client_certificate`
      └─ NGINX serves content ONLY if certificate verification succeeds
```

### Implementing Per-Zone / Hostname Client Verification
In NGINX, you can inspect the client certificate subject and issuer in configuration variables:
- `$ssl_client_s_dn`: Subject Distinguished Name of client certificate.
- `$ssl_client_i_dn`: Issuer Distinguished Name.
- `$ssl_client_verify`: Returns `SUCCESS`, `FAILED`, or `NONE`.

```nginx
location / {
    if ($ssl_client_verify != "SUCCESS") {
        return 403 "Forbidden: Invalid CDN Certificate\n";
    }
    proxy_pass http://backend_pool;
}
```

---

## 8. Common Pitfalls & Anti-Patterns

| Anti-Pattern / Mistake | Root Cause | Observable Production Impact | Correct Solution |
| :--- | :--- | :--- | :--- |
| Exposing Origin IP via Outbound Emails | Sending emails directly from origin server (Postfix on origin host) | Origin IP revealed in email `Received:` headers; direct DDoS attack | Route outbound emails through SendGrid, SES, or dedicated relay |
| Using `max-age` Without `s-maxage` on APIs | Attempting to cache on CDN by setting `Cache-Control: max-age=300` | Users' browsers cache the API for 5 minutes; data updates invisible | Use `no-cache, s-maxage=300` to cache on CDN but revalidate on browser |
| Leaking SSL Certificates on Raw IP | Default server presents the customer wildcard certificate on raw IP scans | Scanners find domain name from TLS certificate SAN fields | Use dummy self-signed fallback cert on `default_server` |
| Overwriting CDN Headers in Upstream | Backend application emits `Cache-Control: private` | CDN drops cache; all traffic falls through to origin | Use `proxy_ignore_headers Cache-Control;` or tune backend app |
| Missing `Vary: Accept-Encoding` | Caching uncompressed and compressed responses under same URL | Clients lacking gzip support receive broken compressed bytes | Ensure origin emits `Vary: Accept-Encoding` on compressible assets |
| Neglecting Origin Shielding on Large Catalogs | 300+ CDN PoPs fetching misses simultaneously | Origin CPU spikes to 100% after clearing CDN cache | Enable Origin Shield in CDN settings |

---

## 9. Architectural Comparison: Direct Hosting vs CDN-Fronted NGINX

| Feature / Metric | Direct-to-Internet NGINX | CDN-Fronted NGINX with Cloaking |
| :--- | :--- | :--- |
| **Origin Protection** | Publicly exposed to direct IP scans | Cloaked via firewall + mTLS client certs |
| **DDoS Absorption Capacity**| Capped by origin datacenter bandwidth (1–10 Gbps) | 100+ Tbps global Anycast edge capacity |
| **Static Asset Latency** | High for distant geographic users (200ms+ RTT) | Ultra-low (5–20ms from nearest local PoP) |
| **Origin Bandwidth Costs** | High (100% of egress billed at host rates) | Reduced by 80–95% via edge cache offload |
| **TLS Termination** | Executed entirely on origin NGINX | Terminated at edge; optional mTLS to origin |
| **Configuration Complexity**| Low | Moderate (Requires Real-IP, mTLS, header sync) |

---

## 10. Technical Interview Questions & Scenarios

<details>
<summary><strong>1. What is the technical difference between `max-age` and `s-maxage` in the `Cache-Control` header?</strong></summary>

- `max-age=<seconds>` defines the maximum time an asset is considered fresh by **all** caches, primarily targeting private caches (such as end-user web browsers).
- `s-maxage=<seconds>` (where the 's' stands for Shared) applies **strictly to shared public caches**, such as CDNs, corporate forward proxies, and reverse proxies. When both directives are present (`Cache-Control: public, max-age=60, s-maxage=3600`), the CDN caches the asset for 1 hour, while individual user browsers cache it for only 60 seconds. This allows operators to leverage high CDN cache hit rates without locking clients into stale data.
</details>

<details>
<summary><strong>2. How can an attacker discover the real IP address of an NGINX origin hidden behind Cloudflare?</strong></summary>

1. **Historical DNS Records**: Querying DNS history archives (SecurityTrails, ViewDNS) for past A records before Cloudflare was enabled.
2. **Subdomain Leaks**: Inspecting secondary subdomains (`mail.example.com`, `direct.example.com`, `ftp.example.com`) that point directly to the origin server without Cloudflare proxying.
3. **Outbound Traffic & Webhooks**: If the origin server initiates outbound connections (e.g. sending emails via an on-host Postfix daemon, fetching user-supplied avatar URLs, or triggering webhooks), the destination server logs the origin's real IP address.
4. **Certificate Transparency Logs**: Scanning IPv4 ranges with SSL handshakes to find IP addresses presenting the target domain's TLS certificate.
</details>

<details>
<summary><strong>3. How does Authenticated Origin Pulls (mTLS) defend against origin bypass attacks?</strong></summary>

Authenticated Origin Pulls leverages Mutual TLS (mTLS). In a standard TLS handshake, only the server presents a certificate. With mTLS, NGINX is configured with `ssl_client_certificate /path/to/cdn-ca.pem` and `ssl_verify_client on;`. During the TLS handshake between the CDN and NGINX, the CDN must present an X.509 client certificate signed by its internal private CA. If an attacker discovers the origin IP and attempts to connect directly, the attacker cannot present a valid client certificate signed by the CDN CA. NGINX aborts the TLS handshake immediately, dropping the connection before any HTTP request bytes are processed.
</details>

<details>
<summary><strong>4. What is "stale-while-revalidate" and why is it beneficial for edge caching?</strong></summary>

`stale-while-revalidate=<seconds>` is an HTTP cache control extension defined in RFC 5861. When an asset's primary cache expiration (`s-maxage`) passes, the cached asset becomes stale. Normally, the next client request blocks synchronously while the CDN fetches a fresh copy from the origin. With `stale-while-revalidate`, the CDN serves the stale cached copy to the client instantly (0ms latency penalty) while initiating an asynchronous background fetch to the NGINX origin to update the cache. This eliminates latency spikes and protects the origin from cache stampedes.
</details>

<details>
<summary><strong>5. Why is `immutable` recommended for versioned static assets?</strong></summary>

When a user refreshes a web page (e.g. pressing F5), browsers typically send conditional requests (`If-Modified-Since` or `If-None-Match: <etag>`) to revalidate whether cached static files have changed, generating unnecessary HTTP 304 round-trips. Adding `immutable` to `Cache-Control` informs the browser that the file contents will never change during its valid lifetime, instructing the browser to serve the asset from local disk cache without making conditional revalidation requests over the network.
</details>

<details>
<summary><strong>6. What is the role of the `Vary: Accept-Encoding` header when NGINX is fronted by a CDN?</strong></summary>

The `Vary: Accept-Encoding` header informs intermediate CDN caches that the server produces different representations of the same URL based on the client's `Accept-Encoding` request header (e.g. `gzip`, `br`, or uncompressed). Without `Vary: Accept-Encoding`, a CDN might store a gzip-compressed response from an initial request and mistakenly serve those compressed bytes to a subsequent legacy client that does not support compression, corrupting the page.
</details>

<details>
<summary><strong>7. How do you prevent sensitive user authentication cookies from being cached by a CDN?</strong></summary>

1. Ensure all authenticated or personalized endpoints emit `Cache-Control: private, no-store, no-cache`.
2. Configure NGINX to strip `Set-Cookie` headers on public cacheable assets using `proxy_hide_header Set-Cookie;`.
3. In CDN configuration rules, enforce policies that bypass caching automatically whenever the `Cookie` or `Authorization` request headers are present.
</details>

<details>
<summary><strong>8. What is Origin Shielding and what problem does it solve?</strong></summary>

Global CDNs maintain hundreds of Points of Presence (PoPs). When an asset expires, each PoP must fetch a fresh copy independently from the origin server, resulting in hundreds of redundant backend queries. Origin Shielding designates an intermediate caching tier (a centralized "Shield PoP") close to the origin. All regional PoPs query the Shield PoP; the Shield PoP consolidates misses into a single request to NGINX, shielding the origin from cache-miss stampedes.
</details>

---

## 11. Hands-On Practice Exercises

<details>
<summary><strong>Exercise 1: Audit and Verify `Cache-Control` Directives with `curl`</strong></summary>

**Goal**: Verify that distinct paths emit appropriate caching headers.
1. Curl a static asset endpoint:
```bash
curl -sI https://example.com/static/bundle.123.js | grep -i "cache-control"
# Verify: public, max-age=31536000, immutable
```
2. Curl an API endpoint:
```bash
curl -sI https://example.com/api/v1/items | grep -i "cache-control"
# Verify: s-maxage is present and max-age is zero or no-cache
```
3. Curl an auth endpoint:
```bash
curl -sI https://example.com/api/v1/me | grep -i "cache-control"
# Verify: private, no-store
```
</details>

<details>
<summary><strong>Exercise 2: Implement Shared Secret Origin Verification</strong></summary>

**Goal**: Block direct IP access using custom headers.
1. Implement the `map $http_x_origin_verify` configuration from Example 2.
2. Curl without header: `curl -I http://localhost/` -> Verify HTTP 403 Forbidden.
3. Curl with header: `curl -I -H "X-Origin-Verify: 4f8a9b2c-e1d3-4a5f-9876-1234567890ab" http://localhost/` -> Verify HTTP 200 OK.
</details>

<details>
<summary><strong>Exercise 3: Configure Self-Signed Client Certificate Authentication</strong></summary>

**Goal**: Simulate mTLS authenticated origin pulls locally.
1. Generate CA key and certificate:
```bash
openssl req -x509 -newkey rsa:2048 -nodes -keyout ca.key -out ca.crt -subj "/CN=Test-CDN-CA"
```
2. Generate and sign a client certificate:
```bash
openssl req -newkey rsa:2048 -nodes -keyout client.key -out client.csr -subj "/CN=Test-CDN-Client"
openssl x509 -req -in client.csr -CA ca.crt -CAkey ca.key -CAcreateserial -out client.crt -days 365
```
3. Configure `ssl_client_certificate ca.crt; ssl_verify_client on;` in NGINX.
4. Test curl without client cert -> Handshake fails.
5. Test curl with client cert: `curl --cert client.crt --key client.key https://localhost/` -> Handshake succeeds!
</details>

<details>
<summary><strong>Exercise 4: Test Cloudflare Real IP Restoration</strong></summary>

**Goal**: Verify origin unwinding from simulated Cloudflare headers.
1. Include `snippets/cloudflare-realip.conf` in NGINX.
2. Simulate request from a Cloudflare IP with `CF-Connecting-IP`:
```bash
curl -I -H "CF-Connecting-IP: 203.0.113.88" http://127.0.0.1/
```
3. Verify access log records `203.0.113.88` as `$remote_addr`.
</details>

<details>
<summary><strong>Exercise 5: Verify Stripping of Sensitive Cookies on Public Assets</strong></summary>

**Goal**: Prevent backend cookies from polluting CDN cache keys.
1. Configure `location /public/ { proxy_pass http://backend; proxy_hide_header Set-Cookie; }`.
2. Configure backend to return `Set-Cookie: session=123`.
3. Curl through NGINX and confirm `Set-Cookie` header is stripped from downstream response.
</details>

<details>
<summary><strong>Exercise 6: Test `stale-while-revalidate` Timing</strong></summary>

**Goal**: Authorize background updates on cached endpoints.
1. Configure NGINX proxy caching with `proxy_cache_use_stale updating;`.
2. Add header `add_header Cache-Control "public, s-maxage=5, stale-while-revalidate=30";`.
3. Observe client responses during backend revalidation windows.
</details>

---

## 12. Summary & Quick Reference

```
+--------------------------------------------------------------------------------+
| CDN & Edge Integration Cheat Sheet                                             |
|                                                                                |
| Directive / Header            | Purpose                                        |
| :---------------------------- | :--------------------------------------------- |
| ssl_verify_client on;         | Enforces mTLS client certificate verification  |
| ssl_client_certificate <ca>;  | Whitelists CDN root/intermediate CA bundle    |
| real_ip_header CF-Connecting..| Uses Cloudflare verified origin header         |
| s-maxage=<seconds>            | Sets cache lifetime on CDN edge exclusively    |
| max-age=<seconds>             | Sets cache lifetime on user web browser        |
| stale-while-revalidate=N      | Serves stale cached data during origin fetch   |
| immutable                     | Eliminates 304 revalidation round-trips        |
| proxy_hide_header Set-Cookie; | Prevents cookie caching on public assets       |
+--------------------------------------------------------------------------------+
```

---

## 13. Further Reading

- [RFC 5861: HTTP Cache-Control Extensions for Stale Content](https://datatracker.ietf.org/doc/html/rfc5861)
- [Cloudflare: Authenticated Origin Pulls (mTLS) Documentation](https://developers.cloudflare.com/ssl/origin-configuration/authenticated-origin-pull/)
- [AWS CloudFront: Origin Shield Documentation](https://docs.aws.amazon.com/AmazonCloudFront/latest/DeveloperGuide/origin-shield.html)
- [Next Concept: NGINX vs Alternatives: Envoy, HAProxy, Traefik & Caddy](08-nginx-vs-alternatives.md)

---

[← Previous Note: 06-high-availability-and-vrrp.md](06-high-availability-and-vrrp.md) | [Back to Module Index](README.md) | [Next Note: 08-nginx-vs-alternatives.md →](08-nginx-vs-alternatives.md)
