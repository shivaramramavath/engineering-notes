<!--
================================================================================
Metadata:
  Title: Technical Interview Preparation - GitHub Actions CI/CD Architecture Q&A
  Track: Git & GitHub / DevOps
  Difficulty: Senior to Staff
  Estimated Time: 55 minutes
  Key Terminology: Workflow Syntax, Triggers, Matrix, Caching, Reusable Workflows, Composite Actions, Self-Hosted Runners, ARC, Concurrency, Secrets Masking
================================================================================
-->

# GitHub Actions CI/CD Architecture: 20 In-Depth Interview Questions & Answers

[<- Back to Interview Overview](./README.md) | [Next Note: 07-security-qa.md ->](./07-security-qa.md)

---

## Overview

This guide provides 20 senior- and staff-level interview questions analyzing GitHub Actions architecture, workflow execution lifecycles, runner orchestration, caching mechanics, composite action engineering, enterprise self-hosted scaling, and pipeline security hardening.

---

## Questions Index

1. [Q01: Explain the architectural hierarchy of GitHub Actions (Workflow, Job, Step, Action, Runner).](#q01-explain-the-architectural-hierarchy-of-github-actions-workflow-job-step-action-runner)
2. [Q02: What is the difference between `pull_request` and `pull_request_target` triggers, and what is the security implication?](#q02-what-is-the-difference-between-pull_request-and-pull_request_target-triggers-and-what-is-the-security-implication)
3. [Q03: How do Reusable Workflows (`workflow_call`) differ from Custom Composite Actions (`action.yml`)?](#q03-how-do-reusable-workflows-workflow_call-differ-from-custom-composite-actions-actionyml)
4. [Q04: How does GitHub Actions dependency caching (`actions/cache`) work internally, and what is the cache eviction policy?](#q04-how-does-github-actions-dependency-caching-actionscache-work-internally-and-what-is-the-cache-eviction-policy)
5. [Q05: Explain how a Matrix Build strategy operates, and how `fail-fast: false` impacts execution.](#q05-explain-how-a-matrix-build-strategy-operates-and-how-fail-fast-false-impacts-execution)
6. [Q06: What are Concurrency Groups, and how does `cancel-in-progress: true` save enterprise CI compute costs?](#q06-what-are-concurrency-groups-and-how-does-cancel-in-progress-true-save-enterprise-ci-compute-costs)
7. [Q07: How does GitHub Actions handle secret masking, and how can a developer inadvertently leak a secret in logs?](#q07-how-does-github-actions-handle-secret-masking-and-how-can-a-developer-inadvertently-leak-a-secret-in-logs)
8. [Q08: What are GitHub Environments, and how do they enforce deployment gates and approvals?](#q08-what-are-github-environments-and-how-do-they-enforce-deployment-gates-and-approvals)
9. [Q09: Compare GitHub-Hosted Runners vs Self-Hosted Runners on Kubernetes using Actions Runner Controller (ARC).](#q09-compare-github-hosted-runners-vs-self-hosted-runners-on-kubernetes-using-actions-runner-controller-arc)
10. [Q10: Why should self-hosted runners NEVER be used for public open-source repositories?](#q10-why-should-self-hosted-runners-never-be-used-for-public-open-source-repositories)
11. [Q11: Explain the purpose and syntax of the `permissions:` block in GitHub Actions workflows.](#q11-explain-the-purpose-and-syntax-of-the-permissions-block-in-github-actions-workflows)
12. [Q12: How do you pass data or artifacts between two different jobs in the same workflow?](#q12-how-do-you-pass-data-or-artifacts-between-two-different-jobs-in-the-same-workflow)
13. [Q13: What is the difference between `env` at the workflow level, job level, and step level?](#q13-what-is-the-difference-between-env-at-the-workflow-level-job-level-and-step-level)
14. [Q14: Explain the difference between `${{ env.VAR }}` and `$VAR` inside a `run:` script.](#q14-explain-the-difference-between-envvar-and-var-inside-a-run-script)
15. [Q15: How does the `repository_dispatch` event work, and how do external systems trigger workflows?](#q15-how-does-the-repository_dispatch-event-work-and-how-do-external-systems-trigger-workflows)
16. [Q16: How do you implement a step that runs ONLY when a previous step fails?](#q16-how-do-you-implement-a-step-that-runs-only-when-a-previous-step-fails)
17. [Q17: What are Workflow Commands in GitHub Actions (e.g., `::set-output`, `::notice`, `$GITHUB_ENV`)?](#q17-what-are-workflow-commands-in-github-actions-eg-set-output-notice-github_env)
18. [Q18: How does GitHub Actions prevent runaway workflows from consuming infinite billing minutes?](#q18-how-does-github-actions-prevent-runaway-workflows-from-consuming-infinite-billing-minutes)
19. [Q19: What is OpenID Connect (OIDC) in Actions, and how does it eliminate long-lived cloud credentials?](#q19-what-is-openid-connect-oidc-in-actions-and-how-does-it-eliminate-long-lived-cloud-credentials)
20. [Q20: How do you test and debug GitHub Actions workflows locally without pushing to remote?](#q20-how-do-you-test-and-debug-github-actions-workflows-locally-without-pushing-to-remote)

---

## Questions & Answers

### Q01: Explain the architectural hierarchy of GitHub Actions (Workflow, Job, Step, Action, Runner).
**Answer:**  
- **Workflow**: The top-level automated process defined in a `.github/workflows/*.yml` file, triggered by events (push, PR, cron, dispatch).
- **Job**: A set of steps executed sequentially on the **same runner instance**. By default, multiple jobs run concurrently unless coordinated via `needs: [jobA]`.
- **Step**: An individual task inside a job. Runs either a shell command (`run:`) or an action (`uses:`). Steps share the local runner filesystem.
- **Action**: The smallest reusable execution unit (written as a Composite Action, JavaScript Action, or Docker Container Action).
- **Runner**: The physical virtual machine or container running the GitHub Actions runner agent operating system.

---

### Q02: What is the difference between `pull_request` and `pull_request_target` triggers, and what is the security implication?
**Answer:**  
- **`pull_request`**: Runs in the context of the **PR merge ref** (`refs/pull/N/merge`) using code from the pull request. It runs with **read-only** `GITHUB_TOKEN` permissions and has **no access to repository secrets**.
- **`pull_request_target`**: Runs in the context of the **base repository's default branch** (`main`). It has **write permissions** and **full access to repository secrets**.
**Severe Security Hazard**: If a workflow triggered on `pull_request_target` checks out the PR branch code (`actions/checkout` with `ref: ${{ github.event.pull_request.head.sha }}`) and runs build scripts (e.g., `npm run build`), an external attacker can submit a PR containing malicious code that executes with full access to production repository secrets.

---

### Q03: How do Reusable Workflows (`workflow_call`) differ from Custom Composite Actions (`action.yml`)?
**Answer:**  
- **Reusable Workflows**: Operates at the **job level**. Encapsulates complete jobs, multiple runners, environment approvals, permissions, and matrix strategies. Invoked with `uses: org/repo/.github/workflows/file.yml@v1`.
- **Composite Actions**: Operates at the **step level**. Combines multiple shell commands and nested actions inside a single job on the existing runner. Cannot define independent jobs, matrixes, or environments.

---

### Q04: How does GitHub Actions dependency caching (`actions/cache`) work internally, and what is the cache eviction policy?
**Answer:**  
`actions/cache` computes a cryptographic hash of your lockfile (e.g., `hashFiles('**/package-lock.json')`).
- If an exact match exists, it downloads and uncompresses the tar archive to the specified path during job setup.
- If no match occurs, the job runs `npm install`, and a post-job action compresses and uploads the cache archive to GitHub's cache storage.
**Eviction Policy**: Repositories receive a maximum of **10GB** total cache storage. GitHub automatically purges the least-recently-accessed caches when storage exceeds 10GB or when cache entries are older than **7 days**.

---

### Q05: Explain how a Matrix Build strategy operates, and how `fail-fast: false` impacts execution.
**Answer:**  
A matrix build generates multiple job variations from variable arrays:
```yaml
strategy:
  fail-fast: false
  matrix:
    os: [ubuntu-latest, macos-latest]
    node: [18, 20, 22]
```
This spawns $2 \times 3 = 6$ concurrent jobs.
**`fail-fast: true` (default)**: If any single job in the matrix fails, GitHub cancels all remaining active jobs immediately to save compute time.
**`fail-fast: false`**: Allows all matrix jobs to run to completion regardless of individual failures, ensuring engineers can see test results across all platforms and versions simultaneously.

---

### Q06: What are Concurrency Groups, and how does `cancel-in-progress: true` save enterprise CI compute costs?
**Answer:**  
When a developer pushes 5 commits in rapid succession to a feature branch PR, standard GitHub Actions spawns 5 separate workflow runs, queuing jobs and consuming hundreds of runner minutes for obsolete intermediate commits.
A **Concurrency Group** locks workflow execution to a specific key (e.g., `${{ github.workflow }}-${{ github.ref }}`). With `cancel-in-progress: true`, GitHub automatically terminates any active or queued workflow runs for that branch the instant a newer commit is pushed, ensuring compute resources are dedicated only to latest commits.

---

### Q07: How does GitHub Actions handle secret masking, and how can a developer inadvertently leak a secret in logs?
**Answer:**  
When a secret is referenced in a workflow, GitHub's runner logs daemon adds the literal secret value to an in-memory redact filter. Any output stream matching the exact secret string is replaced with `***`.
**Inadvertent Leak Vectors**:
1. Base64 or URL-encoding a secret: The encoded string differs from the raw registered secret, bypassing the redact filter (`echo "$SECRET" | base64` prints in plaintext).
2. Printing single-character or short secrets (secrets shorter than 3 characters are not masked by default).
3. Writing secrets into exported build artifacts or downloadable test reports.

---

### Q08: What are GitHub Environments, and how do they enforce deployment gates and approvals?
**Answer:**  
GitHub Environments model deployment targets (`development`, `staging`, `production`).
They provide:
- **Environment Secrets**: Scoped exclusively to that environment (e.g., production DB password).
- **Deployment Protection Rules**: Mandate required reviewers (human approval button in UI) or timer delays (wait 30 mins) before jobs can execute.
- **Deployment Branch Restrictions**: Restrict environments so only designated protected branches (e.g., `main` or `release/*`) can deploy.

---

### Q09: Compare GitHub-Hosted Runners vs Self-Hosted Runners on Kubernetes using Actions Runner Controller (ARC).
**Answer:**  
- **GitHub-Hosted**: Managed, fully isolated ephemeral VMs (Ubuntu, Windows, macOS). Zero maintenance, pre-installed software, auto-patched. Billed per minute.
- **Actions Runner Controller (ARC)**: Kubernetes operator running in your private VPC. Dynamically scales ephemeral container pods or VMs on demand based on GitHub webhook workflow queues.
  - *Benefits*: Access to private VPC resources, customized container images, unlimited compute capacity, substantial cost savings for high-volume pipelines.

---

### Q10: Why should self-hosted runners NEVER be used for public open-source repositories?
**Answer:**  
Anyone can fork a public repository and open a pull request. If the workflow executes on a self-hosted runner, untrusted code authored by external threat actors executes directly inside your private infrastructure network. Malicious actors can compromise the host, extract AWS metadata tokens, pivot to internal VPC databases, or install cryptominers. Public repositories should exclusively use GitHub-hosted runners or ephemeral sandboxes.

---

### Q11: Explain the purpose and syntax of the `permissions:` block in GitHub Actions workflows.
**Answer:**  
The `permissions:` block enforces the Principle of Least Privilege on the automatic `GITHUB_TOKEN`.
By default, older repos granted permissive read/write access. Declaring explicit permissions prevents compromised dependencies from pushing malicious commits or tampering with releases:
```yaml
permissions:
  contents: read
  pull-requests: write
  id-token: write
```
Any undeclared permission evaluates strictly to `none`.

---

### Q12: How do you pass data or artifacts between two different jobs in the same workflow?
**Answer:**  
Because separate jobs execute on completely different virtual machines, they cannot share filesystem state.
Two methods:
1. **Small scalar values (strings, SHAs)**: Use **Job Outputs** (`outputs:`) passed via `needs.<job_id>.outputs.<key>`.
2. **Files, binaries, reports**: Use **Artifacts** via `actions/upload-artifact@v4` in Job A, and `actions/download-artifact@v4` in Job B.

---

### Q13: What is the difference between `env` at the workflow level, job level, and step level?
**Answer:**  
- **Workflow-level `env:`**: Available to every step across all jobs in the workflow.
- **Job-level `env:`**: Available to all steps within that specific job.
- **Step-level `env:`**: Strictly scoped to that single step.
Variables declared at deeper levels override variables with identical names from outer scopes.

---

### Q14: Explain the difference between `${{ env.VAR }}` and `$VAR` inside a `run:` script.
**Answer:**  
- `${{ env.VAR }}`: Evaluated by the **GitHub Actions expression engine** *before* the shell process starts. GitHub textually interpolates the value directly into the shell script string. (Can lead to shell injection vulnerabilities if unquoted).
- `$VAR`: Evaluated by the **OS shell process (Bash/PowerShell)** at runtime via standard environment variable resolution. This is safer for dynamic values.

---

### Q15: How does the `repository_dispatch` event work, and how do external systems trigger workflows?
**Answer:**  
`repository_dispatch` enables external webhooks (e.g., from Jira, Shopify, or an internal microservice) to trigger workflows on GitHub.
External systems make an HTTP POST to:
`https://api.github.com/repos/{owner}/{repo}/dispatches`
Passing a custom `event_type` and optional `client_payload` JSON. The workflow triggers via:
```yaml
on:
  repository_dispatch:
    types: [deploy_ping, order_created]
```

---

### Q16: How do you implement a step that runs ONLY when a previous step fails?
**Answer:**  
Use the `if: failure()` job status check function:
```yaml
- name: Run Test Suite
  run: npm test

- name: Post Failure Alert to Slack
  if: failure()
  run: ./scripts/notify-slack-failure.sh
```
Other status functions include `always()` (runs regardless of failure) and `cancelled()` (runs if the workflow was cancelled).

---

### Q17: What are Workflow Commands in GitHub Actions (e.g., `::set-output`, `::notice`, `$GITHUB_ENV`)?
**Answer:**  
Workflow commands allow shell scripts in a `run:` block to communicate with the GitHub runner daemon:
- **Set Environment Variable**: `echo "NODE_ENV=production" >> $GITHUB_ENV`
- **Add to System PATH**: `echo "/custom/bin" >> $GITHUB_PATH`
- **Set Job Output**: `echo "version=1.2.0" >> $GITHUB_OUTPUT`
- **Emit PR Annotation**: `echo "::error file=app.js,line=10::Syntax error"`
- **Create Job Summary**: `echo "### Test Results" >> $GITHUB_STEP_SUMMARY`

---

### Q18: How does GitHub Actions prevent runaway workflows from consuming infinite billing minutes?
**Answer:**  
- **Job Timeout**: Every job has a default timeout of **360 minutes (6 hours)**, after which GitHub kills the runner.
- **Configurable `timeout-minutes`**: Engineers should explicitly set tight job timeouts:
  ```yaml
  jobs:
    test:
      runs-on: ubuntu-latest
      timeout-minutes: 15
  ```
- **Execution Limits**: Workflows cannot exceed 72 hours of queued and active time.

---

### Q19: What is OpenID Connect (OIDC) in Actions, and how does it eliminate long-lived cloud credentials?
**Answer:**  
OIDC allows GitHub Actions runners to obtain short-lived cloud credentials (AWS, GCP, Azure) without storing static API keys in repository secrets.
The runner requests a cryptographically signed JWT from GitHub containing repository claims. The cloud provider's IAM STS validates GitHub's public certificate and issues temporary credentials valid for 15 to 60 minutes, eliminating credential rotation and theft risks.

---

### Q20: How do you test and debug GitHub Actions workflows locally without pushing to remote?
**Answer:**  
Use **`act`** (an open-source CLI tool by nektos). `act` parses `.github/workflows/*.yml`, pulls local Docker containers matching GitHub-hosted runner environments, and executes workflow jobs locally on your workstation, allowing rapid iteration of shell commands and environment variables without spamming Git commit history.

---

[<- Back to Interview Overview](./README.md) | [Next Note: 07-security-qa.md ->](./07-security-qa.md)
