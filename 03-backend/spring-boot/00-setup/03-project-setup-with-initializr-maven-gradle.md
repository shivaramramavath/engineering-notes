# Project Setup with Spring Initializr, Maven & Gradle

> A comprehensive deep-dive into bootstrapping modern Spring Boot applications using start.spring.io, configuring Maven 3.9+ POMs and Gradle 8.10+ Kotlin DSL build scripts, managing bill of materials (BOM), and tailoring enterprise build wrappers.

| Metadata | Value |
| :--- | :--- |
| **Level** | Beginner |
| **Prerequisites** | [Java Language Foundations](file:///01-languages/java/), [Spring Ecosystem Overview](file:///03-backend/spring-boot/00-setup/01-spring-ecosystem-overview.md) |
| **Time to Read** | 25 minutes |
| **Status** | Complete |
| **Spring Boot Version** | 3.4.x / 4.x |
| **Spring Framework Version** | 6.2.x / 7.0.x |
| **Java Baseline** | Java 21 / 25 LTS |
| **Build Tool** | Maven 3.9+ / Gradle 8.10+ |
| **Tested With** | OpenJDK 25.0.1, Maven 3.9.12, Spring Boot 3.4.0 |

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
Configuring a Java enterprise build from scratch requires stitching together hundreds of compatible transitive dependencies, setting compiler flags, configuring repackaging plugins for executable fat JARs, and harmonizing build-time testing configurations. Manually managing versions across Jackson, Netty, Hibernate, Tomcat, and Micrometer inevitably introduces dependency divergence (`NoSuchMethodError`, `ClassNotFoundException`, runtime classpath collisions).

### Historical Evolution
- **Manual Ant / Ivy Builds (pre-2005)**: Libraries were manually dropped into `WEB-INF/lib` directories without automated dependency resolution.
- **Maven Central & Spring Framework BOMs (2005-2014)**: Developers wrote verbose Maven XML configurations, manually synchronizing compatible versions of Spring modules.
- **Spring Initializr & Spring Boot Starter Parent (2014-Present)**: Spring Boot introduced the Starter Parent and Bill of Materials (`spring-boot-dependencies`), curating tested, compatible versions of hundreds of third-party libraries alongside Spring modules. Spring Initializr (`start.spring.io`) standardized instant web and CLI generation of ready-to-run projects.

### Real-World Relevance
Understanding the inner mechanics of `spring-boot-starter-parent`, `spring-boot-dependencies`, Gradle's `org.springframework.boot` plugin, and build wrappers (`mvnw`, `gradlew`) is essential for maintaining reproducible builds across developer laptops, CI/CD pipelines, and container image generation.

---

## 2. Core Concepts

### 1. Spring Initializr (`start.spring.io`)
Spring Initializr is an open-source web application and REST API that generates production-ready project archives. It configures:
- Project coordinates (`groupId`, `artifactId`, `version`, `packaging`)
- Language and Java version (`javaVersion=21` or `25`)
- Selected starters (`web`, `data-jpa`, `security`, `actuator`, `testcontainers`)
- Verified directory layouts and executable wrappers (`mvnw`, `gradlew`).

### 2. Starter Parent vs. BOM Import
There are two distinct approaches to integrating Spring Boot into a build:
1. **Inheritance (`spring-boot-starter-parent`)**: The project POM declares `spring-boot-starter-parent` as its `<parent>`. This grants default plugin configurations (compiler target, resource filtering, `spring-boot-maven-plugin` repackaging), sensible default properties (`java.version`, UTF-8 encoding), and version management.
2. **Composition (`spring-boot-dependencies` BOM)**: When an enterprise project already has a company-wide corporate parent POM, Spring Boot cannot be inherited. Instead, `spring-boot-dependencies` is imported into `<dependencyManagement>` with `<scope>import</scope>`.

### 3. Build Tool Wrappers
Every production repository must check in the Maven Wrapper (`mvnw`, `mvnw.cmd`, `.mvn/wrapper/`) or Gradle Wrapper (`gradlew`, `gradlew.bat`, `gradle/wrapper/`). Wrappers guarantee that developers and CI build runners execute the exact specified version of the build tool without requiring pre-installed system binaries.

---

## 3. How It Works Internally

### Maven & Gradle Dependency Resolution & Repackaging Lifecycle

```
+-------------------------------------------------------------------------+
|                  Developer or CI Invocation (`./mvnw clean verify`)     |
+------------------------------------+------------------------------------+
                                     |
                                     v
+-------------------------------------------------------------------------+
|                     1. Dependency Resolution Phase                      |
|  - Read `spring-boot-starter-parent` or `spring-boot-dependencies` BOM  |
|  - Resolve transitive versions for declared starters                   |
|  - Detect exclusions and resolve version conflicts                      |
+------------------------------------+------------------------------------+
                                     |
                                     v
+-------------------------------------------------------------------------+
|                      2. Compilation & Testing Phase                     |
|  - javac compiler plugin (enforce Java 21/25 bytecode targets)          |
|  - Process resources & filter `@..@` tokens in application.properties   |
|  - Execute unit tests & Testcontainers integration tests                |
+------------------------------------+------------------------------------+
                                     |
                                     v
+-------------------------------------------------------------------------+
|                3. Spring Boot Repackaging (`package` goal)               |
|  - Standard `jar:jar` produces library JAR (classes + resources)        |
|  - `spring-boot-maven-plugin:repackage` executes:                       |
|    * Re-locates application classes to `BOOT-INF/classes/`              |
|    * Copies all dependency JARs to `BOOT-INF/lib/`                      |
|    * Writes custom Class-Path in `META-INF/MANIFEST.MF`                 |
|    * Sets Main-Class: `org.springframework.boot.loader.launch.JarLauncher`
|    * Sets Start-Class: `com.example.app.Application`                    |
+------------------------------------+------------------------------------+
                                     |
                                     v
+-------------------------------------------------------------------------+
|             Output Artifact: Self-Executing Fat JAR                     |
|                   `java -jar target/app-0.0.1-SNAPSHOT.jar`             |
+-------------------------------------------------------------------------+
```

---

## 4. Syntax & Usage

### Maven Starter Parent Configuration
```xml
<?xml version="1.0" encoding="UTF-8"?>
<project xmlns="http://maven.apache.org/POM/4.0.0"
         xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance"
         xsi:schemaLocation="http://maven.apache.org/POM/4.0.0 
         https://maven.apache.org/xsd/maven-4.0.0.xsd">
    <modelVersion>4.0.0</modelVersion>

    <parent>
        <groupId>org.springframework.boot</groupId>
        <artifactId>spring-boot-starter-parent</artifactId>
        <version>3.4.0</version>
        <relativePath/> <!-- lookup parent from repository -->
    </parent>

    <groupId>com.example</groupId>
    <artifactId>modern-service</artifactId>
    <version>0.0.1-SNAPSHOT</version>
    <name>modern-service</name>
    <description>Modern Spring Boot 3.4 Service</description>

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
            <artifactId>spring-boot-starter-test</artifactId>
            <scope>test</scope>
        </dependency>
    </dependencies>

    <build>
        <plugins>
            <plugin>
                <groupId>org.springframework.boot</groupId>
                <artifactId>spring-boot-maven-plugin</artifactId>
            </plugin>
        </plugins>
    </build>
</project>
```

### Gradle Kotlin DSL (`build.gradle.kts`)
```kotlin
plugins {
    java
    id("org.springframework.boot") version "3.4.0"
    id("io.spring.dependency-management") version "1.1.6"
}

group = "com.example"
version = "0.0.1-SNAPSHOT"

java {
    toolchain {
        languageVersion = JavaLanguageVersion.of(21)
    }
}

repositories {
    mavenCentral()
}

dependencies {
    implementation("org.springframework.boot:spring-boot-starter-web")
    implementation("org.springframework.boot:spring-boot-starter-actuator")
    testImplementation("org.springframework.boot:spring-boot-starter-test")
}

tasks.withType<Test> {
    useJUnitPlatform()
}
```

---

## 5. Practical Examples

### Example 1: Full-Featured Maven POM with Starters and Cloud Dependencies
```xml
<?xml version="1.0" encoding="UTF-8"?>
<project xmlns="http://maven.apache.org/POM/4.0.0"
         xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance"
         xsi:schemaLocation="http://maven.apache.org/POM/4.0.0 
         https://maven.apache.org/xsd/maven-4.0.0.xsd">
    <modelVersion>4.0.0</modelVersion>

    <parent>
        <groupId>org.springframework.boot</groupId>
        <artifactId>spring-boot-starter-parent</artifactId>
        <version>3.4.0</version>
        <relativePath/>
    </parent>

    <groupId>com.example.inventory</groupId>
    <artifactId>inventory-service</artifactId>
    <version>1.0.0-SNAPSHOT</version>

    <properties>
        <java.version>21</java.version>
        <spring-cloud.version>2024.0.0</spring-cloud.version>
        <testcontainers.version>1.20.4</testcontainers.version>
    </properties>

    <dependencyManagement>
        <dependencies>
            <dependency>
                <groupId>org.springframework.cloud</groupId>
                <artifactId>spring-cloud-dependencies</artifactId>
                <version>${spring-cloud.version}</version>
                <type>pom</type>
                <scope>import</scope>
            </dependency>
        </dependencies>
    </dependencyManagement>

    <dependencies>
        <!-- Web & Validation -->
        <dependency>
            <groupId>org.springframework.boot</groupId>
            <artifactId>spring-boot-starter-web</artifactId>
        </dependency>
        <dependency>
            <groupId>org.springframework.boot</groupId>
            <artifactId>spring-boot-starter-validation</artifactId>
        </dependency>

        <!-- Observability -->
        <dependency>
            <groupId>org.springframework.boot</groupId>
            <artifactId>spring-boot-starter-actuator</artifactId>
        </dependency>
        <dependency>
            <groupId>io.micrometer</groupId>
            <artifactId>micrometer-registry-prometheus</artifactId>
        </dependency>

        <!-- Testing -->
        <dependency>
            <groupId>org.springframework.boot</groupId>
            <artifactId>spring-boot-starter-test</artifactId>
            <scope>test</scope>
        </dependency>
        <dependency>
            <groupId>org.springframework.boot</groupId>
            <artifactId>spring-boot-testcontainers</artifactId>
            <scope>test</scope>
        </dependency>
        <dependency>
            <groupId>org.testcontainers</groupId>
            <artifactId>junit-jupiter</artifactId>
            <scope>test</scope>
        </dependency>
    </dependencies>

    <build>
        <plugins>
            <plugin>
                <groupId>org.springframework.boot</groupId>
                <artifactId>spring-boot-maven-plugin</artifactId>
                <configuration>
                    <image>
                        <builder>paketobuildpacks/builder-jammy-base:latest</builder>
                    </image>
                </configuration>
            </plugin>
        </plugins>
    </build>
</project>
```

### Example 2: Gradle Kotlin DSL Build with Version Catalogs
```kotlin
// settings.gradle.kts
rootProject.name = "catalog-service"

// build.gradle.kts
plugins {
    java
    id("org.springframework.boot") version "3.4.0"
    id("io.spring.dependency-management") version "1.1.6"
}

group = "com.example.catalog"
version = "1.0.0-SNAPSHOT"

java {
    toolchain {
        languageVersion = JavaLanguageVersion.of(21)
    }
}

repositories {
    mavenCentral()
}

dependencies {
    implementation("org.springframework.boot:spring-boot-starter-web")
    implementation("org.springframework.boot:spring-boot-starter-data-jpa")
    implementation("org.springframework.boot:spring-boot-starter-actuator")
    
    runtimeOnly("org.postgresql:postgresql")
    
    testImplementation("org.springframework.boot:spring-boot-starter-test")
    testImplementation("org.springframework.boot:spring-boot-testcontainers")
    testImplementation("org.testcontainers:postgresql")
}

tasks.withType<Test> {
    useJUnitPlatform()
}

tasks.bootJar {
    archiveFileName.set("catalog-service.jar")
}
```

### Example 3: Initializr CLI / cURL Script for Automated Project Generation
```bash
#!/usr/bin/env bash
# Programmatic project bootstrapping using start.spring.io REST API
set -euo pipefail

PROJECT_NAME="order-service"
PACKAGE_NAME="com.example.orders"

echo "Generating Spring Boot 3.4 project with Java 21..."

curl -s https://start.spring.io/starter.zip \
  -d type=maven-project \
  -d language=java \
  -d bootVersion=3.4.0 \
  -d baseDir="${PROJECT_NAME}" \
  -d groupId="com.example" \
  -d artifactId="${PROJECT_NAME}" \
  -d name="${PROJECT_NAME}" \
  -d description="High-Throughput Order Management Service" \
  -d packageName="${PACKAGE_NAME}" \
  -d packaging=jar \
  -d javaVersion=21 \
  -d dependencies=web,data-jpa,postgresql,actuator,validation,testcontainers,docker-compose \
  -o "${PROJECT_NAME}.zip"

unzip -q "${PROJECT_NAME}.zip"
rm "${PROJECT_NAME}.zip"

cd "${PROJECT_NAME}"
chmod +x ./mvnw
./mvnw clean compile
echo "Successfully bootstrapped and compiled ${PROJECT_NAME}!"
```

### Example 4: Build Failure Anti-Pattern & Remediation (Overriding Managed Versions)
```xml
<!-- ANTI-PATTERN: Manually hardcoding versions managed by Spring Boot causes version skew -->
<dependencies>
    <dependency>
        <groupId>org.springframework.boot</groupId>
        <artifactId>spring-boot-starter-web</artifactId>
    </dependency>
    <!-- BROKEN: Hardcoding Jackson or Hibernate causes ClassNotFoundException / NoSuchMethodError -->
    <dependency>
        <groupId>com.fasterxml.jackson.core</groupId>
        <artifactId>jackson-databind</artifactId>
        <version>2.14.0</version> <!-- CONFLICT: Boot 3.4 expects 2.18+ -->
    </dependency>
</dependencies>

<!-- REMEDIATION: Omit the <version> tag completely or override via designated property -->
<properties>
    <!-- Safe mechanism if an emergency CVE patch is required -->
    <jackson-bom.version>2.18.2</jackson-bom.version>
</properties>
<dependencies>
    <dependency>
        <groupId>com.fasterxml.jackson.core</groupId>
        <artifactId>jackson-databind</artifactId>
        <!-- Version automatically resolved from jackson-bom.version property in parent -->
    </dependency>
</dependencies>
```

### Example 5: Enterprise Multi-Module Maven Build Layout
```xml
<!-- Root pom.xml -->
<project xmlns="http://maven.apache.org/POM/4.0.0">
    <modelVersion>4.0.0</modelVersion>
    <parent>
        <groupId>org.springframework.boot</groupId>
        <artifactId>spring-boot-starter-parent</artifactId>
        <version>3.4.0</version>
        <relativePath/>
    </parent>

    <groupId>com.example.enterprise</groupId>
    <artifactId>platform-parent</artifactId>
    <version>1.0.0-SNAPSHOT</version>
    <packaging>pom</packaging>

    <modules>
        <module>common-domain</module>
        <module>order-service</module>
        <module>billing-service</module>
    </modules>

    <properties>
        <java.version>21</java.version>
    </properties>

    <!-- Shared dependency management across submodules -->
    <dependencyManagement>
        <dependencies>
            <dependency>
                <groupId>com.example.enterprise</groupId>
                <artifactId>common-domain</artifactId>
                <version>${project.version}</version>
            </dependency>
        </dependencies>
    </dependencyManagement>
</project>
```

---

## 6. Best Practices

1. **Always Check In Wrapper Binaries**: Check `mvnw`, `mvnw.cmd`, `.mvn/wrapper/maven-wrapper.properties` (and corresponding Gradle files) into git. Never force engineers to install local CLI runtimes.
2. **Never Specify Versions on Managed Starters**: Let `spring-boot-starter-parent` or `spring-boot-dependencies` control dependency versions for all standard libraries.
3. **Use Java Toolchains**: In Gradle or Maven Compiler Plugin, declare explicit toolchains targeting Java 21 or 25 to guarantee deterministic compilation regardless of host `JAVA_HOME`.
4. **Use Property Overrides for CVE Hotfixes**: If a transitive dependency has a vulnerability, override its version via `<name.version>` property (e.g., `<tomcat.version>10.1.34</tomcat.version>`) rather than declaring direct dependencies everywhere.
5. **Enforce Reproducible Builds**: Enable `project.build.outputTimestamp` in Maven properties to ensure byte-for-byte identical JAR output across builds.
6. **Exclude `spring-boot-maven-plugin` Execution from Pure Library Modules**: Multi-module builds should only run the `repackage` goal in executable application modules, not shared domain/library jars.

---

## 7. Common Mistakes & Pitfalls

| Mistake | Why It Fails | Practical Remedy |
| :--- | :--- | :--- |
| **Hardcoding dependency versions** | Overriding versions for libraries like Jackson, Hibernate, or Tomcat causes classpath incompatibility and NoSuchMethodError. | Remove `<version>` tags for any artifact included in `spring-boot-dependencies`. |
| **Omitting build wrappers in git** | CI pipelines or team members with different Maven/Gradle versions face build deviations or plugin crashes. | Commit `.mvn/wrapper` and `mvnw` (or `gradle/wrapper` and `gradlew`) to version control. |
| **Repackaging shared library JARs** | `spring-boot-maven-plugin` repackages JARs with `BOOT-INF/classes`, making classes invisible when imported as a dependency in other modules. | In library modules, disable repackaging or add `<classifier>exec</classifier>`. |
| **Using `spring-boot-starter-web` alongside `spring-boot-starter-webflux`** | By default, Spring Boot prioritizes the Servlet stack (Tomcat) over Netty, leading to subtle reactive runtime mismatches. | Choose one stack. For reactive, use WebFlux alone; for virtual-thread blocking, use Web. |
| **Incorrect token filtering syntax** | Spring Boot uses `@property.name@` for resource filtering in Maven rather than standard `${property.name}` to avoid clashes. | Use `@project.version@` inside `application.properties` or `application.yaml`. |
| **Missing test scope on Testcontainers** | Production fat JAR balloons in size and packages container orchestration code into production artifacts. | Ensure all test-specific starters and Testcontainers dependencies declare `<scope>test</scope>`. |

---

## 8. Performance & Security Notes

### Build Performance
- **Maven Daemon (`mvnd`)**: Accelerates build execution by up to 5x using background JVM workers and GraalVM-compiled native clients.
- **Gradle Configuration Cache**: Reuses the configuration phase across repeated builds, dramatically reducing task execution latency.
- **Dependency Download Caching**: Configure CI pipelines (GitHub Actions, GitLab CI) to cache `~/.m2/repository` or `~/.gradle/caches`.

### Security Hardening
- **OWASP Dependency-Check Plugin**: Integrate into CI pipelines to detect known Common Vulnerabilities and Exposures (CVEs) during the build:
  ```xml
  <plugin>
      <groupId>org.owasp</groupId>
      <artifactId>dependency-check-maven</artifactId>
      <version>10.0.4</version>
      <executions>
          <execution>
              <goals><goal>check</goal></goals>
          </execution>
      </executions>
  </plugin>
  ```
- **Software Bill of Materials (SBOM)**: Use CycloneDX plugin (`cyclonedx-maven-plugin`) to emit standard CycloneDX JSON SBOMs with each build artifact.

---

## 9. Comparison / When to Use What

| Feature / Criteria | Maven (`pom.xml`) | Gradle (`build.gradle.kts`) |
| :--- | :--- | :--- |
| **Configuration Style** | Declarative XML (Rigid, predictable lifecycle) | Imperative / Declarative Kotlin DSL (Turing-complete) |
| **Adoption in Enterprise** | ~70% dominant in enterprise banking and healthcare | Common in modern cloud-native, Android, and polyglot teams |
| **Build Speed (Incremental)** | Standard (Faster with `mvnd`) | Exceptional (Advanced caching, daemon, configuration cache) |
| **Learning Curve** | Low; highly standardized convention | Moderate; requires familiarity with Gradle lifecycle and DSL |
| **Multi-Module Ergonomics** | Rigid `<modules>` hierarchy, verbose | Flexible projects, version catalogs (`libs.versions.toml`) |
| **Recommended Choice** | Default for enterprise microservices & stability | Best for complex custom pipelines and ultra-fast CI loops |

---

## 10. Interview Questions

<details>
<summary><strong>Q1: What is the purpose of `spring-boot-starter-parent` and what does it provide?</strong></summary>

**Answer:**  
`spring-boot-starter-parent` is an opinionated parent POM designed to simplify build configurations. It provides:
1. **Dependency Management**: Inherits `spring-boot-dependencies`, managing versions for hundreds of compatible libraries.
2. **Default Plugin Configurations**: Pre-configures plugins such as `maven-compiler-plugin` (Java 21 bytecode targets), `maven-surefire-plugin`, and `spring-boot-maven-plugin`.
3. **Sensible Defaults**: UTF-8 source encoding, resource filtering for `application.properties`/`application.yaml` using `@..@` delimiters.
4. **Executable Packaging**: Automatically binds the `repackage` goal of `spring-boot-maven-plugin` to the `package` phase.
</details>

<details>
<summary><strong>Q2: How do you use Spring Boot if your organization already requires a custom corporate parent POM?</strong></summary>

**Answer:**  
Because Maven only allows single parent inheritance, you cannot inherit `spring-boot-starter-parent`. Instead, import `spring-boot-dependencies` into the `<dependencyManagement>` section with `<type>pom</type>` and `<scope>import</scope>`:
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
You must also explicitly configure the `spring-boot-maven-plugin` and set the `<java.version>` property manually.
</details>

<details>
<summary><strong>Q3: How does the Spring Boot Fat JAR execute at runtime?</strong></summary>

**Answer:**  
A Spring Boot executable JAR does not run your `main` class directly via the JVM system classloader. Instead:
1. In `META-INF/MANIFEST.MF`, `Main-Class` is set to `org.springframework.boot.loader.launch.JarLauncher`.
2. `Start-Class` is set to your application class (e.g., `com.example.app.Application`).
3. When `java -jar app.jar` is invoked, `JarLauncher` initializes a custom `LaunchedURLClassLoader` that understands nested JAR archives inside `BOOT-INF/lib/` and classes inside `BOOT-INF/classes/`.
4. It delegates execution to the `main()` method of the `Start-Class`.
</details>

<details>
<summary><strong>Q4: Why does Spring Boot use `@property.name@` instead of `${property.name}` for Maven resource filtering?</strong></summary>

**Answer:**  
Spring Boot uses the `@` delimiter to prevent syntax collisions between Maven's build-time property replacement and Spring's runtime property placeholder resolution (`${...}`). If Maven filtered `${property}`, expressions intended for runtime resolution (like `${PORT:8080}`) would be erased or evaluated at compile time by Maven.
</details>

<details>
<summary><strong>Q5: What is a Spring Boot Starter and what is inside it?</strong></summary>

**Answer:**  
A Starter is an empty or near-empty coordination artifact that bundles transitive dependencies and metadata needed to enable a specific capability. For example, `spring-boot-starter-web` contains no business logic code; its POM aggregates `spring-boot-starter`, `spring-web`, `spring-webmvc`, `spring-boot-starter-json` (Jackson), and `spring-boot-starter-tomcat`.
</details>

<details>
<summary><strong>Q6: How can you safely upgrade a single transitive dependency (e.g., Tomcat) without changing the Spring Boot version?</strong></summary>

**Answer:**  
In `pom.xml`, define the version property managed by the parent POM:
```xml
<properties>
    <tomcat.version>10.1.34</tomcat.version>
</properties>
```
Because the parent POM defines `<version>${tomcat.version}</version>` inside its dependency management, overriding this property upgrades Tomcat across all transitive dependencies consistently.
</details>

<details>
<summary><strong>Q7: What is the purpose of the `.mvn/wrapper/maven-wrapper.properties` file?</strong></summary>

**Answer:**  
It contains the metadata that controls the Maven Wrapper, specifically `distributionUrl`, which specifies the exact version of the Apache Maven binary distribution to download and cache in `~/.m2/wrapper/dists/` if not already present on the host.
</details>

<details>
<summary><strong>Q8: What is the difference between `bootJar` and standard `jar` tasks in Gradle?</strong></summary>

**Answer:**  
The standard `jar` task produces a thin library JAR containing only compiled project classes and resources. The `bootJar` task creates the executable fat JAR including the Spring Boot loader and all nested dependency JARs. By default, applying the `org.springframework.boot` plugin enables `bootJar` and disables `jar`.
</details>

---

## 11. Practice Exercises

<details>
<summary><strong>Exercise 1: Inspect Fat JAR Structure (Easy)</strong></summary>

**Task:**  
Build a Spring Boot application using `./mvnw clean package`. Use `jar -tf target/*.jar` or `unzip -l target/*.jar` to inspect the contents.  
**Verification:**  
Verify the presence of:
- `META-INF/MANIFEST.MF`
- `org/springframework/boot/loader/`
- `BOOT-INF/classes/`
- `BOOT-INF/lib/`
Confirm that `Main-Class` in the manifest points to `JarLauncher`.
</details>

<details>
<summary><strong>Exercise 2: Generate Project via cURL (Easy)</strong></summary>

**Task:**  
Write a one-line terminal command using `curl` against `https://start.spring.io/starter.tgz` that outputs an extracted Gradle Kotlin project with Java 21, Web, and Actuator dependencies.  
**Solution:**  
```bash
curl https://start.spring.io/starter.tgz \
  -d type=gradle-project-kotlin \
  -d javaVersion=21 \
  -d dependencies=web,actuator | tar -xzvf -
```
</details>

<details>
<summary><strong>Exercise 3: Configure Corporate Parent with BOM Import (Medium)</strong></summary>

**Task:**  
Create a Maven POM that does NOT inherit from `spring-boot-starter-parent`. Use `<dependencyManagement>` with `spring-boot-dependencies` BOM and configure `spring-boot-maven-plugin` with execution goals to produce a working fat JAR.  
**Verification:**  
Run `./mvnw clean package` and confirm that an executable fat JAR is produced in `target/`.
</details>

<details>
<summary><strong>Exercise 4: Emergency Transitive Dependency Override (Medium)</strong></summary>

**Task:**  
Check the dependency tree of your project using `./mvnw dependency:tree`. Identify the Jackson Databind version. Add a `<properties>` override to bump the Jackson version to a newer release, and verify via `dependency:tree` that all transitive Jackson modules updated in unison.
</details>

<details>
<summary><strong>Exercise 5: Multi-Module Project with Executable vs Library Submodules (Hard)</strong></summary>

**Task:**  
Configure a two-module Maven project:
1. `core-model`: Domain records and business logic. Must produce a standard library JAR.
2. `api-server`: Web service dependent on `core-model`. Must produce an executable Spring Boot Fat JAR.  
Ensure `core-model` does not have `repackage` enabled so that `api-server` can consume its classes.
</details>

<details>
<summary><strong>Exercise 6: Gradle Version Catalog Migration (Hard)</strong></summary>

**Task:**  
Create a `gradle/libs.versions.toml` file. Declare versions, libraries, and bundles for Spring Boot, PostgreSQL, and Testcontainers. Update `build.gradle.kts` to reference dependencies via the generated type-safe `libs` accessors.
</details>

---

## 12. Summary / Cheat Sheet

| Command / Configuration | Purpose | Example / Notes |
| :--- | :--- | :--- |
| `./mvnw clean package` | Build executable fat JAR with Maven | Output located in `target/*.jar` |
| `./gradlew bootJar` | Build executable fat JAR with Gradle | Output located in `build/libs/*.jar` |
| `./mvnw dependency:tree` | Display full resolved dependency graph | Essential for debugging version conflicts |
| `./gradlew dependencies --configuration runtimeClasspath` | Display Gradle dependency tree | Shows resolved vs requested versions |
| `spring-boot-starter-parent` | Maven parent POM | Provides BOM + compiler + plugin defaults |
| `spring-boot-dependencies` | Reusable BOM for `<dependencyManagement>` | Used when parent POM cannot be inherited |
| `mvnw wrapper:wrapper -Dmaven=3.9.9` | Upgrade Maven Wrapper version | Updates `.mvn/wrapper/maven-wrapper.properties` |
| `gradle wrapper --gradle-version 8.10.2` | Upgrade Gradle Wrapper version | Updates `gradle/wrapper/gradle-wrapper.properties` |

---

## 13. Further Reading
- [Official Spring Boot Maven Plugin Documentation](https://docs.spring.io/spring-boot/docs/current/maven-plugin/reference/htmlsingle/)
- [Official Spring Boot Gradle Plugin Documentation](https://docs.spring.io/spring-boot/docs/current/gradle-plugin/reference/htmlsingle/)
- [Spring Initializr API Reference](https://start.spring.io)
- [Maven Bill of Materials (BOM) Mechanics](https://maven.apache.org/guides/introduction/introduction-to-dependency-mechanism.html)
