# Containerization with Docker and Google Jib in Spring Boot

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
- [4. Syntax & Plugin Configuration Reference](#4-syntax--plugin-configuration-reference)
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
Building Docker images using traditional `docker build` and manual Dockerfiles presents major operational bottlenecks in modern CI/CD pipelines:
- **Docker Daemon Dependency**: Building images requires running the Docker daemon (`dockerd`). In Kubernetes CI environments (e.g. GitLab CI, GitHub Actions runners, Tekton), this requires insecure **Docker-in-Docker (DinD)** configurations, which require privileged container permissions and introduce severe security vulnerabilities.
- **Slow, Non-Reproducible Builds**: Standard Dockerfiles execute sequentially, pulling base images and running shell commands (`apt-get`, `chmod`). Build timestamps differ on every run, breaking build reproducibility.
- **OS Bloat and CVE Exposure**: Packaging full Linux operating systems (Debian, Ubuntu) into containers introduces hundreds of unused binaries (shells, package managers, compilers) that trigger critical security vulnerability alerts in container scanners.

### Historical Evolution
Created by Google, **Jib** revolutionized Java containerization. Jib is a Maven and Gradle plugin that builds Open Container Initiative (OCI) and Docker-compatible container images **directly from Java build artifacts without requiring a Dockerfile or an installed Docker daemon**.

Coupled with Google's **Distroless** container images, Jib packages Spring Boot applications into minimal, secure, and reproducible images that contain strictly the Java runtime and application classes—eliminating shells, package managers, and root access completely.

### Production Relevance
Adopting Google Jib and Distroless containers guarantees that:
- CI/CD build runners execute completely daemonless without elevated root/privileged access.
- Image sizes shrink by up to 70%, with **zero OS-level CVE vulnerabilities**.
- Builds are 100% reproducible down to the exact bit-level digest.

---

## 2. Core Concepts

### Jib and Container Primitives
1. **Daemonless Container Builds**: Jib constructs OCI-compliant tarballs and pushes layers directly to a remote container registry (AWS ECR, Docker Hub, Google Artifact Registry) using HTTP REST APIs, without ever calling the Docker CLI or daemon.
2. **Declarative Layering**: Jib automatically separates your application into optimized caching layers:
   - Dependencies (JARs from Maven Central)
   - Snapshot Dependencies
   - Project Resources (`src/main/resources`)
   - Project Classes (`src/main/java`)
3. **Distroless Base Images (`gcr.io/distroless/java21`)**: Minimal container images containing *only* the Java runtime and its system library dependencies (glibc, libssl). They contain **no Linux shell (`/bin/sh`, `/bin/bash`)**, no package managers (`apt`, `apk`), and no standard Unix utilities.
4. **Reproducible Builds**: Jib normalizes file creation timestamps (epoch 1970) and file permissions, ensuring that compiling the exact same Git commit always produces the exact same container image SHA-256 hash.

---

## 3. How It Works Internally

### Google Jib Direct Registry Push Architecture

```
DEVELOPER / CI RUNNER (Zero Docker Daemon Required!)
  |
  |  mvn compile jib:build
  v
+------------------------------------------------------------------------------------+
| 1. GOOGLE JIB MAVEN ENGINE                                                         |
|    - Inspects compiled target/classes and dependencies                             |
|    - Organizes artifacts into 4 separate OCI layer tarballs                        |
+------------------------------------------------------------------------------------+
  |
  v
+------------------------------------------------------------------------------------+
| 2. DISTROLESS BASE IMAGE REGISTRY MANIFEST CHECK                                   |
|    - Queries target registry for gcr.io/distroless/java21-debian12                 |
|    - Identifies which layers already exist in registry cache                       |
+------------------------------------------------------------------------------------+
  |
  v
+------------------------------------------------------------------------------------+
| 3. DIRECT HTTP/2 REGISTRY UPLOAD (Docker Registry v2 API)                         |
|    - Uploads ONLY the modified classes layer (~1MB) via HTTP PUT                   |
|    - Assembles OCI Image Manifest and config JSON                                  |
|    - Pushes image tag: registry.company.com/order-service:1.0.0                    |
+------------------------------------------------------------------------------------+
  |
  v
CONTAINER REGISTRY (AWS ECR / Harbor / Docker Hub)
Image is immediately ready to be pulled by Kubernetes!
```

---

## 4. Syntax & Plugin Configuration Reference

### Production Jib Maven Plugin Configuration (`pom.xml`)

```xml
<project>
    <build>
        <plugins>
            <plugin>
                <groupId>com.google.cloud.tools</groupId>
                <artifactId>jib-maven-plugin</artifactId>
                <version>3.4.4</version>
                <configuration>
                    <!-- Base Image: Distroless Java 21 LTS -->
                    <from>
                        <image>gcr.io/distroless/java21-debian12:nonroot</image>
                    </from>
                    <!-- Target Destination Image -->
                    <to>
                        <image>registry.company.com/apps/${project.artifactId}:${project.version}</image>
                    </to>
                    <!-- Container Runtime Configuration -->
                    <container>
                        <!-- Runs as non-root user (UID 65532 built into distroless) -->
                        <user>nonroot:nonroot</user>
                        <ports>
                            <port>8080</port>
                            <port>9090</port>
                        </ports>
                        <jvmFlags>
                            <jvmFlag>-XX:MaxRAMPercentage=75.0</jvmFlag>
                            <jvmFlag>-XX:InitialRAMPercentage=75.0</jvmFlag>
                            <jvmFlag>-XX:+ExitOnOutOfMemoryError</jvmFlag>
                            <jvmFlag>-XX:+UseG1GC</jvmFlag>
                            <jvmFlag>-Djava.security.egd=file:/dev/./urandom</jvmFlag>
                        </jvmFlags>
                        <environment>
                            <SPRING_PROFILES_ACTIVE>prod</SPRING_PROFILES_ACTIVE>
                        </environment>
                        <creationTime>USE_CURRENT_TIMESTAMP</creationTime>
                    </container>
                </configuration>
            </plugin>
        </plugins>
    </build>
</project>
```

### Jib Build Goals
```bash
# 1. Build and push directly to remote registry (No Docker required)
mvn compile jib:build

# 2. Build image into local Docker daemon (if running locally for testing)
mvn compile jib:dockerBuild

# 3. Export image to a local tar file
mvn compile jib:buildTar
```

---

## 5. Practical Examples

### Example 1: Comparing Traditional Dockerfile vs. Google Jib

#### Approach A: Multi-Stage Dockerfile (Traditional)
```dockerfile
FROM maven:3.9-eclipse-temurin-21 AS build
WORKDIR /app
COPY pom.xml .
RUN mvn dependency:go-offline
COPY src src
RUN mvn clean package -DskipTests

FROM eclipse-temurin:21-jre-jammy
WORKDIR /app
COPY --from=build /app/target/*.jar app.jar
USER 1001:1001
ENTRYPOINT ["java", "-jar", "app.jar"]
```
*Drawback*: Requires a Docker daemon, compiles slowly, pulls heavy build images, and includes a full Linux shell.

#### Approach B: Google Jib in Maven (`pom.xml`)
```xml
<plugin>
    <groupId>com.google.cloud.tools</groupId>
    <artifactId>jib-maven-plugin</artifactId>
    <version>3.4.4</version>
    <configuration>
        <from><image>gcr.io/distroless/java21-debian12:nonroot</image></from>
        <to><image>myregistry.io/billing-service:1.0.0</image></to>
    </configuration>
</plugin>
```
```bash
./mvnw compile jib:build
```
*Result*: Builds and pushes the image in **12 seconds** without Docker installed!

### Example 2: Security Scanning Distroless vs. Standard Ubuntu Images with Trivy
When scanning standard Ubuntu-based JRE containers versus Google Distroless containers with Trivy:

```bash
# 1. Scan standard Ubuntu JRE image:
trivy image openjdk:21-jdk-slim
# Output: Total Vulnerabilities: 48 (5 Critical, 14 High, 29 Medium)
# Vulnerabilities found in: bash, coreutils, login, libssl, apt, dpkg

# 2. Scan Google Distroless JRE image:
trivy image gcr.io/distroless/java21-debian12:nonroot
# Output: Total Vulnerabilities: 0 (Zero Vulnerabilities Detected!)
```
*Takeaway*: Because distroless contains no shell, no package manager, and no OS utilities, 100% of standard OS-level CVEs are eliminated!

### Example 3: Adding Extra Files (Agents, Certificates) with Jib `extraDirectories`
If you need to include an OpenTelemetry Java agent or custom corporate root CA certificates:

```xml
<configuration>
    <extraDirectories>
        <paths>
            <path>
                <from>src/main/jib-extra</from>
                <into>/app/extra</into>
            </path>
        </paths>
        <permissions>
            <permission>
                <file>/app/extra/opentelemetry-javaagent.jar</file>
                <mode>755</mode>
            </permission>
        </permissions>
    </extraDirectories>
    <container>
        <jvmFlags>
            <jvmFlag>-javaagent:/app/extra/opentelemetry-javaagent.jar</jvmFlag>
        </jvmFlags>
    </container>
</configuration>
```

### Example 4: Anti-Pattern vs. Production Fix (Privileged Docker-in-Docker in CI)
```yaml
# ANTI-PATTERN: Using Docker-in-Docker (DinD) in Kubernetes CI/CD runners
# Requires: privileged: true (Grants container full root access to host node kernel!)
# Major Security Breach: Compromising this runner compromises the entire Kubernetes cluster!
spec:
  containers:
    - name: dind-daemon
      image: docker:dind
      securityContext:
        privileged: true # CATASTROPHIC SECURITY HOLE!

# PRODUCTION FIX: Daemonless Google Jib
# Executes standard unprivileged Java process; pushes directly via HTTPS APIs!
spec:
  containers:
    - name: maven-builder
      image: maven:3.9-eclipse-temurin-21
      command: ["mvn", "compile", "jib:build"]
      securityContext:
        privileged: false
        readOnlyRootFilesystem: true
```

### Example 5: Debugging Distroless Containers via Ephemeral Containers
Because Distroless contains no shell (`/bin/sh`), you cannot run `kubectl exec -it pod -- /bin/bash`. In modern Kubernetes, troubleshoot using **Kubernetes Ephemeral Debug Containers**:

```bash
# Attach an ephemeral debug container with bash and diagnostic tools to the live distroless pod!
kubectl debug -it order-service-78f99-x21z \
  --image=busybox \
  --target=order-service
```
*Result*: An interactive shell opens sharing the exact same process and network namespace as the distroless Spring Boot app without compromising the production image!

---

## 6. Best Practices & Production Guidelines

1. **Standardize on Distroless Base Images**: Use `gcr.io/distroless/java21-debian12:nonroot`. Distroless images reduce image size by up to 70% and eliminate attack surfaces by omitting shells, compilers, and package managers.
2. **Execute as `nonroot` User**: Distroless provides a built-in unprivileged user (`nonroot`, UID 65532). Ensure `<user>nonroot:nonroot</user>` is declared in Jib configurations.
3. **Use Jib for Daemonless CI/CD Pipelines**: Eliminate Docker-in-Docker (DinD) from GitHub Actions, GitLab CI, and Tekton. Jib builds and pushes OCI images purely via standard Java network calls.
4. **Set Explicit JVM Container Flags in Jib**: Always configure `-XX:MaxRAMPercentage=75.0` and `-XX:+ExitOnOutOfMemoryError` in `<jvmFlags>` to ensure the JVM respects Kubernetes resource limits.
5. **Authenticate via Registry Credentials or IAM Roles**: Never hardcode registry credentials in `pom.xml`. In AWS ECR, use the `jib-ecr-extension` or pass credentials via environment variables (`JIB_TO_AUTH_USERNAME`, `JIB_TO_AUTH_PASSWORD`).

---

## 7. Common Mistakes & Pitfalls

| Mistake | Why It Fails | Practical Remedy |
|---|---|---|
| Attempting `docker exec -it ... /bin/sh` on distroless | Distroless contains no shell binary; command fails with `executable file not found in $PATH`. | Use Kubernetes ephemeral debug containers (`kubectl debug`). |
| Hardcoding registry credentials in `pom.xml` | Exposes registry authentication tokens in source control. | Use Maven credential helpers or environment variables. |
| Hardcoding fixed `-Xmx` in Jib `<jvmFlags>` | Prevents dynamic scaling when Kubernetes pod memory limits are adjusted. | Use `-XX:MaxRAMPercentage=75.0`. |
| Running container as `root` in custom Dockerfile | Violates security compliance frameworks (CIS benchmarks, PCI-DSS). | Use distroless `nonroot` or declare `USER 1001:1001`. |
| Forgetting to expose management ports in Jib | Actuator port 9090 is not documented in image metadata, complicating network mapping. | Add `<port>9090</port>` under `<container><ports>`. |
| Missing `creationTime: USE_CURRENT_TIMESTAMP` | By default, Jib sets creation time to epoch 0 (1970) for reproducibility, which confuses some legacy tools. | Set `<creationTime>USE_CURRENT_TIMESTAMP</creationTime>` if human-readable dates are required. |

---

## 8. Performance & Security Notes

### Elimination of Attack Vectors via Distroless
- Attackers who discover a Remote Code Execution (RCE) vulnerability typically attempt to spawn a shell (`Runtime.getRuntime().exec("/bin/sh")`) to curl malicious payloads or establish reverse shells.
- In a distroless container, `/bin/sh`, `/bin/bash`, `curl`, and `wget` **do not exist**. Attempts to execute shell commands throw immediate `IOException: No such file or directory`, neutralizing the exploit.

### Reproducible Builds and Supply Chain Integrity
- Jib guarantees reproducible builds: building the exact same Git commit produces the exact same image digest (`sha256:abc...`).
- This allows security teams to verify that binary container artifacts have not been tampered with between build and deployment.

---

## 9. Comparison / When to Use What

| Feature | Google Jib | Multi-Stage Dockerfile | Cloud Native Buildpacks (CNB) |
|---|---|---|---|
| **Requires Docker Daemon?** | **No (Daemonless)** | Yes (Requires dockerd) | No (Can run daemonless with pack) |
| **Configuration Location** | `pom.xml` / `build.gradle` | `Dockerfile` | `pom.xml` or zero-config |
| **Base Image Customization**| **Any base (Distroless/Temurin)** | Any base | Tied to CNB Stack (Paketo/Ubuntu) |
| **Layer Caching** | **Automatic (Classes, Deps)** | Manual (Requires layer extraction)| Automatic (Slices) |
| **Build Speed** | **Fastest (Direct HTTP upload)** | Medium | Medium-Slow |
| **Shell Included?** | **No (Distroless)** | Yes (unless using distroless)| Yes (Standard Ubuntu runtime) |

---

## 10. Interview Questions & Answers

### Q1: How does Google Jib build container images without a Docker daemon?
<details>
<summary>Click to view answer</summary>

Jib bypasses the Docker CLI and daemon entirely. An OCI or Docker container image is fundamentally a collection of tarballs (representing filesystem layers) and a JSON manifest document. Jib compiles Java source code, bundles classes and dependencies into standard tarballs in memory, and pushes them directly to the remote container registry via standard HTTPS calls implementing the Docker Registry v2 API.
</details>

### Q2: What is a "Distroless" container image and what are its primary advantages?
<details>
<summary>Click to view answer</summary>

A Distroless image (created by Google) contains strictly the application runtime (Java 21 JRE) and minimal shared C libraries (glibc, libssl). It contains no operating system package managers (apt, apk), no shells (sh, bash), and no standard Linux command utilities (ls, curl, top).  
Advantages:
1. Drastically reduced image size (often under 200MB).
2. Elimination of OS-level security vulnerabilities (zero CVEs).
3. Hardened attack surface: attackers cannot execute shell scripts or download arbitrary tools if a vulnerability is exploited.
</details>

### Q3: Why does Google Jib set the image creation timestamp to epoch 0 (1970) by default?
<details>
<summary>Click to view answer</summary>

For **reproducible builds**. If the build timestamp were set to the current time, two builds of the exact same Git commit would produce different image hashes. By setting the timestamp to a fixed value (January 1, 1970 UTC), Jib ensures that identical code and dependencies always produce an identical SHA-256 image digest. If desired, this can be overridden using `<creationTime>USE_CURRENT_TIMESTAMP</creationTime>`.
</details>

### Q4: How do you troubleshoot or inspect a running Distroless Spring Boot pod in Kubernetes?
<details>
<summary>Click to view answer</summary>

Because Distroless containers lack `/bin/sh`, running `kubectl exec -it <pod> -- /bin/sh` fails. The standard production troubleshooting method is to use **Kubernetes Ephemeral Debug Containers**:
```bash
kubectl debug -it <pod_name> --image=busybox --target=<container_name>
```
The debug container runs with busybox utilities while attaching directly to the target container's process namespace, allowing operators to run `ps`, inspect files, and analyze processes.
</details>

### Q5: What layers does Jib create automatically for Spring Boot?
<details>
<summary>Click to view answer</summary>

Jib splits the application into 4 distinct layers ordered by volatility:
1. `dependencies`: Third-party release dependencies from Maven Central.
2. `snapshot-dependencies`: Internal project snapshot dependencies.
3. `resources`: Static resources, templates, and `application.yml`.
4. `classes`: Compiled Java `.class` files.
When code changes, only the `classes` layer is rebuilt and pushed.
</details>

### Q6: Why is Docker-in-Docker (DinD) considered a security risk in CI/CD pipelines?
<details>
<summary>Click to view answer</summary>

To run the Docker daemon inside a container in CI/CD, the container must be run in `privileged` mode (`--privileged`). Privileged containers have access to all host kernel devices, can bypass AppArmor/SELinux profiles, and can manipulate host filesystems. If an attacker injects malicious code into the CI build pipeline, they can escape the container and achieve complete root control over the host Kubernetes node. Jib eliminates the need for privileged DinD entirely.
</details>

### Q7: Can Google Jib build GraalVM native executables?
<details>
<summary>Click to view answer</summary>

No. Jib is designed specifically to containerize standard JVM bytecode (`.class` files and JAR dependencies). For compiling and containerizing GraalVM native executables, use Cloud Native Buildpacks (`mvn spring-boot:build-image`) or multi-stage Dockerfiles with GraalVM Native Build Tools.
</details>

### Q8: How do you supply custom JVM flags when using Google Jib?
<details>
<summary>Click to view answer</summary>

Via the `<container><jvmFlags>` block in `pom.xml`:
```xml
<container>
    <jvmFlags>
        <jvmFlag>-XX:MaxRAMPercentage=75.0</jvmFlag>
        <jvmFlag>-XX:+ExitOnOutOfMemoryError</jvmFlag>
    </jvmFlags>
</container>
```
Alternatively, at runtime, pass the standard environment variable `JAVA_TOOL_OPTIONS="-XX:MaxRAMPercentage=75.0"`.
</details>

---

## 11. Practice Exercises

### Exercise 1: Build Local Image Using Jib Docker Daemon (Easy)
<details>
<summary>Click to view challenge & solution</summary>

**Challenge**: Write the Maven command to build a container image directly into your local Docker daemon named `my-app:dev` using Jib.

**Solution**:
```bash
mvn compile com.google.cloud.tools:jib-maven-plugin:dockerBuild -Dimage=my-app:dev
```
</details>

### Exercise 2: Configure Complete Jib Plugin with Distroless Base in pom.xml (Medium)
<details>
<summary>Click to view challenge & solution</summary>

**Challenge**: Configure `pom.xml` with Jib to build from `gcr.io/distroless/java21-debian12:nonroot`, expose ports 8080 and 9090, set 75% heap limit, and push to `registry.internal/crm/customer-service:1.2.0`.

**Solution**:
```xml
<plugin>
    <groupId>com.google.cloud.tools</groupId>
    <artifactId>jib-maven-plugin</artifactId>
    <version>3.4.4</version>
    <configuration>
        <from>
            <image>gcr.io/distroless/java21-debian12:nonroot</image>
        </from>
        <to>
            <image>registry.internal/crm/customer-service:1.2.0</image>
        </to>
        <container>
            <user>nonroot:nonroot</user>
            <ports>
                <port>8080</port>
                <port>9090</port>
            </ports>
            <jvmFlags>
                <jvmFlag>-XX:MaxRAMPercentage=75.0</jvmFlag>
                <jvmFlag>-XX:+ExitOnOutOfMemoryError</jvmFlag>
                <jvmFlag>-XX:+UseG1GC</jvmFlag>
            </jvmFlags>
            <creationTime>USE_CURRENT_TIMESTAMP</creationTime>
        </container>
    </configuration>
</plugin>
```
</details>

### Exercise 3: GitHub Actions Daemonless CI Pipeline with Jib and ECR (Hard)
<details>
<summary>Click to view challenge & solution</summary>

**Challenge**: Author a GitHub Actions workflow that compiles a Spring Boot application, authenticates to Amazon ECR, and executes a daemonless Jib build and push without running Docker.

**Solution**:
```yaml
name: Daemonless Build & Deploy to ECR

on:
  push:
    branches: [ main ]

jobs:
  build-and-push:
    runs-on: ubuntu-latest
    permissions:
      id-token: write
      contents: read
    steps:
      - name: Checkout Code
        uses: actions/checkout@v4

      - name: Set up JDK 21
        uses: actions/setup-java@v4
        with:
          java-version: '21'
          distribution: 'temurin'
          cache: maven

      - name: Configure AWS Credentials (OIDC)
        uses: aws-actions/configure-aws-credentials@v4
        with:
          role-to-assume: arn:aws:iam::123456789012:role/GitHubActionsECRRole
          aws-region: us-east-1

      - name: Log in to Amazon ECR
        id: login-ecr
        uses: aws-actions/amazon-ecr-login@v2

      - name: Build & Push Image with Jib (No Docker Daemon!)
        env:
          REGISTRY: ${{ steps.login-ecr.outputs.registry }}
        run: |
          mvn compile jib:build \
            -Djib.to.image=$REGISTRY/order-service:${{ github.sha }} \
            -DskipTests
```
</details>

---

## 12. Summary / Cheat Sheet

```
+---------------------------------------------+-------------------------------------------------------------+
| Feature / Concept                           | Description & Production Rule                               |
+---------------------------------------------+-------------------------------------------------------------+
| Google Jib                                  | Daemonless container builder; pushes directly via HTTP APIs |
| Distroless (gcr.io/distroless/java21)       | Minimal secure base image: NO shell, NO package manager     |
| mvn compile jib:build                       | Builds and pushes image directly to remote registry         |
| mvn compile jib:dockerBuild                 | Builds image into local Docker daemon                       |
| nonroot:nonroot                             | Unprivileged system user (UID 65532) in Distroless          |
| kubectl debug                               | Tool used to attach debug shells to distroless containers   |
| Zero-CVE Guarantee                          | Eliminates all OS-level shell and utility vulnerabilities   |
+---------------------------------------------+-------------------------------------------------------------+
```

---

## 13. Further Reading
- Google Container Tools: Jib Official GitHub Documentation
- Google Distroless Official Container Images Repository
- Kubernetes Documentation: Debugging Pods with Ephemeral Debug Containers
- SRE Best Practices: Daemonless Container Builds in Cloud Native CI/CD