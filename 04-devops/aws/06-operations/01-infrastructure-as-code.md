# Infrastructure as Code (IaC) Fundamentals & Cloud Automation

> **Module**: `06-operations`  
> **Topic**: Declarative vs Imperative, State Management, Drift Detection & GitOps Workflows  
> **Target Audience**: DevOps Engineers, Cloud Platform Architects, SREs  
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

In the early days of cloud computing, infrastructure was provisioned by clicking through the AWS Management Console or running ad-hoc Bash scripts. This manual approach introduced severe operational risks:
- **Configuration Drift & Snowflake Servers**: Inconsistencies between development, staging, and production environments created bugs that only manifested in production.
- **Lack of Auditability & Version Control**: Zero historical visibility into who changed a security group rule, opened port 22, or deleted a database.
- **Slow Disaster Recovery**: Rebuilding an entire environment after an outage took days or weeks of manual, error-prone reconstruction.

**Infrastructure as Code (IaC)** is the practice of defining, provisioning, and managing cloud infrastructure using machine-readable configuration files rather than manual processes. IaC brings software engineering rigor to infrastructure: version control in Git, automated pull-request code reviews, continuous integration/continuous delivery (CI/CD) pipelines, automated static security scanning, and reproducible environment spin-up in minutes.

---

## 2. Core Concepts

### 2.1 Declarative vs Imperative Paradigms
1. **Declarative IaC (What, Not How)**:
   - You define the **desired end state** of the infrastructure (e.g., "I want a VPC with CIDR 10.0.0.0/16 and 3 public subnets").
   - The IaC engine figures out the current state, computes the diff, and generates the necessary API calls to reach the target state.
   - Idempotent by nature: running the template 10 times results in the same state.
   - Examples: **AWS CloudFormation**, **HashiCorp Terraform**, **OpenTofu**.
2. **Imperative / Programmable IaC (How to Build)**:
   - You write code in programming languages (TypeScript, Python, Go) that explicitly executes construction logic.
   - Examples: **AWS CDK (Cloud Development Kit)** compiles imperative code down to declarative CloudFormation templates; **Pulumi** executes imperative state graphs directly.

### 2.2 State Management
- Declarative tools must maintain a **State File** mapping code definitions to actual physical AWS resource IDs (ARNs, VPC IDs).
- **Terraform State (`terraform.tfstate`)**: Stored remotely in an Amazon S3 bucket with server-side encryption and state locking via Amazon DynamoDB to prevent concurrent modifications.
- **CloudFormation State**: Managed internally and transparently by AWS within the CloudFormation service control plane.

### 2.3 Configuration Drift & Detection
- **Drift**: Occurs when team members manually modify cloud resources (e.g., adding an inbound rule in the EC2 Console) outside of the IaC codebase.
- **Drift Detection**: Tools scan live AWS API states, compare them against the IaC template or state file, and highlight differences for automated or manual reconciliation.

### 2.4 Immutable Infrastructure & GitOps
- **Immutable Infrastructure**: Servers are never patched or modified in-place. If code or configuration changes, a brand-new AMI or container image is built, deployed, and the old instance is terminated.
- **GitOps**: The practice where Git is the **single source of truth** for all infrastructure definitions. Infrastructure changes are executed strictly via automated pull-request approvals and CI/CD pipelines, completely revoking human write permissions to the AWS Console.

---

## 3. How It Works Internally

```
+---------------------------------------------------------------------------------------------------+
|                            GITOPS INFRASTRUCTURE AS CODE EXECUTION PIPELINE                       |
|                                                                                                   |
|  [DevOps Engineer]                                                                                |
|         |                                                                                         |
|         v (Git Commit & Push: Add RDS Read Replica)                                               |
|  +---------------------------------------------------------------------------------------------+  |
|  | GITHUB / GITLAB REPOSITORY (Source of Truth)                                                |  |
|  | - Pull Request triggers CI Pipeline via GitHub Actions / CodePipeline                       |  |
|  +---------------------------------------------------------------------------------------------+  |
|         |                                                                                         |
|         v (CI Triggered via OIDC Federation)                                                      |
|  +---------------------------------------------------------------------------------------------+  |
|  | AUTOMATED CI VALIDATION STAGE                                                               |  |
|  | 1. Static Linting: tflint / cfn-lint                                                         |  |
|  | 2. Security & Policy as Code: checkov / tfsec / cdk-nag (Ensures no public S3 or 0.0.0.0/0) |  |
|  | 3. Execution Plan Generation: `terraform plan` or `cdk diff`                                |  |
|  | 4. Plan posted automatically to PR comments for peer review                                 |  |
|  +---------------------------------------------------------------------------------------------+  |
|         |                                                                                         |
|         v (PR Merged to Main)                                                                     |
|  +---------------------------------------------------------------------------------------------+  |
|  | CD DEPLOYMENT STAGE (Assume Deployment IAM Role)                                            |  |
|  | 1. State Lock Acquired (DynamoDB lock table or CloudFormation ChangeSet)                     |  |
|  | 2. AWS SDK / API Calls Dispatched: `rds:CreateDBInstanceReadReplica`                        |  |
|  | 3. State file updated & released in S3 remote backend                                       |  |
|  +---------------------------------------------------------------------------------------------+  |
|         |                                                                                         |
|         v (Continuous Monitoring)                                                                 |
|  +---------------------------------------------------------------------------------------------+  |
|  | DRIFT DETECTION ENGINE (AWS Config / Terraform Cloud / OpenTofu)                           |  |
|  | Periodically compares Live AWS APIs vs Stored State -> Alerts on manual console drift       |  |
+---------------------------------------------------------------------------------------------------+
```

---

## 4. Syntax & Usage

### 4.1 Comparing Syntax Across Tools (Creating an S3 Bucket)

#### CloudFormation (YAML)
```yaml
AWSTemplateFormatVersion: '2010-09-09'
Description: Production S3 Storage Bucket
Resources:
  SecureStorageBucket:
    Type: AWS::S3::Bucket
    Properties:
      BucketName: enterprise-secure-vault-2026
      VersioningConfiguration:
        Status: Enabled
      BucketEncryption:
        ServerSideEncryptionConfiguration:
          - ServerSideEncryptionByDefault:
              SSEAlgorithm: aws:kms
```

#### Terraform (HCL)
```hcl
resource "aws_s3_bucket" "secure_vault" {
  bucket = "enterprise-secure-vault-2026"
}

resource "aws_s3_bucket_versioning" "versioning" {
  bucket = aws_s3_bucket.secure_vault.id
  versioning_configuration {
    status = "Enabled"
  }
}

resource "aws_s3_bucket_server_side_encryption_configuration" "kms_enc" {
  bucket = aws_s3_bucket.secure_vault.id
  rule {
    apply_server_side_encryption_by_default {
      sse_algorithm = "aws:kms"
    }
  }
}
```

#### AWS CDK (TypeScript)
```typescript
import * as cdk from 'aws-cdk-lib';
import * as s3 from 'aws-cdk-lib/aws-s3';
import { Construct } from 'constructs';

export class StorageStack extends cdk.Stack {
  constructor(scope: Construct, id: string, props?: cdk.StackProps) {
    super(scope, id, props);

    new s3.Bucket(this, 'SecureStorageBucket', {
      bucketName: 'enterprise-secure-vault-2026',
      versioned: true,
      encryption: s3.BucketEncryption.KMS_MANAGED,
      blockPublicAccess: s3.BlockPublicAccess.BLOCK_ALL,
      enforceSSL: true,
    });
  }
}
```

---

## 5. Practical Examples

### 5.1 Remote State Backend with S3 & DynamoDB Locking (Terraform)

```hcl
# backend-bootstrap.tf
terraform {
  required_version = ">= 1.5.0"
  required_providers {
    aws = {
      source  = "hashicorp/aws"
      version = "~> 5.0"
    }
  }

  backend "s3" {
    bucket         = "corp-terraform-state-us-east-1"
    key            = "prod/networking/terraform.tfstate"
    region         = "us-east-1"
    dynamodb_table = "corp-terraform-lock-table"
    encrypt        = true
  }
}
```

### 5.2 Automated Policy-as-Code Scanning with Checkov in GitHub Actions

```yaml
name: "IaC Security Scan"

on:
  pull_request:
    branches: [ main ]

jobs:
  checkov:
    runs-on: ubuntu-latest
    steps:
      - name: Checkout Code
        uses: actions/checkout@v4

      - name: Run Checkov Security Scan
        uses: bridgecrewio/checkov-action@master
        with:
          framework: terraform
          output_format: cli
          soft_fail: false # Fails build if high severity CVE or security misconfig found
          skip_check: CKV_AWS_144 # Skip cross-region replication check if not applicable
```

### 5.3 Automated Drift Detection Workflow (Bash / CLI)

```bash
#!/usr/bin/env bash
# Runs automated drift detection against production infrastructure
set -euo pipefail

echo "==> Initializing Terraform Backend..."
terraform init -backend=true

echo "==> Running Plan with Detailed Exit Code..."
# -detailed-exitcode: 0 = No changes, 1 = Error, 2 = Drift detected!
set +e
terraform plan -detailed-exitcode -no-color > drift_report.txt
EXIT_CODE=$?
set -e

if [ $EXIT_CODE -eq 2 ]; then
    echo "CRITICAL: Configuration drift detected in production AWS environment!"
    # Dispatch alert to Slack webhook or PagerDuty
    curl -X POST -H 'Content-type: application/json' \
        --data '{"text":"⚠️ *Alert*: Production infrastructure has drifted from Git source of truth!"}' \
        "$SLACK_WEBHOOK_URL"
    exit 2
elif [ $EXIT_CODE -eq 0 ]; then
    echo "SUCCESS: Infrastructure is 100% in sync with code."
else
    echo "ERROR: Terraform plan failed."
    exit 1
fi
```

---

## 6. Best Practices

1. **Lock State Files Remotely**: Always store Terraform state files in a private, encrypted S3 bucket with versioning enabled and DynamoDB state locking to prevent concurrent state corruption.
2. **Never Commit Secrets to Git**: Use `.gitignore` for `.tfvars` containing secrets. Inject sensitive values at runtime using AWS Secrets Manager or environment variables.
3. **Enforce Policy-as-Code in CI**: Integrate tools like Checkov, tfsec, cfn-nag, or OPA (Open Policy Agent) to automatically fail PRs that attempt to create unencrypted buckets or open security groups (`0.0.0.0/0` on port 22/3389).
4. **Decompose State Files by Lifecycle**: Never create a single "god state" file for your entire company. Separate infrastructure into independent state boundaries: `networking`, `database`, `compute`, and `applications`.
5. **Enforce Read-Only Access in Production**: Strip human engineers of write access in the production AWS console. All changes must originate from reviewed, tested Git pull requests.

---

## 7. Common Mistakes

| Mistake | Root Cause | Impact | Fix |
|---|---|---|---|
| Committing State File to Git | `terraform.tfstate` tracked in repository. | Exposes database passwords and access keys stored in plaintext state. | Add `*.tfstate*` to `.gitignore` and configure remote S3 backend. |
| Monolithic Giant State File | Managing VPC, RDS, and ECS tasks in a single 10,000-line file. | Blast radius is catastrophic; `terraform apply` takes 30 minutes. | Split into modular stacks with discrete state keys. |
| Manual Hotfixes in Console | Engineer edits SG rule directly in console during an incident. | Future IaC deployment reverts the fix or fails with state collision. | Backport manual fixes to Git immediately, or import into state. |
| Missing S3 Versioning on Backend | State file corrupted by network disruption; no history. | Permanent loss of infrastructure mapping. | Enable bucket versioning and MFA delete on state bucket. |
| Concurrent Unlocked Applies | Two engineers run `terraform apply` simultaneously. | Race condition corrupts or overwrites remote state. | Configure DynamoDB locking table (`dynamodb_table`). |
| Hardcoding Availability Zones | Hardcoding `us-east-1a` across templates. | AZ IDs differ across accounts; deployment fails in secondary accounts. | Use dynamic datasource lookups (`aws_availability_zones`). |

---

## 8. Performance, Security & Cost Notes

### 8.1 Plan & Apply Performance
- **Targeted Applies (`-target`)**: Avoid using `-target` in normal workflows as it can cause incomplete dependency state; reserve strictly for emergency break-glass recovery.
- **Parallelism**: Increase `terraform apply -parallelism=20` to accelerate provisioning of large resource trees.

### 8.2 Security & Compliance
- **KMS State Encryption**: Terraform state files store resource attributes (including DB master passwords and private keys) in plaintext JSON. The S3 backend bucket must enforce KMS CMK encryption and strict IAM bucket policies.

### 8.3 Cost Breakdown
- **CloudFormation**: Native service is **100% Free** (you pay only for the underlying resources created).
- **Terraform / OpenTofu**: Open-source CLI is **100% Free**.
- **S3 & DynamoDB Backend**: Costs pennies per month (~$0.25/month for S3 storage and DynamoDB pay-per-request locking).

---

## 9. Comparison Table

| Dimension | AWS CloudFormation | AWS CDK (v2) | HashiCorp Terraform | Pulumi |
|---|---|---|---|---|
| **Authoring Paradigm** | Declarative (YAML/JSON) | Imperative (TS, Python, Go)| Declarative (HCL) | Imperative (TS, Python, Go) |
| **Cloud Target** | AWS Native Only | AWS Native (Compiles to CFN)| Multi-Cloud (AWS, GCP, Azure) | Multi-Cloud |
| **State Storage** | AWS Managed Internal | AWS Managed Internal | Remote S3 + DynamoDB | Pulumi Service / S3 |
| **Ecosystem Maturity** | Extreme (First-party) | High (Modern standard) | Extreme (De-facto industry standard)| High |
| **Abstraction Level** | Low (Resource level) | High (L2/L3 Constructs) | Moderate (Modules) | High (Components) |
| **Drift Detection** | Native AWS Console | Via CloudFormation | CLI Plan Diff (`plan -detailed`)| CLI Refresh Diff |

---

## 10. Interview Questions & Answers

<details>
<summary><strong>Q1: What is the difference between Declarative and Imperative Infrastructure as Code?</strong></summary>

- **Declarative IaC** focuses on the **what**: you declare the desired end state of the system in configuration files (YAML, JSON, or HCL). The tool calculates the current state, identifies discrepancies, and executes the minimal set of changes to match the declared state. Examples include CloudFormation and Terraform.
- **Imperative IaC** focuses on the **how**: you write procedural code in standard programming languages (Python, TypeScript) defining explicit sequence steps to build resources. AWS CDK bridges both: developers write imperative code that synthesizes into purely declarative CloudFormation templates.
</details>

<details>
<summary><strong>Q2: Why does Terraform require a state file, whereas CloudFormation does not appear to have one?</strong></summary>

Both tools require state tracking. 
- **Terraform** is cloud-agnostic; it does not own the AWS backend. It must store a mapping of declared code resources to physical AWS ARNs and IDs in a `terraform.tfstate` file, which developers must manage and secure remotely (e.g., in S3 with DynamoDB locking).
- **CloudFormation** is an AWS-native managed service. The CloudFormation service control plane automatically maintains and tracks resource state internally within AWS database tables, abstracting state storage away from the developer.
</details>

<details>
<summary><strong>Q3: What is Configuration Drift and how do you prevent and remediate it?</strong></summary>

Configuration Drift occurs when resources in the live environment are modified outside of the IaC codebase (e.g., via the AWS Console or manual CLI commands). 
- **Prevention**: Enforce GitOps and remove console write permissions from engineers using strict IAM SCPs (Service Control Policies).
- **Detection**: Run scheduled daily automated drift detection jobs (`terraform plan -detailed-exitcode` or CloudFormation Drift Detection).
- **Remediation**: Backport the changes into the IaC codebase, or re-run `apply` to overwrite and align the live resources back to the Git source of truth.
</details>

<details>
<summary><strong>Q4: Why is state locking essential when multiple engineers collaborate on infrastructure?</strong></summary>

Without state locking, if Engineer A and Engineer B run `terraform apply` simultaneously, both processes read the same initial state, generate conflicting execution plans, and write conflicting updates back to the state file concurrently. This leads to race conditions, partial resource creations, and state corruption. 
DynamoDB state locking acquires a mutex lock item before any state modification; any concurrent apply attempt is blocked with an error until the active operation completes.
</details>

<details>
<summary><strong>Q5: What are the risks of using a single monolithic state file for an entire enterprise?</strong></summary>

1. **Massive Blast Radius**: A typo or syntax bug during an apply could accidentally delete or modify unrelated core production databases.
2. **Performance Degradation**: As resources grow, refreshing the state of thousands of resources on every plan takes 20-40 minutes.
3. **Concurrency Bottlenecks**: Only one engineer can apply changes across the entire company at any given time due to state locking.
**Fix**: Break state files into distinct stacks: Network, Data, Core Compute, and App.
</details>

<details>
<summary><strong>Q6: What is Policy as Code and where should it be enforced?</strong></summary>

Policy as Code is the practice of defining compliance, security, and operational guardrails as executable code. 
- **Shift-Left Enforcement**: Tools like Checkov, tfsec, and cfn-nag scan IaC templates during the local build and CI/CD pull-request phase, blocking PR merges if security violations are detected.
- **Run-time Enforcement**: AWS Config rules and Sentinel evaluate live resources and prevent deployment of non-compliant configurations.
</details>

<details>
<summary><strong>Q7: What is Immutable Infrastructure and how does it relate to IaC?</strong></summary>

In immutable infrastructure, existing instances are never patched, upgraded, or reconfigured in-place (no SSHing to update packages). When an application or OS update is needed, the IaC pipeline launches brand-new instances using updated AMIs or container images, verifies health checks, shifts traffic over, and destroys the old instances. This eliminates configuration drift and ensures reproducible deployments.
</details>

<details>
<summary><strong>Q8: What is a CloudFormation Change Set?</strong></summary>

A Change Set is a preview of the proposed changes that CloudFormation will execute on a stack before modifying live resources. It details which resources will be added, modified, or deleted, and critically highlights whether a resource modification requires **replacement** (e.g., changing an RDS DB instance identifier or KMS key replaces the database, which causes data loss if unmanaged).
</details>

---

## 11. Practice Exercises

<details>
<summary><strong>Exercise 1: Bootstrap an S3 and DynamoDB Remote State Backend</strong></summary>

**Goal**: Provision a secure backend for Terraform.
1. Create an S3 bucket with versioning and SSE-KMS encryption enabled.
2. Create a DynamoDB table `terraform-locks` with primary key `LockID` (String).
3. Configure your Terraform backend block to reference them.
4. Run `terraform init` and verify the state file is uploaded to S3.
</details>

<details>
<summary><strong>Exercise 2: Simulate State Locking Concurrency</strong></summary>

**Goal**: Observe DynamoDB state lock protection in action.
1. In terminal 1, run a command that acquires a lock (e.g., `terraform apply` with a long pause).
2. In terminal 2, simultaneously run `terraform plan`.
3. Verify that terminal 2 is rejected with `Error acquiring the state lock`.
</details>

<details>
<summary><strong>Exercise 3: Detect and Reconcile Configuration Drift</strong></summary>

**Goal**: Catch manual console modifications.
1. Deploy an S3 bucket with tag `Environment = "Production"`.
2. Manually change the tag in the AWS Console to `Environment = "Staging"`.
3. Run `terraform plan` and inspect the detected drift.
4. Run `terraform apply` to overwrite the manual change and restore code alignment.
</details>

<details>
<summary><strong>Exercise 4: Run Static Security Scans with Checkov</strong></summary>

**Goal**: Block unencrypted resource definitions.
1. Write a Terraform snippet creating an S3 bucket without encryption.
2. Run `checkov -d .`.
3. Review the failure output (`CKV_AWS_19: Ensure all data stored in S3 is encrypted`).
4. Add the encryption block and verify Checkov passes.
</details>

<details>
<summary><strong>Exercise 5: Split a Monolith into Layered Stacks</strong></summary>

**Goal**: Structure infrastructure by lifecycle.
1. Create a `01-network` directory managing VPC and subnets.
2. Create a `02-database` directory using `terraform_remote_state` data source to read the VPC ID.
3. Apply both stacks independently.
</details>

<details>
<summary><strong>Exercise 6: Generate CloudFormation Change Set via CLI</strong></summary>

**Goal**: Preview infrastructure modifications safely.
1. Deploy a basic CloudFormation template.
2. Update the template with an additional tag.
3. Execute `aws cloudformation create-change-set`.
4. Inspect the changes with `describe-change-set` before calling `execute-change-set`.
</details>

---

## 12. Summary & Quick Reference

```
+---------------------------------------------------------------------------------------------------+
| INFRASTRUCTURE AS CODE LIFECYCLE CHEAT SHEET                                                      |
+--------------------------+-------------------------------------+----------------------------------+
| Phase                    | Purpose                             | Key Best Practice                |
+--------------------------+-------------------------------------+----------------------------------+
| Authoring                | Define desired state in code        | Use modules, avoid hardcoding IDs|
| Static Analysis          | Pre-commit security & lint check    | cfn-lint, tflint, checkov        |
| Planning / Diff          | Preview delta before applying       | Review Change Sets / terraform plan|
| Execution                | Apply API state modifications       | Enforce automated CI/CD applying |
| State Storage            | Mapping code to physical ARNs       | Encrypted S3 + DynamoDB locking  |
| Drift Monitoring         | Continuous synchronization check    | Daily automated plan diff alerts |
+--------------------------+-------------------------------------+----------------------------------+
```

- **Git as Source of Truth**: All infrastructure changes originate in Git pull requests.
- **Remote State Locking**: Essential protection against simultaneous corrupting updates.
- **Layered Architecture**: Decouple network, database, and application lifecycles.

---

## 13. Further Reading

- [AWS Well-Architected Framework: Operational Excellence Pillar](https://docs.aws.amazon.com/wellarchitected/latest/operational-excellence-pillar/)
- [HashiCorp Terraform AWS Provider Documentation](https://registry.terraform.io/providers/hashicorp/aws/latest/docs)
- [AWS Cloud Development Kit (CDK) v2 Developer Guide](https://docs.aws.amazon.com/cdk/v2/guide/home.html)
- [Checkov Open Source Policy-as-Code](https://www.checkov.io/)
- [06-operations/02-cloudformation.md](file:///04-devops/aws/06-operations/02-cloudformation.md)
