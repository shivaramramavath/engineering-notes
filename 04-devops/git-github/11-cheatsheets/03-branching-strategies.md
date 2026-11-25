# Quick Reference Cheatsheet: Branching & Delivery Models

[<- Back to Cheatsheets Overview](./README.md) | [Previous Cheatsheet: Undo & Recovery Matrix <-](./02-undo-and-recovery-matrix.md) | [Next Cheatsheet: GitHub Actions Syntax ->](./04-github-actions-syntax.md)

---

## 1. Branching Strategy Comparison Matrix

| Model | Branch Lifetime | Release Cadence | Complexity | Best For | Primary Risk |
|:---|:---|:---|:---|:---|:---|
| **Trunk-Based Development** | < 24 Hours | Continuous (Multiple/Day) | Very Low (High CI maturity)| SaaS Web Services, Cloud-Native | Releasing unfinished code without feature flags |
| **GitHub Flow** | 1 – 3 Days | Continuous / Daily | Very Low | Startups, Small Teams, Web Apps | Lack of multi-version release maintenance |
| **Release Branching** | 1 – 2 Weeks | Bi-weekly / Monthly | Moderate | Mobile Apps, Enterprise SaaS, Libraries | Backporting cherry-pick debt |
| **GitFlow** | Weeks to Months | Milestones / Scheduled | Very High | Boxed Software, Multi-version OS | Merge hell, stale branches, ceremony overhead |
| **GitLab Flow** | Environment-linked | Per-environment | Moderate | Multi-tier deployment environments | Environment branch drift anti-pattern |

---

## 2. Trunk-Based Development (TBD)

```
[main / trunk] --------------------------------------------- (Deploy to Prod)
      \               ^         \             ^
       \-- [feat-A] -/ (<24h)    \-- [feat-B] / (<24h)
```

- **Core Rule**: Feature branches live less than 24 hours. Developers merge small, incremental batches into `main`.
- **Enabler**: **Feature Flags / Toggles**. Unfinished features are merged into production disabled behind flags.
- **Merge Strategy**: Fast-forward with linear history or Squash Merge.

---

## 3. GitHub Flow

```
[main] ---------------------------------------------------- (Deploy to Prod)
   \                                                 ^
    \-- [feature-branch] --- (PR + Review + CI) ----/
```

- **Core Rule**: Anything in `main` is always deployable.
- **Workflow**:
  1. Branch off `main`.
  2. Commit and push changes.
  3. Open Pull Request; discuss, review, and pass CI checks.
  4. Merge into `main` and deploy immediately.

---

## 4. Release Branching (Mobile & Enterprise)

```
[main] --------------------------------------------------------------------
           \                                       \
            \-- [release/v1.2]                      \-- [release/v1.3]
                     \                                      \
                      +-- Hotfix (Cherry-picked to main)     +-- Deploy v1.3.0
```

- **Core Rule**: `main` contains ongoing development. When code freeze arrives, a dedicated `release/vX.Y` branch is cut.
- **Hardening**: Only bugfixes land on the release branch during QA stabilization.
- **Hotfixes**: Fixed on `main` first, then cherry-picked (`git cherry-pick -x`) to the release branch.

---

## 5. GitFlow (Legacy Multi-Branch Architecture)

```
[main] -------------------------- (v1.0.0 Tag) ----------------------------
                                       ^
[hotfix] -----------------------------/ \----------------------------------
                                         \
[release] ---------------------- (Cut) ---\---- (Merge back) --------------
                                   ^
[develop] ------------------------/ \--------------------------------------
              \                  /   \                  /
[feature]      +-- [feature-1] -+     +-- [feature-2] -+
```

- **Branches**: Two permanent branches (`main` for releases, `develop` for integration), plus temporary `feature/*`, `release/*`, and `hotfix/*` branches.
- **Drawback**: High cognitive overhead and frequent merge conflicts between `develop` and `main`. Recommended only when managing multiple simultaneous supported versions of installed desktop software.

---

[<- Back to Cheatsheets Overview](./README.md) | [Previous Cheatsheet: Undo & Recovery Matrix <-](./02-undo-and-recovery-matrix.md) | [Next Cheatsheet: GitHub Actions Syntax ->](./04-github-actions-syntax.md)
