# Module 02: Essential Commands

## Overview

Welcome to **Module 02: Essential Commands** of the Docker and Container Engineering track. This module provides a comprehensive, production-grade guide to the Docker Command Line Interface (CLI).

Rather than presenting an arbitrary list of commands, this module explores the architectural mechanics, operational flags, forensic diagnostics, and automation patterns that power professional container management. You will master the full lifecycle of containers (`run`, `ps`, `stop`, `kill`), container observability (`logs`, `exec`, `inspect`), image manipulation (`pull`, `tag`, `push`, `rmi`, `save`, `load`), the modern BuildKit engine (`build`, `buildx`), automated resource reclamation (`system df`, `prune`), fine-grained cgroup resource governance (CPU, Memory, Swap, PIDs, `docker update`), restart policies, file transfer pipelines (`docker cp`), real-time daemon event streaming (`events`, `stats`, `top`), and advanced Go template output formatting.

---

## Learning Objectives

By completing this module, you will be able to:
- Execute containers with precise runtime control using detached mode, interactive TTYs, port publishing, and environment variables.
- Filter and query active and historical container processes using advanced `docker ps` predicates and Go template formatting.
- Stream live container logs across specific time windows and understand how logging drivers handle `stdout` and `stderr`.
- Inject diagnostic shells and monitoring processes into running containers using `docker exec` and Linux `setns()`.
- Query the engine's internal data model using `docker inspect` to extract IPs, ports, mount points, and health states deterministically.
- Manage local image content stores, optimize network pushes and pulls via layer deduplication, and export images into air-gapped tar archives using `docker save` and `docker load`.
- Compile images using modern BuildKit features: parallel multi-stage targeting, persistent cache mounts, ephemeral secret mounts, and cross-platform multi-arch builds.
- Safely reclaim host disk space using `docker system df` and selective `prune` commands without risking persistent database volume loss.
- Enforce hard and soft hardware resource limits (Memory, Swap, Completely Fair Scheduler CPU quotas, core pinning, and PID caps), and tune limits dynamically on running containers using `docker update`.
- Implement automated self-healing using container restart policies (`on-failure`, `unless-stopped`), understanding exponential backoff and host reboot semantics.
- Stream files and database backups seamlessly between host and containers via `docker cp` and interactive exec pipelines without network overhead.
- Monitor real-time daemon event buses (`docker events`), live performance metrics (`docker stats`), and host process trees (`docker top`).
- Master the Go `text/template` formatting engine to generate custom tables, conditional evaluations, loops, and structured JSON output for automated scripting.

---

## Module Roadmap & Notes

| Note | Title | Focus & Core Competencies | Est. Read Time |
|:---|:---|:---|:---|
| **[01-run-ps-stop.md](./01-run-ps-stop.md)** | **Container Lifecycle Commands** | `docker run` flags, detached execution, `docker ps` filtering/sizing, graceful `docker stop` vs immediate `docker kill`, and signal targeting. | 25 min |
| **[02-logs-exec-inspect.md](./02-logs-exec-inspect.md)** | **Container Diagnostics & Observability** | Live log streaming, time-window filtering (`--since`), process injection via `docker exec`, and Go template metadata extraction via `docker inspect`. | 25 min |
| **[03-images.md](./03-images.md)** | **Image Management Commands** | Layer deduplication in `pull`/`push`, tag aliasing, untagging vs layer deletion in `rmi`, air-gapped archiving via `save`/`load`, and `docker history`. | 25 min |
| **[04-build.md](./04-build.md)** | **Image Build Commands** | Build contexts, `.dockerignore`, BuildKit DAG solver, multi-stage targets, build arguments, cache mounts, secret mounts, and cross-platform compilation. | 25 min |
| **[05-cleanup.md](./05-cleanup.md)** | **Disk Hygiene & Resource Reclamation** | Storage analysis via `system df`, selective vs nuclear pruning, the dangers of `--volumes`, time-windowed GC, and virtual disk compaction. | 25 min |
| **[06-resource-limits-cpu-memory.md](./06-resource-limits-cpu-memory.md)** | **Container Resource Constraints** | Hard/soft memory limits, swap controls, CFS CPU quotas, CPU core pinning (`--cpuset-cpus`), fork bomb protection via `--pids-limit`, and `docker update`. | 25 min |
| **[07-restart-policies.md](./07-restart-policies.md)** | **Restart Policies & Self-Healing** | The 4 policies (`no`, `on-failure`, `always`, `unless-stopped`), exponential backoff algorithms, host reboot recovery, and orchestrator boundaries. | 25 min |
| **[08-copying-files-and-exec-patterns.md](./08-copying-files-and-exec-patterns.md)** | **File Copying & Streaming Pipelines** | `docker cp` tar streaming architecture, copying to stopped containers, streaming database backups/restores, and artifact extraction patterns. | 25 min |
| **[09-docker-events-stats-and-top.md](./09-docker-events-stats-and-top.md)** | **Real-Time Observability** | The daemon event bus (`docker events`), live performance metrics (`docker stats`), process tree inspection on minimal containers (`docker top`), and auto-healing daemons. | 25 min |
| **[10-formatting-output-with-go-templates.md](./10-formatting-output-with-go-templates.md)** | **Advanced CLI Formatting with Go Templates** | The Go template engine grammar, built-in functions, conditionals, loops, tabular output (`table \t`), and high-yield recipes across all subcommands. | 25 min |

---

## Recommended Learning Flow

```
[01-run-ps-stop.md] 
         |
         v
[02-logs-exec-inspect.md]
         |
         v
[03-images.md]
         |
         v
[04-build.md]
         |
         v
[05-cleanup.md]
         |
         +----------------------------------+
         |                                  |
         v                                  v
[06-resource-limits-cpu-memory.md]       [07-restart-policies.md]
         |                                  |
         +----------------------------------+
         |
         v
[08-copying-files-and-exec-patterns.md]
         |
         v
[09-docker-events-stats-and-top.md]
         |
         v
[10-formatting-output-with-go-templates.md]
         |
         v
[Proceed to Module 03: Dockerfile Deep Dive]
```

---

## Prerequisites

Before starting this module, you should have:
1. Completed **[Module 00: Setup & Installation](../00-setup/README.md)** and **[Module 01: Core Fundamentals](../01-fundamentals/README.md)**.
2. A functioning local or remote Docker daemon (v23+ recommended with BuildKit enabled).
3. Working familiarity with Linux shell commands and pipelines.

---

| [< Previous: Module 01 Fundamentals](../01-fundamentals/README.md) | [Next: Module 03 Dockerfile Deep Dive >](../03-dockerfile/README.md) |
| :--- | :--- |
