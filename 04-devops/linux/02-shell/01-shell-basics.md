<!--
================================================================================
Metadata:
  Title: Shell Basics: Terminals, Shells & Environments
  Track: Linux & Systems Engineering / DevOps
  Difficulty: Beginner
  Estimated Time: 45 minutes
  Prerequisites: 01-fundamentals/01-getting-started.md, 01-fundamentals/03-filesystem-hierarchy-and-navigation.md
  Key Terminology: Terminal, PTY, Shell, Bash, POSIX, Builtin, Subshell, Exit Code ($?), Environment Variables
================================================================================
-->

# Shell Basics: Terminals, Shells & Environments

[Back to DevOps Track Overview](../README.md) | [Back to Module Overview](./README.md) | [02-pipes-redirection-and-file-descriptors.md ->](./02-pipes-redirection-and-file-descriptors.md)

---

## Table of Contents
- [1. What & Why](#1-what--why)
- [2. Core Concepts](#2-core-concepts)
- [3. How It Works Internally](#3-how-it-works-internally)
- [4. Syntax & Usage](#4-syntax--usage)
- [5. Practical Examples](#5-practical-examples)
- [6. Best Practices](#6-best-practices)
- [7. Common Mistakes & Troubleshooting](#7-common-mistakes--troubleshooting)
- [8. Performance, Security & Cost Implications](#8-performance-security--cost-implications)
- [9. Comparison Table](#9-comparison-table)
- [10. Real-World Interview Questions](#10-real-world-interview-questions)
- [11. Practice Exercises](#11-practice-exercises)
- [12. Summary & Quick Reference Cheat Sheet](#12-summary--quick-reference-cheat-sheet)
- [13. Further Reading & Official Documentation](#13-further-reading--official-documentation)

---

## 1. What & Why

In modern Linux systems engineering and DevOps automation, the shell is the primary interface between human operators, automation agents (like CI/CD runners), and the operating system kernel. Every automated deployment script, Docker entrypoint container routine, Kubernetes liveness probe, and incident remediation runbook operates through the shell.

Understanding shell mechanics at a systems level is essential:
- A shell is not merely a prompt that executes binary applications; it is a full programming language runtime and process orchestrator that controls standard streams, forks child processes, establishes inter-process pipes, and handles system signals.
- Distinguishing between the graphical terminal emulator, the kernel pseudo-terminal (PTY) subsystem, and the shell interpreter resolves confusing behaviors around terminal freezing, broken character encodings, and background job disconnects.
- Differentiating between shell builtins (e.g., `cd`, `echo`, `kill`) and external binaries (e.g., `/usr/bin/find`, `/usr/bin/grep`) explains subtle performance differentials, execution environments, and security privilege boundaries.

Mastering shell fundamentals ensures that production automation runs reliably, predictably, and with robust error handling.

---

## 2. Core Concepts

### Terminal Emulator vs. PTY vs. Shell
Engineers frequently conflate the terminal, the pseudo-terminal, and the shell:
1. **Terminal Emulator**: A graphical or console client (e.g., Windows Terminal, iTerm2, Alacritty, GNOME Terminal) that captures keyboard keystrokes, handles graphical font rendering, and displays characters emitted by the operating system.
2. **Pseudo-Terminal (PTY)**: A bidirectional kernel software device pair consisting of a controller (master) and target (slave, `/dev/pts/N`). The PTY emulates a physical hardware teletypewriter, performing line editing, carriage return conversions, and signal translations (e.g., converting `Ctrl+C` into a `SIGINT` signal).
3. **Shell**: The command language interpreter (e.g., `bash`, `zsh`, `sh`) running inside the PTY slave. It parses command strings, performs word expansions, searches `$PATH`, and executes system calls like `fork()` and `execve()`.

```
+-------------------------------------------------------------------------+
| TERMINAL & SHELL SUBSYSTEM ARCHITECTURE                                  |
|                                                                         |
|  [ User Physical Interface ]                                            |
|        │ Keyboard Input / Video Display Output                           |
|        v                                                                |
|  [ Terminal Emulator (GNOME Terminal, Alacritty, Windows Terminal) ]    |
|        │ IPC via Unix Domain Socket / File Descriptor                    |
|        v                                                                |
|  [ Kernel Pseudo-Terminal Subsystem (PTY) ]                             |
|  ├── PTY Controller / Master (/dev/ptmx)                                |
|  │     │ Line discipline (canonical mode, echo, Ctrl+C -> SIGINT)       |
|  └── PTY Target / Slave (/dev/pts/1)                                    |
|        │ stdin (FD 0), stdout (FD 1), stderr (FD 2)                     |
|        v                                                                |
|  [ Shell Process: /bin/bash (PID 1042) ]                                |
|  ├── Parser & Lexer (Word splitting, tokenization)                      |
|  ├── Builtin Commands Table (cd, export, read, history)                 |
|  └── Process Spawner (fork() -> execve("/usr/bin/ls") -> waitpid())     |
+-------------------------------------------------------------------------+
```

### Anatomy of a Command
Every shell command line follows a structured grammatical structure:
```bash
ls -l --color=auto /var/log
│  │  │             └── Argument (Operand)
│  │  └── Long option (flag with parameter)
│  └── Short option (flag)
└── Command name
```
When you press `Enter`, the shell performs a sequence of operations before any code executes:
1. Reads input characters from `stdin`.
2. Splits the input line into tokens based on the Internal Field Separator (`$IFS`, by default space, tab, and newline).
3. Evaluates quotes, aliases, parameter expansions, command substitutions, arithmetic expansions, and filename globs.
4. Identifies command type: alias, function, builtin, or external executable found in `$PATH`.
5. Prepares I/O redirections (`>`, `<`, `|`).
6. Executes the command.

### Command Categories: `type -a`
Commands in Linux fall into four distinct categories:
- **Alias**: A user-defined shortcut for a longer command string (e.g., `alias ll='ls -la'`).
- **Function**: A shell-native function residing in memory.
- **Builtin**: A command implemented directly inside the shell binary itself (e.g., `cd`, `pwd`, `echo`, `exit`, `history`). Builtins execute within the current shell process; they do not fork.
- **External Binary**: An executable file located on the filesystem (e.g., `/usr/bin/grep`, `/bin/systemctl`). Running an external binary requires the shell to fork a child process and invoke `execve()`.

You can inspect the execution precedence with `type -a`:
```bash
type -a echo
# Output:
# echo is a shell builtin
# echo is /usr/bin/echo
```

---

## 3. How It Works Internally

When an external binary command (such as `ls -l /tmp`) is executed, the Linux shell and the kernel execute a coordinated sequence of POSIX system calls:

```mermaid
sequenceDiagram
    autonumber
    actor User as Operator / Terminal
    participant Shell as Bash Shell (PID 1000)
    participant Kernel as Linux Kernel (Syscalls)
    participant Child as Child Process (ls, PID 1001)

    User->>Shell: Types "ls -l /tmp" and presses Enter
    Shell->>Shell: Tokenizes input line & parses tokens
    Shell->>Shell: Looks up "ls" in hash table / searches $PATH
    Shell->>Kernel: fork() or clone() system call
    Kernel-->>Shell: Returns child PID 1001
    Kernel->>Child: Creates child address space (Copy-On-Write)
    Shell->>Kernel: waitpid(1001, &status, 0) [Shell blocks]
    Child->>Child: Configures file descriptors (0, 1, 2)
    Child->>Kernel: execve("/usr/bin/ls", ["ls", "-l", "/tmp"], environ)
    Kernel->>Child: Overwrites address space with /usr/bin/ls ELF binary
    Child->>User: Writes directory entries to stdout (FD 1)
    Child->>Kernel: exit_group(0)
    Kernel->>Child: Cleans up process memory (Enters Zombie State)
    Kernel->>Shell: Sends SIGCHLD signal & unblocks waitpid()
    Shell->>Shell: Harvests status: exit code 0, stores in $?
    Shell->>User: Renders next prompt ($PS1)
```

### Environment Inheritance and the Environment Pointer
Every Linux process maintains an array of strings called the **environment**:
- When a shell variable is declared without `export` (e.g., `PORT=8080`), it exists purely in the shell's internal hash table. Child processes spawned by the shell cannot see it.
- When `export PORT=8080` is executed, the shell places the variable into the `environ` pointer array (`char **environ`).
- When `execve()` runs, the kernel passes the parent's `environ` array to the child process. Any mutation made by the child to its own environment has zero effect on the parent shell.

---

## 4. Syntax & Usage

### Inspecting Shell State
```bash
# Verify the current active login shell defined in /etc/passwd
echo "$SHELL"

# Identify the currently executing shell interpreter process
echo "$0"

# Print the process ID (PID) of the current shell
echo "$$"

# Check the exit code of the last executed foreground command (0 = success)
echo "$?"
```

### Variable Declaration & Manipulation
```bash
# Correct: No spaces around '='
APP_NAME="orders-api"
APP_PORT=3000

# Incorrect: Spaces cause bash to treat APP_NAME as a command
# APP_NAME = "orders-api"

# Accessing variables: Use quotes and curly braces for safety
echo "Starting ${APP_NAME} on port ${APP_PORT}..."

# Transient environment variable (only visible to this single command)
NODE_ENV=production node server.js

# Exporting permanently to the shell's execution environment
export DATABASE_URL="postgresql://user:pass@127.0.0.1:5432/prod"
```

### Subshells vs. Current Shell Execution
```bash
# Parentheses run commands inside an isolated subshell (forked child)
(cd /tmp && touch testfile.txt)
pwd # Remains in original working directory!

# Curly braces run commands in the CURRENT shell context (requires spaces & trailing semicolon)
{ cd /tmp && touch testfile2.txt; }
pwd # Working directory has changed to /tmp!
```

---

## 5. Practical Examples

### 1. Basic: Determining Command Types and Search Paths
Inspect whether common commands are builtins, aliases, or disk binaries:
```bash
type -a cd
type -a ls
type -a which
type -a test
```
**Output**:
```text
cd is a shell builtin
ls is aliased to `ls --color=auto'
ls is /usr/bin/ls
which is /usr/bin/which
test is a shell builtin
test is /usr/bin/test
```
*Explanation*: Notice that `cd` has no disk binary in `/usr/bin/` because changing the working directory must mutate the current process's memory space via `chdir()`. An external binary `cd` would change its own directory and exit immediately without altering the parent shell.

### 2. Real-World: Testing Command Success via Exit Codes in CI/CD
In continuous integration pipelines, scripts must branch deterministically based on tool exit statuses:
```bash
# Check if PostgreSQL service is reachable on port 5432
nc -z -w 2 127.0.0.1 5432
STATUS=$?

if [ "$STATUS" -eq 0 ]; then
  echo "[INFO] PostgreSQL database socket is active and accepting connections."
else
  echo "[ERROR] Database port unreachable! Code: $STATUS" >&2
  exit 1
fi
```

### 3. Edge Case: Preserving Whitespace and Empty Values in Variables
Improperly quoted variables trigger word splitting:
```bash
LOG_FILES="access.log error.log debug.log"
SEARCH_QUERY="Critical Database Error"

# Incorrect: Unquoted variable undergoes word splitting and parameter expansion
grep $SEARCH_QUERY $LOG_FILES
# Evaluates to: grep Critical Database Error access.log error.log debug.log
# (Grep treats "Database", "Error", "access.log", etc., as target filenames)

# Correct: Double quotes preserve literal space characters
grep "$SEARCH_QUERY" access.log
```

### 4. Failure Case: Modifying Parent Environment from a Child Process
A common mistake among junior developers is attempting to export an environment variable from a script and expecting the calling shell to retain it:
```bash
# Contents of setup-env.sh
#!/usr/bin/env bash
export CLUSTER_NAME="prod-us-east-1"
echo "Inside script: CLUSTER_NAME=${CLUSTER_NAME}"
```
Executing via direct invocation:
```bash
chmod +x setup-env.sh
./setup-env.sh
echo "Parent shell: CLUSTER_NAME=${CLUSTER_NAME}"
```
**Output**:
```text
Inside script: CLUSTER_NAME=prod-us-east-1
Parent shell: CLUSTER_NAME=
```
*Remediation*: To execute a script inside the current shell without forking, use the `source` builtin or `.` notation:
```bash
source ./setup-env.sh
echo "Parent shell: CLUSTER_NAME=${CLUSTER_NAME}"
# Parent shell: CLUSTER_NAME=prod-us-east-1
```

### 5. Production-Grade: Safe Script Header and Diagnostic Trap
Every production automation script should initialize its execution environment with explicit safety flags and signal traps:
```bash
#!/usr/bin/env bash
# ==============================================================================
# Production Automation Bootstrap Script
# ==============================================================================
set -euo pipefail

# Define cleanup handler
cleanup() {
  local EXIT_CODE=$?
  if [ "$EXIT_CODE" -ne 0 ]; then
    echo "[CRITICAL] Script terminated abnormally with exit code: ${EXIT_CODE}" >&2
  else
    echo "[SUCCESS] Script execution completed successfully."
  fi
  # Remove temporary work directory if exists
  if [[ -n "${WORK_DIR:-}" && -d "$WORK_DIR" ]]; then
    rm -rf "$WORK_DIR"
  fi
}
trap cleanup EXIT

# Create atomic workspace
WORK_DIR=$(mktemp -d /tmp/deploy-XXXXXX)
echo "[INFO] Working in isolated directory: ${WORK_DIR}"

# Execute critical payload
echo "Deploying artifact..." > "${WORK_DIR}/deploy.log"
```

---

## 6. Best Practices

1. **Always Quote Variable Expansions**: Write `"$VAR"`, never `$VAR`. Without quotes, variable strings containing spaces, wildcards (`*`), or tabs undergo accidental word splitting and glob expansion.
2. **Use `#!/usr/bin/env bash` Instead of `#!/bin/bash`**: The location of the `bash` binary can vary across operating systems (e.g., `/bin/bash` on Debian/Ubuntu, `/usr/local/bin/bash` on FreeBSD or macOS via Homebrew). The `env` wrapper ensures portability.
3. **Use Meaningful Exit Codes**: Return standard POSIX exit codes: `0` for success, `1` for general application errors, `2` for syntax/argument misuse.
4. **Prefer `$(command)` Over Backticks `` `command` ``**: Command substitution with `$()` can be cleanly nested (e.g., `$(basename $(dirname "$PATH"))`) without convoluted backslash escaping.
5. **Differentiate Shell Scope**: Keep variables private to scripts unless they must be passed to child processes, in which case use explicit `export`.

---

## 7. Common Mistakes & Troubleshooting

| Mistake / Antipattern | Underlying Cause | Consequence | Production Solution |
| :--- | :--- | :--- | :--- |
| `VAR = "val"` (spaces around `=`) | Shell grammar expects variable assignments to be contiguous tokens. | Bash attempts to execute command `VAR` with arguments `=` and `val`. | Remove all spaces: `VAR="val"`. |
| Unquoted `$?` multi-check | Inspecting `$?` executes a command (e.g., `echo $?`), which immediately resets `$?` to `0`. | Subsequent checks evaluate the success of the `echo` statement. | Capture exit status immediately: `STATUS=$?` before running any other command. |
| Using `which` in scripts | `which` is an external binary with inconsistent output across distros and platforms. | May fail or emit non-standard return codes in Alpine/Debian slim images. | Use POSIX builtin `command -v <cmd>` instead. |
| Editing `.bashrc` for daemon environment | Non-interactive daemons (systemd, cron, SSH remote commands) do not source `.bashrc`. | Service crashes with "command not found" or missing environment variables. | Define variables in `/etc/environment` or systemd unit `Environment=` directives. |
| Neglecting `set -e` in deployments | Shell scripts continue executing sequentially even after a fatal command failure. | Deployments proceed despite failed compilation or missing downloads. | Prepend `set -euo pipefail` at the top of every automation script. |
| Subshell variable loss in pipelines | Piping into a `while` loop spawns a subshell: `cat file \| while read line; do count=$((count+1)); done`. | Variable mutations inside the loop are lost when the subshell terminates. | Use process substitution or redirection: `while read line; do ...; done < file`. |

---

## 8. Performance, Security & Cost Implications

### Performance
- **Builtins vs. External Binaries**: Executing a shell builtin takes microseconds because no new process is created. Invoking an external binary requires `fork()` and `execve()`, creating a new process control block (`task_struct`), loading ELF binary headers, resolving shared libraries via dynamic linker `ld-linux.so`, and performing page table initialization. In loops with $10^5$ iterations, using `echo` (builtin) vs `/usr/bin/printf` can mean the difference between 0.1s and 30s.

### Security
- **Command Injection**: Passing unvalidated user input into string-evaluated commands (e.g., `eval`, `sh -c "$USER_INPUT"`) enables remote code execution.
- **`$PATH` Hijacking**: If current directory `.` is placed first in `$PATH`, an adversary can place a malicious `ls` or `sl` binary in `/tmp` that executes when an administrator runs commands. Never put `.` in root's `$PATH`.

### Cost
- In cloud environments (AWS Lambda custom runtimes, Fargate containers), bloated shell scripts that repeatedly spawn hundreds of subshells consume unnecessary vCPU cycles, prolonging execution time and directly inflating monthly billing.

---

## 9. Comparison Table

| Attribute | Shell Builtin | External Binary | Shell Alias | Shell Function |
| :--- | :--- | :--- | :--- | :--- |
| **Execution Context** | Current shell process | Forked child process | Text substitution | Current shell process |
| **System Calls** | None (direct C function call) | `fork()`, `execve()`, `waitpid()` | Re-parsed as command | None (executes in RAM) |
| **Performance Overhead** | Sub-microsecond | Milliseconds (process setup) | Zero | Microsecond |
| **Alters Parent Shell State?**| Yes (e.g. `cd`, `export`) | No (child memory isolated) | Yes (executes builtins) | Yes (can mutate global variables) |
| **Availability in Scripts**| Always available | Available if in `$PATH` | Disabled in scripts by default| Available if defined in script |
| **Inspection Tool** | `type -t <cmd>` -> `builtin` | `type -t <cmd>` -> `file` | `type -t <cmd>` -> `alias` | `type -t <cmd>` -> `function` |

---

## 10. Real-World Interview Questions

### Q1: Why can `cd` never be implemented as an external binary executable?
<details>
<summary>Answer</summary>
In Unix process architecture, child processes inherit a copy of their parent's environment, memory, and file descriptor table, but a child process can **never** alter the execution state or current working directory (`CWD`) of its parent process. If `cd` were an external binary (e.g., `/usr/bin/cd`), executing `cd /var/log` would cause the shell to `fork()` a child process. The child would execute `chdir("/var/log")`, update its own working directory, and then exit. When the parent shell resumed execution, its working directory would remain unchanged. Therefore, `cd` must be implemented as a shell builtin that invokes the `chdir()` system call directly within the parent shell's own process.
</details>

### Q2: What is the exact difference between `sh`, `bash`, and `zsh`?
<details>
<summary>Answer</summary>
- **`sh` (Bourne Shell)**: Originally written by Steve Bourne in 1979. Modern systems link `/bin/sh` to a POSIX-compliant minimalist shell (such as `dash` on Ubuntu/Debian) designed for fast, lightweight script execution without advanced interactive conveniences.
- **`bash` (Bourne Again Shell)**: Authored by the GNU project to be fully POSIX-compliant while adding extensive extensions: associative arrays, parameter expansions (`${var//a/b}`), process substitution (`<()`), programmable auto-completion, and command history.
- **`zsh` (Z Shell)**: An extended shell incorporating features from Bash, Ksh, and tcsh, offering advanced interactive tab completion, themeable prompts (Oh-My-Zsh), and plugin architectures. Default shell on macOS since macOS Catalina.
</details>

### Q3: Explain what happens when a command line ends with `&` vs. `&&`.
<details>
<summary>Answer</summary>
- **`&` (Asynchronous / Background Operator)**: Instructs the shell to spawn the command in a child process and immediately return control to the prompt without calling `waitpid()`. The background process runs asynchronously while the parent shell accepts new input.
- **`&&` (Logical AND Operator)**: Executes the command on the right-hand side **only if** the command on the left-hand side returns an exit code of `0` (success). It is synchronous and sequential.
</details>

### Q4: How does the shell resolve a command when multiple entities share the same name?
<details>
<summary>Answer</summary>
The shell follows a strict order of precedence when evaluating command names:
1. **Aliases** (if enabled in the shell context).
2. **Special Builtins** (e.g., `break`, `continue`, `exit`, `return`, `eval`).
3. **Shell Functions** defined in the current shell session.
4. **Regular Builtins** (e.g., `cd`, `echo`, `kill`, `pwd`).
5. **External Binaries** located by iterating sequentially through directories listed in `$PATH` from left to right.
You can bypass aliases and functions to force execution of builtins via `builtin <cmd>`, or external binaries via `command <cmd>` or by specifying the absolute path (e.g., `/usr/bin/ls`).
</details>

### Q5: What does `echo $?` return after running `cat non_existent_file | grep "error"`? How do you inspect the exit status of both commands?
<details>
<summary>Answer</summary>
By default, the exit code `$?` of a pipeline reflects only the exit status of the **last command** in the pipeline (`grep`). Since `grep` received no input and found no match, it returns `1`. The failure of `cat` (exit code `1` or `2`) is masked.
To inspect the status of every command in the pipeline, Bash provides the special array variable `${PIPESTATUS[@]}`:
```bash
cat non_existent_file | grep "error"
echo "${PIPESTATUS[0]} ${PIPESTATUS[1]}"
# Output: 1 1 (or 2 1)
```
Setting `set -o pipefail` ensures that the pipeline as a whole fails if any command fails.
</details>

### Q6: What is a PTY, and why do container runners (like Docker or Kubernetes) require `-t` or `pty: true`?
<details>
<summary>Answer</summary>
A PTY (Pseudo-Terminal) is a software emulation of a hardware terminal device. It creates a master/slave character device pair (`/dev/ptmx` and `/dev/pts/N`). Standard Linux programs behave differently when connected to a terminal vs. a raw pipe or file:
- Standard I/O streams are line-buffered or unbuffered when connected to a PTY, but block-buffered (typically 4KB or 64KB) when connected to a pipe or file.
- Interactive programs like `vim`, `top`, `bash`, and `sudo` check `isatty(STDIN_FILENO)`. If false, they refuse to launch interactive full-screen curses interfaces or prompt for passwords.
Container runners use `-t` to allocate a PTY slave inside the container, allowing color rendering, dynamic terminal window resizing (`SIGWINCH`), and line editing.
</details>

### Q7: What is the danger of executing `export PATH=$PATH:.`?
<details>
<summary>Answer</summary>
Appending or prepending `.` (the current directory) to `$PATH` introduces a severe local privilege escalation vulnerability. If an engineer navigates to `/tmp`, `/var/tmp`, or a world-writable shared directory and executes a common command (e.g., `ls` or `git`), the shell will search `.` either first (if prepended) or last (if appended). A malicious local user could deposit an executable script named `ls` or `sl` in `/tmp` containing malicious payload. When the administrator runs `ls`, the attacker's script executes with the administrator's privileges.
</details>

### Q8: What is the difference between login shells, non-login interactive shells, and non-interactive shells?
<details>
<summary>Answer</summary>
1. **Login Shell**: Started after successful authentication (SSH login or console `login`). Sources `/etc/profile`, then the first found of `~/.bash_profile`, `~/.bash_login`, or `~/.profile`.
2. **Interactive Non-Login Shell**: Started from an already logged-in session (e.g., opening a new terminal window or running `bash`). Sources `/etc/bash.bashrc` and `~/.bashrc`. Does not parse `/etc/profile`.
3. **Non-Interactive Shell**: Started to run a script (e.g., `./script.sh` or automated cron/CI job). Does not prompt for input. Does not source `~/.bashrc` unless the `$BASH_ENV` variable is set. Inherits exported environment variables from the parent process.
</details>

---

## 11. Practice Exercises

### Exercise 1: Identify Command Execution Types (Easy)
**Task**: Write a shell command pipeline that scans a list of common commands (`cd`, `ls`, `grep`, `pwd`, `export`, `git`, `ifconfig`, `ip`) and outputs a clean two-column table indicating whether each item is a `builtin`, `alias`, `file`, or `not found`.

<details>
<summary>Solution</summary>

```bash
#!/usr/bin/env bash
for cmd in cd ls grep pwd export git ifconfig ip non_existent_tool; do
  cmd_type=$(type -t "$cmd" 2>/dev/null || echo "not found")
  printf "%-20s %-15s\n" "$cmd" "$cmd_type"
done
```
</details>

### Exercise 2: Tracing Subshell Environment Isolation (Easy)
**Task**: Demonstrate that an environment variable exported inside a subshell cannot leak into the parent environment, whereas a variable set inside a grouped command block (`{ ...; }`) persists.

<details>
<summary>Solution</summary>

```bash
#!/usr/bin/env bash
# Subshell test
( export SUBSHELL_VAR="isolated" )
echo "Subshell variable value: '${SUBSHELL_VAR:-UNSET}'"

# Current shell block test
{ export BLOCK_VAR="persisted"; }
echo "Block variable value: '${BLOCK_VAR:-UNSET}'"
```
**Expected Output**:
```text
Subshell variable value: 'UNSET'
Block variable value: 'persisted'
```
</details>

### Exercise 3: Inspecting Process Environment via `/proc` (Medium)
**Task**: Start a background process that sleeps for 60 seconds with a custom environment variable `SECRET_TOKEN="alpha-99"`. Without using `printenv`, extract and print that environment variable directly from the Linux `/proc` virtual filesystem using the process's PID.

<details>
<summary>Solution</summary>

```bash
#!/usr/bin/env bash
# Start background process with transient env variable
SECRET_TOKEN="alpha-99" sleep 60 &
TARGET_PID=$!

echo "[INFO] Started sleep process PID: ${TARGET_PID}"

# In /proc/<PID>/environ, variables are null-byte delimited (\0)
tr '\0' '\n' < "/proc/${TARGET_PID}/environ" | grep "^SECRET_TOKEN="

# Clean up
kill "$TARGET_PID" 2>/dev/null || true
```
</details>

### Exercise 4: Pipeline Failure Isolation with PIPESTATUS (Medium)
**Task**: Construct a 3-stage command pipeline (`cat`, `awk`, `grep`) where the intermediate `awk` command fails intentionally (e.g., division by zero or invalid syntax). Capture and print the individual exit codes of all 3 pipeline stages without letting the script crash prematurely.

<details>
<summary>Solution</summary>

```bash
#!/usr/bin/env bash
echo -e "apple\nbanana\ncherry" | awk '{ if (NR==2) exit 42; print }' | grep "apple" || true

STATUSES=("${PIPESTATUS[@]}")
echo "Pipeline exit codes:"
echo "Stage 1 (echo): ${STATUSES[0]}"
echo "Stage 2 (awk):  ${STATUSES[1]}"
echo "Stage 3 (grep): ${STATUSES[2]}"
```
</details>

### Exercise 5: Building a Robust Command Runner Function (Hard)
**Task**: Author a reusable bash function named `run_with_retry` that executes any arbitrary command passed as arguments. It must retry up to 3 times with exponential backoff (1s, 2s, 4s) if the command fails, logging timestamps and exit codes to `stderr`.

<details>
<summary>Solution</summary>

```bash
#!/usr/bin/env bash
set -euo pipefail

run_with_retry() {
  local max_attempts=3
  local timeout=1
  local attempt=1
  local exit_code=0

  until "$@"; do
    exit_code=$?
    echo "[$(date -u +'%Y-%m-%dT%H:%M:%SZ')] [ERROR] Command '$*' failed with exit code ${exit_code}. Attempt ${attempt}/${max_attempts}." >&2
    if (( attempt >= max_attempts )); then
      echo "[$(date -u +'%Y-%m-%dT%H:%M:%SZ')] [FATAL] Max retries exceeded for '$*'." >&2
      return "$exit_code"
    fi
    sleep "$timeout"
    timeout=$(( timeout * 2 ))
    attempt=$(( attempt + 1 ))
  done
  echo "[$(date -u +'%Y-%m-%dT%H:%M:%SZ')] [SUCCESS] Command '$*' executed successfully." >&2
}

# Test execution:
run_with_retry true
# run_with_retry false # (Will retry 3 times and return code 1)
```
</details>

### Exercise 6: Automated PTY Detection and Output Adaptation (Hard)
**Task**: Author a script that adapts its output format dynamically: if stdout is connected to an interactive terminal (`isatty`), render bold green color-coded text; if stdout is redirected to a file or pipe, strip all ANSI color escape sequences automatically.

<details>
<summary>Solution</summary>

```bash
#!/usr/bin/env bash
set -euo pipefail

if [[ -t 1 ]]; then
  # File descriptor 1 is an interactive terminal (PTY)
  COLOR_GREEN="\033[0;32m"
  COLOR_RESET="\033[0m"
else
  # File descriptor 1 is a file, pipe, or non-terminal sink
  COLOR_GREEN=""
  COLOR_RESET=""
fi

log_status() {
  local message="$1"
  echo -e "${COLOR_GREEN}[STATUS] ${message}${COLOR_RESET}"
}

log_status "Database backup completed successfully."
```
</details>

---

## 12. Summary & Quick Reference Cheat Sheet

| Task | Command / Syntax | Operational Context |
| :--- | :--- | :--- |
| **Inspect Command Nature** | `type -a <cmd>` | Shows whether command is an alias, builtin, or disk file |
| **Bypass Aliases & Functions**| `command <cmd>` / `\cmd` | Executes disk binary directly without alias interference |
| **Check Last Exit Status** | `echo $?` | `0` denotes success, non-zero denotes error |
| **Capture Pipeline Exit Codes**| `echo "${PIPESTATUS[@]}"` | Array of exit codes for every stage in preceding pipeline |
| **Transient Env Variable** | `VAR="val" <command>` | Sets environment variable exclusively for that command |
| **Export Variable** | `export VAR="val"` | Makes variable visible to all descendant child processes |
| **Check Current Process PID** | `echo $$` | Returns PID of current shell instance |
| **Check Parent Process PID** | `echo $PPID` | Returns PID of parent process |
| **Detect Terminal Connection**| `[[ -t 1 ]]` | True if stdout (FD 1) is attached to a terminal/PTY |
| **Run in Subshell** | `(cd /tmp && make)` | Executes in isolated child process without mutating parent |

---

## 13. Further Reading & Official Documentation

- [GNU Bash Reference Manual](https://www.gnu.org/software/bash/manual/bash.html)
- [The Open Group Base Specifications Issue 7 - Shell Command Language](https://pubs.opengroup.org/onlinepubs/9699919799/utilities/V3_chap02.html)
- [Brendan Gregg: Linux Performance - Systems Diagnostics](https://www.brendangregg.com/linuxperf.html)
- [Linux Man Pages: bash(1)](https://man7.org/linux/man-pages/man1/bash.1.html)
- [Linux Man Pages: pty(7)](https://man7.org/linux/man-pages/man7/pty.7.html)

---

[Back to DevOps Track Overview](../README.md) | [Back to Module Overview](./README.md) | [02-pipes-redirection-and-file-descriptors.md ->](./02-pipes-redirection-and-file-descriptors.md)
