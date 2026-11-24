# Banner Customization & SpringApplicationBuilder

> Master application bootstrapping customizations in Spring Boot: the mechanics of `SpringApplication`, fluent hierarchy chaining with `SpringApplicationBuilder`, custom ANSI-colored `banner.txt`, dynamic banner variables, and programmatic telemetry emitters.

| Metadata | Value |
| :--- | :--- |
| **Level** | Beginner |
| **Prerequisites** | [First Spring Boot Application Walkthrough](file:///03-backend/spring-boot/00-setup/05-first-application-walkthrough.md), [IoC Container & ApplicationContext Internals](file:///03-backend/spring-boot/01-core-spring/01-ioc-container-and-application-context.md) |
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
Standard static `main()` methods (`SpringApplication.run(App.class, args)`) provide a one-line entry point, but real-world enterprise deployments often require fine-grained customization: suppressing console ASCII banners in automated CI test runners, programmatically injecting fallback default properties, configuring parent-child context hierarchies, and branding internal platform frameworks with corporate identifiers.

### Historical Evolution
- **Spring Boot 1.0 (2014)**: Introduced the iconic ASCII art "Spring" console banner.
- **Spring Boot 1.3 - 2.x**: Added ANSI color support (`${AnsiColor.BRIGHT_RED}`) and image banner rendering.
- **Spring Boot 3.x - 3.4+**: Added modern build-version metadata variables, structured logging integration, and deprecated legacy image banners in favor of lightweight text streams.

### Real-World Relevance
Customizing the `SpringApplication` instance and creating corporate banners provides immediate visual feedback to developers and operations teams regarding running versions, active profiles, and platform compliance.

---

## 2. Core Concepts

### 1. `SpringApplication` Customization
Instead of the static `run()` method, developers can instantiate and configure `SpringApplication` directly:
```java
SpringApplication app = new SpringApplication(DemoApplication.class);
app.setBannerMode(Banner.Mode.OFF);
app.setWebApplicationType(WebApplicationType.SERVLET);
app.run(args);
```

### 2. `SpringApplicationBuilder`
A fluent builder API designed for constructing complex application configurations, listeners, initializers, and hierarchical parent-child contexts.

### 3. Banner Modes
- **`Banner.Mode.CONSOLE` (Default)**: Prints the ASCII banner to `System.out`.
- **`Banner.Mode.LOG`**: Emits the banner through the logging subsystem (Logback / SLF4J) at `INFO` level.
- **`Banner.Mode.OFF`**: Completely disables banner printing (ideal for CI pipelines, batch jobs, and serverless runtimes).

### 4. Dynamic Banner Variables (`banner.txt`)
Spring Boot exposes dynamic variables inside `src/main/resources/banner.txt`:
- `${spring-boot.version}`: Current Spring Boot version.
- `${application.version}`: Project version extracted from `pom.xml` via MANIFEST.MF.
- `${application.title}`: Project title from MANIFEST.MF.
- `${AnsiColor.BRIGHT_CYAN}`, `${AnsiBackground.DEFAULT}`, `${AnsiStyle.BOLD}`: ANSI styling.

---

## 3. How It Works Internally

### Banner Resolution & Printing Lifecycle

```
+-------------------------------------------------------------------------+
|                  1. Entry: new SpringApplication(App.class).run(args)   |
+------------------------------------+------------------------------------+
                                     |
                                     v
+-------------------------------------------------------------------------+
|                  2. Prepare Environment                                 |
|  - Reads application.yaml, system properties, and CLI args             |
+------------------------------------+------------------------------------+
                                     |
                                     v
+-------------------------------------------------------------------------+
|                  3. Print Banner (Before Context Creation!)             |
|  - Checks banner mode (`spring.main.banner-mode`)                       |
|  - IF OFF: Skipped.                                                     |
|  - Searches for `banner.txt` in classpath (`src/main/resources/`)       |
|    * Found: Evaluates ANSI colors and `${...}` variables                |
|    * Not Found: Renders default Spring Boot ASCII banner                |
+------------------------------------+------------------------------------+
                                     |
                                     v
+-------------------------------------------------------------------------+
|                  4. Create & Refresh ApplicationContext                 |
+-------------------------------------------------------------------------+
```

---

## 4. Syntax & Usage

### Sample ANSI Colored `banner.txt`
```text
${AnsiColor.BRIGHT_CYAN}
   _  _____ __  __ ___   ___ _        _    __                     
  /_\| ___ |  \/  | __| | _ \ |__ _ _| |_ / _|___ _ _ _ __  ___  
 / _ \ ___|| |\/| | _|  |  _/ / _` |  _|  _| _ \ '_| '  \/ -_) 
/_/ \_\    |_|  |_|___| |_| |_\__,_|\__|_| \___/_| |_|_|_\___| 
${AnsiColor.BRIGHT_GREEN}
 :: Acme Enterprise Platform ::                  (v${application.version})
 :: Powered by Spring Boot ::                    (v${spring-boot.version})
${AnsiColor.DEFAULT}
```

### Disabling Banner in `application.yaml`
```yaml
spring:
  main:
    banner-mode: "off" # Options: console, log, off
```

---

## 5. Practical Examples

### Example 1: Full Programmatic `SpringApplication` Customization
```java
package com.example.demo;

import org.springframework.boot.Banner;
import org.springframework.boot.SpringApplication;
import org.springframework.boot.WebApplicationType;
import org.springframework.boot.autoconfigure.SpringBootApplication;

import java.util.Map;

@SpringBootApplication
public class CustomApp {

    public static void main(String[] args) {
        SpringApplication app = new SpringApplication(CustomApp.class);

        // 1. Configure Banner
        app.setBannerMode(Banner.Mode.CONSOLE);

        // 2. Set Default Fallback Properties
        app.setDefaultProperties(Map.of(
            "server.port", "8085",
            "app.default-timeout", "10s"
        ));

        // 3. Explicitly enforce Web Application Type
        app.setWebApplicationType(WebApplicationType.SERVLET);

        // 4. Run application
        app.run(args);
    }
}
```

### Example 2: Fluent Chaining with `SpringApplicationBuilder`
```java
package com.example.demo;

import org.springframework.boot.Banner;
import org.springframework.boot.builder.SpringApplicationBuilder;

public class BuilderApp {

    public static void main(String[] args) {
        new SpringApplicationBuilder()
            .sources(CustomApp.class)
            .bannerMode(Banner.Mode.OFF)
            .profiles("dev", "local-db")
            .properties("server.port=9099")
            .logStartupInfo(true)
            .run(args);
    }
}
```

### Example 3: Parent-Child Context Hierarchy via `SpringApplicationBuilder`
```java
package com.example.demo.hierarchy;

import org.springframework.boot.builder.SpringApplicationBuilder;
import org.springframework.context.annotation.Bean;
import org.springframework.context.annotation.Configuration;

@Configuration(proxyBeanMethods = false)
class SharedInfrastructureConfig {
    @Bean
    public String sharedDatabasePool() { return "HikariPool-Master-Primary"; }
}

@Configuration(proxyBeanMethods = false)
class CustomerWebConfig {
    @Bean
    public String customerController(String sharedDatabasePool) {
        return "CustomerController using -> " + sharedDatabasePool;
    }
}

public class MultiContextHierarchyApp {

    public static void main(String[] args) {
        // Parent context manages shared infrastructure; child manages web API
        new SpringApplicationBuilder()
            .parent(SharedInfrastructureConfig.class)
            .web(org.springframework.boot.WebApplicationType.NONE)
            .child(CustomerWebConfig.class)
            .web(org.springframework.boot.WebApplicationType.SERVLET)
            .run(args);
    }
}
```

### Example 4: Large Image Banner Anti-Pattern & Modern Text Remediation
```java
// ANTI-PATTERN: Placing large JPEG/PNG image banners in resources
// FLAW:
// 1. Spring Boot must convert pixels to ASCII character blocks, wasting CPU during startup.
// 2. Can crash in headless Linux CI environments with no font or color support.
// 3. Deprecated in recent Spring Boot versions in favor of text banners.

// REMEDIATION:
// Use lightweight text banner in `src/main/resources/banner.txt`.
// It loads in <1 millisecond and has 100% deterministic rendering across all platforms.
```

### Example 5: Custom Programmatic `Banner` Implementation
```java
package com.example.demo.banner;

import org.springframework.boot.Banner;
import org.springframework.boot.SpringApplication;
import org.springframework.core.env.Environment;

import java.io.PrintStream;
import java.time.Instant;

public class JsonTelemetryBanner implements Banner {

    @Override
    public void printBanner(Environment environment, Class<?> sourceClass, PrintStream out) {
        // Emits structured JSON banner for container telemetry indexers
        String jsonBanner = String.format("""
            {"event":"APPLICATION_BOOTSTRAP","source":"%s","active_profiles":"%s","timestamp":"%s"}
            """, 
            sourceClass.getSimpleName(), 
            String.join(",", environment.getActiveProfiles()), 
            Instant.now()
        );
        out.println(jsonBanner);
    }

    public static void main(String[] args) {
        SpringApplication app = new SpringApplication(com.example.demo.CustomApp.class);
        app.setBanner(new JsonTelemetryBanner());
        app.run(args);
    }
}
```

---

## 6. Best Practices

1. **Disable Banners in CI/CD and Automated Tests**: Set `spring.main.banner-mode=off` in test configurations to keep test output logs clean and uncluttered.
2. **Never Put Passwords or Secrets in Banners**: Banners print to standard console output and are frequently indexed by log collectors; never print sensitive tokens.
3. **Use `banner.txt` over Programmatic Banners**: Placing `banner.txt` in `src/main/resources/` keeps presentation separate from Java code.
4. **Use `${application.version}` Dynamically**: Reference `${application.version}` rather than hardcoding version numbers inside `banner.txt`.
5. **Use `SpringApplicationBuilder` for Parent-Child Contexts**: When sharing database or cache infrastructure across multiple microservice entry points, use `SpringApplicationBuilder.parent().child()`.
6. **Prefer Text Over Image Banners**: Text banners execute instantly, whereas image banners consume CPU cycles during the critical startup path.

---

## 7. Common Mistakes & Pitfalls

| Mistake | Why It Fails | Practical Remedy |
| :--- | :--- | :--- |
| **`${application.version}` resolving to empty string** | The project is not packaged as a JAR or the Maven/Gradle build did not write version metadata to `MANIFEST.MF`. | Configure `maven-jar-plugin` to generate implementation version entries. |
| **Using deprecated image banner formats** | Image banner support was deprecated/removed in recent Boot releases; image fails to render. | Migrate to standard ASCII text banners. |
| **ANSI color codes corrupting CI log files** | Plaintext terminal viewers in Jenkins/GitLab show raw escape sequences (e.g., `\033[31m`). | Disable ANSI colors in CI using `spring.output.ansi.enabled=never`. |
| **Typo in `banner-mode` key** | Writing `spring.banner.mode` (legacy) instead of `spring.main.banner-mode`. | Use modern key: `spring.main.banner-mode`. |
| **Putting heavy logic in a programmatic `Banner`** | Blocking operations in `Banner.printBanner()` delay application startup before the context is even created. | Keep banner printing purely algorithmic. |
| **Overlapping parent-child context port bindings** | Both parent and child contexts attempting to boot servlet web servers cause port collision errors. | Set parent context to `WebApplicationType.NONE`. |

---

## 8. Performance & Security Notes

### Startup CPU Savings
Disabling the console banner saves between 10 to 50 milliseconds of startup latency. In serverless functions (AWS Lambda) or CLI utilities where startup time directly affects billing and execution latency, always set `bannerMode(Banner.Mode.OFF)`.

### Security Information Leakage
Printing detailed patch versions of Spring Boot and third-party libraries in publicly accessible terminal logs provides attackers with reconnaissance data on known CVEs. In public-facing or multi-tenant production clusters, suppress version output in banners.

---

## 9. Comparison / When to Use What

| Approach | Syntax / Configuration | Flexibility | Best For |
| :--- | :--- | :--- | :--- |
| **`SpringApplication.run()`** | Static single-line call | Standard | Standard microservice entry points |
| **`SpringApplicationBuilder`** | Fluent chained builder | **High** | Multi-context hierarchies, fluent configs |
| **`banner.txt`** | Text file in `src/main/resources` | Declarative | Standard team branding and version display |
| **Programmatic `Banner`** | `implements Banner` | **Maximum** | Custom JSON output, telemetry integration |

---

## 10. Interview Questions

<details>
<summary><strong>Q1: What are the three available Banner modes in Spring Boot?</strong></summary>

**Answer:**  
1. `Banner.Mode.CONSOLE`: Prints the banner directly to `System.out` (default).
2. `Banner.Mode.LOG`: Emits the banner through the logging framework (SLF4J/Logback) at `INFO` level.
3. `Banner.Mode.OFF`: Completely disables banner printing.
</details>

<details>
<summary><strong>Q2: When in the startup sequence does Spring Boot render the banner?</strong></summary>

**Answer:**  
Spring Boot renders the banner **after** the `Environment` has been prepared, but **before** the `ApplicationContext` is created or refreshed. This allows the banner to display environment variables, active profiles, and version numbers.
</details>

<details>
<summary><strong>Q3: How do you pass dynamic version variables into `banner.txt`?</strong></summary>

**Answer:**  
By referencing built-in variables:
- `${spring-boot.version}`: Returns the running version of Spring Boot.
- `${application.version}`: Extracted from the `Implementation-Version` entry in the JAR's `META-INF/MANIFEST.MF`.
- `${application.title}`: Extracted from `Implementation-Title` in `MANIFEST.MF`.
</details>

<details>
<summary><strong>Q4: What is `SpringApplicationBuilder` and when would you use it?</strong></summary>

**Answer:**  
`SpringApplicationBuilder` is a fluent builder wrapper around `SpringApplication`. It allows constructing hierarchical parent-child application contexts, chaining profiles, setting banner modes, and configuring listeners in an expressive, readable fluent syntax.
</details>

<details>
<summary><strong>Q5: Why does `${application.version}` sometimes fail to display when running from an IDE?</strong></summary>

**Answer:**  
When running directly inside an IDE (like IntelliJ or Eclipse), the application executes from unpacked compiled `.class` files in `target/classes`, not from a packaged `.jar` archive. Because no `META-INF/MANIFEST.MF` file has been packaged, Spring Boot cannot extract the implementation version and prints an empty string.
</details>

<details>
<summary><strong>Q6: How can you disable ANSI terminal colors in a CI/CD build?</strong></summary>

**Answer:**  
Set the property `spring.output.ansi.enabled=never` in your test/CI configuration, or pass `--spring.output.ansi.enabled=never` as a CLI argument.
</details>

<details>
<summary><strong>Q7: Can a Spring Boot application have multiple `ApplicationContext` instances?</strong></summary>

**Answer:**  
Yes, by creating a **Parent-Child Context Hierarchy** using `SpringApplicationBuilder.parent(...).child(...)`. The child context inherits all beans defined in the parent context, but the parent context remains completely isolated from beans in the child context.
</details>

<details>
<summary><strong>Q8: How do you completely disable the banner in `application.yaml`?</strong></summary>

**Answer:**  
```yaml
spring:
  main:
    banner-mode: "off"
```
</details>

---

## 11. Practice Exercises

<details>
<summary><strong>Exercise 1: Create a Custom `banner.txt` (Easy)</strong></summary>

**Task:**  
Create `src/main/resources/banner.txt`. Use an online ASCII generator to create your project name. Add `${AnsiColor.BRIGHT_BLUE}` and print `${spring-boot.version}`. Run the app and verify the colored output.
</details>

<details>
<summary><strong>Exercise 2: Disable Banner via Property (Easy)</strong></summary>

**Task:**  
Set `spring.main.banner-mode: "off"` in `application.yaml`. Boot the application and verify that no banner is printed to standard output.
</details>

<details>
<summary><strong>Exercise 3: Refactor to `SpringApplicationBuilder` (Medium)</strong></summary>

**Task:**  
Refactor the static `main()` method of your application to use `SpringApplicationBuilder`. Configure it to disable the banner and set a custom server port `8091`.
</details>

<details>
<summary><strong>Exercise 4: Verify `MANIFEST.MF` Version Extraction (Medium)</strong></summary>

**Task:**  
Configure `maven-jar-plugin` to generate `Implementation-Version` in `MANIFEST.MF`. Package the app (`./mvnw package`) and run `java -jar target/*.jar`. Verify that `${application.version}` prints the correct Maven POM version in the banner.
</details>

<details>
<summary><strong>Exercise 5: Implement Custom Programmatic Banner (Hard)</strong></summary>

**Task:**  
Implement the `org.springframework.boot.Banner` interface. In `printBanner()`, calculate the available JVM memory (`Runtime.getRuntime().freeMemory()`) and print an ASCII box showing available memory at boot time.
</details>

<details>
<summary><strong>Exercise 6: Build Parent-Child Context Pipeline (Hard)</strong></summary>

**Task:**  
Using `SpringApplicationBuilder`, build a non-web parent context containing an `AuditService` and two child web contexts (`ClientApiConfig` on port 8081, `AdminApiConfig` on port 8082). Verify both child contexts can invoke the parent's `AuditService`.
</details>

---

## 12. Summary / Cheat Sheet

| Feature / Artifact | Syntax / Command | Purpose |
| :--- | :--- | :--- |
| **Custom Banner File** | `src/main/resources/banner.txt` | Custom ASCII art and metadata |
| **Disable Banner** | `spring.main.banner-mode: "off"` | Completely silences banner in CI/CD |
| **Fluent Builder** | `new SpringApplicationBuilder().sources(...).run(args)` | Expressive bootstrap configuration |
| **Parent-Child Context**| `.parent(Parent.class).child(Child.class)`| Establishes context hierarchies |
| **Color Styling** | `${AnsiColor.BRIGHT_RED}` | ANSI color terminal formatting |
| **Version Variable** | `${spring-boot.version}` | Dynamic Spring Boot version display |
| **App Version Variable**| `${application.version}` | Dynamic version from `MANIFEST.MF` |

---

## 13. Further Reading
- [Official Spring Boot Reference: Customizing the Banner](https://docs.spring.io/spring-boot/docs/current/reference/html/features.html#features.spring-application.banner)
- [SpringApplicationBuilder Javadoc](https://docs.spring.io/spring-boot/docs/current/api/org/springframework/boot/builder/SpringApplicationBuilder.html)
- [Spring AnsiColor and AnsiOutput Reference](https://docs.spring.io/spring-boot/docs/current/api/org/springframework/boot/ansi/AnsiColor.html)
