---
title: "NGINX vs The Alternatives: Envoy, HAProxy, Traefik & Caddy"
description: "Comprehensive technical and architectural comparison of NGINX vs Envoy Proxy, HAProxy, Traefik, and Caddy: concurrency models, dynamic configuration, memory footprint, and production selection criteria."
module: "06-production"
topic: "08-nginx-vs-alternatives"
status: "completed"
tags: ["nginx", "envoy", "haproxy", "traefik", "caddy", "architecture", "comparison", "benchmarks", "service-mesh"]
prerequisites: ["04-internals/01-architecture-master-worker-and-event-loop.md", "06-production/02-kubernetes-ingress-and-gateway-api.md"]
estimated_read_time: "40 min"
---

# NGINX vs The Alternatives: Envoy, HAProxy, Traefik & Caddy

```
+-------------------------------------------------------------------------------+
| Modern Reverse Proxy Architectural Paradigms                                  |
|                                                                               |
|  1. NGINX: Multi-Process Non-Blocking Event-Driven (ANSI C)                   |
|     - Master Process + N Worker Processes (Single-threaded epoll loops)      |
|     - Declarative static config files; zero-downtime SIGHUP reloads           |
|     - Best for: Edge ingress, high-concurrency static delivery, unified web   |
|                                                                               |
|  2. Envoy Proxy: Multi-Threaded Event Loop (Modern C++20)                     |
|     - Single Process + N Worker Threads (Libevent / Epoll per thread)         |
|     - 100% Dynamic gRPC Control Plane (xDS API: LDS, RDS, CDS, EDS)           |
|     - Best for: Kubernetes service mesh (Istio), microservice sidecars        |
|                                                                               |
|  3. HAProxy: Multi-Threaded High-Throughput Layer 4/7 Proxy (ANSI C)          |
|     - Single Process + Multi-threading (epoll + custom pollers)              |
|     - Native Runtime API; unmatched TCP/HTTP connection pooling               |
|     - Best for: Dedicated high-load load balancing, database connection pools |
|                                                                               |
|  4. Traefik: Cloud-Native Microservice Gateway (Pure Go)                      |
|     - Goroutine-per-connection concurrency model                             |
|     - Auto-discovers backends via Docker labels, Kubernetes Ingress, Consul   |
|     - Best for: Dynamic container environments, rapid microservice routing    |
|                                                                               |
|  5. Caddy 2: Modern Developer Web Server & Edge Proxy (Pure Go)               |
|     - Goroutine concurrency + internal ACME Let's Encrypt engine (Auto HTTPS) |
|     - JSON configuration API + human-friendly Caddyfile                      |
|     - Best for: Developer ergonomics, small-to-mid scale auto-TLS workloads   |
+-------------------------------------------------------------------------------+
```

---

## Table of Contents
1. [What & Why](#1-what--why)
2. [Concurrency & Process Models Compared](#2-concurrency--process-models-compared)
3. [Configuration Architectures: Static Files vs Dynamic xDS APIs](#3-configuration-architectures-static-files-vs-dynamic-xds-apis)
4. [In-Depth Technology Profiles](#4-in-depth-technology-profiles)
5. [Comprehensive Architectural Comparison Matrix](#5-comprehensive-architectural-comparison-matrix)
6. [Performance, Latency & Memory Footprint Analysis](#6-performance-latency--memory-footprint-analysis)
7. [Protocol Support: HTTP/3, gRPC & WebSockets](#7-protocol-support-http3-grpc--websockets)
8. [Common Pitfalls & Anti-Patterns in Proxy Selection](#8-common-pitfalls--anti-patterns-in-proxy-selection)
9. [Production Decision Matrix: How to Choose](#9-production-decision-matrix-how-to-choose)
10. [Technical Interview Questions & Scenarios](#10-technical-interview-questions--scenarios)
11. [Hands-On Practice Exercises](#11-hands-on-practice-exercises)
12. [Summary & Quick Reference](#12-summary--quick-reference)
13. [Further Reading](#13-further-reading)

---

## 1. What & Why

Selecting a reverse proxy or load balancer is one of the most foundational architectural decisions in modern software infrastructure. While NGINX has powered over 30% of the world's top websites for nearly two decades, the rise of containerization, Kubernetes service meshes, and dynamic cloud environments has popularized alternative engines:
- **Envoy Proxy**: Engineered by Lyft to solve the observability and dynamic routing requirements of hyper-scale microservices, becoming the de-facto data plane for Istio.
- **HAProxy**: Renowned for surgical, ultra-low-latency Layer 4 and Layer 7 load balancing with unmatched health checking and TCP queue management.
- **Traefik**: Built from day one for Docker and Kubernetes with automated service discovery via container labels.
- **Caddy**: Revolutionized developer ergonomics with out-of-the-box automatic HTTPS certificate provisioning and human-friendly configuration syntax.

Understanding the internal trade-offs—threading vs multiprocessing, static files vs streaming gRPC APIs, memory safety vs raw C execution—enables architects to choose the exact tool for the job.

---

## 2. Concurrency & Process Models Compared

### Multi-Process (NGINX) vs Multi-Threaded (Envoy, HAProxy) vs Goroutines (Traefik, Caddy)

```
  NGINX Architecture:
  [Master (PID 100)] ──── forks ────> [Worker 1] (Isolated Virtual Memory Space)
                     ──── forks ────> [Worker 2] (Isolated Virtual Memory Space)
  - Process isolation: If a worker segfaults, other workers are completely unaffected.
  - Requires explicit Shared Memory segments (shm) for cross-worker state.

  Envoy Architecture:
  [Single Process (PID 200)]
       ├── Worker Thread 1 (Own Epoll Event Loop)
       ├── Worker Thread 2 (Own Epoll Event Loop)
       └── Main Thread (xDS dynamic configuration manager)
  - Shared heap memory: Fast cross-thread statistics and state updates.
  - Risk: Thread memory corruption or unhandled exception crashes the entire daemon.

  Traefik / Caddy Architecture:
  [Go Runtime Process]
       ├── Goroutine Pool (Go Scheduler: M:N user-space multiplexing over OS threads)
       └── Non-blocking Go netpoller
  - Garbage Collection: Automatic memory management, but subject to GC pause spikes.
  - Memory consumption: 2KB to 8KB per active goroutine.
```

---

## 3. Configuration Architectures: Static Files vs Dynamic xDS APIs

### Static Declarative Configurations (NGINX, HAProxy)
- Configurations are written as human-readable text files (`nginx.conf`, `haproxy.cfg`).
- Changes require testing syntax and issuing a signal (`kill -HUP`).
- **Limitation**: In ephemeral Kubernetes clusters where pod IP addresses scale up and down dozens of times per minute, continuous configuration compilation and process reloading can thrash worker processes.

### Streaming Dynamic Control Planes (Envoy xDS API)
- Envoy does not require configuration files or process reloads.
- It connects to a centralized **Control Plane** (like Istio Pilot) via persistent bidirectional gRPC streams.
- The control plane pushes updates dynamically:
  - **LDS (Listener Discovery Service)**: Updates open ports and TLS certificates.
  - **RDS (Route Discovery Service)**: Updates HTTP path matching rules.
  - **CDS (Cluster Discovery Service)**: Updates backend service clusters.
  - **EDS (Endpoint Discovery Service)**: Streams live pod IP/port changes in real-time.

---

## 4. In-Depth Technology Profiles

### 1. NGINX
- **Core Strengths**: Industry gold-standard stability, ultra-low memory footprint (~2MB per worker), unparalleled static file delivery (`sendfile`), battle-tested WAF and security modules, rich caching engine.
- **Best Suited For**: Public edge reverse proxying, high-throughput static CDN caching, enterprise API gateways, and monolithic or containerized web servers.

### 2. Envoy Proxy
- **Core Strengths**: Native dynamic xDS API, rich L7 protocol filters (gRPC, Redis, MongoDB, DynamoDB), built-in distributed tracing, advanced circuit breaking (outlier detection, concurrent request caps).
- **Best Suited For**: Service mesh sidecars (Istio, Linkerd), internal microservice ingress, and Kubernetes Gateway API implementations.

### 3. HAProxy
- **Core Strengths**: Extreme Layer 4 and Layer 7 routing performance, surgical health checking, native runtime socket CLI for draining backends, zero-downtime live reload engine.
- **Best Suited For**: High-throughput database load balancing (MySQL, Postgres), high-density Layer 4 proxying, and mission-critical TCP clustering.

### 4. Traefik
- **Core Strengths**: Automated service discovery from Docker, Kubernetes Ingress, and Consul; automatic Let's Encrypt TLS; modern web UI dashboard.
- **Best Suited For**: Dynamic microservices in Docker Swarm and small-to-medium Kubernetes clusters where zero-configuration onboarding is prioritized.

### 5. Caddy 2
- **Core Strengths**: Out-of-the-box automatic TLS with certificate renewal and OCSP stapling, clean declarative syntax (Caddyfile), dynamic JSON REST API, memory-safe Go execution.
- **Best Suited For**: Local development, SaaS multi-tenant automated custom domain provisioning, and modern static web applications.

---

## 5. Comprehensive Architectural Comparison Matrix

| Architectural Feature | NGINX Core | Envoy Proxy | HAProxy | Traefik | Caddy 2 |
| :--- | :--- | :--- | :--- | :--- | :--- |
| **Language** | ANSI C (C89/C99) | Modern C++20 | ANSI C | Pure Go | Pure Go |
| **Concurrency Model** | Multi-Process (epoll) | Multi-Threaded (epoll)| Multi-Threaded (epoll)| Goroutines | Goroutines |
| **Memory per 10k Conns**| ~15 MB | ~40 MB | ~12 MB | ~120 MB | ~140 MB |
| **Dynamic Config API** | NGINX Plus only / Lua | Native xDS (gRPC) | Native Socket CLI | Dynamic Provider API | Native JSON REST API |
| **Static File Delivery**| Exceptional (`sendfile`)| Basic (Not optimized) | None (Requires web) | Good | Excellent |
| **HTTP Caching** | Advanced (`proxy_cache`)| Basic (Filter based) | Basic cache applet | None (Requires plugin)| Basic |
| **Automatic HTTPS** | Requires Certbot | Requires cert manager | Requires script | Native (Built-in) | **Industry Best (Native)**|
| **HTTP/3 (QUIC)** | Supported (1.25+) | Fully Supported | Supported (2.6+) | Supported | Supported |
| **gRPC Support** | Bidirectional proxy | Native gRPC engine | Bidirectional proxy | Native proxying | Native proxying |
| **Service Mesh Role** | NGINX Service Mesh | De-Facto Mesh Standard| Rarely used in mesh | Traefik Mesh | Rarely used in mesh |

---

## 6. Performance, Latency & Memory Footprint Analysis

Under rigorous synthetic and real-world benchmarks, the structural differences between languages and concurrency models become evident:

### 1. Raw Latency & Jitter Percentiles ($p99$ and $p99.9$)
- **C-Based Proxies (NGINX & HAProxy)**: Demonstrate the flattest latency profiles. Because C avoids garbage collection pauses, $p99.9$ latency remains under **2ms** under 50,000 req/sec loads.
- **C++ (Envoy)**: Highly predictable latency matching C, with minor variance during heavy xDS configuration streaming recomputations.
- **Go-Based Proxies (Traefik & Caddy)**: Deliver exceptional developer productivity, but under extreme concurrency (100,000+ connections), Go runtime Garbage Collection (GC) sweeps introduce latency spikes, inflating $p99.9$ latency to **15ms–45ms**.

### 2. Resident Memory Footprint (RSS)
- **NGINX**: Master process consumes ~3MB RAM; each worker process consumes ~2MB to 4MB RAM, scaling linearly with configured shared memory zones.
- **HAProxy**: Most memory-efficient proxy in existence, consuming ~1.5MB to 3MB total RAM for tens of thousands of concurrent TCP sockets.
- **Envoy**: Requires ~35MB to 80MB baseline RAM to maintain internal protobuf descriptors, xDS route tables, and telemetry counters.
- **Traefik / Caddy**: Require ~80MB to 250MB baseline RAM, scaling with the number of active goroutines and Go heap allocations.

---

## 7. Protocol Support: HTTP/3, gRPC & WebSockets

```
+-------------------------------------------------------------------------------+
| Protocol Capability Comparison                                                |
|                                                                               |
|  Protocol          | NGINX       | Envoy       | HAProxy     | Traefik/Caddy  |
|  :---------------- | :---------- | :---------- | :---------- | :------------- |
|  HTTP/1.1 Keepalive| Yes         | Yes         | Yes         | Yes            |
|  HTTP/2 Multiplex  | Yes (Native)| Yes (Native)| Yes (Native)| Yes (Native)   |
|  HTTP/3 QUIC (UDP) | Yes (1.25+) | Yes         | Yes (2.6+)  | Yes            |
|  gRPC Transcoding  | Basic proxy | Native JSON | Basic proxy | Basic proxy    |
|  WebSocket Tunnel  | Yes (Upgrade| Yes         | Yes         | Yes (Auto)     |
|  Layer 4 TCP/UDP   | Stream mod  | TCP filter  | Core engine | TCP/UDP Routers|
+-------------------------------------------------------------------------------+
```

---

## 8. Common Pitfalls & Anti-Patterns in Proxy Selection

| Anti-Pattern / Mistake | Root Cause | Observable Production Impact | Correct Solution |
| :--- | :--- | :--- | :--- |
| Using Envoy as a Static CDN Web Server | Envoy is designed as a proxy, not a file server | Poor disk page caching; high CPU during large file reads | Use NGINX for static asset hosting |
| Using NGINX with Rapid Pod Churn | Relying on static `nginx.conf` reloads during fast pod autoscaling | Worker thrashing and CPU spikes during frequent `kill -HUP` | Use Envoy / NGINX Gateway Fabric / Lua balancers |
| Using Go Proxies for Multi-Gigabit L4 Routing | High packet rates saturate Go network poller and trigger GC thrashing | Elevated CPU usage and packet drops at 40Gbps line rates | Use HAProxy or NGINX Stream module |
| Running Multiple Uncoordinated Proxies | Stacking Traefik behind NGINX behind HAProxy | Debugging nightmare, latency inflation, unmanageable XFF chains | Consolidate edge routing into a unified tier |
| Missing Hot Reloading in HAProxy | Using service restarts on legacy HAProxy versions | Dropped database connections during config updates | Use HAProxy seamless reload (`-x` socket transfer) |

---

## 9. Production Decision Matrix: How to Choose

Follow this architectural decision flowchart when selecting a proxy:

```
  What is your primary architectural use case?
  │
  ├── 1. Edge Ingress, Public CDN, Static Web Serving & High-Security WAF:
  │      └── Choose: **NGINX** (Proven stability, low memory, best static engine)
  │
  ├── 2. Internal Microservice Mesh, Dynamic xDS Routing & gRPC APIs:
  │      └── Choose: **Envoy Proxy** (Industry-standard data plane for Kubernetes)
  │
  ├── 3. Ultra-High Throughput Layer 4 / Database Load Balancing:
  │      └── Choose: **HAProxy** (Unmatched L4 performance and TCP health checks)
  │
  ├── 4. Rapid Docker / Kubernetes Prototyping with Auto-Discovery:
  │      └── Choose: **Traefik** (Label-driven routing, minimal manual configuration)
  │
  └── 5. Automatic HTTPS for SaaS Custom Domains & Developer Ergonomics:
         └── Choose: **Caddy 2** (Frictionless automated Let's Encrypt / ZeroSSL)
```

---

## 10. Technical Interview Questions & Scenarios

<details>
<summary><strong>1. Why is Envoy Proxy preferred over NGINX as the data-plane sidecar in Kubernetes service meshes like Istio?</strong></summary>

Kubernetes microservices are inherently dynamic: pods are created, destroyed, and autoscaled constantly. Standard NGINX relies on static configuration files and requires reloading (`SIGHUP`) to update routing rules or upstream clusters, which causes performance degradation at high frequency. Envoy was purpose-built with the **xDS API** (Discovery Services over gRPC). Envoy connects to the service mesh control plane (e.g. Istio Pilot) and receives real-time, streaming updates of newly arriving pod IP addresses (EDS) and route policies (RDS) without restarting, reloading, or dropping in-flight connections. Furthermore, Envoy provides native out-of-the-box distributed tracing, advanced circuit breaking, and deep L7 observability filters.
</details>

<details>
<summary><strong>2. What is the fundamental difference in concurrency architecture between NGINX and Envoy?</strong></summary>

- **NGINX** employs a **multi-process** architecture. A master process forks independent single-threaded worker processes. Each worker runs its own non-blocking `epoll` event loop in an isolated memory address space. Workers communicate shared state (such as rate limits and cache keys) via pre-allocated shared memory segments (`mmap`). If a worker crashes, other workers continue running unaffected.
- **Envoy** employs a **single-process, multi-threaded** architecture. A single master daemon runs multiple worker threads, with each worker thread running its own non-blocking event loop bound to a CPU core. Because all threads share the same process heap memory, sharing state, connection counters, and telemetry across threads is trivial and fast, but an unhandled memory fault crashes the entire proxy daemon.
</details>

<details>
<summary><strong>3. When would you choose HAProxy over NGINX for Layer 4 load balancing?</strong></summary>

HAProxy is chosen when raw proxying performance, connection queuing, and deep health checking are paramount, particularly for non-HTTP workloads like MySQL, PostgreSQL, or Redis clusters. HAProxy provides unmatched TCP connection scheduling algorithms, native runtime management APIs (allowing operators to drain or change backend server weights dynamically over a UNIX socket without reloads), and detailed connection queue buffers that protect overwhelmed backends from connection spikes.
</details>

<details>
<summary><strong>4. How does Go runtime Garbage Collection (GC) affect the operational characteristics of Traefik and Caddy compared to NGINX?</strong></summary>

NGINX is written in C, utilizing custom arena memory pools (`ngx_pool_t`) where memory allocation and destruction are instantaneous and deterministic. Traefik and Caddy are written in Go. The Go runtime manages memory automatically using a concurrent garbage collector. Under moderate traffic, Go's GC pause times are sub-millisecond. However, under high concurrency (e.g. 100,000+ active WebSockets or heavy JSON processing), the generation of millions of temporary objects triggers frequent GC cycles, consuming CPU and causing tail latency jitter ($p99.9$ spikes) that does not exist in C-based proxies.
</details>

<details>
<summary><strong>5. Can open-source NGINX be configured to update upstreams dynamically without reloading like Envoy?</strong></summary>

Standard open-source NGINX cannot update upstreams without reloads using native core directives. However, teams achieve dynamic upstream updates without reloads by:
1. Embedding OpenResty Lua modules (`lua-nginx-module`) to route requests via shared memory dictionaries (`lua_shared_dict`), as used in the community `kubernetes/ingress-nginx` controller.
2. Using NGINX Plus, which provides an official on-the-fly upstream reconfiguration REST API (`http/upstreams`).
3. Using NGINX Gateway Fabric for Kubernetes Gateway API deployments.
</details>

<details>
<summary><strong>6. What makes Caddy's automatic HTTPS implementation unique compared to Certbot + NGINX?</strong></summary>

With NGINX, automating TLS typically requires installing an external cron job or systemd timer running Certbot, configuring ACME challenge webroot locations, managing DNS plugins, and signaling NGINX to reload (`nginx -s reload`) upon renewal. Caddy has a full ACME client integrated directly into its core runtime. It automatically detects domains defined in its configuration, requests certificates from Let's Encrypt or ZeroSSL, manages OCSP stapling, and automates renewals in memory with zero external cron jobs, zero configuration, and zero reloads.
</details>

<details>
<summary><strong>7. Why is NGINX significantly superior to Envoy for static asset delivery?</strong></summary>

NGINX was engineered from its inception as a high-performance web server. It deeply integrates with the Linux kernel's zero-copy `sendfile(2)`, `tcp_nopush`, and asynchronous thread pools (`aio threads`), allowing file pages to transfer directly from the kernel page cache to the network socket without copying bytes into user space. Envoy was engineered strictly as a service proxy; it lacks zero-copy static file delivery engines, and serving static files through Envoy incurs user-space memory copies and elevated CPU utilization.
</details>

<details>
<summary><strong>8. What is Traefik's Provider architecture?</strong></summary>

Traefik uses a pluggable "Provider" abstraction. Instead of reading configuration files, Traefik queries external infrastructure APIs (Docker daemon, Kubernetes API server, Consul catalog, Nomad). When a developer deploys a new container with labels (e.g. `traefik.http.routers.my-app.rule=Host('app.local')`), Traefik automatically detects the container, creates the route, provisions TLS, and routes traffic without any manual configuration file edits.
</details>

---

## 11. Hands-On Practice Exercises

<details>
<summary><strong>Exercise 1: Compare Memory Footprints Under Idle State</strong></summary>

**Goal**: Measure real RSS memory consumption of NGINX vs Envoy vs Traefik.
1. Spin up basic Docker containers for NGINX, Envoy, and Traefik:
```bash
docker run -d --name test-nginx -p 8081:80 nginx:alpine
docker run -d --name test-envoy -p 8082:10000 envoyproxy/envoy:v1.30-latest
docker run -d --name test-traefik -p 8083:80 traefik:v3.0
```
2. Measure memory via `docker stats --no-stream`.
3. Observe NGINX consuming ~2MB to 4MB, Envoy consuming ~30MB, and Traefik consuming ~45MB to 80MB.
</details>

<details>
<summary><strong>Exercise 2: Inspect Envoy xDS Dynamic Configuration API</strong></summary>

**Goal**: Query Envoy's runtime administration interface.
1. Access Envoy's admin port: `curl http://localhost:10000/config_dump`.
2. Inspect the JSON structure of listeners, clusters, and routes.
3. Compare the dynamic programmatic schema with NGINX's declarative text syntax.
</details>

<details>
<summary><strong>Exercise 3: Test Traefik Docker Label Auto-Discovery</strong></summary>

**Goal**: Add a route dynamically with zero config file editing.
1. Launch Traefik listening on Docker socket:
```bash
docker run -d -p 80:80 -v /var/run/docker.sock:/var/run/docker.sock:ro traefik:v3.0 --providers.docker
```
2. Launch a whoami service with labels:
```bash
docker run -d --name whoami --label "traefik.http.routers.whoami.rule=Host(\`whoami.local\`)" containous/whoami
```
3. Test curl: `curl -H "Host: whoami.local" http://localhost/` -> Route resolves instantly!
</details>

<details>
<summary><strong>Exercise 4: Benchmark Latency Percentiles Under Load (NGINX vs Traefik)</strong></summary>

**Goal**: Compare tail latency jitter under concurrency.
1. Run `wrk2` against NGINX: `wrk -t4 -c200 -d30s -R10000 --latency http://localhost:8081/`.
2. Run `wrk2` against Traefik: `wrk -t4 -c200 -d30s -R10000 --latency http://localhost:8083/`.
3. Compare the $p99.9$ latency metrics and analyze GC jitter.
</details>

<details>
<summary><strong>Exercise 5: Inspect HAProxy Runtime Socket Management</strong></summary>

**Goal**: Modify backend server state live without reloads.
1. On an HAProxy instance, connect to the runtime socket:
```bash
echo "show stat" | sudo socat stdio /var/run/haproxy.sock
```
2. Drain a backend server dynamically:
```bash
echo "set server backend_pool/srv1 state drain" | sudo socat stdio /var/run/haproxy.sock
```
3. Verify new traffic routes to other backends without reloading the daemon.
</details>

<details>
<summary><strong>Exercise 6: Test Caddy Automatic Local HTTPS</strong></summary>

**Goal**: Observe zero-configuration TLS provisioning.
1. Run Caddy: `caddy reverse-proxy --from localhost:8443 --to localhost:3000`.
2. Observe Caddy automatically creating and trusting an internal root CA, generating a local TLS certificate, and binding HTTPS seamlessly.
</details>

---

## 12. Summary & Quick Reference

```
+--------------------------------------------------------------------------------+
| Reverse Proxy Architecture Quick Reference Matrix                              |
|                                                                                |
| Proxy      | Core Strength               | Primary Weakness                   |
| :--------- | :-------------------------- | :--------------------------------- |
| **NGINX**  | Static files, low RAM, edge | Static configs; reload for updates  |
| **Envoy**  | Dynamic xDS API, mesh, L7   | Complex config, not for static web  |
| **HAProxy**| L4 performance, runtime CLI | No native static file web server   |
| **Traefik**| Docker / K8s auto-discovery | Go GC pauses under extreme load     |
| **Caddy**  | Native auto-HTTPS, simple   | Higher memory footprint than C     |
+--------------------------------------------------------------------------------+
```

---

## 13. Further Reading

- [Envoy Proxy Architecture & Concepts](https://www.envoyproxy.io/docs/envoy/latest/intro/arch_overview/arch_overview)
- [HAProxy Architecture & Configuration Manual](https://www.haproxy.org/#docs)
- [Traefik Cloud-Native Proxy Documentation](https://doc.traefik.io/traefik/)
- [Caddy Server Documentation](https://caddyserver.com/docs/)
- [Next Module: Production Reference Projects](../07-projects/README.md)

---

[← Previous Note: 07-cdn-and-edge-integration.md](07-cdn-and-edge-integration.md) | [Back to Module Index](README.md) | [Next Module: 07-projects →](../07-projects/README.md)
