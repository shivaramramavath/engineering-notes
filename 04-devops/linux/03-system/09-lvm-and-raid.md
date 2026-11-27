<!--
================================================================================
Metadata:
  Title: Logical Volume Management (LVM) & Software RAID
  Track: Linux & Systems Engineering / DevOps
  Difficulty: Advanced
  Estimated Time: 65 minutes
  Prerequisites: 03-system/07-storage-and-filesystems.md, 03-system/08-mounting-fstab-and-disks.md
  Key Terminology: LVM2, Physical Volume (PV), Volume Group (VG), Logical Volume (LV), Physical Extent (PE), Logical Extent (LE), lvextend, lvreduce, LVM Snapshots, Thin Pools, mdadm, Software RAID (RAID 0, 1, 5, 10), Rebuild
================================================================================
-->

# Logical Volume Management (LVM) & Software RAID

[<- 08-mounting-fstab-and-disks.md](./08-mounting-fstab-and-disks.md) | [Back to Module Overview](./README.md) | [10-swap-and-memory-management.md ->](./10-swap-and-memory-management.md)

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

In traditional partitioning, filesystems are permanently constrained by physical disk boundaries. If `/dev/sda2` runs out of space, an administrator cannot simply allocate unused sectors from `/dev/sdb1` without complex, high-risk offline repartitioning and downtime.

Modern production storage relies on virtualization abstractions:
1. **Logical Volume Management (LVM2)**: Decouples the filesystem from physical silicon. Multiple physical disks are pooled into a single storage group, from which virtual partitions (Logical Volumes) can be carved out, resized on the fly while mounted and serving traffic, and snapshotted for instantaneous database backups.
2. **Software RAID (`mdadm`)**: Aggregates multiple physical disks at the block level into resilient arrays (RAID 1, 5, 10) to deliver hardware fault tolerance and high read/write throughput without requiring expensive hardware RAID controllers.

Mastering LVM and software RAID gives systems engineers complete flexibility to expand, protect, and snapshot storage in mission-critical environments.

---

## 2. Core Concepts

### The Three-Tier LVM Architecture
LVM organizes storage into three distinct logical abstraction layers:

```
+-------------------------------------------------------------------------+
| LVM THREE-TIER STORAGE ABSTRACTION STACK                                |
|                                                                         |
|  [ Physical Volumes (PV) ]                                              |
|  Raw block devices or partitions initialized for LVM                    |
|  /dev/sdb1 (100 GB)             /dev/sdc1 (200 GB)                      |
|         │                                │                              |
|         └────────────────┬───────────────┘                              |
|                          v                                              |
|  [ Volume Group (VG) ]                                                  |
|  Storage pool aggregating all PVs (Total Pool: 300 GB)                  |
|  Divided into uniform Physical Extents (PE, default 4 MB chunks)        |
|  ┌───────────────────────────────────────────────────────────────────┐  |
|  │ [PE] [PE] [PE] [PE] [PE] [PE] [PE] [PE] [PE] ... (76,800 Extents) │  |
|  └───────────────────────────────┬───────────────────────────────────┘  |
|                                  │                                      |
|            ┌─────────────────────┴─────────────────────┐                |
|            v                                           v                |
|  [ Logical Volumes (LV) ]                                               |
|  Virtual partitions exposed as /dev/mapper/vg-lv or /dev/vg/lv          |
|  lv_db (150 GB) ──> Ext4                                lv_logs (50 GB) │
+-------------------------------------------------------------------------+
```

1. **Physical Volume (PV)**: A raw disk (`/dev/nvme1n1`) or partition (`/dev/sdb1`) initialized with LVM metadata headers using `pvcreate`.
2. **Volume Group (VG)**: A unified storage pool created by grouping one or more PVs together using `vgcreate`. Storage within a VG is sliced into uniform chunks called **Physical Extents (PE)** (typically 4 MB each).
3. **Logical Volume (LV)**: A slice of extents allocated from a VG, formatted with a filesystem, and mounted to the VFS tree. Can span across multiple physical disks transparently.

### Software RAID Levels (`mdadm`)
- **RAID 0 (Striping)**: Data is sliced across $\ge 2$ disks. Doubles read/write throughput. **Zero fault tolerance**: failure of one disk destroys the entire array!
- **RAID 1 (Mirroring)**: Data is duplicated identically across $\ge 2$ disks. Reads are fast; writes incur slight overhead. Survives failure of $N-1$ disks. Capacity is that of the smallest single disk.
- **RAID 5 (Striping with Distributed Parity)**: Requires $\ge 3$ disks. Data and parity blocks are distributed. Survives failure of 1 disk. Usable capacity is $(N - 1) \times \text{size}$.
- **RAID 10 (1+0, Mirrored Stripes)**: Requires $\ge 4$ disks. Combines the high performance of RAID 0 with the redundancy of RAID 1. Can survive multiple disk failures as long as no mirrored pair is lost simultaneously.

---

## 3. How It Works Internally

LVM is implemented inside the Linux kernel via the **Device Mapper (`dm-mod`)** framework:

```mermaid
flowchart TD
    A[User Space: lv_database] --> B[/dev/mapper/vg_prod-lv_database]
    B --> C[Kernel Device Mapper Engine]
    
    C --> D{Linear Extent Mapping Table}
    D -->|LE 0..25000| E[Physical Disk 1: /dev/sdb1]
    D -->|LE 25001..50000| F[Physical Disk 2: /dev/sdc1]
    
    E --> G[Storage Hardware Controller]
    F --> G
```

### The Device Mapper Mapping Table
- Every logical volume is assigned a device mapper target node (e.g., `/dev/dm-0`) and symlinked under `/dev/mapper/<vg>-<lv>`.
- The kernel translates Logical Extent (LE) block offsets to Physical Extent (PE) sector numbers on physical drives using a dynamic in-memory mapping table.
- When an LV is expanded, the kernel simply appends new extents to the mapping table. Data on the disk is **not moved**, making expansion instantaneous.

---

## 4. Syntax & Usage

### 1. The LVM Command Hierarchy
LVM commands follow a strict, intuitive naming pattern across the three tiers:

| Operation | Physical Volumes (`pv*`) | Volume Groups (`vg*`) | Logical Volumes (`lv*`) |
| :--- | :--- | :--- | :--- |
| **Create** | `sudo pvcreate <dev>` | `sudo vgcreate <vg> <pv...>`| `sudo lvcreate -n <lv> -L <size> <vg>` |
| **Display Summary**| `pvs` | `vgs` | `lvs` |
| **Detailed View** | `sudo pvdisplay` | `sudo vgdisplay` | `sudo lvdisplay` |
| **Expand / Extend**| `sudo pvresize <dev>` | `sudo vgextend <vg> <pv>` | `sudo lvextend -L +<size> <lv>` |
| **Reduce / Shrink**| N/A | `sudo vgreduce <vg> <pv>` | `sudo lvreduce -L -<size> <lv>` |
| **Remove / Wipe** | `sudo pvremove <dev>` | `sudo vgremove <vg>` | `sudo lvremove <lv>` |

### 2. Logical Volume Creation & Management
```bash
# 1. Initialize physical partitions as PVs
sudo pvcreate /dev/sdb1 /dev/sdc1

# 2. Create Volume Group named 'vg_data'
sudo vgcreate vg_data /dev/sdb1 /dev/sdc1

# 3. Create a 50 GB Logical Volume named 'lv_postgres'
sudo lvcreate -L 50G -n lv_postgres vg_data

# Alternatively, allocate 100% of remaining free extents in VG:
sudo lvcreate -l 100%FREE -n lv_app vg_data

# Format and mount
sudo mkfs.ext4 /dev/vg_data/lv_postgres
sudo mkdir -p /var/lib/postgresql
sudo mount /dev/vg_data/lv_postgres /var/lib/postgresql
```

### 3. Online Storage Expansion (Zero Downtime)
Expand an existing volume by 20 GB and resize its filesystem while mounted:
```bash
# For Ext4: Expand LV and resize filesystem in a SINGLE command with -r (--resizefs)
sudo lvextend -r -L +20G /dev/vg_data/lv_postgres

# Manual two-step expansion for Ext4:
sudo lvextend -L +20G /dev/vg_data/lv_postgres
sudo resize2fs /dev/vg_data/lv_postgres

# Manual expansion for XFS (requires mount point directory!):
sudo lvextend -L +20G /dev/vg_data/lv_postgres
sudo xfs_growfs /var/lib/postgresql
```

### 4. Software RAID Management with `mdadm`
```bash
# Create a RAID 1 mirrored array using two drives
sudo mdadm --create /dev/md0 --level=1 --raid-devices=2 /dev/sdb1 /dev/sdc1

# Inspect array synchronization status and health
cat /proc/mdstat
sudo mdadm --detail /dev/md0

# Mark a disk as faulty (simulate hardware failure)
sudo mdadm /dev/md0 --fail /dev/sdb1

# Remove failed disk
sudo mdadm /dev/md0 --remove /dev/sdb1

# Insert new replacement disk (automatic rebuild begins)
sudo mdadm /dev/md0 --add /dev/sdd1

# Save RAID configuration persistently
sudo mdadm --detail --scan | sudo tee -a /etc/mdadm/mdadm.conf
sudo update-initramfs -u
```

---

## 5. Practical Examples

### 1. Basic: Adding a New Hard Drive to an Existing Full Volume Group
A database volume runs low on space. A new 500 GB NVMe drive (`/dev/nvme1n1`) is added to the server:
```bash
#!/usr/bin/env bash
set -euo pipefail

NEW_DISK="/dev/nvme1n1"
VG_NAME="vg_data"
LV_PATH="/dev/vg_data/lv_postgres"

echo "[INFO] Initializing new PV..."
sudo pvcreate "$NEW_DISK"

echo "[INFO] Expanding Volume Group $VG_NAME..."
sudo vgextend "$VG_NAME" "$NEW_DISK"

echo "[INFO] Expanding Logical Volume and filesystem online..."
sudo lvextend -r -l +100%FREE "$LV_PATH"

echo "[SUCCESS] Storage capacity expanded live with zero downtime:"
df -h /var/lib/postgresql
```

### 2. Real-World: Atomic Database Backups Using LVM Snapshots
Backing up a 200 GB active database without locking tables for hours:
```bash
#!/usr/bin/env bash
set -euo pipefail

# 1. Flush tables and create LVM snapshot (takes ~1 second!)
# A snapshot reserves copy-on-write (CoW) space (e.g. 10 GB)
sudo lvcreate -L 10G -s -n lv_db_snap /dev/vg_data/lv_postgres

# 2. Mount snapshot in read-only mode to a temporary directory
# Note: XFS requires 'nouuid' mount option because UUID duplicates the source!
sudo mkdir -p /mnt/snapshot
sudo mount -o ro,nouuid /dev/vg_data/lv_db_snap /mnt/snapshot

# 3. Stream backup to compressed tar archive or S3 at leisure
tar --zstd -cf /mnt/backups/db_$(date +%F).tar.zst -C /mnt/snapshot .

# 4. Unmount and destroy snapshot immediately
sudo umount /mnt/snapshot
sudo lvremove -y /dev/vg_data/lv_db_snap
echo "[SUCCESS] Atomic snapshot backup completed."
```

### 3. Edge Case: Snapshot Space Exhaustion (Invalidated Snapshots)
LVM snapshots operate on **Copy-on-Write (CoW)**:
- When original data blocks change, the old blocks are copied into the snapshot's allocated volume.
- If the original database writes 12 GB of modifications while a 10 GB snapshot is active:
  ```text
  device-mapper: snapshots: Snapshot is invalid!
  ```
- **Consequence**: The snapshot is immediately dropped by the kernel (`INACTIVE`) and cannot be mounted or read!
- **Rule**: Size snapshot volumes generously, or configure LVM auto-extend in `/etc/lvm/lvm.conf` (`snapshot_autoextend_threshold = 70`).

### 4. Failure Case: Attempting to Shrink an XFS Logical Volume
An administrator attempts to shrink an XFS logical volume to reallocate space to another service:
```bash
# DISASTROUS ATTEMPT:
sudo lvreduce -L -20G /dev/vg_data/lv_xfs_app
# Consequence: The underlying block volume is truncated by 20GB,
# but XFS cannot shrink! The filesystem metadata is permanently corrupted!

# PRODUCTION RULE:
# 1. NEVER reduce an LV formatted with XFS!
# 2. If using Ext4, ALWAYS resize the filesystem FIRST before reducing the LV:
#    sudo resize2fs /dev/vg/lv 30G
#    sudo lvreduce -L 30G /dev/vg/lv
# Or use the safe flag: sudo lvreduce -r -L 30G /dev/vg/lv
```

### 5. Production-Grade: Automated RAID Health Monitoring & Slack Alerting
A production cron daemon that checks `/proc/mdstat`, identifies degraded arrays, and dispatches emergency alerts:
```bash
#!/usr/bin/env bash
set -euo pipefail

MDSTAT="/proc/mdstat"
DEGRADED_COUNT=0

if grep -q "\[.*_.*\]" "$MDSTAT"; then
  echo "[CRITICAL ALERT] Degraded Software RAID Array detected!" >&2
  DEGRADED_ARRAYS=$(grep -B 1 "\[.*_.*\]" "$MDSTAT" | awk '/^md/ {print $1}')
  
  for array in $DEGRADED_ARRAYS; do
    echo "--- Details for /dev/$array ---" >&2
    sudo mdadm --detail "/dev/$array" >&2
    ((DEGRADED_COUNT++))
  done
  
  # Trigger pager alert
  # curl -X POST -H 'Content-type: application/json' --data '{"text":"RAID DEGRADED"}' $SLACK_WEBHOOK
  exit 1
else
  echo "[OK] All Software RAID arrays are healthy and fully synchronized."
fi
```

---

## 6. Best Practices

1. **Always Use `lvextend -r` (`--resizefs`)**: Avoid running `lvextend` followed separately by `resize2fs`. The `-r` flag atomically resizes both the logical volume and the underlying filesystem in a single verified operation.
2. **Leave Unallocated Space in Volume Groups**: Never allocate 100% of a Volume Group immediately. Keep 15%–20% of the VG unallocated so you can create LVM snapshots for backups or emergency storage expansions.
3. **Never Shrink XFS Volumes**: XFS does not support shrinking. Always verify filesystem type (`lsblk -f`) before running `lvreduce`.
4. **Save RAID Configurations to `/etc/mdadm/mdadm.conf`**: Failure to persist array definitions and update initramfs can prevent RAID arrays from assembling properly on reboot.
5. **Monitor Snapshot CoW Percentage**: Monitor `lvs` during snapshot backups; destroy snapshots as soon as the backup stream completes to minimize I/O overhead.

---

## 7. Common Mistakes & Troubleshooting

| Mistake / Antipattern | Underlying Cause | Consequence | Production Solution |
| :--- | :--- | :--- | :--- |
| Reducing LV without shrinking filesystem | Block device is truncated before filesystem metadata is scaled back. | Massive filesystem corruption and total data loss. | Use `lvreduce -r` (Ext4 only), or recreate filesystem. |
| Forgetting `nouuid` when mounting XFS snapshots | XFS refuses to mount a second volume with an identical filesystem UUID. | `mount: /dev/vg/snap: Filesystem has duplicate UUID`. | Mount with option: `mount -o ro,nouuid /dev/vg/snap /mnt`. |
| Allocating 100% of VG at install time | No free extents remain for LVM snapshots or volume expansion. | Cannot take LVM snapshots without destroying existing data. | Reserve 15%–20% free space in every Volume Group. |
| Neglecting to update initramfs after RAID changes | Early boot environment lacks updated `mdadm.conf`. | Boot fails with root array missing or renamed to `/dev/md127`. | Run `sudo update-initramfs -u`. |
| Snapshot filling up (100% CoW) | High write volume to origin volume exhausts snapshot space. | Snapshot is marked invalid and dropped by kernel. | Allocate larger snapshot volume or set autoextend threshold. |
| Using RAID 0 for production databases | Zero fault tolerance; disk failure destroys entire stripe. | Complete catastrophic data loss on single disk failure. | Use RAID 10 or RAID 5 for production workloads. |

---

## 8. Performance, Security & Cost Implications

### Performance
- **RAID Performance Profiles**:
  - **RAID 0**: Maximum sequential read/write speed ($N \times \text{throughput}$). Zero redundancy.
  - **RAID 1**: $N \times$ read speed; write speed equals slowest single disk.
  - **RAID 10**: Balances write speed with read speed, offering optimal IOPS for high-write databases (PostgreSQL/MySQL).
- **Snapshot I/O Tax**: While an LVM snapshot is active, every disk write to an unchanged block incurs a **write penalty** (read old block, write to snapshot, write new block). Destroy snapshots promptly once backups finish.

### Security
- **Secure Wiping of Discarded LVs**: When removing an LV (`lvremove`), the kernel merely deletes the extent pointers. The raw data remains intact in physical extents. If security compliance (PCI-DSS, HIPAA) requires sanitization, overwrite with `shred` before calling `lvremove`.

### Cost
- Software RAID eliminates the need to purchase expensive, proprietary hardware RAID cards. It allows using commodity NVMe or SATA drives while preserving high availability and enterprise disk redundancy.

---

## 9. Comparison Table

| Attribute | LVM (Logical Volume Manager) | Software RAID (`mdadm`) | Hardware RAID Controller |
| :--- | :--- | :--- | :--- |
| **Primary Goal** | **Storage Flexibility & Online Resizing**| **Fault Tolerance & Performance Striping**| Fault Tolerance & Battery-backed Cache |
| **Fault Tolerance** | None (unless LVM-RAID used) | **High (Survives disk failures)** | High (Survives disk failures) |
| **Online Resize** | **Instant and Seamless** | Complex / Multi-step | Requires controller reboot/GUI |
| **Snapshots** | **Native Copy-on-Write Snapshots** | None | Controller-dependent |
| **Portability** | Universal across all Linux kernels | Universal across all Linux kernels | **Vendor Lock-in (Requires exact card)**|
| **Cost** | 100% Free / Open Source | 100% Free / Open Source | Expensive hardware controller |

---

## 10. Real-World Interview Questions

### Q1: What is the exact relationship between Physical Extents (PE) and Logical Extents (LE) in LVM?
<details>
<summary>Answer</summary>
In LVM architecture:
- A **Volume Group (VG)** divides all attached Physical Volumes (PVs) into equal-sized chunks of storage called **Physical Extents (PE)** (by default, 4 MB each).
- When a **Logical Volume (LV)** is created, LVM allocates a corresponding number of **Logical Extents (LE)**.
- Under linear mapping, LE 0 maps to PE 0 on Disk 1, LE 1 maps to PE 1, and so forth.
- The kernel's Device Mapper driver maps LE numbers to PE coordinates. When an LV is resized, LVM simply adds or removes extents from the mapping table. Because extents are uniform blocks, an LV can seamlessly combine extents from different physical disks into a single continuous logical block device.
</details>

### Q2: Why is the `-r` (`--resizefs`) flag critical when running `lvextend` or `lvreduce`?
<details>
<summary>Answer</summary>
A storage volume consists of two layers: the underlying **Block Device** (managed by LVM) and the **Filesystem** (Ext4, XFS) formatted on top of it.
If an administrator runs `lvextend -L +20G /dev/vg/lv` without `-r`, the block device grows, but the filesystem remains its original size until an additional command (`resize2fs` or `xfs_growfs`) is manually invoked.
Conversely, if shrinking via `lvreduce` without `-r`, the block device is truncated **before** the filesystem is shrunk, instantly truncating data blocks and corrupting the filesystem.
The `-r` flag coordinates both layers atomically:
- On expand: grows the LV, then grows the filesystem.
- On shrink (Ext4): shrinks the filesystem first, then shrinks the LV safely.
</details>

### Q3: How does an LVM Copy-on-Write (CoW) Snapshot work, and what causes it to become invalid?
<details>
<summary>Answer</summary>
When an LVM snapshot is created (`lvcreate -s`), it does not duplicate data; it creates an empty mapping table pointing to the origin volume's blocks.
- **Copy-on-Write Operation**: When a write occurs on the origin volume to an unchanged block, LVM intercepts the write, copies the **original unmodified block** into the snapshot volume's reserved space, and then allows the write to proceed on the origin.
- Reads to the snapshot see unchanged blocks from the origin and modified blocks from the snapshot storage, presenting a frozen point-in-time view.
- **Invalidation**: If the snapshot volume's reserved storage fills up (100% capacity) because too many changes occurred on the origin, the snapshot can no longer store historical blocks. The kernel marks the snapshot **invalid**, drops it from memory, and refuses subsequent reads.
</details>

### Q4: Explain the difference between RAID 1, RAID 5, and RAID 10.
<details>
<summary>Answer</summary>
- **RAID 1 (Mirroring)**: Data is duplicated across 2 disks. Survives 1 disk failure. 50% capacity efficiency. Excellent read throughput; write speed bounded by slowest disk.
- **RAID 5 (Striping with Parity)**: Requires $\ge 3$ disks. Parity is distributed across all drives. Survives 1 disk failure. High capacity efficiency ($(N-1)/N$). Suffers from the "RAID 5 write penalty" due to calculating and rewriting parity blocks on random writes.
- **RAID 10 (Striped Mirrors)**: Requires $\ge 4$ disks. Pairs disks into RAID 1 mirrors, then stripes data across the pairs (RAID 0). Combines the high IOPS and write speed of striping with the redundancy of mirroring. Can survive multiple disk failures as long as no two failed disks belong to the same mirror.
</details>

### Q5: What is Thin Provisioning in LVM (Thin Pools)?
<details>
<summary>Answer</summary>
Standard LVM volumes pre-allocate all requested extents immediately (thick provisioning).
In **LVM Thin Provisioning**:
1. A shared storage pool (Thin Pool) is created.
2. Multiple Thin Volumes are carved out with virtual capacities larger than the physical pool (over-provisioning, e.g., creating five 100 GB volumes from a 200 GB physical pool).
3. Extents are allocated **only when data blocks are actually written** by the filesystem.
4. It also supports instantaneous recursive snapshots.
*Operational Risk*: If consumers write more data than the physical pool contains without administrators expanding the pool, all thin volumes lock up.
</details>

### Q6: How do you replace a failed hard drive in an `mdadm` RAID 1 array?
<details>
<summary>Answer</summary>
1. Mark the drive as failed:
   ```bash
   sudo mdadm /dev/md0 --fail /dev/sdb1
   ```
2. Remove the failed drive from the active array:
   ```bash
   sudo mdadm /dev/md0 --remove /dev/sdb1
   ```
3. Physically replace the hardware drive.
4. Copy the partition table from the healthy drive to the new drive:
   ```bash
   sudo sfdisk -d /dev/sdc | sudo sfdisk /dev/sdb
   ```
5. Add the new partition back into the RAID array:
   ```bash
   sudo mdadm /dev/md0 --add /dev/sdb1
   ```
The kernel immediately initiates background synchronization, visible in `/proc/mdstat`.
</details>

### Q7: What is the purpose of `xfs_growfs` requiring a mount point instead of a device path?
<details>
<summary>Answer</summary>
Unlike `resize2fs` (which can take a block device node like `/dev/sdb1`), `xfs_growfs` operates exclusively through filesystem-level ioctl calls sent to a **currently mounted filesystem**.
Therefore, `xfs_growfs` must be passed the **mount point directory** (e.g., `xfs_growfs /mnt/data`), not the block device node. Passing a block device path triggers an immediate error.
</details>

### Q8: What is a "Split Brain" scenario in RAID, and how does `mdadm` prevent it?
<details>
<summary>Answer</summary>
A split-brain occurs in mirrored storage when two halves of an array become disconnected, both accept independent writes, and their data diverges irreversibly.
`mdadm` prevents this using metadata event counters (stored in the RAID superblock on each disk sector). Every time an array state changes or an unclean shutdown occurs, the event counter increments. When assembling at boot, `mdadm` compares event counters across all disks: if a drive is out of date, it is rejected and marked degraded, ensuring only consistent data is exposed.
</details>

---

## 11. Practice Exercises

### Exercise 1: Inspecting LVM Hierarchy (Easy)
**Task**: Use `pvs`, `vgs`, and `lvs` to display a clean columnar summary of all physical volumes, volume groups, and logical volumes on the machine.

<details>
<summary>Solution</summary>

```bash
sudo pvs
echo "----------------------------------------"
sudo vgs
echo "----------------------------------------"
sudo lvs
```
</details>

### Exercise 2: Building an LVM Volume from Loopback Images (Easy)
**Task**: Create two 50 MB loopback images, initialize them as PVs, assemble them into a VG named `vg_test`, create a 60 MB LV named `lv_demo`, and format it with Ext4.

<details>
<summary>Solution</summary>

```bash
#!/usr/bin/env bash
set -euo pipefail

# 1. Create loopback images
dd if=/dev/zero of=/tmp/disk1.img bs=1M count=50
dd if=/dev/zero of=/tmp/disk2.img bs=1M count=50
DEV1=$(sudo losetup -f --show /tmp/disk1.img)
DEV2=$(sudo losetup -f --show /tmp/disk2.img)

# 2. LVM Setup
sudo pvcreate "$DEV1" "$DEV2"
sudo vgcreate vg_test "$DEV1" "$DEV2"
sudo lvcreate -L 60M -n lv_demo vg_test
sudo mkfs.ext4 /dev/vg_test/lv_demo

# 3. Verify
sudo lvs vg_test

# 4. Clean up
sudo lvremove -y /dev/vg_test/lv_demo
sudo vgremove -y vg_test
sudo pvremove -y "$DEV1" "$DEV2"
sudo losetup -d "$DEV1" "$DEV2"
rm -f /tmp/disk1.img /tmp/disk2.img
```
</details>

### Exercise 3: Online Expansion of Ext4 Logical Volume (Medium)
**Task**: Using loop devices, build an LVM setup, mount an Ext4 volume, write a test file, expand the volume and filesystem by 30 MB online using `lvextend -r`, and verify data integrity.

<details>
<summary>Solution</summary>

```bash
#!/usr/bin/env bash
set -euo pipefail

# Setup 100MB disk
dd if=/dev/zero of=/tmp/lvm_expand.img bs=1M count=100
DEV=$(sudo losetup -f --show /tmp/lvm_expand.img)

sudo pvcreate "$DEV"
sudo vgcreate vg_expand "$DEV"
sudo lvcreate -L 40M -n lv_store vg_expand
sudo mkfs.ext4 /dev/vg_expand/lv_store

sudo mkdir -p /mnt/expand_test
sudo mount /dev/vg_expand/lv_store /mnt/expand_test

# Write test data
echo "Important Data" | sudo tee /mnt/expand_test/payload.txt

# Online expansion with -r
echo "[INFO] Capacity before expansion:"
df -h /mnt/expand_test

sudo lvextend -r -L +30M /dev/vg_expand/lv_store

echo "[INFO] Capacity after expansion:"
df -h /mnt/expand_test
cat /mnt/expand_test/payload.txt

# Clean up
sudo umount /mnt/expand_test
sudo lvremove -y /dev/vg_expand/lv_store
sudo vgremove -y vg_expand
sudo pvremove -y "$DEV"
sudo losetup -d "$DEV"
sudo rm -rf /mnt/expand_test /tmp/lvm_expand.img
```
</details>

### Exercise 4: Building a Software RAID 1 Mirrored Array (Medium)
**Task**: Create two 40 MB image files, bind them to loop devices, assemble them into a RAID 1 array `/dev/md99` using `mdadm`, format with Ext4, verify synchronization in `/proc/mdstat`, and stop the array.

<details>
<summary>Solution</summary>

```bash
#!/usr/bin/env bash
set -euo pipefail

# 1. Create loop devices
dd if=/dev/zero of=/tmp/raid1.img bs=1M count=40
dd if=/dev/zero of=/tmp/raid2.img bs=1M count=40
DEV1=$(sudo losetup -f --show /tmp/raid1.img)
DEV2=$(sudo losetup -f --show /tmp/raid2.img)

# 2. Build RAID 1
sudo mdadm --create /dev/md99 --level=1 --raid-devices=2 "$DEV1" "$DEV2" --metadata=1.2

# 3. Check status
cat /proc/mdstat
sudo mdadm --detail /dev/md99

# 4. Stop and clean up
sudo mdadm --stop /dev/md99
sudo mdadm --zero-superblock "$DEV1" "$DEV2" || true
sudo losetup -d "$DEV1" "$DEV2"
rm -f /tmp/raid1.img /tmp/raid2.img
```
</details>

### Exercise 5: Live Simulated Disk Failure and Rebuild in RAID (Hard)
**Task**: Create a 3-disk RAID 5 array with loop devices. Simulate a physical drive failure using `mdadm --fail`, verify array continues serving reads, remove the broken disk, insert a fresh replacement disk, and watch `/proc/mdstat` rebuild the parity.

<details>
<summary>Solution</summary>

```bash
#!/usr/bin/env bash
set -euo pipefail

# 1. Setup 3 disks for RAID 5
for i in {1..3}; do
  dd if=/dev/zero of="/tmp/r5_${i}.img" bs=1M count=40
done
D1=$(sudo losetup -f --show /tmp/r5_1.img)
D2=$(sudo losetup -f --show /tmp/r5_2.img)
D3=$(sudo losetup -f --show /tmp/r5_3.img)

sudo mdadm --create /dev/md88 --level=5 --raid-devices=3 "$D1" "$D2" "$D3"
sleep 2

# 2. Simulate failure of Drive 2
echo "[INFO] Simulating disk failure on $D2..."
sudo mdadm /dev/md88 --fail "$D2"
sudo mdadm /dev/md88 --remove "$D2"

# Array is now degraded but still functioning!
grep -A 2 "md88" /proc/mdstat

# 3. Add replacement disk
dd if=/dev/zero of=/tmp/r5_spare.img bs=1M count=40
D_SPARE=$(sudo losetup -f --show /tmp/r5_spare.img)

echo "[INFO] Adding spare disk to initiate rebuild..."
sudo mdadm /dev/md88 --add "$D_SPARE"
sleep 2

# Rebuild in progress
grep -A 3 "md88" /proc/mdstat

# Clean up
sudo mdadm --stop /dev/md88
sudo losetup -d "$D1" "$D2" "$D3" "$D_SPARE"
rm -f /tmp/r5_*.img
```
</details>

### Exercise 6: Automated LVM Snapshot Rollback System (Hard)
**Task**: Build a script that creates an LVM snapshot before executing a simulated risky database migration. If the migration fails, cleanly unmount, merge the snapshot back into the origin using `lvconvert --merge`, and verify the original data state is restored.

<details>
<summary>Solution</summary>

```bash
#!/usr/bin/env bash
set -euo pipefail

# Assumes existing Volume Group vg_data with lv_prod
# 1. Take snapshot
echo "[INFO] Taking pre-migration snapshot..."
sudo lvcreate -L 200M -s -n lv_prod_snap /dev/vg_data/lv_prod

# 2. Simulate risky migration failure
echo "[ERROR] Database migration corrupted table indices!"

# 3. Unmount origin
sudo umount /mnt/production

# 4. Rollback: merge snapshot into origin
echo "[INFO] Merging snapshot to rollback state..."
sudo lvconvert --merge /dev/vg_data/lv_prod_snap

# 5. Remount origin
sudo mount /dev/vg_data/lv_prod /mnt/production
echo "[SUCCESS] Rollback complete. Original state restored."
```
</details>

---

## 12. Summary & Quick Reference Cheat Sheet

| Task | LVM Command | Software RAID Command |
| :--- | :--- | :--- |
| **Initialize Disk** | `sudo pvcreate /dev/sdb` | `sudo mdadm --zero-superblock /dev/sdb` |
| **Create Pool / Array** | `sudo vgcreate <vg> <pvs...>` | `sudo mdadm --create /dev/md0 --level=1 ...` |
| **Create Volume** | `sudo lvcreate -L <size> -n <lv> <vg>` | N/A (Array is the block device) |
| **Status Summary** | `pvs`, `vgs`, `lvs` | `cat /proc/mdstat` |
| **Detailed Status** | `sudo lvdisplay /dev/vg/lv` | `sudo mdadm --detail /dev/md0` |
| **Expand Storage** | `sudo lvextend -r -L +10G /dev/vg/lv` | `sudo mdadm --grow /dev/md0 ...` |
| **Create Snapshot** | `sudo lvcreate -L 5G -s -n snap /dev/vg/lv` | N/A |
| **Merge / Rollback Snap**| `sudo lvconvert --merge /dev/vg/snap` | N/A |
| **Fail Disk in Array** | N/A | `sudo mdadm /dev/md0 --fail <dev>` |
| **Replace Disk in Array**| `sudo pvmove <old_pv> <new_pv>` | `sudo mdadm /dev/md0 --add <dev>` |

---

## 13. Further Reading & Official Documentation

- [Red Hat Enterprise Linux 9: Configuring and Managing LVM](https://access.redhat.com/documentation/en-us/red_hat_enterprise_linux/9/html/configuring_and_managing_logical_volumes/)
- [Linux Man Pages: lvm(8)](https://man7.org/linux/man-pages/man8/lvm.8.html)
- [Linux Man Pages: mdadm(8)](https://man7.org/linux/man-pages/man8/mdadm.8.html)
- [Linux Kernel Documentation: Software RAID](https://docs.kernel.org/driver-api/md/index.html)
- [ArchWiki: LVM Architecture and Best Practices](https://wiki.archlinux.org/title/LVM)

---

[<- 08-mounting-fstab-and-disks.md](./08-mounting-fstab-and-disks.md) | [Back to Module Overview](./README.md) | [10-swap-and-memory-management.md ->](./10-swap-and-memory-management.md)
