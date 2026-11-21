# Method References

> First-class function handles in Java: the four method reference types, bytecode desugaring, constructor references, and array initializers.

| Metadata | Value |
|---|---|
| **Level** | Intermediate |
| **Prerequisites** | [00_lambda-expressions.md](00_lambda-expressions.md), [01_functional-interfaces.md](01_functional-interfaces.md) |
| **Time to Read** | 22 minutes |
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

When using lambda expressions, developers frequently find that the lambda body does nothing more than invoke an already existing method:
```java
// Lambda repeating existing method call
names.forEach(x -> System.out.println(x));
```
Writing `x -> System.out.println(x)` forces the developer to name a redundant temporary parameter (`x`) and re-state the method invocation.

**Method References** provide an even more compact, readable syntax to treat existing methods directly as first-class functions. Using the delimiter `::`, the lambda above simplifies to:
```java
names.forEach(System.out::println);
```
Where a lambda says "take parameter $x$ and pass it to method $M$", a method reference points directly to method $M$.

---

## 2. Core Concepts

### The Four Kinds of Method References
Java supports four distinct categories of method references:

| Kind | Syntax | Lambda Equivalent | Example |
|---|---|---|---|
| **1. Static Method Reference** | `ClassName::staticMethod` | `(args) -> ClassName.staticMethod(args)` | `Integer::parseInt` |
| **2. Bound Instance Method Reference** | `instance::instanceMethod` | `(args) -> instance.instanceMethod(args)` | `System.out::println` |
| **3. Unbound Instance Method Reference** | `ClassName::instanceMethod` | `(inst, args) -> inst.instanceMethod(args)` | `String::toLowerCase` |
| **4. Constructor Reference** | `ClassName::new` / `Type[]::new` | `(args) -> new ClassName(args)` | `ArrayList::new` |

### Distinction Between Bound and Unbound Method References
- **Bound Instance**: The target object instance is known and bound at the time the method reference is created.
  `myFormatter::format` binds to `myFormatter`. The functional interface takes only the method arguments.
- **Unbound Instance**: The target object instance is **not** known at definition time; it becomes the **first parameter** of the functional interface.
  `String::compareToIgnoreCase` corresponds to `(String s1, String s2) -> s1.compareToIgnoreCase(s2)`.

---

## 3. How It Works Internally

### Bytecode Desugaring via `invokedynamic`
Just like lambdas, method references do not compile into anonymous inner classes. They compile into `invokedynamic` instructions linked via `LambdaMetafactory`.

Consider:
```java
Function<String, Integer> parser = Integer::parseInt;
```
Bytecode:
```
invokedynamic #2, 0 // InvokeDynamic #0:apply:()Ljava/util/function/Function;
```
Inside the `BootstrapMethods` attribute:
```
0: #24 invokestatic java/lang/invoke/LambdaMetafactory.metafactory:...
  Method arguments:
    #25 (Ljava/lang/Object;)Ljava/lang/Object;
    #26 invokestatic java/lang/Integer.parseInt:(Ljava/lang/String;)I
    #27 (Ljava/lang/String;)Ljava/lang/Integer;
```
Instead of generating a synthetic forwarding method (like standard lambdas), the bootstrap method receives a direct `MethodHandle` pointing straight to `Integer.parseInt(String)`.

### Constructor Reference for Arrays
Constructor references can also instantiate arrays:
```java
IntFunction<String[]> arrayCreator = String[]::new;
String[] array = arrayCreator.apply(10); // Equivalent to new String[10];
```
This is essential in the Streams API:
```java
String[] names = stream.toArray(String[]::new);
```

---

## 4. Syntax & Usage

### 1. Static Method Reference
```java
import java.util.function.Function;

public class StaticRefDemo {
    public static void main(String[] args) {
        // Points to static method Integer.parseInt(String)
        Function<String, Integer> parser = Integer::parseInt;
        int value = parser.apply("1234");
        System.out.println("Parsed: " + value);
    }
}
```

### 2. Bound Instance Method Reference
```java
import java.util.function.Consumer;

public class BoundRefDemo {
    public static void main(String[] args) {
        String prefix = "PREFIX: ";
        // Bound to the specific instance referenced by 'prefix'
        Consumer<String> printer = System.out::println;
        printer.accept("Hello world");
    }
}
```

### 3. Unbound Instance Method Reference
```java
import java.util.function.BiPredicate;

public class UnboundRefDemo {
    public static void main(String[] args) {
        // String::startsWith where first argument is target instance!
        // Lambda: (str, prefix) -> str.startsWith(prefix)
        BiPredicate<String, String> checker = String::startsWith;
        boolean matches = checker.test("Java25", "Java");
        System.out.println("Matches: " + matches); // true
    }
}
```

### 4. Constructor Reference
```java
import java.util.ArrayList;
import java.util.List;
import java.util.function.Supplier;

public class ConstructorRefDemo {
    public static void main(String[] args) {
        // Supplier instantiating new ArrayList instances
        Supplier<List<String>> listFactory = ArrayList::new;
        List<String> list = listFactory.get();
        list.add("Item");
        System.out.println(list);
    }
}
```

---

## 5. Practical Examples

### Example 1: Stream Transformations (Basic)
```java
import java.util.List;

public class StreamMethodRefDemo {
    public static void main(String[] args) {
        List<String> names = List.of("alice", "bob", "charlie");

        // String::toUpperCase (Unbound) and System.out::println (Bound)
        names.stream()
            .map(String::toUpperCase)
            .forEach(System.out::println);
    }
}
```

### Example 2: Polymorphic Factory Pattern (Real-World)
```java
import java.util.Map;
import java.util.function.Supplier;

public class ConnectionFactory {
    public interface Connection { void connect(); }
    public static class MySqlConnection implements Connection {
        public void connect() { System.out.println("Connected to MySQL"); }
    }
    public static class PostgresConnection implements Connection {
        public void connect() { System.out.println("Connected to PostgreSQL"); }
    }

    private static final Map<String, Supplier<Connection>> REGISTRY = Map.of(
        "mysql", MySqlConnection::new,
        "postgres", PostgresConnection::new
    );

    public static Connection create(String driver) {
        Supplier<Connection> supplier = REGISTRY.get(driver.toLowerCase());
        if (supplier == null) throw new IllegalArgumentException("Unknown driver: " + driver);
        return supplier.get();
    }

    public static void main(String[] args) {
        Connection conn = create("postgres");
        conn.connect(); // Connected to PostgreSQL
    }
}
```

### Example 3: Edge Case - Ambiguous Overload Resolution
```java
public class AmbiguousOverloadRef {
    public static void print(String s) { System.out.println("String: " + s); }
    public static void print(Object o) { System.out.println("Object: " + o); }

    public static void main(String[] args) {
        // In functional interface with String, compiler selects print(String) cleanly:
        java.util.function.Consumer<String> stringConsumer = AmbiguousOverloadRef::print;
        stringConsumer.accept("Test");

        // If target type is Consumer<Object>, compiler selects print(Object):
        java.util.function.Consumer<Object> objConsumer = AmbiguousOverloadRef::print;
        objConsumer.accept(123);
    }
}
```

### Example 4: Failure Case - Method Reference with Side-Effects in Bound Receiver
```java
public class NullReceiverFailure {
    public static void main(String[] args) {
        String text = null;
        try {
            // Evaluates receiver expression immediately at definition time!
            java.util.function.Supplier<String> sup = text::toUpperCase;
        } catch (NullPointerException e) {
            System.out.println("Caught NPE: Bound receiver expression evaluated eagerly!");
        }
    }
}
```

---

## 6. Best Practices

1. **Favor Method References Over Lambdas for Conciseness**: If a lambda only forwards arguments to an existing method (`x -> foo(x)`), replace it with `Class::foo`.
2. **Keep Lambdas if Method References are Ambiguous or Confusing**: If a method reference makes the code less readable (e.g. `Function.identity()` vs `x -> x`, or complex overloaded methods), keep the lambda.
3. **Use Array Constructor References in Streams**: Use `stream.toArray(MyClass[]::new)` to collect into strongly-typed arrays.
4. **Be Mindful of Bound Receiver Null Checks**: In `expr::method`, `expr` is evaluated immediately when the method reference is created; if null, it throws `NullPointerException` instantly.

---

## 7. Common Mistakes & Pitfalls

| Mistake | Why Wrong | Fix |
|---|---|---|
| Confusing bound vs unbound instance references | Expecting `String::length` to know which string to measure without passing instance. | Pass target string as first parameter to unbound reference. |
| Null bound receiver expression | `nullInstance::method` throws `NullPointerException` immediately upon definition. | Ensure receiver instance is non-null before referencing. |
| Forgetting constructor arguments in `Class::new` | Compiler selects constructor matching the functional interface SAM signature. | Match constructor parameter types to SAM arguments. |
| Ambiguous overloaded static methods | Compiler cannot resolve reference if multiple overloads match the target type. | Revert to explicit lambda with cast parameters. |
| Using method reference when extra logic is needed | E.g. `x -> foo(x) + 1` cannot be written as `Class::foo`. | Use a standard lambda expression. |
| Expecting method references to ignore exceptions | Method must not throw checked exceptions undeclared by functional interface. | Wrap in unchecked exception. |

---

## 8. Performance & Security Notes

### Performance
- **Direct MethodHandle Linkage**: Method references avoid generating a synthetic forwarding method in the bytecode. `LambdaMetafactory` links directly to the target method handle, enabling aggressive inlining by the C2 JIT compiler.

### Security
- **Access Control Verification**: Method references enforce standard Java access control rules (`public`, `protected`, `private`) at compile time. A private method reference can only be formed within scopes that have legal access to that private method.

---

## 9. Comparison / When to Use What

| Scenario | Recommendation | Rationale |
|---|---|---|
| Pure argument forwarding (`x -> System.out.println(x)`) | Method Reference (`System.out::println`) | Cleaner, concise, zero parameter clutter |
| Instantiating objects (`() -> new ArrayList<>()`) | Constructor Reference (`ArrayList::new`) | Standard idiomatic factory syntax |
| Collecting stream to array (`stream.toArray(...)`) | Array Constructor (`String[]::new`) | Type-safe array allocation |
| Argument transformation (`x -> x.trim().toLowerCase()`) | Lambda Expression | Method references cannot chain multiple calls |
| Argument reordering (`(a, b) -> process(b, a)`) | Lambda Expression | Method references pass arguments in direct order |

---

## 10. Interview Questions

<details>
<summary>1. What are the four types of method references in Java?</summary>
1. Static method reference (`ClassName::staticMethod`). 2. Bound instance method reference (`instance::method`). 3. Unbound instance method reference (`ClassName::method`). 4. Constructor reference (`ClassName::new` / `Type[]::new`).
</details>

<details>
<summary>2. What is the difference between bound and unbound instance method references?</summary>
A bound method reference (`inst::method`) binds to a pre-existing object instance; the functional interface only supplies the method's parameters. An unbound method reference (`Class::method`) does not specify an instance; the functional interface's first parameter becomes the receiver object on which the method is called.
</details>

<details>
<summary>3. When does evaluation occur for a bound instance method reference?</summary>
The receiver expression is evaluated eagerly at the moment the method reference is created. If the receiver expression is `null`, a `NullPointerException` is thrown immediately at creation time, not when the functional interface method is invoked.
</details>

<details>
<summary>4. How does `stream.toArray(String[]::new)` work?</summary>
`toArray` accepts an `IntFunction<A[]> generator`. `String[]::new` satisfies `IntFunction<String[]>` by accepting an integer length and returning a `new String[length]`.
</details>

<details>
<summary>5. How does the compiler resolve overloaded methods in method references?</summary>
The compiler compares the parameter types and return type of the functional interface's abstract method with the available overloads of the referenced method, selecting the most specific match. If multiple overloads match ambiguously, compilation fails.
</details>

<details>
<summary>6. Does a method reference generate a synthetic method like a lambda does?</summary>
Usually no. Because the target method already exists, `LambdaMetafactory` can link a `MethodHandle` directly to the target method without needing `javac` to synthesize a forwarding bridge method.
</details>

<details>
<summary>7. Can a method reference refer to a generic method?</summary>
Yes. Generic type arguments can be inferred by the compiler or explicitly specified: `ClassName::<String>genericMethod`.
</details>

<details>
<summary>8. Why can't `(a, b) -> a + b` be written as a method reference?</summary>
Because `+` is an operator, not a method declared on any class. However, it can be written as `Integer::sum` which delegates to the static method `Integer.sum(int, int)`.
</details>

---

## 11. Practice Exercises

### Easy
1. Convert the lambda `s -> s.length()` to a method reference.
2. Convert the lambda `() -> new HashSet<>()` to a constructor reference.
3. Print all elements of a list using a method reference to `System.out.println`.

### Medium
4. Use a method reference with `Comparator.comparing` to sort a list of employees by salary.
5. Create a function that parses a list of numeric strings into an array of integers using `Integer::parseInt` and `Integer[]::new`.
6. Write a factory registry using constructor references for different shapes (`Circle::new`, `Square::new`).

### Hard
7. Implement an unbound method reference dispatcher that maps method names to `BiConsumer<Entity, Object>` setters.
8. Decompile and analyze the bytecode generated for bound vs unbound method references using `javap -v`.
9. Construct a dynamic plugin loader that instantiates components using constructor references with reflection tokens.

<details>
<summary>Solutions</summary>

```java
// Easy 1: String length
java.util.function.Function<String, Integer> lenRef = String::length;

// Easy 2: HashSet constructor
java.util.function.Supplier<java.util.Set<String>> setSup = java.util.HashSet::new;

// Medium 4: Sort employees
record Emp(String name, double salary) {}
List<Emp> emps = new ArrayList<>();
emps.sort(Comparator.comparingDouble(Emp::salary));

// Medium 5: Parse to array
List<String> raw = List.of("10", "20", "30");
Integer[] numbers = raw.stream().map(Integer::parseInt).toArray(Integer[]::new);
```
</details>

---


### Deep-Dive: Bytecode Differences Between Bound and Unbound Method References
Let us disassemble both kinds of method references with `javap -c`:

**Unbound (`String::length`):**
```
BootstrapMethods:
  0: #20 invokestatic LambdaMetafactory.metafactory:...
    Method arguments:
      (Ljava/lang/Object;)Ljava/lang/Object;
      invokevirtual java/lang/String.length:()I
      (Ljava/lang/String;)Ljava/lang/Integer;
```
The functional interface expects an input argument that matches the receiver class (`String`).

**Bound (`prefix::toUpperCase`):**
```
aload_1 // loads the 'prefix' instance from local variables
invokedynamic #3, 0 // InvokeDynamic #1:get:(Ljava/lang/String;)Ljava/util/function/Supplier;
```
Here, the `prefix` instance is captured as an argument into the `invokedynamic` call site!

### Detailed Exercise Solutions (All 9 Problems)
<details>
<summary>View Complete Solutions for All Exercises</summary>

```java
// Easy 1: String length reference
Function<String, Integer> lenRef = String::length;

// Easy 2: HashSet constructor
Supplier<Set<String>> hashSetSupplier = HashSet::new;

// Easy 3: System.out.println
List.of("A", "B").forEach(System.out::println);

// Medium 4: Sort employees by salary
record Employee(String name, double salary) {}
List<Employee> emps = new ArrayList<>();
emps.sort(Comparator.comparingDouble(Employee::salary));

// Medium 5: Parse numeric array
List<String> raw = List.of("10", "20", "30");
Integer[] parsed = raw.stream().map(Integer::parseInt).toArray(Integer[]::new);

// Medium 6: Factory Registry
interface Shape {}
class Circle implements Shape {}
class Square implements Shape {}
Map<String, Supplier<Shape>> factory = Map.of(
    "circle", Circle::new,
    "square", Square::new
);

// Hard 7: Unbound method dispatcher
public class Dispatcher<T> {
    private final Map<String, BiConsumer<T, Object>> setters = new HashMap<>();
    public void register(String property, BiConsumer<T, Object> setter) {
        setters.put(property, setter);
    }
    public void dispatch(T target, String prop, Object value) {
        var setter = setters.get(prop);
        if (setter != null) setter.accept(target, value);
    }
}

// Hard 8: Decompilation verification
// javap -c -p confirms direct MethodHandle linkage without synthetic forwarding bridge.

// Hard 9: Reflection token plugin loader
public static <T> T loadPlugin(Class<T> clazz) throws Exception {
    Constructor<T> ctor = clazz.getDeclaredConstructor();
    return ctor.newInstance();
}
```
</details>

## 12. Summary / Cheat Sheet

- **Static**: `ClassName::staticMethod` $\equiv$ `(args) -> ClassName.staticMethod(args)`.
- **Bound**: `instance::method` $\equiv$ `(args) -> instance.method(args)`.
- **Unbound**: `ClassName::method` $\equiv$ `(inst, args) -> inst.method(args)`.
- **Constructor**: `ClassName::new` $\equiv$ `(args) -> new ClassName(args)`.
- **Array Constructor**: `Type[]::new` $\equiv$ `(size) -> new Type[size]`.
- **Evaluation**: Bound receiver is evaluated eagerly at definition.

---

## 13. Further Reading
- Bloch, Joshua. *Effective Java (3rd Edition)*: Item 43 (Prefer method references to lambdas).
- Oracle Java Documentation: *Method References*.
- Goetz, Brian. *Design of JSR 335: Lambda Expressions and Method References*.
