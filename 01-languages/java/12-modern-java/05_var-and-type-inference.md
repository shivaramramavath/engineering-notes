# Local Variable Type Inference (`var`)

> Comprehensive guide to `var` in Java (JEP 286): local variable type inference, non-denotable and intersection types, lambda parameter syntax (JEP 323), readability guidelines, and common pitfalls.

| Metadata | Value |
|---|---|
| **Level** | Intermediate |
| **Prerequisites** | [01-fundamentals/03_variables-constants.md](../01-fundamentals/03_variables-constants.md), [07-generics/00_generic-classes-and-methods.md](../07-generics/00_generic-classes-and-methods.md) |
| **Time to Read** | 22 minutes |
| **Status** | Active |
| **Java Version** | Java 10 Final (JEP 286), Java 11 Lambda Parameters (JEP 323) |

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

### The Redundancy of Manifest Typing
Java is a statically typed language. Prior to Java 10, every variable declaration required explicit (manifest) typing, even when the type was completely obvious from the right-hand side:
```java
ByteArrayOutputStream outputStream = new ByteArrayOutputStream();
Map<String, List<Map<Integer, String>>> complexMap = new HashMap<String, List<Map<Integer, String>>>();
```
This repetitive ceremony introduced visual clutter, obscuring the meaningful part of the code (the variable name and its initialization expression).

### The Solution: Local-Variable Type Inference (JEP 286)
Java 10 introduced `var`. When declaring a local variable with an initializer, developers can write `var` instead of the explicit type name:
```java
var outputStream = new ByteArrayOutputStream();
var complexMap = new HashMap<String, List<Map<Integer, String>>>();
```
**Important Distinction**: `var` does **not** introduce dynamic typing (like JavaScript or Python). Java remains 100% statically typed. The compiler infers the exact static type from the initializer expression at compile time and writes that exact type into the class bytecode.

---

## 2. Core Concepts

### Where `var` CAN Be Used
`var` is strictly limited to local variable contexts where an initializer is present:
- Local variables with initializers inside methods, constructors, and initialization blocks.
- Enhanced `for`-loop variables (`for (var item : list)`).
- Traditional `for`-loop index variables (`for (var i = 0; i < n; i++)`).
- Try-with-resources resource declarations (`try (var stream = Files.newInputStream(path))`).
- Formal parameters of implicitly typed lambda expressions (Java 11 JEP 323).

### Where `var` CANNOT Be Used
To maintain class contract clarity and ABI stability, `var` is forbidden in:
- Field declarations (`class Person { var name = "Alice"; }` -> **ILLEGAL**).
- Method return types (`public var compute()` -> **ILLEGAL**).
- Method parameter types (`public void save(var item)` -> **ILLEGAL**).
- Local variables without initializers (`var x;` -> **ILLEGAL**).
- Null initializers (`var x = null;` -> **ILLEGAL** because null has no type).
- Array initializers without explicit type (`var arr = { 1, 2, 3 };` -> **ILLEGAL**).

### Non-Denotable and Intersection Types
`var` can capture **non-denotable types**—types that exist inside the compiler's type system but cannot be expressed in standard Java source code syntax:
- **Anonymous Class Types**: `var runnable = new Object() { void ping() { ... } }; runnable.ping();` (Legal with `var`, impossible with manifest types!).
- **Intersection Types**: Types resulting from generic bounds (`Comparable<T> & Serializable`).

---

## 3. How It Works Internally

### Compile-Time Static Type Resolution
`var` is not a keyword; it is a **reserved type name**. When `javac` encounters `var x = expr;`:
1. It computes the compile-time type of `expr`.
2. It assigns that exact type to `x`.
3. It writes the compiled bytecode identically to if the developer had typed the full manifest type.

There is **zero runtime overhead**, zero performance penalty, and no reflection involved. Decompiling class files compiled with `var` reveals the exact manifest type.

```mermaid
flowchart LR
    A[Source Code: var list = List.of(1, 2);] --> B[Javac Type Inference]
    B --> C[Resolved Type: java.util.List<java.lang.Integer>]
    C --> D[Bytecode: List list = List.of(...);]
    D --> E[JVM Runtime: Identical to explicit typing]
```

---

## 4. Syntax & Usage

### Basic Local Variable Inference
```java
import java.nio.file.*;
import java.util.*;

public class VarSyntaxDemo {
    public static void main(String[] args) throws Exception {
        var message = "Hello, Modern Java"; // Inferred: String
        var count = 42;                     // Inferred: int
        var ratio = 3.14159;                // Inferred: double
        var names = List.of("Ada", "Alan"); // Inferred: List<String>

        // Try-with-resources
        try (var lines = Files.lines(Path.of("test.txt"))) {
            lines.forEach(System.out::println);
        } catch (NoSuchFileException e) {
            // Handled
        }

        // Enhanced for-loop
        for (var name : names) {
            System.out.println("Name: " + name);
        }
    }
}
```

### Lambda Parameter Type Inference (Java 11 JEP 323)
```java
// Goal: Apply annotations to lambda parameters while preserving inference
import java.lang.annotation.*;
import java.util.function.BiFunction;

@Target(ElementType.PARAMETER)
@Retention(RetentionPolicy.RUNTIME)
@interface NonNull {}

public class LambdaVarDemo {
    public static void main(String[] args) {
        // Legal in Java 11: permits annotating inferred lambda arguments!
        BiFunction<String, String, String> concat = (@NonNull var a, @NonNull var b) -> a + b;
        System.out.println(concat.apply("Hello ", "World"));
    }
}
```

---

## 5. Practical Examples

### Example 1: Eliminating Generic Noise in Enterprise DTOs
```java
// Goal: Clean up verbose stream aggregation pipelines
import java.util.*;
import java.util.stream.Collectors;

public class CleanPipelines {
    public record Order(String customerId, double amount) {}

    public static void summarize(List<Order> orders) {
        // Manifest type: Map<String, DoubleSummaryStatistics>
        var customerStats = orders.stream()
            .collect(Collectors.groupingBy(
                Order::customerId,
                Collectors.summarizingDouble(Order::amount)
            ));

        for (var entry : customerStats.entrySet()) {
            System.out.println("Customer: " + entry.getKey() + " -> Total: " + entry.getValue().getSum());
        }
    }
}
```
*Explanation:* `var` shifts the visual focus from redundant type declarations to the business logic of grouping and summarizing.

### Example 2: Anonymous Class Specialization (Non-Denotable Type)
```java
// Goal: Use anonymous classes with custom methods without an explicit interface
public class AnonymousSpecialization {
    public static void main(String[] args) {
        // 'var' infers the exact anonymous subtype of Object!
        var helper = new Object() {
            int counter = 0;
            void increment() { counter++; }
            int getCount() { return counter; }
        };

        helper.increment();
        helper.increment();
        System.out.println("Counter value: " + helper.getCount());
    }
}
```
*Output:*
```
Counter value: 2
```
*Explanation:* If declared with `Object helper = new Object() {...}`, calling `helper.increment()` is a compile-time error. `var` preserves the anonymous type's members!

### Example 3: Edge Case: Diamond Operator with `var`
```java
// Goal: Illustrate diamond operator inference behavior
import java.util.*;

public class DiamondHazard {
    public static void test() {
        // HAZARD: Inferred as ArrayList<Object>, NOT ArrayList<String>!
        var list = new ArrayList<>();
        list.add("hello");
        // Object item = list.get(0); // Lost type precision!

        // CORRECT:
        var specificList = new ArrayList<String>();
        String item = specificList.get(0); // Type safe!
    }
}
```
*Explanation:* Never pair `var` with an empty diamond `<>` unless `Object` elements are intentionally desired.

### Example 4: Failure Case: Inferred Type Prevents Reassignment
```java
// Goal: Show static type invariance with var
public class ReassignmentFailure {
    public static void test() {
        var value = "Initial String"; // Inferred as String
        // COMPILE ERROR: incompatible types: int cannot be converted to String
        // value = 42;
    }
}
```
*Explanation:* `var` is statically typed. Once inferred as `String`, assigning an `int` fails compilation.

---

## 6. Best Practices
1. **Choose Good Variable Names**: When using `var`, the variable name carries the burden of communicating intent. Use descriptive names like `customerList` rather than single-letter names like `c`.
2. **Use `var` When Type is Explicit on the Right**: `var client = new HttpClient();` or `var p = Path.of("file.txt");` are ideal because the type is immediately obvious.
3. **Avoid `var` with Raw Generic Diamonds**: Always supply type parameters in constructors: `var list = new ArrayList<String>();`, never `var list = new ArrayList<>();`.
4. **Use `var` in Try-With-Resources**: Simplifies resource declarations where stream or reader types clutter the method header.
5. **Do Not Sacrific Readability for Brevity**: If the returned type from a method call is ambiguous (e.g. `var x = service.process();`), prefer an explicit manifest type.

---

## 7. Common Mistakes & Pitfalls

| Mistake | Why Wrong | Fix |
|---|---|---|
| `var list = new ArrayList<>();` | Infers `ArrayList<Object>`, losing compile-time generic type safety. | Specify generic argument: `new ArrayList<String>()`. |
| Attempting `var x = null;` | Compiler cannot infer a type from the untyped `null` literal. | Provide an explicit type: `String x = null;`. |
| Declaring `var` as field or method return | `var` is strictly confined to local variable scopes to protect public API contracts. | Use manifest types for fields and method signatures. |
| Expecting dynamic typing | `var` is statically typed at compile-time; reassignment to a different type fails. | Retain consistent types or use separate variables. |
| Using uninformative variable names with `var` | Code becomes completely unreadable: `var d = service.get();`. | Use meaningful names: `var customerDiscount = service.get();`. |
| Mixing `var` and array literals | `var arr = {1, 2, 3};` cannot resolve array component type. | Write `var arr = new int[]{1, 2, 3};`. |

---

## 8. Performance & Security Notes

### Zero Runtime Overhead
`var` is resolved entirely during the compilation phase by `javac`. The compiled `.class` files contain identical bytecode instructions (`aload`, `astore`, `invokevirtual`) as if full manifest types were written. There is zero runtime CPU, memory, or reflection penalty.

### ABI and Binary Compatibility Stability
Because `var` cannot be used in public class fields, method parameters, or method return types, public API boundaries and compiled JAR binary compatibility (ABI) are completely insulated from local type inference refactorings.

---

## 9. Comparison / When to Use What

| Scenario | Use `var`? | Reason |
|---|---|---|
| `new StringBuilder()` | **Yes** | Type is completely redundant |
| Factory method: `Path.of("a")` | **Yes** | Obvious return type from method name |
| Method with ambiguous return: `search()` | **No** | Reader cannot determine type without IDE |
| Try-with-resources streams | **Yes** | Eliminates visual clutter |
| `var list = new ArrayList<>()` | **No** | Infers `ArrayList<Object>` |
| Lambda parameters with annotations | **Yes** | Required by Java 11 JEP 323 |

---

## 10. Interview Questions

<details>
<summary>1. Is `var` dynamically typed in Java?</summary>
No. Java remains strictly, statically typed. `var` is local variable type inference performed entirely at compile-time by `javac`. Once resolved, the variable type is immutable and verified at compilation.
</details>

<details>
<summary>2. Why can't `var` be used for fields or method return types?</summary>
To protect API contracts and readability. Public class APIs, method signatures, and fields form binary contracts. Permitting `var` would make contracts brittle and obscure interfaces for external callers.
</details>

<details>
<summary>3. What is the result of `var list = new ArrayList<>();`?</summary>
The compiler infers `ArrayList<Object>`. Any generic type safety for stored elements is lost. To preserve type safety, specify the generic parameter: `new ArrayList<String>()`.
</details>

<details>
<summary>4. Why is `var x = null;` invalid?</summary>
Because `null` is a special literal that belongs to the null type and can be converted to any reference type. The compiler cannot infer which concrete type was intended.
</details>

<details>
<summary>5. What are non-denotable types and how does `var` support them?</summary>
Non-denotable types are internal types in the compiler's type system that have no valid syntax name in source code (such as anonymous class types or intersection types). `var` can capture and preserve these types, allowing access to custom methods on anonymous objects.
</details>

<details>
<summary>6. How was `var` enhanced in Java 11 (JEP 323)?</summary>
Java 11 allowed `var` to be used for formal parameters of implicitly typed lambda expressions, primarily enabling parameter annotations (like `@Nonnull var x`).
</details>

<details>
<summary>7. Is `var` a keyword in Java?</summary>
No, `var` is a reserved type name, not a keyword. Code that uses `var` as a method name or variable name (e.g. `int var = 5;`) remains legal and backward-compatible.
</details>

<details>
<summary>8. Does using `var` impact bytecode performance or compilation speed?</summary>
Bytecode performance is completely identical to manifest typing. Compilation speed impact is virtually immeasurable because `javac` already infers expression types during standard type checking.
</details>

---

## 11. Practice Exercises

### Exercise 1 (Easy): Refactoring Redundant Manifest Types
- **Task**: Refactor the following snippet to use `var` appropriately:
```java
Map<String, List<String>> userRoles = new HashMap<String, List<String>>();
for (Map.Entry<String, List<String>> entry : userRoles.entrySet()) {
    List<String> roles = entry.getValue();
}
```
<details>
<summary>Solution</summary>

```java
var userRoles = new HashMap<String, List<String>>();
for (var entry : userRoles.entrySet()) {
    var roles = entry.getValue();
}
```
</details>

### Exercise 2 (Medium): Annotated Lambda Parameter with `var`
- **Task**: Write a lambda expression taking two strings that uses Java 11 `var` parameter syntax to apply a `@Deprecated` annotation to the parameters.
<details>
<summary>Solution</summary>

```java
import java.util.function.BiConsumer;

public class LambdaVarExercise {
    public static void main(String[] args) {
        BiConsumer<String, String> printer = (@Deprecated var a, @Deprecated var b) -> {
            System.out.println(a + ": " + b);
        };
        printer.accept("Key", "Value");
    }
}
```
</details>

### Exercise 3 (Hard): Anonymous Struct Projection
- **Task**: Use `var` and an anonymous object to create an ad-hoc local data projection containing a `String id` and `double totalScore` from two unrelated objects, and print the values.
<details>
<summary>Solution</summary>

```java
public class AdHocStructExercise {
    public static void main(String[] args) {
        String studentId = "S1024";
        double exam1 = 88.5;
        double exam2 = 94.0;

        // Anonymous struct holding calculated projection
        var projection = new Object() {
            final String id = studentId;
            final double finalGrade = (exam1 + exam2) / 2.0;
        };

        System.out.println("Student: " + projection.id + ", Grade: " + projection.finalGrade);
    }
}
```
</details>

---

## 12. Summary / Cheat Sheet

```
+-----------------------------------------------------------------------------------------+
|                                  JAVA 'var' CHEAT SHEET                                 |
+-----------------------------------------------------------------------------------------+
| Scope Allowed            | Local variables with initializers, for loops, try-with-res   |
| Scope Forbidden          | Fields, method params, method returns, uninitialized vars    |
| Typing Mechanics         | 100% Statically typed at compile time                        |
| Reserved Type Name       | Not a keyword; existing identifiers named 'var' still compile|
| Generic Diamond Rule     | Use new ArrayList<String>(), NEVER new ArrayList<>()         |
| Null Literal             | var x = null; is a COMPILE ERROR                             |
| Java 11 Lambda           | (@Nonnull var x, var y) -> x.process(y);                     |
| Runtime Performance      | Identical bytecode to explicit manifest typing               |
+-----------------------------------------------------------------------------------------+
```

---

## 13. Further Reading
- [JEP 286: Local-Variable Type Inference (Java 10)](https://openjdk.org/jeps/286)
- [JEP 323: Local-Variable Syntax for Lambda Parameters (Java 11)](https://openjdk.org/jeps/323)
- Style Guidelines for Local Variable Type Inference in Java (by Stuart Marks)
- Java Language Specification — Section 14.4 (Local Variable Declarations)
