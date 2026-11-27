# Module 07: Production Reference Projects & Labs

[<- Back to Linux Track Root](../README.md) | [<- Previous Module: 06-advanced](../06-advanced/README.md) | [Next Module: 08-interview ->](../08-interview/README.md)

---

## 1. Overview & Scope

Theory and command flags become operational intuition when applied to end-to-end production scenarios. Module 07 delivers eight comprehensive, industry-grade reference projects and troubleshooting labs.

Each project is authored with production-ready implementations: complete configurations, defensive bash scripts, unit definitions, and simulated execution logs. All code complies with repository standards (strictly markdown code blocks, zero repository state pollution).

---

## 2. Project Catalog

| # | Project Title | Complexity | Domain | Deliverables & Artifacts |
| :---: | :--- | :---: | :---: | :--- |
| **01** | [Harden a Fresh Cloud Server to CIS Standards](./01-harden-a-fresh-server.md) | Intermediate | Security | Idempotent bash hardening script, SSH key lockdown, UFW firewall, auditd rules, sysctl network hardening. |
| **02** | [Automated Enterprise Backup Pipeline with Rotation](./02-bash-backup-script-with-rotation.md) | Intermediate | Systems | Defensive backup script (`tar` + `zstd`), SHA-256 integrity verification, GPG encryption, Grandfather-Father-Son rotation policy. |
| **03** | [High-Throughput Log Stream Analyser in Awk & Jq](./03-log-analyser-with-awk-and-jq.md) | Intermediate | Scripting | High-speed log parser processing Nginx access logs and JSON application streams, calculating HTTP status rates, 99th percentile latencies, and IP abuse rates. |
| **04** | [Production Systemd Service & Timer Architecture](./04-systemd-service-and-timer-for-an-app.md) | Intermediate | Services | Hardened systemd unit file with security sandboxing (`ProtectSystem`, `PrivateTmp`), health-check watchdogs, and monotonic cleanup timer. |
| **05** | [Build a Container Runtime from Scratch](./05-build-a-container-with-unshare.md) | Advanced | Internals | Complete bash script that constructs an isolated root filesystem, creates PID/NET/MNT/UTS namespaces via `unshare`, attaches cgroups v2 limits, and executes a sandboxed process. |
| **06** | [Production Nginx Reverse Proxy & Systemd Automation](./06-nginx-plus-systemd-deployment.md) | Intermediate | Networking | Automated Nginx deployment, TLS termination with Let's Encrypt, systemd reload automation, and rate-limiting configurations. |
| **07** | [Disk Exhaustion & Out-Of-Memory Recovery Labs](./07-disk-full-and-oom-troubleshooting-labs.md) | Advanced | SRE Ops | Two simulated incident scenarios: unlinked open file descriptor disk exhaustion, and runaway memory leak inducing OOM panic. Step-by-step diagnostic and remediation runbooks. |
| **08** | [Infrastructure Monitoring with Prometheus Node Exporter](./08-monitoring-with-node-exporter.md) | Intermediate | Observability | Production Node Exporter systemd deployment, textfile collectors for custom metrics, security sandboxing, and Prometheus scrape target configuration. |

---

## 3. Project Standards & Format

Every project in this module adheres to the standard 5-part engineering lab format:
1. **Architecture & Design**: Scenario context, threat/operational model, and architectural diagram.
2. **Implementation Artifacts**: Full, copy-pasteable script and configuration code blocks.
3. **Execution & Verification**: Step-by-step test commands and realistic simulated output logs.
4. **Failure Modes & Edge Cases**: What happens during network partitions, disk-full conditions, or bad inputs.
5. **Production Considerations**: Security hardening, maintenance runbooks, and monitoring integration.
