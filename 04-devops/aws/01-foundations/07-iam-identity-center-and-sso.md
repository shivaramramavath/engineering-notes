# AWS IAM Identity Center: Enterprise SSO, Permission Sets & SCIM

> AWS IAM Identity Center (formerly AWS Single Sign-On) is the modern enterprise identity hub that centralizes workforce authentication, connects external SAML/OIDC Identity Providers, and vends temporary short-lived credentials across multi-account organizations via Permission Sets.

| Metadata | Details |
|---|---|
| **Level** | Intermediate to Advanced |
| **Prerequisites** | [05-iam-fundamentals.md](file:///04-devops/aws/01-foundations/05-iam-fundamentals.md) |
| **Time to Read** | 25 minutes |
| **Status** | Complete |
| **OS / Cloud Scope** | Enterprise Workforce Identity & Multi-Account Cloud Access |
| **Tool Version** | IAM Identity Center (v2), AWS CLI v2 SSO |
| **Tested With** | AWS CLI v2.22+, SAML 2.0 / SCIM IdP, Okta / Entra ID |

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
In the early days of cloud computing, administering an enterprise with 50 AWS accounts required creating individual IAM users in every single account. Developers juggled dozens of long-lived access keys, passwords lacked centralized complexity policies, and when an employee departed the organization, security teams had to manually hunt down and revoke IAM users across 50 separate consoles.

**AWS IAM Identity Center** is the definitive replacement for individual IAM users. Acting as the workforce identity gateway, it connects directly to corporate Identity Providers (Okta, Microsoft Entra ID, Google Workspace) using SAML 2.0 and SCIM. Users log in once through an enterprise portal and receive one-click federated access to authorized accounts. On the CLI, `aws sso login` authenticates via browser OAuth 2.0 PKCE, caching temporary STS credentials and eliminating static `AWS_ACCESS_KEY_ID` tokens entirely.

---

## 2. Core Concepts
- **Identity Sources**:
  - *Internal Directory*: Users and groups managed directly within IAM Identity Center.
  - *External Identity Provider (IdP)*: Enterprise federation using SAML 2.0 for single sign-on authentication and **SCIM (System for Cross-domain Identity Management)** for automated user and group lifecycle provisioning.
- **Permission Sets**:
  - Reusable collections of managed and inline policies defined once in the management account.
  - Conceptually identical to IAM roles, but dynamically provisioned across target member accounts by Identity Center.
- **Account Assignments**:
  - The binding linking a Principal (User or Group) to a target AWS Account using a specific Permission Set:
    $$\text{Account Assignment} = \text{Principal (Group)} + \text{Permission Set} + \text{Target Account}$$
- **AWS Access Portal**:
  - A customizable, HTTPS-secured dashboard (`https://<portal-name>.awsapps.com/start`) where authenticated users view accessible accounts and launch console sessions or copy CLI credentials.
- **CLI v2 SSO Token Cache**:
  - AWS CLI v2 retrieves OAuth 2.0 access tokens, caching them locally in `~/.aws/sso/cache/`. The CLI automatically calls `sts:AssumeRoleWithSAML` or OIDC endpoints to refresh credentials without user re-authentication for up to 8 hours.

---

## 3. How It Works Internally
Federated Identity Flow from Corporate IdP to Target AWS Member Account:

```
[ User Browser / CLI ]
        |
        | 1. Initiates Login (OAuth 2.0 Device Code / PKCE)
        v
+-------------------------------------------------------------+
|               AWS IAM Identity Center Portal                |
+-------------------------------------------------------------+
        |
        | 2. Redirects SAML 2.0 Auth Request
        v
+-------------------------------------------------------------+
|          Corporate IdP (Okta / Entra ID / Google)           |
|          - Verifies MFA, corporate password, FIDO2          |
+-------------------------------------------------------------+
        |
        | 3. Returns Signed SAML 2.0 Assertion
        v
+-------------------------------------------------------------+
|               AWS IAM Identity Center Portal                |
|  - Maps SAML user/group to assigned Permission Sets         |
+-------------------------------------------------------------+
        |
        | 4. Assumes Auto-Provisioned Service-Linked Role:
        |    "AWSReservedSSO_AdministratorAccess_xxx"
        v
+-------------------------------------------------------------+
|                 Target Member AWS Account                   |
|  - STS vends temporary ASIA... credentials (1h - 12h)       |
+-------------------------------------------------------------+
```

### Permission Set Provisioning Mechanics
When an administrator attaches a Permission Set to an Account, Identity Center automatically deploys an IAM Role in the member account prefixed with `AWSReservedSSO_<PermissionSetName>_<hash>`. The role's trust policy explicitly trusts the Identity Center service principal in the management account.

---

## 4. Syntax & Usage
Configuring and using AWS CLI v2 with IAM Identity Center:

```bash
# Configure an AWS CLI profile backed by IAM Identity Center
aws configure sso

# Interactive setup prompt sequence:
# SSO session name: company-sso
# SSO start URL [None]: https://my-company.awsapps.com/start
# SSO region [None]: us-east-1
# SSO registration scopes [sso:account:access]: sso:account:access

# Authenticate via browser
aws sso login --profile developer-prod

# Verify identity and assumed temporary role
aws sts get-caller-identity --profile developer-prod

# List permission sets via CLI
aws sso-admin list-permission-sets --instance-arn <INSTANCE_ARN>
```

---

## 5. Practical Examples

### Example 1: Local AWS CLI SSO Configuration
- **Goal:** Configure `~/.aws/config` to support zero-key SSO authentication across multiple accounts.
- **File Content (`~/.aws/config`):**
```ini
[sso-session enterprise-sso]
sso_start_url = https://enterprise-core.awsapps.com/start
sso_region = us-east-1
sso_registration_scopes = sso:account:access

[profile production-admin]
sso_session = enterprise-sso
sso_account_id = 111122223333
sso_role_name = AdministratorAccess
region = us-east-1
output = json

[profile development-developer]
sso_session = enterprise-sso
sso_account_id = 444455556666
sso_role_name = DeveloperPowerUser
region = us-west-2
output = json
```
- **CLI Login Execution:**
```bash
aws sso login --sso-session enterprise-sso
```
- **Real Output:**
```text
Attempting to automatically open the SSO authorization page in your default browser.
If the browser does not open or you wish to use a different device to authorize this request, open the following URL:

https://device.sso.us-east-1.amazonaws.com/

Then enter the code:
ABCD-EFGH
Successfully logged into SSO session: enterprise-sso
```
- **Explanation:** Browser opens, developer authenticates with corporate Okta/Google credentials, and the CLI caches rotating STS tokens.

### Example 2: Creating a Custom Permission Set with Inline Scoped Policy
- **Goal:** Define a Permission Set granting S3 read access and CloudWatch log inspection.
- **Commands:**
```bash
INSTANCE_ARN=$(aws sso-admin list-instances --query "Instances[0].InstanceArn" --output text)

# Create Permission Set
PERMISSION_SET_ARN=$(aws sso-admin create-permission-set \
    --instance-arn "$INSTANCE_ARN" \
    --name "AuditorAccess" \
    --description "Read-only access to S3 and CloudWatch" \
    --session-duration "PT2H" \
    --query "PermissionSet.PermissionSetArn" --output text)

# Attach AWS Managed Policy
aws sso-admin attach-managed-policy-to-permission-set \
    --instance-arn "$INSTANCE_ARN" \
    --permission-set-arn "$PERMISSION_SET_ARN" \
    --managed-policy-arn "arn:aws:iam::aws:policy/AmazonS3ReadOnlyAccess"
```
- **Explanation:** Establishes a reusable permission blueprint without creating IAM roles manually in individual accounts.

### Example 3: Edge Case - Session Duration Limits
- **Goal:** Configure maximum session duration for high-privilege administrative access.
- **Commands:**
```bash
aws sso-admin update-permission-set \
    --instance-arn "$INSTANCE_ARN" \
    --permission-set-arn "$PERMISSION_SET_ARN" \
    --session-duration "PT1H"
```
- **Explanation:** Duration strings follow ISO 8601 duration format (`PT1H` = 1 hour, `PT12H` = 12 hours). Production administrative roles should be capped at 1–2 hours.

### Example 4: Failure & Anti-Pattern - Manual IAM User Creation in Member Accounts
- **Goal:** Illustrate what happens when an engineering team circumvents SSO by creating IAM users.
- **Audit Script:**
```bash
aws iam list-users --query "Users[].[UserName, CreateDate]" --output table
```
- **Vulnerability:** Dispersed IAM users lack centralized MFA, bypass SCIM offboarding, and accumulate unrotated access keys. In modern AWS compliance audits (SOC 2, ISO 27001), having local IAM users in member accounts produces immediate audit findings.

### Example 5: Assigning a Corporate Group to an Account via CLI
- **Goal:** Grant the corporate `DevOps-Engineers` group access to the Production account using the `AdministratorAccess` permission set.
- **Commands:**
```bash
aws sso-admin create-account-assignment \
    --instance-arn "$INSTANCE_ARN" \
    --target-id "111122223333" \
    --target-type "AWS_ACCOUNT" \
    --permission-set-arn "$PERMISSION_SET_ARN" \
    --principal-type "GROUP" \
    --principal-id "9067793d-4031-707a-d0b8-0123456789ab"
```
- **Real Output:**
```json
{
    "AccountAssignmentCreationStatus": {
        "Status": "IN_PROGRESS",
        "RequestId": "b1a2c3d4-e5f6-7890-abcd-1234567890ab"
    }
}
```
- **Explanation:** Identity Center provisions the underlying IAM role and updates the user's access portal asynchronously.

---

## 6. Best Practices
1. **Assign Permissions to Groups, Never Individuals**: Map corporate Okta/Entra ID groups to permission sets. Onboard and offboard users solely by managing IdP group memberships.
2. **Enable SCIM Protocol**: Automate user and group provisioning so that deactivating an employee in Google/Okta immediately terminates all active AWS console sessions and CLI tokens.
3. **Use Short Session Durations for Production**: Set production permission set session durations to 1–2 hours (`PT1H`), while non-prod can be 4–8 hours (`PT8H`).
4. **Delete All IAM Users in Member Accounts**: Strive for 100% Identity Center adoption. Enforce an SCP denying `iam:CreateUser` and `iam:CreateAccessKey`.
5. **Standardize CLI Profiles with `sso-session`**: Share a single `sso-session` block across multiple account profiles in `~/.aws/config` so one `aws sso login` authenticates all accounts simultaneously.

---

## 7. Common Mistakes & Pitfalls

| Mistake | Why It Fails | Practical Remedy |
|---|---|---|
| Creating IAM users for employees in 2026 | Sprawling credentials, no central MFA, compliance failure | Transition all human users to IAM Identity Center |
| Assigning permission sets to individual users | High administrative overhead; access remains when user changes roles | Bind permission sets strictly to SCIM-synced IdP groups |
| Hardcoding access keys because "SSO is only for console" | AWS CLI v2 natively supports SSO login with zero static keys | Use `aws configure sso` and `aws sso login` |
| Overlooking permission set re-provisioning | Changing an attached policy requires Identity Center to update member accounts | Monitor the asynchronous provisioning status until `SUCCEEDED` |
| Using long session durations (12h) on admin roles | Compromised machine retains valid tokens for half a day | Enforce 1-hour session durations on privileged permission sets |
| Enabling Identity Center in a member account | Only the AWS Organizations Management Account can host the primary instance | Deploy IAM Identity Center in the Management Account or delegated admin |

---

## 8. Performance, Security & Cost Notes
- **Performance**: The AWS CLI v2 SSO token cache avoids repeated browser logins. When a token expires, the CLI checks the refresh token locally and silently requests a new STS session in ~150ms.
- **Security**: Identity Center supports WebAuthn / FIDO2 security keys (YubiKeys), biometric authentication (Touch ID, Windows Hello), and contextual access rules from enterprise IdPs.
- **Cost**: AWS IAM Identity Center is provided at **zero additional charge** with no per-user license fees.

---

## 9. Comparison / When to Use What

| Identity Solution | Identity Source | Credential Type | Best Fit |
|---|---|---|---|
| **IAM Identity Center (SSO)** | Okta / Entra ID / Internal | Rotating STS (1h–12h) | **Mandatory standard** for all human access in multi-account AWS |
| **IAM User** | AWS local account | Static permanent keys | Legacy workloads unable to assume roles (anti-pattern) |
| **IAM Role (STS)** | AWS Services / OIDC | Rotating STS | Service workloads, EC2, Lambda, GitHub Actions |
| **Amazon Cognito** | Social / Customer DB | JWT (OIDC/OAuth 2) | External mobile and web consumer application users |

---

## 10. Interview Questions

<details>
<summary>1. How does IAM Identity Center replace traditional IAM users across a multi-account organization?</summary>
Instead of managing distinct IAM users in each account, Identity Center federates with an enterprise IdP (e.g. Okta, Entra ID). Users log in once to an access portal. Permission Sets define policies centrally, and Identity Center automatically provisions corresponding temporary IAM roles across target member accounts.
</details>

<details>
<summary>2. What is the role of SCIM in IAM Identity Center?</summary>
SCIM (System for Cross-domain Identity Management) is an open API standard that synchronizes user identities and group memberships automatically from an external IdP to AWS. When an employee is removed from the corporate IdP, SCIM immediately deletes their identity in AWS, revoking access across all accounts.
</details>

<details>
<summary>3. What is an AWS Permission Set and how is it implemented under the hood?</summary>
A Permission Set is a template of managed policies, customer inline policies, and session durations defined centrally. When assigned to an account, Identity Center provisions an IAM role in that account named `AWSReservedSSO_<Name>_<hash>` that trusts Identity Center to vend STS credentials.
</details>

<details>
<summary>4. How does `aws sso login` work without static access keys?</summary>
`aws sso login` initiates an OAuth 2.0 Device Authorization flow, opening a browser where the user completes SSO authentication. The CLI receives an authorization token, caches it in `~/.aws/sso/cache/`, and automatically calls STS to fetch short-lived `ASIA...` credentials as needed.
</details>

<details>
<summary>5. Why should Permission Sets be assigned to Groups rather than Users?</summary>
Role-Based Access Control (RBAC) best practices dictate managing permissions via group memberships. Assigning to groups ensures access is automatically granted or revoked when a user moves teams in the corporate directory, eliminating manual AWS assignment maintenance.
</details>

<details>
<summary>6. What happens when an administrator updates a Permission Set policy in Identity Center?</summary>
Identity Center asynchronously triggers a re-provisioning workflow across every member account where that Permission Set is assigned, updating the corresponding `AWSReservedSSO_` IAM roles.
</details>

<details>
<summary>7. What is the difference between Amazon Cognito and IAM Identity Center?</summary>
IAM Identity Center is for **workforce identities** (employees, contractors, engineers accessing AWS accounts and internal business apps). Amazon Cognito is for **customer identities** (external end-users signing up for a consumer mobile app or SaaS web portal).
</details>

<details>
<summary>8. Can an SCP restrict a role assumed through IAM Identity Center?</summary>
Yes. Roles created by Identity Center (`AWSReservedSSO_...`) are standard IAM roles in the member account. They are fully subject to all Service Control Policies (SCPs) active on that account and its parent OUs.
</details>

---

## 11. Practice Exercises

<details>
<summary>Exercise 1 (Easy): Configure an SSO session in the AWS CLI</summary>
**Task:** Run the interactive wizard to set up an SSO session named `dev-session`.

**Solution:**
```bash
aws configure sso
# Follow interactive prompts to set start URL and region
```
</details>

<details>
<summary>Exercise 2 (Easy): List active Identity Center instances</summary>
**Task:** Query the ARN and identity store ID of your primary Identity Center instance.

**Solution:**
```bash
aws sso-admin list-instances --query "Instances[].[InstanceArn, IdentityStoreId]" --output table
```
</details>

<details>
<summary>Exercise 3 (Medium): Inspect assigned permission sets for an account</summary>
**Task:** List all Permission Sets provisioned into account `123456789012`.

**Solution:**
```bash
INSTANCE_ARN=$(aws sso-admin list-instances --query "Instances[0].InstanceArn" --output text)
aws sso-admin list-permission-sets-provisioned-to-account \
    --instance-arn "$INSTANCE_ARN" \
    --account-id "123456789012" \
    --output table
```
</details>

<details>
<summary>Exercise 4 (Medium): Query SCIM-synced groups in Identity Store</summary>
**Task:** Search the Identity Store for a group named `Security-Admins`.

**Solution:**
```bash
STORE_ID=$(aws sso-admin list-instances --query "Instances[0].IdentityStoreId" --output text)
aws identitystore list-groups \
    --identity-store-id "$STORE_ID" \
    --filters AttributePath=DisplayName,AttributeValue=Security-Admins \
    --output json
```
</details>

<details>
<summary>Exercise 5 (Hard): Enforce SSO-only access via an SCP</summary>
**Task:** Write an SCP denying all actions unless the principal is using an assumed role or federated session (blocking raw IAM user access keys).

**Solution:**
```json
{
    "Version": "2012-10-17",
    "Statement": [{
        "Sid": "DenyRawIAMUserCredentials",
        "Effect": "Deny",
        "Action": "*",
        "Resource": "*",
        "Condition": {
            "StringLike": {
                "aws:PrincipalArn": "arn:aws:iam::*:user/*"
            }
        }
    }]
}
```
</details>

<details>
<summary>Exercise 6 (Hard): Script automated permission set provisioning across all OU accounts</summary>
**Task:** Write a bash script that iterates over all member accounts in a Workloads OU and assigns a `ReadOnlyAccess` permission set to the `Auditors` group.

**Solution:**
```bash
#!/usr/bin/env bash
set -euo pipefail

INSTANCE_ARN=$(aws sso-admin list-instances --query "Instances[0].InstanceArn" --output text)
PSET_ARN="arn:aws:sso:::permissionSet/ssoins-1234/ps-abcd"
GROUP_ID="9067793d-4031-707a-d0b8-0123456789ab"
OU_ID="ou-1234-workloads"

ACCOUNTS=$(aws organizations list-children --parent-id "$OU_ID" --child-type "ACCOUNT" --query "Children[].Id" --output text)

for acc in $ACCOUNTS; do
    echo "Assigning Auditors to Account: $acc"
    aws sso-admin create-account-assignment \
        --instance-arn "$INSTANCE_ARN" \
        --target-id "$acc" \
        --target-type "AWS_ACCOUNT" \
        --permission-set-arn "$PSET_ARN" \
        --principal-type "GROUP" \
        --principal-id "$GROUP_ID" > /dev/null
done
echo "All account assignments initiated successfully."
```
</details>

---

## 12. Summary / Cheat Sheet

```
+---------------------------------------------------------------------------------+
|                         IAM IDENTITY CENTER CHEAT SHEET                         |
+---------------------------------------------------------------------------------+
| ARCHITECTURE   | Enterprise IdP (SAML/SCIM) -> Identity Center -> Member Roles  |
| CORE BINDING   | Group + Permission Set + Target Account = Account Assignment   |
| ROLE CREATED   | AWSReservedSSO_<PermissionSetName>_<hash> in member accounts   |
| CLI SETUP      | aws configure sso                                              |
| CLI LOGIN      | aws sso login --profile <profile-name>                         |
| SCIM PROTOCOL  | Real-time automated user/group provisioning & deprovisioning   |
| BEST PRACTICE  | Assign to groups, zero local IAM users, 1-2h admin sessions     |
| PRICE          | Free of charge (Zero per-user license fees)                     |
+---------------------------------------------------------------------------------+
```

---

## 13. Further Reading
- [AWS IAM Identity Center User Guide](https://docs.aws.amazon.com/singlesignon/latest/userguide/what-is.html)
- [Configuring the AWS CLI to Use IAM Identity Center](https://docs.aws.amazon.com/cli/latest/userguide/cli-configure-sso.html)
- [SCIM Implementation Guide for AWS Identity Center](https://docs.aws.amazon.com/singlesignon/latest/userguide/scim-profile.html)
- [AWS Security Reference Architecture: Workforce Identity](https://docs.aws.amazon.com/prescriptive-guidance/latest/security-reference-architecture/identity-center.html)

---

[Previous: 06-iam-policy-evaluation-and-least-privilege.md](file:///04-devops/aws/01-foundations/06-iam-policy-evaluation-and-least-privilege.md) | [Section Index](file:///04-devops/aws/01-foundations/README.md) | [Next: 08-aws-cli.md](file:///04-devops/aws/01-foundations/08-aws-cli.md)
