# NGINX Learning Track: Architectural Decision Records (ADRs)

---

### ADR-001: Baseline Version Standards (NGINX Stable 1.30 vs. Mainline 1.31)
- **Context**: The NGINX project maintains two release streams: Stable (annual branch receiving critical security/bug fixes) and Mainline (active feature branch).
- **Decision**: Standardize all production guidance on the current Stable series (`1.30.x`), while explicitly noting Mainline (`1.31.x`) enhancements where bleeding-edge capabilities (such as modern HTTP/3 features and njs updates) originate. Document that stock Ubuntu 24.04 LTS ships with `1.24.x`.
- **Consequences**: Ensures enterprise stability recommendations while keeping engineers ahead of protocol migrations.

---

### ADR-002: Modern HTTP/2 Standalone Directive (`http2 on;`) vs. Legacy Syntax
- **Context**: In NGINX 1.25.1, `listen ... http2;` was deprecated and replaced with the standalone `http2 on;` directive. Older tutorials universally show the deprecated syntax, causing confusion on modern deployments.
- **Decision**: Use `http2 on;` as the primary standard across all modern configuration notes. Clearly highlight the deprecation notice and provide fallback guidance for legacy LTS installations (`listen 443 ssl http2;`).
- **Consequences**: Future-proofs all curriculum configurations while explaining warning logs to engineers operating older systems.

---

### ADR-003: HTTP/3 (QUIC) Implementation Standard
- **Context**: HTTP/3 operates over UDP using the QUIC transport protocol, requiring distinct configuration compared to TCP listeners.
- **Decision**: Standardize HTTP/3 examples using `listen 443 quic reuseport;` alongside standard TCP `listen 443 ssl;`. Mandate `ssl_protocols TLSv1.3;` and explicit `add_header Alt-Svc` configuration. Document the critical requirement to open UDP port 443 in firewalls and perform full process restarts when altering `reuseport`.
- **Consequences**: Prevents common networking failures where HTTP/3 is configured in NGINX but blocked by perimeter firewalls or bottlenecked on a single worker core.

---

### ADR-004: Kubernetes Ingress-NGINX EOL Documentation & Gateway API Architecture
- **Context**: The open-source `kubernetes/ingress-nginx` project officially reached End-of-Life (EOL) in March 2026. Many repositories still teach ingress-nginx as the standard ingress controller.
- **Decision**: Document the official retirement status of `kubernetes/ingress-nginx`. Transition Kubernetes routing curriculum to the modern **Kubernetes Gateway API** using **NGINX Gateway Fabric**, providing migration playbooks using `ingress2gateway`.
- **Consequences**: Equips engineers with the contemporary cloud-native routing standard while eliminating security vulnerabilities associated with legacy ingress annotations.

---

### ADR-005: Restructuring Reference Materials into Modules 08 & 09
- **Context**: The original track placed `cheatsheet.md` and `interview-questions.md` inside an unnumbered `08-reference/` directory.
- **Decision**: In accordance with `04-devops/CONVENTIONS.md`, dissolve `08-reference/` into two distinct numbered modules:
  - `08-interview/`: 8 comprehensive technical interview notes covering conceptual questions, debugging scenarios, and architecture discussions.
  - `09-cheatsheets/`: 8 task-oriented syntax and configuration snippet sheets.
- **Consequences**: Standardizes navigation across all DevOps tracks (`aws/`, `docker/`, `git-github/`, `linux/`, `nginx/`).

---

### ADR-006: 5-Section Production Project Specification
- **Context**: Existing projects in `07-projects/` were unstructured and lacked verifiable topologies.
- **Decision**: Enforce the strict 5-section project template for all 8 reference projects in `07-projects/`:
  1. Metadata & Production Baseline
  2. Architectural Overview & Component Topology (with ASCII network diagram)
  3. Complete Production Implementation (fenced configuration blocks)
  4. Automated Verification & Test Harness (cURL test commands, health probes, output assertions)
  5. Operational Runbook & Troubleshooting Playbook
- **Consequences**: Guarantees production-grade repeatability and realistic operational practice.
