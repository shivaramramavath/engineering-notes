# Archiving and Compression in Linux

| Metadata | Details |
| :--- | :--- |
| **Topic** | Archiving and Compression (`tar`, `gzip`, `bzip2`, `xz`, `zstd`, `pigz`) |
| **Module** | `03-system` |
| **Track** | `04-devops/linux` |
| **Prerequisites** | `01-fundamentals/03-working-with-files.md`, `01-fundamentals/05-permissions.md`, `02-shell/02-pipes-redirection-and-file-descriptors.md` |
| **Target Audience** | DevOps Engineers, SREs, Systems Architects |
| **Target Distribution** | Ubuntu 24.04 LTS, Rocky Linux / RHEL 9.4 |
| **Key Tools** | `tar`, `gzip`, `gunzip`, `bzip2`, `xz`, `zstd`, `pigz`, `cpio`, `zip` |

---

## Table of Contents
1. [What & Why](#what--why)
2. [Core Concepts](#core-concepts)
3. [Internal Working](#internal-working)
4. [Syntax & CLI Tools](#syntax--cli-tools)
5. [Progressive Practical Examples](#progressive-practical-examples)
6. [Best Practices](#best-practices)
7. [Common Mistakes & Gotchas](#common-mistakes--gotchas)
8. [Performance, Security & Cost Implications](#performance-security--cost-implications)
9. [Comparison Table / Alternatives](#comparison-table--alternatives)
10. [Real-World Troubleshooting & Interview Q&A](#real-world-troubleshooting--interview-qa)
11. [Hands-On Practice Exercises](#hands-on-practice-exercises)
12. [Summary Cheat Sheet & Further Reading](#summary-cheat-sheet--further-reading)

---

## What & Why

In modern software deployment, data backup, and cloud infrastructure pipelines, handling large collections of files efficiently is foundational. 
- **Archiving** bundles multiple files, directory hierarchies, permissions, symbolic links, and metadata into a single contiguous byte stream or container.
- **Compression** applies mathematical algorithms to eliminate redundancy, reducing storage footprints and network transmission times.

A critical design distinction in Unix is the separation of archiving from compression. Unlike formats such as `.zip`, Unix historically separated packaging (`tar`, short for **t**ape **ar**chive) from data reduction (`compress`, `gzip`, `bzip2`, `xz`, `zstd`). This modular architecture allows streaming pipelines where data is packaged, compressed, encrypted, and transmitted across SSH or object storage without touching intermediary disk files.

Modern production workloads demand a deep understanding of trade-offs: `gzip` provides universal compatibility; `xz` achieves maximum compression ratios for release distribution; and modern `zstd` (Zstandard) offers unmatched decompression speeds and flexible compression levels, making it the default standard in modern Linux kernel modules, systemd journal logs, and package managers (e.g., Arch `pacman`, Fedora/RHEL `rpm`, Ubuntu `apt`).

---

## Core Concepts

### Archiving vs Compression
- **Archive File (`.tar`, `.cpio`)**: A concatenation of file contents preceded by standardized headers (USTAR, POSIX pax format). Preserves Unix file modes, ownership (UID/GID), mtimes, hard links, symlinks, extended attributes (xattrs), and SELinux contexts. Does not reduce file size on its own.
- **Compressed File (`.gz`, `.bz2`, `.xz`, `.zst`)**: A single file or stream compressed using an entropy encoder (e.g., DEFLATE, Burrows-Wheeler, LZMA, Zstandard). Compression utilities independently do not store multiple files or directory structures.
- **Compressed Archive (`.tar.gz`, `.tar.xz`, `.tar.zst`)**: A tar archive piped directly through a compression algorithm.

### Modern Compression Algorithms
1. **Gzip (`DEFLATE` / RFC 1951)**:
   - Combination of LZ77 and Huffman coding.
   - Fast, ubiquitous, low memory consumption during compression and decompression.
   - Standard across HTTP encoding and legacy tarballs (`.tar.gz` or `.tgz`).
2. **Bzip2 (`Burrows-Wheeler Transform`)**:
   - High compression ratio for text files; computationally expensive and slow decompression.
   - Largely superseded today by `xz` and `zstd`.
3. **XZ (`LZMA2`)**:
   - Extreme compression ratio using dictionary sizes up to hundreds of megabytes.
   - Excellent for software release distribution where an archive is compressed once and downloaded millions of times. Decompression is moderately slow and memory-intensive.
4. **Zstandard (`zstd` / RFC 8878)**:
   - Developed by Yann Collet (Meta). Combines Finite State Entropy (FSE) with LZ77.
   - Tunable from level 1 (ultra-fast, approaching `lz4`) to level 19+ (approaching `xz`).
   - Exceptionally fast decompression (several gigabytes per second per core), independent of compression level.

### POSIX Pax Format and Extended Attributes
Modern GNU `tar` supports the POSIX.1-2001 (`pax`) format (`--format=posix`). Unlike legacy USTAR, POSIX pax supports:
- Unlimited file sizes (beyond 8 GB).
- High-precision sub-second nanosecond timestamps.
- Extended attributes (`xattr`), POSIX ACLs (`--acls`), and SELinux security contexts (`--selinux`).

---

## Internal Working

```
+---------------------------------------------------------------------------------------------------------+
|                                    Input Files / Directory Hierarchy                                    |
|   file1.txt (rw-r--r--)   |   app.bin (rwxr-xr-x)   |   config.yaml   |   symlink -> /etc/app.conf      |
+---------------------------------------------------------------------------------------------------------+
                                                     |
                                                     v
+---------------------------------------------------------------------------------------------------------+
|                                              GNU tar Engine                                             |
|   - Serializes files, directories, symlinks, and headers into 512-byte blocks (POSIX pax format)        |
|   - Preserves UID, GID, Mode, mtime, nanoseconds, ACLs, and SELinux labels                             |
+---------------------------------------------------------------------------------------------------------+
                                                     |
                                          Contiguous Stream (.tar)
                                                     |
                                                     v
                      +-------------------------------------------------------------+
                      |                 Compression Engine Selection                |
                      +-------------------------------------------------------------+
                                     |                              |
                                     v                              v
                      +-----------------------------+ +-----------------------------+
                      |   Single-Threaded Engine    | |    Multi-Threaded Engine    |
                      |   gzip, bzip2, xz, zstd     | |    pigz, pxz, zstd -T0      |
                      +-----------------------------+ +-----------------------------+
                                     \                              /
                                      \                            /
                                       v                          v
+---------------------------------------------------------------------------------------------------------+
|                                        Output Compressed Tarball                                        |
|                       archive.tar.gz   |   archive.tar.xz   |   archive.tar.zst                         |
+---------------------------------------------------------------------------------------------------------+
```

### The 512-Byte Block Structure of TAR
A tar file is divided into 512-byte records. Every file begins with a 512-byte header containing file name, mode, UID, GID, size in octal, mtime, checksum, typeflag (regular file, hardlink, symlink, directory), linkname, magic string (`ustar\0`), user name, and group name. The file's binary content follows, padded with null bytes to align with the next 512-byte boundary. Two consecutive 512-byte blocks of null bytes mark the end of the archive.

---

## Syntax & CLI Tools

### Standard `tar` Flag Overview
```bash
# Main operation modes (exactly one required)
-c, --create        # Create a new archive
-x, --extract       # Extract files from an archive
-t, --list          # List contents of an archive
-r, --append        # Append files to the end of an uncompressed archive
-u, --update        # Append files that are newer than their archive copy

# Compression filters
-z, --gzip          # Filter through gzip
-j, --bzip2         # Filter through bzip2
-J, --xz            # Filter through xz
--zstd              # Filter through zstd
-a, --auto-compress # Select compression algorithm based on archive suffix

# Critical control options
-f, --file=ARCHIVE  # Use archive file or device (mandatory; '-' denotes stdin/stdout)
-v, --verbose       # Verbosely list files processed
-C, --directory=DIR # Change to DIR before performing operations
-p, --preserve-permissions # Restore permissions as recorded (default for superuser)
--numeric-owner     # Store and restore numeric UIDs/GIDs rather than usernames
--strip-components=N # Strip N leading directory components on extraction
--exclude=PATTERN   # Exclude files matching PATTERN
--acls              # Preserve POSIX Access Control Lists
--selinux           # Preserve SELinux security contexts
--xattrs            # Preserve extended file attributes
```

### Direct Compression Tools
```bash
# Gzip (single-threaded) and Pigz (parallel gzip)
gzip -9 access.log               # Compresses to access.log.gz (removes original)
gzip -dk access.log.gz           # Decompresses keeping (.k) the original compressed file
pigz -p 8 access.log             # Uses 8 CPU cores to compress with DEFLATE

# XZ (extreme compression)
xz -9 -k database.dump           # Compresses to database.dump.xz keeping original
xz -d -T 0 database.dump.xz      # Multi-threaded decompression using all available cores

# Zstandard (modern high-speed)
zstd -3 logfile.log              # Level 3 (default balanced: high speed, good ratio)
zstd -19 -T0 backup.tar          # Level 19 (ultra compression) using all CPU threads (-T0)
zstd -d backup.tar.zst           # Fast decompression
```

---

## Progressive Practical Examples

### Example 1: Creating and Extracting Standard Compressed Tarballs
```bash
# 1. Create a gzipped archive of a source directory
tar -czvf project-v1.0.tar.gz ./myproject

# 2. Inspect archive table of contents without extracting to disk
tar -tzvf project-v1.0.tar.gz

# 3. Extract into a specific destination directory (-C) safely
mkdir -p /opt/deploy/myproject
tar -xzvf project-v1.0.tar.gz -C /opt/deploy/myproject

# 4. Modern auto-compression (-a): tar automatically invokes zstd based on extension
tar -caf backup-data.tar.zst /var/log/app
```

### Example 2: Stripping Leading Directory Components on Extraction
When downloading GitHub releases or vendor tarballs, archives often wrap contents in a root folder (e.g., `package-1.2.3/`). You can flatten the directory directly upon extraction.

```bash
# Download and unpack directly into /opt/node, stripping the top-level directory
curl -sSL https://nodejs.org/dist/v20.12.2/node-v20.12.2-linux-x64.tar.xz \
  | tar -xJ --strip-components=1 -C /opt/node
```

### Example 3: Pipeline Streaming Over SSH (Zero Intermediate Disk Usage)
When migrating large databases or directory trees between servers, avoid writing an intermediate `.tar.gz` file on the source disk. Stream standard output directly through SSH.

```bash
# Stream from local server to remote server
tar -cf - -C /var/lib/postgresql/16/main . \
  | zstd -3 -T0 \
  | ssh user@backup-node.internal "cat > /storage/backups/pg-cluster-$(date +%F).tar.zst"

# Alternatively, extract directly on the remote server in real-time
tar -cf - -C /var/www/html . \
  | ssh user@web-node-02.internal "tar -xvf - -C /var/www/html"
```

### Example 4: Excluding Patterns and Handling Sparse Files
Databases (PostgreSQL, MySQL, SQLite) and virtual machine disk images often contain sparse files (blocks of empty zeros). Without `--sparse`, tar allocates gigabytes of real zeros.

```bash
# Archive virtual disk images preserving sparse allocation and excluding temporary caches
tar -cvS \
  --sparse \
  --exclude="*.tmp" \
  --exclude="*.cache" \
  --exclude="lost+found" \
  -f /mnt/backups/vms.tar \
  /var/lib/libvirt/images
```

### Example 5: High-Performance Multi-Threaded Backup with `zstd` and POSIX Metadata
Production backup script leveraging POSIX ACLs, SELinux contexts, and all available CPU cores.

```bash
#!/usr/bin/env bash
set -euo pipefail

SOURCE_DIR="/var/data/production"
BACKUP_DEST="/mnt/nas/backups"
TIMESTAMP=$(date +%Y%m%d_%H%M%S)
ARCHIVE_NAME="prod_backup_${TIMESTAMP}.tar.zst"

mkdir -p "${BACKUP_DEST}"

echo "==> Starting backup of ${SOURCE_DIR} using zstd parallel compression..."
tar --create \
    --format=posix \
    --acls \
    --xattrs \
    --selinux \
    --sparse \
    --numeric-owner \
    --exclude="*/cache/*" \
    --exclude="*/tmp/*" \
    --file=- \
    -C "${SOURCE_DIR}" . \
  | zstd -T0 -8 -o "${BACKUP_DEST}/${ARCHIVE_NAME}"

echo "==> Verifying archive integrity..."
zstd -t "${BACKUP_DEST}/${ARCHIVE_NAME}"

echo "==> Backup completed successfully: ${BACKUP_DEST}/${ARCHIVE_NAME}"
ls -lh "${BACKUP_DEST}/${ARCHIVE_NAME}"
```

---

## Best Practices

1. **Always Use `--numeric-owner` for System and Container Backups**: By default, tar resolves UIDs/GIDs to usernames during extraction. If extracting on a different system or Docker host where user IDs differ (e.g., user `redis` is UID 1001 instead of 999), permissions become corrupted. `--numeric-owner` guarantees exact integer UID/GID preservation.
2. **Never Create Archives with Absolute Paths**: Using `tar -czf app.tar.gz /var/www/app` can be dangerous if extracted unintentionally because it may overwrite files starting from root (`/`). GNU tar strips leading slashes by default for safety; always use `-C /var/www app` to produce clean relative paths.
3. **Adopt `zstd` as the Default Standard**: For application logs, database backups, and internal container images, prefer `zstd` over `gzip`. `zstd` compresses faster at equivalent ratios and decompresses **5x–10x faster**, drastically reducing recovery time objectives (RTO).
4. **Use Multi-Threaded Tools for Large Datasets**: Replace single-threaded `gzip` with `pigz` and `xz` with `pxz` or `zstd -T0`. In multi-core cloud instances, single-threaded compression bottlenecks on one CPU core while leaving the remaining cores idle.
5. **Always Test Archive Integrity Before Deleting Sources**: Run `tar -tvf <archive>` or `zstd -t <archive>` / `gzip -t <archive>` to verify checksums before removing source data.
6. **Include `--acls` and `--xattrs` for System Infrastructure**: If backing up system directories (`/etc`, `/home`, `/var`) on distributions with SELinux (RHEL/Rocky) or POSIX ACLs, standard `tar -czf` drops these attributes unless explicitly told to preserve them.

---

## Common Mistakes & Gotchas

| Mistake | Why It Fails | Correct Pattern |
| :--- | :--- | :--- |
| Writing `tar -cf backup.tar.gz dir/` without `-z` | Creates an uncompressed archive with a misleading `.gz` extension; confuses downstream tools. | Use `tar -czf backup.tar.gz dir/` or modern auto-compress: `tar -caf backup.tar.gz dir/`. |
| Putting `-f` before other option letters: `tar -czf archive.tar.gz -v` | `-f` consumes the immediately following argument as the filename; here `-v` becomes the file name. | Ensure `-f` is followed directly by the filename: `tar -czvf archive.tar.gz dir/`. |
| Creating archives with absolute paths (`tar -czf out.tar /etc`) | Stripping leading `/` generates warnings; extracting poorly written archives can accidentally overwrite `/etc`. | Change directory first: `tar -czf out.tar -C /etc .` |
| Attempting to append files (`-r`) to a compressed archive (`tar -rf backup.tar.gz newfile`) | Compressed streams (Gzip, XZ) do not support inline record appending; tar fails with an error. | Decompress first, append, then recompress; or maintain uncompressed `.tar` until finalize. |
| Extracting archives without `-C` in the current working directory | Dumps hundreds of files indiscriminately into your present directory ("tar bomb"). | Always extract into an explicit target directory: `tar -xzf archive.tar.gz -C /target/dir`. |
| Running `gzip large_db.sql` without realizing it deletes the source | `gzip`, `bzip2`, and `xz` delete the uncompressed source file by default upon success. | Use `-k` (`--keep`) flag: `gzip -k large_db.sql` or stream: `gzip -c file > file.gz`. |

---

## Performance, Security & Cost Implications

### Throughput vs Compression Ratio
```
Algorithm    Compression Speed    Decompression Speed    Ratio       CPU Utilization
gzip (-6)    ~40 MB/s             ~250 MB/s              Moderate    Low memory, single-core
bzip2 (-9)   ~10 MB/s             ~30 MB/s               High        High CPU, legacy
xz (-6)      ~3 MB/s              ~80 MB/s               Very High   Extremely CPU & RAM intensive
zstd (-3)    ~350 MB/s            ~1200 MB/s             Moderate    Fast, multi-core scalable (-T0)
zstd (-19)   ~5 MB/s              ~1100 MB/s             Very High   Comparable to xz, but 10x faster unpack
```

### Security Considerations
- **Tar Bomb Vulnerability**: Malicious archives can contain thousands of uncontained files or directory traversal paths (`../../etc/shadow`). Modern GNU `tar` automatically refuses member names containing `..` unless overridden. Always inspect with `tar -tf` before extracting untrusted archives.
- **Setuid/Setgid Bits**: Extracting archives as root restores setuid/setgid binaries if they were archived. If extracting untrusted developer tarballs, use `--no-same-owner` and ensure the filesystem is mounted with `nosuid`.

### Cost Implications
- **Network Egress vs Compute Cost**: Compressing backups with `zstd` or `xz` before uploading to AWS S3 or GCP Cloud Storage cuts network egress bandwidth costs by 60%–85% and recurring storage bills proportionally.
- **Disaster Recovery (RTO)**: Using `zstd` allows 500GB database dumps to decompress in minutes rather than hours, directly minimizing outage windows during production recovery.

---

## Comparison Table / Alternatives

| Feature / Format | `.tar.gz` (`gzip`) | `.tar.xz` (`xz`) | `.tar.zst` (`zstd`) | `.zip` |
| :--- | :--- | :--- | :--- | :--- |
| **Primary Use Case** | Legacy compatibility, Web assets | Software releases, OS kernels | Production backups, modern distros | Cross-platform (Windows/macOS/Linux) |
| **Speed (Compression)** | Medium | Very Slow | Ultra Fast to Slow (tunable) | Fast to Medium |
| **Speed (Decompression)** | Fast | Medium | Ultra Fast (GB/s) | Fast |
| **Unix Metadata** | Full (via tar) | Full (via tar) | Full (via tar) | Incomplete (drops symlinks/UIDs often) |
| **Random File Access** | No (must scan whole stream) | No | No (requires seekable tar extension) | Yes (central directory at end of zip) |
| **Multi-Threading** | `pigz` | `pxz` or `xz -T0` | Native (`zstd -T0`) | Distro-dependent |

---

## Real-World Troubleshooting & Interview Q&A

### Q1: What is a "tar bomb" and how do you protect production systems from it?
<details>
<summary>View Solution</summary>

A **tar bomb** is an archive that contains hundreds of loose files and directories in its root rather than encapsulating them inside a dedicated parent folder. Extracting it pollutes the current working directory, potentially overwriting existing configuration files.

**Protection Strategy**:
1. Never extract directly without checking: `tar -tf archive.tar.gz | head -n 20`
2. Always extract into a newly created sandboxed directory using `-C`:
   ```bash
   mkdir -p /tmp/unpack && tar -xzf archive.tar.gz -C /tmp/unpack
   ```
</details>

### Q2: Why does `tar -czf archive.tar.gz /var/log` emit `tar: Removing leading '/' from member names`?
<details>
<summary>View Solution</summary>

This is a built-in security feature of GNU tar. If tar preserved the leading `/`, extracting the archive would forcibly overwrite absolute system paths (e.g., writing straight into `/var/log/` instead of where the user is currently working). Tar automatically transforms `/var/log` into `var/log` (a relative path). 

To prevent this warning cleanly, use `-C`:
```bash
tar -czf archive.tar.gz -C /var log
```
</details>

### Q3: How do you extract a single file from a 50GB `.tar.gz` archive?
<details>
<summary>View Solution</summary>

Specify the exact relative path of the file as an argument following the archive name:
```bash
# 1. Identify the exact path
tar -tzf large_archive.tar.gz | grep "target_config.json"

# 2. Extract that specific file only
tar -xzf large_archive.tar.gz "etc/myapp/target_config.json"
```
*Note*: Because gzip is a solid compression stream, `tar` must still read and decompress the stream sequentially until it finds the requested file.
</details>

### Q4: Why does restoring a tarball on a different server break service ownership (e.g., files owned by wrong users)?
<details>
<summary>View Solution</summary>

**Root Cause**: By default, `tar` records both usernames and numeric UIDs/GIDs. On extraction by `root`, tar attempts to match usernames. If user `nginx` is UID `101` on server A but UID `998` on server B, extracting with standard flags can map files to the wrong UID or fallback to numeric IDs unpredictably.

**Fix**:
When archiving for cross-server transfers, use `--numeric-owner`:
```bash
tar --numeric-owner -czf site_backup.tar.gz /var/www
```
Upon extraction, ensure the target system's service user matches the expected UID, or run `chown -R` after unpacking.
</details>

### Q5: How do you stream an archive directly from Amazon S3 or a remote URL without downloading the tarball to disk first?
<details>
<summary>View Solution</summary>

Pipe standard output from `curl` or `aws s3 cp` directly into `tar` using standard input (`-`):
```bash
# Using curl:
curl -sSL https://example.com/build.tar.gz | tar -xzf - -C /opt/app

# Using AWS CLI:
aws s3 cp s3://my-corp-backups/database.tar.zst - | zstd -d | tar -xf - -C /var/lib/data
```
</details>

### Q6: What is the difference between `pigz` and `gzip`?
<details>
<summary>View Solution</summary>

`gzip` is single-threaded and can only utilize one CPU core. `pigz` (Parallel Implementation of GZip) breaks the input data into 128KB chunks and compresses them concurrently across all available CPU cores. It produces output that is 100% compliant with the standard gzip/DEFLATE specification and can be decompressed with regular `gunzip`.

You can instruct `tar` to use `pigz`:
```bash
tar --use-compress-program=pigz -cf archive.tar.gz directory/
```
</details>

### Q7: How does `tar --sparse` (`-S`) work, and why is it essential for database backups?
<details>
<summary>View Solution</summary>

Sparse files contain unallocated blocks (holes) that read as zeros but consume no physical disk space. Virtual machine images (`.qcow2`, `.raw`) and database tablespaces frequently contain gigabytes of holes.

If archived normally, `tar` reads the holes as literal zero bytes and writes them to the archive, blowing up archive size and runtime. With `--sparse` (`-S`), `tar` detects blocks of consecutive zeros and records them as sparse metadata in the archive, reconstructing them as physical holes when unpacked on a filesystem supporting sparse files (ext4, XFS).
</details>

### Q8: What happens if an archive creation process is interrupted (e.g., out of disk space)?
<details>
<summary>View Solution</summary>

The resulting file is incomplete and lacks the terminal double-512-byte null blocks. When downstream tools attempt extraction, `tar` extracts files up to the point of truncation and then exits with `tar: Unexpected EOF in archive` and an error status code. 

**Best Practice**: In automated scripts, always write archives to a temporary filename (`archive.tar.gz.tmp`) and atomically rename (`mv`) upon successful exit.
</details>

---

## Hands-On Practice Exercises

### Exercise 1: Multi-Format Compression Benchmark
**Task**: Create a directory containing 100MB of repetitive textual data. Compress it using `gzip`, `xz`, and `zstd`. Measure execution time and compare resulting file sizes.
<details>
<summary>View Solution</summary>

```bash
# 1. Setup workspace and dummy data
mkdir -p /tmp/bench && cd /tmp/bench
cat /etc/services /var/log/dpkg.log /var/log/syslog 2>/dev/null > source.txt || true
# Pad to ~100MB
while [ $(stat -c%s source.txt) -lt 100000000 ]; do
    cat source.txt >> source.txt
done

# 2. Benchmark gzip
time gzip -k -6 source.txt

# 3. Benchmark xz
time xz -k -6 source.txt

# 4. Benchmark zstd
time zstd -k -3 source.txt

# 5. Compare sizes
ls -lh source.txt*

# 6. Cleanup
rm -rf /tmp/bench
```
</details>

### Exercise 2: Packaging an Application Directory with Relative Paths and Exclusions
**Task**: Archive `/etc` while excluding all `.conf` files, systemd directories, and subdirectories named `ssl`. Ensure all archive entries are stored as relative paths.
<details>
<summary>View Solution</summary>

```bash
sudo tar -czvf /tmp/etc_filtered.tar.gz \
  --exclude="*.conf" \
  --exclude="systemd" \
  --exclude="ssl" \
  -C /etc .

# Verify contents
tar -tzvf /tmp/etc_filtered.tar.gz | head -n 30
rm -f /tmp/etc_filtered.tar.gz
```
</details>

### Exercise 3: Setting Up a Live Inter-Server SSH Pipeline
**Task**: Write a one-liner command that packages `/var/log` on the local machine and pipes it via SSH to a remote machine, compressing on-the-fly with `pigz` or `zstd` without saving any intermediate archive file locally.
<details>
<summary>View Solution</summary>

```bash
# Pipe directly across SSH
tar -cf - -C /var log \
  | zstd -3 -T0 \
  | ssh remoteuser@remotehost.internal "mkdir -p /storage/logs && cat > /storage/logs/varlog-$(date +%F).tar.zst"
```
</details>

### Exercise 4: Testing Archive Corruption and Recovery
**Task**: Create a small compressed tarball. Intentionally corrupt bytes inside the compressed archive using `dd`. Verify how `tar -tzf` or `gzip -t` catches the corruption.
<details>
<summary>View Solution</summary>

```bash
mkdir -p /tmp/corrupt-test && cd /tmp/corrupt-test
echo "Critical payload" > secret.txt
tar -czf safe.tar.gz secret.txt

# Verify healthy
gzip -t safe.tar.gz && echo "Archive Healthy"

# Corrupt bytes in the middle of the file
dd if=/dev/urandom of=safe.tar.gz bs=1 count=16 seek=30 conv=notrunc

# Test integrity again
gzip -t safe.tar.gz || echo "Corruption successfully detected!"
tar -xzf safe.tar.gz || echo "Extraction safely failed."

# Cleanup
cd / && rm -rf /tmp/corrupt-test
```
</details>

### Exercise 5: Creating an Atomic Safe Backup Script
**Task**: Write a shell script that creates a backup of a target directory. The archive must write to a `.tmp` file first, test its own integrity, and only rename to the final target name if the integrity check succeeds.
<details>
<summary>View Solution</summary>

```bash
#!/usr/bin/env bash
set -euo pipefail

TARGET_DIR="/etc/nginx"
DEST_FILE="/tmp/nginx_backup_$(date +%s).tar.zst"
TEMP_FILE="${DEST_FILE}.tmp"

echo "==> Creating backup stream..."
tar -cf - -C "${TARGET_DIR}" . | zstd -3 -o "${TEMP_FILE}"

echo "==> Verifying archive integrity..."
if zstd -t "${TEMP_FILE}"; then
    mv "${TEMP_FILE}" "${DEST_FILE}"
    echo "==> Backup committed: ${DEST_FILE}"
else
    echo "==> ERROR: Archive verification failed! Cleaning up."
    rm -f "${TEMP_FILE}"
    exit 1
fi
```
</details>

### Exercise 6: Preserving Sparse Files in Virtual Machine Images
**Task**: Create a 1GB sparse test file using `truncate`. Archive it with and without the `--sparse` flag, and inspect the resulting tar file sizes.
<details>
<summary>View Solution</summary>

```bash
mkdir -p /tmp/sparse-test && cd /tmp/sparse-test

# Create a 1GB sparse file (takes 0 disk space)
truncate -s 1G sparse.img
ls -lhs sparse.img

# Archive without --sparse
tar -cf non_sparse.tar sparse.img

# Archive WITH --sparse
tar -Scf sparse.tar sparse.img

# Compare physical file sizes
ls -lh *.tar

# Notice non_sparse.tar is ~1GB while sparse.tar is only a few kilobytes!
cd / && rm -rf /tmp/sparse-test
```
</details>

---

## Summary Cheat Sheet & Further Reading

### Command Quick Reference
| Operation | Command Example |
| :--- | :--- |
| **Create `.tar.gz`** | `tar -czvf archive.tar.gz -C /path/to/source .` |
| **Extract `.tar.gz`** | `tar -xzvf archive.tar.gz -C /target/dir` |
| **Create `.tar.zst`** | `tar --zstd -cvf archive.tar.zst -C /path .` or `tar -caf archive.tar.zst ...` |
| **Extract `.tar.zst`** | `tar --zstd -xvf archive.tar.zst -C /target/dir` |
| **Create `.tar.xz`** | `tar -cJvf archive.tar.xz -C /path .` |
| **List Archive Contents** | `tar -tvf archive.tar.gz` |
| **Extract Single File** | `tar -xvf archive.tar.gz "path/to/file.txt"` |
| **Preserve Permissions & ACLs**| `tar --format=posix --acls --xattrs --selinux --numeric-owner -czf out.tar.gz ...` |
| **Strip Top Directory** | `tar -xvf download.tar.gz --strip-components=1 -C /opt/app` |
| **Direct Stream over SSH** | `tar -cf - -C /dir . \| ssh user@host "tar -xf - -C /remote/dir"` |

### Further Reading
- `man 1 tar` and `man 1 zstd`
- `man 1 pigz` and `man 1 xz`
- [GNU `tar` Manual - Free Software Foundation](https://www.gnu.org/software/tar/manual/)
- [RFC 8878: Zstandard Compression and The 'application/zstd' Type](https://datatracker.ietf.org/doc/html/rfc8878)
- [RFC 1951: DEFLATE Compressed Data Format Specification](https://datatracker.ietf.org/doc/html/rfc1951)
