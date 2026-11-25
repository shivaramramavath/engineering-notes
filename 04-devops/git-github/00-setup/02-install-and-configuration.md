<!--
================================================================================
Metadata:
  Title: Git Installation & Configuration
  Track: Git & GitHub / DevOps
  Difficulty: Beginner
  Estimated Time: 45 minutes
  Prerequisites: 01-getting-started.md
  Key Terminology: git config, Configuration Scopes (system, global, local, worktree), core.autocrlf, credential.helper, init.defaultBranch, pull.rebase
================================================================================
-->

# Git Installation & Configuration

[<- 01-getting-started.md](./01-getting-started.md) | [Back to Module Overview](./README.md) | [03-ssh-keys-and-gpg-or-ssh-signing.md ->](./03-ssh-keys-and-gpg-or-ssh-signing.md)

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

Installing Git properly and establishing standard configurations is the foundational prerequisite for stable, predictable source control management. A default, unconfigured Git environment exhibits several pain points:
- Commits are attributed to generic machine hostnames or system usernames rather than verified developer identities.
- Line ending disparities between Windows (`CRLF` - `\r\n`) and Unix/macOS (`LF` - `\n`) cause massive, spurious multi-thousand-line diffs across shared cross-platform repositories.
- Legacy defaults (such as default branch naming to `master`, or fast-forward pull ambiguity) introduce friction in modern CI/CD pipelines.

By establishing rigorous installation procedures and tuning configuration parameters—including user identity, default branches, editor integrations, line ending normalization, and credential management—development teams prevent commit poisoning, merge conflicts, and credential leakage across operating systems.

---

## 2. Core Concepts

### Configuration Scopes and Precedence
Git manages settings across four distinct hierarchical scopes. More specific scopes override broader ones:

1. **System Scope (`--system`)**:
   - Applies to every user on the operating system and all their repositories.
   - Stored in `/etc/gitconfig` on Linux/macOS, or `%ProgramData%\Git\config` (or `C:\Program Files\Git\etc\gitconfig`) on Windows.
   - Requires administrative privileges (`sudo` / elevated console) to edit.
2. **Global Scope (`--global`)**:
   - Applies to the currently logged-in operating system user across all repositories they touch.
   - Stored in `~/.gitconfig` or `~/.config/git/config` on Unix/macOS, and `%USERPROFILE%\.gitconfig` on Windows.
   - Primary location for personal defaults: identity, global aliases, default branch names.
3. **Local Scope (`--local`)**:
   - Applies exclusively to the single specific repository where the command is executed.
   - Stored in `.git/config` inside the repository root.
   - Overrides global and system configurations. Essential for repository-specific email addresses (e.g. personal vs. work client repositories). Default scope when `--global` or `--system` is omitted.
4. **Worktree Scope (`--worktree`)**:
   - Applies to a specific linked working tree when utilizing `git worktree`.
   - Stored in `.git/config.worktree` (requires `extensions.worktreeConfig = true`).

```
+-------------------------------------------------------------+
|                     Configuration Hierarchy                 |
|                                                             |
|   +-----------------------------------------------------+   |
|   | 1. System Config (/etc/gitconfig)                   |   |
|   |    (Lowest Priority - Applies to all users)         |   |
|   +-----------------------------------------------------+   |
|                              |                              |
|                              v Overridden by                |
|   +-----------------------------------------------------+   |
|   | 2. Global Config (~/.gitconfig)                     |   |
|   |    (User-wide defaults: name, email, global aliases)|   |
|   +-----------------------------------------------------+   |
|                              |                              |
|                              v Overridden by                |
|   +-----------------------------------------------------+   |
|   | 3. Local Config (.git/config)                       |   |
|   |    (Repository-specific remotes, branches, hooks)   |   |
|   +-----------------------------------------------------+   |
|                              |                              |
|                              v Overridden by                |
|   +-----------------------------------------------------+   |
|   | 4. Worktree Config (.git/config.worktree)           |   |
|   |    (Highest Scope Priority - Worktree specific)     |   |
|   +-----------------------------------------------------+   |
|                              |                              |
|                              v Overridden by                |
|   +-----------------------------------------------------+   |
|   | 5. CLI Flags (-c key=value) & Environment Variables |   |
|   |    (Absolute Precedence - Single command execution) |   |
|   +-----------------------------------------------------+   |
+-------------------------------------------------------------+
```

---

## 3. How It Works Internally

### Storage Engine of Git Configuration
Git configuration files use standard INI-style plain text formatting. Sections are demarcated by square brackets `[section "subsection"]`, followed by `key = value` pairs.

When Git evaluates a configuration value (e.g., `git config user.name`), it scans files in ascending precedence order:
```
/etc/gitconfig -> ~/.gitconfig -> .git/config -> .git/config.worktree
```
The last encountered value wins. When flags like `-c <key>=<value>` or environment variables (e.g., `GIT_AUTHOR_NAME`) are supplied on the command line, they supersede all on-disk configuration files for that process execution.

### Line Ending Mechanics: `core.autocrlf` vs `.gitattributes`
Different operating systems employ different byte sequences to terminate lines in plain text:
- **POSIX / Linux / macOS**: Line Feed (`LF` = `0x0A` = `\n`).
- **Windows (DOS)**: Carriage Return + Line Feed (`CRLF` = `0x0D 0x0A` = `\r\n`).

If unmanaged, a Windows developer committing a file with `CRLF` changes every single line in Git's object database, destroying blame history and causing merge conflicts.

```
Windows Working Tree (CRLF)           Git Object Database (LF)           Linux/macOS Working Tree (LF)
      [\r\n]                                  [\n]                                  [\n]
        |                                       ^                                     ^
        |--- core.autocrlf=true (commit) -------|                                     |
        |    Converts CRLF -> LF                |                                     |
        |                                       |--- core.autocrlf=input (checkout) --|
        |<-- core.autocrlf=true (checkout) -----|    Leaves LF as LF                  |
             Converts LF -> CRLF                |                                     |
```

Configurations:
- `core.autocrlf = true`: On Windows, Git converts `CRLF` to `LF` upon commit, and converts `LF` to `CRLF` upon checkout into the working tree.
- `core.autocrlf = input`: On Linux/macOS, Git converts `CRLF` to `LF` upon commit, but leaves `LF` untouched on checkout.
- `core.autocrlf = false`: Disables automated translation. (Recommended only when `.gitattributes` explicitly governs normalization).

---

## 4. Syntax & Usage

### Installing Git Across Platforms

#### Debian / Ubuntu Linux:
```bash
sudo apt update
sudo apt install -y git
git --version
```

#### RHEL / Fedora / CentOS:
```bash
sudo dnf install -y git
# Or on enterprise RHEL 8/9:
sudo yum install -y git
```

#### macOS (Homebrew or Xcode Command Line Tools):
```bash
# Via Homebrew (preferred for latest releases):
brew install git

# Via Apple Command Line Tools:
xcode-select --install
```

#### Windows (winget or Git for Windows):
```powershell
# Using Windows Package Manager (winget)
winget install --id Git.Git -e --source winget

# Or using Scoop
scoop install git
```

---

### Core Configuration Commands

```bash
# Set author identity globally
git config --global user.name "Alex Morgan"
git config --global user.email "alex.morgan@company.com"

# Set default branch name for new repositories
git config --global init.defaultBranch main

# Set default code editor (VS Code, Vim, Nano)
git config --global core.editor "code --wait"
# Or Vim:
# git config --global core.editor "vim"

# Set line ending handling
# On Windows:
git config --global core.autocrlf true
# On Linux / macOS:
git config --global core.autocrlf input

# Enforce pull strategy to avoid unintended merge commits
git config --global pull.rebase true

# Automatically set upstream branch on push (Git 2.37+)
git config --global push.autoSetupRemote true

# Inspect all configuration values with origin file tracking
git config --list --show-origin
```

---

## 5. Practical Examples

### Example 1: Multi-Identity Configuration with Conditional Includes
When working on both personal and enterprise repositories on the same machine, conditional includes dynamically switch identities based on directory paths.

In `~/.gitconfig`:
```ini
[user]
    name = Alex Morgan
    email = alex.personal@gmail.com

[init]
    defaultBranch = main

# Conditionally include work identity for all repos inside ~/work/
[includeIf "gitdir:~/work/"]
    path = ~/.gitconfig-work
```

In `~/.gitconfig-work`:
```ini
[user]
    name = Alex Morgan
    email = alex.morgan@enterprise-corp.com
[core]
    sshCommand = ssh -i ~/.ssh/id_ed25519_work
```

Verification:
```bash
mkdir -p ~/work/project-alpha && cd ~/work/project-alpha
git init
git config user.email
# Outputs: alex.morgan@enterprise-corp.com
```

### Example 2: Configuring Git Credential Manager (GCM)
Modern Git workflows authenticate over HTTPS using Git Credential Manager with OAuth / Personal Access Tokens (PAT).

```bash
# On Windows / macOS / Linux with GUI:
git config --global credential.helper manager

# On headless Linux servers (store credentials in encrypted memory cache for 1 hour):
git config --global credential.helper 'cache --timeout=3600'

# Store plaintext credentials on disk (USE ONLY IN ISOLATED LAB ENVIRONMENTS):
# git config --global credential.helper store
```

### Example 3: Essential Productivity Aliases
Simplify verbose Git operations by configuring standard aliases:

```bash
git config --global alias.st status
git config --global alias.co checkout
git config --global alias.br branch
git config --global alias.ci commit

# Formatted graph log
git config --global alias.lg "log --color --graph --pretty=format:'%Cred%h%Creset -%C(yellow)%d%Creset %s %Cgreen(%cr) %C(bold blue)<%an>%Creset' --abbrev-commit"

# Quick unstage
git config --global alias.unstage "restore --staged"

# Show last commit details
git config --global alias.last "log -1 HEAD --stat"
```

### Example 4: Configuring External Diff and Merge Tools (VS Code)
Integrate Visual Studio Code as the interactive 3-way merge conflict resolver:

```bash
git config --global diff.tool vscode
git config --global difftool.vscode.cmd 'code --wait --diff "$LOCAL" "$REMOTE"'

git config --global merge.tool vscode
git config --global mergetool.vscode.cmd 'code --wait "$MERGED"'
git config --global mergetool.keepBackup false
```

### Example 5: Auditing and Unsetting Configurations
Diagnose erroneous or conflicting configuration keys:

```bash
# Check the exact origin of user.email
git config --show-origin user.email
# Output: file:/home/alex/.gitconfig    alex.personal@gmail.com

# Unset a locally set email to fall back to global
git config --local --unset user.email

# Remove an entire configuration section
git config --global --remove-section alias
```

---

## 6. Best Practices

1. **Explicit Identity Declaration**: Always set `user.name` and `user.email` immediately following Git installation. Ensure `user.email` matches the verified email address configured on your GitHub/GitLab account.
2. **Standardize on `main`**: Set `git config --global init.defaultBranch main` to align local repositories with modern cloud platform defaults.
3. **Adopt `push.autoSetupRemote true`**: Eliminate the repetitive requirement to run `git push --set-upstream origin <branch>` on first push.
4. **Use `pull.rebase true` or `pull.ff only`**: Avoid generating automatic "Merge branch 'main' of github.com" commits whenever developers sync their local feature branches.
5. **Protect Against Windows Line Ending Disasters**: Ensure `core.autocrlf` is configured or, preferably, establish repository-level `.gitattributes` files (`* text=auto eol=lf`).
6. **Store Work Credentials Separately**: Leverage `includeIf` directives to ensure client/enterprise codebases never accidentally commit under personal email identities.

---

## 7. Common Mistakes & Troubleshooting

| Error / Symptom | Root Cause | Solution |
| :--- | :--- | :--- |
| `Author identity unknown: Please tell me who you are.` | `user.name` or `user.email` is not configured in any scope. | Run `git config --global user.name "Your Name"` and `git config --global user.email "you@example.com"`. |
| Every single line in a file shows as modified in `git diff`. | Line ending mismatch (`CRLF` vs `LF`) converted line terminators across the file. | Configure `core.autocrlf` correctly, commit a `.gitattributes` file with `* text=auto`, and run `git add --renormalize .`. |
| `fatal: not in a git directory` when running `git config --local` | Attempted to set or inspect local repository configuration outside of a Git working tree. | Navigate into an initialized Git directory before using `--local`, or use `--global`. |
| Git repeatedly prompts for username and password on every HTTPS push/fetch. | No credential helper configured, or personal access token expired. | Configure `credential.helper manager` (or `cache`), and generate a valid GitHub Fine-Grained Personal Access Token. |
| Pull creates unintended "Merge branch..." commits. | `pull.rebase` is set to false or unset, defaulting to non-fast-forward merge. | Run `git config --global pull.rebase true` or `git config --global pull.ff only`. |
| Accidental commit using company email in open-source personal repository. | Single global email configured without path-based conditional includes. | Use `[includeIf "gitdir:~/work/"]` in `~/.gitconfig` or set `git config --local user.email "personal@email.com"`. |

---

## 8. Performance, Security & Cost Implications

### Performance
- **Reftable Backend**: On repositories with tens of thousands of refs, modern Git (2.45+) supports the `reftable` storage engine, which replaces single-file loose refs with binary block tables, accelerating ref lookups by up to 10x.
- **FSMonitor**: On huge repositories (monorepos with 100k+ files), turn on the built-in file system monitor:
  ```bash
  git config --global core.fsmonitor true
  git config --global core.untrackedCache true
  ```
  This reduces `git status` execution times from several seconds to milliseconds.

### Security
- **Credential Storage**: Avoid setting `credential.helper store` without encryption. It writes personal access tokens and passwords in plaintext to `~/.git-credentials`. Use OS-native keychains (`credential.helper manager` on Windows/macOS, `libsecret` on Linux).
- **Safe Directory Protection (CVE-2022-24765)**: Git restricts operations when a repository directory is owned by a different OS user. If running Git inside Docker or shared network drives, explicitly declare safe directories:
  ```bash
  git config --global --add safe.directory /workspace
  ```

---

## 9. Comparison Table

| Attribute | System Scope (`--system`) | Global Scope (`--global`) | Local Scope (`--local`) |
| :--- | :--- | :--- | :--- |
| **Storage Path (Unix)** | `/etc/gitconfig` | `~/.gitconfig` | `.git/config` |
| **Storage Path (Windows)** | `C:\Program Files\Git\etc\gitconfig` | `%USERPROFILE%\.gitconfig` | `.git\config` |
| **Target Scope** | All OS users & all repos | Current user & all repos | Single repository only |
| **Privileges Required** | Root / Administrator | Standard User | Repository write access |
| **Precedence** | Lowest (Overridden by global) | Medium (Overridden by local) | High (Overrides global & system) |
| **Typical Use Cases** | System proxy, site policies | User identity, aliases, editor | Unique remotes, repo work email |

---

## 10. Real-World Interview Questions

<details>
<summary><strong>1. Explain the order of precedence in Git configuration resolution.</strong></summary>

Git evaluates configuration settings from broadest to most specific. When resolving a configuration key, Git reads settings in the following sequence:
1. System-wide configuration (`/etc/gitconfig`).
2. User-specific global configuration (`~/.gitconfig` or `~/.config/git/config`).
3. Repository-specific local configuration (`.git/config`).
4. Worktree-specific configuration (`.git/config.worktree`, if enabled).
5. Command-line flags passed via `-c <key>=<value>` or environment variables (e.g. `GIT_CONFIG_PARAMETERS`, `GIT_AUTHOR_EMAIL`).

The most specific scope always overrides values defined in broader scopes.
</details>

<details>
<summary><strong>2. How does `core.autocrlf` resolve line ending discrepancies across Windows and Unix environments?</strong></summary>

Windows uses Carriage Return + Line Feed (`\r\n` or `CRLF`), whereas Unix/macOS uses Line Feed (`\n` or `LF`). If unhandled, committing files across OS platforms creates massive false diffs.
- `core.autocrlf = true` (standard for Windows): Converts `LF` to `CRLF` when checking files out to the working directory, and converts `CRLF` to `LF` when committing into the Git object store.
- `core.autocrlf = input` (standard for Linux/macOS): Ensures that if a file containing `CRLF` is committed, it is converted to `LF`. On checkout, it leaves existing line endings unchanged.
- `core.autocrlf = false`: Disables automated translation.
Modern repositories typically complement or override this setting using a `.gitattributes` file.
</details>

<details>
<summary><strong>3. What is the difference between `credential.helper store` and `credential.helper manager`?</strong></summary>

- `credential.helper store`: Saves credentials (usernames and personal access tokens) in plain text inside a file on disk (default `~/.git-credentials`). Anyone with read access to the filesystem can compromise the account.
- `credential.helper manager` (Git Credential Manager - GCM): Integrates directly with operating system credential vaults (Windows Credential Manager, macOS Keychain, Linux Secret Service). It supports multi-factor authentication (MFA), OAuth web flows, and securely stores tokens encrypted with system keys.
</details>

<details>
<summary><strong>4. How do you configure Git to use separate email addresses for personal and work projects automatically?</strong></summary>

Use conditional configuration includes via the `includeIf` directive in `~/.gitconfig`:
```ini
[user]
    name = Alex Developer
    email = alex@personal.org

[includeIf "gitdir:~/work/"]
    path = ~/.gitconfig-work
```
Inside `~/.gitconfig-work`:
```ini
[user]
    email = alex@work-enterprise.com
```
Any repository cloned inside `~/work/` automatically resolves `user.email` to the enterprise address, while repositories elsewhere default to the personal address.
</details>

<details>
<summary><strong>5. Why is `pull.rebase true` often preferred over default merge pull in team workflows?</strong></summary>

By default, running `git pull` when the remote has upstream commits and the local branch has local commits creates a 3-way merge commit ("Merge branch 'main' of ..."). This pollutes commit history with empty merge bubbles. Setting `pull.rebase = true` automatically replays local commits on top of the fetched upstream branch, preserving a clean, linear commit graph.
</details>

<details>
<summary><strong>6. How can an engineer verify which configuration file is responsible for a specific setting?</strong></summary>

Execute `git config --list --show-origin` or target a specific key with:
```bash
git config --show-origin <key>
# Example:
git config --show-origin user.email
```
This prints both the absolute file path of the configuration file and the corresponding key-value pair.
</details>

<details>
<summary><strong>7. What security mechanism was introduced in Git 2.35.2 via `safe.directory`?</strong></summary>

Git addresses CVE-2022-24765 by verifying that the owner of the `.git` directory matches the current operating system user executing the command. In multi-user systems, Windows shared drives, or containerized environments (Docker mounts), an unprivileged user could place a malicious `.git/config` executable hook in a directory that an administrative user later accesses. If ownership does not match, Git halts execution unless explicitly whitelisted with `git config --global --add safe.directory <path>`.
</details>

<details>
<summary><strong>8. What is the effect of configuring `push.autoSetupRemote true`?</strong></summary>

Prior to Git 2.37, pushing a newly created local branch for the first time resulted in `fatal: The current branch has no upstream branch. To push the current branch and set the remote as upstream, use git push --set-upstream origin <branch>`. Enabling `push.autoSetupRemote = true` instructs Git to automatically create the tracking reference on the default remote without requiring manual `--set-upstream` flags.
</details>

---

## 11. Practice Exercises

<details>
<summary><strong>Exercise 1: Basic Identity and Global Configuration Setup</strong></summary>

**Task**: Configure your workstation with your full name, developer email, default branch `main`, and VS Code as your default editor. Then verify the settings.

**Solution**:
```bash
git config --global user.name "Jordan Dev"
git config --global user.email "jordan@example.com"
git config --global init.defaultBranch main
git config --global core.editor "code --wait"

# Verify:
git config --global user.name
git config --global user.email
git config --global init.defaultBranch
git config --global core.editor
```
</details>

<details>
<summary><strong>Exercise 2: Diagnosing Configuration Hierarchy</strong></summary>

**Task**: Create a test repository. Set a local `user.email` that differs from your global email. Inspect the configuration using `--show-origin` to confirm local overrides global.

**Solution**:
```bash
mkdir /tmp/config-test && cd /tmp/config-test
git init
git config --local user.email "local-override@project.internal"

# Check active email:
git config user.email
# Returns: local-override@project.internal

# Verify origins:
git config --show-origin user.email
```
</details>

<details>
<summary><strong>Exercise 3: Building a Useful Log Alias</strong></summary>

**Task**: Create a global alias named `git tree` that outputs a colorized, compact commit graph displaying commit hash, relative time, author name, refs, and subject.

**Solution**:
```bash
git config --global alias.tree "log --graph --oneline --decorate --all"

# Test inside any repository:
git tree
```
</details>

<details>
<summary><strong>Exercise 4: Implementing Directory-Based Conditional Git Configurations</strong></summary>

**Task**: Configure your global `~/.gitconfig` so that any repository inside `~/github-personal/` uses email `me@personal.io`, while any repository inside `~/github-work/` uses `me@corp.com`.

**Solution**:
1. In `~/.gitconfig`:
```ini
[includeIf "gitdir:~/github-personal/"]
    path = ~/.gitconfig-personal
[includeIf "gitdir:~/github-work/"]
    path = ~/.gitconfig-work
```
2. Create `~/.gitconfig-personal`:
```ini
[user]
    email = me@personal.io
```
3. Create `~/.gitconfig-work`:
```ini
[user]
    email = me@corp.com
```
4. Verify by creating directories, initializing Git, and checking `git config user.email`.
</details>

<details>
<summary><strong>Exercise 5: Configuring Modern Safe Pull and Push Behaviors</strong></summary>

**Task**: Configure Git globally to automatically track remote branches on initial push and automatically rebase local commits when pulling.

**Solution**:
```bash
git config --global push.autoSetupRemote true
git config --global pull.rebase true

# Verify settings in ~/.gitconfig
git config --global --get push.autoSetupRemote
git config --global --get pull.rebase
```
</details>

<details>
<summary><strong>Exercise 6: Cleaning Up Legacy or Erroneous Settings</strong></summary>

**Task**: You accidentally set an invalid alias `git foo` in your global configuration. Identify and completely remove the alias without corrupting the rest of the configuration file.

**Solution**:
```bash
# Add erroneous alias:
git config --global alias.foo "status --invalid-flag"

# Unset specific key:
git config --global --unset alias.foo

# Verify removal:
git config --global --get alias.foo
# Exit code 1 (no output) indicates success
```
</details>

---

## 12. Summary & Quick Reference Cheat Sheet

| Command | Description |
| :--- | :--- |
| `git config --global user.name "Name"` | Set global commit author name |
| `git config --global user.email "Email"` | Set global commit author email |
| `git config --global init.defaultBranch main` | Set default branch for `git init` |
| `git config --global core.editor "code --wait"` | Set default commit editor to VS Code |
| `git config --global core.autocrlf true` | Convert CRLF to LF on commit, LF to CRLF on checkout (Windows) |
| `git config --global core.autocrlf input` | Convert CRLF to LF on commit, leave LF intact (Linux/macOS) |
| `git config --global pull.rebase true` | Rebase local branch onto upstream during pull |
| `git config --global push.autoSetupRemote true` | Auto-configure upstream tracking on first push |
| `git config --global credential.helper manager` | Securely store HTTPS credentials in OS keychain |
| `git config --list --show-origin` | List all resolved settings along with their source files |
| `git config --local --unset <key>` | Remove a setting from local repository configuration |

---

## 13. Further Reading & Official Documentation

- [Official Git Documentation - git-config](https://git-scm.com/docs/git-config)
- [Pro Git Book - First-Time Git Setup](https://git-scm.com/book/en/v2/Getting-Started-First-Time-Git-Setup)
- [Git Credential Manager Documentation](https://github.com/git-ecosystem/git-credential-manager)
- [GitHub Docs - Setting your commit email address](https://docs.github.com/en/account-and-profile/setting-up-and-managing-your-personal-account-on-github/managing-email-preferences/setting-your-commit-email-address)

---

[<- 01-getting-started.md](./01-getting-started.md) | [Back to Module Overview](./README.md) | [03-ssh-keys-and-gpg-or-ssh-signing.md ->](./03-ssh-keys-and-gpg-or-ssh-signing.md)
