---
title: "Self-Hosted Runners & Actions Runner Controller (ARC)"
category: "04-devops"
subcategory: "git-github / 04-github-actions"
tags: ["github-actions", "self-hosted-runners", "arc", "kubernetes", "autoscaling", "runner-security"]
difficulty: "Advanced"
estimated_read_time: "30 min"
prerequisites:
  - "04-github-actions/06-runners"
  - "04-github-actions/17-security-hardening"
last_updated: "2026-11-25"
---

# Self-Hosted Runners & Actions Runner Controller (ARC)

> **Navigation**: [← Previous: 20-oidc-and-cloud-auth.md](file:///e:/Learning/engineering-notes/04-devops/git-github/04-github-actions/20-oidc-and-cloud-auth.md) | [Module README](file:///e:/Learning/engineering-notes/04-devops/git-github/04-github-actions/README.md) | [Next: 22-workflow-performance-and-cost.md →](file:///e:/Learning/engineering-notes/04-devops/git-github/04-github-actions/22-workflow-performance-and-cost.md)

---

## Table of Contents
1. [What & Why](#1-what--why)
2. [Core Concepts](#2-core-concepts)
3. [How It Works Internally](#3-how-it-works-internally)
4. [Actions Runner Controller (ARC) Architecture](#4-actions-runner-controller-arc-architecture)
5. [Syntax & Configuration](#5-syntax--configuration)
6. [Practical Examples](#6-practical-examples)
7. [Best Practices & Security Hardening](#7-best-practices--security-hardening)
8. [Common Mistakes](#8-common-mistakes)
9. [Performance, Security & Cost](#9-performance-security--cost)
10. [Comparison: GitHub-Hosted vs Standalone Self-Hosted vs ARC](#10-comparison-github-hosted-vs-standalone-self-hosted-vs-arc)
11. [Interview Questions & Answers](#11-interview-questions--answers)
12. [Practice Exercises](#12-practice-exercises)
13. [Summary & Cheat Sheet](#13-summary--cheat-sheet)
14. [Further Reading](#14-further-reading)

---

## 1. What & Why

### What are Self-Hosted Runners?
Self-hosted runners are physical machines, virtual machines, or containerized pods running in your own infrastructure (AWS VPC, GCP, Azure, or on-premises datacenter) that register with GitHub to execute Actions jobs. 

### Why Do We Use Them?
1. **Private Network Access**: Access databases, internal Kubernetes clusters, LDAP directories, and internal package registries that reside behind corporate firewalls without exposing them to the public internet.
2. **Specialized Compute Hardware**: Leverage custom hardware architectures: ARM64 servers, massive 128-core build nodes, 512GB RAM instances for large monorepos, or NVIDIA GPUs for machine learning workloads.
3. **Drastic Cost Optimization**: For engineering teams running millions of build minutes monthly, self-hosted infrastructure (e.g. AWS Spot instances or existing Kubernetes clusters) can reduce CI compute costs by 70% to 90% compared to GitHub-hosted runner per-minute pricing.
4. **Persistent Local Caching**: Shared NVMe SSD caches for compiler artifacts (C/C++, Rust, Gradle) that persist across builds.

---

## 2. Core Concepts

### 1. Runner Scopes
- **Repository-level**: Registered to a single repository.
- **Organization-level**: Shared across all (or selected) repositories in an organization.
- **Enterprise-level**: Shared across multiple organizations within a GitHub Enterprise account.

### 2. Labels and Targeting
Workflows target runners via labels specified in `runs-on`:
```yaml
runs-on: [self-hosted, linux, arm64, gpu-large]
```
GitHub routes jobs to the runner matching **all** specified labels.

### 3. Ephemeral vs Persistent Runners
- **Persistent Runners**: The runner daemon stays alive across multiple jobs. **Anti-pattern**: Leaves state, leaked environment variables, and dirty workspaces between runs.
- **Ephemeral Runners**: The runner daemon executes **exactly one job** and terminates immediately (`--ephemeral` flag). The host or orchestrator recreates a pristine container/VM for the next job.

### 4. The Golden Security Rule for Self-Hosted Runners
> **NEVER use self-hosted runners on public repositories.**
Anyone can open a pull request against a public repository. If untrusted code runs on your self-hosted runner, the attacker gains direct access to your internal corporate network, secrets, and underlying host machine.

---

## 3. How It Works Internally

```
                Long-Polling Runner Architecture:

 [ Corporate VPC / Private Network ]           [ GitHub Cloud Infrastructure ]
┌───────────────────────────────────────┐     ┌───────────────────────────────┐
│ Actions Runner Daemon (actions-runner)│     │  GitHub Actions Service Bus   │
│                                       │     │                               │
│  1. Outbound HTTPS (Port 443) Long    │     │                               │
│     Poll to GitHub Message Queue      ├────►│  • Enqueues jobs for labels   │
│     (NO INBOUND PORTS OPENED!)        │     │  • No public IP required for  │
│                                       │     │    customer runner!           │
│  2. Receive Job Assignment Token      │     │                               │
│◄──────────────────────────────────────┤     │                               │
│                                       │     │                               │
│  3. Execute steps on local machine    │     │                               │
│  4. Stream logs & exit code via 443   │     │                               │
│──────────────────────────────────────►│     │  • Displays live logs in UI   │
└───────────────────────────────────────┘     └───────────────────────────────┘
```

The runner establishes an outbound WebSocket/HTTPS connection to GitHub. **No firewall inbound rules or public IP addresses are ever required.**

---

## 4. Actions Runner Controller (ARC) Architecture

**Actions Runner Controller (ARC)** is GitHub's official Kubernetes operator for managing auto-scaling, ephemeral runner pods at enterprise scale.

```
                  ARC Kubernetes Architecture:

               [ GitHub Actions Workflow Queues ]
                                │
                                ▼
                   (GitHub Webhook / AutoScaler)
                                │
                                ▼
┌─────────────────────────────────────────────────────────────┐
│ Kubernetes Cluster                                          │
│                                                             │
│   ┌─────────────────────────────────────────────────────┐   │
│   │ ARC Controller (actions-runner-controller)          │   │
│   │ • Monitors GitHub queue depth via AutoscalingListener│   │
│   └──────────────────────────┬──────────────────────────┘   │
│                              │                              │
│         ┌────────────────────┼────────────────────┐         │
│         ▼                    ▼                    ▼         │
│   ┌───────────┐        ┌───────────┐        ┌───────────┐   │
│   │ Pod 1     │        │ Pod 2     │        │ Pod 3     │   │
│   │ Ephemeral │        │ Ephemeral │        │ Ephemeral │   │
│   │ Runner    │        │ Runner    │        │ Runner    │   │
│   │ [Job A]   │        │ [Job B]   │        │ [Job C]   │   │
│   └─────┬─────┘        └─────┬─────┘        └─────┬─────┘   │
│         │                    │                    │         │
│         └────────────────────┴────────────────────┘         │
│             Terminates and purges immediately upon          │
│                    job completion (Single-Use)              │
└─────────────────────────────────────────────────────────────┘
```

### Key ARC Custom Resources (v2 Architecture):
1. **`AutoScalingRunnerSet`**: Defines the runner pod template, labels, and min/max replicas.
2. **`AutoScalingListener`**: Connects directly to GitHub via long-polling to monitor pending jobs and scales the RunnerSet instantaneously.

---

## 5. Syntax & Configuration

### 1. Targeting an ARC Runner Set in Workflows
```yaml
name: High-Performance Build
on:
  push:
    branches: [main]

jobs:
  build:
    # Target custom runner group or scale set name
    runs-on: arc-k8s-runners-16core
    steps:
      - uses: actions/checkout@v4
      - name: Compile large C++ codebase
        run: make -j16
```

### 2. ARC Helm Installation & RunnerSet Definition
```bash
# 1. Install ARC Controller via Helm
helm install arc \
    oci://ghcr.io/actions/actions-runner-controller-charts/gha-runner-scale-set-controller \
    -n arc-systems --create-namespace

# 2. Deploy an AutoScalingRunnerSet
helm install arc-runner-set \
    oci://ghcr.io/actions/actions-runner-controller-charts/gha-runner-scale-set \
    -n arc-runners --create-namespace \
    --set githubConfigUrl="https://github.com/my-org" \
    --set githubConfigSecret="github-token-secret" \
    --set minRunners=0 \
    --set maxRunners=50 \
    --values values.yaml
```

### Sample `values.yaml` for Ephemeral Pod Runners:
```yaml
githubConfigUrl: "https://github.com/my-org"

minRunners: 0
maxRunners: 30

template:
  spec:
    containers:
      - name: runner
        image: ghcr.io/actions/actions-runner:latest
        resources:
          requests:
            cpu: "4"
            memory: "8Gi"
          limits:
            cpu: "8"
            memory: "16Gi"
```

---

## 6. Practical Examples

### Example 1: Deploying an Ephemeral Standalone Runner (Docker)
```bash
# Register an ephemeral runner that runs 1 job and self-destructs
docker run -d --rm \
  --name ephemeral-runner \
  -e GITHUB_REPOSITORY="my-org/my-repo" \
  -e RUNNER_TOKEN="BCA12345EXAMPLETOKEN" \
  -e RUNNER_NAME="ephemeral-docker-01" \
  -e RUNNER_LABELS="linux,x64,docker-runner" \
  -e EPHEMERAL="true" \
  my-runner-image:latest
```

### Example 2: GPU-Accelerated ML Training Job
```yaml
name: GPU Model Training
on:
  workflow_dispatch:

jobs:
  train:
    runs-on: [self-hosted, gpu-a100, high-memory]
    steps:
      - uses: actions/checkout@v4
      - name: Verify CUDA Available
        run: nvidia-smi
      - name: Train PyTorch Model
        run: python train.py --epochs 50 --batch-size 128
```

### Example 3: Internal VPC Database Migration
```yaml
name: DB Migration
on:
  push:
    branches: [main]

jobs:
  migrate:
    # Runs on a runner inside the private AWS VPC
    runs-on: [self-hosted, private-vpc, us-east-1]
    steps:
      - uses: actions/checkout@v4
      - name: Run Flyway Migration against private RDS endpoint
        run: |
          ./flyway migrate -url="jdbc:postgresql://db.internal.vpc:5432/app"
```

---

## 7. Best Practices & Security Hardening

1. **Strictly Require Ephemeral Pods / VMs**: Always use single-use runners that are destroyed immediately after a job. This eliminates persistent malware or credential cross-contamination between workflows.
2. **Never Allow Untrusted Fork PRs to Run on Self-Hosted Runners**: If a repository must accept external pull requests, configure GitHub repository settings to require explicit manual approval from a maintainer before running workflows.
3. **Isolate Runner Network via Security Groups / NetworkPolicies**: The runner should only be allowed to communicate with the specific internal resources it needs (e.g., PostgreSQL port 5432), blocking access to internal metadata services (`169.254.169.254`).
4. **Use Rootless Podman or Container Mode**: Avoid mounting `/var/run/docker.sock` from the host node into runner containers, as Docker socket access grants root privilege escalation on the underlying host.
5. **Scale Down to Zero**: Configure `minRunners: 0` in ARC to avoid paying for idle compute during off-peak hours and weekends.
6. **Restrict Runner Groups**: Assign self-hosted runner groups strictly to specific approved private repositories rather than making them globally visible across the organization.

---

## 8. Common Mistakes

| Mistake | Consequence | Root Cause | Fix / Prevention |
|:---|:---|:---|:---|
| Self-hosted runner on public repo | Cryptomining attack; corporate network compromised | Malicious PR executed on runner | Never use self-hosted runners on public repos |
| Non-ephemeral persistent runner | Builds fail due to leftover files; leaked credentials | Runner recycled without wiping disk | Configure `--ephemeral` or use ARC |
| Mounting Docker socket (`docker.sock`) | Container breakout to host root | Attempting Docker-in-Docker via socket | Use rootless Docker or Kubernetes Pods |
| Inadequate Kubernetes resources | Runner pods crash with `OOMKilled` | Low memory limits on large builds | Configure generous CPU/RAM requests & limits |
| Allowing access to AWS metadata endpoint | Workflow extracts instance profile IAM credentials | Open access to `169.254.169.254` | Enforce IMDSv2 and block metadata IP in pod |
| Static runner pool running 24/7 | Skyrocketing cloud infrastructure costs | Not implementing autoscaling | Deploy ARC with scale-to-zero |

---

## 9. Performance, Security & Cost

- **Cost Analysis**: GitHub charges \$0.008/min for standard 2-core Linux runners. A 100-developer team utilizing 250,000 minutes/month spends ~\$2,000/mo on GitHub-hosted runners. Running ARC on AWS EKS Spot instances (c6i.xlarge, 4 vCPU, 8GB RAM) costs approximately \$450/mo, generating **75%+ cost savings**.
- **Build Speed**: Local caching on SSD volumes and bare-metal multi-core processors reduces compilation times for large C++, Rust, or monorepo suites from 45 minutes down to 8 minutes.
- **Security Posture**: GitHub-hosted runners offer guaranteed ephemeral microVM isolation (Hyper-V). Replicating this isolation in self-hosted environments requires Kata Containers, Firecracker, or disposable cloud VMs.

---

## 10. Comparison: GitHub-Hosted vs Standalone Self-Hosted vs ARC

| Dimension | GitHub-Hosted | Standalone Self-Hosted | Actions Runner Controller (ARC) |
|:---|:---|:---|:---|
| **Management Effort** | **Zero** | High (manual patching) | Medium (Kubernetes operator) |
| **Autoscaling** | Automatic | None (static VMs) | **Dynamic, event-driven (K8s)** |
| **Isolation** | Ephemeral Azure VM | Often persistent / dirty | **Ephemeral Single-Use Pods** |
| **VPC Connectivity** | None (requires VPN/tunnel) | Native VPC access | Native VPC / K8s access |
| **Custom Hardware** | Limited options | Any physical/VM hardware | Any K8s-supported node (GPU, ARM) |
| **Cost at Scale** | Linear / Expensive | Fixed VM costs | Highly optimized (Spot instances) |

---

## 11. Interview Questions & Answers

<details>
<summary><strong>Q1: Why is using self-hosted runners on public GitHub repositories considered a severe security vulnerability?</strong></summary>

Anyone on the internet can fork a public repository and open a pull request. In GitHub Actions, pull requests can trigger workflows automatically. If that workflow executes on a self-hosted runner, the attacker can submit malicious code in their PR (e.g. inside `run:` steps). This malicious code runs directly on your private server, allowing the attacker to:
1. Scan and pivot across your internal corporate network.
2. Read local environment variables, files, and IAM credentials attached to the host.
3. Install rootkits, backdoors, or launch cryptocurrency miners.
</details>

<details>
<summary><strong>Q2: How does the Actions Runner Controller (ARC) achieve autoscaling in Kubernetes?</strong></summary>

In the modern ARC (v2) architecture, ARC deploys an `AutoScalingListener` pod for each `AutoScalingRunnerSet`. The listener establishes an outbound HTTPS connection to GitHub's Actions service and receives real-time webhook/event notifications regarding pending jobs matching the RunnerSet's labels. When jobs enter the queue, ARC requests the Kubernetes API to immediately spawn the required number of ephemeral runner pods. Once a pod completes its single job, it automatically unregisters and Kubernetes deletes the pod.
</details>

<details>
<summary><strong>Q3: How do self-hosted runners communicate with GitHub without opening inbound firewall ports?</strong></summary>

Self-hosted runners use an outbound-only polling model. When the runner daemon (`actions-runner`) starts, it establishes an outbound long-polling HTTPS (port 443) connection to GitHub's message broker. GitHub responds with job assignments over this open outbound channel. All step logs and status updates are sent outbound over HTTPS to GitHub. Because communication is initiated from inside your network, no inbound firewall ports or public IP addresses are needed.
</details>

<details>
<summary><strong>Q4: What is an ephemeral runner, and why is it essential for production self-hosted infrastructure?</strong></summary>

An ephemeral runner is started with the `--ephemeral` flag. It registers with GitHub, accepts exactly **one** job, executes it, automatically unregisters itself from GitHub, and terminates.
This is essential because:
1. It guarantees that no modified files, rogue background processes, or cached state persist from one job to the next.
2. If a workflow runs malicious or buggy code, the environment is immediately incinerated upon job completion.
3. It prevents secrets exported into the environment from being read by subsequent jobs.
</details>

<details>
<summary><strong>Q5: What is the risk of mounting `/var/run/docker.sock` into a containerized runner pod, and what are the safer alternatives?</strong></summary>

Mounting `/var/run/docker.sock` gives the container root access to the Docker daemon of the host node. Any workflow running `docker run` can mount the host's root filesystem (`-v /:/host`) and achieve total root compromise of the underlying Kubernetes node.
**Safer Alternatives**:
1. Use **Kaniko** or **Buildah** for daemonless container image builds.
2. Use **Docker-in-Docker (dind)** with an isolated sidecar container rather than sharing the host socket.
3. Use rootless container engines.
</details>

<details>
<summary><strong>Q6: How do you configure a workflow to run only on a specific self-hosted runner pool?</strong></summary>

By specifying multiple labels in the `runs-on` array:
```yaml
runs-on: [self-hosted, linux, arm64, production-vpc]
```
GitHub's scheduler matches only runners that possess **all** of the specified labels.
</details>

<details>
<summary><strong>Q7: How do you prevent self-hosted runners from accessing cloud metadata services (e.g. AWS IMDS)?</strong></summary>

When runners execute in cloud environments (like AWS EC2 or EKS), attackers can query `http://169.254.169.254/latest/meta-data/` to steal the EC2 instance profile's IAM credentials.
To mitigate this:
1. Enforce **IMDSv2** with a hop limit of 1 (`HttpPutResponseHopLimit=1`), which prevents containerized pods from reaching the host's metadata endpoint.
2. Apply Kubernetes **NetworkPolicies** or iptables rules on the host blocking egress traffic to `169.254.169.254`.
</details>

<details>
<summary><strong>Q8: What is a Runner Group in GitHub Organization settings?</strong></summary>

A Runner Group is an administrative boundary used to manage access to pools of self-hosted runners. Instead of exposing runners to every repository, an organization admin can create a Runner Group (e.g. "Payment-Runners") and explicitly select which repositories are allowed to send jobs to those runners. This prevents unapproved projects from monopolizing high-cost compute or accessing sensitive VPC networks.
</details>

---

## 11. Practice Exercises

<details>
<summary><strong>Exercise 1: Inspect Labels on a Runner via GitHub CLI</strong></summary>

**Scenario**: Use `gh` CLI to list self-hosted runners and their configured labels.
```bash
gh api /orgs/my-org/actions/runners | jq '.runners[] | {name: .name, os: .os, status: .status, labels: [.labels[].name]}'
```
</details>

<details>
<summary><strong>Exercise 2: Write a Workflow Targeting Custom Runner Labels</strong></summary>

**Scenario**: Target an ARM64 self-hosted runner.
```yaml
name: ARM64 Build
on: push
jobs:
  build:
    runs-on: [self-hosted, linux, arm64]
    steps:
      - run: uname -m
```
</details>

<details>
<summary><strong>Exercise 3: Simulate Ephemeral Containerized Runner Execution</strong></summary>

**Scenario**: Start a runner container passing the `--ephemeral` flag.
```bash
./config.sh --url https://github.com/my-org/my-repo --token $RUNNER_TOKEN --ephemeral
./run.sh
# Runner terminates immediately after finishing one job!
```
</details>

<details>
<summary><strong>Exercise 4: Define an ARC AutoScalingRunnerSet Spec</strong></summary>

**Scenario**: Write a minimal Kubernetes YAML for an ARC RunnerSet.
```yaml
apiVersion: actions.github.com/v1alpha1
kind: AutoscalingRunnerSet
metadata:
  name: arc-k8s-runner
  namespace: arc-runners
spec:
  githubConfigUrl: "https://github.com/my-org"
  minRunners: 0
  maxRunners: 10
```
</details>

<details>
<summary><strong>Exercise 5: Block IMDSv2 Hop Limit in AWS</strong></summary>

**Scenario**: Modify an EC2 instance to restrict metadata access to the host only.
```bash
aws ec2 modify-instance-metadata-options \
    --instance-id i-1234567890abcdef0 \
    --http-tokens required \
    --http-put-response-hop-limit 1
```
</details>

<details>
<summary><strong>Exercise 6: Test Daemonless Image Building with Kaniko</strong></summary>

**Scenario**: Build Docker images in self-hosted Kubernetes without mounting `docker.sock`.
```yaml
steps:
  - uses: actions/checkout@v4
  - name: Build with Kaniko
    run: |
      /kaniko/executor \
        --context dir://. \
        --dockerfile ./Dockerfile \
        --destination my-registry.internal/app:latest \
        --no-push
```
</details>

---

## 12. Summary & Cheat Sheet

```yaml
# Self-Hosted Runner Targeting Cheat Sheet
jobs:
  job-custom-hardware:
    # Requires matching ALL specified labels
    runs-on: [self-hosted, linux, x64, gpu]
    timeout-minutes: 30
    steps:
      - uses: actions/checkout@v4
      - run: make test

# ARC Helm ScaleSet Installation
# helm install <release-name> oci://ghcr.io/actions/actions-runner-controller-charts/gha-runner-scale-set \
#   --namespace arc-runners \
#   --set githubConfigUrl="https://github.com/<org>" \
#   --set minRunners=0 \
#   --set maxRunners=20

# Golden Security Checklist:
# [x] Never use on public repositories
# [x] Always enforce ephemeral (single-use) lifecycles
# [x] Isolate private network egress via NetworkPolicies
# [x] Block access to cloud metadata services (169.254.169.254)
# [x] Never mount /var/run/docker.sock into runners
```

---

## 13. Further Reading
- [Official GitHub Docs - About Self-Hosted Runners](https://docs.github.com/en/actions/hosting-your-own-runners/managing-self-hosted-runners/about-self-hosted-runners)
- [Actions Runner Controller (ARC) GitHub Repository](https://github.com/actions/actions-runner-controller)
- [Hardening Self-Hosted Runners Guide](https://docs.github.com/en/actions/security-guides/security-hardening-for-github-actions#hardening-for-self-hosted-runners)
- [04-github-actions/22-workflow-performance-and-cost.md](file:///e:/Learning/engineering-notes/04-devops/git-github/04-github-actions/22-workflow-performance-and-cost.md)
