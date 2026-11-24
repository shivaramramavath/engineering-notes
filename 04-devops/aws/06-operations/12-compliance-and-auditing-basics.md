# AWS Compliance, Governance & Continuous Auditing: CloudTrail, Config & Audit Manager

> **Module**: `06-operations`  
> **Topic**: Immutable Audit Trails, AWS Config Rules, Auto-Remediation & Compliance Evidence Collection  
> **Target Audience**: Compliance Officers, Security Engineers, Cloud Governance Specialists  
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

In regulated industries (Financial Services, Healthcare, Government, SaaS), organizations must comply with stringent regulatory frameworks such as **SOC 2 Type II**, **PCI-DSS**, **HIPAA**, **ISO/IEC 27001**, and **FedRAMP**. In traditional on-premises environments, preparing for an audit required weeks of manual labor: taking screenshots of firewall rules, pulling server logs into spreadsheets, and interviewing engineers to prove that backups were running.

AWS transforms compliance from a manual, periodic snapshot into an **automated, continuous compliance pipeline**:
1. **AWS CloudTrail**: Records every single API call made across your AWS infrastructure, providing an immutable audit record of "Who did what, where, and when."
2. **AWS Config**: Continuously monitors, records, and evaluates the configurations of all your AWS resources against desired baseline rules, providing automated self-healing remediation.
3. **AWS Audit Manager**: Automatically maps your AWS resource usage, CloudTrail logs, and Config checks directly to compliance control frameworks (SOC 2, HIPAA), continuously gathering evidence for auditors with a single click.

---

## 2. Core Concepts

### 2.1 AWS CloudTrail
- **The Record of Actions**: Logs every API interaction executed via the AWS Management Console, AWS CLI, SDKs, or internal service-to-service calls.
- **Event Types**:
  - *Management Events*: Control plane actions (e.g., `ec2:RunInstances`, `iam:CreateRole`, `s3:CreateBucket`). Enabled by default; first copy is free.
  - *Data Events*: High-volume resource data plane operations (e.g., `s3:GetObject`, `s3:PutObject`, `lambda:Invoke`). Incur charges; must be explicitly enabled.
  - *Insights Events*: Machine learning anomaly detection identifying unusual API call volume or error spikes (e.g., a burst of `AccessDenied` errors).
- **Log File Integrity Validation**: Uses SHA-256 hashing and RSA digital signatures to prove mathematically that log files in S3 have not been altered, tampered with, or deleted since CloudTrail delivered them.
- **CloudTrail Lake**: A managed, immutable SQL data lake that lets you run SQL queries directly on years of CloudTrail logs without setting up Athena, Glue, or S3 pipelines.

### 2.2 AWS Config
- **The Record of State & Configuration**: Acts as a time-machine for your cloud infrastructure, tracking every configuration change over time.
- **Configuration Items (CIs)**: A point-in-time record of a resource's attributes, relationships (e.g., which Security Group is attached to which ENI), and tags.
- **Rules**:
  - *AWS Managed Rules*: Pre-built checks (e.g., `s3-bucket-public-read-prohibited`, `encrypted-volumes`, `root-account-mfa-enabled`).
  - *Custom Rules*: Authored in Python/Node.js Lambda functions using the Rule Development Kit (RDK) or Guard DSL.
- **Conformance Packs**: A collection of AWS Config rules and remediation actions packaged into a single template representing a compliance standard (e.g., CIS AWS Foundations v1.4 Conformance Pack).
- **Automated Remediation**: Triggers an AWS Systems Manager (SSM) Automation document upon rule non-compliance (e.g., automatically applying a bucket policy that blocks public access when an open bucket is detected).

### 2.3 AWS Audit Manager
- Maps raw AWS telemetry (CloudTrail events, Config rule states, Security Hub findings) into standardized regulatory control sets.
- Automatically generates **Assessment Reports** with cryptographically verifiable evidence ready for external compliance auditors.

---

## 3. How It Works Internally

```
+---------------------------------------------------------------------------------------------------+
|                        AWS CONTINUOUS COMPLIANCE & AUTO-REMEDIATION ENGINE                        |
|                                                                                                   |
|  [Engineer / Script executes: `aws s3api put-bucket-acl --acl public-read`]                       |
|         |                                                                                         |
|         v (API Call Routed through AWS Control Plane)                                             |
|  +---------------------------------------------------------------------------------------------+  |
|  | AWS CLOUDTRAIL (Who, When, Where?)                                                         |  |
|  | - Records Event: `PutBucketAcl`, Caller: `arn:aws:iam::...:user/dev`, IP: `198.51.100.4`   |  |
|  | - Writes SHA-256 Digest & Delivers Log to Central Immutable S3 Log Archive                   |  |
|  +---------------------------------------------------------------------------------------------+  |
|         |                                                                                         |
|         v (Notifies AWS Config of S3 Configuration Change)                                        |
|  +---------------------------------------------------------------------------------------------+  |
|  | AWS CONFIG (State Tracking & Compliance Evaluation)                                         |  |
|  | 1. Generates Configuration Item (CI) recording the ACL change                               |  |
|  | 2. Evaluates Managed Rule: `s3-bucket-public-read-prohibited`                                |  |
|  | 3. Evaluation Result: NON_COMPLIANT!                                                        |  |
|  | 4. Emits Compliance Change Event to EventBridge & Updates Security Hub                       |  |
|  +---------------------------------------------------------------------------------------------+  |
|         |                                                                                         |
|         v (Automated Remediation Triggered)                                                       |
|  +---------------------------------------------------------------------------------------------+  |
|  | AWS SYSTEMS MANAGER (SSM) AUTOMATION RUNBOOK                                                |  |
|  | Document: `AWS-DisableS3BucketPublicReadWrite`                                              |  |
|  | Executes: `s3:PutBucketPublicAccessBlock` setting BlockAll = True                           |  |
|  +---------------------------------------------------------------------------------------------+  |
|         |                                                                                         |
|         v                                                                                         |
|  +---------------------------------------------------------------------------------------------+  |
|  | AWS AUDIT MANAGER (Evidence Collection)                                                     |  |
|  | - Automatically records: Non-compliance event + Remediation log + CloudTrail evidence       |  |
|  | - Mapped to SOC 2 Type II: "CC6.1 - Logical Access Controls"                                |  |
|  | - Included in final auditor export package                                                  |  |
|  +---------------------------------------------------------------------------------------------+  |
+---------------------------------------------------------------------------------------------------+
```

---

## 4. Syntax & Usage

### 4.1 Querying CloudTrail Lake via SQL

```bash
# Execute an immutable SQL query in CloudTrail Lake to find root user logins
aws cloudtrail start-query \
    --query-statement "SELECT eventTime, userIdentity.arn, sourceIPAddress 
                       FROM 12345678-1234-1234-1234-123456789012 
                       WHERE userIdentity.type = 'Root' 
                       AND eventTime > '2026-11-01 00:00:00' 
                       ORDER BY eventTime DESC"
```

### 4.2 CLI Commands for AWS Config

```bash
# 1. Start the AWS Config Configuration Recorder
aws configservice start-configuration-recorder \
    --configuration-recorder-name "default"

# 2. Check compliance status for a specific rule
aws configservice get-compliance-details-by-config-rule \
    --config-rule-name "s3-bucket-public-read-prohibited" \
    --compliance-types "NON_COMPLIANT"

# 3. Validate CloudTrail log file integrity
aws cloudtrail validate-logs \
    --trail-arn "arn:aws:cloudtrail:us-east-1:123456789012:trail/organization-multi-region-trail" \
    --start-time 2026-11-01T00:00:00Z
```

---

## 5. Practical Examples

### 5.1 Multi-Region Organization CloudTrail with Log Integrity & KMS (Terraform)

```hcl
# KMS Key for CloudTrail Log Encryption
resource "aws_kms_key" "cloudtrail_key" {
  description             = "CMK for CloudTrail Log File Encryption"
  deletion_window_in_days = 30
  enable_key_rotation     = true

  policy = jsonencode({
    Version = "2012-10-17"
    Statement = [
      {
        Sid       = "EnableIAMRootPermissions"
        Effect    = "Allow"
        Principal = { AWS = "arn:aws:iam::${var.account_id}:root" }
        Action    = "kms:*"
        Resource  = "*"
      },
      {
        Sid       = "AllowCloudTrailEncrypt"
        Effect    = "Allow"
        Principal = { Service = "cloudtrail.amazonaws.com" }
        Action    = ["kms:GenerateDataKey*", "kms:DescribeKey"]
        Resource  = "*"
      }
    ]
  })
}

# S3 Bucket for Centralized CloudTrail Logs
resource "aws_s3_bucket" "audit_bucket" {
  bucket = "central-cloudtrail-audit-vault-2026"
}

resource "aws_s3_bucket_versioning" "versioning" {
  bucket = aws_s3_bucket.audit_bucket.id
  versioning_configuration {
    status = "Enabled"
  }
}

# Multi-Region Organization CloudTrail
resource "aws_cloudtrail" "org_trail" {
  name                          = "enterprise-multi-region-audit-trail"
  s3_bucket_name                = aws_s3_bucket.audit_bucket.id
  kms_key_id                    = aws_kms_key.cloudtrail_key.arn
  include_global_service_events = true
  is_multi_region_trail         = true
  enable_log_file_validation    = true # Cryptographic SHA-256 digest validation
  is_organization_trail         = true # Covers all member accounts automatically

  tags = {
    Environment = "production"
    Compliance  = "SOC2-PCI"
  }
}
```

### 5.2 AWS Config Managed Rule with Automated SSM Remediation (Terraform)

```hcl
# AWS Config Managed Rule: S3 Bucket Public Read Prohibited
resource "aws_config_config_rule" "s3_public_read" {
  name = "s3-bucket-public-read-prohibited"

  source {
    owner             = "AWS"
    source_identifier = "S3_BUCKET_PUBLIC_READ_PROHIBITED"
  }
}

# Automated Remediation using SSM Automation Runbook
resource "aws_config_remediation_configuration" "s3_remediation" {
  config_rule_name = aws_config_config_rule.s3_public_read.name
  target_type      = "SSM_DOCUMENT"
  target_id        = "AWS-DisableS3BucketPublicReadWrite"
  target_version   = "1"

  automatic                  = true # Automatically execute remediation upon non-compliance
  maximum_automatic_attempts = 3
  retry_attempt_seconds      = 60

  parameter {
    name           = "BucketName"
    resource_value = "RESOURCE_ID"
  }

  parameter {
    name         = "AutomationAssumeRole"
    static_value = var.ssm_remediation_role_arn
  }
}
```

### 5.3 Deploying a Full Conformance Pack for CIS AWS Foundations Benchmark

```bash
# Deploy CIS AWS Foundations v1.4.0 conformance pack across the entire AWS Organization
aws configservice put-organization-conformance-pack \
    --organization-conformance-pack-name "CIS-AWS-Foundations-v1-4" \
    --template-s3-uri "s3://aws-config-conformance-packs-us-east-1/Operational-Best-Practices-for-CIS-AWS-Foundations-Benchmark-v1.4.yaml" \
    --delivery-s3-bucket "central-config-delivery-bucket-2026"
```

---

## 6. Best Practices

1. **Enable Multi-Region Trail with `include_global_service_events = true`**: Never configure single-region CloudTrails. Malicious attackers specifically target unmonitored AWS regions to launch unauthorized GPU instances or create backdoor IAM users.
2. **Always Enable Log File Validation**: Cryptographic log file validation generates digest files every hour containing SHA-256 signatures of delivered log files, providing incontrovertible proof to forensic investigators that logs were not modified.
3. **Stream CloudTrail Logs to Amazon CloudWatch Logs**: Deliver CloudTrail events to a CloudWatch Log Group with Metric Filters alerting instantly on high-risk events (e.g., Root account usage, changes to IAM policies, security group opening port 22, unauthorized API errors).
4. **Deploy Automated Remediation on Critical Config Rules**: Do not wait for quarterly compliance audits to discover open S3 buckets or unencrypted EBS volumes. Pair Config rules with SSM Automation documents to self-heal non-compliant resources within seconds.
5. **Protect CloudTrail S3 Bucket with S3 Object Lock**: Apply S3 Object Lock in **Compliance Mode** (WORM) on your central audit bucket. Even an account administrator or compromised root credential cannot delete or overwrite log files during the retention period.

---

## 7. Common Mistakes

| Mistake | Root Cause | Impact | Fix |
|---|---|---|---|
| Single-Region CloudTrail Trail | Enabling CloudTrail only in `us-east-1`. | Zero visibility into attacker actions in unmonitored regions (`eu-central-1`). | Enable `is_multi_region_trail: true`. |
| Disabling Log File Validation | Leaving `enable_log_file_validation` at default false. | Auditors reject logs; cannot prove logs were not tampered with. | Set `enable_log_file_validation = true`. |
| Excessive Data Events Billing | Enabling S3 Data Events (`s3:GetObject`) on high-traffic data lake. | Tens of thousands of dollars in CloudTrail data ingestion billing! | Scope data events to sensitive S3 prefixes; exclude read-heavy lakes. |
| Missing AWS Config Resource Recording | Configuration recorder not set to record all resources. | Config rules evaluate nothing; compliance status shows blank. | Set recorder `allSupported: true` and `includeGlobalResourceTypes: true`. |
| Inadequate KMS Key Policy for CloudTrail | CloudTrail bucket encrypted with KMS, but key policy lacks CloudTrail principal. | CloudTrail silently fails to deliver logs to S3. | Grant `kms:GenerateDataKey*` to `cloudtrail.amazonaws.com` in key policy. |
| Forgetting to Enable Organization Trail | Deploying individual CloudTrails in 50 separate member accounts. | High management overhead; member account admins can disable trails. | Enable a single centralized **Organization Trail** in management account. |

---

## 8. Performance, Security & Cost Notes

### 8.1 Performance Impact
- **Zero Latency Overhead**: CloudTrail and AWS Config operate asynchronously out-of-band. Capturing API events and configuration snapshots does not add latency to underlying AWS API calls.

### 8.2 Security & Tamper-Proofing
- **Air-Gapped S3 Storage**: Deliver CloudTrail and Config snapshots to an air-gapped **Log Archive Account** inaccessible to developers and workload administrators.

### 8.3 Cost Breakdown
- **CloudTrail Management Events**: First copy delivered to S3 is **100% Free**; additional copies are \$2.00 per 100,000 events.
- **CloudTrail Data Events**: \$0.10 per 100,000 events.
- **CloudTrail Lake**: \$0.75 per GB of data ingested; queries are \$0.005 per GB scanned.
- **AWS Config**:
  - \$0.003 per recorded Configuration Item (CI).
  - \$0.001 per Config rule evaluation (first 100,000 evaluations/month free).

---

## 9. Comparison Table

| Dimension | AWS CloudTrail | AWS Config | AWS Audit Manager |
|---|---|---|---|
| **Core Question Answered** | **Who** called what API, when, and from where? | **What** does my resource look like right now and over time? | **How** does my cloud comply with SOC 2 / HIPAA / PCI? |
| **Data Captured** | Ephemeral API event streams | Point-in-time configuration snapshots & relationships| Pre-packaged audit evidence reports |
| **Query Engine** | CloudTrail Lake / Athena | AWS Config Advanced Query (SQL) | Compliance Dashboard Export |
| **Remediation Action** | EventBridge -> Lambda/SSM | Native SSM Automation Runbooks | N/A (Auditing tool) |
| **Primary Audience** | Security Incident Responders | DevOps Engineers & SREs | External Auditors & Compliance Leads |

---

## 10. Interview Questions & Answers

<details>
<summary><strong>Q1: What is the architectural difference between AWS CloudTrail and AWS Config?</strong></summary>

- **AWS CloudTrail** captures **Actions and API Events** (the "Who", "What", "When", and "Where"). It records when an identity calls an API (e.g., `Alice called ec2:ModifySecurityGroupRules at 14:02 UTC from IP 198.51.100.2`). Once an API event is delivered, CloudTrail does not track the resulting resource state over time.
- **AWS Config** captures **State, Relationships, and Compliance** (the "Inventory"). It records what the resource looks like before and after the modification (e.g., `SecurityGroup sg-123 had port 22 open from 14:02 to 14:45 UTC`), evaluates whether that state complies with rules, and tracks relationships between resources over their complete lifecycle.
</details>

<details>
<summary><strong>Q2: How does CloudTrail Log File Integrity Validation prevent tampering?</strong></summary>

When enabled, CloudTrail generates a cryptographic **Digest File** in your S3 bucket every hour:
1. CloudTrail hashes every delivered log file using **SHA-256**.
2. It concatenates the hashes and signs the combined digest using an **RSA private key** held exclusively by the AWS CloudTrail service.
3. It includes the digital signature and the previous digest file's hash in the new digest (creating an immutable hash chain, similar to a blockchain).
4. Running `aws cloudtrail validate-logs` uses the public key to verify that no log files have been modified, appended to, or deleted since delivery.
</details>

<details>
<summary><strong>Q3: What are CloudTrail Data Events and why should you be cautious when enabling them?</strong></summary>

- **Management Events** track control plane operations (creating instances, modifying security groups, creating buckets). They have low-to-moderate volume and the first copy is free.
- **Data Events** track high-frequency data plane operations within a resource (such as `s3:GetObject`, `s3:PutObject`, and `lambda:Invoke`). 
- **Caution**: An active S3 data lake or high-throughput Lambda fleet can generate hundreds of millions of data events daily. Enabling data events on all buckets can result in thousands of dollars in unexpected monthly CloudTrail ingestion billing (\$0.10 per 100,000 events). Data events should be selectively scoped strictly to high-sensitivity S3 buckets.
</details>

<details>
<summary><strong>Q4: How does automated remediation work in AWS Config?</strong></summary>

When a resource modification occurs, AWS Config records a Configuration Item and evaluates the resource against attached Config Rules. 
If a rule evaluates as `NON_COMPLIANT`:
1. Config triggers an **AWS Systems Manager (SSM) Automation Document** associated with the rule remediation configuration.
2. The SSM document executes predefined remediation logic (e.g., calling `s3:PutBucketPublicAccessBlock` to seal an open bucket, or `ec2:RevokeSecurityGroupIngress` to close port 22).
3. The resource is re-evaluated and transitions back to `COMPLIANT` status within seconds.
</details>

<details>
<summary><strong>Q5: What is an AWS Config Conformance Pack?</strong></summary>

A Conformance Pack is a single declarative YAML template that bundles a collection of AWS Config rules and automated remediation actions aligned to a specific compliance framework or organizational governance standard (e.g., CIS AWS Foundations Benchmark, PCI-DSS, or Internal Enterprise Security Guidelines). 
It can be deployed organization-wide with a single CLI command (`put-organization-conformance-pack`), ensuring that every account in an organization automatically enforces identical compliance rules.
</details>

<details>
<summary><strong>Q6: What is CloudTrail Lake and when should you use it over Athena?</strong></summary>

Historically, running SQL queries on CloudTrail logs required configuring S3 bucket exports, AWS Glue crawlers, partitioning tables, and managing Amazon Athena queries. 
**CloudTrail Lake** is a fully managed, serverless audit query data lake. You create an Event Data Store in CloudTrail with a retention period up to 10 years, and execute ANSI SQL queries directly in the CloudTrail console or CLI without managing S3 storage, Glue schemas, or Athena tables.
</details>

<details>
<summary><strong>Q7: How does AWS Audit Manager gather compliance evidence automatically?</strong></summary>

AWS Audit Manager continuously collects evidence from underlying AWS data sources:
- **CloudTrail**: Captures evidence of administrative actions and IAM changes.
- **AWS Config**: Captures evidence of resource configurations and rule evaluations.
- **AWS Security Hub**: Captures compliance benchmark check results.
It maps this evidence directly to specific regulatory requirements (e.g., HIPAA Security Rule 45 CFR § 164.312(a)(2)(iv) Encryption at Rest) and packages it into an auditor-ready PDF/CSV report with cryptographically signed verification links.
</details>

<details>
<summary><strong>Q8: What are CloudTrail Insights Events?</strong></summary>

CloudTrail Insights uses machine learning to establish a baseline of normal administrative API call rates and error codes. When abnormal spikes occur—such as a sudden surge in `ec2:RunInstances` calls indicating a compromised account spinning up miners, or a spike in `AccessDenied` errors indicating credential probing—CloudTrail Insights emits an alert with diagnostic context and remediation links.
</details>

---

## 11. Practice Exercises

<details>
<summary><strong>Exercise 1: Create a Multi-Region CloudTrail with Log File Validation</strong></summary>

**Goal**: Provision an audit trail via AWS CLI.
1. Create a secure S3 bucket with versioning enabled.
2. Apply an S3 policy granting `cloudtrail.amazonaws.com` write access.
3. Execute `aws cloudtrail create-trail --name test-audit-trail --s3-bucket-name <bucket> --is-multi-region-trail --enable-log-file-validation`.
4. Start logging: `aws cloudtrail start-logging --name test-audit-trail`.
</details>

<details>
<summary><strong>Exercise 2: Validate CloudTrail Log Integrity via CLI</strong></summary>

**Goal**: Mathematically verify that logs have not been tampered with.
1. Allow CloudTrail to run for at least 1 hour so digest files generate.
2. Run `aws cloudtrail validate-logs --trail-arn <trail-arn> --start-time 2026-11-01T00:00:00Z`.
3. Verify output confirms `Results: 0 modified, 0 deleted, all digests valid`.
</details>

<details>
<summary><strong>Exercise 3: Deploy an AWS Config Managed Rule</strong></summary>

**Goal**: Evaluate S3 bucket encryption compliance.
1. Ensure AWS Config recorder is running.
2. Deploy managed rule `s3-bucket-server-side-encryption-enabled`.
3. Create an unencrypted S3 bucket.
4. Check compliance status in AWS Config; verify the bucket is flagged as `NON_COMPLIANT`.
</details>

<details>
<summary><strong>Exercise 4: Test CloudWatch Metric Filter on CloudTrail Root Activity</strong></summary>

**Goal**: Alert immediately on root user login.
1. Configure CloudTrail to stream events to a CloudWatch Log Group.
2. Create a Metric Filter matching: `{ $.userIdentity.type = "Root" && $.userIdentity.invokedBy NOT EXISTS && $.eventType != "AwsServiceEvent" }`.
3. Create an alarm on this metric alerting your security SNS topic.
</details>

<details>
<summary><strong>Exercise 5: Execute an Advanced SQL Query in AWS Config</strong></summary>

**Goal**: Query infrastructure inventory with SQL.
1. Open AWS Config -> Advanced queries.
2. Run: `SELECT resourceId, resourceType, configuration.instanceType WHERE resourceType = 'AWS::EC2::Instance' AND configuration.state.name = 'running'`.
3. Review the returned inventory list.
</details>

<details>
<summary><strong>Exercise 6: Configure an AWS Audit Manager Assessment</strong></summary>

**Goal**: Automate compliance evidence gathering for SOC 2.
1. Open AWS Audit Manager.
2. Create an Assessment using the pre-built **SOC 2 Type II** framework.
3. Scope the assessment to your production AWS account.
4. Review how controls are automatically populated with Config and CloudTrail evidence.
</details>

---

## 12. Summary & Quick Reference

```
+---------------------------------------------------------------------------------------------------+
| AWS COMPLIANCE & GOVERNANCE TOOLKIT                                                               |
+--------------------------+-----------------------+------------------------------------------------+
| Tool / Service           | Core Function         | Key Best Practice                              |
+--------------------------+-----------------------+------------------------------------------------+
| AWS CloudTrail           | Immutable API Auditing| Multi-region, log validation on, S3 Object Lock|
| CloudTrail Lake          | Managed SQL Audit Lake| Serverless queries for compliance investigations|
| AWS Config               | State & Configuration | Managed rules, auto-remediation via SSM runbooks|
| Conformance Packs        | Compliance Bundles    | Deploy CIS Benchmark pack organization-wide    |
| AWS Audit Manager        | Audit Evidence Hub    | Continuous evidence mapping to SOC 2 / HIPAA   |
+--------------------------+-----------------------+------------------------------------------------+
```

- **CloudTrail**: Answers "Who called what API and when?" (Mandatory multi-region).
- **AWS Config**: Answers "What was the state of my resources and did it comply?"
- **Audit Manager**: Automates continuous evidence collection for external auditors.

---

## 13. Further Reading

- [AWS CloudTrail User Guide](https://docs.aws.amazon.com/awscloudtrail/latest/userguide/cloudtrail-user-guide.html)
- [AWS Config Developer Guide](https://docs.aws.amazon.com/config/latest/developerguide/WhatIsConfig.html)
- [AWS Audit Manager User Guide](https://docs.aws.amazon.com/audit-manager/latest/userguide/what-is.html)
- [CIS AWS Foundations Benchmark Documentation](https://www.cisecurity.org/benchmark/amazon_web_services)
- [06-operations/11-multi-account-strategy.md](file:///04-devops/aws/06-operations/11-multi-account-strategy.md)
