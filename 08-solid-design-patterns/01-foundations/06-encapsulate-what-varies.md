# Encapsulate What Varies

> **One-line summary:** Identify the aspects of your application that are subject to frequent change and separate them from the parts that remain stable, isolating volatility so business shifts do not impact core system policies.

| Attribute | Specification |
|---|---|
| **Level** | Foundations / Core |
| **Prerequisites** | [01-oop-pillars.md](01-oop-pillars.md), [03-coupling-and-cohesion.md](03-coupling-and-cohesion.md) |
| **Estimated Reading Time** | 25 minutes |
| **Status** | Complete |
| **Category** | Foundations |
| **Also Known As** | Separation of Volatility, Policy vs Mechanism |
| **Languages Tested** | Java 25 (LTS) / TypeScript 5.8 (Node 24 native) / Python 3.14 |

---

## Table of Contents
- [1 The Problem](#1-the-problem)
- [2 Why It Exists](#2-why-it-exists)
- [3 What It Is](#3-what-it-is)
- [4 The Problem Without It](#4-the-problem-without-it)
- [5 How It Solves It](#5-how-it-solves-it)
- [6 How It Works](#6-how-it-works)
- [7 Real-World Examples](#7-real-world-examples)
- [8 When to Use and When NOT to Use](#8-when-to-use-and-when-not-to-use)
- [9 Variations and Language Idioms](#9-variations-and-language-idioms)
- [10 Relationship to Other Principles and Patterns](#10-relationship-to-other-principles-and-patterns)
- [11 Common Mistakes and Anti-Patterns](#11-common-mistakes-and-anti-patterns)
- [12 Testing and Design Verification](#12-testing-and-design-verification)
- [13 Performance, Concurrency and Security Notes](#13-performance-concurrency-and-security-notes)
- [14 Interview Questions](#14-interview-questions)
- [15 Practice Exercises](#15-practice-exercises)
- [16 Summary / Cheat Sheet](#16-summary--cheat-sheet)
- [17 Further Reading](#17-further-reading)

---

## 1 The Problem

An international e-commerce platform processes millions of dollars in orders. In the core checkout flow, the engineering team has a stable business policy:
> *Total Price = Subtotal + Shipping Cost + Taxes - Discounts*

However, the team embedded the calculation of taxes directly within the `CheckoutService`:
```java
if (country.equals("US")) { tax = subtotal * 0.08; }
else if (country.equals("EU")) { tax = subtotal * 0.20; }
```

Every two weeks, tax regulations change:
- A new jurisdiction requires split state and local county taxes (US Colorado vs New York).
- The UK changes digital services VAT rates.
- France introduces a seasonal holiday exemption on books.

Every single change requires opening and modifying `CheckoutService.java`. Because `CheckoutService` also handles credit card tokenization, stock inventory locking, and fraud evaluation, modifying it for tax rules repeatedly introduces regressions into payment processing. The blast radius of tax policy volatility constantly endangers the core revenue-generating pipeline.

---

## 2 Why It Exists

In software design, not all code changes at the same rate. 
- **Stable Policy**: The high-level workflow of the business (e.g. "An order must calculate taxes, authorize payment, and reserve inventory").
- **Volatile Mechanism**: The specific implementation algorithms that change frequently due to external laws, marketing campaigns, or third-party vendors (e.g. tax percentage calculations, discount rules, shipping carrier APIs).

The rule *"Identify what varies and encapsulate it"* was popularized by the Gang of Four (1994) and Kent Beck (Extreme Programming). It balances the force between stable business workflows and volatile external rules.

---

## 3 What It Is

The principle states:
1. Examine code that requires repeated modification.
2. Extract that volatile behavior out of the stable enclosing class.
3. Encapsulate it inside its own class hierarchy behind an interface.
4. Inject the encapsulated behavior back into the stable class.

```mermaid
graph TD
    subgraph Stable["Stable Enclosing Policy (Rarely Changes)"]
        Checkout[CheckoutService: Total = Subtotal + Tax]
    end
    subgraph Volatile["Encapsulated Volatility (Changes Constantly)"]
        ITax[TaxStrategy Interface]
        ITax <|.. US[USTaxStrategy]
        ITax <|.. EU[EUVatStrategy]
        ITax <|.. UK[UKDigitalTaxStrategy]
    end
    Checkout --> ITax
```

---

## 4 The Problem Without It

Here is the naive implementation where volatile tax calculation is hardcoded into checkout:

```java
// File: VariesWithoutApp.java
class NaiveCheckout {
    public double calculateTotal(double subtotal, String country) {
        // Volatile tax logic embedded directly in checkout
        double taxRate;
        if (country.equals("US")) {
            taxRate = 0.08;
        } else if (country.equals("EU")) {
            taxRate = 0.20;
        } else if (country.equals("UK")) {
            taxRate = 0.20;
        } else {
            taxRate = 0.05;
        }
        double tax = subtotal * taxRate;
        System.out.println("Naive checkout: subtotal=" + subtotal + ", tax=" + tax + ", total=" + (subtotal + tax));
        return subtotal + tax;
    }
}

public class VariesWithoutApp {
    public static void main(String[] args) {
        NaiveCheckout checkout = new NaiveCheckout();
        checkout.calculateTotal(100.0, "EU");
    }
}
```

```text
Naive checkout: subtotal=100.0, tax=20.0, total=120.0
```

### The Change Request: Add Canadian Provincial Sales Tax (PST/GST)
When Canada is added:
1. You must edit `NaiveCheckout.java`.
2. Any syntax or logic mistake in the Canadian branch risks breaking checkout for US and European customers.
3. Every test for `NaiveCheckout` must be re-run in CI/CD.

---

## 5 How It Solves It

By encapsulating tax calculations behind `TaxStrategy`, the checkout policy becomes immune to jurisdictional changes.

### Java Implementation
```java
// File: VariesWithApp.java
interface TaxStrategy {
    double calculateTax(double subtotal);
}

class UsTaxStrategy implements TaxStrategy {
    public double calculateTax(double subtotal) { return subtotal * 0.08; }
}

class EuVatStrategy implements TaxStrategy {
    public double calculateTax(double subtotal) { return subtotal * 0.20; }
}

class ZeroTaxStrategy implements TaxStrategy {
    public double calculateTax(double subtotal) { return 0.0; }
}

class DecoupledCheckout {
    // Stable policy: calculate total = subtotal + tax
    public double calculateTotal(double subtotal, TaxStrategy taxStrategy) {
        double tax = taxStrategy.calculateTax(subtotal);
        double total = subtotal + tax;
        System.out.println("Decoupled checkout: subtotal=" + subtotal + ", tax=" + tax + ", total=" + total);
        return total;
    }
}

public class VariesWithApp {
    public static void main(String[] args) {
        DecoupledCheckout checkout = new DecoupledCheckout();
        checkout.calculateTotal(100.0, new EuVatStrategy());
        checkout.calculateTotal(100.0, new UsTaxStrategy());
    }
}
```

### TypeScript Implementation
```typescript
// File: VariesWithApp.ts
interface TaxStrategy {
    calculateTax(subtotal: number): number;
}

class UsTaxStrategy implements TaxStrategy {
    calculateTax(subtotal: number): number { return subtotal * 0.08; }
}

class EuVatStrategy implements TaxStrategy {
    calculateTax(subtotal: number): number { return subtotal * 0.20; }
}

class DecoupledCheckout {
    calculateTotal(subtotal: number, taxStrategy: TaxStrategy): number {
        const tax = taxStrategy.calculateTax(subtotal);
        const total = subtotal + tax;
        console.log(`TS Decoupled checkout: subtotal=${subtotal}, tax=${tax}, total=${total}`);
        return total;
    }
}

const checkout = new DecoupledCheckout();
checkout.calculateTotal(100.0, new EuVatStrategy());
```

### Python Implementation
```python
# File: varies_with_app.py
from typing import Protocol

class TaxStrategy(Protocol):
    def calculate_tax(self, subtotal: float) -> float: ...

class UsTaxStrategy:
    def calculate_tax(self, subtotal: float) -> float:
        return subtotal * 0.08

class EuVatStrategy:
    def calculate_tax(self, subtotal: float) -> float:
        return subtotal * 0.20

class DecoupledCheckout:
    def calculate_total(self, subtotal: float, tax_strategy: TaxStrategy) -> float:
        tax = tax_strategy.calculate_tax(subtotal)
        total = subtotal + tax
        print(f"Python Decoupled checkout: subtotal={subtotal}, tax={tax}, total={total}")
        return total

checkout = DecoupledCheckout()
checkout.calculate_total(100.0, EuVatStrategy())
```

```text
Decoupled checkout: subtotal=100.0, tax=20.0, total=120.0
Decoupled checkout: subtotal=100.0, tax=8.0, total=108.0
TS Decoupled checkout: subtotal=100, tax=20, total=120
Python Decoupled checkout: subtotal=100.0, tax=20.0, total=120.0
```

---

## 6 How It Works

### Separating Policy from Mechanism

| Dimension | Stable Policy | Volatile Mechanism |
|---|---|---|
| **Rate of Change** | Low (changes once every 2–5 years) | High (changes monthly or weekly) |
| **Examples** | Order workflow, authentication sequence | Tax formulas, shipping APIs, third-party auth providers |
| **Coupling** | Depends only on domain abstractions | Implements specific interface |
| **Risk** | Highest risk if broken | Isolated blast radius |

---

## 7 Real-World Examples

### 1. Business Domain: Dynamic Discount Engine
A retail platform supports dozens of dynamic marketing campaigns (Black Friday BOGO, loyalty member tier discounts, coupon codes). The checkout logic encapsulates discounts into `DiscountStrategy`. Adding a "Buy 2 Get 1 Free" promotion requires authoring one new strategy class without touching checkout.

### 2. Standard Library: `java.util.Comparator`
The algorithm for sorting an array or list (`TimSort`) is mathematically complex and stable. The criteria for comparing two elements varies for every business domain. Java encapsulates what varies into the `Comparator<T>` functional interface, keeping the sorting algorithm completely decoupled from comparison rules.

### 3. Modern Cloud / Infrastructure: Kubernetes Storage Classes
In Kubernetes, a pod declares that it needs a `PersistentVolumeClaim` with 50GB of storage. The pod does not know or care whether the volume is backed by AWS EBS, Google Cloud Persistent Disk, or local NVMe. Kubernetes encapsulates what varies behind the CSI (Container Storage Interface).

---

## 8 When to Use and When NOT to Use

### Encapsulate What Varies When:
- Code contains long `switch` statements inspecting type, region, or environment.
- Git history shows that a specific method is edited in nearly every release cycle.
- A business rule is dictated by external organizations (tax authorities, partner APIs).

### Avoid / Over-Engineering Signals:
- **Speculative Volatility**: Encapsulating an operation that has never changed and has zero likelihood of changing. Do not introduce strategies for things that are stable.

---

## 9 Variations and Language Idioms

- **Functional Closures**: In modern Java, TypeScript, and Python, you don't even need a full class hierarchy. You can encapsulate what varies into a simple lambda or higher-order function: `checkout.calculateTotal(100.0, subtotal -> subtotal * 0.20)`.
- **Strategy Pattern vs Template Method**:
  - Encapsulate via **Strategy (Composition)**: preferred when algorithms can be swapped dynamically.
  - Encapsulate via **Template Method (Inheritance)**: used when the invariant skeleton resides in an abstract class and subclasses fill in specific varying steps.

---

## 10 Relationship to Other Principles and Patterns

- **Open/Closed Principle (OCP)**: "Encapsulate what varies" is the primary operational technique to achieve OCP.
- **Single Responsibility Principle (SRP)**: Volatile logic and stable policy are two distinct reasons to change; separating them satisfies SRP.
- **Strategy Pattern & State Pattern**: Both patterns are direct applications of this principle.

---

## 11 Common Mistakes and Anti-Patterns

| Mistake | Why It Breaks | Production Fix |
|---|---|---|
| **Encapsulating What is Stable** | Creating strategies for basic integer math creates needless class bloat. | Encapsulate only proven axes of change. |
| **Leaking Policy into the Strategy** | Letting the tax strategy decide whether the order should be approved violates single responsibility. | Keep strategy focused strictly on its varying calculation. |
| **Passing Unnecessary Data to Strategy** | Passing the entire 50-field `Order` object to a tax calculator that only needs `subtotal` and `state`. | Pass only the minimal input values required. |
| **Hardcoded Factory Switches** | Replacing a switch in the client with a switch in a factory still leaves the switch vulnerable. | Use a registry map (`Map<Country, TaxStrategy>`) or dependency injection container. |
| **Ignoring Performance in Hot Paths** | Creating new strategy object instances inside a tight loop with millions of iterations. | Reuse stateless strategy singleton instances. |
| **Neglecting Default Behavior** | Forgetting to provide a fallback strategy when an unrecognized region or variant is provided. | Use the Null Object pattern or a strict default strategy. |

---

## 12 Testing and Design Verification

Testing becomes trivial because the volatile calculation can be verified in isolation with 100% boundary test coverage without running the checkout pipeline:

```java
class EuVatStrategyTest {
    void testStandardVatCalculation() {
        EuVatStrategy strategy = new EuVatStrategy();
        assert strategy.calculateTax(100.0) == 20.0;
        assert strategy.calculateTax(0.0) == 0.0;
    }
}
```

---

## 13 Performance, Concurrency and Security Notes

- **Thread Safety**: Encapsulated strategies should be stateless. A stateless strategy instance can be shared safely across thousands of concurrent request threads without synchronization locks.
- **Security**: Isolating third-party integration code ensures that external SDK vulnerabilities cannot tamper with core business workflow state.

---

## 14 Interview Questions

<details>
<summary><strong>1. What does the principle "Encapsulate What Varies" mean in software design?</strong></summary>

**Answer:**  
It means identifying parts of the codebase that change frequently or are expected to have multiple variations, extracting them out of the stable enclosing code, and encapsulating them behind an interface. This ensures that when the varying behavior changes, the stable surrounding policy requires no modification.
</details>

<details>
<summary><strong>2. How does "Encapsulate What Varies" differ from standard data encapsulation?</strong></summary>

**Answer:**  
- **Data Encapsulation**: Hiding an object's internal fields and state using access modifiers (`private`).
- **Encapsulate What Varies**: An architectural principle of identifying volatile *behavior or algorithms* and separating them from invariant workflow policy.
</details>

<details>
<summary><strong>3. Which GoF design patterns directly implement "Encapsulate What Varies"?</strong></summary>

**Answer:**  
- **Strategy Pattern**: Encapsulates varying algorithms.
- **State Pattern**: Encapsulates varying state-dependent behaviors.
- **Factory Method**: Encapsulates varying object instantiation logic.
- **Template Method**: Encapsulates varying algorithm steps in subclasses while keeping the skeleton invariant.
- **Command Pattern**: Encapsulates varying user requests as objects.
</details>

<details>
<summary><strong>4. How do you identify "what varies" in an existing legacy codebase?</strong></summary>

**Answer:**  
Inspect git commit history to find hot files that change in almost every release. Look for long `if/else` or `switch` statements checking enums, strings, or types. Look for code that deals with external vendors, tax laws, or third-party APIs.
</details>

<details>
<summary><strong>5. What is the risk of applying this principle prematurely?</strong></summary>

**Answer:**  
Prematurely encapsulating behavior that never actually varies leads to speculative generality and needless cognitive indirection. Developers must trace through multiple interfaces and files to understand a simple 5-line operation that has never changed.
</details>

<details>
<summary><strong>6. How can modern functional programming idioms simplify this principle?</strong></summary>

**Answer:**  
Instead of creating formal interface hierarchies and concrete class files (`TaxStrategy`, `UsTaxStrategy`, `EuVatStrategy`), functional languages allow passing pure functions or closures directly (`subtotal -> subtotal * 0.20`), eliminating boilerplate classes.
</details>

<details>
<summary><strong>7. How does this principle support the Open/Closed Principle (OCP)?</strong></summary>

**Answer:**  
By encapsulating the varying axis behind an interface, the system can be extended with new variants (new implementations of the interface) without modifying the closed, stable client code that consumes the interface.
</details>

<details>
<summary><strong>8. In what way does this principle reduce regression testing overhead?</strong></summary>

**Answer:**  
When a change occurs within an encapsulated strategy, only that strategy needs to be re-tested. The stable policy class and all other existing strategy implementations remain untouched, drastically reducing regression risk.
</details>

---

## 15 Practice Exercises

### Exercise 1: Spot the Volatility Vector (Easy)
Identify the varying behavior that should be encapsulated:
```java
public class TextDocument {
    private String content;
    public void export(String format) {
        if (format.equals("HTML")) { /* complex html tags */ }
        else if (format.equals("MARKDOWN")) { /* markdown asterisks */ }
        else if (format.equals("EPUB")) { /* epub zip binary formatting */ }
    }
}
```
<details>
<summary><strong>Solution</strong></summary>

The **export formatting algorithm** is the varying behavior.  
**Fix:** Extract an `Exporter` interface (`void export(String content)`) with `HtmlExporter`, `MarkdownExporter`, and `EpubExporter` implementations.
</details>

### Exercise 2: Refactor to Functional Strategy (Medium)
Refactor this search filter to use a functional predicate:
```java
public class ProductCatalog {
    public List<Product> filter(List<Product> items, String criteria, double val) {
        List<Product> res = new ArrayList<>();
        for (Product p : items) {
            if (criteria.equals("PRICE_UNDER") && p.price < val) res.add(p);
            else if (criteria.equals("WEIGHT_OVER") && p.weight > val) res.add(p);
        }
        return res;
    }
}
```
<details>
<summary><strong>Solution</strong></summary>

```java
import java.util.function.Predicate;

public class ProductCatalog {
    public List<Product> filter(List<Product> items, Predicate<Product> condition) {
        List<Product> res = new ArrayList<>();
        for (Product p : items) {
            if (condition.test(p)) res.add(p);
        }
        return res;
    }
}
// Caller:
catalog.filter(items, p -> p.price < 50.0);
```
</details>

### Exercise 3: Encapsulate Delivery Routing (Hard)
Design a delivery service where package routing calculations (Fastest, Cheapest, Eco-Friendly) are encapsulated so that new routing algorithms can be added at runtime.
<details>
<summary><strong>Solution</strong></summary>

```java
interface RouteStrategy { Route calculateRoute(Location start, Location dest); }
class FastestRouteStrategy implements RouteStrategy { public Route calculateRoute(Location s, Location d) { /* shortest travel time */ return new Route(); } }
class EcoRouteStrategy implements RouteStrategy { public Route calculateRoute(Location s, Location d) { /* lowest carbon footprint */ return new Route(); } }

class DeliveryManager {
    private RouteStrategy routeStrategy;
    public DeliveryManager(RouteStrategy strategy) { this.routeStrategy = strategy; }
    public void setStrategy(RouteStrategy strategy) { this.routeStrategy = strategy; }
    public Route planDelivery(Location s, Location d) {
        return routeStrategy.calculateRoute(s, d);
    }
}
```
</details>

### Exercise 4: Spot the Inappropriate Abstraction (Easy)
Why is creating a `MathAdditionStrategy` and `MathSubtractionStrategy` for an arithmetic calculator an over-engineering mistake?
<details>
<summary><strong>Solution</strong></summary>

Basic addition and subtraction are immutable mathematical primitives defined by the language and CPU architecture. They do not vary over time or across jurisdictions. Encapsulating basic arithmetic into polymorphic strategies adds needless cognitive load and memory overhead for zero benefit.
</details>

### Exercise 5: Refactor in TypeScript (Medium)
Refactor an image resizing pipeline that switches on compression formats (`JPEG`, `PNG`, `WEBP`) using TypeScript interfaces.
<details>
<summary><strong>Solution</strong></summary>

```typescript
interface ImageCompressor {
    compress(rawBytes: Uint8Array): Uint8Array;
}

class WebpCompressor implements ImageCompressor {
    compress(raw: Uint8Array): Uint8Array { return /* webp logic */ raw; }
}

class JpegCompressor implements ImageCompressor {
    compress(raw: Uint8Array): Uint8Array { return /* jpeg logic */ raw; }
}

class ImageProcessor {
    process(raw: Uint8Array, compressor: ImageCompressor): Uint8Array {
        // resize pixels (stable)
        return compressor.compress(raw); // volatile compression format
    }
}
```
</details>

### Exercise 6: Complete Python Protocol Strategy (Hard)
Implement a dynamic rate-limiting strategy in Python where the throttle algorithm (Token Bucket vs Fixed Window) is encapsulated behind a `typing.Protocol`.
<details>
<summary><strong>Solution</strong></summary>

```python
from typing import Protocol
import time

class RateLimitAlgorithm(Protocol):
    def allow_request(self, client_id: str) -> bool: ...

class FixedWindowAlgorithm:
    def __init__(self, limit: int):
        self._limit = limit
        self._counts: dict[str, int] = {}

    def allow_request(self, client_id: str) -> bool:
        current = self._counts.get(client_id, 0)
        if current >= self._limit:
            return False
        self._counts[client_id] = current + 1
        return True

class ApiGateway:
    def __init__(self, limiter: RateLimitAlgorithm):
        self._limiter = limiter

    def handle_request(self, client_id: str) -> str:
        if not self._limiter.allow_request(client_id):
            return "429 Too Many Requests"
        return "200 OK"
```
</details>

---

## 16 Summary / Cheat Sheet

```
+-----------------------------------------------------------------------------------------+
| ENCAPSULATE WHAT VARIES CHEAT SHEET                                                     |
| Principle: Separate volatile mechanisms from stable policy.                             |
|                                                                                         |
| Action Steps:                                                                           |
| 1. Find the code that changes frequently (switches, external rules, vendor APIs).       |
| 2. Extract that code into a separate class or functional strategy.                      |
| 3. Define an abstract interface representing the domain operation.                      |
| 4. Inject the strategy into the stable policy workflow.                                 |
|                                                                                         |
| Invariant: Changes in volatile details must never force edits to stable policies.       |
+-----------------------------------------------------------------------------------------+
```

---

## 17 Further Reading

- Gamma, Helm, Johnson, Vlissides (1994). *"Design Patterns"*, Addison-Wesley (Chapter 1: "Encapsulate the concept that varies").
- Freeman, Eric; Robson, Elisabeth (2020). *"Head First Design Patterns"*, 2nd Edition, O'Reilly Media.
- Beck, Kent (2007). *"Implementation Patterns"*, Addison-Wesley.

### In Your Language
| Language Track | Deep Dive Reference |
|---|---|
| Java | [Java Strategy & Functional Interfaces](file:///e:/Learning/engineering-notes/01-languages/java/24-design-patterns/README.md) |
| TypeScript | [TypeScript Functional Patterns](file:///e:/Learning/engineering-notes/01-languages/typescript/17-design-patterns/README.md) |
| Python | [Python Higher-Order Functions](file:///e:/Learning/engineering-notes/01-languages/python/23-design-patterns/README.md) |

---
[← Previous: 05 - Abstraction and Information Hiding](05-abstraction-and-information-hiding.md) | [Section Index](README.md) | [Next: 07 - Immutability and Value Objects →](07-immutability-and-value-objects.md)
