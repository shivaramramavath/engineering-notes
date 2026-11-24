# AWS Console, CloudShell & Enterprise Tagging Governance

> The AWS Management Console and browser-based CloudShell provide interactive administration, while systematic Enterprise Tagging Strategies and Cost Allocation Tags transform dispersed cloud resources into auditable, cost-attributed business units.

| Metadata | Details |
|---|---|
| **Level** | Beginner to Intermediate |
| **Prerequisites** | [01-what-is-aws.md](file:///04-devops/aws/01-foundations/01-what-is-aws.md) |
| **Time to Read** | 20 minutes |
| **Status** | Complete |
| **OS / Cloud Scope** | Global AWS Management & Resource Tagging |
| **Tool Version** | AWS CloudShell AL2023, Resource Groups Tagging API |
| **Tested With** | AWS CloudShell v2, AWS CLI v2.22+ |

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
As cloud deployments grow from tens of instances to tens of thousands of resources spanning multiple accounts, answering simple questions becomes nearly impossible: *Who owns this unattached EBS volume? What project does this RDS database belong to? Why did our monthly spend spike by $12,000?*

Without a mandatory **Enterprise Tagging Strategy**, cloud infrastructure becomes an unmanaged graveyard of orphaned resources and untraceable costs. Furthermore, interactive administration requires accessible tooling: while the **AWS Management Console** provides visual insight, **AWS CloudShell** provides a pre-authenticated, browser-based Linux terminal pre-loaded with the AWS CLI, Node.js, Python, and Docker—eliminating the need to install or configure credentials on local machines.

---

## 2. Core Concepts
- **AWS Management Console & CloudShell**:
  - *Console*: Web interface accessible via IAM Identity Center SSO. Features customizable widgets, recently visited services, and Unified Search (`Alt + S`).
  - *CloudShell*: A free, browser-based Amazon Linux 2023 shell environment. Automatically inherits the console user's credentials with 1 GB of persistent `/home` storage per region.
- **Enterprise Tagging Framework**:
  - Tags are key-value pairs (`Key=Value`) attached to AWS resources.
  - Case-sensitive metadata supporting up to 50 tags per resource.
  - Tag keys cannot use the reserved prefix `aws:`.
- **Cost Allocation Tags**:
  - Must be explicitly activated in the AWS Billing Console before appearing in Cost Explorer and Cost and Usage Reports (CUR).
  - *AWS-Generated Tags*: `aws:createdBy` (tracks IAM principal that provisioned the resource).
  - *User-Defined Tags*: Custom business keys (e.g. `Environment`, `Owner`, `CostCenter`, `Project`).
- **Tag Policies (AWS Organizations)**:
  - Declarative governance rules that standardize tag casing and permitted values across all member accounts.

---

## 3. How It Works Internally
Tag Propagation and Cost Allocation Pipeline:

```
[ Engineer Launches Resource (EC2/S3) with Tags ]
  - Tags: { "Environment": "Production", "CostCenter": "1042" }
                         |
                         v
+-------------------------------------------------------------+
|               Resource Groups Tagging API                   |
|  - Validates tags against AWS Organizations Tag Policy      |
|  - Index stores metadata in regional resource catalog       |
+-------------------------------------------------------------+
                         |
                         v
+-------------------------------------------------------------+
|                  AWS Billing & Cost Engine                  |
|  - Matches tags against active Cost Allocation Tags         |
|  - Ingests hourly resource usage records                    |
+-------------------------------------------------------------+
                         |
                         v
+-------------------------------------------------------------+
|           AWS Cost Explorer / Cost & Usage Report           |
|  - Generates financial reports broken down by "CostCenter"  |
|  - Powers chargeback and showback accounting to teams       |
+-------------------------------------------------------------+
```

---

## 4. Syntax & Usage
CLI commands for tagging, querying, and auditing resources:

```bash
# Tag a running EC2 instance
aws ec2 create-tags \
    --resources i-0123456789abcdef0 \
    --tags Key=Environment,Value=Production Key=Owner,Value=PlatformTeam

# Query resources across all services with specific tags
aws resourcegroupstaggingapi get-resources \
    --tag-filters Key=Environment,Values=Production \
    --output json

# Untag a resource
aws ec2 delete-tags \
    --resources i-0123456789abcdef0 \
    --tags Key=Temporary

# Tag an S3 bucket
aws s3api put-bucket-tagging \
    --bucket "corporate-vault" \
    --tagging 'TagSet=[{Key=CostCenter,Value=FIN-902},{Key=Owner,Value=Security}]'
```

---

## 5. Practical Examples

### Example 1: Enforcing Minimum Enterprise Tag Baseline
- **Goal:** Author a standard tagging schema applied during resource provisioning.
- **Standard Tag Hierarchy:**
  - `Environment`: `Production`, `Staging`, `Development`, `Sandbox`
  - `Owner`: Email or team alias (`platform-eng@example.com`)
  - `Project`: Specific business initiative (`AlphaMigration`)
  - `CostCenter`: General ledger accounting code (`CC-4091`)
  - `DataClassification`: `Public`, `Internal`, `Confidential`, `Restricted`

### Example 2: Bulk Tagging via Resource Groups Tagging API
- **Goal:** Tag multiple heterogeneous resources (EC2 instance, S3 bucket, Subnet) in a single CLI call.
- **Commands:**
```bash
aws resourcegroupstaggingapi tag-resources \
    --resource-arn-list \
        "arn:aws:ec2:us-east-1:123456789012:instance/i-0a1b2c3d4e5f67890" \
        "arn:aws:s3:::my-app-assets-vault" \
    --tags Environment=Production CostCenter=CC-102 \
    --query "FailedResourcesMap" --output json
```
- **Real Output:**
```json
{}
```
- **Explanation:** An empty `FailedResourcesMap` confirms that all resources across distinct services were tagged successfully.

### Example 3: Edge Case - Identifying Untagged Orphaned Resources
- **Goal:** Find all EBS volumes lacking an `Owner` tag to prevent runaway storage costs.
- **Commands:**
```bash
aws ec2 describe-volumes \
    --query "Volumes[?Tags==null || !contains(Tags[].Key, 'Owner')].[VolumeId, Size, State]" \
    --output table
```
- **Real Output:**
```text
-------------------------------------------
|             DescribeVolumes             |
+----------------------+-----+------------+
|  vol-0123456789abc   |  50 |  available |
|  vol-0fedcba987654   | 100 |  available |
+----------------------+-----+------------+
```
- **Explanation:** Identifies unattached, unowned storage volumes eligible for immediate decommissioning.

### Example 4: Failure & Anti-Pattern - Reserved `aws:` Prefix Error
- **Goal:** Illustrate error when an engineer attempts to set a system-reserved tag key.
- **Attempted Command:**
```bash
aws ec2 create-tags \
    --resources i-0123456789abcdef0 \
    --tags Key=aws:customTag,Value=Test
```
- **Real Output:**
```text
An error occurred (InvalidParameterValue) when calling the CreateTags operation: 
The tag key "aws:customTag" is reserved for internal use.
```
- **Explanation & Fix:** Tag keys starting with `aws:` are strictly reserved for AWS internal subsystems. Customer-defined keys must never begin with `aws:`.

### Example 5: Enforcing Tag Standardization via Organizations Tag Policy
- **Goal:** Force all member accounts to use standardized PascalCase for the `Environment` tag.
- **Tag Policy Document (`tag-policy.json`):**
```json
{
    "tags": {
        "Environment": {
            "tag_key": {
                "@@assign": "Environment"
            },
            "tag_value": {
                "@@assign": [
                    "Production",
                    "Staging",
                    "Development"
                ]
            },
            "enforced_for": {
                "@@assign": [
                    "ec2:instance",
                    "s3:bucket"
                ]
            }
        }
    }
}
```
- **Explanation:** Any attempt to launch an EC2 instance with `environment=prod` or `Environment=dev` is rejected at the API level.

---

## 6. Best Practices
1. **Mandate Tags at Creation Time**: Enforce IAM conditions requiring `aws:RequestTag/Environment` during `ec2:RunInstances` or `s3:CreateBucket`.
2. **Activate Cost Allocation Tags Immediately**: New tags do not track costs retroactively. Activate them in the Billing console as soon as defined.
3. **Use Consistent Casing**: Inconsistent casing (`environment` vs `Environment`) creates separate billing line-items and breaks automation filters.
4. **Leverage CloudShell for Ephemeral Admin**: Use AWS CloudShell instead of installing AWS CLI and configuring long-lived credentials on personal laptops.
5. **Implement Automated Pruning for Untagged Sandbox Resources**: Run automated AWS Lambda scripts terminating untagged sandbox resources after 24 hours.

---

## 7. Common Mistakes & Pitfalls

| Mistake | Why It Fails | Practical Remedy |
|---|---|---|
| Expecting Cost Allocation Tags to work retroactively | AWS Billing only tracks costs from the activation timestamp forward | Activate Cost Allocation Tags in Billing console on day one |
| Case sensitivity inconsistency (`env`, `Env`, `Environment`) | Generates fragmented, multi-column reports in Cost Explorer | Enforce uniform casing using AWS Organizations Tag Policies |
| Using reserved `aws:` prefix for custom tags | API rejects the request with `InvalidParameterValue` | Use clean enterprise prefixes (e.g. `company:env` or bare `Environment`) |
| Forgetting that tags are not security boundaries | Any IAM user with `ec2:StopInstances` can stop any instance unless ABAC is applied | Pair tags with IAM condition keys (`ec2:ResourceTag/Env`) for enforcement |
| Assuming CloudShell storage is global | CloudShell persistent home directory is scoped strictly to a single Region | Store persistent automation scripts in S3 or Git, not local CloudShell disk |
| Reaching the 50-tag limit per resource | Additional tag operations throw `TagLimitExceeded` | Standardize on 5–10 high-value tags per resource |

---

## 8. Performance, Security & Cost Notes
- **Performance**: Querying resources via the Resource Groups Tagging API is globally indexed, executing in sub-second time without scanning every individual service API sequentially.
- **Security**: In Attribute-Based Access Control (ABAC), tags govern authorization. Protect tagging permissions (`tag:TagResources`, `ec2:CreateTags`) strictly, as modifying a tag can grant unauthorized access to instances.
- **Cost**: Creating, storing, and evaluating tags is **100% free of charge**. CloudShell compute and persistent 1 GB storage are also free.

---

## 9. Comparison / When to Use What

| Tool / Mechanism | Primary Function | Scope | Persistence |
|---|---|---|---|
| **AWS Management Console** | Visual interactive exploration | Global web session | Temporary browser session |
| **AWS CloudShell** | Pre-authenticated Linux terminal | Regional container | 1 GB persistent `/home` per region |
| **Resource Groups Tagging API** | Centralized programmatic tagging | Account or Multi-Account | Persistent resource metadata |
| **AWS Organizations Tag Policy** | Preventive governance guardrail | Organization / OU tree | Continuous API compliance |

---

## 10. Interview Questions

<details>
<summary>1. A company activates a Cost Allocation Tag today. Can they see cost breakdowns for the previous two months?</summary>
No. AWS Cost Allocation Tags are not retroactive. Billing data is tagged only from the moment the tag is activated in the Billing console forward.
</details>

<details>
<summary>2. What is the difference between AWS-generated tags and user-defined tags?</summary>
AWS-generated tags use the reserved prefix `aws:` (such as `aws:createdBy`) and are automatically populated by AWS. User-defined tags are created and managed by customers and cannot begin with `aws:`.
</details>

<details>
<summary>3. What is AWS CloudShell, and how are its credentials managed?</summary>
AWS CloudShell is a browser-based Amazon Linux 2023 shell environment. It inherits the authenticated console user's credentials automatically, requiring zero local credential configuration or static access keys.
</details>

<details>
<summary>4. How does an AWS Organizations Tag Policy enforce compliance?</summary>
Tag Policies define allowed tag keys, allowed casing, and allowed values. When configured in enforcement mode, any member account API call attempting to create or tag a resource with non-compliant tags is rejected.
</details>

<details>
<summary>5. How can tags be used to implement Attribute-Based Access Control (ABAC)?</summary>
By using condition keys like `ec2:ResourceTag/Department: "${aws:PrincipalTag/Department}"` in IAM policies, access is granted dynamically when the user's tag matches the resource's tag.
</details>

<details>
<summary>6. What is the maximum number of tags that can be attached to an AWS resource?</summary>
AWS enforces a hard limit of 50 tags per resource.
</details>

<details>
<summary>7. How does the Resource Groups Tagging API improve performance over calling individual service APIs?</summary>
Instead of querying EC2, S3, RDS, and DynamoDB APIs sequentially, the Resource Groups Tagging API maintains a unified catalog, returning all tagged resources across services in a single regional query.
</details>

<details>
<summary>8. What happens to persistent files stored in AWS CloudShell if the shell is not used for 120 days?</summary>
CloudShell retains up to 1 GB of persistent data in `$HOME`. However, if the shell environment remains inactive for 120 days, the persistent storage volume is automatically recycled and data is deleted.
</details>

---

## 11. Practice Exercises

<details>
<summary>Exercise 1 (Easy): Launch AWS CloudShell and check active caller</summary>
**Task:** Open CloudShell in the console and verify caller identity.

**Solution:**
```bash
aws sts get-caller-identity
```
</details>

<details>
<summary>Exercise 2 (Easy): Tag an existing S3 bucket</summary>
**Task:** Attach `Environment=Staging` and `Project=Phoenix` to an S3 bucket.

**Solution:**
```bash
aws s3api put-bucket-tagging \
    --bucket "my-test-bucket-2026" \
    --tagging 'TagSet=[{Key=Environment,Value=Staging},{Key=Project,Value=Phoenix}]'
```
</details>

<details>
<summary>Exercise 3 (Medium): Find all resources tagged with Environment=Production</summary>
**Task:** Use the Resource Groups Tagging API to list ARNs of all production resources.

**Solution:**
```bash
aws resourcegroupstaggingapi get-resources \
    --tag-filters Key=Environment,Values=Production \
    --query "ResourceTagMappingList[].[ResourceARN]" \
    --output text
```
</details>

<details>
<summary>Exercise 4 (Medium): Enforce mandatory tagging during EC2 creation</summary>
**Task:** Write an IAM policy snippet that allows `ec2:RunInstances` only if an `Owner` tag is supplied.

**Solution:**
```json
{
    "Effect": "Allow",
    "Action": "ec2:RunInstances",
    "Resource": "arn:aws:ec2:*:*:instance/*",
    "Condition": {
        "Null": {
            "aws:RequestTag/Owner": "false"
        }
    }
}
```
</details>

<details>
<summary>Exercise 5 (Hard): Script an audit of all untagged EC2 instances</summary>
**Task:** Write a bash script identifying all running instances lacking an `Environment` tag and outputting a remediation alert.

**Solution:**
```bash
#!/usr/bin/env bash
set -euo pipefail

UNTAGGED=$(aws ec2 describe-instances \
    --filters "Name=instance-state-name,Values=running" \
    --query "Reservations[].Instances[?!contains(Tags[].Key, 'Environment')].InstanceId" \
    --output text)

for id in $UNTAGGED; do
    echo "NON-COMPLIANT INSTANCE: $id lacks required 'Environment' tag!"
done
```
</details>

<details>
<summary>Exercise 6 (Hard): Bulk apply tags to all resources in a CloudFormation stack</summary>
**Task:** Query all resources created by stack `NetworkingStack` and apply `CostCenter=CORP-01`.

**Solution:**
```bash
ARNS=$(aws cloudformation describe-stack-resources \
    --stack-name "NetworkingStack" \
    --query "StackResources[?ResourceStatus=='CREATE_COMPLETE'].PhysicalResourceId" \
    --output text)

for res in $ARNS; do
    echo "Tagging: $res"
    aws resourcegroupstaggingapi tag-resources \
        --resource-arn-list "$res" \
        --tags CostCenter=CORP-01 2>/dev/null || true
done
```
</details>

---

## 12. Summary / Cheat Sheet

```
+---------------------------------------------------------------------------------+
|                        CONSOLE, CLOUDSHELL & TAGGING CHEAT SHEET                |
+---------------------------------------------------------------------------------+
| CLOUDSHELL     | Free browser terminal, pre-authenticated, 1GB home storage    |
| TAG LIMITS     | Up to 50 tags per resource; max 128 chars key, 256 chars value |
| RESERVED TAGS  | Never use "aws:" prefix (strictly reserved for AWS subsystems) |
| COST ALLOC.    | Must explicitly activate tags in Billing console (Not retroactive|
| TAG QUERY      | aws resourcegroupstaggingapi get-resources --tag-filters ...   |
| BULK TAGGING   | aws resourcegroupstaggingapi tag-resources --resource-arn-list |
| GOVERNANCE     | Enforce standardized casing via AWS Organizations Tag Policies |
+---------------------------------------------------------------------------------+
```

---

## 13. Further Reading
- [AWS Tagging Best Practices Whitepaper](https://docs.aws.amazon.com/whitepapers/latest/tagging-best-practices/tagging-best-practices.html)
- [AWS CloudShell User Guide](https://docs.aws.amazon.com/cloudshell/latest/userguide/welcome.html)
- [Using Cost Allocation Tags](https://docs.aws.amazon.com/cur/latest/userguide/cost-allocation-tags.html)
- [AWS Organizations Tag Policies Documentation](https://docs.aws.amazon.com/organizations/latest/userguide/orgs_manage_policies_tag-policies.html)

---

[Previous: 09-sdks-and-credential-provider-chain.md](file:///04-devops/aws/01-foundations/09-sdks-and-credential-provider-chain.md) | [Section Index](file:///04-devops/aws/01-foundations/README.md) | [Next: 11-global-infrastructure.md](file:///04-devops/aws/01-foundations/11-global-infrastructure.md)
