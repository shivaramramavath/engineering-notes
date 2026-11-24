# Amazon Route 53 & Enterprise DNS Routing Patterns

> **Module**: `04-networking`  
> **Topic**: Global Anycast DNS, Public/Private Hosted Zones, Routing Policies, Health Checks & Resolver  
> **Target Audience**: Cloud Engineers, Network Architects, Site Reliability Engineers  
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

Domain Name System (DNS) is the phonebook of the Internet, translating human-readable hostnames (`api.example.com`) into machine-routable IP addresses (`198.51.100.24` or `2001:db8::1`). In traditional self-managed infrastructure, running redundant BIND9 or PowerDNS clusters across continents required complex Anycast BGP peering, DDoS mitigation hardware, and manual zone file synchronization.

**Amazon Route 53** is a highly available and scalable cloud DNS web service named after TCP/UDP port 53. Designed for 100% SLA availability, Route 53 combines three critical network services:
1. **Domain Registration**: TLD registrar functionality for purchasing and managing domain ownership.
2. **Authoritative DNS Service**: An Anycast network of globally distributed name servers providing ultra-low-latency DNS queries and private split-horizon DNS inside VPCs.
3. **Health Checking & Intelligent Traffic Routing**: Real-time endpoint monitoring combined with sophisticated routing policies (Latency, Geolocation, Geoproximity, Weighted, and Active-Passive Failover).

---

## 2. Core Concepts

### 2.1 Hosted Zones
- **Public Hosted Zone**: Contains records evaluated on the public Internet. Route 53 assigns four unique Anycast name servers across distinct TLDs (e.g., `.com`, `.net`, `.org`, `.co.uk`) to ensure resilience against registrar-level outages.
- **Private Hosted Zone**: Associated strictly with one or more VPCs across any AWS account or region. Resolves internal microservice domain names (`db.internal.corp`) without exposing records to public resolvers.

### 2.2 Record Types & Alias Records
Standard DNS records supported include:
- `A` (IPv4 address), `AAAA` (IPv6 address), `CNAME` (Canonical name alias), `MX` (Mail exchange), `TXT` (Text / SPF / DKIM), `PTR` (Reverse DNS pointer), `SRV` (Service locator), `CAA` (Certificate Authority Authorization).

**Route 53 Alias Records**:
- A proprietary AWS extension that acts like a CNAME but responds to DNS queries directly with `A` or `AAAA` IP records.
- **Zone Apex Support**: Unlike standard CNAME records which cannot coexist at the zone apex (`example.com`), Alias records can be created directly at the root zone apex.
- **Free Internal Queries**: Route 53 does not bill for DNS queries mapped to AWS internal endpoints via Alias records (e.g., CloudFront distributions, Elastic Load Balancers, S3 website endpoints, API Gateway custom domains).

### 2.3 Traffic Routing Policies
1. **Simple Routing**: Default single-record mapping. Can return multiple IPs in random order for client-side round-robin.
2. **Weighted Routing**: Distributes traffic based on assigned percentage weights (e.g., 90% production, 10% canary).
3. **Latency-Based Routing**: Directs user queries to the AWS region that delivers the lowest round-trip latency.
4. **Failover Routing**: Active-passive DR configuration driven by Route 53 Health Checks.
5. **Geolocation Routing**: Routes traffic based on the physical location of the client's DNS resolver (continent, country, US state).
6. **Geoproximity Routing (Traffic Flow)**: Routes based on geographic distance between user and AWS resources, modifiable via an adjustable bias value.
7. **Multivalue Answer Routing**: Returns up to eight healthy records selected randomly from a pool, supporting basic DNS-based load balancing and health checking.
8. **IP-Based Routing**: Fine-grained routing matching client CIDR blocks to specific service endpoints.

### 2.4 Route 53 Resolver (Amazon Provided DNS)
- Operates at the reserved VPC base address plus two (`10.0.0.2` in a `10.0.0.0/16` network).
- **Inbound Endpoints**: Allows on-premises DNS servers to forward conditional queries into AWS private hosted zones.
- **Outbound Endpoints**: Allows EC2 instances and Lambda functions inside a VPC to conditionally resolve on-premises enterprise domains via corporate DNS servers.
- **DNS Firewall**: Inspects and filters outbound DNS queries from VPCs to prevent data exfiltration to malicious command-and-control domains.

---

## 3. How It Works Internally

```
+---------------------------------------------------------------------------------------------------+
|                                ROUTE 53 GLOBAL ANYCAST ARCHITECTURE                              |
|                                                                                                   |
|  [Client Browser]                                                                                 |
|         |                                                                                         |
|         v (DNS Query: api.enterprise.com)                                                         |
|  [ISP Recursive Resolver]                                                                         |
|         |                                                                                         |
|         +---------------------------+----------------------------+                                |
|         | Anycast BGP               | Anycast BGP                | Anycast BGP                    |
|         v                           v                            v                                |
|  [Edge PoP: Frankfurt]       [Edge PoP: Virginia]         [Edge PoP: Tokyo]                       |
|  +---------------------+     +---------------------+      +---------------------+                 |
|  | Route 53 Name Server|     | Route 53 Name Server|      | Route 53 Name Server|                 |
|  | ns-102.awsdns.org   |     | ns-844.awsdns.co.uk |      | ns-1612.awsdns.com  |                 |
|  +----------+----------+     +----------+----------+      +----------+----------+                 |
|             |                           |                            |                            |
|             +---------------------------+----------------------------+                            |
|                                         |                                                         |
|                             [Routing Policy Evaluation]                                           |
|                                         |                                                         |
|         +-------------------------------+-------------------------------+                         |
|         |                                                               |                         |
|   (Health: PASS)                                                  (Health: FAIL)                  |
|         v                                                               v                         |
|  [Primary Region: us-east-1]                                      [Failover Region: eu-central-1] |
|  Alias Target -> ALB                                              Standby DR ALB                  |
|  Returns: A -> 52.86.10.14                                        (Suppressed)                    |
+---------------------------------------------------------------------------------------------------+
```

1. **Anycast Routing**: Global edge locations announce identical BGP IP prefixes. Recursive resolvers automatically hit the network-topologically nearest Route 53 Anycast name server.
2. **EDNS0 Client Subnet (ECS)**: Route 53 supports ECS to determine client location even when querying through public aggregators like Google (`8.8.8.8`) or Cloudflare (`1.1.1.1`), ensuring accurate geolocation and latency calculations.
3. **Continuous Health Probing**: Route 53 health checkers situated in ~15 global AWS regions poll application endpoints every 10 or 30 seconds. A target is marked unhealthy when threshold failures occur sequentially.

---

## 4. Syntax & Usage

### 4.1 Creating Public and Private Hosted Zones via AWS CLI

```bash
# 1. Create a public hosted zone
aws route53 create-hosted-zone \
    --name "cloudenterprise.internal" \
    --caller-reference "hosted-zone-$(date +%s)" \
    --hosted-zone-config Comment="Primary production public zone",PrivateZone=false

# 2. Create a private hosted zone associated with a VPC
aws route53 create-hosted-zone \
    --name "corp.internal" \
    --vpc VPCRegion="us-east-1",VPCId="vpc-0123456789abcdef0" \
    --caller-reference "priv-zone-$(date +%s)" \
    --hosted-zone-config Comment="Internal microservices private zone",PrivateZone=true

# 3. Associate an additional VPC from another region to the private zone
aws route53 associate-vpc-with-hosted-zone \
    --hosted-zone-id "Z0123456789ABCDEF" \
    --vpc VPCRegion="eu-central-1",VPCId="vpc-0987654321fedcba0"
```

### 4.2 Creating an Alias Record to an Application Load Balancer

```bash
# Upsert an Alias record at zone apex pointing to an ALB
# Note: HostedZoneId inside AliasTarget is the canonical hosted zone ID of the ALB in that region
aws route53 change-resource-record-sets \
    --hosted-zone-id "Z0123456789ABCDEF" \
    --change-batch '{
      "Comment": "Upsert zone apex Alias to ALB",
      "Changes": [
        {
          "Action": "UPSERT",
          "ResourceRecordSet": {
            "Name": "cloudenterprise.internal.",
            "Type": "A",
            "AliasTarget": {
              "HostedZoneId": "Z35SXDOTRQ7X7K",
              "DNSName": "dualstack.prod-alb-123456789.us-east-1.elb.amazonaws.com.",
              "EvaluateTargetHealth": true
            }
          }
        }
      ]
    }'
```

---

## 5. Practical Examples

### 5.1 Active-Passive Failover Routing with Route 53 Health Checks (Terraform)

```hcl
# Route 53 Health Check probing primary ALB endpoint
resource "aws_route53_health_check" "primary_web" {
  fqdn              = "primary-alb.us-east-1.elb.amazonaws.com"
  port              = 443
  type              = "HTTPS"
  resource_path     = "/healthz"
  failure_threshold = "3"
  request_interval  = "10"

  tags = {
    Environment = "production"
    Role        = "primary-health-check"
  }
}

# Primary Record (Active)
resource "aws_route53_record" "primary" {
  zone_id = var.hosted_zone_id
  name    = "api.example.com"
  type    = "A"

  failover_routing_policy {
    type = "PRIMARY"
  }

  set_identifier = "primary-us-east-1"
  health_check_id = aws_route53_health_check.primary_web.id

  alias {
    name                   = aws_lb.primary.dns_name
    zone_id                = aws_lb.primary.zone_id
    evaluate_target_health = true
  }
}

# Secondary Record (Passive DR)
resource "aws_route53_record" "secondary" {
  zone_id = var.hosted_zone_id
  name    = "api.example.com"
  type    = "A"

  failover_routing_policy {
    type = "SECONDARY"
  }

  set_identifier = "secondary-eu-central-1"

  alias {
    name                   = aws_lb.secondary.dns_name
    zone_id                = aws_lb.secondary.zone_id
    evaluate_target_health = true
  }
}
```

### 5.2 Weighted Canary Routing for Blue/Green Deployments

```hcl
# 90% Production Traffic
resource "aws_route53_record" "prod_blue" {
  zone_id        = var.hosted_zone_id
  name           = "service.example.com"
  type           = "CNAME"
  ttl            = 60
  set_identifier = "blue-production"

  weighted_routing_policy {
    weight = 90
  }

  records = ["blue-v1.internal.example.com"]
}

# 10% Canary Traffic
resource "aws_route53_record" "prod_green" {
  zone_id        = var.hosted_zone_id
  name           = "service.example.com"
  type           = "CNAME"
  ttl            = 60
  set_identifier = "green-canary"

  weighted_routing_policy {
    weight = 10
  }

  records = ["green-v2.internal.example.com"]
}
```

### 5.3 Hybrid On-Premises to VPC DNS using Route 53 Resolver Inbound Endpoints

```hcl
resource "aws_security_group" "resolver_inbound_sg" {
  name        = "r53-resolver-inbound-sg"
  description = "Allow DNS queries from on-premises datacenter"
  vpc_id      = var.vpc_id

  ingress {
    description = "DNS TCP"
    from_port   = 53
    to_port     = 53
    protocol    = "tcp"
    cidr_blocks = ["192.168.0.0/16"]
  }

  ingress {
    description = "DNS UDP"
    from_port   = 53
    to_port     = 53
    protocol    = "udp"
    cidr_blocks = ["192.168.0.0/16"]
  }

  egress {
    from_port   = 0
    to_port     = 0
    protocol    = "-1"
    cidr_blocks = ["0.0.0.0/0"]
  }
}

resource "aws_route53_resolver_endpoint" "inbound" {
  name      = "onprem-to-aws-resolver"
  direction = "INBOUND"

  security_group_ids = [aws_security_group.resolver_inbound_sg.id]

  ip_address {
    subnet_id = var.private_subnet_a_id
  }

  ip_address {
    subnet_id = var.private_subnet_b_id
  }

  tags = {
    Environment = "production"
  }
}
```

### 5.4 DNSSEC Setup for Domain Integrity

```bash
# 1. Enable DNSSEC signing on public hosted zone
aws route53 enable-hosted-zone-dnssec \
    --hosted-zone-id "Z0123456789ABCDEF"

# 2. Retrieve Delegation Signer (DS) record metadata
aws route53 get-dnssec \
    --hosted-zone-id "Z0123456789ABCDEF"

# Output provides Key Tag, Algorithm, Digest Type, and Digest string
# 3. Add the DS record to parent registrar (e.g., Route 53 Domains or external registrar)
```

### 5.5 Geolocation Routing with Fallback Default

```hcl
# European users route to Frankfurt
resource "aws_route53_record" "europe" {
  zone_id        = var.hosted_zone_id
  name           = "portal.example.com"
  type           = "A"
  set_identifier = "europe-users"

  geolocation_routing_policy {
    continent = "EU"
  }

  alias {
    name                   = aws_lb.frankfurt.dns_name
    zone_id                = aws_lb.frankfurt.zone_id
    evaluate_target_health = true
  }
}

# Global Default Record for all other continents
resource "aws_route53_record" "default" {
  zone_id        = var.hosted_zone_id
  name           = "portal.example.com"
  type           = "A"
  set_identifier = "global-default"

  geolocation_routing_policy {
    country = "*"
  }

  alias {
    name                   = aws_lb.virginia.dns_name
    zone_id                = aws_lb.virginia.zone_id
    evaluate_target_health = true
  }
}
```

---

## 6. Best Practices

1. **Always Use Alias Records for AWS Targets**: Never use CNAME for ALBs, API Gateways, or CloudFront distributions. Alias records provide apex routing, automated IP change tracking, and zero query cost.
2. **Enable Target Health Evaluation**: Set `evaluate_target_health = true` on Alias records. If an ALB has no healthy backend targets, Route 53 immediately fails over to the standby record without waiting for external health checkers.
3. **Keep TTLs Low for Migration Windows**: Standard TTL is 300 to 86,400 seconds. Lower TTL to 60 seconds 24 hours prior to scheduled infrastructure migrations to ensure prompt recursive cache eviction.
4. **Deploy Redundant Resolver Endpoints Across AZs**: Always specify at least two subnets in distinct AZs when creating Route 53 Resolver Inbound or Outbound endpoints to eliminate single points of failure.
5. **Implement Route 53 Resolver DNS Firewall**: Block DNS exfiltration and malware lookups by attaching managed domain blocklists to your VPCs.

---

## 7. Common Mistakes

| Mistake | Root Cause | Impact | Fix |
|---|---|---|---|
| CNAME at Zone Apex (`example.com`) | Violates RFC 1034 section 3.6.2 (CNAME cannot coexist with SOA/NS). | Zone fails validation or mail MX records fail to resolve. | Use Route 53 `Alias` record (Type `A`/`AAAA`) at zone apex. |
| Forgetting Geolocation Default Record | Geolocation policy only handles specified countries. | Users in non-configured countries receive `NXDOMAIN`. | Always provide a fallback record with `country = "*"`. |
| Excessive Route 53 Health Check Interval | Default 30s interval with 3 retries takes 90 seconds to trigger failover. | Extended downtime during localized outage. | Enable Fast Health Checks (10-second interval). |
| Cross-Account Private Hosted Zone Link Failure | Trying to associate VPC without authorization. | `AccessDeniedException` during CLI association. | Call `create-vpc-association-authorization` in owner account first. |
| Inbound Resolver Endpoint Exposed to Public | Misconfigured security group allowing `0.0.0.0/0` on port 53. | Resolver becomes open DNS relay target for DDoS amplification. | Restrict ingress strictly to on-premise RFC 1918 CIDR blocks. |
| Overwriting NS Records During Migration | Deleting Route 53 default NS/SOA records in zone file. | Hosted zone becomes unreachable globally. | Never edit or delete assigned Route 53 NS or SOA records. |

---

## 8. Performance, Security & Cost Notes

### 8.1 Latency Optimization
- **EDNS0 Client Subnet**: Ensures routing decisions reflect end-user IPs rather than centralized public DNS resolvers.
- **Route 53 Traffic Flow**: Visual editor for chaining complex rules (e.g., Geolocation -> Latency -> Failover) computed instantaneously at edge PoPs.

### 8.2 Security & Compliance
- **DNSSEC**: Protects against DNS spoofing and cache poisoning by cryptographically signing records with asymmetric key pairs (KSK and ZSK).
- **VPC DNS Privacy**: Queries inside VPCs to Amazon Provided DNS (`10.0.0.2`) remain inside the AWS hypervisor fabric; packets never touch public transit.

### 8.3 Cost Breakdown
- **Public Hosted Zone**: \$0.50 / zone / month (first 25 zones).
- **Standard Queries**: \$0.40 per million queries (first 1 billion).
- **Alias Queries to AWS Resources**: Free.
- **Health Checks**: Standard AWS endpoint (\$0.50 / month), Non-AWS / HTTPS (\$1.00 - \$2.00 / month).
- **Resolver Endpoints**: \$0.11 per ENI per hour (~$80/month for 2-AZ HA pair) + \$0.40 per million resolver queries.

---

## 9. Comparison Table

| Feature / Dimension | Route 53 Alias Record | Standard DNS CNAME Record |
|---|---|---|
| **Protocol Compatibility** | Proprietary AWS extension | Standard IETF RFC 1034 / 2181 |
| **Zone Apex Allowed** | Yes (`example.com`) | No (Forbidden by RFC) |
| **Target Endpoints** | AWS native resources (ALB, S3, CloudFront) | Any arbitrary FQDN |
| **Query Cost** | Free for native AWS resources | Standard query fees apply (\$0.40/million) |
| **Health Check Aware** | Yes (`EvaluateTargetHealth: true`) | No (Requires explicit health check mapping) |
| **Response Type** | Returns `A` or `AAAA` IP records | Returns canonical hostname string |

---

## 10. Interview Questions & Answers

<details>
<summary><strong>Q1: Why can you create an Alias record at the zone apex, but not a CNAME record?</strong></summary>

According to DNS RFC 1034 (Section 3.6.2), a CNAME record cannot coexist with any other record type for the same name. Because a zone apex (`example.com`) must have `SOA` and `NS` records to function as an authoritative zone, placing a CNAME at the apex creates an illegal protocol collision. Route 53 Alias records bypass this limitation because to the querying client, Route 53 responds with standard `A` or `AAAA` records directly—the alias mapping is resolved dynamically inside the Route 53 control plane without exposing a CNAME.
</details>

<details>
<summary><strong>Q2: How does Route 53 Latency-Based Routing differ from Geolocation Routing?</strong></summary>

- **Latency-Based Routing** is dynamic: Route 53 periodically measures network latency across the Internet to AWS regions and directs the user to the AWS region that currently provides the lowest round-trip time. It can shift traffic dynamically if ISP routing conditions change.
- **Geolocation Routing** is deterministic: It maps the client's resolver IP to a fixed geographic boundary (Continent, Country, or US State) regardless of latency. This is used for compliance (GDPR data residency), localized licensing, or language targeting.
</details>

<details>
<summary><strong>Q3: How do Route 53 Health Checks evaluate an Application Load Balancer with EvaluateTargetHealth set to true?</strong></summary>

When `EvaluateTargetHealth` is enabled on an Alias record pointing to an ALB, Route 53 does not send external HTTP probes to the ALB. Instead, it queries the internal health state of the target groups registered behind that ALB. If all registered target EC2 instances/containers are failing their health checks, the ALB reports itself unhealthy to Route 53, which immediately triggers DNS failover to the backup record.
</details>

<details>
<summary><strong>Q4: How do you resolve internal domain names across multiple AWS accounts in private hosted zones?</strong></summary>

You associate the private hosted zone in Account A with the VPC in Account B. Because this crosses account boundaries, the CLI or API workflow requires two steps:
1. Account A executes `aws route53 create-vpc-association-authorization --hosted-zone-id <ID> --vpc VPCRegion=<region>,VPCId=<Account-B-VPC>`.
2. Account B executes `aws route53 associate-vpc-with-hosted-zone --hosted-zone-id <ID> --vpc VPCRegion=<region>,VPCId=<Account-B-VPC>`.
Once associated, EC2 instances in Account B query their local `10.0.0.2` resolver and seamlessly resolve Account A's private zone.
</details>

<details>
<summary><strong>Q5: What are Route 53 Resolver Inbound and Outbound Endpoints used for?</strong></summary>

They bridge DNS between on-premises datacenters and AWS VPCs over Direct Connect or VPN:
- **Inbound Endpoint**: Elastic Network Interfaces (ENIs) with private VPC IPs that receive DNS queries forwarded from corporate on-premise DNS servers, resolving AWS Private Hosted Zones.
- **Outbound Endpoint**: ENIs used by Route 53 Resolver rules to forward queries originating inside VPCs (e.g. for `*.corp.internal`) to on-premises DNS server IP addresses.
</details>

<details>
<summary><strong>Q6: What is the effect of setting a very high TTL vs a very low TTL on a DNS record?</strong></summary>

- **High TTL (e.g., 86400s / 24h)**: Reduces DNS query costs and improves client lookup latency because intermediate ISP resolvers cache the record. However, changing the record during an outage or migration will take up to 24 hours to propagate across all clients.
- **Low TTL (e.g., 10s - 60s)**: Enables rapid traffic shifting and near-instant failover. However, it increases DNS query volume and cost, while slightly increasing connection setup latency for cold lookups.
</details>

<details>
<summary><strong>Q7: How does Route 53 prevent split-brain issues during failover routing?</strong></summary>

Route 53 uses decentralized health checking nodes across multiple AWS geographic regions. By default, it requires a configurable number of consecutive failures (e.g., 3 out of 3 probes) across a majority of global health check reporters before a primary endpoint is declared unhealthy. This prevents localized ISP routing hiccups from causing premature or flapping DNS failover.
</details>

<details>
<summary><strong>Q8: What is DNSSEC and how is it implemented in Route 53?</strong></summary>

DNS Security Extensions (DNSSEC) adds cryptographic signatures (`RRSIG`) to DNS records, verified against Public Keys (`DNSKEY`) and Delegation Signer (`DS`) records up to the root zone (`.`). Route 53 supports hosted zone DNSSEC signing using AWS KMS asymmetric customer-managed keys (ECC_NIST_P256). The generated DS record must be published at the parent domain registrar to establish the cryptographic chain of trust.
</details>

---

## 11. Practice Exercises

<details>
<summary><strong>Exercise 1: Create a Split-Horizon DNS Architecture</strong></summary>

**Goal**: Configure `api.internal.company` to resolve to a private IP `10.0.20.50` inside a VPC, while resolving to a public ALB `52.14.80.20` on the public Internet.
1. Create a Public Hosted Zone for `internal.company`.
2. Create an `A` record for `api.internal.company` pointing to `52.14.80.20`.
3. Create a Private Hosted Zone with the exact same domain name `internal.company`, associated with your VPC.
4. Create an `A` record for `api.internal.company` pointing to `10.0.20.50`.
5. Test with `dig` from both an EC2 instance inside the VPC and an external machine.
</details>

<details>
<summary><strong>Exercise 2: Weighted Canary Release with Route 53</strong></summary>

**Goal**: Split traffic 80/20 between two distinct mock environments.
1. Deploy two sample Nginx servers with distinct response payloads (`v1` and `v2`).
2. Create two Route 53 records with identical name `canary.example.com`.
3. Assign `Weight: 80` with identifier `version-1` to the first instance.
4. Assign `Weight: 20` with identifier `version-2` to the second instance.
5. Set TTL to 10 seconds. Execute 100 consecutive `curl` commands and analyze response ratios.
</details>

<details>
<summary><strong>Exercise 3: Automated Failover with CloudWatch Alarm Health Check</strong></summary>

**Goal**: Trigger DNS failover when application error rate spikes.
1. Create a CloudWatch Alarm monitoring ALB `HTTPCode_Target_5XX_Count`.
2. Create a Route 53 Calculated Health Check that triggers based on the CloudWatch Alarm status.
3. Attach this health check to a Primary Failover record.
4. Trigger artificial 500 errors to fire the alarm and verify Route 53 shifts DNS queries to the secondary DR record.
</details>

<details>
<summary><strong>Exercise 4: Private Hosted Zone Cross-Account Peering</strong></summary>

**Goal**: Share a private zone across two distinct AWS accounts.
1. In Account A, execute `aws route53 create-vpc-association-authorization`.
2. In Account B, execute `aws route53 associate-vpc-with-hosted-zone`.
3. Verify name resolution using `nslookup` from an EC2 instance in Account B.
4. Clean up the authorization using `delete-vpc-association-authorization`.
</details>

<details>
<summary><strong>Exercise 5: Route 53 Resolver DNS Firewall Rule Group</strong></summary>

**Goal**: Block outbound VPC communication to command-and-control phishing domains.
1. Navigate to Route 53 Resolver -> DNS Firewall -> Rule Groups.
2. Create a rule group blocking the AWS-managed threat list `AWSManagedDomainsMalwareDomainList`.
3. Associate the rule group with your production VPC.
4. Attempt to resolve a test domain in the malware list from an EC2 instance; verify it returns `NODATA` or a sinkhole response.
</details>

<details>
<summary><strong>Exercise 6: DNSSEC Validation with Route 53 and DIG</strong></summary>

**Goal**: Cryptographically verify a signed domain.
1. Enable DNSSEC on a test hosted zone in Route 53 using AWS KMS.
2. Publish the DS record at the registrar.
3. Run `dig +dnssec +multiline api.yourdomain.com` from a Linux terminal.
4. Verify the presence of the `RRSIG` record and the `ad` (Authenticated Data) flag in the DNS header response.
</details>

---

## 12. Summary & Quick Reference

```
+---------------------------------------------------------------------------------------------------+
| ROUTE 53 ROUTING POLICY SELECTION MATRIX                                                          |
+----------------------+-------------------------------------+--------------------------------------+
| Policy               | Primary Use Case                    | Key Mechanism                        |
+----------------------+-------------------------------------+--------------------------------------+
| Simple               | Single resource / Basic round-robin | 1 record or random client-side list  |
| Weighted             | Canary rollouts / Load testing      | Percentage weights (0 - 255)         |
| Latency-Based        | Global low-latency response         | Lowest network RTT to AWS regions    |
| Failover             | Active / Passive Disaster Recovery  | Driven by Route 53 Health Checks     |
| Geolocation          | Compliance / Legal / Localization   | Client continent / country / state   |
| Geoproximity         | Geographic distance + bias tuning   | Traffic Flow visual policy engine    |
| Multivalue Answer    | Simple resilient load balancing     | Returns up to 8 healthy A records    |
| IP-Based             | Client network segment routing      | Matches client source IP CIDR blocks |
+----------------------+-------------------------------------+--------------------------------------+
```

- **Alias Record**: Free DNS queries for AWS resources; supports Zone Apex.
- **Resolver**: `10.0.0.2` handles local VPC resolution; Inbound/Outbound endpoints enable hybrid corporate DNS.
- **Failover**: Pair Route 53 Health Checks with `EvaluateTargetHealth: true` for zero-downtime resilience.

---

## 13. Further Reading

- [AWS Route 53 Developer Guide](https://docs.aws.amazon.com/Route53/latest/DeveloperGuide/)
- [RFC 1034: Domain Names - Concepts and Facilities](https://datatracker.ietf.org/doc/html/rfc1034)
- [RFC 1035: Domain Names - Implementation and Specification](https://datatracker.ietf.org/doc/html/rfc1035)
- [Route 53 Resolver Architecture & Hybrid DNS](https://aws.amazon.com/route53/resolver/)
- [AWS Well-Architected Framework: Reliability Pillar](https://docs.aws.amazon.com/wellarchitected/latest/reliability-pillar/)
- [04-networking/01-vpc.md](file:///04-devops/aws/04-networking/01-vpc.md)
