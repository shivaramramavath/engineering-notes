# Reusable Workflows

A **reusable workflow** is a complete workflow that other workflows can **call** like a function. It declares inputs, secrets, and outputs, and a caller invokes it from a **job**. Use it to share the same build, test, or deploy logic across many repositories without copy-pasting YAML.

```
caller workflow  ──uses──►  reusable workflow
   (job with uses:)         (on: workflow_call, with its own jobs and steps)
```

## The simplest reusable workflow

**The reusable workflow** (`.github/workflows/test.yml`):

```yaml
name: Reusable test

on:
  workflow_call:             # makes it callable

jobs:
  test:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v4
      - run: npm ci && npm test
```

**The caller** (`.github/workflows/ci.yml`):

```yaml
name: CI

on: [push, pull_request]

jobs:
  run-tests:
    uses: ./.github/workflows/test.yml      # call at the job level
```

The `run-tests` job in the caller expands into the jobs defined in `test.yml`.

## Passing inputs

**Reusable:**

```yaml
on:
  workflow_call:
    inputs:
      node-version:
        description: "Node.js version"
        type: string
        required: false
        default: "20"
      run-lint:
        type: boolean
        default: true

jobs:
  test:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v4
      - uses: actions/setup-node@v4
        with:
          node-version: ${{ inputs.node-version }}
      - run: npm ci
      - if: inputs.run-lint
        run: npm run lint
      - run: npm test
```

**Caller:**

```yaml
jobs:
  run-tests:
    uses: ./.github/workflows/test.yml
    with:
      node-version: "22"
      run-lint: false
```

| Input type | Values |
|------------|--------|
| `string` | Text |
| `number` | Numbers |
| `boolean` | `true` or `false` |

Inputs are read as `${{ inputs.<name> }}`. Required inputs without a default must be provided by the caller.

## Passing secrets

Secrets are **not** passed automatically. The reusable workflow declares them, and the caller supplies them.

**Reusable:**

```yaml
on:
  workflow_call:
    secrets:
      deploy-token:
        required: true

jobs:
  deploy:
    runs-on: ubuntu-latest
    steps:
      - run: ./deploy.sh
        env:
          TOKEN: ${{ secrets.deploy-token }}
```

**Caller:**

```yaml
jobs:
  release:
    uses: ./.github/workflows/deploy.yml
    secrets:
      deploy-token: ${{ secrets.PROD_DEPLOY_TOKEN }}     # map one by one
    # or:
    # secrets: inherit                                   # pass everything the caller can see
```

| Option | Use when |
|--------|----------|
| Explicit mapping | Default choice; least privilege |
| `secrets: inherit` | Caller and called workflow are in the same organization or enterprise and you trust the called workflow with all secrets |

## Returning outputs

A reusable workflow can hand values back to the caller by mapping job outputs to workflow outputs.

**Reusable:**

```yaml
on:
  workflow_call:
    outputs:
      version:
        description: "Version that was built"
        value: ${{ jobs.build.outputs.version }}

jobs:
  build:
    runs-on: ubuntu-latest
    outputs:
      version: ${{ steps.ver.outputs.value }}
    steps:
      - id: ver
        run: echo "value=1.4.2" >> "$GITHUB_OUTPUT"
```

**Caller:**

```yaml
jobs:
  build:
    uses: ./.github/workflows/build.yml

  deploy:
    needs: build
    runs-on: ubuntu-latest
    steps:
      - run: echo "Deploying ${{ needs.build.outputs.version }}"
```

The chain is: step output → job output → workflow output → `needs.<caller-job>.outputs.<name>`.

## Where the reusable workflow can live

| Reference | Meaning |
|-----------|---------|
| `./.github/workflows/test.yml` | Same repository, same commit as the caller |
| `my-org/shared/.github/workflows/test.yml@v1` | Another repository, at tag `v1` |
| `my-org/shared/.github/workflows/test.yml@main` | Another repository, at a branch |
| `my-org/shared/.github/workflows/test.yml@<sha>` | Another repository, pinned to a commit |

Rules:

- The file must be inside `.github/workflows/` in the repository that holds it
- Public repositories can be called by anyone; private ones need **Settings → Actions → General → Access** configured to allow other repositories to use them
- Version external references with a tag or SHA, never a moving branch for anything important
- The `uses:` value must be a literal; no expressions

## A caller with a matrix

```yaml
jobs:
  test:
    strategy:
      matrix:
        node: ["18", "20", "22"]
    uses: ./.github/workflows/test.yml
    with:
      node-version: ${{ matrix.node }}
```

Each matrix entry becomes one call of the reusable workflow.

## What a calling job can and cannot have

A job that uses `uses:` is **only a call**.

| Allowed | Not allowed |
|---------|-------------|
| `uses`, `with`, `secrets` | `runs-on`, `steps`, `container`, `services` |
| `needs`, `if` | `env` on the job |
| `strategy` (matrix) | |
| `permissions`, `concurrency` | |

The jobs inside the called workflow have their own `runs-on` and `steps`.

## Permissions and context

| Topic | Behavior |
|-------|----------|
| `GITHUB_TOKEN` permissions | The called workflow can keep or **reduce** the caller's token permissions, never increase them |
| `github` context | Belongs to the **caller**, so `github.repository` and `github.ref` refer to the calling repository and ref |
| `env` from the caller | **Not** passed through; use `inputs` instead |
| `vars` | Resolved from the caller's repository, organization, and environment variables |
| Secrets | Only those you pass or `inherit` |

```yaml
# Caller grants what the called workflow needs
jobs:
  release:
    permissions:
      contents: write
      packages: write
    uses: ./.github/workflows/release.yml
```

## Nesting

Reusable workflows can call other reusable workflows, up to a limited depth (four levels at the time of writing). Loops are not allowed. Keep nesting shallow; deep chains are hard to debug.

## A realistic shared pipeline

**`my-org/.github` repository, `.github/workflows/node-ci.yml`:**

```yaml
name: Node CI (shared)

on:
  workflow_call:
    inputs:
      node-version:
        type: string
        default: "20"
      working-directory:
        type: string
        default: "."

permissions:
  contents: read

jobs:
  ci:
    runs-on: ubuntu-latest
    defaults:
      run:
        working-directory: ${{ inputs.working-directory }}
    steps:
      - uses: actions/checkout@v4
      - uses: actions/setup-node@v4
        with:
          node-version: ${{ inputs.node-version }}
          cache: npm
          cache-dependency-path: ${{ inputs.working-directory }}/package-lock.json
      - run: npm ci
      - run: npm run lint --if-present
      - run: npm test
```

**Any project repository, `.github/workflows/ci.yml`:**

```yaml
name: CI

on:
  pull_request:
  push:
    branches: [main]

jobs:
  ci:
    uses: my-org/.github/.github/workflows/node-ci.yml@v1
    with:
      node-version: "22"
```

Eleven lines in the project, one place to improve for every repository.

## Reusable workflows vs composite actions

| | Reusable workflow | Composite action (chapter 13) |
|---|-------------------|-------------------------------|
| Unit of reuse | A whole set of **jobs** | A sequence of **steps** |
| Called with | `jobs.<id>.uses` | `steps[*].uses` |
| Runner choice | Defined inside (own `runs-on`) | Uses the caller job's runner |
| Can contain multiple jobs | **Yes** | No |
| Can use `secrets` | Yes, declared and passed | Pass as inputs |
| Appears in logs as | Separate jobs | Steps within the caller's job |
| Shares the caller's filesystem | No, separate jobs | **Yes**, same job |
| Best for | Standard pipelines, deployment flows | Reusable setup and utility steps |

Rule of thumb: reuse a **pipeline** with a reusable workflow, reuse a **recipe** with a composite action.

## Mental model checklist

- Called workflows declare `on: workflow_call`
- The caller invokes with `jobs.<id>.uses`, not in steps
- Data flows in through `inputs` and `secrets`, and out through `outputs`
- The `github` context is the caller's; `env` does not cross the boundary
- Permissions can only narrow, never widen
- Pin shared workflows to a tag or SHA

## Common questions

| Question | Answer |
|----------|--------|
| Can a workflow have `workflow_call` and other triggers? | Yes, add `push`, `workflow_dispatch`, and so on beside it |
| Can I call a workflow from a step? | No, reusable workflows are called at job level |
| Why can't I read `env.X` from the caller? | `env` is not propagated; pass it as an input |
| Do called workflows run as separate runs? | No, they are part of the caller's run, shown as nested jobs |
| How do I test a reusable workflow? | Add a small caller workflow in the same repository that uses `./.github/workflows/...` |
| Can I override `runs-on` from the caller? | Only if the reusable workflow takes it as an input and uses it |
| Can the reusable workflow access the caller's repository code? | It must run `actions/checkout`, which checks out the **caller's** repository by default |
| Is there a limit to the number of reusable workflows? | Yes, limits exist per workflow file; see the current documentation |

## Pitfalls

| Pitfall | Why it hurts | Better |
|---------|--------------|--------|
| Calling a moving branch (`@main`) of a shared workflow | A change upstream can break or compromise every consumer | Tag releases (`@v1`) or pin a SHA |
| Forgetting to pass secrets | The called workflow sees empty values | Map them or use `secrets: inherit` |
| `secrets: inherit` across trust boundaries | Gives the called workflow every secret | Use explicit mapping |
| Expecting caller `env:` to carry over | It does not | Pass values via `inputs` |
| Giving a reusable workflow more permissions than the caller | Not possible; the job fails at runtime | Grant permissions in the caller job |
| Putting `steps:` in the calling job | Invalid | Calling jobs only have `uses`, `with`, `secrets`, and the allowed keys |
| Deep nesting | Hard to trace failures | Keep calls shallow |
| Workflow file outside `.github/workflows/` | Cannot be called | Place it correctly |

## Try it

1. Create `build.yml` with `on: workflow_call`, one string input, and one output
2. Create a caller workflow that calls it with a matrix of two values and prints the output from a later job
3. Move the reusable file to a second repository, reference it with `@main`, then switch the reference to a tag

## Key takeaways

- A reusable workflow is a callable set of jobs declared with `on: workflow_call`
- Call it with `jobs.<id>.uses`, passing `with:` inputs and `secrets:`
- Outputs return through workflow-level `outputs`, then `needs.<job>.outputs`
- Permissions can only narrow, and `env` is not inherited
- Use reusable workflows for pipelines and composite actions for step sequences

**Next:** [Custom Actions](./13_custom-actions.md)
