# Kubernetes Deployment Architecture for Spring Boot

| Metadata | Value |
|---|---|
| **Spring Boot Version** | 3.4.x |
| **Spring Framework Version** | 6.2.x |
| **Java Version** | 21 LTS |
| **Module** | 14-production-and-deployment |
| **Status** | Complete |

---

## Table of Contents
- [1. What & Why](#1-what--why)
- [2. Core Concepts](#2-core-concepts)
- [3. How It Works Internally](#3-how-it-works-internally)
- [4. Syntax & Manifest Reference](#4-syntax--manifest-reference)
- [5. Practical Examples](#5-practical-examples)
- [6. Best Practices & Production Guidelines](#6-best-practices--production-guidelines)
- [7. Common Mistakes & Pitfalls](#7-common-mistakes--pitfalls)
- [8. Performance & Security Notes](#8-performance--security-notes)
- [9. Comparison / When to Use What](#9-comparison--when-to-use-what)
- [10. Interview Questions & Answers](#10-interview-questions--answers)
- [11. Practice Exercises](#11-practice-exercises)
- [12. Summary / Cheat Sheet](#12-summary--cheat-sheet)
- [13. Further Reading](#13-further-reading)

---

## 1. What & Why

### Problem Statement
Deploying a Spring Boot container to Kubernetes using a naive single-pod manifest is a recipe for production downtime:
- Rolling updates drop live in-flight user transactions if `preStop` hooks and graceful shutdown timeouts are absent.
- Cold JVM startup times cause Kubernetes liveness probes to prematurely kill healthy pods.
- Sudden node drain events during cluster upgrades take down all application replicas simultaneously if a `PodDisruptionBudget` is missing.
- Unconstrained CPU limits introduce Linux kernel CFS throttling, causing artificial 50ms latency spikes.

### Historical Evolution
In early Kubernetes deployments, developers treated containers like traditional virtual machines, pointing generic health checks at `/actuator/health` and relying on default rolling update parameters.

Modern **Spring Boot 3.4** on **Kubernetes 1.28+** provides deep cloud-native integration:
- Dedicated probe groups (`/actuator/health/liveness`, `/actuator/health/readiness`).
- Native graceful shutdown coordination (`server.shutdown=graceful`).
- Horizontal Pod Autoscaling (HPA v2) driven by both hardware metrics (CPU/RAM) and application-level metrics (HTTP request rate via Prometheus Adapter).

### Production Relevance
A production-grade Kubernetes architecture guarantees:
- **Zero-Downtime Rolling Updates**: Traffic is seamlessly routed to new pods only when readiness checks confirm full bean initialization.
- **High Availability**: `PodDisruptionBudget` ensures quorum during voluntary node upgrades and maintenance.
- **Elastic Autoscaling**: HPA scales pods from 3 to 30 replicas within 60 seconds of traffic spikes.

---

## 2. Core Concepts

### Primary Kubernetes Resource Manifests
1. **Deployment**: Manages declarative replica scaling, rolling updates (`maxSurge`, `maxUnavailable`), and container pod lifecycle.
2. **Service (ClusterIP)**: Stable internal virtual IP providing Layer 4 load balancing across healthy pod replicas.
3. **Ingress**: Layer 7 edge routing, TLS termination, path-based routing, and CORS header enforcement.
4. **HorizontalPodAutoscaler (HPA v2)**: Automatically adjusts replica count based on CPU, memory, or custom Prometheus metrics.
5. **PodDisruptionBudget (PDB)**: Enforces a minimum number of available replicas during voluntary disruptions (e.g. `kubectl drain`).
6. **ConfigMap & Secret**: Injects environment properties and encrypted credentials directly into pod containers.
7. **SecurityContext**: Hardens containers by enforcing unprivileged non-root users (`runAsNonRoot: true`), dropping Linux capabilities, and mounting a read-only root filesystem.

---

## 3. How It Works Internally

### Kubernetes Pod Lifecycle & Traffic Routing Transition

```
KUBERNETES ROLLING UPDATE TIMELINE
+------------------------------------------------------------------------------------+
| 1. NEW POD SCHEDULED (v2.0)                                                        |
|    - Image pulled, container starts                                                |
|    - Startup Probe polls /actuator/health/liveness                                 |
|    - Pod is in 'Running' state, but NOT in Service Endpoints (Zero traffic sent!)  |
+------------------------------------------------------------------------------------+
                                         |
                                         v
+------------------------------------------------------------------------------------+
| 2. READINESS PROBE TRANSITION                                                      |
|    - Spring Boot finishes Flyway migrations and publishes ApplicationReadyEvent   |
|    - Readiness Probe returns HTTP 200 OK (/actuator/health/readiness)              |
|    - Kube-Proxy adds Pod IP to Service Endpoints slice                             |
|    - Ingress routes live user traffic to new v2.0 Pod!                             |
+------------------------------------------------------------------------------------+
                                         |
                                         v
+------------------------------------------------------------------------------------+
| 3. OLD POD TERMINATION (v1.0)                                                      |
|    - Kubernetes sends SIGTERM & triggers preStop sleep hook (15s)                  |
|    - Endpoint controller removes Old Pod IP from Service Endpoints                 |
|    - Ingress stops routing new connections to Old Pod                              |
|    - Spring Boot drains in-flight requests (server.shutdown=graceful, timeout=30s) |
|    - JVM exits cleanly (exit code 0); Old Pod destroyed                            |
+------------------------------------------------------------------------------------+
```

---

## 4. Syntax & Manifest Reference

### Complete Production Kubernetes Manifest Suite (`k8s-deployment.yaml`)

```yaml
# 1. ConfigMap for Externalized Properties
apiVersion: v1
kind: ConfigMap
metadata:
  name: order-service-config
data:
  SPRING_PROFILES_ACTIVE: "prod"
  MANAGEMENT_SERVER_PORT: "9090"
---
# 2. Production Deployment
apiVersion: apps/v1
kind: Deployment
metadata:
  name: order-service
  labels:
    app.kubernetes.io/name: order-service
spec:
  replicas: 3
  strategy:
    type: RollingUpdate
    rollingUpdate:
      maxSurge: 25% # Max 1 new pod above replica count during update
      maxUnavailable: 0 # Never allow available replicas to drop below target!
  selector:
    matchLabels:
      app: order-service
  template:
    metadata:
      labels:
        app: order-service
    spec:
      terminationGracePeriodSeconds: 60 # Allows 60s for graceful shutdown
      securityContext:
        runAsNonRoot: true
        runAsUser: 10001
        runAsGroup: 10001
        fsGroup: 10001
      containers:
        - name: app
          image: registry.internal/apps/order-service:1.2.0
          imagePullPolicy: IfNotPresent
          securityContext:
            allowPrivilegeEscalation: false
            readOnlyRootFilesystem: true
            capabilities:
              drop: [ "ALL" ]
          ports:
            - name: http
              containerPort: 8080
            - name: management
              containerPort: 9090
          envFrom:
            - configMapRef:
                name: order-service-config
          env:
            - name: DATABASE_PASSWORD
              valueFrom:
                secretKeyRef:
                  name: order-service-secrets
                  key: db-password
          resources:
            requests:
              cpu: "1000m"
              memory: "2048Mi"
            limits:
              cpu: "2000m"
              memory: "2048Mi"
          volumeMounts:
            - name: tmp-volume
              mountPath: /tmp # Required when readOnlyRootFilesystem: true
          # Startup Probe (Protects cold JVM boots up to 60s)
          startupProbe:
            httpGet:
              path: /actuator/health/liveness
              port: 9090
            failureThreshold: 30
            periodSeconds: 2
          # Liveness Probe (Restarts container ONLY on internal deadlocks)
          livenessProbe:
            httpGet:
              path: /actuator/health/liveness
              port: 9090
            initialDelaySeconds: 5
            periodSeconds: 10
            timeoutSeconds: 3
            failureThreshold: 3
          # Readiness Probe (Controls traffic ingress routing)
          readinessProbe:
            httpGet:
              path: /actuator/health/readiness
              port: 9090
            initialDelaySeconds: 5
            periodSeconds: 5
            timeoutSeconds: 2
            failureThreshold: 2
          lifecycle:
            preStop:
              exec:
                # Wait 15s to allow Kube-Proxy and Ingress to withdraw pod from endpoints
                command: [ "sh", "-c", "sleep 15" ]
      volumes:
        - name: tmp-volume
          emptyDir: {}
```

---

## 5. Practical Examples

### Example 1: Service and Ingress Manifests (`k8s-service-ingress.yaml`)
```yaml
# 1. ClusterIP Internal Service
apiVersion: v1
kind: Service
metadata:
  name: order-service
  labels:
    app: order-service
spec:
  type: ClusterIP
  ports:
    - name: http
      port: 80
      targetPort: 8080
    - name: management
      port: 9090
      targetPort: 9090
  selector:
    app: order-service
---
# 2. Ingress with TLS Termination and Path Routing
apiVersion: networking.k8s.io/v1
kind: Ingress
metadata:
  name: order-service-ingress
  annotations:
    kubernetes.io/ingress.class: nginx
    cert-manager.io/cluster-issuer: letsencrypt-prod
    nginx.ingress.kubernetes.io/proxy-read-timeout: "60"
    nginx.ingress.kubernetes.io/proxy-connect-timeout: "5"
spec:
  tls:
    - hosts:
        - api.company.com
      secretName: api-tls-cert
  rules:
    - host: api.company.com
      http:
        paths:
          - path: /api/orders
            pathType: Prefix
            backend:
              service:
                name: order-service
                port:
                  number: 80
```

### Example 2: HorizontalPodAutoscaler (HPA v2) with CPU and Request Rate
```yaml
apiVersion: autoscaling/v2
kind: HorizontalPodAutoscaler
metadata:
  name: order-service-hpa
spec:
  scaleTargetRef:
    apiVersion: apps/v1
    kind: Deployment
    name: order-service
  minReplicas: 3
  maxReplicas: 20
  metrics:
    # 1. Standard Resource Metric: Scale if CPU > 75%
    - type: Resource
      resource:
        name: cpu
        target:
          type: Utilization
          averageUtilization: 75
    # 2. Custom Metric: Scale if HTTP Request Rate exceeds 200 req/sec per pod
    - type: Pods
      pods:
        metric:
          name: http_requests_per_second
        target:
          type: AverageValue
          averageValue: "200"
  behavior:
    scaleUp:
      stabilizationWindowSeconds: 0 # Scale up immediately on spike
      policies:
        - type: Percent
          value: 100 # Double replica count every 15s if needed
          periodSeconds: 15
    scaleDown:
      stabilizationWindowSeconds: 300 # Wait 5 minutes to prevent thrashing
      policies:
        - type: Percent
          value: 10 # Scale down at most 10% per minute
          periodSeconds: 60
```

### Example 3: PodDisruptionBudget (PDB)
Ensures voluntary disruptions (node maintenance, cluster upgrades) never violate minimum availability:

```yaml
apiVersion: policy/v1
kind: PodDisruptionBudget
metadata:
  name: order-service-pdb
spec:
  minAvailable: 2 # At least 2 pods MUST be running at all times!
  selector:
    matchLabels:
      app: order-service
```

### Example 4: Anti-Pattern vs. Production Fix (Missing `preStop` Sleep Hook)
```yaml
# ANTI-PATTERN: No preStop hook configured
# Problem: When Pod terminates, Kubernetes sends SIGTERM to Spring Boot IMMEDIATELY.
# Ingress and Kube-Proxy take 2 to 5 seconds to update iptables routing tables.
# Result: Ingress continues sending requests to a terminating pod -> Users get 502 Bad Gateway!
lifecycle: {} # NO HOOK

# PRODUCTION FIX: preStop hook sleeping for 15 seconds
lifecycle:
  preStop:
    exec:
      # Allows Kube-Proxy, Ingress, and AWS ALB to update routing rules FIRST,
      # ensuring ZERO new traffic is directed to the pod before SIGTERM is sent!
      command: [ "sh", "-c", "sleep 15" ]
```

### Example 5: Troubleshooting Pod Evictions and OOMKilled via Kubectl
```bash
# 1. List pods with status
kubectl get pods -l app=order-service -o wide

# 2. Check why pod failed (OOMKilled vs CrashLoopBackOff)
kubectl get pod order-service-78f99-x21z -o jsonpath='{.status.containerStatuses[0].state.terminated}'

# Sample Output:
# {"exitCode":137,"reason":"OOMKilled","startedAt":"...","finishedAt":"..."}

# 3. View live JVM container logs
kubectl logs -f deployment/order-service -c app --tail=100
```

---

## 6. Best Practices & Production Guidelines

1. **Set `maxUnavailable: 0` in Rolling Updates**: Guarantees zero dropped capacity during deployments. Kubernetes always spins up a healthy replacement pod before terminating an old one.
2. **Always Pair `server.shutdown: graceful` with a `preStop` Hook**:
   - `preStop: sleep 15`: Ensures upstream Ingress controllers and Kube-Proxy withdraw the pod from load balancer endpoints before the JVM begins shutting down.
   - `spring.lifecycle.timeout-per-shutdown-phase: 30s`: Gives active in-flight transactions 30 seconds to finish cleanly.
   - `terminationGracePeriodSeconds: 60`: Overall pod grace period ($15\text{s sleep} + 30\text{s drain} + 15\text{s buffer} = 60\text{s}$).
3. **Always Set `resources.requests` Equal to `resources.limits` for Memory**: Guaranteed QoS (Quality of Service) class prevents the Linux node kernel from evicting your Spring Boot pod during node memory contention.
4. **Isolate Actuator Port on Dedicated Service**: Expose port `8080` (Business API) to the external Ingress and expose port `9090` (Actuator/Prometheus) strictly via internal ClusterIP for cluster monitoring scraping.
5. **Enforce Read-Only Root Filesystem**: Set `securityContext.readOnlyRootFilesystem: true` and mount an `emptyDir` on `/tmp` to prevent malicious attackers from downloading and executing binaries in container filesystems.

---

## 7. Common Mistakes & Pitfalls

| Mistake | Why It Fails | Practical Remedy |
|---|---|---|
| Omitting `preStop` sleep hook | Upstream Ingress sends requests to terminating pod during iptables propagation, returning HTTP 502. | Add `lifecycle.preStop.exec: ["sh", "-c", "sleep 15"]`. |
| Pointing Liveness Probe to external database | If DB experiences brief network hiccup, Kubernetes kills and restarts all pods simultaneously (Death Spiral). | Point liveness probe strictly to `/actuator/health/liveness`. |
| Omitting Startup Probe | Slow-starting JVM cold boots get killed by aggressive liveness probes before Spring context initializes. | Configure `startupProbe` with high failure threshold (e.g. 30 $\times$ 2s). |
| Missing `PodDisruptionBudget` | A cluster node upgrade drains all worker nodes, terminating all 3 pod replicas at once. | Deploy a `PodDisruptionBudget` with `minAvailable: 2`. |
| Over-constraining CPU limits | Strict CPU limits trigger Linux CFS quota throttling, adding artificial 50ms latency spikes. | Set CPU limits generously or leave uncapped while setting strict requests. |
| Forgetting `/tmp` volume with read-only root | Embedded Tomcat fails to unpack embedded WAR/JAR or write temporary files during boot. | Mount an `emptyDir` volume at `/tmp`. |

---

## 8. Performance & Security Notes

### Guaranteed QoS vs. Burstable QoS
- **Guaranteed QoS** (`requests.memory == limits.memory` AND `requests.cpu == limits.cpu`): The pod receives highest priority. The Linux kernel will **never** evict a Guaranteed pod to reclaim node memory unless the pod itself exceeds its limit.
- **Burstable QoS** (`requests.memory < limits.memory`): Vulnerable to node eviction during node-level memory pressure.

### SecurityContext Hardening Checklist
- `runAsNonRoot: true` (Prevents running as UID 0).
- `allowPrivilegeEscalation: false` (Prevents setuid binary exploits).
- `capabilities.drop: ["ALL"]` (Strips unnecessary Linux kernel capabilities).

---

## 9. Comparison / When to Use What

| Deployment Pattern | Resilience Level | Zero-Downtime Guarantee | Cost / Complexity |
|---|---|---|---|
| **Standard RollingUpdate (`maxUnavailable: 0`)** | **High** | **Yes (with preStop hook)** | **Low (Built-in standard)** |
| **Blue/Green Deployment** | Very High | Yes (Instant rollback) | High (Requires $2\times$ cluster capacity)|
| **Canary Deployment (Argo Rollouts)** | **Maximum** | **Yes (Progressive traffic shift)**| Medium-High (Requires service mesh / Argo) |
| **Recreate Strategy** | Zero | No (Guaranteed downtime!) | Obsolete for web APIs |

---

## 10. Interview Questions & Answers

### Q1: Why is a `preStop` sleep hook essential for zero-downtime rolling updates in Kubernetes?
<details>
<summary>Click to view answer</summary>

When a pod enters termination, Kubernetes sends `SIGTERM` to the container and concurrently notifies the Endpoint controller to remove the pod IP from Service endpoints. Because network propagation to Kube-Proxy and Ingress load balancers is asynchronous and takes 1 to 5 seconds, the Ingress controller may continue routing new HTTP requests to the pod *after* the JVM has already received `SIGTERM`.  
Adding a `preStop: sleep 15` hook pauses termination inside the container for 15 seconds, giving the network layer ample time to withdraw the pod from routing tables before the Spring Boot application begins graceful shutdown.
</details>

### Q2: How do you configure Spring Boot and Kubernetes for graceful shutdown?
<details>
<summary>Click to view answer</summary>

Three configurations must align:
1. In Spring Boot `application.yml`: Set `server.shutdown: graceful` and `spring.lifecycle.timeout-per-shutdown-phase: 30s`.
2. In Kubernetes `Deployment`: Add `lifecycle.preStop.exec: ["sh", "-c", "sleep 15"]`.
3. In Kubernetes `Deployment`: Set `terminationGracePeriodSeconds: 60` (ensuring the grace period exceeds the preStop sleep plus Spring shutdown timeout).
</details>

### Q3: What is the purpose of a PodDisruptionBudget (PDB)?
<details>
<summary>Click to view answer</summary>

A `PodDisruptionBudget` protects an application during voluntary cluster maintenance events, such as Kubernetes node drain (`kubectl drain`), cluster autoscaler scale-downs, and Kubernetes version upgrades. By setting `minAvailable: 2`, Kubernetes will refuse to drain a node if evicting that pod would drop the total healthy replica count below 2, preventing inadvertent service outages.
</details>

### Q4: Why should `readOnlyRootFilesystem: true` be set and what Spring Boot requirement does it introduce?
<details>
<summary>Click to view answer</summary>

A read-only root filesystem prevents an attacker from writing malicious scripts, modifying application binaries, or dropping exploit payloads into the container.  
However, embedded Tomcat and Spring Boot require temporary directory access to unpack nested resources and compile templates. To accommodate this, an in-memory `emptyDir` volume must be mounted at `/tmp`.
</details>

### Q5: What is the difference between `maxSurge` and `maxUnavailable` in a RollingUpdate strategy?
<details>
<summary>Click to view answer</summary>

- `maxSurge`: The maximum number or percentage of pods that can be created above the desired replica count during an update (e.g. `maxSurge: 25%` on 4 replicas allows 1 temporary extra pod).
- `maxUnavailable`: The maximum number or percentage of pods that can be unavailable during the update process. Setting `maxUnavailable: 0` ensures that 100% of target capacity is maintained at all times during deployment.
</details>

### Q6: How does Horizontal Pod Autoscaling (HPA v2) work with Spring Boot custom metrics?
<details>
<summary>Click to view answer</summary>

Spring Boot Actuator exports application metrics (e.g. `http_server_requests_seconds_count`) to Prometheus. A **Prometheus Adapter** installed in the cluster converts these PromQL metrics into Kubernetes Custom Metrics APIs (`custom.metrics.k8s.io`). The HPA v2 controller queries these custom metrics to scale pod replicas based on real-time business metrics (e.g. request rate per second) rather than lagging CPU metrics.
</details>

### Q7: What causes a Spring Boot pod to fail with `CrashLoopBackOff`?
<details>
<summary>Click to view answer</summary>

`CrashLoopBackOff` indicates that the container repeatedly starts, fails, and terminates. Common causes:
1. Missing or unresolvable environment variables (`DATABASE_URL`, passwords).
2. Failed Flyway/Liquibase database migration on boot.
3. OutOfMemoryError during Spring bean instantiation.
4. Port binding collision (e.g. attempting to bind to privileged port $< 1024$ without root).
</details>

### Q8: What is the difference between a Startup Probe and a Liveness Probe?
<details>
<summary>Click to view answer</summary>

A **Startup Probe** checks if the application has completed its initial boot sequence. While the startup probe is active, liveness and readiness checks are disabled. This gives slow-starting JVMs ample time (e.g. 60s) to warm up without triggering container restarts. Once the startup probe succeeds, the **Liveness Probe** takes over to monitor ongoing runtime health.
</details>

---

## 11. Practice Exercises

### Exercise 1: Write a Minimal Production Service Manifest (Easy)
<details>
<summary>Click to view challenge & solution</summary>

**Challenge**: Write a Kubernetes `Service` manifest named `payment-service` exposing port 80 targeting container port 8080 for pods labeled `app: payment-service`.

**Solution**:
```yaml
apiVersion: v1
kind: Service
metadata:
  name: payment-service
spec:
  type: ClusterIP
  ports:
    - name: http
      port: 80
      targetPort: 8080
  selector:
    app: payment-service
```
</details>

### Exercise 2: Configure Guaranteed QoS with Probes (Medium)
<details>
<summary>Click to view challenge & solution</summary>

**Challenge**: Write the `resources` and `readinessProbe` block for a container ensuring Guaranteed QoS (2 CPU cores, 4GB RAM) and readiness check on `/actuator/health/readiness` on port 9090.

**Solution**:
```yaml
resources:
  requests:
    cpu: "2000m"
    memory: "4096Mi"
  limits:
    cpu: "2000m"
    memory: "4096Mi"
readinessProbe:
  httpGet:
    path: /actuator/health/readiness
    port: 9090
  initialDelaySeconds: 5
  periodSeconds: 5
  timeoutSeconds: 2
  failureThreshold: 2
```
</details>

### Exercise 3: Complete Zero-Downtime Deployment with PreStop and PDB (Hard)
<details>
<summary>Click to view challenge & solution</summary>

**Challenge**: Author a complete Kubernetes manifest containing both a `Deployment` with `maxUnavailable: 0`, 15-second preStop sleep hook, and a `PodDisruptionBudget` requiring at least 2 available replicas.

**Solution**:
```yaml
apiVersion: apps/v1
kind: Deployment
metadata:
  name: inventory-service
spec:
  replicas: 4
  strategy:
    type: RollingUpdate
    rollingUpdate:
      maxSurge: 1
      maxUnavailable: 0
  selector:
    matchLabels:
      app: inventory-service
  template:
    metadata:
      labels:
        app: inventory-service
    spec:
      terminationGracePeriodSeconds: 60
      containers:
        - name: app
          image: registry.internal/apps/inventory-service:1.0.0
          ports:
            - containerPort: 8080
            - containerPort: 9090
          lifecycle:
            preStop:
              exec:
                command: ["sh", "-c", "sleep 15"]
---
apiVersion: policy/v1
kind: PodDisruptionBudget
metadata:
  name: inventory-service-pdb
spec:
  minAvailable: 2
  selector:
    matchLabels:
      app: inventory-service
```
</details>

---

## 12. Summary / Cheat Sheet

```
+---------------------------------------------+-------------------------------------------------------------+
| Manifest / Setting                          | Purpose & Zero-Downtime Rule                                |
+---------------------------------------------+-------------------------------------------------------------+
| maxUnavailable: 0                           | Never reduce capacity during rolling updates                |
| preStop: sleep 15                           | Pauses JVM termination to allow network endpoints to update |
| terminationGracePeriodSeconds: 60           | Total pod grace window for graceful shutdown draining       |
| startupProbe                                | Protects cold JVM boot up to 60s before liveness kicks in   |
| PodDisruptionBudget (PDB)                   | Guarantees minimum replicas during cluster node maintenance |
| HPA v2                                      | Automatically scales replicas on CPU and request rates      |
| readOnlyRootFilesystem: true                | Security hardening; mount emptyDir on /tmp                  |
+---------------------------------------------+-------------------------------------------------------------+
```

---

## 13. Further Reading
- Kubernetes Official Documentation: Deployments & Rolling Update Strategies
- Spring Boot Documentation: Deploying to Kubernetes & Container Probes
- Google Cloud Architecture: Kubernetes Best Practices - Terminating with Grace
- CNCF: Hardening Kubernetes Pod Security Standards (PSS)