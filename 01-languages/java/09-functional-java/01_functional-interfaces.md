# Functional Interfaces and java.util.function

> The Single Abstract Method (SAM) contract, standard functional taxonomy, primitive specializations, and custom functional abstractions.

| Metadata | Value |
|---|---|
| **Level** | Intermediate |
| **Prerequisites** | [00_lambda-expressions.md](00_lambda-expressions.md), [04-oop/09_interfaces.md](../04-oop/09_interfaces.md) |
| **Time to Read** | 25 minutes |
| **Status** | Active |
| **Java Version** | Java 8+ |

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

In functional programming, functions are first-class citizens: they can be stored in variables, passed into arguments, and returned from other functions. In Java, because everything exists within the type system of classes and interfaces, functional behaviors are represented as **Functional Interfaces**.

A functional interface is an interface that declares **exactly one abstract method**. This Single Abstract Method (SAM) contract provides the structural blueprint that allows the Java compiler to assign a lambda expression or method reference to a strongly-typed reference.

To prevent developers from creating redundant, custom single-method interfaces across every project, Java 8 introduced the comprehensive `java.util.function` package containing 43 standardized functional interfaces covering predicates, functions, consumers, suppliers, operators, and their primitive specializations.

---

## 2. Core Concepts

### The `@FunctionalInterface` Annotation
An informative annotation used to declare that an interface is intended to be a functional interface:
```java
@FunctionalInterface
public interface Transformer<T, R> {
    R transform(T input);
}
```
If an interface annotated with `@FunctionalInterface` declares more than one abstract method, or zero abstract methods, the compiler emits a compile-time error. (Note: An interface with exactly one abstract method is functional even without this annotation, but omitting it is poor practice).

### Abstract Methods vs Default and Static Methods
A functional interface can contain:
- Exactly **one** abstract method.
- Any number of **default methods** (which provide concrete implementation).
- Any number of **static methods**.
- Any abstract method that overrides a public method of `java.lang.Object` (e.g., `boolean equals(Object o)` does **not** count toward the SAM limit!).

### The 4 Core Functional Archetypes
Every interface in `java.util.function` derives from four fundamental shapes:

| Category | Interface | Method Signature | Conceptual Role |
|---|---|---|---|
| **Predicate** | `Predicate<T>` | `boolean test(T t)` | Evaluates a boolean condition |
| **Function** | `Function<T, R>` | `R apply(T t)` | Transforms input of type T to output of type R |
| **Consumer** | `Consumer<T>` | `void accept(T t)` | Accepts input and produces side-effects |
| **Supplier** | `Supplier<T>` | `T get()` | Generates/supplies values with no input |

Specialized variants:
- **`UnaryOperator<T>`**: Extends `Function<T, T>` where argument and return types are identical.
- **`BinaryOperator<T>`**: Extends `BiFunction<T, T, T>` combining two arguments of type T.
- **`BiPredicate<T, U>`**, **`BiFunction<T, U, R>`**, **`BiConsumer<T, U>`**: Two-argument variants.

---

## 3. How It Works Internally

### Primitive Specializations: Eliminating Auto-Boxing
Generic type arguments only accept reference objects (`T` must be `Object`). If you use `Predicate<Integer>`, every evaluation boxes primitive `int` to `Integer` (16 bytes heap allocation) and unboxes it.
Under high-throughput loops, this destroys CPU cache lines and triggers continuous garbage collection.

To solve this, `java.util.function` provides 39 primitive specializations for `int`, `long`, and `double`:
- `IntPredicate`: `boolean test(int value)`
- `LongFunction<R>`: `R apply(long value)`
- `DoubleConsumer`: `void accept(double value)`
- `ToIntFunction<T>`: `int applyAsInt(T value)`
- `IntToLongFunction`: `long applyAsLong(int value)`

```
Predicate<Integer>:
Primitive int (4B) ---> Boxed Integer Object (16B on heap) ---> test() ---> unbox

IntPredicate:
Primitive int (4B) ---------------------------------------------> test(int) (0B heap!)
```

---

## 4. Syntax & Usage

### 1. Functional Composition
```java
import java.util.function.Function;
import java.util.function.Predicate;

public class CompositionDemo {
    public static void main(String[] args) {
        // Predicate composition
        Predicate<String> nonNull = s -> s != null;
        Predicate<String> notEmpty = s -> !s.isEmpty();
        Predicate<String> valid = nonNull.and(notEmpty);

        // Function composition (andThen vs compose)
        Function<Integer, Integer> multiplyBy2 = x -> x * 2;
        Function<Integer, Integer> add10 = x -> x + 10;

        // andThen: multiplyBy2 first, then add10: (5 * 2) + 10 = 20
        Function<Integer, Integer> pipeline1 = multiplyBy2.andThen(add10);
        System.out.println("andThen: " + pipeline1.apply(5)); // 20

        // compose: add10 first, then multiplyBy2: (5 + 10) * 2 = 30
        Function<Integer, Integer> pipeline2 = multiplyBy2.compose(add10);
        System.out.println("compose: " + pipeline2.apply(5)); // 30
    }
}
```

### 2. High-Performance Primitive Pipelines
```java
import java.util.function.IntPredicate;
import java.util.function.IntUnaryOperator;

public class PrimitivePipeline {
    public static void main(String[] args) {
        IntPredicate isEven = n -> (n & 1) == 0;
        IntUnaryOperator square = n -> n * n;

        int num = 8;
        if (isEven.test(num)) {
            System.out.println("Squared: " + square.applyAsInt(num)); // 64
        }
    }
}
```

---

## 5. Practical Examples

### Example 1: Dynamic Validation Rule Engine (Basic to Intermediate)
```java
import java.util.ArrayList;
import java.util.List;
import java.util.function.Predicate;

public class ValidationEngine<T> {
    private final List<Rule<T>> rules = new ArrayList<>();

    public record Rule<T>(Predicate<T> condition, String errorMessage) {}

    public ValidationEngine<T> addRule(Predicate<T> condition, String error) {
        rules.add(new Rule<>(condition, error));
        return this;
    }

    public List<String> validate(T target) {
        List<String> errors = new ArrayList<>();
        for (Rule<T> rule : rules) {
            if (!rule.condition().test(target)) {
                errors.add(rule.errorMessage());
            }
        }
        return errors;
    }

    public static void main(String[] args) {
        record User(String username, int age) {}

        ValidationEngine<User> engine = new ValidationEngine<User>()
            .addRule(u -> u.username() != null && u.username().length() >= 3, "Username must be >= 3 chars")
            .addRule(u -> u.age() >= 18, "User must be an adult (18+)");

        List<String> violations = engine.validate(new User("al", 16));
        System.out.println("Violations: " + violations);
    }
}
```

### Example 2: Lazy Execution via `Supplier` (Real-World)
```java
import java.util.function.Supplier;

public class LazyLogger {
    public enum Level { INFO, DEBUG }
    private Level currentLevel = Level.INFO;

    // Evaluates expensive string construction ONLY if DEBUG is enabled!
    public void debug(Supplier<String> messageSupplier) {
        if (currentLevel == Level.DEBUG) {
            System.out.println("[DEBUG] " + messageSupplier.get());
        }
    }

    public static void main(String[] args) {
        LazyLogger logger = new LazyLogger();

        // Expensive method is NEVER called because level is INFO!
        logger.debug(() -> computeExpensiveMetrics());
    }

    private static String computeExpensiveMetrics() {
        System.out.println("Computing expensive metrics...");
        return "Metrics: CPU=99%, Heap=4GB";
    }
}
```

### Example 3: Edge Case - Object Methods in Functional Interface
```java
@FunctionalInterface
public interface ComparatorComparator<T> {
    int compare(T a, T b); // The SINGLE abstract method!

    // Overriding public methods of java.lang.Object does NOT violate @FunctionalInterface!
    boolean equals(Object obj);
    String toString();
}
```

### Example 4: Failure Case - Adding a Second Abstract Method
```java
// COMPILE ERROR: Invalid '@FunctionalInterface' annotation;
// MultiAction is not a functional interface because it has 2 abstract methods!
/*
@FunctionalInterface
public interface MultiAction {
    void execute();
    void rollback();
}
*/
```

---

## 6. Best Practices

1. **Always Annotate with `@FunctionalInterface`**: Explicitly document design intent and let the compiler catch accidental abstract method additions.
2. **Favor Built-in Interfaces from `java.util.function`**: Do not create custom `MyPredicate<T>` or `StringMapper` interfaces; reusing standard interfaces fosters API interoperability.
3. **Use Primitive Specializations in Hot Paths**: Use `IntPredicate`, `LongConsumer`, `DoubleFunction` in loops and stream pipelines to avoid boxing allocations.
4. **Leverage Default Methods for Composition**: Combine predicates using `.and()`, `.or()`, `.negate()`, and functions using `.andThen()`, `.compose()`.

---

## 7. Common Mistakes & Pitfalls

| Mistake | Why Wrong | Fix |
|---|---|---|
| Using `Function<T, Boolean>` instead of `Predicate<T>` | Requires auto-boxing `boolean` to `Boolean`; fails to convey semantic predicate intent. | Use `Predicate<T>`. |
| Using `Consumer<Integer>` in tight numeric loops | Boxes millions of integers, causing major GC pauses. | Use `IntConsumer`. |
| Missing `@FunctionalInterface` annotation | Allows future developers to inadvertently add a second abstract method, breaking client lambdas. | Always annotate SAM interfaces. |
| Overloading methods with ambiguous functional interfaces | E.g. `process(Consumer<String>)` vs `process(Function<String, Void>)` creates compiler ambiguity. | Use distinct method names. |
| Invoking `Supplier.get()` eagerly | Defeats the entire purpose of lazy supplier evaluation. | Pass supplier to method; call `.get()` only inside conditional. |
| Forgetting side-effects in `Supplier` | Suppliers should be deterministic generators; unexpected side-effects cause bugs. | Keep suppliers side-effect free. |

---

## 8. Performance & Security Notes

### Performance
- **Primitive Throughput**: Benchmarks demonstrate that `IntPredicate` can execute up to **$8\times$ faster** than `Predicate<Integer>` due to zero object allocation and direct CPU register manipulation.
- **Inlining of Default Combinators**: Methods like `Predicate.and()` are trivial forwarding closures easily inlined by the JIT compiler.

### Security
- **Untrusted Supplier Invocation**: If a `Supplier` is passed from an untrusted source, ensure it does not execute unauthorized privileged actions (e.g., file system reads or network queries) without adequate permission checks.

---

## 9. Comparison / When to Use What

| Interface | Arguments | Return Type | Typical Use Case |
|---|---|---|---|
| `Predicate<T>` | `T` | `boolean` | Filtering, validation rules |
| `Function<T, R>` | `T` | `R` | Transformations, mapping, projections |
| `Consumer<T>` | `T` | `void` | Logging, publishing, side-effects |
| `Supplier<T>` | None | `T` | Factory methods, lazy evaluation, default values |
| `UnaryOperator<T>` | `T` | `T` | In-place value mutation, math updates |
| `BinaryOperator<T>` | `T, T` | `T` | Aggregations, reductions (`reduce`) |

---

## 10. Interview Questions

<details>
<summary>1. What constitutes a functional interface in Java?</summary>
An interface that contains exactly one abstract method. It may contain any number of default methods, static methods, and public methods overriding `java.lang.Object`.
</details>

<details>
<summary>2. Why does `boolean equals(Object o)` not count as an abstract method in `@FunctionalInterface`?</summary>
Because every implementation of the interface will automatically inherit an implementation of `equals(Object)` from `java.lang.Object`. Therefore, the interface does not require an implementing class to provide custom code for it.
</details>

<details>
<summary>3. What is the difference between `Function.andThen()` and `Function.compose()`?</summary>
`f.andThen(g)` applies `f` first and then applies `g` to the result: $g(f(x))$. `f.compose(g)` applies `g` first and then applies `f` to the result: $f(g(x))$.
</details>

<details>
<summary>4. Why do primitive functional interfaces exist in Java?</summary>
To eliminate auto-boxing overhead. Using `Predicate<Integer>` requires boxing primitive `int` to heap-allocated `Integer` objects. `IntPredicate` operates directly on primitive `int`, achieving zero heap allocation and superior performance.
</details>

<details>
<summary>5. What is the difference between `UnaryOperator<T>` and `Function<T, R>`?</summary>
`UnaryOperator<T>` is a specialized sub-interface of `Function<T, T>` where the input and output types are identical.
</details>

<details>
<summary>6. Can a functional interface extend another interface?</summary>
Yes, as long as the total number of non-Object abstract methods across the entire inheritance hierarchy remains exactly one.
</details>

<details>
<summary>7. How does `Supplier<T>` enable lazy evaluation in Java?</summary>
By wrapping expensive computations inside a lambda `() -> computeExpensive()`, execution is deferred until the caller explicitly invokes `.get()`. If the value is never needed, the computation never runs.
</details>

<details>
<summary>8. Is the `@FunctionalInterface` annotation required for an interface to be functional?</summary>
No. Any interface satisfying the Single Abstract Method contract is functional by definition. However, the annotation is strongly recommended because it forces compiler verification and documents design intent.
</details>

---

## 11. Practice Exercises

### Easy
1. Declare a custom `@FunctionalInterface` `TriFunction<A, B, C, R>` that accepts three arguments and returns a result.
2. Create a `Predicate<String>` that checks if a string is non-null and longer than 5 characters using `.and()`.
3. Use a `Supplier<Double>` to generate random numbers between 0.0 and 1.0.

### Medium
4. Implement a memoizing decorator `memoize(Function<T, R> fn)` that caches function results in a `Map`.
5. Build a pipeline of `UnaryOperator<String>` transformations (trim $\to$ lowerCase $\to$ prefix) using `andThen()`.
6. Write a conditional execution helper `executeIf(T val, Predicate<T> condition, Consumer<T> action)`.

### Hard
7. Implement an asynchronous retry utility `retryAsync(Supplier<CompletableFuture<T>> task, int retries, Duration delay)`.
8. Implement currying for a `BiFunction<A, B, R>`, converting it into `Function<A, Function<B, R>>`.
9. Design a compile-time typesafe expression AST evaluator using custom functional interfaces and visitors.

<details>
<summary>Solutions</summary>

```java
// Easy 1: TriFunction
@FunctionalInterface
public interface TriFunction<A, B, C, R> {
    R apply(A a, B b, C c);
}

// Medium 4: Memoize decorator
public static <T, R> java.util.function.Function<T, R> memoize(java.util.function.Function<T, R> fn) {
    java.util.Map<T, R> cache = new java.util.concurrent.ConcurrentHashMap<>();
    return input -> cache.computeIfAbsent(input, fn);
}

// Hard 8: Currying BiFunction
public static <A, B, R> java.util.function.Function<A, java.util.function.Function<B, R>> curry(
        java.util.function.BiFunction<A, B, R> biFn) {
    return a -> b -> biFn.apply(a, b);
}
```
</details>

---


### Deep-Dive: Bytecode Analysis of @FunctionalInterface
When compiling an interface with `@FunctionalInterface`, `javac` emits the interface definition with standard class flags:
```
public interface java.util.function.Predicate<T>
  flags: (0x0601) ACC_PUBLIC, ACC_INTERFACE, ACC_ABSTRACT
```
Default methods on functional interfaces compile into:
```
public default java.util.function.Predicate<T> and(java.util.function.Predicate<? super T>);
  flags: (0x0001) ACC_PUBLIC
  Code:
     0: aload_1
     1: invokestatic  #1 // Method java/util/Objects.requireNonNull:(Ljava/lang/Object;)Ljava/lang/Object;
     4: pop
     5: aload_0
     6: aload_1
     7: invokedynamic #7, 0 // InvokeDynamic #0:test:(Ljava/util/function/Predicate;Ljava/util/function/Predicate;)Ljava/util/function/Predicate;
    12: areturn
```
Notice that default combinators like `and()` use `invokedynamic` to construct composite predicates, avoiding manual anonymous class instantiation!

### Detailed Exercise Solutions (All 9 Problems)
<details>
<summary>View Complete Solutions for All Exercises</summary>

```java
// Easy 1: TriFunction
@FunctionalInterface
public interface TriFunction<A, B, C, R> {
    R apply(A a, B b, C c);
}

// Easy 2: Predicate combination
Predicate<String> nonNull = Objects::nonNull;
Predicate<String> longEnough = s -> s.length() > 5;
Predicate<String> valid = nonNull.and(longEnough);

// Easy 3: Random Double Supplier
Supplier<Double> randomSupplier = Math::random;

// Medium 4: Memoization function decorator
public static <T, R> Function<T, R> memoize(Function<T, R> fn) {
    Map<T, R> cache = new java.util.concurrent.ConcurrentHashMap<>();
    return input -> cache.computeIfAbsent(input, fn);
}

// Medium 5: UnaryOperator pipeline
UnaryOperator<String> trim = String::trim;
UnaryOperator<String> lower = String::toLowerCase;
UnaryOperator<String> prefix = s -> "ID_" + s;
Function<String, String> pipeline = trim.andThen(lower).andThen(prefix);

// Medium 6: Conditional executor
public static <T> void executeIf(T val, Predicate<T> condition, Consumer<T> action) {
    if (condition.test(val)) action.accept(val);
}

// Hard 7: Async retry utility
public static <T> CompletableFuture<T> retryAsync(Supplier<CompletableFuture<T>> task, int retries) {
    return task.get().exceptionallyCompose(ex -> {
        if (retries > 0) return retryAsync(task, retries - 1);
        return CompletableFuture.failedFuture(ex);
    });
}

// Hard 8: Currying BiFunction
public static <A, B, R> Function<A, Function<B, R>> curry(BiFunction<A, B, R> biFn) {
    return a -> b -> biFn.apply(a, b);
}

// Hard 9: Expression AST evaluator
public interface Expr {
    <R> R accept(ExprVisitor<R> visitor);
}
public interface ExprVisitor<R> {
    R visitConst(int val);
    R visitAdd(Expr left, Expr right);
}
```
</details>

## 12. Summary / Cheat Sheet

- **SAM Rule**: Exactly one abstract method (Object methods excluded).
- **Core 4**: `Predicate<T>` (boolean), `Function<T, R>` (transform), `Consumer<T>` (void), `Supplier<T>` (generate).
- **Operators**: `UnaryOperator<T>` (single type), `BinaryOperator<T>` (combine two).
- **Primitives**: Always prefer `IntPredicate`, `LongFunction`, etc. to eliminate boxing.
- **Composition**: `and()`, `or()`, `negate()` for Predicates; `andThen()`, `compose()` for Functions.

---

## 13. Further Reading
- Bloch, Joshua. *Effective Java (3rd Edition)*: Item 44 (Favor the use of standard functional interfaces).
- Oracle Java Documentation: `java.util.function` package summary.
- Goetz, Brian. *Java 8: The Functional Interfaces Paradigm*.
