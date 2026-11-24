---
title: "Kubernetes Deployment and Probes for FastAPI"
description: "Deploying production FastAPI services on Kubernetes: Liveness, Readiness, and Startup probes, resource limits, HPA autoscaling, and PDB disruptions."
fastapi_version: "0.115+"
kubernetes_version: "1.30+"
python_version: "3.12+"
free_threading_compatible: true
tested_with: "Kubernetes 1.31, FastAPI 0.115.6, Uvicorn 0.32.0"
---

# Kubernetes Deployment and Probes for FastAPI

## 1. Conceptual Foundation & Mental Model

Deploying FastAPI services into Kubernetes (K8s) requires transitioning from traditional single-host process management to distributed container orchestration. In Kubernetes, applications run inside transient, immutable Pods governed by controllers (Deployments, StatefulSets). The Kubernetes control plane manages container scheduling, scaling, traffic ingress routing, rolling updates, and self-healing.

The mental model for Kubernetes-native FastAPI services centers on **Orchestrator-Application Symbiosis**:
1. **The Three Probes (Lifecycle Contracts)**:
   - **Startup Probe**: Informs Kubernetes whether slow initialization (database migrations, ML model weights loading, cache warming) is still in progress.
   - **Readiness Probe**: Informs Kubernetes whether the pod is currently capable of handling live user traffic. If ready, the pod's IP is added to the Service's Endpoints; if not ready, it is temporarily pulled from traffic without killing the container.
   - **Liveness Probe**: Informs Kubernetes whether the application has deadlocked or frozen permanently. If the liveness probe fails, the kubelet kills the container and restarts it.
2. **Resource Boundaries & Quality of Service (QoS)**: Defining exact CPU and memory `requests` and `limits` prevents a single noisy neighbor pod from starving other containers and triggering node-level kernel out-of-memory panics.
3. **Horizontal Pod Autoscaling (HPA)**: Dynamically scaling the number of pod replicas based on real-time metrics (CPU utilization, request latency, or external queue depth).

```
Kubernetes Ingress / Service
            │
            ├── Pod 1 [Ready: YES, Live: YES] ──► Receives Live User Traffic
            │
            ├── Pod 2 [Ready: NO,  Live: YES] ──► Saturated / Warming Up (Traffic Halted)
            │
            └── Pod 3 [Ready: NO,  Live: NO ] ──► Deadlocked! Kubelet executes SIGKILL
```

---

## 2. Architecture & Internal Mechanics

```
+---------------------------------------------------------------+
|                      Kubernetes Node                          |
|                                                               |
|  +---------------------------------------------------------+  |
|  |                    Kubelet Agent                        |  |
|  | - Periodically queries HTTP probe endpoints             |  |
|  | - Evaluates failureThreshold & initialDelaySeconds      |  |
|  +-----------------------------│---------------------------+  |
|                                │                              |
|                                ▼ HTTP GET /health/ready       |
|  +---------------------------------------------------------+  |
|  |                 FastAPI Application Pod                 |  |
|  |  +-----------------------+   +-----------------------+  |  |
|  |  | /health/startup       |   | /health/ready         |  |  |
|  |  | Returns 200 once DB   |   | Pings DB & Redis with |  |  |
|  |  | schemas are verified  |   | 1-second timeout      |  |  |
|  |  +-----------------------+   +-----------------------+  |  |
|  |  +-----------------------+                              |  |
|  |  | /health/live          |                              |  |
|  |  | Fast event loop check |                              |  |
|  |  | Returns 200 OK        |                              |  |
|  |  +-----------------------+                              |  |
|  +---------------------------------------------------------+  |
+---------------------------------------------------------------+
```

### The Pitfall of Probe Coupling
A catastrophic anti-pattern is checking external shared dependencies (like the primary PostgreSQL database) inside the **liveness** probe. If PostgreSQL experiences a temporary network hiccup or 10-second failover:
1. Every pod's liveness probe fails simultaneously.
2. Kubelet forcefully terminates and restarts every FastAPI pod in the cluster at the exact same moment.
3. When the pods reboot simultaneously, they slam PostgreSQL with a SYN connection storm, compounding the outage.

**The Golden Rule**: Liveness probes must only verify that the local Python process and event loop are responsive. Readiness probes check external dependency connectivity.

---

## 3. Comprehensive Syntax & Core Mechanics

### FastAPI Health Probe Endpoints
```python
from fastapi import FastAPI, Response, status
from sqlalchemy import text
from database import engine

app = FastAPI()

@app.get("/health/live", status_code=status.HTTP_200_OK)
async def liveness_probe():
    # Only verify that the local event loop is actively scheduling coroutines
    return {"status": "ALIVE"}

@app.get("/health/ready")
async def readiness_probe():
    # Verify downstream dependencies can serve requests
    try:
        async with engine.connect() as conn:
            await conn.execute(text("SELECT 1"))
        return {"status": "READY"}
    except Exception as exc:
        return Response(
            content=f'{{"status": "NOT_READY", "error": "{str(exc)}"}}',
            media_type="application/json",
            status_code=status.HTTP_503_SERVICE_UNAVAILABLE
        )
```

---

## 4. Practical Implementation: 5 Production Scenarios

### Scenario 1: Complete Kubernetes Deployment Manifest with Hardened SecurityContext
```yaml
apiVersion: apps/v1
kind: Deployment
metadata:
  name: fastapi-service
  namespace: production
  labels:
    app.kubernetes.io/name: fastapi-service
    app.kubernetes.io/part-of: backend-platform
spec:
  replicas: 4
  revisionHistoryLimit: 5
  strategy:
    type: RollingUpdate
    rollingUpdate:
      maxSurge: 25%
      maxUnavailable: 0
  selector:
    matchLabels:
      app: fastapi-service
  template:
    metadata:
      labels:
        app: fastapi-service
    spec:
      securityContext:
        runAsNonRoot: true
        runAsUser: 10001
        runAsGroup: 10001
        fsGroup: 10001
      containers:
        - name: api
          image: myregistry.internal/fastapi-app:v1.4.0
          imagePullPolicy: IfNotPresent
          securityContext:
            readOnlyRootFilesystem: true
            allowPrivilegeEscalation: false
            capabilities:
              drop: ["ALL"]
          ports:
            - containerPort: 8000
              name: http
          resources:
            requests:
              cpu: "500m"
              memory: "512Mi"
            limits:
              cpu: "2000m"
              memory: "1024Mi"
          startupProbe:
            httpGet:
              path: /health/startup
              port: http
            initialDelaySeconds: 5
            periodSeconds: 5
            failureThreshold: 12 # Gives up to 60s for cold start
          readinessProbe:
            httpGet:
              path: /health/ready
              port: http
            periodSeconds: 5
            timeoutSeconds: 2
            successThreshold: 1
            failureThreshold: 2
          livenessProbe:
            httpGet:
              path: /health/live
              port: http
            periodSeconds: 10
            timeoutSeconds: 2
            failureThreshold: 3
          volumeMounts:
            - name: tmp-dir
              mountPath: /tmp
      volumes:
        - name: tmp-dir
          emptyDir: {}
```

### Scenario 2: Horizontal Pod Autoscaler (HPA) Sizing
```yaml
apiVersion: autoscaling/v2
kind: HorizontalPodAutoscaler
metadata:
  name: fastapi-hpa
  namespace: production
spec:
  scaleTargetRef:
    apiVersion: apps/v1
    kind: Deployment
    name: fastapi-service
  minReplicas: 3
  maxReplicas: 20
  metrics:
    - type: Resource
      resource:
        name: cpu
        target:
          type: Utilization
          averageUtilization: 70
    - type: Resource
      resource:
        name: memory
        target:
          type: Utilization
          averageUtilization: 80
  behavior:
    scaleDown:
      stabilizationWindowSeconds: 300 # Prevent flapping during intermittent traffic dips
      policies:
        - type: Percent
          value: 10
          periodSeconds: 60
    scaleUp:
      stabilizationWindowSeconds: 0
      policies:
        - type: Percent
          value: 50
          periodSeconds: 15
```

### Scenario 3: PodDisruptionBudget (PDB) to Guard High Availability
```yaml
apiVersion: policy/v1
kind: PodDisruptionBudget
metadata:
  name: fastapi-pdb
  namespace: production
spec:
  minAvailable: 75%
  selector:
    matchLabels:
      app: fastapi-service
```

### Scenario 4: PreStop Hook for Graceful Drain Before SIGTERM
```yaml
# In Deployment container spec:
lifecycle:
  preStop:
    exec:
      command: ["/bin/sh", "-c", "sleep 5"]
# sleep 5 ensures Kubernetes Service endpoint removal propagates
# to all kube-proxies and Ingress controllers before Uvicorn stops accepting requests.
```

### Scenario 5: Kubernetes Service Definition
```yaml
apiVersion: v1
kind: Service
metadata:
  name: fastapi-service
  namespace: production
spec:
  type: ClusterIP
  selector:
    app: fastapi-service
  ports:
    - protocol: TCP
      port: 80
      targetPort: http
```

---

## 5. Edge Cases, Pitfalls & Failure Modes

1. **Race Conditions During Rolling Deploys (Dropped Connections)**:
   When a pod is terminated during a rolling update, Kubernetes simultaneously removes the pod from the Service endpoints and sends `SIGTERM` to the container. Because endpoint removal propagation across all nodes takes 1-3 seconds, ingress controllers may continue routing new requests to a pod that has already begun shutting down. Implementing a `preStop` hook with `sleep 5` ensures network routing updates complete before Uvicorn initiates shutdown.

2. **CPU Throttling from Overly Restrictive Limits**:
   Linux Completely Fair Scheduler (CFS) enforces CPU limits by throttling container threads within 100ms periods. If CPU limits are set too tight (e.g. `cpu: 200m`), Pydantic validation or JSON parsing can trigger severe CFS throttling, causing p99 latency to spike from 10ms to 800ms. Sizing CPU limits generously or omitting them entirely while setting accurate `requests` prevents throttling.

3. **Read-Only Root Filesystem Failures**:
   Hardening pods with `readOnlyRootFilesystem: true` will cause Python or temporary file operations (like `tempfile.NamedTemporaryFile`) to fail unless an explicit `emptyDir` volume is mounted at `/tmp`.

4. **Flapping Readiness on Transient Latency**:
   If a readiness probe has `timeoutSeconds: 1` and `failureThreshold: 1`, a momentary 1,100ms query latency will immediately pull the pod from service, forcing remaining pods to handle more load and causing a cascading cluster-wide outage. Set `failureThreshold: 2` or `3`.

---

## 6. Anti-Patterns & Common Mistakes

| Anti-Pattern | Operational Impact | Correct Pattern |
| :--- | :--- | :--- |
| Checking database in `livenessProbe` | DB hiccup causes all pods to restart simultaneously | Only check local process responsiveness in liveness probe |
| Omitting `resources.requests` | Kube-scheduler cannot make intelligent node placement decisions | Always specify CPU and memory `requests` and `limits` |
| Setting `maxUnavailable: 50%` with few replicas | High proportion of traffic dropped during rolling updates | Set `maxUnavailable: 0` and `maxSurge: 25%` |
| Omitting `preStop: sleep 5` | Dropped TCP connections during pod termination | Add `preStop` hook to allow kube-proxy iptables propagation |
| Omitting `PodDisruptionBudget` | Node drain/maintenance can terminate all pods simultaneously | Define a PDB enforcing `minAvailable: 50%` or higher |
| Using overly aggressive HPA scale-down | Rapid traffic fluctuations trigger pod thrashing and cold starts | Set `stabilizationWindowSeconds: 300` on scale-down |

---

## 7. Performance & Scalability Considerations

- **Single Worker per Pod in K8s**: In Kubernetes, run standalone Uvicorn with `workers: 1` per pod. Horizontal Pod Autoscaler handles scaling much more effectively than internal process managers, providing isolated memory heaps and preventing single-pod failures from affecting multiple workers.
- **Node Affinity & Anti-Affinity**: Configure `podAntiAffinity` so that pod replicas are scheduled across different Kubernetes worker nodes and Availability Zones (AZs) for high availability:
```yaml
affinity:
  podAntiAffinity:
    preferredDuringSchedulingIgnoredDuringExecution:
      - weight: 100
        podAffinityTerm:
          labelSelector:
            matchExpressions:
              - key: app
                operator: In
                values: ["fastapi-service"]
          topologyKey: "topology.kubernetes.io/zone"
```

---

## 8. Security & OWASP Guardrails

- **Enforce Non-Root Execution**: Always declare `securityContext.runAsNonRoot: true` and specify a dedicated UID/GID.
- **Drop Kernel Capabilities**: Explicitly drop all Linux capabilities (`capabilities: drop: ["ALL"]`) to prevent privilege escalation.
- **Network Policies**: Implement Kubernetes `NetworkPolicy` to restrict ingress traffic to known ingress controllers and deny direct pod-to-pod traffic from untrusted namespaces.

---

## 9. Testing Strategies & Pytest Recipes

```python
import pytest
from httpx import AsyncClient, ASGITransport
from main import app

@pytest.mark.asyncio
async def test_k8s_probes():
    transport = ASGITransport(app=app)
    async with AsyncClient(transport=transport, base_url="http://test") as client:
        # 1. Test Liveness Probe (Never fails unless app crashed)
        live_resp = await client.get("/health/live")
        assert live_resp.status_code == 200
        assert live_resp.json() == {"status": "ALIVE"}

        # 2. Test Readiness Probe
        ready_resp = await client.get("/health/ready")
        assert ready_resp.status_code == 200
        assert ready_resp.json()["status"] == "READY"
```

---

## 10. Real-World Production Incident

**Incident**: During a Kubernetes cluster node upgrade, a financial services API suffered a 4-minute full service outage, dropping thousands of user checkout requests.

**Root Cause**: The engineering team configured the deployment with `replicas: 3` but did not define a `PodDisruptionBudget`. Additionally, their `livenessProbe` queried the primary database with `timeoutSeconds: 1` and `failureThreshold: 1`. When the node drain evicted Pod 1, the temporary load surge caused database query latency to hit 1.2s. Pods 2 and 3 failed their liveness probes and were terminated simultaneously by the kubelet, leaving zero active replicas in the cluster.

**Resolution**:
1. Implemented a `PodDisruptionBudget` with `minAvailable: 2`.
2. Decoupled the database check from `livenessProbe`, keeping it strictly within `readinessProbe`.
3. Increased `readinessProbe` failure threshold to 3 and timeout to 2 seconds.
Subsequent node upgrades and maintenance operations completed with 100% availability.

---

## 11. Technical Interview Questions

<details>
<summary>1. What is the critical distinction between a Liveness Probe and a Readiness Probe in Kubernetes?</summary>

- **Liveness Probe**: Determines if the container process is alive and healthy. If it fails, Kubernetes kills the container and initiates a restart. It should only test that the application process is not deadlocked.
- **Readiness Probe**: Determines if the container is currently able to accept network traffic. If it fails, Kubernetes removes the pod's IP from the Service endpoints without restarting the container, shielding the pod until it recovers. It tests external dependencies and capacity.
</details>

<details>
<summary>2. Why is checking a shared database in a Liveness Probe an architectural anti-pattern?</summary>

If the shared database slows down or fails over, every pod's liveness probe will fail simultaneously. Kubelet will kill and restart every pod in the entire cluster at once. When all pods restart together, they create a massive connection storm against the recovering database, worsening the outage (cascading failure).
</details>

<details>
<summary>3. Why is a <code>preStop</code> hook with a sleep command recommended for FastAPI deployments?</summary>

When a pod is deleted, the endpoint removal in Kubernetes is asynchronous and takes a few seconds to propagate through the network (kube-proxy, iptables, ingress controllers). Without a `preStop` hook, Uvicorn terminates immediately upon receiving `SIGTERM`, but ingress controllers may still route requests to the pod for another 1-2 seconds, resulting in HTTP 502 Bad Gateway errors. A `preStop: exec: command: ["sleep", "5"]` allows traffic routing to drain cleanly before Uvicorn initiates shutdown.
</details>

<details>
<summary>4. What does a <code>PodDisruptionBudget</code> (PDB) do?</summary>

A PDB guarantees that a minimum number or percentage of pod replicas remain running and healthy during voluntary disruptions (such as node drains, cluster upgrades, or auto-scaling down). It prevents Kubernetes from evicting too many pods at once.
</details>

<details>
<summary>5. How does Horizontal Pod Autoscaler (HPA) decide when to add or remove pods?</summary>

HPA monitors resource metrics (like CPU or memory utilization) and calculates desired replicas using the formula:
$$\text{Desired Replicas} = \left\lceil \text{Current Replicas} \times \left( \frac{\text{Current Metric Value}}{\text{Target Metric Value}} \right) \right\rceil$$
If current CPU utilization is 80% and the target is 50% across 5 pods, HPA scales up to $\lceil 5 \times (80 / 50) \rceil = 8$ pods.
</details>

<details>
<summary>6. What is CFS CPU throttling and how does it impact FastAPI services in Kubernetes?</summary>

Linux Completely Fair Scheduler (CFS) enforces container CPU limits using time slices (typically 100ms periods). If a container exceeds its allocated quota within that period, CFS freezes all container threads until the next period. In FastAPI, this causes severe tail-latency spikes during CPU-intensive tasks (e.g. JSON parsing or crypto hashing) even when overall node CPU is idle.
</details>

<details>
<summary>7. What is the role of a Startup Probe in modern Kubernetes versions?</summary>

A Startup Probe disables liveness and readiness checks until the container has finished its initial startup sequence. This is essential for applications that require long initialization times (e.g., loading large ML model weights or warming extensive caches) without risking premature termination by aggressive liveness probes.
</details>

<details>
<summary>8. Why should you prefer 1 Uvicorn worker per container in Kubernetes instead of multiple workers?</summary>

In Kubernetes, pod replicas are the fundamental unit of scaling, monitoring, and resource allocation. Running 1 worker per container provides complete CPU and memory isolation, simplifies HPA metric tracking, prevents noisy-neighbor worker contention inside a pod, and allows Kubernetes to reschedule single crashed workers without affecting others.
</details>

---

## 12. Practical Exercises & Solutions

### Exercise 1 (Easy): Create Liveness and Readiness Endpoints
**Task**: Write two FastAPI routes `/live` and `/ready` conforming to Kubernetes probe best practices.
<details>
<summary>Solution</summary>

```python
from fastapi import FastAPI, Response, status

app = FastAPI()

@app.get("/live")
async def live():
    return {"status": "ok"}

@app.get("/ready")
async def ready():
    # Perform lightweight internal check
    is_ready = True
    if is_ready:
        return {"status": "ready"}
    return Response(status_code=status.HTTP_503_SERVICE_UNAVAILABLE)
```
</details>

### Exercise 2 (Easy): Define Kubernetes Service Manifest
**Task**: Write a Kubernetes Service manifest for `fastapi-service` routing port 80 to container port 8000.
<details>
<summary>Solution</summary>

```yaml
apiVersion: v1
kind: Service
metadata:
  name: fastapi-service
spec:
  type: ClusterIP
  selector:
    app: fastapi-service
  ports:
    - name: http
      port: 80
      targetPort: 8000
```
</details>

### Exercise 3 (Medium): Configure Kubernetes Probes in YAML
**Task**: Write the container probe configuration block for a FastAPI pod including Startup, Liveness, and Readiness probes.
<details>
<summary>Solution</summary>

```yaml
startupProbe:
  httpGet:
    path: /health/startup
    port: 8000
  failureThreshold: 30
  periodSeconds: 2

readinessProbe:
  httpGet:
    path: /health/ready
    port: 8000
  periodSeconds: 5
  timeoutSeconds: 2
  failureThreshold: 2

livenessProbe:
  httpGet:
    path: /health/live
    port: 8000
  periodSeconds: 10
  timeoutSeconds: 2
  failureThreshold: 3
```
</details>

### Exercise 4 (Medium): Pod Disruption Budget Manifest
**Task**: Write a `PodDisruptionBudget` manifest ensuring at least 2 replicas of `fastapi-app` are available at all times.
<details>
<summary>Solution</summary>

```yaml
apiVersion: policy/v1
kind: PodDisruptionBudget
metadata:
  name: fastapi-pdb
spec:
  minAvailable: 2
  selector:
    matchLabels:
      app: fastapi-app
```
</details>

### Exercise 5 (Hard): Complete Resilient Deployment with Anti-Affinity
**Task**: Write a complete Deployment manifest including resource requests/limits, securityContext, preStop sleep hook, and pod anti-affinity across availability zones.
<details>
<summary>Solution</summary>

```yaml
apiVersion: apps/v1
kind: Deployment
metadata:
  name: fastapi-hardened
spec:
  replicas: 3
  selector:
    matchLabels:
      app: fastapi-hardened
  template:
    metadata:
      labels:
        app: fastapi-hardened
    spec:
      affinity:
        podAntiAffinity:
          preferredDuringSchedulingIgnoredDuringExecution:
            - weight: 100
              podAffinityTerm:
                labelSelector:
                  matchExpressions:
                    - key: app
                      operator: In
                      values: ["fastapi-hardened"]
                topologyKey: "topology.kubernetes.io/zone"
      securityContext:
        runAsNonRoot: true
        runAsUser: 10001
      containers:
        - name: app
          image: myregistry/fastapi:v1.0
          securityContext:
            readOnlyRootFilesystem: true
            allowPrivilegeEscalation: false
          resources:
            requests:
              cpu: "250m"
              memory: "256Mi"
            limits:
              cpu: "1000m"
              memory: "512Mi"
          lifecycle:
            preStop:
              exec:
                command: ["/bin/sh", "-c", "sleep 5"]
          ports:
            - containerPort: 8000
          volumeMounts:
            - name: tmp
              mountPath: /tmp
      volumes:
        - name: tmp
          emptyDir: {}
```
</details>

### Exercise 6 (Hard): Readiness Probe with Dependency Circuit Breaker
**Task**: Implement a FastAPI readiness probe that tests database and Redis connections with strict 500ms timeouts; if either fails 3 consecutive times, mark the pod not ready for 15 seconds without executing further connection queries.
<details>
<summary>Solution</summary>

```python
import time
import asyncio
from fastapi import FastAPI, Response, status

app = FastAPI()

class DependencyHealthMonitor:
    def __init__(self):
        self.consecutive_failures = 0
        self.cooldown_until = 0.0

    async def check_dependencies(self) -> bool:
        now = time.time()
        if now < self.cooldown_until:
            return False

        try:
            # Simulate DB and Redis checks with strict 500ms timeout
            async with asyncio.timeout(0.5):
                await asyncio.sleep(0.05) # Simulated fast check
            self.consecutive_failures = 0
            return True
        except Exception:
            self.consecutive_failures += 1
            if self.consecutive_failures >= 3:
                self.cooldown_until = now + 15.0 # 15s cooldown
            return False

HEALTH_MONITOR = DependencyHealthMonitor()

@app.get("/health/ready")
async def readiness_check():
    is_healthy = await HEALTH_MONITOR.check_dependencies()
    if is_healthy:
        return {"status": "READY"}
    return Response(
        content='{"status": "UNHEALTHY"}',
        media_type="application/json",
        status_code=status.HTTP_503_SERVICE_UNAVAILABLE
    )
```
</details>
