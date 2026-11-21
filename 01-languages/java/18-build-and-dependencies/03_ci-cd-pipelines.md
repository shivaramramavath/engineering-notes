# Modern Java CI/CD Pipelines: Automation, Containers, and Deployment

> End-to-end continuous integration and deployment for enterprise Java: GitHub Actions, JDK Provisioning, Build Caching, Cloud Native Buildpacks/Jib, and Release Publishing.

| Metadata | Details |
|---|---|
| **Level** | Core / Advanced |
| **Prerequisites** | Maven / Gradle Basics, Docker Basics, Git |
| **Time to Read** | 25 minutes |
| **Status** | Complete |
| **JDK Baseline** | Java 17 / 21 / 25 LTS |

---

## Table of Contents
1. [1. What & Why](#1-what--why)
2. [2. Core Concepts](#2-core-concepts)
3. [3. How It Works Internally: CI Runner Lifecycle & Container Builds](#3-how-it-works-internally-ci-runner-lifecycle--container-builds)
4. [4. Syntax & Usage](#4-syntax--usage)
5. [5. Practical Examples](#5-practical-examples)
6. [6. Best Practices](#6-best-practices)
7. [7. Common Mistakes & Pitfalls](#7-common-mistakes--pitfalls)
8. [8. Performance & Security Notes](#8-performance--security-notes)
9. [9. Comparison / When to Use What](#9-comparison--when-to-use-what)
10. [10. Interview Questions](#10-interview-questions)
11. [11. Practice Exercises](#11-practice-exercises)
12. [12. Summary / Cheat Sheet](#12-summary--cheat-sheet)
13. [13. Further Reading](#13-further-reading)

---

## 1. What & Why
In modern cloud-native engineering, code does not live on developer workstations; it lives in a continuous integration and continuous deployment (CI/CD) delivery pipeline. Every pull request must be automatically compiled, statically analyzed, unit-tested, integration-tested against live database containers, packaged into secure minimal container images, and deployed to staging environments without human intervention.

Building high-performance CI/CD pipelines for Java requires addressing Java-specific pipeline challenges: cold JVM startup latency, multi-gigabyte dependency cache restoration (`~/.m2` or `~/.gradle`), container layer caching, reproducible builds, and secure credential handling. Mastering pipeline automation transforms slow, fragile 30-minute releases into lightning-fast, reproducible 3-minute deployment workflows.

---

## 2. Core Concepts
- **Continuous Integration (CI)**: Automatically compiling, testing, and verifying code changes on every commit or pull request to detect regressions immediately.
- **Continuous Delivery (CD)**: Automatically packaging verified artifacts and deploying them safely into staging and production environments.
- **JDK Provisioning (`actions/setup-java`)**: Declaratively downloading and configuring verified LTS JDK distributions (Temurin, Corretto, Oracle, Zulu) in CI environments.
- **Dependency Cache Restoration**: Persisting Maven and Gradle dependency repositories across pipeline runs using cache keys based on POM/Gradle lockfiles.
- **Test Reporting & Coverage Gates**: Automated generation and publishing of JUnit XML test reports and JaCoCo code coverage thresholds (e.g., requiring 80%+ branch coverage to merge).
- **OCI Container Packaging (Jib vs Dockerfile)**:
  - *Google Jib*: Builds optimized, layered Docker/OCI images directly from Maven/Gradle without requiring a local Docker daemon.
  - *Multi-Stage Dockerfiles*: Standard containerization using minimal Alpine or Distroless runtime base images.
- **Semantic Versioning & Release Automation**: Automating git tagging, changelog generation, and publishing releases to artifact registries.

---

## 3. How It Works Internally: CI Runner Lifecycle & Container Builds

A high-performance Java pipeline processes code through distinct, cached pipeline stages:

```
[ Git Push / Pull Request ]
             |
             v
+-------------------------------------------------------+
|             1. Runner Provisioning                    |
|  - Spins up clean Ubuntu VM / container               |
|  - Clones Git repository (fetch-depth: 1)             |
|  - actions/setup-java installs JDK 21 Temurin         |
+-------------------------------------------------------+
                           |
                           v
+-------------------------------------------------------+
|             2. Cache Restoration                      |
|  - Restores ~/.m2/repository or ~/.gradle/caches      |
|  - Cache hit skips downloading 500 MB of dependencies |
+-------------------------------------------------------+
                           |
                           v
+-------------------------------------------------------+
|             3. Verification & Testing                 |
|  - Unit tests executed via Surefire / Gradle          |
|  - Integration tests against Testcontainers           |
|  - JaCoCo generates code coverage report              |
+-------------------------------------------------------+
                           |
                           v
+-------------------------------------------------------+
|             4. Container Build & Publish              |
|  - Google Jib separates application into layers:      |
|    Layer 1: Dependencies (rarely changes - cached)    |
|    Layer 2: Application Bytecode (changes frequently) |
|  - Pushes OCI image to Container Registry             |
+-------------------------------------------------------+
```

### Google Jib vs Standard Dockerfile:
In a standard `Dockerfile`, running `docker build` copies the entire fat JAR into a single container layer. When a developer changes one line of code, Docker invalidates the layer and pushes an 80 MB image over the network.

Google Jib splits Java applications into distinct semantic layers:
1. `Dependencies` (90% of size, rarely changes) $\rightarrow$ Cached indefinitely on registry.
2. `Resources` (changes occasionally).
3. `Classes` (small, changes every commit) $\rightarrow$ Only 200 KB pushed to registry!

---

## 4. Syntax & Usage

### 4.1 Production GitHub Actions Workflow (`.github/workflows/ci.yml`)
```yaml
name: Java CI/CD Pipeline

on:
  push:
    branches: [ main ]
  pull_request:
    branches: [ main ]

jobs:
  build-and-test:
    runs-on: ubuntu-latest
    timeout-minutes: 15

    steps:
      - name: Checkout Code
        uses: actions/checkout@v4

      - name: Set up JDK 21 (Eclipse Temurin)
        uses: actions/setup-java@v4
        with:
          distribution: 'temurin'
          java-version: '21'
          cache: 'maven' # Automatically caches ~/.m2/repository based on pom.xml hash!

      - name: Compile and Run Unit Tests
        run: ./mvnw -B test

      - name: Run Integration Tests (Failsafe & Testcontainers)
        run: ./mvnw -B verify -DskipUnitTests

      - name: Verify Code Coverage (JaCoCo)
        run: ./mvnw -B jacoco:check

      - name: Upload Test Report Artifacts
        if: always()
        uses: actions/upload-artifact@v4
        with:
          name: surefire-reports
          path: '**/target/surefire-reports/*.xml'
```

### 4.2 Building Layered Container Images with Google Jib
```xml
<!-- In pom.xml: Zero Docker daemon required! -->
<plugin>
    <groupId>com.google.cloud.tools</groupId>
    <artifactId>jib-maven-plugin</artifactId>
    <version>3.4.3</version>
    <configuration>
        <from>
            <!-- Minimal Google Distroless Java 21 Base Image -->
            <image>gcr.io/distroless/java21-debian12:nonroot</image>
        </from>
        <to>
            <image>registry.example.com/services/order-service:${project.version}</image>
        </to>
        <container>
            <mainClass>com.example.service.ApplicationMain</mainClass>
            <jvmFlags>
                <jvmFlag>-XX:+UseZGC</jvmFlag>
                <jvmFlag>-XX:MaxRAMPercentage=75.0</jvmFlag>
            </jvmFlags>
            <ports>
                <port>8080</port>
            </ports>
            <user>nonroot:nonroot</user>
        </container>
    </configuration>
</plugin>
```

---

## 5. Practical Examples

### Example 1: Multi-Stage Production Dockerfile for Java 21
```dockerfile
# Stage 1: Build & Package
FROM eclipse-temurin:21-jdk-alpine AS builder
WORKDIR /workspace

# Copy wrapper and configuration first to leverage Docker layer caching
COPY mvnw .
COPY .mvn .mvn
COPY pom.xml .
RUN ./mvnw dependency:go-offline -B

# Copy source code and package application
COPY src src
RUN ./mvnw clean package -DskipTests

# Stage 2: Minimal Distroless Runtime
FROM gcr.io/distroless/java21-debian12:nonroot
WORKDIR /app

# Run as non-root user for security
USER nonroot:nonroot

# Copy only the compiled JAR from builder stage
COPY --from=builder --chown=nonroot:nonroot /workspace/target/*.jar app.jar

ENV JAVA_OPTS="-XX:MaxRAMPercentage=75.0 -XX:+UseZGC -XX:+ExitOnOutOfMemoryError"
EXPOSE 8080

ENTRYPOINT ["sh", "-c", "exec java $JAVA_OPTS -jar app.jar"]
```

### Example 2: Automated Semantic Versioning and Release Tagging
```yaml
# GitHub Actions release step
- name: Create Release Tag
  if: github.ref == 'refs/heads/main'
  uses: anothrNick/github-tag-action@1.67.0
  env:
    GITHUB_TOKEN: ${{ secrets.GITHUB_TOKEN }}
    WITH_V: true
    DEFAULT_BUMP: patch
```

### Example 3: Edge Case - Reproducible Builds Configuration
```xml
<properties>
    <!-- Fixes build timestamps to git commit timestamp for bit-for-bit reproducible JARs -->
    <project.build.outputTimestamp>2026-11-20T12:00:00Z</project.build.outputTimestamp>
</properties>
```

---

## 6. Best Practices
1. **Always Use the Maven/Gradle Wrapper in CI**: Invoke `./mvnw` or `./gradlew` rather than relying on system-installed build binaries.
2. **Use Distroless Runtime Base Images**: Deploy containers using `gcr.io/distroless/java21:nonroot`. Distroless images contain no package managers (`apt`, `apk`), no shells (`/bin/sh`, `/bin/bash`), and run as non-root, neutralizing 99% of CVE vulnerabilities.
3. **Configure Memory Percentage Flags**: Always set `-XX:MaxRAMPercentage=75.0` in container runtime flags to allow the JVM to adjust heap dynamically to container cgroup memory limits.
4. **Leverage Dependency Caching**: Use built-in cache mechanisms (`cache: 'maven'` or `cache: 'gradle'`) in `actions/setup-java`.
5. **Separate Fast Unit Tests from Slow Integration Tests**: Run in-memory unit tests first. If unit tests fail, abort the pipeline immediately before spinning up expensive Docker Testcontainers.
6. **Set `-XX:+ExitOnOutOfMemoryError`**: Ensure the JVM immediately terminates the container upon OutOfMemoryError, allowing Kubernetes to restart the pod cleanly.

---

## 7. Common Mistakes & Pitfalls

| Mistake | Why It Fails | Correction |
|---|---|---|
| Running containers as root | Security violation; container breakouts grant root host access | Use `USER nonroot:nonroot` or Distroless non-root |
| Hardcoding `-Xmx2g` in container | JVM ignores Kubernetes cgroup limits; pod killed by OOMKilled | Use `-XX:MaxRAMPercentage=75.0` |
| Missing dependency cache in CI | Downloads 500 MB of dependencies on every build, wasting 5+ minutes | Configure `actions/setup-java` caching |
| Single-layer Docker builds | Changes in 1 class force rebuilding and pushing 100 MB layers | Use Google Jib or layered multi-stage Dockerfiles |
| Missing `./mvnw` executable permission | `Permission denied` error on Linux CI runners | Run `git update-index --chmod=+x mvnw` |
| Skipping integration tests in PRs | Breaking database or contract changes merge undetected | Run Testcontainers in PR pipeline |
| Leaving sensitive secrets in Docker images | Anyone with image access can extract credentials from history | Pass secrets via environment variables or secret managers |
| Missing `timeout-minutes` on CI jobs | Stalled tests or hanging deadlocks consume CI runner minutes | Set `timeout-minutes: 15` on all jobs |

---

## 8. Performance & Security Notes
- **Distroless Container Security**: Distroless images contain strictly the Java runtime and application bytecode. Vulnerability scanners (Trivy, Grype) typically report **zero OS-level CVEs** for distroless images, compared to 50–150 CVEs found in standard Ubuntu/Alpine base images.
- **Layer Caching Economics**: Because Google Jib stores application dependencies in an independent layer, CI/CD runners only build and push the thin `classes` layer (~100 KB), completing container image deployment in under 5 seconds.
- **Reproducible Builds**: Reproducible builds guarantee that compiling the exact same source code produces byte-for-byte identical JAR binaries. This allows independent verification that binaries have not been tampered with by compromised CI pipelines.

---

## 9. Comparison / When to Use What

| Packaging Tool | Docker Daemon Needed | Layering Optimization | Speed | Best Use Case |
|---|---|---|---|---|
| **Google Jib** | No (Native Java) | Automatic (Fastest) | Ultra Fast | Standard Maven/Gradle microservices |
| **Cloud Native Buildpacks** | Yes | High (Pack CLI) | Fast | Cloud Foundry, Heroku, Spring Boot `bootBuildImage` |
| **Multi-Stage Dockerfile** | Yes | Manual Layering | Moderate | Custom OS packages, native C binaries, complex setups |

---

## 10. Interview Questions

<details>
<summary>1. How does Google Jib optimize container image creation compared to a standard Dockerfile?</summary>
In a standard Dockerfile, building a container copies the entire fat JAR into a single image layer. Any code change requires invalidating the layer and re-uploading the entire 100 MB JAR. Google Jib builds OCI container images directly from Java AST metadata without requiring a Docker daemon. It automatically separates the application into distinct layers: dependencies, resources, and compiled classes. When code changes, Jib only builds and uploads the tiny (sub-megabyte) classes layer, reducing build and push times from minutes to seconds.
</details>

<details>
<summary>2. Why is hardcoding -Xmx flags dangerous inside containerized Java applications?</summary>
In containerized environments (Docker/Kubernetes), memory constraints are enforced by Linux control groups (cgroups). If a pod limit is set to 2 GB and Java is configured with <code>-Xmx2g</code>, the total process memory (Heap + Metaspace + Thread Stacks + JVM Native Memory + JIT Cache) will exceed 2 GB. The Linux kernel OOM-killer immediately terminates the container process with exit code 137 (OOMKilled). Setting <code>-XX:MaxRAMPercentage=75.0</code> ensures the heap dynamically scales to 75% of container memory, reserving 25% for native JVM structures.
</details>

<details>
<summary>3. What is a Distroless container image and why is it recommended for enterprise Java deployments?</summary>
Distroless images (provided by Google) contain only the application and its runtime dependencies (the Java runtime and glibc). They do not contain package managers (apt, apk), shells (bash, sh), or standard Linux utilities (curl, wget). This drastically reduces the image size (down to ~150 MB), eliminates OS-level security vulnerabilities, and prevents attackers from running interactive shells or downloading secondary exploit payloads if a container is compromised.
</details>

<details>
<summary>4. What is the role of -XX:+ExitOnOutOfMemoryError in Kubernetes?</summary>
When a JVM experiences an <code>OutOfMemoryError</code>, some threads may terminate while others remain running, leaving the service in a zombie/corrupted state where it fails requests but does not crash. Enabling <code>-XX:+ExitOnOutOfMemoryError</code> forces the JVM to terminate immediately with a non-zero exit code upon encountering an OOM, allowing Kubernetes liveness probes to fail and restart the pod cleanly.
</details>

<details>
<summary>5. How does actions/setup-java cache dependencies effectively in GitHub Actions?</summary>
<code>actions/setup-java</code> computes a cryptographic hash of the project's build descriptors (<code>pom.xml</code> or <code>*.gradle*</code>). It restores the cached <code>~/.m2/repository</code> or <code>~/.gradle/caches</code> folder at the start of the job. If the hash matches, no dependencies are downloaded from Maven Central, saving several minutes of network round-trips.
</details>

<details>
<summary>6. What are Reproducible Builds and how does project.build.outputTimestamp achieve them?</summary>
By default, archive formats (JAR, ZIP) record the file creation timestamp for every entry. Compiling the exact same source code on two different days produces different SHA-256 hashes. Setting <code><project.build.outputTimestamp></code> freezes the timestamp metadata inside the JAR to a fixed git commit timestamp, ensuring byte-for-byte identical binaries across independent build machines.
</details>

<details>
<summary>7. How do you run Testcontainers inside a containerized CI environment?</summary>
Testcontainers requires access to a Docker daemon. In CI runners (like GitHub Actions), the Docker socket (<code>/var/run/docker.sock</code>) is available by default on Linux host VMs. In Kubernetes-based CI runners (like GitLab CI or Tekton), you must provide Docker-in-Docker (dind) or mount the host Docker socket with appropriate permissions.
</details>

<details>
<summary>8. What is the purpose of passing -B (--batch-mode) to Maven in CI pipelines?</summary>
The <code>-B</code> flag disables interactive mode and suppresses ANSI color escape codes and dynamic console progress updates. This prevents log spam in CI job output consoles and ensures Maven does not hang waiting for user keyboard input if a plugin prompts for confirmation.
</details>

---

## 11. Practice Exercises

<details>
<summary>Exercise 1: Write a complete GitHub Actions matrix build testing against Java 17, 21, and 25.</summary>

```yaml
jobs:
  matrix-test:
    runs-on: ubuntu-latest
    strategy:
      matrix:
        java-version: ['17', '21', '25']
    steps:
      - uses: actions/checkout@v4
      - uses: actions/setup-java@v4
        with:
          distribution: 'temurin'
          java-version: ${{ matrix.java-version }}
          cache: 'maven'
      - run: ./mvnw -B test
```
</details>

<details>
<summary>Exercise 2: Write a Spring Boot buildpacks command generating a container image.</summary>

```bash
# Using Spring Boot Maven Plugin to build OCI image via Paketo Buildpacks
./mvnw spring-boot:build-image \
  -Dspring-boot.build-image.imageName=registry.example.com/order-service:1.0.0
```
</details>

<details>
<summary>Exercise 3: Configure a JaCoCo coverage check failing the build if instruction coverage is under 80%.</summary>

```xml
<plugin>
    <groupId>org.jacoco</groupId>
    <artifactId>jacoco-maven-plugin</artifactId>
    <version>0.8.12</version>
    <executions>
        <execution>
            <id>check-coverage</id>
            <goals><goal>check</goal></goals>
            <configuration>
                <rules>
                    <rule>
                        <element>BUNDLE</element>
                        <limits>
                            <limit>
                                <counter>INSTRUCTION</counter>
                                <value>COVEREDRATIO</value>
                                <minimum>0.80</minimum>
                            </limit>
                        </limits>
                    </rule>
                </rules>
            </configuration>
        </execution>
    </executions>
</plugin>
```
</details>

---

## 12. Summary / Cheat Sheet
- **Pipeline Stages**: Compile $\rightarrow$ Unit Test $\rightarrow$ Integration Test $\rightarrow$ Coverage Check $\rightarrow$ Container Build $\rightarrow$ Publish.
- **Base Images**: Use Distroless Java (`gcr.io/distroless/java21:nonroot`); never run containers as root.
- **JVM Flags**: Always configure `-XX:MaxRAMPercentage=75.0` and `-XX:+ExitOnOutOfMemoryError`.
- **Google Jib**: Fast, daemonless container creation with intelligent layer separation.
- **CI Caching**: Leverage `actions/setup-java` caching to skip downloading external dependencies.
- **CLI Flags**: Always run Maven with `-B` (batch mode) in CI to avoid console hangs.

---

## 13. Further Reading
- GitHub Actions Documentation: Building and Testing Java
- Google Jib Containerization Guide and Best Practices
- Google Distroless Container Images Repository
- *Continuous Delivery* by Jez Humble and David Farley (Deployment Pipelines)
