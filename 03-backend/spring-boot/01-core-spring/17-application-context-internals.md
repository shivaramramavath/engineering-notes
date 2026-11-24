# ApplicationContext Internals: The 12 Steps of refresh()

> An exhaustive architectural journey through the heart of Spring Framework: the 12 phases of `AbstractApplicationContext.refresh()`, the internal three-level singleton cache of `DefaultSingletonBeanRegistry`, and lifecycle shutdown hooks.

| Metadata | Value |
| :--- | :--- |
| **Level** | Advanced |
| **Prerequisites** | [IoC Container & ApplicationContext Internals](file:///03-backend/spring-boot/01-core-spring/01-ioc-container-and-application-context.md), [BeanPostProcessors & FactoryPostProcessors](file:///03-backend/spring-boot/01-core-spring/07-bean-post-processors-and-factory-post-processors.md) |
| **Time to Read** | 30 minutes |
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
When a developer invokes `SpringApplication.run()` or `new AnnotationConfigApplicationContext()`, what actually happens inside the JVM? Behind that single invocation lies `AbstractApplicationContext.refresh()`, one of the most meticulously engineered methods in Java history. It coordinates configuration parsing, placeholder resolution, post-processor sorting, proxy generation, embedded server bootstrap, and event multicaster setup. Without understanding these phases, debugging bootstrap deadlocks, classloading conflicts, or lifecycle timing errors is nearly impossible.

### Historical Evolution
- **Spring 1.0 (2003)**: Defined the basic `refresh()` template method in `AbstractApplicationContext`.
- **Spring 3.x - 4.x**: Enhanced with dynamic JavaConfig parsing and condition evaluation.
- **Spring 6.x & Boot 3.x**: Optimized with `StartupStep` recording, Ahead-of-Time (AOT) bytecode compilation, and virtual thread readiness.

### Real-World Relevance
`refresh()` is the universal engine driving all flavors of Spring: Spring MVC, Spring WebFlux, Spring Batch, Spring Cloud, and Spring Boot. Mastering its 12 phases turns senior engineers into platform architects.

---

## 2. Core Concepts

### The 12 Sacred Phases of `refresh()`
Inside `org.springframework.context.support.AbstractApplicationContext`:
1. **`prepareRefresh()`**: Sets startup timestamp, activates property sources, validates required properties.
2. **`obtainFreshBeanFactory()`**: Instructs the subclass to refresh its internal `DefaultListableBeanFactory`.
3. **`prepareBeanFactory(beanFactory)`**: Configures standard classloader, SpEL resolver, property editors, and ignores specific Aware interfaces for standard DI.
4. **`postProcessBeanFactory(beanFactory)`**: Template method allowing subclasses (like Web contexts) to register web-specific scopes and beans.
5. **`invokeBeanFactoryPostProcessors(beanFactory)`**: Executes all registered `BeanDefinitionRegistryPostProcessors` and `BeanFactoryPostProcessors` (e.g., parses `@Configuration`).
6. **`registerBeanPostProcessors(beanFactory)`**: Discovers, sorts, and registers all `BeanPostProcessor` beans.
7. **`initMessageSource()`**: Initializes i18n internationalization message resolver.
8. **`initApplicationEventMulticaster()`**: Initializes the event broadcaster (`SimpleApplicationEventMulticaster`).
9. **`onRefresh()`**: Template method. In Spring Boot Web, **this is where the embedded Tomcat/Netty server is created and started!**
10. **`registerListeners()`**: Registers all `ApplicationListener` beans with the multicaster and publishes early events.
11. **`finishBeanFactoryInitialization(beanFactory)`**: **The heavy lifting phase**: Initializes the `ConversionService`, resolves remaining placeholders, and **pre-instantiates all eager singleton beans**.
12. **`finishRefresh()`**: Publishes `ContextRefreshedEvent`, starts `Lifecycle` beans, and initializes LiveReload/JMX.

---

## 3. How It Works Internally

### The `refresh()` Bootstrap Architecture

```
+-------------------------------------------------------------------------+
|                  AbstractApplicationContext.refresh()                   |
+------------------------------------+------------------------------------+
                                     |
    +--------------------------------+--------------------------------+
    | 1. prepareRefresh()            | 7. initMessageSource()         |
    | 2. obtainFreshBeanFactory()    | 8. initApplicationEventMultic..|
    | 3. prepareBeanFactory()        | 9. onRefresh() (BOOT TOMCAT!)  |
    | 4. postProcessBeanFactory()    | 10. registerListeners()        |
    | 5. invokeBeanFactoryPostProc.. | 11. finishBeanFactoryInit()    |
    | 6. registerBeanPostProcessors()| 12. finishRefresh()            |
    +--------------------------------+--------------------------------+
                                     |
                                     v
+-------------------------------------------------------------------------+
|      Phase 11 Detail: finishBeanFactoryInitialization()                 |
|                                                                         |
|  DefaultSingletonBeanRegistry (The 3-Level Singleton Cache):            |
|                                                                         |
|  [ Level 1: singletonObjects ]                                          |
|    - Holds fully initialized, post-processed, ready-to-use beans.       |
|                                                                         |
|  [ Level 2: earlySingletonObjects ]                                     |
|    - Holds exposed early raw/proxy instances to resolve circular refs.  |
|                                                                         |
|  [ Level 3: singletonFactories ]                                        |
|    - Holds ObjectFactory lambda suppliers that generate early proxies.  |
+-------------------------------------------------------------------------+
```

---

## 4. Syntax & Usage

### Listening to Context Lifecycle Events
```java
@Component
public class LifecycleMonitor {

    @EventListener
    public void onContextRefreshed(ContextRefreshedEvent event) {
        System.out.println("ApplicationContext fully booted and all singletons ready!");
    }

    @EventListener
    public void onContextClosed(ContextClosedEvent event) {
        System.out.println("ApplicationContext closing: Releasing all resources.");
    }
}
```

---

## 5. Practical Examples

### Example 1: Tracing `refresh()` Phases Programmatically
```java
package com.example.demo.lifecycle;

import org.springframework.boot.CommandLineRunner;
import org.springframework.context.ApplicationContext;
import org.springframework.context.support.AbstractApplicationContext;
import org.springframework.stereotype.Component;

@Component
public class ContextInspectionRunner implements CommandLineRunner {

    private final ApplicationContext context;

    public ContextInspectionRunner(ApplicationContext context) {
        this.context = context;
    }

    @Override
    public void run(String... args) {
        System.out.println("=== Context Inspection Telemetry ===");
        System.out.println("Context ID: " + context.getId());
        System.out.println("Application Name: " + context.getApplicationName());
        System.out.println("Startup Timestamp: " + context.getStartupDate());
        System.out.println("Total Registered Beans: " + context.getBeanDefinitionCount());

        if (context instanceof AbstractApplicationContext aac) {
            System.out.println("Context Active? " + aac.isActive());
            System.out.println("Context Running? " + aac.isRunning());
        }
    }
}
```

### Example 2: Inspecting Internal Singleton Cache Statistics
```java
package com.example.demo.cache;

import org.springframework.beans.factory.support.DefaultListableBeanFactory;
import org.springframework.context.ConfigurableApplicationContext;
import org.springframework.stereotype.Component;

@Component
public class SingletonRegistryAuditor {

    public SingletonRegistryAuditor(ConfigurableApplicationContext context) {
        DefaultListableBeanFactory beanFactory = (DefaultListableBeanFactory) context.getBeanFactory();

        int singletonCount = beanFactory.getSingletonCount();
        String[] singletonNames = beanFactory.getSingletonNames();

        System.out.printf("[REGISTRY AUDITOR] Total Singletons Cached: %d%n", singletonCount);
        System.out.println("Sample cached singletons: " + 
            java.util.Arrays.stream(singletonNames).limit(5).toList());
    }
}
```

### Example 3: Programmatic Bootstrap with Shutdown Hook
```java
package com.example.demo.standalone;

import org.springframework.context.annotation.AnnotationConfigApplicationContext;
import org.springframework.context.annotation.Bean;
import org.springframework.context.annotation.Configuration;

import jakarta.annotation.PreDestroy;

class WorkerDaemon {
    public void execute() { System.out.println("Daemon executing work..."); }

    @PreDestroy
    public void shutdown() { System.out.println("Daemon gracefully stopped via JVM Shutdown Hook."); }
}

@Configuration(proxyBeanMethods = false)
class DaemonConfig {
    @Bean
    public WorkerDaemon workerDaemon() { return new WorkerDaemon(); }
}

public class StandaloneApp {
    public static void main(String[] args) {
        var context = new AnnotationConfigApplicationContext();
        context.register(DaemonConfig.class);

        // Registers JVM runtime shutdown hook (SIGTERM handling)
        context.registerShutdownHook();

        context.refresh(); // Executes the 12 steps

        WorkerDaemon daemon = context.getBean(WorkerDaemon.class);
        daemon.execute();

        // JVM terminates, shutdown hook triggers context.close() cleanly
    }
}
```

### Example 4: Unclosed Context Anti-Pattern & Remediation
```java
// ANTI-PATTERN: Standalone batch app forgetting to close ApplicationContext
public class LeakyBatchJob {
    public static void main(String[] args) {
        var context = new AnnotationConfigApplicationContext(BatchConfig.class);
        BatchService service = context.getBean(BatchService.class);
        service.runJob();
        
        // FLAW: Method exits without context.close() or registerShutdownHook()!
        // HikariCP connection pools, ThreadPoolTaskExecutors, and @PreDestroy methods
        // are NEVER executed, leaving database connections hung on the DB server!
    }
}

// REMEDIATION: Always use try-with-resources or registerShutdownHook()
public class CleanBatchJob {
    public static void main(String[] args) {
        // AnnotationConfigApplicationContext implements AutoCloseable!
        try (var context = new AnnotationConfigApplicationContext(BatchConfig.class)) {
            BatchService service = context.getBean(BatchService.class);
            service.runJob();
            // Automatically invokes context.close(), executing all @PreDestroy hooks cleanly!
        }
    }
}
```

### Example 5: Environment Modification via `ApplicationContextInitializer`
```java
package com.example.demo.initializer;

import org.springframework.context.ApplicationContextInitializer;
import org.springframework.context.ConfigurableApplicationContext;
import org.springframework.core.env.MapPropertySource;

import java.util.Map;

// Executes during Step 1 (prepareRefresh) before beans are loaded
public class CustomEarlyEnvironmentInitializer 
    implements ApplicationContextInitializer<ConfigurableApplicationContext> {

    @Override
    public void initialize(ConfigurableApplicationContext applicationContext) {
        System.out.println("[INITIALIZER] Executing before refresh(). Injecting dynamic flags...");
        applicationContext.getEnvironment().getPropertySources().addFirst(
            new MapPropertySource("earlyFlags", Map.of("app.bootstrap.node-id", "NODE-ALPHA-01"))
        );
    }
}
```

---

## 6. Best Practices

1. **Always Register Shutdown Hooks for Standalone Apps**: If running outside a servlet container, always call `context.registerShutdownHook()` to ensure `@PreDestroy` and resource pools close on SIGTERM.
2. **Use Try-With-Resources in Tests and CLI Scripts**: `ConfigurableApplicationContext` implements `AutoCloseable`; wrapping context in try-with-resources guarantees cleanup.
3. **Never Call `refresh()` on a Running Spring Boot App**: `SpringApplication.run()` invokes `refresh()` automatically. Calling `refresh()` again throws `IllegalStateException`.
4. **Keep Step 9 (`onRefresh`) Web Containers Non-Blocking**: Do not introduce manual thread blocking in embedded Tomcat initialization.
5. **Use `ApplicationStartup` to Profile Steps**: In production environments with strict SLA limits on startup duration, track `refresh()` metrics using `BufferingApplicationStartup`.
6. **Prefer `ApplicationContextInitializer` for Pre-Boot Logic**: When configuration must be altered before bean definitions are parsed, use an `ApplicationContextInitializer`.

---

## 7. Common Mistakes & Pitfalls

| Mistake | Why It Fails | Practical Remedy |
| :--- | :--- | :--- |
| **Calling `refresh()` twice** | `refresh()` can only be called once on standard contexts; throws `IllegalStateException: GenericApplicationContext does not support multiple refresh attempts`. | Create a new context instance if a reboot is needed. |
| **Forgetting `registerShutdownHook()`** | Standalone apps terminating via `SIGTERM` kill the process without running `@PreDestroy` or flushing connection pools. | Call `context.registerShutdownHook()`. |
| **Accessing beans during Step 5 (BFPP)** | Calling `getBean()` inside a `BeanFactoryPostProcessor` instantiates the bean before post-processors are registered. | Only inspect/mutate `BeanDefinition` metadata. |
| **Calling `getBean()` before `refresh()`** | Invoking `context.getBean()` on an un-refreshed context throws `IllegalStateException: BeanFactory not initialized`. | Ensure `context.refresh()` completes before querying beans. |
| **Blocking in `ContextRefreshedEvent`** | A slow listener on `ContextRefreshedEvent` blocks the startup thread, delaying HTTP traffic acceptance. | Offload long tasks asynchronously using `@Async`. |
| **Missing required property validation** | Forgetting to declare required properties in Step 1 causes the application to crash later during user traffic. | Use `environment.setRequiredProperties("db.url", "auth.secret")`. |

---

## 8. Performance & Security Notes

### Profiling `refresh()` with `ApplicationStartup`
Spring Framework 5.3+ provides `ApplicationStartup` to measure exact nanosecond durations for each of the 12 phases:
```java
AnnotationConfigApplicationContext context = new AnnotationConfigApplicationContext();
context.setApplicationStartup(new FlightRecorderApplicationStartup()); // Records to Java Flight Recorder (JFR)
context.refresh();
```

### Security of Context Closure
Closing an `ApplicationContext` must be restricted. Endpoints like Actuator `/actuator/shutdown` must be disabled by default or locked down with multi-factor authentication, as invoking `close()` terminates the application process.

---

## 9. Comparison / When to Use What

| ApplicationContext Subclass | Environment | Key Characteristic | Best For |
| :--- | :--- | :--- | :--- |
| **`AnnotationConfigApplicationContext`** | Standalone / CLI | Annotation-driven JavaConfig | Batch jobs, command-line tools |
| **`AnnotationConfigServletWebServerApplicationContext`** | Web (Servlet) | Boots embedded Tomcat/Jetty in Step 9 | Standard Spring Boot REST APIs |
| **`AnnotationConfigReactiveWebServerApplicationContext`**| Web (Reactive) | Boots embedded Netty in Step 9 | High-throughput Spring WebFlux APIs |
| **`GenericApplicationContext`** | Lightweight / Serverless | Supports functional bean registration | AWS Lambda, GraalVM AOT native images |

---

## 10. Interview Questions

<details>
<summary><strong>Q1: What are the primary responsibilities of `AbstractApplicationContext.refresh()`?</strong></summary>

**Answer:**  
`refresh()` completely initializes the Spring IoC container:
1. Prepares the environment and validates required properties.
2. Executes `BeanFactoryPostProcessors` (parsing `@Configuration` and resolving properties).
3. Registers `BeanPostProcessors` (AOP proxy creators, autowiring processors).
4. Boots embedded web servers (Tomcat/Netty) in `onRefresh()`.
5. Pre-instantiates all eager singleton beans.
6. Publishes `ContextRefreshedEvent` and starts lifecycle components.
</details>

<details>
<summary><strong>Q2: Where in the `refresh()` sequence does Spring Boot start the embedded Tomcat web server?</strong></summary>

**Answer:**  
In **Step 9: `onRefresh()`**. In a servlet web environment, `AnnotationConfigServletWebServerApplicationContext` overrides `onRefresh()` to create the `WebServer` instance (Tomcat, Jetty, or Undertow) and bind the HTTP connector port.
</details>

<details>
<summary><strong>Q3: What are the three levels of the singleton cache in `DefaultSingletonBeanRegistry`?</strong></summary>

**Answer:**  
1. **Level 1 (`singletonObjects`)**: Map holding fully initialized, completely post-processed singleton bean instances ready for application use.
2. **Level 2 (`earlySingletonObjects`)**: Map holding early, partially constructed bean references (or early proxies) to resolve circular dependencies.
3. **Level 3 (`singletonFactories`)**: Map holding `ObjectFactory<?>` lambda suppliers that generate early exposed references (creating AOP proxies if necessary) when a circular dependency is detected.
</details>

<details>
<summary><strong>Q4: What happens if an unhandled exception is thrown during `refresh()`?</strong></summary>

**Answer:**  
Spring catches the exception, logs a fatal error, and automatically invokes **`destroyBeans()`** and **`cancelRefresh()`**. It clears all registered singletons, calls destruction callbacks on any beans that were already created, releases file locks, and marks the context as inactive.
</details>

<details>
<summary><strong>Q5: What is the purpose of `context.registerShutdownHook()`?</strong></summary>

**Answer:**  
It registers a standard Java JVM shutdown hook (`Runtime.getRuntime().addShutdownHook(...)`). When the JVM receives an external termination signal (`SIGTERM`, `Ctrl+C`), the hook executes and calls `context.close()`, guaranteeing that all `@PreDestroy` methods run and resources (connection pools, thread pools) are closed cleanly before the process exits.
</details>

<details>
<summary><strong>Q6: Why is Step 11 (`finishBeanFactoryInitialization`) typically the slowest step in `refresh()`?</strong></summary>

**Answer:**  
Because this is where Spring instantiates all non-lazy singleton beans: invoking constructors, performing reflection, injecting dependencies, calling `@PostConstruct` hooks, and generating CGLIB dynamic proxies for AOP. In large enterprise systems with thousands of beans, 90% of total startup time is spent in this phase.
</details>

<details>
<summary><strong>Q7: Can an `ApplicationContext` be refreshed multiple times?</strong></summary>

**Answer:**  
Historically, XML-based contexts (`ClassPathXmlApplicationContext`) supported multiple `refresh()` calls. However, modern annotation-based contexts (`AnnotationConfigApplicationContext`, Spring Boot web contexts) do **not** support multiple refreshes and will throw an `IllegalStateException` if `refresh()` is called more than once.
</details>

<details>
<summary><strong>Q8: What is the difference between `ContextRefreshedEvent` and `ApplicationReadyEvent`?</strong></summary>

**Answer:**  
- `ContextRefreshedEvent`: Standard Spring Framework event published at the end of `refresh()` (Step 12) when the IoC container is fully initialized.
- `ApplicationReadyEvent`: Spring Boot-specific event published later by `SpringApplication`, after `CommandLineRunner` and `ApplicationRunner` beans have completed, indicating the application is ready to accept external HTTP traffic.
</details>

---

## 11. Practice Exercises

<details>
<summary><strong>Exercise 1: Inspect Startup Timestamp (Easy)</strong></summary>

**Task:**  
Write a `@Component` that implements `ApplicationContextAware`. In `@PostConstruct`, print `context.getStartupDate()` and format it as an ISO-8601 timestamp.
</details>

<details>
<summary><strong>Exercise 2: Validate Required Environment Properties (Easy)</strong></summary>

**Task:**  
In an `ApplicationContextInitializer`, configure `environment.setRequiredProperties("mandatory.api.key")`. Start the application without defining that property and observe the `MissingRequiredPropertiesException` during Step 1 of `refresh()`.
</details>

<details>
<summary><strong>Exercise 3: Custom ApplicationListener for `ContextRefreshedEvent` (Medium)</strong></summary>

**Task:**  
Create an `ApplicationListener<ContextRefreshedEvent>`. When triggered, print the total count of singleton beans currently present in the context.
</details>

<details>
<summary><strong>Exercise 4: Verify Shutdown Hook Execution (Medium)</strong></summary>

**Task:**  
Create a standalone application using `AnnotationConfigApplicationContext`. Call `context.registerShutdownHook()`. Add a bean with `@PreDestroy`. Terminate the application via `System.exit(0)` and assert that the destruction message prints to the console.
</details>

<details>
<summary><strong>Exercise 5: Profile Startup Steps with `BufferingApplicationStartup` (Hard)</strong></summary>

**Task:**  
Configure `SpringApplication.setApplicationStartup(new BufferingApplicationStartup(2048))`. Query the `/actuator/startup` endpoint (or inspect startup steps programmatically) to identify the top 3 slowest beans during Step 11.
</details>

<details>
<summary><strong>Exercise 6: Programmatic Child Context Refresh (Hard)</strong></summary>

**Task:**  
Manually construct a parent context and a child context. Trigger `parent.refresh()`, then set the parent on the child and call `child.refresh()`. Prove that singletons in the parent are not re-instantiated when the child refreshes.
</details>

---

## 12. Summary / Cheat Sheet

| Step # | Phase Name | Primary Responsibility |
| :---: | :--- | :--- |
| **1** | `prepareRefresh()` | Validates required properties, records start time |
| **2** | `obtainFreshBeanFactory()` | Refreshes internal `DefaultListableBeanFactory` |
| **3** | `prepareBeanFactory()` | Configures SpEL, classloader, property editors |
| **4** | `postProcessBeanFactory()` | Registers web scopes (request, session) |
| **5** | `invokeBeanFactoryPostProcessors()`| Parses `@Configuration`, resolves properties |
| **6** | `registerBeanPostProcessors()` | Registers AOP auto-proxy creators, `@Autowired` BPP |
| **7** | `initMessageSource()` | Initializes i18n internationalization engine |
| **8** | `initApplicationEventMulticaster()`| Sets up event dispatcher |
| **9** | `onRefresh()` | **Starts embedded Tomcat / Web server!** |
| **10** | `registerListeners()` | Attaches event listeners to multicaster |
| **11** | `finishBeanFactoryInitialization()`| **Pre-instantiates all eager singleton beans!** |
| **12** | `finishRefresh()` | Publishes `ContextRefreshedEvent`, starts Lifecycle |

---

## 13. Further Reading
- [Official Spring Framework Reference: Standard ApplicationContext Architecture](https://docs.spring.io/spring-framework/reference/core/beans/context-introduction.html)
- [Spring Framework AbstractApplicationContext Source Code](https://github.com/spring-projects/spring-framework/blob/main/spring-context/src/main/java/org/springframework/context/support/AbstractApplicationContext.java)
- [Spring Boot Startup Tracking Documentation](https://docs.spring.io/spring-boot/docs/current/reference/html/features.html#features.spring-application.startup-tracking)
