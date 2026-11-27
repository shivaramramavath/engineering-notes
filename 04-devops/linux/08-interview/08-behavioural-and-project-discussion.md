# System Design, SRE Operations & Behavioral Discussions: Technical Interview Q&A

| Metadata | Details |
| :--- | :--- |
| **Topic** | Production System Design, Incident Management, Post-Mortems & Senior SRE Leadership |
| **Module** | `08-interview` — Technical Interview Preparation & Scenario Q&A |
| **Track** | `04-devops/linux` |
| **Target Level**| Senior / Staff / Principal DevOps & Site Reliability Engineers |
| **Questions** | 15 Deep Architectural & Scenario Questions |
| **Last Updated**| 2026 |

---

## Overview

At Senior, Staff, and Principal engineering levels, technical interviews shift beyond raw commands and single-host mechanics. Interviewers evaluate how you design multi-tier architectures, lead teams through catastrophic production outages, conduct blameless root-cause analyses, manage technical debt, and communicate engineering trade-offs to business stakeholders.

---

### Q1: Design a high-availability, multi-region Linux infrastructure baseline for a mission-critical financial API.
<details>
<summary>Detailed Answer & Architectural Blueprint</summary>

**Architectural Blueprint:**
```
  [ Global Anycast DNS / Cloudflare Route 53 Latency Routing ]
                              |
       +----------------------+----------------------+
       |                                             |
       v (Region East - Primary)                     v (Region West - Secondary)
  [ Edge Nginx / HAProxy Clustered Gateways ]   [ Edge Nginx / HAProxy Clustered Gateways ]
  - TLS 1.3 Termination, WAF, Rate Limiting    - Active standby or Active/Active
       |                                             |
       v                                             v
  [ Application Kubernetes / Systemd Fleet ]    [ Application Kubernetes / Systemd Fleet ]
  - Hardened OS (CIS Level 1 Ubuntu 24.04)     - DynamicUser, cgroups v2, read-only rootfs
       |                                             |
       v                                             v
  [ Distributed Database (PostgreSQL / Aurora)] <---> [ Cross-Region Asynchronous Replication ]
```

**Key Pillars to Emphasize:**
1. **Immutable Infrastructure**: Provision base operating systems using Packer-generated golden images. Golden images have SSH password authentication disabled, CIS kernel hardening baked in, and minimal package footprints.
2. **Perimeter Isolation**: Zero public IP addresses on application hosts. All ingress routes through redundant load balancers in public subnets; application servers live in private VPC subnets accessed only via VPN or ephemeral SSH certificate jump bastions.
3. **Automated Failover & Health Probing**: Health probes must check deep dependency health (e.g., database read/write readiness), not just static HTTP 200 checks on a `/healthz` file.
</details>

---

### Q2: Walk me through a zero-downtime operating system migration (e.g., migrating 500 servers from Ubuntu 20.04 to 24.04 LTS).
<details>
<summary>Detailed Answer & Operational Execution</summary>

**The Anti-Pattern (In-Place Upgrade):**
Never run `do-release-upgrade` in-place on 500 production instances! In-place upgrades are non-deterministic, take 30+ minutes per host, break custom package dependencies, leave deprecated config files behind, and cannot be rolled back cleanly upon failure.

**The Production Strategy (Blue/Green or Rolling Canaries):**
1. **Phase 1: Build & Validate New Golden Image**:
   - Construct a standardized Ubuntu 24.04 LTS image via Packer.
   - Run automated compliance verification suites (Lynis / OpenSCAP) and integration test suites in staging.
2. **Phase 2: Deploy Parallel "Green" Fleet**:
   - Spin up new instances running Ubuntu 24.04 alongside existing 20.04 instances in the same VPC using Terraform/Ansible.
   - Deploy identical application builds to Green nodes.
3. **Phase 3: Canary Traffic Routing (1% -> 10% -> 50% -> 100%)**:
   - Add a small pool of Green instances to the load balancer target group (1% of traffic).
   - Closely monitor P99 latency, error rates (HTTP 5xx), and kernel crash logs via Prometheus and Datadog.
4. **Phase 4: Complete Shift & Teardown**:
   - Shift 100% of traffic to Green.
   - Retain Blue instances powered down for 24 hours as an instantaneous rollback contingency. Terminate Blue once confidence is established.
</details>

---

### Q3: You are the Incident Commander for a P0 outage: all production database servers have suddenly become unresponsive. Walk me through your first 30 minutes.
<details>
<summary>Detailed Answer & Operational Execution</summary>

**Minute 0–5: Establish Command & Communication Channels:**
- Declare the P0 incident in Slack (`#incident-2026-db-outage`) and start an incident bridge call.
- Assign clear incident roles: **Incident Commander (IC)** (runs the bridge, coordinates investigation), **Operations Lead** (executes diagnostic actions), and **Communications Lead** (posts updates to internal stakeholders and public status page every 15 minutes).
- Mute extraneous chatter; mandate that no changes are applied to production without IC approval.

**Minute 5–15: Triage & Blast Radius Assessment:**
- Verify symptoms: Are instances unreachable via network, or are database ports hanging?
- Access instances via Cloud Console Serial Interface (bypassing potentially saturated network).
- Execute non-invasive initial triage checklist:
  - Check kernel logs: `dmesg -T | grep -E -i "oom|panic|error|hung_task"`.
  - Check storage queue latency: `iostat -xz 1`.
  - Check connection queues: `ss -lnt` on database port.
- Prevent well-intentioned engineers from blindly restarting all instances simultaneously, which destroys in-memory diagnostic state, core dumps, and risks database corruption!

**Minute 15–30: Mitigation Over Root Cause:**
- Prioritize **service restoration over deep forensic analysis**.
- If a runaway analytics query or memory leak caused an OOM loop, terminate the offending connection queries, increase emergency swap, or failover to the standby replica.
- Once traffic stabilizes, preserve system logs (`journalctl`, core dumps, memory maps) for post-mortem analysis.
</details>

---

### Q4: Explain the principles of a "Blameless Post-Mortem" and how you lead one following a catastrophic outage.
<details>
<summary>Detailed Answer & Cultural Mechanics</summary>

**Core Philosophy:**
A blameless post-mortem operates on the foundational premise that **engineers are skilled, well-intentioned professionals making the best decisions possible with the information they had at the time**. 

Punishing an engineer for typing an incorrect command or pushing a flawed configuration does not improve reliability—it breeds a culture of fear, secrecy, and cover-ups. Human error is the *starting point* of an investigation, not the conclusion. The real question is: **Why did our systems allow a single human mistake to take down production?**

**Structure of the Review:**
1. **Executive Summary**: High-level impact (duration, user disruption, financial/SLA impact).
2. **Timeline of Events (UTC)**: Detailed chronology of actions, alerts, detections, and remediations.
3. **Root Cause Analysis (The 5 Whys)**: Drill down beyond the superficial trigger to discover systemic deficiencies in testing, tooling, architecture, or guardrails.
4. **Where We Got Lucky**: Identify accidental saves that prevented worse outcomes.
5. **Action Items (SMART)**: Concrete engineering deliverables with designated owners and target completion dates (e.g., "Add automated syntax validation to deployment pipeline" rather than "Remind engineers to be careful").
</details>

---

### Q5: How do you perform Capacity Planning for an e-commerce platform anticipating a 10x traffic surge on Black Friday?
<details>
<summary>Detailed Answer & Operational Execution</summary>

**Capacity Planning Framework:**
1. **Establish Baseline Metrics**: Determine current resource consumption per active user request across CPU, memory footprint, database IOPS, and network egress bandwidth.
2. **Stress & Chaos Testing (Locust / k6 / Chaos Mesh)**:
   - Run synthetic distributed load tests simulating 10x traffic profiles in a staging environment mirrored from production.
   - Incrementally increase load to identify the **first bottleneck in the chain** (e.g., does Nginx worker socket limits overflow? Does database connection pooling saturate? Does storage disk latency spike?).
3. **Horizontal & Vertical Sizing**:
   - Autoscale stateless application fleets horizontally.
   - Upscale stateful database instances vertically (provisioning high-IOPS NVMe storage volumes).
4. **Kernel & OS Parameter Tuning**:
   - Enlarge listen backlogs: `net.core.somaxconn=65535` and `tcp_max_syn_backlog=65535`.
   - Maximize file descriptor ceilings: `fs.file-max=2097152` and `systemd LimitNOFILE=1048576`.
   - Pre-warm database buffer caches and enable edge CDN caching (Cloudflare) for static assets.
</details>

---

### Q6: How do you combat Alert Fatigue in an SRE team overwhelmed by hundreds of false-positive PagerDuty alerts weekly?
<details>
<summary>Detailed Answer & Operational Execution</summary>

**The Anti-Fatigue Strategy:**
1. **Audit Every Alert Against Actionability**:
   - If an engineer receives an alert at 03:00 AM, wakes up, looks at a dashboard, decides no action is required, and goes back to sleep, **that alert must be deleted immediately or converted into a daytime ticketing report**.
   - An alert must strictly signify that **user-facing SLOs are burning**, and human intervention is required *now*.
2. **Shift from Cause-Based Alerting to Symptom-Based Alerting**:
   - Stop alerting on "CPU utilization > 85%" (a healthy batch processing server should run at 100% CPU!).
   - Alert on user impact: **High Error Rate (HTTP 5xx > 1%)** or **High Latency (P99 > 500ms)**.
3. **Enforce Cooldowns, Grouping & Auto-Resolution**:
   - Configure alert managers (Prometheus Alertmanager) with `group_by`, `group_wait: 30s`, and `repeat_interval: 4h` to prevent alert storms where a single network glitch triggers 50 separate alerts.
4. **Mandate Runbook Links for Every Alert**:
   - Every alert must contain a direct link to an actionable, verified runbook detailing diagnostic steps and remediation commands.
</details>

---

### Q7: An engineer accidentally executes `chmod -R 777 /` on a production server. What breaks, and how do you recover?
<details>
<summary>Detailed Answer & Operational Execution</summary>

**What Breaks Immediately:**
1. **`sudo` Stops Working**: `sudo` requires that its binary (`/usr/bin/sudo`) has SUID permissions and that `/etc/sudoers` has permissions `0440` owned by root. If `/etc/sudoers` is world-writable (`777`), `sudo` refuses to execute: `sudo: /etc/sudoers is world writable, exiting`.
2. **SSH Daemon Refuses Connections**: OpenSSH strictly enforces permissions on host keys (`/etc/ssh/ssh_host_*_key` must be `0600`) and user authorized keys. It refuses to start or authenticate sessions with insecure world-writable keys.
3. **Authentication Failure (`/etc/shadow`)**: Passwords and PAM modules abort when critical security databases are world-writable.

**Recovery Strategy:**
- Do **NOT** attempt to manually guess and `chmod` thousands of files back by hand!
- If instances are cloud-based and stateless: **Immediately terminate the instance and let the Auto-Scaling Group replace it with a clean image**.
- If instance is a stateful pet:
  - Boot via rescue environment or mount disk on a healthy helper VM.
  - On Debian/Ubuntu: Re-apply default package permissions using `dpkg --set-selections` and `debsums` / `dpkg-statoverride`.
  - On RHEL/CentOS: Restore package permissions directly from RPM database:
    `rpm --setperms -a && rpm --setugids -a`.
</details>

---

### Q8: Describe a real-world scenario where you resolved a difficult, elusive production bug.
<details>
<summary>Detailed Answer & Behavioral Storytelling (STAR Method)</summary>

**Structure (Situation, Task, Action, Result):**

- **Situation**: In a high-throughput microservice architecture, API responses intermittently experienced 5-second connection latency spikes during traffic peaks, causing upstream circuit breakers to trip. Application logs showed zero errors.
- **Task**: Isolate the root cause of the 5-second delays across a cluster of 50 Linux nodes without disrupting live customer transactions.
- **Action**:
  1. Observed that 5 seconds is the standard default Linux DNS resolver timeout.
  2. Captured network packets using `tcpdump -nn -i any port 53` during a latency spike. Discovered that the primary corporate DNS server occasionally dropped UDP DNS queries under microbursts.
  3. Traced userland glibc calls using `strace -e trace=network` and noticed that when the primary nameserver dropped the UDP packet, the application thread blocked synchronously for exactly 5000ms before falling back to the secondary nameserver.
  4. Deployed local DNS caching via `systemd-resolved` / `dnsmasq` on every host and tuned `/etc/resolv.conf` with `options timeout:1 attempts:2 rotate`.
- **Result**: Completely eliminated the 5-second latency spikes, reducing P99 API latency from 5,200ms to 45ms.
</details>

---

### Q9: How do you design and enforce a patching and CVE vulnerability remediation strategy across 1,000 Linux servers?
<details>
<summary>Detailed Answer & Operational Execution</summary>

**Four-Tier Patching Strategy:**
1. **Automated Kernel & Security Micro-Updates**:
   - Enable `unattended-upgrades` (Ubuntu) or `dnf-automatic` (RHEL) configured strictly for Security repositories. Run non-disruptive security updates daily with randomized execution windows.
2. **Live Kernel Patching (Ksplice / Canonical Livepatch / kpatch)**:
   - Apply critical Linux kernel security patches directly into memory without requiring system reboots for high-availability database nodes.
3. **Immutable Fleet Image Recycling**:
   - For container nodes and stateless application clusters, bake security updates into weekly golden images (AMIs) and cycle servers automatically via rolling instance refreshes.
4. **Vulnerability Scanning & Compliance Tracking**:
   - Continuously scan hosts using agents (Wazuh, Qualys, Amazon Inspector).
   - Enforce strict SLA remediation tiers: **Critical CVEs (CVSS > 9.0)** patched within 48 hours; **High CVEs** patched within 14 days.
</details>

---

### Q10: How do you handle a production host that has been compromised by an attacker?
<details>
<summary>Detailed Answer & Operational Execution</summary>

**Four-Step Forensic Containment:**
1. **Isolate, Do NOT Power Off**:
   - Do **NOT** execute `reboot` or `poweroff`! Powering off destroys volatile RAM, killing memory-resident rootkits, active process execution paths, and unwritten network socket states.
   - Immediately isolate the host at the network layer: apply a security group or firewall rule dropping all inbound and outbound traffic, allowing access only from an isolated forensic analyst bastion IP.
2. **Live Forensic Memory Acquisition**:
   - Snapshot the physical memory of the VM using the cloud provider hypervisor API (AWS EC2 Memory Snapshot or LiME kernel module) to capture encryption keys, injected shellcode, and decrypted commands.
   - Snapshot all attached block storage volumes.
3. **Evidence Preservation**:
   - Mount disk snapshots read-only on an isolated forensic workstation. Extract audit logs (`/var/log/audit/`), shell histories, and check for unlinked hidden files.
4. **Rebuild from Scratch**:
   - Never attempt to "clean" a compromised host! You can never trust the kernel, binaries, or libraries. Terminate the compromised instance and redeploy from verified clean code and golden images.
</details>

---

### Q11: Explain how you balance the conflicting requirements of Developer Velocity vs. Production Security Hardening.
<details>
<summary>Detailed Answer & Engineering Leadership</summary>

**Core Philosophy: "Paved Roads" Over Blockades:**
If security policies force developers to file manual tickets and wait two weeks for an approved Linux server or port opening, developers will inevitably bypass security using shadow IT, insecure workarounds, or permissive configurations.

**The Strategy:**
1. **Automate the Secure Path ("The Paved Road")**: Provide pre-hardened, self-service infrastructure templates (Terraform modules, hardened container base images, pre-configured systemd templates with least-privilege sandboxing). When doing the secure thing is the easiest and fastest way to deploy, developers naturally adopt it.
2. **Shift-Left Guardrails in CI/CD**: Integrate automated linters and security checks directly into GitHub Actions / GitLab CI (`shellcheck`, `trivy`, `semgrep`). Alert developers of permission or secret issues inside their pull requests before code reaches staging.
3. **Granular, Ephemeral Access**: Replace permanent static SSH keys and root passwords with ephemeral, auditable, certificate-based access (Teleport, HashiCorp Boundary, AWS SSM Session Manager) with automatic session recording.
</details>

---

### Q12: How do you calculate and manage Error Budgets and SLOs (Service Level Objectives)?
<details>
<summary>Detailed Answer & Operational Execution</summary>

**The Math:**
- **SLA (Service Level Agreement)**: The contractual promise to customers with financial penalties (e.g., 99.9% monthly uptime).
- **SLO (Service Level Objective)**: The internal engineering target, set stricter than the SLA (e.g., 99.95% availability).
- **Error Budget**: The allowable room for failure:
  $$\text{Error Budget} = 100\% - \text{SLO} = 100\% - 99.95\% = \mathbf{0.05\%}$$
  For a 30-day month (43,200 minutes), an Error Budget of 0.05% equates to exactly **21.6 minutes of allowable downtime**.

**The Governance Policy:**
- **Budget Remaining (> 20%)**: Developers are free to push new features, test aggressive architecture changes, and deploy continuously.
- **Budget Exhausted (< 0%)**: Feature deployments are **frozen**! All engineering velocity shifts exclusively to reliability, technical debt elimination, test coverage expansion, and infrastructure stabilization until the error budget recovers.
</details>

---

### Q13: Walk me through designing an automated Disaster Recovery (DR) plan with an RTO of 15 minutes and RPO of 1 minute.
<details>
<summary>Detailed Answer & Architectural Blueprint</summary>

**Definitions:**
- **RPO (Recovery Point Objective)**: Maximum acceptable data loss window ($\le 1 \text{ minute}$).
- **RTO (Recovery Time Objective)**: Maximum acceptable time to restore service ($\le 15 \text{ minutes}$).

**Implementation Blueprint:**
1. **Data Layer (RPO $\le 1$ min)**:
   - Synchronous or sub-second asynchronous cross-region database replication (e.g., PostgreSQL streaming replication with physical WAL streaming).
   - Continuous WAL archiving to cross-region replicated object storage (S3 Glacier Instant Retrieval).
2. **Infrastructure Layer (RTO $\le 15$ min - "Pilot Light" Architecture)**:
   - Maintain a minimal standby "pilot light" footprint in the recovery region (database standby replica active, minimal base VPC).
   - Infrastructure as Code (Terraform) pipelines pre-tested in CI to provision full application auto-scaling groups on demand within 6 minutes.
   - Global Anycast DNS routing (Cloudflare / Route 53) configured with automated health-check failover to switch traffic to the recovery region within 3 minutes.
3. **Continuous Drills (Chaos Engineering)**:
   - A DR plan that is not regularly tested does not work! Execute scheduled quarterly automated "Game Day" failovers.
</details>

---

### Q14: How do you communicate a severe technical incident and technical debt to a non-technical Chief Executive Officer (CEO)?
<details>
<summary>Detailed Answer & Executive Communication</summary>

**Key Principles:**
1. **Lead with Business & Customer Impact**: The CEO does not care about `ptrace`, `TCP MSS clamping`, or `inode exhaustion`. They care about: *How many customers were affected? Was customer data lost or stolen? What was the financial impact? Is the service stable right now?*
2. **Translate Technical Debt into Business Risk**:
   - Bad: "We need 3 weeks to refactor our systemd services and upgrade to Ubuntu 24.04."
   - Good: "Our underlying server infrastructure is reaching its end-of-life support date. Operating on this unsupported version exposes us to unpatched security vulnerabilities and increases our risk of a multi-day outage during our peak holiday sales season. Investing three weeks now prevents customer-facing downtime later."
3. **Provide Clear Ownership and Prevention Steps**: State clearly what actions are being taken, who owns them, and what guardrails will be deployed to guarantee the issue cannot recur.
</details>

---

### Q15: What is the most significant architectural mistake you have witnessed or made, and what did you learn from it?
<details>
<summary>Detailed Answer & Candidate Reflection</summary>

**Exemplary Reflection Framework:**
- **The Mistake**: Early in my career, we designed a centralized cron server that executed multi-threaded synchronization scripts over NFS shared storage mounts across 30 instances without distributed locking.
- **The Failure**: When the NFS server experienced transient network packet loss, the file locks hung. Cron spawned hundreds of overlapping duplicate script processes, exhausting file descriptors and causing a cascade of race conditions that corrupted user profile caches.
- **The Technical Epiphany**: Shared state over network storage is an anti-pattern. Stateful jobs require strict, deterministic distributed locking (via Redis or Consul) or must be decomposed into event-driven message queues (SQS / RabbitMQ).
- **The Engineering Takeaway**: Architect systems to be **idempotent by design**. Assume network filesystems, networks, and remote APIs will fail, drop packets, or hang, and design failure boundaries into every layer of the operating system stack.
</details>

---
*Module 08 Complete. Proceed to Module 09: High-Density Cheatsheets.*
