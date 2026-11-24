# ApplicationRunners & CommandLineRunners: Startup Execution

> Master post-startup execution in Spring Boot: the mechanics of `CommandLineRunner` vs `ApplicationRunner`, structured argument parsing with `ApplicationArguments`, deterministic ordering via `@Order`, and custom exit code generation.

| Metadata | Value |
| :--- | :--- |
| **Level** | Beginner |
| **Prerequisites** | [First Spring Boot Application Walkthrough](file:///03-backend/spring-boot/00-setup/05-first-application-walkthrough.md), [ApplicationContext Internals: refresh()](file:///03-backend/spring-boot/01-core-spring/17-application-context-internals.md) |
| **Time to Read** | 20 minutes |
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
Applications frequently need to execute specific business code immediately after the `ApplicationContext` is fully booted: pre-warming in-memory caches, executing database seed data migrations, verifying connectivity to external message brokers, or parsing command-line parameters in batch scripts. Putting this logic inside a bean's constructor or `@PostConstruct` method is dangerous because the application context has not finished refreshing, web servers are not listening, and collaborator proxies may not yet be initialized.

### Historical Evolution
- **Pre-Spring Boot**: Developers implemented `ApplicationListener<ContextRefreshedEvent>`, requiring manual extraction of command-line arguments and complex lifecycle checks.
- **Spring Boot 1.0 (2014)**: Introduced `CommandLineRunner` (raw string arguments) and `ApplicationRunner` (structured arguments) as dedicated callback interfaces.
- **Spring Boot 3.x**: Runners integrate with `ExitCodeGenerator` to guarantee clean container termination codes in Kubernetes cronjobs and batch pipelines.

### Real-World Relevance
Runners provide the standard, safe entry point for startup execution after all beans, proxies, and embedded servers are fully established, before the application is declared ready to receive traffic.

---

## 2. Core Concepts

### 1. `CommandLineRunner`
Functional interface providing raw string arguments passed to `main(String[] args)`:
```java
@FunctionalInterface
public interface CommandLineRunner {
    void run(String... args) throws Exception;
}
```

### 2. `ApplicationRunner`
More sophisticated functional interface providing structured `ApplicationArguments`:
```java
@FunctionalInterface
public interface ApplicationRunner {
    void run(ApplicationArguments args) throws Exception;
}
```

### 3. `ApplicationArguments` Structure
Splits arguments into:
- **Option Arguments** (`--key=value`): Parsed into accessible key-value collections (`args.getOptionValues("port")`).
- **Non-Option Arguments** (`file1.txt`, `run`): Clean list of standalone positional arguments.

### 4. Runner Execution Order
Multiple runner beans execute in order based on the `@Order(int)` annotation or the `Ordered` interface (lowest integer value executes first).

---

## 3. How It Works Internally

### Runner Execution in the `SpringApplication.run()` Sequence

```
+-------------------------------------------------------------------------+
|                  1. Complete ApplicationContext `refresh()`             |
|  - All singletons instantiated, injected, and post-processed            |
|  - Embedded Tomcat started and listening on port 8080                   |
+------------------------------------+------------------------------------+
                                     |
                                     v
+-------------------------------------------------------------------------+
|                  2. SpringApplication.callRunners()                     |
|  - Discovers all beans implementing:                                    |
|    * `ApplicationRunner`                                                |
|    * `CommandLineRunner`                                                |
|  - Sorts all runners via `AnnotationAwareOrderComparator`               |
+------------------------------------+------------------------------------+
                                     |
                                     v
+-------------------------------------------------------------------------+
|                  3. Sequential Execution                                |
|  For each runner in sorted order:                                       |
|    * Runner #1 (@Order(1)): Database Schema Verification                |
|    * Runner #2 (@Order(2)): Cache Pre-Warming                           |
|    * Runner #3 (@Order(3)): Telemetry Registration                      |
+------------------------------------+------------------------------------+
                                     |
                                     v
+-------------------------------------------------------------------------+
|                  4. Publish ApplicationReadyEvent                       |
|  - Application is officially declared READY!                            |
+-------------------------------------------------------------------------+
```

---

## 4. Syntax & Usage

### Implementing `ApplicationRunner` with Option Arguments
```java
@Component
@Order(1)
public class DataMigrationRunner implements ApplicationRunner {

    @Override
    public void run(ApplicationArguments args) {
        if (args.containsOption("seed-data")) {
            List<String> values = args.getOptionValues("seed-data");
            System.out.println("Executing data seeding: " + values);
        }
    }
}
```

---

## 5. Practical Examples

### Example 1: Idempotent Database Seed Runner with `ApplicationRunner`
```java
package com.example.demo.runner;

import org.slf4j.Logger;
import org.slf4j.LoggerFactory;
import org.springframework.boot.ApplicationArguments;
import org.springframework.boot.ApplicationRunner;
import org.springframework.core.annotation.Order;
import org.springframework.stereotype.Component;

import java.util.List;

record Role(String name) {}
interface RoleRepository {
    long count();
    void saveAll(Iterable<Role> roles);
}

@Component
@Order(10) // Executes early in startup sequence
public class DatabaseSeedRunner implements ApplicationRunner {

    private static final Logger log = LoggerFactory.getLogger(DatabaseSeedRunner.class);
    private final RoleRepository roleRepository;

    public DatabaseSeedRunner(RoleRepository roleRepository) {
        this.roleRepository = roleRepository;
    }

    @Override
    public void run(ApplicationArguments args) {
        log.info("Checking database role catalog...");
        if (roleRepository.count() == 0) {
            log.info("Database empty. Seeding default platform roles...");
            roleRepository.saveAll(List.of(
                new Role("ROLE_ADMIN"),
                new Role("ROLE_USER"),
                new Role("ROLE_AUDITOR")
            ));
            log.info("Default roles seeded successfully.");
        } else {
            log.info("Database roles already populated. Skipping seed.");
        }
    }
}
```

### Example 2: Parsing Command-Line Flags in Batch Applications
```java
package com.example.demo.runner;

import org.slf4j.Logger;
import org.slf4j.LoggerFactory;
import org.springframework.boot.ApplicationArguments;
import org.springframework.boot.ApplicationRunner;
import org.springframework.stereotype.Component;

import java.util.List;

@Component
public class BatchJobParameterRunner implements ApplicationRunner {

    private static final Logger log = LoggerFactory.getLogger(BatchJobParameterRunner.class);

    @Override
    public void run(ApplicationArguments args) {
        // Non-option arguments: e.g. "import", "daily-settlement"
        List<String> nonOptionArgs = args.getNonOptionArgs();
        log.info("Non-option positional arguments: {}", nonOptionArgs);

        // Option arguments: e.g. "--batch-size=500 --dry-run"
        if (args.containsOption("batch-size")) {
            String batchSize = args.getOptionValues("batch-size").get(0);
            log.info("Batch size configured via CLI: {}", batchSize);
        }

        if (args.containsOption("dry-run")) {
            log.warn("DRY RUN MODE ENABLED: Database modifications will be skipped!");
        }
    }
}
```

### Example 3: Multiple Ordered Runners
```java
package com.example.demo.runner;

import org.springframework.boot.CommandLineRunner;
import org.springframework.core.annotation.Order;
import org.springframework.stereotype.Component;

@Component
@Order(1) // Runs FIRST
class NetworkConnectivityChecker implements CommandLineRunner {
    @Override public void run(String... args) {
        System.out.println("Step 1: Network connectivity to payment gateway verified.");
    }
}

@Component
@Order(2) // Runs SECOND
class CacheWarmupRunner implements CommandLineRunner {
    @Override public void run(String... args) {
        System.out.println("Step 2: In-memory product cache pre-warmed.");
    }
}
```

### Example 4: Custom Exit Code Generation on Runner Failure
```java
package com.example.demo.runner;

import org.springframework.boot.ExitCodeGenerator;
import org.springframework.boot.SpringApplication;
import org.springframework.boot.autoconfigure.SpringBootApplication;
import org.springframework.context.annotation.Bean;
import org.springframework.stereotype.Component;

@SpringBootApplication
public class BatchExitApp {

    public static void main(String[] args) {
        // Exits process with custom code if a runner throws an exception
        System.exit(SpringApplication.exit(SpringApplication.run(BatchExitApp.class, args)));
    }

    @Bean
    public ExitCodeGenerator customExitCodeGenerator() {
        return () -> 88; // Return exit code 88 to Kubernetes pod on failure
    }
}

@Component
class FailingHealthCheckRunner implements org.springframework.boot.ApplicationRunner {
    @Override
    public void run(org.springframework.boot.ApplicationArguments args) {
        if (args.containsOption("fail-on-start")) {
            throw new RuntimeException("Forced startup failure for testing exit codes!");
        }
    }
}
```

### Example 5: Testing Runners in Isolation
```java
package com.example.demo.runner;

import org.junit.jupiter.api.Test;
import org.mockito.Mockito;
import org.springframework.boot.DefaultApplicationArguments;

import static org.mockito.Mockito.verify;

class RunnerUnitTest {

    @Test
    void shouldExecuteSeedingWhenDatabaseEmpty() throws Exception {
        RoleRepository mockRepo = Mockito.mock(RoleRepository.class);
        Mockito.when(mockRepo.count()).thenReturn(0L);

        DatabaseSeedRunner runner = new DatabaseSeedRunner(mockRepo);
        
        // Pass simulated CLI arguments
        runner.run(new DefaultApplicationArguments("--seed-data=true"));

        verify(mockRepo).saveAll(Mockito.any());
    }
}
```

---

## 6. Best Practices

1. **Prefer `ApplicationRunner` Over `CommandLineRunner`**: `ApplicationRunner` parses `--key=value` option flags and non-option parameters cleanly, avoiding fragile string-splitting logic.
2. **Always Use `@Order` with Multiple Runners**: Ensure deterministic sequencing by assigning explicit order values to all runners.
3. **Make Startup Data Seeding Idempotent**: Database seed runners must check `repository.count() == 0` or use `UPSERT` statements to prevent duplicate key violations when applications restart.
4. **Never Block Indefinitely in a Runner**: In Kubernetes, a runner that hangs prevents `ApplicationReadyEvent` from firing, causing the Kubernetes readiness probe to fail and killing the pod.
5. **Handle Runner Exceptions Gracefully**: Unhandled exceptions in a runner abort startup. If a task is non-critical (e.g., telemetry ping), catch exceptions and log warnings rather than crashing the service.
6. **Keep Heavy Async Tasks in Separate Threads**: If cache warming takes more than 5 seconds, launch it in a background thread or `@Async` method to let the web server start accepting traffic immediately.

---

## 7. Common Mistakes & Pitfalls

| Mistake | Why It Fails | Practical Remedy |
| :--- | :--- | :--- |
| **Heavy logic in `@PostConstruct` instead of Runner** | Running blocking database calls in `@PostConstruct` blocks the `ApplicationContext` refresh before web servers boot. | Move startup logic to an `ApplicationRunner`. |
| **Non-idempotent database seeding** | Restarting the application runs the seed runner again, crashing on duplicate primary keys. | Check if records exist before inserting. |
| **Hanging/Infinite loop in runner** | The application never enters the "Ready" state; Kubernetes readiness probes fail and restart the pod in an endless crash-loop. | Run long-running tasks in background threads. |
| **Unhandled exceptions killing the app** | Any exception thrown from `run()` halts the JVM and terminates the application process. | Wrap non-critical logic in `try-catch`. |
| **Parsing raw strings manually in `CommandLineRunner`** | Writing custom `arg.startsWith("--")` string parsing is bug-prone. | Use `ApplicationRunner` and `ApplicationArguments`. |
| **Assuming runners execute before Tomcat starts** | In Spring Boot Web, Tomcat starts listening *before* runners execute. | Use `ApplicationContextInitializer` if pre-server logic is strictly required. |

---

## 8. Performance & Security Notes

### Kubernetes Readiness & Liveness Probes
- Spring Boot's internal `ApplicationAvailability` marks the application as **Liveness: CORRECT** when context finishes refreshing, and **Readiness: ACCEPTING_TRAFFIC** only **after** all runners have successfully completed.
- Long-running runners delay readiness probes. Keep runner execution under 2 seconds.

### Security Restrictions
Command-line runners often accept parameters from container manifests. Sanitize inputs to prevent command injection or directory traversal if arguments specify file paths.

---

## 9. Comparison / When to Use What

| Startup Mechanism | Trigger Timing | Arguments Access | Primary Use Case |
| :--- | :--- | :--- | :--- |
| **`@PostConstruct`** | When specific bean is created | None | Validating internal bean invariants |
| **`CommandLineRunner`** | Context refreshed, server running | Raw `String[] args` | Quick scripts, legacy code |
| **`ApplicationRunner`** | Context refreshed, server running | Structured `ApplicationArguments`| **Standard for enterprise startup tasks** |
| **`ApplicationReadyEvent`** | Context ready, runners finished | Event object | Publishing "ready" metrics, external pings |

---

## 10. Interview Questions

<details>
<summary><strong>Q1: What is the primary difference between `CommandLineRunner` and `ApplicationRunner`?</strong></summary>

**Answer:**  
Both interfaces execute code after the `ApplicationContext` has refreshed and the web server is running. The difference is argument presentation:
- `CommandLineRunner.run(String... args)` passes raw, unparsed string arguments.
- `ApplicationRunner.run(ApplicationArguments args)` passes structured `ApplicationArguments`, providing convenient methods to access option arguments (`--key=value`), non-option positional arguments, and parsed option names.
</details>

<details>
<summary><strong>Q2: When exactly do runners execute in the Spring Boot lifecycle?</strong></summary>

**Answer:**  
Runners execute at the very end of `SpringApplication.run()`:
1. `ApplicationContext.refresh()` has fully completed.
2. All eager singletons are initialized.
3. Embedded Tomcat/Jetty web server is started.
4. `callRunners()` invokes all `ApplicationRunner` and `CommandLineRunner` beans.
5. `ApplicationReadyEvent` is published.
</details>

<details>
<summary><strong>Q3: What happens if an `ApplicationRunner` throws an unhandled exception?</strong></summary>

**Answer:**  
The exception propagates up to `SpringApplication.run()`, which catches it, publishes `ApplicationFailedEvent`, initiates container shutdown, and exits the JVM process with a non-zero exit code.
</details>

<details>
<summary><strong>Q4: How do you control the execution order of multiple runners?</strong></summary>

**Answer:**  
Annotate runner classes with `@Order(int)` or implement the `org.springframework.core.Ordered` interface. Runners are sorted using `AnnotationAwareOrderComparator`, where the runner with the lowest integer value executes first.
</details>

<details>
<summary><strong>Q5: Why should you use `ApplicationRunner` instead of `@PostConstruct` for database seeding?</strong></summary>

**Answer:**  
`@PostConstruct` runs during the bean initialization phase while the `ApplicationContext` is still refreshing. At that point, transactions, proxies, and other collaborator beans may not yet be fully initialized. `ApplicationRunner` runs *after* the entire context is fully refreshed and stable, making it safe for database operations.
</details>

<details>
<summary><strong>Q6: How does Spring Boot integrate runner exit codes with the operating system?</strong></summary>

**Answer:**  
By returning exit codes through `ExitCodeGenerator` beans and wrapping the main method in `System.exit(SpringApplication.exit(ctx))`. When an exception occurs or the application terminates, Spring Boot queries all `ExitCodeGenerator` beans and returns the code to the operating system shell.
</details>

<details>
<summary><strong>Q7: How do you mock or test an `ApplicationRunner` in a unit test?</strong></summary>

**Answer:**  
Because `ApplicationRunner` is a simple interface, you can instantiate the class directly with `new MyRunner(mockDependencies)` and pass a `new DefaultApplicationArguments("--flag=true")` instance into the `run()` method without booting a Spring test runner.
</details>

<details>
<summary><strong>Q8: Does a runner delay the Kubernetes Readiness probe?</strong></summary>

**Answer:**  
Yes. Spring Boot sets its internal readiness state (`ReadinessState.ACCEPTING_TRAFFIC`) and exposes the `/actuator/health/readiness` probe as `UP` only **after** all runners have completed successfully. A slow or hung runner delays pod readiness in Kubernetes.
</details>

---

## 11. Practice Exercises

<details>
<summary><strong>Exercise 1: Implement an Option Argument Reader (Easy)</strong></summary>

**Task:**  
Create an `ApplicationRunner` that checks for `--environment=...`. Print the environment name if present, or log a warning if absent.
</details>

<details>
<summary><strong>Exercise 2: Order Two Runners (Easy)</strong></summary>

**Task:**  
Create `RunnerAlpha` (`@Order(2)`) and `RunnerBeta` (`@Order(1)`). Add print statements in both and verify that `RunnerBeta` prints first.
</details>

<details>
<summary><strong>Exercise 3: Idempotent Seed Runner (Medium)</strong></summary>

**Task:**  
Write a seed runner that checks if an in-memory `List<String>` contains `"ADMIN"`. If absent, add it; if present, do nothing. Verify that calling the runner multiple times never creates duplicates.
</details>

<details>
<summary><strong>Exercise 4: Test Custom Exit Code (Medium)</strong></summary>

**Task:**  
Configure a runner to throw an exception that implements `ExitCodeGenerator` returning code `42`. Verify that `SpringApplication.exit()` produces return code `42`.
</details>

<details>
<summary><strong>Exercise 5: Non-Blocking Background Warmup (Hard)</strong></summary>

**Task:**  
Create an `ApplicationRunner` that triggers a cache pre-warm task asynchronously using `CompletableFuture.runAsync()`. Verify that the runner returns immediately (<10ms) while the background task completes 3 seconds later.
</details>

<details>
<summary><strong>Exercise 6: Mock ApplicationArguments in Unit Test (Hard)</strong></summary>

**Task:**  
Write a unit test for an `ApplicationRunner` using Mockito. Verify that passing `--mode=strict` triggers a specific collaborator method while omitting it triggers a fallback method.
</details>

---

## 12. Summary / Cheat Sheet

| Feature / Concept | Syntax / Code Snippet | Key Guarantee |
| :--- | :--- | :--- |
| **`ApplicationRunner`** | `implements ApplicationRunner` | Structured argument access (`--key=value`) |
| **`CommandLineRunner`**| `implements CommandLineRunner` | Raw string arguments (`String... args`) |
| **Execution Timing** | End of `SpringApplication.run()` | Context refreshed, server listening |
| **Ordering** | `@Order(1)` | Lowest integer executes first |
| **Option Arguments** | `args.getOptionValues("key")` | Returns parsed values for `--key` |
| **Non-Option Args** | `args.getNonOptionArgs()` | Returns positional arguments |
| **Exit Code** | `implements ExitCodeGenerator` | Custom shell exit code on termination |

---

## 13. Further Reading
- [Official Spring Boot Reference: ApplicationRunner and CommandLineRunner](https://docs.spring.io/spring-boot/docs/current/reference/html/features.html#features.spring-application.application-runner)
- [Spring ApplicationArguments Javadoc](https://docs.spring.io/spring-boot/docs/current/api/org/springframework/boot/ApplicationArguments.html)
- [Kubernetes Container Lifecycle and Probes](https://kubernetes.io/docs/concepts/workloads/pods/pod-lifecycle/)
