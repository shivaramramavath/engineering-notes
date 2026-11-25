---
title: "GitHub Releases and GitHub Packages (GHCR)"
category: "04-devops"
subcategory: "git-github / 05-github-collaboration"
tags: ["github", "github-releases", "github-packages", "ghcr", "docker", "npm", "package-registry"]
difficulty: "Intermediate-Advanced"
estimated_read_time: "24 min"
prerequisites:
  - "02-commands/12-tags-and-releases"
  - "04-github-actions/19-release-automation"
last_updated: "2026-11-25"
---

# GitHub Releases and GitHub Packages (GHCR)

> **Navigation**: [← Previous: 09-github-pages-and-documentation.md](file:///e:/Learning/engineering-notes/04-devops/git-github/05-github-collaboration/09-github-pages-and-documentation.md) | [Module README](file:///e:/Learning/engineering-notes/04-devops/git-github/05-github-collaboration/README.md) | [Next: 11-organisations-teams-and-permissions.md →](file:///e:/Learning/engineering-notes/04-devops/git-github/05-github-collaboration/11-organisations-teams-and-permissions.md)

---

## Table of Contents
1. [What & Why](#1-what--why)
2. [Git Tags vs GitHub Releases](#2-git-tags-vs-github-releases)
3. [GitHub Packages Ecosystem](#3-github-packages-ecosystem)
4. [GitHub Container Registry (GHCR) In-Depth](#4-github-container-registry-ghcr-in-depth)
5. [Publishing npm, Maven & NuGet Packages](#5-publishing-npm-maven--nuget-packages)
6. [Authentication: GITHUB_TOKEN vs PAT](#6-authentication-github_token-vs-pat)
7. [Practical Examples](#7-practical-examples)
8. [Best Practices](#8-best-practices)
9. [Common Mistakes](#9-common-mistakes)
10. [Comparison Matrix: GitHub Packages vs Artifactory vs Docker Hub](#10-comparison-matrix-github-packages-vs-artifactory-vs-docker-hub)
11. [Interview Questions & Answers](#11-interview-questions--answers)
12. [Practice Exercises](#12-practice-exercises)
13. [Summary & Cheat Sheet](#13-summary--cheat-sheet)
14. [Further Reading](#14-further-reading)

---

## 1. What & Why

### What are GitHub Releases & GitHub Packages?
- **GitHub Releases**: Deployable software delivery milestones tied to Git tags. They include human-readable release notes, changelogs, binary assets (tarballs, installers, firmware), and cryptographic checksums hosted on GitHub's global CDN.
- **GitHub Packages (including GHCR)**: A private or public software package hosting service supporting container images (Docker/OCI via `ghcr.io`) and ecosystem package managers (npm, Apache Maven, Gradle, NuGet, RubyGems).

### Why Do We Use Them?
1. **Unified Developer Platform**: Keep source code, issues, CI/CD builds, release artifacts, and container registries in a single authenticated ecosystem.
2. **Seamless Permissions**: Package access is governed by the same organization teams, repository permissions, and SSO controls as the source code.
3. **Internal Package Sharing**: Share private internal libraries across corporate microservices without paying for external third-party package registries (like Artifactory or Cloudsmith).
4. **Native Container Hosting (GHCR)**: High-speed OCI image registry tightly integrated with GitHub Actions runners for fast layer caching.

---

## 2. Git Tags vs GitHub Releases

```
┌─────────────────────────────────────────────────────────────────────────┐
│ Comparison: Git Tag vs GitHub Release                                   │
├──────────────────────────┬──────────────────────┬───────────────────────┤
│ Dimension                │ Git Tag              │ GitHub Release        │
├──────────────────────────┼──────────────────────┼───────────────────────┤
│ Storage Layer            │ Local Git Object     │ GitHub Database & CDN │
│ Formatted Release Notes  │ Raw tag message only │ Rich Markdown & HTML  │
│ Binary Assets            │ Not supported        │ Up to 2GB per asset   │
│ Checksums & SBOMs        │ Not supported        │ Downloadable assets   │
│ Mark as Pre-release      │ No                   │ Yes                   │
│ Mark as Latest           │ Implicitly highest   │ Explicit toggle       │
└──────────────────────────┴──────────────────────┴───────────────────────┘
```

---

## 3. GitHub Packages Ecosystem

GitHub Packages provides a unified multi-ecosystem package registry:

| Ecosystem | Registry Hostname | Standard Config File | Package Scoping Rule |
|:---|:---|:---|:---|
| **Docker / OCI** | `ghcr.io` | `docker login ghcr.io` | `ghcr.io/owner/image:tag` |
| **npm** | `npm.pkg.github.com` | `.npmrc` | `@owner/package-name` |
| **Maven** | `maven.pkg.github.com` | `pom.xml` / `settings.xml` | `com.example.owner` |
| **NuGet** | `nuget.pkg.github.com` | `nuget.config` | `Owner.PackageName` |
| **RubyGems** | `rubygems.pkg.github.com` | `~/.gem/credentials` | Gem name |

---

## 4. GitHub Container Registry (GHCR) In-Depth

`ghcr.io` replaces the legacy Docker package registry and provides:
- Anonymous public image pulls (for public images).
- Fine-grained container image permissions independent of the source repository.
- Native multi-architecture manifests (`linux/amd64`, `linux/arm64`).
- Support for OCI artifacts (Helm charts, WebAssembly, Cosign signatures).

### Authentication with GHCR:
```bash
# Log in via CLI using personal access token (classic or fine-grained with read:packages)
echo "$CR_PAT" | docker login ghcr.io -u "$USERNAME" --password-stdin
```

---

## 5. Publishing npm, Maven & NuGet Packages

### Scoped npm Packages (`.npmrc`)
To publish or consume private npm packages from GitHub:
```ini
# .npmrc
@my-org:registry=https://npm.pkg.github.com
//npm.pkg.github.com/:_authToken=${NODE_AUTH_TOKEN}
```
In `package.json`:
```json
{
  "name": "@my-org/shared-utils",
  "version": "1.2.0",
  "publishConfig": {
    "registry": "https://npm.pkg.github.com"
  }
}
```

---

## 6. Authentication: GITHUB_TOKEN vs PAT

### Inside GitHub Actions:
The built-in `GITHUB_TOKEN` has native access to GitHub Packages:
```yaml
permissions:
  packages: write
  contents: read

steps:
  - uses: docker/login-action@v3
    with:
      registry: ghcr.io
      username: ${{ github.actor }}
      password: ${{ secrets.GITHUB_TOKEN }}
```

### Outside GitHub Actions (Local Developer / Production Cluster):
You must use a **Personal Access Token (PAT)** or a **GitHub App Token** with scopes:
- `read:packages`: Pull private images/packages.
- `write:packages`: Publish new packages.
- `delete:packages`: Delete packages.

---

## 7. Practical Examples

### Example 1: Building and Pushing a Docker Image to GHCR in CI
```yaml
# .github/workflows/docker-publish.yml
name: Publish Container to GHCR
on:
  push:
    tags: ["v*.*.*"]

permissions:
  contents: read
  packages: write

jobs:
  docker:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v4

      - name: Log in to GitHub Container Registry
        uses: docker/login-action@v3
        with:
          registry: ghcr.io
          username: ${{ github.actor }}
          password: ${{ secrets.GITHUB_TOKEN }}

      - name: Build and Push OCI Image
        uses: docker/build-push-action@v5
        with:
          context: .
          push: true
          tags: |
            ghcr.io/${{ github.repository }}:${{ github.ref_name }}
            ghcr.io/${{ github.repository }}:latest
```

### Example 2: Creating a GitHub Release with Assets via GitHub CLI
```bash
# Compile binary
go build -o dist/mycli main.go
tar -czf dist/mycli-v1.4.0-linux-amd64.tar.gz -C dist mycli
sha256sum dist/*.tar.gz > dist/SHA256SUMS.txt

# Create release, attach assets, and generate release notes automatically
gh release create v1.4.0 dist/* \
  --title "v1.4.0 Production Release" \
  --generate-notes
```

### Example 3: Pulling Private GHCR Image in Kubernetes (`imagePullSecrets`)
```bash
# 1. Create docker-registry secret in Kubernetes
kubectl create secret docker-registry ghcr-secret \
  --docker-server=ghcr.io \
  --docker-username=my-service-account \
  --docker-password=$GHCR_TOKEN \
  --docker-email=ops@my-org.com

# 2. Reference in pod manifest:
# spec.imagePullSecrets:
#   - name: ghcr-secret
```

---

## 8. Best Practices

1. **Always Use `packages: write` at Job Level**: Restrict package write tokens strictly to the publishing job in CI.
2. **Scope npm Packages to Organization**: npm packages on GitHub Packages **must** be scoped to the repository owner or organization (`@org/name`).
3. **Publish Multi-Arch Images on GHCR**: Use Docker Buildx to compile and push images for both `linux/amd64` and `linux/arm64`.
4. **Publish Checksums (`SHA256SUMS.txt`) with Every Release**: Enables downstream users to verify asset integrity.
5. **Establish Retention & Pruning Policies**: Container images consume organization storage quotas. Regularly delete untagged, ephemeral PR images via GitHub REST API or lifecycle actions.

---

## 9. Common Mistakes

| Mistake | Consequence | Root Cause | Fix / Prevention |
|:---|:---|:---|:---|
| Unscoped npm package name | `npm publish` fails with 400 Bad Request | Package not prefixed with `@org/` | Use `@owner/package-name` in package.json |
| Missing `packages: write` | `docker push` fails with 403 Forbidden | `GITHUB_TOKEN` lacks package write permission | Add `permissions: packages: write` to job |
| Exceeding storage quotas | Surprise billing charges on private repos | Retaining gigabytes of obsolete container layers | Configure automated package pruning |
| Deleting release deletes tag? | Deleting release leaves Git tag intact on remote | Release is a platform metadata layer | Delete tag explicitly: `git push origin --delete <tag>` |
| Storing compiled binaries in Git | Massive repo bloat | Putting `.tar.gz` in git instead of Releases | Upload assets via `gh release upload` |

---

## 10. Comparison Matrix: GitHub Packages vs Artifactory vs Docker Hub

| Dimension | GitHub Packages (GHCR) | JFrog Artifactory | Docker Hub |
|:---|:---|:---|:---|
| **Platform Integration** | **Native GitHub (SSO, IAM)** | Standalone Enterprise Server | Standalone Cloud Service |
| **Supported Formats** | Containers, npm, Maven, NuGet, Gems | 30+ formats (All packages) | Containers only |
| **Public Image Pulls** | Free / Anonymous | Depends on licensing | Rate-limited (100–200 pulls) |
| **Management Overhead** | **Zero** | Heavy (self-hosted / managed) | Minimal |
| **Pricing** | Included in GitHub allowance | Expensive enterprise license | Per-user / per-seat |

---

## 11. Interview Questions & Answers

<details>
<summary><strong>Q1: What is the relationship between a Git Tag and a GitHub Release?</strong></summary>

A **Git Tag** is a pointer in the local and remote Git object store that marks a specific commit as immutable (lightweight or annotated).
A **GitHub Release** is a platform-level feature built on top of a Git tag. It associates rich markdown release notes, changelog attributions, pre-release flags, and binary downloadable assets (up to 2 GB per asset) hosted on GitHub's content delivery network. A release requires a tag, but a tag does not require a release.
</details>

<details>
<summary><strong>Q2: Why must npm packages published to GitHub Packages be scoped?</strong></summary>

GitHub Packages uses a multi-tenant registry architecture where all users share the same domain (`npm.pkg.github.com`). To prevent package namespace collisions and squatting, GitHub enforces that every package must be prefixed with the organization or user account name (e.g. `@my-org/auth-lib`).
</details>

<details>
<summary><strong>Q3: How does authentication differ when pulling a GHCR container image inside GitHub Actions versus outside on a production server?</strong></summary>

- **Inside GitHub Actions**: The workflow uses the built-in ephemeral `secrets.GITHUB_TOKEN` with `permissions: packages: read` or `packages: write`.
- **Outside GitHub Actions (Production)**: External systems cannot access `GITHUB_TOKEN`. You must generate a Fine-Grained Personal Access Token (PAT) or use a GitHub App with the `read:packages` scope, configured as a Kubernetes `imagePullSecret` or Docker credential.
</details>

<details>
<summary><strong>Q4: Are public container images hosted on `ghcr.io` subject to rate-limiting on pulls?</strong></summary>

Unlike Docker Hub, which severely rate-limits anonymous image pulls (100 pulls per 6 hours per IP), GitHub Container Registry (`ghcr.io`) offers **free, unauthenticated, and un-rate-limited public container pulls** for public repositories, making it ideal for open-source container distribution.
</details>

<details>
<summary><strong>Q5: Can you delete a package version on GitHub Packages? What precautions exist?</strong></summary>

Yes, users with Admin access or tokens with `delete:packages` can delete specific package versions or entire packages. However, to protect supply chain stability, once a package version is deleted, GitHub prevents reusing that exact same version string in subsequent publishes to avoid cache poisoning attacks.
</details>

<details>
<summary><strong>Q6: How do you link a published GHCR container image to its source repository?</strong></summary>

By adding the standard OCI label in your `Dockerfile`:
```dockerfile
LABEL org.opencontainers.image.source="https://github.com/my-org/my-repo"
```
GitHub automatically recognizes this label and links the container image directly to the repository homepage.
</details>

<details>
<summary><strong>Q7: What is the maximum file size for an individual release asset uploaded to a GitHub Release?</strong></summary>

An individual release asset can be up to **2 GB** in size. There is no hard limit on the total number of assets attached to a release, though total repository asset bandwidth is monitored.
</details>

<details>
<summary><strong>Q8: What happens to a GitHub Package if the parent repository is deleted?</strong></summary>

Packages that were published to a repository inherit that repository's visibility. If the repository is deleted, the associated packages may become orphaned or deleted depending on the package type. For GHCR container images, images can be detached and managed at the organization level to ensure continuous container availability even if code repos are refactored.
</details>

---

## 12. Practice Exercises

<details>
<summary><strong>Exercise 1: Create a Release with Binary via GitHub CLI</strong></summary>

**Scenario**: Tag a commit and create a GitHub Release attaching a text asset.
```bash
echo "Release binary artifact" > app.bin
gh release create v1.0.0 app.bin --title "v1.0.0" --notes "Initial public release"
```
</details>

<details>
<summary><strong>Exercise 2: Log in to GHCR via CLI</strong></summary>

**Scenario**: Authenticate with GitHub Container Registry.
```bash
echo "$CR_PAT" | docker login ghcr.io -u "$GITHUB_USER" --password-stdin
```
</details>

<details>
<summary><strong>Exercise 3: Tag and Push a Docker Image to GHCR</strong></summary>

**Scenario**: Build and push a local image to GHCR.
```bash
docker tag my-local-app:latest ghcr.io/my-org/my-repo/app:1.0.0
docker push ghcr.io/my-org/my-repo/app:1.0.0
```
</details>

<details>
<summary><strong>Exercise 4: Download Release Asset via GitHub CLI</strong></summary>

**Scenario**: Download `app.bin` from release `v1.0.0`.
```bash
gh release download v1.0.0 -p "app.bin"
```
</details>

<details>
<summary><strong>Exercise 5: Configure `.npmrc` for GitHub Packages</strong></summary>

**Scenario**: Setup `.npmrc` to pull `@my-org` packages from GitHub registry.
```ini
@my-org:registry=https://npm.pkg.github.com
```
</details>

<details>
<summary><strong>Exercise 6: Query Releases via GitHub CLI</strong></summary>

**Scenario**: List all releases in repository.
```bash
gh release list
```
</details>

---

## 13. Summary & Cheat Sheet

```bash
# GitHub CLI Release Commands
gh release create <tag> [files...]     # Create release and upload assets
gh release list                        # List releases
gh release view <tag>                  # View release notes
gh release download <tag>              # Download all assets
gh release delete <tag> --yes          # Delete release (leaves tag)

# GitHub Container Registry (GHCR)
docker login ghcr.io -u <user> --password-stdin
docker tag <img> ghcr.io/<owner>/<repo>/<img>:<tag>
docker push ghcr.io/<owner>/<repo>/<img>:<tag>
docker pull ghcr.io/<owner>/<repo>/<img>:<tag>
```

---

## 14. Further Reading
- [GitHub Docs - About Releases](https://docs.github.com/en/repositories/releasing-projects-on-github/about-releases)
- [Working with the Container Registry](https://docs.github.com/en/packages/working-with-a-github-packages-registry/working-with-the-container-registry)
- [Working with the npm Registry](https://docs.github.com/en/packages/working-with-a-github-packages-registry/working-with-the-npm-registry)
- [05-github-collaboration/11-organisations-teams-and-permissions.md](file:///e:/Learning/engineering-notes/04-devops/git-github/05-github-collaboration/11-organisations-teams-and-permissions.md)
