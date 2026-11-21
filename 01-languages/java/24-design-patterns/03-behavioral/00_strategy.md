# Strategy Pattern in Modern Java

> Encapsulating interchangeable algorithms: family of algorithms, runtime algorithm swapping, replacement of complex conditionals, and functional strategies with lambdas (`Function`, `Comparator`).

| Metadata | Details |
| :--- | :--- |
| **Level** | Core Design Pattern / Behavioral |
| **Prerequisites** | [00_solid-principles](../23-design-and-clean-code/00_solid-principles.md), [09_functional-java](../../09-functional-java/README.md) |
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

### What is the Strategy Pattern?
The Strategy pattern is a behavioral design pattern that defines a **family of algorithms, encapsulates each one into a separate class, and makes them interchangeable**. Strategy lets the algorithm vary independently from the clients that use it.

In enterprise software engineering:
- Different payment calculations (credit card, crypto, PayPal)
- Route calculation algorithms (fastest, shortest, eco-friendly)
- Compression schemes (GZIP, Snappy, Zstandard)
- Sorting and prioritization policies (`Comparator`)

### Why Strategy Replaces Conditionals
Without Strategy, business logic becomes littered with fragile `switch` or `if-else` blocks:
- Adding a new algorithm requires modifying existing methods, violating the Open/Closed Principle.
- Testing requires complex test cases covering every branch.
- Strategy encapsulates each algorithm in its own isolated, easily testable class.

---

## 2. Core Concepts

### Strategy, Concrete Strategies, and Context
1. **Strategy Interface**: The contract common to all algorithmic variants (`PricingStrategy`).
2. **Concrete Strategies**: Independent implementations (`VipPricing`, `HolidayPricing`, `ClearancePricing`).
3. **Context**: The business service that maintains a reference to a Strategy and delegates the calculation to it (`CheckoutService`).

### Modern Evolution: Functional Strategies
In modern Java (Java 8+), because most strategies define only one method, **every `@FunctionalInterface` is a Strategy**!
- You do not need to create 10 separate boilerplate classes; you can pass lambdas or method references (`BigDecimal::add`) directly.

---

## 3. How It Works Internally

### Runtime Algorithm Swapping
```
+--------------------------------------------------------------------------+
|                  STRATEGY PATTERN ARCHITECTURE                           |
|                                                                          |
|  [ Context (OrderService) ]                                              |
|    - strategy: DiscountStrategy                                         |
|    + setStrategy(newStrategy)                                            |
|    + calculateTotal(order): calls strategy.applyDiscount(order)          |
|                 |                                                        |
|                 v                                                        |
|  [ DiscountStrategy (Interface / Functional) ]                           |
|                 ^                        ^                               |
|                 |                        |                               |
|     [ BlackFridayDiscount ]     [ SeniorCitizenDiscount ]                |
+--------------------------------------------------------------------------+
```

---

## 4. Syntax & Usage

### 1. Functional Strategy Definition
```java
import java.math.BigDecimal;

@FunctionalInterface
public interface DiscountStrategy {
    BigDecimal applyDiscount(BigDecimal total);
}

public class OrderCheckout {
    private DiscountStrategy strategy;

    public OrderCheckout(DiscountStrategy strategy) {
        this.strategy = strategy;
    }

    public void setStrategy(DiscountStrategy strategy) {
        this.strategy = strategy;
    }

    public BigDecimal calculateFinal(BigDecimal subtotal) {
        return strategy.applyDiscount(subtotal);
    }
}
```

---

## 5. Practical Examples

### Example 1: Payment Routing Strategy Matrix
**Goal:** Route customer payments dynamically through CreditCard, PayPal, or Crypto strategies.

```java
import java.math.BigDecimal;
import java.util.Objects;

// Strategy Interface
public interface PaymentStrategy {
    boolean pay(BigDecimal amount, String customerRef);
}

// Concrete Strategy 1
public class CreditCardStrategy implements PaymentStrategy {
    private final String cardNumber;
    public CreditCardStrategy(String cardNumber) { this.cardNumber = cardNumber; }

    @Override
    public boolean pay(BigDecimal amount, String customerRef) {
        System.out.println("Charging $" + amount + " to Card: " + cardNumber);
        return true;
    }
}

// Concrete Strategy 2
public class PayPalStrategy implements PaymentStrategy {
    private final String email;
    public PayPalStrategy(String email) { this.email = email; }

    @Override
    public boolean pay(BigDecimal amount, String customerRef) {
        System.out.println("Charging $" + amount + " to PayPal: " + email);
        return true;
    }
}

// Context Service
public class PaymentContext {
    private PaymentStrategy strategy;

    public PaymentContext(PaymentStrategy strategy) {
        this.strategy = Objects.requireNonNull(strategy);
    }

    public void setStrategy(PaymentStrategy strategy) {
        this.strategy = Objects.requireNonNull(strategy);
    }

    public boolean executePayment(BigDecimal amount, String customerId) {
        return strategy.pay(amount, customerId);
    }
}
```

### Example 2: Strategy Pattern via Java `Comparator`
**Goal:** Use standard Java library strategies for flexible multi-attribute sorting.

```java
import java.util.ArrayList;
import java.util.Comparator;
import java.util.List;

public record Product(String name, double price, int stock) {}

public class StrategySortDemo {
    public static void main(String[] args) {
        List<Product> products = new ArrayList<>(List.of(
            new Product("Laptop", 1200.0, 5),
            new Product("Mouse", 25.0, 150),
            new Product("Keyboard", 80.0, 50)
        ));

        // Strategy 1: Sort by Price Ascending
        products.sort(Comparator.comparingDouble(Product::price));

        // Strategy 2: Swap algorithm to sort by Stock Descending
        products.sort(Comparator.comparingInt(Product::stock).reversed());
    }
}
```

### Example 3: Sealed Interface Strategy with Pattern Matching
```java
public sealed interface RoutingStrategy permits FastestRoute, EcoRoute, ScenicRoute {
    int estimateDurationMinutes(int distanceKm);
}

public record FastestRoute() implements RoutingStrategy {
    @Override public int estimateDurationMinutes(int d) { return (int) (d * 0.8); }
}
public record EcoRoute() implements RoutingStrategy {
    @Override public int estimateDurationMinutes(int d) { return (int) (d * 1.0); }
}
public record ScenicRoute() implements RoutingStrategy {
    @Override public int estimateDurationMinutes(int d) { return (int) (d * 1.5); }
}
```

---

### Example 4: Spring Framework Strategy Injection with Map
**Goal:** Automatically inject all strategies into a Spring bean using `Map<String, Strategy>`.

```java
import java.util.Map;

// Spring automatically populates bean name -> bean instance into the Map!
public class StrategyResolver {
    private final Map<String, PaymentStrategy> paymentStrategies;

    public StrategyResolver(Map<String, PaymentStrategy> paymentStrategies) {
        this.paymentStrategies = paymentStrategies;
    }

    public PaymentStrategy resolve(String channelName) {
        PaymentStrategy strategy = paymentStrategies.get(channelName);
        if (strategy == null) {
            throw new IllegalArgumentException("Unknown payment channel: " + channelName);
        }
        return strategy;
    }
}
```

## 6. Best Practices

1. **Use Functional Interfaces for Single-Method Strategies**: Annotate with `@FunctionalInterface` to allow lambda syntax at call sites.
2. **Inject Strategy via Constructor or Setter**: Use constructor injection for defaults, and setter methods for dynamic runtime switching.
3. **Keep Concrete Strategies Stateless Where Possible**: Stateless strategies can be declared as constants or singletons and reused across threads.
4. **Use Java `Comparator` as Design Model**: The JDK\'s `Comparator` interface is the ultimate expression of the Strategy pattern.
5. **Combine with Enum for Fixed Strategy Sets**: When algorithms are static and fixed, an Enum implementing an interface makes an elegant strategy holder.

---

## 7. Common Mistakes & Pitfalls

| Mistake | Why It Fails | Secure Fix |
| :--- | :--- | :--- |
| **Passing Strategy as method parameter without context** | Creates noise across business method signatures. | Store strategy as context field or use dependency injection. |
| **Hardcoding concrete strategies in context** | Defeats the pattern; tightly couples context to specific algorithms. | Depend strictly on Strategy interface. |
| **Stateful strategies in multi-threaded contexts** | Race conditions if strategy modifies its own fields during calculation. | Make strategies pure functions or immutable records. |
| **Creating strategy classes for 1-line lambdas** | Class explosion; adds clutter. | Pass lambdas or method references directly. |

---

## 8. Performance & Security Notes

### JIT Inlining of Lambda Strategies
When passing a lambda or functional strategy (`order.applyDiscount(price -> price.multiply(BigDecimal.valueOf(0.9)))`):
- HotSpot C2 compiler devirtualizes the call site via `invokedynamic`.
- The strategy lambda is inlined directly into the calling context, executing at native speed with zero method invocation penalty!

---

## 9. Comparison / When to Use What

| Pattern | Focus | Mechanism |
| :--- | :--- | :--- |
| **Strategy** | Swaps how an entire algorithm executes | Object composition / Functional interface |
| **Template Method** | Fixes algorithm skeleton, allows step overrides | Inheritance / Subclassing |
| **State** | Changes behavior based on internal state changes | State object transition |
| **Command** | Encapsulates request into standalone object | Action object with `execute()` |

---

## 10. Interview Questions & Answers

### Q1: What is the main difference between Strategy and Template Method?
<details>
<summary>View Answer</summary>
- **Strategy** uses **object composition**: the entire algorithm is encapsulated in a separate strategy object passed to the context. The algorithm can be swapped dynamically at runtime.
- **Template Method** uses **inheritance**: the algorithm skeleton is defined in an abstract superclass, and subclasses override individual steps. The algorithm structure is fixed at compile time.
</details>

### Q2: How did Java 8 Lambdas revolutionize the Strategy Pattern?
<details>
<summary>View Answer</summary>
Prior to Java 8, implementing the Strategy pattern required creating a separate class file for every algorithm variant (or clumsy anonymous inner classes). With Java 8 lambdas and functional interfaces, any Single Abstract Method (SAM) interface is a strategy. Developers can pass algorithms inline as lambdas or method references (`list.sort(Comparator.comparing(User::getName))`) with zero boilerplate.
</details>

### Q3: How can Enums be used to implement the Strategy Pattern?
<details>
<summary>View Answer</summary>
An enum can declare an abstract method, with each enum constant providing its own concrete implementation:
```java
public enum ShippingStrategy {
    STANDARD { @Override public double calc(double wt) { return wt * 1.5; } },
    EXPRESS { @Override public double calc(double wt) { return wt * 3.0; } };
    public abstract double calc(double weight);
}
```
This encapsulates algorithms into a fixed, type-safe, singleton set of strategies.
</details>

### Q4: Give an example of the Strategy pattern in the Java Standard Library.
<details>
<summary>View Answer</summary>
1. `java.util.Comparator`: Strategy for object sorting.
2. `java.util.concurrent.RejectedExecutionHandler`: Strategy for handling rejected tasks when thread pool queues fill (`AbortPolicy`, `CallerRunsPolicy`, `DiscardPolicy`).
3. `javax.swing.border.Border`: Strategy for rendering GUI component borders.
</details>

### Q5: How do you choose between Strategy and State patterns?
<details>
<summary>View Answer</summary>
In Strategy, the client typically chooses the strategy or passes it to the context, and strategies are generally independent of each other. In the State pattern, state objects frequently transition the context from one state to another internally (e.g. `Draft -> InReview -> Published`), and the client has little or no awareness of the state transitions.
</details>

### Q6: Can a Strategy return a different Strategy?
<details>
<summary>View Answer</summary>
Yes. In adaptive algorithms, a strategy can evaluate workload metrics and return an alternative strategy optimized for higher throughput.
</details>

### Q7: Are functional strategies thread-safe?
<details>
<summary>View Answer</summary>
Yes, as long as the strategy lambda or class is stateless (it does not capture or mutate external mutable state). Pure functional strategies are completely thread-safe and can be shared globally across virtual threads.
</details>

### Q8: What design principle is the core foundation of Strategy?
<details>
<summary>View Answer</summary>
The **Open/Closed Principle**: software entities should be open for extension (adding new strategies) but closed for modification (the context class remains untouched).
</details>

---

## 11. Practice Exercises

### Exercise 1: Tax Calculation Strategy
**Task:** Build a tax calculator context using functional strategies for US, EU, and Zero Tax.
<details>
<summary>View Solution</summary>

```java
import java.math.BigDecimal;

@FunctionalInterface
public interface TaxStrategy { BigDecimal computeTax(BigDecimal amount); }

public class TaxContext {
    public static final TaxStrategy US_TAX = a -> a.multiply(new BigDecimal("0.07"));
    public static final TaxStrategy EU_VAT = a -> a.multiply(new BigDecimal("0.20"));
    public static final TaxStrategy ZERO_TAX = a -> BigDecimal.ZERO;
}
```
</details>

### Exercise 2: Enum Strategy Pattern
**Task:** Implement a discount calculator using an enum strategy.
<details>
<summary>View Solution</summary>

```java
public enum DiscountPolicy {
    NONE { @Override public double apply(double p) { return p; } },
    TEN_PERCENT { @Override public double apply(double p) { return p * 0.90; } },
    HALF_OFF { @Override public double apply(double p) { return p * 0.50; } };

    public abstract double apply(double price);
}
```
</details>

### Exercise 3: Dynamic Compression Strategy Context
**Task:** Create a `Compressor` that switches between GZIP and Deflate algorithms.
<details>
<summary>View Solution</summary>

```java
public interface CompressionStrategy { byte[] compress(byte[] data); }

public class DataCompressor {
    private CompressionStrategy strategy;
    public DataCompressor(CompressionStrategy strategy) { this.strategy = strategy; }
    public void setStrategy(CompressionStrategy s) { this.strategy = s; }
    public byte[] execute(byte[] in) { return strategy.compress(in); }
}
```
</details>

---

## 12. Summary / Cheat Sheet

- **Core Goal**: Encapsulate interchangeable algorithms behind a shared interface.
- **Open/Closed**: Add new algorithms without touching existing context classes.
- **Functional Java**: Any `@FunctionalInterface` is a strategy; use lambdas and method references.
- **JDK Exemplar**: `java.util.Comparator` is the standard Strategy implementation.
- **Statelessness**: Keep strategies stateless to ensure 100% thread safety across threads.
- **Enum Strategy**: Use enums when the set of algorithms is fixed and known.

---

## 13. Further Reading
- GoF Design Patterns: [Strategy Pattern](https://www.oreilly.com/library/view/design-patterns-elements/0201633612/)
- Effective Java: [Item 42: Prefer lambdas to anonymous classes](https://www.oreilly.com/library/view/effective-java-3rd/9780134686097/)
- Refactoring Guru: [Strategy Design Pattern](https://refactoring.guru/design-patterns/strategy)
