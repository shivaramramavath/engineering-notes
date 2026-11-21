# Packaging and Deployment in Java

> Enterprise packaging and modern deployment architectures for Java workloads: Fat JARs, layered Docker containerization, minimal runtimes with `jlink`, Application Class Data Sharing (AppCDS), and GraalVM Native Image considerations.

| Metadata | Details |
| :--- | :--- |
| **Level** | Advanced / Enterprise DevOps |
| **Prerequisites** | [18_build-and-dependencies](../18-build-and-dependencies/README.md), [05_packages-and-modules](../05-packages-and-modules/README.md), [15_jvm-internals](../15-jvm-internals/README.md) |
| **Time to Read** | 35 minutes |
| **Status** | Verified Production Guide |
| **Java Baseline** | Java 17 LTS / Java 21 LTS / Java 25 LTS |

---

## Table of Contents
1. [What & Why](#1-what--why)
2. [Core Concepts](#2-core-concepts)
3. [How It Works Internally](#3-how-it-works-internally)
4. [Syntax & Usage](#4-syntax--usage)
5. [Practical Examples](#5-practical-examples)
6. [Best Practices](#6-best-practices)
7. [Common Mistakes & Pitfalls](#7-common-mistakes--pitfalls)
8. [Performance & Security Notes](#8-performance--security-notes)
9. [Comparison / When to Use What](#9-comparison--when-to-use-what)
10. [Interview Questions & Answers](#10-interview-questions--answers)
11. [Practice Exercises](#11-practice-exercises)
12. [Summary / Cheat Sheet](#12-summary--cheat-sheet)
13. [Further Reading](#13-further-reading)

---

## 1. What & Why

### What is Modern Java Packaging?
Historically, enterprise Java relied on heavy application servers (WebLogic, WebSphere, JBoss) hosting monolithic `.war` or `.ear` deployment archives.

Modern Java packaging focuses on **self-contained, immutable microservices**:
- **Fat JARs (Uber-JARs)**: Bundling application bytecode, all third-party dependencies, and an embedded web server (Tomcat, Jetty, Netty) into a single executable `.jar`.
- **Layered Container Images**: Splitting dependencies, Spring Boot loaders, snapshot dependencies, and application classes into discrete Docker filesystem layers for rapid container caching and CI/CD distribution.
- **Custom Java Runtimes (`jlink`)**: Stripping unused JDK modules to produce custom, stripped-down runtimes as small as 35–50 MB.
- **Class Data Sharing (AppCDS)**: Pre-parsing and pre-mapping JVM class metadata to disk archives to achieve near-instantaneous startup times.

### Why Packaging Architecture Matters
In modern Kubernetes environments running hundreds of microservice replicas:
- Large 500 MB container images saturate network bandwidth, delay auto-scaling events, and balloon container registry storage costs.
- Slow cold start times (10–30 seconds) impede serverless deployments and scale-from-zero architectures.
- Properly layered and optimized Java images start in under 1 second and occupy less than 90 MB of disk space.

---

## 2. Core Concepts

### Fat JAR vs. Layered JAR
- **Monolithic Fat JAR**: All classes and library JARs are unpacked and merged into a single flat directory structure or embedded inside a nested loader archive.
  - *Problem*: Changing one line of business code invalidates the entire 80 MB Docker layer. Every continuous integration build must upload the full 80 MB layer to the container registry.
- **Layered JAR**: Separates artifacts by change frequency:
  1. `dependencies` (rarely change)
  2. `spring-boot-loader` (rarely changes)
  3. `snapshot-dependencies` (moderate change)
  4. `application` (changes on every commit)
  - *Benefit*: Docker caches the 70 MB dependencies layer. Rebuilds and pushes only transfer the tiny 200 KB application layer!

### Custom JRE with `jlink` (JEP 282)
The full JDK distribution contains hundreds of modules (Desktop Swing, Corba, XML tools, Management, Compilers). By analyzing the module graph with `jdeps`, `jlink` strips all unnecessary modules, producing a minimal Java runtime containing only what your application requires.

### Application Class Data Sharing (AppCDS, JEP 310)
When a JVM starts, it spends up to 40% of its initial boot time reading class files, verifying bytecode, and loading internal HotSpot metadata into memory.
- AppCDS runs a training pass, extracts all parsed class metadata, and dumps it into a shared memory-mapped archive file (`classes.jsa`).
- Subsequent JVM boots map `classes.jsa` directly into memory via `mmap()`, completely bypassing class parsing and bytecode verification for pre-loaded classes.

---

## 3. How It Works Internally

### Docker Layer Caching for Java Applications
```
+--------------------------------------------------------------------------+
|                  UNOPTIMIZED FAT JAR DOCKERFILE                          |
|  COPY target/app.jar app.jar  ---> Single 80MB Layer                     |
|  (Any 1-line code change causes full 80MB re-push to Docker Registry)    |
+--------------------------------------------------------------------------+
|                   OPTIMIZED LAYERED CONTAINER BUILD                      |
|  Layer 1: Base Linux / Minimal JRE (35 MB) ---------> CACHED 100%        |
|  Layer 2: Third-party Dependencies (45 MB) ---------> CACHED 99%         |
|  Layer 3: Internal Shared Libraries (5 MB) ----------> CACHED 90%         |
|  Layer 4: Application Classes (300 KB) -------------> PUSHED EVERY COMMIT|
+--------------------------------------------------------------------------+
```

### AppCDS Memory-Mapped Architecture
```
Disk Archive: classes.jsa
       |
       | OS mmap() System Call (Zero-Copy)
       v
JVM Virtual Memory Address Space
  +-----------------------------------------------------+
  | Read-Only Shared Metaspace Region                   |
  | [ java.lang.Object ][ java.lang.String ][ Spring...]|
  +-----------------------------------------------------+
       ^
       | Multiple JVM containers share identical physical RAM pages!
```
When multiple Java containers run on the same Kubernetes worker node, the Linux kernel shares the identical physical RAM pages for the read-only sections of `classes.jsa`, significantly reducing host memory footprint!

---

## 4. Syntax & Usage

### 1. Extracting Layered JARs via CLI
Spring Boot and modern packaging plugins support layer extraction:
```bash
# Extract layers into separate folders
java -Djarmode=layertools -jar target/application.jar extract
```
This produces four discrete directories:
- `dependencies/`
- `spring-boot-loader/`
- `snapshot-dependencies/`
- `application/`

### 2. Generating a Minimal JRE via `jdeps` and `jlink`
```bash
# Step 1: Detect required modules
jdeps --ignore-missing-deps --multi-release 21 --print-module-deps --class-path "lib/*" target/application.jar > modules.txt

# Step 2: Assemble custom runtime image
jlink   --add-modules $(cat modules.txt),jdk.crypto.ec   --strip-debug   --no-man-pages   --no-header-files   --compress=2   --output custom-jre
```

---

## 5. Practical Examples

### Example 1: Production Multi-Stage Layered Dockerfile
**Goal:** Build a production-grade, highly-cached Docker image on Java 21 with non-root security.

```dockerfile
# ==========================================
# Stage 1: Build Application
# ==========================================
FROM maven:3.9.6-eclipse-temurin-21 AS builder
WORKDIR /workspace
COPY pom.xml .
RUN mvn dependency:go-offline -B
COPY src src
RUN mvn clean package -DskipTests

# Extract layers using layertools
WORKDIR /workspace/target
RUN java -Djarmode=layertools -jar *.jar extract

# ==========================================
# Stage 2: Production Distroless Runtime
# ==========================================
FROM eclipse-temurin:21-jre-jammy AS runtime
WORKDIR /app

# Create unprivileged service user
RUN groupadd -r appgroup && useradd -r -g appgroup -u 10001 appuser

# Copy layers in order of lowest to highest change frequency
COPY --from=builder --chown=appuser:appgroup /workspace/target/dependencies/ ./
COPY --from=builder --chown=appuser:appgroup /workspace/target/spring-boot-loader/ ./
COPY --from=builder --chown=appuser:appgroup /workspace/target/snapshot-dependencies/ ./
COPY --from=builder --chown=appuser:appgroup /workspace/target/application/ ./

USER 10001:10001

EXPOSE 8080

ENTRYPOINT ["java",             "-XX:+UseContainerSupport",             "-XX:MaxRAMPercentage=75.0",             "org.springframework.boot.loader.launch.JarLauncher"]
```

### Example 2: Training and Activating AppCDS
**Goal:** Reduce microservice startup time from 4.5 seconds to 1.2 seconds using Dynamic Class Data Sharing.

```bash
# Step 1: Generate CDS Archive during container build or test phase
java -XX:ArchiveClassesAtExit=application.jsa      -Dspring.context.exit=onRefresh      -jar target/application.jar

# Step 2: Launch in production with memory-mapped CDS archive
java -XX:SharedArchiveFile=application.jsa      -XX:+UseContainerSupport      -XX:MaxRAMPercentage=75.0      -jar target/application.jar
```

### Example 3: Minimal Self-Contained JLink Distribution Script
**Goal:** Create an ultra-small 40 MB standalone package for an enterprise CLI tool.

```bash
#!/usr/bin/env bash
set -euo pipefail

TARGET_JAR="target/my-cli.jar"
OUTPUT_DIR="dist/runtime"

echo "Calculating required Java modules..."
MODULES=$(jdeps --print-module-deps --ignore-missing-deps "${TARGET_JAR}")

echo "Building custom runtime with modules: ${MODULES}"
rm -rf "${OUTPUT_DIR}"
jlink   --add-modules "${MODULES}"   --strip-debug   --no-header-files   --no-man-pages   --compress=zip-6   --output "${OUTPUT_DIR}"

echo "Runtime size:"
du -sh "${OUTPUT_DIR}"
```

### Example 4: Failure Case — Inefficient Monolithic Dockerfile
```dockerfile
# ANTI-PATTERN: Inefficient monolithic container packaging
FROM openjdk:21
COPY target/giant-application-0.0.1-SNAPSHOT.jar app.jar
# Every code change invalidates this entire 200MB layer!
# Pulling and pushing takes minutes instead of seconds!
ENTRYPOINT ["java", "-jar", "app.jar"]
```

---

## 6. Best Practices

1. **Always Use Layered JARs in Containers**: Split dependencies and application code into discrete Docker image layers.
2. **Run as Unprivileged UID**: Never deploy containers running as `root`. Specify `USER 10001` or use distroless images.
3. **Configure JVM for Containers**: Always specify `-XX:+UseContainerSupport` and `-XX:MaxRAMPercentage=75.0` (or 80.0) so the JVM respects cgroup memory limits.
4. **Use AppCDS for Fast Startup**: Enable Application Class Data Sharing in scale-to-zero or autoscaling environments.
5. **Pin Base Image Digests**: Use SHA-256 digests (`eclipse-temurin:21@sha256:...`) to ensure reproducible container builds.
6. **Multi-Stage Builds**: Never leave Maven, Gradle, or build-time compilers in the final production runtime container image.

---

## 7. Common Mistakes & Pitfalls

| Mistake | Why It Fails | Secure Fix |
| :--- | :--- | :--- |
| **Using `latest` tag for base images** | Builds become unpredictable; upstream changes break production images silently. | Pin exact tags or SHA-256 image digests. |
| **Omitting container memory limits** | JVM sees total host RAM (e.g. 128 GB), allocates 32 GB heap, gets killed by Linux OOM killer. | Set `-XX:MaxRAMPercentage=75.0`. |
| **Running as root user** | Container breakouts compromise host OS kernel with root privileges. | Enforce unprivileged UID (`USER 10001`). |
| **Copying full JAR in single Docker layer** | Wastes registry storage and network bandwidth on every commit. | Use layered JAR extraction (`layertools`). |
| **Leaving build tools in runtime image** | Bloats image size by 500+ MB and increases attack surface. | Use multi-stage Docker builds. |
| **Missing `jdk.crypto.ec` in `jlink`** | Custom JRE fails during TLS 1.3 handshakes when negotiating elliptic curves. | Explicitly include `jdk.crypto.ec` in `--add-modules`. |

---

## 8. Performance & Security Notes

### Native Image vs. HotSpot JIT Tradeoffs (GraalVM)
- **GraalVM Native Image**: Compiles Java bytecode ahead-of-time (AOT) into a native OS executable.
  - *Advantages*: Instantaneous startup (10–50 ms), tiny memory baseline (30 MB RAM).
  - *Disadvantages*: No dynamic class loading, limited runtime reflection, no JIT runtime profile-guided optimization (peak throughput can be 10–20% lower than C2 JIT under sustained load).
- **HotSpot JIT with AppCDS**: Offers the best of both worlds for long-running enterprise workloads: fast startup combined with adaptive JIT peak performance.

---

## 9. Comparison / When to Use What

| Packaging Architecture | Image Size | Startup Time | Peak Throughput | Complexity |
| :--- | :--- | :--- | :--- | :--- |
| **Monolithic Fat JAR** | Large (300+ MB) | Slow (5–15s) | Maximum | Very Low |
| **Layered Container JAR** | Moderate (150 MB) | Normal (4–10s)| Maximum | Low |
| **JLink Minimal Runtime** | Small (60–90 MB) | Fast (2–5s) | Maximum | Moderate |
| **AppCDS Optimized JAR** | Small (90 MB) | Very Fast (1–2s)| Maximum | Moderate |
| **GraalVM Native Image** | Tiny (35–50 MB) | Instant (0.05s) | Moderate / High | High |

---

## 10. Interview Questions & Answers

### Q1: What is a Layered JAR and why is it superior for Docker deployments?
<details>
<summary>View Answer</summary>
A Layered JAR physically separates third-party dependencies, loaders, snapshot libraries, and application classes into distinct directories. In Dockerfiles, each directory is copied in its own `COPY` instruction ordered by change frequency. Because third-party dependencies rarely change, Docker caches that heavy 50–80 MB layer indefinitely. When developers modify application code, only the tiny 200 KB application layer is invalidated, resulting in sub-second image builds and lightning-fast registry push/pull operations.
</details>

### Q2: What is `jlink` and how does it reduce container image sizes?
<details>
<summary>View Answer</summary>
`jlink` (Java Linker, introduced in Java 9 via the Java Platform Module System) is a tool that packages a set of modules and their transitive dependencies into a custom, stripped runtime image. Instead of bundling the full 300+ MB standard JDK/JRE, `jlink` analyzes the application\'s module requirements (using `jdeps`) and creates a minimal standalone runtime containing only the necessary JVM modules. This reduces runtime image sizes to 35–50 MB.
</details>

### Q3: What is Application Class Data Sharing (AppCDS) and how does it improve startup latency?
<details>
<summary>View Answer</summary>
AppCDS allows class metadata (parsed class structures, symbol tables, and bytecode verification state) to be pre-processed and dumped into an archive file (`.jsa`). At JVM startup, this archive is memory-mapped (`mmap`) directly into the JVM\'s address space in a single operation. This bypasses the expensive CPU-bound process of locating class files in JARs, parsing class formats, and verifying bytecode, reducing JVM boot times by 30% to 60%.
</details>

### Q4: Why must production Java containers specify `-XX:MaxRAMPercentage` instead of `-Xmx`?
<details>
<summary>View Answer</summary>
In container orchestrators like Kubernetes, container memory limits are configured dynamically via pod resource specifications (e.g., `resources.limits.memory: 2Gi`). Hardcoding `-Xmx1500m` into container launch scripts tightly couples the image to a specific sizing. Using `-XX:+UseContainerSupport -XX:MaxRAMPercentage=75.0` ensures the JVM dynamically calculates its heap limit as 75% of whatever cgroup limit the container is given at runtime, preventing OOM kills while adapting cleanly across varying deployment environments.
</details>

### Q5: What is the risk of running a Java application as `root` inside a container?
<details>
<summary>View Answer</summary>
Container isolation relies on Linux kernel namespaces and cgroups. If an attacker exploits a remote code execution vulnerability (e.g. Log4Shell or command injection) in an application running as root (`UID 0`), the attacker possesses root capabilities within the container. If a container breakout vulnerability exists in the Linux kernel or container runtime (such as runc), the attacker immediately obtains root access over the host operating system.
</details>

### Q6: What is a multi-stage Docker build and why is it mandatory for enterprise Java?
<details>
<summary>View Answer</summary>
A multi-stage build uses multiple `FROM` instructions in a single Dockerfile. The first stage contains the complete build toolchain (Maven/Gradle, full JDK, compilers, source code) to compile the application and run tests. The final stage starts with a clean, minimal JRE or Distroless base image and copies only the compiled JARs or extracted layers from the builder stage. This prevents build tools, source code, and local caching artifacts from bloating the production container image and expanding the security attack surface.
</details>

### Q7: Why is `jdk.crypto.ec` often explicitly required when building runtimes with `jlink`?
<details>
<summary>View Answer</summary>
Modern web servers and HTTPS connections require Elliptic Curve Cryptography for TLS 1.3 cipher suites. The ECC cryptographic provider is packaged in the `jdk.crypto.ec` module. Because cryptographic providers are loaded dynamically via service provider interfaces (SPI) at runtime, static dependency analyzers like `jdeps` may not detect a compile-time dependency on `jdk.crypto.ec`. If omitted from `jlink`, HTTPS calls fail at runtime with SSL handshake errors.
</details>

### Q8: What are Distroless container images?
<details>
<summary>View Answer</summary>
Distroless images (maintained by Google) contain only the application and its runtime dependencies (e.g. Java runtime and glibc). They do not contain package managers (apt, apk), shells (`/bin/sh`, `/bin/bash`), or standard Unix command-line utilities. This minimizes image size, dramatically reduces reported CVE vulnerabilities, and severely impedes attackers because typical post-exploitation tools (like downloading curl scripts or spawning interactive shells) are completely absent.
</details>

---

## 11. Practice Exercises

### Exercise 1: Analyze JAR Module Dependencies with `jdeps`
**Task:** Write a shell script that invokes `jdeps` on a target JAR file, outputs all required JDK modules, and prints a warning if `java.desktop` is unintentionally pulled in.
<details>
<summary>View Solution</summary>

```bash
#!/usr/bin/env bash
set -euo pipefail

JAR_PATH="${1:-app.jar}"

echo "Analyzing module dependencies for ${JAR_PATH}..."
MODULES=$(jdeps --print-module-deps --ignore-missing-deps "${JAR_PATH}")
echo "Required modules: ${MODULES}"

if echo "${MODULES}" | grep -q "java.desktop"; then
    echo "WARNING: java.desktop detected! Ensure GUI dependencies are excluded for server deployments."
else
    echo "SUCCESS: Clean headless server module profile."
fi
```
</details>

### Exercise 2: Calculate JVM CGroup Memory Allocation
**Task:** Write a small Java program that inspects and prints the JVM\'s detected container memory limits and heap sizing using `Runtime` and `ManagementFactory`.
<details>
<summary>View Solution</summary>

```java
import java.lang.management.ManagementFactory;
import java.lang.management.MemoryMXBean;

public class ContainerMemoryDiagnostic {
    public static void main(String[] args) {
        MemoryMXBean mem = ManagementFactory.getMemoryMXBean();
        long maxHeapBytes = mem.getHeapMemoryUsage().getMax();
        long totalMemory = Runtime.getRuntime().totalMemory();

        System.out.printf("Max Heap: %.2f MB%n", maxHeapBytes / (1024.0 * 1024.0));
        System.out.printf("Total Allocated: %.2f MB%n", totalMemory / (1024.0 * 1024.0));
        System.out.printf("Available Processors: %d%n", Runtime.getRuntime().availableProcessors());
    }
}
```
</details>

### Exercise 3: Generate Dynamic AppCDS Training Script
**Task:** Write a shell script that runs an application for 10 seconds to generate a Class Data Sharing archive and verifies the `.jsa` file was created successfully.
<details>
<summary>View Solution</summary>

```bash
#!/usr/bin/env bash
set -euo pipefail

ARCHIVE="app.jsa"
JAR="app.jar"

echo "Starting JVM in training mode..."
java -XX:ArchiveClassesAtExit="${ARCHIVE}" -Dspring.context.exit=onRefresh -jar "${JAR}" || true

if [ -f "${ARCHIVE}" ]; then
    echo "CDS archive created successfully: $(ls -lh ${ARCHIVE})"
else
    echo "Failed to generate CDS archive!"
    exit 1
fi
```
</details>

---

## 12. Summary / Cheat Sheet

- **Layered Containers**: Extract layers using `layertools` to maximize Docker cache efficiency.
- **Unprivileged Users**: Enforce `USER 10001` or use distroless images.
- **Dynamic Sizing**: Configure `-XX:+UseContainerSupport` and `-XX:MaxRAMPercentage=75.0`.
- **Minimal Runtimes**: Use `jdeps` and `jlink` to build small runtimes (35–50 MB).
- **Fast Startup**: Use AppCDS (`-XX:ArchiveClassesAtExit` / `-XX:SharedArchiveFile`).
- **Multi-Stage Builds**: Compile in stage 1; deploy minimal runtime in stage 2.

---

## 13. Further Reading
- OpenJDK JEP 282: [jlink: The Java Linker](https://openjdk.org/jeps/282)
- OpenJDK JEP 310: [Application Class-Data Sharing](https://openjdk.org/jeps/310)
- Spring Boot Documentation: [Layering Docker Images](https://docs.spring.io/spring-boot/docs/current/reference/html/container-images.html#container-images.dockerfiles)
- GoogleContainerTools: [Distroless Container Images](https://github.com/GoogleContainerTools/distroless)
