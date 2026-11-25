# Permissions and Environments

Two features decide **what a workflow is allowed to do**. **Permissions** limit what the automatic `GITHUB_TOKEN` can access in your repository. **Environments** represent deployment targets (staging, production) and attach protection rules, such as required approvals, plus their own secrets and variables.

```
permissions   →  what can this job DO to the repository?   (token scopes)
environments  →  who must APPROVE, and which secrets unlock, for this deployment?
```

## The simplest permissions block

```yaml
permissions:
  contents: read

jobs:
  test:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v4
      - run: npm test
```

The token can read the repository contents and nothing else. If a step is compromised, the damage is limited.

## The `GITHUB_TOKEN`

For every job, GitHub creates a short-lived token that expires when the job ends. It is available as `secrets.GITHUB_TOKEN` or `github.token`, and many actions and the `gh` CLI use it automatically.

```yaml
- run: gh issue comment 1 --body "Build finished"
  env:
    GH_TOKEN: ${{ secrets.GITHUB_TOKEN }}
```

What it can do depends entirely on the `permissions` you grant.

## Permission scopes

Each scope can be set to `read`, `write`, or `none`.

| Scope | Controls |
|-------|----------|
| `contents` | Reading code, pushing commits and tags, creating releases |
| `pull-requests` | Commenting on, labeling, and editing pull requests |
| `issues` | Creating and editing issues and comments |
| `packages` | GitHub Packages and Container Registry (GHCR) |
| `actions` | Workflow runs, artifacts, caches via the API |
| `checks` | Check runs and annotations |
| `statuses` | Commit statuses |
| `deployments` | Deployment records |
| `security-events` | Code scanning results |
| `pages` | GitHub Pages deployments |
| `id-token` | Requesting an OIDC token (`write` only) |
| `attestations` | Build provenance attestations |
| `discussions` | Discussions |

New scopes are added over time; the workflow syntax documentation lists the current set.

## How the permission keys behave

```yaml
permissions:
  contents: read
  pull-requests: write
```

**Any scope you do not list becomes `none`** as soon as you write a `permissions:` block. That is the intent: you list only what the job needs.

| Form | Meaning |
|------|---------|
| `permissions:` with scopes | Listed scopes as given, all others `none` |
| `permissions: read-all` | Read access to every scope |
| `permissions: write-all` | Write access to every scope (avoid) |
| `permissions: {}` | No permissions at all |
| No `permissions:` key | Falls back to the repository or organization default |

## Workflow level and job level

```yaml
permissions:                    # default for every job
  contents: read

jobs:
  test:
    runs-on: ubuntu-latest
    steps: [...]                # uses contents: read

  release:
    runs-on: ubuntu-latest
    permissions:                # replaces the workflow default for this job
      contents: write
      packages: write
    steps: [...]
```

A job-level block **replaces** the workflow-level block for that job (it does not merge). Put a locked-down default at the top and widen only the job that needs more.

## Common permission recipes

| Task | Permissions needed |
|------|--------------------|
| Plain CI (checkout, build, test) | `contents: read` |
| Comment on or label a pull request | `contents: read`, `pull-requests: write` |
| Create or edit issues | `issues: write` |
| Push a commit or tag back to the repo | `contents: write` |
| Create a GitHub Release | `contents: write` |
| Push to GHCR | `contents: read`, `packages: write` |
| Upload code scanning results | `security-events: write` |
| Deploy to GitHub Pages | `pages: write`, `id-token: write` |
| Authenticate to a cloud with OIDC | `id-token: write`, `contents: read` |
| Sign or attest build artifacts | `id-token: write`, `attestations: write`, `contents: read` |

## Repository default permissions

**Settings → Actions → General → Workflow permissions** sets what the token gets when a workflow has no `permissions:` key.

| Setting | Effect |
|---------|--------|
| Read repository contents and packages permissions | Restricted default, recommended |
| Read and write permissions | Broad default; older repositories may still have this |

Even when the default is restricted, **write explicit `permissions:` in every workflow**, so the file documents what it needs and does not depend on a setting someone can change.

## Fork pull requests

| Event | Token for PRs from forks |
|-------|--------------------------|
| `pull_request` | **Read-only**, no secrets (write permissions are downgraded) |
| `pull_request_target` | Uses the base repository's permissions, so be careful (chapter 17) |

This is why a workflow that comments on PRs usually fails on fork PRs unless you design around it.

## Environments

An **environment** is a named deployment target configured in **Settings → Environments**, such as `staging` or `production`.

### Using an environment in a job

```yaml
jobs:
  deploy:
    runs-on: ubuntu-latest
    environment: production
    steps:
      - run: ./deploy.sh
        env:
          API_KEY: ${{ secrets.PROD_API_KEY }}      # environment secret
```

When the job reaches `environment: production`, GitHub applies that environment's rules **before the job starts**. Environment secrets are released only after the rules pass.

### With a URL on the deployment

```yaml
environment:
  name: production
  url: ${{ steps.deploy.outputs.url }}
```

The URL shows as a "View deployment" button on the run and in the repository's **Deployments** list.

### Environment-level features

| Feature | Description |
|---------|-------------|
| **Required reviewers** | Up to six people or teams must approve before the job proceeds |
| **Prevent self-review** | The person who triggered the run cannot approve their own deployment |
| **Wait timer** | Delay the job by a set number of minutes |
| **Deployment branches and tags** | Only selected branches or tags may deploy (for example `main`, `release/*`) |
| **Custom protection rules** | Gate deployments on external systems through GitHub Apps |
| **Environment secrets** | Visible only to jobs that target this environment |
| **Environment variables** | Same idea, for non-sensitive configuration |

Availability of some features, such as required reviewers on private repositories, depends on your GitHub plan. Check the current documentation.

## The approval flow

```
push to main
   │
   ▼
job: test  ──✓──►  job: deploy (environment: production)
                       │
                       ▼
                 ⏸ waiting for approval
                       │  reviewer clicks "Approve and deploy"
                       ▼
                 secrets released ──► steps run
```

The run shows as **Waiting**, reviewers are notified, and the job starts only when approved. If nobody approves within the time limit (30 days), the run fails.

## A staged deployment

```yaml
name: Deploy

on:
  push:
    branches: [main]

permissions:
  contents: read

jobs:
  staging:
    runs-on: ubuntu-latest
    environment: staging
    steps:
      - uses: actions/checkout@v4
      - run: ./deploy.sh staging
        env:
          DEPLOY_TOKEN: ${{ secrets.DEPLOY_TOKEN }}     # staging's own value

  production:
    needs: staging
    runs-on: ubuntu-latest
    environment: production                              # requires approval
    steps:
      - uses: actions/checkout@v4
      - run: ./deploy.sh production
        env:
          DEPLOY_TOKEN: ${{ secrets.DEPLOY_TOKEN }}     # production's own value
```

The same secret **name** (`DEPLOY_TOKEN`) resolves to a different value in each environment. The workflow stays identical; only the environment changes.

## Which value wins?

If the same name exists at several levels, the narrowest scope wins.

```
environment  >  repository  >  organization
```

## Manual deployment with an environment input

```yaml
on:
  workflow_dispatch:
    inputs:
      target:
        type: environment
        required: true

jobs:
  deploy:
    runs-on: ubuntu-latest
    environment: ${{ inputs.target }}
    steps:
      - run: echo "Deploying to ${{ inputs.target }}"
```

The UI shows a dropdown of your environments.

## Combining with concurrency and branch rules

| Pairing | Purpose |
|---------|---------|
| `environment` + `concurrency` group per environment | Never deploy twice at once to production |
| Environment branch restriction + required reviewers | Only `main`, and only after human sign-off |
| `permissions: contents: read` + environment secrets | Least privilege for both repo access and credentials |
| OIDC (`id-token: write`) + environment-bound cloud trust | Cloud credentials issued only to the correct environment (chapter 17) |

## Mental model checklist

- `permissions` shrinks the **token**; environments gate **deployments and secrets**
- Declare permissions in every workflow, start from `contents: read`
- A job-level `permissions` block replaces, not merges
- Anything not listed is `none` once you list something
- Production deploys belong behind an environment with required reviewers and a branch restriction

## Common questions

| Question | Answer |
|----------|--------|
| Why do I get "Resource not accessible by integration"? | The token lacks a needed permission, so add it to `permissions:` |
| Is the `GITHUB_TOKEN` the same as a personal access token? | No, it is per-run, short-lived, and limited to this repository |
| Can `GITHUB_TOKEN` trigger other workflows? | Pushes and events made with it do not trigger new runs (except `workflow_dispatch` and `repository_dispatch`) |
| Can I use one workflow for staging and production? | Yes, select the environment by input or job |
| Do I need environments to deploy? | No, but they add approvals, scoped secrets, and deployment history |
| Can environment secrets be read by any job? | Only by jobs that declare that `environment:` and pass its rules |
| What if the environment name does not exist? | GitHub creates it automatically on first use, with no protection rules |
| Can a workflow bypass required reviewers? | No, the job waits regardless of who triggered it |

## Pitfalls

| Pitfall | Why it hurts | Better |
|---------|--------------|--------|
| No `permissions` key and a permissive repo default | Every job gets broad write access | Set `contents: read` at the top |
| `permissions: write-all` | A compromised step can modify almost anything | List only the scopes needed |
| Job-level `permissions` that omits `contents: read` | Checkout may fail because the block replaced the default | Include everything the job needs |
| Environment created implicitly with no rules | Looks protected but is not | Configure reviewers and branch restrictions |
| Production secrets stored at repository level | Any workflow on any branch can read them | Store them as **environment** secrets |
| Required reviewers who trigger their own deploys | Self-approval defeats the purpose | Enable prevent self-review |
| Cancelling deploys with concurrency | Half-applied deployment | `cancel-in-progress: false` |

## Try it

1. Add `permissions: contents: read` to an existing workflow, then add a step that comments on an issue and observe the 403 error
2. Fix it by adding `issues: write` to only that job
3. Create `staging` and `production` environments, add a required reviewer to production, and run the staged deployment above

## Key takeaways

- `GITHUB_TOKEN` is per-run; its power comes only from `permissions`
- Start with `contents: read` and grant extra scopes per job
- Environments add approvals, wait timers, branch restrictions, and scoped secrets
- The same secret name can hold different values per environment
- Protect production with required reviewers and a branch rule

**Next:** [Reusable Workflows](./12_reusable-workflows.md)
