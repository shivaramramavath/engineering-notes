# Spring Boot DevTools Internals & Classloaders

| Metadata | Value |
| :--- | :--- |
| **Level** | Advanced |
| **Prerequisites** | Java ClassLoading, JVM Bytecode Basics, Spring Boot Lifecycle |
| **Estimated Reading Time** | 24 minutes |
| **Status** | Production-Ready |
| **Spring Boot Version** | 3.4.x / 4.x |
| **Spring Framework Version** | 6.2.x / 7.x |
| **Java Baseline** | OpenJDK 21 LTS / 25 LTS |
| **Build Tool** | Maven 3.9+ / Gradle 8.10+ |
| **Tested With** | OpenJDK 21.0.2, IntelliJ IDEA 2024.1, VS Code |

---

## Table of Contents
1. [What & Why](#1-what--why)
2. [Core Concepts](#2-core-concepts)
3. [How It Works Internally](#3-how-it-works-internally)
4. [Syntax & Configuration](#4-syntax--configuration)
5. [Practical Examples](#5-practical-examples)
6. [Best Practices](#6-best-practices)
7. [Common Mistakes & Pitfalls](#7-common-mistakes--pitfalls)
8. [Performance & Security Considerations](#8-performance--security-considerations)
9. [Comparison: DevTools Restart vs HotSpot vs JRebel](#9-comparison-devtools-restart-vs-hotspot-vs-jrebel)
10. [Interview Questions & Deep Dives](#10-interview-questions--deep-dives)
11. [Hands-On Practice Exercises](#11-hands-on-practice-exercises)
12. [Summary & Quick Reference](#12-summary--quick-reference)
13. [Further Reading](#13-further-reading)

---

## 1. What & Why

The developer feedback loop directly dictates engineering productivity. Rebuilding, repackaging, and restarting a complete Java enterprise application on every code tweak takes anywhere from 15 to 60 seconds. Much of this latency is spent loading unchanging third-party libraries (Spring framework, Hibernate, Jackson, AWS SDK) into JVM memory.

### The Problem
Traditional JVM cold restarts:
1. Reload thousands of third-party `.jar` classes repeatedly.
2. Invalidate developer flow for trivial modifications in controllers or services.
3. Require cumbersome external agents or commercial tools to achieve rapid iteration.

### The Solution: `spring-boot-devtools`
Spring Boot DevTools provides sub-second restarts using a dual-classloader architecture. Unchanging third-party dependencies are isolated in a persistent base classloader, while mutable project classes reside in a disposable restart classloader. When source code changes, only the project classes are discarded and reloaded.

---

## 2. Core Concepts

### The Dual-Classloader Architecture
```
                         AppClassLoader (System)
                                   │
                                   ▼
                      Base ClassLoader (Persistent)
                     ───────────────────────────────
                     • spring-core.jar, hibernate.jar
                     • jackson.jar, postgresql.jar
                     • Never discarded during restart
                                   │
                                   ▼
                     Restart ClassLoader (Disposable)
                     ────────────────────────────────
                     • target/classes or build/classes
                     • Controllers, Services, Entities
                     • Discarded & recreated in ~500ms
```

### Invariants
1. **Production Exclusion**: DevTools automatically disables itself when the application is packaged as a fully executable Uber-JAR (`java -jar app.jar`).
2. **Class Identity Separation**: A class loaded by the `RestartClassLoader` is **not type-equal** to the same class loaded by the `BaseClassLoader` (`ClassCastException` risk if leaked across boundaries).
3. **Property Defaults Overrides**: DevTools automatically disables template caching (Thymeleaf, FreeMarker) and sets lenient caching defaults in local environments.

---

## 3. How It Works Internally

### The FileSystem Watcher
DevTools includes a background daemon thread running `FileSystemWatcher`.
1. It polls configured classpath directories (e.g., `target/classes`) at periodic intervals (`spring.devtools.restart.poll-interval`).
2. It tracks file modification timestamps and handles quiet periods (`spring.devtools.restart.quiet-period`) to avoid restarting while the IDE is mid-compilation.
3. Once changes settle, it signals `Restarter.getInstance().restart()`.

### The Restart Mechanism
```
  Code Changed in IDE
          │
          ▼
  FileSystemWatcher detects changes after quiet period
          │
          ▼
  Restarter.restart() called
          │
          ├─► 1. Close current ConfigurableApplicationContext (triggers @PreDestroy)
          ├─► 2. Dereference RestartClassLoader (eligible for GC)
          ├─► 3. Instantiate new RestartClassLoader with BaseClassLoader as parent
          ├─► 4. Invoke main() method on a new thread using new classloader
          └─► 5. New ApplicationContext refreshes in < 1 second!
```

---

## 4. Syntax & Configuration

### Dependency Setup (`pom.xml`)
```xml
<dependency>
    <groupId>org.springframework.boot</groupId>
    <artifactId>spring-boot-devtools</artifactId>
    <scope>runtime</scope>
    <optional>true</optional> <!-- Ensures it is NOT transitively inherited -->
</dependency>
```

### Advanced `application-dev.yaml` Configuration
```yaml
spring:
  devtools:
    restart:
      enabled: true
      poll-interval: 1000ms
      quiet-period: 400ms
      additional-paths:
        - src/main/resources/static
      exclude:
        - static/**
        - public/**
        - META-INF/resources/**
      trigger-file: .trigger # Only restart when this specific file is touched
    livereload:
      enabled: true
      port: 35729
```

---

## 5. Practical Examples

### Example 1: Programmatic Inspection of the Active Classloader

```java
package com.example.devtools.diagnostics;

import org.slf4j.Logger;
import org.slf4j.LoggerFactory;
import org.springframework.boot.CommandLineRunner;
import org.springframework.stereotype.Component;

@Component
public class ClassLoaderInspector implements CommandLineRunner {

    private static final Logger log = LoggerFactory.getLogger(ClassLoaderInspector.class);

    @Override
    public void run(String... args) {
        ClassLoader appClassLoader = getClass().getClassLoader();
        ClassLoader thirdPartyClassLoader = org.springframework.boot.SpringApplication.class.getClassLoader();

        log.info("Current Bean ClassLoader: {}", appClassLoader);
        log.info("SpringApplication ClassLoader: {}", thirdPartyClassLoader);

        if (appClassLoader.getClass().getName().contains("RestartClassLoader")) {
            log.info("SUCCESS: Running under Spring Boot DevTools RestartClassLoader!");
        } else {
            log.warn("WARNING: Running under standard JVM ClassLoader. DevTools restart inactive.");
        }
    }
}
```

### Example 2: Customizing Restart Classloader Inclusions (`META-INF/spring-devtools.properties`)

When developing in a multi-module Maven/Gradle project, sibling modules should also be reloaded by the `RestartClassLoader`. By default, DevTools treats all `.jar` files in `target` or repository caches as non-restartable.

```properties
# Place in: src/main/resources/META-INF/spring-devtools.properties

# Include custom shared module in RestartClassLoader (regex pattern)
restart.include.shared-kernel=/shared-kernel-[\\w\\d\\.-]+\\.jar
restart.include.domain-model=/domain-model-[\\w\\d\\.-]+\\.jar

# Exclude heavy third party library that does not change
restart.exclude.company-sdk=/internal-immutable-sdk-[\\w\\d\\.-]+\\.jar
```

### Example 3: Edge Case — Handling `ClassCastException` with Deserialization

When caching objects in an external memory store (e.g., Redis or disk serialization) while using DevTools, classes deserialized across restarts can fail with `ClassCastException` because their classloader identity changed.

```java
package com.example.devtools.cache;

import org.springframework.context.annotation.Bean;
import org.springframework.context.annotation.Configuration;
import org.springframework.data.redis.connection.RedisConnectionFactory;
import org.springframework.data.redis.core.RedisTemplate;
import org.springframework.data.redis.serializer.GenericJackson2JsonRedisSerializer;
import org.springframework.data.redis.serializer.StringRedisSerializer;

@Configuration(proxyBeanMethods = false)
public class SafeRedisDevToolsConfig {

    /**
     * Using JSON serialization avoids ClassCastException caused by standard
     * Java Serialization across different DevTools RestartClassLoaders.
     */
    @Bean
    public RedisTemplate<String, Object> redisTemplate(RedisConnectionFactory connectionFactory) {
        RedisTemplate<String, Object> template = new RedisTemplate<>();
        template.setConnectionFactory(connectionFactory);
        
        // JSON serializer dynamically resolves classes via the current thread context classloader
        GenericJackson2JsonRedisSerializer jsonSerializer = new GenericJackson2JsonRedisSerializer();
        
        template.setKeySerializer(new StringRedisSerializer());
        template.setValueSerializer(jsonSerializer);
        template.setHashKeySerializer(new StringRedisSerializer());
        template.setHashValueSerializer(jsonSerializer);
        template.afterPropertiesSet();
        return template;
    }
}
```

### Example 4: Anti-Pattern vs Production Fix

#### Anti-Pattern: Shipping DevTools to Production Container
```xml
<!-- ANTI-PATTERN: Scoped as compile without optional flag -->
<dependency>
    <groupId>org.springframework.boot</groupId>
    <artifactId>spring-boot-devtools</artifactId>
    <!-- Missing <optional>true</optional> -->
    <!-- Increases memory footprint, can restart unexpectedly if files change -->
</dependency>
```

#### Production Fix: Exclude DevTools in Spring Boot Maven Plugin
```xml
<!-- PRODUCTION FIX: Guarantee DevTools is stripped during repackaging -->
<build>
    <plugins>
        <plugin>
            <groupId>org.springframework.boot</groupId>
            <artifactId>spring-boot-maven-plugin</artifactId>
            <configuration>
                <excludeDevtools>true</excludeDevtools>
            </configuration>
        </plugin>
    </plugins>
</build>
```

### Example 5: Trigger File Strategy for Controlled Restart

```java
package com.example.devtools.trigger;

import org.slf4j.Logger;
import org.slf4j.LoggerFactory;
import org.springframework.boot.SpringApplication;
import org.springframework.boot.autoconfigure.SpringBootApplication;

import java.io.File;
import java.io.IOException;

@SpringBootApplication
public class TriggerFileDemoApplication {

    private static final Logger log = LoggerFactory.getLogger(TriggerFileDemoApplication.class);

    public static void main(String[] args) {
        // Enforce trigger-file mode programmatically if needed
        System.setProperty("spring.devtools.restart.trigger-file", ".reload-trigger");
        SpringApplication.run(TriggerFileDemoApplication.class, args);
    }

    /**
     * Utility method for IDE plugins or scripts to trigger application reload on demand.
     */
    public static void touchTriggerFile() throws IOException {
        File trigger = new File(".reload-trigger");
        if (!trigger.exists()) {
            boolean created = trigger.createNewFile();
            log.info("Created trigger file: {}", created);
        } else {
            boolean updated = trigger.setLastModified(System.currentTimeMillis());
            log.info("Updated trigger file timestamp: {}", updated);
        }
    }
}
```

---

## 6. Best Practices

1. **Always Set `<optional>true</optional>`**: Prevents projects that depend on your artifact from inheriting DevTools accidentally.
2. **Use Trigger Files in Large Codebases**: In massive projects, continuous compilation by modern IDEs can cause rapid, annoying restart loops. Set a trigger file (`spring.devtools.restart.trigger-file=.trigger`) so restarts happen only when requested.
3. **Use JSON or Text-based Caches Locally**: Avoid JDK serialized sessions or binary caches across DevTools restarts to prevent `ClassCastException`.
4. **Configure Multi-Module Inclusions**: Use `META-INF/spring-devtools.properties` with `restart.include` so changes in sibling library modules trigger restarts immediately.
5. **Disable DevTools in Continuous Integration**: Pass `-Dspring.devtools.restart.enabled=false` in CI test suites to avoid unnecessary classloader overhead.

---

## 7. Common Mistakes & Pitfalls

| Mistake | Why It Fails | Practical Remedy |
| :--- | :--- | :--- |
| **ClassCastException across Deserialization** | Cached class was loaded by old `RestartClassLoader`, while target cast refers to new `RestartClassLoader`. | Use JSON/String serialization or clear caches on restart. |
| **Static State Leaks** | Holding references to instances in static fields on classes loaded by the `BaseClassLoader` causes memory leaks. | Clean static references in `@PreDestroy` or avoid cross-classloader statics. |
| **DevTools Enabled in Docker Production Image** | Causes overhead, security surface expansion, and potential unexpected restarts. | Configure `<excludeDevtools>true</excludeDevtools>` in the build plugin. |
| **IDE Auto-Build Not Configured** | Editing files in IntelliJ does not automatically compile to `target/classes`, so DevTools never triggers. | Enable "Build project automatically" and "Allow auto-make to start even if developed application is currently running". |
| **LiveReload Port Conflicts** | Multiple DevTools applications running locally clash on default port `35729`. | Set `spring.devtools.livereload.port=0` or disable it on secondary services. |
| **Thread Deadlocks on Class Loading** | Complex parallel classloaders can deadlock on cyclic class dependencies. | Ensure Java 21+ parallel class loading is active and avoid circular class dependencies. |

---

## 8. Performance & Security Considerations

### Memory Footprint of RestartClassLoader
Old classloaders become eligible for garbage collection once all references to their loaded classes and instances are gone. If a library retains a thread or static reference to a class loaded by `RestartClassLoader`, Metaspace will leak and eventually exhaust available memory (`java.lang.OutOfMemoryError: Metaspace`).

### Security Surface in DevTools Remote
DevTools includes a remote application feature (`org.springframework.boot.devtools.RemoteSpringApplication`) allowing class pushing over HTTP. This requires passing a shared secret (`spring.devtools.remote.secret`). **Never enable this on internet-accessible or shared development clusters**, as it represents an arbitrary remote code execution vulnerability if compromised.

---

## 9. Comparison: DevTools Restart vs HotSpot vs JRebel

| Feature | Spring Boot DevTools | JVM HotSpot Hotswap | JRebel (Commercial) |
| :--- | :--- | :--- | :--- |
| **Method Body Changes** | Yes (~1s restart) | Yes (instant) | Yes (instant) |
| **Adding New Methods/Fields** | Yes (~1s restart) | No (requires JVM restart) | Yes (instant) |
| **Adding New Beans/Configs** | Yes (~1s restart) | No | Yes |
| **Cost / License** | Free & Open Source | Free (built-in JVM) | Paid Commercial |
| **Configuration Complexity** | Minimal (single starter) | Zero config in IDE | Requires JVM agent |
| **State Retention** | Discards in-memory state | Preserves JVM heap state | Preserves JVM heap state |

---

## 10. Interview Questions & Deep Dives

<details>
<summary><strong>Q1: How does Spring Boot DevTools achieve fast restarts compared to a full JVM cold restart?</strong></summary>

**Answer:**
DevTools uses two classloaders: a `BaseClassLoader` that loads unchanging external JARs (Spring, Hibernate, Jackson), and a `RestartClassLoader` that loads mutable project classes (`target/classes`). When code changes, DevTools discards only the `RestartClassLoader` and its associated `ApplicationContext`, instantiates a fresh one, and calls `main()`. Because 95% of the classpath (external libraries) is already loaded and JIT-compiled in the `BaseClassLoader`, startup takes ~500ms instead of 15-30s.
</details>

<details>
<summary><strong>Q2: Why does `ClassCastException` occur when using DevTools with standard Java deserialization?</strong></summary>

**Answer:**
In Java, a class's fully qualified identity in the runtime is governed by the tuple `(Package.ClassName, ClassLoader)`. If an object is serialized during run 1 and deserialized during run 2 after a restart, the serialized bytes are deserialized against classes loaded by the *new* `RestartClassLoader`. If any reference still points to an instance from the *old* `RestartClassLoader`, the JVM treats them as incompatible types and throws `ClassCastException`.
</details>

<details>
<summary><strong>Q3: How does DevTools prevent itself from running in production?</strong></summary>

**Answer:**
DevTools checks how the application was launched. If the application is launched from an executable archive (`java -jar my-app.jar`) or via a custom launcher, DevTools recognizes this as a production run and disables the restart mechanism entirely. Additionally, configuring `<optional>true</optional>` and `<excludeDevtools>true</excludeDevtools>` guarantees the JAR does not physically contain the devtools dependency.
</details>

<details>
<summary><strong>Q4: What is the purpose of `spring-devtools.properties`?</strong></summary>

**Answer:**
`META-INF/spring-devtools.properties` allows developers to customize which JARs are loaded by the `RestartClassLoader` vs the `BaseClassLoader`. By default, DevTools treats all `.jar` files as static libraries. In a multi-module project, you specify `restart.include.my-module=/my-module.*\\.jar` so that changes in internal libraries trigger restarts.
</details>

<details>
<summary><strong>Q5: What is LiveReload and how does it integrate with DevTools?</strong></summary>

**Answer:**
LiveReload is an embedded server running on port `35729` within DevTools. When static assets (HTML, CSS, JS) or backend endpoints refresh, the server sends a WebSocket message to connected browser extensions, prompting the browser to refresh the page automatically without manual reload.
</details>

<details>
<summary><strong>Q6: How does DevTools handle property defaults for template engines?</strong></summary>

**Answer:**
In development, template caching (such as `spring.thymeleaf.cache=true`) forces developers to restart the server to see HTML changes. DevTools automatically applies sensible development defaults, setting `spring.thymeleaf.cache=false`, `spring.freemarker.cache=false`, and enabling debug logging for web requests, without altering your production configuration files.
</details>

<details>
<summary><strong>Q7: Can DevTools be used with Virtual Threads in Java 21?</strong></summary>

**Answer:**
Yes. DevTools is fully compatible with Java 21 virtual threads. Virtual threads created during an application execution are interrupted and cleaned up when the `ApplicationContext` closes, allowing the new restart thread to launch clean virtual thread executors.
</details>

<details>
<summary><strong>Q8: What causes Metaspace OutOfMemoryError when using DevTools repeatedly?</strong></summary>

**Answer:**
Classloader leaks. If a class in the `BaseClassLoader` (or a long-lived thread/ThreadLocal) holds a strong reference to a class or object loaded by the `RestartClassLoader`, the old classloader cannot be garbage collected. Repeated restarts accumulate orphaned classloaders in Metaspace until `OutOfMemoryError: Metaspace` occurs.
</details>

---

## 11. Hands-On Practice Exercises

<details>
<summary><strong>Exercise 1 (Easy): Add DevTools to Maven POM cleanly</strong></summary>

**Task**: Write the Maven dependency snippet for Spring Boot DevTools ensuring it is optional and runtime-scoped.

```xml
<!-- Solution: -->
<dependency>
    <groupId>org.springframework.boot</groupId>
    <artifactId>spring-boot-devtools</artifactId>
    <scope>runtime</scope>
    <optional>true</optional>
</dependency>
```
</details>

<details>
<summary><strong>Exercise 2 (Easy): Configure Trigger File Restart in YAML</strong></summary>

**Task**: Configure `application-dev.yaml` so that DevTools only restarts when a `.trigger` file is modified.

```yaml
# Solution:
spring:
  devtools:
    restart:
      trigger-file: .trigger
      quiet-period: 500ms
```
</details>

<details>
<summary><strong>Exercise 3 (Medium): Custom Multi-Module DevTools Properties</strong></summary>

**Task**: Create a `spring-devtools.properties` file that includes all internal corporate libraries matching `com.acme.*` into the restart classloader.

```properties
# Solution (src/main/resources/META-INF/spring-devtools.properties):
restart.include.acme-libs=/acme-[\\w\\d\\.-]+\\.jar
```
</details>

<details>
<summary><strong>Exercise 4 (Medium): Detect DevTools Restart in Code</strong></summary>

**Task**: Write a `@Component` that prints a distinct warning if DevTools is active and informs the developer that file watching is active.

```java
// Solution:
@Component
public class DevToolsDetector {
    @PostConstruct
    public void detect() {
        if (Thread.currentThread().getContextClassLoader().getClass().getName().contains("RestartClassLoader")) {
            System.out.println(">>> RUNNING IN DEVTOOLS MODE: Instant restarts enabled. <<<");
        }
    }
}
```
</details>

<details>
<summary><strong>Exercise 5 (Hard): Diagnose and Fix a Static Reference Metaspace Leak</strong></summary>

**Task**: Create a reproducible test case where a static list in a custom singleton holds onto entities across restarts. Fix the leak by registering an application listener on `ContextClosedEvent` that clears the static collection.
</details>

<details>
<summary><strong>Exercise 6 (Hard): Build a Custom FileSystemWatcher Integration</strong></summary>

**Task**: Programmatically instantiate a `FileSystemWatcher` from `org.springframework.boot.devtools.filewatch` to listen for modifications on an external configuration directory and trigger an application event when a configuration file changes.
</details>

---

## 12. Summary & Quick Reference

- **Dual Classloader**: `BaseClassLoader` (third-party JARs) + `RestartClassLoader` (your code).
- **Restart Speed**: Typically < 1 second.
- **Dependency Invariant**: Always mark `<optional>true</optional>`.
- **Packaging Invariant**: Excluded automatically from production executable JARs.
- **Multi-Module Support**: Configure `META-INF/spring-devtools.properties`.

---

## 13. Further Reading

- [Spring Boot Official Documentation: Developer Tools](https://docs.spring.io/spring-boot/reference/using/devtools.html)
- [Java Virtual Machine Specification: Chapter 5 - Loading, Linking, and Initializing](https://docs.oracle.com/javase/specs/jvms/se21/html/jvms-5.html)
- [Understanding ClassLoader Leaks and Metaspace Tuning](https://www.oracle.com/technical-resources/articles/java/classloaders.html)
- [IntelliJ IDEA Configuration for Automatic Compilation with DevTools](https://www.jetbrains.com/help/idea/spring-boot.html#devtools)
