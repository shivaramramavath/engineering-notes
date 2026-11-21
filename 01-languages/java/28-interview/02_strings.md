# Interview Mastery: Strings, Compact Strings & String Pool

> In-depth interview guide for Java String mechanics: String Constant Pool internals, Compact Strings architecture, `StringBuilder` vs `StringBuffer`, security ramifications, and bytecode concatenation.

| Metadata | Value |
|---|---|
| **Level** | Intermediate to Advanced |
| **Prerequisites** | [00_core-java.md](00_core-java.md), [03-strings-and-text](../03-strings-and-text/README.md) |
| **Time to Read** | 35 minutes |
| **Status** | Complete |
| **Java Version** | Java 8 to Java 25 LTS |

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

In modern Java applications, strings account for **25% to 40% of the entire heap footprint**. Consequently, interviewers at top-tier tech companies probe string internals extensively to test an engineer's grasp of memory efficiency, security, and low-level JVM optimization.

Key interview themes include:
- The exact lifecycle and location of the **String Constant Pool (SCP)**.
- Why **Compact Strings (JEP 254)** replaced `char[]` with `byte[]` in Java 9.
- Why passwords must be handled in `char[]` rather than `String`.
- How the compiler rewrites string concatenation using **`invokedynamic` and `StringConcatFactory`** instead of naive `StringBuilder` instances.

---

## 2. Core Concepts

### 1. Immutability & Security Invariants
Once a `java.lang.String` instance is constructed on the heap, its character sequence can never be altered:
- **Security**: Database connection strings, network socket URLs, and file paths are immutable; an attacker cannot alter an authorized path after a permission check.
- **Hash Stability**: The 32-bit `hashCode` is calculated lazily on first access and cached in the instance (`int hash`), making `String` the canonical high-performance key for `HashMap`.
- **String Pool Sharing**: Different threads can share literal references without synchronization locks.

### 2. Compact Strings (JEP 254, Java 9+)
Prior to Java 9, every `String` was backed by a UTF-16 `char[]` array where every character consumed 2 bytes, even if the text consisted entirely of ASCII characters.
Java 9 introduced **Compact Strings**:
```java
private final byte[] value;
private final byte coder; // 0 for LATIN1 (1 byte/char), 1 for UTF16 (2 bytes/char)
```
If a string contains only characters whose code points are $\le 255$ (Latin-1/ASCII), it allocates 1 byte per character, **reducing heap consumption by 50%**.

### 3. String Constant Pool (SCP)
- The String Constant Pool is a native C++ hash table (`StringTable`) residing inside the **Java Heap** (migrated out of PermGen in Java 7).
- String literals (e.g. `"hello"`) are interned automatically during class loading.
- `new String("hello")` explicitly forces the creation of a **new, unpooled object on the standard heap**, even if `"hello"` already resides in the pool.

---

## 3. How It Works Internally

### Memory Layout of a Compact String (64-bit JVM, Compressed OOPs)
```
+-------------------------------------------------------+
| String Object Header: Mark Word                       | (8 bytes)
| String Object Header: Klass Word                      | (4 bytes)
| byte[] value reference                                | (4 bytes)
| int hash (cached hash code)                           | (4 bytes)
| byte coder (0 = LATIN1, 1 = UTF16)                    | (1 byte)
| boolean hashIsZero (Java 13+)                         | (1 byte)
| padding                                               | (2 bytes)
+-------------------------------------------------------+
Total Shallow Object Size: 24 bytes

Backing byte[] Array for "Java" (LATIN1):
| Array Header                                          | (16 bytes)
| 4 bytes payload ('J', 'a', 'v', 'a')                  | (4 bytes)
| padding                                               | (4 bytes)
Total Array Size: 24 bytes
Combined footprint: 48 bytes (vs 56+ bytes in Java 8)
```

### String Concatenation Bytecode Evolution
- **Java 5–8**: The compiler replaced `a + b` with `new StringBuilder().append(a).append(b).toString()`.
- **Java 9+ (JEP 280)**: Replaced with an `invokedynamic` call site targeting `java.lang.invoke.StringConcatFactory.makeConcatWithConstants()`. At runtime, the JVM generates optimal bytecode tailored directly to the architecture, using method handles and direct array copies to minimize intermediate object allocations.

---

## 4. Syntax & Usage

### Text Blocks & String Formatting (Java 15–25)

```java
public class StringModernSyntax {
    public static void main(String[] args) {
        // Java 15+ Multi-line Text Blocks
        String jsonPayload = "\"\"\"\n            {\n                \"userId\": 1001,\n                \"role\": \"ADMIN\",\n                \"active\": true\n            }\n            \"\"\"";

        // Modern formatted string (Java 15+)
        String greeting = "Hello %s, balance is $%.2f".formatted("Alice", 1250.50);

        // Repeat, strip, isBlank (Java 11+)
        String banner = "=".repeat(10);
        assert "   ".isBlank() : "Whitespace only strings evaluate to isBlank() true";
        assert "  text  ".strip().equals("text") : "strip() handles Unicode whitespace correctly";
    }
}
```

---

## 5. Practical Examples

### Example 1: Literal vs Heap Instantiation (`==` vs `.equals()`)

```java
public class StringPoolProof {
    public static void main(String[] args) {
        String s1 = "interview";           // In String Constant Pool
        String s2 = "interview";           // Reuses existing SCP instance
        String s3 = new String("interview"); // New heap object outside SCP

        System.out.println(s1 == s2);      // true (same memory address)
        System.out.println(s1 == s3);      // false (distinct heap objects)
        System.out.println(s1.equals(s3)); // true (identical content)

        // intern() returns canonical reference from pool
        String s4 = s3.intern();
        System.out.println(s1 == s4);      // true (points back to SCP instance)
    }
}
```

### Example 2: StringBuilder Pre-Sizing Performance Benchmark

```java
public class StringBuilderCapacity {
    public static void benchmark() {
        int iterations = 100_000;

        // Default capacity is 16: triggers repeated array re-allocations and copies!
        long start1 = System.nanoTime();
        StringBuilder defaultSb = new StringBuilder();
        for (int i = 0; i < iterations; i++) {
            defaultSb.append("x");
        }
        long time1 = System.nanoTime() - start1;

        // Pre-sized capacity: zero re-allocations
        long start2 = System.nanoTime();
        StringBuilder preSizedSb = new StringBuilder(iterations);
        for (int i = 0; i < iterations; i++) {
            preSizedSb.append("x");
        }
        long time2 = System.nanoTime() - start2;

        System.out.printf("Default capacity: %.2f ms%n", time1 / 1e6);
        System.out.printf("Pre-sized capacity: %.2f ms%n", time2 / 1e6);
    }
}
```

### Example 3: Why Passwords Must Use `char[]` Rather Than `String`

```java
import java.util.Arrays;

public class PasswordSecurityInterview {
    public static void handleAuth() {
        // INSECURE: String remains in heap memory until GC; vulnerable to heap dump inspection
        String insecurePass = "SuperSecret123!";

        // SECURE: char[] can be explicitly zeroed out immediately after verification
        char[] securePass = new char[]{'S', 'u', 'p', 'e', 'r', '1', '2', '3', '!'};

        try {
            authenticate(securePass);
        } finally {
            // Overwrite memory immediately
            Arrays.fill(securePass, '0');
        }

        // Memory now contains ['0', '0', '0', ...]
        System.out.println("Password wiped from memory.");
    }

    private static void authenticate(char[] pass) {
        // Authenticate credentials...
    }
}
```

### Example 4: String Deduplication with G1GC

```java
public class StringDeduplicationDemo {
    public static void main(String[] args) {
        // When running with: -XX:+UseG1GC -XX:+UseStringDeduplication
        // G1GC background thread scans strings in Young Gen.
        // If two strings have identical content, their 'value' byte[] arrays
        // are unified to share the exact same backing array on the heap!
        String a = new String("duplicate_content");
        String b = new String("duplicate_content");

        // Prior to dedup: distinct byte[] arrays
        // After G1GC dedup pass: a.value == b.value (shares byte[] memory)
        System.out.println(a.equals(b)); // true
    }
}
```

### Example 5: Constant Expression Inlining at Compile Time

```java
public class CompileTimeConstants {
    public static void main(String[] args) {
        String base = "hello";
        String s1 = "helloworld";

        // s2 is computed from literals at COMPILE TIME (inlined into bytecode as "helloworld")
        String s2 = "hello" + "world";

        // s3 involves a variable; computed at RUNTIME via dynamic concat
        String s3 = base + "world";

        System.out.println(s1 == s2); // true (both reference the exact same literal in SCP)
        System.out.println(s1 == s3); // false (s3 is a newly allocated heap string)
    }
}
```

---

## 6. Best Practices

1. **Pre-Size `StringBuilder`**: Always provide an estimated capacity `new StringBuilder(estimatedLength)` when concatenating in loops to eliminate intermediate buffer resizing copies.
2. **Never Concatenate in Loops with `+`**: Writing `for (...) str += token;` compiles to creating a new `StringBuilder` on **every single iteration**, causing catastrophic $O(N^2)$ quadratic allocations.
3. **Use `strip()` Over `trim()`**: Java 11's `strip()` handles all Unicode whitespace characters (including non-breaking spaces), whereas `trim()` only checks characters $\le \text{U+0020}$.
4. **Use `char[]` for Sensitive Secrets**: Store passwords, tokens, and cryptographic keys in `char[]` or `byte[]` arrays, and wipe them via `Arrays.fill(array, (char) 0)` inside a `finally` block.

---

## 7. Common Mistakes & Pitfalls

| Mistake | Why It Fails | Fix |
|---|---|---|
| String concatenation inside loops (`s += item`) | Creates a new `StringBuilder` and allocates a new array on every iteration ($O(N^2)$ time) | Use a single `StringBuilder` outside the loop. |
| Comparing strings with `==` | Compares memory addresses, not string text | Always compare string content with `.equals()`. |
| Using `String.intern()` on arbitrary untrusted input | Floods the internal `StringTable` hash table, causing memory leaks and GC latency | Use application-level bounded caches (e.g. Caffeine). |
| Using `toLowerCase()` without `Locale` | Turkish 'i' collapses unpredictably depending on OS locale setting | Use `toLowerCase(Locale.ROOT)`. |
| Assuming `StringBuffer` is faster | `StringBuffer` methods are synchronized, incurring unnecessary locking overhead | Use `StringBuilder` unless cross-thread safety is required. |

---

## 8. Performance & Security Notes

- **StringTable Sizing**: The native String Constant Pool hash table size can be configured via `-XX:StringTableSize=N` (default 65,536 in Java 17+). If an application interns millions of strings into an under-sized table, hash collisions degrade lookups to $O(N)$.
- **Constant-Time Comparison for Cryptography**: Standard `String.equals()` returns `false` as soon as the first differing character is found. This leaks character timing information. In authentication routines, always compare tokens using `MessageDigest.isEqual(a.getBytes(), b.getBytes())` to prevent timing attacks.

---

## 9. Comparison / When to Use What

| Feature | `String` | `StringBuilder` | `StringBuffer` |
|---|---|---|---|
| **Immutability** | Immutable | Mutable | Mutable |
| **Thread Safety** | Thread-safe (read-only) | Not thread-safe | Thread-safe (`synchronized`) |
| **Performance** | High for reads; slow for mutations | Maximum performance | Slower due to lock overhead |
| **Memory Allocation**| New object on every modification | Resizable buffer (`byte[]`) | Resizable buffer (`byte[]`) |
| **Primary Use Case** | Keys, DTOs, domain values | Loop aggregations, text building | Legacy concurrent formatting |

---

## 10. Interview Questions

<details>
<summary>1. How does Compact Strings (JEP 254) work in Java 9+ and what problem did it solve?</summary>

In Java 8 and earlier, `String` stored characters in a `char[]` array where every character took 16 bits (2 bytes). Profiling showed that over 75% of strings in typical heaps contained only ASCII characters requiring at most 8 bits. JEP 254 changed the backing field to `byte[]` paired with an encoding flag `byte coder` (`LATIN1` = 0, `UTF16` = 1). Latin-1 strings take 1 byte per character, cutting string memory footprint in half.
</details>

<details>
<summary>2. What is the difference between `String s = "abc"` and `String s = new String("abc")`?</summary>

- `String s = "abc"` checks the String Constant Pool (SCP). If `"abc"` exists, it returns the pooled reference. If not, it creates `"abc"` in the pool.
- `String s = new String("abc")` creates `"abc"` in the pool (if absent), and **additionally creates a brand new `String` object in standard heap memory**, binding `s` to the heap object. It results in two distinct objects.
</details>

<details>
<summary>3. Why should sensitive passwords never be stored in a `String`?</summary>

Because strings are immutable and cannot be modified or cleared from memory. Once created, a password string remains in heap memory until the garbage collector collects it, leaving a window of vulnerability where a heap dump or memory scanner can extract plaintext credentials. With `char[]`, the memory can be immediately overwritten with zeros (`Arrays.fill(chars, '0')`) once authentication completes.
</details>

<details>
<summary>4. How does `String.intern()` work and where is the String Constant Pool stored?</summary>

Since Java 7, the String Constant Pool is stored in the **Java Heap** (migrated out of PermGen). When `s.intern()` is called, the JVM checks the internal `StringTable`. If the pool already contains a string equal to `s`, the pooled reference is returned. If not, `s` is added to the pool and its reference is returned.
</details>

<details>
<summary>5. What is the difference between `trim()` and `strip()` in Java 11?</summary>

`trim()` only removes characters whose Unicode value is $\le \text{U+0020}$ (standard ASCII space, tab, newline). It fails to remove Unicode whitespaces such as non-breaking space (`\u00A0`), em-space, or ideographic space. Java 11's `strip()` is Unicode-aware and correctly strips all characters defined as whitespace by `Character.isWhitespace()`.
</details>

<details>
<summary>6. How did Java 9 (JEP 280) improve string concatenation bytecode?</summary>

Prior to Java 9, the compiler generated explicit `StringBuilder` bytecodes for string concatenation. JEP 280 introduced `invokedynamic` with `StringConcatFactory.makeConcatWithConstants()`. This decouples bytecode generation from the concatenation strategy, allowing HotSpot to optimize concatenation at runtime without generating intermediary `StringBuilder` allocations.
</details>

<details>
<summary>7. What happens if you concatenate strings inside a loop using the `+` operator?</summary>

On every iteration of the loop, the JVM instantiates a new `StringBuilder` instance, appends the current string and new element, converts it back to a string via `toString()`, and discards the `StringBuilder`. This results in $O(N^2)$ quadratic allocations, creating millions of temporary garbage objects and destroying application performance.
</details>

<details>
<summary>8. Does `String.substring()` cause memory leaks in modern Java?</summary>

Not anymore. In Java 6, `substring()` shared the entire underlying `char[]` array of the parent string, adjusting only the `offset` and `count`. Storing a tiny substring of a 100MB string kept the entire 100MB array alive in memory. Starting in **Java 7u6**, `substring()` creates a brand-new, trimmed backing array, completely eliminating this memory leak.
</details>

---

## 11. Practice Exercises

### Exercise 1: Count Palindromic Substrings with Manacher's Algorithm
Find the total number of palindromic substrings in a string in $O(N)$ time.

<details>
<summary>Solution</summary>

```java
public class PalindromicSubstringsCount {
    public int countSubstrings(String s) {
        if (s == null || s.isEmpty()) return 0;

        StringBuilder sb = new StringBuilder("^");
        for (char c : s.toCharArray()) sb.append('#').append(c);
        sb.append("#$");
        char[] t = sb.toString().toCharArray();

        int[] p = new int[t.length];
        int center = 0, right = 0;
        int count = 0;

        for (int i = 1; i < t.length - 1; i++) {
            int mirror = 2 * center - i;
            if (right > i) {
                p[i] = Math.min(right - i, p[mirror]);
            }

            while (t[i + 1 + p[i]] == t[i - 1 - p[i]]) {
                p[i]++;
            }

            if (i + p[i] > right) {
                center = i;
                right = i + p[i];
            }

            // Each expansion unit corresponds to a valid palindrome substring
            count += (p[i] + 1) / 2;
        }

        return count;
    }
}
```
</details>

### Exercise 2: Predict the Output of String Comparisons

```java
public class StringPuzzle {
    public static void main(String[] args) {
        String a = "hello";
        String b = "hel" + "lo";
        String c = "hel";
        String d = c + "lo";

        System.out.print((a == b) + " ");
        System.out.print((a == d) + " ");
        System.out.print((a == d.intern()) + " ");
    }
}
```

<details>
<summary>Solution</summary>

**Output**: `true false true`

**Explanation**:
1. `a == b`: Both are compile-time constants evaluated by the compiler to the literal `"hello"` in the SCP. Evaluates to `true`.
2. `a == d`: `d` is computed at runtime using variable `c`. It creates a new heap string outside the SCP. `a == d` evaluates to `false`.
3. `a == d.intern()`: `d.intern()` retrieves the canonical pooled reference from the SCP, which is identical to `a`. Evaluates to `true`.
</details>

### Exercise 3: Implement Custom String Deduplicator
Create a memory-bounded thread-safe string deduplicator using a WeakHashMap or ConcurrentHashMap.

<details>
<summary>Solution</summary>

```java
import java.util.concurrent.ConcurrentHashMap;
import java.util.Map;

public class BoundedStringDeduplicator {
    private final Map<String, String> pool = new ConcurrentHashMap<>();

    public String deduplicate(String input) {
        if (input == null) return null;
        // putIfAbsent returns the existing string if present, or null if inserted
        String canonical = pool.putIfAbsent(input, input);
        return canonical != null ? canonical : input;
    }

    public int size() {
        return pool.size();
    }
}
```
</details>

---

## 12. Summary / Cheat Sheet

- **Compact Strings**: Java 9+ stores Latin-1 strings in 1 byte per char via `byte[] value` and `byte coder`.
- **String Constant Pool**: Located in Java Heap; literals are interned automatically.
- **Pass-by-Value**: Strings are immutable; methods cannot modify caller string content.
- **Security Rule**: Use `char[]` for passwords; zero memory via `Arrays.fill()` after use.
- **Loop Concatenation**: Always use a single `StringBuilder`; never concatenate with `+` in loops.

---

## 13. Further Reading

- [JEP 254: Compact Strings (Java 9)](https://openjdk.org/jeps/254)
- [JEP 280: Indify String Concatenation](https://openjdk.org/jeps/280)
- [Baeldung: Guide to the Java String Pool](https://www.baeldung.com/java-string-pool)
- [Aleksey Shipilëv: The Secret Life of Strings](https://shipilev.net/jvm/anatomy-quarks/10-string-intern/)
