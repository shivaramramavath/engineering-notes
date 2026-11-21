# Java Version Management

> Version management is the practice of maintaining multiple parallel Java Development Kits on a single development machine, dynamically switching the active runtime per terminal session or per project, and decoupling compile-time toolchains from runtime environments.

| Metadata | Value |
| :--- | :--- |
| **Level** | Beginner |
| **Prerequisites** | [00_jdk-installation.md](file:///E:/Learning/engineering-notes/01-languages/java/00-setup/00_jdk-installation.md) |
| **Time to Read** | 25 minutes |
| **Status** | Complete |
| **Java LTS Baseline** | Java 25 LTS (with management strategies for Java 8, 11, 17, 21) |

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
In professional software engineering, developers rarely work on a single, isolated project. A typical workflow involves maintaining legacy production systems built on Java 11 or 17, developing contemporary microservices on Java 21, and experimenting with cutting-edge capabilities on Java 25. Manually editing system-wide environment variables (`JAVA_HOME` and `PATH`) every time you switch repositories is slow, error-prone, and causes cross-project contamination.

### Historical Context
- **Pre-2015:** Java releases were spaced 3–5 years apart (Java 6 to 7 took 5 years; Java 7 to 8 took 3 years). Developers often had a single global JDK installed.
- **Six-Month Release Cadence (Java 9+):** With OpenJDK releasing a new feature version every March and September and an LTS every two years, multiple active JDK versions became the standard operating procedure.
- **Modern Solution Space:** Tools like **SDKMAN!** (POSIX/WSL), **Scoop/Winget** (Windows), and **Maven Toolchains** emerged to automate version discovery, directory switching, and project isolation.

### Real-World Relevance
Version drift causes critical CI/CD build failures and runtime bugs. Understanding how to manage multiple JDKs ensures that local builds replicate production environments exactly, prevents accidental bytecode incompatibility, and allows teams to test new LTS features without destabilizing existing services.

---

## 2. Core Concepts

### LTS vs. Non-LTS Releases
OpenJDK releases follow a predictable release lifecycle:
- **Feature Releases (Non-LTS):** Released every 6 months (e.g., Java 22, 23, 24, 26). Supported for only 6 months until the next release. Designed for early adoption, feedback, and short-lived experimental workloads.
- **Long-Term Support (LTS):** Designated every 2 years (Java 8, 11, 17, 21, 25). Vendor distributions provide 5 to 10+ years of enterprise security patches, bug fixes, and stability updates.

### The Version Management Tooling Ecosystem
1. **SDKMAN! (Software Development Kit Manager):** The premier CLI tool for UNIX-like environments (Linux, macOS, Windows WSL). Manages parallel candidate installations for Java, Maven, Gradle, Micronaut, and more.
2. **Directory-Based Auto-Switching (`.sdkmanrc`):** A project-level configuration file that automatically switches the terminal's JDK when navigating into the repository directory.
3. **Windows Shims & Environment Switchers (Scoop / PowerShell):** Windows package managers maintain symbolic links or shims pointing to the active candidate directory.
4. **Build Toolchains (`toolchains.xml`):** Decouples the JDK running the build tool (e.g., Maven running on Java 25) from the compiler JDK used to compile the application code (e.g., compiling against JDK 17).

---

## 3. How It Works Internally

### Directory Architecture & Symbolic Link Resolution
Version managers avoid copying massive binary directories by utilizing filesystem **symbolic links (symlinks)** or execution **shims**.

```
~/.sdkman/candidates/java/
├── 17.0.12-tem/                # Actual physical JDK 17 installation
├── 21.0.4-tem/                 # Actual physical JDK 21 installation
├── 25.0.1-tem/                 # Actual physical JDK 25 installation
└── current ──────────────────► points to 25.0.1-tem (Symlink)
        │
        ▼
   PATH=$HOME/.sdkman/candidates/java/current/bin:$PATH
   JAVA_HOME=$HOME/.sdkman/candidates/java/current
```

When you issue `sdk use java 21.0.4-tem`:
1. **Subshell Environment Update:** SDKMAN! alters the `JAVA_HOME` and `PATH` environment variables for your active shell process only.
2. **Symlink Re-pointing (for `sdk default`):** When changing the default, it atomically re-points the `current` symlink to target the new directory (`21.0.4-tem`), immediately changing the global baseline without file duplication.

### Windows Shims vs. Dynamic PowerShell Modules
On native Windows without WSL, Scoop creates a lightweight executable wrapper (`shim`) inside `~/scoop/shims/java.exe`. When invoked, the shim reads a configuration file and executes the target JDK binary. Alternatively, PowerShell helper functions dynamically manipulate `$env:JAVA_HOME` and `$env:Path` within the current session.

---

## 4. Syntax & Usage

### SDKMAN! (Linux, macOS, WSL)

```bash
# 1. Install SDKMAN!
curl -s "https://get.sdkman.io" | bash
source "$HOME/.sdkman/bin/sdkman-init.sh"

# 2. List available Java distributions and versions
sdk list java

# 3. Install specific LTS versions
sdk install java 25.0.1-tem
sdk install java 21.0.4-tem
sdk install java 17.0.12-tem

# 4. Switch version for the current shell session only
sdk use java 21.0.4-tem

# 5. Set the system-wide default version
sdk default java 25.0.1-tem
```

### Automatic Directory Switching with `.sdkmanrc`
Place a `.sdkmanrc` file in the root of your project repository:

```properties
# .sdkmanrc
java=21.0.4-tem
```

Enable auto-switching in `~/.sdkman/etc/config`:
```properties
sdkman_auto_env=true
```
When you `cd` into the project directory, SDKMAN! automatically executes `sdk use java 21.0.4-tem`. When you leave the folder, it restores your default JDK.

### Windows PowerShell Multi-JDK Switching Script
On Windows, you can add this function to your PowerShell Profile (`notepad $PROFILE`):

```powershell
function Set-JavaVersion {
    param(
        [Parameter(Mandatory=$true)]
        [ValidateSet("17", "21", "25")]
        [string]$Version
    )

    $JdkMap = @{
        "17" = "C:\Program Files\Java\jdk-17"
        "21" = "C:\Program Files\Java\jdk-21"
        "25" = "C:\Program Files\Java\jdk-25"
    }

    $TargetDir = $JdkMap[$Version]
    if (Test-Path $TargetDir) {
        $env:JAVA_HOME = $TargetDir
        $env:Path = "$TargetDir\bin;" + ($env:Path -replace [regex]::Escape("$TargetDir\bin;"), "")
        Write-Host "Switched to Java $Version ($TargetDir)" -ForegroundColor Green
        java --version
    } else {
        Write-Error "JDK path not found: $TargetDir"
    }
}
```

---

## 5. Practical Examples

### Example 1: Basic Idiomatic Usage — Runtime Version Matrix Detector
- **Goal:** Programmatically validate the running JVM against a predefined matrix of supported enterprise LTS baselines.
```java
public class VersionMatrixValidator {
    public record JavaRelease(int feature, String status, boolean isLts) {}

    public static void main(String[] args) {
        var matrix = java.util.List.of(
            new JavaRelease(8, "Legacy", true),
            new JavaRelease(11, "Legacy LTS", true),
            new JavaRelease(17, "Enterprise LTS", true),
            new JavaRelease(21, "Modern LTS", true),
            new JavaRelease(25, "Current Baseline LTS", true)
        );

        int currentFeature = Runtime.version().feature();
        System.out.println("Running on Feature: " + currentFeature);

        matrix.stream()
            .filter(r -> r.feature() == currentFeature)
            .findFirst()
            .ifPresentOrElse(
                r -> System.out.println("Active Environment: " + r.status() + " (LTS: " + r.isLts() + ")"),
                () -> System.out.println("Non-standard or intermediate release")
            );
    }
}
```
- **Console Output:**
```text
Running on Feature: 25
Active Environment: Current Baseline LTS (LTS: true)
```
- **Explanation:** Demonstrates querying `Runtime.version().feature()` and matching it with an immutable `record` model to enforce version compatibility assertions at startup.

---

### Example 2: Real-World Enterprise Scenario — Maven Toolchains Configuration
- **Goal:** Decouple the JDK used to execute Maven from the JDK used to compile the application, allowing Maven running on Java 25 to safely compile a Java 17 service.
- **Configuration File:** `~/.m2/toolchains.xml`
```xml
<?xml version="1.0" encoding="UTF-8"?>
<toolchains xmlns="http://maven.apache.org/TOOLCHAINS/1.1.0"
            xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance"
            xsi:schemaLocation="http://maven.apache.org/TOOLCHAINS/1.1.0 
            http://maven.apache.org/xsd/toolchains-1.1.0.xsd">
    <toolchain>
        <type>jdk</type>
        <provides>
            <version>17</version>
            <vendor>temurin</vendor>
        </provides>
        <configuration>
            <jdkHome>/usr/lib/jvm/temurin-17</jdkHome>
        </configuration>
    </toolchain>
    <toolchain>
        <type>jdk</type>
        <provides>
            <version>25</version>
            <vendor>oracle</vendor>
        </provides>
        <configuration>
            <jdkHome>C:\Program Files\Java\jdk-25</jdkHome>
        </configuration>
    </toolchain>
</toolchains>
```
- **Project Configuration (`pom.xml`):**
```xml
<plugin>
    <groupId>org.apache.maven.plugins</groupId>
    <artifactId>maven-toolchains-plugin</artifactId>
    <version>3.2.0</version>
    <executions>
        <execution>
            <goals><goal>toolchain</goal></goals>
        </execution>
    </executions>
    <configuration>
        <toolchains>
            <jdk>
                <version>17</version>
                <vendor>temurin</vendor>
            </jdk>
        </toolchains>
    </configuration>
</plugin>
```
- **Explanation:** This guarantees that even if a developer runs `mvn package` using Java 25, the Maven Toolchain plugin intercepts compilation and forces execution using the verified JDK 17 compiler and standard libraries.

---

### Example 3: Edge Case — Handling Bytecode Cross-Compilation via `--release`
- **Goal:** Compile code for older Java versions without installing an older JDK, using the compiler's built-in historical API signatures (`--release` flag).
```bash
# Using JDK 25 compiler to produce Java 17 compatible bytecode:
$ javac --release 17 LegacyService.java
```
- **Explanation:** Unlike the legacy `-source 17 -target 17` flags (which compiled against the *current* JDK's standard library, causing runtime `NoSuchMethodError` when calling APIs that changed), `--release 17` automatically configures the compiler against the historical Java 17 symbol database bundled in `ct.sym` inside the JDK.

---

### Example 4: Failure Case — Cross-Compilation Trap with `-target` vs `--release`
- **Goal:** Demonstrate why `-source` and `-target` fail silently where `--release` succeeds.
```java
// ModernCall.java
import java.nio.ByteBuffer;

public class ModernCall {
    public static void main(String[] args) {
        ByteBuffer buffer = ByteBuffer.allocate(10);
        buffer.position(5); // In Java 9+, returns ByteBuffer; in Java 8, returns Buffer
    }
}
```
- **Execution & Pitfall:**
```bash
# Compiling with JDK 25 using legacy flags:
$ javac -source 8 -target 8 ModernCall.java
# WARNING: -source 8 is obsolete and will be removed in a future release.
# Code compiles, but at runtime on Java 8 JRE:
# java.lang.NoSuchMethodError: java.nio.ByteBuffer.position(I)Ljava/nio/ByteBuffer;
```
- **Remedy:** Always use `--release 8` or `--release 17`. This ensures the compiler links against the historical class definitions, preventing binary incompatibilities.

---

### Example 5: High-Performance / Modern Idiom — Multi-Release JAR (MRJAR) Probe
- **Goal:** Inspect how modern Java runtimes selectively load version-specific class implementations from a Multi-Release JAR (MRJAR).
```java
public class MultiReleaseProbe {
    public static void main(String[] args) {
        // Multi-Release JAR support is transparent to the caller
        System.out.println("Runtime Version: " + Runtime.version().feature());
        System.out.println("Class Location:  " + 
            MultiReleaseProbe.class.getProtectionDomain().getCodeSource().getLocation());
    }
}
```
- **Console Output:**
```text
Runtime Version: 25
Class Location:  file:/C:/Users/RAMAVATH SHIVA RAM/.gemini/antigravity/brain/a9bfc182-e985-4a5d-9df0-ebf568360240/scratch/verification/
```
- **Explanation:** MRJARs store standard bytecode under the root directory and version-specific enhancements under `META-INF/versions/21/` or `META-INF/versions/25/`. The JVM's ClassLoader automatically selects the highest versioned class file supported by the executing JVM.

---

## 6. Best Practices

1. **Commit `.sdkmanrc` to Version Control:** Include `.sdkmanrc` in every repository so that team members and CI runners immediately know the exact tested JDK distribution.
2. **Standardize on LTS for Repositories:** Standardize team repos on the latest LTS (Java 25) or previous LTS (Java 21, 17); avoid building production release artifacts on short-lived intermediate versions (Java 22, 23, 24).
3. **Use `--release` for Target Specification:** Never use `-source` and `-target` in Maven or Gradle builds. Configure `<maven.compiler.release>21</maven.compiler.release>` to prevent accidental usage of newer standard library methods.
4. **Isolate CI Agent JDKs:** In Dockerized CI runners, run the build inside a dedicated container image containing the specific target JDK (e.g., `eclipse-temurin:21-jdk`) rather than switching versions dynamically during the build step.

---

## 7. Common Mistakes & Pitfalls

| Mistake | Why It Fails | Practical Remedy |
| :--- | :--- | :--- |
| **Using `-source`/`-target` instead of `--release`** | Compiles against current JDK runtime classes, causing runtime `NoSuchMethodError` on older JVMs. | Always use `--release <version>`. |
| **IDE vs. Terminal JDK Mismatch** | IntelliJ or Eclipse uses its internal JDK, masking terminal build failures. | Configure Project SDK in Project Structure to match `.sdkmanrc` / terminal JDK. |
| **Global Path Mutation on Windows** | Modifying system-level PATH breaks background services expecting a specific Java version. | Use session-level PowerShell scripts or Scoop shims rather than global environment changes. |
| **Ignoring `.sdkmanrc` in Nested Folders** | SDKMAN! only triggers on directory entry; launching a build from a root folder may use the wrong version. | Navigate to the repository root where `.sdkmanrc` is located before invoking builds. |
| **Unset `JAVA_HOME` with Active `PATH`** | Some tools (like Gradle) inspect `JAVA_HOME` first; if mismatched with `PATH`, unpredictable builds occur. | Ensure both `JAVA_HOME` and `PATH` are updated in tandem. |
| **Running JIT Benchmarks on Cold JDK Switch** | Swapping JDK versions without clearing local build caches (`target/` or `.gradle/`) leads to poisoned class caches. | Always execute `clean` (`mvn clean` or `gradle clean`) after switching JDK versions. |

---

## 8. Performance & Security Notes

### Security Implications of Outdated JDKs
Running an unpatched Java 8 or Java 11 runtime exposes systems to known CVEs in cryptographic algorithms, XML parsers, and TLS negotiation. Using a version manager enables automated quarterly updates:
```bash
sdk upgrade java
```
This single command pulls the latest quarterly security patch release across all installed candidate distributions.

### Memory Overhead of Multiple Installations
Multiple JDK installations only consume disk space (typically 300–400 MB per distribution). Because each JVM process runs independently, having multiple installed versions incurs **zero** runtime memory or CPU performance penalty.

---

## 9. Comparison / When to Use What

| Management Strategy | Operating System | Pros | Cons | Best Suited For |
| :--- | :--- | :--- | :--- | :--- |
| **SDKMAN!** | Linux, macOS, WSL | Comprehensive CLI, auto-environment switching, manages Maven/Gradle too | Requires bash/zsh shell environment | All UNIX/Linux and WSL development environments |
| **Scoop / Winget** | Windows | Native Windows support, clean uninstalls, shim-based execution | Limited auto-switching based on directory | Native Windows development without WSL |
| **Maven Toolchains** | All (OS Agnostic) | Enforces strict compiler versions in build configs across all team members | Requires local XML configuration file setup | Enterprise multi-JDK CI/CD and large monorepos |
| **Docker Multi-Stage** | All (Containers) | 100% environment reproducibility, zero host machine contamination | Slower local feedback loops compared to native execution | CI/CD pipelines, containerized deployments |

---

## 10. Interview Questions

<details>
<summary><strong>1. What is the fundamental difference between compiling with `-source 17 -target 17` versus `--release 17`?</strong></summary>

**Answer:**  
`-source 17 -target 17` instructs the compiler to produce bytecode conforming to Java 17 format, but it compiles against the *current* JDK's standard library (`rt.jar` or modern JMODs). If the code calls a method added in Java 21, compilation succeeds, but the application will crash at runtime with `NoSuchMethodError` when executed on a real Java 17 JVM. The `--release 17` flag (introduced in Java 9 via JEP 247) configures the compiler to use historical API symbol tables (`ct.sym`), ensuring that both bytecode and API usage strictly match Java 17.
</details>

<details>
<summary><strong>2. How does SDKMAN! achieve session-level version switching without root permissions?</strong></summary>

**Answer:**  
SDKMAN! is implemented entirely as shell functions sourced into the user's active shell (`~/.bashrc` or `~/.zshrc`). When `sdk use java <version>` is executed, it updates the `JAVA_HOME` and `PATH` shell environment variables for that specific shell process and its child processes only. Because environment variables are process-local in POSIX operating systems, other open terminal windows and system daemons remain completely unaffected.
</details>

<details>
<summary><strong>3. What are Maven Toolchains, and why are they used in enterprise builds?</strong></summary>

**Answer:**  
Maven Toolchains allow a build to decouple the JDK used to run Maven itself from the JDK used to compile and test the project code. For example, Maven can run on a high-performance modern JDK (Java 25) with virtual threads enabled, while compiling, running tests, and generating bytecode strictly using a target JDK 11 installation defined in `~/.m2/toolchains.xml`.
</details>

<details>
<summary><strong>4. What is a Multi-Release JAR (MRJAR), and how does the JVM handle it?</strong></summary>

**Answer:**  
Introduced in JEP 238 (Java 9), an MRJAR allows library maintainers to bundle multiple versions of a class file within a single `.jar` archive. The base classes live in the root of the JAR (e.g., compiled for Java 8), while version-specific optimized classes live under `META-INF/versions/17/`, `META-INF/versions/21/`, etc. When running on Java 25, the JVM's ClassLoader automatically searches the highest matching version directory first, enabling libraries to utilize new language features without dropping backward compatibility.
</details>

<details>
<summary><strong>5. Why is `.sdkmanrc` crucial for microservice repositories?</strong></summary>

**Answer:**  
In microservice architectures, different services often migrate to newer Java LTS releases at different speeds. Committing a `.sdkmanrc` file into each repository repository ensures that whenever a developer or build script enters the directory, the active shell immediately switches to the exact validated JDK version for that service, preventing accidental cross-compilation errors.
</details>

<details>
<summary><strong>6. How do symbolic links facilitate atomic default JDK changes?</strong></summary>

**Answer:**  
Rather than altering system path strings across multiple files, version managers maintain a symlink named `current` pointing to the desired physical JDK folder. Updating the default version is an atomic filesystem operation: the manager recreates the symlink to target the new folder. All tools referencing `$HOME/.sdkman/candidates/java/current` immediately reflect the change across all future processes.
</details>

<details>
<summary><strong>7. What happens if a system has multiple JDKs on `PATH`?</strong></summary>

**Answer:**  
Operating system shells search the directories listed in `PATH` sequentially from left to right. The first directory containing an executable matching the requested name (`java` or `javac`) is executed. If an older JDK appears earlier in `PATH` than `$JAVA_HOME/bin`, the shell will invoke the older binary, causing version desynchronization.
</details>

<details>
<summary><strong>8. What is the Long-Term Support (LTS) cadence established by Oracle and the OpenJDK community?</strong></summary>

**Answer:**  
OpenJDK releases a new feature release every 6 months (March and September). Starting with Java 17, the LTS cadence was shortened from 3 years to every 2 years. Consequently, Java 17, Java 21, and Java 25 are official enterprise LTS releases, while intermediate versions (22, 23, 24) are non-LTS releases supported for 6 months each.
</details>

---

## 11. Practice Exercises

### Exercise 1: Version Switcher Verification Utility (Easy)
- **Task:** Write a Java program that validates whether the active JVM feature version matches an expected target version passed as a command-line argument.
<details>
<summary>Solution</summary>

```java
public class VersionAssertion {
    public static void main(String[] args) {
        if (args.length == 0) {
            System.err.println("Usage: java VersionAssertion <expected-feature-version>");
            System.exit(1);
        }

        int expected = Integer.parseInt(args[0]);
        int actual = Runtime.version().feature();

        if (expected == actual) {
            System.out.printf("SUCCESS: Active Java version (%d) matches expected (%d).%n", actual, expected);
        } else {
            System.err.printf("ERROR: Version mismatch! Expected: %d, but running on: %d%n", expected, actual);
            System.exit(2);
        }
    }
}
```
</details>

### Exercise 2: Automated `.sdkmanrc` Detector and Validator (Medium)
- **Task:** Create a program that searches the current directory for a `.sdkmanrc` file, extracts the configured `java` version, and verifies if the running JVM feature version matches the candidate string.
<details>
<summary>Solution</summary>

```java
import java.io.IOException;
import java.nio.file.Files;
import java.nio.file.Path;
import java.util.Properties;

public class SdkmanrcValidator {
    public static void main(String[] args) {
        Path rcPath = Path.of(".sdkmanrc");
        if (!Files.exists(rcPath)) {
            System.out.println("No .sdkmanrc file present in current working directory.");
            return;
        }

        Properties props = new Properties();
        try (var reader = Files.newBufferedReader(rcPath)) {
            props.load(reader);
            String javaCandidate = props.getProperty("java");
            System.out.println("Found .sdkmanrc candidate: " + javaCandidate);

            int currentFeature = Runtime.version().feature();
            System.out.println("Running JVM Feature: " + currentFeature);

            if (javaCandidate != null && javaCandidate.startsWith(String.valueOf(currentFeature))) {
                System.out.println("SUCCESS: Active JVM aligns with .sdkmanrc configuration.");
            } else {
                System.out.println("WARNING: Active JVM differs from .sdkmanrc specification!");
            }
        } catch (IOException e) {
            System.err.println("Failed to read .sdkmanrc: " + e.getMessage());
        }
    }
}
```
</details>

### Exercise 3: Cross-Version Bytecode Target Inspector (Hard)
- **Task:** Implement a Java utility that reads a `.class` file's binary header from disk, parses the 4-byte magic number (`0xCAFEBABE`) and major version number, and outputs the corresponding Java release baseline.
<details>
<summary>Solution</summary>

```java
import java.io.DataInputStream;
import java.io.FileInputStream;
import java.io.IOException;

public class ClassVersionInspector {
    public static void main(String[] args) {
        if (args.length == 0) {
            System.err.println("Usage: java ClassVersionInspector <path-to-class-file>");
            return;
        }

        try (var dis = new DataInputStream(new FileInputStream(args[0]))) {
            int magic = dis.readInt();
            if (magic != 0xCAFEBABE) {
                System.err.println("Invalid class file: magic header is not 0xCAFEBABE");
                return;
            }

            int minor = dis.readUnsignedShort();
            int major = dis.readUnsignedShort();

            String javaVersion = switch (major) {
                case 52 -> "Java 8";
                case 55 -> "Java 11";
                case 61 -> "Java 17 (LTS)";
                case 65 -> "Java 21 (LTS)";
                case 69 -> "Java 25 (LTS)";
                default -> "Major version " + major;
            };

            System.out.printf("Class File: %s%n", args[0]);
            System.out.printf("Bytecode Version: %d.%d (%s)%n", major, minor, javaVersion);
        } catch (IOException e) {
            System.err.println("Error reading class file: " + e.getMessage());
        }
    }
}
```
</details>

---

## 12. Summary / Cheat Sheet

- **LTS Roadmap:** Java 8 $\to$ Java 11 $\to$ Java 17 $\to$ Java 21 $\to$ **Java 25 (Current LTS)**.
- **SDKMAN! Commands:**
  - `sdk list java`: View all available versions and distributions.
  - `sdk install java <id>`: Download and install a specific JDK.
  - `sdk use java <id>`: Switch version for the active terminal session.
  - `sdk default java <id>`: Set the global baseline JDK.
- **Compilation Safety:** Always use `javac --release <version>` or `<maven.compiler.release>` to prevent accidental usage of newer standard library APIs.
- **Project Isolation:** Place `.sdkmanrc` in repository roots to automate version switching.

---

## 13. Further Reading

- [SDKMAN! Official Documentation](https://sdkman.io/)
- [JEP 247: Compile for Older Platform Versions (`--release`)](https://openjdk.org/jeps/247)
- [JEP 238: Multi-Release JAR Files](https://openjdk.org/jeps/238)
- [Apache Maven Toolchains Plugin Guide](https://maven.apache.org/plugins/maven-toolchains-plugin/)
- [OpenJDK Release Cadence & Support Roadmaps](https://www.oracle.com/java/technologies/java-se-support-roadmap.html)

---

| [← Previous: 00_jdk-installation.md](file:///E:/Learning/engineering-notes/01-languages/java/00-setup/00_jdk-installation.md) | [Section Index](file:///E:/Learning/engineering-notes/01-languages/java/00-setup/README.md) | [Next: 02_how-java-works.md](file:///E:/Learning/engineering-notes/01-languages/java/00-setup/02_how-java-works.md) → |
