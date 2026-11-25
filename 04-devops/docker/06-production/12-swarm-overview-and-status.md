<!--
================================================================================
Metadata:
  Title: Docker Swarm Overview, Architecture, and Industry Status
  Topic: Docker Production Engineering
  Difficulty: Intermediate to Advanced
  Estimated Time: 45 minutes
  Prerequisites: 04-volumes-and-networking/04-networking.md, 05-compose/01-compose-basics.md
  Key Terminology: Swarm Mode, Raft Consensus, Manager Node, Worker Node, Routing Mesh, Ingress, VIP, docker stack deploy
================================================================================
-->

# Docker Swarm Overview, Architecture, and Industry Status

[<- 11-docker-daemon-configuration-and-troubleshooting.md](./11-docker-daemon-configuration-and-troubleshooting.md) | [Back to Module Overview](./README.md) | [13-from-docker-to-kubernetes.md ->](./13-from-docker-to-kubernetes.md)

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

As containerized applications expand beyond single-host virtual machines, engineering teams face the orchestration challenge: scheduling containers across multi-node clusters, managing rolling updates with zero downtime, balancing network ingress across replicated containers, and healing failed tasks automatically.

Docker Swarm (specifically "Swarm Mode", integrated directly into Docker Engine since version 1.12) is Docker's native clustering and orchestration engine. While Kubernetes has become the enterprise standard for planetary-scale container orchestration, Docker Swarm remains one of the simplest, lowest-overhead orchestrators available:
- Requires **zero external dependencies** (no etcd, no external CNI plugins, no separate control plane binaries).
- Operates out-of-the-box using the standard `docker` CLI.
- Native declarative support for multi-service deployments via `docker stack deploy` using Compose files.

Understanding Swarm provides essential architectural foundations for distributed state, consensus algorithms, and cluster ingress routing.

---

## 2. Core Concepts

### Nodes: Managers vs Workers
A Swarm cluster consists of one or more physical or virtual nodes:
- **Manager Nodes**: Maintain cluster state, schedule service tasks across nodes, handle Raft consensus, and expose the Swarm API. A cluster should have an odd number of managers ($3$ or $5$) to maintain quorum during network partitions.
- **Worker Nodes**: Receive and execute tasks assigned by managers. Workers do not participate in Raft consensus and have zero authority over cluster state.

### Quorum and Raft Consensus
Managers use the **Raft Consensus Protocol** to agree on cluster state and task assignments.
- Quorum is calculated as:
  $$\text{Quorum} = \left\lfloor \frac{N}{2} \right\rfloor + 1$$
- For a 3-manager cluster, quorum is 2; the cluster tolerates 1 manager failure.
- For a 5-manager cluster, quorum is 3; the cluster tolerates 2 manager failures.
- If quorum is lost, existing workloads continue executing, but no new services can be created, updated, or rescheduled.

### Services and Tasks
- **Service**: The declarative definition of desired cluster state (e.g., "Run 5 replicas of `redis:7-alpine` with 256MB memory limit").
- **Task**: An atomic slot containing one running container and the execution commands assigned to a specific worker node.

### Ingress Routing Mesh & IPVS
Docker Swarm includes a built-in Layer 4 routing mesh. When a service publishes a port (e.g., `-p 8080:80`), every node in the Swarm listens on port 8080. Even if a node hosts zero replicas of that service, Linux IPVS (IP Virtual Server) inside the kernel automatically routes inbound requests across the overlay network to a node hosting an active task.

---

## 3. How It Works Internally

```
+-----------------------------------------------------------------------------------+
|                            DOCKER SWARM CLUSTER                                   |
|                                                                                   |
|  +-----------------------------------------------------------------------------+  |
|  |                         RAFT CONSENSUS RING (MANAGERS)                      |  |
|  |                                                                             |  |
|  |     +--------------------+   gRPC (2377)   +--------------------+           |  |
|  |     |  Manager 1 (Leader)| <-------------> |  Manager 2 (Follow)|           |  |
|  |     +---------+----------+                 +---------+----------+           |  |
|  |               ^                                      ^                      |  |
|  |               |                 gRPC (2377)          |                      |  |
|  |               +--------------------------------------+                      |  |
|  |                               |                                             |  |
|  |                               v                                             |  |
|  |                     +--------------------+                                  |  |
|  |                     |  Manager 3 (Follow)|                                  |  |
|  |                     +--------------------+                                  |  |
|  +-----------------------------------------------------------------------------+  |
|                                  | Dispatcher / Scheduler                         |
|                                  v                                                |
|  +-----------------------------------------------------------------------------+  |
|  |                          VXLAN OVERLAY NETWORK (Data Path)                  |  |
|  |                               UDP Port 4789                                 |  |
|  |                                                                             |  |
|  |   +-----------------------+                    +-----------------------+    |  |
|  |   |     Worker Node 1     |                    |     Worker Node 2     |    |  |
|  |   |                       |                    |                       |    |  |
|  |   | [Ingress IPVS Mesh]   |   VXLAN Encaps     | [Ingress IPVS Mesh]   |    |  |
|  |   | Listen :8080          | =================> | Listen :8080          |    |  |
|  |   |        |              |                    |        |              |    |  |
|  |   |        v              |                    |        v              |    |  |
|  |   | (Task 1: web:v1)      |                    | (Task 2: web:v1)      |    |  |
|  |   | Virtual IP: 10.0.0.5  |                    | Virtual IP: 10.0.0.5  |    |  |
|  |   +-----------------------+                    +-----------------------+    |  |
|  +-----------------------------------------------------------------------------+  |
+-----------------------------------------------------------------------------------+
```

---

## 4. Syntax & Usage

### Cluster Lifecycle Commands

```bash
# 1. Initialize Swarm on first manager node
docker swarm init --advertise-addr 192.168.1.10

# 2. View join tokens
docker swarm join-token worker    # Generates command for worker nodes
docker swarm join-token manager   # Generates command for manager nodes

# 3. Join a worker node to the cluster
docker swarm join --token SWMTKN-1-49nj1... 192.168.1.10:2377

# 4. List cluster nodes
docker node ls

# 5. Promote or demote nodes
docker node promote worker-02
docker node demote manager-03

# 6. Drain a node for maintenance (migrates all tasks away)
docker node update --availability drain worker-01
```

### Declarative Service & Stack Commands

```bash
# Create a replicated service with rolling updates
docker service create \
  --name web-app \
  --replicas 4 \
  --publish published=80,target=8080 \
  --update-delay 10s \
  --update-parallelism 2 \
  --update-failure-action rollback \
  my-registry.io/api:v1.0.0

# Deploy an entire multi-tier stack via Compose file
docker stack deploy -c compose.swarm.yml prod-stack

# List running stacks and services
docker stack ls
docker stack services prod-stack
docker service ps prod-stack_web-app

# Scale a service dynamically
docker service scale prod-stack_web-app=8
```

---

## 5. Practical Examples

### Example 1: Declarative Stack Compose File (`compose.swarm.yml`)

```yaml
# compose.swarm.yml
version: "3.8"

services:
  web:
    image: nginx:1.27-alpine
    ports:
      - "80:80"
    networks:
      - frontend
      - backend
    deploy:
      mode: replicated
      replicas: 3
      update_config:
        parallelism: 1
        delay: 10s
        failure_action: rollback
        monitor: 15s
        order: start-first
      restart_policy:
        condition: on-failure
        max_attempts: 3
      resources:
        limits:
          cpus: "0.5"
          memory: 256M

  api:
    image: company/api:v2.1.0
    networks:
      - backend
    secrets:
      - db_password
    deploy:
      replicas: 4
      placement:
        constraints:
          - node.role == worker

secrets:
  db_password:
    external: true

networks:
  frontend:
    driver: overlay
  backend:
    driver: overlay
    internal: true
```

### Example 2: Managing Secrets in Swarm (Encrypted in Raft Log)

```bash
# 1. Create a secure secret from stdin or file
echo "SuperSecretProdDBKey2026!" | docker secret create db_password -

# 2. Verify secret is registered in cluster
docker secret ls

# 3. Deploy service consuming the secret
docker service create \
  --name secure-app \
  --secret db_password \
  --replicas 2 \
  alpine sh -c "cat /run/secrets/db_password && sleep 3600"

# Note: Secret is mounted as an in-memory tmpfs file inside the container at /run/secrets/db_password
```

### Example 3: Zero-Downtime Rolling Update with Rollback Trigger

```bash
# Deploy initial service
docker service create --name payment-service --replicas 4 \
  --update-parallelism 1 \
  --update-delay 5s \
  --update-failure-action rollback \
  nginx:1.26-alpine

# Trigger rolling update to an intentionally broken image
docker service update --image nginx:non-existent-tag-404 payment-service

# Inspect rolling update in progress
docker service ps payment-service
# Output shows task failures, followed by automatic rollback to nginx:1.26-alpine!
```

---

## 6. Best Practices

1. **Maintain Odd Number of Managers**: Run 3 managers for small-to-medium clusters (tolerates 1 failure) or 5 managers for large enterprise clusters (tolerates 2 failures). Never run an even number (e.g., 4 managers has the same fault tolerance as 3, but higher overhead).
2. **Dedicated Manager Nodes in Large Clusters**: In clusters exceeding 20 nodes, drain managers (`docker node update --availability drain <mgr>`) so they only run control plane logic and do not execute heavy user application containers.
3. **Always Configure `order: start-first` for Zero-Downtime Updates**: By default, Swarm stops the old container before starting the new one (`stop-first`). Specifying `order: start-first` spins up the healthy replacement before terminating the predecessor.
4. **Isolate Networks with `internal: true`**: Overlay networks connecting backend databases should be marked `internal: true` to prevent accidental public egress or ingress.
5. **Backup the Raft Directory**: Periodically snapshot `/var/lib/docker/swarm/raft/` on manager nodes while Docker is temporarily paused or using consistent volume snapshots.

---

## 7. Common Mistakes & Troubleshooting

| Symptom / Error | Root Cause | Diagnosis Command | Remediation |
|:----------------|:-----------|:------------------|:------------|
| **`Error response from daemon: rpc error: code = Unknown desc = The swarm does not have a leader`** | Manager quorum lost due to node network partition or failure | `docker node ls` fails on remaining managers | Recover quorum: `docker swarm init --force-new-cluster --advertise-addr <ip>`. |
| **Containers cannot communicate across overlay network** | UDP port 4789 (VXLAN) blocked by cloud security group or firewall | `nc -zvu <worker-ip> 4789` | Open UDP 4789, TCP/UDP 7946, and TCP 2377 in firewall. |
| **`no suitable node (scheduling constraints not satisfied)`** | Node labels or resource requests cannot be satisfied | `docker service ps --no-trunc <svc>` | Adjust CPU/memory limits or add missing node labels (`docker node update --label-add`). |
| **External traffic hits node but gets connection refused** | Target container failed health checks; removed from ingress VIP | `docker inspect <container> --format '{{.State.Health.Status}}'` | Fix application health check script or port binding. |
| **`docker swarm init` fails: "could not choose an IP address"** | Host has multiple network interfaces | `ip addr show` | Specify explicit interface: `docker swarm init --advertise-addr <eth0_ip>`. |
| **High latency on ingress routing mesh** | IPVS connection tracking table saturated under high traffic | `dmesg \| grep -i "nf_conntrack"` | Bypass routing mesh by using `mode: host` port publication. |

---

## 8. Performance, Security & Cost Implications

### Performance
Swarm's routing mesh relies on Linux IPVS and iptables NAT. For microservices processing tens of thousands of requests per second, the extra hop across the routing mesh introduces 1–3ms of latency. To achieve maximum throughput, publish ports in `host` mode (`mode: host`), allowing an external load balancer (such as AWS ALB or HAProxy) to route directly to container host ports.

### Security
Swarm features **mutual TLS (mTLS)** by default. Every manager and worker node receives an X.509 certificate signed by the Swarm root CA. Node-to-manager communications, node join operations, and telemetry are encrypted automatically with automated certificate rotation every 90 days.

### Cost
Swarm uses a fraction of the compute and memory footprint of Kubernetes. A 3-node Swarm manager cluster runs comfortably on 1 vCPU / 1GB RAM instances, whereas a production Kubernetes control plane requires significantly higher resource allocations.

---

## 9. Comparison Table: Docker Swarm vs. Kubernetes

| Feature | Docker Swarm | Kubernetes (K8s) |
|:--------|:-------------|:-----------------|
| **Installation & Setup** | Built-in to `dockerd` (`docker swarm init`) | Complex (`kubeadm`, managed EKS/GKE/AKS) |
| **CLI & Tooling** | Standard `docker` CLI & Compose | `kubectl`, Helm, Kustomize |
| **Resource Footprint** | Extremely low (~50MB per node) | High (etcd, apiserver, scheduler, kubelet) |
| **Autoscaling** | Manual scaling (or custom scripts) | Native HPA, VPA, and Cluster Autoscaler |
| **Extensibility / Ecosystem**| Rigid, fixed feature set | Massive (CRDs, Operators, Service Meshes) |
| **Storage Orchestration** | Basic (host volumes, NFS plugins) | Advanced CSI (Dynamic PVC provisioning) |
| **Learning Curve** | 1–2 days | Several months |
| **Current Maintenance Status** | Maintained in Docker CE (bugfixes/stability) | Hyper-active global open-source ecosystem |

---

## 10. Real-World Interview Questions

<details>
<summary><strong>Q1: What is the current industry status and support roadmap of Docker Swarm?</strong></summary>

In 2019, Mirantis acquired Docker's Enterprise business and initially signaled a transition toward Kubernetes, but later committed to long-term support for Swarm in Mirantis Kubernetes Engine (MKE). In the open-source community, Swarm Mode remains integrated directly into Docker Engine (Moby project) and Docker Desktop. While Swarm is not actively receiving major new architectural feature overhauls (it is largely in a stable maintenance and security patching state), it remains widely utilized by small-to-medium businesses and edge IoT deployments that prioritize operational simplicity over Kubernetes' complexity.
</details>

<details>
<summary><strong>Q2: Explain how the Swarm Ingress Routing Mesh works under the hood.</strong></summary>

When a port is published with `target: 8080, published: 80` in routing mesh mode:
1. Every node in the cluster joins an internal overlay network named `ingress`.
2. A virtual IP (VIP) is assigned to the service within the `ingress` network.
3. Linux IPVS rules are configured inside the ingress network namespace on every node.
4. When external traffic hits port 80 on *any* node, iptables forwards the packet into the ingress sandbox.
5. Linux IPVS load-balances the TCP connection across all healthy tasks participating in that service across the entire cluster using VXLAN encapsulation.
</details>

<details>
<summary><strong>Q3: What firewall ports must be open between nodes for a Docker Swarm cluster to function?</strong></summary>

Three primary network ports are mandatory:
1. **TCP port 2377**: For cluster management communications (Manager-to-Manager Raft consensus and Worker-to-Manager control plane).
2. **TCP and UDP port 7946**: For node-to-node communication, gossip-based node discovery, and health checking.
3. **UDP port 4789**: For overlay network data traffic (VXLAN encapsulation).
If encrypted overlay networks are used, IP protocol 50 (ESP) must also be permitted.
</details>

<details>
<summary><strong>Q4: How does Swarm handle Manager Quorum loss, and how do you perform disaster recovery?</strong></summary>

If an odd-numbered manager ring loses more than $(N-1)/2$ nodes (e.g., 2 out of 3 managers permanently crash), Raft consensus terminates. The surviving manager enters read-only mode: existing containers continue executing, but no state changes can be processed.
To recover without restoring dead nodes:
On the surviving manager, run:
```bash
docker swarm init --force-new-cluster --advertise-addr <surviving_manager_ip>
```
This forces the local manager to discard existing Raft peer records, elect itself as the single authoritative leader of a new 1-node manager ring, and regain control of the cluster.
</details>

<details>
<summary><strong>Q5: What is the difference between a `replicated` and a `global` service in Docker Swarm?</strong></summary>

- **`replicated` (default)**: The user defines a desired replica count (e.g., `replicas: 5`). Swarm schedules exactly 5 tasks across the available worker nodes based on resource capacity and spread constraints.
- **`global`**: Swarm automatically schedules exactly one task on *every* active node in the cluster that meets placement constraints. This mode is typically used for monitoring daemons (Prometheus node-exporter), logging agents (Fluentbit), or security scanners.
</details>

<details>
<summary><strong>Q6: How do Docker Secrets differ in Swarm mode compared to standard Docker Compose?</strong></summary>

In standard standalone Docker Compose, secrets are simply bind-mounted from local host files into the container filesystem (unencrypted at rest on the host). In Docker Swarm mode:
1. Secrets are transmitted to managers over TLS.
2. Secrets are stored strictly in the encrypted Raft log on disk.
3. When a task is scheduled on a worker node, the secret is dispatched securely over mTLS and mounted into the container as an in-memory `tmpfs` file at `/run/secrets/<name>`.
4. The secret is never written to disk on the worker node and is immediately wiped when the container stops.
</details>

<details>
<summary><strong>Q7: Can a node be gracefully removed from a Swarm cluster without impacting running services?</strong></summary>

Yes:
1. Drain the node: `docker node update --availability drain <node-id>`. Swarm immediately stops all tasks on that node and reschedules healthy replicas onto remaining active nodes.
2. Wait for `docker service ps` to verify all tasks have migrated.
3. On the worker node, run `docker swarm leave`.
4. On a manager node, remove the node record: `docker node rm <node-id>`.
</details>

<details>
<summary><strong>Q8: When should an enterprise choose Kubernetes over Docker Swarm?</strong></summary>

Kubernetes is required when an organization needs:
1. Fine-grained Role-Based Access Control (RBAC) and multi-tenancy.
2. Advanced autoscaling (Horizontal Pod Autoscaler based on custom Prometheus metrics, Vertical Pod Autoscaling, and automated cloud node pool provisioning).
3. Complex storage orchestration requiring dynamic provisioning across multiple cloud block storage providers (AWS EBS, GCP Persistent Disk, Ceph).
4. Rich ecosystem tooling such as Service Meshes (Istio, Linkerd), GitOps engines (ArgoCD, Flux), and Custom Resource Definitions (CRDs).
</details>

---

## 11. Practice Exercises

<details>
<summary><strong>Exercise 1: Single-Node Swarm Initialization and Teardown</strong></summary>

**Task**: Initialize Swarm mode on your local machine, inspect the cluster status, and tear down Swarm mode cleanly.

**Solution**:
```bash
# 1. Initialize Swarm
docker swarm init

# 2. Inspect Swarm info
docker info --format 'Swarm State: {{.Swarm.LocalNodeState}} | Control Leaders: {{.Swarm.Managers}}'

# 3. Leave Swarm mode (must use --force on a single manager)
docker swarm leave --force
```
</details>

<details>
<summary><strong>Exercise 2: Deploy and Inspect a Replicated Service with Health Check</strong></summary>

**Task**: Deploy a 3-replica Nginx service published on host port 8088 with an automated HTTP health check.

**Solution**:
```bash
# Initialize swarm if needed
docker swarm init 2>/dev/null || true

# Deploy service
docker service create \
  --name nginx-demo \
  --replicas 3 \
  --publish 8088:80 \
  --health-cmd "wget -q -O - http://localhost/ || exit 1" \
  --health-interval 5s \
  nginx:1.27-alpine

# Verify task distribution across nodes
docker service ps nginx-demo

# Clean up
docker service rm nginx-demo
```
</details>

<details>
<summary><strong>Exercise 3: Deploy a Global Monitoring Daemon</strong></summary>

**Task**: Write a `docker service create` command that deploys an Alpine sleep task as a `global` service to guarantee it runs on every node.

**Solution**:
```bash
docker service create \
  --name cluster-agent \
  --mode global \
  alpine sleep 3600

docker service ps cluster-agent
docker service rm cluster-agent
```
</details>

<details>
<summary><strong>Exercise 4: Create and Verify Encrypted Overlay Network</strong></summary>

**Task**: Create an overlay network named `secure-net` with IPSec data plane encryption enabled.

**Solution**:
```bash
# Create encrypted overlay network
docker network create \
  --driver overlay \
  --opt encrypted \
  secure-net

# Verify configuration
docker network inspect secure-net --format 'Driver: {{.Driver}} | Encrypted: {{index .Options "encrypted"}}'
```
</details>

<details>
<summary><strong>Exercise 5: Dynamic Service Scaling and Resource Clamping</strong></summary>

**Task**: Update an existing service `api-worker` to limit memory to 128MB and scale it to 6 replicas.

**Solution**:
```bash
# Create service
docker service create --name api-worker --replicas 2 alpine sleep 3600

# Update memory limit and scale in one command
docker service update --limit-memory 128M --replicas 6 api-worker

# Verify update
docker service ps api-worker
docker service rm api-worker
```
</details>

<details>
<summary><strong>Exercise 6: Node Labeling and Placement Constraints</strong></summary>

**Task**: Add a custom label `disk=ssd` to the local node and schedule a service constrained to run only on nodes with that label.

**Solution**:
```bash
# 1. Retrieve local node ID
NODE_ID=$(docker node ls -q)

# 2. Add label
docker node update --label-add disk=ssd $NODE_ID

# 3. Schedule service with placement constraint
docker service create \
  --name ssd-app \
  --constraint 'node.labels.disk == ssd' \
  alpine sleep 3600

# 4. Verify placement
docker service ps ssd-app
docker service rm ssd-app
```
</details>

---

## 12. Summary & Quick Reference Cheat Sheet

### Core Swarm CLI Commands

```bash
# Cluster Management
docker swarm init --advertise-addr <ip>    # Start new cluster as Leader
docker swarm join --token <token> <ip>:2377 # Join cluster as worker/manager
docker swarm leave [--force]              # Leave cluster
docker swarm update                       # Update swarm settings (e.g. cert expiry)

# Node Administration
docker node ls                            # List all nodes and status
docker node inspect <node>                # Detailed node metadata
docker node promote <node>                # Worker -> Manager
docker node demote <node>                 # Manager -> Worker
docker node update --availability drain <n> # Evacuate tasks for maintenance

# Service Management
docker service create [flags] <image>     # Create service
docker service ls                         # List active services
docker service ps <service>               # View tasks and placement
docker service scale <service>=<n>        # Adjust replica count
docker service update [flags] <service>   # Rolling update parameters
docker service rollback <service>         # Revert to previous config
docker service rm <service>               # Teardown service

# Stack Management
docker stack deploy -c <file> <name>      # Deploy/update Compose stack
docker stack ls                           # List stacks
docker stack ps <name>                    # View stack tasks
docker stack rm <name>                    # Teardown stack
```

---

## 13. Further Reading & Official Documentation

- [Docker Documentation: Swarm mode overview](https://docs.docker.com/engine/swarm/)
- [Docker Documentation: How Swarm mode works (Raft algorithm)](https://docs.docker.com/engine/swarm/raft/)
- [Docker Documentation: Use Swarm mode routing mesh](https://docs.docker.com/engine/swarm/ingress/)
- [The Raft Consensus Algorithm Explained (Interactive)](https://raft.github.io/)
- [Docker Documentation: Deploy a stack to a swarm](https://docs.docker.com/engine/swarm/stack-deploy/)

---

[<- 11-docker-daemon-configuration-and-troubleshooting.md](./11-docker-daemon-configuration-and-troubleshooting.md) | [Back to Module Overview](./README.md) | [13-from-docker-to-kubernetes.md ->](./13-from-docker-to-kubernetes.md)
