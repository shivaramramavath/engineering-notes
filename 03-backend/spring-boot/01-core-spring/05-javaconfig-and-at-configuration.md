# JavaConfig, @Configuration & @Bean Internals

> The ultimate guide to Spring's programmatic JavaConfig engine: the mechanics of `@Configuration`, `@Bean`, Full vs Lite mode (`proxyBeanMethods`), CGLIB subclassing, inter-bean method calls, and GraalVM AOT compilation compatibility.

| Metadata | Value |
| :--- | :--- |
| **Level** | Intermediate |
| **Prerequisites** | [IoC Container & ApplicationContext Internals](file:///03-backend/spring-boot/01-core-spring/01-ioc-container-and-application-context.md), [Spring Beans & BeanDefinitions](file:///03-backend/spring-boot/01-core-spring/02-beans-and-bean-definitions.md) |
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
While annotation stereotypes (`@Component`) are ideal for classes whose source code you control, third-party libraries (e.g., Jackson `ObjectMapper`, Apache Kafka producers, AWS SDK clients) cannot be annotated with Spring stereotypes. Furthermore, complex bean construction logic requires programmatic Java code: conditional instantiations, builder executions, and explicit property assignments.

### Historical Evolution
- **Spring 1.x - 2.x**: Third-party beans were declared in XML via `<bean class="com.fasterxml.jackson.databind.ObjectMapper"/>`.
- **Spring 3.0**: Introduced JavaConfig (`@Configuration` and `@Bean`), making bean definitions type-safe, refactor-friendly, and fully debuggable.
- **Spring 5.2 / Boot 2.2**: Introduced `proxyBeanMethods = false` (Lite Mode) to eliminate CGLIB proxying overhead.
- **Spring 6.x / Boot 3.x**: Lite Mode became the de facto standard in auto-configurations and native compilation, paving the way for instant GraalVM AOT execution.

### Real-World Relevance
Understanding `@Configuration` lifecycle, inter-bean calls, and the difference between Full mode and Lite mode is critical for preventing subtle duplicate bean instantiation bugs and writing high-performance cloud-native microservices.

---

## 2. Core Concepts

### 1. `@Configuration` vs. `@Bean`
- **`@Configuration`**: Marks a class as an authoritative source of bean definitions. It is meta-annotated with `@Component`, so it is automatically discovered by `@ComponentScan`.
- **`@Bean`**: Placed on a method within a configuration class. The method return value is registered as a bean in the `ApplicationContext`. The method name becomes the default bean name.

### 2. Full Mode vs. Lite Mode (`proxyBeanMethods`)
- **Full Mode (`proxyBeanMethods = true`, Default)**:
  * Spring wraps the `@Configuration` class in a **CGLIB dynamic proxy**.
  * Direct inter-bean method calls (e.g., calling `dataSource()` from inside `transactionManager()`) are intercepted by the proxy. If the bean has already been created, the cached singleton is returned instead of re-executing the method.
- **Lite Mode (`proxyBeanMethods = false`)**:
  * Spring does **not** proxy the configuration class; it instantiates the class as a plain Java object.
  * Direct calls to other `@Bean` methods execute plain Java logic, returning a **new, unmanaged instance** each time!
  * Dependencies must be injected via method parameters.
  * *Advantage*: Faster startup, reduced memory, 100% GraalVM AOT native-image friendly.

### 3. Modular Configuration Composition
- **`@Import`**: Imports additional `@Configuration` classes or `ImportSelector` / `ImportBeanDefinitionRegistrar` components.
- **`@DependsOn`**: Explicitly forces one bean to initialize before another, useful for background daemons or cache warming.

---

## 3. How It Works Internally

### Full Mode CGLIB Interception vs. Lite Mode Direct Execution

```
A. FULL MODE (@Configuration(proxyBeanMethods = true))
+-------------------------------------------------------------------------+
|                  ConfigurationClassPostProcessor                        |
|  - Generates CGLIB subclass: `AppConfig$$SpringCGLIB$$0`                |
+------------------------------------+------------------------------------+
                                     |
                                     v
+-------------------------------------------------------------------------+
|                  Method Invocation: appConfig.database()                |
|  - CGLIB Interceptor intercepts method call                             |
|  - Checks `DefaultListableBeanFactory.containsSingleton("database")`   |
|                                                                         |
|  IF FOUND:                                                              |
|    -> Returns existing cached singleton instance from context!          |
|  IF NOT FOUND:                                                          |
|    -> Delegates to super.database(), stores result in singleton cache,  |
|       and returns it.                                                   |
+-------------------------------------------------------------------------+

B. LITE MODE (@Configuration(proxyBeanMethods = false))
+-------------------------------------------------------------------------+
|                  Plain Java Class Instantiation                         |
|  - No CGLIB proxy generated! `new AppConfig()`                          |
+------------------------------------+------------------------------------+
                                     |
                                     v
+-------------------------------------------------------------------------+
|                  Method Invocation: appConfig.database()                |
|  - Directly executes bytecodes inside database() method                 |
|  - WARNING: Every direct method invocation calls `new Database()`       |
|    creating unmanaged, duplicate instances!                             |
|  - REMEDY: Always inject collaborators as METHOD PARAMETERS!            |
+-------------------------------------------------------------------------+
```

---

## 4. Syntax & Usage

### Full Mode Configuration (Default)
```java
@Configuration // proxyBeanMethods = true by default
public class FullConfig {

    @Bean
    public DataSource dataSource() {
        return new HikariDataSource();
    }

    @Bean
    public TransactionManager transactionManager() {
        // CGLIB intercepts dataSource() call and returns cached singleton!
        return new DataSourceTransactionManager(dataSource());
    }
}
```

### Modern Lite Mode Configuration (`proxyBeanMethods = false`)
```java
@Configuration(proxyBeanMethods = false) // High-performance / GraalVM friendly
public class LiteConfig {

    @Bean
    public DataSource dataSource() {
        return new HikariDataSource();
    }

    @Bean
    public TransactionManager transactionManager(DataSource dataSource) {
        // IDIOMATIC: Injected via method parameter, zero CGLIB overhead
        return new DataSourceTransactionManager(dataSource);
    }
}
```

---

## 5. Practical Examples

### Example 1: Modern Enterprise Configuration with Third-Party Beans
```java
package com.example.demo.config;

import com.fasterxml.jackson.databind.DeserializationFeature;
import com.fasterxml.jackson.databind.ObjectMapper;
import com.fasterxml.jackson.databind.SerializationFeature;
import com.fasterxml.jackson.datatype.jsr310.JavaTimeModule;
import org.springframework.context.annotation.Bean;
import org.springframework.context.annotation.Configuration;
import org.springframework.context.annotation.Primary;

import java.net.http.HttpClient;
import java.time.Duration;

@Configuration(proxyBeanMethods = false)
public class InfrastructureConfiguration {

    @Bean
    @Primary
    public ObjectMapper customObjectMapper() {
        return new ObjectMapper()
            .registerModule(new JavaTimeModule())
            .disable(SerializationFeature.WRITE_DATES_AS_TIMESTAMPS)
            .configure(DeserializationFeature.FAIL_ON_UNKNOWN_PROPERTIES, false);
    }

    @Bean
    public HttpClient sharedHttpClient() {
        return HttpClient.newBuilder()
            .version(HttpClient.Version.HTTP_2)
            .connectTimeout(Duration.ofSeconds(5))
            .build();
    }
}
```

### Example 2: Lifecycle Management (`initMethod` and `destroyMethod`)
```java
package com.example.demo.lifecycle;

import org.springframework.context.annotation.Bean;
import org.springframework.context.annotation.Configuration;

class EmbeddedCacheEngine {
    public void start() {
        System.out.println("Cache Engine started: Allocated memory buffers.");
    }

    public void stop() {
        System.out.println("Cache Engine stopped: Flushed dirty pages to disk.");
    }
}

@Configuration(proxyBeanMethods = false)
public class CacheConfiguration {

    // Custom lifecycle methods declared on @Bean definition
    @Bean(initMethod = "start", destroyMethod = "stop")
    public EmbeddedCacheEngine embeddedCacheEngine() {
        return new EmbeddedCacheEngine();
    }
}
```

### Example 3: Modular Composition via `@Import`
```java
package com.example.demo.modular;

import org.springframework.context.annotation.Bean;
import org.springframework.context.annotation.Configuration;
import org.springframework.context.annotation.Import;

@Configuration(proxyBeanMethods = false)
class SecurityModuleConfig {
    @Bean
    public String tokenValidator() { return "JWT-RS256-Validator"; }
}

@Configuration(proxyBeanMethods = false)
class DatabaseModuleConfig {
    @Bean
    public String connectionPool() { return "HikariCP-Worker-Pool"; }
}

// Master configuration importing specialized modular sub-configurations
@Configuration(proxyBeanMethods = false)
@Import({SecurityModuleConfig.class, DatabaseModuleConfig.class})
public class AppMasterConfiguration {

    @Bean
    public String applicationCoordinator(String tokenValidator, String connectionPool) {
        return "Coordinator initialized with [" + tokenValidator + "] and [" + connectionPool + "]";
    }
}
```

### Example 4: Lite Mode Anti-Pattern & Method Parameter Remediation
```java
// ANTI-PATTERN: Calling @Bean method directly in Lite mode
@Configuration(proxyBeanMethods = false)
public class BadLiteConfig {

    @Bean
    public ConnectionPool pool() {
        return new ConnectionPool(); // Instance #1 created for pool bean
    }

    @Bean
    public ServiceA serviceA() {
        // BUG: Calling pool() directly creates Instance #2!
        return new ServiceA(pool());
    }

    @Bean
    public ServiceB serviceB() {
        // BUG: Calling pool() directly creates Instance #3!
        // Three completely separate connection pools now exist in memory!
        return new ServiceB(pool());
    }
}

// REMEDIATION: Inject collaborator as method parameter
@Configuration(proxyBeanMethods = false)
public class CleanLiteConfig {

    @Bean
    public ConnectionPool pool() {
        return new ConnectionPool(); // Single managed singleton instance
    }

    @Bean
    public ServiceA serviceA(ConnectionPool pool) {
        return new ServiceA(pool); // Reuses singleton pool instance
    }

    @Bean
    public ServiceB serviceB(ConnectionPool pool) {
        return new ServiceB(pool); // Reuses same singleton pool instance
    }
}
```

### Example 5: Explicit Dependency Sequencing via `@DependsOn`
```java
package com.example.demo.sequencing;

import org.springframework.context.annotation.Bean;
import org.springframework.context.annotation.Configuration;
import org.springframework.context.annotation.DependsOn;

class SchemaMigrator {
    public SchemaMigrator() {
        System.out.println("1. SchemaMigrator executed: Database DDL initialized.");
    }
}

class QueryEngine {
    public QueryEngine() {
        System.out.println("2. QueryEngine initialized: Ready for SQL queries.");
    }
}

@Configuration(proxyBeanMethods = false)
public class SequencingConfig {

    @Bean("schemaMigrator")
    public SchemaMigrator schemaMigrator() {
        return new SchemaMigrator();
    }

    // Forces schemaMigrator to initialize prior to queryEngine
    @Bean
    @DependsOn("schemaMigrator")
    public QueryEngine queryEngine() {
        return new QueryEngine();
    }
}
```

---

## 6. Best Practices

1. **Default to `proxyBeanMethods = false` in Modern Boot**: Spring Boot internal auto-configurations use `proxyBeanMethods = false`. It eliminates CGLIB proxy overhead, reduces startup latency, and aligns with GraalVM native image requirements.
2. **Always Inject Dependencies via `@Bean` Method Parameters**: Rather than calling sibling `@Bean` methods, declare collaborators as method parameters. This works identically in both Full and Lite modes without risk of duplicate instantiation.
3. **Never Make `@Configuration` Classes or `@Bean` Methods `final` in Full Mode**: CGLIB works by subclassing the configuration class and overriding methods. If the class or method is `final`, CGLIB fails at runtime.
4. **Use `@Primary` for Default Third-Party Beans**: When defining custom beans of common types (`ObjectMapper`, `RestClient`), mark the primary instance with `@Primary` to avoid autowiring ambiguity.
5. **Use Meaningful Bean Method Names**: The method name becomes the bean's identifier in the container. Use expressive nouns (`orderDatabasePool`, `cloudAuditClient`) rather than generic names (`client`, `pool`).
6. **Set `destroyMethod = ""` to Prevent Inferred Destruction**: If a third-party class has a `close()` or `shutdown()` method that should *not* be automatically closed on Spring shutdown, override with `@Bean(destroyMethod = "")`.

---

## 7. Common Mistakes & Pitfalls

| Mistake | Why It Fails | Practical Remedy |
| :--- | :--- | :--- |
| **Direct `@Bean` calls in Lite mode** | With `proxyBeanMethods = false`, calling a `@Bean` method executes normal Java code, instantiating duplicate unmanaged instances. | Pass dependencies as method arguments to the `@Bean` method. |
| **Declaring `@Configuration` as `final`** | In Full mode, CGLIB cannot generate a subclass of a `final` class, throwing `BeanDefinitionParsingException`. | Remove `final` keyword from `@Configuration` class and its methods. |
| **Inferred `close()` destroying shared resources** | Spring automatically infers `close()` or `shutdown()` methods as destroy callbacks, prematurely shutting down shared pools. | Set `@Bean(destroyMethod = "")` to disable inference. |
| **Overloading `@Bean` method names** | Overloaded `@Bean` methods with the same name but different parameters cause unpredictable resolution bugs. | Use distinct, unambiguous method names for different bean definitions. |
| **Calling `@Bean` methods from constructors** | Calling sibling `@Bean` methods in the `@Configuration` constructor executes before the CGLIB proxy is established. | Wire dependencies into bean methods, not the configuration constructor. |
| **Putting `@Bean` inside `@Component` (Lite Mode Confusion)** | Placing `@Bean` in a `@Component` class implicitly runs in Lite mode; direct method calls do not return singletons. | Convert to `@Configuration` or use method parameter injection. |

---

## 8. Performance & Security Notes

### CGLIB Proxy Cost vs. GraalVM AOT
Full Mode requires CGLIB to generate bytecode subclasses at startup:
- Class generation incurs memory allocation and classloader registration latency.
- In GraalVM Native Image builds, dynamic class generation is strictly disallowed; Ahead-of-Time compilation must pre-compute reflection and proxy metadata.
- Setting `proxyBeanMethods = false` completely eliminates CGLIB, speeding up startup by ~15% and providing seamless native image generation.

### Security Boundaries
Configuration classes define security filters, authentication providers, and database datasources. Ensure `@Configuration` classes are located inside your application's protected package structure and never exposed to external classloader injection.

---

## 9. Comparison / When to Use What

| Feature | Full Mode (`proxyBeanMethods = true`) | Lite Mode (`proxyBeanMethods = false`) |
| :--- | :--- | :--- |
| **CGLIB Proxying** | Yes (subclasses `@Configuration`) | No (plain Java instance) |
| **Inter-Bean Calls** | Safe; returns cached singleton bean | **Unsafe; creates new duplicate object** |
| **Dependency Injection Style** | Direct method calls OR parameters | **Strictly Method Parameters** |
| **Startup Performance** | Standard | Faster (bypasses CGLIB generation) |
| **GraalVM Native Image** | Requires reflection hints | Native out of the box |
| **Recommended Usage** | Legacy Spring codebases | **Modern Spring Boot 3.4+ / Microservices** |

---

## 10. Interview Questions

<details>
<summary><strong>Q1: What does `proxyBeanMethods = false` do on `@Configuration`?</strong></summary>

**Answer:**  
It switches the configuration class from **Full Mode** to **Lite Mode**. Spring will not generate a CGLIB subclass to intercept `@Bean` method invocations. As a result, direct calls to other `@Bean` methods will execute as plain Java calls (creating new instances), but startup is faster and the class requires zero dynamic bytecode generation—making it ideal for GraalVM native compilation.
</details>

<details>
<summary><strong>Q2: How does Spring enforce singleton semantics for inter-bean calls in Full Mode?</strong></summary>

**Answer:**  
In Full Mode, `ConfigurationClassPostProcessor` uses CGLIB to dynamically subclass the `@Configuration` class at runtime. It intercepts calls to `@Bean` methods. When method `A()` calls method `B()`, the CGLIB method interceptor checks the `ApplicationContext` for an existing singleton named `B`. If it exists, the cached instance is returned immediately without executing the method body again.
</details>

<details>
<summary><strong>Q3: Why shouldn't you make a `@Configuration` class or `@Bean` method `final`?</strong></summary>

**Answer:**  
In default Full Mode, Spring uses CGLIB to subclass the configuration class and override its `@Bean` methods to inject singleton interception logic. Java prohibits subclassing `final` classes and overriding `final` methods, so Spring will throw a `BeanDefinitionParsingException` on startup.
</details>

<details>
<summary><strong>Q4: What is the difference between `@Import` and `@ComponentScan`?</strong></summary>

**Answer:**  
- `@ComponentScan` automatically scans specified package paths on the filesystem, parsing bytecode for stereotype annotations (`@Component`, `@Service`).
- `@Import` provides explicit, programmatic composition. It directly imports specific configuration classes, `ImportSelector`, or `ImportBeanDefinitionRegistrar` implementations without performing classpath scanning.
</details>

<details>
<summary><strong>Q5: What is inferred bean destruction in `@Bean`?</strong></summary>

**Answer:**  
By default, if a bean class contains a public method named `close()` or `shutdown()` (such as `AutoCloseable`), Spring automatically infers this method as the `destroyMethod` callback upon context closure. If this behavior is unwanted (e.g., for a shared pool), you can explicitly disable it with `@Bean(destroyMethod = "")`.
</details>

<details>
<summary><strong>Q6: What happens if a `@Bean` method is declared inside a standard `@Component` class instead of `@Configuration`?</strong></summary>

**Answer:**  
Spring processes the `@Bean` method in **Lite Mode**. The enclosing `@Component` class is not enhanced by CGLIB. Direct calls from one `@Bean` method to another inside that class will NOT return singletons; they will invoke standard Java methods and create duplicate, unmanaged objects.
</details>

<details>
<summary><strong>Q7: When should you use `@DependsOn`?</strong></summary>

**Answer:**  
`@DependsOn` forces the container to initialize specified beans *before* the annotated bean is instantiated, even when there is no direct dependency injection between them. Typical use cases include initializing database schemas, starting embedded servers, or setting system properties before other beans bootstrap.
</details>

<details>
<summary><strong>Q8: Can a `@Bean` method accept arguments, and how does Spring resolve them?</strong></summary>

**Answer:**  
Yes. Any parameter declared on a `@Bean` method is automatically resolved from the `ApplicationContext` by type and qualifier, exactly like constructor injection. This is the idiomatic way to wire dependencies between beans in Lite Mode.
</details>

---

## 11. Practice Exercises

<details>
<summary><strong>Exercise 1: Create a Third-Party Client Bean (Easy)</strong></summary>

**Task:**  
Write a `@Configuration(proxyBeanMethods = false)` class that declares a custom `java.net.http.HttpClient` configured with an HTTP/2 protocol and a 10-second connection timeout as a `@Bean`.
</details>

<details>
<summary><strong>Exercise 2: Demonstrate Inferred Destruction (Easy)</strong></summary>

**Task:**  
Create a class with a `public void close()` method. Register it with `@Bean`. Close the application context and observe Spring automatically calling `close()` without specifying `destroyMethod`.
</details>

<details>
<summary><strong>Exercise 3: Reproduce Duplicate Instantiations in Lite Mode (Medium)</strong></summary>

**Task:**  
Create a `@Configuration(proxyBeanMethods = false)` class. Define a `@Bean Database db()`. In two other `@Bean` methods, call `db()` directly. Boot the context and verify that two distinct `Database` instances were created (different `System.identityHashCode`).
</details>

<details>
<summary><strong>Exercise 4: Fix Lite Mode via Parameter Injection (Medium)</strong></summary>

**Task:**  
Refactor the configuration from Exercise 3 so that both service `@Bean` methods accept `Database db` as a method argument. Verify that both services now share the exact same singleton instance.
</details>

<details>
<summary><strong>Exercise 5: Dynamic ImportSelector Implementation (Hard)</strong></summary>

**Task:**  
Implement `ImportSelector` that inspects an environment property `app.feature.tier` (e.g., "basic" vs "enterprise") and dynamically imports either `BasicTierConfig.class` or `EnterpriseTierConfig.class`.
</details>

<details>
<summary><strong>Exercise 6: Enforce `proxyBeanMethods = false` via ArchUnit (Hard)</strong></summary>

**Task:**  
Write an ArchUnit test asserting that all classes annotated with `@Configuration` across the codebase must have `proxyBeanMethods = false` explicitly configured.
</details>

---

## 12. Summary / Cheat Sheet

| Annotation / Attribute | Syntax | Meaning / Behavior |
| :--- | :--- | :--- |
| **`@Configuration`** | `@Configuration(proxyBeanMethods = false)` | Defines bean factory methods; Lite mode avoids CGLIB |
| **`@Bean`** | `@Bean(initMethod="init", destroyMethod="")` | Registers method return value into context |
| **Method Parameter DI** | `@Bean public B b(A a) { return new B(a); }` | Idiomatic way to inject dependencies in `@Bean` methods |
| **`@Import`** | `@Import({ModuleA.class, ModuleB.class})` | Composes modular configuration classes |
| **`@DependsOn`** | `@DependsOn("schemaMigrator")` | Enforces bean initialization order without direct DI |
| **Disable Inferred Destroy** | `@Bean(destroyMethod = "")` | Prevents automatic calling of `close()` / `shutdown()` |

---

## 13. Further Reading
- [Official Spring Framework Reference: Java-based Container Configuration](https://docs.spring.io/spring-framework/reference/core/beans/java.html)
- [Spring proxyBeanMethods Javadoc](https://docs.spring.io/spring-framework/docs/current/javadoc-api/org/springframework/context/annotation/Configuration.html#proxyBeanMethods())
- [GraalVM Ahead-of-Time Compilation with Spring Boot](https://docs.spring.io/spring-boot/docs/current/reference/html/native-image.html)
