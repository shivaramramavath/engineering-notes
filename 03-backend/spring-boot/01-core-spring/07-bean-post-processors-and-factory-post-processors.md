# BeanPostProcessors & BeanFactoryPostProcessors

> The internal engine of Spring customization: differentiating `BeanFactoryPostProcessor` (metadata mutation) from `BeanPostProcessor` (instance mutation and proxying), ordering protocols with `PriorityOrdered`, and building custom container interceptors.

| Metadata | Value |
| :--- | :--- |
| **Level** | Advanced |
| **Prerequisites** | [IoC Container & ApplicationContext Internals](file:///03-backend/spring-boot/01-core-spring/01-ioc-container-and-application-context.md), [Spring Beans & BeanDefinitions](file:///03-backend/spring-boot/01-core-spring/02-beans-and-bean-definitions.md) |
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
How does Spring implement `@Autowired`, `@Value`, `@Transactional`, `@Async`, `@PostConstruct`, and validation without requiring developers to write invasive boilerplate code around every bean? The container achieves this magic through two fundamental extension points: post-processors. Without them, Spring would be nothing more than a static map of Java objects.

### Historical Evolution
- **Spring 1.x**: Introduced `PropertyPlaceholderConfigurer` (a `BeanFactoryPostProcessor`) to resolve `${...}` expressions from properties files.
- **Spring 2.0 - 2.5**: Introduced `BeanPostProcessor` to power annotation-driven injection (`@Autowired`) and dynamic proxy wrapping for AOP.
- **Spring 3.0+**: Introduced `BeanDefinitionRegistryPostProcessor` to allow programmatic registration of new bean definitions before any factory post-processing occurs.
- **Spring 6.x & Boot 3.x**: Post-processor analysis is leveraged by the Ahead-of-Time (AOT) engine to pre-compute bean modifications at build time.

### Real-World Relevance
Understanding the distinction between metadata-level processors (`BFPP`) and instance-level processors (`BPP`) is the key to mastering Spring framework internals, troubleshooting ordering bugs, and developing enterprise-grade custom extensions.

---

## 2. Core Concepts

### 1. `BeanFactoryPostProcessor` (BFPP)
- **Operates On**: `BeanDefinition` metadata **before** any bean instances are created.
- **Purpose**: Modifies bean definitions, resolves property placeholders, alters bean scopes, or enforces configuration policies.
- **Lifecycle Timing**: Runs immediately after bean definitions are loaded into the `DefaultListableBeanFactory`, before any standard beans exist.

### 2. `BeanDefinitionRegistryPostProcessor` (BDRPP)
- **Sub-interface of BFPP**: Provides direct access to `BeanDefinitionRegistry`.
- **Purpose**: Dynamically registers **new** bean definitions at runtime (e.g., `ConfigurationClassPostProcessor` uses this to parse `@Configuration` classes and register `@Bean` definitions).

### 3. `BeanPostProcessor` (BPP)
- **Operates On**: Live Java bean **instances** as they are being initialized.
- **Two Hook Methods**:
  * `postProcessBeforeInitialization(Object bean, String beanName)`: Executes *before* `@PostConstruct` and `InitializingBean`.
  * `postProcessAfterInitialization(Object bean, String beanName)`: Executes *after* initialization. **This is where AOP dynamic proxies are wrapped!**

### 4. Execution Ordering: `PriorityOrdered` vs. `Ordered`
Spring executes post-processors in strict order:
1. Processors implementing `PriorityOrdered`
2. Processors implementing `Ordered` (sorted by `@Order(int)` or `getOrder()`)
3. Non-ordered processors

---

## 3. How It Works Internally

### ApplicationContext Post-Processor Execution Timeline

```
+-------------------------------------------------------------------------+
|                  1. Load Raw BeanDefinitions Into Registry              |
+------------------------------------+------------------------------------+
                                     |
                                     v
+-------------------------------------------------------------------------+
|                  2. BeanDefinitionRegistryPostProcessor (BDRPP)         |
|  - `ConfigurationClassPostProcessor` parses @Configuration classes      |
|  - Registers newly discovered @Bean definitions                         |
+------------------------------------+------------------------------------+
                                     |
                                     v
+-------------------------------------------------------------------------+
|                  3. BeanFactoryPostProcessor (BFPP)                     |
|  - `PropertySourcesPlaceholderConfigurer` resolves `${...}` tokens      |
|  - Modifies BeanDefinition properties in-place                           |
+------------------------------------+------------------------------------+
                                     |
                                     v
+-------------------------------------------------------------------------+
|                  4. Register BeanPostProcessors (BPP)                   |
|  - Discovers all BPP classes in the registry                            |
|  - Instantiates BPPs (PriorityOrdered -> Ordered -> Non-Ordered)        |
+------------------------------------+------------------------------------+
                                     |
                                     v
+-------------------------------------------------------------------------+
|                  5. Pre-Instantiate All Standard Singletons             |
|  For each normal bean:                                                  |
|    a. Constructor Instantiation (`new MyBean()`)                        |
|    b. Property Population                                               |
|    c. BPP: postProcessBeforeInitialization()                            |
|    d. Lifecycle Callbacks (@PostConstruct, InitializingBean)            |
|    e. BPP: postProcessAfterInitialization() (WRAP IN PROXY HERE!)       |
|    f. Bean placed into Singleton Cache                                  |
+-------------------------------------------------------------------------+
```

---

## 4. Syntax & Usage

### Implementing a `BeanPostProcessor`
```java
@Component
public class CustomAuditPostProcessor implements BeanPostProcessor {

    @Override
    public Object postProcessBeforeInitialization(Object bean, String beanName) {
        // Inspect or configure bean before @PostConstruct runs
        return bean; // Return original or wrapped object
    }

    @Override
    public Object postProcessAfterInitialization(Object bean, String beanName) {
        // Wrap with dynamic proxy if required
        return bean;
    }
}
```

### Implementing a `BeanFactoryPostProcessor`
```java
@Component
public class MandatoryScopeEnforcer implements BeanFactoryPostProcessor {

    @Override
    public void postProcessBeanFactory(ConfigurableListableBeanFactory beanFactory) {
        for (String name : beanFactory.getBeanDefinitionNames()) {
            BeanDefinition bd = beanFactory.getBeanDefinition(name);
            // Metadata inspection / mutation
        }
    }
}
```

---

## 5. Practical Examples

### Example 1: Custom Annotation Processor via `BeanPostProcessor`
```java
package com.example.demo.bpp;

import org.springframework.beans.BeansException;
import org.springframework.beans.factory.config.BeanPostProcessor;
import org.springframework.stereotype.Component;

import java.lang.annotation.*;
import java.lang.reflect.Field;
import java.util.UUID;

// Custom Marker Annotation
@Target(ElementType.FIELD)
@Retention(RetentionPolicy.RUNTIME)
public @interface UniqueInstanceId {}

// BeanPostProcessor that injects a random UUID into annotated fields
@Component
public class UniqueInstanceIdPostProcessor implements BeanPostProcessor {

    @Override
    public Object postProcessBeforeInitialization(Object bean, String beanName) throws BeansException {
        Field[] fields = bean.getClass().getDeclaredFields();
        for (Field field : fields) {
            if (field.isAnnotationPresent(UniqueInstanceId.class) && field.getType().equals(String.class)) {
                field.setAccessible(true);
                try {
                    field.set(bean, "NODE-" + UUID.randomUUID().toString().substring(0, 8));
                } catch (IllegalAccessException e) {
                    throw new RuntimeException("Failed to inject unique instance ID", e);
                }
            }
        }
        return bean; // Return the modified bean instance
    }
}
```

### Example 2: Dynamic Proxy Wrapping in `postProcessAfterInitialization`
```java
package com.example.demo.proxy;

import org.springframework.beans.BeansException;
import org.springframework.beans.factory.config.BeanPostProcessor;
import org.springframework.cglib.proxy.Enhancer;
import org.springframework.cglib.proxy.MethodInterceptor;
import org.springframework.stereotype.Component;

import java.lang.annotation.*;

@Target(ElementType.TYPE)
@Retention(RetentionPolicy.RUNTIME)
public @interface TimedExecution {}

@Component
public class TimedExecutionPostProcessor implements BeanPostProcessor {

    @Override
    public Object postProcessAfterInitialization(Object bean, String beanName) throws BeansException {
        Class<?> beanClass = bean.getClass();

        if (beanClass.isAnnotationPresent(TimedExecution.class)) {
            Enhancer enhancer = new Enhancer();
            enhancer.setSuperclass(beanClass);
            enhancer.setCallback((MethodInterceptor) (obj, method, args, proxy) -> {
                long start = System.nanoTime();
                try {
                    return proxy.invoke(bean, args);
                } finally {
                    long elapsed = (System.nanoTime() - start) / 1_000_000;
                    System.out.printf("[METRIC] %s.%s executed in %d ms%n", 
                        beanClass.getSimpleName(), method.getName(), elapsed);
                }
            });
            return enhancer.create(); // Return CGLIB proxy in place of original bean!
        }
        return bean;
    }
}
```

### Example 3: Dynamic Registration via `BeanDefinitionRegistryPostProcessor`
```java
package com.example.demo.bdrpp;

import org.springframework.beans.BeansException;
import org.springframework.beans.factory.config.ConfigurableListableBeanFactory;
import org.springframework.beans.factory.support.BeanDefinitionBuilder;
import org.springframework.beans.factory.support.BeanDefinitionRegistry;
import org.springframework.beans.factory.support.BeanDefinitionRegistryPostProcessor;
import org.springframework.core.Ordered;
import org.springframework.core.PriorityOrdered;
import org.springframework.stereotype.Component;

public record CloudRegion(String regionName, boolean active) {}

@Component
public class RegionRegistryPostProcessor implements BeanDefinitionRegistryPostProcessor, PriorityOrdered {

    @Override
    public void postProcessBeanDefinitionRegistry(BeanDefinitionRegistry registry) throws BeansException {
        // Dynamically register 3 regional beans into the context
        for (String region : new String[]{"us-east-1", "eu-west-1", "ap-southeast-1"}) {
            var definition = BeanDefinitionBuilder.rootBeanDefinition(CloudRegion.class)
                .addConstructorArgValue(region)
                .addConstructorArgValue(true)
                .getBeanDefinition();

            registry.registerBeanDefinition("region_" + region, definition);
            System.out.println("Dynamically registered bean definition: region_" + region);
        }
    }

    @Override
    public void postProcessBeanFactory(ConfigurableListableBeanFactory beanFactory) throws BeansException {
        // Metadata mutation phase if required
    }

    @Override
    public int getOrder() {
        return Ordered.HIGHEST_PRECEDENCE;
    }
}
```

### Example 4: Premature Bean Instantiation Anti-Pattern & Remediation
```java
// ANTI-PATTERN: Injecting a service bean into a BeanPostProcessor
@Component
public class BadAuditPostProcessor implements BeanPostProcessor {

    // FLAW: Autowiring AuditLogService causes AuditLogService to be instantiated 
    // BEFORE BeanPostProcessors are fully registered! AuditLogService misses out 
    // on all subsequent BPPs (e.g., @Transactional will NOT work on AuditLogService)!
    @Autowired
    private AuditLogService auditLogService;

    @Override
    public Object postProcessAfterInitialization(Object bean, String beanName) {
        auditLogService.log("Bean created: " + beanName);
        return bean;
    }
}

// REMEDIATION: Inject collaborator lazily using ObjectProvider or ApplicationContext
@Component
public class CleanAuditPostProcessor implements BeanPostProcessor {

    private final org.springframework.beans.factory.ObjectProvider<AuditLogService> auditLogProvider;

    public CleanAuditPostProcessor(org.springframework.beans.factory.ObjectProvider<AuditLogService> provider) {
        this.auditLogProvider = provider; // Lazy resolution
    }

    @Override
    public Object postProcessAfterInitialization(Object bean, String beanName) {
        if (!beanName.equals("auditLogService")) {
            AuditLogService service = auditLogProvider.getIfAvailable();
            if (service != null) {
                service.log("Bean created: " + beanName);
            }
        }
        return bean;
    }
}
```

### Example 5: Custom Property Decryption Post-Processor
```java
package com.example.demo.decrypt;

import org.springframework.beans.BeansException;
import org.springframework.beans.factory.config.BeanDefinition;
import org.springframework.beans.factory.config.BeanFactoryPostProcessor;
import org.springframework.beans.factory.config.ConfigurableListableBeanFactory;
import org.springframework.beans.factory.config.TypedStringValue;
import org.springframework.stereotype.Component;

@Component
public class DecryptingBeanFactoryPostProcessor implements BeanFactoryPostProcessor {

    @Override
    public void postProcessBeanFactory(ConfigurableListableBeanFactory beanFactory) throws BeansException {
        for (String beanName : beanFactory.getBeanDefinitionNames()) {
            BeanDefinition bd = beanFactory.getBeanDefinition(beanName);
            
            // Inspect property values for encrypted tags: "ENC(xyz)"
            bd.getPropertyValues().forEach(pv -> {
                if (pv.getValue() instanceof TypedStringValue tsv) {
                    String raw = tsv.getValue();
                    if (raw != null && raw.startsWith("ENC(") && raw.endsWith(")")) {
                        String encrypted = raw.substring(4, raw.length() - 1);
                        String decrypted = "DECRYPTED-" + encrypted; // Replace with AES decrypt
                        pv.setConvertedValue(decrypted);
                        System.out.printf("Decrypted property '%s' for bean '%s'%n", pv.getName(), beanName);
                    }
                }
            });
        }
    }
}
```

---

## 6. Best Practices

1. **Keep `BeanPostProcessor` Logic Extremely Fast**: A BPP intercepts *every single bean* created in the entire application. Any slow blocking I/O inside `postProcessAfterInitialization()` compounds across thousands of beans, devastating startup latency.
2. **Never Autowire Standard Services Directly into BPPs**: Direct dependencies cause premature bean instantiation, stripping those services of AOP proxies, transaction managers, and security filters. Use `ObjectProvider<T>` or `Lazy<T>`.
3. **Declare BFPPs as `static @Bean` Methods**: In `@Configuration` classes, always declare `@Bean` methods returning `BeanFactoryPostProcessor` as `static`:
   ```java
   @Bean
   public static CustomBeanFactoryPostProcessor customBfpp() {
       return new CustomBeanFactoryPostProcessor();
   }
   ```
   Declaring it `static` allows the container to instantiate the BFPP without eagerly instantiating the enclosing `@Configuration` class!
4. **Implement `PriorityOrdered` or `Ordered`**: Always assign an explicit order to custom post-processors to prevent non-deterministic execution relative to built-in Spring processors.
5. **Always Return the Bean**: In both `postProcessBeforeInitialization` and `postProcessAfterInitialization`, always return the bean (either original or wrapped). Returning `null` silences the bean, causing other BPPs to be skipped and throwing `NullPointerException`.
6. **Use `BeanDefinitionRegistryPostProcessor` for Programmatic Additions**: When you need to discover and register *new* bean definitions dynamically, implement `BeanDefinitionRegistryPostProcessor`.

---

## 7. Common Mistakes & Pitfalls

| Mistake | Why It Fails | Practical Remedy |
| :--- | :--- | :--- |
| **Non-static BFPP `@Bean` method** | Forces the enclosing `@Configuration` class to be instantiated prematurely, breaking `@Autowired` and `@Value` on it. | Mark the `@Bean` method as `static`. |
| **Premature bean instantiation via BPP** | Injecting a regular service into a BPP initializes that service before other BPPs are registered. | Use `ObjectProvider<T>` for lazy resolution. |
| **Returning `null` from a BPP method** | Skips subsequent post-processors and replaces the bean instance with `null` in the container. | Always return the bean instance (`return bean;`). |
| **Calling `getBean()` inside BFPP** | Instantiates beans before property placeholder resolution or security configuration is ready. | Only read/modify `BeanDefinition` metadata; never invoke `getBean()`. |
| **Missing `Ordered` interface** | BPP runs in arbitrary order, possibly before Spring's core annotation processors or after proxy creators. | Implement `Ordered` or annotate with `@Order`. |
| **Overwriting proxies unintentionally** | Returning the raw original bean in `postProcessAfterInitialization` when a previous BPP had already wrapped it in a proxy removes the proxy. | Always operate on the `bean` parameter passed in, not an original saved reference. |

---

## 8. Performance & Security Notes

### Overhead of Broad Instance Inspection
If a `BeanPostProcessor` performs heavy Java Reflection (`clazz.getDeclaredMethods()`) on every bean, startup time scales linearly with bean count. Cache reflection metadata or filter early by checking `beanName` or package prefix before performing reflection.

### Security Wrapping
`BeanPostProcessor` is the primary mechanism used by security frameworks to create defensive proxies, sanitize inputs, and attach RBAC checks. Ensure custom BPPs execute *before* or *after* security proxy creators deterministically.

---

## 9. Comparison / When to Use What

| Feature | `BeanFactoryPostProcessor` | `BeanPostProcessor` |
| :--- | :--- | :--- |
| **Target** | `BeanDefinition` (Metadata) | Live Java Object (Instance) |
| **Execution Timing** | Before any regular bean instantiation | After instantiation, during bean lifecycle |
| **AOP Proxying** | Cannot create proxies | **Primary mechanism for creating proxies** |
| **Can Modify Property Placeholders?** | **Yes** (e.g. `${db.url}`) | No |
| **Can Wrap/Swap Bean Instance?** | No | **Yes** (returns proxy) |
| **Best For** | Environment variables, definition changes | Annotations (`@Async`), dynamic proxies |

---

## 10. Interview Questions

<details>
<summary><strong>Q1: What is the fundamental difference between `BeanFactoryPostProcessor` and `BeanPostProcessor`?</strong></summary>

**Answer:**  
- `BeanFactoryPostProcessor` operates on **bean metadata** (`BeanDefinition`) *before* any bean instances are created. It can modify bean definitions (e.g., resolving `${...}` property placeholders or changing scopes).
- `BeanPostProcessor` operates on **live bean instances** *during* their initialization lifecycle. It executes before and after initialization callbacks (`@PostConstruct`) and can wrap, modify, or replace bean instances with dynamic proxies (e.g., for AOP, transactions, or security).
</details>

<details>
<summary><strong>Q2: Why must `@Bean` methods that return a `BeanFactoryPostProcessor` be declared `static`?</strong></summary>

**Answer:**  
Because `BeanFactoryPostProcessors` must execute before any standard beans are initialized. If the `@Bean` method is non-static, Spring must instantiate the enclosing `@Configuration` class to call the method. This causes premature instantiation of the configuration class, meaning any `@Autowired` fields, `@Value` properties, or post-processors on the configuration class itself will fail or be skipped. Making the method `static` allows Spring to invoke it without instantiating the configuration class.
</details>

<details>
<summary><strong>Q3: How does Spring create `@Transactional` dynamic proxies under the hood?</strong></summary>

**Answer:**  
Through an internal `BeanPostProcessor` (specifically `InfrastructureAdvisorAutoProxyCreator` / `AnnotationAwareAspectJAutoProxyCreator`). In its `postProcessAfterInitialization()` method, it inspects the bean class and its methods for `@Transactional`. If found, it creates and returns a dynamic proxy (CGLIB or JDK Dynamic Proxy) wrapping the original bean, intercepting method calls to manage transactions.
</details>

<details>
<summary><strong>Q4: What causes the infamous "Bean is not eligible for getting processed by all BeanPostProcessors" warning?</strong></summary>

**Answer:**  
This warning occurs when a regular bean is instantiated **prematurely**—usually because it is injected directly into a `BeanFactoryPostProcessor` or `BeanPostProcessor`. Because the bean is created before all `BeanPostProcessors` have been discovered and registered, it misses out on post-processing (such as autowiring, property resolution, and AOP proxying).
</details>

<details>
<summary><strong>Q5: What are the two lifecycle hook methods of `BeanPostProcessor` and when do they run?</strong></summary>

**Answer:**  
1. `postProcessBeforeInitialization(Object bean, String beanName)`: Runs *after* constructor instantiation and property population, but *before* `@PostConstruct`, `InitializingBean.afterPropertiesSet()`, and custom init methods.
2. `postProcessAfterInitialization(Object bean, String beanName)`: Runs *after* all initialization callbacks have completed. This is where dynamic proxies are returned.
</details>

<details>
<summary><strong>Q6: What is `BeanDefinitionRegistryPostProcessor` and how does it relate to `@Configuration`?</strong></summary>

**Answer:**  
`BeanDefinitionRegistryPostProcessor` is a sub-interface of `BeanFactoryPostProcessor` that can register new bean definitions before regular BFPPs execute. Spring's core `ConfigurationClassPostProcessor` implements this interface to discover, parse `@Configuration` classes, and register all `@Bean` definitions into the container.
</details>

<details>
<summary><strong>Q7: What happens if a `BeanPostProcessor` returns `null` from `postProcessAfterInitialization`?</strong></summary>

**Answer:**  
Returning `null` tells Spring that the bean should be suppressed. Subsequent `BeanPostProcessors` will not be invoked for this bean, and the bean reference in the container will be set to `null`, causing subsequent dependency injections of that bean to fail with a `NullPointerException` or `NoSuchBeanDefinitionException`.
</details>

<details>
<summary><strong>Q8: How does Spring determine the order in which multiple `BeanPostProcessors` execute?</strong></summary>

**Answer:**  
Spring categorizes processors into three tiers:
1. Processors implementing `org.springframework.core.PriorityOrdered` (executed first, sorted by order value).
2. Processors implementing `org.springframework.core.Ordered` (executed second, sorted by order value).
3. Processors implementing neither (executed last, in non-deterministic order).
</details>

---

## 11. Practice Exercises

<details>
<summary><strong>Exercise 1: Create a Field-Injection Warning BPP (Easy)</strong></summary>

**Task:**  
Write a `BeanPostProcessor` that inspects every bean's fields in `postProcessBeforeInitialization()`. If any field has `@Autowired`, log a high-priority warning: `"ANTI-PATTERN DETECTED: Field injection used in bean [beanName]"`.
</details>

<details>
<summary><strong>Exercise 2: Implement Static BFPP Declaration (Easy)</strong></summary>

**Task:**  
Create a `@Configuration` class declaring a `static @Bean` returning a custom `BeanFactoryPostProcessor`. Verify in console logs that the BFPP runs before the `@Configuration` constructor is called.
</details>

<details>
<summary><strong>Exercise 3: Custom Timing Proxy BPP (Medium)</strong></summary>

**Task:**  
Create a custom annotation `@Audited`. Write a `BeanPostProcessor` that checks for `@Audited` on class level in `postProcessAfterInitialization()` and wraps the bean in a JDK Dynamic Proxy that prints method execution times.
</details>

<details>
<summary><strong>Exercise 4: Reproduce and Fix "Not Eligible for BPP" Warning (Medium)</strong></summary>

**Task:**  
Create a `BeanPostProcessor` that autowires a regular `@Service DatabaseService`. Observe the startup warning `"is not eligible for getting processed by all BeanPostProcessors"`. Refactor using `ObjectProvider<DatabaseService>` to eliminate the warning.
</details>

<details>
<summary><strong>Exercise 5: Dynamic Bean Registration with BDRPP (Hard)</strong></summary>

**Task:**  
Implement `BeanDefinitionRegistryPostProcessor`. Query a configuration file or environment list of tenant IDs (`tenantA`, `tenantB`), and programmatically register a dedicated `TenantService` bean definition for each tenant ID.
</details>

<details>
<summary><strong>Exercise 6: Property Overriding BFPP (Hard)</strong></summary>

**Task:**  
Implement a `BeanFactoryPostProcessor` that scans all registered bean definitions and forces all singleton beans whose class name ends with `Cache` to be reconfigured as `lazyInit = true` and `scope = prototype`.
</details>

---

## 12. Summary / Cheat Sheet

| Extension Point | Interface | Timing | Capability |
| :--- | :--- | :--- | :--- |
| **BDRPP** | `BeanDefinitionRegistryPostProcessor` | Step 1: Definition registration | Registers new `BeanDefinition`s |
| **BFPP** | `BeanFactoryPostProcessor` | Step 2: Metadata mutation | Resolves `${...}`, mutates scopes |
| **BPP (Before)** | `postProcessBeforeInitialization` | Step 4a: Pre-init | Injects annotations, custom config |
| **Lifecycle** | `@PostConstruct` / `afterPropertiesSet` | Step 4b: Init callback | Bean self-initialization |
| **BPP (After)** | `postProcessAfterInitialization` | Step 4c: Post-init | **Wraps beans in AOP Proxies** |
| **Ordering** | `PriorityOrdered` > `Ordered` | Execution sequence | Guarantees deterministic execution |

---

## 13. Further Reading
- [Official Spring Framework Reference: Customizing Beans using a BeanPostProcessor](https://docs.spring.io/spring-framework/reference/core/beans/factory-extension.html#beans-factory-extension-bpp)
- [Official Spring Framework Reference: Customizing Configuration Metadata with a BeanFactoryPostProcessor](https://docs.spring.io/spring-framework/reference/core/beans/factory-extension.html#beans-factory-extension-factory-postprocessors)
- [Spring PriorityOrdered Javadoc](https://docs.spring.io/spring-framework/docs/current/javadoc-api/org/springframework/core/PriorityOrdered.html)
