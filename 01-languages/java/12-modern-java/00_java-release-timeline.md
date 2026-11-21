# Java Release Timeline and Cadence

> Comprehensive architectural analysis of Java's modern 6-month release cadence, Long-Term Support (LTS) roadmap from Java 8 through Java 25 LTS, JEP incubation/preview lifecycle, and enterprise migration strategies.

| Metadata | Value |
|---|---|
| **Level** | Intermediate |
| **Prerequisites** | [00-setup/00_installing-jdk.md](../00-setup/00_installing-jdk.md), [05-packages-and-modules/02_java-modules.md](../05-packages-and-modules/02_java-modules.md) |
| **Time to Read** | 25 minutes |
| **Status** | Active |
| **Java Version** | Java 8 to Java 25 LTS (Oracle JDK 25 LTS baseline) |

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

### The Legacy Release Dilemma
Historically, Java releases arrived on irregular, multi-year cycles. Java 6 launched in 2006, Java 7 in 2011 (5 years), and Java 8 in 2014 (3 years). Major releases carried massive backlogs of deferred features. If an ambitious feature missed the release window, it was delayed for half a decade. This sluggish cadence hindered Java's ability to compete with rapidly iterating ecosystems like Go, Rust, and Node.js.

### The Six-Month Modern Cadence
Starting after Java 9 (March 2018), Oracle and the OpenJDK governing board transitioned to a **strict, time-driven six-month release cadence** (every March and September). Features ship when they are ready. If a feature misses a ship date, it simply rolls into the next release six months later.

### The LTS Model
To balance rapid developer innovation with enterprise stability requirements, specific releases are designated as **Long-Term Support (LTS)**:
- **Java 8 LTS** (March 2014) — Extended community support.
- **Java 11 LTS** (September 2018) — First modern modular LTS.
- **Java 17 LTS** (September 2021) — 3-year cadence LTS.
- **Java 21 LTS** (September 2023) — Cadence shifted to every **2 years** for LTS releases.
- **Java 25 LTS** (September 2025) — Current enterprise standard baseline.

---

## 2. Core Concepts

### JEP (JDK Enhancement Proposal)
A JEP is the formal mechanism for proposing, drafting, reviewing, and tracking significant enhancements to the JDK. Every major feature in modern Java is assigned a permanent JEP number (e.g., JEP 444 for Virtual Threads).

### Feature Lifecycle Stages
1. **Draft / Candidate**: Proposed concept undergoing design discussions on OpenJDK mailing lists.
2. **Incubator Module (`jdk.incubator.*`)**: Experimental APIs packaged in isolated non-standard modules to solicit feedback (e.g., Vector API).
3. **Preview Feature (`--enable-preview`)**: Fully specified, fully implemented compiler and JVM features shipped in standard packages, but requiring explicit opt-in flags. Preview features can evolve or change based on community feedback.
4. **Final / Permanent Feature**: Standardized, permanent part of the Java SE platform specification with backward compatibility guarantees.
5. **Deprecated for Removal**: APIs flagged with `@Deprecated(forRemoval = true)` slated for permanent deletion in upcoming releases.

### LTS vs Non-LTS Feature Releases
- **Feature Releases (e.g., 22, 23, 24)**: Supported for 6 months until the subsequent release. Perfect for early adoption, experimentation, and modern containerized services that upgrade frequently.
- **LTS Releases (e.g., 17, 21, 25)**: Backed by multi-year commercial and community security patches (Oracle, Adoptium/Eclipse Temurin, Amazon Corretto, Red Hat, Azul Zulu).

---

## 3. How It Works Internally

### JDK Release Pipeline & Branching
```
OpenJDK Mainline (Tip of Trunk)
  │
  ├────── Fork JDK 24 (Dec 2024) ─── Rampdown Phase 1/2 ─── GA (March 2025)
  │
  ├────── Fork JDK 25 LTS (June 2025) ─ Stabilization ───── GA (Sept 2025)
  │
  ▼
OpenJDK Mainline (Development for JDK 26+)
```

### The `--enable-preview` Flag Architecture
Preview features alter bytecode verification and class file versioning:
- Standard class files compiled for Java 25 have major version `69.0`.
- Preview class files have major version `69.65535` (minor version `65535`).
- The JVM refuses to load a class compiled with minor version `65535` unless the JVM itself is launched with the `--enable-preview` command-line argument. This prevents accidental deployment of unstable preview bytecodes into production runtimes.

---

## 4. Syntax & Usage

### Compiling and Running with Preview Features
```bash
# Compiling Java 25 preview features
javac --release 25 --enable-preview App.java

# Running the compiled preview bytecode
java --enable-preview App
```

### Checking Active Runtime Version Programmatically
```java
public class VersionChecker {
    public static void main(String[] args) {
        Runtime.Version version = Runtime.version();
        System.out.println("Feature version: " + version.feature());
        System.out.println("Interim: " + version.interim());
        System.out.println("Update: " + version.update());
        System.out.println("Patch: " + version.patch());
        System.out.println("Full version: " + version);
    }
}
```

---

## 5. Practical Examples

### Example 1: Chronological Evolution of a Data Carrier
```java
// Goal: Compare Java 8 boilerplate vs Java 16+ Record
// Java 8: 40+ lines of equals, hashCode, getters, constructors
// Java 16+ Record:
public record CustomerDto(long id, String name, String email) {}
```
*Explanation:* Demonstrates how modern Java has transformed verbose enterprise boilerplate into concise, readable, high-performance domain definitions.

### Example 2: Inspecting Java Version Features Dynamically
```java
// Goal: Programmatically verify if current runtime satisfies LTS requirements
public class JavaLtsValidator {
    public static void main(String[] args) {
        int currentFeature = Runtime.version().feature();
        System.out.println("Detected Java Feature: " + currentFeature);

        boolean isModernLts = (currentFeature == 17 || currentFeature == 21 || currentFeature == 25);
        System.out.println("Is recognized modern LTS: " + isModernLts);

        if (currentFeature < 21) {
            System.out.println("WARNING: Runtime is older than Java 21 LTS. Virtual threads not available.");
        } else {
            System.out.println("SUCCESS: Modern concurrency (Virtual Threads) supported.");
        }
    }
}
```
*Output (on JDK 25):*
```
Detected Java Feature: 25
Is recognized modern LTS: true
SUCCESS: Modern concurrency (Virtual Threads) supported.
```
*Explanation:* Enterprise libraries use `Runtime.version()` to dynamically adapt execution strategies based on available JVM features.

### Example 3: Edge Case: ClassFile Version Rejection on Preview Code
```java
// Goal: Understand what happens when running preview bytecode without the flag
// Scenario: App compiled with 'javac --enable-preview --release 25 App.java'
// Executing: 'java App'
// Output:
// Error: LinkageError occurred while loading main class App
//   java.lang.UnsupportedClassVersionError: Preview features are not enabled for App
//   (class file version 69.65535). Try running with '--enable-preview'
```
*Explanation:* The JVM prevents accidental execution of preview APIs in production by validating class version headers against the `--enable-preview` JVM flag.

### Example 4: Multi-Release JAR (MR-JAR) File Structure
```
// Goal: Distribute one JAR file that runs on Java 17, 21, and 25 using optimal APIs
myapp.jar
├── META-INF
│   ├── MANIFEST.MF (Multi-Release: true)
│   └── versions
│       ├── 21
│       │   └── com/corp/Engine.class (uses Virtual Threads)
│       └── 25
│           └── com/corp/Engine.class (uses Compact Source / Module imports)
└── com
    └── corp
        └── Engine.class (Java 17 baseline implementation)
```
*Explanation:* Multi-Release JARs (JEP 238) allow libraries to exploit modern Java features on newer JVMs while maintaining backwards compatibility for older runtimes.

---

### Long-Term Support Enterprise Matrix
Enterprise organizations require clear stability roadmaps when provisioning virtual machines and container images. The OpenJDK ecosystem provides predictable support commitments across major enterprise vendors:

| LTS Release | GA Date | Oracle Premier Support | Adoptium / Temurin Community Support | Primary Runtime Innovations |
|---|---|---|---|---|
| **Java 8** | March 2014 | Dec 2030 (Extended) | May 2026+ | Lambdas, Streams, java.time |
| **Java 11** | September 2018 | Jan 2032 (Extended) | Oct 2027+ | JPMS Modules, HttpClient, var |
| **Java 17** | September 2021 | Sep 2029 | Oct 2027+ | Records, Sealed Classes, G1 optimizations |
| **Java 21** | September 2023 | Sep 2031 | Dec 2029+ | Virtual Threads, Sequenced Collections |
| **Java 25** | September 2025 | Sep 2033 | Oct 2031+ | Flexible Constructors, Module Imports |

Transitioning between LTS versions is simplified when builds avoid internal `sun.misc.*` APIs and adhere strictly to standard Java SE contracts.

## 6. Best Practices
1. **Target LTS as Production Anchors**: Enterprise applications should standardize production clusters on LTS releases (Java 21 or Java 25 LTS).
2. **Never Deploy `--enable-preview` to Production**: Preview APIs are subject to breaking syntax and binary changes in subsequent versions. Use them for prototyping only.
3. **Automate Continuous JDK Testing**: Test continuous integration (CI) builds against both the current LTS and the latest feature release to detect regressions early.
4. **Use `javac --release N`**: Avoid `--source` and `--target` alone. `--release N` configures the compiler to cross-compile against the exact standard library signature of version N.
5. **Monitor Deprecation Warnings**: Treat `-Xlint:deprecation` warnings seriously in build pipelines, especially those marked `forRemoval = true`.

---

## 7. Common Mistakes & Pitfalls

| Mistake | Why Wrong | Fix |
|---|---|---|
| Using `--enable-preview` in production | Bytecode version `65535` is locked to that exact compiler build and APIs may break or be dropped. | Only use permanent standardized features in production builds. |
| Using `-source` / `-target` instead of `--release` | Compiles for older bytecode but accidentally links against modern JDK runtime methods, crashing older JVMs at runtime. | Always use `--release <version>`. |
| Assuming non-LTS releases are unstable | Non-LTS releases undergo the identical rigorous TCK (Technology Compatibility Kit) testing as LTS releases. | Deploy non-LTS releases if team can upgrade every 6 months. |
| Neglecting JVM GC defaults across versions | Java 8 defaulted to Parallel GC; Java 9+ defaults to G1 GC; Java 21+ includes Generational ZGC. | Profile application memory profile when upgrading LTS releases. |
| Confusing Incubator modules with Preview features | Incubator modules are separate JARs (`jdk.incubator.*`); Preview features reside inside core packages (`java.lang.*`). | Understand the distinct opt-in flags (`--add-modules` vs `--enable-preview`). |
| Missing the 2-year LTS shift | Assuming 3-year cycles (expecting Java 23 LTS instead of Java 21 and 25). | Plan upgrades according to the official 2-year cadence adopted since Java 17. |

---

## 8. Performance & Security Notes

### Garbage Collection Evolution Across LTS Releases
- **Java 8**: Parallel GC default. High throughput, long stop-the-world (STW) pauses.
- **Java 11**: G1 GC default. Low pause times for multi-gigabyte heaps.
- **Java 17**: Highly optimized G1 GC and introduction of production-ready ZGC.
- **Java 21**: **Generational ZGC (JEP 439)** introduced, delivering sub-millisecond pauses across multi-terabyte heaps.
- **Java 25**: Generational ZGC made the default for low-latency profiles, near-zero pause times.

### Compact Strings & Memory Footprint
Java 9 replaced the internal `char[]` (2 bytes per character) in `java.lang.String` with `byte[]` plus an encoding flag (Latin-1 vs UTF-16). Upgrading from Java 8 to modern LTS immediately reduces application heap footprint by 15% to 30% without changing a single line of code.

---

## 9. Comparison / When to Use What

| Java Version | Release Date | Support Type | Key Highlights |
|---|---|---|---|
| **Java 8** | Mar 2014 | Legacy LTS | Lambdas, Streams, java.time |
| **Java 11** | Sep 2018 | Modern LTS | JPMS Modules, HttpClient, var, String methods |
| **Java 17** | Sep 2021 | Modern LTS | Records, Sealed Classes, Pattern Matching instanceof |
| **Java 21** | Sep 2023 | Modern LTS | Virtual Threads, Sequenced Collections, Record Patterns |
| **Java 25** | Sep 2025 | Current LTS | Flexible Constructors, Module Imports, Compact Source |

---

## 10. Interview Questions

<details>
<summary>1. Describe Java's modern release cadence and LTS roadmap.</summary>
Since Java 9, Java releases arrive every 6 months (March and September). Releases undergo strict time-based feature docking. LTS releases occur every 2 years (Java 17, 21, 25). LTS versions receive long-term commercial and enterprise support, whereas interim releases are supported for 6 months.
</details>

<details>
<summary>2. What is the difference between an Incubator API and a Preview Feature?</summary>
An Incubator API lives in isolated non-standard modules (`jdk.incubator.*`) and tests experimental APIs without affecting standard libraries. A Preview Feature is fully implemented in standard packages and language syntax, but requires `--enable-preview` to prevent deployment of non-final bytecode.
</details>

<details>
<summary>3. Why was `--release` introduced to replace `-source` and `-target`?</summary>
When compiling with `-source 8 -target 8` on a Java 17 JDK, the compiler generated Java 8 bytecode but linked against the Java 17 standard library. If code invoked a method added in Java 9+, the application compiled but threw `NoSuchMethodError` on Java 8 JVMs. `--release 8` cross-compiles against the exact signature data of Java 8.
</details>

<details>
<summary>4. What are the major architectural milestones between Java 8 and Java 25?</summary>
Java 9 (Modules & Compact Strings), Java 11 (HttpClient & var), Java 16 (Records), Java 17 (Sealed Classes), Java 21 (Virtual Threads & Sequenced Collections), Java 25 (Flexible Constructors & Module Imports).
</details>

<details>
<summary>5. What is a Multi-Release JAR (MR-JAR)?</summary>
A JAR file containing different class file versions under `META-INF/versions/N/`. Modern JVMs load the optimized version matching their feature release, while older JVMs load the baseline root class files.
</details>

<details>
<summary>6. How does the JVM enforce preview feature restrictions?</summary>
Class files compiled with `--enable-preview` are tagged with minor version `65535`. The JVM refuses to load them unless started with `--enable-preview`, preventing preview bytecode from running in unintended production environments.
</details>

<details>
<summary>7. Why did the OpenJDK board reduce the LTS cadence from 3 years to 2 years?</summary>
To allow enterprises to consume major language and runtime innovations (like Project Loom Virtual Threads and Project Panama) at a faster, predictable pace without waiting 3 to 4 years.
</details>

<details>
<summary>8. What happens to deprecated APIs marked `forRemoval = true`?</summary>
They are actively removed in subsequent releases. Examples include `Thread.stop()`, the `finalize()` method (deprecated in Java 9, removed in JEP 421), and the Security Manager (deprecated in Java 17, permanently disabled in Java 24/25).
</details>

---

## 11. Practice Exercises

### Exercise 1 (Easy): Parsing Runtime Versions
- **Task**: Write a program that extracts the major, minor, and security numbers from `Runtime.version()` and prints whether the JVM supports Virtual Threads (Java 21+).
<details>
<summary>Solution</summary>

```java
public class VersionCheckExercise {
    public static void main(String[] args) {
        Runtime.Version ver = Runtime.version();
        int feature = ver.feature();
        System.out.println("Java Feature Version: " + feature);
        if (feature >= 21) {
            System.out.println("Virtual Threads supported!");
        } else {
            System.out.println("Virtual Threads NOT supported.");
        }
    }
}
```
</details>

### Exercise 2 (Medium): Simulating JEP Evolution Tracking
- **Task**: Model a `JEP` record containing `int id`, `String title`, `JepStatus status` (DRAFT, PREVIEW, FINAL), and `int targetVersion`. Filter a list of JEPs to find all finalized features in Java 21 and Java 25.
<details>
<summary>Solution</summary>

```java
import java.util.List;

public class JepTrackerDemo {
    public enum JepStatus { DRAFT, PREVIEW, FINAL }
    public record Jep(int id, String title, JepStatus status, int version) {}

    public static void main(String[] args) {
        List<Jep> jeps = List.of(
            new Jep(444, "Virtual Threads", JepStatus.FINAL, 21),
            new Jep(431, "Sequenced Collections", JepStatus.FINAL, 21),
            new Jep(454, "Foreign Function & Memory API", JepStatus.FINAL, 22),
            new Jep(513, "Flexible Constructor Bodies", JepStatus.FINAL, 25),
            new Jep(512, "Compact Source Files", JepStatus.FINAL, 25)
        );

        List<Jep> ltsFeatures = jeps.stream()
            .filter(j -> j.status() == JepStatus.FINAL && (j.version() == 21 || j.version() == 25))
            .toList();

        ltsFeatures.forEach(System.out::println);
    }
}
```
</details>

### Exercise 3 (Hard): Dynamic Feature-Adaptive Dispatcher
- **Task**: Implement a class that inspects the runtime environment and dynamically invokes a high-performance modern API if running on Java 21+ (e.g. Virtual Threads), or falls back to traditional thread pools on older JVMs.
<details>
<summary>Solution</summary>

```java
import java.util.concurrent.*;

public class AdaptiveExecutor {
    public static ExecutorService createOptimalExecutor() {
        int feature = Runtime.version().feature();
        if (feature >= 21) {
            try {
                // Use reflection or direct call to VirtualThreadPerTaskExecutor
                return (ExecutorService) Executors.class
                    .getMethod("newVirtualThreadPerTaskExecutor")
                    .invoke(null);
            } catch (Exception e) {
                // fallback
            }
        }
        return Executors.newFixedThreadPool(Runtime.getRuntime().availableProcessors() * 2);
    }

    public static void main(String[] args) {
        try (ExecutorService executor = createOptimalExecutor()) {
            executor.submit(() -> System.out.println("Executed on: " + Thread.currentThread()));
        }
    }
}
```
</details>

---

## 12. Summary / Cheat Sheet

```
+-----------------------------------------------------------------------------------------+
|                               JAVA RELEASE TIMELINE CHEAT SHEET                         |
+-----------------------------------------------------------------------------------------+
| Release Cadence          | Every 6 months (March & September)                           |
| LTS Cadence              | Every 2 years (17 in 2021, 21 in 2023, 25 in 2025)           |
| Cross-compilation        | javac --release <N> App.java (Strict signature validation)   |
| Preview Flag             | --enable-preview (Sets minor version 65535)                  |
| Incubator Modules        | --add-modules jdk.incubator.<name>                           |
| String Optimization      | Compact Strings (Latin-1 byte[] vs char[]) since Java 9      |
| Low-latency GC           | Generational ZGC (JEP 439) in Java 21, Default low-latency 25|
+-----------------------------------------------------------------------------------------+
```

---

## 13. Further Reading
- [JEP 223: New Version-String Scheme](https://openjdk.org/jeps/223)
- [OpenJDK Release Process Guidelines](https://openjdk.org/projects/jdk/releases/)
- [Oracle Java SE Support Roadmap](https://www.oracle.com/java/technologies/java-se-support-roadmap.html)
- [Adoptium Eclipse Temurin Support Documentation](https://adoptium.net/support/)
