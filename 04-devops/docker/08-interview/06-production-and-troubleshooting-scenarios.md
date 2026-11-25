<!--
================================================================================
Metadata:
  Title: Technical Interview Preparation - Production Incident Troubleshooting Scenarios
  Track: Docker / DevOps
  Difficulty: Advanced to Staff
  Estimated Time: 75 minutes
  Key Terminology: Root Cause Analysis (RCA), Incident Triage, OOMKilled, Inode Exhaustion, Dead Container, Signal Swallowing, Firewall Bypass, Multi-arch mismatch
================================================================================
-->

# Production Troubleshooting: 10 Real-World Incident Scenarios

[<- 05-security-qa.md](./05-security-qa.md) | [Back to Interview Overview](./README.md) | [Next Note: 07-docker-vs-kubernetes-and-vms.md ->](./07-docker-vs-kubernetes-and-vms.md)

---

## Overview

In Staff and Senior DevOps/SRE interviews, candidates are evaluated on their ability to diagnose complex, multi-system failures under pressure. This guide details **10 realistic production incident scenarios**, breaking down each into:
1. **The Alert & Symptoms**: What PagerDuty reports and what users experience.
2. **System Architecture**: The infrastructure topology.
3. **Diagnostic Triage**: Step-by-step CLI commands executed to narrow down the fault.
4. **Root Cause Analysis (RCA)**: Deep kernel and container engine mechanics.
5. **Immediate Remediation & Long-Term Prevention**: Code changes and architectural safeguards.

---

## Scenario Index

- [Scenario 1: The Midnight OOMKill with Exit Code 137](#scenario-1-the-midnight-oomkill-with-exit-code-137)
- [Scenario 2: The 100% Host Disk Outage (JSON Logging Flood)](#scenario-2-the-100-host-disk-outage-json-logging-flood)
- [Scenario 3: The 10-Second Shutdown Delay and Corrupted Inflight Transactions](#scenario-3-the-10-second-shutdown-delay-and-corrupted-inflight-transactions)
- [Scenario 4: The Vanishing Database Data on Container Re-deployment](#scenario-4-the-vanishing-database-data-on-container-re-deployment)
- [Scenario 5: The Bridge Subnet Corporate VPN Black Hole](#scenario-5-the-bridge-subnet-corporate-vpn-black-hole)
- [Scenario 6: "No Space Left on Device" with 500GB Free Disk (Inode Exhaustion)](#scenario-6-no-space-left-on-device-with-500gb-free-disk-inode-exhaustion)
- [Scenario 7: The Host Process Table Saturation (Fork Bomb)](#scenario-7-the-host-process-table-saturation-fork-bomb)
- [Scenario 8: The Ghost Container Stuck in "Removal In Progress"](#scenario-8-the-ghost-container-stuck-in-removal-in-progress)
- [Scenario 9: The Perimeter Firewall Bypass (Docker iptables Infiltration)](#scenario-9-the-perimeter-firewall-bypass-docker-iptables-infiltration)
- [Scenario 10: "Exec Format Error" on AWS Graviton Production Rollout](#scenario-10-exec-format-error-on-aws-graviton-production-rollout)

---

## Scenario 1: The Midnight OOMKill with Exit Code 137

### 1. Alert & Symptoms
At 02:14 UTC, PagerDuty triggers a P1 alert: `Payment-Gateway-Service CrashLooping`. Synthetic HTTP probes report 502 Bad Gateway. `docker ps -a` shows the container exited 10 seconds ago with status:
`Exited (137) 10 seconds ago`.

### 2. Architecture Context
A high-throughput Node.js microservice running in standalone Docker on an AWS EC2 instance (`m5.xlarge`, 4 vCPUs, 16GB RAM). The container is configured with `-m 1g --memory-swap 1g`.

### 3. Diagnostic Triage
```bash
# Step 1: Inspect container exit metadata
docker inspect payment-api --format 'ExitCode: {{.State.ExitCode}} | OOMKilled: {{.State.OOMKilled}}'
# Output: ExitCode: 137 | OOMKilled: true

# Step 2: Query Linux kernel ring buffer for OOM Killer details
dmesg -T | grep -E -i "oom[-_]killer|killed process"
# Output:
# [Wed Nov 25 02:14:02 2026] node invoked oom-killer: gfp_mask=0x1100cca(GFP_HIGHUSER_MOVABLE), order=0, oom_score_adj=0
# [Wed Nov 25 02:14:02 2026] Memory cgroup out of memory: Killed process 38412 (node) total-vm:1489204kB, anon-rss:1044120kB

# Step 3: Inspect memory usage patterns prior to crash from metrics
docker stats payment-api --no-stream
```

### 4. Root Cause Analysis (RCA)
The Node.js V8 runtime had no internal heap constraint configured (`--max-old-space-size`). Node.js believed it had access to the full 16GB host physical RAM. A batch query loaded 80,000 unpaginated transactions into memory. As V8 allocated heap memory beyond 1024MB, it crossed the container's hard cgroup memory ceiling (`memory.max = 1073741824`). The kernel's cgroup memory controller invoked `out_of_memory()`, terminating the Node.js process immediately with `SIGKILL`.

### 5. Immediate Remediation & Long-Term Prevention
- **Immediate Fix**: Increase container memory limit to `-m 2g`.
- **Long-Term Safeguard**: Align application heap limits with container cgroups:
  ```dockerfile
  ENV NODE_OPTIONS="--max-old-space-size=1500"
  ```
  Setting V8 heap to 1500MB on a 2048MB container leaves 548MB headroom for off-heap buffers, native libraries, and OS paging, preventing kernel OOM kills.

---

## Scenario 2: The 100% Host Disk Outage (JSON Logging Flood)

### 1. Alert & Symptoms
SRE receives alerts: `EC2 Host Disk Space Critical (100% /)`. The Docker daemon stops accepting commands (`docker ps` hangs or errors with `no space left on device`). SSH sessions fail with `cannot create temp file`.

### 2. Architecture Context
An Ubuntu 22.04 LTS host running 15 microservices managed via Docker Compose. The server has been running without restarts for 120 days.

### 3. Diagnostic Triage
```bash
# Step 1: Check filesystem disk utilization
df -h
# Output: /dev/nvme0n1p1   50G   50G     0 100% /

# Step 2: Find top disk consuming directories
sudo du -sh /var/lib/docker/* | sort -hr | head -n 5
# Output:
# 42G    /var/lib/docker/containers

# Step 3: Find specific oversized log files
sudo find /var/lib/docker/containers/ -name "*-json.log" -exec ls -lh {} + | sort -k5 -hr | head -n 3
# Output:
# -rw-r----- 1 root root 38G Nov 25 04:30 /var/lib/docker/containers/7b2.../7b2...-json.log
```

### 4. Root Cause Analysis (RCA)
A chatty backend microservice had debug logging enabled (`LOG_LEVEL=debug`), emitting 2,000 JSON log entries per second. Docker's default logging driver is `json-file` with **zero file rotation or size caps**. Over 4 months, a single container accumulated 38GB of raw text logs, completely filling the root filesystem partition and wedging the Docker daemon.

### 5. Immediate Remediation & Long-Term Prevention
- **Immediate Fix**: Truncate (do not delete while running) the oversized log file to immediately reclaim disk space:
  ```bash
  sudo truncate -s 0 /var/lib/docker/containers/7b2*/*-json.log
  ```
- **Long-Term Safeguard**: Enforce global log rotation in `/etc/docker/daemon.json`:
  ```json
  {
    "log-driver": "json-file",
    "log-opts": {
      "max-size": "50m",
      "max-file": "3"
    }
  }
  ```
  Reload daemon with `sudo systemctl reload docker`.

---

## Scenario 3: The 10-Second Shutdown Delay and Corrupted Inflight Transactions

### 1. Alert & Symptoms
Deployments take over 5 minutes per service. During rollouts, payment transactions fail mid-flight, leaving orders in inconsistent database states.

### 2. Architecture Context
A Python Flask web API containerized with Dockerfile and deployed via automated CI/CD rolling updates.

### 3. Diagnostic Triage
```bash
# Step 1: Measure exact container stop latency
time docker stop api-service
# Output:
# real    0m10.231s   <-- Notice exact 10 second delay!
# user    0m0.015s
# sys     0m0.010s

# Step 2: Inspect Dockerfile ENTRYPOINT
cat Dockerfile | grep -E "ENTRYPOINT|CMD"
# Output:
# ENTRYPOINT python app.py   <-- Shell form!

# Step 3: Inspect process tree inside container
docker exec api-service ps -ef
# Output:
# UID   PID  PPID  CMD
# root    1     0  /bin/sh -c python app.py
# root    7     1  python app.py
```

### 4. Root Cause Analysis (RCA)
The Dockerfile used **shell form** (`ENTRYPOINT python app.py`). Docker spawned `/bin/sh` as PID 1, and the Python app ran as child process PID 7. Standard POSIX shells do not forward signals to child processes. When Docker initiated `docker stop`, `SIGTERM` was sent to `/bin/sh`, which ignored it. The Python app never received `SIGTERM` and continued serving traffic until Docker's 10-second timeout expired, at which point the engine sent `SIGKILL`, abruptly aborting inflight database transactions.

### 5. Immediate Remediation & Long-Term Prevention
- **Fix**: Convert `ENTRYPOINT` to **JSON exec form** so Python executes directly as PID 1:
  ```dockerfile
  ENTRYPOINT ["python", "app.py"]
  ```
- Add signal handling in Python:
  ```python
  import signal, sys
  signal.signal(signal.SIGTERM, lambda s, f: sys.exit(0))
  ```
  `docker stop` now terminates cleanly in under 400 milliseconds.

---

## Scenario 4: The Vanishing Database Data on Container Re-deployment

### 1. Alert & Symptoms
Following a routine security patch re-deploying PostgreSQL (`company/postgres:16.1` $\to$ `company/postgres:16.2`), all database tables, user records, and product inventory completely disappeared.

### 2. Architecture Context
PostgreSQL managed via a basic `docker run` shell script in staging/production.

### 3. Diagnostic Triage
```bash
# Step 1: Inspect container mount metadata
docker inspect postgres-old --format '{{json .Mounts}}'
# Output: []   <-- ZERO persistent volumes or bind mounts attached!

# Step 2: Inspect storage location inside container
docker exec postgres-old ls -la /var/lib/postgresql/data
# Data files were written strictly into the container's overlay2 writable layer!
```

### 4. Root Cause Analysis (RCA)
The deployment script ran `docker run -d --name db postgres:16`. When updating the image, the script ran `docker rm -f db` followed by `docker run -d --name db postgres:16.2`. Because no named volume was attached, the database wrote all tables directly into the container's ephemeral writable overlay layer. Running `docker rm -f` permanently deleted the container layer and all database blocks on disk.

### 5. Immediate Remediation & Long-Term Prevention
- **Prevention**: Never run stateful services without explicit named volumes or persistent cloud block mounts:
  ```bash
  docker volume create pg_prod_data
  docker run -d --name db -v pg_prod_data:/var/lib/postgresql/data postgres:16.2
  ```
  In Compose, declare top-level `volumes:` and enforce CI checks rejecting database containers lacking volume mounts.

---

## Scenario 5: The Bridge Subnet Corporate VPN Black Hole

### 1. Alert & Symptoms
Developers connect to the corporate WireGuard/OpenVPN tunnel. Immediately, all access to corporate Jira, GitHub Enterprise (`172.17.5.20`), and internal databases drops with `Destination Host Unreachable`.

### 2. Architecture Context
Developer workstations running Docker Desktop or native Docker Engine on Ubuntu/macOS.

### 3. Diagnostic Triage
```bash
# Step 1: Inspect host routing table
ip route show
# Output:
# 172.17.0.0/16 dev docker0 proto kernel scope link src 172.17.0.1
# 172.17.0.0/16 dev wg0 proto static scope link

# Step 2: Trace route to corporate Jira server (172.17.5.20)
ip route get 172.17.5.20
# Output: 172.17.5.20 dev docker0 src 172.17.0.1
```

### 4. Root Cause Analysis (RCA)
Docker's default bridge network (`docker0`) automatically allocates subnet `172.17.0.0/16`. The corporate network also uses `172.17.0.0/16` for internal services. The Linux kernel routing table preferred the local interface `docker0` over the VPN virtual interface (`wg0`). All corporate traffic was routed into Docker's empty local bridge network and dropped.

### 5. Immediate Remediation & Long-Term Prevention
- **Fix**: Reconfigure Docker's default IP pools in `/etc/docker/daemon.json` to use an unallocated private CIDR:
  ```json
  {
    "bip": "10.220.0.1/24",
    "default-address-pools": [
      {
        "base": "10.220.1.0/16",
        "size": 24
      }
    ]
  }
  ```
  Restart Docker daemon: `sudo systemctl restart docker`. Route conflicts vanish immediately.

---

## Scenario 6: "No Space Left on Device" with 500GB Free Disk (Inode Exhaustion)

### 1. Alert & Symptoms
Applications fail to write logs or temp files with `Error: ENOSPC: no space left on device`. Running `df -h` shows **500GB of available free storage** on the NVMe drive.

### 2. Architecture Context
A high-throughput CI/CD build worker running hundreds of automated `docker build` jobs daily.

### 3. Diagnostic Triage
```bash
# Step 1: Check block disk space
df -h /var/lib/docker
# Output: Filesystem Size Used Avail Use% Mounted on
#         /dev/nvme0n1  1.0T 450G  550G  45% /var/lib/docker

# Step 2: Check inode utilization (-i flag)
df -i /var/lib/docker
# Output: Filesystem     Inodes   IUsed   IFree IUse% Mounted on
#         /dev/nvme0n1  65536000 65536000      0  100% /var/lib/docker   <-- INODE EXHAUSTION!

# Step 3: Find layers with millions of tiny files
sudo find /var/lib/docker/overlay2 -maxdepth 2 -type d -exec ls -1 {} \; | wc -l
```

### 4. Root Cause Analysis (RCA)
Filesystems allocate a fixed number of **inodes** (data structures indexing files). The CI server ran hundreds of unpruned Node.js builds containing millions of tiny `node_modules` files. Although total byte usage was only 450GB, the system exhausted all 65 million inodes. The kernel cannot create any new file when inodes reach 100%, regardless of free gigabytes.

### 5. Immediate Remediation & Long-Term Prevention
- **Immediate Fix**: Prune dangling images, builder cache, and stopped containers:
  ```bash
  docker system prune -a --volumes -f
  ```
- **Long-Term Safeguard**: Schedule daily cron maintenance:
  ```bash
  0 3 * * * /usr/bin/docker image prune -a --filter "until=48h" -f
  ```
  Format storage volumes with higher inode ratios (`mkfs.ext4 -i 8192`) or migrate to XFS (which supports dynamic inode allocation).

---

## Scenario 7: The Host Process Table Saturation (Fork Bomb)

### 1. Alert & Symptoms
An entire multi-tenant Linux server freezes. Terminal logins via SSH hang. Existing processes cannot spawn threads (`bash: fork: retry: Resource temporarily unavailable`).

### 2. Architecture Context
A multi-tenant host executing customer-submitted batch jobs inside unprivileged Docker containers.

### 3. Diagnostic Triage
```bash
# Step 1: Check active host process count
cat /proc/sys/kernel/pid_max
# Output: 32768

ps -eLf | wc -l
# Output: 32768   <-- Process table completely saturated!

# Step 2: Identify offending container
for cid in $(docker ps -q); do
  echo "$cid: $(docker top $cid | wc -l) processes"
done
# Output: 4f89a12c8e: 31850 processes
```

### 4. Root Cause Analysis (RCA)
A container executed a recursive fork bomb (`:(){ :|:& };:` or runaway thread leak). Because the container was started without `--pids-limit`, it spawned thousands of child processes, exhausting the host kernel's global PID allocation ceiling (`kernel.pid_max`).

### 5. Immediate Remediation & Long-Term Prevention
- **Immediate Fix**: Forcibly terminate the rogue container using SIGKILL directly on its host cgroup:
  ```bash
  docker kill 4f89a12c8e
  ```
- **Long-Term Safeguard**: Always enforce `--pids-limit` on all containers:
  ```bash
  docker run -d --pids-limit=200 my-batch-job
  ```
  In Compose: `deploy.resources.limits.pids: 200`.

---

## Scenario 8: The Ghost Container Stuck in "Removal In Progress"

### 1. Alert & Symptoms
CI pipeline fails on cleanup: `Error response from daemon: removal of container 8a9f... is already in progress`. Running `docker rm -f` hangs indefinitely.

### 2. Architecture Context
Linux host with Docker running long-lived volumes and frequent container churn.

### 3. Diagnostic Triage
```bash
# Step 1: Check container status
docker ps -a --filter "status=dead"
# Output shows container in "Removal In Progress" or "Dead"

# Step 2: Search for host processes holding locks on container overlay2 directory
CID=$(docker inspect --format '{{.Id}}' 8a9f...)
grep "$CID" /proc/*/mountinfo
# Output shows PID 29141 (orphaned sh process) holding an open file descriptor in /var/lib/docker/overlay2/.../merged
```

### 4. Root Cause Analysis (RCA)
An orphaned host process spawned during an earlier `docker exec` session remained running and held an open file lock on the container's overlay2 `merged` mount point. When `dockerd` attempted to unmount and delete the filesystem, the kernel returned `EBUSY (Device or resource busy)`. Docker marked the container as "Removal In Progress" and entered an indefinite deadlock.

### 5. Immediate Remediation & Long-Term Prevention
- **Immediate Fix**:
  ```bash
  # 1. Kill the locking host process
  sudo kill -9 29141

  # 2. Lazily unmount the stuck overlay directory
  sudo umount -l /var/lib/docker/overlay2/<CID>/merged

  # 3. Remove container cleanly
  docker rm -f 8a9f...
  ```

---

## Scenario 9: The Perimeter Firewall Bypass (Docker iptables Infiltration)

### 1. Alert & Symptoms
Security compliance audit reports that production MongoDB databases (`port 27017`) containing customer PII are accessible from public internet IPs, despite UFW being enabled with strict default-deny policies.

### 2. Architecture Context
An Ubuntu server running MongoDB via Docker Compose with UFW configured:
`ufw default deny incoming`, `ufw allow 22/tcp`, `ufw enable`.

### 3. Diagnostic Triage
```bash
# Step 1: Verify UFW status
sudo ufw status
# Output shows port 27017 is NOT allowed!

# Step 2: Inspect raw Linux kernel iptables NAT table
sudo iptables -t nat -L PREROUTING -n -v
# Output shows Docker injected a high-priority rule:
# DNAT tcp -- * * 0.0.0.0/0 0.0.0.0/0 tcp dpt:27017 to:172.18.0.4:27017
```

### 4. Root Cause Analysis (RCA)
`compose.yaml` declared `ports: ["27017:27017"]`. Docker automatically created a DNAT rule in `iptables PREROUTING`. When an external packet arrives, the kernel evaluates PREROUTING before UFW filter chains. The packet is forwarded directly to the container, completely bypassing UFW.

### 5. Immediate Remediation & Long-Term Prevention
- **Immediate Fix**: Bind the port strictly to localhost loopback:
  ```yaml
  ports:
    - "127.0.0.1:27017:27017"
  ```
- **Architectural Safeguard**: Inject custom perimeter protection into the `DOCKER-USER` chain or place database containers exclusively on internal isolated networks without port mappings.

---

## Scenario 10: "Exec Format Error" on AWS Graviton Production Rollout

### 1. Alert & Symptoms
Following an upgrade to AWS Graviton (ARM64) EC2 instances to reduce cloud compute costs, all deployed container services immediately crash loop with:
`exec /entrypoint.sh: exec format error`.

### 2. Architecture Context
GitHub Actions pipeline building images using standard `docker build` on default Ubuntu runners and pushing to Amazon ECR.

### 3. Diagnostic Triage
```bash
# Step 1: Inspect container logs
docker logs crashed-api
# Output: standard_init_linux.go:228: exec user process caused: exec format error

# Step 2: Inspect image CPU architecture metadata
docker image inspect my-ecr-registry/api:latest --format 'Architecture: {{.Architecture}} | OS: {{.Os}}'
# Output: Architecture: amd64 | OS: linux

# Step 3: Check host architecture
uname -m
# Output: aarch64 (ARM64)
```

### 4. Root Cause Analysis (RCA)
The CI/CD build runner ran on x86_64 hardware and executed standard `docker build`, producing an `amd64` ELF binary. When deployed to AWS Graviton (`aarch64`), the ARM64 Linux kernel attempted to execute x86 machine code instructions, failing immediately with Linux kernel error `ENOEXEC` (`exec format error`).

### 5. Immediate Remediation & Long-Term Prevention
- **Fix**: Update CI/CD pipeline to compile multi-platform images using Buildx:
  ```bash
  docker buildx build --platform linux/amd64,linux/arm64 -t my-ecr-registry/api:latest --push .
  ```
  The registry now hosts an OCI Manifest List serving ARM64 binaries to Graviton instances and AMD64 binaries to x86 instances.

---

[<- 05-security-qa.md](./05-security-qa.md) | [Back to Interview Overview](./README.md) | [Next Note: 07-docker-vs-kubernetes-and-vms.md ->](./07-docker-vs-kubernetes-and-vms.md)
