# Interview Mastery: Design Patterns & SOLID Principles

> Architectural interview guide for software design patterns in modern Java: GoF patterns transformed by Lambdas and Sealed Types, Dynamic Proxies, Enum Singletons, and SOLID principle enforcement.

| Metadata | Value |
|---|---|
| **Level** | Advanced |
| **Prerequisites** | [01_oop.md](01_oop.md), [24-design-patterns](../24-design-patterns/README.md) |
| **Time to Read** | 35 minutes |
| **Status** | Complete |
| **Java Version** | Java 8 to Java 25 LTS |

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

Design patterns represent proven, battle-tested solutions to recurring software engineering problems. In technical interviews, interviewers test whether candidates understand **how patterns decouple systems, isolate volatility, and simplify maintenance**.

Senior engineers are expected to understand:
- How modern Java (Lambdas, Records, Sealed Types) **simplifies or eliminates** historical Gang of Four (GoF) boilerplate.
- The mechanics of **Dynamic Proxies (`java.lang.reflect.Proxy`)** that power Spring AOP, `@Transactional`, and Hibernate lazy loading.
- Why **Enum Singletons** provide bulletproof guarantees against serialization, reflection, and multi-threaded attacks.
- How to strictly enforce the **SOLID Principles** in enterprise systems.

---

## 2. Core Concepts

### 1. SOLID Principles
- **Single Responsibility Principle (SRP)**: A class should have one, and only one, reason to change.
- **Open-Closed Principle (OCP)**: Software entities should be open for extension, but closed for modification.
- **Liskov Substitution Principle (LSP)**: Subtypes must be substitutable for their base types without breaking client correctness.
- **Interface Segregation Principle (ISP)**: Clients should not be forced to depend on interfaces they do not use.
- **Dependency Inversion Principle (DIP)**: Depend upon abstractions, not concrete implementations.

### 2. The 3 GoF Pattern Classifications
1. **Creational**: Object instantiation mechanisms (Builder, Factory Method, Abstract Factory, Singleton, Prototype).
2. **Structural**: Class and object composition (Adapter, Decorator, Facade, Composite, Proxy).
3. **Behavioral**: Algorithms and assignment of responsibilities between objects (Strategy, Observer, Command, Chain of Responsibility, State).

### 3. Functional Modernization of GoF Patterns
- **Strategy Pattern** $\to$ Lambdas / Method References (`Predicate`, `Comparator`).
- **Command Pattern** $\to$ `Runnable` / `Consumer`.
- **Factory Method** $\to$ Static factory methods on interfaces (`List.of()`, `Set.of()`).
- **State Pattern** $\to$ Sealed Interface Hierarchies + Pattern Matching switch.

---

## 3. How It Works Internally

### Java Dynamic Proxy Architecture
The Dynamic Proxy API (`Proxy.newProxyInstance()`) generates synthetic bytecode at runtime:
```
Client Call: service.execute()
        |
        v
[Dynamic Proxy Class: $Proxy0 extends Proxy implements Service]
        |
        v (Interception)
[InvocationHandler.invoke(Object proxy, Method method, Object[] args)]
        |-- Pre-processing: Open Transaction / Start Timer / Log
        |-- Real Method: method.invoke(targetObject, args)
        +-- Post-processing: Commit Transaction / Clean up
```
This is the foundational architecture of Spring's `@Transactional`, Security interceptors, and RPC clients.

---

## 4. Syntax & Usage

### Modern Fluent Builder with Immutability

```java
import java.util.Objects;

public final class DatabaseConfig {
    private final String url;
    private final String username;
    private final int poolSize;
    private final boolean sslEnabled;

    private DatabaseConfig(Builder builder) {
        this.url = Objects.requireNonNull(builder.url, "url must not be null");
        this.username = Objects.requireNonNull(builder.username, "username must not be null");
        this.poolSize = builder.poolSize;
        this.sslEnabled = builder.sslEnabled;
    }

    public static class Builder {
        private String url;
        private String username;
        private int poolSize = 10; // Default
        private boolean sslEnabled = true;

        public Builder url(String url) { this.url = url; return this; }
        public Builder username(String username) { this.username = username; return this; }
        public Builder poolSize(int size) { this.poolSize = size; return this; }
        public Builder sslEnabled(boolean ssl) { this.sslEnabled = ssl; return this; }

        public DatabaseConfig build() {
            return new DatabaseConfig(this);
        }
    }
}
```

---

## 5. Practical Examples

### Example 1: The Bulletproof Enum Singleton (Effective Java Item 3)

```java
public enum ConnectionRegistry {
    INSTANCE; // Guaranteed by JVM to be instantiated exactly once

    private final String endpoint = "https://api.internal.network";

    public void broadcast(String message) {
        System.out.println("Broadcasting: " + message + " via " + endpoint);
    }

    // Immune to Reflection Attacks (Constructor.newInstance throws Cannot reflectively create enum)
    // Immune to Serialization Attacks (JVM handles enum serialization specially)
    // Immune to Multi-threading Race Conditions (Initialized during class loading)
}
```

### Example 2: Dynamic Proxy for Audit Logging & Execution Timing

```java
import java.lang.reflect.InvocationHandler;
import java.lang.reflect.Method;
import java.lang.reflect.Proxy;

public class DynamicProxyDemo {
    public interface OrderService {
        void placeOrder(String itemId, int quantity);
    }

    static class RealOrderService implements OrderService {
        @Override
        public void placeOrder(String itemId, int quantity) {
            System.out.printf("Order placed: %d of %s%n", quantity, itemId);
        }
    }

    static class TimingInvocationHandler implements InvocationHandler {
        private final Object target;
        public TimingInvocationHandler(Object target) { this.target = target; }

        @Override
        public Object invoke(Object proxy, Method method, Object[] args) throws Throwable {
            long start = System.nanoTime();
            try {
                return method.invoke(target, args);
            } finally {
                long durationMs = (System.nanoTime() - start) / 1_000_000;
                System.out.printf("[AUDIT] Method %s took %d ms%n", method.getName(), durationMs);
            }
        }
    }

    public static void main(String[] args) {
        OrderService real = new RealOrderService();
        OrderService proxy = (OrderService) Proxy.newProxyInstance(
            OrderService.class.getClassLoader(),
            new Class<?>[]{OrderService.class},
            new TimingInvocationHandler(real)
        );

        proxy.placeOrder("ITEM_99", 5);
    }
}
```

### Example 3: Strategy Pattern with Functional Lambdas

```java
import java.util.*;

public class ModernStrategyDemo {
    @FunctionalInterface
    public interface CompressionStrategy {
        byte[] compress(byte[] data);
    }

    // Registry of strategies using Lambdas
    private static final Map<String, CompressionStrategy> STRATEGIES = Map.of(
        "GZIP", data -> { System.out.println("Gzip compressing"); return data; },
        "SNAPPY", data -> { System.out.println("Snappy compressing"); return data; },
        "ZSTD", data -> { System.out.println("Zstd compressing"); return data; }
    );

    public static byte[] compress(String algorithm, byte[] data) {
        CompressionStrategy strategy = STRATEGIES.get(algorithm.toUpperCase());
        if (strategy == null) throw new IllegalArgumentException("Unknown algorithm: " + algorithm);
        return strategy.compress(data);
    }

    public static void main(String[] args) {
        compress("ZSTD", new byte[]{1, 2, 3});
    }
}
```

### Example 4: Chain of Responsibility for Request Filter Pipeline

```java
import java.util.*;

public class RequestPipeline {
    public record Request(String token, String payload, boolean isEncrypted) {}

    public interface Handler {
        boolean handle(Request req);
    }

    public static void main(String[] args) {
        List<Handler> pipeline = List.of(
            req -> {
                if (req.token() == null || req.token().isBlank()) {
                    System.err.println("Rejected: Missing token");
                    return false;
                }
                return true;
            },
            req -> {
                if (!req.isEncrypted()) {
                    System.err.println("Rejected: Unencrypted payload");
                    return false;
                }
                return true;
            },
            req -> {
                System.out.println("Processing valid payload: " + req.payload());
                return true;
            }
        );

        Request sample = new Request("valid_token", "order_data", true);
        for (Handler h : pipeline) {
            if (!h.handle(sample)) break;
        }
    }
}
```

### Example 5: State Pattern via Modern Sealed Types

```java
public class SealedStateDemo {
    public sealed interface OrderState permits Created, Paid, Shipped, Cancelled {}

    public record Created() implements OrderState {}
    public record Paid(String transactionId) implements OrderState {}
    public record Shipped(String trackingNumber) implements OrderState {}
    public record Cancelled(String reason) implements OrderState {}

    public static OrderState transition(OrderState current, String action) {
        return switch (current) {
            case Created c when action.equals("PAY")     -> new Paid("TXN_123");
            case Created c when action.equals("CANCEL")  -> new Cancelled("User cancelled");
            case Paid p when action.equals("SHIP")       -> new Shipped("TRACK_999");
            case Shipped s                               -> throw new IllegalStateException("Already shipped");
            case Cancelled c                             -> throw new IllegalStateException("Already cancelled");
            default                                      -> current;
        };
    }
}
```

---

## 6. Best Practices

1. **Use Enum for Singletons**: Always implement singletons using a single-element `enum`. It provides ironclad guarantees against reflection, serialization, and concurrency hazards.
2. **Favor Composition over Inheritance (Decorator/Adapter)**: Never subclass an implementation merely to add logging, caching, or monitoring; wrap it in a Decorator or Dynamic Proxy.
3. **Use Builder for Classes with $\ge 4$ Constructor Parameters**: Avoid telescoping constructors (`User(a)`, `User(a, b)`, `User(a, b, c)`) which invite subtle argument-order bugs.
4. **Replace Single-Method Strategy Classes with Lambdas**: Don't create 10 independent `.java` classes implementing a single-method interface; use lambda method references.

---

## 7. Common Mistakes & Pitfalls

| Mistake | Why It Fails | Fix |
|---|---|---|
| Lazy DCL Singleton without `volatile` | Reordering allows other threads to see partially constructed instance | Declare `instance` as `volatile` or use Enum. |
| Overusing the Singleton pattern | Creates hidden global state, tight coupling, and makes unit testing impossible | Use Dependency Injection (Spring/Guice) to manage lifecycles. |
| Violating LSP in inheritance hierarchies | Subclasses throw `UnsupportedOperationException` for inherited methods | Segregate interfaces or use composition. |
| Creating an Abstract Factory when a simple Factory Method suffices | Massive over-engineering with dozens of empty factory classes | Keep designs simple until multiple product families emerge. |
| Using Dynamic Proxy on concrete classes | Java `Proxy` works **strictly on interfaces**, not classes | Use ByteBuddy or CGLIB class-subclassing proxies. |

---

## 8. Performance & Security Notes

- **Dynamic Proxy Performance**: In modern HotSpot JVMs, Dynamic Proxy invocation costs are minimal (~10ns to 20ns) because the proxy bytecode is JIT-compiled into direct invocations after warmup.
- **Reflection Vulnerability in Standard Singletons**: Malicious callers can invoke `setAccessible(true)` on private constructors to create duplicate singleton instances. An `enum` constructor is hardcoded in the JVM specification to throw `IllegalArgumentException` on reflective instantiation.

---

## 9. Comparison / When to Use What

| Pattern | Category | Intent | Key Trade-Off |
|---|---|---|---|
| **Builder** | Creational | Construct complex immutable objects with optional fields | Adds builder class boilerplate |
| **Factory Method** | Creational | Delegate object creation to subclasses or static factories | Hides concrete constructors |
| **Adapter** | Structural | Convert an incompatible interface into an expected interface | Adds an indirection layer |
| **Decorator** | Structural | Dynamically add responsibilities to an object without subclassing | Can create many small wrapping objects |
| **Proxy** | Structural | Control or intercept access to another object (security, lazy load) | Reflection/AOP indirection |
| **Strategy** | Behavioral | Swap algorithms interchangeably at runtime | Increases strategy count |

---

## 10. Interview Questions

<details>
<summary>1. Why is an Enum considered the best way to implement a Singleton in Java?</summary>

Joshua Bloch famously proved in *Effective Java* that an `enum` with a single element (`INSTANCE`) is the most robust implementation because:
1. **Thread Safety**: Initialized during class loading under JVM synchronization guarantees.
2. **Serialization Safety**: Standard singletons require `readResolve()` and transient fields to avoid creating duplicate instances during deserialization. Enums are guaranteed by the JVM specification to never duplicate instances during deserialization.
3. **Reflection Immunity**: The JVM's `Constructor.newInstance()` explicitly checks `(clazz.getModifiers() & Modifier.ENUM) != 0` and throws `IllegalArgumentException: Cannot reflectively create enum objects`.
</details>

<details>
<summary>2. What is the difference between the Decorator Pattern and the Adapter Pattern?</summary>

- **Adapter Pattern**: Changes the interface of an existing object to match an incompatible target interface without altering behavior.
- **Decorator Pattern**: Keeps the **exact same interface**, but dynamically enriches or extends the behavior of the wrapped object (e.g., `new BufferedReader(new FileReader(...))`).
</details>

<details>
<summary>3. How does Spring implement AOP (Aspect-Oriented Programming) using Proxies?</summary>

Spring chooses between two proxy mechanisms at runtime:
1. **JDK Dynamic Proxies**: Used when the target class implements an interface. Creates a dynamic proxy implementing that interface via `java.lang.reflect.Proxy`.
2. **CGLIB / ByteBuddy Proxies**: Used when the target class does not implement an interface. Dynamically generates a subclass of the target class at runtime, overriding public methods to inject interceptors.
</details>

<details>
<summary>4. Explain the Dependency Inversion Principle (DIP) and how Dependency Injection (DI) relates to it.</summary>

- **Dependency Inversion Principle (DIP)** is a high-level architectural principle: high-level modules should depend on abstractions (interfaces), not concrete implementations.
- **Dependency Injection (DI)** is a concrete design pattern / mechanism used to satisfy DIP: instead of a service creating its own dependencies (`new DatabaseRepository()`), dependencies are injected from the outside via constructor injection or a framework container.
</details>

<details>
<summary>5. What is the difference between Abstract Factory and Factory Method?</summary>

- **Factory Method**: Uses inheritance. A method in a base class or interface delegates the creation of a **single object** to subclasses.
- **Abstract Factory**: Uses object composition. An interface declares a family of methods for creating a **family of related or dependent objects** without specifying their concrete classes (e.g. `GUIFactory` creates `Button`, `Checkbox`, `Window`).
</details>

<details>
<summary>6. When does the Singleton pattern become an Anti-Pattern?</summary>

When used as a disguised global variable. It introduces tight coupling across components, hides dependencies, makes unit testing and mocking nearly impossible, and creates state pollution across independent test runs.
</details>

<details>
<summary>7. How does the Observer Pattern relate to the Publisher-Subscriber Pattern?</summary>

- In **Observer**, the Subject maintains a direct collection of observers and notifies them synchronously. Subject and Observers are directly coupled.
- In **Publisher-Subscriber**, Publishers and Subscribers do not know each other; they communicate through a centralized Message Broker or Event Bus asynchronously (e.g. Kafka, RabbitMQ).
</details>

<details>
<summary>8. How do Java 17+ Sealed Types improve the Visitor Pattern?</summary>

The classic GoF Visitor Pattern requires double-dispatch (`element.accept(visitor)` and `visitor.visit(element)`) to work around Java's lack of pattern matching. With Sealed Types and Pattern Matching for switch, you can inspect and handle all concrete subtypes exhaustively in a clean, single-pass `switch (shape) { case Circle c -> ...; case Square s -> ...; }`, rendering the verbose Visitor pattern obsolete.
</details>

---

## 11. Practice Exercises

### Exercise 1: Implement an Extensible Plugin Architecture Using Factory Method
Design an extensible image processor using an enum or map factory.

<details>
<summary>Solution</summary>

```java
import java.util.*;

public class ImageProcessorFactory {
    public interface ImageFilter {
        void apply(byte[] pixels);
    }

    private static final Map<String, ImageFilter> REGISTRY = new HashMap<>();

    public static void register(String name, ImageFilter filter) {
        REGISTRY.put(name.toUpperCase(), filter);
    }

    public static ImageFilter getFilter(String name) {
        ImageFilter filter = REGISTRY.get(name.toUpperCase());
        if (filter == null) throw new IllegalArgumentException("Unknown filter: " + name);
        return filter;
    }

    static {
        register("BLUR", p -> System.out.println("Applying Gaussian blur"));
        register("GRAYSCALE", p -> System.out.println("Applying Grayscale"));
    }
}
```
</details>

### Exercise 2: Diagnose the Open-Closed Principle Violation
Refactor this monolithic notification class to obey OCP:
```java
class Notifier {
    void notify(String medium, String msg) {
        if (medium.equals("EMAIL")) sendEmail(msg);
        else if (medium.equals("SMS")) sendSms(msg);
        else if (medium.equals("SLACK")) sendSlack(msg);
    }
}
```

<details>
<summary>Solution</summary>

```java
import java.util.List;

public class OCPNotificationRefactor {
    public interface Channel {
        void send(String message);
    }

    public static class EmailChannel implements Channel {
        @Override public void send(String msg) { System.out.println("Email: " + msg); }
    }

    public static class SmsChannel implements Channel {
        @Override public void send(String msg) { System.out.println("SMS: " + msg); }
    }

    public static class NotificationService {
        private final List<Channel> channels;
        public NotificationService(List<Channel> channels) { this.channels = channels; }

        public void broadcast(String message) {
            channels.forEach(ch -> ch.send(message));
        }
    }
}
```
</details>

### Exercise 3: Implement an In-Memory Circuit Breaker Pattern
Implement a state machine transitioning between `CLOSED`, `OPEN`, and `HALF_OPEN`.

<details>
<summary>Solution</summary>

```java
import java.util.concurrent.Callable;

public class CircuitBreaker {
    public enum State { CLOSED, OPEN, HALF_OPEN }

    private State state = State.CLOSED;
    private int failureCount = 0;
    private final int failureThreshold = 3;
    private long lastStateChange = System.currentTimeMillis();
    private final long resetTimeoutMs = 5000;

    public synchronized <T> T execute(Callable<T> action) throws Exception {
        if (state == State.OPEN) {
            if (System.currentTimeMillis() - lastStateChange > resetTimeoutMs) {
                state = State.HALF_OPEN;
            } else {
                throw new RuntimeException("Circuit Breaker is OPEN; call rejected");
            }
        }

        try {
            T result = action.call();
            if (state == State.HALF_OPEN) {
                state = State.CLOSED;
                failureCount = 0;
            }
            return result;
        } catch (Exception e) {
            failureCount++;
            if (failureCount >= failureThreshold) {
                state = State.OPEN;
                lastStateChange = System.currentTimeMillis();
            }
            throw e;
        }
    }
}
```
</details>

---

## 12. Summary / Cheat Sheet

- **SOLID Principles**: Foundation of clean, maintainable, modular software architecture.
- **Enum Singleton**: The only 100% attack-proof singleton in Java (JVM guaranteed).
- **Dynamic Proxy**: Runtime interface interception via `InvocationHandler`.
- **Builder Pattern**: Best practice for immutable objects with $\ge 4$ constructor fields.
- **Modern Strategy**: Replace single-method strategy classes with functional lambdas.

---

## 13. Further Reading

- [Erich Gamma et al.: Design Patterns: Elements of Reusable Object-Oriented Software](https://en.wikipedia.org/wiki/Design_Patterns)
- [Joshua Bloch: Effective Java (3rd Edition)](https://www.oreilly.com/library/view/effective-java-3rd/9780134686097/)
- [Robert C. Martin: Agile Software Development, Principles, Patterns, and Practices](https://en.wikipedia.org/wiki/SOLID)
- [Baeldung: Guide to the Gang of Four Design Patterns in Java](https://www.baeldung.com/category/design-patterns)
