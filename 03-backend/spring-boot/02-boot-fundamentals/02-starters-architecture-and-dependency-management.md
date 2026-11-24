# Spring Boot Starters: Architecture & Dependency Management

> The complete architectural anatomy of Spring Boot Starters: official vs community naming conventions, the two-module starter pattern (`starter` + `autoconfigure`), the `spring-boot-dependencies` BOM, swapping embedded servlet containers, and resolving dependency drift.

| Metadata | Value |
| :--- | :--- |
| **Level** | Intermediate |
| **Prerequisites** | [Auto-Configuration Mechanics](file:///03-backend/spring-boot/02-boot-fundamentals/01-auto-configuration-mechanics.md), [Project Setup with Initializr, Maven & Gradle](file:///03-backend/spring-boot/00-setup/03-project-setup-with-initializr-maven-gradle.md) |
| **Time to Read** | 25 minutes |
| **Status** | Complete |
| **Spring Boot Version** | 3.4.x / 4.x |
| **Spring Framework Version** | 6.2.x / 7.0.x |
| **Java Baseline** | Java 21 / 25 LTS |
| **Build Tool** | Maven 3.9+ / Gradle 8.10+ |
| **Tested With** | OpenJDK 25.0.1, Spring Boot 3.4.0 |

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
Before Spring Boot, adding JPA to an application required manually hunting down dozens of compatible dependencies: `hibernate-core`, `hibernate-validator`, `jakarta.persistence-api`, `HikariCP`, `spring-orm`, `spring-data-jpa`, and `spring-tx`. If the version of Hibernate was incompatible with the version of `spring-orm`, the application crashed with cryptic bytecode errors (`NoSuchMethodError`, `IncompatibleClassChangeError`). Managing compatible versions across hundreds of open-source libraries was a continuous operational burden.

### Historical Evolution
- **The "JAR Hell" Era (pre-2014)**: Developers maintained hundreds of lines of explicit `<dependency>` tags with hardcoded versions.
- **Spring Boot Starters (2014-Present)**: Introduced curated, opinionated dependency aggregates. Adding `spring-boot-starter-data-jpa` pulls in all compatible persistence dependencies tested together as a certified unit.
- **Spring Boot 3.4+**: Starters now manage Jakarta EE 10/11 dependencies, native GraalVM AOT hints, and Testcontainers dev services.

### Real-World Relevance
Understanding starter architecture and dependency management enables engineers to cleanly manage corporate dependencies, swap embedded servers, exclude vulnerable transitive libraries, and design custom internal starters for enterprise platforms.

---

## 2. Core Concepts

### 1. What is a Starter?
A Spring Boot Starter is an empty or near-empty coordination artifact that bundles transitive dependencies and metadata needed to enable a specific functional capability. It contains no business logic code; its `pom.xml` acts as an aggregation manifest.

### 2. Naming Conventions (Strict Rule)
- **Official Starters**: Must follow the pattern **`spring-boot-starter-*`** (e.g., `spring-boot-starter-web`, `spring-boot-starter-security`). The `spring-boot-` prefix is officially reserved for the Spring team.
- **Third-Party / Community Starters**: Must follow the pattern **`{project}-spring-boot-starter`** (e.g., `mybatis-spring-boot-starter`, `stripe-spring-boot-starter`, `acme-spring-boot-starter`).

### 3. The Two-Module Starter Architecture
Production-grade starters are divided into two distinct modules:
1. **`{name}-spring-boot-autoconfigure`**: Contains the actual auto-configuration classes, conditional beans, and configuration properties.
2. **`{name}-spring-boot-starter`**: An empty coordination POM that brings in the `autoconfigure` module plus all required third-party client JARs.

### 4. Bill of Materials (BOM)
The `spring-boot-dependencies` BOM curates tested, compatible versions of hundreds of third-party libraries (Tomcat, Jackson, Hibernate, Micrometer, Netty, Flyway). Declaring dependencies included in the BOM requires **no `<version>` tag**.

---

## 3. How It Works Internally

### Starter Resolution & Transitive Aggregation

```
Developer declares ONE dependency in pom.xml:
<dependency>
    <groupId>org.springframework.boot</groupId>
    <artifactId>spring-boot-starter-web</artifactId>
</dependency>
                          |
                          v
+-------------------------------------------------------------------------+
|                  spring-boot-starter-web (Coordination POM)             |
|  Transitively pulls in:                                                 |
|  +-- spring-boot-starter                                                |
|  |   +-- spring-boot (core runtime)                                     |
|  |   +-- spring-boot-autoconfigure (auto-config engine)                 |
|  |   +-- spring-boot-starter-logging (Logback, SLF4J)                   |
|  +-- spring-web (RestClient, HTTP core)                                 |
|  +-- spring-webmvc (DispatcherServlet, Controllers)                     |
|  +-- spring-boot-starter-json (Jackson Databind, JSR310)                |
|  +-- spring-boot-starter-tomcat (Embedded Tomcat Web Server)            |
+------------------------------------+------------------------------------+
                                     |
                                     v
+-------------------------------------------------------------------------+
|                  Resolved Classpath & Execution                         |
|  - All versions locked by `spring-boot-dependencies` BOM               |
|  - Zero version conflicts                                               |
|  - Spring Boot auto-configures Tomcat, Jackson, and MVC automatically!  |
+-------------------------------------------------------------------------+
```

---

## 4. Syntax & Usage

### Maven Starter Declaration (No Versions Needed!)
```xml
<dependencies>
    <!-- Web MVC + Jackson + Tomcat -->
    <dependency>
        <groupId>org.springframework.boot</groupId>
        <artifactId>spring-boot-starter-web</artifactId>
    </dependency>

    <!-- JPA + Hibernate + HikariCP -->
    <dependency>
        <groupId>org.springframework.boot</groupId>
        <artifactId>spring-boot-starter-data-jpa</artifactId>
    </dependency>

    <!-- Production Observability -->
    <dependency>
        <groupId>org.springframework.boot</groupId>
        <artifactId>spring-boot-starter-actuator</artifactId>
    </dependency>
</dependencies>
```

### Overriding Managed Dependency Versions via Properties
```xml
<properties>
    <!-- Safely override a managed dependency for a hotfix -->
    <hibernate.version>6.6.3.Final</hibernate.version>
    <tomcat.version>10.1.34</tomcat.version>
</properties>
```

---

## 5. Practical Examples

### Example 1: Swapping Embedded Tomcat for Undertow
```xml
<!-- pom.xml: Replace default Tomcat with Undertow for high-throughput non-blocking I/O -->
<dependencies>
    <dependency>
        <groupId>org.springframework.boot</groupId>
        <artifactId>spring-boot-starter-web</artifactId>
        <exclusions>
            <!-- 1. Exclude standard embedded Tomcat -->
            <exclusion>
                <groupId>org.springframework.boot</groupId>
                <artifactId>spring-boot-starter-tomcat</artifactId>
            </exclusion>
        </exclusions>
    </dependency>

    <!-- 2. Add high-performance embedded Undertow starter -->
    <dependency>
        <groupId>org.springframework.boot</groupId>
        <artifactId>spring-boot-starter-undertow</artifactId>
    </dependency>
</dependencies>
```

### Example 2: Swapping Embedded Tomcat for Jetty
```kotlin
// build.gradle.kts
dependencies {
    implementation("org.springframework.boot:spring-boot-starter-web") {
        exclude(group = "org.springframework.boot", module = "spring-boot-starter-tomcat")
    }
    implementation("org.springframework.boot:spring-boot-starter-jetty")
}
```

### Example 3: Adding Custom Corporate Starter POM
```xml
<!-- Internal Starter: acme-audit-spring-boot-starter/pom.xml -->
<?xml version="1.0" encoding="UTF-8"?>
<project xmlns="http://maven.apache.org/POM/4.0.0">
    <modelVersion>4.0.0</modelVersion>

    <groupId>com.acme.platform</groupId>
    <artifactId>acme-audit-spring-boot-starter</artifactId>
    <version>1.0.0</version>
    <name>Acme Audit Starter</name>

    <dependencies>
        <!-- 1. Include autoconfigure module -->
        <dependency>
            <groupId>com.acme.platform</groupId>
            <artifactId>acme-audit-spring-boot-autoconfigure</artifactId>
            <version>${project.version}</version>
        </dependency>

        <!-- 2. Include required third-party client libraries -->
        <dependency>
            <groupId>org.springframework.boot</groupId>
            <artifactId>spring-boot-starter-aop</artifactId>
        </dependency>
        <dependency>
            <groupId>org.apache.kafka</groupId>
            <artifactId>kafka-clients</artifactId>
        </dependency>
    </dependencies>
</project>
```

### Example 4: Dependency Skew Anti-Pattern & Remediation
```xml
<!-- ANTI-PATTERN: Manually hardcoding versions managed by the parent BOM -->
<dependencies>
    <dependency>
        <groupId>org.springframework.boot</groupId>
        <artifactId>spring-boot-starter-web</artifactId>
    </dependency>
    <!-- FLAW: Explicit version introduces classpath incompatibility! -->
    <dependency>
        <groupId>com.fasterxml.jackson.core</groupId>
        <artifactId>jackson-databind</artifactId>
        <version>2.14.0</version> <!-- CONFLICT: Boot 3.4 BOM expects 2.18+! -->
    </dependency>
</dependencies>

<!-- REMEDIATION: Remove version tag, letting the parent BOM control alignment -->
<dependencies>
    <dependency>
        <groupId>org.springframework.boot</groupId>
        <artifactId>spring-boot-starter-web</artifactId>
    </dependency>
    <dependency>
        <groupId>com.fasterxml.jackson.core</groupId>
        <artifactId>jackson-databind</artifactId>
        <!-- Version omitted: Automatically resolved to 2.18.x -->
    </dependency>
</dependencies>
```

### Example 5: Auditing Transitive Dependencies via CLI
```bash
# Print the full resolved dependency tree
./mvnw dependency:tree

# Filter for specific library to detect who is bringing it in
./mvnw dependency:tree -Dincludes=org.apache.tomcat.embed:tomcat-embed-core

# Analyze dependency conflicts and unused dependencies
./mvnw dependency:analyze
```

---

## 6. Best Practices

1. **Never Specify `<version>` on Managed Starters**: If a library is covered by `spring-boot-dependencies`, omit the `<version>` tag entirely.
2. **Follow the Official Naming Rule for Custom Starters**: Always name community and custom starters as `{project}-spring-boot-starter` (e.g., `acme-security-spring-boot-starter`). Never prefix custom starters with `spring-boot-starter-*`.
3. **Split Custom Starters into Two Modules**: Separate the `autoconfigure` module (containing the `@AutoConfiguration` classes) from the `starter` module (containing empty coordination dependencies).
4. **Use Property Overrides for Security Hotfixes**: To patch an emergency CVE in a managed dependency (e.g., Tomcat or Logback), override its version property (e.g., `<tomcat.version>10.1.34</tomcat.version>`) rather than hardcoding individual JAR versions.
5. **Regularly Audit Transitive Dependencies**: Run `./mvnw dependency:tree` in CI to detect unexpected duplicate JARs or legacy `javax.*` packages.
6. **Exclude Embedded Tomcat When Building Pure Reactive WebFlux**: If using `spring-boot-starter-webflux`, do NOT include `spring-boot-starter-web`; WebFlux boots high-performance Netty by default.

---

## 7. Common Mistakes & Pitfalls

| Mistake | Why It Fails | Practical Remedy |
| :--- | :--- | :--- |
| **Hardcoding dependency versions** | Overriding BOM versions leads to `NoSuchMethodError` and classpath skew. | Strip `<version>` tags for all dependencies managed by Boot. |
| **Naming custom starter `spring-boot-starter-*`** | Confuses developers and conflicts with official Spring portfolio naming conventions. | Use `{name}-spring-boot-starter`. |
| **Mixing `starter-web` and `starter-webflux`** | By default, Spring Boot prioritizes the Servlet stack (Tomcat), ignoring Netty. | Choose one: Web (Servlet/Tomcat) OR WebFlux (Reactive/Netty). |
| **Forgetting to exclude Tomcat when adding Undertow** | Both Tomcat and Undertow starters are on the classpath; Boot defaults to Tomcat, ignoring Undertow. | Exclude `spring-boot-starter-tomcat` from `spring-boot-starter-web`. |
| **Creating monolithic starters without separation** | Combining auto-config and business libraries into one module prevents users from using auto-config with alternative clients. | Use the two-module architecture (`starter` + `autoconfigure`). |
| **Leaking test dependencies into production starter** | Forgetting `<scope>test</scope>` on Testcontainers packages inside a custom starter bloats consumer JARs. | Ensure test libraries are strictly scoped as `<scope>test</scope>`. |

---

## 8. Performance & Security Notes

### Pruning Classpath to Reduce Attack Surface
Every unused JAR on the classpath increases the application's attack surface for deserialization exploits and ClassLoader gadget chains:
- Run `./mvnw dependency:analyze` to identify and remove declared dependencies that are never referenced.
- Exclude unused transitive modules (e.g., excluding embedded Tomcat WebSocket dependencies if WebSockets are not used).

### Automated Vulnerability Scanning
Integrate OWASP Dependency-Check into your Maven build to automatically block PRs introducing known CVEs:
```xml
<plugin>
    <groupId>org.owasp</groupId>
    <artifactId>dependency-check-maven</artifactId>
    <version>10.0.4</version>
</plugin>
```

---

## 9. Comparison / When to Use What

| Starter Type | Naming Convention | Artifact Group | Best For |
| :--- | :--- | :--- | :--- |
| **Official Core Starters** | `spring-boot-starter-{name}` | `org.springframework.boot` | Standard Spring web, data, security |
| **Third-Party Starters** | `{project}-spring-boot-starter` | e.g. `org.mybatis.spring.boot` | Ecosystem libraries (MyBatis, Camunda)|
| **Custom Internal Starters** | `{company}-{name}-spring-boot-starter`| `com.company.platform` | Corporate auditing, auth, observability |

---

## 10. Interview Questions

<details>
<summary><strong>Q1: What is a Spring Boot Starter and what problem does it solve?</strong></summary>

**Answer:**  
A Spring Boot Starter is an aggregation descriptor (typically an empty POM) that bundles all the transitive dependencies, configurations, and auto-configurations required for a specific capability (e.g., web development, data persistence). It eliminates manual dependency management ("JAR Hell"), guarantees version compatibility across libraries, and provides sensible out-of-the-box defaults.
</details>

<details>
<summary><strong>Q2: What is the strict naming convention rule for custom or third-party starters?</strong></summary>

**Answer:**  
Custom and third-party starters must **never** start with `spring-boot-starter-`, as that prefix is reserved strictly for official Spring Boot artifacts. Instead, custom starters must follow the pattern **`{name}-spring-boot-starter`** (e.g., `acme-logging-spring-boot-starter`).
</details>

<details>
<summary><strong>Q3: What is the purpose of the `spring-boot-dependencies` Bill of Materials (BOM)?</strong></summary>

**Answer:**  
`spring-boot-dependencies` is a central POM that defines the `<dependencyManagement>` for hundreds of compatible libraries (Spring Framework, Hibernate, Jackson, Micrometer, Tomcat). When a project inherits from `spring-boot-starter-parent` (or imports `spring-boot-dependencies`), developers can declare dependencies without specifying `<version>` tags, ensuring all components use verified, compatible versions.
</details>

<details>
<summary><strong>Q4: Why are custom starters structured as two modules (`autoconfigure` and `starter`)?</strong></summary>

**Answer:**  
This pattern separates concerns:
1. **`autoconfigure` module**: Contains the actual code—`@AutoConfiguration` classes, conditional beans, and configuration properties.
2. **`starter` module**: A lightweight aggregation POM providing the `autoconfigure` module plus the necessary third-party client dependencies.
This separation allows advanced users to import only the auto-configuration module if they want to manage client library versions independently.
</details>

<details>
<summary><strong>Q5: How do you swap embedded Tomcat with Undertow in a Spring Boot application?</strong></summary>

**Answer:**  
In `pom.xml`, exclude `spring-boot-starter-tomcat` from `spring-boot-starter-web`, and add `spring-boot-starter-undertow`:
```xml
<dependency>
    <groupId>org.springframework.boot</groupId>
    <artifactId>spring-boot-starter-web</artifactId>
    <exclusions>
        <exclusion>
            <groupId>org.springframework.boot</groupId>
            <artifactId>spring-boot-starter-tomcat</artifactId>
        </exclusion>
    </exclusions>
</dependency>
<dependency>
    <groupId>org.springframework.boot</groupId>
    <artifactId>spring-boot-starter-undertow</artifactId>
</dependency>
```
</details>

<details>
<summary><strong>Q6: How can you safely override the version of a managed dependency (e.g., Tomcat) without changing the Spring Boot version?</strong></summary>

**Answer:**  
In your project's `pom.xml`, override the corresponding version property defined by the parent POM:
```xml
<properties>
    <tomcat.version>10.1.34</tomcat.version>
</properties>
```
Because `spring-boot-dependencies` defines its Tomcat dependency version using `${tomcat.version}`, overriding this property updates all transitive Tomcat dependencies simultaneously.
</details>

<details>
<summary><strong>Q7: What command line tool displays the full transitive dependency tree in Maven?</strong></summary>

**Answer:**  
`./mvnw dependency:tree`. It outputs an ASCII graph showing direct dependencies and every nested transitive dependency, which is essential for diagnosing version conflicts and duplicate classes.
</details>

<details>
<summary><strong>Q8: What is inside `spring-boot-starter-test`?</strong></summary>

**Answer:**  
`spring-boot-starter-test` aggregates essential testing libraries: JUnit 5 (Jupiter), Spring Test & Spring Boot Test, AssertJ, Hamcrest, Mockito, JSONassert, and JsonPath.
</details>

---

## 11. Practice Exercises

<details>
<summary><strong>Exercise 1: Inspect Transitive Dependencies (Easy)</strong></summary>

**Task:**  
Run `./mvnw dependency:tree` on a project with `spring-boot-starter-web`. Identify all transitive artifacts pulled in by Jackson and Tomcat.
</details>

<details>
<summary><strong>Exercise 2: Swap Tomcat for Jetty (Easy)</strong></summary>

**Task:**  
Exclude `spring-boot-starter-tomcat` and add `spring-boot-starter-jetty`. Boot the application and verify in the console logs that Jetty (not Tomcat) starts on port 8080.
</details>

<details>
<summary><strong>Exercise 3: Emergency Tomcat Version Override (Medium)</strong></summary>

**Task:**  
Check the current Tomcat version using `dependency:tree`. Add `<tomcat.version>10.1.34</tomcat.version>` in `pom.xml`. Run `dependency:tree` again and verify all Tomcat modules updated to 10.1.34.
</details>

<details>
<summary><strong>Exercise 4: Prune WebSockets from Tomcat (Medium)</strong></summary>

**Task:**  
If your application does not use WebSockets, exclude `tomcat-embed-websocket` from `spring-boot-starter-web`. Verify the application boots normally and verify the fat JAR size reduction.
</details>

<details>
<summary><strong>Exercise 5: Build a Two-Module Starter (Hard)</strong></summary>

**Task:**  
Create a Maven multi-module project:
1. `acme-security-spring-boot-autoconfigure`
2. `acme-security-spring-boot-starter`
Package both modules, deploy to local `~/.m2/repository`, and consume the starter from a separate test Spring Boot service.
</details>

<details>
<summary><strong>Exercise 6: Automated Dependency Vulnerability Gate (Hard)</strong></summary>

**Task:**  
Configure `owasp-dependency-check` plugin in `pom.xml`. Run `./mvnw dependency-check:check` and inspect the generated HTML report in `target/dependency-check-report.html`.
</details>

---

## 12. Summary / Cheat Sheet

| Concern / Artifact | Syntax / Command | Purpose |
| :--- | :--- | :--- |
| **Official Starter** | `spring-boot-starter-web` | Official Spring dependency bundle |
| **Custom Starter** | `acme-spring-boot-starter` | Custom / Community dependency bundle |
| **Inspect Tree** | `./mvnw dependency:tree` | Displays full transitive dependency graph |
| **Container Swap** | Exclude `starter-tomcat`, include `starter-undertow` | Replaces embedded web server |
| **Version Override** | `<tomcat.version>10.1.34</tomcat.version>` | Safely updates managed dependency |
| **BOM Import** | `spring-boot-dependencies` (`scope=import`)| Used when corporate parent is required |
| **Starter Architecture**| `*-autoconfigure` + `*-starter` | Standard two-module starter design |

---

## 13. Further Reading
- [Official Spring Boot Starters Documentation](https://docs.spring.io/spring-boot/docs/current/reference/html/using.html#using.build-systems.starters)
- [Spring Boot Dependency Versions List](https://docs.spring.io/spring-boot/docs/current/reference/html/dependency-versions.html)
- [Maven Dependency Mechanism & BOM Import](https://maven.apache.org/guides/introduction/introduction-to-dependency-mechanism.html)
