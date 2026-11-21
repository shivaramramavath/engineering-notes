# Text Blocks: Multi-Line String Literals in Java

> A comprehensive deep-dive into Java Text Blocks (JEP 378), covering delimiter rules, compile-time whitespace stripping algorithms, new escape sequences, and multi-line templating.

| Metadata | Details |
| :--- | :--- |
| **Level** | Intermediate |
| **Prerequisites** | [00_string-basics.md](./00_string-basics.md), [04_string-formatting.md](./04_string-formatting.md) |
| **Time to Read** | 25 minutes |
| **Status** | Active |
| **Java Version** | Java 15 LTS+ (Preview in Java 13/14, Standard in Java 15, current in Java 21/25) |

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
9. [Comparison: Traditional Strings vs Text Blocks](#9-comparison-traditional-strings-vs-text-blocks)
10. [Interview Questions](#10-interview-questions)
11. [Practice Exercises](#11-practice-exercises)
12. [Summary & Cheat Sheet](#12-summary--cheat-sheet)
13. [Further Reading](#13-further-reading)

---

## 1. What & Why

Historically in Java, declaring multi-line string literals (such as SQL queries, HTML documents, JSON payloads, or shell scripts) was notoriously clumsy and error-prone. Developers had to concatenate individual lines with `+`, manually insert `\n` or `\r\n` escapes, and escape every internal quotation mark with `\"`:

```java
// Pre-Java 15 Clunky Multi-Line String:
String query = "SELECT id, username, email\n" +
               "FROM users\n" +
               "WHERE status = 'ACTIVE'\n" +
               "ORDER BY created_at DESC;\n";
```

To eliminate this friction, **Text Blocks** were finalized in **Java 15 LTS (JEP 378)**. A text block is a multi-line string literal that avoids the need for most escape sequences, automatically preserves layout, and smartly strips incidental indentation based on compiler whitespace stripping rules.

---

## 2. Core Concepts

### 2.1 The Delimiter Rule
A text block opens with three double-quote characters `"""` followed by zero or more white space characters and a **mandatory line terminator**. The text block content begins on the line **after** the opening delimiter:

```java
// Correct: Content starts on the next line
String html = """
              <html>
                  <body>Hello</body>
              </html>
              """;

// SYNTAX ERROR: Content on the same line as opening """ is illegal!
// String bad = """<html>""";
```

The closing delimiter `"""` marks the end of the text block and crucially acts as the **indentation anchor**.

---

## 3. How It Works Internally

### 3.1 The 3-Step Compilation Algorithm
At compile time, `javac` processes every text block through a deterministic 3-step algorithm:

```
Source Code with Text Block
            │
            ▼
┌────────────────────────────────────────┐
│ Step 1: Line Terminator Normalization  │  All \r\n and \r are converted
│                                        │  to standard \n line terminators
└───────────────────┬────────────────────┘
                    │
                    ▼
┌────────────────────────────────────────┐
│ Step 2: Incidental Whitespace Removal  │  Calculates common whitespace prefix
│                                        │  relative to the closing """ anchor
└───────────────────┬────────────────────┘
                    │
                    ▼
┌────────────────────────────────────────┐
│ Step 3: Escape Sequence Interpretation │  Translates \s, \, \", \\
│                                        │  and traditional escape sequences
└───────────────────┬────────────────────┘
                    │
                    ▼
Produces Ordinary CONSTANT_String_info in Bytecode
```

### 3.2 The Indentation Anchor Explained
The horizontal column position of the closing `"""` establishes how many leading spaces are considered "incidental":

```java
// Example A: Closing quotes aligned with content
String sqlA = """
              SELECT *
              FROM users
              """;
// Result has 0 leading spaces on both lines!

// Example B: Closing quotes shifted left by 4 columns (Indentation Anchor)
String sqlB = """
                  SELECT *
                  FROM users
              """;
// Result preserves 4 leading spaces before SELECT and FROM!
```

---

## 4. Syntax & Usage

### 4.1 New Escape Sequences for Text Blocks
Text blocks introduced two critical escape sequences:
1. **`\s` (Explicit Space):** Prevents the compiler from stripping trailing spaces at the end of a line.
2. **`\` (Line Continuation):** Suppresses the line terminator, allowing long single-line strings to be formatted comfortably across multiple editor lines without introducing newlines in the string value.

```java
String singleLine = """
                    This is a very long SQL query that we want \
                    to write on multiple editor lines without \
                    introducing newline characters in the final string.\
                    """;
```

### 4.2 Quotation Marks Inside Text Blocks
Inside a text block, single quotes (`'`) and unescaped double quotes (`"`) do **not** need to be escaped:
```java
String json = """
              {
                "user": "Alice",
                "role": "Admin"
              }
              """;
```
You only need to escape a quote if you write three consecutive double quotes (`\"""`).

---

## 5. Practical Examples

### Example 1: Clean JSON Payload Construction
```java
public class TextBlockJsonDemo {
    public static void main(String[] args) {
        String jsonPayload = """
            {
              "service": "billing",
              "port": 8080,
              "features": ["metrics", "health", "tracing"]
            }
            """;

        System.out.println(jsonPayload);
    }
}
```
**Output:**
```text
{
  "service": "billing",
  "port": 8080,
  "features": ["metrics", "health", "tracing"]
}
```

### Example 2: Real-World Formatted SQL Template with Fluent `.formatted()`
```java
public class SqlTemplateDemo {
    public static String buildUserQuery(String role, int limit, int offset) {
        return """
            SELECT u.id, u.username, u.email, r.role_name
            FROM application_users u
            JOIN user_roles r ON u.role_id = r.id
            WHERE r.role_name = '%s'
              AND u.is_deleted = FALSE
            ORDER BY u.created_at DESC
            LIMIT %d OFFSET %d;
            """.formatted(role, limit, offset);
    }

    public static void main(String[] args) {
        System.out.println(buildUserQuery("ADMIN", 25, 0));
    }
}
```
**Output:**
```text
SELECT u.id, u.username, u.email, r.role_name
FROM application_users u
JOIN user_roles r ON u.role_id = r.id
WHERE r.role_name = 'ADMIN'
  AND u.is_deleted = FALSE
ORDER BY u.created_at DESC
LIMIT 25 OFFSET 0;
```

### Example 3: Edge Case – Trailing Whitespace and `\s`
```java
public class TrailingWhitespaceDemo {
    public static void main(String[] args) {
        // By default, the compiler strips trailing whitespace from every line of a text block
        String stripped = """
            Line 1   
            Line 2   
            """;

        // Using \s forces trailing whitespace to be preserved
        String preserved = """
            Line 1  \s
            Line 2  \s
            """;

        System.out.println("Stripped line 1 length:  " + stripped.lines().findFirst().get().length()); // 6
        System.out.println("Preserved line 1 length: " + preserved.lines().findFirst().get().length()); // 9
    }
}
```
**Output:**
```text
Stripped line 1 length:  6
Preserved line 1 length: 9
```

### Example 4: Failure Case – Opening Delimiter on the Same Line
```java
public class IllegalTextBlockSyntax {
    public static void main(String[] args) {
        // The following line will NOT compile in javac:
        // String bad = """SELECT * FROM users""";
        // Compiler error: illegal text block open delimiter sequence, missing line terminator
        System.out.println("Opening delimiter MUST be followed by a line terminator!");
    }
}
```
**Output:**
```text
Opening delimiter MUST be followed by a line terminator!
```

### Example 5: Optimized / Idiomatic – HTML Email Generator with Escapes
```java
public class HtmlEmailGenerator {
    public static String generateWelcomeEmail(String userName, String resetLink) {
        return """
            <!DOCTYPE html>
            <html lang="en">
            <head>
                <meta charset="UTF-8">
                <title>Welcome to the Platform</title>
            </head>
            <body style="font-family: sans-serif; line-height: 1.6;">
                <h2>Welcome, %s!</h2>
                <p>Your account has been provisioned successfully.</p>
                <a href="%s" style="padding: 10px 20px; background-color: #007bff; color: white; text-decoration: none;">
                    Activate Account
                </a>
            </body>
            </html>
            """.formatted(userName, resetLink);
    }

    public static void main(String[] args) {
        System.out.println(generateWelcomeEmail("Ada Lovelace", "https://auth.company.com/activate?token=xyz"));
    }
}
```
**Output:**
```text
<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <title>Welcome to the Platform</title>
</head>
<body style="font-family: sans-serif; line-height: 1.6;">
    <h2>Welcome, Ada Lovelace!</h2>
    <p>Your account has been provisioned successfully.</p>
    <a href="https://auth.company.com/activate?token=xyz" style="padding: 10px 20px; background-color: #007bff; color: white; text-decoration: none;">
        Activate Account
    </a>
</body>
</html>
```

---

## 6. Best Practices

1. **Align the Closing Delimiter `"""` with the Desired Base Indentation:** If you want zero leading whitespace, place `"""` in column 1 or aligned exactly with the leftmost character of the block content.
2. **Combine with `.formatted()` for Dynamic Values:** Use standard format specifiers (`%s`, `%d`) inside the text block and chain `.formatted(args...)`.
3. **Use `\` to Avoid Artificial Line Wrapping:** When a line must remain a single unbroken string in output (e.g. bash commands or long regex patterns), use trailing `\` to break the source code across lines without inserting newlines.
4. **Use `\s` When Trailing Spaces Matter:** For fixed-width formatting or Markdown where two trailing spaces denote a line break, terminate the line with `\s`.
5. **Never Escape Single or Double Quotes Inside Text Blocks:** Writing `\"` inside `"""` is unnecessary and clutters readability. Only escape if three consecutive quotes appear (`\"""`).

---

## 7. Common Mistakes & Pitfalls

| Mistake | Why It Is Wrong | Correct Idiom |
| :--- | :--- | :--- |
| Opening `"""` without line break | Compile error: `illegal text block open delimiter sequence`. | Must press Enter immediately after opening `"""`. |
| Unintended leading indentation | Indenting the content far to the right of the closing `"""` preserves accidental leading whitespace. | Align the closing `"""` with the left margin of your code block. |
| Expecting trailing spaces without `\s` | The compiler strips all trailing whitespace by default in Step 2. | Append `\s` at the end of the line. |
| Using `\r\n` expectations | Step 1 normalizes all line breaks to `\n` regardless of OS. | If Windows CRLF is required, use `.replace("\n", "\r\n")`. |
| Thinking text block creates a new type | Text blocks compile to ordinary `java.lang.String` constants; there is no runtime "TextBlock" object. | Treat as regular `String`. |

---

## 8. Performance & Security Notes

### 8.1 Zero Runtime Overhead
Text blocks are entirely resolved at compile time by `javac`. In the generated `.class` bytecode, a text block is represented identically to a traditional `CONSTANT_String_info` entry in the constant pool. There is zero runtime performance penalty, zero regex parsing overhead, and zero intermediate object creation compared to traditional string literals.

---

## 9. Comparison: Traditional Strings vs Text Blocks

| Feature | Traditional Literal (`"..."`) | Text Block (`"""..."""`) |
| :--- | :--- | :--- |
| **Multi-Line Syntax** | Requires explicit `+` and `\n` | Native multi-line support |
| **Quote Escaping** | Double quotes MUST be escaped (`\"`) | Double quotes do NOT require escaping |
| **Indentation Stripping** | None (manual) | Automated compile-time algorithm |
| **Line Ending Normalization** | Whatever is typed | Normalized to `\n` |
| **Runtime Representation** | `java.lang.String` | `java.lang.String` (identical) |
| **Line Continuation** | Not supported | Supported via `\` |
| **Preserve Trailing Spaces** | Automatic | Requires explicit `\s` |

---

## 10. Interview Questions

<details>
<summary><strong>1. In which Java release were Text Blocks standardized, and under which JEP?</strong></summary>

Text Blocks were finalized and made a permanent feature of the Java language in **Java 15 LTS** under **JEP 378**. They were previously previewed in Java 13 (JEP 355) and Java 14 (JEP 368).
</details>

<details>
<summary><strong>2. Describe the 3-step compilation process used by javac for text blocks.</strong></summary>

1. **Line Terminator Normalization:** All line terminators (`\r\n` and `\r`) are converted to Unix line feeds (`\n`).
2. **Incidental Whitespace Stripping:** The compiler determines the common whitespace prefix shared across all non-empty lines and the closing `"""` delimiter, and strips that prefix from every line.
3. **Escape Sequence Interpretation:** The compiler resolves escape sequences such as `\s` (preserve space), `\` (line continuation), and standard escapes (`\t`, `\b`, `\"`).
</details>

<details>
<summary><strong>3. How does the position of the closing """ delimiter affect the resulting string?</strong></summary>

The closing `"""` delimiter acts as the **indentation anchor**. If the closing `"""` is positioned to the left of the content lines, the difference in indentation between the closing delimiter and the content is treated as essential whitespace and preserved. If the closing delimiter is aligned with or to the right of the leftmost content line, no extra leading spaces are added.
</details>

<details>
<summary><strong>4. Why does String bad = """hello"""; fail to compile?</strong></summary>

The Java Language Specification strictly requires that the opening delimiter `"""` be followed by zero or more whitespace characters and an immediate line terminator. Having character content on the same line as the opening delimiter violates the grammar and produces a compilation error.
</details>

<details>
<summary><strong>5. What purpose does the \ escape sequence serve at the end of a line in a text block?</strong></summary>

The `\` escape sequence acts as a line continuation character. It instructs the compiler to suppress the newline terminator, allowing a long single-line string to be written across multiple lines in the source code without introducing newlines in the runtime string value.
</details>

<details>
<summary><strong>6. Why do text blocks strip trailing whitespace by default, and how can you preserve it?</strong></summary>

Most code editors automatically strip or inadvertently add trailing spaces to lines. To ensure reproducible compilation across different developer environments and editors, the Java compiler strips all trailing whitespace. To explicitly preserve trailing whitespace, end the line with the `\s` escape sequence.
</details>

<details>
<summary><strong>7. Are text blocks stored differently in bytecode compared to ordinary strings?</strong></summary>

No. In the compiled class file, a text block is represented as a standard `CONSTANT_String_info` entry in the constant pool. At runtime, it is an ordinary `java.lang.String` object with zero extra memory overhead or performance cost.
</details>

<details>
<summary><strong>8. Do you need to escape double quotes inside a text block?</strong></summary>

No. Single quotes and double quotes (`"`) do not need to be escaped inside a text block. You only need to escape a quote if three consecutive double quotes (`\"""`) appear in the text to prevent premature closure of the text block.
</details>

---

## 11. Practice Exercises

### Easy
1. **Multi-Line JSON Config:** Write a configuration JSON block using text blocks representing database credentials and port numbers.
2. **Bash Script Generator:** Generate a 5-line bash setup script with text blocks using `\n` and quotes without syntax errors.
3. **Ascii Art Banner:** Render an ASCII art company logo using text blocks and verify that leading spaces are correctly aligned.

### Medium
4. **Markdown Table Generator:** Create a method that generates a formatted multi-line Markdown table using text blocks and `.formatted()`.
5. **Indentation Anchor Verification:** Write a program demonstrating the exact character difference when the closing `"""` is placed in column 1 vs column 5.
6. **SQL Query Normalizer:** Build a utility that takes a SQL text block, strips indentation, and removes redundant internal whitespace into a compact single-line query.

### Hard
7. **Custom Text Block Indenter:** Implement your own version of `String.stripIndent()` that replicates `javac`'s incidental whitespace stripping algorithm on any arbitrary multi-line string.
8. **HTML Template Engine with Conditionals:** Build a lightweight HTML component renderer using text blocks that dynamically conditionally includes child elements.
9. **Multi-Dialect DDL Generator:** Construct a database schema generator that emits DDL tables for PostgreSQL and Oracle using text blocks and localized timestamp types.

<details>
<summary><strong>Exercise Solutions</strong></summary>

```java
// Solution to Exercise 1: Multi-Line JSON Config
public static String getDbConfig(String host, int port, String user) {
    return """
        {
          "database": {
            "host": "%s",
            "port": %d,
            "username": "%s",
            "ssl": true
          }
        }
        """.formatted(host, port, user);
}

// Solution to Exercise 5: Indentation Anchor Verification
public class AnchorVerification {
    public static void main(String[] args) {
        String zeroIndent = """
            SELECT 1
            """;
        String fourIndent = """
                SELECT 1
            """;
        System.out.println("Zero indent length: " + zeroIndent.length());
        System.out.println("Four indent length: " + fourIndent.length());
        System.out.println("Prefix in fourIndent: '" + fourIndent.substring(0, 4) + "'");
    }
}
```
</details>

---

## 12. Summary & Cheat Sheet

```
┌────────────────────────────────────────────────────────────────────────┐
│                        TEXT BLOCKS CHEAT SHEET                         │
├────────────────────────────────────────────────────────────────────────┤
│ Syntax:            String s = \"""                                     │
│                               line 1                                   │
│                               line 2                                   │
│                               \""";                                    │
│ Opening Delimiter: Must be followed immediately by a newline!          │
│ Closing Delimiter: Acts as the Indentation Anchor for whitespace       │
│ Quote Escaping:    " does NOT need to be escaped (only \""" if 3 consec)│
│ Newlines:          All lines automatically normalized to Unix \n       │
│ Line Continuation: \ at the end of line suppresses the newline         │
│ Trailing Spaces:   Stripped by default; use \s to preserve them        │
│ Interpolation:     Use .formatted(args...) on the text block           │
│ Bytecode:          Standard CONSTANT_String_info (Zero runtime overhead│
└────────────────────────────────────────────────────────────────────────┘
```

---

## 13. Further Reading
- [JEP 378: Text Blocks (Standard in Java 15)](https://openjdk.org/jeps/378)
- [JEP 355: Text Blocks (Preview)](https://openjdk.org/jeps/355)
- Java Language Specification (JLS) Section 3.10.6: Text Blocks
