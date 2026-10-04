# Filesystem and Navigation

Linux has one directory tree rooted at `/`. There are no drive letters; other disks and devices are mounted somewhere inside that tree. Knowing what lives where tells you immediately where to look for configs, logs, and data when something breaks.

Prerequisite: a working shell ([Getting Started](./01-getting-started.md)).

## The big idea: one tree, everything is a file

Files, directories, devices (`/dev/sda`), and even live kernel and process information (`/proc`) are exposed as paths. The same tools (`ls`, `cat`, `grep`) work on all of them. That is why Linux troubleshooting is so file-centric.

Paths are **case-sensitive**: `App.log` and `app.log` are different files.

## Directory layout

The layout follows the Filesystem Hierarchy Standard (FHS), with small variations between distros.

```text
/
├── etc/     system-wide configuration (text files)
├── var/     data that changes: logs, caches, spools, databases
├── usr/     installed programs and libraries (read-mostly)
├── home/    user home directories (/home/alice)
├── root/    home of the root user
├── tmp/     temporary files, often cleared on reboot
├── opt/     self-contained third-party software
├── srv/     data served by this machine (sites, FTP), by convention
├── boot/    kernel and bootloader files
├── dev/     device files
├── proc/    process and kernel info (virtual)
├── sys/     kernel/device tree (virtual)
├── run/     runtime state since boot: PIDs, sockets (virtual, in RAM)
└── mnt/ media/   mount points for extra filesystems
```

The ones you will use constantly:

| Path | What to look for there |
|---|---|
| `/etc` | `ssh/sshd_config`, `fstab`, `hosts`, service configs |
| `/var/log` | Logs (`syslog`, `auth.log`, nginx logs) |
| `/var/lib` | Persistent app data (databases, package state) |
| `/usr/bin`, `/usr/local/bin` | Commands; `/usr/local` is for software you install yourself |
| `/home/<user>` | Your files and dotfiles (`~/.bashrc`, `~/.ssh`) |
| `/proc`, `/sys` | Not on disk. Reading them queries the kernel |

On modern Debian/Ubuntu, `/bin`, `/sbin`, and `/lib` are symlinks into `/usr` ("merged usr"). So `/bin/ls` and `/usr/bin/ls` are the same file.

```bash
ls -ld /bin       # lrwxrwxrwx ... /bin -> usr/bin
```

## Paths

- **Absolute** paths start at `/`: `/etc/nginx/nginx.conf`. They mean the same thing from anywhere.
- **Relative** paths start from the current directory: `./app/server.js`, `../logs`.

Special names:

| Symbol | Meaning |
|---|---|
| `.` | current directory |
| `..` | parent directory |
| `~` | your home directory (expanded by the shell) |
| `-` | previous directory (with `cd -`) |

## Navigating

```bash
pwd                  # print working directory
cd /var/log          # absolute
cd ..                # up one level
cd ~                 # home (same as plain `cd`)
cd -                 # back to where you were
```

### `ls`

```bash
ls                   # names only
ls -l                # long: permissions, owner, size, date
ls -a                # include hidden files (names starting with .)
ls -lah              # long, all, human-readable sizes
ls -lt               # newest first
ls -ld /etc          # the directory itself, not its contents
```

Reading `ls -l`:

```text
-rw-r--r-- 1 alice dev 4096 Oct  4 15:30 notes.md
│└──┬───┘ │  │     │   │    └─ modified time  └─ name
│   │     │  │     │   └─ size in bytes
│   │     │  │     └─ group
│   │     │  └─ owner
│   │     └─ hard link count
│   └─ permissions (see 05-permissions.md)
└─ type: - file, d directory, l symlink
```

### Finding things quickly

```bash
tree -L 2 /etc/nginx      # directory structure (install `tree` first)
file /bin/ls              # what kind of file is this?
stat notes.md             # size, inode, timestamps, permissions
which node                # first match in PATH
type -a python3           # all matches, and whether it's an alias/builtin
```

Hidden files are just names starting with `.`. There is no hidden attribute. Your configs live there: `~/.bashrc`, `~/.ssh/`, `~/.config/`.

## Shell conveniences worth learning early

- **Tab completion** for paths and commands. Press Tab twice to list options.
- **`Ctrl+R`** to search command history.
- **Globbing** is done by the shell, not by `ls`: `ls *.log` is expanded before `ls` runs. Details in [shell basics](../02-shell/01-shell-basics.md).

## Common mistakes

- **Spaces in names.** `cd My Documents` is two arguments. Quote it (`cd "My Documents"`) or escape (`My\ Documents`). Better: avoid spaces in names you create.
- **Assuming `/tmp` is persistent.** It is often cleared on reboot, and `/run` always is. Don't store anything you need there.
- **Editing the wrong copy.** A relative path depends on where you are. When in doubt, run `pwd` or use an absolute path.
- **Looking in `/usr/bin` for a command's config.** Configuration is in `/etc`; binaries are in `/usr`; data and logs are in `/var`.
- **Thinking `/proc` and `/sys` use disk space.** They are virtual views of kernel state.

## Quick Summary

- One tree from `/`; other disks are mounted into it; everything is a file.
- Memorize the five: `/etc` config, `/var` changing data and logs, `/usr` programs, `/home` users, `/tmp` scratch.
- Absolute paths start with `/`; use `.`, `..`, `~`, and `cd -` to move around.
- `ls -lah`, `stat`, `file`, `which`, and `tree` answer most "what is this?" questions.
- Paths are case-sensitive and spaces need quoting.

**Next:** [Working with Files](./03-working-with-files.md)
