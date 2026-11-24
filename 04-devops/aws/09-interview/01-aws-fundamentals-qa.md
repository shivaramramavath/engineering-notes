---
topic: "AWS Fundamentals & Cloud Concepts Technical Interview Q&A"
module: "09-interview"
track: "aws"
difficulty: "foundational-to-intermediate"
last-tested: "2026-11"
tags:
  - "interview"
  - "fundamentals"
  - "regions"
  - "availability-zones"
  - "shared-responsibility"
  - "billing"
---

# AWS Fundamentals & Cloud Concepts Technical Interview Q&A

## Table of Contents
- [1. Core Foundational Concepts](#1-core-foundational-concepts)
- [2. Global Infrastructure: Regions, AZs, and Edge Locations](#2-global-infrastructure-regions-azs-and-edge-locations)
- [3. The Shared Responsibility Model](#3-the-shared-responsibility-model)
- [4. Account Management & Governance](#4-account-management--governance)
- [5. Billing, Cost Surprises & Budgeting](#5-billing-cost-surprises--budgeting)
- [6. Deep-Dive Interview Scenarios & Diagnostics](#6-deep-dive-interview-scenarios--diagnostics)

---

## 1. Core Foundational Concepts

### Q1: What is Cloud Computing according to AWS, and what are its six core advantages?
**Answer**:
Cloud computing is the on-demand delivery of IT resources (compute, database, storage, networking) over the internet with pay-as-you-go pricing. Instead of buying, owning, and maintaining physical data centers and servers, organizations access technology services on an as-needed basis from a cloud provider.

The six core advantages of cloud computing are:
1. **Trade Capital Expense for Variable Expense**: Pay only when you consume resources, eliminating multi-million-dollar upfront hardware procurement (CapEx to OpEx).
2. **Benefit from Massive Economies of Scale**: AWS aggregates usage from hundreds of thousands of customers, driving higher efficiencies that translate into lower pay-as-you-go prices.
3. **Stop Guessing Capacity**: Eliminate over-provisioning (idle servers costing money) and under-provisioning (crashed sites during traffic spikes) by scaling elastically in real-time.
4. **Increase Speed and Agility**: Provision new compute, storage, and database resources in seconds with code, dramatically reducing deployment cycles from months to minutes.
5. **Stop Spending Money Running Data Centers**: Focus engineering efforts on business differentiators (applications, customer experiences) rather than "undifferentiated heavy lifting" (power, cooling, rack cabling, physical security).
6. **Go Global in Minutes**: Deploy applications across multiple geographic regions worldwide with a few clicks or Terraform commands to deliver low latency to end users.

### Q2: What are the differences between IaaS, PaaS, and SaaS, and how do they map to AWS services?
**Answer**:
- **Infrastructure as a Service (IaaS)**: Highest level of user control and management over underlying infrastructure. AWS provides virtualized hardware, networking, and storage; the customer manages OS patching, runtimes, middleware, and applications.
  - *AWS Examples*: Amazon EC2, Amazon EBS, Amazon VPC.
- **Platform as a Service (PaaS)**: AWS manages the underlying hardware, operating system, runtime, and scaling; the customer focuses solely on deploying application code and configuring environment variables.
  - *AWS Examples*: AWS Elastic Beanstalk, AWS App Runner.
- **Software as a Service (SaaS)**: Complete end-to-end software product managed and operated entirely by the provider. The user consumes the service via web UI or API with zero infrastructure management.
  - *AWS Examples*: Amazon WorkSpaces, Amazon QuickSight, AWS Marketplace SaaS offerings.
- **Serverless (Function as a Service - FaaS)**: A specialized evolution beyond PaaS where compute executes only in response to events, automatically scales from zero to tens of thousands of instances, and charges strictly per millisecond of execution.
  - *AWS Examples*: AWS Lambda, AWS Fargate, Amazon Aurora Serverless.

---

## 2. Global Infrastructure: Regions, AZs, and Edge Locations

### Q3: What is the technical difference between an AWS Region, an Availability Zone (AZ), and an Edge Location?
**Answer**:
- **AWS Region**: A separate, physically isolated geographic area (e.g., `us-east-1` in N. Virginia, `eu-west-1` in Ireland). Each region operates completely independently of other regions and contains a minimum of three isolated Availability Zones connected via low-latency, redundant private fiber networks.
- **Availability Zone (AZ)**: One or more discrete physical data centers within a Region, each with independent redundant power, networking, and flood/fire protection. AZs within a region are separated by a meaningful physical distance (typically tens of kilometers) to prevent correlated disaster failures, while maintaining sub-2 millisecond latency to support synchronous database replication.
- **Edge Location**: A Point of Presence (PoP) located in major metropolitan areas worldwide (over 450+ PoPs). Edge locations run **Amazon CloudFront** (CDN), **Route 53** (DNS), and **AWS Global Accelerator** to cache content, terminate TLS connections close to end users, and accelerate internet ingress onto the private AWS global network.

### Q4: An engineer logs into the AWS Console and panics because all EC2 instances, S3 buckets, and RDS databases have "disappeared." What is the most likely cause?
**Answer**:
The engineer has selected the wrong **AWS Region** in the console navigation bar. 
With the exception of global services (such as IAM, Route 53, CloudFront, and AWS Organizations, which operate under a global endpoint), almost all AWS services are scoped regionally. Resources provisioned in `us-east-1` will not appear in the console when `us-west-2` or `eu-central-1` is selected. The engineer should check the region selector in the top-right corner or execute `aws ec2 describe-instances --region <target-region>` via the CLI.

### Q5: What is the difference between Multi-AZ and Multi-Region architectures, and when should you choose each?
**Answer**:
- **Multi-AZ Architecture**: Workloads are distributed across multiple Availability Zones within a single AWS Region.
  - *Purpose*: High Availability (HA) against localized hardware, power, or networking failures in a single data center.
  - *Latency*: Sub-2ms synchronous data replication.
  - *Failover*: Automated in 60-120 seconds (e.g., Amazon RDS Multi-AZ).
  - *Cost*: Low cross-AZ data transfer fees (\$0.01/GB).
- **Multi-Region Architecture**: Workloads are replicated across two or more geographically distinct Regions (e.g., `us-east-1` and `us-west-2`).
  - *Purpose*: Disaster Recovery (DR) against regional catastrophic events, fiber cuts, or regulatory data residency requirements.
  - *Latency*: Asynchronous replication (typical lag: hundreds of milliseconds to seconds) due to speed-of-light physical distance.
  - *Failover*: DNS-based (Route 53) or Anycast (Global Accelerator), taking seconds to minutes.
  - *Cost*: High; duplicated compute infrastructure and cross-region egress data transfer (\$0.02/GB).

---

## 3. The Shared Responsibility Model

### Q6: Explain the AWS Shared Responsibility Model. Where does the boundary lie for EC2 vs AWS Lambda vs Amazon S3?
**Answer**:
The Shared Responsibility Model differentiates between **Security OF the Cloud** (AWS responsibility) and **Security IN the Cloud** (Customer responsibility):
- **AWS Responsibility (Security OF the Cloud)**: Protecting the global infrastructure, physical data centers, host hardware, hypervisors, physical network virtualization, and the operational facilities running AWS services.
- **Customer Responsibility (Security IN the Cloud)**: Customer data, IAM identities and access policies, operating system configuration, network firewall rules (Security Groups and NACLs), application code, and data encryption (at rest and in transit).

**How the boundary shifts by service type**:
1. **Amazon EC2 (IaaS)**:
   - *Customer owns*: Operating system installation, security patching, guest OS firewall (iptables), user accounts, runtime dependencies, and network Security Groups.
   - *AWS owns*: Physical server rack, hypervisor (Nitro System), physical cabling, and power.
2. **Amazon S3 (Managed Storage)**:
   - *Customer owns*: Bucket policies, IAM user permissions, Block Public Access configuration, object versioning, and choosing KMS encryption keys.
   - *AWS owns*: Storage hardware disk maintenance, multi-datacenter data replication (11 9s durability), and underlying storage operating systems.
3. **AWS Lambda (Serverless Compute)**:
   - *Customer owns*: Application code, third-party libraries packaged in the zip/container, IAM execution roles, and function timeout/concurrency settings.
   - *AWS owns*: Operating system security patching, container isolation (Firecracker MicroVMs), language runtime updates, and compute capacity scaling.

---

## 4. Account Management & Governance

### Q7: Why is it an architectural anti-pattern to use the AWS Account Root User for daily administrative operations?
**Answer**:
The AWS Account Root User is the identity created when the account is initially registered. It possesses absolute, unrestricted administrative privileges that **cannot be revoked or restricted by any IAM policy or Service Control Policy (SCP)**.
- *Risks*: If root credentials (email and password) are compromised or an API access key is leaked, an attacker can permanently delete all resources, close the account, transfer hosted zones, or spin up untraceable crypto-mining fleets.
- *Best Practices*:
  1. Secure the root account with a physical hardware MFA token (FIDO2 / YubiKey).
  2. Immediately delete all static root Access Keys (`AKIA...`).
  3. Create an administrative IAM Identity Center (SSO) user or IAM role for daily tasks.
  4. Retain the root user strictly for tasks that specifically require root (e.g., closing the account, changing account support tiers, or configuring AWS GovCloud access).

### Q8: What is AWS Organizations, and how do Service Control Policies (SCPs) work?
**Answer**:
**AWS Organizations** is an account management service that allows enterprises to consolidate multiple AWS accounts into an organizational hierarchy managed from a central Management Account. Accounts are grouped into **Organizational Units (OUs)**.

**Service Control Policies (SCPs)** are guardrail policies attached to the Organization Root, an OU, or individual accounts:
- **Filtering, Not Granting**: SCPs *never* grant permissions; they define the **maximum allowable permissions** across member accounts.
- **Evaluation Rule**: An action is permitted only if it is explicitly allowed by IAM policies *and* not denied by any applicable SCP.
- **Precedence**: An explicit `Deny` in an SCP overrides everything—including the local account root user and IAM administrators with `AdministratorAccess` (`*:*`).
- **Common Enterprise SCPs**: Denying the creation of resources in unapproved geographic regions; preventing member accounts from disabling AWS CloudTrail or Amazon GuardDuty; preventing public S3 bucket policies.

---

## 5. Billing, Cost Surprises & Budgeting

### Q9: An AWS Budget is set to \$100/month. What happens when your account spend hits \$100?
**Answer**:
**Nothing stops or shuts down automatically.**
AWS Budgets is an alerting service, not a resource execution killer. When the threshold is reached (actual or forecasted spend), AWS Budgets sends notification emails or publishes an alert message to an Amazon SNS topic. Your EC2 instances, RDS databases, and Lambda functions continue running and accumulating charges uninterrupted.
To actively halt spending, you must attach an **AWS Budgets Action** (which can execute an automated SSM runbook, apply a restrictive SCP, or invoke a Lambda function to shut down non-critical instances), though hard-stopping production workloads risks catastrophic data corruption.

### Q10: What are the top 5 unexpected "cost surprises" that enterprise DevOps teams encounter on AWS?
**Answer**:
1. **Managed NAT Gateways**: Billed at \$0.045 per hour (\$32.40/month per gateway) plus \$0.045 per GB processed. Streaming massive S3 or public API traffic through a NAT Gateway rather than using free S3 Gateway Endpoints can result in thousands of dollars in unintended data processing fees.
2. **Idle Resources**: Unattached Amazon EBS volumes (orphaned after EC2 termination), unattached Elastic IPs (\$0.005/hour when idle), forgotten RDS manual snapshots, and idle Application Load Balancers (\$22+/month each).
3. **Cross-AZ and Inter-Region Data Transfer**: While data transfer within the same AZ is free, traffic across Availability Zones incurs \$0.01/GB in each direction (\$0.02/GB round-trip), and cross-region egress costs \$0.02/GB.
4. **CloudWatch Logs Ingestion and Infinite Retention**: Ingesting verbose debug logs costs \$0.50 per GB. Without an explicit retention policy (default is "Never Expire"), logs accumulate perpetually, causing storage bills to grow monotonically.
5. **Public IPv4 Address Charges**: Since February 2024, AWS charges \$0.005 per hour (\$3.65/month) for every public IPv4 address assigned to running instances, NAT gateways, and load balancers.

---

## 6. Deep-Dive Interview Scenarios & Diagnostics

### Scenario 1: Debugging an "Access Denied" Error on AWS CLI
**Interviewer**: *"A developer runs `aws s3 cp file.txt s3://corporate-data/` and receives `An error occurred (AccessDenied) when calling the PutObject operation`. How do you systematically debug this?"*

**Structured Candidate Response**:
I would troubleshoot this using a 6-step diagnostic sequence:
1. **Verify Caller Identity**: Execute `aws sts get-caller-identity` to confirm the developer is authenticated as the expected IAM role or user, and check that they are operating in the intended AWS account.
2. **Check IAM Identity Policy**: Verify that the IAM user or role has `s3:PutObject` permitted on the exact resource ARN: `arn:aws:s3:::corporate-data/*` (note the trailing `/*` required for object-level actions).
3. **Check S3 Bucket Policy & ACLs**: Inspect the bucket policy for explicit `Deny` statements or conditions requiring specific VPCs, IP ranges, or encryption headers.
4. **Inspect AWS KMS Permissions**: If the bucket is encrypted with an AWS KMS Customer Managed Key (CMK), verify that the IAM caller has `kms:GenerateDataKey` and `kms:Encrypt` permissions on the KMS key. A failure in KMS permissions manifests as an S3 `AccessDenied`.
5. **Check Account Guardrails (SCPs & Permission Boundaries)**: If the account is part of an AWS Organization, verify that no Service Control Policy (SCP) or IAM Permission Boundary on the role denies S3 writes.
6. **Inspect AWS CloudTrail Events**: Query CloudTrail for the failed `PutObject` event. CloudTrail logs often provide detailed authorization failure messages (e.g., `Access Denied by explicit deny in SCP`).

### Scenario 2: Choosing between On-Demand, Savings Plans, and Spot Instances
**Interviewer**: *"You are modernizing an enterprise compute fleet consisting of: (1) Core production payment API running 24/7, (2) Nightly 4-hour batch ETL job that can tolerate restarts, and (3) A new experimental machine learning model. How do you allocate AWS purchasing options?"*

**Structured Candidate Response**:
- **Workload 1 (Core Payment API - Steady 24/7)**:
  - *Purchasing Option*: **Compute Savings Plans** (1-year or 3-year term).
  - *Justification*: Provides up to 66% discount compared to On-Demand while offering flexible application across EC2 instance families, AWS Fargate, and AWS Lambda regardless of region or OS.
- **Workload 2 (Nightly 4-Hour Batch ETL - Fault Tolerant)**:
  - *Purchasing Option*: **EC2 Spot Instances**.
  - *Justification*: Provides up to 90% discount off On-Demand rates. Because the ETL job is designed to checkpoint and tolerate instance terminations (interruption notices received 2 minutes prior via EventBridge), Spot delivers massive cost savings.
- **Workload 3 (Experimental ML Model - Short-Lived / Unpredictable)**:
  - *Purchasing Option*: **On-Demand Instances**.
  - *Justification*: Zero commitment or upfront cost. When experimentation is complete, instances are terminated immediately without unused capacity penalties. Once usage stabilizes into production, it can be transitioned into a Savings Plan.
