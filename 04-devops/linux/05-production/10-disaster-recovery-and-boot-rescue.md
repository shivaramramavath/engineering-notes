# Linux Disaster Recovery, Boot Rescue, and Chroot Diagnostics

| Metadata | Details |
| :--- | :--- |
| **Topic** | Linux Boot Failure Recovery, GRUB Rescue, Initramfs Shell, Chroot Repair, Kernel Panic Triage |
| **Module** | `05-production` |
| **Track** | `04-devops/linux` |
| **Prerequisites** | `01-fundamentals/11-boot-process.md`, `03-system/07-storage-and-filesystems.md`, `03-system/08-mounting-fstab-and-disks.md` |
| **Target Audience** | SREs, Systems Administrators, Cloud Operations, Incident Responders |
| **Target Distribution** | Ubuntu 24.04 LTS, Rocky Linux / RHEL 9.4 |
| **Key Tools / Prompts** | `grub-install`, `update-grub`, `fsck`, `chroot`, `dracut`, `update-initramfs`, GRUB Rescue |

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

The ultimate nightmare for any systems engineer is a critical server that fails to boot after an unexpected power outage, bad kernel upgrade, corrupted `/etc/fstab`, or damaged bootloader. Symptoms include:
- A frozen screen showing `grub rescue>`.
- Dropping into a minimal BusyBox emergency shell: `(initramfs)`.
- A kernel panic screen displaying `Kernel panic - not syncing: VFS: Unable to mount root fs`.
- Systemd entering emergency mode: `You are in emergency mode. After logging in, type "journalctl -xb"`.

In cloud and bare-metal environments, servers do not have physical monitors or keyboards attached. Recovery must be executed through serial consoles, cloud rescue modes, or attached live rescue media.

Understanding the sequential phases of the Linux boot chain, mastering the **Live CD `chroot` repair technique**, rebuilding corrupted initramfs images, and navigating emergency shells transforms a catastrophic multi-day rebuild into a 10-minute surgical recovery.

---

## Core Concepts

### The Boot Failure Diagnostic Matrix
A server fails at one of four discrete boot stages:

| Boot Stage | Manifestation / Prompt | Common Root Causes | Recovery Strategy |
| :--- | :--- | :--- | :--- |
| **1. BIOS / UEFI** | `No bootable device found` | Disk hardware failure, wiped EFI System Partition (ESP), boot order corrupted | UEFI NVRAM reconfiguration, restore EFI partition |
| **2. GRUB 2** | `grub rescue>` or `grub>` | Missing `/boot/grub`, broken partition layout, UUID mismatch | Manual GRUB root/prefix specification, `insmod normal` |
| **3. Initramfs** | `(initramfs)` or `dracut:/#` | Missing disk driver modules, corrupted root filesystem, UUID changed | Manual root `fsck`, inspect `/dev/disk/by-uuid/`, rebuild initrd |
| **4. Systemd Early Boot**| `Emergency Mode` prompt | Bad `/etc/fstab` mount option (missing disk), failed network target | Mount root read-write, edit `/etc/fstab`, fix mount options |

### The Power of `chroot` (Change Root)
A live rescue environment boots an ephemeral Linux operating system running in RAM from a USB, ISO, or network PXE. 
The **`chroot` (Change Root)** system call changes the apparent root directory for the current running process and its children.
- By mounting the damaged server's hard drive partitions and binding host kernel virtual filesystems (`/dev`, `/proc`, `/sys`), an engineer can "step inside" the broken operating system.
- Once inside the chroot, you execute native commands (`apt install`, `update-grub`, `passwd`, `dracut`) as if the broken machine were booted normally.

### Emergency Mode vs Rescue Mode in Systemd
- **`rescue.target` (Single-User Mode)**: Mounts all local filesystems and starts basic system services. Prompts for root password before launching a root maintenance shell.
- **`emergency.target`**: The most minimal recovery state. Mounts the root filesystem **read-only**, mounts no other filesystems, and starts zero background daemons. Provides a bare root shell to fix corrupted configurations.

---

## Internal Working

```
+-----------------------------------------------------------------------------------------+
|                                    UEFI Firmware Execution                              |
|                              Executes \EFI\ubuntu\grubx64.efi                           |
+----------------------------------------------------+------------------------------------+
                                                     |
                                                     v
+-----------------------------------------------------------------------------------------+
|                                       GRUB 2 Bootloader                                 |
|                               Reads /boot/grub/grub.cfg                                 |
|   [Failure: Cannot find /boot/grub] ------------------------> [ Drops to grub rescue> ] |
+----------------------------------------------------+------------------------------------+
                                                     | Success: Loads Kernel + Initramfs
                                                     v
+-----------------------------------------------------------------------------------------+
|                                   Initramfs (Early User Space)                          |
|                             Loads storage & NVMe drivers into RAM                       |
|                             Mounts real root filesystem via UUID                        |
|   [Failure: Cannot mount root or dirty fs] -----------------> [ Drops to (initramfs)  ] |
+----------------------------------------------------+------------------------------------+
                                                     | Success: switch_root to Real Storage
                                                     v
+-----------------------------------------------------------------------------------------+
|                                    Systemd (PID 1 Execution)                            |
|                             Mounts /etc/fstab, starts targets                           |
|   [Failure: Missing fstab drive or failed unit] ------------> [ Drops to Emergency Mode]|
+----------------------------------------------------+------------------------------------+
                                                     | Success
                                                     v
                                      System Reaches multi-user.target
```

### The Chroot Pseudo-Filesystem Binding Chain
For tools like `grub-install` or `apt` to function inside a chroot, they must communicate with the host kernel's hardware representation:
```
Rescue Host OS                                           Chroot Mount Target (/mnt)
/dev  (Device Nodes)      ====== bind mount ======>      /mnt/dev
/proc (Kernel Process FS) ====== proc mount ======>      /mnt/proc
/sys  (Hardware State)    ====== sysfs mount =====>      /mnt/sys
/run  (Runtime Sockets)   ====== bind mount ======>      /mnt/run
```
Without binding `/dev` and `/sys`, `grub-install` fails because it cannot discover physical disk geometry or partition tables!

---

## Syntax & CLI Tools

### Essential GRUB Rescue Commands
```text
grub rescue> ls                      # List all visible disks and partitions, e.g. (hd0) (hd0,gpt1) (hd0,gpt2)
grub rescue> ls (hd0,gpt2)/          # Inspect contents of a specific partition to locate /boot/grub
grub rescue> set root=(hd0,gpt2)     # Set root partition containing /boot
grub rescue> set prefix=(hd0,gpt2)/boot/grub  # Set path to GRUB modules
grub rescue> insmod normal           # Load standard normal execution module
grub rescue> normal                  # Return to the standard graphical GRUB menu!
```

### Filesystem Repair with `fsck`
```bash
# Check and repair ext4 filesystem (partition MUST be unmounted or mounted read-only!)
sudo fsck.ext4 -fy /dev/sda2
# (-f = force check even if marked clean, -y = assume yes to all repair prompts)

# Check and repair XFS filesystem (XFS uses dedicated repair tool)
sudo xfs_repair /dev/sdb1
# (If log is dirty, force zeroing with -L only as a last resort: xfs_repair -L /dev/sdb1)
```

### Rebuilding Initramfs and Bootloader
```bash
# Rebuild initramfs on Debian / Ubuntu
sudo update-initramfs -u -k all

# Rebuild initramfs on RHEL / Rocky Linux (dracut)
sudo dracut --regenerate-all --force

# Reinstall GRUB bootloader to disk MBR/EFI
sudo grub-install /dev/sda

# Re-scan disks and regenerate /boot/grub/grub.cfg
sudo update-grub             # On Ubuntu/Debian
sudo grub2-mkconfig -o /boot/grub2/grub.cfg  # On RHEL/Rocky
```

---

## Progressive Practical Examples

### Example 1: The Canonical Live-CD `chroot` System Repair Procedure
The master runbook for resurrecting an unbootable Linux system using an Ubuntu or Rocky Live USB.

```bash
#!/usr/bin/env bash
# Run as root in the Live Rescue Environment
set -euo pipefail

TARGET_ROOT="/dev/nvme0n1p2"  # Root (/) partition
TARGET_EFI="/dev/nvme0n1p1"   # EFI System Partition (ESP)
MOUNT_POINT="/mnt"

echo "==> 1. Mounting root partition to ${MOUNT_POINT}..."
mount "${TARGET_ROOT}" "${MOUNT_POINT}"

# If subvolumes (Btrfs) or LVM are used, mount accordingly:
# mount -o subvol=@ "${TARGET_ROOT}" "${MOUNT_POINT}"

echo "==> 2. Mounting separate boot and EFI partitions..."
# mount /dev/nvme0n1p3 /mnt/boot (if separate /boot)
mount "${TARGET_EFI}" "${MOUNT_POINT}/boot/efi"

echo "==> 3. Binding host virtual pseudo-filesystems into chroot target..."
mount --bind /dev "${MOUNT_POINT}/dev"
mount --bind /dev/pts "${MOUNT_POINT}/dev/pts"
mount -t proc proc "${MOUNT_POINT}/proc"
mount -t sysfs sys "${MOUNT_POINT}/sys"
mount --bind /run "${MOUNT_POINT}/run"

# Copy resolv.conf to guarantee working DNS inside chroot
cp /etc/resolv.conf "${MOUNT_POINT}/etc/resolv.conf"

echo "==> 4. Entering Chroot Environment..."
chroot "${MOUNT_POINT}" /bin/bash << 'EOF'
echo "Successfully inside broken system environment!"

# Repair package manager
dpkg --configure -a 2>/dev/null || true

# Rebuild initramfs
update-initramfs -u -k all

# Reinstall and update GRUB bootloader
grub-install /dev/nvme0n1
update-grub

echo "Repairs complete. Exiting chroot."
EOF

echo "==> 5. Unmounting cleanly in reverse order..."
umount "${MOUNT_POINT}/boot/efi"
umount "${MOUNT_POINT}/run"
umount "${MOUNT_POINT}/sys"
umount "${MOUNT_POINT}/proc"
umount "${MOUNT_POINT}/dev/pts"
umount "${MOUNT_POINT}/dev"
umount "${MOUNT_POINT}"

echo "System repaired successfully. You may now reboot."
```

### Example 2: Recovering from Systemd Emergency Mode Due to Broken `/etc/fstab`
**Scenario**: An administrator added a secondary data disk to `/etc/fstab`, but later detached the virtual disk. On next boot, systemd cannot find the disk and drops into Emergency Mode:
`Cannot open access to console, the root account is locked. Press Enter to continue.`

```bash
# 1. At emergency root prompt, notice root filesystem is mounted READ-ONLY:
mount | grep "on / "
# Output: /dev/sda2 on / type ext4 (ro,relatime)

# 2. Remount root filesystem read-write:
mount -o remount,rw /

# 3. Edit /etc/fstab to comment out missing drive or append 'nofail' option:
nano /etc/fstab
# Before: UUID=38914-abcd /data ext4 defaults 0 2
# After:  UUID=38914-abcd /data ext4 defaults,nofail 0 2

# 4. Reload systemd daemon and resume standard boot:
systemctl daemon-reload
exit
# System immediately continues booting into multi-user target!
```

### Example 3: Repairing a Corrupted Filesystem from the `(initramfs)` BusyBox Shell
**Scenario**: An ungraceful power loss leaves ext4 metadata inconsistent; kernel halts at `(initramfs)`:
`ALERT! /dev/mapper/vg_data-root does not exist. Dropping to a shell!`
`UNEXPECTED INCONSISTENCY; RUN fsck MANUALLY.`

```bash
# At the (initramfs) prompt:

# 1. Inspect block devices
ls -la /dev/sd* /dev/mapper/*

# 2. Run manual fsck on root volume
fsck.ext4 -y /dev/sda2
# (fsck repairs corrupted inodes, orphaned blocks, and cleans journal)

# 3. Exit initramfs to resume boot:
exit
```

### Example 4: Bypassing Forgotten Root Password via GRUB Kernel Command Line
When root credentials are lost and sudo users are unavailable.

```text
1. Reboot the server. At the GRUB selection menu, highlight the default kernel and press 'e' to edit.
2. Locate the line starting with 'linux' or 'linux16'.
3. Move to the end of the line, remove 'rhgb' and 'quiet', and append:
   init=/bin/bash
   (Or on RHEL: rd.break)
4. Press 'Ctrl+X' or 'F10' to boot.
```

```bash
# The kernel drops directly into an unauthenticated root bash shell!

# 1. Remount root filesystem read-write:
mount -o remount,rw /

# 2. Reset root password:
passwd root

# 3. If SELinux is active (RHEL/Rocky), trigger automatic relabeling:
touch /.autorelabel

# 4. Reboot system:
exec /sbin/init
```

### Example 5: Resolving Kernel Panic "Unable to Mount Root FS"
**Scenario**: After a kernel update, selecting the new kernel emits:
`Kernel panic - not syncing: VFS: Unable to mount root fs on unknown-block(0,0)`

```bash
# Root Cause: The kernel image (vmlinuz) exists, but its companion initramfs image 
# is missing, corrupted, or lacks the NVMe/SATA storage driver.

# 1. At GRUB menu, select "Advanced options for Ubuntu / Rocky"
# 2. Boot into the previous working kernel version (e.g. 6.8.0-30 instead of 6.8.0-31)
# 3. Once booted into the previous kernel, regenerate the broken initramfs:
sudo update-initramfs -u -k 6.8.0-31-generic
sudo update-grub
```

---

## Best Practices

1. **Always Append `nofail` to Non-Essential Mounts in `/etc/fstab`**: For secondary data disks, NAS mounts, or backup partitions, always include the `nofail` option in `/etc/fstab`:
   ```text
   UUID=... /mnt/storage ext4 defaults,nofail 0 2
   ```
   If a disk with `nofail` is missing or unplugged, systemd logs a warning and **continues booting normally**, rather than halting the entire server in Emergency Mode.
2. **Never Run `fsck` on a Read-Write Mounted Filesystem**: Running `fsck` on an actively mounted read-write filesystem will cause severe, irreversible data corruption. Always unmount the partition first (`umount /dev/sdX`) or mount it read-only (`mount -o remount,ro /`).
3. **Always Retain at Least Two Kernel Versions**: Never remove the current running kernel image until the newly installed kernel has successfully booted and stabilized. Most package managers retain the current and previous kernel by default; verify `/boot` contains fallback kernels.
4. **Practice Chroot Recovery in Staging Before an Incident Occurs**: Familiarity with `mount --bind /dev /mnt/dev` under calm conditions ensures flawless execution when production databases are offline.
5. **Remember `touch /.autorelabel` After Out-of-Band Password Resets**: On systems with SELinux (RHEL, Rocky, Fedora), resetting passwords via `init=/bin/bash` creates a new `/etc/shadow` without correct SELinux security contexts. Without `touch /.autorelabel`, SELinux blocks all logins on the subsequent boot.
6. **Set up Cloud Serial Consoles in Advance**: On AWS (EC2 Serial Console), GCP, and Azure, enable interactive serial console access ahead of time. When networking fails or GRUB breaks, the serial console is the only tool that allows interactive boot recovery without detaching volumes.

---

## Common Mistakes & Gotchas

| Mistake | Why It Fails | Correct Pattern |
| :--- | :--- | :--- |
| Forgetting to mount `/dev`, `/proc`, and `/sys` before entering `chroot` | Commands like `grub-install` and `update-initramfs` fail with errors about missing devices or descriptors. | Always bind `/dev`, `/dev/pts`, `/proc`, `/sys`, and `/run` before running `chroot`. |
| Running `xfs_repair` on a mounted XFS volume | Fails immediately with `xfs_repair: device or resource busy`. | Unmount the volume first: `umount /dev/sdb1`. |
| Using `xfs_repair -L` as a first troubleshooting step | `-L` forcefully zeros the log, destroying in-flight database transactions and corrupting data. | Try mounting and unmounting cleanly first to replay the log; use `-L` only as an absolute last resort. |
| Forgetting `touch /.autorelabel` after `rd.break` on RHEL | SELinux blocks root login on next boot due to missing context on `/etc/shadow`. | Always execute `touch /.autorelabel` before exiting. |
| Misidentifying the EFI System Partition (ESP) during `chroot` | `grub-install` writes to the wrong partition or fails with `EFI directory not found`. | Ensure `/boot/efi` is mounted to the FAT32 ESP partition (`vfat`). |
| Editing `/boot/grub/grub.cfg` manually | Changes are overwritten and wiped out on the next package update. | Modify `/etc/default/grub` and compile with `update-grub`. |

---

## Performance, Security & Cost Implications

### Performance
- **Recovery Time Objective (RTO)**: Mastering chroot and emergency mode recovery directly reduces RTO from hours (restoring multi-terabyte VM backups from cold storage) to minutes (fixing a broken bootloader or repairing a corrupted fstab line).

### Security
- **GRUB Password Protection**: Bypassing root passwords via `init=/bin/bash` or `rd.break` demonstrates that **physical or console access equals root access**. To protect bare-metal servers from unauthorized local password resets, protect the GRUB bootloader with a hashed password (`grub-mkpasswd-pbkdf2` in `/etc/grub.d/40_custom`).
- **Secure Boot Verification**: UEFI Secure Boot cryptographically signs the bootloader and kernel. Tampered kernels or unsigned rogue rootkit modules are blocked from booting at the hardware level.

### Cost
- In cloud environments, repairing an unbootable instance in-place via rescue mode saves hundreds of dollars in engineering downtime and avoids the architectural complexity of rebuilding static IP associations, IAM roles, and storage volume re-attachments.

---

## Comparison Table / Alternatives

| Recovery Mode | Boot Stage Activated | Filesystem State | Network Access | Best Used For |
| :--- | :--- | :--- | :--- | :--- |
| **`grub rescue>`** | Stage 2 (Bootloader) | None (Raw disk sectors only) | No | Missing or broken GRUB configuration / partition relocation |
| **`(initramfs)` Shell** | Stage 3 (Early RAM disk) | Read-only memory disk (`rootfs`) | No | Root filesystem corruption, dirty ext4/xfs, missing storage drivers |
| **`emergency.target`** | Stage 4 (Systemd PID 1) | Root (/) mounted Read-Only | No | Broken `/etc/fstab`, corrupted systemd units |
| **`rescue.target`** | Stage 4 (Systemd PID 1) | Local filesystems mounted Read-Write | No | Service dependency deadlocks, single-user maintenance |
| **Live CD `chroot`** | External (Out-of-band)| Controlled by host environment | **Yes** (via Live OS) | Complete bootloader reinstallation, broken libc, kernel rebuild |

---

## Real-World Troubleshooting & Interview Q&A

### Q1: What are the exact steps to rescue a corrupted system using `chroot`?
<details>
<summary>View Solution</summary>

1. Boot into Live Rescue Media.
2. Identify partitions using `lsblk -f`.
3. Mount the root partition: `mount /dev/nvme0n1p2 /mnt`.
4. Mount EFI/Boot partitions: `mount /dev/nvme0n1p1 /mnt/boot/efi`.
5. Bind host virtual filesystems:
   ```bash
   mount --bind /dev /mnt/dev
   mount --bind /dev/pts /mnt/dev/pts
   mount -t proc proc /mnt/proc
   mount -t sysfs sys /mnt/sys
   mount --bind /run /mnt/run
   ```
6. Enter chroot: `chroot /mnt /bin/bash`.
7. Execute repairs (`update-initramfs`, `grub-install`, `passwd`).
8. Exit chroot and unmount all filesystems in reverse order before rebooting.
</details>

### Q2: What causes `grub rescue> error: no such partition` on system boot?
<details>
<summary>View Solution</summary>

**Cause**: GRUB's Stage 1 bootloader (in the MBR or EFI partition) has hardcoded disk sector pointers or partition UUIDs pointing to its Stage 2 files (`/boot/grub`). 
If a disk was cloned, resized, repartitioned, or moved to a different controller slot, the stored partition index is invalid.

**Fix**:
1. At `grub rescue>`, run `ls` to find visible partitions.
2. Test each: `ls (hd0,gpt2)/boot/grub`.
3. Set variables:
   ```text
   set root=(hd0,gpt2)
   set prefix=(hd0,gpt2)/boot/grub
   insmod normal
   normal
   ```
4. Once booted, re-run `sudo grub-install /dev/sda && sudo update-grub`.
</details>

### Q3: Why does `xfs_repair` fail with `xfs_repair: /dev/sdb1 contains a dirty log`?
<details>
<summary>View Solution</summary>

XFS is a journaling filesystem. When a server loses power unexpectedly, metadata updates remain pending in the log journal.
`xfs_repair` refuses to run on a dirty log because replaying the log requires the kernel mount engine.
- **Correct Fix**: Mount the filesystem and immediately unmount it:
  ```bash
  mount /dev/sdb1 /mnt && umount /mnt
  ```
  The kernel mounts the volume, cleanly replays the journal log, and unmounts cleanly. Now `xfs_repair /dev/sdb1` will succeed.
- *Last Resort*: Only if mounting fails completely should you run `xfs_repair -L /dev/sdb1` (which destroys in-flight journaled data).
</details>

### Q4: How do you bypass the root password on a modern RHEL 9 system using `rd.break`?
<details>
<summary>View Solution</summary>

1. At the GRUB menu, press `e` to edit the kernel line.
2. Append `rd.break` to the line starting with `linux`.
3. Press `Ctrl+X` to boot.
4. The kernel halts in early initramfs before switching root, dropping to a root shell at `switch_root:/#`.
5. The real root filesystem is mounted read-only at `/sysroot`.
6. Remount read-write: `mount -o remount,rw /sysroot`.
7. Chroot into root: `chroot /sysroot`.
8. Change password: `passwd root`.
9. Enforce SELinux relabeling: `touch /.autorelabel`.
10. Type `exit` twice to resume boot.
</details>

### Q5: What is the purpose of `systemd.unit=emergency.target` in GRUB parameters?
<details>
<summary>View Solution</summary>

Appending `systemd.unit=emergency.target` to the kernel command line instructs systemd to boot directly into the most minimal maintenance state:
- It mounts the root partition read-only.
- It mounts **no other partitions** from `/etc/fstab`.
- It starts zero system daemons or networking.
- It provides a single root shell to repair disk configuration files or broken systemd units without interference.
</details>

### Q6: How do you fix a system that fails to boot because of a corrupted initramfs image?
<details>
<summary>View Solution</summary>

1. At the GRUB boot menu, select "Advanced Options" and choose an older, functional kernel.
2. Once booted into the alternate kernel:
   - On Ubuntu/Debian: `sudo update-initramfs -u -k all`.
   - On RHEL/Rocky: `sudo dracut --regenerate-all --force`.
3. Update GRUB: `sudo update-grub`.
4. Reboot into the primary kernel.
</details>

### Q7: What is the difference between `chroot` and a Linux container (Docker/LXC)?
<details>
<summary>View Solution</summary>

- **`chroot`**: Only changes the root filesystem mount view for a process. It provides **zero isolation** for processes, network interfaces, user UIDs, or IPC. A root process inside a chroot can see all host processes in `/proc` and can easily escape the chroot using standard syscalls (`fchdir`).
- **Container**: Uses **Linux Kernel Namespaces** (Mount, PID, Network, IPC, UTS, User) and **Cgroups** to provide comprehensive process, network, and resource isolation.
</details>

### Q8: What does the `nofail` option in `/etc/fstab` do?
<details>
<summary>View Solution</summary>

By default, systemd treats every mount listed in `/etc/fstab` as a mandatory dependency. If a device fails to appear within the 90-second timeout, systemd halts the boot sequence and drops into Emergency Mode.
Appending `nofail` instructs systemd to treat the mount as optional: if the disk is missing or unformatted, systemd logs a failure and **continues booting normally into multi-user mode**.
</details>

---

## Hands-On Practice Exercises

### Exercise 1: Exploring GRUB Configuration and Fallback Kernels
**Task**: Inspect `/boot/grub/grub.cfg` to identify all installed kernel versions and fallback recovery menu entries.
<details>
<summary>View Solution</summary>

```bash
sudo grep -E "menuentry '|submenu '" /boot/grub/grub.cfg | head -n 15
```
</details>

### Exercise 2: Simulating Safe Read-Only Remounting
**Task**: Practice remounting a non-critical filesystem read-only and then returning it to read-write mode.
<details>
<summary>View Solution</summary>

```bash
# 1. Create temporary loopback filesystem for testing
truncate -s 100M /tmp/rescue_test.img
mkfs.ext4 /tmp/rescue_test.img
mkdir -p /tmp/test_mount
sudo mount /tmp/rescue_test.img /tmp/test_mount

# 2. Remount read-only
sudo mount -o remount,ro /tmp/test_mount
touch /tmp/test_mount/fail.txt 2>&1 | grep "Read-only file system" || true

# 3. Remount read-write
sudo mount -o remount,rw /tmp/test_mount
touch /tmp/test_mount/success.txt
ls -la /tmp/test_mount/

# 4. Clean up
sudo umount /tmp/test_mount
rm -rf /tmp/rescue_test.img /tmp/test_mount
```
</details>

### Exercise 3: Inspecting Disk UUIDs for Fstab Integrity
**Task**: Compare the UUIDs listed in `/etc/fstab` against the live partition UUIDs reported by `blkid`.
<details>
<summary>View Solution</summary>

```bash
echo "=== Live Block Device UUIDs ==="
sudo blkid

echo "=== Configured /etc/fstab Entries ==="
grep -v "^#" /etc/fstab | grep -E "UUID|/dev"
```
</details>

### Exercise 4: Testing Dry-Run Rebuilding of Initramfs
**Task**: Test the creation of a temporary initramfs image using `dracut` or `mkinitramfs`.
<details>
<summary>View Solution</summary>

```bash
# On Debian/Ubuntu:
if command -v mkinitramfs &>/dev/null; then
    sudo mkinitramfs -o /tmp/test_initrd.img $(uname -r)
    ls -lh /tmp/test_initrd.img
    rm -f /tmp/test_initrd.img
fi
```
</details>

### Exercise 5: Inspecting Systemd Default Target
**Task**: Query the default systemd target. Verify how to change between graphical and multi-user targets safely.
<details>
<summary>View Solution</summary>

```bash
# Query default boot target
systemctl get-default

# Check available rescue and emergency targets
systemctl list-unit-files | grep -E "rescue|emergency"
```
</details>

### Exercise 6: Verifying Ext4 Filesystem Status with `tune2fs`
**Task**: Inspect the filesystem state, clean status, and mount count of your root partition.
<details>
<summary>View Solution</summary>

```bash
ROOT_DEV=$(df / | awk 'NR==2 {print $1}')
echo "Inspecting device: ${ROOT_DEV}"
sudo tune2fs -l "${ROOT_DEV}" 2>/dev/null | grep -E "Filesystem state|Last checked|Mount count" || echo "Device is not standard ext4"
```
</details>

---

## Summary Cheat Sheet & Further Reading

### Command Reference
| Operation | Command Example |
| :--- | :--- |
| **Remount Root Read-Write** | `mount -o remount,rw /` |
| **Check & Repair Ext4** | `sudo fsck.ext4 -fy /dev/sdX` |
| **Check & Repair XFS** | `sudo xfs_repair /dev/sdX` |
| **Bind Virtual FS for Chroot** | `mount --bind /dev /mnt/dev && mount -t proc proc /mnt/proc && mount -t sysfs sys /mnt/sys` |
| **Enter Chroot** | `chroot /mnt /bin/bash` |
| **Rebuild Initramfs (Debian)**| `sudo update-initramfs -u -k all` |
| **Rebuild Initramfs (RHEL)** | `sudo dracut --regenerate-all --force` |
| **Reinstall GRUB Bootloader**| `sudo grub-install /dev/sdX` |
| **Update GRUB Configuration** | `sudo update-grub` |
| **Trigger SELinux Relabel** | `touch /.autorelabel` |

### Further Reading
- `man 8 fsck`, `man 8 xfs_repair`, and `man 8 grub-install`
- `man 1 chroot` and `man 5 fstab`
- [GNU GRUB Manual 2.12](https://www.gnu.org/software/grub/manual/grub/grub.html)
- [Systemd Boot and Target Administration](https://systemd.io/)
- [Red Hat Enterprise Linux 9: Diagnosing and Resetting Boot Issues](https://access.redhat.com/documentation/en-us/red_hat_enterprise_linux/9/html/managing_storage_devices/troubleshooting-storage-issues)
