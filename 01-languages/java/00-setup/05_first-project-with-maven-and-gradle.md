# First Project with Maven and Gradle

> Build automation tools manage the entire software lifecycle—compiling source code, resolving transitive dependencies, running test suites, and packaging deployable artifacts—ensuring repeatable and deterministic builds across developer machines and CI/CD pipelines.

| Metadata | Value |
| :--- | :--- |
| **Level** | Beginner to Intermediate |
| **Prerequisites** | [00_jdk-installation.md](file:///E:/Learning/engineering-notes/01-languages/java/00-setup/00_jdk-installation.md) |
| **Time to Read** | 30 minutes |
| **Status** | Complete |
| **Java LTS Baseline** | Java 25 LTS (Maven 3.9+ / Gradle 8.10+) |

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
In professional software development, manually executing `javac` and `java` commands from a terminal quickly becomes unmanageable. Real-world applications depend on dozens of third-party libraries (JSON parsers, database drivers, logging frameworks, HTTP clients). Each library in turn depends on other libraries (**transitive dependencies**). Manually downloading hundreds of JAR files, placing them on the classpath, handling version collisions, running unit tests, and assembling executable archives is error-prone, untracked, and unreproducible.

### Historical Context
- **Make & Ant (1995–2004):** Early Java projects used Apache Ant. Ant provided task execution via XML, but had no built-in dependency management or standardized project directory layouts. Developers checked huge binary `.jar` files directly into version control.
- **Apache Maven (2004–Present):** Revolutionized Java engineering by introducing **Convention over Configuration**, central binary repositories (Maven Central), declarative dependency management, and a standardized build lifecycle.
- **Gradle (2012–Present):** Built on Maven’s repository infrastructure but introduced a Directed Acyclic Graph (DAG) task execution engine, incremental build caching, and expressive domain-specific languages (initially Groovy, now standardizing on Kotlin DSL).

### Real-World Relevance
Nearly 100% of production Java systems are built with either Maven or Gradle. Understanding their directory conventions, dependency resolution rules, and wrapper mechanics is required for every Java software engineer.

---

## 2. Core Concepts

### Standard Directory Layout
Both Maven and Gradle enforce the standard directory convention:

```
my-application/
├── pom.xml                   # Maven build descriptor (OR build.gradle.kts for Gradle)
├── mvnw                      # Maven Wrapper script (Linux/macOS)
├── mvnw.cmd                  # Maven Wrapper script (Windows)
├── .mvn/wrapper/             # Wrapper binary properties
├── src/
│   ├── main/
│   │   ├── java/             # Production Java source code (.java)
│   │   └── resources/        # Configuration, schemas, static assets
│   └── test/
│       ├── java/             # Unit and integration test source code
│       └── resources/        # Test-specific properties and fixtures
└── target/                   # Maven build output directory (OR build/ for Gradle)
    ├── classes/              # Compiled production .class files
    ├── test-classes/         # Compiled test .class files
    └── my-app-1.0.0.jar      # Packaged binary archive
```

### The Maven Build Lifecycle
Maven structures execution around three built-in lifecycles: **default**, **clean**, and **site**. The `default` lifecycle consists of ordered phases:
1. `validate`: Verify project structure and POM validity.
2. `compile`: Compile production source code (`src/main/java`).
3. `test`: Run unit tests using an execution framework (JUnit 5, TestNG).
4. `package`: Assemble compiled code into a distributable archive (`.jar` or `.war`).
5. `verify`: Run integration tests and check quality gates.
6. `install`: Copy the artifact into your local cache (`~/.m2/repository`).
7. `deploy`: Publish the final artifact to an enterprise remote repository (Nexus, Artifactory).

### The Build Wrapper Pattern (`mvnw` and `gradlew`)
The **Wrapper** is a lightweight script committed to the Git repository. It automatically downloads, caches, and executes the exact required version of Maven or Gradle if it is not already installed on the host machine.
- **Command:** `./mvnw clean package` or `./gradlew build`
- **Benefit:** Developers and CI runners do not need to install Maven or Gradle manually. Everyone on the team executes the build using the exact same toolchain version, eliminating toolchain drift.

---

## 3. How It Works Internally

### Dependency Resolution & Conflict Algorithms

```
Application
├── Dependency A (v1.0) ──► Transitive Lib X (v1.0)  [Depth: 2]
└── Dependency B (v2.0) ──► Dependency C (v1.0) ──► Transitive Lib X (v2.0) [Depth: 3]
```

When two dependencies pull in different versions of the same transitive library (`Lib X`), the build tool must resolve the collision:
1. **Maven "Nearest-Wins" Rule:** Maven selects the version that is closest to the project root in the dependency tree. In the diagram above, `Lib X (v1.0)` is chosen because its tree depth is 2, whereas `v2.0` has a depth of 3. If depths are equal, the first declared dependency wins.
2. **Gradle "Highest-Wins" Rule:** Gradle inspects all requested versions and by default selects the highest version number (`Lib X v2.0`), assuming backward compatibility.

### Cache Storage Layout
- **Maven Local Repository (`~/.m2/repository/`):** Artifacts are stored in a rigid hierarchical filesystem matching coordinate namespaces:
  `~/.m2/repository/com/google/guava/guava/33.0.0-jre/guava-33.0.0-jre.jar`
- **Gradle Global Cache (`~/.gradle/caches/`):** Gradle stores artifacts in content-addressable storage hashed by SHA-1/SHA-256 checksums, enabling cross-project deduplication and parallel daemon caching.

---

## 4. Syntax & Usage

### 1. Minimal Production Maven Descriptor (`pom.xml`)
Configured for **Java 25 LTS** and **JUnit 5**:

```xml
<?xml version="1.0" encoding="UTF-8"?>
<project xmlns="http://maven.apache.org/POM/4.0.0"
         xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance"
         xsi:schemaLocation="http://maven.apache.org/POM/4.0.0 
         http://maven.apache.org/xsd/maven-4.0.0.xsd">
    <modelVersion>4.0.0</modelVersion>

    <groupId>com.learning.java</groupId>
    <artifactId>modern-app</artifactId>
    <version>1.0.0</version>
    <packaging>jar</packaging>

    <properties>
        <maven.compiler.release>25</maven.compiler.release>
        <project.build.sourceEncoding>UTF-8</project.build.sourceEncoding>
        <junit.jupiter.version>5.11.4</junit.jupiter.version>
    </properties>

    <dependencies>
        <!-- Unit Testing Framework -->
        <dependency>
            <groupId>org.junit.jupiter</groupId>
            <artifactId>junit-jupiter</artifactId>
            <version>${junit.jupiter.version}</version>
            <scope>test</scope>
        </dependency>
    </dependencies>

    <build>
        <plugins>
            <!-- Compiler Plugin enforcing Java 25 release baseline -->
            <plugin>
                <groupId>org.apache.maven.plugins</groupId>
                <artifactId>maven-compiler-plugin</artifactId>
                <version>3.13.0</version>
            </plugin>
            <!-- Test Runner Plugin -->
            <plugin>
                <groupId>org.apache.maven.plugins</groupId>
                <artifactId>maven-surefire-plugin</artifactId>
                <version>3.5.2</version>
            </plugin>
        </plugins>
    </build>
</project>
```

### 2. Minimal Production Gradle Descriptor (`build.gradle.kts`)
Configured for **Java 25 LTS** using modern Kotlin DSL:

```kotlin
plugins {
    java
    application
}

group = "com.learning.java"
version = "1.0.0"

repositories {
    mavenCentral()
}

java {
    toolchain {
        languageVersion.set(JavaLanguageVersion.of(25))
    }
}

dependencies {
    testImplementation("org.junit.jupiter:junit-jupiter:5.11.4")
    testRuntimeOnly("org.junit.platform:junit-platform-launcher")
}

tasks.named<Test>("test") {
    useJUnitPlatform()
}

application {
    mainClass.set("com.learning.java.App")
}
```

---

## 5. Practical Examples

### Example 1: Creating and Building a Maven Project from Scratch
- **Goal:** Create a standard directory structure, write an application class and a JUnit 5 test, and execute the build using the Maven Wrapper.
- **Application Class (`src/main/java/com/learning/java/Calculator.java`):**
```java
package com.learning.java;

public class Calculator {
    public int add(int a, int b) {
        return a + b;
    }
}
```
- **Test Class (`src/test/java/com/learning/java/CalculatorTest.java`):**
```java
package com.learning.java;

import org.junit.jupiter.api.Test;
import static org.junit.jupiter.api.Assertions.assertEquals;

class CalculatorTest {
    @Test
    void testAddition() {
        Calculator calc = new Calculator();
        assertEquals(42, calc.add(20, 22), "Addition must match expected sum");
    }
}
```
- **Execution Command & Output:**
```bash
$ ./mvnw clean test
```
```text
[INFO] Scanning for projects...
[INFO] -------------------< com.learning.java:modern-app >-------------------
[INFO] Building modern-app 1.0.0
[INFO] --------------------------------[ jar ]---------------------------------
[INFO] --- maven-compiler-plugin:3.13.0:compile (default-compile) @ modern-app ---
[INFO] Compiling 1 source file with javac [debug target 25] to target/classes
[INFO] --- maven-surefire-plugin:3.5.2:test (default-test) @ modern-app ---
[INFO] Running com.learning.java.CalculatorTest
[INFO] Tests run: 1, Failures: 0, Errors: 0, Skipped: 0, Time elapsed: 0.045 s -- in com.learning.java.CalculatorTest
[INFO] ------------------------------------------------------------------------
[INFO] BUILD SUCCESS
[INFO] ------------------------------------------------------------------------
```
- **Explanation:** Maven coordinates downloading dependencies from Maven Central, compiles source code targeting Java 25, runs the JUnit 5 engine via Surefire, and reports verified test results.

---

### Example 2: Inspecting Dependency Trees
- **Goal:** Diagnose transitive dependencies and locate where a specific library is being introduced.
```bash
# Print the full hierarchical dependency tree in Maven:
$ ./mvnw dependency:tree

# Output:
# [INFO] com.learning.java:modern-app:jar:1.0.0
# [INFO] \- org.junit.jupiter:junit-jupiter:jar:5.11.4:test
# [INFO]    +- org.junit.jupiter:junit-jupiter-api:jar:5.11.4:test
# [INFO]    |  +- org.opentest4j:opentest4j:jar:1.3.0:test
# [INFO]    |  \- org.junit.platform:junit-platform-commons:jar:1.11.4:test
# [INFO]    +- org.junit.jupiter:junit-jupiter-params:jar:5.11.4:test
# [INFO]    \- org.junit.jupiter:junit-jupiter-engine:jar:5.11.4:test
# [INFO]       \- org.junit.platform:junit-platform-engine:jar:1.11.4:test
```
- **Explanation:** `dependency:tree` reveals the exact transitive chain and scopes (`test`, `compile`, `runtime`), enabling developers to identify bloat or vulnerable transitives.

---

### Example 3: Edge Case — Handling Transitive Dependency Collisions (Exclusions)
- **Goal:** Exclude a conflicting or vulnerable transitive dependency brought in by a third-party library in `pom.xml`.
```xml
<dependency>
    <groupId>org.apache.commons</groupId>
    <artifactId>commons-configuration2</artifactId>
    <version>2.10.1</version>
    <exclusions>
        <!-- Exclude obsolete commons-logging to route all logs through SLF4J -->
        <exclusion>
            <groupId>commons-logging</groupId>
            <artifactId>commons-logging</artifactId>
        </exclusion>
    </exclusions>
</dependency>
```
- **Explanation:** The `<exclusions>` element purges `commons-logging` from the resolution graph, preventing class-loading split-brain issues where multiple logging bridges compete at runtime.

---

### Example 4: Failure Case — Missing `<scope>test</scope>` Leakage
- **Goal:** Demonstrate what happens when a test library is declared with default (`compile`) scope rather than `test` scope.
```xml
<!-- ANTI-PATTERN: Missing <scope>test</scope> -->
<dependency>
    <groupId>org.junit.jupiter</groupId>
    <artifactId>junit-jupiter</artifactId>
    <version>5.11.4</version>
</dependency>
```
- **Pitfall:** Because the scope defaults to `compile`, the JUnit test engine is packaged into your final production JAR or Docker container, inflating archive size, introducing security attack surfaces, and allowing developers to accidentally import test assertions inside production service classes.
- **Remedy:** Always declare `<scope>test</scope>` for test frameworks, mock libraries (`mockito`), and assertion suites (`assertj`).

---

### Example 5: High-Performance / Modern Idiom — Executable Fat JAR via Shade Plugin
- **Goal:** Package an application and all its runtime dependencies into a single, standalone executable "Uber/Fat JAR" that can be launched via `java -jar app.jar`.
```xml
<plugin>
    <groupId>org.apache.maven.plugins</groupId>
    <artifactId>maven-shade-plugin</artifactId>
    <version>3.6.0</version>
    <executions>
        <execution>
            <phase>package</phase>
            <goals><goal>shade</goal></goals>
            <configuration>
                <transformers>
                    <transformer implementation="org.apache.maven.plugins.shade.resource.ManifestResourceTransformer">
                        <mainClass>com.learning.java.App</mainClass>
                    </transformer>
                </transformers>
            </configuration>
        </execution>
    </executions>
</plugin>
```
- **Verification:**
```bash
$ ./mvnw clean package
$ java -jar target/modern-app-1.0.0.jar
# Output: Build System Integration Verified. Active Java Spec: 25
```
- **Explanation:** The Shade plugin unpacks all dependent JARs and repacks their `.class` files into a single unified archive with a populated `META-INF/MANIFEST.MF` specifying the `Main-Class`, creating an artifact ready for Docker deployment.

---

## 6. Best Practices

1. **Always Commit the Wrapper (`mvnw` / `gradlew`):** Never require developers to install a global build tool. Commit both the shell script, batch file, and `.mvn/wrapper/maven-wrapper.properties`.
2. **Lock Compiler Release in Properties:** Always define `<maven.compiler.release>25</maven.compiler.release>`. This configures both `-source`, `-target`, and historical symbol linking atomically.
3. **Use Scopes Diligently:**
   - `compile`: Default. Needed at compile time, test time, and runtime.
   - `provided`: Needed for compilation, but provided by runtime container (e.g., servlet containers).
   - `runtime`: Not needed to compile, but needed to execute (e.g., JDBC driver implementations).
   - `test`: Only available in `src/test/java`.
4. **Never Use `RELEASE` or `LATEST` Dynamic Versions:** Always pin explicit version numbers or use BOMs (Bill of Materials) via `<dependencyManagement>`. Dynamic versions compromise build reproducibility.
5. **Run `dependency:analyze` Periodically:** Identifies unused declared dependencies and undeclared transitive dependencies in your codebase.

---

## 7. Common Mistakes & Pitfalls

| Mistake | Why It Fails | Practical Remedy |
| :--- | :--- | :--- |
| **Editing Files in `target/` or `build/`** | Build tools delete and overwrite output directories on every `clean` or `compile`. | Only edit files in `src/main/` or `src/test/`. |
| **Omitting `<scope>test</scope>`** | Bloats production JAR with test frameworks and permits illegal imports in domain code. | Mark all testing dependencies explicitly with `<scope>test</scope>`. |
| **Assuming Maven and Gradle Resolve Conflicts Identically** | Maven uses "nearest-wins" (depth); Gradle uses "highest-version". | Use `dependency:tree` or `gradle dependencies` to verify actual resolved versions. |
| **Committing Local Wrapper JARs to Git** | Binary `.jar` files in git repositories cause repository bloat. | Modern Maven Wrappers only require committing `maven-wrapper.properties` and the text scripts. |
| **Hardcoding Absolute Paths in `pom.xml`** | Breaks the build for other team members with different user home directories. | Use project-relative paths or Maven properties (`${project.basedir}`). |
| **Skipping Tests Permanently (`-DskipTests`)** | Masks broken regressions and creates untracked bugs in master branches. | Fix broken tests or use `@Disabled` with ticket references rather than skipping the build phase. |

---

## 8. Performance & Security Notes

### Supply Chain Security: Checksums and Repositories
- **Central Repository Verification:** Maven Central requires all published artifacts to be cryptographically signed via PGP.
- **Repository Hijacking Defense:** Never add unvetted, unencrypted HTTP repository URLs in `<repositories>`. Always enforce HTTPS.
- **Dependency Vulnerability Scanning:** Integrate OWASP Dependency-Check or Snyk plugins into your build lifecycle to automatically fail builds when known CVEs exist in your dependencies.

### Build Performance Tuning
- **Parallel Maven Builds:** Accelerate multi-module compilation across CPU cores:
  ```bash
  ./mvnw clean compile -T 1C   # Allocates 1 thread per physical CPU core
  ```
- **Gradle Build Cache & Daemon:** Gradle keeps a warm background JVM process (the Daemon) and caches task outputs by input cryptographic hashes, enabling sub-second incremental builds.

---

## 9. Comparison / When to Use What

| Dimension | Apache Maven | Gradle (Kotlin DSL) |
| :--- | :--- | :--- |
| **Configuration Style** | Declarative XML (`pom.xml`) | Programmatic Kotlin Script (`build.gradle.kts`) |
| **Lifecycle Model** | Fixed, convention-based phases | Dynamic Directed Acyclic Graph (DAG) of tasks |
| **Conflict Resolution** | Nearest-Wins (shallowest depth) | Highest-Wins (latest semantic version) |
| **Incremental Performance** | Moderate (Module-level granularity) | **Exceptional** (Task-level & distributed build caching) |
| **Multi-Language Support** | Primarily JVM | JVM, C++, Android, Swift, Kotlin Multiplatform |
| **Enterprise Adoption** | Universal in traditional enterprise | Dominant in Android, microservices, and large monorepos |
| **Recommended Scenario** | Standard microservices, corporate consistency | Massive monorepos, Android apps, custom build logic |

---

## 10. Interview Questions

<details>
<summary><strong>1. What is the fundamental difference between Maven's "Nearest-Wins" and Gradle's "Highest-Wins" conflict resolution strategies?</strong></summary>

**Answer:**  
When multiple conflicting versions of a transitive dependency appear in a dependency tree:
- **Maven** chooses the version that is closest to the root project (shortest path depth). If depth is equal, the first declared branch wins. This can lead to older versions being selected over newer bug-fix releases.
- **Gradle** inspects all requested versions in the graph and by default selects the highest version number, assuming semantic versioning and backward compatibility.
</details>

<details>
<summary><strong>2. What is the Build Wrapper (`mvnw` / `gradlew`), and what architectural problem does it solve?</strong></summary>

**Answer:**  
The Wrapper is a lightweight shell/batch script checked into version control alongside project code. When executed, it checks whether the required build tool runtime exists in the user's home directory (`~/.m2/wrapper` or `~/.gradle/wrapper`). If missing, it downloads the exact distribution specified in its properties file. This guarantees 100% build tool reproducibility across developers, operating systems, and CI/CD agents without requiring pre-installed system software.
</details>

<details>
<summary><strong>3. What are Maven Lifecycles, Phases, and Goals, and how do they relate?</strong></summary>

**Answer:**  
- A **Lifecycle** is an overall build sequence (e.g., `default`, `clean`, `site`).
- A **Phase** is an ordered stage within a lifecycle (e.g., `compile`, `test`, `package`). When you invoke a phase, Maven executes all preceding phases in that lifecycle sequentially.
- A **Goal** is the atomic unit of work implemented by a Maven plugin (e.g., `compiler:compile`, `surefire:test`). Plugins bind their goals to specific lifecycle phases.
</details>

<details>
<summary><strong>4. How does the `<dependencyManagement>` section differ from the `<dependencies>` section in Maven?</strong></summary>

**Answer:**  
- `<dependencies>` directly declares and downloads libraries for the current module.
- `<dependencyManagement>` does **not** add dependencies to the module classpath. Instead, it acts as a centralized lookup table for version numbers, exclusions, and scopes. Child modules or consumer projects can declare dependencies without specifying version numbers, inheriting the centralized version to guarantee consistency across multi-module projects.
</details>

<details>
<summary><strong>5. What is the purpose of `<scope>provided</scope>` in Maven?</strong></summary>

**Answer:**  
The `provided` scope indicates that a dependency is required to compile the application code and run unit tests, but must **not** be bundled into the generated JAR/WAR file because the runtime execution environment (such as a Tomcat servlet container or a cloud platform runtime) already provides that library.
</details>

<details>
<summary><strong>6. What is a "Fat/Uber JAR", and what problem does the Maven Shade plugin solve?</strong></summary>

**Answer:**  
A standard JAR contains only the compiled classes of the project itself, requiring external classpath configuration at runtime. A Fat JAR packages the application classes and the unpacked contents of all transitive dependency JARs into a single distributable archive. The Maven Shade Plugin creates Fat JARs and can additionally **relocate** (shade) package namespaces to prevent classpath collisions when two libraries bundle conflicting versions of an internal utility.
</details>

<details>
<summary><strong>7. How does Gradle achieve faster incremental builds than Maven?</strong></summary>

**Answer:**  
Gradle uses an in-memory daemon process that avoids JVM cold-start latency, keeps classloaders cached, and models builds as a Directed Acyclic Graph (DAG) of fine-grained tasks. Every task defines explicit input and output files. Gradle computes cryptographic checksums of inputs: if inputs have not changed, Gradle marks the task `UP-TO-DATE` and skips execution entirely. Furthermore, Gradle supports a distributed remote build cache where outputs can be shared across team members.
</details>

<details>
<summary><strong>8. Why should you always use `<maven.compiler.release>` instead of `-source` and `-target`?</strong></summary>

**Answer:**  
Setting `-source` and `-target` sets bytecode format versioning, but links against the *current* JDK’s runtime library signatures. Setting `<maven.compiler.release>25</maven.compiler.release>` automatically enforces both source, target, and historical bootstrap symbol tables (`ct.sym`), ensuring that the compiler rejects any API call that did not exist in the targeted Java specification version.
</details>

---

## 11. Practice Exercises

### Exercise 1: Minimal Multi-Module POM Hierarchy (Easy)
- **Task:** Formulate a parent `pom.xml` with `<packaging>pom</packaging>` defining two child modules: `core` and `api`.
<details>
<summary>Solution</summary>

```xml
<project xmlns="http://maven.apache.org/POM/4.0.0"
         xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance"
         xsi:schemaLocation="http://maven.apache.org/POM/4.0.0 
         http://maven.apache.org/xsd/maven-4.0.0.xsd">
    <modelVersion>4.0.0</modelVersion>

    <groupId>com.learning.java</groupId>
    <artifactId>parent-project</artifactId>
    <version>1.0.0</version>
    <packaging>pom</packaging>

    <modules>
        <module>core</module>
        <module>api</module>
    </modules>

    <properties>
        <maven.compiler.release>25</maven.compiler.release>
    </properties>
</project>
```
</details>

### Exercise 2: Transitive Dependency Conflict Resolution (Medium)
- **Task:** In a Maven project where `lib-a` pulls in `jackson-databind:2.14.0` and `lib-b` pulls in `jackson-databind:2.17.0`, write the configuration in `pom.xml` to force the build to resolve strictly to `2.17.0` regardless of tree depth.
<details>
<summary>Solution</summary>

```xml
<dependencyManagement>
    <dependencies>
        <dependency>
            <groupId>com.fasterxml.jackson.core</groupId>
            <artifactId>jackson-databind</artifactId>
            <version>2.17.0</version>
        </dependency>
    </dependencies>
</dependencyManagement>
```
- **Explanation:** Explicit declaration in `<dependencyManagement>` overrides Maven's default nearest-wins algorithm, forcing all modules and transitive references to use version `2.17.0`.
</details>

### Exercise 3: Automated Custom Build Verification Task (Hard)
- **Task:** In a Gradle project (`build.gradle.kts`), write a custom task named `verifyJavaVersion` that executes before `compileJava` and asserts that the executing Gradle JVM feature version is equal to 25.
<details>
<summary>Solution</summary>

```kotlin
tasks.register("verifyJavaVersion") {
    doLast {
        val currentFeature = Runtime.version().feature()
        if (currentFeature != 25) {
            throw GradleException(
                "Build failure: Requires Java 25 LTS, but running on Java $currentFeature"
            )
        }
        println("Verified Java 25 LTS execution environment.")
    }
}

tasks.named("compileJava") {
    dependsOn("verifyJavaVersion")
}
```
</details>

---

## 12. Summary / Cheat Sheet

- **Standard Layout:** Production code in `src/main/java`; tests in `src/test/java`.
- **Maven Lifecycle:** `clean` $\to$ `compile` $\to$ `test` $\to$ `package` $\to$ `install` $\to$ `deploy`.
- **Always Use Wrappers:** Run `./mvnw` or `./gradlew` rather than system-wide installed tools.
- **Dependency Conflicts:**
  - Maven = Nearest-Wins (depth).
  - Gradle = Highest-Wins (latest version).
- **Toolchain Setting:** Always configure `<maven.compiler.release>25</maven.compiler.release>`.

---

## 13. Further Reading

- [Apache Maven Project Documentation](https://maven.apache.org/guides/)
- [Gradle Official User Manual & Kotlin DSL Guide](https://docs.gradle.org/current/userguide/userguide.html)
- [Maven Central Repository Search](https://central.sonatype.com/)
- [JUnit 5 User Guide](https://junit.org/junit5/docs/current/user-guide/)

---

| [← Previous: 04_intellij-idea.md](file:///E:/Learning/engineering-notes/01-languages/java/00-setup/04_intellij-idea.md) | [Section Index](file:///E:/Learning/engineering-notes/01-languages/java/00-setup/README.md) | [Next Module: 01-fundamentals →](file:///E:/Learning/engineering-notes/01-languages/java/01-fundamentals/README.md) |
