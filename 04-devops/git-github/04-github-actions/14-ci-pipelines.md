# CI Pipelines

**Continuous Integration (CI)** means automatically building and testing every change as soon as it is pushed, so problems are found minutes after they are introduced instead of days later. A **CI pipeline** is the workflow that does it: check out the code, set up tools, install dependencies, lint, build, test, and report.

```
push / pull request  →  checkout  →  setup  →  install  →  lint  →  build  →  test  →  report
                                                                                         │
                                                              ✓ merge allowed  ◄─────────┘
```

## The simplest CI workflow

```yaml
name: CI

on:
  pull_request:
  push:
    branches: [main]

permissions:
  contents: read

jobs:
  test:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v4
      - uses: actions/setup-node@v4
        with:
          node-version: 20
          cache: npm
      - run: npm ci
      - run: npm test
```

Every pull request and every push to `main` now gets tested.

## What a CI pipeline should do

| Stage | Purpose | Example commands |
|-------|---------|------------------|
| **Checkout** | Get the code | `actions/checkout` |
| **Setup** | Install language runtimes and tools | `actions/setup-node`, `setup-python` |
| **Install** | Fetch dependencies from lockfiles | `npm ci`, `pip install -r requirements.txt` |
| **Lint and format** | Style and static checks | `eslint`, `prettier --check`, `ruff` |
| **Type check** | Catch type errors | `tsc --noEmit`, `mypy` |
| **Build** | Prove the project compiles | `npm run build`, `mvn package` |
| **Test** | Unit and integration tests | `npm test`, `pytest` |
| **Report** | Coverage, annotations, artifacts | Upload artifacts, job summary |

Order stages from **fastest to slowest** so cheap failures surface first.

## Structuring jobs

### One job: simple and cheap

```yaml
jobs:
  ci:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v4
      - uses: actions/setup-node@v4
        with: { node-version: 20, cache: npm }
      - run: npm ci
      - run: npm run lint
      - run: npm run build
      - run: npm test
```

Install once, run everything. Best for small projects.

### Several jobs: faster feedback, more overhead

```yaml
jobs:
  lint:
    runs-on: ubuntu-latest
    steps: [...]          # checkout, setup, install, lint

  test:
    runs-on: ubuntu-latest
    steps: [...]          # checkout, setup, install, test

  build:
    needs: [lint, test]
    runs-on: ubuntu-latest
    steps: [...]
```

```
lint ─┐
      ├──► build
test ─┘
```

| | One job | Several jobs |
|---|---------|--------------|
| Wall-clock time | Longer | Shorter (parallel) |
| Minutes consumed | Fewer | More (each repeats setup) |
| Clarity of failures | One long log | Failure named by job |
| Best for | Small projects | Larger projects with slow tests |

## A full Node.js pipeline

```yaml
name: Node.js CI

on:
  pull_request:
  push:
    branches: [main]

permissions:
  contents: read

concurrency:
  group: ci-${{ github.workflow }}-${{ github.head_ref || github.run_id }}
  cancel-in-progress: ${{ github.event_name == 'pull_request' }}

jobs:
  lint:
    runs-on: ubuntu-latest
    timeout-minutes: 10
    steps:
      - uses: actions/checkout@v4
      - uses: actions/setup-node@v4
        with:
          node-version: 20
          cache: npm
      - run: npm ci
      - run: npm run lint
      - run: npx tsc --noEmit

  test:
    runs-on: ubuntu-latest
    timeout-minutes: 15
    strategy:
      fail-fast: false
      matrix:
        node: [18, 20, 22]
    steps:
      - uses: actions/checkout@v4
      - uses: actions/setup-node@v4
        with:
          node-version: ${{ matrix.node }}
          cache: npm
      - run: npm ci
      - run: npm test -- --coverage
      - uses: actions/upload-artifact@v4
        if: always()
        with:
          name: coverage-node${{ matrix.node }}
          path: coverage/
          if-no-files-found: ignore

  build:
    needs: [lint, test]
    runs-on: ubuntu-latest
    timeout-minutes: 10
    steps:
      - uses: actions/checkout@v4
      - uses: actions/setup-node@v4
        with:
          node-version: 20
          cache: npm
      - run: npm ci
      - run: npm run build
      - uses: actions/upload-artifact@v4
        with:
          name: dist
          path: dist/
          retention-days: 7
```

The complete runnable file is in [`examples/nodejs-ci.yml`](./examples/nodejs-ci.yml).

What this workflow does:

- **Triggers** on PRs and pushes to `main` only, avoiding double runs
- **Cancels** stale PR runs with `concurrency`
- **Lints** and type-checks in its own job for quick feedback
- **Tests** on three Node versions in parallel, with `fail-fast: false` to see every failure
- **Builds** only after lint and tests pass, and keeps the output as an artifact
- Uses `npm ci`, dependency caching, timeouts, and a read-only token

## Testing against a database with service containers

```yaml
jobs:
  integration:
    runs-on: ubuntu-latest
    services:
      postgres:
        image: postgres:16
        env:
          POSTGRES_PASSWORD: postgres
          POSTGRES_DB: app_test
        ports:
          - 5432:5432
        options: >-
          --health-cmd "pg_isready -U postgres"
          --health-interval 5s
          --health-timeout 5s
          --health-retries 10
    env:
      DATABASE_URL: postgres://postgres:postgres@localhost:5432/app_test
    steps:
      - uses: actions/checkout@v4
      - uses: actions/setup-node@v4
        with: { node-version: 20, cache: npm }
      - run: npm ci
      - run: npm run test:integration
```

| Point | Detail |
|-------|--------|
| Health check | The job waits until the container reports healthy before steps start |
| Host name | `localhost` when the job runs on the runner, the **service name** (`postgres`) when the job itself runs in a `container:` |
| Cleanup | Containers are removed when the job finishes |
| Typical services | PostgreSQL, MySQL, Redis, MongoDB, Elasticsearch |

## The same pattern in other ecosystems

| Ecosystem | Setup action | Install | Test |
|-----------|--------------|---------|------|
| Node.js | `actions/setup-node` with `cache: npm` | `npm ci` | `npm test` |
| Python | `actions/setup-python` with `cache: pip` | `pip install -r requirements.txt` | `pytest` |
| Java (Maven) | `actions/setup-java` with `cache: maven` | built into `mvn` | `mvn -B verify` |
| Go | `actions/setup-go` (cache on by default) | `go mod download` | `go test ./...` |
| .NET | `actions/setup-dotnet` | `dotnet restore` | `dotnet test` |
| Rust | `dtolnay/rust-toolchain` (third-party) plus cache | `cargo fetch` | `cargo test` |

The skeleton is always: checkout, setup with cache, install from lockfile, lint, test, build.

## Reporting results

### Job summary

```yaml
- name: Publish test summary
  if: always()
  run: |
    echo "## Test results" >> "$GITHUB_STEP_SUMMARY"
    echo "" >> "$GITHUB_STEP_SUMMARY"
    echo "- Node: ${{ matrix.node }}" >> "$GITHUB_STEP_SUMMARY"
    echo "- Commit: \`${GITHUB_SHA::7}\`" >> "$GITHUB_STEP_SUMMARY"
```

### Annotations on the code

Many linters and test runners can emit annotations that show up directly in the pull request's **Files changed** view. You can also create them with workflow commands (chapter 18):

```bash
echo "::error file=src/app.js,line=12,title=Lint::Unused variable"
```

### Artifacts

Upload coverage reports, screenshots from UI tests, and logs with `if: always()` so you can inspect failures.

## Making CI mandatory: branch protection

CI only protects your code if merging requires it to pass.

**Settings → Branches → Branch protection rules** (or **Rulesets**) → require status checks to pass before merging, and select your jobs.

### The "all green" gate job

Matrix jobs have many check names, and path filters can leave jobs skipped. A single final job gives branch protection one stable name to require.

```yaml
jobs:
  lint:  { ... }
  test:  { ... }
  build: { ... }

  ci-ok:
    if: always()
    needs: [lint, test, build]
    runs-on: ubuntu-latest
    steps:
      - name: Fail if any required job failed or was cancelled
        if: contains(needs.*.result, 'failure') || contains(needs.*.result, 'cancelled')
        run: exit 1
```

Mark only `ci-ok` as the required check. If any needed job fails or is cancelled, `ci-ok` fails; skipped jobs do not count as failures.

## Keeping CI fast

| Technique | Effect |
|-----------|--------|
| Cache dependencies (`cache:` in setup actions) | Skips most of the install time |
| Run lint and tests as parallel jobs | Shorter wall-clock time |
| `concurrency` with `cancel-in-progress` on PRs | No wasted runs on stale commits |
| `paths-ignore` for docs-only changes | Skips pointless runs (watch required checks) |
| Matrix only what you support | Fewer jobs |
| `fetch-depth: 1` (default) | Fast checkout |
| Split slow tests into shards | Parallelism for large suites |
| Timeouts on every job | Hung jobs do not burn minutes |

## Handling flaky tests

| Practice | Why |
|----------|-----|
| Fix them first | Retries hide real problems |
| Quarantine known flaky tests behind a separate job | The main pipeline stays trustworthy |
| Retry **only at the test-runner level** for specific tests | Narrow, visible, and countable |
| Never auto-retry the whole job silently | Builds trust in red results |

## Mental model checklist

- CI answers one question: **"Is this change safe to merge?"**
- Fast failures first; slow checks later
- Reproducible installs (lockfiles, `npm ci`) and cached downloads
- Required status checks make CI meaningful
- A gate job gives branch protection one stable name

## Common questions

| Question | Answer |
|----------|--------|
| Should CI run on both `push` and `pull_request`? | Yes, but restrict `push` to `main` to avoid duplicates |
| One workflow file or several? | One per concern is common: `ci.yml`, `release.yml`, `deploy.yml` |
| How do I run only on changed files? | Path filters, or a planning job that emits a dynamic matrix |
| How long should CI take? | Aim for under 10 minutes for the main feedback path |
| Do I need a build step in CI for interpreted languages? | Run the equivalent checks (type checks, bundling, packaging) to prove releases will work |
| Can CI auto-fix formatting? | Possible, but pushing commits with `GITHUB_TOKEN` will not trigger new runs; many teams just fail and ask for a fix |
| How do I show test failures on the PR? | Annotations, job summaries, or a test reporter action |
| Where do secrets for tests come from? | Avoid real secrets; use fakes and service containers. Fork PRs do not receive secrets |

## Pitfalls

| Pitfall | Why it hurts | Better |
|---------|--------------|--------|
| `npm install` instead of `npm ci` | Non-reproducible installs, lockfile drift | `npm ci` |
| Tests that need real production secrets | Fork PRs fail, leaks risk | Use mocks and service containers |
| No timeouts | A hung test runs for six hours | `timeout-minutes` on jobs |
| Required check name changes with the matrix | Branch protection waits forever | Use a gate job |
| `paths` filters on a required check | PR stays "pending" when the workflow is skipped | Gate job or no filter |
| Ignoring warnings that become errors later | Surprise breakage on dependency updates | Keep lint at zero warnings |
| Caching `node_modules` | `npm ci` deletes it | Cache `~/.npm` via `cache: npm` |
| Running everything on macOS and Windows | Expensive | Linux for most checks, others only where needed |

## Try it

1. Take the full Node.js pipeline above and run it on a test repository with a trivial `npm test`
2. Break a test on purpose; confirm `build` does not run and the coverage artifact still uploads
3. Add the `ci-ok` gate job and make it the only required status check in a branch protection rule

## Key takeaways

- CI automatically verifies every change: install, lint, build, test, report
- Order cheap checks first and use caching, matrices, and `concurrency` to stay fast
- Services such as databases run as containers beside the job
- Make CI binding with branch protection, using a single gate job for stable naming
- Use `npm ci` and lockfiles for reproducibility

**Next:** [CD and Deployment](./15_cd-and-deployment.md)
