# IDE Configuration & Spring Boot DevTools

> Master developer productivity with IntelliJ IDEA, Eclipse, and VS Code, and uncover the internal mechanics of `spring-boot-devtools`—including two-classloader restart architecture, LiveReload integration, development-time property defaults, and remote debugging.

| Metadata | Value |
| :--- | :--- |
| **Level** | Beginner |
| **Prerequisites** | [Project Setup with Initializr, Maven & Gradle](file:///03-backend/spring-boot/00-setup/03-project-setup-with-initializr-maven-gradle.md) |
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
In traditional enterprise Java development, testing even a one-line controller modification historically required stopping the application, recompiling the entire source tree, repackaging the archive, and waiting 30–90 seconds for a full JVM and context reboot. This friction degrades developer flow and encourages lengthy feedback loops.

### Historical Evolution
- **Pre-Boot Era**: Developers relied on container hot-deployments (Tomcat manager reload) or expensive third-party JVM agents (JRebel) that bytecode-manipulated running classes in memory.
- **Spring Boot DevTools (1.3+)**: Introduced an out-of-the-box, zero-cost developer companion module (`spring-boot-devtools`) that leverages a two-classloader architecture to achieve sub-second context restarts whenever compiled classes change.
- **Modern IDE Integration**: IntelliJ IDEA, VS Code (Spring Tools 4), and Eclipse now provide native compiler triggers, background compilation, and direct LiveReload browser socket communication.

### Real-World Relevance
Configuring an optimal local developer environment with DevTools cuts daily iterative cycle times from minutes to seconds while eliminating accidental production leaks of dev configurations.

---

## 2. Core Concepts

### 1. Dual Classloader Architecture
DevTools accelerates restart times by partitioning the classpath into two distinct classloaders:
- **Base ClassLoader**: Loads all third-party dependencies (Spring Framework, Hibernate, Jackson, JDBC drivers). Because these JARs do not change during an active coding session, this classloader remains persistently loaded in JVM memory.
- **Restart ClassLoader**: Loads only the application's compiled project classes (your controllers, services, repositories, and configurations). When a change occurs, only the restart classloader is discarded and re-instantiated, re-reading the Spring ApplicationContext in a fraction of a second.

### 2. LiveReload Server
DevTools embeds an in-memory LiveReload WebSocket server listening on port `35729`. Coupled with a browser LiveReload extension, any change to frontend templates (Thymeleaf), CSS, or REST responses automatically refreshes open browser tabs without manual user action.

### 3. Development-Time Property Overrides
When DevTools is active, Spring Boot automatically applies sensible development overrides:
- Template caching is disabled (`spring.thymeleaf.cache=false`, `spring.freemarker.cache=false`).
- Web resource caching is relaxed.
- Enhanced debug logging for web requests can be enabled automatically without altering `application.properties`.

---

## 3. How It Works Internally

### The Dual-Classloader Restart Cycle

```
+-------------------------------------------------------------------------+
|                              JVM Process                                |
|                                                                         |
|  +-------------------------------------------------------------------+  |
|  |                         Base ClassLoader                          |  |
|  |  - spring-boot-starter-web.jar                                    |  |
|  |  - hibernate-core.jar                                             |  |
|  |  - jackson-databind.jar                                           |  |
|  |  (PERSISTENT: Never reloaded during development iterations)        |  |
|  +---------------------------------+---------------------------------+  |
|                                    | Parent Delegation                   |
|                                    v                                    |
|  +-------------------------------------------------------------------+  |
|  |                   Restart ClassLoader (Instance #1)               |  |
|  |  - OrderController.class                                          |  |
|  |  - OrderService.class                                             |  |
|  |  - ApplicationConfig.class                                        |  |
|  |  - ApplicationContext (Spring IoC Container)                      |  |
|  +-------------------------------------------------------------------+  |
+------------------------------------+------------------------------------+
                                     |
               File Watcher detects change in `target/classes/`
                                     |
                                     v
+-------------------------------------------------------------------------+
|                  1. Close ApplicationContext #1                         |
|                  2. Discard Restart ClassLoader #1                      |
|                  3. Garbage Collect old bean instances                  |
|                  4. Instantiate Restart ClassLoader #2                  |
|                  5. Boot ApplicationContext #2 from fresh classes       |
|                                                                         |
|       Total Duration: ~400ms - 1200ms (vs 30s cold JVM restart)         |
+-------------------------------------------------------------------------+
```

---

## 4. Syntax & Usage

### Maven Dependency Configuration
DevTools must be marked as `<optional>true</optional>` so that it is excluded transitively from downstream projects:
```xml
<dependency>
    <groupId>org.springframework.boot</groupId>
    <artifactId>spring-boot-devtools</artifactId>
    <scope>runtime</scope>
    <optional>true</optional>
</dependency>
```

### Gradle Kotlin DSL Configuration
Gradle provides the dedicated `developmentOnly` configuration:
```kotlin
dependencies {
    developmentOnly("org.springframework.boot:spring-boot-devtools")
}
```

### IDE Configuration Guide

#### 1. IntelliJ IDEA
1. Open **Settings / Preferences** (`Ctrl+Alt+S` or `Cmd+,`).
2. Navigate to **Build, Execution, Deployment -> Compiler**.
3. Check **Build project automatically**.
4. Navigate to **Advanced Settings**.
5. Check **Allow auto-make to start even if developed application is currently running**.
6. When editing code, press `Ctrl+F9` (Windows/Linux) or `Cmd+F9` (macOS) to force an instantaneous project recompile.

#### 2. Visual Studio Code
1. Install the official **Spring Boot Extension Pack** (VMware).
2. Enable automatic compilation by setting `"java.autobuild.enabled": true` in `settings.json`.
3. Changes to `.java` files are compiled instantly upon save, triggering the DevTools file watcher.

---

## 5. Practical Examples

### Example 1: Fine-Tuning DevTools in `application-dev.yaml`
```yaml
spring:
  devtools:
    restart:
      # Enable or disable automated restart
      enabled: true
      # Poll interval for detecting changes in compiled class directories
      poll-interval: 1000ms
      quiet-period: 400ms
      # Exclude static assets from triggering a full Spring context restart
      exclude:
        - "static/**"
        - "public/**"
        - "templates/**"
      # Additional paths outside classpath to watch for changes
      additional-paths:
        - "../shared-kernel/target/classes"
    livereload:
      enabled: true
      port: 35729
```

### Example 2: Trigger-File Based Restarts
To prevent unwanted restarts while in the middle of typing multi-file edits, configure a trigger file. Restarts occur ONLY when this specific file is touched:
```yaml
spring:
  devtools:
    restart:
      trigger-file: ".reload-trigger"
```
```bash
# Touch file from terminal or IDE macro to trigger restart
touch .reload-trigger
```

### Example 3: Disabling DevTools Programmatically for Performance Tests
```java
package com.example.demo;

import org.springframework.boot.SpringApplication;
import org.springframework.boot.autoconfigure.SpringBootApplication;

@SpringBootApplication
public class DemoApplication {

    public static void main(String[] args) {
        // Explicitly disable restart capabilities before context boots
        // Useful for benchmarking local start-up or profiling memory
        System.setProperty("spring.devtools.restart.enabled", "false");
        SpringApplication.run(DemoApplication.class, args);
    }
}
```

### Example 4: Anti-Pattern & Remediation (DevTools Leaking into Docker Image)
```dockerfile
# ANTI-PATTERN: Copying un-repackaged classes with devtools into production container
FROM eclipse-temurin:21-jre-alpine
COPY target/classes /app/classes
COPY target/dependency /app/lib
# DANGER: DevTools starts in production, opening LiveReload port 35729 and running file watchers!
CMD ["java", "-cp", "/app/classes:/app/lib/*", "com.example.app.Application"]

# REMEDIATION: Always run maven package with production profile
# Maven automatically strips <optional>true</optional> devtools from the fat JAR
FROM eclipse-temurin:21-jre-alpine
WORKDIR /app
COPY target/*.jar app.jar
# Clean production execution: DevTools is completely inert inside repackaged fat JARs
ENTRYPOINT ["java", "-jar", "app.jar"]
```

### Example 5: Remote Application Updates via HTTP Tunnel
DevTools supports pushing updated class files over an authenticated HTTP tunnel to a remote development server:
```yaml
# On the remote development server
spring:
  devtools:
    remote:
      secret: "enterprise-super-secret-token-change-in-production"
```
```java
// On local developer machine: Launch RemoteSpringApplication client
package com.example.demo;

import org.springframework.boot.devtools.RemoteSpringApplication;

public class RemoteClient {
    public static void main(String[] args) {
        // Connects to remote staging/dev container, syncing local compiled bytecodes over HTTP
        RemoteSpringApplication.main(new String[] { "https://dev-server.internal.company.com" });
    }
}
```

---

## 6. Best Practices

1. **Mark DevTools as `<optional>true</optional>` in Maven**: Prevents DevTools from leaking transitively into other modules or downstream microservices.
2. **Use Gradle's `developmentOnly`**: Guarantees that DevTools is included during `./gradlew bootRun` but completely omitted from the final `./gradlew bootJar` distribution.
3. **Configure Trigger Files for Large Codebases**: If typing generates frequent incomplete states, a trigger file (e.g., `.reload-trigger`) ensures restarts only happen when you explicitly request them.
4. **Exclude Static Resources from Restarts**: Static assets (`/static`, `/templates`) do not require an ApplicationContext reboot; LiveReload will reload them without cycling the Spring beans.
5. **Never Use DevTools in Production**: While Spring Boot automatically disables DevTools when running as `java -jar app.jar`, improper unpacking or custom classpaths can leave it active.
6. **Use Fast Hot-Swap When Possible**: Pair DevTools with JVM hot-swap (`Ctrl+Shift+F9` in IntelliJ) for single-method body changes, falling back to DevTools restart for structural changes (new methods, bean annotations).

---

## 7. Common Mistakes & Pitfalls

| Mistake | Why It Fails | Practical Remedy |
| :--- | :--- | :--- |
| **`ClassCastException` across classloaders** | Objects saved into persistent stores (or cached in static variables) loaded by the Base ClassLoader cannot be cast to types from the new Restart ClassLoader. | Avoid storing application-specific domain classes in static singletons or outside Spring-managed context. |
| **IntelliJ not reloading on code edit** | IntelliJ defaults to manual compilation; simply saving a file does not trigger a compile or bytecode update on disk. | Press `Ctrl+F9` / `Cmd+F9` to build, or enable "Build project automatically" in Settings. |
| **LiveReload port collision (35729)** | Running multiple Spring Boot microservices locally causes all instances after the first to fail binding port 35729. | Set `spring.devtools.livereload.enabled=false` on all services except the primary frontend-facing service. |
| **ThreadLocal leaks across restarts** | Custom threads that fail to clear `ThreadLocal` storage leak memory across context restarts, eventually causing `OutOfMemoryError: Metaspace`. | Implement `DisposableBean` or standard cleanup hooks to clear ThreadLocals on shutdown. |
| **Overriding production properties via dev defaults** | DevTools sets `spring.thymeleaf.cache=false` automatically, masking production template caching defects during local testing. | Use a dedicated `@SpringBootTest` with production profiles to verify caching behavior prior to release. |
| **Using DevTools with Security without ignoring LiveReload** | Security filter chains may intercept LiveReload websocket traffic on `/livereload` or static paths. | Permit unrestricted access to LiveReload paths in security configuration during development. |

---

## 8. Performance & Security Notes

### Classloader Metaspace Consumption
Frequent DevTools restarts allocate class metadata in JVM **Metaspace**. The JVM garbage collector automatically reclaims Metaspace when older classloaders are unreferenced:
- To prevent Metaspace exhaustion during marathon 8-hour coding sessions, configure your local run configuration with adequate Metaspace:
  `-XX:MaxMetaspaceSize=512m`

### Security Hardening for Remote DevTools
The remote DevTools feature opens an HTTP endpoint capable of executing bytecode updates on the target host.
- **Mandatory Secret**: You must configure `spring.devtools.remote.secret`. Never use predictable passwords.
- **Network Isolation**: Ensure remote DevTools endpoints are accessible strictly via private VPN or localhost port-forwarding tunnels (`ssh -L`). Never expose them to public internet ingress.

---

## 9. Comparison / When to Use What

| Solution | Mechanism | Speed | Cost / Setup | Best For |
| :--- | :--- | :--- | :--- | :--- |
| **Spring Boot DevTools** | Two-classloader ApplicationContext reload | ~500ms - 1.5s | Free; zero setup | Standard daily Spring Boot development |
| **Standard JVM HotSwap** | JVM debugger bytecode replacement | ~50ms | Free; built into IDE | Single-method body logic edits |
| **JRebel** | Proprietary JVM-level class instrumentation | Instant (<100ms) | Commercial license required | Massive legacy enterprise monolithic codebases |
| **Cold JVM Restart** | Kill process and launch new JVM | 15s - 60s | Free; default behavior | Sanity testing full bootstrap lifecycle |

---

## 10. Interview Questions

<details>
<summary><strong>Q1: How does Spring Boot DevTools achieve fast restart times compared to a cold start?</strong></summary>

**Answer:**  
DevTools uses two classloaders:
1. **Base ClassLoader**: Loads unchanging third-party dependency JARs (Spring, Hibernate, Jackson). This remains permanently in memory.
2. **Restart ClassLoader**: Loads project classes under active development.
When a code change is compiled, only the Restart ClassLoader is destroyed and recreated, reloading only your project classes and the Spring `ApplicationContext`. Because third-party JARs do not need to be re-indexed or reloaded, restart takes a fraction of a second.
</details>

<details>
<summary><strong>Q2: Why does `ClassCastException` sometimes occur when using DevTools with custom caching or serialization?</strong></summary>

**Answer:**  
In Java, a class identity is defined by its fully qualified name AND the `ClassLoader` that defined it. If an object is cached by a class loaded by the Base ClassLoader (or in an external thread), and then cast to a type loaded by the newly created Restart ClassLoader after a reload, the JVM treats them as incompatible types and throws `ClassCastException`.
</details>

<details>
<summary><strong>Q3: How does Spring Boot ensure DevTools does not run in production?</strong></summary>

**Answer:**  
DevTools is automatically disabled whenever the application is packaged as a fully executable fat JAR (e.g., launched via `java -jar app.jar`). Furthermore, declaring `<optional>true</optional>` in Maven or `developmentOnly` in Gradle ensures that DevTools is not packaged into downstream distribution artifacts.
</details>

<details>
<summary><strong>Q4: What is the LiveReload server, and on what port does it operate?</strong></summary>

**Answer:**  
LiveReload is an embedded WebSocket server operating on port `35729`. When static assets or templates are modified and saved, DevTools notifies connected browser clients (via the LiveReload browser extension) to automatically trigger a page refresh without manual browser interaction.
</details>

<details>
<summary><strong>Q5: What are the default property changes applied by DevTools automatically?</strong></summary>

**Answer:**  
DevTools automatically enables properties that favor developer productivity over raw performance:
- Disables template caching for Thymeleaf, FreeMarker, and Groovy Templates (`cache=false`).
- Disables HTTP resource caching.
- Enables DEBUG logging for web groups if configured.
These properties are defined in `DevToolsPropertyDefaultsPostProcessor`.
</details>

<details>
<summary><strong>Q6: How can you prevent an accidental restart while saving multiple files in an IDE?</strong></summary>

**Answer:**  
Configure a **trigger file** using `spring.devtools.restart.trigger-file=.reload-trigger`. DevTools will ignore all intermediate file compiles and will only restart the context when the designated trigger file is updated/touched.
</details>

<details>
<summary><strong>Q7: What is the difference between DevTools restart and standard IDE HotSwap?</strong></summary>

**Answer:**  
- **IDE HotSwap**: Standard JVM feature that replaces method implementations in memory without restarting the Spring context. It cannot handle schema changes, method additions, or annotation changes.
- **DevTools Restart**: Reboots the entire Spring `ApplicationContext` via a fresh classloader. It handles new classes, modified annotations, and bean relationship changes cleanly.
</details>

<details>
<summary><strong>Q8: How does DevTools handle static resource changes without triggering an expensive context restart?</strong></summary>

**Answer:**  
By default, paths matching `META-INF/maven/**`, `META-INF/resources/**`, `resources/**`, `static/**`, `public/**`, or `templates/**` are excluded from triggering a restart. Instead, changes to these paths trigger only a LiveReload notification to refresh the browser UI.
</details>

---

## 11. Practice Exercises

<details>
<summary><strong>Exercise 1: Configure DevTools in Maven (Easy)</strong></summary>

**Task:**  
Add `spring-boot-devtools` to an existing Maven project. Ensure that it is marked as `<optional>true</optional>`. Verify that running `./mvnw spring-boot:run` shows `[restartedMain]` in the console logs instead of `[main]`.
</details>

<details>
<summary><strong>Exercise 2: Setup IntelliJ Automatic Recompile (Easy)</strong></summary>

**Task:**  
Configure IntelliJ IDEA to enable "Build project automatically" and "Allow auto-make to start even if developed application is running". Start your application and test modifying a REST Controller return string. Verify that pressing `Ctrl+F9` triggers a restart in under 1 second.
</details>

<details>
<summary><strong>Exercise 3: Trigger-File Based Reloading (Medium)</strong></summary>

**Task:**  
Configure `spring.devtools.restart.trigger-file=.reload-trigger` in `application.yaml`. Edit a service class and save. Verify that no restart occurs. Then run `touch .reload-trigger` from your terminal and verify the context restarts immediately.
</details>

<details>
<summary><strong>Exercise 4: Multi-Service LiveReload Port Isolation (Medium)</strong></summary>

**Task:**  
Launch two separate Spring Boot microservices locally at the same time. Observe the port bind failure on port `35729` for the second service. Configure the second service to disable LiveReload while keeping restart enabled.
</details>

<details>
<summary><strong>Exercise 5: Reproduce and Fix `ClassCastException` with DevTools (Hard)</strong></summary>

**Task:**  
Write a custom static cache holding a domain object instance. Trigger a DevTools restart by modifying a controller. Attempt to retrieve the cached object and cast it to the updated domain class type. Observe the `ClassCastException`. Refactor the design to store the cache inside a Spring-managed `@Component` bean so it is properly reloaded with the context.
</details>

<details>
<summary><strong>Exercise 6: Remote DevTools Secure Tunnel (Hard)</strong></summary>

**Task:**  
Run a Spring Boot service inside a local Docker container with remote devtools enabled and a strong secret key. Configure a local `RemoteSpringApplication` client on your host to push compiled bytecode changes into the running container over HTTP.
</details>

---

## 12. Summary / Cheat Sheet

| Feature / Setting | Configuration Syntax | Effect |
| :--- | :--- | :--- |
| **Add DevTools (Maven)** | `<dependency><groupId>org.springframework.boot</groupId><artifactId>spring-boot-devtools</artifactId><optional>true</optional></dependency>` | Enables fast restart and LiveReload in dev |
| **Add DevTools (Gradle)** | `developmentOnly("org.springframework.boot:spring-boot-devtools")` | Excludes devtools from production bootJar |
| **Disable DevTools** | `spring.devtools.restart.enabled=false` | Disables classloader restart completely |
| **Trigger File** | `spring.devtools.restart.trigger-file=.reload-trigger` | Restarts only on trigger file modification |
| **Custom Excludes** | `spring.devtools.restart.exclude=static/**,templates/**` | Prevents context restarts on UI asset saves |
| **Disable LiveReload** | `spring.devtools.livereload.enabled=false` | Prevents port 35729 collisions across services |
| **Console Thread Indicator** | Log line shows `restartedMain` | Indicates DevTools restart classloader is active |

---

## 13. Further Reading
- [Official Spring Boot DevTools Reference Documentation](https://docs.spring.io/spring-boot/docs/current/reference/html/using.html#using.devtools)
- [IntelliJ IDEA Spring Boot Integration Guide](https://www.jetbrains.com/help/idea/spring-boot.html)
- [Spring Tools 4 for Visual Studio Code & Eclipse](https://spring.io/tools)
