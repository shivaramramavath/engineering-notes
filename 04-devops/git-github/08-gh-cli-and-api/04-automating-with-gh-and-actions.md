---
title: "Automating Workflows with GitHub CLI (`gh`) in GitHub Actions"
category: "04-devops"
subcategory: "git-github / 08-gh-cli-and-api"
tags: ["git", "github", "gh-cli", "actions", "automation", "ci-cd", "devops", "scripting"]
---

# Automating Workflows with GitHub CLI (`gh`) in GitHub Actions

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

GitHub Actions provides an extensive ecosystem of community-authored actions in the GitHub Marketplace. However, relying on dozens of third-party actions introduces severe supply chain risks, maintenance overhead, and version pinning churn. Simple operations—such as commenting on a pull request, applying a label, closing a stale issue, triggering a downstream workflow, or creating a release—frequently do not require pulling in external JavaScript or Docker containers.

The **GitHub CLI (`gh`)** comes pre-installed on all GitHub-hosted virtual runners (Ubuntu, macOS, Windows). By pairing `gh` with the native `${{ secrets.GITHUB_TOKEN }}`, developers can write lightweight, auditable, high-performance automation scripts directly inside standard `run:` blocks.

This approach delivers:
- **Zero Third-Party Dependencies**: Reduces supply chain attack surfaces.
- **Deterministic Scripting**: Uses standard shell syntax (`gh pr review`, `gh issue comment`, `gh release create`).
- **Seamless Local Parity**: Developers can run and debug the exact same automation commands on their local workstation terminal before embedding them in CI workflows.

---

## 2. Core Concepts

### Pre-Installed Runner Binary
GitHub-hosted runners (`ubuntu-latest`, `windows-latest`, `macos-latest`) maintain the latest stable version of `gh` in the default system `PATH`. There is zero installation or caching overhead required.

### Authentication via `GITHUB_TOKEN`
When `gh` executes inside GitHub Actions, it automatically detects and authenticates against the environment variable `GITHUB_TOKEN` or `GH_TOKEN`. All commands execute under the permissions granted to the running workflow.

### Cross-Repository Automation
By default, `GITHUB_TOKEN` only has access to the repository executing the workflow. To interact with another repository (e.g., dispatching a workflow in `my-org/shared-infra` or creating an issue in `my-org/central-tracker`), pass a **GitHub App token** or fine-grained personal access token into the `GH_TOKEN` environment variable.

### Workflow Dispatches (`gh workflow run`)
Workflows can trigger parameterized jobs across repos using `gh workflow run`, passing inputs, selecting branches, and polling run completion.

---

## 3. How It Works Internally

```
GitHub Actions Workflow Step
+-------------------------------------------------------------------------+
| - name: Auto-Approve Dependabot PR                                      |
|   run: |                                                                |
|     gh pr review --approve "$PR_URL"                                    |
|   env:                                                                  |
|     GITHUB_TOKEN: ${{ secrets.GITHUB_TOKEN }}                           |
|     PR_URL: ${{ github.event.pull_request.html_url }}                   |
+-------------------------------------------------------------------------+
                                    |
                                    v
+-------------------------------------------------------------------------+
| Runner OS Process Execution                                             |
| 1. Evaluates environment: GITHUB_TOKEN is detected                      |
| 2. Evaluates git context: GitHub repository inferred from workspace     |
| 3. Resolves PR_URL argument                                             |
+-------------------------------------------------------------------------+
                                    |
                                    v
+-------------------------------------------------------------------------+
| GitHub GraphQL / REST API Request                                       |
| - Header: Authorization: Bearer <GITHUB_TOKEN>                          |
| - Action: Submits approving review to PR                                |
+-------------------------------------------------------------------------+
                                    |
                                    v
+-------------------------------------------------------------------------+
| GitHub Platform State Updated                                           |
| - PR review status updated to APPROVED                                  |
| - Evaluates branch protection rules and unblocks merge                  |
+-------------------------------------------------------------------------+
```

---

## 4. Syntax & Usage

### Basic Workflow Pattern
```yaml
jobs:
  automate:
    runs-on: ubuntu-latest
    permissions:
      pull-requests: write
      issues: write
      contents: write
    steps:
      - uses: actions/checkout@v4

      - name: Run GitHub CLI Automation
        run: |
          gh pr comment ${{ github.event.pull_request.number }} \
            --body "Automated test suite completed successfully."
        env:
          GITHUB_TOKEN: ${{ secrets.GITHUB_TOKEN }}
```

---

## 5. Practical Examples

### Example 1: Automated PR Size Labeling & Guardrails
Automatically label pull requests based on diff line additions/deletions and warn developers if the PR is excessively large:
```yaml
name: "PR Size Labeler"
on:
  pull_request:
    types: [opened, synchronize]

permissions:
  pull-requests: write

jobs:
  size-label:
    runs-on: ubuntu-latest
    steps:
      - name: Check PR Size
        run: |
          # Fetch PR additions and deletions using gh CLI
          PR_DATA=$(gh pr view ${{ github.event.pull_request.number }} --json additions,deletions)
          ADDITIONS=$(echo "$PR_DATA" | jq '.additions')
          DELETIONS=$(echo "$PR_DATA" | jq '.deletions')
          TOTAL=$((ADDITIONS + DELETIONS))

          echo "Total lines changed: $TOTAL"

          if [ "$TOTAL" -gt 600 ]; then
            gh pr edit ${{ github.event.pull_request.number }} --add-label "size/XL"
            gh pr comment ${{ github.event.pull_request.number }} \
              --body "⚠️ **Warning**: This PR modifies $TOTAL lines. Consider splitting into smaller PRs to streamline code review."
          elif [ "$TOTAL" -gt 300 ]; then
            gh pr edit ${{ github.event.pull_request.number }} --add-label "size/L"
          else
            gh pr edit ${{ github.event.pull_request.number }} --add-label "size/S"
          fi
        env:
          GITHUB_TOKEN: ${{ secrets.GITHUB_TOKEN }}
```

### Example 2: Triggering Downstream Workflow in Another Repository
Dispatch a deployment workflow in a centralized deployment repository and wait for results:
```yaml
name: "Dispatch Deployment"
on:
  push:
    branches: ["main"]

jobs:
  trigger-deploy:
    runs-on: ubuntu-latest
    steps:
      - name: Trigger Downstream Pipeline
        run: |
          # Trigger workflow in centralized infra repository
          gh workflow run deploy-service.yml \
            --repo my-org/gitops-deployments \
            --ref main \
            -f service_name="payment-api" \
            -f image_tag="${{ github.sha }}"

          echo "Deployment triggered successfully."
        env:
          GH_TOKEN: ${{ secrets.CENTRAL_DEPLOY_APP_TOKEN }}
```

### Example 3: Automatic Release Asset Publishing
Create an official GitHub Release and upload built binaries without third-party actions:
```yaml
name: "Publish Release"
on:
  push:
    tags: ["v*"]

permissions:
  contents: write

jobs:
  release:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v4

      - name: Build Artifacts
        run: |
          mkdir dist
          tar -czf dist/app-${{ github.ref_name }}-linux.tar.gz ./bin

      - name: Create GitHub Release with Assets
        run: |
          gh release create ${{ github.ref_name }} \
            dist/* \
            --title "Release ${{ github.ref_name }}" \
            --generate-notes \
            --verify-tag
        env:
          GITHUB_TOKEN: ${{ secrets.GITHUB_TOKEN }}
```

### Example 4: Auto-Closing Stale Issues with Comments
```yaml
name: "Stale Issue Closer"
on:
  schedule:
    - cron: "0 0 * * *" # Daily

permissions:
  issues: write

jobs:
  close-stale:
    runs-on: ubuntu-latest
    steps:
      - name: Close issues marked as inactive
        run: |
          # Find issues labeled 'needs-info' inactive for > 14 days
          STALE_ISSUES=$(gh issue list \
            --label "needs-info" \
            --search "updated:<@now-14d" \
            --json number \
            --jq '.[].number')

          for ISSUE in $STALE_ISSUES; do
            echo "Closing stale issue #$ISSUE"
            gh issue comment "$ISSUE" --body "Closing due to 14 days of inactivity."
            gh issue close "$ISSUE" --reason "not planned"
          done
        env:
          GITHUB_TOKEN: ${{ secrets.GITHUB_TOKEN }}
```

### Example 5: Synchronizing Labels Across Repositories
Synchronize standardized enterprise labels across a fleet of repositories:
```yaml
name: "Sync Labels"
on: [workflow_dispatch]

jobs:
  sync:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v4

      - name: Create or update standard labels
        run: |
          gh label create "priority:high" --color "B60205" --description "High priority bug" --force
          gh label create "type:security" --color "D93F0B" --description "Security vulnerability" --force
        env:
          GITHUB_TOKEN: ${{ secrets.GITHUB_TOKEN }}
```

---

## 6. Best Practices

- **Pass Tokens Explicitly via `env:`**: Always pass `GITHUB_TOKEN: ${{ secrets.GITHUB_TOKEN }}` or `GH_TOKEN: ${{ secrets.MY_TOKEN }}` in the step's environment block.
- **Declare Granular Permissions**: If a step only comments on PRs, declare `permissions: pull-requests: write`. Do not grant broad `write-all` permissions.
- **Use `--force` for Idempotent Labeling**: When creating labels or tags in scripts, use `gh label create ... --force` to prevent the command from failing if the resource already exists.
- **Prefer `gh release create --generate-notes`**: Leverage GitHub's native changelog generator to automatically extract merged PRs and author contributors into the release body.
- **Quote Shell Variables**: Always quote bash variables (`"$ISSUE"`, `"$PR_URL"`) to prevent word splitting and shell injection vulnerabilities.

---

## 7. Common Mistakes

| Mistake | Impact | Correct Approach |
|:---|:---|:---|
| Omitting `GITHUB_TOKEN` in `env:` block | `gh` fails with "gh: To use GitHub CLI in a GitHub Actions workflow, set the GH_TOKEN environment variable" | Add `env: GITHUB_TOKEN: ${{ secrets.GITHUB_TOKEN }}` |
| Installing `gh` via `apt-get` in workflow | Wastes 30–60 seconds per build | `gh` is already pre-installed; invoke directly |
| Forgetting `permissions:` declaration | Operations fail with HTTP 403 Forbidden | Grant specific write permissions in workflow YAML |
| Using `GITHUB_TOKEN` for cross-repo workflows | Fails with "resource not accessible" | Pass a GitHub App installation token or personal token via `GH_TOKEN` |
| Parsing CLI table output instead of `--json` | Scripts break when column padding or UI elements update | Always extract data using `--json` and `--jq` |
| Calling `gh` in un-throttled bash loops | Triggers GitHub secondary rate limits | Add `sleep 1` between iterations in batch loops |

---

## 8. Performance, Security & Cost Considerations

- **Performance**: Executing `gh` directly avoids the overhead of downloading and setting up third-party Node.js/Docker actions, shaving 10–30 seconds off job startup times.
- **Security**: Reduces supply chain vulnerabilities by eliminating untrusted third-party actions from the workflow dependency tree.
- **Cost**: Saves runner minutes through faster job execution. `gh` binary execution consumes zero billable action storage.

---

## 9. Comparison Table

| Attribute | `gh` CLI in `run:` Block | Marketplace Action (`actions/*`) | Custom Composite Action |
|:---|:---|:---|:---|
| **Setup Time** | 0s (Pre-installed) | 5–20s (Download & init) | 5–10s (Repo checkout) |
| **Supply Chain Risk**| Minimal (Official binary) | High (Third-party supply chain) | Low (Internally maintained) |
| **Local Debugging** | Exact 1:1 local reproduction | Requires tools like `act` | Requires local runner testing |
| **Flexibility** | Full Bash/CLI capabilities | Rigid input parameters | Custom shell / composite logic |
| **Maintenance** | Auto-updated by GitHub runners | Requires version tag bumps | Requires internal maintenance |

---

## 10. Real-World Interview Questions

<details>
<summary>1. Why is using native `gh` CLI in Actions preferred over third-party Marketplace actions for simple tasks?</summary>
Third-party actions introduce software supply chain risks (typosquatting, compromised maintainer accounts, malicious updates). They also require network overhead to download and initialize on the runner. Native `gh` is pre-installed on every GitHub runner, officially maintained by GitHub, zero-overhead, and allows engineers to test the exact same command locally.
</details>

<details>
<summary>2. Why does `gh` fail inside a GitHub Actions workflow if you do not export `GITHUB_TOKEN`?</summary>
In a local terminal, `gh` reads tokens saved in your operating system credential store. Inside a GitHub Actions ephemeral container, no user session exists. `gh` explicitly checks for the presence of the `GH_TOKEN` or `GITHUB_TOKEN` environment variables to authenticate. If neither is exported in the step, `gh` aborts with an authentication error.
</details>

<details>
<summary>3. Can a workflow in Repo A use `gh workflow run` to trigger a workflow in Repo B using the standard `GITHUB_TOKEN`?</summary>
No. For security isolation, the default `GITHUB_TOKEN` is strictly scoped to the repository executing the workflow. To interact with Repo B, the step must export a token possessing cross-repository permissions (such as a GitHub App installation token or an organization-scoped Fine-Grained PAT) stored in an Action secret.
</details>

<details>
<summary>4. What permissions are required to create a release with `gh release create`?</summary>
The workflow must declare `permissions: contents: write` because creating a release writes git tags and release metadata to the repository.
</details>

<details>
<summary>5. How can you automate pull request auto-merging using `gh` CLI once CI passes?</summary>
Execute `gh pr merge <number> --auto --squash` inside a workflow step. The `--auto` flag instructs GitHub to automatically merge the pull request the moment all required branch protection checks and approvals are satisfied.
</details>

<details>
<summary>6. How do you pass complex multiline strings or JSON payloads into `gh` commands safely in Bash?</summary>
Use heredocs or environment variables rather than direct string interpolation:
```bash
gh issue comment "$ISSUE_NUM" --body "$COMMENT_BODY"
```
Direct string interpolation (e.g., `--body "${{ github.event.comment.body }}"`) is vulnerable to shell injection attacks if the input contains quotes or shell metacharacters.
</details>

<details>
<summary>7. What flag enables `gh release create` to automatically draft changelogs from merged PRs?</summary>
The `--generate-notes` flag automatically parses merged pull requests and contributors since the previous tag and populates the release description.
</details>

<details>
<summary>8. How do you prevent a batch loop using `gh` from hitting secondary rate limits?</summary>
Introduce small pacing delays (`sleep 1`) between API mutations, limit batch query sizes, and paginate read requests with `--paginate`.
</details>

---

## 11. Hands-On Exercises

<details>
<summary>Exercise 1: Create a PR comment step in Actions</summary>

**Goal:** Author a step commenting on a PR.

```yaml
- name: Add PR Comment
  run: gh pr comment ${{ github.event.pull_request.number }} --body "Test run complete."
  env:
    GITHUB_TOKEN: ${{ secrets.GITHUB_TOKEN }}
```
</details>

<details>
<summary>Exercise 2: Draft a release using `gh` CLI in a workflow</summary>

**Goal:** Create a release draft with auto-generated notes.

```yaml
- name: Draft Release
  run: gh release create ${{ github.ref_name }} --draft --generate-notes
  env:
    GITHUB_TOKEN: ${{ secrets.GITHUB_TOKEN }}
```
</details>

<details>
<summary>Exercise 3: Extract open PR numbers using `--json`</summary>

**Goal:** Script a bash loop processing open PRs.

```bash
PR_LIST=$(gh pr list --json number --jq '.[].number')
for pr in $PR_LIST; do
  echo "Processing PR #$pr"
done
```
</details>

<details>
<summary>Exercise 4: Enable auto-merge on an existing PR</summary>

**Goal:** Configure a PR to auto-merge upon CI completion.

```bash
gh pr merge 42 --auto --squash --delete-branch
```
</details>

<details>
<summary>Exercise 5: Set up an idempotent label creation script</summary>

**Goal:** Create standard labels without failing on existing ones.

```bash
gh label create "env:production" --color "0052CC" --force
```
</details>

<details>
<summary>Exercise 6: Trigger a parameterized workflow dispatch</summary>

**Goal:** Dispatch a workflow passing custom inputs.

```bash
gh workflow run build.yml -f environment=staging -f debug=true
```
</details>

---

## 12. Summary / Cheat Sheet

```text
================================================================================
                    AUTOMATING WITH GH IN GITHUB ACTIONS
================================================================================
Pre-requisite      : None (gh pre-installed on all GitHub-hosted runners)
Authentication     : env: GITHUB_TOKEN: ${{ secrets.GITHUB_TOKEN }}
Cross-Repo Auth    : env: GH_TOKEN: ${{ secrets.CROSS_REPO_APP_TOKEN }}
PR Operations      : gh pr comment <num> --body "..." | gh pr merge --auto --squash
Release Creation   : gh release create <tag> <files> --generate-notes
Workflow Dispatch  : gh workflow run <name.yml> -f key=val
Idempotent Labels  : gh label create <name> --color <hex> --force
Security Rule      : Always declare minimal job-level permissions: block
================================================================================
```

---

## 13. Further Reading
- [GitHub CLI Manual: Actions Integration](https://cli.github.com/manual/gh_help)
- [GitHub Docs: Pre-installed software on GitHub-hosted runners](https://docs.github.com/en/actions/using-github-hosted-runners/about-github-hosted-runners#preinstalled-software)
- [GitHub Docs: Security hardening for GitHub Actions](https://docs.github.com/en/actions/security-guides/security-hardening-for-github-actions)
- [OpenSSF Scorecard: Automated Supply Chain Checks](https://scorecard.dev/)

---

[<- Previous: Webhooks & GitHub Apps](./03-webhooks-and-apps.md) | [Module 08 Overview](./README.md) | [Next: Module 09 Projects Overview ->](../09-projects/README.md)
