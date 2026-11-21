# Apache Maven: Lifecycle, Plugins, and Multi-Module Architecture

> The standard declarative build tool of the Java ecosystem: Build Lifecycles, POM Anatomy, Dependency Scopes, Plugin Pipelines, and Reactor Multi-Module Builds.

| Metadata | Details |
|---|---|
| **Level** | Core / Intermediate |
| **Prerequisites** | Java Packages, JAR Packaging |
| **Time to Read** | 25 minutes |
| **Status** | Complete |
| **JDK Baseline** | Java 17 / 21 / 25 LTS (Maven 3.9+ / 4.0) |

---

## Table of Contents
1. [1. What & Why](#1-what--why)
2. [2. Core Concepts](#2-core-concepts)
3. [3. How It Works Internally: Lifecycles, Phases, & Reactor Graph](#3-how-it-works-internally-lifecycles-phases--reactor-graph)
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
Apache Maven is the foundational build automation and project management tool that standardized the Java ecosystem. Before Maven, Java projects relied on Apache Ant or custom Makefiles, which lacked standardized directory layouts, dependency management, or universal lifecycle definitions.

Maven introduced convention over configuration: standard directory layouts (`src/main/java`, `src/test/resources`), declarative XML project object models (`pom.xml`), universal artifact coordinates (`groupId:artifactId:version`), and an opinionated build lifecycle. Understanding Maven's three lifecycles, transitive dependency resolution mechanisms, plugin bindings, and multi-module reactor topology is indispensable for reproducible enterprise builds.

---

## 2. Core Concepts
- **Project Object Model (`pom.xml`)**: The declarative XML descriptor defining project coordinates, dependencies, build plugins, and profiles.
- **Artifact Coordinates (GAV)**: Unique three-part coordinate: `groupId` (organization reverse domain), `artifactId` (module name), `version` (release or `-SNAPSHOT`).
- **Standard Lifecycles**:
  1. `default`: Core lifecycle handling compile, test, package, install, and deploy.
  2. `clean`: Cleans target build directory (`target/`).
  3. `site`: Generates HTML project documentation and reports.
- **Phases vs Goals**: A *Phase* is an abstract step in a lifecycle (e.g., `compile`, `test`, `package`). A *Goal* is an executable task implemented by a Plugin (e.g., `compiler:compile`, `surefire:test`) bound to a phase.
- **Dependency Scopes**: Control classpath inclusion for compile, runtime, and test phases (`compile`, `provided`, `runtime`, `test`, `system`, `import`).
- **Reactor & Multi-Module Builds**: Mechanism that calculates the topological sort dependency graph across parent and child modules, building modules in deterministic order.
- **Bill of Materials (BOM)**: A POM packaging artifact that centralizes dependency versions in `<dependencyManagement>` for consistent multi-module consumption.

---

## 3. How It Works Internally: Lifecycles, Phases, & Reactor Graph

When a developer invokes `mvn clean package`, Maven executes a deterministic sequence of lifecycles and phase bindings:

```
[ mvn clean package ]
         |
         +--> 1. Clean Lifecycle: pre-clean -> clean -> post-clean
         |
         +--> 2. Default Lifecycle:
                   validate
                      |
                   compile (maven-compiler-plugin:compile)
                      |
                   test (maven-surefire-plugin:test)
                      |
                   package (maven-jar-plugin:jar)
```

### Transitive Dependency Mediation:
When two dependencies request conflicting versions of the same library:
- **Nearest Definition Wins (Depth-First)**: If Project depends directly on Library A (depth 1) which depends on Log4j 2.17 (depth 2), and depends on Library B (depth 1) which depends on Library C (depth 2) which depends on Log4j 2.12 (depth 3), Maven selects **Log4j 2.17** because depth 2 is closer to the root than depth 3.
- **First Declared Wins (Tie-Breaker)**: If two conflicting versions exist at the exact same tree depth, the dependency declared first in the `pom.xml` wins.

---

## 4. Syntax & Usage

### 4.1 Production `pom.xml` Anatomy for Java 21/25
```xml
<project xmlns="http://maven.apache.org/POM/4.0.0"
         xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance"
         xsi:schemaLocation="http://maven.apache.org/POM/4.0.0 
         https://maven.apache.org/xsd/maven-4.0.0.xsd">
    <modelVersion>4.0.0</modelVersion>

    <groupId>com.example.service</groupId>
    <artifactId>order-service</artifactId>
    <version>1.0.0-SNAPSHOT</version>
    <packaging>jar</packaging>

    <properties>
        <java.version>21</java.version>
        <maven.compiler.release>21</maven.compiler.release>
        <project.build.sourceEncoding>UTF-8</project.build.sourceEncoding>
        <jackson.version>2.17.2</jackson.version>
        <junit.jupiter.version>5.10.3</junit.jupiter.version>
    </properties>

    <dependencyManagement>
        <dependencies>
            <!-- Importing Spring Boot or Cloud BOM -->
            <dependency>
                <groupId>com.fasterxml.jackson</groupId>
                <artifactId>jackson-bom</artifactId>
                <version>${jackson.version}</version>
                <type>pom</type>
                <scope>import</scope>
            </dependency>
        </dependencies>
    </dependencyManagement>

    <dependencies>
        <!-- Standard Compile Dependency -->
        <dependency>
            <groupId>com.fasterxml.jackson.core</groupId>
            <artifactId>jackson-databind</artifactId>
        </dependency>

        <!-- Test Scoped Dependency -->
        <dependency>
            <groupId>org.junit.jupiter</groupId>
            <artifactId>junit-jupiter</artifactId>
            <version>${junit.jupiter.version}</version>
            <scope>test</scope>
        </dependency>
    </dependencies>

    <build>
        <plugins>
            <!-- Compiler Plugin with Modern Java Release Flag -->
            <plugin>
                <groupId>org.apache.maven.plugins</groupId>
                <artifactId>maven-compiler-plugin</artifactId>
                <version>3.13.0</version>
                <configuration>
                    <release>${maven.compiler.release}</release>
                    <compilerArgs>
                        <arg>-Xlint:all</arg>
                        <arg>-parameters</arg>
                    </compilerArgs>
                </configuration>
            </plugin>

            <!-- Unit Testing Plugin -->
            <plugin>
                <groupId>org.apache.maven.plugins</groupId>
                <artifactId>maven-surefire-plugin</artifactId>
                <version>3.3.0</version>
            </plugin>
        </plugins>
    </build>
</project>
```

---

## 5. Practical Examples

### Example 1: Multi-Module Root Parent POM Structure
```xml
<!-- Root Aggregator POM -->
<project>
    <modelVersion>4.0.0</modelVersion>
    <groupId>com.example.enterprise</groupId>
    <artifactId>enterprise-parent</artifactId>
    <version>2.0.0</version>
    <packaging>pom</packaging>

    <modules>
        <module>common-domain</module>
        <module>persistence-layer</module>
        <module>api-gateway</module>
    </modules>

    <dependencyManagement>
        <dependencies>
            <dependency>
                <groupId>com.example.enterprise</groupId>
                <artifactId>common-domain</artifactId>
                <version>${project.version}</version>
            </dependency>
            <dependency>
                <groupId>com.example.enterprise</groupId>
                <artifactId>persistence-layer</artifactId>
                <version>${project.version}</version>
            </dependency>
        </dependencies>
    </dependencyManagement>
</project>
```

### Example 2: Configuring Integration Tests with Failsafe Plugin
```xml
<!-- Failsafe runs during 'integration-test' and verifies results during 'verify' -->
<plugin>
    <groupId>org.apache.maven.plugins</groupId>
    <artifactId>maven-failsafe-plugin</artifactId>
    <version>3.3.0</version>
    <executions>
        <execution>
            <goals>
                <goal>integration-test</goal>
                <goal>verify</goal>
            </goals>
        </execution>
    </executions>
    <configuration>
        <includes>
            <include>**/*IT.java</include>
            <include>**/*IntegrationTest.java</include>
        </includes>
    </configuration>
</plugin>
```

### Example 3: Edge Case - Building Fat/Uber Executable JAR with Maven Shade
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
                        <mainClass>com.example.service.ApplicationMain</mainClass>
                    </transformer>
                    <!-- Appends SPI services across META-INF/services -->
                    <transformer implementation="org.apache.maven.plugins.shade.resource.ServicesResourceTransformer"/>
                </transformers>
            </configuration>
        </execution>
    </executions>
</plugin>
```

---

## 6. Best Practices
1. **Always Use `<maven.compiler.release>`**: Configure `<release>21</release>` rather than separate `<source>` and `<target>`, ensuring compilation checks against the exact target JDK standard library API signature.
2. **Commit the Maven Wrapper (`./mvnw`)**: Always commit `.mvn/wrapper/` and `mvnw` scripts to version control, ensuring all team members and CI/CD agents use the exact identical Maven binary.
3. **Centralize Versions in `<dependencyManagement>`**: Never scatter hardcoded version strings across child POMs; manage versions strictly in the root parent BOM.
4. **Use `maven-enforcer-plugin`**: Enforce build constraints (e.g., minimum Java version, banning duplicate classes, requiring dependency convergence).
5. **Separate Unit Tests from Integration Tests**: Run fast in-memory unit tests via Surefire (`*Test.java`), and slow containerized tests via Failsafe (`*IT.java`) during the `verify` phase.
6. **Pass `-parameters` to the Compiler**: Informs the compiler to store method and record parameter names in bytecode metadata, eliminating manual `@Param` annotations.

---

## 7. Common Mistakes & Pitfalls

| Mistake | Why It Fails | Correction |
|---|---|---|
| Using `<source>`/`<target>` instead of `<release>` | Allows code to call newer JDK APIs, failing at runtime on target JDK | Use `<maven.compiler.release>21</maven.compiler.release>` |
| Committing local `~/.m2/repository` paths | Breaks builds on other developer machines and CI servers | Use relative module paths or published repository coordinates |
| Transitive dependency version collision | "Nearest definition" selects an older incompatible library | Enforce versions using `<dependencyManagement>` |
| Running `mvn clean` on every build | Destroys incremental compiler caches, doubling build duration | Run `mvn compile` or `mvn test` directly during development |
| Shadowing SPI files in Uber JARs | Overwrites `META-INF/services` files from different libraries | Use `ServicesResourceTransformer` in `maven-shade-plugin` |
| Putting integration tests in Surefire | Slows down `mvn test`, blocks rapid feedback loop | Use `maven-failsafe-plugin` for `*IT.java` tests |
| Missing `-SNAPSHOT` on development versions | Maven caches release versions permanently, ignoring updates | Append `-SNAPSHOT` to active feature development versions |
| Forgetting `<scope>test</scope>` on test libraries | Bundles JUnit and Mockito into production runtime JARs | Explicitly declare test scopes on test dependencies |

---

## 8. Performance & Security Notes
- **Maven Daemon (`mvnd`)**: A background daemon client utilizing GraalVM native binary and multiple worker threads, speeding up Maven builds by 2x to 5x.
- **Parallel Builds**: Use `mvn -T 1C clean package` (1 thread per CPU core) to build independent multi-module projects concurrently.
- **Dependency Confusion & Repositories**: Declare trusted internal repository mirrors (`<mirrorOf>*</mirrorOf>`) in `settings.xml` to prevent build systems from fetching malicious packages from untrusted external repositories.

---

## 9. Comparison / When to Use What

| Scope | Compile Classpath | Runtime Classpath | Test Classpath | Packaged in JAR/WAR | Best Use Case |
|---|---|---|---|---|---|
| **compile** | Yes | Yes | Yes | Yes | Standard libraries (Jackson, Apache Commons) |
| **provided**| Yes | No | Yes | No | Container provided (Servlet API, Lombok) |
| **runtime** | No | Yes | Yes | Yes | JDBC Drivers, Logback implementations |
| **test** | No | No | Yes | No | Testing libraries (JUnit, Mockito, AssertJ) |
| **import** | N/A | N/A | N/A | No | Consuming external BOMs in `<dependencyManagement>`|

---

## 10. Interview Questions

<details>
<summary>1. How does Maven resolve transitive dependency conflicts, and what is the "Nearest Definition" rule?</summary>
Maven uses a deterministic tree resolution strategy:
1. <b>Nearest Definition</b>: The version closest to the root project in the dependency tree wins. If project A $	o$ B $	o$ C 1.0 (depth 2) and project A $	o$ D $	o$ E $	o$ C 2.0 (depth 3), C 1.0 is selected because it is at depth 2.<br>
2. <b>First Declared Wins</b>: If conflicting versions appear at the exact same depth, the one declared first in the <code>pom.xml</code> is chosen.<br>
To override this behavior, developers must explicitly declare the desired version in the root <code><dependencyManagement></code> block.
</details>

<details>
<summary>2. What is the fundamental difference between the Maven Surefire and Failsafe plugins?</summary>
<code>maven-surefire-plugin</code> is designed for unit tests and executes during the <code>test</code> phase. If any test fails, Surefire immediately aborts the build.<br>
<code>maven-failsafe-plugin</code> is designed for integration tests. It executes tests during the <code>integration-test</code> phase, but does not abort the build immediately on failure, allowing teardown and cleanup tasks (like stopping Testcontainers) to execute during <code>post-integration-test</code> before evaluating test failures during the <code>verify</code> phase.
</details>

<details>
<summary>3. Why is the <release> parameter preferred over <source> and <target> in modern Java builds?</summary>
Setting <code><source></code> and <code><target></code> instructs the compiler to accept older language syntax and emit older bytecode, but the compiler still resolves against the host JDK's runtime classes (<code>rt.jar</code> / <code>java.base</code>). If a developer uses a method introduced in Java 21 while targeting Java 17, compilation succeeds but throws <code>NoSuchMethodError</code> at runtime. The <code><release></code> flag links against the exact system API signature of the target release, catching version mismatches at compile time.
</details>

<details>
<summary>4. What is a Bill of Materials (BOM) in Maven?</summary>
A BOM is a special POM file with <code><packaging>pom</packaging></code> that centralizes and defines compatible dependency versions within a <code><dependencyManagement></code> block. Downstream projects import the BOM using <code><scope>import</scope></code> and <code><type>pom</type></code>, allowing them to declare dependencies without specifying explicit version tags.
</details>

<details>
<summary>5. What does the Maven Reactor do during a multi-module build?</summary>
The Reactor inspects all modules in a multi-module project, resolves inter-module dependencies, and computes a Directed Acyclic Graph (DAG). It performs a topological sort to establish the exact sequential build order so that upstream dependency modules (e.g., <code>core-domain</code>) are built, tested, and packaged before downstream consumer modules (e.g., <code>web-service</code>).
</details>

<details>
<summary>6. What is the Maven Wrapper (mvnw) and why is it standard practice in CI/CD?</summary>
The Maven Wrapper is a set of small scripts (<code>mvnw</code>, <code>mvnw.cmd</code>) and a bootstrap properties file that automatically downloads and verifies the specified version of Apache Maven before executing the build. It eliminates the requirement to pre-install Maven on developer machines or CI build agents, guaranteeing 100% build environment consistency.
</details>

<details>
<summary>7. What is the difference between mvn install and mvn deploy?</summary>
<code>mvn install</code> copies the packaged artifact (JAR/WAR) into the developer's local repository cache (<code>~/.m2/repository</code>), making it available for other local builds on the same machine.<br>
<code>mvn deploy</code> copies the final artifact, its POM, and metadata to an external remote artifact repository (e.g., Nexus, Artifactory, or Maven Central) for consumption by other teams.
</details>

<details>
<summary>8. How do you resolve shaded SPI conflicts in Uber JARs?</summary>
When multiple JARs define Service Provider Interface files at <code>META-INF/services/javax.annotation.processing.Processor</code> or similar, merging them into an Uber JAR causes subsequent files to overwrite earlier ones. Using the <code>ServicesResourceTransformer</code> inside <code>maven-shade-plugin</code> forces Maven to append and concatenate SPI entries instead of overwriting them.
</details>

---

## 11. Practice Exercises

<details>
<summary>Exercise 1: Write a Maven command to display the resolved dependency tree with conflict details.</summary>

```bash
# Print standard dependency tree
./mvnw dependency:tree

# Print verbose tree showing omitted conflicting duplicates
./mvnw dependency:tree -Dverbose
```
</details>

<details>
<summary>Exercise 2: Configure the maven-enforcer-plugin to require Java 21 and ban commons-logging.</summary>

```xml
<plugin>
    <groupId>org.apache.maven.plugins</groupId>
    <artifactId>maven-enforcer-plugin</artifactId>
    <version>3.5.0</version>
    <executions>
        <execution>
            <id>enforce-rules</id>
            <goals><goal>enforce</goal></goals>
            <configuration>
                <rules>
                    <requireJavaVersion>
                        <version>[21,)</version>
                    </requireJavaVersion>
                    <bannedDependencies>
                        <excludes>
                            <exclude>commons-logging:commons-logging</exclude>
                        </excludes>
                    </bannedDependencies>
                </rules>
            </configuration>
        </execution>
    </executions>
</plugin>
```
</details>

<details>
<summary>Exercise 3: Write a parent POM profile activating native compilation on Linux machines only.</summary>

```xml
<profiles>
    <profile>
        <id>linux-native</id>
        <activation>
            <os><family>unix</family><name>linux</name></os>
        </activation>
        <build>
            <plugins>
                <!-- Plugin execution specific to Linux -->
            </plugins>
        </build>
    </profile>
</profiles>
```
</details>

---

## 12. Summary / Cheat Sheet
- **Lifecycles**: `clean` (removes target), `default` (compile $	o$ test $	o$ package $	o$ verify $	o$ install $	o$ deploy), `site`.
- **Target Java**: Use `<maven.compiler.release>21</maven.compiler.release>` to prevent API signature mismatches.
- **Dependency Mediation**: Nearest definition wins; tie-breaker: first declared.
- **BOM Pattern**: Centralize versions in `<dependencyManagement>` using `<scope>import</scope>`.
- **Testing Plugins**: Surefire for unit tests (`*Test.java`); Failsafe for integration tests (`*IT.java`).
- **Parallel Builds**: Accelerate builds with `mvn -T 1C clean package`.

---

## 13. Further Reading
- Apache Maven Official Documentation: Better Builds with Maven
- Maven: The Complete Reference (Sonatype Guide)
- Maven Failsafe Plugin and Integration Testing Guide
- *Continuous Delivery* by Jez Humble and David Farley (Build Automation Patterns)
