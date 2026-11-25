---
title: "Git for Teams Playbook: Operational Standards & Cadence"
category: "04-devops"
subcategory: "git-github / 06-workflows-and-strategies"
tags: ["git", "team-playbook", "operational-cadence", "code-review-sla", "best-practices", "git-hygiene"]
difficulty: "Intermediate-Advanced"
estimated_read_time: "24 min"
prerequisites:
  - "06-workflows-and-strategies/01-trunk-based-development"
  - "06-workflows-and-strategies/04-conventional-commits-and-semantic-versioning"
last_updated: "2026-11-25"
---

# Git for Teams Playbook: Operational Standards & Cadence

> **Navigation**: [← Previous: 07-environment-branching-anti-patterns.md](file:///e:/Learning/engineering-notes/04-devops/git-github/06-workflows-and-strategies/07-environment-branching-anti-patterns.md) | [Module README](file:///e:/Learning/engineering-notes/04-devops/git-github/06-workflows-and-strategies/README.md) | [Next Module: 07-github-security →](file:///e:/Learning/engineering-notes/04-devops/git-github/07-github-security/README.md)

---

## Table of Contents
1. [What & Why](#1-what--why)
2. [The Daily Developer Cadence](#2-the-daily-developer-cadence)
3. [Branch Naming Standards](#3-branch-naming-standards)
4. [Commit Hygiene Standards](#4-commit-hygiene-standards)
5. [Pull Request Review SLAs & Etiquette](#5-pull-request-review-slas--etiquette)
6. [The "Stop the Line" Broken Main Protocol](#6-the-stop-the-line-broken-main-protocol)
7. [Conflict Resolution Team Runbook](#7-conflict-resolution-team-runbook)
8. [Practical Examples](#8-practical-examples)
9. [Best Practices Summary](#9-best-practices-summary)
10. [Common Mistakes](#10-common-mistakes)
11. [Team Maturity Matrix: Low vs High Maturity Teams](#11-team-maturity-matrix-low-vs-high-maturity-teams)
12. [Interview Questions & Answers](#12-interview-questions--answers)
13. [Practice Exercises](#13-practice-exercises)
14. [Summary & Quick Reference Runbook](#14-summary--quick-reference-runbook)
15. [Further Reading](#15-further-reading)

---

## 1. What & Why

### What is the Git for Teams Playbook?
The Git for Teams Playbook is an operational handbook that codifies engineering conventions, daily cadences, review response agreements (SLAs), branch naming standards, commit hygiene rules, and emergency triage protocols across an engineering organization.

### Why Do Teams Need It?
1. **Frictionless Collaboration**: When all engineers follow identical branch naming, commit standards, and PR workflows, cognitive load drops and onboarding takes days rather than weeks.
2. **Predictable Velocity**: Predictable code review turnaround times prevent pull requests from languishing in review queues for days.
3. **High Signal Git History**: Consistent Conventional Commits enable automated changelog generation and painless debugging with `git bisect`.
4. **Resilient Disaster Recovery**: Clear operational protocols ("Stop the Line") ensure production outages caused by broken commits are mitigated in minutes.

---

## 2. The Daily Developer Cadence

```
                          The Daily Developer Rhythm:

 09:00 AM ──► Morning Sync & Local Trunk Update
              `git switch main && git pull --rebase`
                     │
                     ▼
 09:30 AM ──► First Priority: Review Teammate Pull Requests (Clear PR queue!)
                     │
                     ▼
 10:30 AM ──► Pick Task & Branch off latest `main`:
              `git switch -c feat/ticket-123-short-desc`
                     │
                     ▼
 02:00 PM ──► Make Atomic Commits & Run Local Unit Tests (<200 lines)
                     │
                     ▼
 04:00 PM ──► Rebase on Trunk & Open Short-Lived PR (<24h lifespan)
              `git fetch origin main && git rebase origin/main`
              `gh pr create`
                     │
                     ▼
 05:00 PM ──► Peer Review Approved -> Squash & Merge to Main -> Auto Deploy!
```

---

## 3. Branch Naming Standards

All branches must follow the scoped, lowercase kebab-case convention:

```text
<category>/<issue-tracker-id>-<short-description>
```

### Approved Categories:
- `feat/`: New feature development (e.g. `feat/PROJ-102-oauth-google`)
- `fix/`: Bug fixes (e.g. `fix/PROJ-145-null-pointer-login`)
- `refactor/`: Code reorganization with no functional changes (e.g. `refactor/PROJ-201-extract-auth-service`)
- `perf/`: Performance optimizations (e.g. `perf/PROJ-304-database-indexing`)
- `docs/`: Documentation additions (e.g. `docs/PROJ-412-api-openapi-spec`)
- `chore/`: CI/CD, dependency updates (e.g. `chore/deps-bump-node20`)

---

## 4. Commit Hygiene Standards

1. **Atomic Commits**: Each commit represents a single logical unit of work that compiles and passes tests independently.
2. **Conventional Commits**: Format strictly as `type(scope): imperative present tense subject`.
3. **No Unfinished Code Left Untoggled**: Incomplete work must be concealed behind a feature flag.
4. **No Binary Artifacts**: Never commit `.tar.gz`, `.jar`, `.exe`, or SQLite databases to Git. Use Git LFS or artifact registries.
5. **Clean Working Tree**: Clean up WIP and squash commits before opening a final pull request.

---

## 5. Pull Request Review SLAs & Etiquette

### Team Service Level Agreements (SLAs):
- **PR Size Target**: $< 250$ lines of code changes (excluding auto-generated lockfiles/snapshots).
- **Review Response SLA**:
  - Small PRs ($< 150$ lines): **Reviewed within 2 business hours**.
  - Medium PRs ($150 - 300$ lines): **Reviewed within 4 business hours**.
  - Emergency Hotfixes: **Reviewed immediately (P0 interrupt)**.
- **Author Response SLA**: Address or acknowledge reviewer comments within 4 hours.

### Review Etiquette:
- Use **Conventional Comments** (`nit:`, `suggestion:`, `issue:`, `praise:`).
- Always explain **Why** a change is requested, not just "change this".
- If a discussion requires more than 3 back-and-forth messages, jump on a 5-minute huddle and post the agreed-upon summary back on the PR.

---

## 6. The "Stop the Line" Broken Main Protocol

If an unexpected defect slips into `main` and breaks automated tests or staging deployments:

```
                  "STOP THE LINE" Emergency Protocol:

 1. TRIGGER: Main build fails in CI or staging health checks alert
                     │
                     ▼
 2. ANNOUNCE: Post in team Slack channel: "MAIN IS BROKEN. ALL MERGES HALTED."
                     │
                     ▼
 3. TRIAGE (The 5-Minute Rule):
    ├── Can the breaking commit be fixed in < 5 minutes?
    │     ├── YES ──► Author pushes immediate fast-forward fix
    │     └── NO
    │          │
    │          ▼
    └──────────► REVERT IMMEDIATELY:
                 `git switch main && git pull`
                 `git revert -m 1 <breaking-merge-sha>`
                 `git push origin main`
                     │
                     ▼
 4. VERIFY: Main CI pipeline turns GREEN
                     │
                     ▼
 5. RESUME: Merges unlocked. Author fixes issue in a new isolated branch.
```

---

## 7. Conflict Resolution Team Runbook

When a PR shows merge conflicts against `main`:

```bash
# ---------------------------------------------------------------------------
# Team Conflict Resolution Procedure
# ---------------------------------------------------------------------------

# Step 1: Update local trunk
git switch main
git pull --rebase origin main

# Step 2: Switch to feature branch
git switch feat/my-feature

# Step 3: Rebase onto trunk (NOT git merge main!)
git rebase origin/main

# Step 4: Resolve conflict markers in affected files
git status -s
# Open conflicted files, resolve markers (<<<<<<< HEAD ... >>>>>>>)

# Step 5: Stage resolved files
git add path/to/resolved-file.ts

# Step 6: Continue rebase
git rebase --continue

# Step 7: Push safely using force-with-lease
git push --force-with-lease origin feat/my-feature
```

---

## 8. Practical Examples

### Example 1: Local Pre-Push Checklist Script (`validate.sh`)
```bash
#!/usr/bin/env bash
set -e
echo "Running Team Pre-Push Health Check..."

# Check diff size
DIFF_LINES=$(git diff --stat origin/main...HEAD | tail -n 1 | awk '{print $4 + $6}')
if [ "$DIFF_LINES" -gt 350 ]; then
  echo "::warning::PR is large ($DIFF_LINES lines). Consider splitting into smaller PRs."
fi

# Run fast linter and tests
npm run lint
npm test -- --bail
echo "Validation passed! Ready to push."
```

### Example 2: Pruning Dead Local Tracking Branches
```bash
# Prune local references to deleted remote branches
git fetch --prune origin

# Delete all local branches already merged into main
git branch --merged main | grep -v "^\*\|main" | xargs -r git branch -d
```

---

## 9. Best Practices Summary

1. **Review PRs First Thing in the Morning**: Unblocking teammates is higher priority than writing new code.
2. **Rebase Local Work Frequently**: Run `git fetch && git rebase origin/main` twice daily to keep conflicts minimal.
3. **Always Use `--force-with-lease`**: Never execute bare `git push -f`.
4. **Keep Commits Clean via Squash on Merge**: Keep granular commits while developing, but squash-merge to maintain a clean linear trunk.
5. **Automate Everything**: Use Commitlint, ESLint, Prettier, and GitHub Actions to enforce standards automatically rather than policing them manually in reviews.

---

## 10. Common Mistakes

| Mistake | Consequence | Root Cause | Fix / Prevention |
|:---|:---|:---|:---|
| Ignoring PR reviews for 3 days | Blocked sprint velocity; massive merge conflicts | Lack of team review SLA | Treat reviews as morning priority |
| Leaving broken builds on main overnight | Blocks entire engineering team from shipping | Trying to fix complex bug in place | Revert breaking commit within 5 minutes |
| Resolving conflicts by merging `main` into branch | Noisy merge bubbles and tangled history | Running `git merge main` | Always use `git rebase origin/main` |
| Pushing giant 2,000-line PRs | Reviewer fatigue; bugs slip into production | Not slicing tasks into small increments | Split work behind Feature Flags |
| Committing secrets (`.env`) | Security incident; compromised credentials | Missing `.gitignore` entry | Add `.env` to `.gitignore` and use secret scanners |

---

## 11. Team Maturity Matrix: Low vs High Maturity Teams

| Practice | Low Maturity Team | High Maturity (Elite) Team |
|:---|:---|:---|
| **Branching Model** | GitFlow with long-lived feature branches | **Trunk-Based Development (< 24h branches)** |
| **Commit Messages** | "fixed stuff", "wip", "update" | **Conventional Commits (`feat(auth): ...`)** |
| **PR Review Time** | 3 to 7 days | **Under 2 business hours** |
| **Handling Broken Main** | Debugging for hours while blocking others | **Immediate `git revert` within 5 minutes** |
| **Releases** | Scheduled monthly stressful releases | **Continuous on-demand daily deployments** |
| **Deployment Gates**| Long manual QA cycles | **Automated CI/CD + Feature Flags** |

---

## 12. Interview Questions & Answers

<details>
<summary><strong>Q1: What is the "Stop the Line" protocol when the main branch build breaks?</strong></summary>

The "Stop the Line" protocol (borrowed from Toyota's Andon cord manufacturing principle) treats a broken default branch as an immediate emergency. All engineers pause merging new PRs into `main`. The author of the breaking commit has 5 to 10 minutes to deliver an obvious fix. If a trivial fix cannot be applied immediately, the breaking merge is reverted instantly using `git revert` to restore `main` to a deployable, green state.
</details>

<details>
<summary><strong>Q2: Why is a strict Code Review SLA (e.g. < 2 hours) critical for agile velocity?</strong></summary>

Code review wait time is typically the single largest bottleneck in the software development lifecycle. When a developer must wait 2 days for a review, they are forced to switch context to a new task. When review comments finally arrive, they must context-switch back, re-learn their old code, and re-test. Fast reviews (< 2 hours) allow the author to address feedback while the context is fresh in their working memory, eliminating context switching and accelerating sprint delivery.
</details>

<details>
<summary><strong>Q3: Why should developers resolve conflicts via `git rebase origin/main` rather than `git merge main`?</strong></summary>

Running `git merge main` into a feature branch creates a superfluous "Merge branch 'main' into feature" commit. If done multiple times during a week-long branch, the feature history becomes a tangled web of cross-merges that is difficult to review and ruins `git bisect`.
Rebasing (`git rebase origin/main`) replays the feature commits cleanly on top of the latest trunk, keeping the commit graph strictly linear and conflict resolution atomic.
</details>

<details>
<summary><strong>Q4: What are Conventional Comments and how do they improve code review efficiency?</strong></summary>

Conventional Comments is a standardized labeling syntax for code review comments:
- `praise:` Positive feedback.
- `nit:` Non-blocking minor suggestion (author is free to ignore).
- `suggestion:` Concrete alternative proposal.
- `issue:` Blocking bug or defect that must be resolved.
- `question:` Clarification request.
It removes emotional ambiguity by explicitly communicating the severity and expectation of every comment.
</details>

<details>
<summary><strong>Q5: How does keeping pull request diffs under 250 lines improve code quality?</strong></summary>

Cognitive science and empirical software engineering studies (such as SmartBear's Cisco study) prove that reviewer defect detection density drops drastically after reviewing 200–300 lines of code. Small PRs are reviewed thoroughly, receive meaningful feedback, and can be reviewed in 10 minutes. Large 1,000-line PRs overwhelm reviewers, leading to superficial "LGTM" approvals where serious architectural bugs and security vulnerabilities slip through undetected.
</details>

<details>
<summary><strong>Q6: How do you safely push a feature branch after rebasing onto main?</strong></summary>

Always use `git push --force-with-lease` (or `--force-with-lease --force-if-includes`). Never use unchecked `git push --force`, which can overwrite commits pushed by a collaborator who may have contributed to that branch in the interim.
</details>

<details>
<summary><strong>Q7: How should teams handle local branch cleanup to prevent disk clutter?</strong></summary>

1. Enable **"Automatically delete head branches"** in GitHub repository settings.
2. Run `git fetch --prune origin` periodically to purge deleted remote tracking refs.
3. Use a cleanup script or alias (`git branch --merged main | grep -v 'main' | xargs git branch -d`) to delete merged local branches.
</details>

<details>
<summary><strong>Q8: What is the recommended procedure when a code review discussion becomes circular or contentious?</strong></summary>

If a discussion thread exceeds 3 back-and-forth exchanges, stop typing. Jump on a 5-minute video call or huddle. Real-time voice communication resolves misunderstandings, tone misinterpretations, and design disagreements in minutes. After reaching consensus, post a concise summary comment on the PR detailing the agreed-upon resolution and resolve the thread.
</details>

---

## 13. Practice Exercises

<details>
<summary><strong>Exercise 1: Configure Local Prune on Fetch</strong></summary>

**Scenario**: Automatically prune deleted remote refs on every `git fetch`.
```bash
git config --global fetch.prune true
```
</details>

<details>
<summary><strong>Exercise 2: Create a Clean Branch Naming Structure</strong></summary>

**Scenario**: Create a branch following team standard.
```bash
git switch -c feat/PROJ-104-oauth-login main
```
</details>

<details>
<summary><strong>Exercise 3: Simulate Immediate Revert of Bad Merge</strong></summary>

**Scenario**: Execute immediate revert on trunk.
```bash
git switch main
LAST_MERGE=$(git rev-parse HEAD)
git revert -m 1 $LAST_MERGE --no-edit
git log -1 --oneline
```
</details>

<details>
<summary><strong>Exercise 4: Clean Up Merged Local Branches</strong></summary>

**Scenario**: Delete all local branches that are already merged into main.
```bash
git branch --merged main | grep -v "^\*\|main" | xargs -r git branch -d
```
</details>

<details>
<summary><strong>Exercise 5: Add a Safe Force Push Alias</strong></summary>

**Scenario**: Configure global alias `pf` for team safety.
```bash
git config --global alias.pf "push --force-with-lease --force-if-includes"
```
</details>

<details>
<summary><strong>Exercise 6: Check Size of Current Branch Diff</strong></summary>

**Scenario**: Verify branch diff size before opening PR.
```bash
git diff --shortstat origin/main...HEAD
```
</details>

---

## 14. Summary & Quick Reference Runbook

```text
The High-Performance Team Golden Rules:
1. CADENCE: Pull main daily; prioritize reviewing peer PRs every morning.
2. BATCH SIZE: Keep PRs under 250 lines of code.
3. LIFESPAN: Merge branches within 24 hours.
4. REVIEW SLA: Review small PRs within 2 hours using Conventional Comments.
5. CONFLICTS: Always resolve conflicts via `git rebase origin/main`.
6. MAIN BREAKAGE: If main breaks, revert within 5 minutes ("Stop the line").
7. PUSH SAFETY: Never use `git push -f`; always use `git push --force-with-lease`.
```

---

## 15. Further Reading
- [Conventional Comments Guidelines](https://conventionalcomments.org/)
- [Google Engineering Practices - Code Review Developer Guide](https://google.github.io/eng-practices/)
- [DORA Capabilities - Continuous Integration](https://cloud.google.com/devops/state-of-devops)
- [Module README](file:///e:/Learning/engineering-notes/04-devops/git-github/06-workflows-and-strategies/README.md)
