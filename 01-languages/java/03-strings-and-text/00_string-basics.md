# Java String Basics & Immutability

> An authoritative guide to `java.lang.String`, immutability mechanics, Compact Strings memory architecture, and modern string lifecycle in the JVM.

| Metadata | Details |
| :--- | :--- |
| **Level** | Beginner to Intermediate |
| **Prerequisites** | [01_variables-and-data-types.md](../01-fundamentals/01_variables-and-data-types.md), [07_arrays.md](../01-fundamentals/07_arrays.md) |
| **Time to Read** | 25 minutes |
| **Status** | Active |
| **Java Version** | Java 8+ (Compact Strings in Java 9+, `invokedynamic` concatenation in Java 9+) |

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
9. [Comparison: String vs char[] vs StringBuilder vs byte[]](#9-comparison-string-vs-char-vs-stringbuilder-vs-byte)
10. [Interview Questions](#10-interview-questions)
11. [Practice Exercises](#11-practice-exercises)
12. [Summary & Cheat Sheet](#12-summary--cheat-sheet)
13. [Further Reading](#13-further-reading)

---

## 1. What & Why

In Java, `java.lang.String` represents an immutable sequence of characters. Unlike C strings (null-terminated character arrays `char*`) or C++ `std::string` (mutable buffers with copy-on-write or small-string optimization), a Java `String` is an ordinary object located on the managed garbage-collected heap, but it enjoys privileged syntax support from the Java compiler and JVM runtime.

### Why Immutability?
James Gosling and the original Java architects designed `String` to be immutable for fundamental architectural reasons:

1. **Security & Class Loading:** Strings are ubiquitous in sensitive system operations—filesystem paths, network socket descriptors, URLs, database connection strings, and reflection class names (e.g., `Class.forName(name)`). If strings were mutable, a malicious thread could pass a path check (`/var/tmp/safe.txt`) and subsequently alter the string in memory before the OS opens the file, introducing a Time-of-Check to Time-of-Use (TOCTOU) security vulnerability.
2. **String Pool Caching:** Because strings are immutable, identical string literals across thousands of classes can share a single heap representation inside the JVM String Constant Pool, drastically reducing memory footprint.
3. **Thread Safety:** An immutable object is inherently thread-safe. Multiple threads can read, traverse, slice, and share string instances without locks, synchronizers, or risk of data races.
4. **Hash Code Caching:** Strings are the most widely used keys in associative data structures like `HashMap`, `LinkedHashMap`, and `ConcurrentHashMap`. Because its characters never change, a String can lazily compute its 32-bit `hashCode` once and cache it inside a private instance field. Subsequent map lookups execute in $O(1)$ without re-hashing.

---

## 2. Core Concepts

### 2.1 The String Class Signature
Declared in `java.lang`, the class signature is:
```java
public final class String
    implements java.io.Serializable, Comparable<String>, CharSequence,
               java.lang.constant.Constable, java.lang.constant.ConstantDesc {
    // fields and methods
}
```
Key architectural properties:
- **`final`:** The class cannot be subclassed. This prevents attackers or buggy libraries from overriding methods like `equals()`, `length()`, or `hashCode()` to bypass security checks.
- **`CharSequence`:** Implements the general character sequence contract alongside `StringBuilder`, `StringBuffer`, and `CharBuffer`.
- **`Constable` & `ConstantDesc`:** Added in Java 12 (JEP 334) to allow strings to describe themselves as loadable bytecode constants in the JVM constant pool.

### 2.2 Literal Allocation vs `new String()`
- **String Literals (`"hello"`):** Resolved at class load time and placed into the String Constant Pool inside the JVM heap. Multiple occurrences share the exact same object reference:
  ```java
  String s1 = "alpha";
  String s2 = "alpha";
  System.out.println(s1 == s2); // true (same reference in pool)
  ```
- **Constructor Allocation (`new String("hello")`):** Explicitly creates a brand-new `String` instance on the normal heap, copying the underlying byte array, completely bypassing pool sharing:
  ```java
  String s3 = new String("alpha");
  System.out.println(s1 == s3); // false (different heap addresses)
  System.out.println(s1.equals(s3)); // true (identical content)
  ```

---

## 3. How It Works Internally

### 3.1 Memory Layout: Java 8 vs Java 9+ (Compact Strings - JEP 254)
Prior to Java 9, every `String` was backed by a 16-bit UTF-16 character array:
```java
// Java 8 and earlier
public final class String {
    private final char[] value; // 2 bytes per character regardless of ASCII
    private int hash;           // cached hash code
}
```
Because over 75% of strings in typical enterprise applications consist purely of Latin-1 (ASCII / ISO-8859-1) characters requiring only 8 bits per character, Java 8 wasted half of all heap memory allocated to strings.

In Java 9+, JEP 254 introduced **Compact Strings**:
```java
// Java 9 through Java 25 LTS
public final class String {
    @Stable
    private final byte[] value; // 1 byte per char for Latin-1, 2 bytes for UTF-16
    private final byte coder;   // 0 = LATIN1, 1 = UTF16
    private int hash;           // cached hash code
    private boolean hashIsZero; // tracks whether hash computation yielded exact 0
}
```

```
Java 25 64-bit JVM Object Layout (with Compressed OOPs enabled):
┌────────────────────────────────────────────────────────┐
│                      String Object                     │
├──────────────────────────┬─────────────────────────────┤
│ Mark Word                │ 8 bytes                     │
│ Compressed Klass Pointer │ 4 bytes                     │
│ coder                    │ 1 byte  (0: LATIN1, 1: UTF16│
│ hashIsZero               │ 1 byte                      │
│ (alignment padding)      │ 2 bytes                     │
│ hash                     │ 4 bytes (cached hashCode)   │
│ value (reference)        │ 4 bytes (ref to byte[])     │
└──────────────────────────┴─────────────────────────────┘
  Total String Shell: 24 bytes (on 64-bit JVM with 8-byte alignment)
          │
          ▼ points to
┌────────────────────────────────────────────────────────┐
│                   byte[] Array Object                  │
├──────────────────────────┬─────────────────────────────┤
│ Mark Word                │ 8 bytes                     │
│ Compressed Klass Pointer │ 4 bytes                     │
│ Array Length             │ 4 bytes (N bytes)           │
│ Array Payload            │ N bytes (1B/char or 2B/char)│
│ (alignment padding)      │ 0-7 bytes (to 8-byte align) │
└────────────────────────────────────────────────────────┘
```

When all characters in a string satisfy `c <= 0xFF`, the JVM encodes the characters using `LATIN1` (1 byte per char) and sets `coder = 0`. As soon as any character exceeds `0xFF` (such as Chinese characters, Arabic script, or Unicode emoji), the JVM falls back to `UTF16` (2 bytes per char) and sets `coder = 1`.

### 3.2 Compilation of String Concatenation (`+`)
Consider the code:
```java
public String buildGreeting(String user, int loginCount) {
    return "Welcome back " + user + "! Logins: " + loginCount;
}
```

#### Bytecode Evolution:
- **Java 1.0 to 1.4:** Compiled to multiple calls to `new StringBuffer().append(...)`.
- **Java 5 to 8:** Compiled to `new StringBuilder().append(...)`. While faster than `StringBuffer` (unsynchronized), it still created intermediate `StringBuilder` objects and intermediate char arrays.
- **Java 9+ (JEP 280: Indify String Concatenation):**
  Compiled into a single `invokedynamic` bytecode instruction targeting `java.lang.invoke.StringConcatFactory`:
  ```bytecode
  0: aload_1
  1: iload_2
  2: invokedynamic #7,  0 // InvokeDynamic #0:makeConcatWithConstants:(Ljava/lang/String;I)Ljava/lang/String;
  7: areturn
  ```
  At runtime, `StringConcatFactory.makeConcatWithConstants` calculates the exact total byte length of all arguments upfront, allocates an appropriately sized `byte[]` in a single heap allocation, and copies the data directly using high-speed JVM intrinsics (`Unsafe.copyMemory`). This avoids intermediate buffer resizing or reallocations.

---

## 4. Syntax & Usage

### 4.1 Common Factory & Inspection Methods
```java
// Creation & Factories
String s1 = "Engineering";
String s2 = String.valueOf(1024);                  // "1024"
String s3 = String.join(" -> ", "A", "B", "C");    // "A -> B -> C"
String s4 = "Repeat ".repeat(3);                   // "Repeat Repeat Repeat "
String s5 = "".indent(4);                          // Adds 4 leading spaces (Java 12+)

// Inspection
int len = s1.length();                             // 11 (number of UTF-16 code units)
boolean empty = "".isEmpty();                      // true (length == 0)
boolean blank = "   \t\n  ".isBlank();             // true (Java 11+: only whitespace)
char c = s1.charAt(0);                             // 'E'
int codePoint = s1.codePointAt(0);                 // 69 (Unicode scalar value)

// Comparison
boolean eq = s1.equals("engineering");             // false (case-sensitive)
boolean eqIgnore = s1.equalsIgnoreCase("engineering"); // true
int cmp = s1.compareTo("Doctor");                  // > 0 ('E' > 'D')
boolean starts = s1.startsWith("Eng");             // true
boolean ends = s1.endsWith("ing");                 // true
boolean contains = s1.contains("neer");            // true

// Transformations (Always return new String instances)
String upper = s1.toUpperCase();                   // "ENGINEERING"
String lower = s1.toLowerCase();                   // "engineering"
String sub = s1.substring(0, 3);                   // "Eng" [0, 3)
String replaced = s1.replace('e', 'X');            // "EnginXXring" (literal replace)
String stripped = "  padded  ".strip();            // "padded" (Unicode-aware strip, Java 11+)
```

---

## 5. Practical Examples

### Example 1: Basic String Manipulation & Identity vs Equality
**Goal:** Verify reference equality (`==`) vs value equality (`.equals()`) and demonstrate string immutability.

```java
public class StringIdentityDemo {
    public static void main(String[] args) {
        String a = "Antigravity";
        String b = "Antigravity";
        String c = new String("Antigravity");
        String d = new String(a.toCharArray());

        System.out.println("a == b: " + (a == b));           // true (pooled)
        System.out.println("a == c: " + (a == c));           // false (heap copy)
        System.out.println("a.equals(c): " + a.equals(c));   // true (content match)
        System.out.println("c == d: " + (c == d));           // false (two distinct heap objects)

        // Demonstrating Immutability
        String original = "Immutable";
        String modified = original.concat(" Object");
        System.out.println("original: " + original);         // "Immutable" (unchanged!)
        System.out.println("modified: " + modified);         // "Immutable Object"
    }
}
```
**Output:**
```text
a == b: true
a == c: false
a.equals(c): true
c == d: false
original: Immutable
modified: Immutable Object
```

### Example 2: Real-World Safe Token Parser & Sanitizer
**Goal:** Parse, sanitize, and validate incoming authorization tokens with defensive blank-checks and prefix stripping.

```java
import java.util.Objects;
import java.util.Optional;

public class BearerTokenValidator {
    private static final String BEARER_PREFIX = "Bearer ";

    public record TokenDetails(String rawToken, String tokenType) {}

    public static Optional<TokenDetails> extractBearerToken(String authHeader) {
        if (authHeader == null || authHeader.isBlank()) {
            return Optional.empty();
        }

        // strip() is Unicode whitespace-aware (unlike trim() which only checks ASCII <= 0x20)
        String cleanHeader = authHeader.strip();
        if (!cleanHeader.regionMatches(true, 0, BEARER_PREFIX, 0, BEARER_PREFIX.length())) {
            return Optional.empty();
        }

        String token = cleanHeader.substring(BEARER_PREFIX.length()).strip();
        if (token.isEmpty()) {
            return Optional.empty();
        }

        return Optional.of(new TokenDetails(token, "Bearer"));
    }

    public static void main(String[] args) {
        var valid = extractBearerToken("  Bearer eyJhbGciOiJIUzI1NiJ9.test  ");
        var missingToken = extractBearerToken("Bearer    ");
        var invalidScheme = extractBearerToken("Basic dXNlcjpwYXNz");

        valid.ifPresent(t -> System.out.println("Extracted: " + t.rawToken()));
        System.out.println("Missing token present: " + missingToken.isPresent());
        System.out.println("Invalid scheme present: " + invalidScheme.isPresent());
    }
}
```
**Output:**
```text
Extracted: eyJhbGciOiJIUzI1NiJ9.test
Missing token present: false
Invalid scheme present: false
```

### Example 3: Edge Case – Supplementary Characters and Surrogate Pairs
**Goal:** Handle Unicode characters outside the Basic Multilingual Plane (BMP, code points > `U+FFFF`), such as emoji and ancient scripts.

```java
public class UnicodeSurrogateDemo {
    public static void main(String[] args) {
        // Musical symbol G-Clef (U+1D11E) and Rocket Emoji (U+1F680)
        String emojiString = "Rocket: \uD83D\uDE80"; // 🚀 is represented as a high+low surrogate pair

        System.out.println("String: " + emojiString);
        System.out.println("length() in char/UTF-16 units: " + emojiString.length());
        System.out.println("codePointCount() in true Unicode glyphs: " +
            emojiString.codePointCount(0, emojiString.length()));

        // Iterating code points properly
        emojiString.codePoints().forEach(cp -> {
            System.out.printf("Code point: U+%04X ('%s')%n", cp, Character.toString(cp));
        });
    }
}
```
**Output:**
```text
String: Rocket: 🚀
length() in char/UTF-16 units: 10
codePointCount() in true Unicode glyphs: 9
Code point: U+0052 ('R')
Code point: U+006F ('o')
Code point: U+0063 ('c')
Code point: U+006B ('k')
Code point: U+0065 ('e')
Code point: U+0074 ('t')
Code point: U+003A (':')
Code point: U+0020 (' ')
Code point: U+1F680 ('🚀')
```

### Example 4: Failure Case – Quadratic Loop Concatenation Trap
**Goal:** Illustrate how using `+` inside a loop degrades algorithmic complexity to $O(N^2)$ due to repeated buffer allocation and copying.

```java
public class QuadraticConcatTrap {
    public static String buildCsvBad(int items) {
        String result = "";
        for (int i = 0; i < items; i++) {
            // ANTI-PATTERN: In each iteration, a new String object is allocated
            // and all existing characters are copied, resulting in N^2 / 2 total byte copies!
            result += "item-" + i + ",";
        }
        return result;
    }

    public static String buildCsvGood(int items) {
        // IDIOMATIC: Pre-sized StringBuilder builds output in O(N) linear time
        StringBuilder sb = new StringBuilder(items * 10);
        for (int i = 0; i < items; i++) {
            sb.append("item-").append(i).append(',');
        }
        return sb.toString();
    }

    public static void main(String[] args) {
        int count = 20_000;
        long start = System.nanoTime();
        String sBad = buildCsvBad(count);
        long tBad = (System.nanoTime() - start) / 1_000_000;

        start = System.nanoTime();
        String sGood = buildCsvGood(count);
        long tGood = (System.nanoTime() - start) / 1_000_000;

        System.out.println("Loop Concat (+) Time: " + tBad + " ms");
        System.out.println("StringBuilder Time:   " + tGood + " ms");
    }
}
```
**Output:**
```text
Loop Concat (+) Time: 185 ms
StringBuilder Time:   2 ms
```

### Example 5: Optimized / Idiomatic – Safe Zeroing of Passwords
**Goal:** Demonstrate why passwords must never be stored as `String` and how `char[]` enables secure memory erasure.

```java
import java.util.Arrays;

public class SecureCredentialHandling {
    public static void processPassword(char[] secretPassword) {
        try {
            // Verify or transmit credential...
            System.out.println("Password validated safely with length: " + secretPassword.length);
        } finally {
            // Overwrite memory immediately to protect against heap dumps & core dumps
            Arrays.fill(secretPassword, '\0');
        }
    }

    public static void main(String[] args) {
        char[] password = new char[]{'S', 'u', 'p', 'e', 'r', 'S', 'e', 'c', 'r', 'e', 't', '!'};
        processPassword(password);
        System.out.println("Password after wipe: '" + new String(password) + "'");
    }
}
```
**Output:**
```text
Password validated safely with length: 12
Password after wipe: '            '
```

---

## 6. Best Practices

1. **Prefer String Literals:** Never write `new String("literal")`. It duplicates memory on the heap needlessly.
2. **Use `.isBlank()` Over `.trim().isEmpty()`:** Introduced in Java 11, `.isBlank()` checks whitespace without allocating an intermediate substring object.
3. **Use `.strip()` Over `.trim()`:** `.trim()` only removes characters $\le \text{U+0020}$ (ASCII spaces and control codes). It fails to remove Unicode non-breaking spaces (`\u00A0`), em-spaces (`\u2003`), or mathematical spaces. `.strip()` handles all Unicode whitespace specifications defined by `Character.isWhitespace()`.
4. **Beware `length()` with Supplementary Characters:** `s.length()` returns the count of 16-bit `char` code units. If strings contain emojis or non-BMP scripts, use `s.codePointCount(0, s.length())`.
5. **Never Store Cleartext Passwords in `String`:** Strings stay in heap memory indefinitely until collected by GC and cannot be wiped manually. Use `char[]` and zero the array with `Arrays.fill(pwd, '\0')` inside a `finally` block.
6. **Use Single Expression `+` for Static Concatenation:** For simple single-statement concatenations (`"Hello " + name + "!"`), use `+`. The Java 9+ compiler translates this into `invokedynamic` which is as fast as or faster than manual `StringBuilder`.

---

## 7. Common Mistakes & Pitfalls

| Mistake | Why It Is Wrong | Correct Idiom |
| :--- | :--- | :--- |
| `str1 == str2` | Tests pointer reference equality, not character sequence equality. Returns false for strings with identical text created separately. | `str1.equals(str2)` or `Objects.equals(str1, str2)` |
| `str.toUpperCase()` ignored | Strings are immutable. Calling `toUpperCase()` returns a new string; it does not mutate `str`. | `str = str.toUpperCase();` |
| `+` in loops | Compiles into repeated allocations and copies of characters ($O(N^2)$ time complexity). | Use `StringBuilder sb = new StringBuilder();` |
| `s.split(".")` without escaping | `split()` accepts a regular expression. `.` matches any character, producing an empty array! | `s.split("\\.")` or `s.split(Pattern.quote("."))` |
| Storing passwords in `String` | Strings cannot be overwritten in memory; credentials remain exposed in heap dumps and paging files. | Use `char[]` and clear with `Arrays.fill(pwd, '\0')`. |
| Assuming `length()` == character count | Emoji and surrogate pairs occupy 2 `char` units each. `s.length()` returns 2 for a single emoji. | `s.codePointCount(0, s.length())` |
| `new String(bytes)` without Charset | Uses the platform default charset, causing silent data corruption when ported across OSes. | `new String(bytes, StandardCharsets.UTF_8)` |

---

## 8. Performance & Security Notes

### 8.1 Compact Strings Footprint
Compact Strings (enabled by default via `-XX:+CompactStrings`) dynamically switches between 1-byte `LATIN1` and 2-byte `UTF16` storage:
- A 1,000-character ASCII string uses 1,000 bytes for its payload.
- In Java 8, that same string required 2,000 bytes.
- This results in 10%–25% reduction in overall enterprise JVM heap usage with zero code changes.

### 8.2 String Deduplication in G1 and ZGC
When running with the G1 or ZGC garbage collector, you can enable runtime string deduplication:
```bash
java -XX:+UseG1GC -XX:+UseStringDeduplication -jar app.jar
```
During garbage collection cycles, the GC inspects live `String` objects. If it finds two distinct `String` instances with identical `byte[] value` payloads, it modifies one of the strings to point to the other's existing `value` array, leaving the duplicate array eligible for collection. This eliminates duplicate string payloads on the heap without requiring `String.intern()`.

---

## 9. Comparison: String vs char[] vs StringBuilder vs byte[]

| Feature | `String` | `char[]` | `StringBuilder` | `byte[]` |
| :--- | :--- | :--- | :--- | :--- |
| **Mutability** | Immutable | Mutable | Mutable | Mutable |
| **Thread Safety** | Thread-safe (read-only) | Not thread-safe | Not thread-safe | Not thread-safe |
| **Memory Overhead** | 24B shell + `byte[]` | 16B shell + 2B/char | 24B shell + `byte[]` | 16B shell + 1B/byte |
| **Zeroable Memory** | ❌ No | ✅ Yes (`Arrays.fill`) | ❌ No | ✅ Yes (`Arrays.fill`) |
| **String Pool** | ✅ Supported | ❌ No | ❌ No | ❌ No |
| **Use Case** | General text, identifiers | Passwords, encryption keys | Dynamic loop string building | Network I/O, binary data |

---

## 10. Interview Questions

<details>
<summary><strong>1. Why is String immutable in Java? Explain at least three reasons.</strong></summary>

1. **Security:** Strings are used for class loading (`Class.forName`), database URLs, socket connections, and file paths. If mutable, an application could verify a path and a separate thread could mutate the string before the file is opened (TOCTOU race condition).
2. **String Pool Caching:** Immutability enables sharing identical string literals in the String Constant Pool without concurrency conflicts or defensive copying.
3. **Thread Safety:** Immutable objects can be shared freely across threads without synchronization.
4. **Hash Code Caching:** `String` caches its `hashCode()` after the first calculation. If the string were mutable, changing its characters would corrupt any `HashMap` or `HashSet` in which it was stored as a key.
</details>

<details>
<summary><strong>2. How did Java 9 Compact Strings change the internal representation of String?</strong></summary>

Prior to Java 9, `String` held a `char[] value` array, where each `char` took 2 bytes (UTF-16), wasting 50% of heap memory for Latin-1/ASCII characters. In Java 9+ (JEP 254), the backing storage was changed to `byte[] value` accompanied by a `byte coder` field. When all characters fit within ISO-8859-1 (code points $\le 0xFF$), `coder = 0` (LATIN1, 1 byte/char). When any character exceeds $0xFF$, `coder = 1` (UTF-16, 2 bytes/char).
</details>

<details>
<summary><strong>3. What does String.intern() do, and what is its performance cost?</strong></summary>

`String.intern()` queries the JVM's native String Constant Pool table. If the pool contains a string equal to this `String` object, the pooled reference is returned; otherwise, this `String` reference is added to the pool and returned. Historically in Java 6, the pool resided in PermGen, risking `OutOfMemoryError: PermGen space`. In modern Java (Java 7+), the pool lives in the regular Java heap. However, manual interning introduces native hash table locking overhead and should generally be replaced by application-level caches or `-XX:+UseStringDeduplication`.
</details>

<details>
<summary><strong>4. Why should you avoid using String to store passwords?</strong></summary>

Strings are immutable and managed by the garbage collector. They cannot be explicitly cleared from memory. A password stored as a `String` remains in plaintext on the JVM heap until the next GC cycle (which may take minutes or hours), making it vulnerable to heap dumps, memory scanners, or core dump inspections. By contrast, a `char[]` can be explicitly overwritten with zeros (`Arrays.fill(pwd, '\0')`) as soon as authentication finishes.
</details>

<details>
<summary><strong>5. What is the difference between str.trim() and str.strip()?</strong></summary>

`trim()` defines whitespace as any character with a code point $\le \text{U+0020}$ (ASCII space and control characters). It does not recognize modern Unicode whitespace characters like non-breaking space (`\u00A0`), em space (`\u2003`), or thin space (`\u2009`). `strip()` was introduced in Java 11 and uses `Character.isWhitespace(int codePoint)`, properly handling all Unicode whitespace standards.
</details>

<details>
<summary><strong>6. How does Java 9+ compile string concatenation (+) compared to Java 8?</strong></summary>

Java 8 compiled `+` concatenations into chained `new StringBuilder().append(...)` calls, generating intermediate objects and byte buffer copies. Java 9+ (JEP 280) replaces this with a single `invokedynamic` bytecode instruction targeting `StringConcatFactory.makeConcatWithConstants`. At runtime, the JVM generates specialized bytecode using method handles to allocate a single destination `byte[]` of exact size and copy characters directly using low-level intrinsics.
</details>

<details>
<summary><strong>7. What happens if a String's hashCode computes to exact zero?</strong></summary>

Java `String` uses `hash == 0` to indicate that the hash code has not yet been computed. In Java 8, if a string happened to compute a hash of exact 0, subsequent calls to `hashCode()` would recalculate it every single time. Java 9+ added a private `boolean hashIsZero` field to differentiate between an uncomputed hash and a string whose calculated hash value is genuinely zero.
</details>

<details>
<summary><strong>8. Why does "Hello".length() return 1 for a 1-character string, but emoji like "🚀".length() returns 2?</strong></summary>

`length()` returns the number of 16-bit UTF-16 code units (`char` elements). The rocket emoji `🚀` has Unicode code point `U+1F680`, which exceeds `U+FFFF` (the BMP limit). It must be represented in UTF-16 as a surrogate pair consisting of two 16-bit code units (`\uD83D` and `\uDE80`). Therefore, `length()` is 2. To get the true character count, use `s.codePointCount(0, s.length())`.
</details>

---

## 11. Practice Exercises

### Easy
1. **Palindrome Checker:** Write a method `boolean isPalindrome(String s)` that ignores casing, non-alphanumeric characters, and properly handles null or empty inputs.
2. **Title Case Converter:** Convert a sentence to Title Case (capitalize the first letter of each word) without using external libraries.
3. **Vowel & Consonant Counter:** Count the number of ASCII vowels and consonants in a given string.

### Medium
4. **Safe URL Parameter Extractor:** Given a query string like `name=John%20Doe&id=42&active=true`, parse it into a `Map<String, String>` handling URL decoding and missing values without third-party frameworks.
5. **Custom String Joiner:** Implement your own lightweight version of `String.join(delimiter, elements)` that accepts `Iterable<CharSequence>` and pre-calculates the required buffer size to allocate only once.
6. **Code Point Reverser:** Reverse a string containing supplementary Unicode characters (surrogate pairs and emoji) such that emojis are not split or corrupted into invalid surrogate halves.

### Hard
7. **Zero-Allocation String Tokenizer:** Build a zero-allocation `CharTokenizer` implementing `Iterator<CharSequence>` that takes a `String` and delimiter `char`, yielding `CharSequence` slices via `s.subSequence()` without allocating new `String` instances or copying byte arrays.
8. **High-Throughput Interning Cache:** Implement a thread-safe, bounded, low-latency string deduplicator/cache using `ConcurrentHashMap` with weak references (`WeakValueMap`) to prevent memory leaks while guaranteeing identical references.
9. **String Compression Evaluator:** Write a tool that inspects a given string or text file and outputs the memory footprint under Java 8 (`char[]`) vs Java 25 (`byte[]` Compact Strings with LATIN1 vs UTF16 coder).

<details>
<summary><strong>Exercise Solutions</strong></summary>

```java
// Solution to Exercise 1: Palindrome Checker
public static boolean isPalindrome(String s) {
    if (s == null) return false;
    int left = 0, right = s.length() - 1;
    while (left < right) {
        while (left < right && !Character.isLetterOrDigit(s.charAt(left))) left++;
        while (left < right && !Character.isLetterOrDigit(s.charAt(right))) right--;
        if (Character.toLowerCase(s.charAt(left)) != Character.toLowerCase(s.charAt(right))) {
            return false;
        }
        left++;
        right--;
    }
    return true;
}

// Solution to Exercise 6: Supplementary Code Point Reverser
public static String reverseUnicodeSafely(String input) {
    if (input == null || input.length() <= 1) return input;
    int[] codePoints = input.codePoints().toArray();
    int left = 0, right = codePoints.length - 1;
    while (left < right) {
        int temp = codePoints[left];
        codePoints[left] = codePoints[right];
        codePoints[right] = temp;
        left++;
        right--;
    }
    return new String(codePoints, 0, codePoints.length);
}
```
</details>

---

## 12. Summary & Cheat Sheet

```
┌────────────────────────────────────────────────────────────────────────┐
│                        STRING CHEAT SHEET                              │
├────────────────────────────────────────────────────────────────────────┤
│ Literal creation:      String s = "text"; (Pooled in String Constant   │
│                        Pool, shared across classes)                    │
│ Constructor creation:  String s = new String("text"); (AVOID! Heap dup)│
│ Equality comparison:   s1.equals(s2)  // NEVER use s1 == s2           │
│ Empty vs Blank:        "".isEmpty() == true                            │
│                        "   ".isBlank() == true (Unicode-aware)         │
│ Strip vs Trim:         s.strip() removes all Unicode whitespace        │
│                        s.trim() only removes chars <= \u0020           │
│ Java 9+ Internals:     Compact Strings: byte[] + byte coder            │
│                        coder 0 = LATIN1 (1B), coder 1 = UTF16 (2B)     │
│ Concatenation:         Single expression: use + (invokedynamic JEP 280)│
│                        Loop concatenation: ALWAYS use StringBuilder    │
│ Security:              Never store passwords in String (use char[])    │
│ Supplementary Unicode: Use s.codePoints() / s.codePointCount(...)     │
└────────────────────────────────────────────────────────────────────────┘
```

---

## 13. Further Reading
- [JEP 254: Compact Strings](https://openjdk.org/jeps/254)
- [JEP 280: Indify String Concatenation](https://openjdk.org/jeps/280)
- [JEP 334: JVM Constants API](https://openjdk.org/jeps/334)
- Official Java SE 25 API Documentation: `java.lang.String`
- *Effective Java (3rd Edition)* by Joshua Bloch, Item 6: "Avoid creating unnecessary objects" and Item 63: "Beware the performance of string concatenation".
