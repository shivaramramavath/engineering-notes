# Service Discovery & Load Balancing

| Field | Value |
| :--- | :--- |
| **Category** | Distributed & Cloud Patterns (Routing & Topology) |
| **Origin** | Cloud-Native Infrastructure (Consul, Kubernetes DNS, Envoy) |
| **Difficulty** | Intermediate |
| **Prerequisite(s)** | [Health Check](file:///E:/Learning/engineering-notes/08-solid-design-patterns/11-distributed-and-cloud-patterns/18-health-check-and-graceful-degradation.md) |
| **Status** | Complete |

> **TL;DR**: In elastic cloud environments, pod IPs change every minute. **Service Discovery** resolves logical service names (`billing-svc`) to healthy instance endpoints, while **Load Balancing** distributes traffic (Round-Robin, Least-Connections, Power-of-Two-Choices) across them.

---

## 1. Client-Side vs Server-Side Discovery

- **Server-Side Discovery** (AWS ALB, Kubernetes `Service` + `kube-proxy`): Client calls a single VIP/DNS name; the load balancer/proxy resolves healthy endpoints and routes traffic. Simple clients!
- **Client-Side / Mesh Discovery** (Consul/Eureka + gRPC, or Envoy Sidecar): Client (or its Sidecar) queries the Service Registry, caches healthy IPs, and load-balances directly (zero extra LB hop).

---

## 2. Load Balancing Algorithms

| Algorithm | Best For | Pitfall |
| :--- | :--- | :--- |
| **Round-Robin** | Uniform stateless requests | Sends heavy requests to an already-busy node |
| **Least Connections / P2C** | Variable request durations | Power-of-Two-Choices avoids herd bias at scale |
| **Consistent Hashing** | Stateful caches / sticky routing | Hot keys need virtual nodes / bounded loads |

---

## 3. Interview Questions

<details>
<summary>1. Compare Client-Side vs Server-Side Service Discovery.</summary>

In **Server-Side**, clients send requests to a load balancer/DNS VIP that queries the registry and routes them (zero client library coupling). In **Client-Side**, the client/sidecar queries the registry directly and picks an instance (one fewer network hop, but requires SDK/mesh).

</details>

<details>
<summary>2. Why is Power-of-Two-Choices (P2C) popular in modern proxies like Envoy?</summary>

Picking 2 random healthy backends and routing to the one with fewer active requests avoids both the global state overhead of scanning all nodes and the thundering-herd bias of naive "Least Connections".

</details>

---

## 4. Cheat Sheet

- **Server-side** (K8s Service / ALB) = simplest; **Envoy/gRPC** = smart L7 routing.
- **Health checks** must remove unhealthy instances before routing!

---

## 5. Navigation

| Previous | Up | Next |
| :--- | :--- | :--- |
| [Anti-Corruption Layer (Distributed)](file:///E:/Learning/engineering-notes/08-solid-design-patterns/11-distributed-and-cloud-patterns/14-anti-corruption-layer.md) | [Module 11](file:///E:/Learning/engineering-notes/08-solid-design-patterns/11-distributed-and-cloud-patterns/README.md) | [Leader Election & Distributed Lock](file:///E:/Learning/engineering-notes/08-solid-design-patterns/11-distributed-and-cloud-patterns/16-leader-election-and-distributed-lock.md) |
