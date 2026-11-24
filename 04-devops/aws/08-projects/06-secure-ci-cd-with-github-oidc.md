---
project: "Secure CI/CD Deployment with GitHub Actions and OIDC"
module: "08-projects"
track: "aws"
difficulty: "intermediate"
architecture: "zero-static-key-cicd"
last-tested: "2026-11"
tags:
  - "iam"
  - "oidc"
  - "github-actions"
  - "security"
  - "ci-cd"
  - "terraform"
  - "project"
---

# Secure CI/CD Deployment with GitHub Actions and OIDC

## Table of Contents
- [1. Project Topology & Specifications](#1-project-topology--specifications)
- [2. Declarative Code (Terraform)](#2-declarative-code-terraform)
- [3. Verification & Automated Test Harness](#3-verification--automated-test-harness)
- [4. Hardening, Security & Cost Analysis](#4-hardening-security--cost-analysis)
- [5. Operational Runbook & Disaster Recovery](#5-operational-runbook--disaster-recovery)

---

## 1. Project Topology & Specifications

### 1.1 Architecture Overview
Historically, deploying to AWS from third-party CI/CD systems (such as GitHub Actions) required generating long-lived IAM Access Keys (`AKIA...`) and storing them as repository secrets. This pattern is a major security vulnerability: credentials can be leaked in job logs, committed inadvertently, or exfiltrated by malicious workflow pull requests.

This production reference project implements **Zero Static Key CI/CD** using **OpenID Connect (OIDC) Identity Federation**:
1. When a GitHub Actions workflow executes, the runner requests a short-lived JSON Web Token (JWT) signed by GitHub's OIDC Identity Provider (`token.actions.githubusercontent.com`).
2. The GitHub Action presents this token to **AWS Security Token Service (STS)** via the `AssumeRoleWithWebIdentity` API call.
3. AWS STS validates the token signature, audience, and subject claims (`sub: repo:owner/repo:ref:refs/heads/main`).
4. If valid, AWS STS issues temporary, scoped IAM credentials valid for only 1 hour.
5. **Zero static secrets** exist in GitHub repository settings.

```
+----------------------------------------------------------------------------------------------------+
|                         GITHUB ACTIONS OIDC IDENTITY FEDERATION FLOW                               |
+----------------------------------------------------------------------------------------------------+

  [ GitHub Actions Workflow Runner ]
             |
             | 1. Request OIDC Token (audience: "sts.amazonaws.com")
             v
  +--------------------------------------------------------------------------------------------------+
  | GitHub OIDC Token Service (token.actions.githubusercontent.com)                                  |
  |  - Issues RS256-signed JWT containing claims:                                                    |
  |    sub: "repo:my-org/my-app:ref:refs/heads/main"                                                 |
  |    aud: "sts.amazonaws.com"                                                                      |
  +--------------------------------------------------------------------------------------------------+
             |
             | 2. AssumeRoleWithWebIdentity (Presents OIDC JWT)
             v
  +--------------------------------------------------------------------------------------------------+
  | AWS Security Token Service (STS) & IAM OIDC Provider                                             |
  |  - Evaluates Trust Policy Conditions:                                                            |
  |    1. aud MUST equal "sts.amazonaws.com"                                                         |
  |    2. sub MUST match "repo:my-org/my-app:ref:refs/heads/main"                                    |
  +--------------------------------------------------------------------------------------------------+
             |
             | 3. Returns Ephemeral STS Credentials (AccessKeyId, SecretAccessKey, SessionToken - 1 hour)
             v
  [ GitHub Actions Runner executes `aws s3 sync` or `terraform apply` securely ]
```

### 1.2 Bill of Materials
- `aws_iam_openid_connect_provider`: Connects AWS IAM to GitHub's OIDC IdP using official thumbprints.
- `aws_iam_role`: Scoped CI/CD deployment role with an OIDC trust policy.
- `aws_iam_policy`: Least-privilege permissions granting only deployment capabilities (e.g., S3 deployment bucket access).
- Sample `.github/workflows/deploy.yml`: Production GitHub Actions workflow.

---

## 2. Declarative Code (Terraform)

Create `main.tf`:

```hcl
terraform {
  required_version = ">= 1.5.0"
  required_providers {
    aws = {
      source  = "hashicorp/aws"
      version = "~> 5.40"
    }
  }
}

provider "aws" {
  region = "us-east-1"
}

# Change to your actual GitHub Organization and Repository name
variable "github_org" {
  type        = string
  description = "GitHub Organization or Username"
  default     = "enterprise-org"
}

variable "github_repo" {
  type        = string
  description = "GitHub Repository Name"
  default     = "core-application"
}

variable "target_branch" {
  type        = string
  description = "Target branch permitted to assume production role"
  default     = "main"
}

# -----------------------------------------------------------------------------
# 1. IAM OpenID Connect Provider for GitHub Actions
# -----------------------------------------------------------------------------
# GitHub's well-known OIDC certificate thumbprints
resource "aws_iam_openid_connect_provider" "github" {
  url             = "https://token.actions.githubusercontent.com"
  client_id_list  = ["sts.amazonaws.com"]
  thumbprint_list = [
    "6938fd4d98bab03faadb97b34396831e3780aea1",
    "1c58a3a8518e8759bf075b76b750d4f8d264fcd9"
  ]
}

# -----------------------------------------------------------------------------
# 2. Target S3 Deployment Bucket (Example Workload)
# -----------------------------------------------------------------------------
resource "aws_s3_bucket" "release_artifacts" {
  bucket_prefix = "github-actions-release-artifacts-"
}

# -----------------------------------------------------------------------------
# 3. Scoped IAM Deployment Role
# -----------------------------------------------------------------------------
resource "aws_iam_role" "github_deploy" {
  name = "github-actions-deploy-role"

  # Trust Policy: Permit ONLY the specific repository and branch to assume this role
  assume_role_policy = jsonencode({
    Version = "2012-10-17"
    Statement = [
      {
        Effect = "Allow"
        Principal = {
          Federated = aws_iam_openid_connect_provider.github.arn
        }
        Action = "sts:AssumeRoleWithWebIdentity"
        Condition = {
          StringEquals = {
            "token.actions.githubusercontent.com:aud" = "sts.amazonaws.com"
          }
          StringLike = {
            # Restricts role assumption strictly to main branch pushes on this specific repo
            "token.actions.githubusercontent.com:sub" = "repo:${var.github_org}/${var.github_repo}:ref:refs/heads/${var.target_branch}"
          }
        }
      }
    ]
  })
}

# Least Privilege Permissions Policy
resource "aws_iam_policy" "deploy_permissions" {
  name = "github-actions-deploy-policy"

  policy = jsonencode({
    Version = "2012-10-17"
    Statement = [
      {
        Effect = "Allow"
        Action = [
          "s3:PutObject",
          "s3:GetObject",
          "s3:ListBucket"
        ]
        Resource = [
          aws_s3_bucket.release_artifacts.arn,
          "${aws_s3_bucket.release_artifacts.arn}/*"
        ]
      }
    ]
  })
}

resource "aws_iam_role_policy_attachment" "deploy_attach" {
  role       = aws_iam_role.github_deploy.name
  policy_arn = aws_iam_policy.deploy_permissions.arn
}

output "role_arn_to_assume" {
  value       = aws_iam_role.github_deploy.arn
  description = "Copy this ARN into your GitHub Actions workflow file"
}

output "s3_bucket_name" {
  value = aws_s3_bucket.release_artifacts.id
}
```

---

## 3. Verification & Automated Test Harness

### 3.1 Deployment Instructions

```bash
terraform init
terraform apply -auto-approve
ROLE_ARN=$(terraform output -raw role_arn_to_assume)
BUCKET_NAME=$(terraform output -raw s3_bucket_name)

echo "Deployment Role ARN: $ROLE_ARN"
```

### 3.2 GitHub Actions Workflow File (`.github/workflows/deploy.yml`)

Create the workflow file inside your GitHub repository:

```yaml
name: Production Continuous Deployment

on:
  push:
    branches:
      - main

permissions:
  id-token: write   # MANDATORY: Required for requesting the OIDC JWT
  contents: read    # Required for checking out repository code

jobs:
  deploy:
    name: Build & Deploy to AWS
    runs-on: ubuntu-latest

    steps:
      - name: Checkout Code
        uses: actions/checkout@v4

      - name: Configure AWS Credentials via OIDC
        uses: aws-actions/configure-aws-credentials@v4
        with:
          role-to-assume: ${{ secrets.AWS_DEPLOY_ROLE_ARN }}
          aws-region: us-east-1
          audience: sts.amazonaws.com

      - name: Verify Identity and Caller ARN
        run: |
          echo "Verifying assumed AWS identity..."
          aws sts get-caller-identity

      - name: Deploy Artifacts to S3
        run: |
          echo "Packaging build artifacts..."
          echo "Build Commit: $GITHUB_SHA" > build_metadata.txt
          aws s3 cp build_metadata.txt s3://${{ secrets.AWS_BUCKET_NAME }}/releases/$GITHUB_SHA.txt
          echo "Deployment completed successfully!"
```

### 3.3 Verification Steps
1. In your GitHub repository, configure two Repository Secrets (or Environment Secrets):
   - `AWS_DEPLOY_ROLE_ARN`: Value of `terraform output -raw role_arn_to_assume`.
   - `AWS_BUCKET_NAME`: Value of `terraform output -raw s3_bucket_name`.
2. Commit and push a change to the `main` branch.
3. Inspect the GitHub Actions workflow run:
   - Notice the step `Configure AWS Credentials via OIDC` succeeds without any static access keys.
   - The `aws sts get-caller-identity` step prints:
     `"Arn": "arn:aws:sts::123456789012:assumed-role/github-actions-deploy-role/GitHubJob"`

### 3.4 Teardown Script

```bash
terraform destroy -auto-approve
```

---

## 4. Hardening, Security & Cost Analysis

### 4.1 Security Analysis & Attack Vector Mitigations
- **Zero Static Credentials**: There are no `AWS_ACCESS_KEY_ID` or `AWS_SECRET_ACCESS_KEY` credentials in GitHub repository settings, eliminating secrets sprawl and rotation overhead.
- **Strict Subject (`sub`) Claim Pinning**:
  - If the trust condition were `StringLike: { "token.actions.githubusercontent.com:sub": "*" }`, any developer with a GitHub account could assume your AWS role by running a workflow from their own repository.
  - Pinning `repo:org/repo:ref:refs/heads/main` guarantees that only workflows executed on the specific repository's `main` branch can assume the role.
- **Pull Request Protection**: Pull requests originating from fork repositories cannot access the production role, mitigating malicious PR injection attacks.

### 4.2 Cost Breakdown
- **AWS IAM**: Free service.
- **AWS STS**: Free service (assume role calls via OIDC are free).
- **Total Cost**: **\$0.00 / month**.

---

## 5. Operational Runbook & Disaster Recovery

### Common Operational Issues

#### Issue 1: GitHub Action Fails with `Error: Credentials could not be loaded, please check your action inputs`
- **Root Cause**: The GitHub Actions job is missing the `id-token: write` permission.
- **Remediation**: Ensure your workflow defines top-level or job-level permissions:
  ```yaml
  permissions:
    id-token: write
    contents: read
  ```

#### Issue 2: AWS STS Rejects Request with `NotAuthorized: WebIdentityErr`
- **Root Cause**: The `sub` claim emitted by GitHub does not match the IAM trust condition. (e.g., Workflow ran on a pull request `refs/pull/1/merge` or a feature branch `refs/heads/feature-1`, but the trust policy restricts to `refs/heads/main`).
- **Remediation**: Adjust the condition in Terraform to allow tag releases (`refs/tags/*`) or create separate IAM roles for staging and production environments.
