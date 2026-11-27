# Module 08: Linux Technical Interview Preparation & Scenario Q&A

[<- Back to Linux Track Root](../README.md) | [<- Previous Module: 07-projects](../07-projects/README.md) | [Next Module: 09-cheatsheets ->](../09-cheatsheets/README.md)

---

## 1. Overview & Scope

Technical interviews for Systems Engineering, Site Reliability Engineering (SRE), Cloud Operations, and DevOps positions rigorously probe an engineer's mental model of operating system behavior. Modern engineering organizations rarely ask trivia questions; they evaluate candidates on distributed debugging scenarios, failure domain isolation, kernel trade-offs, and root-cause analysis under pressure.

Module 08 organizes 150+ comprehensive interview questions into eight specialized domains. Every question provides an in-depth answer detailing the underlying mechanism, practical troubleshooting commands, common candidate pitfalls, and follow-up depth questions.

---

## 2. Interview Domain Catalog

| # | Domain Note | Target Role Level | Questions | Primary Focus |
| :---: | :--- | :---: | :---: | :--- |
| **01** | [Linux Basics & Architecture Q&A](./01-linux-basics-qa.md) | Junior - Mid | 20 | Monolithic kernel vs microkernel, Ring 0 vs Ring 3, system call dispatch, POSIX, distros, boot sequence. |
| **02** | [Filesystems, Inodes & Permissions Q&A](./02-filesystem-and-permissions-qa.md) | Mid - Senior | 20 | Inode mechanics, hard links vs soft links, deleted open files (`df` vs `du`), POSIX ACLs, sticky bit, directory permissions. |
| **03** | [Shell, Pipes & Text Processing Q&A](./03-shell-and-text-processing-qa.md) | Mid - Senior | 20 | File descriptors, subshells, process substitution vs pipes, word splitting, regex engines, `sed`/`awk` streaming. |
| **04** | [Processes, Signals & Systemd Q&A](./04-processes-and-systemd-qa.md) | Mid - Senior | 20 | Process states (`D` state unkillable sleep), zombie vs orphan processes, SIGTERM vs SIGKILL, systemd unit dependencies, D-Bus. |
| **05** | [Linux Networking & Protocol Diagnostics Q&A](./05-networking-qa.md) | Senior - Staff | 20 | TCP 3-way handshake, TIME_WAIT socket states, Netfilter packet path, DNS resolution bottlenecks, `ss` vs `netstat`, MTU black holes. |
| **06** | [Performance Bottlenecks & Troubleshooting Scenarios](./06-performance-and-troubleshooting-scenarios.md) | Senior - Staff | 20 | CPU load average vs CPU utilization, I/O wait, OOM killer scoring, disk latency triage, Brendan Gregg's USE Method. |
| **07** | [Security, Hardening & Mandatory Access Control Q&A](./07-security-qa.md) | Senior - Staff | 20 | AppArmor vs SELinux contexts, Linux capabilities, seccomp filtering, SSH hardening, sudoers security vulnerabilities. |
| **08** | [System Design & Behavioral Project Discussions](./08-behavioural-and-project-discussion.md) | Senior - Principal | 15 | Designing high-availability Linux infrastructure, zero-downtime OS migration, post-mortem blameless culture, war stories. |

---

## 3. Preparation Strategy

To excel in technical interviews:
1. **Explain the Why, Not Just the Flag**: When asked how to inspect network sockets, state *why* `ss -tulpn` is superior to `netstat` (direct kernel Netlink socket structure queries vs slow sequential `/proc/net/` text file parsing).
2. **Apply Structured Methodologies**: When presented with "The server is slow", apply the **USE Method** (Utilization, Saturation, Errors across CPU, Memory, Disk, Network) rather than guessing.
3. **Trace System Boundaries**: Trace problems across the boundary between user space libraries and kernel space system calls using `strace` and `lsof`.
