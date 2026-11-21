# Template Method Pattern in Modern Java

> Defining the skeleton of an algorithm in a superclass: invariant steps vs. hook methods, Hollywood Principle ("Don't call us, we'll call you"), and functional composition alternatives.

| Metadata | Details |
| :--- | :--- |
| **Level** | Core Design Pattern / Behavioral |
| **Prerequisites** | [00_solid-principles](../23-design-and-clean-code/00_solid-principles.md), [04_oop](../../04-oop/README.md) |
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

### What is the Template Method Pattern?
The Template Method is a behavioral design pattern that defines the **skeleton of an algorithm in an abstract base class**, deferring some steps to subclasses. Subclasses can redefine certain steps of the algorithm without changing its overall structure.

In enterprise software engineering:
- Standard data processing pipelines: Extract $\to$ Transform $\to$ Validate $\to$ Load.
- Game loop or turn processing lifecycles.
- Spring Framework template helpers (`JdbcTemplate`, `TransactionTemplate`).

### The Hollywood Principle
Template Method enforces the Hollywood Principle:
> *"Don\'t call us, we\'ll call you."*

The high-level abstract class controls the entire flow of execution and calls subclass methods only when needed, preventing subclasses from altering the invariant sequence of the algorithm.

---

## 2. Core Concepts

### Abstract Methods, Concrete Methods, and Hook Methods
1. **The Template Method**: Declared `final` in the abstract class. Orchestrates the algorithm steps.
2. **Abstract Steps**: Must be implemented by subclasses (`abstract void parseData()`).
3. **Concrete Steps**: Shared logic implemented directly in the base class (`logAudit()`).
4. **Hook Methods**: Optional steps with default (often empty) implementations that subclasses can override if desired (`boolean shouldFilter() { return false; }`).

---

## 3. How It Works Internally

### Algorithm Execution Flow
```
[ Base Class: executePipeline() (FINAL) ]
      |
      | 1. openConnection()       (Concrete Base Step)
      v
      | 2. fetchData()            (Abstract Step -> Subclass overrides)
      v
      | 3. if (shouldValidate())  (Hook Method -> Subclass optional)
      |        validate()
      v
      | 4. writeReport()          (Abstract Step -> Subclass overrides)
      v
      | 5. closeConnection()      (Concrete Base Step)
```

By marking `executePipeline()` as `final`, subclasses cannot break the invariant order of opening and closing connections.

---

## 4. Syntax & Usage

### 1. Classic Template Method Hierarchy
```java
public abstract class DataProcessor {
    // The Template Method: final prevents subclasses from altering the sequence
    public final void process() {
        read();
        transform();
        if (shouldAudit()) {
            audit();
        }
        write();
    }

    protected abstract void read();
    protected abstract void transform();
    protected abstract void write();

    // Hook Method with default implementation
    protected boolean shouldAudit() { return true; }
    protected void audit() { System.out.println("Auditing process run."); }
}
```

---

## 5. Practical Examples

### Example 1: Enterprise Report Generator (PDF vs. CSV)
**Goal:** Guarantee header, footer, and database transaction lifecycles while allowing subclasses to format data.

```java
import java.util.List;

public abstract class AbstractReportGenerator {
    // The Template Method
    public final byte[] generateReport(String reportTitle) {
        System.out.println("Acquiring report database lock...");
        try {
            List<String> data = extractData();
            StringBuilder sb = new StringBuilder();
            
            sb.append(formatHeader(reportTitle));
            sb.append(formatBody(data));
            sb.append(formatFooter());

            return sb.toString().getBytes();
        } finally {
            System.out.println("Releasing report database lock.");
        }
    }

    protected abstract List<String> extractData();
    protected abstract String formatHeader(String title);
    protected abstract String formatBody(List<String> data);

    // Hook with default behavior
    protected String formatFooter() {
        return "--- End of Report ---\n";
    }
}

// Concrete Subclass 1: CSV
public class CsvReportGenerator extends AbstractReportGenerator {
    @Override
    protected List<String> extractData() { return List.of("Row 1", "Row 2"); }
    @Override
    protected String formatHeader(String title) { return "CSV: " + title + "\n"; }
    @Override
    protected String formatBody(List<String> data) { return String.join(",", data) + "\n"; }
}

// Concrete Subclass 2: HTML
public class HtmlReportGenerator extends AbstractReportGenerator {
    @Override
    protected List<String> extractData() { return List.of("Data A", "Data B"); }
    @Override
    protected String formatHeader(String title) { return "<h1>" + title + "</h1>\n"; }
    @Override
    protected String formatBody(List<String> data) { return "<ul><li>" + String.join("</li><li>", data) + "</li></ul>\n"; }
    @Override
    protected String formatFooter() { return "<footer>Generated by System</footer>\n"; }
}
```

### Example 2: Modern Functional Composition Alternative (Loan Pattern)
**Goal:** Replace inheritance-based Template Method with a functional lambda loan pattern (`Consumer` or `Function`).

```java
import java.util.function.Consumer;

public class ResourceLifecycleTemplate {
    public static void executeWithResource(Consumer<String> action) {
        System.out.println("OPEN: Allocating expensive hardware resource");
        try {
            action.accept("ACTIVE_RESOURCE_HANDLE");
        } finally {
            System.out.println("CLOSE: Releasing expensive hardware resource");
        }
    }

    public static void main(String[] args) {
        // Subclassing completely replaced by a clean lambda!
        executeWithResource(handle -> {
            System.out.println("Using resource: " + handle);
        });
    }
}
```

---

### Example 3: Microservice REST Controller Template Method
**Goal:** Standardize request authentication, audit logging, and exception handling across all API endpoints.

```java
public abstract class AbstractApiController<REQ, RES> {
    public final RES handleRequest(REQ request, String authToken) {
        authenticate(authToken);
        logAuditStart(request);
        try {
            validate(request);
            RES response = execute(request);
            logAuditSuccess(response);
            return response;
        } catch (Exception ex) {
            handleError(ex);
            throw ex;
        }
    }

    private void authenticate(String token) {
        if (token == null || !token.startsWith("Bearer ")) throw new SecurityException("Unauthenticated");
    }
    protected abstract void validate(REQ request);
    protected abstract RES execute(REQ request);
    protected void logAuditStart(REQ req) { System.out.println("Processing: " + req); }
    protected void logAuditSuccess(RES res) { System.out.println("Success: " + res); }
    protected void handleError(Exception ex) { System.err.println("Endpoint failed: " + ex.getMessage()); }
}
```

## 6. Best Practices

1. **Always Declare the Template Method `final`**: Prevent subclasses from overriding the master algorithm sequence.
2. **Minimize Abstract Methods**: If a template requires 10 abstract methods, subclassing becomes exhausting. Use sane hook defaults.
3. **Use Protected Access for Steps**: Internal steps should be `protected`, not `public`. Clients should only call the public template method.
4. **Prefer Functional Loan Pattern for Simple Cleanups**: Use lambda consumers (`executeWith(consumer)`) instead of subclassing for resource management.
5. **Name Hooks Clearly**: Prefix hook methods with `should...`, `is...`, or `before...` to signal their optional nature.

---

## 7. Common Mistakes & Pitfalls

| Mistake | Why It Fails | Secure Fix |
| :--- | :--- | :--- |
| **Omitting `final` on the template method** | Subclasses override the template method, bypassing invariant cleanup steps. | Mark the template method `public final void`. |
| **Too many abstract steps** | Makes subclassing tedious and rigid. | Provide default implementations or use hooks. |
| **Calling template methods from constructors** | Subclass fields are not yet initialized when superclass constructor runs. | Never invoke overridable methods in constructors. |
| **Violating the Liskov Substitution Principle** | Subclass overrides a step and throws `UnsupportedOperationException`. | Redesign step hierarchy so all subclasses support all steps. |

---

## 8. Performance & Security Notes

### Inlining Virtual Template Steps
Template Method relies on virtual dispatch (`invokevirtual`).
- Because a given application typically runs with a known concrete subclass (e.g. `CsvReportGenerator`), HotSpot C2 compiler applies monomorphic devirtualization.
- The template method and all concrete steps are inlined into a single continuous native code block.

---

## 9. Comparison / When to Use What

| Pattern | Mechanism | Focus |
| :--- | :--- | :--- |
| **Template Method** | Inheritance (Subclassing) | Fixes algorithm skeleton; overrides individual steps |
| **Strategy** | Composition (Interfaces) | Swaps the entire algorithm at runtime |
| **Factory Method** | Specialization of Template Method | Focuses specifically on creating objects |

---

## 10. Interview Questions & Answers

### Q1: What is the "Hollywood Principle" and how does it relate to Template Method?
<details>
<summary>View Answer</summary>
The Hollywood Principle ("Don\'t call us, we\'ll call you") states that high-level modules should control the flow of execution and call low-level components, rather than low-level components controlling high-level flow. In the Template Method pattern, the superclass owns the algorithm sequence and calls the subclass\'s overridden methods when required. The subclass never dictates when or how the algorithm proceeds.
</details>

### Q2: Why should the template method itself always be declared `final`?
<details>
<summary>View Answer</summary>
If the template method is not `final`, a subclass can override it and alter the execution sequence, skip mandatory security or validation steps, or omit critical resource cleanup blocks (e.g. `finally { close(); }`). Marking it `final` preserves the invariant structure of the algorithm.
</details>

### Q3: What is a "Hook Method" in the Template Method pattern?
<details>
<summary>View Answer</summary>
A hook method is a method in the abstract base class that has a default (often empty or returning true/false) implementation. Subclasses can override the hook to inject optional behavior at strategic points in the algorithm, but are not forced to implement it if they do not need customization.
</details>

### Q4: How do modern Java Lambdas replace the need for Template Method?
<details>
<summary>View Answer</summary>
The Template Method pattern historically required creating a new concrete subclass for every slight variation in a step. With modern Java functional interfaces (`Consumer`, `Function`, `Supplier`), the base class can simply accept lambdas for individual steps (the "Loan Pattern" or "Execute-Around Pattern"), achieving custom step execution with zero subclassing.
</details>

### Q5: Give an example of Template Method in the Java Standard Library or Spring.
<details>
<summary>View Answer</summary>
1. `java.io.InputStream`: The concrete `read(byte[] b, int off, int len)` method is a template method that repeatedly calls the abstract `read()` method.
2. `java.util.AbstractList`: Implements methods like `addAll()` as template methods relying on `add()`.
3. Spring\'s `JdbcTemplate`: Manages database connections and transactions, calling developer-supplied `RowMapper` steps.
</details>

### Q6: What is the risk of calling an overridable method inside an abstract class constructor?
<details>
<summary>View Answer</summary>
In Java, superclass constructors run *before* subclass constructors. If a superclass constructor invokes an overridable method that is implemented in the subclass, that subclass method executes before the subclass\'s own fields have been initialized, causing `NullPointerExceptions` or corrupted state.
</details>

### Q7: How does Template Method differ from Strategy?
<details>
<summary>View Answer</summary>
Template Method relies on class inheritance to vary parts of an algorithm while keeping the structure fixed. Strategy relies on object composition to replace the entire algorithm dynamically at runtime.
</details>

### Q8: Should the step methods in the abstract base class be `public`?
<details>
<summary>View Answer</summary>
No. Step methods should be `protected` so they are accessible to subclasses, but hidden from external clients. External clients should only invoke the public `final` template method.
</details>

---

## 11. Practice Exercises

### Exercise 1: Build a Game Turn Processor
**Task:** Create an abstract `Game` with `playGame()` template method calling `initialize()`, `takeTurn()`, `isGameOver()`, and `endGame()`.
<details>
<summary>View Solution</summary>

```java
public abstract class Game {
    public final void playGame() {
        initialize();
        while (!isGameOver()) {
            takeTurn();
        }
        endGame();
    }

    protected abstract void initialize();
    protected abstract boolean isGameOver();
    protected abstract void takeTurn();
    protected abstract void endGame();
}
```
</details>

### Exercise 2: Functional Execute-Around Pattern
**Task:** Implement a file processor template using functional `Consumer`.
<details>
<summary>View Solution</summary>

```java
import java.util.function.Consumer;

public class FileTemplate {
    public static void withFile(String filename, Consumer<String> action) {
        System.out.println("Opening file: " + filename);
        try {
            action.accept("CONTENT_BUFFER");
        } finally {
            System.out.println("Closing file: " + filename);
        }
    }
}
```
</details>

### Exercise 3: Hook Method Implementation
**Task:** Add a `shouldSendEmail()` hook to an abstract order processor.
<details>
<summary>View Solution</summary>

```java
public abstract class OrderProcessor {
    public final void processOrder() {
        validate();
        charge();
        if (shouldSendEmail()) {
            sendEmail();
        }
    }
    protected abstract void validate();
    protected abstract void charge();
    protected boolean shouldSendEmail() { return true; } // Hook
    protected void sendEmail() { System.out.println("Email sent"); }
}
```
</details>

---

## 12. Summary / Cheat Sheet

- **Core Goal**: Define algorithm skeleton in superclass; defer steps to subclasses.
- **Always Final**: Declare the main template method `final` to protect algorithm invariants.
- **Hollywood Principle**: Superclass calls subclass steps, never the reverse.
- **Hook Methods**: Optional steps with default implementations.
- **Protected Steps**: Keep individual steps `protected`, not `public`.
- **Modern Alternative**: Use functional lambdas (Loan Pattern) instead of subclassing.

---

## 13. Further Reading
- GoF Design Patterns: [Template Method](https://www.oreilly.com/library/view/design-patterns-elements/0201633612/)
- Refactoring Guru: [Template Method Pattern](https://refactoring.guru/design-patterns/template-method)
- Effective Java: [Item 19: Design and document for inheritance or else prohibit it](https://www.oreilly.com/library/view/effective-java-3rd/9780134686097/)
