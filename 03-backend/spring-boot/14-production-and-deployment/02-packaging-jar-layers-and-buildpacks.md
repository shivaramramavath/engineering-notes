# Packaging: Layered JARs and Cloud Native Buildpacks

| Metadata | Value |
|---|---|
| **Spring Boot Version** | 3.4.x |
| **Spring Framework Version** | 6.2.x |
| **Java Version** | 21 LTS |
| **Module** | 14-production-and-deployment |
| **Status** | Complete |

---

## Table of Contents
- [1. What & Why](#1-what--why)
- [2. Core Concepts](#2-core-concepts)
- [3. How It Works Internally](#3-how-it-works-internally)
- [4. Syntax & Build Configuration Reference](#4-syntax--build-configuration-reference)
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
In traditional Spring Boot container packaging, the entire application is bundled into a single monolithic "fat JAR" (executable archive containing application classes, resources, and all third-party dependencies). When building Docker images:
- Every minor code change (e.g. changing 1 line in a controller) modifies the entire 80MB–150MB fat JAR.
- Docker invalidates its layer cache at the `COPY app.jar .` step, forcing the container builder to re-upload, re-push, and re-pull the entire 150MB layer to container registries on every single CI build.
- Deployment pipelines become slow, wasting gigabytes of registry storage and network bandwidth.

### Historical Evolution
Prior to Spring Boot 2.3, developers attempted to speed up builds by manually writing complex unzip commands in multi-stage Dockerfiles. However, manual extraction broke Spring Boot's specialized classloader hierarchy and nested JAR isolation.

Spring Boot 2.3 introduced **Layered JARs** and integrated support for **Cloud Native Buildpacks (CNB)**. 

In **Spring Boot 3.3+ and 3.4**, this architecture is modernized with:
- Native layer extraction via `jarmode=tools`.
- Automated Open Container Initiative (OCI) image creation via the **Paketo Buildpacks** ecosystem.
- Instant operating system vulnerability patching via **OCI image rebasing** without re-compiling Java code.

### Production Relevance
Layered JAR packaging and Cloud Native Buildpacks enable:
- **Instant Docker Layer Caching**: CI builds push only the modified application layer ($\approx 1\text{MB}$–$5\text{MB}$) instead of the entire 100MB+ fat JAR.
- **Dockerfile-Free Containerization**: Standardized, secure OCI-compliant container builds directly from Maven/Gradle without requiring Dockerfiles or local Docker daemon installs.
- **Zero-CVE Security Maintenance**: Update the underlying base OS image (Ubuntu/Alpine) across 50 microservices in seconds using CNB `rebase`.

---

## 2. Core Concepts

### Layered JAR Foundations
1. **The Four Standard Layers**:
   - `dependencies`: Third-party release dependencies from Maven Central (rarely change; cached indefinitely).
   - `spring-boot-loader`: The custom Spring Boot bootstrap classloader code (changes only when upgrading Spring Boot).
   - `snapshot-dependencies`: Internal snapshot builds (change occasionally).
   - `application`: Your application class files, resources, and configuration (changes with every Git commit).
2. **`layers.idx`**: The indexing file generated inside `BOOT-INF/layers.idx` that dictates the exact order and content of each layer.
3. **`jarmode=tools` (Boot 3.3+) / `jarmode=layertools`**: Built-in CLI mode in the Spring Boot executable JAR to inspect and extract layers for Dockerfile builds.
4. **Cloud Native Buildpacks (CNB)**: An open-source CNCF specification that transforms application source code into secure, production-ready OCI container images without Dockerfiles.
5. **Paketo Buildpacks**: The standard, production-ready buildpack implementation maintained by the community and used by Spring Boot's `spring-boot:build-image` goal.

---

## 3. How It Works Internally

### Docker Layer Caching with Layered JAR Extraction

```
TRADITIONAL FAT JAR BUILD (Poor Caching):
+------------------------------------------------------------------------------------+
| Base OS Layer (Ubuntu / Alpine JRE)                                     [Cached]   |
+------------------------------------------------------------------------------------+
| COPY target/app.jar app.jar (120 MB - Invalidation on EVERY commit!)    [PUSH 120M]|
+------------------------------------------------------------------------------------+

SPRING BOOT LAYERED JAR BUILD (Optimal Caching):
+------------------------------------------------------------------------------------+
| Base OS Layer (Ubuntu / Alpine JRE)                                     [Cached]   |
+------------------------------------------------------------------------------------+
| Layer 1: dependencies (Spring, Hibernate, Jackson - 95 MB)             [Cached]   |
+------------------------------------------------------------------------------------+
| Layer 2: spring-boot-loader (Spring Boot Launcher code - 500 KB)        [Cached]   |
+------------------------------------------------------------------------------------+
| Layer 3: snapshot-dependencies (Internal company snapshots - 15 MB)    [Cached]   |
+------------------------------------------------------------------------------------+
| Layer 4: application (Your controller & service classes - 1.2 MB)       [PUSH 1.2M]|
+------------------------------------------------------------------------------------+
  Result: CI pipeline uploads only 1.2 MB instead of 120 MB! 99% bandwidth savings!
```

---

## 4. Syntax & Build Configuration Reference

### Maven Build Configuration (`pom.xml`)

```xml
<project>
    <build>
        <plugins>
            <plugin>
                <groupId>org.springframework.boot</groupId>
                <artifactId>spring-boot-maven-plugin</artifactId>
                <configuration>
                    <!-- Layered JAR is enabled by default in Spring Boot 2.3+ -->
                    <layers>
                        <enabled>true</enabled>
                    </layers>
                    <!-- Cloud Native Buildpacks Configuration -->
                    <image>
                        <builder>paketobuildpacks/builder-jammy-base:latest</builder>
                        <name>docker.internal/myorg/${project.artifactId}:${project.version}</name>
                        <env>
                            <BP_JVM_VERSION>21</BP_JVM_VERSION>
                            <BP_JVM_CDS_ENABLED>true</BP_JVM_CDS_ENABLED>
                        </env>
                    </image>
                </configuration>
            </plugin>
        </plugins>
    </build>
</project>
```

### Inspecting Layers via CLI
```bash
# In Spring Boot 3.3+:
java -Djarmode=tools -jar target/service-1.0.jar list-layers

# Output:
# dependencies
# spring-boot-loader
# snapshot-dependencies
# application
```

---

## 5. Practical Examples

### Example 1: Production Multi-Stage Dockerfile Using Layer Extraction
```dockerfile
# Stage 1: Extraction Stage
FROM eclipse-temurin:21-jre-jammy AS extractor
WORKDIR /build
COPY target/billing-service-*.jar app.jar

# Extract layers into separate directory structures
# (Boot 3.3+ uses jarmode=tools; Boot 2.3-3.2 uses jarmode=layertools)
RUN java -Djarmode=tools -jar app.jar extract --destination extracted/

# Stage 2: Final Minimal Runtime Image
FROM eclipse-temurin:21-jre-jammy AS runtime

# Secure non-root system user
RUN groupadd -r appgroup && useradd -r -g appgroup -u 1001 appuser
USER 1001:1001

WORKDIR /app

# Copy layers from least frequently changed to most frequently changed!
COPY --from=extractor --chown=appuser:appgroup /build/extracted/dependencies/ ./
COPY --from=extractor --chown=appuser:appgroup /build/extracted/spring-boot-loader/ ./
COPY --from=extractor --chown=appuser:appgroup /build/extracted/snapshot-dependencies/ ./
COPY --from=extractor --chown=appuser:appgroup /build/extracted/application/ ./

EXPOSE 8080 9090
ENV JAVA_TOOL_OPTIONS="-XX:MaxRAMPercentage=75.0 -XX:+ExitOnOutOfMemoryError"

# Use Spring Boot JarLauncher to boot directly from extracted layers
ENTRYPOINT ["java", "org.springframework.boot.loader.launch.JarLauncher"]
```

### Example 2: Building OCI Images Directly with Buildpacks (Zero Dockerfile)
No Dockerfile is needed! Simply execute:

```bash
# Build production OCI container image directly via Maven
./mvnw spring-boot:build-image -Dspring-boot.build-image.imageName=registry.internal/orders:1.0.0
```

**Buildpack Output**:
```text
[INFO] --- spring-boot:3.4.1:build-image (default-cli) @ orders ---
[INFO] Building image 'registry.internal/orders:1.0.0'
[INFO]  > Pulling builder image 'docker.io/paketobuildpacks/builder-jammy-base:latest'
[INFO]  > Using cache volume 'pack-cache-orders.build'
[INFO]  > Executing buildpack 'paketo-buildpacks/bellsoft-liberica'
[INFO]    - Contributing BellSoft Liberica JRE 21.0.2
[INFO]  > Executing buildpack 'paketo-buildpacks/spring-boot'
[INFO]    - Creating slices from layers index
[INFO]      - dependencies (88.4 MB)
[INFO]      - spring-boot-loader (450 KB)
[INFO]      - application (1.8 MB)
[INFO] Successfully built image 'registry.internal/orders:1.0.0'
```

### Example 3: Customizing Layer Definitions with `layers.xml`
If you maintain internal corporate shared libraries (e.g. `com.mycompany.common`), you can isolate them into a dedicated caching layer:

```xml
<!-- src/main/resources/META-INF/spring/layers.xml -->
<layers xmlns="http://www.springframework.org/schema/boot/layers"
        xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance"
        xsi:schemaLocation="http://www.springframework.org/schema/boot/layers
        https://www.springframework.org/schema/boot/layers/layers-3.4.xsd">
    <application>
        <into layer="application"/>
    </application>
    <dependencies>
        <into layer="company-dependencies">
            <include>com.mycompany:*</include>
        </into>
        <into layer="dependencies"/>
    </dependencies>
    <layer-order>
        <layer>dependencies</layer>
        <layer>company-dependencies</layer>
        <layer>spring-boot-loader</layer>
        <layer>snapshot-dependencies</layer>
        <layer>application</layer>
    </layer-order>
</layers>
```

### Example 4: Anti-Pattern vs. Production Fix (Raw `COPY app.jar` Dockerfile)
```dockerfile
# ANTI-PATTERN: Copying raw fat JAR directly into Docker image
FROM eclipse-temurin:21-jre
WORKDIR /app
COPY target/service.jar app.jar
# CATASTROPHIC IN CI/CD:
# Modifying 1 line of Java code invalidates this layer!
# CI pipeline must push and re-download 120MB on every single commit!
ENTRYPOINT ["java", "-jar", "app.jar"]

# PRODUCTION FIX: Extract layers and copy sequentially (Example 1)
# Only the 2MB 'application' layer is invalidated; 95MB of dependencies stay cached!
```

### Example 5: Instant Base Image Vulnerability Patching via CNB `rebase`
When a critical Linux kernel or glibc CVE is discovered in your base image, traditional Docker requires rebuilding all 50 microservices from scratch. With Cloud Native Buildpacks, you rebase in seconds without touching Java:

```bash
# Rebase image against updated patched base run image in 5 seconds!
pack rebase registry.internal/orders:1.0.0 --run-image paketobuildpacks/run-jammy-base:latest
```
*Result*: The OS layer is swapped under the application layers with zero recompilation!

---

## 6. Best Practices & Production Guidelines

1. **Always Use Layered JARs in Dockerfiles**: Never use `COPY target/*.jar app.jar`. Always extract layers and copy `dependencies`, `spring-boot-loader`, `snapshot-dependencies`, and `application` in strict caching order.
2. **Launch with `JarLauncher` for Extracted Layers**: When launching from extracted layers, invoke `java org.springframework.boot.loader.launch.JarLauncher` (Boot 3.2+) rather than raw classpath commands. This ensures Spring Boot's internal index mappings remain identical to fat JAR behavior.
3. **Prefer Cloud Native Buildpacks in Standard CI Pipelines**: Using `./mvnw spring-boot:build-image` enforces industry-standard security practices (non-root execution, reproducible builds, SBOM generation) without maintaining custom Dockerfiles.
4. **Generate Software Bill of Materials (SBOM)**: Paketo Buildpacks automatically generate Syft and CycloneDX SBOMs inside the container image, providing instant compliance auditing for enterprise security scans.
5. **Separate Company Artifacts with Custom Layers**: If your organization shares internal common libraries, use `layers.xml` to separate `company-dependencies` from open-source dependencies.

---

## 7. Common Mistakes & Pitfalls

| Mistake | Why It Fails | Practical Remedy |
|---|---|---|
| Copying `application` layer before `dependencies` | Modifying application code invalidates all subsequent layers, destroying cache benefits. | Copy `dependencies` first, and `application` last. |
| Using deprecated `jarmode=layertools` in Boot 3.3+ | `layertools` is deprecated in favor of `tools` in Spring Boot 3.3+. | Use `java -Djarmode=tools -jar app.jar extract`. |
| Running as root user in custom Dockerfile | Fails security compliance audits (CIS benchmarks, Kubernetes security policies). | Add `USER 1001:1001` to Dockerfile runtime stage. |
| Hardcoding launcher class name across major versions | Spring Boot 3.2 relocated `JarLauncher` from `org.springframework.boot.loader.JarLauncher` to `org.springframework.boot.loader.launch.JarLauncher`. | Verify exact package name based on Spring Boot version. |
| Bloating container with Maven/JDK in final image | Packaging full JDK and Maven source code in production image inflates image size to $> 800\text{MB}$. | Use multi-stage Dockerfiles or Buildpacks to produce minimal JRE runtime images ($< 250\text{MB}$). |
| Forgetting `.dockerignore` | Multi-stage build copies `.git`, target files, and local logs into Docker build context, slowing down builds. | Include `.dockerignore` excluding `.git`, `target`, and `*.log`. |

---

## 8. Performance & Security Notes

### Registry Push & Pull Acceleration
- In Kubernetes clusters, when an autoscaling event triggers pod creation across 5 new worker nodes:
  - With fat JAR images: Each node downloads 120MB over the network ($\approx 600\text{MB}$ total network transfer).
  - With Layered images: Worker nodes already have the base and dependency layers cached locally; they download only the modified 2MB application layer, slashing pod startup time by 80%.

### Software Supply Chain Security (SBOM)
- Spring Boot 3 + Paketo Buildpacks natively embed a Software Bill of Materials (SBOM) into the container image in CycloneDX and SPDX formats.
- Security vulnerability scanners (Trivy, Grype) can query the embedded SBOM directly without needing to decompile JARs.

---

## 9. Comparison / When to Use What

| Packaging Method | Build Complexity | Layer Caching | CVE Patching (Rebase) | Non-Root by Default |
|---|---|---|---|---|
| **Fat JAR `COPY app.jar`** | Minimal (1 line) | **None (Fails on every commit)** | No (Full rebuild required) | No (Manual config) |
| **Multi-Stage Layered Dockerfile** | Medium (Requires Dockerfile maintenance) | **Optimal (Granular layers)** | No (Full rebuild required) | Yes (Explicit `USER`) |
| **Cloud Native Buildpacks (Paketo)**| **Zero Dockerfile (1 command)** | **Optimal (Native CNB slices)**| **Yes (Instant `pack rebase`)** | **Yes (Out of the box)** |
| **Google Jib** | Zero Dockerfile (Daemonless) | Optimal (Granular layers) | No | Yes |

---

## 10. Interview Questions & Answers

### Q1: What is a Layered JAR in Spring Boot and why was it introduced?
<details>
<summary>Click to view answer</summary>

A Layered JAR splits a Spring Boot executable archive into distinct logical layers based on change frequency:
1. `dependencies` (third-party release JARs - rarely change)
2. `spring-boot-loader` (Spring Boot launcher code - changes on framework updates)
3. `snapshot-dependencies` (internal team snapshots)
4. `application` (business classes and resources - changes on every commit)
It was introduced to optimize Docker layer caching. By extracting and copying these layers sequentially into a Dockerfile, Docker caches the heavy dependencies layer, requiring CI pipelines to upload only the small application layer ($1\text{MB}$–$5\text{MB}$) on code changes.
</details>

### Q2: How do you extract layers from a Spring Boot JAR in modern versions?
<details>
<summary>Click to view answer</summary>

In Spring Boot 3.3+, use the `tools` mode:
```bash
java -Djarmode=tools -jar app.jar extract --destination extracted/
```
In Spring Boot 2.3 through 3.2, the command was:
```bash
java -Djarmode=layertools -jar app.jar extract
```
This extracts the archive into subdirectories matching the layer names defined in `BOOT-INF/layers.idx`.
</details>

### Q3: What are Cloud Native Buildpacks (CNB) and how does Spring Boot integrate with them?
<details>
<summary>Click to view answer</summary>

Cloud Native Buildpacks (a CNCF project) transform application source code or build artifacts directly into standardized OCI container images without requiring a Dockerfile. Spring Boot provides native integration via the Maven plugin goal `mvn spring-boot:build-image` (and Gradle `bootBuildImage`). It automatically downloads the Paketo buildpack, selects an optimized JRE base, applies layered slices, configures memory ergonomics, and outputs a hardened, production-ready container image.
</details>

### Q4: What is "Image Rebasing" in Cloud Native Buildpacks?
<details>
<summary>Click to view answer</summary>

Rebasing is a unique feature of Cloud Native Buildpacks that allows updating the underlying operating system layer (e.g. patching an OS-level OpenSSL vulnerability) **without rebuilding or recompiling the application**. By executing `pack rebase <image>`, the buildpack runtime swaps the base OS image layer beneath the application layers directly in the container registry metadata, updating the image in seconds.
</details>

### Q5: What class is used as the ENTRYPOINT when running an extracted layered Spring Boot application?
<details>
<summary>Click to view answer</summary>

`org.springframework.boot.loader.launch.JarLauncher` (in Spring Boot 3.2+) or `org.springframework.boot.loader.JarLauncher` (in Boot 2.x and 3.0-3.1). Invoking `JarLauncher` instructs Spring Boot's custom classloader to reconstruct the nested classpath from the extracted directories.
</details>

### Q6: How do you customize layer mappings if your project has proprietary internal libraries?
<details>
<summary>Click to view answer</summary>

Create a `src/main/resources/META-INF/spring/layers.xml` file. Within this file, define custom layer names (e.g. `company-dependencies`) and specify matching rules using Maven coordinates (e.g. `<include>com.mycompany:*</include>`). Spring Boot compiles this definition into `BOOT-INF/layers.idx`.
</details>

### Q7: Why should containerized Java applications NOT be run as the root user?
<details>
<summary>Click to view answer</summary>

Running as `root` (`UID 0`) is a major security risk. If an attacker exploits a remote code execution vulnerability (e.g. Java deserialization or file upload overwrite), they gain root privileges inside the container. If container isolation boundaries are breached, the attacker can compromise the host node kernel. Production images should always enforce non-root execution via `USER 1001:1001`.
</details>

### Q8: What is the benefit of embedding an SBOM (Software Bill of Materials) into container images?
<details>
<summary>Click to view answer</summary>

An SBOM provides a cryptographically verifiable inventory of all dependencies, library versions, and licenses bundled inside the container. Security scanners and enterprise compliance tools can inspect the embedded SBOM (e.g. CycloneDX or SPDX) to instantly detect vulnerable components (CVEs) without needing to decompile or reverse-engineer runtime binaries.
</details>

---

## 11. Practice Exercises

### Exercise 1: Inspect Layer Order Using CLI (Easy)
<details>
<summary>Click to view challenge & solution</summary>

**Challenge**: Write the bash command to query a Spring Boot 3.4 JAR and display the layer order specified in its internal index.

**Solution**:
```bash
java -Djarmode=tools -jar target/service-1.0.jar list-layers
```
</details>

### Exercise 2: Configure Maven Buildpack with Custom Image Name (Medium)
<details>
<summary>Click to view challenge & solution</summary>

**Challenge**: Configure `pom.xml` so that running `mvn spring-boot:build-image` builds an image named `harbor.internal/apps/order-service:2.1.0` using Java 21 on the Paketo Jammy base builder.

**Solution**:
```xml
<plugin>
    <groupId>org.springframework.boot</groupId>
    <artifactId>spring-boot-maven-plugin</artifactId>
    <configuration>
        <image>
            <builder>paketobuildpacks/builder-jammy-base:latest</builder>
            <name>harbor.internal/apps/order-service:2.1.0</name>
            <env>
                <BP_JVM_VERSION>21</BP_JVM_VERSION>
            </env>
        </image>
    </configuration>
</plugin>
```
</details>

### Exercise 3: Complete Docker Multi-Stage Build with Non-Root Security (Hard)
<details>
<summary>Click to view challenge & solution</summary>

**Challenge**: Write a complete production multi-stage Dockerfile that builds with Eclipse Temurin 21 JRE, extracts layers with `jarmode=tools`, copies layers in optimal caching order, runs as unprivileged user `appuser` (UID 10001), and sets a 75% memory limit.

**Solution**:
```dockerfile
# Stage 1: Layer Extractor
FROM eclipse-temurin:21-jre-jammy AS extractor
WORKDIR /build
COPY target/service-*.jar app.jar
RUN java -Djarmode=tools -jar app.jar extract --destination extracted/

# Stage 2: Production Distroless/Hardened Runtime
FROM eclipse-temurin:21-jre-jammy AS runtime

# Create dedicated non-root system user
RUN groupadd -g 10001 appgroup && \
    useradd -u 10001 -g appgroup -s /bin/false -M appuser

WORKDIR /app

# Copy layers in strict order of volatility (lowest to highest)
COPY --from=extractor --chown=appuser:appgroup /build/extracted/dependencies/ ./
COPY --from=extractor --chown=appuser:appgroup /build/extracted/spring-boot-loader/ ./
COPY --from=extractor --chown=appuser:appgroup /build/extracted/snapshot-dependencies/ ./
COPY --from=extractor --chown=appuser:appgroup /build/extracted/application/ ./

# Enforce non-root execution
USER 10001:10001

EXPOSE 8080 9090

ENV JAVA_TOOL_OPTIONS="-XX:MaxRAMPercentage=75.0 -XX:+ExitOnOutOfMemoryError"

ENTRYPOINT ["java", "org.springframework.boot.loader.launch.JarLauncher"]
```
</details>

---

## 12. Summary / Cheat Sheet

```
+---------------------------------------------+-------------------------------------------------------------+
| Concept / Command                           | Description & Purpose                                       |
+---------------------------------------------+-------------------------------------------------------------+
| java -Djarmode=tools -jar app.jar extract   | Extracts layered JAR into separate directories (Boot 3.3+) |
| Layer Caching Order                         | dependencies -> loader -> snapshot-deps -> application     |
| JarLauncher                                 | Launches extracted layered structure via Spring classloader |
| mvn spring-boot:build-image                 | Builds OCI container image via Cloud Native Buildpacks      |
| pack rebase <image>                         | Instantly patches underlying OS vulnerabilities in seconds  |
| USER 1001:1001                              | Mandatory security best practice for container execution   |
| layers.xml                                  | Custom layer mapping rules for internal corporate libraries |
+---------------------------------------------+-------------------------------------------------------------+
```

---

## 13. Further Reading
- Spring Boot Official Documentation: Layering a Docker Image
- Cloud Native Buildpacks Official Specification
- Paketo Buildpacks for Java Documentation & Configuration
- CNCF: Securing the Software Supply Chain with SBOMs