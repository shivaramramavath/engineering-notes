# Factory Method Pattern in Modern Java

> Decoupling object creation from business logic: classic Factory Method, parameterized creators, static factory methods (Effective Java Item 1), and functional factory suppliers.

| Metadata | Details |
| :--- | :--- |
| **Level** | Core Design Pattern / Creational |
| **Prerequisites** | [00_solid-principles](../23-design-and-clean-code/00_solid-principles.md), [04_oop](../../04-oop/README.md) |
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

### What is the Factory Method Pattern?
The Factory Method is a creational design pattern that defines an interface or abstract method for creating an object, but allows subclasses to decide which class to instantiate.

In practical enterprise Java:
- It eliminates direct coupling between client business logic and concrete product classes (`new PostgresConnection()`, `new SmtpMailer()`).
- Clients depend strictly on high-level interfaces (`Connection`, `Mailer`).
- Instantiation decisions are deferred to dedicated creator subclasses or factory methods.

### Why Factory Method is Essential
Using raw `new` operators scattered across a codebase causes architectural rigidity:
- If `new EmailNotifier()` is called in 50 classes, switching to `SmsNotifier` or a mock test notifier requires editing 50 separate files.
- Constructors have significant limitations: they cannot have descriptive names, they must always return a new instance of the exact declaring class, and they cannot return existing cached instances.
- Factory methods resolve all constructor limitations while strictly adhering to the Open/Closed Principle.

---

## 2. Core Concepts

### GoF Factory Method vs. Static Factory Method
- **GoF Factory Method**: Uses inheritance. An abstract creator class defines an abstract `createProduct()` method; concrete creator subclasses override it to return specific product subclasses.
- **Static Factory Method (Effective Java Item 1)**: A static method on a class that returns an instance (e.g. `List.of()`, `Integer.valueOf()`, `LocalDate.now()`). Does not rely on subclassing.

### Modern Functional Factory Registration
In modern Java (Java 8+), the verbose GoF creator subclass hierarchy can be replaced by registering **Functional Suppliers** (`Map<Type, Supplier<Product>>`), combining the Open/Closed principle with zero boilerplate.

---

## 3. How It Works Internally

### Architectural Decoupling Flow
```
+--------------------------------------------------------------------------+
|                  TRADITIONAL DIRECT COUPLING (NO FACTORY)                |
|  [ OrderService ] -----> calls new PdfInvoice() ----> [ PdfInvoice ]     |
|  (OrderService is tightly coupled to concrete class PdfInvoice)          |
+--------------------------------------------------------------------------+
|                  FACTORY METHOD ARCHITECTURE                             |
|  [ OrderService ] -----> [ InvoiceFactory ]                              |
|                                |                                         |
|                                v returns                                 |
|                         [ Invoice (Interface) ]                          |
|                                ^         ^                               |
|                                |         |                               |
|                     [ PdfInvoice ]     [ HtmlInvoice ]                   |
+--------------------------------------------------------------------------+
```

---

## 4. Syntax & Usage

### 1. Classic GoF Factory Method
```java
// Product Interface
public interface Document {
    void open();
}

// Concrete Products
public class PdfDocument implements Document {
    @Override public void open() { System.out.println("Opening PDF"); }
}
public class TextDocument implements Document {
    @Override public void open() { System.out.println("Opening Text"); }
}

// Creator Hierarchy
public abstract class DocumentApp {
    // The Factory Method
    public abstract Document createDocument();

    public void openDocument() {
        Document doc = createDocument();
        doc.open();
    }
}
```

---

## 5. Practical Examples

### Example 1: Modern Functional Factory Registry (Open/Closed Compliant)
**Goal:** Implement a notification factory where new channels can be registered dynamically without modifying existing factory code.

```java
import java.util.Map;
import java.util.concurrent.ConcurrentHashMap;
import java.util.function.Supplier;

// Product Interface
public interface NotificationChannel {
    void send(String recipient, String message);
}

// Concrete Products
public class EmailChannel implements NotificationChannel {
    @Override public void send(String r, String m) { System.out.println("EMAIL to " + r + ": " + m); }
}

public class SmsChannel implements NotificationChannel {
    @Override public void send(String r, String m) { System.out.println("SMS to " + r + ": " + m); }
}

// Modern Functional Factory
public class NotificationFactory {
    private static final Map<String, Supplier<NotificationChannel>> REGISTRY = new ConcurrentHashMap<>();

    // Static initializer registers default channels
    static {
        registerChannel("EMAIL", EmailChannel::new);
        registerChannel("SMS", SmsChannel::new);
    }

    public static void registerChannel(String type, Supplier<NotificationChannel> supplier) {
        REGISTRY.put(type.toUpperCase(), supplier);
    }

    public static NotificationChannel createChannel(String type) {
        Supplier<NotificationChannel> supplier = REGISTRY.get(type.toUpperCase());
        if (supplier == null) {
            throw new IllegalArgumentException("Unknown notification channel: " + type);
        }
        return supplier.get();
    }
}
```

### Example 2: Static Factory Methods (Effective Java Item 1)
**Goal:** Replace confusing overloaded constructors with descriptive static factory methods.

```java
public final class Coordinate {
    private final double x;
    private final double y;

    // Private constructor forces usage of factory methods
    private Coordinate(double x, double y) {
        this.x = x;
        this.y = y;
    }

    // Descriptive factory method 1: Cartesian coordinates
    public static Coordinate fromCartesian(double x, double y) {
        return new Coordinate(x, y);
    }

    // Descriptive factory method 2: Polar coordinates
    public static Coordinate fromPolar(double radius, double angleRadians) {
        double x = radius * Math.cos(angleRadians);
        double y = radius * Math.sin(angleRadians);
        return new Coordinate(x, y);
    }
}
```

### Example 3: Edge Case — Instance Caching in Static Factory Methods
```java
public class BooleanFactory {
    // Static factories can return pre-allocated cached instances!
    public static Boolean valueOf(boolean b) {
        return b ? Boolean.TRUE : Boolean.FALSE; // Zero heap allocation!
    }
}
```

---

### Example 4: Dependency Injection Factory with ServiceLoader (SPI)
**Goal:** Decouple factory discovery using Java's standard `ServiceLoader` SPI mechanism.

```java
import java.util.Optional;
import java.util.ServiceLoader;

public class ServiceLoaderFactory {
    public static NotificationChannel loadDefaultChannel() {
        ServiceLoader<NotificationChannel> loader = ServiceLoader.load(NotificationChannel.class);
        Optional<NotificationChannel> channel = loader.findFirst();
        return channel.orElseThrow(() -> 
            new IllegalStateException("No NotificationChannel implementation found on module/classpath!"));
    }
}
```

## 6. Best Practices

1. **Consider Static Factory Methods First**: Before writing overloaded constructors, consider static factory methods with descriptive names (`of`, `from`, `valueOf`, `newInstance`).
2. **Program to Product Interfaces**: Factory methods must return the abstract interface type (`Notification`), never the concrete implementation class (`EmailNotification`).
3. **Use Method References for Clean Registration**: Pass constructor references (`EmailChannel::new`) to functional supplier maps.
4. **Make Constructors Private**: When providing static factory methods, lock down the constructor so clients cannot bypass the factory.
5. **Throw Informative Exceptions for Unknown Types**: Throw `IllegalArgumentException` with the unsupported type name when parameterized factories fail.
6. **Leverage Return-Type Subtyping**: Static factories can return any subtype of the declared return type (e.g. `Collections.unmodifiableList()` returns an internal private class).

---

## 7. Common Mistakes & Pitfalls

| Mistake | Why It Fails | Secure Fix |
| :--- | :--- | :--- |
| **Giant switch statements in factory** | Adding a new product requires editing and recompiling the factory class (violates OCP). | Use a functional registry map (`Map<Type, Supplier>`). |
| **Returning concrete classes instead of interfaces** | Leaks implementation details to caller; defeats polymorphism. | Return interface abstraction. |
| **Public constructors alongside factory methods** | Developers bypass factory and call `new` directly. | Make constructors `private` or `protected`. |
| **Creating an over-engineered factory for 1 implementation** | Violates YAGNI; adds cognitive overhead. | Use plain constructor until multiple variants exist. |
| **Returning null for unsupported factory types** | Triggers `NullPointerException` downstream. | Throw explicit `IllegalArgumentException`. |

---

## 8. Performance & Security Notes

### Escape Analysis and Object Reuse
Static factory methods can return cached instances (`Integer.valueOf(100)` returns from `IntegerCache`).
Constructors (`new Integer(100)`) are legally required to allocate a brand-new object on the heap on every call. Using factory methods reduces heap memory allocation and garbage collection pauses.

---

## 9. Comparison / When to Use What

| Creational Technique | Inheritance Needed | Custom Method Names | Instance Caching Possible | Best For |
| :--- | :--- | :--- | :--- | :--- |
| **Constructor** | No | No (Identical to class) | No (Always allocates) | Simple direct classes |
| **Static Factory Method**| No | Yes (`fromPolar`, `of`) | Yes | Value objects, Records |
| **GoF Factory Method** | Yes (Creator hierarchy) | Yes | Yes | Frameworks, extensible plugins |
| **Functional Supplier Map**| No | Yes | Yes | Dynamic plugin architectures |

---

## 10. Interview Questions & Answers

### Q1: What are the primary advantages of Static Factory Methods over Constructors according to Effective Java?
<details>
<summary>View Answer</summary>
1. They have descriptive names: Unlike constructors (which must match the class name), static factories can explain how the object is constructed (e.g. `BigInteger.probablePrime()`).
2. They are not required to create a new object each time: They can return pre-allocated cached instances (e.g. `Boolean.valueOf()`, `Integer.valueOf()`).
3. They can return any subtype of their declared return type: Hides implementation classes and allows returning different implementations based on input arguments.
4. The return type can vary from call to call as a function of the input parameters: (e.g. `EnumSet.noneOf()` returns `RegularEnumSet` for $\le 64$ elements, and `JumboEnumSet` otherwise).
</details>

### Q2: What is the main difference between Factory Method and Abstract Factory?
<details>
<summary>View Answer</summary>
- Factory Method: Relies on inheritance or a single method to create a **single product**. Subclasses override the factory method to change the instantiated product class.
- Abstract Factory: Relies on object composition. An Abstract Factory is an interface containing multiple factory methods designed to create a **family of related or dependent products** (e.g. `createButton()`, `createCheckbox()`, `createScrollbar()`) without specifying their concrete classes.
</details>

### Q3: How do modern Java Functional Interfaces simplify the Factory pattern?
<details>
<summary>View Answer</summary>
In classical Java, extending a factory required creating a new concrete subclass of an abstract factory. With `java.util.function.Supplier<T>`, the factory can maintain a `Map<String, Supplier<Product>>`. New product types are registered by simply adding a constructor reference (`factory.register("SMS", SmsChannel::new)`) without writing any creator boilerplate classes.
</details>

### Q4: What is an example of the Factory Method pattern in the Java Standard Library?
<details>
<summary>View Answer</summary>
`java.util.Collection.iterator()` is a classic GoF Factory Method. The abstract interface defines `Iterator<E> iterator()`. Each concrete collection (`ArrayList`, `LinkedList`, `HashSet`) implements this method to return its own specialized private iterator (`ArrayList.Itr`, `LinkedList.ListItr`). Client code calls `collection.iterator()` without knowing the concrete iterator class.
</details>

### Q5: Can a Factory Method be used with Java Records?
<details>
<summary>View Answer</summary>
Yes. Java records often feature static factory methods alongside canonical constructors. For example, a record `record Complex(double re, double im)` can provide `Complex.fromPolar(r, theta)` or `Complex.real(re)` to provide clear domain-specific instantiation semantics.
</details>

### Q6: Why should factories avoid returning `null` on unsupported arguments?
<details>
<summary>View Answer</summary>
Returning `null` shifts the burden of error detection to the caller. If the caller does not check for null, a `NullPointerException` is thrown far from the factory call. Throwing `IllegalArgumentException("Unsupported product type: " + type)` immediately flags the error at the exact point of failure.
</details>

### Q7: How does the Open/Closed Principle apply to Factory Method?
<details>
<summary>View Answer</summary>
The system is open for extension (new products can be introduced by creating new product classes and registering them) while remaining closed for modification (existing client business services calling the factory remain untouched).
</details>

### Q8: What is the disadvantage of Static Factory Methods?
<details>
<summary>View Answer</summary>
Classes without public or protected constructors cannot be subclassed (which is often an intentional advantage). Furthermore, static factory methods are not easily distinguishable from other static utility methods in Javadoc without standard naming conventions (`of`, `from`, `valueOf`, `getInstance`).
</details>

---

## 11. Practice Exercises

### Exercise 1: Build a Functional Shape Factory
**Task:** Create a shape factory using `Map<String, Supplier<Shape>>` supporting Circle and Square.
<details>
<summary>View Solution</summary>

```java
import java.util.Map;
import java.util.function.Supplier;

public interface Shape { void draw(); }
public record Circle() implements Shape { @Override public void draw() { System.out.println("Circle"); } }
public record Square() implements Shape { @Override public void draw() { System.out.println("Square"); } }

public class ShapeFactory {
    private static final Map<String, Supplier<Shape>> REGISTRY = Map.of(
        "CIRCLE", Circle::new,
        "SQUARE", Square::new
    );

    public static Shape create(String type) {
        Supplier<Shape> supplier = REGISTRY.get(type.toUpperCase());
        if (supplier == null) throw new IllegalArgumentException("Unknown shape: " + type);
        return supplier.get();
    }
}
```
</details>

### Exercise 2: Static Factory for Money Record
**Task:** Create a `Money` record with descriptive static factory methods: `usd(amount)` and `eur(amount)`.
<details>
<summary>View Solution</summary>

```java
import java.math.BigDecimal;
import java.util.Currency;

public record Money(BigDecimal amount, Currency currency) {
    public static Money usd(double amount) {
        return new Money(BigDecimal.valueOf(amount), Currency.getInstance("USD"));
    }
    public static Money eur(double amount) {
        return new Money(BigDecimal.valueOf(amount), Currency.getInstance("EUR"));
    }
}
```
</details>

### Exercise 3: Database Connection Driver Factory
**Task:** Implement a driver factory returning a mock Connection for H2, PostgreSQL, or MySQL.
<details>
<summary>View Solution</summary>

```java
public interface MockConnection { String getDriverName(); }

public class DriverFactory {
    public static MockConnection connect(String url) {
        if (url.startsWith("jdbc:postgresql:")) return () -> "PostgreSQL-Driver";
        if (url.startsWith("jdbc:h2:")) return () -> "H2-Driver";
        if (url.startsWith("jdbc:mysql:")) return () -> "MySQL-Driver";
        throw new IllegalArgumentException("Unsupported JDBC URL: " + url);
    }
}
```
</details>

---

## 12. Summary / Cheat Sheet

- **Core Goal**: Defer object instantiation away from consuming business services.
- **Return Type**: Always return product interfaces, never concrete classes.
- **Static Factories**: Prefer descriptive names (`of`, `from`, `valueOf`).
- **Functional Registration**: Use `Map<String, Supplier<Product>>` for OCP compliance.
- **Instance Reuse**: Static factories can return cached immutable objects.
- **Never Return Null**: Throw `IllegalArgumentException` on unknown types.

---

## 13. Further Reading
- Effective Java: [Item 1: Consider static factory methods instead of constructors](https://www.oreilly.com/library/view/effective-java-3rd/9780134686097/)
- GoF Design Patterns: [Factory Method](https://www.oreilly.com/library/view/design-patterns-elements/0201633612/)
- Refactoring Guru: [Factory Method Design Pattern](https://refactoring.guru/design-patterns/factory-method)
- Oracle Java Documentation: [Collections Class Static Factories](https://docs.oracle.com/en/java/javase/21/docs/api/java.base/java/util/Collections.html)
