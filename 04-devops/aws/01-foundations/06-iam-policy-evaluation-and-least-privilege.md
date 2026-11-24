# IAM Policy Evaluation Logic, Condition Keys & Least Privilege

> The AWS IAM evaluation engine is a deterministic, default-deny policy arbiter that synthesizes identity policies, resource policies, permission boundaries, and SCPs against contextual request keys to enforce mathematically provable least privilege.

| Metadata | Details |
|---|---|
| **Level** | Intermediate to Advanced |
| **Prerequisites** | [05-iam-fundamentals.md](file:///04-devops/aws/01-foundations/05-iam-fundamentals.md) |
| **Time to Read** | 25 minutes |
| **Status** | Complete |
| **OS / Cloud Scope** | Global IAM Authorization Engine |
| **Tool Version** | IAM Policy Grammar 2012-10-17, IAM Access Analyzer |
| **Tested With** | AWS IAM Policy Simulator, AWS CLI v2.22+ |

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
In cloud security, misconfigured authorization policies represent the single largest attack vector for data breaches and lateral movement. Broad permissions like `"Action": "*"` and `"Resource": "*"` turn minor application vulnerabilities (such as SQL injection or SSRF) into catastrophic, full-account takeovers.

Understanding the internal evaluation logic of AWS IAM is essential for engineering secure systems. IAM policies are evaluated through a strict deterministic algorithm where an **explicit Deny always overrides any number of Allows**, and where access is denied by default. By leveraging **Condition Keys** (restricting access by IP, MFA status, TLS version, or resource tags) and **Permission Boundaries**, security teams can delegate role creation to developers while guaranteeing that permissions cannot be escalated beyond defined guardrails.

---

## 2. Core Concepts
- **The Core Policy Elements**:
  - `Version`: Must always be `"2012-10-17"` (the current policy grammar version). Never use the date the policy was written.
  - `Effect`: Either `"Allow"` or `"Deny"`.
  - `Action` / `NotAction`: Specific API operations (e.g. `s3:GetObject`, `ec2:DescribeInstances`).
  - `Resource` / `NotResource`: The Amazon Resource Name (ARN) of the entities affected by the action.
  - `Condition`: Key-value predicates that must evaluate to true for the statement to apply.
  - `Principal`: Required in resource-based policies and trust policies; omitted in identity-based policies.
- **The Fundamental Evaluation Rule**:
  1. Default: **Implicit Deny**.
  2. Any **Explicit Deny** found across any policy? $\rightarrow$ Request is **DENIED** immediately.
  3. Any **Explicit Allow** found and no explicit deny?
     - For same-account requests: An allow in an identity-based policy OR resource-based policy permits the call.
     - For cross-account requests: An allow must exist in **both** the caller's identity policy AND the resource-based policy.
  4. If no explicit allow is found $\rightarrow$ Request is **DENIED** (Implicit Deny).
- **Permission Boundaries**:
  - An advanced guardrail attached to an IAM user or role setting the maximum permissions that can be granted.
  - An identity policy cannot grant permissions that exceed the attached permission boundary.
- **Condition Operators**:
  - String: `StringEquals`, `StringLike`, `StringNotEquals`.
  - Numeric: `NumericLessThan`, `NumericGreaterThanEquals`.
  - Date: `DateGreaterThan`, `DateLessThan`.
  - Boolean: `Bool` (e.g. `aws:MultiFactorAuthPresent`).
  - IP Address: `IpAddress`, `NotIpAddress` (CIDR blocks).
  - Null check: `Null: { "aws:RequestTag/Environment": "true" }`.

---

## 3. How It Works Internally
The Complete IAM Evaluation Algorithm Flowchart:

```
                  [ Inbound API Request Initiated ]
                                  |
                                  v
              +---------------------------------------+
              | Does any applicable policy contain an  | ---- YES ----> [ EXPLICIT DENY ]
              |          EXPLICIT DENY?               |
              +---------------------------------------+
                                  | NO
                                  v
              +---------------------------------------+
              | Is request constrained by an SCP?     | ---- NO -----> [ IMPLICIT DENY ]
              | Does SCP allow the action?            |
              +---------------------------------------+
                                  | YES
                                  v
              +---------------------------------------+
              | Is there a Permission Boundary?       | ---- NO -----> [ IMPLICIT DENY ]
              | Does Boundary allow the action?       |
              +---------------------------------------+
                                  | YES
                                  v
              +---------------------------------------+
              | Is there a Session Policy?            | ---- NO -----> [ IMPLICIT DENY ]
              | Does Session Policy allow the action? |
              +---------------------------------------+
                                  | YES
                                  v
              +---------------------------------------+
              | Does an Identity Policy OR a Resource | ---- YES ----> [ ALLOWED ]
              |       Policy explicitly ALLOW?        |
              +---------------------------------------+
                                  | NO
                                  v
                          [ IMPLICIT DENY ]
```

---

## 4. Syntax & Usage
Authoring and testing policies using the AWS CLI and policy tools:

```bash
# Validate policy syntax using AWS IAM Access Analyzer
aws accessanalyzer validate-policy \
    --policy-document file://policy.json \
    --policy-type IDENTITY_POLICY

# Simulate policy evaluation against target API actions
aws iam simulate-custom-policy \
    --policy-input-list file://policy.json \
    --action-names "s3:GetObject" "s3:PutObject" \
    --resource-arns "arn:aws:s3:::production-vault/data.csv"

# Attach a Permission Boundary to a role
aws iam put-role-permissions-boundary \
    --role-name "DeveloperExecutionRole" \
    --permissions-boundary "arn:aws:iam::123456789012:policy/DeveloperBoundary"
```

---

## 5. Practical Examples

### Example 1: Enforcing TLS 1.2+ and HTTPS on S3 via Condition Key
- **Goal:** Ensure all S3 bucket traffic is encrypted in transit over HTTPS and reject unencrypted HTTP requests.
- **Bucket Policy (`enforce-tls.json`):**
```json
{
    "Version": "2012-10-17",
    "Statement": [
        {
            "Sid": "EnforceHTTPSOnly",
            "Effect": "Deny",
            "Principal": "*",
            "Action": "s3:*",
            "Resource": [
                "arn:aws:s3:::secure-company-vault",
                "arn:aws:s3:::secure-company-vault/*"
            ],
            "Condition": {
                "Bool": {
                    "aws:SecureTransport": "false"
                }
            }
        }
    ]
}
```
- **Explanation:** `aws:SecureTransport: "false"` evaluates to true on plaintext HTTP calls, triggering the explicit Deny.

### Example 2: Restricting Access to a Specific Corporate IP Range
- **Goal:** Allow administrator actions only when originating from corporate office CIDR blocks or VPN IPs.
- **Identity Policy Statement:**
```json
{
    "Version": "2012-10-17",
    "Statement": [
        {
            "Sid": "DenyAccessOutsideCorporateNetwork",
            "Effect": "Deny",
            "Action": "*",
            "Resource": "*",
            "Condition": {
                "NotIpAddress": {
                    "aws:SourceIp": [
                        "198.51.100.0/24",
                        "203.0.113.50/32"
                    ]
                },
                "Bool": {
                    "aws:ViaAWSService": "false"
                }
            }
        }
    ]
}
```
- **Explanation:** Denies API requests originating outside the whitelist while allowing AWS services to make internal calls on behalf of the user (`aws:ViaAWSService`).

### Example 3: Edge Case - Permission Boundary for Delegated Administration
- **Goal:** Allow a developer to create roles for Lambda functions, but prevent them from creating an administrative role that escalates their own privileges.
- **Permission Boundary Policy (`lambda-boundary.json`):**
```json
{
    "Version": "2012-10-17",
    "Statement": [
        {
            "Sid": "AllowedLambdaActions",
            "Effect": "Allow",
            "Action": [
                "logs:CreateLogGroup",
                "logs:CreateLogStream",
                "logs:PutLogEvents",
                "dynamodb:GetItem",
                "dynamodb:PutItem"
            ],
            "Resource": "*"
        }
    ]
}
```
- **Developer Creation Rule**: The developer's IAM policy requires that whenever they call `iam:CreateRole`, they must pass the boundary:
```json
{
    "Effect": "Allow",
    "Action": "iam:CreateRole",
    "Resource": "*",
    "Condition": {
        "StringEquals": {
            "iam:PermissionsBoundary": "arn:aws:iam::123456789012:policy/LambdaBoundary"
        }
    }
}
```
- **Explanation:** Even if the developer attaches `AdministratorAccess` to their new role, the role can only execute the actions allowed by the boundary.

### Example 4: Failure & Anti-Pattern - Misunderstanding `NotAction` with `Allow`
- **Goal:** Demonstrate catastrophic security hole caused by mixing `NotAction` with `Allow`.
- **Dangerous Anti-Pattern Statement:**
```json
{
    "Effect": "Allow",
    "NotAction": "iam:*",
    "Resource": "*"
}
```
- **Explanation & Vulnerability:** Many developers assume this means "allow everything except IAM". In reality, it grants permission to **every single API action in AWS across all 200+ services** (EC2, S3, RDS, Billing, DynamoDB) EXCEPT IAM. An attacker could delete all databases. Never use `NotAction` with `Allow`; use `NotAction` only with `Deny`.

### Example 5: Tag-Based Access Control (ABAC) using Condition Keys
- **Goal:** Allow engineers to start/stop EC2 instances only if the instance's `Department` tag matches their IAM principal tag.
- **ABAC Policy:**
```json
{
    "Version": "2012-10-17",
    "Statement": [
        {
            "Sid": "AllowInstanceControlMatchingTags",
            "Effect": "Allow",
            "Action": [
                "ec2:StartInstances",
                "ec2:StopInstances"
            ],
            "Resource": "arn:aws:ec2:*:*:instance/*",
            "Condition": {
                "StringEquals": {
                    "ec2:ResourceTag/Department": "${aws:PrincipalTag/Department}"
                }
            }
        }
    ]
}
```
- **Explanation:** Attribute-Based Access Control (ABAC) scales effortlessly: adding new departments requires no policy updates, only tag management.

---

## 6. Best Practices
1. **Grant Least Privilege with Specific Resource ARNs**: Avoid `Resource: "*"` on write actions; specify target bucket, table, or instance ARNs.
2. **Always Use `aws:SecureTransport: "false"` on Storage**: Deny non-HTTPS traffic on all S3 bucket and SQS queue policies.
3. **Use Permission Boundaries for CI/CD and Developers**: Prevent automation pipelines from escalating privileges during Terraform deployments.
4. **Leverage IAM Access Analyzer**: Automatically identify public or cross-account access grants on S3 buckets, KMS keys, and IAM roles.
5. **Never Use `NotAction` with `Allow`**: Restrict `NotAction` strictly to `Deny` statements (such as region locks).

---

## 7. Common Mistakes & Pitfalls

| Mistake | Why It Fails | Practical Remedy |
|---|---|---|
| Setting `"Version": "2026-11-24"` | `Version` is the policy grammar version, not creation date; causes syntax parse error | Always specify `"Version": "2012-10-17"` |
| Using `NotAction` with `Effect: Allow` | Grants full administrative access to all AWS services except the omitted ones | Use explicit `Action` arrays or combine `NotAction` only with `Effect: Deny` |
| Overlooking S3 sub-resource ARN syntax | Specifying `arn:aws:s3:::mybucket` grants bucket-level access, but NOT object-level `s3:GetObject` | Specify both `arn:aws:s3:::mybucket` and `arn:aws:s3:::mybucket/*` |
| Forgetting `aws:ViaAWSService` in IP locks | IP restriction denies AWS services (CloudFormation, Lambda) calling APIs internally | Add condition `Bool: {"aws:ViaAWSService": "false"}` to IP deny rules |
| Misunderstanding cross-account evaluation | Caller assumes having an Allow in Account A identity policy is sufficient | Cross-account access mandates an Allow in Account A AND Account B's resource policy |
| Overwriting Customer Managed Policy versions | IAM retains up to 5 policy versions; updating beyond 5 fails if old versions aren't pruned | Automate pruning of older policy versions during deployment |

---

## 8. Performance, Security & Cost Notes
- **Performance**: Policy size limits are hard-capped by AWS: 10,240 characters for customer managed policies and 2,048 characters for inline role policies. Large wildcard-free policies must be split across multiple managed policies or refactored into ABAC tag conditions.
- **Security**: Explicit Deny statements are evaluated first and cannot be overridden by any Allow statement. Use explicit Denies for non-negotiable compliance guardrails.
- **Cost**: IAM policy evaluation and IAM Access Analyzer basic checks are completely **free of charge**.

---

## 9. Comparison / When to Use What

| Access Control Pattern | Primary Mechanism | Scalability | When to Use |
|---|---|---|---|
| **Role-Based (RBAC)** | Discrete policies per job role | Moderate (Policy sprawl as team grows) | Small teams, static service roles |
| **Attribute-Based (ABAC)** | Tags on principals matched against resources | High (Zero policy updates when onboarding) | Large enterprises, multi-tenant engineering |
| **Permission Boundary** | Upper ceiling policy attached to principal | High (Enables safe delegated admin) | CI/CD pipelines, developer sandboxes |
| **Resource Policy** | Policy attached directly to S3, SQS, KMS | Direct | Cross-account access, public endpoints |

---

## 10. Interview Questions

<details>
<summary>1. An IAM role has an identity policy with `Effect: Allow` on `s3:*`. An attached permission boundary has `Effect: Allow` on `dynamodb:*`. Can the role access S3?</summary>
No. For an action to be allowed, it must be permitted by BOTH the identity policy AND the permission boundary. Since the boundary only allows DynamoDB, S3 requests are denied by implicit deny.
</details>

<details>
<summary>2. Explain what happens when an IAM policy uses `NotAction: "iam:*"` with `Effect: "Allow"`.</summary>
It grants permission to every single API action across all AWS services (EC2, S3, RDS, DynamoDB, Lambda, etc.) EXCEPT IAM actions. This is an anti-pattern that inadvertently grants near-complete administrative access.
</details>

<details>
<summary>3. Why must an S3 bucket policy specify both `arn:aws:s3:::bucket` and `arn:aws:s3:::bucket/*`?</summary>
Because `arn:aws:s3:::bucket` represents the bucket container itself (required for operations like `s3:ListBucket`), whereas `arn:aws:s3:::bucket/*` represents the objects stored within the bucket (required for `s3:GetObject` and `s3:PutObject`).
</details>

<details>
<summary>4. How does cross-account authorization evaluation differ from same-account evaluation?</summary>
In the same account, an Allow in either an identity-based policy OR a resource-based policy is sufficient to grant access. In cross-account scenarios, an explicit Allow must be present in BOTH the caller's identity policy AND the target account's resource policy (or role trust policy).
</details>

<details>
<summary>5. What does the `aws:SecureTransport` condition key enforce?</summary>
It checks whether the request was transmitted using TLS/HTTPS. Setting `Effect: Deny` with `Condition: Bool: {"aws:SecureTransport": "false"}` guarantees that unencrypted HTTP requests are rejected.
</details>

<details>
<summary>6. What is the policy grammar version string, and why must it be "2012-10-17"?</summary>
The version string identifies the syntax capabilities of the policy engine (e.g. support for policy variables and condition keys). "2012-10-17" is the current standard; setting any other date causes syntax parsing errors.
</details>

<details>
<summary>7. How does Attribute-Based Access Control (ABAC) reduce policy management overhead?</summary>
ABAC matches tags on the requesting principal (`${aws:PrincipalTag/Key}`) with tags on the target resource (`ec2:ResourceTag/Key`). Instead of updating policies whenever a new project or employee joins, administrators simply tag resources appropriately.
</details>

<details>
<summary>8. Can an explicit Deny in an identity policy be overridden by an administrator?</summary>
No. In the IAM evaluation engine, an explicit Deny takes absolute precedence over all Allow statements across identity policies, resource policies, boundaries, and SCPs.
</details>

---

## 11. Practice Exercises

<details>
<summary>Exercise 1 (Easy): Validate an IAM policy using Access Analyzer</summary>
**Task:** Validate a local JSON policy file to detect syntax errors and security warnings.

**Solution:**
```bash
aws accessanalyzer validate-policy \
    --policy-document file://my-policy.json \
    --policy-type IDENTITY_POLICY \
    --output table
```
</details>

<details>
<summary>Exercise 2 (Easy): Simulate policy action allowing S3 Read</summary>
**Task:** Use the IAM policy simulator to test if a policy allows `s3:GetObject`.

**Solution:**
```bash
aws iam simulate-custom-policy \
    --policy-input-list '{"Version":"2012-10-17","Statement":[{"Effect":"Allow","Action":"s3:GetObject","Resource":"arn:aws:s3:::my-bucket/*"}]}' \
    --action-names "s3:GetObject" \
    --resource-arns "arn:aws:s3:::my-bucket/file.txt" \
    --query "EvaluationResults[0].EvalDecision" \
    --output text
```
</details>

<details>
<summary>Exercise 3 (Medium): Author an S3 bucket policy enforcing HTTPS only</summary>
**Task:** Write and apply a bucket policy denying non-TLS access to `company-archive-vault`.

**Solution:**
```bash
aws s3api put-bucket-policy \
    --bucket "company-archive-vault" \
    --policy '{
        "Version": "2012-10-17",
        "Statement": [{
            "Sid": "EnforceTLS",
            "Effect": "Deny",
            "Principal": "*",
            "Action": "s3:*",
            "Resource": ["arn:aws:s3:::company-archive-vault", "arn:aws:s3:::company-archive-vault/*"],
            "Condition": {"Bool": {"aws:SecureTransport": "false"}}
        }]
    }'
```
</details>

<details>
<summary>Exercise 4 (Medium): Build an ABAC policy for team-based EC2 management</summary>
**Task:** Author a policy allowing users to reboot EC2 instances only if `Project` tag matches.

**Solution:**
```json
{
    "Version": "2012-10-17",
    "Statement": [{
        "Effect": "Allow",
        "Action": "ec2:RebootInstances",
        "Resource": "arn:aws:ec2:*:*:instance/*",
        "Condition": {
            "StringEquals": {
                "ec2:ResourceTag/Project": "${aws:PrincipalTag/Project}"
            }
        }
    }]
}
```
</details>

<details>
<summary>Exercise 5 (Hard): Enforce MFA on sensitive AWS API calls</summary>
**Task:** Write an identity policy denying terminating EC2 instances or deleting RDS databases unless MFA is active and authenticated within the last 1 hour.

**Solution:**
```json
{
    "Version": "2012-10-17",
    "Statement": [{
        "Sid": "DenyDestructiveActionsWithoutMFA",
        "Effect": "Deny",
        "Action": [
            "ec2:TerminateInstances",
            "rds:DeleteDBInstance"
        ],
        "Resource": "*",
        "Condition": {
            "BoolIfExists": {
                "aws:MultiFactorAuthPresent": "false"
            },
            "NumericGreaterThan": {
                "aws:MultiFactorAuthAge": 3600
            }
        }
    }]
}
```
</details>

<details>
<summary>Exercise 6 (Hard): Script automated pruning of old IAM policy versions</summary>
**Task:** Write a bash script that inspects an IAM policy and deletes the oldest non-default version if 5 versions exist.

**Solution:**
```bash
#!/usr/bin/env bash
set -euo pipefail

POLICY_ARN="arn:aws:iam::123456789012:policy/MyCustomPolicy"

VERSIONS=$(aws iam list-policy-versions --policy-arn "$POLICY_ARN" --query "Versions[?IsDefaultVersion==\`false\`].[VersionId, CreateDate]" --output text | sort -k2)
COUNT=$(echo "$VERSIONS" | wc -l)

if [ "$COUNT" -ge 4 ]; then
    OLDEST_VERSION=$(echo "$VERSIONS" | head -n1 | awk '{print $1}')
    echo "Deleting oldest non-default version: $OLDEST_VERSION"
    aws iam delete-policy-version --policy-arn "$POLICY_ARN" --version-id "$OLDEST_VERSION"
else
    echo "Policy has room for new versions ($COUNT non-default versions found)."
fi
```
</details>

---

## 12. Summary / Cheat Sheet

```
+---------------------------------------------------------------------------------+
|                    POLICY EVALUATION & LEAST PRIVILEGE CHEAT SHEET              |
+---------------------------------------------------------------------------------+
| VERSION        | Must strictly be "2012-10-17"                                  |
| EVALUATION     | Explicit Deny > Explicit Allow > Default (Implicit) Deny       |
| SAME-ACCOUNT   | Identity Allow OR Resource Allow = ACCESS GRANTED              |
| CROSS-ACCOUNT  | Identity Allow AND Resource Allow = ACCESS GRANTED             |
| BOUNDARY       | Limits ceiling: Effective = Identity Policy AND Boundary       |
| HTTPS ENFORCE  | Condition: Bool: { "aws:SecureTransport": "false" } -> Deny   |
| MFA ENFORCE    | Condition: BoolIfExists: { "aws:MultiFactorAuthPresent":"false"}|
| NOTACTION TRAP | Never use NotAction with Allow; only use NotAction with Deny   |
| ACCESS ANALYZE | aws accessanalyzer validate-policy --policy-document ...       |
+---------------------------------------------------------------------------------+
```

---

## 13. Further Reading
- [IAM Policy Evaluation Logic Official Guide](https://docs.spring.io/spring-boot/docs/current/reference/html/) -> [AWS Documentation](https://docs.aws.amazon.com/IAM/latest/UserGuide/reference_policies_evaluation-logic.html)
- [IAM JSON Policy Reference](https://docs.aws.amazon.com/IAM/latest/UserGuide/reference_policies.html)
- [AWS Global Condition Context Keys](https://docs.aws.amazon.com/IAM/latest/UserGuide/reference_policies_condition-keys.html)
- [AWS IAM Access Analyzer Documentation](https://docs.aws.amazon.com/IAM/latest/UserGuide/what-is-access-analyzer.html)

---

[Previous: 05-iam-fundamentals.md](file:///04-devops/aws/01-foundations/05-iam-fundamentals.md) | [Section Index](file:///04-devops/aws/01-foundations/README.md) | [Next: 07-iam-identity-center-and-sso.md](file:///04-devops/aws/01-foundations/07-iam-identity-center-and-sso.md)
