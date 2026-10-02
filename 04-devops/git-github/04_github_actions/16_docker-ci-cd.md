# Docker CI/CD

A Docker pipeline in Actions builds an image from your `Dockerfile`, tests it, tags it sensibly, and pushes it to a **registry** such as GitHub Container Registry (GHCR) or Docker Hub. From there, a deploy job or your platform pulls it.

```
Dockerfile  →  build (buildx)  →  test  →  tag  →  push to registry  →  deploy
```

## The simplest Docker build

```yaml
name: Docker

on: [push]

jobs:
  build:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v4
      - run: docker build -t my-app:test .
      - run: docker run --rm my-app:test --version
```

Docker is preinstalled on GitHub-hosted Linux runners, so plain `docker` commands work. For caching, multi-platform builds, and clean tagging, use the official Docker actions below.

## The official Docker actions

| Action | Purpose |
|--------|---------|
| `docker/setup-buildx-action` | Sets up Buildx, the modern builder with caching and multi-platform support |
| `docker/login-action` | Logs in to a registry |
| `docker/metadata-action` | Generates image tags and labels from Git context |
| `docker/build-push-action` | Builds and optionally pushes the image |
| `docker/setup-qemu-action` | Emulation for building other CPU architectures |

## A complete build-and-push workflow to GHCR

```yaml
name: Docker image

on:
  push:
    branches: [main]
    tags: ["v*.*.*"]
  pull_request:

env:
  IMAGE: ghcr.io/${{ github.repository }}

jobs:
  docker:
    runs-on: ubuntu-latest
    permissions:
      contents: read
      packages: write           # push to GHCR with GITHUB_TOKEN
    steps:
      - uses: actions/checkout@v4

      - uses: docker/setup-buildx-action@v3

      - name: Log in to GHCR
        if: github.event_name != 'pull_request'
        uses: docker/login-action@v3
        with:
          registry: ghcr.io
          username: ${{ github.actor }}
          password: ${{ secrets.GITHUB_TOKEN }}

      - id: meta
        uses: docker/metadata-action@v5
        with:
          images: ${{ env.IMAGE }}
          tags: |
            type=ref,event=branch
            type=ref,event=pr
            type=semver,pattern={{version}}
            type=semver,pattern={{major}}.{{minor}}
            type=sha

      - uses: docker/build-push-action@v6
        with:
          context: .
          push: ${{ github.event_name != 'pull_request' }}
          tags: ${{ steps.meta.outputs.tags }}
          labels: ${{ steps.meta.outputs.labels }}
          cache-from: type=gha
          cache-to: type=gha,mode=max
```

The full file is in [`examples/docker-build-push.yml`](./examples/docker-build-push.yml). Check each Docker action's repository for its latest major version.

What happens on each event:

| Event | Build | Push | Tags produced (examples) |
|-------|-------|------|--------------------------|
| Pull request | Yes | **No** | `pr-12` |
| Push to `main` | Yes | Yes | `main`, `sha-ab12cd3` |
| Push tag `v1.4.0` | Yes | Yes | `1.4.0`, `1.4`, `sha-ab12cd3` |

Building on pull requests proves the Dockerfile works without publishing anything.

## Registries

| Registry | Login | Notes |
|----------|-------|-------|
| **GHCR** (`ghcr.io`) | `GITHUB_TOKEN` with `packages: write` | Integrated with the repository, no extra secret |
| **Docker Hub** | Username and access token as secrets | Use an access token, not your password |
| **AWS ECR, Google Artifact Registry, Azure ACR** | OIDC or provider login action | Best with short-lived credentials (chapter 17) |

```yaml
# Docker Hub
- uses: docker/login-action@v3
  with:
    username: ${{ vars.DOCKERHUB_USERNAME }}
    password: ${{ secrets.DOCKERHUB_TOKEN }}
```

Image names must be **lowercase**. `docker/metadata-action` normalizes the name for you; if you build the tag by hand, lowercase it yourself (`${GITHUB_REPOSITORY,,}` in bash).

## Tagging strategy

| Tag | Mutable? | Use for |
|-----|----------|---------|
| `sha-<short>` | No | Traceability; deploy exactly this commit |
| `1.4.0` | No | Released versions |
| `1.4` / `1` | Yes | Users who accept patch or minor updates |
| `main` | Yes | The latest build from `main`, for staging |
| `latest` | Yes | Convention for the latest stable release; avoid deploying production by it |

Rule: **deploy by immutable tag or digest**, never by a floating tag.

## Layer caching

Docker builds are slow because each layer is rebuilt. Caching reuses unchanged layers across runs.

```yaml
- uses: docker/build-push-action@v6
  with:
    context: .
    push: true
    tags: ${{ steps.meta.outputs.tags }}
    cache-from: type=gha
    cache-to: type=gha,mode=max
```

| Cache backend | Description |
|---------------|-------------|
| `type=gha` | Stores layers in the Actions cache; easy, but shares the repository's cache quota (chapter 09) |
| `type=registry,ref=ghcr.io/org/app:buildcache` | Stores cache as an image in the registry; larger and shared across runners |
| `mode=max` | Caches intermediate layers too, so better hit rates and larger size |

A cache only helps if the Dockerfile is written to use it.

### A cache-friendly Dockerfile

```dockerfile
# syntax=docker/dockerfile:1
FROM node:20-slim AS deps
WORKDIR /app
COPY package.json package-lock.json ./      # copy dependency files first
RUN npm ci                                  # cached until lockfile changes

FROM node:20-slim AS build
WORKDIR /app
COPY --from=deps /app/node_modules ./node_modules
COPY . .                                    # source changes invalidate only from here
RUN npm run build

FROM node:20-slim AS runtime
WORKDIR /app
ENV NODE_ENV=production
COPY --from=build /app/dist ./dist
COPY --from=deps /app/node_modules ./node_modules
USER node
CMD ["node", "dist/index.js"]
```

| Technique | Benefit |
|-----------|---------|
| Copy lockfiles before source | Dependency layer is reused while only code changes |
| Multi-stage builds | Small final image without build tools |
| `.dockerignore` (`node_modules`, `.git`, `*.log`) | Smaller context, fewer cache invalidations |
| Pinned base image tags or digests | Reproducible builds |
| `USER` non-root | Safer runtime |

## Multi-platform images

```yaml
- uses: docker/setup-qemu-action@v3
- uses: docker/setup-buildx-action@v3
- uses: docker/build-push-action@v6
  with:
    platforms: linux/amd64,linux/arm64
    push: true
    tags: ${{ steps.meta.outputs.tags }}
```

One tag then works on both Intel and ARM machines. Emulated builds are slower; on large projects, building each platform on a native runner and merging the manifests is faster.

## Testing the image before pushing

```yaml
- name: Build for testing
  uses: docker/build-push-action@v6
  with:
    context: .
    load: true                 # make the image available to local docker
    tags: my-app:test
    cache-from: type=gha

- name: Run tests in the container
  run: docker run --rm my-app:test npm test

- name: Smoke test the running container
  run: |
    docker run -d --name app -p 8080:8080 my-app:test
    for i in {1..10}; do curl --fail http://localhost:8080/health && break || sleep 3; done
    docker logs app
    docker rm -f app
```

Only after tests pass, run the push step (the layer cache makes the second build almost free).

## Scanning and provenance

```yaml
- name: Scan image for vulnerabilities
  uses: aquasecurity/trivy-action@<pinned-sha>      # pin third-party actions (chapter 17)
  with:
    image-ref: my-app:test
    severity: CRITICAL,HIGH
    exit-code: "1"
```

| Practice | Benefit |
|----------|---------|
| Vulnerability scan (Trivy, Grype, Docker Scout) | Catch known CVEs before shipping |
| Build provenance and SBOM (`provenance: true`, `sbom: true` in `build-push-action`, or `actions/attest-build-provenance`) | Verifiable record of how the image was built |
| Sign images (cosign) | Consumers can verify authenticity |

Provenance attestations need `id-token: write` and `attestations: write`.

## Deploying the image

```yaml
deploy:
  needs: docker
  runs-on: ubuntu-latest
  environment: production
  steps:
    - run: |
        ssh deploy@"$HOST" "docker pull $IMAGE:sha-${GITHUB_SHA::7} && \
                            docker compose up -d"
      env:
        HOST: ${{ vars.DEPLOY_HOST }}
        IMAGE: ghcr.io/${{ github.repository }}
```

Whatever the target (Compose on a server, Kubernetes, ECS, Cloud Run), the pattern is the same: **pull or reference the exact immutable tag the build job just pushed**.

## Secrets and Docker builds

| Do | Don't |
|----|-------|
| Use BuildKit secret mounts: `secrets:` input on `build-push-action` and `RUN --mount=type=secret` | Pass secrets with `--build-arg` or `ENV`; they remain in image layers and history |
| Use `GITHUB_TOKEN` for GHCR | Store a personal access token if `GITHUB_TOKEN` suffices |
| Scope registry tokens to push only | Use full account tokens |

```yaml
- uses: docker/build-push-action@v6
  with:
    secrets: |
      npm_token=${{ secrets.NPM_TOKEN }}
```

```dockerfile
RUN --mount=type=secret,id=npm_token \
    NPM_TOKEN=$(cat /run/secrets/npm_token) npm ci
```

## Mental model checklist

- Buildx builds, metadata-action names, login-action authenticates, build-push-action ships
- Build on every PR, **push only** from trusted events
- Cache layers and write the Dockerfile to benefit from it
- Tag with SHA and semver; deploy by immutable tag
- Scan, attest, and never bake secrets into layers

## Common questions

| Question | Answer |
|----------|--------|
| Do I need Buildx? | For caching and multi-platform builds, yes; the actions set it up for you |
| Why does my push fail with "denied"? | Missing `packages: write`, wrong login, or the package is linked to another repository |
| Why is my image name rejected? | Uppercase letters in the repository name; image names must be lowercase |
| Can I use Docker Compose in CI? | Yes, `docker compose up -d` works on hosted Linux runners |
| How big can the GHA cache be? | It counts against the repository cache quota (see chapter 09), so use a registry cache for large images |
| How do I make a GHCR image public? | Change the package visibility in the package settings |
| Can I build on macOS or Windows runners? | Docker is available only on Linux runners by default |
| How do I find the digest I just pushed? | `steps.<id>.outputs.digest` from `build-push-action` |

## Pitfalls

| Pitfall | Why it hurts | Better |
|---------|--------------|--------|
| Pushing from `pull_request` | Fork PRs lack permissions, and you may publish untrusted builds | `push: ${{ github.event_name != 'pull_request' }}` |
| Only a `latest` tag | No rollback or traceability | Add `sha-` and version tags |
| `COPY . .` before installing dependencies | Cache invalidated on every change | Copy lockfiles first |
| Secrets passed as `ARG` or `ENV` | Stored in image layers | BuildKit secret mounts |
| Forgetting `packages: write` | Push to GHCR fails with 403 | Add it to the job |
| Uppercase repository name in the tag | Invalid reference | Use `metadata-action` or lowercase it |
| Huge build context | Slow uploads, cache misses | `.dockerignore` |
| Running containers as root | Larger blast radius | `USER` non-root |
| Deploying `:latest` | You cannot tell what is running | Deploy by SHA or digest |

## Try it

1. Add a `Dockerfile` and the GHCR workflow to a test repository, open a PR (build only), then merge and look at **Packages**
2. Push a tag `v0.1.0` and confirm the semver tags appear
3. Run the pipeline twice and compare build times with and without `cache-from` and `cache-to`

## Key takeaways

- Use the official Docker actions: buildx, login, metadata, build-push
- Build on every PR, but push only on trusted events with the minimum permissions
- Combine layer caching with a cache-friendly, multi-stage Dockerfile
- Tag with SHA and semver, deploy immutable tags
- Scan images, generate provenance, and keep secrets out of layers

**Next:** [Security Hardening](./17_security-hardening.md)
