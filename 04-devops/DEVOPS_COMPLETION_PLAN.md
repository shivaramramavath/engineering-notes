# DevOps Curriculum Architecture & Completion Plan

| Metadata | Details |
|---|---|
| **Area** | `04-devops/` Curriculum Architecture |
| **Status** | Active Implementation Plan |
| **Tracks Covered** | `aws/`, `docker/`, `git-github/`, `linux/`, `nginx/` |
| **Target Standards** | Universal `NN-kebab-case`, strict markdown, 100% template conformance |

---

## 1. Master Rename & Standardization Map

This map defines the precise file and folder migrations for all 5 DevOps tracks to achieve complete architectural consistency with the repository's Java and Spring Boot tracks.

### 1.1 Track: `docker/`
| Current Path | Target Path | Transformation Reason |
|---|---|---|
| `docker/00_setup/` | `docker/00-setup/` | Snake to kebab folder convention |
| `docker/00_setup/getting-started.md` | `docker/00-setup/01-getting-started.md` | Add standard numeric prefix |
| `docker/01_docker_fundamentals/` | `docker/01-fundamentals/` | Concise kebab naming |
| `docker/01_docker_fundamentals/01_docker-vs-vm.md` | `docker/01-fundamentals/01-docker-vs-vm.md` | Snake prefix to kebab |
| `docker/01_docker_fundamentals/02_images-containers.md` | `docker/01-fundamentals/02-images-containers.md` | Snake prefix to kebab |
| `docker/01_docker_fundamentals/03_docker-architecture.md` | `docker/01-fundamentals/03-docker-architecture.md` | Snake prefix to kebab |
| `docker/02_docker_commands/` | `docker/02-commands/` | Concise kebab naming |
| `docker/02_docker_commands/01_run-ps-stop.md` | `docker/02-commands/01-run-ps-stop.md` | Snake prefix to kebab |
| `docker/02_docker_commands/02_logs-exec-inspect.md` | `docker/02-commands/02-logs-exec-inspect.md` | Snake prefix to kebab |
| `docker/02_docker_commands/03_images.md` | `docker/02-commands/03-images.md` | Snake prefix to kebab |
| `docker/02_docker_commands/04_build.md` | `docker/02-commands/04-build.md` | Snake prefix to kebab |
| `docker/02_docker_commands/05_cleanup.md` | `docker/02-commands/05-cleanup.md` | Snake prefix to kebab |
| `docker/03_dockerfile/` | `docker/03-dockerfile/` | Kebab convention |
| `docker/03_dockerfile/01_dockerfile-basics.md` | `docker/03-dockerfile/01-dockerfile-basics.md` | Snake prefix to kebab |
| `docker/03_dockerfile/02_layers-caching.md` | `docker/03-dockerfile/02-layers-caching.md` | Snake prefix to kebab |
| `docker/03_dockerfile/03_multi-stage-builds.md` | `docker/03-dockerfile/03-multi-stage-builds.md` | Snake prefix to kebab |
| `docker/03_dockerfile/04_dockerignore.md` | `docker/03-dockerfile/04-dockerignore.md` | Snake prefix to kebab |
| `docker/04_volumes_networking/` | `docker/04-storage-and-networking/` | Kebab convention |
| `docker/04_volumes_networking/01_volumes-bind-mounts.md` | `docker/04-storage-and-networking/01-volumes-and-bind-mounts.md` | Snake prefix to kebab |
| `docker/04_volumes_networking/02_networking.md` | `docker/04-storage-and-networking/02-networking-and-dns.md` | Snake prefix to kebab |
| `docker/05_docker_compose/` | `docker/05-compose/` | Kebab convention |
| `docker/05_docker_compose/01_compose-basics.md` | `docker/05-compose/01-compose-basics.md` | Snake prefix to kebab |
| `docker/05_docker_compose/02_compose-multi-service.md` | `docker/05-compose/02-compose-multi-service.md` | Snake prefix to kebab |
| `docker/06_production/` | `docker/06-production/` | Kebab convention |
| `docker/06_production/01_environments-secrets.md` | `docker/06-production/01-environments-and-secrets.md` | Snake prefix to kebab |
| `docker/06_production/02_image-optimization.md` | `docker/06-production/02-image-optimization.md` | Snake prefix to kebab |
| `docker/06_production/03_docker-in-ci.md` | `docker/06-production/03-docker-in-ci.md` | Snake prefix to kebab |
| *New Module* | `docker/07-projects/` | Reference multi-service containerized stacks |
| *New Module* | `docker/08-interview/` | Comprehensive Docker container interview Q&A |
| *New Module* | `docker/09-cheatsheets/` | CLI syntax & Compose quick reference |

---

### 1.2 Track: `git-github/`
| Current Path | Target Path | Transformation Reason |
|---|---|---|
| `git-github/00_setup/` | `git-github/00-setup/` | Snake to kebab folder convention |
| `git-github/00_setup/getting-started.md` | `git-github/00-setup/01-getting-started.md` | Add standard numeric prefix |
| `git-github/01_git_fundamentals/` | `git-github/01-fundamentals/` | Concise kebab naming |
| `git-github/01_git_fundamentals/01_git-vs-github.md` | `git-github/01-fundamentals/01-git-vs-github.md` | Snake prefix to kebab |
| `git-github/01_git_fundamentals/02_repositories.md` | `git-github/01-fundamentals/02-repositories.md` | Snake prefix to kebab |
| `git-github/01_git_fundamentals/03_working-tree-staging-area.md` | `git-github/01-fundamentals/03-working-tree-staging-area.md` | Snake prefix to kebab |
| `git-github/01_git_fundamentals/04_commits.md` | `git-github/01-fundamentals/04-commits.md` | Snake prefix to kebab |
| `git-github/01_git_fundamentals/05_git-objects.md` | `git-github/01-fundamentals/05-git-objects.md` | Snake prefix to kebab |
| `git-github/02_git_commands/` | `git-github/02-commands/` | Concise kebab naming |
| `git-github/02_git_commands/01_init-clone.md` to `09_tracking-upstream.md` | `git-github/02-commands/01-init-clone.md` to `09-tracking-upstream.md` | Snake prefix to kebab |
| `git-github/03_history_rewriting/` | `git-github/03-history-rewriting/` | Kebab folder convention |
| `git-github/03_history_rewriting/01_amend.md` to `04_reflog.md` | `git-github/03-history-rewriting/01-amend.md` to `04-reflog.md` | Snake prefix to kebab |
| `git-github/04_github_actions/` | `git-github/04-github-actions/` | Kebab folder convention |
| `git-github/04_github_actions/01_core-concepts.md` to `18_debugging-and-workflow-commands.md` | `git-github/04-github-actions/01-core-concepts.md` to `18-debugging-and-workflow-commands.md` | Snake prefix to kebab |
| `git-github/04_github_actions/examples/*.yml` | Embedded into `04-github-actions/` notes | Absorb YAML into markdown; delete raw files |
| `git-github/04_github_actions/CHEATSHEET.md` | `git-github/06-cheatsheets/01-git-and-actions-cheatsheet.md` | Move and rename uppercase file |
| *New Module* | `git-github/05-projects/` | Production CI/CD pipelines & release automation |
| *New Module* | `git-github/07-interview/` | Git internals & GitHub Actions interview Q&A |

---

### 1.3 Track: `nginx/`
| Current Path | Target Path | Transformation Reason |
|---|---|---|
| `nginx/01-fundamentals/config-structure.md` | `nginx/01-fundamentals/01-config-structure.md` | Add numeric order prefix |
| `nginx/01-fundamentals/installation.md` | `nginx/01-fundamentals/02-installation.md` | Add numeric order prefix |
| `nginx/01-fundamentals/server-and-location.md` | `nginx/01-fundamentals/03-server-and-location.md` | Add numeric order prefix |
| `nginx/01-fundamentals/static-files.md` | `nginx/01-fundamentals/04-static-files.md` | Add numeric order prefix |
| `nginx/02-core-features/logging.md` | `nginx/02-core-features/01-logging.md` | Add numeric order prefix |
| `nginx/02-core-features/reverse-proxy.md` | `nginx/02-core-features/02-reverse-proxy.md` | Add numeric order prefix |
| `nginx/02-core-features/tls-https.md` | `nginx/02-core-features/03-tls-https.md` | Add numeric order prefix |
| `nginx/02-core-features/variables-and-rewrites.md` | `nginx/02-core-features/04-variables-and-rewrites.md` | Add numeric order prefix |
| `nginx/03-traffic-management/caching.md` | `nginx/03-traffic-management/01-caching.md` | Add numeric order prefix |
| `nginx/03-traffic-management/load-balancing.md` | `nginx/03-traffic-management/02-load-balancing.md` | Add numeric order prefix |
| `nginx/03-traffic-management/rate-limiting.md` | `nginx/03-traffic-management/03-rate-limiting.md` | Add numeric order prefix |
| `nginx/04-internals/architecture.md` | `nginx/04-internals/01-architecture.md` | Add numeric order prefix |
| `nginx/04-internals/request-lifecycle.md` | `nginx/04-internals/02-request-lifecycle.md` | Add numeric order prefix |
| `nginx/05-security-performance/performance-tuning.md` | `nginx/05-security-performance/01-performance-tuning.md` | Add numeric order prefix |
| `nginx/05-security-performance/security-hardening.md` | `nginx/05-security-performance/02-security-hardening.md` | Add numeric order prefix |
| `nginx/06-production/docker-kubernetes.md` | `nginx/06-production/01-docker-kubernetes.md` | Add numeric order prefix |
| `nginx/06-production/operations.md` | `nginx/06-production/02-operations.md` | Add numeric order prefix |
| `nginx/06-production/troubleshooting.md` | `nginx/06-production/03-troubleshooting.md` | Add numeric order prefix |
| `nginx/07-projects/secure-gateway-stack.md` | `nginx/07-projects/01-secure-gateway-stack.md` | Add numeric order prefix |
| `nginx/07-projects/spa-with-api-proxy.md` | `nginx/07-projects/02-spa-with-api-proxy.md` | Add numeric order prefix |
| `nginx/08-reference/cheatsheet.md` | `nginx/09-cheatsheets/01-nginx-cheatsheet.md` | Separate cheatsheet into dedicated module |
| `nginx/08-reference/interview-questions.md` | `nginx/08-interview/01-interview-questions.md` | Standalone interview module |

---

### 1.4 Track: `aws/`
| Current Path | Target Path | Transformation Reason |
|---|---|---|
| `aws/reference/cheatsheet.md` | `aws/07-cheatsheets/01-aws-cheatsheet.md` | Numbered module convention |
| `aws/reference/interview.md` | `aws/08-interview/01-aws-interview-qa.md` | Numbered module convention |
| *New Module* | `aws/09-projects/` | Multi-tier cloud architecture projects |
| *New Module READMEs* | `aws/NN-module/README.md` | Provide mandatory per-folder READMEs |

---

### 1.5 Track: `linux/`
| Current Path | Target Path | Transformation Reason |
|---|---|---|
| `linux/reference/cheatsheet.md` | `linux/07-cheatsheets/01-linux-cheatsheet.md` | Numbered module convention |
| `linux/reference/interview.md` | `linux/08-interview/01-linux-interview-qa.md` | Numbered module convention |
| *New Module* | `linux/09-projects/` | Systems automation & hardening projects |
| *New Module READMEs* | `linux/NN-module/README.md` | Provide mandatory per-folder READMEs |

---

## 2. Advanced / Extra Expert Topics (One-Liners)

- **Linux eBPF Observability & Tracing**: In-kernel bytecode execution for low-overhead network and syscall observability without kernel module recompilation.
- **Rootless Container Execution & User Namespaces**: Running Docker and Podman daemonless as unprivileged UID to eliminate host privilege escalation risk.
- **GitHub Actions OIDC AWS Role Assumption**: Short-lived cryptographic JWT authentication to AWS STS eliminating long-lived IAM access keys.
- **Nginx Native HTTP/3 (QUIC) Ingress**: Zero-RTT handshake connection establishment over UDP with TLS 1.3 encryption.
- **AWS Graviton4 ARM Architecture Migration**: Multi-architecture Docker image builds targeting Graviton compute nodes for 40% cost-efficiency.
- **Multi-Cloud Terraform & OpenTofu State Locking**: Remote backend state management with DynamoDB locking and KMS encryption.
- **Kubernetes Pod Lifecycle & Graceful PreStop Draining**: Eliminating HTTP 502 connection drops during container rolling deployments.
