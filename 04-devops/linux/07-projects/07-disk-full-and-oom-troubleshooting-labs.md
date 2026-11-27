# Project 07: Disk Exhaustion & Out-Of-Memory Recovery Labs

| Metadata | Details |
| :--- | :--- |
| **Project** | Production SRE Incident Labs: Disk Exhaustion & OOM Killer Recovery |
| **Module** | `07-projects` — Production Reference Projects & Labs |
| **Track** | `04-devops/linux` |
| **Target OS** | Ubuntu 24.04 LTS, RHEL 9.4 / Rocky Linux 9.4 |
| **Complexity** | Advanced |
| **Estimated Duration**| 60–90 minutes |

---

## 1. Architecture & Design

### Scenario Context
Two of the most frequent, high-severity outages encountered by Site Reliability Engineers (SREs) in cloud production environments are:
1. **The Phantom Disk Exhaustion Incident**: `df -h` reports `100% full` (`No space left on device`), yet running `du -sh /*` accounts for only a fraction of the disk! Alternatively, the disk has 50GB of free space, yet creating a 1-byte file fails with `No space left on device` (Inode Exhaustion).
2. **The Runaway OOM Killer Incident**: The Linux kernel Out-Of-Memory (OOM) killer suddenly executes in the background, terminating a critical database (PostgreSQL, Redis) or application daemon without warning, while CPU was completely normal.

This project delivers two hands-on, simulated incident labs with diagnostic runbooks, forensic command pipelines, and automated remediation scripts.

### Incident Diagnostic Flows
```
+-------------------------------------------------------------------------+
|                    Incident 1: Disk Space Discrepancy                   |
|                                                                         |
|  [ Symptom: "No space left on device" ]                                 |
|                         |                                               |
|       +-----------------+-----------------+                             |
|       |                                   |                             |
|       v Check df -h                       v Check df -i                 |
|  [ Capacity 100% Full ]              [ Inodes 100% Full ]               |
|       |                                   |                             |
|       v Run du -sh /*                     v Find Inode Hog Directories  |
|  [ du shows space is missing! ]      [ Millions of tiny 0-byte files    |
|       |                                in /var/spool or /tmp/sessions ] |
|       v lsof +L1 / lsof | grep deleted                                  |
|  [ Unlinked Open File Descriptors:                                      |
|    Running daemon holds deleted 40GB log file in memory ]               |
|       |                                                                 |
|       v Truncate via /proc/<pid>/fd/<fd>                                |
+-------------------------------------------------------------------------+

+-------------------------------------------------------------------------+
|                    Incident 2: Out-Of-Memory (OOM) Panic                |
|                                                                         |
|  [ Symptom: Critical Service Terminated Abnormally ]                   |
|                         |                                               |
|       v Check dmesg -T | grep -i oom                                    |
|  [ Kernel OOM-Killer Invocation Found:                                  |
|    "Out of memory: Killed process 41205 (postgres)" ]                   |
|                         |                                               |
|       v Analyze Memory Budget & Cgroups                                 |
|  - Anonymous Memory (Heaps) vs. Page Cache vs. Slab Metadata            |
|  - Check oom_score and oom_score_adj                                    |
|                         |                                               |
|       v Remediation Strategy:                                           |
|  - Immunize database: echo -1000 > /proc/<pid>/oom_score_adj            |
|  - Enforce Cgroup limits: MemoryMax=2G MemoryHigh=1.8G                  |
|  - Add emergency zram / swap space                                      |
+-------------------------------------------------------------------------+
```

---

## 2. Implementation Artifacts

### Artifact 1: Disk Exhaustion Incident Simulation & Remediation Lab

A script that simulates an unlinked open file leak and demonstrates live zero-downtime recovery without restarting the application.

```bash
#!/usr/bin/env bash
# ==============================================================================
# Script: lab-disk-exhaustion.sh
# Purpose: Reproduce & Recover from Deleted Open File Descriptor Leaks
# ==============================================================================

set -euo pipefail

LAB_DIR="/tmp/disk-lab"
mkdir -p "${LAB_DIR}"

log() { echo -e "\033[1;32m[+] $*\033[0m"; }
warn() { echo -e "\033[1;33m[!] $*\033[0m"; }

# 1. Simulate a long-running daemon holding an open file
log "Step 1: Spawning background daemon creating a 500MB log file..."
python3 -c '
import time, os

filepath = "/tmp/disk-lab/runaway.log"
with open(filepath, "wb") as f:
    f.write(b"\0" * (500 * 1024 * 1024)) # 500MB
    f.flush()
    print("Daemon: 500MB written. Simulating file deletion by user...")
    os.unlink(filepath) # File is deleted from filesystem, but FD is open!
    print("Daemon: File unlinked. Holding file descriptor open for 120s...")
    time.sleep(120)
' &

DAEMON_PID=$!
sleep 2

# 2. Observe the discrepancy between du and df
log "Step 2: Inspecting disk space..."
echo "--- Checking via du (directory usage) ---"
du -sh "${LAB_DIR}"
echo "--- Notice: 'du' reports 0 bytes because the file is deleted! ---"

# 3. Locate the phantom file using lsof
log "Step 3: Finding deleted files held open by running processes..."
sudo lsof +L1 | grep "${DAEMON_PID}" || true
# Alternatively: sudo lsof | grep "(deleted)"

# 4. Zero-Downtime Remediation: Truncate via /proc
log "Step 4: Executing remediation..."
FD_NUM=$(sudo ls -l /proc/${DAEMON_PID}/fd | grep runaway.log | awk '{print $9}')

if [[ -n "${FD_NUM}" ]]; then
    warn "Truncating file descriptor ${FD_NUM} of PID ${DAEMON_PID} directly in /proc..."
    # Writing empty string to the /proc symlink frees the disk space immediately!
    sudo truncate -s 0 "/proc/${DAEMON_PID}/fd/${FD_NUM}"
    log "Truncation successful! Disk space reclaimed without killing PID ${DAEMON_PID}."
fi

# Cleanup
kill "${DAEMON_PID}" 2>/dev/null || true
rm -rf "${LAB_DIR}"
```

### Artifact 2: Inode Exhaustion Scanner & Emergency Cleanup

```bash
#!/usr/bin/env bash
# ==============================================================================
# Script: find-inode-hogs.sh
# Purpose: Rapidly locate directories with excessive small files (Inode Hogs)
# ==============================================================================

set -euo pipefail

TARGET_DIR="${1:-/var}"

echo "=================================================="
echo "    SCANNING FOR INODE HOGS UNDER ${TARGET_DIR}"
echo "=================================================="

# Check filesystem inode percentage
df -ih "${TARGET_DIR}"

echo -e "\nFinding top 10 directories with highest file counts (Inodes)..."
# Fast discovery using find and awk
sudo find "${TARGET_DIR}" -xdev -printf '%h\n' 2>/dev/null | \
    awk '{count[$1]++} END {for (dir in count) print count[dir], dir}' | \
    sort -rn | head -n 10 | \
    awk 'printf "%-10s %s\n", $1, $2'

echo "=================================================="
echo "Common culprits:"
echo "  1. /var/spool/postfix/maildrop (Stuck local cron emails)"
echo "  2. /tmp or /var/lib/php/sessions (Uncollected session files)"
echo "  3. /var/cache (Unpurged build or package caches)"
```

### Artifact 3: OOM Killer Forensic Extractor & Protection Wrapper

```bash
#!/usr/bin/env bash
# ==============================================================================
# Script: oom-forensics.sh
# Purpose: Extract recent OOM events and immunize critical databases
# ==============================================================================

set -euo pipefail

log() { echo -e "\033[1;32m[+] $*\033[0m"; }

echo "=================================================="
echo "    LINUX OOM KILLER FORENSIC REPORT"
echo "=================================================="

# 1. Search kernel journal for OOM invocations
log "Searching kernel ring buffer for OOM events..."
dmesg -T | grep -E -C 3 -i "out of memory|killed process" || echo "No OOM events recorded in dmesg."

# 2. Inspect memory status
log "System Memory Breakdown:"
free -h
grep -E "MemTotal|MemFree|MemAvailable|SwapTotal|SwapFree" /proc/meminfo

# 3. Immunize a critical service (e.g., PostgreSQL or Redis)
protect_service() {
    local service_name="$1"
    local pid
    pid=$(pgrep -f "${service_name}" | head -n 1 || true)

    if [[ -n "${pid}" ]]; then
        log "Protecting '${service_name}' (PID: ${pid}) from OOM Killer..."
        # -1000 makes the process completely immune to the OOM killer
        echo -1000 | sudo tee "/proc/${pid}/oom_score_adj" >/dev/null
        echo "Active oom_score for PID ${pid}: $(cat /proc/${pid}/oom_score)"
        echo "Active oom_score_adj for PID ${pid}: $(cat /proc/${pid}/oom_score_adj)"
    else
        echo "Service '${service_name}' not currently running."
    fi
}

protect_service "postgres"
```

---

## 3. Execution & Verification

### Step-by-Step Test Procedure

1. **Execute Deleted Open File Lab**:
   ```bash
   chmod +x ./lab-disk-exhaustion.sh
   ./lab-disk-exhaustion.sh
   ```

2. **Simulated Verification Output**:
   ```text
   [+] Step 1: Spawning background daemon creating a 500MB log file...
   Daemon: 500MB written. Simulating file deletion by user...
   Daemon: File unlinked. Holding file descriptor open for 120s...
   [+] Step 2: Inspecting disk space...
   --- Checking via du (directory usage) ---
   0	/tmp/disk-lab
   --- Notice: 'du' reports 0 bytes because the file is deleted! ---
   [+] Step 3: Finding deleted files held open by running processes...
   python3   51204 deploy    3w   REG  253,0 524288000 1310724 /tmp/disk-lab/runaway.log (deleted)
   [+] Step 4: Executing remediation...
   [!] Truncating file descriptor 3 of PID 51204 directly in /proc...
   [+] Truncation successful! Disk space reclaimed without killing PID 51204.
   ```

3. **Verify OOM Protection**:
   ```bash
   chmod +x ./oom-forensics.sh
   ./oom-forensics.sh
   ```
   *Expected Output:*
   ```text
   [+] System Memory Breakdown:
                  total        used        free      shared  buff/cache   available
   Mem:            15Gi       4.2Gi       8.1Gi       120Mi       3.2Gi        11Gi
   Swap:          4.0Gi          0B       4.0Gi
   [+] Protecting 'postgres' (PID: 12045) from OOM Killer...
   Active oom_score for PID 12045: 0
   Active oom_score_adj for PID 12045: -1000
   ```

---

## 4. Failure Modes & Edge Cases

| Failure Mode / Incident | Root Cause | Remediation Runbook |
| :--- | :--- | :--- |
| **`No space left on device` but `df -h` shows 30% used** | Inode exhaustion (`df -i` shows 100%). | Run `./find-inode-hogs.sh`. Identify the directory holding millions of small files. Delete using `find . -type f -delete` (do not run `rm *` which fails with `Argument list too long`). |
| **`df` and `du` mismatch after truncate** | Other processes or secondary threads hold duplicate open file handles. | Re-run `lsof +L1` to verify if other PIDs hold unlinked files; restart the service gracefully if truncate is unsupported. |
| **OOM Killer targets critical DB instead of worker** | Default `oom_score` calculation heavily penalizes large memory users (e.g., PostgreSQL buffer pool). | Set `OOMScoreAdjust=-1000` in the database systemd unit file and set `OOMScoreAdjust=+500` in untrusted worker units. |
| **Kswapd pegged at 100% CPU during memory pressure** | Node has no swap configured; memory allocator continuously scans page cache under low memory. | Configure 2GB–4GB of swap space or zram swap (`vm.swappiness=10`) to provide breathing room for page reclamation. |

---

## 5. Production Considerations

- **Automated Disk Alerting**: Configure Prometheus alert rules triggering at **80%** (Warning) and **90%** (Critical) for *both* capacity (`node_filesystem_free_bytes / node_filesystem_size_bytes`) and inodes (`node_filesystem_files_free / node_filesystem_files`).
- **Systemd OOMD Daemon**: On modern systemd (v247+), enable `systemd-oomd.service` to kill memory-leaking application slices gracefully at the cgroup boundary before the kernel's uncoordinated OOM killer fires.
- **Logrotate Compaction**: Enforce `delaycompress` and `copytruncate` in `/etc/logrotate.d/` for applications that cannot reopen log files upon receiving `SIGHUP`.

---
*Next Project: [08-monitoring-with-node-exporter.md](file:///04-devops/linux/07-projects/08-monitoring-with-node-exporter.md)*
