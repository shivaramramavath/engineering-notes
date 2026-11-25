---
title: "Software Supply Chain Security, SBOM & Build Attestations"
category: "04-devops"
subcategory: "git-github / 07-github-security"
tags: ["git", "github", "sbom", "supply-chain", "slsa", "attestations", "sigstore", "spdx", "cyclonedx"]
---

# Software Supply Chain Security, SBOM & Build Attestations

## Table of Contents
- [1. What & Why](#1-what--why)
- [2. Core Concepts](#2-core-concepts)
- [3. How It Works Internally](#3-how-it-works-internally)
- [4. Syntax & Usage](#4-syntax--usage)
- [5. Practical Examples](#5-practical-examples)
- [6. Best Practices](#6-best-practices)
- [7. Common Mistakes](#7-common-mistakes)
- [8. Performance, Security & Cost Considerations](#8-performance-security--cost-considerations)
- [9. Comparison Table](#9-comparison-table)
- [10. Real-World Interview Questions](#10-real-world-interview-questions)
- [11. Hands-On Exercises](#11-hands-on-exercises)
- [12. Summary / Cheat Sheet](#12-summary--cheat-sheet)
- [13. Further Reading](#13-further-reading)

---

## 1. What & Why

High-profile cyberattacks like SolarWinds, Codecov, and Log4j demonstrated that attacking an organization's software supply chain—compromised build pipelines, hijacked open-source packages, or tampered release binaries—is far more effective than breaching hardened production perimeter firewalls. In response, modern security compliance standards (such as US Executive Order 14028 and NIST SP 800-218) mandate cryptographic transparency throughout the software development lifecycle.

Securing the supply chain requires three pillars:
1. **Software Bill of Materials (SBOM)**: A comprehensive, machine-readable inventory of every library, package, compiler, and transitive dependency included in a software product.
2. **SLSA Framework (Supply-chain Levels for Software Artifacts)**: An industry standard defining guidelines to ensure artifact integrity and prevent tampering during source, build, and packaging stages.
3. **Artifact Attestations & Provenance**: Cryptographic proofs generated during GitHub Actions workflows that bind a built binary or Docker container to its exact source commit, workflow definition, and runner identity using keyless signing (Sigstore).

Without verifiable provenance and SBOMs, downstream consumers have no mathematical guarantee that the binary they download matches the source code audited in Git.

---

## 2. Core Concepts

### SBOM Standards: SPDX vs CycloneDX
- **SPDX (Software Package Data Exchange)**: An ISO standard (ISO/IEC 5962:2021) focused on open-source licensing compliance, package inventories, and copyright tracking.
- **CycloneDX**: An OWASP standard specifically optimized for DevSecOps, application security, vulnerability identification, and software-as-a-service bills of materials (SaaS-BOM).

### SLSA (Supply-chain Levels for Software Artifacts)
SLSA defines four maturity levels:
- **SLSA Build Level 1**: Scripted build process emitting unauthenticated provenance.
- **SLSA Build Level 2**: Hosted build service (e.g., GitHub Actions) generating service-authenticated provenance preventing tampering.
- **SLSA Build Level 3**: Isolated, ephemeral build environments with non-falsifiable provenance and hardened hermetic dependencies.

### GitHub Artifact Attestations (`actions/attest-build-provenance`)
GitHub's native attestation engine uses Sigstore to cryptographically sign build provenance. During a GitHub Actions workflow, the runner requests an ephemeral OIDC identity token from GitHub, signs an **in-toto** statement binding the artifact SHA to the repo commit and workflow run, and publishes the attestation to GitHub's transparency log.

### Dependency Pinning
- **Tag Pinning (`actions/checkout@v4`)**: Human-readable, but vulnerable to tag mutation if a maintainer account is compromised.
- **SHA Pinning (`actions/checkout@b4ffde...`)**: Cryptographically immutable; guarantees the exact code being executed cannot change without an explicit commit change.

---

## 3. How It Works Internally

```
GitHub Actions Runner Environment                      GitHub Transparency Service
+----------------------------------------+            +----------------------------------+
| 1. Build Binary or Docker Image        |            |                                  |
|    - Computes Digest: sha256:4a3b...   |            |                                  |
+----------------------------------------+            |                                  |
                   |                                  |                                  |
                   v                                  |                                  |
+----------------------------------------+            |                                  |
| 2. Request OIDC Token (actions/attest) |            |                                  |
|    - Contains claims: repo, commit,    |            |                                  |
|      workflow, actor, runner environment|           |                                  |
+----------------------------------------+            |                                  |
                   |                                  |                                  |
                   v                                  |                                  |
+----------------------------------------+            |                                  |
| 3. Ephemeral Sigstore Signing          |            |                                  |
|    - Fulcio issues short-lived cert    |            |                                  |
|    - Generates in-toto attestation JSON|            |                                  |
+----------------------------------------+            |                                  |
                   |                                  |                                  |
                   v                                  v                                  |
+----------------------------------------+   Upload   +----------------------------------+
| 4. Publish Release Binary & SBOM       | =========> | 5. Record Attestation in Public /|
+----------------------------------------+            |    Enterprise Transparency Log   |
                                                      +----------------------------------+
                                                                       |
Consumer / Downstream Verifier                                         |
+----------------------------------------------------------------------+
| 6. Verification (gh attestation verify binary.tar.gz --owner my-org) |
|    - Validates binary hash against in-toto attestation               |
|    - Cryptographically proves binary was built by GitHub Actions      |
+----------------------------------------------------------------------+
```

---

## 4. Syntax & Usage

Generating an official SPDX SBOM for a repository using GitHub CLI:
```bash
# Export repository SBOM in SPDX JSON format
gh api -H "Accept: application/vnd.github+json" \
  /repos/my-org/payment-service/dependency-graph/sbom \
  --jq '.sbom' > sbom.spdx.json
```

Verifying an artifact attestation using GitHub CLI:
```bash
# Cryptographically verify provenance of a downloaded release binary
gh attestation verify dist/payment-service-linux-amd64 \
  --owner my-org
```

---

## 5. Practical Examples

### Example 1: Native GitHub Actions Artifact Attestation Workflow
Build a Go binary, generate an attestation, and upload the artifact:
```yaml
name: "Secure Build & Attestation"

on:
  push:
    tags: ["v*"]

permissions:
  contents: write
  id-token: write
  attestations: write

jobs:
  build:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v4

      - uses: actions/setup-go@v5
        with:
          go-version: "1.22"

      - name: Build Binary
        run: |
          go build -o dist/app main.go

      - name: Generate Artifact Attestation
        uses: actions/attest-build-provenance@v1
        with:
          subject-path: "dist/app"

      - name: Create Release
        uses: softprops/action-gh-release@v2
        with:
          files: dist/app
```

### Example 2: Container Attestation for GHCR Docker Images
Attest an OCI container image pushed to GitHub Container Registry:
```yaml
      - name: Build and Push Docker Image
        id: push
        uses: docker/build-push-action@v5
        with:
          context: .
          push: true
          tags: ghcr.io/my-org/auth-service:${{ github.ref_name }}

      - name: Attest Docker Container
        uses: actions/attest-build-provenance@v1
        with:
          subject-name: ghcr.io/my-org/auth-service
          subject-digest: ${{ steps.push.outputs.digest }}
          push-to-registry: true
```

### Example 3: Generating SBOM with Syft in GitHub Actions
Export a detailed CycloneDX SBOM covering both OS-level and application-level dependencies:
```yaml
      - name: Generate CycloneDX SBOM
        uses: anchore/sbom-action@v0
        with:
          path: "."
          format: "cyclonedx-json"
          output-file: "sbom.cyclonedx.json"

      - name: Attest SBOM
        uses: actions/attest-sbom@v1
        with:
          subject-path: "dist/app"
          sbom-path: "sbom.cyclonedx.json"
```

### Example 4: Enforcing Immutable Action SHA Pinning with Step Security
Enforce full hash pinning on all GitHub Actions to protect against supply chain tampering:
```yaml
name: Pin Actions
on: [pull_request]
jobs:
  pin:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v4
      - name: Harden Runner & Verify Pinning
        uses: step-security/harden-runner@v2
        with:
          egress-policy: audit
```

### Example 5: Verification Script for Air-Gapped Deployments
Verify artifact provenance before executing inside an air-gapped production cluster:
```bash
#!/usr/bin/env bash
set -euo pipefail

ARTIFACT="dist/app"
EXPECTED_ORG="my-org"
EXPECTED_REPO="payment-service"

echo "Verifying provenance for $ARTIFACT..."
gh attestation verify "$ARTIFACT" \
  --owner "$EXPECTED_ORG" \
  --repo "$EXPECTED_REPO"

echo "Verification succeeded: Binary is authentic and untampered."
```

---

## 6. Best Practices

- **Pin Third-Party Actions to Immutable Commit SHAs**: Do not rely solely on major version tags (`@v3`). Pin to the full 40-character SHA and use Dependabot to submit automated updates with comments explaining the version bump.
- **Attest All Production Artifacts**: Make `actions/attest-build-provenance` a mandatory final step in release workflows for all binaries and container images.
- **Export and Archive SBOMs in Releases**: Attach a signed SPDX or CycloneDX JSON file to every GitHub Release asset bundle to satisfy enterprise procurement and compliance audits.
- **Enforce Minimal Workflow Permissions**: Restrict workflow tokens; only grant `id-token: write` and `attestations: write` to specific jobs generating provenance.
- **Verify Provenance in CD Deployments**: In Kubernetes or deployment pipelines, verify artifact attestations before rolling out new image tags.

---

## 7. Common Mistakes

| Mistake | Impact | Correct Approach |
|:---|:---|:---|
| Pinning actions to mutable branches (`@main`) | Upstream supply chain compromise immediately executes malicious code in CI | Pin to full commit SHA (`uses: actions/checkout@b4ffde...`) |
| Generating SBOM after deploying to production | Missing runtime context; impossible to audit past zero-day exposures | Generate and archive SBOMs synchronously at build time |
| Omitting `id-token: write` permission during attestation | Workflow fails with OIDC token retrieval error | Declare `id-token: write` and `attestations: write` permissions |
| Verifying only the image tag, not the SHA digest | Image tags can be overwritten in registry by unauthorized actors | Attest and verify using immutable image SHA256 digests |
| Manually crafting custom provenance JSON | Prone to formatting errors and lacks cryptographic non-repudiation | Use standardized `actions/attest-build-provenance` backed by Sigstore |
| Storing signing private keys in repository secrets | Risk of key exfiltration via compromised PR or malicious action | Use keyless OIDC signing via Sigstore Fulcio/Rekor infrastructure |

---

## 8. Performance, Security & Cost Considerations

- **Performance**: Generating an SBOM with Syft adds 5–15 seconds to a build. Generating an artifact attestation via Sigstore adds 2–4 seconds for OIDC exchange and transparency logging.
- **Security**: Keyless signing eliminates long-lived signing keys. Private signing keys are held ephemerally in-memory for seconds and tied directly to the workflow execution identity.
- **Cost**: Artifact attestations and Dependency Graph SBOM exports are free for both public and private repositories. Public repositories write to the public Sigstore Rekor log; private repositories write to GitHub's internal transparency log.

---

## 9. Comparison Table

| Feature | SPDX | CycloneDX | GitHub Attestation (in-toto) |
|:---|:---|:---|:---|
| **Originator** | Linux Foundation (ISO standard) | OWASP Foundation | Linux Foundation / Sigstore |
| **Primary Focus** | Licensing compliance & IP audit | Security vulnerabilities & BOM | Build provenance & integrity proof |
| **Format** | JSON, YAML, tag-value | JSON, XML | JSON (in-toto statement) |
| **Cryptographic Proof**| External signature required | External signature required | Native Sigstore signed envelope |
| **Best Used For** | Legal compliance & licensing | DevSecOps SCA scanners | Verification of build origin |

---

## 10. Real-World Interview Questions

<details>
<summary>1. What is the fundamental difference between an SBOM and an Artifact Attestation?</summary>
An SBOM is an inventory of components (the *ingredients*): packages, libraries, licenses, and versions that make up an application. An Artifact Attestation is a cryptographically signed statement of *provenance* (the *recipe and chef*): it proves which exact GitHub repository, commit SHA, workflow file, and runner environment built the resulting binary, guaranteeing it was not altered or injected after compilation.
</details>

<details>
<summary>2. How does keyless signing with Sigstore work within GitHub Actions?</summary>
The GitHub Actions runner requests an ephemeral OpenID Connect (OIDC) identity token from GitHub's OIDC provider containing claims about the run (repo, commit, workflow). The runner transmits this token to Sigstore's Certificate Authority (Fulcio), which issues a short-lived X.509 certificate (valid for ~10 minutes) binding the runner's ephemeral key pair to the GitHub identity. The runner signs the attestation, logs the record to a transparency log (Rekor), and discards the private key.
</details>

<details>
<summary>3. Why is pinning GitHub Actions to a commit SHA considered more secure than pinning to a release tag?</summary>
Git tags are mutable pointers; a repository maintainer (or an attacker with compromised credentials) can delete and re-point tag `v4` to a malicious commit at any time. A 40-character Git commit SHA is an immutable cryptographic hash of the commit object, tree, and contents. If any code changes, the SHA changes, making silent malicious tampering impossible.
</details>

<details>
<summary>4. What is the SLSA framework and what does SLSA Level 3 mandate?</summary>
SLSA (Supply-chain Levels for Software Artifacts) is a security framework created by Google and OpenSSF. SLSA Level 3 mandates that the build platform is hardened, runs on isolated/ephemeral infrastructure, and produces non-falsifiable provenance that cannot be manipulated by the build script itself.
</details>

<details>
<summary>5. How does `gh attestation verify` protect an engineering team from compromised build servers?</summary>
If an attacker tampers with a build artifact in an S3 bucket or package registry, running `gh attestation verify` will re-hash the local artifact and compare it against the cryptographically signed in-toto statement in the transparency log. If the binary was modified by even a single byte, the hash comparison fails, aborting deployment.
</details>

<details>
<summary>6. How can an enterprise export an organization-wide SBOM across all microservices?</summary>
Administrators can use the GitHub REST API (`/repos/{owner}/{repo}/dependency-graph/sbom`) across all repositories, aggregating the resulting SPDX JSON files into a central data lake (e.g., BigQuery or S3) to perform cross-service vulnerability queries.
</details>

<details>
<summary>7. What permissions are strictly required in GitHub Actions to generate build provenance?</summary>
The job requires `id-token: write` (to request the OIDC token from GitHub's token service) and `attestations: write` (to record and upload the signed attestation).
</details>

<details>
<summary>8. What is the difference between CycloneDX and SPDX?</summary>
SPDX was created by the Linux Foundation with a heavy emphasis on open-source licensing, copyright attribution, and ISO compliance. CycloneDX was developed by OWASP specifically for security operations, supporting vulnerability references (VEX), software services (SaaS-BOM), and hardware components.
</details>

---

## 11. Hands-On Exercises

<details>
<summary>Exercise 1: Export a repository SBOM via GitHub CLI</summary>

**Goal:** Export the dependency graph as an SPDX JSON file.

```bash
gh api /repos/:owner/:repo/dependency-graph/sbom \
  --jq '.sbom' > sbom.json
cat sbom.json | jq '.packages[0]'
```
</details>

<details>
<summary>Exercise 2: Generate an artifact attestation in Actions</summary>

**Goal:** Add `actions/attest-build-provenance` to a build step.

```yaml
- name: Generate Provenance
  uses: actions/attest-build-provenance@v1
  with:
    subject-path: "bin/my-app"
```
</details>

<details>
<summary>Exercise 3: Verify an attestation locally</summary>

**Goal:** Use GitHub CLI to verify a downloaded binary.

```bash
gh attestation verify bin/my-app --owner :owner
```
</details>

<details>
<summary>Exercise 4: Pin an action to an immutable SHA</summary>

**Goal:** Convert `@v4` into an immutable 40-character SHA.

```yaml
# Before: uses: actions/checkout@v4
# After:
uses: actions/checkout@b4ffde65f46336ab88eb53be808477a3936bae11 # v4.1.1
```
</details>

<details>
<summary>Exercise 5: Inspect in-toto provenance statements</summary>

**Goal:** Query GitHub API for attestations linked to a repository.

```bash
gh api /repos/:owner/:repo/attestations
```
</details>

<details>
<summary>Exercise 6: Generate an SBOM using Syft</summary>

**Goal:** Install Syft and generate a CycloneDX JSON file from local directory.

```bash
syft dir:. -o cyclonedx-json=local-sbom.json
```
</details>

---

## 12. Summary / Cheat Sheet

```text
================================================================================
            SOFTWARE SUPPLY CHAIN SECURITY, SBOM & ATTESTATIONS
================================================================================
SBOM Standards    : SPDX (ISO/Licensing) | CycloneDX (OWASP/Vulnerability focused)
SBOM Export API   : gh api /repos/:owner/:repo/dependency-graph/sbom
Attestation Action: actions/attest-build-provenance@v1
Signing Backbone  : Sigstore (Fulcio ephemeral certs + Rekor transparency log)
Action Permissions: id-token: write | attestations: write
Verification CLI  : gh attestation verify <file> --owner <org>
Action Security   : Pin actions to full 40-character commit SHAs, never branches
SLSA Standard     : Levels 1-3 ensuring verifiable build provenance & integrity
================================================================================
```

---

## 13. Further Reading
- [GitHub Docs: About artifact attestations](https://docs.github.com/en/actions/security-guides/using-artifact-attestations-to-establish-provenance-for-builds)
- [SLSA Framework Specification](https://slsa.dev/spec/v1.0/)
- [OpenSSF: Best Practices for Open Source Security](https://bestpractices.coreinfrastructure.org/)
- [Sigstore: Free software signing for everyone](https://www.sigstore.dev/)

---

[<- Previous: Security Policies & Advisories](./05-security-policies-and-advisories.md) | [Module 07 Overview](./README.md) | [Next: Least Privilege Tokens & Permissions ->](./07-least-privilege-tokens-and-permissions.md)
