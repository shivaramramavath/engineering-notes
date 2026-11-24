# AWS IAM Fundamentals: Principals, Roles, STS & Policies

> AWS Identity and Access Management (IAM) is the zero-trust identity control plane governing authentication and authorization for human users, container workloads, and automated cloud services via cryptographic temporary credentials.

| Metadata | Details |
|---|---|
| **Level** | Beginner to Intermediate |
| **Prerequisites** | [01-what-is-aws.md](file:///04-devops/aws/01-foundations/01-what-is-aws.md) |
| **Time to Read** | 25 minutes |
| **Status** | Complete |
| **OS / Cloud Scope** | Global IAM Control Plane (`us-east-1`) |
| **Tool Version** | AWS CLI v2.22+, AWS STS |
| **Tested With** | AWS IAM v1, AWS STS API, Python Boto3 |

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
In traditional enterprise networking, security boundaries are anchored to physical firewall perimeters, VPN concentrators, and corporate LAN subnets. In hyperscale cloud environments, IP perimeters dissolve: services communicate over public HTTPS control planes, developers work remotely, and ephemeral microservices scale up and down in seconds. Identity is the new operational perimeter.

AWS IAM provides centralized, granular control over *who* (authentication) can perform *what actions* on *which resources* under *what conditions* (authorization). Historically, systems relied on static, long-lived access keys (`AKIA...`), which represented massive security liabilities when committed to Git or leaked. Modern AWS IAM enforces dynamic, short-lived cryptographic credentials minted by the **AWS Security Token Service (STS)**, binding execution roles directly to virtual machines, container tasks, and serverless functions without hardcoded secrets.

---

## 2. Core Concepts
- **Principals**: Human users or automated workloads that can initiate API actions against AWS resources.
  - *IAM User*: A long-lived identity with permanent credentials (passwords, access keys). Strongly discouraged for humans in modern enterprise setups.
  - *IAM Group*: A collection of IAM users used solely for attaching batch permissions. A group cannot be designated as a `Principal` in resource policies.
  - *IAM Role*: An identity with zero permanent credentials. Assumed dynamically by users, applications, or AWS services to obtain temporary security credentials.
- **Role Architecture**: Every IAM role requires two distinct policies:
  1. *Trust Policy (AssumeRolePolicyDocument)*: Defines **who** is allowed to assume the role (e.g. EC2 service, Lambda service, another AWS account, or an OIDC provider).
  2. *Permissions Policy*: Defines **what** the assumed role is allowed to do once assumed.
- **AWS Security Token Service (STS)**:
  - Global service that vends temporary credentials comprising an `AccessKeyId` (`ASIA...`), a `SecretAccessKey`, and a `SessionToken`.
  - Credentials expire automatically after a configurable duration (15 minutes to 12 hours).
- **Policy Types**:
  - *Identity-Based Policies*: Attached to users, groups, or roles (e.g. AWS Managed, Customer Managed, Inline).
  - *Resource-Based Policies*: Attached directly to resources (e.g. S3 bucket policies, KMS key policies, SQS queue policies).

---

## 3. How It Works Internally
The STS Role Assumption & Credential Exchange Flow:

```
+------------------+         1. sts:AssumeRole (Signed SigV4)       +-------------------+
| Caller Principal | ---------------------------------------------> |  AWS STS Service  |
| (EC2 / GitHub CI)|                                                +-------------------+
+------------------+                                                          |
        ^                                                                     | 2. Evaluates Role
        |                                                                     |    Trust Policy
        |                    3. Vends Temporary Credentials                   v
        |                       (ASIA..., SecretKey, Token)         +-------------------+
        +---------------------------------------------------------- |  Target IAM Role  |
        |                                                           +-------------------+
        v
4. Issues API Call to Target Service (e.g. DynamoDB)
   Signed with ASIA... + Token
        |
        v
+------------------+         5. Evaluates Role Permissions          +-------------------+
|  DynamoDB Table  | <--------------------------------------------- | IAM Engine (SigV4)|
+------------------+                                                +-------------------+
```

### Instance Profiles (EC2 Role Binding)
Virtual machines on EC2 cannot store secrets securely on disk. Instead, an **Instance Profile** acts as a container for an IAM Role. The EC2 host hypervisor queries STS automatically, vending rotating credentials directly to the local Instance Metadata Service (IMDSv2) at `http://169.254.169.254/latest/meta-data/iam/security-credentials/<role-name>`. AWS SDKs automatically query this internal URI without developer intervention.

---

## 4. Syntax & Usage
CLI syntax for IAM inspection, role assumption, and policy creation:

```bash
# Query active caller identity and ARN
aws sts get-caller-identity

# Assume an IAM role programmatically
aws sts assume-role \
    --role-arn "arn:aws:iam::123456789012:role/DeployerRole" \
    --role-session-name "CLI-Session-1"

# Create an IAM Role with a Trust Policy
aws iam create-role \
    --role-name "Lambda-DynamoDB-ExecutionRole" \
    --assume-role-policy-document file://trust-policy.json

# Attach an AWS managed policy to the role
aws iam attach-role-policy \
    --role-name "Lambda-DynamoDB-ExecutionRole" \
    --policy-arn "arn:aws:iam::aws:policy/service-role/AWSLambdaBasicExecutionRole"
```

---

## 5. Practical Examples

### Example 1: Creating a Least-Privilege Trust Policy for AWS Lambda
- **Goal:** Author a trust policy allowing the AWS Lambda service to assume an execution role.
- **Trust Policy Document (`lambda-trust-policy.json`):**
```json
{
    "Version": "2012-10-17",
    "Statement": [
        {
            "Sid": "AllowLambdaServiceAssumeRole",
            "Effect": "Allow",
            "Principal": {
                "Service": "lambda.amazonaws.com"
            },
            "Action": "sts:AssumeRole"
        }
    ]
}
```
- **CLI Execution:**
```bash
aws iam create-role \
    --role-name "OrderProcessingLambdaRole" \
    --assume-role-policy-document file://lambda-trust-policy.json \
    --query "Role.[RoleName, Arn]" --output table
```
- **Real Output:**
```text
-----------------------------------------------------------------------------------------------
|                                         CreateRole                                          |
+---------------------------+-----------------------------------------------------------------+
|  OrderProcessingLambdaRole|  arn:aws:iam::123456789012:role/OrderProcessingLambdaRole       |
+---------------------------+-----------------------------------------------------------------+
```
- **Explanation:** Establishes the trust anchor; without this, Lambda cannot assume the role.

### Example 2: Scoped DynamoDB Customer-Managed Permissions Policy
- **Goal:** Grant read-only access to a specific DynamoDB table and its indexes.
- **Permissions Policy (`dynamodb-read-policy.json`):**
```json
{
    "Version": "2012-10-17",
    "Statement": [
        {
            "Sid": "AllowDynamoDBItemRead",
            "Effect": "Allow",
            "Action": [
                "dynamodb:GetItem",
                "dynamodb:BatchGetItem",
                "dynamodb:Query"
            ],
            "Resource": [
                "arn:aws:dynamodb:us-east-1:123456789012:table/Orders",
                "arn:aws:dynamodb:us-east-1:123456789012:table/Orders/index/*"
            ]
        }
    ]
}
```
- **Explanation:** Scoped strictly to reading from the `Orders` table; prevents `PutItem`, `DeleteItem`, or table deletion.

### Example 3: Edge Case - Cross-Account Role Assumption
- **Goal:** Allow an administrative role in Account A (`111122223333`) to assume a deployment role in Account B (`444455556666`).
- **Trust Policy in Account B:**
```json
{
    "Version": "2012-10-17",
    "Statement": [
        {
            "Sid": "AllowAccountAToAssume",
            "Effect": "Allow",
            "Principal": {
                "AWS": "arn:aws:iam::111122223333:root"
            },
            "Action": "sts:AssumeRole",
            "Condition": {
                "Bool": {
                    "aws:MultiFactorAuthPresent": "true"
                }
            }
        }
    ]
}
```
- **Explanation:** Requires cross-account authorization from both accounts and enforces MFA presence before vending credentials.

### Example 4: Failure & Anti-Pattern - Trust Policy Missing Principal Action
- **Goal:** Demonstrate error when a trust policy omits `sts:AssumeRole`.
- **Anti-Pattern Scenario:** Attempting to assume a role where trust policy action was set to `iam:PassRole`.
- **Real Output:**
```text
An error occurred (AccessDenied) when calling the AssumeRole operation: 
User: arn:aws:iam::111122223333:user/developer is not authorized to perform: 
sts:AssumeRole on resource: arn:aws:iam::444455556666:role/DeployerRole
```
- **Explanation & Fix:** Trust policies MUST explicitly declare `"Action": "sts:AssumeRole"`. Any other action fails during STS handshake.

### Example 5: Production Instance Profile Creation for EC2
- **Goal:** Create an Instance Profile, attach an IAM Role, and associate it with an EC2 instance.
- **Commands:**
```bash
# 1. Create Instance Profile
aws iam create-instance-profile --instance-profile-name "WebServerInstanceProfile"

# 2. Add Role to Instance Profile
aws iam add-role-to-instance-profile \
    --instance-profile-name "WebServerInstanceProfile" \
    --role-name "WebServerRole"

# 3. Associate with running EC2 instance
aws ec2 associate-iam-instance-profile \
    --instance-id i-0123456789abcdef0 \
    --iam-instance-profile Name="WebServerInstanceProfile"
```
- **Real Output:**
```json
{
    "IamInstanceProfileAssociation": {
        "AssociationId": "iip-assoc-0123456789abcdef0",
        "InstanceId": "i-0123456789abcdef0",
        "IamInstanceProfile": {
            "Arn": "arn:aws:iam::123456789012:instance-profile/WebServerInstanceProfile",
            "Id": "AIPAJ5EXAMPLEPROFILE"
        },
        "State": "associating"
    }
}
```
- **Explanation:** Enables EC2 software workloads to query IMDSv2 for short-lived credentials without baking keys into disk images.

---

## 6. Best Practices
1. **Eliminate Static IAM User Access Keys**: Use IAM Identity Center (SSO) for human users and IAM Roles for workloads.
2. **Never Grant Wildcards on Actions or Resources in Production**: Avoid `"Action": "*"` and `"Resource": "*"`. Specify exact ARNs.
3. **Use Customer Managed Policies Over Inline Policies**: Customer managed policies support versioning, automated rollbacks, and multi-principal reuse.
4. **Enforce `MFA` on High-Privilege Roles**: Require `aws:MultiFactorAuthPresent: "true"` condition keys on production write roles.
5. **Audit Inactive Credentials Continuously**: Run IAM Credential Reports (`aws iam generate-credential-report`) to identify and remove unused credentials older than 90 days.

---

## 7. Common Mistakes & Pitfalls

| Mistake | Why It Fails | Practical Remedy |
|---|---|---|
| Attaching `AdministratorAccess` to application roles | If the app is breached, attacker gains full control of the entire AWS account | Scope permissions down strictly to the specific database/queue needed |
| Confusing Trust Policy with Permissions Policy | Appending DynamoDB permissions to a trust policy results in `MalformedPolicyDocument` | Keep trust logic (`AssumeRole`) in trust policy; service permissions in permissions policy |
| Trying to use an IAM Group as a resource policy principal | S3 bucket policies and KMS key policies do not support IAM Groups as principals | Specify IAM Roles or IAM Users directly in resource policy `Principal` blocks |
| Committing `credentials` or `.env` files to Git | Public GitHub scanners find AWS access keys in under 60 seconds | Use IAM Roles, git secret scanners (git-secrets, Gitleaks), and `.gitignore` |
| Setting session duration too long on STS tokens | Compromised token remains valid for hours after an incident | Limit STS session durations to 15–60 minutes for high-privilege operations |
| Forgetting KMS key permissions when accessing S3 | IAM policy allows `s3:GetObject`, but object is encrypted with customer KMS key; access denied | Add `kms:Decrypt` permission for the target KMS key ARN |

---

## 8. Performance, Security & Cost Notes
- **Performance**: IAM policy evaluations are cached across AWS regional edge endpoints; typical evaluation latency is sub-millisecond. STS token requests take ~50–100ms; always cache temporary STS credentials until expiration rather than calling `sts:AssumeRole` on every request.
- **Security**: Access keys starting with `AKIA...` represent permanent, long-lived credentials. Access keys starting with `ASIA...` represent short-lived STS credentials accompanied by a security token.
- **Cost**: AWS IAM and AWS STS (for global default endpoints) are provided at **zero additional charge**.

---

## 9. Comparison / When to Use What

| Identity Primitive | Long-Lived? | Rotation Method | Recommended Use Case |
|---|---|---|---|
| **IAM User** | Yes (Permanent) | Manual (90-day cycle) | Legacy systems unable to use STS; strongly discouraged |
| **IAM Role** | No (Temporary STS) | Automatic by AWS SDK / IMDS | EC2, Lambda, ECS, GitHub Actions OIDC, human SSO |
| **Instance Profile** | Container for Role | N/A | Binding an IAM Role specifically to an EC2 instance |
| **IAM Group** | N/A (Bundle of users) | N/A | Batch policy attachment to multiple IAM users |

---

## 10. Interview Questions

<details>
<summary>1. What are the two policies that make up an IAM Role, and what does each do?</summary>
An IAM Role requires a **Trust Policy** (defining *who* can assume the role, e.g. a service or external account via `sts:AssumeRole`) and one or more **Permissions Policies** (defining *what* actions the assumed role can perform against AWS resources).
</details>

<details>
<summary>2. How do credentials on an EC2 instance rotate automatically?</summary>
An EC2 instance is associated with an Instance Profile containing an IAM Role. The AWS Nitro hypervisor periodically calls STS to vend short-lived credentials (`ASIA...`) into the Instance Metadata Service (IMDS). The AWS SDK reads from IMDS and refreshes the in-memory credentials before they expire.
</details>

<details>
<summary>3. What is the difference between an Access Key starting with AKIA vs ASIA?</summary>
`AKIA` designates permanent, long-lived credentials generated for an IAM User. `ASIA` designates temporary, short-lived credentials vended by the AWS Security Token Service (STS) associated with an IAM Role or temporary session.
</details>

<details>
<summary>4. Why cannot an IAM Group be specified as a Principal in an S3 Bucket Policy?</summary>
IAM Groups are organizational administrative constructs within an account, not real identity principals recognized by the AWS authorization engine. Only IAM Users, IAM Roles, AWS Services, or Account Roots can be specified as `Principal`.
</details>

<details>
<summary>5. How does Cross-Account access work between Account A and Account B?</summary>
Account B creates an IAM Role with a Trust Policy permitting Account A's root/principal to assume it. Account A attaches an identity policy to its user/role granting permission to call `sts:AssumeRole` on Account B's role ARN. Both accounts must explicitly authorize the transaction.
</details>

<details>
<summary>6. What is the maximum duration for temporary credentials requested via sts:AssumeRole?</summary>
By default, 1 hour. It can be configured on the IAM role from 15 minutes up to a maximum of 12 hours. Chained role assumptions (Role A assumes Role B) are hard-limited to 1 hour.
</details>

<details>
<summary>7. What is an IAM Permission Boundary?</summary>
A permission boundary is an advanced feature that sets the maximum permissions an identity-based policy can grant to an IAM principal. It allows developers to create custom roles for applications while guaranteeing they cannot elevate their own privileges beyond the boundary ceiling.
</details>

<details>
<summary>8. If an IAM role has permission to access an S3 object, but the KMS key encrypting it does not permit the role, what happens?</summary>
The request is denied (`AccessDenied`). For encrypted resources, the principal must have permissions granted in both the service policy (S3) AND the resource policy of the encryption key (KMS).
</details>

---

## 11. Practice Exercises

<details>
<summary>Exercise 1 (Easy): Query active caller ARN and account ID</summary>
**Task:** Use the AWS CLI to extract caller user ID, ARN, and account.

**Solution:**
```bash
aws sts get-caller-identity --output table
```
</details>

<details>
<summary>Exercise 2 (Easy): List all IAM roles in the account</summary>
**Task:** Query all role names and creation dates in table format.

**Solution:**
```bash
aws iam list-roles --query "Roles[].[RoleName, CreateDate]" --output table
```
</details>

<details>
<summary>Exercise 3 (Medium): Inspect an IAM role's trust policy document</summary>
**Task:** Retrieve the trust policy JSON of a role named `EC2-S3-ReadRole`.

**Solution:**
```bash
aws iam get-role \
    --role-name "EC2-S3-ReadRole" \
    --query "Role.AssumeRolePolicyDocument" \
    --output json
```
</details>

<details>
<summary>Exercise 4 (Medium): Test policy evaluation using IAM Policy Simulator</summary>
**Task:** Simulate whether an IAM role can perform `s3:PutObject` on a bucket.

**Solution:**
```bash
aws iam simulate-principal-policy \
    --policy-source-arn "arn:aws:iam::123456789012:role/AppRole" \
    --action-names "s3:PutObject" \
    --resource-arns "arn:aws:s3:::my-company-bucket/test.txt" \
    --query "EvaluationResults[].[EvalActionName, EvalDecision]" \
    --output table
```
</details>

<details>
<summary>Exercise 5 (Hard): Assume a role and export temporary credentials to environment variables</summary>
**Task:** Write a bash script that assumes a role and exports `AWS_ACCESS_KEY_ID`, `AWS_SECRET_ACCESS_KEY`, and `AWS_SESSION_TOKEN`.

**Solution:**
```bash
#!/usr/bin/env bash
ROLE_ARN="arn:aws:iam::123456789012:role/DeployerRole"
SESSION_JSON=$(aws sts assume-role --role-arn "$ROLE_ARN" --role-session-name "test-session" --output json)

export AWS_ACCESS_KEY_ID=$(echo "$SESSION_JSON" | jq -r .Credentials.AccessKeyId)
export AWS_SECRET_ACCESS_KEY=$(echo "$SESSION_JSON" | jq -r .Credentials.SecretAccessKey)
export AWS_SESSION_TOKEN=$(echo "$SESSION_JSON" | jq -r .Credentials.SessionToken)

echo "Temporary credentials assumed successfully. Current caller:"
aws sts get-caller-identity
```
</details>

<details>
<summary>Exercise 6 (Hard): Build an automated IAM credential report auditor</summary>
**Task:** Generate an IAM credential report and find all IAM users with active access keys that have not been rotated in 90 days.

**Solution:**
```bash
aws iam generate-credential-report > /dev/null
REPORT=$(aws iam get-credential-report --query Content --output text | base64 -d)

echo "$REPORT" | awk -F, 'NR>1 {
    if ($9 == "true" && $11 != "N/A") {
        print "User:", $1, "AccessKey1 Active, Last Rotated:", $11
    }
}'
```
</details>

---

## 12. Summary / Cheat Sheet

```
+---------------------------------------------------------------------------------+
|                                 IAM CHEAT SHEET                                 |
+---------------------------------------------------------------------------------+
| PRINCIPALS     | Users (discouraged), Roles (recommended), AWS Services         |
| IAM ROLE       | Trust Policy (Who can assume) + Permissions Policy (What they do|
| STS TOKENS     | ASIA... short-lived keys + session token (15m to 12h)           |
| EC2 BINDING    | EC2 -> Instance Profile -> IAM Role -> IMDSv2                  |
| CROSS-ACCOUNT  | Both accounts must allow (Account A caller + Account B role)   |
| ACCESS KEY     | AKIA = permanent (dangerous) | ASIA = temporary (STS)          |
| CLI WHOAMI     | aws sts get-caller-identity                                    |
| CLI ASSUME     | aws sts assume-role --role-arn <ARN> --role-session-name <NAME> |
+---------------------------------------------------------------------------------+
```

---

## 13. Further Reading
- [AWS IAM Official User Guide](https://docs.aws.amazon.com/IAM/latest/UserGuide/introduction.html)
- [AWS Security Token Service (STS) API Reference](https://docs.aws.amazon.com/STS/latest/APIReference/welcome.html)
- [Security Best Practices in IAM](https://docs.aws.amazon.com/IAM/latest/UserGuide/best-practices.html)
- [IAM Policy Evaluation Logic Documentation](https://docs.aws.amazon.com/IAM/latest/UserGuide/reference_policies_evaluation-logic.html)

---

[Previous: 04-organizations-scp-and-control-tower.md](file:///04-devops/aws/01-foundations/04-organizations-scp-and-control-tower.md) | [Section Index](file:///04-devops/aws/01-foundations/README.md) | [Next: 06-iam-policy-evaluation-and-least-privilege.md](file:///04-devops/aws/01-foundations/06-iam-policy-evaluation-and-least-privilege.md)
