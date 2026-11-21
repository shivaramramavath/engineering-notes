# Chain of Responsibility Pattern in Modern Java

> Decoupling request senders from sequential handlers: linear processing pipelines, Servlet Filters, Spring Interceptors, and recursive vs. iterative handler chains.

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

### What is the Chain of Responsibility Pattern?
The Chain of Responsibility is a behavioral design pattern that allows you to pass requests along a **dynamic chain of handlers**. Upon receiving a request, each handler decides either to process the request or to pass it to the next handler in the chain.

In enterprise software engineering:
- HTTP Middleware & Servlet Filters (`FilterChain`)
- Security & Authentication Filters (IP Check $\to$ Rate Limit $\to$ JWT Auth $\to$ RBAC)
- Multi-tier support escalation ticket routing
- Request validation and sanitization pipelines

### Why Chain of Responsibility is Essential
Without this pattern, sequential request verification logic becomes a massive, monolithic method:
```java
// Anti-pattern: Hardcoded sequential validation
public void handle(Request req) {
    if (!checkIp(req)) return;
    if (!checkRateLimit(req)) return;
    if (!checkAuth(req)) return;
    if (!validatePayload(req)) return;
    execute(req);
}
```
Chain of Responsibility extracts each check into an independent, reusable, and reorderable handler class adhering to the Single Responsibility Principle.

---

## 2. Core Concepts

### Handler, Concrete Handlers, and Next Pointer
1. **Handler Interface**: Declares method to process request (`handle(Request)`) and method to set successor (`setNext(Handler)`).
2. **Base Handler**: Optional helper class holding the `next` reference and default forwarding logic.
3. **Concrete Handlers**: Inspect request; if condition met, handle or forward to `next`.

### Two Varieties of Handlers
- **Pass-Through / Pipeline**: Every handler inspects/transforms the request and passes it along (e.g. Logging $\to$ Compression $\to$ Auth).
- **Single-Consumer**: The first handler capable of handling the request consumes it and terminates the chain (e.g. Level 1 Tech Support $\to$ Level 2 $\to$ Lead Engineer).

---

## 3. How It Works Internally

### Linear Pipeline Architecture
```
[ Incoming HTTP Request ]
           |
           v
[ RateLimitHandler ]
  - Checks client IP quota
  - Pass? -> calls next.handle(req)
           |
           v
[ AuthenticationHandler ]
  - Validates JWT signature
  - Pass? -> calls next.handle(req)
           |
           v
[ PayloadValidationHandler ]
  - Validates JSON schema
  - Pass? -> calls next.handle(req)
           |
           v
[ Target Business Controller ]
```

---

## 4. Syntax & Usage

### 1. Classic Base Handler with Fluent Chaining
```java
public abstract class Middleware {
    private Middleware next;

    public Middleware linkWith(Middleware next) {
        this.next = next;
        return next;
    }

    public abstract boolean check(String user, String pass);

    protected boolean checkNext(String user, String pass) {
        if (next == null) return true;
        return next.check(user, pass);
    }
}
```

---

## 5. Practical Examples

### Example 1: Security Authorization Middleware Pipeline
**Goal:** Build a modular security pipeline checking IP Whitelisting, Rate Limiting, and Role Permission.

```java
import java.util.Objects;

public record HttpRequest(String ip, String role, String payload) {}

// Handler Interface
public interface SecurityHandler {
    void handle(HttpRequest request);
    void setNext(SecurityHandler next);
}

// Base Handler
public abstract class AbstractSecurityHandler implements SecurityHandler {
    private SecurityHandler next;

    @Override
    public void setNext(SecurityHandler next) {
        this.next = next;
    }

    protected void forward(HttpRequest request) {
        if (next != null) {
            next.handle(request);
        }
    }
}

// Concrete Handler 1: IP Whitelist Check
public class IpCheckHandler extends AbstractSecurityHandler {
    @Override
    public void handle(HttpRequest request) {
        if (request.ip().startsWith("10.")) {
            throw new SecurityException("Private IP access blocked: " + request.ip());
        }
        System.out.println("IP Check Passed: " + request.ip());
        forward(request);
    }
}

// Concrete Handler 2: RBAC Role Check
public class RoleCheckHandler extends AbstractSecurityHandler {
    @Override
    public void handle(HttpRequest request) {
        if (!"ADMIN".equals(request.role())) {
            throw new SecurityException("Insufficient privileges: " + request.role());
        }
        System.out.println("Role Check Passed: ADMIN");
        forward(request);
    }
}

// Execution Pipeline Assembly:
// SecurityHandler ip = new IpCheckHandler();
// SecurityHandler role = new RoleCheckHandler();
// ip.setNext(role);
// ip.handle(new HttpRequest("192.168.1.1", "ADMIN", "data"));
```

### Example 2: Modern Java Functional Chain with `UnaryOperator`
**Goal:** Chain pipeline handlers using standard Java 8 functional composition (`Function.andThen`).

```java
import java.util.function.UnaryOperator;

public class FunctionalChainDemo {
    public static void main(String[] args) {
        UnaryOperator<String> sanitize = text -> text.replace("<", "&lt;");
        UnaryOperator<String> trim = String::trim;
        UnaryOperator<String> appendAudit = text -> text + " [VERIFIED]";

        // Assemble chain via andThen
        UnaryOperator<String> pipeline = sanitize
                .andThen(trim)
                .andThen(appendAudit);

        String result = pipeline.apply("  <script>alert()</script>  ");
        System.out.println(result); // &lt;script>alert()&lt;/script> [VERIFIED]
    }
}
```

---

### Example 4: Spring WebFilter Implementation
**Goal:** Implement a non-blocking reactive WebFilter in modern Spring WebFlux.

```java
// Conceptual WebFilter chain:
public interface ModernFilter {
    void filter(String request, ModernFilterChain chain);
}
public interface ModernFilterChain {
    void proceed(String request);
}

## 6. Best Practices

1. **Provide a Convenient Linking Method**: Return `next` or `this` from `linkWith()` to enable clean builder-style chaining (`first.linkWith(second).linkWith(third)`).
2. **Ensure Unhandled Requests Don\'t Vanish Silently**: In single-consumer chains, provide a default fallback handler at the tail of the chain.
3. **Keep Handlers Independent**: Handlers should not rely on the specific ordering of other handlers unless explicitly designed as a dependency pipeline.
4. **Beware of Broken Chains**: If a handler forgets to call `checkNext()` or `forward()`, request processing silently terminates.
5. **Use Standard Servlet / Filter Interfaces in Web Apps**: Do not reinvent custom chains in web apps; leverage `jakarta.servlet.Filter` or Spring `HandlerInterceptor`.

---

## 7. Common Mistakes & Pitfalls

| Mistake | Why It Fails | Secure Fix |
| :--- | :--- | :--- |
| **Forgetting to forward request** | Pipeline halts silently; request never reaches business controller. | Always call `forward(request)` on success. |
| **Circular Chains** | A handler points back to an ancestor; infinite loop crashes thread. | Prevent cycles during chain assembly. |
| **Uncaught exceptions breaking entire pipeline** | Exception in non-critical handler (e.g. analytics) aborts entire request. | Catch and log non-fatal exceptions in handler. |
| **Modifying shared mutable request objects** | Handlers concurrently modifying request cause race conditions. | Use immutable request records. |

---

## 8. Performance & Security Notes

### Overhead of Deep Filter Chains
Modern web servers (Tomcat, Netty) run requests through 15–30 servlet filters.
- If handlers perform fast in-memory checks (header validation, boolean tests), stack traversal takes $< 50\text{ nanoseconds}$.
- Avoid blocking I/O (database queries, remote REST calls) inside intermediate filter chains.

---

## 9. Comparison / When to Use What

| Pattern | Chain Structure | Termination Condition |
| :--- | :--- | :--- |
| **Chain of Responsibility** | Dynamic linked sequence | Handler decides to consume or pass |
| **Decorator** | Recursive nested wrappers | Always passes through all layers |
| **Composite** | Recursive Tree | Aggregates all children |
| **Command** | Single standalone action | Executes and returns |

---

## 10. Interview Questions & Answers

### Q1: What is the classic example of Chain of Responsibility in the Java ecosystem?
<details>
<summary>View Answer</summary>
The Java Servlet API\'s `jakarta.servlet.FilterChain` and `jakarta.servlet.Filter`. When an HTTP request enters a Java web application, it passes through a chain of filters (`SecurityFilter`, `CorsFilter`, `EncodingFilter`, `LoggingFilter`). Each filter inspects or modifies the request/response and calls `chain.doFilter(request, response)` to pass control to the next filter until the target Servlet is reached.
</details>

### Q2: How does Chain of Responsibility differ from the Decorator pattern?
<details>
<summary>View Answer</summary>
- **Decorator**: Focuses on adding new responsibilities or modifying behavior. All decorators in the chain are executed unconditionally in an onion-skin wrapping order.
- **Chain of Responsibility**: Handlers can terminate execution at any point without calling the rest of the chain. Handlers can also operate independently without wrapping the core component.
</details>

### Q3: What happens if a request reaches the end of the chain without being handled?
<details>
<summary>View Answer</summary>
In a single-consumer chain, an unhandled request falls off the end of the chain. If not anticipated, this leads to silent failures. Best practice is to attach a default fallback handler at the tail of the chain that logs a warning, returns an error response, or throws an `UnsupportedOperationException`.
</details>

### Q4: Can Chain of Responsibility be implemented iteratively instead of recursively?
<details>
<summary>View Answer</summary>
Yes. Instead of each handler holding a reference to `next` (recursive pointer chaining), an invoker can store handlers in a `List<Handler>`. The invoker iterates through the list, evaluating each handler in a `for` loop until a handler consumes the request.
</details>

### Q5: How do Java 8 Lambdas simplify Chain of Responsibility?
<details>
<summary>View Answer</summary>
When handlers are single-method functional components, they can be represented as `Function<T, T>` or `Predicate<T>`. Chains can be composed dynamically using `Function.andThen()` or `Predicate.and()`, completely eliminating the need for abstract base classes and next pointer management.
</details>

---

## 11. Practice Exercises

### Exercise 1: Build a Multi-Tier Support Ticket Escalation Chain
**Task:** Create Level 1, Level 2, and Director handlers that resolve tickets based on difficulty level (1, 2, 3).
<details>
<summary>View Solution</summary>

```java
public record Ticket(int difficulty, String issue) {}

public abstract class SupportHandler {
    private SupportHandler next;
    public SupportHandler setNext(SupportHandler n) { this.next = n; return n; }

    public void handle(Ticket t) {
        if (!resolve(t) && next != null) {
            next.handle(t);
        }
    }
    protected abstract boolean resolve(Ticket t);
}

public class Level1Support extends SupportHandler {
    @Override protected boolean resolve(Ticket t) {
        if (t.difficulty() == 1) { System.out.println("L1 resolved: " + t.issue()); return true; }
        return false;
    }
}

public class DirectorSupport extends SupportHandler {
    @Override protected boolean resolve(Ticket t) {
        System.out.println("Director resolved: " + t.issue()); return true;
    }
}
```
</details>

### Exercise 2: Functional Authentication Chain
**Task:** Combine three `Predicate<String>` checks (Length, SpecialChar, Number) via `.and()`.
<details>
<summary>View Solution</summary>

```java
import java.util.function.Predicate;

public class PasswordValidatorChain {
    public static Predicate<String> createChain() {
        Predicate<String> minLength = s -> s != null && s.length() >= 8;
        Predicate<String> hasNumber = s -> s.chars().anyMatch(Character::isDigit);
        Predicate<String> hasSpecial = s -> s.chars().anyMatch(c -> !Character.isLetterOrDigit(c));

        return minLength.and(hasNumber).and(hasSpecial);
    }
}
```
</details>

### Exercise 3: Currency Dispenser ATM Chain
**Task:** Dispense banknotes (\$50, \$20, \$10) using a Chain of Responsibility.
<details>
<summary>View Solution</summary>

```java
public abstract class Dispenser {
    protected Dispenser next;
    public void setNext(Dispenser n) { this.next = n; }
    public abstract void dispense(int amount);
}

public class BanknoteDispenser extends Dispenser {
    private final int denom;
    public BanknoteDispenser(int denom) { this.denom = denom; }

    @Override
    public void dispense(int amount) {
        if (amount >= denom) {
            int count = amount / denom;
            int rem = amount % denom;
            System.out.printf("Dispensing %d x $%d bills%n", count, denom);
            if (rem != 0 && next != null) next.dispense(rem);
        } else if (next != null) {
            next.dispense(amount);
        }
    }
}
```
</details>

---

## 12. Summary / Cheat Sheet

- **Core Goal**: Decouple request sender from sequential handlers.
- **Flexibility**: Dynamically reorder, add, or remove handlers at runtime.
- **Servlet Filters**: `jakarta.servlet.Filter` is the canonical enterprise chain.
- **Termination**: Handlers decide whether to forward to `next` or terminate chain.
- **Functional**: Use `Function.andThen` or `Predicate.and` for lightweight chains.
- **Fail-Safe**: Provide a terminal fallback handler for unhandled requests.

---

## 13. Further Reading
- GoF Design Patterns: [Chain of Responsibility](https://www.oreilly.com/library/view/design-patterns-elements/0201633612/)
- Refactoring Guru: [Chain of Responsibility Pattern](https://refactoring.guru/design-patterns/chain-of-responsibility)
- Jakarta Servlet Specification: [Servlet Filters](https://jakarta.ee/specifications/servlet/)
