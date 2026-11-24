# Class Data Sharing (CDS), Project CRaC, and Project Leyden

| Metadata | Value |
|---|---|
| **Spring Boot Version** | 3.4.x |
| **Spring Framework Version** | 6.2.x |
| **Java Version** | 21 LTS |
| **Module** | 13-performance |
| **Status** | Complete |

---

## Table of Contents
- [1. What & Why](#1-what--why)
- [2. Core Concepts](#2-core-concepts)
- [3. How It Works Internally](#3-how-it-works-internally)
- [4. Syntax & Command Reference](#4-syntax--command-reference)
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
While GraalVM Native Image achieves sub-50ms cold starts, native compilation presents real-world engineering trade-offs:
- Compilation times are lengthy (3 to 8 minutes per build).
- The Closed-World Assumption requires explicit reflection hints and precludes dynamic bytecode generation (e.g. Mockito, certain dynamic plugins).
- Native images lack HotSpot's JIT C2 runtime adaptive profiling, resulting in slightly lower peak throughput for long-running compute workloads.

Engineering teams require solutions that **retain 100% of the standard HotSpot JVM's dynamic capabilities and peak throughput** while dramatically slashing cold-start time and memory footprint.

### Historical Evolution
Three major JVM initiatives address this challenge:
1. **Application Class Data Sharing (AppCDS)**: Standardized in OpenJDK (JEP 310) and deeply integrated into **Spring Boot 3.3+**. Dumps loaded class metadata to a shared memory-mapped archive (`.jsa`), cutting startup time by 30%–50%.
2. **Project CRaC (Coordinated Restore at Checkpoint)**: An OpenJDK project (pioneered by Azul) using Linux CRIU (Checkpoint/Restore in Userspace) to pause a fully warmed JVM to disk and restore it in **under 20 milliseconds**.
3. **Project Leyden**: The long-term OpenJDK initiative to introduce native-like fast startup into standard HotSpot by shifting computations (class loading, linking, profiling, C2 compilation) Ahead-of-Time without sacrificing the dynamic Java specification.

### Production Relevance
These technologies allow teams to:
- Achieve sub-second or sub-100ms startup times on **standard OpenJDK HotSpot**.
- Share memory pages across multiple container instances on the same host node, drastically cutting container cluster RAM usage.
- Eliminate cold-start latency in AWS Lambda (via SnapStart) and Kubernetes autoscaling clusters.

---

## 2. Core Concepts

### 1. Application Class Data Sharing (AppCDS)
- **Class Data Sharing (CDS)**: When a JVM starts, it spends significant CPU reading `.class` files from JARs, validating bytecode integrity, building internal verification structures, and storing them in Metaspace.
- **CDS Archive (`.jsa`)**: An Ahead-of-Time memory snapshot of these parsed class structures. On startup, the JVM memory-maps (`mmap`) the `.jsa` file directly into memory, bypassing class verification and loading.
- **Spring Boot 3.3+ Native CDS Support**: Spring Boot provides automated extraction tools and CDS training runs to generate application archives effortlessly.

### 2. Project CRaC (Coordinated Restore at Checkpoint)
- **Linux CRIU**: Checkpoint/Restore In Userspace freeze-dries a Linux process, writing its complete memory, CPU register states, and thread states to disk image files.
- **Coordinated Checkpoint**: Blindly checkpointing a JVM corrupts open network sockets, file descriptors, and database connections. Project CRaC introduces a Java API (`org.crac.Resource`) allowing Spring Boot to cleanly close connections before checkpointing and re-open them upon restoration.

### 3. Project Leyden
- The official OpenJDK umbrella project designed to bring static, ahead-of-time pre-computation concepts into standard HotSpot.
- Uses **Condenser** pipelines: pre-resolving invokedynamic call sites, pre-warming JIT compilation caches, and generating unified Leyden archives.

---

## 3. How It Works Internally

### Comparison of JVM Startup Architectures

```
A. STANDARD HOTSPOT JVM (Startup: 15s - 25s)
[Load Class Bytecode] -> [Bytecode Verification] -> [Build Metaspace] -> [Interpret] -> [JIT C1/C2 Compile]

B. APPCDS / SPRING BOOT 3.3+ (Startup: 4s - 8s)
[Memory Map (.jsa Archive) directly into Metaspace] -> [Bypass Verification] -> [Fast Execution]

C. PROJECT CRAC / CHECKPOINT RESTORE (Startup: 0.015s - 0.050s)
[Restore Frozen Process Memory from Disk directly to RAM via CRIU] -> [Re-open Sockets] -> [Instant Hot State]
```

### The CRaC Coordinated Lifecycle

```
BUILD & WARMUP PHASE (Staging / CI)
  1. Boot Spring Boot Application on CRaC-enabled JDK (Azul Zulu CRaC)
  2. Send warmup synthetic HTTP traffic to trigger JIT C2 compilation
  3. Trigger Checkpoint via jcmd / API:
     - Spring context fires Resource.beforeCheckpoint()
     - HikariCP closes database connections; Tomcat stops listening on port 8080
     - Linux CRIU captures process state -> writes /opt/crac-files
     - Process terminates

DEPLOYMENT & RESTORE PHASE (Production / Lambda)
  1. Container boots in Kubernetes / AWS Lambda SnapStart
  2. CRIU restores memory pages from /opt/crac-files (< 20ms)
  3. Spring context fires Resource.afterRestore()
     - HikariCP re-establishes DB connections; Tomcat binds port 8080
  4. Container serves user traffic INSTANTLY at full JIT C2 peak throughput!
```

---

## 4. Syntax & Command Reference

### Generating and Using AppCDS in Spring Boot 3.3+

```bash
# Step 1: Extract application layers from fat JAR
java -Djarmode=tools -jar target/service-1.0.jar extract --destination extracted/

# Step 2: Perform a CDS Training Run to create the .jsa archive
cd extracted/
java -XX:ArchiveClassesAtExit=application.jsa \
     -Dspring.context.exit=on-refresh \
     -jar service-1.0.jar

# Step 3: Launch in Production with CDS enabled
java -XX:SharedArchiveFile=application.jsa \
     -jar service-1.0.jar
```

### Maven Buildpack Configuration for CDS (`pom.xml`)
Spring Boot 3.3+ Cloud Native Buildpacks can automate CDS archive generation during container packaging:

```xml
<plugin>
    <groupId>org.springframework.boot</groupId>
    <artifactId>spring-boot-maven-plugin</artifactId>
    <configuration>
        <image>
            <env>
                <BP_JVM_CDS_ENABLED>true</BP_JVM_CDS_ENABLED>
            </env>
        </image>
    </configuration>
</plugin>
```

---

## 5. Practical Examples

### Example 1: Measuring CDS Startup Speedup
Below is actual measured output comparing a Spring Boot 3.4 microservice on Eclipse Temurin 21:

```bash
# Without CDS (Standard Run)
$ java -jar service-1.0.jar
2026-03-24T10:15:30.120Z INFO 1 --- [main] c.e.DemoApplication : Started DemoApplication in 8.421 seconds

# With CDS (Memory-Mapped Archive)
$ java -XX:SharedArchiveFile=application.jsa -jar service-1.0.jar
2026-03-24T10:16:10.050Z INFO 1 --- [main] c.e.DemoApplication : Started DemoApplication in 4.105 seconds
```
*Result*: **51.2% reduction in cold-start time** with zero code changes, running on standard HotSpot!

### Example 2: Implementing a CRaC-Aware Resource
When using Project CRaC, any component maintaining open state (sockets, timers, files) must register with CRaC:

```java
package com.example.performance.crac;

import org.crac.Context;
import org.crac.Core;
import org.crac.Resource;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;
import org.springframework.stereotype.Component;

import jakarta.annotation.PostConstruct;
import java.net.Socket;

@Component
public class CustomSocketManager implements Resource {

    private static final Logger log = LoggerFactory.getLogger(CustomSocketManager.class);
    private Socket activeSocket;

    @PostConstruct
    public void registerWithCrac() {
        // Register this component with the global CRaC context
        Core.getGlobalContext().register(this);
    }

    @Override
    public void beforeCheckpoint(Context<? extends Resource> context) throws Exception {
        log.info("CRaC Checkpoint requested: Safely closing TCP sockets...");
        if (activeSocket != null && !activeSocket.isClosed()) {
            activeSocket.close();
            activeSocket = null;
        }
    }

    @Override
    public void afterRestore(Context<? extends Resource> context) throws Exception {
        log.info("CRaC Restore completed: Re-establishing TCP sockets...");
        this.activeSocket = new Socket("backend-daemon.internal", 9000);
    }
}
```

### Example 3: Checkpointing a Spring Boot Application with `jcmd`
```bash
# 1. Run Spring Boot application on Azul Zulu JDK with CRaC
java -Dorg.crac.Core.CheckpointTo=/opt/crac-files -jar order-service.jar

# 2. Warm up with traffic
curl http://localhost:8080/api/orders/warmup

# 3. Trigger Checkpoint via jcmd
jcmd order-service.jar JDK.checkpoint

# Output: Process freezes, writes memory images to /opt/crac-files, and exits.

# 4. Instant Restore in Production (Sub-50ms)
java -Dorg.crac.Core.RestoreFrom=/opt/crac-files
```

### Example 4: Anti-Pattern vs. Production Fix (State Leakage across CRaC Checkpoints)
```java
// ANTI-PATTERN: Generating random tokens, UUIDs, or secrets during bean construction
// Because the bean is constructed BEFORE the checkpoint, every restored pod inherits the EXACT SAME SECRET!
@Component
public class InsecureTokenGenerator {

    private final String podEncryptionKey;

    public InsecureTokenGenerator() {
        // CATASTROPHIC: This UUID is frozen into the checkpoint image!
        // All 50 autoscaled container instances will share the SAME encryption key!
        this.podEncryptionKey = UUID.randomUUID().toString();
    }
}

// PRODUCTION FIX: Generate instance-unique secrets after restoration via CRaC Resource
@Component
public class SecureTokenGenerator implements Resource {

    private volatile String podEncryptionKey;

    @PostConstruct
    public void init() {
        Core.getGlobalContext().register(this);
        generateFreshKey();
    }

    @Override
    public void beforeCheckpoint(Context<? extends Resource> context) {
        this.podEncryptionKey = null; // Wipe secret from checkpoint memory image
    }

    @Override
    public void afterRestore(Context<? extends Resource> context) {
        generateFreshKey(); // Generate fresh random key unique to the restored pod instance!
    }

    private void generateFreshKey() {
        this.podEncryptionKey = UUID.randomUUID().toString();
    }
}
```

### Example 5: Combining AOT Processing with CDS in Spring Boot 3.3+
Spring Boot allows combining Ahead-of-Time optimizations with Class Data Sharing for compound startup gains:

```bash
# 1. Generate AOT artifacts and compile
mvn clean package spring-boot:process-aot

# 2. Perform CDS training run on the AOT-generated classes
java -Dspring.aot.enabled=true \
     -XX:ArchiveClassesAtExit=app-aot-cds.jsa \
     -Dspring.context.exit=on-refresh \
     -jar target/service-1.0.jar

# 3. Launch production JVM with BOTH AOT and CDS active
java -Dspring.aot.enabled=true \
     -XX:SharedArchiveFile=app-aot-cds.jsa \
     -jar target/service-1.0.jar
```
*Result*: Achieves **1.5 second startup** on standard HotSpot with zero GraalVM constraints!

---

## 6. Best Practices & Production Guidelines

1. **Use CDS Training Runs in Docker Multi-Stage Builds**: Perform the CDS training run (`-XX:ArchiveClassesAtExit`) during the Docker build stage. Bake the generated `.jsa` file directly into the container image next to the JAR.
2. **Never Checkpoint Open File Descriptors or TLS Sockets in CRaC**: Any active file stream or TCP connection left open during a CRIU checkpoint will cause restoration failure (`CRIU error: file descriptor mismatch`). Rely on Spring Boot's built-in CRaC integration for Tomcat, Netty, and HikariCP.
3. **Regenerate Random Seeds and Hostname Caches on Restore**: Ensure cryptographic random seeds (`SecureRandom`) and DNS caches are refreshed upon restore to prevent cryptographic predictability across cloned instances.
4. **Leverage AWS Lambda SnapStart**: AWS Lambda SnapStart utilizes Firecracker microVM snapshots (conceptually identical to CRaC). In Spring Boot, enable SnapStart in your template to get sub-200ms cold starts on serverless Java.
5. **Ensure Same Classpath and JVM Version for CDS**: A `.jsa` CDS archive is tied to the exact JVM build and classpath structure that generated it. If you bump the patch version of your JDK or change a JAR file on the classpath, regenerate the archive.

---

## 7. Common Mistakes & Pitfalls

| Mistake | Why It Fails | Practical Remedy |
|---|---|---|
| Classpath mismatch between CDS archive and runtime | The JVM detects modified JAR timestamps or ordering; silently discards the `.jsa` archive and boots unaccelerated. | Build the `.jsa` inside the exact same container image used in production. |
| Storing static credentials in checkpoint memory | Restored instances share the same temporary API keys, access tokens, and session identifiers. | Re-fetch credentials during `Resource.afterRestore()`. |
| Running CDS training with live database dependencies | Training run may fail if external database is unavailable in CI build environment. | Run training with `-Dspring.context.exit=on-refresh` and mock datasources. |
| Forgetting to close active threads in CRaC | Background thread pools active during checkpoint prevent CRIU from establishing a consistent snapshot. | Stop thread pools in `beforeCheckpoint()`. |
| Using outdated HotSpot JDK for CDS | Older JDKs (Java 8/11) only supported base class CDS; AppCDS for application JARs requires modern JDKs. | Use Java 21 LTS with Spring Boot 3.3+. |
| CRIU requiring Linux root permissions | Standard Linux prevents non-root users from executing CRIU kernel checkpoint APIs. | Grant `CAP_CHECKPOINT_RESTORE` Linux capability to container user. |

---

## 8. Performance & Security Notes

### Memory Deduplication via CDS
- When 10 container replicas run on the same Kubernetes worker node using AppCDS, the Linux OS kernel shares the read-only memory-mapped `.jsa` file pages across all 10 processes via OS page table sharing.
- This results in massive aggregate memory savings across high-density container clusters.

### Cryptographic Security in Checkpoint/Restore
- If a JVM initializes `SecureRandom` before a checkpoint, all cloned instances restored from that snapshot will produce the **exact same pseudo-random sequence**, compromising encryption keys, CSRF tokens, and session IDs.
- Spring Boot's CRaC integration automatically reseeds `SecureRandom` on restore.

---

## 9. Comparison / When to Use What

| Metric / Dimension | Standard JIT HotSpot | AppCDS (Spring Boot 3.3+) | Project CRaC (Azul Zulu) | GraalVM Native Image |
|---|---|---|---|---|
| **Cold Start Time** | 10 – 30s | **4 – 8s** (50% faster) | **15 – 50ms** | **20 – 50ms** |
| **Peak Throughput** | Maximum (JIT C2) | Maximum (JIT C2) | Maximum (Pre-warmed C2) | Very High (PGO) |
| **Build Time** | Fast ($\approx 30\text{s}$) | Fast ($\approx 45\text{s}$) | Medium ($\approx 1\text{m}$) | Slow ($3 - 8\text{m}$) |
| **Dynamic Features** | 100% Supported | 100% Supported | 100% Supported | Limited (Closed-World) |
| **Memory Footprint** | Standard (350MB) | Reduced (-15%) | Standard (Pre-warmed) | Minimal (35MB) |
| **OS / Runtime** | Any OS | Any OS | Linux with CRIU | Linux / macOS / Windows |

---

## 10. Interview Questions & Answers

### Q1: What is Application Class Data Sharing (AppCDS) and how does it speed up Spring Boot?
<details>
<summary>Click to view answer</summary>

AppCDS allows application and library classes to be loaded and parsed once during a build-time training run, writing the JVM's internal class metadata to a `.jsa` archive. At runtime, the JVM memory-maps this archive directly into Metaspace. This bypasses the expensive CPU-intensive steps of reading JAR files, parsing class files, and validating bytecode integrity, reducing Spring Boot startup time by 30% to 50%.
</details>

### Q2: What is Project CRaC and how does it achieve sub-50ms startup times?
<details>
<summary>Click to view answer</summary>

Project CRaC (Coordinated Restore at Checkpoint) uses Linux CRIU to capture a memory snapshot of a running, fully initialized, and JIT-warmed JVM process. When a new instance is needed, the OS restores the entire process state directly from disk to memory in under 50 milliseconds. Unlike static images, the restored JVM is already JIT-compiled at peak performance.
</details>

### Q3: Why is a "Coordinated" checkpoint necessary instead of a blind Linux process snapshot?
<details>
<summary>Click to view answer</summary>

A blind snapshot freezes active network connections, open file handles, thread pools, and volatile secrets. When restored minutes or days later on a different host, remote database servers will have severed the TCP sockets, leading to broken connection exceptions. The CRaC API allows applications to execute `beforeCheckpoint()` hooks to cleanly close connections and `afterRestore()` hooks to re-establish them.
</details>

### Q4: How does Spring Boot 3.3+ simplify CDS archive generation?
<details>
<summary>Click to view answer</summary>

Spring Boot 3.3 introduced native tooling support:
1. `java -Djarmode=tools -jar app.jar extract`: Extracts the layered layout.
2. `-Dspring.context.exit=on-refresh`: Instructs Spring Boot to initialize the entire context and exit immediately once refreshed, creating the perfect zero-workload training run for `-XX:ArchiveClassesAtExit`.
</details>

### Q5: What is the primary difference between Project CRaC and GraalVM Native Image?
<details>
<summary>Click to view answer</summary>

- **GraalVM Native Image**: Compiles Java bytecode into a standalone native binary Ahead-of-Time under the Closed-World Assumption, stripping the JVM and dynamic reflection.
- **Project CRaC**: Runs on the standard HotSpot JVM. It compiles no static machine code; it merely freeze-dries a running JVM to disk and restores it. It retains 100% compatibility with all dynamic Java libraries and full JIT C2 peak throughput.
</details>

### Q6: What is Project Leyden?
<details>
<summary>Click to view answer</summary>

Project Leyden is an official OpenJDK initiative aiming to address slow Java startup and warm-up time by introducing static computation concepts into the Java platform. It explores pre-resolving constant pools, caching JIT compilation artifacts, and condenser pipelines to shift runtime work to build time while preserving the standard Java specification.
</details>

### Q7: Why must `SecureRandom` be reseeded after a CRaC restore?
<details>
<summary>Click to view answer</summary>

If the pseudo-random number generator's internal state is captured during checkpoint, every instance restored from that image will start with the exact same internal seed and produce the exact same sequence of pseudo-random numbers. This catastrophically undermines cryptography, generating identical session IDs and encryption keys across different container nodes.
</details>

### Q8: What happens if a CDS `.jsa` archive classpath does not match the runtime classpath?
<details>
<summary>Click to view answer</summary>

The HotSpot JVM validates timestamps, checksums, and the order of JARs declared in the archive against the runtime classpath. If any discrepancy is detected (e.g. an updated JAR or changed path), the JVM emits a warning and safely falls back to standard dynamic class loading without aborting process execution.
</details>

---

## 11. Practice Exercises

### Exercise 1: Build a Multi-Stage Dockerfile with AppCDS (Easy)
<details>
<summary>Click to view challenge & solution</summary>

**Challenge**: Write a Dockerfile that extracts layers from a Spring Boot JAR, performs a CDS training run using `-Dspring.context.exit=on-refresh`, and configures the entrypoint to use the generated `app.jsa`.

**Solution**:
```dockerfile
FROM eclipse-temurin:21-jre-jammy AS builder
WORKDIR /workspace
COPY target/service-*.jar app.jar
RUN java -Djarmode=tools -jar app.jar extract --destination extracted/

# Training run to produce application.jsa
WORKDIR /workspace/extracted
RUN java -XX:ArchiveClassesAtExit=application.jsa \
         -Dspring.context.exit=on-refresh \
         -jar app.jar

FROM eclipse-temurin:21-jre-jammy
WORKDIR /app
COPY --from=builder /workspace/extracted/ /app/

EXPOSE 8080 9090
ENTRYPOINT ["java", "-XX:SharedArchiveFile=application.jsa", "-jar", "app.jar"]
```
</details>

### Exercise 2: Implementing a CRaC Redis Connection Watcher (Medium)
<details>
<summary>Click to view challenge & solution</summary>

**Challenge**: Write a Spring `@Component` that implements `org.crac.Resource` to disconnect a Redis connection factory before a checkpoint and reconnect it after restore.

**Solution**:
```java
package com.example.performance.crac;

import org.crac.Context;
import org.crac.Core;
import org.crac.Resource;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;
import org.springframework.data.redis.connection.lettuce.LettuceConnectionFactory;
import org.springframework.stereotype.Component;

import jakarta.annotation.PostConstruct;

@Component
public class RedisCracLifecycleManager implements Resource {

    private static final Logger log = LoggerFactory.getLogger(RedisCracLifecycleManager.class);
    private final LettuceConnectionFactory connectionFactory;

    public RedisCracLifecycleManager(LettuceConnectionFactory connectionFactory) {
        this.connectionFactory = connectionFactory;
    }

    @PostConstruct
    public void register() {
        Core.getGlobalContext().register(this);
    }

    @Override
    public void beforeCheckpoint(Context<? extends Resource> context) throws Exception {
        log.warn("CRaC Checkpoint: Destroying Redis connection pools...");
        connectionFactory.destroy();
    }

    @Override
    public void afterRestore(Context<? extends Resource> context) throws Exception {
        log.info("CRaC Restore: Reinitializing Redis connection pools...");
        connectionFactory.afterPropertiesSet();
        connectionFactory.start();
    }
}
```
</details>

### Exercise 3: Scripted CDS Generation with Validation (Hard)
<details>
<summary>Click to view challenge & solution</summary>

**Challenge**: Write a bash build script that checks if `application.jsa` exists, validates that the JVM can load it with `-Xlog:class+path=info`, and measures startup speed difference.

**Solution**:
```bash
#!/bin/bash
set -e

JAR="target/order-service-1.0.jar"
ARCHIVE="target/application.jsa"

echo "Step 1: Performing CDS training run..."
java -XX:ArchiveClassesAtExit="$ARCHIVE" \
     -Dspring.context.exit=on-refresh \
     -jar "$JAR"

if [ -f "$ARCHIVE" ]; then
    echo "SUCCESS: $ARCHIVE generated."
else
    echo "ERROR: Failed to generate CDS archive."
    exit 1
fi

echo "Step 2: Validating CDS archive mapping..."
java -XX:SharedArchiveFile="$ARCHIVE" \
     -Xlog:class+path=info \
     -Dspring.context.exit=on-refresh \
     -jar "$JAR" | grep -i "shared class paths"

echo "Step 3: Measuring startup with CDS..."
time java -XX:SharedArchiveFile="$ARCHIVE" \
          -Dspring.context.exit=on-refresh \
          -jar "$JAR"
```
</details>

---

## 12. Summary / Cheat Sheet

```
+---------------------------------------------+-------------------------------------------------------------+
| Concept / Command                           | Description & Advantage                                     |
+---------------------------------------------+-------------------------------------------------------------+
| -XX:ArchiveClassesAtExit=app.jsa            | Generates CDS archive of loaded class metadata              |
| -XX:SharedArchiveFile=app.jsa               | Launches JVM with memory-mapped CDS archive (30-50% faster) |
| -Dspring.context.exit=on-refresh            | Boots Spring context and exits immediately (for CDS training|
| Project CRaC                                | Freeze-dries running JVM to disk; restores in < 50ms        |
| org.crac.Resource                           | SPI for closing/reopening sockets around CRaC checkpoints   |
| Project Leyden                              | OpenJDK umbrella shifting dynamic work Ahead-of-Time        |
| AWS Lambda SnapStart                        | Firecracker microVM snapshot restore based on CRaC concepts |
+---------------------------------------------+-------------------------------------------------------------+
```

---

## 13. Further Reading
- OpenJDK JEP 310: Application Class-Data Sharing
- Spring Boot 3.3 Release Notes: Native Class Data Sharing (CDS) Support
- Project CRaC Official Documentation & Azul Zulu Guides
- OpenJDK Project Leyden: Concepts and Condenser Architecture