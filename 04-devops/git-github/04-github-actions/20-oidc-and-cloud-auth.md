---
title: "GitHub Actions OpenID Connect (OIDC) and Cloud Authentication"
category: "04-devops"
subcategory: "git-github / 04-github-actions"
tags: ["github-actions", "oidc", "aws", "gcp", "azure", "cloud-security", "iam", "zero-trust"]
difficulty: "Advanced"
estimated_read_time: "28 min"
prerequisites:
  - "04-github-actions/07-variables-and-secrets"
  - "04-github-actions/11-permissions-and-environments"
last_updated: "2026-11-25"
---

# GitHub Actions OpenID Connect (OIDC) and Cloud Authentication

> **Navigation**: [← Previous: 19-release-automation.md](file:///e:/Learning/engineering-notes/04-devops/git-github/04-github-actions/19-release-automation.md) | [Module README](file:///e:/Learning/engineering-notes/04-devops/git-github/04-github-actions/README.md) | [Next: 21-self-hosted-runners-and-scaling.md →](file:///e:/Learning/engineering-notes/04-devops/git-github/04-github-actions/21-self-hosted-runners-and-scaling.md)

---

## Table of Contents
1. [What & Why](#1-what--why)
2. [Core Concepts & The OIDC Handshake](#2-core-concepts--the-oidc-handshake)
3. [How It Works Internally](#3-how-it-works-internally)
4. [Syntax & Permissions](#4-syntax--permissions)
5. [Cloud Provider Integrations](#5-cloud-provider-integrations)
   - [AWS IAM Federation](#aws-iam-federation)
   - [Google Cloud Workload Identity](#google-cloud-workload-identity)
   - [Microsoft Azure Workload Identity](#microsoft-azure-workload-identity)
6. [Best Practices & Trust Policy Hardening](#6-best-practices--trust-policy-hardening)
7. [Common Mistakes](#7-common-mistakes)
8. [Performance, Security & Cost](#8-performance-security--cost)
9. [Comparison: Static Keys vs OIDC Tokens](#9-comparison-static-keys-vs-oidc-tokens)
10. [Interview Questions & Answers](#10-interview-questions--answers)
11. [Practice Exercises](#11-practice-exercises)
12. [Summary & Cheat Sheet](#12-summary--cheat-sheet)
13. [Further Reading](#13-further-reading)

---

## 1. What & Why

### What is OIDC in GitHub Actions?
OpenID Connect (OIDC) is an identity federation protocol that allows GitHub Actions workflows to authenticate directly with cloud providers (AWS, GCP, Azure, HashiCorp Vault) using short-lived, cryptographically signed JSON Web Tokens (JWTs). Instead of generating and storing permanent static credentials (`AWS_ACCESS_KEY_ID`, service account JSON keys) inside GitHub Secrets, the cloud provider verifies GitHub's signed token and temporarily issues short-lived scoped credentials (typically lasting 15 to 60 minutes).

### Why Do We Use It?
1. **Elimination of Secret Sprawl and Leaks**: Static credentials stored in repository secrets have no natural expiration date, are often leaked in logs or commits, and are rarely rotated.
2. **Granular Least Privilege Scoping**: Cloud trust policies can inspect the OIDC claims—allowing a role to be assumed **only** if the job originates from a specific repository (`org/repo`), branch (`refs/heads/main`), environment (`production`), or workflow.
3. **Zero Maintenance**: Eliminates periodic secret rotation overhead, compliance audit tracking, and automated key expiration breaks.
4. **Defense in Depth**: A stolen runner VM or compromised third-party action only gains access to an ephemeral token expiring in minutes rather than permanent master cloud keys.

---

## 2. Core Concepts & The OIDC Handshake

### 1. The GitHub OIDC Provider
GitHub runs an OIDC Identity Provider (`token.actions.githubusercontent.com`) that mints cryptographic JWTs signed by GitHub's private keys.

### 2. Standard Claims in GitHub OIDC Tokens
The minted JWT payload contains rich contextual claims:
```json
{
  "iss": "https://token.actions.githubusercontent.com",
  "aud": "https://github.com/my-org",
  "sub": "repo:my-org/my-repo:ref:refs/heads/main",
  "repository": "my-org/my-repo",
  "repository_owner": "my-org",
  "actor": "octocat",
  "workflow": "Continuous Deployment",
  "job_workflow_ref": "my-org/my-repo/.github/workflows/deploy.yml@refs/heads/main",
  "environment": "production"
}
```

### 3. Required Permission: `id-token: write`
By default, runners cannot request an OIDC JWT. You must explicitly configure:
```yaml
permissions:
  id-token: write
  contents: read
```

---

## 3. How It Works Internally

```
                   The 4-Step OIDC Cloud Handshake:

[ Runner VM ]          [ GitHub OIDC IdP ]          [ Cloud Provider (AWS/GCP/Azure) ]
      │                         │                                  │
      │ 1. Request JWT          │                                  │
      ├────────────────────────►│                                  │
      │    (with env context)   │                                  │
      │                         │                                  │
      │ 2. Return Signed JWT    │                                  │
      │◄────────────────────────┤                                  │
      │                         │                                  │
      │ 3. AssumeRoleWithWebIdentity (Signed JWT)                  │
      ├───────────────────────────────────────────────────────────►│
      │                                                            │
      │                                4. Validate signature against
      │                                   GitHub Public JWKS &
      │                                   evaluate Role Trust Policy
      │                                                            │
      │ 5. Return Ephemeral Cloud Credentials (STS / Token)        │
      │◄───────────────────────────────────────────────────────────┤
      │
[ Workload Runs AWS/Cloud CLI Commands Safely ]
```

---

## 4. Syntax & Permissions

```yaml
name: Deploy via OIDC
on:
  push:
    branches: [main]

permissions:
  id-token: write   # Mandatory for minting OIDC token!
  contents: read    # Required to check out repository code

jobs:
  deploy:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v4
      - name: Authenticate with Cloud Provider
        # Uses OIDC token automatically behind the scenes
        run: ...
```

---

## 5. Cloud Provider Integrations

### AWS IAM Federation

#### 1. AWS IAM Trust Policy
In AWS IAM, create an OIDC Identity Provider with URL `https://token.actions.githubusercontent.com` and Audience `sts.amazonaws.com`.
Then attach this **Trust Policy** to an IAM Role (`GitHubActionsDeployRole`):

```json
{
  "Version": "2012-10-17",
  "Statement": [
    {
      "Effect": "Allow",
      "Principal": {
        "Federated": "arn:aws:iam::123456789012:oidc-provider/token.actions.githubusercontent.com"
      },
      "Action": "sts:AssumeRoleWithWebIdentity",
      "Condition": {
        "StringEquals": {
          "token.actions.githubusercontent.com:aud": "sts.amazonaws.com"
        },
        "StringLike": {
          "token.actions.githubusercontent.com:sub": "repo:my-org/my-repo:ref:refs/heads/main"
        }
      }
    }
  ]
}
```

#### 2. Workflow Step (`aws-actions/configure-aws-credentials`)
```yaml
      - name: Configure AWS Credentials via OIDC
        uses: aws-actions/configure-aws-credentials@v4
        with:
          role-to-assume: arn:aws:iam::123456789012:role/GitHubActionsDeployRole
          aws-region: us-east-1
          role-session-name: GitHubActionsSession

      - name: Verify AWS Caller Identity
        run: aws sts get-caller-identity
```

---

### Google Cloud Workload Identity

#### 1. Workflow Step (`google-github-actions/auth`)
```yaml
      - name: Authenticate to Google Cloud via Workload Identity
        uses: google-github-actions/auth@v2
        with:
          project_id: 'my-gcp-project-id'
          workload_identity_provider: 'projects/123456789/locations/global/workloadIdentityPools/github-pool/providers/github-provider'
          service_account: 'my-service-account@my-gcp-project-id.iam.gserviceaccount.com'

      - name: Verify GCP Authentication
        run: gcloud auth list
```

---

### Microsoft Azure Workload Identity

#### 1. Workflow Step (`azure/login`)
```yaml
      - name: Log in to Azure via OIDC
        uses: azure/login@v2
        with:
          client-id: ${{ secrets.AZURE_CLIENT_ID }}
          tenant-id: ${{ secrets.AZURE_TENANT_ID }}
          subscription-id: ${{ secrets.AZURE_SUBSCRIPTION_ID }}

      - name: Verify Azure Login
        run: az account show
```

---

## 6. Best Practices & Trust Policy Hardening

1. **Never Use Wildcard Subjects (`*`) in Trust Policies**: Always pin the `sub` claim to an exact repository and branch:
   ```text
   # BAD: Allows ANY repo in your organization (or on GitHub!) to assume the role
   "token.actions.githubusercontent.com:sub": "*"

   # GOOD: Strict repository and main branch constraint
   "token.actions.githubusercontent.com:sub": "repo:my-org/backend-api:ref:refs/heads/main"
   ```
2. **Leverage Environment Claims for Production Roles**: Require jobs to run in protected GitHub Environments with required reviewers:
   ```json
   "token.actions.githubusercontent.com:sub": "repo:my-org/backend-api:environment:production"
   ```
3. **Set Short Role Session Durations**: Limit AWS STS role session duration to 900 seconds (15 minutes) or 3600 seconds (1 hour).
4. **Isolate Workflows by Cloud Role**: Have separate IAM roles for `dev`, `staging`, and `prod` rather than a single monolithic cloud admin role.
5. **Always Set `permissions: id-token: write` at the Job Level**: Avoid setting global write permissions if other jobs in the workflow do not need OIDC tokens.

---

## 7. Common Mistakes

| Mistake | Consequence | Root Cause | Fix / Prevention |
|:---|:---|:---|:---|
| Missing `permissions: id-token: write` | Action fails: `Unable to get OIDC token from runner` | Token minting permission disabled | Add `permissions: id-token: write` to job |
| Mismatched `aud` (Audience) claim | Cloud rejects handshake with `AccessDenied` / `InvalidToken` | Expected audience differs from provider | Set `sts.amazonaws.com` for AWS; verify provider config |
| Permissive wildcard `sub` claim | Any GitHub user can assume your cloud IAM role | `repo:*` or omitting `sub` check in IAM | Enforce `repo:owner/repo:ref:...` condition |
| Hardcoding static secrets alongside OIDC | Negates security benefit of OIDC | Unnecessary legacy secrets | Delete static AWS keys from GitHub Secrets |
| Missing `StringLike` vs `StringEquals` | IAM rejects valid token format | `StringEquals` used with wildcards | Use `StringLike` when pattern includes `*` |
| Pull request runs assuming prod role | Untrusted PR fork gains cloud access | Triggering OIDC on `pull_request` without branch checks | Restrict role assumption to `refs/heads/main` or environments |

---

## 8. Performance, Security & Cost

- **Latency**: OIDC exchange requires one additional network roundtrip (~200ms) to fetch the JWT and call STS `AssumeRoleWithWebIdentity`. This is negligible compared to overall build times.
- **Audit Logging**: CloudTrail (AWS), Cloud Audit Logs (GCP), and Azure Activity Logs record every `AssumeRoleWithWebIdentity` call, documenting the exact commit SHA, workflow run ID, actor, and repository that performed the cloud operation.
- **Cost**: GitHub Actions OIDC token generation is completely free. Cloud providers (AWS STS, GCP Workload Identity, Azure AD) do not charge for basic token federation calls.

---

## 9. Comparison: Static Keys vs OIDC Tokens

| Dimension | Static IAM Secret Keys | OpenID Connect (OIDC) |
|:---|:---|:---|
| **Lifespan** | Permanent (infinite until revoked) | **Short-lived** (15–60 minutes) |
| **Storage Location** | Stored in GitHub Secrets database | **Not stored anywhere** (minted dynamically) |
| **Leak Impact** | Catastrophic (attacker has unlimited access) | Minimal (token expires in minutes) |
| **Rotation Burden** | Manual or scheduled rotation scripts required | **Zero maintenance** (automatic) |
| **Contextual Scoping**| Key has fixed permissions | Scoped to exact repo, branch, or environment |
| **Compliance Auditing**| Hard to link actions to specific workflow runs | Direct correlation via CloudTrail / audit logs |

---

## 10. Interview Questions & Answers

<details>
<summary><strong>Q1: How does GitHub Actions OpenID Connect (OIDC) eliminate the need for hardcoded cloud credentials?</strong></summary>

Instead of storing long-lived cloud credentials (`AWS_ACCESS_KEY_ID`, `AWS_SECRET_ACCESS_KEY`, or GCP Service Account JSON keys) inside GitHub Secrets, GitHub Actions acts as an OIDC Identity Provider. When a job runs, the runner requests a cryptographically signed JSON Web Token (JWT) from GitHub's OIDC service. The runner sends this token to the cloud provider's STS (Security Token Service). The cloud provider verifies the signature against GitHub's public OIDC keys, evaluates the IAM Trust Policy claims (ensuring the token originated from the authorized repository and branch), and exchanges the JWT for short-lived, ephemeral cloud credentials.
</details>

<details>
<summary><strong>Q2: What is the purpose of the `sub` (Subject) claim in the GitHub OIDC token, and why is it critical for cloud security?</strong></summary>

The `sub` claim identifies the exact workload entity that requested the token. In GitHub Actions, it follows the format:
```text
repo:<owner>/<repository>:ref:<git-ref>
# or
repo:<owner>/<repository>:environment:<environment-name>
```
In the cloud provider's IAM Trust Policy, you specify a condition matching this claim. This guarantees that **only** workflows running from your specific repository and target branch (e.g. `repo:my-org/payment-service:ref:refs/heads/main`) can assume the role. Without a strict `sub` claim check, **any** GitHub user on the planet could present a valid GitHub OIDC token and assume your cloud role.
</details>

<details>
<summary><strong>Q3: Why must you explicitly configure `permissions: id-token: write` in a workflow?</strong></summary>

By default, GitHub Actions runners operate with minimal token privileges and cannot request an OIDC ID token. Granting `id-token: write` enables the runner's internal token broker service to generate a signed JWT with GitHub's OIDC provider. Without this permission, any action attempting OIDC authentication (such as `aws-actions/configure-aws-credentials` or `google-github-actions/auth`) will fail immediately.
</details>

<details>
<summary><strong>Q4: What happens if an untrusted fork opens a pull request against a repository that uses OIDC for deployment?</strong></summary>

When a pull request from a fork triggers a workflow, GitHub mints a JWT where the `repository` and `sub` claims reflect the fork's identity (e.g., `repo:attacker-org/my-repo-fork:ref:refs/pull/12/merge`).
Because the cloud IAM Role Trust Policy explicitly requires `repo:my-org/my-repo:ref:refs/heads/main`, the cloud provider detects that the `sub` claim does not match and rejects the `AssumeRoleWithWebIdentity` request with an Access Denied error.
</details>

<details>
<summary><strong>Q5: What is the `aud` (Audience) claim, and what values are typically expected by AWS and GCP?</strong></summary>

The `aud` claim indicates the intended recipient of the token to prevent token substitution attacks across services.
- For AWS, the expected audience is typically `sts.amazonaws.com`.
- For Google Cloud Workload Identity, the audience is usually the URL or resource name of the Workload Identity Pool provider (`https://iam.googleapis.com/...`).
- Custom audiences can be configured using `actions/core` or custom action parameters.
</details>

<details>
<summary><strong>Q6: How does OIDC improve compliance and auditability in SOC2 / ISO27001 environments?</strong></summary>

With static keys, CloudTrail logs only show that `service-user-ci` called an API. It is impossible to determine which specific pull request, commit, or developer triggered that action.
With OIDC, CloudTrail logs the full web identity token attributes, including the exact `repository`, `ref`, `workflow`, and `actor` (developer username). This provides an immutable audit trail tying cloud infrastructure changes directly to specific Git commits.
</details>

<details>
<summary><strong>Q7: Can OIDC credentials be used to access HashiCorp Vault from GitHub Actions? How?</strong></summary>

Yes. HashiCorp Vault has a native JWT/OIDC authentication engine. You configure Vault with GitHub's OIDC endpoint (`token.actions.githubusercontent.com`). The workflow uses the `hashicorp/vault-action` action to exchange the GitHub JWT for a short-lived Vault token, applying Vault policies based on the repository and branch claims.
</details>

<details>
<summary><strong>Q8: What is the recommended fallback if an external cloud service does not support OIDC federation?</strong></summary>

If a third-party API only supports static API keys:
1. Store the credential in GitHub Encrypted Secrets scoped to a protected GitHub Environment.
2. Require manual approval gates before jobs accessing that environment can execute.
3. Implement an automated credential rotation mechanism (e.g., using AWS Secrets Manager or Vault to periodically rotate the secret via GitHub REST API).
</details>

---

## 11. Practice Exercises

<details>
<summary><strong>Exercise 1: Inspect the Raw OIDC Token in a Workflow</strong></summary>

**Scenario**: Fetch and inspect the unencrypted payload of a GitHub OIDC token.
```yaml
name: Inspect OIDC
on: workflow_dispatch
permissions:
  id-token: write
jobs:
  inspect:
    runs-on: ubuntu-latest
    steps:
      - name: Fetch token and print claims
        run: |
          TOKEN=$(curl -s -H "Authorization: bearer $ACTIONS_ID_TOKEN_REQUEST_TOKEN" \
            "$ACTIONS_ID_TOKEN_REQUEST_URL&audience=sts.amazonaws.com" | jq -r .value)
          echo "JWT Header & Payload:"
          node -e "const parts = '$TOKEN'.split('.'); console.log(JSON.parse(Buffer.from(parts[1], 'base64').toString()));"
```
</details>

<details>
<summary><strong>Exercise 2: Write an AWS IAM Trust Policy for Main Branch Deployment</strong></summary>

**Scenario**: Create a JSON trust policy that strictly limits role assumption to `my-org/backend` on `main`.
```json
{
  "Version": "2012-10-17",
  "Statement": [
    {
      "Effect": "Allow",
      "Principal": {
        "Federated": "arn:aws:iam::123456789012:oidc-provider/token.actions.githubusercontent.com"
      },
      "Action": "sts:AssumeRoleWithWebIdentity",
      "Condition": {
        "StringEquals": {
          "token.actions.githubusercontent.com:aud": "sts.amazonaws.com",
          "token.actions.githubusercontent.com:sub": "repo:my-org/backend:ref:refs/heads/main"
        }
      }
    }
  ]
}
```
</details>

<details>
<summary><strong>Exercise 3: Write an AWS IAM Trust Policy for a Protected Environment</strong></summary>

**Scenario**: Create a trust policy that allows role assumption only when the job runs in the `production` environment.
```json
"Condition": {
  "StringEquals": {
    "token.actions.githubusercontent.com:aud": "sts.amazonaws.com",
    "token.actions.githubusercontent.com:sub": "repo:my-org/backend:environment:production"
  }
}
```
</details>

<details>
<summary><strong>Exercise 4: Configure AWS Authentication Step in Workflow</strong></summary>

**Scenario**: Use `aws-actions/configure-aws-credentials` to assume role via OIDC.
```yaml
- uses: aws-actions/configure-aws-credentials@v4
  with:
    role-to-assume: arn:aws:iam::112233445566:role/DeployRole
    aws-region: eu-west-1
```
</details>

<details>
<summary><strong>Exercise 5: Verify Least-Privilege Job-Level Permissions</strong></summary>

**Scenario**: Ensure only the deploy job has `id-token: write`.
```yaml
permissions:
  contents: read

jobs:
  test:
    runs-on: ubuntu-latest
    steps:
      - run: npm test

  deploy:
    needs: test
    permissions:
      id-token: write
      contents: read
    runs-on: ubuntu-latest
    steps:
      - run: echo "Assuming cloud role via OIDC..."
```
</details>

<details>
<summary><strong>Exercise 6: Query CloudTrail / Audit Logs for OIDC Sessions</strong></summary>

**Scenario**: Inspect CloudTrail events for `AssumeRoleWithWebIdentity` calls.
```bash
aws cloudtrail lookup-events --lookup-attributes AttributeKey=EventName,AttributeValue=AssumeRoleWithWebIdentity
```
</details>

---

## 12. Summary & Cheat Sheet

```yaml
# GitHub Actions OIDC Standard Workflow Pattern
name: Cloud Deploy
on:
  push:
    branches: [main]

permissions:
  contents: read
  id-token: write   # MANDATORY

jobs:
  deploy:
    runs-on: ubuntu-latest
    environment: production
    steps:
      - uses: actions/checkout@v4

      # AWS Authentication
      - uses: aws-actions/configure-aws-credentials@v4
        with:
          role-to-assume: arn:aws:iam::123456789012:role/ProductionDeployRole
          aws-region: us-east-1

      # GCP Authentication
      # - uses: google-github-actions/auth@v2
      #   with:
      #     workload_identity_provider: 'projects/.../providers/...'
      #     service_account: 'deployer@....iam.gserviceaccount.com'

      # Azure Authentication
      # - uses: azure/login@v2
      #   with:
      #     client-id: ${{ secrets.AZURE_CLIENT_ID }}
      #     tenant-id: ${{ secrets.AZURE_TENANT_ID }}
      #     subscription-id: ${{ secrets.AZURE_SUBSCRIPTION_ID }}

      - run: aws s3 sync dist/ s3://my-production-bucket
```

---

## 13. Further Reading
- [GitHub Docs - About Security Hardening with OpenID Connect](https://docs.github.com/en/actions/deployment/security-hardening-your-deployments/about-security-hardening-with-openid-connect)
- [AWS Docs - Creating OpenID Connect Identity Providers](https://docs.aws.amazon.com/IAM/latest/UserGuide/id_roles_providers_create_oidc.html)
- [Google Cloud - Workload Identity Federation with GitHub Actions](https://cloud.google.com/iam/docs/workload-identity-federation-with-other-providers)
- [04-github-actions/21-self-hosted-runners-and-scaling.md](file:///e:/Learning/engineering-notes/04-devops/git-github/04-github-actions/21-self-hosted-runners-and-scaling.md)
