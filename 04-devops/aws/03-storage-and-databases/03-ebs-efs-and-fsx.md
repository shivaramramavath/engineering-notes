# Amazon EBS, EFS, and FSx: Block and File Storage Architecture

> **Module**: `03-storage-and-databases`  
> **Topic**: AWS Block, Shared NFS, and High-Performance Parallel File Systems  
> **Target Audience**: Systems Administrators, DevOps Architects, Cloud Infrastructure Engineers  
> **Last Updated**: November 2026

---

## Navigation & Contents

- [1. What & Why](#1-what--why)
- [2. Core Concepts](#2-core-concepts)
- [3. How It Works Internally](#3-how-it-works-internally)
- [4. Syntax & Usage](#4-syntax--usage)
- [5. Practical Examples](#5-practical-examples)
- [6. Best Practices](#6-best-practices)
- [7. Common Mistakes](#7-common-mistakes)
- [8. Performance, Security & Cost Notes](#8-performance-security--cost-notes)
- [9. Comparison Table](#9-comparison-table)
- [10. Interview Questions & Answers](#10-interview-questions--answers)
- [11. Practice Exercises](#11-practice-exercises)
- [12. Summary & Quick Reference](#12-summary--quick-reference)
- [13. Further Reading](#13-further-reading)

---

## 1. What & Why

Operating production workloads in the cloud requires choosing the appropriate storage abstraction:
- Databases, transactional processing systems, and boot volumes require direct, ultra-low-latency **block storage** that behaves like a physical hard drive formatted with an operating system filesystem (EXT4, XFS, NTFS).
- Web clusters, container shared volumes, and development environments require **shared file storage** that supports concurrent read/write operations across hundreds of virtual machines via standard POSIX NFS protocols.
- Machine learning training clusters, seismic analysis, and video rendering require **ultra-high-throughput parallel file systems** capable of delivering hundreds of gigabytes per second with sub-millisecond latencies.

AWS provides three primary services to satisfy these diverse storage paradigms:
1. **Amazon Elastic Block Store (EBS)**: Network-attached virtual block storage volumes designed for single-instance attachment (or multi-attach for clustered file systems).
2. **Amazon Elastic File System (EFS)**: A fully managed, serverless, highly durable POSIX NFS file system that automatically scales up or down to petabytes without provisioning.
3. **Amazon FSx Family**: Fully managed, specialized commercial and open-source file systems: **FSx for Lustre** (high-performance compute and AI), **FSx for Windows File Server** (SMB and Active Directory), **FSx for NetApp ONTAP**, and **FSx for OpenZFS**.

---

## 2. Core Concepts

### 2.1 Amazon EBS Volume Types
EBS volumes are categorized by underlying storage medium (Solid-State Drives vs. Hard Disk Drives):
- **General Purpose SSD (`gp3`)**: Current standard. Decouples storage capacity from performance. Provides a free baseline of **3,000 IOPS** and **125 MB/s** throughput. Up to 16,000 IOPS and 1,000 MB/s. Costs 20% less per GB than legacy `gp2`.
- **Provisioned IOPS SSD (`io2 Block Express`)**: Built for mission-critical I/O-intensive databases (Oracle, SAP HANA, Microsoft SQL Server). Delivers sub-millisecond latencies, up to **256,000 IOPS**, 4,000 MB/s throughput, and 99.999% durability.
- **Throughput Optimized HDD (`st1`)**: Low-cost magnetic storage for frequently accessed, large sequential workloads (MapReduce, Kafka, log processing). Capped at 500 MB/s. Cannot be used as an OS boot volume.
- **Cold HDD (`sc1`)**: Lowest-cost block storage for infrequently accessed sequential workloads. Cannot be used as a boot volume.

### 2.2 Advanced EBS Capabilities
- **Elastic Volumes**: Dynamically increase volume size, adjust provisioned IOPS, or change volume types on the fly without detaching the disk or taking the EC2 instance offline.
- **Multi-Attach**: Allows an `io2` or `io1` volume to be attached simultaneously to up to **16 Nitro-based EC2 instances** within the same Availability Zone. Requires a clustered filesystem (e.g., GFS2) to prevent write collisions.
- **EBS Snapshots & Fast Snapshot Restore (FSR)**: Incremental point-in-time backups stored in Amazon S3. FSR eliminates initial read-latency penalties ("lazy loading") by pre-warming blocks instantly upon restoration.

### 2.3 Amazon EFS Architecture
- **POSIX-Compliant NFS**: Supports standard NFSv4.0 and NFSv4.1 protocols. Mountable simultaneously by thousands of EC2 instances, ECS tasks, and EKS pods across multiple AZs.
- **Storage Classes**:
  - **EFS Standard**: Multi-AZ redundancy across a minimum of three AZs.
  - **EFS One Zone**: Single-AZ deployment for 47% lower storage cost.
  - **EFS Infrequent Access (IA) & Archive**: Automated lifecycle management transitions files unaccessed for 30–90 days to low-cost tiers.
- **Throughput Modes**:
  - **Elastic Throughput (Recommended)**: Automatically scales read/write throughput based on application activity, billing purely for data transferred.
  - **Provisioned Throughput**: Dedicates fixed throughput (MB/s) independent of stored data volume.

### 2.4 Amazon FSx for Lustre
Lustre is an open-source, massively parallel file system designed for High-Performance Computing (HPC):
- Capable of processing hundreds of gigabytes per second of throughput and millions of IOPS with sub-millisecond latencies.
- **Seamless S3 Data Lake Integration**: Can hydrate data directly from an S3 bucket, present it as a POSIX filesystem to GPU instances for AI model training, and sync modified outputs back to S3.

---

## 3. How It Works Internally

```
+-----------------------------------------------------------------------------------+
|                        AWS Block vs. File Storage Architecture                    |
+-----------------------------------------------------------------------------------+
|                                                                                   |
|  [ Amazon EBS: Dedicated Block Storage ]                                          |
|  +-----------------------------------------------------------------------------+  |
|  | EC2 Virtual Machine (Nitro System)                                          |  |
|  | - Direct NVMe Block Driver over PCIe                                        |  |
|  | - Filesystem managed inside guest OS (XFS / EXT4)                           |  |
|  +---------------------------------------|-------------------------------------+  |
|                                          | Dedicated 10-100 Gbps Nitro Bus        |
|                                          v                                        |
|  +-----------------------------------------------------------------------------+  |
|  | Amazon EBS Volume (gp3 / io2) [Tied strictly to single Availability Zone]   |  |
|  | - Synchronously mirrored across multiple servers within the same AZ         |  |
|  | - 99.9% to 99.999% volume durability                                        |  |
|  +-----------------------------------------------------------------------------+  |
|                                                                                   |
|  -------------------------------------------------------------------------------  |
|  [ Amazon EFS: Multi-AZ Shared Network File System ]                              |
|                                                                                   |
|  +------------------+     +------------------+     +------------------+           |
|  | EC2 Instance 1a  |     | ECS Task 1b      |     | EKS Pod 1c       |           |
|  +--------|---------+     +--------|---------+     +--------|---------+           |
|           \                        |                       /                      |
|            \ (Mount Target 1a)     | (Mount Target 1b)    / (Mount Target 1c)     |
|             v                      v                     v                        |
|  +-----------------------------------------------------------------------------+  |
|  | Amazon EFS Distributed Storage Fleet (Multi-AZ Serverless Storage)          |  |
|  | - POSIX NFSv4 Protocol                                                      |  |
|  | - Automatic Elastic Throughput Scaling                                      |  |
|  | - Replicated across 3 Availability Zones (11 9's durability)                |  |
|  +-----------------------------------------------------------------------------+  |
+-----------------------------------------------------------------------------------+
```

---

## 4. Syntax & Usage

### 4.1 Creating and Modifying an EBS GP3 Volume via CLI
```bash
# 1. Create a 100 GB gp3 volume with 5000 IOPS and 250 MB/s throughput in us-east-1a
VOLUME_ID=$(aws ec2 create-volume \
  --availability-zone us-east-1a \
  --volume-type gp3 \
  --size 100 \
  --iops 5000 \
  --throughput 250 \
  --encrypted \
  --tag-specifications 'ResourceType=volume,Tags=[{Key=Name,Value=db-data-volume}]' \
  --query 'VolumeId' \
  --output text)

echo "Created Volume: $VOLUME_ID"

# 2. Attach volume to EC2 instance
aws ec2 attach-volume \
  --volume-id "$VOLUME_ID" \
  --instance-id i-0123456789abcdef0 \
  --device /dev/sdf

# 3. Dynamically expand volume size to 200 GB while running (Elastic Volumes)
aws ec2 modify-volume \
  --volume-id "$VOLUME_ID" \
  --size 200 \
  --iops 6000
```

### 4.2 Creating an Amazon EFS File System with Elastic Throughput
```bash
# 1. Create EFS File System
EFS_ID=$(aws efs create-file-system \
  --performance-mode generalPurpose \
  --throughput-mode elastic \
  --encrypted \
  --tags Key=Name,Value=shared-web-assets \
  --query 'FileSystemId' \
  --output text)

# 2. Create Mount Targets in Private Subnets across AZs
aws efs create-mount-target \
  --file-system-id "$EFS_ID" \
  --subnet-id subnet-01a \
  --security-groups sg-0123456789abcdef0

aws efs create-mount-target \
  --file-system-id "$EFS_ID" \
  --subnet-id subnet-01b \
  --security-groups sg-0123456789abcdef0
```

---

## 5. Practical Examples

### Example 1: Mounting Amazon EFS on Linux via `amazon-efs-utils`
Mounting shared EFS storage securely using TLS encryption and IAM authentication.

```bash
# Install EFS mount helper on Amazon Linux 2023 / Ubuntu
sudo dnf install -y amazon-efs-utils

# Create local mount directory
sudo mkdir -p /mnt/shared-data

# Mount EFS file system enforcing TLS and IAM role authentication
sudo mount -t efs -o tls,iam fs-0123456789abcdef0:/ /mnt/shared-data

# Add to /etc/fstab for automatic remounting upon system reboot
echo "fs-0123456789abcdef0:/ /mnt/shared-data efs _netdev,tls,iam 0 0" | sudo tee -a /etc/fstab
```

### Example 2: Declarative Terraform Configuration for Amazon EFS with Lifecycle Policy
Provision an encrypted multi-AZ EFS file system that automatically moves files to Infrequent Access after 30 days.

```hcl
resource "aws_efs_file_system" "shared_storage" {
  creation_token   = "enterprise-shared-efs"
  performance_mode = "generalPurpose"
  throughput_mode  = "elastic"
  encrypted        = true
  kms_key_id       = var.kms_key_arn

  lifecycle_policy {
    transition_to_ia = "AFTER_30_DAYS"
  }

  lifecycle_policy {
    transition_to_archive = "AFTER_90_DAYS"
  }

  tags = {
    Name        = "prod-shared-efs"
    Environment = "production"
  }
}

resource "aws_efs_mount_target" "zone_a" {
  file_system_id  = aws_efs_file_system.shared_storage.id
  subnet_id       = var.private_subnet_a_id
  security_groups = [var.efs_security_group_id]
}

resource "aws_efs_mount_target" "zone_b" {
  file_system_id  = aws_efs_file_system.shared_storage.id
  subnet_id       = var.private_subnet_b_id
  security_groups = [var.efs_security_group_id]
}
```

### Example 3: Provisioning an Amazon FSx for Lustre Cluster Linked to Amazon S3
Provision a high-performance Lustre cluster that synchronizes input data directly from an S3 data lake bucket.

```hcl
resource "aws_fsx_lustre_file_system" "ai_cluster_storage" {
  storage_capacity            = 2400 # Minimum 2,400 GB for SCRATCH_2
  subnet_ids                  = [var.compute_subnet_id]
  deployment_type             = "PERSISTENT_2"
  per_unit_storage_throughput = 250 # MB/s per TB
  storage_type                = "SSD"

  import_path      = "s3://${var.s3_training_data_bucket}/datasets/"
  export_path      = "s3://${var.s3_training_data_bucket}/outputs/"
  auto_import_policy = "NEW_CHANGED_DELETED"

  tags = {
    Name = "ai-training-lustre-fs"
  }
}
```

### Example 4: Automating EBS Snapshots with Amazon Data Lifecycle Manager (DLM)
Declarative snapshot lifecycle policy taking daily backups, retaining 14 days, and replicating snapshots cross-region for DR.

```json
{
  "ResourceTypes": ["VOLUME"],
  "TargetTags": [
    {"Key": "BackupPlan", "Value": "DailyGold"}
  ],
  "Schedules": [
    {
      "Name": "DailySnapshots",
      "CopyTags": true,
      "CreateRule": {
        "Interval": 24,
        "IntervalUnit": "HOURS",
        "Times": ["03:00"]
      },
      "RetainRule": {
        "Count": 14
      },
      "CrossRegionCopyRules": [
        {
          "TargetRegion": "us-west-2",
          "Encrypted": true,
          "RetainRule": { "Interval": 14, "IntervalUnit": "DAYS" }
        }
      ]
    }
  ]
}
```

Apply via CLI:
```bash
aws dlm create-lifecycle-policy \
  --description "Daily Production EBS Snapshots" \
  --state ENABLED \
  --execution-role-arn "arn:aws:iam::111122223333:role/AWSDataLifecycleManagerDefaultRole" \
  --policy-details file://dlm-policy.json
```

### Example 5: Recovering from EBS Disk Full via Online Partition Expansion
Resize an attached EBS volume from 50 GB to 100 GB and extend the Linux XFS filesystem with zero downtime.

```bash
# 1. Expand EBS volume via AWS CLI
aws ec2 modify-volume --volume-id vol-0123456789abcdef0 --size 100

# 2. Inside the running Linux instance, inspect block devices
lsblk
# Output shows nvme1n1 is 100G, but nvme1n1p1 is still 50G

# 3. Grow the disk partition
sudo growpart /dev/nvme1n1 1

# 4. Expand the filesystem (online operation, zero downtime)
# For XFS filesystem:
sudo xfs_growfs -d /data
# For EXT4 filesystem:
# sudo resize2fs /dev/nvme1n1p1

# 5. Verify new size
df -h /data
```

---

## 6. Best Practices

1. **Migrate Legacy GP2 to GP3 Immediately**: Replacing `gp2` with `gp3` saves an immediate 20% on storage costs while unlocking 3,000 baseline IOPS and 125 MB/s throughput independently of volume size.
2. **Enable Account-Level EBS Encryption by Default**: Enable account-level default EBS encryption in every active AWS Region. This guarantees every newly created volume and snapshot is encrypted with KMS without relying on engineer discipline.
3. **Use Elastic Throughput for Amazon EFS**: Avoid hardcoding provisioned throughput on EFS unless application throughput is constant 24/7. Elastic Throughput automatically handles sudden spikes and scales down to zero when idle.
4. **Use `amazon-efs-utils` with TLS and IAM**: When mounting EFS, always use the `amazon-efs-utils` helper with the `tls` and `iam` mount options to encrypt data in transit and eliminate static IP dependencies.
5. **Implement EBS Fast Snapshot Restore (FSR) for Fast Recovery**: When restoring large EBS volumes from snapshots for disaster recovery, enable Fast Snapshot Restore. FSR eliminates the "lazy loading" penalty where blocks must be downloaded from S3 on initial read.

---

## 7. Common Mistakes

| Mistake | Consequence | Correct Pattern |
|:---|:---|:---|
| **Using EBS Multi-Attach with Standard Filesystems** | Formatting a multi-attached EBS volume with standard EXT4/XFS results in catastrophic data corruption. | Multi-Attach requires a cluster-aware shared filesystem (e.g., GFS2, OCFS2) or read-only access. |
| **Attempting Cross-AZ EBS Attachment** | AWS CLI / console returns an invalid parameter error; EBS volumes are strictly confined to a single AZ. | Use Amazon EFS for multi-AZ shared access, or snapshot and recreate the EBS volume in the target AZ. |
| **Over-Allocating GP2 Volumes for IOPS** | Purchasing 1,000 GB disks just to obtain 3,000 IOPS on legacy gp2, wasting 80% of budget. | Use `gp3` where storage, IOPS, and throughput are provisioned and billed independently. |
| **Running High-IOPS Relational DBs on EFS** | High latency and poor performance; EFS is optimized for distributed file sharing, not single-digit microsecond random I/O. | Run databases (PostgreSQL, MySQL) on Amazon EBS `gp3` / `io2` or native Amazon RDS / Aurora. |
| **Forgetting to Grow the Filesystem after Modifying EBS** | Modifying EBS volume size in AWS increases the block device, but `df -h` shows no increase until the OS partition expands. | Run `growpart` followed by `xfs_growfs` or `resize2fs` inside the guest OS. |
| **Mounting EFS across Regions** | Massive inter-region network latency (50–100ms) degrades POSIX file operations significantly. | Mount EFS strictly from instances residing in the same AWS Region via local Mount Targets. |

---

## 8. Performance, Security & Cost Notes

### Performance Profiles
- **Amazon EBS (`io2 Block Express`)**: Up to 256,000 IOPS, 4,000 MB/s throughput, sub-millisecond latency. Pinned to a single AZ.
- **Amazon EFS**: Single-digit millisecond latency (typically 2–5 ms). Scales to 10+ GB/s throughput and 500,000+ IOPS across multiple AZs.
- **Amazon FSx for Lustre**: Sub-millisecond latency. Hundreds of GB/s throughput and millions of IOPS for distributed parallel compute.

### Security Enforcements
- **EBS KMS Envelope Encryption**: Encryption occurs on the host hardware (AWS Nitro Card) before blocks traverse the network to EBS servers. Snapshots created from encrypted volumes are automatically encrypted.
- **EFS Access Points**: Application-specific entry points into an EFS file system that enforce a specific POSIX user identity (`uid`/`gid`) and restrict access to a specific sub-directory, essential for multi-tenant container hosting.

### Cost Breakdown
- **EBS GP3**: $0.08 / GB / month ($0.005 per provisioned IOPS over 3k; $0.04 per provisioned MB/s over 125).
- **Amazon EFS Standard**: $0.30 / GB / month.
- **Amazon EFS Infrequent Access (IA)**: $0.013 / GB / month (+ $0.01/GB access fee).
- **FSx for Lustre**: Starting at ~$0.14 / GB / month for persistent SSD tiers.

---

## 9. Comparison Table

| Attribute | Amazon EBS (gp3/io2) | Amazon EFS | Amazon FSx for Lustre |
|:---|:---|:---|:---|
| **Storage Architecture** | Block Storage (Virtual Disk) | Shared File Storage (POSIX NFS) | Parallel File Storage (Lustre) |
| **Access Model** | Single Instance (or Multi-Attach)| Thousands of instances/pods | Thousands of parallel compute nodes|
| **Availability Scope** | Single Availability Zone | Multi-AZ (3 AZs) or One Zone | Single AZ or Multi-AZ |
| **Latency Profile** | Sub-millisecond (io2) / Low ms | 2–5 milliseconds | Sub-millisecond |
| **Throughput Scaling** | Up to 4,000 MB/s per volume | Elastic (up to 10+ GB/s) | Hundreds of GB/s (Hundreds of TB)|
| **Native S3 Sync** | None (Backup via Snapshots) | AWS DataSync | Bi-directional native S3 sync |

---

## 10. Interview Questions & Answers

<details>
<summary><strong>Q1: What are EBS Elastic Volumes and what modifications can be made without downtime?</strong></summary>

**Answer:**
Amazon EBS Elastic Volumes allow engineers to dynamically modify volume attributes on live, attached EBS volumes without detaching the disk or stopping the EC2 instance.
Supported online modifications:
1. **Increase Volume Size**: Expand disk capacity (e.g., from 50 GB to 200 GB). Disks cannot be shrunk.
2. **Change Volume Type**: Convert between types (e.g., migrating an active volume from `gp2` to `gp3`, or from `gp3` to `io2`).
3. **Adjust Performance**: Dynamically provision additional IOPS or throughput (e.g., bumping a database disk from 3,000 IOPS to 10,000 IOPS during peak sales periods).
After the AWS volume modification completes, the OS partition and filesystem must be grown online using tools like `growpart` and `xfs_growfs`.
</details>

<details>
<summary><strong>Q2: Why does formatting an EBS Multi-Attach volume with EXT4 lead to filesystem corruption?</strong></summary>

**Answer:**
Standard Linux filesystems like EXT4 and XFS are **non-distributed, single-node filesystems**. They assume that a single operating system kernel has exclusive control over the disk's block allocation tables, inode maps, and write caches in memory.

If two separate EC2 instances mount the same block device with EXT4, both instances maintain separate, uncoordinated memory caches of filesystem metadata. When Instance A writes to a block, Instance B is unaware of the mutation and may overwrite that same block with its own cached data, rapidly corrupting the filesystem. Multi-Attach requires **cluster-aware shared filesystems** (such as GFS2 or Oracle OCFS2) that implement distributed lock managers (DLM) to coordinate concurrent writes safely.
</details>

<details>
<summary><strong>Q3: When should an architect choose Amazon EFS over Amazon EBS?</strong></summary>

**Answer:**
- Choose **Amazon EBS** when the workload requires ultra-low latency (sub-millisecond), dedicated high IOPS, single-instance attachment, and block-level filesystem control (relational databases, operating system boot drives, heavy caches).
- Choose **Amazon EFS** when multiple instances, containers, or pods across different Availability Zones require concurrent read/write access to a shared directory tree (WordPress uploads, CI/CD shared workspaces, machine learning training data access across multiple worker nodes, enterprise content management).
</details>

<details>
<summary><strong>Q4: How does EBS Fast Snapshot Restore (FSR) eliminate the "lazy loading" performance penalty?</strong></summary>

**Answer:**
Standard EBS snapshots are backed up incrementally to Amazon S3. When a new EBS volume is created from a snapshot, the volume is immediately available for attachment, but the underlying disk blocks are not physically copied yet. When the OS reads a block for the first time, EBS must fetch the block from S3, causing a significant initial read-latency spike ("lazy loading").

**Fast Snapshot Restore (FSR)** eliminates this by pre-warming blocks in advance. When enabled on an Availability Zone, FSR creates a fully hydrated, instantly performant copy of the snapshot in that AZ. Volumes launched from an FSR-enabled snapshot deliver maximum provisioned IOPS and throughput on the very first read operation with zero retrieval latency.
</details>

<details>
<summary><strong>Q5: What is the architectural difference between EFS Elastic Throughput and Provisioned Throughput?</strong></summary>

**Answer:**
- **Provisioned Throughput**: Dedicates a static throughput limit (e.g., 200 MB/s) independent of the amount of data stored in the file system. You pay a monthly fee for the provisioned throughput even if the file system is completely idle. Ideal for predictable 24/7 high-throughput streaming.
- **Elastic Throughput**: Fully serverless and dynamic. It automatically scales throughput up to several gigabytes per second based on real-time application read/write activity. You pay exclusively for the amount of data read and written ($0.03/GB read, $0.06/GB written), making it significantly more cost-effective for spiky, unpredictable workloads.
</details>

<details>
<summary><strong>Q6: How does Amazon FSx for Lustre integrate with Amazon S3?</strong></summary>

**Answer:**
Amazon FSx for Lustre can link directly to an Amazon S3 bucket via an **import/export data repository association**:
- Upon cluster creation, FSx imports the S3 bucket's object metadata, presenting the objects as standard files and directories in the Lustre filesystem without copying the file contents upfront.
- When compute nodes read a file, Lustre streams the raw data from S3 lazily into high-speed SSD storage.
- High-Performance Computing (HPC) or AI/ML workloads execute calculations at hundreds of GB/s throughput.
- Modified files or new output datasets can be automatically synchronized back to the S3 bucket using FSx data repository tasks.
</details>

<details>
<summary><strong>Q7: How do EFS Access Points enhance security in multi-tenant container environments?</strong></summary>

**Answer:**
In shared environments (like Amazon ECS or EKS), multiple container tasks share access to a single EFS file system:
- **Enforced Root Directory**: An EFS Access Point restricts a container's view to a specific sub-directory (e.g., `/app1/data`), preventing containers from traversing the root directory or accessing other tenants' data.
- **POSIX Identity Enforcement**: It overrides the container's internal Linux user identity and forces all filesystem operations to execute under a predefined POSIX user ID (`uid`) and group ID (`gid`), neutralizing container privilege escalation attacks.
</details>

<details>
<summary><strong>Q8: What happens to data stored on an EBS volume when an EC2 instance is terminated?</strong></summary>

**Answer:**
Data persistence depends on the **`DeleteOnTermination`** attribute of the block device mapping:
- By default, the root EBS volume has `DeleteOnTermination = true`, meaning the root volume is automatically deleted when the instance terminates.
- Secondary data volumes (`/dev/sdb`, `/dev/sdf`) default to `DeleteOnTermination = false`, preserving the volume and all stored data independently in the AWS account.
- To prevent accidental loss of root volumes, `DeleteOnTermination` can be set to `false` via the AWS CLI, Launch Templates, or Terraform.
</details>

---

## 11. Practice Exercises

<details>
<summary><strong>Exercise 1: Create an Encrypted EBS GP3 Volume via CLI</strong></summary>

**Objective**: Provision a 50GB gp3 volume and tag it.

**Command**:
```bash
aws ec2 create-volume \
  --availability-zone us-east-1a \
  --volume-type gp3 \
  --size 50 \
  --iops 3000 \
  --throughput 125 \
  --encrypted \
  --tag-specifications 'ResourceType=volume,Tags=[{Key=Environment,Value=Test}]'
```
</details>

<details>
<summary><strong>Exercise 2: Dynamically Modify EBS Volume Throughput</strong></summary>

**Objective**: Bumping throughput on an active volume from 125 MB/s to 250 MB/s.

**Command**:
```bash
aws ec2 modify-volume \
  --volume-id vol-0123456789abcdef0 \
  --throughput 250
```
</details>

<details>
<summary><strong>Exercise 3: Create an EFS File System with Elastic Throughput</strong></summary>

**Objective**: Provision a serverless EFS file system.

**Command**:
```bash
aws efs create-file-system \
  --performance-mode generalPurpose \
  --throughput-mode elastic \
  --encrypted \
  --tags Key=Name,Value=web-shared-efs
```
</details>

<details>
<summary><strong>Exercise 4: Create an EFS Access Point for Container Isolation</strong></summary>

**Objective**: Enforce sub-directory rooting and POSIX UID 1001.

**Command**:
```bash
aws efs create-access-point \
  --file-system-id fs-0123456789abcdef0 \
  --posix-user Uid=1001,Gid=1001 \
  --root-directory '{"Path": "/app-data", "CreationInfo": {"OwnerUid": 1001, "OwnerGid": 1001, "Permissions": "0755"}}'
```
</details>

<details>
<summary><strong>Exercise 5: Mount EFS Securely via `amazon-efs-utils`</strong></summary>

**Objective**: Execute an in-transit encrypted mount command.

**Command**:
```bash
sudo mkdir -p /mnt/efs-test
sudo mount -t efs -o tls,accesspoint=fsap-0123456789abcdef0 fs-0123456789abcdef0:/ /mnt/efs-test
```
</details>

<details>
<summary><strong>Exercise 6: Expand an Online XFS Partition on EBS</strong></summary>

**Objective**: Practice growing an operating system partition after an EBS resize.

**Steps**:
1. Check disk blocks: `lsblk`.
2. Expand partition: `sudo growpart /dev/nvme1n1 1`.
3. Grow filesystem: `sudo xfs_growfs -d /mnt/data`.
4. Confirm capacity: `df -h /mnt/data`.
</details>

---

## 12. Summary & Quick Reference

```
+---------------------------------------------------------------------------------------+
|                                AWS Storage Decision Matrix                            |
+---------------------------------------------------------------------------------------+
|  Service               | Primary Pattern                   | Key Characteristic       |
|------------------------+-----------------------------------+--------------------------|
|  EBS GP3               | EC2 OS & App Data (Single AZ)     | Decoupled IOPS/Throughput|
|  EBS IO2 Block Express | Mission-critical databases        | Sub-ms, 256k IOPS, 99.999%|
|  Amazon EFS            | Multi-AZ shared POSIX NFS         | Serverless elastic scale |
|  FSx for Lustre        | High-Performance Compute & AI/ML  | Hundreds of GB/s, S3 sync|
|  FSx for Windows       | Native SMB / Active Directory     | Windows enterprise apps  |
+---------------------------------------------------------------------------------------+
```

---

## 13. Further Reading

- [Amazon EBS User Guide](https://docs.aws.amazon.com/AWSEC2/latest/UserGuide/AmazonEBS.html)
- [Amazon EFS User Guide](https://docs.aws.amazon.com/efs/latest/ug/what-is.html)
- [Amazon FSx for Lustre User Guide](https://docs.aws.amazon.com/fsx/latest/LustreGuide/what-is.html)
- [Deep Dive on Amazon EBS GP3 Performance](https://aws.amazon.com/blogs/storage/migrating-your-amazon-ebs-volumes-from-gp2-to-gp3-and-saving-up-to-20-on-costs/)
- [Amazon EFS Performance and Elastic Throughput Guide](https://aws.amazon.com/blogs/aws/announcing-amazon-efs-elastic-throughput/)
