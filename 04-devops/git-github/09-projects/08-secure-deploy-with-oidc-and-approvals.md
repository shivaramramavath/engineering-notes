<!--
================================================================================
Metadata:
  Title: Reference Project 08 - Zero-Secret Cloud Deployment with OIDC & Protection Rules
  Track: Git & GitHub / DevOps
  Difficulty: Advanced
  Estimated Time: 60 minutes
  Prerequisites: 04-github-actions/20-oidc-and-cloud-auth.md, 07-github-security/07-least-privilege-tokens-and-permissions.md
  Key Terminology: OpenID Connect (OIDC), AWS STS, AssumeRoleWithWebIdentity, GitHub Environments, Required Reviewers, Deployment Protection Rules, Automated Rollback
================================================================================
-->

# Reference Project 08: Zero-Secret Cloud Deployment with OIDC & Protection Rules

[<- Back to Projects Overview](./README.md) | [Back to Projects Directory](./README.md)

---

## 1. Overview & Architecture

Storing static cloud provider access keys (`AWS_ACCESS_KEY_ID`, `AWS_SECRET_ACCESS_KEY`) in repository secrets is a major security risk. If a repository secret is leaked or accessed by an unvetted third-party action, the credentials can remain compromised indefinitely until manual rotation. Furthermore, production deployments should never proceed completely automatically without mandatory review gates, environment isolation, and immediate rollback procedures.

This reference project implements an enterprise continuous deployment (CD) architecture featuring:
1. **Zero Static Cloud Secrets**: Authenticates directly to AWS IAM using **OpenID Connect (OIDC)** and STS ephemeral session tokens.
2. **GitHub Environments & Manual Approvals**: Deploys to a protected `production` environment that pauses execution until designated senior engineers submit an approving review in the GitHub UI.
3. **Deployment Concurrency Gates**: Prevents race conditions by locking production deployments to a single active runner (`concurrency: production`).
4. **Automated Health Check & Instant Rollback**: Evaluates live system health metrics post-deploy and automatically triggers an automated rollback if the new release errors out.

### CD Architecture Diagram

```
[Git Push: Tag v* or Merge to main]
                 |
                 v
+-------------------------------------------------------------------------+
| GitHub Actions Runner (deploy.yml)                                      |
| 1. Requests OIDC JWT from GitHub Token Service (id-token: write)        |
+-------------------------------------------------------------------------+
                 |
                 v
+-------------------------------------------------------------------------+
| AWS STS AssumeRoleWithWebIdentity                                       |
| - Verifies JWT against GitHub OIDC Identity Provider                     |
| - Asserts Condition: StringEquals "repo:my-org/api:environment:prod"  |
| - Issues Temporary Ephemeral Credentials (valid 15 minutes)             |
+-------------------------------------------------------------------------+
                 |
                 v
+-------------------------------------------------------------------------+
| GitHub Environment Protection Gate ("production")                       |
| - Execution PAUSED                                                      |
| - Email / Slack notification sent to Designated Security Reviewers      |
| - Requires 1 Senior Engineer Approval before proceeding                 |
+-------------------------------------------------------------------------+
                 |
                 | (Approved in GitHub Web UI)
                 v
+-------------------------------------------------------------------------+
| Execute Deployment to Cloud Target (ECS / S3 / Lambda / EKS)            |
| - Deploys new revision                                                  |
+-------------------------------------------------------------------------+
                 |
                 v
+-------------------------------------------------------------------------+
| Post-Deployment Smoke Test & Health Check                               |
| - Queries GET /healthz                                                  |
| - PASS -> Mark Deployment Complete (Success)                            |
| - FAIL -> Execute Automated Rollback to Previous Revision               |
+-------------------------------------------------------------------------+
```

---

## 2. Prerequisites & Setup

### Environment Requirements
- **Git**: Version 2.25+
- **Cloud Provider**: AWS Account with IAM permissions to configure OIDC identity providers
- **GitHub Plan**: GitHub Free/Team/Enterprise (GitHub Environments are available in public repos for free, and in private repos on GitHub Enterprise or Team plans)

### Repository Directory Hierarchy
```text
secure-deploy-project/
├── .github/
│   └── workflows/
│       └── deploy.yml          # Secure deployment pipeline
├── scripts/
│   └── rollback.sh             # Automated rollback script
└── index.html                  # Sample web application asset
```

---

## 3. Step-by-Step Implementation Walkthrough

### Step 1: Configure AWS IAM OIDC Trust Policy
In the AWS IAM console (or via Terraform), configure GitHub as an OpenID Connect Identity Provider:
- **Provider URL**: `https://token.actions.githubusercontent.com`
- **Audience**: `sts.amazonaws.com`

Author the IAM Role Trust Policy (`trust-policy.json`):
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
          "token.actions.githubusercontent.com:sub": "repo:my-org/secure-deploy-project:environment:production"
        }
      }
    }
  ]
}
```

### Step 2: Configure GitHub Environment Protection Rules
In your GitHub repository:
1. Navigate to **Settings** -> **Environments** -> Click **"New environment"**.
2. Name the environment: `production`.
3. Under **Deployment protection rules**, check **Required reviewers**.
4. Add senior engineering leads or DevOps team members.
5. Under **Deployment branches**, select **Selected branches** -> specify `main` and `refs/tags/v*`.

### Step 3: Author Automated Rollback Script (`scripts/rollback.sh`)
```bash
mkdir -p scripts
cat << 'EOF' > scripts/rollback.sh
#!/usr/bin/env bash
set -euo pipefail

TARGET_BUCKET="$1"
PREVIOUS_VERSION="$2"

echo "🚨 [ROLLBACK INITIATED]: Reverting production deployment..."
echo "Restoring previous stable version: $PREVIOUS_VERSION"

# Example: Restore previous bundle from versioned backup prefix
aws s3 sync "s3://${TARGET_BUCKET}/releases/${PREVIOUS_VERSION}/" "s3://${TARGET_BUCKET}/live/" --delete

echo "✅ [ROLLBACK COMPLETED]: Production restored to $PREVIOUS_VERSION"
EOF
chmod +x scripts/rollback.sh
```

### Step 4: Author Secure Deployment Workflow (`.github/workflows/deploy.yml`)

```yaml
mkdir -p .github/workflows
cat << 'EOF' > .github/workflows/deploy.yml
name: "Secure Production Deployment"

on:
  push:
    tags: ["v*"]

# Ensure only one deployment executes at a time
concurrency:
  group: production-deploy
  cancel-in-progress: false

permissions:
  id-token: write  # MANDATORY for OIDC token minting
  contents: read
  deployments: write

jobs:
  deploy-production:
    name: "Deploy to AWS Production"
    runs-on: ubuntu-latest
    environment:
      name: production
      url: https://app.example.com

    steps:
      - name: "Checkout Release Code"
        uses: actions/checkout@v4

      - name: "Authenticate to AWS via OIDC (Zero Secrets)"
        uses: aws-actions/configure-aws-credentials@v4
        with:
          role-to-assume: "arn:aws:iam::123456789012:role/GitHubActionsProductionDeployer"
          aws-region: "us-east-1"
          role-session-name: "GHA-Deploy-${{ github.sha }}"

      - name: "Record Previous Stable Version"
        id: prev
        run: |
          PREV_TAG=$(git describe --tags --abbrev=0 HEAD^ 2>/dev/null || echo "v0.0.0")
          echo "PREVIOUS_TAG=$PREV_TAG" >> $GITHUB_OUTPUT

      - name: "Execute Deployment"
        run: |
          echo "Deploying release ${{ github.ref_name }} to production..."
          # Deploy assets to S3
          aws s3 sync ./dist "s3://my-prod-app-assets/releases/${{ github.ref_name }}/"
          aws s3 sync ./dist "s3://my-prod-app-assets/live/" --delete
          echo "Deployment sync complete."

      - name: "Execute Post-Deploy Smoke Test"
        id: smoke-test
        run: |
          echo "Performing synthetic health probes against live endpoint..."
          STATUS=$(curl -s -o /dev/null -w "%{http_code}" https://app.example.com/healthz || echo "500")
          if [ "$STATUS" -ne 200 ]; then
            echo "Smoke test FAILED! Received HTTP status $STATUS"
            exit 1
          fi
          echo "Smoke test PASSED! Production is healthy."

      - name: "Automated Rollback on Failure"
        if: failure() && steps.smoke-test.outcome == 'failure'
        run: |
          ./scripts/rollback.sh "my-prod-app-assets" "${{ steps.prev.outputs.PREVIOUS_TAG }}"
EOF
```

---

## 4. Verification & Testing

### Step 1: Create a Release Tag and Push
```bash
git add .
git commit -m "feat(cd): configure OIDC deployment with approval gate"
git tag v2.1.0
git push origin main --tags
```

### Step 2: Observe Execution Pause in GitHub Actions
1. Open the GitHub Actions tab.
2. The workflow job `Deploy to AWS Production` is in status **"Waiting for review"**.
3. Approver clicks **"Review deployments"**, reviews the commit diff and target environment, checks `production`, and clicks **"Approve and deploy"**.
4. Execution resumes:
   - Ephemeral OIDC credentials are exchanged within 1.2 seconds.
   - S3 sync completes successfully.
   - Post-deploy smoke test validates HTTP 200 status.
   - Deployment environment status is marked as `Active`.

---

## 5. Production Readiness & Troubleshooting

### Production Readiness Checklist
- [x] Zero permanent cloud credentials stored in repository secrets.
- [x] AWS IAM Trust Policy strictly bound to repository, environment, and audience.
- [x] Required Reviewers enforced on `production` environment.
- [x] Single-flight concurrency locking (`concurrency.cancel-in-progress: false`).
- [x] Automated rollback script executes if synthetic health checks fail.

### Troubleshooting Table

| Error | Root Cause | Solution |
|:---|:---|:---|
| `Not authorized to perform sts:AssumeRoleWithWebIdentity` | OIDC Trust Policy condition mismatch (`sub` or `aud`) | Verify `repo:org/name:environment:prod` matches exact repository and environment name |
| `Unable to get ACTIONS_ID_TOKEN_REQUEST_URL` | Missing `id-token: write` permission | Add `permissions: id-token: write` to job |
| Deployment queued behind old build | Concurrency lock held by pending approval | Cancel or approve preceding run to free lock |

---

[<- Back to Projects Overview](./README.md)
