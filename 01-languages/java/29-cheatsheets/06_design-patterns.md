# Java Design Patterns Reference Cheat Sheet

> Complete reference for all 23 Gang of Four (GoF) patterns reimagined in Modern Java: creational, structural, and behavioral patterns utilizing lambdas, records, sealed types, and pattern matching.

| Metadata | Details |
| :--- | :--- |
| **Level** | Comprehensive Reference |
| **Prerequisites** | [01_oop](../01_oop/README.md), [00_core-language.md](./00_core-language.md) |
| **Time to Read** | 45 minutes |
| **Status** | Complete |
| **Java Version** | Java 8 to 25 LTS |

---

## Table of Contents
1. [Pattern Classification Matrix](#1-pattern-classification-matrix)
2. [Creational Patterns (Modern Idioms)](#2-creational-patterns-modern-idioms)
3. [Structural Patterns (Modern Idioms)](#3-structural-patterns-modern-idioms)
4. [Behavioral Patterns (Modern Idioms)](#4-behavioral-patterns-modern-idioms)
5. [Modern Java Pattern Replacements (Functional Paradigm)](#5-modern-java-pattern-replacements-functional-paradigm)
6. [JDK Built-In Pattern Implementations](#6-jdk-built-in-pattern-implementations)
7. [Design Patterns Anti-Patterns & Misuse](#7-design-patterns-anti-patterns--misuse)
8. [Summary & Quick Selection Matrix](#8-summary--quick-selection-matrix)

---

## 1. Pattern Classification Matrix

| Category | Pattern | Core Intent | Modern Java Implementation |
| :--- | :--- | :--- | :--- |
| **Creational** | **Singleton** | Ensures class has only one instance | `enum` Singleton or Bill Pugh Holder |
| | **Factory Method** | Subclasses decide which class to instantiate | `Map<Type, Supplier<Product>>` |
| | **Abstract Factory** | Families of related/dependent objects | Record-based factory interfaces |
| | **Builder** | Step-by-step construction of complex objects | Fluent builder or Record with default methods |
| | **Prototype** | Clone existing instances without class coupling | Copy constructors or `.clone()` |
| **Structural** | **Adapter** | Converts interface of a class to another expected | Single abstract method wrapper / Lambdas |
| | **Bridge** | Decouples an abstraction from its implementation | Constructor injection of implementation interface |
| | **Composite** | Treats individual objects and compositions uniformly | Sealed interface with Record leaves and branches |
| | **Decorator** | Dynamically attaches responsibilities to objects | Function composition (`Function.andThen()`) |
| | **Facade** | Simplified high-level interface to a complex subsystem | Unified orchestrator service |
| | **Flyweight** | Shares fine-grained object state to minimize memory | Wrapper caches (`Integer.valueOf()`) |
| | **Proxy** | Provides a surrogate or placeholder to control access | `java.lang.reflect.Proxy` or Virtual Thread proxy |
| **Behavioral** | **Chain of Responsibility** | Passes request along a chain of handlers | `UnaryOperator<Request>` pipeline |
| | **Command** | Encapsulates request as an object | `Runnable` / `Callable` / Lambdas |
| | **Interpreter** | Evaluates sentences in a formal grammar | Sealed record grammar + Pattern Matching switch |
| | **Iterator** | Sequential access without exposing representation | `Iterable<T>` / `Spliterator<T>` |
| | **Mediator** | Encapsulates how objects interact | Event Bus / Reactor |
| | **Memento** | Captures and restores an object's internal state | Immutable Record state snapshots |
| | **Observer** | One-to-many dependency notification | `PropertyChangeListener` / Reactive Flow API |
| | **State** | Alters object behavior when internal state changes | Sealed interface state machine + pattern switch |
| | **Strategy** | Family of interchangeable algorithms | First-class lambda functions (`Predicate`, `Function`) |
| | **Template Method** | Invariant algorithm skeleton with deferred steps | Higher-order function callbacks |
| | **Visitor** | Adds operations to object structures without changing them | Pattern Matching over Sealed Types |

---

## 2. Creational Patterns (Modern Idioms)

### 1. Modern Builder Pattern (Fluent API)
```java
public record UserProfile(String username, String email, int age, boolean active) {
    public static Builder builder() { return new Builder(); }

    public static class Builder {
        private String username;
        private String email;
        private int age;
        private boolean active = true;

        public Builder username(String u) { this.username = u; return this; }
        public Builder email(String e) { this.email = e; return this; }
        public Builder age(int a) { this.age = a; return this; }
        public Builder active(boolean a) { this.active = a; return this; }
        public UserProfile build() {
            return new UserProfile(username, email, age, active);
        }
    }
}
```

### 2. Functional Factory Method
```java
public class NotificationFactory {
    private static final Map<String, Supplier<NotificationService>> REGISTRY = Map.of(
            "EMAIL", EmailNotification::new,
            "SMS", SmsNotification::new,
            "PUSH", PushNotification::new
    );

    public static NotificationService getService(String type) {
        Supplier<NotificationService> supplier = REGISTRY.get(type.toUpperCase());
        if (supplier == null) throw new IllegalArgumentException("Unknown type: " + type);
        return supplier.get();
    }
}
```

---

## 3. Structural Patterns (Modern Idioms)

### 1. Modern Decorator using Function Composition
```java
import java.util.function.UnaryOperator;

public class FunctionalDecorator {
    public static void main(String[] args) {
        UnaryOperator<String> baseFormatter = String::trim;
        UnaryOperator<String> upperDecorator = String::toUpperCase;
        UnaryOperator<String> quoteDecorator = s -> """ + s + """;

        // Chaining decorators using andThen
        UnaryOperator<String> pipeline = baseFormatter
                .andThen(upperDecorator)
                .andThen(quoteDecorator);

        System.out.println(pipeline.apply("   modern java   ")); // ""MODERN JAVA""
    }
}
```

### 2. Composite Pattern via Sealed Types (Java 17+)
```java
public sealed interface FileSystemItem permits FileItem, DirectoryItem {
    long getSize();
}

public record FileItem(String name, long size) implements FileSystemItem {
    @Override public long getSize() { return size; }
}

public record DirectoryItem(String name, List<FileSystemItem> children) implements FileSystemItem {
    @Override
    public long getSize() {
        return children.stream().mapToLong(FileSystemItem::getSize).sum();
    }
}
```

---

## 4. Behavioral Patterns (Modern Idioms)

### 1. Modern Strategy Pattern via Lambdas
```java
public class StrategyDemo {
    @FunctionalInterface
    public interface PaymentStrategy {
        void pay(double amount);
    }

    public static void executePayment(double amount, PaymentStrategy strategy) {
        strategy.pay(amount);
    }

    public static void main(String[] args) {
        // Strategy passed directly as lambda
        executePayment(150.0, amt -> System.out.println("Paid via Credit Card: $" + amt));
        executePayment(80.0, amt -> System.out.println("Paid via PayPal: $" + amt));
    }
}
```

### 2. Modern Visitor Pattern via Pattern Matching (Java 21+)
In classic GoF, Visitor required verbose `accept(Visitor v)` double-dispatch methods. In Modern Java, **Pattern Matching over Sealed Types completely obsoletes classic Visitor boilerplate**:

```java
public sealed interface Expression permits Constant, Add, Multiply {}
public record Constant(int value) implements Expression {}
public record Add(Expression left, Expression right) implements Expression {}
public record Multiply(Expression left, Expression right) implements Expression {}

public class ExpressionEvaluator {
    // Visitor operation implemented cleanly without touching data models
    public static int evaluate(Expression expr) {
        return switch (expr) {
            case Constant(int val) -> val;
            case Add(Expression l, Expression r) -> evaluate(l) + evaluate(r);
            case Multiply(Expression l, Expression r) -> evaluate(l) * evaluate(r);
        };
    }
}
```

---

## 5. Modern Java Pattern Replacements (Functional Paradigm)

| Classic GoF Pattern | Modern Java Replacement | Replaces Boilerplate |
| :--- | :--- | :--- |
| **Strategy** | `java.util.function.Function` / `Predicate` | Dedicated strategy class hierarchy |
| **Command** | `Runnable` / `Callable` / `Consumer` | Command interfaces and concrete command classes |
| **Template Method** | Method accepting functional callback | Abstract base classes with hook methods |
| **Visitor** | Sealed Types + Pattern Matching Switch | `Visitor` interface, `Visitable`, `accept()` methods |
| **Observer** | `CompletableFuture` / Reactive Streams | Subject/Observer interface registries |
| **Singleton** | Single-element `enum` | Double-checked locking and synchronization |

---

## 6. JDK Built-In Pattern Implementations

- **Singleton:** `java.lang.Runtime.getRuntime()`
- **Factory Method:** `java.util.Calendar.getInstance()`, `List.of()`
- **Builder:** `java.lang.StringBuilder.append()`, `java.net.http.HttpRequest.newBuilder()`
- **Prototype:** `java.lang.Object.clone()`
- **Adapter:** `java.util.Arrays.asList()`, `java.io.InputStreamReader(InputStream)`
- **Decorator:** `java.io.BufferedInputStream(new FileInputStream(...))`
- **Proxy:** `java.lang.reflect.Proxy`
- **Chain of Responsibility:** `jakarta.servlet.FilterChain.doFilter()`
- **Iterator:** `java.util.Iterator`
- **Observer:** `java.util.concurrent.Flow` (Publisher/Subscriber)
- **Strategy:** `java.util.Comparator.comparing()`

---

## 7. Design Patterns Anti-Patterns & Misuse

1. **Over-Engineering with Patterns (Patternitis):** Introducing Factories, Builders, and Proxies for trivial classes containing 2 fields creates maintenance overhead without architectural benefit.
2. **Global Mutable Singletons:** Using singletons as hidden global variables violates modularity and makes unit testing impossible without reflection hacks.
3. **Leaking Abstract Factory Details:** Exposing concrete implementation types through abstract factory methods defeats the purpose of abstraction.

---

## 8. Summary & Quick Selection Matrix

- **Need object creation flexibility?** Use **Builder** for multi-parameter models, **Factory Method** for polymorphic creation.
- **Need to adapt incompatible APIs?** Use **Adapter**.
- **Need dynamic functionality without subclassing?** Use **Decorator** or function composition.
- **Need algorithmic variations?** Pass lambdas via **Strategy**.
- **Need recursive tree processing?** Use **Sealed Interfaces** + **Pattern Matching Switch**.

---

## 9. Comprehensive Structural Pattern Implementations

### 1. Adapter Pattern (Object Adapter)
```java
// Target Interface
public interface PaymentGateway {
    void processPayment(String customerId, double amountInUsd);
}

// Adaptee (Legacy third-party service)
public class LegacyStripeService {
    public void chargeInCents(String customerToken, long cents) {
        System.out.printf("Charging %d cents to %s%n", cents, customerToken);
    }
}

// Adapter
public class StripePaymentAdapter implements PaymentGateway {
    private final LegacyStripeService legacyService;
    public StripePaymentAdapter(LegacyStripeService legacyService) { this.legacyService = legacyService; }

    @Override
    public void processPayment(String customerId, double amountInUsd) {
        long cents = Math.round(amountInUsd * 100);
        legacyService.chargeInCents(customerId, cents);
    }
}
```

### 2. Proxy Pattern (Dynamic JDK InvocationHandler)
```java
import java.lang.reflect.*;

public class LoggingProxyHandler implements InvocationHandler {
    private final Object target;
    public LoggingProxyHandler(Object target) { this.target = target; }

    @Override
    public Object invoke(Object proxy, Method method, Object[] args) throws Throwable {
        long start = System.nanoTime();
        System.out.println("[AUDIT] Invoking method: " + method.getName());
        try {
            return method.invoke(target, args);
        } finally {
            System.out.printf("[AUDIT] Method %s completed in %d ns%n", method.getName(), System.nanoTime() - start);
        }
    }

    @SuppressWarnings("unchecked")
    public static <T> T createProxy(T target, Class<T> iface) {
        return (T) Proxy.newProxyInstance(iface.getClassLoader(), new Class<?>[]{iface}, new LoggingProxyHandler(target));
    }
}
```

---

## 10. Comprehensive Behavioral Pattern Implementations

### 1. Chain of Responsibility Pattern (Request Pipeline)
```java
public interface AuthenticationFilter {
    boolean doFilter(Request req);
}

public class AuthenticationPipeline {
    private final List<AuthenticationFilter> filters = new ArrayList<>();

    public AuthenticationPipeline addFilter(AuthenticationFilter filter) {
        filters.add(filter);
        return this;
    }

    public boolean execute(Request req) {
        for (AuthenticationFilter filter : filters) {
            if (!filter.doFilter(req)) return false; // Short-circuit pipeline
        }
        return true;
    }
}
```

### 2. Command Pattern via Functional Interfaces
```java
public class RemoteControl {
    private final Map<String, Runnable> buttonMap = new HashMap<>();

    public void setCommand(String slot, Runnable command) {
        buttonMap.put(slot, command);
    }

    public void pressButton(String slot) {
        Runnable command = buttonMap.get(slot);
        if (command != null) command.run();
    }
}
```

---

## 11. Enterprise Architecture Patterns in Modern Java

### 1. Outbox Pattern
Ensures reliable asynchronous event publishing by storing domain entity mutations and event records within the same atomic ACID database transaction.

### 2. Circuit Breaker Pattern (State Machine)
Prevents cascading service failure by halting calls to failing downstream dependencies:
- **`CLOSED`:** Normal operation; calls pass through.
- **`OPEN`:** Error threshold exceeded; calls fail fast immediately.
- **`HALF-OPEN`:** Trial period testing downstream recovery.

### 3. CQRS (Command Query Responsibility Segregation)
Separates read and write operations into distinct domain models:
- **Write Model:** Validates business invariants and updates relational ACID datastore.
- **Read Model:** Denormalized read-optimized projections stored in Elasticsearch or Redis.

---

## 12. Modern State Pattern via Sealed State Hierarchy

```java
public sealed interface OrderState permits PendingState, PaidState, ShippedState, CancelledState {
    OrderState handle(OrderContext ctx, String event);
}

public record PendingState() implements OrderState {
    @Override
    public OrderState handle(OrderContext ctx, String event) {
        return switch (event) {
            case "PAY" -> new PaidState();
            case "CANCEL" -> new CancelledState();
            default -> throw new IllegalStateException("Invalid event for Pending state: " + event);
        };
    }
}

public record PaidState() implements OrderState {
    @Override
    public OrderState handle(OrderContext ctx, String event) {
        return switch (event) {
            case "SHIP" -> new ShippedState();
            case "REFUND" -> new CancelledState();
            default -> throw new IllegalStateException("Invalid event for Paid state: " + event);
        };
    }
}

public record ShippedState() implements OrderState {
    @Override
    public OrderState handle(OrderContext ctx, String event) {
        throw new IllegalStateException("Shipped orders cannot transition to other states.");
    }
}

public record CancelledState() implements OrderState {
    @Override
    public OrderState handle(OrderContext ctx, String event) {
        throw new IllegalStateException("Cancelled orders cannot transition.");
    }
}
```

---

## 13. Memento Pattern with Immutable Records

```java
public class TextEditor {
    private String content = "";

    // Memento: Completely immutable record snapshot
    public record Memento(String state) {}

    public void write(String text) { content += text; }
    public String getContent() { return content; }

    public Memento save() { return new Memento(content); }
    public void restore(Memento memento) { this.content = memento.state(); }
}

public class EditorHistory {
    private final Deque<TextEditor.Memento> history = new ArrayDeque<>();

    public void push(TextEditor.Memento m) { history.push(m); }
    public TextEditor.Memento pop() { return history.isEmpty() ? null : history.pop(); }
}
```

---

## 14. Pattern Selection Decision Flowchart

```
                            [Design Problem]
                                   |
             +---------------------+---------------------+
             |                     |                     |
     [Object Creation]     [Object Structure]    [Object Behavior]
             |                     |                     |
      Complex steps?        Incompatible APIs?     Different algorithms?
      Yes -> Builder         Yes -> Adapter         Yes -> Strategy
      Family of items?      Dynamic wrapper?       Pass down chain?
      Yes -> Abstract       Yes -> Decorator        Yes -> Chain of Resp
             Factory        Unified subsystem?     Traverse structure?
      Only one instance?    Yes -> Facade           Yes -> Pattern Switch
      Yes -> Enum           Tree hierarchy?         State dependent?
             Singleton      Yes -> Composite        Yes -> State Machine
```
