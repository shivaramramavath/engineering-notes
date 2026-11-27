# Text Processing: Awk, Sed, Grep & Core Utilities Cheatsheet

| Metadata | Details |
| :--- | :--- |
| **Topic** | Linux Text Stream Processing: `grep`, `sed`, `awk`, `cut`, `sort`, `uniq`, `tr` |
| **Module** | `09-cheatsheets` — High-Density Linux Production Cheatsheets |
| **Track** | `04-devops/linux` |
| **Target OS** | POSIX / GNU Coreutils |
| **Last Updated**| 2026 |

---

## 1. Grep: Fast Pattern Search

```bash
# === Essential Flags ===
grep -i "error" app.log                     # Case-insensitive search
grep -v "DEBUG" app.log                     # INVERT match (exclude DEBUG lines)
grep -E "50[0-4]|40[1-4]" access.log        # Extended Regex (ERE)
grep -P "^\d{1,3}(\.\d{1,3}){3}$" ips.txt   # Perl-Compatible Regex (PCRE)
grep -r "API_KEY" /etc/                     # Recursive directory search
grep -rn "TODO" src/                        # Recursive with line numbers (-n)
grep -l "database" *.yml                    # Print ONLY matching filenames
grep -L "database" *.yml                    # Print filenames that DO NOT match
grep -o "https?://[^ ]+" page.html          # Extract ONLY the matched substring
grep -c "FATAL" system.log                  # Count matching lines
grep -w "port" config.conf                  # Match WHOLE WORD only (not "portal")

# === Context Lines (Before / After) ===
grep -B 3 -A 5 "OutOfMemory" app.log        # 3 lines Before, 5 lines After
grep -C 2 "panic" kernel.log                # 2 lines context around match

# === Binary & Color Control ===
grep --color=always "status" access.log     # Force colored output through pipes
grep -I "needle" *                          # Ignore binary files
```

---

## 2. Sed: Stream Editor & In-Place Replacements

```bash
# Basic Syntax: sed [options] 'address command' file

# === Substitutions (s///) ===
sed 's/foo/bar/' file.txt                   # Replace first occurrence per line
sed 's/foo/bar/g' file.txt                  # Replace ALL occurrences per line (global)
sed 's/foo/bar/2' file.txt                  # Replace only the 2nd occurrence per line
sed 's/foo/bar/gi' file.txt                 # Global + Case-insensitive
sed -i 's/8080/443/g' server.conf           # IN-PLACE file modification (Destructive!)
sed -i.bak 's/old/new/g' file.txt           # In-place with backup (.bak created)

# === Alternative Delimiters (Path Replacement) ===
# Avoid the "leaning toothpick syndrome" (\/usr\/local\/bin):
sed 's#/usr/local/bin#/usr/bin#g' file.txt  # Use '#' as delimiter
sed 's|/var/log|/mnt/logs|g' file.txt       # Use '|' as delimiter

# === Line Addressing & Ranges ===
sed '5d' file.txt                           # Delete line 5
sed '1,10d' file.txt                        # Delete lines 1 through 10
sed '$d' file.txt                           # Delete the last line ($)
sed '/^#/d' file.txt                        # Delete all lines starting with # (comments)
sed '/^$/d' file.txt                        # Delete all blank / empty lines
sed -n '15,25p' file.txt                    # Print ONLY lines 15 through 25 (-n suppresses)
sed -n '/BEGIN/,/END/p' file.txt            # Print range between two regex matches

# === Insertion & Appending ===
sed '1i # Generated File' file.txt          # Insert before line 1
sed '$a # End of File' file.txt             # Append after last line
```

---

## 3. Awk: High-Performance Column & Analytical Processing

```bash
# Basic Syntax: awk 'BEGIN { ... } /pattern/ { action } END { ... }' file

# Built-in Variables:
#   $0: Entire line    $1, $2, ... $NF: Columns/Fields
#   NF: Number of Fields on current line
#   NR: Total Line Number processed across all files
#   FNR: Line Number relative to CURRENT file
#   FS: Field Separator (default space/tab)
#   OFS: Output Field Separator (default space)

# === Column Extraction & Printing ===
awk '{print $1}' access.log                 # Print 1st column (Client IP)
awk '{print $1, $NF}' access.log            # Print 1st and LAST column ($NF)
awk '{print $(NF-1)}' access.log            # Print second to last column
awk -F: '{print $1, $6}' /etc/passwd        # Colon-delimited (Username & Home dir)
awk -v OFS="," '{print $1, $2}' data.txt    # Output as CSV

# === Row Filtering & Arithmetic ===
awk '$9 == 500 {print $1, $7}' access.log   # Filter HTTP 500 errors (IP and Path)
awk '$NF > 1.5 {print $0}' access.log       # Latency > 1.5 seconds
awk 'NR >= 10 && NR <= 20' file.txt         # Print lines 10 to 20
awk 'NF > 0' file.txt                       # Strip empty lines

# === Aggregation & Summary Metrics ===
# Calculate sum and average of column 3:
awk '{sum += $3} END {print "Total:", sum, "Avg:", sum/NR}' metrics.txt

# Count occurrences using Associative Arrays (Group By):
awk '{count[$1]++} END {for (ip in count) print count[ip], ip}' access.log

# Print line numbers alongside content:
awk '{print NR, $0}' file.txt
```

---

## 4. Cut, Tr, Sort, and Uniq Pipelines

```bash
# === cut (Fast Delimiter Slicing) ===
cut -d: -f1 /etc/passwd                     # Extract field 1 by colon
cut -d' ' -f1,7 access.log                  # Extract fields 1 and 7
cut -c1-8 file.txt                          # Extract character range 1 to 8

# === tr (Translate / Transliterate) ===
tr 'a-z' 'A-Z' < input.txt                  # Lowercase to Uppercase
tr -d '\r' < dos.txt > unix.txt             # Strip Windows Carriage Returns (\r)
tr -s ' ' < input.txt                       # Squeeze consecutive spaces into single space
tr -cd '[:alnum:]' < input.txt              # Delete everything EXCEPT alphanumeric

# === sort (High-Performance Sorting) ===
sort names.txt                              # Alphabetical sort
sort -r names.txt                           # Reverse sort
sort -n numbers.txt                         # Numeric sort (2 before 10)
sort -h disk_sizes.txt                      # Human-readable numbers (2K, 15M, 3G)
sort -k2,2 -n data.txt                      # Sort specifically by column 2 numerically
sort -u list.txt                            # Unique sort (faster than sort | uniq)

# === uniq (Deduplication - REQUIRES SORTED INPUT!) ===
sort list.txt | uniq                        # Deduplicate entries
sort list.txt | uniq -c                     # Count occurrences of each line
sort list.txt | uniq -d                     # Show ONLY duplicated lines
sort list.txt | uniq -u                     # Show ONLY unique lines (lines appearing once)
```

---

## 5. Production Pipeline Recipes (Wire-Speed Analytics)

### Recipe 1: Top 10 Requesting IP Addresses from Nginx Access Log
```bash
awk '{print $1}' /var/log/nginx/access.log | sort | uniq -c | sort -rn | head -n 10
```

### Recipe 2: Calculate 95th Percentile Response Time
```bash
awk '{print $NF}' access.log | sort -n | awk '{all[NR] = $1} END {print "P95:", all[int(NR*0.95)]}'
```

### Recipe 3: Total RAM Consumed by a Process Name (e.g., PHP-FPM)
```bash
ps -C php-fpm -o rss= | awk '{sum += $1} END {printf "Total RAM: %.2f MB\n", sum/1024}'
```

### Recipe 4: Find All Duplicate Files in a Directory via Checksums
```bash
find . -type f -exec sha256sum {} + | sort | awk 'seen[$1]++ {print "Duplicate:", $2}'
```

### Recipe 5: Strip Empty Lines and Comments from a Configuration
```bash
grep -E -v '^[[:space:]]*(#|$)' /etc/nginx/nginx.conf
```

---
*Next Cheatsheet: [05-systemd-and-journalctl.md](file:///04-devops/linux/09-cheatsheets/05-systemd-and-journalctl.md)*
