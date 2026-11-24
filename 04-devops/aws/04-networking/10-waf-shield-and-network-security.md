# AWS Network Security: AWS WAF, AWS Shield & AWS Network Firewall

> **Module**: `04-networking`  
> **Topic**: Edge DDoS Mitigation, Web Application Firewalls, Managed Rulesets & Deep Packet Inspection  
> **Target Audience**: Cloud Security Engineers, Network Architects, DevSecOps Leads  
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

Modern web applications face a constant barrage of sophisticated network threats ranging from volumetric Distributed Denial of Service (DDoS) floods targeting Layer 3 and 4 network infrastructure (SYN floods, UDP reflection attacks), to application-layer attacks (SQL injection, cross-site scripting, credential stuffing, and scraping bots) on Layer 7. Traditional perimeter firewalls cannot inspect encrypted HTTPS payloads at high speed or scale elastically to absorb terabits-per-second volumetric DDoS floods.

AWS delivers a layered, defense-in-depth network security architecture:
1. **AWS Shield**: Global DDoS protection defending against volumetric attacks at OSI Layers 3 and 4.
2. **AWS WAF (Web Application Firewall)**: Inspects Layer 7 HTTP/HTTPS traffic, blocking malicious requests, automated bots, and OWASP Top 10 exploits before they reach application servers.
3. **AWS Network Firewall**: A stateful, managed network firewall providing deep packet inspection (DPI), URL domain filtering, and intrusion prevention (IPS) across entire VPCs.
4. **Route 53 Resolver DNS Firewall**: Inspects and filters outbound DNS queries to prevent data exfiltration to command-and-control (C2) domains.

---

## 2. Core Concepts

### 2.1 AWS WAF (Web Application Firewall)
- **Web ACL (Access Control List)**: Container for rules defining traffic evaluation criteria, attached to Amazon CloudFront distributions, Application Load Balancers, Amazon API Gateways, AWS AppSync GraphQL APIs, or Amazon Cognito user pools.
- **WAF Rules & Actions**:
  - *Allow*: Permits the request to pass to the origin.
  - *Block*: Returns an HTTP 403 Forbidden with optional custom response JSON/HTML.
  - *Count*: Increments a CloudWatch metric and continues evaluating subsequent rules (crucial for testing new rules without breaking production).
  - *CAPTCHA / Challenge*: Presents interactive or silent cryptographic browser verification challenges to block automated bots.
- **Rule Types**:
  - *AWS Managed Rules (AMR)*: Pre-configured rules maintained by AWS threat intelligence (e.g., Core Rule Set, Known Bad Inputs, SQL Database, Linux OS vulnerabilities, Bot Control, Account Takeover Prevention - ATP).
  - *Custom Rate-Based Rules*: Tracks request rates per client IP over 1, 2, or 5-minute sliding evaluation windows.
  - *Custom Match Rules*: Matches headers, query strings, regex patterns, GeoIP, or CIDR blocks.

### 2.2 AWS Shield Standard vs Shield Advanced
- **AWS Shield Standard**:
  - Automatically enabled for all AWS customers at **zero extra cost**.
  - Defends against common Layer 3/4 infrastructure attacks (SYN/ACK floods, UDP reflection).
  - Works seamlessly with CloudFront and Route 53.
- **AWS Shield Advanced**:
  - Enterprise DDoS protection offering 24/7 access to the **AWS Shield Response Team (SRT)**.
  - Automatic application-layer DDoS mitigation on ALB and CloudFront.
  - **DDoS Cost Protection**: Financial credit for scaling costs incurred by backend EC2, ALB, or CloudFront during an attack.
  - Bundles AWS WAF usage at no additional charge for protected resources.

### 2.3 AWS Network Firewall
- Deployed inline within dedicated firewall subnets in a VPC.
- Integrates with AWS Transit Gateway or VPC route tables to inspect east-west (VPC-to-VPC) or north-south (Internet ingress/egress) traffic.
- Built on the open-source **Suricata** engine, supporting stateful intrusion detection/prevention (IDS/IPS), TLS inspection, and outbound domain filtering (e.g., allow outbound access only to `*.github.com`).

---

## 3. How It Works Internally

```
+---------------------------------------------------------------------------------------------------+
|                        AWS MULTI-TIER NETWORK SECURITY DEFENSE ARCHITECTURE                       |
|                                                                                                   |
|  [External Attacker & Legitimate Users]                                                           |
|         |                                                                                         |
|         v (Internet Traffic: Volumetric DDoS + HTTPS Layer 7 Attacks)                             |
|  +---------------------------------------------------------------------------------------------+  |
|  | LAYER 3 / 4 DEFENSE: AWS SHIELD                                                             |  |
|  | - AWS Edge PoP / Global Network Fabric                                                      |  |
|  | - Absorbs Terabit/sec UDP Reflection & SYN Floods at the border                             |  |
|  | - Unaltered clean traffic passed to CloudFront / ALB                                        |  |
|  +---------------------------------------------------------------------------------------------+  |
|         |                                                                                         |
|         v (Clean Layer 4 Traffic)                                                                 |
|  +---------------------------------------------------------------------------------------------+  |
|  | LAYER 7 DEFENSE: AWS WAF (Attached to CloudFront or ALB)                                     |  |
|  | 1. IP Reputation & Rate-Limit Rule (Block if > 1,000 req/min)                               |  |
|  | 2. AWS Managed Rules: CoreRuleSet & SQLi Rule Set                                           |  |
|  | 3. Bot Control: Solves silent browser Challenge / Blocks headless curl bots                 |  |
|  +---------------------------------------------------------------------------------------------+  |
|         |                                                                                         |
|         v (Allowed Layer 7 Requests)                                                              |
|  +---------------------------------------------------------------------------------------------+  |
|  | VPC PERIMETER DEFENSE: AWS NETWORK FIREWALL                                                 |  |
|  | - Ingress Routing Table steers packets to Firewall Endpoint ENI                             |  |
|  | - Suricata Engine: Deep Packet Inspection & IPS signature matching                          |  |
|  | - Domain Whitelist: Blocks egress to unauthorized command-and-control IPs                   |  |
|  +---------------------------------------------------------------------------------------------+  |
|         |                                                                                         |
|         v (Sanitized Packets)                                                                     |
|  [Target Workload: Private ECS Tasks / EC2 Backend Instances]                                     |
+---------------------------------------------------------------------------------------------------+
```

---

## 4. Syntax & Usage

### 4.1 Creating a Web ACL with AWS Managed Rules via AWS CLI

```bash
# 1. Create a Web ACL in us-east-1 for CloudFront (or regional for ALB)
aws wafv2 create-web-acl \
    --name "prod-edge-web-acl" \
    --scope "REGIONAL" \
    --default-action Allow="{}" \
    --rules '[
      {
        "Name": "AWS-AWSManagedRulesCommonRuleSet",
        "Priority": 1,
        "Statement": {
          "ManagedRuleGroupStatement": {
            "VendorName": "AWS",
            "Name": "AWSManagedRulesCommonRuleSet"
          }
        },
        "OverrideAction": {
          "None": {}
        },
        "VisibilityConfig": {
          "SampledRequestsEnabled": true,
          "CloudWatchMetricsEnabled": true,
          "MetricName": "AWSManagedRulesCommonRuleSetMetric"
        }
      }
    ]' \
    --visibility-config SampledRequestsEnabled=true,CloudWatchMetricsEnabled=true,MetricName=prodWebAclMetric
```

### 4.2 Associating Web ACL with an Application Load Balancer

```bash
aws wafv2 associate-web-acl \
    --web-acl-arn "arn:aws:wafv2:us-east-1:123456789012:regional/webacl/prod-edge-web-acl/50dc6c49-5c0c" \
    --resource-arn "arn:aws:elasticloadbalancing:us-east-1:123456789012:loadbalancer/app/prod-alb/12345678"
```

---

## 5. Practical Examples

### 5.1 Production Web ACL with Rate-Limiting, Core Rules & SQLi Protection (Terraform)

```hcl
resource "aws_wafv2_web_acl" "enterprise_waf" {
  name        = "prod-enterprise-web-acl"
  description = "Production WAF with Managed Rules and Rate Limiting"
  scope       = "REGIONAL"

  default_action {
    allow {}
  }

  # Rule 1: Custom Rate-Based Rule (Max 1,000 requests per 5 minutes per IP)
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
      metric_name                = "RateLimitPerIP"
      sampled_requests_enabled   = true
    }
  }

  # Rule 2: AWS Managed Rules Common Rule Set (CRS)
  rule {
    name     = "AWSManagedRulesCommonRuleSet"
    priority = 10

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
      metric_name                = "AWSCommonRules"
      sampled_requests_enabled   = true
    }
  }

  # Rule 3: AWS Managed Rules Known Bad Inputs
  rule {
    name     = "AWSManagedRulesKnownBadInputs"
    priority = 20

    override_action {
      none {}
    }

    statement {
      managed_rule_group_statement {
        name        = "AWSManagedRulesKnownBadInputsRuleSet"
        vendor_name = "AWS"
      }
    }

    visibility_config {
      cloudwatch_metrics_enabled = true
      metric_name                = "AWSKnownBadInputs"
      sampled_requests_enabled   = true
    }
  }

  # Rule 4: SQL Injection Rule Set
  rule {
    name     = "AWSManagedRulesSQLiRuleSet"
    priority = 30

    override_action {
      none {}
    }

    statement {
      managed_rule_group_statement {
        name        = "AWSManagedRulesSQLiRuleSet"
        vendor_name = "AWS"
      }
    }

    visibility_config {
      cloudwatch_metrics_enabled = true
      metric_name                = "AWSSQLiRules"
      sampled_requests_enabled   = true
    }
  }

  visibility_config {
    cloudwatch_metrics_enabled = true
    metric_name                = "enterpriseWebAcl"
    sampled_requests_enabled   = true
  }
}

# Attach Web ACL to Application Load Balancer
resource "aws_wafv2_web_acl_association" "alb_assoc" {
  resource_arn = var.alb_arn
  web_acl_arn  = aws_wafv2_web_acl.enterprise_waf.arn
}
```

### 5.2 Bot Control with Silent Cryptographic Challenge

```hcl
rule {
  name     = "AWSManagedRulesBotControlRuleSet"
  priority = 15

  override_action {
    none {}
  }

  statement {
    managed_rule_group_statement {
      name        = "AWSManagedRulesBotControlRuleSet"
      vendor_name = "AWS"

      managed_rule_group_configs {
        aws_managed_rules_bot_control_rule_set {
          inspection_level = "COMMON"
        }
      }

      rule_action_override {
        action_to_use {
          challenge {}
        }
        name = "CategoryHttpLibrary"
      }
    }
  }

  visibility_config {
    cloudwatch_metrics_enabled = true
    metric_name                = "BotControlMetric"
    sampled_requests_enabled   = true
  }
}
```

### 5.3 AWS Network Firewall Stateful Domain Filtering

```hcl
resource "aws_networkfirewall_rule_group" "domain_filtering" {
  capacity = 100
  name     = "fw-allow-trusted-domains"
  type     = "STATEFUL"

  rule_group {
    rules_source {
      rules_source_list {
        generated_rules_type = "ALLOWLIST"
        target_types         = ["HTTP_HOST", "TLS_SNI"]
        targets              = [
          ".github.com",
          ".amazonaws.com",
          "registry-1.docker.io"
        ]
      }
    }
  }
}
```

### 5.4 WAF Logging to Amazon Kinesis Data Firehose & S3

```hcl
resource "aws_wafv2_web_acl_logging_configuration" "waf_logging" {
  log_destination_configs = [aws_kinesis_firehose_delivery_stream.waf_stream.arn]
  resource_arn            = aws_wafv2_web_acl.enterprise_waf.arn

  redacted_fields {
    single_header {
      name = "authorization"
    }
  }
}
```

### 5.5 IP Geolocation Whitelisting Rule

```hcl
rule {
  name     = "AllowOnlyUSandCanada"
  priority = 5

  action {
    block {}
  }

  statement {
    not_statement {
      statement {
        geo_match_statement {
          country_codes = ["US", "CA"]
        }
      }
    }
  }

  visibility_config {
    cloudwatch_metrics_enabled = true
    metric_name                = "GeoRestrictionMetric"
    sampled_requests_enabled   = true
  }
}
```

---

## 6. Best Practices

1. **Deploy New Rules in `Count` Mode First**: Never push custom regex or complex rule groups straight to `Block` in production. Run them in `Count` mode for at least 7 days to evaluate CloudWatch metrics and rule match sampling for false positives.
2. **Redact Sensitive Headers in WAF Logs**: Always redact `Authorization`, `Cookie`, and `Set-Cookie` headers in WAF logging configurations to prevent PII and authentication credentials from persisting in S3 or OpenSearch.
3. **Layer WAF at CloudFront to Protect Edge**: Attaching WAF to CloudFront terminates attacks at edge PoPs before they consume AWS backbone bandwidth or hit your origin ALBs.
4. **Combine Rate-Limiting with Bot Control**: Apply aggressive rate limiting on sensitive routes (e.g., `/api/login`, `/auth/reset-password`) while using silent browser challenges on general pages.
5. **Set Up Shield Advanced for Mission-Critical Public Endpoints**: Provides 24/7 access to the Shield Response Team (SRT) and protects against sudden financial spikes from DDoS-driven scaling.

---

## 7. Common Mistakes

| Mistake | Root Cause | Impact | Fix |
|---|---|---|---|
| Selecting Wrong Scope for CloudFront WAF | Web ACL created with `REGIONAL` scope instead of `CLOUDFRONT`. | Cannot associate Web ACL with CloudFront. | Create Web ACL in `us-east-1` with scope `CLOUDFRONT`. |
| False Positives Blocking Valid Form Payloads | Core Rule Set detects JSON or XML payload as SQL injection. | Legitimate customer requests blocked with 403. | Override the specific failing rule inside the group to `Count`. |
| Neglecting Rate-Limiting Evaluation Window | Setting 1-minute window on bursty legitimate traffic. | Flash sale buyers inadvertently throttled. | Use a 5-minute evaluation window with customized burst tolerance. |
| Forgetting Symmetric Routing in Network Firewall | Firewall endpoint in only one AZ or missing route table entry. | Packets cross AZs and are dropped by stateful engine. | Deploy a dedicated firewall subnet and endpoint in every application AZ. |
| Storing Sensitive Tokens in Unredacted WAF Logs | Firehose dumps full raw HTTP headers into S3 data lake. | Severe compliance failure (PCI-DSS / HIPAA breach). | Explicitly configure `redacted_fields` for auth and session headers. |
| Expecting Shield Standard to Mitigate L7 DDoS | Relying on free Shield Standard against HTTP request floods. | Web servers overwhelmed; Shield Standard only covers L3/L4. | Deploy AWS WAF with rate-based rules or subscribe to Shield Advanced. |

---

## 8. Performance, Security & Cost Notes

### 8.1 Performance Impact
- **Sub-millisecond WAF Latency**: AWS WAF rule evaluation runs inline within the CloudFront edge or ALB reverse-proxy pipeline, adding less than 1ms overhead per request.
- **Network Firewall Flow Scaling**: Each AWS Network Firewall endpoint processes up to 100 Gbps per AZ with minimal microsecond packet inspection latency.

### 8.2 Security & Compliance
- **PCI-DSS Compliance**: Requirement 6.6 mandates installing an automated technical solution that detects and prevents web-based attacks; AWS WAF satisfies this compliance requirement.

### 8.3 Cost Breakdown
- **AWS WAF**:
  - \$5.00 per Web ACL per month.
  - \$1.00 per rule per month.
  - \$0.60 per million requests evaluated.
  - Bot Control adds \$10.00/month + \$1.00 per million requests.
- **AWS Shield Standard**: \$0.00 (Free).
- **AWS Shield Advanced**: \$3,000 per month (1-year enterprise subscription) + data transfer out fees.
- **AWS Network Firewall**: \$0.395 per firewall endpoint hour (~$285/month per AZ) + \$0.065 per GB processed.

---

## 9. Comparison Table

| Dimension | AWS WAF | AWS Shield Advanced | AWS Network Firewall |
|---|---|---|---|
| **OSI Layer** | Layer 7 (Application) | Layer 3, 4, and 7 | Layer 3 to Layer 7 |
| **Primary Threats** | SQLi, XSS, bots, scraping | Volumetric DDoS, SYN floods | Protocol anomalies, C2 malware |
| **Attached To** | CloudFront, ALB, API Gateway | CloudFront, ALB, Route 53, EIP | VPC Route Tables / Subnets |
| **Inspection Engine** | Pattern / Regex / Token Bucket | Anomaly detection / BGP scrubbing| Suricata IPS / DPI |
| **Managed Rulesets** | Yes (AWS & Marketplace) | Managed by AWS SRT | AWS managed threat signatures |
| **Pricing Model** | Pay-as-you-go (\$5/ACL + \$1/rule)| \$3,000 / month flat subscription| Endpoint hourly + GB processed |

---

## 10. Interview Questions & Answers

<details>
<summary><strong>Q1: What is the architectural difference between AWS Shield Standard and AWS Shield Advanced?</strong></summary>

- **AWS Shield Standard** is an automatic, always-on perimeter protection service included at no cost for all AWS customers. It mitigates common Layer 3 and Layer 4 attacks (such as SYN floods, UDP reflection attacks) across Amazon CloudFront, Route 53, and Elastic Load Balancers.
- **AWS Shield Advanced** is an enterprise paid subscription (\$3,000/mo) providing:
  1. Automated application-layer (Layer 7) DDoS mitigation.
  2. 24/7 direct access to the AWS Shield Response Team (SRT) to write custom mitigation rules during an active incident.
  3. Comprehensive attack visibility and reporting via CloudWatch and Shield console.
  4. "DDoS Cost Protection": Reimburses auto-scaling and resource costs incurred as a result of a DDoS attack.
  5. Included licenses for AWS WAF on all protected assets.
</details>

<details>
<summary><strong>Q2: How do you safely test a new AWS WAF rule in production without blocking legitimate users?</strong></summary>

When deploying a new custom rule or adding an AWS Managed Rule Group:
1. Set the rule action to **Count** (or set `OverrideAction: count` for a managed group).
2. Deploy the rule to the production Web ACL.
3. In `Count` mode, the rule inspects traffic and increments the associated CloudWatch metric (`CountedRequests`) without dropping packets.
4. Analyze the sampled requests in the WAF console or query CloudWatch/Firehose logs to verify that legitimate customer requests are not matching the rule.
5. Once verified over several days of peak traffic, change the rule action to **Block**.
</details>

<details>
<summary><strong>Q3: How does AWS WAF evaluate rule priority and terminate processing?</strong></summary>

Rules within a Web ACL are evaluated in strict numeric order based on their assigned **Priority** (starting from 0, ascending). 
- If a request matches a rule configured with action `Block`, WAF immediately drops the request and terminates further evaluation.
- If a request matches a rule configured with action `Allow`, WAF immediately allows the request to pass to the origin and terminates further evaluation.
- If a request matches a rule configured with action `Count`, WAF records the metric and **continues evaluating** the remaining rules in the list until a terminating action or the default Web ACL action is reached.
</details>

<details>
<summary><strong>Q4: How does AWS Network Firewall inspect outbound internet traffic from private VPC subnets?</strong></summary>

Using **VPC Ingress Routing** and custom route tables:
1. Private application subnets route `0.0.0.0/0` traffic to an **AWS Network Firewall Endpoint (VPCE)** deployed in a dedicated firewall subnet.
2. The firewall endpoint intercepts the packets and runs them through its stateful Suricata inspection engine (checking IP reputation, TLS SNI headers, or domain allowlists).
3. Packets that pass inspection are forwarded from the firewall subnet route table to the VPC's NAT Gateway, which routes them to the Internet Gateway.
</details>

<details>
<summary><strong>Q5: What is the purpose of WAF Custom Response Bodies?</strong></summary>

When WAF blocks a malicious request, it defaults to returning a generic `403 Forbidden` plain text error. With Custom Response Bodies, developers can configure WAF to return a localized branded HTML error page or a standardized JSON payload (e.g., `{"error": "rate_limit_exceeded", "retry_after": 60}`) with custom HTTP status codes (such as `429 Too Many Requests`), ensuring consistent API client handling.
</details>

<details>
<summary><strong>Q6: How does WAF Bot Control distinguish between good bots and malicious scrapers?</strong></summary>

WAF Bot Control analyzes request signatures, IP reputation, and browser characteristics:
- **Verified Bots**: Automatically allows verified search engine crawlers (Googlebot, Bingbot) and monitoring services by verifying their reverse DNS lookups and IP authenticity.
- **Unverified Scrapers**: Detects scraping scripts (Scrapy, Python Requests, Puppeteer) based on missing headers, non-standard TLS fingerprints (JA3), or user-agent inconsistencies.
- **Silent Challenges**: Issues invisible JavaScript browser challenges to verify human browser execution without user interaction.
</details>

<details>
<summary><strong>Q7: What is Route 53 Resolver DNS Firewall and what attack does it mitigate?</strong></summary>

Route 53 Resolver DNS Firewall inspects outbound DNS queries originating from inside your VPCs to the Amazon Provided DNS resolver (`10.0.0.2`). It protects against:
1. **DNS Exfiltration**: Malicious software encoding stolen enterprise data into DNS query names (e.g., `stolen-data.attacker-c2.com`).
2. **Command & Control (C2) Callouts**: Prevents compromised internal instances from resolving and communicating with known botnet command servers.
</details>

<details>
<summary><strong>Q8: What is WAF Web ACL Scope and why does it matter?</strong></summary>

AWS WAF Web ACLs exist in two distinct scopes:
- **CLOUDFRONT**: Must be created in the `us-east-1` (N. Virginia) region. Can only be associated with global Amazon CloudFront distributions.
- **REGIONAL**: Created in a specific AWS region (e.g., `eu-west-1`, `us-west-2`). Can be associated with Application Load Balancers, Amazon API Gateways, AppSync GraphQL APIs, and Cognito User Pools in that specific region.
</details>

---

## 11. Practice Exercises

<details>
<summary><strong>Exercise 1: Deploy a Web ACL with AWS Managed Rules</strong></summary>

**Goal**: Block SQL injection attacks on an Application Load Balancer.
1. Create a Web ACL in your local region with scope `REGIONAL`.
2. Add the `AWSManagedRulesSQLiRuleSet` with priority 1.
3. Attach the Web ACL to your test ALB.
4. Execute `curl -G "http://<alb-dns>/?id=1'%20OR%20'1'='1"` and verify WAF blocks the request with HTTP 403 Forbidden.
</details>

<details>
<summary><strong>Exercise 2: Implement Custom Rate-Limiting</strong></summary>

**Goal**: Throttle clients sending more than 100 requests in 5 minutes.
1. Create a custom rate-based rule in WAF evaluated per client IP.
2. Set rate limit to 100 requests.
3. Use a load-testing script or loop to send 150 consecutive requests.
4. Verify requests 1-100 succeed, and subsequent requests return HTTP 403.
</details>

<details>
<summary><strong>Exercise 3: Test New Rules Safely using Count Mode</strong></summary>

**Goal**: Inspect metric counts without impacting live users.
1. Add `AWSManagedRulesCommonRuleSet` to a Web ACL.
2. Configure `OverrideAction: count`.
3. Send requests containing non-standard user-agent headers.
4. Open CloudWatch Metrics -> WAFV2 and observe `AWSCommonRules CountedRequests` incrementing while responses remain HTTP 200.
</details>

<details>
<summary><strong>Exercise 4: Stream WAF Sample Logs to CloudWatch</strong></summary>

**Goal**: Inspect blocked request details.
1. Configure WAF Logging configuration targeting a CloudWatch Log Group.
2. Trigger a blocked request using a known bad user-agent.
3. Query the log group using CloudWatch Logs Insights:
   `fields @timestamp, httpRequest.clientIp, httpRequest.uri, terminatingRuleId | filter action = "BLOCK"`.
</details>

<details>
<summary><strong>Exercise 5: Configure Geographic Blocking</strong></summary>

**Goal**: Restrict API access to specific operating regions.
1. Create a rule matching client IP Country Code NOT in `["US", "GB", "DE"]`.
2. Set Action to `Block`.
3. Use a VPN or proxy to simulate an excluded country and verify traffic is rejected.
</details>

<details>
<summary><strong>Exercise 6: Deploy Route 53 Resolver DNS Firewall</strong></summary>

**Goal**: Block malicious outbound domain resolutions inside a VPC.
1. In Route 53 Resolver, create a DNS Firewall Rule Group.
2. Add the AWS-managed threat list `AWSManagedDomainsMalwareDomainList`.
3. Associate the rule group with your test VPC.
4. Attempt to resolve a test malicious domain from an EC2 instance; verify the query returns `NODATA`.
</details>

---

## 12. Summary & Quick Reference

```
+---------------------------------------------------------------------------------------------------+
| AWS NETWORK SECURITY DEFENSE STACK                                                                |
+--------------------------+-----------------------+------------------------------------------------+
| Service                  | OSI Layer Target      | Key Defense Capabilities                       |
+--------------------------+-----------------------+------------------------------------------------+
| AWS Shield Standard      | Layer 3 & Layer 4     | Border SYN/ACK flood & UDP reflection defense  |
| AWS Shield Advanced      | Layer 3, 4 & Layer 7  | 24/7 SRT support, DDoS cost guarantee, L7 auto |
| AWS WAF                  | Layer 7               | OWASP Top 10, SQLi, XSS, rate-limiting, bots   |
| AWS Network Firewall     | Layer 3 to Layer 7    | Deep Packet Inspection (DPI), Suricata IPS     |
| Route 53 DNS Firewall    | Application (DNS)     | Blocks C2 malware lookups & DNS data leaks     |
+--------------------------+-----------------------+------------------------------------------------+
```

- **WAF**: Protects web endpoints (ALB, CloudFront, API Gateway) from malicious HTTP payloads.
- **Shield**: Protects global infrastructure against multi-terabit volumetric DDoS floods.
- **Network Firewall**: Provides stateful inline traffic inspection and outbound domain filtering for entire VPCs.

---

## 13. Further Reading

- [AWS WAF, AWS Firewall Manager, and AWS Shield Advanced Developer Guide](https://docs.aws.amazon.com/waf/latest/developerguide/)
- [AWS Managed Rules for AWS WAF Reference](https://docs.aws.amazon.com/waf/latest/developerguide/aws-managed-rule-groups-list.html)
- [AWS Network Firewall Architecture & Deployment Guides](https://docs.aws.amazon.com/network-firewall/latest/developerguide/)
- [AWS Best Practices for DDoS Resiliency](https://docs.aws.amazon.com/whitepapers/latest/aws-best-practices-for-ddos-resiliency/)
- [04-networking/07-load-balancing.md](file:///04-devops/aws/04-networking/07-load-balancing.md)
