# Storage and Filesystems

"Disk full" is one of the most common production incidents, and also one of the most misdiagnosed: space can be consumed by files, by deleted-but-open files, or by inodes. This note covers how Linux presents storage (block devices, filesystems, mounts), how to measure usage correctly, how to add and persist a disk, and how LVM lets you grow volumes without downtime.

Prerequisites: [Filesystem and Navigation](../01-fundamentals/02-filesystem-and-navigation.md) (the single directory tree) and [Permissions](../01-fundamentals/05-permissions.md).

## Layers

```text
Physical/virtual disk         /dev/sda, /dev/nvme0n1, /dev/vda
   └─ Partition (optional)    /dev/sda1
        └─ (LVM, RAID, or encryption layer, optional)
             └─ Filesystem     ext4, XFS, btrfs
                  └─ Mounted at a directory   /, /var, /data
```

A **block device** is raw storage. A **filesystem** organizes it into files and directories. A **mount** attaches a filesystem to a directory in the single tree. Nothing is usable until it has a filesystem and a mount point.

Common device names: `sdX` (SATA/SCSI/USB), `nvmeXnY` with partitions `nvmeXnYpZ`, `vdX` (many virtualized/cloud disks). Names can change between boots, so persistent configuration uses **UUIDs**.

## Seeing what you have

```bash
lsblk                     # tree of disks, partitions, sizes, mount points
lsblk -f                  # plus filesystem type, label, UUID
sudo blkid                # UUIDs and types
findmnt                   # tree of current mounts
findmnt /var              # which filesystem holds /var
df -hT                    # usage per mounted filesystem, with type
```

## Measuring usage: df vs du

```bash
df -h                          # free space per filesystem
df -h /var                     # filesystem containing /var
du -sh /var/log                # total size of one directory
du -h --max-depth=1 /var 2>/dev/null | sort -h    # who is big under /var
sudo du -xh --max-depth=1 / 2>/dev/null | sort -h # -x stays on one filesystem
```

`ncdu` (`sudo apt install ncdu`) is an interactive du and the fastest way to hunt down space.

**`df` and `du` can disagree**, and that's a clue, not a bug:

| `df` says full, `du` finds less | Likely cause |
|---|---|
| A **deleted file is still open** by a process | space isn't freed until the process closes it |
| Data is hidden under a **mount point** | files written to `/data` *before* a disk was mounted over it still consume the parent filesystem |
| **Reserved blocks** | ext4 keeps ~5% for root by default (adjustable with `tune2fs -m`) |
| Another mount is involved | `du` on `/` with `-x` skips other filesystems |

Find deleted-but-open files:

```bash
sudo lsof +L1                        # open files with link count 0
sudo lsof -nP | grep '(deleted)'
```

Then restart the owning process (or truncate the file via `/proc/<pid>/fd/<n>`). Log files deleted with `rm` while the app is still writing them are the classic cause. Rotate or truncate (`: > file`) instead.

### Inodes

An **inode** stores a file's metadata (owner, permissions, timestamps, pointers to data). A filesystem has a fixed number of them, set when it's created. Millions of tiny files (session files, mail queues, caches) can use up all inodes while plenty of space remains, causing "No space left on device" even though `df -h` looks fine.

```bash
df -i                     # inode usage; IUse% near 100% is the problem
```

Find the culprit directory:

```bash
sudo find /var -xdev -type f | cut -d/ -f1-4 | sort | uniq -c | sort -nr | head
```

Fix by deleting or rotating the files, and fix the application that creates them.

## Filesystems you'll meet

| Type | Notes |
|---|---|
| **ext4** | Ubuntu/Debian default; mature, general purpose; can grow online and shrink offline |
| **XFS** | Red Hat family default; great with large files and parallel I/O; can grow, **cannot shrink** |
| **btrfs** | snapshots, checksums; default on some distros |
| **tmpfs** | lives in RAM (`/run`, `/dev/shm`); lost at reboot |
| **overlayfs** | layered filesystem used by Docker images |
| **NFS** | network filesystem; a hung server can leave processes stuck in `D` state |

## Adding and mounting a new disk

Say a new disk shows up as `/dev/sdb` (check with `lsblk` first).

> **Double-check the device name.** Creating a filesystem destroys whatever was on it. Confirm with `lsblk` that the disk is empty and is the one you mean.

```bash
# 1. partition (GPT, one partition using the whole disk)
sudo parted /dev/sdb --script mklabel gpt mkpart primary ext4 0% 100%

# 2. create the filesystem
sudo mkfs.ext4 /dev/sdb1

# 3. mount it
sudo mkdir -p /data
sudo mount /dev/sdb1 /data

# 4. find its UUID
sudo blkid /dev/sdb1
```

(For a whole disk used as a single filesystem you can skip partitioning, but a partition table makes the disk's purpose obvious to other tools.)

Mounts made with `mount` disappear on reboot. To persist, add a line to `/etc/fstab`:

```text
# <device>                                 <mount point> <type> <options>        <dump> <fsck>
UUID=3f1c0a9e-1111-2222-3333-444455556666   /data         ext4   defaults,nofail   0      2
```

Fields: what to mount, where, filesystem type, mount options, `dump` (legacy, use 0), and `fsck` pass order (`1` for root, `2` for others, `0` to skip).

**Test before rebooting**, because a bad `fstab` can drop the machine into emergency mode at boot:

```bash
sudo umount /data
sudo mount -a             # mounts everything in fstab; errors show up now
findmnt --verify          # sanity-check fstab syntax
df -h /data
```

Use `nofail` for non-critical data disks so a missing disk doesn't block boot. Use UUIDs, not `/dev/sdb1`, since device names can shuffle.

After mounting, fix ownership of the new filesystem's root so your app can write: `sudo chown myapp:myapp /data` ([permissions](../01-fundamentals/05-permissions.md)).

### Useful mount options

| Option | Effect |
|---|---|
| `ro` | read-only |
| `noexec` | can't run binaries from it (hardening for `/tmp`, data disks) |
| `nosuid`, `nodev` | ignore setuid bits / device files |
| `noatime` | skip updating access times; less write I/O |
| `nofail` | boot continues if the device is absent |

If a process gets "Permission denied" or "Read-only file system" despite correct permissions, check `findmnt -no OPTIONS /path`. A filesystem remounts itself read-only after serious errors, which also shows up in `dmesg`.

### Unmounting

```bash
sudo umount /data
```

`target is busy` means a process still uses it (open file or a shell whose current directory is inside it):

```bash
sudo lsof +f -- /data
sudo fuser -vm /data
```

Stop or move those processes, then unmount. `umount -l` (lazy) detaches the mount point but leaves processes using it, so use it only when you know why.

## LVM basics

LVM (Logical Volume Manager) adds a flexible layer between disks and filesystems so you can resize, span, and snapshot storage.

```text
Physical Volumes (PV)  →  Volume Group (VG)  →  Logical Volumes (LV)
   /dev/sdb, /dev/sdc       vg0 (the pool)         /dev/vg0/data, /dev/vg0/logs
                                                       └─ filesystem on each LV
```

```bash
# inspect
sudo pvs; sudo vgs; sudo lvs           # concise summaries (pvdisplay/vgdisplay/lvdisplay for detail)

# build
sudo pvcreate /dev/sdb
sudo vgcreate vg0 /dev/sdb
sudo lvcreate -L 20G -n data vg0       # or: -l 100%FREE
sudo mkfs.ext4 /dev/vg0/data
sudo mount /dev/vg0/data /data         # in fstab, use the UUID or /dev/mapper/vg0-data

# grow later, online
sudo vgextend vg0 /dev/sdc             # add another disk to the pool
sudo lvextend -r -L +10G /dev/vg0/data # -r also resizes the filesystem
```

`lvextend -r` grows both the LV and the filesystem in one step (ext4 via `resize2fs`, XFS via `xfs_growfs`). Without `-r` you must resize the filesystem yourself, and a bigger LV with an unchanged filesystem gains nothing.

A very common situation on **Ubuntu Server installed with LVM**: the root logical volume is created smaller than the volume group, so the disk looks bigger than `df` shows.

```bash
sudo vgs                                  # VFree shows unallocated space
sudo lvextend -r -l +100%FREE /dev/ubuntu-vg/ubuntu-lv   # use `lvs` to confirm the real names
```

### Growing a cloud disk

If you enlarge the volume in your cloud console, the OS still needs to be told:

```bash
lsblk                                  # disk is bigger, partition is not
sudo growpart /dev/vda 1               # (cloud-guest-utils) grow partition 1
sudo resize2fs /dev/vda1               # ext4; for XFS: sudo xfs_growfs /
# with LVM: growpart, then pvresize /dev/vda3, then lvextend -r ...
```

Shrinking is riskier: ext4 must be unmounted, XFS can't shrink. Take a backup first ([backups](./04-cron-and-timers.md)).

## Checking and repairing

- `fsck` repairs a filesystem and must be run on an **unmounted** (or read-only) one; on a mounted filesystem it can cause damage.
- Look for I/O errors in `dmesg -T | grep -i -E "error|i/o"`.
- A disk that's slow or failing shows up in `iostat`/`iotop` ([performance](../05-production/03-performance.md)); SMART data comes from `smartctl` (package `smartmontools`) on physical disks.

## Common mistakes

- **Formatting the wrong device** (`mkfs` on a disk that holds data). Verify with `lsblk -f` first.
- **`fstab` entry that fails** and no `nofail`, causing boot into emergency mode. Always `mount -a` before reboot.
- **Using `/dev/sdX` names in fstab** instead of UUIDs.
- **Reading only `df -h`** when the real problem is inodes (`df -i`) or deleted-but-open files (`lsof +L1`).
- **Mounting over a non-empty directory**, hiding existing files.
- **Extending an LV but not the filesystem.**
- **Deleting logs that are still being written** instead of rotating/truncating.
- **Forgetting Docker's footprint**: `docker system df` shows images, containers, and volumes under `/var/lib/docker`.
- **Running fsck on a mounted filesystem.**

## Quick Summary

- Disk → (partition) → (LVM) → filesystem → mount point; check with `lsblk -f`, `findmnt`, `df -hT`.
- Diagnose "disk full" with `df -h`, `df -i`, `du`/`ncdu`, and `lsof +L1`.
- New disk: partition, `mkfs`, `mount`, UUID in `/etc/fstab` with `nofail`, then **`mount -a` to test**.
- LVM = PV → VG → LV; `lvextend -r` grows the volume and its filesystem online.
- XFS grows but doesn't shrink; ext4 shrinks only offline.
- Back up before any resize or repair.

**Next:** [Networking Basics](../04-networking/01-networking-basics.md)
