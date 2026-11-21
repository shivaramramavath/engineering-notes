# Design Pattern Selection Guide

> Comprehensive decision matrix and taxonomy for selecting the right GoF and enterprise design pattern in modern Java 17/21/25 applications.

| Metadata | Details |
| :--- | :--- |
| **Level** | Core Architecture / Decision Matrix |
| **Prerequisites** | Familiarity with Creational, Structural, and Behavioral patterns |
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

### What is the Pattern Selection Guide?
Design patterns are generalized, battle-tested solutions to recurring software engineering problems. However, choosing the **wrong** pattern—or applying a pattern prematurely—introduces accidental complexity, bloats class hierarchies, and confuses maintainers.

This guide provides an authoritative, algorithmic decision framework to help software engineers rapidly identify the optimal design pattern based on concrete architectural requirements.

---

## 2. Core Concepts

### The Master Pattern Decision Tree

#### 1. Are you creating objects? (Creational)
- Need exactly one global instance? $\to$ **Singleton** (Enum or Bill Pugh)
- Object creation involves $\ge 4$ parameters or complex optional configurations? $\to$ **Builder**
- Want subclasses or lambdas to determine which concrete class to instantiate? $\to$ **Factory Method**
- Need to instantiate families of related products that must match? $\to$ **Abstract Factory**
- Creating objects is computationally heavy and can be cloned from templates? $\to$ **Prototype**

#### 2. Are you composing classes and interfaces? (Structural)
- Need to make two incompatible interfaces work together? $\to$ **Adapter**
- Need to add optional responsibilities dynamically without subclassing? $\to$ **Decorator**
- Need to simplify a chaotic subsystem of 10+ classes into one clean entry point? $\to$ **Facade**
- Need to control access, lazy-load, or intercept calls to an object? $\to$ **Proxy**
- Need to treat individual objects and tree hierarchies uniformly? $\to$ **Composite**

#### 3. Are you managing algorithms, state, or object collaboration? (Behavioral)
- Need to swap algorithms dynamically at runtime? $\to$ **Strategy**
- Need a 1-to-many broadcast when an object changes state? $\to$ **Observer**
- Need to support Undo/Redo or queue operations as objects? $\to$ **Command**
- Need an algorithm skeleton with customizable steps? $\to$ **Template Method**
- Need behavior to change automatically as an object transitions lifecycle states? $\to$ **State**
- Need a request to pass through sequential filters until handled? $\to$ **Chain of Responsibility**
- Need to traverse a collection without exposing internal layout? $\to$ **Iterator**

---

## 3. How It Works Internally

### Cognitive Complexity Tradeoff Curve
```
[ Problem Complexity ]
       ^
       |                                   / (Over-engineering Danger)
       |                            Pattern
       |                             Zone
       |                       /
       |                Direct
       |                 Code
       |           /
       +---------------------------------------------> [ System Scale / Change Frequency ]
```
- If an algorithm never changes: Write direct, plain code.
- If behavior varies across 3+ variants or changes frequently: Apply the appropriate Design Pattern.

---

## 4. Syntax & Usage

### Pattern Quick Reference Matrix

| Problem Scenario | Recommended Pattern | Modern Java Idiom |
| :--- | :--- | :--- |
| **Multiple optional constructor parameters** | Builder | Static Builder class or Record with wither |
| **Incompatible third-party vendor SDK** | Adapter | Method reference or Object Adapter |
| **Dynamic runtime algorithm switching** | Strategy | `@FunctionalInterface` lambda / `Comparator` |
| **State-driven entity workflow** | State | Sealed Interfaces + Pattern Matching |
| **Uniform tree hierarchy operations** | Composite | Sealed interface with leaf & composite records |
| **Cross-cutting security / transactions** | Proxy | Dynamic Proxy (`java.lang.reflect.Proxy`) |

---

## 5. Practical Examples

### Example 1: Solving the "Notification Routing" Problem
**Scenario:** A system needs to send alerts via Email, SMS, Slack, or Webhook. Alerts can have optional attachments, rate-limiting, and auditing.

**Pattern Combination:**
1. **Strategy**: Encapsulates each communication protocol (`EmailStrategy`, `SmsStrategy`).
2. **Decorator**: Dynamically wraps channels with auditing and rate-limiting (`RateLimitedNotification(AuditedNotification(channel))`).
3. **Factory Method**: Resolves the base channel strategy dynamically from a configuration string.

---

## 6. Best Practices

1. **Follow the Rule of Three**: Do not introduce an abstract pattern until you have at least three distinct concrete use cases.
2. **Prefer Composition Over Inheritance**: Favor patterns that assemble behavior dynamically (Strategy, Decorator, Adapter) over rigid class hierarchies.
3. **Leverage Modern Java Features**: Replace verbose GoF boilerplate with Java records, sealed interfaces, and lambdas.
4. **Refactor Towards Patterns, Not From Patterns**: Let the design emerge organically as code grows, rather than forcing patterns into empty scaffolding on day one.

---

## 7. Common Mistakes & Pitfalls

| Mistake | Why It Fails | Secure Fix |
| :--- | :--- | :--- |
| **Pattern Mania (Golden Hammer)** | Applying design patterns to trivial 5-line methods. | Use simple, direct, readable code (KISS). |
| **Using Singleton as a Global Variable** | Introduces hidden dependencies and breaks unit testing. | Use Dependency Injection. |
| **Confusing Decorator with Adapter** | Changing interface when intending to decorate. | Decorators preserve identical interfaces. |

---

## 8. Performance & Security Notes

### Inlining and Escape Analysis
Modern JVM JIT compilers recognize standard design pattern idioms:
- Small factories, builders, and adapters are inlined directly into the caller.
- Using clean patterns does not degrade throughput in compiled HotSpot C2 code.

---

## 9. Comparison / When to Use What

| Pattern Family | Primary Focus | Best Starting Point |
| :--- | :--- | :--- |
| **Creational** | How objects are instantiated | Builder & Factory Method |
| **Structural** | How classes are composed | Adapter & Decorator |
| **Behavioral** | How algorithms and messages flow | Strategy & Observer |

---

## 10. Interview Questions & Answers

### Q1: How do modern Java language features (Java 8–25) reduce the need for classic GoF design patterns?
<details>
<summary>View Answer</summary>
1. **Strategy**: Replaced by lambdas and `@FunctionalInterface`.
2. **Command**: Replaced by `Runnable`, `Callable`, and `Consumer`.
3. **Template Method**: Replaced by the functional execute-around (loan) pattern.
4. **State & Visitor**: Replaced by Sealed Interfaces and Pattern Matching switch expressions.
5. **Builder / DTO**: Simplified using immutable Java Records.
</details>

### Q2: What is the "Rule of Three" in software refactoring?
<details>
<summary>View Answer</summary>
The Rule of Three states that you should not extract an abstraction or introduce a design pattern the first or second time you write code. The first time, just write it. The second time you duplicate something similar, wince but duplicate it. The third time you see the exact same pattern, refactor and introduce the design pattern.
</details>

---

## 11. Practice Exercises

### Exercise 1: Pattern Identification Quiz
**Task:** Identify which pattern fits each problem:
1. Converting an array of strings to a `List`.
2. GZIP-compressing a file while writing.
3. Adding an item to an undo stack.
<details>
<summary>View Solution</summary>

```
1. Adapter (Arrays.asList)
2. Decorator (GZIPOutputStream)
3. Command (UndoableCommand)
```
</details>

---


## 1. Full GoF Pattern Catalog & Decision Matrix

The Gang of Four (GoF) design patterns solve recurring object-oriented structural and behavioral challenges. Below is the comprehensive classification matrix for all 23 GoF patterns:

| Pattern | Category | Intent | Key Structural Indicator | Typical JDK / Spring Example |
| :--- | :--- | :--- | :--- | :--- |
| **Singleton** | Creational | Guarantee a single instance with global access | Private constructor, static instance | `Runtime.getRuntime()`, Spring singletons |
| **Factory Method** | Creational | Delegate instantiation to subclasses | Abstract creator or factory method | `List.of()`, `Calendar.getInstance()` |
| **Abstract Factory**| Creational | Create families of related objects | Factory interface with multiple creation methods | `DocumentBuilderFactory`, JDBC `Driver` |
| **Builder** | Creational | Construct complex objects step by step | Fluent API, step builder, `build()` method | `StringBuilder`, `HttpRequest.newBuilder()` |
| **Prototype** | Creational | Clone existing objects without coupling to classes | `clone()` or copy constructors | `Object.clone()`, Prototype beans |
| **Adapter** | Structural | Convert one interface to another callers expect | Wrapper implementing target, holding adaptee | `Arrays.asList()`, `InputStreamReader` |
| **Bridge** | Structural | Decouple abstraction from its implementation | Abstraction delegates to an implementor interface | JDBC driver manager vs database drivers |
| **Composite** | Structural | Treat individual objects and compositions uniformly| Component interface implemented by leaf and node | File system (File vs Directory), AWT containers |
| **Decorator** | Structural | Attach additional responsibilities dynamically | Wrapper implementing interface and delegating | `BufferedInputStream(new FileInputStream())` |
| **Facade** | Structural | Provide unified simple interface to complex subsystem | High-level orchestrating class | `org.springframework.web.client.RestClient` |
| **Flyweight** | Structural | Share common fine-grained state among many objects | Intrinsic (shared) vs Extrinsic state | `Integer.valueOf(-128..127)`, String pool |
| **Proxy** | Structural | Provide surrogate or placeholder to control access | Target interface proxy implementing interceptors| `java.lang.reflect.Proxy`, Spring AOP |
| **Chain of Resp**| Behavioral | Pass requests along a chain of handlers | Successor link; pass down or handle | `jakarta.servlet.Filter`, Spring Security chain |
| **Command** | Behavioral | Encapsulate request as an object | Command interface with `execute()` | `Runnable`, `Callable`, CQRS commands |
| **Interpreter** | Behavioral | Define grammar and interpreter for language | Abstract syntax tree hierarchy | `java.util.regex.Pattern`, SpEL expressions |
| **Iterator** | Behavioral | Sequentially access elements without exposing structure| `hasNext()`, `next()`, cursor tracking | `java.util.Iterator`, Spliterator |
| **Mediator** | Behavioral | Encapsulate how a set of objects interact | Central coordinator; peers do not reference each other | `java.util.concurrent.ExecutorService` |
| **Memento** | Behavioral | Capture and externalize internal state for undo | State snapshot token without breaking encapsulation | Undo/redo managers, transaction savepoints |
| **Observer** | Behavioral | Notify dependents automatically upon state change | Subject maintains subscriber list; publish event | `Flow.Publisher`, Spring `ApplicationListener` |
| **State** | Behavioral | Alter behavior when internal state changes | Context delegates to polymorphic State objects | Workflow engines, connection state machine |
| **Strategy** | Behavioral | Encapsulate interchangeable algorithms | Context takes strategy interface at runtime | `Comparator.comparing()`, Compression algorithms |
| **Template Method**| Behavioral| Skeleton of algorithm with customizable steps | Final template method calling abstract hooks | `AbstractList.get()`, Spring `JdbcTemplate` |
| **Visitor** | Behavioral | Separate operations from object structure | Double dispatch: `element.accept(visitor)` | `FileVisitor` in `Files.walkFileTree()` |

---

## 2. Exhaustive Decision Tree

Follow this branch logic to pinpoint the exact design pattern required for your architectural scenario:

```
Start
  |
  +---> Are you creating objects? (Creational)
  |       |
  |       +---> Single instance across application? --------------> Singleton
  |       +---> Complex construction with many parameters? -------> Builder
  |       +---> Caller doesn't know concrete subtype to create? ---> Factory Method
  |       +---> Need to create families of matched products? ------> Abstract Factory
  |       +---> Costly initialization; clone existing object? -----> Prototype
  |
  +---> Are you assembling classes and interfaces? (Structural)
  |       |
  |       +---> Incompatible interface needs bridging? -----------> Adapter
  |       +---> Add dynamic behavior without subclassing? --------> Decorator
  |       +---> Need simplified facade over complex subsystem? ---> Facade
  |       +---> Lazy loading, security checks, caching, proxying? -> Proxy
  |       +---> Tree-like part-whole hierarchies? -----------------> Composite
  |       +---> Decouple abstraction and implementation? ---------> Bridge
  |       +---> Billions of small objects consuming memory? -------> Flyweight
  |
  +---> Are you coordinating runtime behaviors & algorithms? (Behavioral)
          |
          +---> Swap algorithm at runtime? ------------------------> Strategy
          |
          +---> Object behavior depends entirely on state? --------> State
          |
          +---> One-to-many event notification? -------------------> Observer
          |
          +---> Encapsulate action as an object for queue/undo? ---> Command
          |
          +---> Sequential processing across multiple handlers? ---> Chain of Responsibility
          |
          +---> Algorithm skeleton fixed, steps overridden? -------> Template Method
          |
          +---> Decouple N-to-N communication between objects? ----> Mediator
          |
          +---> Need to save and restore internal snapshot? -------> Memento
          |
          +---> Add new operations over existing object tree? -----> Visitor
```

---

## 3. Pattern Anti-Smell & Selection Heuristics

When evaluating whether to introduce a design pattern, check against these practical rules of thumb:

1. **Rule of Three**: Do not introduce Strategy or Abstract Factory for only two implementations unless dynamic runtime swapping is an immediate production requirement.
2. **Favor Composition over Inheritance**: Decorator, Strategy, and Composite prevent deep inheritance hierarchies (e.g. avoiding 5 levels of abstract base classes).
3. **Prefer Modern Functional Interfaces**: In Java 8+, many GoF behavioral patterns collapse into clean lambdas:
   - Strategy $	o$ `Function<T, R>` or `Consumer<T>`
   - Command $	o$ `Runnable` or `Supplier<T>`
   - Template Method $	o$ Higher-order function taking lambdas for steps
   - Factory $	o$ Method reference (`Order::new`)

## 12. Summary / Cheat Sheet

- **Creational**: Singleton (Enum), Builder ($\ge 4$ params), Factory Method (1 product), Abstract Factory (family).
- **Structural**: Adapter (compat), Decorator (add behavior), Facade (simplify), Proxy (access control), Composite (trees).
- **Behavioral**: Strategy (swap algorithm), Observer (events), Command (actions/undo), State (lifecycles), Chain (filters).
- **Golden Rule**: Simplicity first; refactor to patterns when duplication or complexity emerges.

---

## 13. Further Reading
- Design Patterns: Elements of Reusable Object-Oriented Software (GoF)
- Refactoring Guru Design Patterns Catalog
- Effective Java (3rd Edition) by Joshua Bloch


---

## 4. End-to-End Refactoring Walkthrough: From Anti-Pattern to Clean Pattern

### Refactoring 1: From Massive Switch Statement to Polymorphic Strategy Pattern

#### Legacy Code (Violates Open/Closed Principle)
```java
public class ShippingCalculator {
    public double calculate(String type, double weight) {
        if ("STANDARD".equalsIgnoreCase(type)) {
            return weight * 1.5;
        } else if ("EXPRESS".equalsIgnoreCase(type)) {
            return weight * 3.0 + 5.0;
        } else if ("OVERNIGHT".equalsIgnoreCase(type)) {
            return weight * 5.0 + 15.0;
        } else if ("INTERNATIONAL".equalsIgnoreCase(type)) {
            return weight * 12.0 + 25.0;
        } else {
            throw new IllegalArgumentException("Unknown shipping type: " + type);
        }
    }
}
```

#### Modern Java 25 Refactored Code (Clean Strategy with Records and Pattern Matching)
```java
public sealed interface ShippingTier permits StandardShipping, ExpressShipping, OvernightShipping, InternationalShipping {
    double ratePerKg();
    double flatFee();

    default double calculateCost(double weightKg) {
        if (weightKg <= 0.0) throw new IllegalArgumentException("Weight must be positive");
        return (weightKg * ratePerKg()) + flatFee();
    }
}

public record StandardShipping() implements ShippingTier {
    @Override public double ratePerKg() { return 1.5; }
    @Override public double flatFee() { return 0.0; }
}

public record ExpressShipping() implements ShippingTier {
    @Override public double ratePerKg() { return 3.0; }
    @Override public double flatFee() { return 5.0; }
}

public record OvernightShipping() implements ShippingTier {
    @Override public double ratePerKg() { return 5.0; }
    @Override public double flatFee() { return 15.0; }
}

public record InternationalShipping(String destinationCountry) implements ShippingTier {
    @Override public double ratePerKg() { return 12.0; }
    @Override public double flatFee() { return 25.0; }
}

public final class ModernShippingService {
    public double quoteShipping(ShippingTier tier, double weightKg) {
        return tier.calculateCost(weightKg);
    }
}
```

---

### Refactoring 2: From Rigid Object Instantiation to Fluent Step Builder

#### Legacy Code (Telescoping Constructors with High Error Probability)
```java
public class DatabaseConnectionConfig {
    public DatabaseConnectionConfig(String host, int port) { ... }
    public DatabaseConnectionConfig(String host, int port, String user) { ... }
    public DatabaseConnectionConfig(String host, int port, String user, String pass) { ... }
    public DatabaseConnectionConfig(String host, int port, String user, String pass, int timeout, boolean ssl) { ... }
}
```

#### Refactored Code (Compile-Time Enforced Step Builder)
```java
public final class DatabaseConnectionConfig {
    private final String host;
    private final int port;
    private final String username;
    private final String password;
    private final int timeoutSeconds;
    private final boolean ssl;

    private DatabaseConnectionConfig(Builder builder) {
        this.host = builder.host;
        this.port = builder.port;
        this.username = builder.username;
        this.password = builder.password;
        this.timeoutSeconds = builder.timeoutSeconds;
        this.ssl = builder.ssl;
    }

    public static HostStep builder() { return new Builder(); }

    public interface HostStep { PortStep host(String host); }
    public interface PortStep { AuthStep port(int port); }
    public interface AuthStep { OptionsStep credentials(String user, String pass); }
    public interface OptionsStep {
        OptionsStep timeout(int seconds);
        OptionsStep ssl(boolean enabled);
        DatabaseConnectionConfig build();
    }

    private static class Builder implements HostStep, PortStep, AuthStep, OptionsStep {
        private String host;
        private int port;
        private String username;
        private String password;
        private int timeoutSeconds = 30;
        private boolean ssl = true;

        @Override public PortStep host(String host) { this.host = Objects.requireNonNull(host); return this; }
        @Override public AuthStep port(int port) { this.port = port; return this; }
        @Override public OptionsStep credentials(String user, String pass) { 
            this.username = Objects.requireNonNull(user); 
            this.password = Objects.requireNonNull(pass); 
            return this; 
        }
        @Override public OptionsStep timeout(int seconds) { this.timeoutSeconds = seconds; return this; }
        @Override public OptionsStep ssl(boolean enabled) { this.ssl = enabled; return this; }
        @Override public DatabaseConnectionConfig build() { return new DatabaseConnectionConfig(this); }
    }
}
```
