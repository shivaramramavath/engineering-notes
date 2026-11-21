# Maven and Gradle Build Tools Reference Cheat Sheet

> Complete operational guide and command reference for Maven and Gradle: build lifecycles, dependency management, scopes, plugins, Gradle Kotlin DSL, multi-module project structures, and CI/CD automation.

| Metadata | Details |
| :--- | :--- |
| **Level** | Comprehensive Reference |
| **Prerequisites** | [18-build-and-dependencies](../18-build-and-dependencies/README.md) |
| **Time to Read** | 40 minutes |
| **Status** | Complete |
| **Java Version** | Java 8 to 25 LTS |

---

## Table of Contents
1. [Standard Directory Layout](#1-standard-directory-layout)
2. [Maven Architecture & Core Build Lifecycles](#2-maven-architecture--core-build-lifecycles)
3. [Maven CLI Commands & Flag Matrix](#3-maven-cli-commands--flag-matrix)
4. [Maven Dependency Scopes & Resolution](#4-maven-dependency-scopes--resolution)
5. [Multi-Module Maven Architecture (Inheritance vs Aggregation)](#5-multi-module-maven-architecture-inheritance-vs-aggregation)
6. [Gradle Architecture & Core Phases](#6-gradle-architecture--core-phases)
7. [Gradle CLI Commands & Build Optimization](#7-gradle-cli-commands--build-optimization)
8. [Gradle Dependency Configurations Reference](#8-gradle-dependency-configurations-reference)
9. [Gradle Kotlin DSL (`build.gradle.kts`) Reference Template](#9-gradle-kotlin-dsl-buildgradlekts-reference-template)
10. [Custom Gradle Tasks & Incremental Caching](#10-custom-gradle-tasks--incremental-caching)
11. [Publishing Artifacts (Maven Central & Private Registries)](#11-publishing-artifacts-maven-central--private-registries)
12. [Maven vs Gradle Command Translation Matrix](#12-maven-vs-gradle-command-translation-matrix)
13. [Common Build Gotchas & Troubleshooting](#13-common-build-gotchas--troubleshooting)
14. [CI/CD Build Pipeline Recipes](#14-cicd-build-pipeline-recipes)

---

## 1. Standard Directory Layout

Both Maven and Gradle enforce the standard convention-over-configuration directory structure:

```
project-root/
├── pom.xml                     # Maven project descriptor
├── build.gradle.kts            # Gradle project descriptor (Kotlin DSL)
├── settings.gradle.kts         # Gradle multi-project and plugin settings
├── src/
│   ├── main/
│   │   ├── java/               # Application Java source files
│   │   ├── resources/          # Configuration files, properties, templates
│   │   └── webapp/             # Web application files (WAR projects)
│   └── test/
│       ├── java/               # Test Java source files (JUnit, TestNG)
│       └── resources/          # Test configuration files & mock data
└── target/                     # Maven output directory (bytecode, jars)
└── build/                      # Gradle output directory
```

---

## 2. Maven Architecture & Core Build Lifecycles

Maven defines three distinct, independent built-in lifecycles: **`default`**, **`clean`**, and **`site`**.

### The Default Lifecycle Phases (in execution order):
```
  validate -> compile -> test -> package -> verify -> install -> deploy
```
1. **`validate`:** Validates project structure and confirms all prerequisite information is available.
2. **`compile`:** Compiles the source code located in `src/main/java`.
3. **`test`:** Runs automated unit tests located in `src/test/java` using Surefire plugin.
4. **`package`:** Packages compiled code into distribution format (`.jar`, `.war`).
5. **`verify`:** Runs integration tests using Failsafe plugin to verify package quality.
6. **`install`:** Installs package into local repository (`~/.m2/repository`).
7. **`deploy`:** Copies the final package to a remote artifact repository (Nexus / Artifactory).

### Clean Lifecycle Phases:
- `pre-clean` -> `clean` (deletes `target/`) -> `post-clean`

---

## 3. Maven CLI Commands & Flag Matrix

| Command | Action | Recommended Context |
| :--- | :--- | :--- |
| `mvn clean compile` | Deletes target and recompiles source files | Fast local recompile |
| `mvn test` | Executes unit tests via Maven Surefire | Local testing |
| `mvn test -Dtest=OrderServiceTest` | Runs a single designated test class | Targeted unit test |
| `mvn test -Dtest=OrderServiceTest#testCreate` | Runs a single designated test method | Targeted method test |
| `mvn package -DskipTests` | Builds JAR while compiling but skipping test runs | Quick local packaging |
| `mvn package -Dmaven.test.skip=true` | Builds JAR without compiling or running tests | Fastest packaging |
| `mvn verify` | Runs unit tests, packaging, and integration tests | Pre-merge verification |
| `mvn install` | Packages and publishes artifact to local `~/.m2` | Local multi-repo testing |
| `mvn dependency:tree` | Displays full hierarchical dependency tree | Diagnosing dependency conflicts |
| `mvn dependency:analyze` | Identifies unused declared and undeclared used deps | Cleaning dependencies |

### Essential Maven CLI Flags
| Flag | Long Option | Description |
| :--- | :--- | :--- |
| `-T <N>` | `--threads` | Parallel multi-threaded build (e.g., `-T 1C` uses 1 thread per CPU core) |
| `-B` | `--batch-mode` | Non-interactive batch mode (removes terminal escape characters in CI) |
| `-U` | `--update-snapshots` | Forces remote download of updated SNAPSHOT dependencies |
| `-pl <mod>` | `--projects` | Builds only designated sub-modules in a multi-module project |
| `-am` | `--also-make` | Builds prerequisite dependencies required by `-pl` project |
| `-X` | `--debug` | Produces verbose debugging output |
| `-e` | `--errors` | Displays full exception stack traces on build failure |
| `-s <path>` | `--settings` | Custom user `settings.xml` file path |

```bash
# Optimal High-Speed CI Build Command:
mvn clean verify -B -T 1C -U -Dmaven.compile.fork=true
```

---

## 4. Maven Dependency Scopes & Resolution

| Scope | Included in `compile` Classpath? | Included in `test` Classpath? | Included in `runtime` Classpath? | Packaged into Final Artifact? |
| :--- | :---: | :---: | :---: | :---: |
| **`compile`** (Default) | **Yes** | **Yes** | **Yes** | **Yes** |
| **`provided`** | **Yes** | **Yes** | No | **No** (Provided by JDK or Servlet container) |
| **`runtime`** | No | **Yes** | **Yes** | **Yes** (e.g., JDBC driver implementations) |
| **`test`** | No | **Yes** | No | **No** (e.g., JUnit 5, Mockito, AssertJ) |
| **`system`** | **Yes** | **Yes** | No | **No** (Requires explicit local path; deprecated) |
| **`import`** | N/A | N/A | N/A | N/A (Used in `<dependencyManagement>` for BOMs) |

### Transitive Dependency Exclusion Idiom
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
```

---

## 5. Multi-Module Maven Architecture (Inheritance vs Aggregation)

### Parent POM (Inheritance via `<parent>`)
Centralizes plugin versions, dependency versions, and compiler options across all child modules using `<dependencyManagement>`:

```xml
<project>
    <modelVersion>4.0.0</modelVersion>
    <groupId>com.engineering</groupId>
    <artifactId>enterprise-parent</artifactId>
    <version>1.0.0-SNAPSHOT</version>
    <packaging>pom</packaging>

    <properties>
        <java.version>25</java.version>
        <maven.compiler.release>25</maven.compiler.release>
        <project.build.sourceEncoding>UTF-8</project.build.sourceEncoding>
    </properties>

    <dependencyManagement>
        <dependencies>
            <dependency>
                <groupId>org.junit</groupId>
                <artifactId>junit-bom</artifactId>
                <version>5.11.0</version>
                <type>pom</type>
                <scope>import</scope>
            </dependency>
        </dependencies>
    </dependencyManagement>
</project>
```

### Aggregator POM (Multi-Module Build via `<modules>`)
```xml
<project>
    <modelVersion>4.0.0</modelVersion>
    <artifactId>enterprise-aggregator</artifactId>
    <packaging>pom</packaging>

    <modules>
        <module>common-domain</module>
        <module>payment-service</module>
        <module>order-service</module>
        <module>api-gateway</module>
    </modules>
</project>
```

---

## 6. Gradle Architecture & Core Phases

Gradle evaluates projects in three distinct sequential phases:

```
[1. Initialization Phase] ---> [2. Configuration Phase] ---> [3. Execution Phase]
(Evaluates settings.gradle,      (Executes task configuration; (Executes designated task
 identifies all projects)       builds Task Execution Graph)   actions whose inputs changed)
```
1. **Initialization:** Determines which projects will participate in the build and instantiates `Project` objects.
2. **Configuration:** Evaluates `build.gradle.kts` files for *all* projects in the build graph. Task configuration blocks execute here!
3. **Execution:** Executes task action closures (`doLast`, `@TaskAction`) for tasks specified on the CLI.

---

## 7. Gradle CLI Commands & Build Optimization

| Command | Action | Recommended Context |
| :--- | :--- | :--- |
| `./gradlew build` | Assembles artifacts and runs tests | Standard full build |
| `./gradlew test` | Executes unit tests via test runner | Testing |
| `./gradlew test --tests OrderServiceTest` | Runs targeted test class | Targeted testing |
| `./gradlew assemble` | Compiles and packages JARs without running tests | Fast local packaging |
| `./gradlew clean` | Deletes the `build/` directory | Clean slate build |
| `./gradlew dependencies` | Prints detailed dependency resolution tree | Inspecting dependencies |
| `./gradlew tasks --all` | Lists all available tasks in project | Task discovery |

### Gradle Performance Flags
```bash
# 1. Parallel Task Execution across modules
./gradlew build --parallel

# 2. Build Cache (replaces tasks with cached results across branches and machines)
./gradlew build --build-cache

# 3. Offline Mode (avoids network metadata checks)
./gradlew build --offline

# 4. Continuous Build (watches source files and auto-rebuilds on file change)
./gradlew test --continuous
```

---

## 8. Gradle Dependency Configurations Reference

```
                   implementation
                  /              \
                 /                \
          api (java-library)       \
               /                    v
              v                 runtimeOnly
         compileOnly
```

| Configuration | Compile Classpath? | Runtime Classpath? | Exposed Transitively to Consumers? | Typical Use Case |
| :--- | :---: | :---: | :---: | :--- |
| **`implementation`** | **Yes** | **Yes** | **No** (Internal implementation detail) | Application dependencies (Guava, Apache Commons) |
| **`api`** | **Yes** | **Yes** | **Yes** (Types exposed in public API) | Shared library public interfaces |
| **`compileOnly`** | **Yes** | No | No | Compile-time annotations (Lombok) |
| **`runtimeOnly`** | No | **Yes** | No | Drivers & implementations (PostgreSQL driver) |
| **`testImplementation`**| **Yes** (Test) | **Yes** (Test) | No | Test libraries (JUnit 5, Mockito) |
| **`testRuntimeOnly`** | No | **Yes** (Test) | No | Test execution engines (`junit-jupiter-engine`) |

---

## 9. Gradle Kotlin DSL (`build.gradle.kts`) Reference Template

```kotlin
plugins {
    `java-library`
    application
    id("com.diffplug.spotless") version "6.25.0"
}

group = "com.engineering"
version = "1.0.0-SNAPSHOT"

java {
    toolchain {
        languageVersion.set(JavaLanguageVersion.of(25))
    }
}

repositories {
    mavenCentral()
}

dependencies {
    // API vs Implementation
    api("org.apache.commons:commons-lang3:3.14.0")
    implementation("com.fasterxml.jackson.core:jackson-databind:2.17.0")

    // Compile-time only annotations
    compileOnly("org.projectlombok:lombok:1.18.32")
    annotationProcessor("org.projectlombok:lombok:1.18.32")

    // Runtime database driver
    runtimeOnly("org.postgresql:postgresql:42.7.3")

    // Testing
    testImplementation(platform("org.junit:junit-bom:5.11.0"))
    testImplementation("org.junit.jupiter:junit-jupiter")
    testRuntimeOnly("org.junit.platform:junit-platform-launcher")
}

tasks.withType<JavaCompile> {
    options.encoding = "UTF-8"
    options.compilerArgs.addAll(listOf("-Xlint:all", "-Werror"))
}

tasks.test {
    useJUnitPlatform()
    maxParallelForks = (Runtime.getRuntime().availableProcessors() / 2).coerceAtLeast(1)
    testLogging {
        events("passed", "skipped", "failed")
    }
}
```

---

## 10. Custom Gradle Tasks & Incremental Caching

Gradle avoids re-executing tasks whose inputs and outputs have not changed (**Incremental Build - `UP-TO-DATE`**):

```kotlin
abstract class GenerateLicenseReport : DefaultTask() {
    @get:InputFile
    abstract val inputFile: RegularFileProperty

    @get:OutputFile
    abstract val outputFile: RegularFileProperty

    @TaskAction
    fun generate() {
        val input = inputFile.get().asFile.readText()
        outputFile.get().asFile.writeText("Generated Report from:\n$input")
    }
}

tasks.register<GenerateLicenseReport>("generateLicenseReport") {
    inputFile.set(layout.projectDirectory.file("dependencies.txt"))
    outputFile.set(layout.buildDirectory.file("reports/license-report.txt"))
}
```

---

## 11. Publishing Artifacts (Maven Central & Private Registries)

### Maven Central Publishing Plugin (`build.gradle.kts`)
```kotlin
publishing {
    publications {
        create<MavenPublication>("mavenJava") {
            from(components["java"])
            pom {
                name.set("Engineering Core")
                description.set("High-performance core enterprise utilities")
                url.set("https://github.com/engineering/core")
                licenses {
                    license {
                        name.set("The Apache License, Version 2.0")
                        url.set("http://www.apache.org/licenses/LICENSE-2.0.txt")
                    }
                }
            }
        }
    }
    repositories {
        maven {
            name = "Nexus"
            url = uri("https://nexus.internal.company.com/repository/maven-releases/")
            credentials {
                username = System.getenv("NEXUS_USER")
                password = System.getenv("NEXUS_PASSWORD")
            }
        }
    }
}
```

---

## 12. Maven vs Gradle Command Translation Matrix

| Development Goal | Maven Command | Gradle Command |
| :--- | :--- | :--- |
| **Recompile All** | `mvn clean compile` | `./gradlew clean compileJava` |
| **Run All Tests** | `mvn test` | `./gradlew test` |
| **Run Single Test Class** | `mvn test -Dtest=OrderTest` | `./gradlew test --tests OrderTest` |
| **Build JAR skipping tests** | `mvn package -DskipTests` | `./gradlew assemble` |
| **Full Build & Verify** | `mvn verify` | `./gradlew check` |
| **Install Locally** | `mvn install` | `./gradlew publishToMavenLocal` |
| **Print Dependency Tree** | `mvn dependency:tree` | `./gradlew dependencies` |
| **Parallel CI Build** | `mvn verify -T 1C -B` | `./gradlew check --parallel --build-cache` |
| **Offline Execution** | `mvn compile -o` | `./gradlew compileJava --offline` |

---

## 13. Common Build Gotchas & Troubleshooting

1. **Gradle Configuration-Time Execution Pitfall:** Placing operational code directly in the task block instead of inside `doLast { ... }` causes that code to execute during Gradle's configuration phase, slowing down *every* build task!
2. **Maven SNAPSHOT Cache Invalidation:** Maven caches SNAPSHOTs locally for 24 hours. Force fresh dependency checks using `-U` (`mvn clean compile -U`).
3. **Diamond Dependency Conflicts:** When library A requires `Jackson 2.12` and library B requires `Jackson 2.17`:
   - Maven applies **Nearest Definition Wins** rule (dependency declared nearest to root project wins).
   - Gradle applies **Newest Version Wins** rule (`2.17` is chosen).
4. **Leaking Transitive Dependencies:** In Gradle, always prefer `implementation` over `api` to minimize compilation classpath pollution and avoid unnecessary rebuild cascades.

---

## 14. CI/CD Build Pipeline Recipes

### GitHub Actions Maven Build Workflow
```yaml
name: Java CI with Maven
on: [push, pull_request]

jobs:
  build:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v4
      - name: Set up JDK 25
        uses: actions/setup-java@v4
        with:
          java-version: '25'
          distribution: 'oracle'
          cache: 'maven'
      - name: Build with Maven
        run: mvn clean verify -B -T 1C
```
