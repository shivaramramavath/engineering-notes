# Health Check & Graceful Degradation

| Field | Value |
| :--- | :--- |
| **Category** | Distributed & Cloud Patterns (Availability) |
| **Origin** | Kubernetes Probes / Cloud-Native Resilience |
| **Difficulty** | Beginner → Intermediate |
| **Prerequisite(s)** | [Circuit Breaker](file:///E:/Learning/engineering-notes/08-solid-design-patterns/11-distributed-and-cloud-patterns/01-circuit-breaker.md) |
| **Status** | Complete |

> **TL;DR**: **Liveness probes** (`/healthz`) tell orchestrators whether a process is deadlocked and needs a restart; **Readiness probes** (`/readyz`) tell load balancers whether the pod can serve traffic. When non-critical dependencies fail, **Graceful Degradation** serves cached/default fallbacks instead of crashing the whole page.

---

## 1. Liveness vs Readiness vs Startup Probes

| Probe | Question Answered | Action on Failure | Check Dependencies (DB/Redis)? |
| :--- | :--- | :--- | :--- |
| **Startup** | Has the app finished booting? | Delay liveness/readiness | Optional |
| **Liveness (`/healthz`)** | Is the process alive (not deadlocked)? | **Restart container** | **NO!** (Restarting pods won't fix a down DB—it causes a restart storm!) |
| **Readiness (`/readyz`)** | Can this pod accept live traffic? | **Remove from Load Balancer** | **YES** (critical DB / warm caches) |

---

## 2. Graceful Degradation & Fallbacks

- **Recommendations API down?** Show "Popular Products" from local cache instead of failing `/homepage` with `500`.
- **Overloaded?** Shed low-priority traffic (`analytics`, `bots`) while protecting `/checkout`.

---

## 3. Interview Questions

<details>
<summary>1. Why should a Liveness probe NEVER check external dependencies like PostgreSQL?</summary>

If Postgres blips for 10 seconds and `/healthz` fails, Kubernetes will **kill and restart all your app pods simultaneously**, turning a 10-second DB blip into a 5-minute cold-start outage! Check DB connectivity in **Readiness**, not Liveness.

</details>

<details>
<summary>2. Give an example of Graceful Degradation.</summary>

Returning cached/static personalized recommendations or disabling non-essential avatars when a secondary service is degraded, keeping core checkout 100% operational.

</details>

---

## 4. Cheat Sheet

- **Liveness** = Process alive? (NO external checks!)
- **Readiness** = Ready for traffic? (Check critical DB/caches).
- **Graceful Degradation** = Fallback over `500`.

---

## 5. Navigation

| Previous | Up | Next |
| :--- | :--- | :--- |
| [Sharding & Partitioning](file:///E:/Learning/engineering-notes/08-solid-design-patterns/11-distributed-and-cloud-patterns/17-sharding-and-partitioning.md) | [Module 11](file:///E:/Learning/engineering-notes/08-solid-design-patterns/11-distributed-and-cloud-patterns/README.md) | [Feature Flags & Dark Launch](file:///E:/Learning/engineering-notes/08-solid-design-patterns/11-distributed-and-cloud-patterns/19-feature-flags-and-dark-launch.md) |
