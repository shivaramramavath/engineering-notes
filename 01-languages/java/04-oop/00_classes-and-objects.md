# Classes and Objects in Java

> An authoritative guide to Java classes, heap instance allocation, object memory layout (Mark Word, Klass Pointer, Compressed OOPs), and instance lifecycle.

| Metadata | Details |
| :--- | :--- |
| **Level** | Beginner to Intermediate |
| **Prerequisites** | [01_variables-and-data-types.md](../01-fundamentals/01_variables-and-data-types.md) |
| **Time to Read** | 25 minutes |
| **Status** | Active |
| **Java Version** | Java 8+ (Java 25 LTS verified) |

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
9. [Comparison: Class vs Object vs Record](#9-comparison-class-vs-object-vs-record)
10. [Interview Questions](#10-interview-questions)
11. [Practice Exercises](#11-practice-exercises)
12. [Summary & Cheat Sheet](#12-summary--cheat-sheet)
13. [Further Reading](#13-further-reading)

---

## 1. What & Why

Object-Oriented Programming (OOP) in Java models real-world business domains and software abstractions as collaborative entities possessing **state** (fields) and **behavior** (methods).

- A **Class** is the blueprint or prototype defined in source code. It specifies the structure of data fields, constructors, methods, and access contracts. In the JVM, a class is loaded by a `ClassLoader` and represented in Metaspace as a `Klass` metadata structure and a runtime `java.lang.Class` instance.
- An **Object** (or instance) is a concrete, dynamically allocated region of memory created on the JVM garbage-collected heap using the `new` operator.

Every non-primitive data type in Java is an object that inherits directly or indirectly from `java.lang.Object`.

---

## 2. Core Concepts

### 2.1 Class Structure Anatomy
A Java class definition consists of:
```java
package com.example.model;

import java.time.Instant;

public class BankAccount {
    // 1. Static (Class-level) Fields
    private static final double MIN_BALANCE = 25.0;

    // 2. Instance (Object-level) Fields
    private final String accountNumber;
    private double balance;
    private Instant createdAt;

    // 3. Constructors
    public BankAccount(String accountNumber, double initialDeposit) {
        if (initialDeposit < MIN_BALANCE) {
            throw new IllegalArgumentException("Deposit below minimum threshold");
        }
        this.accountNumber = accountNumber;
        this.balance = initialDeposit;
        this.createdAt = Instant.now();
    }

    // 4. Instance Methods (Behavior)
    public void deposit(double amount) {
        if (amount <= 0) throw new IllegalArgumentException("Amount must be positive");
        this.balance += amount;
    }

    // 5. Getters
    public String getAccountNumber() { return accountNumber; }
    public double getBalance() { return balance; }
}
```

### 2.2 The Reference Variable vs The Heap Object
When you execute:
```java
BankAccount acc = new BankAccount("ACC-9001", 500.0);
```
Two distinct memory allocations occur:
1. **Reference Variable (`acc`):** Allocated in the current thread's stack frame (4 bytes with Compressed OOPs or 8 bytes uncompressed). It stores the 64-bit memory address pointing to the heap object.
2. **Heap Instance:** Allocated in the JVM Eden space. It contains the object header and the actual instance fields (`accountNumber`, `balance`, `createdAt`).

---

## 3. How It Works Internally

### 3.1 HotSpot Object Memory Layout (64-bit JVM)
Every object on the HotSpot JVM heap is laid out in contiguous bytes conforming to 8-byte boundary alignment:

```
┌────────────────────────────────────────────────────────┐
│                   Java Object Header                   │
├──────────────────────────┬─────────────────────────────┤
│ Mark Word                │ 8 bytes (64 bits)           │
│ Compressed Klass Pointer │ 4 bytes (with +UseCompressedClassPointers)
├──────────────────────────┼─────────────────────────────┤
│                    Instance Fields Data                │
├──────────────────────────┼─────────────────────────────┤
│ Field 1: double balance  │ 8 bytes (primitive double)  │
│ Field 2: accountNumber   │ 4 bytes (compressed ref)    │
│ Field 3: createdAt       │ 4 bytes (compressed ref)    │
├──────────────────────────┼─────────────────────────────┤
│ Alignment Padding        │ 4 bytes (rounds to 8-byte   │
│                          │ multiple)                   │
└──────────────────────────┴─────────────────────────────┘
  Total Object Size on Heap: 32 bytes
```

#### Detailed Header Breakdown:
1. **Mark Word (8 bytes):**
   - Stores runtime metadata: identity hash code (31 bits), GC generational age (4 bits: 0–15), lock synchronization flags (biased, thin/fat lock, CAS spin locks), and GC metadata.
2. **Klass Word (4 bytes with Compressed Class Pointers):**
   - A pointer directly to the class metadata structure in native Metaspace (`InstanceKlass`), allowing the JVM to resolve virtual method tables (`vtable`) during dynamic dispatch.
3. **Instance Fields:**
   - Primitives and object references. In HotSpot, fields are packed and reordered to minimize alignment gaps (doubles/longs first, then ints/floats, shorts/chars, bytes/booleans, and object references).
4. **Padding:**
   - JVM heaps enforce 8-byte word alignment. If the sum of headers and fields is not a multiple of 8, empty padding bytes are appended.

---

## 4. Syntax & Usage

### 4.1 Instantiation and Initialization
```java
// Instantiation using 'new' operator
User user1 = new User("Alice", "alice@corp.com");

// Method invocation using dot notation (.)
user1.sendNotification("Welcome to the platform!");

// Multiple references pointing to the SAME object
User alias = user1;
alias.setEmail("new@corp.com");
System.out.println(user1.getEmail()); // Prints "new@corp.com"!

// Garbage Collection Eligibility
user1 = null;
alias = null; // Heap object is now unreachable and eligible for GC collection
```

---

## 5. Practical Examples

### Example 1: Basic Class Definition and Encapsulation
```java
public class Product {
    private final String sku;
    private String name;
    private int stockQuantity;

    public Product(String sku, String name, int initialStock) {
        if (sku == null || sku.isBlank()) throw new IllegalArgumentException("SKU cannot be blank");
        this.sku = sku;
        this.name = name;
        this.stockQuantity = Math.max(0, initialStock);
    }

    public boolean adjustStock(int delta) {
        if (stockQuantity + delta < 0) {
            return false; // Insufficient stock
        }
        stockQuantity += delta;
        return true;
    }

    public String getSku() { return sku; }
    public String getName() { return name; }
    public int getStockQuantity() { return stockQuantity; }

    public static void main(String[] args) {
        Product p = new Product("KB-102", "Ergonomic Keyboard", 5);
        boolean success = p.adjustStock(-2);
        System.out.println("Stock updated: " + success + ", Current: " + p.getStockQuantity());
    }
}
```
**Output:**
```text
Stock updated: true, Current: 3
```

### Example 2: Real-World Domain Entity with State Machine
```java
public class OrderAggregate {
    public enum OrderStatus { PENDING, PAID, SHIPPED, CANCELLED }

    private final String orderId;
    private OrderStatus status;
    private double totalAmount;

    public OrderAggregate(String orderId, double totalAmount) {
        this.orderId = orderId;
        this.totalAmount = totalAmount;
        this.status = OrderStatus.PENDING;
    }

    public void markPaid() {
        if (this.status != OrderStatus.PENDING) {
            throw new IllegalStateException("Only PENDING orders can be paid. Current: " + this.status);
        }
        this.status = OrderStatus.PAID;
    }

    public void markShipped() {
        if (this.status != OrderStatus.PAID) {
            throw new IllegalStateException("Cannot ship order that is not PAID. Current: " + this.status);
        }
        this.status = OrderStatus.SHIPPED;
    }

    public OrderStatus getStatus() { return status; }
    public String getOrderId() { return orderId; }

    public static void main(String[] args) {
        OrderAggregate order = new OrderAggregate("ORD-8921", 149.99);
        System.out.println("Initial status: " + order.getStatus());
        order.markPaid();
        System.out.println("After payment:  " + order.getStatus());
        order.markShipped();
        System.out.println("After shipping: " + order.getStatus());
    }
}
```
**Output:**
```text
Initial status: PENDING
After payment:  PAID
After shipping: SHIPPED
```

### Example 3: Edge Case – Reference Aliasing and Side Effects
```java
public class AliasingHazard {
    static class MutablePoint {
        int x, y;
        MutablePoint(int x, int y) { this.x = x; this.y = y; }
    }

    public static void mutate(MutablePoint p) {
        p.x = 999; // Mutates original heap object!
    }

    public static void main(String[] args) {
        MutablePoint pt = new MutablePoint(10, 20);
        System.out.println("Before: (" + pt.x + ", " + pt.y + ")");
        mutate(pt);
        System.out.println("After:  (" + pt.x + ", " + pt.y + ")");
    }
}
```
**Output:**
```text
Before: (10, 20)
After:  (999, 20)
```

### Example 4: Failure Case – Uninitialized Reference Variables
```java
public class NullReferenceTrap {
    static class Customer {
        String name; // defaults to null
    }

    public static void main(String[] args) {
        Customer c = new Customer();
        try {
            // NullPointerException: c.name is null!
            int len = c.name.length();
        } catch (NullPointerException e) {
            System.out.println("Caught expected NPE on uninitialized field access!");
        }
    }
}
```
**Output:**
```text
Caught expected NPE on uninitialized field access!
```

### Example 5: Optimized / Idiomatic – Factory Method with Private Constructor
```java
public final class Money {
    private final long cents; // Stored as integer to avoid float rounding errors
    private final String currency;

    private Money(long cents, String currency) {
        this.cents = cents;
        this.currency = currency;
    }

    // Static Factory Method
    public static Money of(double dollars, String currency) {
        if (dollars < 0) throw new IllegalArgumentException("Amount cannot be negative");
        long cents = Math.round(dollars * 100);
        return new Money(cents, currency);
    }

    public Money add(Money other) {
        if (!this.currency.equals(other.currency)) {
            throw new IllegalArgumentException("Currency mismatch: " + this.currency + " vs " + other.currency);
        }
        return new Money(this.cents + other.cents, this.currency);
    }

    @Override
    public String toString() {
        return String.format("%s %.2f", currency, cents / 100.0);
    }

    public static void main(String[] args) {
        Money m1 = Money.of(19.99, "USD");
        Money m2 = Money.of(5.01, "USD");
        System.out.println("Sum: " + m1.add(m2));
    }
}
```
**Output:**
```text
Sum: USD 25.00
```

---

## 6. Best Practices

1. **Always Validate Invariants in Constructors:** Never allow an object to be constructed in an invalid or half-initialized state. Check arguments eagerly.
2. **Make Fields `final` by Default:** Declare every field `final` unless state mutation is an explicit requirement. Immutability eliminates concurrency bugs.
3. **Prefer Static Factory Methods Over Overloaded Constructors:** Static factory methods (`Money.of(...)`, `User.createAnonymous()`) have clear domain names, unlike overloaded constructors.
4. **Encapsulate Mutable State:** Never expose internal mutable collections (e.g. `ArrayList`) via public getters; return unmodifiable copies (`List.copyOf(items)`).
5. **Beware Object Header Overhead for Tiny Objects:** An object holding a single `int` takes 16 to 24 bytes on the heap (12B header + 4B int + padding). If modeling millions of primitive coordinates, consider arrays of primitives.

---

## 7. Common Mistakes & Pitfalls

| Mistake | Why It Is Wrong | Correct Idiom |
| :--- | :--- | :--- |
| Exposing public fields | Violates encapsulation; external classes can corrupt state without validation. | Make fields `private`, expose controlled methods. |
| Over-reliance on Getters/Setters | Creates "anemic domain models" with no business logic inside the class. | Implement behavior methods (`order.cancel()`) instead of naked setters. |
| Forgetting default values of fields | Numeric fields default to `0`, booleans to `false`, references to `null`. | Initialize explicitly or check for `null`. |
| Comparing object references with `==` | Tests whether two pointers point to the exact same memory address. | Override and use `.equals()`. |
| Uncontrolled reference leakage | Returning internal mutable arrays or Date/Calendar objects allows callers to mutate internal state. | Return defensive copies (`array.clone()`). |
| Instantiating unnecessary temporary objects in loops | Creates heap churn and triggers frequent Young Generation GC pauses. | Reuse objects or use primitive types where feasible. |

---

## 8. Performance & Security Notes

### 8.1 Compressed OOPs (Ordinary Object Pointers)
On 64-bit systems with heaps smaller than 32GB, HotSpot automatically enables `-XX:+UseCompressedOops`. Because object memory addresses are aligned on 8-byte boundaries, the last 3 bits of any object address are always `000`. HotSpot shifts 32-bit integer pointers by 3 bits to address up to:
$$2^{32} \times 8 \text{ bytes} = 32 \text{ GB}$$
This allows 32-bit pointer efficiency (4 bytes per reference instead of 8 bytes) on 64-bit JVMs!

---

## 9. Comparison: Class vs Object vs Record

| Feature | Class | Object | Record (Java 16+) |
| :--- | :--- | :--- | :--- |
| **Definition** | Blueprint in code | Concrete heap instance | Transparent immutable data carrier |
| **State** | Mutable or immutable | Live data in memory | Strictly immutable (`final` fields) |
| **Boilerplate** | Manual getters, equals, toString| N/A | Automatic getters, equals, toString |
| **Inheritance** | Supports `extends` | N/A | Cannot extend other classes (`final`) |
| **Metaspace** | `InstanceKlass` metadata | N/A | `InstanceKlass` extending `Record` |

---

## 10. Interview Questions

<details>
<summary><strong>1. What is the memory layout of a Java object on a 64-bit JVM?</strong></summary>

A Java object on a 64-bit JVM with Compressed OOPs enabled consists of:
1. **Mark Word (8 bytes):** Stores hashcode, GC age (4 bits), lock status flags.
2. **Klass Pointer (4 bytes):** Points to class metadata in Metaspace.
3. **Instance Fields:** Primitive data and 4-byte compressed references to other objects.
4. **Padding (0-7 bytes):** Added to ensure the total object size is a multiple of 8 bytes.
</details>

<details>
<summary><strong>2. What is Compressed OOPs, and why is 32GB heap size a critical threshold?</strong></summary>

Compressed OOPs (`-XX:+UseCompressedOops`) represents 64-bit heap addresses using 32-bit pointers by exploiting 8-byte alignment (shifting right by 3 bits). This works up to $2^{32} \times 8 = 32\text{ GB}$. If the JVM heap exceeds 32GB, Compressed OOPs must be disabled, forcing all references to occupy 8 bytes, which increases heap consumption by 30-40% for the exact same data.
</details>

<details>
<summary><strong>3. What is the difference between a class and an object?</strong></summary>

A class is compile-time source code defining fields, methods, and constructors, loaded into Metaspace as metadata. An object is a runtime instance allocated on the JVM heap via `new`, containing concrete field values and an object header.
</details>

<details>
<summary><strong>4. What is the difference between pass-by-value and reference aliasing?</strong></summary>

Java is strictly **pass-by-value**. When an object reference is passed to a method, the *pointer value* (memory address) is copied. Both the caller and the callee have distinct reference variables pointing to the same underlying heap object. Mutating object fields inside the method affects the shared heap object (aliasing), but reassigning the parameter variable does not alter the caller's reference.
</details>

<details>
<summary><strong>5. What default values do instance fields receive if not explicitly initialized?</strong></summary>

- `byte`, `short`, `int`, `long`: `0`
- `float`, `double`: `0.0`
- `char`: `'\u0000'` (null character)
- `boolean`: `false`
- Object references: `null`
</details>

<details>
<summary><strong>6. What is an Anemic Domain Model, and why is it considered an OOP anti-pattern?</strong></summary>

An Anemic Domain Model is a class that contains only private fields with public getters and setters and zero business logic. It separates state from behavior, treating objects as passive data structures and scattering business rules across external service classes, defeating the core OOP principle of encapsulation.
</details>

<details>
<summary><strong>7. Why are static factory methods often preferred over constructors?</strong></summary>

1. They have descriptive names (e.g. `Duration.ofMinutes(5)` vs `new Duration(300)`).
2. They are not required to create a new object on every call (can return cached/pooled instances like `Boolean.valueOf()`).
3. They can return a subtype of the declared return type.
4. They can vary the returned implementation based on input arguments (e.g., `EnumSet.noneOf()`).
</details>

<details>
<summary><strong>8. Where do static fields reside in modern Java memory?</strong></summary>

In Java 8 and later, static fields of a class are stored inside the `java.lang.Class` object instance, which resides directly on the **regular Java Heap**, not in native Metaspace. Metaspace stores class metadata, bytecode, and method structures.
</details>

---

## 11. Practice Exercises

### Easy
1. **Rectangle Entity:** Implement a `Rectangle` class with `width` and `height`, validating positive dimensions and calculating area and perimeter.
2. **Counter Class:** Build a thread-unsafe `Counter` class with `increment()`, `decrement()`, and `getValue()`.
3. **Point2D Distance:** Create a `Point2D` class with a method `double distanceTo(Point2D other)`.

### Medium
4. **Immutable Student Record:** Build an immutable class representing a student and course grades with defensive copies.
5. **Bank Account State Machine:** Expand `BankAccount` to handle `freeze()`, `unfreeze()`, and prevent withdrawals on frozen accounts.
6. **Object Size Estimator:** Calculate theoretical heap memory consumption for a class with 2 `int`s, 1 `double`, and 1 `String` reference under 64-bit Compressed OOPs.

### Hard
7. **JOL (Java Object Layout) Inspector:** Write a Java program using reflection or Unsafe to inspect the exact byte offsets of fields in a class.
8. **Flyweight Object Cache:** Implement a flyweight cache for instances of a class with a private constructor and bounded memory.
9. **Domain Event Publisher Entity:** Create a base domain aggregate class that records and dispatches domain events on state transitions.

<details>
<summary><strong>Exercise Solutions</strong></summary>

```java
// Solution to Exercise 1: Rectangle Entity
public final class Rectangle {
    private final double width;
    private final double height;

    public Rectangle(double width, double height) {
        if (width <= 0 || height <= 0) {
            throw new IllegalArgumentException("Dimensions must be positive");
        }
        this.width = width;
        this.height = height;
    }

    public double area() { return width * height; }
    public double perimeter() { return 2 * (width + height); }
}

// Solution to Exercise 6: Object Size Calculation
// Mark Word: 8 bytes
// Klass Pointer: 4 bytes
// double: 8 bytes
// int 1: 4 bytes
// int 2: 4 bytes
// String ref: 4 bytes
// Total fields + header: 8 + 4 + 8 + 4 + 4 + 4 = 32 bytes
// Padding: 0 bytes (32 is multiple of 8)
// Result: Exactly 32 bytes on heap.
```
</details>

---

## 12. Summary & Cheat Sheet

```
┌────────────────────────────────────────────────────────────────────────┐
│                      CLASSES & OBJECTS CHEAT SHEET                     │
├────────────────────────────────────────────────────────────────────────┤
│ Class:              Metaspace InstanceKlass blueprint + static fields  │
│ Object:             Contiguous heap block (Eden / Old generation)      │
│ 64-bit Header:      8B Mark Word (hash, GC age, lock) + 4B Klass Ptr   │
│ Compressed OOPs:    32-bit references on heaps < 32GB (shifts 3 bits)  │
│ Alignment:          All heap objects padded to multiples of 8 bytes    │
│ Fields:             Default to 0, false, or null                       │
│ Static Methods:     Class-scoped, cannot access `this` or instance vars│
│ Static Fields:      Stored on Java Heap inside Class object mirror     │
│ Best Practice:      Validate in constructor; prefer immutability       │
└────────────────────────────────────────────────────────────────────────┘
```

---

## 13. Further Reading
- Official Java 25 Language Specification: Classes (Chapter 8)
- OpenJDK HotSpot Internals: Object Layout and Mark Word Specification
- *Effective Java (3rd Edition)*, Joshua Bloch, Item 1: "Consider static factory methods instead of constructors".
