# Actions and Marketplace

An **action** is a reusable, packaged unit of automation that you call from a step with `uses:`. Instead of writing the same shell commands in every repository, you reference an action that someone (GitHub, the community, or you) already built.

```
step:  uses: owner/repo@version   →   runs that repository's action code
```

## The simplest action step

```yaml
steps:
  - uses: actions/checkout@v4          # downloads your repository onto the runner
```

`actions/checkout` is the most used action in existence: without it the runner has no copy of your code.

## Giving an action inputs

```yaml
- uses: actions/setup-node@v4
  with:                                # inputs go under "with"
    node-version: 20
    cache: npm
```

Inputs are listed in each action's README (and in its `action.yml`). Unknown inputs produce a warning, not an error, so a typo can silently do nothing.

## Reading an action's outputs

```yaml
- id: cache
  uses: actions/cache@v4
  with:
    path: ~/.npm
    key: npm-${{ hashFiles('**/package-lock.json') }}

- if: steps.cache.outputs.cache-hit != 'true'
  run: echo "Cache miss, installing from scratch"
```

Give the step an `id`, then read `steps.<id>.outputs.<name>`.

## Where `uses` can point

| Form | Example | Meaning |
|------|---------|---------|
| Public action | `actions/checkout@v4` | `owner/repo@ref` |
| Action in a subfolder | `github/codeql-action/analyze@v3` | `owner/repo/path@ref` |
| Local action | `./.github/actions/build` | Folder in **your** repo (needs `checkout` first) |
| Docker image | `docker://alpine:3.20` | Runs the image directly |
| Reusable workflow | `org/repo/.github/workflows/ci.yml@main` | Used at **job** level, chapter 12 |

## Versioning: the `@ref` part

```yaml
- uses: actions/checkout@v4                 # moving major tag
- uses: actions/checkout@v4.2.2             # exact release tag
- uses: actions/checkout@main               # branch (changes constantly)
- uses: actions/checkout@<40-char-sha>      # exact commit
```

| Ref | Updates automatically | Safe against tampering | Use for |
|-----|-----------------------|------------------------|---------|
| `@main` | Every push | No | Never in production |
| `@v4` | Yes, with minor and patch releases | Partly; maintainers can move the tag | Official GitHub actions, quick projects |
| `@v4.2.2` | No | Partly; tags can be moved or deleted | Reproducible builds |
| `@<full SHA>` | No | **Yes**, a commit hash cannot be changed | Third-party actions, security-sensitive repos |

The common pattern is a SHA plus a comment:

```yaml
- uses: some-org/some-action@<full-40-character-commit-sha>   # v2.3.1
```

Dependabot can keep both the SHA and the comment up to date (see below). Chapter 17 explains why pinning matters.

## The Marketplace

The **GitHub Marketplace** (`github.com/marketplace?type=actions`) is a searchable catalog of actions.

| Look for | Why |
|----------|-----|
| **Verified creator** badge | GitHub has verified the publisher's identity |
| Recent releases | Unmaintained actions break and become security risks |
| Stars and usage count | Rough signal of trust and community review |
| A clear README with inputs and outputs | Easier to use correctly |
| Small, readable source | You can audit what runs with your secrets |
| Permissions it asks for | Should match what it needs to do |

Not every action in the Marketplace is reviewed by GitHub. Treat third-party actions as **code you are choosing to run** in your pipeline.

## Actions you will use constantly

| Action | Purpose |
|--------|---------|
| `actions/checkout` | Clone your repository |
| `actions/setup-node`, `setup-python`, `setup-java`, `setup-go` | Install a language toolchain, optionally with dependency caching |
| `actions/cache` | Save and restore files between runs (chapter 09) |
| `actions/upload-artifact`, `download-artifact` | Pass files between jobs and keep results (chapter 09) |
| `actions/github-script` | Run JavaScript against the GitHub API inline |
| `docker/setup-buildx-action`, `docker/build-push-action`, `docker/login-action` | Build and push images (chapter 16) |
| `github/codeql-action` | Code scanning |

Always check each action's repository for the current major version before copying an example; versions in this guide may be behind.

## `actions/checkout` in more detail

```yaml
- uses: actions/checkout@v4
  with:
    fetch-depth: 0               # full history (needed for changelogs, git describe)
    ref: ${{ github.head_ref }}  # check out a specific branch or SHA
    submodules: recursive        # also fetch submodules
    persist-credentials: false   # do not leave the token in .git/config
```

| Input | Default | Notes |
|-------|---------|-------|
| `fetch-depth` | `1` | Shallow clone, fastest; set `0` for full history |
| `ref` | The event's ref or SHA | Override to check out something else |
| `persist-credentials` | `true` | Set `false` when later steps run untrusted code |
| `path` | Workspace root | Check out several repos side by side |

## Inline scripting with `github-script`

```yaml
- uses: actions/github-script@v7
  with:
    script: |
      await github.rest.issues.createComment({
        owner: context.repo.owner,
        repo: context.repo.repo,
        issue_number: context.issue.number,
        body: "Thanks for the PR!"
      });
```

Good for small API tasks (labels, comments) without building a custom action.

## How an action runs

1. The runner downloads the action's repository at the requested `@ref`
2. It reads `action.yml` to see the action type: JavaScript, Docker, or composite
3. It maps your `with:` values to inputs
4. It runs the action (plus optional `pre` and `post` hooks)
5. Outputs become available as `steps.<id>.outputs.*`

```
uses: owner/repo@v1  →  fetch  →  read action.yml  →  run  →  outputs
```

`post` hooks run at the **end of the job**, which is how `setup-*` and `cache` save their results after your steps finish.

## Keeping actions up to date with Dependabot

```yaml
# .github/dependabot.yml
version: 2
updates:
  - package-ecosystem: "github-actions"
    directory: "/"
    schedule:
      interval: "weekly"
```

Dependabot opens pull requests when new versions of the actions you use are released, including when you pin to SHAs.

## Mental model checklist

- `uses:` means "run someone's packaged code" and `with:` provides its inputs
- The `@ref` decides **which exact code** runs
- Anything not under `actions/` or `github/` is third-party and deserves a look
- Local actions need `checkout` first
- Pinning to a SHA is the strongest protection

## Common questions

| Question | Answer |
|----------|--------|
| Is `uses` the same as `run`? | No; `run` runs shell commands, `uses` runs a packaged action |
| Can I use an expression in `uses`? | No, the value must be a literal |
| Do I have to publish to the Marketplace? | No, any public repository with an `action.yml` is usable; private ones work within your org or repo settings |
| Can an action see my secrets? | Only those you pass to it through `with:` or `env:`, plus the `GITHUB_TOKEN` it is given; it also runs on your runner, so treat it as trusted code |
| Where do I find an action's inputs? | Its README or `action.yml` |
| Why did my action stop working? | A moving tag such as `@v4` or `@main` may have changed; pin to an exact version or SHA |
| Can I fork an action? | Yes, and pointing to your fork pinned to a SHA is a common way to freeze behavior |

## Pitfalls

| Pitfall | Why it hurts | Better |
|---------|--------------|--------|
| `@main` or `@master` | Behavior can change at any time | Pin to a tag or SHA |
| Trusting an unknown action with secrets | It can exfiltrate them | Review the code, pin a SHA, limit `permissions` |
| Typo in a `with:` key | Input ignored, default used | Check the action's docs and read the run log warnings |
| Using a local action without `checkout` | The folder does not exist | `uses: actions/checkout@v4` first |
| Copying old version numbers from blog posts | Deprecated Node runtimes, missing features | Check the action's releases page |
| Never updating pinned SHAs | Missing security fixes | Enable Dependabot for `github-actions` |

## Try it

1. Build a workflow that checks out your repository and sets up Node 20 with `cache: npm`
2. Add `actions/github-script` to print `context.repo` with `console.log`
3. Pin `actions/checkout` to a full SHA with a version comment, then add the Dependabot config above

## Key takeaways

- An action is reusable automation called with `uses: owner/repo@ref`
- Inputs go in `with:`, outputs come from `steps.<id>.outputs.*`
- The Marketplace is a catalog, not a guarantee of quality or safety
- Prefer verified creators, recent releases, and readable source
- Pin third-party actions to a commit SHA and let Dependabot update them

**Next:** [Runners](./06_runners.md)
