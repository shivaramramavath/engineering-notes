# Security, Hardening & Mandatory Access Control: Technical Interview Q&A

| Metadata | Details |
| :--- | :--- |
| **Topic** | Linux System Security, SELinux, AppArmor, Capabilities & Defensive Hardening |
| **Module** | `08-interview` — Technical Interview Preparation & Scenario Q&A |
| **Track** | `04-devops/linux` |
| **Target Level**| Senior / Staff DevOps, SRE, and Security Operations Engineers |
| **Questions** | 20 Deep Scenario & Architectural Questions |
| **Last Updated**| 2026 |

---

## Overview

Security questions in modern DevOps and SRE interviews probe deep architectural defense-in-depth: the difference between DAC and MAC models, Linux capability containment, system call surface reduction via Seccomp, and the internal mechanics of SELinux and AppArmor.

---

### Q1: What is the fundamental difference between Discretionary Access Control (DAC) and Mandatory Access Control (MAC)?
<details>
<summary>Detailed Answer & Technical Mechanics</summary>

- **Discretionary Access Control (DAC - Standard Unix Permissions)**:
  - Access decisions are based on user/group identity and permission bits (`rwxrwxrwx`).
  - **Discretionary** means the **owner of an object has full discretion** to grant access to anyone else (e.g., `chmod 777 sensitive.db`).
  - The fatal flaw: If an attacker compromises a daemon running as `root`, they inherit unrestricted access to the entire operating system.
- **Mandatory Access Control (MAC - SELinux / AppArmor)**:
  - Access decisions are enforced globally by the operating system kernel based on explicit security policies, completely regardless of user ownership or file permissions.
  - Even if a file is owned by `root` with permissions `777`, if the active MAC policy states that the `httpd_t` process domain cannot read `shadow_t` files, the kernel **blocks access with `-EACCES` and logs an audit violation**. Root itself is constrained by MAC policy!
</details>

---

### Q2: Explain the structure of an SELinux Security Context (`user:role:type:level`).
<details>
<summary>Detailed Answer & Technical Mechanics</summary>

Every process, file inode, directory, and socket in an SELinux system is labeled with a security context tuple:
```text
system_u:object_r:httpd_sys_content_t:s0
   ^        ^             ^           ^
  User     Role          Type       Level (MLS/MCS)
```
1. **User (`system_u`)**: The SELinux user identity (mapped to Linux accounts).
2. **Role (`object_r` for files, `staff_r` for users)**: Role-Based Access Control attribute defining which types a user or object can assume.
3. **Type (`httpd_sys_content_t` - CRITICAL)**: The primary attribute for **Type Enforcement (TE)**. For processes, it is called a **Domain**; for files, it is called a **Type**. SELinux rules state: "Process domain `httpd_t` can read files of type `httpd_sys_content_t`."
4. **Level (`s0` or `s0:c0,c100`)**: Multi-Level / Multi-Category Security. Used in containers (Docker/Podman SELinux separation) to assign unique random category tags to each container, preventing one container from reading another's storage even if they share the same type!
</details>

---

### Q3: What is the difference between SELinux `Enforcing`, `Permissive`, and `Disabled` modes?
<details>
<summary>Detailed Answer & Technical Mechanics</summary>

- **`Enforcing` (Production Default)**: The kernel strictly blocks unauthorized operations and logs the violation to `/var/log/audit/audit.log`. Full protection active.
- **`Permissive`**: The kernel **does NOT block unauthorized operations**; it allows execution to proceed normally, but logs the violation as an `AVC: denied` audit record. Essential for debugging new service policies without breaking production workloads.
- **`Disabled`**: The SELinux kernel subsystem is completely deactivated; files are not labeled. Switching from `Disabled` back to `Enforcing` requires a full filesystem relabel (`touch /.autorelabel && reboot`).
</details>

---

### Q4: An Nginx web server throws `403 Forbidden` accessing files in `/var/www/html`. Linux file permissions are `755` and owned by `www-data`. How do you diagnose SELinux?
<details>
<summary>Detailed Answer & Technical Mechanics</summary>

**Diagnosis Runbook:**
1. **Inspect SELinux Contexts**:
   ```bash
   ls -dZ /var/www/html
   ```
   If files were copied from `/home/deploy/`, their type will likely be `user_home_t` or `admin_home_t` rather than `httpd_sys_content_t`.
2. **Inspect Audit Log**:
   ```bash
   sudo ausearch -m avc -ts recent
   # Look for: type=AVC msg=audit(...): avc: denied { read } for pid=... comm="nginx" name="index.html" dev="sda1" ino=... scontext=system_u:system_r:httpd_t:s0 tcontext=unconfined_u:object_r:user_home_t:s0 tclass=file permissive=0
   ```
3. **Remediation**:
   Restore official default SELinux context defined in policy:
   ```bash
   restorecon -Rv /var/www/html
   ```
</details>

---

### Q5: How does AppArmor differ architecturally from SELinux?
<details>
<summary>Detailed Answer & Technical Mechanics</summary>

| Dimension | AppArmor (Ubuntu / Debian / SUSE) | SELinux (RHEL / CentOS / Fedora) |
| :--- | :--- | :--- |
| **Security Model** | **Path-Based** Access Control | **Inode Label / Type Enforcement** |
| **Profile Association**| Bound to canonical filesystem executable paths (e.g., `/usr/sbin/nginx`). | Bound to security context labels stored in filesystem extended attributes (`xattr`). |
| **Configuration Style**| Human-readable plain text rules in `/etc/apparmor.d/`. | Complex compiled policy modules (`.pp`) written in M4 macro language or CIL. |
| **Renamed Files** | If a binary or file moves, AppArmor rules can break or apply differently. | Security context follows the inode across file moves/renames. |
| **Learning Curve** | Gentle; intuitive profile creation via `aa-genprof`. | Steep; requires deep knowledge of types, booleans, and AVC audits. |
</details>

---

### Q6: What are Linux POSIX Capabilities and why is `CAP_SYS_ADMIN` known as "the new root"?
<details>
<summary>Detailed Answer & Technical Mechanics</summary>

**Linux Capabilities (`capabilities(7)`):**
Decomposes the monolithic authority of the root superuser into ~40 discrete, independent permissions (e.g., `CAP_NET_BIND_SERVICE`, `CAP_CHOWN`, `CAP_SETUID`).

**Why `CAP_SYS_ADMIN` is Dangerous:**
Historically, whenever kernel developers implemented a privileged system call that did not cleanly fit into existing capability definitions, they assigned it to `CAP_SYS_ADMIN`.
Today, `CAP_SYS_ADMIN` grants dozens of dangerous root-equivalent privileges:
- Mounting and unmounting filesystems.
- Configuring kernel namespaces (`unshare`, `clone`).
- Loading eBPF programs.
- Device configuration and driver manipulation.

Granting `CAP_SYS_ADMIN` to a container or user-space daemon is equivalent to granting full root compromise.
</details>

---

### Q7: Explain the Five Capability Sets: Permitted, Effective, Inheritable, Bounding, and Ambient.
<details>
<summary>Detailed Answer & Technical Mechanics</summary>

1. **Effective (`CapEff`)**: The capabilities currently active and checked by the kernel during privileged operations.
2. **Permitted (`CapPrm`)**: The upper boundary of capabilities the process is allowed to assume. A thread can enable or disable capabilities in `CapEff` as long as they exist in `CapPrm`.
3. **Inheritable (`CapInh`)**: Capabilities preserved across an `execve()` system call (rarely used in practice without ambient capabilities).
4. **Bounding (`CapBnd`)**: The system-wide or container ceiling. No process can add a capability to `CapPrm` across `execve()` if it is absent from the Bounding set.
5. **Ambient (`CapAmb` - Kernel 4.3+)**: Capabilities that are automatically inherited by unprivileged child processes across `execve()` without requiring file capabilities on the target binary on disk.
</details>

---

### Q8: What is Seccomp (Secure Computing Mode) and how does Seccomp-BPF protect container runtimes?
<details>
<summary>Detailed Answer & Technical Mechanics</summary>

**Mechanism:**
**Seccomp** is an in-kernel sandboxing framework. When activated via `prctl(PR_SET_SECCOMP)`, the process passes a compiled Berkeley Packet Filter (BPF) program to the kernel.
- On **every system call** invoked by the process, the BPF program inspects the system call number and its memory arguments.
- If the call is on the approved allowlist, the kernel executes it.
- If the call is blocked, Seccomp immediately returns `-EPERM`, logs a violation, or terminates the process with `SIGSYS`.

**Container Protection:**
The Linux kernel exposes ~450 system calls. A standard Docker or Kubernetes container requires only ~70 for normal microservice operations. The default container Seccomp profile blocks ~44 dangerous system calls (such as `kexec_load`, `reboot`, `sysfs`, `keyctl`, and `clone3`), cutting the attack surface against unpatched kernel privilege escalation exploits by over 80%.
</details>

---

### Q9: Why is `PR_SET_NO_NEW_PRIVS` a prerequisite for unprivileged Seccomp and User Namespaces?
<details>
<summary>Detailed Answer & Technical Mechanics</summary>

**The Security Threat It Solves:**
Suppose an unprivileged user attaches a Seccomp filter that intercepts the `setuid()` system call and forces it to return 0 (success) without actually dropping root privileges, and then executes a SUID binary like `sudo`. The SUID binary would execute with elevated root rights while running under an untrusted, compromised filter!

**`no_new_privs` Guarantee:**
Before an unprivileged user is permitted to load Seccomp filters or create user namespaces, they must set `PR_SET_NO_NEW_PRIVS` via `prctl()`. The kernel guarantees that neither the process nor any of its future children can **ever** gain privileges greater than they possessed when the flag was set, rendering SUID binaries inert.
</details>

---

### Q10: Detail the essential security configurations for a production OpenSSH Server (`sshd_config`).
<details>
<summary>Detailed Answer & Technical Mechanics</summary>

**Production SSH Hardening Baseline:**
```text
Port 2222                          # Move off default port 22 to eliminate automated bot scans
PermitRootLogin no                 # Prevent direct root login; require named user with sudo
PasswordAuthentication no          # Enforce cryptographic SSH keys only
PubkeyAuthentication yes
MaxAuthTries 3                     # Disconnect after 3 failed key attempts
ClientAliveInterval 300            # Terminate idle sessions after 10 minutes
ClientAliveCountMax 2
X11Forwarding no                   # Prevent X11 GUI hijacking
AllowAgentForwarding no            # Prevent SSH agent socket theft on shared nodes
KexAlgorithms curve25519-sha256    # Disable weak legacy DH ciphers
Ciphers chacha20-poly1305@openssh.com,aes256-gcm@openssh.com
```
</details>

---

### Q11: Why is Ed25519 overwhelmingly preferred over RSA 2048/4096 for SSH Keys?
<details>
<summary>Detailed Answer & Technical Mechanics</summary>

- **Security & Cryptography**: Ed25519 is an Elliptic Curve Digital Signature Algorithm based on Curve25519. It provides ~128 bits of security with immune design against timing side-channel attacks and bad random number generator failures. RSA 2048 is vulnerable to factorization improvements; RSA 4096 is computationally heavy.
- **Key Length & Speed**: Ed25519 public keys are only **68 characters long** (32 bytes), making them clean and easy to copy into configs without line wrapping. Key generation and cryptographic signing are over **10x faster** than RSA 4096.
</details>

---

### Q12: Explain the PAM (Pluggable Authentication Modules) architecture and the 4 control flags: `required`, `requisite`, `sufficient`, `optional`.
<details>
<summary>Detailed Answer & Technical Mechanics</summary>

**The PAM Pipeline:**
When a user logs in via SSH or runs `sudo`, the program delegates authentication to the PAM subsystem (`/etc/pam.d/*`).

**Control Flags Evaluated:**
- **`required`**: Must succeed for the authentication request to be granted. If it fails, PAM **continues executing remaining modules** in the stack before returning failure to the user (prevents timing attacks).
- **`requisite`**: Must succeed. If it fails, PAM **aborts execution immediately**, returning failure without evaluating subsequent modules.
- **`sufficient`**: If this module succeeds (and no prior `required` module failed), PAM **grants immediate success** and exits the stack without evaluating remaining modules.
- **`optional`**: Success or failure is ignored unless it is the only module in the stack.
</details>

---

### Q13: What security risks are introduced by `NOPASSWD: ALL` in `/etc/sudoers` and how do you restrict it safely?
<details>
<summary>Detailed Answer & Technical Mechanics</summary>

**The Risk:**
Configuring `deploy ALL=(ALL) NOPASSWD: ALL` means that if an attacker achieves Remote Code Execution (RCE) in a web application running under the `deploy` user account, they can instantly execute `sudo su -` or `sudo bash` without entering a password, achieving complete root compromise in 1 second.

**Safe Restriction (Command Whitelisting & Sandboxing):**
1. Never grant unrestricted `ALL`! Restrict to exact binary paths:
   ```text
   deploy ALL=(ALL) NOPASSWD: /usr/bin/systemctl restart nginx, /usr/bin/systemctl reload nginx
   ```
2. **Prevent Shell Escapes**: Never grant `NOPASSWD` to binaries that have built-in shell escape capabilities (such as `vim`, `less`, `more`, `find`, `awk`, `python`, `tar`). For example, running `sudo vim` allows an attacker to type `:!bash` and spawn an unrestricted root shell!
</details>

---

### Q14: How does Address Space Layout Randomization (ASLR) prevent binary exploitation?
<details>
<summary>Detailed Answer & Technical Mechanics</summary>

**The Exploit It Prevents (Return-to-libc / ROP):**
In legacy Linux, the execution stack, heap, and standard libraries (`libc.so`) were loaded at fixed, predictable virtual memory addresses. An attacker exploiting a buffer overflow could overwrite the instruction pointer (`RIP`) with the exact static address of `system("/bin/sh")` inside `libc`.

**ASLR Mechanism (`/proc/sys/kernel/randomize_va_space`):**
- Modern kernels randomize the starting memory addresses of the stack, heap, memory-mapped shared libraries, and Position-Independent Executables (PIE) **every time a program is executed**.
- Even if an attacker can overwrite the instruction pointer, they cannot predict where `libc` or shellcode functions reside in virtual memory, turning a reliable remote exploit into a harmless crashing segmentation fault (`SIGSEGV`).
- Value `2` enables full randomization of stack, mmap regions, and heap.
</details>

---

### Q15: What is the YAMA Linux Security Module and what is the significance of `kernel.yama.ptrace_scope`?
<details>
<summary>Detailed Answer & Technical Mechanics</summary>

**Mechanism:**
YAMA is an in-kernel security module that restricts the use of `ptrace(2)`.

**Values of `/proc/sys/kernel/yama/ptrace_scope`:**
- **`0` (Classic)**: A process can attach to and inspect *any* process owned by the same user account. (High security risk: malware running as an unprivileged user can attach to the user's running SSH agent or browser and scrape decryption keys!).
- **`1` (Restricted Default on Ubuntu/Debian)**: A process can only ptrace its direct child processes. To trace unrelated daemons, `sudo` (`CAP_SYS_PTRACE`) is required.
- **`2` (Admin Only)**: Only processes with `CAP_SYS_PTRACE` can attach.
- **`3` (Locked)**: Disables `ptrace` system-wide permanently until reboot.
</details>

---

### Q16: Why should production servers restrict `/proc/kallsyms` via `kernel.kptr_restrict`?
<details>
<summary>Detailed Answer & Technical Mechanics</summary>

**Mechanism:**
`/proc/kallsyms` contains the memory addresses of all internal kernel symbols and functions.

**Security Impact:**
If an unprivileged attacker achieves local code execution, they can read `/proc/kallsyms` to bypass Kernel ASLR (KASLR). Knowing exact kernel memory addresses allows attackers to craft Return-Oriented Programming (ROP) exploit chains to escalate to root.
- Setting `kernel.kptr_restrict = 2` instructs the kernel to mask all kernel memory pointers as `0000000000000000` for all users (including root), blinding attackers against kernel exploitation.
</details>

---

### Q17: What are the security benefits of `fs.protected_hardlinks` and `fs.protected_symlinks`?
<details>
<summary>Detailed Answer & Technical Mechanics</summary>

**The Attack It Prevents (Symlink / Hardlink Race Conditions in `/tmp`):**
In legacy systems, an unprivileged attacker could create a symlink in world-writable `/tmp`:
```bash
ln -s /etc/shadow /tmp/tempfile
```
When a root-owned maintenance daemon or cron script wrote to `/tmp/tempfile`, it followed the symlink and overwrote `/etc/shadow`!

**Kernel Protection:**
- `fs.protected_symlinks = 1`: The kernel refuses to follow a symlink located in a world-writable directory (like `/tmp`) unless the symlink's owner matches the directory owner or the file's owner.
- `fs.protected_hardlinks = 1`: Prevents users from creating hardlinks to files they do not own, stopping attackers from pinning vulnerable SUID binaries across unprivileged paths.
</details>

---

### Q18: What is Linux `auditd` and how do you track unauthorized changes to `/etc/shadow`?
<details>
<summary>Detailed Answer & Technical Mechanics</summary>

**Mechanism:**
The Linux Audit Subsystem (`auditd`) logs security-relevant kernel events directly to `/var/log/audit/audit.log` based on immutable rules.

**Configuration Rule (`/etc/audit/rules.d/cis.rules`):**
```text
-w /etc/shadow -p wa -k identity_changes
-w /etc/sudoers -p wa -k sudoers_changes
```
- `-w /etc/shadow`: Watch target file path.
- `-p wa`: Trigger on write (`w`) or attribute change (`a`).
- `-k identity_changes`: Tag event with custom searchable key.

Query audit log using `ausearch -k identity_changes` to view the exact PID, executable binary, and invoking User ID responsible for the file modification.
</details>

---

### Q19: What is the purpose of `fail2ban` and how does it interface with `nftables`?
<details>
<summary>Detailed Answer & Technical Mechanics</summary>

**Mechanism:**
`fail2ban` is an intrusion prevention daemon that monitors system log streams (`journald`, `/var/log/auth.log`) using regular expression filters.
- When an IP address accumulates multiple failed authentication attempts (e.g., 5 failed SSH logins in 10 minutes), `fail2ban` dynamically executes firewall commands to ban the IP.
- In modern systems, configuring `banaction = nftables-multiport` instructs `fail2ban` to insert the abusive IP into a high-speed in-kernel **nftables Set**, dropping matching packets at wire speed with sub-microsecond latency and zero CPU overhead.
</details>

---

### Q20: Explain the security implications of Linux Namespaces and how an unprivileged User Namespace can lead to root exploits.
<details>
<summary>Detailed Answer & Technical Mechanics</summary>

**The Promise vs. The Hazard:**
**User Namespaces (`CLONE_NEWUSER`)** allow an unprivileged user (UID 1000) to map their ID to UID 0 (root) *inside the container*. This enables rootless containers (Podman, Rootless Docker).

**The Hazard:**
Inside the user namespace, the process possesses full capabilities (`CAP_SYS_ADMIN`, `CAP_NET_ADMIN`). While these capabilities are bounded to the namespace and do not apply to the host, they allow the unprivileged process to interact with complex kernel subsystems (e.g., netfilter, mount engines, eBPF) that are normally restricted strictly to host root. Historical kernel CVEs (Dirty Pipe, Netfilter use-after-free) were frequently triggered by unprivileged users executing exploit code inside user namespaces to escalate to real host root!
- On hardened servers not running rootless containers, disable unprivileged user namespaces:
  `sysctl -w kernel.unprivileged_userns_clone=0`.
</details>

---
*Next Note: [08-behavioural-and-project-discussion.md](file:///04-devops/linux/08-interview/08-behavioural-and-project-discussion.md)*
