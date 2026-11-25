# Contexts, Expressions, and Conditions

**Contexts** are objects full of information about the run (`github`, `env`, `secrets`, `matrix`, ...). **Expressions** are small formulas, written inside `${{ }}`, that read contexts and compute values. **Conditions** are expressions used in `if:` to decide whether a job or step runs.

```
context     =  data            (github.ref, runner.os, matrix.node)
expression  =  data + logic    (github.ref == 'refs/heads/main')
condition   =  expression in an  if:  key
```

All three are one small language evaluated by **GitHub** before the shell ever sees your script.

## The simplest expression

```yaml
steps:
  - run: echo "Running on ${{ runner.os }}"
```

GitHub replaces `${{ runner.os }}` with `Linux` first, then the shell runs `echo "Running on Linux"`.

## The simplest condition

```yaml
steps:
  - run: ./deploy.sh
    if: github.ref == 'refs/heads/main'
```

The step is skipped unless the run is for `main`.

## Contexts

| Context | Contains | Example |
|---------|----------|---------|
| `github` | Event, repo, ref, actor, run info | `github.ref_name` |
| `env` | Variables set with `env:` | `env.APP_NAME` |
| `vars` | Configuration variables | `vars.AWS_REGION` |
| `secrets` | Secrets | `secrets.API_KEY` |
| `job` | Current job's status and container info | `job.status` |
| `steps` | Earlier steps' outputs, outcome, conclusion | `steps.build.outputs.path` |
| `runner` | The machine | `runner.os` |
| `needs` | Outputs and results of required jobs | `needs.build.outputs.version` |
| `strategy` | Matrix settings | `strategy.job-index` |
| `matrix` | Current matrix combination | `matrix.node` |
| `inputs` | `workflow_dispatch` and `workflow_call` inputs | `inputs.environment` |

Not every context exists everywhere. For example, `matrix` is available inside the job that uses a matrix, `steps` only inside that job's steps, and `needs` only in jobs that declare `needs`. The documentation includes a table of which contexts work in which keys.

## Frequently used `github` properties

| Property | Example value |
|----------|---------------|
| `github.event_name` | `push`, `pull_request`, `workflow_dispatch` |
| `github.ref` | `refs/heads/main`, `refs/tags/v1.0.0`, `refs/pull/12/merge` |
| `github.ref_name` | `main`, `v1.0.0` |
| `github.head_ref` | PR source branch (PR events only) |
| `github.base_ref` | PR target branch (PR events only) |
| `github.sha` | Commit SHA that triggered the run |
| `github.actor` | Username that triggered the run |
| `github.repository` | `owner/repo` |
| `github.run_id`, `github.run_number` | Run identifiers |
| `github.event.pull_request.number` | PR number |
| `github.event.head_commit.message` | Commit message on `push` |

The `github.event` object is the raw webhook payload, so its contents depend on the event.

## Looking at a context

When you are unsure what is in a context, dump it.

```yaml
- name: Dump github context
  run: echo "$GITHUB_CONTEXT"
  env:
    GITHUB_CONTEXT: ${{ toJSON(github) }}
```

Never dump the `secrets` context.

## Expression syntax

### Literals and operators

| Kind | Examples |
|------|----------|
| Strings | `'main'` (single quotes only) |
| Numbers | `10`, `3.5` |
| Booleans | `true`, `false` |
| Null | `null` |
| Property access | `github.ref`, `github['ref']` |
| Comparison | `==`, `!=`, `<`, `<=`, `>`, `>=` |
| Logic | `&&`, `||`, `!` |
| Grouping | `( ... )` |

String comparisons ignore case: `'MAIN' == 'main'` is `true`.

### Truthiness

These are **false**: `false`, `0`, `-0`, `''` (empty string), `null`. Everything else is **true**, including the non-empty string `'false'`.

### Defaults and a ternary idiom

```yaml
# Default value
env:
  ENVIRONMENT: ${{ inputs.environment || 'staging' }}

# Ternary: condition && ifTrue || ifFalse
env:
  DEPLOY_TARGET: ${{ github.ref == 'refs/heads/main' && 'production' || 'preview' }}
```

The ternary idiom only works when `ifTrue` is truthy. If it could be `''` or `0`, it falls through to `ifFalse`.

## Built-in functions

### String and collection helpers

| Function | Example | Result |
|----------|---------|--------|
| `contains(search, item)` | `contains('hello world', 'world')` | `true` |
| `contains(array, item)` | `contains(github.event.pull_request.labels.*.name, 'deploy')` | Whether a label exists |
| `startsWith(str, prefix)` | `startsWith(github.ref, 'refs/tags/')` | `true` for tag pushes |
| `endsWith(str, suffix)` | `endsWith(github.ref, '-rc')` | Release candidates |
| `format(fmt, a, b)` | `format('v{0}.{1}', 1, 2)` | `v1.2` |
| `join(array, sep)` | `join(matrix.*.os, ', ')` | `ubuntu, windows` |

### JSON

| Function | Purpose |
|----------|---------|
| `toJSON(value)` | Pretty-print any value as JSON (great for debugging) |
| `fromJSON(string)` | Parse JSON into an object, array, boolean, or number |

```yaml
# Dynamic matrix built from a job output
strategy:
  matrix:
    include: ${{ fromJSON(needs.plan.outputs.matrix) }}
```

### Files

```yaml
key: ${{ runner.os }}-npm-${{ hashFiles('**/package-lock.json') }}
```

`hashFiles(pattern)` returns a hash of the matching files, so the value changes when any of them change. This is the standard cache key ingredient (chapter 09).

### Object filters

```yaml
${{ github.event.pull_request.labels.*.name }}     # array of every label name
```

The `*` selects that property from each item in a list.

## Status check functions

Only used in `if:` conditions.

| Function | True when |
|----------|-----------|
| `success()` | Everything so far succeeded (**the implicit default**) |
| `failure()` | Any previous step or required job failed |
| `cancelled()` | The run was cancelled |
| `always()` | Always, even after failure or cancellation |

```yaml
steps:
  - run: npm test
  - name: Upload logs if tests failed
    if: failure()
    run: ./upload-logs.sh
  - name: Always clean up
    if: always()
    run: ./cleanup.sh
```

A step with **no** `if:` behaves as `if: success()`. Once any step fails, later steps without their own condition are skipped.

## Step results: `outcome` vs `conclusion`

```yaml
- id: lint
  run: npm run lint
  continue-on-error: true

- run: echo "outcome=${{ steps.lint.outcome }} conclusion=${{ steps.lint.conclusion }}"
```

| Field | Meaning |
|-------|---------|
| `outcome` | Result **before** `continue-on-error` is applied (`success`, `failure`, `cancelled`, `skipped`) |
| `conclusion` | Result **after** `continue-on-error` is applied |

If the lint fails but is allowed to continue, `outcome` is `failure` while `conclusion` is `success`.

## Conditions at job and step level

```yaml
jobs:
  deploy:
    if: github.event_name == 'push' && github.ref == 'refs/heads/main'
    runs-on: ubuntu-latest
    steps:
      - name: Only for tags
        if: startsWith(github.ref, 'refs/tags/v')
        run: echo "Release build"

      - name: Skip on draft PRs
        if: github.event.pull_request.draft == false
        run: npm test
```

### Common condition recipes

| Goal | Condition |
|------|-----------|
| Only on `main` | `github.ref == 'refs/heads/main'` |
| Only on tags | `startsWith(github.ref, 'refs/tags/')` |
| Only for pushes | `github.event_name == 'push'` |
| Not on forks | `github.event.pull_request.head.repo.full_name == github.repository` |
| PR has a label | `contains(github.event.pull_request.labels.*.name, 'deploy')` |
| Skip bot commits | `github.actor != 'dependabot[bot]'` |
| Run on Linux only | `runner.os == 'Linux'` |
| Cleanup after anything | `always()` |
| Run after failure | `failure()` |
| Required job succeeded | `needs.build.result == 'success'` |

### Combining status checks with other conditions

```yaml
if: ${{ always() && github.event_name == 'push' }}
```

When you add your own check you must include a status function (`always()`, `failure()`, ...) if you want the step to run after a failure; otherwise the implicit `success()` still applies.

## `${{ }}` in `if:` is optional, with one trap

```yaml
if: github.ref == 'refs/heads/main'           # fine
if: ${{ github.ref == 'refs/heads/main' }}    # also fine

if: !contains(github.ref, 'release')          # ✗ YAML sees "!" as a tag
if: ${{ !contains(github.ref, 'release') }}   # ✓ wrapper avoids the problem
```

Expressions that **start with** `!` must be wrapped in `${{ }}`. In every other key (`run`, `with`, `env`, `name`), the wrapper is required.

## Expressions are evaluated before the shell

```yaml
- run: echo "${{ github.ref_name }}"       # GitHub inserts "main" first
- run: echo "$GITHUB_REF_NAME"             # the shell reads the variable at run time
```

| | `${{ ... }}` | `$VAR` |
|---|--------------|--------|
| Evaluated by | GitHub | Shell |
| When | Before the step starts | While the step runs |
| Works in `if:`, `with:`, `name:` | Yes | No |
| Safe with untrusted input | **No**, text is pasted into the script | Yes, when passed through `env:` |

## Mental model checklist

- Contexts hold the **data**, expressions do the **logic**, `if:` uses the result
- A step without `if:` is `if: success()`
- Add `always()` or `failure()` when a step must run after something broke
- Everything in `${{ }}` is replaced **before** the shell starts
- Only single quotes work for strings inside expressions

## Common questions

| Question | Answer |
|----------|--------|
| Can I use double quotes inside an expression? | No, use single quotes for expression strings |
| Why does `if: false` still skip but `if: 'false'` run? | The string `'false'` is non-empty and therefore truthy |
| Can I use `secrets` in an `if:`? | Not directly; copy it into `env` first (chapter 07) |
| What is the difference between `github.ref` and `github.ref_name`? | `ref` is the full name (`refs/heads/main`), `ref_name` is short (`main`) |
| `head_ref` is empty. Why? | It is only set on `pull_request` events |
| Does `needs.<job>.result` exist if the job was skipped? | Yes, with the value `skipped` |
| Can expressions do math? | No arithmetic operators; use the shell for computation |
| How do I debug an expression? | Print `toJSON(...)` of the context, or enable debug logging (chapter 18) |

## Pitfalls

| Pitfall | Why it hurts | Better |
|---------|--------------|--------|
| `if: !contains(...)` without wrapper | YAML parse error | `${{ !contains(...) }}` |
| Comparing to `"main"` with double quotes | Expression syntax error | `'main'` |
| Using `github.head_ref` outside PR events | Empty value | Use `github.ref_name` for branches |
| `if: needs.build.result == 'success'` on a job that follows a failure | Job is skipped before the condition is checked | Add `always() &&` |
| Expecting `'false'` to be false | Non-empty strings are truthy | Compare explicitly: `== 'false'` |
| Pasting `${{ github.event.issue.title }}` into `run:` | Script injection | Pass through `env:` |
| Dependence on `outcome` vs `conclusion` confusion | Wrong branch taken after `continue-on-error` | Pick the one you mean |

## Try it

1. Dump `github` with `toJSON` in a workflow triggered by `push`, then trigger it again manually and compare `event_name` and `event`
2. Write a job that runs a deploy step only on `main` and a notify step with `if: always()`
3. Create a PR with a label `deploy` and add a step that runs only when that label exists

## Key takeaways

- Contexts are data objects, expressions compute with them, conditions gate steps and jobs
- `${{ }}` is evaluated by GitHub before the shell starts
- `success()`, `failure()`, `always()`, and `cancelled()` control behavior after failures
- `outcome` is before `continue-on-error`, `conclusion` is after
- Keep untrusted context values out of `run:` strings

**Next:** [Artifacts, Caching, and Dependencies](./09_artifacts-caching-dependencies.md)
