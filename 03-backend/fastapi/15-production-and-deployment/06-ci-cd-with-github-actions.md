---
title: "CI/CD with GitHub Actions for FastAPI"
description: "Production CI/CD workflows: fast linting with Ruff, type checking with Pyright/Mypy, pytest execution with testcontainers, Docker Buildx caching, and automated deployment."
fastapi_version: "0.115+"
python_version: "3.12+"
uv_version: "0.5+"
free_threading_compatible: true
tested_with: "GitHub Actions, uv 0.5.5, Ruff 0.8.0, Pytest 8.3.4"
---

# CI/CD with GitHub Actions for FastAPI

## 1. Conceptual Foundation & Mental Model

A production Continuous Integration and Continuous Deployment (CI/CD) pipeline is the automated gatekeeper of software quality, security, and stability. In modern Python engineering, legacy pipelines that run slow `pip install` commands and take 10+ minutes to execute create friction, discourage frequent commits, and slow down incident response.

By leveraging **Astral uv**, **Ruff**, **Pyright/Mypy**, and **Docker Buildx**, a FastAPI pipeline can execute linting, strict static type checking, integration testing against real database containers, container image compilation, and cloud deployment in under 90 seconds.

The mental model organizes CI/CD into three progressive stages:
1. **Fast-Feedback Stage (Lint & Type Check)**: Executes in parallel in under 10 seconds. Ruff analyzes syntax, formatting, and security rules; Pyright/Mypy enforces type safety across all route signatures and Pydantic models.
2. **Verification Stage (Unit & Integration Testing)**: Spawns ephemeral real database services (PostgreSQL, Redis) via GitHub Actions service containers or `testcontainers-python`, running the Pytest suite with strict coverage thresholds.
3. **Artifact & Release Stage (Build & Deploy)**: Compiles multi-platform Docker images using BuildKit layer caching and deploys to target environments (Kubernetes, AWS ECS, Google Cloud Run) via OpenID Connect (OIDC) without long-lived static secrets.

```
                  [ Developer Pushes Git Commit ]
                                 │
                                 ▼
                  [ GitHub Actions Workflow Trigger ]
                                 │
         ┌───────────────────────┴───────────────────────┐
         ▼                                               ▼
[ Job: Ruff Lint & Format ]                    [ Job: Type Check (Pyright) ]
Runs in ~3 seconds                              Runs in ~6 seconds
         │                                               │
         └───────────────────────┬───────────────────────┘
                                 ▼
                 [ Job: Pytest Test Suite ]
                  - Service Container: PostgreSQL
                  - Service Container: Redis
                  - Executes Unit & Integration Tests
                                 │
                                 ▼ (Only on main branch)
                 [ Job: Docker Build & Push ]
                  - BuildKit Cache Mounts
                  - Pushes to Container Registry
                                 │
                                 ▼
                 [ Job: Deploy to Kubernetes ]
                  - Automated Rolling Deployment
```

---

## 2. Architecture & Internal Mechanics

### Secretless Cloud Authentication with OpenID Connect (OIDC)

Traditional CI/CD pipelines stored static, long-lived AWS IAM Access Keys or service account credentials in GitHub Secrets. If compromised, these credentials gave attackers persistent access to cloud infrastructure.

Modern pipelines use **OIDC Federation**:
1. The GitHub Actions job requests an ephemeral OIDC JSON Web Token (JWT) from GitHub's token authority.
2. The workflow exchanges this JWT with the cloud provider's STS (Security Token Service) (e.g. AWS IAM, GCP Workload Identity).
3. The cloud provider validates the token's cryptographic signature and repository claims (`repo:org/fastapi-app:ref:refs/heads/main`), granting temporary short-lived credentials (valid for 1 hour). Zero static cloud secrets are stored in GitHub!

```
+------------------+           +--------------------+           +------------------+
|  GitHub Actions  | ──(1)────►|  GitHub OIDC Auth  |           |   AWS / GCP STS  |
|  Runner          |           +---------│----------+           +--------│---------+
|                  | ◄──(2) JWT Token────┘                               │
|                  |                                                     │
|                  | ──(3) AssumeRoleWithWebIdentity (JWT)──────────────►│
|                  | ◄──(4) Temporary 1-Hour Credentials─────────────────┘
+------------------+
```

---

## 3. Comprehensive Syntax & Core Mechanics

### Production CI Workflow (`.github/workflows/ci.yml`)
```yaml
name: FastAPI Continuous Integration

on:
  push:
    branches: [main, develop]
  pull_request:
    branches: [main]

concurrency:
  group: ${{ github.workflow }}-${{ github.ref }}
  cancel-in-progress: true

jobs:
  lint-and-typecheck:
    name: Code Quality & Types
    runs-on: ubuntu-latest
    steps:
      - name: Checkout Repository
        uses: actions/checkout@v4

      - name: Install Astral uv
        uses: astral-sh/setup-uv@v4
        with:
          version: "0.5.5"
          enable-cache: true

      - name: Set up Python
        uses: actions/setup-python@v5
        with:
          python-version: "3.12"

      - name: Install Dependencies
        run: uv sync --frozen --all-extras

      - name: Run Ruff Linter
        run: uv run ruff check .

      - name: Run Ruff Format Check
        run: uv run ruff format --check .

      - name: Run Pyright Type Checker
        run: uv run pyright

  unit-and-integration-tests:
    name: Pytest Suite
    runs-on: ubuntu-latest
    needs: [lint-and-typecheck]
    services:
      postgres:
        image: postgres:16-alpine
        env:
          POSTGRES_DB: test_db
          POSTGRES_USER: test_user
          POSTGRES_PASSWORD: test_password
        ports:
          - 5432:5432
        options: >-
          --health-cmd pg_isready
          --health-interval 5s
          --health-timeout 5s
          --health-retries 5

      redis:
        image: redis:7-alpine
        ports:
          - 6379:6379
        options: >-
          --health-cmd "redis-cli ping"
          --health-interval 5s
          --health-timeout 5s
          --health-retries 5

    env:
      APP_ENV: testing
      DATABASE_URL: postgresql+asyncpg://test_user:test_password@localhost:5432/test_db
      REDIS_URL: redis://localhost:6379/0
      JWT_SECRET_KEY: test_secret_key_for_ci_pipeline_only_32_chars

    steps:
      - name: Checkout Repository
        uses: actions/checkout@v4

      - name: Install Astral uv
        uses: astral-sh/setup-uv@v4
        with:
          version: "0.5.5"
          enable-cache: true

      - name: Set up Python
        uses: actions/setup-python@v5
        with:
          python-version: "3.12"

      - name: Install Dependencies
        run: uv sync --frozen --all-extras

      - name: Run Database Migrations (Alembic)
        run: uv run alembic upgrade head

      - name: Execute Tests with Pytest
        run: uv run pytest --cov=app --cov-report=xml --cov-fail-under=85

      - name: Upload Coverage Artifact
        uses: actions/upload-artifact@v4
        with:
          name: coverage-report
          path: coverage.xml
```

---

## 4. Practical Implementation: 5 Production Scenarios

### Scenario 1: Continuous Delivery with Docker Buildx and GitHub Container Registry (GHCR)
```yaml
# .github/workflows/cd.yml
name: Build and Push Container

on:
  push:
    branches: [main]

permissions:
  contents: read
  packages: write

jobs:
  build-and-push:
    runs-on: ubuntu-latest
    steps:
      - name: Checkout
        uses: actions/checkout@v4

      - name: Set up Docker Buildx
        uses: docker/setup-buildx-action@v3

      - name: Log in to GHCR
        uses: docker/login-action@v3
        with:
          registry: ghcr.io
          username: ${{ github.actor }}
          password: ${{ secrets.GITHUB_TOKEN }}

      - name: Extract Metadata
        id: meta
        uses: docker/metadata-action@v5
        with:
          images: ghcr.io/${{ github.repository }}
          tags: |
            type=sha,format=long
            type=ref,event=branch
            latest

      - name: Build and Push Docker Image
        uses: docker/build-push-action@v5
        with:
          context: .
          push: true
          tags: ${{ steps.meta.outputs.tags }}
          labels: ${{ steps.meta.outputs.labels }}
          cache-from: type=gha
          cache-to: type=gha,mode=max
```

### Scenario 2: Secretless AWS Deployment via OIDC Federation
```yaml
name: Deploy to Amazon ECS

on:
  workflow_run:
    workflows: ["Build and Push Container"]
    types: [completed]
    branches: [main]

permissions:
  id-token: write
  contents: read

jobs:
  deploy:
    runs-on: ubuntu-latest
    if: ${{ github.event.workflow_run.conclusion == 'success' }}
    steps:
      - name: Configure AWS Credentials via OIDC
        uses: aws-actions/configure-aws-credentials@v4
        with:
          role-to-assume: arn:aws:iam::123456789012:role/GitHubActionsFastAPIDeployer
          aws-region: us-east-1

      - name: Update ECS Service
        run: |
          aws ecs update-service \
            --cluster production-cluster \
            --service fastapi-api-service \
            --force-new-deployment
```

### Scenario 3: Automated Security Scanning with Trivy
```yaml
  security-vulnerability-scan:
    name: Container Vulnerability Scan
    runs-on: ubuntu-latest
    needs: [build-and-push]
    steps:
      - name: Run Trivy Vulnerability Scanner
        uses: aquasecurity/trivy-action@master
        with:
          image-ref: ghcr.io/${{ github.repository }}:${{ github.sha }}
          format: "table"
          exit-code: "1"  # Fail build if CRITICAL vulnerabilities exist
          ignore-unfixed: true
          severity: "CRITICAL,HIGH"
```

### Scenario 4: Automated Canary Deployments with Kubernetes
```yaml
  deploy-canary:
    runs-on: ubuntu-latest
    steps:
      - name: Set Kubernetes Context
        uses: azure/k8s-set-context@v3
        with:
          method: kubeconfig
          kubeconfig: ${{ secrets.KUBECONFIG }}

      - name: Update Canary Image Tag
        run: |
          kubectl set image deployment/fastapi-canary \
            api=ghcr.io/${{ github.repository }}:${{ github.sha }} \
            -n production
```

### Scenario 5: Automated Dependency Updates with Dependabot or Renovate
```yaml
# .github/dependabot.yml
version: 2
updates:
  - package-ecosystem: "github-actions"
    directory: "/"
    schedule:
      interval: "weekly"

  - package-ecosystem: "pip"
    directory: "/"
    schedule:
      interval: "weekly"
    open-pull-requests-limit: 10
```

---

## 5. Edge Cases, Pitfalls & Failure Modes

1. **Flaky CI Runs from Database Race Conditions**:
   When tests run in parallel (e.g. `pytest -n auto`), tests that write to the same database tables concurrently will collide, causing intermittent test failures. Use unique database schemas per worker (`f"test_schema_{worker_id}"`) or rely on transactional rollbacks.

2. **GitHub Actions Cache Poisoning**:
   If the BuildKit cache backend (`type=gha`) becomes corrupted, subsequent builds may fail cryptographically or incorporate stale artifacts. You can purge caches using the GitHub CLI (`gh cache delete <key>`).

3. **Concurrency Race Conditions on Rapid Git Pushes**:
   If a developer pushes three commits in rapid succession, all three builds run simultaneously. If commit 1 finishes *after* commit 3, an older image tag can overwrite the newer image. Use `concurrency: group: ... cancel-in-progress: true` to terminate stale runs immediately.

4. **Missing OIDC Permissions**:
   Failing to set `permissions: id-token: write` in the workflow YAML prevents GitHub from issuing the JWT token, resulting in obscure `credentials could not be retrieved` errors.

---

## 6. Anti-Patterns & Common Mistakes

| Anti-Pattern | Operational Impact | Correct Pattern |
| :--- | :--- | :--- |
| Using long-lived static AWS/cloud access keys in GitHub | High risk of permanent infrastructure compromise | Use OIDC Workload Identity Federation |
| Running `pip install` without layer or wheel caching | CI builds take 8-12 minutes per pull request | Use Astral uv with `enable-cache: true` |
| Skipping integration tests in CI | Bugs in database schema migrations slip into production | Use GitHub Actions service containers or testcontainers |
| Running `cancel-in-progress: false` on feature branches | Stale commits consume CI runner minutes and queue slots | Set `concurrency: cancel-in-progress: true` |
| Omitting `--cov-fail-under` gate | Test coverage quietly degrades over time | Enforce minimum test coverage threshold (e.g. 85%) |
| Building Docker containers without BuildKit cache | Every image rebuild takes minutes downloading base layers | Use `cache-from: type=gha` and `cache-to: type=gha` |

---

## 7. Performance & Scalability Considerations

- **Pipeline Duration Target**: High-performing engineering teams target PR feedback in under 2 minutes. Using `uv` and `ruff` achieves this comfortably.
- **Matrix Builds for Multi-Version Testing**: When building shared libraries or SDKs, use GitHub Actions matrix strategies to test across Python versions:
```yaml
strategy:
  matrix:
    python-version: ["3.12", "3.13"]
```

---

## 8. Security & OWASP Guardrails

- **Principle of Least Privilege for GitHub Tokens**: By default, restrict the default repository token:
  `permissions: contents: read`
- **Supply Chain Security**: Pin third-party GitHub Actions by full commit SHA rather than floating branch tags:
  `uses: actions/checkout@b4ffde65f46336ab88eb53be808477a3936bae11 # v4.1.1`

---

## 9. Testing Strategies & Pytest Recipes

```python
import os
import pytest

def test_verify_ci_environment_configuration():
    # Verify that CI runs under isolated testing settings
    app_env = os.getenv("APP_ENV")
    assert app_env == "testing", f"Expected APP_ENV=testing, got {app_env}"

    db_url = os.getenv("DATABASE_URL")
    assert db_url is not None
    assert "test" in db_url.lower(), "Danger: CI is not connected to a test database!"
```

---

## 10. Real-World Production Incident

**Incident**: An e-commerce company deployed a broken database migration to production during peak hours, causing a 25-minute outage and corrupting user order records.

**Root Cause**: The CI pipeline ran unit tests using SQLite in-memory, while production used PostgreSQL. A developer added a PostgreSQL-specific JSONB index syntax that passed unit tests on SQLite. Because the CI pipeline did not run integration tests against a real PostgreSQL instance, the syntax error was only discovered when Alembic attempted to execute the migration in production.

**Resolution**: Updated GitHub Actions to run PostgreSQL and Redis service containers. Added an explicit `alembic upgrade head` verification step in the pipeline. All database migrations are now validated against an exact replica of the production database engine before code can be merged.

---

## 11. Technical Interview Questions

<details>
<summary>1. How does GitHub Actions OIDC eliminate the need for static cloud access keys?</summary>

GitHub Actions acts as an OpenID Connect (OIDC) Identity Provider. When a job runs, GitHub signs a temporary JWT containing claims about the repository, branch, and environment. The workflow presents this token to AWS STS, GCP, or Azure. The cloud provider verifies the signature against GitHub's public keys and checks that the claims match an IAM trust policy. If valid, it issues temporary, short-lived security credentials (valid for 1 hour). No static credentials are ever stored or rotated in GitHub Secrets.
</details>

<details>
<summary>2. Why is Astral uv dramatically faster than traditional pip in CI environments?</summary>

`uv` is written in Rust and implements parallel dependency resolution, aggressive HTTP connection pipelining, and a global content-addressable cache. While `pip` resolves dependencies sequentially in pure Python, `uv` resolves and downloads packages in parallel, utilizing all available CPU cores and network bandwidth. Furthermore, `setup-uv` integrates directly with GitHub Actions caching to restore wheels in seconds.
</details>

<details>
<summary>3. What is the purpose of the <code>concurrency</code> key with <code>cancel-in-progress: true</code>?</summary>

If a developer pushes multiple commits to a pull request in rapid succession, each commit triggers a separate workflow run. `concurrency: group: ... cancel-in-progress: true` automatically cancels any in-flight workflow runs for previous commits on that branch, saving CI runner minutes and preventing race conditions during artifact deployment.
</details>

<details>
<summary>4. What are GitHub Actions Service Containers and when should they be used?</summary>

Service Containers are auxiliary Docker containers managed directly by GitHub Actions alongside the job runner. They provide ephemeral instances of network dependencies—such as PostgreSQL, MySQL, Redis, or RabbitMQ—pre-configured with health checks. They allow integration tests to execute against real database engines rather than mocks, catching database-specific bugs and schema migration errors before deployment.
</details>

<details>
<summary>5. How does Docker Buildx GitHub Actions caching (<code>type=gha</code>) work?</summary>

Buildx with `cache-from: type=gha` and `cache-to: type=gha,mode=max` stores intermediate Docker build layers directly in GitHub's Actions cache backend. When a new build runs, BuildKit queries the GitHub cache and downloads only the layers that have changed, drastically reducing image build times on remote cloud runners.
</details>

<details>
<summary>6. Why should database migrations be verified in CI prior to deploying code?</summary>

Application code is often tightly coupled to database schema structure. If a migration script contains syntax errors, invalid default values, or conflicting locks, running the migration during production deployment can fail midway, leaving the database in an inconsistent state. Verifying migrations in CI against an ephemeral database ensures migration scripts execute cleanly from scratch.
</details>

<details>
<summary>7. What is the danger of using floating branch tags like <code>actions/checkout@v4</code>?</summary>

A floating tag (like `@v4` or `@main`) points to a moving Git reference. If an attacker compromises the upstream repository or the action maintainer accidentally introduces a breaking change, the compromised code executes automatically in your pipeline. Pinning actions to immutable full commit SHAs (e.g. `@b4ffde65f...`) guarantees cryptographic immutability.
</details>

<details>
<summary>8. How do you prevent test database state pollution when running tests in parallel in CI?</summary>

When running tests in parallel via `pytest-xdist` (`-n auto`), multiple workers access the database concurrently. To prevent data contamination, each worker can be assigned a dedicated database schema or database name using the `PYTEST_XDIST_WORKER` environment variable (e.g., `test_db_gw0`, `test_db_gw1`), isolating transactions completely.
</details>

---

## 12. Practical Exercises & Solutions

### Exercise 1 (Easy): Basic Ruff Linting Workflow Step
**Task**: Write the GitHub Actions step that runs `ruff check` and `ruff format --check` using Astral uv.
<details>
<summary>Solution</summary>

```yaml
- name: Run Ruff Lint & Format Checks
  run: |
    uv run ruff check .
    uv run ruff format --check .
```
</details>

### Exercise 2 (Easy): Configure Concurrency Group in YAML
**Task**: Write the YAML block to cancel running builds on the same PR when a new commit is pushed.
<details>
<summary>Solution</summary>

```yaml
concurrency:
  group: ${{ github.workflow }}-${{ github.head_ref || github.run_id }}
  cancel-in-progress: true
```
</details>

### Exercise 3 (Medium): Add Redis Service Container to Pytest Job
**Task**: Configure a Redis service container in GitHub Actions with health check validation on port 6379.
<details>
<summary>Solution</summary>

```yaml
services:
  redis:
    image: redis:7-alpine
    ports:
      - 6379:6379
    options: >-
      --health-cmd "redis-cli ping"
      --health-interval 5s
      --health-timeout 5s
      --health-retries 5
```
</details>

### Exercise 4 (Medium): Pytest Coverage Gate Step
**Task**: Write the step to run pytest with code coverage tracking on directory `app` and fail the build if coverage is below 90%.
<details>
<summary>Solution</summary>

```yaml
- name: Run Tests with Coverage Gate
  run: uv run pytest --cov=app --cov-report=term-missing --cov-fail-under=90
```
</details>

### Exercise 5 (Hard): Complete OIDC AWS Role Assumption Job
**Task**: Write a complete GitHub Actions job that requests an OIDC token and assumes an AWS IAM role in `us-west-2` without any static credentials.
<details>
<summary>Solution</summary>

```yaml
jobs:
  assume-aws-role:
    runs-on: ubuntu-latest
    permissions:
      id-token: write
      contents: read
    steps:
      - name: Configure AWS Credentials via OIDC
        uses: aws-actions/configure-aws-credentials@v4
        with:
          role-to-assume: arn:aws:iam::987654321012:role/GitHubActionsDeploymentRole
          role-session-name: GitHubActionsFastAPISession
          aws-region: us-west-2

      - name: Verify Caller Identity
        run: aws sts get-caller-identity
```
</details>

### Exercise 6 (Hard): Multi-Stage Buildx Push with Trivy Scan
**Task**: Write a GitHub Actions job that builds a Docker image using Buildx GHA caching, scans it for HIGH and CRITICAL vulnerabilities using Trivy, and pushes the image only if the scan passes.
<details>
<summary>Solution</summary>

```yaml
jobs:
  build-scan-push:
    runs-on: ubuntu-latest
    permissions:
      contents: read
      packages: write
    steps:
      - name: Checkout
        uses: actions/checkout@v4

      - name: Set up Docker Buildx
        uses: docker/setup-buildx-action@v3

      - name: Log in to GHCR
        uses: docker/login-action@v3
        with:
          registry: ghcr.io
          username: ${{ github.actor }}
          password: ${{ secrets.GITHUB_TOKEN }}

      - name: Build Image (Local Load for Scan)
        uses: docker/build-push-action@v5
        with:
          context: .
          load: true
          tags: ghcr.io/${{ github.repository }}:${{ github.sha }}
          cache-from: type=gha

      - name: Scan Image with Trivy
        uses: aquasecurity/trivy-action@master
        with:
          image-ref: ghcr.io/${{ github.repository }}:${{ github.sha }}
          format: "table"
          exit-code: "1"
          ignore-unfixed: true
          severity: "CRITICAL,HIGH"

      - name: Push Verified Image
        uses: docker/build-push-action@v5
        with:
          context: .
          push: true
          tags: ghcr.io/${{ github.repository }}:${{ github.sha }}
          cache-to: type=gha,mode=max
```
</details>
