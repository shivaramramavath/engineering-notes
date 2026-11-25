# Vim and tmux

On a remote server you often have only a terminal: no GUI editor, and an SSH connection that can drop at any moment. **Vim** gives you a capable editor that's installed (or one `apt install` away) almost everywhere. **tmux** keeps your shells alive on the server when your connection dies, and lets you run several shells in one SSH session. You don't need to master either; you need a reliable core of each.

Prerequisites: [Working with Files](../01-fundamentals/03-working-with-files.md) (nano basics) and [Shell Basics](./01-shell-basics.md). For connecting to servers, see [SSH](../04-networking/02-ssh.md).

# Part 1: Vim

## The one idea: modes

Vim is modal. The same key does different things depending on the mode, which is why it confuses newcomers.

```text
            i, a, o                     :
 NORMAL  ───────────►  INSERT      NORMAL ──────► COMMAND-LINE
 (move, delete, copy)  (type text)  (:w  :q  :%s/…)
    ▲                      │
    └──────── Esc ─────────┘
```

- **Normal**: the default. Keys are commands (`dd` deletes a line).
- **Insert**: typing inserts text. Enter with `i`, leave with `Esc`.
- **Visual**: select text (`v` characters, `V` lines, `Ctrl+V` block).
- **Command-line**: type `:` for commands like `:w`.

If something strange happens, press `Esc` a couple of times to get back to Normal mode.

## Survival kit

```bash
vim file.txt        # open (creates it on save if missing)
```

| Do this | Keys |
|---|---|
| Start typing | `i` |
| Stop typing | `Esc` |
| Save | `:w` |
| Save and quit | `:wq` or `ZZ` |
| Quit | `:q` |
| Quit, discard changes | `:q!` |
| Undo / redo | `u` / `Ctrl+R` |

That's enough to edit a config file safely.

## Moving around (Normal mode)

| Keys | Action |
|---|---|
| `h j k l` | left, down, up, right |
| `w` / `b` | next / previous word |
| `0` / `$` | start / end of line |
| `gg` / `G` | first / last line |
| `42G` or `:42` | go to line 42 |
| `Ctrl+D` / `Ctrl+U` | half page down / up |
| `%` | jump to matching bracket |

## Editing

Vim commands compose as **operator + motion**. Learn a few operators and a few motions and they multiply.

| Operator | Meaning |
|---|---|
| `d` | delete (cut) |
| `c` | change (delete and enter Insert) |
| `y` | yank (copy) |

```text
dw     delete to next word          dd    delete line
d$     delete to end of line        yy    copy line
cw     change word                  p/P   paste after/before
ciw    change the whole word        x     delete one character
ci"    change text inside quotes    o/O   new line below/above and insert
3dd    delete 3 lines               .     repeat last change
```

The `.` key is a major time saver: make one edit, move, press `.` to repeat it.

## Searching and replacing

```text
/error        search forward; n = next, N = previous
?error        search backward
*             search for the word under the cursor
:%s/old/new/g         replace everywhere in the file
:%s/old/new/gc        ask for confirmation on each
:5,20s/old/new/g      only lines 5 to 20
:noh                  clear search highlight
```

## Useful settings

Try them temporarily with `:set …`, or put them in `~/.vimrc`:

```vim
set number          " line numbers
set hlsearch        " highlight matches
set incsearch       " search as you type
set ignorecase smartcase
set expandtab tabstop=2 shiftwidth=2   " spaces instead of tabs, 2-wide
set mouse=a         " optional: mouse support
syntax on
```

Match indentation to the file type; YAML in particular must use spaces, never tabs.

## Server-specific tricks

```text
:w !sudo tee %      you opened a root-owned file without sudo; save it anyway
:e!                 reload the file, discarding unsaved changes
:set paste          stop auto-indent mangling when pasting over SSH (then :set nopaste)
Ctrl+Z              suspend vim to the shell; `fg` brings it back
```

Prefer `sudoedit /etc/file` (or `sudo -e`), which edits a copy as you and installs it as root, instead of running the whole editor as root.

If you see "E325: ATTENTION / swap file exists", another vim session has the file open, or an earlier one crashed. Check before deleting the `.swp` file; it may hold unsaved work.

To set vim as the default editor for tools like `git` and `crontab -e`: `export EDITOR=vim` in `~/.bashrc`, or `sudo update-alternatives --config editor`.

# Part 2: tmux

## Why tmux

A normal SSH session ends when the connection drops, and the running programs die with it (they get a hangup signal; see [signals](../03-system/01-processes-and-signals.md)). tmux runs a **server process on the machine** that owns your shells. You attach and detach from it; if SSH drops, everything keeps running and you reattach later.

```text
 You (SSH) ──► tmux client ──► tmux server ──► session
                                                 ├─ window 1 ─ [pane | pane]
                                                 └─ window 2 ─ [pane]
```

Install: `sudo apt install tmux`.

Terms: a **session** holds **windows** (like tabs), and windows are split into **panes**.

## Sessions

```bash
tmux new -s deploy        # new named session
tmux ls                   # list sessions
tmux attach -t deploy     # reattach (alias: tmux a -t deploy)
tmux new -As deploy       # attach if it exists, else create: handy habit
tmux kill-session -t deploy
```

Typical flow on a server:

```bash
ssh server
tmux new -As work         # start something long (a build, a migration)
# press Ctrl+B then D to detach; close the laptop; come back later
ssh server
tmux a -t work            # still running
```

## Key bindings

Every tmux command starts with the **prefix**, `Ctrl+B` by default: press it, release, then press the key.

| Keys (after prefix) | Action |
|---|---|
| `d` | detach |
| `c` | new window |
| `n` / `p` | next / previous window |
| `0`-`9` | go to window by number |
| `,` | rename window |
| `%` | split vertically (side by side) |
| `"` | split horizontally (stacked) |
| arrow keys | move between panes |
| `z` | zoom or unzoom the current pane |
| `x` | close pane |
| `[` | copy/scroll mode (arrows or PgUp, `q` to exit) |
| `?` | list all key bindings |

Scrolling with the mouse wheel doesn't work by default; use `prefix [`, or enable mouse support below.

## Minimal `~/.tmux.conf`

```tmux
set -g mouse on                 # click to select panes, scroll with wheel
set -g history-limit 50000      # more scrollback
set -g base-index 1             # number windows from 1
setw -g pane-base-index 1
set -g escape-time 10           # snappier Esc (helps in vim)
```

Reload without restarting: `tmux source-file ~/.tmux.conf`. Some people remap the prefix to `Ctrl+A` (`set -g prefix C-a`); only do it if you won't also use `screen` or nested sessions.

## Practical uses

- **Long-running jobs** (backups, large `rsync`, builds) that must survive a disconnect.
- **Layout for debugging**: one pane tailing `journalctl -f`, another running `htop`, a third for commands.
- **Shared pairing**: two people can attach to the same session (`tmux attach -t name`) and see the same screen.

A tmux session survives disconnects, **not reboots**. For anything that must run permanently or restart on its own, use a [systemd service](../03-system/03-systemd-and-services.md), not a tmux window.

## Common mistakes

**Vim**

- Typing text while in Normal mode, where letters run commands. Check for `-- INSERT --` at the bottom.
- Using `:q` with unsaved changes, then not knowing why it refuses. Use `:wq` or `:q!`.
- Pasting into Insert mode and getting a staircase of indentation (use `:set paste`).
- Tabs in YAML or Makefile confusion (YAML forbids tabs; Makefile recipes require them).
- Opening a root file without sudo and editing it, then failing on save.

**tmux**

- Closing the terminal window and assuming the session died; it didn't, so `tmux ls`.
- Nested tmux (tmux inside tmux over SSH): the outer session eats the prefix. Press the prefix twice to send it to the inner one, or use different prefixes.
- Forgetting you're in tmux and starting a second session instead of reattaching.
- Expecting tmux to keep jobs alive across a server reboot.

## Quick Summary

- Vim is modal: `i` to insert, `Esc` to return, `:wq` to save and quit, `:q!` to bail out.
- Compose operators and motions (`ciw`, `dd`, `yy`), repeat with `.`, search with `/`, replace with `:%s/a/b/g`.
- Use `sudoedit` instead of running vim as root.
- tmux keeps shells alive on the server: `tmux new -As name`, detach with `Ctrl+B d`, reattach with `tmux a -t name`.
- Prefix `c` new window, `%`/`"` split, `z` zoom, `[` scroll.
- tmux survives disconnects, not reboots; use systemd for permanent services.

**Next:** [Processes and Signals](../03-system/01-processes-and-signals.md)
