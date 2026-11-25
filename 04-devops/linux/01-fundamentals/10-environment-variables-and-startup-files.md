<!--
================================================================================
Metadata:
  Title: Environment Variables, Shell Initialization & Startup Profiles
  Track: Linux & Systems Engineering / DevOps
  Difficulty: Intermediate
  Estimated Time: 50 minutes
  Prerequisites: 01-fundamentals/01-getting-started.md
  Key Terminology: Environment Variables, export, Login Shell, Non-Login Shell, /etc/profile, ~/.bashrc, /etc/environment, systemd EnvironmentFile
================================================================================
-->

# Environment Variables, Shell Initialization & Startup Profiles

[<- 09-man-pages-and-getting-help.md](./09-man-pages-and-getting-help.md) | [Back to Module Overview](./README.md) | [11-boot-process.md ->](./11-boot-process.md)

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

In modern distributed infrastructure and microservice architectures, **Environment Variables** are the universal mechanism for passing runtime configuration, credentials, feature flags, and service endpoints to running processes (as codified by the Twelve-Factor App methodology).

However, inconsistent configuration across Linux startup profiles represents one of the most frequent causes of deployment failures and silent CI/CD defects:
- An automation script works perfectly when run interactively over SSH, but crashes when executed via a non-interactive Jenkins runner or Cron timer because `$PATH` or API keys are missing.
- Misunderstanding the difference between a **Login Shell** (which sources `/etc/profile` and `~/.bash_profile`) and a **Non-Login Shell** (which sources `~/.bashrc`) leads to duplicate or conflicting variable exports.
- Cloud orchestration systems and `systemd` services do not inherit shell profile files at all; they require structured key-value declarations in `/etc/environment` or `EnvironmentFile=` directives.

---

## 2. Core Concepts

### Local Variables vs. Environment Variables
- **Local Shell Variable**: Exists strictly inside the current memory space of the active shell process. It is **not** passed down to child processes spawned by the shell.
  ```bash
  GREETING="Hello"   # Local variable
  python3 -c "import os; print(os.getenv('GREETING'))"
  # Output: None
  ```
- **Environment Variable**: Explicitly exported using the `export` keyword. The variable is registered in the process's environment array and is automatically inherited by all future child processes across `fork()` and `execve()`.
  ```bash
  export GREETING="Hello"   # Exported environment variable
  python3 -c "import os; print(os.getenv('GREETING'))"
  # Output: Hello
  ```

### The 3 Shell Execution Contexts
Every shell session in Linux falls into one of three execution categories:

1. **Interactive Login Shell**:
   - *When*: Logging in over SSH, logging in at the physical console, or running `su - <user>` / `bash --login`.
   - *Initialization Files*:
     1. `/etc/profile` (System-wide configuration)
     2. `/etc/profile.d/*.sh` (System-wide modular scripts)
     3. Sourced in order (stops at the *first* file found):
        - `~/.bash_profile`
        - `~/.bash_login`
        - `~/.profile`
     4. `~/.bash_logout` (Executed upon terminal exit)

2. **Interactive Non-Login Shell**:
   - *When*: Opening a new terminal tab inside tmux, launching a GUI terminal emulator, or running `bash` without `--login`.
   - *Initialization Files*:
     1. `/etc/bash.bashrc` (System-wide bash settings)
     2. `~/.bashrc` (User personal bash configuration)

3. **Non-Interactive Non-Login Shell**:
   - *When*: Executing automated shell scripts (`#!/usr/bin/env bash`), running Cron jobs, or invoking commands remotely via SSH (`ssh host 'command'`).
   - *Initialization Files*:
     - Ignores `~/.bashrc` and `/etc/profile`!
     - Checks exclusively for the `$BASH_ENV` environment variable, expanding and sourcing that file if defined.

---

## 3. How It Works Internally

### Process Environment Inheritance Architecture
At the Linux kernel level, every process environment is an array of null-terminated strings (`key=value`) stored in the process's high-memory virtual address space:

```
+--------------------------------------------------------------------------+
| PROCESS MEMORY LAYOUT (Virtual Address Space)                            |
|                                                                          |
| Top of Memory (0x7FFFFFFFFFFF)                                           |
| ├── Environment Pointers Array (char **environ)                          |
| │   ├── "PATH=/usr/local/bin:/usr/bin:/bin\0"                            |
| │   ├── "USER=ubuntu\0"                                                  |
| │   └── "DATABASE_URL=postgres://...\0"                                 |
| ├── Command Line Arguments (char **argv)                                 |
| ├── Stack (Local function frames, grows downward)                        |
| │   v                                                                    |
| ├── Heap (malloc / dynamic memory, grows upward)                         |
| └── Text / Code Segment (Compiled binary instructions)                   |
+--------------------------------------------------------------------------+
```

When a process calls `fork()`, the kernel duplicates the entire virtual memory space, including the environment array. When the child subsequently calls `execve(path, argv, envp)`, it passes the environment array forward.

### Inspecting Live Environment via `/proc`
You can inspect the exact, unmodified environment variables of any running process on the system by reading `/proc/<PID>/environ`:

```bash
# Entries are separated by null bytes (\0); translate to newlines for inspection:
cat /proc/1/environ | tr '\0' '\n' | head -n 10

# Inspect environment of current shell ($$)
cat /proc/$$/environ | tr '\0' '\n' | grep "PATH"
```

---

## 4. Syntax & Usage

### Manipulating Variables in Bash
```bash
# Set and export an environment variable in one command
export DATABASE_HOST="db-primary.internal"

# Print all active environment variables
printenv
env

# Query a single environment variable
printenv PATH
echo "$DATABASE_HOST"

# Unset / delete a variable from the environment
unset DATABASE_HOST

# Temporarily pass an environment variable to a single command execution (does not alter shell)
PORT=8080 node server.js
```

### Modifying the `$PATH` Variable
The `$PATH` variable is a colon-delimited list of directories searched sequentially by the shell when resolving executable command names:

```bash
# Correct Pattern: Prepend custom path so local tools take precedence
export PATH="$HOME/.local/bin:$PATH"

# Append custom path (system tools take precedence)
export PATH="$PATH:/opt/custom-suite/bin"
```

### Static System-Wide Variables: `/etc/environment`
`/etc/environment` is **NOT a shell script**. It is parsed directly by the PAM module `pam_env.so` during login:
- Format: Strictly `KEY="value"` lines.
- No shell expansions permitted (writing `PATH=$PATH:/opt/bin` will literally assign the string `"$PATH:/opt/bin"`, corrupting `$PATH`!).

---

## 5. Practical Examples

### Example 1: Bridging `~/.bash_profile` and `~/.bashrc`
```bash
# Standard Unix Best Practice Pattern:
# Keep interactive aliases and functions in ~/.bashrc,
# and have ~/.bash_profile source ~/.bashrc upon login.

# Check ~/.bash_profile
cat << 'EOF' > ~/.bash_profile
# Source ~/.profile if present
if [ -f "$HOME/.profile" ]; then
    . "$HOME/.profile"
fi

# Source ~/.bashrc for interactive login shells
if [ -f "$HOME/.bashrc" ]; then
    . "$HOME/.bashrc"
fi

# Export login-specific environment variables
export PATH="$HOME/bin:$HOME/.local/bin:$PATH"
EOF
```

### Example 2: Configuring System-Wide Environment via `/etc/profile.d/`
```bash
# Production requirement: Automatically configure Java Home for all users
sudo tee /etc/profile.d/java-home.sh << 'EOF'
# /etc/profile.d/java-home.sh - System-wide Java Environment
export JAVA_HOME="/usr/lib/jvm/java-21-openjdk-amd64"
export PATH="$JAVA_HOME/bin:$PATH"
EOF

# Ensure script is executable and readable by all users
sudo chmod 0644 /etc/profile.d/java-home.sh

# Verify loading in a new login shell
bash --login -c 'echo "JAVA_HOME=$JAVA_HOME"'
```

### Example 3: Injecting Environment Variables into a Systemd Unit
```bash
# Systemd does NOT read ~/.bashrc or /etc/profile!
# Create an environment file for an application service:
sudo mkdir -p /etc/my-service
sudo tee /etc/my-service/env << 'EOF'
APP_ENV=production
API_PORT=3000
DATABASE_URL=postgres://app:secret@db.internal:5432/production
EOF
sudo chmod 0600 /etc/my-service/env

# Reference the file inside the systemd service unit (/etc/systemd/system/my-app.service):
# [Service]
# EnvironmentFile=/etc/my-service/env
# ExecStart=/usr/bin/node /opt/my-app/index.js
```

### Example 4: Diagnosing Missing Environment in Non-Interactive SSH Commands
```bash
# Problem: Script fails when run via remote SSH execution:
ssh user@remote-srv "python3 -c 'import os; print(os.getenv(\"MY_KEY\"))'"
# Output: None (because ~/.bashrc was bypassed!)

# Remediation 1: Force a login shell remotely
ssh user@remote-srv "bash --login -c 'python3 -c \"import os; print(os.getenv(\\\"MY_KEY\\\"))\"'"

# Remediation 2: Define the variable in ~/.ssh/environment (requires PermitUserEnvironment yes in sshd_config)
# Remediation 3: Explicitly source ~/.bashrc in the remote command
ssh user@remote-srv "source ~/.bashrc && my_command"
```

### Example 5: Inspecting Secret Leakage via Process Environment
```bash
# Security check: Are sensitive database passwords exposed via /proc?
# Launch process passing secret inline
DATABASE_PASSWORD="SuperSecretPassword123" sleep 100 &
SLEEP_PID=$!

# Any user with permission can inspect the process environment:
cat /proc/$SLEEP_PID/environ | tr '\0' '\n' | grep "DATABASE_PASSWORD"
# Output: DATABASE_PASSWORD=SuperSecretPassword123

# CLEANUP
kill $SLEEP_PID

# Takeaway: Avoid storing secrets in persistent environment variables if untrusted users
# share the system; use mounted secret files or memory-wiped vaults.
```

---

## 6. Best Practices

- **Never Put Variable Expansions in `/etc/environment`**: `/etc/environment` is parsed by `pam_env.so`, not by bash. Never write `PATH=$PATH:/new/bin` in this file; it will break your system `$PATH` on next reboot.
- **Never Include `.` (Current Directory) in `$PATH`**: Writing `export PATH=".:$PATH"` creates a critical privilege escalation vulnerability. If you navigate to `/tmp` and type `ls`, an attacker could place a malicious executable named `ls` in `/tmp` that executes with your privileges.
- **Keep `~/.bashrc` Free of Heavy Network Operations**: Do not add slow commands (e.g., checking git remote status, running `nvm use`, or querying remote APIs) inside `~/.bashrc`. This adds latency to every single new terminal window or SFTP/SCP connection.
- **Export Variables Explicitly**: When authoring bash scripts that call child utilities (Python, Node, Docker), always use `export KEY=val`. Assigning `KEY=val` without `export` leaves the variable invisible to child runtimes.
- **Standardize on `/etc/profile.d/` for Modular Additions**: Never modify the core `/etc/profile` directly. Place modular `.sh` scripts in `/etc/profile.d/` to avoid conflicts during distribution package updates.

---

## 7. Common Mistakes & Troubleshooting

| Mistake / Issue | Why It Fails | Practical Remedy |
| :--- | :--- | :--- |
| Writing variable expansion inside `/etc/environment` | `pam_env` does not execute a shell; it treats `$PATH` as a literal string. | Put dynamic path expansions in `/etc/profile.d/custom.sh`. |
| Script executed by Cron fails with `command not found` | Cron executes in a minimal environment with `$PATH=/usr/bin:/bin`; it does not source user startup profiles. | Define explicit `PATH` at the top of the crontab, or use absolute paths. |
| Setting an alias in `~/.bash_profile` doesn't work in tmux | New tmux panes spawn interactive *non-login* shells, which source `~/.bashrc`, bypassing `~/.bash_profile`. | Define aliases in `~/.bashrc`. |
| Modifying parent shell variable from a bash script (`./set-env.sh`) | Running a script executes in a child process; child processes cannot alter parent environment. | Source the script in the current shell: `source ./set-env.sh` or `. ./set-env.sh`. |
| Spaces around `=` during variable assignment (`VAR = value`) | Bash treats `VAR` as a command name and `=` / `value` as arguments, producing `VAR: command not found`. | Never put spaces around the assignment operator: `VAR=value`. |
| Passing secrets via command line environment variables (`API_KEY=xxx ./app`) | The variable is visible in `/proc/<PID>/environ` and `ps e` to the user and root. | Pass sensitive secrets via secure file descriptors or environment files with `0600` permissions. |

---

## 8. Performance, Security & Cost Implications

- **Performance**: Sourcing bloated `~/.bashrc` files containing hundreds of lines of complex shell scripting, git branch status evaluations, and slow version manager shims (`nvm`, `asdf`, `pyenv`) can add 200–500 ms of latency to every shell invocation. This severely degrades automated batch jobs and SFTP transfers.
- **Security**: The process environment can leak sensitive API tokens, database passwords, and cryptographic keys to unprivileged child processes. Furthermore, child processes inheriting `LD_PRELOAD` or altered `PATH` variables can be tricked into executing arbitrary code.
- **Cost**: Misconfigured startup profiles can cause automated autoscaling health checks (e.g., AWS EC2 UserData scripts or Kubernetes startup probes) to fail, triggering endless container restart loops and unnecessary infrastructure thrashing.

---

## 9. Comparison Table

| Initialization File | Shell Type | Invocation Scope | Allows Logic / Scripting? | Best Used For |
| :--- | :--- | :--- | :--- | :--- |
| `/etc/environment` | All PAM logins | System-wide (all users) | No (static key=value only) | Global locale, base system constants |
| `/etc/profile` | Login Shell | System-wide (all users) | Yes (Shell script) | System-wide shell setup & path rules |
| `/etc/profile.d/*.sh`| Login Shell | System-wide modular | Yes (Shell script) | Per-package environment setup |
| `/etc/bash.bashrc` | Non-Login Shell | System-wide (all users) | Yes (Bash script) | System-wide bash completions & functions |
| `~/.bash_profile` | Login Shell | Single User | Yes (Bash script) | Sourcing `~/.bashrc` and user paths |
| `~/.bashrc` | Non-Login Shell | Single User | Yes (Bash script) | Aliases, prompt (`PS1`), shell functions |
| `$BASH_ENV` | Non-Interactive | Per-Script execution | Yes (Bash script) | CI/CD automation environment preloading |

---

## 10. Real-World Interview Questions

<details>
<summary><strong>1. Explain the difference between an Interactive Login Shell and an Interactive Non-Login Shell. Which configuration files does each source?</strong></summary>

**Answer**:
- **Interactive Login Shell**: Instantiated when a user authenticates to the system (e.g., via SSH, physical console login, `su - <user>`, or `bash --login`).
  - It sources `/etc/profile`, which in turn sources all scripts in `/etc/profile.d/*.sh`.
  - Next, it searches for user-specific profiles, executing the *first* one it finds among: `~/.bash_profile`, `~/.bash_login`, and `~/.profile`.
  - When the user exits the session, it executes `~/.bash_logout`.
- **Interactive Non-Login Shell**: Instantiated after the user is already logged in (e.g., opening a new terminal window inside a desktop GUI, splitting a pane in `tmux`, or running the command `bash`).
  - It skips `/etc/profile` and user profile files completely.
  - Instead, it sources `/etc/bash.bashrc` followed by the user's `~/.bashrc`.
</details>

<details>
<summary><strong>2. Why cannot a bash script modify an environment variable in the terminal session that launched it?</strong></summary>

**Answer**:
In the Unix process model, process execution relies on the `fork()` and `execve()` system calls:
1. When you execute a script by running `./my_script.sh`, the parent interactive shell forks a new, independent child process.
2. The child process receives a copy of the parent's environment memory segment.
3. Any variable modifications or `export` statements executed inside the script alter only the child process's virtual memory space.
4. When the script completes, the child process terminates, and its memory space is deallocated by the kernel.
5. In Linux, a child process can never modify the memory space of its parent process.

To modify the parent shell's environment, the script must not be executed as a separate child; it must be **sourced** inside the current process using `source my_script.sh` or `. my_script.sh`.
</details>

<details>
<summary><strong>3. What is the fundamental difference between `/etc/environment` and `/etc/profile`?</strong></summary>

**Answer**:
- `/etc/environment` is not a script. It is a configuration file parsed directly by the PAM module `pam_env.so` during user login. It supports only static key-value assignments (`KEY="VALUE"`) and does not support bash syntax, conditional statements (`if`), or variable expansions (`PATH=$PATH:/new/bin`).
- `/etc/profile` is an actual Bourne shell script. It is executed by the shell interpreter during login shell initialization. It contains full programming logic, loops over `/etc/profile.d/*.sh`, evaluates conditional logic, and dynamically constructs paths.
</details>

<details>
<summary><strong>4. Why do scripts executed via `cron` or non-interactive SSH frequently fail with "command not found"?</strong></summary>

**Answer**:
Cron daemons and non-interactive SSH invocations (e.g., `ssh user@host "command"`) spawn non-interactive, non-login shells. These execution contexts deliberately bypass both `/etc/profile` and `~/.bashrc`.

Consequently:
- Custom `$PATH` entries defined in `~/.bashrc` (such as `/usr/local/bin`, `/opt/node/bin`, or `~/.local/bin`) are never loaded.
- The environment defaults to a bare, system-default path (typically `/usr/bin:/bin`).
- Any utility located outside these paths fails to resolve unless the script explicitly defines `$PATH` or uses absolute binary paths.
</details>

<details>
<summary><strong>5. What is the security danger of placing `.` (the current working directory) at the beginning of `$PATH`?</strong></summary>

**Answer**:
Placing `.` in `$PATH` (e.g., `export PATH=".:$PATH"`) introduces a severe Trojan horse / path hijacking vulnerability:
1. The shell searches directories in `$PATH` sequentially from left to right.
2. If `.` is first, the shell checks the current working directory before checking `/usr/bin` or `/bin`.
3. If an administrator navigates to a shared or untrusted directory (such as `/tmp` or an untrusted git repository) and types a common command like `ls` or `git`:
4. If an attacker has placed an executable file named `ls` in that directory, the shell executes the attacker's binary instead of `/usr/bin/ls`, executing malicious code with the administrator's privileges.
</details>

<details>
<summary><strong>6. How does `systemd` handle environment variables for background services?</strong></summary>

**Answer**:
`systemd` does not spawn an interactive shell and does not read `/etc/profile`, `/etc/bash.bashrc`, or user dotfiles. Systemd service units execute in an isolated environment containing only basic variables (`PATH=/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin`).

To inject environment variables into a systemd service:
1. Define static key-value pairs inside the unit file using the `Environment="KEY=value"` directive.
2. Reference an external environment file using `EnvironmentFile=/etc/default/myapp` or `EnvironmentFile=/etc/myapp/env`.
3. Define global defaults in `/etc/systemd/system.conf`.
</details>

<details>
<summary><strong>7. How can you inspect the live environment variables of a running process without using the `env` command?</strong></summary>

**Answer**:
By reading the process's synthetic kernel memory file at `/proc/<PID>/environ`.
In `/proc/<PID>/environ`, the kernel exposes the exact array of environment strings passed to `execve()`. Because entries are separated by ASCII null bytes (`\0`) rather than newlines, use `tr` or `strings` to format the output:
```bash
cat /proc/<PID>/environ | tr '\0' '\n'
```
This is particularly valuable for debugging daemons whose environment cannot be determined from external configuration files.
</details>

<details>
<summary><strong>8. What is the purpose of the `$BASH_ENV` variable?</strong></summary>

**Answer**:
`$BASH_ENV` is a special variable recognized by Bash when executed as a non-interactive shell (such as when running a shell script with `#!/usr/bin/env bash`).

When Bash starts non-interactively, it checks if `$BASH_ENV` is set in the environment. If it is defined, Bash expands its value as a filename and sources that file before executing the target script. This provides a mechanism to preload environment variables, aliases, or functions for automated CI/CD runners and batch scripts without modifying the script files themselves.
</details>

---

## 11. Practice Exercises

<details>
<summary><strong>Exercise 1 (Easy): Contrast Local vs. Exported Variables</strong></summary>

**Task**: Create a local shell variable and an exported environment variable. Launch a child bash subshell and observe which variable is visible.

**Solution**:
```bash
LOCAL_VAR="I am local"
export EXPORTED_VAR="I am exported"

# Launch child shell and query both
bash -c 'echo "Child sees Local: $LOCAL_VAR | Exported: $EXPORTED_VAR"'
# Output: Child sees Local:  | Exported: I am exported
```
</details>

<details>
<summary><strong>Exercise 2 (Easy): Safely Prepend to `$PATH`</strong></summary>

**Task**: Write the exact bash command to prepend `$HOME/opt/bin` to your active `$PATH` variable, ensuring that it handles existing paths properly.

**Solution**:
```bash
export PATH="$HOME/opt/bin:$PATH"
echo "$PATH"
```
</details>

<details>
<summary><strong>Exercise 3 (Medium): Inspect Process Environment via `/proc`</strong></summary>

**Task**: Launch a background process passing a custom environment variable, identify its PID, and extract that variable directly from `/proc/<PID>/environ`.

**Solution**:
```bash
# Launch background process
TRACKING_CODE="XYZ-987" sleep 60 &
TARGET_PID=$!

# Extract from /proc
cat /proc/$TARGET_PID/environ | tr '\0' '\n' | grep "TRACKING_CODE"
# Output: TRACKING_CODE=XYZ-987

# Cleanup
kill $TARGET_PID
```
</details>

<details>
<summary><strong>Exercise 4 (Medium): Trace Startup File Loading Order</strong></summary>

**Task**: Add echo statements to `~/.bash_profile` and `~/.bashrc`, then execute `bash --login` versus `bash` to verify which files are loaded in each scenario.

**Solution**:
```bash
echo 'echo ">>> LOADED ~/.bash_profile"' >> ~/.bash_profile
echo 'echo ">>> LOADED ~/.bashrc"' >> ~/.bashrc

# Test Login Shell
bash --login -c "exit"
# Output indicates ~/.bash_profile loaded (which may source ~/.bashrc)

# Test Non-Login Shell
bash -c "exit"
# Notice ~/.bash_profile is skipped!
```
</details>

<details>
<summary><strong>Exercise 5 (Hard): Build an Automated Profile Environment Audit Tool</strong></summary>

**Task**: Author a bash script that parses all files in `/etc/profile.d/`, `/etc/profile`, and the current user's dotfiles to identify if any file unsafely appends `.` to `$PATH`.

**Solution**:
```bash
#!/usr/bin/env bash
set -euo pipefail

TARGET_FILES=(
    "/etc/profile"
    "/etc/bash.bashrc"
    "$HOME/.profile"
    "$HOME/.bash_profile"
    "$HOME/.bashrc"
)

# Append all drop-ins
if [ -d "/etc/profile.d" ]; then
    TARGET_FILES+=(/etc/profile.d/*.sh)
fi

echo "Auditing shell initialization files for insecure PATH assignments..."
for file in "${TARGET_FILES[@]}"; do
    [ -f "$file" ] || continue
    if grep -E 'PATH=.*(\.:|:\.|\.\/)' "$file"; then
        echo "CRITICAL SECURITY WARNING: Insecure current directory '.' found in $file!"
    fi
done
```
</details>

<details>
<summary><strong>Exercise 6 (Hard): Construct an Environment Extraction Harness for Cron Jobs</strong></summary>

**Task**: Write a wrapper script that dumps a user's full interactive login environment into a clean, sanitized environment file that can be sourced safely by non-interactive Cron jobs.

**Solution**:
```bash
#!/usr/bin/env bash
set -euo pipefail

OUTPUT_ENV="$HOME/.cron_environment"

# Execute a login shell to capture the complete environment
bash --login -c 'env' | grep -E '^(PATH|USER|HOME|SHELL|LANG|LC_|JAVA_HOME)=' > "$OUTPUT_ENV"

chmod 0600 "$OUTPUT_ENV"
echo "Generated sanitized cron environment at $OUTPUT_ENV:"
cat "$OUTPUT_ENV"

# Crontab usage:
# * * * * * . $HOME/.cron_environment && /path/to/job.sh
```
</details>

---

## 12. Summary & Quick Reference Cheat Sheet

| Task | Command / Directive | Context |
| :--- | :--- | :--- |
| **Export Variable** | `export KEY="value"` | Active shell & future children |
| **Query Variable** | `printenv KEY` | Reads environment array |
| **Inspect Running PID** | `cat /proc/<PID>/environ \| tr '\0' '\n'` | Kernel direct memory inspection |
| **Prepend Path** | `export PATH="/new/dir:$PATH"` | Preserves existing path entries |
| **Static System Var** | Add `KEY="value"` to `/etc/environment` | Evaluated by `pam_env.so` |
| **Modular System Profile**| Add executable script to `/etc/profile.d/*.sh` | Sourced by all login shells |
| **User Login Config** | Edit `~/.bash_profile` | Sourced on initial user login |
| **User Shell Config** | Edit `~/.bashrc` | Sourced on every interactive shell |
| **Systemd Service Var** | `EnvironmentFile=/etc/app/env` | Injected into daemon process |

---

## 13. Further Reading & Official Documentation

- [GNU Bash Reference Manual: Bash Startup Files](https://www.gnu.org/software/bash/manual/html_node/Bash-Startup-Files.html)
- [The Twelve-Factor App: Config via Environment Variables](https://12factor.net/config)
- [man 7 environ: User Environment Overview](https://man7.org/linux/man-pages/man7/environ.7.html)
- [man 8 pam_env: PAM Environment Module](https://man7.org/linux/man-pages/man8/pam_env.8.html)
- [systemd.exec: Environment in Systemd Units](https://www.freedesktop.org/software/systemd/man/latest/systemd.exec.html)

---

[<- 09-man-pages-and-getting-help.md](./09-man-pages-and-getting-help.md) | [Back to Module Overview](./README.md) | [11-boot-process.md ->](./11-boot-process.md)
