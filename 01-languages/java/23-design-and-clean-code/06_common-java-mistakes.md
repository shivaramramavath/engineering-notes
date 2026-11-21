# Common Java Mistakes and Anti-Patterns

> The definitive guide to the most dangerous, subtle, and frequently made mistakes in Java: reference equality bugs, the `BigDecimal(double)` trap, `ConcurrentModificationException`, `Arrays.asList` mutability surprises, and integer caching quirks.

| Metadata | Details |
| :--- | :--- |
| **Level** | Core Engineering / Technical Review |
| **Prerequisites** | [01_fundamentals](../01-fundamentals/README.md), [08_collections](../08-collections/README.md), [04_immutability](04_immutability.md) |
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

### What are Common Java Pitfalls?
Java is designed to be a safe, strongly-typed, memory-managed language. However, subtle language semantics, historical legacy baggage (dating back to Java 1.0), autoboxing, and floating-point arithmetic create traps that frequently deceive even experienced developers.

Understanding these pitfalls transforms a programmer from someone who merely writes syntax into a rigorous engineer capable of diagnosing bizarre, silent production data corruptions.

### The Impact of Subtle Java Mistakes
- **Financial Loss**: Creating `BigDecimal` with a `double` produces imprecise calculations (`new BigDecimal(0.1)` becomes `0.10000000000000000555111512312578...`), causing billing errors.
- **Silent Security Bypasses**: Using `==` to compare authentication strings works during local tests (due to String interning) but fails intermittently in production when strings originate from network streams.
- **Production Crashes**: Modifying collections during iteration throws unexpected `ConcurrentModificationException` under user load.

---

## 2. Core Concepts

### 1. Identity vs. Equality (`==` vs. `equals()`)
- `==` compares **memory reference identity** (do both pointers point to the identical memory address on the JVM heap?).
- `equals()` compares **logical value equivalence** (do both objects represent the same semantic data?).
- *The Trap*: Java\'s String Pool interns string literals (`"admin" == "admin"` is `true`), giving developers a false belief that `==` works for strings. When strings are dynamically received from HTTP requests or databases, `==` evaluates to `false`!

### 2. The Integer Autoboxing Cache Quirks
Java caches `Integer` objects in the range **-128 to 127** (via `IntegerCache`):
```java
Integer a = 100;
Integer b = 100;
System.out.println(a == b); // TRUE (both reference cached JVM instance)

Integer c = 200;
Integer d = 200;
System.out.println(c == d); // FALSE! (allocated as separate heap objects!)
```
Comparing wrapper objects with `==` causes bugs that only manifest when numeric values exceed 127!

### 3. The `BigDecimal(double)` Precision Disaster
Floating-point numbers in computers follow IEEE 754 binary floating-point representation. Numbers like `0.1` cannot be represented exactly in binary floating point.
- `new BigDecimal(0.1)` stores the exact imprecise double value: `0.1000000000000000055511151231257827021181583404541015625`.
- `BigDecimal.valueOf(0.1)` or `new BigDecimal("0.1")` preserves the exact decimal string representation.

---

## 3. How It Works Internally

### `ConcurrentModificationException` and the `modCount` Field
Inside `java.util.ArrayList`, an internal field tracks structural modifications:
```java
protected transient int modCount = 0;
```
When an enhanced for-loop (`for (String s : list)`) runs, the compiler translates it to an `Iterator`:
1. The iterator initializes `expectedModCount = modCount`.
2. On every call to `iterator.next()`, it checks:
   ```java
   if (modCount != expectedModCount)
       throw new ConcurrentModificationException();
   ```
3. If code calls `list.remove(item)` inside the loop directly on the collection instead of through `iterator.remove()`, `modCount` increments, causing the next iteration step to crash!

```
+--------------------------------------------------------------------------+
|                  ITERATOR STRUCTURAL MODIFICATION CHECK                  |
|                                                                          |
|  ArrayList: modCount = 5                                                 |
|  Iterator:  expectedModCount = 5                                         |
|                                                                          |
|  User code: list.remove(x) ---> ArrayList: modCount = 6                 |
|                                                                          |
|  Next loop step: iterator.next()                                         |
|    Check: modCount (6) != expectedModCount (5)                           |
|    CRASH: throw new ConcurrentModificationException()                    |
+--------------------------------------------------------------------------+
```

---

## 4. Syntax & Usage

### 1. Correct Collection Removal During Iteration
```java
import java.util.ArrayList;
import java.util.List;

public class SafeRemoval {
    public static void cleanList(List<String> items) {
        // MODERN IDIOMATIC JAVA (Java 8+):
        items.removeIf(item -> item.startsWith("deprecated_"));
    }
}
```

### 2. Correct `BigDecimal` Construction
```java
import java.math.BigDecimal;

public class MoneyUtil {
    // ALWAYS use String or BigDecimal.valueOf:
    public static BigDecimal createAmount(String str) {
        return new BigDecimal(str); // Exact decimal
    }
    public static BigDecimal fromDouble(double val) {
        return BigDecimal.valueOf(val); // Calls Double.toString() internally
    }
}
```

---

## 5. Practical Examples

### Example 1: `Arrays.asList()` vs. `List.of()` vs. `new ArrayList<>()`
**Goal:** Understand the three subtly different list factory methods in Java.

```java
import java.util.Arrays;
import java.util.List;
import java.util.ArrayList;

public class ListComparison {
    public static void demonstrateDifferences() {
        // 1. Arrays.asList(arr):
        // Fixed-size wrapper over original array!
        // Mutating elements modifies the original array! Cannot add or remove elements.
        String[] arr = {"A", "B"};
        List<String> fixedList = Arrays.asList(arr);
        fixedList.set(0, "Z"); // Mutates arr[0] to "Z"!
        // fixedList.add("C"); // THROWS UnsupportedOperationException!

        // 2. List.of("A", "B"):
        // 100% Immutable. Cannot set, add, or remove. Disallows nulls.
        List<String> immutableList = List.of("A", "B");
        // immutableList.set(0, "Z"); // THROWS UnsupportedOperationException!

        // 3. new ArrayList<>(List.of("A", "B")):
        // Fully mutable independent heap collection.
        List<String> mutableList = new ArrayList<>(List.of("A", "B"));
        mutableList.add("C"); // Allowed!
    }
}
```

### Example 2: The `equals()` and `hashCode()` Contract Trap
**Goal:** Understand why failing to override `hashCode()` when overriding `equals()` breaks `HashSet` and `HashMap`.

```java
import java.util.HashSet;
import java.util.Objects;
import java.util.Set;

public class BrokenHashContract {
    public static final class Employee {
        private final int id;
        public Employee(int id) { this.id = id; }

        @Override
        public boolean equals(Object o) {
            if (this == o) return true;
            if (!(o instanceof Employee that)) return false;
            return id == that.id;
        }

        // BUG: hashCode() is NOT overridden!
        // Inherits System.identityHashCode() from Object!
    }

    public static void main(String[] args) {
        Set<Employee> set = new HashSet<>();
        set.add(new Employee(101));

        // Looking up an identical employee:
        boolean exists = set.contains(new Employee(101));
        System.out.println("Employee exists in set: " + exists); 
        // PRINTS FALSE! Why? Different default hash codes land in different buckets!
    }
}
```

### Example 3: Substring Memory Leak (Historical vs. Modern)
In Java 6, `String.substring()` shared the internal character array of the original string, causing tiny substrings to retain multi-megabyte parent strings in memory.
Since Java 7u6+, `String.substring()` creates a fresh, independent copy of the character array, eliminating this historical memory leak.

---

## 6. Best Practices

1. **Always Use `.equals()` for Objects**: Never compare `String`, `Integer`, `UUID`, or any object reference using `==`.
2. **Never Pass `double` to `BigDecimal` Constructor**: Always use `new BigDecimal(String)` or `BigDecimal.valueOf(double)`.
3. **Use `Collection.removeIf()` for Filtering**: Avoid manual iterator removal loops.
4. **Always Implement `equals()` and `hashCode()` Together**: Or better, use Java **Records** which generate correct implementations automatically.
5. **Beware of `Arrays.asList` Mutability**: Use `List.of()` for immutable lists, or `new ArrayList<>()` for mutable lists.
6. **Avoid Autoboxing in High-Frequency Loops**: Use primitive arrays (`long[]`) or specialized primitive streams (`LongStream`) to prevent GC churn.

---

## 7. Common Mistakes & Pitfalls

| Mistake | Why It Fails | Secure Fix |
| :--- | :--- | :--- |
| **`new BigDecimal(0.1)`** | Binary floating point cannot represent 0.1 accurately; stores imprecise fraction. | Use `new BigDecimal("0.1")` or `BigDecimal.valueOf(0.1)`. |
| **`Integer a = 200; Integer b = 200; a == b`** | Values outside -128..127 bypass cache; compares references, evaluating to `false`. | Use `a.equals(b)` or compare primitives (`a.intValue() == b.intValue()`). |
| **`str1 == str2`** | Compares pointer memory addresses instead of string contents. | Always use `str1.equals(str2)`. |
| **Modifying list in `for (var x : list)`** | Violates iterator `modCount` contract; throws `ConcurrentModificationException`. | Use `list.removeIf(predicate)`. |
| **Overriding `equals` without `hashCode`** | Equal objects get different hash codes; `HashSet.contains()` fails. | Override both using `Objects.hash()` or use records. |
| **`Optional.get()` without `isPresent()`** | Throws `NoSuchElementException` on empty values. | Use `orElseThrow()`, `orElse()`, or `map()`. |

---

## 8. Performance & Security Notes

### Constant-Time Password and Token Equality
When checking passwords, cryptographic tokens, or HMAC signatures, even `.equals()` is insecure due to **timing attacks**:
- `.equals()` aborts immediately on the first mismatching character, leaking character positioning to remote attackers.
- Always use `java.security.MessageDigest.isEqual(byte[], byte[])` which runs in strictly constant time.

---

## 9. Comparison / When to Use What

| Scenario | Anti-Pattern | Clean Idiomatic Solution |
| :--- | :--- | :--- |
| **Comparing Strings** | `str == "admin"` | `"admin".equals(str)` or `Objects.equals(a, b)` |
| **Financial Math** | `double price = 0.10` | `BigDecimal price = new BigDecimal("0.10")` |
| **Collection Removal** | `for (T x : list) if (...) list.remove(x)` | `list.removeIf(...)` |
| **Wrapper Equality** | `id1 == id2` | `Objects.equals(id1, id2)` |
| **List Creation** | `Arrays.asList("a", "b")` | `List.of("a", "b")` (immutable) |

---

## 10. Interview Questions & Answers

### Q1: Why does `new BigDecimal(0.1)` not equal `new BigDecimal("0.1")`?
<details>
<summary>View Answer</summary>
The double parameter constructor `new BigDecimal(0.1)` passes the binary floating-point representation of 0.1. Because decimal 0.1 has no exact finite representation in binary IEEE 754 (it is an infinite repeating binary fraction), the double value passed to the constructor is actually `0.1000000000000000055511151231257827021181583404541015625`. In contrast, `new BigDecimal("0.1")` parses the string character by character into an exact decimal scale of 1 and unscaled value of 1.
</details>

### Q2: Why does `Integer a = 127; Integer b = 127; a == b` evaluate to true, but `Integer a = 128; Integer b = 128; a == b` evaluate to false?
<details>
<summary>View Answer</summary>
Java specifies an Integer Cache (`java.lang.Integer.IntegerCache`) that caches boxed `Integer` instances for values between -128 and 127. When autoboxing occurs within this range, `Integer.valueOf()` returns the identical cached object reference, so `==` evaluates to `true`. For values $\ge 128$ (or $< -128$), autoboxing instantiates a new `Integer` object on the heap for each assignment, resulting in distinct memory addresses where `==` evaluates to `false`.
</details>

### Q3: What causes a `ConcurrentModificationException` and can it happen in single-threaded code?
<details>
<summary>View Answer</summary>
Yes, it frequently happens in single-threaded code! A `ConcurrentModificationException` is not strictly a multi-threading error; it is thrown by fail-fast iterators when a collection is structurally modified (elements added or removed) while an iterator is actively traversing it. If code calls `collection.remove()` or `collection.add()` directly inside an enhanced for-loop, the iterator detects that `modCount != expectedModCount` on the next step and throws the exception immediately.
</details>

### Q4: What is the contract between `equals()` and `hashCode()` in Java?
<details>
<summary>View Answer</summary>
The contract dictates that:
1. If two objects are equal according to `equals()`, they **must** return the same integer value from `hashCode()`.
2. If two objects return the same `hashCode()`, they are not necessarily equal (hash collision).
If `equals()` is overridden without overriding `hashCode()`, two logically equal objects will produce different hash codes. When inserted into a `HashSet` or `HashMap`, they land in different buckets, violating the set uniqueness guarantee and causing lookups to fail.
</details>

### Q5: What is the difference between `Arrays.asList()` and `List.of()`?
<details>
<summary>View Answer</summary>
- `Arrays.asList()` returns a fixed-size view backed directly by the underlying array. You can modify elements via `set()`, and modifying the array reflects in the list. Structural additions or deletions throw `UnsupportedOperationException`. Null elements are permitted.
- `List.of()` (Java 9+) returns a truly immutable, compact collection. Any call to `set()`, `add()`, or `remove()` throws `UnsupportedOperationException`. It rejects null elements fail-fast with a `NullPointerException`.
</details>

### Q6: Why is `Optional.get()` considered an anti-pattern?
<details>
<summary>View Answer</summary>
Calling `optional.get()` without first checking `optional.isPresent()` reintroduces the exact same crash risk as a `NullPointerException`, throwing a `NoSuchElementException` if the value is empty. It defeats the entire design goal of `Optional`. Idiomatic code should use functional combinators like `optional.map()`, `optional.filter()`, `optional.orElse()`, or `optional.orElseThrow()`.
</details>

### Q7: Why should developers never use `float` or `double` for currency calculations?
<details>
<summary>View Answer</summary>
`float` and `double` are binary floating-point types designed for scientific and engineering calculations where fast approximations over wide dynamic ranges are acceptable. They cannot represent base-10 fractional values (like \$0.10 or \$0.05) precisely. Performing arithmetic on them introduces cumulative rounding errors (e.g. `1.03 - 0.42 = 0.6100000000000001`), leading to imbalanced financial books and transaction discrepancies.
</details>

### Q8: What is the risk of using `String.intern()` in high-throughput applications?
<details>
<summary>View Answer</summary>
`String.intern()` places string references into the JVM\'s internal native String Pool table. In high-throughput services with millions of dynamic, distinct strings, interning puts immense pressure on the native String Table, causing hash bucket collisions, increasing garbage collection safepoint durations, and consuming native memory outside the normal young generation garbage collection cycle.
</details>

---

## 11. Practice Exercises

### Exercise 1: Refactor Flawed Equality and HashCode
**Task:** Identify and fix the bugs in the following `Point` class so it works correctly in a `HashSet`.
```java
public class Point {
    private int x, y;
    public Point(int x, int y) { this.x = x; this.y = y; }
    // BUG 1: Overloaded instead of overridden!
    public boolean equals(Point p) { return this.x == p.x && this.y == p.y; }
    // BUG 2: Missing hashCode()!
}
```
<details>
<summary>View Solution</summary>

```java
import java.util.Objects;

// Preferred Modern Solution: Record
public record Point(int x, int y) {}

// Or Traditional Class Solution:
public class PointClass {
    private final int x, y;
    public PointClass(int x, int y) { this.x = x; this.y = y; }

    @Override
    public boolean equals(Object o) {
        if (this == o) return true;
        if (!(o instanceof PointClass that)) return false;
        return this.x == that.x && this.y == that.y;
    }

    @Override
    public int hashCode() {
        return Objects.hash(x, y);
    }
}
```
</details>

### Exercise 2: Safe Collection Filter
**Task:** Write a method that filters out negative integers from an `ArrayList` without throwing a `ConcurrentModificationException`.
<details>
<summary>View Solution</summary>

```java
import java.util.List;

public class NumberCleaner {
    public static void removeNegatives(List<Integer> numbers) {
        numbers.removeIf(n -> n < 0);
    }
}
```
</details>

### Exercise 3: Precise Money Addition
**Task:** Write a function that adds an array of double prices without losing decimal precision.
<details>
<summary>View Solution</summary>

```java
import java.math.BigDecimal;

public class AccurateCalculator {
    public static BigDecimal sumPrices(double[] prices) {
        BigDecimal total = BigDecimal.ZERO;
        for (double p : prices) {
            total = total.add(BigDecimal.valueOf(p));
        }
        return total;
    }
}
```
</details>

---

## 12. Summary / Cheat Sheet

- **String Comparison**: Always use `.equals()`. Never use `==`.
- **BigDecimal**: Always use `new BigDecimal(String)` or `BigDecimal.valueOf(double)`.
- **Autoboxing**: Wrapper comparison with `==` breaks outside `-128..127`.
- **Concurrent Modification**: Never call `list.remove()` inside enhanced for-loops; use `list.removeIf()`.
- **Equals & HashCode**: Always override both together, or use Java records.
- **Collection Factories**: Use `List.of()` for immutable lists; `Arrays.asList()` is fixed-size and mutable.

---

## 13. Further Reading
- Effective Java: [Item 10: Obey the general contract when overriding equals](https://www.oreilly.com/library/view/effective-java-3rd/9780134686097/)
- Effective Java: [Item 11: Always override hashCode when you override equals](https://www.oreilly.com/library/view/effective-java-3rd/9780134686097/)
- Effective Java: [Item 60: Avoid float and double if exact answers are required](https://www.oreilly.com/library/view/effective-java-3rd/9780134686097/)
- Java Language Specification: [Integer Autoboxing Cache Semantics](https://docs.oracle.com/javase/specs/jls/se21/html/jls-5.html#jls-5.1.7)
