# Spring Boot Build Tools: Maven & Gradle Cheatsheet

| Metadata | Details |
|---|---|
| **Topic** | Maven vs Gradle Build Automation for Spring Boot |
| **Track** | Java Spring Boot |
| **Difficulty** | Intermediate |
| **Prerequisites** | Java 21, Build Automation Basics |
| **Java Version** | Java 21 / 25 LTS |
| **Spring Boot** | 3.4.x / 4.x |
| **Tested With** | Maven 3.9+, Gradle 8.10+ |

---

## Table of Contents
1. [What & Why](#1-what--why)
2. [Core Concepts](#2-core-concepts)
3. [Internals & Architecture](#3-internals--architecture)
4. [Syntax & Categorized Reference](#4-syntax--categorized-reference)
5. [5 Practical Production Examples](#5-5-practical-production-examples)
6. [Best Practices](#6-best-practices)
7. [Common Mistakes to Avoid](#7-common-mistakes-to-avoid)
8. [Performance & Security Considerations](#8-performance--security-considerations)
9. [Comparison Table: Maven vs Gradle Kotlin DSL](#9-comparison-table-maven-vs-gradle-kotlin-dsl)
10. [Technical Interview Questions & Answers](#10-technical-interview-questions--answers)
11. [Hands-On Practice Exercises](#11-hands-on-practice-exercises)
12. [Summary & Cheat Sheet](#12-summary--cheat-sheet)
13. [Further Reading & References](#13-further-reading--references)

---

## 1. What & Why
Spring Boot applications are packaged and managed primarily via Apache Maven or Gradle. Both build tools coordinate transitive dependencies via the Spring Boot Bill of Materials (BOM), execute Ahead-of-Time (AOT) GraalVM compilation, generate layered container JARs, and provide CLI developer tooling (`./mvnw spring-boot:run` or `./gradlew bootRun`).

This cheatsheet provides side-by-side syntax comparisons, plugin configurations, multi-module setups, and dependency resolution techniques.

---

## 2. Core Concepts
- **`spring-boot-dependencies` BOM**: Defines curated, compatible versions for hundreds of third-party libraries (Jackson, Hibernate, Netty, Flyway).
- **Spring Boot Plugin**: Repackages compiled `.class` files into executable Fat/Layered JARs with custom classloader roots (`JarLauncher`).
- **AOT Processing**: Executes build-time reflection hints and proxy generation for GraalVM native images.
- **Dependency Scope Mapping**: Maven scopes (`compile`, `provided`, `runtime`, `test`) vs Gradle configurations (`implementation`, `compileOnly`, `runtimeOnly`, `testImplementation`).
- **Build Cache**: Incremental compilation and task caching to minimize CI/CD build durations.

---

## 3. Internals & Architecture
Spring Boot Repackaging Pipeline in Maven and Gradle:

```
[Java Source Files (.java)]
            |
            v
+-------------------------------------------------------------+
|               Compiler Task (javac / Java 21)               |
|  - Compiles to target/classes or build/classes/java/main    |
+-------------------------------------------------------------+
            |
            v
+-------------------------------------------------------------+
|                  Spring Boot Repackage Goal                 |
|  (spring-boot-maven-plugin / bootJar)                       |
|  1. Discovers Main-Class via Bytecode Inspection            |
|  2. Injects org.springframework.boot.loader infrastructure   |
|  3. Organizes archive into layered layout:                  |
|     - BOOT-INF/classes                                      |
|     - BOOT-INF/lib (Transitive dependencies)                |
|     - BOOT-INF/classpath.idx & layers.idx                   |
+-------------------------------------------------------------+
            |
            v
[Executable Layered Archive: app.jar]
```

---

## 4. Syntax & Categorized Reference

### 4.1 Dependency Scope Translation Reference
| Dependency Need | Apache Maven Scope | Gradle Configuration |
|---|---|---|
| Core library (Jackson, Lombok utils) | `<scope>compile</scope>` (Default) | `implementation` |
| Annotations only (Lombok, MapStruct) | `<scope>provided</scope>` | `compileOnly` + `annotationProcessor` |
| Runtime-only driver (Postgres, H2) | `<scope>runtime</scope>` | `runtimeOnly` |
| Test framework (JUnit 5, Mockito) | `<scope>test</scope>` | `testImplementation` |
| Test runtime (Testcontainers DB) | `<scope>test</scope>` | `testRuntimeOnly` |

### 4.2 CLI Command Equivalence
| Task | Maven Wrapper | Gradle Wrapper |
|---|---|---|
| Run locally | `./mvnw spring-boot:run` | `./gradlew bootRun` |
| Clean build & test | `./mvnw clean verify` | `./gradlew clean check` |
| Build JAR without tests | `./mvnw clean package -DskipTests` | `./gradlew bootJar -x test` |
| Dependency tree | `./mvnw dependency:tree` | `./gradlew dependencies` |
| Compile GraalVM Native | `./mvnw -Pnative native:compile` | `./gradlew nativeCompile` |

---

## 5. 5 Practical Production Examples

### Example 1: Production Maven `pom.xml` with Starters & Plugins
```xml
<?xml version="1.0" encoding="UTF-8"?>
<project xmlns="http://maven.apache.org/POM/4.0.0"
         xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance"
         xsi:schemaLocation="http://maven.apache.org/POM/4.0.0 https://maven.apache.org/xsd/maven-4.0.0.xsd">
    <modelVersion>4.0.0</modelVersion>

    <parent>
        <groupId>org.springframework.boot</groupId>
        <artifactId>spring-boot-starter-parent</artifactId>
        <version>3.4.0</version>
        <relativePath/>
    </parent>

    <groupId>com.example</groupId>
    <artifactId>order-service</artifactId>
    <version>1.0.0</version>

    <properties>
        <java.version>21</java.version>
    </properties>

    <dependencies>
        <dependency>
            <groupId>org.springframework.boot</groupId>
            <artifactId>spring-boot-starter-web</artifactId>
        </dependency>
        <dependency>
            <groupId>org.springframework.boot</groupId>
            <artifactId>spring-boot-starter-data-jpa</artifactId>
        </dependency>
        <dependency>
            <groupId>org.postgresql</groupId>
            <artifactId>postgresql</artifactId>
            <scope>runtime</scope>
        </dependency>
        <dependency>
            <groupId>org.springframework.boot</groupId>
            <artifactId>spring-boot-starter-test</artifactId>
            <scope>test</scope>
        </dependency>
    </dependencies>

    <build>
        <plugins>
            <plugin>
                <groupId>org.springframework.boot</groupId>
                <artifactId>spring-boot-maven-plugin</artifactId>
                <configuration>
                    <layers>
                        <enabled>true</enabled>
                    </layers>
                </configuration>
            </plugin>
        </plugins>
    </build>
</project>
```

### Example 2: Production Gradle Kotlin DSL `build.gradle.kts`
```kotlin
plugins {
    java
    id("org.springframework.boot") version "3.4.0"
    id("io.spring.dependency-management") version "1.1.6"
}

group = "com.example"
version = "1.0.0"

java {
    toolchain {
        languageVersion.set(JavaLanguageVersion.of(21))
    }
}

repositories {
    mavenCentral()
}

dependencies {
    implementation("org.springframework.boot:spring-boot-starter-web")
    implementation("org.springframework.boot:spring-boot-starter-data-jpa")
    runtimeOnly("org.postgresql:postgresql")
    testImplementation("org.springframework.boot:spring-boot-starter-test")
}

tasks.withType<Test> {
    useJUnitPlatform()
}

tasks.bootJar {
    layered {
        enabled.set(true)
    }
}
```

### Example 3: Importing Spring Boot BOM without Parent POM (Maven)
```xml
<dependencyManagement>
    <dependencies>
        <dependency>
            <groupId>org.springframework.boot</groupId>
            <artifactId>spring-boot-dependencies</artifactId>
            <version>3.4.0</version>
            <type>pom</type>
            <scope>import</scope>
        </dependency>
    </dependencies>
</dependencyManagement>
```

### Example 4: Annotation Processor Configuration (MapStruct & Lombok)
```xml
<!-- Maven Compiler Plugin Configuration -->
<plugin>
    <groupId>org.apache.maven.plugins</groupId>
    <artifactId>maven-compiler-plugin</artifactId>
    <version>3.13.0</version>
    <configuration>
        <source>21</source>
        <target>21</target>
        <annotationProcessorPaths>
            <path>
                <groupId>org.projectlombok</groupId>
                <artifactId>lombok</artifactId>
                <version>1.18.34</version>
            </path>
            <path>
                <groupId>org.mapstruct</groupId>
                <artifactId>mapstruct-processor</artifactId>
                <version>1.6.2</version>
            </path>
        </annotationProcessorPaths>
    </configuration>
</plugin>
```

### Example 5: Gradle Multi-Project Root `settings.gradle.kts`
```kotlin
rootProject.name = "enterprise-platform"

include("common-domain")
include("order-service")
include("payment-service")
```

---

## 6. Best Practices
1. **Always Use the Wrapper (`mvnw` / `gradlew`)**: Ensures all developers and CI/CD pipelines use identical build tool versions without local pre-installation.
2. **Never Hardcode Managed Dependency Versions**: Omit `<version>` tags for dependencies managed by the Spring Boot BOM to prevent version incompatibilities.
3. **Enable Layered JAR Packaging**: Keep `<layers><enabled>true</enabled></layers>` enabled for optimal Docker image layer caching.
4. **Use Java Toolchains**: Declare Java 21 via toolchain blocks so the build decouples from the ambient host JDK version.
5. **Separate Annotation Processors**: Explicitly declare `annotationProcessorPaths` to prevent processors from polluting the runtime classpath.

---

## 7. Common Mistakes to Avoid

| Mistake | Consequence | Remedy |
|---|---|---|
| Explicitly specifying versions on starter dependencies | Overrides Spring Boot BOM; causes `NoSuchMethodError` at runtime | Remove explicit `<version>` tags from managed dependencies |
| Omitting `spring-boot-maven-plugin` | Generated JAR is a standard library JAR; throws `No main manifest attribute` | Add `spring-boot-maven-plugin` to `<build><plugins>` |
| Committing local `.mvn/` or `.gradle/` caches | Bloats repository with binary cache files | Add build tool cache paths to `.gitignore` |
| Using `compile` instead of `implementation` in Gradle | Leaks transitive internal types into consumer classpaths | Use `implementation` for encapsulation |
| Missing `testImplementation` scope on test dependencies | Test classes and mockito libraries leak into production fat JAR | Ensure test dependencies use test scopes |
| Bypassing the wrapper with raw `mvn` or `gradle` | Builds fail due to unexpected tool versions across developer machines | Always execute `./mvnw` or `./gradlew` |

---

## 8. Performance & Security Considerations
- **Gradle Build Cache & Daemon**: Enable `--build-cache` and `org.gradle.parallel=true` in `gradle.properties` for 50%+ faster build times.
- **OWASP Dependency-Check**: Integrate `dependency-check-maven` to scan third-party dependencies for known CVEs.
- **Reproducible Builds**: Configure `project.build.outputTimestamp` in Maven for bit-for-bit reproducible JAR artifacts.

---

## 9. Comparison Table: Maven vs Gradle Kotlin DSL

| Metric | Apache Maven | Gradle (Kotlin DSL) |
|---|---|---|
| **Configuration Format** | XML (`pom.xml`) | Kotlin Code (`build.gradle.kts`) |
| **Build Speed** | Predictable, sequential/parallel | Very fast (Incremental, build cache) |
| **Extensibility** | Plugin-based, strict lifecycle | Highly programmable, custom tasks |
| **Ecosystem Dominance** | Enterprise standard, high adoption | Modern standard, high adoption |
| **IDE Auto-completion** | Schema-driven | Type-safe Kotlin DSL auto-completion |

---

## 10. Technical Interview Questions & Answers

<details>
<summary>1. What is the role of `spring-boot-starter-parent` in a Maven POM?</summary>
It provides default plugin configurations (e.g. `maven-compiler-plugin` targeting Java 21, `maven-surefire-plugin`), UTF-8 encoding, resource filtering, and imports `spring-boot-dependencies` BOM for dependency management.
</details>

<details>
<summary>2. How do you use Spring Boot in a Maven project that already has a custom parent POM?</summary>
Import `spring-boot-dependencies` inside the `<dependencyManagement>` section with `<type>pom</type>` and `<scope>import</scope>`, and add `spring-boot-maven-plugin` explicitly.
</details>

<details>
<summary>3. What does `spring-boot-maven-plugin:repackage` do?</summary>
It takes the compiled `.jar` file produced by `maven-jar-plugin` and rewrites it into an executable archive, moving application classes to `BOOT-INF/classes` and adding Spring's `JarLauncher`.
</details>

<details>
<summary>4. What is the difference between `implementation` and `api` in Gradle?</summary>
`implementation` hides transitive dependencies from consumers, improving compilation speed and encapsulation. `api` exposes transitive dependencies to consumers on their compile classpath.
</details>

<details>
<summary>5. How does Spring Boot manage third-party dependency versions?</summary>
Via the Spring Boot BOM (`spring-boot-dependencies`), which curates tested, compatible versions of hundreds of libraries (e.g. Jackson, Hibernate, Micrometer) so developers omit version numbers.
</details>

<details>
<summary>6. How do you configure GraalVM Native Image compilation in Maven?</summary>
Add the `native-maven-plugin` from GraalVM or activate the `native` profile provided by `spring-boot-starter-parent`: `./mvnw -Pnative native:compile`.
</details>

<details>
<summary>7. What does the Maven Wrapper (`mvnw`) contain?</summary>
It contains a small shell script (`mvnw`), a Windows batch file (`mvnw.cmd`), and `.mvn/wrapper/maven-wrapper.properties` referencing the exact Apache Maven binary to download and execute.
</details>

<details>
<summary>8. How do you view dependency conflicts in Gradle?</summary>
Run `./gradlew dependencies --configuration runtimeClasspath` or use `./gradlew dependencyInsight --dependency <name>`.
</details>

---

## 11. Hands-On Practice Exercises

<details>
<summary>Exercise 1: Convert a Maven POM to Gradle Kotlin DSL</summary>
Take a standard Spring Boot Web + JPA Maven POM and rewrite it as a `build.gradle.kts` file.
</details>

<details>
<summary>Exercise 2: Setup BOM import without parent POM</summary>
Configure a Maven POM that imports the Spring Boot BOM in `<dependencyManagement>`.
</details>

<details>
<summary>Exercise 3: Configure MapStruct and Lombok processors</summary>
Configure the Maven Compiler Plugin with MapStruct and Lombok annotation processors in proper order.
</details>

<details>
<summary>Exercise 4: Enable layered packaging in Gradle</summary>
Configure `tasks.bootJar` in `build.gradle.kts` to enable layered archives.
</details>

<details>
<summary>Exercise 5: Inspect dependency hierarchy</summary>
Run `./mvnw dependency:tree` and analyze transitive dependencies introduced by `spring-boot-starter-web`.
</details>

<details>
<summary>Exercise 6: Configure Java 21 Toolchain</summary>
Define a Java toolchain in Gradle targeting Java 21 and verify compilation.
</details>

---

## 12. Summary & Cheat Sheet

```
+---------------------------------------------------------------------------------+
|                    MAVEN & GRADLE CHEAT SHEET                                   |
+---------------------------------------------------------------------------------+
| RUN APP        | ./mvnw spring-boot:run           | ./gradlew bootRun           |
| BUILD JAR      | ./mvnw clean package             | ./gradlew bootJar           |
| DEP TREE       | ./mvnw dependency:tree           | ./gradlew dependencies      |
| SKIP TESTS     | -DskipTests                      | -x test                     |
| CORE DEP       | <dependency> (compile default)   | implementation(...)         |
| TEST DEP       | <scope>test</scope>              | testImplementation(...)     |
| RUNTIME DEP    | <scope>runtime</scope>           | runtimeOnly(...)            |
| BOM IMPORT     | <scope>import</scope><type>pom   | platform(...) / BOM plugin  |
+---------------------------------------------------------------------------------+
```

---

## 13. Further Reading & References
- [Spring Boot Maven Plugin Documentation](https://docs.spring.io/spring-boot/docs/current/maven-plugin/reference/htmlsingle/)
- [Spring Boot Gradle Plugin Documentation](https://docs.spring.io/spring-boot/docs/current/gradle-plugin/reference/htmlsingle/)
- [Gradle Kotlin DSL Primer](https://docs.gradle.org/current/userguide/kotlin_dsl.html)
- [Apache Maven Official Guide](https://maven.apache.org/guides/)
