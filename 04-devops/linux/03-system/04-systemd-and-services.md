# systemd and Services

systemd is the init system and service manager on almost every mainstream distro. It starts at boot as PID 1, launches and supervises services, restarts them when they crash, captures their logs, and orders everything by dependencies. If you run anything long-lived on a server (a Node app, nginx, a database), you'll be writing and debugging systemd **units**.

Prerequisites: [Processes and Signals](./01-processes-and-signals.md) (SIGTERM, exit codes) and [Permissions](../01-fundamentals/05-permissions.md).

## Core concepts

A **unit** is a config file describing something systemd manages. The kinds you'll meet:

| Unit type | Manages |
|---|---|
| `.service` | a process/daemon |
| `.timer` | scheduled activation of another unit ([timers](./04-cron-and-timers.md)) |
| `.socket` | socket activation |
| `.target` | a group of units / boot stage (like runlevels), e.g. `multi-user.target` |
| `.mount` | a filesystem mount ([storage](./05-storage-and-filesystems.md)) |

Where unit files live (later directories win):

```text
/usr/lib/systemd/system/   (or /lib/systemd/system)   shipped by packages; don't edit
/etc/systemd/system/                                    yours and overrides; edit here
/run/systemd/system/                                    runtime-generated
```

## Everyday commands

```bash
systemctl status nginx            # state, PID, recent log lines
sudo systemctl start nginx
sudo systemctl stop nginx
sudo systemctl restart nginx      # stop then start
sudo systemctl reload nginx       # ask the service to re-read config without stopping (if supported)

sudo systemctl enable nginx       # start at boot
sudo systemctl disable nginx
sudo systemctl enable --now nginx # enable AND start immediately

systemctl is-active nginx         # prints active/inactive (script-friendly exit code)
systemctl is-enabled nginx
systemctl list-units --failed     # what's broken right now
systemctl list-unit-files --type=service
systemctl cat nginx               # show the unit file(s) actually in effect, including overrides
```

`enable` and `start` are independent: **enabled** means "start at boot", **active** means "running now". Forgetting one of them is a classic bug ("it works until I reboot").

After creating or editing any unit file:

```bash
sudo systemctl daemon-reload      # make systemd re-read unit files
```

## Writing a service: a Node app

Suppose the app lives in `/opt/myapp` and listens on port 3000.

First create a dedicated unprivileged user ([users and groups](../01-fundamentals/04-users-and-groups.md)):

```bash
sudo useradd --system --no-create-home --shell /usr/sbin/nologin myapp
sudo chown -R myapp:myapp /opt/myapp
```

Environment variables go in a root-owned file with restricted permissions:

```bash
sudo install -d -m 0755 /etc/myapp
sudo tee /etc/myapp/myapp.env >/dev/null <<'EOF'
NODE_ENV=production
PORT=3000
EOF
sudo chmod 640 /etc/myapp/myapp.env
sudo chown root:myapp /etc/myapp/myapp.env
```

`/etc/systemd/system/myapp.service`:

```ini
[Unit]
Description=My Node App
After=network-online.target
Wants=network-online.target

[Service]
Type=simple
User=myapp
Group=myapp
WorkingDirectory=/opt/myapp
EnvironmentFile=/etc/myapp/myapp.env
ExecStart=/usr/bin/node server.js
Restart=on-failure
RestartSec=5
TimeoutStopSec=30
LimitNOFILE=65536

# Basic sandboxing
NoNewPrivileges=true
PrivateTmp=true
ProtectSystem=full
ProtectHome=true
UMask=0027

[Install]
WantedBy=multi-user.target
```

Bring it up:

```bash
sudo systemctl daemon-reload
sudo systemctl enable --now myapp
systemctl status myapp
journalctl -u myapp -f
```

### What each part does

- **`After=` / `Wants=`**: `After` only controls *ordering*; `Wants`/`Requires` control whether the other unit is pulled in. `Requires` is a hard dependency (if it fails, you fail); `Wants` is soft. Using `After` alone does not start anything.
- **`Type=simple`**: the process started by `ExecStart` *is* the service. Your app must run in the **foreground**; don't daemonize or double-fork it. `Type=forking` is for old-style daemons, `oneshot` for tasks that run and exit (used with timers), `notify` for services that signal readiness to systemd.
- **`ExecStart`**: needs an **absolute path** to the executable. It's not run through a shell, so pipes, `&&`, `>`, and `$HOME` don't work as in bash. If you need shell features: `ExecStart=/bin/sh -c '...'`. A variable from `EnvironmentFile` is referenced as `${PORT}`.
- **`EnvironmentFile`**: plain `KEY=value` lines, no `export`, and no shell quoting rules. Keep secrets here, not in the unit file (unit files are world-readable).
- **`Restart=on-failure`**: restart on non-zero exit, signal kill, or timeout, but not after a clean exit or `systemctl stop`. `Restart=always` restarts even after a clean exit. `RestartSec` adds a delay so a crash loop doesn't spin.
- **`User=`**: never run web apps as root. Ports below 1024 need root or `AmbientCapabilities=CAP_NET_BIND_SERVICE`; the more common production answer is to run the app on a high port behind a reverse proxy (see [the deploy project](../07-projects/01-deploy-node-app-on-vm/README.md)).
- **Sandboxing**: `ProtectSystem=full` makes `/usr`, `/boot`, `/etc` read-only for the service; `ProtectHome=true` hides `/home`; `PrivateTmp` gives it its own `/tmp`; `NoNewPrivileges` blocks privilege escalation via setuid. If the app must write somewhere, add `ReadWritePaths=/var/lib/myapp`. These options are a cheap hardening win. Check `systemd-analyze security myapp` for suggestions.
- **`UMask=0027`**: default permissions for files the service creates ([permissions](../01-fundamentals/05-permissions.md)).

### Where is node?

If Node was installed with `nvm`, it lives under the installing user's home directory, which `ProtectHome` hides and which the service user can't read. Use a system-wide Node install and check the path with `which node`; then use that absolute path in `ExecStart`. See [package management](./02-package-management.md).

### Stopping behavior

`systemctl stop` sends **SIGTERM** to the main process, waits `TimeoutStopSec` (default usually 90s), then sends **SIGKILL**. If your app ignores SIGTERM, every stop and restart will hang for that long. Handle SIGTERM in the app ([signals](./01-processes-and-signals.md)).

### Changing a package's unit without editing it

```bash
sudo systemctl edit nginx        # opens an override file: /etc/systemd/system/nginx.service.d/override.conf
```

Put only the changes in it:

```ini
[Service]
LimitNOFILE=65536
```

Overrides survive package upgrades; edits to the shipped file don't. To *reset* a list-type setting like `ExecStart`, set it to empty first (`ExecStart=`) and then give the new value. `systemctl cat` shows the merged result.

## Logs with journalctl

systemd captures each service's **stdout and stderr** into the journal, so your app just needs to log to the console; no log files or rotation to configure.

```bash
journalctl -u myapp                     # all logs for the unit
journalctl -u myapp -f                  # follow
journalctl -u myapp -n 100 --no-pager   # last 100 lines
journalctl -u myapp --since "1 hour ago"
journalctl -u myapp --since "2026-10-04 09:00" --until "2026-10-04 10:00"
journalctl -p err -b                    # errors and worse since this boot
journalctl -b -1                        # previous boot (needs persistent journal)
journalctl -k                           # kernel messages
journalctl -u myapp -o json-pretty | head   # structured output
journalctl --disk-usage
sudo journalctl --vacuum-time=14d       # prune old entries
```

On some distros the journal is volatile (lost on reboot) unless `/var/log/journal` exists or `Storage=persistent` is set in `/etc/systemd/journald.conf`. Check before relying on `-b -1`.

## Targets and boot

```bash
systemctl get-default              # e.g. multi-user.target (servers) or graphical.target (desktops)
systemd-analyze                    # total boot time
systemd-analyze blame              # slowest units at boot
systemctl list-dependencies myapp  # what it pulls in
```

## Debugging a failing service

Start with the status and the logs; most answers are right there.

```bash
systemctl status myapp --no-pager -l
journalctl -u myapp -n 50 --no-pager
```

Read the `Main PID ... (code=exited, status=...)` line. Frequent patterns:

| Symptom | Likely cause |
|---|---|
| `status=203/EXEC` | `ExecStart` path wrong, file not executable, or missing interpreter/shebang |
| `status=200/CHDIR` | `WorkingDirectory` doesn't exist or isn't accessible |
| `status=217/USER` | the `User=` doesn't exist |
| `status=1/FAILURE` in a restart loop | the app itself crashed: see its log lines |
| `Permission denied` on a path | the service user can't read it, or `ProtectSystem`/`ProtectHome` blocks it |
| `EADDRINUSE` | port already in use ([troubleshooting](../05-production/02-troubleshooting.md)) |
| `Start request repeated too quickly` / `start-limit-hit` | crashed too fast too often; fix the cause, then `systemctl reset-failed myapp` |
| Works in your shell, fails as a service | different user, no `PATH` from your profile, no env vars from `.bashrc` |

To reproduce what systemd does, run the command as the service user with a clean environment:

```bash
sudo -u myapp env -i /usr/bin/node /opt/myapp/server.js
```

`systemd-analyze verify /etc/systemd/system/myapp.service` checks a unit file for errors without starting it.

## Common mistakes

- Forgetting `daemon-reload` after editing a unit.
- `enable` without `start`, or `start` without `enable`.
- Relative paths or shell syntax in `ExecStart`.
- Putting secrets in the unit file.
- A daemonizing app with `Type=simple` (systemd thinks it exited), or the reverse.
- Editing files under `/usr/lib/systemd/system` instead of using `systemctl edit`.
- Running the service as root "to avoid permission issues".
- Relying on a tmux session or `nohup` for something that should be a service.
- Assuming `After=postgresql.service` makes it start; it only orders units if both are being started.

## Quick Summary

- systemd supervises services as **units**; yours go in `/etc/systemd/system/`.
- `enable` = at boot, `start` = now; `enable --now` does both; `daemon-reload` after edits.
- A good service unit: dedicated `User=`, absolute `ExecStart`, `EnvironmentFile`, `Restart=on-failure`, and some sandboxing.
- Services log to stdout; read them with `journalctl -u name -f`.
- Stop = SIGTERM then SIGKILL after a timeout; handle SIGTERM.
- Debug with `systemctl status`, the exit code (`203`, `200`, `217`), and by running the command as the service user.

**Next:** [Cron and Timers](./04-cron-and-timers.md)
