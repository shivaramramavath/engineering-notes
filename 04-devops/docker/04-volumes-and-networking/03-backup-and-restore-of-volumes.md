# Volume Backup, Restore, and Disaster Recovery Strategies

```
Track: DevOps & Containerization
Module: 04-volumes-and-networking
Prerequisites: 04-volumes-and-networking/01-volumes-and-bind-mounts.md
Difficulty: Intermediate to Advanced
Estimated Duration: 40 minutes
Target Docker Version: 27.x+
Last Validated: February 2025
```

---

## Table of Contents
1. [What & Why](#1-what--why)
2. [Core Concepts](#2-core-concepts)
3. [How It Works Internally](#3-how-it-works-internally)
4. [Syntax & Command Reference](#4-syntax--command-reference)
5. [Practical Implementation & Examples](#5-practical-implementation--examples)
6. [Production Best Practices & Antipatterns](#6-production-best-practices--antipatterns)
7. [Common Pitfalls & Troubleshooting](#7-common-pitfalls--troubleshooting)
8. [Performance Tuning & Resource Optimization](#8-performance-tuning--resource-optimization)
9. [Security Hardening & Threat Model](#9-security-hardening--threat-model)
10. [Architectural Comparison Table](#10-architectural-comparison-table)
11. [Real-World Scenario Walkthrough](#11-real-world-scenario-walkthrough)
12. [Self-Assessment & Interview Questions](#12-self-assessment--interview-questions)
13. [Hands-on Practice Exercises](#13-hands-on-practice-exercises)
14. [Summary & Reference Cheat Sheet](#summary--reference-cheat-sheet)

---

## 1. What & Why

Docker volumes manage persistent state outside the container lifecycle. However, Docker provides **no native CLI command** like `docker volume backup` or `docker volume snapshot`.

Without robust backup pipelines, organizations risk total data loss from:
- Hardware disk failure or host cloud instance termination.
- Accidental execution of `docker volume prune -a` or `docker compose down -v`.
- Ransomware infection or database corruption.
- Inability to migrate stateful services between bare-metal servers or cloud providers.

This guide details enterprise patterns for snapshotting, archiving, restoring, and migrating Docker volumes using portable ephemeral container pipelines, logical dumps, and automated backup sidecars.

---

## 2. Core Concepts

### The Ephemeral Sidecar Pattern
Because Docker volumes live inside `/var/lib/docker/volumes/` (which is protected and restricted to the root user), the standard, portable method to manipulate volume data is by launching a temporary **throwaway sidecar container** (`alpine:3.21`) that:
1. Mounts the target Docker volume into `/volume` as **read-only** (`:ro`).
2. Mounts a host backup target directory into `/backup`.
3. Executes a compression utility (`tar`, `zstd`, `gzip`) to create an archive.
4. Exits and automatically self-destructs (`--rm`).

### Logical vs Physical (Block/File) Backups
- **Physical Volume Backup (Filesystem Level)**: Compressing the raw files on disk. Extremely fast, but if performed while a database is actively writing, can result in corrupted transaction log headers. Requires database **quiescing** (freezing writes) or stopping the container.
- **Logical Backup (Application Level)**: Using database-native tools (`pg_dump`, `mysqldump`, `mongodump`) to extract transactional SQL/BSON commands. Safe for active databases with zero downtime.

---

## 3. How It Works Internally

### Ephemeral Container Backup Architecture

```
+-------------------------------------------------------------------------+
|                               HOST MACHINE                              |
|                                                                         |
|  +-------------------------------------------------------------------+  |
|  | Named Volume: prod_db_data                                        |  |
|  | (/var/lib/docker/volumes/prod_db_data/_data)                      |  |
|  +---------------------------------+---------------------------------+  |
|                                    |                                    |
|                                    | Read-Only Mount (:ro)              |
|                                    v                                    |
|  +-------------------------------------------------------------------+  |
|  | Ephemeral Container: docker run --rm alpine                       |  |
|  |                                                                   |  |
|  |  tar czf /backup/db-2025.tar.gz -C /volume .                      |  |
|  +---------------------------------+---------------------------------+  |
|                                    |                                    |
|                                    | Writes compressed archive          |
|                                    v                                    |
|  +-------------------------------------------------------------------+  |
|  | Host Backup Directory: /var/backups/docker/                       |  |
|  | File: db-2025.tar.gz                                              |  |
|  +-------------------------------------------------------------------+  |
+-------------------------------------------------------------------------+
```

---

## 4. Syntax & Command Reference

### Physical Volume Backup Command (One-Liner)

```bash
docker run --rm \
  --mount type=volume,source=<VOLUME_NAME>,target=/volume,readonly \
  --mount type=bind,source="<HOST_BACKUP_DIR>",target=/backup \
  alpine:3.21 \
  tar czf /backup/<BACKUP_NAME>.tar.gz -C /volume .
```

### Physical Volume Restore Command (One-Liner)

```bash
# 1. Create target volume
docker volume create <NEW_VOLUME_NAME>

# 2. Extract backup archive into new volume
docker run --rm \
  --mount type=volume,source=<NEW_VOLUME_NAME>,target=/volume \
  --mount type=bind,source="<HOST_BACKUP_DIR>",target=/backup \
  alpine:3.21 \
  tar xzf /backup/<BACKUP_NAME>.tar.gz -C /volume
```

---

## 5. Practical Implementation & Examples

### Example 1: Full Backup of a Production PostgreSQL Volume
Safely pausing database writes and creating a tar.gz archive.

```bash
# 1. Pause container I/O to ensure transaction consistency
docker pause pg-prod-database

# 2. Execute ephemeral backup container
docker run --rm \
  --mount type=volume,source=pg_data_vol,target=/source,readonly \
  --mount type=bind,source="$(pwd)",target=/backup \
  alpine:3.21 \
  tar -czf /backup/pg_data_backup_$(date +%Y%m%d_%H%M%S).tar.gz -C /source .

# 3. Unpause container immediately
docker unpause pg-prod-database

# Verify backup was created
ls -lh pg_data_backup_*.tar.gz
```

### Example 2: Non-Disruptive Zero-Downtime Logical Database Dump
For production databases where taking the container offline or pausing is unacceptable, execute logical streaming dumps.

**PostgreSQL Hot Backup:**
```bash
docker exec -t pg-prod-database \
  pg_dump -U postgres -d enterprise_db -F c \
  > /var/backups/enterprise_db_$(date +%Y%m%d).dump
```

**MySQL / MariaDB Hot Backup:**
```bash
docker exec -t mysql-prod-database \
  mysqldump -u root -p"$MYSQL_ROOT_PASSWORD" --single-transaction --all-databases \
  | gzip > /var/backups/mysql_all_$(date +%Y%m%d).sql.gz
```

### Example 3: Disaster Recovery: Restoring to a Fresh Docker Volume
Restoring the archive to a brand-new volume and validating data integrity.

```bash
# 1. Create new recovery volume
docker volume create pg_data_recovered

# 2. Restore data from tar archive
docker run --rm \
  --mount type=volume,source=pg_data_recovered,target=/target \
  --mount type=bind,source="$(pwd)",target=/backup \
  alpine:3.21 \
  tar -xzf /backup/pg_data_backup_20250225.tar.gz -C /target

# 3. Launch database attached to recovered volume
docker run -d \
  --name pg-recovered \
  -e POSTGRES_PASSWORD=supersecret \
  --mount type=volume,source=pg_data_recovered,target=/var/lib/postgresql/data \
  postgres:17-alpine

# 4. Validate query
docker exec -i pg-recovered psql -U postgres -c "SELECT count(*) FROM users;"
```

### Example 4: Host-to-Host Live Volume Migration over SSH
Streaming an entire Docker volume directly from `host-alpha` to `host-beta` through an encrypted SSH tunnel without creating temporary local files.

```bash
# On Host Alpha: Stream volume to remote Host Beta over SSH
docker run --rm \
  --mount type=volume,source=my_app_vol,target=/data,readonly \
  alpine:3.21 tar -cf - -C /data . \
| ssh user@host-beta.internal \
  "docker run --rm -i --mount type=volume,source=my_app_vol,target=/data alpine:3.21 tar -xf - -C /data"
```

### Example 5: Automated Nightly Volume Backup via Cron Sidecar
A Docker Compose pattern where an automated backup container runs scheduled backups directly to AWS S3 or a mounted storage volume using `restic`.

```yaml
services:
  app:
    image: my-app:latest
    volumes:
      - app_data:/var/app/data

  backup-agent:
    image: restic/restic:0.17.3
    environment:
      - RESTIC_REPOSITORY=s3:s3.amazonaws.com/my-corp-backups/docker-volumes
      - AWS_ACCESS_KEY_ID=${AWS_ACCESS_KEY_ID}
      - AWS_SECRET_ACCESS_KEY=${AWS_SECRET_ACCESS_KEY}
      - RESTIC_PASSWORD=${RESTIC_PASSWORD}
    volumes:
      - app_data:/data:ro
    command: ["backup", "/data", "--tag", "app-state"]
    restart: "no"

volumes:
  app_data:
```

---

## 6. Production Best Practices & Antipatterns

### Best Practices
- **Always Mount Volumes Read-Only During Backup**: Mount with `readonly` (`:ro`) to guarantee that the backup container cannot accidentally delete or corrupt live production state.
- **Never Perform Raw Physical Backups on Active Unquiesced Databases**: Unquiesced raw file backups can capture split-brain transaction logs. Always execute `docker pause` prior to tarring, or use logical dump tools (`pg_dump`).
- **Enforce Backup Encryption and Offsite Replication**: Store archives encrypted (e.g., via GPG or Restic) and replicate off-host to cloud object storage (AWS S3, Google Cloud Storage).
- **Automate Recovery Drills (RTO/RPO Validation)**: A backup that has never been tested in a recovery dry-run is not a verified backup.
- **Set Retention Policies**: Configure automated cleanup (e.g., retain daily backups for 7 days, weekly for 4 weeks, monthly for 12 months).

### Anti-Patterns
- **Copying `/var/lib/docker/volumes` directly via `cp -r`**: Bypasses filesystem flush caches, changes ownership UIDs, and causes file locking conflicts with the running daemon.
- **Storing Backups on the Same Physical Disk**: If the host NVMe or SSD controller dies, both live data and backup archives perish together.
- **Running `docker compose down -v` in CI/CD without checking**: The `-v` flag deletes all volumes declared in the compose file!

---

## 7. Common Pitfalls & Troubleshooting

| Symptom | Root Cause | Diagnosis Command | Permanent Fix |
|:---|:---|:---|:---|
| Restored database fails to boot with `Permission Denied` | Tar extraction set file owner to `root` instead of the database user UID (e.g. `postgres: 999`) | `docker exec <db> ls -la /var/lib/postgresql/data` | Extract with `tar --same-owner` or run `chown -R 999:999` in recovery container. |
| Database reports `database page corruption` after restore | Backup was taken using raw `tar` while heavy writes were actively occurring | Inspect database startup error log | Use `docker pause`/`unpause` around physical tar, or use `pg_dump`. |
| Host disk fills up during backup generation | Backup tarball was written inside container's ephemeral writable layer instead of host bind mount | `docker inspect <container> --format='{{.SizeRootFs}}'` | Ensure `--mount type=bind,source=/host/path,target=/backup` is properly declared. |
| `tar: ./data: file changed as we read it` | Files modified during archive generation | Check tar return code (often exits with 1) | Pause container or use filesystem snapshots (LVM/ZFS). |
| Docker volume prune deleted active volume | Container was stopped and volume had no active container references | `docker volume ls` | Avoid `docker volume prune -a` in production; label volumes with `keep=true`. |
| Backup container hangs indefinitely | Database container was paused, but backup tried to run `docker exec` against the paused container | `docker ps` status shows `Paused` | Run commands against unpaused containers, or only pause during the tar step. |

---

## 8. Performance Tuning & Resource Optimization

1. **Leverage Modern Compression (`zstd`)**:
   Standard `gzip` is CPU-bound and slow on multi-gigabyte volumes. Modern `zstd` (Zstandard) compresses at 500 MB/s per core and decompresses at 1.5 GB/s, cutting backup time by 75%:
   ```bash
   tar -I 'zstd -3' -cf /backup/data.tar.zst -C /volume .
   ```
2. **Parallel Tar Streaming**:
   Using `pigz` (parallel gzip) allows multi-core systems to utilize all available CPUs during compression:
   ```bash
   tar -I pigz -cf /backup/data.tar.gz -C /volume .
   ```

---

## 9. Security Hardening & Threat Model

```
+--------------------------------------------------------------+
| ATTACK: Credential Exposure in Unencrypted Volume Backups    |
+--------------------------------------------------------------+
| Database volume backed up as plaintext `backup.tar.gz`.      |
| S3 bucket permissions misconfigured or developer laptop stolen.|
| Attacker extracts plaintext password hashes and PII data.    |
|                                                              |
| MITIGATION: ENCRYPT ARCHIVES AT REST USING GPG / AGE         |
+--------------------------------------------------------------+
```

### Encryption Pipeline Example
```bash
docker run --rm \
  --mount type=volume,source=my_vol,target=/data,readonly \
  alpine:3.21 tar -czf - -C /data . \
| gpg --symmetric --cipher-algo AES256 --output backup.tar.gz.gpg
```

---

## 10. Architectural Comparison Table

| Backup Technique | Downtime Required? | Data Consistency | Speed | Cloud Integration |
|:---|:---|:---|:---|:---|
| **Raw Ephemeral Tar (Unpaused)** | None | **Low (Risk of corruption)**| Fast | Manual |
| **Raw Tar (with `docker pause`)**| Seconds (Brief pause)| **High** | Ultra-Fast | Manual |
| **Logical Dump (`pg_dump`)** | None | **Maximum** | Moderate | High |
| **Snapshotting (ZFS / EBS)** | None | **High** | Instant | Native |
| **Dedicated Agent (Restic/Kopia)**| None | High (Deduplicated)| Fast | Native S3/GCS |

---

## 11. Real-World Scenario Walkthrough

### Scenario: Recovering from an Accidental `docker compose down -v`

#### The Disaster
A junior DevOps engineer attempted to restart an application stack on a production server. Instead of running `docker compose restart`, they executed:
```bash
docker compose down -v # The -v flag destroyed the persistent database volume!
```
The database containing 250,000 customer orders was completely wiped.

#### The Recovery Plan
1. **Locate Latest Offsite Snapshot**: Retrieved `prod_db_20250225_0300.tar.zst` from AWS S3 Glacier.
2. **Recreate Volume**:
   ```bash
   docker volume create myapp_pg_data
   ```
3. **Stream Decompression**:
   ```bash
   aws s3 cp s3://my-backups/prod_db_20250225_0300.tar.zst - | \
     docker run --rm -i \
       --mount type=volume,source=myapp_pg_data,target=/restore \
       alpine:3.21 sh -c "apk add --no-cache zstd && zstd -dc | tar -xf - -C /restore"
   ```
4. **Fix Permissions**: Enforced UID `999:999` ownership for postgres files.
5. **Re-launch Application**: Executed `docker compose up -d`. Total downtime: 18 minutes; RPO: 4 hours of data.

---

## 12. Self-Assessment & Interview Questions

<details>
<summary><strong>1. Why is running `docker run --rm -v myvol:/data:ro -v $(pwd):/backup alpine tar ...` preferred over accessing `/var/lib/docker/volumes` on the host directly?</strong></summary>

**Answer:**
1. **Portability**: It works consistently across Linux, macOS (Docker Desktop VM), and Windows without needing host VM access.
2. **Security**: Avoids modifying system permissions on `/var/lib/docker/`.
3. **Safety**: Mounting with `:ro` guarantees that the host backup process cannot accidentally corrupt or delete live files.
4. **Driver Abstraction**: It works seamlessly even if the volume uses an external volume plugin (e.g., NFS or Cloud Block Store).
</details>

<details>
<summary><strong>2. What is the danger of taking a raw filesystem archive (`tar`) of a running database volume without pausing it?</strong></summary>

**Answer:**
Relational databases continuously write to memory buffers, write-ahead logs (WAL), and table data files simultaneously. If a raw file copy reads a data file half-way through an in-flight flush, the resulting archive captures split-brain files (partial pages, inconsistent log sequence numbers). When restored, the database engine will detect checksum corruptions and fail to start.
</details>

<details>
<summary><strong>3. How does `docker pause` assist in achieving consistent volume backups?</strong></summary>

**Answer:**
`docker pause` uses the Linux kernel cgroups freezer subsystem to freeze all processes within the container at their current instruction. No new disk writes or transactional updates can occur while paused. This allows a quick tar snapshot of the volume files in an atomic, consistent state before running `docker unpause`.
</details>

<details>
<summary><strong>4. How do you ensure that restored volume files have the correct Linux user ownership (UID/GID)?</strong></summary>

**Answer:**
By using `tar -x --same-owner` during extraction, or executing an ephemeral container step immediately after restore to run `chown -R <uid>:<gid> /volume` matching the application's required user (e.g., UID 999 for PostgreSQL, UID 1000 for Node).
</details>

<details>
<summary><strong>5. How can you migrate a Docker volume between two remote hosts without writing an intermediate archive file to disk?</strong></summary>

**Answer:**
Pipe standard output from an ephemeral tar container on the source host over SSH directly into an ephemeral extraction container on the target host:
`docker run --rm -v vol:/v:ro alpine tar -cf - -C /v . | ssh user@remote "docker run --rm -i -v vol:/v alpine tar -xf - -C /v"`
</details>

<details>
<summary><strong>6. What is the difference between RTO and RPO in the context of container storage disaster recovery?</strong></summary>

**Answer:**
- **RTO (Recovery Time Objective)**: The maximum acceptable duration of downtime required to restore the volume and bring the service back online (e.g., under 30 minutes).
- **RPO (Recovery Point Objective)**: The maximum acceptable age of data that can be lost due to a disaster, dictated by backup frequency (e.g., hourly backups = max 1 hour RPO).
</details>

<details>
<summary><strong>7. How can you determine how much physical disk space an individual named volume consumes?</strong></summary>

**Answer:**
Run `docker system df -v` to view detailed space consumption for all volumes, or launch an ephemeral container:
`docker run --rm -v myvol:/data:ro alpine du -sh /data`
</details>

<details>
<summary><strong>8. Why does `docker compose down -v` represent a critical operational hazard?</strong></summary>

**Answer:**
The `-v` (or `--volumes`) flag instructs Docker Compose to delete not only the containers and networks defined in the `docker-compose.yml`, but also all named volumes declared under the `volumes:` key. If executed in production without backups, all persistent data is permanently destroyed.
</details>

---

## 13. Hands-on Practice Exercises

<details>
<summary><strong>Exercise 1: Full Backup and Restore Verification Workflow</strong></summary>

**Goal:** Create a volume with dummy state, archive it, destroy the volume, recreate it from archive, and verify contents.

**Step-by-step Solution:**
1. Create source volume and populate:
   ```bash
   docker volume create source_vol
   docker run --rm -v source_vol:/data alpine sh -c "echo 'Critical Payload 2025' > /data/payload.txt"
   ```
2. Archive:
   ```bash
   docker run --rm -v source_vol:/data:ro -v $(pwd):/backup alpine tar czf /backup/payload_bak.tar.gz -C /data .
   ```
3. Destroy source volume:
   ```bash
   docker volume rm source_vol
   ```
4. Restore to new volume:
   ```bash
   docker volume create restored_vol
   docker run --rm -v restored_vol:/data -v $(pwd):/backup alpine tar xzf /backup/payload_bak.tar.gz -C /data
   ```
5. Verify:
   ```bash
   docker run --rm -v restored_vol:/data alpine cat /data/payload.txt
   # Outputs: Critical Payload 2025
   ```
</details>

<details>
<summary><strong>Exercise 2: Non-Disruptive Logical Postgres Hot Backup</strong></summary>

**Goal:** Execute `pg_dump` on a live PostgreSQL container without pausing.

**Step-by-step Solution:**
1. Start postgres:
   ```bash
   docker run -d --name pg-test -e POSTGRES_PASSWORD=pass postgres:17-alpine
   sleep 5
   ```
2. Stream logical backup to host:
   ```bash
   docker exec pg-test pg_dump -U postgres postgres > backup.sql
   ```
3. Verify file size and SQL headers:
   ```bash
   head -n 10 backup.sql
   docker rm -f pg-test
   ```
</details>

<details>
<summary><strong>Exercise 3: Measuring Volume Disk Usage via CLI</strong></summary>

**Goal:** Inspect volume sizes across your local Docker daemon.

**Step-by-step Solution:**
1. Query system summary:
   ```bash
   docker system df -v
   ```
2. Locate the "Local Volumes" table showing Volume Name, Links, and Size.
</details>

<details>
<summary><strong>Exercise 4: Streamlined Encrypted Backup with GPG</strong></summary>

**Goal:** Archive and encrypt a volume in a single pipeline without intermediate unencrypted files.

**Step-by-step Solution:**
1. Run pipeline (passphrase: `testpassword`):
   ```bash
   docker run --rm -v restored_vol:/data:ro alpine tar -cf - -C /data . \
     | gpg --batch --yes --passphrase "testpassword" -c > secure_vol.tar.gpg
   ```
2. Inspect encrypted file:
   ```bash
   file secure_vol.tar.gpg
   # Outputs: GPG symmetrically encrypted data
   ```
</details>

<details>
<summary><strong>Exercise 5: Automated Cron Backup Script Authoring</strong></summary>

**Goal:** Author a shell script for automated nightly backups with timestamping and 7-day retention pruning.

**Step-by-step Solution:**
1. Create `backup.sh`:
   ```bash
   #!/bin/bash
   BACKUP_DIR="/var/backups/docker"
   DATE=$(date +%Y%m%d_%H%M%S)
   mkdir -p "$BACKUP_DIR"

   docker run --rm \
     -v pg_prod_data:/data:ro \
     -v "$BACKUP_DIR":/backup \
     alpine:3.21 tar -czf "/backup/pg_${DATE}.tar.gz" -C /data .

   # Prune backups older than 7 days
   find "$BACKUP_DIR" -type f -name "pg_*.tar.gz" -mtime +7 -delete
   ```
2. Test execution.
</details>

<details>
<summary><strong>Exercise 6: Simulating Pipe-Based Volume Cloning Locally</strong></summary>

**Goal:** Clone `vol_a` into `vol_b` without touching the host filesystem.

**Step-by-step Solution:**
1. Create volumes:
   ```bash
   docker volume create vol_src
   docker volume create vol_dst
   docker run --rm -v vol_src:/d alpine sh -c "echo 'Direct Pipe' > /d/test"
   ```
2. Stream from src to dst:
   ```bash
   docker run --rm -v vol_src:/s:ro alpine tar -cf - -C /s . \
   | docker run --rm -i -v vol_dst:/d alpine tar -xf - -C /d
   ```
3. Verify clone:
   ```bash
   docker run --rm -v vol_dst:/d alpine cat /d/test
   # Outputs: Direct Pipe
   ```
</details>

---

## 14. Summary & Reference Cheat Sheet

### Essential Commands
```bash
# Backup:
docker run --rm -v <VOL>:/v:ro -v $(pwd):/b alpine tar czf /b/<NAME>.tar.gz -C /v .

# Restore:
docker run --rm -v <VOL>:/v -v $(pwd):/b alpine tar xzf /b/<NAME>.tar.gz -C /v

# Remote Migration over SSH:
docker run --rm -v <VOL>:/v:ro alpine tar cf - -C /v . | ssh <HOST> "docker run --rm -i -v <VOL>:/v alpine tar xf - -C /v"
```

### Golden Rules
1. **Never raw-tar an active database** without `docker pause` or using logical dump tools.
2. **Mount volumes read-only (`:ro`)** in backup containers.
3. **Always preserve file ownership** (`tar --same-owner` or fix with `chown`).
4. **Replicate archives offsite** to S3/GCS.

---

[← Previous: 02-tmpfs-and-storage-drivers.md](02-tmpfs-and-storage-drivers.md) | [Next: 04-networking.md →](04-networking.md)
