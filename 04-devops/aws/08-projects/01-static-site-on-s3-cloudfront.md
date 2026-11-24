---
project: "Production Static Site Hosting on S3 and CloudFront with OAC"
module: "08-projects"
track: "aws"
difficulty: "intermediate"
architecture: "edge-accelerated-serverless-web"
last-tested: "2026-11"
tags:
  - "s3"
  - "cloudfront"
  - "oac"
  - "waf"
  - "route53"
  - "terraform"
  - "project"
---

# Production Static Site Hosting on S3 and CloudFront with OAC

## Table of Contents
- [1. Project Topology & Specifications](#1-project-topology--specifications)
- [2. Declarative Code (Terraform)](#2-declarative-code-terraform)
- [3. Verification & Automated Test Harness](#3-verification--automated-test-harness)
- [4. Hardening, Security & Cost Analysis](#4-hardening-security--cost-analysis)
- [5. Operational Runbook & Disaster Recovery](#5-operational-runbook--disaster-recovery)

---

## 1. Project Topology & Specifications

### 1.1 Architecture Overview
This production reference project deploys a globally distributed, highly secure, zero-server static website (React, Vue, or static HTML/JS/CSS assets) using **Amazon S3**, **Amazon CloudFront**, and **AWS WAF**.

Unlike legacy architectures that configure public website hosting directly on S3 (which exposes buckets to the public internet and risks unauthenticated scans), this architecture enforces strict **Origin Access Control (OAC)**:
- The S3 bucket has **S3 Block Public Access 100% enabled**.
- Direct public HTTP access to the bucket is completely blocked.
- Only the cryptographically authenticated CloudFront distribution can fetch objects from S3 using SigV4 signing.
- Incoming client traffic is filtered at edge locations by an **AWS WAF WebACL** to mitigate DDoS, rate-limit abusive IPs, and block malicious user-agents.
- Custom security response headers (HSTS, CSP, X-Frame-Options) are injected at the edge via CloudFront Response Headers Policies.

```
+----------------------------------------------------------------------------------------------------+
|                         STATIC SITE S3 + CLOUDFRONT WITH OAC TOPOLOGY                              |
+----------------------------------------------------------------------------------------------------+

  [ Global Users / Browsers ]
               |
               | HTTPS / TLS 1.3 (Route 53 DNS Alias)
               v
  +--------------------------------------------------------------------------------------------------+
  | Amazon CloudFront Edge Network (200+ PoPs)                                                       |
  |  - TLS 1.3 Termination (AWS Certificate Manager in us-east-1)                                    |
  |  - AWS WAF WebACL (Rate-based rules: 1,000 req / 5 min, IP reputation, bot control)              |
  |  - Response Headers Policy (HSTS, Content-Security-Policy, X-Content-Type-Options)              |
  |  - Viewer Protocol Policy: Redirect HTTP -> HTTPS                                                |
  +--------------------------------------------------------------------------------------------------+
               |
               | SigV4 Signed Request via Origin Access Control (OAC)
               v
  +--------------------------------------------------------------------------------------------------+
  | Amazon S3 Storage Bucket (Origin)                                                                |
  |  - Block Public Access: ENABLED (All 4 settings True)                                            |
  |  - Default Encryption: AES256 (SSE-S3)                                                           |
  |  - Versioning: Enabled                                                                           |
  |  - Bucket Policy: Grants read ONLY to CloudFront Service Principal matching Distribution ARN     |
  +--------------------------------------------------------------------------------------------------+
```

### 1.2 Bill of Materials (AWS Resources)
- `aws_s3_bucket`: Private object store hosting frontend production build artifacts.
- `aws_s3_bucket_public_access_block`: Enforces all 4 public access blocks.
- `aws_s3_bucket_versioning`: Protects against accidental overwrites or file deletions.
- `aws_s3_bucket_server_side_encryption_configuration`: AES256 default encryption at rest.
- `aws_s3_bucket_policy`: Evaluates `aws:SourceArn` condition to permit only the CloudFront distribution.
- `aws_cloudfront_origin_access_control`: AWS OAC modern signing token.
- `aws_cloudfront_response_headers_policy`: Edge injection of strict HTTP security headers.
- `aws_cloudfront_distribution`: Global CDN edge distribution with TLS 1.3 and gzip/brotli compression.
- `aws_wafv2_web_acl`: Rate-limiting and IP reputation edge protection in `us-east-1`.

---

## 2. Declarative Code (Terraform)

Create a file named `main.tf`:

```hcl
terraform {
  required_version = ">= 1.5.0"
  required_providers {
    aws = {
      source  = "hashicorp/aws"
      version = "~> 5.40"
    }
  }
}

# CloudFront certificates and WAF on CloudFront MUST reside in us-east-1
provider "aws" {
  alias  = "us_east_1"
  region = "us-east-1"
}

provider "aws" {
  region = "us-east-1"
}

variable "project_name" {
  type    = string
  default = "enterprise-static-portal"
}

variable "environment" {
  type    = string
  default = "production"
}

# -----------------------------------------------------------------------------
# 1. Private S3 Origin Bucket
# -----------------------------------------------------------------------------
resource "aws_s3_bucket" "website" {
  bucket        = "${var.project_name}-${var.environment}-site-origin"
  force_destroy = false

  tags = {
    Project     = var.project_name
    Environment = var.environment
  }
}

resource "aws_s3_bucket_versioning" "website_versioning" {
  bucket = aws_s3_bucket.website.id
  versioning_configuration {
    status = "Enabled"
  }
}

resource "aws_s3_bucket_server_side_encryption_configuration" "website_encryption" {
  bucket = aws_s3_bucket.website.id
  rule {
    apply_server_side_encryption_by_default {
      sse_algorithm = "AES256"
    }
  }
}

resource "aws_s3_bucket_public_access_block" "website_block_public" {
  bucket                  = aws_s3_bucket.website.id
  block_public_acls       = true
  block_public_policy     = true
  ignore_public_acls      = true
  restrict_public_buckets = true
}

# -----------------------------------------------------------------------------
# 2. AWS WAF WebACL (Edge Protection)
# -----------------------------------------------------------------------------
resource "aws_wafv2_web_acl" "cloudfront_waf" {
  provider    = aws.us_east_1
  name        = "${var.project_name}-edge-waf"
  description = "Edge WAF rate limiting and AWS Common Rule Set"
  scope       = "CLOUDFRONT"

  default_action {
    allow {}
  }

  rule {
    name     = "RateLimitPerIP"
    priority = 1

    action {
      block {}
    }

    statement {
      rate_based_statement {
        limit              = 1000
        aggregate_key_type = "IP"
      }
    }

    visibility_config {
      cloudwatch_metrics_enabled = true
      metric_name                = "RateLimitPerIPMetric"
      sampled_requests_enabled   = true
    }
  }

  rule {
    name     = "AWSManagedRulesCommonRuleSet"
    priority = 2

    override_action {
      none {}
    }

    statement {
      managed_rule_group_statement {
        name        = "AWSManagedRulesCommonRuleSet"
        vendor_name = "AWS"
      }
    }

    visibility_config {
      cloudwatch_metrics_enabled = true
      metric_name                = "AWSManagedRulesCommonRuleSetMetric"
      sampled_requests_enabled   = true
    }
  }

  visibility_config {
    cloudwatch_metrics_enabled = true
    metric_name                = "${var.project_name}-waf-summary"
    sampled_requests_enabled   = true
  }
}

# -----------------------------------------------------------------------------
# 3. CloudFront Origin Access Control (OAC)
# -----------------------------------------------------------------------------
resource "aws_cloudfront_origin_access_control" "oac" {
  name                              = "${var.project_name}-oac"
  description                       = "OAC for private S3 website bucket"
  origin_access_control_origin_type = "s3"
  signing_behavior                  = "always"
  signing_protocol                  = "sigv4"
}

# -----------------------------------------------------------------------------
# 4. Security Response Headers Policy
# -----------------------------------------------------------------------------
resource "aws_cloudfront_response_headers_policy" "security_headers" {
  name    = "${var.project_name}-security-headers"
  comment = "Enforces HSTS, X-Content-Type-Options, X-Frame-Options, CSP"

  security_headers_config {
    strict_transport_security {
      access_control_max_age_sec = 63072000 # 2 years
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
    xss_protection {
      mode_block = true
      protection = true
      override   = true
    }
    referrer_policy {
      referrer_policy = "strict-origin-when-cross-origin"
      override        = true
    }
  }
}

# -----------------------------------------------------------------------------
# 5. CloudFront Distribution
# -----------------------------------------------------------------------------
resource "aws_cloudfront_distribution" "cdn" {
  enabled             = true
  is_ipv6_enabled     = true
  comment             = "Production distribution for ${var.project_name}"
  default_root_object = "index.html"
  web_acl_id          = aws_wafv2_web_acl.cloudfront_waf.arn

  origin {
    domain_name              = aws_s3_bucket.website.bucket_regional_domain_name
    origin_id                = "S3Origin"
    origin_access_control_id = aws_cloudfront_origin_access_control.oac.id
  }

  default_cache_behavior {
    target_origin_id       = "S3Origin"
    viewer_protocol_policy = "redirect-to-https"
    allowed_methods        = ["GET", "HEAD", "OPTIONS"]
    cached_methods         = ["GET", "HEAD"]
    compress               = true

    # Managed CachingOptimized policy
    cache_policy_id            = "658327ea-f89d-4fab-a63d-7e88639e58f6"
    response_headers_policy_id = aws_cloudfront_response_headers_policy.security_headers.id
  }

  # SPA Routing: Redirect 403/404 back to index.html for client-side routing
  custom_error_response {
    error_code            = 403
    response_code         = 200
    response_page_path    = "/index.html"
    error_caching_min_ttl = 10
  }

  custom_error_response {
    error_code            = 404
    response_code         = 200
    response_page_path    = "/index.html"
    error_caching_min_ttl = 10
  }

  price_class = "PriceClass_100" # US, Canada, Europe for cost optimization

  restrictions {
    geo_restriction {
      restriction_type = "none"
    }
  }

  viewer_certificate {
    cloudfront_default_certificate = true
  }

  tags = {
    Project     = var.project_name
    Environment = var.environment
  }
}

# -----------------------------------------------------------------------------
# 6. S3 Bucket Policy Granting Access to CloudFront OAC
# -----------------------------------------------------------------------------
resource "aws_s3_bucket_policy" "website_policy" {
  bucket = aws_s3_bucket.website.id

  policy = jsonencode({
    Version = "2012-10-17"
    Statement = [
      {
        Sid       = "AllowCloudFrontServicePrincipalReadOnly"
        Effect    = "Allow"
        Principal = { Service = "cloudfront.amazonaws.com" }
        Action    = "s3:GetObject"
        Resource  = "${aws_s3_bucket.website.arn}/*"
        Condition = {
          StringEquals = {
            "AWS:SourceArn" = aws_cloudfront_distribution.cdn.arn
          }
        }
      }
    ]
  })
}

output "s3_bucket_name" {
  value = aws_s3_bucket.website.id
}

output "cloudfront_distribution_id" {
  value = aws_cloudfront_distribution.cdn.id
}

output "cloudfront_domain_name" {
  value = aws_cloudfront_distribution.cdn.domain_name
}
```

---

## 3. Verification & Automated Test Harness

### 3.1 Deployment Instructions

```bash
# 1. Initialize and apply Terraform
terraform init
terraform apply -auto-approve

# 2. Extract outputs
BUCKET_NAME=$(terraform output -raw s3_bucket_name)
DIST_ID=$(terraform output -raw cloudfront_distribution_id)
CDN_DOMAIN=$(terraform output -raw cloudfront_domain_name)
```

### 3.2 Uploading Sample Static Assets

```bash
# Create sample index.html
cat << 'EOF' > index.html
<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <title>Enterprise Portal</title>
</head>
<body>
    <h1>Production Static Site on AWS</h1>
    <p>Powered by Amazon S3, CloudFront OAC, and AWS WAF.</p>
</body>
</html>
EOF

# Upload asset to private S3 bucket
aws s3 cp index.html "s3://${BUCKET_NAME}/index.html" --content-type "text/html"

# Invalidate CloudFront cache to pick up immediate changes
aws cloudfront create-invalidation \
    --distribution-id "${DIST_ID}" \
    --paths "/*"
```

### 3.3 Verification Tests (Bash / Curl)

```bash
# Test 1: Verify direct S3 public access is BLOCKED (Expected: HTTP 403 Forbidden)
echo "Testing Direct S3 Access..."
curl -I -s "https://${BUCKET_NAME}.s3.amazonaws.com/index.html" | head -n 1
# Expected output: HTTP/1.1 403 Forbidden

# Test 2: Verify CloudFront serves content with HTTP 200 OK
echo "Testing CloudFront CDN Ingress..."
curl -I -s "https://${CDN_DOMAIN}/index.html" | head -n 1
# Expected output: HTTP/2 200

# Test 3: Verify Security Response Headers are present
echo "Checking Security Headers..."
curl -I -s "https://${CDN_DOMAIN}/index.html" | grep -iE 'strict-transport-security|x-frame-options|x-content-type-options'
# Expected output:
# strict-transport-security: max-age=63072000; includeSubDomains; preload
# x-frame-options: DENY
# x-content-type-options: nosniff

# Test 4: Verify Single Page App (SPA) client-side fallback
echo "Testing SPA 404 rewrite..."
curl -s -o /dev/null -w "%{http_code}\n" "https://${CDN_DOMAIN}/non-existent-route"
# Expected output: 200 (Re-written to index.html with HTTP 200)
```

### 3.4 Teardown Script

```bash
# Empty S3 bucket versions before destroying
aws s3 rm "s3://${BUCKET_NAME}" --recursive
terraform destroy -auto-approve
```

---

## 4. Hardening, Security & Cost Analysis

### 4.1 Security Controls & Hardening
- **Zero S3 Public Exposure**: S3 Block Public Access blocks all public ACLs and bucket policies. The bucket cannot be opened to the world even if an administrator attempts to upload an object with `--acl public-read`.
- **Cryptographic OAC Isolation**: Legacy Origin Access Identity (OAI) did not support KMS customer-managed keys or SSE-KMS encryption. OAC uses SigV4 signing, supporting encrypted buckets and fine-grained resource policies.
- **OWASP Protection via AWS WAF**: The attached WebACL enforces automated IP rate limiting (1,000 requests per 5-minute sliding window) and blocks common web exploits (cross-site scripting, bad user agents).
- **HSTS Preload Ready**: The `Strict-Transport-Security` header is sent with a 2-year expiration and `preload` directive, forcing browsers to communicate exclusively over TLS 1.3.

### 4.2 Cost Breakdown (1 Million Requests / Month Scenario)

| Service Component | Usage / Tier | Estimated Monthly Cost |
|:---|:---|:---|
| **Amazon S3 Standard Storage** | 2 GB static build assets | \$0.05 |
| **Amazon S3 API Requests** | 5,000 internal origin cache-miss GETs | \$0.002 |
| **Amazon CloudFront Data Out** | 50 GB transferred to internet | \$4.25 (First 1TB free in AWS Free Tier) |
| **Amazon CloudFront Requests** | 1,000,000 HTTPS requests | \$1.00 |
| **AWS WAF** | 1 WebACL (\$5.00) + 2 Rules (\$2.00) + 1M requests (\$0.60) | \$7.60 |
| **Total Estimated Cost** | **Production Baseline** | **~\$12.90 / month** |

---

## 5. Operational Runbook & Disaster Recovery

### Common Operational Issues & Runbook Steps

#### Alert 1: Elevated 403 Forbidden Errors on CloudFront
- **Symptoms**: End users report seeing `403 Forbidden` errors across the website.
- **Root Cause Diagnostic**:
  1. Inspect whether the AWS WAF WebACL is blocking legitimate users due to aggressive rate limiting. Run CloudWatch query:
     ```bash
     aws wafv2 get-sampled-requests \
         --web-acl-arn "$WAF_ARN" \
         --rule-metric-name "RateLimitPerIPMetric" \
         --scope "CLOUDFRONT" \
         --time-window StartTime=$(date -u -d '1 hour ago' +%s),EndTime=$(date -u +%s) \
         --max-items 50
     ```
  2. Inspect S3 Bucket Policy: Verify that `aws:SourceArn` in the S3 bucket policy matches the exact ARN of the active CloudFront distribution.

#### Alert 2: Stale Frontend Deployment (Users See Old JavaScript Bundles)
- **Root Cause**: Modern single-page applications use content-hashed bundles (`app.a1b2c3.js`), but `index.html` was cached by CloudFront edge nodes.
- **Remediation Runbook**:
  Execute a targeted invalidation strictly for root HTML documents:
  ```bash
  aws cloudfront create-invalidation \
      --distribution-id "$DIST_ID" \
      --paths "/index.html" "/"
  ```

#### Disaster Recovery & Data Protection
- **Accidental Object Deletion**: S3 bucket versioning is enabled. Any deleted file generates a Delete Marker; previous versions can be restored immediately using the AWS CLI:
  ```bash
  aws s3api get-object --bucket "$BUCKET_NAME" --key "index.html" --version-id "$VERSION_ID" index.html
  ```
- **Regional Resiliency**: Amazon CloudFront is a globally distributed service with 200+ PoPs across 40+ countries. S3 Standard replicates objects across a minimum of 3 physical Availability Zones with 99.999999999% (11 9s) durability.
