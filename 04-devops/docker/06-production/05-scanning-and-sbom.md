# Container Vulnerability Scanning, CVE Triaging, and Software Bill of Materials (SBOM)

```
Track: DevOps & Containerization
Module: 06-production
Prerequisites: 06-production/02-image-optimisation.md, 06-production/04-image-security.md
Difficulty: Advanced
Estimated Duration: 50 minutes
Target Docker Version: 27.x+
Last Validated: February 2025
```

---

## Table of Contents
1. [What & Why](#1-what--why)
2. [Core Concepts](#2-core-concepts)
3. [The Vulnerability Lifecycle & CVSS Scoring](#3-the-vulnerability-lifecycle--cvss-scoring)
4. [Software Bill of Materials (SBOM): SPDX vs CycloneDX](#4-software-bill-of-materials-sbom-spdx-vs-cyclonedx)
5. [Industry Standard Scanning Engines (Trivy, Grype, Docker Scout)](#5-industry-standard-scanning-engines-trivy-grype-docker-scout)
6. [Syntax & Implementation Reference](#6-syntax--implementation-reference)
7. [Practical Implementation & CI Gate Workflows](#7-practical-implementation--ci-gate-workflows)
8. [Triage Methodology & Suppressing False Positives](#8-triage-methodology--suppressing-false-positives)
9. [Production Best Practices & Antipatterns](#9-production-best-practices--antipatterns)
10. [Common Pitfalls & Troubleshooting](#10-common-pitfalls--troubleshooting)
11. [Performance Tuning & Resource Optimization](#11-performance-tuning--resource-optimization)
12. [Security Hardening & Threat Model](#12-security-hardening--threat-model)
13. [Architectural Comparison Table: Scanner Tooling](#13-architectural-comparison-table-scanner-tooling)
14. [Real-World Scenario Walkthrough](#14-real-world-scenario-walkthrough)
15. [Self-Assessment & Interview Questions](#15-self-assessment--interview-questions)
16. [Hands-on Practice Exercises](#16-hands-on-practice-exercises)
17. [Summary & Reference Cheat Sheet](#summary--reference-cheat-sheet)

---

## 1. What & Why

Shipping unvetted container images to production is an invitation to supply chain compromise. A modern production container packages two distinct software layers:
1. **The Operating System Layer**: Linux distribution user-space libraries (`glibc`, `openssl`, `zlib`, `busybox`, `systemd`).
2. **The Application Layer**: Language runtime dependencies installed via package managers (`npm`, `pip`, `maven`, `cargo`, `nuget`).

A security vulnerability in either layer compromises the entire container. To achieve compliance (FedRAMP, SOC2, PCI-DSS) and secure the software supply chain:
- **SBOM (Software Bill of Materials)**: Every container must carry an exhaustive cryptographic inventory of every component, library, and license bundled within it.
- **Automated CVE Scanning**: Container images must be continuously scanned in CI/CD pipelines before deployment and inside production registries post-deployment.

---

## 2. Core Concepts

### CVE (Common Vulnerabilities and Exposures)
A publicly registered identification number (e.g. `CVE-2024-6387`) cataloging a specific security flaw, tracked by the National Vulnerability Database (NVD).

### CVSS (Common Vulnerability Scoring System)
A standardized numerical metric (0.0 to 10.0) reflecting severity:
- **Low**: 0.1 – 3.9
- **Medium**: 4.0 – 6.9
- **High**: 7.0 – 8.9
- **Critical**: 9.0 – 10.0 (Immediate exploitation possible, network vector, no auth required).

### What is an SBOM?
A Software Bill of Materials is a machine-readable document describing the exact composition of software artifacts: package names, exact versions, file hashes, licensing, and upstream authors.

---

## 3. The Vulnerability Lifecycle & CVSS Scoring

```
+-------------------------------------------------------------------------+
|                  VULNERABILITY SCANNING ANATOMY                         |
+-------------------------------------------------------------------------+
| Target: mycorp/web-api:latest                                           |
|                                                                         |
| OS Layer (Alpine 3.21)          Application Layer (package-lock.json)  |
| ├── libcrypto3: 3.3.2-r1         ├── express: 4.21.0                    |
| ├── musl: 1.2.5-r0               ├── lodash: 4.17.21 (CVE-2021-23337)   |
| └── zlib: 1.3.1-r1               └── axios: 1.7.4                       |
|                                                                         |
|                v (Scanner cross-references NVD/GitHub Advisory)         |
|                                                                         |
| Finding: lodash v4.17.21 -> CVE-2021-23337                             |
| Severity: HIGH (CVSS 7.2) | Vector: Command Injection via template()    |
| Fix Available: Upgrade to lodash >= 4.17.22                             |
+-------------------------------------------------------------------------+
```

---

## 4. Software Bill of Materials (SBOM): SPDX vs CycloneDX

The industry has converged on two open standard SBOM formats:

| Feature / Trait | SPDX (Software Package Data Exchange) | CycloneDX (OWASP Standard) |
|:---|:---|:---|
| **Origin** | Linux Foundation / ISO/IEC 5962:2021 | OWASP Foundation |
| **Primary Strength** | Open-source licensing compliance & IP audit | Application security & vulnerability analysis |
| **Format Support** | JSON, YAML, Tag:Value | JSON, XML |
| **Buildx Native** | Supported (`docker buildx --sbom=true`) | Supported via Syft |

---

## 5. Industry Standard Scanning Engines (Trivy, Grype, Docker Scout)

1. **Aqua Security Trivy**: The undisputed open-source industry standard. Blazing fast, scans OS packages, language dependencies, misconfigurations, and secrets.
2. **Anchore Grype**: High-speed, focused vulnerability scanner designed to work natively with **Syft** (Anchore's SBOM generator).
3. **Docker Scout**: Docker's native commercial analysis tool integrated directly into Docker Desktop and Docker Hub.

---

## 6. Syntax & Implementation Reference

### Generating SBOMs with Syft and BuildKit

```bash
# 1. Generate CycloneDX JSON SBOM using Syft
syft my-image:latest -o cyclonedx-json=sbom.json

# 2. Generate SPDX JSON SBOM using Syft
syft my-image:latest -o spdx-json=sbom.spdx.json

# 3. Generate native OCI SBOM during image build with Buildx
docker buildx build --sbom=true --output type=image,push=true -t my-image:v1 .
```

### Scanning Images with Trivy

```bash
# Basic scan printing summary table
trivy image my-image:latest

# Fail CI pipeline on Critical or High vulnerabilities only
trivy image --severity CRITICAL,HIGH --exit-code 1 my-image:latest

# Scan only for vulnerabilities that have an available fix
trivy image --ignore-unfixed --severity CRITICAL my-image:latest

# Output SARIF format for GitHub Security integration
trivy image --format sarif --output trivy-results.sarif my-image:latest
```

---

## 7. Practical Implementation & CI Gate Workflows

### Complete GitHub Actions Automated Vulnerability Gate

```yaml
name: Container Vulnerability Gate & SBOM

on:
  push:
    branches: [main]
  pull_request:

jobs:
  scan:
    runs-on: ubuntu-latest
    steps:
      - name: Checkout Code
        uses: actions/checkout@v4

      - name: Build Local Image
        run: docker build -t scan-target:${{ github.sha }} .

      - name: Run Trivy Vulnerability Scanner
        uses: aquasecurity/trivy-action@0.29.0
        with:
          image-ref: scan-target:${{ github.sha }}
          format: 'table'
          exit-code: '1' # Fails CI if vulnerabilities exceed threshold!
          ignore-unfixed: true
          severity: 'CRITICAL,HIGH'

      - name: Generate CycloneDX SBOM
        uses: anchore/sbom-action@v0
        with:
          image: scan-target:${{ github.sha }}
          format: 'cyclonedx-json'
          output-file: 'sbom.cyclonedx.json'

      - name: Upload SBOM Artifact
        uses: actions/upload-artifact@v4
        with:
          name: sbom-artifact
          path: sbom.cyclonedx.json
```

---

## 8. Triage Methodology & Suppressing False Positives

Not every reported CVE represents an exploitable risk in your specific container:
- An image might contain a vulnerable library in an unused CLI binary that is never invoked.
- A vulnerability might require an attack vector that is impossible due to network isolation.

### The `.trivyignore` Suppression File
Suppress vetted false positives or accepted risks with strict documentation and expiration dates:

```text
# .trivyignore
# CVE-2023-38545: SOCKS5 heap buffer overflow in libcurl.
# Triaged by SecOps: Our container does not use SOCKS5 proxies.
# Upstream patch expected in Alpine 3.21.2.
# Expire rule on March 30, 2025:
CVE-2023-38545 exp:2025-03-30
```

---

## 9. Production Best Practices & Antipatterns

### Best Practices
- **Gate Merges on `--ignore-unfixed`**: Failing builds for CVEs that have no available security patch stalls development without an actionable fix. Gate builds on vulnerabilities with actionable updates: `--ignore-unfixed`.
- **Generate SBOMs at Build Time**: Attach the SBOM directly to the OCI image index in your registry using `docker buildx build --sbom=true`.
- **Continuously Scan Inactive Images in Registries**: Security vulnerabilities are discovered daily in *existing* software. Scanning must occur in registries on resting images, not just during CI builds.
- **Distinguish OS vs Application Dependencies**: Check whether a CVE originates in Debian/Alpine base packages or in your application's `package-lock.json` / `poetry.lock`.

### Anti-Patterns
- **Silencing All High Warnings wholesale**: Disabling alerts because "it's too noisy" invites breach. Use explicit suppression files (`.trivyignore`) with mandatory expiry dates.
- **Ignoring Transitive Application Dependencies**: Scanning only the OS packages while skipping `node_modules` or Python wheels misses 80% of real application vulnerabilities.
- **Running Outdated Scanner Databases**: Vulnerability scanners are only as good as their local CVE databases. Always ensure scanners update definitions before execution.

---

## 10. Common Pitfalls & Troubleshooting

| Symptom | Root Cause | Diagnosis Command | Permanent Fix |
|:---|:---|:---|:---|
| Trivy reports 150 CVEs in freshly built image | Base image is `ubuntu:latest` or `debian:full` containing dozens of legacy packages | `trivy image <target>` | Switch to `debian:bookworm-slim`, `alpine:3.21`, or Google Distroless. |
| CI build fails on a CVE that has no available patch | Scanner evaluated all CVEs including unfixed kernel bugs | Inspect scanner table "Fixed Version" column | Add `--ignore-unfixed` flag to scanner CLI. |
| Trivy scan takes 5 minutes in CI | Scanner downloading multi-hundred MB vulnerability database from scratch on every run | Inspect step timings | Cache Trivy DB directory (`~/.cache/trivy`) in CI workflow. |
| Scanner cannot find application dependencies | Dependency manifests (e.g. `package-lock.json`) were stripped or replaced during build | Check build stages | Retain lockfiles in the builder stage to allow static analysis. |
| Syft fails with `unable to determine package type` | Custom in-house binary installed without package manager or version metadata | Inspect syft output | Document custom packages using a custom cataloger or manifest file. |
| `.trivyignore` rules ignored by CI runner | File path not passed to Trivy or file syntax invalid | `trivy image --ignorefile .trivyignore ...` | Ensure `--ignorefile` points to correct relative path in workspace. |

---

## 11. Performance Tuning & Resource Optimization

1. **Caching Vulnerability Databases in CI**:
   Downloading Trivy's vulnerability database takes 30-60 seconds. Cache `~/.cache/trivy` in GitHub Actions using `actions/cache` to speed up pipeline execution by 80%.
2. **Pre-Computed SBOM Scanning**:
   Instead of scanning the full container image (which requires unpacking tar layers), generate the SBOM once (`syft <image> > sbom.json`) and run Grype against the pre-computed SBOM: `grype sbom:sbom.json`. This completes in **under 2 seconds**!

---

## 12. Security Hardening & Threat Model

```
+--------------------------------------------------------------+
| ATTACK: Supply Chain Weaponization (Log4Shell / XZ Utils)     |
+--------------------------------------------------------------+
| A critical Remote Code Execution vulnerability is disclosed  |
| in an open-source dependency (e.g. log4j-core).              |
|                                                              |
| WITHOUT SBOM & AUTOMATED SCANNING:                           |
| - Team spends 2 weeks manually checking 150 repos.           |
| - Production workloads remain vulnerable to internet scans.  |
| - Active ransomware exploitation occurs!                     |
+--------------------------------------------------------------+
| WITH AUTOMATED SBOM & REGISTRY SCANNING:                     |
| - SecOps queries centralized registry SBOM catalog:          |
|   `SELECT image FROM registry WHERE package = 'log4j-core'`  |
| - 4 vulnerable microservices identified in 45 SECONDS.       |
| - Automated patch PRs opened and deployed immediately.       |
+--------------------------------------------------------------+
```

---

## 13. Architectural Comparison Table: Scanner Tooling

| Feature / Trait | Aqua Trivy | Anchore Grype / Syft | Docker Scout | Snyk Container |
|:---|:---|:---|:---|:---|
| **License** | Open Source (Apache 2.0) | Open Source (Apache 2.0) | Proprietary / Freemium| Commercial / Freemium |
| **OS Package Scanning**| **Yes** | **Yes** | **Yes** | **Yes** |
| **Language Dependency Scanning**| **Yes (Fast)** | **Yes (Fast)** | **Yes** | **Yes (Extensive)** |
| **Secret Detection** | **Yes** | No | No | No |
| **Misconfiguration (IaC)**| **Yes (Dockerfile/Compose)**| No | Limited | **Yes** |
| **Execution Speed** | Ultra-Fast (~3-5s) | **Fastest on SBOM (<2s)**| Cloud Managed | Moderate |

---

## 14. Real-World Scenario Walkthrough

### Scenario: Rapid Remediation of OpenSSL RCE in Enterprise Fleet

#### The Crisis
A Critical zero-day vulnerability (CVSS 9.8) in `libssl` was announced, allowing unauthenticated buffer overflow RCE. Enterprise security mandated that all production containers across 80 microservices be audited and patched within 24 hours.

#### The Action Plan
1. **Audited Fleet via Pre-Computed SBOMs**:
   Queried the company's Harbor registry using Grype against stored CycloneDX SBOMs:
   `grype sbom:fleet-catalog.json | grep CVE-2024-xxxx`
   Identified that 14 microservices based on `debian:11-slim` contained the vulnerable library.
2. **Updated Base Images**:
   Updated Dockerfiles to `debian:bookworm-slim@sha256:d6b9...` containing the patched OpenSSL release.
3. **Enforced CI Gate**:
   Added blocking Trivy check in CI pipelines:
   `trivy image --severity CRITICAL --exit-code 1 <image>`
4. **Resolution**: All 14 services patched and deployed to production in **3 hours 15 minutes**, beating the 24-hour compliance SLA.

---

## 15. Self-Assessment & Interview Questions

<details>
<summary><strong>1. What is the fundamental difference between an Operating System vulnerability and an Application Dependency vulnerability in a container image?</strong></summary>

**Answer:**
An OS vulnerability resides in the base Linux user-space packages installed by distribution package managers (`apt`, `apk`, `rpm`), such as `glibc`, `openssl`, or `busybox`. An application dependency vulnerability resides in the language-level libraries installed into the application layer via tools like `npm`, `pip`, `maven`, or `cargo` (e.g. `lodash`, `requests`, `spring-core`). Both must be scanned and audited.
</details>

<details>
<summary><strong>2. What is a Software Bill of Materials (SBOM), and why is it legally mandated in modern cybersecurity frameworks?</strong></summary>

**Answer:**
An SBOM is a comprehensive, machine-readable inventory of all software components, libraries, modules, and licenses included in a software package. Cybersecurity frameworks (such as US Executive Order 14028, EU Cyber Resilience Act) mandate SBOMs to ensure supply chain transparency, enabling organizations to instantly determine whether newly disclosed zero-day vulnerabilities (like Log4Shell) affect their running production software.
</details>

<details>
<summary><strong>3. Why should CI pipelines configure `--ignore-unfixed` when scanning with Trivy?</strong></summary>

**Answer:**
Many disclosed vulnerabilities have no upstream security patch available from Linux distribution maintainers (either because it is low risk, a vendor-disputed issue, or awaiting upstream development). If `--ignore-unfixed` is omitted, CI builds will fail with exit code 1 for vulnerabilities that developers cannot physically fix, creating pipeline gridlock.
</details>

<details>
<summary><strong>4. How do you suppress a known false positive or accepted CVE in Trivy?</strong></summary>

**Answer:**
Add the CVE identifier to a `.trivyignore` file located in the root of the project, accompanied by a comment documenting the business justification and an expiration date (e.g. `CVE-2023-1234 exp:2025-06-01`). Trivy ignores entries in this file until the expiration date passes.
</details>

<details>
<summary><strong>5. What is the advantage of using Syft to generate an SBOM first, followed by Grype to scan the SBOM, rather than scanning the raw image directly?</strong></summary>

**Answer:**
Scanning a raw image requires downloading and extracting multiple multi-megabyte tar layers, consuming significant disk I/O and network bandwidth. Syft extracts the lightweight JSON SBOM once. Grype can scan the resulting JSON SBOM in milliseconds against vulnerability databases without touching container storage drivers, making the scan process significantly faster in CI/CD.
</details>

<details>
<summary><strong>6. How does Base Image selection impact an image's overall CVE count?</strong></summary>

**Answer:**
Full general-purpose distributions (like `ubuntu:latest`) bundle hundreds of non-essential utilities (compilers, manual pages, systemd, text editors), each carrying potential vulnerabilities. Minimalist images like `debian-slim`, `alpine`, and Google `distroless` strip out all non-essential binaries, reducing the baseline package count from 500+ packages to fewer than 50, slashing CVE scan noise by 90% or more.
</details>

<details>
<summary><strong>7. What is the difference between CVSS v3 and CVSS v4?</strong></summary>

**Answer:**
CVSS v4 (released late 2023) refines severity metrics by separating base technical severity from environmental and threat context. It adds new assessments for attack requirements, user interaction, and impact on physical safety, addressing limitations in CVSS v3.1 where network-accessible flaws were frequently over-scored as Critical even if exploitation required extreme prerequisites.
</details>

<details>
<summary><strong>8. How can an administrator verify whether an OCI image in a registry includes a signed SBOM?</strong></summary>

**Answer:**
Using Cosign or the Docker CLI:
`docker buildx imagetools inspect <image> --format '{{json .}}'`
Or using Cosign:
`cosign verify-attestation --type cyclonedx <image>`
</details>

---

## 16. Hands-on Practice Exercises

<details>
<summary><strong>Exercise 1: Scanning an Image with Trivy via Docker</strong></summary>

**Goal:** Run Trivy on an image and review findings.

**Step-by-step Solution:**
1. Execute Trivy scan:
   ```bash
   docker run --rm -v /var/run/docker.sock:/var/run/docker.sock \
     aquasec/trivy:latest image python:3.9
   ```
2. Review findings table showing CVE, Severity, and Fixed Version.
</details>

<details>
<summary><strong>Exercise 2: Generating a CycloneDX SBOM with Syft</strong></summary>

**Goal:** Extract a machine-readable JSON SBOM of an image.

**Step-by-step Solution:**
1. Generate SBOM:
   ```bash
   docker run --rm -v /var/run/docker.sock:/var/run/docker.sock \
     anchore/syft:latest node:22-alpine -o cyclonedx-json > node-sbom.json
   ```
2. Inspect package components:
   ```bash
   jq '.components[0:5]' node-sbom.json
   ```
</details>

<details>
<summary><strong>Exercise 3: Scanning an SBOM with Grype</strong></summary>

**Goal:** Scan the generated JSON SBOM in milliseconds without the container runtime.

**Step-by-step Solution:**
1. Run Grype:
   ```bash
   docker run --rm -v $(pwd):/data anchore/grype:latest sbom:/data/node-sbom.json
   ```
2. Observe instant results.
</details>

<details>
<summary><strong>Exercise 4: Suppressing a CVE via `.trivyignore`</strong></summary>

**Goal:** Suppress a specific CVE and verify Trivy ignores it.

**Step-by-step Solution:**
1. Identify a CVE from Exercise 1.
2. Add to `.trivyignore`: `CVE-XXXX-YYYY`
3. Run Trivy with `--ignorefile .trivyignore`.
4. Verify the CVE no longer appears in output.
</details>

<details>
<summary><strong>Exercise 5: Enforcing CI Pipeline Failure on Critical CVEs</strong></summary>

**Goal:** Configure Trivy to exit with code 1 when Critical flaws exist.

**Step-by-step Solution:**
1. Run:
   ```bash
   docker run --rm -v /var/run/docker.sock:/var/run/docker.sock \
     aquasec/trivy:latest image --severity CRITICAL --exit-code 1 node:14
   echo "Exit status: $?"
   # Exits with 1!
   ```
</details>

<details>
<summary><strong>Exercise 6: Attaching BuildKit Native SBOM to Output</strong></summary>

**Goal:** Build an image with native Buildx SBOM attestation.

**Step-by-step Solution:**
1. Run:
   ```bash
   docker buildx build --sbom=true -t local-sbom-demo .
   ```
</details>

---

## 17. Summary & Reference Cheat Sheet

### Top Scanning Commands
```bash
# Trivy CI Gate:
trivy image --severity CRITICAL,HIGH --ignore-unfixed --exit-code 1 <image>

# Syft SBOM Generation:
syft <image> -o cyclonedx-json=sbom.json

# Fast Grype SBOM Scan:
grype sbom:sbom.json
```

### Top 4 Rules
1. **Always use `--ignore-unfixed`** in CI gates to avoid blocking on unpatchable issues.
2. **Generate SBOMs at build time** (`--sbom=true`).
3. **Use minimal base images** (Distroless/Alpine) to eliminate 90% of baseline CVEs.
4. **Enforce `.trivyignore` expiration dates** to prevent permanent security debt.

---

[← Previous: 04-image-security.md](04-image-security.md) | [Next: 06-image-signing-and-provenance.md →](06-image-signing-and-provenance.md)
