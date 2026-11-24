# Amazon CloudFront & Global Content Delivery Networks (CDN)

> **Module**: `04-networking`  
> **Topic**: Edge Delivery, Origin Shield, Cache Policies, OAC, Edge Compute & Security  
> **Target Audience**: Cloud Architects, DevOps Engineers, Web Infrastructure Leads  
> **Last Updated**: November 2026

---

## Navigation & Contents

- [1. What & Why](#1-what--why)
- [2. Core Concepts](#2-core-concepts)
- [3. How It Works Internally](#3-how-it-works-internally)
- [4. Syntax & Usage](#4-syntax--usage)
- [5. Practical Examples](#5-practical-examples)
- [6. Best Practices](#6-best-practices)
- [7. Common Mistakes](#7-common-mistakes)
- [8. Performance, Security & Cost Notes](#8-performance-security--cost-notes)
- [9. Comparison Table](#9-comparison-table)
- [10. Interview Questions & Answers](#10-interview-questions--answers)
- [11. Practice Exercises](#11-practice-exercises)
- [12. Summary & Quick Reference](#12-summary--quick-reference)
- [13. Further Reading](#13-further-reading)

---

## 1. What & Why

When users access an application hosted in a single AWS region (e.g., `us-east-1` in Virginia) from Sydney or Frankfurt, every network packet must traverse dozens of third-party public Internet transit hops. This introduces high round-trip latency (200ms - 350ms), packet loss, and degraded throughput, alongside placing enormous HTTP connection termination and TLS handshake overhead directly on backend web servers.

**Amazon CloudFront** is a global Content Delivery Network (CDN) service that securely delivers data, videos, applications, and APIs to users with low latency and high transfer speeds. By caching content at hundreds of globally distributed Points of Presence (PoPs) connected via the AWS private global fiber backbone, CloudFront:
1. **Reduces Latency**: Serves static and dynamic content from edge locations closest to end users.
2. **Offloads Backend Infrastructure**: Absorbs sudden traffic spikes, protecting origin servers (S3, ALB, EC2, API Gateway) from overload.
3. **Enhances Security**: Provides built-in DDoS mitigation via AWS Shield Standard, web application firewall filtering with AWS WAF, and automated SSL/TLS termination with modern TLS 1.3 ciphers.

---

## 2. Core Concepts

### 2.1 Edge Locations & Regional Edge Caches (REC)
- **Edge Locations (Points of Presence - PoPs)**: 600+ edge locations worldwide that terminate TCP/TLS connections and cache frequently accessed objects.
- **Regional Edge Caches (RECs)**: Mid-tier caching layers positioned between edge PoPs and your origin. When an edge PoP experiences a cache miss, it queries its nearest REC before hitting the origin server, drastically reducing origin load.
- **Origin Shield**: An optional centralized caching tier deployed in the AWS region closest to your origin to consolidate cache misses from all global RECs into a single request stream.

### 2.2 Origins & Origin Groups
- **Amazon S3 Origins**: Static web assets, media files, and document downloads. Secured using **Origin Access Control (OAC)** to prevent direct public S3 bucket access.
- **Custom HTTP Origins**: Application Load Balancers, Amazon EC2 instances, API Gateways, or on-premises web servers.
- **Origin Groups**: Provides active-passive origin failover. If the primary origin returns HTTP 500, 502, 503, 504, or 404, CloudFront instantly re-routes the request to the secondary origin.

### 2.3 Behaviors, Cache Keys & Origin Request Policies
- **Cache Behaviors**: Path pattern rules (`/static/*`, `/api/*`, `*.jpg`) mapping request paths to origins, cache policies, and edge compute functions. Evaluated in order of precedence.
- **Cache Policy (Cache Key)**: Defines which query strings, headers, and cookies determine object uniqueness in the cache. Only items essential to response rendering should be included in the cache key to maximize cache hit ratio.
- **Origin Request Policy**: Controls which query strings, HTTP headers, and cookies are forwarded to the origin server on a cache miss, independent of the cache key.
- **Response Headers Policy**: Injects security headers (`Strict-Transport-Security`, `Content-Security-Policy`, `X-Frame-Options`) directly at the edge without modifying origin code.

### 2.4 Edge Compute: CloudFront Functions vs Lambda@Edge
- **CloudFront Functions**: Ultra-lightweight JavaScript (ECMAScript 5.1+) running inside the edge PoP at sub-millisecond execution times. Optimized for high-scale, low-latency tasks like URL rewrites/redirects, header normalization, and JWT validation.
- **Lambda@Edge**: Full Node.js and Python runtimes executing within Regional Edge Caches with access to network I/O, file system, and AWS SDK calls. Optimized for server-side rendering (SSR), dynamic origin selection, and complex authentication.

---

## 3. How It Works Internally

```
+---------------------------------------------------------------------------------------------------+
|                            AMAZON CLOUDFRONT CACHING & ORIGIN ARCHITECTURE                        |
|                                                                                                   |
|  [User in London]                                                                                 |
|         |                                                                                         |
|         v (HTTPS Request: https://example.com/images/hero.jpg)                                    |
|  +---------------------------------------------------------------------------------------------+  |
|  | EDGE LOCATION (London PoP)                                                                  |  |
|  | - TLS 1.3 Handshake terminated here                                                         |  |
|  | - AWS WAF / Shield inspection                                                               |  |
|  | - CloudFront Function (Viewer Request): Normalizes headers                                  |  |
|  | - Cache Lookup: Is /images/hero.jpg in local edge SSD cache?                                 |  |
|  +---------------------------------------------------------------------------------------------+  |
|         |                                                                                         |
|    (Cache Miss)                                                                                   |
|         v                                                                                         |
|  +---------------------------------------------------------------------------------------------+  |
|  | REGIONAL EDGE CACHE (Frankfurt REC)                                                         |  |
|  | - Larger cache storage pool                                                                 |  |
|  | - Consolidates requests from multiple European Edge PoPs                                    |  |
|  | - Cache Lookup: Is /images/hero.jpg cached here?                                            |  |
|  +---------------------------------------------------------------------------------------------+  |
|         |                                                                                         |
|    (Cache Miss)                                                                                   |
|         v                                                                                         |
|  +---------------------------------------------------------------------------------------------+  |
|  | ORIGIN SHIELD (us-east-1)                                                                   |  |
|  | - Single aggregation point directly in front of origin                                      |  |
|  +---------------------------------------------------------------------------------------------+  |
|         | (AWS Private Global Backbone Transit)                                                  |
|         v (SigV4 Signed via Origin Access Control - OAC)                                          |
|  +---------------------------------------------------------------------------------------------+  |
|  | ORIGIN S3 BUCKET (us-east-1)                                                                |  |
|  | Bucket Policy: Allow s3:GetObject ONLY if Principal is CloudFront Distribution Arn            |  |
|  +---------------------------------------------------------------------------------------------+  |
+---------------------------------------------------------------------------------------------------+
```

1. **Anycast Edge Routing**: DNS resolves `example.com` to the topologically closest CloudFront Edge location.
2. **Viewer Request Hook**: CloudFront Functions execute instantly.
3. **Tiered Cache Lookup**: Checks local Edge SSD, escalates to Regional Edge Cache, then Origin Shield.
4. **Origin Request via Private Backbone**: Traffic travels over AWS private dedicated fiber rather than the congested public Internet.
5. **Origin Access Control (OAC)**: Authenticates the CloudFront distribution to S3 via SigV4 signing, keeping the bucket entirely private.

---

## 4. Syntax & Usage

### 4.1 Creating CloudFront Origin Access Control (OAC) via AWS CLI

```bash
# 1. Create an Origin Access Control for Amazon S3
aws cloudfront create-origin-access-control \
    --origin-access-control-config '{
      "Name": "s3-production-oac",
      "Description": "OAC for production static website bucket",
      "SigningProtocol": "sigv4",
      "SigningBehavior": "always",
      "OriginAccessControlOriginType": "s3"
    }'
```

### 4.2 Invalidating Cached Content

```bash
# Invalidate specific file patterns across all global edge locations
aws cloudfront create-invalidation \
    --distribution-id "EDFDVBD632BHDS5" \
    --paths "/index.html" "/assets/*" "/css/styles-*.css"

# Check invalidation progress
aws cloudfront get-invalidation \
    --distribution-id "EDFDVBD632BHDS5" \
    --id "I2J1038DKSN382"
```

---

## 5. Practical Examples

### 5.1 Complete CloudFront Distribution with S3 Origin & OAC (Terraform)

```hcl
# S3 Origin Bucket (Completely Private)
resource "aws_s3_bucket" "frontend" {
  bucket = "enterprise-frontend-production-bucket-2026"
}

resource "aws_s3_bucket_public_access_block" "frontend" {
  bucket                  = aws_s3_bucket.frontend.id
  block_public_acls       = true
  block_public_policy     = true
  ignore_public_acls      = true
  restrict_public_buckets = true
}

# CloudFront Origin Access Control (OAC)
resource "aws_cloudfront_origin_access_control" "frontend_oac" {
  name                              = "s3-frontend-oac"
  description                       = "Origin Access Control for frontend S3"
  origin_access_control_origin_type = "s3"
  signing_behavior                  = "always"
  signing_protocol                  = "sigv4"
}

# CloudFront Distribution
resource "aws_cloudfront_distribution" "frontend" {
  enabled             = true
  is_ipv6_enabled     = true
  default_root_object = "index.html"
  price_class         = "PriceClass_100"

  origin {
    domain_name              = aws_s3_bucket.frontend.bucket_regional_domain_name
    origin_id                = "S3-Frontend-Origin"
    origin_access_control_id = aws_cloudfront_origin_access_control.frontend_oac.id
  }

  default_cache_behavior {
    allowed_methods  = ["GET", "HEAD", "OPTIONS"]
    cached_methods   = ["GET", "HEAD"]
    target_origin_id = "S3-Frontend-Origin"

    viewer_protocol_policy = "redirect-to-https"
    compress               = true

    # AWS Managed CachingOptimized Policy
    cache_policy_id = "658327ea-f89d-4fab-a63d-7e88639e58f6"
  }

  restrictions {
    geo_restriction {
      restriction_type = "none"
    }
  }

  viewer_certificate {
    cloudfront_default_certificate = true
  }
}

# S3 Bucket Policy allowing CloudFront OAC SigV4 access only
resource "aws_s3_bucket_policy" "allow_cloudfront" {
  bucket = aws_s3_bucket.frontend.id

  policy = jsonencode({
    Version = "2012-10-17"
    Statement = [
      {
        Sid       = "AllowCloudFrontServicePrincipalReadOnly"
        Effect    = "Allow"
        Principal = {
          Service = "cloudfront.amazonaws.com"
        }
        Action   = "s3:GetObject"
        Resource = "${aws_s3_bucket.frontend.arn}/*"
        Condition = {
          StringEquals = {
            "AWS:SourceArn" = aws_cloudfront_distribution.frontend.arn
          }
        }
      }
    ]
  })
}
```

### 5.2 Edge Compute: CloudFront Function for URL Rewrite (SPA Fallback)

```javascript
// CloudFront Function: Viewer Request
// Rewrites non-file URL requests to /index.html for Single Page Applications (React, Vue)
function handler(event) {
    var request = event.request;
    var uri = request.uri;

    // Check whether the URI is missing a file extension.
    if (!uri.includes('.')) {
        request.uri = '/index.html';
    }

    return request;
}
```

### 5.3 Enforcing Strict Security Headers with Response Headers Policy

```hcl
resource "aws_cloudfront_response_headers_policy" "strict_security" {
  name    = "enterprise-strict-security-headers"
  comment = "Enforce HSTS, CSP, X-Frame-Options, and X-Content-Type-Options"

  security_headers_config {
    strict_transport_security {
      access_control_max_age_sec = 31536000
      include_subdomains         = true
      preload                    = true
      override                   = true
    }
    content_type_options {
      override = true
    }
    frame_options {
      frame_option = "DENY"
      override     = true
    }
    referrer_policy {
      referrer_policy = "strict-origin-when-cross-origin"
      override        = true
    }
    xss_protection {
      mode_block  = true
      protection  = true
      override    = true
    }
  }
}
```

### 5.4 Origin Failover with Origin Groups (ALB Multi-Region Failover)

```hcl
resource "aws_cloudfront_distribution" "api_gateway" {
  enabled = true

  origin {
    domain_name = "primary-alb.us-east-1.elb.amazonaws.com"
    origin_id   = "primary-alb"
    custom_origin_config {
      http_port              = 80
      https_port             = 443
      origin_protocol_policy = "https-only"
      origin_ssl_protocols   = ["TLSv1.2"]
    }
  }

  origin {
    domain_name = "standby-alb.eu-central-1.elb.amazonaws.com"
    origin_id   = "standby-alb"
    custom_origin_config {
      http_port              = 80
      https_port             = 443
      origin_protocol_policy = "https-only"
      origin_ssl_protocols   = ["TLSv1.2"]
    }
  }

  origin_group {
    origin_id = "alb-failover-group"

    failover_criteria {
      status_codes = [500, 502, 503, 504]
    }

    member {
      origin_id = "primary-alb"
    }

    member {
      origin_id = "standby-alb"
    }
  }

  default_cache_behavior {
    target_origin_id       = "alb-failover-group"
    viewer_protocol_policy = "redirect-to-https"
    allowed_methods        = ["GET", "HEAD", "OPTIONS", "PUT", "POST", "PATCH", "DELETE"]
    cached_methods         = ["GET", "HEAD"]

    # Use CachingDisabled for API Gateway dynamic payloads
    cache_policy_id          = "4135ea2d-6df8-44a3-9df3-4b5a84be39ad"
    origin_request_policy_id = "b684b085-5fa2-47d6-9f3a-210748008635"
  }

  restrictions {
    geo_restriction {
      restriction_type = "none"
    }
  }

  viewer_certificate {
    cloudfront_default_certificate = true
  }
}
```

### 5.5 Serving Private Media Content via Signed URLs

```python
"""
Generates a CloudFront Signed URL using the cryptography library and AWS RSA Private Key.
"""
from datetime import datetime, timedelta, timezone
from cryptography.hazmat.primitives import hashes
from cryptography.hazmat.primitives.asymmetric import padding
from cryptography.hazmat.primitives.serialization import load_pem_private_key
import base64
import json

def rsa_signer(message: bytes, private_key_pem: bytes) -> bytes:
    key = load_pem_private_key(private_key_pem, password=None)
    return key.sign(message, padding.PKCS1v15(), hashes.SHA1())

def generate_signed_url(resource_url: str, key_pair_id: str, private_key_pem: bytes, expiry_minutes=60) -> str:
    expires = int((datetime.now(timezone.utc) + timedelta(minutes=expiry_minutes)).timestamp())
    policy = {
        "Statement": [
            {
                "Resource": resource_url,
                "Condition": {"DateLessThan": {"AWS:EpochTime": expires}}
            }
        ]
    }
    policy_json = json.dumps(policy, separators=(',', ':'))
    signature = rsa_signer(policy_json.encode('utf-8'), private_key_pem)
    
    b64_policy = base64.b64encode(policy_json.encode()).decode().replace('+', '-').replace('=', '_').replace('/', '~')
    b64_sig = base64.b64encode(signature).decode().replace('+', '-').replace('=', '_').replace('/', '~')
    
    return f"{resource_url}?Policy={b64_policy}&Signature={b64_sig}&Key-Pair-Id={key_pair_id}"
```

---

## 6. Best Practices

1. **Migrate from OAI to Origin Access Control (OAC)**: Origin Access Identity (OAI) is legacy. OAC supports all S3 regions, SSE-KMS encryption keys, dynamic bucket requests, and granular IAM conditions.
2. **Include Only Essential Elements in Cache Keys**: Forwarding unnecessary query parameters (e.g., `utm_source`, `session_id`) fragments the cache and lowers the Cache Hit Ratio (CHR). Strip marketing tags using CloudFront Functions.
3. **Deploy Origin Shield for Dynamic High-Traffic Origins**: Cuts origin load by up to 90% during massive traffic surges by centralizing cache misses across all regional caches.
4. **Enforce HTTPS with Modern TLS Policies**: Require `viewer_protocol_policy = "redirect-to-https"` and select `TLSv1.2_2021` minimum cipher suite to deprecate insecure TLS 1.0/1.1 protocols.
5. **Always Use Custom Domain with ACM in `us-east-1`**: SSL certificates for CloudFront distributions must be issued in `us-east-1` (N. Virginia), regardless of where your origins are located.

---

## 7. Common Mistakes

| Mistake | Root Cause | Impact | Fix |
|---|---|---|---|
| Requesting ACM Certificate in Wrong Region | ACM cert requested in `eu-west-1` instead of `us-east-1`. | Certificate does not appear in CloudFront distribution dropdown. | Issue or request ACM certificates for CloudFront strictly in `us-east-1`. |
| Forwarding `Host` Header to S3 Origin | Origin Request Policy forwards client `Host: example.com` header to S3. | S3 returns `403 Forbidden` (`SignatureDoesNotMatch`). | Never forward the viewer `Host` header to S3 origins. |
| Using OAI with SSE-KMS Encrypted S3 | Legacy OAI cannot assume IAM roles to decrypt KMS keys. | S3 denies access with `403 AccessDenied`. | Upgrade to Origin Access Control (OAC) and grant KMS decrypt permissions to CloudFront. |
| Wildcard Cache Invalidation Abuse (`/*`) | Triggering `/*` on every CI/CD deployment. | Exhausts free tier invalidations (1,000/mo) and causes origin stampedes. | Use content hashing (`bundle.a81f.js`) and invalidate only `/index.html`. |
| Over-sized Lambda@Edge Instead of CloudFront Function | Using Lambda@Edge for simple URL header rewrites. | 10x higher cost and 5ms - 15ms higher latency per request. | Use CloudFront Functions for lightweight sub-millisecond edge tasks. |
| Missing SPA Fallback Configuration | Direct navigation to `/dashboard` returns `404 NoSuchKey` from S3. | SPA routing fails on browser refresh. | Configure CloudFront Custom Error Response: map 404 to `/index.html` with HTTP 200. |

---

## 8. Performance, Security & Cost Notes

### 8.1 Performance Tuning
- **Gzip and Brotli Compression**: CloudFront automatically compresses responses when the viewer sends `Accept-Encoding: gzip, br`, reducing payload sizes by up to 80%.
- **HTTP/3 Support**: Enable HTTP/3 over QUIC on distributions to minimize connection setup times and packet loss impact over wireless cellular networks.

### 8.2 Security
- **Field-Level Encryption**: Allows viewers to securely upload sensitive credit card or PII data through CloudFront. The data is encrypted at the edge with a public key and can only be decrypted by private keys held by authorized internal backend applications.
- **AWS WAF Integration**: Attach WAF Web ACLs directly to CloudFront to block SQL injection, cross-site scripting (XSS), and automated credential stuffing bots at edge locations before reaching VPCs.

### 8.3 Cost Optimization
- **Price Classes**: Choose `PriceClass_100` (US, Canada, Europe) to save up to 40% if traffic outside these regions does not require ultra-low edge caching.
- **Free Data Transfer Out from AWS to CloudFront**: Data transfer from AWS origins (EC2, S3, ALB) into CloudFront is \$0.00/GB. You only pay for CloudFront egress to the Internet.

---

## 9. Comparison Table

| Dimension | CloudFront Functions | Lambda@Edge |
|---|---|---|
| **Runtime Environment** | Lightweight JavaScript (ES 5.1+) | Node.js, Python |
| **Execution Location** | Edge PoPs (600+ globally) | Regional Edge Caches (13 globally) |
| **Execution Triggers** | Viewer Request, Viewer Response | Viewer Request/Response, Origin Request/Response |
| **Max Execution Time** | < 1 millisecond | 5 seconds (Viewer), 30 seconds (Origin) |
| **Memory Allocation** | 2 MB (fixed) | 128 MB up to 10,000 MB |
| **Network & File System Access**| No (Isolated in-memory sandbox) | Yes (Full network access, AWS SDK, `/tmp`) |
| **Pricing** | \$0.10 per million requests | \$0.60 per million requests + duration fees |

---

## 10. Interview Questions & Answers

<details>
<summary><strong>Q1: What is the architectural difference between Origin Access Identity (OAI) and Origin Access Control (OAC)?</strong></summary>

OAI is a legacy mechanism that created a special CloudFront user identity in AWS. It had severe limitations: it did not support AWS regions launched after 2018, could not authenticate against S3 buckets encrypted with AWS KMS (SSE-KMS), and did not support HTTP `PUT`/`POST` uploads to S3. 
OAC replaces OAI by using standard AWS Signature Version 4 (SigV4) request signing. CloudFront signs each origin request with its own service principal credentials (`cloudfront.amazonaws.com`), passing the distribution ARN in the request context. This supports KMS encryption, all global regions, and dynamic upload methods.
</details>

<details>
<summary><strong>Q2: How does CloudFront handle dynamic content that cannot be cached?</strong></summary>

Even if cache TTL is set to 0 (or caching is disabled), CloudFront accelerates dynamic content significantly:
1. **TCP/TLS Termination at Edge**: The user establishes TCP and TLS handshakes with the nearest edge PoP, avoiding the high latency of multi-hop handshakes to the origin.
2. **Persistent Origin Connections**: CloudFront maintains long-lived, pre-warmed connection pools across the private AWS backbone to your origin, eliminating connection setup overhead.
3. **AWS Backbone Transit**: Dynamic payloads traverse AWS's high-speed private fiber network rather than the public Internet.
</details>

<details>
<summary><strong>Q3: Why must an ACM SSL certificate for CloudFront always be created in the us-east-1 region?</strong></summary>

CloudFront is a global service whose control plane is anchored in the `us-east-1` (N. Virginia) region. When CloudFront edge servers around the world load SSL/TLS certificates into memory, they replicate certificates exclusively from the `us-east-1` ACM repository. Certificates created in other AWS regions cannot be referenced by CloudFront distributions.
</details>

<details>
<summary><strong>Q4: How do you prevent cache stampedes (thundering herd) on your origin during cache expirations?</strong></summary>

1. **Origin Shield**: Deploy Origin Shield in the region closest to the origin. On cache misses across global RECs, requests are aggregated into a single request through Origin Shield to the origin.
2. **CloudFront Request Collapsing**: CloudFront automatically batches concurrent requests for the exact same cache key; only the first request hits the origin, and the response is served to all waiting viewer requests.
</details>

<details>
<summary><strong>Q5: What is the purpose of Field-Level Encryption in CloudFront?</strong></summary>

Field-Level Encryption provides end-to-end payload security for sensitive fields (such as credit card CVVs or social security numbers). CloudFront uses an asymmetric public key to encrypt specific JSON or form fields at the edge before forwarding the request to the origin web server. Intermediate application tiers or load balancers cannot view or log the raw sensitive data; only backend security services holding the corresponding private key can decrypt it.
</details>

<details>
<summary><strong>Q6: How can you protect your backend origin (ALB/EC2) so that it only accepts traffic from CloudFront?</strong></summary>

1. **Custom Header with WAF/ALB Rule**: CloudFront injects a secret custom header (`X-Origin-Verify: <secret-token>`) on all origin requests. The ALB or AWS WAF on the ALB rejects requests lacking this header.
2. **VPC Security Groups with Managed Prefix List**: Attach the AWS-managed prefix list `com.amazonaws.global.cloudfront.origin-facing` to your ALB's inbound security group rules, permitting ingress exclusively from CloudFront IP blocks.
</details>

<details>
<summary><strong>Q7: What is the difference between Cache Policy and Origin Request Policy?</strong></summary>

- **Cache Policy**: Governs what goes into the **Cache Key** (query parameters, cookies, headers). Items in the cache policy directly determine whether a request results in a cache hit or miss.
- **Origin Request Policy**: Dictates what data is passed upstream to the origin when a cache miss occurs. It allows you to pass headers (like `User-Agent` or `Referer`) to the origin for logging or business logic without fragmenting the cache key and hurting the Cache Hit Ratio.
</details>

<details>
<summary><strong>Q8: What happens during a CloudFront Invalidation and what is the recommended continuous delivery pattern?</strong></summary>

An invalidation instructs all 600+ edge locations to purge their cached copies of specified paths, forcing the next viewer request to fetch fresh data from the origin. 
**Recommended Pattern**: Instead of invalidating files on every deployment (which incurs costs after 1,000 paths/month), use **content hashing** or cache busting in asset filenames (e.g., `app.7b9c1d.js`). Then, set long TTLs (1 year) on assets, and only invalidate `/index.html` during CI/CD deployments.
</details>

---

## 11. Practice Exercises

<details>
<summary><strong>Exercise 1: Secure an S3 Static Website with Origin Access Control (OAC)</strong></summary>

**Goal**: Host a private website where S3 denies direct public access, but CloudFront delivers it securely.
1. Create an S3 bucket with all public access blocked.
2. Upload a simple `index.html`.
3. Create a CloudFront distribution pointing to the S3 bucket regional domain name.
4. Create an OAC and attach it to the distribution origin.
5. Apply the generated S3 bucket policy allowing `s3:GetObject` only to the CloudFront distribution ARN.
6. Verify direct S3 URL returns `403 Forbidden`, while the CloudFront domain serves `index.html`.
</details>

<details>
<summary><strong>Exercise 2: Implement Single-Page Application (SPA) Routing</strong></summary>

**Goal**: Prevent HTTP 404 errors when users refresh deep client-side routes (e.g., `/users/profile`).
1. In CloudFront Error Pages, create a custom error response.
2. Select HTTP Error Code `404: Not Found`.
3. Set Response Page Path to `/index.html` and HTTP Response Code to `200: OK`.
4. Test navigating directly to a non-existent URL and verify the SPA shell loads cleanly.
</details>

<details>
<summary><strong>Exercise 3: Deploy a CloudFront Function for Country-Based Redirection</strong></summary>

**Goal**: Redirect European users to `/eu/welcome` using viewer request headers.
1. Enable `CloudFront-Viewer-Country` header in an Origin Request / Cache Policy.
2. Write a CloudFront Function inspecting `request.headers['cloudfront-viewer-country'].value`.
3. If country is in EU, return a 302 redirect.
4. Associate the function with the Viewer Request stage and test with `curl -H "CloudFront-Viewer-Country: FR"`.
</details>

<details>
<summary><strong>Exercise 4: Measure Performance Impact of Brotli Compression</strong></summary>

**Goal**: Verify automated compression of JavaScript bundles.
1. Upload an uncompressed 2MB JavaScript bundle to S3.
2. Ensure `Compress Objects Automatically` is enabled in CloudFront cache behavior.
3. Execute `curl -I -H "Accept-Encoding: br" https://<distribution>.cloudfront.net/bundle.js`.
4. Inspect `Content-Encoding: br` and compare download size with raw S3 transfer.
</details>

<details>
<summary><strong>Exercise 5: Origin Shield Latency Benchmarking</strong></summary>

**Goal**: Compare cache miss latency with and without Origin Shield.
1. Deploy an S3 origin in `ap-southeast-2` (Sydney).
2. Enable Origin Shield in `ap-southeast-2`.
3. Send concurrent requests from European and US test instances.
4. Analyze CloudWatch metrics `OriginLatency` and `CacheHitRate` to observe origin reduction.
</details>

<details>
<summary><strong>Exercise 6: Generate CloudFront Signed URLs with Expiry</strong></summary>

**Goal**: Protect premium downloadable content.
1. Create an RSA 2048-bit key pair and upload public key to CloudFront Key Management.
2. Create a Key Group containing the public key.
3. Configure a cache behavior for `/premium/*` requiring signed URLs/cookies.
4. Run the Python signer script to generate a signed URL valid for 5 minutes.
5. Verify access succeeds before expiry and fails with `403 Forbidden` after expiry.
</details>

---

## 12. Summary & Quick Reference

```
+---------------------------------------------------------------------------------------------------+
| CLOUDFRONT CACHE HIERARCHY & COMPONENT MAP                                                        |
+--------------------------+-----------------------------------+------------------------------------+
| Component                | Primary Purpose                   | Deployment / Location              |
+--------------------------+-----------------------------------+------------------------------------+
| Edge PoP (600+)          | Connection termination, fast SSD  | Global metro cities                |
| Regional Edge Cache (13) | Large cache volume, absorbs misses| AWS Core Regions                   |
| Origin Shield            | Origin offload & aggregation      | Region closest to origin           |
| CloudFront Functions     | < 1ms lightweight request rewrite | Edge PoPs                          |
| Lambda@Edge              | Heavy compute, SSR, network I/O   | Regional Edge Caches               |
| Origin Access Control    | SigV4 S3 bucket authentication    | Global distribution control plane  |
+--------------------------+-----------------------------------+------------------------------------+
```

- **Origin Shield**: Minimizes multi-region cache stampedes on primary databases or application load balancers.
- **Cache Policy vs Origin Request Policy**: Separate cache key uniqueness from upstream payload forwarding.
- **OAC**: Mandatory modern standard replacing legacy OAI for S3 origin security.

---

## 13. Further Reading

- [Amazon CloudFront Developer Guide](https://docs.aws.amazon.com/AmazonCloudFront/latest/DeveloperGuide/)
- [Migrating from OAI to Origin Access Control (OAC)](https://aws.amazon.com/blogs/networking-and-content-delivery/amazon-cloudfront-introduces-origin-access-control-oac/)
- [CloudFront Functions vs Lambda@Edge Selection Guide](https://docs.aws.amazon.com/AmazonCloudFront/latest/DeveloperGuide/edge-functions-choosing.html)
- [AWS Managed Cache and Origin Request Policies](https://docs.aws.amazon.com/AmazonCloudFront/latest/DeveloperGuide/using-managed-cache-policies.html)
- [04-networking/05-route53-and-dns-patterns.md](file:///04-devops/aws/04-networking/05-route53-and-dns-patterns.md)
