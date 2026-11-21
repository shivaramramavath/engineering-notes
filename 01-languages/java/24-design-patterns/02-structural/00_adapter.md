# Adapter Pattern in Modern Java

> Bridging incompatible interfaces cleanly: Class vs. Object Adapters, Two-Way Adapters, Java functional adapters (`Function`, `Consumer`), and real-world legacy modernization.

| Metadata | Details |
| :--- | :--- |
| **Level** | Core Design Pattern / Structural |
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

### What is the Adapter Pattern?
The Adapter pattern is a structural design pattern that converts the interface of a class into another interface that clients expect. It allows classes with incompatible interfaces to collaborate without altering existing source code.

In enterprise software engineering:
- It wraps legacy libraries or third-party SDKs that have awkward method names or data structures.
- It translates modern domain models to and from external SOAP/XML/JSON services.
- It acts as an **Anti-Corruption Layer (ACL)** in Domain-Driven Design (DDD), protecting internal domain purity from external vendor changes.

### Why Adapters are Vital
When integrating third-party billing providers (e.g. Stripe, PayPal, Adyen):
- Every provider defines completely different API signatures (`chargeCard()`, `makePayment()`, `submitTransaction()`).
- Directly calling vendor classes couples your entire application to that specific vendor SDK.
- An adapter wraps the vendor SDK inside your application\'s standardized `PaymentGateway` interface. Switching providers becomes a simple one-line configuration swap.

---

## 2. Core Concepts

### Target, Adaptee, and Adapter
1. **Target**: The domain interface that your application code knows and interacts with (`PaymentProcessor`).
2. **Adaptee**: The existing, incompatible class that provides the required functionality (`LegacyStripeSdk`).
3. **Adapter**: The bridge class that implements `Target` and wraps `Adaptee`, translating calls and mapping data types.

### Object Adapter vs. Class Adapter
- **Object Adapter (Composition)**: The adapter contains a private instance of the adaptee (`private final Adaptee adaptee`). Highly flexible; complies with "Composition over Inheritance".
- **Class Adapter (Inheritance)**: The adapter inherits from both the Target interface and the Adaptee class (`class Adapter extends Adaptee implements Target`). More rigid; less common in modern Java.

---

## 3. How It Works Internally

### Architectural Bridge Translation
```
+--------------------------------------------------------------------------+
|                  ADAPTER PATTERN ARCHITECTURE                            |
|                                                                          |
|  [ Client Code ]                                                         |
|         |                                                                |
|         | calls pay(Money)                                               |
|         v                                                                |
|  [ PaymentProcessor (Target Interface) ]                                 |
|         ^                                                                |
|         | implements                                                     |
|  [ StripePaymentAdapter (Adapter) ]                                      |
|         |                                                                |
|         | converts Money -> cents (long)                                 |
|         | calls chargeCustomer(long cents, String token)                 |
|         v                                                                |
|  [ StripeApiSdk (Adaptee / Third-Party) ]                                |
+--------------------------------------------------------------------------+
```

---

## 4. Syntax & Usage

### 1. Object Adapter Implementation
```java
// Target Domain Interface
public interface AudioPlayer {
    void play(String audioType, String fileName);
}

// Incompatible Adaptee
public class AdvancedMediaService {
    public void playVlc(String file) { System.out.println("VLC: " + file); }
    public void playMp4(String file) { System.out.println("MP4: " + file); }
}

// Object Adapter
public class MediaAdapter implements AudioPlayer {
    private final AdvancedMediaService advancedService;

    public MediaAdapter(AdvancedMediaService service) {
        this.advancedService = java.util.Objects.requireNonNull(service);
    }

    @Override
    public void play(String audioType, String fileName) {
        if ("vlc".equalsIgnoreCase(audioType)) {
            advancedService.playVlc(fileName);
        } else if ("mp4".equalsIgnoreCase(audioType)) {
            advancedService.playMp4(fileName);
        }
    }
}
```

---

## 5. Practical Examples

### Example 1: Anti-Corruption Layer (ACL) Payment Gateway Adapter
**Goal:** Adapt an external PayPal REST SDK to an internal domain `PaymentService` using immutable records.

```java
import java.math.BigDecimal;
import java.util.Objects;

// Internal Clean Domain Model
public record PaymentRequest(String accountId, BigDecimal amount, String currency) {}
public record PaymentResult(boolean successful, String transactionId) {}

public interface PaymentGateway {
    PaymentResult process(PaymentRequest request);
}

// External Incompatible Vendor SDK
public class PayPalSdkClient {
    public static class PayPalTransaction {
        public String authCode;
        public int status; // 200 = OK
    }

    public PayPalTransaction sendPayment(String email, double rawAmount, String curr) {
        PayPalTransaction tx = new PayPalTransaction();
        tx.authCode = "PP-" + System.currentTimeMillis();
        tx.status = 200;
        return tx;
    }
}

// The Production Adapter
public class PayPalPaymentAdapter implements PaymentGateway {
    private final PayPalSdkClient paypalClient;

    public PayPalPaymentAdapter(PayPalSdkClient client) {
        this.paypalClient = Objects.requireNonNull(client);
    }

    @Override
    public PaymentResult process(PaymentRequest request) {
        // Translation: Domain model to Vendor types
        String email = request.accountId();
        double amount = request.amount().doubleValue();
        String currency = request.currency();

        PayPalSdkClient.PayPalTransaction tx = paypalClient.sendPayment(email, amount, currency);

        // Translation: Vendor result back to Domain Model
        boolean success = (tx.status == 200);
        return new PaymentResult(success, tx.authCode);
    }
}
```

### Example 2: Modern Java Functional Adapters (Method References)
**Goal:** Adapt a legacy method signature to a modern Java `java.util.function.Consumer` or `Predicate` without writing a full class.

```java
import java.util.List;
import java.util.function.Consumer;

public class LegacyLogger {
    public void writeLog(String msg) {
        System.out.println("[LEGACY LOG] " + msg);
    }
}

public class FunctionalAdapterDemo {
    public static void main(String[] args) {
        LegacyLogger legacyLogger = new LegacyLogger();

        // Method reference adapts writeLog(String) directly into Consumer<String>!
        Consumer<String> modernConsumer = legacyLogger::writeLog;

        List<String> messages = List.of("Startup", "Transaction 101", "Shutdown");
        messages.forEach(modernConsumer); // Adapted cleanly with zero boilerplate!
    }
}
```

### Example 3: Two-Way Adapter
A Two-Way Adapter implements both the Target interface and the Adaptee interface simultaneously, allowing the same object to be used interchangeably by old and new clients.

---

### Example 4: Asynchronous Reactive Stream Adapter
**Goal:** Adapt a legacy callback-based listener into a modern `CompletableFuture` stream.

```java
import java.util.concurrent.CompletableFuture;

public interface CallbackListener {
    void onSuccess(String data);
    void onFailure(Throwable t);
}

public class LegacyAsyncService {
    public void execute(CallbackListener listener) {
        new Thread(() -> listener.onSuccess("Async result")).start();
    }
}

public class AsyncAdapter {
    public static CompletableFuture<String> adaptToFuture(LegacyAsyncService service) {
        CompletableFuture<String> future = new CompletableFuture<>();
        service.execute(new CallbackListener() {
            @Override public void onSuccess(String data) { future.complete(data); }
            @Override public void onFailure(Throwable t) { future.completeExceptionally(t); }
        });
        return future;
    }
}
```

## 6. Best Practices

1. **Favor Object Adapters Over Class Adapters**: Use composition (`private final Adaptee`) rather than subclassing the adaptee.
2. **Translate Exceptions in the Adapter**: Catch vendor-specific checked exceptions (`PayPalException`) and re-throw clean domain exceptions (`PaymentProcessingException`).
3. **Keep Adapters Focused**: Do not add business logic to an adapter; it should only translate data formats, method calls, and error codes.
4. **Use as Anti-Corruption Layers in Microservices**: Place adapters at the boundaries between your service and external APIs.
5. **Leverage Method References for SAM Interfaces**: Adapt single methods into `Function`, `Predicate`, or `Consumer` using `adaptee::method`.
6. **Defensively Validate Inputs**: Ensure the adapter validates domain parameters before delegating to third-party code.

---

## 7. Common Mistakes & Pitfalls

| Mistake | Why It Fails | Secure Fix |
| :--- | :--- | :--- |
| **Leaking vendor exceptions through adapter** | Callers become coupled to third-party exception classes. | Translate vendor exceptions to domain exceptions. |
| **Adding heavy business logic to adapter** | Violates Single Responsibility Principle. | Keep adapters pure translators; put business logic in services. |
| **Using inheritance (Class Adapter)** | Couples adapter to adaptee internals; cannot adapt interfaces. | Use object composition. |
| **Creating an adapter when direct call suffices** | Adds unnecessary indirection for stable internal libraries. | Only use adapters when interfaces are truly incompatible. |

---

## 8. Performance & Security Notes

### Microsecond Translation Overhead
Adapters introduce an extra stack frame call.
- In HotSpot, delegating adapter methods (`return adaptee.doWork(param)`) are primary candidates for JIT inlining.
- The C2 compiler inlines the adapter, compiling the entire translation pipeline into a single native assembly block with zero dispatch penalty.

---

## 9. Comparison / When to Use What

| Pattern | Intent | Relationship to Existing Code |
| :--- | :--- | :--- |
| **Adapter** | Converts one interface to another | Works with existing incompatible classes |
| **Decorator** | Adds responsibilities dynamically | Extends behavior without changing interface |
| **Facade** | Simplifies a complex subsystem | Provides a new simplified interface |
| **Proxy** | Controls access to an object | Implements the identical interface |

---

## 10. Interview Questions & Answers

### Q1: What is the difference between an Object Adapter and a Class Adapter?
<details>
<summary>View Answer</summary>
An Object Adapter relies on object composition: it implements the Target interface and maintains a private reference to an Adaptee instance, delegating calls to it. A Class Adapter relies on multiple inheritance: it extends the Adaptee class and implements the Target interface. Because Java does not support multiple class inheritance, Class Adapters can only adapt a class if the Target is an interface, making Object Adapters much more flexible and idiomatic.
</details>

### Q2: How does the Adapter pattern differ from the Decorator pattern?
<details>
<summary>View Answer</summary>
The Adapter pattern changes the interface of an existing object to make it compatible with a client that expects a different interface. The Decorator pattern retains the exact same interface as the wrapped object, enhancing or adding responsibilities dynamically without altering the interface contract.
</details>

### Q3: What is an Anti-Corruption Layer (ACL) in Domain-Driven Design?
<details>
<summary>View Answer</summary>
An Anti-Corruption Layer is an architectural application of the Adapter and Facade patterns. When a modern microservice interacts with a legacy system or external third-party vendor, the ACL translates foreign schemas, protocol formats, and error codes into the clean ubiquitous language of the local domain model. This prevents messy external concepts from leaking into and corrupting the core domain logic.
</details>

### Q4: Give an example of the Adapter pattern in the Java Standard Library.
<details>
<summary>View Answer</summary>
1. `java.util.Arrays.asList(T...)`: Adapts an array into a `List` interface.
2. `java.io.InputStreamReader(InputStream)`: Adapts an `InputStream` (byte-stream) into a `Reader` (character-stream).
</details>

### Q5: How can Java method references act as lightweight functional adapters?
<details>
<summary>View Answer</summary>
Because modern Java treats Single Abstract Method (SAM) interfaces as functional types, any method whose parameters and return type match a functional interface can be adapted directly via method reference. For example, `LegacyService::processItem` can be passed directly to a method expecting `Consumer<Item>` without creating an explicit adapter class.
</details>

### Q6: What should an adapter do when the adaptee throws an exception?
<details>
<summary>View Answer</summary>
The adapter should catch the adaptee\'s proprietary or checked exceptions, wrap or translate them into standard domain exceptions declared on the Target interface, and throw the domain exception. Leaking vendor-specific exceptions breaks abstraction and couples client code to third-party libraries.
</details>

### Q7: What is a Two-Way Adapter?
<details>
<summary>View Answer</summary>
A Two-Way Adapter implements both the Target interface and the Adaptee interface simultaneously. It can be passed to legacy clients expecting the Adaptee, and to modern clients expecting the Target, providing bidirectional interoperability during gradual system migrations.
</details>

### Q8: Does an Adapter always have a 1-to-1 method mapping?
<details>
<summary>View Answer</summary>
No. Often one target method call (`registerUser()`) maps to multiple adaptee calls (`createUser()`, `assignRole()`, `sendEmail()`), or multiple target arguments must be combined into a single vendor DTO.
</details>

---

## 11. Practice Exercises

### Exercise 1: Build a Legacy Enumeration to Modern Iterator Adapter
**Task:** Write an adapter `EnumerationIterator<T>` that adapts legacy `java.util.Enumeration<T>` to modern `java.util.Iterator<T>`.
<details>
<summary>View Solution</summary>

```java
import java.util.Enumeration;
import java.util.Iterator;
import java.util.Objects;

public class EnumerationIterator<T> implements Iterator<T> {
    private final Enumeration<T> enumeration;

    public EnumerationIterator(Enumeration<T> enumeration) {
        this.enumeration = Objects.requireNonNull(enumeration);
    }

    @Override
    public boolean hasNext() {
        return enumeration.hasMoreElements();
    }

    @Override
    public T next() {
        return enumeration.nextElement();
    }
}
```
</details>

### Exercise 2: Temperature Unit Adapter
**Task:** Adapt a weather sensor measuring Fahrenheit to a domain interface expecting Celsius.
<details>
<summary>View Solution</summary>

```java
public interface CelsiusSensor { double getTemperatureCelsius(); }
public class FahrenheitSensor { public double readFahrenheit() { return 98.6; } }

public class SensorAdapter implements CelsiusSensor {
    private final FahrenheitSensor fSensor;
    public SensorAdapter(FahrenheitSensor s) { this.fSensor = s; }

    @Override
    public double getTemperatureCelsius() {
        return (fSensor.readFahrenheit() - 32) * 5.0 / 9.0;
    }
}
```
</details>

### Exercise 3: Runnable Adapter for Callable
**Task:** Adapt a `Runnable` into a `Callable<Void>`.
<details>
<summary>View Solution</summary>

```java
import java.util.concurrent.Callable;

public class RunnableAdapter {
    public static Callable<Void> toCallable(Runnable runnable) {
        return () -> {
            runnable.run();
            return null;
        };
    }
}
```
</details>

---

## 12. Summary / Cheat Sheet

- **Core Goal**: Convert an incompatible interface into an interface expected by clients.
- **Object Adapter**: Implement Target interface; wrap Adaptee instance via composition.
- **Exception Safety**: Translate third-party vendor exceptions into clean domain exceptions.
- **Functional Adapters**: Use method references (`service::action`) to satisfy SAM interfaces.
- **Zero Indirection Cost**: HotSpot C2 compiler inlines delegation calls in compiled native code.
- **Anti-Corruption Layer**: Protect domain models from vendor changes in microservices.

---

## 13. Further Reading
- GoF Design Patterns: [Adapter Pattern](https://www.oreilly.com/library/view/design-patterns-elements/0201633612/)
- Refactoring Guru: [Adapter Design Pattern](https://refactoring.guru/design-patterns/adapter)
- Martin Fowler: [Anti-Corruption Layer Pattern](https://martinfowler.com/bliki/AntiCorruptionLayer.html)
- Effective Java: [Item 18: Favor composition over inheritance](https://www.oreilly.com/library/view/effective-java-3rd/9780134686097/)
