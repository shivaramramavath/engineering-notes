---
title: "Kubernetes Ingress & The Migration to Kubernetes Gateway API"
description: "Comprehensive architectural guide to Kubernetes Ingress Controllers, community ingress-nginx internals, the March 2026 EOL retirement, and the modern migration path to NGINX Gateway Fabric and Gateway API."
module: "06-production"
topic: "02-kubernetes-ingress-and-gateway-api"
status: "completed"
tags: ["nginx", "kubernetes", "ingress", "gateway-api", "ingress-nginx", "gateway-fabric", "httproute", "eol-2026", "cloud-native"]
prerequisites: ["06-production/01-docker-and-containers.md", "03-traffic-management/06-canary-and-blue-green-deployments.md"]
estimated_read_time: "40 min"
---

# Kubernetes Ingress & The Migration to Kubernetes Gateway API

```
+-------------------------------------------------------------------------------+
| The Kubernetes Traffic Governance Evolution                                   |
|                                                                               |
|  PAST & PRESENT: Ingress API (networking.k8s.io/v1)                          |
|  [Ingress Resource]                                                           |
|        │  Monolithic spec; fragile annotations (e.g. nginx.ingress.k8s.io/*)  |
|        │  *IMPORTANT*: Official Community `ingress-nginx` Retires March 2026!|
|        ▼                                                                      |
|  [Community ingress-nginx Controller]                                         |
|        │  Generates monolithic nginx.conf; Lua dynamic upstream endpoints    |
|        ▼                                                                      |
|                                                                               |
|  FUTURE & STANDARD: Kubernetes Gateway API (gateway.networking.k8s.io/v1)     |
|  [Infrastructure / Cluster Ops]                                               |
|        │  GatewayClass & Gateway (Defines IP, Ports, TLS Listeners)          |
|        ▼                                                                      |
|  [Application Developer / DevOps Teams]                                       |
|        │  HTTPRoute / GRPCRoute (Cross-namespace routing, canary splits)      |
|        ▼                                                                      |
|  [NGINX Gateway Fabric (F5 / NGINX Open Source)]                              |
|        │  Native C core configuration compilation, zero reload drops         |
|        ▼                                                                      |
|  [Kubernetes Pod Endpoints (Direct Pod IP routing)]                           |
+-------------------------------------------------------------------------------+
```

---

## Table of Contents
1. [What & Why](#1-what--why)
2. [Core Concepts: Ingress vs Gateway API](#2-core-concepts-ingress-vs-gateway-api)
3. [The Community `ingress-nginx` Controller & Lua Architecture](#3-the-community-ingress-nginx-controller--lua-architecture)
4. [The Critical Industry Transition: March 2026 EOL of `ingress-nginx`](#4-the-critical-industry-transition-march-2026-eol-of-ingress-nginx)
5. [NGINX Gateway Fabric: The Modern Successor](#5-nginx-gateway-fabric-the-modern-successor)
6. [Practical Implementation & Migration Examples](#6-practical-implementation--migration-examples)
7. [Canary Deployments & Header Routing in Gateway API](#7-canary-deployments--header-routing-in-gateway-api)
8. [Common Pitfalls & Anti-Patterns](#8-common-pitfalls--anti-patterns)
9. [Architectural Comparison: Ingress API vs Gateway API](#9-architectural-comparison-ingress-api-vs-gateway-api)
10. [Technical Interview Questions & Scenarios](#10-technical-interview-questions--scenarios)
11. [Hands-On Practice Exercises](#11-hands-on-practice-exercises)
12. [Summary & Quick Reference](#12-summary--quick-reference)
13. [Further Reading](#13-further-reading)

---

## 1. What & Why

In modern cloud-native architectures, NGINX acts as the primary reverse proxy and layer-7 ingress gateway for Kubernetes clusters. However, Kubernetes ingress is undergoing the most significant architectural transformation in its history:
- **The Ingress API Limitation**: The legacy `networking.k8s.io/v1 Ingress` resource was designed in 2015 as a simple host/path routing spec. It lacks native support for essential enterprise capabilities—header-based routing, traffic splitting/canary releases, cross-namespace routing, and TLS redirection—forcing controllers to rely on hundreds of proprietary, vendor-locked annotations (e.g., `nginx.ingress.kubernetes.io/rewrite-target`).
- **Community Ingress-NGINX Retirement**: The Kubernetes maintainers officially announced the retirement and End-of-Life (EOL) of the community `kubernetes/ingress-nginx` project in March 2026, directing the entire cloud-native ecosystem to migrate to the **Kubernetes Gateway API**.
- **Role-Oriented Architecture**: The Gateway API separates infrastructure provisioning (`GatewayClass`, `Gateway`) owned by Platform Ops from routing rules (`HTTPRoute`) owned by individual application teams.

Understanding both the legacy community controller and modern NGINX Gateway Fabric is vital for maintaining existing infrastructure while executing successful multi-cluster migrations.

---

## 2. Core Concepts: Ingress vs Gateway API

### The Role Separation Model
Legacy Ingress forces cluster administrators and developers to share the exact same YAML document. If a developer needs to tune a buffer or timeout, they must modify annotations on the shared Ingress, risking cluster-wide outages.

The Gateway API introduces a strict role-oriented hierarchy:
1. **Infrastructure Provider**: Defines the `GatewayClass` (e.g. NGINX Gateway Fabric, Envoy, AWS VPC Lattice).
2. **Cluster / Platform Operator**: Provisions the `Gateway` resource, managing external IP allocations, DNS bindings, firewall rules, and wildcard TLS certificates.
3. **Application Developer**: Authors lightweight `HTTPRoute` or `GRPCRoute` resources in their own namespaces, attaching them to the operator's shared `Gateway` via reference grants.

---

## 3. The Community `ingress-nginx` Controller & Lua Architecture

The classic community controller (`kubernetes/ingress-nginx`) does not simply write a static `nginx.conf` and issue `kill -HUP` for every pod scale event. In a cluster with thousands of pods scaling dynamically, reloading NGINX several times per minute would cause severe worker thrashing and socket dropouts.

### The Lua Dynamic Upstream Balancer
To eliminate reloads during pod scaling, community `ingress-nginx` embeds OpenResty Lua:
```
  [Kube-API Server Watch]
          │ Pod Endpoints change (ReplicaSet scaled up)
          ▼
  [ingress-nginx Go Controller Daemon]
          │
          ├── Does the change alter Server/Host or Location routing rules?
          │     ├── YES: Re-generate nginx.conf & execute `kill -HUP`
          │     └── NO (Only Pod IP/Port list changed):
          ▼
  POST to Internal Unix Domain Socket (/tmp/balancer-subsystem.sock)
          │
          ▼
  [OpenResty Lua Shared Memory Dictionaries (lua_shared_dict)]
          │ Updates upstream backend IP endpoints in-memory in O(1) time!
          ▼
  Zero NGINX process reload required! Connections remain open.
```

---

## 4. The Critical Industry Transition: March 2026 EOL of `ingress-nginx`

In early 2024, the Kubernetes project announced that the community-maintained `kubernetes/ingress-nginx` controller will reach **End-of-Life (EOL) in March 2026**:
- **Why?** The controller has accumulated a decade of technical debt, architectural limitations around config reloading, and an unmaintainable sprawl of over 150 custom annotations, creating recurring CVEs around annotation-based arbitrary command injection.
- **Support Window**: Only critical security vulnerabilities and bug fixes are being backported until March 2026. No new features are being developed.
- **Migration Mandate**: Cloud engineering teams must plan and execute migrations to Gateway API implementations, such as **NGINX Gateway Fabric (F5/NGINX)**, **Envoy Gateway**, or cloud-native managed gateway controllers.

---

## 5. NGINX Gateway Fabric: The Modern Successor

**NGINX Gateway Fabric** is the official open-source project created by F5/NGINX to implement the Kubernetes Gateway API:
- **Direct C Core Generation**: Rather than wrapping NGINX in complex Lua balancers, NGINX Gateway Fabric is written in Go, watching Gateway API resources and compiling optimized, native NGINX configurations.
- **Conformance & Portability**: Adheres 100% to the Kubernetes Gateway API v1 core specifications. An `HTTPRoute` written for NGINX Gateway Fabric can run on any other compliant Gateway controller without modifying a single line of YAML.
- **Enhanced Security**: Eliminates insecure snippet annotations (`configuration-snippet`, `server-snippet`) that historically plagued community ingress controllers.

---

## 6. Practical Implementation & Migration Examples

### Example 1: Legacy Kubernetes Ingress Resource (`networking.k8s.io/v1`)

```yaml
# legacy-ingress.yaml
apiVersion: networking.k8s.io/v1
kind: Ingress
metadata:
  name: order-service-ingress
  namespace: production
  annotations:
    kubernetes.io/ingress.class: "nginx"
    nginx.ingress.kubernetes.io/ssl-redirect: "true"
    nginx.ingress.kubernetes.io/proxy-body-size: "10m"
    nginx.ingress.kubernetes.io/proxy-read-timeout: "60"
    nginx.ingress.kubernetes.io/limit-rps: "20"
spec:
  tls:
    - hosts:
        - api.example.com
      secretName: api-tls-cert
  rules:
    - host: api.example.com
      http:
        paths:
          - path: /orders
            pathType: Prefix
            backend:
              service:
                name: order-service
                port:
                  number: 8080
```

---

### Example 2: Modern Gateway API Migration

Migrating the legacy ingress above requires two separate, decoupled resources:

#### Step 1: Platform Operator Provisions the Gateway
```yaml
# gateway.yaml (Managed by Platform / SRE Team in infra namespace)
apiVersion: gateway.networking.k8s.io/v1
kind: Gateway
metadata:
  name: production-edge-gateway
  namespace: infra-gateways
spec:
  gatewayClassName: nginx
  listeners:
    - name: https-api
      protocol: HTTPS
      port: 443
      hostname: "api.example.com"
      tls:
        mode: Terminate
        certificateRefs:
          - kind: Secret
            name: api-tls-cert
      allowedRoutes:
        namespaces:
          from: All # Allow application namespaces to bind routes
    - name: http-redirect
      protocol: HTTP
      port: 80
      hostname: "api.example.com"
```

#### Step 2: Application Team Provisions the HTTPRoute
```yaml
# httproute.yaml (Managed by Order Service Team in production namespace)
apiVersion: gateway.networking.k8s.io/v1
kind: HTTPRoute
metadata:
  name: order-service-route
  namespace: production
spec:
  parentRefs:
    - name: production-edge-gateway
      namespace: infra-gateways
  hostnames:
    - "api.example.com"
  rules:
    - matches:
        - path:
            type: PathPrefix
            value: /orders
      backendRefs:
        - name: order-service
          port: 8080
          weight: 100
```

---

## 7. Canary Deployments & Header Routing in Gateway API

In legacy Ingress, canary releases required fragile annotations (`nginx.ingress.kubernetes.io/canary: "true"` and `canary-weight: "20"`), creating an invisible secondary ingress object. In the Gateway API, traffic splitting and header routing are first-class, declarative primitives:

### Traffic Weight Canary Split (80% Stable, 20% Canary)
```yaml
apiVersion: gateway.networking.k8s.io/v1
kind: HTTPRoute
metadata:
  name: user-service-canary
  namespace: production
spec:
  parentRefs:
    - name: production-edge-gateway
      namespace: infra-gateways
  rules:
    - matches:
        - path:
            type: PathPrefix
            value: /users
      backendRefs:
        - name: user-service-v1
          port: 8080
          weight: 80 # 80% to stable release
        - name: user-service-v2
          port: 8080
          weight: 20 # 20% to canary release
```

### Advanced Header-Based A/B Testing & Internal Employee Route
Route internal engineers to experimental backends based on an HTTP header:
```yaml
apiVersion: gateway.networking.k8s.io/v1
kind: HTTPRoute
metadata:
  name: ab-testing-route
  namespace: production
spec:
  parentRefs:
    - name: production-edge-gateway
      namespace: infra-gateways
  rules:
    # Rule 1: Beta testers with custom header route to V2
    - matches:
        - path:
            type: PathPrefix
            value: /checkout
          headers:
            - name: X-Beta-Tester
              value: "true"
      backendRefs:
        - name: checkout-service-v2
          port: 8080
    # Rule 2: Default catch-all routes to Stable V1
    - matches:
        - path:
            type: PathPrefix
            value: /checkout
      backendRefs:
        - name: checkout-service-v1
          port: 8080
```

---

## 8. Common Pitfalls & Anti-Patterns

| Anti-Pattern / Mistake | Root Cause | Observable Production Impact | Correct Solution |
| :--- | :--- | :--- | :--- |
| Injecting Insecure Snippet Annotations | Using `configuration-snippet` in legacy Ingress | Multiple high-severity CVEs; cluster takeover risk | Eliminate snippets; migrate to Gateway API extension filters |
| Missing Cross-Namespace `ReferenceGrant` | Trying to reference a Secret or Service in another namespace | Route fails to attach; status reports `RefNotPermitted` | Create a `ReferenceGrant` in the target namespace |
| Excessive Ingress Reloads in Large Clusters | Updating legacy Ingress resources frequently | High CPU spikes, dropped connections during `kill -HUP` | Migrate to Gateway API or verify Lua balancer functionality |
| Forgetting `externalTrafficPolicy: Local` | Default `Cluster` policy performs SNAT on node entry | Real client IP is lost; all traffic appears as Node IP | Set `externalTrafficPolicy: Local` on Ingress/Gateway Service |
| Mixing Controller Annotation Namespaces | Using `nginx.org/*` (F5) annotations on community controller | Directives are completely ignored; configurations fail silently | Match annotations strictly to controller vendor |
| Ignoring the March 2026 EOL Milestone | Postponing Gateway API migration planning | Clusters left on unpatched, vulnerable ingress controller | Audit cluster ingresses and establish migration roadmap |

---

## 9. Architectural Comparison: Ingress API vs Gateway API

| Capability / Dimension | Legacy Ingress (`networking.k8s.io/v1`) | Kubernetes Gateway API (`gateway.networking.k8s.io/v1`) |
| :--- | :--- | :--- |
| **API Status** | Maintenance / Frozen (Retires March 2026)| Official Cloud-Native Standard (GA v1.0+) |
| **Persona Separation**| None (Single monolithic resource) | Full (Platform Ops vs App Developer) |
| **Traffic Weight Splitting**| Proprietary annotations (`canary-weight`)| Native first-class feature (`weight: 20`) |
| **Header / Method Matching**| Proprietary regex annotations | Native declarative match filters |
| **Cross-Namespace Routing**| Impossible without hacks | Fully supported via `ReferenceGrant` |
| **Protocol Support** | HTTP / HTTPS only | HTTP, HTTPS, gRPC, TCP, UDP, TLS |
| **Portability Across Vendors**| Very Low (Annotation lock-in) | 100% portable across compliant controllers |

---

## 10. Technical Interview Questions & Scenarios

<details>
<summary><strong>1. Why is the Kubernetes community retiring `ingress-nginx` in March 2026?</strong></summary>

The community `kubernetes/ingress-nginx` project was built in the early days of Kubernetes around an architectural design that generates a monolithic `nginx.conf` file. Over the past decade, supporting modern enterprise requirements necessitated creating over 150 proprietary annotations and embedding Lua scripts. This created severe architectural technical debt, frequent reload performance issues at scale, and recurring critical CVEs (such as arbitrary command injection via unescaped annotation strings). To provide a clean, standardized, role-oriented, and secure foundation, the Kubernetes project created the **Gateway API** as the modern standard and scheduled the retirement of `ingress-nginx` for March 2026.
</details>

<details>
<summary><strong>2. How does the community `ingress-nginx` controller update upstream pod endpoints without reloading the NGINX master process?</strong></summary>

The community controller embeds OpenResty and custom Lua code. While changes to routing rules or TLS certificates trigger an `nginx -s reload` (`kill -HUP`), changes to Pod endpoints (scaling up or down a Deployment) do not touch `nginx.conf`. Instead, the controller's Go daemon monitors Kubernetes Endpoints/EndpointSlices and posts the updated IP:port list to an internal Unix domain socket. An embedded Lua dynamic balancer updates an in-memory hash table in a shared memory zone (`lua_shared_dict`), routing new requests to active pods instantly without dropping connections or restarting workers.
</details>

<details>
<summary><strong>3. What is the role of a `ReferenceGrant` in the Kubernetes Gateway API?</strong></summary>

In Kubernetes, namespaces are security boundaries. By default, a Gateway in namespace `infra` cannot route traffic to a Service or reference a Secret located in namespace `production`. A `ReferenceGrant` is a security policy created by the owner of the target namespace explicitly permitting cross-namespace references. For example, if an `HTTPRoute` in `production` attaches to a `Gateway` in `infra`, the platform team can enforce that only approved routes are allowed to bind to public IP listeners.
</details>

<details>
<summary><strong>4. How do you implement Canary deployments in Gateway API without annotations?</strong></summary>

In the Gateway API, canary traffic splitting is defined directly in the `HTTPRoute` spec using the `weight` field on `backendRefs`:
```yaml
rules:
  - backendRefs:
      - name: app-v1
        port: 8080
        weight: 90
      - name: app-v2
        port: 8080
        weight: 10
```
The controller automatically calculates the ratio (90% to v1, 10% to v2) and manages upstream load distribution without requiring secondary ingress objects or proprietary annotations.
</details>

<details>
<summary><strong>5. What is the difference between `nginx.ingress.kubernetes.io` annotations and `nginx.org` annotations?</strong></summary>

- `nginx.ingress.kubernetes.io/*`: Annotations utilized exclusively by the open-source **community** Ingress Controller maintained under the `kubernetes/ingress-nginx` GitHub repository (retiring in 2026).
- `nginx.org/*`: Annotations utilized by the commercial and open-source Ingress Controller developed directly by **F5 / NGINX Inc.** under `nginxinc/kubernetes-ingress`.
The two controllers are completely different codebases with incompatible annotation syntaxes.
</details>

<details>
<summary><strong>6. Why is `externalTrafficPolicy: Local` critical when deploying an NGINX Ingress Controller behind a Kubernetes NodePort / LoadBalancer Service?</strong></summary>

By default, Kubernetes Services use `externalTrafficPolicy: Cluster`. When traffic arrives at a worker node, kube-proxy performs Source Network Address Translation (SNAT) and may forward the packet to a pod on another node, replacing the client's real public IP address with the internal node IP. By setting `externalTrafficPolicy: Local`:
1. Kube-proxy preserves the original client IP on incoming packets.
2. Traffic is only routed to nodes running an active Ingress Controller pod, avoiding extra network hops across the internal overlay network.
</details>

<details>
<summary><strong>7. How does NGINX Gateway Fabric differ from Envoy Gateway?</strong></summary>

Both implement the same declarative Kubernetes Gateway API specification. NGINX Gateway Fabric uses NGINX as the underlying proxy engine, translating Gateway API resources into native NGINX configuration structures. Envoy Gateway uses Envoy Proxy, translating Gateway resources into dynamic xDS API calls over gRPC. Teams choose NGINX Gateway Fabric when they rely on NGINX's proven C-core stability, low memory footprint, and familiarity with NGINX operational paradigms.
</details>

<details>
<summary><strong>8. What happens during a migration if both legacy Ingress and Gateway API routes target the same hostname?</strong></summary>

If both controllers share the same external load balancer IP or cloud DNS entry, routing conflicts occur. A safe migration strategy uses weighted DNS (Route 53) or separate hostnames (e.g. `gateway.example.com`), gradually shifting 5%, 25%, 50%, and 100% of public traffic from the legacy Ingress controller to the Gateway API controller while verifying telemetry and error logs.
</details>

---

## 11. Hands-On Practice Exercises

<details>
<summary><strong>Exercise 1: Install Gateway API CRDs in a Local KinD / Minikube Cluster</strong></summary>

**Goal**: Prepare a cluster for Gateway API resources.
1. Apply the official Gateway API experimental/standard CRDs:
```bash
kubectl apply -f https://github.com/kubernetes-sigs/gateway-api/releases/download/v1.1.0/standard-install.yaml
```
2. Verify CRD installation:
```bash
kubectl get crds | grep gateway.networking.k8s.io
# Confirm gatewayclasses, gateways, httproutes, and referencegrants are present
```
</details>

<details>
<summary><strong>Exercise 2: Deploy NGINX Gateway Fabric via Helm</strong></summary>

**Goal**: Install the modern NGINX Gateway controller.
1. Add NGINX Helm repository:
```bash
helm repo add oci-edge https://pkgs.k8s.ovh.net/
helm install ngf oci://ghcr.io/nginxinc/charts/nginx-gateway-fabric --namespace nginx-gateway --create-namespace
```
2. Verify that the `nginx` GatewayClass is registered and `Accepted`:
```bash
kubectl get gatewayclass
```
</details>

<details>
<summary><strong>Exercise 3: Deploy an HTTPRoute with 50/50 Traffic Weight Split</strong></summary>

**Goal**: Validate native canary routing.
1. Deploy two mock backend services: `demo-v1` and `demo-v2`.
2. Apply an `HTTPRoute` with `weight: 50` on each backend.
3. Send 100 requests via curl in a loop:
```bash
for i in {1..100}; do curl -s http://api.local/demo; done | sort | uniq -c
```
4. Verify roughly 50% distribution across v1 and v2.
</details>

<details>
<summary><strong>Exercise 4: Test Header-Based Routing Rules</strong></summary>

**Goal**: Verify deterministic routing for QA engineers.
1. Implement the header-matching rule from Section 7 (`X-Beta-Tester: true`).
2. Test default curl: `curl http://api.local/checkout` -> returns v1.
3. Test header curl: `curl -H "X-Beta-Tester: true" http://api.local/checkout` -> returns v2.
</details>

<details>
<summary><strong>Exercise 5: Configure Cross-Namespace `ReferenceGrant`</strong></summary>

**Goal**: Permit a route in namespace `app` to attach to a gateway in `gateway-infra`.
1. Deploy Gateway in `gateway-infra`.
2. In namespace `app`, deploy an HTTPRoute targeting the parent gateway.
3. Notice route status indicates `Pending` or `RefNotPermitted`.
4. Apply a `ReferenceGrant` in `gateway-infra` and observe route status transitioning to `Programmed: True`.
</details>

<details>
<summary><strong>Exercise 6: Inspect Community Ingress Dynamic Upstream Balancer</strong></summary>

**Goal**: Observe Lua upstream updates without NGINX reloads.
1. On a cluster running community `ingress-nginx`, exec into the controller pod.
2. Inspect the internal dynamic upstream endpoint list:
```bash
curl 127.0.0.1:10246/configuration/backends
```
3. Scale a deployment (`kubectl scale --replicas=5 deployment/demo`) and immediately re-query the endpoint to observe pod IPs added without `nginx.conf` timestamp changes.
</details>

---

## 12. Summary & Quick Reference

```
+--------------------------------------------------------------------------------+
| Kubernetes Ingress & Gateway API Reference Matrix                              |
|                                                                                |
| Resource                      | API Group                       | Primary Role |
| :---------------------------- | :------------------------------ | :----------- |
| Ingress (Legacy - EOL 2026)   | networking.k8s.io/v1            | Monolithic   |
| GatewayClass                  | gateway.networking.k8s.io/v1    | Infra Vendor |
| Gateway                       | gateway.networking.k8s.io/v1    | Cluster Ops  |
| HTTPRoute                     | gateway.networking.k8s.io/v1    | App Developer|
| GRPCRoute                     | gateway.networking.k8s.io/v1    | App Developer|
| ReferenceGrant                | gateway.networking.k8s.io/v1beta1| Security Admin|
+--------------------------------------------------------------------------------+
```

---

## 13. Further Reading

- [Official Announcement: Retiring Community ingress-nginx](https://kubernetes.io/blog/)
- [Kubernetes Gateway API Documentation](https://gateway-api.sigs.k8s.io/)
- [NGINX Gateway Fabric GitHub Repository](https://github.com/nginxinc/nginx-gateway-fabric)
- [Next Concept: Operations, Zero-Downtime Reloads & CI Pipelines](03-operations-zero-downtime-reloads-and-ci.md)

---

[← Previous Note: 01-docker-and-containers.md](01-docker-and-containers.md) | [Back to Module Index](README.md) | [Next Note: 03-operations-zero-downtime-reloads-and-ci.md →](03-operations-zero-downtime-reloads-and-ci.md)
