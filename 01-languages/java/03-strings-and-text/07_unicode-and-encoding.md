# Character Encoding, Unicode, and Charset Handling in Java

> An authoritative reference on character encoding in Java, examining the Unicode Standard, Java 18 UTF-8 by Default, surrogate pairs, grapheme clusters, and robust NIO Charset transformation.

| Metadata | Details |
| :--- | :--- |
| **Level** | Intermediate to Advanced |
| **Prerequisites** | [00_string-basics.md](./00_string-basics.md), [01_string-methods.md](./01_string-methods.md) |
| **Time to Read** | 25 minutes |
| **Status** | Active |
| **Java Version** | Java 8+ (Java 18 JEP 400 UTF-8 by Default) |

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
9. [Comparison: UTF-8 vs UTF-16 vs ASCII vs ISO-8859-1](#9-comparison-utf-8-vs-utf-16-vs-ascii-vs-iso-8859-1)
10. [Interview Questions](#10-interview-questions)
11. [Practice Exercises](#11-practice-exercises)
12. [Summary & Cheat Sheet](#12-summary--cheat-sheet)
13. [Further Reading](#13-further-reading)

---

## 1. What & Why

In computing, characters are abstract human symbols ('A', 'é', 'Ω', '中', '🚀'), while computers exclusively store and transmit 8-bit bytes (`0x00`–`0xFF`). A **character encoding** is the mapping between character symbols and sequences of bytes.

Historically, character encoding was one of the most prolific sources of data corruption ("mojibake"), silent file truncation, and cross-platform bugs. Different operating systems defaulted to incompatible code pages: Windows used `windows-1252`, Linux used `UTF-8`, and legacy Asian systems used `Shift-JIS` or `GBK`.

To resolve this permanently, **Java 18 introduced JEP 400: UTF-8 by Default**. Since Java 18 and across modern Java 21/25, the standard Java APIs default to UTF-8 regardless of the underlying operating system, locale, or environment configuration.

---

## 2. Core Concepts

### 2.1 The Unicode Architecture
The Unicode Standard assigns every human character, symbol, and emoji a unique 21-bit numeric value known as a **Code Point**, expressed in hexadecimal as `U+XXXX` (ranging from `U+0000` to `U+10FFFF`, totaling 1,114,112 code points).

Unicode divides this space into 17 planes of $2^{16}$ (65,536) code points each:
- **Plane 0: Basic Multilingual Plane (BMP):** `U+0000` to `U+FFFF`. Covers virtually all modern languages, punctuation, and common symbols.
- **Planes 1–16: Supplementary Planes:** `U+010000` to `U+10FFFF`. Covers historic scripts, musical notation, mathematical symbols, and all modern emoji.

### 2.2 Code Points vs Code Units vs Grapheme Clusters
- **Code Point:** The atomic Unicode integer value (e.g. `U+1F680` for 🚀).
- **Code Unit:** The bit unit used by an encoding format. In UTF-16 (Java `char`), a code unit is 16 bits. In UTF-8, a code unit is 8 bits.
- **Grapheme Cluster:** What a human user perceives as a single visual character glyph on screen. A single grapheme cluster can consist of **multiple code points** combined (e.g., flag emojis or skin-tone modifiers: 👨 + 🏽 = 👨🏽).

---

## 3. How It Works Internally

### 3.1 UTF-16 Surrogate Pairs in Java
Because Java's primitive `char` is strictly 16 bits (`0x0000` to `0xFFFF`), it cannot represent supplementary code points ($> \text{U+FFFF}$) in a single `char`.

Unicode reserves a special range of 2,048 code points in the BMP exclusively as **Surrogates**:
- **High Surrogates:** `\uD800` to `\uDBFF` (1,024 values)
- **Low Surrogates:** `\uDC00` to `\uDFFF` (1,024 values)

Any supplementary code point $C$ ($0x10000 \le C \le 0x10FFFF$) is mathematically decomposed into a surrogate pair:
1. $C' = C - 0x10000$ (a 20-bit number)
2. $\text{High} = 0xD800 + (C' \gg 10)$
3. $\text{Low} = 0xDC00 + (C' \ \& \ 0x3FF)$

```
Decomposition of Rocket Emoji (🚀 U+1F680):
1. 0x1F680 - 0x10000 = 0x0F680 (binary: 0000 1111 0110 1000 0000)
2. High = 0xD800 + (0x0F680 >> 10) = 0xD800 + 0x003D = 0xD83D (\uD83D)
3. Low  = 0xDC00 + (0x0F680 & 0x3FF)= 0xDC00 + 0x0280 = 0xDE80 (\uDE80)

Java String Representation: "\uD83D\uDE80"
- str.length() == 2 (2 UTF-16 code units)
- str.codePointCount(0, 2) == 1 (1 Unicode character)
```

### 3.2 Java 18 (JEP 400) UTF-8 by Default
Prior to Java 18, APIs like `new FileReader("file.txt")` or `new String(bytes)` used `Charset.defaultCharset()`, which inherited the host OS encoding. A file saved on Windows (`windows-1252`) would silently fail when read by an app deployed on Linux (`UTF-8`).

Since Java 18:
- `Charset.defaultCharset()` returns `UTF-8` on all systems.
- You can override this only via command line: `-Dfile.encoding=COMPAT-LOCALE`.

---

## 4. Syntax & Usage

### 4.1 StandardCharsets and Conversion
```java
import java.nio.charset.StandardCharsets;

// String to byte array (Always specify Charset!)
String text = "Java Architecture 🚀";
byte[] utf8Bytes = text.getBytes(StandardCharsets.UTF_8);

// Byte array to String
String decoded = new String(utf8Bytes, StandardCharsets.UTF_8);

// Iterating true Unicode Code Points
text.codePoints().forEach(cp -> {
    System.out.printf("U+%04X: %s%n", cp, Character.toString(cp));
});

// Checking surrogate halves
char high = text.charAt(text.length() - 2);
boolean isHigh = Character.isHighSurrogate(high); // true
```

---

## 5. Practical Examples

### Example 1: Robust Code Point Traversal and Reversal
```java
public class UnicodeTraversalDemo {
    public static void main(String[] args) {
        String mixed = "A\uD83D\uDE80B\uD83C\uDF0D"; // "A🚀B🌍"

        System.out.println("String: " + mixed);
        System.out.println("char length: " + mixed.length()); // 6
        System.out.println("code points: " + mixed.codePointCount(0, mixed.length())); // 4

        // Safe Reversal preserving surrogate pairs
        int[] codePoints = mixed.codePoints().toArray();
        int left = 0, right = codePoints.length - 1;
        while (left < right) {
            int tmp = codePoints[left];
            codePoints[left] = codePoints[right];
            codePoints[right] = tmp;
            left++;
            right--;
        }

        String reversed = new String(codePoints, 0, codePoints.length);
        System.out.println("Safely reversed: " + reversed); // "🌍B🚀A"
    }
}
```
**Output:**
```text
String: A🚀B🌍
char length: 6
code points: 4
Safely reversed: 🌍B🚀A
```

### Example 2: Real-World Safe NIO Charset Decoder with Error Actions
```java
import java.nio.ByteBuffer;
import java.nio.CharBuffer;
import java.nio.charset.CharsetDecoder;
import java.nio.charset.CodingErrorAction;
import java.nio.charset.StandardCharsets;

public class SafeCharsetDecoderDemo {
    public static void main(String[] args) throws Exception {
        // Corrupted byte array: valid ASCII followed by invalid standalone UTF-8 continuation byte
        byte[] malformedBytes = new byte[]{'H', 'e', 'l', 'l', 'o', (byte) 0xFF, '!'};

        CharsetDecoder decoder = StandardCharsets.UTF_8.newDecoder()
            .onMalformedInput(CodingErrorAction.REPLACE)     // Replace bad bytes with \uFFFD ()
            .onUnmappableCharacter(CodingErrorAction.REPLACE)
            .replaceWith("?");

        ByteBuffer in = ByteBuffer.wrap(malformedBytes);
        CharBuffer out = decoder.decode(in);

        System.out.println("Decoded clean output: " + out.toString());
    }
}
```
**Output:**
```text
Decoded clean output: Hello?!
```

### Example 3: Edge Case – The UTF-8 Byte Order Mark (BOM) Trap
```java
import java.io.ByteArrayInputStream;
import java.io.InputStreamReader;
import java.nio.charset.StandardCharsets;

public class Utf8BomTrap {
    public static void main(String[] args) throws Exception {
        // UTF-8 BOM bytes: 0xEF, 0xBB, 0xBF followed by "test"
        byte[] withBom = new byte[]{(byte) 0xEF, (byte) 0xBB, (byte) 0xBF, 't', 'e', 's', 't'};

        // Standard Java InputStreamReader does NOT strip UTF-8 BOM automatically!
        String result = new String(withBom, StandardCharsets.UTF_8);
        System.out.println("Result length: " + result.length()); // 5! (Includes BOM character \uFEFF)
        System.out.println("Equals 'test': " + "test".equals(result)); // false!

        // Stripping BOM properly
        if (result.startsWith("\uFEFF")) {
            result = result.substring(1);
        }
        System.out.println("After strip equals 'test': " + "test".equals(result)); // true!
    }
}
```
**Output:**
```text
Result length: 5
Equals 'test': false
After strip equals 'test': true
```

### Example 4: Failure Case – Default Charset in Java 8–17 Cross-Platform Files
```java
import java.nio.charset.Charset;

public class DefaultCharsetTrap {
    public static void main(String[] args) {
        // In Java 18+, this is guaranteed to be UTF-8 across all operating systems!
        System.out.println("Default Charset: " + Charset.defaultCharset().name());
    }
}
```
**Output:**
```text
Default Charset: UTF-8
```

### Example 5: Optimized / Idiomatic – Regex Matching by Extended Grapheme Cluster (`\X`)
```java
import java.util.regex.Pattern;
import java.util.regex.Matcher;

public class GraphemeClusterRegexDemo {
    public static void main(String[] args) {
        // Person with skin tone modifier (2 code points, 1 visual glyph)
        String emojiWithModifier = "Hero: \uD83D\uDC68\uD83C\uDFFD (Engineer)"; // 👨🏽

        // \X matches an entire Extended Grapheme Cluster (all combined code points)
        Pattern graphemePattern = Pattern.compile("\\X");
        Matcher matcher = graphemePattern.matcher(emojiWithModifier);

        int count = 0;
        while (matcher.find()) {
            count++;
        }
        System.out.println("Visual grapheme count: " + count);
    }
}
```
**Output:**
```text
Visual grapheme count: 18
```

---

## 6. Best Practices

1. **Always Specify `StandardCharsets.UTF_8` Explicitly:** Never call `new String(bytes)` or `str.getBytes()` without passing `StandardCharsets.UTF_8`. Explicit code is self-documenting and immune to misconfigured JVM flags.
2. **Never Iterate Supplementary Text with `charAt()`:** Traditional `for (int i = 0; i < s.length(); i++) s.charAt(i)` splits surrogate pairs in half, corrupting emojis and non-BMP scripts. Use `s.codePoints()`.
3. **Use `\X` in Regex for User-Perceived Character Splitting:** If tokenizing text where emoji modifiers or accented diacritics appear, use `\X` (extended grapheme cluster boundary).
4. **Configure Explicit Error Actions on Decoders:** When decoding external network buffers, use `CharsetDecoder` with `onMalformedInput(CodingErrorAction.REPORT)` to reject invalid data immediately.
5. **Handle UTF-8 BOM Explicitly:** If parsing files created on Windows tools (like Notepad), check for and strip the leading `\uFEFF` zero-width non-breaking space.

---

## 7. Common Mistakes & Pitfalls

| Mistake | Why It Is Wrong | Correct Idiom |
| :--- | :--- | :--- |
| `str.getBytes()` without Charset | In pre-Java 18, used OS default charset, corrupting text when transferred across Windows/Linux. | `str.getBytes(StandardCharsets.UTF_8)`. |
| `s.length()` to measure character limits | Counts 16-bit code units, rejecting 1-emoji inputs as 2 characters. | `s.codePointCount(0, s.length())`. |
| Splitting surrogate pairs with `substring` | Slicing at an index between a high and low surrogate produces invalid dangling surrogate characters. | Use `Character.offsetByCodePoints()`. |
| Unhandled UTF-8 BOM (`\uFEFF`) | `new String(bytes, UTF_8)` retains BOM as character 0, causing database queries or JSON parsers to fail. | Check `if (str.startsWith("\uFEFF")) str = str.substring(1);`. |
| Using `ISO-8859-1` expecting full Unicode | ISO-8859-1 only supports characters $\le 255$; silently drops or replaces characters outside Latin-1. | Use `StandardCharsets.UTF_8`. |

---

## 8. Performance & Security Notes

### 8.1 Validation of Malformed Byte Sequences
Never decode raw byte arrays into `String` if the byte content determines security boundaries (e.g., path traversal checks). Attackers can send overlong UTF-8 encodings (e.g. `0xC0 0xAF` for `/`) to bypass naive string checks if decoders replace rather than reject malformed bytes. Use `CodingErrorAction.REPORT` to fail fast.

---

## 9. Comparison: UTF-8 vs UTF-16 vs ASCII vs ISO-8859-1

| Encoding | Bytes per Character | ASCII Compatible? | Code Point Coverage | Primary Use |
| :--- | :--- | :--- | :--- | :--- |
| **ASCII** | 1 byte (7-bit) | ✅ Identity | 128 characters | Legacy US systems |
| **ISO-8859-1** | 1 byte (8-bit) | ✅ Identity | 256 characters | Western Europe |
| **UTF-8** | 1 to 4 bytes | ✅ Identity | All 1,114,112 code points | Web, JSON, Files, Default |
| **UTF-16** | 2 or 4 bytes | ❌ Incompatible | All 1,114,112 code points | JVM runtime in-memory |

---

## 10. Interview Questions

<details>
<summary><strong>1. What major change did JEP 400 introduce in Java 18?</strong></summary>

JEP 400 made **UTF-8 the default charset** across the entire Java platform. Prior to Java 18, `Charset.defaultCharset()` was determined by the host operating system's locale (e.g. `windows-1252` on US Windows, `Shift-JIS` on Japanese Windows). Since Java 18, standard APIs like `FileReader`, `FileWriter`, and `String.getBytes()` default to UTF-8 uniformly across all operating systems.
</details>

<details>
<summary><strong>2. What is a Unicode Surrogate Pair in Java?</strong></summary>

Java's `char` type is 16 bits (`0x0000` to `0xFFFF`), which can only directly address the Basic Multilingual Plane (BMP). To represent supplementary characters (code points from `U+010000` to `U+10FFFF`, such as emojis), Java uses a surrogate pair: two consecutive 16-bit `char` units consisting of a High Surrogate (`\uD800`–`\uDBFF`) and a Low Surrogate (`\uDC00`–`\uDFFF`).
</details>

<details>
<summary><strong>3. What is the difference between a Code Point and a Grapheme Cluster?</strong></summary>

A **Code Point** is a specific numeric value assigned by the Unicode standard (e.g., `U+1F468` for 👨).
A **Grapheme Cluster** is a user-perceived visual character glyph. A single grapheme cluster may be composed of multiple code points combined using Zero-Width Joiners (ZWJ) or modifier code points (e.g., 👨‍💻 is composed of Man + ZWJ + Laptop = 3 code points, but 1 grapheme cluster).
</details>

<details>
<summary><strong>4. How should you safely reverse a string that contains emoji?</strong></summary>

Do not reverse using a naive `char` array or standard `StringBuilder.reverse()`, as this reverses individual surrogate halves into invalid sequences (`Low` then `High`). Instead, extract the code points using `str.codePoints().toArray()`, reverse the integer code point array, and reconstruct the string with `new String(codePoints, 0, codePoints.length)`.
</details>

<details>
<summary><strong>5. Why does StandardCharsets.UTF_8 exist when Charset.forName("UTF-8") is available?</strong></summary>

`Charset.forName("UTF-8")` requires runtime string lookup, which performs synchronized hash table checks and can throw `UnsupportedCharsetException` if misspelled. `StandardCharsets.UTF_8` is a statically initialized constant guaranteed by the JVM, avoiding runtime lookups and checked exceptions.
</details>

<details>
<summary><strong>6. Does Java's InputStreamReader automatically strip the UTF-8 Byte Order Mark (BOM)?</strong></summary>

No. Java's standard `InputStreamReader` and `String` constructors do not discard the UTF-8 BOM (`0xEF 0xBB 0xBF`). It is preserved as character `\uFEFF` (zero-width non-breaking space) at index 0 of the string. Applications parsing UTF-8 files from Windows must explicitly check for and strip `\uFEFF`.
</details>

<details>
<summary><strong>7. How do you detect if a char is part of a surrogate pair?</strong></summary>

Use `Character.isHighSurrogate(c)` and `Character.isLowSurrogate(c)`, or `Character.isSurrogate(c)`.
</details>

<details>
<summary><strong>8. What happens if a CharsetDecoder encounters a malformed byte sequence?</strong></summary>

By default, an NIO `CharsetDecoder` reports an error by returning a `CoderResult` indicating malformed input. You can configure its behavior via `.onMalformedInput(action)`:
- `CodingErrorAction.REPORT`: Throws `MalformedInputException`.
- `CodingErrorAction.REPLACE`: Replaces the bad sequence with a replacement character (usually `\uFFFD` ).
- `CodingErrorAction.IGNORE`: Silently discards the invalid bytes.
</details>

---

## 11. Practice Exercises

### Easy
1. **Count True Unicode Characters:** Write a function that returns the true Unicode character count of any string using `codePointCount()`.
2. **UTF-8 Byte Length:** Measure the exact UTF-8 byte length of a string without allocating a byte array using `StandardCharsets.UTF_8.newEncoder()`.
3. **Hex Code Point Printer:** Print each character in a string with its formal Unicode notation (`U+XXXX`).

### Medium
4. **BOM Stripper Stream:** Implement a custom `InputStream` filter that detects and strips a leading UTF-8 Byte Order Mark (`0xEF 0xBB 0xBF`) from an incoming stream.
5. **Surrogate Pair Safe Truncator:** Implement `String truncateSafely(String text, int maxChars)` that ensures truncation never cuts a surrogate pair in half.
6. **Diacritic Normalizer:** Normalize accented characters (e.g. `"résumé"` $\to$ `"resume"`) using `java.text.Normalizer` and Unicode decomposition (NFD).

### Hard
7. **Extended Grapheme Cluster Splitter:** Write a tokenizer that splits a string into individual user-perceived grapheme clusters without using the `\X` regex shortcut.
8. **High-Speed UTF-8 Validator:** Implement an allocation-free UTF-8 byte validator that validates whether a byte buffer contains well-formed UTF-8 according to RFC 3629.
9. **Zero-Copy Charset Transcoder:** Transcode a buffer from ISO-8859-1 to UTF-8 using direct off-heap NIO `ByteBuffer` channels without allocating Java heap objects.

<details>
<summary><strong>Exercise Solutions</strong></summary>

```java
// Solution to Exercise 1: True Unicode Character Count
public static int getTrueCharacterCount(String s) {
    if (s == null) return 0;
    return s.codePointCount(0, s.length());
}

// Solution to Exercise 5: Safe Truncator
public static String truncateSafely(String text, int maxCodePoints) {
    if (text == null) return null;
    int totalCodePoints = text.codePointCount(0, text.length());
    if (totalCodePoints <= maxCodePoints) return text;

    int endIndex = text.offsetByCodePoints(0, maxCodePoints);
    return text.substring(0, endIndex);
}
```
</details>

---

## 12. Summary & Cheat Sheet

```
┌────────────────────────────────────────────────────────────────────────┐
│                   UNICODE & CHARSETS CHEAT SHEET                       │
├────────────────────────────────────────────────────────────────────────┤
│ Java 18 Default:   UTF-8 by Default everywhere (JEP 400)               │
│ Standard Constant: StandardCharsets.UTF_8 (Always specify explicitly)  │
│ Char vs Code Point:char is 16-bit (BMP only). Code point is 21-bit     │
│ Supplementary:     Code points > 0xFFFF use Surrogate Pairs            │
│ High Surrogate:    \uD800 to \uDBFF                                    │
│ Low Surrogate:     \uDC00 to \uDFFF                                    │
│ Length check:      s.codePointCount(0, s.length())                     │
│ Traversal:         s.codePoints().forEach(...)                         │
│ Grapheme Cluster:  Visual glyphs with modifiers/ZWJ; use \\X in regex  │
│ BOM Handling:      UTF-8 BOM is \uFEFF; must be stripped explicitly    │
└────────────────────────────────────────────────────────────────────────┘
```

---

## 13. Further Reading
- [JEP 400: UTF-8 by Default (Java 18)](https://openjdk.org/jeps/400)
- The Unicode Standard 16.0 Specification
- RFC 3629: UTF-8, a transformation format of ISO 10646
- Official Java SE 25 `java.nio.charset.Charset` Documentation
