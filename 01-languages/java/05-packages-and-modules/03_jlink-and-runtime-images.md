# Custom Runtime Images with jlink: Minimal JRE Deployment

> An authoritative guide to `jlink` (Java Linker / JEP 282), building minimal standalone Java runtimes, stripping debug symbols, and shrinking container footprints from 300MB to 35MB.

| Metadata | Details |
| :--- | :--- |
| **Level** | Intermediate to Advanced |
| **Prerequisites** | [02_java-modules.md](./02_java-modules.md) |
| **Time to Read** | 25 minutes |
| **Status** | Active |
| **Java Version** | Java 9+ (Java 25 LTS verified) |

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
9. [Comparison: Full JDK vs JRE vs jlink Custom Runtime](#9-comparison-full-jdk-vs-jre-vs-jlink-custom-runtime)
10. [Interview Questions](#10-interview-questions)
11. [Practice Exercises](#11-practice-exercises)
12. [Summary & Cheat Sheet](#12-summary--cheat-sheet)
13. [Further Reading](#13-further-reading)

---

## 1. What & Why

Historically in Java, deploying a simple application to production required installing a full standard JRE or JDK on the target server or Docker image. A standard JDK installation weighs between **350MB and 600MB**, containing hundreds of unused packages (GUI toolkits like Swing/AWT, CORBA, Kerberos, management tools).

In cloud-native, containerized environments (Kubernetes, AWS ECS, microservices), large images carry significant costs:
- Slow container pull times during auto-scaling events.
- Bloated memory and disk consumption.
- Massive security attack surfaces containing unneeded native libraries and binaries.

**`jlink` (The Java Linker - JEP 282)** is a tool introduced in Java 9 that analyzes a modular application's dependency graph and generates a **custom, stripped-down, fully self-contained runtime image** containing *only* the specific platform modules and application modules required to run that application.

Using `jlink`, production Java microservices can run inside lightweight custom runtimes as small as **35MB to 45MB**!

---

## 2. Core Concepts

### 2.1 How `jlink` Operates
`jlink` links a set of modular JARs and their transitive JDK platform dependencies into a custom runtime image:

```
Application Modular JAR (requires java.base, java.net.http)
             │
             ▼
     ┌───────────────┐
     │  jlink Tool   │ ◄── Analyzes module graph & pulls JDK jmods
     └───────┬───────┘
             │
             ▼
Custom Standalone JRE Directory (~38MB!)
├── bin/
│   ├── java          (Stripped native executable)
│   └── myapp         (Auto-generated launcher script)
├── lib/
│   ├── modules       (Optimized single jimage file)
│   └── server/
│       └── libjvm.so (Stripped HotSpot runtime)
└── conf/
```

### 2.2 The Key Flags of `jlink`
- `--module-path`: Specifies paths to application modules and JDK modular images (`$JAVA_HOME/jmods`).
- `--add-modules`: Root modules to include in the generated runtime.
- `--output`: Destination directory for the custom image.
- `--strip-debug`: Removes debugging information and line number tables (saves ~30% size).
- `--compress zip-6`: Compresses bytecode and resources inside the runtime image.
- `--no-header-files` & `--no-man-pages`: Excludes C header files and documentation.

---

## 3. How It Works Internally

### 3.1 The `lib/modules` JImage Format
In a `jlink` runtime image, individual `.class` files and `.jar` archives are completely eliminated:
- All classes, module descriptors, and resources are packed into a single, high-performance binary file: `lib/modules`.
- The `lib/modules` format uses an **optimized hash table and perfect hashing algorithm** that enables the JVM to locate and load classes in $O(1)$ time with zero disk fragmentation and zero unzipping overhead!

---

## 4. Syntax & Usage

### 4.1 Invoking `jlink` from the Terminal
```bash
# Generate custom runtime image with launcher script
jlink --module-path $JAVA_HOME/jmods:target/modules \
      --add-modules com.app.billing \
      --launcher run-billing=com.app.billing/com.app.billing.Main \
      --strip-debug \
      --compress zip-6 \
      --no-header-files \
      --no-man-pages \
      --output custom-runtime
```

### 4.2 Executing the Custom Runtime
```bash
# Execute directly via generated launcher script:
./custom-runtime/bin/run-billing

# Or invoke via the custom runtime's java binary:
./custom-runtime/bin/java -m com.app.billing/com.app.billing.Main
```

---

## 5. Practical Examples

### Example 1: Finding Dependencies with `jdeps`
Before running `jlink`, you must identify exactly which JDK platform modules your application uses:
```bash
jdeps --multi-release 25 --print-module-deps target/myapp.jar
```
**Output:**
```text
java.base,java.net.http,java.sql
```
Now you can pass `java.base,java.net.http,java.sql` directly to `jlink`!

### Example 2: Multi-Stage Dockerfile with `jlink`
```dockerfile
# Stage 1: Build & Link
FROM eclipse-temurin:25-jdk-alpine AS builder

WORKDIR /build
COPY target/modules /build/modules

# Generate custom stripped runtime
RUN jlink \
    --module-path /opt/java/openjdk/jmods:/build/modules \
    --add-modules com.example.service \
    --strip-debug \
    --compress zip-6 \
    --no-header-files \
    --no-man-pages \
    --output /custom-runtime

# Stage 2: Ultra-Lightweight Production Runner
FROM alpine:3.20

ENV JAVA_HOME=/opt/java/runtime
ENV PATH="$JAVA_HOME/bin:$PATH"

# Copy ONLY the custom runtime from builder stage
COPY --from=builder /custom-runtime $JAVA_HOME

# Total image size: ~48MB!
CMD ["java", "-m", "com.example.service/com.example.service.Main"]
```

### Example 3: Edge Case – Non-Modular Dependencies (`jlink` with Automatic Modules)
`jlink` strictly requires that **all included modules are explicit modules** (having `module-info.class`).
If your application depends on a legacy non-modular JAR:
- `jlink` will fail with: `Error: automatic module cannot be used with jlink`.
- **Solution:** Use the Maven/Gradle Moditect plugin to inject `module-info.java` into legacy dependencies, or package legacy dependencies into an application modular JAR.

---

## 6. Best Practices

1. **Always Use `jdeps --print-module-deps`:** Never guess required modules; use `jdeps` to calculate the exact minimal set of platform modules.
2. **Combine `--strip-debug`, `--no-header-files`, and `--no-man-pages`:** These three flags safely strip 40-50% of the runtime image size without impacting runtime correctness.
3. **Use Multi-Stage Docker Builds:** Compile and link in a full JDK builder image, and copy only the `jlink` custom image into a minimal base OS (like Alpine or distroless).
4. **Generate Native Launchers via `--launcher`:** Eliminates fragile shell scripts; `jlink` generates native OS wrapper scripts automatically.
5. **Test in CI with Container Image Scanners:** Scanners like Trivy or Grype will report near-zero CVE vulnerabilities because unneeded native libraries are omitted.

---

## 7. Common Mistakes & Pitfalls

| Mistake | Why It Is Wrong | Correct Idiom |
| :--- | :--- | :--- |
| Including automatic modules | `jlink` rejects automatic modules (`Error: automatic module cannot be used with jlink`). | Modularize dependencies using Moditect or use explicit modules. |
| Forgetting `$JAVA_HOME/jmods` in path | `jlink` cannot locate core modules like `java.base`. | Include `$JAVA_HOME/jmods` in `--module-path`. |
| Missing dynamic reflection modules | Omitting modules like `java.sql` if loaded via reflection causes runtime failures. | Add them explicitly via `--add-modules`. |
| Stripping debug info in dev images | Removes line numbers from stack traces, complicating debugging. | Only use `--strip-debug` in production release builds. |
| Confusing `jlink` with GraalVM Native Image | `jlink` produces a stripped standard JVM with JIT; GraalVM compiles ahead-of-time (AOT) to machine code. | Use `jlink` for standard JVM workloads. |

---

## 8. Performance & Security Notes

### 8.1 Reduced Attack Surface
A standard JDK contains native shared libraries for sound, printing, 2D graphics, and cryptographic legacy algorithms. If a CVE is discovered in a native AWT or font rendering library, an enterprise application running in a standard JDK container is vulnerable. A `jlink` image containing only `java.base` and `java.net.http` physically deletes those vulnerable native `.so` / `.dll` files from disk.

---

## 9. Comparison: Full JDK vs JRE vs jlink Custom Runtime

| Dimension | Standard Full JDK | Traditional JRE | jlink Custom Runtime |
| :--- | :--- | :--- | :--- |
| **Size on Disk** | 350MB – 600MB | 150MB – 250MB | **35MB – 50MB** |
| **Modules Included** | All 70+ JDK modules | Most platform modules | Only required modules |
| **Compiler (`javac`)** | ✅ Included | ❌ Omitted | ❌ Omitted |
| **Class Loading** | ZIP filesystem lookup | ZIP filesystem lookup | Optimized binary `lib/modules` |
| **Deployment** | Development only | Legacy production | Modern Cloud-Native Containers |

---

## 10. Interview Questions

<details>
<summary><strong>1. What is jlink and what problem does it solve?</strong></summary>

`jlink` (The Java Linker) is a command-line tool introduced in Java 9 (JEP 282) that builds a custom, stripped-down, fully self-contained runtime image containing only the specific platform modules and application modules required by an application. It reduces deployment sizes from 400MB+ down to ~40MB, dramatically accelerating cloud container pull times and minimizing security attack surfaces.
</details>

<details>
<summary><strong>2. Can jlink package non-modular (legacy) JAR files?</strong></summary>

By default, no. `jlink` enforces strict modularity and rejects automatic modules. To use `jlink` with third-party legacy libraries, you must either:
1. Generate `module-info` descriptors for those libraries using tools like Moditect.
2. Bundle the application and libraries into an explicit modular Fat JAR.
</details>

<details>
<summary><strong>3. How does lib/modules in a jlink image improve startup performance?</strong></summary>

In a standard JRE, the JVM searches through multiple `.jar` zip files on the filesystem, decompressing entries and resolving class structures. In a `jlink` image, all classes are merged into a single optimized `lib/modules` file using an internal perfect hashing index, allowing the JVM to locate and map bytecode in memory in $O(1)$ time with zero decompression overhead.
</details>

<details>
<summary><strong>4. What is the difference between jlink and GraalVM Native Image?</strong></summary>

- `jlink`: Creates a custom, stripped **Java Virtual Machine (JVM)** runtime. Bytecode is still executed via the HotSpot JVM with full JIT compilation, dynamic profiling, and standard JVM ergonomics.
- **GraalVM Native Image:** Compiles Java bytecode **ahead-of-time (AOT)** into a native machine binary (ELF/PE) without a JVM. It offers instant startup (sub-millisecond) and tiny memory footprint, but sacrifices JIT peak throughput and dynamic class loading.
</details>

<details>
<summary><strong>5. What does the jdeps tool do in relation to jlink?</strong></summary>

`jdeps` is a Java dependency analysis tool. Running `jdeps --print-module-deps app.jar` scans the application bytecode and prints the exact comma-separated list of platform modules (e.g. `java.base,java.sql,java.net.http`) required by the app, which can be fed directly to `jlink --add-modules`.
</details>

<details>
<summary><strong>6. What optimization flags are most effective when running jlink?</strong></summary>

- `--strip-debug`: Removes debugging line numbers and local variable tables (~30% reduction).
- `--compress zip-6`: Applies level 6 zip compression to runtime modules.
- `--no-header-files`: Excludes C/C++ header files.
- `--no-man-pages`: Excludes documentation and man pages.
</details>

<details>
<summary><strong>7. Does a jlink image require Java to be installed on the host machine?</strong></summary>

No. A `jlink` runtime image is completely standalone and self-contained. It contains its own native `bin/java` launcher, native HotSpot libraries (`libjvm.so` / `jvm.dll`), and all necessary platform modules.
</details>

<details>
<summary><strong>8. What is the --launcher flag in jlink?</strong></summary>

The `--launcher name=module/main.class` flag instructs `jlink` to generate an executable shell script (or `.bat` on Windows) inside `bin/` that launches the application directly without needing `java -m ...` arguments.
</details>

---

## 11. Practice Exercises

### Easy
1. **Analyze Module Deps:** Use `jdeps` on an existing application JAR to print its platform module dependencies.
2. **Minimal Java Base Runtime:** Use `jlink` to build the smallest possible runtime image containing only `java.base` and measure its disk size.
3. **Execute Custom Java:** Run `custom-runtime/bin/java -version` and verify it matches the host JDK version.

### Medium
4. **Modular Microservice Image:** Build a complete modular microservice using `java.net.http` and assemble a `jlink` runtime image with `--strip-debug`.
5. **Launcher Script Verification:** Generate a runtime image with a custom launcher command and verify execution without parameters.
6. **Docker Multi-Stage Build:** Write and build a multi-stage Dockerfile that produces a < 50MB Alpine Linux container.

### Hard
7. **Moditect Legacy Library Modularization:** Use Moditect to inject a `module-info.class` into a non-modular dependency and link it with `jlink`.
8. **Footprint vs Throughput Benchmark:** Measure memory consumption and throughput of a full JDK container vs a `jlink` container under load.
9. **Class Data Sharing (CDS) with jlink:** Generate a shared archive (`-Xshare:dump`) on top of a `jlink` image and measure startup time improvements.

<details>
<summary><strong>Exercise Solutions</strong></summary>

```bash
# Solution to Exercise 2: Minimal Java Base Runtime
jlink --module-path $JAVA_HOME/jmods \
      --add-modules java.base \
      --strip-debug \
      --compress zip-6 \
      --no-header-files \
      --no-man-pages \
      --output minimal-base-jre

# Check size (typically ~32MB):
du -sh minimal-base-jre
```
</details>

---

## 12. Summary & Cheat Sheet

```
┌────────────────────────────────────────────────────────────────────────┐
│                          JLINK CHEAT SHEET                             │
├────────────────────────────────────────────────────────────────────────┤
│ Purpose:            Build miniature, self-contained custom JRE runtimes│
│ Dependency Finder:  jdeps --print-module-deps app.jar                  │
│ Syntax:             jlink --module-path $JAVA_HOME/jmods:mods          │
│                           --add-modules mod.name                       │
│                           --strip-debug                                │
│                           --compress zip-6                             │
│                           --no-header-files --no-man-pages             │
│                           --output custom-jre                          │
│ Custom Launcher:    --launcher run=module.name/com.app.Main            │
│ Target Size:        35MB to 50MB (vs 400MB standard JDK)               │
│ Constraint:         All modules MUST be explicit (No automatic modules)│
└────────────────────────────────────────────────────────────────────────┘
```

---

## 13. Further Reading
- [JEP 282: jlink: The Java Linker](https://openjdk.org/jeps/282)
- Oracle Official Documentation: jlink User Guide
- *The Java Module System*, Nicolai Parlog, Chapter 13: "Custom Runtime Images".
