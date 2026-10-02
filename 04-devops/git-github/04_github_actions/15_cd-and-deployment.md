# CD and Deployment

**Continuous Delivery** means every change that passes CI is automatically packaged and ready to release. **Continuous Deployment** goes one step further and releases it to production automatically. Both are called **CD**. In GitHub Actions, a CD pipeline is a workflow that builds an artifact once, then promotes it through environments such as staging and production.

```
merge to main  →  build once  →  deploy to staging  →  tests  →  (approval)  →  deploy to production
```

## Delivery vs deployment

| Term | What happens automatically | Human step |
|------|----------------------------|------------|
| Continuous Integration | Build and test | Merge |
| Continuous **Delivery** | Build, test, package, deploy to staging | A person approves the production release |
| Continuous **Deployment** | All of the above plus production | None |

Most teams start with delivery (approval before production) and move to deployment as confidence in tests grows.

## The simplest deploy workflow

```yaml
name: Deploy

on:
  push:
    branches: [main]

permissions:
  contents: read

jobs:
  deploy:
    runs-on: ubuntu-latest
    environment: production
    steps:
      - uses: actions/checkout@v4
      - run: ./scripts/deploy.sh
        env:
          DEPLOY_TOKEN: ${{ secrets.DEPLOY_TOKEN }}
```

Every merge to `main` deploys, using an environment-scoped secret (chapter 11).

## Principles of a good CD pipeline

| Principle | Meaning |
|-----------|---------|
| **Build once, deploy many** | The exact artifact tested in staging is what ships to production, never rebuilt |
| **Same process everywhere** | Staging and production differ in configuration, not in steps |
| **Environments with rules** | Approvals and branch restrictions guard production |
| **One deploy at a time** | Concurrency groups prevent overlapping deployments |
| **Verify after deploying** | Smoke tests and health checks |
| **Easy rollback** | Redeploy a previous version in one click |
| **No long-lived credentials** | Prefer OIDC and short-lived tokens (chapter 17) |

## Choosing triggers

| Goal | Trigger |
|------|---------|
| Deploy every merge to `main` | `push` to `main` |
| Deploy only tagged releases | `push` with `tags: ["v*"]` |
| Deploy when a GitHub Release is published | `release` with `types: [published]` |
| Deploy after CI passes in another workflow | `workflow_run` (check `conclusion == 'success'`) |
| Deploy on demand, to a chosen target | `workflow_dispatch` with inputs |

## A staged pipeline with approval

```yaml
name: Release pipeline

on:
  push:
    branches: [main]

permissions:
  contents: read

concurrency:
  group: release-${{ github.ref }}
  cancel-in-progress: false

jobs:
  build:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v4
      - uses: actions/setup-node@v4
        with: { node-version: 20, cache: npm }
      - run: npm ci && npm test && npm run build
      - uses: actions/upload-artifact@v4
        with:
          name: dist
          path: dist/

  deploy-staging:
    needs: build
    runs-on: ubuntu-latest
    environment:
      name: staging
      url: https://staging.example.com
    steps:
      - uses: actions/download-artifact@v4
        with: { name: dist, path: dist }
      - run: ./deploy.sh staging
        env:
          DEPLOY_TOKEN: ${{ secrets.DEPLOY_TOKEN }}

  smoke-test:
    needs: deploy-staging
    runs-on: ubuntu-latest
    steps:
      - run: curl --fail --retry 5 --retry-delay 5 https://staging.example.com/health

  deploy-production:
    needs: smoke-test
    runs-on: ubuntu-latest
    environment:
      name: production              # required reviewers configured here
      url: https://example.com
    steps:
      - uses: actions/download-artifact@v4
        with: { name: dist, path: dist }
      - run: ./deploy.sh production
        env:
          DEPLOY_TOKEN: ${{ secrets.DEPLOY_TOKEN }}
```

A runnable version is in [`examples/deploy-with-approval.yml`](./examples/deploy-with-approval.yml).

Flow:

```
build ──► staging ──► smoke test ──► ⏸ approval ──► production
 (artifact uploaded once, downloaded by both deploys)
```

## Deployment targets

### GitHub Pages

```yaml
name: Deploy site

on:
  push:
    branches: [main]

permissions:
  contents: read
  pages: write
  id-token: write

concurrency:
  group: pages
  cancel-in-progress: false

jobs:
  build:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v4
      - uses: actions/setup-node@v4
        with: { node-version: 20, cache: npm }
      - run: npm ci && npm run build
      - uses: actions/upload-pages-artifact@v3
        with:
          path: dist

  deploy:
    needs: build
    runs-on: ubuntu-latest
    environment:
      name: github-pages
      url: ${{ steps.deployment.outputs.page_url }}
    steps:
      - id: deployment
        uses: actions/deploy-pages@v4
```

Also set **Settings → Pages → Source** to **GitHub Actions**. Check each action's repository for its latest version.

### A server over SSH

```yaml
- name: Deploy over SSH
  env:
    SSH_KEY: ${{ secrets.SSH_PRIVATE_KEY }}
    HOST: ${{ vars.DEPLOY_HOST }}
  run: |
    install -m 600 /dev/null key
    printf '%s\n' "$SSH_KEY" > key
    ssh-keyscan -H "$HOST" >> ~/.ssh/known_hosts
    rsync -az --delete -e "ssh -i key" dist/ "deploy@$HOST:/var/www/app/"
```

Store the host's key fingerprint (or a pinned `known_hosts` entry) instead of blindly scanning in high-security setups.

### A cloud provider with OIDC (no stored keys)

```yaml
permissions:
  id-token: write
  contents: read

steps:
  - uses: aws-actions/configure-aws-credentials@v4
    with:
      role-to-assume: arn:aws:iam::123456789012:role/github-deploy
      aws-region: eu-west-1
  - run: aws s3 sync dist/ s3://my-bucket --delete
```

GitHub proves the workflow's identity to the cloud provider, which hands back short-lived credentials. Setup details for AWS, Azure, and GCP are in chapter 17.

### Publishing a package

```yaml
- uses: actions/setup-node@v4
  with:
    node-version: 20
    registry-url: https://registry.npmjs.org
- run: npm publish --provenance --access public
  env:
    NODE_AUTH_TOKEN: ${{ secrets.NPM_TOKEN }}
```

`--provenance` additionally requires `id-token: write` and links the package to the workflow that built it.

### Container images and Kubernetes

Build, push, and deploy images as described in chapter 16, then update the running service with `kubectl`, Helm, or your platform's CLI.

## Release automation

Tag a version, and Actions builds artifacts and publishes a GitHub Release.

```yaml
name: Release

on:
  push:
    tags: ["v*.*.*"]

permissions:
  contents: write                 # needed to create the release

jobs:
  release:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v4
      - uses: actions/setup-node@v4
        with: { node-version: 20, cache: npm }
      - run: npm ci && npm run build
      - run: tar -czf app-${{ github.ref_name }}.tgz dist/
      - name: Create GitHub Release
        run: gh release create "$GITHUB_REF_NAME" "app-$GITHUB_REF_NAME.tgz" --generate-notes
        env:
          GH_TOKEN: ${{ secrets.GITHUB_TOKEN }}
```

```
git tag v1.2.0 && git push origin v1.2.0  →  build  →  release page with notes and assets
```

`--generate-notes` writes release notes from merged pull requests. See [`examples/release-automation.yml`](./examples/release-automation.yml).

## Manual deploys and rollbacks

```yaml
on:
  workflow_dispatch:
    inputs:
      environment:
        type: environment
        required: true
      ref:
        description: "Git tag or SHA to deploy"
        required: true
        default: main

jobs:
  deploy:
    runs-on: ubuntu-latest
    environment: ${{ inputs.environment }}
    steps:
      - uses: actions/checkout@v4
        with:
          ref: ${{ inputs.ref }}
      - run: ./deploy.sh "${{ inputs.environment }}"
```

To roll back, run the workflow with the previous tag or SHA. This is the simplest rollback and is why every release should be tagged.

## Deployment strategies

| Strategy | Idea | Trade-off |
|----------|------|-----------|
| **Recreate** | Stop old, start new | Downtime, simplest |
| **Rolling** | Replace instances gradually | No downtime, mixed versions briefly |
| **Blue-green** | Run new beside old, switch traffic | Instant rollback, double the infrastructure |
| **Canary** | Send a small share of traffic to the new version first | Safest, needs traffic control and monitoring |

Actions orchestrates these; the traffic switching itself is done by your platform (Kubernetes, load balancers, cloud services).

## Verifying a deployment

```yaml
- name: Smoke test
  run: |
    for i in {1..10}; do
      if curl --fail --silent https://example.com/health; then exit 0; fi
      sleep 6
    done
    echo "::error::Health check failed after deployment"
    exit 1
```

| Check | Why |
|-------|-----|
| Health endpoint | The service started |
| A key user flow | The release works end to end |
| Version endpoint | The expected version is live |
| Automatic rollback step on failure (`if: failure()`) | Limits the time in a broken state |

## Notifications

```yaml
- name: Notify on failure
  if: failure()
  run: |
    curl -X POST -H 'Content-type: application/json' \
      --data "{\"text\":\"Deploy failed: $GITHUB_SERVER_URL/$GITHUB_REPOSITORY/actions/runs/$GITHUB_RUN_ID\"}" \
      "$SLACK_WEBHOOK_URL"
  env:
    SLACK_WEBHOOK_URL: ${{ secrets.SLACK_WEBHOOK_URL }}
```

## Security checklist for deployments

| Check | Detail |
|-------|--------|
| Environment protection | Required reviewers and allowed branches on production |
| Environment-scoped secrets | Production credentials not visible to other workflows |
| Minimal `permissions` | `contents: read` unless the job truly writes |
| OIDC instead of static cloud keys | Short-lived, scoped credentials |
| Deploy only from protected refs | `main` or tags, never arbitrary PR branches |
| No deploys from fork PRs | Use `push` or `workflow_dispatch`, not `pull_request` |

## Mental model checklist

- Build **once**, promote the same artifact
- Environments hold **rules and secrets**; the workflow holds the **steps**
- `concurrency` with `cancel-in-progress: false` keeps deployments serialized
- Verify after deploy, and have a one-click rollback
- Tags make releases reproducible

## Common questions

| Question | Answer |
|----------|--------|
| Delivery or deployment for my project? | Start with delivery (approval gate) and remove the gate when tests and monitoring justify it |
| Should staging and production use the same workflow? | Yes, the same steps with different environments |
| How do I deploy only when CI passes? | Put build and deploy in the same workflow with `needs`, or use `workflow_run` with a success check |
| How do I roll back? | Run the deploy workflow with the previous tag or SHA |
| Can I deploy from a pull request? | Preview deployments are possible, but restrict them carefully; fork PRs have no secrets |
| Where do deployment URLs appear? | On the run and in the repository's **Deployments** page when `environment.url` is set |
| Should I use `latest` tags for releases? | Prefer immutable version tags and SHAs |
| How do I deploy different branches to different targets? | Separate jobs with `if:` conditions, or separate environments with branch rules |

## Pitfalls

| Pitfall | Why it hurts | Better |
|---------|--------------|--------|
| Rebuilding for each environment | What you tested is not what ships | Upload once, download in each deploy |
| `cancel-in-progress: true` on deploy | A deploy can die midway | `false` for deployments |
| Production secrets at repository level | Any workflow can read them | Environment secrets |
| No approval or branch rule on production | Anything merged ships instantly, or a rogue branch deploys | Required reviewers, deployment branches |
| Long-lived cloud keys in secrets | Big target if leaked | OIDC |
| No health check after deploy | Failures found by users | Smoke test step |
| Releases without tags | Rollback is guesswork | Tag every release |
| Using `pull_request` for deploys | Fork PRs lack secrets and may run untrusted code | Deploy from `push` or manual dispatch |

## Try it

1. Create `staging` and `production` environments, with a required reviewer on production, and run the staged pipeline above with a placeholder `deploy.sh`
2. Add a smoke test between the two deploys and break it on purpose to confirm production is blocked
3. Add the tag-based release workflow, push `v0.1.0`, and look at the generated release

## Key takeaways

- CD builds once and promotes the same artifact through environments
- Environments supply approvals, secrets, and deployment history
- Use `concurrency` to serialize deploys, smoke tests to verify them, tags to roll back
- Choose triggers deliberately: `push` to `main`, tags, releases, or manual dispatch
- Prefer OIDC over stored cloud credentials

**Next:** [Docker CI/CD](./16_docker-ci-cd.md)
