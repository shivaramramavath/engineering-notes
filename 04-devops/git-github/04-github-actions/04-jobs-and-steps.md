# Jobs and Steps

A **job** is a set of steps that run on the same runner. A **step** is one individual task inside a job: either a shell command (`run`) or a reusable action (`uses`).

```
workflow
 ├── job A (runner 1)
 │    ├── step 1
 │    ├── step 2
 │    └── step 3
 └── job B (runner 2)
      └── step 1
```

## The simplest job

```yaml
jobs:
  build:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v4
      - run: npm ci
      - run: npm test
```

Three steps, one machine, run top to bottom. If any step fails, the remaining steps are skipped and the job fails.

## Jobs run in parallel by default

```yaml
jobs:
  lint:
    runs-on: ubuntu-latest
    steps: [...]
  test:
    runs-on: ubuntu-latest
    steps: [...]
  docs:
    runs-on: ubuntu-latest
    steps: [...]
```

```
lint ─┐
test ─┼─► all start together
docs ─┘
```

Parallel jobs finish sooner overall, but each one pays its own setup cost (checkout, install).

## Ordering jobs with `needs`

```yaml
jobs:
  lint:
    runs-on: ubuntu-latest
    steps: [...]

  test:
    runs-on: ubuntu-latest
    steps: [...]

  deploy:
    needs: [lint, test]          # waits for both
    runs-on: ubuntu-latest
    steps: [...]
```

```
lint ─┐
      ├──► deploy
test ─┘
```

| Behavior | Detail |
|----------|--------|
| `needs: build` | Wait for one job |
| `needs: [a, b]` | Wait for all listed jobs |
| A needed job fails | Dependent job is **skipped** by default |
| Run anyway | `if: ${{ always() }}` or `if: ${{ failure() }}` on the dependent job |
| Circular dependencies | Rejected as an error |

## Jobs are isolated

Each job gets a **brand-new runner**. Anything created in one job is gone in the next.

```yaml
jobs:
  build:
    runs-on: ubuntu-latest
    steps:
      - run: echo "hello" > file.txt

  check:
    needs: build
    runs-on: ubuntu-latest
    steps:
      - run: cat file.txt        # ✗ No such file
```

To pass data between jobs, use one of:

| Need | Use |
|------|-----|
| A small value (version, flag) | Job **outputs** (below) |
| Files (build output, reports) | **Artifacts** (chapter 09) |
| Rebuildable data (dependencies) | **Cache** (chapter 09) |

## Passing values with job outputs

```yaml
jobs:
  build:
    runs-on: ubuntu-latest
    outputs:
      version: ${{ steps.ver.outputs.value }}
    steps:
      - id: ver
        run: echo "value=1.4.2" >> "$GITHUB_OUTPUT"

  deploy:
    needs: build
    runs-on: ubuntu-latest
    steps:
      - run: echo "Deploying ${{ needs.build.outputs.version }}"
```

Flow: a step writes to `$GITHUB_OUTPUT`, the job's `outputs:` maps it, and downstream jobs read `needs.<job>.outputs.<name>`.

## Steps: `run` vs `uses`

```yaml
steps:
  - name: Check out the code
    uses: actions/checkout@v4

  - name: Set up Node
    uses: actions/setup-node@v4
    with:
      node-version: 20
      cache: npm

  - name: Run tests
    run: npm test
```

| | `run` | `uses` |
|---|-------|--------|
| What it is | A shell command | A packaged action |
| Inputs | Environment variables | `with:` map |
| Good for | Project-specific commands | Common, reusable tasks |
| Example | `run: make build` | `uses: actions/checkout@v4` |

## Multi-line commands and shells

```yaml
- name: Build and package
  shell: bash
  working-directory: ./app
  run: |
    npm ci
    npm run build
    tar -czf dist.tgz dist/
```

| Key | Effect |
|-----|--------|
| `shell` | Choose `bash`, `sh`, `pwsh`, `python`, and so on |
| `working-directory` | Run the command in that folder |
| `|` after `run:` | Multi-line script, executed in order |

## Each `run` step is a separate process

```yaml
steps:
  - run: cd app                # affects only this step
  - run: pwd                   # back at the workspace root
  - run: export TOKEN=abc      # lost when the step ends
  - run: echo "$TOKEN"         # empty
```

What **does** persist between steps in the same job:

| Persists | Does not persist |
|----------|------------------|
| Files in the workspace | `cd` changes |
| Installed tools | `export`ed variables |
| Anything written to `$GITHUB_ENV`, `$GITHUB_PATH`, `$GITHUB_OUTPUT` | Shell functions and aliases |

## Sharing values between steps

```yaml
steps:
  - id: meta
    run: |
      echo "sha_short=$(git rev-parse --short HEAD)" >> "$GITHUB_OUTPUT"

  - name: Use the step output
    run: echo "Commit ${{ steps.meta.outputs.sha_short }}"

  - name: Set an environment variable for later steps
    run: echo "BUILD_MODE=release" >> "$GITHUB_ENV"

  - name: Read it
    run: echo "Mode is $BUILD_MODE"

  - name: Add a folder to PATH
    run: echo "$HOME/.local/bin" >> "$GITHUB_PATH"
```

| File | Purpose | Read it as |
|------|---------|------------|
| `$GITHUB_OUTPUT` | Step outputs | `steps.<id>.outputs.<name>` |
| `$GITHUB_ENV` | Environment variables for later steps | `$NAME` or `env.NAME` |
| `$GITHUB_PATH` | Extra `PATH` entries for later steps | Available automatically |

## Conditions on steps and jobs

```yaml
steps:
  - run: npm test

  - name: Upload logs on failure
    if: failure()
    run: ./collect-logs.sh

  - name: Always clean up
    if: always()
    run: ./cleanup.sh

  - name: Only on main
    if: github.ref == 'refs/heads/main'
    run: ./publish.sh
```

| Function | True when |
|----------|-----------|
| `success()` | All previous steps succeeded (default behavior) |
| `failure()` | Any previous step failed |
| `always()` | Regardless of result, even if cancelled |
| `cancelled()` | The run was cancelled |

More on expressions in chapter 08.

## Handling failure and time

```yaml
jobs:
  test:
    runs-on: ubuntu-latest
    timeout-minutes: 10               # whole job
    steps:
      - run: npm test
        timeout-minutes: 5            # single step

      - name: Optional linter
        run: npm run lint:experimental
        continue-on-error: true       # failure does not stop the job
```

| Key | Default | Note |
|-----|---------|------|
| Job `timeout-minutes` | 360 | Always set a lower value so hung jobs do not burn minutes |
| Step `timeout-minutes` | None | Limited only by the job timeout |
| `continue-on-error` | `false` | Step or job is marked failed-but-ignored |

## Service containers

A job can start helper containers, such as a database, next to the runner.

```yaml
jobs:
  test:
    runs-on: ubuntu-latest
    services:
      postgres:
        image: postgres:16
        env:
          POSTGRES_PASSWORD: example
        ports: ["5432:5432"]
        options: >-
          --health-cmd "pg_isready"
          --health-interval 5s
          --health-retries 5
    steps:
      - uses: actions/checkout@v4
      - run: npm test
        env:
          DATABASE_URL: postgres://postgres:example@localhost:5432/postgres
```

A full walkthrough is in chapter 14.

## Putting it together

```yaml
name: Build and deploy

on:
  push:
    branches: [main]

jobs:
  test:
    runs-on: ubuntu-latest
    timeout-minutes: 10
    steps:
      - uses: actions/checkout@v4
      - uses: actions/setup-node@v4
        with:
          node-version: 20
          cache: npm
      - run: npm ci
      - run: npm test

  build:
    needs: test
    runs-on: ubuntu-latest
    outputs:
      version: ${{ steps.ver.outputs.value }}
    steps:
      - uses: actions/checkout@v4
      - id: ver
        run: echo "value=$(node -p "require('./package.json').version")" >> "$GITHUB_OUTPUT"
      - run: npm ci && npm run build

  deploy:
    needs: build
    runs-on: ubuntu-latest
    steps:
      - run: echo "Deploying version ${{ needs.build.outputs.version }}"
```

## Mental model checklist

- Jobs are **parallel and isolated**; `needs` adds order, outputs and artifacts add data flow
- Steps are **sequential and share a filesystem**, but not shell state
- To pass something forward, write to `$GITHUB_OUTPUT`, `$GITHUB_ENV`, or `$GITHUB_PATH`
- Failure stops the job unless a step says `continue-on-error` or an `if:` overrides it
- Always set a `timeout-minutes`

## Common questions

| Question | Answer |
|----------|--------|
| How many jobs can a workflow have? | Many; GitHub applies limits per plan, so check the current documentation |
| Can jobs run on different operating systems? | Yes, each job has its own `runs-on` |
| Do I need `checkout` in every job? | Yes, each job starts with an empty workspace |
| Can a step use another step's output? | Yes, via `steps.<id>.outputs.<name>`; the earlier step needs an `id` |
| What happens to dependent jobs if one fails? | They are skipped unless you use `always()` or `failure()` |
| Can I run steps in parallel? | Not directly; split them into jobs, or background processes inside one `run` |
| Is `needs` required for sequencing? | Yes, otherwise jobs start simultaneously |

## Pitfalls

| Pitfall | Why it hurts | Better |
|---------|--------------|--------|
| Expecting files from a previous job | Each job is a new machine | Upload and download artifacts |
| `cd` in one step, then assuming the next step is there | Steps are separate processes | Use `working-directory` |
| `export VAR=...` for later steps | Lost at the end of the step | Append to `$GITHUB_ENV` |
| Missing `id:` on a step whose output you need | `steps.<id>` has nothing to refer to | Add `id:` |
| No `timeout-minutes` | A hung job can run for six hours | Set a sensible limit |
| Forgetting `actions/checkout` | No source code in the workspace | First step in nearly every job |
| `if: failure()` on a job whose `needs` was skipped | Condition never evaluates as expected | Combine with `always()` and check `needs.<job>.result` |
| Using the deprecated `::set-output` command | Removed behavior and warnings | Write to `$GITHUB_OUTPUT` |

## Try it

1. Create three jobs: `lint`, `test`, and `build`, where `build` needs both of the others
2. In `build`, write a step output named `version`, expose it as a job output, and print it from a fourth job `report`
3. Add a step that always runs and prints "cleanup", then break a test on purpose and confirm the cleanup step still runs

## Key takeaways

- A job is a group of steps on one runner; jobs run in parallel unless ordered with `needs`
- Jobs do not share files, so pass values through outputs and files through artifacts
- A step is `run` (command) or `uses` (action); each `run` is its own shell process
- Use `$GITHUB_OUTPUT`, `$GITHUB_ENV`, and `$GITHUB_PATH` to share data between steps
- Control flow with `if`, `continue-on-error`, and `timeout-minutes`

**Next:** [Actions and Marketplace](./05_actions-and-marketplace.md)
