---
title: "Trunk-Based Development: Principles & Practice"
category: "04-devops"
subcategory: "git-github / 06-workflows-and-strategies"
tags: ["git", "trunk-based-development", "branching-strategies", "dora-metrics", "continuous-integration", "feature-flags"]
difficulty: "Intermediate-Advanced"
estimated_read_time: "24 min"
prerequisites:
  - "02-commands/06-branching"
  - "02-commands/07-merging"
  - "05-github-collaboration/01-pull-requests-and-code-review"
last_updated: "2026-11-25"
---

# Trunk-Based Development: Principles & Practice

> **Navigation**: [← Module README](file:///e:/Learning/engineering-notes/04-devops/git-github/06-workflows-and-strategies/README.md) | [Next: 02-gitflow-and-github-flow.md →](file:///e:/Learning/engineering-notes/04-devops/git-github/06-workflows-and-strategies/02-gitflow-and-github-flow.md)

---

## Table of Contents
1. [What & Why](#1-what--why)
2. [Core Principles of Trunk-Based Development](#2-core-principles-of-trunk-based-development)
3. [The Branch Lifecycle Comparison](#3-the-branch-lifecycle-comparison)
4. [Enabling Technologies: Feature Flags & Dark Launching](#4-enabling-technologies-feature-flags--dark-launching)
5. [DORA Metrics Correlation (Elite Engineering Performance)](#5-dora-metrics-correlation-elite-engineering-performance)
6. [Step-by-Step Developer Workflow](#6-step-by-step-developer-workflow)
7. [Practical Examples](#7-practical-examples)
8. [Best Practices](#8-best-practices)
9. [Common Mistakes & Anti-Patterns](#9-common-mistakes--anti-patterns)
10. [Comparison Matrix: TBD vs Feature Branching vs GitFlow](#10-comparison-matrix-tbd-vs-feature-branching-vs-gitflow)
11. [Interview Questions & Answers](#11-interview-questions--answers)
12. [Practice Exercises](#12-practice-exercises)
13. [Summary & Cheat Sheet](#13-summary--cheat-sheet)
14. [Further Reading](#14-further-reading)

---

## 1. What & Why

### What is Trunk-Based Development?
Trunk-Based Development (TBD) is a source-control branching model where all developers merge small, frequent code increments directly into a single central branch called the "trunk" (typically `main`) multiple times a day. Feature branches are strictly ephemeral—typically living for less than a few hours to at most one day.

### Why Do Elite Teams Use It?
1. **Elimination of "Merge Hell"**: Long-lived feature branches accumulate hundreds of divergent commits, leading to massive, painful merge conflicts that halt sprint momentum.
2. **True Continuous Integration**: CI is not just running automated tests on an isolated branch. Continuous Integration literally means **integrating code continuously** into the shared mainline.
3. **Faster Time to Market**: Features reach production incrementally behind feature flags rather than waiting weeks for an arbitrary "release freeze" milestone.
4. **Immediate Defect Detection**: Breaking changes are discovered within hours by teammates rather than weeks later during integration testing.

---

## 2. Core Principles of Trunk-Based Development

```
Trunk-Based Development Branch Topology:

                     [ Trunk / main ]
 ───────────────────●──────────●──────────●──────────●────────► Production Deploy
                   / \        / \        / \        / \
                  /   \      /   \      /   \      /   \
                 ●─────●    ●─────●    ●─────●    ●─────●
                Short-lived Short-lived Short-lived Short-lived
                 Branch A    Branch B    Branch C    Branch D
                (1-4 hours) (2-6 hours) (3 hours)   (Half day)
```

### The 4 Foundational Pillars:
1. **Short-Lived Feature Branches**: A branch should contain no more than 1–3 commits and live for fewer than 24 hours.
2. **Small Batch Sizes**: Changes are broken down into bite-sized units (<200 lines of code) that can be reviewed and merged in minutes.
3. **Decoupled Deployment from Release**: Incomplete features are committed to the trunk and deployed to production continuously, concealed behind dynamic runtime **Feature Flags**.
4. **Relentless Automated Testing**: The trunk must remain deployable at all times. A broken build on trunk halts all merges until resolved.

---

## 3. The Branch Lifecycle Comparison

```
Traditional Feature Branching (Painful Integration):
main:    ──●───────────────────────────────────────────────────● (Big Merge Conflict!)
            \                                                 /
feature:     ●───●───●───●───●───●───●───●───●───●───●───●───●
             (2 to 4 weeks of divergence - High Risk)

Trunk-Based Development (Continuous Flow):
main:    ──●───●───●───●───●───●───●───●───●───●───●───●───● (Always Green & Deployable)
            \ /     \ /     \ /     \ /     \ /     \ /
            (Tiny, frequent merges - Low Risk, zero merge debt)
```

---

## 4. Enabling Technologies: Feature Flags & Dark Launching

The primary objection to TBD is: *"How do we merge incomplete features without breaking production for users?"*
The solution is **Feature Toggles (Feature Flags)**:

```typescript
// Code merged into trunk and deployed to production today:
if (featureFlags.isEnabled('NEW_STRIPE_PAYMENT_FLOW', currentUser)) {
  return executeStripeV3Payment(order);
} else {
  return executeLegacyPayPalPayment(order); // Active for 100% of real users
}
```

The new code is deployed to production (Dark Launching), tested internally by developers, and toggled on for real users via a configuration flag without modifying code or re-deploying binaries.

---

## 5. DORA Metrics Correlation (Elite Engineering Performance)

The DevOps Research and Assessment (DORA) state of DevOps reports identify Trunk-Based Development as a **statistically significant predictor of high software delivery performance**:

| DORA Metric | Low Performers (GitFlow / Long Branches) | Elite Performers (Trunk-Based Development) |
|:---|:---|:---|
| **Deployment Frequency** | Monthly or Quarterly | **Multiple times per day (on demand)** |
| **Lead Time for Changes** | 1 to 6 months | **Less than 1 hour** |
| **Mean Time to Recover (MTTR)**| Days to Weeks | **Less than 1 hour** |
| **Change Failure Rate** | 46% – 60% | **0% – 15%** |

---

## 6. Step-by-Step Developer Workflow

```bash
# 1. Update local trunk before starting
git switch main
git pull --rebase origin main

# 2. Create micro-branch for today's small task
git switch -c feat/user-model-v2

# 3. Write code + unit tests (Keep diff < 200 lines!)
# 4. Commit atomic work
git commit -am "feat(user): add phone number column to schema behind toggle"

# 5. Rebase on trunk to ensure fast-forward compatibility
git fetch origin main
git rebase origin/main

# 6. Push to remote and open small PR
git push -u origin feat/user-model-v2
gh pr create --title "feat(user): add phone number column" --body "Small slice 1/3 of profile update."

# 7. Rapid peer review (<2 hours review turnaround) -> Merge to main!
# 8. Automated CI/CD deploys to production immediately.
```

---

## 7. Practical Examples

### Example 1: Branch by Abstraction Pattern
When refactoring a major subsystem (e.g. replacing database ORM):
```
1. Create an abstraction interface (Repository Interface).
2. Point existing code to the interface backed by the Old Implementation.
3. Commit and merge to trunk.
4. Implement the New Implementation behind a feature flag.
5. Commit and merge to trunk (tested in parallel).
6. Flip feature toggle to New Implementation.
7. Remove Old Implementation and abstraction layer.
```

### Example 2: Configuring Fast-Forward Merge Queue on Trunk
In GitHub repository settings (`Settings -> General`):
- Check: **Enable merge queue**.
- Merge queues build combined speculative batches of queued PRs, guaranteeing the trunk never breaks even when 20 PRs merge simultaneously.

---

## 8. Best Practices

1. **Keep Branch Lifespan Under 24 Hours**: If a branch cannot be merged by end-of-day, the task was scoped too broadly. Slice it into smaller incremental PRs.
2. **Review Small PRs with High Priority**: Establish a team SLA: PRs under 150 lines should be reviewed within 60 minutes.
3. **Never Allow Broken Builds on Main**: If a commit breaks trunk, the author must either revert it within 10 minutes (`git revert`) or fix forward immediately.
4. **Use Squash and Merge at the Trunk Boundary**: Keeps the trunk commit log strictly linear and easily bisectable.
5. **Retire Feature Flags Regularly**: Once a feature is at 100% rollout, delete the toggle code within the next sprint to prevent dead code accumulation.

---

## 9. Common Mistakes & Anti-Patterns

| Mistake | Consequence | Root Cause | Fix / Prevention |
|:---|:---|:---|:---|
| 3-week feature branch | Catastrophic merge conflicts; blocked release | Scoping entire epic into single PR | Slice work using Feature Flags |
| Merging without tests | Trunk breaks constantly; team halted | Lack of fast local test suite | Enforce required CI status checks |
| Permanent feature flag debt | Spaghetti codebase with 200 stale flags | Forgetting to delete flags | Add flag cleanup ticket upon launch |
| Slow PR review cycles | Developers forced to context-switch | Reviewers treating PRs as low priority | Enforce team review SLAs for small PRs |
| Direct un-reviewed pushes to trunk | Unvetted defects deployed directly | Misunderstanding TBD as "no reviews" | Use short-lived PRs with branch protection |

---

## 10. Comparison Matrix: TBD vs Feature Branching vs GitFlow

| Dimension | Trunk-Based Development | Feature Branching | GitFlow |
|:---|:---|:---|:---|
| **Branch Lifespan** | **Hours ($< 1$ day)** | Days to Weeks | Weeks to Months |
| **Number of Branches** | 1 trunk + ephemeral branches | Dozens of active branches | 5+ long-lived types (`develop`, `release/*`) |
| **Merge Frequency** | **Multiple times / day** | Weekly / Bi-weekly | Monthly / Quarterly |
| **Merge Conflict Risk**| **Near zero** | Moderate to High | **Severe ("Merge Hell")** |
| **Deployment Cadence** | Continuous (CD) | Sprint-based | Scheduled releases |
| **Feature Flags Needed?**| **Essential** | Optional | Rare |

---

## 11. Interview Questions & Answers

<details>
<summary><strong>Q1: What is Trunk-Based Development and how does it prevent merge conflicts?</strong></summary>

Trunk-Based Development is a branching model where developers merge small, frequent code increments into a single shared branch (`main`) multiple times a day.
It prevents merge conflicts because branches only live for a few hours. The delta between any developer's branch and the trunk is minimal (a few dozen lines of code). Because developers pull from trunk frequently and merge back small batches, code never diverges enough to generate complex, multi-file merge conflicts.
</details>

<details>
<summary><strong>Q2: How do teams manage incomplete or half-implemented features in Trunk-Based Development?</strong></summary>

Through **Feature Flags (Feature Toggles)** and **Dark Launching**. Instead of hiding unfinished code on an unmerged long-lived branch, developers commit the code into the trunk, but wrap the new execution paths inside dynamic conditionals (`if (flags.isEnabled('NEW_FEATURE'))`). The code is deployed safely to production servers in a dormant state. Once the feature is fully completed and verified, the flag is flipped to enable it for users without deploying new code.
</details>

<details>
<summary><strong>Q3: Does Trunk-Based Development mean developers push code directly to `main` without pull requests?</strong></summary>

**No.** While very small, high-trust pairs can practice direct-to-trunk commits, standard modern TBD uses **Short-Lived Pull Requests**. A developer branches off trunk, commits a micro-change (<200 lines), opens a PR, receives a fast peer review within 1–2 hours, passes automated CI checks, and merges into trunk within the same day.
</details>

<details>
<summary><strong>Q4: What is the "Branch by Abstraction" technique in Trunk-Based Development?</strong></summary>

Branch by Abstraction is a refactoring pattern used to replace a major component in trunk without long-lived branches:
1. Introduce an abstract interface in front of the existing component.
2. Refactor existing code to call the interface (backed by the old implementation). Merge to trunk.
3. Write the new implementation behind the interface. Merge to trunk in small PRs.
4. Switch the interface binding to the new implementation via a feature flag.
5. Verify in production, and finally delete the old component and abstraction.
</details>

<details>
<summary><strong>Q5: Why does DORA research strongly favor Trunk-Based Development over GitFlow?</strong></summary>

DORA research proves that Trunk-Based Development correlates with Elite software delivery performance (high deployment frequency, sub-hour lead time for changes, low change failure rates, and fast MTTR).
GitFlow relies on long-lived branches (`develop`, `release/*`, `feature/*`), causing batch sizes to grow large, feedback loops to slow down, and integration testing to be deferred to release freezes, which drastically increases defect rates and deployment delays.
</details>

<details>
<summary><strong>Q6: What is a GitHub Merge Queue and how does it support Trunk-Based Development?</strong></summary>

In high-velocity repositories where hundreds of developers merge PRs to trunk daily, race conditions occur when multiple PRs pass CI independently but break when merged together. A Merge Queue serializes and speculatively batches PRs, running CI on the combined integrated state before fast-forwarding the trunk. If a batch fails, the queue isolates the culprit PR, rejects it, and merges the remaining passing PRs automatically.
</details>

<details>
<summary><strong>Q7: What is the recommended pull request review SLA in Trunk-Based Development teams?</strong></summary>

In TBD, PR review requests should be treated as high priority, with target review turnaround times of **under 1 to 2 hours**. Because PRs are intentionally scoped small (<200 lines), reviews require only 5–10 minutes of reviewer time, enabling continuous flow.
</details>

<details>
<summary><strong>Q8: What should happen when the `main` branch build breaks in a Trunk-Based Development pipeline?</strong></summary>

A broken trunk is treated as a **P0 emergency ("Stop the Line")**. All further PR merges are halted. The author of the breaking commit has two immediate choices:
1. Revert the commit immediately (`git revert <sha>`) within 5–10 minutes to restore the pipeline to green.
2. If the fix is trivial (e.g. typo), push an immediate hotfix.
Unresolved broken builds are never left sitting on `main`.
</details>

---

## 12. Practice Exercises

<details>
<summary><strong>Exercise 1: Rebase Feature Branch on Latest Main</strong></summary>

**Scenario**: Fetch latest trunk commits and rebase your micro-branch.
```bash
git fetch origin main
git rebase origin/main
```
</details>

<details>
<summary><strong>Exercise 2: Implement Feature Flag Guard in Code</strong></summary>

**Scenario**: Wrap an experimental function behind a boolean toggle.
```javascript
const isFeatureEnabled = process.env.ENABLE_V2_AUTH === 'true';
if (isFeatureEnabled) {
  authV2();
} else {
  authV1();
}
```
</details>

<details>
<summary><strong>Exercise 3: Measure Line Count of Branch Diff</strong></summary>

**Scenario**: Verify your PR diff is under 200 lines before opening PR.
```bash
git diff --stat origin/main...HEAD
```
</details>

<details>
<summary><strong>Exercise 4: Fast Revert of a Broken Trunk Commit</strong></summary>

**Scenario**: Immediately revert a bad commit on main.
```bash
git revert --no-edit HEAD
git push origin main
```
</details>

<details>
<summary><strong>Exercise 5: Configure Linear History in Git Repository</strong></summary>

**Scenario**: Ensure local pulls default to rebase.
```bash
git config --global pull.rebase true
```
</details>

<details>
<summary><strong>Exercise 6: Query PR Size via GitHub CLI</strong></summary>

**Scenario**: Check additions and deletions on an open PR.
```bash
gh pr view 42 --json additions,deletions
```
</details>

---

## 13. Summary & Cheat Sheet

```bash
# Trunk-Based Development Checklist
# [x] Keep branches short-lived (< 24 hours lifespan)
# [x] Keep diffs small (< 200 lines of code)
# [x] Use Feature Flags for uncompleted features
# [x] Prioritize PR code reviews (target < 2 hr turnaround)
# [x] Rebase frequently on origin/main
# [x] If main breaks, revert immediately (Stop the line!)
```

---

## 14. Further Reading
- [Trunk-Based Development Portal](https://trunkbaseddevelopment.com/)
- [DORA DevOps Research - Trunk-Based Development](https://cloud.google.com/devops/state-of-devops)
- [Martin Fowler - Branch by Abstraction](https://martinfowler.com/bliki/BranchByAbstraction.html)
- [06-workflows-and-strategies/02-gitflow-and-github-flow.md](file:///e:/Learning/engineering-notes/04-devops/git-github/06-workflows-and-strategies/02-gitflow-and-github-flow.md)
