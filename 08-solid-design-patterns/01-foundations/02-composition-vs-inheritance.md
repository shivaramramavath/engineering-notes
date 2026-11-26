# Composition vs Inheritance

> **One-line summary:** Favor object composition over class inheritance: inheritance creates the tightest coupling in software and exposes subclasses to the Fragile Base Class problem, whereas composition decouples components through swappable, delegating collaborators.

| Attribute | Specification |
|---|---|
| **Level** | Foundations / Core |
| **Prerequisites** | [01-oop-pillars.md](01-oop-pillars.md) |
| **Estimated Reading Time** | 35 minutes |
| **Status** | Complete |
| **Category** | Foundations |
| **Also Known As** | "Favor 'Has-A' over 'Is-A'", Fragile Base Class Defense |
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

An engineering team building a high-throughput notification gateway needs to track how many total items are added to an internal processing cache. A junior developer decides to extend standard library collections:

```java
public class InstrumentedHashSet<E> extends HashSet<E> {
    private int addCount = 0;
    @Override public boolean add(E e) { addCount++; return super.add(e); }
    @Override public boolean addAll(Collection<? extends E> c) { addCount += c.size(); return super.addAll(c); }
    public int getAddCount() { return addCount; }
}
```

The developer inserts 3 items using `addAll(List.of("A", "B", "C"))`. In production, the counter reports **6 items added** instead of 3.
Why? Because inside the JDK's `HashSet`, the base class's `addAll()` method was implemented by looping and calling its own `add()` method. As a result:
1. `InstrumentedHashSet.addAll()` adds 3 to `addCount`.
2. `super.addAll()` executes, which calls `this.add()` three times.
3. Because of dynamic dispatch, `InstrumentedHashSet.add()` intercepts each call, adding 1 each time.
4. $3 + 1 + 1 + 1 = 6$.

The subclass broke because it relied on an invisible implementation detail of the base class. Worse, if the JDK team later refactors `HashSet.addAll()` to use direct internal array manipulation instead of self-invoking `add()`, the subclass silently breaks in the opposite direction.

---

## 2 Why It Exists

Class inheritance was initially celebrated in the 1980s as the ultimate vehicle for software reuse. However, large enterprise systems discovered two fatal flaws in practice:
1. **The Fragile Base Class Problem**: A superclass and subclass are so tightly coupled that changes to the superclass can silently break subclasses without changing their source code.
2. **Combinatorial Explosion of Subclasses**: Attempting to model orthogonal features (e.g. `PlainEmail`, `EncryptedEmail`, `LoggedEmail`, `LoggedEncryptedEmail`, `CompressLoggedEncryptedEmail`) requires $2^N$ subclasses.

The Gang of Four (1994) codified the guiding rule:
> *"Favor object composition over class inheritance."*

---

## 3 What It Is

- **Inheritance ("Is-A")**: A white-box relationship where a derived class inherits both the public interface and internal implementation of its parent at compile time.
- **Composition ("Has-A")**: A black-box relationship where a class contains a reference to another object (a collaborator) and delegates work to it via well-defined interfaces at runtime.

```mermaid
graph TD
    subgraph Inheritance["Inheritance (White-Box, High Coupling)"]
        Base[Superclass BaseSet] <|-- Derived[Subclass InstrumentedSet]
    end
    subgraph Composition["Composition (Black-Box, Low Coupling)"]
        Wrapper[InstrumentedSet Wrapper] -->|delegates to| TargetInterface[Set Interface]
        TargetInterface <|.. RealSet[Concrete HashSet]
    end
```

---

## 4 The Problem Without It

Here is the exact code demonstrating the Fragile Base Class bug:

```java
// File: InheritanceTrapApp.java
import java.util.*;

class InstrumentedHashSet<E> extends HashSet<E> {
    private int addCount = 0;

    @Override
    public boolean add(E e) {
        addCount++;
        return super.add(e);
    }

    @Override
    public boolean addAll(Collection<? extends E> c) {
        addCount += c.size();
        return super.addAll(c); // HashSet.addAll calls add() internally!
    }

    public int getAddCount() { return addCount; }
}

public class InheritanceTrapApp {
    public static void main(String[] args) {
        InstrumentedHashSet<String> set = new InstrumentedHashSet<>();
        set.addAll(List.of("Snap", "Crackle", "Pop"));
        System.out.println("Expected 3, Actual addCount: " + set.getAddCount());
    }
}
```

```text
Expected 3, Actual addCount: 6
```

### The Change Request: Add Security Sanitization
When management requests that every inserted string be sanitized against SQL injection:
1. If you override `add()`, `addAll()` might or might not call it depending on the JVM vendor (Oracle vs OpenJDK vs IBM Semeru).
2. If the parent class adds a new method in Java 26 (e.g. `addIfAbsent()`), callers can bypass your security checks entirely because your subclass does not override methods that did not exist when you wrote it.

---

## 5 How It Solves It

With composition, the `InstrumentedSet` does not inherit from `HashSet`. Instead, it wraps an underlying `Set` and delegates to it.

### Java Implementation
```java
// File: CompositionApp.java
import java.util.*;

class InstrumentedSet<E> implements Set<E> {
    private final Set<E> set;
    private int addCount = 0;

    public InstrumentedSet(Set<E> set) {
        this.set = set;
    }

    public boolean add(E e) {
        addCount++;
        return set.add(e);
    }

    public boolean addAll(Collection<? extends E> c) {
        addCount += c.size();
        return set.addAll(c);
    }

    public int getAddCount() { return addCount; }

    // Forwarding delegates
    public int size() { return set.size(); }
    public boolean isEmpty() { return set.isEmpty(); }
    public boolean contains(Object o) { return set.contains(o); }
    public Iterator<E> iterator() { return set.iterator(); }
    public Object[] toArray() { return set.toArray(); }
    public <T> T[] toArray(T[] a) { return set.toArray(a); }
    public boolean remove(Object o) { return set.remove(o); }
    public boolean containsAll(Collection<?> c) { return set.containsAll(c); }
    public boolean retainAll(Collection<?> c) { return set.retainAll(c); }
    public boolean removeAll(Collection<?> c) { return set.removeAll(c); }
    public void clear() { set.clear(); }
}

public class CompositionApp {
    public static void main(String[] args) {
        InstrumentedSet<String> set = new InstrumentedSet<>(new HashSet<>());
        set.addAll(List.of("Snap", "Crackle", "Pop"));
        System.out.println("Composition resolved! Actual addCount: " + set.getAddCount());
    }
}
```

### TypeScript Implementation
```typescript
// File: CompositionApp.ts
class InstrumentedSet<T> {
    private readonly set: Set<T>;
    private addCount = 0;

    constructor(initialSet: Set<T> = new Set()) {
        this.set = initialSet;
    }

    add(item: T): this {
        this.addCount++;
        this.set.add(item);
        return this;
    }

    addAll(items: Iterable<T>): void {
        for (const item of items) {
            this.add(item);
        }
    }

    get count(): number { return this.addCount; }
    get size(): number { return this.set.size; }
}

const s = new InstrumentedSet<string>();
s.addAll(["Snap", "Crackle", "Pop"]);
console.log(`TS Composition addCount: ${s.count}`);
```

### Python Implementation
```python
# File: composition_app.py
from typing import Set, Iterable, TypeVar

T = TypeVar('T')

class InstrumentedSet:
    def __init__(self, initial_set: set | None = None):
        self._set: set = initial_set if initial_set is not None else set()
        self._add_count = 0

    def add(self, item: T) -> None:
        self._add_count += 1
        self._set.add(item)

    def add_all(self, items: Iterable[T]) -> None:
        for item in items:
            self.add(item)

    @property
    def add_count(self) -> int:
        return self._add_count

s = InstrumentedSet()
s.add_all(["Snap", "Crackle", "Pop"])
print(f"Python Composition addCount: {s.add_count}")
```

```text
Composition resolved! Actual addCount: 3
TS Composition addCount: 3
Python Composition addCount: 3
```

---

## 6 How It Works

### Architectural Difference: White-Box vs Black-Box Reuse

| Dimension | Inheritance (White-Box) | Composition (Black-Box) |
|---|---|---|
| **Binding Time** | Compile time (static) | Runtime (dynamic) |
| **Visibility** | Subclass sees `protected` internals | Wrapper sees only `public` interface |
| **Coupling** | **Tightest coupling possible**: changes in parent ripple down | **Loose coupling**: depends only on stable interface contract |
| **Subtyping** | Subclass is constrained by superclass type rules | Wrapper can implement any interface, wrap any implementation |
| **Swappability** | Impossible to change parent at runtime | Easy to swap collaborator at runtime (`setChannel(new SmsChannel())`) |

---

## 7 Real-World Examples

### 1. Business Domain: Notification Delivery Gateway
Instead of building an unmanageable tree of subclasses (`EmailNotifier`, `SmsNotifier`, `EncryptedEmailNotifier`), the notification gateway composes components:
- `NotificationService` takes a `List<DeliveryChannel>` and a `MessageFormatter`.
- To add encryption, wrap the channel in an `EncryptedChannelDecorator`.
- To add SMS, simply append an `SmsChannel` instance to the list. Zero class hierarchy churn.

### 2. Standard Library: `java.io.FilterInputStream` (The Decorator Invariant)
The Java JDK avoids subclass explosion by using composition in IO:
- `FileInputStream` handles disk reads.
- `BufferedInputStream` wraps any `InputStream` to buffer chunks in RAM.
- `GZIPInputStream` wraps any `InputStream` to decompress gzip frames.  
You compose them dynamically:
```java
InputStream stream = new GZIPInputStream(new BufferedInputStream(new FileInputStream("data.gz")));
```

### 3. Modern Cloud / Infrastructure: HTTP Middleware Pipelines
In frameworks like Express, NestJS, and ASP.NET Core, HTTP handlers are not created by subclassing a `BaseHttpServer`. Instead, pipelines are composed of independent middleware functions (`authMiddleware`, `rateLimitMiddleware`, `compressionMiddleware`). Each function wraps the next handler in the chain.

---

## 8 When to Use and When NOT to Use

### Use Composition When:
- You want to reuse behavior without inheriting identity.
- Collaborators need to be swapped at runtime (e.g. changing strategies based on user preferences).
- You are wrapping a class you do not own (third-party SDKs, standard library classes).
- Features can be combined in arbitrary configurations.

### Use Inheritance ONLY When:
- A true, strict **Liskov-substitutable "Is-A" relationship** holds across all dimensions of behavior.
- The base class was **explicitly designed and documented for extension** (e.g. framework template methods with `final` invariants).
- Subclass and superclass reside within the same package under the same engineering team.

---

## 9 Variations and Language Idioms

- **Kotlin / Dart**: Native language-level delegation (`class InstrumentedSet<E>(val inner: Set<E>) : Set<E> by inner`).
- **Rust / Go**: True zero-inheritance languages. Both languages completely omit class inheritance and enforce composition and trait/interface delegation exclusively.
- **Python**: `__getattr__` allows transparent dynamic forwarding to an internal wrapped delegate.

---

## 10 Relationship to Other Principles and Patterns

- **Decorator Pattern**: Direct formalization of composition to dynamically attach responsibilities.
- **Strategy Pattern**: Replaces inheritance-based algorithm specialization with interchangeable composed strategy objects.
- **Liskov Substitution Principle (LSP)**: Fragile base classes almost always lead to LSP violations. Refactoring to composition restores LSP.

---

## 11 Common Mistakes and Anti-Patterns

| Mistake | Why It Breaks | Production Fix |
|---|---|---|
| **Subclassing Collections** | Standard collections have complex self-invocation patterns (`addAll` calling `add`). | Wrap the collection inside a custom class and expose only necessary methods. |
| **Inheriting for Helper Methods** | Subclassing `UtilityBaseClass` pollutes your public API with irrelevant helper methods. | Inject the utility as a private collaborator or static function. |
| **Yo-Yo Problem** | Deep inheritance hierarchies force developers to jump up and down 6 levels to trace execution. | Flatten the hierarchy into composed delegates. |
| **Overriding Methods to Throw `UnsupportedOperationException`** | Subclass cannot fulfill the base class contract (violates LSP). | The subclass is not a true subtype. Split the base class or use composition. |
| **Boilerplate Fatigue** | Writing forwarding methods for 20 interface methods causes developers to revert to inheritance. | Use an abstract forwarding wrapper class (like Guava's `ForwardingSet`) or language delegation. |
| **Inheriting Across Module Boundaries** | Updating a parent library jar/package silently breaks your subclass. | Treat third-party classes as black boxes; wrap them with adapters. |

---

## 12 Testing and Design Verification

Composition makes unit testing trivial because collaborators can be replaced with lightweight test doubles:

```java
// Testing NotificationService by injecting a fake channel
class FakeChannel implements DeliveryChannel {
    List<String> sentMessages = new ArrayList<>();
    public void send(String msg) { sentMessages.add(msg); }
}

class NotificationServiceTest {
    void testNotificationDelivery() {
        FakeChannel fake = new FakeChannel();
        NotificationService service = new NotificationService(List.of(fake));
        service.notifyAllUsers("System Update");
        assert fake.sentMessages.size() == 1;
    }
}
```

---

## 13 Performance, Concurrency and Security Notes

- **Performance**: Composition adds one extra object pointer dereference per method call. In high-frequency hot paths, JVM JIT compilers inline forwarding methods, resulting in zero runtime performance penalty.
- **Security**: Subclassing creates a vector where attackers can override methods to bypass security checks. Marking critical classes `final` forces developers to use composition, preserving security barriers.

---

## 14 Interview Questions

<details>
<summary><strong>1. Why is composition considered black-box reuse while inheritance is white-box reuse?</strong></summary>

**Answer:**  
- **Inheritance is white-box reuse** because the internal implementation of the parent class is visible to the subclass (via `protected` members and self-invocation), making the subclass vulnerable to internal changes in the parent.
- **Composition is black-box reuse** because the wrapping class interacts with the contained object solely through its public interface without knowing or depending on its private state or internal method calls.
</details>

<details>
<summary><strong>2. Explain the Fragile Base Class problem with a concrete example.</strong></summary>

**Answer:**  
The problem occurs when a change to a superclass breaks subclasses. For instance, if `SuperClass` has `process()` which calls `stepA()`, and a subclass overrides `stepA()`. If a new version of `SuperClass` refactors `process()` to call a new `stepB()` instead, the subclass's overridden behavior is silently skipped.
</details>

<details>
<summary><strong>3. How does composition prevent the combinatorial explosion of subclasses?</strong></summary>

**Answer:**  
If an entity has $N$ independent features, inheritance requires $2^N$ subclasses to represent every permutation. With composition, each feature is encapsulated in a separate collaborator or decorator. You need only $N$ classes, which can be composed in any combination at runtime.
</details>

<details>
<summary><strong>4. What is the forwarding wrapper pattern?</strong></summary>

**Answer:**  
A pattern where a class implements an interface by holding a private instance of that interface and forwarding all method calls to it. Subclasses can override only the specific methods they wish to instrument, avoiding the fragile base class problem entirely.
</details>

<details>
<summary><strong>5. Can you change an object's behavior at runtime with inheritance?</strong></summary>

**Answer:**  
No. Inheritance binds a class to its superclass statically at compile time. Composition allows you to swap an internal collaborator reference at runtime (e.g. `order.setTaxCalculator(new EuropeanTaxCalculator())`), dynamically changing behavior.
</details>

<details>
<summary><strong>6. Why does Go have no <code>extends</code> keyword?</strong></summary>

**Answer:**  
Go was intentionally designed to eliminate the problems of inheritance hierarchies. Go provides struct embedding and interfaces, forcing developers to build systems via composition and implicit interface satisfaction.
</details>

<details>
<summary><strong>7. When is inheritance legitimately preferable to composition?</strong></summary>

**Answer:**  
When there is a genuine subtype relationship that satisfies the Liskov Substitution Principle across all methods, when both classes are developed together in the same package, and when the base class is deliberately designed for extension using template methods with `final` invariants.
</details>

<details>
<summary><strong>8. What is the Yo-Yo problem in object-oriented systems?</strong></summary>

**Answer:**  
An anti-pattern where a programmer trying to understand or debug a class must repeatedly inspect code up and down a deep inheritance hierarchy (e.g. 5+ levels) to understand which method version actually executes.
</details>

---

## 15 Practice Exercises

### Exercise 1: Spot the Fragile Base Class (Easy)
Why will this subclass break if `ListBase` changes?
```java
class ListBase {
    public void add(String s) { /* insert */ }
    public void addAll(List<String> list) {
        for (String s : list) add(s);
    }
}
class AuditedList extends ListBase {
    @Override public void addAll(List<String> list) {
        log("Bulk adding " + list.size());
        super.addAll(list);
    }
}
```
<details>
<summary><strong>Solution</strong></summary>

If `ListBase` is optimized in a future version to perform bulk inserts natively (without calling `add(s)` in a loop), `AuditedList` will log the operation, but if it relies on `add()` being invoked for individual auditing, it will miss audit records. Conversely, if `AuditedList` also overrides `add()` to log individual items, calling `addAll()` would produce duplicate logs (one bulk log + $N$ individual logs).
</details>

### Exercise 2: Refactor Subclass Explosion (Medium)
Refactor these 4 classes into 2 classes using composition:
- `ConsoleLogger`
- `TimestampedConsoleLogger`
- `FileLogger`
- `TimestampedFileLogger`
<details>
<summary><strong>Solution</strong></summary>

```java
interface LogDestination { void write(String msg); }
class ConsoleDestination implements LogDestination { public void write(String m) { System.out.println(m); } }
class FileDestination implements LogDestination { public void write(String m) { /* write to file */ } }

class Logger {
    private final LogDestination destination;
    private final boolean timestamped;
    public Logger(LogDestination dest, boolean timestamped) {
        this.destination = dest;
        this.timestamped = timestamped;
    }
    public void log(String msg) {
        String formatted = timestamped ? System.currentTimeMillis() + ": " + msg : msg;
        destination.write(formatted);
    }
}
```
</details>

### Exercise 3: Implement an Unmodifiable Set Wrapper (Medium)
Create an `UnmodifiableSet<E>` wrapper that uses composition to throw `UnsupportedOperationException` on `add()` and `remove()`, while delegating read calls to the wrapped set.
<details>
<summary><strong>Solution</strong></summary>

```java
public class UnmodifiableSet<E> {
    private final Set<E> inner;
    public UnmodifiableSet(Set<E> inner) { this.inner = inner; }
    public boolean contains(E item) { return inner.contains(item); }
    public int size() { return inner.size(); }
    public void add(E item) { throw new UnsupportedOperationException("Read-only set"); }
    public void remove(E item) { throw new UnsupportedOperationException("Read-only set"); }
}
```
</details>

### Exercise 4: Dynamic Strategy Switching (Hard)
Demonstrate how composition enables switching a discount algorithm at runtime on an `Order` object without instantiating a new `Order`.
<details>
<summary><strong>Solution</strong></summary>

```java
interface DiscountPolicy { double apply(double amount); }
class RegularDiscount implements DiscountPolicy { public double apply(double a) { return a; } }
class VipDiscount implements DiscountPolicy { public double apply(double a) { return a * 0.8; } }

class Order {
    private double total;
    private DiscountPolicy discountPolicy = new RegularDiscount();

    public Order(double total) { this.total = total; }
    public void setDiscountPolicy(DiscountPolicy policy) { this.discountPolicy = policy; }
    public double finalPrice() { return discountPolicy.apply(total); }
}
```
</details>

### Exercise 5: Spot the Inheritance Misuse (Easy)
Explain why `class Stack<E> extends Vector<E>` in early Java is widely recognized as a severe design mistake.
<details>
<summary><strong>Solution</strong></summary>

A Stack is a LIFO (Last-In, First-Out) structure that should only expose `push()`, `pop()`, `peek()`. Because `Stack` extends `Vector`, it publicly inherited methods like `insertElementAt(int index)` and `removeElementAt(int index)`, allowing callers to add or remove elements in the middle of the stack, completely violating the fundamental LIFO invariant.
</details>

### Exercise 6: Complete Forwarding Wrapper in TypeScript (Hard)
Implement a generic `LoggedMap<K, V>` wrapper around JavaScript's native `Map` that logs every `set()` and `delete()` operation using composition.
<details>
<summary><strong>Solution</strong></summary>

```typescript
class LoggedMap<K, V> {
    private readonly inner: Map<K, V>;

    constructor(initialEntries?: [K, V][]) {
        this.inner = new Map<K, V>(initialEntries);
    }

    get(key: K): V | undefined {
        return this.inner.get(key);
    }

    set(key: K, value: V): this {
        console.log(`SET [${String(key)}] = ${String(value)}`);
        this.inner.set(key, value);
        return this;
    }

    delete(key: K): boolean {
        console.log(`DELETE [${String(key)}]`);
        return this.inner.delete(key);
    }

    has(key: K): boolean {
        return this.inner.has(key);
    }

    get size(): number {
        return this.inner.size;
    }
}
```
</details>

---

## 16 Summary / Cheat Sheet

```
+---------------------------------------------------------------------------------------+
| COMPOSITION VS INHERITANCE QUICK REFERENCE                                            |
| Rule: Favor object composition over class inheritance.                                |
|                                                                                       |
| Inheritance ("Is-A"):                                                                 |
| - Compile-time static coupling.                                                       |
| - White-box reuse (vulnerable to fragile base classes).                               |
| - Leads to 2^N subclass explosion when modeling independent features.                 |
|                                                                                       |
| Composition ("Has-A"):                                                                |
| - Runtime dynamic delegation.                                                         |
| - Black-box reuse (shielded behind public interface contracts).                       |
| - Flexible: features are injected and swappable at runtime.                           |
+---------------------------------------------------------------------------------------+
```

---

## 17 Further Reading

- Bloch, Joshua (2018). *"Effective Java"*, 3rd Edition (Item 18: Favor composition over inheritance).
- Gamma, Helm, Johnson, Vlissides (1994). *"Design Patterns"*, Chapter 1: Object Composition vs Class Inheritance.
- Fowler, Martin. *"Refactoring: Improving the Design of Existing Code"*, Replace Inheritance with Delegation.

### In Your Language
| Language Track | Deep Dive Reference |
|---|---|
| Java | [Java OOP & Composition](file:///e:/Learning/engineering-notes/01-languages/java/24-design-patterns/README.md) |
| TypeScript | [TypeScript Mixins & Composition](file:///e:/Learning/engineering-notes/01-languages/typescript/17-design-patterns/README.md) |
| Python | [Python Composition & Multiple Inheritance](file:///e:/Learning/engineering-notes/01-languages/python/23-design-patterns/README.md) |

---
[← Previous: 01 - OOP Pillars](01-oop-pillars.md) | [Section Index](README.md) | [Next: 03 - Coupling and Cohesion →](03-coupling-and-cohesion.md)
