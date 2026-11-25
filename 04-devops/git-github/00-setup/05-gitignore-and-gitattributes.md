<!--
================================================================================
Metadata:
  Title: .gitignore & .gitattributes Deep Dive
  Track: Git & GitHub / DevOps
  Difficulty: Intermediate
  Estimated Time: 50 minutes
  Prerequisites: 02-install-and-configuration.md
  Key Terminology: .gitignore, .gitattributes, git check-ignore, git rm --cached, text=auto, eol=lf, linguist-generated, export-ignore, filter=lfs
================================================================================
-->

# .gitignore & .gitattributes Deep Dive

[<- 04-gitconfig-in-depth.md](./04-gitconfig-in-depth.md) | [Back to Module Overview](./README.md) | [Next Module: 01-fundamentals ->](../01-fundamentals/README.md)

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

In modern software projects, workspaces are continuously filled with generated artifacts: compiled binaries, bytecode, dependency trees (`node_modules/`, `target/`, `venv/`), environment secrets (`.env`), editor state (`.vscode/`, `.idea/`), and OS metadata files (`.DS_Store`, `Thumbs.db`).

Allowing these untracked files into the repository leads to:
1. **Repository Bloat**: Huge binaries permanently increase repository clone sizes and disk consumption.
2. **Security Breaches**: Committing `.env` or API credentials exposes secrets to the entire development team and commit history.
3. **Spurious Merge Conflicts**: Committing lockfiles or platform-specific binaries causes perpetual conflict between developers.
4. **Cross-Platform Breakage**: Inconsistent newline formats (`CRLF` vs `LF`) break bash scripts on Linux servers.

Git provides two foundational declarative files to control file lifecycle:
- **`.gitignore`**: Defines path patterns that Git must exclude from status tracking and staging.
- **`.gitattributes`**: Defines metadata and lifecycle behaviors for specific file paths (line endings, diff filters, merge strategies, GitHub PR diff collapsing, and Git LFS tracking).

---

## 2. Core Concepts

### Precedence Hierarchy of Git Ignore Sources
Git resolves ignore patterns across multiple sources. Patterns evaluated later in the list override earlier ones:

1. **Global Ignore File (`core.excludesFile`)**:
   - Location: Typically `~/.config/git/ignore` or `~/.gitignore_global`.
   - Purpose: Machine-specific, personal tools (`.DS_Store`, `.idea/`, local editor caches).
2. **Repository-Private Ignore File (`.git/info/exclude`)**:
   - Location: `.git/info/exclude` inside the local repository.
   - Purpose: Team-agnostic exclusions private to this clone. Never committed or shared across remotes.
3. **Root `.gitignore`**:
   - Location: Root directory of the repository (`/.gitignore`).
   - Purpose: Project-wide ignore rules committed and shared across all contributors.
4. **Subdirectory `.gitignore` files**:
   - Location: Nested inside submodules or subfolders (e.g. `packages/frontend/.gitignore`).
   - Purpose: Scoped rules applying strictly to that subtree. Overrides patterns defined in parent directories.
5. **Command Line Options**:
   - Patterns supplied directly to Git commands (e.g., `--exclude`).

```
+-------------------------------------------------------------------------+
|                       Ignore Evaluation Hierarchy                       |
|                                                                         |
|  [Global Excludes: ~/.gitignore_global]                                 |
|         │                                                               |
|         ▼ Overridden by                                                 |
|  [Repository Private: .git/info/exclude]                                |
|         │                                                               |
|         ▼ Overridden by                                                 |
|  [Repository Root: /.gitignore]                                         |
|         │                                                               |
|         ▼ Overridden by                                                 |
|  [Nested Folder: /apps/api/.gitignore]                                  |
|         │                                                               |
|         ▼ Final Rule                                                    |
|  Does path match? -> YES -> Excluded from "git status" and "git add"    |
+-------------------------------------------------------------------------+
```

> [!IMPORTANT]
> **The Golden Rule of `.gitignore`**: `.gitignore` prevents *untracked* files from being added to the Git index. It **never** ignores files that are *already tracked*. If a file was previously committed, adding it to `.gitignore` will NOT remove it from tracking or ignore subsequent edits.

---

## 3. How It Works Internally

### Pattern Matching Engine in Git
When you run `git status` or `git add`, Git traverses directory entries on disk. For each path, Git tests it against the active ignore pattern list:
- **Directory Boundaries**: A pattern ending with a forward slash `/` matches only directories. When a directory matches an ignore rule, Git skips descending into that directory entirely, dramatically accelerating traversal.
- **Root Anchoring**: A leading slash `/` anchors the pattern to the directory where the `.gitignore` file resides.
- **Negation (`!`)**: Negation re-includes a previously ignored path. However: **Git cannot re-include a file if its parent directory is excluded**, because Git prunes directory traversal before reaching the nested file!

```
# WRONG - config.json will STILL BE IGNORED:
secret/
!secret/config.json    <-- Will NOT work because Git never enters 'secret/' directory!

# CORRECT:
secret/*
!secret/config.json    <-- Works because the directory itself is traversed!
```

### Attributes Engine (`.gitattributes`)
While `.gitignore` only acts as a binary gate (tracked vs ignored), `.gitattributes` binds rich behaviors to paths during checkout and commit operations:

```
Working Tree                     Git Index / Object Database
[Physical File on Disk]          [Normalized Git Storage]

        │                                  ▲
        │  Smudge Filter (on checkout)     │  Clean Filter (on commit)
        ▼                                  │
  LF converted to CRLF               CRLF converted to LF
  (if text=auto, eol=crlf)           (if text=auto, eol=lf)
```

---

## 4. Syntax & Usage

### Pattern Syntax for `.gitignore`

| Pattern | Behavior / Scope |
| :--- | :--- |
| `# Comment` | Ignored by parser |
| `logs/` | Matches any directory named `logs` anywhere in the tree |
| `/dist` | Matches `dist` directory or file at the root level only |
| `*.log` | Matches all files ending in `.log` across all directories |
| `!important.log`| Negation rule: re-includes `important.log` |
| `temp?.txt` | Matches `temp1.txt`, `tempa.txt`, but not `temp12.txt` (`?` matches 1 char) |
| `**/build` | Matches `build` directory at any nesting depth (`a/build`, `a/b/c/build`) |
| `doc/**/*.pdf` | Matches all `.pdf` files inside `doc/` and any subdirectories beneath it |

### Diagnosing Ignore Rules with `git check-ignore`
To identify exactly why a file is being ignored (or why it isn't being ignored):

```bash
# Check file and show matching gitignore file and line number
git check-ignore -v src/generated/schema.json

# Output format:
# <source-file>:<line-number>:<pattern>    <queried-path>
# .gitignore:42:*.json                     src/generated/schema.json
```

---

### Modern `.gitattributes` Directives

```gitattributes
# 1. Line ending normalization
* text=auto eol=lf
*.bat text eol=crlf
*.cmd text eol=crlf

# 2. Binary classifications (disable diff & merge calculations)
*.png binary
*.jpg binary
*.ico binary
*.pdf -diff -merge

# 3. Git LFS (Large File Storage) binding
*.psd filter=lfs diff=lfs merge=lfs -text
*.zip filter=lfs diff=lfs merge=lfs -text
*.onnx filter=lfs diff=lfs merge=lfs -text

# 4. GitHub PR Diff Collapsing & Language Detection
# Collapse huge auto-generated schema or lock files in GitHub PR review UI
package-lock.json linguist-generated=true
pnpm-lock.yaml linguist-generated=true
*.min.js linguist-generated=true
*.min.css linguist-generated=true

# 5. Archive exclusions (omitted when creating release zip/tarballs via git archive)
.github export-ignore
.gitignore export-ignore
.gitattributes export-ignore
tests/ export-ignore
```

---

## 5. Practical Examples

### Example 1: Full-Stack Polyglot Production `.gitignore`
A robust `.gitignore` covering Node.js, Python, Java/Go, and IDEs:

```gitignore
# ==============================================================================
# OS Metadata (Better in global gitignore, included here as fallback)
# ==============================================================================
.DS_Store
Thumbs.db
Desktop.ini

# ==============================================================================
# IDEs and Editor State
# ==============================================================================
.idea/
.vscode/*
!.vscode/settings.json
!.vscode/extensions.json
*.swp
*.swo

# ==============================================================================
# Node.js / TypeScript / Frontend
# ==============================================================================
node_modules/
dist/
build/
.next/
.turbo/
coverage/
*.tsbuildinfo

# ==============================================================================
# Python
# ==============================================================================
__pycache__/
*.py[cod]
*$py.class
*.so
.env
.venv/
env/
venv/
.pytest_cache/
.mypy_cache/

# ==============================================================================
# Logs & Secrets
# ==============================================================================
*.log
npm-debug.log*
yarn-debug.log*
yarn-error.log*
.env*.local
.env.production
id_rsa*
*.pem
```

### Example 2: How to Untrack Tracked Files
If an engineer accidentally committed a sensitive `.env` or heavy `node_modules/` before adding it to `.gitignore`:

```bash
# 1. Add the path to .gitignore
echo ".env" >> .gitignore

# 2. Untrack the file from Git index WITHOUT deleting it from your local filesystem
git rm --cached .env

# 3. Commit the removal
git commit -m "chore: stop tracking .env file"
```

To untrack an entire repository cleanly after revising `.gitignore`:
```bash
# Stage removal of all tracked files from index
git rm -r --cached .

# Re-add everything honoring the updated .gitignore
git add .

# Commit changes
git commit -m "chore: purge ignored files from git tracking"
```

### Example 3: Renormalizing Repository Line Endings
When introducing `.gitattributes` to an established repository with corrupted mixed line endings:

```bash
# 1. Define .gitattributes with normalization rules
echo "* text=auto eol=lf" > .gitattributes
git add .gitattributes

# 2. Renormalize all files in the working directory and index
git add --renormalize .

# 3. Commit the normalized tree
git commit -m "chore: normalize repository line endings to LF"
```

### Example 4: Configuring Global User Ignores
Avoid cluttering individual team repositories with personal OS or editor files:

```bash
# 1. Create global ignore file
mkdir -p ~/.config/git
cat <<EOF > ~/.config/git/ignore
.DS_Store
Thumbs.db
.vscode/
.idea/
*.sublime-project
*.sublime-workspace
EOF

# 2. Tell Git where to locate the global excludes file
git config --global core.excludesFile ~/.config/git/ignore
```

### Example 5: Custom Diff Driver in `.gitattributes`
Tell Git to use custom diff engines or syntax-aware diffs for structured files:

```gitattributes
# Use built-in Java/Python hunk function headers in diff output
*.java diff=java
*.py diff=python
*.go diff=golang
```

---

## 6. Best Practices

1. **Place Project Ignores in Root `.gitignore`**: Keep all shared project exclusions in a single, well-documented root file unless working in a massive monorepo.
2. **Keep Personal Tooling in Global Ignores**: Never commit developer-specific preferences (`.idea/workspace.xml`, `.vscode/`) into team repositories; place them in `core.excludesFile`.
3. **Commit `.gitattributes` on Day One**: Prevent line ending wars before they start by declaring `* text=auto eol=lf` in repository roots.
4. **Use `linguist-generated=true`**: Collapse minified code, compiled bundles, and generated OpenAPI/GraphQL schemas in GitHub PR reviews to preserve reviewer focus.
5. **Always Use `git check-ignore -v`**: Never guess why a file is not staging—query Git directly.
6. **Beware the Parent Directory Ignore Trap**: Remember that Git will not evaluate negation rules (`!file.txt`) inside an ignored parent folder (`folder/`).

---

## 7. Common Mistakes & Troubleshooting

| Error / Symptom | Root Cause | Solution |
| :--- | :--- | :--- |
| Added file to `.gitignore`, but `git status` still shows it as modified | The file was already tracked in the index prior to adding it to `.gitignore`. | Run `git rm --cached <file>` and commit the removal. |
| Negation rule `!secret/target.json` does not work | The parent directory `secret/` is ignored, preventing traversal. | Change `secret/` to `secret/*` so Git inspects the directory contents. |
| Every line in repository shows modified after checking out on Windows | `core.autocrlf` or line ending mismatch converted all line endings. | Add `* text=auto eol=lf` to `.gitattributes` and run `git add --renormalize .`. |
| GitHub shows massive 5,000-line diff for lockfiles in PR | Lockfile is not flagged as generated. | Add `package-lock.json linguist-generated=true` to `.gitattributes`. |
| Binary files appear corrupted on checkout | Git treated binary assets (e.g. `.png`, `.pdf`) as text and modified byte sequences. | Add `*.png binary` and `*.pdf -diff -merge` to `.gitattributes`. |
| `git check-ignore` returns nothing for ignored file | File might be excluded via global ignore or `.git/info/exclude`. | Run `git check-ignore -v -n <file>` to inspect all evaluation layers. |

---

## 8. Performance, Security & Cost Implications

### Performance
- **Directory Pruning in Git**: When a large directory like `node_modules/` or `target/` is matched by an ignore pattern with a trailing slash (`node_modules/`), Git immediately skips stat/readdir on all tens of thousands of nested files. If written poorly (e.g., `**/node_modules/**`), Git may perform excessive glob matching.
- **Diff Calculation Savings**: Flagging binary files (`*.bin binary`) prevents Git from wasting CPU cycles computing Myers/histogram diffs on multi-megabyte payloads.

### Security
- **Defense in Depth**: `.gitignore` is the first line of defense against committing secrets. However, `.gitignore` alone is insufficient because developers can accidentally bypass it (`git add -f .env`). Couple `.gitignore` with **pre-commit hooks** (e.g. `gitleaks`) and GitHub **Push Protection**.

---

## 9. Comparison Table

| Feature / Aspect | `.gitignore` | `.gitattributes` |
| :--- | :--- | :--- |
| **Primary Scope** | Excludes untracked files from staging | Configures lifecycle attributes for paths |
| **Evaluated When?** | During `status`, `add`, directory traversal | During `checkout`, `commit`, `diff`, `archive` |
| **Effect on Tracked Files** | None (only affects untracked files) | Applies to all matching tracked files |
| **Line Ending Control** | None | Full control (`text`, `eol=lf`, `eol=crlf`) |
| **GitHub PR UI Integration** | None | Controls PR diff collapsing (`linguist-generated`) |
| **Git LFS Integration** | None | Defines LFS filters (`filter=lfs`) |
| **Export Control** | None | Excludes files from release tarballs (`export-ignore`) |

---

## 10. Real-World Interview Questions

<details>
<summary><strong>1. Why does adding an entry to `.gitignore` fail to ignore a file that is already committed?</strong></summary>

Git's tracking mechanism operates on the Index (staging area). `.gitignore` instructs Git's directory crawler to skip untracked filesystem paths when populating the index. If a file is already tracked, its SHA-1/SHA-256 hash is already recorded in the index tree. Git compares the physical file against the indexed hash; since it already knows about the file, it reports modifications regardless of `.gitignore`. To ignore it, the file must be explicitly removed from the index via `git rm --cached <file>`.
</details>

<details>
<summary><strong>2. Explain why the negation pattern `!nested/file.txt` fails if the pattern `nested/` is also present.</strong></summary>

For performance reasons, Git terminates directory traversal as soon as a directory matches an ignore rule. When Git evaluates `nested/`, it marks the entire directory as excluded and does not inspect any subdirectories or files inside it. Because Git never scans inside `nested/`, it never encounters or evaluates the child negation rule `!nested/file.txt`. To fix this, ignore `nested/*` instead of `nested/`, allowing Git to enter the directory and evaluate child rules.
</details>

<details>
<summary><strong>3. What is the purpose of `git add --renormalize .` and when must it be run?</strong></summary>

When you introduce or modify `.gitattributes` rules for line endings (e.g. adding `* text=auto eol=lf`), Git does not retroactively rewrite already indexed files. Running `git add --renormalize .` forces Git to re-evaluate every tracked file against the current `.gitattributes` rules, applying the appropriate smudge/clean filters and staging any files whose line endings need normalization.
</details>

<details>
<summary><strong>4. How does GitHub Linguist utilize `.gitattributes` to improve PR reviews?</strong></summary>

GitHub Linguist parses repository `.gitattributes` to determine language breakdowns and PR diff views:
- `linguist-generated=true`: Automatically collapses huge generated files (lockfiles, minified JS, compiled schemas) in PR diffs so reviewers don't have to scroll through thousands of lines of machine code.
- `linguist-vendored`: Marks third-party dependencies as vendored, omitting them from the repository's primary language statistics bar.
- `linguist-detectable=false`: Excludes specific folders (like test mocks or templates) from language breakdown calculations.
</details>

<details>
<summary><strong>5. What is the difference between `.git/info/exclude` and `.gitignore`?</strong></summary>

Both files use the exact same pattern-matching syntax and behavior. However:
- `.gitignore` is committed into the repository and version-controlled, sharing ignore rules across the entire team and remote clones.
- `.git/info/exclude` resides strictly inside the local clone's `.git/` metadata folder. It is never pushed or shared. It is intended for private, developer-specific ignore patterns that should not be forced on other contributors.
</details>

<details>
<summary><strong>6. How can an engineer identify which specific line of which file caused a path to be ignored?</strong></summary>

By executing the `git check-ignore` debugging command:
```bash
git check-ignore -v <path>
```
The `-v` (verbose) flag outputs the matching ignore file, the exact line number, and the pattern that triggered the match.
</details>

<details>
<summary><strong>7. What does the `export-ignore` attribute accomplish in `.gitattributes`?</strong></summary>

When creating release source bundles via `git archive` (which GitHub also triggers when generating release zip/tarball downloads), paths tagged with `export-ignore` are completely stripped from the resulting distribution archive. This keeps release bundles lean by omitting test suites, CI workflows, and internal documentation.
</details>

<details>
<summary><strong>8. Why is setting `* text=auto` in `.gitattributes` preferred over relying on individual developers' `core.autocrlf`?</strong></summary>

`core.autocrlf` is a client-side workstation configuration that must be manually configured by every developer. If a single developer forgets to set it, they can commit `CRLF` into the repository and corrupt line endings for everyone. `.gitattributes` is checked into the repository root, ensuring that line ending normalization is deterministically enforced for all contributors and CI runners regardless of local client settings.
</details>

---

## 11. Practice Exercises

<details>
<summary><strong>Exercise 1: Diagnosing Ignored Paths with `git check-ignore`</strong></summary>

**Task**: Create a test repository with nested ignore rules, add a file that gets ignored, and use `git check-ignore` to determine which rule caught it.

**Solution**:
```bash
mkdir /tmp/ignore-lab && cd /tmp/ignore-lab
git init
echo "*.log" > .gitignore
mkdir logs && touch logs/server.log

git status -s
# Nothing displayed because logs/server.log is ignored

# Diagnose matching rule:
git check-ignore -v logs/server.log
# Output: .gitignore:1:*.log  logs/server.log
```
</details>

<details>
<summary><strong>Exercise 2: Untracking an Accidentally Committed File</strong></summary>

**Task**: Commit a file named `credentials.env`. Then configure `.gitignore` to exclude `.env` files and untrack the file from Git without deleting it from disk.

**Solution**:
```bash
touch credentials.env
git add credentials.env
git commit -m "feat: add config"

# Add to gitignore
echo "*.env" >> .gitignore

# Untrack from index
git rm --cached credentials.env

# Verify file still exists on disk
ls credentials.env
# Output: credentials.env exists!

# Check status: credentials.env is deleted in index, untracked on disk
git commit -m "chore: stop tracking credentials"
```
</details>

<details>
<summary><strong>Exercise 3: Fixing the Parent Directory Negation Trap</strong></summary>

**Task**: Attempt to ignore an entire directory `build/` while attempting to keep `build/keep.txt` tracked. Observe the failure and fix the rule.

**Solution**:
```bash
mkdir build && touch build/temp.bin build/keep.txt

# Faulty pattern:
echo "build/" > .gitignore
echo "!build/keep.txt" >> .gitignore
git status
# keep.txt is NOT shown because build/ was pruned!

# Working pattern:
cat <<EOF > .gitignore
build/*
!build/keep.txt
EOF

git status
# keep.txt is now recognized and can be staged!
```
</details>

<details>
<summary><strong>Exercise 4: Creating a Repository-Private Exclude Rule</strong></summary>

**Task**: Exclude a personal scratch file `notes.txt` using `.git/info/exclude` so that `.gitignore` remains unmodified.

**Solution**:
```bash
touch notes.txt
git status -s
# Output: ?? notes.txt

# Add to repository-private exclude:
echo "notes.txt" >> .git/info/exclude

git status -s
# Output: clean! notes.txt is ignored without modifying .gitignore
```
</details>

<details>
<summary><strong>Exercise 5: Establishing `.gitattributes` Line Ending Normalization</strong></summary>

**Task**: Create a `.gitattributes` file that normalizes all text to LF, marks shell scripts as LF, and marks batch scripts as CRLF.

**Solution**:
```bash
cat <<EOF > .gitattributes
# Default text normalization
* text=auto eol=lf

# Explicit OS scripts
*.sh text eol=lf
*.bat text eol=crlf
*.cmd text eol=crlf
EOF

git add .gitattributes
git commit -m "chore: establish gitattributes line ending policies"
```
</details>

<details>
<summary><strong>Exercise 6: Configuring GitHub PR Diff Collapsing via Linguist</strong></summary>

**Task**: Add attributes to collapse a mock generated file `schema-generated.json` and verify the syntax.

**Solution**:
```bash
cat <<EOF >> .gitattributes

# Collapse generated schema in GitHub PRs
schema-generated.json linguist-generated=true
EOF

git add .gitattributes
git commit -m "chore: collapse generated schema in pull requests"
```
</details>

---

## 12. Summary & Quick Reference Cheat Sheet

| Directive / Command | Target File | Impact |
| :--- | :--- | :--- |
| `*.log` | `.gitignore` | Ignores all `.log` files in all directories |
| `/dist/` | `.gitignore` | Ignores `dist` folder at repository root only |
| `!keep.log` | `.gitignore` | Negates ignore rule; re-includes `keep.log` |
| `git check-ignore -v <path>` | CLI | Displays the exact ignore rule and line matching a path |
| `git rm --cached <file>` | CLI | Removes file from Git index while preserving physical file |
| `git add --renormalize .` | CLI | Applies `.gitattributes` normalization across all tracked files |
| `* text=auto eol=lf` | `.gitattributes` | Normalizes all text files to LF in Git object store |
| `*.png binary` | `.gitattributes` | Marks file as binary, disabling text diffs and merges |
| `*.min.js linguist-generated`| `.gitattributes` | Auto-collapses file diffs in GitHub pull requests |
| `tests/ export-ignore` | `.gitattributes` | Omits folder when generating release zip via `git archive` |

---

## 13. Further Reading & Official Documentation

- [Official Git Documentation - gitignore](https://git-scm.com/docs/gitignore)
- [Official Git Documentation - gitattributes](https://git-scm.com/docs/gitattributes)
- [GitHub Documentation - Customizing repository language statistics and diffs](https://docs.github.com/en/repositories/working-with-files/managing-files/customizing-how-changed-files-appear-on-github)
- [GitHub Linguist Overrides](https://github.com/github-linguist/linguist/blob/master/docs/overrides.md)

---

[<- 04-gitconfig-in-depth.md](./04-gitconfig-in-depth.md) | [Back to Module Overview](./README.md) | [Next Module: 01-fundamentals ->](../01-fundamentals/README.md)
