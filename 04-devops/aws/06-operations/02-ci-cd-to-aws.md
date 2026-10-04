# CI/CD to AWS: GitHub Actions with OIDC

A deployment pipeline needs to talk to your AWS account. The old way was to create an IAM user, generate an access key, and paste it into your CI system's secrets. That key is long-lived, powerful, and one leaked log line (or compromised repo) away from a breach.

The modern way is **OpenID Connect (OIDC) federation**: GitHub proves to AWS "this job is running in repo X on branch Y", and AWS hands the job **temporary credentials** for a role you defined. **No stored AWS secret exists at all.**

This note covers that setup end to end and the pipeline patterns built on it. The same idea works for GitLab, Bitbucket and others (they publish their own OIDC issuers).

Prerequisites: [IAM](../01-foundations/03-iam.md) (roles and trust policies, the core of this topic), [Infrastructure as Code](./01-infrastructure-as-code.md).

---

## How OIDC federation works

```
GitHub Actions job                          AWS
 1. job requests an ID token
    from GitHub's OIDC provider  ──┐
    (claims: repo, branch, env…)   │
 2. job calls sts:AssumeRoleWithWebIdentity ───────►  STS verifies the token against the
    with the token + role ARN                          IAM OIDC provider, then checks the
                                                       role's TRUST POLICY conditions
 3. ◄──────────── temporary credentials (1 hour by default) ─────────
 4. job uses them for cdk deploy / aws s3 sync / ecs update-service
```

Three pieces you create in AWS (once per account):

1. An **IAM OIDC identity provider** for `token.actions.githubusercontent.com`.
2. An **IAM role** whose **trust policy** says which GitHub repos/branches/environments may assume it.
3. The role's **permissions policy**: what the pipeline may do.

The security lives in the **trust policy conditions**. A role anyone on GitHub could assume would be a disaster.

---

## Setup

### 1. Create the OIDC provider

```bash
aws iam create-open-id-connect-provider \
  --url https://token.actions.githubusercontent.com \
  --client-id-list sts.amazonaws.com
```

(Current IAM validates GitHub's certificate chain itself, so the old "thumbprint" step is no longer something you need to hand-maintain. Some tooling still asks for one, so follow its current docs.) You create the provider **once per account**, not per role.

### 2. Create a role with a tightly scoped trust policy

```json
{
  "Version": "2012-10-17",
  "Statement": [{
    "Effect": "Allow",
    "Principal": {
      "Federated": "arn:aws:iam::111122223333:oidc-provider/token.actions.githubusercontent.com"
    },
    "Action": "sts:AssumeRoleWithWebIdentity",
    "Condition": {
      "StringEquals": {
        "token.actions.githubusercontent.com:aud": "sts.amazonaws.com",
        "token.actions.githubusercontent.com:sub": "repo:my-org/my-repo:ref:refs/heads/main"
      }
    }
  }]
}
```

- **`aud`** must be `sts.amazonaws.com`: it confirms the token was minted for AWS.
- **`sub`** (the *subject*) identifies *who* GitHub says is calling. This is the line that matters most. Common formats:

| Where the job runs | `sub` value |
|---|---|
| Push/workflow on a branch | `repo:ORG/REPO:ref:refs/heads/main` |
| Job using a GitHub **Environment** | `repo:ORG/REPO:environment:prod` |
| Pull request | `repo:ORG/REPO:pull_request` |
| Tag | `repo:ORG/REPO:ref:refs/tags/v1.0.0` |

Use `StringEquals` for exact values, or `StringLike` with a wildcard when you need several (e.g. `repo:my-org/my-repo:*`). **Never** leave `sub` as `*` or as `repo:my-org/*`, which would let any repository in the organisation (or, with `*`, anyone on GitHub) assume your role.

**Best practice:** one role per **environment**, using the **`environment:`** subject. Then GitHub Environment protection rules (required reviewers, branch restrictions) *become* AWS access controls:

```
Role: deploy-prod  trusts  repo:my-org/my-repo:environment:prod   → needs a manual approval in GitHub
Role: deploy-dev   trusts  repo:my-org/my-repo:environment:dev
Role: ci-readonly  trusts  repo:my-org/my-repo:pull_request       → read-only, for `cdk diff`
```

### 3. Give the role only the permissions it needs

For a **CDK** project the deploy role does *not* need broad admin rights. `cdk bootstrap` created dedicated roles (deploy, file-publishing, image-publishing, lookup) that CDK assumes during a deploy. Your pipeline role mainly needs permission to **assume those roles**:

```json
{
  "Effect": "Allow",
  "Action": "sts:AssumeRole",
  "Resource": "arn:aws:iam::111122223333:role/cdk-*"
}
```

(The bootstrap roles are named with a qualifier, `cdk-<qualifier>-…`, `hnb659fds` by default. Scope the resource to your actual qualifier and account/region if you want it tighter.) CloudFormation then performs the changes using the bootstrap **CloudFormation execution role**, which is the one that holds the broad permissions. Restrict *who can reach it* (trust policy) rather than trying to hand-write a perfect policy for every resource type. For other deployment styles (S3 sync, ECR push, ECS update) write a small, specific policy.

---

## The workflow

```yaml
# .github/workflows/deploy.yml
name: deploy
on:
  push:
    branches: [main]

permissions:
  id-token: write      # REQUIRED to request the OIDC token
  contents: read

concurrency:
  group: deploy-prod   # never run two deployments at once
  cancel-in-progress: false

jobs:
  deploy:
    runs-on: ubuntu-latest
    environment: prod                      # enables approvals + the environment: sub claim
    steps:
      - uses: actions/checkout@v6
      - uses: actions/setup-node@v6
        with: { node-version: 22, cache: npm }
      - run: npm ci
      - run: npm test

      - uses: aws-actions/configure-aws-credentials@v6
        with:
          role-to-assume: arn:aws:iam::111122223333:role/deploy-prod
          aws-region: ap-south-1

      - run: aws sts get-caller-identity       # sanity check: who am I?
      - run: npx cdk deploy --all --require-approval never
```

And a separate **PR workflow** that runs checks and `cdk diff` with a **read-only** role:

```yaml
on: { pull_request: { branches: [main] } }
permissions: { id-token: write, contents: read, pull-requests: write }
# ...same setup steps, role-to-assume: .../ci-readonly, then:
#   - run: npx cdk diff
```

(Pin action versions deliberately. Check each action's current release, and for sensitive pipelines pin to a **full commit SHA** rather than a movable tag.)

Key details:

- **`permissions: id-token: write`** is what allows the job to request the token. Without it the credentials action fails.
- **`environment: prod`** both gates the job (reviewers, wait timers) and changes the `sub` claim to `repo:…:environment:prod`, so your trust policy must match that form, **not** the branch form.
- **`concurrency`** serialises deployments so two pushes don't race CloudFormation.
- Make `--require-approval never` safe by reviewing `cdk diff` in the PR and gating production behind an **Environment approval**.
- Fork pull requests don't receive OIDC tokens (by design), and `pull_request_target` workflows run with elevated trust. Don't check out and run untrusted PR code there.

---

## Common deployment patterns

| Target | Pipeline steps |
|---|---|
| **CDK app** | `npm ci` → test → `cdk diff` (PR) → `cdk deploy` (main) |
| **Static site (S3 + CloudFront)** | build → `aws s3 sync dist s3://bucket --delete` → `aws cloudfront create-invalidation --paths "/index.html"` ([CloudFront](../04-networking/02-route53-and-cloudfront.md)) |
| **Container on ECS/Fargate** | build image → push to ECR (tag with the **git SHA**) → register new task definition → update service, waiting for stability ([ECS](../02-compute/03-ecs-fargate.md)) |
| **Lambda** | Usually via CDK/SAM deploy; for fast code-only updates, `aws lambda update-function-code` (then publish a version and shift an alias) |

```yaml
# ECS image deploy sketch
- uses: aws-actions/amazon-ecr-login@v2
  id: ecr
- run: |
    IMAGE=${{ steps.ecr.outputs.registry }}/web:${{ github.sha }}
    docker build -t "$IMAGE" . && docker push "$IMAGE"
```

### Release safety

- **Tag images and artifacts with the commit SHA**, so you always know what's running and can redeploy a previous one.
- **Rolling back = redeploying a previous known-good commit/image.** CloudFormation also auto-rolls back failed updates. Make rollback a practiced action, not an improvisation.
- **Progressive delivery:** ECS rolling updates with a **deployment circuit breaker**; Lambda **aliases with weighted traffic shifting** (via CodeDeploy) for canary/linear releases; feature flags to decouple deploy from release.
- **Database migrations** need their own care: make them backward-compatible (expand → migrate → contract) so old and new code can both run during a rollout.
- Promote the **same artifact** through environments instead of rebuilding for each.
- Alternatives to GitHub Actions: **CodePipeline / CDK Pipelines** (AWS-native, self-mutating pipelines) and other CI systems that support OIDC. The role/trust-policy ideas are identical.

---

## Common mistakes

- **Long-lived IAM user keys** stored in CI secrets.
- **Wildcard `sub`** (`*`, or a whole org) in the trust policy.
- **Missing `permissions: id-token: write`.**
- **Branch-style `sub` in the trust policy but `environment:` in the workflow** (or the reverse). The values must match what GitHub actually sends.
- **One god-role for everything**: dev, prod and PR checks all sharing admin rights.
- **Admin permissions on the pipeline role** "to make it work".
- Running **`cdk deploy` on pull requests** from untrusted contexts.
- Tagging images **`latest`**, so no rollback target.
- No **concurrency control**, causing overlapping stack updates.
- Unpinned third-party actions in a job that holds production credentials.
- Not **bootstrapping** the target account/region first.
- Printing **credentials or secrets** into logs (masking helps, but don't rely on it).

---

## Debugging

The classic failure:

```
Error: Could not assume role with OIDC: Not authorized to perform sts:AssumeRoleWithWebIdentity
```

Work through these in order:

1. **Is `id-token: write` set** on the workflow/job?
2. **`sub` mismatch**: compare the claim GitHub sent to the trust policy exactly (branch vs environment vs pull_request; case-sensitive org/repo names; tag vs branch). The failed `AssumeRoleWithWebIdentity` event in **CloudTrail** shows the request context, which is the fastest way to see what was presented.
3. **`aud` mismatch**: should be `sts.amazonaws.com` unless you changed the action's audience (e.g. China regions use a different one).
4. **OIDC provider missing** in *that account*, or the role ARN points to the wrong account.
5. **Role trust policy** has the wrong `Principal` (provider ARN) or the wrong `Action` (must be `sts:AssumeRoleWithWebIdentity`).
6. Running from a **fork** PR (no token issued).
7. **Session duration / role chaining** limits if you chain roles from the first role.

| Other symptom | Likely cause |
|---|---|
| `Credentials could not be loaded` | The credentials step didn't run or failed, or `aws-region` is missing |
| `AccessDenied` in `cdk deploy` | The role can't assume the `cdk-*` bootstrap roles (wrong qualifier/account), or a lookup role is missing |
| `... must be bootstrapped` | Run `cdk bootstrap` in that account/region (or upgrade a stale bootstrap) |
| Works locally, fails in CI | Different identity: CI uses the role, not your admin profile. Compare `aws sts get-caller-identity` in both |
| Deploy hangs / races | Missing `concurrency`, or a previous run still holds the stack in `UPDATE_IN_PROGRESS` |
| Image pull fails after deploy | The new task definition's execution role/ECR permissions or architecture mismatch ([ECS](../02-compute/03-ecs-fargate.md)) |

---

## Quick Summary

- Use **OIDC federation** so CI gets **temporary credentials**: no AWS keys stored in GitHub.
- Create once: **OIDC provider**, then per environment a **role whose trust policy pins `aud` and a specific `sub`** (repo + branch/environment).
- **One role per environment**; use GitHub **Environments** (approvals) with the `environment:` subject for production. Use a **read-only role** for PR checks and `cdk diff`.
- The workflow needs **`permissions: id-token: write`** plus `aws-actions/configure-aws-credentials`.
- For CDK, the pipeline role mostly **assumes the `cdk-*` bootstrap roles**: don't give it blanket admin.
- Tag artifacts with the **git SHA**, serialise deploys with **`concurrency`**, and treat rollback as redeploying a known-good version.
- `Not authorized to perform sts:AssumeRoleWithWebIdentity` = almost always a **`sub`/`aud`/permissions** mismatch. Check CloudTrail.

**Next:** [Security and Secrets](./03-security-and-secrets.md)
