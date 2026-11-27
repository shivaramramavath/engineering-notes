# Linux Backups and Restore Strategies

| Metadata | Details |
| :--- | :--- |
| **Topic** | Enterprise Linux Backups, Snapshots, and Disaster Recovery Strategies |
| **Module** | `03-system` |
| **Track** | `04-devops/linux` |
| **Prerequisites** | `03-system/07-storage-and-filesystems.md`, `03-system/08-mounting-fstab-and-disks.md`, `03-system/09-lvm-and-raid.md`, `03-system/14-archiving-and-compression.md` |
| **Target Audience** | SREs, Systems Architects, Infrastructure Engineers, Security Operations |
| **Target Distribution** | Ubuntu 24.04 LTS, Rocky Linux / RHEL 9.4 |
| **Key Tools** | `rsync`, `restic`, `borgbackup`, `rclone`, `lvcreate -s`, `btrfs subvolume snapshot`, `dd` |

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

"An untested backup is not a backup; it is merely an assumption." In production environments, data loss occurs from hardware failures, ransomware, accidental operator commands (`rm -rf`), and corrupt application migrations.

A comprehensive backup strategy balances two critical business metrics:
1. **RPO (Recovery Point Objective)**: The maximum acceptable age of files recovered from backup storage for normal operations to resume (how much data you can afford to lose, e.g., 1 hour, 24 hours).
2. **RTO (Recovery Time Objective)**: The maximum acceptable amount of time that a system can be down after a disaster before service is restored (how long recovery takes).

Modern Linux sysadmins must understand the entire spectrum of recovery tools:
- **Filesystem & Block-level Snapshots** (LVM, Btrfs, ZFS) for point-in-time application consistency.
- **Delta-transfer Utilities** (`rsync`) for fast synchronization.
- **Content-Addressable, Deduplicating, Encrypted Repositories** (`restic`, `borg`) for cloud object storage.
- **Disaster Recovery (DR) Orchestration** with immutable retention rules to defend against insider threats and ransomware.

---

## Core Concepts

### The 3-2-1 Backup Rule
The standard enterprise disaster recovery guideline dictates:
- **3 copies of data**: 1 primary production copy + 2 backup copies.
- **2 different storage media types**: e.g., local NVMe/SSD primary + NAS/SAN or local tape/HDD secondary.
- **1 copy stored off-site**: Cloud object storage (AWS S3, GCP GCS, Azure Blob) or a physically remote datacenter, preferably immutable (Object Lock / WORM).

### Backup Modalities
1. **Full Backup**: A complete copy of every selected file or block regardless of when it changed. High storage footprint and slow transfer, but instantaneous, single-step restoration.
2. **Differential Backup**: Captures only the data that changed since the **last full backup**. Restoration requires the baseline full backup plus exactly one differential archive.
3. **Incremental Backup**: Captures only data that changed since the **previous backup of any kind** (full or incremental). Minimizes bandwidth and daily storage, but recovery requires sequential playback of every incremental slice in order.
4. **Synthetic Full / Content-Addressable Deduplicated Backup**: Modern engines (`restic`, `borg`) break files into variable-length cryptographic chunks. Identical chunks are stored exactly once. Every snapshot appears logically as a full backup, but physically consumes only the delta increment.

### Snapshot Mechanics vs Backups
A snapshot is **not** an off-site backup. A snapshot is a point-in-time metadata freeze of a block device or filesystem (via Copy-On-Write). 
- If the underlying physical disk fails, both the active filesystem and its snapshots are lost.
- Snapshots allow a backup agent to read a completely frozen, crash-consistent or application-consistent state of a live database without shutting down services.

---

## Internal Working

```
+---------------------------------------------------------------------------------------------------------+
|                                    Application Layer (Postgres / MySQL)                                 |
|                                       Issues `CHECKPOINT` / Freeze IO                                   |
+---------------------------------------------------------------------------------------------------------+
                                                     |
                                                     v
+---------------------------------------------------------------------------------------------------------+
|                                      Storage Layer: CoW Snapshot                                        |
|                          (LVM Snapshot, Btrfs Subvolume, or EBS Volume Snapshot)                         |
|   - Point-in-time frozen view created instantaneously (0 downtime)                                     |
+---------------------------------------------------------------------------------------------------------+
                                                     |
                                                     v
+---------------------------------------------------------------------------------------------------------+
|                                  Backup Engine: Content-Addressable Chunking                            |
|                                         (e.g., restic or borg)                                          |
|                                                                                                         |
|   1. Read frozen files -> 2. Rabin Fingerprinting (Chunking) -> 3. SHA-256 Hash -> 4. Encrypt (AES-256) |
+---------------------------------------------------------------------------------------------------------+
                                                     |
                                    +----------------+----------------+
                                    |                                 |
                                    v                                 v
                        +-----------------------+         +-----------------------+
                        |  Local NAS / On-Prem  |         |  Remote Cloud (S3)    |
                        |     (Target Media 1)  |         |  Immutable WORM Lock  |
                        |                       |         |   (Offsite Media 2)   |
                        +-----------------------+         +-----------------------+
```

### The Fast Delta-Transfer Algorithm of `rsync`
When synchronizing files between remote machines:
1. Target machine divides existing file into fixed-size blocks (e.g., 2 KB) and computes two checksums per block: a fast 32-bit rolling checksum (Alder-32) and a strong 128-bit MD5/SHA checksum.
2. Target sends these checksum tables to the sender.
3. Sender computes rolling checksums across all byte offsets of the local source file.
4. When a matching block is found, sender transmits only the unmatched byte sequences along with the block references, reconstructing the destination file remotely with minimal network transmission.

---

## Syntax & CLI Tools

### `rsync` Modern Operations
```bash
# Common flags:
-a, --archive        # Equivalent to -rlptgoD (recursive, symlinks, perms, times, owner, group, devices)
-v, --verbose        # Show detailed transfer progress
-z, --compress       # Compress data during network transmission
-h, --human-readable # Human-readable numbers (MB, GB)
-P                   # Equivalent to --partial --progress (resumes aborted transfers)
--delete             # Delete extraneous files from destination (exact mirror)
--numeric-ids        # Do not map UIDs/GIDs by username
--exclude=PATTERN    # Exclude files matching pattern

# Example local to remote sync:
rsync -avzP --delete --numeric-ids /var/www/ user@backup-node.internal:/storage/backups/www/
```

### `restic` Encrypted Deduplicating Backups
```bash
# Initialize encrypted repository (local or S3)
export RESTIC_PASSWORD="strong-encryption-passphrase"
export RESTIC_REPOSITORY="s3:s3.amazonaws.com/my-corp-backups-bucket"
restic init

# Perform a backup snapshot
restic backup /etc /var/www /var/data

# List snapshots
restic snapshots

# Restore snapshot to target directory
restic restore <SNAPSHOT-ID> --target /restore/recovery-path/

# Prune old snapshots (retention policy)
restic forget --keep-daily 7 --keep-weekly 4 --keep-monthly 12 --prune
```

### LVM Snapshot Commands
```bash
# Create 10GB Copy-On-Write snapshot of an active logical volume
sudo lvcreate -L 10G -s -n lv_db_snap /dev/vg_data/lv_db

# Mount snapshot read-only
sudo mkdir -p /mnt/snapshot
sudo mount -o ro /dev/vg_data/lv_db_snap /mnt/snapshot

# Once backup agent finishes copying:
sudo umount /mnt/snapshot
sudo lvremove -y /dev/vg_data/lv_db_snap
```

---

## Progressive Practical Examples

### Example 1: Synchronizing Configurations with `rsync` and Atomic Safety
```bash
# 1. Sync directory ensuring exact mirror, excluding caches
rsync -avhP \
  --delete \
  --exclude="*.cache" \
  --exclude="tmp/" \
  /opt/myapp/configs/ /var/backups/myapp/configs/

# 2. Perform a dry-run first to preview changes without modifying disk
rsync -avun --delete /opt/myapp/configs/ /var/backups/myapp/configs/
```

### Example 2: Crash-Consistent Database Backup Using LVM Snapshots
Backing up a live MySQL or PostgreSQL directory directly causes corruption if write transactions occur during file reading. Using LVM snapshots provides a zero-downtime, point-in-time consistent view.

```bash
#!/usr/bin/env bash
set -euo pipefail

VG_NAME="vg_data"
LV_DATA="lv_postgres"
SNAP_NAME="lv_postgres_snap"
SNAP_SIZE="20G"
MOUNT_POINT="/mnt/db_snapshot"
BACKUP_TARGET="/mnt/nfs_backups/postgres_$(date +%F)"

echo "==> Flushing database buffers to disk..."
# For PostgreSQL: issue CHECKPOINT
sudo -u postgres psql -c "CHECKPOINT;"

echo "==> Creating LVM snapshot..."
sudo lvcreate -L "${SNAP_SIZE}" -s -n "${SNAP_NAME}" "/dev/${VG_NAME}/${LV_DATA}"

echo "==> Mounting snapshot read-only..."
sudo mkdir -p "${MOUNT_POINT}"
# If XFS, mount with nouuid to prevent duplicate UUID collision
sudo mount -o ro,nouuid "/dev/${VG_NAME}/${SNAP_NAME}" "${MOUNT_POINT}"

echo "==> Executing backup from frozen snapshot..."
mkdir -p "${BACKUP_TARGET}"
tar --zstd -cf "${BACKUP_TARGET}/pg_data.tar.zst" -C "${MOUNT_POINT}" .

echo "==> Cleaning up snapshot..."
sudo umount "${MOUNT_POINT}"
sudo lvremove -y "/dev/${VG_NAME}/${SNAP_NAME}"
echo "==> LVM snapshot backup complete."
```

### Example 3: End-to-End Enterprise Cloud Backup with `restic` and S3
Automated script storing encrypted, deduplicated snapshots directly to AWS S3 or MinIO with retention lifecycle management.

```bash
#!/usr/bin/env bash
set -euo pipefail

# Configuration loaded securely
export AWS_ACCESS_KEY_ID="AKIAEXAMPLEKEY"
export AWS_SECRET_ACCESS_KEY="wJalrXUtnFEMI/K7MDENG/bPxRfiCYEXAMPLEKEY"
export RESTIC_REPOSITORY="s3:s3.us-east-1.amazonaws.com/corp-prod-backups"
export RESTIC_PASSWORD_FILE="/etc/restic/backup.pass"

echo "==> Checking restic repository connectivity..."
restic check --quick

echo "==> Starting snapshot of production applications..."
restic backup \
  --verbose \
  --one-file-system \
  --tag "production" \
  --tag "daily-auto" \
  --exclude-file="/etc/restic/excludes.txt" \
  /etc /var/www /home

echo "==> Enforcing backup retention policy (forget & prune)..."
restic forget \
  --keep-daily 7 \
  --keep-weekly 4 \
  --keep-monthly 6 \
  --prune

echo "==> Backup execution successful: $(date -u --iso-8601=seconds)"
```

### Example 4: Mirroring File Trees to Object Storage with `rclone`
When distributing raw ISOs, logs, or static assets to S3, Google Cloud Storage, or Backblaze B2:

```bash
# Sync local directory to cloud bucket with bandwidth throttling and verification
rclone sync /var/log/archives/ remote-s3:corp-cold-storage/logs/ \
  --bwlimit "10M" \
  --transfers 4 \
  --checksum \
  --fast-list \
  --log-file=/var/log/rclone-sync.log
```

### Example 5: Disaster Recovery (DR) Verification Script
Simulate and test data restoration automatically in a staging sandbox to guarantee restore integrity.

```bash
#!/usr/bin/env bash
set -euo pipefail

TEST_RESTORE_DIR="/tmp/dr-test-$(date +%s)"
export RESTIC_REPOSITORY="/mnt/nfs/restic-repo"
export RESTIC_PASSWORD="dr-verification-test"

echo "==> Provisioning sandbox directory: ${TEST_RESTORE_DIR}"
mkdir -p "${TEST_RESTORE_DIR}"

# Find latest snapshot ID
LATEST_SNAP=$(restic snapshots --json | jq -r '.[-1].id')
echo "==> Testing restoration of latest snapshot: ${LATEST_SNAP}"

restic restore "${LATEST_SNAP}" --target "${TEST_RESTORE_DIR}"

echo "==> Verifying restored file integrity..."
if [ -f "${TEST_RESTORE_DIR}/etc/passwd" ]; then
    echo "SUCCESS: Core system files restored properly."
else
    echo "FAILURE: Expected files missing from restored snapshot!"
    exit 1
fi

# Clean up sandbox
rm -rf "${TEST_RESTORE_DIR}"
echo "==> DR Automated Test Passed."
```

---

## Best Practices

1. **Automate Continuous Restore Verification**: Never trust a backup report indicating "Exit code 0". Schedule automated monthly restore drills in an isolated staging environment where data is restored, application services are started, and database queries are validated.
2. **Implement Immutable Storage (WORM / Object Lock)**: Modern ransomware actively seeks out network backup shares (NFS, SMB) and deletes or encrypts backups before triggering host extortion. Use S3 Object Lock (Compliance Mode) or append-only SSH keys so backups cannot be altered or purged even if root credentials are compromised.
3. **Use `--one-file-system` (`-x`) on Filesystem Scans**: When backing up `/`, always pass `-x` to `rsync`, `tar`, or `restic`. This prevents the backup agent from traversing pseudo-filesystems like `/proc`, `/sys`, `/dev`, `/run`, or recursively descending into mounted remote NFS shares.
4. **Distinguish Application Consistency from Crash Consistency**: Copying raw database files from a live filesystem while transactions are occurring leads to corrupted tables. Always use database-native export tools (`pg_dump`, `mariadb-dump`, `pg_basebackup`) or quiesce the database and take a filesystem/LVM snapshot before copying.
5. **Always Keep Encryption Keys and Passphrases in a Separate Offline Vault**: If backups are encrypted with `restic` or GPG and the primary server burns down, losing the encryption key means permanent data loss. Store keys in an enterprise password manager, physical safe, or secondary cloud account.
6. **Watch the trailing slash in `rsync`**:
   - `rsync -a /src/ /dst/` copies the **contents** of `/src` into `/dst`.
   - `rsync -a /src /dst/` creates a subdirectory `/src` inside `/dst` (`/dst/src/`).

---

## Common Mistakes & Gotchas

| Mistake | Why It Fails | Correct Pattern |
| :--- | :--- | :--- |
| Leaving off the trailing slash in `rsync` source path | Creates a nested directory (`/target/source/`) instead of syncing contents into `/target/`. | Use `rsync -av /src/ /dst/` to sync directory contents. |
| Backing up `/proc`, `/sys`, and `/dev` during root backup | Traverses infinite virtual filesystems, dumps memory devices, and crashes the backup utility. | Pass `--one-file-system` (`-x`) or explicitly exclude pseudo-filesystems. |
| Treating RAID 1/5/6 as a backup | RAID protects against physical drive failure, not file deletion, database corruption, or ransomware. | Maintain separate off-site point-in-time versioned backups. |
| Leaving an LVM snapshot active indefinitely | If the snapshot volume fills to 100% capacity due to ongoing disk writes, the snapshot corrupts and drops. | Allocate sufficient space; delete the snapshot immediately after backup completes. |
| Storing encryption keys alongside the backup target | Compromise of the backup destination exposes all encrypted data instantly. | Store repository passphrases in a segregated secret management system (e.g., Vault). |
| Forgetting to test restoration until a disaster occurs | Missing encryption keys, silent bitrot, or misconfigured paths result in total unrecoverable outages. | Implement automated continuous DR verification drills. |

---

## Performance, Security & Cost Implications

### Performance
- **Network Bandwidth**: `rsync` delta transfers drastically reduce bandwidth over slow WAN connections by transmitting only byte diffs.
- **Deduplication Efficiency**: Tools like `restic` and `borg` eliminate 80%–95% of storage overhead across multiple servers running identical operating systems (deduplicating identical `/usr`, `/lib`, and binary packages).

### Security
- **Threat Model (Ransomware)**: Attackers compromise admin credentials and issue `restic forget --prune` or AWS `s3 rm --recursive`. Protecting repositories with AWS S3 Object Lock in Compliance Mode prevents deletion even by the root account until the retention duration expires.
- **Zero-Knowledge Encryption**: Modern tools encrypt data client-side using AES-256-CTR and Poly1305 or ChaCha20 before data ever leaves server memory, ensuring cloud storage providers cannot inspect data contents.

### Cost
- Storing uncompressed, non-deduplicated daily full backups in AWS S3 Standard costs orders of magnitude more than using content-addressable deduplication combined with S3 Glacier Instant Retrieval lifecycle rules.

---

## Comparison Table / Alternatives

| Solution | Technology Type | Deduplication | Native Encryption | Primary Use Case |
| :--- | :--- | :--- | :--- | :--- |
| **`rsync`** | File/Delta Sync | No (hardlinks only via `--link-dest`) | No (relies on SSH transport) | Fast 1-to-1 directory mirroring between two Linux nodes |
| **`tar + zstd`** | Stream Archive | No | No (requires external `gpg` or `openssl`) | Single-file application archives and fast local packaging |
| **`restic`** | Content-Addressable Repo | Yes (Variable chunking) | Yes (Mandatory AES-256) | Direct-to-cloud backups (S3, B2, Azure, SFTP) |
| **`borgbackup`** | Content-Addressable Repo | Yes (Variable chunking) | Yes (Mandatory AES-256) | High-speed on-prem and server-to-server backups with SSH |
| **LVM Snapshots** | Block-level CoW | No | No | Instantaneous zero-downtime database freeze for crash consistency |
| **Btrfs / ZFS Snapshots**| Filesystem-level CoW | Yes (block sharing) | Optional | Ultra-fast local snapshots and incremental `send/receive` replication |

---

## Real-World Troubleshooting & Interview Q&A

### Q1: Why is RAID (Redundant Array of Independent Disks) not a backup?
<details>
<summary>View Solution</summary>

RAID provides hardware high availability and fault tolerance against physical drive failure. However:
1. If an operator accidentally executes `rm -rf /var/lib/data` or runs an incorrect SQL `DROP TABLE`, RAID writes that deletion across all mirrored disks instantaneously.
2. If ransomware encrypts files, RAID writes the encrypted blocks immediately.
3. If an electrical surge or controller failure fries the machine, all disks in the array can be destroyed simultaneously.
A backup requires **point-in-time versioning** and **isolated, out-of-band physical or logical storage**.
</details>

### Q2: How does `rsync --link-dest` enable synthetic incremental backups with minimal disk usage?
<details>
<summary>View Solution</summary>

`--link-dest=DIR` instructs `rsync` to compare incoming files against an existing baseline directory. If a file in the source matches the file in `--link-dest` in size, mtime, and permissions:
- `rsync` creates a **hard link** in the new destination directory pointing to the existing inode in `--link-dest`, consuming 0 additional disk data blocks.
- If the file has changed, `rsync` writes a brand-new file.
This allows every daily backup directory to look and feel like an independent, complete full backup while physically consuming only the disk space of newly modified files.
</details>

### Q3: What happens if an LVM snapshot runs out of allocated space (`100% full`) during a long backup?
<details>
<summary>View Solution</summary>

An LVM snapshot volume stores Copy-on-Write (CoW) blocks whenever original blocks on the parent logical volume are modified. If the rate of writes to the primary volume exceeds the allocated snapshot size:
- The snapshot logical volume reaches 100% capacity.
- The Linux kernel drops the snapshot, marks it as invalid (`INACTIVE`), and returns I/O errors (`EIO`) to any process reading it.
- The backup terminates with an unexpected read error.

**Prevention**: Monitor snapshot utilization using `lvs` or configure `thin_pool_autoextend_threshold` in `/etc/lvm/lvm.conf`.
</details>

### Q4: Explain the difference between crash-consistent and application-consistent backups.
<details>
<summary>View Solution</summary>

- **Crash-Consistent**: Captures data as if the server suddenly lost power. Uncommitted writes in memory or disk cache are lost, and files open for writing may have half-written blocks. Databases recovered from crash-consistent snapshots must replay write-ahead logs (WAL) to reach a valid state.
- **Application-Consistent**: The application is informed of the pending backup. It flushes all in-memory dirty pages to disk, momentarily pauses new writes (quiescing), creates the snapshot, and then unfreezes I/O. Databases restored from application-consistent backups boot immediately without data truncation.
</details>

### Q5: How do you verify the integrity of an encrypted `restic` repository against silent bitrot?
<details>
<summary>View Solution</summary>

Run `restic check`:
```bash
# Quick check: verifies snapshot metadata and structural index trees
restic check

# Deep check: reads and validates cryptographic SHA-256 checksums of 100% of stored data packs
restic check --read-data

# Partial deep check (e.g. read 10% of repository randomly every week):
restic check --read-data-subset=10%
```
</details>

### Q6: Why should production backups always use `--numeric-ids` during `rsync`?
<details>
<summary>View Solution</summary>

By default, `rsync` maps user and group ownership between systems using local user names. If the user `redis` has UID 1001 on the source server, but user `nginx` has UID 1001 on the backup storage server, `rsync` attempting to map by name might map to the wrong numeric UID or fail if user `redis` does not exist on the backup server. `--numeric-ids` forces rsync to store and replicate raw numeric integer UIDs/GIDs without translation.
</details>

### Q7: What is an S3 Object Lock Compliance Mode, and why is it critical against ransomware?
<details>
<summary>View Solution</summary>

AWS S3 Object Lock operates on the WORM (Write Once, Read Many) model. In **Compliance Mode**:
- A protected object version cannot be overwritten or deleted by **any user**, including the AWS root account.
- Its retention period cannot be shortened.
- Even if a ransomware attacker compromises the Linux host, steals root API keys, and attempts to run `aws s3 rm --recursive`, the S3 API rejects the deletion request with `AccessDenied`.
</details>

### Q8: How can you backup MySQL without locking tables or interrupting active transactions?
<details>
<summary>View Solution</summary>

For InnoDB storage engines, use `mariadb-dump` or `mysqldump` with the `--single-transaction` flag:
```bash
mysqldump --single-transaction --quick --routines --triggers --all-databases > backup.sql
```
`--single-transaction` sets the transaction isolation level to `REPEATABLE READ` and begins a global transaction, reading a consistent snapshot of InnoDB without blocking concurrent `INSERT`, `UPDATE`, or `DELETE` operations. Alternatively, use Percona XtraBackup for physical non-blocking copies.
</details>

---

## Hands-On Practice Exercises

### Exercise 1: Building an Incremental Hardlink Backup System with `rsync`
**Task**: Create a script that performs daily backups of `/etc` using `rsync --link-dest`. Verify that unchanged files share the exact same inode across backup directories.
<details>
<summary>View Solution</summary>

```bash
#!/usr/bin/env bash
set -euo pipefail

BASE_DIR="/tmp/rsync-lab"
SOURCE="/tmp/rsync-lab/data"
BACKUPS="/tmp/rsync-lab/backups"

mkdir -p "${SOURCE}" "${BACKUPS}"

# 1. Populate source
echo "Version 1" > "${SOURCE}/file1.txt"
echo "Static file" > "${SOURCE}/static.txt"

# 2. First backup (Day 1)
rsync -a "${SOURCE}/" "${BACKUPS}/day1/"

# 3. Modify source
echo "Version 2" > "${SOURCE}/file1.txt"

# 4. Second backup with hardlinks to Day 1
rsync -a --link-dest="${BACKUPS}/day1" "${SOURCE}/" "${BACKUPS}/day2/"

# 5. Inspect inodes: static.txt should have identical inode number!
ls -l -i "${BACKUPS}/day1/static.txt" "${BACKUPS}/day2/static.txt"

# Notice file1.txt has different inodes:
ls -l -i "${BACKUPS}/day1/file1.txt" "${BACKUPS}/day2/file1.txt"

# Clean up
rm -rf "${BASE_DIR}"
```
</details>

### Exercise 2: Initializing and Restoring a Local `restic` Repository
**Task**: Initialize a local restic repository. Create two snapshots with file modifications in between. Restore an earlier version of a file to an alternate directory.
<details>
<summary>View Solution</summary>

```bash
# 1. Setup paths
export RESTIC_REPOSITORY="/tmp/restic-lab/repo"
export RESTIC_PASSWORD="demopassword123"
SRC="/tmp/restic-lab/source"
RESTORE_TARGET="/tmp/restic-lab/restore"

mkdir -p "${SRC}"

# 2. Initialize
restic init

# 3. Create initial data and snapshot 1
echo "Release v1.0" > "${SRC}/version.txt"
restic backup "${SRC}"

# 4. Modify data and snapshot 2
echo "Release v2.0 - Broken" > "${SRC}/version.txt"
restic backup "${SRC}"

# 5. Review snapshots
restic snapshots

# 6. Restore the FIRST snapshot to restore directory
FIRST_SNAP=$(restic snapshots --json | jq -r '.[0].id')
restic restore "${FIRST_SNAP}" --target "${RESTORE_TARGET}"

# 7. Verify restored file
cat "${RESTORE_TARGET}${SRC}/version.txt"
# Should display "Release v1.0"

# Clean up
rm -rf /tmp/restic-lab
```
</details>

### Exercise 3: Simulating a Dry-Run Directory Mirror
**Task**: Test the impact of `rsync --delete` before applying it. Ensure no files are destroyed during verification.
<details>
<summary>View Solution</summary>

```bash
mkdir -p /tmp/mirror-test/src /tmp/mirror-test/dst
touch /tmp/mirror-test/src/keep.txt
touch /tmp/mirror-test/dst/keep.txt /tmp/mirror-test/dst/obsolete.txt

# Execute dry-run with verbose preview
rsync -avhn --delete /tmp/mirror-test/src/ /tmp/mirror-test/dst/

# Observe that "deleting obsolete.txt" is logged in preview
# Apply changes
rsync -avh --delete /tmp/mirror-test/src/ /tmp/mirror-test/dst/

# Verify obsolete.txt is gone
ls -la /tmp/mirror-test/dst/
rm -rf /tmp/mirror-test
```
</details>

### Exercise 4: Testing Network Throttling with `rsync`
**Task**: Transfer a 50MB file while capping network consumption to 5MB/s using `--bwlimit`.
<details>
<summary>View Solution</summary>

```bash
mkdir -p /tmp/bw-test && cd /tmp/bw-test
dd if=/dev/urandom of=large_payload.bin bs=1M count=30

# Sync with 5MB/s bandwidth cap (5000 KB/s)
rsync -avhP --bwlimit=5000 large_payload.bin copy.bin

rm -rf /tmp/bw-test
```
</details>

### Exercise 5: Creating an Automated Disaster Recovery Drill Script
**Task**: Write an automated test script that pulls a sample backup archive, unpacks it into an isolated sandbox, verifies that a specific sentinel file exists, and logs the outcome.
<details>
<summary>View Solution</summary>

```bash
#!/usr/bin/env bash
set -euo pipefail

SANDBOX="/tmp/dr-lab-$(date +%s)"
ARCHIVE_SOURCE="/tmp/sample_backup.tar.zst"

# Create mock archive
mkdir -p /tmp/mock-prod
echo "DATABASE_HEALTHY=TRUE" > /tmp/mock-prod/healthcheck.cfg
tar --zstd -cf "${ARCHIVE_SOURCE}" -C /tmp/mock-prod .
rm -rf /tmp/mock-prod

echo "==> Running automated DR verification drill..."
mkdir -p "${SANDBOX}"
tar --zstd -xf "${ARCHIVE_SOURCE}" -C "${SANDBOX}"

if grep -q "DATABASE_HEALTHY=TRUE" "${SANDBOX}/healthcheck.cfg"; then
    echo "SUCCESS: DR drill verified sentinel file integrity."
else
    echo "ERROR: Data verification failed."
    rm -rf "${SANDBOX}" "${ARCHIVE_SOURCE}"
    exit 1
fi

rm -rf "${SANDBOX}" "${ARCHIVE_SOURCE}"
```
</details>

### Exercise 6: Configuring Append-Only Secure Remote Storage
**Task**: Explain and configure SSH `authorized_keys` with forced command restrictions so a remote server can only execute `rsync` or `restic` in append-only mode without full shell access.
<details>
<summary>View Solution</summary>

```bash
# In the storage server's ~/.ssh/authorized_keys file:
# Prepend the forced command and restrict port-forwarding, pty allocation, and agents:

# Example for restic REST server or rsync restricted shell (rrsync):
command="/usr/bin/rrsync -ro /storage/backups/",no-port-forwarding,no-X11-forwarding,no-pty ssh-ed25519 AAAAC3NzaC1lZDI1NTE5AAAAI... backup-client-key

# Any SSH connection using this key is restricted to read-only rsync inside /storage/backups/
```
</details>

---

## Summary Cheat Sheet & Further Reading

### Command Reference
| Task | Command |
| :--- | :--- |
| **Mirror Directory** | `rsync -avhP --delete /src/ /dst/` |
| **Bandwidth-Capped Sync** | `rsync -avhP --bwlimit=10000 /src/ user@host:/dst/` |
| **Hardlink Incremental Backup** | `rsync -a --link-dest=/prev/snap /src/ /new/snap/` |
| **Init Restic Repo** | `restic init -r s3:s3.amazonaws.com/bucket-name` |
| **Perform Encrypted Backup** | `restic -r <repo> backup --one-file-system /var/data` |
| **List Snapshots** | `restic -r <repo> snapshots` |
| **Restore Snapshot** | `restic -r <repo> restore <ID> --target /restore/dir` |
| **Prune Old Snapshots** | `restic -r <repo> forget --keep-daily 7 --keep-weekly 4 --prune` |
| **Create LVM Snapshot** | `sudo lvcreate -L 10G -s -n snap_name /dev/vg/lv` |
| **Mount XFS Snapshot** | `sudo mount -o ro,nouuid /dev/vg/snap_name /mnt/snap` |

### Further Reading
- `man 1 rsync` and `man 8 lvcreate`
- [Restic Official Documentation](https://restic.readthedocs.io/en/stable/)
- [BorgBackup Documentation](https://borgbackup.readthedocs.io/en/stable/)
- [Rclone Cloud Sync Documentation](https://rclone.org/)
- [NIST SP 800-209: Security Guidelines for Storage Infrastructure](https://csrc.nist.gov/publications/detail/sp/800-209/final)
