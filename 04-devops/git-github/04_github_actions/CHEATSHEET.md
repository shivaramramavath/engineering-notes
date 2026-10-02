# GitHub Actions Cheatsheet

A quick reference for after you have read the chapters. Each section points to the chapter with the full explanation.

## Workflow skeleton (ch. 01, 02)

```yaml
name: CI                                   # display name
run-name: CI for ${{ github.ref_name }}    # optional per-run name

on:                                        # required: triggers
  push:
    branches: [main]
  pull_request:
  workflow_dispatch:

permissions:                               # token scopes
  contents: read

env:                                       # workflow-wide variables
  NODE_ENV: test

concurrency:                               # overlap control
  group: ci-${{ github.ref }}
  cancel-in-progress: true

jobs:                                      # required
  build:
    runs-on: ubuntu-latest                 # required
    timeout-minutes: 15
    steps:                                 # required
      - uses: actions/checkout@v4
      - run: npm ci
      - run: npm test
```

Location: `.github/workflows/*.yml` (or `.yaml`), directly inside that folder.

## Triggers (ch. 03)

| Event | Fires when |
|-------|-----------|
| `push` | Commits or tags pushed |
| `pull_request` | PR opened, updated (`synchronize`), reopened |
| `workflow_dispatch` | Manual run, with optional `inputs` |
| `schedule` | Cron time (UTC, default branch only) |
| `release` | Release created, published, and so on |
| `workflow_call` | Called by another workflow |
| `workflow_run` | Another workflow completed |
| `repository_dispatch` | External API call |
| `issues`, `issue_comment` | Issue activity |
| `pull_request_target` | PR event in the **base** repo context (dangerous with PR code) |

```yaml
on:
  push:
    branches: [main, "release/**"]
    branches-ignore: [...]          # not together with `branches`
    tags: ["v*.*.*"]
    paths: ["src/**", "!**.md"]
    paths-ignore: ["docs/**"]       # not together with `paths`
  pull_request:
    types: [opened, synchronize, reopened, ready_for_review]
  schedule:
    - cron: "30 2 * * 1-5"          # min hour day month weekday (UTC)
  workflow_dispatch:
    inputs:
      env:
        type: choice                # string | number | boolean | choice | environment
        options: [staging, production]
        default: staging
```

| Glob | Matches |
|------|---------|
| `*` | Anything except `/` |
| `**` | Anything including `/` |
| `?` | One character |
| `!pat` | Exclude |

## Job keys (ch. 04)

```yaml
jobs:
  deploy:
    name: Deploy
    runs-on: ubuntu-latest          # or [self-hosted, linux, x64]
    needs: [build, test]            # wait for these jobs
    if: github.ref == 'refs/heads/main'
    environment: production         # approvals, scoped secrets
    permissions: { contents: read }
    concurrency: { group: deploy, cancel-in-progress: false }
    timeout-minutes: 20             # default 360
    continue-on-error: false
    env: { KEY: value }
    outputs:
      version: ${{ steps.ver.outputs.value }}
    strategy: { ... }               # matrix
    container: node:20              # run steps in a container
    services: { ... }               # sidecar containers
    steps: [ ... ]
```

A job that calls a reusable workflow has only `uses`, `with`, `secrets`, `needs`, `if`, `strategy`, `permissions`, `concurrency`.

## Step keys

```yaml
steps:
  - name: Label in the log
    id: myid                         # reference as steps.myid.outputs.x
    uses: owner/repo@ref             # OR run:
    with: { input: value }           # inputs for `uses`
    run: |                           # shell commands
      echo "hello"
    shell: bash
    working-directory: ./app
    env: { KEY: value }
    if: success()
    continue-on-error: false
    timeout-minutes: 5
```

Each step is `uses` **or** `run`, never both. Each `run` is a separate process.

## Runners (ch. 06)

| Label | Notes |
|-------|-------|
| `ubuntu-latest`, `ubuntu-24.04`, `ubuntu-22.04` | Linux; cheapest |
| `windows-latest` | Windows (2x minutes on private repos) |
| `macos-latest` | macOS (10x minutes on private repos) |
| `[self-hosted, linux, x64]` | Your own machine; never on public repos |

Check GitHub's docs for current labels.

## Using actions (ch. 05)

```yaml
- uses: actions/checkout@v4                 # owner/repo@ref
- uses: github/codeql-action/analyze@v3     # owner/repo/path@ref
- uses: ./.github/actions/my-action         # local (checkout first)
- uses: docker://alpine:3.20                # container image
- uses: some/action@<full-40-char-sha>      # pinned (recommended for third-party)
```

| Commonly used | Purpose |
|---------------|---------|
| `actions/checkout` | Get the code (`fetch-depth: 0` for full history) |
| `actions/setup-node` / `setup-python` / `setup-java` / `setup-go` | Toolchains (+ `cache:`) |
| `actions/cache` | Cache dependencies |
| `actions/upload-artifact` / `download-artifact` | Keep or hand over files |
| `actions/github-script` | JavaScript with the GitHub API |
| `docker/build-push-action` | Build and push images |

## Variables and secrets (ch. 07)

```yaml
env:
  APP: demo                                 # workflow, job, or step level; narrowest wins

steps:
  - run: echo "$APP"                        # shell expands at run time
  - run: echo "${{ env.APP }}"              # GitHub substitutes before the shell
  - run: echo "${{ vars.REGION }}"          # configuration variable (plain text)
  - run: ./deploy.sh
    env:
      API_KEY: ${{ secrets.API_KEY }}       # give secrets to the step that needs them
```

| Store | Where | Masked in logs | Scopes |
|-------|-------|----------------|--------|
| `env` | In the YAML | No | Workflow, job, step |
| `vars` | Settings, Variables | No | Repo, environment, org |
| `secrets` | Settings, Secrets | Yes | Repo, environment, org |

Default variables: `CI`, `GITHUB_SHA`, `GITHUB_REF`, `GITHUB_REF_NAME`, `GITHUB_REPOSITORY`, `GITHUB_ACTOR`, `GITHUB_EVENT_NAME`, `GITHUB_RUN_ID`, `GITHUB_RUN_NUMBER`, `GITHUB_WORKSPACE`, `RUNNER_OS`.

`secrets` cannot be used directly in `if:`; copy to `env` first.

## Passing data (ch. 04, 09)

```yaml
# Step output -> later step
- id: ver
  run: echo "value=1.2.3" >> "$GITHUB_OUTPUT"
- run: echo "${{ steps.ver.outputs.value }}"

# Env var for later steps / PATH entry
- run: echo "MODE=release" >> "$GITHUB_ENV"
- run: echo "$HOME/bin" >> "$GITHUB_PATH"

# Job output -> other job
jobs:
  a:
    outputs: { version: "${{ steps.ver.outputs.value }}" }
  b:
    needs: a
    steps:
      - run: echo "${{ needs.a.outputs.version }}"
```

| Mechanism | For |
|-----------|-----|
| Job `outputs` | Small values between jobs |
| Artifacts | Files between jobs, kept after the run |
| Cache | Speeding up future runs |

## Contexts (ch. 08)

| Context | Examples |
|---------|----------|
| `github` | `github.event_name`, `.ref`, `.ref_name`, `.sha`, `.actor`, `.repository`, `.run_id`, `.head_ref`, `.base_ref`, `.event.*` |
| `env` / `vars` / `secrets` | `env.NAME`, `vars.NAME`, `secrets.NAME` |
| `steps` | `steps.id.outputs.x`, `.outcome`, `.conclusion` |
| `needs` | `needs.job.outputs.x`, `needs.job.result` |
| `matrix` / `strategy` | `matrix.node`, `strategy.job-index` |
| `inputs` | `inputs.name` (dispatch and `workflow_call`) |
| `runner` | `runner.os`, `runner.arch`, `runner.temp` |
| `job` | `job.status` |

## Expressions and conditions (ch. 08)

```yaml
if: github.ref == 'refs/heads/main'                      # wrapper optional
if: ${{ !contains(github.ref, 'release') }}              # wrapper REQUIRED if it starts with !
env:
  TARGET: ${{ github.ref == 'refs/heads/main' && 'prod' || 'preview' }}   # ternary idiom
  NAME: ${{ inputs.name || 'default' }}                  # default value
```

| Function | Purpose |
|----------|---------|
| `contains(a, b)` | Substring or array membership |
| `startsWith(s, p)` / `endsWith(s, x)` | Prefix and suffix |
| `format('{0}-{1}', a, b)` | String formatting |
| `join(array, ', ')` | Join |
| `toJSON(x)` / `fromJSON(s)` | Serialize and parse |
| `hashFiles('**/package-lock.json')` | Hash for cache keys |
| `success()` / `failure()` / `always()` / `cancelled()` | Status checks |

| Goal | Condition |
|------|-----------|
| Only on `main` | `github.ref == 'refs/heads/main'` |
| Only on tags | `startsWith(github.ref, 'refs/tags/')` |
| Not from a fork | `github.event.pull_request.head.repo.full_name == github.repository` |
| PR has label | `contains(github.event.pull_request.labels.*.name, 'deploy')` |
| Cleanup after anything | `always()` |
| Run after failure | `failure()` |
| Job after a failed need | `always() && needs.x.result == 'failure'` |

Only single quotes for strings inside expressions. Non-empty strings (even `'false'`) are truthy.

## Matrix (ch. 10)

```yaml
strategy:
  fail-fast: false                  # default true
  max-parallel: 4
  matrix:
    os: [ubuntu-latest, windows-latest]
    node: [18, 20]
    exclude:
      - { os: windows-latest, node: 18 }
    include:
      - { os: ubuntu-latest, node: 22, experimental: true }
runs-on: ${{ matrix.os }}
```

Limit: 256 jobs per matrix. Unique artifact names per combination. Dynamic: `matrix: ${{ fromJSON(needs.plan.outputs.matrix) }}`.

## Concurrency (ch. 10)

```yaml
concurrency:
  group: ${{ github.workflow }}-${{ github.ref }}
  cancel-in-progress: true          # CI: true   |   deploys: false
```

One running plus one pending per group. Never cancel a deployment midway.

## Permissions (ch. 11)

```yaml
permissions:
  contents: read                    # anything not listed becomes "none"
```

| Task | Scopes |
|------|--------|
| Plain CI | `contents: read` |
| Comment on PR | `pull-requests: write` |
| Issues | `issues: write` |
| Push commit, tag, release | `contents: write` |
| Push to GHCR | `packages: write` |
| Code scanning | `security-events: write` |
| Pages | `pages: write`, `id-token: write` |
| OIDC to cloud | `id-token: write` |
| Attestations | `id-token: write`, `attestations: write` |

`permissions: {}` = none. A job-level block replaces the workflow-level one.

## Environments (ch. 11)

```yaml
environment:
  name: production
  url: ${{ steps.deploy.outputs.url }}
```

Provides required reviewers, wait timers, branch restrictions, and environment-scoped secrets and variables. Same secret name can hold a different value per environment.

## Caching and artifacts (ch. 09)

```yaml
# Built-in caching (easiest)
- uses: actions/setup-node@v4
  with: { node-version: 20, cache: npm }

# Manual cache
- uses: actions/cache@v4
  with:
    path: ~/.npm
    key: ${{ runner.os }}-npm-${{ hashFiles('**/package-lock.json') }}
    restore-keys: ${{ runner.os }}-npm-

# Artifacts
- uses: actions/upload-artifact@v4
  with: { name: dist, path: dist/, retention-days: 7 }
- uses: actions/download-artifact@v4
  with: { name: dist, path: dist }
```

Caches and v4 artifacts are immutable. Cache `~/.npm`, not `node_modules`.

## Reusable workflows (ch. 12)

```yaml
# Called workflow
on:
  workflow_call:
    inputs:  { version: { type: string, required: true } }
    secrets: { token: { required: true } }
    outputs: { result: { value: "${{ jobs.build.outputs.result }}" } }

# Caller (job level)
jobs:
  call:
    uses: ./.github/workflows/build.yml       # or org/repo/.github/workflows/build.yml@v1
    with: { version: "1.2.3" }
    secrets:
      token: ${{ secrets.TOKEN }}             # or: secrets: inherit
```

`github` context is the caller's; `env` is not inherited; permissions can only narrow.

## Custom actions (ch. 13)

```yaml
# action.yml (composite)
name: My action
description: Does a thing
inputs:
  who: { description: Whom, default: world }
outputs:
  result:
    description: Output
    value: ${{ steps.s.outputs.result }}
runs:
  using: composite
  steps:
    - id: s
      run: echo "result=Hello ${{ inputs.who }}" >> "$GITHUB_OUTPUT"
      shell: bash                              # required in composite run steps
```

| Type | `runs.using` | Notes |
|------|--------------|-------|
| Composite | `composite` | Steps; start here |
| JavaScript | `node20` (check current) | Fast, cross-platform; commit bundled `dist/` |
| Docker | `docker` | Linux only; slowest start |

## Docker (ch. 16)

```yaml
permissions: { contents: read, packages: write }
steps:
  - uses: actions/checkout@v4
  - uses: docker/setup-buildx-action@v3
  - uses: docker/login-action@v3
    with:
      registry: ghcr.io
      username: ${{ github.actor }}
      password: ${{ secrets.GITHUB_TOKEN }}
  - id: meta
    uses: docker/metadata-action@v5
    with:
      images: ghcr.io/${{ github.repository }}
      tags: |
        type=ref,event=branch
        type=semver,pattern={{version}}
        type=sha
  - uses: docker/build-push-action@v6
    with:
      context: .
      push: ${{ github.event_name != 'pull_request' }}
      tags: ${{ steps.meta.outputs.tags }}
      labels: ${{ steps.meta.outputs.labels }}
      cache-from: type=gha
      cache-to: type=gha,mode=max
```

Image names must be lowercase. Deploy by SHA or digest, not `latest`. Never pass secrets as build args.

## Security quick rules (ch. 17)

```yaml
# ✗ Injection: untrusted text inside the script
- run: echo "${{ github.event.pull_request.title }}"

# ✓ Pass through an environment variable
- run: echo "$TITLE"
  env:
    TITLE: ${{ github.event.pull_request.title }}
```

| Rule | Do |
|------|----|
| Token | `permissions: contents: read` in every workflow |
| Actions | Pin third-party actions to a full SHA; Dependabot for updates |
| Triggers | Do not run PR code under `pull_request_target` or `workflow_run` |
| Secrets | Environment secrets for production; one secret per value |
| Cloud | OIDC (`id-token: write`) with a narrow `sub` condition |
| Runners | No self-hosted runners on public repos |
| Files | CODEOWNERS on `.github/workflows/` |
| Scanning | `actionlint`, `zizmor`, CodeQL, secret scanning |

Untrusted values: issue and PR titles and bodies, branch names, commit messages, comments, author names, and any output derived from them.

## OIDC example (ch. 17)

```yaml
permissions: { id-token: write, contents: read }
steps:
  - uses: aws-actions/configure-aws-credentials@v4
    with:
      role-to-assume: arn:aws:iam::123456789012:role/github-deploy
      aws-region: eu-west-1
```

| Provider | Action |
|----------|--------|
| AWS | `aws-actions/configure-aws-credentials` |
| Azure | `azure/login` |
| Google Cloud | `google-github-actions/auth` |

## Workflow commands (ch. 18)

```bash
echo "::debug::message"                       # visible with debug logging
echo "::notice::message"
echo "::warning file=a.js,line=3::message"
echo "::error file=a.js,line=3,title=T::msg"  # annotates only; add `exit 1` to fail
echo "::group::Title"; ...; echo "::endgroup::"
echo "::add-mask::$VALUE"                     # hide a value in logs
```

| File | Use |
|------|-----|
| `$GITHUB_OUTPUT` | `name=value` step outputs |
| `$GITHUB_ENV` | `NAME=value` for later steps |
| `$GITHUB_PATH` | Add a directory to `PATH` |
| `$GITHUB_STEP_SUMMARY` | Markdown on the run summary page |

Multi-line value:

```bash
D="EOF_$(openssl rand -hex 8)"
{ echo "report<<$D"; cat report.txt; echo "$D"; } >> "$GITHUB_OUTPUT"
```

`::set-output` and `::set-env` are removed; use the files above.

## Debugging (ch. 18)

| Action | How |
|--------|-----|
| Extra logs on one run | **Re-run jobs** and tick **Enable debug logging** |
| Always-on debug | Secret or variable `ACTIONS_STEP_DEBUG=true` (and `ACTIONS_RUNNER_DEBUG=true`) |
| Dump a context | `env: CTX: ${{ toJSON(github) }}` then `echo "$CTX"` (never `secrets`) |
| Trace shell | `set -euxo pipefail` |
| Lint workflows | `actionlint` |
| Run locally | `act` (approximate) |

```bash
gh run list
gh run view <id> --log-failed
gh run watch
gh run rerun <id> --failed
gh run download <id>
gh workflow run deploy.yml -f environment=staging
```

## Common errors (ch. 18)

| Message | Usual fix |
|---------|-----------|
| Invalid workflow file | Indentation, keys, quoting; run `actionlint` |
| Resource not accessible by integration | Add a scope to `permissions:` |
| exit code 127 | Command not installed or not on `PATH` |
| Permission denied on a script | `chmod +x` or `git update-index --chmod=+x` |
| `^M` bad interpreter | CRLF line endings; use LF |
| Empty secret | Fork PR, wrong environment, or typo |
| Variable missing next step | Use `$GITHUB_ENV`, not `export` |
| Workflow does not start | Wrong folder, filters, or `GITHUB_TOKEN`-created event |
| Waiting for a runner | No runner matches labels |

## YAML gotchas (ch. 02)

| Gotcha | Fix |
|--------|-----|
| Tabs | Use spaces |
| `python-version: 3.10` becomes `3.1` | Quote: `"3.10"` |
| `if: !contains(...)` fails | Wrap: `${{ !contains(...) }}` |
| Colon in a value | Quote it |
| `uses` and `run` in one step | Split into two steps |

## Examples in this section

| File | Shows |
|------|-------|
| [`examples/nodejs-ci.yml`](./examples/nodejs-ci.yml) | Lint, matrix tests, build, gate job |
| [`examples/docker-build-push.yml`](./examples/docker-build-push.yml) | Build and push to GHCR with caching and tags |
| [`examples/deploy-with-approval.yml`](./examples/deploy-with-approval.yml) | Staging, smoke test, approved production deploy, rollback input |
| [`examples/release-automation.yml`](./examples/release-automation.yml) | Tag, test, build, GitHub Release |
| [`examples/reusable-workflow-caller.yml`](./examples/reusable-workflow-caller.yml) | Calling reusable workflows with inputs, secrets, outputs, matrix |
