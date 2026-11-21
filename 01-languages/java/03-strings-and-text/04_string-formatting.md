# String Formatting, MessageFormat, and Modern Text Templating

> A comprehensive reference for string formatting in Java, exploring `Formatter` specifiers, Java 15 `.formatted()`, localized `MessageFormat`, and the status of String Templates.

| Metadata | Details |
| :--- | :--- |
| **Level** | Intermediate |
| **Prerequisites** | [00_string-basics.md](./00_string-basics.md), [01_string-methods.md](./01_string-methods.md) |
| **Time to Read** | 25 minutes |
| **Status** | Active |
| **Java Version** | Java 8+ (`.formatted()` in Java 15+; String Templates withdrawn in Java 23) |

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
9. [Comparison: Formatting Mechanisms](#9-comparison-formatting-mechanisms)
10. [Interview Questions](#10-interview-questions)
11. [Practice Exercises](#11-practice-exercises)
12. [Summary & Cheat Sheet](#12-summary--cheat-sheet)
13. [Further Reading](#13-further-reading)

---

## 1. What & Why

Composing structured, human-readable strings from heterogeneous data types (numbers, dates, flags, identifiers) is a recurring task across logging, UI presentation, and document generation.

While `+` string concatenation works for simple text, it quickly becomes unreadable and unwieldy when numbers require fixed decimal precision, integers require zero-padding, or dates require localized formatting.

Java provides multiple formatting engines:
1. **`java.util.Formatter` (`String.format()`, `System.out.printf()`):** C-style printf formatting.
2. **`String.formatted(Object... args)` (Java 15+):** A fluent instance method built directly on `String.format()`.
3. **`java.text.MessageFormat`:** Localized placeholder-based formatting (`{0,number,currency}`) ideal for internationalized UI messages.
4. **Text Blocks + Formatting:** The modern idiomatic standard for generating formatted multi-line text (JSON, SQL, HTML).

> [!IMPORTANT]
> **Status of String Templates (`STR."..."`):**
> JEP 430 (Java 21) and JEP 459 (Java 22) introduced String Templates as a preview feature. However, based on architecture and security feedback from the Java community and OpenJDK architects, **String Templates were WITHDRAWN in JDK 23**. They are NOT present in Java 23, Java 24, or Java 25. Modern Java developers use Text Blocks combined with `.formatted()` or custom builder APIs.

---

## 2. Core Concepts

### 2.1 The Format Specifier Grammar
A format specifier in Java follows this formal syntax:
```
%[argument_index$][flags][width][.precision]conversion
```

Components:
- **`%`:** Begins the format specifier.
- **`argument_index$`:** 1-based index pointing to the parameter list (e.g. `%1$s`, `%2$d`). Enables reordering arguments or reusing the same argument multiple times.
- **`flags`:** Modifies output (e.g., `-` for left-justify, `0` for zero-padding, `,` for locale thousand-grouping separator, `+` for explicit sign).
- **`width`:** The minimum number of characters written to the output.
- **`.precision`:** For floating-point, specifies digits after the decimal point; for strings, specifies maximum character truncation.
- **`conversion`:** Mandatory character indicating how the argument is formatted (`s` for string, `d` for decimal integer, `f` for float/double, `b` for boolean, `t` for date/time, `%` for literal percent).

### 2.2 `%n` vs `\n`
Always use `%n` instead of `\n` inside format strings. `%n` emits the **platform-specific line separator** (`\r\n` on Windows, `\n` on Linux/macOS) determined by `System.lineSeparator()`.

---

## 3. How It Works Internally

### 3.1 `java.util.Formatter` Architecture
When you invoke `String.format(format, args)` or `format.formatted(args)`:
```
Caller
  │
  ▼
String.format(format, args)
  │
  ▼
new Formatter().format(Locale.getDefault(), format, args).toString()
  │
  ▼
Regex parsing of format specifier tokens (%[index$][flags][width][.precision]conversion)
  │
  ▼
Switch-based dispatcher to FormatSpecifier implementations
  │
  ▼
Append to internal StringBuilder
  │
  ▼
sb.toString() -> returns String
```

Because `java.util.Formatter` parses the format string using regular expressions and dynamic reflection on every single invocation, it is substantially slower than raw string concatenation. For high-throughput hot paths, pre-compiled templates or dedicated `StringBuilder` logic is preferred.

---

## 4. Syntax & Usage

### 4.1 Common Format Specifiers
```java
// Numbers
String.format("%d", 42);                  // "42" (decimal integer)
String.format("%05d", 42);                // "00042" (zero-padded to 5 digits)
String.format("%,d", 1_000_000);          // "1,000,000" (locale thousand separators)
String.format("%+d", 42);                 // "+42" (explicit sign)
String.format("%.2f", 3.14159);           // "3.14" (floating-point precision)
String.format("%10.2f", 3.14159);         // "      3.14" (width 10, right-aligned)
String.format("%-10.2f", 3.14159);        // "3.14      " (width 10, left-aligned)
String.format("%x", 255);                 // "ff" (hexadecimal)
String.format("%X", 255);                 // "FF" (uppercase hex)

// Strings & Booleans
String.format("%s", "Java");              // "Java"
String.format("%.3s", "Antigravity");     // "Ant" (truncate to 3 chars)
String.format("%b", true);                // "true"
String.format("%b", (Object) null);       // "false" (null converts to false with %b!)

// Argument Indexing & Reuse
String.format("%2$s -> %1$s", "second", "first"); // "first -> second"
String.format("%1$s %1$s %1$s", "Echo");          // "Echo Echo Echo"

// Java 15 Fluent .formatted()
String msg = "User %s has %d items".formatted("Alice", 5);
```

---

## 5. Practical Examples

### Example 1: Basic Columnar Table Generation
```java
public class ColumnarFormatterDemo {
    public static void main(String[] args) {
        String header = "%-6s | %-16s | %10s | %8s%n";
        String row    = "%-6d | %-16s | %,10.2f | %8d%n";

        System.out.printf(header, "ID", "PRODUCT", "PRICE ($)", "STOCK");
        System.out.println("-".repeat(50));
        System.out.printf(row, 101, "Wireless Mouse", 29.99, 150);
        System.out.printf(row, 102, "Mechanical Keybd", 129.50, 42);
        System.out.printf(row, 103, "4K Monitor 32\"", 499.00, 8);
    }
}
```
**Output:**
```text
ID     | PRODUCT          |  PRICE ($) |    STOCK
--------------------------------------------------
101    | Wireless Mouse   |      29.99 |      150
102    | Mechanical Keybd |     129.50 |       42
103    | 4K Monitor 32"   |     499.00 |        8
```

### Example 2: Real-World Localized Message Formatting with `MessageFormat`
```java
import java.text.MessageFormat;
import java.util.Date;
import java.util.Locale;

public class LocalizedMessageDemo {
    public static void main(String[] args) {
        String pattern = "At {1,time,short} on {1,date,long}, order #{0} was processed with total {2,number,currency}.";

        Object[] params = {
            104829,
            new Date(),
            249.95
        };

        // US English
        MessageFormat usFormat = new MessageFormat(pattern, Locale.US);
        System.out.println("US: " + usFormat.format(params));

        // Germany German
        MessageFormat deFormat = new MessageFormat(pattern, Locale.GERMANY);
        System.out.println("DE: " + deFormat.format(params));
    }
}
```
**Output:**
```text
US: At ... on ..., order #104,829 was processed with total $249.95.
DE: At ... on ..., order #104.829 was processed with total 249,95 €.
```

### Example 3: Edge Case – The `%b` Null Trap
```java
public class BooleanFormatTrap {
    public static void main(String[] args) {
        String nullStr = null;
        Boolean nullBool = null;

        // %b prints "false" if argument is null, "true" if non-null and not Boolean.FALSE!
        System.out.println(String.format("null string with %%b: %b", nullStr));   // "false"
        System.out.println(String.format("null boolean with %%b: %b", nullBool)); // "false"
        System.out.println(String.format("'hello' with %%b: %b", "hello"));       // "true"!
    }
}
```
**Output:**
```text
null string with %b: false
null boolean with %b: false
'hello' with %b: true
```

### Example 4: Failure Case – Mismatched Types & Missing Arguments
```java
import java.util.MissingFormatArgumentException;
import java.util.IllegalFormatConversionException;

public class FormatExceptionDemo {
    public static void main(String[] args) {
        try {
            // Missing required argument for %d
            String.format("User %s, Age %d", "Alice");
        } catch (MissingFormatArgumentException e) {
            System.out.println("Caught: " + e.getClass().getSimpleName() + " for specifier: " + e.getFormatSpecifier());
        }

        try {
            // Passing String where decimal integer %d is expected
            String.format("Number: %d", "forty-two");
        } catch (IllegalFormatConversionException e) {
            System.out.println("Caught: " + e.getClass().getSimpleName() + " for conversion: " + e.getConversion());
        }
    }
}
```
**Output:**
```text
Caught: MissingFormatArgumentException for specifier: d
Caught: IllegalFormatConversionException for conversion: d
```

### Example 5: Optimized / Idiomatic – Modern Text Block Formatting
```java
public class TextBlockFormattingDemo {
    public static String buildUserPayload(long id, String name, String email, boolean active) {
        String template = """
            {
              "id": %d,
              "name": "%s",
              "email": "%s",
              "active": %b,
              "timestamp": %d
            }
            """;

        return template.formatted(id, name, email, active, System.currentTimeMillis());
    }

    public static void main(String[] args) {
        System.out.println(buildUserPayload(42, "Grace Hopper", "grace@navy.mil", true));
    }
}
```
**Output:**
```text
{
  "id": 42,
  "name": "Grace Hopper",
  "email": "grace@navy.mil",
  "active": true,
  "timestamp": ...
}
```

---

## 6. Best Practices

1. **Use `%n` for Newlines in Format Strings:** Never hardcode `\n` inside format templates; `%n` resolves the correct platform newline.
2. **Use Java 15 `.formatted()` on String Literals and Text Blocks:** Write `"""SELECT ...""".formatted(id)` instead of `String.format("""SELECT ...""", id)`. It reads left-to-right fluently.
3. **Prefer `MessageFormat` for Internationalized UI Text:** When supporting multilingual bundles, translators often need to reorder words and placeholders; `MessageFormat` with `{0}`, `{1}` permits index shuffling without altering Java code.
4. **Avoid `String.format()` in Microsecond-Sensitive Hot Paths:** `Formatter` allocates multiple internal objects and parses regex tokens at runtime. In hot loops, use `StringBuilder` directly.
5. **Escape Literal `%` with `%%`:** If outputting a percentage, write `"Discount: %d%%".formatted(20)` -> `"Discount: 20%"`.

---

## 7. Common Mistakes & Pitfalls

| Mistake | Why It Is Wrong | Correct Idiom |
| :--- | :--- | :--- |
| Hardcoding `\n` in format string | Causes broken line endings when generated on Linux and viewed on Windows (or vice versa). | Use `%n`. |
| Unescaped `%` | Throws `UnknownFormatConversionException` at runtime. | Use `%%` for literal `%`. |
| `%b` for null checking | `%b` evaluates `null` as `"false"` and ANY non-null non-Boolean object as `"true"`. | Check `val != null` explicitly before formatting. |
| Type mismatch (e.g. `%d` with `Double`) | Throws `IllegalFormatConversionException` at runtime. | Match specifiers: `%d` for integer, `%f` for float/double. |
| Over-reliance in high-throughput logs | `String.format()` creates heavy GC churn compared to SLF4J parameterized logging (`log.info("{}", arg)`). | Use logging framework `{}` syntax in log statements. |
| Assuming String Templates (`STR."..."`) exist in Java 25 | Feature was previewed in Java 21/22 and permanently withdrawn in Java 23. | Use Text Blocks + `.formatted()`. |

---

## 8. Performance & Security Notes

### 8.1 Format String Injection
Never pass untrusted user input as the **format string** parameter itself:
```java
// CRITICAL SECURITY VULNERABILITY:
String userInput = "%s%s%s%s%s%s%s%s%s";
String.format(userInput, "arg"); // Throws MissingFormatArgumentException or crashes application!
```
Always use a hardcoded or constant format string and supply dynamic user values strictly through the variable arguments array.

---

## 9. Comparison: Formatting Mechanisms

| Mechanism | Speed | Localization Support | Type Safety | Best Used For |
| :--- | :--- | :--- | :--- | :--- |
| `+` Concatenation | Fastest (invokedynamic) | None | Compile-time | Simple single statements |
| `StringBuilder` | Extremely Fast | None | Compile-time | Loops, dynamic assembly |
| `String.format()` | Moderate (regex parsing) | Basic (Locale) | Runtime checked | Console output, tables |
| `.formatted()` | Moderate (delegates to Formatter)| Basic (Locale) | Runtime checked | Text blocks, templates |
| `MessageFormat` | Moderate | Advanced (Date/Currency) | Runtime checked | i18n resource bundles |
| SLF4J / Log4j2 `{}` | Fast (zero parsing if disabled)| None | Runtime checked | Application logging |

---

## 10. Interview Questions

<details>
<summary><strong>1. What happened to String Templates (STR."...") in modern Java?</strong></summary>

String Templates were introduced as a preview feature under JEP 430 in Java 21 and refreshed in JEP 459 in Java 22. However, based on architecture and security evaluations regarding processor syntax and design cohesion, the feature was **withdrawn in JDK 23**. It is not part of Java 23, 24, or 25 LTS. Modern Java utilizes Text Blocks combined with `.formatted(...)` or custom builder APIs for safe text generation.
</details>

<details>
<summary><strong>2. Why should developers use %n instead of \n in format strings?</strong></summary>

`\n` explicitly outputs the Unix newline character (`0x0A`). On Windows systems, standard text files expect CRLF (`\r\n`, `0x0D 0x0A`). `%n` automatically resolves to the platform-specific line separator dynamically via `System.lineSeparator()`.
</details>

<details>
<summary><strong>3. How does %b handle arguments that are not Boolean?</strong></summary>

If the argument to `%b` is `null`, it prints `"false"`. If the argument is an instance of `java.lang.Boolean`, it prints the boolean value (`"true"` or `"false"`). For **any other non-null object** (such as `"false"` as a String or `0` as an Integer), `%b` evaluates the presence of the object and prints `"true"`.
</details>

<details>
<summary><strong>4. What does the String.formatted() method introduced in Java 15 provide?</strong></summary>

`formatted(Object... args)` is an instance method on `java.lang.String` equivalent to `String.format(this, args)`. It allows fluent, readable chaining directly on string literals and text blocks without nesting the string as the first argument in `String.format(...)`.
</details>

<details>
<summary><strong>5. What is the difference between %s and %S?</strong></summary>

`%s` formats the argument as a string using `String.valueOf(arg)`. `%S` formats the string and automatically converts all characters to uppercase according to the specified `Locale`.
</details>

<details>
<summary><strong>6. How do you format a number with thousands separators and two decimal places?</strong></summary>

Use `%,.2f`:
- `,` is the flag for grouping separators.
- `.2` specifies two decimal places.
- `f` is the conversion for floating-point numbers.
Example: `String.format("%,.2f", 1234567.89)` yields `"1,234,567.89"` in US locale.
</details>

<details>
<summary><strong>7. How can you reuse the same argument across multiple specifiers?</strong></summary>

Use argument index notation: `%1$s`. The number before the `$` indicates the 1-based index in the parameter list.
Example: `String.format("%1$s is %1$s", "Java")` returns `"Java is Java"`.
</details>

<details>
<summary><strong>8. Why is String.format() not recommended inside performance-critical loops?</strong></summary>

`String.format()` instantiates a `java.util.Formatter` object, parses the format string using regular expressions, boxes primitive parameters into heap objects (e.g., `int` to `Integer`), and builds output into an intermediate buffer. Inside a tight loop executing millions of iterations, this generates massive heap allocation and GC pauses.
</details>

---

## 11. Practice Exercises

### Easy
1. **Padded Hex Viewer:** Format an integer array as a hex dump where each byte is displayed as a two-digit uppercase hex string (`%02X`) separated by spaces.
2. **Currency Formatter:** Given a double value, format it as currency with two decimal places and thousands grouping.
3. **Log Header Formatter:** Use `.formatted()` to format a log message with level, thread name, and message text.

### Medium
4. **ANSI Colored Terminal Formatter:** Build a helper that takes text, foreground color, and background color ANSI codes and formats terminal output using `%s` templates.
5. **Dynamic SQL Insert Builder:** Create a method that accepts a table name and a `Map<String, Object>` of column-value pairs, generating a formatted SQL `INSERT INTO table (cols) VALUES (vals)` statement.
6. **Multi-Locale Invoice Summary:** Given order details, use `MessageFormat` to produce invoice summaries in English (`en-US`), French (`fr-FR`), and Japanese (`ja-JP`).

### Hard
7. **High-Throughput Fast Formatter:** Write a specialized numeric formatter for integers that formats zero-padded decimals into a byte array without allocating any `String` or `Formatter` objects.
8. **Safe Template Engine:** Implement a safe template processor that replaces named tokens like `${userName}` and `${orderId}` in a text block, validating tokens against an allowlist to prevent injection vulnerabilities.
9. **JMH Benchmark: format vs StringBuilder:** Author a JMH benchmark comparing `+`, `StringBuilder`, `String.format()`, and `.formatted()` across 1,000,000 operations.

<details>
<summary><strong>Exercise Solutions</strong></summary>

```java
// Solution to Exercise 1: Padded Hex Viewer
public static String toHexDump(byte[] bytes) {
    if (bytes == null) return "";
    StringBuilder sb = new StringBuilder(bytes.length * 3);
    for (int i = 0; i < bytes.length; i++) {
        if (i > 0) sb.append(' ');
        sb.append(String.format("%02X", bytes[i]));
    }
    return sb.toString();
}

// Solution to Exercise 3: Log Header Formatter
public static String formatLog(String level, String msg) {
    return "[%-5s] [%-15s] %s%n".formatted(
        level,
        Thread.currentThread().getName(),
        msg
    );
}
```
</details>

---

## 12. Summary & Cheat Sheet

```
┌────────────────────────────────────────────────────────────────────────┐
│                      STRING FORMATTING CHEAT SHEET                     │
├────────────────────────────────────────────────────────────────────────┤
│ Syntax:            %[index$][flags][width][.precision]conversion       │
│ Common Specifiers: %d (int), %f (float), %s (str), %b (bool), %x (hex) │
│ Padding & Width:   %05d (e.g. 00042), %10s (right align), %-10s (left) │
│ Separators:        %,d (e.g. 1,000,000), %.2f (2 decimals)             │
│ Platform Newline:  %n (Resolves \r\n on Windows, \n on Unix)           │
│ Literal Percent:   %% (Escape percent sign)                            │
│ Fluent Java 15:    "Hello %s".formatted(name)                          │
│ MessageFormat:     {0,number,currency} (Ideal for i18n resource bundles│
│ String Templates:  WITHDRAWN in JDK 23 (Use Text Blocks + .formatted())│
└────────────────────────────────────────────────────────────────────────┘
```

---

## 13. Further Reading
- Official Java 25 `java.util.Formatter` Specification
- [JEP 378: Text Blocks](https://openjdk.org/jeps/378)
- OpenJDK Note on the Withdrawal of String Templates in JDK 23
