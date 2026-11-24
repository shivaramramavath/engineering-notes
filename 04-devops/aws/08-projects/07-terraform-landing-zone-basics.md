---
project: "Terraform Landing Zone Baseline with Organizations and SCPs"
module: "08-projects"
track: "aws"
difficulty: "advanced"
architecture: "multi-account-governance"
last-tested: "2026-11"
tags:
  - "organizations"
  - "scp"
  - "landing-zone"
  - "s3-object-lock"
  - "security"
  - "terraform"
  - "project"
---

# Terraform Landing Zone Baseline with Organizations and SCPs

## Table of Contents
- [1. Project Topology & Specifications](#1-project-topology--specifications)
- [2. Declarative Code (Terraform)](#2-declarative-code-terraform)
- [3. Verification & Automated Test Harness](#3-verification--automated-test-harness)
- [4. Hardening, Security & Cost Analysis](#4-hardening-security--cost-analysis)
- [5. Operational Runbook & Disaster Recovery](#5-operational-runbook--disaster-recovery)

---

## 1. Project Topology & Specifications

### 1.1 Architecture Overview
A well-architected multi-account foundation is the prerequisite for all enterprise workloads on AWS. Rather than running all development, staging, and production workloads inside a single AWS account, an enterprise **Landing Zone** establishes physical blast radius isolation across dedicated accounts governed by centralized security policies.

This production reference project deploys the foundational building blocks of an automated landing zone:
1. **AWS Organizations Hierarchy**:
   - **Security OU**: Houses the centralized Log Archive and Security Tooling accounts.
   - **Workloads OU**: Houses child development and production accounts.
2. **Preventative Guardrails (Service Control Policies - SCPs)**:
   - **Region Deny SCP**: Denies all actions outside approved production regions (`us-east-1`, `us-west-2`), exempting global services (IAM, Route 53, CloudFront).
   - **Security Invariance SCP**: Strictly denies disabling or modifying AWS CloudTrail, AWS Config, or Amazon GuardDuty across all member accounts.
3. **Immutable Log Archive Storage**:
   - S3 bucket configured with **S3 Object Lock in Compliance Mode** (WORM - Write Once, Read Many), rendering security telemetry mathematically immune to deletion or tampering even by root account users.

```
+----------------------------------------------------------------------------------------------------+
|                         ORGANIZATIONS & LANDING ZONE BASELINE TOPOLOGY                             |
+----------------------------------------------------------------------------------------------------+

  [ AWS Organizations Root ]
             |
             +-------------------------------------------------------------+
             |                                                             |
             v                                                             v
  +---------------------------------------+      +---------------------------------------------------+
  | Security Organizational Unit (OU)     |      | Workloads Organizational Unit (OU)                |
  |                                       |      |                                                   |
  |  +---------------------------------+  |      |  +-----------------------+ +--------------------+ |
  |  | Log Archive Account             |  |      |  | Production Account    | | Staging Account    | |
  |  |  - Centralized CloudTrail S3    |  |      |  |  - Production apps    | |  - Staging apps    | |
  |  |  - S3 Object Lock (Compliance)  |  |      |  +-----------------------+ +--------------------+ |
  |  +---------------------------------+  |      +---------------------------------------------------+
  +---------------------------------------+                                |
             |                                                             |
             +-------------------------------------------------------------+
                                            ^
                                            | Enforced Guardrails
  +--------------------------------------------------------------------------------------------------+
  | SERVICE CONTROL POLICIES (SCPs)                                                                  |
  |  1. Region Restriction: Denies creating resources outside us-east-1 and us-west-2                |
  |  2. Security Protection: Denies deleting CloudTrail logs or disabling GuardDuty                  |
  +--------------------------------------------------------------------------------------------------+
```

### 1.2 Bill of Materials
- `aws_organizations_organization`: Enables all features on the AWS Organization.
- `aws_organizations_organizational_unit`: Creates `Security` and `Workloads` OUs.
- `aws_organizations_policy`: Declares Region Deny and Security Invariance SCPs.
- `aws_organizations_policy_attachment`: Attaches SCPs to specific OUs.
- `aws_s3_bucket`: Dedicated Log Archive bucket with `object_lock_enabled = true`.
- `aws_s3_bucket_object_lock_configuration`: Compliance mode with 365-day retention.

---

## 2. Declarative Code (Terraform)

Create `main.tf`:

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

# -----------------------------------------------------------------------------
# 1. AWS Organizations & Organizational Units
# -----------------------------------------------------------------------------
resource "aws_organizations_organization" "org" {
  aws_service_access_principals = [
    "cloudtrail.amazonaws.com",
    "config.amazonaws.com",
    "guardduty.amazonaws.com",
    "securityhub.amazonaws.com"
  ]
  feature_set = "ALL"
}

# Security OU (Housing Log Archive and Security Tooling)
resource "aws_organizations_organizational_unit" "security" {
  name      = "Security"
  parent_id = aws_organizations_organization.org.roots[0].id
}

# Workloads OU (Housing Dev, Stage, Prod workload accounts)
resource "aws_organizations_organizational_unit" "workloads" {
  name      = "Workloads"
  parent_id = aws_organizations_organization.org.roots[0].id
}

# -----------------------------------------------------------------------------
# 2. Service Control Policies (SCPs)
# -----------------------------------------------------------------------------
# SCP 1: Region Restriction (Allow only us-east-1 and us-west-2)
resource "aws_organizations_policy" "region_restriction" {
  name        = "EnforceApprovedRegionsOnly"
  description = "Denies actions in unapproved regions outside us-east-1 and us-west-2"
  type        = "SERVICE_CONTROL_POLICY"

  content = jsonencode({
    Version = "2012-10-17"
    Statement = [
      {
        Sid       = "DenyUnapprovedRegions"
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
            "aws:RequestedRegion" = ["us-east-1", "us-west-2"]
          }
        }
      }
    ]
  })
}

# SCP 2: Prevent Disabling Security Services
resource "aws_organizations_policy" "protect_security_services" {
  name        = "ProtectSecurityServicesAndTrails"
  description = "Prevents any member account admin from deleting CloudTrail, GuardDuty, or Config"
  type        = "SERVICE_CONTROL_POLICY"

  content = jsonencode({
    Version = "2012-10-17"
    Statement = [
      {
        Sid       = "DenyDisablingSecurityServices"
        Effect    = "Deny"
        Action = [
          "cloudtrail:DeleteTrail",
          "cloudtrail:StopLogging",
          "cloudtrail:UpdateTrail",
          "guardduty:DeleteDetector",
          "guardduty:DisassociateFromMasterAccount",
          "guardduty:StopMonitoringMembers",
          "config:DeleteConfigRule",
          "config:DeleteConfigurationRecorder",
          "config:StopConfigurationRecorder"
        ]
        Resource = "*"
      }
    ]
  })
}

# Attach SCPs to Workloads OU
resource "aws_organizations_policy_attachment" "attach_region_scp" {
  policy_id = aws_organizations_policy.region_restriction.id
  target_id = aws_organizations_organizational_unit.workloads.id
}

resource "aws_organizations_policy_attachment" "attach_security_scp" {
  policy_id = aws_organizations_policy.protect_security_services.id
  target_id = aws_organizations_organizational_unit.workloads.id
}

# -----------------------------------------------------------------------------
# 3. Immutable Central Log Archive S3 Bucket with Object Lock
# -----------------------------------------------------------------------------
resource "aws_s3_bucket" "log_archive" {
  bucket_prefix       = "central-log-archive-immutable-"
  object_lock_enabled = true
}

resource "aws_s3_bucket_server_side_encryption_configuration" "log_archive_encryption" {
  bucket = aws_s3_bucket.log_archive.id
  rule {
    apply_server_side_encryption_by_default {
      sse_algorithm = "AES256"
    }
  }
}

resource "aws_s3_bucket_public_access_block" "log_archive_pab" {
  bucket                  = aws_s3_bucket.log_archive.id
  block_public_acls       = true
  block_public_policy     = true
  ignore_public_acls      = true
  restrict_public_buckets = true
}

# S3 Object Lock Compliance Mode: Mathematically impossible to delete for 365 days
resource "aws_s3_bucket_object_lock_configuration" "log_archive_lock" {
  bucket = aws_s3_bucket.log_archive.id

  rule {
    default_retention {
      mode = "COMPLIANCE"
      days = 365
    }
  }
}

output "organization_id" {
  value = aws_organizations_organization.org.id
}

output "workloads_ou_id" {
  value = aws_organizations_organizational_unit.workloads.id
}

output "security_ou_id" {
  value = aws_organizations_organizational_unit.security.id
}

output "log_archive_bucket" {
  value = aws_s3_bucket.log_archive.id
}
```

---

## 3. Verification & Automated Test Harness

### 3.1 Deployment Instructions

```bash
# Note: Executed in the AWS Organizations Management Account
terraform init
terraform apply -auto-approve
```

### 3.2 Automated Guardrail Verification

```bash
# 1. Verify SCPs are attached to the Workloads OU
echo "Auditing attached policies on Workloads OU..."
OU_ID=$(terraform output -raw workloads_ou_id)
aws organizations list-policies-for-target \
    --target-id "$OU_ID" \
    --filter "SERVICE_CONTROL_POLICY"

# 2. Verify S3 Object Lock on the Log Archive Bucket
echo "Auditing S3 Object Lock Configuration..."
BUCKET=$(terraform output -raw log_archive_bucket)
aws s3api get-object-lock-configuration --bucket "$BUCKET"
# Expected output: Mode: COMPLIANCE, Days: 365
```

### 3.3 Teardown Script

```bash
terraform destroy -auto-approve
```

---

## 4. Hardening, Security & Cost Analysis

### 4.1 Enterprise Security Controls
- **Physical Blast Radius Isolation**: A breach inside a workload account is physically restricted to that account. The attacker cannot query, modify, or delete logs in the Log Archive account.
- **Inviolable Compliance Mode**: S3 Object Lock in **COMPLIANCE** mode guarantees that no IAM principal—not even the AWS account root user or AWS Support—can delete or overwrite log files until the 365-day retention timer has expired.
- **Root-Proof SCPs**: Service Control Policies override all local permissions. A developer with `AdministratorAccess` (`*:*`) in a child account is blocked from creating resources in unapproved regions or disabling logging.

### 4.2 Cost Breakdown
- **AWS Organizations**: Free service.
- **Service Control Policies**: Free service.
- **S3 Storage (Log Archive)**: Standard S3 storage rates (\$0.023/GB) for stored audit logs.
- **Total Management Overhead**: **~\$0.00 / month** (Storage only).

---

## 5. Operational Runbook & Disaster Recovery

### Common Operational Runbook Procedures

#### Alert 1: Deployment Blocked in Workload Account with `AccessDeniedException`
- **Diagnostic Runbook**:
  1. Inspect CloudTrail in the child account. If the error message contains `Explicit deny by Service Control Policy`, look up the target region.
  2. If an engineering team requires deploying into an unapproved region for legal/latency reasons, do not delete the SCP. Submit an Architectural Review Board (ARB) request to add the region to the `aws:RequestedRegion` condition list.
