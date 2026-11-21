# StringBuilder and StringBuffer: Mutable Character Sequences

> An authoritative guide to mutable string buffers in Java, exploring `AbstractStringBuilder` internals, dynamic array capacity expansion, concurrency trade-offs, and modern compiler optimization.

| Metadata | Details |
| :--- | :--- |
| **Level** | Intermediate |
| **Prerequisites** | [00_string-basics.md](./00_string-basics.md), [07_arrays.md](../01-fundamentals/07_arrays.md) |
| **Time to Read** | 25 minutes |
| **Status** | Active |
| **Java Version** | Java 8+ (Compact Strings layout in Java 9+) |

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
9. [Comparison: String vs StringBuilder vs StringBuffer](#9-comparison-string-vs-stringbuilder-vs-stringbuffer)
10. [Interview Questions](#10-interview-questions)
11. [Practice Exercises](#11-practice-exercises)
12. [Summary & Cheat Sheet](#12-summary--cheat-sheet)
13. [Further Reading](#13-further-reading)

---

## 1. What & Why

Because `java.lang.String` is immutable, repeatedly appending, inserting, or modifying text using ordinary `+` concatenation in a loop results in quadratic memory allocation ($O(N^2)$) and severe garbage collection pressure.

To provide high-performance, in-place character manipulation, Java provides two companion classes in `java.lang`:
1. **`StringBuilder` (Introduced in Java 5):** An unsynchronized, highly optimized mutable sequence of characters designed for single-threaded usage and local method scopes.
2. **`StringBuffer` (Introduced in Java 1.0):** A synchronized, thread-safe mutable sequence where almost every public method is guarded by `synchronized`.

In 99.9% of modern applications, `StringBuilder` is the proper choice because string construction is almost always confined to a single thread.

---

## 2. Core Concepts

### 2.1 The Hierarchy
Both classes extend the package-private class `AbstractStringBuilder` and implement `CharSequence`, `Appendable`, and `Serializable`:

```
               ┌──────────────────────┐
               │     CharSequence     │
               └──────────┬───────────┘
                          │
               ┌──────────▼───────────┐
               │AbstractStringBuilder │
               └─────┬───────────┬────┘
                     │           │
     ┌───────────────▼┐         ┌▼────────────────┐
     │ StringBuilder  │         │  StringBuffer   │
     │(Unsynchronized)│         │ (Synchronized)  │
     └────────────────┘         └─────────────────┘
```

### 2.2 Capacity vs Length
- **`length()`:** The number of characters actually stored in the buffer.
- **`capacity()`:** The total number of characters the allocated backing array can hold before requiring resizing.
- Default initial capacity: Calling `new StringBuilder()` allocates a backing buffer with a capacity of **16 characters**.

---

## 3. How It Works Internally

### 3.1 Backing Storage & Compact Strings
Since Java 9, `AbstractStringBuilder` shares the Compact Strings optimization introduced in JEP 254:
```java
abstract class AbstractStringBuilder implements Appendable, CharSequence {
    byte[] value; // Backing byte array
    byte coder;   // 0 = LATIN1 (1 byte/char), 1 = UTF16 (2 bytes/char)
    int count;    // Number of characters currently used
}
```

```
StringBuilder Internal Memory Layout:
┌────────────────────────────────────────────────────────┐
│                  StringBuilder Object                  │
├──────────────────────────┬─────────────────────────────┤
│ Mark Word                │ 8 bytes                     │
│ Compressed Klass Pointer │ 4 bytes                     │
│ count (int)              │ 4 bytes                     │
│ coder (byte)             │ 1 byte                      │
│ (alignment padding)      │ 3 bytes                     │
│ value (reference)        │ 4 bytes (points to byte[])  │
└──────────────────────────┴─────────────────────────────┘
          │
          ▼
┌────────────────────────────────────────────────────────┐
│                   byte[] Array Object                  │
├──────────────────────────┬─────────────────────────────┤
│ Array Header             │ 16 bytes                    │
│ Capacity Payload         │ capacity * (coder == 0? 1:2)│
└────────────────────────────────────────────────────────┘
```

### 3.2 Dynamic Growth Algorithm
When an append operation causes `count + addedLength > capacity`, `AbstractStringBuilder` expands its backing array using a growth formula:
$$\text{newCapacity} = (\text{oldCapacity} \times 2) + 2$$
If `newCapacity` is still insufficient, it expands directly to `count + addedLength`.

Whenever resizing occurs:
1. A new `byte[]` array of the larger size is allocated on the heap.
2. The existing characters are copied from the old array to the new array using `System.arraycopy()`.
3. The old array is discarded and becomes eligible for GC collection.

Pre-sizing the `StringBuilder` with `new StringBuilder(expectedLength)` completely eliminates intermediate reallocations and array copies!

---

## 4. Syntax & Usage

### 4.1 Common Mutation APIs
```java
StringBuilder sb = new StringBuilder(64); // Pre-sized capacity 64

// Chained Append
sb.append("Order: #")
  .append(1024)
  .append(" | Status: ")
  .append("CONFIRMED");

// Insertion
sb.insert(0, "[URGENT] ");

// Deletion
sb.delete(0, 9);                         // Deletes "[URGENT] " [start, end)
sb.deleteCharAt(sb.length() - 1);        // Deletes last char

// In-place Modification
sb.setCharAt(0, 'o');
sb.replace(0, 5, "Invoice");             // Replaces range

// Reversal
sb.reverse();

// Convert to immutable String
String finalResult = sb.toString();
```

---

## 5. Practical Examples

### Example 1: Basic Buffer Operations & Capacity Growth
```java
public class StringBuilderBasicsDemo {
    public static void main(String[] args) {
        StringBuilder sb = new StringBuilder(5);
        System.out.println("Initial capacity: " + sb.capacity() + ", length: " + sb.length());

        sb.append("Hello");
        System.out.println("After 'Hello' capacity: " + sb.capacity() + ", length: " + sb.length());

        // Adding 1 more character triggers growth: (5 * 2) + 2 = 12
        sb.append("!");
        System.out.println("After '!' capacity: " + sb.capacity() + ", length: " + sb.length());
    }
}
```
**Output:**
```text
Initial capacity: 5, length: 0
After 'Hello' capacity: 5, length: 5
After '!' capacity: 12, length: 6
```

### Example 2: Real-World High-Performance SQL Query Builder
```java
import java.util.List;

public class SqlQueryBuilder {
    public static String buildSelectQuery(String table, List<String> columns, String condition) {
        // Pre-allocate capacity: table length + column estimates + boilerplate
        StringBuilder sb = new StringBuilder(128);

        sb.append("SELECT ");
        for (int i = 0; i < columns.size(); i++) {
            if (i > 0) sb.append(", ");
            sb.append(columns.get(i));
        }

        sb.append(" FROM ").append(table);

        if (condition != null && !condition.isBlank()) {
            sb.append(" WHERE ").append(condition);
        }

        return sb.toString();
    }

    public static void main(String[] args) {
        String query = buildSelectQuery("users", List.of("id", "username", "email"), "active = 1");
        System.out.println("Generated SQL: " + query);
    }
}
```
**Output:**
```text
Generated SQL: SELECT id, username, email FROM users WHERE active = 1
```

### Example 3: Edge Case – Appending `null` and `char[]`
```java
public class StringBuilderEdgeCases {
    public static void main(String[] args) {
        StringBuilder sb = new StringBuilder();

        // Appending null Object appends the 4 characters "null"
        Object obj = null;
        sb.append(obj);
        System.out.println("Appended null obj: [" + sb + "]");

        // Appending null String appends "null"
        String s = null;
        sb.append(s);
        System.out.println("Appended null str: [" + sb + "]");

        // Appending null char[] THROWS NullPointerException!
        try {
            char[] nullChars = null;
            sb.append(nullChars);
        } catch (NullPointerException e) {
            System.out.println("Caught expected NPE when appending null char[]!");
        }
    }
}
```
**Output:**
```text
Appended null obj: [null]
Appended null str: [nullnull]
Caught expected NPE when appending null char[]!
```

### Example 4: Failure Case – Sharing `StringBuilder` Across Threads
```java
import java.util.concurrent.CountDownLatch;
import java.util.concurrent.Executors;

public class StringBuilderRaceCondition {
    public static void main(String[] args) throws InterruptedException {
        StringBuilder unsafeSb = new StringBuilder();
        int threads = 10;
        int appendsPerThread = 1000;
        var executor = Executors.newFixedThreadPool(threads);
        var latch = new CountDownLatch(threads);

        for (int t = 0; t < threads; t++) {
            executor.submit(() -> {
                try {
                    for (int i = 0; i < appendsPerThread; i++) {
                        unsafeSb.append("x"); // Non-atomic increment of count!
                    }
                } finally {
                    latch.countDown();
                }
            });
        }

        latch.await();
        executor.shutdown();

        int expected = threads * appendsPerThread;
        System.out.println("Expected length: " + expected);
        System.out.println("Actual length:   " + unsafeSb.length());
        System.out.println("Data corruption occurred: " + (unsafeSb.length() != expected));
    }
}
```
**Output:**
```text
Expected length: 10000
Actual length:   ... (significantly less than 10000, e.g. 8421, due to race conditions)
Data corruption occurred: true
```

### Example 5: Optimized / Idiomatic – Reusing Buffers with `setLength(0)`
```java
public class ReusableBufferPool {
    private final StringBuilder buffer = new StringBuilder(256);

    public String formatLog(String level, String msg) {
        // Reset count without reallocating backing array!
        buffer.setLength(0);

        buffer.append("[").append(level).append("] ")
              .append(System.currentTimeMillis()).append(" - ")
              .append(msg);

        return buffer.toString();
    }

    public static void main(String[] args) {
        ReusableBufferPool pool = new ReusableBufferPool();
        System.out.println(pool.formatLog("INFO", "Server starting"));
        System.out.println(pool.formatLog("DEBUG", "Loaded config"));
    }
}
```
**Output:**
```text
[INFO] ... - Server starting
[DEBUG] ... - Loaded config
```

---

## 6. Best Practices

1. **Always Pre-Size When Output Size is Estimable:** If you expect ~500 characters, construct `new StringBuilder(512)`. This saves multiple array allocations and copy cycles.
2. **Confine `StringBuilder` to Local Method Scopes:** Never store a `StringBuilder` in a multi-threaded instance field without synchronization. Keep it inside the method where it is instantiated and converted via `.toString()`.
3. **Reset Buffers with `setLength(0)`:** When processing high-throughput streams or batches in a single thread, reset an existing builder using `sb.setLength(0)` rather than allocating a new `StringBuilder` per record.
4. **Prefer Single `+` for Inline Code:** Do not write `new StringBuilder().append("a").append(b).toString()`. Standard `"a" + b` is cleaner and compiled directly to `invokedynamic` in Java 9+.
5. **Beware `sb.append(null)`:** Appending a null object or string appends the literal `"null"`. Always guard optional variables if `"null"` text in output is undesirable.

---

## 7. Common Mistakes & Pitfalls

| Mistake | Why It Is Wrong | Correct Idiom |
| :--- | :--- | :--- |
| Sharing `StringBuilder` across threads | Not thread-safe; concurrent updates corrupt internal `count` and produce garbled strings or array index exceptions. | Use local `StringBuilder`, thread-local buffer, or `StringBuffer`. |
| Default capacity in large loops | Default capacity is only 16; repeatedly doubling causes repeated array allocations and GC churn. | `new StringBuilder(estimatedLength)`. |
| `sb.equals(otherSb)` | `StringBuilder` does **not** override `equals()` or `hashCode()`. It inherits reference equality (`==`) from `Object`! | `sb.toString().equals(otherSb.toString())` or `sb.compareTo(otherSb) == 0`. |
| `sb.append((char[]) null)` | Overloaded `append(char[])` throws `NullPointerException` unlike `append(Object)` which appends `"null"`. | Guard `null` arrays before appending. |
| Using `StringBuffer` unnecessarily | `StringBuffer` incurs synchronization locking overhead on every single method call. | Use `StringBuilder` unless concurrent thread safety is strictly required. |
| Over-clearing with `new StringBuilder()` | Allocating new objects per iteration inside hot loops creates heap pressure. | Use `sb.setLength(0)`. |

---

## 8. Performance & Security Notes

### 8.1 Why `StringBuilder` Does Not Override `equals()`
Neither `StringBuilder` nor `StringBuffer` overrides `equals()` or `hashCode()`. Because they are mutable, modifying their character contents would alter their hash code dynamically, corrupting any `HashSet` or `HashMap` holding them as keys. To compare content, use `sb.compareTo(otherSb) == 0` (which implements `Comparable<StringBuilder>` since Java 11).

---

## 9. Comparison: String vs StringBuilder vs StringBuffer

| Feature | `String` | `StringBuilder` | `StringBuffer` |
| :--- | :--- | :--- | :--- |
| **Mutability** | Immutable | Mutable | Mutable |
| **Thread Safe** | ✅ Thread-safe | ❌ Not thread-safe | ✅ Thread-safe (`synchronized`) |
| **Performance** | Slower in loops ($O(N^2)$) | Extremely fast ($O(N)$) | Slower due to lock contention |
| **Since** | Java 1.0 | Java 5 | Java 1.0 |
| **Memory Reuse** | New object per mutation | Reuses backing array | Reuses backing array |
| **equals() behavior**| Content equality | Reference equality | Reference equality |

---

## 10. Interview Questions

<details>
<summary><strong>1. What is the fundamental difference between StringBuilder and StringBuffer?</strong></summary>

`StringBuffer` was introduced in Java 1.0 and is thread-safe; all its public mutation methods are marked `synchronized`. `StringBuilder` was introduced in Java 5 as an unsynchronized alternative. Because synchronization incurs locking overhead and string construction is almost always confined to a single thread, `StringBuilder` is significantly faster and should be used in virtually all modern scenarios.
</details>

<details>
<summary><strong>2. How does StringBuilder dynamically resize its internal buffer?</strong></summary>

When characters are appended that exceed the current capacity, `AbstractStringBuilder` calculates a new capacity using the formula `(oldCapacity * 2) + 2`. If that is still insufficient, it expands to `count + addedLength`. It then allocates a new `byte[]` array of that size, copies existing bytes via `System.arraycopy`, and updates the internal reference.
</details>

<details>
<summary><strong>3. Why does sb1.equals(sb2) return false even when both have identical characters?</strong></summary>

`StringBuilder` does not override `equals()` from `java.lang.Object`. Consequently, `equals()` performs reference equality (`==`). Because mutable objects change their hash code and contents over time, overriding `equals()` could violate collection contracts. To compare content, use `sb1.compareTo(sb2) == 0` or compare their `.toString()` values.
</details>

<details>
<summary><strong>4. How do you reset a StringBuilder to reuse it without allocating a new object?</strong></summary>

Call `sb.setLength(0)`. This simply sets the internal `count` field to 0 without discarding or reallocating the underlying backing array, allowing the buffer to be reused cleanly with zero garbage collection overhead.
</details>

<details>
<summary><strong>5. What is the default capacity of a StringBuilder, and why does pre-sizing matter?</strong></summary>

The default capacity is 16 characters (`new StringBuilder()`). If building a 500-character payload, the builder will resize and copy its array approximately 5 times (16 $\to$ 34 $\to$ 70 $\to$ 142 $\to$ 286 $\to$ 574). Pre-sizing with `new StringBuilder(512)` executes zero intermediate reallocations and zero array copies.
</details>

<details>
<summary><strong>6. When should you prefer + over an explicit StringBuilder?</strong></summary>

In a single statement or method return (e.g. `return "Prefix: " + id + " - " + name;`), prefer `+`. Since Java 9, the compiler replaces single-expression concatenations with `invokedynamic` (`StringConcatFactory`), which computes the exact required buffer size upfront and executes faster or equal to manual `StringBuilder`. Explicit `StringBuilder` is required inside loops or complex conditional branches.
</details>

<details>
<summary><strong>7. What happens if you call sb.append((char[]) null)?</strong></summary>

It throws a `NullPointerException`. Although `sb.append((String) null)` and `sb.append((Object) null)` safely append the characters `"null"`, the overloaded `append(char[] str)` directly attempts to access `str.length`, which immediately throws `NullPointerException`.
</details>

<details>
<summary><strong>8. Does StringBuilder implement Comparable?</strong></summary>

Yes. Since Java 11, `StringBuilder` implements `Comparable<StringBuilder>`, performing a lexicographical comparison of characters without allocating intermediate `String` instances.
</details>

---

## 11. Practice Exercises

### Easy
1. **Word Reverser:** Write a method using `StringBuilder` to reverse every word in a sentence while preserving original word order and whitespace.
2. **Remove Vowels:** Use `StringBuilder` to remove all vowels from a string in-place using `deleteCharAt()`.
3. **Format Int Array:** Format an array of integers as `[1, 2, 3]` using `StringBuilder`.

### Medium
4. **Custom Delimited Builder:** Implement a fluent builder `DelimitedStringBuilder` that automatically inserts a delimiter (such as `, `) only between elements, avoiding trailing delimiters without conditional index checks.
5. **Palindrome Verification with StringBuilder:** Verify if a string is a palindrome using `sb.reverse()` and `sb.compareTo()`.
6. **Thread-Safe Append Benchmark:** Write a test comparing the throughput of `StringBuilder` vs `StringBuffer` under single-threaded and multi-threaded scenarios.

### Hard
7. **Zero-GC JSON Stream Serializer:** Build a high-throughput JSON object serializer using a thread-local, reusable `StringBuilder` pool that writes nested objects with zero intermediate allocations.
8. **RLE Compressor:** Implement Run-Length Encoding (RLE) (e.g., `"AAABBBCC"` $\to$ `"A3B3C2"`) using `StringBuilder` in linear time with dynamic pre-sizing.
9. **Byte-Level String Builder:** Implement a simplified custom `ByteStringBuilder` supporting Latin-1 encoding directly to understand JVM internals.

<details>
<summary><strong>Exercise Solutions</strong></summary>

```java
// Solution to Exercise 1: Word Reverser
public static String reverseWords(String sentence) {
    if (sentence == null) return null;
    String[] words = sentence.split(" ");
    StringBuilder result = new StringBuilder(sentence.length());
    for (int i = 0; i < words.length; i++) {
        if (i > 0) result.append(' ');
        result.append(new StringBuilder(words[i]).reverse());
    }
    return result.toString();
}

// Solution to Exercise 4: DelimitedStringBuilder
public class DelimitedStringBuilder {
    private final String delimiter;
    private final StringBuilder sb = new StringBuilder();
    private boolean first = true;

    public DelimitedStringBuilder(String delimiter) {
        this.delimiter = delimiter;
    }

    public DelimitedStringBuilder append(Object obj) {
        if (!first) sb.append(delimiter);
        else first = false;
        sb.append(obj);
        return this;
    }

    @Override
    public String toString() {
        return sb.toString();
    }
}
```
</details>

---

## 12. Summary & Cheat Sheet

```
┌────────────────────────────────────────────────────────────────────────┐
│                   STRINGBUILDER & BUFFER CHEAT SHEET                   │
├────────────────────────────────────────────────────────────────────────┤
│ StringBuilder:     Fast, unsynchronized, single-threaded buffer (Java 5)│
│ StringBuffer:      Thread-safe, synchronized, legacy (Java 1.0)        │
│ Default Capacity:  16 characters (doubles as: (old * 2) + 2)           │
│ Pre-sizing:        new StringBuilder(expectedLength) prevents resizing │
│ Reset Buffer:      sb.setLength(0) resets count, keeps array memory    │
│ Equality Trap:     sb.equals(sb2) checks reference equality (==)!      │
│ Comparison:        sb.compareTo(sb2) == 0 checks character contents    │
│ Single expression: Use + (compiled to invokedynamic in Java 9+)        │
│ Loop concatenation:ALWAYS use StringBuilder                            │
└────────────────────────────────────────────────────────────────────────┘
```

---

## 13. Further Reading
- Official Java 25 `java.lang.StringBuilder` Documentation
- [JEP 254: Compact Strings](https://openjdk.org/jeps/254)
- *Effective Java (3rd Edition)*, Joshua Bloch, Item 63: "Beware the performance of string concatenation".
