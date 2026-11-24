# CI/CD Pipelines for Spring Boot Microservices

| Metadata | Value |
|---|---|
| **Spring Boot Version** | 3.4.x |
| **Spring Framework Version** | 6.2.x |
| **Java Version** | 21 LTS |
| **Module** | 14-production-and-deployment |
| **Status** | Complete |

---

## Table of Contents
- [1. What & Why](#1-what--why)
- [2. Core Concepts](#2-core-concepts)
- [3. How It Works Internally](#3-how-it-works-internally)
- [4. Syntax & Workflow Pipeline Reference](#4-syntax--workflow-pipeline-reference)
- [5. Practical Examples](#5-practical-examples)
- [6. Best Practices & Production Guidelines](#6-best-practices--production-guidelines)
- [7. Common Mistakes & Pitfalls](#7-common-mistakes--pitfalls)
- [8. Performance & Security Notes](#8-performance--security-notes)
- [9. Comparison / When to Use What](#9-comparison--when-to-use-what)
- [10. Interview Questions & Answers](#10-interview-questions--answers)
- [11. Practice Exercises](#11-practice-exercises)
- [12. Summary / Cheat Sheet](#12-summary--cheat-sheet)
- [13. Further Reading](#13-further-reading)

---

## 1. What & Why

### Problem Statement
In modern cloud engineering, manual deployments—compiling JARs on local developer machines, manually executing `docker build`, and applying manifests via `kubectl apply`—are the root cause of production incidents:
- Untested code changes bypass quality gates and break production databases.
- Containers built on developer laptops contain uncommitted local files, unvetted binaries, and inconsistent JDK patch versions.
- Security vulnerabilities and leaked API keys go undetected until post-incident forensics.
- Release rollbacks are slow and error-prone when deployment history is unmanaged.

### Historical Evolution
Continuous Integration and Continuous Deployment (CI/CD) transitioned from centralized, heavy Jenkins master-slave clusters to lightweight, container-native **declarative YAML pipelines** (GitHub Actions, GitLab CI, Tekton, ArgoCD).

Modern Spring Boot 3.x delivery pipelines integrate:
- Fast multi-threaded compilation with Maven/Gradle caching.
- Ephemeral integration testing using **Testcontainers**.
- Automated code coverage gates via **JaCoCo**.
- Daemonless OCI container generation via **Google Jib** or **Paketo Buildpacks**.
- Container vulnerability scanning via **Trivy**.
- **GitOps** synchronization via **ArgoCD**.

### Production Relevance
An automated CI/CD pipeline guarantees that:
- Every Git pull request is validated against strict automated quality, security, and coverage gates.
- Ephemeral test containers (PostgreSQL, Kafka, Redis) spin up and tear down cleanly in CI without shared database collisions.
- Production releases are fully reproducible, immutable, and deployed automatically via declarative GitOps.

---

## 2. Core Concepts

### Stages of a Modern Microservice Pipeline
1. **Linting & Code Quality**: Static analysis verifying code standards, formatting, and security rules (Checkstyle, SpotBugs, SonarQube).
2. **Unit & Slice Testing**: Fast isolated tests executed via Maven Surefire (`mvn test`), validating controllers, services, and repository contracts.
3. **Integration Testing (Testcontainers)**: Comprehensive end-to-end component tests using real Dockerized databases and brokers via Maven Failsafe (`mvn verify`).
4. **Code Coverage Gates (JaCoCo)**: Automated verification enforcing minimum instruction and branch coverage thresholds (e.g. $\ge 80\%$).
5. **Security Scanning**: Static analysis scanning for hardcoded secrets (TruffleHog) and container vulnerability scanning (Trivy).
6. **Container Packaging**: Building OCI-compliant images using daemonless tools (Google Jib) or Cloud Native Buildpacks and pushing to AWS ECR / Harbor.
7. **GitOps Continuous Delivery**: Committing the updated image tag to a dedicated GitOps repository, prompting **ArgoCD** to sync the change to the Kubernetes cluster.

---

## 3. How It Works Internally

### Staged CI/CD Pipeline Architecture (GitHub Actions to ArgoCD)

```
GIT COMMIT PUSH (Developer)
  |
  v
+------------------------------------------------------------------------------------+
| STAGE 1: BUILD, LINT & UNIT TEST (Matrix: Java 21)                                 |
| - Restores Maven ~/.m2 cache                                                       |
| - Executes Checkstyle, SpotBugs static analysis                                    |
| - Runs unit tests via Maven Surefire (mvn test)                                    |
+------------------------------------------------------------------------------------+
  |
  v (Pass)
+------------------------------------------------------------------------------------+
| STAGE 2: INTEGRATION TESTING & COVERAGE (Testcontainers)                          |
| - Spins up real PostgreSQL 16 & Kafka containers via Testcontainers                |
| - Runs Maven Failsafe (mvn verify)                                                 |
| - JaCoCo verifies coverage >= 80% (Fails pipeline if threshold breached!)          |
+------------------------------------------------------------------------------------+
  |
  v (Pass)
+------------------------------------------------------------------------------------+
| STAGE 3: SECURITY SCANNING & OCI PACKAGING                                         |
| - Scans repository for secrets (TruffleHog)                                        |
| - Builds container image using Google Jib (Daemonless, distroless base)            |
| - Trivy scans container image for CRITICAL/HIGH CVEs                               |
| - Pushes image to Amazon ECR: registry/order-service:${{ github.sha }}             |
+------------------------------------------------------------------------------------+
  |
  v (Pass)
+------------------------------------------------------------------------------------+
| STAGE 4: GITOPS DEPLOYMENT (ArgoCD Sync)                                           |
| - Updates values.yaml in gitops-manifests repo with new image tag (${github.sha})  |
| - ArgoCD detects Git commit -> executes zero-downtime rolling update in K8s!       |
+------------------------------------------------------------------------------------+
```

---

## 4. Syntax & Workflow Pipeline Reference

### Complete GitHub Actions Workflow (`.github/workflows/ci-cd.yml`)

```yaml
name: Spring Boot Microservice CI/CD

on:
  push:
    branches: [ main ]
  pull_request:
    branches: [ main ]

permissions:
  contents: write
  id-token: write # Required for AWS OIDC authentication

jobs:
  # -------------------------------------------------------------
  # Job 1: Build, Test & Quality Gates
  # -------------------------------------------------------------
  build-and-test:
    runs-on: ubuntu-latest
    steps:
      - name: Checkout Code
        uses: actions/checkout@v4

      - name: Set up Java 21 LTS
        uses: actions/setup-java@v4
        with:
          java-version: '21'
          distribution: 'temurin'
          cache: maven

      - name: Run Unit Tests & Verify Coverage
        run: ./mvnw clean verify -Pcoverage

      - name: Upload JaCoCo Coverage Report
        uses: actions/upload-artifact@v4
        with:
          name: jacoco-report
          path: target/site/jacoco/

  # -------------------------------------------------------------
  # Job 2: Security Vulnerability Scanning
  # -------------------------------------------------------------
  security-scan:
    needs: build-and-test
    runs-on: ubuntu-latest
    steps:
      - name: Checkout Code
        uses: actions/checkout@v4

      - name: Run Secret Scanning (TruffleHog)
        uses: trufflesecurity/trufflehog@main
        with:
          path: ./
          base: ${{ github.event.repository.default_branch }}
          head: HEAD

  # -------------------------------------------------------------
  # Job 3: Package Container & Deploy (Runs only on main branch)
  # -------------------------------------------------------------
  package-and-deploy:
    needs: [ build-and-test, security-scan ]
    if: github.ref == 'refs/heads/main' && github.event_name == 'push'
    runs-on: ubuntu-latest
    steps:
      - name: Checkout Code
        uses: actions/checkout@v4

      - name: Set up Java 21 LTS
        uses: actions/setup-java@v4
        with:
          java-version: '21'
          distribution: 'temurin'
          cache: maven

      - name: Configure AWS Credentials (OIDC - No long-lived keys!)
        uses: aws-actions/configure-aws-credentials@v4
        with:
          role-to-assume: arn:aws:iam::123456789012:role/GitHubActionsECRRole
          aws-region: us-east-1

      - name: Log in to Amazon ECR
        id: login-ecr
        uses: aws-actions/amazon-ecr-login@v2

      - name: Build and Push Container Image via Jib
        env:
          REGISTRY: ${{ steps.login-ecr.outputs.registry }}
          IMAGE_TAG: ${{ github.sha }}
        run: |
          ./mvnw compile com.google.cloud.tools:jib-maven-plugin:build \
            -Dimage=$REGISTRY/order-service:$IMAGE_TAG \
            -DskipTests

      - name: Scan Built Image with Trivy
        uses: aquasecurity/trivy-action@master
        with:
          image-ref: ${{ steps.login-ecr.outputs.registry }}/order-service:${{ github.sha }}
          format: 'table'
          exit-code: '1' # Fails build if CRITICAL CVEs are found!
          ignore-unfixed: true
          severity: 'CRITICAL,HIGH'

      - name: Update GitOps Repository Image Tag
        env:
          GITOPS_TOKEN: ${{ secrets.GITOPS_REPO_TOKEN }}
        run: |
          git config --global user.name "github-actions-bot"
          git config --global user.email "bot@company.com"
          git clone https://x-access-token:${GITOPS_TOKEN}@github.com/myorg/gitops-manifests.git /tmp/gitops
          cd /tmp/gitops/apps/order-service
          sed -i "s/tag: .*/tag: \"${{ github.sha }}\"/" values.yaml
          git add values.yaml
          git commit -m "chore(order-service): update image tag to ${{ github.sha }}"
          git push origin main
```

---

## 5. Practical Examples

### Example 1: Enforcing Coverage Gates in Maven with JaCoCo (`pom.xml`)
```xml
<plugin>
    <groupId>org.jacoco</groupId>
    <artifactId>jacoco-maven-plugin</artifactId>
    <version>0.8.12</version>
    <executions>
        <execution>
            <goals>
                <goal>prepare-agent</goal>
            </goals>
        </execution>
        <execution>
            <id>report</id>
            <phase>verify</phase>
            <goals>
                <goal>report</goal>
            </goals>
        </execution>
        <!-- Enforce 80% instruction coverage gate -->
        <execution>
            <id>check</id>
            <phase>verify</phase>
            <goals>
                <goal>check</goal>
            </goals>
            <configuration>
                <rules>
                    <rule>
                        <element>BUNDLE</element>
                        <limits>
                            <limit>
                                <counter>INSTRUCTION</counter>
                                <value>COVEREDRATIO</value>
                                <minimum>0.80</minimum>
                            </limit>
                        </limits>
                    </rule>
                </rules>
            </configuration>
        </execution>
    </executions>
</plugin>
```

### Example 2: Testcontainers Execution in GitHub Actions Runners
When running Testcontainers inside GitHub Actions runners:

```java
package com.example.production.integration;

import org.junit.jupiter.api.Test;
import org.springframework.boot.test.context.SpringBootTest;
import org.springframework.boot.testcontainers.service.connection.ServiceConnection;
import org.testcontainers.containers.PostgreSQLContainer;
import org.testcontainers.junit.jupiter.Container;
import org.testcontainers.junit.jupiter.Testcontainers;

import static org.assertj.core.api.Assertions.assertThat;

@SpringBootTest(webEnvironment = SpringBootTest.WebEnvironment.RANDOM_PORT)
@Testcontainers
class OrderServiceIntegrationTest {

    // Automatically wires Spring Boot DataSource to ephemeral container!
    @Container
    @ServiceConnection
    static PostgreSQLContainer<?> postgres = new PostgreSQLContainer<>("postgres:16-alpine");

    @Test
    void databaseStartsAndContextBoots() {
        assertThat(postgres.isRunning()).isTrue();
    }
}
```

### Example 3: ArgoCD Application Custom Resource (`order-service-app.yaml`)
In a GitOps workflow, ArgoCD continuously monitors the Git repository and updates Kubernetes:

```yaml
apiVersion: argoproj.io/v1alpha1
kind: Application
metadata:
  name: order-service-prod
  namespace: argocd
spec:
  project: default
  source:
    repoURL: 'https://github.com/myorg/gitops-manifests.git'
    targetRevision: main
    path: apps/order-service
    helm:
      valueFiles:
        - values.yaml
        - values-prod.yaml
  destination:
    server: 'https://kubernetes.default.svc'
    namespace: production
  syncPolicy:
    automated:
      prune: true     # Deletes obsolete resources removed from Git
      selfHeal: true  # Reverts manual configuration drift automatically
    syncOptions:
      - CreateNamespace=true
```

### Example 4: Anti-Pattern vs. Production Fix (Hardcoded AWS Credentials in CI)
```yaml
# ANTI-PATTERN: Using long-lived static AWS access keys in CI secrets
# High Security Risk: If leaked, keys grant permanent attacker access until revoked!
- name: Configure AWS Credentials
  uses: aws-actions/configure-aws-credentials@v4
  with:
    aws-access-key-id: ${{ secrets.AWS_ACCESS_KEY_ID }}
    aws-secret-access-key: ${{ secrets.AWS_SECRET_ACCESS_KEY }}

# PRODUCTION FIX: OpenID Connect (OIDC) Federated IAM Authentication
# Generates short-lived, ephemeral STS tokens valid for 1 hour; ZERO static credentials!
- name: Configure AWS Credentials (OIDC)
  uses: aws-actions/configure-aws-credentials@v4
  with:
    role-to-assume: arn:aws:iam::123456789012:role/GitHubActionsECRRole
    aws-region: us-east-1
```

### Example 5: Caching Maven Dependencies in GitHub Actions
```yaml
# Caches ~/.m2 repository based on pom.xml hash, reducing build time by 3-5 minutes!
- name: Set up Java & Cache Maven Dependencies
  uses: actions/setup-java@v4
  with:
    java-version: '21'
    distribution: 'temurin'
    cache: maven
```

---

## 6. Best Practices & Production Guidelines

1. **Use OIDC for Cloud Authentication**: Eliminate static, long-lived cloud credentials (`AWS_ACCESS_KEY_ID`, service account JSON keys) from CI/CD secret vaults. Standardize on OpenID Connect (OIDC) to exchange GitHub/GitLab identity tokens for short-lived cloud credentials.
2. **Never Build on Developer Laptops**: All production container images must be built, tagged, scanned, and published strictly by automated CI runners from version-controlled Git tags or main branch commits.
3. **Adopt the GitOps Pull Model (ArgoCD / Flux)**: Do not grant CI build runners direct `kubectl` access or cluster admin credentials to production Kubernetes clusters. Use GitOps controllers running *inside* the cluster that pull changes from a dedicated config repository.
4. **Enforce Static Analysis and Coverage Gates**: Configure pipelines to automatically reject pull requests if test coverage drops below thresholds (e.g. 80%) or if static analyzers flag security bugs.
5. **Fail Builds on Critical Vulnerabilities (CVEs)**: Run automated container image vulnerability scans (Trivy, Grype) with `--exit-code 1` on `CRITICAL` severities before pushing to production registries.

---

## 7. Common Mistakes & Pitfalls

| Mistake | Why It Fails | Practical Remedy |
|---|---|---|
| Direct `kubectl apply` from CI runner | Grants broad cluster credentials to CI runner; violates principle of least privilege. | Use GitOps (ArgoCD/Flux) where cluster pulls manifests from Git. |
| Tagging container images as `:latest` | Kubernetes caches images by default; `:latest` prevents rollbacks and creates non-reproducible releases. | Tag images immutably with the Git commit SHA: `:${{ github.sha }}`. |
| Skipping integration tests in CI | In-memory mocks pass, but real database queries fail in production due to dialect differences. | Run real databases via Testcontainers in CI pipelines. |
| Storing static cloud keys in repository secrets | Stolen tokens grant indefinite access; credential rotation is painful. | Migrate to AWS/GCP OIDC federation. |
| Omitting Maven dependency caching | Every build downloads the entire internet (hundreds of MBs of dependencies), slowing builds to 10+ minutes. | Configure `cache: maven` in `actions/setup-java`. |
| Running tests sequentially on single core | Long integration test suites stall developer feedback loops. | Run tests with Maven Surefire fork concurrency: `<forkCount>2</forkCount>`. |

---

## 8. Performance & Security Notes

### Ephemeral Runner Security
- GitHub Actions hosted runners are ephemeral VMs destroyed immediately after job completion, guaranteeing a clean build environment.
- When using self-hosted runners on Kubernetes, ensure pods are ephemeral or run unprivileged to prevent lateral cluster movement.

### Supply Chain Security (SLSA Framework)
- Protect against supply chain poisoning by signing container images with **Cosign** (Sigstore).
- Kubernetes clusters can use admission controllers (Kyverno, OPA Gatekeeper) to reject any container image not signed by your enterprise CI/CD pipeline key.

---

## 9. Comparison / When to Use What

| CI/CD Component | GitHub Actions / GitLab CI | Jenkins (Legacy) | ArgoCD / Flux (GitOps) |
|---|---|---|---|
| **Primary Domain** | CI (Compile, Test, Package, Scan) | CI (Legacy VM master-agent) | **CD (Kubernetes Cluster Deployment)** |
| **Execution Model** | Ephemeral cloud/container runners | Persistent worker VMs | In-cluster reconciliation loop |
| **Cluster Credentials** | Needs zero cluster credentials (with GitOps)| Often requires broad cluster admin keys| Runs inside cluster securely |
| **Drift Detection** | No | No | **Yes (Reverts manual kubectl edits)** |

---

## 10. Interview Questions & Answers

### Q1: What is GitOps and how does it differ from traditional CI/CD push deployments?
<details>
<summary>Click to view answer</summary>

- **Traditional Push Model**: The CI server (GitHub Actions, Jenkins) builds the image and uses direct `kubectl` or Helm commands to push the update into the production Kubernetes cluster. This requires granting broad, privileged cluster credentials to external CI runners.
- **GitOps Pull Model**: The CI pipeline builds the image and updates the desired image tag in a dedicated Git repository. An in-cluster GitOps controller (like **ArgoCD**) continuously compares the live cluster state against the Git repository. When a diff is detected, ArgoCD pulls the update and reconciles the cluster automatically, eliminating the need to expose cluster credentials to external CI systems.
</details>

### Q2: Why is OpenID Connect (OIDC) preferred over static cloud credentials in CI/CD?
<details>
<summary>Click to view answer</summary>

Static credentials (e.g. `AWS_ACCESS_KEY_ID` / `AWS_SECRET_ACCESS_KEY`) are long-lived secrets stored in repository settings. If an attacker gains access or a job leaks logs, the credentials remain valid until manually revoked.  
With OIDC, GitHub Actions requests an ephemeral cryptographic token signed by GitHub's OIDC provider. Cloud providers (AWS, GCP, Azure) validate the token against pre-configured trust policies and issue short-lived credentials (valid for 1 hour) specifically scoped to that single build job, eliminating secret storage.
</details>

### Q3: How does Testcontainers execute inside a containerized GitHub Actions runner?
<details>
<summary>Click to view answer</summary>

GitHub Actions standard hosted runners (`ubuntu-latest`) have the Docker daemon pre-installed on the host VM. When Maven Failsafe executes, the Testcontainers Java library communicates with the host Docker socket (`/var/run/docker.sock`) to pull images (e.g. `postgres:16-alpine`), spawn ephemeral containers, map random ports, and destroy the containers immediately upon test completion.
</details>

### Q4: Why should production container images NEVER be tagged with `:latest`?
<details>
<summary>Click to view answer</summary>

1. **Non-Reproducible**: `:latest` is a mutable pointer. You cannot know which exact Git commit is running in production.
2. **Kubernetes Caching Bug**: If `imagePullPolicy` is set to `IfNotPresent`, Kubernetes worker nodes will not pull new versions if `:latest` already exists locally.
3. **Broken Rollbacks**: Reverting a release with `helm rollback` or `kubectl rollout undo` is impossible because both the old and new releases point to the same `:latest` tag string.  
Always tag images immutably with the Git commit SHA or semantic version (e.g. `:v2.1.0-4bf92f`).
</details>

### Q5: What is the purpose of JaCoCo verification rules in Maven?
<details>
<summary>Click to view answer</summary>

The `jacoco:check` goal evaluates code coverage against defined mathematical thresholds (e.g. `COVEREDRATIO < 0.80`). If a developer submits a pull request with new business logic but fails to write corresponding tests, JaCoCo fails the Maven `verify` build phase, which automatically blocks the CI pipeline from passing and prevents merging untested code.
</details>

### Q6: How does Trivy scan container images for vulnerabilities in CI?
<details>
<summary>Click to view answer</summary>

Trivy downloads an updated vulnerability database and inspects container image layers, parsing both OS packages (RPM, Debian, Alpine) and language-specific dependency manifests (Maven `pom.xml`, JAR metadata). If known CVEs matching configured severity thresholds (`CRITICAL`, `HIGH`) are detected, Trivy outputs a report table and exits with code 1, halting the pipeline.
</details>

### Q7: What is "Configuration Drift" and how does ArgoCD self-healing resolve it?
<details>
<summary>Click to view answer</summary>

Configuration drift occurs when an engineer manually alters live Kubernetes resources (e.g. running `kubectl edit deployment` or manually tweaking replica counts) without updating Git. With ArgoCD's `selfHeal: true` enabled, ArgoCD detects the discrepancy between Git (the single source of truth) and the cluster, and immediately overwrites the manual changes, bringing the cluster back into compliance with the version-controlled Git state.
</details>

### Q8: What is the difference between Maven Surefire and Maven Failsafe plugins?
<details>
<summary>Click to view answer</summary>

- **Maven Surefire**: Executes unit tests (`*Test.java`) during the `test` phase. If a unit test fails, the build halts immediately.
- **Maven Failsafe**: Executes integration tests (`*IT.java`) during the `integration-test` and `verify` phases. If a test fails, Failsafe allows subsequent teardown tasks (e.g. shutting down test servers or cleaning up resources) to finish before failing the build during the `verify` phase.
</details>

---

## 11. Practice Exercises

### Exercise 1: Configure Maven Failsafe for Integration Tests (Easy)
<details>
<summary>Click to view challenge & solution</summary>

**Challenge**: Configure `pom.xml` with the Maven Failsafe plugin to execute all `*IT.java` integration tests during the `verify` lifecycle phase.

**Solution**:
```xml
<plugin>
    <groupId>org.apache.maven.plugins</groupId>
    <artifactId>maven-failsafe-plugin</artifactId>
    <version>3.2.5</version>
    <executions>
        <execution>
            <goals>
                <goal>integration-test</goal>
                <goal>verify</goal>
            </goals>
        </execution>
    </executions>
</plugin>
```
</details>

### Exercise 2: Add Trivy Security Scan Step to GitHub Actions (Medium)
<details>
<summary>Click to view challenge & solution</summary>

**Challenge**: Write a GitHub Actions step that scans image `my-app:${{ github.sha }}` with Trivy and fails the pipeline if any CRITICAL vulnerability is found.

**Solution**:
```yaml
- name: Run Trivy Vulnerability Scanner
  uses: aquasecurity/trivy-action@master
  with:
    image-ref: 'my-app:${{ github.sha }}'
    format: 'table'
    exit-code: '1'
    ignore-unfixed: true
    severity: 'CRITICAL'
```
</details>

### Exercise 3: Complete ArgoCD GitOps Repository Synchronization Script (Hard)
<details>
<summary>Click to view challenge & solution</summary>

**Challenge**: Write a shell script for a CI runner that clones a GitOps repository, uses `yq` to update the Helm `image.tag` property to the current Git SHA, commits the change, and pushes to Git.

**Solution**:
```bash
#!/bin/bash
set -e

GIT_SHA=$1
GITOPS_REPO="https://x-access-token:${GITHUB_TOKEN}@github.com/myorg/gitops-infra.git"

echo "Cloning GitOps infrastructure repository..."
git clone "$GITOPS_REPO" /tmp/gitops
cd /tmp/gitops/charts/billing-service

echo "Updating image tag to $GIT_SHA using yq..."
yq -i ".image.tag = \"$GIT_SHA\"" values.yaml

git config user.name "ci-deploy-bot"
git config user.email "bot@company.com"

if git diff --quiet values.yaml; then
    echo "No change in image tag. Skipping commit."
else
    git add values.yaml
    git commit -m "chore(billing-service): promote image tag $GIT_SHA to production"
    echo "Pushing updated values to GitOps repository..."
    git push origin main
    echo "ArgoCD will synchronize the release automatically."
fi
```
</details>

---

## 12. Summary / Cheat Sheet

```
+---------------------------------------------+-------------------------------------------------------------+
| CI/CD Principle                             | Production Implementation Rule                              |
+---------------------------------------------+-------------------------------------------------------------+
| OIDC Authentication                         | Zero static AWS/GCP keys; exchange short-lived tokens       |
| Immutable Image Tagging                     | Tag strictly with Git SHA (:${{ github.sha }}); never :latest|
| Testcontainers                              | Run real PostgreSQL/Kafka integration tests via Failsafe    |
| JaCoCo Coverage Gate                        | Fail builds if branch/instruction coverage drops below 80%  |
| Trivy Vulnerability Scan                    | Block deployments containing unpatched CRITICAL CVEs        |
| GitOps Delivery                             | Cluster pulls from Git via ArgoCD; zero direct kubectl in CI|
| Maven Dependency Caching                    | Cache ~/.m2 repository across runs to slash build times     |
+---------------------------------------------+-------------------------------------------------------------+
```

---

## 13. Further Reading
- GitHub Actions Official Documentation: Building and Testing Java
- Testcontainers Official Documentation: Continuous Integration Setup
- ArgoCD Official Documentation: Declarative GitOps Workflows
- CNCF: Automated Software Supply Chain Security Best Practices