<!--
================================================================================
Metadata:
  Title: Reference Project 05 - Automated Release Pipeline with Conventional Commits
  Track: Git & GitHub / DevOps
  Difficulty: Advanced
  Estimated Time: 50 minutes
  Prerequisites: 06-workflows-and-strategies/04-conventional-commits-and-semantic-versioning.md, 04-github-actions/19-release-automation.md
  Key Terminology: Conventional Commits, Commitlint, SemVer, Release Please, Release PR, CHANGELOG.md, Automated Tagging
================================================================================
-->

# Reference Project 05: Automated Release Pipeline with Conventional Commits

[<- Back to Projects Overview](./README.md) | [Next Project: 06-monorepo-ci-with-path-filters.md ->](./06-monorepo-ci-with-path-filters.md)

---

## 1. Overview & Architecture

Manual release management—deciding whether a change is a patch, minor, or major version, editing changelog markdown files by hand, creating Git tags, and authoring GitHub Releases—is tedious, error-prone, and causes version drift across engineering teams.

This reference project automates the entire software release lifecycle using **Conventional Commits** and **Google's `release-please`**. The system operates on a continuous release PR cycle:
1. Developers commit changes adhering strictly to the Conventional Commits format (`feat:`, `fix:`, `chore:`, `BREAKING CHANGE:`). Commitlint verifies format compliance on PRs.
2. When commits merge into `main`, the `release-please` GitHub Action analyzes the commit history since the last release tag.
3. It automatically calculates the next Semantic Version (SemVer), maintains a curated `CHANGELOG.md`, and creates/updates a dedicated **Release PR**.
4. When maintainers merge the Release PR, the action tags the repository, publishes an official GitHub Release with release notes, and triggers downstream deployment steps.

### Release Automation Architecture Diagram

```
Developer Workstation
+---------------------------------------------------+
| git commit -m "feat(auth): add OAuth2 refresh"    |
| (Checked locally by commitlint hook)              |
+---------------------------------------------------+
                          |
                          | git push & PR Merge to main
                          v
GitHub Actions Workflow (release.yml)
+---------------------------------------------------+
| 1. release-please-action analyzes commit history   |
|    - Evaluates commit types: feat -> BUMP MINOR   |
|    - Updates CHANGELOG.md                         |
|    - Updates version in package.json (1.2.0)      |
+---------------------------------------------------+
                          |
                          v
+---------------------------------------------------+
| 2. Automated Pull Request: "chore: release 1.2.0" |
|    - Displays generated changelog & diff          |
+---------------------------------------------------+
                          |
                          | Maintainer Reviews & Merges Release PR
                          v
+---------------------------------------------------+
| 3. Release Execution Phase                        |
|    - Automatically creates Git Tag 'v1.2.0'       |
|    - Publishes GitHub Release with assets         |
|    - Broadcasts release event to downstream CD     |
+---------------------------------------------------+
```

---

## 2. Prerequisites & Setup

### Environment Requirements
- **Git**: Version 2.25+
- **Node.js**: Version 18.x or 20.x
- **GitHub Permissions**: `contents: write`, `pull-requests: write`

### Project Layout
```text
release-automation-project/
├── .github/
│   └── workflows/
│       ├── commitlint.yml     # Validates PR commit titles
│       └── release.yml        # Release Please automation
├── commitlint.config.js       # Conventional commit rules
├── package.json
└── src/
    └── index.js
```

---

## 3. Step-by-Step Implementation Walkthrough

### Step 1: Initialize Repository & Install Commitlint
```bash
mkdir -p ~/git-labs/release-automation-project && cd ~/git-labs/release-automation-project
git init
```

Create `package.json`:
```json
cat << 'EOF' > package.json
{
  "name": "release-automation-demo",
  "version": "1.0.0",
  "description": "Automated SemVer releases using Conventional Commits",
  "main": "src/index.js",
  "scripts": {
    "test": "echo \"Tests passed\""
  },
  "devDependencies": {
    "@commitlint/cli": "^19.0.0",
    "@commitlint/config-conventional": "^19.0.0"
  }
}
EOF
```

Create `commitlint.config.js`:
```javascript
cat << 'EOF' > commitlint.config.js
module.exports = {
  extends: ['@commitlint/config-conventional'],
  rules: {
    'type-enum': [
      2,
      'always',
      ['feat', 'fix', 'docs', 'style', 'refactor', 'perf', 'test', 'build', 'ci', 'chore', 'revert']
    ],
    'subject-case': [2, 'always', 'lower-case']
  }
};
EOF
```

### Step 2: Configure PR Commitlint Workflow (`.github/workflows/commitlint.yml`)
Block any PR whose commits violate Conventional Commits:
```yaml
mkdir -p .github/workflows
cat << 'EOF' > .github/workflows/commitlint.yml
name: "Lint Commit Messages"

on:
  pull_request:
    types: [opened, synchronize, reopened, edited]

jobs:
  commitlint:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v4
        with:
          fetch-depth: 0

      - uses: wagoid/commitlint-github-action@v5
        with:
          configFile: "commitlint.config.js"
EOF
```

### Step 3: Configure Release Please Workflow (`.github/workflows/release.yml`)
```yaml
cat << 'EOF' > .github/workflows/release.yml
name: "Release Automation"

on:
  push:
    branches: ["main"]

permissions:
  contents: write
  pull-requests: write

jobs:
  release-please:
    runs-on: ubuntu-latest
    steps:
      - uses: googleapis/release-please-action@v4
        id: release
        with:
          release-type: node
          package-name: release-automation-demo

      # Next steps only run when a Release PR is merged and an official release is created
      - name: "Log Release Created"
        if: ${{ steps.release.outputs.release_created }}
        run: |
          echo "Released Version: ${{ steps.release.outputs.tag_name }}"
          echo "Release URL: ${{ steps.release.outputs.html_url }}"
          echo "Upload URL: ${{ steps.release.outputs.upload_url }}"
EOF
```

---

## 4. Verification & Testing

### Step 1: Verify Commitlint Locally
Test commit message validation:
```bash
# Bad commit message (fails)
echo "bad message" | npx commitlint
# Output: ✖   type must be one of [feat, fix, ...]

# Good commit message (passes)
echo "feat(api): implement user password reset endpoint" | npx commitlint
# Output: (zero errors)
```

### Step 2: Push Baseline to GitHub
```bash
git add .
git commit -m "chore: initial release automation setup"
git push -u origin main
```

### Step 3: Simulate Feature PR and Release PR Generation
Make a feature commit adhering to Conventional Commits:
```bash
git checkout -b feature-metrics
echo "console.log('Metrics active');" >> src/index.js
git commit -am "feat(metrics): add prometheus counter metrics"
git push -u origin feature-metrics
```
Merge this PR into `main`. The `release-please` action triggers automatically and opens a **Release PR**:
- Title: `chore(main): release 1.1.0`
- Changes: Bumps `package.json` from `1.0.0` to `1.1.0` and appends `## [1.1.0]` into `CHANGELOG.md` with links to the feature commit.

Merge the Release PR. `release-please` automatically tags `v1.1.0` and creates the official GitHub Release!

---

## 5. Production Readiness & Troubleshooting

### SemVer Release Calculation Matrix

| Commit Prefix | SemVer Bump | Example Commit | Resulting Release (from 1.0.0) |
|:---|:---|:---|:---|
| `fix:` | PATCH | `fix(auth): resolve session null pointer` | **1.0.1** |
| `feat:` | MINOR | `feat(billing): add paypal subscription` | **1.1.0** |
| `feat!:`, `BREAKING CHANGE:` | MAJOR | `feat(api)!: drop deprecated v1 endpoints` | **2.0.0** |
| `chore:`, `docs:`, `ci:` | NONE | `docs: update readme with quickstart` | No release PR generated |

### Troubleshooting Table

| Issue | Cause | Fix |
|:---|:---|:---|
| Release PR not opening | No `feat` or `fix` commits since last tag | Ensure commit messages start with lowercase `feat:` or `fix:` |
| `GITHUB_TOKEN` permission errors | Actions token missing write permission | Declare `permissions: contents: write` and `pull-requests: write` |
| Monorepo release conflicts | Multiple packages sharing one release config | Configure `release-please-config.json` with multi-package support |

---

[<- Back to Projects Overview](./README.md) | [Next Project: 06-monorepo-ci-with-path-filters.md ->](./06-monorepo-ci-with-path-filters.md)
