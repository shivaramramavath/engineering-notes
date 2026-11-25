<!--
================================================================================
Metadata:
  Title: Reference Project 06 - Selective Monorepo CI with Path Filtering
  Track: Git & GitHub / DevOps
  Difficulty: Advanced
  Estimated Time: 55 minutes
  Prerequisites: 04-github-actions/24-monorepo-and-path-filters.md, 06-workflows-and-strategies/06-monorepo-vs-polyrepo-git-strategies.md
  Key Terminology: Monorepo, Selective CI, Path Filters, dorny/paths-filter, Turborepo, Dependency Graph, Affected Projects
================================================================================
-->

# Reference Project 06: Selective Monorepo CI with Path Filtering

[<- Back to Projects Overview](./README.md) | [Next Project: 07-reusable-workflow-library.md ->](./07-reusable-workflow-library.md)

---

## 1. Overview & Architecture

Monorepos simplify code sharing, atomic cross-service refactoring, and dependency management. However, as a monorepo grows to dozens of applications and shared libraries, executing a full CI build and test suite on every single commit causes severe build queue congestion and drains GitHub Actions billing minutes.

The solution is **Selective CI Pipeline Execution**:
- If a commit modifies only `services/billing/**`, execute tests and builds **only** for the Billing Service.
- If a commit modifies `packages/core-logger/**`, intelligently trigger tests for the logger **and all downstream consumer services**.
- If a commit modifies only root documentation (`docs/**` or `README.md`), skip all compute-heavy builds entirely.

This project implements an enterprise-grade monorepo pipeline using **`dorny/paths-filter`** and modular job conditions.

### Monorepo Selective Dependency Graph

```
                        [packages/common-auth]
                                |
                +---------------+---------------+
                |                               |
                v                               v
    [services/user-service]         [services/payment-service]
                |                               |
                +---------------+---------------+
                                |
                                v
               [CI Pipeline Selective Matrix]
  - If common-auth changes: RUN ALL TESTS (user + payment + auth)
  - If user-service changes: RUN ONLY user-service
  - If docs/ changes: SKIP ALL JOBS
```

---

## 2. Prerequisites & Setup

### Environment Requirements
- **Git**: Version 2.25+
- **Node.js**: Version 20.x

### Monorepo Directory Hierarchy
```text
monorepo-ci-project/
├── .github/
│   └── workflows/
│       └── monorepo-ci.yml    # Selective orchestrator workflow
├── packages/
│   └── common-auth/           # Shared authentication library
│       ├── package.json
│       └── index.js
├── services/
│   ├── user-service/          # Microservice A
│   │   ├── package.json
│   │   └── server.js
│   └── payment-service/       # Microservice B
│       ├── package.json
│       └── server.js
└── package.json
```

---

## 3. Step-by-Step Implementation Walkthrough

### Step 1: Initialize Monorepo Directory & Packages
```bash
mkdir -p ~/git-labs/monorepo-ci-project && cd ~/git-labs/monorepo-ci-project
git init
mkdir -p packages/common-auth services/user-service services/payment-service
```

Root `package.json`:
```json
cat << 'EOF' > package.json
{
  "name": "enterprise-monorepo",
  "private": true,
  "workspaces": [
    "packages/*",
    "services/*"
  ]
}
EOF
```

### Step 2: Populate Shared Library & Services

Shared Library (`packages/common-auth`):
```json
cat << 'EOF' > packages/common-auth/package.json
{
  "name": "@monorepo/common-auth",
  "version": "1.0.0",
  "main": "index.js",
  "scripts": {
    "test": "echo \"Testing common-auth... OK\""
  }
}
EOF

cat << 'EOF' > packages/common-auth/index.js
module.exports = {
  verifyToken: (token) => token === "valid-secret-token"
};
EOF
```

User Service (`services/user-service`):
```json
cat << 'EOF' > services/user-service/package.json
{
  "name": "@monorepo/user-service",
  "version": "1.0.0",
  "scripts": {
    "test": "echo \"Testing user-service... OK\"",
    "build": "echo \"Building user-service... OK\""
  }
}
EOF
```

Payment Service (`services/payment-service`):
```json
cat << 'EOF' > services/payment-service/package.json
{
  "name": "@monorepo/payment-service",
  "version": "1.0.0",
  "scripts": {
    "test": "echo \"Testing payment-service... OK\"",
    "build": "echo \"Building payment-service... OK\""
  }
}
EOF
```

### Step 3: Author Selective CI Workflow (`.github/workflows/monorepo-ci.yml`)

```yaml
mkdir -p .github/workflows
cat << 'EOF' > .github/workflows/monorepo-ci.yml
name: "Selective Monorepo CI"

on:
  push:
    branches: ["main"]
  pull_request:
    branches: ["main"]

permissions:
  contents: read

jobs:
  # -------------------------------------------------------------
  # Job 1: Path Filter Evaluation Engine
  # Inspects git diff and outputs boolean flags for each module
  # -------------------------------------------------------------
  detect-changes:
    name: "Analyze Changed Paths"
    runs-on: ubuntu-latest
    outputs:
      common-auth: ${{ steps.filter.outputs.common-auth }}
      user-service: ${{ steps.filter.outputs.user-service }}
      payment-service: ${{ steps.filter.outputs.payment-service }}
    steps:
      - uses: actions/checkout@v4

      - uses: dorny/paths-filter@v3
        id: filter
        with:
          filters: |
            common-auth:
              - 'packages/common-auth/**'
            user-service:
              - 'services/user-service/**'
              - 'packages/common-auth/**'
            payment-service:
              - 'services/payment-service/**'
              - 'packages/common-auth/**'

  # -------------------------------------------------------------
  # Job 2: Common Auth Shared Package CI
  # -------------------------------------------------------------
  test-common-auth:
    name: "Test @monorepo/common-auth"
    needs: [detect-changes]
    if: ${{ needs.detect-changes.outputs.common-auth == 'true' }}
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v4
      - uses: actions/setup-node@v4
        with:
          node-version: 20
      - name: Run Tests
        working-directory: packages/common-auth
        run: npm test

  # -------------------------------------------------------------
  # Job 3: User Service CI
  # Runs if user-service changed OR if common-auth dependency changed
  # -------------------------------------------------------------
  ci-user-service:
    name: "CI @monorepo/user-service"
    needs: [detect-changes]
    if: ${{ needs.detect-changes.outputs.user-service == 'true' }}
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v4
      - uses: actions/setup-node@v4
        with:
          node-version: 20
      - name: Run Tests
        working-directory: services/user-service
        run: npm test
      - name: Run Build
        working-directory: services/user-service
        run: npm run build

  # -------------------------------------------------------------
  # Job 4: Payment Service CI
  # Runs if payment-service changed OR if common-auth dependency changed
  # -------------------------------------------------------------
  ci-payment-service:
    name: "CI @monorepo/payment-service"
    needs: [detect-changes]
    if: ${{ needs.detect-changes.outputs.payment-service == 'true' }}
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v4
      - uses: actions/setup-node@v4
        with:
          node-version: 20
      - name: Run Tests
        working-directory: services/payment-service
        run: npm test
      - name: Run Build
        working-directory: services/payment-service
        run: npm run build

  # -------------------------------------------------------------
  # Job 5: Monorepo Required Status Check Gate
  # Satisfies GitHub branch protection rules even if jobs are skipped
  # -------------------------------------------------------------
  ci-gateway:
    name: "Monorepo Quality Gate"
    needs: [detect-changes, test-common-auth, ci-user-service, ci-payment-service]
    if: always()
    runs-on: ubuntu-latest
    steps:
      - name: Verify All Active Jobs Succeeded
        run: |
          # Ensure no dependent job failed or was cancelled
          if [[ "${{ needs.test-common-auth.result }}" =~ ^(failure|cancelled)$ ]] || \
             [[ "${{ needs.ci-user-service.result }}" =~ ^(failure|cancelled)$ ]] || \
             [[ "${{ needs.ci-payment-service.result }}" =~ ^(failure|cancelled)$ ]]; then
            echo "One or more monorepo CI jobs failed!"
            exit 1
          fi
          echo "All active monorepo verification jobs passed successfully."
EOF
```

---

## 4. Verification & Testing

### Scenario A: Modify Only User Service
```bash
git add . && git commit -m "chore: initial monorepo structure"
git checkout -b feature-user-profile
echo "// profile edit" >> services/user-service/server.js
git commit -am "feat(user): add profile endpoint"
git push -u origin feature-user-profile
```
**Observation**:
- `detect-changes` outputs: `user-service = true`, `payment-service = false`, `common-auth = false`.
- `ci-user-service` runs and passes.
- `ci-payment-service` and `test-common-auth` are **skipped**.
- Build completes in 25 seconds instead of 90 seconds.

### Scenario B: Modify Shared `common-auth` Library
```bash
git checkout -b fix-auth-secret
echo "// secret update" >> packages/common-auth/index.js
git commit -am "fix(auth): update token verification algorithm"
git push -u origin fix-auth-secret
```
**Observation**:
- `detect-changes` outputs: `common-auth = true`, `user-service = true`, `payment-service = true`.
- All three test jobs run in parallel, guaranteeing no downstream services break!

---

## 5. Production Readiness & Troubleshooting

### Handling Branch Protection with Skipped Jobs
A common pitfall with selective CI is GitHub branch protection: if `ci-payment-service` is marked as a required check, and the job is skipped because payment files did not change, the PR remains blocked forever.
**Solution**: Always use an **aggregating gateway job** (`ci-gateway`) with `if: always()` as the single required check in repository rulesets. The gateway verifies that any job that actually ran succeeded.

---

[<- Back to Projects Overview](./README.md) | [Next Project: 07-reusable-workflow-library.md ->](./07-reusable-workflow-library.md)
