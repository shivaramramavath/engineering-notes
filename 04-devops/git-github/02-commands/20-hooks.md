<!--
================================================================================
Metadata:
  Title: Git Hooks Automation: Client-Side & Server-Side Architectures
  Track: Git & GitHub / DevOps
  Difficulty: Advanced
  Estimated Time: 55 minutes
  Prerequisites: 19-git-lfs.md
  Key Terminology: Git Hooks, pre-commit, commit-msg, pre-push, pre-receive, post-receive, core.hooksPath, Husky, Lefthook, Secret Scanning
================================================================================
-->

# Git Hooks Automation: Client-Side & Server-Side Architectures

[<- 19-git-lfs.md](./19-git-lfs.md) | [Back to Module Overview](./README.md) | [21-aliases-and-productivity.md ->](./21-aliases-and-productivity.md)

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

In modern software development, code quality, security policies, and commit standards cannot rely on developer memory. If compliance depends on every engineer remembering to run a linter, scan for leaked AWS credentials, format their code, and format commit messages according to Conventional Commits before pushing, human error is guaranteed.

**Git Hooks** provide automated lifecycle enforcement.
Hooks are executable scripts that Git invokes automatically whenever specific lifecycle events occur (such as committing, merging, pushing, or receiving code on a remote server).

Hooks operate across two distinct architectural tiers:
- **Client-Side Hooks**: Execute on developer laptops to catch errors *before* code is recorded or pushed (e.g. `pre-commit` secret scanning, `commit-msg` linter).
- **Server-Side Hooks**: Execute on the central Git server or cloud forge to enforce absolute organization-wide policies *before* code is accepted into shared repositories (e.g. `pre-receive` signed commit enforcement, branch protection).

Mastering Git hooks allows DevOps and security teams to build automated shift-left security barriers that catch vulnerabilities at the developer's keyboard.

---

## 2. Core Concepts

### Client-Side vs. Server-Side Hooks

```
+-----------------------------------------------------------------------------------+
|                            Git Hooks Lifecycle Pipeline                           |
|                                                                                   |
|  CLIENT-SIDE HOOKS (Developer Workstation)                                        |
|                                                                                   |
|  git commit                                                                       |
|       │                                                                           |
|       ▼                                                                           |
|  1. [ pre-commit ] ──────────► Checks linters, tests, secret scanners             |
|       │ (Non-zero exit aborts commit!)                                            |
|       ▼                                                                           |
|  2. [ prepare-commit-msg ] ──► Auto-injects JIRA issue ticket into template       |
|       │                                                                           |
|       ▼                                                                           |
|  3. [ commit-msg ] ──────────► Validates Conventional Commits syntax regex        |
|       │ (Non-zero exit aborts commit!)                                            |
|       ▼                                                                           |
|  4. [ post-commit ] ─────────► Desktop alerts, metrics, build notifications       |
|                                                                                   |
|  git push                                                                         |
|       │                                                                           |
|       ▼                                                                           |
|  5. [ pre-push ] ────────────► Runs fast unit tests before network upload         |
|       │                                                                           |
|       ▼====================================================► [ Git Remote Server ]|
|                                                                                   |
|  SERVER-SIDE HOOKS (GitHub / Bare Remote Hub)                                     |
|                                                                                   |
|  6. [ pre-receive ] ─────────► Enforces signed commits, file size limits (<100MB)|
|       │ (Non-zero exit rejects push for the entire team!)                         |
|       ▼                                                                           |
|  7. [ update ] ──────────────► Enforces per-branch RBAC permissions               |
|       │                                                                           |
|       ▼ (Refs updated on server)                                                  |
|  8. [ post-receive ] ────────► Triggers CI/CD pipelines, webhooks, Slack alerts  |
+-----------------------------------------------------------------------------------+
```

### The Hook Sharing Problem & `core.hooksPath`
By default, Git stores client-side hooks inside `.git/hooks/`.
- However, **the `.git/` directory is never version-controlled or pushed to remote repositories!**
- If an engineer writes a brilliant `pre-commit` hook in `.git/hooks/`, none of their teammates will receive it when they clone!

Modern Git (2.9+) solves this via **`core.hooksPath`**:
- Teams create a version-controlled directory in the repository root (e.g. `.githooks/`).
- Developers configure: `git config core.hooksPath .githooks`.
- Now, all hooks are committed, versioned, and shared across the entire engineering team!

---

## 3. How It Works Internally

### Execution Mechanics of a Git Hook
When an event occurs (e.g. `git commit`):
1. Git looks for an executable file named `pre-commit` inside the configured hooks directory (`core.hooksPath` or `.git/hooks/`).
2. If found, Git executes the script with standard POSIX environment variables (`GIT_DIR`, `GIT_INDEX_FILE`, etc.).
3. **Exit Code Evaluation**:
   - **Exit Code `0`**: Success. Git proceeds with the operation.
   - **Non-Zero Exit Code (e.g. `1`)**: Failure. Git immediately aborts the operation and displays the hook's error output on `stderr`!
4. **Bypassing Client Hooks**: Client-side hooks can be bypassed by the developer passing the `--no-verify` flag to `git commit` or `git push`. (Server-side hooks *cannot* be bypassed!).

---

## 4. Syntax & Usage

### 1. Activating and Configuring Version-Controlled Hooks
```bash
# 1. Create a version-controlled hooks directory in repo root
mkdir .githooks

# 2. Tell Git to read hooks from .githooks instead of .git/hooks
git config core.hooksPath .githooks

# 3. Ensure hook scripts have executable POSIX permissions (+x)
chmod +x .githooks/*
```

---

### 2. Standard Client Hook Templates

#### Pre-Commit Hook (`.githooks/pre-commit`):
Executes before the commit message prompt appears. Used for linters, secret scanning, and formatting:
```bash
#!/usr/bin/env bash
set -e

echo "[Hook] Running pre-commit verification..."

# 1. Prevent committing AWS keys or private keys
if git diff --cached | grep -iE 'AKIA[0-9A-Z]{16}|BEGIN (RSA|EC|OPENSSH) PRIVATE KEY'; then
    echo "ERROR: Attempted to commit a sensitive cryptographic key or AWS token!"
    echo "Commit rejected by pre-commit hook."
    exit 1
fi

# 2. Prevent committing unresolved conflict markers
if git diff --cached | grep -E '^(<{7}|={7}|>{7})'; then
    echo "ERROR: Unresolved merge conflict markers detected in staged files!"
    exit 1
fi

echo "[Hook] Pre-commit checks passed!"
exit 0
```

---

#### Commit-Msg Hook (`.githooks/commit-msg`):
Validates that commit messages adhere to Conventional Commits:
```bash
#!/usr/bin/env bash
# Git passes the path to the commit message file as argument $1
COMMIT_MSG_FILE=$1
COMMIT_MSG=$(head -n 1 "$COMMIT_MSG_FILE")

# Conventional Commits regex pattern
PATTERN="^(feat|fix|docs|style|refactor|perf|test|build|ci|chore|revert)(\([a-zA-Z0-9_-]+\))?!?: .+$"

if ! echo "$COMMIT_MSG" | grep -qE "$PATTERN"; then
    echo "ERROR: Invalid commit message format!"
    echo "Your message: '$COMMIT_MSG'"
    echo "Format must follow Conventional Commits:"
    echo "  <type>(<scope>): <subject>"
    echo "  Example: feat(auth): implement oauth token rotation"
    exit 1
fi

exit 0
```

---

#### Prepare-Commit-Msg Hook (`.githooks/prepare-commit-msg`):
Automatically prepends the active JIRA ticket from the branch name (e.g. `feat/JIRA-1042-login`) into the commit message template:
```bash
#!/usr/bin/env bash
COMMIT_MSG_FILE=$1
COMMIT_SOURCE=$2

# Only modify if creating a standard commit (not a merge or amend)
if [ -z "$COMMIT_SOURCE" ]; then
    BRANCH_NAME=$(git branch --show-current)
    # Extract JIRA ticket pattern (e.g. JIRA-1042 or PROJ-88)
    TICKET=$(echo "$BRANCH_NAME" | grep -oE '[A-Z]+-[0-9]+' || true)
    
    if [ -n "$TICKET" ]; then
        # Prepend ticket to commit message template
        sed -i.bak "1s/^/[$TICKET] /" "$COMMIT_MSG_FILE"
    fi
fi
```

---

## 5. Practical Examples

### Example 1: Building a Secret-Blocking Pre-Commit Hook
Prevent developers from staging `.env` or password strings:

```bash
mkdir /tmp/hooks-lab && cd /tmp/hooks-lab
git init -b main
mkdir .githooks
git config core.hooksPath .githooks

cat << 'EOF' > .githooks/pre-commit
#!/usr/bin/env bash
# Scan staged files for secret patterns
if git diff --cached | grep -iE '(api_key|client_secret|password)\s*=\s*["\x27][^"\x27]+["\x27]'; then
    echo "SECURITY ALERT: Hardcoded secret detected in staged changes!"
    exit 1
fi
exit 0
EOF
chmod +x .githooks/pre-commit

# Test 1: Commit benign file (Succeeds)
echo "console.log('safe');" > app.js
git add app.js && git commit -m "feat: initial safe commit"
# Output: Commit succeeds!

# Test 2: Commit secret (Hook blocks commit!)
echo "const api_key = 'sk_live_1234567890';" >> app.js
git add app.js
git commit -m "feat: add stripe integration"
# Output:
# SECURITY ALERT: Hardcoded secret detected in staged changes!
# Commit blocked!
```

### Example 2: Pre-Push Hook for Test Automation
Block `git push` if unit tests fail, avoiding broken CI builds:

```bash
cat << 'EOF' > .githooks/pre-push
#!/usr/bin/env bash
echo "[Hook] Running test suite prior to push..."

# Run fast unit tests
if ! npm test -- --silent; then
    echo "ERROR: Unit tests failed! Push aborted."
    echo "Fix failing tests before pushing to remote."
    exit 1
fi
exit 0
EOF
chmod +x .githooks/pre-push
```

### Example 3: Simulating a Server-Side `pre-receive` Hook
Examine how enterprise servers like GitHub validate incoming pushes across the whole team:

In a bare repository's `hooks/pre-receive`:
```bash
#!/usr/bin/env bash
set -e

# pre-receive reads lines from stdin: <old-sha> <new-sha> <refname>
while read old_sha new_sha refname; do
    # 1. Enforce branch naming conventions
    if [[ "$refname" =~ refs/heads/.* ]] && ! [[ "$refname" =~ refs/heads/(main|master|feat/.*|fix/.*|release/.*) ]]; then
        echo "REJECTED: Branch name '$refname' violates organization naming rules!"
        echo "Branches must start with 'feat/', 'fix/', or 'release/'."
        exit 1
    fi
    
    # 2. Block individual files larger than 50MB
    for blob in $(git rev-list --objects "$old_sha..$new_sha" | awk '{print $1}'); do
        size=$(git cat-file -s "$blob" 2>/dev/null || true)
        if [ -n "$size" ] && [ "$size" -gt 52428800 ]; then
            echo "REJECTED: Push contains blob exceeding 50MB! Use Git LFS."
            exit 1
        fi
    done
done
exit 0
```

### Example 4: Automating Hook Setup via `package.json` (Husky Pattern)
In Node.js ecosystems, automate hook installation during `npm install`:

In `package.json`:
```json
{
  "scripts": {
    "prepare": "git config core.hooksPath .githooks"
  }
}
```
Whenever an engineer or CI runner executes `npm install`, Git automatically configures `core.hooksPath .githooks`!

---

## 6. Best Practices

1. **Always Use `core.hooksPath`**: Never instruct team members to manually copy files into `.git/hooks/`. Commit your hooks into `.githooks/` and set `git config core.hooksPath .githooks`.
2. **Keep Client Hooks Blazingly Fast (<2 seconds)**: If a `pre-commit` hook takes 30 seconds to run a massive test suite, developers will habitually bypass it using `git commit --no-verify`. Keep pre-commit to fast linters and secret scanners; leave heavy end-to-end suites to CI.
3. **Never Rely on Client Hooks as Your Sole Security Gate**: Client hooks are voluntary; any developer can run `git commit --no-verify` or disable them. Always enforce mandatory security policies on the **server side** (GitHub Branch Rulesets, CI status checks, `pre-receive` hooks).
4. **Always Exit with Clean Exit Codes**: Ensure your hook script explicitly returns `exit 0` on success and `exit 1` on error.
5. **Set POSIX Permissions (`+x`)**: Ensure all hook files have executable permissions (`chmod +x .githooks/*`) in Git so they execute on Linux and macOS workstations.

---

## 7. Common Mistakes & Troubleshooting

| Error / Symptom | Root Cause | Solution |
| :--- | :--- | :--- |
| Hook script does not execute at all | Missing executable permissions on the script file. | Run `chmod +x .githooks/<hook-name>` and commit the permission change. |
| Teammates do not have hooks active after cloning | Hooks were placed in unversioned `.git/hooks/`. | Move to `.githooks/` and configure `git config core.hooksPath .githooks`. |
| Hook fails with `: No such file or directory` on Linux/macOS | Windows `CRLF` line endings in the bash shebang line (`#!/usr/bin/env bash\r`). | Convert line endings to LF: `dos2unix .githooks/*` and set `.gitattributes`. |
| Hook executes, but commits are allowed despite errors | Script did not return a non-zero exit code on failure. | Add `set -e` at the top of the bash script or call `exit 1` explicitly. |
| Developer bypassed hook with `--no-verify` | `--no-verify` bypasses `pre-commit` and `commit-msg`. | Enforce policy on GitHub via Branch Rulesets and CI status checks. |

---

## 8. Performance, Security & Cost Implications

### Performance
- **Staged-File Scans Only**: Never scan the entire 1,000,000-file repository in a `pre-commit` hook! Use `git diff --cached --name-only` to run linters and secret scanners strictly on the files being staged. This keeps execution times under **200 milliseconds**.

### Security
- **Shift-Left Defense**: Preventing API keys and AWS tokens from ever being recorded in a local commit saves thousands of dollars in credential rotation overhead, security revocation procedures, and compliance reporting.

---

## 9. Comparison Table

| Attribute | Client-Side Hooks | Server-Side Hooks |
| :--- | :--- | :--- |
| **Execution Location** | Developer Workstation | Central Git Host (GitHub/Enterprise) |
| **Bypassable?** | Yes (via `--no-verify`) | NO (impossible to bypass by clients) |
| **Primary Examples** | `pre-commit`, `commit-msg`, `pre-push` | `pre-receive`, `update`, `post-receive` |
| **Version Controlled?** | Yes (via `core.hooksPath`) | Managed via server administration |
| **Best Used For** | Fast feedback, linting, secret catch | Strict compliance, security gate, CI triggers |

---

## 10. Real-World Interview Questions

<details>
<summary><strong>1. Explain the difference between client-side and server-side Git hooks.</strong></summary>

- **Client-Side Hooks**: Execute locally on a developer's workstation triggered by actions such as committing (`pre-commit`, `commit-msg`), checking out (`post-checkout`), or pushing (`pre-push`). They are designed for fast developer feedback (linting, secret detection). Because developers can bypass them using `--no-verify`, they cannot be relied upon as absolute security guarantees.
- **Server-Side Hooks**: Execute on the central Git server (or cloud forge) when receiving pushes (`pre-receive`, `update`, `post-receive`). They cannot be bypassed by clients. If a server-side `pre-receive` hook exits with a non-zero code, the push is rejected for the entire transaction.
</details>

<details>
<summary><strong>2. Why are files in `.git/hooks/` not shared when cloning a repository, and how does `core.hooksPath` solve this?</strong></summary>

The `.git/` administrative directory is strictly private to each local repository clone and is never transferred over the wire during `git push` or `git clone` (executing arbitrary unreviewed scripts automatically upon clone would also be a massive remote code execution security vulnerability). Git 2.9 introduced the `core.hooksPath` configuration directive. Teams can place hook scripts in a tracked directory (e.g. `.githooks/`) that is version-controlled in the repository, and configure Git to execute hooks from that directory via `git config core.hooksPath .githooks`.
</details>

<details>
<summary><strong>3. What does the `--no-verify` flag do and which hooks does it bypass?</strong></summary>

The `--no-verify` (or `-n`) flag instructs Git to bypass client-side verification hooks:
- During `git commit --no-verify`: Bypasses `pre-commit` and `commit-msg`.
- During `git push --no-verify`: Bypasses `pre-push`.
It does **not** bypass server-side hooks (`pre-receive`, `update`).
</details>

<details>
<summary><strong>4. How does the `commit-msg` hook receive the commit message content to validate it?</strong></summary>

Git passes the filesystem path of the temporary commit message file (typically `.git/COMMIT_EDITMSG`) as the first command-line argument (`$1`) to the `commit-msg` hook script. The script reads or regex-matches the first line of this file and exits with code `0` to accept the message, or exits with non-zero (e.g. `1`) to reject the commit.
</details>

<details>
<summary><strong>5. What is the difference between a `pre-receive` hook and an `update` hook on a Git server?</strong></summary>

- `pre-receive`: Executes once per push transaction. It receives a list of all updated references (`<old-sha> <new-sha> <refname>`) on standard input (`stdin`). If it exits non-zero, none of the references are updated.
- `update`: Executes once for *each* individual branch or reference being pushed. It receives arguments via command-line flags (`$1 refname`, `$2 old-sha`, `$3 new-sha`). It can accept updates for one branch while rejecting updates for another.
</details>

<details>
<summary><strong>6. How can an engineer ensure a `pre-commit` hook scans only staged changes rather than the entire repository?</strong></summary>

By querying Git's index using `git diff --cached --name-only`:
```bash
STAGED_FILES=$(git diff --cached --name-only --diff-filter=ACM)
eslint $STAGED_FILES
```
The `--diff-filter=ACM` ensures that deleted files (`D`) are excluded from linter input, preventing linting errors on nonexistent paths.
</details>

<details>
<summary><strong>7. Why do bash Git hooks written on Windows frequently fail when run on Linux or macOS?</strong></summary>

Windows text editors default to DOS line endings (`\r\n` - Carriage Return + Line Feed). When a bash script containing CRLF is executed on POSIX systems, the kernel interprets the carriage return as part of the binary name in the shebang: `#!/usr/bin/env bash\r`, resulting in `env: 'bash\r': No such file or directory`. The fix is ensuring `.gitattributes` normalizes all hook scripts to LF (`.githooks/* text eol=lf`).
</details>

<details>
<summary><strong>8. What is the role of `prepare-commit-msg`?</strong></summary>

`prepare-commit-msg` executes after Git creates the default commit message template, but *before* the editor is opened for the author. It is used to programmatically prepopulate or format the commit message editor buffer (such as automatically inserting issue tracker ticket numbers extracted from the branch name, or inserting merge commit summaries).
</details>

---

## 11. Practice Exercises

<details>
<summary><strong>Exercise 1: Configuring `core.hooksPath` and Testing an Executable Hook</strong></summary>

**Task**: Create a `.githooks` directory, configure `core.hooksPath`, create an executable `pre-commit` hook that prints a confirmation message, and verify execution on commit.

**Solution**:
```bash
mkdir /tmp/hook-drill && cd /tmp/hook-drill
git init -b main
mkdir .githooks
git config core.hooksPath .githooks

cat << 'EOF' > .githooks/pre-commit
#!/usr/bin/env bash
echo ">>> PRE-COMMIT HOOK EXECUTED SUCCESSFULLY <<<"
exit 0
EOF
chmod +x .githooks/pre-commit

echo "test" > file.txt && git add file.txt
git commit -m "feat: hook test"
# Displays the confirmation message during commit!
```
</details>

<details>
<summary><strong>Exercise 2: Building a Conventional Commits Validator in `commit-msg`</strong></summary>

**Task**: Write a `commit-msg` hook that enforces Conventional Commits (`feat:`, `fix:`, `chore:`). Test that an invalid message is rejected, and a valid message is accepted.

**Solution**:
```bash
cat << 'EOF' > .githooks/commit-msg
#!/usr/bin/env bash
MSG=$(head -n 1 "$1")
if ! echo "$MSG" | grep -qE "^(feat|fix|chore): .+$"; then
    echo "ERROR: Commit message must start with 'feat:', 'fix:', or 'chore:'!"
    exit 1
fi
exit 0
EOF
chmod +x .githooks/commit-msg

echo "change" >> file.txt && git add file.txt

# Attempt invalid message (Rejected!)
git commit -m "bad message" 2>/dev/null || echo "Commit successfully blocked!"

# Attempt valid message (Accepted!)
git commit -m "fix: resolve file issue"
# Commit succeeds!
```
</details>

<details>
<summary><strong>Exercise 3: Testing `--no-verify` Bypass</strong></summary>

**Task**: Bypass the `commit-msg` hook from Exercise 2 using `git commit --no-verify`.

**Solution**:
```bash
echo "bypass" >> file.txt && git add file.txt
git commit --no-verify -m "unconventional message bypassed"

git log -1 --oneline
# Commit succeeded despite failing validation!
```
</details>

<details>
<summary><strong>Exercise 4: Auto-Injecting Branch Ticket with `prepare-commit-msg`</strong></summary>

**Task**: Create a branch `feat/JIRA-404-auth`. Create a `prepare-commit-msg` hook that automatically injects `[JIRA-404]` into the commit message.

**Solution**:
```bash
git switch -c feat/JIRA-404-auth

cat << 'EOF' > .githooks/prepare-commit-msg
#!/usr/bin/env bash
FILE=$1
BRANCH=$(git branch --show-current)
TICKET=$(echo "$BRANCH" | grep -oE '[A-Z]+-[0-9]+' || true)
if [ -n "$TICKET" ]; then
    echo "[$TICKET] $(cat "$FILE")" > "$FILE"
fi
EOF
chmod +x .githooks/prepare-commit-msg

echo "auth" >> file.txt && git add file.txt
git commit -m "feat: implement login"

git log -1 --oneline
# Shows: [JIRA-404] feat: implement login
```
</details>

<details>
<summary><strong>Exercise 5: Staged-Only Secret Scanner</strong></summary>

**Task**: Write a `pre-commit` hook that scans only staged files for the word `AWS_SECRET_KEY`.

**Solution**:
```bash
cat << 'EOF' > .githooks/pre-commit
#!/usr/bin/env bash
for f in $(git diff --cached --name-only --diff-filter=ACM); do
    if grep -q "AWS_SECRET_KEY" "$f"; then
        echo "ERROR: Found AWS_SECRET_KEY in staged file $f!"
        exit 1
    fi
done
exit 0
EOF
chmod +x .githooks/pre-commit
```
</details>

<details>
<summary><strong>Exercise 6: Line Ending Normalization for Hooks</strong></summary>

**Task**: Configure `.gitattributes` to guarantee that all hook files always check out with `LF` line endings across all operating systems.

**Solution**:
```bash
cat << 'EOF' >> .gitattributes
.githooks/* text eol=lf
EOF
git add .gitattributes && git commit -m "chore: enforce LF on hooks"
```
</details>

---

## 12. Summary & Quick Reference Cheat Sheet

| Hook Name | Stage | Common Use Case | Can Bypass with `--no-verify`? |
| :--- | :--- | :--- | :--- |
| `pre-commit` | Before message editor | Linters, secret scanning, formatting | Yes |
| `prepare-commit-msg` | Template preparation | Prepending JIRA ticket from branch | No |
| `commit-msg` | Message validation | Conventional Commits regex validation | Yes |
| `post-commit` | After commit written | Notifications, desktop alerts | No |
| `pre-push` | Before network push | Fast unit tests, branch policy checks | Yes |
| `pre-receive` | Server: before ref update | Signed commit check, file size limits (<100MB)| **NO** (Server side) |
| `update` | Server: per-branch update | Branch-specific access control (RBAC) | **NO** (Server side) |
| `post-receive` | Server: after ref update | Triggers CI pipelines, deployment webhooks | **NO** (Server side) |

---

## 13. Further Reading & Official Documentation

- [Official Git Documentation - githooks](https://git-scm.com/docs/githooks)
- [Pro Git Book - Customizing Git - Git Hooks](https://git-scm.com/book/en/v2/Customizing-Git-Git-Hooks)
- [Lefthook - Fast Polyglot Git Hooks Manager](https://github.com/evilmartians/lefthook)
- [Husky - Modern Native Git Hooks](https://typicode.github.io/husky/)

---

[<- 19-git-lfs.md](./19-git-lfs.md) | [Back to Module Overview](./README.md) | [21-aliases-and-productivity.md ->](./21-aliases-and-productivity.md)
