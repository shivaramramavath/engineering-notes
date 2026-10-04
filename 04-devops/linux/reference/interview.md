# Linux Interview Questions

Questions grouped by topic, simple to hard within each group, with answers short enough to say out loud. Each section links to the note that explains it properly. For DevOps, backend, and SRE roles the usual pattern is: a few concept questions, then a **scenario** ("the site is down, walk me through it") where *how you think* matters more than any single command.

How to use this: cover the answer, say yours aloud, then compare. The follow-ups in italics are what interviewers typically ask next.

---

## 1. Fundamentals
→ [Getting started](../01-fundamentals/01-getting-started.md), [Filesystem](../01-fundamentals/02-filesystem-and-navigation.md)

**What is Linux? How is it different from a distribution?**
Linux is the kernel: it manages CPU, memory, devices, and processes. A distribution bundles the kernel with userland tools (shell, coreutils, libc, systemd), a package manager, and defaults. Ubuntu, Debian, and RHEL share the same kernel family but differ in packaging and defaults. *Follow-up: name the distro families and their package managers (Debian: apt/dpkg, Red Hat: dnf/rpm, Alpine: apk with musl).*

**What does "everything is a file" mean?**
Regular files, directories, devices (`/dev/sda`), sockets, pipes, and even kernel and process state (`/proc`, `/sys`) are exposed through the filesystem, so the same tools (`cat`, `ls`, `grep`) work on them. It's why debugging is so file-centric, e.g. reading `/proc/<pid>/status`.

**Where do configs, logs, and programs live?**
Config in `/etc`, logs and changing data in `/var` (`/var/log`, `/var/lib`), programs in `/usr` (your own installs in `/usr/local` or `/opt`), user data in `/home`, scratch in `/tmp`, and virtual kernel views in `/proc` and `/sys`. On modern Debian/Ubuntu `/bin` is a symlink to `/usr/bin`.

**Terminal vs shell?**
The terminal is the window/program that displays text; the shell (bash) is the interpreter running inside it that reads commands, expands them, and starts programs. Closing the terminal typically sends SIGHUP to the shell and its jobs.

**Absolute vs relative path? What do `.`, `..`, `~`, and `cd -` mean?**
Absolute starts at `/` and means the same from anywhere; relative starts at the current directory. `.` current, `..` parent, `~` home (expanded by the shell), `cd -` previous directory.

---

## 2. Files, links, and permissions
→ [Working with files](../01-fundamentals/03-working-with-files.md), [Permissions](../01-fundamentals/05-permissions.md)

**Hard link vs symbolic link?**
A hard link is another name for the *same inode*, so the data survives deleting the original name, but it can't cross filesystems or link directories. A symlink is a small file holding a *path*; it works across filesystems and for directories but dangles if the target is removed. *Follow-up: what's an inode? The structure storing metadata (owner, mode, times, pointers to data); the filename lives in the directory entry.*

**What does `chmod 755 file` mean? And 644, 600?**
755 = `rwxr-xr-x` (owner full; group and others read/execute), typical for programs and directories. 644 = `rw-r--r--` for normal files. 600 = owner-only, used for secrets such as SSH private keys.

**What do r, w, x mean on a directory?**
`r` lists names, `w` creates/deletes/renames entries, `x` lets you enter it and access items by name. You need `x` on every directory in a path to reach a file.

**Who can delete a file?**
Deleting needs `w` (and `x`) on the **directory**, not on the file. Hence the sticky bit on `/tmp` (`drwxrwxrwt`): everyone can write there but only a file's owner (or the directory owner/root) can delete it.

**What are setuid, setgid, and sticky?**
Setuid (4) on an executable runs it with the owner's privileges (e.g. `passwd`). Setgid (2) on an executable runs with the group's; on a *directory* it makes new files inherit the directory's group, useful for shared project folders. Sticky (1) on a directory restricts deletion to owners. Setuid is ignored for shell scripts and setuid binaries are a classic privilege-escalation target.

**What is umask?**
A mask of bits removed from default permissions (666 for files, 777 for directories). `umask 022` yields 644 files and 755 directories; `002` gives 664/775; `077` gives private 600/700.

**Why is `chmod -R 755 dir` usually wrong?**
It makes every *file* executable too. Use `chmod -R u=rwX,go=rX dir` (capital `X` sets execute only on directories), or `find -type d` and `find -type f` separately.

**Someone says "I have the right permissions but still get permission denied." Why?**
Check the whole path (`namei -l`: a directory missing `x`), the *effective* user of the process (`ps -o user`), group membership requiring a new login, ACLs (`getfacl`, a `+` in `ls -l`), mount options (`ro`, `noexec`), and MAC layers (SELinux/AppArmor).

**Which file-deletion gotcha involves disk space?**
Deleting a file that a process still has open doesn't free the space until the process closes it. `lsof +L1` finds them. Truncate (`: > file`) or rotate logs instead of `rm`.

---

## 3. Users and sudo
→ [Users and groups](../01-fundamentals/04-users-and-groups.md)

**What is UID 0? What are system accounts?**
UID 0 is root, which the kernel treats as all-powerful (the *name* is only a convention). System/service accounts (UIDs below 1000 on Debian/Ubuntu) run daemons, usually with a `nologin` shell and no password.

**`su` vs `sudo`?**
`su` switches user and needs the *target's* password. `sudo` runs a command with elevated rights using *your* password, is configured per user/command in `sudoers`, and logs each use, which gives an audit trail. `su -` / `sudo -i` give a login shell with the target's environment.

**Where are users and passwords stored?**
`/etc/passwd` (world-readable account info; shell, home, UID/GID), `/etc/shadow` (password hashes, root only), `/etc/group`. The `x` in the password field of `/etc/passwd` means the hash is in `shadow`.

**What's the danger in `usermod -G docker alice`?**
Without `-a` it *replaces* all supplementary groups, potentially removing `sudo`. Use `usermod -aG`. Also, group changes need a new login to take effect.

**Why does `sudo echo hi > /root/file` fail?**
The redirection is performed by *your* shell before `sudo` runs. Use `echo hi | sudo tee /root/file`.

**Why is the `docker` group dangerous?**
Anyone who can talk to the Docker daemon can start a container mounting the host filesystem, which is effectively root on the host.

---

## 4. Shell and scripting
→ [Shell basics](../02-shell/01-shell-basics.md), [Bash scripting](../02-shell/03-bash-scripting.md)

**What does `cmd > out 2>&1` do, and does order matter?**
It sends stdout to `out`, then points stderr at wherever stdout currently goes (the file). Order matters: `cmd 2>&1 > out` sends stderr to the terminal and only stdout to the file.

**Single vs double quotes?**
Single quotes are fully literal. Double quotes allow `$var`, `$(cmd)`, and backslash escapes but prevent word splitting and globbing. Rule: quote every variable expansion (`"$var"`).

**What is an exit code? How do `&&` and `||` use it?**
Every command returns a number; 0 means success, non-zero failure. `a && b` runs `b` only if `a` succeeded; `a || b` only if it failed. `$?` holds the last code. Typical codes: 126 not executable, 127 not found, 130 Ctrl+C, 137 SIGKILL, 143 SIGTERM.

**What does `set -euo pipefail` do? Any caveats?**
`-e` exit on failure, `-u` error on unset variables, `pipefail` fail a pipeline if any stage fails. Caveats: `-e` doesn't trigger inside `if`, `&&`/`||` lists, or `!`; `grep` with no match returns 1; `local x=$(cmd)` masks `cmd`'s failure (declare, then assign). Pair with explicit checks for critical steps.

**`[ ]` vs `[[ ]]`? bash vs sh?**
`[[ ]]` is a bash keyword: no word splitting, supports `&&`, `=~`, and pattern matching. `[ ]` is the POSIX `test` command and needs strict quoting. On Debian/Ubuntu `/bin/sh` is `dash`, which lacks bash features, so scripts using them need a bash shebang.

**How do you make a script clean up after itself?**
`trap cleanup EXIT` with `mktemp -d` for temp files. EXIT fires on normal exit, errors under `set -e`, and catchable signals. SIGKILL can't be trapped.

**Why does a cron job work in my terminal but not in cron?**
Cron has a minimal environment: tiny `PATH`, no profile or aliases, `/bin/sh`, no TTY. Use absolute paths, set `PATH`, redirect output, and remember `%` must be escaped in crontab lines.

**What does `$@` vs `$*` do? Why quote `"$@"`?**
`"$@"` expands to each argument as a separate word, preserving spaces inside arguments. `$*` and unquoted `$@` re-split on whitespace.

---

## 5. Text processing
→ [Text processing](../02-shell/02-text-processing.md)

**Why must you `sort` before `uniq`?**
`uniq` only collapses *adjacent* duplicates. Idiom for counting: `sort | uniq -c | sort -nr`.

**`cut` vs `awk`?**
`cut` splits on a single literal delimiter and treats repeated spaces as separate empty fields. `awk` splits on runs of whitespace by default and can do conditions and arithmetic, so it's better for aligned output like `ps` or `df`.

**Why use `find -print0 | xargs -0`?**
Filenames can contain spaces and newlines; NUL-separated names are the only fully safe delimiter. (`find -exec ... {} +` achieves the same.)

**How do you edit a file in place safely with `sed`?**
Run without `-i` first to preview, then `sed -i.bak ...` to keep a backup. Note GNU and BSD/macOS `sed -i` differ.

**How do you parse JSON in a shell script?**
Use `jq`, not regex: `jq -r '.items[] | select(.status=="failed") | .id'`. `-r` outputs raw strings.

---

## 6. Processes and signals
→ [Processes and signals](../03-system/01-processes-and-signals.md)

**What happens when you run a command?**
The shell `fork()`s a child, then the child `exec()`s the program, replacing itself. The parent waits (foreground) or continues (background) and collects the exit status.

**Zombie vs orphan?**
A zombie has exited but its parent hasn't reaped its exit status; it uses only a process-table entry and can't be killed. Fix the parent. An orphan is a running process whose parent died; it's adopted by PID 1.

**SIGTERM vs SIGKILL?**
SIGTERM (15) politely asks a process to stop and can be handled, so it can flush and clean up. SIGKILL (9) can't be caught or ignored and gives no chance to clean up. Try TERM first, wait, then KILL. *Follow-up: SIGHUP is "terminal closed", often reused to reload daemon config.*

**What does a process in `D` state mean?**
Uninterruptible sleep, usually waiting on disk or network storage. It can't be killed (even by `-9`) until the I/O returns; many of them point to a failing disk or hung NFS mount.

**How do you keep a job running after you log out?**
`nohup`/`disown` help but are fragile. Use `tmux` for interactive work and a systemd service for anything permanent, since only that restarts it and captures logs.

**Exit code 137 on a process. What does it mean?**
128 + 9: it was killed by SIGKILL, often by the OOM killer (check `dmesg -T | grep -i oom`) or `docker kill`/a stop timeout.

**Process vs thread?**
Threads inside a process share the same address space and file descriptors; each has its own stack and ID. Processes have separate address spaces. Linux schedules both as "tasks".

**What does the load average mean?**
The average number of tasks that are runnable *or* in uninterruptible sleep over 1/5/15 minutes. Compare to `nproc`. High load with low CPU usually means processes blocked on I/O.

---

## 7. systemd and scheduling
→ [systemd](../03-system/03-systemd-and-services.md), [Cron and timers](../03-system/04-cron-and-timers.md)

**`enable` vs `start`?**
`start` runs the unit now; `enable` makes it start at boot. They're independent; `enable --now` does both. "Works until I reboot" usually means it was started but not enabled.

**How do you run a Node app as a service?**
A unit with `User=` (non-root), `WorkingDirectory=`, `EnvironmentFile=`, an absolute `ExecStart=/usr/bin/node server.js`, `Restart=on-failure`, `WantedBy=multi-user.target`, then `daemon-reload` and `enable --now`. The app must run in the foreground and log to stdout (captured by journald).

**Why does `ExecStart=/bin/echo hi > /tmp/x` not work?**
`ExecStart` doesn't run through a shell, so redirection, pipes, `&&`, and `$HOME` aren't interpreted. Wrap in `/bin/sh -c '...'` if needed.

**What happens on `systemctl stop`?**
SIGTERM to the main process, then SIGKILL after `TimeoutStopSec` (default ~90s) if it hasn't exited. Services should handle SIGTERM.

**Where do unit files go, and how do you modify a packaged one?**
Packages ship to `/usr/lib/systemd/system`; yours (and overrides) go in `/etc/systemd/system`. Use `systemctl edit unit` to create a drop-in override instead of editing the packaged file. Always `daemon-reload` after changes.

**How do you read a service's logs?**
`journalctl -u name -f`, `-n 100`, `--since "1 hour ago"`, `-p err`, `-b` for this boot.

**Cron vs systemd timers?**
Cron is simple and universal but minimal-environment, with output lost unless redirected. Timers log to the journal, support `Persistent=true` (run missed jobs after downtime), dependencies, and service sandboxing, and a still-running `oneshot` isn't started twice. Pick one per host and be consistent.

**A cron job overlaps with its previous run. Fix?**
`flock -n /var/lock/job.lock command` so a second instance exits immediately.

---

## 8. Packages
→ [Package management](../03-system/02-package-management.md)

**`apt update` vs `apt upgrade`?**
`update` refreshes the package *index*; `upgrade` installs newer versions of installed packages. Without `update`, you install stale versions or see "Unable to locate package".

**How do you add a third-party apt repository safely?**
Download its signing key into `/etc/apt/keyrings/`, reference it with `signed-by=` in the repo entry, `apt update`. Avoid deprecated `apt-key`, which trusts a key for every repo.

**How do you keep a package from upgrading?**
`apt-mark hold pkg`. For databases, pin and test upgrades before applying.

**"Could not get lock /var/lib/dpkg/lock-frontend."**
Another apt/dpkg process (often unattended-upgrades) is running. Wait; don't delete the lock unless you've confirmed nothing is running. If an install was interrupted: `dpkg --configure -a` and `apt --fix-broken install`.

**How do you patch servers automatically?**
`unattended-upgrades` (security updates by default) on Debian/Ubuntu, `dnf-automatic` on Red Hat. Track pending reboots (`/var/run/reboot-required`) and use `needrestart` for services using updated libraries.

---

## 9. Storage
→ [Storage and filesystems](../03-system/05-storage-and-filesystems.md)

**`df` says the disk is full but `du` doesn't add up. Why?**
Likely a deleted file still held open by a process (`lsof +L1`), data hidden under a mount point, reserved blocks (ext4 keeps ~5% for root), or `du` excluding other filesystems.

**"No space left on device" but `df -h` shows free space?**
Inode exhaustion (`df -i`), typically millions of tiny files. Find the directory, clean up, and fix the producer.

**How do you add and persist a new disk?**
`lsblk` to identify it, partition, `mkfs`, `mount`, get the UUID with `blkid`, add an `/etc/fstab` line with the UUID and `nofail`, then run `mount -a` to test before rebooting. Use UUIDs, not `/dev/sdX` names, which can change.

**What happens if `/etc/fstab` has a bad entry?**
Boot can drop into emergency mode. Prevent it with `nofail` for non-critical mounts and test with `mount -a` / `findmnt --verify`.

**What is LVM, and how do you grow a volume online?**
PV → VG → LV: physical disks pooled into a volume group, carved into logical volumes. Grow with `lvextend -r -L +10G /dev/vg/lv` (`-r` resizes the filesystem too). XFS can grow but not shrink; ext4 shrinks only offline.

**ext4 vs XFS?**
Both are mature journaling filesystems. ext4 is the Debian/Ubuntu default and is flexible; XFS is the Red Hat default and excels at large files and parallel I/O but can't be shrunk.

---

## 10. Networking
→ [Networking basics](../04-networking/01-networking-basics.md)

**What happens when you run `curl https://example.com`?**
DNS resolves the name, the route/gateway is chosen, a TCP three-way handshake opens a connection to port 443, TLS negotiates certificates and keys, then the HTTP request and response flow. Each step has a failure signature.

**"Connection refused" vs "connection timed out"?**
Refused: the host answered but nothing is listening (or a REJECT rule). Timed out: packets are being dropped (firewall, cloud security group, routing) or the host is unreachable.

**`127.0.0.1` vs `0.0.0.0`?**
A service bound to `127.0.0.1` is reachable only from the same machine; `0.0.0.0` listens on all IPv4 interfaces. "Works locally, not remotely" is often a loopback bind or a firewall.

**How do you see what's listening on a port and which process?**
`sudo ss -tlnp` (or `ss -tlnp 'sport = :3000'`), or `lsof -i :3000`. `ss` replaces `netstat`; `ip` replaces `ifconfig`.

**`dig` vs `getent hosts`?**
`dig` queries a DNS server directly and ignores `/etc/hosts`. `getent hosts` uses the system resolver order (including `/etc/hosts`), which is what applications see.

**What is "DNS propagation"?**
Cache expiry: resolvers keep answers until the TTL runs out. Lower the TTL before a planned change.

**Why can't a normal user bind port 80?**
Ports below 1024 are privileged: root or `CAP_NET_BIND_SERVICE` is needed. Common solutions: reverse proxy on 80/443 with the app on a high port.

**Ping fails. Is the host down?**
Not necessarily; ICMP is often blocked. Test the real port (`nc -zv host 443`, `curl -v`).

---

## 11. SSH
→ [SSH](../04-networking/02-ssh.md)

**How does SSH key authentication work?**
The client has a private key, the server has the matching public key in `~/.ssh/authorized_keys`. The server challenges the client, which proves possession of the private key by signing data; the key itself is never sent. Separately, the client verifies the *server's* host key against `known_hosts`.

**"REMOTE HOST IDENTIFICATION HAS CHANGED!"**
The host key differs from the saved one: either a legitimate rebuild or a man-in-the-middle. Verify out-of-band, then `ssh-keygen -R host`. Don't disable host key checking globally.

**"Permission denied (publickey)": how do you debug?**
`ssh -vvv` on the client to see which keys are offered; on the server, `journalctl -u ssh` for the reason. Common causes: wrong user or key, public key missing from `authorized_keys`, wrong permissions (`~/.ssh` 700, `authorized_keys` and private keys 600, home not group-writable), too many keys offered (use `IdentitiesOnly yes`).

**Explain `ssh -L 5433:localhost:5432 host`.**
Opens port 5433 on your machine and forwards it through the SSH connection to `localhost:5432` *as seen from the server*. You can `psql -p 5433 localhost` to reach a database that only listens on the server's loopback. `-R` is the reverse; `-D` creates a SOCKS proxy.

**How do you reach a private server through a bastion?**
`ssh -J bastion user@internal` or `ProxyJump bastion` in `~/.ssh/config`. Prefer it over agent forwarding (`-A`).

**How do you harden SSH without locking yourself out?**
Make sure key login works first; set `PermitRootLogin no`, `PasswordAuthentication no`, `AllowUsers` in an early-named drop-in file (sshd uses the first value it reads); `sshd -t`, verify with `sshd -T`, reload, and test from a **second** session before closing the first.

---

## 12. Firewall and security
→ [Firewall](../04-networking/03-firewall.md), [Hardening](../05-production/01-server-hardening.md)

**How should a host firewall be configured?**
Default deny inbound, default allow outbound, allow only needed ports (restrict sensitive ones by source IP/subnet), allow established/related replies. With ufw: allow SSH **before** `enable`.

**Host firewall vs cloud security group?**
Two separate layers; traffic must pass both. "Port open in ufw but still times out" is often the cloud layer.

**Why does a Docker-published port ignore ufw rules?**
Docker inserts its own iptables/nftables rules for published ports that are evaluated before ufw's. Bind to loopback (`-p 127.0.0.1:8080:80`), put a reverse proxy in front, or use the `DOCKER-USER` chain. Verify from outside the host.

**DROP vs REJECT?**
DROP silently discards (client sees a timeout); REJECT answers with an error (client sees "refused").

**What would you do to harden a new server?**
Non-root admin with sudo; SSH keys only, root login off; default-deny firewall; remove unneeded services and bind databases to loopback/private IPs; automatic security updates; dedicated service users with sandboxing; strict permissions on secrets; AppArmor/SELinux enforcing; fail2ban (optional); log review, time sync, and tested off-machine backups.

**Does fail2ban secure SSH?**
It reduces brute-force noise and load but isn't the real protection; disabling passwords and using keys is. Set `ignoreip` so you can't ban yourself.

**SELinux vs AppArmor? Why not `setenforce 0`?**
Both are mandatory access control on top of normal permissions. AppArmor confines programs by path (Ubuntu/Debian); SELinux uses labels on everything (RHEL family). Permissive mode is for *diagnosing* only; fix the label, boolean, or profile and return to enforcing. Typical SELinux fix: `semanage fcontext` + `restorecon`.

---

## 13. Performance
→ [Performance](../05-production/03-performance.md)

**Where do you start when a server is slow?**
Identify the bottleneck resource: `uptime`/`vmstat 1` (CPU, run queue, swap, I/O wait), `free -h`, `iostat -xz 1`, `ss -s`/`ip -s link`, then drill to the process (`top`, `pidstat`, `iotop`).

**`free` shows almost no free memory. Is that a problem?**
Not by itself. Linux uses spare RAM as page cache and releases it on demand. Check `available`, swap *activity* (`si`/`so`), and OOM messages.

**Is swap use bad?**
Some swap use is normal (idle pages). Continuous swap in/out means thrashing. Disabling swap doesn't make things faster; it just turns memory exhaustion into an immediate OOM kill.

**High load average but low CPU?**
Load includes tasks in uninterruptible I/O sleep. Look for `D` processes and check disk (`iostat`), NFS, or a saturated cloud disk.

**What do `wa` and `st` mean in `top`?**
`wa` is CPU time waiting on I/O; `st` (steal) is time the hypervisor gave your vCPU to another VM, which you can't fix from inside.

**"Too many open files": how do you fix it?**
Check the real limit in `/proc/<pid>/limits` and the current count in `/proc/<pid>/fd`. For a systemd service set `LimitNOFILE=`; `limits.conf` only applies to login sessions. If the count keeps growing, you have a descriptor leak.

**What is the OOM killer and how do you recognize it?**
When memory is exhausted the kernel kills a process (chosen by `oom_score`). Look for "Out of memory: Killed process" in `dmesg`/`journalctl -k` and exit code 137. A cgroup/container limit can trigger it even when the host has free memory.

**How do you tune the kernel?**
Measure first, change one `sysctl` at a time, persist in `/etc/sysctl.d/*.conf` with `sysctl --system`, document why. Defaults are reasonable; don't paste tuning lists you don't understand.

---

## 14. Containers and internals
→ [Namespaces and cgroups](../06-advanced/01-namespaces-and-cgroups.md)

**Container vs VM?**
A VM runs its own kernel on virtualized hardware. A container is a regular process (or processes) on the *host* kernel, given isolation by namespaces and limits by cgroups, so it's lighter but shares the kernel's attack surface.

**What are namespaces? Name some.**
Per-process views of global resources: `pid`, `net`, `mnt`, `uts` (hostname), `ipc`, `user`, `cgroup`. They control what a process can *see*.

**What are cgroups?**
Hierarchical groups that **limit and account** resources (CPU, memory, I/O, pids). They control what a process can *use*. cgroup v2 exposes files like `memory.max` and `cpu.max`.

**What happens when a container exceeds its memory limit? Its CPU limit?**
Memory: the kernel OOM-kills a process in that cgroup (exit 137, `OOMKilled: true`). CPU: the group is throttled, so it gets slow rather than dead. No limit means no protection for the host.

**Why might a container ignore `docker stop`?**
Its main process is PID 1, which doesn't get default signal actions; without a SIGTERM handler it ignores the signal until SIGKILL after the timeout. Handle SIGTERM, use exec-form `CMD`, or `docker run --init`.

**Is root in a container root on the host?**
By default, yes (UID 0), unless user namespaces/rootless mode are used. Capabilities, seccomp, and AppArmor/SELinux limit it, and `--privileged` removes most of those protections.

**How do you debug a container that has no tools?**
From the host: find its PID (`docker inspect -f '{{.State.Pid}}'`) and use `nsenter -t PID -n ss -tlnp`, `strace -p`, `lsof -p`, or `tcpdump` inside its network namespace.

---

## 15. Debugging tools
→ [Advanced debugging](../06-advanced/02-advanced-debugging.md)

**When do you reach for `strace`?**
When logs don't explain a hang or error: it shows system calls and errno (`ENOENT`, `EACCES`, `ECONNREFUSED`). Use `-f` to follow threads/children, `-e trace=...` to filter, `-y` to see what an fd is, `-c` for a summary. It slows the target, so be careful in production.

**`lsof` uses?**
Who owns a port (`-i :PORT`), who holds a file or mount, deleted-but-open files (`+L1`), and descriptor leaks (`-p PID`).

**How do you read a `tcpdump` handshake problem?**
SYN with no reply: packets dropped upstream or by a firewall. SYN followed by RST: nothing listening. SYN-ACK but no final ACK: return-path issue. Handshake fine then long gap: the app is slow.

**CPU is pinned in one process. What now?**
`top -H` to find the hot thread, then `perf top -p PID` or `perf record -F 99 -g` for a profile/flame graph. For Node use `--perf-basic-prof`; in VMs fall back to `-e cpu-clock`.

---

## 16. Scenarios (talk through them)

For each, say your *first three checks*, what each result would imply, and what you'd change last. Interviewers listen for method: **symptom → what changed → evidence → one hypothesis → verify**.

**"The website is down."**
1. Reproduce and classify: `curl -v` from outside; timeout vs refused vs 502/504 vs DNS vs TLS error.
2. On the server: is the service up (`systemctl status`, `journalctl -u`)? listening on the right address (`ss -tlnp`)? responds locally (`curl localhost:PORT`)?
3. If it works locally but not remotely: firewall (host and cloud), bind address, security group. If nginx returns 502: upstream down or wrong port; 504: slow upstream.
4. Ask what changed (deploy, config, updates, cert expiry, disk full). Fix, verify, then find root cause.

**"Disk is full."**
`df -h` (space), `df -i` (inodes), `lsof +L1` (deleted-open files); `du -xh --max-depth=1` to find the culprit (logs, Docker, uploads, core dumps); free space safely (vacuum journal, prune Docker, rotate/truncate logs); then fix the cause (logrotate, retention) and add an alert.

**"The server is slow."**
`uptime`, `vmstat 1`: CPU saturated, swapping, I/O wait, or steal? Then the specific tool: `top`/`pidstat` (CPU), `free -h` + swap activity + OOM logs (memory), `iostat -xz` + `iotop` (disk), `ss -s`/`ip -s link` (network). Check recent deploys and traffic; fix the app issue before resizing hardware.

**"I can't SSH in."**
Timeout → network/firewall/security group/wrong IP (check from another network, use the provider console). Refused → sshd down or wrong port. `Permission denied (publickey)` → `ssh -vvv`, key/user, permissions, `authorized_keys`, and server-side `journalctl -u ssh`. Use the console to fix config; `sshd -t` before reloading.

**"The service starts manually but fails under systemd" / "doesn't come back after reboot."**
Manual vs service differences: user, environment, working directory, `PATH`. Check `systemctl status` exit code (`203/200/217`), `journalctl -u`, run as the service user with `env -i`. After reboot: is it `enabled`? Does it depend on something not yet ready (`After=`/`Wants=network-online.target`, a mount with a bad `fstab`)?

**"A process keeps dying."**
Exit code: `137` → OOM or SIGKILL (`dmesg`, cgroup limit); `143` → SIGTERM from systemd/Docker/an operator; otherwise check the app's own log. Look at memory growth over time, `Restart=` loops (`start-limit-hit`), and resource limits.

**"Permission denied even though permissions look right."**
`id`, effective user of the process, `namei -l path`, `getfacl`, mount options, group membership needing re-login, SELinux/AppArmor logs (`ausearch -m AVC`, `journalctl -k | grep apparmor`).

**"A cron job isn't running."**
Is the daemon running and the entry loaded (`crontab -l`, `/etc/cron.d`)? Filename dots in `/etc/cron.daily`, unescaped `%`, minimal `PATH`, output not captured, wrong time zone, `run-parts` naming. Check `journalctl -u cron` and the job's own log; reproduce with `env -i`.

---

## 17. Hands-on tasks

Be ready to write these without looking:

```bash
# 1. Top 10 IPs in an access log
awk '{print $1}' access.log | sort | uniq -c | sort -nr | head

# 2. Files over 100 MB under /var, largest first
sudo find /var -xdev -type f -size +100M -exec ls -lh {} + | sort -k5 -h -r | head

# 3. Which process uses port 3000? Stop it gracefully.
sudo ss -tlnp 'sport = :3000'; kill <PID>

# 4. Count lines containing "ERROR" in all .log files, per file
grep -c ERROR *.log

# 5. Replace a string across files, with backup
grep -rl "old" conf/ | xargs sed -i.bak 's/old/new/g'

# 6. Make a directory shared by group "dev"
sudo mkdir /srv/project && sudo chown root:dev /srv/project && sudo chmod 2775 /srv/project

# 7. Give a user sudo, create a service account with no login
sudo usermod -aG sudo alice
sudo useradd --system --no-create-home --shell /usr/sbin/nologin myapp

# 8. Check a TCP port is open without nc
timeout 3 bash -c '</dev/tcp/host/443' && echo open

# 9. Back up a directory with a dry run first
rsync -an --delete /srv/app/ /backup/app/ && rsync -a --delete /srv/app/ /backup/app/

# 10. Tail errors for one service from the last hour
journalctl -u myapp --since "1 hour ago" -p err --no-pager
```

---

## Common traps in interviews

- **Naming a command without saying what the output would mean.** Say what you expect and what each result implies.
- **Jumping to `kill -9`, `chmod 777`, disabling the firewall, or `setenforce 0`.** Interviewers read these as red flags.
- **Restarting before gathering evidence.**
- **Not asking what changed or when it started.**
- **Treating `free` low as OOM, ping failure as "down", or any swap use as a problem.**
- **Forgetting the second layer** (cloud firewall, Docker iptables rules, SELinux, cgroup limits).
- **Skipping the root cause** after the quick fix.

## Quick Summary

- Know the model: files/permissions, users/sudo, processes/signals, systemd, storage, networking layers.
- Know the **distinctions** interviewers love: hard vs soft link, TERM vs KILL, refused vs timeout, enable vs start, `df` vs `du`, `available` vs `free`, namespaces vs cgroups.
- Practice the scenarios out loud with a fixed method, and keep [the cheatsheet](./cheatsheet.md) open for command syntax.
- Rebuild the broken-server scenarios in [the troubleshooting lab](../07-projects/03-troubleshooting-lab/README.md) until the diagnosis is automatic.

**Related:** [Cheatsheet](./cheatsheet.md) · [Projects](../07-projects/README.md)