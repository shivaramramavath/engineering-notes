---
title: "Least Privilege Access: Fine-Grained PATs, GITHUB_TOKEN & OIDC"
category: "04-devops"
subcategory: "git-github / 07-github-security"
tags: ["git", "github", "least-privilege", "tokens", "pats", "oidc", "iam", "github-token", "security"]
---

# Least Privilege Access: Fine-Grained PATs, GITHUB_TOKEN & OIDC

## Table of Contents
- [1. What & Why](#1-what--why)
- [2. Core Concepts](#2-core-concepts)
- [3. How It Works Internally](#3-how-it-works-internally)
- [4. Syntax & Usage](#4-syntax--usage)
- [5. Practical Examples](#5-practical-examples)
- [6. Best Practices](#6-best-practices)
- [7. Common Mistakes](#7-common-mistakes)
- [8. Performance, Security & Cost Considerations](#8-performance-security--cost-considerations)
- [9. Comparison Table](#9-comparison-table)
- [10. Real-World Interview Questions](#10-real-world-interview-questions)
- [11. Hands-On Exercises](#11-hands-on-exercises)
- [12. Summary / Cheat Sheet](#12-summary--cheat-sheet)
- [13. Further Reading](#13-further-reading)

---

## 1. What & Why

Credential over-privileging is among the leading causes of enterprise lateral movement and data exfiltration. Historically, developers and automation scripts relied on **Personal Access Tokens (Classic)**. Classic tokens grant sweeping, account-wide read/write permissions across every public and private repository the user can access, often without expiration dates. If an over-privileged classic token is leaked, an attacker inherits complete administrative control over dozens of repositories.

Applying the **Principle of Least Privilege (PoLP)** requires bounding credentials across three dimensions:
1. **Scope of Operations**: Only grant the precise permissions needed (e.g., `pull-requests: write`, but not `contents: write` or `admin: repo`).
2. **Resource Boundary**: Target only designated repositories, rather than granting account-wide access.
3. **Temporal Lifespan**: Restrict credentials to short lifespans (hours or minutes) or eliminate static secrets entirely using federated **OpenID Connect (OIDC)** authentication.

GitHub provides three modern primitives to implement strict least privilege:
- **Fine-Grained Personal Access Tokens (FG-PATs)**
- **Explicit `GITHUB_TOKEN` Permission Blocks**
- **Keyless OIDC Cloud Federation**

---

## 2. Core Concepts

### Classic PATs vs Fine-Grained PATs
- **Classic PATs (`ghp_...`)**: Coarse-grained scopes (e.g., `repo` grants read/write/delete access to code, issues, webhooks, and settings across all repositories). No organization approval controls.
- **Fine-Grained PATs (`github_pat_...`)**: Scoped to specific repositories or organizations; permissions are broken down into granular read/write flags (e.g., "Read access to issues", "Write access to deployments"). Organizations can mandate administrator approval before fine-grained PATs become active.

### Hardening `GITHUB_TOKEN` in GitHub Actions
Every GitHub Actions workflow receives an automatic `GITHUB_TOKEN` secret. By default, legacy repository configurations often grant this token permissive read/write access. Modern security practice dictates:
1. Setting the organization default workflow permission to **Read-only**.
2. Explicitly declaring a top-level or job-level `permissions:` block in every workflow, requesting only the specific scopes required (e.g., `contents: read`, `pull-requests: write`).

### GitHub Apps vs Machine Users
- **Machine Users**: Ordinary user accounts used for automation. They consume paid user seats, rely on static credentials, and violate enterprise identity governance.
- **GitHub Apps**: Dedicated, first-class programmatic identities. They do not consume seats, have granular repository permissions, maintain their own audit logs, and authenticate using short-lived installation access tokens generated via asymmetric RSA key pairs.

### OpenID Connect (OIDC) Cloud Authentication
Instead of storing permanent cloud provider credentials (like AWS `AWS_SECRET_ACCESS_KEY` or GCP service account keys) inside GitHub repository secrets, the runner exchanges an ephemeral JSON Web Token (JWT) directly with the cloud provider's Identity and Access Management (IAM) engine. The cloud provider validates the JWT claims (repository name, workflow ref, branch) and issues short-lived, temporary credentials valid for that single job.

---

## 3. How It Works Internally

```
GitHub Actions Runner                                  AWS IAM (or GCP / Azure)
+----------------------------------------+            +----------------------------------+
| 1. Workflow Step Requests Token        |            |                                  |
|    actions/aws-actions/configure-aws   |            |                                  |
+----------------------------------------+            |                                  |
                   |                                  |                                  |
                   v                                  |                                  |
+----------------------------------------+            |                                  |
| 2. GitHub Token Service Issues JWT     |            |                                  |
|    Claims:                             |            |                                  |
|    - sub: repo:org/api:ref:refs/heads  |            |                                  |
|    - iss: token.actions.githubusercontent|          |                                  |
|    - aud: sts.amazonaws.com            |            |                                  |
+----------------------------------------+            |                                  |
                   |                                  |                                  |
                   | 3. Send JWT to AWS STS AssumeRoleWithWebIdentity                            |
                   +=================================>+ 4. Validate GitHub OIDC Thumbprint|
                                                      | 5. Verify Trust Policy Condition:|
                                                      |    StringEquals "repo:org/api"   |
                                                      +----------------------------------+
                                                                       |
                                                                       v
+----------------------------------------+   Issue Short-Lived +----------------------------------+
| 7. Execute AWS CLI / Terraform Commands|<=================== | 6. Temporary IAM Credentials     |
|    (Valid for 15 - 60 minutes)         |   Assumed Role      |    (AccessKeyId, Secret, Session)|
+----------------------------------------+                     +----------------------------------+
```

---

## 4. Syntax & Usage

### Restricting `GITHUB_TOKEN` Permissions in Workflow YAML
Always declare minimal permissions at the job or workflow root:

```yaml
# Strict baseline: deny all permissions by default
permissions: {}

jobs:
  triage:
    runs-on: ubuntu-latest
    permissions:
      issues: write        # Only grant permission to label/comment on issues
      contents: read       # Read repository source code
    steps:
      - uses: actions/checkout@v4
      - run: gh issue comment 42 --body "Triaged"
        env:
          GITHUB_TOKEN: ${{ secrets.GITHUB_TOKEN }}
```

Available granular scopes for `GITHUB_TOKEN`:
- `actions: read|write`
- `attestations: read|write`
- `checks: read|write`
- `contents: read|write`
- `deployments: read|write`
- `id-token: write` (Required for OIDC)
- `issues: read|write`
- `packages: read|write`
- `pull-requests: read|write`
- `security-events: read|write`

---

## 5. Practical Examples

### Example 1: AWS OIDC Keyless Cloud Authentication
Eliminate static AWS keys by assuming an IAM Role via GitHub's OIDC provider:
```yaml
name: "Deploy to AWS"
on:
  push:
    branches: ["main"]

permissions:
  id-token: write  # Mandatory for requesting the OIDC JWT
  contents: read

jobs:
  deploy:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v4

      - name: Configure AWS Credentials via OIDC
        uses: aws-actions/configure-aws-credentials@v4
        with:
          role-to-assume: "arn:aws:iam::123456789012:role/GitHubActionsDeploymentRole"
          aws-region: "us-east-1"
          role-session-name: "GitHubActions-ProductionDeploy"

      - name: Sync S3 Bucket
        run: |
          aws s3 sync ./dist s3://my-production-static-assets/
```

AWS IAM Trust Policy matching the workflow:
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
          "token.actions.githubusercontent.com:sub": "repo:my-org/payment-service:ref:refs/heads/main"
        }
      }
    }
  ]
}
```

### Example 2: GCP Workload Identity Federation
Authenticate to Google Cloud without service account JSON keys:
```yaml
      - id: "auth"
        name: "Authenticate to Google Cloud"
        uses: "google-github-actions/auth@v2"
        with:
          workload_identity_provider: "projects/123456789/locations/global/workloadIdentityPools/github-pool/providers/github-provider"
          service_account: "deployer@my-gcp-project.iam.gserviceaccount.com"
```

### Example 3: Generating GitHub App Installation Tokens
In workflows needing cross-repository access, use a GitHub App rather than a user PAT:
```yaml
      - name: Generate GitHub App Token
        id: app-token
        uses: actions/create-github-app-token@v1
        with:
          app-id: ${{ vars.AUTOMATION_APP_ID }}
          private-key: ${{ secrets.AUTOMATION_APP_PRIVATE_KEY }}
          owner: ${{ github.repository_owner }}
          repositories: "shared-infra,payment-service"

      - name: Checkout Private Infrastructure Repo
        uses: actions/checkout@v4
        with:
          repository: "my-org/shared-infra"
          token: ${{ steps.app-token.outputs.token }}
```

### Example 4: Auditing Expired and Over-Privileged PATs via API
```bash
# List fine-grained personal access tokens pending admin approval in an org
gh api -H "Accept: application/vnd.github+json" \
  /orgs/my-org/personal-access-token-requests \
  --jq '.[] | {requester: .owner.login, repo_selection: .repository_selection, permissions: .permissions}'
```

### Example 5: Revoking Compromised PAT via GitHub CLI
```bash
# Revoke a fine-grained PAT request
gh api --method POST -H "Accept: application/vnd.github+json" \
  /orgs/my-org/personal-access-tokens/investigate/12345/revoke
```

---

## 6. Best Practices

- **Set Organization-Wide Read-Only Default**: In GitHub Enterprise / Organization settings, set "Workflow permissions" to "Read repository contents permission".
- **Zero Static Cloud Secrets**: Completely eliminate long-lived cloud credentials (`AWS_ACCESS_KEY_ID`, service account JSON files) in favor of OIDC federated identities.
- **Mandate Maximum PAT Lifespans**: Enforce enterprise policies limiting PAT expiration dates to 30, 60, or 90 days maximum.
- **Use GitHub Apps for Multi-Repo Automation**: When a CI workflow needs to trigger workflows or read code from another private repo, install a GitHub App scoped only to those specific repos.
- **Restrict OIDC Sub Claims**: In cloud trust policies, never use wildcard `sub: repo:my-org/*` without branch restrictions. Restrict deployments strictly to `repo:my-org/repo:ref:refs/heads/main` or release tags.

---

## 7. Common Mistakes

| Mistake | Impact | Correct Approach |
|:---|:---|:---|
| Using Classic PATs for CI/CD automation | Leaked token grants attacker full access across all personal/org repos | Use Fine-Grained PATs or GitHub App tokens |
| Storing AWS IAM user access keys in repository secrets | Keys never expire; if leaked, attacker has continuous cloud access | Use OIDC with short-lived STS credentials |
| Wildcarding OIDC subject claim (`repo:my-org/*:*`) | Any pull request from any repo in the organization can assume cloud production role | Bind trust policy strictly to repository and production branch |
| Forgetting `id-token: write` permission in OIDC workflows | Workflow fails with "Unable to get ACTIONS_ID_TOKEN_REQUEST_URL" | Explicitly add `id-token: write` to the job's `permissions:` block |
| Relying on machine user accounts with password logins | Violates SSO policies, consumes paid licenses, difficult to audit | Replace machine users with GitHub Apps |
| Granting `contents: write` when only `issues: write` is needed | Compromised action or dependency can push malicious commits to repo | Declare granular least-privilege permissions per job |

---

## 8. Performance, Security & Cost Considerations

- **Performance**: OIDC token exchange requires a single HTTPS call to the cloud provider's STS endpoint, adding < 1 second to workflow initiation.
- **Security**: OIDC eliminates credential rotation debt. Even if an attacker dumps the environment variables of a runner job, the STS credentials expire within minutes.
- **Cost**: Fine-Grained PATs, `GITHUB_TOKEN` controls, and OIDC federation are free on all GitHub plans. GitHub Apps do not consume paid user seats, reducing organization licensing costs.

---

## 9. Comparison Table

| Mechanism | Scope Granularity | Target Boundary | Expiration | Key Benefit |
|:---|:---|:---|:---|:---|
| **Classic PAT** | Coarse (Account-wide) | All accessible repos | Optional (Never) | Legacy compatibility only |
| **Fine-Grained PAT**| Granular (Specific APIs) | Specific repositories | Mandatory (Max 1 yr)| User personal access control |
| **GITHUB_TOKEN** | Granular (`permissions:`) | Single repository | Ephemeral (Job duration)| Automatic, native to Actions |
| **GitHub App Token**| Granular (Specific APIs) | Selected repositories | Ephemeral (1 hour) | Multi-repo CI/CD identity |
| **OIDC Federation** | Cloud IAM Policies | Bound to repo + ref | Ephemeral (15–60 min)| Zero static cloud secrets |

---

## 10. Real-World Interview Questions

<details>
<summary>1. What are the security risks of using Classic Personal Access Tokens (PATs) across an organization?</summary>
Classic PATs suffer from coarse scopes (e.g., `repo` grants full write access to all code, releases, and settings), lack repository-level isolation (a leak compromises every repository the user can access), have no mandatory expiration, and cannot be centrally governed or approved by organization administrators.
</details>

<details>
<summary>2. How does GitHub Actions OpenID Connect (OIDC) eliminate static cloud secrets?</summary>
OIDC uses short-lived cryptographic trust. The GitHub Actions runner requests a signed JWT from GitHub containing claims about the repo, ref, and workflow. The runner presents this JWT to the cloud provider (AWS STS, GCP, Azure), which verifies GitHub's public cryptographic keys and assumes an IAM role, returning short-lived credentials (valid for ~1 hour). No static secret is ever stored in GitHub repository settings.
</details>

<details>
<summary>3. Why must `id-token: write` be explicitly declared when using OIDC in GitHub Actions?</summary>
By default, GitHub Actions workflows run with restrictive permissions. The token service will only mint and provide an OIDC identity token if the workflow explicitly declares `id-token: write` in its `permissions:` block. This prevents unauthorized steps or third-party actions from requesting cloud identity tokens without maintainer consent.
</details>

<details>
<summary>4. What is the danger of setting an AWS IAM trust policy condition to `StringLike "repo:my-org/*:*"`?</summary>
This condition allows any repository in `my-org` on any branch or pull request to assume the AWS IAM role. A junior developer or an unprivileged internal project could trigger a workflow and gain full production AWS permissions. Trust policies should strictly enforce both repository name and target branch (`repo:my-org/prod-service:ref:refs/heads/main`).
</details>

<details>
<summary>5. Why are GitHub Apps preferred over "machine users" (service accounts) for automation?</summary>
Machine users are standard user accounts that consume paid enterprise licenses, require manual credential management (PATs or SSH keys), and introduce security vulnerabilities if not linked to SSO. GitHub Apps are first-class platform entities that do not consume seats, have precise repository and permission boundaries, and generate ephemeral 1-hour access tokens dynamically.
</details>

<details>
<summary>6. What happens if a workflow defines no `permissions:` block and runs with default settings?</summary>
The workflow inherits the repository or organization's default workflow permissions. If the organization default is set to "Read and write permissions" (legacy default), the `GITHUB_TOKEN` possesses full write access to code, releases, packages, and PRs, creating severe blast radius risk if a third-party dependency is compromised.
</details>

<details>
<summary>7. How can an organization prevent developers from using Classic PATs?</summary>
Organization owners can navigate to **Organization Settings** -> **Personal Access Tokens** -> **Settings**, and restrict or completely disable access via personal access tokens (classic), while mandating administrative approval for any fine-grained personal access tokens.
</details>

<details>
<summary>8. How do you pass a GitHub App token to `actions/checkout` to clone a second private repository?</summary>
Generate the token using `actions/create-github-app-token`, then pass the resulting token output into the `token:` input parameter of `actions/checkout@v4`.
</details>

---

## 11. Hands-On Exercises

<details>
<summary>Exercise 1: Configure explicit minimal permissions in a workflow</summary>

**Goal:** Author a workflow with zero base permissions granting only read access to code.

```yaml
name: Lint Code
on: [push]
permissions:
  contents: read
jobs:
  lint:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v4
      - run: npm run lint
```
</details>

<details>
<summary>Exercise 2: Create an AWS IAM Trust Policy for GitHub OIDC</summary>

**Goal:** Write a policy binding an AWS IAM role to the `main` branch of `acme-corp/api-gateway`.

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
          "token.actions.githubusercontent.com:sub": "repo:acme-corp/api-gateway:ref:refs/heads/main"
        }
      }
    }
  ]
}
```
</details>

<details>
<summary>Exercise 3: Test OIDC authentication step in Actions</summary>

**Goal:** Use `aws-actions/configure-aws-credentials` to assume a role.

```yaml
- name: Authenticate via OIDC
  uses: aws-actions/configure-aws-credentials@v4
  with:
    role-to-assume: arn:aws:iam::123456789012:role/DeployRole
    aws-region: us-east-1
```
</details>

<details>
<summary>Exercise 4: Inspect organization PAT policy</summary>

**Goal:** Check organization settings for personal access tokens.

```bash
gh api /orgs/:owner --jq '.default_repository_permission'
```
</details>

<details>
<summary>Exercise 5: Generate an ephemeral GitHub App token</summary>

**Goal:** Use `actions/create-github-app-token` in a workflow.

```yaml
- uses: actions/create-github-app-token@v1
  id: app-token
  with:
    app-id: ${{ vars.APP_ID }}
    private-key: ${{ secrets.APP_PRIVATE_KEY }}
```
</details>

<details>
<summary>Exercise 6: Query active Fine-Grained PAT requests</summary>

**Goal:** List pending PAT approval requests in your organization.

```bash
gh api /orgs/:owner/personal-access-token-requests
```
</details>

---

## 12. Summary / Cheat Sheet

```text
================================================================================
                    LEAST PRIVILEGE ACCESS & OIDC
================================================================================
Classic PATs       : ghp_... | Coarse | Unscoped | Avoid completely
Fine-Grained PATs  : github_pat_... | Granular APIs | Scoped repos | Max 1 yr
GITHUB_TOKEN       : Native to Actions | Always declare permissions: block
Default Setting    : Org Settings -> Actions -> Workflow permissions: Read-only
OIDC Federation    : Keyless cloud auth -> Exchanges JWT for temporary STS credentials
OIDC Permissions   : id-token: write (mandatory)
Automation Identity: Use GitHub Apps instead of Machine Users (0 seat cost, ephemeral tokens)
Trust Policy Rule  : Always pin OIDC 'sub' claim to exact repo AND target branch
================================================================================
```

---

## 13. Further Reading
- [GitHub Docs: Automatic token authentication](https://docs.github.com/en/actions/security-guides/automatic-token-authentication)
- [GitHub Docs: About security hardening with OpenID Connect](https://docs.github.com/en/actions/deployment/security-hardening-your-deployments/about-security-hardening-with-openid-connect)
- [GitHub Docs: Managing your personal access tokens](https://docs.github.com/en/authentication/keeping-your-account-and-data-secure/managing-your-personal-access-tokens)
- [AWS Security Blog: Use IAM roles to connect GitHub Actions to AWS](https://aws.amazon.com/blogs/security/use-iam-roles-to-connect-github-actions-to-actions-in-aws/)

---

[<- Previous: Software Supply Chain & SBOM](./06-sbom-and-supply-chain-security.md) | [Module 07 Overview](./README.md) | [Next: Leaked Secret Response Runbook ->](./08-leaked-secret-response-runbook.md)
