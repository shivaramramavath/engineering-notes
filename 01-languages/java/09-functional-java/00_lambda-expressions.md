# Lambda Expressions

> Anonymous functional expressions in Java: invokedynamic desugaring, LambdaMetafactory, capturing vs non-capturing mechanics, and closure invariants.

| Metadata | Value |
|---|---|
| **Level** | Intermediate |
| **Prerequisites** | [04-oop/09_interfaces.md](../04-oop/09_interfaces.md), [04-oop/12_nested-and-inner-classes.md](../04-oop/12_nested-and-inner-classes.md) |
| **Time to Read** | 24 minutes |
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

Prior to Java 8, passing behavior (a piece of code) as a parameter required creating an **Anonymous Inner Class**. This approach suffered from three major problems:
1. **Verbose Syntax**: Instantiating an anonymous inner class required 4 to 6 lines of boilerplate code to express a single line of logic.
2. **Memory Footprint**: Every anonymous inner class generated a distinct compiled `.class` file on disk (`Outer$1.class`) and allocated a brand new heap object with its own object header and enclosed `this` reference every time it was evaluated.
3. **Variable Shadowing Confusion**: In an anonymous inner class, `this` referred to the anonymous inner class instance itself, not the enclosing class, leading to subtle scope bugs.

Introduced in Java 8 (JSR 335), **Lambda Expressions** treat code as data: they allow passing anonymous functional behaviors concisely. Rather than compiling to anonymous classes, Java lambdas are desugared at compile time into private synthetic methods and linked at runtime using the high-performance **`invokedynamic`** JVM instruction and `LambdaMetafactory`.

---

## 2. Core Concepts

### Syntax Structure
A lambda expression consists of three components:
1. **Parameter List**: `(int a, int b)` or `(a, b)` (types inferred) or `x` (single parameter without parentheses).
2. **Arrow Token**: `->` separates parameters from the body.
3. **Body**: Either a single expression `a + b` (implicit return) or a block `{ ...; return result; }`.

```
(param1, param2) -> expression
(param1, param2) -> { statements; return value; }
```

### Functional Interface Target Typing
A lambda expression cannot exist in isolation; it has no standalone type. Its type is inferred from the context where it appears—known as the **target type**. The target type **must** be a **Functional Interface** (an interface with exactly one abstract method).

### Capturing vs Non-Capturing Lambdas
- **Non-Capturing Lambda**: Does not reference any variables from its enclosing lexical scope. It evaluates to a static singleton instance reused across all invocations (zero heap allocation overhead).
- **Capturing Lambda**: References `final` or **effectively final** variables from its enclosing scope (a closure). It must capture the enclosing state, allocating an instance to hold the captured values.

### Effectively Final Invariant
A local variable is **effectively final** if its value is never modified after it is initialized. Lambdas can capture local variables only if they are effectively final. This restriction ensures thread safety and avoids data races between the stack frame where the variable lives and the thread executing the lambda.

---

## 3. How It Works Internally

### The `invokedynamic` and `LambdaMetafactory` Architecture
Unlike inner classes, the Java compiler does **NOT** generate a `.class` file for a lambda expression.

Instead:
1. **Synthetic Method Generation**: `javac` extracts the lambda body into a private synthetic method inside the enclosing class:
   - If non-capturing: `private static synthetic void lambda$main$0(...)`
   - If capturing: `private synthetic void lambda$main$0(...)`
2. **`invokedynamic` Call Site**: At the location of the lambda expression, `javac` emits an `invokedynamic` bytecode instruction pointing to the bootstrap method:
   `java.lang.invoke.LambdaMetafactory.metafactory(...)`.
3. **Spinning Dynamic Call Site**: On the first execution of `invokedynamic`, the JVM calls `LambdaMetafactory`. It dynamically generates a lightweight class implementing the functional interface via the internal Class-File generator and links a `MethodHandle` directly to the synthetic lambda method.
4. **Subsequent Calls**: All subsequent invocations bypass reflection and execute directly at raw bytecode speed.

```
+-------------------------------------------------------+
| Lambda Compilation and Runtime Flow                   |
|                                                       |
| Source: list.forEach(x -> System.out.println(x));     |
|                        |                              |
|                        v javac compilation            |
| 1. Generates private synthetic method:                |
|    private static void lambda$0(String x) { ... }     |
| 2. Emits invokedynamic instruction at call site.      |
|                        |                              |
|                        v First execution (JVM Runtime)|
| 3. LambdaMetafactory generates implementation class   |
|    in-memory (no disk class file).                    |
| 4. Returns CallSite with MethodHandle.                |
|                        |                              |
|                        v Subsequent calls             |
| 5. Direct optimized JIT machine code call!            |
+-------------------------------------------------------+
```

### Lexical Scoping and `this`
In a lambda expression, `this` refers to the **enclosing class instance**, not the lambda object. There is no shadowing of enclosing member variables:
```java
public class ScopeDemo {
    private String name = "Enclosing";

    public void test() {
        Runnable r = () -> {
            System.out.println(this.name); // Refers to ScopeDemo.name!
        };
        r.run();
    }
}
```

---

## 4. Syntax & Usage

### 1. Parameter Syntax Variations
```java
// 1. Zero parameters
Runnable r = () -> System.out.println("No params");

// 2. Single parameter (parentheses optional)
Consumer<String> c1 = s -> System.out.println(s);
Consumer<String> c2 = (s) -> System.out.println(s);

// 3. Multiple parameters
BiFunction<Integer, Integer, Integer> adder = (a, b) -> a + b;

// 4. Explicit parameter types (useful with annotations)
BiFunction<Integer, Integer, Integer> explicit = (Integer a, Integer b) -> a + b;

// 5. Block body with return
BiFunction<Integer, Integer, Integer> block = (a, b) -> {
    int sum = a + b;
    return sum * 2;
};
```

### 2. Capturing Enclosing State
```java
public class CaptureDemo {
    public static void main(String[] args) {
        String prefix = "LOG: "; // Effectively final variable

        // Captures 'prefix' from enclosing stack frame
        Consumer<String> logger = msg -> System.out.println(prefix + msg);
        logger.accept("Connection established");

        // prefix = "WARN: "; // COMPILE ERROR: Variable used in lambda must be effectively final!
    }
}
```

---

## 5. Practical Examples

### Example 1: Custom Strategy Injection (Basic)
```java
import java.util.ArrayList;
import java.util.List;

public class LambdaStrategyDemo {
    @FunctionalInterface
    public interface StringFilter {
        boolean test(String s);
    }

    public static List<String> filterStrings(List<String> input, StringFilter filter) {
        List<String> result = new ArrayList<>();
        for (String s : input) {
            if (filter.test(s)) result.add(s);
        }
        return result;
    }

    public static void main(String[] args) {
        List<String> names = List.of("Alice", "Bob", "Charlie", "Anna");
        List<String> aNames = filterStrings(names, s -> s.startsWith("A"));
        System.out.println("Names starting with A: " + aNames); // [Alice, Anna]
    }
}
```

### Example 2: Event Listener Pipeline with Closures (Real-World)
```java
import java.util.ArrayList;
import java.util.List;
import java.util.function.Consumer;

public class EventPipeline {
    private final List<Consumer<String>> listeners = new ArrayList<>();

    public void register(Consumer<String> listener) {
        listeners.add(listener);
    }

    public void dispatch(String event) {
        for (Consumer<String> listener : listeners) {
            listener.accept(event);
        }
    }

    public static void main(String[] args) {
        EventPipeline pipeline = new EventPipeline();
        long startTime = System.currentTimeMillis();

        // Capturing lambda attaching timestamp metadata
        pipeline.register(event -> {
            long latency = System.currentTimeMillis() - startTime;
            System.out.println("[+" + latency + "ms] Event received: " + event);
        });

        pipeline.dispatch("USER_LOGIN");
    }
}
```

### Example 3: Edge Case - Emulating Mutable State via AtomicReference or Array
```java
import java.util.concurrent.atomic.AtomicInteger;

public class MutableCaptureEdgeCase {
    public static void main(String[] args) {
        // WRONG: int counter = 0; () -> counter++; // COMPILE ERROR!

        // WORKAROUND A: 1-element array
        int[] counterArr = {0};
        Runnable r1 = () -> counterArr[0]++;
        r1.run();
        System.out.println("Counter via array: " + counterArr[0]); // 1

        // WORKAROUND B: AtomicInteger (Thread-safe, preferred)
        AtomicInteger atomicCounter = new AtomicInteger(0);
        Runnable r2 = () -> atomicCounter.incrementAndGet();
        r2.run();
        System.out.println("Counter via atomic: " + atomicCounter.get()); // 1
    }
}
```

### Example 4: Failure Case - Serialization Hazard of Lambdas
```java
import java.io.*;

public class LambdaSerializationFailure {
    // If a lambda captures an enclosing non-serializable object,
    // serializing the functional interface throws NotSerializableException!
    public static void main(String[] args) {
        NonSerializableOuter outer = new NonSerializableOuter();
        outer.runTest();
    }

    static class NonSerializableOuter {
        String data = "Sensitive";
        public void runTest() {
            // Captures 'this' implicitly!
            Runnable r = (Runnable & Serializable) () -> System.out.println(data);
            try (var oos = new ObjectOutputStream(new ByteArrayOutputStream())) {
                oos.writeObject(r); // THROWS NotSerializableException: NonSerializableOuter!
            } catch (Exception e) {
                System.out.println("Serialization failed: " + e.getClass().getSimpleName());
            }
        }
    }
}
```

---

## 6. Best Practices

1. **Keep Lambdas Short and Focused (1-3 lines)**: If lambda logic requires complex branching or multiple statements, extract it into a named private method and use a method reference.
2. **Favor Standard Functional Interfaces**: Use `java.util.function` interfaces (`Predicate`, `Function`, `Consumer`, `Supplier`) rather than inventing custom single-method interfaces.
3. **Avoid Side Effects Inside Lambdas**: Lambdas should be pure functions that compute outputs from inputs without mutating external shared state.
4. **Use Primitive Specializations to Avoid Boxing**: Use `IntConsumer`, `LongPredicate`, `DoubleFunction` instead of `Consumer<Integer>`, etc.

---

## 7. Common Mistakes & Pitfalls

| Mistake | Why Wrong | Fix |
|---|---|---|
| Modifying local variables inside lambda | Violates the effectively final requirement; causes compilation error. | Use `AtomicInteger` or redesign to avoid mutation. |
| Writing multi-line spaghetti code inside lambda | Degrades readability, impedes stack trace debugging, and prevents unit testing. | Extract to private method and use method reference. |
| Confusing `this` inside lambda | In a lambda, `this` refers to enclosing class, NOT the lambda instance. | Access enclosing members directly or use inner class if inner `this` is needed. |
| Catching checked exceptions inside concise lambdas | Functional interface method signature doesn't declare throws; requires messy try-catch. | Wrap in unchecked exception or use custom throwing functional interface. |
| Re-declaring parameter names that match local variables | Parameter names cannot shadow variables in the enclosing scope. | Choose distinct parameter names. |
| Serializing lambdas | Lambdas capture synthetic compiler names and enclosing classes, leading to fragile serialization. | Avoid serializing lambdas; use explicit classes if serialization is mandatory. |

---

## 8. Performance & Security Notes

### Performance
- **Non-Capturing Lambdas Allocate Once**: `LambdaMetafactory` generates a singleton instance for non-capturing lambdas. Subsequent executions reuse the same instance, generating **zero garbage collection pressure**.
- **Capturing Lambdas**: Each evaluation allocates a small synthetic object holding captured variables (~16-24 bytes).

### Security
- **Encapsulated Synthetic Methods**: `javac` marks synthetic lambda methods with package or private access and the `ACC_SYNTHETIC` flag, preventing unauthorized third-party classes from invoking them directly.

---

## 9. Comparison / When to Use What

| Feature | Lambda Expression | Anonymous Inner Class | Method Reference |
|---|---|---|---|
| **Class File on Disk** | None (dynamic in-memory) | Distinct `.class` file | None (dynamic in-memory) |
| **`this` Reference** | Enclosing class instance | Anonymous inner instance | Enclosing class instance |
| **Interfaces Supported** | Functional Interfaces only | Any interface or abstract class | Functional Interfaces only |
| **State Fields** | Cannot declare fields | Can declare fields & methods | Cannot declare fields |
| **Allocation Cost** | Zero (non-capturing) / Single instance | Object allocation per instantiation | Zero (non-capturing) |

---

## 10. Interview Questions

<details>
<summary>1. How does Java compile and execute a lambda expression under the hood?</summary>
`javac` extracts the lambda body into a private synthetic method on the enclosing class and emits an `invokedynamic` instruction pointing to `LambdaMetafactory.metafactory`. At runtime, on first execution, the JVM dynamically generates a lightweight implementation class and links a direct `MethodHandle` call site, avoiding class-loader overhead and reflection.
</details>

<details>
<summary>2. What is an "effectively final" variable and why is it required by lambdas?</summary>
An effectively final variable is a local variable whose value is never changed after initialization, even though it lacks the `final` keyword. Lambdas capture the value of the variable by copying it. If mutation were allowed, concurrency hazards, race conditions, and synchronization discrepancies between stack copies and heap copies would arise.
</details>

<details>
<summary>3. What is the difference between capturing and non-capturing lambdas?</summary>
A non-capturing lambda references no local or instance variables from its enclosing scope; the JVM optimizes it into a shared singleton instance. A capturing lambda references enclosing state, requiring an object allocation on each evaluation to hold the captured values.
</details>

<details>
<summary>4. What does `this` refer to inside a lambda expression?</summary>
`this` refers strictly to the instance of the enclosing class in which the lambda expression is written. It never refers to the lambda instance itself.
</details>

<details>
<summary>5. Can a lambda expression implement an abstract class?</summary>
No. A lambda expression can only target a **Functional Interface** (an interface with exactly one abstract method). It cannot implement an abstract class, even if that class has only one abstract method.
</details>

<details>
<summary>6. How do you handle checked exceptions inside a lambda expression?</summary>
Standard functional interfaces (like `Function`, `Predicate`) do not declare checked exceptions in their signatures. You must either handle the exception inside the lambda with a `try-catch` block (rethrowing as `RuntimeException`), or declare a custom throwing functional interface.
</details>

<details>
<summary>7. What is target typing in the context of lambdas?</summary>
Target typing is the compiler's mechanism for inferring the functional interface type of a lambda expression from the context where it appears (e.g., parameter type of a method call, variable assignment, or return statement).
</details>

<details>
<summary>8. Why should you avoid serializing lambda expressions?</summary>
Because the generated synthetic method names, class structures, and captured references are compiler-implementation details. Any change in compiler version, code refactoring, or enclosing class will break deserialization with `InvalidClassException`.
</details>

---

## 11. Practice Exercises

### Easy
1. Write a lambda expression that checks if a string is non-empty.
2. Sort a list of names alphabetically using a lambda comparator.
3. Create a `Runnable` lambda that prints the current system time.

### Medium
4. Write a higher-order function `retry(Runnable task, int maxRetries)` using lambdas.
5. Implement a generic exception wrapper `uncheck(ThrowingConsumer<T> consumer)` that wraps checked exceptions into `RuntimeException`.
6. Implement a builder pattern where steps are configured via chained consumer lambdas (`builder.configure(c -> c.setPort(8080))`).

### Hard
7. Implement an in-memory event bus supporting strongly typed topic subscriptions via lambda consumers.
8. Decompile a compiled class containing lambdas using `javap -c -p -v` and inspect the `BootstrapMethods` table and `invokedynamic` instructions.
9. Implement a dynamic pipeline composer that chains an arbitrary list of `Function<T, T>` instances into a single optimized composite function.

<details>
<summary>Solutions</summary>

```java
// Easy 1: Non-empty string
java.util.function.Predicate<String> isNonEmpty = s -> s != null && !s.isEmpty();

// Medium 5: Throwing consumer wrapper
@FunctionalInterface
public interface ThrowingConsumer<T> {
    void accept(T t) throws Exception;
    static <T> java.util.function.Consumer<T> uncheck(ThrowingConsumer<T> consumer) {
        return t -> {
            try { consumer.accept(t); }
            catch (Exception e) { throw new RuntimeException(e); }
        };
    }
}

// Hard 9: Dynamic pipeline composer
public static <T> java.util.function.Function<T, T> composePipeline(List<java.util.function.Function<T, T>> stages) {
    return stages.stream().reduce(java.util.function.Function.identity(), java.util.function.Function::andThen);
}
```
</details>

---

## 12. Summary / Cheat Sheet

- **Syntax**: `(args) -> body`.
- **Target Type**: Must be a functional interface with single abstract method (SAM).
- **Compilation**: Desugared into private synthetic methods and linked via `invokedynamic` + `LambdaMetafactory`.
- **Scope**: Lexical; `this` refers to enclosing class instance.
- **Variables**: Can only capture `final` or effectively final variables.
- **Allocation**: Non-capturing lambdas allocate once (singleton); capturing lambdas allocate per execution.

---

## 13. Further Reading
- Goetz, Brian. *Translation of Lambda Expressions* (OpenJDK design document).
- Bloch, Joshua. *Effective Java (3rd Edition)*: Item 42 (Prefer lambdas to anonymous classes).
- Oracle Java Documentation: *The Java Tutorials - Lambda Expressions*.
