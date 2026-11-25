# Linux Cheatsheet

A scan-first reference for the commands in this repo. Each section links to the full note; this page only has the syntax and the gotchas worth remembering. Examples assume Ubuntu/Debian unless noted.

## Files and navigation
→ [Filesystem](../01-fundamentals/02-filesystem-and-navigation.md), [Working with files](../01-fundamentals/03-working-with-files.md)

```bash
pwd; cd -; cd ~; cd ..             # where am I / previous dir / home / up
ls -lah                            # long, hidden, human sizes
ls -ld /path                       # the directory itself
tree -L 2                          # structure (apt install tree)
stat f; file f; type -a cmd        # metadata / kind / where is a command from

mkdir -p a/b/c
cp -a src/ dst/                    # copy keeping perms, times, links
mv -n old new                      # -n never overwrite
rm -r dir/                         # NO trash; ls the glob first
ln -s TARGET LINKNAME              # symlink (target first!)
readlink -f link                   # resolve to real path

less file    # /text search, n next, G end, F follow, q quit
tail -f log  # follow;   head -n 20 f;   wc -l f
```

| Path | What lives there |
|---|---|
| `/etc` | configuration |
| `/var/log`, `/var/lib` | logs, persistent app data |
| `/usr`, `/usr/local` | programs; your own installs |
| `/home`, `/root` | user homes |
| `/tmp` | scratch (often cleared at boot) |
| `/proc`, `/sys`, `/run` | virtual: kernel/process state |

## Find and archive

```bash
find . -name "*.log"                          # QUOTE the pattern
find . -type f -size +100M
find . -mtime -1                              # modified < 24h ago
find . -name "*.tmp" -delete                  # -delete goes LAST
find . -name "*.js" -exec grep -l TODO {} +
find . -print0 | xargs -0 cmd                 # safe with odd filenames

tar -czf out.tar.gz dir/      # create     tar -tzf out.tar.gz   # list
tar -xzf out.tar.gz -C /dest  # extract    (dest must exist)
gzip -k f   gunzip f.gz   zcat f.gz
```

## Shell essentials
→ [Shell basics](../02-shell/01-shell-basics.md)

| Syntax | Meaning |
|---|---|
| `"$var"` | expand, no word splitting (**quote variables**) |
| `'$var'` | fully literal |
| `$(cmd)` | command substitution |
| `cmd > f` / `>> f` | stdout to file (overwrite / append) |
| `cmd 2> f` | stderr to file |
| `cmd > f 2>&1` | both to file (**order matters**); bash: `&> f` |
| `cmd < f` | file as stdin |
| `a \| b` | pipe stdout of a to b (`\|&` includes stderr) |
| `a && b` / `a \|\| b` / `a; b` | on success / on failure / always |
| `cmd <<'EOF' … EOF` | here-document (quoted EOF = no expansion) |
| `x \| tee f` | write to file and stdout; `sudo tee` for root-owned files |

```bash
export VAR=value; VAR=x cmd        # env for children / one command
echo $?                            # last exit code (0 = success)
export PATH="$HOME/bin:$PATH"
{a,b}.txt   *.log   file?.txt      # brace expansion, globs (done by the shell)
Ctrl+R history search   !!   sudo !!   Ctrl+A/E start/end   Ctrl+W del word
```

Common exit codes: `1` general, `2` misuse, `126` not executable, `127` not found, `130` Ctrl+C, `137` SIGKILL/OOM, `143` SIGTERM.

## Text processing
→ [Text processing](../02-shell/02-text-processing.md)

```bash
grep -rn "text" dir/          # recursive, line numbers
grep -iv / -c / -l / -o / -E / -F / -C3 / -q
cut -d' ' -f1 f               # fields by delimiter
sort | uniq -c | sort -nr | head    # count and rank (uniq needs sorted input)
sort -h                       # human sizes     sort -k2,2 -t,
awk '{print $1}' f            awk -F: '{print $1,$3}' /etc/passwd
awk '{s+=$5} END{print s}' f  awk '$9==500' access.log
sed 's/old/new/g' f           sed -i.bak 's/old/new/g' f    # test without -i first
sed -n '10,20p' f             sed '/^#/d' f
tr -d '\r' < in > out         # strip CRLF
jq -r '.items[] | select(.ok|not) | .id' data.json
```

```bash
# top 10 client IPs
awk '{print $1}' access.log | sort | uniq -c | sort -nr | head
# strip comments and blanks from a config
grep -Ev '^\s*(#|$)' /etc/ssh/sshd_config
```

## Users, groups, sudo
→ [Users and groups](../01-fundamentals/04-users-and-groups.md)

```bash
id; id alice; groups; getent passwd alice
sudo adduser alice                       # Debian/Ubuntu friendly
sudo useradd -m -s /bin/bash alice       # portable
sudo useradd --system --no-create-home --shell /usr/sbin/nologin svc
sudo usermod -aG docker alice            # ALWAYS -a (without it groups are replaced)
sudo passwd -l alice                     # lock;   userdel -r alice  # delete + home
sudo -u user cmd;  sudo -i;  su - user
sudo visudo -f /etc/sudoers.d/name       # never edit sudoers directly
echo x | sudo tee /etc/file              # not: sudo echo x > file
```

New group membership applies after **re-login** (or `newgrp`). Files: `/etc/passwd` (users), `/etc/shadow` (hashes, root only), `/etc/group`.

## Permissions
→ [Permissions](../01-fundamentals/05-permissions.md)

```text
-rwxr-xr--   owner group others;   r=4 w=2 x=1
755 rwxr-xr-x  programs/dirs      644 rw-r--r--  files
600 rw-------  secrets/keys       700 rwx------  private dirs
```

| | File | Directory |
|---|---|---|
| `r` | read | list names |
| `w` | modify | create/delete/rename entries |
| `x` | execute | enter / access by name |

```bash
chmod 644 f;  chmod u+x f;  chmod -R u=rwX,go=rX dir/    # X = x only on dirs
chown user:group f;  chown -R www-data:www-data /var/www
umask          # 022 → files 644, dirs 755;  002 → 664/775;  077 → 600/700
chmod 2775 dir   # setgid dir: new files inherit group
chmod +t dir     # sticky: only owner can delete (like /tmp)
setfacl -m u:bob:r f;  getfacl f      # ACLs; "+" in ls -l means one exists
namei -l /full/path/to/file           # find the dir missing x
```

Delete/rename needs **`w` on the directory**, not the file. Setuid: `chmod 4755`; ignored on scripts.

## Processes and signals
→ [Processes and signals](../03-system/01-processes-and-signals.md)

```bash
ps aux;  ps -ef --forest;  pstree -p;  pgrep -af name
top / htop      # top: P cpu, M mem, 1 per-core, k kill, q quit
kill PID  →  kill -9 PID (last resort);  kill -HUP PID (reload);  kill -0 PID (exists?)
pkill -f pattern   # check with pgrep -af first
cmd &;  jobs;  fg %1;  bg %1;  Ctrl+Z;  nohup cmd > out 2>&1 &;  disown
nice -n 10 cmd;  renice -n 15 -p PID;  ionice -c3 cmd
cat /proc/PID/status;  ls -l /proc/PID/fd;  tr '\0' ' ' < /proc/PID/cmdline
```

| Signal | # | Notes |
|---|---|---|
| SIGHUP | 1 | terminal closed / daemon reload |
| SIGINT | 2 | Ctrl+C |
| SIGKILL | 9 | uncatchable, no cleanup |
| SIGTERM | 15 | polite stop (default) |
| SIGSTOP / SIGCONT | 19 / 18 | pause / resume |

States: `R` running, `S` sleeping, **`D`** uninterruptible I/O (can't kill), `T` stopped, **`Z`** zombie (fix the parent).

## systemd, logs, scheduling
→ [systemd](../03-system/03-systemd-and-services.md), [Cron and timers](../03-system/04-cron-and-timers.md)

```bash
systemctl status|start|stop|restart|reload svc
systemctl enable --now svc;  systemctl disable svc        # enabled = at boot, active = now
systemctl daemon-reload                                   # after editing ANY unit
systemctl list-units --failed;  systemctl cat svc;  systemctl edit svc
systemctl reset-failed svc;  systemctl list-timers

journalctl -u svc -f
journalctl -u svc -n 100 --no-pager --since "1 hour ago"
journalctl -p err -b;  journalctl -b -1;  journalctl -k
sudo journalctl --vacuum-time=14d
```

Minimal service (`/etc/systemd/system/myapp.service`):

```ini
[Unit]
Description=My app
After=network-online.target
Wants=network-online.target
[Service]
User=myapp
WorkingDirectory=/opt/myapp
EnvironmentFile=/etc/myapp/myapp.env
ExecStart=/usr/bin/node server.js
Restart=on-failure
RestartSec=5
NoNewPrivileges=true
ProtectSystem=full
ProtectHome=true
PrivateTmp=true
[Install]
WantedBy=multi-user.target
```

`ExecStart`: absolute path, no shell features, foreground process. Exit status `203` bad ExecStart, `200` bad WorkingDirectory, `217` bad User.

```text
cron:  m h dom mon dow  command      */5 * * * *   30 2 * * *   @reboot
crontab -e / -l / -r(!)              % must be escaped as \%
minimal env → absolute paths, redirect output: >> /var/log/x.log 2>&1
no overlap: flock -n /var/lock/x.lock cmd
timer: OnCalendar=*-*-* 02:30:00  Persistent=true   (enable the .timer, not the .service)
systemd-analyze calendar "Mon..Fri 09:00"
```

## Packages
→ [Package management](../03-system/02-package-management.md)

| Task | apt (Debian/Ubuntu) | dnf (RHEL family) |
|---|---|---|
| Refresh index | `apt update` | automatic |
| Upgrade | `apt upgrade` (`full-upgrade`) | `dnf upgrade` |
| Install / remove | `apt install x` / `apt purge x` | `dnf install x` / `dnf remove x` |
| Search / info | `apt search x` / `apt show x` | `dnf search x` / `dnf info x` |
| Which version/repo | `apt policy x` | `dnf info x` |
| Owner of file | `dpkg -S /path` | `dnf provides /path` |
| Files in package | `dpkg -L x` | `rpm -ql x` |
| Hold a version | `apt-mark hold x` | `dnf versionlock` (plugin) |
| Undo | – | `dnf history undo N` |

```bash
sudo apt autoremove;  sudo apt clean
sudo dpkg --configure -a;  sudo apt --fix-broken install   # repair
sudo dpkg-reconfigure -plow unattended-upgrades            # auto security updates
ls /var/run/reboot-required                                # reboot pending?
```

Third-party repo: key into `/etc/apt/keyrings/`, `signed-by=` in the sources entry, never `apt-key`.

## Storage
→ [Storage and filesystems](../03-system/05-storage-and-filesystems.md)

```bash
lsblk -f;  findmnt;  sudo blkid;  df -hT;  df -i
sudo du -xh --max-depth=1 / 2>/dev/null | sort -h | tail
sudo lsof +L1                         # deleted-but-open files eating space
```

```bash
sudo mkfs.ext4 /dev/sdb1              # DESTROYS data: verify device with lsblk first
sudo mount /dev/sdb1 /data;  sudo umount /data
# /etc/fstab:  UUID=<uuid>  /data  ext4  defaults,nofail  0  2
sudo mount -a;  findmnt --verify      # test BEFORE rebooting
sudo lsof +f -- /data                 # "target is busy": who holds it?
```

```bash
pvs; vgs; lvs                                        # LVM overview
sudo lvextend -r -l +100%FREE /dev/vg/lv             # -r also grows the filesystem
sudo growpart /dev/vda 1 && sudo resize2fs /dev/vda1 # cloud disk grown (XFS: xfs_growfs)
```

"Disk full" checklist: **space** (`df -h`), **inodes** (`df -i`), **deleted-open files** (`lsof +L1`), reserved blocks, data hidden under a mount point.

## Networking
→ [Networking basics](../04-networking/01-networking-basics.md)

```bash
ip -br addr;  ip route;  ip route get 8.8.8.8;  ip neigh
ss -tulpn                             # listening sockets + process
ss -tnp;  ss -s;  ss -tlnp 'sport = :3000'
ping -c3 host;  mtr -rwc 20 host;  traceroute host
timeout 3 bash -c '</dev/tcp/host/443' && echo open     # or: nc -zv host 443
getent hosts name       # what apps see (honors /etc/hosts)
dig name +short;  dig @1.1.1.1 name;  dig MX name;  dig name +trace
resolvectl status       # real upstream resolvers (systemd-resolved)
```

```bash
curl -I url;  curl -v url;  curl -sS -f url;  curl -L url;  curl -o f url
curl -X POST -H 'Content-Type: application/json' -d '{"a":1}' url
curl -s -o /dev/null -w '%{http_code}\n' url
curl -s -o /dev/null url -w 'dns=%{time_namelookup} conn=%{time_connect} tls=%{time_appconnect} ttfb=%{time_starttransfer} total=%{time_total}\n'
curl --resolve app.example.com:443:203.0.113.10 https://app.example.com/
```

| Symptom | Likely meaning |
|---|---|
| Connection **refused** | host reachable, nothing listening (or REJECT) |
| Connection **timed out** | packets dropped: firewall/security group/routing |
| Could not resolve host | DNS |
| 502 / 504 | proxy up, upstream app failed / slow |
| works locally, not remotely | bound to `127.0.0.1`, or firewall |

Ladder: `ip -br addr` → `ip route` → `ping 1.1.1.1` → `getent hosts` → `nc -zv host port` → `ss -tlnp` → `curl -v`.

## SSH
→ [SSH](../04-networking/02-ssh.md)

```bash
ssh-keygen -t ed25519 -C "me@laptop"        # use a passphrase
ssh-copy-id user@host
ssh -p 2222 -i key user@host;  ssh -vvv user@host;  ssh host 'cmd'
ssh-keygen -R host                          # remove stale host key (verify first!)
chmod 700 ~/.ssh; chmod 600 ~/.ssh/id_ed25519 ~/.ssh/authorized_keys

ssh -L 5433:localhost:5432 host             # local fwd: laptop:5433 → host's Postgres
ssh -R 8080:localhost:3000 host             # remote fwd
ssh -D 1080 host                            # SOCKS proxy        add -fN to background
ssh -J bastion user@internal                # jump host

scp file host:/path;  scp -P 2222 ...       # scp uses -P, ssh uses -p
rsync -avz -e ssh src/ host:/dest/          # trailing slash on src = copy CONTENTS
rsync -an --delete src/ dest/               # -n dry run FIRST
```

```text
# ~/.ssh/config
Host web
    HostName 203.0.113.10
    User alice
    IdentityFile ~/.ssh/id_ed25519
    IdentitiesOnly yes
    ProxyJump bastion
Host *
    ServerAliveInterval 60
```

Server: `sudo sshd -t` then `sudo systemctl reload ssh` (Red Hat: `sshd`). Keep a **second session open**. Stuck session: `Enter` `~` `.`

## Firewall and hardening
→ [Firewall](../04-networking/03-firewall.md), [Hardening](../05-production/01-server-hardening.md)

```bash
sudo ufw default deny incoming; sudo ufw default allow outgoing
sudo ufw allow OpenSSH            # BEFORE enabling!
sudo ufw allow 80,443/tcp
sudo ufw allow from 10.0.0.0/24 to any port 5432 proto tcp
sudo ufw enable;  sudo ufw status numbered;  sudo ufw delete N
sudo nft list ruleset;  sudo nft -c -f /etc/nftables.conf     # check syntax
sudo iptables -L -n -v --line-numbers
```

```text
sshd drop-in (name it 01-… so it is read FIRST):  /etc/ssh/sshd_config.d/01-hardening.conf
  PermitRootLogin no   PasswordAuthentication no   KbdInteractiveAuthentication no
  AllowUsers alice     MaxAuthTries 3
verify:  sudo sshd -T | grep -Ei 'passwordauth|permitroot'
```

```bash
sudo fail2ban-client status sshd;  sudo fail2ban-client set sshd unbanip IP
sudo aa-status                              # AppArmor
getenforce; ls -Z; sudo ausearch -m AVC -ts recent; sudo restorecon -Rv /path   # SELinux
```

Docker-published ports **bypass ufw**: use `-p 127.0.0.1:8080:80`.

## Bash scripting
→ [Bash scripting](../02-shell/03-bash-scripting.md)

```bash
#!/usr/bin/env bash
set -euo pipefail
usage() { echo "Usage: $0 <arg>" >&2; exit 2; }
[[ $# -eq 1 ]] || usage
tmp=$(mktemp -d); trap 'rm -rf "$tmp"' EXIT

if [[ -f "$f" ]]; then …; elif [[ -d "$f" ]]; then …; fi
for f in *.log; do …; done
while IFS= read -r line; do …; done < file
case "$1" in start) …;; stop) …;; *) usage;; esac
log() { printf '%s %s\n' "$(date +%T)" "$*" >&2; }
echo "${PORT:-8080}"   : "${DB:?DB is required}"   ${f##*/}  ${f%.log}
command -v jq >/dev/null || { echo "need jq" >&2; exit 1; }
```

Tests: `-f` file, `-d` dir, `-e` exists, `-x` executable, `-s` non-empty, `-z`/`-n` empty/non-empty string, `-eq -ne -lt -gt` numbers. Use `"$@"` to forward args. Debug: `bash -n`, `bash -x`, **ShellCheck**. `set -e` doesn't fire inside `if`/`&&`/`||`; `local x=$(cmd)` hides failures.

## Vim and tmux
→ [Vim and tmux](../02-shell/04-vim-and-tmux.md)

```text
vim:  i insert   Esc normal   :w save   :wq / ZZ save+quit   :q! discard   u undo   Ctrl+R redo
      h j k l   w b   0 $   gg G   42G   %          dd yy p   x   cw ciw ci"   .  repeat
      /text n N   :%s/old/new/g   :set paste   :w !sudo tee %   v V Ctrl+V visual
tmux: tmux new -As name   tmux ls   tmux a -t name   tmux kill-session -t name
      prefix = Ctrl+B:   d detach   c new win   n/p next/prev   , rename
                         % split |   " split —   arrows move   z zoom   [ scroll (q quits)
```

## Troubleshooting triage
→ [Troubleshooting](../05-production/02-troubleshooting.md)

```bash
uptime; dmesg -T | tail; journalctl -p err -n 30 --no-pager
free -h; df -h; df -i; vmstat 1 5; ss -s; top -b -n1 | head -15
last -x | head;  grep -E "Start-Date|Commandline" /var/log/apt/history.log | tail
```

| You see | Check |
|---|---|
| `EADDRINUSE` | `ss -tlnp 'sport = :PORT'` / `lsof -i :PORT` |
| `No space left on device` | `df -h`, `df -i`, `lsof +L1` |
| `Permission denied` | `id`, `namei -l path`, service user, `getfacl`, mount opts, SELinux/AppArmor |
| `command not found` (127) | `type -a`, `$PATH`, cron/systemd minimal env |
| `bad interpreter: /bin/bash^M` | CRLF: `sed -i 's/\r$//' script` |
| exit 137 / killed | `dmesg -T \| grep -i oom`, cgroup limit |
| `Too many open files` | `/proc/PID/limits`, `LimitNOFILE=` |
| TLS "not yet valid / expired" | `timedatectl`, cert dates: `openssl x509 -noout -dates` |

Logs: `journalctl -u svc`, `/var/log/syslog` (RHEL: `messages`), `auth.log` (RHEL: `secure`), `/var/log/nginx/error.log`.

## Performance
→ [Performance](../05-production/03-performance.md)

```bash
uptime; nproc                 # load vs cores (load counts R + D tasks)
vmstat 1;  mpstat -P ALL 1;  pidstat 1;  iostat -xz 1;  sudo iotop -o
free -h                       # read "available", not "free"
ps aux --sort=-%mem | head
ulimit -n;  cat /proc/PID/limits;  ls /proc/PID/fd | wc -l
sysctl vm.swappiness;  sudo sysctl -w vm.swappiness=10
echo 'vm.swappiness = 10' | sudo tee /etc/sysctl.d/99-custom.conf; sudo sysctl --system
```

`vmstat`: `r` runnable, `b` blocked, `si/so` swap in/out (sustained >0 = pressure), `wa` I/O wait, `st` steal (noisy host). Raise file limits with `LimitNOFILE=` for **services**; `limits.conf` is for login sessions.

## Containers and deep debugging
→ [Namespaces and cgroups](../06-advanced/01-namespaces-and-cgroups.md), [Advanced debugging](../06-advanced/02-advanced-debugging.md)

```bash
ls -l /proc/$$/ns;  lsns;  sudo unshare --pid --fork --mount-proc bash
PID=$(docker inspect -f '{{.State.Pid}}' name)
sudo nsenter -t $PID -n ss -tlnp           # container's network view with host tools
cat /sys/fs/cgroup/<path>/{memory.max,memory.current,cpu.max,cpu.stat}
sudo systemd-run --unit=demo -p MemoryMax=100M -p CPUQuota=50% sleep 600
docker run --init --memory=256m --cpus=0.5 --pids-limit=100 --cap-drop=ALL image
```

Memory limit → OOM kill (137); CPU limit → throttling; PID 1 ignores unhandled signals (use `--init`, exec-form `CMD`).

```bash
sudo strace -f -y -p PID                              # what is it blocked on?
strace -f -e trace=file cmd 2>&1 | grep -E 'ENOENT|EACCES'
strace -c -f cmd                                       # syscall summary
sudo lsof -p PID;  sudo lsof -i :PORT;  sudo lsof +L1
sudo tcpdump -ni any port 3000                         # -n always; filter always
sudo tcpdump -ni any 'tcp[tcpflags] & (tcp-syn|tcp-ack) == tcp-syn'
sudo perf top -p PID;  sudo perf record -F 99 -g -p PID -- sleep 30; sudo perf report --stdio
```

tcpdump: `[S]` then nothing = dropped upstream/firewall; `[S]` then `[R.]` = refused; `[S.]` = server accepted.

---

**Related:** [Interview questions](./interview.md) · [Projects](../07-projects/README.md)