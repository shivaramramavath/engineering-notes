# Spring Boot Docker & Kubernetes Cheatsheet

| Metadata | Details |
|---|---|
| **Topic** | Docker Containerization & Kubernetes Deployment Cheatsheet |
| **Track** | Java Spring Boot |
| **Difficulty** | Intermediate to Advanced |
| **Prerequisites** | Java 21, Linux Containers, Docker, Kubernetes Basics |
| **Java Version** | Java 21 / 25 LTS |
| **Spring Boot** | 3.4.x / 4.x |
| **Tested With** | Docker 27+, Kubernetes 1.30+ |

---

## Table of Contents
1. [What & Why](#1-what--why)
2. [Core Concepts](#2-core-concepts)
3. [Internals & Architecture](#3-internals--architecture)
4. [Syntax & Categorized Reference](#4-syntax--categorized-reference)
5. [5 Practical Production Examples](#5-5-practical-production-examples)
6. [Best Practices](#6-best-practices)
7. [Common Mistakes to Avoid](#7-common-mistakes-to-avoid)
8. [Performance & Security Considerations](#8-performance--security-considerations)
9. [Comparison Table: JVM Container Packaging Strategies](#9-comparison-table-jvm-container-packaging-strategies)
10. [Technical Interview Questions & Answers](#10-technical-interview-questions--answers)
11. [Hands-On Practice Exercises](#11-hands-on-practice-exercises)
12. [Summary & Cheat Sheet](#12-summary--cheat-sheet)
13. [Further Reading & References](#13-further-reading--references)

---

## 1. What & Why
Packaging and orchestrating Spring Boot microservices on Kubernetes demands optimized image construction, predictable memory management within cgroups, fast startup times, and seamless integration with Kubernetes health checking and graceful termination lifecycles.

Modern containerization relies on Spring Boot layered JAR extraction, non-root execution, Class Data Sharing (CDS), and well-tuned Kubernetes Deployment specifications.

---

## 2. Core Concepts
- **Multi-Stage Layered JAR**: Separates slow-changing third-party dependencies from fast-changing application business logic to maximize Docker layer cache efficiency.
- **JVM Container Awareness**: Modern OpenJDK natively respects Linux cgroups v2 memory and CPU limits (`-XX:MaxRAMPercentage`).
- **Non-Root Execution**: Running container processes under unprivileged users (`appuser:appgroup`) to prevent container breakout exploits.
- **Graceful Termination**: Coordinating Kubernetes `SIGTERM` signals with `server.shutdown=graceful` and `preStop` sleep hooks.
- **Probe Alignment**: Actuator liveness and readiness probe mapping.

---

## 3. Internals & Architecture
Multi-Stage Layered Docker Build & Kubernetes Pod Lifecycle:

```
+-------------------------------------------------------------+
|               Stage 1: Layer Extraction                     |
|  java -Djarmode=tools -jar app.jar extract --layers --dest  |
|  -> dependencies/ (spring-core, jackson, hibernate)         |
|  -> spring-boot-loader/                                     |
|  -> snapshot-dependencies/                                  |
|  -> application/ (com/example/MyService.class)              |
+-------------------------------------------------------------+
                             |
                             v
+-------------------------------------------------------------+
|               Stage 2: Minimal Runtime Image                |
|  eclipse-temurin:21-jre-alpine                              |
|  - Creates unprivileged user (uid 10001)                    |
|  - Copies layers (dependencies first for caching)           |
|  - Sets ENTRYPOINT ["java", "-jar", "app.jar"]             |
+-------------------------------------------------------------+
                             |
                             v
+-------------------------------------------------------------+
|                  Kubernetes Pod Lifecycle                   |
|  1. Kubelet starts Pod -> Invokes startupProbe              |
|  2. startupProbe succeeds -> Periodically calls liveness    |
|  3. readinessProbe passes -> Pod added to Service Endpoints |
|  4. Ingress routes production traffic to Pod                |
|  5. Pod termination -> preStop hook -> SIGTERM -> Drain    |
+-------------------------------------------------------------+
```

---

## 4. Syntax & Categorized Reference

### 4.1 Production Multi-Stage Dockerfile
```dockerfile
# Stage 1: Dependency Extraction
FROM eclipse-temurin:21-jre-alpine AS builder
WORKDIR /builder
ARG JAR_FILE=target/*.jar
COPY ${JAR_FILE} application.jar
RUN java -Djarmode=tools -jar application.jar extract --layers --destination extracted

# Stage 2: Minimal Production Runtime
FROM eclipse-temurin:21-jre-alpine
WORKDIR /application

# Create unprivileged runtime user
RUN addgroup -S appgroup && adduser -S appuser -G appgroup

# Copy extracted layers in cache-optimized order
COPY --from=builder --chown=appuser:appgroup /builder/extracted/dependencies/ ./
COPY --from=builder --chown=appuser:appgroup /builder/extracted/spring-boot-loader/ ./
COPY --from=builder --chown=appuser:appgroup /builder/extracted/snapshot-dependencies/ ./
COPY --from=builder --chown=appuser:appgroup /builder/extracted/application/ ./

USER appuser:appgroup
EXPOSE 8080 9090

ENV JAVA_OPTS="-XX:MaxRAMPercentage=75.0 -XX:+UseG1GC -XX:+ExitOnOutOfMemoryError"
ENTRYPOINT ["sh", "-c", "exec java $JAVA_OPTS org.springframework.boot.loader.launch.JarLauncher"]
```

### 4.2 Production Docker Compose (compose.yaml)
```yaml
services:
  app:
    build: .
    ports:
      - "8080:8080"
      - "9090:9090"
    environment:
      SPRING_PROFILES_ACTIVE: prod
      SPRING_DATASOURCE_URL: jdbc:postgresql://postgres:5432/app_db
      SPRING_DATASOURCE_USERNAME: postgres
      SPRING_DATASOURCE_PASSWORD: secretpassword
    depends_on:
      postgres:
        condition: service_healthy

  postgres:
    image: postgres:16-alpine
    environment:
      POSTGRES_DB: app_db
      POSTGRES_USER: postgres
      POSTGRES_PASSWORD: secretpassword
    ports:
      - "5432:5432"
    volumes:
      - postgres_data:/var/lib/postgresql/data
    healthcheck:
      test: ["CMD-SHELL", "pg_isready -U postgres"]
      interval: 5s
      timeout: 5s
      retries: 5

volumes:
  postgres_data:
```

---

## 5. 5 Practical Production Examples

### Example 1: Complete Kubernetes Deployment Manifest with Probes
```yaml
apiVersion: apps/v1
kind: Deployment
metadata:
  name: order-service
  labels:
    app: order-service
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
      terminationGracePeriodSeconds: 60
      containers:
        - name: order-service
          image: myregistry.azurecr.io/order-service:1.0.0
          imagePullPolicy: IfNotPresent
          ports:
            - containerPort: 8080
              name: http
            - containerPort: 9090
              name: management
          envFrom:
            - configMapRef:
                name: order-service-config
            - secretRef:
                name: order-service-secrets
          resources:
            requests:
              cpu: "500m"
              memory: "1Gi"
            limits:
              cpu: "2000m"
              memory: "2Gi"
          lifecycle:
            preStop:
              exec:
                command: ["sh", "-c", "sleep 15"]
          startupProbe:
            httpGet:
              path: /actuator/health/liveness
              port: 9090
            failureThreshold: 30
            periodSeconds: 2
          livenessProbe:
            httpGet:
              path: /actuator/health/liveness
              port: 9090
            failureThreshold: 3
            periodSeconds: 10
          readinessProbe:
            httpGet:
              path: /actuator/health/readiness
              port: 9090
            failureThreshold: 2
            periodSeconds: 5
```

### Example 2: Kubernetes Service & ConfigMap
```yaml
apiVersion: v1
kind: Service
metadata:
  name: order-service
spec:
  type: ClusterIP
  selector:
    app: order-service
  ports:
    - name: http
      port: 80
      targetPort: 8080

---
apiVersion: v1
kind: ConfigMap
metadata:
  name: order-service-config
data:
  SPRING_PROFILES_ACTIVE: "prod"
  SERVER_SHUTDOWN: "graceful"
  SPRING_THREADS_VIRTUAL_ENABLED: "true"
  MANAGEMENT_SERVER_PORT: "9090"
```

### Example 3: Kubernetes HorizontalPodAutoscaler (HPA)
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
  maxReplicas: 15
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
```

### Example 4: Graceful Shutdown Configuration (application.yaml)
```yaml
server:
  port: 8080
  shutdown: graceful

management:
  server:
    port: 9090

spring:
  lifecycle:
    timeout-per-shutdown-phase: 30s
```

### Example 5: CDS (Class Data Sharing) Optimized Dockerfile
```dockerfile
# Generate CDS archive for fast startup
FROM eclipse-temurin:21-jre-alpine AS cds-builder
WORKDIR /application
COPY target/app.jar app.jar
RUN java -Djarmode=tools -jar app.jar extract --layers --destination extracted
WORKDIR /application/extracted
RUN java -XX:ArchiveClassesAtExit=app-cds.jsa -Dspring.context.exit=onRefresh -jar application/app.jar

FROM eclipse-temurin:21-jre-alpine
WORKDIR /application
COPY --from=cds-builder /application/extracted/ ./
ENTRYPOINT ["java", "-XX:SharedArchiveFile=app-cds.jsa", "-jar", "application/app.jar"]
```

---

## 6. Best Practices
1. **Always Use Layered JAR Extraction**: Do not `COPY app.jar` directly; layered extraction ensures dependencies are cached across builds.
2. **Set `MaxRAMPercentage` Instead of Hard `-Xmx`**: Use `-XX:MaxRAMPercentage=75.0` to dynamically adapt heap size to Kubernetes container limits.
3. **Include `preStop` Sleep Hook in Kubernetes**: A `sleep 15` command allows Kubernetes Ingress controllers time to remove the terminating pod from service endpoints before Spring closes the socket.
4. **Configure Explicit Startup Probes**: Prevents slow-starting Spring Boot apps from being prematurely killed by liveness probes.
5. **Always Run as Non-Root**: Ensure the Docker container switches to an unprivileged UID before executing the JVM.

---

## 7. Common Mistakes to Avoid

| Mistake | Consequence | Remedy |
|---|---|---|
| Setting hard `-Xmx` exceeding container memory limit | Container is instantly killed with Linux OOMKilled (Exit Code 137) | Use `-XX:MaxRAMPercentage=75.0` |
| Omitting `preStop` hook on terminating pods | In-flight HTTP requests receive 502 Bad Gateway during pod rollover | Add `preStop: exec: command: ["sh", "-c", "sleep 15"]` |
| Pointing Liveness probe to `/actuator/health` | Downstream DB outage causes Kubernetes to crash-loop all pods | Point liveness probe strictly to `/actuator/health/liveness` |
| Single-layer Docker builds | Every 1-line code change re-uploads 150MB of unchanged JAR dependencies | Use multi-stage build with `extract --layers` |
| Running container as `root` | High security vulnerability; allows potential host breakout | Use `RUN adduser -S appuser` and `USER appuser` |
| Missing `server.shutdown=graceful` | Active database writes are abruptly aborted on pod restart | Enable `server.shutdown=graceful` in configuration |

---

## 8. Performance & Security Considerations
- **Cgroup v2 Awareness**: OpenJDK 21 automatically detects memory and CPU quota limits set in Kubernetes manifests.
- **Exit on OOM**: Add `-XX:+ExitOnOutOfMemoryError` so the JVM crashes immediately on out-of-memory errors, triggering Kubernetes restart.
- **Read-Only Root Filesystem**: Configure Kubernetes `securityContext.readOnlyRootFilesystem: true` with an ephemeral `/tmp` volume.

---

## 9. Comparison Table: JVM Container Packaging Strategies

| Packaging Approach | Startup Time | Image Size | Docker Layer Caching | GraalVM Dependency |
|---|---|---|---|---|
| Fat JAR (`java -jar`) | Standard (~3-5s) | Moderate (~250MB) | Terrible (rebuilds entire JAR) | No |
| Layered JAR Extraction | Fast (~2-4s) | Moderate (~250MB) | Excellent (caches dependencies) | No |
| CDS (Class Data Sharing) | Very Fast (~1-2s) | Moderate (~280MB) | Excellent | No |
| Native Image (AOT) | Instantaneous (~50ms) | Ultra-Small (~80MB) | Excellent | Yes (Requires AOT compilation) |

---

## 10. Technical Interview Questions & Answers

<details>
<summary>1. How does Spring Boot layered JAR packaging improve Docker build times?</summary>
Spring Boot splits JAR contents into 4 layers: dependencies, Spring Boot loader, snapshot dependencies, and application classes. Unchanged dependency layers are cached by Docker, meaning code changes only trigger re-uploading the tiny application layer (typically < 1MB).
</details>

<details>
<summary>2. Why is `-XX:MaxRAMPercentage` preferred over `-Xmx` in Kubernetes?</summary>
In containerized environments, memory limits are dynamic and defined in Kubernetes manifests (`resources.limits.memory`). `-XX:MaxRAMPercentage` dynamically scales the JVM heap proportional to the container's cgroup memory limit without hardcoding flags.
</details>

<details>
<summary>3. Why is a `preStop` hook necessary when Kubernetes terminates a Pod?</summary>
When a Pod terminates, the Kubernetes Service endpoint deregistration and the `SIGTERM` signal happen concurrently. The `preStop` sleep hook delays the `SIGTERM` signal, ensuring the Ingress proxy updates its routing table before Spring stops accepting connections.
</details>

<details>
<summary>4. What does Exit Code 137 indicate in Kubernetes?</summary>
Exit Code 137 signifies that the Linux kernel OOM Killer terminated the container because its memory usage exceeded the configured Kubernetes memory limit (`128 + 9 (SIGKILL)`).
</details>

<details>
<summary>5. How do Startup Probes solve the slow-start dilemma?</summary>
Startup probes disable liveness and readiness checks until the application has completely started up, giving heavy applications enough time to warm up without risking sudden restarts.
</details>

<details>
<summary>6. What is the role of Class Data Sharing (CDS) in Spring Boot 3.3+?</summary>
CDS dumps JVM class metadata into an archive (`.jsa`) file. On subsequent boots, the JVM memory-maps this archive, reducing class loading time and memory footprint by up to 30%.
</details>

<details>
<summary>7. How do you ensure Spring Boot Actuator is not exposed outside the cluster?</summary>
Run Actuator on an isolated port (`management.server.port: 9090`) and only expose port 8080 in the public Kubernetes Ingress definition.
</details>

<details>
<summary>8. What is the difference between CPU requests and CPU limits?</summary>
CPU requests reserve guaranteed CPU capacity for scheduling. CPU limits enforce a hard cgroup quota; exceeding the limit causes CPU throttling rather than container termination.
</details>

---

## 11. Hands-On Practice Exercises

<details>
<summary>Exercise 1: Create a multi-stage layered Dockerfile</summary>
Write a multi-stage Dockerfile extracting Spring Boot layers and running as an unprivileged user.
</details>

<details>
<summary>Exercise 2: Write a docker-compose.yaml with health checks</summary>
Define a composition containing Spring Boot and PostgreSQL with health dependency conditions.
</details>

<details>
<summary>Exercise 3: Write a Kubernetes Deployment with probe alignment</summary>
Create a manifest configuring startup, liveness, and readiness probes against port 9090.
</details>

<details>
<summary>Exercise 4: Test graceful shutdown locally</summary>
Trigger a long-running request, send `SIGTERM` to the Spring Boot process, and verify the request completes cleanly.
</details>

<details>
<summary>Exercise 5: Configure HPA auto-scaling</summary>
Write an HPA manifest scaling between 2 and 10 pods when CPU utilization exceeds 75%.
</details>

<details>
<summary>Exercise 6: Test OOMKilled prevention with JVM flags</summary>
Run a Docker container with `--memory=512m` and configure `-XX:MaxRAMPercentage=70.0` to verify heap limits.
</details>

---

## 12. Summary & Cheat Sheet

```
+---------------------------------------------------------------------------------+
|                   DOCKER & KUBERNETES CHEAT SHEET                               |
+---------------------------------------------------------------------------------+
| LAYER EXTRACT  | java -Djarmode=tools -jar app.jar extract --layers             |
| CONTAINER MEM  | -XX:MaxRAMPercentage=75.0 -XX:+ExitOnOutOfMemoryError          |
| NON-ROOT USER  | RUN adduser -S appuser && USER appuser                         |
| LIVENESS PROBE | httpGet: /actuator/health/liveness (checks internal deadlocks) |
| READINESS PROBE| httpGet: /actuator/health/readiness (checks DB & downstream)   |
| PRE-STOP HOOK  | lifecycle: preStop: exec: command: ["sh", "-c", "sleep 15"]   |
| GRACEFUL STOP  | server.shutdown=graceful, timeout-per-shutdown-phase=30s       |
+---------------------------------------------------------------------------------+
```

---

## 13. Further Reading & References
- [Spring Boot Docker Official Guide](https://docs.spring.io/spring-boot/docs/current/reference/html/container-images.html)
- [Kubernetes Best Practices: Pod Termination Lifecycle](https://kubernetes.io/docs/concepts/workloads/pods/pod-lifecycle/)
- [Eclipse Temurin Alpine Container Images](https://hub.docker.com/_/eclipse-temurin)
- [Baeldung: Spring Boot CDS Optimization](https://www.baeldung.com/spring-boot-class-data-sharing)
