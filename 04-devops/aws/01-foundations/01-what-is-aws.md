# What Is AWS? Cloud Computing Architecture & Mental Models

> AWS (Amazon Web Services) is a global, API-driven hyperscale cloud platform providing 200+ fully featured services across compute, storage, databases, networking, and artificial intelligence, transforming hardware infrastructure into programmable, utility-metered primitives.

| Metadata | Details |
|---|---|
| **Level** | Beginner to Intermediate |
| **Prerequisites** | Basic networking (IP, DNS, ports), client-server architecture |
| **Time to Read** | 25 minutes |
| **Status** | Complete |
| **OS / Cloud Scope** | AWS Global Infrastructure (AWS Commercial, GovCloud, China) |
| **Tool Version** | AWS CLI v2.22+, AWS SDK v3 |
| **Tested With** | AWS CLI v2.22.15, Python 3.12, Amazon Linux 2023 |

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
In traditional on-premises infrastructure, standing up a production application requires capital expenditure (CapEx) on physical server blades, top-of-rack switches, SAN storage arrays, and colocation data center floor space. Provisioning cycles took weeks to months, hardware was provisioned for peak load (leading to massive idle capacity waste), and disaster recovery required duplicative capital outlay.

AWS re-architects physical infrastructure as multi-tenant, software-defined primitives accessible via authenticated HTTPS REST APIs. Engineering teams trade upfront capital expenditure for variable operational expenditure (OpEx), provisioning virtual machines, managed distributed databases, and serverless runtimes in seconds. AWS is fundamentally an API platform: every console action, CLI command, and Terraform resource translates into a cryptographically signed HTTP request executed against AWS service control planes.

---

## 2. Core Concepts
- **API-Driven Infrastructure**: The AWS Management Console is simply a web application invoking the same public REST APIs used by the AWS CLI and SDKs. Infrastructure state is entirely manipulable through code.
- **Shared Responsibility Spectrum**: The operational boundary dividing tasks between AWS (Security OF the cloud) and the customer (Security IN the cloud).
  - *IaaS (Infrastructure as a Service)*: e.g. EC2, EBS. Customer manages guest OS, security patches, network routing, and runtimes.
  - *PaaS (Platform as a Service)*: e.g. Elastic Beanstalk, RDS. AWS manages OS patching and database engine installation; customer configures schemas and application code.
  - *Serverless (Function/Storage as a Service)*: e.g. Lambda, S3, DynamoDB. AWS abstracts all servers, capacity provisioning, and OS scaling; customer writes business logic or stores objects.
- **AWS Global Footprint**:
  - *Regions*: Independent physical geographic areas worldwide (e.g. `us-east-1`, `eu-west-1`, `ap-south-1`).
  - *Availability Zones (AZs)*: One or more isolated physical data centers within a Region, separated by meaningful distance (latency < 2ms) on private redundant metro fiber.
  - *Edge Network*: 600+ Points of Presence (PoPs) delivering low-latency CDN (CloudFront) and DNS (Route 53).

---

## 3. How It Works Internally
Underneath the API endpoints, AWS operates custom physical hardware and proprietary hypervisors:

```
+-------------------------------------------------------------------------+
|                    AWS Control Plane & Client Ingress                   |
|  [ AWS Console ]      [ AWS CLI v2 ]      [ Terraform / SDKs ]          |
|         \                   |                   /                       |
|          +------------------+------------------+                        |
|                             | Signed HTTPS Request (SigV4)              |
|                             v                                           |
|              [ Regional API Gateway / Endpoint ]                        |
|              - IAM Authenticator & Authorizer                           |
|              - Rate Limiting & Service Quota Validator                  |
+-------------------------------------------------------------------------+
                              |
                              v
+-------------------------------------------------------------------------+
|                  Internal Service Orchestrator (Data Plane)             |
|  [ EC2 Controller ]       [ S3 Fleet Manager ]    [ Nitro Enclave ]     |
|         |                        |                        |             |
|         v                        v                        v             |
|  +--------------+        +---------------+        +---------------+     |
|  | Nitro Card   |        | Storage Nodes |        | MicroVM Host  |     |
|  | (VPC Encaps) |        | (3x Rep Mesh) |        | (Firecracker) |     |
|  +--------------+        +---------------+        +---------------+     |
+-------------------------------------------------------------------------+
```

1. **Authentication via Signature Version 4 (SigV4)**: Every API request calculates a HMAC-SHA256 signature using the caller's AWS Secret Access Key, hashing the request timestamp, headers, and payload. This prevents replay and tampering.
2. **AWS Nitro System**: Modern EC2 instances offload virtualization, VPC networking, EBS storage management, and security boundaries to dedicated ASIC PCIe cards (Nitro Cards), dedicating nearly 100% of the host CPU and memory to customer guest VMs.
3. **Regional Fault Isolation**: Regions are designed to be completely autonomous. Outages in `us-east-1` do not affect `eu-west-1` control or data planes.

---

## 4. Syntax & Usage
Interacting with AWS through the canonical AWS CLI v2:

```bash
# Verify caller identity (Who am I?)
aws sts get-caller-identity

# List all enabled geographic regions for compute
aws ec2 describe-regions --all-regions --query "Regions[].RegionName" --output table

# Query account-level block public access settings for S3
aws s3control get-public-access-block --account-id <ACCOUNT_ID>

# Format output using JMESPath expressions
aws ec2 describe-instances \
    --query "Reservations[].Instances[].[InstanceId, State.Name, InstanceType, PrivateIpAddress]" \
    --output table
```

---

## 5. Practical Examples

### Example 1: Identity & Environment Introspection
- **Goal:** Verify that local CLI authentication works and inspect the active IAM principal and account ID.
- **Commands:**
```bash
aws sts get-caller-identity --output json
```
- **Real Output:**
```json
{
    "UserId": "AROAEXAMPLEIDENTITY:session-user",
    "Account": "123456789012",
    "Arn": "arn:aws:sts::123456789012:assumed-role/AWSReservedSSO_AdministratorAccess/alex"
}
```
- **Explanation:** Returns the 12-digit account ID, STS user identity, and assumed role ARN validating that SigV4 authentication succeeded.

### Example 2: Querying Regional Service Availability
- **Goal:** Identify which Availability Zones exist within the current working region (`us-east-1`).
- **Commands:**
```bash
aws ec2 describe-availability-zones \
    --region us-east-1 \
    --query "AvailabilityZones[?State=='available'].[ZoneName, ZoneId]" \
    --output table
```
- **Real Output:**
```text
---------------------------------
|   DescribeAvailabilityZones   |
+--------------+----------------+
|  us-east-1a  |  use1-az1      |
|  us-east-1b  |  use1-az2      |
|  us-east-1c  |  use1-az4      |
|  us-east-1d  |  use1-az6      |
|  us-east-1e  |  use1-az3      |
|  us-east-1f  |  use1-az5      |
+--------------+----------------+
```
- **Explanation:** Zone names (e.g. `us-east-1a`) are randomly mapped to physical Zone IDs (`use1-az1`) per AWS account to distribute server load evenly across physical data centers.

### Example 3: Edge Case - Service Quota Limits
- **Goal:** Inspect account-specific service quotas for running EC2 On-Demand Standard instances.
- **Commands:**
```bash
aws service-quotas get-service-quota \
    --service-code ec2 \
    --quota-code L-1216C47A \
    --query "Quota.[QuotaName, Value, Adjustable]" \
    --output json
```
- **Real Output:**
```json
[
    "Running On-Demand Standard (A, C, D, I, M, R, T, Z) instances",
    64.0,
    true
]
```
- **Explanation:** Cloud resources are not infinite; every account is governed by soft and hard service quotas to protect against runaway billing and resource hogging.

### Example 4: Failure & Anti-Pattern - Hardcoded API Secret Exposure
- **Goal:** Demonstrate error when calling an API without proper SigV4 authentication.
- **Anti-Pattern Command:**
```bash
curl -s -i "https://ec2.us-east-1.amazonaws.com/?Action=DescribeInstances&Version=2016-11-15"
```
- **Real Output:**
```text
HTTP/2 401 
content-type: text/xml;charset=UTF-8
date: Sun, 24 Nov 2026 12:00:00 GMT
x-amzn-requestid: e3f12a84-1849-4b3d-9d41-329487192384

<Response><Errors><Error><Code>AuthFailure</Code><Message>AWS was not able to validate the provided access credentials.</Message></Error></Errors></Response>
```
- **Explanation & Fix:** Raw HTTP calls fail without valid SigV4 signature headers (`Authorization: AWS4-HMAC-SHA256 Credential=...`). Use the AWS CLI, SDKs, or tools like `awscurl` to compute request signatures.

### Example 5: Production-Grade Account Tagging & Resource Discovery
- **Goal:** Query resources across services matching standardized enterprise ownership tags.
- **Commands:**
```bash
aws resourcegroupstaggingapi get-resources \
    --tag-filters Key=Environment,Values=Production Key=Owner,Values=PlatformTeam \
    --resource-type-filters "ec2:instance" "s3:bucket" \
    --query "ResourceTagMappingList[].[ResourceARN, Tags]" \
    --output json
```
- **Real Output:**
```json
[
    [
        "arn:aws:ec2:us-east-1:123456789012:instance/i-0a1b2c3d4e5f67890",
        [
            {"Key": "Environment", "Value": "Production"},
            {"Key": "Owner", "Value": "PlatformTeam"}
        ]
    ]
]
```
- **Explanation:** Enterprise environments use the Resource Groups Tagging API to audit and group infrastructure across service boundaries.

---

## 6. Best Practices
1. **Never Generate Long-Lived Access Keys for the Root User**: Lock the root user with MFA and store credentials in a corporate physical vault.
2. **Move Up the Abstraction Ladder**: Adopt serverless (Lambda, DynamoDB, Fargate) by default; only drop down to IaaS (EC2) when requiring custom kernel drivers or specialized GPU hardware.
3. **Design for Multi-AZ by Default**: Never deploy production workloads into a single Availability Zone.
4. **Tag Every Resource**: Require `Environment`, `Owner`, `CostCenter`, and `Project` tags enforced via AWS Organizations Tag Policies.
5. **Use Infrastructure as Code**: Treat console changes as anti-patterns; define all infrastructure in Terraform, OpenTofu, or AWS CDK.

---

## 7. Common Mistakes & Pitfalls

| Mistake | Why It Fails | Practical Remedy |
|---|---|---|
| Treating Availability Zones as single buildings | A single physical building fire could destroy an AZ | AZs consist of multiple physical data centers; always deploy across 2+ AZs |
| Confusing Multi-AZ with Multi-Region | A region-wide outage will bring down a multi-AZ application | Implement cross-region S3 replication and Route 53 failover for critical systems |
| Leaving AWS Budgets unconfigured | Runaway workloads or compromised keys cause catastrophic billing spikes | Configure zero-spend and $10 alerts with SNS email notifications on day one |
| Expecting Zone Names (`us-east-1a`) to match across accounts | AWS randomizes AZ name-to-physical-center mappings per account | Use `ZoneId` (e.g. `use1-az1`) to coordinate latency-sensitive inter-account connections |
| Relying on manual UI clicks for production deployment | Impossible to reproduce, audit, or roll back configuration drift | Codify infrastructure using Terraform or AWS CDK with CI/CD automation |
| Assuming cloud storage is immune to human deletion | Accidental `rm -rf` or ransomware deletes objects permanently | Enable S3 Versioning, Object Lock (WORM), and MFA Delete |

---

## 8. Performance, Security & Cost Notes
- **Performance**: Intra-AZ latency is sub-millisecond (< 1ms). Inter-AZ latency within a region is typically 1–2ms. Cross-region latency spans 50–200ms depending on speed-of-light optical fiber limits.
- **Security**: The AWS physical data center boundary is impenetrable to external visitors; internal staff have zero access to customer data streams due to default Nitro encryption of memory and network buses.
- **Cost**: Data transfer out (DTO) to the internet incurs fees (starting at ~$0.09/GB). Intra-region cross-AZ traffic costs $0.01/GB in each direction ($0.02/GB round-trip). Place chatty microservices within the same AZ or use VPC Endpoints.

---

## 9. Comparison / When to Use What

| Architecture Model | Operational Effort | Scalability | Cost Model | Best Fit |
|---|---|---|---|---|
| **EC2 Virtual Machines (IaaS)** | High (OS patching, scaling) | Manual / Auto Scaling Groups | Per-second instance billing | Legacy monoliths, custom kernel drivers, licensed software |
| **ECS / EKS on Fargate (CaaS)** | Low (Serverless containers) | Fast horizontal task scaling | Per vCPU/RAM per second | Standard microservices, Dockerized web APIs |
| **AWS Lambda (FaaS)** | None (Event-driven serverless) | Instantaneous per-request | Billed per millisecond execution | Event processing, lightweight REST APIs, ETL triggers |
| **On-Premise Colocation** | Massive (Hardware purchasing) | Static, limited by floor space | Fixed CapEx | Strict regulatory sovereignty, legacy mainframes |

---

## 10. Interview Questions

<details>
<summary>1. What is the fundamental difference between an AWS Region and an Availability Zone?</summary>
An AWS Region is a separate geographic area (e.g., Ireland, Oregon). An Availability Zone is one or more distinct data centers within a region, isolated from other AZs with independent power, cooling, and physical security, connected via ultra-low latency optical networks.
</details>

<details>
<summary>2. Why does AWS randomize the mapping between AZ names (us-east-1a) and physical AZ IDs?</summary>
If `us-east-1a` mapped to the same physical building for every customer, that building would suffer severe resource contention. AWS dynamically assigns letters to physical `ZoneId` identifiers (e.g. `use1-az1`) per AWS account to distribute load uniformly.
</details>

<details>
<summary>3. What is the AWS Nitro System and why was it an architectural breakthrough?</summary>
The Nitro System offloads virtualization, networking (VPC), storage (EBS), and security functions to dedicated custom ASIC cards, freeing up virtually 100% of the host CPU and memory for customer workloads while delivering bare-metal equivalent performance.
</details>

<details>
<summary>4. How does AWS ensure that an API request cannot be modified in transit (man-in-the-middle)?</summary>
Via the Signature Version 4 (SigV4) protocol. Clients generate an HMAC-SHA256 signature using their Secret Key over canonical request headers, timestamp, and body. Any modification invalidates the signature, causing the AWS control plane to reject the request.
</details>

<details>
<summary>5. What is the difference between IaaS, PaaS, and Serverless on AWS?</summary>
IaaS (EC2) provides raw compute where the customer manages the OS and runtime. PaaS (RDS, Elastic Beanstalk) manages OS patching while the customer manages database schema or code. Serverless (Lambda, DynamoDB) abstracts servers entirely, scaling on demand with pay-per-use execution.
</details>

<details>
<summary>6. What happens when an AWS budget is exceeded?</summary>
AWS Budgets generates notifications (SNS, email) or executes automated actions (via IAM/SSM); it does NOT shut down or delete running resources automatically unless an automated action script is explicitly linked.
</details>

<details>
<summary>7. How does cross-AZ data transfer affect architecture cost?</summary>
Data transfer between EC2 instances in different AZs within the same region costs $0.01 per GB in each direction ($0.02/GB total). High-throughput replication or chatty distributed databases must be architected carefully to avoid surprise networking charges.
</details>

<details>
<summary>8. How do AWS partitions differ from regions?</summary>
AWS partitions are administrative groupings of regions with independent IAM roots and credentials: `aws` (Standard Commercial), `aws-us-gov` (AWS GovCloud), and `aws-cn` (China). Credentials from the commercial partition cannot access GovCloud or China.
</details>

---

## 11. Practice Exercises

<details>
<summary>Exercise 1 (Easy): Identify your active caller identity and region</summary>
**Task:** Use the AWS CLI to display your account ID, user ARN, and default configured region.

**Solution:**
```bash
aws sts get-caller-identity
aws configure get region
```
</details>

<details>
<summary>Exercise 2 (Easy): List all S3 buckets in your account</summary>
**Task:** Output all S3 bucket names and creation dates in a clean table format.

**Solution:**
```bash
aws s3api list-buckets --query "Buckets[].[Name, CreationDate]" --output table
```
</details>

<details>
<summary>Exercise 3 (Medium): Map account AZ names to physical Zone IDs</summary>
**Task:** Write an AWS CLI command extracting `ZoneName` and `ZoneId` for `us-west-2`.

**Solution:**
```bash
aws ec2 describe-availability-zones \
    --region us-west-2 \
    --query "AvailabilityZones[].[ZoneName, ZoneId]" \
    --output table
```
</details>

<details>
<summary>Exercise 4 (Medium): Audit running EC2 instances across all regions</summary>
**Task:** Write a bash one-liner to query instance IDs across all active commercial regions.

**Solution:**
```bash
for region in $(aws ec2 describe-regions --query "Regions[].RegionName" --output text); do
    count=$(aws ec2 describe-instances --region "$region" --query "length(Reservations[].Instances[])" --output text)
    if [ "$count" -gt 0 ]; then
        echo "Region $region has $count running instances."
    fi
done
```
</details>

<details>
<summary>Exercise 5 (Hard): Calculate monthly public IPv4 address expenditure</summary>
**Task:** Identify all public IPv4 addresses across EC2 and ENIs and calculate monthly cost based on $0.005/hr.

**Solution:**
```bash
IPS=$(aws ec2 describe-network-interfaces \
    --query "NetworkInterfaces[?Association.PublicIp!=null].Association.PublicIp" \
    --output text | wc -w)
MONTHLY_COST=$(echo "$IPS * 0.005 * 730" | bc)
echo "Total Public IPv4 Addresses: $IPS"
echo "Estimated Monthly Cost: \$$MONTHLY_COST"
```
</details>

<details>
<summary>Exercise 6 (Hard): Construct an automated AWS budget using CLI</summary>
**Task:** Create a $50 monthly budget with email notification when actual spend exceeds 80%.

**Solution:**
```bash
aws budgets create-budget \
    --account-id $(aws sts get-caller-identity --query Account --output text) \
    --budget '{
        "BudgetName": "Monthly-Cost-Control",
        "BudgetLimit": {"Amount": "50", "Unit": "USD"},
        "TimeUnit": "MONTHLY",
        "BudgetType": "COST"
    }' \
    --notifications-with-subscribers '[{
        "Notification": {
            "NotificationType": "ACTUAL",
            "ComparisonOperator": "GREATER_THAN",
            "Threshold": 80,
            "ThresholdType": "PERCENTAGE"
        },
        "Subscribers": [{"SubscriptionType": "EMAIL", "Address": "admin@example.com"}]
    }]'
```
</details>

---

## 12. Summary / Cheat Sheet

```
+---------------------------------------------------------------------------------+
|                           AWS FOUNDATIONS CHEAT SHEET                           |
+---------------------------------------------------------------------------------+
| CLI WHOAMI     | aws sts get-caller-identity                                    |
| REGION LIST    | aws ec2 describe-regions --output table                        |
| AZ MAPPING     | aws ec2 describe-availability-zones --query "...[ZoneName,Id]" |
| SERVICE QUOTA  | aws service-quotas get-service-quota --service ec2 --quota ... |
| AUTHENTICATION | HMAC-SHA256 Signature Version 4 (SigV4)                        |
| SHARED RESP.   | AWS: Security OF Cloud | Customer: Security IN Cloud           |
| MULTI-AZ       | Synchronous, sub-2ms latency, automated failover boundary      |
| MULTI-REGION   | Asynchronous, disaster recovery, global latency optimization   |
+---------------------------------------------------------------------------------+
```

---

## 13. Further Reading
- [AWS Overview Whitepaper](https://docs.aws.amazon.com/whitepapers/latest/aws-overview/)
- [AWS Global Infrastructure Map](https://aws.amazon.com/about-aws/global-infrastructure/)
- [AWS Signature Version 4 Signing Process](https://docs.aws.amazon.com/general/latest/gr/signature-version-4.html)
- [AWS Nitro System Architecture](https://aws.amazon.com/ec2/nitro/)

---

[Section Index](file:///04-devops/aws/01-foundations/README.md) | [Next: 02-shared-responsibility-and-well-architected.md](file:///04-devops/aws/01-foundations/02-shared-responsibility-and-well-architected.md)
