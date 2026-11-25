# 04-DevOps Master Learning Roadmap & Study Paths

This roadmap outlines structured learning sequences, role-based tracks, and estimated completion times for mastering Linux systems, version control, container runtimes, web servers, and cloud infrastructure.

---

## 1. Master Learning Paths Overview

| Path Name | Target Audience | Primary Focus | Total Est. Time |
|---|---|---|---|
| **1. Beginner DevOps Foundation** | New to infrastructure / Junior devs | Linux $\rightarrow$ Git $\rightarrow$ Docker $\rightarrow$ Nginx $\rightarrow$ AWS | 12–16 weeks |
| **2. Backend Developer Ops** | Fullstack & Backend engineers | Local containers, CI/CD pipelines, reverse proxying, cloud deployments | 6–8 weeks |
| **3. Container & Kubernetes Focused** | Platform / Infrastructure engineers | Linux namespaces/cgroups, Docker, multi-stage builds, Compose | 5–7 weeks |
| **4. Cloud Infrastructure (AWS Focused)** | Cloud architects & Sysadmins | IAM, VPC, multi-AZ networking, ECS, RDS, IaC (Terraform) | 8–10 weeks |
| **5. CI/CD & Automation Focused** | Release engineers & Automation leads | Git internals, GitHub Actions, reusable workflows, OIDC security | 4–6 weeks |
| **6. DevOps Technical Interview Prep** | Engineers preparing for SRE/DevOps roles | Core interview Q&As, scenario troubleshooting, system design | 3–4 weeks |

---

## 2. Path 1: Beginner DevOps Foundation (The Canonical Sequence)

```
[Phase 1: Linux Foundations]
       |  (Terminal navigation, permissions, systemd, bash scripting)
       v
[Phase 2: Git & Version Control]
       |  (Object model, branches, merge conflicts, interactive rebase)
       v
[Phase 3: Docker Containers]
       |  (Images, containers, multi-stage Dockerfiles, Compose)
       v
[Phase 4: Nginx Web Gateway]
       |  (Reverse proxy, SSL/TLS 1.3, rate limiting, caching)
       v
[Phase 5: AWS Cloud Operations]
          (VPC networking, IAM security, EC2/ECS workloads, S3)
```

### Phase 1: Linux Systems (Weeks 1–4)
- **Week 1: Foundations**: Filesystem Hierarchy Standard, navigation, user/group management, octal permissions (`chmod`, `chown`).
  - *Read:* [`linux/01-fundamentals/`](file:///04-devops/linux/01-fundamentals/)
- **Week 2: Shell & Scripting**: Pipes, redirection, text manipulation with `grep`/`sed`/`awk`, Bash scripting with `set -euo pipefail`.
  - *Read:* [`linux/02-shell/`](file:///04-devops/linux/02-shell/)
- **Week 3: System Administration**: Process management, signals, `systemd` unit files, `journalctl`, package managers (`apt`/`dnf`).
  - *Read:* [`linux/03-system/`](file:///04-devops/linux/03-system/)
- **Week 4: Networking & Security**: Socket inspection (`ss`, `ip`), OpenSSH hardening, firewall rules with `nftables`/UFW, Linux namespaces and cgroup v2.
  - *Read:* [`linux/04-networking/`](file:///04-devops/linux/04-networking/), [`linux/05-production/`](file:///04-devops/linux/05-production/), [`linux/06-advanced/`](file:///04-devops/linux/06-advanced/)

### Phase 2: Git & GitHub Actions (Weeks 5–7)
- **Week 5: Git Internals**: Blobs, trees, commits, staging area mechanics, branch pointers, commit graphs.
  - *Read:* [`git-github/01_git_fundamentals/`](file:///04-devops/git-github/01_git_fundamentals/)
- **Week 6: Daily Workflow & History Rewriting**: Patch staging, merge strategies, interactive rebase (`git rebase -i`), reflog disaster recovery.
  - *Read:* [`git-github/02_git_commands/`](file:///04-devops/git-github/02_git_commands/), [`git-github/03_history_rewriting/`](file:///04-devops/git-github/03_history_rewriting/)
- **Week 7: GitHub Actions CI/CD**: Workflow syntax, runners, build matrices, caching, artifact management, automated test pipelines.
  - *Read:* [`git-github/04_github_actions/`](file:///04-devops/git-github/04_github_actions/)

### Phase 3: Docker Containers (Weeks 8–10)
- **Week 8: Container Mechanics**: Docker architecture, namespaces vs VMs, image layers, container execution flags.
  - *Read:* [`docker/01_docker_fundamentals/`](file:///04-devops/docker/01_docker_fundamentals/), [`docker/02_docker_commands/`](file:///04-devops/docker/02_docker_commands/)
- **Week 9: Production Dockerfiles & Storage**: BuildKit caching, multi-stage optimization, named volumes, bridge networks.
  - *Read:* [`docker/03_dockerfile/`](file:///04-devops/docker/03_dockerfile/), [`docker/04_volumes_networking/`](file:///04-devops/docker/04_volumes_networking/)
- **Week 10: Multi-Container Stacks & Hardening**: Compose v2 specification, secret management, non-root user execution, image scanning.
  - *Read:* [`docker/05_docker_compose/`](file:///04-devops/docker/05_docker_compose/), [`docker/06_production/`](file:///04-devops/docker/06_production/)

### Phase 4: Nginx Web Gateway (Weeks 11–12)
- **Week 11: Reverse Proxy & TLS**: Virtual hosts, regex location matching, `proxy_pass` buffering, Let's Encrypt TLS 1.3 automation.
  - *Read:* [`nginx/01-fundamentals/`](file:///04-devops/nginx/01-fundamentals/), [`nginx/02-core-features/`](file:///04-devops/nginx/02-core-features/)
- **Week 12: Traffic Control & Hardening**: Reverse proxy caching, load balancing algorithms, rate limiting zones, troubleshooting 502/504 errors.
  - *Read:* [`nginx/03-traffic-management/`](file:///04-devops/nginx/03-traffic-management/), [`nginx/05-security-performance/`](file:///04-devops/nginx/05-security-performance/), [`nginx/06-production/`](file:///04-devops/nginx/06-production/)

### Phase 5: AWS Cloud Operations (Weeks 13–16)
- **Week 13: Cloud Security & IAM**: Multi-account strategy, IAM least privilege, role assumption, AWS CLI v2 scripting.
  - *Read:* [`aws/01-foundations/`](file:///04-devops/aws/01-foundations/)
- **Week 14: Compute & Containers**: EC2 Graviton instances, security groups, AWS Lambda serverless functions, ECS with AWS Fargate.
  - *Read:* [`aws/02-compute/`](file:///04-devops/aws/02-compute/)
- **Week 15: Storage, Databases & VPC**: Multi-AZ VPC architecture, public/private subnets, NAT Gateways, S3, RDS Aurora failover.
  - *Read:* [`aws/03-storage-and-databases/`](file:///04-devops/aws/03-storage-and-databases/), [`aws/04-networking/`](file:///04-devops/aws/04-networking/)
- **Week 16: Operations & IaC**: Declarative infrastructure with Terraform / OpenTofu, CloudWatch alarms, secrets management, cost optimization.
  - *Read:* [`aws/05-app-services/`](file:///04-devops/aws/05-app-services/), [`aws/06-operations/`](file:///04-devops/aws/06-operations/)

---

## 3. Path 2: Backend Developer Ops (6–8 Weeks)
Designed for backend engineers building microservices in Spring Boot, NestJS, Go, or Python FastAPI:
1. **Local Containerization**: Build efficient multi-stage Dockerfiles and orchestrate database + cache with `compose.yaml`.
   - Notes: [`docker/03_dockerfile/03_multi-stage-builds.md`](file:///04-devops/docker/03_dockerfile/03_multi-stage-builds.md), [`docker/05_docker_compose/02_compose-multi-service.md`](file:///04-devops/docker/05_docker_compose/02_compose-multi-service.md)
2. **Reverse Proxying & API Gateway**: Place Nginx in front of backend APIs for SSL termination and path routing.
   - Notes: [`nginx/02-core-features/reverse-proxy.md`](file:///04-devops/nginx/02-core-features/reverse-proxy.md), [`nginx/03-traffic-management/rate-limiting.md`](file:///04-devops/nginx/03-traffic-management/rate-limiting.md)
3. **Automated CI/CD**: Automate linting, unit testing, and Docker image publishing via GitHub Actions.
   - Notes: [`git-github/04_github_actions/14_ci-pipelines.md`](file:///04-devops/git-github/04_github_actions/14_ci-pipelines.md), [`git-github/04_github_actions/16_docker-ci-cd.md`](file:///04-devops/git-github/04_github_actions/16_docker-ci-cd.md)
4. **Cloud Deployment (AWS ECS Fargate)**: Deploy containerized APIs to managed serverless containers.
   - Notes: [`aws/02-compute/03-ecs-fargate.md`](file:///04-devops/aws/02-compute/03-ecs-fargate.md), [`aws/04-networking/01-vpc.md`](file:///04-devops/aws/04-networking/01-vpc.md)

---

## 4. Path 3: Cloud Infrastructure (AWS Focused, 8–10 Weeks)
Designed for cloud engineers and solutions architects:
1. **Identity & Governance**: IAM Roles, AWS Organizations, SCP policies, AWS CLI v2 (`aws/01-foundations/`).
2. **Enterprise Networking**: Custom VPCs, transit gateways, route tables, security groups, ALBs (`aws/04-networking/`).
3. **Compute & Storage Engine**: EC2 Graviton, Fargate tasks, S3 lifecycle, Aurora PostgreSQL clusters (`aws/02-compute/`, `aws/03-storage-and-databases/`).
4. **Automated Provisioning (IaC)**: Terraform / OpenTofu modules, remote state locking with S3 & DynamoDB (`aws/06-operations/01-infrastructure-as-code.md`).
5. **Observability & Cost Control**: CloudWatch metrics, alarms, Cost Allocation Tags, AWS Budgets (`aws/06-operations/04-observability.md`, `aws/06-operations/05-reliability-and-cost.md`).

---

## 5. Path 4: DevOps Technical Interview Prep (3–4 Weeks)
High-density revision track for technical interviews:
1. **Week 1: Linux & Systems Q&A**: Kernel concepts, process states, signals, permissions, troubleshooting.
   - Cheatsheet & Q&A: [`linux/reference/cheatsheet.md`](file:///04-devops/linux/reference/cheatsheet.md), [`linux/reference/interview.md`](file:///04-devops/linux/reference/interview.md)
2. **Week 2: Docker & Networking Q&A**: Container isolation, OverlayFS, cgroups, network namespaces, Compose.
   - Notes: [`docker/01_docker_fundamentals/`](file:///04-devops/docker/01_docker_fundamentals/), [`docker/04_volumes_networking/`](file:///04-devops/docker/04_volumes_networking/)
3. **Week 3: Nginx & Web Architecture Q&A**: Event loop, upstream load balancing, SSL handshakes, 502 vs 504 errors.
   - Cheatsheet & Q&A: [`nginx/08-reference/cheatsheet.md`](file:///04-devops/nginx/08-reference/cheatsheet.md), [`nginx/08-reference/interview-questions.md`](file:///04-devops/nginx/08-reference/interview-questions.md)
4. **Week 4: AWS Cloud Architecture Q&A**: High availability, multi-AZ failover, VPC design, IAM evaluation logic.
   - Cheatsheet & Q&A: [`aws/10-cheatsheets/`](file:///04-devops/aws/10-cheatsheets/), [`aws/09-interview/`](file:///04-devops/aws/09-interview/)
