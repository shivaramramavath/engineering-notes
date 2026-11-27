<!--
================================================================================
Metadata:
  Title: Bash Scripting: Control Structures, Functions & Traps
  Track: Linux & Systems Engineering / DevOps
  Difficulty: Intermediate
  Estimated Time: 60 minutes
  Prerequisites: 02-shell/01-shell-basics.md, 02-shell/02-pipes-redirection-and-file-descriptors.md
  Key Terminology: Shebang, Control Structures (if/then/else, case, for, while), Functions, Variable Scope (local), Signal Traps (trap), Signal Handling (SIGINT, SIGTERM, EXIT, ERR), Subshells, Return vs Exit
================================================================================
-->

# Bash Scripting: Control Structures, Functions & Traps

[<- 06-jq-and-structured-data.md](./06-jq-and-structured-data.md) | [Back to Module Overview](./README.md) | [08-bash-scripting-advanced.md ->](./08-bash-scripting-advanced.md)

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

In production systems administration and site reliability engineering, shell scripts serve as the glue that binds independent software components together. They orchestrate system bootstrapping (`cloud-init`), perform database backup rotations, execute container healthchecks, and automate multi-step deployment pipelines.

However, writing shell scripts that are suitable for mission-critical enterprise environments requires moving far beyond basic command sequences:
- Naive scripts fail silently when commands fail, continuing to execute subsequent steps and potentially corrupting storage or applying bad configurations.
- Unhandled aborts (e.g., operator pressing `Ctrl+C` or a SIGTERM from an autoscaler) leave stale lockfiles, orphaned processes, and dangling temporary files on disk.
- Unscoped global variables cause subtle side effects across functions that are notoriously difficult to debug.

Mastering structured control flows, modular function scoping, and defensive signal trapping enables engineers to write bash automation that is as robust, maintainable, and resilient as compiled application code.

---

## 2. Core Concepts

### The Shebang (`#!`)
The first line of an executable script instructs the Linux kernel's ELF binary loader which interpreter to spawn:
```bash
#!/usr/bin/env bash
```
When `execve()` executes a file that begins with the magic byte sequence `0x23 0x21` (`#!`), the kernel suspends normal binary loading, reads the path to the interpreter, and spawns that interpreter with the script path passed as an argument. Using `#!/usr/bin/env bash` ensures cross-platform portability across environments where `bash` is not located at `/bin/bash` (e.g., FreeBSD, NixOS, or macOS).

### Control Flow Constructs
Bash provides structured control statements:
- **Conditionals (`if`, `then`, `elif`, `else`, `fi`)**: Evaluates the exit status of a command or test expression (`[[ ... ]]`).
- **Pattern Branching (`case ... in ... esac`)**: Performs high-speed glob pattern matching against strings; clean alternative to cascading `if-elif` chains.
- **Iteration Loops**:
  - `for var in "${list[@]}"; do ... done`: Iterates over discrete items or arrays.
  - `for ((i=0; i<N; i++)); do ... done`: C-style arithmetic iteration loop.
  - `while condition; do ... done`: Loops as long as `condition` returns exit status 0 (success).
  - `until condition; do ... done`: Loops as long as `condition` returns non-zero (failure).

### Functions and Variable Scope
Functions in Bash are declared via `func_name() { ... }`. By default, **all variables declared in Bash functions are global**. To prevent variable pollution, variables within functions must be explicitly scoped using the `local` keyword.

### Signals and the `trap` Builtin
Operating systems communicate with processes using asynchronous software interrupts called **Signals** (e.g., `SIGINT` from `Ctrl+C`, `SIGTERM` on shutdown).
The `trap` builtin intercepts these signals and executes cleanup routines before the script terminates:

```
+-------------------------------------------------------------------------+
| SIGNAL TRAP & CLEANUP LIFECYCLE                                         |
|                                                                         |
|  [ Script Starts ]                                                      |
|         │                                                               |
|         v                                                               |
|  [ Register Handler ]: trap cleanup_handler EXIT SIGINT SIGTERM         |
|         │                                                               |
|         ├───> [ Execution Phase ]: Processing files, creating /tmp/lock  |
|         │           │                                                   |
|         │           ├──── Abnormal Termination (Ctrl+C / Kill / Error)   |
|         │           │             │                                     |
|         │           v             v                                     |
|         │     [ Signal Caught by Kernel ]                               |
|         │     Kernel pauses execution and passes control to trap handler|
|         │                         │                                     |
|         v                         v                                     |
|  [ Normal Exit ] ─────────> [ cleanup_handler() ]                        |
|                             ├── Removes /tmp/lock                       |
|                             ├── Flushes buffers                         |
|                             └── Releases resources                      |
|                                   │                                     |
|                                   v                                     |
|                             [ Process Terminates Cleanly ]              |
+-------------------------------------------------------------------------+
```

---

## 3. How It Works Internally

When a bash script executes a function or loop, the internal state machine maintains stack frames and traps inside the shell process memory:

```mermaid
sequenceDiagram
    autonumber
    actor OS as Linux Kernel / Autoscaler
    participant Shell as Bash Process (PID 2048)
    participant Trap as Trap Handler Stack
    participant FS as Host Filesystem

    Note over Shell: trap cleanup EXIT SIGTERM
    Shell->>FS: touch /var/run/deploy.lock (Creates Lock)
    Shell->>Shell: Executes heavy deployment workload...
    OS->>Shell: Delivers SIGTERM (15) (e.g., Spot instance draining)
    Shell->>Trap: Interrupts main loop; invokes cleanup()
    Trap->>FS: rm -f /var/run/deploy.lock
    Trap->>Shell: Emits log: "[INFO] Cleanup completed."
    Trap-->>Shell: Exits with preserved signal code (128 + 15 = 143)
    Shell->>OS: Process exits cleanly (Zero dangling lockfiles)
```

### The Difference Between `return` and `exit`
- **`return N`**: Exits the **currently executing function**, passing status code `N` back to the caller. Does not terminate the script.
- **`exit N`**: Completely terminates the **entire shell process** immediately, returning exit code `N` to the parent process and firing any registered `EXIT` traps.

---

## 4. Syntax & Usage

### 1. Conditionals (`if` and `[[ ... ]]`)
Always prefer `[[ ... ]]` over legacy `[ ... ]` in Bash scripts:
```bash
if [[ -f "/etc/app.conf" && ! -s "/etc/app.conf" ]]; then
  echo "Configuration file exists but is 0 bytes (empty)!"
elif [[ -d "/etc/app.d" ]]; then
  echo "Configuration directory exists."
else
  echo "No configuration found."
  exit 1
fi

# Numerical Comparisons: -eq, -ne, -lt, -le, -gt, -ge
COUNT=5
if [[ "$COUNT" -ge 5 ]]; then
  echo "Threshold reached."
fi

# String Comparisons: ==, !=, =~ (regex)
SERVICE_STATE="active (running)"
if [[ "$SERVICE_STATE" =~ running ]]; then
  echo "Service is operational."
fi
```

### 2. Pattern Matching with `case`
```bash
case "$ENVIRONMENT" in
  production|prod)
    DB_HOST="db-prod.internal"
    REPLICAS=5
    ;;
  staging|stage)
    DB_HOST="db-stage.internal"
    REPLICAS=2
    ;;
  development|dev|*)
    DB_HOST="127.0.0.1"
    REPLICAS=1
    ;;
esac
```

### 3. Iteration Loops
```bash
# For loop over array
REGIONS=("us-east-1" "us-west-2" "eu-west-1")
for region in "${REGIONS[@]}"; do
  echo "Deploying to region: ${region}"
done

# While loop reading a file line-by-line safely
while IFS= read -r line || [[ -n "$line" ]]; do
  echo "Processing entry: $line"
done < "/etc/hosts"

# While loop with counter
RETRY_COUNT=0
while [[ $RETRY_COUNT -lt 3 ]]; do
  if ping -c 1 -W 1 1.1.1.1 > /dev/null 2>&1; then
    echo "Gateway reachable."
    break
  fi
  ((RETRY_COUNT++))
  sleep 2
done
```

### 4. Function Scoping and Argument Handling
```bash
deploy_service() {
  local service_name="$1"
  local version="${2:-latest}" # Default parameter
  local exit_status=0
  
  echo "Deploying ${service_name} at version ${version}..."
  
  # Return status code (0 = success, 1 = failure)
  return "$exit_status"
}

# Invoking function
deploy_service "auth-service" "v2.1.0"
```

---

## 5. Practical Examples

### 1. Basic: Parsing Command-Line Options with `getopts`
Standardize CLI argument parsing using the POSIX `getopts` builtin:
```bash
#!/usr/bin/env bash
set -euo pipefail

usage() {
  echo "Usage: $0 -h <host> -p <port> [-v]" >&2
  exit 1
}

VERBOSE=false
HOST=""
PORT=""

while getopts "h:p:v" opt; do
  case "$opt" in
    h) HOST="$OPTARG" ;;
    p) PORT="$OPTARG" ;;
    v) VERBOSE=true ;;
    *) usage ;;
  esac
done

if [[ -z "$HOST" || -z "$PORT" ]]; then
  echo "[ERROR] Missing required arguments." >&2
  usage
fi

echo "Connecting to ${HOST}:${PORT} (Verbose: ${VERBOSE})"
```

### 2. Real-World: Service Health Poller with Timeout and Exponential Backoff
A production-ready daemon checker polling an API endpoint until healthy:
```bash
#!/usr/bin/env bash
set -euo pipefail

check_health() {
  local endpoint="$1"
  local max_retries="${2:-5}"
  local delay=1
  local attempt=1

  echo "[INFO] Polling ${endpoint} for health check..."

  until curl -sf "${endpoint}/health" > /dev/null; do
    if (( attempt >= max_retries )); then
      echo "[ERROR] Service failed health check after ${max_retries} attempts." >&2
      return 1
    fi
    echo "[WARN] Health check failed (attempt ${attempt}/${max_retries}). Retrying in ${delay}s..."
    sleep "$delay"
    delay=$(( delay * 2 )) # Exponential backoff
    ((attempt++))
  done

  echo "[SUCCESS] Service is healthy!"
  return 0
}

check_health "http://127.0.0.1:8080" 4
```

### 3. Edge Case: Proper Line-by-Line File Reading
Naive `for line in $(cat file)` fails on lines containing spaces and ignores the last line if it lacks a trailing newline:
```bash
# WRONG: Word splits on every space and tab
for line in $(cat /etc/resolv.conf); do
  echo "$line"
done

# CORRECT: Preserves whitespace and processes trailing lines lacking newlines
while IFS= read -r line || [[ -n "$line" ]]; do
  # Skip comments and empty lines
  [[ "$line" =~ ^[[:space:]]*(#|$) ]] && continue
  echo "Nameserver configured: $line"
done < /etc/resolv.conf
```

### 4. Failure Case: The Accidental Global Variable Leak
Omitting `local` inside functions leads to silent data corruption in nested operations:
```bash
# BUGGY CODE:
traverse_tree() {
  local dir="$1"
  for file in "$dir"/*; do # 'file' is GLOBAL!
    if [[ -d "$file" ]]; then
      traverse_tree "$file" # Nested call overwrites the global 'file' variable!
    fi
    echo "Processed: $file"
  done
}

# FIXED CODE:
traverse_tree_safe() {
  local dir="$1"
  local file # Explicitly scoped to this invocation stack frame
  for file in "$dir"/*; do
    if [[ -d "$file" ]]; then
      traverse_tree_safe "$file"
    fi
    echo "Processed: $file"
  done
}
```

### 5. Production-Grade: Resilient Atomic Mutex with Exit Traps
In production, running cron jobs or deployment scripts concurrently must be prevented via an atomic mutex (lockfile). This script guarantees lock release on any termination condition:
```bash
#!/usr/bin/env bash
set -euo pipefail

LOCK_DIR="/var/run/backup_job.lock"

# Cleanup function triggered on EXIT, SIGINT, SIGTERM
cleanup() {
  local exit_code=$?
  if [[ -d "$LOCK_DIR" ]]; then
    rmdir "$LOCK_DIR" 2>/dev/null || true
    echo "[INFO] Mutex lock released."
  fi
  exit "$exit_code"
}
trap cleanup EXIT INT TERM

# Atomic lock acquisition using mkdir (mkdir is an atomic POSIX system call)
if ! mkdir "$LOCK_DIR" 2>/dev/null; then
  echo "[FATAL] Job is already running (Lock exists: ${LOCK_DIR}). Aborting." >&2
  exit 1
fi

echo "[INFO] Mutex lock acquired. Starting backup routine..."
# Simulate workload
sleep 3
echo "[INFO] Backup payload completed."
```

---

## 6. Best Practices

1. **Always Use `local` in Functions**: Never declare a variable inside a function without `local` unless your explicit intention is to mutate a global state.
2. **Handle Signals Explicitly**: Register a `trap` for `EXIT`, `INT` (Ctrl+C), and `TERM` (kill) to clean up temporary directories, sockets, and lockfiles.
3. **Use POSIX `getopts` for CLI Tools**: Provide standard `-h`, `-v`, and argument flags rather than inspecting positional parameters manually ($1, $2).
4. **Use `[[ ... ]]` for Compound Tests**: Avoid legacy `[` and `test`. The modern `[[ ... ]]` syntax supports regex matching (`=~`), pattern matching, and prevents word splitting errors.
5. **Always Quote Array Expansions**: Use `"${ARRAY[@]}"` (with quotes) when passing arrays to functions or loops to preserve spaces within array elements.

---

## 7. Common Mistakes & Troubleshooting

| Mistake / Antipattern | Underlying Cause | Consequence | Production Solution |
| :--- | :--- | :--- | :--- |
| `for line in $(cat file)` | Word splitting decomposes input by space, tab, and newline. | Lines with spaces are chopped into multiple loop iterations. | Use `while IFS= read -r line; do ... done < file`. |
| Omitting `local` in recursive functions | All variables in Bash functions are global by default. | Recursive calls overwrite parent loop counters or variables. | Declare all internal variables with `local var=...`. |
| Missing `EXIT` in trap | Trapping only `SIGINT` misses normal script exits or `SIGTERM`. | Lockfiles or temporary folders are left behind on clean exit or kill. | Trap multiple events: `trap cleanup EXIT INT TERM`. |
| `return` used outside function | Calling `return` in the main script body instead of `exit`. | Bash returns syntax error: `return: can only `return' from a function or sourced script`. | Use `exit <code >` in top-level script bodies. |
| Using `[ ... ]` with unquoted variables | Legacy test command undergoes word splitting. | `unary operator expected` syntax errors when variables are empty. | Use `[[ -n "$VAR" ]]`. |
| Reading piped input into while loop: `cat file \| while read line` | The pipe `\|` executes the `while` loop inside a subshell. | Variable changes inside the loop are lost when the loop ends. | Redirect input into loop: `while read line; do ... done < file`. |

---

## 8. Performance, Security & Cost Implications

### Performance
- **Subshell Spawning in Loops**: Spawning commands in subshells inside tight loops (e.g., `for i in {1..1000}; do date_val=$(date); done`) forces 1,000 `fork()` and `execve()` calls. Keep operations in native Bash memory whenever possible.
- **`mkdir` vs `touch` for Locks**: Creating an atomic lock with `mkdir` avoids race conditions. `touch` requires a separate existence check (`if [ -f lock ]`), creating a Time-of-Check to Time-of-Use (TOCTOU) race condition.

### Security
- **Command Injection via Dynamic Evaluation (`eval`)**: Never pass user input to `eval` or dynamic command strings (`sh -c "$INPUT"`).
- **Environment Variable Poisoning**: In scripts executing as root via sudo or cron, set `$PATH` explicitly at the start of the script: `PATH="/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin"`.

### Cost
- In cloud autoscale environments, scripts that fail to trap `SIGTERM` prevent instances from draining cleanly before VM termination, causing dropped user requests and SLA violations.

---

## 9. Comparison Table

| Feature / Construct | `[ ... ]` (Test) | `[[ ... ]]` (Bash Compound) | `(( ... ))` (Arithmetic) |
| :--- | :--- | :--- | :--- |
| **Origin** | POSIX Standard / Builtin | Bash / Zsh Keyword | Bash / Korn Shell Keyword |
| **Word Splitting** | Performed on variables | **Suppressed** | Not applicable |
| **Filename Globbing**| Performed on arguments | **Suppressed** | Not applicable |
| **Regular Expressions**| Not supported | **Supported (`=~`)** | Not applicable |
| **Logical Operators**| `-a` (AND), `-o` (OR) | `&&` (AND), `\|\|` (OR) | `&&`, `\|\|` |
| **Comparisons** | String (`=`), Numeric (`-eq`)| String (`==`), Regex (`=~`) | Arithmetic (`==`, `>`, `<`) |

---

## 10. Real-World Interview Questions

### Q1: What is the exact difference between `EXIT` and `ERR` signals in Bash traps?
<details>
<summary>Answer</summary>
- **`EXIT`**: A pseudo-signal triggered whenever the shell process exits for **any reason**: successful completion, explicit call to `exit`, script abort via `set -e`, or termination signals. It is the primary signal used for deterministic resource cleanups.
- **`ERR`**: A pseudo-signal triggered **only when a command exits with a non-zero exit status**. It is typically used for error diagnostics, alerting, or logging stack traces before the script crashes. Note that `ERR` is not inherited by shell functions or subshells unless `set -o errtrace` (`set -E`) is enabled.
</details>

### Q2: Why does modifying a variable inside a `while` loop that receives data via a pipe (`cat file | while read ...`) fail to update the variable outside the loop?
<details>
<summary>Answer</summary>
In Bash, each component of a pipeline (`cmd1 | cmd2`) executes within a **separate subshell** (a forked child process). When you pipe data into a `while` loop, the entire `while` loop runs in a child subshell. Any mutations made to variables inside the loop occur in the child's isolated memory space. When the loop finishes, the child subshell exits, and the parent shell retains its original unmodified variable value.
**Fix**: Avoid the pipe by using input redirection:
```bash
while read -r line; do
  ((count++))
done < file
echo "Total lines: $count" # Count persists!
```
</details>

### Q3: What is the purpose of `local -r` in a Bash function?
<details>
<summary>Answer</summary>
`local -r var="value"` creates a **read-only local variable** scoped strictly to the function. Any attempt to modify or unset the variable within that function triggers a runtime error. This enforces immutability for critical internal configuration constants.
</details>

### Q4: How does `getopts` track its position across command line arguments?
<details>
<summary>Answer</summary>
`getopts` uses two internal shell variables:
- **`$OPTIND`**: The index of the next argument to be processed. It is initialized to `1` when the shell starts and incremented automatically each time `getopts` runs.
- **`$OPTARG`**: Stores the argument value for options that require a parameter (specified by a trailing colon `:` in the option string, e.g., `h:p:`).
After the `while getopts` loop finishes, engineers commonly run `shift $((OPTIND - 1))` to discard parsed options and leave only trailing positional arguments in `$1`, `$2`, etc.
</details>

### Q5: What is the difference between `trap '' SIGINT` and `trap - SIGINT`?
<details>
<summary>Answer</summary>
- **`trap '' SIGINT`** (Empty string argument): Instructs the shell to **completely ignore** the `SIGINT` signal. Pressing `Ctrl+C` will have zero effect on the script.
- **`trap - SIGINT`** (Dash argument): **Resets** the signal disposition back to its default system action (which for `SIGINT` is abnormal process termination).
</details>

### Q6: Why is `mkdir` preferred over `touch` for implementing atomic lockfiles in shell scripts?
<details>
<summary>Answer</summary>
`mkdir` is implemented via the POSIX `mkdir()` system call, which is guaranteed to be **atomic** at the filesystem level. If two concurrent processes execute `mkdir /tmp/lock` at the exact same microsecond, exactly one process will succeed with exit code `0`, and the other will fail with `EEXIST` (File exists).
In contrast, implementing a lock with `touch` requires first checking if the file exists (`[ ! -f lock ]`) and then creating it (`touch lock`). The time gap between checking and creating introduces a race condition (TOCTOU) where both processes can see the file as missing and both create it simultaneously.
</details>

### Q7: How can you capture a function's return string without using subshell command substitution `$(...)`?
<details>
<summary>Answer</summary>
Command substitution `RESULT=$(my_func)` spawns a subshell, incurring a `fork()` overhead. To return a string from a function without forking, pass a variable name by reference using `local -n` (Bash 4.3+):
```bash
get_cluster_endpoint() {
  local -n out_var="$1" # Nameref variable
  out_var="https://k8s-prod.internal:6443"
}

get_cluster_endpoint ENDPOINT
echo "Endpoint: $ENDPOINT"
```
This directly mutates the caller's variable in the same process with zero subshell overhead.
</details>

### Q8: What does `set -E` (`set -o errtrace`) do?
<details>
<summary>Answer</summary>
By default in Bash, `trap ... ERR` handlers are not inherited by shell functions, command substitutions, or subshells. Enabling `set -E` ensures that any active `ERR` trap is inherited by all functions, command substitutions, and subshells, allowing centralized error handling across the entire codebase.
</details>

---

## 11. Practice Exercises

### Exercise 1: Argument Validation with Getopts (Easy)
**Task**: Write a script `backup.sh` that accepts `-s <source_dir>`, `-d <dest_dir>`, and optional `-c` (compression enabled). If either `-s` or `-d` is omitted, display usage and exit with code 1.

<details>
<summary>Solution</summary>

```bash
#!/usr/bin/env bash
set -euo pipefail

usage() {
  echo "Usage: $0 -s <source> -d <destination> [-c]" >&2
  exit 1
}

SOURCE=""
DEST=""
COMPRESS=false

while getopts "s:d:c" opt; do
  case "$opt" in
    s) SOURCE="$OPTARG" ;;
    d) DEST="$OPTARG" ;;
    c) COMPRESS=true ;;
    *) usage ;;
  esac
done

[[ -z "$SOURCE" || -z "$DEST" ]] && usage

echo "Backing up '$SOURCE' to '$DEST' (Compression: $COMPRESS)"
```
</details>

### Exercise 2: Self-Healing Temporary Directory Trap (Easy)
**Task**: Author a script that creates a temporary directory via `mktemp -d /tmp/work.XXXXXX`. Register a trap that ensures the directory is completely deleted when the script finishes or is killed via `Ctrl+C`.

<details>
<summary>Solution</summary>

```bash
#!/usr/bin/env bash
set -euo pipefail

WORK_DIR=$(mktemp -d /tmp/work.XXXXXX)

cleanup() {
  echo "[INFO] Cleaning up temporary workspace: $WORK_DIR"
  rm -rf "$WORK_DIR"
}
trap cleanup EXIT INT TERM

echo "Working inside $WORK_DIR..."
touch "$WORK_DIR/payload.bin"
sleep 2
```
</details>

### Exercise 3: Safe File Reader with Checksum Validation (Medium)
**Task**: Write a function `verify_file <file> <expected_sha256>` that checks if the file exists and is readable, computes its SHA-256 hash using `sha256sum`, and returns 0 if it matches or 1 if it fails or does not exist.

<details>
<summary>Solution</summary>

```bash
#!/usr/bin/env bash
verify_file() {
  local file="$1"
  local expected="$2"

  if [[ ! -f "$file" || ! -r "$file" ]]; then
    echo "[ERROR] File '$file' is missing or unreadable." >&2
    return 1
  fi

  local actual
  actual=$(sha256sum "$file" | awk '{print $1}')

  if [[ "$actual" == "$expected" ]]; then
    echo "[MATCH] Checksum verified: $file"
    return 0
  else
    echo "[MISMATCH] Expected $expected, got $actual" >&2
    return 1
  fi
}
```
</details>

### Exercise 4: Parallel Worker Pool with Wait (Medium)
**Task**: Write a loop that spawns 5 background child processes simultaneously, each sleeping for a random duration (1 to 3 seconds) and emitting a completion message. Use `wait` to ensure the parent script does not exit until all 5 children have finished.

<details>
<summary>Solution</summary>

```bash
#!/usr/bin/env bash
set -euo pipefail

worker() {
  local id="$1"
  local duration=$(( 1 + RANDOM % 3 ))
  echo "[WORKER $id] Sleeping for ${duration}s..."
  sleep "$duration"
  echo "[WORKER $id] Done."
}

PIDS=()
for i in {1..5}; do
  worker "$i" &
  PIDS+=("$!")
done

echo "[PARENT] Waiting for ${#PIDS[@]} workers to finish..."
wait "${PIDS[@]}"
echo "[PARENT] All background workers completed successfully."
```
</details>

### Exercise 5: Generic Circuit Breaker Pattern (Hard)
**Task**: Author a function `circuit_breaker <max_failures> <command...>` that wraps the execution of another command. If the wrapped command fails consecutively `max_failures` times, trip the circuit and refuse subsequent executions for 30 seconds.

<details>
<summary>Solution</summary>

```bash
#!/usr/bin/env bash
STATE_FILE="/tmp/circuit_breaker.state"

circuit_breaker() {
  local max_failures="$1"
  shift
  local now
  now=$(date +%s)

  # Check if circuit is open
  if [[ -f "$STATE_FILE" ]]; then
    local fail_count last_fail
    read -r fail_count last_fail < "$STATE_FILE"
    if (( fail_count >= max_failures && (now - last_fail) < 30 )); then
      echo "[CIRCUIT OPEN] Service unavailable. Try again in $((30 - (now - last_fail)))s." >&2
      return 2
    fi
  fi

  # Execute payload
  if "$@"; then
    rm -f "$STATE_FILE"
    return 0
  else
    local status=$?
    local count=1
    if [[ -f "$STATE_FILE" ]]; then
      local prev_count
      read -r prev_count _ < "$STATE_FILE"
      count=$(( prev_count + 1 ))
    fi
    echo "$count $now" > "$STATE_FILE"
    echo "[CIRCUIT WARNING] Command failed. Consecutive failures: $count/$max_failures" >&2
    return "$status"
  fi
}
```
</details>

### Exercise 6: Complete Daemon Supervisor with PID Tracking (Hard)
**Task**: Build a minimal process supervisor script that launches an application binary, records its PID in `/var/run/app.pid`, monitors it, and automatically restarts it if it crashes unexpectedly, up to a maximum of 3 restarts in 60 seconds.

<details>
<summary>Solution</summary>

```bash
#!/usr/bin/env bash
set -euo pipefail

PID_FILE="/tmp/supervised_app.pid"
APP_CMD=("sleep" "10") # Simulated application
RESTART_LIMIT=3
RESTARTS=0
START_TIME=$(date +%s)

cleanup() {
  echo "[SUPERVISOR] Shutting down application..."
  if [[ -f "$PID_FILE" ]]; then
    local pid
    pid=$(cat "$PID_FILE")
    kill -TERM "$pid" 2>/dev/null || true
    rm -f "$PID_FILE"
  fi
  exit 0
}
trap cleanup EXIT INT TERM

while true; do
  "${APP_CMD[@]}" &
  APP_PID=$!
  echo "$APP_PID" > "$PID_FILE"
  echo "[SUPERVISOR] Started application (PID: $APP_PID)"

  wait "$APP_PID" || true
  EXIT_STATUS=$?

  NOW=$(date +%s)
  if (( NOW - START_TIME > 60 )); then
    RESTARTS=0
    START_TIME="$NOW"
  fi

  ((RESTARTS++))
  echo "[SUPERVISOR] Application exited with code $EXIT_STATUS. Restart count: $RESTARTS/$RESTART_LIMIT"

  if (( RESTARTS > RESTART_LIMIT )); then
    echo "[SUPERVISOR FATAL] Restart limit exceeded within 60s. Halting." >&2
    exit 1
  fi
  sleep 1
done
```
</details>

---

## 12. Summary & Quick Reference Cheat Sheet

| Construct | Syntax | Usage Notes |
| :--- | :--- | :--- |
| **Shebang** | `#!/usr/bin/env bash` | Directs kernel to load environment's bash binary |
| **Local Variable** | `local var_name="value"` | Scopes variable to the executing function |
| **Read-Only Local** | `local -r MAX=10` | Immutably binds variable within function |
| **Test Expression** | `[[ condition ]]` | Modern test keyword; suppresses word splitting |
| **Regex Match** | `[[ $str =~ regex ]]` | Regex evaluation with capture group support |
| **Case Statement** | `case "$var" in pat) ... ;; esac` | High-speed pattern matching |
| **Safe Line Reading** | `while IFS= read -r line; do` | Preserves whitespace and trailing lines |
| **Trap on Exit** | `trap cleanup EXIT` | Always runs on script termination |
| **Trap on Error** | `trap on_error ERR` | Runs when any command fails |
| **Ignore Signal** | `trap '' SIGINT` | Disables Ctrl+C interruption |
| **Restore Signal** | `trap - SIGINT` | Resets signal to default OS handler |
| **Return Status** | `return 0` | Returns status code from a function |
| **Exit Process** | `exit 1` | Terminates whole script with exit code |

---

## 13. Further Reading & Official Documentation

- [GNU Bash Reference Manual: Shell Builtin Commands](https://www.gnu.org/software/bash/manual/html_node/Bash-Builtins.html)
- [POSIX.1-2017: Shell & Utilities - trap](https://pubs.opengroup.org/onlinepubs/9699919799/utilities/trap.html)
- [Google Shell Style Guide](https://google.github.io/styleguide/shellguide.html)
- [Advanced Bash-Scripting Guide: Functions and Traps](https://tldp.org/LDP/abs/html/functions.html)

---

[<- 06-jq-and-structured-data.md](./06-jq-and-structured-data.md) | [Back to Module Overview](./README.md) | [08-bash-scripting-advanced.md ->](./08-bash-scripting-advanced.md)
