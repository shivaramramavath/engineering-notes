# Project 02: Automated Enterprise Backup Pipeline with GPG & Rotation

| Metadata | Details |
| :--- | :--- |
| **Project** | Automated Enterprise Backup Pipeline with Rotation & Encryption |
| **Module** | `07-projects` — Production Reference Projects & Labs |
| **Track** | `04-devops/linux` |
| **Target OS** | Ubuntu 24.04 LTS, RHEL 9.4 / Rocky Linux 9.4 |
| **Complexity** | Intermediate |
| **Estimated Duration**| 45–60 minutes |

---

## 1. Architecture & Design

### Scenario Context
Production systems require dependable, automated data protection pipelines. Relying on ad-hoc copy commands or manual tarballs leads to silent data corruption, unencrypted sensitive dumps stored on disks, and unmanaged disk space exhaustion.

This project implements an enterprise-grade backup automation pipeline written defensively in Bash. It bundles target application directories and database dumps, compresses using multi-threaded **Zstandard (`zstd`)**, computes cryptographic **SHA-256 integrity checksums**, encrypts the payload using **GPG (AES-256)**, and enforces a **Grandfather-Father-Son (GFS)** retention rotation policy (keeping 7 daily, 4 weekly, and 6 monthly archives).

### Operational & Data Flow Architecture
```
+-------------------------------------------------------------------------+
|                      Backup Pipeline Execution Flow                     |
|                                                                         |
|  [ Source Data ]                                                        |
|  - Application Configs (/etc/myapp)                                     |
|  - Database Data Dumps (/var/backups/db)                                |
|  - User Uploads (/var/www/uploads)                                      |
|                                                                         |
|                                  |                                      |
|                                  v                                      |
|  +-------------------------------------------------------------------+  |
|  | Phase 1: Pre-Flight Safety Checks                                 |  |
|  | - Verify Free Disk Space (> 20% available on backup volume)       |  |
|  | - Check binary dependencies (tar, zstd, gpg, sha256sum)           |  |
|  +-------------------------------------------------------------------+  |
|                                  |                                      |
|                                  v                                      |
|  +-------------------------------------------------------------------+  |
|  | Phase 2: Archive & Zstandard Compression                          |  |
|  | tar -cf - <sources> | zstd -T0 -3 > backup_YYYYMMDD.tar.zst       |  |
|  +-------------------------------------------------------------------+  |
|                                  |                                      |
|                                  v                                      |
|  +-------------------------------------------------------------------+  |
|  | Phase 3: Integrity Checksum & Encryption                          |  |
|  | - Calculate SHA-256 Checksum file                                 |  |
|  | - GPG Symmetric / Asymmetric AES-256 Encryption                   |  |
|  +-------------------------------------------------------------------+  |
|                                  |                                      |
|                                  v                                      |
|  +-------------------------------------------------------------------+  |
|  | Phase 4: Grandfather-Father-Son Rotation Engine                   |  |
|  | - Daily Archives: Retain 7 latest                                 |  |
|  | - Weekly Archives (Sundays): Retain 4 latest                      |  |
|  | - Monthly Archives (1st of month): Retain 6 latest                |  |
|  | - Prune expired artifacts & Log to Syslog / Slack Webhook         |  |
|  +-------------------------------------------------------------------+  |
+-------------------------------------------------------------------------+
```

---

## 2. Implementation Artifacts

### Artifact 1: Production Backup Script (`enterprise-backup.sh`)

```bash
#!/usr/bin/env bash
# ==============================================================================
# Script: enterprise-backup.sh
# Purpose: Enterprise Backup Pipeline with zstd, GPG, and GFS Rotation
# Dependencies: tar, zstd, gpg, sha256sum, mail / curl
# ==============================================================================

set -euo pipefail
IFS=$'\n\t'

# --- Configuration ---
BACKUP_NAME="app-prod"
SOURCE_DIRS=("/etc/nginx" "/var/www/html" "/var/lib/myapp/data")
BACKUP_ROOT="/mnt/backups"
GPG_PASSPHRASE_FILE="/etc/backup-secrets/gpg-passphrase.txt"
MIN_FREE_SPACE_MB=5120 # Require at least 5GB free

TIMESTAMP=$(date +%Y%m%d_%H%M%S)
DATE_DAY=$(date +%d)
DATE_DOW=$(date +%u) # 1=Mon, 7=Sun

DAILY_DIR="${BACKUP_ROOT}/daily"
WEEKLY_DIR="${BACKUP_ROOT}/weekly"
MONTHLY_DIR="${BACKUP_ROOT}/monthly"
LOG_FILE="/var/log/backup-${BACKUP_NAME}.log"

# --- Logging Helpers ---
log() {
    local msg="[$(date -u '+%Y-%m-%dT%H:%M:%SZ')] [INFO] $*"
    echo -e "\033[1;32m${msg}\033[0m"
    echo "${msg}" >> "${LOG_FILE}"
}

error() {
    local msg="[$(date -u '+%Y-%m-%dT%H:%M:%SZ')] [ERROR] $*"
    echo -e "\033[1;31m${msg}\033[0m" >&2
    echo "${msg}" >> "${LOG_FILE}"
    exit 1
}

# --- Pre-Flight Checks ---
preflight_checks() {
    log "Initiating pre-flight verification..."
    
    # 1. Verify Root
    [[ "${EUID}" -eq 0 ]] || error "Backup script must run as root to read all source data."

    # 2. Check Dependencies
    for cmd in tar zstd gpg sha256sum df awk; do
        command -v "${cmd}" >/dev/null 2>&1 || error "Required command '${cmd}' is missing."
    done

    # 3. Verify Passphrase File
    [[ -f "${GPG_PASSPHRASE_FILE}" ]] || error "GPG Passphrase file not found at ${GPG_PASSPHRASE_FILE}"
    [[ "$(stat -c '%a' "${GPG_PASSPHRASE_FILE}")" == "400" || "$(stat -c '%a' "${GPG_PASSPHRASE_FILE}")" == "600" ]] || \
        error "Insecure permissions on ${GPG_PASSPHRASE_FILE}. Must be 0400 or 0600."

    # 4. Check Storage Destination Capacity
    mkdir -p "${DAILY_DIR}" "${WEEKLY_DIR}" "${MONTHLY_DIR}"
    local free_space
    free_space=$(df -m "${BACKUP_ROOT}" | awk 'NR==2 {print $4}')
    if [[ "${free_space}" -lt "${MIN_FREE_SPACE_MB}" ]]; then
        error "Insufficient disk space on ${BACKUP_ROOT}. Available: ${free_space}MB, Required: ${MIN_FREE_SPACE_MB}MB."
    fi
    log "Pre-flight checks passed. Free space on target: ${free_space}MB."
}

# --- Core Backup Routine ---
create_backup() {
    local base_filename="${BACKUP_NAME}_${TIMESTAMP}"
    local raw_tar_zst="${BACKUP_ROOT}/${base_filename}.tar.zst"
    local checksum_file="${BACKUP_ROOT}/${base_filename}.sha256"
    local final_archive="${DAILY_DIR}/${base_filename}.tar.zst.gpg"

    log "Step 1: Creating compressed archive with Zstandard (multithreaded)..."
    # -T0 uses all available CPU cores, -3 is fast balanced compression
    tar -cf - "${SOURCE_DIRS[@]}" 2>/dev/null | zstd -T0 -3 > "${raw_tar_zst}"

    log "Step 2: Generating SHA-256 checksum..."
    sha256sum "${raw_tar_zst}" > "${checksum_file}"
    log "Checksum generated: $(cat "${checksum_file}")"

    log "Step 3: Encrypting archive with GPG (AES-256)..."
    gpg --batch --yes --cipher-algo AES256 --passphrase-file "${GPG_PASSPHRASE_FILE}" \
        --symmetric --output "${final_archive}" "${raw_tar_zst}"

    # Also encrypt and store the checksum
    gpg --batch --yes --cipher-algo AES256 --passphrase-file "${GPG_PASSPHRASE_FILE}" \
        --symmetric --output "${DAILY_DIR}/${base_filename}.sha256.gpg" "${checksum_file}"

    # Clean unencrypted temporary files immediately
    rm -f "${raw_tar_zst}" "${checksum_file}"
    chmod 0600 "${final_archive}"

    log "Backup created successfully: ${final_archive} ($(du -h "${final_archive}" | awk '{print $1}'))"

    # GFS Tiering (Hardlinks save storage space!)
    # Weekly on Sundays (day 7)
    if [[ "${DATE_DOW}" -eq 7 ]]; then
        log "Tagging as Weekly backup..."
        ln "${final_archive}" "${WEEKLY_DIR}/${base_filename}.tar.zst.gpg"
    fi

    # Monthly on the 1st of every month
    if [[ "${DATE_DAY}" -eq "01" ]]; then
        log "Tagging as Monthly backup..."
        ln "${final_archive}" "${MONTHLY_DIR}/${base_filename}.tar.zst.gpg"
    fi
}

# --- Grandfather-Father-Son Retention Rotation ---
rotate_backups() {
    log "Step 4: Executing retention rotation policy..."

    # Retain 7 Daily Backups
    log "Pruning Daily backups (retaining newest 7)..."
    find "${DAILY_DIR}" -type f -name "*.tar.zst.gpg" -printf '%T+ %p\n' | \
        sort -r | tail -n +8 | awk '{print $2}' | while read -r old_file; do
            log "Deleting expired daily: ${old_file}"
            rm -f "${old_file}" "${old_file%.tar.zst.gpg}.sha256.gpg"
        done

    # Retain 4 Weekly Backups
    log "Pruning Weekly backups (retaining newest 4)..."
    find "${WEEKLY_DIR}" -type f -name "*.tar.zst.gpg" -printf '%T+ %p\n' | \
        sort -r | tail -n +5 | awk '{print $2}' | while read -r old_file; do
            log "Deleting expired weekly: ${old_file}"
            rm -f "${old_file}"
        done

    # Retain 6 Monthly Backups
    log "Pruning Monthly backups (retaining newest 6)..."
    find "${MONTHLY_DIR}" -type f -name "*.tar.zst.gpg" -printf '%T+ %p\n' | \
        sort -r | tail -n +7 | awk '{print $2}' | while read -r old_file; do
            log "Deleting expired monthly: ${old_file}"
            rm -f "${old_file}"
        done

    log "Rotation policy execution complete."
}

main() {
    log "======================================================================"
    log "Starting Automated Backup Pipeline: ${BACKUP_NAME}"
    log "======================================================================"
    preflight_checks
    create_backup
    rotate_backups
    log "======================================================================"
    log "Pipeline Finished Successfully."
    log "======================================================================"
}

main "$@"
```

### Artifact 2: Systemd Automation Unit & Timer

`/etc/systemd/system/enterprise-backup.service`:
```ini
[Unit]
Description=Enterprise Production Backup Pipeline
Documentation=file:///usr/local/sbin/enterprise-backup.sh
After=network.target local-fs.target

[Service]
Type=oneshot
ExecStart=/usr/local/sbin/enterprise-backup.sh
StandardOutput=journal
StandardError=journal
Nice=19
IOSchedulingClass=idle
```

`/etc/systemd/system/enterprise-backup.timer`:
```ini
[Unit]
Description=Trigger Enterprise Backup Daily at 02:30 AM UTC

[Timer]
OnCalendar=*-*-* 02:30:00 UTC
Persistent=true
RandomizedDelaySec=600

[Install]
WantedBy=timers.target
```

---

## 3. Execution & Verification

### Step-by-Step Test Procedure

1. **Setup Secrets and Directories**:
   ```bash
   sudo mkdir -p /etc/backup-secrets /mnt/backups
   echo "SuperSecretPassphrase2026!" | sudo tee /etc/backup-secrets/gpg-passphrase.txt > /dev/null
   sudo chmod 600 /etc/backup-secrets/gpg-passphrase.txt
   sudo chmod 700 /mnt/backups
   ```

2. **Trigger Standalone Execution**:
   ```bash
   sudo chmod 700 /usr/local/sbin/enterprise-backup.sh
   sudo /usr/local/sbin/enterprise-backup.sh
   ```

3. **Simulated Verification Output**:
   ```text
   [2026-11-26T02:30:01Z] [INFO] ======================================================================
   [2026-11-26T02:30:01Z] [INFO] Starting Automated Backup Pipeline: app-prod
   [2026-11-26T02:30:01Z] [INFO] ======================================================================
   [2026-11-26T02:30:01Z] [INFO] Initiating pre-flight verification...
   [2026-11-26T02:30:02Z] [INFO] Pre-flight checks passed. Free space on target: 45210MB.
   [2026-11-26T02:30:02Z] [INFO] Step 1: Creating compressed archive with Zstandard (multithreaded)...
   [2026-11-26T02:30:08Z] [INFO] Step 2: Generating SHA-256 checksum...
   [2026-11-26T02:30:09Z] [INFO] Checksum generated: e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855
   [2026-11-26T02:30:09Z] [INFO] Step 3: Encrypting archive with GPG (AES-256)...
   [2026-11-26T02:30:14Z] [INFO] Backup created successfully: /mnt/backups/daily/app-prod_20261126_023001.tar.zst.gpg (142M)
   [2026-11-26T02:30:14Z] [INFO] Step 4: Executing retention rotation policy...
   [2026-11-26T02:30:14Z] [INFO] Pruning Daily backups (retaining newest 7)...
   [2026-11-26T02:30:14Z] [INFO] Pruning Weekly backups (retaining newest 4)...
   [2026-11-26T02:30:14Z] [INFO] Pruning Monthly backups (retaining newest 6)...
   [2026-11-26T02:30:14Z] [INFO] Pipeline Finished Successfully.
   ```

4. **Verify Restoration & Integrity Procedure**:
   ```bash
   # Decrypt test
   gpg --batch --yes --passphrase-file /etc/backup-secrets/gpg-passphrase.txt \
       --decrypt /mnt/backups/daily/app-prod_20261126_023001.tar.zst.gpg > /tmp/test-restore.tar.zst

   # Verify Checksum
   gpg --batch --yes --passphrase-file /etc/backup-secrets/gpg-passphrase.txt \
       --decrypt /mnt/backups/daily/app-prod_20261126_023001.sha256.gpg > /tmp/expected.sha256
   sha256sum /tmp/test-restore.tar.zst

   # Inspect contents without extracting
   zstd -dc /tmp/test-restore.tar.zst | tar -tvf - | head -n 15

   # Cleanup
   rm -f /tmp/test-restore.tar.zst /tmp/expected.sha256
   ```

---

## 4. Failure Modes & Edge Cases

| Failure Mode / Incident | Root Cause | Remediation Runbook |
| :--- | :--- | :--- |
| **Disk Space Exhaustion (`No space left on device`)** | Backup destination volume has less free space than compressed dataset size. | Pre-flight check verifies `MIN_FREE_SPACE_MB`. Script aborts cleanly before writing partial artifacts. |
| **Silent Bit Rot / Data Corruption** | Physical disk sector decay on storage media over months. | The pipeline mandates SHA-256 checksum generation before encryption. Run monthly automated verification jobs verifying decrypted checksums. |
| **GPG Memory Starvation / OOM** | GPG encrypting multi-hundred gigabyte file using standard buffers on small RAM node. | Stream directly using Unix pipes (`tar ... \| zstd ... \| gpg ... > output.gpg`) rather than writing intermediate staging files. |
| **Files Modified During Read (`file changed as we read it`)** | Database or log files being written while `tar` is reading them. | Use database logical dump tools (e.g., `pg_dump`, `mysqldump --single-transaction`) into an atomic staging folder prior to archiving. |

---

## 5. Production Considerations

- **IO Nice Scheduling**: Note the systemd unit settings `Nice=19` and `IOSchedulingClass=idle`. This ensures that heavy backup compression runs strictly during spare CPU and I/O cycles, preventing production web traffic degradation.
- **Off-Site Replication**: Local backups do not protect against total datacenter loss or ransomware compromise. Add an `rclone` or AWS CLI sync phase (`aws s3 sync /mnt/backups/ s3://my-offsite-backup-bucket/ --storage-class GLACIER_IR`).
- **Passphrase Security**: Store GPG passphrases or private keys in HashiCorp Vault or AWS Secrets Manager, pulling the credential dynamically into an in-memory `tmpfs` RAM disk during pipeline execution.

---
*Next Project: [03-log-analyser-with-awk-and-jq.md](file:///04-devops/linux/07-projects/03-log-analyser-with-awk-and-jq.md)*
