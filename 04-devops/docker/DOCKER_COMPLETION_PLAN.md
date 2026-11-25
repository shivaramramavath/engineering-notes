# Docker Track Curriculum Architecture & Completion Plan

## 1. Directory Structure & Rename Map

### 1.1 Directory Renames (Snake_case to NN-kebab-case)
```text
04-devops/docker/00_setup/              -> 04-devops/docker/00-setup/
04-devops/docker/01_docker_fundamentals -> 04-devops/docker/01-fundamentals/
04-devops/docker/02_docker_commands/    -> 04-devops/docker/02-commands/
04-devops/docker/03_dockerfile/         -> 04-devops/docker/03-dockerfile/
04-devops/docker/04_volumes_networking/ -> 04-devops/docker/04-volumes-and-networking/
04-devops/docker/05_docker_compose/     -> 04-devops/docker/05-compose/
04-devops/docker/06_production/         -> 04-devops/docker/06-production/
```

### 1.2 Note Rename Map (`git mv`)
```text
00_setup/getting-started.md                     -> 00-setup/01-getting-started.md
01_docker_fundamentals/01_docker-vs-vm.md       -> 01-fundamentals/01-docker-vs-vm.md
01_docker_fundamentals/02_images-containers.md  -> 01-fundamentals/02-images-and-containers.md
01_docker_fundamentals/03_docker-architecture.md-> 01-fundamentals/03-docker-architecture.md
02_docker_commands/01_run-ps-stop.md            -> 02-commands/01-run-ps-stop.md
02_docker_commands/02_logs-exec-inspect.md      -> 02-commands/02-logs-exec-inspect.md
02_docker_commands/03_images.md                 -> 02-commands/03-images.md
02_docker_commands/04_build.md                  -> 02-commands/04-build.md
02_docker_commands/05_cleanup.md                -> 02-commands/05-cleanup.md
03_dockerfile/01_dockerfile-basics.md           -> 03-dockerfile/01-dockerfile-basics.md
03_dockerfile/02_layers-caching.md              -> 03-dockerfile/03-layers-and-caching.md
03_dockerfile/03_multi-stage-builds.md          -> 03-dockerfile/04-multi-stage-builds.md
03_dockerfile/04_dockerignore.md                -> 03-dockerfile/05-dockerignore.md
04_volumes_networking/01_volumes-bind-mounts.md -> 04-volumes-and-networking/01-volumes-and-bind-mounts.md
04_volumes_networking/02_networking.md          -> 04-volumes-and-networking/04-networking.md
05_docker_compose/01_compose-basics.md          -> 05-compose/01-compose-basics.md
05_docker_compose/02_compose-multi-service.md   -> 05-compose/02-compose-multi-service.md
06_production/01_environments-secrets.md        -> 06-production/01-environments-and-secrets.md
06_production/02_image-optimization.md          -> 06-production/02-image-optimisation.md
06_production/03_docker-in-ci.md                -> 06-production/03-docker-in-ci.md
```

---

## 2. Complete Target Curriculum Specification (94 Notes across 10 Modules)

### Module 00: Setup & Installation (6 notes + README)
1. `01-getting-started.md` (Rewritten from existing)
2. `02-install-on-linux-macos-windows.md` (Engine vs Desktop, WSL 2 backend)
3. `03-rootless-and-post-install-steps.md` (dockerd-rootless, docker group security implications)
4. `04-docker-alternatives.md` (Podman, Colima, nerdctl, rootless differences)
5. `05-docker-context-and-remote-hosts.md` (Docker contexts, SSH remote daemon, DOCKER_HOST)
6. `06-first-container-walkthrough.md` (End-to-end hello-world, port mapping, interactive flags)

### Module 01: Core Fundamentals (9 notes + README)
1. `01-docker-vs-vm.md` (Rewritten: kernel virtualization vs hypervisor)
2. `02-images-and-containers.md` (Rewritten: OCI manifests, config JSON, layer tarballs)
3. `03-docker-architecture.md` (Rewritten: client, dockerd, containerd 2.0, runc, containerd-shim)
4. `04-oci-standards-and-runtimes.md` (OCI image-spec, runtime-spec, distribution-spec)
5. `05-how-isolation-works.md` (Linux PID/NET/MNT namespaces, cgroups v2, capabilities; link to linux)
6. `06-image-layers-and-union-filesystems.md` (OverlayFS lowerdir/upperdir/merged, copy-on-write)
7. `07-registries-and-image-naming.md` (Image references, tags, immutable SHA256 digests)
8. `08-container-lifecycle-and-states.md` (Created, Running, Paused, Restarting, Exited, Dead)
9. `09-pid-1-signals-and-init.md` (PID 1 zombie reaping, SIGTERM/SIGINT propagation, tini/dumb-init)

### Module 02: Essential Commands (10 notes + README)
1. `01-run-ps-stop.md` (Rewritten: flags, detached, interactive, tty)
2. `02-logs-exec-inspect.md` (Rewritten: log streaming, timestamps, exec patterns)
3. `03-images.md` (Rewritten: image management, history, tagging)
4. `04-build.md` (Rewritten: build context, build-args, target stages)
5. `05-cleanup.md` (Rewritten: prune filters, disk reclamation, safe volume management)
6. `06-resource-limits-cpu-memory.md` (cgroup limits, --memory, --cpus, oom-score-adj)
7. `07-restart-policies.md` (no, on-failure, always, unless-stopped)
8. `08-copying-files-and-exec-patterns.md` (docker cp, streaming tar, non-interactive exec)
9. `09-docker-events-stats-and-top.md` (Real-time telemetry, streaming events, process inspection)
10. `10-formatting-output-with-go-templates.md` (Formatting `--format` with Go templates and JSON)

### Module 03: Dockerfile Deep Dive (12 notes + README)
1. `01-dockerfile-basics.md` (Rewritten: syntax, comments, execution flow)
2. `02-instructions-reference.md` (Comprehensive reference for all 15 instructions)
3. `03-layers-and-caching.md` (Rewritten: cache invalidation rules, optimization)
4. `04-multi-stage-builds.md` (Rewritten: builder pattern, minimal final images)
5. `05-dockerignore.md` (Rewritten: syntax, secrets exclusion, build performance)
6. `06-buildkit-and-buildx.md` (Cache mounts, secret mounts, ssh mounts, Bake)
7. `07-multi-platform-images.md` (buildx, QEMU, native cross-compilation, manifest lists)
8. `08-base-image-choice.md` (Alpine vs Debian-slim vs Distroless vs Scratch vs Chainguard)
9. `09-healthchecks.md` (HEALTHCHECK instruction, intervals, start-period, retries)
10. `10-language-recipes.md` (Node.js, Python, Java Spring Boot, Go, .NET, Nginx static)
11. `11-dockerfile-linting-with-hadolint.md` (Static analysis, best practice enforcement)
12. `12-reproducible-builds-and-pinning.md` (Deterministic builds, SOURCE_DATE_EPOCH, digest pinning)

### Module 04: Storage & Networking (9 notes + README)
1. `01-volumes-and-bind-mounts.md` (Rewritten: named volumes vs host bind mounts)
2. `02-tmpfs-and-storage-drivers.md` (In-memory tmpfs mounts, overlay2 storage driver mechanics)
3. `03-backup-and-restore-of-volumes.md` (Tar volume snapshots, migration across hosts)
4. `04-networking.md` (Rewritten: bridge, host, none, overlay, macvlan)
5. `05-user-defined-networks-and-dns.md` (Custom bridge networks, embedded 127.0.0.11 DNS resolver)
6. `06-ports-publishing-and-firewalls.md` (Port binding, iptables integration, exposing vs publishing)
7. `07-container-to-host-and-container-to-container.md` (host.docker.internal, inter-container routing)
8. `08-ipv6-and-proxies.md` (Dual-stack IPv6 configuration, HTTP_PROXY setup)
9. `09-network-troubleshooting.md` (tcpdump, nslookup, iptables inspection, packet loss triage)

### Module 05: Docker Compose v2 (10 notes + README)
1. `01-compose-basics.md` (Rewritten: Compose Spec v2, compose.yaml)
2. `02-compose-multi-service.md` (Rewritten: multi-tier apps, networks, volumes)
3. `03-compose-file-reference.md` (Services, networks, volumes, configs, secrets)
4. `04-env-files-and-interpolation.md` (.env files, variable substitution, default values)
5. `05-depends-on-and-healthchecks.md` (Service order, condition: service_healthy)
6. `06-profiles-and-overrides.md` (Profiles, compose.override.yaml, include directive)
7. `07-compose-watch-and-dev-workflows.md` (Hot-reloading, sync, action: rebuild)
8. `08-compose-for-testing.md` (Ephemeral test runners, integration test orchestration)
9. `09-compose-in-production-limits.md` (When Compose works in prod vs when orchestrator required)
10. `10-migrating-from-compose-v1.md` (Deprecations, syntax changes, upgrade runbook)

### Module 06: Production Engineering & Security (14 notes + README)
1. `01-environments-and-secrets.md` (Rewritten: secrets mounting without baking into images)
2. `02-image-optimisation.md` (Rewritten: stripping binaries, distroless, layer squashing)
3. `03-docker-in-ci.md` (Rewritten: GitHub Actions buildx caching, matrix builds)
4. `04-image-security.md` (Non-root user, read-only rootfs, drop capabilities, seccomp)
5. `05-scanning-and-sbom.md` (Trivy, Docker Scout, Syft, Grype vulnerability auditing)
6. `06-image-signing-and-provenance.md` (Cosign, Sigstore, SLSA build provenance attestations)
7. `07-registries.md` (Docker Hub, GitHub Container Registry GHCR, AWS ECR, local Registry)
8. `08-tagging-and-release-strategies.md` (SemVer, commit SHA, immutable tags vs :latest antipattern)
9. `09-logging-drivers-and-monitoring.md` (json-file, local, journald, cAdvisor, Prometheus metrics)
10. `10-resource-management-and-oom.md` (cgroup accounting, memory limits, OOM killer defense)
11. `11-docker-daemon-configuration-and-troubleshooting.md` (daemon.json, live-restore, logs rotation)
12. `12-swarm-overview-and-status.md` (Swarm mode overview, services, overlay networks, current state)
13. `13-from-docker-to-kubernetes.md` (Container to Pod, Compose to Deployment, Service/Ingress)
14. `14-anti-patterns-and-common-mistakes.md` (Catalog of 15 production container anti-patterns)

### Module 07: Production Reference Projects (8 reference projects + README)
1. `01-node-api-with-postgres-and-redis.md` (Full Express/TypeScript stack with healthchecks)
2. `02-python-fastapi-stack.md` (Uvicorn, async Postgres, non-root user, Pydantic)
3. `03-spring-boot-with-database.md` (Layered JAR packaging, CDS, PostgreSQL)
4. `04-nginx-reverse-proxy-with-tls-local.md` (TLS termination, reverse proxy, upstream load balancing)
5. `05-multi-stage-minimal-go-image.md` (Static binary compiled into scratch image < 15MB)
6. `06-full-dev-environment-with-compose-watch.md` (Compose watch sync, database seeding, hot-reload)
7. `07-secure-hardened-image-pipeline.md` (Distroless, non-root, Trivy scan, Cosign signing)
8. `08-local-registry-and-mirroring.md` (Private distribution registry with basic auth and TLS)

### Module 08: Technical Interview Preparation (8 notes + README)
1. `01-docker-basics-qa.md` (Containers vs VMs, run flags, architecture fundamentals)
2. `02-images-and-dockerfile-qa.md` (Layer caching, multi-stage, BuildKit, entrypoint vs cmd)
3. `03-networking-and-volumes-qa.md` (Bridge networks, DNS, volume drivers, port publishing)
4. `04-compose-qa.md` (Compose Spec, service dependencies, profiles, env interpolation)
5. `05-security-qa.md` (Rootless, capabilities, seccomp, socket exposure, secrets)
6. `06-production-and-troubleshooting-scenarios.md` (Crashlooping, OOM kills, slow builds, 502 errors)
7. `07-docker-vs-kubernetes-and-vms.md` (Orchestration comparisons, trade-offs, when to graduate)
8. `08-behavioural-and-project-discussion.md` (STAR method container projects, migration stories)

### Module 09: Quick Reference Cheatsheets (8 notes + README)
1. `01-cli-commands.md` (High-frequency container, image, volume, and network commands)
2. `02-dockerfile-instructions.md` (Syntax reference for all Dockerfile instructions)
3. `03-compose-syntax.md` (Compose specification YAML schema quick lookup)
4. `04-networking-and-volumes.md` (Network drivers, volume flags, DNS resolution syntax)
5. `05-buildx-and-buildkit.md` (Buildx CLI, cache mounts, secret mounts, Bake syntax)
6. `06-security-checklist.md` (Production container hardening audit checklist)
7. `07-debugging-flowchart.md` (Visual troubleshooting decision tree for failing containers)
8. `08-common-errors-and-fixes.md` (Top 25 Docker error messages and solutions)
