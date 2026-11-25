# Matrix and Concurrency

Two features control **how many jobs run**. A **matrix** generates many jobs from one definition (for example, every combination of OS and Node version). **Concurrency** limits how many runs of a workflow or job can happen at once, so you can cancel outdated runs or stop two deployments from colliding.

```
matrix       →  one job definition  ×  many value combinations  =  many jobs
concurrency  →  many runs  →  only one active per group (others wait or cancel)
```

## The simplest matrix

```yaml
jobs:
  test:
    runs-on: ubuntu-latest
    strategy:
      matrix:
        node: [18, 20, 22]
    steps:
      - uses: actions/checkout@v4
      - uses: actions/setup-node@v4
        with:
          node-version: ${{ matrix.node }}
      - run: npm ci && npm test
```

Three jobs are created, one per value, running in parallel:

```
test (18)   test (20)   test (22)
```

## Multiple dimensions

```yaml
strategy:
  matrix:
    os: [ubuntu-latest, windows-latest]
    node: [18, 20]
runs-on: ${{ matrix.os }}
```

The matrix is the **cross product** of all lists: 2 × 2 = 4 jobs.

| `os` | `node` |
|------|--------|
| ubuntu-latest | 18 |
| ubuntu-latest | 20 |
| windows-latest | 18 |
| windows-latest | 20 |

Every combination appears in the UI as `test (ubuntu-latest, 18)` and so on.

## Reading matrix values

```yaml
steps:
  - run: echo "OS=${{ matrix.os }} Node=${{ matrix.node }}"
  - uses: actions/upload-artifact@v4
    with:
      name: results-${{ matrix.os }}-${{ matrix.node }}   # unique per combination
      path: reports/
```

Any key you invent under `matrix:` becomes `matrix.<key>`.

## `exclude`: remove combinations

```yaml
strategy:
  matrix:
    os: [ubuntu-latest, windows-latest, macos-latest]
    node: [18, 20]
    exclude:
      - os: macos-latest
        node: 18             # skip this one pair
```

An `exclude` entry removes every combination that matches **all** of its keys.

## `include`: add combinations or extra values

```yaml
strategy:
  matrix:
    os: [ubuntu-latest, windows-latest]
    node: [20]
    include:
      - os: ubuntu-latest
        experimental: true           # adds a property to the existing ubuntu+20 job
      - os: macos-latest
        node: 22                     # no match, so this becomes a brand-new job
```

| Case | Result |
|------|--------|
| `include` entry matches an existing combination without changing its original values | Extra properties are **added** to that job |
| `include` entry does not match any combination | A **new** job is created |

Order of evaluation: matrix is expanded, then `exclude` is applied, then `include`.

### Matrix built only from `include`

```yaml
strategy:
  matrix:
    include:
      - name: linux
        os: ubuntu-latest
        cmd: make linux
      - name: windows
        os: windows-latest
        cmd: make windows
```

Useful when each job needs several values that go together instead of a cross product.

## `fail-fast` and `max-parallel`

```yaml
strategy:
  fail-fast: false        # default is true
  max-parallel: 2         # at most two jobs at the same time
  matrix:
    node: [18, 20, 22]
```

| Key | Default | Effect |
|-----|---------|--------|
| `fail-fast` | `true` | When one matrix job fails, GitHub **cancels** the others still running or queued |
| `max-parallel` | As many as the runners allow | Cap on simultaneous matrix jobs |

Set `fail-fast: false` when you want to see **every** failing combination in one run.

## Allowing some combinations to fail

```yaml
jobs:
  test:
    runs-on: ubuntu-latest
    continue-on-error: ${{ matrix.experimental == true }}
    strategy:
      fail-fast: false
      matrix:
        node: [20]
        experimental: [false]
        include:
          - node: 23
            experimental: true
```

The Node 23 job can fail without turning the whole workflow red.

## Dynamic matrix from a previous job

```yaml
jobs:
  plan:
    runs-on: ubuntu-latest
    outputs:
      matrix: ${{ steps.set.outputs.matrix }}
    steps:
      - id: set
        run: echo 'matrix={"service":["api","web","worker"]}' >> "$GITHUB_OUTPUT"

  build:
    needs: plan
    runs-on: ubuntu-latest
    strategy:
      matrix: ${{ fromJSON(needs.plan.outputs.matrix) }}
    steps:
      - run: echo "Building ${{ matrix.service }}"
```

The `plan` job can compute the list from changed folders, a config file, or an API call, which is how monorepos build only what changed.

## Matrix limits and cost

| Fact | Detail |
|------|--------|
| Maximum jobs from one matrix | 256 per workflow run |
| Each combination | Is a full job with its own runner, checkout, and setup |
| Billing | Each job consumes minutes; matrices multiply cost quickly |
| Required status checks | Each matrix job reports as its **own** check, with a name that includes the values |

A 3 × 3 × 3 matrix is 27 jobs. Keep matrices as small as your support policy requires, and put rare combinations in `include` instead of the main cross product.

## Concurrency

By default, GitHub runs as many workflow runs as it has capacity for, even for the same branch. **Concurrency** groups put a limit on that.

### The simplest concurrency block

```yaml
concurrency:
  group: deploy-production
  cancel-in-progress: false
```

Only **one** run in the group `deploy-production` is active at a time. Others wait.

### What happens with a group

| Situation | `cancel-in-progress: false` | `cancel-in-progress: true` |
|-----------|-----------------------------|----------------------------|
| A run is active, a new one starts | New run is **queued** (pending) | Active run is **cancelled**, new run starts |
| A run is active, one is pending, a third arrives | The older pending run is cancelled, the third becomes pending | Same as above, plus the active run is cancelled |

At most **one running and one pending** per group, so the queue is not a long line; only the newest waiting run survives.

### Cancel outdated runs on the same branch (typical for CI)

```yaml
concurrency:
  group: ci-${{ github.workflow }}-${{ github.ref }}
  cancel-in-progress: true
```

Push three commits quickly to a branch and only the last one finishes, saving minutes.

### Keep deployments safe (typical for CD)

```yaml
concurrency:
  group: deploy-${{ inputs.environment || 'production' }}
  cancel-in-progress: false      # never kill a deployment halfway through
```

Two deployments to the same target never overlap, and none is cancelled midway.

### Cancel only on pull requests

```yaml
concurrency:
  group: ci-${{ github.workflow }}-${{ github.head_ref || github.run_id }}
  cancel-in-progress: ${{ github.event_name == 'pull_request' }}
```

- On PRs: `head_ref` is the PR branch, so newer pushes cancel older runs
- On other events (`push` to `main`): `head_ref` is empty and `run_id` is unique, so every run has its own group and nothing is cancelled

### Concurrency at job level

```yaml
jobs:
  deploy:
    runs-on: ubuntu-latest
    concurrency:
      group: deploy-${{ github.ref }}
      cancel-in-progress: false
    steps: [...]
```

Use job-level concurrency when only part of the workflow needs serializing, and the earlier jobs (tests) should still run freely.

### Group names

| Fact | Detail |
|------|--------|
| Any string | Can include expressions |
| Case-insensitive | `Deploy` and `deploy` are the same group |
| Shared across workflows | Two workflows using the same group name share one queue |
| Include the workflow name | Avoid accidental collisions between unrelated workflows |
| Context availability | Workflow level can use `github`, `inputs`, and `vars`; job level can also use `needs`, `strategy`, and `matrix` |

## Matrix plus concurrency

Concurrency groups apply per workflow run or per job, so in a matrix every job shares the group name unless you include a matrix value.

```yaml
jobs:
  deploy:
    strategy:
      matrix:
        region: [eu, us]
    concurrency:
      group: deploy-${{ matrix.region }}      # one lane per region
      cancel-in-progress: false
```

## Mental model checklist

- Matrix: **values in, jobs out**; each combination is an independent job
- `include` adds, `exclude` removes, `fail-fast` decides whether siblings are cancelled
- Concurrency: **one active run per group**; choose cancel (CI) or queue (CD)
- Group names must identify exactly what must not overlap
- Never cancel a deployment in progress

## Common questions

| Question | Answer |
|----------|--------|
| How do I refer to the current matrix values in a job name? | `name: Test on ${{ matrix.os }}` |
| Can matrix values be objects? | Yes, matrix values can be objects and you access them with `matrix.config.field` |
| Why did my matrix jobs stop when one failed? | `fail-fast` defaults to `true` |
| How do I limit parallel jobs to save runners? | `max-parallel` |
| Can I use a matrix on a reusable workflow call? | Yes, `strategy` works on a job that calls a reusable workflow |
| Does concurrency queue unlimited runs? | No, one pending run per group; newer pending runs replace older ones |
| Does cancelled run count as failure? | It is reported as cancelled, not failed |
| Can I use concurrency to rate-limit across repositories? | No, groups are scoped to the repository |

## Pitfalls

| Pitfall | Why it hurts | Better |
|---------|--------------|--------|
| Huge cross product | Slow, expensive, may hit the 256-job limit | Trim; use `include` for special cases |
| Number versions like `3.10` unquoted | Parsed as `3.1` | `["3.9", "3.10", "3.12"]` |
| Same artifact name from every matrix job | Upload conflict | Add matrix values to the name |
| `cancel-in-progress: true` on deployments | A deploy can be killed midway, leaving a broken state | Use `false` for deploys |
| `cancel-in-progress: true` on `main` pushes | Skips runs for commits you may want tested individually | Cancel only on PRs |
| Group name identical for unrelated workflows | They block each other | Prefix with `github.workflow` |
| Required checks and matrix renames | Branch protection expects exact check names | Keep matrix values stable, or require a final "all tests" job |
| `fail-fast` left on for a compatibility matrix | You see only the first failure | `fail-fast: false` |

## Try it

1. Create a matrix of two operating systems and two Node versions; print the four combinations
2. Add an `exclude` for one pair and an `include` that adds `experimental: true` for one combination; compare the job list
3. Add the PR-only cancellation block above, push twice quickly to a PR, and watch the first run cancel

## Key takeaways

- A matrix turns lists of values into parallel jobs via `matrix.<key>`
- `include` and `exclude` shape the combinations; `fail-fast` and `max-parallel` control behavior
- Use `fromJSON` for dynamic matrices
- `concurrency.group` allows one active run per group; `cancel-in-progress` chooses cancel or queue
- Cancel stale CI runs, but never cancel deployments midway

**Next:** [Permissions and Environments](./11_permissions-and-environments.md)
