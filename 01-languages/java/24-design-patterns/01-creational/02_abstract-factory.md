# Abstract Factory Pattern in Modern Java

> Creating families of related or dependent objects without specifying their concrete classes: UI toolkits, cross-platform persistence families, and DI integration.

| Metadata | Details |
| :--- | :--- |
| **Level** | Core Design Pattern / Creational |
| **Prerequisites** | [01_factory-method](01_factory-method.md), [00_solid-principles](../23-design-and-clean-code/00_solid-principles.md) |
| **Time to Read** | 35 minutes |
| **Status** | Verified Production Guide |
| **Java Baseline** | Java 17 LTS / Java 21 LTS / Java 25 LTS |

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
10. [Interview Questions & Answers](#10-interview-questions--answers)
11. [Practice Exercises](#11-practice-exercises)
12. [Summary / Cheat Sheet](#12-summary--cheat-sheet)
13. [Further Reading](#13-further-reading)

---

## 1. What & Why

### What is the Abstract Factory Pattern?
The Abstract Factory is a creational design pattern that provides an interface for creating **families of related or dependent objects** without specifying their concrete classes.

Unlike the Factory Method pattern (which focuses on creating a single product), Abstract Factory ensures that products from the same family are always used together, preventing incompatible mismatches:
- UI Theme families (Dark Theme vs. Light Theme buttons, text boxes, and scrollbars).
- Multi-cloud infrastructure families (AWS vs. Azure vs. GCP storage buckets, compute instances, and queues).
- Cross-platform database toolkits (Postgres connection, dialect, and query generator).

### Why Abstract Factory Matters
Without an Abstract Factory, client code can accidentally pair a `DarkThemeButton` with a `LightThemeScrollbar`, or an `AwsS3Bucket` with an `AzureQueueClient`. Abstract Factory guarantees family consistency at compile time.

---

## 2. Core Concepts

### Product Families
A product family is a set of distinct products designed to collaborate:
- Family A (Dark Mode): `DarkButton`, `DarkCheckbox`, `DarkDialog`.
- Family B (Light Mode): `LightButton`, `LightCheckbox`, `LightDialog`.
The client code interacts only with abstract interfaces (`Button`, `Checkbox`, `Dialog`).

### The Abstract Factory Interface
Defines creation methods for every member of the product family:
```java
public interface GUIFactory {
    Button createButton();
    Checkbox createCheckbox();
}
```

---

## 3. How It Works Internally

### Architectural Family Consistency Flow
```
+--------------------------------------------------------------------------+
|                  ABSTRACT FACTORY COLLABORATION MATRIX                   |
|                                                                          |
|                     [ GUIFactory (Interface) ]                           |
|                      + createButton(): Button                            |
|                      + createCheckbox(): Checkbox                        |
|                                 ^                                        |
|                +----------------+----------------+                       |
|                |                                 |                       |
|     [ DarkThemeFactory ]               [ LightThemeFactory ]             |
|       + createButton()  -> DarkBtn       + createButton()  -> LightBtn   |
|       + createCheckbox()-> DarkChk       + createCheckbox()-> LightChk   |
|                                                                          |
|  Client executes:                                                        |
|    Button b = factory.createButton();                                    |
|    Checkbox c = factory.createCheckbox();                                |
|  (Client CANNOT mix DarkButton with LightCheckbox; family is enforced!)  |
+--------------------------------------------------------------------------+
```

---

## 4. Syntax & Usage

### 1. Declaring the Abstract Factory and Products
```java
// Abstract Products
public interface Button { void render(); }
public interface Checkbox { void render(); }

// Abstract Factory
public interface ThemeFactory {
    Button createButton();
    Checkbox createCheckbox();
}
```

---

## 5. Practical Examples

### Example 1: Multi-Cloud Infrastructure Provisioner
**Goal:** Provision compute and object storage across cloud vendors (AWS vs. Azure) without client coupling.

```java
// Abstract Products
public interface ComputeInstance { void launch(); }
public interface ObjectStorage { void upload(String key, byte[] data); }

// Abstract Factory Interface
public interface CloudResourceFactory {
    ComputeInstance createCompute();
    ObjectStorage createStorage();
}

// Concrete Family 1: AWS
public class AwsCompute implements ComputeInstance {
    @Override public void launch() { System.out.println("Launching AWS EC2 c6g.xlarge"); }
}
public class AwsStorage implements ObjectStorage {
    @Override public void upload(String k, byte[] d) { System.out.println("Uploading to AWS S3 bucket: " + k); }
}
public class AwsResourceFactory implements CloudResourceFactory {
    @Override public ComputeInstance createCompute() { return new AwsCompute(); }
    @Override public ObjectStorage createStorage() { return new AwsStorage(); }
}

// Concrete Family 2: Azure
public class AzureCompute implements ComputeInstance {
    @Override public void launch() { System.out.println("Launching Azure VM D4s_v5"); }
}
public class AzureStorage implements ObjectStorage {
    @Override public void upload(String k, byte[] d) { System.out.println("Uploading to Azure Blob: " + k); }
}
public class AzureResourceFactory implements CloudResourceFactory {
    @Override public ComputeInstance createCompute() { return new AzureCompute(); }
    @Override public ObjectStorage createStorage() { return new AzureStorage(); }
}

// Client Application
public class CloudDeployer {
    private final ComputeInstance compute;
    private final ObjectStorage storage;

    public CloudDeployer(CloudResourceFactory factory) {
        // Enforces that compute and storage originate from the exact same cloud provider!
        this.compute = factory.createCompute();
        this.storage = factory.createStorage();
    }

    public void deployWorkload() {
        compute.launch();
        storage.upload("config.json", new byte[]{1, 2, 3});
    }
}
```

### Example 2: Dynamic Factory Selection via Configuration
**Goal:** Instantiate the appropriate factory family based on an external environment variable.

```java
public class CloudFactoryProvider {
    public static CloudResourceFactory getFactory(String provider) {
        return switch (provider.toUpperCase()) {
            case "AWS" -> new AwsResourceFactory();
            case "AZURE" -> new AzureResourceFactory();
            default -> throw new IllegalArgumentException("Unsupported cloud vendor: " + provider);
        };
    }
}
```

---

### Example 3: Cross-Platform Operating System Widget Toolkit
**Goal:** Abstract Native Windows vs. Linux GUI widgets with explicit theme contracts.

```java
public interface Window { void draw(); }
public interface ScrollBar { void scroll(int offset); }

public interface SystemWidgetFactory {
    Window createWindow();
    ScrollBar createScrollBar();
}

public class LinuxWidgetFactory implements SystemWidgetFactory {
    @Override public Window createWindow() { return () -> System.out.println("Rendering Wayland/X11 Window"); }
    @Override public ScrollBar createScrollBar() { return o -> System.out.println("Wayland Scroll: " + o); }
}

public class WindowsWidgetFactory implements SystemWidgetFactory {
    @Override public Window createWindow() { return () -> System.out.println("Rendering DWM/Win32 Window"); }
    @Override public ScrollBar createScrollBar() { return o -> System.out.println("Win32 Scroll: " + o); }
}
```

### Example 4: Dependency Injection Configuration with Spring Beans
**Goal:** Bind abstract factory beans cleanly based on active environment profiles.

```java
// Spring JavaConfig configuration class:
// @Configuration
public class CloudInfrastructureConfig {
    // @Bean
    // @Profile("aws")
    public CloudResourceFactory awsFactory() {
        return new AwsResourceFactory();
    }

    // @Bean
    // @Profile("azure")
    public CloudResourceFactory azureFactory() {
        return new AzureResourceFactory();
    }
}
```

## 6. Best Practices

1. **Use Abstract Factory for Cohesive Families Only**: Do not create an abstract factory if products do not need to work together.
2. **Combine with Dependency Injection**: In Spring or Guice, configure the concrete factory as a bean and inject it into consuming services.
3. **Keep Product Interfaces Focused**: Follow the Interface Segregation Principle for individual product contracts.
4. **Enforce Immutability Where Possible**: Product configuration objects should be immutable records.
5. **Use Factory of Factories for Dynamic Selection**: Encapsulate the choice of concrete factory in a clean provider method.
6. **Beware of Interface Bloat**: Adding a new product type to an existing Abstract Factory forces modifications across all concrete factory subclasses.

---

## 7. Common Mistakes & Pitfalls

| Mistake | Why It Fails | Secure Fix |
| :--- | :--- | :--- |
| **Adding a new product type frequently** | Modifying `AbstractFactory` breaks all concrete factory implementations. | Use Factory Method or plugin registry if product types vary rapidly. |
| **Mixing products across families** | Instantiating `new AwsCompute()` alongside `new AzureStorage()`. | Strictly use factory methods to obtain products. |
| **Returning concrete classes from factory** | Defeats polymorphism; clients become coupled to implementations. | Always return abstract product interfaces. |
| **Over-engineering for 1 cloud or theme** | Adds 6 interfaces and 8 classes without business need (violates YAGNI). | Start with direct classes; refactor to Abstract Factory when 2nd family arrives. |

---

## 8. Performance & Security Notes

### Interface Call Virtualization
Abstract Factory relies heavily on interface method dispatch (`invokeinterface`).
- Because a running application typically configures only **one active factory implementation** at runtime (e.g. `AwsResourceFactory`), the JVM C2 JIT compiler recognizes the calls as **monomorphic**.
- The JIT inlines the factory method calls directly into the client code, yielding identical performance to direct `new` allocation.

---

## 9. Comparison / When to Use What

| Feature | Factory Method | Abstract Factory | Builder |
| :--- | :--- | :--- | :--- |
| **Focus** | Creates 1 product | Creates families of related products | Creates 1 complex multi-step object |
| **Mechanism** | Subclassing / Supplier | Object composition | Step-by-step method chaining |
| **Complexity** | Low | Moderate to High | Low to Moderate |
| **Primary Advantage**| Decouples client from class | Prevents mixing incompatible products | Handles 10+ optional parameters |

---

## 10. Interview Questions & Answers

### Q1: What problem does the Abstract Factory pattern solve that Factory Method does not?
<details>
<summary>View Answer</summary>
Factory Method creates a single product type. Abstract Factory coordinates the creation of entire families of related products that are designed to be used together. It guarantees that client code cannot accidentally mix products from incompatible families (e.g. pairing an AWS storage bucket with an Azure compute worker), enforcing architectural consistency across product suites.
</details>

### Q2: What is the main drawback of the Abstract Factory pattern?
<details>
<summary>View Answer</summary>
The main drawback is that extending the factory to produce a new product type is difficult. Adding a new method (e.g. `createDatabase()`) to the `CloudResourceFactory` interface requires updating and recompiling every concrete factory implementation (`AwsResourceFactory`, `AzureResourceFactory`, `GcpResourceFactory`).
</details>

### Q3: How does the Open/Closed Principle apply to Abstract Factory?
<details>
<summary>View Answer</summary>
It is open for extending new product *families* (you can introduce a `GcpResourceFactory` without modifying existing code), but closed for extending new product *types* (adding a new product method requires modifying the interface and all subclasses).
</details>

### Q4: How is Abstract Factory commonly implemented with modern Dependency Injection (Spring)?
<details>
<summary>View Answer</summary>
In Spring Boot, the concrete factory is annotated with `@Component` and qualified by an active profile (e.g. `@Profile("aws") public class AwsResourceFactory implements CloudResourceFactory`). The application context automatically injects the active cloud factory into client services based on deployment configuration.
</details>

### Q5: Can an Abstract Factory be implemented using Java Records?
<details>
<summary>View Answer</summary>
Yes. While the factory itself is typically a service class or interface, the factory methods can return immutable Java records as products (e.g. `record AwsBucket(String name, String region) implements StorageBucket`).
</details>

### Q6: What is the relationship between Abstract Factory and the Singleton pattern?
<details>
<summary>View Answer</summary>
Concrete factory classes in an Abstract Factory system are frequently implemented as Singletons because an application only needs one instance of the factory to produce all family members.
</details>

### Q7: What is an example of Abstract Factory in the JDK?
<details>
<summary>View Answer</summary>
`javax.xml.parsers.DocumentBuilderFactory` is an abstract factory that produces XML document builders and parsers configured for secure processing.
</details>

### Q8: How can Java default interface methods mitigate the fragility of Abstract Factory?
<details>
<summary>View Answer</summary>
If a new product type is added to the abstract factory interface, declaring a default method (`default MessageQueue createQueue() { throw new UnsupportedOperationException(); }`) prevents breaking existing concrete factory subclasses that have not yet implemented the new product type.
</details>

---

## 11. Practice Exercises

### Exercise 1: Build a Multi-Platform UI Abstract Factory
**Task:** Create an abstract factory `UiFactory` with methods `Button createButton()` and `Menu createMenu()` for Windows and macOS.
<details>
<summary>View Solution</summary>

```java
public interface Button { void render(); }
public interface Menu { void render(); }

public interface UiFactory {
    Button createButton();
    Menu createMenu();
}

// Windows
public class WinButton implements Button { @Override public void render() { System.out.println("WinButton"); } }
public class WinMenu implements Menu { @Override public void render() { System.out.println("WinMenu"); } }
public class WinFactory implements UiFactory {
    @Override public Button createButton() { return new WinButton(); }
    @Override public Menu createMenu() { return new WinMenu(); }
}

// Mac
public class MacButton implements Button { @Override public void render() { System.out.println("MacButton"); } }
public class MacMenu implements Menu { @Override public void render() { System.out.println("MacMenu"); } }
public class MacFactory implements UiFactory {
    @Override public Button createButton() { return new MacButton(); }
    @Override public Menu createMenu() { return new MacMenu(); }
}
```
</details>

### Exercise 2: Abstract Factory Provider with Switch Expression
**Task:** Write a factory selector method that returns `WinFactory` or `MacFactory` based on an OS string enum.
<details>
<summary>View Solution</summary>

```java
public enum OsType { WINDOWS, MACOS }

public class PlatformSelector {
    public static UiFactory selectFactory(OsType os) {
        return switch (os) {
            case WINDOWS -> new WinFactory();
            case MACOS -> new MacFactory();
        };
    }
}
```
</details>

### Exercise 3: Cross-Database Toolset Factory
**Task:** Design a factory interface producing a `QueryBuilder` and a `TypeMapper` for PostgreSQL and Oracle.
<details>
<summary>View Solution</summary>

```java
public interface QueryBuilder { String select(String table); }
public interface TypeMapper { String mapType(String genericType); }

public interface DatabaseDialectFactory {
    QueryBuilder createQueryBuilder();
    TypeMapper createTypeMapper();
}

public class PostgresDialectFactory implements DatabaseDialectFactory {
    @Override public QueryBuilder createQueryBuilder() { return t -> "SELECT * FROM " + t + " LIMIT 10"; }
    @Override public TypeMapper createTypeMapper() { return t -> t.equals("BOOL") ? "BOOLEAN" : "TEXT"; }
}
```
</details>

---

## 12. Summary / Cheat Sheet

- **Core Goal**: Produce families of related products without coupling clients to concrete classes.
- **Consistency**: Guarantees incompatible products from different families cannot be mixed.
- **Factory Interface**: Declares one creation method per product in the family.
- **Concrete Factories**: Implement the factory to return products of a specific family.
- **JIT Inlining**: Monomorphic dispatch ensures abstract factory calls are inlined with zero overhead.
- **Tradeoff**: Adding new families is easy; adding new product types is difficult.

---

## 13. Further Reading
- GoF Design Patterns: [Abstract Factory](https://www.oreilly.com/library/view/design-patterns-elements/0201633612/)
- Refactoring Guru: [Abstract Factory Pattern](https://refactoring.guru/design-patterns/abstract-factory)
- Effective Java: [Item 1: Consider static factory methods instead of constructors](https://www.oreilly.com/library/view/effective-java-3rd/9780134686097/)
- Head First Design Patterns: [The Abstract Factory Pattern](https://www.oreilly.com/library/view/head-first-design/9781492077992/)
