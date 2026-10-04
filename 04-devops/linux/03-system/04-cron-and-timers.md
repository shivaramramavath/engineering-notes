# Cron and Timers

Servers need to do things on a schedule: take backups, rotate logs, renew certificates, run health checks. Linux has two main schedulers: the classic **cron** and **systemd timers**. Cron is everywhere and simple; timers integrate with systemd logging and handle missed runs. This note covers both, then uses them for a real job: backups with `rsync`.

Prerequisites: [Bash Scripting](../02-shell/03-bash-scripting.md) (the thing you'll schedule) and [systemd and Services](./03-systemd-and-services.md).

## cron

### Crontab format

```text
┌───────── minute        (0-59)
│ ┌─────── hour          (0-23)
│ │ ┌───── day of month  (1-31)
│ │ │ ┌─── month         (1-12)
│ │ │ │ ┌─ day of week   (0-7; 0 and 7 are Sunday)
│ │ │ │ │
* * * * *  command
```

Field syntax: `*` any, `5` exact, `1,15` list, `1-5` range, `*/10` every 10 units.

```text
30 2 * * *       /usr/local/bin/backup.sh        # every day at 02:30
*/5 * * * *      /usr/local/bin/healthcheck.sh   # every 5 minutes
0 9 * * 1-5      /usr/local/bin/report.sh        # weekdays at 09:00
0 0 1 * *        /usr/local/bin/monthly.sh       # first of each month
@reboot          /usr/local/bin/on-boot.sh       # at startup
@daily           /usr/local/bin/cleanup.sh       # also @hourly, @weekly, @monthly
```

If you set **both** day-of-month and day-of-week (neither `*`), cron runs when **either** matches, not both. Surprising, and a classic source of extra runs.

### Managing crontabs

```bash
crontab -e          # edit your own crontab
crontab -l          # list it
crontab -r          # remove it entirely, with no confirmation (use crontab -ri to be asked)
sudo crontab -u alice -l    # another user's
```

Typing `-r` instead of `-e` (they're neighbours on the keyboard) deletes the whole file. Keep your crontab in version control or at least `crontab -l > crontab.bak`.

System-wide locations (managed as files):

| Path | Notes |
|---|---|
| `/etc/crontab` | has an extra **user** column |
| `/etc/cron.d/*` | drop-in files, same format as `/etc/crontab` (with user column) |
| `/etc/cron.hourly`, `.daily`, `.weekly`, `.monthly` | just drop an executable script; run by `run-parts` |

`run-parts` on Debian/Ubuntu skips files whose names contain a dot, so `backup.sh` in `/etc/cron.daily/` may silently never run. Name it `backup`.

### Why cron jobs "work in my terminal but not in cron"

Cron runs jobs with a **minimal environment**: a tiny `PATH` (often `/usr/bin:/bin`), `SHELL=/bin/sh`, no profile, no TTY, and your home as working directory. So:

- **Use absolute paths** for commands and files, or set `PATH` at the top of the crontab.
- **Don't rely on `.bashrc`**, aliases, or nvm. They aren't loaded.
- **Escape `%`** in the command: cron turns an unescaped `%` into a newline. `date +%F` must be written `date +\%F` (or move the logic into a script).
- **Capture output.** Cron mails stdout/stderr to the owner (if mail is configured) or discards it. Log explicitly:

```text
SHELL=/bin/bash
PATH=/usr/local/bin:/usr/bin:/bin
MAILTO=""

30 2 * * * /usr/local/bin/backup.sh >> /var/log/backup.log 2>&1
```

Reproduce the environment when debugging:

```bash
env -i SHELL=/bin/sh PATH=/usr/bin:/bin /usr/local/bin/backup.sh
```

### Prevent overlapping runs

If a job takes longer than its interval, cron starts another copy anyway. Use `flock`:

```text
*/5 * * * * flock -n /var/lock/healthcheck.lock /usr/local/bin/healthcheck.sh >> /var/log/health.log 2>&1
```

`-n` makes it skip the run if the lock is held.

### Time zones

Cron uses the **system timezone** (`timedatectl`). Servers are often UTC, so "02:30" is not local time. DST changes can skip or repeat runs on a local-time zone, which is a good reason to run servers in UTC.

### Checking whether it ran

```bash
journalctl -u cron --since today           # Debian/Ubuntu service name is "cron"
grep CRON /var/log/syslog                  # if rsyslog writes syslog
journalctl -u crond --since today          # Red Hat family: "crond"
```

The log shows that cron *started* the command, not that it succeeded. Check your own log file for that.

## systemd timers

A timer is a `.timer` unit that activates a matching `.service` unit. You write two small files, and gain journal logging, dependencies, catch-up for missed runs, and the full set of service options (users, sandboxing, resource limits).

`/etc/systemd/system/backup.service`:

```ini
[Unit]
Description=Nightly backup

[Service]
Type=oneshot
User=backup
ExecStart=/usr/local/bin/backup.sh
Nice=10
IOSchedulingClass=idle
```

`/etc/systemd/system/backup.timer`:

```ini
[Unit]
Description=Run backup nightly

[Timer]
OnCalendar=*-*-* 02:30:00
Persistent=true
RandomizedDelaySec=5min

[Install]
WantedBy=timers.target
```

```bash
sudo systemctl daemon-reload
sudo systemctl enable --now backup.timer     # enable the TIMER, not the service
systemctl list-timers                         # next/last run for every timer
sudo systemctl start backup.service           # run it once now, to test
journalctl -u backup.service -n 50
```

Key options:

- **`OnCalendar=`**: wall-clock schedule. Examples: `daily`, `hourly`, `Mon..Fri 09:00`, `*-*-01 00:00:00` (monthly). Validate with `systemd-analyze calendar "Mon..Fri 09:00"`; it prints the next elapse times.
- **`OnBootSec=` / `OnUnitActiveSec=`**: relative ("5 minutes after boot", "every 15 minutes after last run").
- **`Persistent=true`**: if the machine was off at the scheduled time, run once at next boot. Cron doesn't do this (unless you use anacron).
- **`RandomizedDelaySec=`**: spread load so a fleet doesn't hit a backend at the same second.

A `oneshot` service that's still running when the timer fires again isn't started a second time, so basic overlap protection is built in.

### Cron or timer?

| Need | Pick |
|---|---|
| Quick one-liner, everyone understands it | cron |
| Logs in the journal, `systemctl status` visibility | timer |
| Run missed jobs after downtime | timer (`Persistent=true`) |
| Resource limits, sandboxing, dependencies | timer |
| Per-user schedule without root | either (`crontab -e` or `systemctl --user`) |

Both are fine. Pick one per server and stay consistent.

## Backups with rsync

`rsync` copies only what changed, locally or over SSH ([SSH](../04-networking/02-ssh.md)), and preserves metadata.

```bash
rsync -a /srv/app/ /backup/app/                 # local mirror
rsync -a -e ssh /srv/app/ backup@host:/backup/app/   # to a remote host
rsync -a --delete /srv/app/ /backup/app/        # also remove files deleted at the source
rsync -an --delete /srv/app/ /backup/app/       # -n = dry run: show what WOULD happen
rsync -a --exclude 'node_modules' --exclude '*.log' /srv/app/ /backup/app/
rsync -av --progress ...                        # verbose + progress
```

`-a` ("archive") = recursive and keeps permissions, owner, group, times, and symlinks.

### The trailing slash

The slash on the **source** changes the meaning:

```bash
rsync -a /srv/app  /backup/    # creates /backup/app/   (copies the directory itself)
rsync -a /srv/app/ /backup/    # copies the CONTENTS of app into /backup/ (no app/ level)
```

A trailing slash on the destination doesn't matter. Mistaking one for the other either buries data one level deeper or, with `--delete`, wipes unrelated files in the destination. **Always do a `-n` dry run first when `--delete` is involved.**

### Snapshot-style incremental backups

`--link-dest` hard-links unchanged files to the previous snapshot, so each dated directory looks like a full backup but only changed files use new space ([links](../01-fundamentals/03-working-with-files.md)).

```bash
#!/usr/bin/env bash
set -euo pipefail

SRC="/srv/app/"
DEST="/backup/app"
TODAY="$(date +%F)"

mkdir -p "$DEST"
rsync -a --delete \
  --link-dest="$DEST/latest" \
  "$SRC" "$DEST/$TODAY.partial/"

mv "$DEST/$TODAY.partial" "$DEST/$TODAY"
ln -sfn "$DEST/$TODAY" "$DEST/latest"

# keep 14 days
find "$DEST" -maxdepth 1 -type d -name '20??-??-??' -mtime +14 -exec rm -rf {} +
```

Writing to a `.partial` directory and renaming at the end means a failed run never looks like a good backup. The first run has no `latest` yet and rsync warns but proceeds with a full copy.

Schedule it with the cron line or timer above.

### Backup rules that matter more than the tool

- **Test restores.** A backup you've never restored is a hope, not a backup.
- **Keep a copy off the machine**, ideally in another location. A backup on the same disk doesn't survive disk failure or a compromise.
- **Databases need a dump** (`pg_dump`, `mysqldump`) or a snapshot; copying live database files with rsync can produce a corrupt backup.
- **Backups aren't RAID**, and sync isn't backup: `--delete` faithfully mirrors an accidental deletion.
- **Alert on failure.** A silently failing cron job is the usual way backups quietly stop. Log, check recency, notify.

A fuller backup/rotation/health-check set lives in [the bash automation project](../07-projects/02-bash-automation/README.md).

## Common mistakes

- Relying on environment (`PATH`, aliases, nvm) that cron doesn't have.
- Unescaped `%` in crontab lines.
- No output redirection, so failures vanish.
- `crontab -r` by accident.
- Name with a dot in `/etc/cron.daily/`.
- Overlapping runs of slow jobs (no `flock`).
- Assuming the cron timezone is your local one.
- `rsync --delete` with the wrong source/destination or trailing slash, and no dry run.
- Enabling `backup.service` instead of `backup.timer`.
- Forgetting `daemon-reload` after writing timer/service files.

## Quick Summary

- Cron: five fields + command; minimal environment, so use absolute paths and redirect output.
- Escape `%`, guard slow jobs with `flock -n`, and know the server's timezone.
- systemd timers pair `.timer` + `.service`, log to the journal, and `Persistent=true` catches missed runs; check with `systemctl list-timers`.
- `rsync -a src/ dest/` for backups; mind the trailing slash; dry-run (`-n`) before `--delete`.
- Backups need off-machine copies, tested restores, and failure alerts.

**Next:** [Storage and Filesystems](./05-storage-and-filesystems.md)
