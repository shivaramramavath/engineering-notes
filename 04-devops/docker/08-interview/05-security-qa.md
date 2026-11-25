<!--
================================================================================
Metadata:
  Title: Technical Interview Preparation - Container Security and Hardening Q&A
  Track: Docker / DevOps
  Difficulty: Intermediate to Staff
  Estimated Time: 70 minutes
  Key Terminology: Container Breakout, Linux Capabilities, Seccomp, AppArmor, Rootless Docker, Cosign, Trivy, SBOM, CVE-2019-5736, no-new-privileges
================================================================================
-->

# Container Security and Hardening: 25 In-Depth Interview Questions & Answers

[<- 04-compose-qa.md](./04-compose-qa.md) | [Back to Interview Overview](./README.md) | [Next Note: 06-production-and-troubleshooting-scenarios.md ->](./06-production-and-troubleshooting-scenarios.md)

---

## Overview

This guide provides 25 advanced security interview questions covering container breakout mechanics, Linux kernel isolation primitives (capabilities, seccomp, AppArmor), supply chain security (SBOM, Cosign), vulnerability scanning, and defense-in-depth hardening.

---

## Questions Index

1. [Q01: What is a container breakout (escape) and what are the most common breakout vectors?](#q01-what-is-a-container-breakout-escape-and-what-are-the-most-common-breakout-vectors)
2. [Q02: Explain the technical mechanics of the infamous runc container escape vulnerability (CVE-2019-5736).](#q02-explain-the-technical-mechanics-of-the-infamous-runc-container-escape-vulnerability-cve-2019-5736)
3. [Q03: Why is mounting `/var/run/docker.sock` considered equivalent to giving root access to the host?](#q03-why-is-mounting-varrundockersock-considered-equivalent-to-giving-root-access-to-the-host)
4. [Q04: What are Linux Capabilities and why is running `--cap-drop=ALL` a best practice?](#q04-what-are-linux-capabilities-and-why-is-running---cap-dropall-a-best-practice)
5. [Q05: Why is `CAP_SYS_ADMIN` dangerous and often referred to as "the new root"?](#q05-why-is-cap_sys_admin-dangerous-and-often-referred-to-as-the-new-root)
6. [Q06: What does the `--privileged` flag actually do to a container at the kernel level?](#q06-what-does-the---privileged-flag-actually-do-to-a-container-at-the-kernel-level)
7. [Q07: How does Seccomp (Secure Computing Mode) protect the host kernel from container exploits?](#q07-how-does-seccomp-secure-computing-mode-protect-the-host-kernel-from-container-exploits)
8. [Q08: What is `no-new-privileges` and how does it prevent SUID/SGID privilege escalation?](#q08-what-is-no-new-privileges-and-how-does-it-prevent-suidsgid-privilege-escalation)
9. [Q09: How does User Namespaces (`userns-remap`) prevent root-level host compromise?](#q09-how-does-user-namespaces-userns-remap-prevent-root-level-host-compromise)
10. [Q10: What is Rootless Docker and what are its operational constraints?](#q10-what-is-rootless-docker-and-what-are-its-operational-constraints)
11. [Q11: Why should production containers run with a read-only root filesystem (`--read-only`)?](#q11-why-should-production-containers-run-with-a-read-only-root-filesystem---read-only)
12. [Q12: How do AppArmor and SELinux enforce Mandatory Access Control (MAC) on containers?](#q12-how-do-apparmor-and-selinux-enforce-mandatory-access-control-mac-on-containers)
13. [Q13: How do vulnerability scanners like Trivy and Grype detect CVEs in container images?](#q13-how-do-vulnerability-scanners-like-trivy-and-grype-detect-cves-in-container-images)
14. [Q14: What is a Software Bill of Materials (SBOM) and what is the difference between CycloneDX and SPDX?](#q14-what-is-a-software-bill-of-materials-sbom-and-what-is-the-difference-between-cyclonedx-and-spdx)
15. [Q15: How does Cosign implement cryptographic image signing and verification?](#q15-how-does-cosign-implement-cryptographic-image-signing-and-verification)
16. [Q16: Explain Sigstore "Keyless Signing" using OIDC and Fulcio / Rekor.](#q16-explain-sigstore-keyless-signing-using-oidc-and-fulcio--rekor)
17. [Q17: What are SLSA (Supply-chain Levels for Software Artifacts) framework levels?](#q17-what-are-slsa-supply-chain-levels-for-software-artifacts-framework-levels)
18. [Q18: How can an attacker abuse Docker's default iptables rules to bypass perimeter firewalls?](#q18-how-can-an-attacker-abuse-dockers-default-iptables-rules-to-bypass-perimeter-firewalls)
19. [Q19: How do you defend against "Zip Bombs" and resource exhaustion attacks inside containers?](#q19-how-do-you-defend-against-zip-bombs-and-resource-exhaustion-attacks-inside-containers)
20. [Q20: What is the security risk of using unpinned floating tags like `:latest`?](#q20-what-is-the-security-risk-of-using-unpinned-floating-tags-like-latest)
21. [Q21: How do you securely handle sensitive runtime secrets in Docker without disk persistence?](#q21-how-do-you-securely-handle-sensitive-runtime-secrets-in-docker-without-disk-persistence)
22. [Q22: Why is running an SSH daemon inside a container considered a security anti-pattern?](#q22-why-is-running-an-ssh-daemon-inside-a-container-considered-a-security-anti-pattern)
23. [Q23: What are the security implications of exposing `/proc` and `/sys` inside a container?](#q23-what-are-the-security-implications-of-exposing-proc-and-sys-inside-a-container)
24. [Q24: How does Docker Content Trust (DCT) prevent untrusted image execution?](#q24-how-does-docker-content-trust-dct-prevent-untrusted-image-execution)
25. [Q25: What is the CIS Docker Benchmark and how do you audit a production host against it?](#q25-what-is-the-cis-docker-benchmark-and-how-do-you-audit-a-production-host-against-it)

---

## Technical Q&A

<details>
<summary><strong>Q01: What is a container breakout (escape) and what are the most common breakout vectors?</strong></summary>

A container breakout occurs when an adversary executing code inside a container bypasses kernel isolation boundaries to execute arbitrary code or access resources directly on the host operating system.
**Primary Breakout Vectors**:
1. **Docker Socket Exposure**: Mounting `/var/run/docker.sock` allows spawning privileged containers with host root mounts.
2. **Excessive Capabilities / Privileged Flags**: `--privileged` or `CAP_SYS_ADMIN` allows mounting physical host disk devices or loading kernel modules.
3. **Kernel Exploits**: Linux kernel privilege escalation flaws (e.g., Dirty COW, Dirty Pipe) shared between container and host.
4. **Runtime Flaws**: Bugs in container runtimes (`runc` CVE-2019-5736, CVE-2024-21626) allowing filesystem or process namespace overwrites.
5. **Misconfigured `/proc` or `/sys` mounts**: Writable `release_agent` in cgroups v1 allowing arbitrary host binary execution.
</details>

<details>
<summary><strong>Q02: Explain the technical mechanics of the infamous runc container escape vulnerability (CVE-2019-5736).</strong></summary>

In CVE-2019-5736:
1. When `docker exec` runs, the host `runc` binary executes and briefly enters the container's mount and PID namespaces before dropping privileges.
2. An attacker with root access inside the container opens `/proc/self/exe` (which points to the host's active `runc` binary).
3. The attacker opens `/proc/self/exe` in write-only mode (via `/proc/<runc-pid>/fd/...`) and overwrites the host's physical `/usr/bin/runc` executable on disk with malicious shellcode.
4. The next time the host administrator or Docker daemon invokes `runc` to start or exec any container, the malicious shellcode executes as `root` on the host operating system.
*Remediation*: Modern `runc` seals its memory and uses anonymous memory file descriptors (`memfd_create`) to prevent process image rewriting.
</details>

<details>
<summary><strong>Q03: Why is mounting `/var/run/docker.sock` considered equivalent to giving root access to the host?</strong></summary>

The Docker daemon socket is the unrestricted API control plane for the engine. If an attacker controls a container with `/var/run/docker.sock` mounted, they can run:
```bash
docker run -v /:/host-root --privileged -it alpine chroot /host-root
```
This instructs the host Docker daemon to spawn a new privileged container mounting the entire host root filesystem (`/`) to `/host-root` and chroots into it. The attacker is immediately granted an interactive root shell on the host machine, bypassing all user permissions, container cgroups, and network isolation.
</details>

<details>
<summary><strong>Q04: What are Linux Capabilities and why is running `--cap-drop=ALL` a best practice?</strong></summary>

In traditional UNIX, root (UID 0) has full authority. Linux Capabilities divide root privileges into distinct units (e.g. `CAP_NET_BIND_SERVICE` to bind low ports, `CAP_CHOWN` to change file ownership).
By default, Docker grants ~14 capabilities to containers. Most application workloads (Node, Python, Go web services) require **zero** root capabilities.
Running:
```bash
docker run --cap-drop=ALL --cap-add=NET_BIND_SERVICE my-app
```
Strips all capabilities except binding low ports. Even if an attacker achieves remote code execution as root inside the container, they cannot load kernel modules, manipulate network routing tables, or debug sibling processes.
</details>

<details>
<summary><strong>Q05: Why is `CAP_SYS_ADMIN` dangerous and often referred to as "the new root"?</strong></summary>

`CAP_SYS_ADMIN` is a catch-all Linux capability that grants vast administrative kernel privileges. A container with `CAP_SYS_ADMIN` can:
- Mount and unmount arbitrary filesystems.
- Configure Linux namespaces and cgroups.
- Interact with device drivers and load BPF programs.
An attacker with `CAP_SYS_ADMIN` can exploit cgroups v1 `release_agent` or mount host `/dev/sda1` block devices directly, reliably breaking out to host root within seconds.
</details>

<details>
<summary><strong>Q06: What does the `--privileged` flag actually do to a container at the kernel level?</strong></summary>

`--privileged` disables almost all container isolation boundaries:
1. Grants **all Linux Capabilities** (equivalent to `--cap-add=ALL`).
2. Disables the default **Seccomp** profile (permitting all system calls).
3. Disables **AppArmor** and **SELinux** restrictions.
4. Mounts host `/dev` devices into the container, allowing the container to access raw host hard drive block devices (`/dev/nvme0n1`, `/dev/sda`).
*Rule of Thumb*: `--privileged` should almost never be used in production application workloads.
</details>

<details>
<summary><strong>Q07: How does Seccomp (Secure Computing Mode) protect the host kernel from container exploits?</strong></summary>

Seccomp filters and restricts the system calls that container processes can execute against the shared Linux kernel.
- The Linux kernel supports over 300 system calls.
- Docker's default Seccomp profile blocks approximately 44 system calls (including `reboot`, `swapon`, `swapoff`, `kexec_load`, `mount`, `acct`).
- If an unpatched kernel vulnerability relies on a rarely used system call (e.g. `bpf` or `keyctl`), Docker's Seccomp filter blocks the call, neutralizing the kernel exploit before it can execute.
</details>

<details>
<summary><strong>Q08: What is `no-new-privileges` and how does it prevent SUID/SGID privilege escalation?</strong></summary>

Linux binaries with the SUID (Set User ID) bit set (like `/usr/bin/passwd` or `/bin/su`) execute with the permissions of the file owner (root), regardless of which user ran them.
Configuring `--security-opt=no-new-privileges:true` (or setting `"no-new-privileges": true` in `daemon.json`) sets the `PR_SET_NO_NEW_PRIVS` kernel bit. This guarantees that neither the container process nor any spawned child can ever gain additional privileges via `execve()` of SUID/SGID binaries, mitigating local privilege escalation attacks.
</details>

<details>
<summary><strong>Q09: How does User Namespaces (`userns-remap`) prevent root-level host compromise?</strong></summary>

Without User Namespaces, UID 0 inside a container is UID 0 on the host kernel.
When `userns-remap` is enabled in `/etc/docker/daemon.json`:
- Docker maps UID 0 (root) inside the container to an unprivileged subordinate user (e.g. UID 165536) on the host.
- The process believes it is root inside its container (can install packages, bind ports).
- If the process escapes to the host filesystem, it has only the restricted permissions of host UID 165536 and cannot modify host root files (`/etc/shadow`, `/boot`).
</details>

<details>
<summary><strong>Q10: What is Rootless Docker and what are its operational constraints?</strong></summary>

Rootless Docker runs both the Docker daemon (`dockerd`) and container shims entirely as a regular, unprivileged non-root user account using User Namespaces.
**Operational Constraints**:
1. Cannot bind host ports below 1024 without sysctl adjustment (`net.ipv4.ip_unprivileged_port_start=80`).
2. Network performance is slightly degraded due to userspace network translation (`slirp4netns`).
3. Cgroups v1 resource limits are unsupported; requires Linux cgroups v2 with systemd delegation.
</details>

<details>
<summary><strong>Q11: Why should production containers run with a read-only root filesystem (`--read-only`)?</strong></summary>

Running `--read-only` mounts the container's root filesystem (`/`) as immutable:
1. **Malware Prevention**: Attackers exploiting web vulnerabilities (e.g. RCE) cannot download web shells, drop malicious binaries, or patch system scripts in `/tmp` or `/app`.
2. **Enforces Immutability**: Guarantees that applications do not save state locally.
3. If temporary scratch space is needed, combine `--read-only` with explicit in-memory `tmpfs` mounts:
   ```bash
   docker run --read-only --tmpfs /tmp:rw,noexec,nosuid my-app
   ```
</details>

<details>
<summary><strong>Q12: How do AppArmor and SELinux enforce Mandatory Access Control (MAC) on containers?</strong></summary>

- **AppArmor (Ubuntu/Debian)**: Path-based MAC. Docker loads a default profile (`docker-default`) that restricts reading/writing to sensitive kernel paths (`/proc/sysrq-trigger`, `/proc/kcore`, `/sys`) and forbids mounting filesystems.
- **SELinux (RHEL/CentOS/Fedora)**: Label-based MAC. Assigns multi-category security labels (`svirt_lxc_net_t`) to container processes and storage. Containers with different category labels cannot access each other's storage even if host file permissions are `777`.
</details>

<details>
<summary><strong>Q13: How do vulnerability scanners like Trivy and Grype detect CVEs in container images?</strong></summary>

1. **Unpacking**: The scanner parses the image manifest and unpacks all layer tarballs into a merged filesystem view.
2. **OS Package Detection**: Queries operating system package manager databases (`/var/lib/dpkg/status`, `/var/lib/rpm`, `/lib/apk/db/installed`) to extract installed packages and versions.
3. **Language Dependency Detection**: Scans application lockfiles (`package-lock.json`, `Pipfile.lock`, `pom.xml`, `go.sum`).
4. **CVE Matching**: Matches detected software components and versions against upstream vulnerability feeds (NVD, GitHub Advisories, Debian/RedHat security trackers) and reports matching Common Vulnerabilities and Exposures (CVEs) with CVSS severity scores.
</details>

<details>
<summary><strong>Q14: What is a Software Bill of Materials (SBOM) and what is the difference between CycloneDX and SPDX?</strong></summary>

An SBOM is a comprehensive, machine-readable inventory of all software components, libraries, OS packages, and licenses embedded inside a container image.
- **CycloneDX (OWASP standard)**: Designed specifically for application security, automated vulnerability analysis, and software supply chain risk management. Supports rich component dependency graphs and vulnerability exploitability exchange (VEX).
- **SPDX (Linux Foundation / ISO/IEC 5962 standard)**: Designed originally for open-source license compliance and intellectual property tracking, with extended support for software security cataloging.
</details>

<details>
<summary><strong>Q15: How does Cosign implement cryptographic image signing and verification?</strong></summary>

Cosign (part of the Sigstore project) signs container images using standard PKI or OIDC:
1. Cosign downloads the image manifest and computes its SHA256 digest.
2. Cosign signs the digest using a private key (or ephemeral OIDC keypair).
3. The cryptographic signature and payload are uploaded directly to the OCI registry as a separate artifact layer tagged `<digest>.sig`.
4. Verification (`cosign verify --key cosign.pub <image>`) downloads the signature from the registry, verifies it against the public key, and guarantees the image has not been tampered with since signing.
</details>

<details>
<summary><strong>Q16: Explain Sigstore "Keyless Signing" using OIDC and Fulcio / Rekor.</strong></summary>

Traditional signing requires managing and rotating vulnerable private key files on disk. Sigstore "Keyless" signing eliminates long-lived keys:
1. **OIDC Authentication**: In a CI workflow (e.g. GitHub Actions), Cosign obtains an OIDC token proving its identity (`github.com/myorg/myrepo`).
2. **Ephemeral Certificate (Fulcio)**: Cosign generates an in-memory keypair and presents the OIDC token to Fulcio (a free Certificate Authority). Fulcio issues an X.509 certificate bound to the workflow identity, valid for only 10 minutes.
3. **Transparency Log (Rekor)**: The signature and certificate are appended to Rekor (an immutable, append-only Merkle tree ledger).
4. **Verification**: Verifiers check the Rekor log to confirm the signature was generated during the certificate's valid 10-minute validity window by the authorized identity.
</details>

<details>
<summary><strong>Q17: What are SLSA (Supply-chain Levels for Software Artifacts) framework levels?</strong></summary>

SLSA is an industry framework providing incremental standards for software supply chain integrity:
- **SLSA Level 1**: Scripted, automated build process generating basic provenance metadata (recording how artifact was built).
- **SLSA Level 2**: Hosted build service (GitHub Actions, GitLab CI) generating authenticated provenance signed by the build platform.
- **SLSA Level 3**: Hermetic, isolated build environments where build steps cannot communicate with external networks and source code/dependencies are cryptographically verified.
</details>

<details>
<summary><strong>Q18: How can an attacker abuse Docker's default iptables rules to bypass perimeter firewalls?</strong></summary>

When Docker publishes a port (`-p 5432:5432`), it inserts `PREROUTING` rules in the Linux kernel `nat` table before standard host filtering rules (like UFW) are processed.
If an engineer assumes an internal database container is protected because UFW is active on the server, Docker's NAT rule forwards external traffic directly to the container anyway, exposing internal databases to the public internet.
*Remediation*: Always publish ports bound strictly to localhost loopback (`-p 127.0.0.1:5432:5432`) or inject rules into the `DOCKER-USER` chain.
</details>

<details>
<summary><strong>Q19: How do you defend against "Zip Bombs" and resource exhaustion attacks inside containers?</strong></summary>

A Zip Bomb is a tiny compressed file (e.g. 42KB) that expands to petabytes of uncompressed data when unpacked, exhausting host memory and disk space.
**Defenses**:
1. Run containers with `--pids-limit=200` to prevent fork bombs.
2. Set hard memory ceilings (`-m 512m --memory-swap 512m`).
3. Set disk quota limits or mount a size-restricted `tmpfs` volume for file extraction (`--tmpfs /tmp:size=100m`).
4. Never execute `ADD` on untrusted remote tarballs; unpack via verified scripts with stream limiters.
</details>

<details>
<summary><strong>Q20: What is the security risk of using unpinned floating tags like `:latest`?</strong></summary>

Floating tags (`:latest`, `:20`, `:alpine`) are mutable pointers:
1. **Supply Chain Poisoning**: If an attacker compromises an upstream Docker Hub repository, they can overwrite `:latest` with a backdoored image.
2. **Non-Deterministic Rollbacks**: If production crashes and rollbacks re-pull `:latest`, the cluster may download an untested version containing breaking changes or unpatched vulnerabilities.
*Remediation*: Pin exact semantic versions or immutable SHA256 digests (`image@sha256:...`).
</details>

<details>
<summary><strong>Q21: How do you securely handle sensitive runtime secrets in Docker without disk persistence?</strong></summary>

1. **BuildKit Secret Mounts**: Use `RUN --mount=type=secret,id=mysecret` during image builds.
2. **Docker Secrets (Swarm/K8s)**: Mount secrets in-memory as `tmpfs` files at `/run/secrets/<name>`.
3. **External Secrets Integration**: Use HashiCorp Vault Agent or AWS Secrets Manager sidecar to fetch secrets in-memory at process startup rather than storing plaintext `.env` files on disk.
</details>

<details>
<summary><strong>Q22: Why is running an SSH daemon inside a container considered a security anti-pattern?</strong></summary>

1. **Increases Attack Surface**: Introduces OpenSSH daemon vulnerabilities, PAM modules, and brute-force authentication risks.
2. **Credential Sprawl**: Requires managing SSH authorized keys and credentials across individual container instances.
3. **Violates Immutability**: Encourages operators to log in and manually make unversioned configuration changes on running containers instead of fixing the Dockerfile in source control.
*Correct Approach*: Use `docker exec` for debugging, and central CI/CD for updates.
</details>

<details>
<summary><strong>Q23: What are the security implications of exposing `/proc` and `/sys` inside a container?</strong></summary>

The Linux `/proc` and `/sys` filesystems provide direct interfaces into kernel data structures and hardware knobs.
If `/proc` or `/sys` are mounted with read-write permissions inside a container:
- An attacker can modify host kernel parameters via `/proc/sys/kernel/` (e.g. disabling ASLR or kernel module signing).
- An attacker can invoke cgroups v1 `release_agent` to execute arbitrary host commands.
Docker automatically protects containers by masking sensitive `/proc` paths (mounting them as `read-only` or masking them with `tmpfs`).
</details>

<details>
<summary><strong>Q24: How does Docker Content Trust (DCT) prevent untrusted image execution?</strong></summary>

Docker Content Trust (DCT) integrates The Update Framework (TUF) and Notary:
- When enabled via `export DOCKER_CONTENT_TRUST=1`, Docker verifies cryptographic signatures on image manifests before pulling or running them.
- If an image publisher has not signed the image with a valid Notary key, or if the signature has expired, the Docker daemon rejects the pull with:
  `Error: remote trust data does not exist for docker.io/...`.
</details>

<details>
<summary><strong>Q25: What is the CIS Docker Benchmark and how do you audit a production host against it?</strong></summary>

The Center for Internet Security (CIS) Docker Benchmark is a standardized consensus guideline covering security configurations for Docker Engine and Linux hosts.
It defines rules across 5 sections:
1. Host configuration (dedicated partitions for `/var/lib/docker`, auditd logging).
2. Docker daemon configuration (restricting network access, enabling user namespaces, setting default logging).
3. Docker daemon configuration files (file permissions of `daemon.json` and systemd units).
4. Container images and build files (non-root users, content trust, healthchecks).
5. Container runtime (dropping capabilities, read-only rootfs, resource limits).
**Auditing**: Run the automated open-source auditing tool `docker-bench-security`:
```bash
docker run --rm --net host --pid host --userns host --cap-add audit_control \
  -v /etc:/etc:ro -v /var/lib:/var/lib:ro -v /var/run/docker.sock:/var/run/docker.sock \
  docker/docker-bench-security
```
</details>

---

[<- 04-compose-qa.md](./04-compose-qa.md) | [Back to Interview Overview](./README.md) | [Next Note: 06-production-and-troubleshooting-scenarios.md ->](./06-production-and-troubleshooting-scenarios.md)
