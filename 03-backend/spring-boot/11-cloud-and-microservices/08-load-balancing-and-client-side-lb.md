# Client-Side Load Balancing with Spring Cloud LoadBalancer

| Metadata | Value |
|---|---|
| **Spring Boot Version** | 3.4.x / 4.x |
| **Spring Framework Version** | 6.2.x |
| **Java Version** | 21 / 25 LTS |
| **Build Tool** | Maven 3.9+ / Gradle 8.10+ |
| **Tested With** | `spring-cloud-starter-loadbalancer:4.2.x`, Round-Robin, Random, ServiceInstanceListSupplier |

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
- [9. Comparison / When to Use What](#9-comparison--When-to-use-what)
- [10. Interview Questions](#10-interview-questions)
- [11. Practice Exercises](#11-practice-exercises)
- [12. Summary / Cheat Sheet](#12-summary--cheat-sheet)
- [13. Further Reading](#13-further-reading)

---

## 1. What & Why

### The Problem
In distributed microservices, service instances scale up and down across dynamic IP addresses. In traditional architectures, all inter-service traffic routes through centralized hardware or software load balancers (F5, AWS ALB, Nginx). This server-side load balancing introduces a physical extra network hop for every single inter-service call, creates centralized bottlenecks and single points of failure, and incurs substantial infrastructure and cloud data transfer costs.

### The Evolution
Client-Side Load Balancing eliminates the centralized proxy: the calling microservice holds a local list of healthy target instances (fetched from service discovery) and executes a local load-balancing algorithm directly in memory. Historically, Netflix Ribbon provided this capability in Spring Cloud. However, Ribbon was blocking, unmaintained, and architecturally incompatible with Spring WebFlux. **Spring Cloud LoadBalancer** is the official reactive replacement, built from the ground up for Project Reactor, non-blocking I/O, and modern Spring Boot 3 clients.

### Relevance in Modern Engineering
Spring Cloud LoadBalancer integrates natively with `RestClient`, `WebClient`, and Spring Cloud Gateway via the `@LoadBalanced` annotation. It supports pluggable load-balancing algorithms (Round-Robin, Random, Zone-Preference), local caching of instance lists, and custom health checking.

---

## 2. Core Concepts

- **Client-Side Load Balancing**: The calling service queries the service registry, caches available instances locally, and selects a target instance in-process before making the network call.
- **`@LoadBalanced`**: An annotation applied to a `RestClient.Builder`, `WebClient.Builder`, or `RestTemplate` bean that attaches a client-side load-balancing interceptor.
- **`ReactorServiceInstanceLoadBalancer`**: The core reactive strategy interface defining the algorithm for selecting an instance from a list (e.g., `RoundRobinLoadBalancer`, `RandomLoadBalancer`).
- **`ServiceInstanceListSupplier`**: Reactive supplier providing the list of available `ServiceInstance` objects, with built-in decorators for caching, health-checking, and zone-affinity.
- **`@LoadBalancerClient`**: Annotation used to customize load balancer configuration per individual target service name.

---

## 3. How It Works Internally

```
+-----------------------------------------------------------------------------------+
|                     Spring Cloud LoadBalancer Execution Flow                      |
|                                                                                   |
| 1. Application Calls Virtual URI                                                  |
|    restClient.get().uri("http://inventory-service/stock/SKU-100").retrieve()      |
|                   |                                                               |
|                   v                                                               |
| 2. Intercepted by LoadBalancerInterceptor                                         |
|    Extracts service name: "inventory-service"                                     |
|                   |                                                               |
|                   v                                                               |
| 3. ServiceInstanceListSupplier Look-Up                                            |
|    Checks Caffeine In-Memory Cache -> Hits (Instant list: [10.0.1.4, 10.0.1.9])   |
|    (If cache expired, queries DiscoveryClient / Eureka in background)             |
|                   |                                                               |
|                   v                                                               |
| 4. ReactorServiceInstanceLoadBalancer Algorithm Evaluation                        |
|    RoundRobinLoadBalancer: AtomicInteger.incrementAndGet() % instanceCount        |
|    Selected Instance: ServiceInstance(id="inst-2", host="10.0.1.9", port=8082)   |
|                   |                                                               |
|                   v                                                               |
| 5. URI Reconstruction & Network Call                                              |
|    Replaces "http://inventory-service/..." with "http://10.0.1.9:8082/..."        |
|    Direct point-to-point HTTP request to target host (Zero extra proxy hops!)     |
+-----------------------------------------------------------------------------------+
```

When an HTTP request is made to a virtual hostname (`http://inventory-service/`), the `@LoadBalanced` interceptor captures the call. It asks `LoadBalancerClientFactory` for the load balancer assigned to `inventory-service`. The `ServiceInstanceListSupplier` returns a cached list of healthy instances. The `RoundRobinLoadBalancer` atomically increments a counter to select the next instance. The interceptor rewrites the URI to the physical host and port and forwards the request over the network.

---

## 4. Syntax & Usage

### Maven Dependency
```xml
<dependency>
    <groupId>org.springframework.cloud</groupId>
    <artifactId>spring-cloud-starter-loadbalancer</artifactId>
</dependency>
<!-- Recommended: Caffeine for high-performance in-memory instance caching -->
<dependency>
    <groupId>com.github.ben-manes.caffeine</groupId>
    <artifactId>caffeine</artifactId>
</dependency>
```

### Essential Application Configuration (`application.yml`)
```yaml
spring:
  cloud:
    loadbalancer:
      cache:
        enabled: true
        ttl: 35s # Cache instance list for 35 seconds
        capacity: 256
```

---

## 5. Practical Examples

### Example 1: Configuring `@LoadBalanced` with Modern `RestClient.Builder`
```java
package com.example.platform.loadbalancer.config;

import org.springframework.cloud.client.loadbalancer.LoadBalanced;
import org.springframework.context.annotation.Bean;
import org.springframework.context.annotation.Configuration;
import org.springframework.web.client.RestClient;

@Configuration
public class ClientLoadBalancerConfig {

    @Bean
    @LoadBalanced // Attaches Spring Cloud LoadBalancer interceptor
    public RestClient.Builder loadBalancedRestClientBuilder() {
        return RestClient.builder();
    }
}
```

### Example 2: Using the LoadBalanced Client to Call Remote Services
```java
package com.example.platform.loadbalancer.service;

import org.slf4j.Logger;
import org.slf4j.LoggerFactory;
import org.springframework.stereotype.Service;
import org.springframework.web.client.RestClient;

public record ProductStock(String sku, int quantity) {}

@Service
public class InventoryClientService {

    private static final Logger log = LoggerFactory.getLogger(InventoryClientService.class);
    private final RestClient restClient;

    public InventoryClientService(RestClient.Builder loadBalancedBuilder) {
        // Points to virtual service name 'inventory-service'
        this.restClient = loadBalancedBuilder.baseUrl("http://inventory-service").build();
    }

    public ProductStock getStock(String sku) {
        log.info("Requesting stock for SKU: {} across load-balanced instances", sku);

        return restClient.get()
                .uri("/api/v1/stock/{sku}", sku)
                .retrieve()
                .body(ProductStock.class);
    }
}
```

### Example 3: Legacy Netflix Ribbon Anti-Pattern and Fix
```xml
<!-- ANTI-PATTERN: Retaining obsolete Netflix Ribbon dependencies:
     spring-cloud-starter-netflix-ribbon is DEAD, blocking, and incompatible with Spring Boot 3! -->

<!-- FIX: Replace with spring-cloud-starter-loadbalancer -->
<dependency>
    <groupId>org.springframework.cloud</groupId>
    <artifactId>spring-cloud-starter-loadbalancer</artifactId>
</dependency>
```

### Example 4: Custom Random Load Balancer Algorithm Configuration
```java
package com.example.platform.loadbalancer.config;

import org.springframework.cloud.client.ServiceInstance;
import org.springframework.cloud.client.loadbalancer.reactive.ReactiveLoadBalancer;
import org.springframework.cloud.loadbalancer.annotation.LoadBalancerClient;
import org.springframework.cloud.loadbalancer.core.RandomLoadBalancer;
import org.springframework.cloud.loadbalancer.core.ServiceInstanceListSupplier;
import org.springframework.cloud.loadbalancer.support.LoadBalancerClientFactory;
import org.springframework.context.annotation.Bean;
import org.springframework.core.env.Environment;

// Applies custom load balancer configuration specifically for 'payment-service'
@LoadBalancerClient(name = "payment-service", configuration = PaymentLoadBalancerCustomConfig.class)
public class CustomLoadBalancerSetup {}

class PaymentLoadBalancerCustomConfig {

    @Bean
    public ReactiveLoadBalancer<ServiceInstance> randomLoadBalancer(
            Environment environment,
            LoadBalancerClientFactory loadBalancerClientFactory) {

        String name = environment.getProperty(LoadBalancerClientFactory.PROPERTY_NAME);
        return new RandomLoadBalancer(
                loadBalancerClientFactory.getLazyProvider(name, ServiceInstanceListSupplier.class),
                name
        );
    }
}
```

### Example 5: High-Performance Zone-Preference Supplier Decorator
```java
package com.example.platform.loadbalancer.config;

import org.springframework.cloud.loadbalancer.annotation.LoadBalancerClient;
import org.springframework.cloud.loadbalancer.core.DiscoveryClientServiceInstanceListSupplier;
import org.springframework.cloud.loadbalancer.core.ServiceInstanceListSupplier;
import org.springframework.cloud.loadbalancer.core.ZonePreferenceServiceInstanceListSupplier;
import org.springframework.context.ConfigurableApplicationContext;
import org.springframework.context.annotation.Bean;

@LoadBalancerClient(name = "order-service", configuration = ZonePreferenceLoadBalancerConfig.class)
public class ZoneAffinitySetup {}

class ZonePreferenceLoadBalancerConfig {

    @Bean
    public ServiceInstanceListSupplier zonePreferenceSupplier(ConfigurableApplicationContext context) {
        // Base discovery supplier fetching from Eureka / Consul
        ServiceInstanceListSupplier discoverySupplier =
                new DiscoveryClientServiceInstanceListSupplier(context);

        // Decorates with Zone-Preference: prefers instances in local AWS availability zone
        return new ZonePreferenceServiceInstanceListSupplier(discoverySupplier, context);
    }
}
```

---

## 6. Best Practices

1. **Always Include Caffeine in Classpath**: Include `com.github.ben-manes.caffeine:caffeine` to ensure Spring Cloud LoadBalancer caches instance lists in-memory rather than querying the discovery server on every single HTTP call.
2. **Use Zone-Preference Routing**: Decorate suppliers with `ZonePreferenceServiceInstanceListSupplier` in multi-AZ cloud deployments to keep traffic within the same availability zone, reducing cross-AZ network latency and egress fees.
3. **Set Appropriate Cache TTL**: Configure `spring.cloud.loadbalancer.cache.ttl=30s`; setting TTL too long causes traffic to hit terminated instances, while setting it too short causes excessive discovery registry load.
4. **Always Use `@LoadBalanced` on the Builder**: Annotate `RestClient.Builder` or `WebClient.Builder`, not the built client instance itself.
5. **Combine with Resilience4j Retries**: If an instance fails, allow a retry policy to automatically trigger the load balancer to select a *different* healthy instance for the next attempt.

---

## 7. Common Mistakes & Pitfalls

| Mistake | Why It Fails | Practical Remedy |
|---|---|---|
| Omitting `@LoadBalanced` on `RestClient.Builder` | Client treats `http://inventory-service` as an actual DNS name and fails with `UnknownHostException` | Add `@LoadBalanced` to the `RestClient.Builder` bean. |
| Retaining Netflix Ribbon starter in POM | Causes classpath collisions, classloader crashes, and breaks WebFlux/Netty | Remove `spring-cloud-starter-netflix-ribbon`; use `spring-cloud-starter-loadbalancer`. |
| Missing Caffeine dependency | Defaults to `DefaultLoadBalancerCache`, which has suboptimal eviction and concurrency characteristics | Add `com.github.ben-manes.caffeine:caffeine` to dependencies. |
| Hardcoding port numbers with virtual service names (`http://service:8080`) | LoadBalancer ignores port override and uses the instance's registered port | Use bare service name: `http://service-name/api/...`. |
| Calling `new RestClient()` directly in services | Bypasses Spring's load-balancing interceptor chain completely | Always inject the `@LoadBalanced RestClient.Builder`. |
| Unbalanced custom load balancer configuration | Placing `@Configuration` on custom client classes without proper scoping pollutes all clients | Ensure custom configuration classes are NOT picked up by global `@ComponentScan`. |

---

## 8. Performance & Security Notes

- **Zero Network Proxy Latency**: Client-side load balancing eliminates the intermediate load balancer hop, shaving 2-10ms off every inter-service call.
- **Atomic Concurrency**: Spring Cloud LoadBalancer uses atomic integers (`AtomicInteger`) for round-robin pointer arithmetic, supporting millions of calls per second with zero thread blocking.
- **Instance Health Staling**: In-memory caching means a terminated instance might still be called for up to `ttl` seconds after crashing. Ensure clients implement Resilience4j retries.

---

## 9. Comparison / When to Use What

| Feature | Spring Cloud LoadBalancer | Netflix Ribbon (Legacy) | Server-Side Load Balancer (ALB / K8s VIP) |
|---|---|---|---|
| **Execution Point** | Client JVM memory | Client JVM memory | External network appliance / Kernel |
| **Reactive Support** | 100% Native (Reactor) | None (Blocking only) | Protocol-level |
| **Network Hops** | 1 (Direct to target pod) | 1 (Direct to target pod) | 2 (Client $\to$ Proxy $\to$ Target pod) |
| **Maintenance** | Actively maintained by Spring | Discontinued / Dead | Managed by Cloud / Infra team |
| **Best For** | JVM-to-JVM Spring microservices | Legacy maintenance only | Heterogeneous polyglot microservices |

---

## 10. Interview Questions

<details>
<summary>1. How does Client-Side Load Balancing differ from Server-Side Load Balancing?</summary>
In Server-Side Load Balancing, the client sends requests to a centralized hardware or software load balancer (e.g., AWS ALB, Nginx, or Kubernetes Service VIP), which routes traffic to backend servers. In Client-Side Load Balancing, the client queries a service registry, maintains an in-memory list of healthy backend instances, and selects an instance directly using a local algorithm (like Round-Robin), eliminating the intermediate proxy hop.
</details>

<details>
<summary>2. Why did Spring Cloud replace Netflix Ribbon with Spring Cloud LoadBalancer?</summary>
Netflix placed Ribbon into maintenance mode in 2018. Ribbon was designed in the early Java era, built on blocking I/O and synchronous threads, making it fundamentally incompatible with non-blocking reactive architectures (Project Reactor, WebFlux, and Spring Cloud Gateway). Spring Cloud LoadBalancer is reactive, non-blocking, lightweight, and actively maintained.
</details>

<details>
<summary>3. What happens under the hood when a method calls a `@LoadBalanced RestClient`?</summary>
The `@LoadBalanced` annotation attaches a `LoadBalancerInterceptor` to the client. When an HTTP call is executed against `http://service-name/path`, the interceptor extracts the service name, delegates to `ReactorServiceInstanceLoadBalancer` to select an active `ServiceInstance` from memory, rewrites the URI to the selected instance's physical host and port, and forwards the request.
</details>

<details>
<summary>4. Why is Caffeine recommended when using Spring Cloud LoadBalancer?</summary>
Without Caffeine, Spring Cloud LoadBalancer uses a basic default cache. Adding Caffeine enables high-performance, non-blocking in-memory caching of the service instance list with configurable TTLs, reducing query overhead on service discovery registries (like Eureka or Consul) while ensuring thread-safe access under extreme concurrency.
</details>

<details>
<summary>5. What is Zone-Preference routing in Spring Cloud LoadBalancer?</summary>
Zone-Preference routing (`ZonePreferenceServiceInstanceListSupplier`) filters the discovered instances to prefer those running in the same cloud availability zone (AZ) as the calling service. This minimizes inter-zone latency and eliminates costly cross-AZ data transfer fees charged by cloud providers.
</details>

<details>
<summary>6. How do you configure a specific service to use a Random load-balancing algorithm instead of Round-Robin?</summary>
By using the `@LoadBalancerClient` annotation with a custom configuration class that returns a `RandomLoadBalancer` bean instead of the default `RoundRobinLoadBalancer`.
</details>

<details>
<summary>7. What happens if a selected instance crashes right before a client-side load balancer sends a request?</summary>
Because client-side instance lists are cached, the client may attempt to call a dead instance before the cache refreshes. To handle this cleanly, applications pair Spring Cloud LoadBalancer with Resilience4j Retry: if the first selected instance fails with a connection error, the retry mechanism immediately requests a different instance from the load balancer.
</details>

<details>
<summary>8. Why must custom `@LoadBalancerClient` configuration classes NOT be annotated with `@Configuration` in standard component scan paths?</summary>
If custom load balancer configuration classes are annotated with `@Configuration` and located within the main `@ComponentScan` package, they become registered globally in the root application context, accidentally applying the custom algorithm to *all* services instead of just the targeted service.
</details>

---

## 11. Practice Exercises

<details>
<summary>Exercise 1 (Easy): Configure @LoadBalanced RestClient.Builder</summary>
Write a Spring `@Configuration` class declaring a `@LoadBalanced RestClient.Builder` bean.
</details>

<details>
<summary>Exercise 2 (Easy): Configure LoadBalancer cache TTL in YAML</summary>
Configure `application.yml` setting Spring Cloud LoadBalancer cache TTL to 20 seconds.
</details>

<details>
<summary>Exercise 3 (Medium): Implement client calling virtual service</summary>
Write a service that calls `http://account-service/accounts/{id}` using an injected `@LoadBalanced RestClient`.
</details>

<details>
<summary>Exercise 4 (Medium): Custom RandomLoadBalancer configuration</summary>
Configure `@LoadBalancerClient(name = "shipping-service")` to use `RandomLoadBalancer`.
</details>

<details>
<summary>Exercise 5 (Hard): Implement custom health-check supplier decorator</summary>
Write a custom `ServiceInstanceListSupplier` that pings each instance's `/actuator/health` endpoint before returning the list to the load balancer.
</details>

<details>
<summary>Exercise 6 (Hard): Integration test testing round-robin alternation</summary>
Write a `@SpringBootTest` test providing two static instances (`localhost:8081`, `localhost:8082`) and verify that two sequential client calls alternate between the two ports.
</details>

---

## 12. Summary / Cheat Sheet

```
+---------------------------------------------------------------------------------+
|                    Spring Cloud LoadBalancer Summary                            |
+---------------------------------------------------------------------------------+
| Dependency:         spring-cloud-starter-loadbalancer + caffeine                |
| Injection:          @LoadBalanced RestClient.Builder (Attaches interceptor)     |
| Default Algorithm:  RoundRobinLoadBalancer (AtomicInteger rotation)             |
| Custom Algorithm:   @LoadBalancerClient(name = "svc", configuration = ...)      |
| Zone Affinity:      ZonePreferenceServiceInstanceListSupplier                   |
| Performance Rule:   Include Caffeine to enable non-blocking instance caching    |
| Port Trap:          Use http://service-name/path (Never hardcode port)          |
| Invariant:          Purge all Netflix Ribbon dependencies from classpath        |
+---------------------------------------------------------------------------------+
```

---

## 13. Further Reading
- [Official Spring Cloud LoadBalancer Reference](https://docs.spring.io/spring-cloud-commons/reference/spring-cloud-commons/loadbalancer.html)
- [Caffeine Cache Integration with Spring Cloud](https://github.com/ben-manes/caffeine)
- [Netflix Ribbon Deprecation Announcement](https://github.com/Netflix/ribbon#netflix-ribbon)
