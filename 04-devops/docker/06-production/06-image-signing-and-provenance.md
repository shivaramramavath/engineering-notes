# Image Signing, Cosign, and SLSA Build Provenance

```
Track: DevOps & Containerization
Module: 06-production
Prerequisites: 06-production/03-docker-in-ci.md, 06-production/05-scanning-and-sbom.md
Difficulty: Advanced
Estimated Duration: 50 minutes
Target Docker Version: 27.x+
Last Validated: February 2025
```

---

## Table of Contents
1. [What & Why](#1-what--why)
2. [Core Concepts](#2-core-concepts)
3. [The Supply Chain Threat Model](#3-the-supply-chain-threat-model)
4. [The Evolution of Container Signing: From Notary v1 to Sigstore Cosign](#4-the-evolution-of-container-signing-from-notary-v1-to-sigstore-cosign)
5. [Sigstore Architecture: Cosign, Fulcio, and Rekor](#5-sigstore-architecture-cosign-fulcio-and-rekor)
6. [Keypair-Based Signing vs Keyless OIDC Signing](#6-keypair-based-signing-vs-keyless-oidc-signing)
7. [Syntax & Implementation Reference](#7-syntax--implementation-reference)
8. [SLSA Build Provenance with Docker BuildKit](#8-slsa-build-provenance-with-docker-buildkit)
9. [Complete Production CI/CD Workflow (GitHub Actions Keyless Signing)](#9-complete-production-cicd-workflow-github-actions-keyless-signing)
10. [Production Best Practices & Antipatterns](#10-production-best-practices--antipatterns)
11. [Common Pitfalls & Troubleshooting](#11-common-pitfalls--troubleshooting)
12. [Performance Tuning & Resource Optimization](#12-performance-tuning--resource-optimization)
13. [Security Hardening & Threat Model](#13-security-hardening--threat-model)
14. [Architectural Comparison Table: Signing Approaches](#14-architectural-comparison-table-signing-approaches)
15. [Real-World Scenario Walkthrough](#15-real-world-scenario-walkthrough)
16. [Self-Assessment & Interview Questions](#16-self-assessment--interview-questions)
17. [Hands-on Practice Exercises](#17-hands-on-practice-exercises)
18. [Summary & Reference Cheat Sheet](#summary--reference-cheat-sheet)

---

## 1. What & Why

In modern containerized deployments, building and scanning images is insufficient if you cannot verify that the image running in production is **cryptographically identical to the exact binary produced by your authorized CI/CD pipeline**.

Without cryptographic signing and provenance attestations:
- **Registry Tampering**: A compromised container registry account allows attackers to overwrite image tags (e.g. `api:v1.2.0`) with malicious backdoored binaries.
- **Man-in-the-Middle (MITM) Attacks**: Attackers intercept image pulls across unauthenticated networks.
- **Supply Chain Impersonation**: Compromised developer laptops can push rogue images directly to production registries, bypassing mandatory code review and automated test suites.

**Sigstore Cosign** and **SLSA (Supply-chain Levels for Software Artifacts) Provenance** provide the cryptographic foundation for container trust, ensuring that orchestrators (Kubernetes, AWS ECS, Docker) strictly execute images that have verifiable, tamper-proof cryptographic signatures.

---

## 2. Core Concepts

### Cryptographic Signatures
A digital signature computed over the image's immutable SHA256 digest using a private key or temporary OIDC certificate. The signature is stored in the container registry as an OCI artifact alongside the image.

### SLSA Provenance
A signed JSON predicate (in-toto specification) generated during the build process that cryptographically records:
- The exact source Git repository commit SHA.
- The build trigger (GitHub Actions, GitLab CI).
- The build parameters, environment, and dependencies.
- The cryptographic hash of the resulting binary artifacts.

---

## 3. The Supply Chain Threat Model

```
+-------------------------------------------------------------------------+
|                  CONTAINER SUPPLY CHAIN ATTACK VECTORS                  |
+-------------------------------------------------------------------------+
| [Source Code] ---> [CI Pipeline] ---> [OCI Registry] ---> [Production]  |
|      |                    |                  |                   |      |
|      | Attack A           | Attack B         | Attack C          |      |
|      v                    v                  v                   |      |
| Untrusted Commits   Compromised Runner  Registry Overwrite       |      |
| (No code review)    (Malware injected)  (Tag swap on ECR)        |      |
|                                                                  v      |
|                                                      [VULNERABLE RUN!]  |
+-------------------------------------------------------------------------+
| WITH COSIGN & SLSA VERIFICATION:                                        |
| Kubernetes / Production Server verifies signature & provenance:         |
|  - Was this built by GitHub Actions from repo 'mycorp/api'?             |
|  - Is signature valid in Rekor public ledger?                           |
| If NO: POD DEPLOYMENT REJECTED BY ADMISSION CONTROLLER!                 |
+-------------------------------------------------------------------------+
```

---

## 4. The Evolution of Container Signing: From Notary v1 to Sigstore Cosign

- **Docker Content Trust (DCT) / Notary v1 (Legacy)**: Based on The Update Framework (TUF). Highly complex, required dedicated Notary server infrastructure, required managing multiple offline root and repository keys, and stored signatures in separate proprietary metadata databases rather than within OCI registries. Largely abandoned by the industry.
- **Sigstore Cosign (Modern Standard)**: Linux Foundation / OpenSSF open-source standard. Stores signatures, SBOMs, and attestations as native standard **OCI artifacts** directly in any standard container registry (Docker Hub, GHCR, AWS ECR, Harbor). Requires zero external metadata servers.

---

## 5. Sigstore Architecture: Cosign, Fulcio, and Rekor

The Sigstore ecosystem consists of three interconnected components:

1. **Cosign**: The developer and CI command-line utility for signing, verifying, and attaching attestations to OCI containers.
2. **Fulcio**: A free, public Root Certificate Authority (CA) that issues short-lived (10-minute) X.509 certificates tied to an OpenID Connect (OIDC) identity (e.g. GitHub Actions, Google, Microsoft).
3. **Rekor**: An append-only, tamper-evident cryptographic transparency log that permanently records signed metadata, preventing signature back-dating or certificate forgery.

---

## 6. Keypair-Based Signing vs Keyless OIDC Signing

### Approach A: Keypair-Based Signing (Traditional)
- Generate a local public/private keypair (`cosign.key` and `cosign.pub`).
- Sign using the private key; verify using the public key.
- **Disadvantage**: Organizations must securely store, rotate, and manage private keys in secret managers. If the private key leaks, trust is destroyed.

### Approach B: Keyless OIDC Signing (Modern Enterprise Best Practice)
- **Zero permanent private keys to manage, rotate, or leak!**
- The CI runner (e.g. GitHub Actions) requests an OIDC token proving its identity (`job: build`, `repo: mycorp/api`).
- Fulcio issues a temporary 10-minute certificate bound to that OIDC identity.
- Cosign signs the container digest and logs the entry into the Rekor transparency ledger.
- Anyone can verify: *"This image was signed by GitHub Actions workflow in repository `mycorp/api`."*

---

## 7. Syntax & Implementation Reference

### Keypair-Based Signing Workflow

```bash
# 1. Generate cryptographic keypair (Secured by password)
cosign generate-key-pair

# 2. Sign container image digest using private key
cosign sign --key cosign.key myregistry.corp/api@sha256:4a8b...

# 3. Verify container signature using public key
cosign verify --key cosign.pub myregistry.corp/api@sha256:4a8b...
```

### Keyless Verification via OIDC Identity

```bash
# Verify image was signed by a specific GitHub Actions repository
cosign verify \
  --certificate-identity-regexp "https://github.com/mycorp/api/.*" \
  --certificate-oidc-issuer "https://token.actions.githubusercontent.com" \
  ghcr.io/mycorp/api@sha256:4a8b...
```

---

## 8. SLSA Build Provenance with Docker BuildKit

Docker Buildx v0.10+ natively integrates SLSA Level 2+ provenance generation. When enabled, BuildKit cryptographically documents the complete build environment and attaches it directly to the multi-platform manifest list.

```bash
docker buildx build \
  --provenance=mode=max \
  --sbom=true \
  --push -t ghcr.io/mycorp/api:v1.0.0 .
```

### Verifying Provenance Attestation
```bash
cosign verify-attestation \
  --type slsaprovenance \
  --certificate-identity "https://github.com/mycorp/api/.github/workflows/deploy.yml@refs/heads/main" \
  --certificate-oidc-issuer "https://token.actions.githubusercontent.com" \
  ghcr.io/mycorp/api:v1.0.0
```

---

## 9. Complete Production CI/CD Workflow (GitHub Actions Keyless Signing)

```yaml
# .github/workflows/sign-and-attest.yml
name: Secure Build, Sign, and Provenance

on:
  push:
    tags: ['v*.*.*']

jobs:
  build-sign:
    runs-on: ubuntu-latest
    permissions:
      contents: read
      packages: write
      id-token: write # MANDATORY for OIDC Keyless Signing!

    steps:
      - name: Checkout Code
        uses: actions/checkout@v4

      - name: Install Cosign
        uses: sigstore/cosign-installer@v3.8.0

      - name: Set up Docker Buildx
        uses: docker/setup-buildx-action@v3

      - name: Log in to GitHub Container Registry
        uses: docker/login-action@v3
        with:
          registry: ghcr.io
          username: ${{ github.actor }}
          password: ${{ secrets.GITHUB_TOKEN }}

      - name: Build and Push OCI Image with Provenance
        id: build-push
        uses: docker/build-push-action@v6
        with:
          context: .
          push: true
          tags: ghcr.io/${{ github.repository }}:${{ github.ref_name }}
          provenance: mode=max
          sbom: true

      - name: Sign Image Cryptographically with Cosign (Keyless)
        env:
          TAG: ${{ github.ref_name }}
          DIGEST: ${{ steps.build-push.outputs.digest }}
        run: |
          cosign sign --yes "ghcr.io/${{ github.repository }}@${DIGEST}"

      - name: Verify Signed Image
        run: |
          cosign verify \
            --certificate-identity "https://github.com/${{ github.repository }}/.github/workflows/sign-and-attest.yml@${{ github.ref }}" \
            --certificate-oidc-issuer "https://token.actions.githubusercontent.com" \
            "ghcr.io/${{ github.repository }}@${{ steps.build-push.outputs.digest }}"
```

---

## 10. Production Best Practices & Antipatterns

### Best Practices
- **Adopt Keyless Signing in CI/CD**: Eliminate static private keys (`cosign.key`). Use GitHub Actions or GitLab OIDC tokens with Fulcio and Rekor.
- **Sign by Digest, Never by Tag**: Always execute `cosign sign image@sha256:...`. Tags are mutable; signing a tag creates race condition vulnerabilities.
- **Enforce Admission Control in Production**: Deploy **Kyverno** or **Open Policy Agent (OPA) Gatekeeper** in Kubernetes clusters to reject any pod deployment that lacks a valid Cosign signature.
- **Attach Both SBOM and Provenance**: Always generate both `--provenance=mode=max` and `--sbom=true` during BuildKit builds.

### Anti-Patterns
- **Storing Private Keys in Plaintext Environment Variables**: Hardcoding `COSIGN_PRIVATE_KEY` in shell scripts defeats security.
- **Signing Before Automated Tests Pass**: Only sign images *after* integration tests and vulnerability scans have passed with zero Critical CVEs.
- **Using Docker Content Trust (DCT)**: DCT / Notary v1 is obsolete. Modern systems should standardize on Cosign.

---

## 11. Common Pitfalls & Troubleshooting

| Symptom | Root Cause | Diagnosis Command | Permanent Fix |
|:---|:---|:---|:---|
| Cosign keyless signing fails with `no OIDC token found` | GitHub Actions workflow missing `id-token: write` permission | Inspect workflow YAML `permissions:` block | Add `permissions: { id-token: write }` to job. |
| `cosign verify` fails with `no matching signatures` | Image was updated after signing, changing its SHA256 digest | Compare `docker inspect` digest with sign log | Re-sign the new image digest; always verify by `@sha256:` digest. |
| Cosign fails to push signature to registry with `403 Forbidden` | Registry token lacks write access to OCI artifact tags | Check registry permissions | Ensure token has full read/write permissions for package repository. |
| Verification fails with `certificate identity mismatch` | `--certificate-identity` regex does not match the exact workflow path or Git ref | Check Fulcio cert details in Rekor | Inspect certificate: `cosign verify ... \| jq .` and align regex. |
| Buildx provenance attestation creates unexpected extra tags | BuildKit stores attestations as OCI index manifests (e.g. `image:tag.att`) | `docker buildx imagetools inspect <tag>` | Expected behavior; modern OCI registries natively store attestations as linked artifacts. |
| Private registry blocks OCI media types | Legacy enterprise registry (older Artifactory/Nexus) does not support OCI artifact specifications | Check registry HTTP response | Upgrade container registry to version supporting OCI Image Spec v1.1. |

---

## 12. Performance Tuning & Resource Optimization

1. **Registry Co-location**:
   Cosign stores signatures and attestations directly within the target image repository as sibling OCI artifacts. Because they are tiny JSON/signature blobs (< 50KB), verification checks incur negligible network transfer (< 10ms latency).
2. **Admission Controller Caching**:
   In Kubernetes clusters, configure policy engines (Kyverno) to cache signature verification results in memory, eliminating redundant network calls to Rekor on container restarts.

---

## 13. Security Hardening & Threat Model

```
+--------------------------------------------------------------+
| ATTACK: Deployment of Backdoored Image by Rogue Admin        |
+--------------------------------------------------------------+
| An attacker steals registry credentials.                     |
| Attacker pushes malicious container: `mycorp/api:v1.2.0`.    |
| Attacker commands production Kubernetes cluster to deploy.   |
+--------------------------------------------------------------+
| WITHOUT SIGNING:                                             |
| - Kubernetes pulls image.                                    |
| - Malware executes in production cluster!                    |
+--------------------------------------------------------------+
| WITH COSIGN & KYVERNO ADMISSION POLICY:                      |
| - Kyverno intercepts deployment request.                     |
| - Queries Rekor: "Was this signed by GitHub Actions OIDC?"   |
| - Signature is MISSING!                                      |
| - ADMISSION DENIED: Pod creation blocked!                    |
| - Security Operations receives immediate alert.              |
+--------------------------------------------------------------+
```

---

## 14. Architectural Comparison Table: Signing Approaches

| Metric / Trait | Notary v1 (DCT) | Cosign (Keypair) | Cosign (Keyless OIDC) |
|:---|:---|:---|:---|
| **Underlying Standard** | TUF (The Update Framework) | OCI Artifact Spec | OCI Artifact + Sigstore |
| **Key Management** | Heavy offline/root keys | Static local key files | **Zero Static Keys (Ephemeral)** |
| **Public Transparency Log**| No | No | **Yes (Rekor)** |
| **CI/CD Integration** | Complex / Fragile | Moderate | **Native & Seamless** |
| **Industry Adoption** | Deprecated / Declining | High | **De Facto Modern Standard** |

---

## 15. Real-World Scenario Walkthrough

### Scenario: Implementing Strict Supply Chain Enforcement for a Healthcare FinTech

#### The Challenge
Under strict federal regulatory audit (HIPAA / FedRAMP), a healthcare payment platform was required to prove that **every container running in production originated from reviewed source code in their private GitHub repository and had never been altered in transit**.

#### The Implementation Plan
1. **Keyless Signing in CI**:
   Configured GitHub Actions with `id-token: write` and `cosign-installer` to sign all release digests automatically upon merging to `main`.
2. **SLSA Provenance Attestation**:
   Added `--provenance=mode=max` to all Docker Buildx build pipelines.
3. **Kyverno Admission Controller in Production**:
   Deployed a Kyverno policy in Kubernetes:
   ```yaml
   apiVersion: kyverno.io/v1
   kind: ClusterPolicy
   metadata:
     name: check-image-signature
   spec:
     validationFailureAction: Enforce
     rules:
       - name: verify-signature
         match:
           resources:
             kinds: [Pod]
         verifyImages:
           - imageReferences: ["ghcr.io/healthcare-corp/*"]
             attestors:
               - entries:
                   - keyless:
                       issuer: "https://token.actions.githubusercontent.com"
                       subject: "https://github.com/healthcare-corp/*"
   ```
4. **Outcome**:
   Zero unauthorized images can physically be scheduled in the production cluster. The company passed its federal compliance audit with zero findings.

---

## 16. Self-Assessment & Interview Questions

<details>
<summary><strong>1. What is Keyless Signing in Sigstore Cosign, and how does it eliminate private key management?</strong></summary>

**Answer:**
Keyless signing replaces static, long-lived private keys with ephemeral, short-lived cryptographic credentials. In CI/CD (e.g. GitHub Actions), Cosign authenticates using an OpenID Connect (OIDC) identity token. Sigstore's Certificate Authority (Fulcio) verifies the token and issues a short-lived (10-minute) X.509 certificate bound to that specific repository and workflow. Cosign signs the image digest and records the certificate in the public Rekor transparency log. Because the certificate validity is recorded immutably in Rekor, verifiers can validate the signature indefinitely without anyone needing to store or rotate a permanent private key.
</details>

<details>
<summary><strong>2. Why should you always sign container images by their SHA256 digest rather than an image tag?</strong></summary>

**Answer:**
Image tags (e.g. `:latest` or `:v1.2.0`) are completely mutable; registry administrators or automated tools can re-push new layers to an existing tag at any time. If you sign a tag, an attacker could replace the underlying layers while retaining the same tag name, causing verifiers to mistakenly trust the new malicious layers. Signing the immutable cryptographic SHA256 digest (`image@sha256:...`) binds the signature permanently and tamper-proof to that exact byte-for-byte binary artifact.
</details>

<details>
<summary><strong>3. What is the role of Rekor in the Sigstore ecosystem?</strong></summary>

**Answer:**
Rekor is an append-only, tamper-evident transparency log based on a cryptographic Merkle tree (similar to Certificate Transparency in web PKI). When an image is signed using keyless signing, the signature, public key, and timestamp are committed to Rekor. Verifiers query Rekor to confirm that a signature was recorded while the ephemeral certificate was valid, preventing malicious actors from back-dating compromised certificates.
</details>

<details>
<summary><strong>4. What is SLSA Build Provenance, and what information does it record?</strong></summary>

**Answer:**
SLSA (Supply-chain Levels for Software Artifacts) provenance is a cryptographically verifiable attestation describing the origin of a software artifact. It documents the exact Git repository URL, commit hash, build platform (e.g., GitHub Actions), invocation parameters, build time, and cryptographic digests of all input dependencies and output artifacts, proving the binary was built cleanly by an authorized builder.
</details>

<details>
<summary><strong>5. Why has the industry transitioned away from Docker Content Trust (Notary v1)?</strong></summary>

**Answer:**
Docker Content Trust / Notary v1 suffered from severe usability and operational limitations: it required managing complex hierarchies of offline root keys, required organizations to deploy and maintain dedicated Notary server clusters, stored signature metadata in separate databases outside standard registries, and did not support modern OIDC keyless signing or OCI artifact specifications.
</details>

<details>
<summary><strong>6. How does a production Kubernetes cluster enforce image signing at runtime?</strong></summary>

**Answer:**
Using **Admission Controllers** (such as Kyverno or OPA Gatekeeper). When a deployment request is submitted to the Kubernetes API, the admission controller intercepts the Pod creation request, executes `cosign verify` against the target image digest in the registry, and blocks the pod from launching (`Admission Denied`) if a valid cryptographic signature or attestation is missing.
</details>

<details>
<summary><strong>7. How do you generate an SLSA provenance attestation using Docker Buildx?</strong></summary>

**Answer:**
Add the `--provenance=mode=max` flag to the build command:
`docker buildx build --provenance=mode=max --push -t myregistry/app:v1 .`
BuildKit compiles the in-toto provenance predicate and attaches it directly to the image index in the registry.
</details>

<details>
<summary><strong>8. What GitHub Actions permission is required for keyless Cosign signing?</strong></summary>

**Answer:**
`permissions: { id-token: write }`. This permission allows the GitHub Actions runner to request a JSON Web Token (JWT) from GitHub's OIDC provider to prove its identity to Sigstore Fulcio.
</details>

---

## 17. Hands-on Practice Exercises

<details>
<summary><strong>Exercise 1: Generating a Local Cosign Keypair</strong></summary>

**Goal:** Generate a public/private cryptographic keypair and inspect the output.

**Step-by-step Solution:**
1. Install Cosign (or run via container).
2. Generate keypair:
   ```bash
   cosign generate-key-pair
   ```
3. Enter a password when prompted.
4. Verify files created: `cosign.key` (private, encrypted) and `cosign.pub` (public).
</details>

<details>
<summary><strong>Exercise 2: Signing and Verifying an Image Locally with Keypair</strong></summary>

**Goal:** Push a test image to a local or public registry, sign it, and verify the signature.

**Step-by-step Solution:**
1. Sign image digest:
   ```bash
   cosign sign --key cosign.key <registry>/<repo>@sha256:<digest>
   ```
2. Verify signature:
   ```bash
   cosign verify --key cosign.pub <registry>/<repo>@sha256:<digest>
   ```
3. Observe JSON output containing verification claims.
</details>

<details>
<summary><strong>Exercise 3: Inspecting BuildKit Provenance Attestations</strong></summary>

**Goal:** Inspect the SLSA provenance attestation attached to an image index.

**Step-by-step Solution:**
1. Inspect image manifest list:
   ```bash
   docker buildx imagetools inspect <registry>/<repo>:<tag>
   ```
2. Notice the `application/vnd.in-toto+json` attestation manifest linked to the image digest.
</details>

<details>
<summary><strong>Exercise 4: Simulating Signature Verification Failure on Altered Image</strong></summary>

**Goal:** Verify that modifying an image causes signature verification to fail.

**Step-by-step Solution:**
1. Re-tag an unsigned image with the same name.
2. Attempt verification against the new digest:
   ```bash
   cosign verify --key cosign.pub <registry>/<repo>@sha256:<new_digest>
   ```
3. Verify failure: `no matching signatures found`.
</details>

<details>
<summary><strong>Exercise 5: Formatting Cosign Verification Output with JQ</strong></summary>

**Goal:** Extract the signing subject and timestamp using `jq`.

**Step-by-step Solution:**
1. Run:
   ```bash
   cosign verify --key cosign.pub <target> | jq '.[0].critical.identity'
   ```
</details>

<details>
<summary><strong>Exercise 6: Setting Up Kyverno Local Cluster Policy</strong></summary>

**Goal:** Inspect a sample Kyverno image verification policy manifest.

**Step-by-step Solution:**
1. Review the ClusterPolicy template in Section 15 to understand admission criteria.
</details>

---

## 18. Summary & Reference Cheat Sheet

### Essential Cosign Commands
```bash
# Generate Keypair:
cosign generate-key-pair

# Sign by Digest:
cosign sign --key cosign.key <image>@sha256:<digest>

# Verify with Key:
cosign verify --key cosign.pub <image>@sha256:<digest>

# Keyless Verification (GitHub Actions OIDC):
cosign verify \
  --certificate-identity-regexp "https://github.com/mycorp/.*" \
  --certificate-oidc-issuer "https://token.actions.githubusercontent.com" \
  <image>@sha256:<digest>
```

### Top 4 Rules of Image Signing
1. **Always sign by `@sha256:` digest**, never by mutable tags.
2. **Adopt Keyless Signing** in CI/CD to eliminate static private keys.
3. **Attach SLSA Provenance** via `--provenance=mode=max`.
4. **Enforce admission control** (Kyverno) to reject unsigned images in production.

---

[← Previous: 05-scanning-and-sbom.md](05-scanning-and-sbom.md) | [Next: 07-registries.md →](07-registries.md)
