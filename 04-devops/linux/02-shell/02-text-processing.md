# Text Processing

Logs, configs, command output, and API responses are all text, and Linux ships a set of small tools that each do one thing and chain together with pipes. Learning a handful of them lets you answer questions like "which IPs hit us the most?" or "what changed in this config?" in one line, without writing a program.

Prerequisite: pipes and redirection from [Shell Basics](./01-shell-basics.md).

The model: **read lines, filter them, reshape them, aggregate them.**

```text
file/command → grep (filter) → cut/awk/sed (reshape) → sort → uniq -c (count) → head
```

## grep: filter lines

```bash
grep "error" app.log              # lines containing "error"
grep -i "error" app.log           # case-insensitive
grep -v "healthcheck" app.log     # invert: lines NOT matching
grep -n "timeout" app.log         # show line numbers
grep -c "500" access.log          # count matching lines
grep -r "TODO" src/               # recursive search through a directory
grep -rl "TODO" src/              # only list file names
grep -E "error|fatal" app.log     # extended regex (alternation)
grep -F "a.b" file                # fixed string, no regex (also faster)
grep -o "[0-9]\+\.[0-9]\+\.[0-9]\+\.[0-9]\+" access.log   # print only the match
grep -C 3 "panic" app.log         # 3 lines of context around each match (-A after, -B before)
```

`grep -q` prints nothing and just sets the exit code, which is what you want in `if` statements ([scripting](./03-bash-scripting.md)). Exit codes: `0` match, `1` no match, `2` error.

Quote your pattern so the shell doesn't interpret `*`, `$`, or `|`. Plain `grep` uses basic regex, where `+`, `?`, `|`, `()` need a backslash. Use `-E` to avoid that. `grep -P` (Perl regex) exists on GNU grep but isn't portable.

## cut: pick columns

```bash
cut -d',' -f1,3 data.csv          # fields 1 and 3, comma-delimited
cut -d' ' -f1 access.log          # first space-separated field
cut -c1-10 file                   # first 10 characters
echo "/etc/ssh/sshd_config" | cut -d/ -f2-    # from field 2 to the end
```

`cut` treats each delimiter literally. Runs of spaces count as separate (empty) fields, so for variable whitespace use `awk`.

## sort and uniq: order and count

```bash
sort file.txt                # alphabetical
sort -n numbers.txt          # numeric
sort -nr numbers.txt         # numeric, descending
sort -u file.txt             # sort and drop duplicates
sort -k2,2 -t, data.csv      # by 2nd field, comma-delimited
sort -h sizes.txt            # human sizes (10K, 2M, 1G)

uniq file.txt                # collapse ADJACENT duplicates only
uniq -c                      # prefix each line with its count
uniq -d                      # show only duplicated lines
```

**`uniq` only compares neighbouring lines, so sort first.** The idiom for "count occurrences" is:

```bash
sort | uniq -c | sort -nr | head
```

Sorting follows your locale, which can order mixed case unexpectedly. Use `LC_ALL=C sort` for byte order and speed.

## tr, wc, head, tail

```bash
echo "Hello" | tr 'a-z' 'A-Z'      # translate characters
tr -d '\r' < dos.txt > unix.txt    # remove Windows carriage returns
tr -s ' '                          # squeeze repeated spaces to one
wc -l file                         # count lines (-w words, -c bytes)
head -n 5 / tail -n 5
```

## sed: stream editor

`sed` applies commands to each line. Its most-used feature is substitution.

```bash
sed 's/old/new/' file          # first match per line
sed 's/old/new/g' file         # all matches per line
sed -i 's/old/new/g' file      # edit the file in place
sed -i.bak 's/old/new/g' file  # in place, keep file.bak backup
sed -n '10,20p' file           # print lines 10 to 20 only
sed '/^#/d' file               # delete comment lines
sed '/^$/d' file               # delete blank lines
sed -E 's/([0-9]+)-([0-9]+)/\2-\1/' file   # extended regex with capture groups
```

Tips:

- Use another delimiter when the pattern has slashes: `sed 's|/usr/local|/opt|g'`.
- Test **without** `-i` first and check the output, then add `-i`.
- GNU `sed -i` (Linux) and BSD/macOS `sed -i ''` differ. Scripts for both need care.
- Single-quote the script so the shell doesn't touch `$` or `\`; use double quotes only when you need variable expansion.

## awk: columns and tiny programs

`awk` splits each line into fields (`$1`, `$2`, ..., `$NF` is the last, `$0` the whole line) on whitespace by default, then runs `pattern { action }` for each line.

```bash
awk '{print $1}' access.log                  # first column
awk -F: '{print $1, $3}' /etc/passwd         # custom delimiter: user and UID
awk '$9 == 500' access.log                   # only lines where field 9 is 500
awk '$3 > 100 {print $1, $3}' data.txt       # condition + action
awk '{sum += $5} END {print sum}' data.txt   # total of column 5
awk '{c[$1]++} END {for (k in c) print c[k], k}' access.log | sort -nr   # count by key
awk 'NR > 1' file                            # skip the header (NR = line number)
awk 'BEGIN{FS=","; OFS="\t"} {print $1,$2}' data.csv   # CSV to TSV (simple CSV only)
```

Structure: `BEGIN {…}` runs once before input, `END {…}` once after. `awk` handles variable whitespace and arithmetic that `cut` can't. It does not parse real CSV with quoted commas; use a proper tool for that.

## xargs: turn lines into arguments

Many commands take arguments, not stdin. `xargs` bridges that gap.

```bash
find . -name "*.log" | xargs wc -l
find . -name "*.tmp" -print0 | xargs -0 rm         # safe with spaces/newlines
cat hosts.txt | xargs -I{} ping -c1 {}              # one command per line, {} is the item
find . -name "*.png" -print0 | xargs -0 -P4 -n1 optipng   # 4 in parallel, 1 file each
```

Always pair `find -print0` with `xargs -0` unless you're certain names are plain. `-n` limits arguments per invocation; `-P` sets parallelism. For simple cases `find -exec … {} +` does the same without xargs ([working with files](../01-fundamentals/03-working-with-files.md)).

## jq: JSON on the command line

Regex tools are the wrong choice for JSON. `jq` understands the structure. Install it with `sudo apt install jq`.

```bash
curl -s https://api.example.com/users | jq .                 # pretty-print
jq '.name' user.json                                          # a field
jq -r '.name' user.json                                       # raw string, no quotes
jq '.items[]' data.json                                       # iterate an array
jq '.items[] | {id, name}' data.json                          # reshape
jq '.items[] | select(.status == "failed")' data.json         # filter
jq '[.items[].price] | add' data.json                         # sum
jq '.items | length' data.json                                # count
jq -r '.items[] | [.id, .name] | @csv' data.json              # to CSV
```

Use `-r` whenever the result goes to another command or script. Without it strings keep their JSON quotes. `jq -e` sets a non-zero exit code when the result is `null` or `false`, handy for checks.

## Practical one-liners

```bash
# Top 10 client IPs in an nginx access log
awk '{print $1}' access.log | sort | uniq -c | sort -nr | head

# Count of each HTTP status code (status is field 9 in the default combined format)
awk '{print $9}' access.log | sort | uniq -c | sort -nr

# Errors in the last hour from the journal, summarized by message
journalctl --since "1 hour ago" -p err --no-pager | cut -d' ' -f5- | sort | uniq -c | sort -nr | head

# Biggest directories under /var
du -h --max-depth=1 /var 2>/dev/null | sort -h | tail

# Show non-comment, non-empty config lines
grep -Ev '^\s*(#|$)' /etc/ssh/sshd_config

# Replace a value across many files, safely
grep -rl "old.example.com" conf/ | xargs sed -i 's/old\.example\.com/new.example.com/g'
```

## Common mistakes

- **`uniq` without `sort`**: duplicates that aren't adjacent are missed.
- **Unquoted patterns** that the shell globs or splits.
- **`sed -i` on the first try**, with no backup and no dry run.
- **Using `cut` on space-aligned output** (like `ps` or `df`); use `awk` instead.
- **Parsing `ls` output**; use globs, `find`, or `stat`.
- **Regex on JSON/HTML**, which breaks on formatting changes. Use `jq` or a real parser.
- **Reading and writing the same file in one pipeline** (`sort f > f` empties it). Write to a temp file, or use `sponge`/`sort -o`.
- Forgetting that `grep` exits `1` when nothing matches, which fails scripts running with `set -e`.

## Quick Summary

- `grep` filters lines, `cut`/`awk` pick fields, `sed` edits, `sort | uniq -c | sort -nr` counts.
- `awk` is the right tool once whitespace or arithmetic is involved.
- `xargs` (with `-0`) turns lines into arguments; `jq` handles JSON.
- Test `sed -i` without `-i` first; always sort before `uniq`.
- Build the pipeline step by step, checking the output after each `|`.

**Next:** [Bash Scripting](./03-bash-scripting.md)
