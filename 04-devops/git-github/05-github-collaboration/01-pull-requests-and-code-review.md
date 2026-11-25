---
title: "GitHub Pull Requests and Code Review Workflows"
category: "04-devops"
subcategory: "git-github / 05-github-collaboration"
tags: ["github", "pull-requests", "code-review", "collaboration", "suggested-changes", "draft-pr"]
difficulty: "Intermediate"
estimated_read_time: "24 min"
prerequisites:
  - "02-commands/06-branching"
  - "02-commands/07-merging"
last_updated: "2026-11-25"
---

# GitHub Pull Requests and Code Review Workflows

> **Navigation**: [← Module README](file:///e:/Learning/engineering-notes/04-devops/git-github/05-github-collaboration/README.md) | [Next: 02-issues-labels-and-milestones.md →](file:///e:/Learning/engineering-notes/04-devops/git-github/05-github-collaboration/02-issues-labels-and-milestones.md)

---

## Table of Contents
1. [What & Why](#1-what--why)
2. [The Pull Request Lifecycle](#2-the-pull-request-lifecycle)
3. [Core Concepts](#3-core-concepts)
4. [Syntax & Markdown Features](#4-syntax--markdown-features)
5. [Practical Examples](#5-practical-examples)
6. [Best Practices & Review Etiquette](#6-best-practices--review-etiquette)
7. [Common Mistakes](#7-common-mistakes)
8. [Performance, Security & Compliance](#8-performance-security--compliance)
9. [Comparison: PR Merge Strategies](#9-comparison-pr-merge-strategies)
10. [Interview Questions & Answers](#10-interview-questions--answers)
11. [Practice Exercises](#11-practice-exercises)
12. [Summary & Cheat Sheet](#12-summary--cheat-sheet)
13. [Further Reading](#13-further-reading)

---

## 1. What & Why

### What is a Pull Request?
A Pull Request (PR) is GitHub's collaborative mechanism for proposing, reviewing, discussing, and integrating changes from a topic branch into a base branch (typically `main`). Beyond a simple Git merge, a PR serves as a dedicated communication thread, code diff viewer, CI/CD status dashboard, and compliance audit trail.

### Why Do We Use It?
1. **Defect Prevention**: Peer code reviews catch syntax bugs, logical edge cases, security vulnerabilities, and architectural drift before code reaches production.
2. **Knowledge Sharing**: Teammates learn codebase conventions, new language features, and architectural decisions by reading and discussing PR diffs.
3. **Automated Quality Gates**: Connects CI status checks, automated security scans (SAST, secret scanning), and code coverage directly to the merge button.
4. **Regulatory Audit Trails**: Financial and healthcare compliance frameworks (SOC2, HIPAA, PCI-DSS) strictly mandate independent, documented peer reviews prior to production deployments.

---

## 2. The Pull Request Lifecycle

```
                      The Pull Request State Machine:

 1. [ Developer ] ──► git push origin feature/auth
                             │
                             ▼
 2. [ GitHub UI / CLI ] ──► Create Pull Request (Draft or Open)
                             │
                             ▼
 3. [ Automated CI/CD ] ──► Linters, unit tests, security scans execute
     ├── FAILED  ──► Author pushes fixes (auto-updates PR)
     └── PASSED
             │
             ▼
 4. [ Reviewers Assigned ] (via CODEOWNERS or Manual Request)
     ├── Comment Only
     ├── Request Changes (Blocks merge!)
     └── Approve
             │
             ▼
 5. [ Conversation Resolution ] ──► All review threads resolved
                             │
                             ▼
 6. [ Merge Gate Satisfied ] ──► Checks green, approvals granted, branch up to date
                             │
                             ▼
 7. [ Integrated to Main ] (Merge Commit / Squash & Merge / Rebase & Merge)
     └── Topic branch deleted automatically
```

---

## 3. Core Concepts

### 1. Draft Pull Requests
A Draft PR signals work-in-progress. It cannot be merged, and code reviewers are not officially notified until the author clicks **"Ready for review"**. It allows running early CI test suites and sharing WIP diffs with peers safely.

### 2. Suggested Changes (````suggestion`)
Reviewers can write exact drop-in code replacements directly inside review comments. Authors can apply the suggested diff into their branch with a single click, automatically generating a commit.

### 3. Review States:
- **Comment**: General feedback without formal approval or block.
- **Approve**: Satisfies required review count.
- **Request Changes**: Explicitly blocks merging until the author pushes updates and the reviewer re-evaluates.

### 4. Conventional Comments
A standard taxonomy used to clarify the intent and severity of review comments:
- `praise:` Highlight outstanding code or clever solutions.
- `nit:` Non-blocking trivial preference (formatting, naming).
- `suggestion:` Proposed alternative implementation.
- `issue:` Critical bug, logic error, or security flaw that must be fixed.
- `question:` Clarification required.

---

## 4. Syntax & Markdown Features

### 1. Suggested Changes Block
In GitHub review comments:
````markdown
```suggestion
const timeoutMs = 5000;
const retryCount = 3;
```
````
This renders an interactive diff in the GitHub UI with an **"Apply suggestion"** button.

### 2. Closing Keywords
Linking PRs to issues so that merging the PR automatically closes the issue:
```markdown
Resolves #42
Fixes #128
Closes #256
```

### 3. Task Lists
```markdown
### Pre-Merge Checklist
- [x] Unit tests written and passing
- [x] OpenAPI documentation updated
- [ ] Database migration script executed on staging
```

---

## 5. Practical Examples

### Example 1: Creating a PR via GitHub CLI (`gh`)
```bash
# Push branch and open interactive PR prompt
git push -u origin feature/user-profile
gh pr create --title "feat(user): implement profile avatar upload" \
             --body "Closes #84. Adds S3 upload support for user profile avatars." \
             --reviewer team-frontend \
             --label "enhancement"
```

### Example 2: Checking Out a PR Locally for Testing
```bash
# Checkout PR #42 into a local branch automatically
gh pr checkout 42

# Run local tests against the PR diff
npm test
```

### Example 3: Submitting Batch Review Comments
```bash
# Start a review session, add comments across files, and submit all at once:
gh pr review 42 --approve --body "Architecture looks solid. Verified test coverage."
```

### Example 4: Automatic Branch Deletion Configuration
In repository settings (`Settings -> General`):
- Check **"Automatically delete head branches"**.
When a PR is merged, GitHub instantly deletes `feature/user-profile` on the remote, eliminating dead branch clutter.

### Example 5: Resolving Merge Conflicts via GitHub Web UI
If conflicts are simple (textual diffs without binary files), GitHub enables the **"Resolve conflicts"** button in the PR view, allowing inline conflict marker editing directly in the browser.

---

## 6. Best Practices & Review Etiquette

1. **Keep PRs Small and Atomic**: Research shows review efficacy drops drastically above 400 lines of code. Keep PRs under 200–300 lines of code changes whenever possible.
2. **Provide Rich PR Descriptions**: State **Why** the change was made, link the issue, and include before/after screenshots or terminal outputs for UI and CLI changes.
3. **Use Draft PRs for Early Spikes**: Open Draft PRs to solicit early architectural feedback before spending days refining implementation.
4. **Differentiate Blocking vs Non-Blocking Comments**: Prefix trivial remarks with `nit:` so authors know they do not block merging.
5. **Respond to Every Comment**: Authors should reply or react with emojis to all reviewer questions to confirm feedback was seen.
6. **Never Force-Push Blindly During Active Review**: Pushing force updates makes incremental diff viewing harder for reviewers. Prefer incremental fix commits, and squash upon merging.

---

## 7. Common Mistakes

| Mistake | Consequence | Root Cause | Fix / Prevention |
|:---|:---|:---|:---|
| Massive 2,000-line PR | Reviewer fatigue; bugs slip through; reviews delayed | Bundling multiple features together | Split into sequential, smaller PRs |
| Merging with unresolved conversations | Forgotten feedback or unapplied fixes | Bypassing review resolution | Enable "Require conversation resolution" rule |
| Vague PR title like "updates" | Useless git history and changelogs | Poor naming habits | Use Conventional Commits (`feat(auth): ...`) |
| Reviewers approving without reading | Outages and defects in production | "LGTM" rubber-stamping culture | Enforce minimum review requirements and tests |
| Applying 10 suggestions as 10 micro-commits | Cluttered branch history | Clicking suggestions one-by-one | Use "Add suggestion to batch" then commit |
| Pushing directly to `main` | Skips CI checks and peer reviews | Unprotected default branch | Enable GitHub Branch Protection Rules |

---

## 8. Performance, Security & Compliance

- **SOC2 & ISO27001 Access Controls**:
  - Setting **"Dismiss stale pull request approvals when new commits are pushed"** ensures any new code pushed after approval requires re-review.
  - Setting **"Require review from Code Owners"** guarantees domain experts approve changes touching critical paths.
- **Prevent Self-Review**: Enable **"Restrict who can dismiss reviews"** and ensure authors cannot approve their own PRs.
- **Audit Logging**: GitHub audit logs record every review event, approval, dismissal, and merge action with cryptographic timestamps and actor identities.

---

## 9. Comparison: PR Merge Strategies

```
1. Create a Merge Commit:
   main:    A ─── B ─── C ─── M (Merge commit with 2 parents)
                  \         /
   feature:        D ─── E ┘
   (Preserves complete branch topology and all granular commits)

2. Squash and Merge:
   main:    A ─── B ─── C ─── F (Single commit containing D + E)
   (Combines all feature commits into 1 clean commit on main)

3. Rebase and Merge:
   main:    A ─── B ─── C ─── D' ─── E' (Linear replay of individual commits)
   (Preserves individual commit messages but rewrites hashes linearly)
```

| Dimension | Merge Commit | Squash and Merge | Rebase and Merge |
|:---|:---|:---|:---|
| **Git Topology** | Non-linear (explicit merge node) | Strictly linear | Strictly linear |
| **Commit Count on Main** | All feature commits + 1 merge | **Exactly 1 commit** | All feature commits (replayed) |
| **History Readability** | Noisy if feature had WIP commits | **Cleanest & most readable** | Clean if branch commits were curated |
| **`git bisect` Ease** | Can be complex | **Easiest** | Medium |
| **Recommended For** | Long-lived release branches | **Standard feature PRs (Default)** | Meticulously curated feature branches |

---

## 10. Interview Questions & Answers

<details>
<summary><strong>Q1: What are the three merge strategies available for GitHub Pull Requests, and when should you choose each?</strong></summary>

1. **Squash and Merge**: Combines all commits from the PR into a single new commit on the base branch. Best for daily feature branches where developers made messy WIP commits. Produces the cleanest, linear history.
2. **Rebase and Merge**: Replays each commit from the PR onto the tip of the base branch without a merge commit. Best for mature projects where authors curate atomic, Conventional Commits that each pass tests independently.
3. **Merge Commit (`--no-ff`)**: Preserves all individual commits and adds a 2-parent merge commit. Best for merging long-running release branches or large architectural initiatives where preserving the exact branching topology is required.
</details>

<details>
<summary><strong>Q2: What is the purpose of a Draft Pull Request?</strong></summary>

A Draft Pull Request allows developers to publish early work-in-progress code without notifying assignees or CODEOWNERS. It automatically blocks the merge button and disables status check requirements. It allows running automated CI tests in GitHub Actions and discussing architectural direction with peers before the PR is marked "Ready for review".
</details>

<details>
<summary><strong>Q3: How do GitHub "Closing Keywords" work in pull request descriptions?</strong></summary>

When words like `close`, `closes`, `closed`, `fix`, `fixes`, `fixed`, `resolve`, `resolves`, or `resolved` followed by an issue reference (e.g. `Fixes #42`) are included in a PR description or commit message on the default branch, GitHub automatically closes the referenced issue the instant the PR is merged into the default branch.
</details>

<details>
<summary><strong>Q4: Why should repositories enable "Dismiss stale pull request approvals when new commits are pushed"?</strong></summary>

If this setting is disabled, a developer could get approval on a benign change, and subsequently push a malicious or untested commit to the branch and immediately merge it without further review. Enabling this setting ensures that any new commit pushed to an approved branch resets approvals back to pending, requiring re-review.
</details>

<details>
<summary><strong>Q5: How does the "Suggested Changes" feature in GitHub PR code reviews work?</strong></summary>

When viewing a diff, a reviewer selects one or more lines and clicks the **"Insert suggestion"** icon (or formats a fenced block as ````suggestion`). The reviewer edits the code within the block. In the PR interface, GitHub renders a diff of the suggestion. The author (or anyone with write access) can click **"Commit suggestion"** or batch multiple suggestions into a single commit, applying the changes directly to the branch without touching a local terminal.
</details>

<details>
<summary><strong>Q6: What is the benefit of enabling "Require conversation resolution" in Branch Protection Rules?</strong></summary>

It prevents pull requests from being merged if any review comment thread remains unresolved. This guarantees that authors cannot accidentally overlook reviewer feedback, unanswered questions, or requested refactorings before deploying code.
</details>

<details>
<summary><strong>Q7: How do you checkout a colleague's pull request locally using the GitHub CLI?</strong></summary>

Run:
```bash
gh pr checkout <pr-number>
```
The GitHub CLI configures the remote tracking branch, fetches the PR ref, and switches your local working tree to that exact branch automatically.
</details>

<details>
<summary><strong>Q8: What is the difference between a Pull Request and a standard Git merge?</strong></summary>

A standard Git merge is a local operation that combines two commit graphs into a target branch. A Pull Request is a platform-level abstraction built by GitHub on top of Git. It includes discussion threads, inline line-by-line code reviews, suggested changes, CI status checks, security scanning results, deployment approvals, issue linking, and enterprise audit logs.
</details>

---

## 11. Practice Exercises

<details>
<summary><strong>Exercise 1: Create a Pull Request via GitHub CLI</strong></summary>

**Scenario**: Create a branch, commit a change, push, and open a PR with `gh`.
```bash
git switch -c feature/avatar-upload
echo "avatar logic" > avatar.ts && git add avatar.ts && git commit -m "feat(user): add avatar logic"
git push -u origin feature/avatar-upload

gh pr create --title "feat(user): add avatar logic" --body "Implements avatar upload. Resolves #12."
```
</details>

<details>
<summary><strong>Exercise 2: Create a Draft Pull Request</strong></summary>

**Scenario**: Open a PR in draft mode.
```bash
gh pr create --draft --title "wip: experimental search" --body "Draft PR for feedback"
```
</details>

<details>
<summary><strong>Exercise 3: Review a PR from the Command Line</strong></summary>

**Scenario**: Approve a PR with a comment using `gh`.
```bash
gh pr review 12 --approve --body "LGTM. Verified unit tests pass."
```
</details>

<details>
<summary><strong>Exercise 4: View PR Diff in Terminal</strong></summary>

**Scenario**: Inspect the diff of an open PR directly in the terminal.
```bash
gh pr diff 12
```
</details>

<details>
<summary><strong>Exercise 5: Merge a PR Using Squash Strategy via CLI</strong></summary>

**Scenario**: Squash and merge PR #12 and delete remote branch.
```bash
gh pr merge 12 --squash --delete-branch
```
</details>

<details>
<summary><strong>Exercise 6: Format a Conventional Comment</strong></summary>

**Scenario**: Draft a non-blocking review comment using conventional comment syntax.
```markdown
nit: Consider extracting magic number `86400` into a named constant `SECONDS_IN_DAY` for readability.
```
</details>

---

## 12. Summary & Cheat Sheet

```bash
# GitHub CLI PR Essentials
gh pr create                   # Interactive PR creation
gh pr create --draft           # Create Draft PR
gh pr list                     # List open PRs in repo
gh pr checkout <id>            # Checkout PR branch locally
gh pr diff <id>                # View diff of PR
gh pr review <id> --approve    # Approve PR
gh pr review <id> --request-changes -b "Needs tests"
gh pr merge <id> --squash --delete-branch # Merge and cleanup branch

# Closing Keywords in PR Body:
# Closes #123
# Fixes #456
# Resolves #789
```

---

## 13. Further Reading
- [GitHub Docs - About Pull Requests](https://docs.github.com/en/pull-requests/collaborating-with-pull-requests/proposing-changes-to-your-work-with-pull-requests/about-pull-requests)
- [Conventional Comments Specification](https://conventionalcomments.org/)
- [Google Engineering Practices - Code Review Developer Guide](https://google.github.io/eng-practices/review/)
- [05-github-collaboration/02-issues-labels-and-milestones.md](file:///e:/Learning/engineering-notes/04-devops/git-github/05-github-collaboration/02-issues-labels-and-milestones.md)
