# Docker in CI/CD

The last piece: building and pushing images automatically, instead of running `docker build`/`docker push` by hand every time you ship a change.

```
Push code → CI builds image → CI pushes image to a registry → deployment pulls it
```

---

## The basic pipeline

Every CI/CD setup that builds Docker images does roughly the same three things:

1. **Build** the image from the Dockerfile
2. **Tag** it meaningfully (not just `latest`)
3. **Push** it to a registry the deployment target can pull from

```bash
docker build -t myuser/myapp:abc1234 .
docker push myuser/myapp:abc1234
```

CI just automates running these commands on every push, with the repeatability and consistency a human typing commands by hand can't guarantee.

---

## Example: GitHub Actions

```yaml
# .github/workflows/docker-publish.yml
name: Build and Push Docker Image

on:
  push:
    branches: [main]

jobs:
  build-and-push:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v4

      - name: Log in to Docker Hub
        uses: docker/login-action@v3
        with:
          username: ${{ secrets.DOCKERHUB_USERNAME }}
          password: ${{ secrets.DOCKERHUB_TOKEN }}

      - name: Build and push
        uses: docker/build-push-action@v6
        with:
          context: .
          push: true
          tags: |
            myuser/myapp:latest
            myuser/myapp:${{ github.sha }}
```

Notice the registry credentials come from `secrets.DOCKERHUB_TOKEN` — GitHub's own encrypted secret store, injected as an environment variable at build time, never committed to the repo. This is the CI equivalent of the "use the platform's secret mechanism" principle from `01_environments-secrets.md`.

---

## Tagging strategy: don't rely only on `latest`

```yaml
tags: |
  myuser/myapp:latest
  myuser/myapp:${{ github.sha }}
```

Tagging with the Git commit SHA (or a version number, or the CI run ID) alongside `latest` means:

- You can always trace exactly which commit produced a given running image
- Rolling back means deploying a previous, still-existing tag — not trying to rebuild an old commit and hope it produces identical output
- `latest` moving forward doesn't silently break anything already deployed against a pinned SHA tag

A common pattern for releases:

```yaml
tags: |
  myuser/myapp:latest
  myuser/myapp:${{ github.sha }}
  myuser/myapp:v1.4.0
```

---

## Using layer caching in CI

Without caching, every CI run rebuilds every layer from scratch — slow, and wasteful given how much of a typical rebuild (base image, unchanged dependencies) doesn't actually need to change (see `03_dockerfile/02_layers-caching.md`).

```yaml
- name: Build and push
  uses: docker/build-push-action@v6
  with:
    context: .
    push: true
    tags: myuser/myapp:${{ github.sha }}
    cache-from: type=gha
    cache-to: type=gha,mode=max
```

`cache-from`/`cache-to` with `type=gha` uses GitHub Actions' own cache storage to persist Docker layers between CI runs, so an unchanged dependency-install layer doesn't have to be redone on every single build.

---

## A typical full flow, end to end

```
Developer pushes to main
        ↓
CI checks out code
        ↓
CI logs into the registry (using a secret, not a hardcoded credential)
        ↓
CI builds the image (using layer caching)
        ↓
CI tags it: latest + commit SHA (+ version, on a release)
        ↓
CI pushes to the registry
        ↓
Deployment platform (Kubernetes, ECS, a server running Compose, etc.)
pulls the new image and rolls it out
```

---

## Security scanning as part of the pipeline

Many pipelines add an image vulnerability scan before pushing, to catch known CVEs in the base image or dependencies:

```yaml
- name: Scan image
  uses: aquasecurity/trivy-action@master
  with:
    image-ref: myuser/myapp:${{ github.sha }}
    severity: CRITICAL,HIGH
```

Failing the build (or at least flagging it) on critical vulnerabilities catches problems before they reach production, rather than discovering them after deployment.

---

## Other CI platforms (the same idea, different syntax)

The three-step build/tag/push flow, and the "credentials come from the platform's secret store" principle, apply the same way in:

- **GitLab CI** — `docker build`/`docker push` inside a job, credentials from GitLab CI/CD variables
- **CircleCI** — similar, using CircleCI's environment variable/context system
- **Jenkins** — via the Docker Pipeline plugin, credentials from Jenkins' credential store

The specific YAML/Groovy syntax differs, but you're always looking for the same three ingredients: build, tag meaningfully, push using securely-stored credentials.

## Quick summary

- CI automates build → tag → push, the same three commands you'd otherwise run by hand
- Tag with something traceable (commit SHA, version) in addition to `latest`, so you can always identify and roll back to a specific build
- Use the CI platform's layer caching (e.g. `type=gha` on GitHub Actions) so unchanged layers aren't rebuilt every run
- Registry credentials always come from the CI platform's secret store — never hardcoded in the pipeline file
- Consider adding an image vulnerability scan as a pipeline step before deployment

## Guide complete

This closes out the entire Docker guide — setup, fundamentals, day-to-day commands, the Dockerfile, volumes and networking, Compose, and now production concerns including automated builds. From here, the best next step is applying this to a real project, coming back to specific sections as reference whenever something doesn't behave as expected.
