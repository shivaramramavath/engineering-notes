# Module 02: Shell & Command Line Mastery

[<- Back to Linux Track Root](../README.md) | [<- Previous Module: 01-fundamentals](../01-fundamentals/README.md) | [Next Module: 03-system ->](../03-system/README.md)

---

## 1. Overview & Scope

The Unix shell is not simply a command-line prompt; it is a high-throughput, composable text stream processing engine and programming environment. In enterprise operations and cloud engineering, system administrators, DevOps engineers, and SREs use the shell to orchestrate pipelines, manipulate configuration structures, automate maintenance routines, and diagnose production incidents.

Module 02 takes the learner from fundamental terminal mechanics to advanced defensive bash programming. You will master standard I/O streams (`stdin`, `stdout`, `stderr`), file descriptor multiplexing, shell word splitting and expansion rules, structured data parsing with `jq`, regex engines, robust automation standards (`set -euo pipefail`), and static code analysis using ShellCheck.

---

## 2. Core Architecture: Stream Processing & Pipeline Multiplexing

```
+---------------------------------------------------------------------------------------+
| UNIX SHELL STREAM ARCHITECTURE & PROCESS PIPELINES                                    |
|                                                                                       |
|   Standard File Descriptors:                                                          |
|   FD 0 (stdin)  <-- /dev/stdin (Keyboard / Pipe)                                      |
|   FD 1 (stdout) --> /dev/stdout (Terminal Display / File / Pipe)                       |
|   FD 2 (stderr) --> /dev/stderr (Diagnostic Log / Error Sink)                         |
|                                                                                       |
|   Process Pipeline Flow:                                                              |
|   [ Process A ]                                 [ Process B ]                         |
|   +-----------+                                 +-----------+                         |
|   |  stdout   | ===[ Kernel Anonymous Pipe ]===>|   stdin   |                         |
|   |  (FD 1)   |       (Ring Buffer: 64KB)       |  (FD 0)   |                         |
|   +-----------+                                 +-----------+                         |
|         | stderr (FD 2)                               | stderr (FD 2)                 |
|         v                                             v                               |
|   +-------------------------------------------------------------------------+         |
|   | Diagnostic Error Log Stream / Terminal (`2>&1` or `2>/dev/null`)         |         |
|   +-------------------------------------------------------------------------+         |
+---------------------------------------------------------------------------------------+
```

---

## 3. Module Notes Catalog

| # | Note | Level | Time | Key Concepts & Topics |
| :---: | :--- | :---: | :---: | :--- |
| **01** | [Shell Basics: Terminals, Shells & Environments](./01-shell-basics.md) | Beginner | 45m | Terminal vs shell, builtins vs binaries (`type -a`), command anatomy, variables, subshells, exit codes (`$?`) |
| **02** | [Pipes, Redirection & File Descriptors](./02-pipes-redirection-and-file-descriptors.md) | Intermediate | 55m | Standard streams (`0`, `1`, `2`), custom FDs (`3-9`), `2>&1` vs `&>`, `tee`, heredocs (`<<EOF`), herestrings (`<<<`), `/dev/null` |
| **03** | [Globbing, Quoting & Shell Expansion](./03-globbing-quoting-and-expansion.md) | Intermediate | 50m | 7 expansion phases: brace, tilde, parameter, command `$(...)`, arithmetic `$((...))`, word splitting, pathname globbing |
| **04** | [Text Processing: Grep, Sed, Awk, Cut, Sort & Tr](./04-text-processing.md) | Intermediate | 60m | Unix text filter philosophy, `grep -E`/`-F`, `sed` substitution & address ranges, `awk` records/fields/blocks, `sort`, `uniq -c` |
| **05** | [Regular Expressions in Linux: BRE vs ERE vs PCRE](./05-regular-expressions.md) | Intermediate | 50m | POSIX Basic vs Extended Regular Expressions, PCRE (`grep -P`), metacharacters, character classes, lookahead/behind |
| **06** | [JSON & Structured Data with jq](./06-jq-and-structured-data.md) | Intermediate | 50m | Streaming JSON parsing, identity filters, selectors, object construction, mapping (`map()`), select filters, raw output (`-r`) |
| **07** | [Bash Scripting: Control Structures, Functions & Traps](./07-bash-scripting.md) | Intermediate | 60m | Shebangs, `if`/`case`, `for`/`while` loops, functions, local variables, signals, `trap` cleanup routines, exit codes |
| **08** | [Advanced Bash: Parameter Expansion, Arrays & Concurrency](./08-bash-scripting-advanced.md) | Advanced | 65m | `${var:-default}`, `${var//pattern/replace}`, indexed & associative arrays, subshells, coprocesses, `xargs -P`, GNU `parallel` |
| **09** | [ShellCheck, Static Analysis & Defensive Scripting](./09-shellcheck-and-script-quality.md) | Advanced | 50m | Defensive header (`set -euo pipefail`), ShellCheck SC codes (SC2086, SC2181), error handling, shunit2 unit testing |
| **10** | [Terminal Productivity: Vim & Tmux](./10-vim-and-tmux.md) | Beginner-Int | 50m | Vim modal editing (normal, insert, visual, command), buffers, registers, `~/.vimrc`, Tmux sessions, windows, panes, detach/attach |
| **11** | [Dotfiles & Shell Customization](./11-dotfiles-and-productivity.md) | Intermediate | 45m | `.bashrc`, `.bash_profile`, custom prompts (`PS1`), git integration, aliases, directory history, dotfiles management via bare git repos |

---

## 4. Key Milestones & Learning Objectives

By completing Module 02, you will be able to:
1. **Control Process Streams Precisely**: Redirect output and errors independently, multiplex logs using `tee`, and debug pipeline failures using `PIPESTATUS`.
2. **Execute Complex Log Extraction**: Combine `grep`, `sed`, and `awk` into high-performance pipelines processing millions of log entries in seconds without loading whole files into RAM.
3. **Automate Infrastructure Safely**: Write battle-tested, defensive bash scripts utilizing `set -euo pipefail`, parameter expansion defaults, and automated exit traps that clean up temporary locks and files.
4. **Manipulate Cloud API Payloads**: Extract, filter, and restructure complex nested JSON responses from AWS CLI, Docker, and Kubernetes using `jq` filters.
5. **Maintain Persistent Remote Sessions**: Operate reliably on remote production hosts using `tmux` sessions resilient to network drops.
