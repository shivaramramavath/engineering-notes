# Proxy Pattern in Modern Java

> Controlling object access and lifecycle: Virtual Proxies (lazy loading), Protection Proxies (RBAC), Caching Proxies, JDK Dynamic Proxies (`java.lang.reflect.Proxy`), and ByteBuddy.

| Metadata | Details |
| :--- | :--- |
| **Level** | Core Design Pattern / Structural |
| **Prerequisites** | [00_solid-principles](../23-design-and-clean-code/00_solid-principles.md), [13_advanced-features](../../13-advanced-language-features/README.md) |
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

### What is the Proxy Pattern?
The Proxy pattern is a structural design pattern that provides a surrogate or placeholder for another object to **control access to it**.

A proxy implements the identical interface as the real subject object. When a client invokes a method on the proxy, the proxy intercepts the call, performs pre-processing (security check, cache lookup, lazy instantiation), forwards the request to the real object, and performs post-processing (auditing, transaction commit).

### Common Types of Proxies in Enterprise Java
1. **Virtual Proxy (Lazy Loading)**: Defers the creation of expensive objects (e.g. 50 MB images or database connections) until they are actually needed.
2. **Protection Proxy (Security / RBAC)**: Validates user permissions before allowing access to sensitive operations.
3. **Caching Proxy**: Intercepts requests and returns cached results if available, bypassing expensive backend computations.
4. **Remote Proxy (RPC)**: Represents an object residing in a different JVM address space (gRPC, RMI).
5. **AOP / Transactional Proxy (Spring `@Transactional`)**: Wraps business methods in transaction boundaries (`conn.commit()` / `conn.rollback()`).

---

## 2. Core Concepts

### Subject, RealSubject, and Proxy
- **Subject Interface**: Declares common operations for both `RealSubject` and `Proxy` (`DocumentService`).
- **RealSubject**: The actual business component containing core logic (`RealDocumentService`).
- **Proxy**: Contains a reference to `RealSubject`, implements `Subject`, and controls access.

### Static vs. Dynamic Proxies
- **Static Proxy**: Hand-coded Java class implementing the interface. Requires writing boilerplate for every method.
- **Dynamic Proxy (JDK `java.lang.reflect.Proxy`)**: Generates proxy bytecode dynamically at runtime for any interface using an `InvocationHandler`.
- **Class-Based Dynamic Proxies (ByteBuddy / CGLIB)**: Generates runtime subclasses for concrete classes that do not implement interfaces.

---

## 3. How It Works Internally

### JDK Dynamic Proxy Bytecode Generation
When you call `Proxy.newProxyInstance()`:

```
[ Client Code: service.fetchData(10) ]
                  |
                  v
[ Generated Class $Proxy0 (Implements DocumentService) ]
                  |
                  | Invokes: invocationHandler.invoke(this, method, args)
                  v
[ Custom InvocationHandler ]
  - Step 1: Pre-processing (Check JWT token / RBAC permission)
  - Step 2: method.invoke(realTarget, args) via reflection
  - Step 3: Post-processing (Record metric / commit transaction)
                  |
                  v
[ RealDocumentService.fetchData(10) ]
```

---

## 4. Syntax & Usage

### 1. JDK Dynamic Proxy with `InvocationHandler`
```java
import java.lang.reflect.InvocationHandler;
import java.lang.reflect.Method;
import java.lang.reflect.Proxy;

public class LoggingProxyFactory {
    @SuppressWarnings("unchecked")
    public static <T> T createProxy(T target, Class<T> interfaceType) {
        return (T) Proxy.newProxyInstance(
            interfaceType.getClassLoader(),
            new Class<?>[]{interfaceType},
            new InvocationHandler() {
                @Override
                public Object invoke(Object proxy, Method method, Object[] args) throws Throwable {
                    System.out.println("INTERCEPTED: " + method.getName());
                    return method.invoke(target, args);
                }
            }
        );
    }
}
```

---

## 5. Practical Examples

### Example 1: Virtual Proxy for Lazy-Loaded Heavy Image
**Goal:** Prevent loading 50 MB images from disk until `display()` is explicitly called.

```java
// Subject Interface
public interface HighResolutionImage {
    void display();
}

// Real Subject (Expensive)
public class RealImage implements HighResolutionImage {
    private final String filename;

    public RealImage(String filename) {
        this.filename = filename;
        loadFromDisk(); // Expensive 2-second disk I/O
    }

    private void loadFromDisk() {
        System.out.println("Loading 50MB image from disk: " + filename);
    }

    @Override
    public void display() {
        System.out.println("Displaying image: " + filename);
    }
}

// Virtual Proxy (Fast initialization)
public class LazyImageProxy implements HighResolutionImage {
    private final String filename;
    private RealImage realImage; // Instantiated only on demand

    public LazyImageProxy(String filename) {
        this.filename = filename;
    }

    @Override
    public void display() {
        if (realImage == null) {
            realImage = new RealImage(filename); // Lazy instantiation
        }
        realImage.display();
    }
}
```

### Example 2: Protection Proxy with Role-Based Access Control (RBAC)
**Goal:** Allow only users with role `"ADMIN"` to execute destructive database purges.

```java
public interface DocumentRepository {
    void readDocument(long id);
    void deleteDocument(long id);
}

public class RealDocumentRepository implements DocumentRepository {
    @Override public void readDocument(long id) { System.out.println("Reading doc: " + id); }
    @Override public void deleteDocument(long id) { System.out.println("DELETING doc: " + id); }
}

public class SecurityDocumentProxy implements DocumentRepository {
    private final DocumentRepository realRepo = new RealDocumentRepository();
    private final String currentUserRole;

    public SecurityDocumentProxy(String currentUserRole) {
        this.currentUserRole = currentUserRole;
    }

    @Override
    public void readDocument(long id) {
        realRepo.readDocument(id); // Allowed for everyone
    }

    @Override
    public void deleteDocument(long id) {
        if (!"ADMIN".equals(currentUserRole)) {
            throw new SecurityException("Access Denied: ADMIN role required to delete!");
        }
        realRepo.deleteDocument(id);
    }
}
```

---

### Example 3: Thread-Safe Dynamic Proxy with Rate Limiting
**Goal:** Intercept service calls and enforce a concurrency threshold using `Semaphore`.

```java
import java.lang.reflect.*;
import java.util.concurrent.Semaphore;

public class ConcurrencyThrottlingProxy {
    @SuppressWarnings("unchecked")
    public static <T> T throttle(T target, Class<T> intf, int maxConcurrentCalls) {
        Semaphore semaphore = new Semaphore(maxConcurrentCalls);

        return (T) Proxy.newProxyInstance(
            intf.getClassLoader(),
            new Class<?>[]{intf},
            (proxy, method, args) -> {
                semaphore.acquire();
                try {
                    return method.invoke(target, args);
                } catch (InvocationTargetException e) {
                    throw e.getCause();
                } finally {
                    semaphore.release();
                }
            }
        );
    }
}
```

## 6. Best Practices

1. **Prefer Interfaces for Dynamic Proxies**: JDK built-in dynamic proxies (`java.lang.reflect.Proxy`) require interfaces and need no third-party bytecode libraries.
2. **Handle Exceptions Cleanly in InvocationHandlers**: When invoking via reflection, unwrapping `InvocationTargetException.getCause()` is mandatory to avoid masking real domain exceptions.
3. **Use Proxies for Cross-Cutting Concerns**: Security, logging, caching, and transactions are prime candidates for proxies.
4. **Be Aware of the "Self-Invocation" Trap**: If `MethodA()` in `RealSubject` calls `this.MethodB()`, the call bypasses the proxy entirely!
5. **Keep Dynamic Proxy Handlers Thread-Safe**: Ensure `InvocationHandler` state is thread-safe if shared across worker threads.

---

## 7. Common Mistakes & Pitfalls

| Mistake | Why It Fails | Secure Fix |
| :--- | :--- | :--- |
| **The Self-Invocation Bug** | Method calls via `this.otherMethod()` bypass proxy interception (e.g. Spring `@Transactional` ignored). | Inject the proxy into itself or call through the interface. |
| **Masking Exceptions with `InvocationTargetException`** | Reflection wraps errors in `InvocationTargetException`; breaks caller catch blocks. | Catch and throw `e.getCause()`. |
| **Using JDK Proxy on concrete class** | `Proxy.newProxyInstance` throws `IllegalArgumentException` if passed a class instead of interface. | Use interfaces or ByteBuddy/CGLIB. |
| **Over-using proxies for simple logic** | Introduces reflection and stack trace confusion. | Use standard Decorator or direct calls when possible. |

---

## 8. Performance & Security Notes

### Reflection Overhead in Modern HotSpot
Historical Java reflection was slow. In modern Java (17+):
- HotSpot generates direct bytecode stubs (MethodAccessor) after 15 invocations (Inflation threshold).
- Dynamic proxy method calls incur $< 5\text{ nanoseconds}$ overhead, negligible in enterprise REST/DB flows.

---

## 9. Comparison / When to Use What

| Pattern | Intent | Who creates target? |
| :--- | :--- | :--- |
| **Proxy** | Access control / lazy lifecycle | Proxy controls target lifecycle |
| **Decorator** | Dynamically adds behavior | Client creates target and wraps it |
| **Adapter** | Converts incompatible interfaces | Client wraps existing adaptee |
| **Facade** | Simplifies complex subsystems | Facade coordinates multiple subsystem classes |

---

## 10. Interview Questions & Answers

### Q1: What is the "Self-Invocation Problem" with Spring and Java Dynamic Proxies?
<details>
<summary>View Answer</summary>
When a method in a proxied bean calls another method within the same class using `this.internalMethod()`, the call executes directly on the target instance bypassing the proxy. As a result, cross-cutting proxy concerns (such as Spring\'s `@Transactional`, `@Async`, or `@Cacheable`) on `internalMethod()` are completely skipped. The solution is to move `internalMethod()` to a separate service bean or use AspectJ compile-time weaving.
</details>

### Q2: What is the limitation of JDK Dynamic Proxies compared to ByteBuddy / CGLIB?
<details>
<summary>View Answer</summary>
JDK Dynamic Proxies (`java.lang.reflect.Proxy`) can only proxy classes that implement at least one Java **interface**. If a class does not implement an interface, JDK proxies cannot be used. ByteBuddy and CGLIB generate runtime subclasses using bytecode manipulation, allowing them to proxy concrete classes.
</details>

### Q3: How does a Virtual Proxy differ from standard lazy initialization?
<details>
<summary>View Answer</summary>
Standard lazy initialization requires modifying the class itself or scattering `if (instance == null)` checks throughout calling code. A Virtual Proxy encapsulates the lazy initialization completely behind the target interface. The calling code has zero awareness that the underlying object is uninitialized, preserving the Single Responsibility Principle.
</details>

### Q4: Why must `InvocationTargetException` be unwrapped in dynamic proxy handlers?
<details>
<summary>View Answer</summary>
When a method invoked via `Method.invoke()` throws an exception, Java reflection wraps the actual exception inside an `InvocationTargetException`. If the proxy re-throws this wrapper directly, callers expecting business exceptions (e.g. `OrderNotFoundException`) will fail to catch it, causing unexpected runtime crashes. The handler must inspect `e.getCause()` and re-throw the underlying cause.
</details>

### Q5: What is a Remote Proxy?
<details>
<summary>View Answer</summary>
A Remote Proxy provides a local representative for an object in a different address space (such as a remote server). Method calls on the local proxy are marshaled into network packets (via gRPC, RMI, or HTTP), transmitted over the wire, executed on the remote server, and the unmarshaled result is returned to the local caller.
</details>

### Q6: Can a Java Record be used as a Proxy?
<details>
<summary>View Answer</summary>
Yes. A record can implement an interface and hold the real subject as an immutable component, acting as a lightweight static protection or logging proxy.
</details>

### Q7: What is the difference between a Protection Proxy and a Firewall?
<details>
<summary>View Answer</summary>
A firewall operates at the network packet and IP layer. A Protection Proxy operates at the object method and application layer, evaluating user authentication tokens and role permissions (RBAC) before dispatching specific Java method calls.
</details>

### Q8: What role do Proxies play in JPA / Hibernate lazy loading?
<details>
<summary>View Answer</summary>
When an entity relationship is configured with `FetchType.LAZY`, Hibernate injects a ByteBuddy/CGLIB proxy object instead of loading the child entity from the database. When the application calls a getter on the child (e.g. `order.getCustomer().getName()`), the proxy intercepts the call, executes a SQL `SELECT` query against the database, populates the real entity, and returns the data.
</details>

---

## 11. Practice Exercises

### Exercise 1: Build a Method Execution Timer Dynamic Proxy
**Task:** Create a dynamic proxy utility that logs execution time for every method on an interface.
<details>
<summary>View Solution</summary>

```java
import java.lang.reflect.*;

public class TimingProxy {
    @SuppressWarnings("unchecked")
    public static <T> T attach(T target, Class<T> intf) {
        return (T) Proxy.newProxyInstance(
            intf.getClassLoader(),
            new Class<?>[]{intf},
            (proxy, method, args) -> {
                long start = System.nanoTime();
                try {
                    return method.invoke(target, args);
                } catch (InvocationTargetException e) {
                    throw e.getCause(); // Unwrap real exception!
                } finally {
                    long durationMs = (System.nanoTime() - start) / 1_000_000;
                    System.out.printf("[TIMER] %s took %d ms%n", method.getName(), durationMs);
                }
            }
        );
    }
}
```
</details>

### Exercise 2: Static Protection Proxy
**Task:** Build a `SecurityProxy` that throws `SecurityException` if a non-admin calls `purge()`.
<details>
<summary>View Solution</summary>

```java
public interface AdminService { void purge(); }
public class RealAdminService implements AdminService { @Override public void purge() { System.out.println("Purged"); } }

public class SecureAdminProxy implements AdminService {
    private final AdminService delegate = new RealAdminService();
    private final boolean isAdmin;
    public SecureAdminProxy(boolean isAdmin) { this.isAdmin = isAdmin; }

    @Override
    public void purge() {
        if (!isAdmin) throw new SecurityException("Forbidden");
        delegate.purge();
    }
}
```
</details>

### Exercise 3: Caching Proxy with Map
**Task:** Implement a caching proxy for a `WeatherService` returning temperatures.
<details>
<summary>View Solution</summary>

```java
import java.util.Map;
import java.util.concurrent.ConcurrentHashMap;

public interface WeatherService { double getTemp(String city); }

public class CachingWeatherProxy implements WeatherService {
    private final WeatherService realService;
    private final Map<String, Double> cache = new ConcurrentHashMap<>();

    public CachingWeatherProxy(WeatherService real) { this.realService = real; }

    @Override
    public double getTemp(String city) {
        return cache.computeIfAbsent(city, realService::getTemp);
    }
}
```
</details>

---

## 12. Summary / Cheat Sheet

- **Core Goal**: Provide a placeholder to control access, lifecycle, or execution of an object.
- **Identical Interface**: Proxy implements the exact same interface as the real subject.
- **Dynamic Proxies**: Use `Proxy.newProxyInstance(loader, interfaces, handler)`.
- **Exception Unwrapping**: Always catch `InvocationTargetException` and re-throw `e.getCause()`.
- **Self-Invocation Trap**: Internal method calls via `this` bypass proxy interception.
- **Enterprise Use**: Backbone of Spring `@Transactional`, `@Security`, and Hibernate lazy loading.

---

## 13. Further Reading
- GoF Design Patterns: [Proxy Pattern](https://www.oreilly.com/library/view/design-patterns-elements/0201633612/)
- Java Reflection Documentation: [Dynamic Proxy Classes](https://docs.oracle.com/en/java/javase/21/docs/api/java.base/java/lang/reflect/Proxy.html)
- Spring Framework Documentation: [Understanding AOP Proxies](https://docs.spring.io/spring-framework/reference/core/aop/proxying.html)
