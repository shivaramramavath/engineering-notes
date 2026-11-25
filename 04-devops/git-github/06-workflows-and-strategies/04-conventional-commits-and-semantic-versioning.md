---
title: "Conventional Commits & Semantic Versioning (SemVer)"
category: "04-devops"
subcategory: "git-github / 06-workflows-and-strategies"
tags: ["git", "conventional-commits", "semantic-versioning", "semver", "commitlint", "changelog", "automation"]
difficulty: "Beginner-Intermediate"
estimated_read_time: "24 min"
prerequisites:
  - "01-fundamentals/04-commits"
  - "02-commands/03-add-commit"
last_updated: "2026-11-25"
---

# Conventional Commits & Semantic Versioning (SemVer)

> **Navigation**: [← Previous: 03-release-branching-and-hotfixes.md](file:///e:/Learning/engineering-notes/04-devops/git-github/06-workflows-and-strategies/03-release-branching-and-hotfixes.md) | [Module README](file:///e:/Learning/engineering-notes/04-devops/git-github/06-workflows-and-strategies/README.md) | [Next: 05-feature-flags-and-dark-launching.md →](file:///e:/Learning/engineering-notes/04-devops/git-github/06-workflows-and-strategies/05-feature-flags-and-dark-launching.md)

---

## Table of Contents
1. [What & Why](#1-what--why)
2. [The Conventional Commits Specification (v1.0.0)](#2-the-conventional-commits-specification-v100)
3. [The Semantic Versioning (SemVer 2.0.0) Mapping](#3-the-semantic-versioning-semver-200-mapping)
4. [Anatomy of a Conventional Commit](#4-anatomy-of-a-conventional-commit)
5. [Standard Types Taxonomy](#5-standard-types-taxonomy)
6. [Breaking Changes Syntax (`!` and Footers)](#6-breaking-changes-syntax--and-footers)
7. [Automated Tooling: commitlint, Husky & Commitizen](#7-automated-tooling-commitlint-husky--commitizen)
8. [Automated Changelog Generation](#8-automated-changelog-generation)
9. [Practical Examples](#9-practical-examples)
10. [Best Practices](#10-best-practices)
11. [Common Mistakes](#11-common-mistakes)
12. [Comparison Table: Messy Git Log vs Conventional Log](#12-comparison-table-messy-git-log-vs-conventional-log)
13. [Interview Questions & Answers](#13-interview-questions--answers)
14. [Practice Exercises](#14-practice-exercises)
15. [Summary & Cheat Sheet](#15-summary--cheat-sheet)
16. [Further Reading](#16-further-reading)

---

## 1. What & Why

### What are Conventional Commits?
Conventional Commits is a lightweight convention on top of commit messages that provides an easy set of rules for creating an explicit commit history. It structures commit messages into a machine-readable format (`type(scope): subject`) that maps directly to **Semantic Versioning (SemVer)**.

### Why Do We Use It?
1. **Automated Version Bumping**: Eliminates manual arguments over version numbers. CI/CD pipelines automatically calculate whether to release a `MAJOR`, `MINOR`, or `PATCH` version.
2. **Automated Changelog Generation**: Tools like `semantic-release` or `standard-version` parse commits to generate clean, categorized `CHANGELOG.md` files without manual editing.
3. **Structured Communication**: Other developers, reviewers, and automated bots instantly understand the intent and impact of a change just by reading the commit header.
4. **Enhanced Searchability**: Filtering commits by feature, bug fix, or breaking change (`git log --grep="^feat"`) becomes trivial.

---

## 2. The Conventional Commits Specification (v1.0.0)

The specification requires commit messages to follow this exact structural format:

```text
<type>[optional scope][!]: <description>

[optional body]

[optional footer(s)]
```

### Key Structural Rules:
- **Header**: Single line, maximum 50–72 characters, imperative present tense (`add`, not `added` or `adds`), lowercase, no trailing period.
- **Body**: Detailed explanation of **Why** the change was made (not how), separated from header by a blank line.
- **Footers**: Issue references (`Fixes #42`) and Breaking Change notices (`BREAKING CHANGE: <explanation>`).

---

## 3. The Semantic Versioning (SemVer 2.0.0) Mapping

Semantic Versioning follows the format: **`MAJOR.MINOR.PATCH`** (e.g. `2.4.1`). Conventional Commits correlates directly to SemVer increments:

```
┌─────────────────────────────────────────────────────────────┐
│ Commit Type                         │ SemVer Impact         │
├─────────────────────────────────────┼───────────────────────┤
│ `fix:` or `fix(scope):`             │ PATCH bump (0.0.X)    │
│ `feat:` or `feat(scope):`           │ MINOR bump (0.X.0)    │
│ `BREAKING CHANGE:` or `feat!:`      │ MAJOR bump (X.0.0)    │
│ `chore:`, `docs:`, `style:`, `test:`│ NO version bump       │
└─────────────────────────────────────┴───────────────────────┘
```

---

## 4. Anatomy of a Conventional Commit

```
┌─────────────────────────────────────────────────────────────────────────┐
│ type(scope)!: short description                                         │
│  │     │   │         │                                                  │
│  │     │   │         └── Summary in imperative mood ("add", not "added")│
│  │     │   └──────────── Optional ! indicates BREAKING CHANGE (MAJOR)   │
│  │     └──────────────── Optional noun describing section of codebase   │
│  └────────────────────── Mandatory type (feat, fix, chore, docs, etc.)  │
│                                                                         │
│ Longer body text explaining motivation and context behind the change.   │
│ Why was this necessary? What was the previous behavior?                 │
│                                                                         │
│ BREAKING CHANGE: Detailed explanation of what breaks and migration path.│
│ Fixes #128                                                              │
└─────────────────────────────────────────────────────────────────────────┘
```

---

## 5. Standard Types Taxonomy

| Type | Description | SemVer Bump |
|:---|:---|:---:|
| **`feat`** | A new feature for the user or consumer API | **MINOR** |
| **`fix`** | A bug fix for the user or consumer API | **PATCH** |
| **`docs`** | Documentation-only changes (README, guides) | None |
| **`style`** | Code formatting, missing semi-colons, whitespace (no logic changes) | None |
| **`refactor`** | Code change that neither fixes a bug nor adds a feature | None |
| **`perf`** | Code change that improves performance | **PATCH** |
| **`test`** | Adding missing tests or correcting existing tests | None |
| **`build`** | Changes affecting build system or external dependencies (webpack, npm) | None |
| **`ci`** | Changes to CI configuration files and scripts (GitHub Actions) | None |
| **`chore`** | Other changes that don't modify src or test files | None |
| **`revert`** | Reverts a previous commit | Depends |

---

## 6. Breaking Changes Syntax (`!` and Footers)

A breaking change signals that consumer code or APIs must be updated to maintain compatibility.

### Method 1: The Exclamation Mark (`!`) in Header
Place `!` immediately before the colon:
```text
feat(auth)!: replace cookie sessions with JWT bearer tokens
```

### Method 2: The `BREAKING CHANGE:` Footer
Add a dedicated footer block at the bottom of the commit:
```text
feat(api): update user response schema

BREAKING CHANGE: The `id` field is now a UUID string instead of an integer.
To migrate, update database foreign keys to UUID type.
```

---

## 7. Automated Tooling: commitlint, Husky & Commitizen

To prevent developers from accidentally pushing malformed commits, enforce validation via git hooks:

### 1. Install Commitlint & Husky
```bash
npm install -D @commitlint/config-conventional @commitlint/cli husky
npx husky init
```

### 2. Configure Commitlint (`commitlint.config.js`)
```javascript
module.exports = {
  extends: ['@commitlint/config-conventional'],
  rules: {
    'type-enum': [
      2,
      'always',
      ['feat', 'fix', 'docs', 'style', 'refactor', 'perf', 'test', 'build', 'ci', 'chore', 'revert'],
    ],
    'subject-case': [2, 'never', ['upper-case']],
  },
};
```

### 3. Add `commit-msg` Hook (`.husky/commit-msg`)
```bash
echo 'npx --no -- commitlint --edit "$1"' > .husky/commit-msg
```

Now, typing `git commit -m "fixed typo"` will instantly fail:
```text
⧗   input: fixed typo
✖   type must be one of [feat, fix, docs, ...] [type-enum]
✖   found 1 errors
```

---

## 8. Automated Changelog Generation

Using **`standard-version`** or **`semantic-release`**:
```bash
npx standard-version
```
This automated command:
1. Analyzes commit messages since the last tag.
2. Bumps version in `package.json` according to SemVer rules.
3. Automatically writes/updates `CHANGELOG.md` with categorized markdown.
4. Creates a Git commit and annotated tag `v1.3.0`.

### Sample Generated `CHANGELOG.md`:
```markdown
# 1.3.0 (2026-11-25)

### Features
* **auth:** add OAuth2 Google login ([4b2e8d1](https://github.com/my-org/app/commit/4b2e8d1))
* **billing:** support Stripe Elements checkout ([8c9d01e](https://github.com/my-org/app/commit/8c9d01e))

### Bug Fixes
* **api:** handle database connection timeout ([1a2b3c4](https://github.com/my-org/app/commit/1a2b3c4))
* **ui:** fix button alignment on mobile screens ([7f8e9d0](https://github.com/my-org/app/commit/7f8e9d0))

### BREAKING CHANGES
* **auth:** Cookie session support removed; migrate to JWT tokens.
```

---

## 9. Practical Examples

### Example 1: Standard Bug Fix Commit
```bash
git commit -m "fix(payment): prevent duplicate charges on double-click" \
           -m "Closes #142. Adds client-side idempotency key to checkout request."
```

### Example 2: Feature with Scope Commit
```bash
git commit -m "feat(profile): allow users to upload avatar image" \
           -m "Uploads images to AWS S3 and resizes to 256x256 thumbnails."
```

### Example 3: Major Breaking Change Commit
```bash
git commit -m "refactor(database)!: drop legacy PostgreSQL 11 support" \
           -m "Upgrades connection driver to PG 16. Uses scram-sha-256." \
           -m "BREAKING CHANGE: Self-hosted deployments must run PostgreSQL >= 14."
```

### Example 4: Enforcing Conventional Commits on PR Titles in GitHub Actions
```yaml
# .github/workflows/lint-pr-title.yml
name: "Lint PR Title"
on:
  pull_request:
    types: [opened, edited, synchronize]

jobs:
  main:
    runs-on: ubuntu-latest
    steps:
      - uses: amannn/action-semantic-pull-request@v5
        env:
          GITHUB_TOKEN: ${{ secrets.GITHUB_TOKEN }}
```

---

## 10. Best Practices

1. **Use Imperative Present Tense in Subject**: Write `feat: add logging`, NOT `feat: added logging` or `feat: adds logging`.
2. **Keep the Subject Under 72 Characters**: Prevents truncation in GitHub web UI and terminal `git log --oneline`.
3. **Never Put Periods at the End of the Subject**: E.g. `fix: handle null error` (no dot).
4. **Enforce at the PR Title Level for Squash Merges**: If your team uses "Squash and Merge", GitHub uses the PR title as the commit subject. Enforcing Conventional Commits on PR titles guarantees a clean trunk history!
5. **Reserve `!` Strictly for Real Breaking Changes**: Do not use `!` simply to indicate "important" changes. `!` will trigger a SemVer MAJOR release!

---

## 11. Common Mistakes

| Mistake | Consequence | Root Cause | Fix / Prevention |
|:---|:---|:---|:---|
| `feat: Added login.` | Commitlint error | Past tense and trailing period | `feat: add login` |
| Using `!` for non-breaking important changes | Accidental MAJOR version bump | Misunderstanding `!` as emphasis | Reserve `!` strictly for breaking changes |
| Multiple unrelated changes in one commit | Impossible to categorize type | Non-atomic commit | Split into atomic commits |
| Missing blank line between header and body | Body is parsed as part of subject | Formatting error | Always leave an empty newline |
| `chore: fix memory leak` | No version bump; users don't get patch | Wrong type chosen | Use `fix:` for user-facing bug fixes |

---

## 12. Comparison Table: Messy Git Log vs Conventional Log

```text
MESSY GIT LOG (Unparseable):
* 4a5e62f updates
* 8c9d01e fix typo
* 1f8e7d6 fixed bug in checkout
* 3b4c1a2 WIP
* 9e2cf31 pr comments

CONVENTIONAL COMMITS LOG (Machine-Readable & Clean):
* 4a5e62f feat(auth): add google sso login
* 8c9d01e fix(checkout): prevent duplicate payment submissions
* 1f8e7d6 docs(readme): update local setup instructions
* 3b4c1a2 perf(db): add index on user_id column
* 9e2cf31 feat(api)!: migrate endpoints from v1 to v2
```

---

## 13. Interview Questions & Answers

<details>
<summary><strong>Q1: What are the three main components of a Conventional Commit message?</strong></summary>

1. **Header**: `<type>(<optional scope>): <description>`. Single line describing what changed.
2. **Body**: Detailed paragraph(s) separated from the header by a blank line explaining **Why** the change was necessary and what changed.
3. **Footer**: One or more footers separated by a blank line, used to reference issues (e.g. `Fixes #42`) or document breaking changes (`BREAKING CHANGE: ...`).
</details>

<details>
<summary><strong>Q2: How do Conventional Commits map to Semantic Versioning (SemVer 2.0.0)?</strong></summary>

- A commit of type `fix:` maps to a SemVer **PATCH** bump (`0.0.X`), indicating a backwards-compatible bug fix.
- A commit of type `feat:` maps to a SemVer **MINOR** bump (`0.X.0`), indicating backwards-compatible new functionality.
- A commit containing `!` after type/scope or a `BREAKING CHANGE:` footer maps to a SemVer **MAJOR** bump (`X.0.0`), indicating breaking API changes.
- Types like `docs:`, `chore:`, `style:`, `test:`, `ci:` produce **NO** SemVer bump.
</details>

<details>
<summary><strong>Q3: What are the two ways to denote a breaking change in Conventional Commits?</strong></summary>

1. Adding an exclamation mark (`!`) immediately after the type or scope in the commit header:
   ```text
   feat(auth)!: remove password authentication
   ```
2. Including a footer starting with `BREAKING CHANGE:` followed by a colon, a space, and an explanation of the breaking change:
   ```text
   BREAKING CHANGE: The /v1/users endpoint is removed. Migrate to /v2/users.
   ```
</details>

<details>
<summary><strong>Q4: Why should commit subjects use the imperative present tense (e.g. "add" instead of "added" or "adds")?</strong></summary>

Imperative mood follows Git's built-in conventions (e.g. `Merge branch...`, `Revert...`). It reads as a command describing what applying the commit will do: *"If applied, this commit will [add user authentication]"*. It provides consistency across all team members and automated changelogs.
</details>

<details>
<summary><strong>Q5: How can a team enforce Conventional Commits in a team repository where developers squash-merge pull requests?</strong></summary>

When a repository uses "Squash and Merge", GitHub uses the **Pull Request title** as the commit subject on the default branch. By installing a GitHub Actions workflow (like `amannn/action-semantic-pull-request`) that runs on `pull_request` events, GitHub validates the PR title against Conventional Commit rules. If the PR title is non-compliant (e.g. "Fixing bug"), the CI check fails, blocking the merge until the author corrects the title.
</details>

<details>
<summary><strong>Q6: What is Commitlint and how does it integrate with Git hooks?</strong></summary>

Commitlint is an automated linter for commit messages. It checks the text against specified rules (like `@commitlint/config-conventional`). By integrating with **Husky** on the `commit-msg` Git hook, Commitlint intercepts `git commit` commands locally. If the message fails validation, the hook aborts the commit before it is recorded in the Git database.
</details>

<details>
<summary><strong>Q7: Can a commit have both a scope and an exclamation mark? Give an example.</strong></summary>

Yes:
```text
feat(database)!: migrate primary keys to UUID
```
The scope is `database` and the `!` signals a breaking change.
</details>

<details>
<summary><strong>Q8: What is the difference between `chore:`, `build:`, and `ci:` types?</strong></summary>

- `build:` Changes that affect the build system or external dependencies (e.g. webpack config, npm package version upgrades, Makefile).
- `ci:` Changes to continuous integration configuration files and scripts (e.g. GitHub Actions workflows in `.github/workflows/`).
- `chore:` Miscellaneous maintenance tasks that do not modify src or test files (e.g. updating `.gitignore`, license text, or internal release scripts).
</details>

---

## 14. Practice Exercises

<details>
<summary><strong>Exercise 1: Write a Valid Feature Commit</strong></summary>

**Scenario**: Commit a new shopping cart feature.
```bash
git commit -m "feat(cart): implement coupon code discount calculation"
```
</details>

<details>
<summary><strong>Exercise 2: Write a Breaking Change Commit with Footer</strong></summary>

**Scenario**: Document an API breaking change.
```bash
git commit -m "feat(api)!: migrate user endpoint to v2" \
           -m "BREAKING CHANGE: User payload now nests addresses under address array."
```
</details>

<details>
<summary><strong>Exercise 3: Test Commitlint Locally with npx</strong></summary>

**Scenario**: Test if a string passes Conventional Commits.
```bash
echo "feat(auth): add login endpoint" | npx commitlint
# Exits with 0 (Valid!)

echo "fixed bug" | npx commitlint
# Fails with errors!
```
</details>

<details>
<summary><strong>Exercise 4: Query Git Log for Features Only</strong></summary>

**Scenario**: Search commit history for all `feat` commits.
```bash
git log --oneline --grep="^feat"
```
</details>

<details>
<summary><strong>Exercise 5: Query Git Log for Breaking Changes</strong></summary>

**Scenario**: Search history for all breaking change commits.
```bash
git log --grep="BREAKING CHANGE" --grep="^[a-z]*\(!\):"
```
</details>

<details>
<summary><strong>Exercise 6: Run Standard-Version Dry Run</strong></summary>

**Scenario**: Preview what version bump and changelog standard-version would produce.
```bash
npx standard-version --dry-run
```
</details>

---

## 15. Summary & Cheat Sheet

```text
Conventional Commit Template:
<type>(<scope>): <subject>

[optional body]

[optional footer(s)]

Type Reference:
feat:     New feature (MINOR bump)
fix:      Bug fix (PATCH bump)
feat!:    Breaking change (MAJOR bump)
docs:     Documentation only
refactor: Code refactoring without behavior change
perf:     Performance improvement
test:     Unit/Integration tests
ci:       GitHub Actions / CI pipelines
chore:    Routine maintenance
```

---

## 16. Further Reading
- [Conventional Commits Specification v1.0.0](https://www.conventionalcommits.org/en/v1.0.0/)
- [Semantic Versioning 2.0.0](https://semver.org/)
- [Commitlint Documentation](https://commitlint.js.org/)
- [06-workflows-and-strategies/05-feature-flags-and-dark-launching.md](file:///e:/Learning/engineering-notes/04-devops/git-github/06-workflows-and-strategies/05-feature-flags-and-dark-launching.md)
