# CI/CD

**Continuous Integration (CI)** automatically builds and tests every change so problems are caught within minutes, not at release time. **Continuous Delivery/Deployment (CD)** automatically ships what passes. Together they replace "someone runs a script on their laptop" with a pipeline that is **the same every time, visible to everyone, and boring**.

| Term | Meaning |
|---|---|
| **CI** | On every push/PR: install, lint, typecheck, test, build |
| **Continuous delivery** | Every passing change is *ready* to release; a human clicks "deploy" |
| **Continuous deployment** | Every passing change on `main` is deployed automatically |

Which CD flavor you want depends on your risk tolerance, test confidence, and rollback speed ([deployment](./02-deployment.md#rollbacks)). Most teams automate staging fully and gate production behind an approval or a release tag.

## The pipeline

```text
PR opened/updated                       merge to main
      │                                      │
      ▼                                      ▼
┌───────────┐   ┌───────────┐   ┌───────────┐   ┌─────────────┐   ┌────────────┐
│ install   │ → │ lint +    │ → │ unit +    │ → │ build       │ → │ e2e on the │
│ (npm ci)  │   │ typecheck │   │ integration│   │ (artifact)  │   │ built app  │
└───────────┘   └───────────┘   └───────────┘   └─────────────┘   └────────────┘
                                                        │                │
                                         preview deploy ◄┘                ▼
                                         (per PR)              deploy to staging → production
```

Order checks **fastest-first** so failures surface early and cheaply: static checks (seconds) before unit tests, those before the build, the build before slow E2E.

## A GitHub Actions workflow

GitHub Actions is used here, but every CI system (GitLab CI, CircleCI, Jenkins, Buildkite) has the same concepts: triggers, jobs, steps, caching, secrets, artifacts.

```yaml
# .github/workflows/ci.yml
name: CI

on:
  pull_request:
  push:
    branches: [main]

concurrency:
  group: ci-${{ github.ref }}
  cancel-in-progress: true          # a new push cancels the now-pointless older run

permissions:
  contents: read                    # least privilege: the default token can only read

jobs:
  verify:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v4
      - uses: actions/setup-node@v4
        with:
          node-version-file: .nvmrc      # one source of truth for the Node version
          cache: npm                     # caches ~/.npm keyed by the lockfile
      - run: npm ci
      - run: npm run lint
      - run: npm run typecheck
      - run: npm run test:run
      - run: npm run build
        env:
          VITE_API_URL: ${{ vars.API_URL }}
      - uses: actions/upload-artifact@v4
        with:
          name: dist
          path: dist
          retention-days: 7

  e2e:
    needs: verify
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v4
      - uses: actions/setup-node@v4
        with: { node-version-file: .nvmrc, cache: npm }
      - run: npm ci
      - run: npx playwright install --with-deps chromium
      - uses: actions/download-artifact@v4
        with: { name: dist, path: dist }
      - run: npx playwright test --project=chromium
      - uses: actions/upload-artifact@v4
        if: ${{ !cancelled() }}
        with:
          name: playwright-report
          path: playwright-report/
          retention-days: 14
```

(Action versions move on. Check each action's current major version and release notes.)

Things worth noticing:

- **`npm ci`**, with the committed lockfile, for reproducible installs ([build](./01-build.md#reproducible-builds)).
- **`node-version-file`** keeps CI, Docker, and local on the same Node.
- **`concurrency` + `cancel-in-progress`** saves minutes and money by killing superseded runs.
- **`permissions: contents: read`** restricts what a compromised step could do (see [security](#pipeline-security)).
- The **build** happens in `verify` and is shared as an **artifact**, so later jobs test and deploy exactly that output ([below](#build-once-deploy-many)).
- E2E runs only **after** cheaper checks pass, installs only the browser it needs, and always uploads the report so failures are debuggable ([Playwright in CI](../18-testing-and-debugging/06-e2e-testing-playwright.md#running-in-ci)).

## Speed

Slow pipelines get bypassed. Keep the PR feedback loop under ~10 minutes if you can:

- **Cache** dependencies (`setup-node` with `cache`), the Vite/TypeScript build caches if useful, Playwright browsers, and Docker layers.
- **Parallelize** independent work: lint, typecheck, and unit tests as separate jobs (or one job with parallel steps) rather than one long chain.
- **Shard** big test suites (`vitest --shard=1/3`, `playwright test --shard=1/4`).
- **Run less**: skip or reduce jobs by path (docs-only changes don't need E2E) with `paths` filters, and run the full browser matrix nightly or pre-release instead of on every PR.
- **Fail fast** on cheap checks before spending minutes on expensive ones.
- **Keep tests fast and deterministic** ([flaky tests](../18-testing-and-debugging/00-testing-fundamentals.md#flaky-tests)): retries hide slowness and erode trust.

## Build once, deploy many

> Build **one artifact per commit**, test **that artifact**, and **promote the same artifact** from staging to production.

If you rebuild for production, you ship something you never tested: a different dependency resolution, a different timestamp, or a different env value can change behavior. Instead:

1. CI builds `dist/` (or a Docker image tagged with the commit SHA) once.
2. That artifact is deployed to staging and verified.
3. The *same* artifact is promoted to production.

The difference between environments then comes from **runtime configuration** ([runtime config](./00-environment-variables.md#runtime-configuration), [Docker](./03-docker.md#runtime-configuration)), not from rebuilding. If your setup bakes `VITE_*` values at build time (simpler), you'll build per environment. That's acceptable, but keep the *same commit and lockfile*, and be aware of the trade-off.

## Preview deployments

Deploy every pull request to its own URL so reviewers (and designers, QA) can click through the change:

- Hosts like Netlify, Vercel, and Cloudflare Pages do this automatically from Git.
- With your own pipeline, deploy `dist/` to a path or subdomain per PR (`pr-123.preview.example.com`) and **post the URL as a PR comment**. Clean up when the PR closes.
- Point previews at **non-production backends** and mark them `noindex`.
- Optionally run E2E or Lighthouse against the preview URL.

## Deploy jobs

```yaml
# .github/workflows/deploy.yml (excerpt)
deploy-production:
  needs: [verify, e2e]
  if: github.ref == 'refs/heads/main'
  runs-on: ubuntu-latest
  environment: production              # enables required reviewers + environment-scoped secrets
  steps:
    - uses: actions/download-artifact@v4
      with: { name: dist, path: dist }
    - name: Deploy
      run: ./scripts/deploy.sh dist     # host-specific: CLI upload, S3 sync + CDN invalidation, etc.
      env:
        DEPLOY_TOKEN: ${{ secrets.DEPLOY_TOKEN }}
    - name: Smoke test
      run: npx playwright test --project=smoke
      env:
        BASE_URL: https://app.example.com
```

- **`needs`** ensures deployment only happens after everything passes.
- **`environment: production`** lets you require a **manual approval**, restrict which branches may deploy, and scope secrets to that environment.
- A **post-deploy smoke test** against the live URL verifies the release works where it matters.
- Tag the release in your monitoring tool as part of the deploy ([release tracking](./06-error-monitoring-and-logging.md#context-that-makes-errors-actionable)).
- Make deployment **idempotent**: re-running it is safe, which makes retries and rollbacks straightforward.

### Triggers for production

| Strategy | Trigger | Good for |
|---|---|---|
| Every merge to `main` | `push` | Teams with strong tests and fast rollback (continuous deployment) |
| Manual approval | `environment` with reviewers | Most teams: automatic to staging, one click to production |
| Release tags | `push: tags: ["v*"]` | Versioned releases, clear audit trail |
| Scheduled | `schedule` (cron) | Regular release trains |

## Secrets and environments

The pipeline needs credentials: a deploy token, a source-map upload token, registry access.

- Store them as **encrypted secrets** (`secrets.X`), never in the repo or workflow file.
- Use **environment-scoped** secrets so only the production job (behind approval) can read production credentials.
- **Public configuration** (API URL, feature flags) belongs in plain **variables** (`vars.X`), not secrets. Secrets are masked in logs, which makes debugging harder if you over-use them.
- **Build-time secrets must not use the `VITE_` prefix**, or they're compiled into the bundle ([env vars](./00-environment-variables.md#env-vars-in-cicd)).
- Prefer **short-lived credentials via OIDC** (the CI provider proves its identity to your cloud, which issues a temporary token) over long-lived access keys stored as secrets.
- **Rotate** credentials, scope them minimally (a deploy token that can only deploy), and revoke them when people or tools leave.
- Never `echo` secrets or print `env` in debug steps.

## Pipeline security

CI runs code with access to secrets, which makes it an attractive target:

- **Least privilege**: set `permissions:` explicitly (read-only by default), granting write access only to jobs that need it.
- **Untrusted code**: workflows triggered by `pull_request` from **forks** don't get secrets, by design. Be extremely careful with `pull_request_target` (runs with secrets in the base repo's context). Never check out and run untrusted PR code under it.
- **Pin third-party actions** to a full commit SHA (not just a mutable tag like `@v4`) for anything sensitive, and review what actions you use. A compromised action runs in your pipeline.
- **Don't interpolate untrusted input** (PR titles, branch names, issue text) directly into shell commands (`run: echo ${{ github.event.pull_request.title }}`). Pass through environment variables instead, to avoid script injection.
- **Protect `main`**: require status checks, reviews, and up-to-date branches before merging (below).
- **Supply chain**: the lockfile + `npm ci`, dependency update PRs, and audit ([security](./05-security.md#supply-chain)).

## Branch protection and required checks

CI only helps if it's **enforced**:

- Require the CI checks to pass before merging to `main`.
- Require at least one code review.
- Disallow force-pushes to `main`.
- Keep branches short-lived and merge often. Long-lived branches turn CI into a source of conflict, not confidence.

## Dependency updates

Outdated dependencies accumulate vulnerabilities and make upgrades painful. Automate the nudge with **Dependabot** or **Renovate**:

- They open PRs for new versions; your CI validates them.
- **Group** minor/patch updates to reduce noise; handle majors deliberately.
- Auto-merge only low-risk updates, and only if your tests would genuinely catch breakage.
- Review changelogs for runtime libraries (React, router, query), where behavior changes matter.

## Quality gates (beyond tests)

Add checks that keep quality from eroding:

- **Bundle size budget**: fail if the entry chunk grows past a threshold ([bundle optimization](../14-performance/05-bundle-optimization.md#set-a-budget-and-watch-it)).
- **Lighthouse CI** on key pages against a preview, to catch performance and accessibility regressions ([profiling](../14-performance/00-profiling-and-measuring.md#performance-budgets-and-regression)).
- **Accessibility checks** (axe) in component or E2E tests ([a11y checklist](../08-accessibility/04-accessibility-checklist.md)).
- **Coverage trends**: used as a signal, not a gate on a magic number ([coverage](../18-testing-and-debugging/00-testing-fundamentals.md#coverage)).
- **Formatting** (Prettier check) and **commit/PR conventions**, if your team uses them.
- **Docker image scan** for vulnerabilities, if you ship containers ([Docker](./03-docker.md#security-hardening)).

Gates should be **actionable and low-noise**. A check people learn to ignore is worse than none.

## Handling failures

- **Make failures easy to read**: clear step names, test output, uploaded artifacts (reports, traces, screenshots).
- **Never normalize a red `main`.** If `main` is broken, fixing it is the team's top priority (revert first, investigate after).
- **Treat flaky tests as bugs**: quarantine or fix, and don't just rerun until green.
- **Notify the right people** (Slack/Teams) for failures on `main` and failed deploys, without spamming on every PR run.
- **Re-runs should be safe**: deployments and migrations must be idempotent.

## Versioning and releases

- Stamp each build with the **commit SHA** ([version stamping](./01-build.md#version-stamping)).
- For published packages or versioned apps, **semantic versioning** plus a changelog (tools like changesets or release-please can automate this from commit messages or PR descriptions).
- Keep a **changelog or release notes**, even briefly, so support and QA know what changed.

## Monorepos

When one repo holds several apps and packages, CI should **build and test only what changed** (affected-graph tools like Turborepo, Nx, or pnpm filters) and cache aggressively. Otherwise pipelines grow linearly with the repo. The principles in this note still apply; the orchestration is tool-specific.

## Common mistakes

- **Running CI only before releases**, instead of on every PR.
- **`npm install` instead of `npm ci`**, with an uncommitted or ignored lockfile.
- **Rebuilding per environment** and shipping an artifact that was never tested.
- **Secrets in the repo, workflow file, or `VITE_*` variables.**
- **Exposing production secrets to every job or to PRs from forks.**
- **Overly broad token permissions** and unpinned third-party actions.
- **Script injection** through unescaped PR titles or branch names in `run:` steps.
- **No branch protection**, so red builds get merged.
- **A slow, flaky pipeline** that developers learn to ignore or bypass.
- **No post-deploy verification**, so a broken deploy is found by users.
- **Manual production deploys** with no record, approval, or rollback path.
- **No dependency update process**, then a painful, risky upgrade later.
- **Missing artifacts and reports** on failure, so CI failures can't be reproduced.
- **Treating coverage or Lighthouse scores as goals** rather than guardrails.

## Quick summary

- **CI** runs install → lint → typecheck → test → build on every PR; **CD** ships what passes. Order checks fastest-first and keep PR feedback quick (cache, parallelize, shard, run less).
- **Build once, deploy many**: produce one artifact (or image tagged by commit SHA), test it, and promote it. Environment differences come from runtime config.
- Use **preview deployments** per PR and **staging → production** with approval gates, a **smoke test**, and release tagging.
- Handle **secrets** as encrypted, environment-scoped secrets (prefer OIDC and short-lived credentials); public config goes in variables; build-time secrets never use `VITE_`.
- Harden the pipeline: least-privilege `permissions`, pinned actions, careful with forks and `pull_request_target`, and no untrusted input in shell commands.
- Enforce with **branch protection**, automate **dependency updates**, and add low-noise **quality gates** (bundle size, Lighthouse, accessibility).
- Fix red `main` immediately and treat flakiness as a bug.

## Next

[05 — Security](./05-security.md)