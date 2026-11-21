# Decorator Pattern in Modern Java

> Dynamically attaching behaviors and responsibilities to objects: Open/Closed Principle adherence, wrapping strategies, Java I/O streams, and functional decorator chaining.

| Metadata | Details |
| :--- | :--- |
| **Level** | Core Design Pattern / Structural |
| **Prerequisites** | [00_solid-principles](../23-design-and-clean-code/00_solid-principles.md), [11_io](../../11-io-and-networking/README.md) |
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

### What is the Decorator Pattern?
The Decorator pattern is a structural design pattern that allows behavior to be added to an individual object, dynamically and transparently, without affecting the behavior of other objects from the same class.

Instead of creating hundreds of subclasses to represent every possible combination of features (e.g. `EncryptedCompressedLoggingStream`), decorators wrap the base object in layers:
- Base Object: `FileStream`
- Decorator 1: `BufferedStream(FileStream)`
- Decorator 2: `GzipStream(BufferedStream(FileStream))`
- Decorator 3: `CryptoStream(GzipStream(BufferedStream(FileStream)))`

### Why Decorators Prevent Class Explosion
If a system has 5 optional independent features, supporting all combinations using static inheritance requires $2^5 = 32$ discrete subclasses.
Using the Decorator pattern requires only **5 decorator classes** that can be stacked arbitrarily at runtime.

---

## 2. Core Concepts

### Component, Concrete Component, and Decorator
1. **Component**: The shared interface for both base objects and decorators (`DataSource`).
2. **Concrete Component**: The base class defining basic behavior (`FileDataSource`).
3. **Base Decorator**: An abstract class that implements `Component` and wraps a `Component` reference.
4. **Concrete Decorators**: Classes overriding component methods to add specific behaviors (e.g. `EncryptionDecorator`, `CompressionDecorator`) before or after delegating.

---

## 3. How It Works Internally

### Recursive Execution Pipeline
```
[ Client Call: dataSource.writeData(bytes) ]
                  |
                  v
[ EncryptionDecorator ]
  1. Encrypts payload
  2. Calls wrapped.writeData(encryptedBytes)
         |
         v
[ CompressionDecorator ]
  1. Compresses payload
  2. Calls wrapped.writeData(compressedBytes)
         |
         v
[ FileDataSource (Base Concrete Component) ]
  1. Writes raw bytes to physical disk!
```

---

## 4. Syntax & Usage

### 1. Classic Decorator Structure
```java
// Component Interface
public interface Notifier {
    void send(String message);
}

// Concrete Component
public class EmailNotifier implements Notifier {
    @Override public void send(String msg) { System.out.println("Email: " + msg); }
}

// Base Decorator
public abstract class NotifierDecorator implements Notifier {
    protected final Notifier wrapped;
    public NotifierDecorator(Notifier wrapped) { this.wrapped = java.util.Objects.requireNonNull(wrapped); }
    @Override public void send(String msg) { wrapped.send(msg); }
}

// Concrete Decorator
public class SmsDecorator extends NotifierDecorator {
    public SmsDecorator(Notifier wrapped) { super(wrapped); }
    @Override public void send(String msg) {
        super.send(msg);
        System.out.println("SMS: " + msg);
    }
}
```

---

## 5. Practical Examples

### Example 1: Resilient HTTP Client Decorators (Metrics, Logging, Retries)
**Goal:** Stack metrics, structured logging, and retries onto an HTTP client dynamically without modifying the client.

```java
import java.util.Objects;

public interface HttpClient {
    String get(String url);
}

// Concrete Base Component
public class DefaultHttpClient implements HttpClient {
    @Override
    public String get(String url) {
        return "{"status":"ok","url":"" + url + ""}";
    }
}

// Decorator 1: Logging
public class LoggingHttpDecorator implements HttpClient {
    private final HttpClient delegate;

    public LoggingHttpDecorator(HttpClient delegate) {
        this.delegate = Objects.requireNonNull(delegate);
    }

    @Override
    public String get(String url) {
        System.out.println("LOG: Initiating GET " + url);
        String response = delegate.get(url);
        System.out.println("LOG: Received response for " + url);
        return response;
    }
}

// Decorator 2: Execution Timer
public class MetricsHttpDecorator implements HttpClient {
    private final HttpClient delegate;

    public MetricsHttpDecorator(HttpClient delegate) {
        this.delegate = Objects.requireNonNull(delegate);
    }

    @Override
    public String get(String url) {
        long start = System.nanoTime();
        try {
            return delegate.get(url);
        } finally {
            long duration = (System.nanoTime() - start) / 1_000_000;
            System.out.println("METRICS: GET " + url + " took " + duration + " ms");
        }
    }
}

// Composition at Runtime:
// HttpClient client = new MetricsHttpDecorator(new LoggingHttpDecorator(new DefaultHttpClient()));
```

### Example 2: Modern Functional Decorator Chaining with `Function`
**Goal:** In modern Java, functional interfaces can be decorated using `.andThen()` and `.compose()`.

```java
import java.util.function.UnaryOperator;

public class FunctionalDecorators {
    public static void main(String[] args) {
        UnaryOperator<String> baseProcessor = text -> text.trim();
        UnaryOperator<String> upperCaseDecorator = text -> text.toUpperCase();
        UnaryOperator<String> prefixDecorator = text -> "[AUDIT] " + text;

        // Chain decorators functionally
        UnaryOperator<String> pipeline = baseProcessor
                .andThen(upperCaseDecorator)
                .andThen(prefixDecorator)::apply;

        String result = pipeline.apply("   user logged in   ");
        System.out.println(result); // Prints: [AUDIT] USER LOGGED IN
    }
}
```

---

### Example 3: Thread-Safe Concurrent Decorator with Synchronized Locking
**Goal:** Wrap an unsynchronized data store in a thread-safe decorator layer (equivalent to `Collections.synchronizedList`).

```java
import java.util.Objects;

public interface KeyValueStore {
    void put(String key, String val);
    String get(String key);
}

public class SynchronizedStoreDecorator implements KeyValueStore {
    private final KeyValueStore delegate;
    private final Object lock = new Object();

    public SynchronizedStoreDecorator(KeyValueStore delegate) {
        this.delegate = Objects.requireNonNull(delegate);
    }

    @Override
    public void put(String key, String val) {
        synchronized (lock) {
            delegate.put(key, val);
        }
    }

    @Override
    public String get(String key) {
        synchronized (lock) {
            return delegate.get(key);
        }
    }
}
```

## 6. Best Practices

1. **Keep Component Interface Lightweight**: Avoid putting dozens of methods in the Component interface; decorators must implement all of them.
2. **Ensure Decorators Are Transparent**: Decorators must adhere strictly to the Liskov Substitution Principle. Clients should not know whether they are speaking to a base component or a 5-layer decorator.
3. **Use Functional Composition for Simple Transformations**: Use `Function.andThen()` for lightweight single-method chains.
4. **Order Matters**: Be mindful of decorator wrapping order (e.g. `Encrypt(Compress(Data))` vs `Compress(Encrypt(Data))`—encrypted data cannot be compressed effectively!).
5. **Keep Decorators Stateless Where Possible**: Stateless decorators can be cached or shared safely across threads.

---

## 7. Common Mistakes & Pitfalls

| Mistake | Why It Fails | Secure Fix |
| :--- | :--- | :--- |
| **Compressing after Encrypting** | High-entropy encrypted ciphertexts have zero redundancy; compression fails. | Always compress *before* encrypting. |
| **Breaking the Component Contract** | Decorator throws unexpected exceptions not declared on component. | Adhere to component interface contracts. |
| **Modifying interface signatures** | Adding decorator-specific methods that clients cannot access via Component reference. | Keep all capabilities behind Component methods. |
| **Deep nesting making debugging hard** | 10 layers of decorators make stack traces 50 frames deep. | Balance dynamic decorators with explicit pipelines. |

---

## 8. Performance & Security Notes

### Inlining Layered Method Calls
While nesting 4 decorators creates 4 method calls, the JVM C2 JIT compiler aggressively inlines short delegation methods.
- The chained calls `dec1 -> dec2 -> dec3 -> base` are flattened by the JIT compiler into a single contiguous block of compiled machine code.

---

## 9. Comparison / When to Use What

| Pattern | Purpose | Interface State |
| :--- | :--- | :--- |
| **Decorator** | Adds responsibilities dynamically | Retains identical interface |
| **Adapter** | Converts incompatible interfaces | Changes interface |
| **Proxy** | Controls lifecycle or access | Retains identical interface, but controls access |
| **Strategy** | Swaps internal algorithm | Changes internal engine |

---

## 10. Interview Questions & Answers

### Q1: What is the classic example of the Decorator pattern in the Java Standard Library?
<details>
<summary>View Answer</summary>
The `java.io` package is the canonical example. `InputStream` is the component. `FileInputStream` is the concrete component. `FilterInputStream` is the base decorator. `BufferedInputStream`, `GZIPInputStream`, `DataInputStream`, and `CipherInputStream` are concrete decorators that can be chained arbitrarily: `new GZIPInputStream(new BufferedInputStream(new FileInputStream("data.gz")))`.
</details>

### Q2: How does the Decorator pattern adhere to the Open/Closed Principle?
<details>
<summary>View Answer</summary>
Existing base classes and existing decorators are closed for modification. New responsibilities (such as adding caching, encryption, or auditing) are added by creating new decorator classes that wrap existing components, extending behavior without touching existing code.
</details>

### Q3: Why is Decorator preferred over Inheritance for combining optional features?
<details>
<summary>View Answer</summary>
Inheritance binds behavior at compile time and requires a separate subclass for every permutation of features ($2^N$ class explosion). Decorator combines behaviors dynamically at runtime using object composition, requiring only $N$ decorator classes.
</details>

### Q4: How does wrapping order affect decorators?
<details>
<summary>View Answer</summary>
Because decorators execute in a nested onion-skin order, the sequence of wrapping dictates execution flow. For example, in stream processing, `Encrypt(Compress(data))` ensures data is compressed while redundant, then encrypted. Reversing the order produces inflated ciphertexts.
</details>

### Q5: What is the difference between a Decorator and a Proxy?
<details>
<summary>View Answer</summary>
While both wrap another object and implement the same interface:
- **Decorator** focuses on adding new behaviors or responsibilities dynamically. The client typically creates the base object and wraps it explicitly.
- **Proxy** focuses on controlling access to the underlying object (lazy loading, caching, security checks, remote RPC). The proxy usually manages the lifecycle of the underlying service itself.
</details>

### Q6: Can Java Records be used as Decorators?
<details>
<summary>View Answer</summary>
Yes! A Java record can implement an interface and hold a delegate component as one of its immutable components (e.g. `record TimedProcessor(Processor delegate) implements Processor`).
</details>

### Q7: What is the downside of using heavy Decorator stacks?
<details>
<summary>View Answer</summary>
Deeply nested decorator stacks make object initialization verbose and result in long stack traces during exceptions, making debugging more challenging if an error occurs deep in the call chain.
</details>

### Q8: How can Java default methods in interfaces be used with Decorators?
<details>
<summary>View Answer</summary>
Default methods in interfaces allow adding non-core utility methods to the Component interface without requiring every concrete decorator class to implement them manually.
</details>

---

## 11. Practice Exercises

### Exercise 1: Build a Text Formatter Decorator
**Task:** Create an interface `TextFormatter` with `format(String)`, and decorators for `BoldDecorator` (`**text**`) and `ItalicDecorator` (`_text_`).
<details>
<summary>View Solution</summary>

```java
public interface TextFormatter { String format(String text); }
public class PlainFormatter implements TextFormatter { @Override public String format(String t) { return t; } }

public record BoldDecorator(TextFormatter delegate) implements TextFormatter {
    @Override public String format(String t) { return "**" + delegate.format(t) + "**"; }
}

public record ItalicDecorator(TextFormatter delegate) implements TextFormatter {
    @Override public String format(String t) { return "_" + delegate.format(t) + "_"; }
}
```
</details>

### Exercise 2: Rate Limiting Decorator
**Task:** Build a decorator that enforces a simple execution limit on a `TaskRunner`.
<details>
<summary>View Solution</summary>

```java
public interface TaskRunner { void runTask(); }

public class RateLimitedRunner implements TaskRunner {
    private final TaskRunner delegate;
    private int executions = 0;

    public RateLimitedRunner(TaskRunner delegate) { this.delegate = delegate; }

    @Override
    public synchronized void runTask() {
        if (executions >= 5) throw new IllegalStateException("Rate limit exceeded!");
        executions++;
        delegate.runTask();
    }
}
```
</details>

### Exercise 3: Caching Decorator
**Task:** Decorate an expensive calculator with in-memory memoization.
<details>
<summary>View Solution</summary>

```java
import java.util.Map;
import java.util.concurrent.ConcurrentHashMap;

public interface MathService { long factorial(int n); }

public class CachingMathDecorator implements MathService {
    private final MathService delegate;
    private final Map<Integer, Long> cache = new ConcurrentHashMap<>();

    public CachingMathDecorator(MathService delegate) { this.delegate = delegate; }

    @Override
    public long factorial(int n) {
        return cache.computeIfAbsent(n, delegate::factorial);
    }
}
```
</details>

---

## 12. Summary / Cheat Sheet

- **Core Goal**: Dynamically attach new behaviors to objects without subclassing.
- **Identical Interface**: Decorators implement the exact same interface as the wrapped object.
- **Composition over Inheritance**: Prevents $2^N$ class explosion by stacking decorators.
- **Wrapping Order**: Decorators execute outside-in; sequence is critical.
- **JDK Example**: `BufferedInputStream(GZIPInputStream(FileInputStream))`.
- **Zero Overhead**: HotSpot C2 compiler inlines decorator chains in compiled code.

---

## 13. Further Reading
- GoF Design Patterns: [Decorator Pattern](https://www.oreilly.com/library/view/design-patterns-elements/0201633612/)
- Refactoring Guru: [Decorator Design Pattern](https://refactoring.guru/design-patterns/decorator)
- Effective Java: [Item 18: Favor composition over inheritance](https://www.oreilly.com/library/view/effective-java-3rd/9780134686097/)
