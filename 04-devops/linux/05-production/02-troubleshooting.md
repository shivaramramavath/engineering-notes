# Troubleshooting

Good troubleshooting is a method more than a bag of commands. Most failures on a Linux server fall into a few recurring patterns: a port that's taken, a disk that's full, a permission that doesn't match, a service that won't start, a process that got killed. This note is organized by **symptom**: what you see, then what to check, in order.

Prerequisites: pretty much everything before it, especially [systemd and Services](../03-system/03-systemd-and-services.md), [Networking Basics](../04-networking/01-networking-basics.md), and [Storage](../03-system/05-storage-and-filesystems.md). For practice, the [troubleshooting lab](../07-projects/03-troubleshooting-lab/README.md) has broken-server scenarios.

## Method

1. **Define the symptom precisely.** "Site is down" is vague; "`curl localhost:3000` gets connection refused since 14:10" is workable.
2. **Ask what changed.** Deploys, config edits, updates, disk growth, expired certificates, traffic spikes.
3. **Read the logs and the exact error message** before touching anything.
4. **Form one hypothesis, test it, change one thing at a time.** Multiple simultaneous changes destroy your ability to know what fixed it.
5. **Verify the fix, then find the root cause** (why did the disk fill? why did it crash?), and write it down.

Collect evidence **before** restarting. A restart often clears the very state (memory, open files, connections) that explains the problem.

### What changed?

```bash
last -x | head                                # logins, reboots, shutdowns
journalctl --list-boots | tail -3             # when did it last reboot?
grep -E "Start-Date|Commandline|Upgrade" /var/log/apt/history.log | tail -20   # recent package changes (Debian/Ubuntu)
sudo dnf history | head                       # same on Red Hat family
ls -lt /etc | head                            # recently modified config
```

### The first 60 seconds on a sick server

```bash
uptime                       # load averages
dmesg -T | tail -20          # kernel messages: OOM kills, disk errors
journalctl -p err -n 30 --no-pager   # recent errors
free -h                      # memory (look at "available")
df -h; df -i                 # space and inodes
vmstat 1 5                   # CPU, swap, I/O at a glance
ss -s                        # connection summary
top -b -n1 | head -15        # biggest consumers
```

This gives you a map; then follow the symptom below. Interpretation of load, memory, and I/O numbers is in [Performance](./03-performance.md).

## Where logs live

| What | Where |
|---|---|
| Everything systemd manages | `journalctl -u name` (add `-f`, `--since`, `-p err`) |
| Kernel (OOM, disk, drivers) | `dmesg -T`, `journalctl -k` |
| General system | `/var/log/syslog` (Debian/Ubuntu), `/var/log/messages` (Red Hat) |
| Logins, sudo, SSH | `/var/log/auth.log` (Debian/Ubuntu), `/var/log/secure` (Red Hat) |
| nginx | `/var/log/nginx/access.log`, `error.log` |
| Package changes | `/var/log/apt/`, `dnf history` |

Handy habits:

```bash
sudo tail -f /var/log/nginx/error.log /var/log/syslog   # follow several at once
journalctl -u myapp --since "10 min ago" -p warning
sudo grep -i -E "error|fatal|denied" /var/log/syslog | tail
```

Logs grow forever unless **logrotate** manages them (`/etc/logrotate.d/`). Test a config with `sudo logrotate -d /etc/logrotate.d/myapp` (dry-run). Apps that hold their log file open need a `postrotate` reload or `copytruncate`, or they keep writing to the rotated (or deleted) file.

## Symptom: "Address already in use" (EADDRINUSE)

The app can't start because something already holds the port.

```bash
sudo ss -tlnp 'sport = :3000'        # who owns the port?
sudo lsof -i :3000                   # alternative
```

Then decide:

- It's a **leftover instance** (an old `node` from `nohup` or tmux, or a crash-looping duplicate): stop it properly. If it's a systemd service, `systemctl stop`; otherwise `kill PID` ([signals](../03-system/01-processes-and-signals.md)).
- It's a **different service** that legitimately uses the port: change your app's port.
- **IPv4/IPv6 confusion:** one process on `0.0.0.0:3000` and another on `[::]:3000` can conflict.
- Port **under 1024** and error is `EACCES` (permission denied) rather than in-use: needs root or `CAP_NET_BIND_SERVICE`; better to run on a high port behind nginx.

`TIME_WAIT` sockets on a *client* connection don't stop a *server* from listening; don't chase them here.

## Symptom: "No space left on device"

Work through the three causes in order ([storage note](../03-system/05-storage-and-filesystems.md) explains why):

```bash
df -h                 # 1. out of SPACE?
df -i                 # 2. out of INODES? (IUse% near 100)
sudo lsof +L1         # 3. deleted files still held open by a process
```

Find what's big:

```bash
sudo du -xh --max-depth=1 / 2>/dev/null | sort -h | tail
sudo du -xh --max-depth=1 /var 2>/dev/null | sort -h | tail
```

Common culprits and safe cleanups:

| Culprit | Check / fix |
|---|---|
| Logs | `journalctl --disk-usage`, `sudo journalctl --vacuum-time=7d`; fix logrotate; **truncate** (`: > file.log`) rather than `rm` a log an app still writes |
| Docker | `docker system df`; `docker system prune` (read what it removes first) |
| Package cache / old kernels | `sudo apt clean`, `sudo apt autoremove` (read the list) |
| App data / uploads / core dumps | `du` the app directory; check `/var/crash`, `/var/lib/systemd/coredump` |
| Deleted-but-open files | restart the owning process; the space returns |

After freeing space, find out **why** it filled, or it'll be back.

## Symptom: "Permission denied"

Work from the path outward ([permissions note](../01-fundamentals/05-permissions.md)):

```bash
id                                    # who am I? which groups?
ps -o user,group,cmd -p <pid>         # for a service: who is IT actually running as?
namei -l /var/www/site/index.html     # perms of every directory in the path (a missing x shows up here)
ls -ld /path/to/dir
getfacl /path/to/file                 # if ls shows a trailing "+"
findmnt -no OPTIONS /path             # is the filesystem ro or noexec?
```

If it all looks right and access still fails:

- **Group membership not applied** until a new login.
- **SELinux/AppArmor**: check `sudo ausearch -m AVC -ts recent` or `journalctl -k | grep -i apparmor` ([hardening](./01-server-hardening.md)).
- **`ProtectHome`/`ProtectSystem`** in a systemd unit hiding the path.
- **`noexec`** mount when "Permission denied" occurs on *running* a script.
- **Script not executable** or has a bad shebang (see below).

## Symptom: service fails to start or keeps restarting

```bash
systemctl status myapp --no-pager -l
journalctl -u myapp -n 50 --no-pager
```

Read the exit status: `203/EXEC` (bad `ExecStart`), `200/CHDIR` (bad `WorkingDirectory`), `217/USER` (user doesn't exist), `1` or other (the app itself crashed; read its output). `start-limit-hit` means it crashed too fast too often; fix the cause, then `systemctl reset-failed myapp`.

Reproduce as the service would run:

```bash
sudo -u myapp env -i /usr/bin/node /opt/myapp/server.js
```

Differences between "works in my shell" and "fails as a service" are almost always **user, environment variables, working directory, or `PATH`**. Full table in [systemd and Services](../03-system/03-systemd-and-services.md).

## Symptom: can't connect to a service

Use the ladder from [networking basics](../04-networking/01-networking-basics.md): interface → route → IP reachability → DNS → port → listener → app.

The deciding question is **refused vs timed out**:

- **Connection refused**: host reachable, nothing listening (or a REJECT). On the server: `ss -tlnp`. Check the bind address (`127.0.0.1` vs `0.0.0.0`).
- **Timed out**: packets dropped. Check host firewall (`ufw status verbose`), the **cloud security group**, and routing. `tcpdump` shows whether packets even arrive ([firewall](../04-networking/03-firewall.md)).

Behind nginx: a `502 Bad Gateway` means nginx couldn't reach or got a bad answer from the upstream app (check the app is up and the upstream port matches); `504` means the upstream was too slow.

## Symptom: "command not found" or script won't run

```bash
type -a mytool          # is it in PATH? alias? function?
echo $PATH
hash -r                 # clear bash's command location cache after installing/moving things
echo $?                 # 127 = command not found, 126 = found but not executable
```

- A script in the current directory needs `./script.sh`.
- Works in your shell but not in **cron or systemd**: they have a minimal `PATH` and no profile. Use absolute paths ([cron](../03-system/04-cron-and-timers.md)).
- `bash: ./script.sh: /bin/bash^M: bad interpreter` means the file has **Windows line endings** (CRLF). Fix with `sed -i 's/\r$//' script.sh` (or `dos2unix`), and configure your editor/git to use LF.
- `Permission denied` on a script: missing execute bit (`chmod +x`) or a `noexec` mount.

## Symptom: process killed, exit code 137, or random crashes

```bash
dmesg -T | grep -i -E "out of memory|killed process"
journalctl -k | grep -i oom
free -h
```

If the **OOM killer** shows up, the machine (or a container/cgroup memory limit) ran out of memory and the kernel killed the biggest offender. Find what's using memory (`ps aux --sort=-%mem | head`), look for leaks (rising RSS over time), and set sane limits. Details in [Performance](./03-performance.md).

## Symptom: "Too many open files"

The process hit its file-descriptor limit (`EMFILE`). Often seen as failed accepts or socket errors under load.

```bash
cat /proc/<pid>/limits | grep -i 'open files'
ls /proc/<pid>/fd | wc -l                   # how many it has open now
```

Raise it where it applies (`LimitNOFILE=` in the systemd unit; see [Performance](./03-performance.md)), **and** check for a descriptor leak if the count keeps climbing.

## Symptom: slow, high load, unresponsive

Identify the bottleneck resource first: CPU (`us`/`sy`), memory pressure and swapping (`si`/`so`), disk (`wa`, `iostat`), or network. A server with high load but idle CPU is usually stuck on I/O (processes in `D` state). Walk through it in [Performance](./03-performance.md). If you can't even log in because the box is swamped, use the provider's console and be patient; each command is slow.

## Symptom: weird failures that "make no sense"

- **Clock is wrong**: TLS errors ("certificate not yet valid/expired"), failed token checks, confusing logs. Check `timedatectl`.
- **DNS**: `getent hosts name` vs `dig name`, `/etc/hosts` overrides, stale caches ([networking](../04-networking/01-networking-basics.md)).
- **Environment differences** between interactive shell, cron, systemd, and Docker.
- **A full disk or exhausted inodes** produces unrelated-looking errors in many programs.
- **Certificates expired** (`openssl x509 -noout -enddate -in cert.pem`, or `echo | openssl s_client -connect host:443 2>/dev/null | openssl x509 -noout -dates`).

## When you're locked out or the box won't boot

- Use the provider's **web/serial console** or rescue mode.
- A bad `/etc/fstab` entry drops boot into emergency mode; fix the line (or add `nofail`) and reboot ([storage](../03-system/05-storage-and-filesystems.md)).
- SSH lockout from a config or firewall change: console, revert, `sshd -t`, reload.
- Check `journalctl -b -1 -p err` (previous boot) if the journal is persistent.

For deeper inspection when logs aren't enough (syscalls, open files, packets), see [Advanced Debugging](../06-advanced/02-advanced-debugging.md).

## Common mistakes

- **Restarting first**, destroying the evidence.
- **Changing several things at once**, so you can't tell what worked.
- **Not reading the whole error message** or the lines just *above* it in the log.
- **`rm` on a live log file** to "free space" (the space isn't freed; truncate or rotate instead).
- **`kill -9` as the first move.**
- **Disabling the firewall or SELinux "to test"** and leaving it that way.
- **Fixing the symptom only** (clearing the disk) without finding the cause (runaway log, leaked temp files).
- **Assuming** (it's DNS, it's the network) instead of measuring with the commands above.
- **Not writing down** what was wrong and how it was fixed.

## Quick Summary

- Method: precise symptom → what changed → logs → one hypothesis → one change → verify → root cause.
- Gather evidence before restarting; start with the 60-second checks (`uptime`, `dmesg`, `journalctl -p err`, `free`, `df -h/-i`, `vmstat`).
- Port in use: `ss -tlnp 'sport = :N'`. Disk full: check space, **inodes**, and **deleted-open files**.
- Permission denied: `id`, `namei -l`, effective service user, then ACLs, mount options, SELinux/AppArmor.
- Service won't start: `systemctl status` + `journalctl -u` + exit code; reproduce as the service user.
- Refused = no listener; timeout = dropped by a firewall or route; 137 = killed (often OOM).

**Next:** [Performance](./03-performance.md)
