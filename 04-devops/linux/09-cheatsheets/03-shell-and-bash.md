# Shell, Bash & Scripting Syntax: High-Density Cheatsheet

| Metadata | Details |
| :--- | :--- |
| **Topic** | Linux Shell Internals, Bash Scripting Syntax & Parameter Expansion |
| **Module** | `09-cheatsheets` — High-Density Linux Production Cheatsheets |
| **Track** | `04-devops/linux` |
| **Target OS** | Bash 5.0+, POSIX sh compatible |
| **Last Updated**| 2026 |

---

## 1. Defensive Script Boilerplate

```bash
#!/usr/bin/env bash
# ==============================================================================
# Production Defensive Bash Template
# ==============================================================================
set -euo pipefail
IFS=$'\n\t'

# Script Directory & Metadata
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" >/dev/null 2>&1 && pwd)"
SCRIPT_NAME="$(basename "${BASH_SOURCE[0]}")"

# Logging Functions
log()   { echo -e "\033[1;32m[$(date -u '+%Y-%m-%dT%H:%M:%SZ')] [INFO] $*\033[0m"; }
warn()  { echo -e "\033[1;33m[$(date -u '+%Y-%m-%dT%H:%M:%SZ')] [WARN] $*\033[0m" >&2; }
error() { echo -e "\033[1;31m[$(date -u '+%Y-%m-%dT%H:%M:%SZ')] [ERROR] $*\033[0m" >&2; exit 1; }

# Automatic Cleanup on Any Exit
cleanup() {
    local exit_code=$?
    log "Executing teardown routines (Exit code: ${exit_code})..."
    rm -rf "${TMP_DIR:-}" 2>/dev/null || true
    exit "${exit_code}"
}
trap cleanup EXIT INT TERM

TMP_DIR=$(mktemp -d "/tmp/${SCRIPT_NAME}.XXXXXX")
```

---

## 2. Parameter Expansion Fast Reference

Assuming: `FILE="/var/log/nginx/access.2026.log.gz"` and `NAME=""`

| Syntax | Description | Result / Example |
| :--- | :--- | :--- |
| `${VAR:-default}` | If unset/null, return `default` | `${NAME:-anonymous}` -> `anonymous` |
| `${VAR:=default}` | If unset/null, set `VAR` to `default` | `${TIMEOUT:=30}` |
| `${VAR:?error}` | If unset/null, print error and exit | `${DB_PASS:?Password required}` |
| `${VAR:+alt}` | If set and not null, return `alt` | `${DEBUG:+"--verbose"}` |
| `${#VAR}` | String length | `${#FILE}` -> 32 characters |
| `${VAR:offset:len}` | Substring slice | `${FILE:0:8}` -> `/var/log` |
| `${VAR#pattern}` | Strip **shortest** prefix | `${FILE#*/}` -> `var/log/nginx/access.2026.log.gz` |
| `${VAR##pattern}` | Strip **longest** prefix (Basename) | `${FILE##*/}` -> `access.2026.log.gz` |
| `${VAR%pattern}` | Strip **shortest** suffix (Extension) | `${FILE%.*}` -> `/var/log/nginx/access.2026.log` |
| `${VAR%%pattern}` | Strip **longest** suffix | `${FILE%%.*}` -> `/var/log/nginx/access` |
| `${VAR/search/rep}` | Replace first occurrence | `${FILE/nginx/apache}` |
| `${VAR//search/rep}`| Replace **all** occurrences | `${FILE//\//-}` (Replaces all slashes) |
| `${VAR^}` / `${VAR^^}`| Uppercase first / Uppercase ALL | `text="hi"; ${text^^}` -> `HI` |
| `${VAR,}` / `${VAR,,}`| Lowercase first / Lowercase ALL | `TEXT="HI"; ${TEXT,,}` -> `hi` |

---

## 3. Conditional Testing Operators (`[[ ... ]]`)

### File Tests
```bash
[[ -e "$path" ]]  # Path exists (file, directory, socket, etc.)
[[ -f "$path" ]]  # Regular file exists and is not a directory
[[ -d "$path" ]]  # Directory exists
[[ -s "$path" ]]  # File exists and size > 0 bytes (non-empty)
[[ -L "$path" ]]  # Symbolic link exists
[[ -r "$path" ]]  # Readable by current user
[[ -w "$path" ]]  # Writable by current user
[[ -x "$path" ]]  # Executable by current user
[[ f1 -nt f2 ]]   # File f1 is newer than file f2 (mtime)
[[ f1 -ot f2 ]]   # File f1 is older than file f2
```

### String Tests
```bash
[[ -z "$str" ]]   # String is EMPTY (length is zero)
[[ -n "$str" ]]   # String is NOT empty (length > 0)
[[ "$a" == "$b" ]] # String equality
[[ "$a" != "$b" ]] # String inequality
[[ "$a" < "$b" ]]  # Lexicographical sort comparison
[[ "$str" =~ ^[0-9]+$ ]] # Regex match (Do NOT quote the regex pattern!)
```

### Integer Comparisons
```bash
[[ "$n" -eq 10 ]] # Equal to
[[ "$n" -ne 10 ]] # Not equal to
[[ "$n" -lt 10 ]] # Less than
[[ "$n" -le 10 ]] # Less than or equal to
[[ "$n" -gt 10 ]] # Greater than
[[ "$n" -ge 10 ]] # Greater than or equal to

# Alternatively, use Arithmetic Evaluation syntax:
(( n == 10 ))     # (( n < 10 )), (( n >= 10 )), (( n++ ))
```

---

## 4. Loops & Stream Iteration Patterns

### 1. Reading Files Line-by-Line (Safe from Word Splitting)
```bash
# Correct: Preserves whitespace and backslashes
while IFS= read -r line || [[ -n "$line" ]]; do
    echo "Processing: $line"
done < input.txt

# Streaming command output directly without subshell leak:
while IFS= read -r line; do
    echo "Host: $line"
done < <(grep -v "^#" hosts.txt)
```

### 2. Standard and C-Style For Loops
```bash
# Iterating over positional arguments:
for arg in "$@"; do
    echo "Arg: $arg"
done

# C-Style Loop:
for (( i = 0; i < 10; i++ )); do
    printf "Step %02d\n" "$i"
done
```

---

## 5. Arrays & Associative Arrays (Hash Maps)

```bash
# === Indexed Arrays ===
servers=("web01" "web02" "web03")
servers+=("web04")                          # Append element
echo "${servers[0]}"                        # Access first: web01
echo "${servers[-1]}"                       # Access last: web04
echo "${#servers[@]}"                       # Total array length: 4
echo "${!servers[@]}"                       # Indices: 0 1 2 3
echo "${servers[@]}"                        # All elements

# Iterate over values:
for server in "${servers[@]}"; do
    echo "Deploying to ${server}..."
done

# === Associative Arrays (Hash Maps - Bash 4+) ===
declare -A ports
ports[http]=80
ports[https]=443
ports[ssh]=22

echo "HTTPS port: ${ports[https]}"          # 443
echo "All keys: ${!ports[@]}"               # http https ssh

for proto in "${!ports[@]}"; do
    echo "Protocol: ${proto} -> Port: ${ports[${proto}]}"
done
```

---

## 6. Redirection, Here-Docs & File Descriptors

```bash
# === Redirection Matrix ===
cmd > file.log 2>&1                         # Redirect STDOUT and STDERR to file
cmd &> file.log                             # Bash shortcut for above
cmd >> file.log                             # Append STDOUT to file
cmd 2>/dev/null                             # Discard STDERR completely
cmd &>/dev/null                             # Discard both STDOUT and STDERR

# === Custom File Descriptors ===
exec 3< input.txt                           # Open FD 3 for reading
read -u 3 line                              # Read from FD 3
exec 3<&-                                   # Close FD 3

# === Here-Document (Writing Multiline Files) ===
cat << 'EOF' > /etc/myapp/config.yml
# Quoting 'EOF' prevents variable expansion!
database:
  host: localhost
  port: 5432
EOF

# === Here-String ===
grep "prod" <<< "staging dev prod sandbox"
```

---

## 7. Command-Line Argument Parsing (`getopts`)

```bash
#!/usr/bin/env bash
# Standard getopts option parser
PORT=8080
VERBOSE=false

while getopts "p:vh" opt; do
    case "${opt}" in
        p) PORT="${OPTARG}" ;;
        v) VERBOSE=true ;;
        h) echo "Usage: $0 [-p port] [-v]"; exit 0 ;;
        *) echo "Invalid option"; exit 1 ;;
    esac
done
shift $((OPTIND - 1)) # Strip processed flags, leaving positional args in $@

echo "Port: ${PORT}, Verbose: ${VERBOSE}, Remaining args: $*"
```

---

## 8. Shell Execution & Debugging Flags

```bash
bash -n script.sh                           # Syntax check without executing!
bash -x script.sh                           # Print execution traces
bash -u script.sh                           # Fail on unset variables

# Enable inside script dynamically:
set -x                                      # Turn ON execution tracing
# Critical section...
set +x                                      # Turn OFF execution tracing

# Custom trace prefix with timestamps and line numbers:
export PS4='+ $(date "+%H:%M:%S") [${BASH_SOURCE}:${LINENO}] '
```

---
*Next Cheatsheet: [04-text-tools.md](file:///04-devops/linux/09-cheatsheets/04-text-tools.md)*
