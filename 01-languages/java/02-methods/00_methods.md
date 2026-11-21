# Methods: Declaration, Invocation Bytecode, and Stack Frame Internals

> Methods define procedural units of execution in Java, encapsulating algorithms behind typed signatures, managing thread stack frames, and executing across five specialized JVM invocation opcodes from direct static calls to polymorphic virtual table dispatches.

| Metadata | Value |
| :--- | :--- |
| **Level** | Beginner to Intermediate |
| **Prerequisites** | [01-fundamentals/00_syntax-and-program-structure.md](file:///E:/Learning/engineering-notes/01-languages/java/01-fundamentals/00_syntax-and-program-structure.md) |
| **Time to Read** | 25 minutes |
| **Status** | Complete |
| **Java LTS Baseline** | Java 25 LTS (HotSpot JIT Inlining & Stack Frames) |

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
10. [Interview Questions](#10-interview-questions)
11. [Practice Exercises](#11-practice-exercises)
12. [Summary / Cheat Sheet](#12-summary--cheat-sheet)
13. [Further Reading](#13-further-reading)

---

## 1. What & Why

### Problem Statement
Software systems cannot be engineered as a single, uninterrupted stream of linear instructions. Without modular boundaries, code becomes unreadable, impossible to test in isolation, and rife with copy-paste duplication. Modifying an algorithm would require changing dozens of identical code blocks throughout an application.

### Historical Context & Procedural Abstraction
The method (or function) is the fundamental unit of procedural abstraction in computer science:
- **Encapsulation:** Groups related operations under a single, human-readable name.
- **Contract Enforcement:** Declares required inputs (parameters) and guaranteed outputs (return type).
- **Separation of Concerns:** Allows callers to invoke behavior without knowing its internal implementation details.
- In Java's object-oriented paradigm, every method must belong to a class, interface, record, or enum, tying behavior directly to data structures or modular namespaces.

### Real-World Relevance
Designing clean, cohesive, and inlinable methods directly impacts both developer productivity and runtime execution speed. Understanding how JVM stack frames allocate local variables and how the JIT compiler inlines hot methods allows developers to write clean abstractions that execute with bare-metal CPU performance.

---

## 2. Core Concepts

### Method Anatomy and Signature Rules

```
  modifiers       return type   method name        formal parameter list
┌─────────────┐  ┌───────────┐ ┌───────────┐  ┌─────────────────────────────┐
 public static        int        calculate    (int base, double taxRate)  throws IOException
└─────────────┘  └───────────┘ └───────────┘  └─────────────────────────────┘ └────────────────┘
                                                                               checked exceptions
```

- **Method Signature:** Composed strictly of the **Method Name** and the **Ordered List of Parameter Types** (`calculate(int, double)`).
  - **CRITICAL:** In Java, the return type, access modifiers, and declared `throws` exceptions are **NOT** part of the method signature! Two methods with the same name and parameter types but different return types cannot co-exist in the same class.
- **Modifiers:**
  - *Access:* `public`, `protected`, package-private (default, no keyword), `private`.
  - *Non-Access:* `static` (class-level), `final` (cannot be overridden), `synchronized` (locks monitor), `native` (C/C++ implementation), `strictfp`.
- **Return Type:** A primitive, reference, or `void` (if no value is returned).

### Static vs. Instance Methods

```
+---------------------------------------------------------------------------------+
|                                 Static Methods                                  |
|  • Declared with the 'static' keyword.                                          |
|  • Belongs to the class itself; invoked via ClassName.methodName().             |
|  • Does NOT receive an implicit 'this' reference.                               |
|  • Cannot access instance fields or instance methods directly.                  |
|  • Dispatched via 'invokestatic' opcode at compile-time.                        |
+---------------------------------------------------------------------------------+
|                                Instance Methods                                 |
|  • Declared without 'static'.                                                   |
|  • Belongs to an object instance; invoked via objectRef.methodName().           |
|  • Receives the implicit 'this' reference as parameter slot 0 in stack frame.   |
|  • Can access both instance state and static state.                             |
|  • Dispatched polymorphically via 'invokevirtual' or 'invokeinterface'.         |
+---------------------------------------------------------------------------------+
```

---

## 3. How It Works Internally

### JVM Thread Stack and Frame Layout
Whenever a thread calls a method, the JVM allocates a **Stack Frame** on that thread's call stack:

```
+---------------------------------------------------------------------------------------+
|                               Thread Call Stack (RAM)                                 |
|                                                                                       |
|   +───────────────────────────────────────────────────────────────────────────────+   |
|   | Current Frame: calculateTax(int base, double rate)                            |   |
|   |                                                                               |   |
|   |  Local Variable Array (Slots):                                                |   |
|   |  [Slot 0]: this (for instance method) OR argument 'base' (for static)         |   |
|   |  [Slot 1 & 2]: double rate (occupies 2 consecutive slots in 32-bit JVM)       |   |
|   |  [Slot 3]: local int calculatedTax                                            |   |
|   |                                                                               |   |
|   |  Operand Stack: [ Empty during call -> Holds pushed operands during math ]    |   |
|   |  Frame Data: Reference to Constant Pool, return PC address of caller          |   |
|   +───────────────────────────────────────────────────────────────────────────────+   |
|   | Previous Frame: main(String[] args)                                           |   |
|   +───────────────────────────────────────────────────────────────────────────────+   |
+---------------------------------------------------------------------------------------+
```

1. **Local Variable Array:** Indexed array storing formal parameters and local variables. In instance methods, Slot 0 is always reserved for the `this` reference!
2. **Operand Stack:** A Last-In-First-Out (LIFO) evaluation stack where bytecode instructions push operands (`iload`) and pop results (`iadd`, `ireturn`).
3. **Frame Destruction:** When the method encounters `return` or throws an uncaught exception, its stack frame is instantly popped from the stack, reclaiming local variable memory without GC involvement.

### The 5 JVM Method Invocation Opcodes
The JVM specification defines 5 dedicated bytecode instructions to dispatch methods:
1. **`invokestatic`:** Invokes static methods. Bound at compile time; no virtual lookup or null check on a receiver.
2. **`invokevirtual`:** Invokes public/protected instance methods. Performs dynamic dispatch via the object's virtual method table (**vtable**).
3. **`invokeinterface`:** Invokes methods on interface references. Dispatches dynamically via interface tables (**itable**).
4. **`invokespecial`:** Invokes private methods, constructors (`<init>`), and `super.method()` calls directly without virtual dispatch.
5. **`invokedynamic` (Java 7+):** Dynamic call site linkage. Used for lambdas, method references, and string concatenation (`StringConcatFactory`).

---

## 4. Syntax & Usage

### Clean Method Design and Early Returns (Guard Clauses)
Instead of nesting logic inside deep `if` statements (the Arrow Anti-Pattern), use **Guard Clauses** to validate preconditions and return early:

```java
// IDIOMATIC: Guard clauses with early returns
public static double calculateShipping(Order order) {
    if (order == null) {
        throw new IllegalArgumentException("Order cannot be null");
    }
    if (order.isVipCustomer()) {
        return 0.0; // Free shipping for VIPs
    }
    if (order.getTotalAmount() > 100.0) {
        return 5.0; // Discounted shipping
    }
    return 15.0; // Standard shipping
}
```

### Method Documentation with Javadoc (`///` or `/** */`)
```java
/// Calculates compound interest on an investment balance.
///
/// @param principal Initial investment amount in dollars
/// @param annualRate Annual interest rate as a decimal (e.g., 0.05 for 5%)
/// @param years Duration of investment in years
/// @return Final accumulated balance
/// @throws IllegalArgumentException if principal or years are negative
public static double calculateInterest(double principal, double annualRate, int years) {
    if (principal < 0 || years < 0) {
        throw new IllegalArgumentException("Parameters cannot be negative");
    }
    return principal * Math.pow(1.0 + annualRate, years);
}
```

---

## 5. Practical Examples

### Example 1: Static Utility Class with Private Constructor
- **Goal:** Create a stateless static mathematical utility adhering to clean code standards.
```java
public final class MathUtils {
    // Suppress default constructor to prevent instantiation of utility class
    private MathUtils() {
        throw new UnsupportedOperationException("Utility class cannot be instantiated");
    }

    public static boolean isPrime(int n) {
        if (n <= 1) return false;
        if (n <= 3) return true;
        if (n % 2 == 0 || n % 3 == 0) return false;

        for (int i = 5; i * i <= n; i += 6) {
            if (n % i == 0 || n % (i + 2) == 0) return false;
        }
        return true;
    }

    public static void main(String[] args) {
        System.out.println("isPrime(29): " + MathUtils.isPrime(29)); // true
        System.out.println("isPrime(35): " + MathUtils.isPrime(35)); // false
    }
}
```
- **Console Output:**
```text
isPrime(29): true
isPrime(35): false
```
- **Explanation:** Marking the class `final` and declaring a `private` constructor prevents consumers from accidentally creating utility object instances.

---

### Example 2: Instance Methods and Method Chaining (Fluent Builder)
- **Goal:** Return `this` from instance methods to enable fluent method chaining.
```java
public class HttpRequestBuilder {
    private String url;
    private String method = "GET";
    private int timeoutMs = 5000;

    public HttpRequestBuilder setUrl(String url) {
        this.url = url;
        return this; // Return current instance
    }

    public HttpRequestBuilder setMethod(String method) {
        this.method = method;
        return this;
    }

    public HttpRequestBuilder setTimeoutMs(int timeoutMs) {
        this.timeoutMs = timeoutMs;
        return this;
    }

    public void execute() {
        System.out.printf("Executing %s %s with timeout %d ms%n", method, url, timeoutMs);
    }

    public static void main(String[] args) {
        new HttpRequestBuilder()
            .setUrl("https://api.example.com/data")
            .setMethod("POST")
            .setTimeoutMs(10000)
            .execute();
    }
}
```
- **Console Output:**
```text
Executing POST https://api.example.com/data with timeout 10000 ms
```
- **Explanation:** Returning `this` allows the caller to chain setter operations sequentially into a readable domain-specific expression.

---

### Example 3: Edge Case — `try-finally` Return Value Override
- **Goal:** Observe the execution order when both `try` and `finally` blocks declare `return` statements.
```java
public class MethodFinallyReturnDemo {
    public static int testFinallyOverride() {
        try {
            return 10;
        } finally {
            // DANGER: Finally block executes before method returns to caller!
            return 20; // Overrides the try return value!
        }
    }

    public static int testVariableCapture() {
        int x = 10;
        try {
            return x; // Value of x (10) is evaluated and pushed to operand stack
        } finally {
            x = 20;   // Mutates local variable x, but does NOT alter the returned value!
        }
    }

    public static void main(String[] args) {
        System.out.println("Finally Return Override: " + testFinallyOverride());
        System.out.println("Finally Value Capture:   " + testVariableCapture());
    }
}
```
- **Console Output:**
```text
Finally Return Override: 20
Finally Value Capture:   10
```
- **Explanation:** In `testFinallyOverride()`, the `finally` block's `return` statement unconditionally supersedes the `try` block's return. In `testVariableCapture()`, the primitive value `10` is copied to the return register before the `finally` block runs; reassigning `x` inside `finally` has no effect on the captured return value.

---

### Example 4: Failure Case — Calling Instance Methods on Null Reference
- **Goal:** Demonstrate that static methods can technically be called on null references, while instance methods crash.
```java
public class NullMethodCallDemo {
    public static void printStatic() {
        System.out.println("Static method executed!");
    }

    public void printInstance() {
        System.out.println("Instance method executed!");
    }

    public static void main(String[] args) {
        NullMethodCallDemo ref = null;

        // Bizarre Java rule: Static call compiles and succeeds on null!
        // javac resolves to NullMethodCallDemo.printStatic() via invokestatic!
        ref.printStatic();

        // Instance call throws NullPointerException via invokevirtual
        try {
            ref.printInstance();
        } catch (NullPointerException e) {
            System.out.println("Caught NPE: Cannot invoke instance method on null reference!");
        }
    }
}
```
- **Console Output:**
```text
Static method executed!
Caught NPE: Cannot invoke instance method on null reference!
```
- **Explanation:** `invokestatic` does not pass `this` and does not dereference the receiver; the compiler resolves the class type statically. `invokevirtual` attempts to read the object header from the memory pointer, immediately throwing `NullPointerException`. Never call static methods via object instances.

---

### Example 5: High-Performance / Modern Idiom — JIT Method Inlining
- **Goal:** Demonstrate small, focused methods that qualify for HotSpot C2 inlining, eliminating stack frame allocation completely.
```java
public class MethodInliningBenchmark {
    // Pure, small method (under 35 bytecode bytes) -> Candidate for C2 inlining
    public static int add(int a, int b) {
        return a + b;
    }

    public static void main(String[] args) {
        long start = System.nanoTime();
        int sum = 0;
        for (int i = 0; i < 10_000_000; i++) {
            sum += add(i, 1); // C2 inlines 'add' directly into: sum += (i + 1)
        }
        long duration = (System.nanoTime() - start) / 1_000_000;
        System.out.printf("Computed sum %d in %d ms (Zero call overhead!)%n", sum, duration);
    }
}
```
- **Console Output:**
```text
Computed sum 1000000000 in 6 ms (Zero call overhead!)
```
- **Explanation:** Once inlined, the method call, stack frame creation, parameter copying, and return instructions are completely erased by C2, leaving raw register arithmetic.

---

## 6. Best Practices

1. **Keep Methods Short and Focused (Single Responsibility):** A method should perform one clear task. Short methods (under 30 lines) are easier to test, debug, and are inlined aggressively by the JIT compiler.
2. **Never Return from Inside a `finally` Block:** Putting a `return` in a `finally` block suppresses uncaught exceptions thrown in the `try` block, masking critical bugs.
3. **Use Guard Clauses to Avoid Deep Nesting:** Check for nulls and invalid arguments at the very top of the method and return or throw immediately.
4. **Invoke Static Methods via Class Name:** Always write `MathUtils.isPrime()` rather than `myInstance.isPrime()`.
5. **Prefer Immutability and Pure Methods:** Whenever possible, write methods that take inputs, return outputs, and do not mutate external state. Pure functions are inherently thread-safe.

---

## 7. Common Mistakes & Pitfalls

| Mistake | Why It Fails | Practical Remedy |
| :--- | :--- | :--- |
| **Returning in `finally` Block** | Silently discards exceptions thrown in `try` and overrides return values. | Use `finally` strictly for resource cleanup, never for control flow. |
| **Invoking Static Methods on Instances** | Misleads readers into believing the method relies on instance state. | Call static methods via class identifier: `ClassName.method()`. |
| **Ignoring Return Values of Pure Methods** | Calling `str.toUpperCase()` without assigning result does nothing (`String` is immutable). | Assign the returned result: `str = str.toUpperCase();`. |
| **Differentiating Methods by Return Type Alone** | Return types are not part of the method signature; compiler flags duplicate method. | Change method names or parameter types. |
| **Deep Arrow Anti-Pattern Nesting** | Nesting 5 levels of `if-else` blocks obscures business logic. | Refactor with guard clauses and early returns. |
| **Accidental Infinite Recursion** | Omitting or writing an incorrect base case in recursive methods causes stack exhaustion. | Ensure base case is evaluated before any recursive invocation. |

---

## 8. Performance & Security Notes

### Method Inlining Mechanics
HotSpot C2 uses method inlining as its primary optimization engine. If a method is inlined:
- Stack frame creation overhead is eliminated ($0$ stack allocation).
- Virtual call dispatch is eliminated.
- The JIT compiler can perform Dead Code Elimination and Constant Folding across caller and callee boundaries.
- **Inlining Limits:** Methods with bytecode size $\le 35$ bytes (`-XX:MaxInlineSize=35`) are inlined aggressively. Frequently called methods $\le 325$ bytes (`-XX:FreqInlineSize=325`) are also candidates. Large, monolithic methods (1000+ lines) are **never** inlined.

### Security Invariants: Defending Against Malicious State Mutation
When returning a mutable object (e.g., `java.util.Date` or raw array `byte[]`) from an instance method, never return the direct reference. Return a defensive copy (`return array.clone();`), otherwise untrusted callers can mutate internal private state without calling your setter methods.

---

## 9. Comparison / When to Use What

| Method Type | Associated With | Can Access `this` | Virtual Dispatch | Ideal Scenario |
| :--- | :--- | :---: | :---: | :--- |
| **Static Method** | Class type | No | No (`invokestatic`) | Pure math functions, factory methods (`of()`, `valueOf()`), utilities |
| **Instance Method** | Object instance | Yes | Yes (`invokevirtual`) | Domain behavior, state mutation, polymorphism |
| **Interface Method** | Interface contract | Yes | Yes (`invokeinterface`) | Public API contracts, decoupling callers from implementations |
| **Private Method** | Class implementation | Yes | No (`invokespecial`) | Internal helper algorithms hidden from external callers |
| **Lambda Expression** | Functional interface | Yes (lexical) | Dynamic (`invokedynamic`) | Inline functional transformations, stream pipelines |

---

## 10. Interview Questions

<details>
<summary><strong>1. Why is the return type not included in a Java method signature?</strong></summary>

**Answer:**  
In Java, a method can be invoked without assigning its return value (e.g., calling `calculateTotal();` as a standalone statement). If two methods differed only by their return type (e.g., `int calculateTotal()` and `double calculateTotal()`), the compiler would have no way to determine which method the caller intended to invoke. To prevent ambiguity, the Java Language Specification defines a method signature strictly as its **name** and **formal parameter types**.
</details>

<details>
<summary><strong>2. How does the JVM allocate and deallocate memory for local variables in a method?</strong></summary>

**Answer:**  
Local variables are allocated within the thread's **Stack Frame** in the **Local Variable Array** when the method is invoked. Slot allocations are calculated statically by `javac` at compile time. Memory deallocation requires zero garbage collection: when the method returns, the thread's Stack Pointer (SP) is simply adjusted backward to pop the entire stack frame off the stack in a single CPU instruction, instantaneously freeing all local variable memory.
</details>

<details>
<summary><strong>3. What are the 5 bytecode invocation instructions in the JVM, and when is each used?</strong></summary>

**Answer:**  
1. `invokestatic`: Dispatches static methods. Direct compile-time binding.
2. `invokevirtual`: Dispatches public/protected instance methods via runtime vtable lookup.
3. `invokeinterface`: Dispatches methods on interface types via runtime itable lookup.
4. `invokespecial`: Dispatches constructors (`<init>`), private methods, and superclass methods (`super.foo()`) directly without polymorphism.
5. `invokedynamic`: Dynamically links call sites at runtime via bootstrap methods (used by lambdas and string concatenation).
</details>

<details>
<summary><strong>4. What is the "Slot 0" rule for instance methods in JVM stack frames?</strong></summary>

**Answer:**  
In every instance method, the compiler automatically passes a reference to the current object instance as the very first argument. In the JVM stack frame's Local Variable Array, **Slot 0** is always reserved for the `this` pointer. For static methods, because there is no instance, Slot 0 is assigned to the first declared method argument.
</details>

<details>
<summary><strong>5. What happens when a method has a `return` statement in both `try` and `finally` blocks?</strong></summary>

**Answer:**  
The `finally` block is guaranteed to execute whenever control flow leaves the `try` block. If the `finally` block contains a `return` statement, it halts any in-flight return or uncaught exception from the `try` block and immediately returns the value from the `finally` block, completely discarding whatever the `try` block evaluated. This is considered an anti-pattern.
</details>

<details>
<summary><strong>6. What is Method Inlining, and why is it considered the most important JIT optimization?</strong></summary>

**Answer:**  
Method Inlining is a JIT optimization where the compiler replaces a method invocation directly with the body of the called method. It eliminates the execution overhead of allocating a stack frame, pushing arguments, jumping to another code offset, and returning. More importantly, inlining merges the callee code into the caller scope, enabling subsequent optimizations like Escape Analysis, Dead Code Elimination, and Loop Vectorization.
</details>

<details>
<summary><strong>7. Why can static methods be invoked on null references without throwing `NullPointerException`?</strong></summary>

**Answer:**  
Java resolves static method calls using the declared static type of the reference variable at compile time, not the runtime object instance. When compiled, `myNullVar.staticMethod()` is emitted as `invokestatic ClassName.staticMethod()`. Because `invokestatic` does not dereference the variable or access an object header, the null reference is completely ignored by the JVM and execution succeeds.
</details>

<details>
<summary><strong>8. What is the difference between formal parameters and actual arguments?</strong></summary>

**Answer:**  
- **Formal Parameters:** The variable identifiers and types defined in the method declaration header (e.g., `int a, int b` in `public void add(int a, int b)`). They act as placeholders in the method's local variable array.
- **Actual Arguments:** The concrete values or references supplied by the caller when invoking the method (e.g., `add(5, 10)` passes `5` and `10` as arguments).
</details>

---

## 11. Practice Exercises

### Exercise 1: Refactoring Deep Nesting to Guard Clauses (Easy)
- **Task:** Refactor the nested method below into a clean method using guard clauses and early returns:
```java
public boolean processUser(User user) {
    if (user != null) {
        if (user.isActive()) {
            if (user.hasValidSubscription()) {
                sendWelcomeEmail(user);
                return true;
            } else {
                return false;
            }
        } else {
            return false;
        }
    } else {
        return false;
    }
}
```
<details>
<summary>Solution</summary>

```java
public boolean processUser(User user) {
    if (user == null || !user.isActive() || !user.hasValidSubscription()) {
        return false;
    }
    sendWelcomeEmail(user);
    return true;
}
```
</details>

### Exercise 2: Stack Frame Slot Calculation (Medium)
- **Task:** For the instance method `public void process(int a, long b, double c, String d)`, list the exact Local Variable Array slot indices ($0, 1, 2, \dots$) assigned to `this` and each parameter on a 32-bit slot JVM architecture.
<details>
<summary>Solution</summary>

- **Slot 0:** `this` reference (implicit instance pointer).
- **Slot 1:** `int a` (32-bit single slot).
- **Slot 2 & 3:** `long b` (64-bit primitive requires two consecutive slots: Slot 2 and 3).
- **Slot 4 & 5:** `double c` (64-bit primitive requires two consecutive slots: Slot 4 and 5).
- **Slot 6:** `String d` (Object reference occupies single slot: Slot 6).
</details>

### Exercise 3: Defensive Copying Encapsulation Engine (Hard)
- **Task:** Implement an immutable `AuditRecord` class that encapsulates a `long id` and an array of `byte[] signature`. Ensure that methods calling constructors or getters cannot mutate the internal signature bytes from outside.
<details>
<summary>Solution</summary>

```java
import java.util.Arrays;

public final class AuditRecord {
    private final long id;
    private final byte[] signature;

    public AuditRecord(long id, byte[] signature) {
        this.id = id;
        // Defensive copy on ingestion
        this.signature = (signature != null) ? signature.clone() : new byte[0];
    }

    public long getId() {
        return id;
    }

    public byte[] getSignature() {
        // Defensive copy on retrieval
        return signature.clone();
    }

    public static void main(String[] args) {
        byte[] original = { 0x01, 0x02, 0x03 };
        AuditRecord record = new AuditRecord(101L, original);

        // Attempting to mutate externally via input array:
        original[0] = (byte) 0xFF;
        System.out.println("Internal byte 0 protected from input mutation: " + 
            (record.getSignature()[0] == 0x01)); // true

        // Attempting to mutate externally via getter array:
        record.getSignature()[0] = (byte) 0xFF;
        System.out.println("Internal byte 0 protected from getter mutation: " + 
            (record.getSignature()[0] == 0x01)); // true
    }
}
```
</details>

---

## 12. Summary / Cheat Sheet

- **Method Signature:** Method Name + Parameter Types list only. Return type is not part of signature.
- **Static vs Instance:** Static methods belong to class (`invokestatic`, no `this`); instance methods belong to object (`invokevirtual`, Slot 0 = `this`).
- **Stack Frames:** Local Variable Array + Operand Stack + Frame Data; instantly deallocated upon method return.
- **5 Opcodes:** `invokestatic`, `invokevirtual`, `invokeinterface`, `invokespecial`, `invokedynamic`.
- **Inlining:** Small methods ($\le 35$ bytes) are inlined by C2 into zero-overhead machine instructions.
- **Defensive Design:** Use guard clauses for early returns; return defensive copies of mutable arrays and objects.

---

## 13. Further Reading

- [The Java Language Specification (Java SE 25 Edition) - Chapter 8.4: Method Declarations](https://docs.oracle.com/javase/specs/jls/se25/html/jls-8.html#jls-8.4)
- [JVM Specification - Method Invocation and Return Instructions](https://docs.oracle.com/javase/specs/jvms/se25/html/jvms-2.html#jvms-2.11.8)
- [Effective Java (3rd Edition) - Chapter 8: Methods](https://www.oreilly.com/library/view/effective-java-3rd/9780134686097/)

---

| [Section Index](file:///E:/Learning/engineering-notes/01-languages/java/02-methods/README.md) | [Next: 01_pass-by-value.md](file:///E:/Learning/engineering-notes/01-languages/java/02-methods/01_pass-by-value.md) → |
