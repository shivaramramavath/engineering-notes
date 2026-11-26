# SOLID & Functional Programming: Paradigms in Convergence

> **One-line summary:** SOLID is not an object-oriented exclusive; in functional programming, the same architectural forces are resolved through pure functions, higher-order composition, structural types, and curried dependency injection.

| Attribute | Specification |
|---|---|
| **Level** | Advanced Foundational |
| **Prerequisites** | [01-single-responsibility.md](01-single-responsibility.md), [02-open-closed.md](02-open-closed.md), [03-liskov-substitution.md](03-liskov-substitution.md), [04-interface-segregation.md](04-interface-segregation.md), [05-dependency-inversion.md](05-dependency-inversion.md) |
| **Estimated Reading Time** | 35 minutes |
| **Status** | Complete |
| **Category** | SOLID Principles |
| **Also Known As** | Functional SOLID, Multi-Paradigm Design Principles |
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

A persistent myth in software engineering claims that:
> *"SOLID principles are obsolete artifacts of legacy Java/C++ object-oriented design; modern functional programming renders them irrelevant."*

Engineering teams that buy into this myth often abandon structural discipline when adopting TypeScript, Python, Scala, or Clojure. The result is what seasoned engineers call **"The Functional Spaghetti"**:
1. **Monolithic Functions:** 500-line functions that parse JSON, perform mathematical calculations, mutate global state, and execute HTTP requests in a single procedural cascade (violating SRP).
2. **Switch-Statement Proliferation:** Adding a new transaction type requires opening and editing 15 different pattern-matching functions across the codebase (violating OCP).
3. **Hidden Impurities:** A function documented as a pure mathematical transformer secretly mutates an external cache or throws undocumented exceptions (violating LSP).
4. **Megastruct Parameter Bloat:** Functions accept 35-field objects when they only need two integers (violating ISP).
5. **Hardwired I/O in Pure Transforms:** Domain algorithms import concrete network or database clients directly, making pure unit testing impossible without monkey-patching runtime symbols (violating DIP).

The forces that drive architectural decay—uncontrolled side effects, high coupling, and fragile branching—do not vanish when you switch from `class` to `function`.

---

## 2 Why It Exists

SOLID principles govern **module boundaries, dependency direction, and substitutability**. Whether a module is represented as a Class, a Package, or a Set of Pure Functions, the economic cost of coupling remains identical.

Computer scientists such as **Rich Hickey** (creator of Clojure) and **Simon Peyton Jones** (Haskell designer) have demonstrated that functional programming does not reject SOLID; rather, **FP provides built-in language primitives that make SOLID trivial to express without boilerplate**:
- In OOP, DIP requires an Interface, a Concrete Class, and a DI Container.
- In FP, DIP is simply **Currying or Higher-Order Functions** (passing a function as a parameter).
- In OOP, OCP requires the Strategy or Decorator pattern.
- In FP, OCP is simply **Function Composition** (`f . g`).

---

## 3 What It Is

The architectural mapping between Object-Oriented SOLID and Functional SOLID is direct and mathematically rigorous:

| Principle | OOP Manifestation | Functional Manifestation |
|---|---|---|
| **SRP** | A class has one actor / one reason to change | A **pure function** computes one transformation without hidden side-effects |
| **OCP** | Strategy pattern, Subclassing, Polymorphic Interfaces | **Higher-Order Functions**, Function Composition, and Typeclasses |
| **LSP** | Subtypes obey supertype contracts and invariants | **Parametric Polymorphism**, Type Signatures, and Category Laws (Functor/Monad laws) |
| **ISP** | Small, client-specific role interfaces | **Narrow function parameter signatures** & Structural Tuples/Records |
| **DIP** | Inverting dependencies via Constructor Injection | **Partial Application**, Currying, and the Reader Monad |

```mermaid
graph TD
    subgraph "OOP SOLID Mechanisms"
        Class[Classes & Interfaces]
        Strategy[Strategy Pattern]
        Contracts[DbC Invariants]
        RoleIf[Role Interfaces]
        IoC[IoC Container & DI]
    end

    subgraph "Functional SOLID Equivalents"
        Pure[Pure Functions (SRP)]
        HOF[Higher-Order Functions (OCP)]
        Category[Type Signatures & Laws (LSP)]
        Narrow[Narrow Signatures / Structs (ISP)]
        Curry[Currying & Partial Application (DIP)]
    end

    Class <--> Pure
    Strategy <--> HOF
    Contracts <--> Category
    RoleIf <--> Narrow
    IoC <--> Curry
```

---

## 4 The Problem Without It

Here is naive functional code in TypeScript that claims to be "functional" because it uses arrow functions, but completely violates SOLID:

```typescript
// Naive FP Anti-Pattern: Impure, tightly coupled, and brittle
import { dbConnection } from "./database"; // 🚨 DIP: Direct I/O dependency
import { sendMail } from "./mailer";       // 🚨 DIP: Direct network dependency

// 🚨 SRP: Mixes pricing, database I/O, and email dispatch
// 🚨 ISP: Forces caller to pass the entire 20-field User object just for an email
export const processOrder = (orderId: string, subtotal: number, user: any, tier: string): number => {
    // 🚨 OCP: Editing this function every time marketing introduces a tier!
    let discount = 0;
    if (tier === "GOLD") discount = subtotal * 0.10;
    else if (tier === "SILVER") discount = subtotal * 0.05;

    const total = subtotal - discount;

    // 🚨 DIP & LSP: Hidden side-effects! Throws if DB is down; impossible to unit test purely!
    dbConnection.query(`INSERT INTO orders VALUES ('${orderId}', ${total})`);
    sendMail(user.email, `Order ${orderId} confirmed: $${total}`);

    return total;
};
```

---

## 5 How It Solves It

We refactor to **Functional SOLID**:
1. **SRP:** The pricing logic becomes a **pure function** with zero side effects.
2. **OCP & DIP:** `processOrder` becomes a higher-order function that accepts `discountFn`, `saveFn`, and `notifyFn` as arguments.
3. **ISP:** The function takes only the exact primitive fields it requires (`email: string`, `total: number`), not a bloated user object.
4. **LSP:** Pure functions guarantee referential transparency; given the same inputs, they always return the same output without throwing surprise network exceptions.

### Java Implementation (Java 25 LTS Functional Style)

```java
package com.design.solid.fp;

import java.util.function.DoubleUnaryOperator;

public class SolidFpSolutionApp {
    // 1. SRP: Pure calculation with zero side effects
    public static double calculateTax(double amount, double rate) {
        return amount * rate;
    }

    // 2. OCP & DIP: Higher-order function taking pure functional dependencies
    public static double computeTotal(
            double subtotal, 
            DoubleUnaryOperator discountPolicy, 
            DoubleUnaryOperator taxPolicy) {
        double discounted = discountPolicy.applyAsDouble(subtotal);
        double tax = taxPolicy.applyAsDouble(discounted);
        return discounted + tax;
    }

    public static void main(String[] args) {
        // DIP via functional dependency injection (passing pure lambdas)
        DoubleUnaryOperator goldDiscount = subtotal -> subtotal * 0.90; // OCP extension!
        DoubleUnaryOperator standardTax = amount -> amount * 0.08;

        double finalPrice = computeTotal(100.0, goldDiscount, standardTax);
        System.out.println("Java FP Total: $" + finalPrice);
    }
}
```

### TypeScript Implementation (Node 24 Native Functional Style)

```typescript
// 1. SRP: Pure mathematical calculation
const calculateTax = (amount: number, rate: number): number => amount * rate;

// 2. DIP via Currying & Higher-Order Functions: Zero classes needed!
type Logger = (msg: string) => void;
type SaveOrder = (orderId: string, total: number) => void;
type DiscountPolicy = (subtotal: number) => number;

// Higher-order function factory (Composition Root via Closures)
const createCheckout = (log: Logger, save: SaveOrder) => 
    (orderId: string, subtotal: number, discountFn: DiscountPolicy): number => {
        const total = discountFn(subtotal);
        save(orderId, total);
        log(`Order ${orderId} completed with total $${total}`);
        return total;
    };

// Wiring dependencies in the application entry point
const consoleLogger: Logger = msg => console.log(`TS Log: ${msg}`);
const memorySaver: SaveOrder = (id, total) => console.log(`TS DB: Saved ${id} ($${total})`);

const checkout = createCheckout(consoleLogger, memorySaver);

// OCP: Injecting a custom 15% discount strategy via pure lambda
const finalTotal = checkout("ORD-900", 200, n => n * 0.85);
console.log(`TS Final: $${finalTotal}`);
```

### Python Implementation (Python 3.14 Functional Style)

```python
from typing import Callable

# 1. SRP: Pure domain functions
def apply_gold_discount(subtotal: float) -> float:
    return subtotal * 0.90

def apply_standard_tax(amount: float) -> float:
    return amount * 0.08

# 2. DIP & OCP: Higher-Order Function accepting callables
def checkout(
    subtotal: float, 
    discount_fn: Callable[[float], float], 
    tax_fn: Callable[[float], float],
    sink: Callable[[str], None]
) -> float:
    discounted = discount_fn(subtotal)
    tax = tax_fn(discounted)
    total = discounted + tax
    sink(f"Processed order total: ${total}")
    return total

if __name__ == "__main__":
    sink_output = []
    # Functional Dependency Injection
    result = checkout(100.0, apply_gold_discount, apply_standard_tax, sink_output.append)
    print(f"Python FP Total: ${result}")
    print(f"Python Sink: {sink_output[0]}")
```

### Real Execution Output
```text
Java FP Total: $97.2
TS DB: Saved ORD-900 ($170)
TS Log: Order ORD-900 completed with total $170
TS Final: $170
Python FP Total: $97.2
Python Sink: Processed order total: $97.2
```

---

## 6 How It Works

### 1. SRP as Referential Transparency
In functional programming, SRP is embodied by **Referential Transparency**: an expression can be replaced by its evaluated result without changing the program's behavior. A function that does math *and* writes to a database is not referentially transparent because replacing it with its return value omits the database write.

### 2. OCP as Function Composition (`compose`)
In FP, you don't subclass to extend behavior; you compose functions:
$$h(x) = (f \circ g)(x) = f(g(x))$$
You create new behavior by chaining pure transformers together without modifying either $f$ or $g$.

### 3. LSP as Type Signature Contracts
If a function requires a mapping from `String -> Int`, any pure function with that exact signature is 100% substitutable. In Category Theory, Functor and Monad laws enforce that identity and associativity laws are never violated by implementations.

### 4. ISP as Structural Narrowing
Instead of passing giant objects, FP functions accept only the precise attributes they operate on:
```typescript
// Violates ISP
const getFullName = (user: { id: string; name: string; ssn: string; address: string }) => ...;

// Respects ISP: Accepts only what it uses
const getFullName = (user: { name: string }) => user.name;
```

### 5. DIP as Currying and Partial Application
Currying transforms a multi-argument function $f(a, b)$ into a chain of single-argument functions $f(a)(b)$. The first argument(s) represent the **injected dependencies**, and the final argument represents the **runtime data**:
```typescript
const executeQuery = (db: Database) => (sql: string) => db.run(sql);
```

---

## 7 Real-World Examples

### 1. Java Stream API: Compositional OCP
The Java Stream API (`list.stream().filter(...).map(...).reduce(...)`) is pure functional SOLID:
- `filter` accepts `Predicate<T>` (OCP).
- `map` accepts `Function<T, R>` (OCP).
The Stream engine itself is closed for modification, but infinitely open for extension via custom lambda predicates.

### 2. Modern Frontend: Redux Reducers
Redux state management in TypeScript is the canonical embodiment of functional SOLID:
```typescript
(state: State, action: Action) => State
```
- **SRP:** Reducers are pure functions computing the next state.
- **OCP:** New actions can be dispatched without rewriting the Redux store dispatch core.
- **DIP:** Side effects (HTTP calls) are pushed to the boundary (Redux Thunk / Sagas) rather than embedded inside reducers.

### 3. Data Engineering: Apache Spark / Python PySpark
In distributed big data processing, data transformations are pure map/filter/reduce pipelines. Spark executors distribute pure functions across thousands of worker nodes without locking, achieving linear horizontal scalability.

---

## 8 When to Use and When NOT to Use

| Context | Paradigm Preference | Rationale |
|---|---|---|
| **Complex Domain Rules with Rich Lifecycle** | **Object-Oriented SOLID** | Encapsulating invariants and state transitions inside Domain Entities. |
| **Data Transformation & Analytics Pipelines** | **Functional SOLID** | Pure data mapping from source to sink without mutable state. |
| **High-Concurrency Event Processing** | **Functional SOLID** | Immutability eliminates thread contention and synchronization locks. |
| **Hardware Driver / Peripheral Control** | **Object-Oriented SOLID** | Hardware devices are inherently stateful, physical objects. |

---

## 9 Variations and Language Idioms

### The Reader Monad (Pure DIP in Haskell / Scala / TypeScript)
In pure functional languages, passing dependencies manually down 10 layers of function calls ("prop drilling") is solved using the **Reader Monad**. The Reader Monad encapsulates a computation that reads from a shared environment (dependencies) without passing it explicitly through every signature.

### Railway-Oriented Programming (Error Handling)
Instead of throwing exceptions (which violates referential transparency and LSP), functional programming uses `Result<T, E>` or `Either<L, R>` types to model happy paths and error paths as parallel tracks, composing functions cleanly with `flatMap` / `bind`.

---

## 10 Relationship to Other Principles and Patterns

- **Command Query Separation (CQS):** CQS is the bridge between OOP and FP. It mandates that functions returning values must not produce side effects.
- **Algebraic Data Types (ADTs):** Sum types (unions) and Product types (records) provide compiler-verified data models that eliminate the need for class hierarchies.

---

## 11 Common Mistakes and Anti-Patterns

| Anti-Pattern | Description | Production Consequence | Correct Remedy |
|---|---|---|---|
| **The Fake Pure Function** | Function claims to be pure, but modifies a global dictionary or reads `Date.now()`. | Non-deterministic test runs; race conditions in concurrency. | Pass time and state explicitly as parameters. |
| **Prop-Drilling Hell** | Passing a database client through 8 nested helper functions. | Severe signature pollution and cognitive friction. | Use Currying / Partial Application or a Reader Monad. |
| **Throwing Exceptions in Pipelines** | Throwing `RuntimeException` inside a `map()` lambda. | Crashes stream processing midway; aborts batch jobs. | Return `Optional<T>` or `Result<T, E>`. |
| **Object-Oriented Mimicry** | Creating namespaces with 1-method classes just to hold functions. | Unnecessary class boilerplate. | Export standalone pure functions directly from modules. |
| **Ignoring Data Immutability** | Passing an array to a function that calls `.sort()` or `.push()` directly on it. | Mutates caller state; subtle spooky-action-at-a-distance bugs. | Return a new cloned/frozen array. |
| **Over-Abstraction with Monad Transformers** | Forcing 4-layer nested Monad Transformers onto a simple 50-line script. | Paralyzing type errors that confuse the entire team. | Keep abstractions as simple higher-order functions until complexity demands monads. |

---

## 12 Testing and Design Verification

Functional SOLID enables **Property-Based Testing** (e.g., QuickCheck in Haskell, `jqwik` in Java, `fast-check` in TypeScript, `hypothesis` in Python):

Because functions are pure (SRP/LSP), tests do not just verify single hardcoded examples. Instead, they verify **universal mathematical invariants** across 10,000 randomly generated inputs:

```python
# Property-Based Testing with Hypothesis
from hypothesis import given, strategies as st

@given(st.floats(min_value=0.0, max_value=1e6))
def test_discount_invariant_is_always_less_than_subtotal(subtotal):
    discounted = apply_gold_discount(subtotal)
    assert discounted <= subtotal
    assert discounted >= 0.0
```

---

## 13 Performance, Concurrency and Security Notes

### Performance
- **Allocation Overhead:** Immutability produces new object allocations instead of in-place mutations. In modern generational garbage collectors (JVM ZGC, V8), short-lived objects in young generation are reclaimed in microseconds.
- **Tail Call Optimization:** Pure recursive functions can be optimized by compilers into jump loops, avoiding stack overflow errors.

### Concurrency: The Ultimate Benefit of FP
- **Zero Locks:** Pure functions have no shared mutable state. In a multi-threaded system, 1,000 threads can execute `calculateTax(order)` simultaneously without a single mutex, synchronized block, or data race.

### Security
- Pure functions are immune to **Time-of-Check to Time-of-Use (TOCTOU)** vulnerabilities because state cannot change out from under the function between validation and execution.

---

## 14 Interview Questions

<details>
<summary><strong>1. How does Dependency Inversion (DIP) manifest in functional programming without interfaces?</strong></summary>

In functional programming, DIP is achieved via **Higher-Order Functions, Currying, and Partial Application**. Instead of injecting an interface into a class constructor, a function accepts its dependency functions (e.g. `logger`, `dbSave`) as its initial arguments, returning a specialized function that takes only runtime data.
</details>

<details>
<summary><strong>2. How does the Single Responsibility Principle apply to a pure function?</strong></summary>

A pure function adheres to SRP by computing a single deterministic transformation from input to output, with **zero side effects** (no I/O, no logging, no mutation of global variables). If a function computes a tax rate and also prints to console or writes to a file, it violates SRP.
</details>

<details>
<summary><strong>3. How does Function Composition achieve the Open/Closed Principle?</strong></summary>

Function composition ($f \circ g$) allows developers to build new behavioral pipelines by composing existing functions together, without modifying the internal source code of either $f$ or $g$.
</details>

<details>
<summary><strong>4. Why do exceptions violate the Liskov Substitution Principle in functional programming?</strong></summary>

In functional programming, an expression must be referentially transparent (replaceable by its value). Throwing an unhandled runtime exception aborts execution and bypasses the type signature contract, breaking caller substitutability. FP languages prefer modeling failure explicitly via `Result` or `Either` types.
</details>

<details>
<summary><strong>5. What is the Reader Monad and how does it relate to Dependency Injection?</strong></summary>

The Reader Monad is a functional design pattern that represents a computation with access to a shared read-only environment (dependencies). It allows dependency injection to occur implicitly throughout a call stack without manually passing dependency arguments to every intermediate function ("prop drilling").
</details>

<details>
<summary><strong>6. How does Interface Segregation Principle (ISP) apply to function signatures?</strong></summary>

Under functional ISP, a function should demand only the minimal set of data fields or parameters it actually requires, rather than forcing callers to pass large, bloated records or domain entities.
</details>

<details>
<summary><strong>7. What is "Railway-Oriented Programming"?</strong></summary>

It is a functional programming architectural pattern (popularized by Scott Wlaschin) where operations are modeled as two-track railways (Success and Failure). Functions take a two-track input and output a two-track result, allowing happy-path operations to compose seamlessly while errors bypass subsequent operations without throwing exceptions.
</details>

<details>
<summary><strong>8. Can an application be 100% pure functional code?</strong></summary>

No. Software that produces zero side effects is useless—it cannot draw pixels, write to disk, send network packets, or receive user input. Functional architectures use the **"Functional Core, Imperative Shell"** pattern: the core business rules are 100% pure, while the outer shell (I/O, database, network) executes the necessary side effects.
</details>

---

## 15 Practice Exercises

### Exercise 1: Spot the Functional Violations (Easy)
Identify the three design violations in this JavaScript function:
```javascript
let totalProcessed = 0; // global state

function calculateInvoice(items) {
    totalProcessed++;
    let total = 0;
    for (let item of items) total += item.price;
    fs.appendFileSync("./log.txt", `Processed: ${total}\n`);
    return total;
}
```

<details>
<summary><strong>Solution</strong></summary>

1. **SRP Violation:** Mutates global state (`totalProcessed++`) and performs synchronous disk I/O (`fs.appendFileSync`).
2. **DIP Violation:** Direct hardwired coupling to Node's `fs` module; impossible to test in-memory.
3. **Loss of Referential Transparency:** Not a pure function; calling it produces visible global side effects.
</details>

---

### Exercise 2: Refactor to Curried Dependency Injection (Medium)
Refactor this TypeScript function using currying so that the `emailGateway` is injected from the outside:

```typescript
import { sendGrid } from "./sendGridClient";

function sendWelcomeEmail(userEmail: string, userName: string): void {
    sendGrid.send(userEmail, `Welcome ${userName}!`);
}
```

<details>
<summary><strong>Solution</strong></summary>

```typescript
type EmailSender = (to: string, body: string) => Promise<void>;

// Curried higher-order function: dependencies first, runtime data second
const makeWelcomeEmailSender = (sender: EmailSender) => 
    async (userEmail: string, userName: string): Promise<void> => {
        await sender(userEmail, `Welcome ${userName}!`);
    };

// Test with in-memory fake
const fakeSender: EmailSender = async (to, body) => { console.log(`Test: ${to}`); };
const sendWelcome = makeWelcomeEmailSender(fakeSender);
sendWelcome("alice@test.com", "Alice");
```
</details>

---

### Exercise 3: Functional Core, Imperative Shell in Python (Hard)
Implement an automated order validation pipeline in Python using the "Functional Core, Imperative Shell" pattern:
1. Pure validation function: Checks if order total $> 0$ and items are in stock.
2. Returns a `Result` dataclass (`is_success: bool`, `error_message: Optional[str]`).
3. The imperative shell executes the I/O only if the pure core validates the order.

<details>
<summary><strong>Solution</strong></summary>

```python
from dataclasses import dataclass
from typing import Optional, List

@dataclass(frozen=True)
class Item:
    sku: str
    price: float
    stock: int

@dataclass(frozen=True)
class ValidationResult:
    is_success: bool
    error: Optional[str] = None

# --- FUNCTIONAL CORE (100% Pure, Zero I/O) ---
def validate_order(items: List[Item]) -> ValidationResult:
    if not items:
        return ValidationResult(False, "Order must have at least one item")
    for item in items:
        if item.stock <= 0:
            return ValidationResult(False, f"Item {item.sku} is out of stock")
    return ValidationResult(True)

# --- IMPERATIVE SHELL (Handles I/O at the boundary) ---
def execute_order_shell(items: List[Item], db_save_fn):
    result = validate_order(items)
    if not result.is_success:
        print(f"Order rejected: {result.error}")
        return False
    db_save_fn(items)
    print("Order successfully persisted to database!")
    return True

# Test pure core without any DB!
res = validate_order([Item("LAPTOP", 999.0, 0)])
assert res.is_success is False
assert "out of stock" in res.error
```
</details>

---

## 16 Summary / Cheat Sheet

```
+--------------------------------------------------------------------------+
|                     SOLID & FUNCTIONAL PROGRAMMING CHEAT SHEET           |
+--------------------------------------------------------------------------+
| PRINCIPLE | FUNCTIONAL TRANSLATION                 | CORE MECHANISM      |
|-----------|----------------------------------------|---------------------|
| SRP       | Pure function without side effects     | Referential Transp. |
| OCP       | Function composition / Pipelines       | Higher-Order Funcs  |
| LSP       | Type signatures and Category Laws      | Parametric Polymorph|
| ISP       | Narrow parameter lists / Tuple structs | Structural Typing   |
| DIP       | Partial application and Currying       | Reader Monad / HOFs |
+--------------------------------------------------------------------------+
| ARCHITECTURAL PATTERN: "Functional Core, Imperative Shell"               |
|                        Keep business math pure; push I/O to boundaries.  |
+--------------------------------------------------------------------------+
```

---

## 17 Further Reading

- Scott Wlaschin, *Domain Modeling Made Functional*, Pragmatic Bookshelf, 2018.
- Michael Feathers, *Functional Core, Imperative Shell*, Destroy All Software, 2012.
- Mark Seemann, *Dependency Injection Principles, Practices, and Patterns (Functional DI)*, Manning, 2019.

### In Your Language
| Language Track | Deep Dive Reference |
|---|---|
| Java | [Java OOP & Design Patterns](file:///e:/Learning/engineering-notes/01-languages/java/24-design-patterns/README.md) |
| TypeScript | [TypeScript Design Patterns](file:///e:/Learning/engineering-notes/01-languages/typescript/17-design-patterns/README.md) |
| Python | [Python Design Patterns](file:///e:/Learning/engineering-notes/01-languages/python/23-design-patterns/README.md) |

---

[← Previous: 07 - SOLID Violations Spotting Guide](file:///e:/Learning/engineering-notes/08-solid-design-patterns/02-solid-principles/07-solid-violations-spotting-guide.md) | [Module 02 Index](file:///e:/Learning/engineering-notes/08-solid-design-patterns/02-solid-principles/README.md) | [Next: 09 - Criticisms & Limits of SOLID →](file:///e:/Learning/engineering-notes/08-solid-design-patterns/02-solid-principles/09-criticisms-and-limits-of-solid.md)
