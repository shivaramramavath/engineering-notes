# Dependency Injection vs. Factories

> **One-line summary:** Inject long-lived stateless services directly via constructors (DI); inject a Factory only when the consumer needs to create *new* instances dynamically at runtime using runtime parameters or scoped lifetimes.

| Attribute | Specification |
|---|---|
| **Level** | Intermediate / Architectural |
| **Prerequisites** | [03-dependency-injection.md](../04-patterns-intro/03-dependency-injection.md), [02-factory-method.md](02-factory-method.md) |
| **Estimated Reading Time** | 25 minutes |
| **Status** | Complete |
| **Category** | Creational / Wiring |
| **Origin** | Modern Architecture & DI Practice |
| **Also Known As** | Inject Service vs Inject Factory, Abstract Factory + DI |
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

Developers learning DI and Factories often get confused about **which one to inject**:
1. **Over-Factorying:** Someone injects `UserRepositoryFactory` into `UserService` even though `UserService` only ever needs one stateless `UserRepository`!
2. **Under-Factorying (Lifetime/Runtime-Arg Trap):** Someone injects a single `ReportExporter` into `ExportJobManager`, when each incoming HTTP request specifies `"PDF"` or `"CSV"` dynamically at runtime—making a single startup-injected instance impossible!

---

## 2 Why It Exists

DI and Factories are **complementary, not mutually exclusive**:
- **DI** wires static/startup-known collaborators once in the Composition Root.
- **Factories** (injected via DI!) bridge the gap when **runtime inputs** (e.g. `tenantId`, `exportFormat`) or **short-lived per-operation lifetimes** require creating fresh instances *after* application startup.

---

## 3 What It Is: Decision Rule

| Need | Pattern to Use |
|---|---|
| Collaborator is stateless and known at startup (`OrderRepository`, `Clock`) | **Direct DI** (Inject `OrderRepository` directly) |
| Concrete type or constructor argument is only known at **runtime** (`format`, `userId`) | **Inject a Factory** (`Function<String, Exporter>` or `ExporterFactory`) |
| A Singleton needs a fresh short-lived/non-thread-safe instance per call | **Inject a `Supplier<T>` / Factory** (prevents Captive Dependency!) |

```mermaid
graph LR
    Root[Composition Root] -->|Injects Stateless Service| Svc1[Direct DI:\nOrderService(OrderRepo)]
    Root -->|Injects Factory Interface| Svc2[Factory + DI:\nExportService(ExporterFactory)]
    Svc2 -->|Runtime arg 'PDF'| Svc2
```

---

## 4 The Problem Without It

```java
// Trap 1: Injecting IoC ApplicationContext to resolve runtime type (Service Locator Anti-Pattern!)
// Trap 2: Injecting a single CsvExporter when runtime requests need PDF or JSON dynamically!
```

---

## 5 How It Solves It

### Java Implementation (Java 25 LTS — Injecting Functional Factory `Function<String, Exporter>`)

```java
package com.design.patterns.creational.divsfactory;

import java.util.Map;
import java.util.function.Function;

interface Exporter { String export(String data); }

class ExportService {
    private final Function<String, Exporter> exporterFactory;

    // DI injects the Factory! No IoC container reference needed!
    public ExportService(Function<String, Exporter> exporterFactory) {
        this.exporterFactory = exporterFactory;
    }

    public String runExport(String format, String data) {
        return exporterFactory.apply(format).export(data);
    }
}
```

### TypeScript Implementation (Node 24 Native)

```typescript
type Exporter = (data: string) => string;
type ExporterFactory = (format: string) => Exporter;

class ExportService {
    private readonly factory: ExporterFactory;
    constructor(factory: ExporterFactory) { this.factory = factory; }
    runExport(format: string, data: string): string {
        return this.factory(format)(data);
    }
}
```

### Python Implementation (Python 3.14)

```python
from typing import Callable

Exporter = Callable[[str], str]
ExporterFactory = Callable[[str], Exporter]

class ExportService:
    def __init__(self, factory: ExporterFactory) -> None:
        self._factory = factory

    def run_export(self, fmt: str, data: str) -> str:
        return self._factory(fmt)(data)
```

---

## 6 How It Works

Notice how injecting `Function<String, Exporter>` (or `ExporterFactory`) preserves **100% testability** (pass a 1-line lambda in tests) while still allowing dynamic runtime selection without `ApplicationContext.getBean()`!

---

## 7 Real-World Examples

1. **Java `java.util.function.Supplier<T>` / Jakarta `Provider<T>`**: Standard way for Singletons to request fresh transient instances on demand.
2. **Spring `ObjectProvider<T>`**: Safe factory injection avoiding circular or captive dependency bugs.
3. **Database Connection Factories (`DataSource.getConnection()`)**: `DataSource` is injected via DI; callers ask it for a connection per transaction.

---

## 8 When to Use and When NOT to Use

Never inject a Factory if you only ever call `factory.create()` once in your constructor—just inject the product directly!

---

## 9 Variations and Language Idioms

- **Java:** `Supplier<T>` (0 runtime args) or `Function<Arg, T>` (1 runtime arg).
- **TypeScript / Python:** First-class factory functions `() => T` or `(arg: A) => T`.

---

## 10 Relationship to Other Principles and Patterns

- **DIP & Service Locator:** Injecting an explicit `ExporterFactory` obeys DIP and replaces Service Locator cleanly.
- **Abstract Factory:** When runtime creation requires a *family* of products, inject an `AbstractFactory`.

---

## 11 Common Mistakes and Anti-Patterns

| Anti-Pattern | Description | Production Consequence | Correct Remedy |
|---|---|---|---|
| **Factory Wrapping Stateless Singletons** | Injecting `UserRepoFactory` just to call `.create()` once in constructor. | Pointless indirection. | Inject `UserRepo` directly. |
| **Injecting IoC Container as a Factory** | Passing `ApplicationContext` or `Container` into services. | Service Locator anti-pattern; hides real dependencies. | Inject narrow `Supplier<T>` or `FooFactory`. |
| **Captive Dependency** | Injecting short-lived stateful `UnitOfWork` directly into Singleton service. | Concurrency corruption! | Inject `Supplier<UnitOfWork>` / Factory. |
| **New-ing Concrete Classes Inside Business Logic** | Skipping both DI and Factory (`new PdfExporter()`). | Violates DIP and blocks unit testing. | Inject Factory or Service. |
| **Over-Complicated Factory Interfaces** | Writing `IExporterFactory` + `ExporterFactoryImpl` when `Function<String, Exporter>` suffices. | Boilerplate bloat. | Use standard `Supplier`/`Function`. |
| **Stateful Factories** | Storing per-request state inside the Factory itself. | Race conditions across threads. | Keep factories stateless. |

---

## 12 Testing and Design Verification

Testing `ExportService` requires passing a simple lambda `fmt -> data -> "FAKE:" + data`—zero container or concrete exporters needed.

---

## 13 Performance, Concurrency and Security Notes

- **Concurrency:** Injecting a `Supplier<T>` factory into a Singleton guarantees each thread gets its own isolated transient instance, preventing shared-state races.

---

## 14 Interview Questions

<details>
<summary><strong>1. When should you inject a Service directly vs injecting a Factory?</strong></summary>

Inject the **Service directly** when it is stateless/long-lived and known at startup. Inject a **Factory** (`Supplier<T>` / `Function<Arg, T>`) when the concrete type depends on runtime parameters, or when a Singleton needs a fresh short-lived/stateful instance per call.
</details>

<details>
<summary><strong>2. Why is injecting `Supplier<T>` better than injecting `ApplicationContext`?</strong></summary>

`Supplier<T>` declares the exact dependency explicitly in the constructor and keeps the class decoupled from the IoC framework, whereas `ApplicationContext` is a Service Locator hiding dependencies.
</details>

<details>
<summary><strong>3. How does injecting a Factory solve the "Captive Dependency" problem?</strong></summary>

Instead of holding one long-lived reference to a short-lived `Transaction` inside a Singleton, the Singleton holds a `Supplier<Transaction>` and acquires a fresh `Transaction` per method call.
</details>

<details>
<summary><strong>4. What is the smell of injecting `FooFactory` and calling `fooFactory.create()` inside the constructor?</strong></summary>

It means runtime parameters weren't needed at all—you should have just injected `Foo` directly from the Composition Root!
</details>

<details>
<summary><strong>5. How do TypeScript and Python simplify Factory injection compared to classic Java?</strong></summary>

In TypeScript and Python, functions are first-class citizens, so `(arg: string) => Product` can be passed directly without declaring a dedicated `ProductFactory` interface or class.
</details>

<details>
<summary><strong>6. Give a real JDK example of a Factory injected via DI.</strong></summary>

`javax.sql.DataSource`: injected via DI into repositories, acting as a connection factory (`dataSource.getConnection()`) per request.
</details>

<details>
<summary><strong>7. How does `Function<String, Exporter>` keep `ExportService` OCP-compliant?</strong></summary>

`ExportService` never contains `switch(format)` or `new PdfExporter()`. New formats are registered in the Composition Root's factory map without touching `ExportService`.
</details>

<details>
<summary><strong>8. What if creating the object requires 3 runtime parameters?</strong></summary>

Define a narrow domain factory interface (e.g., `interface InvoiceFactory { Invoice create(String id, Money total, LocalDate due); }`) rather than nested generic `Function`s.
</details>

---

## 15 Practice Exercises

### Exercise 1: DI vs Factory Triage (Easy)
Which needs Direct DI vs Factory injection? (a) `PasswordHasher`, (b) Per-request `PdfWriter(pageSize)` chosen by user dropdown.

<details>
<summary><strong>Solution</strong></summary>

(a) Direct DI (stateless). (b) Factory injection (`Function<PageSize, PdfWriter>`).
</details>

---

### Exercise 2: Fix Captive Dependency in Java (Medium)
A Singleton `OrderProcessor` injects a non-thread-safe `ValidationContext` directly in its constructor. Fix it with `Supplier<ValidationContext>`.

<details>
<summary><strong>Solution</strong></summary>

Inject `Supplier<ValidationContext> contextFactory` and call `ValidationContext ctx = contextFactory.get();` inside `process()`.
</details>

---

### Exercise 3: Wire Runtime Strategy Factory in Python (Hard)
Implement `PaymentRouter(gateway_factory: Callable[[str], PaymentGateway])` and test routing `"stripe"` and `"paypal"` without any `if` inside `PaymentRouter`.

<details>
<summary><strong>Solution</strong></summary>

Pass `gateways = {"stripe": StripeGateway, "paypal": PayPalGateway}` and `lambda k: gateways[k]()`.
</details>

---

## 16 Summary / Cheat Sheet

```
+--------------------------------------------------------------------------+
|                  DEPENDENCY INJECTION vs FACTORIES                       |
+--------------------------------------------------------------------------+
| DIRECT DI  : Stateless collaborator known at startup -> inject T.        |
| FACTORY+DI : Runtime params or fresh per-call lifetime -> inject         |
|              Supplier<T> / Function<Arg, T> / FooFactory.                |
| NEVER      : Inject ApplicationContext / Service Locator!                |
+--------------------------------------------------------------------------+
```

---

## 17 Further Reading

- Mark Seemann, *Dependency Injection Principles, Practices, and Patterns*, Manning, 2019.

### In Your Language
| Language Track | Deep Dive Reference |
|---|---|
| Java | [Java OOP & Design Patterns](file:///e:/Learning/engineering-notes/01-languages/java/24-design-patterns/README.md) |
| TypeScript | [TypeScript Design Patterns](file:///e:/Learning/engineering-notes/01-languages/typescript/17-design-patterns/README.md) |
| Python | [Python Design Patterns](file:///e:/Learning/engineering-notes/01-languages/python/23-design-patterns/README.md) |

---

[← Previous: 09 - Multiton & Registry](file:///e:/Learning/engineering-notes/08-solid-design-patterns/05-creational-patterns/09-multiton-and-registry.md) | [Module 05 Index](file:///e:/Learning/engineering-notes/08-solid-design-patterns/05-creational-patterns/README.md) | [Next: 11 - Creational Patterns Comparison →](file:///e:/Learning/engineering-notes/08-solid-design-patterns/05-creational-patterns/11-creational-patterns-comparison.md)
