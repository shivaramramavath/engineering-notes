# Java String Constant Pool, Interning, and Equality

> A comprehensive deep-dive into the JVM String Constant Pool (`StringTable`), literal deduplication, the semantics of `String.intern()`, and accurate multi-dimensional string comparison.

| Metadata | Details |
| :--- | :--- |
| **Level** | Intermediate |
| **Prerequisites** | [00_string-basics.md](./00_string-basics.md), [01_string-methods.md](./01_string-methods.md) |
| **Time to Read** | 25 minutes |
| **Status** | Active |
| **Java Version** | Java 8+ (StringTable in heap since Java 7; G1 String Deduplication in Java 8u20+ / Java 9+) |

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
9. [Comparison: Reference vs Content vs Natural Ordering](#9-comparison-reference-vs-content-vs-natural-ordering)
10. [Interview Questions](#10-interview-questions)
11. [Practice Exercises](#11-practice-exercises)
12. [Summary & Cheat Sheet](#12-summary--cheat-sheet)
13. [Further Reading](#13-further-reading)

---

## 1. What & Why

Strings are by far the most populated object type on the Java heap, frequently consuming 20% to 40% of an application's total memory. To conserve memory and boost performance, Java implements the **String Constant Pool** (often called the String Intern Pool).

The String Constant Pool is a native hash map maintained by the JVM C++ runtime (`StringTable`). Whenever the JVM encounters a string literal (e.g. `"prod-server"`), it checks whether an identical character sequence is already registered in the table:
- If present, the JVM returns the existing reference.
- If absent, the JVM allocates the `String` object in the heap, records its reference in the pool table, and returns it.

Because string instances are immutable, sharing a single instance across thousands of classes and threads is completely thread-safe and eliminates redundant memory allocations.

---

## 2. Core Concepts

### 2.1 Literals vs Explicit Allocation
```java
String s1 = "java";                  // Enters String Constant Pool
String s2 = "java";                  // Reuses pooled instance: s1 == s2 is TRUE
String s3 = new String("java");      // Forces new instance on heap: s1 == s3 is FALSE
String s4 = s3.intern();             // Returns the pooled instance: s1 == s4 is TRUE
```

### 2.2 Constant Expression Folding
The Java compiler (`javac`) folds constant expressions at compile time:
```java
String base = "hello";
String a = "hello world";
String b = "hello " + "world";       // Constant expression: folded by javac into "hello world"
String c = base + " world";          // NOT folded: computed dynamically at runtime via invokedynamic

System.out.println(a == b);          // true (both point to the same pooled literal)
System.out.println(a == c);          // false (c was constructed on the heap at runtime)
```

If `base` is declared `final`, it becomes a compile-time constant:
```java
final String finalBase = "hello";
String d = finalBase + " world";     // Folded at compile time!
System.out.println(a == d);          // true!
```

---

## 3. How It Works Internally

### 3.1 Memory Location Evolution: PermGen vs Java Heap
In Java 6 and earlier, the String Constant Pool was located in **PermGen** (Permanent Generation):
- PermGen had a fixed maximum size (controlled by `-XX:MaxPermSize=82m`).
- Strings interned via `String.intern()` were rarely garbage collected because PermGen GC only ran during full Stop-The-World collections.
- Calling `.intern()` on untrusted input frequently crashed enterprise servers with `java.lang.OutOfMemoryError: PermGen space`.

Starting in **Java 7** (and continued through modern Java 21/25):
- The String Constant Pool was moved directly into the **regular Java Heap**.
- Interned strings are garbage collected just like any other ordinary heap object when they are no longer reachable from any live thread.

```
JVM Memory Layout (Java 7 through Java 25):
┌─────────────────────────────────────────────────────────────┐
│                          JVM Heap                           │
│  ┌───────────────────────┐       ┌────────────────────────┐ │
│  │   Normal Java Heap    │       │  String Constant Pool  │ │
│  │  ┌─────────────────┐  │       │  ┌──────────────────┐  │ │
│  │  │ new String("A") │  │       │  │   "alpha"        │  │ │
│  │  └─────────────────┘  │       │  │   "beta"         │  │ │
│  │  ┌─────────────────┐  │       │  │   "prod-cluster" │  │ │
│  │  │ dynamic result  │  │       │  └──────────────────┘  │ │
│  │  └─────────────────┘  │                   ▲            │ │
│  └───────────────────────┘                   │            │ │
│                                              │            │ │
│  HotSpot C++ Runtime (Native Memory)         │            │ │
│  ┌────────────────────────────────────────┐  │            │ │
│  │ StringTable (Hashtable of Oop pointers)└──┘            │ │
│  └────────────────────────────────────────────────────────┘ │
└─────────────────────────────────────────────────────────────┘
```

### 3.2 The HotSpot `StringTable` Hash Table
In HotSpot JVM internals, `StringTable` is an open-hash table with fixed bucket chains.
- In Java 7u40+, the default bucket count was 60,013.
- In modern Java 64-bit servers, the bucket size defaults to 65,536 (or higher depending on memory).
- If an application calls `.intern()` on millions of distinct strings without increasing the bucket count via `-XX:StringTableSize=N`, hash collisions cause bucket chains to grow linearly ($O(N)$), degrading JVM performance during interning.

---

## 4. Syntax & Usage

### 4.1 Equality and Comparison APIs
```java
String a = "data";
String b = new String("data");

// 1. Reference Equality (==) -> Compares memory pointers
boolean samePointer = (a == b);                      // false

// 2. Value Equality (.equals) -> Compares byte payloads
boolean sameContent = a.equals(b);                   // true

// 3. Case-Insensitive Equality
boolean sameIgnore  = a.equalsIgnoreCase("DATA");    // true

// 4. Lexicographical Ordering (.compareTo)
// Returns < 0 if a precedes b, 0 if equal, > 0 if a succeeds b
int ordering = a.compareTo("database");              // < 0

// 5. Region Matching (Zero allocation substring comparison)
boolean regionMatches = "prefix_user_key".regionMatches(
    true,         // ignoreCase
    7,            // toffset (start at 'u')
    "USER_TEST",  // other string
    0,            // ooffset
    4             // len ("USER")
); // true
```

---

## 5. Practical Examples

### Example 1: Demonstrating Compile-Time Folding and Interning
```java
public class StringPoolInternDemo {
    public static void main(String[] args) {
        String s1 = "Java25";
        String s2 = "Java" + 25;                       // Folded at compile time
        String s3 = "Java" + String.valueOf(25);       // Evaluated at runtime
        String s4 = s3.intern();                       // Explicit interning

        System.out.println("s1 == s2: " + (s1 == s2)); // true
        System.out.println("s1 == s3: " + (s1 == s3)); // false
        System.out.println("s1 == s4: " + (s1 == s4)); // true
    }
}
```
**Output:**
```text
s1 == s2: true
s1 == s3: false
s1 == s4: true
```

### Example 2: High-Performance Zero-Copy Prefix Matching with `regionMatches()`
```java
public class FastProtocolRouter {
    private static final String API_V1 = "/api/v1/";
    private static final String API_V2 = "/api/v2/";

    public static String route(String path) {
        if (path == null) return "400 BAD REQUEST";

        // Avoids allocating path.substring(0, 8)
        if (path.regionMatches(0, API_V1, 0, API_V1.length())) {
            return "Routing to Legacy Gateway: " + path;
        } else if (path.regionMatches(0, API_V2, 0, API_V2.length())) {
            return "Routing to Modern Cloud Gateway: " + path;
        }
        return "404 NOT FOUND";
    }

    public static void main(String[] args) {
        System.out.println(route("/api/v1/users"));
        System.out.println(route("/api/v2/orders"));
        System.out.println(route("/static/style.css"));
    }
}
```
**Output:**
```text
Routing to Legacy Gateway: /api/v1/users
Routing to Modern Cloud Gateway: /api/v2/orders
404 NOT FOUND
```

### Example 3: Edge Case – Interning Large Dynamic Strings vs Deduplication
```java
import java.util.UUID;

public class InternPitfallDemo {
    public static void main(String[] args) {
        // Interning dynamically generated, non-repeating tokens exhausts native table memory
        for (int i = 0; i < 5; i++) {
            String token = UUID.randomUUID().toString();
            String interned = token.intern();
            System.out.println("Interned distinct token: " + interned);
        }
        System.out.println("Completed without crashing heap, but native table grew!");
    }
}
```
**Output:**
```text
Interned distinct token: ... (5 distinct UUIDs)
Completed without crashing heap, but native table grew!
```

### Example 4: Failure Case – The `null` Equality Trap
```java
public class NullEqualityTrap {
    public static boolean checkStatusBad(String userStatus) {
        // DANGER: Throws NullPointerException if userStatus is null!
        return userStatus.equals("ACTIVE");
    }

    public static boolean checkStatusGood(String userStatus) {
        // SAFE: Literal is never null
        return "ACTIVE".equals(userStatus);
    }

    public static void main(String[] args) {
        try {
            checkStatusBad(null);
        } catch (NullPointerException ex) {
            System.out.println("Caught NPE as expected in checkStatusBad!");
        }

        boolean result = checkStatusGood(null);
        System.out.println("checkStatusGood(null) handled safely: " + result);
    }
}
```
**Output:**
```text
Caught NPE as expected in checkStatusBad!
checkStatusGood(null) handled safely: false
```

### Example 5: Optimized / Idiomatic – Canonical Symbol Table with ConcurrentHashMap
```java
import java.util.concurrent.ConcurrentHashMap;

public class BoundedStringDeduplicator {
    private final ConcurrentHashMap<String, String> cache = new ConcurrentHashMap<>();

    public String deduplicate(String input) {
        if (input == null) return null;
        // Replaces input with existing equal instance if present, else caches input
        return cache.computeIfAbsent(input, key -> key);
    }

    public int size() {
        return cache.size();
    }

    public static void main(String[] args) {
        BoundedStringDeduplicator dedup = new BoundedStringDeduplicator();

        String s1 = new String("TENANT_US_EAST");
        String s2 = new String("TENANT_US_EAST");

        String c1 = dedup.deduplicate(s1);
        String c2 = dedup.deduplicate(s2);

        System.out.println("c1 == c2: " + (c1 == c2)); // Exactly identical reference!
        System.out.println("Cache entries: " + dedup.size()); // Exactly 1
    }
}
```
**Output:**
```text
c1 == c2: true
Cache entries: 1
```

---

## 6. Best Practices

1. **Never Compare Strings with `==`:** Always use `str.equals(target)` or `Objects.equals(a, b)`. `==` only checks memory references and will produce intermittent, hard-to-reproduce bugs when strings originate from I/O or network calls.
2. **Yoda Conditions for Constants:** Write `"APPROVED".equals(status)` instead of `status.equals("APPROVED")`. This avoids `NullPointerException` without cluttering the code with `if (status != null)`.
3. **Avoid Uncontrolled `String.intern()` on External Input:** Do not call `.intern()` on arbitrary user inputs (e.g., query params, HTTP headers, XML tags). Attackers can flood the JVM StringTable with millions of unique strings, causing hash collision storms and CPU starvation.
4. **Prefer G1/ZGC String Deduplication:** Instead of writing manual `.intern()` calls, run your application with `-XX:+UseStringDeduplication`. The GC automatically deduplicates identical backing `byte[]` arrays in the background.
5. **Use `regionMatches()` to Avoid Substring Allocations:** When validating protocol headers or parsing prefixes/suffixes, use `str.regionMatches()` to test substrings without allocating temporary slice objects.

---

## 7. Common Mistakes & Pitfalls

| Mistake | Why It Is Wrong | Correct Idiom |
| :--- | :--- | :--- |
| `if (str == "admin")` | Only returns true if `str` happens to point to the exact pooled literal; fails for dynamic inputs. | `if ("admin".equals(str))` |
| Unbounded `intern()` | Floats unique strings into the JVM StringTable hash table forever, bloating native table memory. | Use application-level bounded LRU cache or `-XX:+UseStringDeduplication`. |
| `compareTo() == -1` | `compareTo()` is only specified to return negative, zero, or positive; it is NOT guaranteed to return `-1`. | `if (a.compareTo(b) < 0)` |
| `var.equals("A")` without null check | Throws `NullPointerException` if `var` is null. | `"A".equals(var)` or `Objects.equals(var, "A")` |
| Calling `.intern()` on already pooled literals | Redundant no-op overhead; `"text".intern()` simply returns `"text"`. | Avoid calling `.intern()` on literals. |
| Forgetting case differences in equality | `.equals()` is strictly case-sensitive. `"true".equals("TRUE")` is false. | Use `.equalsIgnoreCase()` or `Boolean.parseBoolean()`. |

---

## 8. Performance & Security Notes

### 8.1 Tuning the StringTable
If your application legitimately interns tens of thousands of domain entities (e.g. ISO country codes, currency symbols, airport codes), tune the StringTable bucket count at JVM launch:
```bash
java -XX:StringTableSize=1000003 -XX:+PrintStringTableStatistics -jar server.jar
```
Using a large prime number for `-XX:StringTableSize` prevents hash bucket clustering and maintains $O(1)$ table lookup speeds.

### 8.2 Hash Denial-of-Service (HashDoS)
Never intern user-supplied string data directly. If an adversary crafts thousands of distinct strings that produce identical 32-bit hash codes (a known cryptographic property of Java's polynomial $s[0]\cdot 31^{n-1} + \dots + s[n-1]$ hash algorithm), lookups in the internal `StringTable` degrade from $O(1)$ to $O(N)$, causing complete CPU saturation.

---

## 9. Comparison: Reference vs Content vs Natural Ordering

| Comparison Strategy | Syntax | What It Tests | Performance | Null-Safe? |
| :--- | :--- | :--- | :--- | :--- |
| **Reference Equality** | `a == b` | Pointer address equality | $O(1)$ (single instruction) | ✅ Yes |
| **Content Equality** | `a.equals(b)` | Identical byte characters | $O(N)$ (early exit on mismatch) | ❌ If caller null |
| **Safe Content Equality**| `Objects.equals(a, b)` | Identical byte characters | $O(N)$ | ✅ Yes |
| **Case-Insensitive** | `a.equalsIgnoreCase(b)` | Case-folded character match | $O(N)$ | ❌ If caller null |
| **Lexicographical** | `a.compareTo(b)` | Dictionary ordering | $O(N)$ | ❌ If caller null |
| **Prefix / Region** | `a.regionMatches(...)` | Substring match without copy | $O(K)$ ($K =$ region length) | ❌ If caller null |

---

## 10. Interview Questions

<details>
<summary><strong>1. What is the difference between a String literal and a String created via new String()?</strong></summary>

A String literal (e.g. `"data"`) is resolved at class load time and interned into the JVM's String Constant Pool. If another class references the identical literal, both reference the exact same memory address. In contrast, `new String("data")` allocates a new distinct `String` object on the heap, bypassing the pool. While both have identical characters (`s1.equals(s2)` is true), their object references differ (`s1 == s2` is false).
</details>

<details>
<summary><strong>2. Where is the String Constant Pool stored in modern Java?</strong></summary>

Prior to Java 7, the String Constant Pool was stored in the **PermGen** (Permanent Generation) space of native JVM memory, which had a fixed size and could cause `OutOfMemoryError: PermGen space`. Since Java 7 and throughout modern Java 21/25, the pool resides in the **main Java Heap**, allowing unused interned strings to be reclaimed by standard garbage collection cycles.
</details>

<details>
<summary><strong>3. What does String.intern() return, and how does it work?</strong></summary>

When `.intern()` is called on a `String` instance:
1. The JVM queries the native `StringTable` hash table for an entry whose character contents equal this string.
2. If found, the reference to that existing pooled `String` is returned.
3. If not found, this `String` reference is registered in the pool and returned.
Thus, for any two strings `s` and `t`, `s.intern() == t.intern()` is guaranteed to be `true` if and only if `s.equals(t)`.
</details>

<details>
<summary><strong>4. Why should you avoid using String.intern() on arbitrary external user input?</strong></summary>

The JVM `StringTable` is an internal JVM hash table. Interning millions of unique strings from HTTP requests or external files pollutes native memory, causes severe hash bucket chaining, and triggers high CPU overhead. Furthermore, it opens the application to HashDoS attacks where malicious inputs cause catastrophic bucket collisions.
</details>

<details>
<summary><strong>5. How does compile-time constant folding affect String literals?</strong></summary>

The Java compiler (`javac`) automatically evaluates expressions where all operands are compile-time constants (such as `"a" + "b"` or `"val" + 42` or concatenating variables declared `final String`). The compiler computes the concatenated result and stores the final combined string in the class file's constant pool, resulting in zero runtime concatenation overhead.
</details>

<details>
<summary><strong>6. What is the JVM option -XX:+UseStringDeduplication and how does it work?</strong></summary>

Introduced for G1 GC in Java 8u20 and expanded to modern garbage collectors (including ZGC and Shenandoah), `-XX:+UseStringDeduplication` inspects `String` objects during GC passes. If the collector identifies two distinct `String` instances with identical backing byte arrays (`byte[] value`), it points both strings to the same array and marks the duplicate array for collection. This achieves memory savings comparable to `.intern()` without modifying code and without polluting the `StringTable`.
</details>

<details>
<summary><strong>7. Why is it dangerous to write if (str.compareTo(other) == -1)?</strong></summary>

The specification for `Comparable.compareTo()` states that the method returns a negative integer, zero, or a positive integer. It does **not** guarantee returning `-1` for less-than. For example, comparing `'a'` to `'c'` may return `-2`. Checking `== -1` will cause subtle bugs. The correct check is `str.compareTo(other) < 0`.
</details>

<details>
<summary><strong>8. What is the difference between str.equals() and str.contentEquals()?</strong></summary>

`equals()` only returns true if the argument is another `java.lang.String` instance with matching characters.
`contentEquals(CharSequence cs)` accepts any implementation of `CharSequence` (including `StringBuilder`, `StringBuffer`, and `CharBuffer`) and compares character contents without requiring the argument to be a `String`.
</details>

---

## 11. Practice Exercises

### Easy
1. **Pooled Identity Test:** Predict the exact boolean outputs of 6 different combinations of literal and dynamic string concatenations with `==` and `.equals()`.
2. **Safe Equals Wrapper:** Write a null-safe method `boolean areEqual(String a, String b)` without using `java.util.Objects`.
3. **Case-Insensitive Comparator:** Implement a `Comparator<String>` that sorts strings alphabetically ignoring case, but breaks ties using case-sensitive ordering.

### Medium
4. **Custom Intern Pool:** Build a memory-bounded `StringPool` class backed by `WeakReference` and a thread-safe cache to emulate `String.intern()` safely.
5. **Zero-Allocation Token Matcher:** Given a raw HTTP header line `"Authorization: Bearer xyz"`, determine if it starts with `"Authorization"` using `regionMatches()` without creating any substrings.
6. **Lexicographical Sort:** Implement quicksort for an array of strings using `String.compareTo()` and analyze its time complexity on identical prefixes.

### Hard
7. **StringTable Collision Simulator:** Construct a generator that creates distinct Java strings that share identical 32-bit `hashCode()` values and observe their lookup performance.
8. **Off-Heap String Deduplicator:** Write an off-heap string dictionary using Java 22+ Foreign Function & Memory (FFM) API that stores deduplicated UTF-8 byte buffers.
9. **Garbage Collection Deduplication Benchmark:** Write a test that allocates 100,000 duplicate strings and measures heap consumption with and without `-XX:+UseStringDeduplication`.

<details>
<summary><strong>Exercise Solutions</strong></summary>

```java
// Solution to Exercise 2: Safe Equals
public static boolean areEqual(String a, String b) {
    if (a == b) return true; // Covers both null or same reference
    if (a == null || b == null) return false;
    return a.equals(b);
}

// Solution to Exercise 4: Safe Weak String Pool
import java.lang.ref.WeakReference;
import java.util.WeakHashMap;

public class SafeStringPool {
    private final WeakHashMap<String, WeakReference<String>> pool = new WeakHashMap<>();

    public synchronized String intern(String s) {
        if (s == null) return null;
        WeakReference<String> ref = pool.get(s);
        if (ref != null) {
            String existing = ref.get();
            if (existing != null) return existing;
        }
        pool.put(s, new WeakReference<>(s));
        return s;
    }
}
```
</details>

---

## 12. Summary & Cheat Sheet

```
┌────────────────────────────────────────────────────────────────────────┐
│                   STRING POOL & EQUALITY CHEAT SHEET                   │
├────────────────────────────────────────────────────────────────────────┤
│ Literal Storage:       JVM String Constant Pool (in main Java heap)   │
│ Identity vs Content:   == checks reference addresses                   │
│                        .equals() checks character content              │
│ Literal equality:      "a" == "a" is always true                       │
│ Heap equality:         new String("a") == "a" is false                 │
│ Compile-time folding:  "a" + "b" is folded into "ab" by javac          │
│ Runtime concat:        dynamicStr + "b" allocates new String on heap   │
│ String.intern():       Returns reference from JVM StringTable          │
│ Safe constant check:   "CONSTANT".equals(userInput) (Prevents NPE)     │
│ Zero-copy matching:    str.regionMatches(ignoreCase, toff, o, ooff, len│
│ Deduplication:         -XX:+UseStringDeduplication (GC-level dedupl.)  │
└────────────────────────────────────────────────────────────────────────┘
```

---

## 13. Further Reading
- [JEP 192: String Deduplication in G1](https://openjdk.org/jeps/192)
- HotSpot JVM Internals: `src/hotspot/share/classfile/stringTable.cpp`
- *Effective Java (3rd Edition)*, Joshua Bloch, Item 10: "Obey the general contract when overriding equals".
