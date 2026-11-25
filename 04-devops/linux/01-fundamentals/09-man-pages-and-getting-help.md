<!--
================================================================================
Metadata:
  Title: Man Pages, Info Pages & Terminal Documentation Mastery
  Track: Linux & Systems Engineering / DevOps
  Difficulty: Beginner to Intermediate
  Estimated Time: 45 minutes
  Prerequisites: 01-fundamentals/01-getting-started.md
  Key Terminology: man sections, apropos, whatis, mandb, info, help, tldr, POSIX manuals
================================================================================
-->

# Man Pages, Info Pages & Terminal Documentation Mastery

[<- 08-sudo-and-privilege.md](./08-sudo-and-privilege.md) | [Back to Module Overview](./README.md) | [10-environment-variables-and-startup-files.md ->](./10-environment-variables-and-startup-files.md)

---

## Table of Contents
- [1. What & Why](#1-what--why)
- [2. Core Concepts](#2-core-concepts)
- [3. How It Works Internally](#3-how-it-works-internally)
- [4. Syntax & Usage](#4-syntax--usage)
- [5. Practical Examples](#5-practical-examples)
- [6. Best Practices](#6-best-practices)
- [7. Common Mistakes & Troubleshooting](#7-common-mistakes--troubleshooting)
- [8. Performance, Security & Cost Implications](#8-performance-security--cost-implications)
- [9. Comparison Table](#9-comparison-table)
- [10. Real-World Interview Questions](#10-real-world-interview-questions)
- [11. Practice Exercises](#11-practice-exercises)
- [12. Summary & Quick Reference Cheat Sheet](#12-summary--quick-reference-cheat-sheet)
- [13. Further Reading & Official Documentation](#13-further-reading--official-documentation)

---

## 1. What & Why

In air-gapped production data centers, secure bastion hosts, or isolated edge environments, engineers frequently operate without internet connectivity. Search engines, Stack Overflow, and AI chatbots are inaccessible. The definitive, legally binding authority on system behavior is the machine's local documentation stack: **man pages**, **GNU info**, and **shell built-in help**.

Mastering terminal documentation is not merely about looking up CLI flags:
- It reveals the internal C signatures and error codes (`errno`) of kernel system calls.
- It defines the exact schema and file syntax for critical daemon configurations (e.g., `man 5 crontab`, `man 5 sshd_config`).
- It teaches engineers to disambiguate between command-line utilities and kernel system calls that share identical names (such as the `read` command vs. the `read(2)` syscall).

---

## 2. Core Concepts

### The 8 Standard Manual Sections
The Unix manual is strictly divided into eight standardized numbered sections:

```
+--------------------------------------------------------------------------+
| THE 8 STANDARD MANUAL SECTIONS                                           |
|                                                                          |
| Section 1: User Commands (General executable programs)                   |
|            Examples: ls, grep, curl, tar, git, find                      |
|                                                                          |
| Section 2: System Calls (Functions provided by the Linux Kernel)         |
|            Examples: open, read, write, fork, clone, epoll_create        |
|                                                                          |
| Section 3: C Library Functions (glibc runtime library calls)             |
|            Examples: printf, malloc, fopen, qsort, pthread_create        |
|                                                                          |
| Section 4: Special Files & Devices (Interfaces in /dev)                  |
|            Examples: null, zero, random, urandom, tty, loop              |
|                                                                          |
| Section 5: File Formats & Conventions (Configuration file schemas)       |
|            Examples: passwd, shadow, fstab, crontab, systemd.unit        |
|                                                                          |
| Section 6: Games & Amusements (Screensavers, games)                      |
|            Examples: fortune, cowsay, factor                             |
|                                                                          |
| Section 7: Miscellaneous, Conventions & Standards                        |
|            Examples: hier (FHS hierarchy), ip, tcp, regex, signal        |
|                                                                          |
| Section 8: System Administration Commands & Daemons                      |
|            Examples: useradd, iptables, fdisk, systemd, sshd, nginx      |
+--------------------------------------------------------------------------+
```

### Disambiguation: Why Section Numbers Matter
Many concepts in Linux span multiple manual sections. If you run `man passwd` without specifying a section, `man` returns the first match it finds in numerical order (Section 1: the `/usr/bin/passwd` command).

To view the specification of the `/etc/passwd` file format, you must explicitly query **Section 5**:
```bash
man 5 passwd
```

Similarly:
- `man 1 read`: The bash shell builtin command.
- `man 2 read`: The Linux kernel system call.
- `man 3 read`: The standard C library wrapper.

---

## 3. How It Works Internally

### Storage, Formatting & Indexing Architecture
Man pages are stored as compressed `gzip` archives containing `troff`/`groff` formatting markup inside `/usr/share/man/`:

```
/usr/share/man/
├── man1/ (e.g. ls.1.gz, grep.1.gz)
├── man2/ (e.g. open.2.gz, read.2.gz)
├── man5/ (e.g. passwd.5.gz, fstab.5.gz)
└── man8/ (e.g. useradd.8.gz, iptables.8.gz)
```

When you invoke `man <name>`:
1. `man` consults `/etc/manpath.config` to locate manual directory trees.
2. It decompresses the `.gz` file into memory.
3. It pipes the formatting markup through the **`groff`** text processing engine to render formatted text with bolding and underlines.
4. It displays the formatted output inside the system pager (typically `less`).

### The Index Database: `mandb`
Commands like `apropos` and `whatis` perform regex lookups against a precompiled binary index database (`/var/cache/man/index.db`). This database is maintained by the **`mandb`** daemon and cron timers.

---

## 4. Syntax & Usage

### Basic and Section-Targeted Lookups
```bash
# Query manual using default section search order (1 -> 8)
man tar

# Query a specific manual section: man <section> <name>
man 5 fstab          # File format of /etc/fstab
man 2 epoll_create   # Kernel system call documentation
man 8 systemd        # System administration daemon manual

# List all available sections for a given keyword
man -a read
# (Displays each matching manual in sequence; press 'q' to proceed to next)

# Search for commands by keyword description (equivalent to 'apropos')
man -k "firewall"

# Display one-line command summary (equivalent to 'whatis')
man -f passwd
```

### Built-in Shell Help vs. Binary Flags
Not all commands are external binaries found in `/usr/bin/`. Many everyday commands (`cd`, `echo`, `read`, `export`, `type`, `history`) are **Bash built-in commands**:

```bash
# Verify if a command is a binary executable or shell builtin
type cd
# Output: cd is a shell builtin

type ls
# Output: ls is aliased to `ls --color=auto' (or /usr/bin/ls)

# Built-in commands do NOT have dedicated standalone man pages; use 'help':
help cd
help export
help read

# Query command-line flags on standard binaries:
curl --help
tar --help
```

---

## 5. Practical Examples

### Example 1: Finding Network Diagnostic Commands via `apropos`
```bash
# Problem: You need a command to inspect open listening TCP ports,
# but do not remember the modern replacement for netstat.

# Search the manual index for keywords:
apropos "socket statistics"
# Output:
# ss (8) - another utility to investigate sockets

# Search for packet filtering:
apropos "packet filter"
# Output:
# nft (8) - Administration tool for packet filtering and classification
# iptables (8) - administration tool for IPv4 packet filtering and NAT
```

### Example 2: Inspecting System Call Error Numbers (`errno`)
```bash
# Look up kernel system call failure behavior for openat:
man 2 openat

# Inside the pager, search directly for the ERRORS section by typing:
# /^ERRORS
# You can view exact conditions producing:
# EACCES: Search permission denied for one of the directories in path
# EEXIST: Path already exists and O_CREAT | O_EXCL was used
# ENOENT: A directory component in pathname does not exist
```

### Example 3: Rebuilding the Manual Search Index
```bash
# If apropos returns "nothing appropriate" after installing new packages:
sudo mandb

# Sample Output:
# Processing manual pages under /usr/share/man...
# Updating index cache for /usr/share/man/man1...
# 165 man subdirectories contained newer manual pages.
# 2824 manual pages were added.
```

### Example 4: Exporting Man Pages to Plain Text or PDF for Archiving
```bash
# Convert a man page into clean, unformatted plain text (strips escape codes)
man 5 systemd.service | col -b > /tmp/systemd_service_spec.txt

# Render a man page as a formatted PostScript / PDF document (requires groff)
man -t nginx | ps2pdf - /tmp/nginx_manual.pdf
```

### Example 5: Installing and Using `tldr` for Instant Examples
```bash
# In production, engineers often need quick, copy-pasteable examples rather than 40-page specifications
# Install lightweight community tldr client:
sudo apt install -y tldr

# Update offline cache:
tldr --update

# Query practical everyday examples for tar:
tldr tar

# Sample output:
# - Create an archive from files:
#   tar -czvf target.tar.gz /path/to/file1 /path/to/file2
# - Extract an archive into a target directory:
#   tar -xzvf archive.tar.gz -C /target/dir
```

---

## 6. Best Practices

- **Check Section 5 First for Config Files**: When configuring daemons (sshd, nginx, cron, systemd), do not read the Section 1 or 8 server daemon manual. Jump directly to Section 5: `man 5 sshd_config`, `man 5 crontab`, `man 5 systemd.service`.
- **Use `type -a <cmd>` Before Reading Docs**: Before debugging unexpected behavior, verify whether you are invoking a shell alias, a bash function, a shell builtin, or a physical binary.
- **Master In-Pager Navigation in `less`**:
  - `/pattern`: Search forward for `pattern`.
  - `?pattern`: Search backward.
  - `n`: Jump to next match; `N`: Jump to previous match.
  - `g`: Jump to top of manual; `G`: Jump to bottom.
  - `q`: Quit.
- **Install Development Manuals on Lab Servers**: By default, minimal cloud images strip man pages to save disk. On developer machines, install `man-db` and kernel manual pages:
  `sudo apt install -y man-db manpages manpages-dev`.
- **Prefer `help` for Shell Builtins**: Running `man cd` either fails or loads the generic 300-page bash manual. `help cd` prints exact syntax in 20 lines.

---

## 7. Common Mistakes & Troubleshooting

| Mistake / Issue | Why It Fails | Practical Remedy |
| :--- | :--- | :--- |
| `man: nothing appropriate` when running `apropos` | The manual index database has not been compiled or is corrupted. | Rebuild the index database: `sudo mandb`. |
| `No manual entry for cd` | `cd` is a shell builtin internal to Bash, not an external binary executable. | Use the built-in help system: `help cd`. |
| Running `man passwd` when trying to configure `/etc/passwd` | Default lookup order selects Section 1 (the binary) first. | Explicitly target Section 5: `man 5 passwd`. |
| Minimal Docker container returns `man: command not found` | Container base images (Ubuntu/Alpine) strip documentation to minimize image size. | Use `unminimize` on Ubuntu cloud images or install `man-db`. |
| Searching for exact phrases fails in `apropos` | `apropos` treats space-separated words as logical OR by default. | Quote the search string or use regex: `apropos --and "ssh" "key"`. |
| Bolding and formatting characters appear as garbled characters (e.g. `ESC[1m`) | Pager is configured without ANSI escape interpretation. | Set `export PAGER="less -R"` in your shell profile. |

---

## 8. Performance, Security & Cost Implications

- **Performance**: Man pages are compressed with gzip on disk. Reading a man page incurs a single instantaneous decompression into memory; resource overhead is negligible (< 2 MB RAM).
- **Security**: Operating in high-security, air-gapped classified enclaves mandates reliance on local documentation. Downloading unvetted scripts or guides from external internet repositories introduces supply-chain risks. Local man pages represent the exact, verified code state of the locally installed package version.
- **Cost**: Stripping man pages from Docker container base images saves approximately 30–60 MB per container layer. While appropriate for production container images, developer workstations should retain full manual pages.

---

## 9. Comparison Table

| Documentation Tool | Scope | Storage Mechanism | Best Used For |
| :--- | :--- | :--- | :--- |
| **`man`** | Exhaustive reference manuals (Sections 1–8) | Compressed troff/groff files in `/usr/share/man/` | Formal specifications, config schemas, syscalls |
| **`help`** | Bash internal shell builtins | Embedded directly inside Bash binary | Shell builtins (`cd`, `read`, `export`, `test`) |
| **`--help` / `-h`** | Quick binary flag overview | Embedded in application binary compiled code | Quick CLI argument and flag verification |
| **`info`** | Hyperlinked, multi-chapter manuals | Texinfo format in `/usr/share/info/` | GNU core utilities (deep architectural essays) |
| **`tldr`** | Community-driven concise examples | Local Markdown cache | Immediate copy-pasteable real-world examples |

---

## 10. Real-World Interview Questions

<details>
<summary><strong>1. Explain the eight standard sections of the Linux manual. Why are they structured this way?</strong></summary>

**Answer**:
The eight sections are:
1. User Commands (CLI programs like `ls`, `grep`).
2. System Calls (Kernel APIs like `read`, `fork`).
3. C Library Functions (glibc APIs like `printf`, `malloc`).
4. Special Files & Devices (hardware nodes in `/dev`).
5. File Formats & Conventions (configuration schemas like `/etc/fstab`, `crontab`).
6. Games & Amusements.
7. Miscellaneous & Standards (protocols, regex, FHS hierarchy).
8. System Administration Commands (daemons and root tools like `systemd`, `iptables`).

They are structured this way to disambiguate identical names across different abstraction layers. For example, `exit` exists as a shell builtin (help exit), a C library function (man 3 exit), and a kernel system call (man 2 exit). The numerical classification allows developers and administrators to immediately target the exact abstraction level they are working on.
</details>

<details>
<summary><strong>2. What is the difference between an external executable binary and a shell builtin? How do you view documentation for each?</strong></summary>

**Answer**:
- **External Executable Binary**: A compiled ELF file or script located on the filesystem (e.g., `/usr/bin/tar`, `/bin/ls`). When executed, the shell forks a child process and calls `execve()`. Documentation is accessed via `man <command>` or `<command> --help`.
- **Shell Builtin**: A command implemented directly inside the shell process's code (e.g., `cd`, `export`, `alias`, `read`, `source`). Because changing directory or setting environment variables must alter the shell process itself (a child process cannot alter its parent's working directory or environment), these commands cannot be external binaries. Documentation is accessed via `help <command>`.
</details>

<details>
<summary><strong>3. How does `apropos` function internally? What should you do if it returns no results?</strong></summary>

**Answer**:
`apropos` (which is functionally equivalent to `man -k`) performs a regular expression keyword search across the "short description" lines of all manual pages on the system.

Instead of scanning thousands of individual `.gz` files on disk during every query, it queries a centralized, precompiled database (`/var/cache/man/index.db`). If `apropos` returns no results or fails to locate recently installed packages, the index database is either uninitialized, out of date, or corrupted. To remediate, run `sudo mandb` to scan the filesystem and rebuild the database.
</details>

<details>
<summary><strong>4. How do you find the configuration file format for a service like crontab or sshd without guessing?</strong></summary>

**Answer**:
Target **Section 5** (File Formats and Conventions) of the manual:
- For crontab schedule syntax: `man 5 crontab`
- For OpenSSH server configuration: `man 5 sshd_config`
- For systemd unit file directives: `man 5 systemd.service`
- For filesystem mount table syntax: `man 5 fstab`

Section 5 documents exact directive names, valid value types, default behaviors, and configuration examples.
</details>

<details>
<summary><strong>5. What is the difference between GNU Info pages and traditional Man pages?</strong></summary>

**Answer**:
- **Man Pages**: Traditional Unix documentation standard. Flat, monolithic documents designed for quick reference. They lack native hyperlinking between topics.
- **GNU Info Pages**: Designed by the Free Software Foundation (FSF) for the GNU Project. Info documents are hierarchical, multi-chapter books using Texinfo format. They feature node-based cross-references, menus, and hyperlinks, providing deep educational tutorials and complete software architectural overviews (e.g., `info coreutils` or `info gcc`).
</details>

<details>
<summary><strong>6. How do you search forward and backward inside a man page using the default pager?</strong></summary>

**Answer**:
Man pages display using `less` by default:
- Forward Search: Press `/`, enter the search string or regex, and press `Enter`.
- Backward Search: Press `?`, enter the search string, and press `Enter`.
- Next Match: Press `n`.
- Previous Match: Press `N`.
- Case-Insensitive Toggle: Press `-i` inside `less` or run `less -I`.
</details>

<details>
<summary><strong>7. Why do cloud minimal container images lack man pages, and how can an engineer restore them?</strong></summary>

**Answer**:
Container images (e.g., `ubuntu:latest` or `alpine:latest`) are engineered for minimal disk footprint and fast network pull times. Package manager configurations in these images include exclusion filters (such as `path-exclude=/usr/share/man/*` in `/etc/dpkg/dpkg.cfg.d/excludes`) that discard documentation files during package installation.

To restore documentation on Ubuntu:
1. Remove or comment out the exclusion rules in `/etc/dpkg/dpkg.cfg.d/`.
2. Run the Canonical `unminimize` script: `sudo unminimize` (reinstalls standard interactive packages and documentation).
3. Alternatively, install manual tools manually: `sudo apt install -y man-db manpages`.
</details>

<details>
<summary><strong>8. What information is documented in Section 7 of the manual?</strong></summary>

**Answer**:
Section 7 documents **Conventions, Standards, and Miscellaneous Topics**. It does not document specific software commands or C functions, but rather system-wide architectural standards:
- `man 7 hier`: The official Filesystem Hierarchy Standard layout.
- `man 7 ip` / `man 7 tcp`: Linux networking stack protocols, socket options, and TCP sysctl parameters.
- `man 7 signal`: Complete list of Unix signals (`SIGKILL`, `SIGTERM`, `SIGCHLD`) and standard handler behaviors.
- `man 7 regex`: POSIX regular expression syntax specification.
</details>

---

## 11. Practice Exercises

<details>
<summary><strong>Exercise 1 (Easy): Differentiate Command Types with `type`</strong></summary>

**Task**: Inspect the command type for `cd`, `ls`, `grep`, `pwd`, and `if`. Identify which are builtins, aliases, keywords, or disk binaries.

**Solution**:
```bash
for cmd in cd ls grep pwd if; do
    type -a "$cmd"
done
```
</details>

<details>
<summary><strong>Exercise 2 (Easy): Target Specific Manual Sections</strong></summary>

**Task**: Display the manual for the `stat` command-line utility, then display the manual for the `stat` kernel system call.

**Solution**:
```bash
# Command line utility (Section 1)
man 1 stat

# Kernel system call (Section 2)
man 2 stat
```
</details>

<details>
<summary><strong>Exercise 3 (Medium): Keyword Search Across Unindexed System</strong></summary>

**Task**: Use `apropos` to locate all manual entries related to "process scheduling". If it fails, rebuild the database with `mandb` and re-run.

**Solution**:
```bash
# Attempt search
apropos "process scheduling" || (sudo mandb && apropos "process scheduling")
```
</details>

<details>
<summary><strong>Exercise 4 (Medium): Extract Section 5 Config Directives to a Reference File</strong></summary>

**Task**: Extract the manual for `/etc/hosts` (Section 5) into a clean, unformatted ASCII text file inside `/tmp`, stripping all bold/underline backspace control characters.

**Solution**:
```bash
man 5 hosts | col -b > /tmp/hosts_manual.txt
head -n 20 /tmp/hosts_manual.txt
```
</details>

<details>
<summary><strong>Exercise 5 (Hard): Script an Interactive Terminal Cheatsheet Extractor</strong></summary>

**Task**: Author a bash function `quickhelp` that checks if a command is a builtin (calling `help`), or an external binary (checking if `tldr` is available, otherwise showing first 25 lines of `man`).

**Solution**:
```bash
quickhelp() {
    local cmd="$1"
    if type "$cmd" 2>/dev/null | grep -q "shell builtin"; then
        help "$cmd"
    elif command -v tldr >/dev/null 2>&1; then
        tldr "$cmd"
    else
        man "$cmd" | col -b | head -n 35
    fi
}

# Test:
quickhelp cd
quickhelp tar
```
</details>

<details>
<summary><strong>Exercise 6 (Hard): Audit Manual Coverage of Custom In-House Binaries</strong></summary>

**Task**: Write a bash script that iterates over all executable files in `/usr/local/bin` and flags any custom binary that lacks a corresponding man page in `/usr/local/share/man/man1/`.

**Solution**:
```bash
#!/usr/bin/env bash
set -euo pipefail

echo "Auditing /usr/local/bin for missing man pages..."
for binary in /usr/local/bin/*; do
    [ -x "$binary" ] || continue
    name=$(basename "$binary")
    if ! man -w "$name" >/dev/null 2>&1; then
        echo "DOCUMENTATION DEFECT: Executable '$name' has no man page installed!"
    fi
done
```
</details>

---

## 12. Summary & Quick Reference Cheat Sheet

| Task | Command | Example |
| :--- | :--- | :--- |
| **Read Manual** | `man <name>` | `man tar` |
| **Target Section** | `man <section> <name>` | `man 5 fstab` |
| **Search by Keyword** | `apropos <keyword>` | `apropos "firewall"` |
| **Search One-Liner** | `whatis <command>` | `whatis passwd` |
| **Shell Builtin Help** | `help <builtin>` | `help cd` |
| **Identify Type** | `type -a <cmd>` | `type -a ls` |
| **Rebuild Index** | `sudo mandb` | Updates `/var/cache/man/index.db` |
| **Clean Plain Text** | `man <name> \| col -b` | Strips control characters for grep/export |

---

## 13. Further Reading & Official Documentation

- [man 1 man: Interface to the System Reference Manuals](https://man7.org/linux/man-pages/man1/man.1.html)
- [man 7 man-pages: Conventions for Linux Documentation](https://man7.org/linux/man-pages/man7/man-pages.7.html)
- [GNU Texinfo and Info Manual](https://www.gnu.org/software/texinfo/manual/info-stnd/info-stnd.html)
- [Michael Kerrisk: The Linux Programming Interface (TLPI)](https://man7.org/tlpi/)
- [TLDR Pages Project](https://tldr.sh/)

---

[<- 08-sudo-and-privilege.md](./08-sudo-and-privilege.md) | [Back to Module Overview](./README.md) | [10-environment-variables-and-startup-files.md ->](./10-environment-variables-and-startup-files.md)
