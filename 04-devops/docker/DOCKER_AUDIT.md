# Docker Track Baseline Audit & Deficiency Report

## 1. Executive Summary

A baseline audit of `04-devops/docker/` conducted on 2026-11-25 identified structural, stylistic, and technical deficiencies across the existing 20 notes and 7 module README files. The current track contains introductory, conversational notes averaging 60-120 lines that lack production depth, fail to implement the 13-section enterprise concept template, utilize non-standard `snake_case` folder and file naming, and omit critical modern container standards (BuildKit, Compose v2 specification, rootless mode, SBOM scanning, multi-platform images, and security hardening).

---

## 2. Inventory & Compliance Audit of Existing Notes

| Existing File Path | Lines | Template Status | Technical Deficiencies & Gaps | Target Resolution |
|---|:---:|---|---|---|
| `00_setup/getting-started.md` | 107 | Incomplete | Unnumbered filename; conversational; lacks 13 sections; mentions old Docker v27 | Rename to `01-getting-started.md`, rewrite to 13 sections |
| `00_setup/README.md` | 17 | Stub | 17-line directory list; lacks overview roadmap and standards | Rewrite to comprehensive module overview |
| `01_docker_fundamentals/01_docker-vs-vm.md` | 84 | Incomplete | Snake_case prefix; lacks kernel mechanics (cgroups/namespaces); missing practical exercises | Rename to `01-docker-vs-vm.md`, rewrite with full kernel diagrams |
| `01_docker_fundamentals/02_images-containers.md` | 107 | Incomplete | Snake_case prefix; lacks OCI image manifest details, layer tarballs, and immutability math | Rename to `02-images-and-containers.md`, rewrite to 13 sections |
| `01_docker_fundamentals/03_docker-architecture.md` | 114 | Incomplete | Outdated monolithic daemon model; omits containerd 2.0, runc, and shim mechanics | Rename to `03-docker-architecture.md`, rewrite with containerd/runc flows |
| `01_docker_fundamentals/README.md` | 17 | Stub | Minimal stub list | Rewrite to comprehensive module overview |
| `02_docker_commands/01_run-ps-stop.md` | 117 | Incomplete | Covers basic flags; lacks resource constraints, detachment, signals, and formatting | Rename to `01-run-ps-stop.md`, expand to 13 sections |
| `02_docker_commands/02_logs-exec-inspect.md` | 118 | Incomplete | Shallow coverage of log drivers; lacks Go templates for `docker inspect` | Rename to `02-logs-exec-inspect.md`, expand to 13 sections |
| `02_docker_commands/03_images.md` | 100 | Incomplete | Basic tagging only; omits digest inspection, multi-platform manifests, and layer inspection | Rename to `03-images.md`, expand to 13 sections |
| `02_docker_commands/04_build.md` | 100 | Incomplete | References legacy builder; misses BuildKit syntax, secrets mounting, and cache types | Rename to `04-build.md`, expand to 13 sections |
| `02_docker_commands/05_cleanup.md` | 106 | Incomplete | Covers basic `prune`; misses dangling filter nuances, volumes safety, and automated cleanup | Rename to `05-cleanup.md`, expand to 13 sections |
| `02_docker_commands/README.md` | 17 | Stub | Minimal stub list | Rewrite to comprehensive module overview |
| `03_dockerfile/01_dockerfile-basics.md` | 118 | Incomplete | Covers basic syntax; lacks instruction execution lifecycle and user isolation | Rename to `01-dockerfile-basics.md`, expand to 13 sections |
| `03_dockerfile/02_layers-caching.md` | 110 | Incomplete | Explains basic layer order; lacks BuildKit inline/registry cache and cache mount mechanics | Rename to `03-layers-and-caching.md`, expand to 13 sections |
| `03_dockerfile/03_multi-stage-builds.md` | 118 | Incomplete | Basic Go/Node example; lacks cross-stage artifact copying, target stages, and test stages | Rename to `04-multi-stage-builds.md`, expand to 13 sections |
| `03_dockerfile/04_dockerignore.md` | 89 | Incomplete | 89 lines; lacks root/nested glob rules, secret leaks prevention, and `.git` omission | Rename to `05-dockerignore.md`, expand to 13 sections |
| `03_dockerfile/README.md` | 17 | Stub | Minimal stub list | Rewrite to comprehensive module overview |
| `04_volumes_networking/01_volumes-bind-mounts.md` | 112 | Incomplete | Covers volume vs bind mount; lacks tmpfs, permission mapping (`--user`), and NFS drivers | Rename to `01-volumes-and-bind-mounts.md`, expand to 13 sections |
| `04_volumes_networking/02_networking.md` | 118 | Incomplete | Shallow bridge network coverage; omits host/none/macvlan/overlay and embedded DNS | Rename to `04-networking.md`, expand to 13 sections |
| `04_volumes_networking/README.md` | 17 | Stub | Minimal stub list | Rewrite to comprehensive module overview |
| `05_docker_compose/01_compose-basics.md` | 118 | Incomplete | Basic syntax; uses outdated `docker-compose` naming conventions | Rename to `01-compose-basics.md`, rewrite to Compose Spec v2 |
| `05_docker_compose/02_compose-multi-service.md` | 118 | Incomplete | Basic web+db; lacks healthcheck dependencies (`condition: service_healthy`), profiles | Rename to `02-compose-multi-service.md`, expand to 13 sections |
| `05_docker_compose/README.md` | 17 | Stub | Minimal stub list | Rewrite to comprehensive module overview |
| `06_production/01_environments-secrets.md` | 118 | Incomplete | Basic env discussion; lacks BuildKit `--mount=type=secret`, Doppler/Vault integrations | Rename to `01-environments-and-secrets.md`, expand to 13 sections |
| `06_production/02_image-optimization.md` | 118 | Incomplete | Discusses Alpine/Distroless; lacks SBOM generation, Chainguard images, and vulnerability scanning | Rename to `02-image-optimisation.md`, expand to 13 sections |
| `06_production/03_docker-in-ci.md` | 118 | Incomplete | Basic GitHub Actions; lacks buildx cache export/import (GHA cache), Cosign signing, OIDC | Rename to `03-docker-in-ci.md`, expand to 13 sections |
| `06_production/README.md` | 17 | Stub | Minimal stub list | Rewrite to comprehensive module overview |

---

## 3. Structural Deficiencies & Naming Irregularities

1. **Snake_Case Directory Violation**:
   - `00_setup`, `01_docker_fundamentals`, `02_docker_commands`, `03_dockerfile`, `04_volumes_networking`, `05_docker_compose`, `06_production` violate repository standard `NN-kebab-case`.
2. **Underscore File Naming**:
   - Files prefixed with `01_`, `02_`, etc., violate the repository standard `NN-kebab-case.md`.
3. **Unnumbered Filenames**:
   - `00_setup/getting-started.md` lacks standard sequential numbering.
4. **Missing Production Modules**:
   - Complete absence of Module 07 (Production Reference Projects), Module 08 (Technical Interview Preparation), and Module 09 (Quick Reference Cheatsheets).

---

## 4. Technical Advice & Outdated Patterns Found

1. **Outdated Compose Syntax**: Existing notes reference legacy Compose concepts without clarifying that `version:` is obsolete and deprecated in Compose Specification v2.
2. **Missing BuildKit Mechanics**: BuildKit features (`--mount=type=cache`, `--mount=type=secret`, `buildx bake`, multi-platform emulation via QEMU) are largely omitted from the existing Dockerfile notes.
3. **Absence of Modern Security Hardening**: No in-depth coverage of rootless Docker execution, Linux capabilities dropping (`--cap-drop=ALL`), read-only root filesystems, user namespace mapping, seccomp profiles, or Docker daemon hardening.
4. **No Container-to-Kubernetes Bridge**: Lacks conceptual alignment between single-host container management and container orchestration (Kubernetes pods, services, ingress controllers).
