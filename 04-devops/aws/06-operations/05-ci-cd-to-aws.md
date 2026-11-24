# Continuous Integration & Continuous Delivery (CI/CD) to AWS

> **Module**: `06-operations`  
> **Topic**: Deployment Pipelines, CodePipeline, CodeDeploy, GitHub Actions OIDC & Deployment Strategies  
> **Target Audience**: DevOps Engineers, Release Managers, Cloud Architects  
> **Last Updated**: November 2026

---

## Navigation & Contents

- [1. What & Why](#1-what--why)
- [2. Core Concepts](#2-core-concepts)
- [3. How It Works Internally](#3-how-it-works-internally)
- [4. Syntax & Usage](#4-syntax--usage)
- [5. Practical Examples](#5-practical-examples)
- [6. Best Practices](#6-best-practices)
- [7. Common Mistakes](#7-common-mistakes)
- [8. Performance, Security & Cost Notes](#8-performance-security--cost-notes)
- [9. Comparison Table](#9-comparison-table)
- [10. Interview Questions & Answers](#10-interview-questions--answers)
- [11. Practice Exercises](#11-practice-exercises)
- [12. Summary & Quick Reference](#12-summary--quick-reference)
- [13. Further Reading](#13-further-reading)

---

## 1. What & Why

Deploying software manually to production servers via SSH or running ad-hoc build scripts from developer laptops introduces critical failure modes:
- **Human Error & Inconsistency**: Forgetting an environment variable, running migrations out of order, or deploying the wrong Git branch.
- **Downtime During Releases**: In-place replacements terminate running web processes, dropping active customer connections and degrading availability.
- **Security Vulnerabilities (Credential Leakage)**: Storing permanent AWS Access Keys and Secrets inside third-party CI systems (Jenkins, CircleCI, GitHub) creates severe risk of key exfiltration.

**Modern CI/CD to AWS** automates the build, test, and release cycle through declarative pipelines. By leveraging modern **GitHub Actions OIDC Federation** (eliminating static credentials) or the AWS Developer Tools suite (**AWS CodePipeline, AWS CodeBuild, and AWS CodeDeploy**), engineering teams achieve:
1. **Zero-Downtime Deployments**: Traffic shifting via Blue/Green and Canary deployments with automated rollback on health alarm breaches.
2. **Short Mean Time to Recovery (MTTR)**: Fast, repeatable pipelines that promote validated artifacts across development, staging, and production environments.
3. **Strict Security Posture**: Short-lived, temporary AWS IAM credentials exchanged dynamically via OpenID Connect (OIDC).

---

## 2. Core Concepts

### 2.1 The AWS Developer Tools Suite
1. **AWS CodePipeline**: A fully managed continuous delivery service that models, visualizes, and automates software release stages (Source -> Build -> Test -> Manual Approval -> Production Deploy).
2. **AWS CodeBuild**: A serverless build service that compiles source code, runs unit tests, scans dependencies, and produces deployable artifacts (Docker images, zip files) inside isolated Docker containers.
3. **AWS CodeDeploy**: An automated deployment engine that coordinates application releases across Amazon EC2 instances, ECS Fargate services, Lambda functions, and on-premises servers.
4. **AWS CodeCommit**: Legacy AWS Git repository service (AWS deprecated onboarding new customers to CodeCommit in 2024; industry standard is GitHub or GitLab).

### 2.2 Deployment Strategies
- **In-Place Deployment**: Stops the application on existing instances, updates the code, and restarts. Incurs downtime and requires manual rollback if deployment fails.
- **Rolling Deployment**: Updates instances in batches (e.g., 25% at a time) behind an Application Load Balancer. Preserves service availability throughout the deployment.
- **Blue/Green Deployment**: Provisions a completely separate, duplicate production fleet (Green). Once health checks pass on Green, traffic is swapped over instantly at the load balancer level. The old fleet (Blue) is retained for rollback safety.
- **Canary Deployment**: Shifts a small fraction of traffic (e.g., 10%) to the new version for a soak period (e.g., 15 minutes). If error metrics or CloudWatch alarms remain green, the remaining 90% is cut over automatically.

### 2.3 GitHub Actions OIDC Federation (Zero Static Keys)
- Historically, developers created dedicated IAM users with `AWS_ACCESS_KEY_ID` and `AWS_SECRET_ACCESS_KEY` stored in GitHub Secrets.
- **Modern Standard**: GitHub Actions assumes an IAM Role directly using **OpenID Connect (OIDC)** and `sts:AssumeRoleWithWebIdentity`.
- AWS validates GitHub's cryptographically signed OIDC token, evaluates repository/branch trust conditions, and issues short-lived (1-hour) temporary credentials. Static credentials are completely eliminated.

### 2.4 CodeDeploy AppSpec File (`appspec.yml`)
- A configuration file used by CodeDeploy to define deployment actions, target file mappings, and lifecycle event hooks (`BeforeInstall`, `AfterInstall`, `ApplicationStart`, `ValidateService`).

---

## 3. How It Works Internally

```
+---------------------------------------------------------------------------------------------------+
|                        GITHUB ACTIONS TO AWS VIA OIDC FEDERATION & PIPELINE                       |
|                                                                                                   |
|  [Developer Git Push to main]                                                                     |
|         |                                                                                         |
|         v                                                                                         |
|  +---------------------------------------------------------------------------------------------+  |
|  | GITHUB ACTIONS RUNNER                                                                       |  |
|  | 1. Requests OIDC JWT token from GitHub OIDC Provider                                        |  |
|  | 2. Calls AWS STS: `sts:AssumeRoleWithWebIdentity` passing OIDC Token                        |  |
|  +---------------------------------------------------------------------------------------------+  |
|         |                                                                                         |
|         v (Validates Token & Condition: repo:corp/app:ref:refs/heads/main)                        |
|  +---------------------------------------------------------------------------------------------+  |
|  | AWS IDENTITY & ACCESS MANAGEMENT (IAM)                                                      |  |
|  | - Issues temporary 1-hour session credentials (AccessKeyId, SecretKey, SessionToken)       |  |
|  +---------------------------------------------------------------------------------------------+  |
|         |                                                                                         |
|         v                                                                                         |
|  +---------------------------------------------------------------------------------------------+  |
|  | BUILD & PUBLISH STAGE                                                                       |  |
|  | 1. Compiles TypeScript / Go / Python binary                                                 |  |
|  | 2. Builds Docker Container Image & runs vulnerability scan (Trivy)                          |  |
|  | 3. Authenticates with ECR: `docker push 123456789012.dkr.ecr.us-east-1.amazonaws.com/app`   |  |
|  +---------------------------------------------------------------------------------------------+  |
|         |                                                                                         |
|         v (Trigger Deployment)                                                                    |
|  +---------------------------------------------------------------------------------------------+  |
|  | AWS CODEDEPLOY (BLUE / GREEN DEPLOYMENT)                                                    |  |
|  | 1. Provisions Green ECS Task Set / Target Group                                             |  |
|  | 2. Runs Test Traffic Route (Port 8443) -> Validates `/healthz` passes                       |  |
|  | 3. Shifts Production Traffic via ALB Listener Rule (Linear 10% every 3 minutes)            |  |
|  | 4. Monitors CloudWatch 5XX Alarms: If alarm fires -> Immediate Automatic Rollback!          |  |
|  +---------------------------------------------------------------------------------------------+  |
+---------------------------------------------------------------------------------------------------+
```

---

## 4. Syntax & Usage

### 4.1 Configuring AWS OIDC Identity Provider via AWS CLI

```bash
# 1. Create OIDC Provider for GitHub Actions (URL: https://token.actions.githubusercontent.com)
aws iam create-open-id-connect-provider \
    --url "https://token.actions.githubusercontent.com" \
    --client-id-list "sts.amazonaws.com" \
    --thumbprint-list "6938fd4d98bab03faadb97b34396831e3780aea1"

# 2. Trigger a CodeDeploy Deployment via CLI
aws deploy create-deployment \
    --application-name "ProductionWebApp" \
    --deployment-group-name "ProductionECSGroup" \
    --deployment-config-name "CodeDeployDefault.ECSLinear10PercentEvery3Minutes" \
    --description "Deployed via CI Pipeline Git SHA ${GITHUB_SHA}"
```

---

## 5. Practical Examples

### 5.1 Production GitHub Actions Workflow with OIDC & ECR Deployment

```yaml
name: "Production CI/CD Pipeline"

on:
  push:
    branches: [ main ]

permissions:
  id-token: write # Required for requesting the JWT token
  contents: read  # Required for checkout

jobs:
  build-and-deploy:
    runs-on: ubuntu-latest
    steps:
      - name: Checkout Code
        uses: actions/checkout@v4

      # Authenticate to AWS via OIDC (Zero static credentials!)
      - name: Configure AWS Credentials via OIDC
        uses: aws-actions/configure-aws-credentials@v4
        with:
          role-to-assume: arn:aws:iam::123456789012:role/GitHubActionsDeploymentRole
          aws-region: us-east-1
          audience: sts.amazonaws.com

      # Login to Amazon ECR
      - name: Login to Amazon ECR
        id: login-ecr
        uses: aws-actions/amazon-ecr-login@v2

      # Build, tag, and push Docker image
      - name: Build and Push Docker Image
        env:
          REGISTRY: ${{ steps.login-ecr.outputs.registry }}
          REPOSITORY: production-order-service
          IMAGE_TAG: ${{ github.sha }}
        run: |
          docker build -t $REGISTRY/$REPOSITORY:$IMAGE_TAG .
          docker push $REGISTRY/$REPOSITORY:$IMAGE_TAG
          echo "image=$REGISTRY/$REPOSITORY:$IMAGE_TAG" >> $GITHUB_OUTPUT

      # Deploy updated Task Definition to Amazon ECS
      - name: Deploy Amazon ECS Task Definition
        uses: aws-actions/amazon-ecs-deploy-task-definition@v2
        with:
          task-definition: task-definition.json
          service: order-service-prod
          cluster: production-cluster
          wait-for-service-stability: true
```

### 5.2 OIDC IAM Role Definition (Terraform)

```hcl
# IAM Role assumed by GitHub Actions
resource "aws_iam_role" "github_actions" {
  name = "GitHubActionsDeploymentRole"

  assume_role_policy = jsonencode({
    Version = "2012-10-17"
    Statement = [{
      Effect = "Allow"
      Principal = {
        Federated = "arn:aws:iam::${var.account_id}:oidc-provider/token.actions.githubusercontent.com"
      }
      Action = "sts:AssumeRoleWithWebIdentity"
      Condition = {
        StringEquals = {
          "token.actions.githubusercontent.com:aud" = "sts.amazonaws.com"
        }
        StringLike = {
          # Restrict assumption strictly to main branch of your repository!
          "token.actions.githubusercontent.com:sub" = "repo:enterprise-org/production-order-service:ref:refs/heads/main"
        }
      }
    }]
  })
}

# Attach Deployment Permissions to Role
resource "aws_iam_role_policy_attachment" "ecr_policy" {
  role       = aws_iam_role.github_actions.name
  policy_arn = "arn:aws:iam::aws:policy/AmazonEC2ContainerRegistryPowerUser"
}
```

### 5.3 Complete AWS CodePipeline Definition with Manual Approval (Terraform)

```hcl
resource "aws_codepipeline" "pipeline" {
  name     = "order-service-pipeline"
  role_arn = var.codepipeline_role_arn

  artifact_store {
    location = var.s3_artifact_bucket_name
    type     = "S3"
  }

  # Stage 1: Source
  stage {
    name = "Source"
    action {
      name             = "SourceAction"
      category         = "Source"
      owner            = "AWS"
      provider         = "CodeStarSourceConnection"
      version          = "1"
      output_artifacts = ["source_output"]
      configuration = {
        ConnectionArn    = var.codestar_connection_arn
        FullRepositoryId = "enterprise-org/production-order-service"
        BranchName       = "main"
      }
    }
  }

  # Stage 2: Build
  stage {
    name = "Build"
    action {
      name             = "BuildAction"
      category         = "Build"
      owner            = "AWS"
      provider         = "CodeBuild"
      version          = "1"
      input_artifacts  = ["source_output"]
      output_artifacts = ["build_output"]
      configuration = {
        ProjectName = var.codebuild_project_name
      }
    }
  }

  # Stage 3: Manual Approval Gate
  stage {
    name = "ProductionGate"
    action {
      name     = "ApprovalAction"
      category = "Approval"
      owner    = "AWS"
      provider = "Manual"
      version  = "1"
    }
  }

  # Stage 4: Deploy
  stage {
    name = "Deploy"
    action {
      name            = "DeployAction"
      category        = "Deploy"
      owner           = "AWS"
      provider        = "CodeDeploy"
      version         = "1"
      input_artifacts = ["build_output"]
      configuration = {
        ApplicationName     = var.codedeploy_app_name
        DeploymentGroupName = var.codedeploy_group_name
      }
    }
  }
}
```

### 5.4 CodeDeploy AppSpec for EC2 In-Place Deployment (`appspec.yml`)

```yaml
version: 0.0
os: linux
files:
  - source: /build/target/app.jar
    destination: /opt/enterprise/app/
hooks:
  ApplicationStop:
    - location: scripts/stop_service.sh
      timeout: 60
      runas: root
  BeforeInstall:
    - location: scripts/clean_old_artifacts.sh
      timeout: 30
      runas: root
  ApplicationStart:
    - location: scripts/start_service.sh
      timeout: 120
      runas: root
  ValidateService:
    - location: scripts/health_check.sh
      timeout: 60
      runas: root
```

---

## 6. Best Practices

1. **Eliminate All Static AWS Access Keys**: Never store `AWS_ACCESS_KEY_ID` and `AWS_SECRET_ACCESS_KEY` in GitHub Secrets. Always use GitHub Actions OIDC federation with short-lived session tokens.
2. **Strictly Scope the OIDC `sub` Condition**: Never use a wildcard `sub: repo:org/*` in your IAM trust policy. Always lock down the exact repository and branch (`repo:org/repo:ref:refs/heads/main`) to prevent unauthorized workflows from assuming deployment roles.
3. **Automate Rollbacks with CloudWatch Alarms**: In CodeDeploy, configure automated rollback on `5XXErrorRate` or `TargetResponseTime` alarms. If errors spike during traffic shifting, CodeDeploy rolls back to Blue automatically.
4. **Enforce Immutable Container Tags**: Tag Docker images with the exact Git commit SHA (`${{ github.sha }}`) rather than mutable tags like `latest`. This ensures reproducible rollbacks and exact deployment traceability.
5. **Implement Pre-Deployment and Post-Deployment Hook Checks**: Use `ValidateService` hooks to execute synthetic API calls (`curl -f http://localhost:8080/healthz`) before the load balancer starts routing real customer traffic to the instance.

---

## 7. Common Mistakes

| Mistake | Root Cause | Impact | Fix |
|---|---|---|---|
| Wildcard OIDC Subject Condition | Trust policy sets `"sub": "*"`. | Any GitHub user across the entire public website can assume your deployment role! | Restrict `sub` strictly to `repo:your-org/your-repo:ref:refs/heads/main`. |
| Deploying with Mutable `latest` Tag | Task definition points to `image:app:latest`. | ECS cannot detect image updates; deployments do not trigger new task versions. | Use Git SHA tags (`image:app:${{ github.sha }}`). |
| Missing `id-token: write` Permission | GitHub Actions workflow omits `permissions` block. | `configure-aws-credentials` fails with `Unable to retrieve OIDC token`. | Add `permissions: id-token: write` to workflow YAML. |
| CodeDeploy Hook Script Timeout | Hook script executes blocking process without backgrounding (`&`). | Deployment hangs for 1 hour and fails with `ScriptTimedOut`. | Ensure background services disconnect stdout/stderr: `nohup ... > /dev/null 2>&1 &`. |
| Missing S3 Bucket Policy for Artifacts | CodePipeline S3 bucket lacks KMS decrypt permissions for CodeBuild. | Pipeline fails with `AccessDeniedException` during artifact download. | Grant KMS decrypt permissions to the CodeBuild execution role. |
| In-Place Deployments on Single-Instance Services | Running in-place deployment on an application with 1 EC2 instance. | 100% service outage for 2-5 minutes during every release. | Use Blue/Green deployments or maintain at least 2 instances behind ALB. |

---

## 8. Performance, Security & Cost Notes

### 8.1 Performance & Build Caching
- **CodeBuild Docker Layer Caching**: Enable Local or S3 caching in CodeBuild (`cache: type: LOCAL, modes: [LOCAL_DOCKER_LAYER_CACHE]`) to slash container build times from 10 minutes to under 60 seconds.

### 8.2 Security
- **Artifact Encryption**: All CodePipeline and CodeBuild intermediate artifacts stored in Amazon S3 must be encrypted using an AWS KMS Customer Managed Key (CMK).
- **VPC CodeBuild**: Run CodeBuild projects inside private VPC subnets to access internal package registries or databases without public internet transit.

### 8.3 Cost Breakdown
- **GitHub Actions**: 2,000 free runner minutes/month; self-hosted runners are free.
- **AWS CodePipeline**: \$1.00 per active pipeline per month (first 1 active pipeline free).
- **AWS CodeBuild**: Billed per build minute (e.g., `general1.small` is \$0.005/minute).
- **AWS CodeDeploy**: Free for EC2, Lambda, and ECS deployments.

---

## 9. Comparison Table

| Dimension | GitHub Actions (with OIDC) | AWS Native Suite (CodePipeline) | GitLab CI / Jenkins |
|---|---|---|---|
| **Primary Location** | GitHub SaaS | AWS Native Console | Self-hosted or SaaS |
| **Authentication** | OIDC Federation (Temporary STS)| Native IAM Service Roles | Static Keys or OIDC Plugin |
| **Visual Workflow** | GitHub Actions Tab | CodePipeline Visual Stage UI | Jenkins Pipeline View |
| **Maintenance** | Serverless / Zero setup | Serverless / Zero setup | Server / Agent patching |
| **Multi-Cloud Support**| Yes (Universal) | AWS Native | Yes (Universal) |
| **Industry Adoption**| Dominant modern standard | Common in pure AWS shops | Enterprise legacy / hybrid |

---

## 10. Interview Questions & Answers

<details>
<summary><strong>Q1: How does GitHub Actions OIDC federation eliminate the need for static AWS Access Keys?</strong></summary>

In traditional setups, developers store long-lived `AWS_ACCESS_KEY_ID` and `AWS_SECRET_ACCESS_KEY` secrets in GitHub, which risk exfiltration or credential rotation failures.
With OIDC:
1. The GitHub Actions runner requests an OpenID Connect JSON Web Token (JWT) from GitHub's OIDC provider.
2. The runner invokes `sts:AssumeRoleWithWebIdentity`, passing the token to AWS.
3. AWS verifies the cryptographic signature against GitHub's public OIDC thumbprint and checks the IAM Role's trust policy conditions (verifying `aud: sts.amazonaws.com` and matching the exact repository/branch in `sub`).
4. If valid, AWS issues temporary, short-lived session credentials valid for 1 hour. No static credentials exist anywhere.
</details>

<details>
<summary><strong>Q2: Explain the mechanics of a Blue/Green deployment on Amazon ECS using AWS CodeDeploy.</strong></summary>

1. **Initial State**: Production traffic routes to the **Blue** ECS Task Set via the ALB Production Listener (Port 443).
2. **Provisioning Green**: CodeDeploy deploys the new task definition into a newly created **Green** ECS Task Set.
3. **Test Traffic Validation**: CodeDeploy routes test traffic through an ALB Test Listener (e.g., Port 8443) directly to the Green tasks. Automated integration tests or health probes run against Green.
4. **Traffic Shifting**: Once validated, CodeDeploy shifts production traffic from Blue to Green either immediately, linearly (e.g., 10% every 3 minutes), or via canary (10% for 15 minutes, then 100%).
5. **Rollback / Cleanup**: If any CloudWatch Alarm fires during shifting, traffic immediately snaps back 100% to Blue. If successful, CodeDeploy terminates the Blue task set after a configured wait period.
</details>

<details>
<summary><strong>Q3: What are the lifecycle hooks available in an AWS CodeDeploy AppSpec file for EC2 deployments?</strong></summary>

CodeDeploy executes scripts in strict sequential order:
1. `ApplicationStop`: Gracefully terminates the currently running service before code replacement.
2. `DownloadBundle`: CodeDeploy agent downloads and extracts the artifact package.
3. `BeforeInstall`: Pre-installation tasks (e.g., creating directories, backing up current config).
4. `Install`: Copies files to destination paths.
5. `AfterInstall`: Post-install tasks (e.g., setting file permissions, compiling assets).
6. `ApplicationStart`: Starts the service process (e.g., `systemctl start app`).
7. `ValidateService`: Verifies service health (e.g., polling `/healthz` endpoint).
</details>

<details>
<summary><strong>Q4: What is the difference between a Linear deployment and a Canary deployment in CodeDeploy?</strong></summary>

- **Canary Deployment**: Shifts an initial small percentage of traffic (e.g., `CodeDeployDefault.LambdaCanary10Percent15Minutes` shifts 10%), waits for a fixed duration (15 minutes) while monitoring CloudWatch alarms, and then shifts the remaining 90% in a single final step.
- **Linear Deployment**: Shifts traffic incrementally in equal percentages at equal intervals (e.g., `CodeDeployDefault.ECSLinear10PercentEvery3Minutes` shifts 10%, waits 3 minutes, shifts another 10%, until 100% is reached across 30 minutes).
</details>

<details>
<summary><strong>Q5: Why should you avoid using the latest tag for container deployments in CI/CD?</strong></summary>

1. **Lack of Traceability**: `latest` points to different code over time, making it impossible to correlate a running container in production to a specific Git commit SHA.
2. **ECS Update Detection Failure**: Amazon ECS detects task definition changes. If the image tag remains `app:latest`, ECS may determine that no update occurred and fail to trigger rolling deployments.
3. **Rollback Impossibility**: Rolling back to `latest` simply redeploys whatever image currently overwrote that tag, preventing clean rollbacks to prior stable revisions.
</details>

<details>
<summary><strong>Q6: How do you achieve automated rollback when deploying infrastructure with Terraform in a CI/CD pipeline?</strong></summary>

Unlike CloudFormation (which handles automatic rollbacks natively), Terraform does not roll back applied changes on failure; it saves partial state. 
To manage rollbacks in Terraform CI/CD pipelines:
1. Revert the breaking commit in Git, triggering the pipeline to run `terraform apply` against the previous known-good code.
2. Use blue/green infrastructure patterns (provisioning a new environment, verifying it, and flipping DNS/ALB pointers).
3. Implement automated canary tests: if synthetic tests fail immediately post-apply, an automated CI job executes a Git revert and apply.
</details>

<details>
<summary><strong>Q7: How do CodePipeline Artifacts work and where are they stored?</strong></summary>

When a CodePipeline stage finishes (e.g., Source or Build), it packages output files into a `.zip` archive called an **Artifact** and uploads it to an **Amazon S3 Artifact Store Bucket**. The next stage downloads the zip archive, extracts it into its local environment, executes its tasks, and generates its own output artifact for subsequent stages. All artifacts are encrypted at rest using AWS KMS.
</details>

<details>
<summary><strong>Q8: What is the difference between an In-Place deployment and a Blue/Green deployment?</strong></summary>

- **In-Place**: The code update is applied directly to the running server instances. It consumes fewer resources and is cheaper, but it requires stopping the application process, creating downtime unless instances are batched behind an ALB. Rollback requires a full redeployment of the old code.
- **Blue/Green**: Provisions a fresh, independent set of resources (Green) alongside the running environment (Blue). Traffic is cut over at the load balancer or DNS level. It guarantees zero downtime, but doubles resource usage during the release window. Rollback is near-instantaneous (reverting the traffic pointer back to Blue).
</details>

---

## 11. Practice Exercises

<details>
<summary><strong>Exercise 1: Configure GitHub Actions OIDC Provider in AWS</strong></summary>

**Goal**: Establish passwordless federation between GitHub and AWS.
1. Create an OIDC Identity Provider in IAM for `https://token.actions.githubusercontent.com`.
2. Create an IAM Role with trust condition matching your personal GitHub repository.
3. Create a GitHub Actions workflow using `aws-actions/configure-aws-credentials`.
4. Run `aws sts get-caller-identity` in the workflow and confirm successful authentication without static keys.
</details>

<details>
<summary><strong>Exercise 2: Build and Push Docker Image to ECR via GitHub Actions</strong></summary>

**Goal**: Automate container image publishing.
1. Create a private Amazon ECR repository.
2. Grant `ecr:GetAuthorizationToken` and push permissions to your OIDC role.
3. Trigger a GitHub Actions run that builds a Dockerfile and pushes it tagged with `${{ github.sha }}`.
4. Verify the image appears in the ECR repository console.
</details>

<details>
<summary><strong>Exercise 3: Deploy an AWS CodePipeline with CloudFormation Source</strong></summary>

**Goal**: Automate CloudFormation deployment from a Git repo.
1. Create a pipeline in CodePipeline with two stages: Source (GitHub) and Deploy (CloudFormation).
2. Push a template to GitHub.
3. Observe CodePipeline detect the commit, download the template, and deploy the stack.
</details>

<details>
<summary><strong>Exercise 4: Test Blue/Green Traffic Shifting on ECS</strong></summary>

**Goal**: Execute a linear canary deployment with CodeDeploy.
1. Configure an ECS Fargate service with CodeDeploy Blue/Green deployment.
2. Update the task definition image.
3. Watch CodeDeploy shift traffic 10% every 3 minutes across the ALB target groups.
</details>

<details>
<summary><strong>Exercise 5: Simulate Automated Rollback on CloudWatch Alarm</strong></summary>

**Goal**: Verify automated safety mechanisms.
1. Attach a CloudWatch Alarm (monitoring HTTP 5XX errors) to a CodeDeploy Deployment Group.
2. Deploy a bad application version that throws 500 errors.
3. Observe the alarm fire and verify CodeDeploy halts traffic shifting and rolls back 100% to the previous version.
</details>

<details>
<summary><strong>Exercise 6: Author an appspec.yml with Validation Hooks</strong></summary>

**Goal**: Add automated health checks to an EC2 deployment.
1. Write an `appspec.yml` with `ValidateService` hook.
2. In `scripts/health_check.sh`, write a curl command checking `http://localhost/healthz`.
3. Verify CodeDeploy marks the deployment failed if the health check returns non-200.
</details>

---

## 12. Summary & Quick Reference

```
+---------------------------------------------------------------------------------------------------+
| CI/CD DEPLOYMENT STRATEGY SELECTION MATRIX                                                        |
+--------------------------+-----------------------+-----------------------+------------------------+
| Dimension                | In-Place (Rolling)    | Blue / Green          | Canary (Linear)        |
+--------------------------+-----------------------+-----------------------+------------------------+
| Downtime                 | Minimal / None        | Zero                  | Zero                   |
| Resource Cost            | Lowest (Same instances| Doubles during rollout| Doubles during rollout |
| Rollback Speed           | Slow (Re-deploy old)  | Instant (Swap pointer)| Instant (Abort canary) |
| Risk Level               | Moderate              | Low                   | Lowest                 |
| Primary Engine           | CodeDeploy / ASG      | CodeDeploy / Route 53 | CodeDeploy / ALB       |
+--------------------------+-----------------------+-----------------------+------------------------+
```

- **OIDC Federation**: The mandatory modern standard for securing third-party CI/CD runners.
- **Blue/Green & Canary**: Eliminates customer-facing downtime and provides instant automated rollback.
- **Immutable Artifacts**: Tag container images with Git SHAs, never mutable `latest` tags.

---

## 13. Further Reading

- [AWS CodePipeline User Guide](https://docs.aws.amazon.com/codepipeline/latest/userguide/)
- [AWS CodeDeploy User Guide](https://docs.aws.amazon.com/codedeploy/latest/userguide/)
- [Configuring OpenID Connect in Amazon Web Services (GitHub Docs)](https://docs.github.com/en/actions/deployment/security-hardening-your-deployments/configuring-openid-connect-in-amazon-web-services)
- [Blue/Green Deployments with AWS CodeDeploy and ECS](https://docs.aws.amazon.com/AmazonECS/latest/developerguide/deployment-type-bluegreen.html)
- [06-operations/01-infrastructure-as-code.md](file:///04-devops/aws/06-operations/01-infrastructure-as-code.md)
