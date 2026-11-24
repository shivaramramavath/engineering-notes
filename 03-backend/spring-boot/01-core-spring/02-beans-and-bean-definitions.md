# Spring Beans & BeanDefinitions: Deep Dive

> The comprehensive internal guide to how Spring represents, inspects, transforms, and instantiates components: the `BeanDefinition` metadata hierarchy, `BeanDefinitionRegistry`, `FactoryBean<T>` contracts, and runtime definition manipulation.

| Metadata | Value |
| :--- | :--- |
| **Level** | Intermediate |
| **Prerequisites** | [IoC Container & ApplicationContext Internals](file:///03-backend/spring-boot/01-core-spring/01-ioc-container-and-application-context.md) |
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
Before Spring can instantiate an object, wire its dependencies, apply AOP interceptors, or invoke lifecycle methods, it requires a formal, uniform blueprint describing the class, its constructor arguments, properties, lifecycle hooks, and autowiring rules. Without this separation of metadata from live objects, dynamic container features like conditional instantiation, ahead-of-time analysis, and custom starters would be impossible.

### Historical Evolution
- **Spring 1.x**: Bean definitions were parsed strictly from XML `<bean id="..." class="...">` tags into `RootBeanDefinition` structures.
- **Spring 2.x - 3.x**: Introduced `GenericBeanDefinition` and annotation-driven parsing (`@Component`, `@Bean`).
- **Spring 6.x / Boot 3.x**: Bean definitions are now compiled at build time during Ahead-of-Time (AOT) processing into generated Java bytecodes, dramatically accelerating cloud startup.

### Real-World Relevance
Understanding `BeanDefinition` is essential for developing custom Spring Boot starters, implementing dynamic plugins, writing `BeanFactoryPostProcessor` extensions, and debugging complex bean creation errors.

---

## 2. Core Concepts

### 1. The `BeanDefinition` Interface
`BeanDefinition` (`org.springframework.beans.factory.config.BeanDefinition`) models the metadata of a bean. Key attributes include:
- **`beanClassName`**: The fully qualified name of the bean's implementation class.
- **`scope`**: `singleton`, `prototype`, `request`, or `session`.
- **`lazyInit`**: Whether the bean should be created eagerly on context startup or lazily upon first request.
- **`dependsOn`**: Names of beans that must be initialized before this bean.
- **`primary` / `fallback`**: Disambiguation flags when multiple beans match a target type.
- **`constructorArgumentValues` & `propertyValues`**: Dynamic values and references passed to constructors and setters.
- **`initMethodName` & `destroyMethodName`**: Custom lifecycle method identifiers.

### 2. Concrete BeanDefinition Implementations
- **`RootBeanDefinition`**: Represents a resolved, merged bean definition at runtime. Cannot be a child of another definition.
- **`ChildBeanDefinition`**: Inherits configuration from a parent definition.
- **`GenericBeanDefinition`**: Modern, versatile implementation used for standard programmatic and parsed configurations.
- **`AnnotatedGenericBeanDefinition`**: Represents beans declared with `@Configuration` or `@Component`, preserving annotation metadata.

### 3. The `FactoryBean<T>` Contract
A `FactoryBean<T>` is a special programmatic factory bean. When Spring encounters a bean implementing `FactoryBean<T>`, querying the container for the bean name returns the product of `getObject()`, not the factory itself:
- `context.getBean("myBean")` -> Returns the object created by `FactoryBean.getObject()`
- `context.getBean("&myBean")` -> Returns the `FactoryBean` instance itself (prefix with `&`)

---

## 3. How It Works Internally

### BeanDefinition Lifecycle: Parsing to Instantiation

```
+-------------------------------------------------------------------------+
|                    Configuration Discovery Sources                      |
|       (@Component, @Bean, JavaConfig, Functional, XML)                  |
+------------------------------------+------------------------------------+
                                     |
                                     v
+-------------------------------------------------------------------------+
|                  1. BeanDefinition Reader & Scanner                     |
|  - `ClassPathBeanDefinitionScanner` scans annotated bytecodes           |
|  - `ConfigurationClassBeanDefinitionReader` parses @Bean methods        |
+------------------------------------+------------------------------------+
                                     |
                                     v
+-------------------------------------------------------------------------+
|                  2. BeanDefinitionRegistry Storage                      |
|  - BeanDefinitions stored in `DefaultListableBeanFactory`               |
|    `Map<String, BeanDefinition> beanDefinitionMap`                      |
|    `List<String> beanDefinitionNames`                                   |
+------------------------------------+------------------------------------+
                                     |
                                     v
+-------------------------------------------------------------------------+
|                  3. BeanFactoryPostProcessor Interception               |
|  - Interceptors read and mutate BeanDefinitions before beans exist      |
|  - E.g., `PropertySourcesPlaceholderConfigurer` resolves `${...}`       |
+------------------------------------+------------------------------------+
                                     |
                                     v
+-------------------------------------------------------------------------+
|                  4. Merged BeanDefinition Resolution                    |
|  - Parent-child definitions are consolidated into `RootBeanDefinition`  |
+------------------------------------+------------------------------------+
                                     |
                                     v
+-------------------------------------------------------------------------+
|                  5. Instantiation & Bean Wrapper                        |
|  - Container calls constructor / factory method                         |
|  - Injects properties and creates live bean instance                    |
+-------------------------------------------------------------------------+
```

---

## 4. Syntax & Usage

### Programmatic Definition via `BeanDefinitionBuilder`
```java
BeanDefinition definition = BeanDefinitionBuilder
    .genericBeanDefinition(CustomerRepository.class)
    .setScope(BeanDefinition.SCOPE_SINGLETON)
    .setLazyInit(false)
    .addPropertyValue("connectionTimeout", 5000)
    .addConstructorArgReference("databaseConnectionPool")
    .getBeanDefinition();

registry.registerBeanDefinition("customerRepository", definition);
```

### Implementing `FactoryBean<T>`
```java
public class SecureClientFactoryBean implements FactoryBean<SecureClient> {

    @Override
    public SecureClient getObject() throws Exception {
        return new SecureClient("https://api.internal.corp", "api-token-123");
    }

    @Override
    public Class<?> getObjectType() {
        return SecureClient.class;
    }

    @Override
    public boolean isSingleton() {
        return true;
    }
}
```

---

## 5. Practical Examples

### Example 1: Programmatic Bean Registration via `BeanDefinitionBuilder`
```java
package com.example.demo.definitions;

import org.springframework.beans.factory.support.BeanDefinitionBuilder;
import org.springframework.beans.factory.support.DefaultListableBeanFactory;
import org.springframework.context.support.GenericApplicationContext;

public class ProgrammaticDefinitionDemo {

    public record DatabaseConfig(String host, int port) {}

    public static void main(String[] args) {
        var context = new GenericApplicationContext();
        DefaultListableBeanFactory factory = context.getDefaultListableBeanFactory();

        // 1. Build BeanDefinition using builder
        var dbDef = BeanDefinitionBuilder.rootBeanDefinition(DatabaseConfig.class)
            .addConstructorArgValue("postgres-cluster.internal")
            .addConstructorArgValue(5432)
            .setPrimary(true)
            .getBeanDefinition();

        // 2. Register into registry
        factory.registerBeanDefinition("primaryDatabaseConfig", dbDef);

        // 3. Refresh context and query
        context.refresh();

        DatabaseConfig config = context.getBean(DatabaseConfig.class);
        System.out.println("Config Host: " + config.host() + ":" + config.port());

        context.close();
    }
}
```

### Example 2: Complex Third-Party Integration via `FactoryBean`
```java
package com.example.demo.factorybean;

import org.springframework.beans.factory.FactoryBean;
import org.springframework.context.annotation.AnnotationConfigApplicationContext;
import org.springframework.context.annotation.Bean;
import org.springframework.context.annotation.Configuration;

import java.net.URI;
import java.net.http.HttpClient;
import java.time.Duration;

// Third-party client without default constructor or Spring annotations
class ExternalAuditClient {
    private final HttpClient client;
    private final URI endpoint;

    public ExternalAuditClient(HttpClient client, URI endpoint) {
        this.client = client;
        this.endpoint = endpoint;
    }

    public void audit(String message) {
        System.out.println("Audit recorded to " + endpoint + ": " + message);
    }
}

// FactoryBean that encapsulates complex initialization logic
class ExternalAuditClientFactoryBean implements FactoryBean<ExternalAuditClient> {

    @Override
    public ExternalAuditClient getObject() {
        HttpClient httpClient = HttpClient.newBuilder()
            .connectTimeout(Duration.ofSeconds(3))
            .build();
        return new ExternalAuditClient(httpClient, URI.create("https://audit.corp.internal/v1"));
    }

    @Override
    public Class<?> getObjectType() {
        return ExternalAuditClient.class;
    }

    @Override
    public boolean isSingleton() {
        return true;
    }
}

@Configuration
class FactoryBeanConfig {
    @Bean
    public ExternalAuditClientFactoryBean auditClient() {
        return new ExternalAuditClientFactoryBean();
    }
}

public class FactoryBeanApp {
    public static void main(String[] args) {
        try (var ctx = new AnnotationConfigApplicationContext(FactoryBeanConfig.class)) {
            // Retrieves the product of getObject()
            ExternalAuditClient client = ctx.getBean(ExternalAuditClient.class);
            client.audit("Application startup verified");

            // Retrieves the FactoryBean itself using the '&' prefix
            Object factory = ctx.getBean("&auditClient");
            System.out.println("Underlying factory: " + factory.getClass().getName());
        }
    }
}
```

### Example 3: Modifying BeanDefinitions Dynamically via `BeanFactoryPostProcessor`
```java
package com.example.demo.postprocessor;

import org.springframework.beans.BeansException;
import org.springframework.beans.factory.config.BeanDefinition;
import org.springframework.beans.factory.config.BeanFactoryPostProcessor;
import org.springframework.beans.factory.config.ConfigurableListableBeanFactory;
import org.springframework.stereotype.Component;

@Component
public class EnvironmentSecurityPostProcessor implements BeanFactoryPostProcessor {

    @Override
    public void postProcessBeanFactory(ConfigurableListableBeanFactory beanFactory) throws BeansException {
        String[] beanNames = beanFactory.getBeanDefinitionNames();

        for (String name : beanNames) {
            BeanDefinition bd = beanFactory.getBeanDefinition(name);
            
            // Security enforcement: If bean is an unmanaged external connector, force lazy loading
            if (bd.getBeanClassName() != null && bd.getBeanClassName().contains("LegacyExternalConnector")) {
                bd.setLazyInit(true);
                System.out.println("Enforced lazy-init on legacy connector: " + name);
            }
        }
    }
}
```

### Example 4: Anti-Pattern & Remediation (Premature Bean Instantiation)
```java
// ANTI-PATTERN: Calling getBean() inside BeanFactoryPostProcessor
@Component
public class BrokenBeanFactoryPostProcessor implements BeanFactoryPostProcessor {

    @Override
    public void postProcessBeanFactory(ConfigurableListableBeanFactory beanFactory) {
        // FLAW: getBean() forces premature instantiation of OrderService BEFORE 
        // BeanPostProcessors or PropertySourcesPlaceholderConfigurer have registered!
        // This causes missing property injections and broken AOP proxies!
        OrderService service = beanFactory.getBean(OrderService.class);
        System.out.println("Service instantiated prematurely: " + service);
    }
}

// REMEDIATION: Inspect and modify BeanDefinition metadata ONLY, never instantiate beans!
@Component
public class CorrectBeanFactoryPostProcessor implements BeanFactoryPostProcessor {

    @Override
    public void postProcessBeanFactory(ConfigurableListableBeanFactory beanFactory) {
        // SAFE: Read or alter metadata without triggering class instantiation
        if (beanFactory.containsBeanDefinition("orderService")) {
            BeanDefinition bd = beanFactory.getBeanDefinition("orderService");
            bd.setPrimary(true); // Mutate definition safely
        }
    }
}
```

### Example 5: Merged BeanDefinition with Parent-Child Definition Hierarchy
```java
package com.example.demo.hierarchy;

import org.springframework.beans.factory.support.ChildBeanDefinition;
import org.springframework.beans.factory.support.DefaultListableBeanFactory;
import org.springframework.beans.factory.support.RootBeanDefinition;

public class MergedDefinitionDemo {

    public static class BaseService {
        private String environment;
        public void setEnvironment(String env) { this.environment = env; }
        public String getEnvironment() { return environment; }
    }

    public static class ProductionService extends BaseService {}

    public static void main(String[] args) {
        DefaultListableBeanFactory factory = new DefaultListableBeanFactory();

        // 1. Define parent definition template
        var parentDef = new RootBeanDefinition(BaseService.class);
        parentDef.getPropertyValues().add("environment", "PRODUCTION-CLUSTER");
        factory.registerBeanDefinition("baseServiceTemplate", parentDef);

        // 2. Define child definition inheriting parent properties
        var childDef = new ChildBeanDefinition("baseServiceTemplate");
        childDef.setBeanClass(ProductionService.class);
        factory.registerBeanDefinition("productionService", childDef);

        // 3. Factory merges child with parent automatically on getBean
        ProductionService service = factory.getBean("productionService", ProductionService.class);
        System.out.println("Inherited Property: " + service.getEnvironment());
    }
}
```

---

## 6. Best Practices

1. **Use `@Bean` for Standard Custom Creation**: In 95% of cases, standard `@Configuration` methods with `@Bean` are simpler, more type-safe, and easier to debug than custom `FactoryBean` or `BeanDefinitionBuilder`.
2. **Never Call `getBean()` inside `BeanFactoryPostProcessor`**: Doing so triggers premature instantiation of beans before property resolvers and bean post processors are operational.
3. **Use `FactoryBean<T>` Only for Complex Encapsulation**: Use `FactoryBean` when creating complex third-party client objects (e.g., JNDI lookups, dynamic gRPC stubs, connection pools) where object creation logic is too complex for an inline `@Bean` method.
4. **Leverage `BeanDefinitionRegistryPostProcessor` for Programmatic Additions**: When you need to discover and register *new* bean definitions dynamically, implement `BeanDefinitionRegistryPostProcessor` rather than standard `BeanFactoryPostProcessor`.
5. **Set Accurate `getObjectType()` in `FactoryBean`**: Always return the exact target class in `FactoryBean.getObjectType()`. Returning `null` or a generic `Object.class` breaks Spring's type-matching algorithms during autowiring.
6. **Mark Definitions as Primary or Fallback Explicitly**: Use `definition.setPrimary(true)` or `definition.setFallback(true)` when building dynamic starters to avoid autowiring ambiguity.

---

## 7. Common Mistakes & Pitfalls

| Mistake | Why It Fails | Practical Remedy |
| :--- | :--- | :--- |
| **Premature bean instantiation** | Calling `beanFactory.getBean()` inside a `BeanFactoryPostProcessor` breaks post-processing for that bean. | Only inspect and mutate `BeanDefinition` metadata; never invoke `getBean()`. |
| **Omitting the `&` prefix for FactoryBean** | Querying `context.getBean("myFactoryBean")` returns the product of `getObject()`, not the factory itself. | Use `context.getBean("&myFactoryBean")` to retrieve the factory instance. |
| **Returning `null` from `getObjectType()`** | Causes autowiring by type (`@Autowired MyClient client`) to fail because Spring cannot determine the type before instantiation. | Return the concrete class (e.g., `MyClient.class`) in `getObjectType()`. |
| **Mutating definitions after context refresh** | Attempting to modify `BeanDefinition` after `context.refresh()` throws `IllegalStateException`. | Perform all definition mutations inside a `BeanFactoryPostProcessor`. |
| **Forgetting to set scope on `FactoryBean`** | If `isSingleton()` returns `false`, `getObject()` is called on every single injection point, generating unwanted instances. | Override `isSingleton()` to return `true` for singletons. |
| **Bean name collision in registry** | Registering a definition with an existing name silently overwrites the previous definition (or crashes if overrides are disabled). | Check `registry.containsBeanDefinition(name)` before registration. |

---

## 8. Performance & Security Notes

### Memory Overhead of Definitions
Every `BeanDefinition` retains property maps, reflection constructor references, and method metadata. In microservices with minimal RAM budgets:
- Large numbers of unused bean definitions consume Metaspace and heap.
- Spring Boot's Ahead-of-Time (AOT) engine converts these definitions into static code at compile-time, eliminating runtime `BeanDefinition` graph storage in memory.

### Security Restrictions on Class Loading
`BeanDefinition.setBeanClassName(String)` resolves classes via reflection. Never allow user-supplied or untrusted external input (e.g., HTTP request parameters, unverified YAML files) to configure bean class names, as this creates Remote Code Execution (RCE) vulnerabilities.

---

## 9. Comparison / When to Use What

| Mechanism | Abstraction Level | Supports AOP Proxies | Complexity | Best For |
| :--- | :--- | :--- | :--- | :--- |
| **`@Component` / `@Service`** | High (Declarative) | Automatic | Very Low | Everyday domain and web development |
| **`@Bean` Method** | Medium (Declarative Java) | Automatic | Low | Third-party library configuration |
| **`FactoryBean<T>`** | Medium (Programmatic contract) | Automatic | Medium | Encapsulating complex client bootstrap logic |
| **`BeanDefinitionBuilder`** | Low (Internal metadata) | Automatic | High | Starters, dynamic plugins, custom scanners |

---

## 10. Interview Questions

<details>
<summary><strong>Q1: What is a `BeanDefinition` and why does Spring use it instead of direct Java Reflection?</strong></summary>

**Answer:**  
A `BeanDefinition` is an in-memory metadata representation of a Spring bean. Spring decouples configuration from instantiation so that:
1. It can validate and post-process metadata before any object is created.
2. It can resolve `${...}` property placeholders across all definitions simultaneously.
3. It can compute the dependency graph, detect cycles, and reorder bean creation.
4. It can apply conditional checks (`@Conditional`) without loading unneeded classes into JVM memory.
</details>

<details>
<summary><strong>Q2: What is a `FactoryBean<T>` and how does it differ from a standard `@Bean` method?</strong></summary>

**Answer:**  
A `FactoryBean<T>` is an interface (`getObject()`, `getObjectType()`, `isSingleton()`) that acts as an in-container factory for complex objects. While `@Bean` methods in `@Configuration` classes are ideal for straightforward instantiation, a `FactoryBean` encapsulates complex, multi-step construction logic (such as dynamic proxy generation or socket handshakes) into a dedicated, reusable, testable class.
</details>

<details>
<summary><strong>Q3: How do you retrieve the actual `FactoryBean` instance instead of the object it produces?</strong></summary>

**Answer:**  
Prefix the bean name with the ampersand character (`&`).  
For example:
- `context.getBean("myClient")` returns the product of `getObject()`.
- `context.getBean("&myClient")` returns the `MyClientFactoryBean` instance itself.
</details>

<details>
<summary><strong>Q4: What is the purpose of `BeanDefinitionRegistryPostProcessor`?</strong></summary>

**Answer:**  
`BeanDefinitionRegistryPostProcessor` is a sub-interface of `BeanFactoryPostProcessor`. It executes before standard `BeanFactoryPostProcessors` and allows the application to dynamically register *additional* `BeanDefinition` instances into the container before the `BeanFactory` finishes its setup phase.
</details>

<details>
<summary><strong>Q5: What are Merged Bean Definitions in Spring?</strong></summary>

**Answer:**  
When bean definitions use parent-child inheritance (where a child definition inherits properties, constructor arguments, or lifecycle settings from a parent template), Spring's container merges the child definition with its parent definition into a consolidated `RootBeanDefinition` before initiating bean creation.
</details>

<details>
<summary><strong>Q6: Why is calling `getBean()` inside `BeanFactoryPostProcessor.postProcessBeanFactory()` dangerous?</strong></summary>

**Answer:**  
Calling `getBean()` inside a `BeanFactoryPostProcessor` triggers premature bean instantiation. When this happens:
1. The requested bean is instantiated before other `BeanFactoryPostProcessors` (like property placeholder configurers) have run.
2. `BeanPostProcessors` (like AOP auto-proxy creators) may not yet be registered.
3. The bean misses critical post-processing, resulting in un-injected properties and disabled `@Transactional` aspects.
</details>

<details>
<summary><strong>Q7: What is the difference between `RootBeanDefinition` and `GenericBeanDefinition`?</strong></summary>

**Answer:**  
- `GenericBeanDefinition`: A standard, flexible bean definition class used since Spring 2.5 for programmatic and declaration-based bean registration. It can optionally specify a parent bean name.
- `RootBeanDefinition`: Represents a fully resolved, merged definition at runtime that has no parent. Spring converts all generic and child definitions into `RootBeanDefinition` internally before instantiating objects.
</details>

<details>
<summary><strong>Q8: How does Spring Boot 3 AOT optimization affect `BeanDefinition` handling?</strong></summary>

**Answer:**  
During Ahead-of-Time (AOT) compilation, Spring Boot inspects all `BeanDefinition` instances, evaluates conditions, and generates static Java source code (`BeanDefinitions__BeanDefinitions.java`) that directly registers beans via functional suppliers. At runtime, the application skips classpath scanning and dynamic reflection, booting near-instantly.
</details>

---

## 11. Practice Exercises

<details>
<summary><strong>Exercise 1: Inspect BeanDefinition Metadata (Easy)</strong></summary>

**Task:**  
Write a Spring Boot runner that gets the `ConfigurableApplicationContext`, retrieves the `BeanDefinition` for a service bean, and prints its class name, scope, lazy-init status, and primary flag.
</details>

<details>
<summary><strong>Exercise 2: Implement a Custom FactoryBean (Easy)</strong></summary>

**Task:**  
Create a `FactoryBean<java.security.MessageDigest>` that produces a `MessageDigest.getInstance("SHA-256")` instance. Register it as a bean and verify injection into a service.
</details>

<details>
<summary><strong>Exercise 3: Dynamic Registration with `BeanDefinitionBuilder` (Medium)</strong></summary>

**Task:**  
Implement a `BeanDefinitionRegistryPostProcessor` that reads an external property file and registers a separate `DataSourceConfig` bean definition for every database profile found in the file.
</details>

<details>
<summary><strong>Exercise 4: Enforce Mandatory Scopes via Post-Processor (Medium)</strong></summary>

**Task:**  
Write a `BeanFactoryPostProcessor` that scans all registered bean definitions and throws an exception if any class inside `com.example.service` has its scope set to `prototype`.
</details>

<details>
<summary><strong>Exercise 5: Dynamic Proxy FactoryBean (Hard)</strong></summary>

**Task:**  
Create a generic `FactoryBean<T>` that accepts an interface type, creates a JDK Dynamic Proxy intercepting all method calls to log execution durations, and registers the proxy into the context.
</details>

<details>
<summary><strong>Exercise 6: Programmatic Parent-Child Merging (Hard)</strong></summary>

**Task:**  
Create a base abstract `BeanDefinition` specifying common database timeout and retry attributes. Create three child `BeanDefinition` instances inheriting these values, verify successful property merging, and confirm all three beans instantiate properly.
</details>

---

## 12. Summary / Cheat Sheet

| Component / Method | Role | Example |
| :--- | :--- | :--- |
| **`BeanDefinition`** | Metadata blueprint for a Spring bean | Stores scope, lazy-init, constructor args |
| **`BeanDefinitionBuilder`** | Fluent API to construct definitions | `BeanDefinitionBuilder.rootBeanDefinition(...)` |
| **`FactoryBean<T>`** | Factory contract for complex objects | Produces `T` via `getObject()` |
| **`&` Bean Name Prefix** | Fetches the FactoryBean itself | `ctx.getBean("&myFactoryBean")` |
| **`BeanFactoryPostProcessor`** | Modifies definitions before creation | `postProcessBeanFactory(ConfigurableListableBeanFactory)` |
| **`BeanDefinitionRegistryPostProcessor`** | Adds new definitions at runtime | `postProcessBeanDefinitionRegistry(BeanDefinitionRegistry)` |
| **`GenericBeanDefinition`** | Standard versatile definition | Replaced XML/child definitions in modern code |

---

## 13. Further Reading
- [Spring Framework Reference: Bean Overview and Definitions](https://docs.spring.io/spring-framework/reference/core/beans/definition.html)
- [Spring FactoryBean Interface Javadoc](https://docs.spring.io/spring-framework/docs/current/javadoc-api/org/springframework/beans/factory/FactoryBean.html)
- [Spring BeanDefinition Interface Javadoc](https://docs.spring.io/spring-framework/docs/current/javadoc-api/org/springframework/beans/factory/config/BeanDefinition.html)
