# 04-DevOps Master Curriculum Index

This index categorizes all engineering notes across the five DevOps tracks: **Linux**, **Git & GitHub Actions**, **Docker**, **Nginx**, and **AWS**.

---

## 1. Linux Systems Engineering

### 1.1 Fundamentals
- [01-getting-started.md](file:///04-devops/linux/01-fundamentals/01-getting-started.md): Linux distributions, kernel vs userspace, shell prompt architecture.
- [02-filesystem-and-navigation.md](file:///04-devops/linux/01-fundamentals/02-filesystem-and-navigation.md): Filesystem Hierarchy Standard (FHS), inode structure, path traversal.
- [03-working-with-files.md](file:///04-devops/linux/01-fundamentals/03-working-with-files.md): File manipulation, file descriptors, stdout/stderr redirection, pipes.
- [04-users-and-groups.md](file:///04-devops/linux/01-fundamentals/04-users-and-groups.md): `/etc/passwd`, `/etc/shadow`, UID/GID allocation, sudoers privileges.
- [05-permissions.md](file:///04-devops/linux/01-fundamentals/05-permissions.md): Octal permissions, SUID, SGID, sticky bit, POSIX ACLs.

### 1.2 Shell & Text Processing
- [01-shell-basics.md](file:///04-devops/linux/02-shell/01-shell-basics.md): Bash environment variables, aliases, subshells, job control.
- [02-text-processing.md](file:///04-devops/linux/02-shell/02-text-processing.md): Stream manipulation with `grep`, `sed`, `awk`, `cut`, `sort`, `uniq`.
- [03-bash-scripting.md](file:///04-devops/linux/02-shell/03-bash-scripting.md): Strict mode (`set -euo pipefail`), conditionals, loops, functions, traps.
- [04-vim-and-tmux.md](file:///04-devops/linux/02-shell/04-vim-and-tmux.md): Terminal multiplexing with tmux, persistent sessions, modal editing with Vim.

### 1.3 System Administration
- [01-processes-and-signals.md](file:///04-devops/linux/03-system/01-processes-and-signals.md): Process table, state lifecycle, POSIX signals (SIGTERM, SIGKILL, SIGHUP), `/proc`.
- [02-package-management.md](file:///04-devops/linux/03-system/02-package-management.md): Debian `apt`/dpkg and RHEL `dnf`/rpm architecture and repository configuration.
- [03-systemd-and-services.md](file:///04-devops/linux/03-system/03-systemd-and-services.md): Unit files, target dependencies, service supervision, journalctl logging.
- [04-cron-and-timers.md](file:///04-devops/linux/03-system/04-cron-and-timers.md): Crontab scheduling vs modern systemd monotonic and calendar timers.
- [05-storage-and-filesystems.md](file:///04-devops/linux/03-system/05-storage-and-filesystems.md): Block devices, partitioning, ext4, XFS, mount options, LVM volume groups.

### 1.4 Networking & Security
- [01-networking-basics.md](file:///04-devops/linux/04-networking/01-networking-basics.md): Socket inspection (`ss`, `ip`), routing tables, DNS resolution (`/etc/resolv.conf`).
- [02-ssh.md](file:///04-devops/linux/04-networking/02-ssh.md): OpenSSH hardening, ed25519 key generation, ssh-agent, jump hosts, tunneling.
- [03-firewall.md](file:///04-devops/linux/04-networking/03-firewall.md): Linux packet filtering via Netfilter, `nftables`, UFW, and firewalld.
- [01-server-hardening.md](file:///04-devops/linux/05-production/01-server-hardening.md): CIS benchmark compliance, fail2ban, disabling root SSH, AppArmor / SELinux.
- [02-troubleshooting.md](file:///04-devops/linux/05-production/02-troubleshooting.md): Systematic outage diagnostics, dmesg kernel rings, network connection debugging.
- [03-performance.md](file:///04-devops/linux/05-production/03-performance.md): CPU/memory profiling (`top`, `vmstat`, `iostat`, `sar`), load average analysis.
- [01-namespaces-and-cgroups.md](file:///04-devops/linux/06-advanced/01-namespaces-and-cgroups.md): Linux container primitives: PID, NET, MNT namespaces and cgroup v2 controllers.
- [02-advanced-debugging.md](file:///04-devops/linux/06-advanced/02-advanced-debugging.md): Syscall interception with `strace`, library tracing with `ltrace`, eBPF overview.

---

## 2. Git & GitHub Actions

### 2.1 Git Core Architecture
- [01-getting-started.md](file:///04-devops/git-github/00_setup/getting-started.md): Git installation, global config, credential helpers, default branch `main`.
- [01-git-vs-github.md](file:///04-devops/git-github/01_git_fundamentals/01_git-vs-github.md): Distributed version control vs centralized forge collaboration.
- [02-repositories.md](file:///04-devops/git-github/01_git_fundamentals/02_repositories.md): Local `.git` folder structure, bare repositories, remote mirrors.
- [03-working-tree-staging-area.md](file:///04-devops/git-github/01_git_fundamentals/03_working-tree-staging-area.md): The three states: Working Directory, Staging Area (Index), and Git Directory.
- [04-commits.md](file:///04-devops/git-github/01_git_fundamentals/04_commits.md): Anatomy of a cryptographic commit object, author vs committer, PGP signing.
- [05-git-objects.md](file:///04-devops/git-github/01_git_fundamentals/05_git-objects.md): Git object database: Blobs, Trees, Commits, and Annotated Tags.

### 2.2 Git Commands & History Rewriting
- [01-init-clone.md](file:///04-devops/git-github/02_git_commands/01_init-clone.md): Repository initialization, shallow clones (`--depth`), sparse checkout.
- [02-status-diff-log.md](file:///04-devops/git-github/02_git_commands/02_status-diff-log.md): Status flags, diff patch inspection, custom log graphing.
- [03-add-commit.md](file:///04-devops/git-github/02_git_commands/03_add-commit.md): Patch staging (`git add -p`), conventional commits, message structure.
- [04-undoing-changes.md](file:///04-devops/git-github/02_git_commands/04_undoing-changes.md): `git restore`, `git reset` (soft, mixed, hard), `git revert`.
- [05-clean-rm-mv.md](file:///04-devops/git-github/02_git_commands/05_clean-rm-mv.md): Unversioned file cleaning, untracking files, moving files cleanly.
- [06-branching.md](file:///04-devops/git-github/02_git_commands/06_branching.md): Branch pointers, `git switch`, branch topology, deletion safety.
- [07-merging.md](file:///04-devops/git-github/02_git_commands/07_merging.md): Fast-forward vs 3-way recursive merge, conflict resolution, merge commits.
- [08-remotes.md](file:///04-devops/git-github/02_git_commands/08_remotes.md): Fetching, pulling, pushing, remote tracking references.
- [09-tracking-upstream.md](file:///04-devops/git-github/02_git_commands/09_tracking-upstream.md): Setting upstream tracking, syncing open-source forks.
- [01-amend.md](file:///04-devops/git-github/03_history_rewriting/01_amend.md): Amending previous commit message and staged payload.
- [02-rebase.md](file:///04-devops/git-github/03_history_rewriting/02_rebase.md): Interactive rebase (`git rebase -i`), squashing, fixing up, reordering commits.
- [03-cherry-pick.md](file:///04-devops/git-github/03_history_rewriting/03_cherry-pick.md): Applying targeted commits across divergent branches.
- [04-reflog.md](file:///04-devops/git-github/03_history_rewriting/04_reflog.md): Disaster recovery using `git reflog` to resurrect detached/lost commits.

### 2.3 GitHub Actions CI/CD
- [01-core-concepts.md](file:///04-devops/git-github/04_github_actions/01_core-concepts.md): Workflows, jobs, steps, runners, event triggers.
- [02-workflow-syntax.md](file:///04-devops/git-github/04_github_actions/02_workflow-syntax.md): YAML schema, name, env, defaults, and concurrency keys.
- [03-events-and-triggers.md](file:///04-devops/git-github/04_github_actions/03_events-and-triggers.md): Push, pull_request, workflow_dispatch, schedule (cron), filters.
- [04-jobs-and-steps.md](file:///04-devops/git-github/04_github_actions/04_jobs-and-steps.md): Job execution order, `needs` DAG dependencies, outputs.
- [05-actions-and-marketplace.md](file:///04-devops/git-github/04_github_actions/05_actions-and-marketplace.md): Verified actions, SHA pinning for security (`@v4` vs commit SHA).
- [06-runners.md](file:///04-devops/git-github/04_github_actions/06_runners.md): GitHub-hosted runners vs self-hosted runner infrastructure.
- [07-variables-and-secrets.md](file:///04-devops/git-github/04_github_actions/07_variables-and-secrets.md): Encrypted repository secrets, environment variables, masked logging.
- [08-contexts-expressions-conditions.md](file:///04-devops/git-github/04_github_actions/08_contexts-expressions-conditions.md): `github`, `env`, `secrets` contexts; expression interpolation.
- [09-artifacts-caching-dependencies.md](file:///04-devops/git-github/04_github_actions/09_artifacts-caching-dependencies.md): `actions/cache` and artifact passing across workflow jobs.
- [10-matrix-and-concurrency.md](file:///04-devops/git-github/04_github_actions/10_matrix-and-concurrency.md): Multi-OS, multi-version test matrices; build deduplication.
- [11-permissions-and-environments.md](file:///04-devops/git-github/04_github_actions/11_permissions-and-environments.md): Principle of least privilege `permissions` and deployment protection rules.
- [12-reusable-workflows.md](file:///04-devops/git-github/04_github_actions/12_reusable-workflows.md): Modular `workflow_call` pipelines and enterprise templates.
- [13-custom-actions.md](file:///04-devops/git-github/04_github_actions/13_custom-actions.md): Composite actions vs Docker container actions.
- [14-ci-pipelines.md](file:///04-devops/git-github/04_github_actions/14_ci-pipelines.md): End-to-end linting, testing, and security scanning pipeline.
- [15-cd-and-deployment.md](file:///04-devops/git-github/04_github_actions/15_cd-and-deployment.md): Automated staging/production release deployments.
- [16-docker-ci-cd.md](file:///04-devops/git-github/04_github_actions/16_docker-ci-cd.md): Multi-platform image building and publishing to Docker Hub / GitHub GHCR.
- [17-security-hardening.md](file:///04-devops/git-github/04_github_actions/17_security-hardening.md): OIDC cloud credential exchange, secret leakage prevention, dependency audits.
- [18-debugging-and-workflow-commands.md](file:///04-devops/git-github/04_github_actions/18_debugging-and-workflow-commands.md): Action debugging flags, runner SSH debugging, workflow command tokens.

---

## 3. Docker Containers

### 3.1 Fundamentals & CLI
- [01-getting-started.md](file:///04-devops/docker/00_setup/getting-started.md): Docker Engine installation, verification, post-installation rootless steps.
- [01-docker-vs-vm.md](file:///04-devops/docker/01_docker_fundamentals/01_docker-vs-vm.md): Hypervisor VMs vs kernel-level container virtualization.
- [02-images-containers.md](file:///04-devops/docker/01_docker_fundamentals/02_images-containers.md): Read-only OCI image layers vs read-write container overlay layer.
- [03-docker-architecture.md](file:///04-devops/docker/01_docker_fundamentals/03_docker-architecture.md): Client-Server architecture: Docker CLI, dockerd, containerd, and runc.
- [01-run-ps-stop.md](file:///04-devops/docker/02_docker_commands/01_run-ps-stop.md): Container execution flags, port publishing, process listing, graceful stopping.
- [02-logs-exec-inspect.md](file:///04-devops/docker/02_docker_commands/02_logs-exec-inspect.md): Streaming stdout/stderr logs, interactive shells, low-level JSON inspection.
- [03-images.md](file:///04-devops/docker/02_docker_commands/03_images.md): Image tagging, repository pulling, pushing, history inspection.
- [04-build.md](file:///04-devops/docker/02_docker_commands/04_build.md): Buildx context, cache mounts, build arguments (`--build-arg`).
- [05-cleanup.md](file:///04-devops/docker/02_docker_commands/05_cleanup.md): Pruning dangling layers, stopped containers, unused volumes, and builder cache.

### 3.2 Dockerfiles & Networking
- [01-dockerfile-basics.md](file:///04-devops/docker/03_dockerfile/01_dockerfile-basics.md): Core instructions: FROM, RUN, CMD, ENTRYPOINT, COPY, ENV, WORKDIR.
- [02-layers-caching.md](file:///04-devops/docker/03_dockerfile/02_layers-caching.md): BuildKit cache invalidation rules, ordering dependencies before application code.
- [03-multi-stage-builds.md](file:///04-devops/docker/03_dockerfile/03_multi-stage-builds.md): Minimizing production attack surface with multi-stage compilation.
- [04-dockerignore.md](file:///04-devops/docker/03_dockerfile/04_dockerignore.md): Excluding `.git`, node_modules, and secrets from build contexts.
- [01-volumes-bind-mounts.md](file:///04-devops/docker/04_volumes_networking/01_volumes-bind-mounts.md): Named volumes, host bind mounts, tmpfs in-memory mounts.
- [02-networking.md](file:///04-devops/docker/04_volumes_networking/02_networking.md): Bridge networks, embedded DNS resolution, host networking, macvlan.

### 3.3 Compose & Production
- [01-compose-basics.md](file:///04-devops/docker/05_docker_compose/01_compose-basics.md): `compose.yaml` specification, service dependencies, port mappings.
- [02-compose-multi-service.md](file:///04-devops/docker/05_docker_compose/02_compose-multi-service.md): Multi-tier application orchestration (Frontend + API + DB + Redis).
- [01-environments-secrets.md](file:///04-devops/docker/06_production/01_environments-secrets.md): Managing `.env` variables and Docker Secrets securely.
- [02-image-optimization.md](file:///04-devops/docker/06_production/02_image-optimization.md): Alpine vs Distroless, stripping debug symbols, single-layer artifacts.
- [03-docker-in-ci.md](file:///04-devops/docker/06_production/03_docker-in-ci.md): Running Docker inside CI pipelines safely with Docker-in-Docker or socket mounting.

---

## 4. Nginx Web Gateway

### 4.1 Fundamentals & Core Features
- [01-config-structure.md](file:///04-devops/nginx/01-fundamentals/config-structure.md): `nginx.conf` context hierarchy (`main`, `events`, `http`, `server`, `location`).
- [02-installation.md](file:///04-devops/nginx/01-fundamentals/installation.md): Package installation vs compiling with custom modules, directory layouts.
- [03-server-and-location.md](file:///04-devops/nginx/01-fundamentals/server-and-location.md): Virtual hosts, regex location matching precedence rules.
- [04-static-files.md](file:///04-devops/nginx/01-fundamentals/static-files.md): High-performance static delivery: `sendfile`, `tcp_nopush`, `tcp_nodelay`, gzip.
- [01-logging.md](file:///04-devops/nginx/02-core-features/logging.md): Access log formats, JSON structured logging for ELK/Loki, error log levels.
- [02-reverse-proxy.md](file:///04-devops/nginx/02-core-features/reverse-proxy.md): Reverse proxying (`proxy_pass`), header propagation, connection buffering.
- [03-tls-https.md](file:///04-devops/nginx/02-core-features/tls-https.md): TLS 1.3 configuration, Let's Encrypt / Certbot automation, cipher hardening.
- [04-variables-and-rewrites.md](file:///04-devops/nginx/02-core-features/variables-and-rewrites.md): Built-in Nginx variables, URL rewriting, 301/302 redirects.

### 4.2 Traffic Management & Production
- [01-caching.md](file:///04-devops/nginx/03-traffic-management/caching.md): Reverse proxy caching (`proxy_cache`), cache keys, cache purging, bypass rules.
- [02-load-balancing.md](file:///04-devops/nginx/03-traffic-management/load-balancing.md): Upstream pools, Round Robin, Least Connections, IP Hash, active health checks.
- [03-rate-limiting.md](file:///04-devops/nginx/03-traffic-management/rate-limiting.md): Leaky bucket algorithm, `limit_req_zone`, burst handling, connection limiting.
- [01-architecture.md](file:///04-devops/nginx/04-internals/architecture.md): Master-Worker asynchronous non-blocking event-driven epoll architecture.
- [02-request-lifecycle.md](file:///04-devops/nginx/04-internals/request-lifecycle.md): 11 internal processing phases from POST_READ to LOG.
- [01-performance-tuning.md](file:///04-devops/nginx/05-security-performance/performance-tuning.md): Worker affinity, file descriptor limits (`worker_rlimit_nofile`), buffer sizes.
- [02-security-hardening.md](file:///04-devops/nginx/05-security-performance/security-hardening.md): Security headers (HSTS, CSP), blocking bad user agents, mitigating Slowloris.
- [01-docker-kubernetes.md](file:///04-devops/nginx/06-production/docker-kubernetes.md): Running Nginx in Docker containers and Kubernetes Ingress controllers.
- [02-operations.md](file:///04-devops/nginx/06-production/operations.md): Zero-downtime binary upgrades and configuration reloading (`kill -HUP`).
- [03-troubleshooting.md](file:///04-devops/nginx/06-production/troubleshooting.md): Diagnosing 502 Bad Gateway, 504 Gateway Timeout, and 413 Payload Too Large.

---

## 5. Amazon Web Services (AWS)

### 5.1 Cloud Foundations & Compute
- [01-what-is-aws.md](file:///04-devops/aws/01-foundations/01-what-is-aws.md): Cloud computing models, AWS shared responsibility model, global infrastructure.
- [02-accounts-regions-and-billing.md](file:///04-devops/aws/01-foundations/02-accounts-regions-and-billing.md): AWS Organizations, multi-account strategy, Regions, Availability Zones.
- [03-iam.md](file:///04-devops/aws/01-foundations/03-iam.md): IAM users, groups, roles, policy evaluation logic, least privilege enforcement.
- [04-aws-cli.md](file:///04-devops/aws/01-foundations/04-aws-cli.md): AWS CLI v2 configuration, credentials profiles, JMESPath querying.
- [01-ec2.md](file:///04-devops/aws/02-compute/01-ec2.md): Elastic Compute Cloud: instance families, Graviton, EBS storage, security groups.
- [02-lambda.md](file:///04-devops/aws/02-compute/02-lambda.md): Serverless functions: execution environments, concurrency, cold starts, event sources.
- [03-ecs-fargate.md](file:///04-devops/aws/02-compute/03-ecs-fargate.md): Elastic Container Service: task definitions, services, Fargate serverless compute.

### 5.2 Storage, Databases & Networking
- [01-s3.md](file:///04-devops/aws/03-storage-and-databases/01-s3.md): Simple Storage Service: buckets, storage classes, lifecycle policies, encryption.
- [02-dynamodb.md](file:///04-devops/aws/03-storage-and-databases/02-dynamodb.md): Managed NoSQL: partition keys, sort keys, secondary indexes (GSI/LSI), TTL.
- [03-rds-and-aurora.md](file:///04-devops/aws/03-storage-and-databases/03-rds-and-aurora.md): Relational Database Service & Amazon Aurora architecture, multi-AZ failover.
- [01-vpc.md](file:///04-devops/aws/04-networking/01-vpc.md): Virtual Private Cloud: CIDR planning, public/private subnets, NAT Gateways, route tables.
- [02-route53-and-cloudfront.md](file:///04-devops/aws/04-networking/02-route53-and-cloudfront.md): Route 53 DNS routing policies and CloudFront CDN global edge distribution.
- [03-load-balancing-and-api-gateway.md](file:///04-devops/aws/04-networking/03-load-balancing-and-api-gateway.md): Application Load Balancer (ALB) vs Network Load Balancer (NLB), API Gateway.

### 5.3 App Services & Cloud Operations
- [01-sqs.md](file:///04-devops/aws/05-app-services/01-sqs.md): Simple Queue Service: standard vs FIFO queues, dead-letter queues (DLQ), visibility timeout.
- [02-sns-and-eventbridge.md](file:///04-devops/aws/05-app-services/02-sns-and-eventbridge.md): Pub/Sub messaging with SNS and event-driven architectures with EventBridge.
- [03-ses-email.md](file:///04-devops/aws/05-app-services/03-ses-email.md): Simple Email Service: DKIM, SPF verification, transactional email delivery.
- [04-cognito.md](file:///04-devops/aws/05-app-services/04-cognito.md): User Pools, Identity Pools, JWT token verification, social federation.
- [05-bedrock.md](file:///04-devops/aws/05-app-services/05-bedrock.md): Serverless generative AI: foundation model APIs, RAG knowledge bases, guardrails.
- [01-infrastructure-as-code.md](file:///04-devops/aws/06-operations/01-infrastructure-as-code.md): Declarative cloud infrastructure using Terraform / OpenTofu and AWS CloudFormation / CDK.
- [02-ci-cd-to-aws.md](file:///04-devops/aws/06-operations/02-ci-cd-to-aws.md): Deploying containers to ECS/EKS and serverless apps via GitHub Actions with OIDC.
- [03-security-and-secrets.md](file:///04-devops/aws/06-operations/03-security-and-secrets.md): AWS Secrets Manager, Systems Manager Parameter Store, AWS KMS encryption keys.
- [04-observability.md](file:///04-devops/aws/06-operations/04-observability.md): CloudWatch logs, metric alarms, X-Ray distributed tracing, Container Insights.
- [05-reliability-and-cost.md](file:///04-devops/aws/06-operations/05-reliability-and-cost.md): AWS Well-Architected Framework, cost optimization, AWS Budgets, Cost Explorer.

---

## 6. Interview Preparation & Quick Reference

### 6.1 Interview Questions & Answers
- [Linux Technical Interview Q&A](file:///04-devops/linux/reference/interview.md)
- [AWS Cloud Technical Interview Q&A](file:///04-devops/aws/reference/interview.md)
- [Nginx Gateway Technical Interview Q&A](file:///04-devops/nginx/08-reference/interview-questions.md)

### 6.2 Quick Reference Cheatsheets
- [Linux Commands & Sysadmin Cheatsheet](file:///04-devops/linux/reference/cheatsheet.md)
- [AWS CLI & Services Cheatsheet](file:///04-devops/aws/reference/cheatsheet.md)
- [GitHub Actions Workflow Cheatsheet](file:///04-devops/git-github/04_github_actions/CHEATSHEET.md)
- [Nginx Directives Cheatsheet](file:///04-devops/nginx/08-reference/cheatsheet.md)
