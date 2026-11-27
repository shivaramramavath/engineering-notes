<!--
================================================================================
Metadata:
  Title: Globbing, Quoting & Shell Expansion
  Track: Linux & Systems Engineering / DevOps
  Difficulty: Intermediate
  Estimated Time: 50 minutes
  Prerequisites: 02-shell/01-shell-basics.md
  Key Terminology: Shell Expansions, Brace Expansion, Tilde Expansion, Parameter Expansion, Command Substitution, Arithmetic Expansion, Word Splitting, Pathname Expansion (Globbing), Quote Removal, extglob, nullglob, failglob
================================================================================
-->

# Globbing, Quoting & Shell Expansion

[<- 02-pipes-redirection-and-file-descriptors.md](./02-pipes-redirection-and-file-descriptors.md) | [Back to Module Overview](./README.md) | [04-text-processing.md ->](./04-text-processing.md)

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

In Bash and POSIX-compliant shells, the text an engineer types on the command line is rarely what reaches the underlying application binary. Before passing arguments to `execve()`, the shell passes the input line through a multi-stage parser known as **Shell Expansion**.

Understanding shell expansion is arguably the single most critical factor in writing secure, bug-free automation:
- **Catastrophic Deletions**: Unquoted variables combined with wildcards (e.g., `rm -rf "$TARGET_DIR/*"` vs `rm -rf $TARGET_DIR/*` when `TARGET_DIR` is empty) have wiped entire production filesystems due to unexpected word splitting and expansion.
- **Security Vulnerabilities**: Whitespace injection and command injection often exploit unintentional word splitting and globbing phases in scripts that process untrusted file uploads or user arguments.
- **Operational Efficiency**: Harnessing advanced expansions—such as brace expansion (`mkdir -p app/{src,bin,pkg}`), pattern stripping (`${file%.tar.gz}`), and extended globs (`shopt -s extglob`)—enables concise, performant scripting without launching slow external tools like `sed` or `cut`.

---

## 2. Core Concepts

### The 7 Expansion Phases
The POSIX standard and Bash documentation define a strict, sequential order for shell expansions. Understanding this pipeline explains why quotes disable certain operations while enabling others:

```
+-------------------------------------------------------------------------+
| THE BASH EXPANSION PIPELINE (Order of Operations)                        |
|                                                                         |
|  [ 1. Brace Expansion ]             {a,b,c} -> a b c                    |
|             │                                                           |
|             v                                                           |
|  [ 2. Tilde Expansion ]             ~/docs -> /home/user/docs           |
|             │                                                           |
|             v                                                           |
|  [ 3. Parameter & Variable Exp ]    ${VAR:-default}, ${#VAR}            |
|  [    Arithmetic Expansion ]        $((1024 * 1024))                    |
|  [    Command Substitution ]        $(uname -r)                         |
|  [    Process Substitution ]        <(sort list.txt)                    |
|             │                                                           |
|             v                                                           |
|  [ 4. Word Splitting ]              Splits unquoted text on $IFS        |
|             │                                                           |
|             v                                                           |
|  [ 5. Pathname Expansion (Globbing)] *.log -> access.log error.log      |
|             │                                                           |
|             v                                                           |
|  [ 6. Quote Removal ]               Removes unquoted " and ' delimiters |
|             │                                                           |
|             v                                                           |
|  [ 7. Command Execution ]           Passes clean argv[] array to kernel |
+-------------------------------------------------------------------------+
```

### Quoting Rules: Single vs. Double vs. Backslash
- **Single Quotes (`'...'`)**: Completely preserve the literal value of all enclosed characters. No parameter expansion, no command substitution, no arithmetic, and no escaping. A single quote cannot appear inside single quotes.
- **Double Quotes (`"..."`)**: Preserve the literal value of all enclosed characters *except* `$`, `` ` ``, `\`, and `!` (when history expansion is enabled). Double quotes prevent **Word Splitting** and **Pathname Expansion (Globbing)** while permitting variable and command expansion.
- **Escape Character (`\`)**: Preserves the literal value of the next following character.

### Word Splitting and the Internal Field Separator (`$IFS`)
When an unquoted variable is expanded, Bash searches the result for characters found in `$IFS` (by default: space, horizontal tab, newline). Wherever an IFS character appears, Bash splits the string into distinct words. If the variable is wrapped in double quotes (`"$VAR"`), Word Splitting is completely suppressed.

---

## 3. How It Works Internally

The shell lexer parses a command line into words, then passes each token through the expansion functions inside the Bash source code:

```mermaid
flowchart TD
    A["Raw Command Line: cp ${SRC_DIR}/*.txt ~/backup/"] --> B["1. Brace Expansion: (None)"]
    B --> C["2. Tilde Expansion: ~/backup/ -> /home/alice/backup/"]
    C --> D["3. Parameter Expansion: ${SRC_DIR} -> '/var/my data'"]
    D --> E{"Is variable quoted?"}
    E -- "No: \"${SRC_DIR}\" was NOT quoted" --> F["4. Word Splitting: Splits on space<br/>Result: ['/var/my', 'data/*.txt']"]
    E -- "Yes: \"${SRC_DIR}\" WAS quoted" --> G["4. Word Splitting: SUPPRESSED<br/>Result: ['/var/my data/*.txt']"]
    F --> H["5. Pathname Globbing: Searches disk for matches"]
    G --> H
    H --> I["6. Quote Removal: Strips enclosing quotes"]
    I --> J["7. sys_execve: Launches cp with final argv array"]
```

### Globbing Internals (Pathname Expansion)
Unlike Windows `cmd.exe` where wildcards (`*.*`) are passed directly to application programs to resolve, Linux shells resolve globs **before** the program starts:
1. The shell scans directory dentries using `opendir()` and `readdir()`.
2. It compiles the wildcard pattern into an in-memory matcher.
3. If matches are found, it replaces the pattern with a sorted list of matched filenames.
4. If **no matches** are found, Bash's default behavior is to pass the **literal unexpanded pattern string** (e.g., `*.txt`) directly to the command!
5. This default behavior can be altered via shell options:
   - `shopt -s nullglob`: Replaces non-matching patterns with an empty string.
   - `shopt -s failglob`: Emits an error and halts execution if no matches are found.

---

## 4. Syntax & Usage

### Wildcard Globbing Syntax
```bash
# Asterisk (*): Matches zero or more characters
ls *.log

# Question mark (?): Matches exactly one character
ls image_???.png    # Matches image_001.png, but not image_1.png

# Character classes ([...]): Matches any one enclosed character
ls log_[0-9].txt    # Matches log_1.txt, log_2.txt
ls log_[!0-9].txt   # Negation: matches any non-digit

# Range matching: [a-z], [A-Z], [0-9]
ls config_[a-f].json
```

### Extended Globbing (`shopt -s extglob`)
Bash provides advanced regular-expression-like globs when `extglob` is enabled:
```bash
shopt -s extglob

# ?(pattern-list): Matches zero or one occurrence
rm -f file?(.txt|.log)

# *(pattern-list): Matches zero or more occurrences
ls *([0-9]).tar.gz

# +(pattern-list): Matches one or more occurrences
ls +([a-z]).py

# @(pattern-list): Matches exactly one occurrence
cat @(app|worker|web).conf

# !(pattern-list): Matches anything EXCEPT the pattern (Negative Matching!)
rm -f !(*.zip|*.tar.gz)   # Deletes everything EXCEPT zip and tar.gz files!
```

### Advanced Parameter Expansion Syntax
```bash
# Default Value Substitution (if unset or empty)
DB_PORT="${PORT:-5432}"

# Assign Default Value (mutates variable if unset)
: "${CONFIG_PATH:=/etc/app/config.yaml}"

# Error if Unset or Empty
: "${API_KEY:?Error: API_KEY environment variable must be set!}"

# String Length
echo "${#DATABASE_URL}"

# Substring Extraction: ${VAR:offset:length}
COMMIT_HASH="d41d8cd98f00b204e9800998ecf8427e"
SHORT_HASH="${COMMIT_HASH:0:7}" # d41d8cd

# Suffix Removal (Shortest: %, Longest: %%)
FILE="backup.2026.tar.gz"
echo "${FILE%.tar.gz}"  # backup.2026
echo "${FILE%%.*}"      # backup

# Prefix Removal (Shortest: #, Longest: ##)
URL="https://api.github.com/v1"
echo "${URL#https://}"   # api.github.com/v1
echo "${URL##*/}"        # v1

# Search and Replace (${VAR//pattern/replacement})
HOST="192-168-1-100"
echo "${HOST//-/.}"      # 192.168.1.100
```

---

## 5. Practical Examples

### 1. Basic: Generating Directories and Combinations with Brace Expansion
Rapidly construct complex nested directory layouts or bulk test files without loops:
```bash
# Create microservice project scaffold
mkdir -p services/{auth,orders,payments}/{src,tests,config}

# Generate 10 mock data files
touch /tmp/test_chunk_{01..10}.bin
```

### 2. Real-World: Renaming and Parsing File Extensions in Pure Bash
Avoid calling external binaries (`basename`, `dirname`, `cut`) inside high-volume batch loops:
```bash
for filepath in /var/log/nginx/*.log; do
  [ -e "$filepath" ] || continue
  
  filename="${filepath##*/}"        # Strip leading directory path (basename)
  dirname="${filepath%/*}"          # Strip trailing file name (dirname)
  basename="${filename%.log}"       # Strip extension
  
  echo "Archiving: directory=${dirname}, base=${basename}"
  gzip -c "$filepath" > "${dirname}/${basename}_$(date +%F).log.gz"
done
```

### 3. Edge Case: Hidden Files and Dotglob
By default, the `*` wildcard in Linux does **not** match hidden files (dotfiles starting with `.`):
```bash
# Given a directory containing: normal.txt and .env
ls *
# Output: normal.txt (.env is omitted!)

# Enable dotglob to include hidden files in wildcard expansions
shopt -s dotglob
ls *
# Output: normal.txt  .env

# Clean up
shopt -u dotglob
```

### 4. Failure Case: Unquoted Variables in File Deletion Scripts
The classic "Space in Path" catastrophe:
```bash
TARGET_DIR="/mnt/storage/shared backup"

# DISASTROUS MISTAKE: Unquoted variable
rm -rf $TARGET_DIR/cache
# Word splitting decomposes this into two separate arguments:
# Argument 1: /mnt/storage/shared
# Argument 2: backup/cache
# Result: 'rm' recursively wipes out '/mnt/storage/shared' entirely!

# PRODUCTION SOLUTION: Always double-quote variables
rm -rf "${TARGET_DIR}/cache"
```

### 5. Production-Grade: Safe Directory Cleaner with Failglob and Extglob
A hardened maintenance script that removes obsolete files while preserving critical configurations, utilizing strict glob safety options:
```bash
#!/usr/bin/env bash
set -euo pipefail
shopt -s extglob failglob

CLEAN_DIR="/tmp/app_artifacts"

if [[ ! -d "$CLEAN_DIR" ]]; then
  echo "[ERROR] Target directory does not exist: $CLEAN_DIR" >&2
  exit 1
fi

echo "[INFO] Cleaning temporary artifacts while preserving manifests..."

# Subshell to safely scope shell options
(
  cd "$CLEAN_DIR"
  # Delete all files EXCEPT .json manifests and .lock files
  # If no files exist to match, failglob prevents running rm with literal strings
  if compgen -G "!(*.json|*.lock)" > /dev/null; then
    rm -rf !(*.json|*.lock)
    echo "[SUCCESS] Obsolete artifacts purged."
  else
    echo "[INFO] No purgeable artifacts found."
  fi
)
```

---

## 6. Best Practices

1. **Quote Every Variable Reference**: Treat `"$VAR"` as the mandatory default syntax. Only omit quotes when you explicitly desire word splitting or glob expansion.
2. **Use `${VAR:-default}` for Fallbacks**: Never assume an environment variable is populated. Always provide safe fallbacks in automation.
3. **Guard Against Unexpanded Globs with `nullglob` or `failglob`**: In scripts that iterate over globs (`for f in *.csv; do`), ensure that if no CSV files exist, the loop does not execute with the literal string `"*.csv"`.
4. **Use String Manipulations Over Subprocesses**: Use `${var##*/}` instead of `$(basename "$var")` and `${var%/*}` instead of `$(dirname "$var")`. Builtin parameter expansion runs in RAM in nanoseconds; `basename` forks a process taking milliseconds.
5. **Double-Quote Arrays with `"${ARRAY[@]}"`**: Always expand arrays using `"[@]"` inside quotes to preserve element boundaries and internal whitespaces intact.

---

## 7. Common Mistakes & Troubleshooting

| Mistake / Antipattern | Underlying Cause | Consequence | Production Solution |
| :--- | :--- | :--- | :--- |
| `rm -rf $DIR/*` | Unquoted `$DIR` splits on spaces; if `$DIR` is empty, expands to `/*`. | Recursively deletes root directory (`/`)! | Use `rm -rf "${DIR:?Error}/*"` with verification. |
| Forgetting `nullglob` in loops | If no files match `*.log`, Bash leaves `*.log` as a literal string. | Script attempts to process non-existent file named literally `*.log`. | Enable `shopt -s nullglob` or test `[ -e "$f" ]`. |
| Quotes inside brace expansion: `{"a", "b"}` | Space after comma or quoting braces disables brace expansion parser. | Strings are treated as literal characters. | Omit spaces: `{a,b,c}`. |
| Using single quotes for variable expansion | Single quotes disable all expansion phases. | `echo '$USER'` prints the literal characters `$USER`. | Use double quotes: `echo "$USER"`. |
| `[ $VAR == "val" ]` without quotes | If `$VAR` is empty, condition evaluates to `[ == "val" ]` (syntax error). | Script crashes with "unary operator expected". | Quote: `[ "$VAR" == "val" ]` or use `[[ $VAR == "val" ]]`. |
| Overwriting `$IFS` globally | Mutating `$IFS` without restoring it disrupts all subsequent word splits. | Subsequent commands, loops, and scripts behave unpredictably. | Scope IFS locally: `IFS=',' read -ra ADDR <<< "$DATA"`. |

---

## 8. Performance, Security & Cost Implications

### Performance
- **Subshell Overhead vs Parameter Expansion**: Spawning a subshell to run `cut` or `awk` for string slicing inside a loop of 10,000 lines takes ~15 seconds due to 10,000 `fork()` and `execve()` system calls. Performing the equivalent string slicing using native Bash parameter expansion (`${line#*,}`) completes in **0.08 seconds**—a ~200x performance increase.

### Security
- **Wildcard Injection (Glob Traps)**: If an attacker creates a file named `-rf` or `--checkpoint-action=exec=sh shell.sh` inside a public upload folder, running `chown user:user *` or `tar -cf archive.tar *` expands the filename as a command-line flag!
  - *Remediation*: Terminate options using `--` before globs: `chown user:user -- *`, or prefix globs with `./*`.

### Cost
- In cloud autoscaling groups, inefficient shell scripts running slow external pipelines during VM boot (`cloud-init`) delay node readiness, slowing auto-scale responses during traffic spikes and causing degraded service.

---

## 9. Comparison Table

| Expansion / Quoting Technique | Performs Var Expansion? | Word Splitting? | Globbing (Wildcards)? | Primary Use Case |
| :--- | :---: | :---: | :---: | :--- |
| **Double Quotes (`"..."`)** | Yes (`$VAR`) | **No (Suppressed)** | **No (Suppressed)** | Standard variable passing, file paths |
| **Single Quotes (`'...'`)** | **No** | **No** | **No** | Literal scripts, regex patterns, SQL queries |
| **Unquoted (`$VAR`)** | Yes | **Yes** | **Yes** | Deliberate whitespace-delimited argument list |
| **Brace Expansion (`{a,b}`)** | Yes | N/A (Pre-split) | N/A | Batch path creation, permutation lists |
| **Backticks (`` `cmd` ``)** | Yes | Yes (if unquoted)| Yes (if unquoted) | Obsolete command substitution |
| **Command Sub (`$(cmd)`)** | Yes | Yes (if unquoted)| Yes (if unquoted) | Modern nestable command output capture |

---

## 10. Real-World Interview Questions

### Q1: What is the exact difference between `"$*"` and `"$@"` in Bash?
<details>
<summary>Answer</summary>
When positional parameters are expanded:
- **`"$*"`**: Expands to a **single word** containing all positional parameters joined together by the first character of `$IFS` (typically a space): `"$1 $2 $3"`.
- **`"$@"`**: Expands to **separate words** for each positional parameter: `"$1" "$2" "$3"`. Each argument retains its exact original boundary, whitespace, and quoting intact.
In almost all shell scripting scenarios, `"$@"` is the correct choice to forward arguments to child commands without distorting arguments that contain spaces.
</details>

### Q2: What happens if you execute `ls *.txt` in an empty directory in default Bash? How do you prevent that behavior?
<details>
<summary>Answer</summary>
By default, if a wildcard pattern finds no matching files, Bash does not produce an error; instead, it leaves the pattern unmodified as a literal string. Thus, `ls *.txt` attempts to list a file literally named `*.txt`, causing `ls` to emit `ls: cannot access '*.txt': No such file or directory` with exit code 2.
To prevent this in scripts:
1. `shopt -s nullglob`: Replaces non-matching patterns with nothing (empty word list).
2. `shopt -s failglob`: Causes the shell to fail immediately with an expansion error without executing the command.
</details>

### Q3: What is "Wildcard Injection", and how can an attacker exploit `tar *`?
<details>
<summary>Answer</summary>
Wildcard injection occurs when filenames are structured to look like command-line flags. Because the shell expands `*` alphabetically into arguments passed to the program, a file named `--checkpoint=1` and another named `--checkpoint-action=exec=sh malicious.sh` will be expanded by `tar -cf backup.tar *` as flags passed to `tar`. When `tar` parses its arguments, it interprets those files as options, executing `sh malicious.sh` with the privileges of the user running `tar`.
Protection:
- Always use `--` to signify the end of command options: `tar -cf backup.tar -- *`
- Or prefix the glob with path delimiters: `tar -cf backup.tar ./*`
</details>

### Q4: Explain what `${VAR:+alternate}` does vs `${VAR:-default}`.
<details>
<summary>Answer</summary>
- **`${VAR:-default}` (Use Default)**: If `VAR` is unset or null, return `default`; otherwise, return the value of `VAR`.
- **`${VAR:+alternate}` (Use Alternate)**: If `VAR` is set and not null, return `alternate`; otherwise, return nothing (null).
*Use case for `:+`*: Conditional command-line flags:
```bash
# Append --port flag only if PORT variable is non-empty
docker run ${PORT:+--publish "$PORT:$PORT"} my-image
```
</details>

### Q5: Why is `[[ ... ]]` preferred over `[ ... ]` in Bash when evaluating variables?
<details>
<summary>Answer</summary>
`[` is a POSIX standard command (often linked to `/usr/bin/test` or implemented as a builtin) that undergoes standard shell expansion, word splitting, and globbing. If `$VAR` is empty in `[ $VAR = "foo" ]`, the shell evaluates `[ = "foo" ]`, causing a syntax error.
`[[ ... ]]` is a Bash compound command keyword. The shell parser treats expressions inside `[[ ... ]]` specially:
- Word splitting and globbing are suppressed inside `[[ ... ]]` (so `[[ $VAR == "foo" ]]` works even if `$VAR` is unset or contains spaces).
- It supports pattern matching and regular expressions via `=~`.
- It supports logical operators `&&` and `||` natively instead of `-a` and `-o`.
</details>

### Q6: How does the shell handle command substitution `$(cat file)` vs `< file`?
<details>
<summary>Answer</summary>
In Bash, `$(cat file)` forks a child process to run the `cat` binary, reads its output across a pipe, strips any trailing newlines, and replaces the expression with the resulting text.
However, Bash provides a built-in optimization: `$(< file)`. This syntax reads the file contents directly into shell memory using internal C library read calls without forking a subshell or executing `cat`. It is significantly faster.
</details>

### Q7: Explain the difference between `${FILE#*.}` and `${FILE##*.}`.
<details>
<summary>Answer</summary>
Both perform prefix removal by matching patterns from the beginning of the string:
- **`#` (Shortest match)**: Removes the shortest matching prefix. For `FILE="archive.tar.gz"`, `${FILE#*.}` removes `archive.` leaving `tar.gz`.
- **`##` (Longest match)**: Removes the longest matching prefix. For `FILE="archive.tar.gz"`, `${FILE##*.}` matches everything up to the final period, leaving `gz`.
</details>

### Q8: What does `shopt -s extglob` enable, and how does `!(pattern)` work?
<details>
<summary>Answer</summary>
`shopt -s extglob` enables extended pattern matching operators adapted from the Korn shell. The operator `!(pattern1|pattern2)` is a negative matcher that matches any filename that does **not** match the specified sub-patterns. For example, `rm -f !(*.log)` removes every file in the directory except those ending with `.log`.
</details>

---

## 11. Practice Exercises

### Exercise 1: Multi-Service Directory Scaffold (Easy)
**Task**: Use a single command with nested brace expansion to construct a complete microservice directory tree with subdirectories for `api`, `auth`, and `billing`, each containing `src/controllers`, `src/services`, `tests`, and `docs`.

<details>
<summary>Solution</summary>

```bash
mkdir -p services/{api,auth,billing}/{src/{controllers,services},tests,docs}
```
*Verification*: Run `tree services` or `find services -type d` to confirm the structure.
</details>

### Exercise 2: Pure Bash URL Parsing (Easy)
**Task**: Given a URL variable `URL="https://storage.prod.cloud.com:8443/v2/blobs/archive.tar.gz"`, extract the protocol (`https`), port (`8443`), and file extension (`tar.gz`) using only Bash parameter expansions without calling `awk`, `sed`, or `cut`.

<details>
<summary>Solution</summary>

```bash
#!/usr/bin/env bash
URL="https://storage.prod.cloud.com:8443/v2/blobs/archive.tar.gz"

PROTOCOL="${URL%%://*}"
TEMP="${URL#*://}"       # storage.prod.cloud.com:8443/v2/blobs/archive.tar.gz
PORT_TEMP="${TEMP%%/*}"  # storage.prod.cloud.com:8443
PORT="${PORT_TEMP##*:}"  # 8443
FILENAME="${URL##*/}"    # archive.tar.gz
EXT="${FILENAME#*.}"     # tar.gz

echo "Protocol:  $PROTOCOL"
echo "Port:      $PORT"
echo "Extension: $EXT"
```
</details>

### Exercise 3: Safe Batch File Renaming with Substring Slicing (Medium)
**Task**: Write a shell loop that renames all files matching `IMG_YYYYMMDD_*.jpg` into `YYYYMMDD-*.jpg` in `/tmp/images/`, ignoring cases where no matching images exist.

<details>
<summary>Solution</summary>

```bash
#!/usr/bin/env bash
shopt -s nullglob

for img in /tmp/images/IMG_[0-9][0-9][0-9][0-9][0-9][0-9][0-9][0-9]_*.jpg; do
  dirname="${img%/*}"
  filename="${img##*/}"
  # Strip "IMG_" prefix
  new_name="${filename#IMG_}"
  # Replace first underscore with hyphen
  new_name="${new_name/_/-}"
  
  mv -v "$img" "${dirname}/${new_name}"
done
```
</details>

### Exercise 4: Dynamic Environment Validation Header (Medium)
**Task**: Author a script snippet that verifies three mandatory environment variables (`DB_HOST`, `DB_USER`, `DB_PASS`). If any variable is missing or empty, emit a clear error and terminate immediately with exit code 1.

<details>
<summary>Solution</summary>

```bash
#!/usr/bin/env bash
: "${DB_HOST:?Fatal: DB_HOST environment variable is required and must not be empty.}"
: "${DB_USER:?Fatal: DB_USER environment variable is required and must not be empty.}"
: "${DB_PASS:?Fatal: DB_PASS environment variable is required and must not be empty.}"

echo "[SUCCESS] Database environment configuration validated."
```
</details>

### Exercise 5: Automated In-Memory Config Generator (Hard)
**Task**: Write a script that takes a comma-delimited list of hostnames `HOSTS="web-01,web-02,web-03"` and, without external tools, parses it into an array, builds an Nginx upstream configuration block, and validates it with a dry-run test.

<details>
<summary>Solution</summary>

```bash
#!/usr/bin/env bash
set -euo pipefail

HOSTS="web-01,web-02,web-03"

# Split comma-separated string into array using scoped IFS
IFS=',' read -ra SERVER_ARRAY <<< "$HOSTS"

UPSTREAM_CONF="upstream backend_cluster {\n"
for srv in "${SERVER_ARRAY[@]}"; do
  UPSTREAM_CONF+="    server ${srv}.internal:8080 max_fails=3 fail_timeout=10s;\n"
done
UPSTREAM_CONF+="}"

echo -e "$UPSTREAM_CONF"
```
</details>

### Exercise 6: Selective Purge with Extended Globbing and Verification (Hard)
**Task**: In a directory containing hundreds of files of various types, delete every file *except* `.gitkeep`, files ending in `.sh`, and files with extension `.config`. The script must fail if the directory is missing, must safely handle filenames containing spaces or punctuation, and must log deleted items.

<details>
<summary>Solution</summary>

```bash
#!/usr/bin/env bash
set -euo pipefail
shopt -s extglob nullglob

TARGET_DIR="/tmp/test_purge"
mkdir -p "$TARGET_DIR"

(
  cd "$TARGET_DIR"
  for file in !(*.sh|*.config|.gitkeep); do
    if [[ -f "$file" || -d "$file" ]]; then
      echo "Removing: $file"
      rm -rf -- "$file"
    fi
  done
)
```
</details>

---

## 12. Summary & Quick Reference Cheat Sheet

| Expansion Pattern | Action | Example |
| :--- | :--- | :--- |
| `{A,B,C}` | Permutative brace expansion | `echo {1..3}` -> `1 2 3` |
| `~` / `~user` | Tilde expansion to home path | `~/bin` -> `/home/alice/bin` |
| `$(command)` | Command substitution (captures output) | `TODAY=$(date +%F)` |
| `$((expression))` | Arithmetic evaluation | `COUNT=$((COUNT + 1))` |
| `${VAR:-default}` | Return default if unset/null | `PORT="${PORT:-8080}"` |
| `${VAR:=default}` | Set and return default if unset/null | `: "${TIMEOUT:=30}"` |
| `${VAR:?error}` | Abort with error if unset/null | `: "${SECRET_KEY:?Missing}"` |
| `${#VAR}` | Length of string in characters | `echo "${#TOKEN}"` |
| `${VAR#pattern}` | Strip shortest matching prefix | `${URL#https://}` |
| `${VAR##pattern}` | Strip longest matching prefix | `${PATH##*/}` (basename) |
| `${VAR%pattern}` | Strip shortest matching suffix | `${PATH%/*}` (dirname) |
| `${VAR%%pattern}` | Strip longest matching suffix | `${FILE%%.*}` |
| `${VAR//pat/rep}` | Replace all pattern occurrences | `${IP//./_}` |
| `!(pattern)` (extglob)| Matches anything except pattern | `rm !(*.tar.gz)` |

---

## 13. Further Reading & Official Documentation

- [GNU Bash Manual: Shell Expansions](https://www.gnu.org/software/bash/manual/html_node/Shell-Expansions.html)
- [POSIX.1-2017: Word Expansions Standard](https://pubs.opengroup.org/onlinepubs/9699919799/utilities/V3_chap02.html#tag_18_06)
- [Bash Hackers Wiki: Parameter Expansion](https://wiki.bash-hackers.org/syntax/pe)
- [Greg's Wiki (BashFAQ): Quoting and Word Splitting](https://mywiki.wooledge.org/Quotes)

---

[<- 02-pipes-redirection-and-file-descriptors.md](./02-pipes-redirection-and-file-descriptors.md) | [Back to Module Overview](./README.md) | [04-text-processing.md ->](./04-text-processing.md)
