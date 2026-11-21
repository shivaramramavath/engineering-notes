# Wrapper Classes, Autoboxing, and Memory Internals in Java

> Wrapper classes bridge Java's high-performance primitives with its object-oriented type system, enabling generic collections, nullable entity fields, and utility transformations, while autoboxing automates bidirectional conversions through cached factory methods and explicit bytecode instructions.

| Metadata | Value |
| :--- | :--- |
| **Level** | Beginner to Intermediate |
| **Prerequisites** | [01_variables-and-data-types.md](file:///E:/Learning/engineering-notes/01-languages/java/01-fundamentals/01_variables-and-data-types.md) |
| **Time to Read** | 25 minutes |
| **Status** | Complete |
| **Java LTS Baseline** | Java 25 LTS (Value-based classes & Constructor Deprecation) |

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
Java was architected with a dual type system: primitives for raw execution performance, and objects for encapsulation and polymorphism. However, core language features—including **Generics** (`List<T>`, `Map<K, V>`), reflection, and database ORMs—only operate on reference types extending `java.lang.Object`. Furthermore, primitives cannot represent missing or uninitialized states (`null`). To bridge this divide, the language requires object wrappers capable of encapsulating raw primitive values.

### Historical Context & Autoboxing
- **Java 1.0 to 1.4:** Developers had to manually box and unbox primitives:
  `Integer boxed = new Integer(42); int val = boxed.intValue();`
  This created repetitive boilerplate throughout data access layers and collection manipulations.
- **Java 5 (JSR 201):** Introduced **Autoboxing and Auto-unboxing**, allowing the compiler to insert conversion methods automatically.
- **Java 9+ Deprecation:** Constructors like `new Integer(x)` were deprecated for removal (JEP 390). In modern Java, wrappers are designated as **Value-Based Classes**, preparing the ecosystem for Project Valhalla. Factory methods like `Integer.valueOf(x)` are mandatory.

### Real-World Relevance
While autoboxing provides immense syntactic convenience, naive usage in production causes severe performance degradation: hidden heap allocations that saturate the young generation garbage collector, unexpected `NullPointerException` crashes during unboxing, and critical bugs caused by identity comparison (`==`) over cached integer ranges.

---

## 2. Core Concepts

### The 8 Wrapper Classes

| Primitive Type | Wrapper Class | Superclass | Object Size (Heap) | Cached Range |
| :--- | :--- | :--- | :---: | :--- |
| `byte` | `java.lang.Byte` | `java.lang.Number` | 16 bytes | $-128$ to $127$ (All values) |
| `short` | `java.lang.Short` | `java.lang.Number` | 16 bytes | $-128$ to $127$ |
| `int` | `java.lang.Integer` | `java.lang.Number` | 16 bytes | $-128$ to $127$ (Configurable) |
| `long` | `java.lang.Long` | `java.lang.Number` | 24 bytes | $-128$ to $127$ |
| `float` | `java.lang.Float` | `java.lang.Number` | 16 bytes | None (No cache) |
| `double` | `java.lang.Double` | `java.lang.Number` | 24 bytes | None (No cache) |
| `char` | `java.lang.Character` | `java.lang.Object` | 16 bytes | `\u0000` to `\u007f` ($0$ to $127$) |
| `boolean`| `java.lang.Boolean` | `java.lang.Object` | 16 bytes | `Boolean.TRUE`, `Boolean.FALSE` |

### Core Properties of Wrapper Classes
1. **Immutability:** All wrapper objects are completely immutable. Once created, the underlying primitive field cannot be modified.
2. **Value-Based Classes:** Modern Java treats wrappers as value-based: instances have no identity guarantee, and synchronizing on a wrapper object (e.g., `synchronized(myInteger)`) triggers compile-time warnings and runtime degradation.
3. **Nullability:** Unlike primitives (which default to `0` or `false`), wrappers can hold `null`, making them suitable for optional JSON payloads, SQL NULL columns, and sparse data structures.

---

## 3. How It Works Internally

### Bytecode Deconstruction of Autoboxing
Autoboxing is syntactic sugar managed entirely by `javac`:

```java
Integer boxed = 100;    // Autoboxing
int unboxed = boxed;    // Auto-unboxing
```
When compiled, `javac` desugars this code into:
```text
0: bipush        100
2: invokestatic  #7   // Method java/lang/Integer.valueOf:(I)Ljava/lang/Integer;
5: astore_1
6: aload_1
7: invokevirtual #13  // Method java/lang/Integer.intValue:()I
10: istore_2
```
- **Autoboxing** maps to `static valueOf(primitive)`.
- **Auto-unboxing** maps to instance `<primitive>Value()`.

### The Integer Cache (`IntegerCache`)
To avoid heap allocation churn, HotSpot initializes an internal static array (`Integer.IntegerCache.cache`) upon class loading:

```java
public static Integer valueOf(int i) {
    if (i >= IntegerCache.low && i <= IntegerCache.high)
        return IntegerCache.cache[i + (-IntegerCache.low)];
    return new Integer(i);
}
```
- By default, `IntegerCache.low = -128` and `IntegerCache.high = 127`.
- Values within this range return the exact same canonical flyweight object reference from the cache.
- Values outside this range instantiate a brand new object on the heap.
- The upper bound can be tuned in production via JVM flag: `-XX:AutoBoxCacheMax=<size>`.

### Memory Footprint Breakdown
On a 64-bit JVM with Compressed OOPs:
- **Primitive `int`:** 4 bytes on stack or inlined in an object.
- **Boxed `Integer` Object:**
  - 8-byte Mark Word (locking, GC age, hashcode)
  - 4-byte Klass Pointer (pointer to `java.lang.Integer.class` in Metaspace)
  - 4-byte primitive `int` value field
  - Total = **16 bytes on heap**, plus a 4-byte reference pointer on the stack (total **20 bytes** vs 4 bytes).

---

## 4. Syntax & Usage

### Creating Wrapper Instances (Factory Methods)
```java
// RECOMMENDED (Modern): Uses cache where possible
Integer a = Integer.valueOf(100);
Double d = Double.valueOf(3.14);

// ANTI-PATTERN: Deprecated since Java 9, for removal
// Integer legacy = new Integer(100); // WARNING: 'Integer(int)' is deprecated

// Auto-boxing:
Integer count = 50; // Invocates Integer.valueOf(50)
```

### Parsing Strings to Primitives vs. Wrappers
Each wrapper provides two distinct parsing methods:
```java
// 1. Returns primitive (zero heap allocation):
int primitiveInt = Integer.parseInt("12345");
double primitiveDouble = Double.parseDouble("3.14");

// 2. Returns boxed wrapper (triggers valueOf cache lookup):
Integer boxedInt = Integer.valueOf("12345");
Double boxedDouble = Double.valueOf("3.14");
```

---

## 5. Practical Examples

### Example 1: The Integer Cache Equality Trap
- **Goal:** Demonstrate why reference equality (`==`) produces erratic results with boxed numbers.
```java
public class IntegerCacheDemo {
    public static void main(String[] args) {
        Integer a1 = 127, b1 = 127;
        Integer a2 = 128, b2 = 128;

        System.out.println("127 == 127: " + (a1 == b1)); // true (Cached flyweight)
        System.out.println("128 == 128: " + (a2 == b2)); // false (Distinct heap allocations!)
        System.out.println("128.equals(128): " + a2.equals(b2)); // true (Value equality)
    }
}
```
- **Console Output:**
```text
127 == 127: true
128 == 128: false
128.equals(128): true
```
- **Explanation:** `127` falls within the $[-128, 127]$ cache, so `a1` and `b1` share the exact same memory address. `128` exceeds the cache boundary, so `a2` and `b2` point to separate objects on the heap. Never use `==` to compare boxed numbers; always use `.equals()`.

---

### Example 2: Hidden Autoboxing in Loops (GC Saturation)
- **Goal:** Measure the catastrophic performance penalty of autoboxing in tight mathematical loops.
```java
public class AutoboxingLoopBenchmark {
    public static void main(String[] args) {
        // ANTI-PATTERN: Accidental boxed accumulator creates 10,000,000 heap objects!
        long start = System.nanoTime();
        Long boxedSum = 0L;
        for (long i = 0; i < 10_000_000; i++) {
            boxedSum += i; // Unboxes to long, adds i, boxes result via Long.valueOf()!
        }
        long boxedDuration = (System.nanoTime() - start) / 1_000_000;

        // IDIOMATIC: Primitive accumulator stays on CPU register
        start = System.nanoTime();
        long primitiveSum = 0L;
        for (long i = 0; i < 10_000_000; i++) {
            primitiveSum += i;
        }
        long primitiveDuration = (System.nanoTime() - start) / 1_000_000;

        System.out.println("Boxed Long Loop Duration:     " + boxedDuration + " ms (Sum: " + boxedSum + ")");
        System.out.println("Primitive long Loop Duration: " + primitiveDuration + " ms (Sum: " + primitiveSum + ")");
        System.out.printf("Performance Degradation:      %.1fx slower%n", 
            (double) boxedDuration / Math.max(1, primitiveDuration));
    }
}
```
- **Console Output:**
```text
Boxed Long Loop Duration:     64 ms (Sum: 49999995000000)
Primitive long Loop Duration: 4 ms (Sum: 49999995000000)
Performance Degradation:      16.0x slower
```
- **Explanation:** In the boxed loop, each addition creates an ephemeral `java.lang.Long` object on the heap ($24$ bytes), allocating over 240 MB of garbage and triggering young-generation GC pauses.

---

### Example 3: Edge Case — The Unboxing `NullPointerException`
- **Goal:** Demonstrate auto-unboxing failure when evaluating nullable wrapper fields.
```java
public class UnboxingNpeDemo {
    record Order(String id, Integer discountPercent) {}

    public static void main(String[] args) {
        Order order = new Order("ORD-101", null); // Discount is optional

        try {
            // DANGER: Auto-unboxing order.discountPercent() invokes intValue() on null!
            if (order.discountPercent() > 0) {
                System.out.println("Discount applied");
            }
        } catch (NullPointerException e) {
            System.out.println("CAUGHT NPE: Unboxing a null wrapper always throws NullPointerException!");
        }

        // SAFE DEFENSE: Check for null before evaluating
        if (order.discountPercent() != null && order.discountPercent() > 0) {
            System.out.println("Discount applied safely");
        } else {
            System.out.println("No discount applicable");
        }
    }
}
```
- **Console Output:**
```text
CAUGHT NPE: Unboxing a null wrapper always throws NullPointerException!
No discount applicable
```
- **Explanation:** Whenever a wrapper is unboxed implicitly (e.g., in a relational `<` comparison or arithmetic operator), the compiler injects `.intValue()`. If the reference is `null`, it throws `NullPointerException`.

---

### Example 4: Failure Case — `List.remove()` Overload Ambiguity
- **Goal:** Demonstrate the dangerous collision between `List.remove(int index)` and `List.remove(Object o)` when working with `List<Integer>`.
```java
import java.util.ArrayList;
import java.util.List;

public class ListRemoveTrap {
    public static void main(String[] args) {
        List<Integer> numbers = new ArrayList<>(List.of(10, 20, 30));

        // Intention: Remove the element '20'
        // TRAP: Passes primitive int '1' -> Invokes remove(int index)!
        numbers.remove(1); // Removes element at index 1 (which is 20)
        System.out.println("After remove(1): " + numbers);

        // Intention: Remove the element '10'
        // FIX: Must pass Integer object to target remove(Object o)!
        numbers.remove(Integer.valueOf(10)); // Removes element 10
        System.out.println("After remove(Integer.valueOf(10)): " + numbers);
    }
}
```
- **Console Output:**
```text
After remove(1): [10, 30]
After remove(Integer.valueOf(10)): [30]
```
- **Explanation:** `List` declares two overloads: `remove(int index)` and `remove(Object o)`. Passing a primitive integer calls the index-based removal. To remove by value, you must explicitly box the argument via `Integer.valueOf()`.

---

### Example 5: High-Performance / Modern Idiom — Primitive Streams
- **Goal:** Process large numeric sequences using specialized primitive streams (`IntStream`, `LongStream`) to completely eliminate autoboxing overhead.
```java
import java.util.stream.IntStream;

public class PrimitiveStreamDemo {
    public static void main(String[] args) {
        long start = System.nanoTime();
        // Zero boxing overhead: IntStream operates directly on primitive int
        long sum = IntStream.rangeClosed(1, 1_000_000)
                            .filter(n -> n % 2 == 0)
                            .mapToLong(n -> (long) n * 2)
                            .sum();
        long duration = (System.nanoTime() - start) / 1_000;

        System.out.println("Primitive Stream Sum: " + sum + " (Elapsed: " + duration + " µs)");
    }
}
```
- **Console Output:**
```text
Primitive Stream Sum: 1000002000000 (Elapsed: 4210 µs)
```
- **Explanation:** `IntStream` avoids `Stream<Integer>`, maintaining values directly on CPU registers and primitive buffers, delivering massive throughput advantages.

---

## 6. Best Practices

1. **Always Use `.equals()` to Compare Wrappers:** Never use `==` on wrapper objects. Even though `==` works for small numbers due to `IntegerCache`, it fails silently in production when values exceed $127$.
2. **Use Primitive Types for Local Variables and Loops:** Only use wrappers when required by generic collections (`List<Integer>`) or nullable domain fields. Keep method locals and loop accumulators primitive.
3. **Use `parseInt()` Over `valueOf()` When Returning Primitives:**
   ```java
   int x = Integer.parseInt(str);  // GOOD: No boxing
   int y = Integer.valueOf(str);   // BAD: Boxes to Integer, then unboxes to int
   ```
4. **Never Synchronize on Wrapper Objects:** Wrapper classes are Value-Based. In modern Java, synchronizing on an `Integer` or `Boolean` instance produces compiler warnings and can cause deadlocks due to shared cached instances.
5. **Guard Against Null Before Unboxing:** If an entity field is a wrapper (e.g., `Integer age`), check for `null` before evaluating it in arithmetic expressions or passing it to methods expecting primitives.

---

## 7. Common Mistakes & Pitfalls

| Mistake | Why It Fails | Practical Remedy |
| :--- | :--- | :--- |
| **Comparing Wrappers with `==`** | Passes for values $-128$ to $127$ (cached), fails for $128+$ (different heap references). | Always use `.equals()` or compare after unboxing. |
| **Accumulating in Boxed Types in Loops** | Causes millions of heap allocations, saturating the GC. | Use primitive `int` or `long` for accumulators. |
| **Unboxing a Null Wrapper** | Invokes `.intValue()` on `null`, throwing `NullPointerException`. | Add explicit `!= null` guard before unboxing. |
| **Calling `List.remove(int)` by Value** | Invokes `remove(int index)` instead of `remove(Object element)`. | Wrap the value: `list.remove(Integer.valueOf(val))`. |
| **Using `new Integer()` Constructor** | Deprecated for removal; bypasses `IntegerCache` and wastes heap RAM. | Use `Integer.valueOf()` or autoboxing. |
| **Ternary Operator Auto-Unboxing Crash** | Conditional expression with mixed primitive/wrapper unboxes null branch. | Ensure both branches return matching wrapper types. |

---

## 8. Performance & Security Notes

### Value-Based Classes & Project Valhalla
The OpenJDK community has formally designated wrapper classes as **Value-Based Classes** (JEP 390). In future Java releases under Project Valhalla, the JVM will migrate wrappers to primitive value types that have no object headers and zero pointer indirection. To remain forward-compatible:
- Do not call `new Integer()`.
- Do not use `==` for identity checks on wrappers.
- Do not use wrappers as monitor locks (`synchronized (num)`).

### Memory Inflation in Microservices
In microservices handling millions of JSON requests:
- Storing 1,000,000 IDs as `List<Long>` requires $\approx 32 \text{ MB}$ of heap.
- Storing them in a primitive `long[]` array requires only $8 \text{ MB}$.
For high-scale memory-sensitive caches, prefer primitive collections (Eclipse Collections, FastUtil, Agrona).

---

## 9. Comparison / When to Use What

| Dimension | Primitive Type (`int`) | Wrapper Class (`Integer`) |
| :--- | :--- | :--- |
| **Storage Location** | Stack frame local or inlined in object heap | Separate heap object with 16-byte header |
| **Memory Footprint** | Exactly 4 bytes | 16 bytes heap + 4-byte reference (20 bytes total) |
| **Nullability** | Never null (defaults to `0`) | Can be `null` (represents absence) |
| **Generic Collections** | Not allowed directly (`List<int>` illegal) | Supported (`List<Integer>`) |
| **Performance** | Direct CPU ALU operations | Pointer dereference + method call unboxing |
| **Equality Check** | `==` compares raw values | `.equals()` compares values; `==` compares addresses |

---

## 10. Interview Questions

<details>
<summary><strong>1. What is the Integer Cache, what is its default range, and how can it be configured?</strong></summary>

**Answer:**  
The `IntegerCache` is a private static flyweight cache inside `java.lang.Integer`. When `Integer.valueOf(int)` is called (explicitly or via autoboxing), the method checks if the value is within the cached range. If so, it returns an existing pre-allocated instance from an array, avoiding heap allocation. The default range is $-128$ to $127$. The lower bound is fixed at $-128$, but the upper bound can be tuned via the JVM command-line flag `-XX:AutoBoxCacheMax=<size>`.
</details>

<details>
<summary><strong>2. Why was `new Integer(int)` deprecated in Java 9 and marked for removal?</strong></summary>

**Answer:**  
Calling `new Integer(100)` explicitly forces the JVM to allocate a brand new object on the heap, bypassing the `IntegerCache` flyweight mechanism. This wastes heap memory and degrades GC performance. Furthermore, deprecating constructors is a mandatory step for Project Valhalla, which intends to convert wrapper classes into true identity-free primitive value classes.
</details>

<details>
<summary><strong>3. How does the compiler transform autoboxing and auto-unboxing into bytecode?</strong></summary>

**Answer:**  
Autoboxing is transformed into a call to the static factory method `valueOf`:
`Integer a = 10;` $\to$ `Integer a = Integer.valueOf(10);` (`invokestatic`).
Auto-unboxing is transformed into a call to the corresponding instance accessor method:
`int b = a;` $\to$ `int b = a.intValue();` (`invokevirtual`).
</details>

<details>
<summary><strong>4. What happens when you execute `Boolean b1 = true; Boolean b2 = true; System.out.println(b1 == b2);`?</strong></summary>

**Answer:**  
It prints `true`. The `Boolean` class defines exactly two static immutable instances: `Boolean.TRUE` and `Boolean.FALSE`. Autoboxing `true` maps directly to `Boolean.valueOf(true)`, which returns the singleton `Boolean.TRUE`. Because both references point to the exact same static singleton in memory, `b1 == b2` evaluates to `true`.
</details>

<details>
<summary><strong>5. Why does `List.remove(1)` behave differently for `List<Integer>` than `List<String>`?</strong></summary>

**Answer:**  
The `List` interface defines two overloaded methods: `E remove(int index)` and `boolean remove(Object o)`. When working with `List<String>`, calling `list.remove("1")` matches `remove(Object o)` while `list.remove(1)` matches `remove(int index)`. However, for `List<Integer>`, passing a primitive `1` binds to `remove(int index)` by Java's overload resolution rules (primitives match primitive parameters without boxing). To remove the element with value `1`, you must pass an object: `list.remove(Integer.valueOf(1))`.
</details>

<details>
<summary><strong>6. What is the performance danger of using wrapper types in Java Streams?</strong></summary>

**Answer:**  
Standard `Stream<T>` operates on objects. Using `Stream<Integer>` to process a million numbers forces every element through boxing on collection entry and unboxing during arithmetic reductions. This generates millions of short-lived heap objects, triggering GC pressure. Specialized primitive streams (`IntStream`, `LongStream`, `DoubleStream`) use primitive array buffers and specialized primitive consumers, eliminating boxing overhead entirely.
</details>

<details>
<summary><strong>7. What are the rules defining a "Value-Based Class" in the Java specification?</strong></summary>

**Answer:**  
Value-based classes (such as `Integer`, `LocalDate`, `Optional`):
1. Are immutable and final.
2. Implement `equals()`, `hashCode()`, and `toString()` based solely on state, not identity.
3. Do not provide public constructors; instances are created only via factory methods.
4. Have no accessible identity (calling `System.identityHashCode()` or synchronizing on instances is discouraged or produces warnings).
5. Are freely substitutable: two instances with equal values are interchangeable.
</details>

<details>
<summary><strong>8. Why does `Double.valueOf(1.0) == Double.valueOf(1.0)` evaluate to `false`?</strong></summary>

**Answer:**  
Unlike `Byte`, `Short`, `Integer`, `Long`, and `Character`, the floating-point wrapper classes `Float` and `Double` **do not maintain a flyweight cache**. Because floating-point numbers represent an infinite continuum of values, caching a small subset is rarely useful. Every call to `Double.valueOf(1.0)` allocates a new `Double` object on the heap, so `==` compares two different memory addresses and evaluates to `false`.
</details>

---

## 11. Practice Exercises

### Exercise 1: Cache Boundary Prediction (Easy)
- **Task:** Predict the console output of the following comparisons:
```java
Short s1 = 100, s2 = 100;
Short s3 = 200, s4 = 200;
Character c1 = 'A', c2 = 'A';
Character c3 = '\u0100', c4 = '\u0100'; // Unicode 256
System.out.println(s1 == s2);
System.out.println(s3 == s4);
System.out.println(c1 == c2);
System.out.println(c3 == c4);
```
<details>
<summary>Solution</summary>

- `s1 == s2`: `true` ($100$ is in ShortCache $[-128, 127]$).
- `s3 == s4`: `false` ($200$ is outside ShortCache).
- `c1 == c2`: `true` (`'A'` is code point 65, in CharacterCache $[0, 127]$).
- `c3 == c4`: `false` (Unicode 256 is outside CharacterCache).
</details>

### Exercise 2: Null-Safe Unboxing Calculation (Medium)
- **Task:** Implement a method `int sumNullableIntegers(List<Integer> numbers)` that computes the sum of a list of boxed integers, treating `null` elements as `0` without throwing `NullPointerException`.
<details>
<summary>Solution</summary>

```java
import java.util.List;

public class NullSafeSum {
    public static int sumNullableIntegers(List<Integer> numbers) {
        if (numbers == null) return 0;
        int total = 0;
        for (Integer num : numbers) {
            // Guard against null before unboxing
            if (num != null) {
                total += num; // Safe auto-unboxing
            }
        }
        return total;
    }

    public static void main(String[] args) {
        var list = java.util.Arrays.asList(10, null, 25, null, 5);
        System.out.println("Sum: " + sumNullableIntegers(list)); // Output: 40
    }
}
```
</details>

### Exercise 3: Safe Collection Value Remover (Hard)
- **Task:** Write a generic utility method `<E> void removeFirstOccurrenceByValue(List<E> list, E targetValue)` that guarantees value-based removal from a `List` even when instantiated with `List<Integer>`, ensuring the index-based `remove(int)` overload is never mistakenly called.
<details>
<summary>Solution</summary>

```java
import java.util.ArrayList;
import java.util.List;

public class SafeRemover {
    public static <E> boolean removeFirstOccurrenceByValue(List<E> list, E targetValue) {
        if (list == null || targetValue == null) return false;
        // Casting targetValue to Object explicitly invokes remove(Object o)
        return list.remove((Object) targetValue);
    }

    public static void main(String[] args) {
        List<Integer> list = new ArrayList<>(List.of(5, 10, 15, 20));

        // Attempting to remove value 10:
        boolean removed = removeFirstOccurrenceByValue(list, 10);
        System.out.println("Removed value 10: " + removed);
        System.out.println("Resulting List:   " + list); // [5, 15, 20]
    }
}
```
</details>

---

## 12. Summary / Cheat Sheet

- **8 Wrappers:** `Byte`, `Short`, `Integer`, `Long`, `Float`, `Double`, `Character`, `Boolean`.
- **Cache Flyweights:**
  - `Byte`, `Short`, `Integer`, `Long`: Cached for $[-128, 127]$.
  - `Character`: Cached for $[0, 127]$.
  - `Boolean`: `TRUE` and `FALSE` static singletons.
  - `Float`, `Double`: Never cached.
- **Equality Rule:** Always use `.equals()` to compare wrapper instances; `==` checks reference identity and fails outside cached ranges.
- **Parsing:** Use `Integer.parseInt()` for primitives; `Integer.valueOf()` for wrappers.
- **Performance:** Avoid boxed wrappers in tight loops; use primitive accumulators and primitive streams (`IntStream`, `LongStream`).

---

## 13. Further Reading

- [The Java Language Specification (Java SE 25 Edition) - Boxing and Unboxing Conversions](https://docs.oracle.com/javase/specs/jls/se25/html/jls-5.html#jls-5.1.7)
- [JEP 390: Warnings for Value-Based Classes](https://openjdk.org/jeps/390)
- [Effective Java (3rd Edition) - Item 61: Prefer primitive types to boxed primitives](https://www.oreilly.com/library/view/effective-java-3rd/9780134686097/)

---

| [← Previous: 03_type-casting-and-conversion.md](file:///E:/Learning/engineering-notes/01-languages/java/01-fundamentals/03_type-casting-and-conversion.md) | [Section Index](file:///E:/Learning/engineering-notes/01-languages/java/01-fundamentals/README.md) | [Next: 05_conditionals.md](file:///E:/Learning/engineering-notes/01-languages/java/01-fundamentals/05_conditionals.md) → |
