# Java String Methods: Inspection, Search, and Transformation

> A deep dive into the extensive standard API of `java.lang.String`, covering inspection, Unicode-aware manipulation, pattern searching, and modern functional transformations.

| Metadata | Details |
| :--- | :--- |
| **Level** | Beginner to Intermediate |
| **Prerequisites** | [00_string-basics.md](./00_string-basics.md), [01_variables-and-data-types.md](../01-fundamentals/01_variables-and-data-types.md) |
| **Time to Read** | 25 minutes |
| **Status** | Active |
| **Java Version** | Java 8+ (with additions from Java 11, 12, 15, and 21) |

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
9. [Comparison: Method Trade-offs](#9-comparison-method-trade-offs)
10. [Interview Questions](#10-interview-questions)
11. [Practice Exercises](#11-practice-exercises)
12. [Summary & Cheat Sheet](#12-summary--cheat-sheet)
13. [Further Reading](#13-further-reading)

---

## 1. What & Why

The `java.lang.String` class provides over 60 public methods that form the backbone of text processing in Java. Because `String` instances are immutable, every transformation method (such as `toUpperCase()`, `trim()`, or `substring()`) constructs and returns a new `String` object without altering the source operand.

Modern Java releases have continuously modernized this API:
- **Java 11:** Added `isBlank()`, `lines()`, `strip()`, `stripLeading()`, `stripTrailing()`, and `repeat(int)`.
- **Java 12:** Added `indent(int)` and `transform(Function)`.
- **Java 15:** Added `formatted(Object... args)` and `stripIndent()`.
- **Java 21:** Added `splitWithDelimiters(regex, limit)` for lexing and tokenization while keeping delimiter tokens.

Understanding the behavior, algorithmic complexity, and memory implications of these methods is vital for building clean, high-performance Java applications.

---

## 2. Core Concepts

### 2.1 Categorization of String Methods

1. **Inspection:** Character retrieval and length validation (`length()`, `isEmpty()`, `isBlank()`, `charAt()`, `codePointAt()`).
2. **Search & Predicates:** Substring and pattern detection (`contains()`, `startsWith()`, `endsWith()`, `indexOf()`, `lastIndexOf()`, `matches()`).
3. **Extraction & Slicing:** Selecting subsets of characters (`substring()`, `subSequence()`).
4. **Transformations:** Producing modified content (`toLowerCase()`, `toUpperCase()`, `replace()`, `replaceAll()`, `strip()`, `indent()`, `transform()`).
5. **Partitioning & Splitting:** Dividing text into streams or arrays (`split()`, `splitWithDelimiters()`, `lines()`).
6. **Formatting:** Interpolating structured parameters (`formatted()`, `String.format()`).

---

## 3. How It Works Internally

### 3.1 Substring Memory Evolution
One of the most consequential changes in Java's history was the reimplementation of `substring()` in Java 7 update 6:

```
Java 6 Substring Sharing (Memory Leak Hazard):
Original String "Huge 100MB File Contents..."
┌──────────────────────────────┐
│ String object (offset=0, len=100M)
│ char[] value ───────────────► [ 100MB character array on heap ]
└──────────────────────────────┘        ▲
                                        │ (Shared buffer)
Small Substring "ID_123"                │
┌──────────────────────────────┐        │
│ String object (offset=5, len=6)───────┘
└──────────────────────────────┘
```

In Java 6, `substring()` created a new `String` object pointing to the **same** underlying `char[]` array with updated `offset` and `count` fields. While $O(1)$ fast, retaining a 6-character substring prevented the entire 100MB parent `char[]` from being garbage collected!

```
Java 7u6+ and Java 25 Substring (Defensive Copy):
Original String:
┌──────────────────────────────┐
│ String Object                │
│ byte[] value (coder=LATIN1) ─► [ 'H', 'u', 'g', 'e', ... 100MB ]
└──────────────────────────────┘

Small Substring "ID_123":
┌──────────────────────────────┐
│ String Object                │
│ byte[] value (coder=LATIN1) ─► [ 'I', 'D', '_', '1', '2', '3' ] (Only 6 bytes!)
└──────────────────────────────┘
```
In modern Java, `substring()` always allocates a new `byte[]` containing strictly the requested slice. It takes $O(K)$ time where $K$ is the slice length, but guarantees that the parent array can be reclaimed immediately by the garbage collector.

### 3.2 Java 11 `lines()` and Spliterator
`str.lines()` returns a `Stream<String>`. Rather than splitting the entire text into memory at once like `str.split("\R")`, `lines()` uses an internal `LinesSpliterator` that scans for `
`, `
`, or `` lazily as downstream stream consumers request items. This enables memory-efficient line processing of giant multi-megabyte strings.

---

## 4. Syntax & Usage

### 4.1 Key Method Signatures
```java
// Inspection
public int length();
public boolean isEmpty();
public boolean isBlank();                        // Java 11
public char charAt(int index);
public int codePointAt(int index);

// Searching
public boolean contains(CharSequence s);
public boolean startsWith(String prefix);
public boolean endsWith(String suffix);
public int indexOf(String str);
public int indexOf(String str, int fromIndex);
public int lastIndexOf(String str);

// Transformations
public String toLowerCase(Locale locale);
public String toUpperCase(Locale locale);
public String strip();                           // Java 11 (Unicode-aware)
public String stripLeading();                    // Java 11
public String stripTrailing();                   // Java 11
public String repeat(int count);                 // Java 11
public String indent(int n);                     // Java 12
public <R> R transform(Function<? super String, ? extends R> f); // Java 12
public String formatted(Object... args);         // Java 15

// Splitting
public String[] split(String regex);
public String[] split(String regex, int limit);
public String[] splitWithDelimiters(String regex, int limit); // Java 21
public Stream<String> lines();                   // Java 11
```

---

## 5. Practical Examples

### Example 1: Basic String Querying and Transformations
```java
public class StringMethodsBasic {
    public static void main(String[] args) {
        String title = "  Software Architecture & Design Patterns  ";

        // Cleaning whitespace
        String clean = title.strip();
        System.out.println("Cleaned: [" + clean + "]");

        // Substring and search
        int ampPos = clean.indexOf('&');
        String firstPart = clean.substring(0, ampPos).stripTrailing();
        String secondPart = clean.substring(ampPos + 1).stripLeading();

        System.out.println("Part 1: [" + firstPart + "]");
        System.out.println("Part 2: [" + secondPart + "]");
        System.out.println("Contains 'Design': " + clean.contains("Design"));
    }
}
```
**Output:**
```text
Cleaned: [Software Architecture & Design Patterns]
Part 1: [Software Architecture]
Part 2: [Design Patterns]
Contains 'Design': true
```

### Example 2: Real-World Safe CSV & Log Line Parser with `splitWithDelimiters`
```java
import java.util.Arrays;

public class LogLineTokenizer {
    public static void main(String[] args) {
        String logEntry = "2026-11-21 [ERROR] DatabaseConnectionPool: connection timed out; retry=3";

        // Java 21 splitWithDelimiters keeps the separators for syntax-highlighting or rebuilding
        String[] tokens = logEntry.splitWithDelimiters("[\[\]:;]", 0);

        System.out.println("Tokens count: " + tokens.length);
        for (int i = 0; i < tokens.length; i++) {
            System.out.printf("[%02d] -> "%s"%n", i, tokens[i]);
        }
    }
}
```
**Output:**
```text
Tokens count: 9
[00] -> "2026-11-21 "
[01] -> "["
[02] -> "ERROR"
[03] -> "]"
[04] -> " DatabaseConnectionPool"
[05] -> ":"
[06] -> " connection timed out"
[07] -> ";"
[08] -> " retry=3"
```

### Example 3: Edge Case – Locale-Sensitive Case Conversion (The Turkish 'i' Problem)
```java
import java.util.Locale;

public class TurkishLocaleCaseTrap {
    public static void main(String[] args) {
        String word = "title";

        // Default locale may alter lowercase 'i' behavior
        String defaultUpper = word.toUpperCase();
        String turkishUpper = word.toUpperCase(Locale.forLanguageTag("tr-TR"));
        String rootUpper    = word.toUpperCase(Locale.ROOT);

        System.out.println("Default: " + defaultUpper);
        System.out.println("Turkish: " + turkishUpper); // Produces dotted capital İ!
        System.out.println("ROOT:    " + rootUpper);    // Guarantees ASCII 'TITLE'

        System.out.println("Turkish equals 'TITLE': " + "TITLE".equals(turkishUpper));
        System.out.println("ROOT equals 'TITLE':    " + "TITLE".equals(rootUpper));
    }
}
```
**Output:**
```text
Default: TITLE
Turkish: TİTLE
ROOT:    TITLE
Turkish equals 'TITLE': false
ROOT equals 'TITLE':    true
```

### Example 4: Failure Case – Negative Limit in `split()` vs Default Limit 0
```java
public class SplitTrailingLimitTrap {
    public static void main(String[] args) {
        String record = "Alice,30,Engineer,,,";

        // Default split drops trailing empty strings!
        String[] defaultSplit = record.split(",");
        // Negative limit preserves trailing empty strings!
        String[] fullSplit = record.split(",", -1);

        System.out.println("Default split length: " + defaultSplit.length); // 3 (missing empty trailing fields!)
        System.out.println("Full split length:    " + fullSplit.length);    // 6 (correct 6 columns)
    }
}
```
**Output:**
```text
Default split length: 3
Full split length:    6
```

### Example 5: Optimized / Idiomatic – Fluent Functional Transformations with `.transform()`
```java
public class FunctionalTransformDemo {
    public static void main(String[] args) {
        String rawInput = "   order_id_89432   ";

        Long orderId = rawInput
            .strip()
            .transform(s -> s.replace("order_id_", ""))
            .transform(Long::parseLong);

        System.out.println("Parsed Order ID: " + orderId);
    }
}
```
**Output:**
```text
Parsed Order ID: 89432
```

---

## 6. Best Practices

1. **Always Use `Locale.ROOT` for System Identifiers:** When converting protocol headers, JSON keys, SQL statements, or file extensions to upper/lower case, always use `s.toUpperCase(Locale.ROOT)` to prevent runtime bugs on systems configured with Turkish, Azeri, or Lithuanian locales.
2. **Use `String.repeat(n)` Instead of Custom Loops:** `repeat()` uses optimized low-level memory intrinsics (`System.arraycopy`) to duplicate byte segments without intermediate object creation.
3. **Use `s.strip()` Over `s.trim()`:** `strip()` correctly removes all Unicode whitespace, whereas `trim()` only removes characters $\le 	ext{U+0020}$.
4. **Use `s.lines()` for Multi-Line Processing:** When parsing logs or multi-line text, `lines()` provides a lazy, garbage-friendly stream instead of loading all lines into a `String[]` array upfront.
5. **Pass Negative Limit to `split()` When Trailing Delimiters Matter:** By default, `str.split(",")` discards all trailing empty strings. Pass `-1` as the second argument (`str.split(",", -1)`) when parsing delimited files (CSV/TSV).

---

## 7. Common Mistakes & Pitfalls

| Mistake | Why It Is Wrong | Correct Idiom |
| :--- | :--- | :--- |
| `str.toUpperCase()` without Locale | Uses system default locale; fails on Turkish systems for words with 'i' (produces `İ` instead of `I`). | `str.toUpperCase(Locale.ROOT)` |
| `str.split(".")` | `.` in regex matches any character, returning an empty array. | `str.split("\.")` or `str.split(Pattern.quote("."))` |
| Dropped trailing tokens in `split()` | Default `split(regex)` uses `limit = 0`, discarding trailing empty tokens silently. | `str.split(regex, -1)` |
| Using `indexOf` with `+` without bounds checking | May trigger `StringIndexOutOfBoundsException` if `indexOf` returned `-1`. | Always check `if (idx != -1)` before `substring(idx)`. |
| Calling `.substring()` expecting $O(1)$ pointer slice | Since Java 7u6, `substring()` copies characters; in loops this can create large memory churn. | Use `CharSequence` or index cursors for zero-copy parsing. |
| Inverting `equals()` check with null | Calling `variable.equals("CONSTANT")` throws `NullPointerException` if `variable` is null. | Use `"CONSTANT".equals(variable)` or `Objects.equals(a, b)`. |

---

## 8. Performance & Security Notes

### 8.1 Complexity of Search & Replace Methods
- `indexOf()`: Employs vectorized SIMD instructions in modern JVM HotSpot JIT (AVX2/AVX-512) for single-character and short-string searches, outperforming standard Boyer-Moore implementations in pure Java.
- `replace(CharSequence, CharSequence)`: Performs literal replacement without regex engine overhead.
- `replaceAll(String regex, String replacement)`: Compiles the regex pattern on every invocation. If executed frequently in loops, compile a `Pattern` once and reuse a `Matcher`.

---

## 9. Comparison: Method Trade-offs

| Method | Returns | Allocates Memory? | Unicode-Aware? |
| :--- | :--- | :--- | :--- |
| `trim()` | `String` | Yes (if spaces present) | ❌ No (only ASCII $\le 32$) |
| `strip()` | `String` | Yes (if spaces present) | ✅ Yes (`Character.isWhitespace`) |
| `split(regex)` | `String[]` | Yes (array + all slices) | Depends on regex flag |
| `lines()` | `Stream<String>` | Lazy (on demand) | ✅ Yes (``, `
`, `
`) |
| `charAt(idx)` | `char` | ❌ No (primitive) | ❌ No (fails on surrogate pairs) |
| `codePointAt(idx)`| `int` | ❌ No (primitive) | ✅ Yes (full 21-bit code point) |

---

## 10. Interview Questions

<details>
<summary><strong>1. What is the difference between str.isEmpty() and str.isBlank()?</strong></summary>

`isEmpty()` was added in Java 6 and returns `true` if and only if `str.length() == 0`.
`isBlank()` was added in Java 11 and returns `true` if the string is either empty OR contains only Unicode whitespace code points according to `Character.isWhitespace(int)`.
Example: `"   	   ".isEmpty()` is `false`, but `"   	   ".isBlank()` is `true`.
</details>

<details>
<summary><strong>2. Why should system software and parsers always use Locale.ROOT when converting casing?</strong></summary>

The parameterless methods `toLowerCase()` and `toUpperCase()` use the JVM's current default locale (`Locale.getDefault()`). In certain languages like Turkish and Azeri, the lowercase letter 'i' maps to uppercase dotted capital 'İ' (`İ`), while the uppercase letter 'I' maps to lowercase dotless 'ı' (`ı`). If a Turkish user runs an application that executes `"select".toUpperCase()`, it produces `"SELECT"` with a dotted `İ`, which causes SQL syntax errors or HTTP header corruption. `Locale.ROOT` guarantees invariant ASCII casing behavior.
</details>

<details>
<summary><strong>3. How does String.split() handle trailing empty strings by default?</strong></summary>

`String.split(regex)` internally delegates to `String.split(regex, 0)`. A limit of `0` directs the regex engine to strip all trailing empty strings from the resulting array. To retain trailing empty delimiters (such as missing values in CSV records), you must supply a negative limit: `str.split(regex, -1)`.
</details>

<details>
<summary><strong>4. What does the String.transform() method introduced in Java 12 do?</strong></summary>

`transform(Function<? super String, ? extends R> f)` allows applying an arbitrary function to the current string instance in a fluent call chain without breaking intermediate method chaining. It simply executes `f.apply(this)`. This is very helpful when chaining multiple cleaning, parsing, and mapping steps.
</details>

<details>
<summary><strong>5. Why did Java change the implementation of String.substring() in Java 7u6?</strong></summary>

In Java 6 and earlier, `substring()` shared the underlying `char[]` buffer with the parent string, only modifying the internal `offset` and `count` fields. If an application read a 10MB document and saved a small 5-character substring, the entire 10MB buffer remained pinned in heap memory, causing subtle and severe memory leaks. Starting in Java 7u6, `substring()` creates a defensive copy containing only the sliced characters, allowing the original parent array to be garbage collected.
</details>

<details>
<summary><strong>6. What is the difference between String.replace() and String.replaceAll()?</strong></summary>

`replace(CharSequence target, CharSequence replacement)` treats both arguments as literal strings. It does not parse regular expressions.
`replaceAll(String regex, String replacement)` treats the first argument as a regular expression pattern and interprets `$` and `\` in the replacement string as capturing group backreferences. `replace()` is significantly faster when regular expressions are not needed.
</details>

<details>
<summary><strong>7. How does String.lines() differ from String.split("\R")?</strong></summary>

`String.split("\R")` eagerly compiles a regex and allocates a full `String[]` array containing all parsed lines in memory simultaneously. `String.lines()` (Java 11) returns a lazy `Stream<String>` backed by an optimized character-scanning `Spliterator`. It allocates string instances on-the-fly as stream elements are requested and does not parse regexes.
</details>

<details>
<summary><strong>8. What does String.indent(int n) do?</strong></summary>

Added in Java 12, `indent(int n)` adjusts the indentation of each line in the string by `n` spaces. If `n > 0`, `n` spaces are inserted at the start of each line. If `n < 0`, up to `|n|` leading whitespace characters are stripped from each line. It also automatically normalizes line terminators to `
` and ensures the string ends with a newline.
</details>

---

## 11. Practice Exercises

### Easy
1. **Sanitize Filename:** Write a function `String sanitizeFilename(String filename)` that strips leading/trailing whitespace, replaces spaces with underscores, and forces lowercase using `Locale.ROOT`.
2. **Initials Extractor:** Extract initials from a full name (e.g. `"Ada Lovelace"` $	o$ `"A.L."`).
3. **Count Occurrences:** Count how many times a substring appears in a larger string without using regular expressions.

### Medium
4. **Custom CSV Parser:** Write a CSV line parser that properly splits lines on commas while respecting quotes containing embedded commas.
5. **Slug Generator:** Create a URL slug generator that converts arbitrary text (including accented characters) to hyphen-delimited ASCII strings.
6. **Mask Sensitive Card Numbers:** Given a credit card number string, mask all digits except the last 4 with `*`, preserving any existing spaces or hyphens.

### Hard
7. **Zero-Allocation Substring Matcher:** Implement a custom `CharSequence` wrapper that provides a virtual subSequence view across a string without allocating any heap memory.
8. **Balanced Brackets Checker:** Check whether a mathematical expression string has balanced parentheses, curly braces, and square brackets.
9. **Stream-Based Log Analyser:** Write a pipeline using `str.lines()` that filters error entries, groups them by component name, and counts occurrences using Java Streams.

<details>
<summary><strong>Exercise Solutions</strong></summary>

```java
// Solution to Exercise 1: Sanitize Filename
public static String sanitizeFilename(String filename) {
    if (filename == null || filename.isBlank()) {
        throw new IllegalArgumentException("Filename cannot be blank");
    }
    return filename.strip()
                   .replace(' ', '_')
                   .toLowerCase(java.util.Locale.ROOT);
}

// Solution to Exercise 6: Credit Card Masker
public static String maskCardNumber(String card) {
    if (card == null) return null;
    int digitCount = 0;
    for (int i = 0; i < card.length(); i++) {
        if (Character.isDigit(card.charAt(i))) digitCount++;
    }
    int unmaskedDigitsTarget = 4;
    int digitsToMask = Math.max(0, digitCount - unmaskedDigitsTarget);
    StringBuilder sb = new StringBuilder(card.length());
    int maskedSoFar = 0;
    for (int i = 0; i < card.length(); i++) {
        char c = card.charAt(i);
        if (Character.isDigit(c)) {
            if (maskedSoFar < digitsToMask) {
                sb.append('*');
                maskedSoFar++;
            } else {
                sb.append(c);
            }
        } else {
            sb.append(c);
        }
    }
    return sb.toString();
}
```
</details>

---

## 12. Summary & Cheat Sheet

```
┌────────────────────────────────────────────────────────────────────────┐
│                      STRING METHODS CHEAT SHEET                        │
├────────────────────────────────────────────────────────────────────────┤
│ isEmpty()            length() == 0                                     │
│ isBlank()            length() == 0 OR only Unicode whitespace (Java 11)│
│ strip()              Unicode-aware whitespace stripping (Java 11)      │
│ repeat(n)            Duplicates string n times efficiently (Java 11)   │
│ lines()              Returns lazy Stream<String> split by \R (Java 11)│
│ indent(n)            Adjusts indentation of all lines (Java 12)        │
│ transform(fn)        Applies function in fluent call chain (Java 12)   │
│ formatted(args...)   Equivalent to String.format(str, args) (Java 15)  │
│ split(regex, -1)     Keeps trailing empty segments in split array      │
│ toUpperCase(Locale)  Always use Locale.ROOT for invariant tokens       │
│ replace(a, b)        Literal substitution (no regex compilation)       │
└────────────────────────────────────────────────────────────────────────┘
```

---

## 13. Further Reading
- Official Java 25 `java.lang.String` API Documentation
- [JEP 320: Remove the Java EE and CORBA Modules](https://openjdk.org/jeps/320)
- *Effective Java (3rd Edition)*, Joshua Bloch, Item 63: "Beware the performance of string concatenation".
