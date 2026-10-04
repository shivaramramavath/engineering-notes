# Working with Files

Creating, copying, moving, deleting, finding, archiving, and reading files covers most of what you do in a terminal. The commands are simple, but a few of them (`rm`, `mv`, `cp` over existing files) destroy data silently, so the habits matter as much as the syntax.

Prerequisite: [Filesystem and Navigation](./02-filesystem-and-navigation.md).

## Create, copy, move, delete

```bash
mkdir -p app/logs/archive      # -p creates parents, no error if it exists
touch notes.md                 # create empty file or update its timestamp

cp config.yml config.yml.bak   # copy a file
cp -r src/ backup/             # copy a directory recursively
cp -a src/ backup/             # archive mode: recursive + keep perms, times, links

mv old.txt new.txt             # rename
mv file.txt /tmp/              # move

rm file.txt                    # delete a file
rm -r olddir/                  # delete a directory and contents
rmdir emptydir/                # only works if empty
```

Behavior to internalize:

- **`rm` has no trash.** Deleted means gone. There is no undo.
- **`cp` and `mv` overwrite silently** when the destination exists. Use `-i` to be asked, or `-n` to never overwrite.
- **`mv` within one filesystem is just a rename** (instant, no data copied). Across filesystems it copies then deletes.
- `cp -r src dst` behaves differently depending on whether `dst` exists: if it does, you get `dst/src`. If not, `dst` becomes the copy. A trailing slash on the *source* with `rsync` also changes meaning (see [cron and timers](../03-system/04-cron-and-timers.md)).

A safe habit before deleting with a glob: run `ls` with the same pattern first.

```bash
ls ./build/*.tmp      # check what matches
rm ./build/*.tmp
```

### The classic disasters

```bash
rm -rf /            # modern GNU rm refuses without --no-preserve-root
rm -rf $DIR/        # if $DIR is empty or unset, this becomes rm -rf /
rm -rf ~ /tmp/x     # a stray space; deletes your home directory
```

In scripts, quote variables and use `set -u` so unset variables are an error ([bash scripting](../02-shell/03-bash-scripting.md)).

## Looking at file contents

```bash
cat file.txt              # print whole file
less /var/log/syslog      # scroll, search, quit
head -n 20 file.txt       # first 20 lines
tail -n 50 file.txt       # last 50 lines
tail -f /var/log/app.log  # follow new lines as they arrive
wc -l file.txt            # count lines
```

`less` is the right tool for big files. Keys: `Space`/`b` page forward/back, `/text` search forward, `n` next match, `G` end, `g` start, `F` follow like `tail -f` (Ctrl+C to stop following), `q` quit.

## Editing with nano

`nano` is the easy editor; learn it first so you can edit on any server. [Vim and tmux](../02-shell/04-vim-and-tmux.md) come later.

```bash
nano file.txt
```

Commands are shown at the bottom (`^` means Ctrl): `Ctrl+O` write out (save), `Ctrl+X` exit, `Ctrl+W` search, `Ctrl+K` cut line, `Ctrl+U` paste.

For system files use `sudo nano /etc/...`, or better, `sudoedit`, which edits a temp copy as you and writes it back with privileges.

## Finding files with `find`

`find` walks a directory tree and tests every entry.

```bash
find . -name "*.log"                    # by name (quote the pattern!)
find . -iname "readme*"                 # case-insensitive
find /var/log -type f -size +100M       # regular files over 100 MB
find . -type d -name node_modules       # directories
find . -mtime -1                        # modified in the last 24 hours
find . -mtime +30                       # not modified for over 30 days
find . -user alice -perm -u+x           # owned by alice, user-executable
```

Acting on results:

```bash
find . -name "*.tmp" -delete                       # delete matches
find . -name "*.js" -exec grep -l "TODO" {} +      # run a command on matches
find . -name "*.log" -print0 | xargs -0 gzip       # safe with odd filenames
```

Notes:

- **Quote the pattern.** Unquoted `*.log` is expanded by the shell *before* `find` sees it, and gives wrong results.
- `-delete` is applied in the order given. Put your tests *before* it, or `find . -delete -name x` deletes everything.
- `-exec ... {} +` batches many files per command (fast); `-exec ... {} \;` runs one command per file.
- `-print0 | xargs -0` handles spaces and newlines in names. More in [text processing](../02-shell/02-text-processing.md).

For "where is this command", use `which`; for a quick name lookup across the system, `locate` works if `plocate`/`mlocate` is installed and its index is fresh.

## Links

Two kinds of links let one file appear at two paths.

```bash
ln -s /opt/app-v2 /opt/app       # symbolic (soft) link: a path pointer
ln original.txt alias.txt        # hard link: another name for the same inode
```

| | Hard link | Symbolic link |
|---|---|---|
| Points to | the same inode (same data) | a path (text) |
| Survives deleting original | yes | no (becomes dangling) |
| Works across filesystems | no | yes |
| Can link directories | no | yes |

Symlinks are everywhere in practice: `current -> releases/2026-10-04` for deployments, and the merged `/bin -> usr/bin` from the previous note. Note the argument order: `ln -s TARGET LINKNAME`.

```bash
ls -l /opt/app          # shows: app -> /opt/app-v2
readlink -f /opt/app    # resolve to the real path
```

## Archives and compression

`tar` bundles files into one archive; `gzip` compresses one stream. Together they give `.tar.gz` (or `.tgz`).

```bash
tar -czf backup.tar.gz project/        # create (c), gzip (z), file (f)
tar -tzf backup.tar.gz                 # list contents without extracting
tar -xzf backup.tar.gz                 # extract here
tar -xzf backup.tar.gz -C /tmp/restore # extract into a directory (must exist)
```

Remember it as: **c**reate, **x**tract, **t**est/list; **z** for gzip; **f** names the file and must be followed by the filename.

```bash
gzip file.log        # replaces file.log with file.log.gz
gunzip file.log.gz   # reverse
zcat file.log.gz     # read without extracting
```

Gotchas:

- `gzip` replaces the original by default; use `gzip -k` to keep it.
- Archive a directory by relative path (`tar -czf x.tar.gz project/`), not absolute, so it extracts cleanly elsewhere. GNU tar strips the leading `/` and warns.
- Always `tar -tzf` an archive from someone else before extracting, to see what it will unpack into.
- For `.zip` use `zip -r out.zip dir/` and `unzip out.zip`.

## Common mistakes

- Running `rm -rf` with an unquoted or possibly empty variable.
- Forgetting `cp -r` for directories (`omitting directory` error) or `-a` when permissions and timestamps matter.
- Overwriting with `mv`/`cp` and having no backup.
- Unquoted patterns in `find -name`.
- Confusing `ln -s` argument order; the link name is last.
- Deleting a file that a process still has open: the disk space is **not** freed until the process closes it. See [troubleshooting](../05-production/02-troubleshooting.md).

## Quick Summary

- `cp -a` to copy faithfully, `mv` to rename or move, `rm` is permanent; use `-i` and `ls` first when unsure.
- `less` for reading, `tail -f` for logs, `nano` for quick edits.
- `find` filters by name, type, size, and time; quote patterns, put `-delete` last, use `-print0 | xargs -0` for odd names.
- Symlinks point at paths and are flexible; hard links share an inode and stay on one filesystem.
- `tar -czf` / `-xzf` / `-tzf` covers 95% of archive work.

**Next:** [Users and Groups](./04-users-and-groups.md)
