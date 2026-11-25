<!--
================================================================================
Metadata:
  Title: Code Archaeology & Forensics: git blame & Advanced Log Search
  Track: Git & GitHub / DevOps
  Difficulty: Intermediate
  Estimated Time: 50 minutes
  Prerequisites: 13-diff-patch-and-format-patch.md
  Key Terminology: git blame, .git-blame-ignore-revs, blame.ignoreRevsFile, git blame -CCC, Reverse Blame, Pickaxe (-S), Regex Search (-G)
================================================================================
-->

# Code Archaeology & Forensics: git blame & Advanced Log Search

[<- 13-diff-patch-and-format-patch.md](./13-diff-patch-and-format-patch.md) | [Back to Module Overview](./README.md) | [15-bisect.md ->](./15-bisect.md)

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

In mature engineering organizations, software engineers spend significantly more time reading and diagnosing existing code than writing new lines. When encountering an obscure conditional check or undocumented workaround:
```javascript
if (user.flags & 0x08 && request.headers['x-legacy-mode'] !== 'skip') { ... }
```
The immediate questions are:
- *Why was this rule added?*
- *What production incident or bug report required this edge-case check?*
- *Is this workaround still necessary, or was it meant to be temporary?*

Finding the answers requires **Code Archaeology**.
While naive developers use `git blame` merely to find "who broke the build," senior engineers use `git blame` and forensic `git log` searches as **investigative tools to reconstruct original engineering intent and context**.

By mastering advanced flags—such as **line-move detection (`-CCC`)**, **ignoring mass-reformatting revisions (`.git-blame-ignore-revs`)**, and **Pickaxe search (`-S`)**—you can trace the lineage of any line of code across renames, refactorings, and years of repository history.

---

## 2. Core Concepts

### What `git blame` Actually Displays
`git blame` annotates each line of a file with the metadata of the last commit that modified it:
```text
8a1e2f7b (Alex Morgan 2024-03-15 14:22:01 -0400 42) const MAX_RETRIES = 5;
```
Columns:
1. `8a1e2f7b`: Commit hash that last edited this line.
2. `(Alex Morgan`: Author name.
3. `2024-03-15...`: Timestamp of author commit.
4. `42)`: Current line number in the file.
5. `const MAX_RETRIES = 5;`: File content.

### The Problem of "Formatting Poisoning"
When a team runs a repository-wide automated code formatter (like Prettier, Black, ESLint, or clang-format) across 100,000 lines of code, running standard `git blame` will attribute *every single line in the project* to that reformatting commit, destroying years of original author context!

Modern Git solves this via **`.git-blame-ignore-revs`**:
- A committed list of formatting or mass-refactoring commit hashes.
- Git automatically bypasses those commits during `git blame`, looking *through* the formatting changes to attribute the true original author!

```
Commit 1: Alice authors feature logic (2021)
   │
   ▼
Commit 2: Prettier re-indents the entire repository (2023)
   │
   ▼
Standard git blame ──────────────► Points to Prettier Commit (2023) (Useless!)
git blame with ignoreRevsFile ───► Points to Alice (2021) (Context Preserved!)
```

---

## 3. How It Works Internally

### Copy and Move Detection (`-M` and `-C`)
Lines of code frequently move between functions or files during refactorings.
- **Standard `git blame`**: Attributes the line to the refactoring commit that cut and pasted it.
- **`git blame -M`**: Compares deleted and added lines within the same commit *inside the same file*, tracing lines that were simply moved across lines.
- **`git blame -C`**: Inspects lines that were copied or moved *from other files in the same commit*.
- **`git blame -CCC`**: Inspects lines that were copied or moved from **ANY commit in the repository's entire history**, tracing a function back to its original inception file!

### Reverse Blame (`--reverse`)
Normally, `git blame` walks backwards in time from `HEAD` to find the commit that introduced a line.
What if a critical line of code was **deleted**, and you want to know which commit deleted it?
**`git blame --reverse <start>..<end> <file>`** walks forward in time, showing the last commit where the line still existed and identifying the exact commit that erased it!

---

## 4. Syntax & Usage

### 1. Advanced `git blame` Invocations
```bash
# Blame specific line range (-L <start>,<end>)
git blame -L 40,65 src/auth.ts

# Ignore whitespace changes when assigning attribution (-w)
git blame -w src/auth.ts

# Detect lines moved or copied within the same file (-M)
git blame -M src/auth.ts

# Detect lines moved or copied from ANY file in repository history (-CCC)
git blame -CCC -w src/auth.ts

# Ignore mass-reformatting commits specified in a configuration file
git blame --ignore-revs-file=.git-blame-ignore-revs src/auth.ts

# Reverse blame: find which commit deleted a line between two revisions
git blame --reverse v1.0.0..main -L 10,20 src/auth.ts
```

---

### 2. Forensic `git log` Search Commands
```bash
# Pickaxe (-S): Search for commits where the exact string "RATE_LIMIT" was added or removed
git log -S "RATE_LIMIT" --oneline

# Regex Search (-G): Search for commits where diff hunks match a regex pattern
git log -G "def parse_auth_header\(" -p

# Filter commits by Author
git log --author="Alex Morgan" --since="6 months ago" --oneline

# Trace history of a specific function definition (-L :<funcname>:<file>)
git log -L :validateSession:src/auth.ts

# Search commit messages only for a specific issue number or text
git log --grep="CVE-2024" --grep="SEC-" --all-match
```

---

## 5. Practical Examples

### Example 1: Bypassing Mass-Formatting Commits with `.git-blame-ignore-revs`
Set up automated formatting commit ignore for your team:

```bash
mkdir /tmp/blame-lab && cd /tmp/blame-lab
git init -b main

# 1. Alice writes business logic
cat << 'EOF' > auth.js
function login(u, p) {
if(u==="admin"&&p==="secret"){return true;}
return false;
}
EOF
git add auth.js && git commit -m "feat: implement auth logic by Alice"
ALICE_HASH=$(git rev-parse HEAD)

# 2. Automated tool (Prettier) reformats the entire file
cat << 'EOF' > auth.js
function login(u, p) {
  if (u === "admin" && p === "secret") {
    return true;
  }
  return false;
}
EOF
git commit -am "style: reformat code with prettier"
PRETTIER_HASH=$(git rev-parse HEAD)

# Standard blame attributes everything to Prettier!
git blame auth.js | grep "admin"
# Shows: PRETTIER_HASH (style: reformat code with prettier)

# 3. Create .git-blame-ignore-revs file
echo "# Ignore Prettier Reformat" > .git-blame-ignore-revs
echo "$PRETTIER_HASH" >> .git-blame-ignore-revs
git add .git-blame-ignore-revs && git commit -m "chore: add prettier commit to ignore-revs"

# Configure Git to use this file automatically
git config blame.ignoreRevsFile .git-blame-ignore-revs

# Blame now points through the formatting commit straight to Alice!
git blame auth.js | grep "admin"
# Shows: ALICE_HASH (feat: implement auth logic by Alice)!
```

### Example 2: Tracing Code Relocated from Another File (`-C`)
```bash
# Move login function into a dedicated module
mkdir -p src/services
git mv auth.js src/services/authService.js
git commit -m "refactor: move auth to services directory"

# Standard blame shows the refactoring commit
git blame -L 1,5 src/services/authService.js

# Deep copy detection traces through the move and formatting:
git blame -CCC -w src/services/authService.js
# Confirms original authorship is attributed to Alice!
```

### Example 3: Forensic Discovery with Pickaxe (`-S`)
Find the exact commit that introduced a secret API key or suspicious configuration:

```bash
# Search repository history for commits altering the occurrence count of a token
git log -S "STRIPE_SECRET_KEY" --oneline
```
Output:
```text
8f1e2d4 feat(billing): add direct stripe charge handler
```
Inspect the exact diff of that commit:
```bash
git show 8f1e2d4
# Displays full commit context, commit message, author, and PR link!
```

### Example 4: Function-Level Evolution Tracking (`git log -L`)
```bash
git log -L :login:src/services/authService.js
# Outputs a chronological record of every diff that ever touched 'login',
# ignoring commits that touched other functions in the same file!
```

---

## 6. Best Practices

1. **Commit a `.git-blame-ignore-revs` File in Root**: Whenever executing mass-formatting (Prettier, Black, ESLint), record the commit hash in `.git-blame-ignore-revs` and commit it.
2. **Configure `blame.ignoreRevsFile` Globally**: Configure `git config --global blame.ignoreRevsFile .git-blame-ignore-revs` so every cloned repository respects team ignore files automatically.
3. **Always Combine `git blame` with `-w` (Ignore Whitespace)**: Whitespace-only edits frequently obscure the true author of a line.
4. **Use `git show <hash>` to Read the "Why"**: The output of `git blame` gives you a commit hash; never stop there. Run `git show <hash>` to read the full commit message and discover the architectural justification for the code.
5. **Use Pickaxe (`-S`) Instead of Simple Grep**: Simple text grep only tells you if a string exists *today*. Pickaxe tells you *every historical moment* that string was added or removed across the entire commit graph.

---

## 7. Common Mistakes & Troubleshooting

| Error / Symptom | Root Cause | Solution |
| :--- | :--- | :--- |
| `git blame` shows a formatting commit on every single line | Mass reformatting wiped blame attribution. | Add the formatting commit hash to `.git-blame-ignore-revs` and run `git blame --ignore-revs-file=.git-blame-ignore-revs`. |
| `git log -S` returns commits that only reformatted code | The target string was re-indented or moved across lines. | Add `-w` to ignore whitespace: `git log -S "target" -w`. |
| Blame output is too long to navigate in terminal | Entire file was blamed instead of target section. | Restrict to line range: `git blame -L 120,160 <file>`. |
| `git blame -CCC` takes 30+ seconds to run | Searching across millions of commits in a massive monorepo. | Limit search with `--since="1 year ago"` or constrain line range `-L`. |
| Cannot find who deleted a line from a file | Standard `git blame` only tracks existing lines. | Use reverse blame: `git blame --reverse <start>..<end> -L <range> <file>`. |

---

## 8. Performance, Security & Cost Implications

### Performance
- **Cost of `-CCC` Deep Inspection**: `git blame -C` inspects other files in the same commit. `git blame -CCC` inspects all files across all commits in history. On large repositories, `-CCC` can consume several gigabytes of memory and take 10+ seconds. Use standard `-w` and line ranges `-L` first.

### Security
- **Audit Trails & Incident Forensics**: When a data breach or unauthorized backdoor is discovered, forensic `git log -S` and `git blame` combined with cryptographic signature verification (`git verify-commit`) provides legal and compliance audit evidence of exactly who introduced the payload.

---

## 9. Comparison Table

| Tool / Technique | Scope | Speed | Best Used For |
| :--- | :--- | :--- | :--- |
| **`git blame -w`** | Single file | Fast | Finding who last modified a line |
| **`git blame -CCC`** | Entire repo history | Slower | Tracing code moved across files/refactorings |
| **`git log -S "<str>"`** | All commit diffs | Fast | Finding when a specific token was added/deleted |
| **`git log -G "<regex>"`**| All commit diffs | Medium | Finding diffs matching structural code patterns |
| **`git log -L :<func>:<file>`**| Function in file | Medium | Tracking lifecycle evolution of a single function |

---

## 10. Real-World Interview Questions

<details>
<summary><strong>1. How does `.git-blame-ignore-revs` solve the problem of mass-reformatting commits in Git?</strong></summary>

When a repository runs an automated code formatter (such as Prettier or Black) across thousands of lines of code, standard `git blame` attributes every reformatted line to that formatting commit, erasing original author and contextual history. By recording the formatting commit hash in a `.git-blame-ignore-revs` file and configuring `blame.ignoreRevsFile`, Git automatically ignores those revisions during blame calculation, looking backward through the formatting changes to attribute each line to its true functional author.
</details>

<details>
<summary><strong>2. What is the difference between `git log -S` (Pickaxe) and `git log -G`?</strong></summary>

- `git log -S "<string>"` (Pickaxe): Searches for commits where the **number of occurrences** of the exact string changed (i.e. the string was added or removed). If a commit moves the string without changing how many times it appears, `-S` skips it.
- `git log -G "<regex>"`: Searches for commits where the **diff hunk** matches the regular expression. It flags any commit where lines containing the pattern were touched, added, removed, or modified.
</details>

<details>
<summary><strong>3. What does `git blame -CCC` do and when is it necessary?</strong></summary>

Standard `git blame` attributes lines to the commit that introduced them to the current file. If a file was refactored by moving helper functions into a new module, standard blame points to the refactoring commit.
- `-C` detects lines copied or moved from other files modified in the *same* commit.
- `-CCC` instructs Git to search across **any commit in the entire repository history** for the source of the copied lines, successfully tracing a function back to its original author even across multiple rounds of file splits and renames.
</details>

<details>
<summary><strong>4. How can you find which commit deleted a specific line of code?</strong></summary>

Using **Reverse Blame** (`git blame --reverse`):
```bash
git blame --reverse <commit-where-line-existed>..HEAD -L <line-range> <file>
```
Standard blame walks backward in time to find origins; reverse blame walks *forward* in time from a revision where the line existed toward `HEAD`, pinpointing the exact commit that removed the line.
</details>

<details>
<summary><strong>5. Why is running `git show <commit-hash>` an essential second step after running `git blame`?</strong></summary>

`git blame` only tells you *who* touched the line and *when*. It does not explain *why*. Running `git show <commit-hash>` reveals the full commit object: the complete commit message, the author's explanation of the problem, links to ticket/issue numbers, and the surrounding diff, providing the critical architectural context needed to understand the change.
</details>

<details>
<summary><strong>6. What does `git log -L 50,75:src/server.ts` do?</strong></summary>

It performs a line-range history trace, displaying only the commits and diff hunks that modified lines 50 through 75 of `src/server.ts` over time, while automatically adjusting line numbers as the file grew or shrank in previous revisions.
</details>

<details>
<summary><strong>7. How does the `-w` flag affect `git blame`?</strong></summary>

The `-w` flag instructs `git blame` to **ignore whitespace changes** (indentation changes, trailing spaces, tabs vs spaces, and line terminators). If a line was indented by 2 spaces in a minor cleanup commit, `-w` skips that commit and attributes the line to the author who actually wrote the substantive code.
</details>

<details>
<summary><strong>8. How can an engineer configure Git to always use `.git-blame-ignore-revs` if present?</strong></summary>

By configuring:
```bash
git config --global blame.ignoreRevsFile .git-blame-ignore-revs
```
If `.git-blame-ignore-revs` exists in the repository root, Git automatically reads and applies the ignored revisions for all `git blame` queries.
</details>

---

## 11. Practice Exercises

<details>
<summary><strong>Exercise 1: Basic Line-Range Blame with `-w`</strong></summary>

**Task**: In a test repository, inspect lines 5 through 15 of a file with whitespace changes ignored using `git blame -L 5,15 -w`.

**Solution**:
```bash
mkdir /tmp/blame-drill && cd /tmp/blame-drill
git init -b main
seq 1 20 > numbers.txt
git add numbers.txt && git commit -m "init numbers"

# Run line-range blame
git blame -L 5,15 -w numbers.txt
# Shows attribution strictly for lines 5 through 15!
```
</details>

<details>
<summary><strong>Exercise 2: Setting Up `.git-blame-ignore-revs`</strong></summary>

**Task**: Modify indentation across all lines in `numbers.txt`, record the formatting commit in `.git-blame-ignore-revs`, and verify that blame points to the initial commit.

**Solution**:
```bash
# Indent lines
sed -i 's/^/  /' numbers.txt
git commit -am "style: re-indent numbers"
FORMAT_HASH=$(git rev-parse HEAD)

# Verify standard blame shows formatting commit
git blame -L 1,5 numbers.txt | grep "$FORMAT_HASH"

# Add to ignore revs
echo "$FORMAT_HASH" > .git-blame-ignore-revs
git config blame.ignoreRevsFile .git-blame-ignore-revs

# Verify blame now bypasses format commit!
git blame -L 1,5 numbers.txt
# Attribution points back to initial commit!
```
</details>

<details>
<summary><strong>Exercise 3: Code Archaeology with Pickaxe (`-S`)</strong></summary>

**Task**: Add a secret key constant, make unrelated commits, and locate the exact commit that introduced the key using `git log -S`.

**Solution**:
```bash
echo "SECRET_API_KEY = 'AKIA123456'" >> numbers.txt
git commit -am "feat: add api key constant"

echo "unrelated update" >> numbers.txt && git commit -am "chore: update"

# Use Pickaxe
git log -S "SECRET_API_KEY" --oneline
# Outputs: feat: add api key constant
```
</details>

<details>
<summary><strong>Exercise 4: Tracing Function History with `git log -L`</strong></summary>

**Task**: Create a JavaScript file with two functions. Modify both across separate commits. Use `git log -L :<func>:<file>` to isolate the history of one function.

**Solution**:
```bash
cat << 'EOF' > math.js
function add(a, b) {
    return a + b;
}

function subtract(a, b) {
    return a - b;
}
EOF
git add math.js && git commit -m "add math functions"

# Modify add
sed -i 's/a + b/a + b + 0/' math.js
git commit -am "fix: add logic"

# Modify subtract
sed -i 's/a - b/a - b - 0/' math.js
git commit -am "fix: subtract logic"

# Trace ONLY add function
git log -L :add:math.js --oneline
# Shows only the commits modifying 'add'!
```
</details>

<details>
<summary><strong>Exercise 5: Tracing Moved Lines Across Files with `-C`</strong></summary>

**Task**: Copy the `add` function from `math.js` into `calc.js`. Verify that `git blame -C` detects the copy.

**Solution**:
```bash
cat math.js > calc.js
git add calc.js && git commit -m "refactor: copy math to calc"

# Trace with copy detection
git blame -C calc.js
# Identifies original lines from math.js!
```
</details>

<details>
<summary><strong>Exercise 6: Regex Diff Search (`git log -G`)</strong></summary>

**Task**: Find all commits that added or removed lines containing regex pattern `return [a-z]`.

**Solution**:
```bash
git log -G "return [a-z]" -p
```
</details>

---

## 12. Summary & Quick Reference Cheat Sheet

| Command | Usage | Role |
| :--- | :--- | :--- |
| `git blame <file>` | CLI | Annotates every line with author, timestamp, and commit |
| `git blame -L <s,e> <file>` | Line Range | Blames only specified line numbers |
| `git blame -w <file>` | Ignore Whitespace | Bypasses whitespace and indentation edits |
| `git blame -M <file>` | Move Detection | Traces lines moved within the same file |
| `git blame -CCC <file>` | Full Repo Copy | Traces lines copied/moved from ANY file in history |
| `git blame --ignore-revs-file=<f>`| Ignore Reformatting | Bypasses mass-reformatting commits listed in `<f>` |
| `git blame --reverse <a>..<b> <f>`| Reverse Blame | Finds which commit deleted a specific line |
| `git log -S "<str>"` | Pickaxe Search | Finds commits where `<str>` count changed in diff |
| `git log -G "<regex>"` | Regex Diff Search | Finds commits whose diff matches regular expression |
| `git log -L :<func>:<file>` | Function History | Chronological diff evolution of a single function |

---

## 13. Further Reading & Official Documentation

- [Official Git Documentation - git-blame](https://git-scm.com/docs/git-blame)
- [Official Git Documentation - git-log (Pickaxe options)](https://git-scm.com/docs/git-log#_diff_formatting)
- [GitHub Blog - Ignore white space and mass reformatting in git blame](https://github.blog/changelog/2020-12-15-ignore-commits-in-the-blame-view-beta/)
- [Pro Git Book - Git Tools - Debugging with Git](https://git-scm.com/book/en/v2/Git-Tools-Debugging-with-Git)

---

[<- 13-diff-patch-and-format-patch.md](./13-diff-patch-and-format-patch.md) | [Back to Module Overview](./README.md) | [15-bisect.md ->](./15-bisect.md)
