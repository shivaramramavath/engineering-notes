<!--
================================================================================
Metadata:
  Title: Reference Project 07 - Enterprise Reusable Workflow Library & Composite Actions
  Track: Git & GitHub / DevOps
  Difficulty: Advanced
  Estimated Time: 50 minutes
  Prerequisites: 04-github-actions/12-reusable-workflows.md, 04-github-actions/25-composite-vs-javascript-vs-docker-actions.md
  Key Terminology: Reusable Workflows, workflow_call, Composite Actions, Shared Secrets, inputs, secrets: inherit, Enterprise Governance
================================================================================
-->

# Reference Project 07: Enterprise Reusable Workflow Library & Composite Actions

[<- Back to Projects Overview](./README.md) | [Next Project: 08-secure-deploy-with-oidc-and-approvals.md ->](./08-secure-deploy-with-oidc-and-approvals.md)

---

## 1. Overview & Architecture

In large engineering organizations maintaining hundreds of microservices, duplicating CI/CD YAML configurations across every repository leads to configuration drift, unpatched vulnerabilities, and high maintenance costs. When security mandates a new scanner or Docker baseline, platform engineers cannot manually update hundreds of separate `.github/workflows/` files.

The industry-standard solution is an **Enterprise Reusable Workflow & Action Library**.
- Centralized repository (e.g., `my-org/shared-workflows`) houses canonical workflow definitions (`.github/workflows/*.yml`) triggered via `workflow_call`.
- Custom **Composite Actions** encapsulate multi-step shell patterns (e.g., cache setup, tool installations, notification scripts).
- Service repositories act as **Caller Workflows**, invoking the centralized templates in 5 to 10 lines of declarative YAML with strict input validation.

### Centralized Workflow Architecture

```
Centralized Library Repository: [my-org/shared-workflows]
├── .github/workflows/
│   ├── standard-node-ci.yml    (Reusable Workflow with workflow_call)
│   └── container-deploy.yml    (Reusable Workflow with workflow_call)
└── actions/
    └── setup-enterprise-tools/ (Composite Action with action.yml)
          |
          | Referenced by version tag (e.g., @v1 or @v1.2.0)
          v
Caller Service Repositories:
├── [my-org/billing-service]   -> uses: my-org/shared-workflows/.github/workflows/standard-node-ci.yml@v1
├── [my-org/auth-service]      -> uses: my-org/shared-workflows/.github/workflows/standard-node-ci.yml@v1
└── [my-org/catalog-service]   -> uses: my-org/shared-workflows/.github/workflows/standard-node-ci.yml@v1
```

---

## 2. Prerequisites & Setup

### Environment Requirements
- **Git**: Version 2.25+
- **GitHub Permissions**: Read access to internal/organization repositories (`actions: read`)

### Repository Hierarchy for the Shared Library
```text
shared-workflows/
├── .github/
│   └── workflows/
│       └── standard-node-ci.yml    # Reusable workflow
├── actions/
│   └── setup-vault-agent/          # Custom composite action
│       └── action.yml
└── README.md
```

---

## 3. Step-by-Step Implementation Walkthrough

### Step 1: Author Custom Composite Action (`actions/setup-vault-agent/action.yml`)
Encapsulate multi-step shell setup into a single reusable unit:

```yaml
mkdir -p ~/git-labs/shared-workflows/actions/setup-vault-agent
cd ~/git-labs/shared-workflows

cat << 'EOF' > actions/setup-vault-agent/action.yml
name: "Setup Enterprise Vault Agent"
description: "Configures local environment variables and validates corporate token"

inputs:
  environment:
    description: "Deployment environment target"
    required: true
    default: "staging"

runs:
  using: "composite"
  steps:
    - name: "Log Environment Initialization"
      shell: bash
      run: |
        echo "Initializing Enterprise Security Context for: ${{ inputs.environment }}"
        echo "SECURITY_TIER=ENCLAVE_RESTRICTED" >> $GITHUB_ENV
        echo "::notice title=Security::Enterprise Vault Agent successfully initialized."
EOF
```

### Step 2: Author Reusable Workflow (`.github/workflows/standard-node-ci.yml`)
Define the parameterized `workflow_call` template:

```yaml
mkdir -p .github/workflows
cat << 'EOF' > .github/workflows/standard-node-ci.yml
name: "Standard Node.js CI Template"

on:
  workflow_call:
    inputs:
      node-version:
        description: "Node.js runtime version"
        required: false
        type: string
        default: "20"
      run-coverage:
        description: "Whether to enforce code coverage threshold"
        required: false
        type: boolean
        default: true
    secrets:
      SONAR_TOKEN:
        description: "Optional SonarQube token"
        required: false

permissions:
  contents: read

jobs:
  standard-ci:
    name: "Enterprise Node CI"
    runs-on: ubuntu-latest
    steps:
      - name: "Checkout Caller Code"
        uses: actions/checkout@v4

      - name: "Invoke Central Composite Action"
        uses: my-org/shared-workflows/actions/setup-vault-agent@v1
        with:
          environment: "ci"

      - name: "Setup Node.js Runtime"
        uses: actions/setup-node@v4
        with:
          node-version: ${{ inputs.node-version }}
          cache: "npm"

      - name: "Install Dependencies"
        run: npm ci

      - name: "Execute Linter"
        run: npm run lint --if-present

      - name: "Execute Tests"
        run: |
          if [ "${{ inputs.run-coverage }}" = "true" ]; then
            npm test -- --coverage
          else
            npm test
          fi
EOF
```

### Step 3: Implement Caller Workflow in Service Repository
In consumer repository `billing-service`, author `.github/workflows/ci.yml`:

```yaml
# In billing-service repo: .github/workflows/ci.yml
name: "Billing Service Pipeline"

on:
  push:
    branches: ["main"]
  pull_request:
    branches: ["main"]

jobs:
  call-enterprise-ci:
    name: "Run Reusable CI"
    uses: my-org/shared-workflows/.github/workflows/standard-node-ci.yml@v1
    with:
      node-version: "20"
      run-coverage: true
    secrets:
      SONAR_TOKEN: ${{ secrets.SONAR_TOKEN }}
```
Notice how `billing-service` gets full enterprise compliance, security context, and testing in **only 18 lines of code**!

---

## 4. Verification & Testing

### Step 1: Version Tagging in Shared Library
Always tag releases using semantic versions:
```bash
git add .
git commit -m "feat(templates): release standard node ci v1.0.0"
git tag v1.0.0
git tag v1 -f # Moving major tag
git push origin main --tags
```

### Step 2: Caller Run Inspection
Trigger the caller workflow in `billing-service`:
```bash
gh run list --workflow=ci.yml
gh run view --verbose
```
Output verifies execution:
- Job `call-enterprise-ci / standard-ci` executes.
- Composite action `Setup Enterprise Vault Agent` runs and emits notice annotation.
- Node.js setup, caching, and tests execute with 100% compliance.

---

## 5. Production Readiness & Troubleshooting

### Security & Governance Best Practices
- **Pin to Semantic Version Tags or SHAs**: Never call `@main` in caller workflows (`uses: ...@main`). Use major version tags (`@v1`) or immutable SHAs (`@a1b2c3d`) to prevent unannounced breaking changes.
- **Access Control on Shared Repo**: In Organization Settings -> Actions -> General, enable **"Access" -> "Accessible from repositories in the 'my-org' organization"**.
- **Passing Secrets**: Use `secrets: inherit` only when all caller secrets are trusted. Prefer explicitly declared secrets (`secrets: SONAR_TOKEN: ${{ secrets.SONAR_TOKEN }}`) for tightest least-privilege scoping.

---

[<- Back to Projects Overview](./README.md) | [Next Project: 08-secure-deploy-with-oidc-and-approvals.md ->](./08-secure-deploy-with-oidc-and-approvals.md)
