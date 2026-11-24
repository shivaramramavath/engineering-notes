# Technical Interview Mastery: Production Deployment, Kubernetes & Observability

## Overview & Evaluation Criteria

Senior backend engineers must know how FastAPI behaves in production environments: multi-process worker scaling, Kubernetes probe design (Liveness vs. Readiness), OpenTelemetry distributed tracing, structured JSON logging, and graceful shutdown handling.

---

## Technical Q&A Deep Dive

<details>
<summary><strong>Q1: Why is Uvicorn's <code>--workers</code> flag an anti-pattern inside a Kubernetes Pod? What is the recommended production architecture?</strong></summary>

### Answer:
- **Inside Kubernetes**:
  - The Kubernetes pod is already the atomic unit of horizontal scaling.
  - Running `--workers 4` inside a single container circumvents Kubernetes CPU/Memory quota management and makes Horizontal Pod Autoscaling (HPA) metrics inaccurate.
  - If one worker consumes excessive memory, the container runtime (cgroups) kills the entire pod, terminating all 4 workers simultaneously.
- **Production Standard**:
  - Run **1 Uvicorn process per container** (`uvicorn main:app --host 0.0.0.0 --port 8000`).
  - Set container CPU requests and limits cleanly.
  - Allow Kubernetes HPA to scale pods up and down based on CPU, Memory, or Prometheus request latency metrics.

</details>

<details>
<summary><strong>Q2: Explain the critical difference between Kubernetes Liveness Probes and Readiness Probes for a FastAPI application. Why should Liveness Probes NEVER query the database?</strong></summary>

### Answer:
- **Readiness Probe (`/health/ready`)**:
  - Determines if the pod is ready to accept incoming traffic from the load balancer / service mesh.
  - **Checks**: Database connection pool health, Redis ping, cache warm state.
  - If it fails, Kubernetes stops routing traffic to this specific pod, but **does not kill it**.
- **Liveness Probe (`/health/live`)**:
  - Determines if the Python process is alive or deadlocked (e.g. event loop frozen).
  - If it fails, Kubernetes **forcibly kills the container and restarts it (`SIGKILL`)**.
- **The Database Outage Disaster**:
  - If the liveness probe queries PostgreSQL (`SELECT 1`) and the database experiences a 10-second failover or latency spike, the liveness probes of **all 100 API pods fail simultaneously**.
  - Kubernetes kills and restarts all 100 pods at once, creating a catastrophic restart loop that crashes the entire platform.
  - **Rule**: Liveness probe should verify only that the local event loop is responsive; never check external dependencies.

</details>

<details>
<summary><strong>Q3: How do you achieve true Zero-Downtime Graceful Shutdown in FastAPI on Kubernetes?</strong></summary>

### Answer:
- When a pod is deleted during a rolling deployment, Kubernetes sends a `SIGTERM` signal and removes the pod IP from the Service endpoint list.
- **The Race Condition**:
  - Removing the pod IP from kube-proxy / iptables across all nodes takes 1 to 5 seconds.
  - If Uvicorn terminates immediately upon receiving `SIGTERM`, incoming requests still being routed to that pod fail with `502 Bad Gateway`.
- **Solution Recipe**:
  1. Add a `preStop` hook with a small sleep (`sleep 5`) to give iptables time to propagate:
     ```yaml
     lifecycle:
       preStop:
         exec:
           command: ["/bin/sh", "-c", "sleep 5"]
     ```
  2. In FastAPI lifespan:
     ```python
     @asynccontextmanager
     async def lifespan(app: FastAPI):
         yield
         # Shutdown phase: stop accepting new work, flush pending analytics, close DB pool
         await engine.dispose()
     ```
  3. Uvicorn finishes servicing active in-flight requests before exiting cleanly.
</details>
