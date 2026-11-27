# Mandatory Access Control: SELinux and AppArmor Architecture

| Metadata | Details |
| :--- | :--- |
| **Topic** | Mandatory Access Control (MAC), SELinux (RHEL/Rocky), AppArmor (Ubuntu/Debian), LSM Architecture |
| **Module** | `05-production` |
| **Track** | `04-devops/linux` |
| **Prerequisites** | `01-fundamentals/07-permissions.md`, `05-production/01-server-hardening.md` |
| **Target Audience** | Security Engineers, SREs, Systems Architects, Linux Administrators |
| **Target Distribution** | Rocky Linux / RHEL 9.4 (SELinux default), Ubuntu 24.04 LTS (AppArmor default) |
| **Key Tools** | `getenforce`, `setenforce`, `restorecon`, `semanage`, `audit2allow`, `aa-status`, `aa-enforce`, `aa-complain` |

---

## Table of Contents
1. [What & Why](#what--why)
2. [Core Concepts](#core-concepts)
3. [Internal Working](#internal-working)
4. [Syntax & CLI Tools](#syntax--cli-tools)
5. [Progressive Practical Examples](#progressive-practical-examples)
6. [Best Practices](#best-practices)
7. [Common Mistakes & Gotchas](#common-mistakes--gotchas)
8. [Performance, Security & Cost Implications](#performance-security--cost-implications)
9. [Comparison Table / Alternatives](#comparison-table--alternatives)
10. [Real-World Troubleshooting & Interview Q&A](#real-world-troubleshooting--interview-qa)
11. [Hands-On Practice Exercises](#hands-on-practice-exercises)
12. [Summary Cheat Sheet & Further Reading](#summary-cheat-sheet--further-reading)

---

## What & Why

Traditional Unix security relies on **Discretionary Access Control (DAC)**: standard permissions (`rwxrwxrwx`), ownership (`chown`), and POSIX ACLs. Under DAC:
- If a web server daemon running as `www-data` or `apache` is compromised via a Remote Code Execution (RCE) vulnerability, the attacker acquires all permissions of that user.
- If a service runs as `root` (e.g. Nginx, Docker, SSHD), an exploit yields **unrestricted, catastrophic control** over the entire operating system, permitting the attacker to modify `/etc/shadow`, tamper with kernel parameters, or install rootkits.

**Mandatory Access Control (MAC)** solves this fatal flaw. Integrated into the Linux kernel via the **Linux Security Modules (LSM)** framework:
- Security policies are defined centrally by the system administrator and enforced directly by the Linux kernel.
- **Even the `root` superuser is bound by MAC policy constraints.**
- Two primary MAC implementations dominate the enterprise Linux ecosystem:
  1. **SELinux (Security-Enhanced Linux)**: Developed by the NSA and Red Hat; default on RHEL, CentOS, Rocky Linux, and Fedora. Uses comprehensive **label-based Type Enforcement**.
  2. **AppArmor**: Developed by Canonical and SUSE; default on Ubuntu, Debian, and openSUSE. Uses **path-based profile enforcement**.

Understanding how to read Access Vector Cache (AVC) denials, manage file labels, toggle booleans, and tailor AppArmor profiles eliminates the dangerous sysadmin habit of disabling security frameworks (`setenforce 0`).

---

## Core Concepts

### DAC vs MAC
- **Discretionary Access Control (DAC)**: The owner of an object (file or directory) decides who has access to it. If user `alice` makes `/home/alice/secret.txt` readable by all (`chmod 777`), the operating system permits everyone to read it.
- **Mandatory Access Control (MAC)**: The kernel checks system-wide policy rules on **every single system call**, regardless of DAC permissions. Even if `/var/www/html/index.html` has mode `777`, if the SELinux policy states that `httpd_t` cannot read files labeled `user_home_t`, access is **blocked and logged**.

### SELinux Architecture (Label-Based MAC)
In SELinux, everything is categorized into **Subjects** (processes) and **Objects** (files, sockets, ports, directories).
Every subject and object carries a **Security Context** formatted in 4 fields:
```text
user:role:type:level
Example: system_u:object_r:httpd_sys_content_t:s0
```
1. **User (`system_u`)**: SELinux identity, separate from Unix accounts.
2. **Role (`object_r`)**: Role-Based Access Control (RBAC) component (e.g., `system_r`, `user_r`).
3. **Type (`httpd_sys_content_t`)**: **The primary enforcement mechanism (Type Enforcement)**. Defines what domain a process belongs to (`httpd_t`) and what types of files it can touch (`httpd_sys_content_t`).
4. **Level (`s0` / MLS/MCS)**: Multi-Level Security / Multi-Category Security (e.g., used by Docker/Kubernetes container isolation `s0:c123,c456`).

### SELinux Operating Modes
- **Enforcing**: Policy is actively enforced. Violations are blocked and logged to the audit subsystem as AVC denials.
- **Permissive**: Policy is loaded, but violations are **not blocked**; they are only logged. Ideal for troubleshooting and developing new policies.
- **Disabled**: SELinux is completely unloaded from the kernel. Files created while disabled lose their extended security attributes (`xattr`), requiring a full filesystem relabel upon re-enabling.

### AppArmor Architecture (Path-Based MAC)
AppArmor ties security policies directly to **executable file paths** rather than inode labels:
- Profiles reside in `/etc/apparmor.d/` (e.g., `/etc/apparmor.d/usr.sbin.nginx`).
- Rules define what files, network sockets, and Linux capabilities the named binary is permitted to touch:
  ```text
  /usr/sbin/nginx {
      /var/www/html/** r,
      /var/log/nginx/* w,
      network inet tcp,
      capability net_bind_service,
  }
  ```
- **Modes**:
  - `enforce`: Restricts execution and logs violations.
  - `complain`: Permits unlisted operations while logging warnings to `/var/log/syslog` or `journald`.

---

## Internal Working

```
                                Userspace Application Syscall
                                 (e.g., Nginx open("/etc/shadow"))
                                               |
                                               v
+-----------------------------------------------------------------------------------------+
|                        Step 1: Standard Linux DAC Check                                 |
|                       Checks Unix UID, GID, Mode (rwxr-xr-x)                            |
+----------------------------------------------+------------------------------------------+
                                               | Passed DAC?
                                               v
+-----------------------------------------------------------------------------------------+
|                  Step 2: Linux Security Module (LSM) Hook Call                          |
|                       Kernel calls security_file_open()                                 |
+----------------------------------------------+------------------------------------------+
                                               |
                     +-------------------------+-------------------------+
                     | SELinux Path                                      | AppArmor Path
                     v                                                   v
+-----------------------------------------+     +-----------------------------------------+
|    Access Vector Cache (AVC) Lookup     |     |     AppArmor Profile Path Evaluator     |
|  Source Type: httpd_t                   |     |  Binary: /usr/sbin/nginx                |
|  Target Type: shadow_t                  |     |  Requested Path: /etc/shadow            |
|  Operation:   read                      |     |  Operation:      read                   |
+--------------------+--------------------+     +--------------------+--------------------+
                     |                                               |
                     | Cache Miss?                                   | Path Not Permitted
                     v                                               v
+-----------------------------------------+     +-----------------------------------------+
|     Security Server Policy Evaluation   |     |      Policy Decision: DENIED            |
|     Result: DENIED (No matching rule)   |     +--------------------+--------------------+
+--------------------+--------------------+                          |
                     |                                               |
                     +-------------------------+---------------------+
                                               |
                                               v
+-----------------------------------------------------------------------------------------+
|                                    Audit Subsystem Logging                              |
|         Logs AVC Denial to /var/log/audit/audit.log or /var/log/syslog                  |
|         Returns -EACCES (Permission Denied) to Application Process                      |
+-----------------------------------------------------------------------------------------+
```

### The Access Vector Cache (AVC)
Evaluating complex policy rules in the security server on every single filesystem read or network packet would devastate performance. 
- SELinux maintains an in-kernel **Access Vector Cache (AVC)**.
- Once a subject-to-object decision is evaluated, the result is cached.
- Subsequent system calls execute at near-native speed ($O(1)$ hash table lookup).

---

## Syntax & CLI Tools

### SELinux Inspection and Control (RHEL / Rocky Linux)
```bash
# Query active SELinux mode (Enforcing, Permissive, Disabled)
getenforce

# Switch mode at runtime (1 = Enforcing, 0 = Permissive)
sudo setenforce 0
sudo setenforce 1

# View detailed status and policy version
sestatus

# Inspect security context of files (-Z)
ls -laZ /var/www/html/

# Inspect security context of running processes (-Z)
ps auxZ | grep nginx

# Restore default filesystem contexts from policy database
sudo restorecon -Rv /var/www/html/

# Temporarily change file context manually
sudo chcon -t httpd_sys_content_t /var/www/html/app.py
```

### Managing SELinux Booleans and Ports
```bash
# List all booleans and their current states
getsebool -a | grep httpd

# Toggle a boolean persistently (-P)
sudo setsebool -P httpd_can_network_connect 1
sudo setsebool -P httpd_can_network_connect_db 1

# List network ports permitted for a service type
sudo semanage port -l | grep http_port_t

# Allow Nginx to listen on a custom non-standard port (e.g. 8088)
sudo semanage port -a -t http_port_t -p tcp 8088
```

### AppArmor Inspection and Control (Ubuntu / Debian)
```bash
# View loaded profiles and their status (enforce vs complain)
sudo aa-status

# Put a profile into Complain Mode (logs only, does not block)
sudo aa-complain /usr/sbin/nginx

# Put a profile into Enforce Mode (actively blocks violations)
sudo aa-enforce /usr/sbin/nginx

# Disable a profile completely
sudo aa-disable /etc/apparmor.d/usr.sbin.nginx

# Parse system logs and generate updated profile rules interactively
sudo aa-logprof
```

---

## Progressive Practical Examples

### Example 1: Resolving a Custom Web DocumentRoot SELinux Denial
**Scenario**: An administrator changes Nginx or Apache's web root from `/var/www/html` to `/srv/webroot`. Browsing to the site returns `403 Forbidden` even though Unix permissions are `755` (`nginx:nginx`).

```bash
# 1. Inspect file security context
ls -ldZ /srv/webroot
# Output: drwxr-xr-x. root root unconfined_u:object_r:var_t:s0 /srv/webroot
# (Notice the type is 'var_t' instead of 'httpd_sys_content_t'!)

# 2. Check the audit log for AVC denial
sudo ausearch -m avc -ts recent
# Output shows: avc: denied { read } for pid=1240 comm="nginx" name="index.html" dev="sda2"
# scontext=system_u:system_r:httpd_t:s0 tcontext=unconfined_u:object_r:var_t:s0 tclass=file

# 3. Permanently add the new directory mapping to the SELinux policy database
sudo semanage fcontext -a -t httpd_sys_content_t "/srv/webroot(/.*)?"

# 4. Relabel the directory hierarchy
sudo restorecon -Rv /srv/webroot

# 5. Verify corrected context
ls -ldZ /srv/webroot
# Output: drwxr-xr-x. root root system_u:object_r:httpd_sys_content_t:s0 /srv/webroot
# (Web page now loads instantly with HTTP 200 OK!)
```

### Example 2: Allowing Nginx to Act as Reverse Proxy via SELinux Booleans
By default, the `httpd_t` domain is prohibited from establishing outbound network connections. If Nginx proxies to a local application server (e.g., Gunicorn on `127.0.0.1:8000`), Nginx fails with `502 Bad Gateway` and `13: Permission denied while connecting to upstream`.

```bash
# 1. Audit the denial
sudo grep "comm=\"nginx\"" /var/log/audit/audit.log | grep "denied"

# 2. Identify the governing boolean
sudo semanage boolean -l | grep httpd | grep network

# 3. Enable outbound network connections persistently (-P)
sudo setsebool -P httpd_can_network_connect 1

# 4. Verify boolean status
getsebool httpd_can_network_connect
# Output: httpd_can_network_connect --> on
```

### Example 3: Generating Custom Policy Modules with `audit2allow`
When third-party proprietary services trigger legitimate AVC denials that cannot be resolved via existing booleans or standard labels.

```bash
# 1. Filter AVC denials for the specific service from audit log
sudo ausearch -m avc -c mycustomapp > /tmp/app_denials.log

# 2. Generate human-readable explanation (-w)
audit2allow -w -a -i /tmp/app_denials.log

# 3. Compile a custom Type Enforcement (.te) module and binary package (-M)
sudo audit2allow -M mycustomapp_policy -i /tmp/app_denials.log

# 4. Inspect generated policy rules
cat mycustomapp_policy.te

# 5. Install the compiled policy module into the running kernel
sudo semodule -i mycustomapp_policy.pp

# 6. Verify module is loaded
sudo semodule -l | grep mycustomapp_policy
```

### Example 4: Creating a Custom AppArmor Profile for a Python Microservice
Isolate a custom internal API script (`/opt/microservice/app.py`) on Ubuntu 24.04.

```text
# /etc/apparmor.d/opt.microservice.app.py
abi <abi/3.0>,
#include <tunables/global>

/opt/microservice/app.py {
  #include <abstractions/base>
  #include <abstractions/python>

  # Allow reading application directory
  /opt/microservice/ r,
  /opt/microservice/** r,

  # Allow write access strictly to log directory
  /var/log/microservice/*.log rw,

  # Allow network sockets
  network inet tcp,
  network inet udp,

  # Deny access to sensitive system paths explicitly
  deny /etc/shadow r,
  deny /etc/ssh/** r,
  deny /root/** r,
}
```

```bash
# Load and enforce profile
sudo apparmor_parser -r /etc/apparmor.d/opt.microservice.app.py
sudo aa-enforce /opt/microservice/app.py

# Verify enforcement in aa-status
sudo aa-status | grep app.py
```

### Example 5: Non-Destructive Full System Relabeling
If a system was run in `Disabled` mode and re-enabled, or if contexts are corrupted across the disk.

```bash
# Instruct kernel to relabel entire filesystem on next system boot
sudo touch /.autorelabel

# Reboot to allow early-boot systemd relabeling to execute cleanly
sudo reboot

# (The system relabels all inodes before starting user services, ensuring clean security context)
```

---

## Best Practices

1. **Never Set SELinux to `Disabled` or Leave in `Permissive` in Production**: Setting `SELINUX=disabled` strips the kernel of MAC enforcement and invalidates file extended attributes. If an AVC denial occurs, investigate the denial using `ausearch -m avc -ts recent` and resolve via labels or booleans.
2. **Use `semanage fcontext` Before Running `restorecon`**: Running `chcon` only updates the file's runtime context in memory and inode; any future `restorecon` or system update will wipe out the change. Always add the rule to the policy database with `semanage fcontext -a -t <type> "regex"` first, then run `restorecon`.
3. **Use `audit2allow` with Extreme Caution**: Do not run `audit2allow -a -M fixall` blindly. Inspect the generated `.te` file before installing. If an attacker has already compromised the process, `audit2allow` might generate a policy module that permanently authorizes the attacker's malicious actions!
4. **Leverage AppArmor Complain Mode During Staging**: When authoring new AppArmor profiles on Ubuntu, run the service under load in Complain mode (`aa-complain`) for a week. Use `aa-logprof` to review observed system calls, approve necessary paths, and switch to Enforce mode (`aa-enforce`) prior to production rollout.
5. **Install `policycoreutils-python-utils` / `setroubleshoot-server` on RHEL**: These packages provide `semanage`, `audit2allow`, and `sealert`, converting raw hexadecimal kernel audit logs into clear, actionable plain-English diagnostic recommendations.
6. **Ensure Container Workloads Run with Dedicated Contexts**: In Docker and Podman, never use `--security-opt label=disable` or `--privileged` in production. Podman automatically isolates each container in a distinct MCS category (`system_u:system_r:container_t:s0:c123,c456`).

---

## Common Mistakes & Gotchas

| Mistake | Why It Fails | Correct Pattern |
| :--- | :--- | :--- |
| Disabling SELinux (`SELINUX=disabled`) to fix permission errors | Strips all MAC protections; breaches compliance (PCI-DSS/HIPAA); hard to re-enable. | Run in `Permissive` mode (`setenforce 0`) temporarily to diagnose, fix labels, then restore `Enforcing`. |
| Using `chcon` instead of `semanage fcontext` | Changes are ephemeral; running `restorecon` or package updates restores default labels and breaks service. | Use `semanage fcontext -a ...` followed by `restorecon -Rv`. |
| Binding Nginx to a custom port (e.g. 8080) without updating SELinux | SELinux `http_port_t` policy restricts Nginx from binding to unapproved ports; fails with `Permission denied`. | Add port to policy: `sudo semanage port -a -t http_port_t -p tcp 8080`. |
| Forgetting regex quotes in `semanage fcontext` (`/srv/data(/.*)?`) | Shell interprets parentheses as subshell syntax or glob expansion, corrupting the command. | Always enclose path regex in quotes: `"/srv/data(/.*)?"`. |
| Blindly executing `audit2allow -a -M mypolicy` | Authorizes all recent denials, potentially white-listing active exploit activity. | Inspect `.te` file before installing `.pp` module. |
| Forgetting to reload AppArmor profile after editing (`/etc/apparmor.d/`) | In-memory kernel profile remains unchanged until parsed. | Run `sudo apparmor_parser -r /etc/apparmor.d/<profile>`. |

---

## Performance, Security & Cost Implications

### Performance
- **Microsecond In-Kernel Caching**: Thanks to the Access Vector Cache (AVC), the performance overhead of SELinux in modern Linux kernels is **less than 1%–2%** on typical web and database workloads.
- **AppArmor Path Lookup**: AppArmor evaluates dentry path names during `open()` calls. Once file descriptors are established, read/write I/O operations execute at zero performance penalty.

### Security
- **Mitigating Zero-Day Vulnerabilities**: MAC provides containment. In famous vulnerabilities like the Apache Struts RCE or Shellshock, attackers who gained remote command execution inside the web server process were prevented from reading `/etc/shadow`, modifying `/etc/hosts`, or opening outbound reverse shells because the `httpd_t` domain blocked those operations.

### Cost
- Enterprise cybersecurity insurance policies and major compliance frameworks (FedRAMP, SOC 2, ISO 27001) mandate Mandatory Access Control. Running servers with SELinux or AppArmor in Enforcing mode is often a mandatory contractual prerequisite.

---

## Comparison Table / Alternatives

| Feature / Dimension | SELinux | AppArmor |
| :--- | :--- | :--- |
| **Enforcement Mechanism** | Label-Based Type Enforcement (Inodes / Extended Attributes) | Path-Based (Filesystem paths) |
| **Complexity / Granularity**| High (Fine-grained, MLS/MCS, RBAC, Roles) | Moderate (Human-readable profiles) |
| **Default Distribution** | RHEL, Rocky Linux, Fedora, CentOS, Amazon Linux | Ubuntu, Debian, openSUSE, SUSE Enterprise |
| **File Renaming Handling** | Context stays on inode when moved | Must update path rule in profile |
| **Network Port Filtering** | Built-in via `semanage port` | Limited; relies on firewall |
| **Tooling** | `semanage`, `restorecon`, `audit2allow` | `aa-status`, `aa-enforce`, `aa-logprof` |

---

## Real-World Troubleshooting & Interview Q&A

### Q1: How do you verify whether a service failure is caused by DAC (permissions) or MAC (SELinux / AppArmor)?
<details>
<summary>View Solution</summary>

1. **For SELinux**:
   - Temporarily switch to Permissive mode: `sudo setenforce 0`.
   - Test the application operation.
   - If the operation **now succeeds**, the issue is **100% SELinux**.
   - If it **still fails**, the issue is standard Linux permissions (`chmod`, `chown`, ACLs).
   - *Immediately return to Enforcing*: `sudo setenforce 1`.
2. **For AppArmor**:
   - Put the profile into Complain mode: `sudo aa-complain /usr/sbin/<service>`.
   - Test the application. If it succeeds, the AppArmor profile is missing path permissions.
</details>

### Q2: What is an AVC denial and where do you locate it on RHEL and Ubuntu?
<details>
<summary>View Solution</summary>

An **Access Vector Cache (AVC) denial** occurs when the Linux kernel security engine blocks an operation because no rule in the active policy permits it.
- **On RHEL / Rocky Linux**: Recorded by the audit daemon in `/var/log/audit/audit.log` (search with `ausearch -m avc -ts recent`).
- **On Ubuntu / Debian (AppArmor)**: Recorded in `/var/log/audit/audit.log` if auditd is installed, or in the kernel systemd journal: `journalctl -k | grep -i apparmor`.
</details>

### Q3: Why does copying a file with `cp` vs `mv` cause different SELinux behavior?
<details>
<summary>View Solution</summary>

- **`cp` (Copy)**: Creates a brand-new file (new inode) at the destination. The new file **inherits the default SELinux context** of the destination parent directory.
- **`mv` (Move)**: Simply updates the directory pointer; **the file retains its original inode and its original SELinux security context!**
  *Classic Incident*: A developer edits a configuration file in `/home/user/nginx.conf` (context: `user_home_t`) and runs `sudo mv /home/user/nginx.conf /etc/nginx/nginx.conf`. Nginx fails to start because the file still carries `user_home_t`. Running `restorecon -v /etc/nginx/nginx.conf` fixes it immediately.
</details>

### Q4: How do you allow a web server to bind to port 8443 on Rocky Linux?
<details>
<summary>View Solution</summary>

By default, SELinux only permits the `httpd_t` domain to bind to ports defined under the `http_port_t` type (80, 443, 8008, 8009, 8443 in newer policies).
If a custom port (e.g. 9443) is denied:
```bash
# Query currently permitted ports:
sudo semanage port -l | grep http_port_t

# Add custom port 9443 to http_port_t:
sudo semanage port -a -t http_port_t -p tcp 9443
```
</details>

### Q5: What does the `restorecon` command do?
<details>
<summary>View Solution</summary>

`restorecon` (Restore Context) reads the system's compiled SELinux policy database (which maps directory patterns like `/var/www/html(/.*)?` to expected types like `httpd_sys_content_t`) and resets the extended attributes (`security.selinux`) on disk to match the policy baseline.
- `-R`: Recursive.
- `-v`: Verbose (shows which files changed).
</details>

### Q6: How do you identify which SELinux boolean controls a specific capability?
<details>
<summary>View Solution</summary>

Use `semanage boolean -l` combined with `grep`:
```bash
# Find booleans related to databases and web servers
sudo semanage boolean -l | grep httpd | grep db
# Output displays: httpd_can_network_connect_db (off, off) Allow httpd to connect to database
```
Alternatively, search using `getsebool -a`.
</details>

### Q7: What is the difference between AppArmor's `enforce` and `complain` modes?
<details>
<summary>View Solution</summary>

- **Enforce Mode**: The kernel actively enforces the profile rules. Any unlisted system call or file access is blocked, and an audit message is logged.
- **Complain Mode**: The kernel does **not** block any operations. The application is allowed to execute unlisted actions, but every violation is logged as a "complain" audit event. This mode is used during staging to profile application behavior.
</details>

### Q8: What does `/.autorelabel` do during system boot?
<details>
<summary>View Solution</summary>

If the file `/.autorelabel` exists at the root of the filesystem during system boot, the `systemd-autorelabel` service pauses normal startup, takes the filesystem into maintenance mode, and runs `restorecon` across 100% of the filesystem inodes. 
Upon completion, it deletes `/.autorelabel` and reboots into normal multi-user mode with valid security contexts.
</details>

---

## Hands-On Practice Exercises

### Exercise 1: Querying Active MAC Subsystem and Status
**Task**: Check whether SELinux or AppArmor is active on your Linux distribution. Inspect its mode and loaded profiles.
<details>
<summary>View Solution</summary>

```bash
# On RHEL/Rocky:
if command -v sestatus &>/dev/null; then
    sestatus
fi

# On Ubuntu/Debian:
if command -v aa-status &>/dev/null; then
    sudo aa-status
fi
```
</details>

### Exercise 2: Inspecting Security Contexts of Files and Processes
**Task**: Use `ls -Z` and `ps -Z` to inspect the security labels of `/etc/passwd`, `/var/log`, and running processes.
<details>
<summary>View Solution</summary>

```bash
# Inspect file contexts
ls -lZ /etc/passwd /var/log

# Inspect process contexts
ps -efZ | head -n 15
```
</details>

### Exercise 3: Simulating the Move vs Copy SELinux Pitfall
**Task**: Create a test file in `/tmp`. Move it to `/var/www/html` (if on RHEL). Observe the retained context. Run `restorecon` to observe the correction.
<details>
<summary>View Solution</summary>

```bash
# Create file in /tmp
touch /tmp/test_page.html
ls -Z /tmp/test_page.html
# Context contains tmp_t

# Move to web directory
sudo mv /tmp/test_page.html /var/www/html/test_page.html 2>/dev/null || true
ls -Z /var/www/html/test_page.html 2>/dev/null || true
# Context still contains tmp_t!

# Fix with restorecon
sudo restorecon -v /var/www/html/test_page.html 2>/dev/null || true
# Restores to httpd_sys_content_t
sudo rm -f /var/www/html/test_page.html 2>/dev/null || true
```
</details>

### Exercise 4: Toggling and Testing AppArmor Modes
**Task**: On Ubuntu, view the status of the `ping` or `man` AppArmor profile. Switch it to Complain mode, then switch it back to Enforce mode.
<details>
<summary>View Solution</summary>

```bash
# Check profile exists
if command -v aa-status &>/dev/null; then
    # Switch to complain
    sudo aa-complain /usr/bin/man 2>/dev/null || true
    sudo aa-status | grep man

    # Switch back to enforce
    sudo aa-enforce /usr/bin/man 2>/dev/null || true
fi
```
</details>

### Exercise 5: Searching AVC Denials with `ausearch`
**Task**: Query the audit log for any AVC denials recorded in the last 24 hours.
<details>
<summary>View Solution</summary>

```bash
if command -v ausearch &>/dev/null; then
    sudo ausearch -m avc -ts today || echo "No AVC denials recorded today."
else
    echo "auditd ausearch not available."
fi
```
</details>

### Exercise 6: Inspecting Available SELinux Booleans
**Task**: List all SELinux booleans related to SSH or FTP.
<details>
<summary>View Solution</summary>

```bash
if command -v getsebool &>/dev/null; then
    getsebool -a | grep -E "ssh|ftp"
fi
```
</details>

---

## Summary Cheat Sheet & Further Reading

### Command Reference
| Goal | SELinux (RHEL/Rocky) | AppArmor (Ubuntu/Debian) |
| :--- | :--- | :--- |
| **Check Status** | `sestatus` or `getenforce` | `sudo aa-status` |
| **Set Permissive/Complain**| `sudo setenforce 0` | `sudo aa-complain <profile>` |
| **Set Enforcing** | `sudo setenforce 1` | `sudo aa-enforce <profile>` |
| **Inspect File Context** | `ls -laZ <path>` | N/A (path-based rules in `/etc/apparmor.d/`)|
| **Reset File Context** | `sudo restorecon -Rv <path>` | `sudo apparmor_parser -r <profile>` |
| **Persistent Context Map**| `sudo semanage fcontext -a -t <type> "regex"` | Edit `/etc/apparmor.d/<profile>` |
| **Inspect Denials** | `sudo ausearch -m avc -ts recent` | `sudo journalctl -k \| grep apparmor` |
| **Generate Policy Module**| `sudo audit2allow -M <mod> -i <log>` | `sudo aa-logprof` |

### Further Reading
- `man 8 selinux` and `man 8 semanage`
- `man 8 restorecon` and `man 8 audit2allow`
- `man 8 apparmor` and `man 8 aa-status`
- [Red Hat Enterprise Linux 9: Using SELinux](https://access.redhat.com/documentation/en-us/red_hat_enterprise_linux/9/html/using_selinux/)
- [Ubuntu Server AppArmor Documentation](https://ubuntu.com/server/docs/security-apparmor)
