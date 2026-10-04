# Processes and Signals

A running program is a **process**. When a server is slow, stuck, or leaking memory, you are really asking: which process, doing what, and how do I stop it politely or forcefully? Signals are the mechanism for that conversation, and understanding them explains why `kill -9` is a last resort, why `Ctrl+C` works, and why your app may not shut down cleanly under systemd or Docker.

Prerequisites: [Shell Basics](../02-shell/01-shell-basics.md) (exit codes) and [Users and Groups](../01-fundamentals/04-users-and-groups.md).

## What a process is

Each process has a **PID**, a parent (**PPID**), an owner (UID/GID), an environment, open files, and a state. New processes are created by `fork()` (copy the parent) then `exec()` (replace it with a new program). That is exactly what your shell does for every command.

```text
systemd (PID 1)
 └─ sshd
     └─ sshd: alice
         └─ bash
             ├─ node server.js
             └─ tail -f app.log
```

PID 1 is the first process (systemd on most distros). Orphaned processes are re-parented to it, and in containers PID 1 is your app, which matters later.

## Looking at processes

```bash
ps aux                      # all processes, BSD style (user, %cpu, %mem, command)
ps -ef                      # all processes, with PPID
ps -ef --forest             # show parent/child tree
pstree -p                   # tree with PIDs
pgrep -af nginx             # find PIDs by name/command line (-a shows the command)
ps -o pid,ppid,user,%cpu,%mem,etime,cmd -p 1234   # chosen columns for one PID
```

Useful `ps aux` columns: `RSS` is resident memory in KB; `STAT` is the state; `TIME` is cumulative CPU time. The common pattern `ps aux | grep nginx` also matches the grep itself; `pgrep -af nginx` avoids that.

### Process states (the `STAT` column)

| Code | Meaning |
|---|---|
| `R` | running or runnable |
| `S` | sleeping, waiting for an event (normal) |
| `D` | uninterruptible sleep, usually waiting on disk/NFS I/O. **Can't be killed.** |
| `T` | stopped (Ctrl+Z or SIGSTOP) |
| `Z` | zombie: exited, but the parent hasn't collected its exit status |

A zombie uses no CPU or memory beyond a process-table entry, and you can't kill it (it's already dead). Fix the **parent** (make it reap children, or restart it). Lots of `D` processes pile up on a stuck network mount or failing disk; see [troubleshooting](../05-production/02-troubleshooting.md).

### Live view: top and htop

```bash
top                  # built in
htop                 # nicer; sudo apt install htop
```

In `top`: `P` sort by CPU, `M` by memory, `1` per-CPU view, `k` kill a PID, `q` quit. The header shows **load average** (1/5/15 minutes), memory and swap, and task counts. How to interpret load properly is in [performance](../05-production/03-performance.md).

### Inside `/proc`

Everything `ps` shows comes from `/proc/<pid>/`:

```bash
cat /proc/1234/status          # state, memory, UIDs, threads
tr '\0' ' ' < /proc/1234/cmdline   # full command line
ls -l /proc/1234/fd            # open files and sockets
ls -l /proc/1234/cwd /proc/1234/exe
sudo lsof -p 1234              # friendlier view of open files
```

## Signals

A signal is a small asynchronous message sent to a process. The process can handle it, ignore it, or take the default action (usually terminate). Two signals can't be caught or ignored: `SIGKILL` and `SIGSTOP`.

| Signal | # | Default | Typical meaning |
|---|---|---|---|
| `SIGHUP` | 1 | terminate | terminal closed; many daemons reload config on it |
| `SIGINT` | 2 | terminate | `Ctrl+C` |
| `SIGQUIT` | 3 | core dump | `Ctrl+\` |
| `SIGKILL` | 9 | terminate | forced kill, **uncatchable** |
| `SIGTERM` | 15 | terminate | polite "please shut down" (default for `kill`) |
| `SIGSTOP` | 19 | stop | pause, uncatchable |
| `SIGTSTP` | 20 | stop | `Ctrl+Z` |
| `SIGCONT` | 18 | continue | resume a stopped process |
| `SIGUSR1/2` | 10/12 | terminate | app-defined (e.g. reopen logs) |

(Numbers shown are for x86 and ARM Linux. Use names in scripts; `kill -l` lists them.)

### Sending signals

```bash
kill 1234              # SIGTERM to PID 1234
kill -TERM 1234        # same, explicit
kill -9 1234           # SIGKILL
kill -HUP 1234         # ask daemon to reload
kill -0 1234           # no signal; just test whether the PID exists and you may signal it

pkill nginx            # by name
pkill -f "node server.js"   # match full command line
killall nginx          # all processes with that exact name
```

### The right order for stopping something

1. `kill PID` (SIGTERM). The app can flush buffers, close connections, remove lock files.
2. Wait a few seconds; check with `ps -p PID`.
3. Only then `kill -9 PID`. The kernel ends it immediately: no cleanup, possible corrupted state or stale lock/PID files.

`kill -9` first is a bad habit. If SIGTERM is ignored, ask why (the process may be stuck in `D` state, which `-9` won't fix either).

Be careful with `pkill -f`: the pattern matches any command line that contains it, including your own shell command or an unrelated process. Check with `pgrep -af pattern` first.

### Handling signals in your own code

A well-behaved service handles SIGTERM, finishes in-flight work, and exits. Node example:

```js
process.on('SIGTERM', () => {
  server.close(() => process.exit(0));   // stop accepting, drain, exit
});
```

systemd sends SIGTERM on `systemctl stop`, then SIGKILL after a timeout (90s by default, see [systemd](./03-systemd-and-services.md)). Docker does the same with a 10s default.

In a **container**, your app is PID 1. The kernel doesn't apply default signal actions to PID 1, so a program with no SIGTERM handler may ignore `docker stop` until it gets killed after the timeout. Handle SIGTERM explicitly or run with an init (`docker run --init`). More in [namespaces and cgroups](../06-advanced/01-namespaces-and-cgroups.md).

### Exit status and signals

A process killed by signal *N* reports exit code **128 + N** to its parent shell: `130` is Ctrl+C (SIGINT), `143` is SIGTERM, `137` is SIGKILL. A surprise `137` often means the **OOM killer** struck:

```bash
dmesg -T | grep -i -E "out of memory|killed process"
journalctl -k | grep -i oom
```

## Foreground, background, and job control

```bash
long_task &            # run in background (still tied to this terminal)
jobs                   # list this shell's jobs
fg %1                  # bring job 1 to foreground
Ctrl+Z                 # suspend the foreground job (SIGTSTP)
bg %1                  # resume it in the background
```

When the terminal closes, the shell sends **SIGHUP** to its jobs, and they usually die. Options, from weakest to best:

```bash
nohup ./job.sh > job.log 2>&1 &   # ignore SIGHUP, redirect output
disown -h %1                      # detach an already-running job from the shell
```

For anything you care about, use [tmux](../02-shell/04-vim-and-tmux.md) (interactive work) or a [systemd service](./03-systemd-and-services.md) (permanent). A backgrounded job is not a service: nothing restarts it or collects its logs.

## Priority: nice and renice

Niceness ranges from **-20** (highest priority) to **19** (lowest); the default is 0. A "nicer" process yields CPU to others. Only root can lower niceness (raise priority).

```bash
nice -n 10 ./backup.sh          # start with lower priority
renice -n 15 -p 1234            # change a running process
ionice -c3 ./backup.sh          # idle I/O class: only use disk when nobody else does
```

Use these for batch work (backups, compression) that shouldn't slow down the main service. They don't create more CPU; they only decide who waits.

## Common mistakes

- **Reaching for `kill -9` immediately**, then wondering about corrupted data or leftover lock files.
- **`pkill -f` with a loose pattern** killing unrelated processes.
- **Killing the wrong PID.** Re-check with `ps -p PID -o pid,user,cmd` first.
- **Trying to kill a zombie** or a `D`-state process. Fix the parent or the I/O problem.
- **Using `&` and assuming survival**: closing the terminal still sends SIGHUP.
- **Services that ignore SIGTERM**, causing 90-second stop delays or killed containers.
- **Confusing %CPU in `ps`** (average over process lifetime) with `top` (current interval).

## Quick Summary

- A process has a PID, PPID, owner, and state; `ps`, `pgrep`, `top`, and `/proc/<pid>` inspect it.
- Signals are messages: SIGTERM asks nicely, SIGKILL forces, SIGHUP often means "reload".
- Always try SIGTERM before SIGKILL; handle SIGTERM in your own services.
- Exit code 128+N tells you which signal ended a process; `137` often means OOM-kill.
- `&`/`nohup` are fragile; use tmux or systemd for anything that matters.
- `nice`/`ionice` keep background work from hurting foreground work.

**Next:** [Package Management](./02-package-management.md)
