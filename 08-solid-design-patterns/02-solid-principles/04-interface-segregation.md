# Interface Segregation Principle (ISP)

> **One-line summary:** Clients should never be forced to depend upon interfaces, methods, or abstractions that they do not use.

| Attribute | Specification |
|---|---|
| **Level** | Core / Foundational |
| **Prerequisites** | [01-single-responsibility.md](01-single-responsibility.md), [04-programming-to-interfaces.md](../01-foundations/04-programming-to-interfaces.md) |
| **Estimated Reading Time** | 30 minutes |
| **Status** | Complete |
| **Category** | SOLID Principles |
| **Also Known As** | Role Interface Pattern, Client-Specific Interfaces |
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

Imagine an enterprise document management platform. Early in the product roadmap, developers defined a single, overarching interface to represent all hardware and virtual document devices:

```
    +-------------------------------------------------------------+
    |                 <<interface>> MultiFunctionDevice           |
    +-------------------------------------------------------------+
    | + print(doc: String)                                        |
    | + scan(doc: String)                                         |
    | + fax(doc: String)                                          |
    | + staple()                                                  |
    | + duplexPrint(doc: String)                                  |
    | + ocrRecognize(image: byte[]): String                       |
    +-------------------------------------------------------------+
```

Now, the engineering team encounters three operational crises:
1. **The Dummy Stub Epidemic:** The company manufactures an affordable, high-speed thermal receipt printer for point-of-sale checkout counters. Because the core system requires passing a `MultiFunctionDevice`, the receipt printer driver is forced to implement all six methods. It implements `print()` and stubs out `scan()`, `fax()`, and `staple()` with empty bodies or `throw new UnsupportedOperationException()`.
2. **The Sneak-Attack Production Crash:** An automated nightly batch job selects all registered devices and invokes `device.fax()` for legal notices. It grabs the receipt printer, triggers the runtime exception, and crashes the checkout register during an active sale.
3. **The Recompilation Ripple Effect:** The legal department requests a modification to fax protocol standards (`fax(doc, recipientPhone, internationalPrefix)`). Changing this single method on `MultiFunctionDevice` requires editing, recompiling, and redeploying the thermal receipt printer driver, the barcode scanner driver, and the PDF virtual printer—even though none of them have ever sent a fax in their operational lifespan.

When an interface becomes a catch-all "fat interface", every implementer and every client is held hostage to changes requested by unrelated consumers.

---

## 2 Why It Exists

The Interface Segregation Principle was coined by **Robert C. Martin ("Uncle Bob")** in the late 1990s while consulting for **Xerox Corporation**.

Xerox had manufactured a state-of-the-art multi-function printer system that could perform printing, copying, scanning, and faxing. The entire software stack was designed around a massive, monolithic class called `Job`. Whether a client wanted to print a single text document, bind a ledger, or check toner levels, it had to depend directly on `Job`.

As new features were added to `Job`, Xerox developers faced skyrocketing build times (taking hours to recompile the entire firmware). Worse, an edit made to support a new stapler accessory would break the printing engine because changes to the memory layout and header files required all client subsystems to be recompiled and re-certified.

Uncle Bob solved this by breaking the monolithic `Job` interface into **client-specific role interfaces**: `PrintJob`, `ScanJob`, `FaxJob`. A client that only needed printing depended strictly on `PrintJob`. Changes to faxing or stapling could no longer force re-compilation or testing of the printing subsystem.

---

## 3 What It Is

The formal definition of ISP states:
> *"Clients should not be forced to depend upon interfaces that they do not use."*

In system architecture, ISP advocates for **Role Interfaces** rather than **Header Interfaces**:
- **Header Interface:** An interface that simply duplicates all the public methods of a single concrete class. (Anti-pattern).
- **Role Interface:** A focused, client-centric interface that represents a single capability or behavioral contract from the *consumer's* perspective.

```mermaid
graph TD
    subgraph "ISP Violation: Fat Interface"
        ClientP1[Print Client] --> Fat["<<interface>>\nMultiFunctionDevice\n+print()\n+scan()\n+fax()\n+staple()"]
        ClientF1[Fax Client] --> Fat
        
        Inkjet1[BasicInkjetPrinter\n(Throws on scan/fax)] -.->|implements| Fat
        Copier1[EnterpriseCopier] -.->|implements| Fat
    end

    subgraph "ISP Compliant: Segregated Role Interfaces"
        ClientP2[Print Client] --> Printer["<<interface>>\nPrinter\n+print()"]
        ClientF2[Fax Client] --> Fax["<<interface>>\nFaxMachine\n+fax()"]
        ClientS2[Scan Client] --> Scanner["<<interface>>\nScanner\n+scan()"]

        Inkjet2[BasicInkjetPrinter] -.->|implements| Printer
        Copier2[EnterpriseCopier] -.->|implements| Printer
        Copier2 -.->|implements| Scanner
        Copier2 -.->|implements| Fax
    end
```

---

## 4 The Problem Without It

Here is the monolithic `MultiFunctionDevice` code. Notice how `BasicInkjetPrinter` is forced to implement operations it physically cannot support:

```java
// Naive Monolithic Interface violating ISP
public interface MultiFunctionDevice {
    void print(String doc);
    void scan(String doc);
    void fax(String doc);
    void staple();
}

// Low-cost hardware forced to implement methods it cannot support
public class BasicInkjetPrinter implements MultiFunctionDevice {
    @Override
    public void print(String doc) {
        System.out.println("Printing document: " + doc);
    }

    @Override
    public void scan(String doc) {
        throw new UnsupportedOperationException("Inkjet printer cannot scan!");
    }

    @Override
    public void fax(String doc) {
        throw new UnsupportedOperationException("Inkjet printer cannot fax!");
    }

    @Override
    public void staple() {
        throw new UnsupportedOperationException("Inkjet printer cannot staple!");
    }
}
```

### The Change Request & Catastrophic Blast Radius
- The Fax department adds high-resolution secure faxing: `void fax(String doc, EncryptionKey key)`.
- Because `MultiFunctionDevice` changed, `BasicInkjetPrinter.java` fails compilation! A developer working on ink delivery must stop their work to fix a fax method signature.
- Client code that only prints is coupled to fax libraries and encryption binaries.

### The Failure Output
```text
Printing document: QuarterlyReport.pdf
[RUNTIME EXCEPTION DETONATION]
java.lang.UnsupportedOperationException: Inkjet printer cannot fax!
    at com.design.solid.isp.BasicInkjetPrinter.fax(BasicInkjetPrinter.java:18)
    at com.design.solid.isp.BatchLegalNotifier.notifyAll(BatchLegalNotifier.java:42)
[BUILD STATUS] FAILED: BasicInkjetPrinter.java does not implement new method fax(String, EncryptionKey)!
```

---

## 5 How It Solves It

We break down the monolithic interface into discrete, cohesive role interfaces: `Printer`, `Scanner`, and `FaxMachine`. Devices implement only the contracts they actually support, and client services depend exclusively on the exact capability they require.

### Java Implementation (Java 25 LTS)

```java
package com.design.solid.isp;

// Segregated Role Interfaces
public interface Printer {
    void print(String doc);
}

public interface Scanner {
    void scan(String doc);
}

public interface FaxMachine {
    void fax(String doc);
}

// Basic printer implements ONLY Printer
public class BasicInkjetPrinter implements Printer {
    @Override
    public void print(String doc) {
        System.out.println("Java: Printing: " + doc);
    }
}

// High-end multi-function device implements all three roles
public class EnterpriseCopier implements Printer, Scanner, FaxMachine {
    @Override
    public void print(String doc) {
        System.out.println("Java: Copier Printing: " + doc);
    }

    @Override
    public void scan(String doc) {
        System.out.println("Java: Copier Scanning: " + doc);
    }

    @Override
    public void fax(String doc) {
        System.out.println("Java: Copier Faxing: " + doc);
    }
}

// Client services depend ONLY on the role they consume
public class PrintSpooler {
    public void submitJob(Printer printer, String doc) {
        printer.print(doc);
    }
}

public class IspSolutionApp {
    public static void main(String[] args) {
        PrintSpooler spooler = new PrintSpooler();
        Printer inkjet = new BasicInkjetPrinter();
        EnterpriseCopier copier = new EnterpriseCopier();

        // Spooler accepts ANY printer, whether single-function or multi-function
        spooler.submitJob(inkjet, "Budget2026.pdf");
        spooler.submitJob(copier, "AuditReport.pdf");
        copier.scan("Receipt.png");
    }
}
```

### TypeScript Implementation (Node 24 Native)

```typescript
interface Printer {
    print(doc: string): void;
}

interface Scanner {
    scan(doc: string): void;
}

interface FaxMachine {
    fax(doc: string): void;
}

class BasicInkjetPrinter implements Printer {
    print(doc: string): void {
        console.log(`TS: Printing: ${doc}`);
    }
}

class EnterpriseCopier implements Printer, Scanner, FaxMachine {
    print(doc: string): void { console.log(`TS: Copier Printing: ${doc}`); }
    scan(doc: string): void { console.log(`TS: Copier Scanning: ${doc}`); }
    fax(doc: string): void { console.log(`TS: Copier Faxing: ${doc}`); }
}

class PrintSpooler {
    submitJob(printer: Printer, doc: string): void {
        printer.print(doc);
    }
}

// Execution
const spooler = new PrintSpooler();
const inkjet = new BasicInkjetPrinter();
const copier = new EnterpriseCopier();

spooler.submitJob(inkjet, "Budget2026.pdf");
spooler.submitJob(copier, "AuditReport.pdf");
copier.scan("Receipt.png");
```

### Python Implementation (Python 3.14)

```python
from typing import Protocol

class Printer(Protocol):
    def print_doc(self, doc: str) -> None: ...

class Scanner(Protocol):
    def scan_doc(self, doc: str) -> None: ...

class BasicInkjetPrinter:
    def print_doc(self, doc: str) -> None:
        print(f"Python: Printing: {doc}")

class EnterpriseCopier:
    def print_doc(self, doc: str) -> None:
        print(f"Python: Copier Printing: {doc}")

    def scan_doc(self, doc: str) -> None:
        print(f"Python: Copier Scanning: {doc}")

class PrintSpooler:
    def submit_job(self, printer: Printer, doc: str) -> None:
        printer.print_doc(doc)

if __name__ == "__main__":
    spooler = PrintSpooler()
    inkjet = BasicInkjetPrinter()
    copier = EnterpriseCopier()

    spooler.submit_job(inkjet, "Budget2026.pdf")
    spooler.submit_job(copier, "AuditReport.pdf")
    copier.scan_doc("Receipt.png")
```

### Real Execution Output
```text
Java: Printing: Budget2026.pdf
Java: Copier Printing: AuditReport.pdf
Java: Copier Scanning: Receipt.png
TS: Printing: Budget2026.pdf
TS: Copier Printing: AuditReport.pdf
TS: Copier Scanning: Receipt.png
Python: Printing: Budget2026.pdf
Python: Copier Printing: AuditReport.pdf
Python: Copier Scanning: Receipt.png
```

---

## 6 How It Works

ISP functions by decoupling caller needs from concrete object taxonomy:

1. **Client-Driven Interface Design:** Interfaces belong to their **clients**, not to the classes that implement them. A class might provide 50 methods, but if Client A only requires `save()`, Client A defines a role interface with just `save()`.
2. **Multiple Interface Inheritance:** Modern object-oriented languages (Java, C#, TypeScript, Python) support implementing multiple interfaces. An `EnterpriseCopier` is conceptually a `Printer`, a `Scanner`, and a `FaxMachine`.
3. **Intersection Types (TypeScript) / Compound Types:** In TypeScript, a consumer requiring both printing and scanning can simply demand an intersection type without creating a new artificial interface:
   ```typescript
   function archiveAndPrint(device: Printer & Scanner) {
       device.scan("doc.pdf");
       device.print("doc.pdf");
   }
   ```

---

## 7 Real-World Examples

### 1. Standard Library: Java `Closeable` and `AutoCloseable`
In the JDK, resource management does not force streams to implement a fat `FileResource` interface with seek, read, write, flush, and close. Instead:
- `java.lang.AutoCloseable`: Defines a single method: `void close() throws Exception`.
- `java.io.Flushable`: Defines `void flush()`.
- `java.lang.Readable`: Defines `int read(CharBuffer cb)`.
Any class—from an in-memory byte buffer to a network socket—can be used inside a `try-with-resources` statement simply by implementing the 1-method `AutoCloseable` interface.

### 2. Go Standard Library: The 1-Method Interfaces (`io.Reader`, `io.Writer`)
The programming language Go is famous for embodying ISP to its purest degree:
```go
type Reader interface {
    Read(p []byte) (n int, err error)
}
type Writer interface {
    Write(p []byte) (n int, err error)
}
```
Because interfaces are single-method, HTTP request bodies, Gzip compressors, TLS sockets, and in-memory byte buffers can all be piped together seamlessly using `io.Copy(writer, reader)`.

### 3. Framework: Spring Framework Aware Interfaces
Spring does not expose a giant `SpringEnvironment` interface that forces every bean to receive all container metadata. Instead, it provides granular marker/callback interfaces:
- `BeanNameAware`: Receives bean ID.
- `ApplicationContextAware`: Receives application context.
- `BeanFactoryAware`: Receives bean factory.
A developer implements only the exact lifecycle callback their component needs.

---

## 8 When to Use and When NOT to Use

| Context | Action | Rationale |
|---|---|---|
| **Public API / SDK Design** | **Keep interfaces lean & segregated** | Third-party consumers will need to mock and implement them. |
| **Methods with unrelated lifecycles** (e.g. read vs admin config) | **Segregate into separate interfaces** | Prevents standard users from depending on admin APIs. |
| **Single-method cohesion** | **Prefer single-method interfaces** | Enables Java `@FunctionalInterface` and lambda expressions. |
| **Highly Cohesive Operations** (e.g., `Iterator.hasNext()` and `next()`) | **Keep together!** | Separating `hasNext` from `next` makes no sense; they are intrinsically coupled. |

### Diagnostic Signs of ISP Violations
- Classes implementing an interface with empty bodies (`{ /* not supported */ }`).
- Implementations throwing `UnsupportedOperationException` or returning `null`.
- Unit test mocks requiring 20 mock stub definitions when the test only exercises 1 method.
- An interface with 15+ methods that serves 5 completely different customer use cases.

---

## 9 Variations and Language Idioms

### Java: Functional Interfaces & Default Methods
In Java 8+, single-method interfaces can be annotated with `@FunctionalInterface`, allowing them to be satisfied directly by lambdas:
```java
@FunctionalInterface
public interface DataFilter<T> {
    boolean accept(T item);
}
```
Java also allows `default` methods in interfaces to provide backwards compatibility without breaking existing implementers, although this must not be abused to bloat interfaces.

### TypeScript: Structural Intersection
TypeScript doesn't require explicit `implements` clauses. It validates compatibility structurally. Callers can demand exactly the slice of methods they require:
```typescript
type Auditable = { auditId: string; logAccess(): void };
function verify(item: Auditable) { item.logAccess(); }
```

### Python: Mypy Protocols and ABCs
Python uses `typing.Protocol` for compile-time structural checking. You can define granular 1-method protocols without runtime inheritance overhead:
```python
class Closable(Protocol):
    def close(self) -> None: ...
```

---

## 10 Relationship to Other Principles and Patterns

- **ISP is SRP for Interfaces:** SRP states: "A class should have one reason to change." ISP states: "An interface should represent one coherent role for a client."
- **ISP Prevents LSP Violations:** When an interface is too fat, implementers throw `UnsupportedOperationException`, directly causing an LSP violation. Segregating the interface eliminates the violation.
- **Adapter Pattern:** When forced to work with a third-party fat interface, wrap it in an Adapter that exposes only the segregated role interface your client needs.

---

## 11 Common Mistakes and Anti-Patterns

| Anti-Pattern | Description | Production Consequence | Correct Remedy |
|---|---|---|---|
| **Header Interface** | Generating an interface that blindly clones every public method of a class. | Interface bloat, tight coupling, zero semantic role abstraction. | Define interfaces based on what callers need, not what a class has. |
| **The 1-Method Fetish** | Splitting every single method into its own separate interface regardless of cohesion. | Proliferation of micro-interfaces; loss of conceptual cohesion. | Group methods that operate on the same cohesive invariant (e.g., `push`/`pop`). |
| **Throwing Stubs** | Stubbing out unwanted interface methods with `throw new UnsupportedOperationException()`. | Runtime crashes when callers treat the instance polymorphically. | Decompose interface so the class only implements valid operations. |
| **Polluting Domain with Infrastructure** | Adding `serializeToXml()` or `backupToS3()` to a domain `User` interface. | Binds business logic to infrastructure and serialization formats. | Segregate persistence and serialization into infrastructure interfaces. |
| **Ignoring Default Methods** | Bloating an interface with 10 helper methods instead of using default/extension methods. | Forces implementers to write repetitive boilerplate. | Provide default implementations or standalone utility functions. |
| **Role Interface Collision** | Defining duplicate identical interfaces (`UserFetcher`, `UserLoader`, `UserFinder`). | Naming confusion and duplicate boilerplate. | Consolidate identical role semantics within the same architectural layer. |

---

## 12 Testing and Design Verification

Fat interfaces make unit testing agonizing. If a method accepts `MultiFunctionDevice`, creating a test double requires implementing or mocking 10 irrelevant methods.

Under ISP, writing a test double takes **two lines of code**:

```java
// Testing PrintSpooler requires mocking ONLY the Printer interface!
class PrintSpoolerTest {
    @Test
    void shouldPrintDocumentCorrectly() {
        PrintSpooler spooler = new PrintSpooler();
        
        // Lambda implementation satisfies Printer interface directly!
        List<String> printedDocs = new ArrayList<>();
        Printer testPrinter = doc -> printedDocs.add(doc);

        spooler.submitJob(testPrinter, "TestDoc.txt");

        assertEquals(1, printedDocs.size());
        assertEquals("TestDoc.txt", printedDocs.get(0));
    }
}
```

---

## 13 Performance, Concurrency and Security Notes

### Performance
- Interface segregation enables **Data-Oriented Design**: separating read-heavy interfaces from write-heavy interfaces allows caching layers to intercept read interfaces without locking write pipelines.

### Concurrency
- Segregating read and write interfaces (Command Query Segregation) allows read operations to run lock-free (`ReentrantReadWriteLock.readLock()`), dramatically improving concurrent throughput in high-contention systems.

### Security: Principle of Least Privilege
- ISP enforces security at the type level. If a worker service only needs to read customer records, inject `CustomerReader` instead of `CustomerRepository`. Even if a developer accidentally attempts to call `delete()` or `update()`, the code **will not compile**. The security boundary is enforced by the compiler.

---

## 14 Interview Questions

<details>
<summary><strong>1. What is the fundamental difference between SRP and ISP?</strong></summary>

- **SRP (Single Responsibility Principle)** is focused on **classes and modules** and their reasons to change (actors). It asks: *"Does this class have only one master?"*
- **ISP (Interface Segregation Principle)** is focused on **interfaces and callers**. It asks: *"Is any client forced to see or depend on methods it does not use?"*
</details>

<details>
<summary><strong>2. How did Robert C. Martin discover ISP while working at Xerox?</strong></summary>

Xerox had a monolithic `Job` software class used by all printer and scanner hardware. Whenever changes were made to support a new stapler accessory, the entire firmware had to be recompiled and re-tested, often causing regressions in printing. Uncle Bob solved this by splitting `Job` into client-specific interfaces (`PrintJob`, `ScanJob`), isolating unrelated subsystems from recompilation and regressions.
</details>

<details>
<summary><strong>3. How does ISP directly prevent LSP violations?</strong></summary>

When an interface contains methods that a concrete class cannot physically support (e.g. `fax()` on an inkjet printer), the implementer is forced to override that method to throw `UnsupportedOperationException`. This breaks the caller's behavioral contract (LSP). Segregating the interface removes unsupported methods, ensuring that every implemented method fulfills its contract.
</details>

<details>
<summary><strong>4. Why is Go's interface philosophy considered the ultimate expression of ISP?</strong></summary>

Go interfaces are satisfied implicitly (structural subtyping) and are typically tiny (1 to 2 methods, like `io.Reader` and `io.Writer`). In Go, packages define the interfaces they consume rather than the packages that implement them, ensuring that interfaces remain strictly tailored to client needs.
</details>

<details>
<summary><strong>5. What is a "Role Interface" versus a "Header Interface"?</strong></summary>

A **Header Interface** simply mirrors all public methods of a concrete class (`class OrderServiceImpl` implementing `interface OrderService`). A **Role Interface** is tailored to a specific client interaction or role (e.g., `OrderCreator`, `OrderCanceller`, `OrderExporter`), containing only the methods needed for that role.
</details>

<details>
<summary><strong>6. Can an interface be too segregated? What is the danger?</strong></summary>

Yes. Over-segregating leads to "Interface Explosion", where every single method has its own interface. This creates massive navigation overhead, boilerplate code, and destroys cohesion for operations that inherently belong together (e.g. splitting `Iterator` into `HasNextInterface` and `NextInterface`).
</details>

<details>
<summary><strong>7. How does ISP help in microservice and REST/gRPC API design?</strong></summary>

In distributed systems, fat APIs force clients to download bloated payloads and handle deprecation cycles for endpoints they never call. By using BFFs (Backend for Frontend) or GraphQL field selection, clients receive segregated API contracts tailored to their specific consumption requirements.
</details>

<details>
<summary><strong>8. How does ISP improve unit testability?</strong></summary>

When a class depends on a lean, segregated interface with 1 or 2 methods, creating a mock or fake for unit tests requires only a couple lines of code or a single lambda expression. Fat interfaces force test authors to write 20 lines of mock stubs just to satisfy the compiler.
</details>

---

## 15 Practice Exercises

### Exercise 1: Spot the ISP Violation (Easy)
Identify the ISP violation in this database repository contract:
```java
public interface UserRepository {
    User findById(String id);
    List<User> findAll();
    void save(User user);
    void delete(String id);
    void purgeAllAuditLogs();
    void rebuildDatabaseIndexes();
    void backupToS3Bucket(String bucketName);
}
```

<details>
<summary><strong>Solution</strong></summary>

The interface couples basic CRUD operations with administrative maintenance operations (`purgeAllAuditLogs`, `rebuildDatabaseIndexes`, `backupToS3Bucket`). A standard web checkout service only needs `findById` or `save`, but is exposed to catastrophic administrative methods.
**Fix:** Segregate into `UserReadOnlyRepository`, `UserWriteRepository`, and `DatabaseMaintenanceOperations`.
</details>

---

### Exercise 2: Refactor UI Event Listeners (Medium)
Refactor this monolithic UI listener interface in TypeScript so that click handlers don't have to implement mouse wheel and drag events:

```typescript
interface UIEventListener {
    onClick(x: number, y: number): void;
    onDoubleClick(x: number, y: number): void;
    onMouseWheel(delta: number): void;
    onDragStart(x: number, y: number): void;
    onDragEnd(x: number, y: number): void;
    onKeyDown(keyCode: number): void;
}
```

<details>
<summary><strong>Solution</strong></summary>

```typescript
interface ClickListener {
    onClick(x: number, y: number): void;
    onDoubleClick?(x: number, y: number): void;
}

interface WheelListener {
    onMouseWheel(delta: number): void;
}

interface DragListener {
    onDragStart(x: number, y: number): void;
    onDragEnd(x: number, y: number): void;
}

interface KeyboardListener {
    onKeyDown(keyCode: number): void;
}
```
Components implement only the event roles they listen to.
</details>

---

### Exercise 3: Python File Processing Pipeline (Hard)
Design a file processing pipeline in Python adhering to ISP where:
1. Some files are streamable (`read_chunk(size)`).
2. Some files are seekable (`seek(offset)`).
3. Some files are compressible (`compress(algorithm)`).
4. Write the structural protocols and a function that processes streamable-only files without knowing about seek or compress.

<details>
<summary><strong>Solution</strong></summary>

```python
from typing import Protocol

class Streamable(Protocol):
    def read_chunk(self, size: int) -> bytes: ...

class Seekable(Protocol):
    def seek(self, offset: int) -> None: ...

class Compressible(Protocol):
    def compress(self, algorithm: str) -> bytes: ...

# Function depends STRICTLY on the Streamable role
def stream_to_network(source: Streamable, chunk_size: int = 4096) -> None:
    while True:
        chunk = source.read_chunk(chunk_size)
        if not chunk:
            break
        print(f"Streaming {len(chunk)} bytes to network socket")
```
</details>

---

## 16 Summary / Cheat Sheet

```
+--------------------------------------------------------------------------+
|                       INTERFACE SEGREGATION PRINCIPLE                    |
+--------------------------------------------------------------------------+
| CORE RULE      : Clients should not be forced to depend upon interfaces   |
|                  that they do not use.                                   |
| KEY PATTERN    : Role Interfaces (small, client-specific contracts).      |
| SYMPTOMS       : Empty method stubs ({}), UnsupportedOperationException,|
|                  massive mock setup in unit tests, recompilation ripples.|
| BEST PRACTICE  : Keep interfaces lean (1-3 cohesive methods). Compose    |
|                  multiple interfaces when an entity fulfills many roles.  |
| CAUTION        : Do not split apart methods that share an invariant      |
|                  (e.g., Iterator hasNext/next).                          |
+--------------------------------------------------------------------------+
```

---

## 17 Further Reading

- Robert C. Martin, *Clean Architecture*, Prentice Hall, 2017 (Chapter 10: Interface Segregation Principle).
- Martin Fowler, *Role Interface*, martinfowler.com/bliki/RoleInterface.html.
- Alan A. A. Donovan, Brian W. Kernighan, *The Go Programming Language* (Chapter on Interfaces), Addison-Wesley, 2015.

### In Your Language
| Language Track | Deep Dive Reference |
|---|---|
| Java | [Java OOP & Design Patterns](file:///e:/Learning/engineering-notes/01-languages/java/24-design-patterns/README.md) |
| TypeScript | [TypeScript Design Patterns](file:///e:/Learning/engineering-notes/01-languages/typescript/17-design-patterns/README.md) |
| Python | [Python Design Patterns](file:///e:/Learning/engineering-notes/01-languages/python/23-design-patterns/README.md) |

---

[← Previous: 03 - Liskov Substitution Principle](file:///e:/Learning/engineering-notes/08-solid-design-patterns/02-solid-principles/03-liskov-substitution.md) | [Module 02 Index](file:///e:/Learning/engineering-notes/08-solid-design-patterns/02-solid-principles/README.md) | [Next: 05 - Dependency Inversion Principle →](file:///e:/Learning/engineering-notes/08-solid-design-patterns/02-solid-principles/05-dependency-inversion.md)
