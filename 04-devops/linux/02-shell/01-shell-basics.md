# Shell Basics

The shell is the program that reads your commands, expands them, starts other programs, and wires their input and output together. Nearly everything in Linux administration (and every script you'll write) rests on a handful of shell rules: how words are split, how quoting works, how data flows through pipes, and how success or failure is reported.

Prerequisite: [Filesystem and Navigation](../01-fundamentals/02-filesystem-and-navigation.md).

This note uses **bash**, the default on Ubuntu. Check yours with `echo $SHELL` (login shell) or `echo $0` (current shell).

## Terminal vs shell

The **terminal** is the window; the **shell** is the program running inside it. Closing the terminal usually kills the shell and its children, which is why [tmux](./04-vim-and-tmux.md) exists for remote work.

## Anatomy of a command

```bash
ls -l --color=auto /var/log
│  │  │             └─ argument
│  │  └─ option (long form)
│  └─ option (short form)
└─ command
```

The shell splits the line into **words** on whitespace, performs expansions, then runs the command with the resulting words as arguments. The program never sees quotes, `*`, or `$VAR`; the shell has already handled them.

Find out what a command is and how to use it:

```bash
type -a ls        # alias, builtin, or file? (shows all matches)
help cd           # docs for shell builtins
man ls            # manual page (q to quit)
ls --help         # quick usage for most programs
```

## Variables

```bash
name="alice"            # no spaces around =
echo "$name"            # use with $
echo "${name}_backup"   # braces when text follows directly
```

- Shell variables are local to the current shell. `export` makes a variable visible to child processes (environment variables).
- Inspect with `env` or `printenv`.

```bash
export APP_ENV=production
APP_ENV=staging node server.js     # set only for this one command
```

Useful built-ins: `$HOME`, `$USER`, `$PWD`, `$?` (last exit code), `$$` (this shell's PID), `$0` to `$9` (script arguments, see [scripting](./03-bash-scripting.md)).

### Command substitution

```bash
today=$(date +%F)
echo "Backup for $today"
echo "Files: $(ls | wc -l)"
```

Use `$(...)`, not backticks; it nests and reads better.

## Quoting

This is where most beginner bugs come from.

| Form | Effect |
|---|---|
| `'single'` | everything literal; no expansion at all |
| `"double"` | `$var`, `$(cmd)` and `\` still work; word splitting and globbing are suppressed |
| `\x` | escape one character |
| no quotes | word splitting **and** globbing apply to expanded results |

```bash
f="my file.txt"
rm $f        # BAD: runs rm my file.txt (two arguments)
rm "$f"      # right: one argument
echo '$HOME' # prints: $HOME
echo "$HOME" # prints: /home/alice
```

**Rule of thumb: quote every variable expansion** (`"$var"`, `"$(cmd)"`) unless you specifically want splitting.

## Globbing and brace expansion

The shell expands these *before* running the command:

```bash
ls *.log            # any chars
ls file?.txt        # exactly one char
ls [ab]*.conf       # a or b, then anything
echo {a,b,c}.txt    # a.txt b.txt c.txt
mkdir -p proj/{src,tests,docs}
cp config.yml{,.bak}   # cp config.yml config.yml.bak
```

If a glob matches nothing, bash passes the pattern through literally (`*.xyz`), which can surprise you in scripts. Globs are not regular expressions; those are covered in [text processing](./02-text-processing.md).

## Streams, redirection, and pipes

Every process starts with three streams:

| FD | Name | Default |
|---|---|---|
| 0 | stdin | keyboard |
| 1 | stdout | terminal |
| 2 | stderr | terminal |

```bash
cmd > out.txt         # stdout to file (overwrite)
cmd >> out.txt        # append
cmd 2> err.txt        # stderr only
cmd > all.txt 2>&1    # both to the same file
cmd &> all.txt        # bash shorthand for the line above
cmd > /dev/null 2>&1  # discard everything
cmd < input.txt       # file as stdin
```

**Order matters**: `2>&1` means "point stderr at wherever stdout points *right now*."

```bash
cmd > f 2>&1    # both go to f
cmd 2>&1 > f    # stderr goes to the terminal, only stdout to f
```

### Pipes

A pipe connects one command's stdout to the next command's stdin. The commands run concurrently.

```bash
ps aux | grep nginx | wc -l
cat access.log | cut -d' ' -f1 | sort | uniq -c | sort -nr | head
```

Pipes carry **stdout only**. To pipe stderr too: `cmd 2>&1 | less` (or `|&` in bash).

### Here-documents

```bash
cat <<'EOF' > config.ini
[server]
port = 8080
EOF
```

Quoting `'EOF'` stops variable expansion inside the block; with plain `EOF`, `$vars` expand.

### `tee`: split a stream

```bash
make 2>&1 | tee build.log        # watch and save
echo "x" | sudo tee /etc/file    # write a root-owned file (sudo with > doesn't work)
```

## Exit codes

Every command returns a number: **0 means success; anything else is a failure**. It's the opposite of most programming languages' truthiness.

```bash
grep -q "error" app.log
echo $?          # 0 = found, 1 = not found, 2 = error
```

Combine commands with them:

```bash
mkdir build && cd build        # cd only if mkdir succeeded
cmd || echo "cmd failed"       # run on failure
a; b                           # run b regardless
```

Common codes: `1` general error, `2` misuse, `126` not executable, `127` command not found, `130` killed by Ctrl+C, `137` killed by SIGKILL (often the OOM killer or `docker kill`).

`$?` is overwritten by every command, so save it right away if you need it: `rc=$?`. In a pipeline the exit status is the **last** command's, unless you enable `set -o pipefail` ([scripting](./03-bash-scripting.md)).

## PATH and finding commands

`PATH` is a colon-separated list of directories searched, in order, for commands you type without a slash.

```bash
echo $PATH
which node           # first match in PATH
export PATH="$HOME/bin:$PATH"   # prepend your own directory
```

A script in the current directory isn't found by name for safety; run it as `./script.sh`. If the wrong version of a tool runs, `type -a toolname` shows every match and their order.

## Startup files and aliases

| File | Read when |
|---|---|
| `~/.bashrc` | interactive non-login shells (new terminal tab, tmux pane) |
| `~/.profile` or `~/.bash_profile` | login shells (SSH login, `bash -l`) |

On Ubuntu, `~/.profile` loads `~/.bashrc`, so put aliases and prompt tweaks in `.bashrc`. Reload it with `source ~/.bashrc`.

```bash
alias ll='ls -lah'
alias gs='git status'
```

Aliases work only in interactive shells, not in scripts.

## History

```bash
history | tail           # recent commands
!!                       # repeat last command (sudo !! to retry with sudo)
Ctrl+R                   # reverse search; Ctrl+R again for older matches
```

Useful editing keys: `Ctrl+A` start of line, `Ctrl+E` end, `Ctrl+W` delete word, `Ctrl+L` clear, `Ctrl+C` interrupt, `Ctrl+D` end of input / exit.

## Common mistakes

- **Unquoted variables** that contain spaces or are empty (`rm -rf $DIR/` with `DIR` empty).
- **Spaces around `=`**: `name = "x"` runs a command called `name`.
- **`2>&1` in the wrong order**, so errors still hit the screen.
- **`sudo cmd > /root/file`**: the redirect runs as you. Use `tee`.
- **Expecting a variable to survive**: `cd` or assignments inside a pipeline's subshell (`cmd | while read x; do v=$x; done`) don't change the parent shell.
- **Testing `$?` too late**, after another command has overwritten it.
- Treating globs as regex (`ls *.log` vs `grep '.*log'`).

## Quick Summary

- The shell splits, expands, and then runs; programs never see quotes or globs.
- Quote variables: `"$var"`. Single quotes are fully literal.
- `>`, `>>`, `2>`, `2>&1`, `<`, and `|` route the three streams; redirection order matters.
- Exit code 0 is success; use `&&`, `||`, and `$?` to branch on it.
- `PATH` decides what runs; `type -a` tells you why.
- Interactive config goes in `~/.bashrc`.

**Next:** [Text Processing](./02-text-processing.md)
