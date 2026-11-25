# Bash Scripting

A shell script is just commands saved in a file, plus variables, conditionals, loops, and functions. It is the standard glue for backups, deployments, health checks, and anything repetitive on a server. The goal of this note is scripts that fail loudly and clean up after themselves, rather than ones that silently keep going after something breaks.

Prerequisites: [Shell Basics](./01-shell-basics.md) (quoting, exit codes, redirection) and [Text Processing](./02-text-processing.md).

## Your first script

```bash
#!/usr/bin/env bash
echo "Hello from $(hostname)"
```

```bash
chmod +x hello.sh
./hello.sh
```

- The first line, the **shebang**, tells the kernel which interpreter to use. `#!/usr/bin/env bash` finds `bash` on `PATH`.
- You need the execute bit ([permissions](../01-fundamentals/05-permissions.md)), or run it as `bash hello.sh`.
- **bash ≠ sh.** On Debian/Ubuntu `/bin/sh` is `dash`, which lacks bash features like `[[ ]]`, arrays, and `${var//a/b}`. If you use those, the shebang must say bash. Running `sh script.sh` ignores the shebang.

## Variables and arguments

```bash
name="world"
echo "Hello, ${name}"

readonly BACKUP_DIR="/var/backups"       # constant
count=$((count + 1))                     # arithmetic
```

Positional parameters:

| Variable | Meaning |
|---|---|
| `$0` | script name |
| `$1`, `$2`, … | arguments |
| `$#` | number of arguments |
| `"$@"` | all arguments, each preserved as a separate word |
| `$?` | exit code of the last command |

Always use `"$@"` (quoted) to forward arguments; `$*` and unquoted `$@` re-split on spaces.

### Parameter expansion (defaults and checks)

```bash
echo "${PORT:-8080}"             # use 8080 if PORT is unset or empty
: "${DB_HOST:?DB_HOST is required}"   # abort with a message if unset
file="/var/log/app.log"
echo "${file##*/}"               # app.log      (strip longest prefix up to /)
echo "${file%.log}"              # /var/log/app (strip suffix)
echo "${name^^}"                 # uppercase (bash 4+)
```

## Conditionals

```bash
if [[ -f "$file" ]]; then
  echo "file exists"
elif [[ -d "$file" ]]; then
  echo "it's a directory"
else
  echo "missing"
fi
```

Use `[[ ... ]]` in bash scripts: it doesn't word-split variables, supports `&&`, `||`, `==` pattern matching, and `=~` regex. The older `[ ... ]` (a command named `[`) works in plain `sh` but needs strict quoting.

Common tests:

| Test | True when |
|---|---|
| `-f path` / `-d path` | regular file / directory exists |
| `-e path` | anything exists |
| `-r`, `-w`, `-x path` | readable / writable / executable |
| `-s path` | file exists and is non-empty |
| `-z "$s"` / `-n "$s"` | string empty / non-empty |
| `"$a" == "$b"` | strings equal |
| `$n -eq 5`, `-ne`, `-lt`, `-gt`, `-le`, `-ge` | numeric comparison (or use `(( n > 5 ))`) |

Because `if` tests a command's exit code, you can use any command:

```bash
if grep -q "ERROR" app.log; then
  echo "errors found"
fi

if ! command -v jq >/dev/null; then
  echo "jq is required" >&2
  exit 1
fi
```

`case` is cleaner than a ladder of `elif` for matching one value:

```bash
case "$1" in
  start)   echo "starting" ;;
  stop)    echo "stopping" ;;
  restart) "$0" stop; "$0" start ;;
  *)       echo "Usage: $0 {start|stop|restart}" >&2; exit 2 ;;
esac
```

## Loops

```bash
for f in /var/log/*.log; do
  echo "processing $f"
done

for i in {1..5}; do echo "$i"; done

for ((i = 0; i < 3; i++)); do echo "$i"; done

while [[ $retries -lt 5 ]]; do
  curl -fsS http://localhost:3000/health && break
  retries=$((retries + 1))
  sleep 2
done
```

Reading a file or command output line by line, the safe form:

```bash
while IFS= read -r line; do
  echo "got: $line"
done < input.txt
```

`IFS=` keeps leading/trailing whitespace and `-r` stops backslash processing. Never do `for line in $(cat file)`, which splits on every space.

Piping into `while` runs it in a subshell, so variables set inside are lost afterward. Use redirection or process substitution instead:

```bash
while IFS= read -r host; do ... done < <(some_command)
```

## Functions

```bash
log() {
  printf '%s %s\n' "$(date +%T)" "$*" >&2
}

backup() {
  local src="$1" dest="$2"      # local keeps variables out of the global scope
  tar -czf "$dest" "$src" || return 1
}

if backup /etc /tmp/etc.tgz; then log "ok"; else log "backup failed"; fi
```

- A function's "return value" is an exit code (0 to 255), set with `return`. To get data out, `echo` it and capture with `$(...)`.
- `exit` ends the whole script; `return` leaves just the function.
- Send logs and errors to **stderr** (`>&2`) so stdout stays clean for data.

## Arrays

```bash
servers=(web1 web2 db1)
servers+=(cache1)
echo "${servers[0]}"
echo "${#servers[@]}"             # length
for s in "${servers[@]}"; do ping -c1 "$s"; done   # quote to preserve elements
```

Associative arrays (`declare -A map; map[key]=value`) need bash 4+.

## Failing safely: `set -euo pipefail`

Put this near the top of scripts you rely on:

```bash
set -euo pipefail
IFS=$'\n\t'
```

| Option | Effect |
|---|---|
| `-e` | exit when a command fails |
| `-u` | error on use of an unset variable (prevents `rm -rf "$UNSET/"`) |
| `-o pipefail` | a pipeline fails if **any** stage fails, not just the last |

Caveats you should know, because `-e` is not magic:

- It doesn't trigger inside `if`, `while`, `&&`/`||` chains, or `!`. `cmd || true` deliberately ignores failure.
- `grep` returning 1 for "no match" counts as a failure. Handle it: `grep -q x f || true`.
- `local x=$(cmd)` hides `cmd`'s failure because `local` itself succeeds. Declare first, assign next:

```bash
local out
out=$(cmd)
```

- Behavior inside functions called from conditions is inconsistent. For critical steps, check explicitly: `cmd || { log "failed"; exit 1; }`.

## Cleanup with `trap`

`trap` runs a handler when the script exits or receives a signal. `EXIT` fires on normal exit, errors under `set -e`, and signals like SIGINT/SIGTERM that terminate the script.

```bash
tmpdir=$(mktemp -d)
cleanup() {
  rm -rf "$tmpdir"
}
trap cleanup EXIT

# use "$tmpdir" freely; it is removed however the script ends
```

Other useful traps: `trap 'echo "failed at line $LINENO" >&2' ERR` for debugging. Signals such as `kill -9` (SIGKILL) can't be trapped; see [processes and signals](../03-system/01-processes-and-signals.md).

Use `mktemp` for temp files, never fixed names like `/tmp/out`, which can collide or be hijacked.

## A script skeleton

```bash
#!/usr/bin/env bash
set -euo pipefail

usage() {
  echo "Usage: $0 <source-dir> <dest-dir>" >&2
  exit 2
}

[[ $# -eq 2 ]] || usage
src="$1"
dest="$2"

[[ -d "$src" ]] || { echo "No such directory: $src" >&2; exit 1; }

log() { printf '%s %s\n' "$(date '+%F %T')" "$*" >&2; }

tmp=$(mktemp -d)
trap 'rm -rf "$tmp"' EXIT

archive="$tmp/backup-$(date +%F).tar.gz"
log "archiving $src"
tar -czf "$archive" -C "$src" .

mkdir -p "$dest"
mv "$archive" "$dest/"
log "done: $dest/$(basename "$archive")"
```

Worked scripts (backups, log rotation, health checks) live in [the bash automation project](../07-projects/02-bash-automation/README.md).

## Debugging

```bash
bash -n script.sh        # syntax check only, doesn't run
bash -x script.sh        # trace every command with expansions
set -x ... set +x        # trace just a section
PS4='+ ${BASH_SOURCE}:${LINENO}: ' bash -x script.sh   # include file:line in the trace
```

Install **ShellCheck** (`sudo apt install shellcheck`) and run it on every script. It catches unquoted variables, bad tests, and most bugs listed below, with an explanation for each.

## Common mistakes

- Unquoted `$var` and `$(cmd)` leading to word splitting and globbing.
- Using bash features under `sh` or a `#!/bin/sh` shebang.
- Assuming `set -e` catches everything (see caveats above).
- Looping with `for x in $(ls)` or `$(cat file)`.
- Forgetting `local` in functions, so they clobber globals.
- Hard-coded temp paths and no cleanup.
- Writing `[ $a == $b ]` unquoted, or `[[ $a = $b ]]` when you meant a numeric compare.
- Ignoring failures of `cd`: `cd "$dir" || exit 1` before relative `rm`s.
- Scripts that depend on the caller's current directory or `PATH`. Use absolute paths, or `cd "$(dirname "$0")"` where appropriate; cron's environment is minimal ([cron](../03-system/04-cron-and-timers.md)).

## Quick Summary

- Shebang + `chmod +x`; use `#!/usr/bin/env bash` when using bash features.
- Quote everything; use `[[ ]]`, `"$@"`, and `${var:-default}`.
- `if` tests exit codes, so `if grep -q …` and `if ! command -v …` are idiomatic.
- Start serious scripts with `set -euo pipefail`, and know its gaps.
- `trap … EXIT` plus `mktemp` gives reliable cleanup.
- Debug with `bash -x`, check with ShellCheck.

**Next:** [Vim and tmux](./04-vim-and-tmux.md)
