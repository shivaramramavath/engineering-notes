# Workflow Syntax

A workflow file is a **YAML document** with a fixed set of top-level keys. Knowing those keys, and the keys allowed inside `jobs` and `steps`, is most of what there is to know about the syntax.

```
name / run-name   →  labels
on                →  when it runs
permissions       →  what the token may do
env / defaults    →  shared settings
concurrency       →  run-overlap control
jobs              →  the actual work
```

## The anatomy of a workflow

```yaml
name: CI                              # shown in the Actions tab
run-name: CI for ${{ github.ref_name }} by @${{ github.actor }}

on:                                   # required
  push:
    branches: [main]
  pull_request:

permissions:                          # limit GITHUB_TOKEN
  contents: read

env:                                  # available to every job and step
  NODE_ENV: test

defaults:
  run:
    shell: bash
    working-directory: ./app

concurrency:
  group: ci-${{ github.ref }}
  cancel-in-progress: true

jobs:                                 # required
  build:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v4
      - run: npm ci
      - run: npm test
```

## Top-level keys

| Key | Required | Purpose |
|-----|----------|---------|
| `name` | No | Display name; defaults to the file path |
| `run-name` | No | Name of each individual run; can use expressions |
| `on` | **Yes** | Events that trigger the workflow (chapter 03) |
| `permissions` | No | Default `GITHUB_TOKEN` permissions (chapter 11) |
| `env` | No | Environment variables for the whole workflow (chapter 07) |
| `defaults` | No | Default `shell` and `working-directory` for `run` steps |
| `concurrency` | No | Prevent or cancel overlapping runs (chapter 10) |
| `jobs` | **Yes** | One or more jobs |

## Job keys

```yaml
jobs:
  build:                              # job id
    name: Build and test              # display name
    runs-on: ubuntu-latest
    needs: [lint]
    if: github.event_name == 'push'
    timeout-minutes: 15
    env:
      CI: true
    outputs:
      version: ${{ steps.v.outputs.value }}
    steps: [...]
```

| Key | Purpose |
|-----|---------|
| `runs-on` | **Required.** Runner label or labels |
| `name` | Display name in the UI |
| `needs` | Jobs that must finish first |
| `if` | Skip the job unless the condition is true |
| `timeout-minutes` | Cancel the job after this long (default is 360) |
| `env`, `defaults`, `permissions`, `concurrency` | Same as top level, scoped to this job |
| `outputs` | Values exposed to later jobs |
| `strategy` | Matrix and fail-fast settings (chapter 10) |
| `environment` | Deployment environment and its rules (chapter 11) |
| `container`, `services` | Run in a container, or start sidecars such as a database |
| `continue-on-error` | Do not fail the workflow if this job fails |
| `steps` | **Required.** The work |

Job ids must start with a letter or `_` and contain only letters, numbers, `-` and `_`.

## Step keys

```yaml
steps:
  - name: Install dependencies
    id: install
    uses: actions/setup-node@v4
    with:
      node-version: 20
    env:
      FOO: bar
    if: success()
    continue-on-error: false
    timeout-minutes: 5
```

| Key | Purpose |
|-----|---------|
| `name` | Label in the logs |
| `id` | Reference this step from later steps (`steps.<id>.outputs...`) |
| `uses` | Run an action |
| `run` | Run a shell command |
| `with` | Inputs for an action |
| `env` | Environment variables for this step |
| `if` | Conditional execution |
| `shell` | `bash`, `pwsh`, `python`, `sh`, `cmd`, `powershell` |
| `working-directory` | Directory for a `run` step |
| `continue-on-error` | Keep going if this step fails |
| `timeout-minutes` | Limit for this step |

Every step has **either** `uses` **or** `run`, never both.

## YAML refresher

```yaml
# Scalars
name: CI
count: 3
enabled: true

# Lists (two forms, same meaning)
branches: [main, dev]
branches:
  - main
  - dev

# Maps
with:
  node-version: 20
  cache: npm

# Multi-line command (| keeps newlines)
- run: |
    npm ci
    npm test

# Folded text (> joins lines into one)
- run: >
    docker build
    --tag app:latest
    .
```

| Rule | Detail |
|------|--------|
| Indentation | Spaces only, never tabs; two spaces is the convention |
| Comments | `# text` |
| Strings | Quotes are optional, but use them when in doubt |
| Case | Keys are case-sensitive |
| Special characters | Quote any value that contains `:`, `#`, `{`, or starts with `*`, `!`, `%` |

## Expressions in syntax

Anything inside `${{ }}` is evaluated by GitHub **before** the step runs.

```yaml
- run: echo "Branch is ${{ github.ref_name }}"
- if: ${{ github.event_name == 'push' }}
  run: echo "pushed"
```

In an `if:` key the `${{ }}` wrapper is optional. Full coverage in chapter 08.

## Quote version numbers

```yaml
with:
  python-version: 3.10        # parsed as the number 3.1
  python-version: "3.10"      # correct
  node-version: 20            # fine, 20 has no trailing zero issue
  node-version: "20.x"        # also fine
```

## Multi-line `run` and shell behavior

```yaml
- name: Build
  shell: bash
  run: |
    set -euo pipefail          # stop on first error
    npm ci
    npm run build
```

- On Linux runners the default shell is `bash -e {0}`, which exits on the first failing command
- Explicit `shell: bash` also adds `-o pipefail`, so a failure inside a pipe is not hidden
- Each `run` step starts a **new** shell process

## Validating your YAML

| Tool | Use |
|------|-----|
| VS Code **GitHub Actions** extension | Schema validation and autocomplete as you type |
| `actionlint` | Command-line linter for workflows, catches expression and syntax errors |
| The Actions tab | Shows "Invalid workflow file" with a line number when parsing fails |

## Mental model checklist

- Two keys are required at the top: `on` and `jobs`
- Two keys are required for a job: `runs-on` and `steps`
- Each step is `uses` **or** `run`
- Indentation **is** the structure
- `${{ }}` is replaced with its value before the shell sees it

## Common questions

| Question | Answer |
|----------|--------|
| `.yml` or `.yaml`? | Both work; pick one and be consistent |
| Can I use YAML anchors to avoid repetition? | Supported in workflow files today, but reusable workflows and composite actions are the more common approach |
| Is `name` required? | No, GitHub falls back to the file path |
| Why does `on:` sometimes look like a boolean in editors? | Some YAML 1.1 tools read `on` as `true`; GitHub reads it correctly as a key |
| Can a workflow have zero jobs? | No, `jobs` is required |
| Can I split a long workflow across files? | Yes, using reusable workflows (chapter 12) |

## Pitfalls

| Pitfall | Why it hurts | Better |
|---------|--------------|--------|
| Tabs for indentation | YAML parse error | Spaces only; set your editor to insert spaces |
| Unquoted `3.10` | Becomes `3.1` | `"3.10"` |
| `uses` and `run` in the same step | Invalid workflow | Split into two steps |
| Colon inside an unquoted value | Parsed as a nested map | Quote the value |
| Wrong indentation under `with:` | Input silently ignored or parse error | Align keys under `with:` |
| `if: ${{ ... }} == 'x'` | Entire string treated as truthy text | Put the whole expression inside `${{ }}` or omit the wrapper |

## Try it

1. Build a workflow with `name`, `run-name`, `on: push`, one `env` variable, and one job with three steps
2. Deliberately break the indentation and push; read the error shown in the Actions tab
3. Install `actionlint` or the VS Code extension and confirm it catches the same error before you push

## Key takeaways

- Top-level keys: `name`, `run-name`, `on`, `permissions`, `env`, `defaults`, `concurrency`, `jobs`
- `on` and `jobs` are required; a job needs `runs-on` and `steps`
- Steps are `uses` (action) or `run` (command)
- YAML relies on spaces, so quote ambiguous values
- Validate locally with an editor extension or `actionlint`

**Next:** [Events and Triggers](./03_events-and-triggers.md)
