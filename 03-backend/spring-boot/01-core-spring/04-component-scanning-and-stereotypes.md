# Component Scanning & Stereotype Annotations

> A deep architectural breakdown of Spring's component scanning subsystem: stereotype semantics (`@Component`, `@Service`, `@Repository`, `@Controller`), ASM bytecode parsing without classloading, custom meta-annotations, filter rules, and `spring-context-indexer` compile-time indexing.

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
In large enterprise systems with thousands of classes, manually writing `@Bean` methods or XML declarations for every single service, repository, and controller is tedious, repetitive, and error-prone. Component scanning automates this process: developers mark classes with stereotype annotations, and Spring discovers, registers, and configures them during bootstrap.

### Historical Evolution
- **Spring 1.x**: Pure XML configuration (`<bean id="customerService" class="..."/>`).
- **Spring 2.5**: Introduced `@Component`, `@Repository`, `@Service`, and `@Controller` along with `<context:component-scan>`.
- **Spring 3.x - 5.x**: Shifted to type-safe `@ComponentScan` on `@Configuration` classes and introduced `spring-context-indexer` for pre-indexing.
- **Spring 6.x & Boot 3.x**: Scanners are integrated with Ahead-of-Time (AOT) code generation, verifying stereotypes and generating static bean definitions before deployment.

### Real-World Relevance
Stereotype annotations provide more than just automated bean registration; they convey architectural intent to team members, static analysis tools, and framework post-processors (e.g., automated SQL exception translation on `@Repository`).

---

## 2. Core Concepts

### 1. The Stereotype Hierarchy
All Spring stereotypes are meta-annotated with `@Component`:
- **`@Component`**: The generic archetype for any Spring-managed class.
- **`@Service`**: Designates a business service layer class. Conveys domain intent; currently has the same behavior as `@Component`.
- **`@Repository`**: Designates a Data Access Object (DAO) or persistence repository. Critically, enables automatic **persistence exception translation** (`DataAccessException`).
- **`@Controller`**: Designates a Spring MVC web controller returning views or templates.
- **`@RestController`**: Composed meta-annotation combining `@Controller` and `@ResponseBody`, returning JSON/XML payloads.

### 2. Meta-Annotations & Composed Stereotypes
Spring supports custom annotations meta-annotated with stereotypes. For instance, you can define `@UseCase` meta-annotated with `@Service`, and Spring will detect and scan `@UseCase` classes automatically.

### 3. Filters in `@ComponentScan`
Fine-grained control over scanned candidates:
- `includeFilters`: Explicitly include classes matching specific annotations or regex patterns.
- `excludeFilters`: Discard classes matching specific profiles, test configurations, or packages.

---

## 3. How It Works Internally

### The Bytecode Scanning Pipeline (ASM Engine)

```
+-------------------------------------------------------------------------+
|                  1. Initiate Scan: @ComponentScan                       |
|  - Reads base package (e.g., `com.example.service`)                     |
|  - Resolves resource patterns: `classpath*:com/example/service/**/*.class
+------------------------------------+------------------------------------+
                                     |
                                     v
+-------------------------------------------------------------------------+
|             2. ASM Bytecode Inspection (SimpleMetadataReader)           |
|  - Reads class file byte streams directly WITHOUT loading classes into  |
|    JVM memory via ClassLoader!                                          |
|  - Extracts class name, interfaces, annotations, and modifiers          |
+------------------------------------+------------------------------------+
                                     |
                                     v
+-------------------------------------------------------------------------+
|             3. TypeFilter Evaluation                                    |
|  - Checks if class is concrete (not interface or abstract)              |
|  - Checks if class is annotated with @Component or meta-annotations     |
|  - Applies includeFilters and excludeFilters                            |
+------------------------------------+------------------------------------+
                                     |
                                     v
+-------------------------------------------------------------------------+
|             4. BeanDefinition Generation                                |
|  - Constructs `ScannedGenericBeanDefinition`                            |
|  - Generates bean name via `BeanNameGenerator` (de-capitalized simple)  |
|  - Parses `@Scope`, `@Primary`, `@Lazy`, `@Qualifier` metadata          |
|  - Registers into `DefaultListableBeanFactory`                          |
+-------------------------------------------------------------------------+
```

---

## 4. Syntax & Usage

### Standard Configuration with Filters
```java
@Configuration
@ComponentScan(
    basePackages = "com.example.app",
    includeFilters = @ComponentScan.Filter(
        type = FilterType.ANNOTATION, 
        classes = UseCase.class
    ),
    excludeFilters = @ComponentScan.Filter(
        type = FilterType.REGEX, 
        pattern = "com\\.example\\.app\\.legacy\\..*"
    )
)
public class ComponentScanConfig {}
```

### Persistence Exception Translation on `@Repository`
```java
@Repository
public class JpaCustomerRepository implements CustomerRepository {

    @PersistenceContext
    private EntityManager entityManager;

    public void save(Customer customer) {
        // Any raw Hibernate / JPA exception thrown here is automatically 
        // caught and translated into Spring's DataAccessException hierarchy!
        entityManager.persist(customer);
    }
}
```

---

## 5. Practical Examples

### Example 1: Layered Stereotypes Across Architecture Boundaries
```java
package com.example.demo.architecture;

import org.springframework.stereotype.Component;
import org.springframework.stereotype.Repository;
import org.springframework.stereotype.Service;
import org.springframework.web.bind.annotation.GetMapping;
import org.springframework.web.bind.annotation.RestController;

import java.util.List;

// 1. Persistence Layer
@Repository
class InMemoryCatalogRepository {
    public List<String> fetchItems() {
        return List.of("Laptop", "Monitor", "Keyboard");
    }
}

// 2. Business Domain Service Layer
@Service
class CatalogService {
    private final InMemoryCatalogRepository repository;

    CatalogService(InMemoryCatalogRepository repository) {
        this.repository = repository;
    }

    public List<String> getCatalog() {
        return repository.fetchItems().stream().map(String::toUpperCase).toList();
    }
}

// 3. Web Presentation Layer
@RestController
class CatalogController {
    private final CatalogService catalogService;

    CatalogController(CatalogService catalogService) {
        this.catalogService = catalogService;
    }

    @GetMapping("/api/catalog")
    public List<String> list() {
        return catalogService.getCatalog();
    }
}
```

### Example 2: Custom Domain Meta-Annotation (`@UseCase`)
```java
package com.example.demo.custom;

import org.springframework.core.annotation.AliasFor;
import org.springframework.stereotype.Component;

import java.lang.annotation.*;

// Custom composed stereotype: domain business use case
@Target(ElementType.TYPE)
@Retention(RetentionPolicy.RUNTIME)
@Documented
@Component // Meta-annotated with @Component so Spring scanner discovers it
public @interface UseCase {
    @AliasFor(annotation = Component.class)
    String value() default "";
}
```
```java
package com.example.demo.custom;

// Developers use domain terminology instead of generic @Service
@UseCase("registerUserUseCase")
public class RegisterUserUseCase {
    public void execute(String username) {
        System.out.println("Executing registration for: " + username);
    }
}
```

### Example 3: Component Scan with Custom Regex Exclude Filter
```java
package com.example.demo.scan;

import org.springframework.context.annotation.ComponentScan;
import org.springframework.context.annotation.Configuration;
import org.springframework.context.annotation.FilterType;

@Configuration
@ComponentScan(
    basePackages = "com.example.demo",
    excludeFilters = {
        // Exclude all mock or prototype classes ending with 'Mock'
        @ComponentScan.Filter(
            type = FilterType.REGEX, 
            pattern = ".*Mock.*"
        ),
        // Exclude specific deprecated integration package
        @ComponentScan.Filter(
            type = FilterType.ASPECTJ, 
            pattern = "com.example.demo.deprecated..*"
        )
    }
)
public class TargetedScanConfig {}
```

### Example 4: Missing Stereotype Anti-Pattern & Remediation
```java
// ANTI-PATTERN: Class created without stereotype annotation
package com.example.demo.billing;

// FLAW: Omitted @Service! Spring component scan will completely ignore this class!
public class PaymentCalculator {
    public double computeTax(double amount) {
        return amount * 0.20;
    }
}

// Result in dependent class:
// @Service
// public class CheckoutService {
//     public CheckoutService(PaymentCalculator calculator) { ... }
// }
// FATAL: NoSuchBeanDefinitionException: No qualifying bean of type 'PaymentCalculator' available!

// REMEDIATION: Add appropriate stereotype or register via @Bean
package com.example.demo.billing;

import org.springframework.stereotype.Service;

@Service // FIXED: Automatically discovered and registered into ApplicationContext
public class PaymentCalculator {
    public double computeTax(double amount) {
        return amount * 0.20;
    }
}
```

### Example 5: Verifying Persistence Exception Translation with `@Repository`
```java
package com.example.demo.repository;

import jakarta.persistence.EntityManager;
import jakarta.persistence.PersistenceContext;
import jakarta.persistence.PersistenceException;
import org.springframework.dao.DataAccessException;
import org.springframework.stereotype.Repository;

@Repository
public class SafeOrderRepository {

    @PersistenceContext
    private EntityManager em;

    public void persistOrder() {
        // If em.persist() throws a raw JPA PersistenceException,
        // Spring's PersistenceExceptionTranslationPostProcessor translates it
        // into a subclass of org.springframework.dao.DataAccessException
        throw new PersistenceException("Database lock acquisition timeout");
    }
}
```

---

## 6. Best Practices

1. **Use Specific Stereotypes Over Generic `@Component`**: Use `@Service` for business logic, `@Repository` for persistence, and `@RestController` for web endpoints. This improves semantic readability and activates post-processors.
2. **Never Scan Root Packages (`com` or `org`)**: Scanning from the root package forces Spring to parse every bytecode on the entire JVM classpath, severely slowing down bootstrap times.
3. **Use `spring-context-indexer` for Instant Scans**: Add the indexer annotation processor to generate a compile-time index (`META-INF/spring.components`), eliminating runtime file system scanning.
4. **Leverage Custom Composed Stereotypes for DDD**: Create `@UseCase`, `@DomainService`, or `@EventHandler` meta-annotated with `@Component` to align code with Domain-Driven Design terminology.
5. **Always Place Main Class in Root Package**: Let `@SpringBootApplication` use its default scan configuration rather than manually hardcoding string package paths in `@ComponentScan`.
6. **Use Type-Safe Base Package Classes**: In `@ComponentScan(basePackageClasses = MyMarker.class)`, pass class references instead of string literals to avoid refactoring bugs.

---

## 7. Common Mistakes & Pitfalls

| Mistake | Why It Fails | Practical Remedy |
| :--- | :--- | :--- |
| **Typo in `basePackages` String** | Misspelling package string (e.g., `com.exampel`) causes scanning to find 0 beans silently without errors. | Use type-safe `basePackageClasses = Marker.class`. |
| **Expecting `@Service` to translate SQL exceptions** | `@Service` is purely descriptive; only `@Repository` triggers automatic `DataAccessException` translation. | Mark persistence components explicitly with `@Repository`. |
| **Scanning Outside the Root Application Package** | Placing classes in a package higher than or sibling to `@SpringBootApplication` causes them to be ignored. | Move the main class to the common root ancestor package. |
| **Stereotype on Interface** | Annotating an `interface` with `@Component` does not create a bean; Spring only instantiates concrete classes. | Place stereotype annotations on implementing classes (except Spring Data repos). |
| **Duplicate Bean Names in Separate Packages** | Two classes with identical simple names (`com.a.Service` and `com.b.Service`) collide in the registry. | Specify custom names: `@Service("billingService")` or configure a fully-qualified name generator. |
| **Inner Class Not Static** | Placing `@Component` on a non-static inner class fails because Spring cannot instantiate it without an outer instance. | Declare inner components `public static class`. |

---

## 8. Performance & Security Notes

### Compile-Time Indexing (`spring-context-indexer`)
In projects with thousands of classes, runtime classpath scanning with ASM incurs measurable I/O overhead. Adding the context indexer generates `META-INF/spring.components` during compilation:
```xml
<dependency>
    <groupId>org.springframework</groupId>
    <artifactId>spring-context-indexer</artifactId>
    <optional>true</optional>
</dependency>
```
When present, Spring skips directory traversal and reads the indexed candidate list directly, cutting startup latency significantly.

### Security Isolation
Ensure test configuration classes (e.g., mock generators) are excluded from production scans using `@Profile("test")` or explicit exclude filters to prevent test backdoors in production.

---

## 9. Comparison / When to Use What

| Annotation | Semantic Role | Framework Behavior | Target Class |
| :--- | :--- | :--- | :--- |
| **`@Component`** | Generic Spring-managed bean | Standard bean registration | Utilities, helpers, adapters |
| **`@Service`** | Business logic layer | Standard bean registration | Application services, use cases |
| **`@Repository`** | Persistence / DAO layer | **Persistence exception translation** | JPA/JDBC DAO implementations |
| **`@Controller`** | Web presentation layer | Handles HTTP requests, view resolution | MVC controllers (HTML/Thymeleaf) |
| **`@RestController`** | REST API presentation | `@Controller` + `@ResponseBody` | REST API endpoints (JSON/XML) |
| **`@Configuration`** | Factory source of bean definitions | CGLIB method proxying for `@Bean` | Configuration classes |

---

## 10. Interview Questions

<details>
<summary><strong>Q1: What is the difference between `@Component`, `@Service`, and `@Repository`?</strong></summary>

**Answer:**  
- `@Component` is the generic meta-annotation for any Spring-managed class.
- `@Service` is a semantic specialization of `@Component` indicating business domain logic. Currently, it behaves identically to `@Component` at the framework level.
- `@Repository` is a specialization for the persistence layer. Critically, Spring's `PersistenceExceptionTranslationPostProcessor` recognizes `@Repository` and automatically intercepts database exceptions (JPA, Hibernate, JDBC) and translates them into Spring's unified `DataAccessException` hierarchy.
</details>

<details>
<summary><strong>Q2: How does Spring scan classes without loading them into the JVM ClassLoader?</strong></summary>

**Answer:**  
Spring uses the **ASM library** via `SimpleMetadataReader`. It reads the raw `.class` file byte streams directly from the filesystem or JAR files and inspects the constant pool and bytecode headers to detect class names, annotations, and interfaces. This avoids premature class loading and prevents class initialization side-effects.
</details>

<details>
<summary><strong>Q3: How does Spring resolve bean names during component scanning by default?</strong></summary>

**Answer:**  
By default, Spring uses `AnnotationBeanNameGenerator`. It takes the simple class name and converts the first character to lowercase (e.g., `OrderService` -> `orderService`). However, if the class name starts with more than one uppercase letter (e.g., `URLValidator`), the casing is preserved as-is (`URLValidator`).
</details>

<details>
<summary><strong>Q4: What is `spring-context-indexer` and how does it optimize startup?</strong></summary>

**Answer:**  
`spring-context-indexer` is an annotation processor that runs at compile time. It inspects all `@Component` and stereotype annotations and writes their fully qualified names into `META-INF/spring.components`. At startup, Spring checks for this file; if found, it bypasses slow classpath directory traversal and loads candidates directly from the index.
</details>

<details>
<summary><strong>Q5: Can you create a custom stereotype annotation in Spring? How?</strong></summary>

**Answer:**  
Yes. Define a standard Java annotation and meta-annotate it with `@Component` (or another stereotype):
```java
@Target(ElementType.TYPE)
@Retention(RetentionPolicy.RUNTIME)
@Component
public @interface DomainService {
    String value() default "";
}
```
Any class annotated with `@DomainService` will be recognized and registered by Spring's component scanner.
</details>

<details>
<summary><strong>Q6: Why is `basePackageClasses` preferred over `basePackages` strings in `@ComponentScan`?</strong></summary>

**Answer:**  
`basePackageClasses` uses type references (e.g., `basePackageClasses = OrderMarker.class`), which are validated by the Java compiler. If packages are renamed or refactored, the IDE updates references automatically. String paths (`basePackages = "com.example.order"`) cannot be verified at compile time; typos fail silently at runtime.
</details>

<details>
<summary><strong>Q7: What happens if two classes in different packages have the same class name and are both scanned?</strong></summary>

**Answer:**  
By default, Spring generates bean names using simple class names. If `com.app.orders.Service` and `com.app.billing.Service` are both scanned, they both receive the bean name `service`. In Spring Boot, this throws `ConflictingBeanDefinitionException` on startup unless bean definition overriding is explicitly enabled.
</details>

<details>
<summary><strong>Q8: What is the difference between `@Controller` and `@RestController`?</strong></summary>

**Answer:**  
`@RestController` is a composed annotation that combines `@Controller` and `@ResponseBody`. On a standard `@Controller`, method return values represent view names resolved by a `ViewResolver` (like Thymeleaf). On a `@RestController`, method return values are automatically serialized into the HTTP response body as JSON or XML via `HttpMessageConverters`.
</details>

---

## 11. Practice Exercises

<details>
<summary><strong>Exercise 1: Create a Custom Composed Stereotype (Easy)</strong></summary>

**Task:**  
Create a `@Worker` stereotype meta-annotated with `@Component`. Apply it to a background worker class. Verify via `context.getBean(WorkerClass.class)` that Spring discovers and registers it.
</details>

<details>
<summary><strong>Exercise 2: Type-Safe Base Package Scanning (Easy)</strong></summary>

**Task:**  
Create an empty marker interface `DomainMarker` in a package. Configure `@ComponentScan(basePackageClasses = DomainMarker.class)` and confirm that all components in that package and its sub-packages are registered.
</details>

<details>
<summary><strong>Exercise 3: Configure Exclude Filters (Medium)</strong></summary>

**Task:**  
Configure a `@ComponentScan` with an `excludeFilter` of type `ASSIGNABLE_TYPE` that specifically ignores a `DevMockRepository` class during standard application startup.
</details>

<details>
<summary><strong>Exercise 4: Benchmark Startup with `spring-context-indexer` (Medium)</strong></summary>

**Task:**  
In a project with 30+ components, measure the startup time before and after adding `spring-context-indexer`. Inspect the generated `target/classes/META-INF/spring.components` file.
</details>

<details>
<summary><strong>Exercise 5: Custom BeanNameGenerator (Hard)</strong></summary>

**Task:**  
Implement a custom `BeanNameGenerator` that prefixes all bean names with the module name extracted from the package hierarchy (e.g., `com.example.billing.PaymentService` -> `billing_paymentService`). Register it with `@ComponentScan(nameGenerator = CustomNameGenerator.class)`.
</details>

<details>
<summary><strong>Exercise 6: Verify Persistence Exception Translation (Hard)</strong></summary>

**Task:**  
Create a custom DAO annotated with `@Repository` and an identical one annotated with `@Component`. Have both throw a `jakarta.persistence.EntityNotFoundException`. Demonstrate that the `@Repository` bean automatically translates it to Spring's `DataAccessException` when invoked through a proxy.
</details>

---

## 12. Summary / Cheat Sheet

| Annotation / Concept | Primary Role | Key Feature |
| :--- | :--- | :--- |
| **`@Component`** | Root archetype | Discoverable by `@ComponentScan` |
| **`@Service`** | Business layer | Semantic domain classification |
| **`@Repository`** | Persistence layer | Automatic persistence exception translation |
| **`@Controller`** | MVC Web presentation | View resolution / template rendering |
| **`@RestController`** | REST API presentation | Inlines `@ResponseBody` for JSON serialization |
| **`basePackageClasses`** | Type-safe scanning root | Safe against package renaming refactoring |
| **`spring.components`** | Compile-time index | Bypasses runtime filesystem scanning |

---

## 13. Further Reading
- [Official Spring Framework Reference: Classpath Scanning and Managed Components](https://docs.spring.io/spring-framework/reference/core/beans/classpath-scanning.html)
- [Spring Context Indexer Guide](https://docs.spring.io/spring-framework/reference/core/beans/classpath-scanning.html#beans-scanning-index)
- [ASM Bytecode Manipulation Framework](https://asm.ow2.io/)
