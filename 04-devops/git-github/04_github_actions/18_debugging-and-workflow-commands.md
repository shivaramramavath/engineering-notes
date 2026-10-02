# Debugging and Workflow Commands

**Workflow commands** are special lines of text (and special files) that your steps use to talk to the runner: set outputs, add log annotations, group log lines, hide secrets, and write job summaries. **Debugging** is the practice of reading logs, turning on extra logging, and reproducing failures. The two belong together, since the same commands make your logs easier to debug.

```
your script  ──"::error::..." / >> $GITHUB_OUTPUT──►  runner  ──►  annotations, outputs, summary, logs
```

## The simplest workflow command

```yaml
steps:
  - run: echo "::notice::Deployment started"
```

The runner recognizes the `::notice::` prefix and shows a highlighted annotation on the run page instead of a plain log line.

## Two mechanisms

| Mechanism | Syntax | Used for |
|-----------|--------|----------|
| **Log commands** | `echo "::command parameters::message"` | Annotations, groups, masking, debug messages |
| **Environment files** | `echo "name=value" >> "$FILE"` | Outputs, environment variables, PATH, summaries |

The old `::set-output` and `::set-env` commands were **removed in favor of environment files** because of security issues; do not use them.

## Log commands

### Debug, notice, warning, error

```bash
echo "::debug::Cache key is $KEY"
echo "::notice::Build finished"
echo "::warning::Using deprecated API"
echo "::error::Could not connect to database"
```

| Command | Shows as | Effect on the run |
|---------|----------|-------------------|
| `::debug::` | Hidden unless debug logging is on | None |
| `::notice::` | Blue annotation | None |
| `::warning::` | Yellow annotation | None |
| `::error::` | Red annotation | **Does not fail the step by itself**; add `exit 1` |

### Annotations on specific files and lines

```bash
echo "::error file=src/app.js,line=12,col=5,endLine=12,endColumn=20,title=Lint error::Unused variable 'x'"
echo "::warning file=README.md,line=3::Broken link"
```

| Parameter | Meaning |
|-----------|---------|
| `file` | Path relative to the workspace |
| `line`, `endLine` | Line range |
| `col`, `endColumn` | Column range |
| `title` | Annotation heading |

On pull requests, annotations with a file and line appear inline in the **Files changed** tab.

### Grouping log lines

```bash
echo "::group::Install dependencies"
npm ci
echo "::endgroup::"
```

Groups collapse in the log viewer, which keeps long logs readable.

### Masking a value

```bash
TOKEN=$(./get-temporary-token.sh)
echo "::add-mask::$TOKEN"
echo "Token is $TOKEN"        # shown as ***
```

Mask **before** printing or exporting the value.

### Temporarily stopping command processing

```bash
STOP=$(openssl rand -hex 8)
echo "::stop-commands::$STOP"
cat untrusted-output.txt      # any "::error::" inside is printed as plain text
echo "::$STOP::"              # resume
```

Useful when logging untrusted text that might contain workflow commands.

### Echoing commands

```bash
echo "::echo::on"             # show each command as it runs
echo "::echo::off"
```

## Environment files

Each step receives file paths in these variables. **Append** to them with `>>`.

| Variable | Purpose | Read it as |
|----------|---------|------------|
| `$GITHUB_OUTPUT` | Step outputs | `steps.<id>.outputs.<name>` |
| `$GITHUB_ENV` | Environment variables for **later** steps | `$NAME` or `env.NAME` |
| `$GITHUB_PATH` | Directories added to `PATH` for later steps | Commands resolve automatically |
| `$GITHUB_STEP_SUMMARY` | Markdown shown on the run summary page | Visible on the run page |
| `$GITHUB_STATE` | Data passed from an action's `pre` or `main` to `post` | Used by action authors |

### Outputs

```yaml
- id: build
  run: |
    echo "version=1.4.2" >> "$GITHUB_OUTPUT"
    echo "image=ghcr.io/org/app:1.4.2" >> "$GITHUB_OUTPUT"

- run: echo "Built ${{ steps.build.outputs.version }}"
```

### Environment variables

```yaml
- run: echo "DEPLOY_ENV=staging" >> "$GITHUB_ENV"
- run: echo "Deploying to $DEPLOY_ENV"          # available from the next step on
```

### PATH

```yaml
- run: echo "$HOME/tools/bin" >> "$GITHUB_PATH"
- run: my-tool --version
```

### Multi-line values

Use a **delimiter** syntax. Choose a delimiter that cannot appear in the value.

```bash
DELIM="EOF_$(openssl rand -hex 8)"
{
  echo "report<<$DELIM"
  cat report.txt
  echo "$DELIM"
} >> "$GITHUB_OUTPUT"
```

The same pattern works for `$GITHUB_ENV`. A random delimiter prevents an attacker from injecting extra variables by including your delimiter in the text.

### Job summaries

```yaml
- name: Write summary
  if: always()
  run: |
    {
      echo "## Deployment result :rocket:"
      echo ""
      echo "| Item | Value |"
      echo "|------|-------|"
      echo "| Version | ${VERSION} |"
      echo "| Commit | \`${GITHUB_SHA::7}\` |"
    } >> "$GITHUB_STEP_SUMMARY"
  env:
    VERSION: ${{ steps.build.outputs.version }}
```

The Markdown renders on the run's summary page: good for test tables, deployment links, and coverage numbers. Each step can add to it; the content from all steps is combined.

## Reading logs effectively

| Where | What to look for |
|-------|------------------|
| Run summary page | Which job and step failed, annotations, summary, artifacts |
| Failed step (expanded) | The first error, not the last; later errors are often fallout |
| Step timings | Slow steps, hangs, and cache effects |
| `Set up job` group | Runner image version and the permissions given to `GITHUB_TOKEN` |
| `Post` steps | Cache save results and cleanup |
| Search box in the log viewer | Search for `error`, `Error:`, or the failing command |
| **Download log archive** | Full logs for offline search |

Look for the first line that says what failed. "Process completed with exit code 1" only tells you a command returned non-zero.

## Turning on debug logging

### Per re-run (quickest)

On a finished run, choose **Re-run jobs → Re-run all jobs** (or failed jobs) and tick **Enable debug logging**.

### Permanently

Create a repository **secret or variable**:

| Name | Value | Effect |
|------|-------|--------|
| `ACTIONS_STEP_DEBUG` | `true` | Shows `::debug::` messages and extra step diagnostics |
| `ACTIONS_RUNNER_DEBUG` | `true` | Shows runner diagnostic logs |

Remove them when finished, as debug logs are verbose and may reveal more internals.

## Debugging techniques

### Dump contexts

```yaml
- name: Dump contexts
  run: |
    echo "github:  $GITHUB_CONTEXT"
    echo "job:     $JOB_CONTEXT"
    echo "steps:   $STEPS_CONTEXT"
    echo "runner:  $RUNNER_CONTEXT"
    echo "matrix:  $MATRIX_CONTEXT"
  env:
    GITHUB_CONTEXT: ${{ toJSON(github) }}
    JOB_CONTEXT: ${{ toJSON(job) }}
    STEPS_CONTEXT: ${{ toJSON(steps) }}
    RUNNER_CONTEXT: ${{ toJSON(runner) }}
    MATRIX_CONTEXT: ${{ toJSON(matrix) }}
```

Never dump `secrets`, and delete this step before merging if the output includes anything sensitive.

### See what the shell is doing

```yaml
- run: |
    set -euxo pipefail        # -x prints every command before running it
    ./build.sh
```

### Inspect the environment safely

```yaml
- run: |
    pwd
    ls -la
    echo "PATH=$PATH"
    node --version
    df -h
```

Avoid `env` or `printenv` on their own, since they can print secrets passed to the step.

### Check files and permissions

```yaml
- run: |
    git status
    ls -la scripts/
    file scripts/deploy.sh    # reveals CRLF line endings
```

### Isolate the failure

| Technique | How |
|-----------|-----|
| Reduce the workflow | Comment out jobs and steps until the failure remains |
| Add `if: always()` diagnostics | Capture state after a failure |
| Bisect | Find the commit that broke it with `git bisect` or by re-running old commits |
| Compare with a passing run | Diff the logs of a good and bad run |
| Pin versions | Check whether an unpinned tool or action changed |

### Interactive debugging with tmate

```yaml
- name: Debug with SSH
  if: failure()
  uses: mxschmitt/action-tmate@<pinned-sha>
  timeout-minutes: 15
  with:
    limit-access-to-actor: true
```

This opens an SSH session into the runner so you can poke around. Use it only on repositories you control, with `limit-access-to-actor: true`, pin the action to a SHA, and remove it afterwards. Verify the action's current maintained repository name before use.

### Run workflows locally

Tools such as [`act`](https://github.com/nektos/act) run workflows in Docker on your machine for quick iteration. Behavior is similar but not identical (runner images, secrets, some contexts), so confirm on GitHub before trusting a result.

## The GitHub CLI for runs

```bash
gh run list                         # recent runs
gh run view <run-id>                # summary
gh run view <run-id> --log-failed   # only the failed steps' logs
gh run watch                        # follow a run live
gh run rerun <run-id> --failed      # re-run failed jobs
gh run download <run-id>            # download artifacts
gh workflow run deploy.yml -f environment=staging   # dispatch a workflow
gh workflow list
```

## Common errors and fixes

| Message or symptom | Likely cause | Fix |
|--------------------|--------------|-----|
| **Invalid workflow file** with a line number | YAML or schema error | Fix indentation, keys, or quoting; run `actionlint` |
| **Unrecognized named-value** or **Unexpected symbol** | Typo in a context or expression | Check spelling and quote strings with `'` |
| **Resource not accessible by integration** | Token lacks a permission | Add the scope in `permissions:` (chapter 11) |
| **Process completed with exit code 1** | A command failed | Read the lines above it |
| **exit code 127**, "command not found" | Tool not installed or not on `PATH` | Install it, or add to `$GITHUB_PATH` |
| **Permission denied** running `./script.sh` | Missing execute bit | `chmod +x`, or `git update-index --chmod=+x script.sh` |
| `bad interpreter: /bin/bash^M` | Windows line endings (CRLF) | Convert to LF; add `.gitattributes` with `*.sh text eol=lf` |
| **No such file or directory** | Forgot `checkout`, wrong `working-directory`, or file from another job | Add checkout, fix the path, use artifacts |
| **Context access might be invalid** (editor warning) | Context not available in that key | Check the context availability table in the docs |
| `No space left on device` | Large builds or images fill the runner disk | Clean up, use fewer or smaller caches, delete unneeded preinstalled tools |
| `Waiting for a runner to pick up this job` | No runner matches the labels, or all are busy | Check `runs-on` labels and runner status |
| Cache never hits | Key changes every run, or branch scope | Hash only lockfiles; review cache scope (chapter 09) |
| Secret is empty | Fork PR, wrong environment, or name typo | Check the event and the secret's scope |
| Variable set with `export` is missing later | Steps are separate processes | Append to `$GITHUB_ENV` |
| Step output is empty | Missing `id`, typo, or written to wrong file | Check `id:` and `>> "$GITHUB_OUTPUT"` |
| Workflow does not trigger | Wrong folder, trigger filters, file not on default branch (for some events), or `[skip ci]` | Check `on:` and file location |
| Workflow triggered by a bot commit did not run | `GITHUB_TOKEN` events do not trigger runs | Use a PAT or GitHub App token deliberately |
| Run cancelled unexpectedly | `concurrency` with `cancel-in-progress`, or `fail-fast` | Review those settings |

## Making your own workflows easy to debug

| Practice | Benefit |
|----------|---------|
| Give every step a clear `name:` | Failures read like sentences |
| Wrap noisy output in `::group::` | Shorter, scannable logs |
| Use `::error::` and `::warning::` with file and line | Problems appear on the PR |
| Write a job summary | Key results without opening logs |
| Add `if: always()` steps that collect logs, screenshots, or reports as artifacts | Evidence survives failures |
| Use `set -euo pipefail` in multi-line scripts | Fail at the first problem, not ten lines later |
| Use `timeout-minutes` | Hangs fail fast and visibly |
| Echo non-secret inputs at the start | You can see what the run was given |

## Mental model checklist

- Logs are the first stop: find the **first** error and read upward
- `::error::` annotates, `exit 1` fails
- Environment files (`$GITHUB_OUTPUT`, `$GITHUB_ENV`, `$GITHUB_PATH`) replace removed commands
- Enable debug logging on a re-run before adding ad hoc print statements
- Reproduce the smallest failing case, then fix it
- Never print secrets while debugging

## Common questions

| Question | Answer |
|----------|--------|
| How do I make a step fail from a script? | Exit non-zero (`exit 1`); `::error::` only annotates |
| Why did `::set-output` stop working? | It was deprecated and removed; write to `$GITHUB_OUTPUT` |
| Can I see debug logs without re-running? | Only if `ACTIONS_STEP_DEBUG` was already set; otherwise re-run with debug logging |
| How long are logs kept? | A retention period set by your repository or organization (90 days by default); check your settings |
| How do I add a link to the run summary? | Write Markdown to `$GITHUB_STEP_SUMMARY` |
| What does `::group::` do to the exit status? | Nothing; it only formats the log |
| Can annotations appear without a file? | Yes, they show on the run summary |
| Is there a limit to annotations? | Yes, GitHub caps the number shown per step and per run; check the docs |
| Where do I see the timing of each step? | Click the step in the run page; timings appear beside each step name |

## Pitfalls

| Pitfall | Why it hurts | Better |
|---------|--------------|--------|
| Using `::set-output` | Removed, produces warnings or failures | `>> "$GITHUB_OUTPUT"` |
| `echo "::error::..."` and nothing else | Step still passes | Add `exit 1` |
| Printing environment with `env` | Secrets may appear in logs | Print selected variables |
| Expecting `$GITHUB_ENV` to apply in the same step | It takes effect for the next step | Set variables in an earlier step |
| Multi-line value written with a plain `echo` | Breaks the `name=value` format | Delimiter syntax with a random delimiter |
| Leaving `ACTIONS_STEP_DEBUG` on | Noisy logs, more exposure | Remove when done |
| Leaving tmate or dump steps in merged workflows | Security and leakage risk | Delete before merging |
| Debugging only through repeated pushes | Slow feedback | Use `act`, `actionlint`, and re-run with debug logging |

## Try it

1. Write a step that sets an output, an environment variable, and a PATH entry, then use each in a later step
2. Add an `::error file=...,line=...::` annotation on a real file and open it in a pull request's **Files changed**
3. Break a workflow on purpose, re-run with **Enable debug logging**, and compare the logs to the normal run
4. Write a markdown table to `$GITHUB_STEP_SUMMARY` and view it on the run page

## Key takeaways

- Workflow commands and environment files are how steps communicate with the runner
- `::error::`, `::warning::`, `::notice::`, `::group::`, and `::add-mask::` shape your logs
- Use `$GITHUB_OUTPUT`, `$GITHUB_ENV`, `$GITHUB_PATH`, and `$GITHUB_STEP_SUMMARY`; never the removed `::set-output`
- Debug with step names, grouped logs, re-runs with debug logging, `gh run`, and `actionlint`
- Reproduce minimally, never print secrets, and remove debug scaffolding before merging

**Next:** Back to the [section README](./00_README.md), then explore the [`examples/`](./examples/) and [`CHEATSHEET.md`](./CHEATSHEET.md)
