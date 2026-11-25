# 04-DevOps: Systems, Cloud & Infrastructure Curriculum

Welcome to the **DevOps & Cloud Systems Architecture** learning area. This area provides deep-dive, production-grade engineering documentation, reference architectures, CLI recipes, and operational runbooks across five core tracks: **Linux**, **Git & GitHub Actions**, **Docker**, **Nginx**, and **Amazon Web Services (AWS)**.

---

## 1. Tracks Overview & Status

| Track | Directory | Description | Scope / Tech Baseline | Status |
|---|---|---|---|---|
| **Linux Systems** | [`linux/`](file:///04-devops/linux/) | OS fundamentals, kernel namespaces, cgroups, systemd, shell scripting, networking, and performance tuning | Ubuntu 24.04 LTS, Kernel 6.12 LTS, systemd 255+ | In Progress (Foundation ready) |
| **Git & GitHub Actions** | [`git-github/`](file:///04-devops/git-github/) | Version control internals, object trees, history rewriting, CI/CD pipelines, reusable workflows, and OIDC | Git 2.56+, GitHub Actions Node 20 runners | In Progress (Foundation ready) |
| **Docker Containers** | [`docker/`](file:///04-devops/docker/) | Container runtime internals, multi-stage builds, rootless execution, Compose v2, volumes, and networking | Docker Engine v28+, Compose v2.30+, Buildx | In Progress (Foundation ready) |
| **Nginx Web Gateway** | [`nginx/`](file:///04-devops/nginx/) | Reverse proxying, TLS 1.3 termination, HTTP/3 (QUIC), load balancing, rate limiting, and edge security | Nginx 1.30+ Stable / 1.31+ Mainline | In Progress (Foundation ready) |
| **Amazon Web Services (AWS)** | [`aws/`](file:///04-devops/aws/) | Cloud foundations, IAM least privilege, VPC networking, compute (EC2/ECS/Lambda), storage (S3/DynamoDB), and IaC | AWS CLI v2.22+, AL2023, Terraform/OpenTofu | Complete (101 notes across 10 modules) |

---

## 2. Shared Governance & Standards

All tracks in `04-devops/` adhere to a unified engineering standard:
- **[CONVENTIONS.md](file:///04-devops/CONVENTIONS.md)**: Master naming convention (`NN-kebab-case`), strict markdown encapsulation, and metadata schema.
- **[_templates/concept-template.md](file:///04-devops/_templates/concept-template.md)**: Authoritative 13-section concept template.
- **[_templates/project-template.md](file:///04-devops/_templates/project-template.md)**: Authoritative 5-section reference project template.
- **[ROADMAP.md](file:///04-devops/ROADMAP.md)**: Recommended learning paths with estimated completion times.
- **[GLOSSARY.md](file:///04-devops/GLOSSARY.md)**: Master lexicon of DevOps, Linux, container, and cloud terms.
- **[INDEX.md](file:///04-devops/INDEX.md)**: Topic-to-note catalog cross-linking all 5 tracks.
- **[CONTRIBUTING.md](file:///04-devops/CONTRIBUTING.md)**: Contribution guidelines and code verification standards.

---

## 3. Cross-Track Integration Map

DevOps does not exist in a vacuum; it directly operationalizes the application tracks across the repository. The following table establishes explicit cross-track integration points:

| DevOps Component | Operationalizes Track | Real-World Interaction & Relative Cross-Link |
|---|---|---|
| **Docker Multi-Stage Build** | [`01-languages/java/`](file:///01-languages/java/) | Multi-stage JRE Alpine packaging, CDS archives, and jlink runtime minimization |
| **Docker & Compose** | [`03-backend/spring-boot/`](file:///03-backend/spring-boot/) | Spring Boot 3.4+ layered JARs, Testcontainers `@ServiceConnection`, and compose profiles |
| **Nginx Reverse Proxy** | [`03-backend/nestjs/`](file:///03-backend/nestjs/) | SSL/TLS termination, request routing, WebSockets proxying, and rate limiting NestJS APIs |
| **Container & Cloud Ingress** | [`03-backend/fastapi/`](file:///03-backend/fastapi/) | Uvicorn worker process supervision, Gunicorn process managers, and Docker deployment |
| **Edge Routing & Caching** | [`02-frontend/nextjs/`](file:///02-frontend/nextjs/) | Static asset caching, header forwarding, SSR reverse proxying, and standalone output |
| **Linux Volumes & Storage** | [`09-sql/`](file:///09-sql/) | PostgreSQL/MySQL disk IOPS, WAL journal mount tuning, and persistent volume provisioning |
| **AWS VPC & Network Security** | [`07-system-design/`](file:///07-system-design/) | High-availability multi-AZ architectures, subnets, NAT Gateways, and ALB failover |
| **Git & AWS Secrets (OIDC)** | [`06-project-patterns/`](file:///06-project-patterns/) | Zero-trust authentication, IAM role assumption, and automated secrets rotation |

---

## 4. How to Use This Curriculum

1. **Follow a Guided Learning Path**: Choose one of the role-based roadmaps in [ROADMAP.md](file:///04-devops/ROADMAP.md) (e.g. *Beginner DevOps*, *Backend-Developer-Ops*, or *AWS-Focused*).
2. **Execute Practical Examples**: Every note includes exactly 5 verified practical examples with real command output. Use the provided commands in a local Linux/Docker sandbox.
3. **Inspect the Architecture Diagrams**: Conceptualize kernel data flows, container network namespaces, and cloud packet traversal through embedded ASCII/Mermaid diagrams.
4. **Test Knowledge with Collapsible Interview & Practice Sections**: Each note provides 8 technical interview Q&As and 6 hands-on exercises in collapsible `<details>` blocks.
