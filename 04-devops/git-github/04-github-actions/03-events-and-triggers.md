# Events and Triggers

An **event** is something that happens in or to your repository. The `on:` key tells GitHub which events should start the workflow, and optional **filters** narrow it down to the cases you care about.

```
on: <event>  +  filters (branches, paths, types)  =  when the workflow runs
```

## The simplest trigger

```yaml
on: push
```

Every push to any branch or tag starts the workflow.

## Three ways to write `on`

```yaml
# 1. One event
on: push

# 2. Several events, no configuration
on: [push, pull_request]

# 3. A map, when any event needs filters or settings
on:
  push:
    branches: [main]
  pull_request:
    branches: [main]
  workflow_dispatch:
```

Use form 3 as soon as you need anything beyond the defaults.

## Common events

| Event | Fires when | Typical use |
|-------|-----------|-------------|
| `push` | Commits or tags are pushed | CI on every branch, deploy on `main` |
| `pull_request` | A PR is opened, updated, or reopened | Tests before merging |
| `workflow_dispatch` | Someone runs it manually | One-click deploys, maintenance tasks |
| `schedule` | A cron time arrives | Nightly builds, dependency audits |
| `release` | A release is published or edited | Publish packages |
| `workflow_call` | Another workflow calls this one | Reusable workflows (chapter 12) |
| `workflow_run` | Another workflow finishes | Chain after CI completes |
| `issues`, `issue_comment` | Issue activity | Triage bots, slash commands |
| `repository_dispatch` | An external system calls the API | Trigger from outside GitHub |
| `pull_request_target` | Like `pull_request`, but runs in the base repo context | Labeling fork PRs (see warning below) |

The full list is much longer; these cover the vast majority of real workflows.

## Activity types

Many events have **types** that say what happened. Use `types:` to choose which ones count.

```yaml
on:
  pull_request:
    types: [opened, synchronize, reopened, ready_for_review]
```

| Event | Default types |
|-------|---------------|
| `pull_request` | `opened`, `synchronize`, `reopened` |
| `issues` | All types |
| `release` | All types |

`synchronize` means new commits were pushed to the PR branch.

## Branch and tag filters

```yaml
on:
  push:
    branches:
      - main
      - "release/**"          # release/1.0, release/2.x/hotfix
    tags:
      - "v*.*.*"              # v1.2.3
```

| Pattern | Matches |
|---------|---------|
| `*` | Any characters except `/` |
| `**` | Any characters including `/` |
| `?` | One character |
| `!pattern` | Exclude |

```yaml
on:
  push:
    branches:
      - "**"
      - "!experimental/**"    # everything except experimental/*
```

Rules to remember:

- `branches` and `branches-ignore` cannot be used together for the same event; use `!` inside `branches` instead
- A `push` with both `branches` and `tags` runs when **either** matches
- For `pull_request`, `branches` filters on the **target** (base) branch

## Path filters

Run only when certain files change.

```yaml
on:
  push:
    paths:
      - "src/**"
      - "package*.json"
    paths-ignore:
      - "**.md"
      - "docs/**"
```

- `paths` and `paths-ignore` cannot be combined for the same event; use `!` patterns inside `paths`
- Path filters compare against the files changed in the push or PR

## Scheduled runs

```yaml
on:
  schedule:
    - cron: "30 2 * * 1-5"    # 02:30 UTC, Monday to Friday
```

```
┌───────── minute (0-59)
│ ┌─────── hour (0-23)
│ │ ┌───── day of month (1-31)
│ │ │ ┌─── month (1-12)
│ │ │ │ ┌─ day of week (0-6, Sunday = 0)
* * * * *
```

| Fact | Detail |
|------|--------|
| Time zone | Always **UTC** |
| Shortest interval | Every 5 minutes |
| Branch | Runs only against the **default branch** |
| Accuracy | Can be delayed during busy periods |
| Inactivity | In public repositories, scheduled workflows are disabled after 60 days without repository activity |

## Manual runs: `workflow_dispatch`

```yaml
on:
  workflow_dispatch:
    inputs:
      environment:
        description: "Where to deploy"
        required: true
        type: choice
        options: [staging, production]
        default: staging
      dry_run:
        description: "Skip the actual deploy"
        type: boolean
        default: true
      version:
        description: "Version to release"
        type: string

jobs:
  deploy:
    runs-on: ubuntu-latest
    steps:
      - run: echo "Deploying ${{ inputs.version }} to ${{ inputs.environment }} (dry run: ${{ inputs.dry_run }})"
```

| Input type | Shown as |
|------------|----------|
| `string` | Text box |
| `number` | Number box |
| `boolean` | Checkbox |
| `choice` | Dropdown (needs `options`) |
| `environment` | Dropdown of repository environments |

Ways to start it:

| Method | How |
|--------|-----|
| UI | **Actions** tab, pick the workflow, **Run workflow** |
| GitHub CLI | `gh workflow run deploy.yml -f environment=staging -f dry_run=false` |
| REST API | `POST /repos/{owner}/{repo}/actions/workflows/{id}/dispatches` |

The **Run workflow** button appears once the workflow file exists on the **default branch**.

## Triggering from outside: `repository_dispatch`

```yaml
on:
  repository_dispatch:
    types: [deploy-request]
```

An external system sends a `POST /repos/{owner}/{repo}/dispatches` request with `{"event_type": "deploy-request"}`. Like `schedule`, this runs against the default branch.

## Chaining workflows: `workflow_run`

```yaml
on:
  workflow_run:
    workflows: ["CI"]
    types: [completed]

jobs:
  deploy:
    if: github.event.workflow_run.conclusion == 'success'
    runs-on: ubuntu-latest
    steps:
      - run: echo "CI passed, deploying"
```

`completed` fires on success **and** failure, so check `conclusion`.

## Avoiding duplicate runs

```yaml
on:
  push:
    branches: [main]      # only direct pushes to main
  pull_request:           # every PR
```

If you listed `push` without a branch filter **and** `pull_request`, a commit pushed to a PR branch triggers both. Restricting `push` to `main` avoids the double run.

## Which event for which job?

```
Run tests on every PR            →  pull_request
Deploy after merge to main       →  push (branches: [main])
Publish on a version tag         →  push (tags: v*) or release
Nightly job                      →  schedule
Button in the UI                 →  workflow_dispatch
Called by another workflow       →  workflow_call
After another workflow finishes  →  workflow_run
```

## Security note: forks

| Event | Code that runs | Secrets for fork PRs |
|-------|----------------|----------------------|
| `pull_request` | The PR's merged code | **Not available**; token is read-only |
| `pull_request_target` | Your **base** branch workflow, with secrets available | Available |

Never check out and execute untrusted PR code inside a `pull_request_target` workflow. That combination is a well-known way to leak secrets. Chapter 17 covers it.

## Events and the GITHUB_TOKEN

Events created by actions that use the built-in `GITHUB_TOKEN` do **not** start new workflow runs (except `workflow_dispatch` and `repository_dispatch`). This prevents infinite loops. If a workflow pushes a commit and you expect another workflow to react, use a personal access token or GitHub App token instead.

## Mental model checklist

- Pick the **event** first, then narrow with **types**, **branches**, **tags**, **paths**
- `schedule` and `repository_dispatch` always use the default branch
- Cron is UTC
- `pull_request` branch filters look at the **target** branch
- Filters of the same kind cannot be combined with their `-ignore` twin

## Common questions

| Question | Answer |
|----------|--------|
| Can I trigger on several events? | Yes, list them under `on:` |
| How do I run on tags only? | `on: push: tags: ["v*"]` with no `branches` key |
| Why did my cron not run exactly on time? | Scheduled runs can be delayed under load |
| Can I run a workflow manually if it has no `workflow_dispatch`? | No, add the trigger |
| Does `paths` work for `schedule`? | No, only for `push` and `pull_request` style events |
| How do I trigger on a PR comment? | `issue_comment` and check `github.event.issue.pull_request` |
| Can I skip CI for a commit? | Include `[skip ci]` or `[ci skip]` in the commit message for `push` and `pull_request` events |

## Pitfalls

| Pitfall | Why it hurts | Better |
|---------|--------------|--------|
| `push` and `pull_request` both unfiltered | Double runs on PR branches | Restrict `push` to `main` |
| Required status check plus `paths` filter | Check never reports when no matching files change, so the PR stays pending | Use a lightweight always-run job, or avoid filters on required checks |
| Cron in local time | Runs at the wrong hour | Convert to UTC |
| `workflow_run` without checking `conclusion` | Deploys after failed CI | `if: github.event.workflow_run.conclusion == 'success'` |
| `pull_request_target` plus checkout of PR code | Secret exfiltration risk | Use `pull_request`, or do not run untrusted code |
| Expecting a pushed-by-Actions commit to trigger CI | `GITHUB_TOKEN` events do not trigger runs | Use a PAT or App token deliberately |

## Try it

1. Create a workflow that triggers on `push` to `main` and on `workflow_dispatch` with a `choice` input
2. Run it from the Actions tab and print the chosen value
3. Add `paths-ignore: ["**.md"]`, push a change to `README.md` only, and confirm no run starts

## Key takeaways

- `on:` selects the events; filters narrow them
- Use `types`, `branches`, `tags`, and `paths` to avoid unnecessary runs
- `workflow_dispatch` gives you a manual button with typed inputs
- `schedule` is UTC, default branch only, and not perfectly punctual
- Be careful with fork PRs and `pull_request_target`

**Next:** [Jobs and Steps](./04_jobs-and-steps.md)
