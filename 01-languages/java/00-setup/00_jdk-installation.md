# JDK Installation and Architecture

> The Java Development Kit (JDK) is the foundational software development package providing the compiler (`javac`), runtime launcher (`java`), core class libraries, and diagnostic toolchains required to engineer, profile, and execute modern Java applications.

| Metadata | Value |
| :--- | :--- |
| **Level** | Beginner |
| **Prerequisites** | None |
| **Time to Read** | 25 minutes |
| **Status** | Complete |
| **Java LTS Baseline** | Java 25 LTS (with migration notes for Java 8, 11, 17, 21) |

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
10. [Interview Questions](#10-interview-questions)
11. [Practice Exercises](#11-practice-exercises)
12. [Summary / Cheat Sheet](#12-summary--cheat-sheet)
13. [Further Reading](#13-further-reading)

---

## 1. What & Why

### Problem Statement
Software development requires an environment capable of translating human-readable source code into machine-executable instructions, isolating application code from underlying operating system differences, and providing comprehensive diagnostic tooling for monitoring and debugging running applications. Without a unified SDK, developers would need separate compilers, runtime engines, and profiling utilities for each target platform.

### Historical Context
- **Java 1.0 to Java 8:** Oracle distributed separate packages: the **JRE (Java Runtime Environment)** for end-users containing only the virtual machine and standard libraries, and the **JDK** for developers containing the compiler and tools.
- **Java 9 Modularization (JPMS):** The separate JRE distribution was deprecated and subsequently removed in Java 11. The JDK became a single, self-contained modular distribution. Custom minimal runtimes are now created on demand using `jlink`.
- **Modern Cadence (Java 17 $\to$ 21 $\to$ 25):** Java follows a strict six-month release cycle, with Long-Term Support (LTS) releases every two years. Java 25 represents the definitive enterprise LTS baseline, delivering mature Virtual Threads, Sequenced Collections, Scoped Values, Stream Gatherers, and the new Class-File API.

### Real-World Relevance
Every enterprise Java system—from high-frequency trading engines and microservice meshes to Apache Kafka brokers and Android toolchains—relies on a correctly configured, secure, and performant JDK distribution. Selecting the correct vendor distribution and setting up robust environment variables is the critical first step in production engineering.

---

## 2. Core Concepts

### The Architectural Triad: JVM, JRE, and JDK

```
+---------------------------------------------------------------------------------+
|                                      JDK                                        |
|  Development Tools: javac, jshell, jar, javadoc, jcmd, jstack, jmap, jfr, jlink  |
|                                                                                 |
|  +---------------------------------------------------------------------------+  |
|  |                    Java Standard Library (java.base, ...)                 |  |
|  |                                                                           |  |
|  |  +---------------------------------------------------------------------+  |  |
|  |  |                                JVM                                  |  |  |
|  |  |   Class Loader Subsystem  |  Execution Engine (JIT C1/C2 + Interp)  |  |  |
|  |  |   Runtime Data Areas      |  Garbage Collector (ZGC, G1, Parallel)  |  |  |
|  |  +---------------------------------------------------------------------+  |  |
|  +---------------------------------------------------------------------------+  |
+---------------------------------------------------------------------------------+
```

- **JVM (Java Virtual Machine):** The abstract computing machine that loads compiled `.class` files, verifies bytecode security, interprets instructions, executes just-in-time (JIT) compilation to native assembly, and manages automatic memory reclamation (GC).
- **Standard Library:** The modular set of packages defined in the Java SE specification, rooted in `java.base` (e.g., `java.lang`, `java.util`, `java.io`, `java.nio`, `java.net`, `java.util.concurrent`).
- **JDK (Java Development Kit):** The superset bundle containing the JVM, the complete modular standard library, and the developer command-line utilities.

### Key JDK Utilities
| Binary | Description | Primary Role |
| :--- | :--- | :--- |
| `javac` | The Java language compiler | Translates `.java` source code into platform-independent `.class` bytecode. |
| `java` | The application launcher | Spawns the JVM process, configures heap and GC, and executes the specified class or JAR. |
| `jshell` | Java REPL (Read-Eval-Print Loop) | Interactive shell for exploratory prototyping, evaluating expressions, and testing APIs. |
| `jar` | Archive management tool | Packages class files and resources into a single zip-compatible `.jar` archive. |
| `jcmd` | Unified JVM diagnostic utility | Sends diagnostic commands to running JVM processes (thread dumps, GC triggers, JFR recording). |
| `jlink` | Custom runtime image builder | Assembles a minimal, stripped-down JVM containing only the modules needed by an application. |
| `jpackage` | Native installer packager | Packages Java applications into native OS installers (`.msi`, `.exe`, `.deb`, `.rpm`, `.dmg`). |

---

## 3. How It Works Internally

### JDK Directory Structure & Process Layout
A modern modular JDK (such as Oracle JDK 25 or Eclipse Temurin 25) conforms to a standardized filesystem hierarchy:

```
<JDK_HOME>/
├── bin/                 # Native executables (java.exe, javac.exe, jcmd.exe)
│   ├── server/          # jvm.dll / libjvm.so (HotSpot virtual machine core engine)
├── conf/                # Security policies, management configs (logging.properties, net.properties)
├── include/             # C/C++ header files for JNI and native integration (jni.h, jvmti.h)
├── jmods/               # Pre-compiled module packages (java.base.jmod, etc.) for jlink
├── legal/               # Vendor licensing terms and copyright notices
├── lib/                 # Core internal modules, tzdb (time zone data), class libraries
└── release              # Metadata file specifying JAVA_VERSION, OS_NAME, and OS_ARCH
```

### OS Process Execution Architecture
When you invoke `java -jar app.jar` from your shell:

```
[ User Shell (bash/powershell) ]
       │
       ▼
1. Resolves binary via PATH environment variable -> points to <JDK_HOME>/bin/java.exe
       │
       ▼
2. Launcher reads OS command arguments and environment variables (JAVA_HOME, _JAVA_OPTIONS)
       │
       ▼
3. Launcher dynamically loads <JDK_HOME>/bin/server/jvm.dll (or libjvm.so) using dlopen()/LoadLibrary()
       │
       ▼
4. Calls JNI_CreateJavaVM() entry point exported by the JVM shared library
       │
       ▼
5. JVM initializes runtime subsystems:
   ├── Initializes memory spaces: Metaspace, Thread Stacks, Young Gen, Old Gen
   ├── Boots Primordial ClassLoader to load java.base classes
   └── Initializes Signal Handlers and Safepoint mechanisms
       │
       ▼
6. JVM locates and loads main class -> executes public static void main(String[] args)
```

### PATH vs JAVA_HOME Resolution
- `JAVA_HOME`: Must point directly to the root of the JDK installation directory (e.g., `C:\Program Files\Java\jdk-25` or `/usr/lib/jvm/temurin-25`). Build tools (Maven, Gradle, Ant), application servers, and IDEs read this variable to discover the compiler and JMODs.
- `PATH`: Must include `%JAVA_HOME%\bin` (Windows) or `$JAVA_HOME/bin` (POSIX). The operating system shell traverses `PATH` sequentially to locate the `java` and `javac` executables.

---

## 4. Syntax & Usage

### Verifying the Toolchain
Open your terminal and verify that both the runtime and compiler report the expected Java 25 LTS build:

```bash
# Check JVM launcher version and vendor
$ java --version

# Output:
# java 25.0.1 2025-10-21 LTS
# Java(TM) SE Runtime Environment (build 25.0.1+8-LTS-27)
# Java HotSpot(TM) 64-Bit Server VM (build 25.0.1+8-LTS-27, mixed mode, sharing)

# Check Java compiler version
$ javac --version

# Output:
# javac 25.0.1
```

### Environment Variable Setup Across Platforms

#### Windows (PowerShell)
```powershell
# Set for current session
$env:JAVA_HOME = "C:\Program Files\Java\jdk-25"
$env:Path = "$env:JAVA_HOME\bin;" + $env:Path

# Persist permanently for user environment
[System.Environment]::SetEnvironmentVariable("JAVA_HOME", "C:\Program Files\Java\jdk-25", "User")
[System.Environment]::SetEnvironmentVariable("Path", "%JAVA_HOME%\bin;" + [System.Environment]::GetEnvironmentVariable("Path", "User"), "User")
```

#### Linux & macOS (bash / zsh)
```bash
# Add to ~/.bashrc or ~/.zshrc
export JAVA_HOME="/usr/lib/jvm/temurin-25-jdk"
export PATH="$JAVA_HOME/bin:$PATH"

# Apply immediately
source ~/.bashrc
```

---

## 5. Practical Examples

### Example 1: Basic Idiomatic Usage — Inspecting Runtime Version
- **Goal:** Programmatically retrieve and parse the active Java version and vendor properties using the type-safe `Runtime.Version` API (introduced in Java 9, standard in Java 25).
```java
public class JdkProbe {
    public static void main(String[] args) {
        Runtime.Version ver = Runtime.version();
        System.out.println("Java Feature: " + ver.feature());
        System.out.println("Java Interim: " + ver.interim());
        System.out.println("Java Update:  " + ver.update());
        System.out.println("VM Name:      " + System.getProperty("java.vm.name"));
        System.out.println("VM Vendor:    " + System.getProperty("java.vm.vendor"));
        System.out.println("Java Home:    " + System.getProperty("java.home"));
    }
}
```
- **Console Output:**
```text
Java Feature: 25
Java Interim: 0
Java Update:  1
VM Name:      Java HotSpot(TM) 64-Bit Server VM
VM Vendor:    Oracle Corporation
Java Home:    C:\Program Files\Java\jdk-25
```
- **Explanation:** `Runtime.version()` returns a structured `Runtime.Version` record containing feature, interim, update, and patch numbers, eliminating fragile regex parsing of the `java.version` string.

---

### Example 2: Real-World Enterprise Scenario — Diagnostic Environment Probe
- **Goal:** Probe the running JVM's process ID, spec version, uptime, and memory usage via JMX Management Beans.
```java
import java.lang.management.ManagementFactory;
import java.lang.management.MemoryMXBean;
import java.lang.management.RuntimeMXBean;

public class JvmDiagnosticsProbe {
    public static void main(String[] args) {
        RuntimeMXBean runtime = ManagementFactory.getRuntimeMXBean();
        MemoryMXBean mem = ManagementFactory.getMemoryMXBean();

        System.out.println("PID: " + ProcessHandle.current().pid());
        System.out.println("JVM Spec Version: " + runtime.getSpecVersion());
        System.out.println("Uptime Valid: " + (runtime.getUptime() >= 0));
        System.out.println("Init Heap (MB): " + (mem.getHeapMemoryUsage().getInit() / (1024 * 1024)));
        System.out.println("Max Heap (MB):  " + (mem.getHeapMemoryUsage().getMax() / (1024 * 1024)));
    }
}
```
- **Console Output:**
```text
PID: 9628
JVM Spec Version: 25
Uptime Valid: true
Init Heap (MB): 252
Max Heap (MB):  4008
```
- **Explanation:** Production microservices use JMX beans or Micrometer metrics bindings to expose real-time JVM telemetry (heap limits, garbage collection counts, and thread states) to Prometheus or Datadog.

---

### Example 3: Edge Case — Verifying Single-File Source Execution
- **Goal:** Demonstrate Java's ability (since Java 11, refined in modern Java) to compile and execute a single-file program in memory without leaving a `.class` file on disk.
```bash
# Execute directly from source code without explicit 'javac' invocation:
$ java JdkProbe.java
```
- **Console Output:**
```text
Java Feature: 25
Java Interim: 0
Java Update:  1
VM Name:      Java HotSpot(TM) 64-Bit Server VM
VM Vendor:    Oracle Corporation
Java Home:    C:\Program Files\Java\jdk-25
```
- **Explanation:** When `java` is passed a file ending in `.java`, it invokes an in-memory compiler, generates bytecode in memory, and dispatches to the entry point, enabling lightweight scripting and CI automation.

---

### Example 4: Failure & Anti-Pattern Case — Path / Home Desynchronization
- **Goal:** Demonstrate the classic production fault where `javac` compiles code targeting a newer bytecode version, but `java` invokes an older runtime, causing an `UnsupportedClassVersionError`.
```bash
# Scenario: Developer compiles with JDK 25:
$ javac --release 25 App.java

# But PATH points to a legacy Java 17 runtime:
$ java App
```
- **Console Error:**
```text
Error: LinkageError occurred while loading main class App
        java.lang.UnsupportedClassVersionError: App has been compiled by a more recent version of the Java Runtime (class file version 69.0), this version of the Java Runtime only recognizes class file versions up to 61.0
```
- **Explanation & Fix:** Bytecode class file version 69 corresponds to Java 25, while 61 corresponds to Java 17. Ensure `PATH` references `%JAVA_HOME%\bin` rather than a hardcoded older JDK or JRE path. Use `where java` (Windows) or `which java` (Linux/macOS) to diagnose which executable is first in search order.

---

### Example 5: High-Performance / Modern Idiom — Compact Source Execution
- **Goal:** Leverage Java 25 Compact Source Files (JEP 512) allowing implicit classes and instance main methods for zero-boilerplate scripts.
```java
// CompactScript.java
void main() {
    IO.println("Compact Source File in Java 25: Hello, World!");
    IO.println("Available CPU Cores: " + Runtime.getRuntime().availableProcessors());
}
```
- **Console Output:**
```text
Compact Source File in Java 25: Hello, World!
Available CPU Cores: 16
```
- **Explanation:** Java 25 finalizes compact source files and instance main methods. Classes without explicit packaging or class declaration wrappers can be executed directly as scripts or micro-utilities.

---

## 6. Best Practices

1. **Pin JAVA_HOME in CI/CD:** Never rely on default system paths in GitHub Actions, GitLab CI, or Jenkins. Explicitly declare `JAVA_HOME` pointing to the target LTS distribution.
2. **Use Vendor-Neutral Distributions for Production:** Eclipse Temurin (Adoptium) is the industry standard for general-purpose server workloads. On AWS EC2 or ECS, prefer Amazon Corretto for vendor-optimized kernel and network patches.
3. **Use Minimal Container Images:** Never package a full JDK inside a production Docker container. Use a multi-stage Docker build with `jlink` or deploy on a minimal JRE/base image (such as `eclipse-temurin:25-jre-alpine` or distroless images) to minimize attack surface and image size.
4. **Verify Cryptographic Checksums:** Always verify the SHA-256 hash of downloaded JDK tarballs or installers before rolling out infrastructure updates.
5. **Set `-XX:+UseContainerSupport`:** Modern JVMs enable this by default, ensuring the JVM calculates heap limits based on Docker/cgroup memory limits rather than total host RAM.

---

## 7. Common Mistakes & Pitfalls

| Mistake | Why It Fails | Practical Remedy |
| :--- | :--- | :--- |
| **Installing JRE instead of JDK** | JRE lacks `javac`, `jcmd`, and diagnostics needed for builds and monitoring. | Install a full OpenJDK distribution (e.g., Eclipse Temurin 25). |
| **Trailing Slash in `JAVA_HOME`** | Scripts appending `/bin/java` produce `//bin/java`, causing path resolution failures on some tools. | Define `JAVA_HOME` without a trailing slash: `C:\Java\jdk-25` or `/opt/java/jdk-25`. |
| **Mismatched 32-bit vs 64-bit** | 32-bit JDKs cannot address more than 4 GB RAM and lack modern GC engines. | Always verify `os.arch` is `x86_64` or `aarch64`. |
| **Hardcoding Binary Paths** | Scripts breaking when JDK is upgraded or moved. | Always reference `$JAVA_HOME/bin/java` rather than `/usr/bin/java`. |
| **Leaving Old JDKs in PATH** | Multiple JDK installations cause the shell to select an obsolete version found earlier in `PATH`. | Ensure `%JAVA_HOME%\bin` is at the very beginning of the system `PATH`. |
| **Ignoring Preview Feature Flags** | Using preview features without `--enable-preview` causes compilation or runtime failure. | Explicitly pass `--enable-preview --release 25` to both `javac` and `java`. |

---

## 8. Performance & Security Notes

### Memory Sizing & Ergonomics
By default, the 64-bit HotSpot JVM allocates:
- **Initial Heap (`-Xms`):** Approximately 1/64th of physical RAM.
- **Maximum Heap (`-Xmx`):** Approximately 1/4th of physical RAM (up to 32 GB before Compressed OOPs threshold shifts).

### Container Awareness
In Kubernetes or Docker environments, HotSpot respects cgroup CPU quotas and memory limits. Always configure:
```bash
java -XX:MaxRAMPercentage=75.0 -XX:InitialRAMPercentage=50.0 -jar app.jar
```
This guarantees the JVM scales dynamically with container resource limits without hardcoding megabyte thresholds.

### Security Updates Lifecycle
OpenJDK releases quarterly Critical Patch Updates (CPU) in January, April, July, and October. Deploying these updates ensures protection against newly discovered cryptographic, TLS, and classloading vulnerabilities. Note that Java 24 permanently removed `SecurityManager` (JEP 486); application security must be enforced via OS-level sandboxing, containers, and secure coding practices.

---

## 9. Comparison / When to Use What

| Distribution | Vendor / Backer | License | Primary Strengths | Recommended Scenario |
| :--- | :--- | :--- | :--- | :--- |
| **Eclipse Temurin** | Eclipse Foundation / Adoptium | GPLv2 + Classpath Exception | Vendor-neutral, rigorous AQAvit testing, broad platform support | Default choice for cross-cloud enterprise microservices |
| **Amazon Corretto** | Amazon Web Services | GPLv2 + Classpath Exception | Performance optimizations for AWS Graviton (ARM64) and EC2 | Native workloads deployed on AWS EC2, ECS, or EKS |
| **Azul Zulu** | Azul Systems | GPLv2 + Classpath Exception | Extended commercial support, older architecture builds | Specialized legacy system support or customized SLAs |
| **Oracle JDK** | Oracle Corporation | Free Terms and Conditions (FTC) for production / OTN | Reference implementation, commercial Oracle support | Enterprise accounts with Oracle support contracts |
| **GraalVM** | Oracle / Open Source | GPLv2 / Community | Native AOT compilation (`native-image`), polyglot capabilities | Serverless Lambdas requiring sub-10ms startup time |

---

## 10. Interview Questions

<details>
<summary><strong>1. What is the fundamental difference between the JDK, JRE, and JVM?</strong></summary>

**Answer:**  
The **JVM (Java Virtual Machine)** is the runtime specification and execution engine that executes bytecode, manages heap memory, and handles garbage collection. The **JRE (Java Runtime Environment)** is an execution bundle containing the JVM plus the standard class libraries needed to run Java programs (deprecated and removed as a standalone download since Java 11). The **JDK (Java Development Kit)** is the complete SDK containing the JVM, standard libraries, and development tools (`javac`, `jshell`, `jar`, `jcmd`, `jlink`).
</details>

<details>
<summary><strong>2. How does the JVM launcher dynamically load and initialize the HotSpot engine?</strong></summary>

**Answer:**  
The native binary (`java` or `java.exe`) parses command-line arguments and uses dynamic linking (`dlopen` on UNIX, `LoadLibrary` on Windows) to load `libjvm.so` or `jvm.dll` located in the `server/` directory. It invokes the exported JNI function `JNI_CreateJavaVM(&jvm, &env, &vm_args)`. This initializes the memory subsystems, class loaders, and HotSpot JIT compilers, then locates the main entry point via reflection or native invocation.
</details>

<details>
<summary><strong>3. What is class file versioning, and why does `UnsupportedClassVersionError` occur?</strong></summary>

**Answer:**  
Every `.class` file begins with a 4-byte magic number (`0xCAFEBABE`) followed by minor and major version numbers. Java 8 is major version 52, Java 17 is 61, Java 21 is 65, and Java 25 is 69. When a newer compiler builds a class file with a major version higher than what the executing JVM supports, the JVM class verifier rejects the class and throws `java.lang.UnsupportedClassVersionError`.
</details>

<details>
<summary><strong>4. How did Java 9 modularization change the structure and deployment of the JDK?</strong></summary>

**Answer:**  
Prior to Java 9, the runtime was packaged as a monolithic `rt.jar` containing all core classes. Project Jigsaw (JPMS) decomposed the JDK into separate modules (`java.base`, `java.sql`, `java.desktop`, etc.). The monolithic JRE was eliminated, and `jlink` was introduced, allowing developers to create custom runtime images containing only the modules their application uses, reducing runtime image size from 300+ MB to under 40 MB.
</details>

<details>
<summary><strong>5. What is the purpose of `JAVA_HOME`, and how does it differ from the system `PATH`?</strong></summary>

**Answer:**  
`JAVA_HOME` is an environment variable pointing to the root directory of the JDK installation. Build tools like Maven and Gradle, application servers, and IDEs inspect `JAVA_HOME` to locate JDK resources like compiler libraries and header files. `PATH` is an operating system variable containing a list of directories searched by the shell to find executable binaries. Setting `PATH` to include `$JAVA_HOME/bin` ensures the terminal executes the intended `java` and `javac` commands.
</details>

<details>
<summary><strong>6. What is the impact of `-XX:+UseContainerSupport` on modern containerized applications?</strong></summary>

**Answer:**  
Inside Linux containers (Docker, Kubernetes), the JVM previously read memory and CPU metrics from `/proc/meminfo` and `/proc/cpuinfo`, which reflect host hardware rather than container cgroup limits. This frequently led to the Linux Out-Of-Memory (OOM) killer terminating the container. With `-XX:+UseContainerSupport` (enabled by default in modern JDKs), the JVM reads memory limits from `/sys/fs/cgroup`, allowing ergonomic heap sizing (`-XX:MaxRAMPercentage`) to function properly.
</details>

<details>
<summary><strong>7. How does single-file source-code execution work under the hood?</strong></summary>

**Answer:**  
Introduced in JEP 330 (Java 11), when the launcher is invoked as `java File.java`, it detects the `.java` extension and delegates to the in-memory compiler. The compiler compiles the source file into an in-memory class representation without writing a `.class` file to disk. The launcher then defines the class in a custom ClassLoader and invokes the main method.
</details>

<details>
<summary><strong>8. Why was `SecurityManager` permanently removed in Java 24?</strong></summary>

**Answer:**  
JEP 486 permanently removed `SecurityManager` because it was rarely used in modern deployments, incurred significant performance overhead on core method calls, was brittle and complex to configure correctly, and provided a false sense of security. Modern security boundaries are enforced using OS processes, containers, hypervisors, and strict network policies rather than JVM-internal permission checks.
</details>

---

## 11. Practice Exercises

### Exercise 1: Programmatic Toolchain Validation (Easy)
- **Task:** Write a standalone Java program that inspects system properties and verifies that the current runtime environment is running Java 25 or higher. If running on an older version, terminate with exit code 1.
<details>
<summary>Solution</summary>

```java
public class VersionCheck {
    public static void main(String[] args) {
        int feature = Runtime.version().feature();
        System.out.println("Detected Java Feature Version: " + feature);
        if (feature < 25) {
            System.err.println("ERROR: Requires Java 25 or higher. Found: " + feature);
            System.exit(1);
        }
        System.out.println("SUCCESS: Java 25+ toolchain verified.");
    }
}
```
</details>

### Exercise 2: Operating System & Memory Inspection (Medium)
- **Task:** Write a diagnostic utility that queries `java.lang.management.OperatingSystemMXBean` and prints the operating system architecture, available processors, system load average, and committed JVM memory in megabytes.
<details>
<summary>Solution</summary>

```java
import java.lang.management.ManagementFactory;
import java.lang.management.OperatingSystemMXBean;
import java.lang.management.MemoryMXBean;

public class SystemProbe {
    public static void main(String[] args) {
        OperatingSystemMXBean os = ManagementFactory.getOperatingSystemMXBean();
        MemoryMXBean mem = ManagementFactory.getMemoryMXBean();

        System.out.println("OS Name:       " + os.getName());
        System.out.println("OS Arch:       " + os.getArch());
        System.out.println("OS Version:    " + os.getVersion());
        System.out.println("Logical Cores: " + os.getAvailableProcessors());
        System.out.println("Committed Heap: " + 
            (mem.getHeapMemoryUsage().getCommitted() / (1024 * 1024)) + " MB");
    }
}
```
</details>

### Exercise 3: Cross-Platform Environment Variable Analyzer (Hard)
- **Task:** Implement a Java utility that validates the host environment: check that `JAVA_HOME` exists, that it points to a valid directory containing a `bin/java` (or `bin/java.exe`) file, and that the binary version matches the running JVM version.
<details>
<summary>Solution</summary>

```java
import java.io.File;
import java.nio.file.Files;
import java.nio.file.Path;

public class EnvironmentValidator {
    public static void main(String[] args) {
        String javaHome = System.getenv("JAVA_HOME");
        if (javaHome == null || javaHome.isBlank()) {
            System.err.println("FAILURE: JAVA_HOME is not defined in environment variables.");
            return;
        }

        Path homePath = Path.of(javaHome);
        if (!Files.isDirectory(homePath)) {
            System.err.println("FAILURE: JAVA_HOME does not point to a directory: " + javaHome);
            return;
        }

        boolean isWindows = System.getProperty("os.name").toLowerCase().contains("win");
        Path javaBin = homePath.resolve("bin").resolve(isWindows ? "java.exe" : "java");

        if (!Files.isExecutable(javaBin)) {
            System.err.println("FAILURE: Java binary not found or not executable at: " + javaBin);
            return;
        }

        System.out.println("SUCCESS: Validated JAVA_HOME: " + javaHome);
        System.out.println("SUCCESS: Binary verified at: " + javaBin);
    }
}
```
</details>

---

## 12. Summary / Cheat Sheet

- **Core Rule:** The JDK contains the compiler (`javac`), runtime (`java`), standard library (`java.base`), and diagnostic utilities (`jcmd`, `jshell`, `jlink`).
- **LTS Strategy:** Target **Java 25 LTS** for all modern enterprise greenfield development and migrations.
- **Variables:** Always configure `JAVA_HOME` pointing to the JDK root and `%JAVA_HOME%\bin` in `PATH`.
- **Diagnostic Commands:**
  - `java --version`: Displays JVM runtime build, vendor, and VM mode.
  - `javac --version`: Displays compiler version.
  - `jcmd <pid> VM.version`: Inspects runtime version of an active background process.
  - `jcmd <pid> VM.system_properties`: Prints all system properties of a running JVM.

---

## 13. Further Reading

- [OpenJDK Quality Group Guidelines](https://wiki.openjdk.org/display/quality/Quality+Outreach)
- [Adoptium / Eclipse Temurin Project Documentation](https://adoptium.net/)
- [JEP 330: Launch Single-File Source-Code Programs](https://openjdk.org/jeps/330)
- [JEP 486: Permanently Disable the Security Manager](https://openjdk.org/jeps/486)
- [JEP 512: Compact Source Files and Instance Main Methods](https://openjdk.org/jeps/512)

---

| [Section Index](file:///E:/Learning/engineering-notes/01-languages/java/00-setup/README.md) | [Next: 01_java-version-management.md](file:///E:/Learning/engineering-notes/01-languages/java/00-setup/01_java-version-management.md) → |
