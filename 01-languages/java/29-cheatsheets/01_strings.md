# Java Strings & Text Processing Reference Cheat Sheet

> Comprehensive quick-reference for String internals, String Constant Pool, StringBuilder vs StringBuffer, Compact Strings, Text Blocks, Formatting, and Regular Expressions.

| Metadata | Details |
| :--- | :--- |
| **Level** | Comprehensive Reference |
| **Prerequisites** | [00_core-language.md](./00_core-language.md) |
| **Time to Read** | 35 minutes |
| **Status** | Complete |
| **Java Version** | Java 8 to 25 LTS |

---

## Table of Contents
1. [String Architecture & Compact Strings](#1-string-architecture--compact-strings)
2. [String Constant Pool & Native StringTable](#2-string-constant-pool--native-stringtable)
3. [Complete String API Reference](#3-complete-string-api-reference)
4. [StringBuilder vs StringBuffer Architecture](#4-stringbuilder-vs-stringbuffer-architecture)
5. [Text Blocks Syntax & Formatting Rules (Java 15+)](#5-text-blocks-syntax--formatting-rules-java-15)
6. [String Formatting & Conversion Specifiers](#6-string-formatting--conversion-specifiers)
7. [Regular Expressions & Regex Matching](#7-regular-expressions--regex-matching)
8. [Unicode & Code Point Processing](#8-unicode--code-point-processing)
9. [String Performance & HotSpot Optimizations](#9-string-performance--hotspot-optimizations)
10. [String Deduplication in Garbage Collectors](#10-string-deduplication-in-garbage-collectors)
11. [Common String Gotchas & Memory Leaks](#11-common-string-gotchas--memory-leaks)
12. [Practical Code Recipes](#12-practical-code-recipes)

---

## 1. String Architecture & Compact Strings

### Memory Layout (Since Java 9 - JEP 254)
Prior to Java 9, `String` stored characters in a `char[]` (UTF-16, consuming 2 bytes per character). Modern HotSpot JVMs employ **Compact Strings**:
```java
public final class String implements java.io.Serializable, Comparable<String>, CharSequence {
    private final byte[] value; // Encoded bytes: Latin-1 (1 byte) or UTF-16 (2 bytes)
    private final byte coder;   // 0 = LATIN1, 1 = UTF16
    private int hash;           // Cached 32-bit hash code
    private boolean hashIsZero; // True if calculated hash is exactly zero (Java 13+)
}
```

```
+-------------------------------------------------------------+
|                     java.lang.String                        |
|  [Mark Word (8B)] [Klass Word (4B)] [coder: 1B] [hash: 4B]  |
|  [hashIsZero: 1B] [padding: 2B] [value ref: 4B]             |
+-------------------------------------------------------------+
                               |
                               v
               +-------------------------------+
               |          byte[] value         |
               | [Header: 16B] [Data: N bytes] |
               +-------------------------------+
```

---

## 2. String Constant Pool & Native StringTable

- The **String Constant Pool (SCP)** is a native hash table (`StringTable`) residing inside the Java Heap memory.
- String literals are automatically interned at class-loading time.
- `new String("abc")` allocates a new `String` object in general heap memory, bypassing pool reuse.
- Calling `.intern()` returns the canonical reference from the pool.

```java
String s1 = "hello";              // Placed into String Constant Pool
String s2 = "hello";              // Reuses pool instance
String s3 = new String("hello");  // Distinct heap instance outside pool
String s4 = s3.intern();          // Returns canonical pool reference

System.out.println(s1 == s2); // true
System.out.println(s1 == s3); // false
System.out.println(s1 == s4); // true
```

---

## 3. Complete String API Reference

### Inspection & Querying
| Method | Description | Complexity |
| :--- | :--- | :---: |
| `length()` | Returns number of UTF-16 code units | $O(1)$ |
| `isEmpty()` | Returns `true` if `length() == 0` | $O(1)$ |
| `isBlank()` | Returns `true` if empty or only whitespace (Java 11+) | $O(N)$ |
| `charAt(int index)` | Returns `char` value at specified index | $O(1)$ |
| `codePointAt(int index)` | Returns Unicode code point at index | $O(1)$ |
| `contains(CharSequence s)` | Tests if sequence is present | $O(N 	imes M)$ |
| `startsWith(String prefix)` | Tests if starts with prefix | $O(M)$ |
| `endsWith(String suffix)` | Tests if ends with suffix | $O(M)$ |
| `indexOf(String str)` | Index of first occurrence (or -1) | $O(N 	imes M)$ |
| `lastIndexOf(String str)` | Index of last occurrence (or -1) | $O(N 	imes M)$ |

### Transformation & Slicing
| Method | Description | Complexity |
| :--- | :--- | :---: |
| `substring(int begin, int end)` | Returns new substring from `begin` (inclusive) to `end` (exclusive) | $O(K)$ |
| `strip()` | Whitespace removal supporting full Unicode whitespace (Java 11+) | $O(N)$ |
| `trim()` | Whitespace removal for ASCII $\le 32$ only | $O(N)$ |
| `stripLeading()` / `stripTrailing()` | Strip whitespace from front or back (Java 11+) | $O(N)$ |
| `toLowerCase()` / `toUpperCase()` | Converts casing using `Locale.ROOT` or specified locale | $O(N)$ |
| `replace(CharSequence t, CharSequence r)` | Replaces all exact literal occurrences | $O(N)$ |
| `replaceAll(String regex, String r)` | Replaces all regex matches | $O(N)$ |
| `split(String regex, int limit)` | Splits string around matches of regex | $O(N)$ |
| `repeat(int count)` | Concatenates string `count` times (Java 11+) | $O(N 	imes K)$ |
| `lines()` | Returns `Stream<String>` split by line terminators `
`, `
` (Java 11+) | $O(N)$ |

---

## 4. StringBuilder vs StringBuffer Architecture

| Characteristic | `String` | `StringBuilder` | `StringBuffer` |
| :--- | :--- | :--- | :--- |
| **Mutability** | Immutable | Mutable | Mutable |
| **Thread Safety** | Inherently Thread-safe | **Not Thread-safe** | Thread-safe (`synchronized`) |
| **Performance** | Allocation heavy on concat | **Fastest** for single-thread | Slower due to lock overhead |
| **Default Buffer** | Exact character length | 16 characters | 16 characters |
| **Growth Strategy** | Creates new object | `(oldCapacity * 2) + 2` | `(oldCapacity * 2) + 2` |
| **Recommended Use** | Constants, keys, DTOs | String concatenation, loops | Legacy code, shared buffers |

---

## 5. Text Blocks Syntax & Formatting Rules (Java 15+)

```java
// Begins with """ followed by mandatory newline
String htmlDoc = """
        <html>
            <body>
                <p>Hello, World!</p>
            </body>
        </html>
        """;

// Escape Sequences inside Text Blocks:
// \s  -> Preserves explicit trailing whitespace (which is otherwise stripped)
// \   -> Suppresses newline, continuing text on the same line
String sqlQuery = """
        SELECT id, username, email \
        FROM users \
        WHERE status = 'ACTIVE'\s\
        ORDER BY created_at DESC;
        """;
```

---

## 6. String Formatting & Conversion Specifiers

```java
// Syntax: %[flags][width][.precision]conversion-character
String s = String.format("%04d - %-10s : %8.2f USD", 42, "Invoice", 1245.50);
```

| Specifier | Data Type | Example Output |
| :---: | :--- | :--- |
| `%s` | String / Object (`toString()`) | `"hello"` |
| `%d` | Signed Integer (`byte`, `short`, `int`, `long`) | `42` |
| `%f` | Floating-point decimal | `3.141593` |
| `%e` | Scientific notation | `3.141593e+00` |
| `%x` / `%X` | Hexadecimal integer | `2a` / `2A` |
| `%b` | Boolean (`true` or `false`) | `true` |
| `%tY-%tm-%td` | Date formatting | `2026-11-21` |
| `%%` | Literal percent sign | `%` |
| `%n` | Platform-independent line separator | `
` (Linux) / `
` (Win) |

---

## 7. Regular Expressions & Regex Matching

```java
import java.util.regex.Pattern;
import java.util.regex.Matcher;

public class RegexQuickRef {
    // Compile regex once into static immutable Pattern for reuse
    private static final Pattern PHONE_PATTERN = Pattern.compile(
            "^(?:\\+1\\s*)?(?:\\((?<area>\\d{3})\\)|(?<area2>\\d{3}))[-.\\s]*(?<prefix>\\d{3})[-.\\s]*(?<line>\\d{4})$"
    );

    public static void parsePhone(String phone) {
        Matcher matcher = PHONE_PATTERN.matcher(phone);
        if (matcher.matches()) {
            String area = matcher.group("area") != null ? matcher.group("area") : matcher.group("area2");
            System.out.printf("Area: %s, Prefix: %s, Line: %s%n", area, matcher.group("prefix"), matcher.group("line"));
        }
    }
}
```

### Essential Regex Syntax Reference
- `^` / `$`: Start / End of input line
- `\d` / `\D`: Digit `[0-9]` / Non-digit `[^0-9]`
- `\w` / `\W`: Word char `[a-zA-Z_0-9]` / Non-word char
- `\s` / `\S`: Whitespace / Non-whitespace
- `*` / `+` / `?`: 0 or more / 1 or more / 0 or 1
- `*?` / `+?`: Reluctant (non-greedy) quantifiers
- `(?<name>...)`: Named capturing group
- `(?:...)`: Non-capturing group
- `(?=...)` / `(?!...)`: Positive / Negative lookahead

---

## 8. Unicode & Code Point Processing

Java strings use UTF-16 representation. Characters outside the Basic Multilingual Plane (BMP, code points $> \text{U+FFFF}$) require **two 16-bit code units** called a **surrogate pair**.

```java
String emoji = "Hello 🚀"; // Rocket is U+1F680

System.out.println("Length (code units): " + emoji.length()); // 8 (not 7!)
System.out.println("Code points count: " + emoji.codePointCount(0, emoji.length())); // 7

// Correct iteration over Unicode characters
emoji.codePoints().forEach(cp -> {
    System.out.println("Code Point: 0x" + Integer.toHexString(cp));
});
```

---

## 9. String Performance & HotSpot Optimizations

1. **InvokeDynamic Concatenation (Java 9+ JEP 280):**
   - Simple string concatenation with `+` outside loops is translated into `invokedynamic` calling `StringConcatFactory.makeConcatWithConstants()`, outperforming manual `StringBuilder` by pre-calculating exact target array sizes.
2. **Loop Concatenation Anti-Pattern:**
   - Inside loops, `+` creates a new `StringBuilder` and allocates a new `String` on *every iteration*, yielding $O(N^2)$ algorithmic complexity. Always use an explicit `StringBuilder` instantiated outside the loop.
3. **Pre-Sizing `StringBuilder`:**
   - Always initialize `StringBuilder` with expected capacity: `new StringBuilder(estimatedLength)`. Avoids repeated array doubling and buffer copying.

---

## 10. String Deduplication in Garbage Collectors

In large enterprise applications, duplicate string objects frequently consume 15–25% of total Java heap memory.
HotSpot's G1 and ZGC garbage collectors include transparent **String Deduplication**:

```bash
# Enable G1 string deduplication
-XX:+UseG1GC -XX:+UseStringDeduplication -XX:StringDeduplicationAgeThreshold=3
```
- Operates during garbage collection pauses.
- When two distinct `String` objects hold identical `byte[]` arrays, G1 rewires one string to point to the other's `byte[]` and reclaims the duplicate array.

---

## 11. Common String Gotchas & Memory Leaks

| Anti-Pattern | Root Cause | Remediated Code |
| :--- | :--- | :--- |
| `str == "test"` | Compares object reference addresses, not content. | `Objects.equals(str, "test")` or `"test".equals(str)` |
| `String.split(".")` failing | `.` is regex wildcard matching any character. | `String.split("\\.")` or `String.split(Pattern.quote("."))` |
| `str.toUpperCase()` without Locale | Converts characters inconsistently on Turkish locales (dotted/dotless 'i'). | `str.toUpperCase(Locale.ROOT)` |
| Unbounded `String.intern()` | Excessive interning in long-running applications fills JVM `StringTable`, increasing GC pause times. | Use a bounded cache (`Caffeine`) for interning dynamic strings. |

---

## 12. Practical Code Recipes

### Recipe 1: Fast Delimited Joiner
```java
String joined = String.join(", ", "Apple", "Banana", "Cherry");
```

### Recipe 2: String Tokenization with Regex Stream
```java
public static List<String> extractWords(String text) {
    return Pattern.compile("\\W+")
            .splitAsStream(text)
            .filter(w -> !w.isBlank())
            .toList();
}
```

### Recipe 3: Masking Sensitive Information
```java
public static String maskCreditCard(String ccNumber) {
    if (ccNumber == null || ccNumber.length() < 4) return "****";
    return "*".repeat(ccNumber.length() - 4) + ccNumber.substring(ccNumber.length() - 4);
}
```


---

## 13. High-Performance Text Parsing & Scanning Patterns

```java
package com.engineering.strings;

import java.nio.ByteBuffer;
import java.nio.charset.StandardCharsets;

public class ZeroAllocationAsciiParser {
    // Parses integers directly from byte buffers without intermediate String allocation
    public static int parsePositiveInt(byte[] bytes, int offset, int length) {
        int result = 0;
        for (int i = 0; i < length; i++) {
            byte b = bytes[offset + i];
            if (b < '0' || b > '9') {
                throw new NumberFormatException("Non-digit byte found: " + b);
            }
            result = result * 10 + (b - '0');
        }
        return result;
    }

    public static boolean asciiEquals(byte[] a, int aOffset, byte[] b, int bOffset, int length) {
        for (int i = 0; i < length; i++) {
            if (a[aOffset + i] != b[bOffset + i]) return false;
        }
        return true;
    }
}
```

---

## 14. Comprehensive Regex Quantifiers & Character Classes Reference

| Character Class | Meaning | Matching Example |
| :--- | :--- | :--- |
| `[abc]` | Any of 'a', 'b', or 'c' (simple class) | `b` matches |
| `[^abc]` | Any character except 'a', 'b', or 'c' (negation) | `x` matches |
| `[a-zA-Z]` | Lowercase or uppercase ASCII letter (range) | `M` matches |
| `[a-d[m-p]]` | Union of two ranges ('a' through 'd', or 'm' through 'p') | `c`, `n` match |
| `[a-z&&[def]]` | Intersection: characters in both ranges ('d', 'e', 'f') | `e` matches |
| `[a-z&&[^bc]]` | Subtraction: 'a' through 'z' except 'b' and 'c' | `a`, `d` match |
| `\p{Lower}` | Lowercase US-ASCII character: `[a-z]` | `g` matches |
| `\p{Upper}` | Uppercase US-ASCII character: `[A-Z]` | `T` matches |
| `\p{ASCII}` | Any US-ASCII character: `[\x00-\x7F]` | `$` matches |
| `\p{Alpha}` | Alphabetic character: `[\p{Lower}\p{Upper}]` | `k` matches |
| `\p{Digit}` | Decimal digit: `[0-9]` | `7` matches |
| `\p{Alnum}` | Alphanumeric: `[\p{Alpha}\p{Digit}]` | `9`, `W` match |
| `\p{Punct}` | Punctuation: `[!"#$%&'()*+,-./:;<=>?@[\]^_\`{|}~]` | `@` matches |

### Boundary Matchers Reference
| Boundary | Meaning |
| :--- | :--- |
| `^` | The beginning of a line |
| `$` | The end of a line |
| `\b` | A word boundary (transition between word char and non-word char) |
| `\B` | A non-word boundary |
| `\A` | The beginning of the entire input |
| `\G` | The end of the previous match |
| `\Z` | The end of the input but for the final terminator, if any |
| `\z` | The end of the entire input |

---

## 15. Advanced String Formatting Syntax & Locale Variations

```java
import java.util.Locale;

public class AdvancedFormattingPatterns {
    public static void main(String[] args) {
        // Thousands separators and currency
        System.out.println(String.format(Locale.US, "%,d", 1000000)); // "1,000,000"
        System.out.println(String.format(Locale.GERMANY, "%,d", 1000000)); // "1.000.000"

        // Floating point precision and signs
        System.out.println(String.format("%+10.4f", 42.12)); // "  +42.1200"

        // Hexadecimal and Octal representation
        System.out.println(String.format("%#x", 255)); // "0xff"
        System.out.println(String.format("%#o", 255)); // "0377"

        // Reordering argument indexes (%1$s, %2$s)
        System.out.println(String.format("%2$s before %1$s", "First", "Second")); // "Second before First"
    }
}
```

---

## 16. StringJoiner and String.join Reference

```java
import java.util.StringJoiner;

public class StringJoinerGuide {
    public static void main(String[] args) {
        // StringJoiner(delimiter, prefix, suffix)
        StringJoiner sj = new StringJoiner(", ", "[", "]");
        sj.setEmptyValue("EMPTY_LIST"); // Output if nothing is added
        System.out.println("Empty: " + sj);

        sj.add("Alpha").add("Beta").add("Gamma");
        System.out.println("Joined: " + sj); // "[Alpha, Beta, Gamma]"

        // Merging another StringJoiner
        StringJoiner other = new StringJoiner(", ", "[", "]");
        other.add("Delta").add("Epsilon");
        sj.merge(other);
        System.out.println("Merged: " + sj); // "[Alpha, Beta, Gamma, Delta, Epsilon]"
    }
}
```

---

## 17. Quick Summary / Cheat Sheet Card

- **Compact Strings:** HotSpot stores strings as `byte[]` with Latin-1 (1 byte/char) or UTF-16 (2 bytes/char) based on contents.
- **Constant Pool:** Literals intern automatically in native heap `StringTable`. Bounded heap caches prevent leak on manual interning.
- **Concatenation:** Single expressions with `+` use `invokedynamic` (JEP 280). Inside loops, always instantiate explicit `StringBuilder`.
- **Regex Patterns:** Always compile `Pattern.compile(...)` as static final fields to avoid runtime bytecode recompilation.
