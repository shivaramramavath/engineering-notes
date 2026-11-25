# Module 06: Production Engineering and Security

[<- Back to Docker Track Overview](../README.md) | [Next Module: 07-projects ->](../07-projects/README.md)

---

## Overview

Moving containers from local development workstations to enterprise production environments requires a fundamental shift in rigor. Production container engineering spans secret management without disk leakage, image optimization for fast cold starts, automated CI/CD pipeline builds, vulnerability scanning, cryptographic signing, registry management, structured logging, kernel resource budgeting, daemon tuning, cluster orchestration, and Kubernetes translation.

This module provides 14 deep-dive engineering guides covering the operational, architectural, and security standards needed to deploy and maintain containerized workloads reliably at scale.

---

## Learning Objectives

By completing this module, you will be able to:
- Securely inject credentials using BuildKit secret mounts, runtime environment variables, and Docker Secrets without leaking tokens in image layers.
- Optimize container images using multi-stage builds, layer minimization, and minimal distroless/alpine base images.
- Design high-speed, secure CI/CD pipelines incorporating layer caching, registry mirrors, and ephemeral build runners.
- Implement defense-in-depth security: non-root users, Linux capability dropping, read-only root filesystems, and seccomp/AppArmor profiling.
- Generate Software Bills of Materials (SBOMs) and automate CVE vulnerability scanning with Trivy and Grype.
- Sign images cryptographically with Cosign and Sigstore to enforce supply chain provenance.
- Configure enterprise container registries with automated retention policies, authentication, and vulnerability gating.
- Implement production tagging strategies (semantic versioning, git commit SHAs, immutable tags).
- Manage centralized logging drivers (JSON-file rotation, Syslog, Fluentd, Loki) and container metrics.
- Prevent noisy neighbors and host kernel panics using cgroups v2 memory limits, CFS CPU quotas, and OOM killer tuning.
- Configure and troubleshoot `dockerd` using `/etc/docker/daemon.json`, systemd overrides, and non-disruptive SIGHUP reloads.
- Evaluate Docker Swarm architecture, ingress routing mesh, and high-availability Raft consensus.
- Map Docker and Compose concepts seamlessly to Kubernetes primitives (Pods, Deployments, Services, Ingress, PVCs).
- Identify and remediate the top 20 container anti-patterns across Dockerfile design, networking, storage, and runtime operations.

---

## Module Roadmap

| Note | Title | Difficulty | Estimated Time | Key Focus Areas |
|:-----|:------|:-----------|:---------------|:----------------|
| **[01](./01-environments-and-secrets.md)** | Environments and Secrets Management | Intermediate | 45 min | `.env` hierarchy, BuildKit `--mount=type=secret`, 12-factor config, runtime injection |
| **[02](./02-image-optimisation.md)** | Production Image Optimisation | Advanced | 50 min | Multi-stage pruning, distroless, layer squashing, dive analysis, cold-start reduction |
| **[03](./03-docker-in-ci.md)** | Docker in CI/CD Pipelines | Advanced | 50 min | DinD vs DooD, GitHub Actions cache backends (gha, s3), buildx bake, parallel matrix |
| **[04](./04-image-security.md)** | Container Image Hardening and Security | Advanced | 55 min | Non-root execution, Linux capabilities, seccomp, AppArmor, rootless Docker |
| **[05](./05-scanning-and-sbom.md)** | Vulnerability Scanning and SBOM | Intermediate | 45 min | Trivy, Grype, Syft, CycloneDX, SPDX, automated CI security gates |
| **[06](./06-image-signing-and-provenance.md)** | Image Signing and Supply Chain Provenance | Advanced | 50 min | Cosign, Sigstore keyless signing, SLSA provenance, in-toto attestations |
| **[07](./07-registries.md)** | Enterprise Container Registries | Intermediate | 45 min | Docker Hub, AWS ECR, Harbor, GHCR, OCI distribution spec, retention rules |
| **[08](./08-tagging-and-release-strategies.md)** | Tagging and Release Strategies | Intermediate | 40 min | SemVer, Git SHA tagging, floating vs immutable tags, blue-green & canary deployments |
| **[09](./09-logging-drivers-and-monitoring.md)** | Logging Drivers and Metrics Monitoring | Intermediate | 50 min | JSON log rotation, Syslog, Fluentd, Loki, Prometheus daemon metrics, cAdvisor |
| **[10](./10-resource-management-and-oom.md)** | Resource Management and OOM Handling | Advanced | 50 min | cgroups v1 vs v2, CFS scheduler, `--memory`, `--cpus`, OOM killer score, exit code 137 |
| **[11](./11-docker-daemon-configuration-and-troubleshooting.md)** | Daemon Configuration & Troubleshooting | Advanced | 50 min | `daemon.json`, SIGHUP reload, data-root migration, systemd drop-ins, crash debugging |
| **[12](./12-swarm-overview-and-status.md)** | Docker Swarm Architecture & Status | Intermediate | 45 min | Raft consensus, overlay mesh, IPVS ingress, declarative stacks, Swarm vs K8s |
| **[13](./13-from-docker-to-kubernetes.md)** | From Docker to Kubernetes Migration | Advanced | 50 min | CRI runtime, dockershim deprecation, Pods, Deployments, Services, PVCs, migration |
| **[14](./14-anti-patterns-and-common-mistakes.md)** | Anti-Patterns and Common Mistakes | Intermediate | 50 min | Top 20 production anti-patterns, PID 1 signal swallower, socket exposure, remediation |

---

## Prerequisites

Before beginning this module, you should have completed:
- [Module 03: Dockerfile Deep Dive](../03-dockerfile/README.md)
- [Module 04: Storage and Networking](../04-volumes-and-networking/README.md)
- [Module 05: Docker Compose v2](../05-compose/README.md)

---

## Recommended Learning Path

```
  [01 Secrets & Config] ---------> [02 Image Optimisation] ---------> [03 Docker in CI]
           |                                                                  |
           v                                                                  v
  [04 Container Security] -------> [05 Scanning & SBOM] -----------> [06 Image Signing]
           |                                                                  |
           v                                                                  v
  [07 Registries & Storage] -----> [08 Tagging & Releases] --------> [09 Logging & Metrics]
           |                                                                  |
           v                                                                  v
  [10 cgroups & OOM Handling] ---> [11 Daemon Configuration] ------> [12 Swarm & Orchestration]
                                                                              |
                                                                              v
  [14 Anti-Patterns & Pitfalls] <--------------------------------- [13 From Docker to K8s]
```

---

[<- Back to Docker Track Overview](../README.md) | [Next Module: 07-projects ->](../07-projects/README.md)
