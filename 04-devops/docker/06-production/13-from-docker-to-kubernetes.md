<!--
================================================================================
Metadata:
  Title: From Docker to Kubernetes: Migration, Mapping, and Architecture
  Topic: Docker Production Engineering
  Difficulty: Advanced
  Estimated Time: 50 minutes
  Prerequisites: 05-compose/01-compose-basics.md, 06-production/12-swarm-overview-and-status.md
  Key Terminology: CRI (Container Runtime Interface), OCI (Open Container Initiative), Pod, Deployment, Service, Ingress, PVC, ConfigMap, Dockershim Deprecation
================================================================================
-->

# From Docker to Kubernetes: Migration, Mapping, and Architecture

[<- 12-swarm-overview-and-status.md](./12-swarm-overview-and-status.md) | [Back to Module Overview](./README.md) | [14-anti-patterns-and-common-mistakes.md ->](./14-anti-patterns-and-common-mistakes.md)

---

## Table of Contents
- [1. What & Why](#1-what--why)
- [2. Core Concepts & Mapping Framework](#2-core-concepts--mapping-framework)
- [3. How It Works Internally: Dockershim Deprecation & CRI](#3-how-it-works-internally-dockershim-deprecation--cri)
- [4. Architectural Shifts](#4-architectural-shifts)
- [5. Practical Migration: Docker Compose to Kubernetes](#5-practical-migration-docker-compose-to-kubernetes)
- [6. Best Practices](#6-best-practices)
- [7. Common Mistakes & Troubleshooting](#7-common-mistakes--troubleshooting)
- [8. Performance, Security & Cost Implications](#8-performance-security--cost-implications)
- [9. Comprehensive Concept Comparison Table](#9-comprehensive-concept-comparison-table)
- [10. Real-World Interview Questions](#10-real-world-interview-questions)
- [11. Practice Exercises](#11-practice-exercises)
- [12. Summary & Quick Reference Cheat Sheet](#12-summary--quick-reference-cheat-sheet)
- [13. Further Reading & Official Documentation](#13-further-reading--official-documentation)

---

## 1. What & Why

Docker transformed software delivery by packaging applications and dependencies into immutable container images. However, when moving from a single virtual machine running Docker Compose to a large-scale, multi-zone production cluster, single-host Docker reaches its natural operational boundaries:
1. **Dynamic Scheduling**: Distributing microservices across hundreds of physical compute nodes based on real-time CPU/memory utilization and affinity rules.
2. **Horizontal Autoscaling**: Expanding or contracting replica counts dynamically based on request queue depth or CPU saturation.
3. **Automated Storage Lifecycle**: Dynamically provisioning and attaching cloud block storage volumes (AWS EBS, GCP PD) to moving containers.
4. **Self-Healing & Traffic Gating**: Decoupling container liveness from traffic readiness to prevent black-holing requests during cold starts.

Kubernetes (K8s) is the industry-standard container orchestration platform designed to solve these distributed systems challenges. Transitioning from Docker to Kubernetes requires engineers to translate single-container and Compose abstractions into distributed Kubernetes primitives.

---

## 2. Core Concepts & Mapping Framework

### Conceptual Translation Matrix

| Docker / Docker Compose Concept | Kubernetes Equivalent Primitive | Architectural Scope |
|:--------------------------------|:--------------------------------|:--------------------|
| **Container** | **Pod** (specifically container within Pod) | Atomic unit of scheduling (1 or more tightly coupled containers sharing network & IPC) |
| **Compose `service`** | **Deployment** (stateless) or **StatefulSet** (stateful) | Replicas, rolling update strategy, pod template definition |
| **`restart: always`** | **Deployment Pod Template** + **RestartPolicy** | Managed by ReplicaSet controller reconciling desired state |
| **Global Service (Swarm)** | **DaemonSet** | Exactly one pod per cluster node (e.g., node exporters) |
| **Compose `network`** | **ClusterIP Service** + **CNI Plugin** | Overlay networking, stable internal DNS, virtual IP load balancing |
| **`-p 80:80` (Port Binding)** | **LoadBalancer Service** or **Ingress** | Ingress controller routing external HTTP/HTTPS traffic to internal services |
| **Compose `volume`** | **PersistentVolumeClaim (PVC)** & **StorageClass** | Abstract request for persistent storage fulfilled dynamically by cloud CSI |
| **Bind Mount (`./app:/app`)** | **HostPath Volume** (dev only) or emptyDir | HostPath discouraged in production due to node mobility |
| **`environment:` / `.env`** | **ConfigMap** | Decoupled key-value pairs or configuration files |
| **Compose `secrets:`** | **Secret** | Base64-encoded or KMS-encrypted credentials mounted as tmpfs |
| **Healthcheck (`test: [...]`)**| **Liveness, Readiness, and Startup Probes** | Fine-grained health checking split by lifecycle stage |

---

## 3. How It Works Internally: Dockershim Deprecation & CRI

In early Kubernetes releases (pre-1.24), the Kubernetes `kubelet` communicated with Docker Engine through an internal translation layer called **dockershim**. Because Docker was built as a human-friendly developer tool rather than a headless machine interface, this introduced unnecessary overhead:

```
LEGACY ARCHITECTURE (K8s < 1.24):
[kubelet] ---> [dockershim] ---> [dockerd] ---> [containerd] ---> [containerd-shim] ---> [runc] ---> (Container)

MODERN CRI ARCHITECTURE (K8s 1.24+):
[kubelet] -- (gRPC CRI) --------> [containerd] -----------------> [containerd-shim] ---> [runc] ---> (Container)
          -- (or CRI-O) --------> [CRI-O] ----------------------> [conmon] ----------> [crun] ---> (Container)
```

### Why Docker Images Still Work Unmodified
When Kubernetes removed `dockershim` in version 1.24, many developers feared their Dockerfiles and Docker images would no longer function. **This is completely false.**
- Docker images adhere to the **OCI (Open Container Initiative) Image Specification**.
- CRI runtimes like `containerd` and `CRI-O` pull and execute OCI-compliant images natively.
- Any image built using `docker build`, `docker buildx`, or Kaniko runs flawlessly on Kubernetes. Only the underlying daemon orchestrating runtime execution changed.

---

## 4. Architectural Shifts

### 1. From Containers to Pods
In Docker, the container is the smallest addressable unit. In Kubernetes, the **Pod** is the atomic scheduling unit:
- A Pod encapsulates one or more containers that share the same Linux Network Namespace (same IP address, localhost loopback), IPC namespace, and storage volumes.
- Common pattern: **Sidecar Container** (e.g., main app container + Envoy proxy sidecar + log forwarding agent).

### 2. Tri-Partite Health Probes
Docker provides a single binary health check (`HEALTHCHECK`). Kubernetes splits health monitoring into three distinct mechanisms:
- **Startup Probe**: Determines if slow-starting legacy apps have initialized. Disables liveness and readiness checks until startup succeeds.
- **Liveness Probe**: Determines if the container needs to be restarted (`SIGKILL` followed by fresh spawn).
- **Readiness Probe**: Determines if the pod is ready to accept user network traffic. If failing, the pod is removed from Service endpoints without killing the container.

### 3. Decoupled Service Routing
In Docker Compose, service names resolve directly to container IPs via internal DNS. In Kubernetes:
- Pods are ephemeral; their IP addresses change every time they are rescheduled.
- A **Service** provides a durable, static Virtual IP (ClusterIP) and DNS record (`service-name.namespace.svc.cluster.local`).
- Traffic hitting the Service VIP is load-balanced via kernel iptables or IPVS to the endpoints of healthy Pods.

---

## 5. Practical Migration: Docker Compose to Kubernetes

### Source: Production Docker Compose Stack

```yaml
# docker-compose.yml
version: "3.8"

services:
  web-api:
    image: company/web-api:v1.4.0
    ports:
      - "8080:8080"
    environment:
      - NODE_ENV=production
      - DB_HOST=postgres-db
      - DB_NAME=appdb
    env_file:
      - .env.secret
    depends_on:
      postgres-db:
        condition: service_healthy
    deploy:
      resources:
        limits:
          cpus: "1.0"
          memory: 512M

  postgres-db:
    image: postgres:16-alpine
    environment:
      - POSTGRES_DB=appdb
      - POSTGRES_USER=appuser
      - POSTGRES_PASSWORD=secretpassword
    volumes:
      - db-data:/var/lib/postgresql/data
    healthcheck:
      test: ["CMD-SHELL", "pg_isready -U appuser -d appdb"]
      interval: 5s
      timeout: 5s
      retries: 5

volumes:
  db-data:
```

### Destination: Equivalent Kubernetes Declarative Manifests

#### 1. Configuration & Secrets (`config-secret.yaml`)

```yaml
apiVersion: v1
kind: ConfigMap
metadata:
  name: api-config
data:
  NODE_ENV: "production"
  DB_HOST: "postgres-service"
  DB_NAME: "appdb"
---
apiVersion: v1
kind: Secret
metadata:
  name: db-credentials
type: Opaque
stringData:
  POSTGRES_USER: "appuser"
  POSTGRES_PASSWORD: "secretpassword"
```

#### 2. Stateful Database Deployment & Service (`postgres.yaml`)

```yaml
apiVersion: v1
kind: PersistentVolumeClaim
metadata:
  name: postgres-pvc
spec:
  accessModes:
    - ReadWriteOnce
  resources:
    requests:
      storage: 10Gi
---
apiVersion: apps/v1
kind: StatefulSet
metadata:
  name: postgres-db
spec:
  serviceName: "postgres-service"
  replicas: 1
  selector:
    matchLabels:
      app: postgres-db
  template:
    metadata:
      labels:
        app: postgres-db
    spec:
      containers:
        - name: postgres
          image: postgres:16-alpine
          ports:
            - containerPort: 5432
          env:
            - name: POSTGRES_DB
              valueFrom:
                configMapKeyRef:
                  name: api-config
                  key: DB_NAME
            - name: POSTGRES_USER
              valueFrom:
                secretKeyRef:
                  name: db-credentials
                  key: POSTGRES_USER
            - name: POSTGRES_PASSWORD
              valueFrom:
                secretKeyRef:
                  name: db-credentials
                  key: POSTGRES_PASSWORD
          volumeMounts:
            - name: data
              mountPath: /var/lib/postgresql/data
          readinessProbe:
            exec:
              command: ["sh", "-c", "pg_isready -U appuser -d appdb"]
            initialDelaySeconds: 5
            periodSeconds: 5
      volumes:
        - name: data
          persistentVolumeClaim:
            claimName: postgres-pvc
---
apiVersion: v1
kind: Service
metadata:
  name: postgres-service
spec:
  ports:
    - port: 5432
      targetPort: 5432
  selector:
    app: postgres-db
  clusterIP: None # Headless service for direct StatefulSet resolution
```

#### 3. Stateless Web API Deployment & Service (`web-api.yaml`)

```yaml
apiVersion: apps/v1
kind: Deployment
metadata:
  name: web-api
spec:
  replicas: 3
  selector:
    matchLabels:
      app: web-api
  template:
    metadata:
      labels:
        app: web-api
    spec:
      containers:
        - name: api
          image: company/web-api:v1.4.0
          ports:
            - containerPort: 8080
          envFrom:
            - configMapRef:
                name: api-config
          env:
            - name: DB_PASSWORD
              valueFrom:
                secretKeyRef:
                  name: db-credentials
                  key: POSTGRES_PASSWORD
          resources:
            requests:
              cpu: "250m"
              memory: "256Mi"
            limits:
              cpu: "1000m"
              memory: "512Mi"
          readinessProbe:
            httpGet:
              path: /ready
              port: 8080
            initialDelaySeconds: 5
            periodSeconds: 5
          livenessProbe:
            httpGet:
              path: /live
              port: 8080
            initialDelaySeconds: 15
            periodSeconds: 10
---
apiVersion: v1
kind: Service
metadata:
  name: web-api-service
spec:
  type: ClusterIP
  ports:
    - port: 80
      targetPort: 8080
  selector:
    app: web-api
```

---

## 6. Best Practices

1. **Decouple Configuration from Images**: Never bake configuration into Dockerfiles. Use Kubernetes `ConfigMap` for non-sensitive values and `Secret` (or external Vault / AWS Secrets Manager operators) for credentials.
2. **Always Define Both Requests and Limits**: Kubernetes schedules Pods based on resource **requests** and throttles/kills based on resource **limits**. Omitting requests causes scheduling hot-spots on cluster worker nodes.
3. **Split Liveness and Readiness Endpoints**:
   - `Liveness`: Checks internal application deadlocks (fails if process hung). Fails -> Kubelet restarts container.
   - `Readiness`: Checks external dependency availability (DB, cache). Fails -> Service stops routing traffic; process remains alive to recover.
4. **Use StatefulSets for Databases**: Unlike Deployments which replace pods randomly with ephemeral hostnames, StatefulSets guarantee stable, persistent hostnames (`postgres-0`, `postgres-1`) and ordered volume attachment.
5. **Never Use `latest` Tags**: Always pin immutable semantic version tags or sha256 digests in Kubernetes manifests. `imagePullPolicy: IfNotPresent` ensures deterministic node execution.

---

## 7. Common Mistakes & Troubleshooting

| Symptom / Error | Root Cause | Diagnosis Command | Remediation |
|:----------------|:-----------|:------------------|:------------|
| **`CrashLoopBackOff`** | Application crashed immediately on startup | `kubectl logs <pod> --previous` | Check environment variables, missing secrets, or exit codes. |
| **`ImagePullBackOff` / `ErrImagePull`** | Missing private registry secret or misspelled image tag | `kubectl describe pod <pod>` (Events section) | Create imagePullSecrets (`kubectl create secret docker-registry`). |
| **Traffic drops during rolling update** | Readiness probe missing or `initialDelaySeconds` too short | `kubectl get endpoints <service>` | Configure proper Readiness Probe and `readinessGates`. |
| **Pod stuck in `Pending`** | Insufficient CPU/Memory requests across worker nodes | `kubectl describe pod <pod> \| grep -A 5 Events` | Add worker nodes to cluster or lower Pod resource requests. |
| **Volume fails to attach on pod rescheduling** | AWS EBS volume in AZ `us-east-1a` cannot attach to pod in `us-east-1b` | Check pod events: `Multi-Attach error for volume` | Restrict StatefulSet or use node affinity to pin pod to matching storage AZ. |
| **DNS resolution fails for service name** | CoreDNS failure or namespace mismatch | `kubectl run -it --rm debug --image=busybox -- nslookup <svc>` | Use fully qualified domain name: `<svc>.<namespace>.svc.cluster.local`. |

---

## 8. Performance, Security & Cost Implications

### Performance
In Kubernetes, network overlays (e.g., Calico, Cilium using eBPF) replace standard Docker bridge networks. Using Cilium eBPF host-routing completely bypasses iptables connection tracking, reducing inter-service latency to near bare-metal wire speed.

### Security
Kubernetes supports fine-grained **RBAC (Role-Based Access Control)**, **NetworkPolicies** (pod-to-pod firewalling), and **Pod Security Standards (Restricted Profile)**. Unlike Docker Engine where any user with access to `/var/run/docker.sock` has effective root on the host, Kubernetes access is governed by signed TLS service account tokens.

### Cost
Kubernetes allows dynamic multi-tenant bin-packing and cluster autoscaling (`karpenter` or Cluster Autoscaler), automatically shutting down idle EC2/Compute instances during low traffic periods and spinning up spot instances for batch worker pods.

---

## 9. Comprehensive Concept Comparison Table

| Operational Domain | Docker Compose | Kubernetes |
|:-------------------|:---------------|:-----------|
| **Primary Scope** | Local dev, single-node VM | Enterprise distributed clusters (1 to 5,000+ nodes) |
| **Scheduling Algorithm**| Local Docker Engine execution | Advanced filtering, scoring, affinity, and taints/tolerations |
| **Networking Model**| Host-local bridge NAT / simple overlay | Flat CNI network (Every Pod gets unique routable IP) |
| **Ingress Management**| Port mapping (`-p 80:80`) | Ingress Controller (NGINX, Traefik, AWS ALB Controller) |
| **Autoscaling** | None (manual `scale`) | Horizontal Pod Autoscaler (HPA), KEDA, Karpenter |
| **Configuration Tooling**| Single YAML file | YAML manifests, Helm Charts, Kustomize, Jsonnet |
| **Service Mesh Support**| None | Native integration with Istio, Linkerd, Consul |

---

## 10. Real-World Interview Questions

<details>
<summary><strong>Q1: Why did Kubernetes deprecate dockershim in version 1.24, and does this mean Dockerfiles are obsolete?</strong></summary>

Dockershim was an internal shim inside kubelet created when Docker was the only container engine. Docker was not designed to speak Kubernetes' CRI (Container Runtime Interface) directly; it wrapped containerd with extra tooling, causing duplicate memory overhead and development friction. Deprecating dockershim allowed kubelet to communicate directly with CRI runtimes (`containerd`, `CRI-O`) via gRPC.
Dockerfiles are **not** obsolete. Kubernetes executes OCI-compliant container images; Docker produces OCI-compliant images. A container image built with Docker today runs completely unchanged on modern Kubernetes clusters.
</details>

<details>
<summary><strong>Q2: Explain the architectural difference between a Kubernetes Pod and a Docker Container.</strong></summary>

A Docker container is a single isolated process tree wrapped in Linux namespaces and cgroups. A Kubernetes Pod is a collection of one or more containers that share:
1. The **Network Namespace**: All containers in the Pod share the same IP address and port space (`localhost` communication).
2. The **IPC Namespace**: Containers can communicate via standard POSIX shared memory or message queues.
3. **Storage Volumes**: Volumes defined in the Pod specification can be mounted simultaneously by multiple containers in that Pod.
</details>

<details>
<summary><strong>Q3: What is the difference between a Liveness Probe and a Readiness Probe?</strong></summary>

- **Liveness Probe**: Detects when an application enters an unrecoverable state (e.g., deadlock). When the liveness probe fails, kubelet terminates the container with `SIGKILL` and creates a replacement container according to its `restartPolicy`.
- **Readiness Probe**: Detects when an application is temporarily unable to serve incoming network traffic (e.g., warm-up cache loading, database connection pool exhaustion). When the readiness probe fails, kubelet does *not* restart the container; instead, it removes the Pod's IP from all matching Service endpoints so no new requests are routed to it until it recovers.
</details>

<details>
<summary><strong>Q4: How does service discovery differ between Docker Compose and Kubernetes?</strong></summary>

In Docker Compose, an internal embedded DNS server resolves the service name (e.g., `web`) directly to the container's private bridge IP. In Kubernetes, Pod IPs are ephemeral and change constantly. Kubernetes creates an abstract **Service** with a static Virtual IP (ClusterIP). CoreDNS resolves the service name to this ClusterIP. Traffic directed to the ClusterIP is distributed to the healthy Pod endpoints via kernel-level `iptables` or `IPVS` load balancing managed by `kube-proxy`.
</details>

<details>
<summary><strong>Q5: What is Kompose and what are its limitations when migrating production stacks?</strong></summary>

Kompose (`kompose convert`) is an official Kubernetes conversion tool that translates `docker-compose.yml` files into Kubernetes Deployments, Services, and PVC manifests.
**Limitations**:
1. It produces basic, non-production manifests lacking production readiness/liveness probes.
2. It defaults all persistent volumes to basic PVCs without dynamic StorageClass parameters.
3. It cannot infer complex rolling update strategies, pod anti-affinity rules, security contexts, or ingress controllers. It serves as a scaffolding starting point, not a finished production pipeline.
</details>

<details>
<summary><strong>Q6: Why should you use a StatefulSet instead of a Deployment for database workloads?</strong></summary>

Deployments treat Pods as interchangeable, ephemeral entities: Pod names are randomized (`api-7b89d4-x9q2j`), and scaling creates/terminates pods in arbitrary order. StatefulSets provide:
1. **Stable Network Identifiers**: Predictable names (`db-0`, `db-1`, `db-2`) with matching DNS records.
2. **Ordered Graceful Deployment & Scaling**: Pods are created sequentially ($0 \to 1 \to 2$) and terminated in reverse order.
3. **Dedicated Volume Claims**: Each Pod receives its own distinct PersistentVolumeClaim (`data-db-0`, `data-db-1`) that re-attaches to that specific identity even if the Pod is rescheduled onto a different physical node.
</details>

<details>
<summary><strong>Q7: What is an Ingress Controller and why isn't a NodePort or LoadBalancer Service enough?</strong></summary>

A `NodePort` service exposes a high port (30000–32767) on every node IP, which is non-standard for HTTP/HTTPS web clients. A `LoadBalancer` service creates a cloud load balancer (e.g., AWS NLB), which incurs substantial cloud provider costs if provisioned for every individual microservice.
An **Ingress Controller** (e.g., ingress-nginx, Traefik) provisions a single cloud load balancer for the entire cluster and acts as a Layer 7 reverse proxy, routing traffic based on Host headers (`api.example.com`, `auth.example.com`) and URI paths (`/users`, `/orders`) to internal ClusterIP services while terminating TLS centrally.
</details>

<details>
<summary><strong>Q8: How do you handle environment variables containing secrets when transitioning from Docker to Kubernetes?</strong></summary>

In Docker Compose, secrets are often loaded from local `.env` files or plaintext Compose strings. In Kubernetes:
1. Create a Kubernetes `Secret` containing base64-encoded strings (`kubectl create secret generic`).
2. Reference the secret in the Pod specification using `secretKeyRef` or mount it as a file volume.
3. For enterprise compliance, integrate with external secret managers (HashiCorp Vault, AWS Secrets Manager, GCP Secret Manager) using the **External Secrets Operator (ESO)** or CSI Secret Store Driver, ensuring secrets are dynamically fetched in memory and never stored as plaintext in git repositories.
</details>

---

## 11. Practice Exercises

<details>
<summary><strong>Exercise 1: Convert a Single Docker Run Command to a Pod Manifest</strong></summary>

**Task**: Convert `docker run -d --name cache -p 6379:6379 -m 256m redis:7.4-alpine` into a declarative Kubernetes Pod manifest.

**Solution**:
```yaml
apiVersion: v1
kind: Pod
metadata:
  name: cache
  labels:
    app: cache
spec:
  containers:
    - name: redis
      image: redis:7.4-alpine
      ports:
        - containerPort: 6379
      resources:
        limits:
          memory: "256Mi"
        requests:
          memory: "128Mi"
```
</details>

<details>
<summary><strong>Exercise 2: Write a Complete Deployment with Liveness and Readiness Probes</strong></summary>

**Task**: Create a Kubernetes Deployment for an NGINX image with 2 replicas, an HTTP liveness probe on `/` (interval 10s), and readiness probe on `/` (interval 5s).

**Solution**:
```yaml
apiVersion: apps/v1
kind: Deployment
metadata:
  name: nginx-web
spec:
  replicas: 2
  selector:
    matchLabels:
      app: nginx-web
  template:
    metadata:
      labels:
        app: nginx-web
    spec:
      containers:
        - name: nginx
          image: nginx:1.27-alpine
          ports:
            - containerPort: 80
          livenessProbe:
            httpGet:
              path: /
              port: 80
            initialDelaySeconds: 10
            periodSeconds: 10
          readinessProbe:
            httpGet:
              path: /
              port: 80
            initialDelaySeconds: 3
            periodSeconds: 5
```
</details>

<details>
<summary><strong>Exercise 3: Translating Docker Compose Environment Variables to ConfigMap</strong></summary>

**Task**: Given an `.env` file containing `PORT=8080` and `LOG_LEVEL=info`, create a ConfigMap and write a Pod container spec consuming them via `envFrom`.

**Solution**:
```yaml
apiVersion: v1
kind: ConfigMap
metadata:
  name: app-env
data:
  PORT: "8080"
  LOG_LEVEL: "info"
---
apiVersion: v1
kind: Pod
metadata:
  name: app-demo
spec:
  containers:
    - name: app
      image: alpine
      command: ["sh", "-c", "env && sleep 3600"]
      envFrom:
        - configMapRef:
            name: app-env
```
</details>

<details>
<summary><strong>Exercise 4: Expose a Pod Internally with a ClusterIP Service</strong></summary>

**Task**: Write the Service manifest that exposes the `nginx-web` Deployment from Exercise 2 on port 80 across the cluster.

**Solution**:
```yaml
apiVersion: v1
kind: Service
metadata:
  name: nginx-service
spec:
  type: ClusterIP
  selector:
    app: nginx-web
  ports:
    - protocol: TCP
      port: 80
      targetPort: 80
```
</details>

<details>
<summary><strong>Exercise 5: Simulate Kompose Conversion on Docker Compose File</strong></summary>

**Task**: Show how to inspect a Docker Compose file and run `kompose` to output Kubernetes YAML files.

**Solution**:
```bash
# 1. Install kompose (Linux/WSL)
# curl -L https://github.com/kubernetes/kompose/releases/download/v1.34.0/kompose-linux-amd64 -o kompose
# chmod +x kompose

# 2. Run conversion dry-run to stdout
kompose convert -f docker-compose.yml --stdout

# 3. Generate distinct manifest files
kompose convert -f docker-compose.yml -o ./k8s-manifests/
```
</details>

<details>
<summary><strong>Exercise 6: Map a Docker Volume to a Kubernetes PersistentVolumeClaim</strong></summary>

**Task**: Define a 20Gi `PersistentVolumeClaim` named `app-storage` with `ReadWriteOnce` access and standard storage class.

**Solution**:
```yaml
apiVersion: v1
kind: PersistentVolumeClaim
metadata:
  name: app-storage
spec:
  accessModes:
    - ReadWriteOnce
  storageClassName: standard
  resources:
    requests:
      storage: 20Gi
```
</details>

---

## 12. Summary & Quick Reference Cheat Sheet

### Core Mapping Rules
- **Docker Image $\to$ OCI Image**: Compatible 1:1; build with `docker build`, run on K8s.
- **Docker Container $\to$ Pod**: Smallest scheduled entity; pods can hold sidecars.
- **Docker Compose Service $\to$ Deployment**: Manages replica counts and rolling updates.
- **Docker Compose Network $\to$ Service (ClusterIP)**: Provides stable internal virtual IP and DNS name.
- **Docker Compose Volume $\to$ PersistentVolumeClaim (PVC)**: Decoupled volume provisioning.
- **Docker Compose Port Binding $\to$ Ingress / LoadBalancer**: Layer 7 routing into cluster.

### CLI Equivalence Quick Reference

```bash
# Docker CLI                         # Kubernetes (kubectl)
docker run <image>                   kubectl run <pod> --image=<image>
docker ps                            kubectl get pods
docker ps -a                         kubectl get pods -A
docker logs -f <id>                  kubectl logs -f <pod>
docker exec -it <id> sh              kubectl exec -it <pod> -- sh
docker stop <id>                     kubectl delete pod <pod>
docker inspect <id>                  kubectl describe pod <pod>
docker-compose up -d                 kubectl apply -f <manifests_dir>/
docker-compose down                  kubectl delete -f <manifests_dir>/
```

---

## 13. Further Reading & Official Documentation

- [Kubernetes Official Documentation: Don't Panic: Kubernetes and Docker](https://kubernetes.io/blog/2020/12/02/dont-panic-kubernetes-and-docker/)
- [Kubernetes Documentation: Container Runtime Interface (CRI)](https://kubernetes.io/docs/concepts/architecture/cri/)
- [Kompose: Translate Docker Compose to Kubernetes](https://kompose.io/)
- [Open Container Initiative (OCI) Specifications](https://opencontainers.org/)
- [Kubernetes Documentation: Pod Lifecycle and Probes](https://kubernetes.io/docs/concepts/workloads/pods/pod-lifecycle/)

---

[<- 12-swarm-overview-and-status.md](./12-swarm-overview-and-status.md) | [Back to Module Overview](./README.md) | [14-anti-patterns-and-common-mistakes.md ->](./14-anti-patterns-and-common-mistakes.md)
