---
title: "Docker Container Restart Policies: no, on-failure, always, and unless-stopped"
description: "Mastering Docker restart policies: self-healing automation, exponential backoff delays, host reboot recovery, crash loop containment, and orchestrator boundaries."
category: "DevOps / Containers"
tags: ["docker", "restart-policies", "reliability", "self-healing", "systemd", "commands"]
difficulty: "Beginner"
prerequisites: ["01-fundamentals/08-container-lifecycle-and-states.md", "02-commands/01-run-ps-stop.md"]
estimated_read_time: "25 min"
---

# Docker Container Restart Policies: no, on-failure, always, and unless-stopped

## Table of Contents
- [1. What It Is & Why It Matters](#1-what-it-is--why-it-matters)
- [2. The Four Restart Policies Deep Dive](#2-the-four-restart-policies-deep-dive)
- [3. How It Works Internally: Exponential Backoff](#3-how-it-works-internally-exponential-backoff)
- [4. Host Reboots and the `unless-stopped` Paradigm](#4-host-reboots-and-the-unless-stopped-paradigm)
- [5. Practical Diagnostic Demonstrations](#5-practical-diagnostic-demonstrations)
- [6. Best Practices & Production Guidelines](#6-best-practices--production-guidelines)
- [7. Common Pitfalls & Anti-Patterns](#7-common-pitfalls--anti-patterns)
- [8. Performance, Security & Cost Implications](#8-performance-security--cost-implications)
- [9. Comparison Table: Restart Policies](#9-comparison-table-restart-policies)
- [10. Technical Interview Questions](#10-technical-interview-questions)
- [11. Hands-On Practice Exercises](#11-hands-on-practice-exercises)
- [12. Summary / Cheat Sheet](#12-summary--cheat-sheet)
- [13. Further Reading & References](#13-further-reading--references)

---

## 1. What It Is & Why It Matters

In a production environment, applications crash. Network blips sever database connections, uncaught exceptions bubble up, external APIs time out, and edge devices experience power interruptions.

Without automated process supervision, a crashed container remains stuck in the `exited` state until a human engineer notices an outage alert and manually executes `docker start`.

Docker provides **Restart Policies** directly inside the daemon to automate container self-healing. A restart policy controls whether, when, and how frequently the Docker Engine automatically revives a container when its process exits or when the physical host reboots.

Understanding the subtle differences between policies—such as why `always` can unexpectedly restart containers you intentionally turned off, or why orchestrators like Kubernetes forbid Docker-level restart policies—is essential for building resilient infrastructure.

```
+---------------------------------------------------------------------------------+
|                         RESTART POLICY DECISION TREE                            |
|                                                                                 |
|                        [Container Process Exits]                                |
|                                    |                                            |
|                                    v                                            |
|                  What is the configured --restart policy?                       |
|                   /                |                 \                          |
|         +--------+                 |                  +--------+                |
|         |                          |                           |                |
|         v                          v                           v                |
|       ["no"]                ["on-failure"]            ["always" / "unless-      |
|  (Default behavior)                |                        stopped"]           |
|         |                          |                           |                |
|         v                          v                           v                |
|  Stay in EXITED state    Did it exit with code 0?      ALWAYS RESTART!          |
|  Do not restart.           /              \            (Wait backoff delay,     |
|                   +-------+                +-------+    revive container)       |
|                   |                                |                            |
|                   v (Exit 0 = Clean)               v (Exit > 0 = Crash)         |
|             Stay in EXITED.                   RESTART CONTAINER!                |
|             Job succeeded.                    (Up to max-retries)               |
+---------------------------------------------------------------------------------+
```

---

## 2. The Four Restart Policies Deep Dive

A restart policy is configured during container instantiation via the `--restart` flag on `docker run`, or updated on a running container via `docker update`:

```bash
docker run -d --restart <policy> <image>
```

### 1. `no` (Default)
Do not automatically restart the container under any circumstance. If the process terminates—whether due to a clean `exit 0` or a catastrophic crash `exit 137`—the container transitions to `exited` and stays there.
- **Ideal For**: Batch jobs, database migration scripts, one-off test runs.

### 2. `on-failure[:max-retries]`
Restarts the container **only if it exits with a non-zero exit code** (indicating an error or crash). 
- If the application completes successfully with exit code 0, Docker respects the clean exit and does **not** restart.
- An optional maximum retry limit can be attached: `--restart on-failure:5`. If the container crashes 5 times in a row, Docker stops retrying, preventing infinite crash loops.
- **Ideal For**: Background task workers, transient queues, and batch scripts that need transient fault tolerance.

### 3. `always`
Always restart the container whenever it terminates, regardless of whether it exited with code 0 or an error code.
- If the Docker daemon restarts or the physical server reboots, Docker **automatically restarts this container on boot**, even if an administrator had explicitly stopped it prior to the reboot!
- **Ideal For**: Core infrastructure daemons that must never remain offline under any circumstance.

### 4. `unless-stopped`
Behaves identically to `always`, with one critical, highly practical operational distinction:
- If an engineer explicitly stops the container (`docker stop my-app`), Docker records this intentional administrative action.
- When the Docker daemon restarts or the host machine reboots, Docker will **NOT** restart the container if it was in an explicitly stopped state before the reboot!
- **Ideal For**: Standard production web services, databases, and microservices in non-orchestrated environments.

---

## 3. How It Works Internally: Exponential Backoff

If an application crashes immediately upon boot (e.g., due to an invalid configuration file or missing database password), restarting it instantly in an infinite loop would consume 100% of host CPU and flood disk drives with gigabytes of identical crash logs per minute.

To prevent system exhaustion, the Docker daemon implements an **Exponential Backoff Algorithm**:
1. 1st crash: Restart delayed by $100\text{ ms}$.
2. 2nd crash: Restart delayed by $200\text{ ms}$.
3. 3rd crash: Restart delayed by $400\text{ ms}$.
4. Subsequent crashes: Delay doubles progressively up to a maximum cap (typically $\sim 1\text{ minute}$).
5. If the container stays running successfully for at least 10 seconds, Docker resets the backoff timer to zero.

During backoff delays, `docker ps` reports the container state as:
```text
STATUS: Restarting (1) 4 seconds ago
```

---

## 4. Host Reboots and the `unless-stopped` Paradigm

Consider the following common operational scenario:
1. It is Friday afternoon. An engineer runs `docker stop staging-database` to conduct hardware maintenance.
2. Over the weekend, the cloud provider applies kernel patches and reboots the underlying virtual machine host.
3. The Docker daemon boots up on system startup.

```
+---------------------------------------------------------------------------------+
|                       HOST REBOOT COMPARISON MATRIX                             |
+---------------------+-------------------------+---------------------------------+
| CONFIGURED POLICY   | PRE-REBOOT STATE        | BEHAVIOR AFTER SYSTEM REBOOT    |
+---------------------+-------------------------+---------------------------------+
| `no`                | Running                 | Stays Exited (Does not start)   |
| `no`                | Stopped (Exited)        | Stays Exited                    |
+---------------------+-------------------------+---------------------------------+
| `always`            | Running                 | Automatically starts on boot    |
| `always`            | Stopped manually        | AUTOMATICALLY STARTS ON BOOT!   |
|                     |                         | (Surprises engineers!)          |
+---------------------+-------------------------+---------------------------------+
| `unless-stopped`    | Running                 | Automatically starts on boot    |
| `unless-stopped`    | Stopped manually        | STAYS STOPPED!                  |
|                     |                         | (Respects engineer's intention) |
+---------------------+-------------------------+---------------------------------+
```
Because `unless-stopped` respects intentional human maintenance operations across host reboots, it is universally considered the **gold standard policy** for standalone production environments.

---

## 5. Practical Diagnostic Demonstrations

### Example 1: Observing Clean Exit with `on-failure`
Demonstrate that `on-failure` does not restart an application that completes cleanly with code 0:

```bash
# Run container with on-failure that completes cleanly with code 0
docker run -d --name clean-job --restart on-failure:3 alpine echo "Task completed successfully"

# Wait 2 seconds and inspect status
sleep 2
docker ps -a --filter "name=clean-job" --format "table {{.Names}}\t{{.Status}}\t{{.RestartCount}}"
```
*Output:*
```text
NAMES        STATUS                     RESTARTCOUNT
clean-job    Exited (0) 2 seconds ago   0
```
Notice `RestartCount: 0`. Docker recognizes code 0 as success and leaves the container cleanly exited.

```bash
# Cleanup
docker rm clean-job
```

### Example 2: Observing Automatic Retry Loop with `on-failure`
Demonstrate automatic restart when an application crashes with code 1:

```bash
# Run container with on-failure:3 that crashes with code 1
docker run -d --name crash-job --restart on-failure:3 alpine sh -c "echo 'Crashing!'; exit 1"

# Monitor over 6 seconds
sleep 4
docker ps -a --filter "name=crash-job" --format "table {{.Names}}\t{{.Status}}\t{{.RestartCount}}"
```
*Output:*
```text
NAMES        STATUS                  RESTARTCOUNT
crash-job    Restarting (1) 2s ago   2
```
Notice `RestartCount` increments as Docker automatically re-executes the container!

```bash
# Cleanup
docker rm -f crash-job
```

### Example 3: Modifying Restart Policy Dynamically via `docker update`
Change the restart policy on a running container without recreating or restarting it:

```bash
# Start container with default "no" policy
docker run -d --name dynamic-policy nginx:alpine

# Verify initial policy
docker inspect --format 'Policy: {{.HostConfig.RestartPolicy.Name}}' dynamic-policy
# Output: Policy: no

# Dynamically upgrade policy to unless-stopped!
docker update --restart unless-stopped dynamic-policy

# Verify updated policy
docker inspect --format 'Policy: {{.HostConfig.RestartPolicy.Name}}' dynamic-policy
# Output: Policy: unless-stopped

# Cleanup
docker rm -f dynamic-policy
```

### Example 4: Inspecting Exponential Backoff Delays
Observe the daemon throttling rapid restart cycles:

```bash
# Start failing container
docker run -d --name backoff-lab --restart always alpine sh -c "exit 42"

# Poll status every second for 5 seconds
for i in 1 2 3 4 5; do
  docker inspect --format 'Status: {{.State.Status}} | Retries: {{.RestartCount}}' backoff-lab
  sleep 1
done

# Cleanup
docker rm -f backoff-lab
```

### Example 5: Testing `unless-stopped` Persistence Simulation
Simulate a daemon restart by stopping and restarting the system service (Linux):

```bash
# Start container with unless-stopped
docker run -d --name persist-lab --restart unless-stopped nginx:alpine

# Manually stop the container
docker stop persist-lab

# Restart system Docker daemon (requires sudo)
sudo systemctl restart docker

# Confirm that persist-lab REMAINED in Exited state and was NOT revived!
docker ps -a --filter "name=persist-lab" --format "table {{.Names}}\t{{.Status}}"
# Output shows: Exited (0) ...

# Cleanup
docker rm -f persist-lab
```

---

## 6. Best Practices & Production Guidelines

1. **Use `unless-stopped` for Standalone Production Daemons**: On standalone servers or virtual machines (not managed by Kubernetes or Swarm), always configure `--restart unless-stopped` for web servers, databases, and background services.
2. **Cap Retries on Transient Batch Tasks**: Always pass a retry limit to `on-failure` (e.g., `--restart on-failure:5`) on worker processes to prevent infinite restart loops if external dependencies disappear permanently.
3. **Never Configure Restart Policies in Kubernetes or Nomad**: Orchestrators (Kubernetes Kubelet, Nomad, AWS ECS) provide their own sophisticated restart, backoff, and rescheduling logic. Setting Docker restart policies inside Kubernetes pods conflicts with orchestrator health probes. In Kubernetes, set container restart policies in the Pod spec, never in the container runtime engine.
4. **Pair Restart Policies with Healthchecks**: A container that deadlocks or hangs (infinite loop) will not exit, so a restart policy will never trigger! Pair restart policies with **Docker Healthchecks** and auto-healing daemons (like autoheal) to restart hung containers.
5. **Ensure Application Cleans Up Lockfiles on Boot**: If a database or queue crashes, it may leave stale `.pid` or `.lock` files on disk. Ensure your container's entrypoint script checks for and removes stale lockfiles on startup, or the restarted container will crash immediately.

---

## 7. Common Pitfalls & Anti-Patterns

| Anti-Pattern | Root Cause | Impact | Proper Fix |
|:---|:---|:---|:---|
| **Using `--restart always` in dev environments** | Developer testing broken code with `always`. | Laptop fans spin up; CPU at 100% running infinite 0-second crash loops. | Use `no` or `on-failure:3` during local development. |
| **Surprise startup of disabled services on boot** | Staging database stopped with `docker stop`, configured with `always`. | Server reboots; database restarts automatically, consuming memory and exposing ports. | Use `unless-stopped` instead of `always`. |
| **Stale lockfile crash loop** | App (e.g. Postgres or Rails) leaves `postmaster.pid` or `server.pid` after crash. | Restarted container detects lockfile, assumes another instance is running, and exits instantly. | Clean stale PID lockfiles in entrypoint script before spawning app. |
| **Using Docker restart policies in Kubernetes** | Setting restart policy in Docker daemon for K8s pods. | Kubelet and Docker fight over container lifecycle; pods fail to transition correctly. | Manage restart policies strictly via Kubernetes Pod manifests (`restartPolicy: Always`). |
| **Assuming restart policies fix deadlocks** | App hangs in infinite loop without crashing. | Process never exits, so Docker never restarts it; service remains completely unresponsive. | Implement container `HEALTHCHECK` instructions. |
| **Uncapped `on-failure` on permanent errors** | Missing database credentials with un-capped `on-failure`. | Container restarts endlessly every 60s, spamming logs for weeks. | Set explicit retry cap: `--restart on-failure:5`. |

---

## 8. Performance, Security & Cost Implications

### Performance
- **Zero Steady-State Overhead**: Evaluating restart policies occurs only when a process terminates; it consumes 0% CPU and zero memory during active container execution.
- **Exponential Backoff Protection**: Backoff throttling prevents runaway crash loops from starving host operating systems of CPU cycles.

### Security
- **Denial-of-Service Containment**: An attacker who discovers a remote crash exploit cannot keep a service down permanently if `--restart unless-stopped` is active, forcing the attacker to flood the server continuously and triggering intrusion detection alerts.

### Cost
- Prevents expensive, unplanned production downtime: transient out-of-memory spikes or network blips are resolved automatically in milliseconds without waking on-call engineers.

---

## 9. Comparison Table: Restart Policies

| Policy | Restarts on Exit Code 0? | Restarts on Exit Code > 0? | Restarts on Daemon/Host Boot? | Respects Manual `docker stop` on Boot? | Best Use Case |
|:---|:---|:---|:---|:---|:---|
| **`no`** | No | No | No | N/A | Batch jobs, unit tests, DB migrations |
| **`on-failure`** | No | **Yes** (Up to retries) | Only if exited with error before reboot | Yes | Background queue workers, transient tasks |
| **`always`** | **Yes** | **Yes** | **Yes** | **No** (Starts anyway!) | Critical system daemons |
| **`unless-stopped`**| **Yes**| **Yes** | **Yes** | **Yes** (Stays stopped!)| **Standard Production Web Services** |

---

## 10. Technical Interview Questions

<details>
<summary><strong>1. What is the fundamental difference between <code>always</code> and <code>unless-stopped</code> restart policies?</strong></summary>

Both policies automatically restart a container whenever its process exits. However, they differ in how they handle **system reboots** when a container was manually stopped prior to the reboot:
- Under **`always`**, when the host machine or Docker daemon reboots, Docker starts the container unconditionally, even if an administrator had previously executed `docker stop`.
- Under **`unless-stopped`**, Docker tracks whether the container was in an explicitly stopped state at the time of daemon shutdown. If it was manually stopped, it remains stopped after the reboot, respecting the administrator's intent.
</details>

<details>
<summary><strong>2. How does the Docker daemon prevent a crashing container from consuming 100% host CPU in a restart loop?</strong></summary>

The Docker daemon implements an **exponential backoff algorithm**. When a container exits repeatedly, Docker inserts a progressively doubling sleep delay between restart attempts ($100\text{ ms}$, $200\text{ ms}$, $400\text{ ms}$, ..., up to roughly 1 minute). During this delay, the container remains in the `restarting` state without executing instructions. If the container runs successfully for at least 10 seconds, the backoff timer resets to zero.
</details>

<details>
<summary><strong>3. If an application exits with code 0 (clean completion), will <code>--restart on-failure</code> restart it?</strong></summary>

No. The `on-failure` policy triggers restarts **only when the process exits with a non-zero exit code** (indicating an abnormal termination or crash). An exit code of 0 represents intentional, successful completion; Docker recognizes this and allows the container to transition to the `exited` state permanently.
</details>

<details>
<summary><strong>4. Why should restart policies be avoided when deploying containers under Kubernetes or Nomad?</strong></summary>

Container orchestrators like Kubernetes possess their own centralized lifecycle supervisors (the Kubelet). Kubernetes manages restart policies at the Pod specification level (`RestartPolicy: Always | OnFailure | Never`), tracking container failure counts, backoff timings, and determining whether to reschedule the pod to a different physical node. Configuring low-level Docker engine restart policies creates conflicting state machines between Docker and Kubernetes, leading to unexpected behavior.
</details>

<details>
<summary><strong>5. What occurs if a containerized application hangs in an infinite loop or deadlock without crashing?</strong></summary>

Restart policies only activate when a process **terminates and returns an exit code**. If an application deadlocks or hangs, its PID 1 process remains technically alive and running in the host process table. Consequently, Docker's restart policy will never trigger. To resolve deadlocks, administrators must define a **`HEALTHCHECK`** instruction in the Dockerfile paired with an auto-healing agent that terminates unhealthy containers.
</details>

<details>
<summary><strong>6. Can you update the restart policy of an existing container without deleting it?</strong></summary>

Yes. You can use the `docker update` command:
```bash
docker update --restart unless-stopped <container_name_or_id>
```
The Docker daemon updates its internal configuration state on disk immediately without interrupting or restarting the running container process.
</details>

<details>
<summary><strong>7. How can you query the restart count of a crashing container using <code>docker inspect</code>?</strong></summary>

Using Go templates:
```bash
docker inspect --format '{{.RestartCount}}' <container_name_or_id>
```
This returns the integer number of times Docker has automatically restarted the container.
</details>

<details>
<summary><strong>8. What is a common startup issue that prevents crashed database containers from recovering after automatic restart?</strong></summary>

Many stateful applications and databases (such as PostgreSQL, MySQL, Apache, or Rails) create a PID lockfile (e.g., `/var/run/postgresql.pid`) when booting to prevent concurrent instances from accessing the same data directory. If the container process is abruptly terminated by an OOM kill or power outage, the lockfile remains on the persistent volume. When Docker restarts the container, the application detects the stale lockfile, assumes an instance is already running, and exits immediately. Entrypoint scripts must be designed to clean up stale lockfiles on startup.
</details>

---

## 11. Hands-On Practice Exercises

<details>
<summary><strong>Exercise 1: Inspect Restart Policy Configuration via `docker inspect`</strong></summary>

**Goal**: Inspect the restart policy object of a running container.

```bash
docker run -d --name policy-lab --restart unless-stopped nginx:alpine
docker inspect --format '{{json .HostConfig.RestartPolicy}}' policy-lab
docker rm -f policy-lab
```
</details>

<details>
<summary><strong>Exercise 2: Test Retry Cap on `on-failure`</strong></summary>

**Goal**: Observe `on-failure:2` stopping after exactly two crash restarts.

```bash
# Launch container configured to fail after 2 retries
docker run -d --name cap-lab --restart on-failure:2 alpine sh -c "sleep 1; exit 42"

# Wait 6 seconds for retry attempts to complete
sleep 6

# Verify final status and restart count
docker inspect --format 'Status: {{.State.Status}} | Retries: {{.RestartCount}} | Exit: {{.State.ExitCode}}' cap-lab
# Retries will equal 2!

# Cleanup
docker rm cap-lab
```
</details>

<details>
<summary><strong>Exercise 3: Dynamically Toggle Restart Policies with `docker update`</strong></summary>

**Goal**: Switch a container from `always` to `no` on the fly.

```bash
docker run -d --name toggle-lab --restart always alpine sleep 300
docker update --restart no toggle-lab
docker inspect --format 'Updated Policy: {{.HostConfig.RestartPolicy.Name}}' toggle-lab
docker rm -f toggle-lab
```
</details>

<details>
<summary><strong>Exercise 4: Filter Containers Currently in `restarting` State</strong></summary>

**Goal**: Use CLI filters to identify crash-looping containers.

```bash
docker ps --filter "status=restarting"
```
</details>

<details>
<summary><strong>Exercise 5: Simulate Stale Lockfile Detection</strong></summary>

**Goal**: Write an entrypoint script that handles stale lockfiles before starting.

```bash
# Test lockfile cleanup pattern
docker run --rm alpine sh -c '
  LOCKFILE="/tmp/app.pid"
  [ -f "$LOCKFILE" ] && rm -f "$LOCKFILE" && echo "Cleaned stale lockfile."
  echo $$ > "$LOCKFILE"
  echo "Application running with PID $(cat $LOCKFILE)"
  rm -f "$LOCKFILE"
'
```
</details>

<details>
<summary><strong>Exercise 6: Query Container Failure Exit Codes in Scripting</strong></summary>

**Goal**: Format container status to display failure codes and restart counts.

```bash
docker ps -a --format "table {{.Names}}\t{{.Status}}\t{{.State}}"
```
</details>

---

## 12. Summary / Cheat Sheet

```
+---------------------------------------------------------------------------------------+
|                         RESTART POLICIES CHEAT SHEET                                  |
+---------------------------------------------------------------------------------------+
| Default (No Restart):  docker run --restart no <image> (Batch jobs, migrations)       |
| Crash Only with Cap:   docker run --restart on-failure:5 <image> (Queues, workers)    |
| Always (Even on Boot): docker run --restart always <image> (System daemons)           |
| Production Standard:   docker run --restart unless-stopped <image> (Web, APIs, DBs)   |
| Live Dynamic Update:   docker update --restart unless-stopped <container>             |
| Query Policy:          docker inspect --format '{{.HostConfig.RestartPolicy.Name}}'   |
| Query Retries:         docker inspect --format '{{.RestartCount}}' <container>        |
| Golden Rule:           Never use Docker restart policies inside Kubernetes pods!      |
+---------------------------------------------------------------------------------------+
```

---

## 13. Further Reading & References

- [Docker Documentation: Start Containers Automatically (Restart Policies)](https://docs.docker.com/config/containers/start-containers-automatically/)
- [Docker CLI Command Reference: docker update](https://docs.docker.com/reference/cli/docker/container/update/)
- [Kubernetes Documentation: Pod Lifecycle and Container Restart Policy](https://kubernetes.io/docs/concepts/workloads/pods/pod-lifecycle/#restart-policy)
- [Twelve-Factor App Methodology: Process Concurrency and Disposability](https://12factor.net/disposability)

---

| [< Previous: 06-resource-limits-cpu-memory.md](./06-resource-limits-cpu-memory.md) | [Next: 08-copying-files-and-exec-patterns.md](./08-copying-files-and-exec-patterns.md) > |
| :--- | :--- |
