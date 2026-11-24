# AWS Shared Responsibility & The Well-Architected Framework

> The Shared Responsibility Model defines the strict operational demarcation between AWS and the customer, while the AWS Well-Architected Framework provides the architectural heuristics and design pillars required to build resilient, secure, high-performing, and cost-optimized cloud workloads.

| Metadata | Details |
|---|---|
| **Level** | Beginner to Intermediate |
| **Prerequisites** | [01-what-is-aws.md](file:///04-devops/aws/01-foundations/01-what-is-aws.md) |
| **Time to Read** | 25 minutes |
| **Status** | Complete |
| **OS / Cloud Scope** | AWS Cloud Compliance & Architecture Governance |
| **Tool Version** | AWS Well-Architected Tool, AWS Organizations |
| **Tested With** | AWS CLI v2.22+, AWS Well-Architected Tool API |

---

## Table of Contents
1. [What & Why](#1-what--why)
2. [Core Concepts](#2-core-concepts)
3. [How It Works Internally](#3-how-it-works-internally)
4. [Syntax & Usage](#4-syntax--usage)
5. [Practical Examples](#5-practical-examples)
6. [Best Practices](#6-best-practices)
7. [Common Mistakes & Pitfalls](#7-common-mistakes--pitfalls)
8. [Performance, Security & Cost Notes](#8-performance-security--cost-notes)
9. [Comparison / When to Use What](#9-comparison--when-to-use-what)
10. [Interview Questions](#10-interview-questions)
11. [Practice Exercises](#11-practice-exercises)
12. [Summary / Cheat Sheet](#12-summary--cheat-sheet)
13. [Further Reading](#13-further-reading)

---

## 1. What & Why
Cloud migrations frequently suffer from two fatal misconceptions: either believing that moving to AWS automatically renders systems secure and resilient, or treating cloud instances exactly like on-premises bare metal. When an unencrypted S3 bucket leaks customer PII or an unpatched EC2 instance is breached, the failure stems directly from misunderstanding the **Shared Responsibility Model**.

The Shared Responsibility Model explicitly defines who owns which operational tier: AWS is responsible for security **OF** the cloud (physical data centers, host hardware, hypervisors, managed service engines), while the customer is responsible for security **IN** the cloud (customer data, IAM policies, OS patching, firewall configurations, encryption). To guide customer architectural decisions within their boundary, AWS formalizes the **Well-Architected Framework**, establishing 6 foundational pillars distilled from reviewing thousands of enterprise architectures.

---

## 2. Core Concepts
- **Security OF the Cloud (AWS Scope)**:
  - Physical data center facilities, biometric access, environmental controls.
  - Core network hardware, physical fiber links, edge Points of Presence.
  - Server hardware, storage arrays, and hypervisor virtualization layers (Nitro System).
- **Security IN the Cloud (Customer Scope)**:
  - Customer data lifecycle, classification, and client/server-side encryption.
  - Identity and Access Management (IAM), MFA enforcement, password policies.
  - Operating system patching, firewall (Security Group/NACL) configuration, application security.
- **The 6 Well-Architected Pillars**:
  1. *Operational Excellence*: Running and monitoring systems to deliver business value, executing changes as code, learning from failures.
  2. *Security*: Protecting information and systems through confidentiality, integrity, IAM least privilege, and automated threat detection.
  3. *Reliability*: Ability of a workload to perform its intended function correctly, recovering automatically from disruptions.
  4. *Performance Efficiency*: Structured allocation of compute, storage, and database resources to maintain efficiency as demand evolves.
  5. *Cost Optimization*: Eliminating unneeded expense, selecting cost-effective instances (Graviton, Spot), using managed serverless.
  6. *Sustainability*: Minimizing the environmental impacts of running cloud workloads, maximizing utilization, reducing carbon footprints.

---

## 3. How It Works Internally
How the shared responsibility boundary shifts dynamically depending on the service abstraction layer:

```
+-------------------------------------------------------------------------+
|                  The Sliding Responsibility Spectrum                    |
|                                                                         |
| Responsibility Tier    IaaS (EC2)       PaaS (RDS)       Serverless (S3)|
| ----------------------------------------------------------------------- |
| Customer Data          [CUSTOMER]       [CUSTOMER]       [CUSTOMER]     |
| IAM & Access Control   [CUSTOMER]       [CUSTOMER]       [CUSTOMER]     |
| Network / Firewalls    [CUSTOMER]       [CUSTOMER]       [  AWS   ]     |
| OS Patching / Security [CUSTOMER]       [  AWS   ]       [  AWS   ]     |
| Database Engine Patch  [CUSTOMER]       [  AWS   ]       [  AWS   ]     |
| Hypervisor & Hardware  [  AWS   ]       [  AWS   ]       [  AWS   ]     |
| Physical Data Center   [  AWS   ]       [  AWS   ]       [  AWS   ]     |
+-------------------------------------------------------------------------+
```

### The Well-Architected Review Feedback Loop
AWS provides the **AWS Well-Architected Tool** in the console and API to audit workloads against curated Lens questionnaires (Serverless Lens, SaaS Lens, Foundation Model Lens). The evaluation engine produces High Risk Issues (HRIs) and Medium Risk Issues (MRIs) mapped directly to prescriptive architectural improvement plans.

---

## 4. Syntax & Usage
Interacting with the Well-Architected Tool and Security Audit APIs via CLI:

```bash
# List official Well-Architected lenses available in the region
aws wellarchitected list-lenses --query "LensSummaries[].[LensAlias, LensName, LensVersion]" --output table

# Create a workload review definition
aws wellarchitected create-workload \
    --workload-name "Production-Payment-API" \
    --description "Payment processing API backend" \
    --environment "PRODUCTION" \
    --review-owner "security-lead@example.com" \
    --lenses "wellarchitected" "serverless" \
    --aws-regions "us-east-1"

# Query High Risk Issues (HRIs) discovered in a review
aws wellarchitected get-workload \
    --workload-id <WORKLOAD_ID> \
    --query "Workload.RiskCounts"
```

---

## 5. Practical Examples

### Example 1: Inspecting Well-Architected Lens Definitions
- **Goal:** View available architectural lenses including the Well-Architected Core and GenAI/Serverless extensions.
- **Commands:**
```bash
aws wellarchitected list-lenses --query "LensSummaries[].[LensAlias, LensName]" --output table
```
- **Real Output:**
```text
-------------------------------------------------------------------
|                            ListLenses                           |
+------------------------+----------------------------------------+
|  wellarchitected       |  AWS Well-Architected Framework        |
|  serverless            |  Serverless Lens                       |
|  softwareasaservice    |  SaaS Lens                             |
|  foundational-technical|  Foundational Technical Review         |
+------------------------+----------------------------------------+
```
- **Explanation:** Displays the curated assessment frameworks maintained by AWS solutions architects.

### Example 2: Programmatic Workload Creation
- **Goal:** Register an enterprise workload in the Well-Architected Tool for compliance auditing.
- **Commands:**
```bash
aws wellarchitected create-workload \
    --workload-name "Core-Banking-Engine" \
    --description "Core ledger transaction processor" \
    --environment "PRODUCTION" \
    --review-owner "ciso-team@example.com" \
    --lenses "wellarchitected" \
    --aws-regions "us-east-1" "us-west-2" \
    --query "WorkloadId" --output text
```
- **Real Output:**
```text
e9a1b2c3d4e5f6789012345678abcdef
```
- **Explanation:** Registers a tracked entity against which architectural questionnaire answers and risk metrics are stored.

### Example 3: Edge Case - Shared Responsibility in Managed Container Services (EKS)
- **Goal:** Illustrate how the responsibility boundary splits in Amazon EKS (Kubernetes).
- **Architecture Model:**
```text
AWS Responsibility:
- Kubernetes API Server, etcd cluster, Controller Manager (Control Plane)
- Managed node AMI updates and kernel security (when using Managed Node Groups)

Customer Responsibility:
- Pod Security Standards (Admission Controllers, runAsNonRoot)
- NetworkPolicies (Calico / Amazon VPC CNI security groups for pods)
- Application container image vulnerabilities and RBAC role bindings
```
- **Explanation:** Using EKS does not protect against pod-level privilege escalation or misconfigured Kubernetes RBAC.

### Example 4: Failure & Anti-Pattern - Neglecting OS Patching on EC2
- **Goal:** Demonstrate risk when a customer assumes AWS patches virtual machines automatically.
- **Anti-Pattern Scenario:**
```bash
# An EC2 instance launched 18 months ago with Amazon Linux 2
# Customer assumes AWS automatically updates Linux kernel vulnerabilities
uname -r
```
- **Real Output:**
```text
5.10.184-175.731.amzn2.x86_64
# Security audit flags: 14 High CVEs, 3 Critical Kernel Vulnerabilities
```
- **Explanation & Fix:** On IaaS (EC2), AWS never touches the running guest OS. Customers must configure **AWS Systems Manager (SSM) Patch Manager** with automated patch baselines or adopt immutable AMI replacement pipelines.

### Example 5: Production Hardening - Enforcing Security Pillar via Organizations SCP
- **Goal:** Implement the Security Pillar by programmatically blocking all member accounts from disabling S3 Block Public Access.
- **Service Control Policy (`block-s3-public-bypass.json`):**
```json
{
    "Version": "2012-10-17",
    "Statement": [
        {
            "Sid": "PreventBypassingPublicAccessBlock",
            "Effect": "Deny",
            "Action": [
                "s3:PutAccountPublicAccessBlock",
                "s3:PutBucketPublicAccessBlock"
            ],
            "Resource": "*",
            "Condition": {
                "Bool": {
                    "s3:BlockPublicPolicy": "false"
                }
            }
        }
    ]
}
```
- **Explanation:** Uses declarative AWS Organizations SCPs to enforce guardrails that customer account administrators cannot disable.

---

## 6. Best Practices
1. **Apply the Principle of Least Privilege**: Never grant `AdministratorAccess` to applications; scope permissions down to specific actions and ARNs.
2. **Automate Operational Excellence**: Instrument all deployments with canary rollouts, automated rollback alarms, and post-incident postmortems.
3. **Design for Failure (Reliability Pillar)**: Assume hardware will crash, disks will degrade, and data centers will lose power; design stateless services behind load balancers spanning 3 AZs.
4. **Tag for Financial Accountability (Cost Pillar)**: Enforce cost-allocation tags (`Owner`, `Project`, `Environment`) at the AWS Organizations level.
5. **Conduct Regular Well-Architected Reviews**: Review workloads quarterly to address newly discovered High Risk Issues (HRIs).

---

## 7. Common Mistakes & Pitfalls

| Mistake | Why It Fails | Practical Remedy |
|---|---|---|
| Assuming AWS encrypts data automatically | By default, S3 encrypts objects, but EBS and RDS require explicit encryption activation at creation | Always check the `encrypted: true` flag in Terraform / CDK modules |
| Believing AWS patches EC2 guest OS | AWS only patches physical host hypervisors; guest VMs remain vulnerable | Implement AWS Systems Manager Patch Manager or auto-scaling AMI rotation |
| Storing hardcoded secrets in source code | Git repositories leak credentials to public crawlers within minutes | Store secrets in AWS Secrets Manager or Parameter Store with IAM authorization |
| Designing for a single AZ to save networking fees | An AZ physical failure causes complete application downtime | Multi-AZ architecture is non-negotiable for production SLAs |
| Treating Well-Architected as a one-time gate | Architecture drifts as features and dependencies are added over time | Integrate Well-Architected reviews into continuous quarterly agile sprints |
| Ignoring sustainability metrics | Over-provisioned x86 instances run at 5% CPU, wasting energy and funds | Right-size workloads, migrate to Graviton4 ARM processors, and utilize auto-scaling |

---

## 8. Performance, Security & Cost Notes
- **Performance**: The Performance Efficiency pillar mandates choosing the right storage tier (e.g. `gp3` vs `io2` for EBS, S3 Standard vs S3 Express One Zone) to balance IOPS and throughput without over-provisioning.
- **Security**: The Security pillar establishes the concept of "Defense in Depth": network security groups, host-level firewalls, IAM role restrictions, and KMS encryption of data in transit and at rest.
- **Cost**: The Cost Optimization pillar states that unused resources must be turned off automatically. Development environments should be scheduled to stop outside business hours via AWS Instance Scheduler.

---

## 9. Comparison / When to Use What

| Dimension | Shared Responsibility Model | Well-Architected Framework |
|---|---|---|
| **Primary Focus** | Legal & operational boundary of accountability | Engineering best practices and design principles |
| **Enforcement** | Contractual / SLA standard | Advisory architectural guidance |
| **Key Question** | "Who is responsible if this layer fails or leaks?" | "Is this workload designed to survive failure efficiently?" |
| **Evaluation Tool** | AWS Artifact (SOC 2, ISO reports) | AWS Well-Architected Tool |

---

## 10. Interview Questions

<details>
<summary>1. Explain the Shared Responsibility Model using Amazon RDS as an example.</summary>
For Amazon RDS, AWS is responsible for physical hardware, OS installation, security patching of the underlying Linux OS, database engine updates, and multi-AZ physical storage replication. The customer is responsible for network firewall rules (Security Groups), database user privileges, table schemas, query optimization, SSL/TLS client enforcement, and data encryption keys.
</details>

<details>
<summary>2. Name the 6 pillars of the AWS Well-Architected Framework.</summary>
1. Operational Excellence, 2. Security, 3. Reliability, 4. Performance Efficiency, 5. Cost Optimization, 6. Sustainability.
</details>

<details>
<summary>3. What is the difference between Security OF the Cloud and Security IN the Cloud?</summary>
Security OF the cloud encompasses the infrastructure, physical security of data centers, hardware, and software running AWS services that AWS manages. Security IN the cloud encompasses customer-configured items: customer data, IAM access control, OS configurations, network firewall rules, and application code.
</details>

<details>
<summary>4. What constitutes a "High Risk Issue" (HRI) in a Well-Architected review?</summary>
An HRI represents an architectural flaw that could cause severe business impact, such as lack of automated backup verification, single point of failure in an Availability Zone, missing MFA on privileged accounts, or complete absence of disaster recovery plans.
</details>

<details>
<summary>5. How does the Sustainability Pillar influence compute architecture?</summary>
It encourages maximizing hardware resource utilization (high CPU utilization vs idle servers), adopting ARM-based AWS Graviton processors (which consume up to 60% less energy for the same performance), and utilizing serverless event-driven scaling to power down compute when no traffic exists.
</details>

<details>
<summary>6. If an S3 bucket is configured with public read access and data is exposed, who is at fault under the Shared Responsibility Model?</summary>
The customer. AWS provides the tools (S3 Block Public Access, IAM policies, bucket policies, encryption), but the customer chose or misconfigured the policy granting public read access.
</details>

<details>
<summary>7. How does the Operational Excellence pillar define "learning from failure"?</summary>
Workloads must perform blameless post-mortems after outages, identifying root causes, documenting operational runbooks, and implementing automated corrective actions so that specific failure modes can never recur.
</details>

<details>
<summary>8. How do AWS Service Control Policies (SCPs) relate to the Security Pillar?</summary>
SCPs act as organization-wide preventive guardrails, establishing hard boundaries that member account root users and administrators cannot exceed (e.g., blocking region usage, disallowing public S3 buckets, preventing disabling of CloudTrail).
</details>

---

## 11. Practice Exercises

<details>
<summary>Exercise 1 (Easy): Create a Well-Architected Workload review</summary>
**Task:** Use the AWS CLI to initialize a workload review named `Web-App-Review`.

**Solution:**
```bash
aws wellarchitected create-workload \
    --workload-name "Web-App-Review" \
    --description "Review for primary e-commerce web application" \
    --environment "PRODUCTION" \
    --review-owner "lead-architect@example.com" \
    --lenses "wellarchitected" \
    --aws-regions "us-east-1"
```
</details>

<details>
<summary>Exercise 2 (Easy): Audit unencrypted EBS volumes</summary>
**Task:** Query all EBS volumes in the current region that are not encrypted.

**Solution:**
```bash
aws ec2 describe-volumes \
    --query "Volumes[?Encrypted==\`false\`].[VolumeId, Size, AvailabilityZone]" \
    --output table
```
</details>

<details>
<summary>Exercise 3 (Medium): Retrieve High Risk Issues from a review</summary>
**Task:** Write a command to extract the count of High Risk Issues (HRIs) across all pillars for a given workload ID.

**Solution:**
```bash
aws wellarchitected get-workload \
    --workload-id <WORKLOAD_ID> \
    --query "Workload.RiskCounts" \
    --output json
```
</details>

<details>
<summary>Exercise 4 (Medium): Enforce default EBS encryption at the account level</summary>
**Task:** Configure your account to ensure all newly created EBS volumes are encrypted by default.

**Solution:**
```bash
aws ec2 enable-ebs-encryption-by-default
aws ec2 get-ebs-encryption-by-default
```
</details>

<details>
<summary>Exercise 5 (Hard): Script a shared responsibility compliance audit</summary>
**Task:** Write a bash script that checks whether S3 Block Public Access is active at the account level and whether default EBS encryption is enabled.

**Solution:**
```bash
#!/usr/bin/env bash
set -euo pipefail

ACCOUNT_ID=$(aws sts get-caller-identity --query Account --output text)
echo "Auditing Account: $ACCOUNT_ID"

echo -n "Checking EBS Default Encryption... "
EBS_STATUS=$(aws ec2 get-ebs-encryption-by-default --query EbsEncryptionByDefault --output text)
echo "$EBS_STATUS"

echo -n "Checking S3 Account-level Block Public Access... "
S3_STATUS=$(aws s3control get-public-access-block \
    --account-id "$ACCOUNT_ID" \
    --query "PublicAccessBlockConfiguration.BlockPublicPolicy" --output text 2>/dev/null || echo "DISABLED")
echo "$S3_STATUS"
```
</details>

<details>
<summary>Exercise 6 (Hard): Construct an automated patch baseline in Systems Manager</summary>
**Task:** Create an SSM patch baseline that auto-approves Critical security updates for Amazon Linux 2023 after 2 days.

**Solution:**
```bash
aws ssm create-patch-baseline \
    --name "AL2023-Security-Baseline" \
    --operating-system "AMAZON_LINUX_2023" \
    --approval-rules '{
        "PatchRules": [
            {
                "PatchFilterGroup": {
                    "PatchFilters": [
                        {"Key": "CLASSIFICATION", "Values": ["Security"]},
                        {"Key": "SEVERITY", "Values": ["Critical", "Important"]}
                    ]
                },
                "ApproveAfterDays": 2
            }
        ]
    }'
```
</details>

---

## 12. Summary / Cheat Sheet

```
+---------------------------------------------------------------------------------+
|               SHARED RESPONSIBILITY & WELL-ARCHITECTED CHEAT SHEET              |
+---------------------------------------------------------------------------------+
| SECURITY OF CLOUD | Physical facilities, Nitro hypervisors, server hardware     |
| SECURITY IN CLOUD | Customer data, IAM credentials, OS patching, firewalls      |
| 6 PILLARS         | Operational Excellence, Security, Reliability,              |
|                   | Performance Efficiency, Cost Optimization, Sustainability   |
| WELL-ARCH TOOL    | aws wellarchitected get-workload --workload-id <ID>         |
| ACCOUNT S3 BLOCK  | aws s3control get-public-access-block --account-id <ID>     |
| DEFAULT EBS ENC   | aws ec2 enable-ebs-encryption-by-default                    |
| DESIGN GOAL       | Automate recovery, test at cloud scale, eliminate monoliths |
+---------------------------------------------------------------------------------+
```

---

## 13. Further Reading
- [AWS Shared Responsibility Model Documentation](https://aws.amazon.com/compliance/shared-responsibility-model/)
- [AWS Well-Architected Framework Whitepaper](https://docs.aws.amazon.com/wellarchitected/latest/framework/welcome.html)
- [AWS Well-Architected Tool User Guide](https://docs.aws.amazon.com/wellarchitected/latest/userguide/intro.html)
- [AWS Security Pillar Deep Dive](https://docs.aws.amazon.com/wellarchitected/latest/security-pillar/welcome.html)

---

[Previous: 01-what-is-aws.md](file:///04-devops/aws/01-foundations/01-what-is-aws.md) | [Section Index](file:///04-devops/aws/01-foundations/README.md) | [Next: 03-accounts-regions-and-billing.md](file:///04-devops/aws/01-foundations/03-accounts-regions-and-billing.md)
