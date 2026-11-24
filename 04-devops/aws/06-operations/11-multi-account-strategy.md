# Multi-Account Architecture & Landing Zones: Organizations & Control Tower

> **Module**: `06-operations`  
> **Topic**: Enterprise OU Structure, AWS Control Tower, Landing Zones, Service Control Policies & RAM  
> **Target Audience**: Principal Cloud Architects, Security Directors, Lead DevOps Engineers  
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

In the early stages of cloud adoption, organizations often deploy all their environments—development, staging, and production—inside a single AWS account. While simple initially, this single-account model introduces catastrophic operational and security risks:
- **Massive Blast Radius**: A mistake made by a junior developer testing an IAM script or deleting a test database in development can inadvertently destroy the production database.
- **Security & Privilege Escalation**: Enforcing least-privilege IAM policies across dozens of teams and compliance tiers inside one account is practically impossible.
- **Service Quota Contention**: A load-testing job in staging exhausts the account-level API Gateway or EC2 limit, starving production of capacity.
- **Financial Ambiguity**: Disentangling blended shared costs across multiple business units becomes an accounting nightmare.

**An Enterprise Multi-Account Strategy** uses AWS accounts as the primary boundary for security, isolation, and financial accounting. Managed centrally via **AWS Organizations** and automated using **AWS Control Tower**, a multi-account landing zone provides isolated sandboxes, dedicated security and logging accounts, centralized network egress hubs, and automated compliance guardrails.

---

## 2. Core Concepts

### 2.1 AWS Organizations & Hierarchy
- **Management Account (Root)**: The parent account that pays the consolidated bill and controls the organization. **Must not host application workloads.**
- **Organizational Units (OUs)**: Hierarchical containers grouping accounts to apply uniform governance policies:
  - *Core / Foundation OU*: Shared infrastructure services.
  - *Security OU*: Security Tooling (GuardDuty/Security Hub) and Log Archive accounts.
  - *Infrastructure / Network OU*: Shared Transit Gateway, Direct Connect, and centralized egress inspection.
  - *Workloads OU*: Business applications separated into sub-OUs (`Prod`, `Non-Prod`).
  - *Sandbox OU*: Isolated environments for developer experimentation with automated budget caps and detached corporate networks.
  - *Policy Staging OU*: Used to test new Service Control Policies (SCPs) before deploying them globally.

### 2.2 Service Control Policies (SCPs)
- The highest level of policy governance in AWS.
- SCPs do **NOT** grant permissions; they define the **maximum permissions boundary** for IAM entities within the accounts or OUs they are attached to.
- If an SCP explicitly denies an action (e.g., `Deny ec2:RunInstances in unapproved regions`), **nobody—not even the root user or an AdministratorAccess role in that account—can perform that action**.

### 2.3 AWS Control Tower
- A fully managed service that automates the setup of a secure, multi-account AWS environment (a **Landing Zone**) based on AWS best practices.
- **Core Components**:
  - *Account Factory*: Standardized, automated account vending machine provisioning pre-configured, baseline-compliant accounts via Service Catalog and CodePipeline.
  - *Controls (formerly Guardrails)*: Preventive (enforced via SCPs), Detective (monitored via AWS Config rules), and Proactive (validated via CloudFormation hooks).

### 2.4 Dedicated Shared Service Accounts
1. **Log Archive Account**: Central repository for immutable security logs. All member accounts stream AWS CloudTrail, VPC Flow Logs, and S3 access logs here into a write-once S3 bucket protected by S3 Object Lock.
2. **Security Tooling Account**: The Delegated Administrator account for Amazon GuardDuty, AWS Security Hub, Amazon Inspector, and IAM Access Analyzer.
3. **Network Hub Account**: Houses centralized AWS Transit Gateways, Gateway Load Balancers, and AWS Network Firewalls to inspect and route all inbound and outbound enterprise traffic.

### 2.5 AWS Resource Access Manager (RAM)
- Allows accounts in an organization to securely share resources (e.g., Transit Gateways, VPC Subnets, Route 53 Resolver Rules, License Manager licenses) without duplicating them or configuring complex cross-account roles.

---

## 3. How It Works Internally

```
+---------------------------------------------------------------------------------------------------+
|                        AWS ENTERPRISE MULTI-ACCOUNT LANDING ZONE TOPOLOGY                         |
|                                                                                                   |
|  +---------------------------------------------------------------------------------------------+  |
|  | AWS ORGANIZATIONS MANAGEMENT ROOT ACCOUNT (Consolidated Billing, SCP Enforcement)          |  |
|  +---------------------------------------------------------------------------------------------+  |
|         |                                              |                                          |
|         v                                              v                                          |
|  +-----------------------------+               +-----------------------------------------------+  |
|  | SECURITY ORGANIZATIONAL UNIT|               | INFRASTRUCTURE ORGANIZATIONAL UNIT            |  |
|  |                             |               |                                               |  |
|  |  +-----------------------+  |               |  +-----------------------------------------+  |  |
|  |  | LOG ARCHIVE ACCOUNT   |  |               |  | NETWORK HUB ACCOUNT                     |  |  |
|  |  | - S3 Immutable Vault  |  |               |  | - AWS Transit Gateway (TGW)             |  |  |
|  |  | - CloudTrail Aggregat.|  |               |  | - AWS Network Firewall Central Egress   |  |  |
|  |  +-----------------------+  |               |  | - Direct Connect Gateway Connection     |  |  |
|  |                             |               |  +--------------------+--------------------+  |  |
|  |  +-----------------------+  |               +-----------------------|-----------------------+  |
|  |  | SECURITY TOOLING ACCT |  |                                       |                          |
|  |  | - Delegated Admin for |  |                                       | (VPC Subnets Shared via  |
|  |  |   GuardDuty & Hub     |  |                                       |  AWS RAM)                |
|  |  +-----------------------+  |                                       |                          |
|  +-----------------------------+                                       |                          |
|                                                                        v                          |
|  +---------------------------------------------------------------------------------------------+  |
|  | WORKLOADS ORGANIZATIONAL UNIT                                                               |  |
|  |                                                                                             |  |
|  |    NON-PRODUCTION SUB-OU                           PRODUCTION SUB-OU                        |  |
|  |    +---------------------------+                   +---------------------------+            |  |
|  |    | DEV / QA ACCOUNT          |                   | PRODUCTION ACCOUNT        |            |  |
|  |    | - App Workloads (EC2/ECS) |                   | - Prod Workloads (Aurora) |            |  |
|  |    | - Attached to TGW via RAM |                   | - Zero Public Subnets     |            |  |
|  |    +---------------------------+                   +---------------------------+            |  |
|  +---------------------------------------------------------------------------------------------+  |
+---------------------------------------------------------------------------------------------------+
```

---

## 4. Syntax & Usage

### 4.1 CLI Commands for AWS Organizations and SCPs

```bash
# 1. Create a new Organizational Unit (OU)
aws organizations create-organizational-unit \
    --parent-id "r-1234" \
    --name "Workloads-Production"

# 2. Create a Service Control Policy (SCP) restricting regions
aws organizations create-policy \
    --name "DenyUnapprovedRegions" \
    --description "Blocks launching resources outside us-east-1 and eu-west-1" \
    --type "SERVICE_CONTROL_POLICY" \
    --content file://deny-unapproved-regions-scp.json

# 3. Attach the SCP to an Organizational Unit
aws organizations attach-policy \
    --policy-id "p-0123456789abcdef" \
    --target-id "ou-1234-98765432"
```

---

## 5. Practical Examples

### 5.1 Service Control Policy: Restrict Allowed AWS Regions (JSON)

```json
{
  "Version": "2012-10-17",
  "Statement": [
    {
      "Sid": "DenyAllOutsideApprovedRegions",
      "Effect": "Deny",
      "NotAction": [
        "cloudfront:*",
        "iam:*",
        "route53:*",
        "support:*",
        "wafv2:*",
        "organizations:*",
        "artifact:*"
      ],
      "Resource": "*",
      "Condition": {
        "StringNotEquals": {
          "aws:RequestedRegion": [
            "us-east-1",
            "eu-west-1"
          ]
        }
      }
    }
  ]
}
```

### 5.2 Service Control Policy: Prevent Deletion of CloudWatch Logs & KMS Keys

```json
{
  "Version": "2012-10-17",
  "Statement": [
    {
      "Sid": "ProtectSecurityLogAndKeyDeletion",
      "Effect": "Deny",
      "Action": [
        "logs:DeleteLogGroup",
        "logs:DeleteLogStream",
        "kms:ScheduleKeyDeletion",
        "kms:DeleteCustomKeyStore"
      ],
      "Resource": "*",
      "Condition": {
        "ArnNotLike": {
          "aws:PrincipalArn": [
            "arn:aws:iam::*:role/AWSControlTowerExecution",
            "arn:aws:iam::*:role/EmergencyBreakGlassAdmin"
          ]
        }
      }
    }
  ]
}
```

### 5.3 AWS Resource Access Manager (RAM) Sharing VPC Subnets (Terraform)

```hcl
# In the Network Hub Account: Share private subnets with the entire Organization
resource "aws_ram_resource_share" "network_share" {
  name                      = "corp-shared-vpc-subnets"
  allow_external_principals = false

  tags = {
    Environment = "production"
  }
}

# Associate Private Subnets with RAM Share
resource "aws_ram_resource_association" "subnet_assoc" {
  resource_arn       = aws_subnet.shared_private_subnet_a.arn
  resource_share_arn = aws_ram_resource_share.network_share.arn
}

# Share directly with the Workloads Organizational Unit ARN
resource "aws_ram_principal_association" "org_principal" {
  principal          = "arn:aws:organizations::123456789012:ou/o-abcd123456/ou-1234-98765432"
  resource_share_arn = aws_ram_resource_share.network_share.arn
}
```

### 5.4 Centralized S3 Log Bucket Policy for Multi-Account Logging

```json
{
  "Version": "2012-10-17",
  "Statement": [
    {
      "Sid": "AllowAWSCloudTrailAcrossOrganization",
      "Effect": "Allow",
      "Principal": { "Service": "cloudtrail.amazonaws.com" },
      "Action": "s3:PutObject",
      "Resource": "arn:aws:s3:::central-log-archive-12345678/AWSLogs/*",
      "Condition": {
        "StringEquals": {
          "aws:SourceOrgID": "o-abcd123456",
          "s3:x-amz-acl": "bucket-owner-full-control"
        }
      }
    }
  ]
}
```

---

## 6. Best Practices

1. **Keep the Management Account Free of Workloads**: Never run applications, databases, or developer compute inside the AWS Organizations Management account. If the management account is compromised, the entire organization is compromised.
2. **Always Use `aws:SourceOrgID` in Resource Policies**: When authoring cross-account S3 bucket policies or KMS key policies, use the `aws:SourceOrgID` condition key. This automatically grants access to all member accounts in your organization without manually updating account IDs every time a new account is created.
3. **Stage New SCPs in a Dedicated Test OU First**: Never attach an untracked, experimental SCP directly to the Root OU. A syntax error or accidental `Deny` condition can instantly lock out all production workloads across the entire enterprise.
4. **Automate Account Creation via Account Factory**: Never create accounts manually through the AWS console. Use Control Tower Account Factory or Terraform to provision accounts pre-wired with baseline VPCs, IAM Identity Center bindings, and logging configurations.
5. **Separate Non-Production and Production into Distinct Accounts**: Never rely on IAM policies or naming conventions to isolate dev from prod within the same account. Strict account boundaries guarantee zero blast radius.

---

## 7. Common Mistakes

| Mistake | Root Cause | Impact | Fix |
|---|---|---|---|
| Hosting Workloads in Management Account | Running EC2 or RDS instances inside the root billing account. | Catastrophic security blast radius; breaks landing zone compliance. | Migrate workloads to dedicated member accounts inside Workloads OU. |
| Locking Out Global Services with Region SCP | Denying all regions without exempting global services (`cloudfront`, `iam`). | IAM console breaks; CloudFront distributions cannot be updated. | Add global service exceptions to `NotAction` in Region SCP. |
| Disabling Full Duplication in Log Archive | Leaving logs inside individual member accounts. | Malicious attackers compromise account and purge CloudTrail logs. | Stream all logs to an air-gapped **Log Archive Account** with S3 Object Lock. |
| Hardcoding Account IDs in Sharing Policies | Updating S3/KMS policies with 50 individual account IDs. | Unmaintainable overhead; new accounts cannot access shared services. | Use the condition key `"StringEquals": {"aws:SourceOrgID": "o-xxxx"}`. |
| Over-Broad Sandbox Accounts | Creating developer sandbox accounts connected to corporate Direct Connect. | Compromised sandbox environment provides pivot path into internal datacenter. | Keep Sandbox OU completely isolated with zero VPC peering or corporate VPN routes. |
| Accidental Detachment of `FullAWSAccess` SCP | Detaching default `FullAWSAccess` before applying replacement policy. | All permissions inside the OU are instantly blocked; major outage. | Attach the replacement policy first, then modify or detach. |

---

## 8. Performance, Security & Cost Notes

### 8.1 Network Performance with VPC Sharing
- **VPC Sharing via AWS RAM**: Allows multiple accounts in the same organization to launch compute resources (EC2, ECS, Lambda) into subnets belonging to a centrally managed VPC in the Network Hub account. This simplifies network security while eliminating cross-account VPC peering bandwidth charges.

### 8.2 Security & Compliance
- **Preventive Guardrails**: SCPs are evaluated at the AWS hypervisor API layer before IAM identity policies. They cannot be bypassed by any local account administrator.

### 8.3 Cost Breakdown
- **AWS Organizations**: **100% Free**.
- **AWS Control Tower**: Free service orchestration (underlying services like AWS Config rules and CloudTrail are billed at standard rates).
- **Consolidated Billing Benefits**: Volume discount pricing tiers across all accounts are aggregated (e.g., S3 storage volume discounts and tiered data transfer rates are calculated across the total organizational spend).

---

## 9. Comparison Table

| Dimension | Single AWS Account | Multi-Account (AWS Organizations) |
|---|---|---|
| **Blast Radius** | Catastrophic (Cross-environment impact)| Contained within single member account |
| **Security Controls** | Complex IAM policies with high risk of leaks| Enforced via immutable Service Control Policies (SCPs)|
| **Cost Allocation** | Complex tagging showback required | Clean invoice separation per account/OU |
| **Service Quotas** | Shared across Dev, Test & Prod (Contention)| Dedicated independent quotas per account |
| **Compliance Auditing**| Difficult to prove environment separation | Simple: Accounts map 1-to-1 to compliance scopes |

---

## 10. Interview Questions & Answers

<details>
<summary><strong>Q1: What is the primary architectural purpose of a Service Control Policy (SCP) and how does it interact with IAM policies?</strong></summary>

An SCP is an organization-level governance policy that sets the **maximum permissions boundary** for all IAM users and roles in an account or Organizational Unit. 
- **Crucial Rule**: An SCP does **NOT** grant permissions. To perform an action, the principal must have an explicit **Allow** in an IAM policy, AND the action must be permitted by all inherited SCPs up to the Root.
- If an SCP explicitly **Denies** an action, that action is completely blocked for all users, roles, and even the root user in that account, regardless of any `AdministratorAccess` IAM policies granted locally.
</details>

<details>
<summary><strong>Q2: Why must the AWS Organizations Management (Root) Account be strictly isolated from workloads?</strong></summary>

1. **Unconstrained Authority**: Service Control Policies (SCPs) do **not** apply to the Management Account. Any misconfiguration or compromise in the management account cannot be restricted by organizational guardrails.
2. **Blast Radius**: The management account controls consolidated billing, account creation, and organization-wide deletion. A compromise grants the attacker full root authority to destroy all member accounts.
3. **Compliance Requirements**: CIS AWS Foundations and AWS Landing Zone standards mandate that the management account is used exclusively for organization governance, consolidated billing, and SCP management.
</details>

<details>
<summary><strong>Q3: Explain the role and security design of the Log Archive Account.</strong></summary>

The Log Archive Account is a dedicated security account designed to serve as an immutable, tamper-proof repository for all enterprise telemetry:
- **Centralized S3 Bucket**: All member accounts stream their AWS CloudTrail management events, VPC Flow Logs, and S3 access logs into this bucket.
- **Air-Gapping**: Developers and account administrators in workload accounts have zero IAM permissions to access, view, or modify the Log Archive account.
- **Immutability via S3 Object Lock**: The bucket enforces S3 Object Lock in **Compliance Mode** (WORM), ensuring that even if an attacker compromises an administrator credential, audit logs cannot be modified, encrypted by ransomware, or deleted before the retention window expires.
</details>

<details>
<summary><strong>Q4: How does AWS Resource Access Manager (RAM) simplify multi-account network design?</strong></summary>

Historically, sharing network connectivity across 50 accounts required configuring 50 individual VPC Peering connections or transit attachments. 
With **AWS RAM**:
- The centralized **Network Hub Account** provisions a shared VPC or an AWS Transit Gateway.
- The administrator shares the private subnets with an entire Organizational Unit (OU) via RAM.
- Application teams in member accounts can launch EC2 instances and ECS tasks directly into the shared subnets without managing VPCs, internet gateways, or route tables, while maintaining complete network isolation.
</details>

<details>
<summary><strong>Q5: What is the difference between a Preventive Control and a Detective Control in AWS Control Tower?</strong></summary>

- **Preventive Controls**: Implemented using **Service Control Policies (SCPs)**. They actively block non-compliant API actions before they happen (e.g., blocking the creation of unencrypted EBS volumes or blocking access outside approved regions). The API call fails with `AccessDenied`.
- **Detective Controls**: Implemented using **AWS Config Rules**. They monitor deployed resources continuously and flag violations after they occur (e.g., detecting if an S3 bucket has public read access enabled), generating an alert in Security Hub and triggering automated remediation.
</details>

<details>
<summary><strong>Q6: How does the condition key aws:SourceOrgID streamline cross-account resource policies?</strong></summary>

In large enterprises that launch dozens of new AWS accounts every month, maintaining S3 bucket policies or KMS key policies with individual 12-digit account numbers is unmaintainable. 
The **`aws:SourceOrgID`** condition key checks the caller's AWS Organization ID:
```json
"Condition": {
  "StringEquals": { "aws:SourceOrgID": "o-1234567890" }
}
```
Any account belonging to the organization automatically satisfies the policy condition; accounts outside the organization are rejected, eliminating manual policy maintenance.
</details>

<details>
<summary><strong>Q7: What is an AWS Control Tower Account Factory?</strong></summary>

Account Factory is a pre-configured account vending machine built into AWS Control Tower. It standardizes account provisioning:
When a team requests a new account via AWS Service Catalog or Terraform:
1. Account Factory creates the member account in AWS Organizations.
2. Moves it to the designated Organizational Unit.
3. Automatically applies required SCPs, provisions baseline VPCs, enables AWS Config rules, and establishes IAM Identity Center SSO roles.
</details>

<details>
<summary><strong>Q8: What happens if an SCP contains an explicit Deny on an action, but an IAM policy has an explicit Allow?</strong></summary>

In AWS policy evaluation logic, an **Explicit Deny always overrides any Allow**. 
The request is immediately denied. An action is permitted only if there is an explicit `Allow` in the principal's identity policy AND no explicit `Deny` exists in any attached identity policies, resource-based policies, permissions boundaries, or inherited Service Control Policies.
</details>

---

## 11. Practice Exercises

<details>
<summary><strong>Exercise 1: Create an Organizational Unit and Member Account via CLI</strong></summary>

**Goal**: Structure an organizational hierarchy.
1. Create an OU named `SandboxOU`: `aws organizations create-organizational-unit ...`.
2. Create a test account inside the OU: `aws organizations create-account --email <unique-email> --account-name "DevSandbox1"`.
3. Verify the account creation status.
</details>

<details>
<summary><strong>Exercise 2: Author and Test a Region-Restricting SCP</strong></summary>

**Goal**: Block resource creation in unapproved regions.
1. Write an SCP that denies `ec2:RunInstances` unless `aws:RequestedRegion` is `us-east-1`.
2. Attach the SCP to a test sandbox account.
3. Attempt to launch an EC2 instance in `eu-west-1` from the sandbox account; verify access is denied by SCP.
</details>

<details>
<summary><strong>Exercise 3: Share a Subnet Across Accounts with AWS RAM</strong></summary>

**Goal**: Practice centralized network sharing.
1. In Account A, create a VPC and private subnet.
2. Create an AWS RAM resource share targeting Account B.
3. In Account B, navigate to the VPC console and verify the shared subnet is visible.
4. Launch an EC2 instance into the shared subnet from Account B.
</details>

<details>
<summary><strong>Exercise 4: Verify Multi-Account Organization ID in S3 Bucket Policy</strong></summary>

**Goal**: Enforce organization-wide access control.
1. In a central account, create an S3 bucket.
2. Apply a policy allowing `s3:PutObject` with condition `"StringEquals": {"aws:SourceOrgID": "<your-org-id>"}`.
3. Attempt an upload from a member account within the org (confirm success).
4. Attempt from an external test account (confirm access denied).
</details>

<details>
<summary><strong>Exercise 5: Inspect AWS Control Tower Landing Zone Controls</strong></summary>

**Goal**: Review enterprise guardrail posture.
1. Open the AWS Control Tower console.
2. Navigate to Controls -> All Controls.
3. Inspect the mandatory control: "Disallow public read access to Amazon S3 buckets".
4. Review the underlying SCP and AWS Config rule implementations.
</details>

<details>
<summary><strong>Exercise 6: Test SCP Exemption with Break-Glass Role</strong></summary>

**Goal**: Author safe emergency break-glass exceptions.
1. Write an SCP with a `Deny` statement that contains a `Condition: ArnNotLike: aws:PrincipalArn: "arn:aws:iam::*:role/BreakGlassRole"`.
2. Verify normal admin roles are blocked by the SCP.
3. Assume `BreakGlassRole` and confirm the action succeeds.
</details>

---

## 12. Summary & Quick Reference

```
+---------------------------------------------------------------------------------------------------+
| MULTI-ACCOUNT LANDING ZONE CORE ACCOUNTS & OUS                                                    |
+--------------------------+-----------------------+------------------------------------------------+
| Account / OU             | Purpose               | Security Boundary / Safeguard                  |
+--------------------------+-----------------------+------------------------------------------------+
| Management Account       | Root Billing & SCPs   | ZERO application workloads allowed             |
| Security Tooling Account | Threat Detection Hub  | Delegated Admin for GuardDuty & Security Hub   |
| Log Archive Account      | Immutable Audit Vault | S3 Object Lock (WORM); air-gapped from devs    |
| Network Hub Account      | Central Transit & Egress| AWS Transit Gateway, Direct Connect & Firewall |
| Workloads (Prod / Dev)   | Application Hosting   | 100% isolated accounts per environment stage   |
| Sandbox OU               | Developer R&D         | Budget capped; zero corporate network routes   |
+--------------------------+-----------------------+------------------------------------------------+
```

- **Account as Security Boundary**: Eliminates cross-environment blast radius.
- **SCPs**: Unbypassable guardrails setting maximum permissions across OUs.
- **Resource Access Manager (RAM)**: Securely shares centralized networking without duplication.

---

## 13. Further Reading

- [AWS Organizations User Guide](https://docs.aws.amazon.com/organizations/latest/userguide/orgs_introduction.html)
- [AWS Control Tower User Guide](https://docs.aws.amazon.com/controltower/latest/userguide/what-is-control-tower.html)
- [Organizing Your AWS Environment Using Multiple Accounts (Whitepaper)](https://docs.aws.amazon.com/whitepapers/latest/organizing-your-aws-environment/organizing-your-aws-environment.html)
- [AWS Security Reference Architecture (AWS SRA)](https://docs.aws.amazon.com/prescriptive-guidance/latest/security-reference-architecture/architecture.html)
- [06-operations/07-security-services.md](file:///04-devops/aws/06-operations/07-security-services.md)
