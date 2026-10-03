# CI/CD

Automating everything between `git push` and a running production service: every change is linted, tested, scanned, built into an image, and deployed through staging to production, with a way back when something goes wrong.

## What CI/CD means

| Term | Meaning | Answers |
|---|---|---|
| **Continuous Integration (CI)** | Every push or pull request is automatically built and tested | "Does this change break anything?" |
| **Continuous Delivery** | Every change that passes CI is automatically prepared as a *releasable* artifact, with deployment one approval away | "Could we ship this right now?" |
| **Continuous Deployment** | Every change that passes all checks is deployed to production **automatically**, with no human gate | "Does shipping happen on every merge?" |

The purpose is to make releasing **boring**: small, frequent, automated, reversible. Large infrequent releases are risky because many changes ship at once and nobody remembers what changed. Small releases are easy to test, easy to understand, and easy to roll back.

### The cost of manual deploys

```
Manual:   SSH to the server → git pull → npm install → pm2 restart → "did it work?"
          • different on every machine and for every person
          • no record of who deployed what, when
          • skipped tests "just this once"
          • rollback = frantic guessing
Automated: merge → pipeline → same steps every time, logged, tested, reversible
```

---

## The pipeline, stage by stage

```
┌────────── on every pull request ──────────┐   ┌──────── on merge to main ─────────┐
│                                            │   │                                   │
│  install → lint → typecheck → unit tests   │   │  build image → scan → push to ECR │
│  → integration/API tests (real DB)         │   │       │                           │
│  → security checks (audit, secrets, SAST)  │   │       ▼                           │
│                                            │   │  deploy to STAGING → migrate →    │
│  ✅ all green required to merge             │   │  smoke tests                      │
│                                            │   │       │                           │
└────────────────────────────────────────────┘   │       ▼  (approval, or automatic) │
                                                 │  deploy to PRODUCTION → migrate → │
                                                 │  verify → watch → (auto-rollback) │
                                                 └───────────────────────────────────┘
```

| Stage | What | Fails the build when |
|---|---|---|
| **Install** | `npm ci` (exact lockfile install, cached) | Lockfile and `package.json` disagree |
| **Lint & format** | ESLint, Prettier | Style or likely-bug rules violated |
| **Type check** | `tsc --noEmit` (`17-typescript/`) | Type errors |
| **Unit tests** | Fast, isolated (`13-testing/01-unit-and-integration-testing.md`) | Any failure |
| **Integration / API tests** | Against real Postgres/Redis service containers (`13-testing/03-test-database-and-coverage.md`) | Any failure |
| **Coverage** | Report and threshold (`13-testing/03-test-database-and-coverage.md`) | Below the floor |
| **Security** | `npm audit`, secret scanning, CodeQL/SAST, container scan | High/critical findings |
| **Build** | `docker build` of the production image (`03-docker-and-compose.md`) | Build fails |
| **Publish** | Push to the registry, tagged with the commit SHA | Registry/auth failure |
| **Deploy** | Roll out the image to an environment | Rollout health checks fail |
| **Verify** | Smoke tests, health checks, error-rate watch | Unhealthy after deploy |

**Order stages from fastest/cheapest to slowest/most expensive**, so developers get failure feedback in a minute (lint) rather than after ten (full suite). Run independent stages **in parallel**.

---

## GitHub Actions: a complete CI workflow

GitHub Actions workflows live in `.github/workflows/*.yml`. A workflow has **jobs** (run on separate machines, in parallel by default), each made of **steps**.

```yaml
# .github/workflows/ci.yml
name: CI

on:
  pull_request:
  push:
    branches: [main]

concurrency:                                   # cancel superseded runs of the same branch/PR
  group: ci-${{ github.ref }}
  cancel-in-progress: true

permissions:                                   # least privilege for the GITHUB_TOKEN
  contents: read

jobs:
  lint:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v4
      - uses: actions/setup-node@v4
        with:
          node-version-file: .nvmrc            # one source of truth for the Node version
          cache: npm                           # caches ~/.npm keyed on package-lock.json
      - run: npm ci
      - run: npm run lint
      - run: npm run typecheck                 # if using TypeScript

  test:
    runs-on: ubuntu-latest
    services:                                  # real dependencies as service containers
      postgres:
        image: postgres:16
        env: { POSTGRES_USER: test, POSTGRES_PASSWORD: test, POSTGRES_DB: app_test }
        ports: ["5432:5432"]
        options: >-
          --health-cmd "pg_isready -U test -d app_test"
          --health-interval 5s --health-timeout 5s --health-retries 10
      redis:
        image: redis:7
        ports: ["6379:6379"]
        options: --health-cmd "redis-cli ping" --health-interval 5s --health-timeout 3s --health-retries 10
    env:
      NODE_ENV: test
      DATABASE_URL: postgres://test:test@localhost:5432/app_test
      REDIS_URL: redis://localhost:6379
      JWT_ACCESS_SECRET: ci-only-secret-ci-only-secret-ci-only
    steps:
      - uses: actions/checkout@v4
      - uses: actions/setup-node@v4
        with: { node-version-file: .nvmrc, cache: npm }
      - run: npm ci
      - run: npm run db:migrate                # migrations run against the CI database: tests them too
      - run: npm test -- --coverage
      - uses: actions/upload-artifact@v4       # keep the coverage report for reviewers
        if: always()
        with: { name: coverage, path: coverage/ }

  security:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v4
        with: { fetch-depth: 0 }               # full history so secret scanning sees all commits in the PR
      - uses: actions/setup-node@v4
        with: { node-version-file: .nvmrc, cache: npm }
      - run: npm ci
      - run: npm audit --omit=dev --audit-level=high      # fail on high/critical vulnerabilities in production deps
      - uses: gitleaks/gitleaks-action@v2                  # scan for committed secrets (check the action's current usage/licensing)
        env: { GITHUB_TOKEN: "${{ secrets.GITHUB_TOKEN }}" }
```

Pin third-party actions to a **full commit SHA** (not just a tag like `@v4`) for supply-chain safety on sensitive pipelines, and let Dependabot update them. Action versions and inputs change, so confirm current versions in each action's documentation.

### Speed tips

| Tip | Effect |
|---|---|
| **Cache dependencies** (`setup-node` with `cache: npm`) | Seconds instead of a minute to install |
| **Parallel jobs** (lint, test, security above) | Wall-clock time = the slowest job, not the sum |
| **`concurrency` with `cancel-in-progress`** | Stale runs don't waste minutes |
| **Docker layer caching** (`cache-from/to` with Buildx) | Image builds in seconds when only code changed |
| **Split slow suites** (matrix/sharding: `vitest --shard=1/4`) | Large suites finish quickly |
| **Run the cheap checks first** (`needs:` ordering) | Fast failure |
| **Test only affected code** on big monorepos (Nx, Turborepo caching) | Avoids rebuilding the world |

A CI run that takes 20 minutes stops being waited for. Aim for **under ~10 minutes** to a green/red answer.

### A test matrix

```yaml
test:
  strategy:
    fail-fast: false
    matrix:
      node: [20, 22]                  # your supported Node versions
  steps:
    - uses: actions/setup-node@v4
      with: { node-version: "${{ matrix.node }}", cache: npm }
```

Libraries need a matrix. Applications usually run **one** Node version (the one in the Dockerfile) and test exactly that, for production parity.

---

## Building and publishing the image

```yaml
# .github/workflows/build.yml (or a job that needs: [lint, test, security])
build:
  needs: [lint, test, security]
  if: github.ref == 'refs/heads/main'
  runs-on: ubuntu-latest
  permissions:
    contents: read
    id-token: write                              # needed for OIDC (below)
  outputs:
    image: ${{ steps.meta.outputs.image }}
  steps:
    - uses: actions/checkout@v4

    - uses: aws-actions/configure-aws-credentials@v4
      with:
        role-to-assume: arn:aws:iam::123456789012:role/github-actions-ecr-push    # OIDC: NO stored access keys
        aws-region: ap-south-1

    - id: ecr
      uses: aws-actions/amazon-ecr-login@v2

    - uses: docker/setup-buildx-action@v3

    - id: meta
      run: echo "image=${{ steps.ecr.outputs.registry }}/orders-api:${{ github.sha }}" >> "$GITHUB_OUTPUT"

    - uses: docker/build-push-action@v6
      with:
        context: .
        push: true
        tags: ${{ steps.meta.outputs.image }}                  # immutable tag = the commit SHA (never :latest)
        cache-from: type=gha
        cache-to: type=gha,mode=max
        build-args: GIT_SHA=${{ github.sha }}

    - name: Scan the image for vulnerabilities
      uses: aquasecurity/trivy-action@master                   # pin to a specific release in real pipelines
      with:
        image-ref: ${{ steps.meta.outputs.image }}
        severity: CRITICAL,HIGH
        exit-code: "1"
        ignore-unfixed: true
```

Principles:

- **Build once, promote the same image** through staging and production. Never rebuild per environment (`00-README.md`).
- **Tag with the commit SHA.** The running version is traceable to an exact commit, and rollback means redeploying a previous SHA.
- Scan **before** (or at least alongside) publishing, and scan on a schedule too, since new CVEs appear against old images.

---

## Authenticating to the cloud: OIDC, not stored keys

The old way: create an IAM user, generate a long-lived access key, paste it into GitHub secrets. That key never expires, can be exfiltrated from CI logs or a compromised dependency, and grants whatever the user allowed, forever.

The modern way: **OpenID Connect (OIDC) federation.** GitHub issues a short-lived signed token for each workflow run; AWS trusts GitHub as an identity provider and lets *that specific repo and branch* assume a role for about an hour.

```
GitHub Actions run ──(1) OIDC token: "I'm repo X, branch main"──▶ AWS STS
                   ◀──(2) temporary credentials (1 hour)──────────
```

```json
// IAM role trust policy: only THIS repo's main branch can assume the role
{
  "Effect": "Allow",
  "Principal": { "Federated": "arn:aws:iam::123456789012:oidc-provider/token.actions.githubusercontent.com" },
  "Action": "sts:AssumeRoleWithWebIdentity",
  "Condition": {
    "StringEquals": { "token.actions.githubusercontent.com:aud": "sts.amazonaws.com" },
    "StringLike":   { "token.actions.githubusercontent.com:sub": "repo:my-org/orders-api:ref:refs/heads/main" }
  }
}
```

The workflow needs `permissions: id-token: write` (shown above). Separate roles for **pushing images** and **deploying to production** keep permissions narrow (`06-aws.md`). GCP and Azure offer the same pattern (workload identity federation).

### Secrets in CI

| Do | Don't |
|---|---|
| Use **OIDC** for cloud access | Store long-lived cloud keys in repository secrets |
| Store remaining secrets in **GitHub secrets / environments**, scoped to the environments that need them | Put secrets in workflow files or the repository |
| Rely on log **masking**, but never `echo` secrets | Print secrets or dump the environment (`env`, `set -x` near secrets) |
| Restrict `GITHUB_TOKEN` with `permissions:` | Leave default broad write permissions on |
| **Be careful with workflows triggered by forks:** untrusted PRs must not get secrets (`pull_request` from forks gets none by design; avoid `pull_request_target` with checkout of PR code) | Run untrusted code with access to production credentials |
| Pin third-party actions by SHA | Use `@main` or unreviewed marketplace actions in privileged workflows |

Application secrets (database passwords, API keys) don't need to pass through CI at all, since the platform injects them at runtime from the secret manager (`01-environment-management.md`).

---

## Deployment strategies

How you swap the old version for the new one determines risk, downtime, and rollback speed.

| Strategy | How | Downtime | Rollback | Cost | Risk |
|---|---|---|---|---|---|
| **Recreate** | Stop all old, start all new | **Yes** | Redeploy old | Low | High: outage window |
| **Rolling** | Replace instances gradually (a few at a time) | None | Roll forward/back gradually | Low | Medium: old and new run side by side |
| **Blue/green** | Stand up a full new environment ("green") beside the old ("blue"); switch traffic at the load balancer | None | **Instant**: flip back | 2× capacity temporarily | Low |
| **Canary** | Send a small percentage (1–10%) to the new version; watch metrics; ramp up | None | Shift traffic back | Slightly above normal | **Lowest**: limits blast radius |
| **Feature flags** | Deploy code dark; enable per user/percentage at runtime | None | Flip the flag | Low | Separates *deploy* from *release* |

```
Rolling:      [v1][v1][v1]  →  [v1][v1][v2]  →  [v1][v2][v2]  →  [v2][v2][v2]

Blue/green:   LB ──▶ blue (v1)                LB ──▶ blue (v1) idle  (kept for rollback)
                     green (v2) warming   →          green (v2) live

Canary:       LB ──▶ 95% v1 / 5% v2  →  metrics fine?  →  50/50  →  100% v2    (or back to v1 at any step)
```

Practical guidance:

- **Rolling** with health checks is the default on ECS, Kubernetes, and most PaaS, and is enough for most teams. It requires **v1 and v2 to coexist** (compatible database schema and API contracts: next section).
- **Blue/green** (ECS via CodeDeploy, ALB weighted target groups) gives instant rollback at the cost of temporary double capacity.
- **Canary** is the best defense for risky changes and high traffic: needs good metrics to judge the canary automatically (`14-logging-observability/03-metrics-and-prometheus.md`).
- **Feature flags** decouple shipping code from exposing behavior, so incomplete features can merge to `main` safely and a bad one is disabled in seconds.

### ECS rolling deploy with automatic rollback

ECS can **detect a failing deployment and roll it back by itself** with a deployment circuit breaker:

```jsonc
// ECS service deployment configuration
{
  "deploymentConfiguration": {
    "minimumHealthyPercent": 100,            // never go below current capacity
    "maximumPercent": 200,                   // start new tasks before stopping old ones
    "deploymentCircuitBreaker": { "enable": true, "rollback": true }   // new tasks keep failing health checks → auto-rollback
  }
}
```

A new task that crashes on startup (e.g. config validation failure, `01-environment-management.md`) never becomes healthy, so the rollout stalls and rolls back, **and users never reach the broken version.** This is the payoff of fail-fast startup plus readiness probes (`02-graceful-shutdown-and-health-checks.md`).

---

## Database migrations in deployments

Code is easy to roll back. **Databases are not.** The hardest part of continuous deployment is changing the schema while old and new code run at the same time.

### The core problem

During a rolling deploy, **both versions are live simultaneously**. If v2's migration drops a column v1 still reads, v1 starts failing mid-deploy.

```
Migration drops "users.name" (v2 uses full_name)  →  v1 instances still running → SELECT name → 💥 errors
```

### The rule: make every migration backward compatible with the *previous* app version

Use the **expand → migrate → contract** pattern across **multiple releases**:

```
Release 1 (EXPAND):    add the new column (nullable / with default). Old code ignores it. App writes BOTH old and new.
                       migration is purely additive → safe with v0 still running
Backfill:              copy old data into the new column (in batches, as a background job)
Release 2 (MIGRATE):   app reads from the NEW column (still writes both, for safe rollback)
Release 3 (CONTRACT):  once nothing uses the old column and you're confident, drop it
```

| Change | Safe approach |
|---|---|
| **Add a column** | Add as nullable or with a default; deploy code that uses it after |
| **Rename a column** | Expand (add new) → dual-write → backfill → switch reads → drop old (never `ALTER ... RENAME` in one step) |
| **Drop a column/table** | Stop using it in code first, deploy, *then* drop in a later release |
| **Add `NOT NULL`** | Add nullable → backfill → add the constraint (use `NOT VALID` then `VALIDATE CONSTRAINT` in Postgres to avoid long locks) |
| **Add an index** | `CREATE INDEX CONCURRENTLY` (PostgreSQL) so writes aren't blocked |
| **Change a column type** | Add a new column, dual-write, backfill, switch, drop (don't rewrite a big table in place) |
| **Large backfills** | Batched background jobs, never one giant `UPDATE` holding locks (`11-async-processing/`) |

### When and how to run migrations

```
 ✅ Run migrations as a SEPARATE, EXPLICIT pipeline step, ONCE, BEFORE the new app version rolls out
 ❌ Don't run migrations at app startup in every instance (N instances race; slow start; a failed migration crash-loops the fleet)
```

```yaml
deploy-staging:
  needs: build
  steps:
    - name: Run database migrations (one-off task using the SAME image)
      run: |
        aws ecs run-task --cluster staging --task-definition orders-api-migrate \
          --launch-type FARGATE --network-configuration "$NETWORK_CONFIG" \
          --overrides '{"containerOverrides":[{"name":"api","command":["npm","run","db:migrate"]}]}'
        # wait for the task to stop and check its exit code; fail the pipeline if non-zero
    - name: Roll out the new application version
      run: ...
```

- Use the **same image** for the migration task as for the app (twelve-factor "admin processes": `00-README.md`).
- Make migrations **idempotent and transactional** where the database allows, and use a migration tool that records what has run (Knex, Prisma Migrate, node-pg-migrate, Flyway, Liquibase).
- Use a **lock** (the tool's own, or a Postgres advisory lock) so two concurrent runs can't collide.
- **Test migrations** in CI (the `db:migrate` step above) and rehearse destructive/long-running ones against a production-sized copy in staging.
- Migrations are **forward-only** in practice: "down" migrations on production data are rarely safe. Plan rollback as *deploy previous code* (which works because the schema is backward compatible), not *undo the schema*.
- **Take a backup/snapshot** before risky migrations (`06-aws.md`: RDS snapshots).

---

## Deploying to staging and production

```yaml
# .github/workflows/deploy.yml
name: Deploy

on:
  workflow_run:
    workflows: [CI]
    types: [completed]
    branches: [main]

jobs:
  deploy-staging:
    if: ${{ github.event.workflow_run.conclusion == 'success' }}
    runs-on: ubuntu-latest
    environment: staging                         # GitHub Environment: scoped secrets and variables
    permissions: { id-token: write, contents: read }
    steps:
      - uses: actions/checkout@v4
      - uses: aws-actions/configure-aws-credentials@v4
        with: { role-to-assume: "${{ vars.DEPLOY_ROLE_ARN }}", aws-region: ap-south-1 }

      # render the ECS task definition with the new image tag, then deploy and WAIT for stability
      - id: render
        uses: aws-actions/amazon-ecs-render-task-definition@v1
        with:
          task-definition: infra/ecs/task-definition.staging.json
          container-name: api
          image: ${{ vars.ECR_REGISTRY }}/orders-api:${{ github.event.workflow_run.head_sha }}

      - uses: aws-actions/amazon-ecs-deploy-task-definition@v2
        with:
          task-definition: ${{ steps.render.outputs.task-definition }}
          service: orders-api
          cluster: staging
          wait-for-service-stability: true       # the step fails if the rollout fails its health checks

      - name: Smoke tests
        run: npm run test:smoke -- --base-url https://staging.api.example.com

  deploy-production:
    needs: deploy-staging
    runs-on: ubuntu-latest
    environment: production                      # configure "required reviewers" on this environment = manual approval gate
    permissions: { id-token: write, contents: read }
    concurrency: { group: production-deploy, cancel-in-progress: false }     # never run two production deploys at once
    steps:
      # ... same as staging with production role, task definition, cluster ...
      - name: Smoke tests
        run: npm run test:smoke -- --base-url https://api.example.com
      - name: Mark the deploy on dashboards
        run: ./scripts/grafana-annotate.sh "Deployed ${{ github.event.workflow_run.head_sha }}"
```

(The exact action names and inputs evolve. Check the AWS actions' docs. The structure is what matters.)

### Gates between environments

- **Required reviewers** on the `production` GitHub Environment is a manual approval (Continuous *Delivery*); remove it for Continuous *Deployment* once your tests and monitoring earn that trust.
- **Smoke tests:** a tiny, fast, **read-mostly** suite against the freshly deployed environment (health endpoint, a login, one read path, one safe write). It proves the *deployment* worked, not that the code is correct.
- **Bake time / progressive delivery:** after production deploy, watch error rate and latency for a few minutes before declaring success. Tools like Argo Rollouts, Flagger, or CodeDeploy can automate promote/rollback on metrics.
- **Deployment windows & freezes:** many teams avoid deploying late Friday or during peak events; automation makes this a policy choice, not a necessity.

### Annotate deploys

Push a marker to Grafana (or log a `deployment_info` metric) so every dashboard shows *"v1.8.2 deployed here."* When a regression appears, "what changed?" has an immediate visual answer (`14-logging-observability/05-grafana.md`).

---

## Rollbacks

A deploy process is only as good as its **rollback**. Decide the rollback plan *before* shipping.

| Layer | Rollback approach |
|---|---|
| **Application code** | Redeploy the previous image tag (the previous commit SHA). Keep the last N images in the registry. Platforms with circuit breakers do this automatically |
| **Feature behavior** | Turn off the feature flag (seconds, no deploy) |
| **Configuration** | Revert the config/parameter change (versioned in Git or the parameter store) |
| **Database schema** | Generally **don't** roll back schemas: rely on backward-compatible migrations so previous code still works. For data damage, **restore from backup/point-in-time recovery** (a last resort) |
| **Messages already published / jobs already queued** | Consumers must tolerate old and new formats (`11-async-processing/02-workers-retry-dlq.md`: version your payloads) |

Rollback rules:

- **Rolling back is a deploy.** It goes through the same pipeline and the same health checks, just with an older SHA, and ideally via a **one-click "redeploy this version"** workflow: `workflow_dispatch` with an input for the SHA.
- **Prefer roll-forward for tiny fixes,** but keep rollback as the default response to an *unknown* problem. Stop the bleeding first, then investigate.
- **Practice it.** A rollback you've never run is a rollback that won't work at 3 a.m.
- **Define triggers:** e.g. "error ratio > 2% for 3 minutes after deploy" or "p95 doubles" → roll back automatically or page.

```yaml
# manual rollback workflow
on:
  workflow_dispatch:
    inputs:
      sha: { description: "Commit SHA of the image to redeploy", required: true }
```

---

## Branching and release flow

| Model | Description | Best for |
|---|---|---|
| **Trunk-based development** | Short-lived branches, merge to `main` daily, deploy from `main`, use feature flags for incomplete work | Teams practicing CI/CD; **recommended** |
| **GitHub Flow** | Feature branch → PR → merge to `main` → deploy | Most web services |
| **GitFlow** (`develop`, `release/*`, `hotfix/*`) | Long-lived branches and formal releases | Packaged software with versioned releases; usually too heavy for web services |

Habits that make CI/CD work:

- **Protect `main`**: require PRs, passing CI checks, and at least one review; disallow force-pushes; require linear history if you like.
- **Keep branches short-lived:** long-lived branches mean painful merges and big-bang releases.
- **Small PRs** are easier to review, test, deploy, and revert.
- **Semantic versioning and changelogs** can be automated from commit messages (`semantic-release`, `release-please`; Conventional Commits): `04-npm-ecosystem/01-semantic-versioning.md`.
- **Preview environments:** spin up an ephemeral environment per pull request (a namespace, or an ECS service with its own URL) for review and QA, torn down on merge.

---

## Dependency and supply-chain hygiene in CI

- **Dependabot or Renovate:** automated PRs for dependency, base-image, and GitHub Actions updates, which run through the same CI.
- **`npm audit` / OSV-Scanner / Snyk:** known-vulnerability checks (fail on high/critical production dependencies).
- **Lockfile integrity:** use `npm ci`; review lockfile changes in PRs.
- **CodeQL / Semgrep:** static analysis for vulnerabilities in your own code (`08-authentication-security/05-common-vulnerabilities.md`).
- **Secret scanning:** gitleaks in CI plus GitHub push protection.
- **Image scanning:** Trivy/Scout/ECR scanning (above), plus signing (cosign) and an SBOM for mature setups.
- **Reproducible builds:** pinned base images and action SHAs.

---

## Other CI systems

| Tool | Notes |
|---|---|
| **GitHub Actions** | Tight GitHub integration, large action ecosystem. Used for the examples |
| **GitLab CI/CD** | `.gitlab-ci.yml`; integrated registry, environments, review apps |
| **CircleCI, Buildkite, Jenkins** | Popular alternatives; Jenkins for self-hosted, heavily customized setups |
| **AWS CodePipeline/CodeBuild/CodeDeploy** | Native to AWS; blue/green deployments through CodeDeploy |
| **Argo CD / Flux (GitOps)** | A cluster controller continuously syncs the cluster to a Git repo of manifests: **deploy = merge a manifest change** |

The ideas (pipeline-as-code, immutable artifacts, environments, approvals, rollbacks) are identical across them.

---

## Common mistakes

```yaml
# ❌ deploying from a developer's laptop or via SSH: unrepeatable, unaudited
# ❌ rebuilding the image per environment instead of promoting one artifact
# ❌ deploying `:latest` or a mutable tag; no way to know or restore what's running
# ❌ long-lived cloud access keys in CI secrets (use OIDC)
# ❌ secrets echoed in logs, or exposed to workflows triggered by untrusted forks
# ❌ third-party actions referenced by moving tags in privileged workflows
# ❌ tests that need a real internet/third-party service → flaky pipelines
# ❌ a CI suite so slow people merge without waiting, or so flaky people ignore red builds
# ❌ migrations run at app startup in every instance; migrations that break the previous app version
# ❌ dropping/renaming columns in one release
# ❌ no health-check-gated rollout, so a broken version reaches users
# ❌ no tested rollback path; rollback = "revert and redeploy and hope"
# ❌ two production deploys running concurrently
# ❌ skipping staging "because it's a small change"
# ❌ deploying without annotating dashboards or watching metrics afterward
# ❌ unprotected main branch: direct pushes bypass CI
# ❌ hotfixes that skip the pipeline, leaving environments inconsistent
```

## Checklist

**CI**
- [ ] Every PR runs: install (`npm ci`), lint, type check, unit + integration tests against real service containers, coverage, security scans
- [ ] Stages ordered fast → slow, run in parallel, cached; total under ~10 minutes
- [ ] `main` is protected: PR + passing checks + review required
- [ ] One Node version source of truth (`.nvmrc`/Dockerfile) used in CI and production

**Build and publish**
- [ ] One image built per commit, tagged with the SHA, scanned, pushed to a private registry; immutable tags
- [ ] Cloud access via **OIDC**; narrow roles; no long-lived keys; third-party actions pinned

**Deploy**
- [ ] The same image promoted staging → production; environment-specific config injected at runtime
- [ ] Migrations run as a separate, one-off, locked step **before** rollout; backward compatible (expand → migrate → contract)
- [ ] Health-check-gated rollouts with automatic rollback (circuit breaker / canary analysis)
- [ ] Smoke tests after each deploy; approval gate for production (or automated promotion backed by metrics)
- [ ] One production deploy at a time; deploys annotated on dashboards

**Recovery**
- [ ] One-click rollback to a previous SHA, rehearsed
- [ ] Feature flags for risky changes; payloads versioned for queue/event consumers
- [ ] Backups/snapshots before risky data changes; point-in-time recovery tested

## Next

**`06-aws.md`** is the destination the pipeline deploys to: a reference architecture on AWS with ECS Fargate, an Application Load Balancer, RDS, ElastiCache, secrets, IAM, networking, and the costs to watch.
