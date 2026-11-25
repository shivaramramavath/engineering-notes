---
title: "GitHub CLI (`gh`) Essentials & Daily Productivity"
category: "04-devops"
subcategory: "git-github / 08-gh-cli-and-api"
tags: ["git", "github", "gh-cli", "cli", "terminal", "automation", "productivity", "devops"]
---

# GitHub CLI (`gh`) Essentials & Daily Productivity

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

Context switching between terminal command lines and web browser tabs is one of the greatest drains on developer velocity. An engineer authoring code in a terminal frequently has to open a browser window to create a pull request, check CI workflow status, assign reviewers, inspect issue descriptions, or download release assets.

**GitHub CLI (`gh`)** is GitHub's official command-line interface, bringing the entire GitHub web platform directly into the developer's terminal. With `gh`, engineers can:
- Clone, fork, and manage repositories.
- Create, review, checkout, and merge Pull Requests.
- Triage, label, and close Issues.
- Watch, rerun, and debug GitHub Actions workflows in real time.
- Manage repository secrets, environment variables, and releases.
- Query any REST or GraphQL endpoint with built-in JSON parsing and Go templating.

By operating directly against Git's local remotes, `gh` automatically infers context (the target repository, branch, and author), reducing dozens of browser clicks to single concise terminal commands.

---

## 2. Core Concepts

### Context Auto-Detection
When executed inside any local Git repository, `gh` inspects the `.git/config` file to extract the `origin` remote URL. It automatically targets that specific repository without requiring manual `--repo owner/name` flags. When executed outside a Git repository, it gracefully accepts explicit target arguments.

### Authentication & Token Storage
`gh auth login` provides multiple authentication workflows:
- **Interactive Web Browser**: Generates a one-time cryptographic device code.
- **Personal Access Token (PAT)**: Non-interactive pipeline ingestion via standard input (`gh auth login --with-token`).
- **Secure Token Storage**: Persists authentication tokens in system keychains (macOS Keychain, Windows Credential Manager, Linux Secret Service via `libsecret`) or plaintext config files (`~/.config/gh/hosts.yml`).

### Output Formatting Primitives
Every read command in `gh` supports programmatic data extraction:
- `--json <fields>`: Emits raw JSON containing only the specified fields.
- `--jq <filter>`: Pipes JSON directly through an embedded jq interpreter without requiring an external `jq` binary.
- `--template <gotemplate>`: Formats output using Go template strings for human-readable custom CLI tables and terminal colorization.

### Aliases
`gh alias set` allows users to construct custom commands, shorthand shortcuts, and shell pipelines that extend native CLI behavior.

---

## 3. How It Works Internally

```
Developer Terminal Command (e.g., gh pr create --fill)
                        |
                        v
+-------------------------------------------------------------------------+
| Local Repository Context Discovery Engine                               |
| 1. Reads .git/config -> Resolves 'origin' (github.com/my-org/api)       |
| 2. Reads .git/HEAD   -> Identifies current branch (feature-billing)     |
| 3. Analyzes git log  -> Extracts PR title and body from commit history  |
+-------------------------------------------------------------------------+
                        |
                        v
+-------------------------------------------------------------------------+
| GitHub GraphQL & REST API Client Engine                                 |
| - Authenticates with stored token in OS Keychain / hosts.yml            |
| - Issues GraphQL Mutation: CreatePullRequest(input: {...})              |
+-------------------------------------------------------------------------+
                        |
                        v
+-------------------------------------------------------------------------+
| Output Formatting Engine                                                |
| - Checks TTY: If terminal, formats rich ANSI tables & links             |
| - If non-TTY / pipe: formats tab-delimited or processes --json / --jq  |
+-------------------------------------------------------------------------+
                        |
                        v
              Terminal Display / URL
```

---

## 4. Syntax & Usage

### Core CLI Hierarchy
```bash
gh [command] [subcommand] [flags]
```

### Essential Daily Commands
```bash
# Repository Operations
gh repo clone owner/repo
gh repo fork --clone
gh repo view --web

# Pull Request Operations
gh pr create --title "feat: add payment retry" --body "Closes #14"
gh pr checkout 42       # Checks out PR branch locally
gh pr view --web        # Opens PR in browser
gh pr diff              # Diffs PR against base branch
gh pr review --approve  # Approves current PR
gh pr merge --squash    # Merges and deletes local/remote branch

# Issue Operations
gh issue list --assignee "@me"
gh issue create --title "bug: race condition in worker" --body "Repro steps..."
gh issue close 102

# GitHub Actions Monitoring
gh run list --workflow=ci.yml
gh run watch            # Live-streams active workflow execution in terminal
gh run view --log-failed
```

---

## 5. Practical Examples

### Example 1: Non-Interactive Scripted Login for CI/CD
```bash
# Authenticate headless container using token from environment
echo "$GH_AUTOMATION_TOKEN" | gh auth login --with-token
gh auth status
```

### Example 2: Checking Out and Testing a Pull Request Locally
```bash
# Replaces 4 git commands (fetch, checkout, branch creation, upstream tracking)
gh pr checkout 89

# Run tests on the PR code
npm test

# Submit approving review directly from terminal
gh pr review --approve --body "Verified locally with integration test suite."
```

### Example 3: Extracting Machine-Readable Data with `--json` and `--jq`
Extract the URLs of all open critical issues:
```bash
gh issue list --label "critical" --json number,title,url \
  --jq '.[] | "Issue #" + (.number|tostring) + ": " + .title + " -> " + .url'
```

### Example 4: Formatting Custom Terminal Output with Go Templates
Display a formatted table of recent CI runs with colored status:
```bash
gh run list --limit 5 --template \
  '{{range .}}{{tablerow (printf "#%v" .number) .name .headBranch .status .conclusion}}{{end}}'
```

### Example 5: Creating Custom Productivity Aliases
```bash
# Alias to list PRs assigned to you needing your review
gh alias set my-reviews 'pr list --search "review-requested:@me is:open"'

# Alias to clean up local branches whose PRs are already merged
gh alias set pr-clean '!git branch --merged main | grep -v "main" | xargs git branch -d'

# Use the alias
gh my-reviews
```

---

## 6. Best Practices

- **Use `--fill` for Rapid PR Creation**: When authoring a PR for a single commit, use `gh pr create --fill` to automatically populate the PR title and body from the commit message.
- **Prefer SSH Protocol for Git Operations**: Configure `gh config set git_protocol ssh` so cloning and repository initialization default to SSH keys.
- **Use `--web` to Transition Smoothly**: When complex web interactions are necessary (e.g., viewing complex visual image diffs), append `--web` (e.g., `gh pr view --web` or `gh run view --web`) to open the exact browser page immediately.
- **Combine `gh run watch` with Notifications**: Run `gh run watch && notify-send "Build Complete"` to be alerted when a long-running CI build finishes.
- **Store Scripts with `--json`**: Never parse standard human-readable `gh` table output with `awk` or `sed`; terminal table formatting is subject to change. Always use `--json` and `--jq` for scripting stability.

---

## 7. Common Mistakes

| Mistake | Impact | Correct Approach |
|:---|:---|:---|
| Parsing human-readable table output with `cut` or `awk` | Scripts break when column widths or headers change | Always use `--json` and `--jq` for reliable parsing |
| Running `gh pr create` without checking remotes | Targets upstream fork instead of origin repository | Specify base repo explicitly or set default via `gh repo set-default` |
| Forgetting `gh auth refresh -s <scope>` when adding permissions | Operations fail with HTTP 403 insufficient scope | Refresh authentication token with required scope |
| Hardcoding personal PATs into developer bash profiles | Leaked dotfiles expose account credentials | Use system credential manager via `gh auth login` |
| Overwriting remote branches during `gh pr checkout` | Discards local unpushed changes on the same branch name | Stash or commit working tree changes before running checkout |
| Expecting `gh` commands to work without Git remotes | Returns "no git remotes found" error | Run command inside a valid Git repo or pass `--repo owner/name` |

---

## 8. Performance, Security & Cost Considerations

- **Performance**: `gh` utilizes GitHub's GraphQL API for composite queries, fetching issues, PRs, and review states in a single HTTP request rather than sequential REST round-trips.
- **Security**: Tokens stored via `gh auth login` are saved in encrypted OS credential vaults. On headless Linux, ensure `libsecret` is installed or lock down permissions on `~/.config/gh/hosts.yml` (`chmod 600`).
- **Cost**: The GitHub CLI is free and open source. API requests made via `gh` consume standard user API rate limits (5,000 requests/hour for standard personal tokens, 15,000/hour for GitHub Enterprise).

---

## 9. Comparison Table

| Capability | GitHub CLI (`gh`) | Git Native (`git`) | Hub (Legacy CLI) |
|:---|:---|:---|:---|
| **Primary Domain** | GitHub Platform (PRs, Issues, Actions, Secrets)| Distributed VCS (Blobs, Trees, Commits, Branches) | Legacy GitHub wrapper |
| **Actions Monitoring**| Full native streaming (`gh run watch`) | None | None |
| **API Scripting** | Native `--json`, `--jq`, `gh api` | None | Basic API wrapper |
| **Maintenance** | Active official GitHub product | Active Git project | Deprecated in favor of `gh` |
| **Context Awareness** | Inferred from Git remote | Local filesystem | Inferred from Git remote |

---

## 10. Real-World Interview Questions

<details>
<summary>1. How does GitHub CLI differ from native Git, and how do they complement each other?</summary>
Git is a distributed version control engine managing files, commits, trees, and branches on the local filesystem and syncing via protocols (SSH/HTTPS). It has no awareness of pull requests, issues, CI pipelines, or code reviews. GitHub CLI (`gh`) is a client for GitHub's proprietary platform APIs. They complement each other: `git` manages local revision history, while `gh` orchestrates collaboration, reviews, and infrastructure automation.
</details>

<details>
<summary>2. How does `gh pr checkout <number>` streamline developer workflows compared to raw Git?</summary>
In raw Git, checking out a coworker's PR requires adding their fork as a remote, fetching their branch refs, and creating a local tracking branch. `gh pr checkout` queries the GitHub API to locate the fork or branch, configures a remote refspec mapping (`refs/pull/<number>/head`), fetches the commits, and switches the local working tree to that branch in a single command.
</details>

<details>
<summary>3. How do you extract structured data from `gh` in automated bash scripts without external dependencies?</summary>
By using the built-in `--json` and `--jq` flags. `gh` includes an embedded jq evaluation engine:
```bash
gh pr list --json number,title --jq '.[] | select(.title | startswith("fix"))'
```
This eliminates the need to install or maintain an external `jq` binary on the host system.
</details>

<details>
<summary>4. What happens when you run `gh repo fork --clone`?</summary>
It makes an API call to GitHub to fork the target repository into the authenticated user's account, clones the fork to the local workstation, and automatically configures two Git remotes: `origin` pointing to the user's fork (for pushing changes) and `upstream` pointing to the original parent repo (for fetching updates).
</details>

<details>
<summary>5. How can you inspect the logs of a failed GitHub Actions job without opening a browser?</summary>
Using the `gh run` command:
```bash
# View the failed log lines of the latest run
gh run view --log-failed

# Or live-stream the running workflow
gh run watch
```
</details>

<details>
<summary>6. How does `gh` authenticate non-interactively in headless Docker containers or CI pipelines?</summary>
By piping a token into `gh auth login --with-token`:
```bash
echo "$GITHUB_TOKEN" | gh auth login --with-token
```
Alternatively, `gh` automatically respects the `GITHUB_TOKEN` environment variable if exported in the shell.
</details>

<details>
<summary>7. What command allows a developer to add additional OAuth scopes to an existing `gh` session?</summary>
`gh auth refresh -s <scope>`:
```bash
# Example: Add permissions to manage repository secrets
gh auth refresh -s repo,admin:org
```
</details>

<details>
<summary>8. How do you create a custom reusable command in GitHub CLI?</summary>
Using `gh alias set`:
```bash
gh alias set co 'pr checkout'
# Execution: gh co 42
```
For shell scripts, prefix with an exclamation mark (`!`):
```bash
gh alias set sync-main '!git checkout main && git pull upstream main && git push origin main'
```
</details>

---

## 11. Hands-On Exercises

<details>
<summary>Exercise 1: Verify CLI installation and authentication status</summary>

**Goal:** Check the active login account and permissions.

```bash
gh auth status
```
</details>

<details>
<summary>Exercise 2: Create a pull request using `--fill`</summary>

**Goal:** Author a PR using current commit metadata.

```bash
git checkout -b test-pr-feature
git commit --allow-empty -m "feat(api): add health check probe"
git push -u origin test-pr-feature
gh pr create --fill
```
</details>

<details>
<summary>Exercise 3: Filter repository issues with `--jq`</summary>

**Goal:** Output issue titles authored by a specific user.

```bash
gh issue list --json title,author --jq '.[] | select(.author.login == "octocat") | .title'
```
</details>

<details>
<summary>Exercise 4: Live-stream a running GitHub Action</summary>

**Goal:** Monitor the active CI run in the terminal.

```bash
gh run watch
```
</details>

<details>
<summary>Exercise 5: Create a custom alias with shell parameters</summary>

**Goal:** Define an alias to quickly view open PRs for a specific label.

```bash
gh alias set prs-by-label 'pr list --label "$1"'
gh prs-by-label bug
```
</details>

<details>
<summary>Exercise 6: Set repository secrets via CLI</summary>

**Goal:** Store an encrypted secret using standard input.

```bash
echo "prod_secret_token_value" | gh secret set DATABASE_PASSWORD
```
</details>

---

## 12. Summary / Cheat Sheet

```text
================================================================================
                        GITHUB CLI (`gh`) CHEAT SHEET
================================================================================
Auth Status        : gh auth status | gh auth login --with-token
PR Workflows       : gh pr create --fill | gh pr checkout <num> | gh pr review --approve
PR Merge           : gh pr merge <num> --squash --delete-branch
Issue Triage       : gh issue list --assignee "@me" | gh issue create
Actions Debugging  : gh run list | gh run watch | gh run view --log-failed
Query Formatting   : gh <command> --json <fields> --jq '<query>'
Go Templating      : gh <command> --template '{{range .}}{{tablerow .title}}{{end}}'
Custom Aliases     : gh alias set <name> '<command>'  (prefix with ! for bash)
Browser Jump       : gh pr view --web | gh repo view --web | gh run view --web
================================================================================
```

---

## 13. Further Reading
- [GitHub CLI Manual & Documentation](https://cli.github.com/manual/)
- [GitHub CLI GitHub Repository](https://github.com/cli/cli)
- [Go Template Syntax Reference](https://pkg.go.dev/text/template)
- [Jq Language Manual](https://jqlang.github.io/jq/manual/)

---

[<- Previous: Module 07 Overview](../07-github-security/README.md) | [Module 08 Overview](./README.md) | [Next: GitHub REST & GraphQL APIs ->](./02-github-rest-and-graphql-api.md)
