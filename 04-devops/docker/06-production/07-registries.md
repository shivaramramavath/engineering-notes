# Container Registries: Architecture, Cloud Providers, Harbor, and Mirroring

```
Track: DevOps & Containerization
Module: 06-production
Prerequisites: 01-fundamentals/07-registries-and-image-naming.md, 06-production/03-docker-in-ci.md
Difficulty: Intermediate to Advanced
Estimated Duration: 50 minutes
Target Docker Version: 27.x+
Last Validated: February 2025
```

---

## Table of Contents
1. [What & Why](#1-what--why)
2. [Core Concepts](#2-core-concepts)
3. [The OCI Distribution Specification Architecture](#3-the-oci-distribution-specification-architecture)
4. [Registry Landscape: Public, Cloud-Managed, and Self-Hosted](#4-registry-landscape-public-cloud-managed-and-self-hosted)
5. [Overcoming Docker Hub Rate Limits (Pull-Through Mirrors)](#5-overcoming-docker-hub-rate-limits-pull-through-mirrors)
6. [Secure Authentication & Credential Helpers](#6-secure-authentication--credential-helpers)
7. [Syntax & Implementation Reference](#7-syntax--implementation-reference)
8. [Practical Implementation: Running a Secure Local & Self-Hosted Registry](#8-practical-implementation-running-a-secure-local--self-hosted-registry)
9. [Enterprise Self-Hosting with CNCF Harbor](#9-enterprise-self-hosting-with-cncf-harbor)
10. [Production Best Practices & Antipatterns](#10-production-best-practices--antipatterns)
11. [Common Pitfalls & Troubleshooting](#11-common-pitfalls--troubleshooting)
12. [Performance Tuning & Resource Optimization](#12-performance-tuning--resource-optimization)
13. [Security Hardening & Threat Model](#13-security-hardening--threat-model)
14. [Architectural Comparison Table: Registry Providers](#14-architectural-comparison-table-registry-providers)
15. [Real-World Scenario Walkthrough](#15-real-world-scenario-walkthrough)
16. [Self-Assessment & Interview Questions](#16-self-assessment--interview-questions)
17. [Hands-on Practice Exercises](#17-hands-on-practice-exercises)
18. [Summary & Reference Cheat Sheet](#summary--reference-cheat-sheet)

---

## 1. What & Why

A **Container Registry** is the central distribution hub of the container lifecycle. It is a specialized, content-addressable HTTP service responsible for storing, indexing, and serving OCI image manifests, layer blobs, signatures, and SBOMs.

Managing container registries effectively is critical for production operations:
- **Rate Limit Resilience**: Unauthenticated Docker Hub pulls face strict rate limits (100 pulls per 6 hours per IP address), causing cascading deployment failures during major cloud autoscaling events.
- **Latency & Cloud Egress Optimization**: Pulling multi-gigabyte images from third-party public registries introduces network latency and high cloud egress fees. Co-locating private registries within local cloud VPCs (e.g. AWS ECR) delivers sub-second pulls and zero egress fees.
- **Supply Chain Governance**: Enterprise registries must enforce Role-Based Access Control (RBAC), automated vulnerability scanning, tag immutability, and compliance auditing.

---

## 2. Core Concepts

### OCI Image Manifests vs Layer Blobs
A container registry does not store images as monolithic files:
- **Blobs (`/v2/<name>/blobs/sha256:...`)**: The immutable compressed tar layers containing filesystem diffs and the container configuration JSON. Blobs are content-addressable; identical layers shared across multiple images are stored only once (global deduplication).
- **Manifests (`/v2/<name>/manifests/<tag>`)**: A lightweight JSON document mapping an image tag to its configuration hash and ordered list of layer blob digests.
- **Index / Manifest List**: An umbrella JSON document pointing to architecture-specific manifests (e.g. `linux/amd64` and `linux/arm64`).

---

## 3. The OCI Distribution Specification Architecture

```
+-------------------------------------------------------------------------+
|                  OCI DISTRIBUTION SPEC PROTOCOL (HTTP API)              |
+-------------------------------------------------------------------------+
| Client (Docker Engine)                   OCI Registry (ECR / Harbor)    |
|                                                                         |
| 1. GET /v2/my-app/manifests/v1.0 -------------------------------------> |
|    <---------------------------------- Returns Manifest JSON (Layers)   |
|                                                                         |
| 2. Check Local Cache: Do I have Layer Blob sha256:1a2b...?              |
|    - Layer exists locally: SKIP DOWNLOAD!                               |
|                                                                         |
| 3. GET /v2/my-app/blobs/sha256:4c5d... (Missing Layer) -------------->  |
|    <---------------------------------- Streams Compressed Tar Layer     |
|                                                                         |
| Client unpacks layer tar into OverlayFS2 storage -> Image Ready!        |
+-------------------------------------------------------------------------+
```

---

## 4. Registry Landscape: Public, Cloud-Managed, and Self-Hosted

1. **Public SaaS Registries**:
   - **Docker Hub**: The default registry. Massive public catalog, but enforces rate limits and public visibility by default.
   - **GitHub Container Registry (GHCR)**: Native integration with GitHub Actions, scoped to GitHub organizations and personal accounts.
2. **Cloud-Managed Private Registries**:
   - **AWS Elastic Container Registry (ECR)**: Deep integration with AWS IAM, VPC Endpoints (PrivateLink for zero-egress pulls), and automated KMS encryption.
   - **Google Artifact Registry (GAR)**: Successor to GCR; supports OCI containers, Maven, npm, and Python wheels under a single managed service.
   - **Azure Container Registry (ACR)**: Deep Azure AD integration, automated geo-replication, and quarantine workflows.
3. **Enterprise Self-Hosted Registries**:
   - **CNCF Harbor**: Feature-rich open-source enterprise registry with built-in RBAC, LDAP/OIDC auth, Trivy vulnerability scanning, Cosign signature validation, and replication rules.
   - **CNCF Distribution (Registry v2)**: The ultra-minimal reference open-source registry binary provided by Docker/OCI.

---

## 5. Overcoming Docker Hub Rate Limits (Pull-Through Mirrors)

Docker Hub rate limits anonymous IP addresses to **100 pulls per 6 hours**, and authenticated free accounts to **200 pulls per 6 hours**. In shared corporate networks or cloud NAT gateways (e.g. AWS NAT Gateway sharing a single public IP across 500 EC2 instances), these rate limits are exhausted in minutes.

### The Solution: Pull-Through Cache / Registry Mirror
Configure the local Docker daemon to query a local cache mirror before contacting public registries:

```json
// In /etc/docker/daemon.json
{
  "registry-mirrors": [
    "https://mirror.gcr.io",
    "https://harbor.corp.internal/v2/dockerhub-proxy"
  ]
}
```

When a container pulls `alpine:3.21`, Docker queries the local mirror first. If cached, the image downloads at gigabit LAN speed with zero Docker Hub rate limit consumption!

---

## 6. Secure Authentication & Credential Helpers

### The Plaintext Storage Hazard
By default, running `docker login` saves your registry username and password in **plaintext base64** in `~/.docker/config.json`:
```json
{
  "auths": {
    "https://index.docker.io/v1/": {
      "auth": "dXNlcm5hbWU6cGFzc3dvcmQ=" // PLAINTEXT BASE64!
    }
  }
}
```
Any process or malicious script running on your machine can read and decode this file.

### Secure Native Credential Helpers
Modern systems use native OS credential stores:
- **Linux**: `pass` or `secretservice` (`docker-credential-secretservice`)
- **macOS**: Native Keychain (`docker-credential-osxkeychain`)
- **Windows**: Windows Credential Manager (`docker-credential-wincred`)
- **AWS ECR**: IAM Credential Helper (`docker-credential-ecr-login` - generates temporary 12-hour tokens automatically without static credentials).

Configuration in `~/.docker/config.json`:
```json
{
  "credsStore": "osxkeychain",
  "credHelpers": {
    "public.ecr.aws": "ecr-login",
    "*.dkr.ecr.us-east-1.amazonaws.com": "ecr-login"
  }
}
```

---

## 7. Syntax & Implementation Reference

### CLI Registry Operations

```bash
# 1. Authenticate with a private registry
docker login ghcr.io -u myusername -p ghp_mytoken

# 2. Tag a local image for a private registry
# Convention: <registry_domain>[:<port>]/[namespace/]<repository>:<tag>
docker tag my-api:latest ghcr.io/mycorp/my-api:1.0.0

# 3. Push to private registry
docker push ghcr.io/mycorp/my-api:1.0.0

# 4. Pull from private registry
docker pull ghcr.io/mycorp/my-api:1.0.0

# 5. Log out and purge local credentials
docker logout ghcr.io
```

---

## 8. Practical Implementation: Running a Secure Local & Self-Hosted Registry

Deploying a lightweight, private OCI registry on a local network using the official `registry:2` container.

```bash
# 1. Create persistent storage directory
mkdir -p /opt/registry/data /opt/registry/certs

# 2. Generate self-signed TLS certificate (or use Let's Encrypt)
openssl req -newkey rsa:4096 -nodes -sha256 -keyout /opt/registry/certs/domain.key \
  -x509 -days 365 -out /opt/registry/certs/domain.crt \
  -subj "/CN=registry.local"

# 3. Launch secure local registry with TLS and persistent volume
docker run -d \
  --name local-registry \
  --restart=always \
  -p 5000:5000 \
  -v /opt/registry/data:/var/lib/registry \
  -v /opt/registry/certs:/certs \
  -e REGISTRY_HTTP_TLS_CERTIFICATE=/certs/domain.crt \
  -e REGISTRY_HTTP_TLS_KEY=/certs/domain.key \
  registry:2

# 4. Tag and push an image to the local registry
docker tag alpine:3.21 localhost:5000/my-alpine:v1
docker push localhost:5000/my-alpine:v1

# 5. Query registry catalog via HTTP API
curl -k https://localhost:5000/v2/_catalog
# Outputs: {"repositories":["my-alpine"]}
```

---

## 9. Enterprise Self-Hosting with CNCF Harbor

For enterprise production deployments, **Harbor** is the de facto open-source registry platform.

```
+-------------------------------------------------------------------------+
|                         CNCF HARBOR ARCHITECTURE                        |
+-------------------------------------------------------------------------+
| [Harbor Core / UI] <---> [PostgreSQL] (Metadata & RBAC)                 |
|        ^                                                                |
|        v                                                                |
| [Registry Core]    <---> [Object Storage] (S3 / MinIO / Local FS)       |
|        ^                                                                |
|        +---> [Trivy Scanner]   (Automatic CVE scan on push)             |
|        +---> [Cosign Verifier] (Enforce signature policies)             |
|        +---> [Replication Engine] (Sync images to remote data centers)  |
+-------------------------------------------------------------------------+
```

### Harbor Key Features
- **Project-Level RBAC**: Segregate images by project (`finance`, `core-platform`, `interns`) with fine-grained read/write permissions.
- **Robot Accounts**: Dedicated service accounts with limited expiration tokens for CI/CD runners.
- **Tag Immutability Rules**: Prevent developers or CI scripts from overwriting existing release tags (e.g. `v1.0.0`).
- **Automated Lifecycle Retention Policies**: Automatically purge untagged images or retain only the last 10 tags per repository.

---

## 10. Production Best Practices & Antipatterns

### Best Practices
- **Enable Tag Immutability**: Configure registry rules to block force-pushing over existing semantic tags (`v1.0.0`).
- **Implement Automated Lifecycle Policies**: Automatically delete untagged intermediate build layers older than 14 days to prevent storage bloat.
- **Use Cloud VPC Endpoints for Registries**: In AWS, configure an S3 / ECR Gateway VPC Endpoint so ECS and EKS nodes pull images over internal AWS backbones rather than the public internet.
- **Deploy Registry Mirrors for Large Fleets**: Use pull-through caches in enterprise networks to protect against Docker Hub rate limits.

### Anti-Patterns
- **Using Insecure HTTP Registries in Production**: Running unencrypted registries (`http://`) without TLS exposes credentials and image layers to network sniffing.
- **Storing Plaintext Passwords in CI Configuration**: Hardcoding passwords in pipeline scripts; always use short-lived OIDC tokens or IAM credential helpers.
- **Treating Registries as Permanent Garbage Dumps**: Neglecting retention rules allows abandoned experimental tags to balloon storage costs to thousands of dollars per month.

---

## 11. Common Pitfalls & Troubleshooting

| Symptom | Root Cause | Diagnosis Command | Permanent Fix |
|:---|:---|:---|:---|
| `toomanyrequests: You have reached your pull rate limit` | Exceeded Docker Hub anonymous rate limits (100 pulls / 6 hrs) | Check Docker Hub response headers | Authenticate via `docker login`, deploy a pull-through cache, or mirror to ECR/GHCR. |
| `http: server gave HTTP response to HTTPS client` | Docker defaults to HTTPS; private registry running on plain HTTP | `docker push <host>:5000/...` | Install TLS certificate on registry or add host to `insecure-registries` in `daemon.json`. |
| `x509: certificate signed by unknown authority` | Registry uses self-signed TLS cert not trusted by host OS | `openssl s_client -connect <host>:5000` | Copy registry CA certificate to `/etc/docker/certs.d/<host>:5000/ca.crt`. |
| `docker push` fails with `name unknown: The repository with name 'xyz' does not exist` | AWS ECR requires repositories to be explicitly created before pushing | `aws ecr describe-repositories` | Create repo via CLI: `aws ecr create-repository --repository-name xyz`. |
| AWS ECR login fails after 12 hours | AWS ECR authentication tokens have a hardcoded 12-hour expiration window | Run `aws ecr get-login-password` | Use `docker-credential-ecr-login` helper for automated dynamic token refresh. |
| Registry runs out of disk space despite deleting images via UI | Deleting manifests does not automatically trigger garbage collection on underlying blobs | Check registry host disk `df -h` | Run registry garbage collection: `registry garbage-collect /etc/docker/registry/config.yml`. |

---

## 12. Performance Tuning & Resource Optimization

1. **Registry Blob Storage on Cloud Object Stores (S3/GCS)**:
   Never back enterprise registries with host block storage. Configure the registry storage driver to use Amazon S3 or Google Cloud Storage, which provide infinite elasticity, 99.999999999% durability, and high multi-part download throughput.
2. **Layer Compression Tuning**:
   In high-throughput registries, serve pre-compressed Zstandard (`zstd`) layers. Zstd decompression consumes 70% less CPU on client nodes during large image pulls.

---

## 13. Security Hardening & Threat Model

```
+--------------------------------------------------------------+
| ATTACK: Man-in-the-Middle (MITM) Layer Injection             |
+--------------------------------------------------------------+
| Developer pulls image over insecure HTTP registry connection.|
| Attacker on local LAN executes ARP spoofing.                 |
| Attacker intercepts blob download and swaps `bash` with a   |
| reverse shell backdoor.                                      |
|                                                              |
| PERMANENT DEFENSE:                                           |
| 1. Enforce strict TLS (HTTPS only).                          |
| 2. Never whitelist `insecure-registries` in production.      |
| 3. Cryptographically verify image digests with Cosign.       |
+--------------------------------------------------------------+
```

---

## 14. Architectural Comparison Table: Registry Providers

| Feature | Docker Hub | AWS ECR | GitHub Packages (GHCR) | CNCF Harbor |
|:---|:---|:---|:---|:---|
| **Hosting Model** | Public Cloud SaaS | AWS Cloud Managed | GitHub Cloud Managed | Self-Hosted (Kubernetes/VM) |
| **Authentication** | Docker ID / Tokens | **Native AWS IAM** | GitHub Personal Access Tokens | **LDAP / OIDC / Robot Accounts** |
| **VPC Endpoint Support**| No | **Yes (AWS PrivateLink)**| No | Yes (Internal LAN) |
| **Vulnerability Scanning**| Docker Scout | Native AWS Inspector | Limited | **Native Trivy / Grype** |
| **Tag Immutability** | Paid Tier Only | **Yes (Native)** | Yes | **Yes (Configurable Rules)** |
| **Pull Rate Limits** | 100-200 pulls / 6h | **None (AWS Bandwidth)** | High | **None (Self-hosted)** |

---

## 15. Real-World Scenario Walkthrough

### Scenario: Resolving Production CI Failure Caused by Docker Hub Rate Limits

#### The Outage
A company with 80 developers and 400 daily CI/CD pipeline builds experienced total pipeline gridlock at 11:00 AM. Every single build failed with:
`Error response from daemon: toomanyrequests: You have reached your pull rate limit`.

#### Investigation
The entire company routed outbound internet traffic through a single corporate NAT Gateway IP (`203.0.113.50`). Docker Hub treated all 400 builds as originating from the same anonymous user, exhausting the 100-pull limit within 45 minutes of the workday starting.

#### The Architectural Solution
1. **Deployed an AWS ECR Pull-Through Cache**:
   Configured AWS ECR to act as an automated caching proxy for Docker Hub:
   ```bash
   aws ecr create-pull-through-cache-rule \
     --ecr-repository-prefix docker-hub \
     --upstream-registry-url registry-1.docker.io
   ```
2. **Updated Base Image Pointers**:
   Changed Dockerfiles from `FROM node:22-alpine` to:
   `FROM <aws_account_id>.dkr.ecr.us-east-1.amazonaws.com/docker-hub/library/node:22-alpine`.
3. **Outcome**:
   - Docker Hub images are pulled only once from Docker Hub; all subsequent pulls originate directly from internal AWS ECR over high-speed 25Gbps VPC backbones.
   - Zero rate limit errors; image pull latency dropped from 12s to **1.1s**.

---

## 16. Self-Assessment & Interview Questions

<details>
<summary><strong>1. What is an OCI Image Index (or Manifest List), and why is it necessary for multi-platform images?</strong></summary>

**Answer:**
An OCI Image Index is an umbrella JSON manifest that references multiple platform-specific image manifests under a single unified tag (e.g. `python:3.12`). Each entry specifies a target operating system and architecture (`linux/amd64`, `linux/arm64`). When a Docker client pulls the image, the engine inspects the Image Index and automatically downloads only the specific manifest and layer blobs matching the host's CPU architecture.
</details>

<details>
<summary><strong>2. Why is storing credentials in plaintext in `~/.docker/config.json` dangerous, and how do credential helpers resolve this?</strong></summary>

**Answer:**
By default, `docker login` stores usernames and passwords as base64-encoded plaintext in `~/.docker/config.json`, which can be trivially read by any script or malware running under that user account. Credential helpers delegate secret storage to the host OS's native secure keyring (such as macOS Keychain, Windows Credential Manager, or Linux Secret Service) or generate temporary short-lived cloud IAM tokens (like `docker-credential-ecr-login`), keeping static credentials off the filesystem.
</details>

<details>
<summary><strong>3. How does a Docker registry achieve layer deduplication across completely different repositories?</strong></summary>

**Answer:**
OCI registries are content-addressable storage systems. Every layer is identified exclusively by the cryptographic SHA256 digest of its contents. If Repository A and Repository B both inherit from `alpine:3.21`, the registry detects that the underlying base layer blobs have identical SHA256 hashes and stores only a single physical copy of those blobs on disk, linking both manifests to the same shared blob.
</details>

<details>
<summary><strong>4. What is a "pull-through cache" mirror, and how does it protect against Docker Hub rate limits?</strong></summary>

**Answer:**
A pull-through cache (such as Harbor or AWS ECR Pull-Through Cache) is a local caching proxy deployed within your network. When a container requests an upstream image, the request routes to the cache mirror. If the image is cached locally, it is returned immediately without contacting Docker Hub. If not cached, the mirror fetches the image from Docker Hub once, stores it, and serves subsequent requests locally, drastically reducing external rate limit usage.
</details>

<details>
<summary><strong>5. What is the difference between deleting an image tag and running registry garbage collection?</strong></summary>

**Answer:**
Deleting an image tag in a registry merely removes the manifest reference (the pointer). The underlying layer blobs remain physically stored on disk or S3 to ensure other images sharing those layers are not broken. To physically free up storage space, an administrator must execute **registry garbage collection**, which scans the storage backend, identifies orphaned blobs that are no longer referenced by any manifest, and permanently deletes them.
</details>

<details>
<summary><strong>6. Why should enterprise registries enforce "Tag Immutability"?</strong></summary>

**Answer:**
Tag immutability prevents existing tags (such as `v1.2.0`) from being overwritten by new pushes. This guarantees that production deployments are deterministic and repeatable, prevents malicious actors from overwriting vetted release images with malware, and ensures that audit trails accurately reflect the exact software version deployed.
</details>

<details>
<summary><strong>7. How do you configure a Docker daemon to trust a self-signed private registry certificate without using `insecure-registries`?</strong></summary>

**Answer:**
Copy the registry's root CA certificate (`ca.crt`) into the Docker daemon's certificate trust directory:
`/etc/docker/certs.d/<registry_hostname>:<port>/ca.crt`
The Docker daemon will automatically use this CA certificate to establish an encrypted TLS connection with the private registry without disabling security verification.
</details>

<details>
<summary><strong>8. What is the primary operational advantage of AWS ECR over Docker Hub for workloads deployed on AWS ECS or EKS?</strong></summary>

**Answer:**
AWS ECR integrates natively with AWS IAM (eliminating static passwords via IAM roles), supports PrivateLink VPC Endpoints (allowing container nodes to pull images over internal AWS network backbones with zero internet gateway or data egress costs), enforces tag immutability natively, and eliminates Docker Hub's pull rate limits.
</details>

---

## 17. Hands-on Practice Exercises

<details>
<summary><strong>Exercise 1: Deploying an Ephemeral In-Memory Local Registry</strong></summary>

**Goal:** Start a local registry on port 5001, push an image to it, and pull it back.

**Step-by-step Solution:**
1. Start registry:
   ```bash
   docker run -d -p 5001:5000 --name test-reg registry:2
   ```
2. Retag image:
   ```bash
   docker tag alpine:3.21 localhost:5001/test-alpine:v1
   ```
3. Push:
   ```bash
   docker push localhost:5001/test-alpine:v1
   ```
4. Query catalog:
   ```bash
   curl http://localhost:5001/v2/_catalog
   # {"repositories":["test-alpine"]}
   ```
5. Clean up: `docker rm -f test-reg`
</details>

<details>
<summary><strong>Exercise 2: Auditing Plaintext Credentials in `~/.docker/config.json`</strong></summary>

**Goal:** Check if your local environment is exposing plaintext base64 credentials.

**Step-by-step Solution:**
1. Inspect config:
   ```bash
   cat ~/.docker/config.json | grep -i auth
   ```
2. If `auth` strings appear, decode them:
   ```bash
   echo "<base64_string>" | base64 --decode
   # Displays username:password!
   ```
3. Solution: Configure a native credential helper (`credsStore`).
</details>

<details>
<summary><strong>Exercise 3: Inspecting OCI Manifest Layers via Curled Registry API</strong></summary>

**Goal:** Retrieve the raw JSON manifest of an image directly using `curl`.

**Step-by-step Solution:**
1. Push an image to `localhost:5001`.
2. Query manifest with OCI accept header:
   ```bash
   curl -H "Accept: application/vnd.oci.image.manifest.v1+json" \
     http://localhost:5001/v2/test-alpine/manifests/v1 | jq .
   ```
3. Inspect `schemaVersion`, `config`, and array of `layers`.
</details>

<details>
<summary><strong>Exercise 4: Configuring a Registry Mirror in `daemon.json`</strong></summary>

**Goal:** Configure Google's public mirror in `/etc/docker/daemon.json`.

**Step-by-step Solution:**
1. Edit `/etc/docker/daemon.json`:
   ```json
   {
     "registry-mirrors": ["https://mirror.gcr.io"]
   }
   ```
2. Reload daemon: `sudo systemctl reload docker`
3. Verify with `docker info | grep -A 2 "Registry Mirrors"`.
</details>

<details>
<summary><strong>Exercise 5: Creating an ECR Repository and Pushing via AWS CLI</strong></summary>

**Goal:** Authenticate with AWS ECR using temporary login tokens.

**Step-by-step Solution:**
1. Retrieve password and login:
   ```bash
   aws ecr get-login-password --region us-east-1 | \
     docker login --username AWS --password-stdin <aws_account_id>.dkr.ecr.us-east-1.amazonaws.com
   ```
2. Verify login succeeds.
</details>

<details>
<summary><strong>Exercise 6: Running Registry Garbage Collection</strong></summary>

**Goal:** Test running dry-run garbage collection inside a registry container.

**Step-by-step Solution:**
1. Run GC command:
   ```bash
   docker exec test-reg bin/registry garbage-collect --dry-run /etc/docker/registry/config.yml
   ```
</details>

---

## 18. Summary & Reference Cheat Sheet

### Essential Naming Conventions
```
[registry_host[:port]/][namespace/]repository[:tag|@sha256:digest]

Examples:
alpine:3.21                                  # Docker Hub official library
ghcr.io/myorg/api:1.0.0                      # GitHub Container Registry
123456789.dkr.ecr.us-east-1.amazonaws.com/api:v1 # AWS ECR
localhost:5000/my-image:latest               # Local Private Registry
```

### Top 4 Production Rules
1. **Never commit plaintext credentials**: Use native credential helpers (`credsStore`).
2. **Deploy pull-through cache mirrors** to bypass Docker Hub rate limits.
3. **Enable Tag Immutability** on production repositories.
4. **Co-locate registries in local cloud VPCs** to eliminate egress fees and minimize latency.

---

[← Previous: 06-image-signing-and-provenance.md](06-image-signing-and-provenance.md) | [Next: 08-tagging-and-release-strategies.md →](08-tagging-and-release-strategies.md)
