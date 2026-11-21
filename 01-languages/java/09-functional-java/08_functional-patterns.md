# Functional Programming Design Patterns

> Monads, currying, partial application, Railway Oriented Programming, and fluent functional combinators in modern Java.

| Metadata | Value |
|---|---|
| **Level** | Advanced |
| **Prerequisites** | [00_lambda-expressions.md](00_lambda-expressions.md), [01_functional-interfaces.md](01_functional-interfaces.md), [03_optional.md](03_optional.md) |
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

Traditional object-oriented design patterns (GoF patterns like Strategy, Command, Template Method, Observer) frequently require creating extensive class hierarchies, abstract base classes, and boilerplate interfaces. In functional programming, many of these classic patterns collapse into single-line lambda expressions or functional compositions.

Furthermore, advanced functional architectures introduce concepts that fundamentally improve robustness and readability:
- **Pure Functions**: Functions without side effects that return deterministic results for given inputs.
- **Higher-Order Functions**: Functions that accept other functions as arguments or return functions.
- **Monads**: Containers that encapsulate computational effects (like absence with `Optional`, failure with `Result`, or latency with `CompletableFuture`) and support composable chaining (`flatMap`).
- **Railway Oriented Programming**: Modeling business workflows as a two-track pipeline (Success vs Failure) without throwing exceptions.

This note covers how to apply idiomatic functional design patterns within Java's strong static type system.

---

## 2. Core Concepts

### 1. Currying and Partial Application
- **Currying**: Translating a function that takes multiple arguments into a chain of functions that each take a single argument:
  $f(x, y) \to f(x)(y)$.
- **Partial Application**: Fixing a subset of arguments of a function to produce a new function with fewer arguments.

```java
// Curried function:
Function<Integer, Function<Integer, Integer>> add = x -> y -> x + y;
Function<Integer, Integer> add5 = add.apply(5); // Partially applied!
int result = add5.apply(10); // 15
```

### 2. The Monad Laws in Java
A monad in Java (like `Optional`, `Stream`, `CompletableFuture`) consists of:
1. **Type Constructor**: Wraps a value of type `T` into `M<T>`.
2. **Unit / Return**: `of(val)` wraps value.
3. **Bind / FlatMap**: `flatMap(Function<T, M<U>>)` chains operations.

Monadic Laws:
- **Left Identity**: `M.of(x).flatMap(f) == f(x)`
- **Right Identity**: `m.flatMap(M::of) == m`
- **Associativity**: `m.flatMap(f).flatMap(g) == m.flatMap(x -> f(x).flatMap(g))`

### 3. Railway Oriented Programming (`Result<T, E>`)
In standard Java, errors are often signaled by throwing exceptions. In Railway Oriented Programming, methods return a `Result<T, E>` monad representing either **Success(T)** or **Failure(E)**. Computations stay on the "success track" until an error occurs, cleanly switching to the "failure track" without stack unwinding overhead.

```
Success Track: [ Step 1: Parse ] ===> [ Step 2: Validate ] ===> [ Step 3: Save ] ===> Success!
                                \                   \
                                 \--[ Failure Track: Error ]-------------------------> Error!
```

---

## 3. How It Works Internally

### Lambda Closures and Closure Chaining
When currying functions in Java:
```java
Function<String, Function<String, String>> formatter = prefix -> body -> prefix + body;
```
1. Calling `formatter.apply("LOG: ")` creates a synthetic lambda object capturing the `prefix` argument in an instance field.
2. The returned `Function<String, String>` holds a live reference to that captured state.
3. Calling `apply("Connection OK")` evaluates the inner synthetic method with both the captured field and the new argument.

---

## 4. Syntax & Usage

### 1. Fluent Predicate Combinator Pattern
```java
import java.util.function.Function;
import java.util.function.Predicate;

public class CombinatorDemo {
    public interface UserValidation extends Function<User, ValidationResult> {
        static UserValidation isEmailValid() {
            return user -> user.email().contains("@") ? ValidationResult.SUCCESS : ValidationResult.EMAIL_INVALID;
        }

        static UserValidation isAdult() {
            return user -> user.age() >= 18 ? ValidationResult.SUCCESS : ValidationResult.UNDERAGE;
        }

        default UserValidation and(UserValidation other) {
            return user -> {
                ValidationResult result = this.apply(user);
                return result == ValidationResult.SUCCESS ? other.apply(user) : result;
            };
        }
    }

    public enum ValidationResult { SUCCESS, EMAIL_INVALID, UNDERAGE }
    public record User(String email, int age) {}

    public static void main(String[] args) {
        User user = new User("bad-email", 25);
        ValidationResult result = UserValidation.isEmailValid()
            .and(UserValidation.isAdult())
            .apply(user);

        System.out.println("Validation result: " + result); // EMAIL_INVALID
    }
}
```

### 2. Currying a Multi-Parameter Function
```java
import java.util.function.BiFunction;
import java.util.function.Function;

public class CurryingDemo {
    public static void main(String[] args) {
        // Standard BiFunction
        BiFunction<Double, Double, Double> calcTax = (rate, amount) -> amount * (1.0 + rate);

        // Curried Function
        Function<Double, Function<Double, Double>> curriedTax = rate -> amount -> amount * (1.0 + rate);

        // Create specialized VAT function for 20%
        Function<Double, Double> applyVat = curriedTax.apply(0.20);

        System.out.println("Price with VAT: $" + applyVat.apply(100.0)); // $120.0
        System.out.println("Price with VAT: $" + applyVat.apply(250.0)); // $300.0
    }
}
```

---

## 5. Practical Examples

### Example 1: Full Railway Oriented Programming Implementation (Real-World)
```java
import java.util.Objects;
import java.util.function.Consumer;
import java.util.function.Function;

public final class Result<T, E> {
    private final T value;
    private final E error;
    private final boolean isSuccess;

    private Result(T value, E error, boolean isSuccess) {
        this.value = value;
        this.error = error;
        this.isSuccess = isSuccess;
    }

    public static <T, E> Result<T, E> ok(T value) {
        return new Result<>(Objects.requireNonNull(value), null, true);
    }

    public static <T, E> Result<T, E> err(E error) {
        return new Result<>(null, Objects.requireNonNull(error), false);
    }

    public boolean isSuccess() { return isSuccess; }

    public <U> Result<U, E> map(Function<? super T, ? extends U> mapper) {
        if (isSuccess) return Result.ok(mapper.apply(value));
        return Result.err(error);
    }

    public <U> Result<U, E> flatMap(Function<? super T, Result<U, E>> mapper) {
        if (isSuccess) return mapper.apply(value);
        return Result.err(error);
    }

    public void ifSuccess(Consumer<T> action) { if (isSuccess) action.accept(value); }
    public void ifError(Consumer<E> action) { if (!isSuccess) action.accept(error); }

    public static void main(String[] args) {
        // Railway pipeline
        Result<Integer, String> pipeline = Result.<String, String>ok("123")
            .flatMap(s -> s.length() <= 5 ? Result.ok(s) : Result.err("String too long"))
            .map(Integer::parseInt)
            .map(n -> n * 2);

        pipeline.ifSuccess(val -> System.out.println("Success: " + val)); // Success: 246
        pipeline.ifError(err -> System.out.println("Error: " + err));
    }
}
```

### Example 2: Loan Pattern via Functional Resource Management (Basic to Intermediate)
```java
import java.util.function.Consumer;

public class ResourceLoanPattern {
    public static class DatabaseTransaction {
        public void commit() { System.out.println("Transaction committed."); }
        public void rollback() { System.out.println("Transaction rolled back."); }
    }

    // Guarantees transaction cleanup around arbitrary user logic
    public static void withTransaction(Consumer<DatabaseTransaction> action) {
        DatabaseTransaction tx = new DatabaseTransaction();
        try {
            action.accept(tx);
            tx.commit();
        } catch (Exception e) {
            tx.rollback();
            throw e;
        }
    }

    public static void main(String[] args) {
        withTransaction(tx -> {
            System.out.println("Executing SQL insert 1...");
            System.out.println("Executing SQL insert 2...");
        });
    }
}
```

### Example 3: Edge Case - Stack Overflow in Recursive Lambda Compositions
```java
import java.util.function.Function;

public class DeepCompositionStackOverflow {
    public static void main(String[] args) {
        Function<Integer, Integer> pipeline = Function.identity();

        // Chaining 50,000 function compositions creates 50,000 nested call frames!
        for (int i = 0; i < 50_000; i++) {
            pipeline = pipeline.andThen(x -> x + 1);
        }

        try {
            pipeline.apply(0); // Deep recursion throws StackOverflowError!
        } catch (StackOverflowError e) {
            System.out.println("Caught expected StackOverflowError from deep composition!");
        }
    }
}
```

### Example 4: Failure Case - Side-Effects in Pure Functional Pipelines
```java
// Anti-pattern: Mutating external state inside functional map
// list.stream().map(item -> { externalCounter++; return item; });
// Violates referential transparency and breaks in parallel execution!
```

---

## 6. Best Practices

1. **Favor Pure Functions**: Ensure functions produce no side-effects and return the exact same output for the same input.
2. **Use Railway Oriented Programming for Domain Workflows**: Replace throwing exceptions for predictable domain errors (e.g., validation failures, user not found) with `Result<T, E>`.
3. **Use the Combinator Pattern for Multi-Rule Validations**: Build composable validation rules using default methods on functional interfaces.
4. **Beware of Deep Lambda Chains**: Chaining thousands of `.andThen()` functions creates deeply nested call stacks that risk `StackOverflowError`.

---

## 7. Common Mistakes & Pitfalls

| Mistake | Why Wrong | Fix |
|---|---|---|
| Side-effects in functional lambdas | Violates purity; creates race conditions and non-deterministic behavior. | Keep functions pure; isolate side-effects to consumers. |
| Deeply nested `.andThen()` chains | Stack frames accumulate recursively, causing `StackOverflowError`. | Use an iteration loop over a list of functions. |
| Using exceptions for business control flow | Exception generation requires expensive stack-walk overhead. | Use `Result<T, E>` monad. |
| Over-currying simple methods | Incurs cognitive complexity and garbage allocations for trivial code. | Use standard multi-parameter methods unless partial application is needed. |
| Forgetting left/right identity in custom monads | Violates monad algebraic laws, leading to unexpected composition bugs. | Test monad laws explicitly. |

---

## 8. Performance & Security Notes

### Performance
- **Zero Stack-Trace Overhead with Monads**: Throwing an exception costs thousands of CPU cycles due to JVM stack walking (`fillInStackTrace()`). Returning a `Result<T, E>` monad is an ordinary object return costing a few nanoseconds.
- **Closure Allocation in Currying**: Each partially applied curried function allocates a closure object on the heap. Avoid currying inside nanosecond loops.

### Security
- **Deterministic Pure Pipelines**: Pure functions cannot be exploited through side-channel state poisoning, making security verification straightforward.

---

## 9. Comparison / When to Use What

| Pattern | Traditional OOP | Functional Equivalent | Primary Benefit |
|---|---|---|---|
| **Strategy** | Strategy interface + concrete classes | Single `Function` / `Predicate` lambda | Eliminates boilerplate class files |
| **Command** | Command interface with `execute()` | `Runnable` / `Consumer` | Inline command dispatch |
| **Template Method** | Abstract base class + hook methods | Higher-order function with lambda hook | Avoids inheritance rigidness |
| **Error Handling** | Throws checked/unchecked exceptions | Monadic `Result<T, E>` / `Optional<T>` | Explicit, fast, composable |

---

## 10. Interview Questions

<details>
<summary>1. What is Railway Oriented Programming and what advantages does it offer?</summary>
Railway Oriented Programming is a functional design pattern where operations return a two-track container (`Result<T, E>`). If an operation succeeds, execution continues down the "success track". If an error occurs, execution switches to the "failure track", bypassing subsequent steps. It avoids expensive exception stack-trace overhead and makes error handling explicit in types.
</details>

<details>
<summary>2. What is currying versus partial application?</summary>
Currying is the process of converting a function of $N$ arguments into a chain of $N$ functions that each accept 1 argument ($f(x, y) \to f(x)(y)$). Partial application is the process of binding values to a subset of arguments, producing a new function of lower arity ($f(x, y)$ with $x=5 \to g(y)$).
</details>

<details>
<summary>3. What are the three mathematical laws of a Monad?</summary>
1. **Left Identity**: Wrapping a value in the monad and flatMapping a function produces the same result as applying the function directly (`M.of(x).flatMap(f) == f(x)`).
2. **Right Identity**: FlatMapping the monad's unit constructor over a monad returns the original monad (`m.flatMap(M::of) == m`).
3. **Associativity**: Chaining flatMap operations produces identical results regardless of nesting order (`m.flatMap(f).flatMap(g) == m.flatMap(x -> f(x).flatMap(g))`).
</details>

<details>
<summary>4. Why does deep `Function.andThen()` chaining risk a `StackOverflowError`?</summary>
Because each `.andThen(g)` wraps the previous function in a closure `(arg) -> g.apply(f.apply(arg))`. When invoked, each wrapped function adds a new stack frame, leading to linear call-stack growth.
</details>

<details>
<summary>5. How does the Loan Pattern work in Java?</summary>
The Loan Pattern is a resource management pattern where a method acquires a resource, passes it to a user-supplied lambda (loaning the resource), and guarantees cleanup (closing/committing) in a `finally` block once the lambda returns.
</details>

<details>
<summary>6. How do lambdas simplify the GoF Strategy Pattern?</summary>
In classic OOP, the Strategy pattern requires an interface and multiple concrete class implementations. With lambdas, any functional interface serves as the strategy, and callers pass arbitrary inline lambdas or method references without declaring classes.
</details>

<details>
<summary>7. What is a "pure function"?</summary>
A function whose return value depends exclusively on its input parameters and which produces zero observable side effects (no mutation of external variables, no I/O, no database writes).
</details>

<details>
<summary>8. How can you implement memoization in Java using functional programming?</summary>
By creating a higher-order function `memoize(Function<T, R>)` that maintains a concurrent hash map cache and wraps the function using `cache.computeIfAbsent(input, originalFunction)`.
</details>

---

## 11. Practice Exercises

### Easy
1. Write a curried function `multiply` that multiplies two doubles.
2. Implement a higher-order function `repeat(int times, Runnable action)`.
3. Create a predicate combinator that negates an existing predicate.

### Medium
4. Implement a thread-safe memoizer function decorator using `ConcurrentHashMap`.
5. Implement the Loan pattern for a simulated file writer that guarantees flush and close.
6. Write a pipeline using `Result<T, E>` that parses a string, validates that it is a positive integer, and computes its square root.

### Hard
7. Implement an immutable Functional State Monad (`State<S, A>`) supporting `run`, `map`, and `flatMap`.
8. Implement an asynchronous Railway Oriented Pipeline combining `CompletableFuture` and `Result<T, E>`.
9. Construct a domain validation framework using the Combinator Pattern that collects all validation errors in an aggregate report.

<details>
<summary>Solutions</summary>

```java
// Easy 1: Curried multiply
Function<Double, Function<Double, Double>> multiply = x -> y -> x * y;

// Medium 4: Memoizer
public static <T, R> Function<T, R> memoize(Function<T, R> fn) {
    Map<T, R> cache = new java.util.concurrent.ConcurrentHashMap<>();
    return input -> cache.computeIfAbsent(input, fn);
}

// Medium 5: Loan pattern
public static void withFileWriter(String path, Consumer<java.io.PrintWriter> consumer) throws Exception {
    try (var writer = new java.io.PrintWriter(new java.io.FileWriter(path))) {
        consumer.accept(writer);
    }
}
```
</details>

---

## 12. Summary / Cheat Sheet

- **Currying**: `A -> B -> C` (Chained single-argument functions).
- **Monads**: Encapsulate computational context (`Optional`, `CompletableFuture`, `Stream`); chained via `flatMap`.
- **Railway Oriented**: `Result<T, E>` separates success and failure tracks without exceptions.
- **Combinator Pattern**: Composes validation functions via fluent default methods on functional interfaces.
- **Purity**: Strive for pure, side-effect-free functions.

---

## 13. Further Reading
- Wlaschin, Scott. *Domain Modeling Made Functional* (Railway Oriented Programming).
- Ghosh, Debasish. *Functional and Reactive Domain Modeling*.
- Bloch, Joshua. *Effective Java (3rd Edition)*: Item 42 (Prefer lambdas to anonymous classes).
