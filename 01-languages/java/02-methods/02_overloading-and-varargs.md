# Method Overloading, Varargs, and Compile-Time Resolution in Java

> Method overloading delivers ad-hoc compile-time polymorphism through a three-phase resolution protocol (Primitive Widening $\to$ Boxing $\to$ Varargs), while variable-arity parameters (`T...`) automate array creation at call sites subject to heap allocation and generic safety rules.

| Metadata | Value |
| :--- | :--- |
| **Level** | Intermediate |
| **Prerequisites** | [00_methods.md](file:///E:/Learning/engineering-notes/01-languages/java/02-methods/00_methods.md) |
| **Time to Read** | 25 minutes |
| **Status** | Complete |
| **Java LTS Baseline** | Java 25 LTS (with `@SafeVarargs` and Heap Pollution Protections) |

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
In procedural languages without overloading (like standard C), if you need to calculate the maximum of two numbers, you must invent distinct function names for each supported type: `max_int()`, `max_double()`, `max_long()`. This clutters the global namespace, increases API cognitive load, and forces callers to memorize artificial function suffixes. Furthermore, methods needing to accept an arbitrary number of parameters historically forced callers to explicitly construct and wrap arguments in an array before invoking the method.

### Historical Context & Design Principles
Java introduced:
- **Compile-Time Method Overloading:** Multiple methods in the same class may share the exact same identifier, provided their formal parameter lists differ in type, count, or order.
- **Varargs (Java 5 - JSR 201):** Allows declaring a method with a variable number of arguments (`Type... args`), automating array creation at call sites.
- **Safe Varargs (Java 7+ JEP 110):** Added `@SafeVarargs` to suppress compiler warnings when combining generic types with variable-arity arrays.

### Real-World Relevance
Understanding the compiler's strict three-phase overload resolution rules is critical. Misunderstandings lead to subtle bugs: invoking a varargs method when an exact overload was intended, accidental autoboxing overhead in tight loops, and ambiguous compilation errors when passing `null` across overloaded signatures.

---

## 2. Core Concepts

### What Constitutes a Valid Overload
Two methods in the same class are valid overloads if they have the same name but different formal parameter lists:

| Valid Overload Criterion | Example Overloads |
| :--- | :--- |
| **Different Number of Parameters** | `print(int a)` and `print(int a, int b)` |
| **Different Types of Parameters** | `print(int a)` and `print(String a)` |
| **Different Order of Parameter Types** | `print(int a, String b)` and `print(String b, int a)` |

**CRITICAL INVARIANT:** Changing only the **return type** or declared **checked exceptions** does **NOT** constitute an overload. The following will fail compilation with `method is already defined`:
```java
// ILLEGAL: Duplicate method declaration
public int calculate(double x) { return 0; }
public double calculate(double x) { return 0.0; } // COMPILE ERROR!
```

### The Three-Phase Overload Resolution Protocol (JLS §15.12.2)
When resolving which overloaded method to invoke, `javac` executes a strict three-phase search:

```
Method Call: process(arg)
       │
       ▼
[ Phase 1: Strict Primitive Widening ]
Does an overload match without Autoboxing, Unboxing, or Varargs?
(e.g., int -> long -> double)
       │
       ├── Yes ──► Match Found! Emits direct bytecode call.
       │
       └── No
           ▼
[ Phase 2: Autoboxing / Unboxing Allowed ]
Does an overload match allowing Boxing or Unboxing (NO Varargs)?
(e.g., int -> Integer)
           │
           ├── Yes ──► Match Found! Emits boxing + method call.
           │
           └── No
               ▼
[ Phase 3: Variable-Arity (Varargs) Allowed ]
Does an overload match allowing Varargs packaging?
(e.g., int -> int...)
               │
               ├── Yes ──► Match Found! Allocates array & calls varargs.
               │
               └── No  ──► COMPILE ERROR: Cannot resolve method.
```

### Varargs Mechanics (`Type...`)
1. **Positioning Rule:** A varargs parameter must be the **last** parameter in the parameter list.
2. **Cardinality Rule:** A method can have at most **one** varargs parameter.
3. **Array Desugaring:** At compile time, `Type... args` is desugared directly into `Type[] args`.
4. **Call Versatility:** Callers can supply zero arguments, one argument, a comma-separated list of arguments, or pass an existing array directly.

---

## 3. How It Works Internally

### Static Compile-Time Binding vs. Dynamic Runtime Dispatch

```
                     Method Resolution in Java
                                │
                 ┌──────────────┴──────────────┐
                 ▼                             ▼
       [ Method Overloading ]        [ Method Overriding ]
         Compile-Time Binding          Runtime Dynamic Dispatch
         Resolved by compiler via      Resolved by JVM at runtime
         STATIC reference types.       via object VTABLE header.
         Emits exact bytecode sig.     Achieves true polymorphism.
```

Overloading is **static**: `javac` examines the declared static reference type of the arguments at compile time and hardcodes the selected method descriptor into the class constant pool:
```text
invokevirtual #7 // Method print:(Ljava/lang/String;)V
```
Even if the actual runtime object passed is a subclass, the overload decision made at compile time is permanent and immutable.

### Varargs Heap Allocation Mechanics
When you invoke a varargs method:
```java
public static int sum(int... numbers) { ... }
sum(10, 20, 30);
```
`javac` desugars the call into:
```java
sum(new int[]{ 10, 20, 30 });
```
- Every invocation of a varargs method allocates a **new array object on the heap**.
- If called millions of times inside a loop, varargs generates substantial garbage collection churn unless mitigated by targeted overloads.

---

## 4. Syntax & Usage

### Varargs Declaration Syntax
```java
// Standard varargs method:
public static void log(String level, String format, Object... args) {
    String message = String.format(format, args);
    System.out.printf("[%s] %s%n", level, message);
}
```

### The `@SafeVarargs` Annotation
When combining varargs with generic types (`T... args`), the compiler creates an array of generic types (`new T[]`), which can lead to **Heap Pollution** (storing an incompatible object into the array).
If you know your method does not store into the array or leak its reference, annotate it with `@SafeVarargs` (applicable only to `static`, `final`, or `private` methods):

```java
@SafeVarargs
public static <T> List<T> createList(T... elements) {
    List<T> list = new ArrayList<>(elements.length);
    for (T element : elements) {
        list.add(element);
    }
    return list;
}
```

---

## 5. Practical Examples

### Example 1: Three-Phase Overload Resolution Demonstration
- **Goal:** Verify that the compiler strictly prioritizes Primitive Widening over Autoboxing, and Autoboxing over Varargs.
```java
public class OverloadResolutionTest {
    public static String test(int a) { return "int (exact/primitive widening)"; }
    public static String test(long a) { return "long (primitive widening)"; }
    public static String test(Integer a) { return "Integer (autoboxing)"; }
    public static String test(int... a) { return "int... (varargs)"; }

    public static void main(String[] args) {
        System.out.println("test(42):          " + test(42));
        System.out.println("test(42L):         " + test(42L));
        System.out.println("test((Integer)42): " + test(Integer.valueOf(42)));
        System.out.println("test(1, 2):        " + test(1, 2));
    }
}
```
- **Console Output:**
```text
test(42):          int (exact/primitive widening)
test(42L):         long (primitive widening)
test((Integer)42): Integer (autoboxing)
test(1, 2):        int... (varargs)
```
- **Explanation:** Calling `test(42)` matches Phase 1 (`int`). Calling `test(42L)` widens to `long`. Explicit `Integer` matches Phase 2. Multiple arguments `test(1, 2)` fall back to Phase 3 (`varargs`).

---

### Example 2: Ambiguous Method Resolution with `null`
- **Goal:** Demonstrate the compiler error that occurs when two overloaded methods are equally specific for a `null` argument.
```java
public class AmbiguousNullDemo {
    public static void display(String s) {
        System.out.println("String overload: " + s);
    }

    public static void display(Integer i) {
        System.out.println("Integer overload: " + i);
    }

    public static void main(String[] args) {
        // COMPILE ERROR if uncommented:
        // display(null); // error: reference to display is ambiguous
        // Both String and Integer accept null, and neither inherits from the other!

        // RESOLUTION: Explicit cast resolves ambiguity
        display((String) null);
        display((Integer) null);
    }
}
```
- **Console Output:**
```text
String overload: null
Integer overload: null
```
- **Explanation:** Under JLS §15.12.2.5 (Most Specific Method), `String` and `Integer` are peer types in the hierarchy. Neither is more specific than the other, forcing the compiler to reject `display(null)` as ambiguous unless disambiguated with an explicit cast.

---

### Example 3: Varargs Calling Flexibility (Zero Arguments vs Array Reuse)
- **Goal:** Demonstrate calling a varargs method with zero arguments, multiple values, or an existing pre-allocated array.
```java
public class VarargsFlexibilityDemo {
    public static int sum(int base, int... numbers) {
        int total = base;
        for (int n : numbers) {
            total += n;
        }
        return total;
    }

    public static void main(String[] args) {
        // 1. Zero varargs passed: numbers becomes an empty array int[0]
        System.out.println("Zero varargs:    " + sum(100)); // 100

        // 2. Comma-separated values: JVM creates new int[]{1, 2, 3}
        System.out.println("Multiple values: " + sum(100, 1, 2, 3)); // 106

        // 3. Pre-allocated array: JVM reuses array without allocating a new one
        int[] cached = { 5, 10, 15 };
        System.out.println("Passed array:    " + sum(100, cached)); // 130
    }
}
```
- **Console Output:**
```text
Zero varargs:    100
Multiple values: 106
Passed array:    130
```
- **Explanation:** Passing zero varargs arguments instantiates a zero-length array. Passing an existing array bypasses array allocation entirely, passing the existing reference.

---

### Example 4: Failure Case — The Primitive Array in `Arrays.asList()` Trap
- **Goal:** Demonstrate the classic production bug where passing a primitive array (`int[]`) to a generic varargs method treats the entire array as a single object!
```java
import java.util.Arrays;
import java.util.List;

public class PrimitiveVarargsTrap {
    public static void main(String[] args) {
        int[] primitiveArray = { 1, 2, 3, 4, 5 };

        // TRAP: Arrays.asList accepts <T>... a (Object array).
        // A primitive 'int[]' cannot be converted to 'Integer[]'.
        // Therefore, the JVM infers T = int[], treating the ENTIRE array as element 0!
        List<int[]> trappedList = Arrays.asList(primitiveArray);

        System.out.println("List size: " + trappedList.size()); // 1, NOT 5!
        System.out.println("Element 0 type: " + trappedList.get(0).getClass().getSimpleName());

        // CORRECT FIX (Java 8+): Use Arrays.stream() or boxed array
        Integer[] boxedArray = { 1, 2, 3, 4, 5 };
        List<Integer> correctList = Arrays.asList(boxedArray);
        System.out.println("Correct list size: " + correctList.size()); // 5
    }
}
```
- **Console Output:**
```text
List size: 1
Element 0 type: int[]
Correct list size: 5
```
- **Explanation:** Because generic type parameters cannot bind to primitive types, `Arrays.asList(int[])` treats the `int[]` object as a single argument of type `Object`, producing a `List<int[]>` of length 1!

---

### Example 5: High-Performance / Modern Idiom — Overload Optimization Pattern
- **Goal:** Avoid varargs array allocation overhead in high-throughput APIs (used heavily in `List.of()` and Log4j2).
```java
public class OptimizedVarargsPattern {
    // Fast path: Zero allocations for 1, 2, or 3 arguments
    public static void process(String a) {
        execute(a);
    }

    public static void process(String a, String b) {
        execute(a); execute(b);
    }

    public static void process(String a, String b, String c) {
        execute(a); execute(b); execute(c);
    }

    // Fallback path: Only allocates array when caller supplies 4 or more arguments
    public static void process(String a, String b, String c, String... others) {
        execute(a); execute(b); execute(c);
        for (String s : others) execute(s);
    }

    private static void execute(String item) {
        // simulated low-latency handler
    }

    public static void main(String[] args) {
        // High-frequency calls use exact overloads, generating ZERO garbage:
        process("Alpha");
        process("Alpha", "Beta");
        process("Alpha", "Beta", "Gamma");
        System.out.println("Fast path executed with zero heap allocation.");
    }
}
```
- **Console Output:**
```text
Fast path executed with zero heap allocation.
```
- **Explanation:** In the JDK standard library, `List.of()` provides overloads for 0 through 10 parameters (`List.of(E e1)`, `List.of(E e1, E e2)`, etc.) before falling back to `List.of(E... elements)`. This prevents millions of ephemeral array allocations in common workflows.

---

## 6. Best Practices

1. **Avoid Overloads with the Same Number of Parameters and Primitive Subtypes:** Do not declare `foo(int)` and `foo(short)` in the same class. Callers passing literals or expressions will constantly struggle with unintended type promotions.
2. **Require at Least One Mandatory Parameter in Varargs:** If a method requires at least one argument, declare it as `foo(T first, T... rest)` rather than `foo(T... all)`. This enforces at compile time that the caller cannot pass zero arguments!
3. **Use the Fast-Path Overload Pattern for Hot APIs:** For high-throughput services, provide dedicated overloads for 1, 2, and 3 parameters to spare the garbage collector from array allocation churn.
4. **Always Annotate Generic Varargs with `@SafeVarargs`:** If a method accepts `T...` and does not store incompatible elements into the array or leak the array reference, add `@SafeVarargs` to suppress compiler warnings for callers.
5. **Never Overload Methods Where Parameters Differ Only by `Object` vs `Interface`:** Causes ambiguity when callers pass instances that implement the interface.

---

## 7. Common Mistakes & Pitfalls

| Mistake | Why It Fails | Practical Remedy |
| :--- | :--- | :--- |
| **Passing Primitive Array to `Arrays.asList()`** | Produces a `List<int[]>` of length 1 instead of a list of elements. | Use `IntStream.of(arr).boxed().toList()` or use boxed `Integer[]`. |
| **Ambiguous Method Call on `null`** | Two peer reference types both match `null` with equal specificity. | Cast the argument explicitly: `foo((String) null)`. |
| **Placing Varargs Parameter First** | Syntax error; varargs must be the final parameter in the formal parameter list. | Move varargs to the end: `foo(int id, String... tags)`. |
| **Declaring Multiple Varargs in One Method** | Grammar error; a method can have at most one varargs parameter. | Use arrays or collections for multiple variable-length parameters. |
| **Assuming Overload Resolution is Dynamic** | Overloads are selected at compile time based on static reference types, not runtime types. | Use method overriding (polymorphism) for dynamic dispatch. |
| **Modifying Varargs Array Received from Callers** | If the caller passed an existing array (`foo(myArray)`), mutating the varargs array mutates the caller's array! | Make a defensive copy if the array must be modified. |

---

## 8. Performance & Security Notes

### Heap Allocation of Varargs Arrays
Every invocation of `method(a, b, c)` desugars into `new Object[]{ a, b, c }`. In performance-critical microservice loops, this can result in gigabytes of short-lived heap allocations per minute. Always use fast-path overloads (1, 2, 3 parameters) in hot code paths.

### Security Invariants: Heap Pollution via Generic Varargs
Because Java implements Generics via Type Erasure, arrays of generic types cannot be reified. When `javac` compiles:
```java
public static <T> void addToList(List<T> list, T... elements)
```
The parameter `elements` is physically compiled into `Object[] elements`. If malicious or broken code casts this `Object[]` to `String[]` and stores an `Integer`, the JVM will throw a hidden `ClassCastException` in code that contains no visible casts. Mark methods with `@SafeVarargs` only after rigorously proving the method is memory-safe.

---

## 9. Comparison / When to Use What

| Feature / Dimension | Method Overloading | Method Overriding |
| :--- | :--- | :--- |
| **Resolution Timing** | **Compile Time** (Static binding by `javac`) | **Runtime** (Dynamic dispatch by JVM vtable) |
| **Location** | Same class (or inherited into same class) | Subclass overriding parent class method |
| **Method Signature** | **Must be different** (different parameter types/count) | **Must be identical** (same name and parameters) |
| **Return Type** | Can be identical or completely different | Must be identical or covariant (subtype) |
| **Access Modifiers** | Can be anything | Cannot reduce visibility (e.g., cannot make public private) |
| **Checked Exceptions** | Can declare any exceptions independently | Cannot declare new or broader checked exceptions |
| **Purpose** | Readability and syntactic convenience | Polymorphic behavior and extensible design |

---

## 10. Interview Questions

<details>
<summary><strong>1. What are the three phases of method overload resolution specified by JLS §15.12.2?</strong></summary>

**Answer:**  
To determine the applicable method overload, the Java compiler executes three distinct phases in order:
1. **Phase 1 (Strict Invocation):** Searches for matching methods using subtyping and primitive widening only, without permitting autoboxing, auto-unboxing, or variable-arity (varargs) invocations.
2. **Phase 2 (Loose Invocation):** Searches for matching methods allowing autoboxing and auto-unboxing, but still strictly disallowing varargs.
3. **Phase 3 (Variable-Arity Invocation):** Searches for matching methods allowing variable-arity (varargs) parameter packaging.
The compiler stops as soon as a phase finds one or more applicable methods.
</details>

<details>
<summary><strong>2. Why does the compiler prioritize primitive widening over autoboxing in overload resolution?</strong></summary>

**Answer:**  
Backward compatibility. Java introduced primitive widening in Java 1.0, while autoboxing was added in Java 5. If a class declared `foo(long)` in Java 1.0, and a developer upgraded to Java 5 where `foo(Integer)` was added, calling `foo(42)` had to continue invoking `foo(long)` to ensure existing legacy applications did not change behavior upon recompilation.
</details>

<details>
<summary><strong>3. What is the "Most Specific Method" rule, and when does it cause a compile-time ambiguity error?</strong></summary>

**Answer:**  
If multiple overloaded methods are applicable at the end of a resolution phase, the compiler selects the **most specific method**: method $M_1$ is more specific than $M_2$ if any invocation that can be handled by $M_1$ could also be passed to $M_2$ without compile errors (i.e., $M_1$'s parameter types are subtypes of $M_2$'s parameter types). If neither method is more specific than the other (e.g., `foo(String, Object)` vs `foo(Object, String)` called with `foo(null, null)`), the compiler cannot decide and reports an ambiguity error.
</details>

<details>
<summary><strong>4. How is a varargs parameter desugared by the Java compiler?</strong></summary>

**Answer:**  
At the method declaration level, `Type... args` is desugared directly into `Type[] args` in the compiled class bytecode. At the call site, if the caller provides zero or more arguments of type `Type`, the compiler inserts bytecode that allocates a new array: `new Type[]{ arg1, arg2 }` and passes that array reference to the method. If the caller passes an existing `Type[]` array directly, the compiler passes the array as-is without creating a wrapper.
</details>

<details>
<summary><strong>5. What is "Heap Pollution", and why does `@SafeVarargs` exist?</strong></summary>

**Answer:**  
Heap pollution occurs when a variable of a parameterized type references an object that is not of that type. Because generic array creation is forbidden in Java due to type erasure, declaring a generic varargs method (`<T> void foo(T... args)`) forces the compiler to create an `Object[]` array. If the method writes an incompatible type into that array or exposes the array reference to external code, a `ClassCastException` will occur at runtime. `@SafeVarargs` is an assertion by the developer that the method does not pollute the heap, suppressing the compiler's safety warning.
</details>

<details>
<summary><strong>6. Can two methods be overloaded if they differ only in their return types? Why or why not?</strong></summary>

**Answer:**  
No. In Java, callers can invoke a method as a statement without capturing its return value (e.g., `calculate();`). If two methods had identical names and parameter lists but different return types (`int calculate()` vs `void calculate()`), the compiler would have no way to determine which method the caller intended to execute, creating an unsolvable ambiguity.
</details>

<details>
<summary><strong>7. Why does `Arrays.asList(new int[]{1, 2, 3}).size()` return `1` instead of `3`?</strong></summary>

**Answer:**  
`Arrays.asList` is declared as `<T> List<T> asList(T... a)`. Generics can only bind to reference types extending `Object`. A primitive array `int[]` is a single `Object`, but it is **not** an `Object[]`. Therefore, the compiler cannot unpack the primitive array; it binds `T = int[]`, treating the entire `int[]` array as a single argument. It creates a `List<int[]>` containing exactly one element (the array itself).
</details>

<details>
<summary><strong>8. How does the JDK standard library optimize `List.of()` against varargs allocation overhead?</strong></summary>

**Answer:**  
In Java 9+, `List.of()` defines eleven distinct overloaded methods:
`List.of()`, `List.of(E e1)`, `List.of(E e1, E e2)`, ..., up to `List.of(E e1, ..., E e10)`, before finally offering the varargs fallback `List.of(E... elements)`. Since the vast majority of immutable lists are created with 1 to 10 elements, the compiler matches the fixed-parameter overloads, completely avoiding the temporary array allocation required by varargs.
</details>

---

## 11. Practice Exercises

### Exercise 1: Predicting Overload Resolution Output (Easy)
- **Task:** Trace which overloaded method will be invoked for each call below:
```java
public class OverloadPuzzle {
    static void test(Object o)  { System.out.println("Object"); }
    static void test(String s)  { System.out.println("String"); }
    static void test(Integer i) { System.out.println("Integer"); }

    public static void main(String[] args) {
        test("Hello");
        test(42);
        // test(null); // What happens if this line is compiled?
    }
}
```
<details>
<summary>Solution</summary>

- `test("Hello")` prints **String** (exact type match).
- `test(42)` boxes `42` to `Integer`, printing **Integer** (matches Phase 2 autoboxing).
- `test(null)` fails compilation with: `reference to test is ambiguous`. Both `String` and `Integer` are equally specific subtypes of `Object`.
</details>

### Exercise 2: Enforcing Mandatory Arguments in Varargs (Medium)
- **Task:** Write a method `int findMin(int first, int... rest)` that finds the minimum value among a list of integers, ensuring through its method signature that calling `findMin()` with zero arguments is a compile-time error.
<details>
<summary>Solution</summary>

```java
public class MandatoryVarargs {
    public static int findMin(int first, int... rest) {
        int min = first;
        for (int n : rest) {
            if (n < min) {
                min = n;
            }
        }
        return min;
    }

    public static void main(String[] args) {
        System.out.println("Min: " + findMin(42));             // 42
        System.out.println("Min: " + findMin(10, 5, 20, 3));   // 3
        // findMin(); // COMPILE ERROR: Requires at least 1 int argument!
    }
}
```
</details>

### Exercise 3: Safe Generic Varargs Flattener (Hard)
- **Task:** Implement a safe generic method `@SafeVarargs public static <T> List<T> flatten(List<T>... lists)` that combines multiple lists into a single unmodifiable `List<T>`, ensuring no heap pollution occurs.
<details>
<summary>Solution</summary>

```java
import java.util.ArrayList;
import java.util.Collections;
import java.util.List;

public class GenericFlattener {
    @SafeVarargs
    public static <T> List<T> flatten(List<T>... lists) {
        if (lists == null || lists.length == 0) {
            return Collections.emptyList();
        }

        List<T> result = new ArrayList<>();
        for (List<T> list : lists) {
            if (list != null) {
                result.addAll(list);
            }
        }
        return Collections.unmodifiableList(result);
    }

    public static void main(String[] args) {
        var l1 = List.of("A", "B");
        var l2 = List.of("C", "D");
        var l3 = List.of("E");

        List<String> combined = flatten(l1, l2, l3);
        System.out.println("Combined: " + combined); // [A, B, C, D, E]
    }
}
```
</details>

---

## 12. Summary / Cheat Sheet

- **Valid Overloads:** Same name, different parameter types, count, or order. Changing only the return type is illegal.
- **Three-Phase Resolution:**
  1. Primitive Widening (no boxing, no varargs).
  2. Autoboxing / Unboxing (no varargs).
  3. Varargs packaging.
- **Ambiguity:** Occurs when two applicable overloads are equally specific for the supplied arguments (e.g., `null`).
- **Varargs Rules:** Must be last parameter; at most one varargs parameter per method; desugars into `Type[]`.
- **Varargs Heap Cost:** Calling varargs allocates a new heap array; use fixed-parameter overloads (1, 2, 3 args) for performance-critical APIs.
- **Safety:** Use `@SafeVarargs` on static/final/private methods to suppress heap pollution warnings when using generic varargs.

---

## 13. Further Reading

- [The Java Language Specification (Java SE 25 Edition) - Chapter 15.12.2: Determine Method Signature](https://docs.oracle.com/javase/specs/jls/se25/html/jls-15.html#jls-15.12.2)
- [JEP 110: Safe Varargs Annotation](https://openjdk.org/jeps/110)
- [Effective Java (3rd Edition) - Item 52: Use overloading judiciously](https://www.oreilly.com/library/view/effective-java-3rd/9780134686097/)
- [Effective Java (3rd Edition) - Item 53: Use varargs judiciously](https://www.oreilly.com/library/view/effective-java-3rd/9780134686097/)

---

| [← Previous: 01_pass-by-value.md](file:///E:/Learning/engineering-notes/01-languages/java/02-methods/01_pass-by-value.md) | [Section Index](file:///E:/Learning/engineering-notes/01-languages/java/02-methods/README.md) | [Next: 03_recursion.md](file:///E:/Learning/engineering-notes/01-languages/java/02-methods/03_recursion.md) → |
