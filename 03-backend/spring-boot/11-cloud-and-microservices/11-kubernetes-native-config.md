# Kubernetes-Native Configuration: ConfigMaps, Secrets, and Spring Cloud Kubernetes

| Metadata | Value |
|---|---|
| **Spring Boot Version** | 3.4.x / 4.x |
| **Spring Framework Version** | 6.2.x |
| **Java Version** | 21 / 25 LTS |
| **Build Tool** | Maven 3.9+ / Gradle 8.10+ |
| **Tested With** | `spring-cloud-starter-kubernetes-fabric8-config:3.2.x`, Kubernetes 1.30+ |

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
When running Spring Boot microservices inside Kubernetes clusters, deploying and operating a separate, dedicated Spring Cloud Config Server creates significant operational redundancy and overhead. A dedicated Config Server consumes valuable cluster memory (512MB-1GB per replica), requires its own high-availability deployment, and introduces an external network dependency during pod bootstrap. Meanwhile, Kubernetes already provides native, enterprise-grade configuration abstractions: **ConfigMaps** (for non-sensitive properties) and **Secrets** (for base64/encrypted credentials).

### The Evolution
To eliminate redundant infrastructure, Spring Cloud provides **Spring Cloud Kubernetes**. Instead of reaching out to an external Config Server via HTTP, the microservice integrates directly with the Kubernetes API server or reads volume-mounted filesystems. Spring Boot 3 natively supports **ConfigData API (`spring.config.import`)** to bind Kubernetes ConfigMaps and Secrets directly into Spring's `Environment`, with optional dynamic reloading when ConfigMaps are modified in the cluster.

### Relevance in Modern Engineering
In cloud-native GitOps workflows (ArgoCD, Flux, Helm), configurations and secrets are managed as native Kubernetes manifests. Spring Cloud Kubernetes allows developers to treat Kubernetes as the authoritative configuration provider while retaining Spring's familiar `@Value`, `@ConfigurationProperties`, and `@RefreshScope` programming models.

---

## 2. Core Concepts

- **Kubernetes ConfigMap**: An API object used to store non-confidential configuration data in key-value pairs or complete YAML/properties files.
- **Kubernetes Secret**: An API object used to store sensitive data (passwords, tokens, keys) base64-encoded or integrated with KMS providers.
- **Access Modes**:
  - *API-Server Mode*: The Spring application queries the Kubernetes API server directly using a service account token to fetch ConfigMaps and Secrets.
  - *Volume Mount Mode*: Kubernetes mounts ConfigMaps and Secrets as local files/directories into the pod container filesystem (e.g., `/etc/config/`); Spring Boot reads them as standard files without needing cluster API permissions.
- **`spring.config.import=kubernetes:`**: Spring Boot 3 syntax importing Kubernetes ConfigMaps and Secrets into the Spring `Environment`.
- **Dynamic Configuration Watcher**: An optional background watcher detecting ConfigMap modifications and triggering Spring context reloads automatically.
- **Kubernetes RBAC**: Role-Based Access Control rules granting the pod's `ServiceAccount` permission to read (`get`, `list`, `watch`) ConfigMaps and Secrets.

---

## 3. How It Works Internally

```
+-----------------------------------------------------------------------------------+
|                     Kubernetes Configuration Access Modes                         |
|                                                                                   |
| [ Mode 1: Direct API-Server Query (Network-Based) ]                               |
|                                                                                   |
|  +---------------------------+        GET /api/v1/namespaces/default/configmaps/  |
|  | Spring Boot Pod           | -------------------------------------------------+ |
|  | ServiceAccount Token Auth |                                                  | |
|  +---------------------------+ <------------------------------------------------+ |
|                                       Returns ConfigMap JSON Payload              |
|                                                                                   |
| [ Mode 2: Volume Mount Mode (Filesystem-Based - RECOMMENDED) ]                    |
|                                                                                   |
|  Kubernetes Control Plane                                                         |
|         | Mounts ConfigMap as Directory                                           |
|         v                                                                         |
|  Pod Container Filesystem:                                                        |
|    /etc/config/application.yml  <--- Managed atomically by Kubelet                |
|    /etc/secrets/db-password     <--- Projected from Kubernetes Secret             |
|         |                                                                         |
|         v Spring Boot reads standard files on startup                             |
|  spring.config.import=optional:configtree:/etc/config/                            |
|  (Zero Kubernetes API Server permissions required! Minimal Security Footprint!)  |
+-----------------------------------------------------------------------------------+
```

In Volume Mount Mode, Kubernetes mounts the ConfigMap into the container filesystem at `/etc/config/`. Spring Boot's native `configtree:` importer reads each file in the directory as a configuration property (where filename is the property key and file content is the property value). Because the application reads local files, it requires zero Kubernetes API server network calls and zero RBAC cluster permissions, maximizing security and resilience.

---

## 4. Syntax & Usage

### Maven Dependency (Spring Cloud Kubernetes)
```xml
<dependency>
    <groupId>org.springframework.cloud</groupId>
    <artifactId>spring-cloud-starter-kubernetes-fabric8-config</artifactId>
</dependency>
```

### Application Configuration (`application.yml`)
```yaml
spring:
  application:
    name: order-service
  config:
    # Volume mount mode (Zero API server permissions required)
    import:
      - "optional:configtree:/etc/config/"
      - "optional:configtree:/etc/secrets/"

# Alternatively, API-Server Mode:
# spring.cloud.kubernetes.config.enabled=true
# spring.cloud.kubernetes.secrets.enabled=true
```

---

## 5. Practical Examples

### Example 1: Kubernetes ConfigMap Manifest (`order-configmap.yaml`)
```yaml
apiVersion: v1
kind: ConfigMap
metadata:
  name: order-service-config
  namespace: default
data:
  # Multi-line YAML embedded directly inside the ConfigMap
  application.yml: |
    order:
      discount:
        rate: 0.15
      max-items-per-order: 50
    feature:
      express-delivery:
        enabled: true
```

### Example 2: Kubernetes Deployment Mounting ConfigMap and Secret as Volumes
```yaml
apiVersion: apps/v1
kind: Deployment
metadata:
  name: order-service
  namespace: default
spec:
  replicas: 3
  selector:
    matchLabels:
      app: order-service
  template:
    metadata:
      labels:
        app: order-service
    spec:
      containers:
        - name: order-service
          image: registry.example.com/order-service:1.0.0
          ports:
            - containerPort: 8080
          volumeMounts:
            # Mount ConfigMap as read-only directory
            - name: config-volume
              mountPath: /etc/config
              readOnly: true
            # Mount Secret as read-only directory
            - name: secret-volume
              mountPath: /etc/secrets
              readOnly: true
          env:
            - name: SPRING_CONFIG_IMPORT
              value: "optional:configtree:/etc/config/,optional:configtree:/etc/secrets/"
      volumes:
        - name: config-volume
          configMap:
            name: order-service-config
        - name: secret-volume
          secret:
            secretName: order-service-secrets
```

### Example 3: Consuming Properties in a Typed `@ConfigurationProperties` Record
```java
package com.example.platform.order.config;

import org.springframework.boot.context.properties.ConfigurationProperties;

@ConfigurationProperties(prefix = "order")
public record OrderProperties(
        DiscountProperties discount,
        int maxItemsPerOrder
) {
    public record DiscountProperties(double rate) {}
}
```

```java
package com.example.platform.order.service;

import com.example.platform.order.config.OrderProperties;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;
import org.springframework.boot.context.properties.EnableConfigurationProperties;
import org.springframework.stereotype.Service;

@Service
@EnableConfigurationProperties(OrderProperties.class)
public class OrderDiscountService {

    private static final Logger log = LoggerFactory.getLogger(OrderDiscountService.class);
    private final OrderProperties properties;

    public OrderDiscountService(OrderProperties properties) {
        this.properties = properties;
    }

    public double calculateDiscount(double amount) {
        log.info("Applying discount rate [{}] from Kubernetes ConfigMap", properties.discount().rate());
        return amount * properties.discount().rate();
    }
}
```

### Example 4: Unnecessary RBAC API Server Permissions Anti-Pattern and Fix
```yaml
# ANTI-PATTERN: Granting wide Kubernetes RBAC cluster permissions so Spring Boot can query ConfigMaps via API:
# kind: Role
# rules:
#   - apiGroups: [""]
#     resources: ["configmaps", "secrets"]
#     verbs: ["get", "list", "watch"] # SECURITY RISK! Pod can inspect other secrets!

# FIX: Mount ConfigMaps and Secrets as local filesystem volumes; use configtree: with ZERO RBAC permissions!
```

### Example 5: Kubernetes RBAC Manifest (When API-Server Mode is Required)
```yaml
apiVersion: rbac.authorization.k8s.io/v1
kind: Role
metadata:
  name: spring-cloud-k8s-reader
  namespace: default
rules:
  - apiGroups: [""]
    resources: ["configmaps"]
    resourceNames: ["order-service-config"] # Restrict strictly to service's own ConfigMap!
    verbs: ["get"]
---
apiVersion: rbac.authorization.k8s.io/v1
kind: RoleBinding
metadata:
  name: spring-cloud-k8s-binding
  namespace: default
subjects:
  - kind: ServiceAccount
    name: default
roleRef:
  kind: Role
  name: spring-cloud-k8s-reader
  apiGroup: rbac.authorization.k8s.io
```

---

## 6. Best Practices

1. **Prefer Volume Mount Mode Over API-Server Mode**: Mount ConfigMaps as files and use `spring.config.import=configtree:/etc/config/`; it requires zero cluster RBAC permissions and eliminates API server network dependencies.
2. **Use `optional:` Prefix**: Always prefix imports with `optional:configtree:...` so the application can still boot during local integration testing when Kubernetes volume paths do not exist.
3. **Restrict RBAC if API-Server Mode is Used**: If querying the API server directly, use `resourceNames` to restrict permissions strictly to the service's own ConfigMap.
4. **Leverage Native Kubernetes Rolling Restarts for Config Updates**: Rather than enabling complex dynamic reload watchers, trigger a rollout restart (`kubectl rollout restart deployment/<name>`) to apply new ConfigMaps deterministically.
5. **Never Commit Plaintext Secrets to Git**: Use Sealed Secrets, External Secrets Operator, or HashiCorp Vault to inject secrets into Kubernetes Secret objects safely.

---

## 7. Common Mistakes & Pitfalls

| Mistake | Why It Fails | Practical Remedy |
|---|---|---|
| Pod crashes with 403 Forbidden querying K8s API | Missing RBAC permissions on the pod's `ServiceAccount` to read ConfigMaps | Grant appropriate Role/RoleBinding or switch to Volume Mount mode. |
| Forgetting `optional:` prefix in local development | Application fails to start locally because `/etc/config` directory does not exist | Use `optional:configtree:/etc/config/`. |
| Expecting immediate updates in Volume Mount mode | Kubelet syncs ConfigMap updates to mounted volumes with a delay of up to 60-90s | Trigger a rolling pod restart (`kubectl rollout restart`) for instant updates. |
| Base64 encoding secrets manually in Git | Base64 is encoding, not encryption; easily decoded by anyone viewing Git | Use Sealed Secrets or External Secrets Operator with AWS/Vault. |
| Deploying redundant Spring Cloud Config Server on Kubernetes | Wastes 1GB RAM per replica duplicating features native to Kubernetes | Replace Config Server with native Kubernetes ConfigMaps and Secrets. |
| Mismatch in ConfigMap name | Spring Cloud Kubernetes searches for ConfigMap matching `spring.application.name` by default | Ensure ConfigMap metadata name matches application name. |

---

## 8. Performance & Security Notes

- **API Server Throttling**: If hundreds of pods restart simultaneously and query the Kubernetes API server for ConfigMaps, they can overwhelm `kube-apiserver`. Volume mount mode bypasses the API server completely.
- **Least Privilege Principle**: Avoid granting `secrets` read access across the namespace; malicious dependencies or vulnerabilities could read foreign microservice passwords.
- **Memory Footprint Reduction**: Removing Config Server client libraries and relying on native `configtree:` saves 30-50MB of JVM heap per microservice pod.

---

## 9. Comparison / When to Use What

| Feature | Volume Mount Mode (`configtree:`) | Spring Cloud K8s (API Server Mode) | Spring Cloud Config Server |
|---|---|---|---|
| **Mechanism** | Standard local filesystem read | HTTPS API call to `kube-apiserver` | HTTPS call to dedicated Config JVM |
| **RBAC Permissions** | None required (Zero cluster access)| Requires `get`/`list` on ConfigMaps | None (Uses Git/Vault credentials) |
| **API Server Load** | Zero | High during pod restart spikes | Zero |
| **Dynamic Reload** | Requires pod rolling restart | Automatic reload watcher | `@RefreshScope` / Spring Cloud Bus |
| **Best For** | Modern secure Kubernetes clusters | Specialized dynamic K8s workflows | Multi-cloud / Bare-metal VM setups |

---

## 10. Interview Questions

<details>
<summary>1. Why is Volume Mount mode generally preferred over API-Server mode for Kubernetes configuration in Spring Boot?</summary>
Volume Mount mode is preferred for security and performance:
1. **Security**: It requires zero Kubernetes RBAC permissions. The pod container does not need a ServiceAccount token with rights to query the cluster API.
2. **Performance**: ConfigMaps are mounted by the Kubelet as local files. When pods boot, they read local disk files rather than hammering the Kubernetes API server with HTTPS requests, eliminating API server throttling risks.
</details>

<details>
<summary>2. How does the `configtree:` import prefix work in Spring Boot 3?</summary>
The `configtree:` prefix instructs Spring Boot's `ConfigData` loader to treat a directory as a property source. Each file in the directory becomes a configuration property, where the filename is the property key (with dots preserved or directories traversed) and the file content is the property value.
</details>

<details>
<summary>3. What is the recommended strategy for updating configuration in a production Kubernetes cluster?</summary>
The industry best practice is **GitOps-driven Rolling Restarts**. When a ConfigMap is updated via Git (ArgoCD/Flux), trigger a Kubernetes rolling deployment (`kubectl rollout restart deployment/order-service`). This ensures zero downtime, creates an immutable audit trail of which pod replica ran which configuration version, and avoids the runtime state anomalies of in-memory `@RefreshScope` reloads.
</details>

<details>
<summary>4. Why should you always prefix file configuration imports with `optional:`?</summary>
Prefixing with `optional:` (e.g., `optional:configtree:/etc/config/`) ensures that if the directory does not exist (such as when running the application on a developer's local machine or during JUnit integration tests), Spring Boot ignores the missing directory and boots normally using fallback `application.properties`.
</details>

<details>
<summary>5. How does Kubernetes handle ConfigMap updates on mounted volumes?</summary>
When a ConfigMap is updated, the Kubelet periodically syncs the changes to the mounted volume using atomic symbolic link swapping. However, there is an inherent delay (typically 30-90 seconds) governed by the Kubelet sync frequency and TTL caches.
</details>

<details>
<summary>6. How can you store a complete application.yml inside a Kubernetes ConfigMap?</summary>
By placing the entire multi-line YAML content under a single key named `application.yml` inside the ConfigMap's `data:` block. When mounted into `/etc/config/`, Spring Boot automatically detects and parses the `application.yml` file as a standard hierarchical configuration source.
</details>

<details>
<summary>7. What are the security risks of storing sensitive credentials in standard Kubernetes Secrets?</summary>
Standard Kubernetes Secrets are merely Base64-encoded strings, not cryptographically encrypted. Anyone with `read` permissions in the namespace or access to the etcd datastore can decode them instantly. Production systems must use envelope encryption in etcd and inject secrets using tools like HashiCorp Vault, AWS Secrets Manager, or External Secrets Operator.
</details>

<details>
<summary>8. How do you migrate from Spring Cloud Config Server to native Kubernetes ConfigMaps?</summary>
1. Translate Git property files into Kubernetes `ConfigMap` and `Secret` manifests.
2. Update the Kubernetes Deployment manifest to mount the ConfigMaps at `/etc/config` and `/etc/secrets`.
3. Replace the `configserver:` import in `application.yml` with `optional:configtree:/etc/config/`.
4. Decommission the standalone Config Server deployments.
</details>

---

## 11. Practice Exercises

<details>
<summary>Exercise 1 (Easy): Create a Kubernetes ConfigMap manifest</summary>
Write a YAML manifest for a ConfigMap named `catalog-config` containing property `catalog.max-page-size: "50"`.
</details>

<details>
<summary>Exercise 2 (Easy): Configure configtree import in application.yml</summary>
Write an `application.yml` importing configuration optionally from `/etc/config`.
</details>

<details>
<summary>Exercise 3 (Medium): Mount ConfigMap in a Pod Deployment</summary>
Write a Kubernetes Deployment manifest that mounts `catalog-config` as a volume at `/etc/config`.
</details>

<details>
<summary>Exercise 4 (Medium): Bind ConfigMap property to @Value</summary>
Write a Spring service that injects property `${catalog.max-page-size}` with a default fallback of 20.
</details>

<details>
<summary>Exercise 5 (Hard): Configure RBAC for Spring Cloud Kubernetes API mode</summary>
Write a Kubernetes `Role` and `RoleBinding` granting read access strictly to a single named ConfigMap.
</details>

<details>
<summary>Exercise 6 (Hard): Integration test testing configtree loading</summary>
Write a JUnit 5 test that creates a temporary directory with a file named `app.message`, sets `spring.config.import=configtree:<tempDir>`, and asserts that `@Value("${app.message}")` is injected correctly.
</details>

---

## 12. Summary / Cheat Sheet

```
+---------------------------------------------------------------------------------+
|                    Kubernetes-Native Configuration Summary                      |
+---------------------------------------------------------------------------------+
| Preferred Mode:     Volume Mount Mode (Zero RBAC permissions required)          |
| Import Syntax:      spring.config.import=optional:configtree:/etc/config/       |
| Secret Mounting:    Mount Kubernetes Secrets at /etc/secrets                    |
| K8s Manifest:       volumeMounts: - mountPath: /etc/config readOnly: true       |
| Local Safety:       Always use 'optional:' prefix for local testing             |
| Reload Best Practice: Trigger rolling restart (kubectl rollout restart)         |
| Architecture Win:   Eliminates dedicated Spring Cloud Config Server JVMs        |
+---------------------------------------------------------------------------------+
```

---

## 13. Further Reading
- [Official Spring Cloud Kubernetes Documentation](https://docs.spring.io/spring-cloud-kubernetes/reference/)
- [Kubernetes ConfigMaps Official Documentation](https://kubernetes.io/docs/concepts/configuration/configmap/)
- [Spring Boot 3 Config Tree Processing Guide](https://docs.spring.io/spring-boot/docs/current/reference/html/features.html#features.external-config.files.configtree)
