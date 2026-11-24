# Service Discovery: Eureka, Consul, and Kubernetes DNS / Service Mesh

| Metadata | Value |
|---|---|
| **Spring Boot Version** | 3.4.x / 4.x |
| **Spring Framework Version** | 6.2.x |
| **Java Version** | 21 / 25 LTS |
| **Build Tool** | Maven 3.9+ / Gradle 8.10+ |
| **Tested With** | `spring-cloud-starter-netflix-eureka-client`, `spring-cloud-starter-consul-discovery`, Kubernetes DNS |

---

## Table of Contents
- [1. What & Why](#1-what--why)
- [2. Core Concepts](#2-core-concepts)
- [3. How It Works Internally](#3-how-it-works-internally)
- [4. Syntax & Usage](#4-syntax--usage)
- [5. Practical Examples](#5-practical-examples)
- [6. Best Practices](#6-best-practices)
- [7. Common Mistakes & Pitfalls](#7-common-mistakes--pitfalls)
- [8. Performance & Security Notes](#8-performance--security-notes)
- [9. Comparison / When to Use What](#9-comparison--when-to-use-what)
- [10. Interview Questions](#10-interview-questions)
- [11. Practice Exercises](#11-practice-exercises)
- [12. Summary / Cheat Sheet](#12-summary--cheat-sheet)
- [13. Further Reading](#13-further-reading)

---

## 1. What & Why

### The Problem
In modern containerized and auto-scaled microservice platforms, service network topology is highly volatile. Pods and container instances scale up, scale down, crash, restart, and migrate across physical worker nodes dynamically, acquiring unpredictable ephemeral IP addresses and ports on every lifecycle event. Hardcoding static IP addresses or hostnames into configuration files is impossible. Services need a reliable, dynamic mechanism to answer: "Where are all currently healthy instances of `payment-service` located right now?"

### The Evolution
Service discovery solves dynamic network location mapping. Historically, Spring Cloud popularized **Client-Side Service Discovery** using Netflix Eureka and HashiCorp Consul. Microservices register themselves with a central registry upon startup, send periodic heartbeats to maintain active status, and query the registry to discover peers. Concurrently, **Server-Side Service Discovery** emerged as standard infrastructure in Kubernetes, where CoreDNS, `kube-proxy`, and Service Meshes (like Istio and Linkerd) resolve virtual service hostnames transparently without requiring application-level discovery libraries.

### Relevance in Modern Engineering
Spring Boot 3 supports both paradigms. In VM-based and hybrid cloud environments, Eureka or Consul registries provide fine-grained routing control and health metadata. In containerized Kubernetes deployments, native DNS resolution (`service.namespace.svc.cluster.local`) simplifies operations by eliminating separate registry JVMs.

---

## 2. Core Concepts

- **Service Registry**: A centralized, highly available database of registered service instances, their network coordinates (IP, port), and health status metadata.
- **Client-Side Discovery**: The calling microservice queries the registry, receives a list of available instance addresses, runs a local load-balancing algorithm (Spring Cloud LoadBalancer), and connects directly to one instance.
- **Server-Side Discovery**: The calling service sends the request to a stable virtual IP or DNS hostname (e.g., Kubernetes `ClusterIP`); a proxy/router balances traffic to backend pods transparently.
- **Heartbeat & Eviction**: Registered instances emit periodic heartbeats (e.g., every 30 seconds). If heartbeats cease, the registry marks the instance unhealthy and evicts it.
- **Self-Preservation Mode (Eureka)**: A safety mechanism where Eureka pauses instance eviction during network partitions if it detects a sudden drop in heartbeats across the entire cluster, preventing mass de-registration.
- **`DiscoveryClient`**: The core Spring Cloud interface (`org.springframework.cloud.client.discovery.DiscoveryClient`) providing programmatic access to service instance metadata.

---

## 3. How It Works Internally

```
+-----------------------------------------------------------------------------------+
|                        Client-Side vs Server-Side Discovery                       |
|                                                                                   |
| [ Client-Side Discovery (Eureka / Consul) ]                                       |
|                                                                                   |
|  +---------------------------+        1. Heartbeat Registration                   |
|  | Payment Service Instance  | -------------------------------------\             |
|  +---------------------------+                                      |             |
|                                                                     v             |
|  +---------------------------+        2. Query Registry     +------------------+  |
|  | Order Service             | ---------------------------> | Eureka / Consul  |  |
|  | (Local LoadBalancer)      | <--------------------------- | Service Registry |  |
|  +---------------------------+  Returns: [10.0.1.5, 10.0.1.8] +------------------+  |
|                |                                                                  |
|                v 3. Direct HTTP call to chosen IP: http://10.0.1.5:8080/charge    |
|  +---------------------------+                                                    |
|  | Payment Service (10.0.1.5)|                                                    |
|  +---------------------------+                                                    |
|                                                                                   |
| [ Server-Side Discovery (Kubernetes DNS & Service Mesh) ]                         |
|                                                                                   |
|  +---------------------------+        1. HTTP GET http://payment-service/charge   |
|  | Order Service Pod         | ------------------------------------+              |
|  +---------------------------+                                     |              |
|                                                                    v              |
|                                                      +--------------------------+ |
|                                                      | K8s CoreDNS / Service VIP| |
|                                                      | (Kube-Proxy / Envoy Pod) | |
|                                                      +--------------------------+ |
|                                                                    |              |
|                                                                    v Load Balances|
|                                                      +--------------------------+ |
|                                                      | Payment Pod (10.244.1.12)| |
|                                                      +--------------------------+ |
+-----------------------------------------------------------------------------------+
```

In Client-Side Discovery, `Order Service` periodically caches the instance registry from Eureka. When resolving `http://payment-service/`, Spring Cloud LoadBalancer picks an IP directly from its local cache, bypassing external load balancers. In Server-Side Discovery (Kubernetes), `Order Service` simply issues an HTTP request to `http://payment-service`. Kubernetes CoreDNS resolves the name to a virtual `ClusterIP`, and Linux kernel iptables/IPVS or an Envoy sidecar proxy intercepts the packet and routes it directly to a healthy backend pod.

---

## 4. Syntax & Usage

### Maven Dependency (Eureka Client)
```xml
<dependency>
    <groupId>org.springframework.cloud</groupId>
    <artifactId>spring-cloud-starter-netflix-eureka-client</artifactId>
</dependency>
```

### Eureka Client Configuration (`application.yml`)
```yaml
spring:
  application:
    name: order-service

eureka:
  client:
    service-url:
      defaultZone: http://eureka-server:8761/eureka/
    fetch-registry: true
    register-with-eureka: true
  instance:
    prefer-ip-address: true
    lease-renewal-interval-in-seconds: 10
    lease-expiration-duration-in-seconds: 30
```

---

## 5. Practical Examples

### Example 1: Programmatic Instance Discovery via `DiscoveryClient`
```java
package com.example.platform.discovery.service;

import org.slf4j.Logger;
import org.slf4j.LoggerFactory;
import org.springframework.cloud.client.ServiceInstance;
import org.springframework.cloud.client.discovery.DiscoveryClient;
import org.springframework.stereotype.Service;

import java.net.URI;
import java.util.List;

@Service
public class DynamicRoutingService {

    private static final Logger log = LoggerFactory.getLogger(DynamicRoutingService.class);
    private final DiscoveryClient discoveryClient;

    public DynamicRoutingService(DiscoveryClient discoveryClient) {
        this.discoveryClient = discoveryClient;
    }

    public URI resolveServiceUri(String serviceId) {
        List<ServiceInstance> instances = discoveryClient.getInstances(serviceId);
        
        if (instances == null || instances.isEmpty()) {
            log.error("Discovery error: no instances available for service: {}", serviceId);
            throw new IllegalStateException("Service unavailable: " + serviceId);
        }

        // Simple round-robin or first healthy instance selection
        ServiceInstance selected = instances.get(0);
        log.info("Resolved service [{}] to instance [{}] at URI [{}]",
                serviceId, selected.getInstanceId(), selected.getUri());
        
        return selected.getUri();
    }
}
```

### Example 2: Minimal Eureka Server Application
```java
package com.example.platform.eurekaserver;

import org.springframework.boot.SpringApplication;
import org.springframework.boot.autoconfigure.SpringBootApplication;
import org.springframework.cloud.netflix.eureka.server.EnableEurekaServer;

@SpringBootApplication
@EnableEurekaServer // Spins up embedded Netflix Eureka registry on port 8761
public class EurekaServerApplication {
    public static void main(String[] args) {
        SpringApplication.run(EurekaServerApplication.class, args);
    }
}
```

### Example 3: Container Hostname Anti-Pattern and Fix
```yaml
# ANTI-PATTERN: Default eureka.instance.prefer-ip-address = false.
# Inside Docker or Kubernetes, containers register their ephemeral hostname (e.g., container_id_abc123).
# Other containers cannot resolve this hostname via DNS, causing connection refused errors!

# FIX: Always force IP address registration inside containerized environments
eureka:
  instance:
    prefer-ip-address: true # Registers 10.x.x.x IP address instead of unresolvable hostname
```

### Example 4: HashiCorp Consul Discovery Client Configuration
```yaml
# Configuration for HashiCorp Consul Discovery
spring:
  application:
    name: inventory-service
  cloud:
    consul:
      host: localhost
      port: 8500
      discovery:
        enabled: true
        register: true
        instance-id: ${spring.application.name}:${random.value}
        health-check-path: /actuator/health
        health-check-interval: 10s
```

### Example 5: High-Performance Custom Service Instance Filter
```java
package com.example.platform.discovery.filter;

import org.springframework.cloud.client.ServiceInstance;
import org.springframework.stereotype.Component;

import java.util.List;

@Component
public class ZoneAffinityInstanceFilter {

    private final String currentZone = "us-east-1a";

    // Prioritizes instances running in the same availability zone to reduce cross-AZ latency and data transfer costs
    public List<ServiceInstance> filterByZoneAffinity(List<ServiceInstance> instances) {
        List<ServiceInstance> sameZone = instances.stream()
                .filter(i -> currentZone.equalsIgnoreCase(i.getMetadata().get("zone")))
                .toList();

        return !sameZone.isEmpty() ? sameZone : instances; // Fallback to any zone if none available locally
    }
}
```

---

## 6. Best Practices

1. **Always Set `prefer-ip-address = true` in Containers**: Prevent services from registering unresolvable container hostnames; always register their routed IP addresses.
2. **Align Lease Renewal and Expiration Intervals**: Keep `lease-renewal-interval-in-seconds` (e.g., 10s) and `lease-expiration-duration-in-seconds` (e.g., 30s) tight to evict crashed instances rapidly.
3. **Use Kubernetes Native DNS on Kubernetes**: If your architecture runs entirely in Kubernetes, use Kubernetes Services (`my-service.default.svc.cluster.local`) rather than deploying separate Eureka servers.
4. **Attach Metadata for Zone-Aware Routing**: Configure instance metadata (e.g., availability zone, region, version) to enable zone-affinity routing and green-blue canary testing.
5. **Secure Registry Endpoints with Basic Auth or mTLS**: Protect the registry so unauthorized external nodes cannot register malicious endpoints or snoop on internal service topology.

---

## 7. Common Mistakes & Pitfalls

| Mistake | Why It Fails | Practical Remedy |
|---|---|---|
| Registering container hostname instead of IP | Other pods cannot resolve random Docker container IDs via DNS | Set `eureka.instance.prefer-ip-address=true`. |
| Running Eureka Server without clustering in production | Single Eureka node becomes a single point of failure | Deploy at least 2 or 3 peered Eureka server instances across AZs. |
| Misunderstanding Eureka Self-Preservation Mode | Eureka stops evicting crashed instances during network hiccups, routing traffic to dead nodes | Understand it prevents mass de-registration; tune `renewal-percent-threshold`. |
| Deploying Eureka inside a Kubernetes Service Mesh | Redundant discovery layers increase memory consumption and cause routing conflicts | Rely on Kubernetes native DNS and Envoy sidecars; remove Eureka. |
| Missing Actuator health check integration | Service reports UP to Eureka even when its database connection is broken | Set `eureka.client.healthcheck.enabled=true`. |
| Duplicate `instance-id` in multi-replica deployments | New pod instance overwrites existing instance in registry | Suffix instance ID with random UUID or port number. |

---

## 8. Performance & Security Notes

- **Registry Caching Latency**: Eureka clients cache the registry locally and refresh every 30 seconds. A newly deployed instance may take up to 30-60 seconds to receive traffic, and a crashed instance may receive traffic for 30 seconds after dying.
- **Network Traffic Optimization**: Eureka client sends delta updates (`fetch-registry=true`) rather than downloading the entire registry every cycle.
- **Registry Poisoning**: If discovery endpoints are unsecured, attackers can register rogue nodes and intercept sensitive inter-service customer traffic (Man-In-The-Middle).

---

## 9. Comparison / When to Use What

| Feature | Netflix Eureka | HashiCorp Consul | Kubernetes DNS / Service Mesh |
|---|---|---|---|
| **Architecture** | AP (Eventual Consistency) | CP (Raft Consensus) | Platform Native (CoreDNS + iptables) |
| **Discovery Model** | Client-Side | Client-Side or DNS | Server-Side |
| **Additional Features** | None (Registry only) | KV Store, Health Checks | Built-in K8s, mTLS (Istio) |
| **Resource Footprint** | Heavy JVM process | Lightweight Go binary | Zero extra JVMs (Kernel/Sidecar) |
| **Best For** | Legacy AWS EC2, Spring Cloud | Multi-cloud, non-JVM stacks | Kubernetes container platforms |

---

## 10. Interview Questions

<details>
<summary>1. What is the fundamental difference between Client-Side and Server-Side Service Discovery?</summary>
In Client-Side Discovery (Eureka), the consumer queries the registry directly, receives a list of available instance addresses, and selects an instance using a local client-side load balancer. In Server-Side Discovery (Kubernetes), the consumer sends requests to a stable virtual IP or DNS hostname, and a platform load balancer/proxy (like `kube-proxy` or an AWS ALB) routes traffic to healthy backends transparently.
</details>

<details>
<summary>2. What is Eureka's Self-Preservation Mode and when is it triggered?</summary>
Self-Preservation is an AP (CAP theorem) resilience mechanism. If Eureka stops receiving heartbeats from a significant percentage of instances (e.g., > 15% drop within 15 minutes), it assumes a network partition occurred rather than all services failing simultaneously. Eureka pauses instance evictions to protect the registry, allowing traffic to continue reaching surviving instances.
</details>

<details>
<summary>3. Why is `eureka.instance.prefer-ip-address=true` essential in containerized environments?</summary>
In containers (Docker/Kubernetes), each container receives an ephemeral hostname matching its container ID (e.g., `a4f891b2c`). By default, Eureka registers this hostname. Since other containers cannot resolve individual container IDs via standard DNS, inter-service HTTP calls fail. Setting `prefer-ip-address=true` forces the instance to register its routable IP address instead.
</details>

<details>
<summary>4. What CAP theorem trade-off does Eureka make versus Consul?</summary>
Eureka chooses **Availability and Partition Tolerance (AP)**: during network splits, nodes keep serving stale registration data and accept new registrations without strong consensus. Consul chooses **Consistency and Partition Tolerance (CP)**: it uses the Raft consensus algorithm, ensuring strong consistency across nodes, but will reject writes if a quorum cannot be reached.
</details>

<details>
<summary>5. How does the Spring Cloud `DiscoveryClient` interface promote portability?</summary>
`DiscoveryClient` is an abstraction. Application code calls `discoveryClient.getInstances("service-name")` without knowing whether the underlying registry is Eureka, Consul, Zookeeper, or Kubernetes. Switching discovery providers requires only swapping dependency starters without rewriting application code.
</details>

<details>
<summary>6. Why do many Kubernetes-native architectures eliminate Eureka entirely?</summary>
Kubernetes already provides robust built-in service discovery through CoreDNS and `ClusterIP` services. Running Eureka inside Kubernetes wastes JVM heap memory, introduces caching latency (30s refresh windows), and duplicates functionality already handled natively by the Linux kernel and container runtime.
</details>

<details>
<summary>7. How does Spring integrate Actuator health checks with service discovery?</summary>
By setting `eureka.client.healthcheck.enabled=true`. Instead of relying only on basic network heartbeats, Eureka propagates the actual Spring Boot Actuator status (`UP`, `DOWN`, `OUT_OF_SERVICE`). If a service's database connection fails, Actuator reports `DOWN`, prompting Eureka to stop routing traffic to that instance immediately.
</details>

<details>
<summary>8. What is Zone-Aware (Zone-Affinity) routing in service discovery?</summary>
Zone-Aware routing instructs the load balancer to prefer routing requests to service instances located within the same cloud availability zone (e.g., `us-east-1a`). This minimizes cross-zone network latency and eliminates cross-AZ cloud network data transfer costs.
</details>

---

## 11. Practice Exercises

<details>
<summary>Exercise 1 (Easy): Configure @EnableEurekaServer</summary>
Write a Spring Boot main application class configured with `@EnableEurekaServer` on port 8761.
</details>

<details>
<summary>Exercise 2 (Easy): Configure Eureka client registration</summary>
Write an `application.yml` configuring a service named `"order-service"` to register with Eureka with `prefer-ip-address=true`.
</details>

<details>
<summary>Exercise 3 (Medium): Programmatically query DiscoveryClient</summary>
Write a controller endpoint `/instances/{serviceId}` that uses `DiscoveryClient` to return a list of active URIs for a given service.
</details>

<details>
<summary>Exercise 4 (Medium): Configure Consul health check endpoint</summary>
Write an `application.yml` for Spring Cloud Consul configuring health check path `/actuator/health` and 5s check interval.
</details>

<details>
<summary>Exercise 5 (Hard): Implement zone-affinity round-robin load balancer</summary>
Write a custom `ServiceInstanceListSupplier` bean that sorts instances to prefer matching the caller's availability zone before round-robining across foreign zones.
</details>

<details>
<summary>Exercise 6 (Hard): Integration test with Testcontainers Consul</summary>
Write a Spring Boot test using `org.testcontainers:consul` that starts a Consul container, registers a mock service, and verifies that `DiscoveryClient` discovers the instance.
</details>

---

## 12. Summary / Cheat Sheet

```
+---------------------------------------------------------------------------------+
|                         Service Discovery Summary                               |
+---------------------------------------------------------------------------------+
| Eureka Server:      @EnableEurekaServer on port 8761                            |
| Eureka Client:      spring-cloud-starter-netflix-eureka-client                  |
| Container Rule:     eureka.instance.prefer-ip-address=true [MANDATORY]          |
| Health Coupling:    eureka.client.healthcheck.enabled=true (Actuator sync)      |
| Programmatic API:   discoveryClient.getInstances("service-name")                |
| Self-Preservation:  Pauses eviction during network splits (AP model)           |
| Kubernetes Rule:    Rely on K8s CoreDNS / Service Mesh rather than Eureka       |
| Consul Model:       CP architecture with Raft consensus and built-in KV store   |
+---------------------------------------------------------------------------------+
```

---

## 13. Further Reading
- [Official Spring Cloud Netflix Reference Documentation](https://docs.spring.io/spring-cloud-netflix/reference/)
- [Spring Cloud Consul Reference](https://docs.spring.io/spring-cloud-consul/reference/)
- [Kubernetes Service Discovery Explained](https://kubernetes.io/docs/concepts/services-networking/service/)
