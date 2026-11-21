# Refactoring and Code Smells in Java

> Identifying common architectural code smells (God Classes, Feature Envy, Primitive Obsession, Long Parameter Lists) and applying standard Martin Fowler refactoring patterns in modern Java 17/21/25.

| Metadata | Details |
| :--- | :--- |
| **Level** | Core Engineering / Software Craftsmanship |
| **Prerequisites** | [00_solid-principles](00_solid-principles.md), [02_clean-code](02_clean-code.md) |
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

### What is Refactoring?
Refactoring is the disciplined technique of restructuring an existing body of code—altering its internal structure without changing its external observable behavior.

Martin Fowler defines it:
> *"Refactoring is a controlled technique for improving the design of an existing codebase. Its essence is applying a series of small behavior-preserving transformations."*

### What is a Code Smell?
A code smell is a surface indication that usually corresponds to a deeper architectural or structural flaw in a system. Smells are not bugs—the program runs and passes tests—but they slow down development, increase cognitive load, and heighten the risk of future defects.

Common Java Code Smells:
1. **Primitive Obsession**: Using raw primitives (`String`, `long`, `double`) for domain concepts (Email, Money, ISBN).
2. **Feature Envy**: A method that seems more interested in the data of another class than the class it belongs to.
3. **God Class / Large Class**: A massive class aggregating dozens of disparate responsibilities.
4. **Data Clumps**: The same 3 or 4 variables (e.g. `street`, `city`, `zip`) constantly appearing together across multiple classes.
5. **Switch Statement Smells**: Repetitive `switch` or `if-else` branching on type codes scattered across multiple methods.

---

## 2. Core Concepts

### The Refactoring Cycle (Red-Green-Refactor)
Refactoring must never be conducted without a safety net:
1. **Write Unit Tests**: Ensure 100% test coverage over the target method before touching its structure.
2. **Small Incremental Steps**: Apply one transformation (e.g. Extract Method), compile, and run tests.
3. **Never Add Features While Refactoring**: Keep behavioral changes and structural refactorings in separate commits.

### Primitive Obsession vs. Domain Value Objects
Using raw primitives loses type safety:
```java
// Primitive Obsession (Bug Waiting to Happen):
public void transferMoney(long fromAccount, long toAccount, double amount, String currency) {}
// transferMoney(105, 204, 100.0, "USD") -> Arguments easily swapped by mistake!
```
By wrapping primitives into immutable Java records (`AccountId`, `Money`), the compiler prevents accidental parameter swapping.

---

## 3. How It Works Internally

### Replace Conditional with Polymorphism (Sealed Interfaces)
In legacy codebases, type codes dictate behavior via fragile switches:

```
[ Type Code: Enum ] ---> switch(type) ---> Case A: doA()
                                      ---> Case B: doB()
                                      ---> Case C: doC()
```
When a new type is added, developers must hunt through 10 files to find every switch statement.

In Modern Java:
```
[ Sealed Interface Shape ]
        |
        +-- permits Circle, Square, Triangle
        |
        v
Method area() overridden in each Record! (Polymorphic dispatch)
```
Dispatch occurs through the virtual method table (`vtable`) or exhaustive pattern matching, enforced by the compiler.

---

## 4. Syntax & Usage

### 1. Extract Method Refactoring
Turn an explanatory comment into a self-documenting method name:
```java
// BEFORE:
void printOwing() {
    printBanner();
    // print details
    System.out.println("name: " + name);
    System.out.println("amount: " + getOutstanding());
}

// AFTER:
void printOwing() {
    printBanner();
    printDetails(getOutstanding());
}
private void printDetails(double outstanding) {
    System.out.println("name: " + name);
    System.out.println("amount: " + outstanding);
}
```

---

## 5. Practical Examples

### Example 1: Refactoring Primitive Obsession to Value Records
**Goal:** Eliminate primitive types for monetary calculations and currencies to guarantee compile-time safety and encapsulation.

**Before (Primitive Obsession):**
```java
public class PaymentService {
    // Dangerous: doubles suffer from floating-point inaccuracy; currency is unvalidated string
    public void processPayment(long userId, double amount, String currencyCode) {
        if (amount <= 0) throw new IllegalArgumentException("Negative amount");
        if (!currencyCode.equals("USD") && !currencyCode.equals("EUR")) throw new IllegalArgumentException();
        // execute payment...
    }
}
```

**After (Clean Domain Value Objects):**
```java
import java.math.BigDecimal;
import java.util.Currency;
import java.util.Objects;

public record Money(BigDecimal amount, Currency currency) {
    public Money {
        Objects.requireNonNull(amount, "Amount cannot be null");
        Objects.requireNonNull(currency, "Currency cannot be null");
        if (amount.compareTo(BigDecimal.ZERO) <= 0) {
            throw new IllegalArgumentException("Amount must be positive");
        }
    }

    public static Money of(String amount, String currencyCode) {
        return new Money(new BigDecimal(amount), Currency.getInstance(currencyCode));
    }
}

public record UserId(long value) {
    public UserId {
        if (value <= 0) throw new IllegalArgumentException("UserId must be positive");
    }
}

public class PaymentService {
    // Impossible to swap arguments; invariants guaranteed at compile/construction time!
    public void processPayment(UserId userId, Money money) {
        // execute payment...
    }
}
```

### Example 2: Refactoring Feature Envy
**Goal:** A method in `CustomerReport` accesses 5 fields of `Address`. Move the logic to where the data lives.

**Before (Feature Envy):**
```java
public class CustomerReport {
    // Smells of Feature Envy: Accesses address extensively
    public String formatAddressLabel(Customer customer) {
        Address addr = customer.getAddress();
        return addr.getStreet() + "\n" + addr.getCity() + ", " + addr.getState() + " " + addr.getZip();
    }
}
```

**After (Move Method to Address):**
```java
public record Address(String street, String city, String state, String zip) {
    public String toLabel() {
        return street + "\n" + city + ", " + state + " " + zip;
    }
}

public class CustomerReport {
    public String formatAddressLabel(Customer customer) {
        return customer.getAddress().toLabel(); // Clean delegation
    }
}
```

### Example 3: Replacing Conditional with Polymorphism
**Goal:** Refactor repetitive `switch` statements calculating insurance premiums.

```java
// Sealed Hierarchy replacing Type-code Switch
public sealed interface Vehicle permits Car, Truck, Motorcycle {
    double calculateAnnualPremium();
}

public record Car(int ageYears) implements Vehicle {
    @Override public double calculateAnnualPremium() { return 500.0 + (ageYears * 20); }
}

public record Truck(double cargoCapacityTons) implements Vehicle {
    @Override public double calculateAnnualPremium() { return 1200.0 + (cargoCapacityTons * 150); }
}

public record Motorcycle(int engineCc) implements Vehicle {
    @Override public double calculateAnnualPremium() { return 800.0 + (engineCc > 600 ? 400 : 0); }
}
```

---

## 6. Best Practices

1. **Have Tests First**: Never begin refactoring code without automated test coverage.
2. **Take Micro-Steps**: Apply one small refactoring pattern at a time; commit often.
3. **Encapsulate Primitives**: Use Java records to turn loose primitives into rich domain concepts.
4. **Move Logic Close to Data**: Eliminate Feature Envy by moving calculation methods into the class owning the data.
5. **Separate Refactoring from New Features**: Never refactor code in the same Git commit that adds new business capabilities.
6. **Use IDE Automated Refactoring Tools**: Leverage IntelliJ/Eclipse AST-safe refactoring shortcuts (Extract Method, Inline, Rename, Change Signature).

---

## 7. Common Mistakes & Pitfalls

| Mistake | Why It Fails | Secure Fix |
| :--- | :--- | :--- |
| **Refactoring without tests** | Introduces silent regression bugs that are hard to trace. | Write comprehensive unit tests before changing structure. |
| **Mixing feature work with refactoring** | Code review becomes impossible; rollbacks discard both features and cleanup. | Commit refactoring separately from feature branches. |
| **Giant single refactoring PRs** | Merging 2,000-line refactoring PRs causes massive merge conflicts. | Break into small, digestible, self-contained PRs. |
| **Over-extracting 1-line helper methods** | Fragmenting simple logic into 10 disjointed files increases cognitive overhead. | Inline methods if they do not provide conceptual abstraction. |
| **Ignoring Primitive Obsession** | Swapping two `long` or `String` arguments compiles cleanly but corrupts data. | Wrap domain types in lightweight Java records. |
| **Refactoring working legacy code with zero business need** | Wastes engineering cycles without delivering value. | Follow the Boy Scout Rule: Refactor only when modifying that code for new features. |

---

## 8. Performance & Security Notes

### Zero Overhead of Record Wrappers
Developers often hesitate to wrap primitives (`long userId`) in records (`record UserId(long id)`) fearing garbage collection overhead.
- In HotSpot, modern C2 JIT performs **Escape Analysis**.
- Small records that do not escape the local method execution are **scalar replaced**: HotSpot eliminates the heap allocation entirely and maps the record components directly to CPU registers!

---

## 9. Comparison / When to Use What

| Code Smell | Recommended Refactoring Pattern | Key Benefit |
| :--- | :--- | :--- |
| **Primitive Obsession** | Replace Primitive with Object / Record | Compile-time type safety & validation |
| **Feature Envy** | Move Method / Move Field | Restores encapsulation and cohesion |
| **Long Method** | Extract Method / Decompose Conditional | Improves readability and unit testability |
| **Data Clumps** | Extract Class / Introduce Parameter Object | Groups related fields into reusable records |
| **Type-code Switch** | Replace Conditional with Polymorphism | Closed for modification, open for extension |

---

## 10. Interview Questions & Answers

### Q1: What is "Primitive Obsession" and why is it considered a serious code smell?
<details>
<summary>View Answer</summary>
Primitive Obsession is the antipattern of using basic primitive types (`String`, `Integer`, `long`, `double`) to represent domain concepts (such as `EmailAddress`, `Money`, `ZipCode`, `OrderId`). It is serious because primitives cannot enforce business invariants (a `String` can hold null, an empty string, or malicious SQL), and methods taking multiple primitives of the same type (`transfer(long from, long to)`) are prone to argument swapping bugs that compile without error.
</details>

### Q2: What is "Feature Envy" and how do you resolve it?
<details>
<summary>View Answer</summary>
Feature Envy occurs when a method in class A accesses the getters and internal fields of class B more frequently than its own data to perform a calculation. It indicates that the method is in the wrong place. The resolution is the **Move Method** refactoring: move the method into class B where the data actually lives, and have class A simply delegate to it.
</details>

### Q3: What is the "Boy Scout Rule" in software development?
<details>
<summary>View Answer</summary>
"Always leave the campground cleaner than you found it." In software engineering, this means that whenever an engineer touches a module or file to fix a bug or add a feature, they should make a small, incremental improvement—such as renaming an obscure variable, extracting a long method, or removing dead code. Over time, the codebase organically improves rather than decays.
</details>

### Q4: How does Escape Analysis make wrapping primitives in Java Records zero-cost?
<details>
<summary>View Answer</summary>
Escape Analysis is an optimization in the HotSpot C2 compiler that analyzes whether an object\'s reference escapes outside the method that created it. If an immutable record (like `UserId(105)`) is created and consumed locally, HotSpot applies **Scalar Replacement**: it breaks the record into its individual primitive fields, storing them in CPU registers instead of allocating heap memory. This provides type safety at compile time with zero garbage collection overhead at runtime.
</details>

### Q5: What is a "Data Clump"?
<details>
<summary>View Answer</summary>
A data clump is a group of variables that frequently appear together in method signatures and class fields (e.g. `street`, `city`, `zipCode` or `startDate`, `endDate`). When these variables travel together across multiple classes, it indicates they form a conceptual whole. The fix is to apply **Extract Class** or **Introduce Parameter Object**, consolidating them into a cohesive record (e.g. `Address` or `DateRange`).
</details>

### Q6: Why must refactoring be kept strictly separate from adding new features?
<details>
<summary>View Answer</summary>
Mixing refactoring with feature development violates the principle of isolated changes. If a bug or regression occurs, it is difficult to determine whether the fault lies in the behavioral feature logic or the structural refactoring. Furthermore, pull requests that combine massive renames with feature code are nearly impossible for peers to review effectively.
</details>

### Q7: What is the "Replace Conditional with Polymorphism" refactoring?
<details>
<summary>View Answer</summary>
It is the process of replacing complex `switch` or `if-else` chains that inspect type codes with an object-oriented hierarchy. Each branch of the conditional is moved into an overriding method of a subclass or record implementing a shared interface. Adding a new type in the future requires only creating a new class rather than finding and modifying every switch statement across the codebase.
</details>

### Q8: What role do automated tests play in refactoring?
<details>
<summary>View Answer</summary>
Automated unit and integration tests provide the indispensable safety net that enables refactoring. Because refactoring by definition must preserve external behavior, running tests after every micro-transformation provides instant verification that the restructuring has not broken existing functionality. Refactoring without tests is reckless guessing.
</details>

---

## 11. Practice Exercises

### Exercise 1: Eliminate Data Clump into Record
**Task:** Identify the data clump in the method below and refactor it into an immutable record:
```java
public void scheduleAppointment(String patientName, LocalDate startDate, LocalDate endDate, LocalTime startTime, LocalTime endTime) {}
```
<details>
<summary>View Solution</summary>

```java
import java.time.LocalDate;
import java.time.LocalTime;

public record TimeWindow(
    LocalDate startDate,
    LocalDate endDate,
    LocalTime startTime,
    LocalTime endTime
) {
    public TimeWindow {
        if (endDate.isBefore(startDate)) {
            throw new IllegalArgumentException("End date cannot precede start date");
        }
    }
}

public class AppointmentScheduler {
    public void scheduleAppointment(String patientName, TimeWindow window) {
        // Clean and decoupled
    }
}
```
</details>

### Exercise 2: Refactor Feature Envy
**Task:** Refactor the calculation in `OrderPrinter` so that it lives on the `Order` class.
```java
// BEFORE:
public class OrderPrinter {
    public double calculateTotalWithTax(Order order) {
        return (order.getSubtotal() - order.getDiscount()) * 1.15;
    }
}
```
<details>
<summary>View Solution</summary>

```java
// AFTER: Move method to Order
public record Order(double subtotal, double discount) {
    public double totalWithTax() {
        return (subtotal - discount) * 1.15;
    }
}

public class OrderPrinter {
    public void print(Order order) {
        System.out.println("Total: " + order.totalWithTax());
    }
}
```
</details>

### Exercise 3: Refactor Switch on Type Code
**Task:** Refactor an enum-based discount calculation to polymorphism using a sealed interface.
<details>
<summary>View Solution</summary>

```java
public sealed interface CustomerDiscount permits RegularDiscount, VipDiscount, EmployeeDiscount {
    double apply(double basePrice);
}

public record RegularDiscount() implements CustomerDiscount {
    @Override public double apply(double basePrice) { return basePrice; }
}

public record VipDiscount() implements CustomerDiscount {
    @Override public double apply(double basePrice) { return basePrice * 0.85; } // 15% off
}

public record EmployeeDiscount() implements CustomerDiscount {
    @Override public double apply(double basePrice) { return basePrice * 0.70; } // 30% off
}
```
</details>

---

## 12. Summary / Cheat Sheet

- **Refactoring Definition**: Changing internal structure without altering external behavior.
- **Safety First**: Never refactor without automated test coverage.
- **Primitive Obsession**: Wrap raw types in immutable Java records for type safety.
- **Feature Envy**: Move methods into the class that owns the data.
- **Data Clumps**: Group frequently paired arguments into domain parameter objects.
- **Polymorphism**: Replace type-code switch chains with sealed interfaces and records.

---

## 13. Further Reading
- Martin Fowler: [Refactoring: Improving the Design of Existing Code](https://martinfowler.com/books/refactoring.html)
- Refactoring Guru: [Code Smells Catalog](https://refactoring.guru/refactoring/smells)
- Effective Java: [Item 1: Consider static factory methods instead of constructors](https://www.oreilly.com/library/view/effective-java-3rd/9780134686097/)
- HotSpot Compiler Internals: [Escape Analysis and Scalar Replacement](https://wiki.openjdk.org/display/HotSpot/EscapeAnalysis)
