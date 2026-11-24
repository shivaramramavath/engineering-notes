# AWS Organizations, SCPs & Control Tower Multi-Account Governance

> AWS Organizations consolidates multiple AWS accounts into a centralized organizational tree governed by Service Control Policies (SCPs), while AWS Control Tower automates the provisioning of enterprise-grade landing zones with prescriptive security guardrails.

| Metadata | Details |
|---|---|
| **Level** | Intermediate to Advanced |
| **Prerequisites** | [03-accounts-regions-and-billing.md](file:///04-devops/aws/01-foundations/03-accounts-regions-and-billing.md) |
| **Time to Read** | 25 minutes |
| **Status** | Complete |
| **OS / Cloud Scope** | Enterprise Cloud Governance & Multi-Account Topologies |
| **Tool Version** | AWS Organizations v1, AWS Control Tower |
| **Tested With** | AWS Organizations CLI, Service Control Policies |

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
As engineering organizations scale, managing hundreds of developers, applications, and environments within a single AWS account leads to operational gridlock: API rate limits are shared, IAM policies become hundreds of lines of conflicting JSON, and an accidental permission leak in a development sandbox can expose production data. 

AWS Organizations solves this by structuring multiple AWS accounts into a hierarchical tree of Organizational Units (OUs). It provides programmatic account creation, consolidated invoicing, cross-account resource sharing via RAM (Resource Access Manager), and—most crucially—**Service Control Policies (SCPs)**. SCPs act as guardrails: they define the maximum available permissions across an account hierarchy. Even if an account administrator or root user attaches `AdministratorAccess`, an SCP explicit Deny overrides everything. Building on Organizations, **AWS Control Tower** automates the deployment of a standardized multi-account "Landing Zone" with built-in audit trails, security logging, and automated account vending.

---

## 2. Core Concepts
- **Organizations Hierarchy**:
  - *Management Account*: The root account of the organization; handles consolidated billing and defines top-level SCPs. Should contain zero application workloads.
  - *Root*: The parent container for all OUs and accounts.
  - *Organizational Units (OUs)*: Hierarchical folders containing member accounts or child OUs (e.g. `Core`, `Workloads`, `Sandbox`).
  - *Member Accounts*: Standard AWS accounts belonging to an OU.
- **Service Control Policies (SCPs)**:
  - JSON policy documents applied to the Root, an OU, or an individual Account.
  - **Invariants**:
    - SCPs **never grant permissions**; they specify the *maximum allowable permissions* (a filter boundary).
    - An action is allowed only if allowed by an SCP at every level of the hierarchy from the Root down to the Account.
    - An explicit `Deny` in any SCP in the hierarchy permanently denies the action for all IAM users and roles—including the member account Root user.
    - SCPs do not affect the Management Account itself.
- **AWS Control Tower**:
  - Managed service orchestrating Organizations, IAM Identity Center, AWS Config, and CloudTrail into a turnkey Landing Zone.
  - *Controls (formerly Guardrails)*:
    - *Preventive*: Enforced via SCPs (blocks non-compliant API calls before execution).
    - *Detective*: Enforced via AWS Config rules (flags non-compliant resources after deployment).
    - *Proactive*: Enforced via CloudFormation Hooks (evaluates templates before provisioning).
  - *Account Factory*: Standardized template for provisioning new baseline-compliant member accounts.

---

## 3. How It Works Internally
Hierarchical SCP Inheritance & Evaluation Pipeline:

```
+-------------------------------------------------------------------------+
|                              Root Container                             |
|  - FullAWSAccess (Allow *)                                              |
|  - SCP: DenyLeaveOrganization                                           |
+-------------------------------------------------------------------------+
                                     |
                                     v
+-------------------------------------------------------------------------+
|                  Workloads Organizational Unit (OU)                     |
|  - FullAWSAccess (Allow *)                                              |
|  - SCP: DenyUnapprovedRegions (Deny * Condition: StringNotEquals ...)   |
+-------------------------------------------------------------------------+
                  |                                           |
                  v                                           v
+-----------------------------------+       +-----------------------------+
|    Production OU / Account        |       |    Development Account      |
|  - SCP: DenyS3PublicAccessBypass  |       |  - FullAWSAccess (Allow *)  |
|  - IAM Admin Role has Allow *     |       |  - IAM Dev Role has Allow * |
|                                   |       |                             |
| Result: Can create resources ONLY |       | Result: Can create resources|
| in approved regions; cannot bypass|       | ONLY in approved regions;   |
| S3 Block Public Access.           |       | can test S3 configurations. |
+-----------------------------------+       +-----------------------------+
```

### The Effective Permission Intersection
An IAM principal executing an action in a member account is evaluated against:
$$\text{Effective Permission} = \text{SCP (Root)} \cap \text{SCP (OUs)} \cap \text{SCP (Account)} \cap \text{Permission Boundary} \cap \text{IAM Policy}$$
If any SCP layer denies the action, evaluation immediately terminates with an implicit or explicit `AccessDenied`.

---

## 4. Syntax & Usage
CLI operations for managing organizations, OUs, and attaching SCPs:

```bash
# Describe organization details
aws organizations describe-organization

# List root container and top-level Organizational Units (OUs)
aws organizations list-roots
aws organizations list-organizational-units-for-parent --parent-id r-1234

# Create a new Organizational Unit
aws organizations create-organizational-unit \
    --parent-id r-1234 \
    --name "Security-Core"

# Create a Service Control Policy (SCP)
aws organizations create-policy \
    --name "Enforce-IMDSv2" \
    --description "Blocks EC2 instances running without IMDSv2 tokens required" \
    --type "SERVICE_CONTROL_POLICY" \
    --content file://enforce-imdsv2.json

# Attach SCP to an OU
aws organizations attach-policy \
    --policy-id p-0123456789abcdef \
    --target-id ou-1234-567890ab
```

---

## 5. Practical Examples

### Example 1: SCP Restricting Cloud Operations to Approved Regions
- **Goal:** Prevent developers or compromised keys from provisioning resources in unapproved global regions.
- **SCP Document (`deny-unapproved-regions.json`):**
```json
{
    "Version": "2012-10-17",
    "Statement": [
        {
            "Sid": "DenyUnapprovedRegions",
            "Effect": "Deny",
            "NotAction": [
                "cloudfront:*",
                "iam:*",
                "route53:*",
                "support:*",
                "organizations:*",
                "wafv2:*"
            ],
            "Resource": "*",
            "Condition": {
                "StringNotEquals": {
                    "aws:RequestedRegion": [
                        "us-east-1",
                        "us-west-2"
                    ]
                }
            }
        }
    ]
}
```
- **Explanation:** Protects against cryptocurrency mining campaigns in obscure regions while exempting global services (IAM, Route 53, CloudFront).

### Example 2: SCP Preventing Deletion of CloudTrail Audit Logs
- **Goal:** Ensure member account administrators cannot disable or delete organization-wide security audit trails.
- **SCP Document (`protect-cloudtrail.json`):**
```json
{
    "Version": "2012-10-17",
    "Statement": [
        {
            "Sid": "PreventCloudTrailTampering",
            "Effect": "Deny",
            "Action": [
                "cloudtrail:DeleteTrail",
                "cloudtrail:StopLogging",
                "cloudtrail:UpdateTrail"
            ],
            "Resource": "*"
        }
    ]
}
```
- **Explanation:** Guarantees immutable forensic integrity across all child accounts.

### Example 3: Edge Case - Detaching Default `FullAWSAccess` Policy
- **Goal:** Convert an OU from a "Blacklist / Deny-list" model to a strict "Whitelist / Allow-list" model.
- **Commands:**
```bash
# 1. Attach custom whitelist policy allowing only EC2 and S3
aws organizations attach-policy --policy-id p-whitelist01 --target-id ou-1234-abcd

# 2. Detach default FullAWSAccess policy
aws organizations detach-policy --policy-id p-FullAWSAccess --target-id ou-1234-abcd
```
- **Explanation:** By default, every node has `FullAWSAccess` attached. If detached without attaching another allow policy, all actions in child accounts are immediately blocked.

### Example 4: Failure & Anti-Pattern - Accidentally Denying Root in the Management Account
- **Goal:** Illustrate what happens when an administrator tries to restrict the Management Account using an SCP.
- **Attempted Action:** Attach an SCP to the Management Account ID.
- **Real Output:**
```text
An error occurred (ConstraintViolationException) when calling the AttachPolicy operation:
You cannot attach a service control policy to the management account of an organization.
```
- **Explanation & Fix:** AWS strictly prevents attaching SCPs to the Management Account. For this reason, **never deploy workloads into the Management Account**, as they cannot be governed by SCP guardrails.

### Example 5: Production Landing Zone Architecture via Control Tower
- **Goal:** Inspect active Control Tower controls across an enterprise account.
- **Commands:**
```bash
aws controltower list-enabled-controls \
    --target-identifier "arn:aws:organizations::123456789012:ou/o-root123/ou-core-456" \
    --query "enabledControls[].[controlIdentifier, status.summary]" \
    --output table
```
- **Real Output:**
```text
-------------------------------------------------------------------------------------------------
|                                      ListEnabledControls                                      |
+------------------------------------------------------------------------------------+----------+
|  arn:aws:controltower:us-east-1::control/AWS-GR_DISALLOW_CROSS_REGION_NETWORKING   |  SUCCEEDED|
|  arn:aws:controltower:us-east-1::control/AWS-GR_ENCRYPTED_VOLUMES                   |  SUCCEEDED|
|  arn:aws:controltower:us-east-1::control/AWS-GR_RESTRICT_ROOT_USER                  |  SUCCEEDED|
+------------------------------------------------------------------------------------+----------+
```
- **Explanation:** Verifies that declarative preventive and detective guardrails are actively protecting core infrastructure.

---

## 6. Best Practices
1. **Keep the Management Account Clean**: Host zero workloads, databases, or public endpoints in the Management Account; use it strictly for billing, Organizations, and Control Tower management.
2. **Organize OUs by Function, Not Org Chart**: Create OUs like `Core` (Shared Services, Network, Security), `Workloads` (Prod, Non-Prod), and `Sandbox` rather than mirroring HR reporting structures.
3. **Use Deny-List SCPs for Safety**: Retain `FullAWSAccess` and append targeted `Deny` statements with exemptions. Pure whitelist models break frequently when AWS releases new services.
4. **Always Exempt Global Services in Region Locks**: Forgetting `iam:*`, `cloudfront:*`, or `support:*` in region-locking SCPs breaks core account administration.
5. **Automate Account Vending via Account Factory**: Never create accounts manually; use Control Tower Account Factory or Terraform to provision accounts pre-wired with VPCs, IAM SSO, and security baselines.

---

## 7. Common Mistakes & Pitfalls

| Mistake | Why It Fails | Practical Remedy |
|---|---|---|
| Believing SCPs grant permissions | Attaching an SCP allowing `s3:*` does not permit an IAM user to access S3 | An IAM policy must still grant the permission; SCP only sets the ceiling |
| Hosting workloads in the Management Account | SCPs cannot be applied to the Management Account; security boundaries are lost | Dedicate the Management Account strictly to billing and governance |
| Region lock SCP blocking global services | Region condition evaluates to empty/null for IAM, locking admins out | Always list global services (`iam:*`, `route53:*`) in the `NotAction` block |
| Detaching `FullAWSAccess` prematurely | An OU with no Allow policies experiences immediate total denial of all services | Maintain `FullAWSAccess` and implement guardrails using explicit `Deny` statements |
| Overlooking SCP inheritance | An SCP attached at Root cascades down to every single member account | Test SCPs in an isolated `Test-OU` before attaching to Root or Production OUs |
| Manually configuring member accounts | Drift occurs across accounts; security tools are missed | Use AWS Control Tower or custom CloudFormation StackSets to enforce identical baselines |

---

## 8. Performance, Security & Cost Notes
- **Performance**: SCP evaluation occurs in-memory at the AWS IAM control-plane edge; it introduces zero measurable latency to API calls or data-plane throughput.
- **Security**: SCPs apply to all identities within member accounts, including the member account Root user. This neutralizes rogue administrators or compromised credentials.
- **Cost**: AWS Organizations and Service Control Policies are provided at **zero additional charge**. Control Tower is free, though underlying resources provisioned (Config rules, S3 logging buckets, CloudTrail trails) incur standard service usage fees.

---

## 9. Comparison / When to Use What

| Governance Layer | Primary Mechanism | Scope | Can Restrict Root? |
|---|---|---|---|
| **Service Control Policy (SCP)** | Declarative JSON Guardrail | Root, OU, or Account | Yes (Member accounts only) |
| **IAM Permission Boundary** | JSON Policy attached to IAM Principal | Specific IAM Users or Roles | No |
| **IAM Policy (Identity-based)**| JSON Policy granting access | Specific IAM Users, Roles, Groups | No |
| **AWS Config Rule** | Detective Python/JSON Rule | Resources within a Region | Evaluates state post-creation |

---

## 10. Interview Questions

<details>
<summary>1. How does AWS evaluate permissions when an SCP and an IAM policy conflict?</summary>
An explicit Deny in an SCP permanently overrides any Allow in an IAM policy. Conversely, an Allow in an SCP does not grant access; it merely establishes that the action is permitted if an identity-based or resource-based IAM policy explicitly allows it.
</details>

<details>
<summary>2. Can a member account's root user bypass a Service Control Policy?</summary>
No. Unlike standard IAM policies, Service Control Policies apply to all principals in a member account, including the member account root user. Only the Management Account root user operates outside SCP restrictions.
</details>

<details>
<summary>3. Why is it dangerous to run production workloads in the AWS Organizations Management Account?</summary>
Because SCPs cannot be attached to the Management Account. Any workload running in the Management Account lacks organizational guardrails (such as region locks, S3 public access blocks, or CloudTrail protection) and shares the ultimate corporate billing identity.
</details>

<details>
<summary>4. What is the difference between a Preventive Control and a Detective Control in AWS Control Tower?</summary>
A Preventive Control uses an SCP to reject non-compliant API calls before they execute (e.g. blocking the creation of unencrypted EBS volumes). A Detective Control uses AWS Config to monitor created resources and flag violations after they occur (e.g. alerting when an unattached Elastic IP exists).
</details>

<details>
<summary>5. Explain the inheritance model of SCPs in an Organizations tree.</summary>
SCPs are inherited downwards from the Root container, through parent OUs, child OUs, down to the member Account. An action must be allowed at every step in the path and denied nowhere.
</details>

<details>
<summary>6. Why must global services be exempted from region-locking SCPs?</summary>
Services like IAM, Route 53, CloudFront, and AWS Support have global control planes (`us-east-1`). If a region-locking SCP uses a `StringNotEquals` condition on `aws:RequestedRegion` without exempting global services, API calls to IAM and Route 53 will fail.
</details>

<details>
<summary>7. What is an AWS Control Tower Landing Zone?</summary>
A Landing Zone is an automated, standardized multi-account AWS environment configured according to security and compliance best practices, featuring identity management (IAM Identity Center), centralized logging, auditing, and pre-configured network topology.
</details>

<details>
<summary>8. How do Tag Policies in AWS Organizations differ from Service Control Policies?</summary>
Tag Policies do not control access to service APIs; they enforce standardization of resource tag keys and allowed values (e.g., ensuring `Environment` can only be set to `Production` or `Development`).
</details>

---

## 11. Practice Exercises

<details>
<summary>Exercise 1 (Easy): List all accounts in your organization</summary>
**Task:** Use the AWS CLI to list member account names, IDs, and statuses.

**Solution:**
```bash
aws organizations list-accounts \
    --query "Accounts[].[Id, Name, Status, JoinedMethod]" \
    --output table
```
</details>

<details>
<summary>Exercise 2 (Easy): Describe the Organization Root</summary>
**Task:** Identify the Root ID of your organization hierarchy.

**Solution:**
```bash
aws organizations list-roots --query "Roots[0].Id" --output text
```
</details>

<details>
<summary>Exercise 3 (Medium): List all SCPs attached to a specific OU</summary>
**Task:** Write a command to display all policies attached to a target OU.

**Solution:**
```bash
aws organizations list-policies-for-target \
    --target-id <OU_ID> \
    --filter "SERVICE_CONTROL_POLICY" \
    --query "Policies[].[Id, Name, AwsManaged]" \
    --output table
```
</details>

<details>
<summary>Exercise 4 (Medium): Build an SCP blocking non-Graviton instance types</summary>
**Task:** Write an SCP denying EC2 instance launches unless the instance type starts with `c8g.`, `m8g.`, or `t4g.`.

**Solution:**
```json
{
    "Version": "2012-10-17",
    "Statement": [
        {
            "Sid": "EnforceGravitonInstancesOnly",
            "Effect": "Deny",
            "Action": "ec2:RunInstances",
            "Resource": "arn:aws:ec2:*:*:instance/*",
            "Condition": {
                "StringNotLike": {
                    "ec2:InstanceType": ["c8g.*", "m8g.*", "t4g.*"]
                }
            }
        }
    ]
}
```
</details>

<details>
<summary>Exercise 5 (Hard): Script the creation of a Sandbox OU with an SCP attached</summary>
**Task:** Write a bash script that creates an OU named `Sandbox`, defines an SCP denying VPC peering, and attaches the policy to the new OU.

**Solution:**
```bash
#!/usr/bin/env bash
set -euo pipefail

ROOT_ID=$(aws organizations list-roots --query "Roots[0].Id" --output text)

OU_ID=$(aws organizations create-organizational-unit \
    --parent-id "$ROOT_ID" \
    --name "Sandbox-Experimental" \
    --query "OrganizationalUnit.Id" --output text)

POLICY_ID=$(aws organizations create-policy \
    --name "Deny-VPC-Peering-Sandbox" \
    --description "Blocks VPC peering in sandbox accounts" \
    --type "SERVICE_CONTROL_POLICY" \
    --content '{
        "Version": "2012-10-17",
        "Statement": [{
            "Effect": "Deny",
            "Action": ["ec2:CreateVpcPeeringConnection", "ec2:AcceptVpcPeeringConnection"],
            "Resource": "*"
        }]
    }' \
    --query "Policy.PolicySummary.Id" --output text)

aws organizations attach-policy --policy-id "$POLICY_ID" --target-id "$OU_ID"
echo "Created OU $OU_ID and attached SCP $POLICY_ID successfully."
```
</details>

<details>
<summary>Exercise 6 (Hard): Audit effective SCP permissions for an account</summary>
**Task:** Use the AWS Organizations API to list the full inheritance chain of policies applying to a member account.

**Solution:**
```bash
ACCOUNT_ID="123456789012"
PARENTS=$(aws organizations list-parents --child-id "$ACCOUNT_ID" --query "Parents[].[Id, Type]" --output table)
echo "Parent hierarchy:"
echo "$PARENTS"
aws organizations list-policies-for-target --target-id "$ACCOUNT_ID" --filter "SERVICE_CONTROL_POLICY" --output table
```
</details>

---

## 12. Summary / Cheat Sheet

```
+---------------------------------------------------------------------------------+
|                  ORGANIZATIONS & CONTROL TOWER CHEAT SHEET                      |
+---------------------------------------------------------------------------------+
| ORGANIZATIONS  | Tree: Root -> Organizational Units (OUs) -> Member Accounts    |
| MANAGEMENT ACC | Billing & policy authority only; NEVER host workloads here     |
| SCP FUNCTION   | Sets maximum permission boundary; NEVER grants permissions     |
| SCP DENY       | Explicit Deny overrides all Allows across all OUs and Root     |
| ROOT AFFECTED  | SCPs constrain member account root users; Management exempt    |
| CONTROL TOWER  | Automated multi-account Landing Zone with preventive guardrails|
| PREVENTIVE     | Enforced via SCPs before API execution (Deny)                  |
| DETECTIVE      | Enforced via AWS Config rules after creation (Audit flag)      |
| CLI COMMAND    | aws organizations list-policies-for-target --target-id <OU>    |
+---------------------------------------------------------------------------------+
```

---

## 13. Further Reading
- [AWS Organizations User Guide](https://docs.aws.amazon.com/organizations/latest/userguide/orgs_introduction.html)
- [Service Control Policies (SCPs) Reference](https://docs.aws.amazon.com/organizations/latest/userguide/orgs_manage_policies_scps.html)
- [AWS Control Tower User Guide](https://docs.aws.amazon.com/controltower/latest/userguide/what-is-control-tower.html)
- [Organizing Your AWS Environment Using Multiple Accounts](https://docs.aws.amazon.com/whitepapers/latest/organizing-your-aws-environment/organizing-your-aws-environment.html)

---

[Previous: 03-accounts-regions-and-billing.md](file:///04-devops/aws/01-foundations/03-accounts-regions-and-billing.md) | [Section Index](file:///04-devops/aws/01-foundations/README.md) | [Next: 05-iam-fundamentals.md](file:///04-devops/aws/01-foundations/05-iam-fundamentals.md)
