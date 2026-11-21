# Regular Expressions in Java: Pattern and Matcher

> A comprehensive deep-dive into regular expression processing in Java, examining the NFA regex engine, Catastrophic Backtracking (ReDoS) prevention, named groups, and modern Stream integration.

| Metadata | Details |
| :--- | :--- |
| **Level** | Intermediate to Advanced |
| **Prerequisites** | [00_string-basics.md](./00_string-basics.md), [01_string-methods.md](./01_string-methods.md) |
| **Time to Read** | 25 minutes |
| **Status** | Active |
| **Java Version** | Java 8+ (`results()` and `replaceAll(Function)` in Java 9+) |

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
9. [Comparison: Greedy vs Reluctant vs Possessive Quantifiers](#9-comparison-greedy-vs-reluctant-vs-possessive-quantifiers)
10. [Interview Questions](#10-interview-questions)
11. [Practice Exercises](#11-practice-exercises)
12. [Summary & Cheat Sheet](#12-summary--cheat-sheet)
13. [Further Reading](#13-further-reading)

---

## 1. What & Why

Regular expressions (regex) provide a concise, declarative domain-specific language for string pattern matching, validation, extraction, and substitution.

In Java, regular expressions are implemented in the `java.util.regex` package, primarily anchored by two fundamental classes:
- **`java.util.regex.Pattern`:** An immutable, compiled representation of a regular expression pattern. Safe for concurrent use across multiple threads.
- **`java.util.regex.Matcher`:** A stateful, mutable engine that interprets the compiled pattern against a specific input character sequence. Not thread-safe.

Understanding how Java's regex engine compiles bytecode trees and evaluates input is essential for writing robust parsers and preventing catastrophic security vulnerabilities like ReDoS (Regular Expression Denial of Service).

---

## 2. Core Concepts

### 2.1 The Core Regex Architecture
```
Regex String Pattern (e.g. "[A-Z0-9._%+-]+@[A-Z0-9.-]+\\.[A-Z]{2,}")
                   │
                   ▼ Pattern.compile(regex, flags)
         ┌───────────────────┐
         │  Pattern Object   │  Immutable, Thread-Safe, Reusable
         └─────────┬─────────┘
                   │
                   ▼ pattern.matcher(inputCharSequence)
         ┌───────────────────┐
         │  Matcher Object   │  Stateful, Single-Threaded Cursor
         └───────────────────┘
```

### 2.2 Compilation Flags
Compile flags alter pattern semantics:
- `Pattern.CASE_INSENSITIVE` (`(?i)`): Case-insensitive matching (ASCII default).
- `Pattern.UNICODE_CASE` (`(?u)`): Enables Unicode-aware case folding.
- `Pattern.MULTILINE` (`(?m)`): `^` and `$` match beginning and end of lines, not just entire input.
- `Pattern.DOTALL` (`(?s)`): The `.` metacharacter matches ANY character, including line terminators.
- `Pattern.COMMENTS` (`(?x)`): Allows whitespace and comments (`#`) inside patterns for readable complex expressions.

---

## 3. How It Works Internally

### 3.1 Nondeterministic Finite Automaton (NFA)
Java's regex engine is an **NFA (Nondeterministic Finite Automaton)** backtracking engine:
- It processes regex tokens sequentially from left to right.
- When an ambiguity or quantifier occurs (`*`, `+`, `?`), the engine makes a speculative choice and records a **backtrack point** on its internal call stack.
- If subsequent tokens fail to match, the engine rewinds to the last backtrack point and tests alternative branches.

### 3.2 Catastrophic Backtracking (ReDoS)
When nested quantifiers operate on overlapping character sets (e.g., `(a+)+$`), an unmatched input string like `"aaaaaaaaaaaaaaaaaaaaX"` triggers exponential backtracking ($O(2^N)$):

```
Pattern: (a+)+$
Input:   "aaaaaaaaaaaaaaaaaaaaX"
Branching Tree:
  (a+) matches all 20 'a's -> fails on 'X'
  Backtrack: outer (a+) takes 19 'a's, second (a+) takes 1 'a' -> fails on 'X'
  Backtrack: outer takes 18 'a's, next takes 2 'a's -> fails on 'X'
  ...
  Total backtrack steps for N=20: 2^20 = 1,048,576 operations!
  CPU spikes to 100%, pinning worker threads indefinitely!
```

#### Defenses:
1. **Possessive Quantifiers (`++`, `*+`, `?+`):** Greedily consume all matching characters and **never** yield or backtrack.
2. **Atomic Grouping (`(?>...)`):** Once the subexpression matches, all internal backtrack points are permanently discarded.

---

## 4. Syntax & Usage

### 4.1 Matcher Methods
```java
Pattern pattern = Pattern.compile("\\b[A-Za-z0-9._%+-]+@[A-Za-z0-9.-]+\\.[A-Za-z]{2,}\\b");
Matcher matcher = pattern.matcher("Contact alice@corp.com or bob@navy.mil");

// 1. Full Match vs Subsequence Search
boolean fullMatch = matcher.matches();         // false (does whole string match?)
boolean found = matcher.find();                // true (searches for next match)

// 2. Extraction
if (found) {
    String matchedText = matcher.group();      // "alice@corp.com"
    int start = matcher.start();               // character index start
    int end = matcher.end();                   // character index end
}

// 3. Named Capturing Groups (Java 7+)
Pattern kvPattern = Pattern.compile("(?<key>[a-zA-Z]+)=(?<val>[0-9]+)");
Matcher kvMatcher = kvPattern.matcher("port=8080");
if (kvMatcher.matches()) {
    String key = kvMatcher.group("key");       // "port"
    String val = kvMatcher.group("val");       // "8080"
}

// 4. Stream Integration (Java 9+)
Stream<MatchResult> results = matcher.results();

// 5. Dynamic Lambda Replacement (Java 9+)
String output = matcher.replaceAll(matchResult ->
    matchResult.group().toUpperCase()
);
```

---

## 5. Practical Examples

### Example 1: Basic Email Validator and Extractor
```java
import java.util.regex.Pattern;
import java.util.regex.Matcher;

public class EmailExtractorDemo {
    private static final Pattern EMAIL_PATTERN = Pattern.compile(
        "[a-zA-Z0-9._%+-]+@[a-zA-Z0-9.-]+\\.[a-zA-Z]{2,}"
    );

    public static void main(String[] args) {
        String input = "Reports sent to admin@company.org and billing.dept@finance.co.uk by 5pm.";
        Matcher matcher = EMAIL_PATTERN.matcher(input);

        while (matcher.find()) {
            System.out.printf("Found email: %s [index %d-%d]%n",
                matcher.group(), matcher.start(), matcher.end());
        }
    }
}
```
**Output:**
```text
Found email: admin@company.org [index 16-33]
Found email: billing.dept@finance.co.uk [index 38-65]
```

### Example 2: Real-World Structured Log Parser with Named Groups
```java
import java.util.regex.Pattern;
import java.util.regex.Matcher;

public class LogRegexParser {
    private static final Pattern LOG_PATTERN = Pattern.compile(
        "^(?<date>\\d{4}-\\d{2}-\\d{2})\\s+\\[(?<level>[A-Z]+)\\]\\s+(?<logger>[\\w.]+):\\s+(?<msg>.*)$"
    );

    public record LogRecord(String date, String level, String logger, String message) {}

    public static LogRecord parse(String line) {
        Matcher m = LOG_PATTERN.matcher(line);
        if (m.matches()) {
            return new LogRecord(
                m.group("date"),
                m.group("level"),
                m.group("logger"),
                m.group("msg")
            );
        }
        return null;
    }

    public static void main(String[] args) {
        String logLine = "2026-11-21 [ERROR] com.app.OrderService: Payment gateway timeout after 3000ms";
        LogRecord record = parse(logLine);
        if (record != null) {
            System.out.println("Parsed Level:   " + record.level());
            System.out.println("Parsed Logger:  " + record.logger());
            System.out.println("Parsed Message: " + record.message());
        }
    }
}
```
**Output:**
```text
Parsed Level:   ERROR
Parsed Logger:  com.app.OrderService
Parsed Message: Payment gateway timeout after 3000ms
```

### Example 3: Edge Case – Catastrophic Backtracking Mitigation
```java
import java.util.regex.Pattern;
import java.util.regex.Matcher;

public class RedosMitigationDemo {
    // VULNERABLE: Exponential backtracking on unmatched input
    private static final Pattern VULNERABLE = Pattern.compile("([a-zA-Z0-9]+)+$");

    // SAFE: Possessive quantifier eliminates all backtracking
    private static final Pattern SAFE = Pattern.compile("([a-zA-Z0-9]++)++$");

    public static void main(String[] args) {
        String maliciousInput = "aaaaaaaaaaaaaaaaaaaaaaaaaaaa!";

        long start = System.currentTimeMillis();
        Matcher safeMatcher = SAFE.matcher(maliciousInput);
        boolean matched = safeMatcher.matches();
        long elapsed = System.currentTimeMillis() - start;

        System.out.println("Safe pattern result: " + matched + " in " + elapsed + " ms");
    }
}
```
**Output:**
```text
Safe pattern result: false in 0 ms
```

### Example 4: Failure Case – Forgetting `Pattern.quote()` with User Input
```java
import java.util.regex.Pattern;

public class PatternQuoteTrap {
    public static void main(String[] args) {
        String userInputPrice = "$19.99";

        // WRONG: Regex interprets '$' as end of line and '.' as any char!
        String wrongRegex = userInputPrice;
        System.out.println("Matches without quote: " + "19a99".matches(wrongRegex)); // false

        // CORRECT: Pattern.quote wraps input in \Q and \E to treat all characters literally
        String safeRegex = Pattern.quote(userInputPrice);
        System.out.println("Matches with quote:    " + "$19.99".matches(safeRegex));   // true
    }
}
```
**Output:**
```text
Matches without quote: false
Matches with quote:    true
```

### Example 5: Optimized / Idiomatic – Java 9+ `replaceAll(Function)`
```java
import java.util.regex.Pattern;
import java.util.regex.Matcher;
import java.util.Map;

public class TemplateReplacerDemo {
    private static final Pattern TOKEN_PATTERN = Pattern.compile("\\$\\{([a-zA-Z0-9_]+)\\}");

    public static String substitute(String template, Map<String, String> values) {
        Matcher matcher = TOKEN_PATTERN.matcher(template);
        return matcher.replaceAll(matchResult -> {
            String key = matchResult.group(1);
            return values.getOrDefault(key, matchResult.group(0));
        });
    }

    public static void main(String[] args) {
        String template = "Hello ${name}, your current balance is ${balance}. Reference: ${ref}";
        Map<String, String> vars = Map.of(
            "name", "Ada Lovelace",
            "balance", "$450.00",
            "ref", "REF-9921"
        );

        System.out.println(substitute(template, vars));
    }
}
```
**Output:**
```text
Hello Ada Lovelace, your current balance is $450.00. Reference: REF-9921
```

---

## 6. Best Practices

1. **Pre-Compile `Pattern` as `static final`:** Never invoke `str.matches(regex)` or `Pattern.compile(regex)` in loops. Compiling a regex allocates AST nodes and compiles bytecode; reuse a static `Pattern` instance.
2. **Use Possessive Quantifiers (`++`, `*+`) for User Validation:** Defend against ReDoS attacks by preventing the NFA engine from attempting futile backtracking on mismatched input.
3. **Use `Pattern.quote()` on Dynamic User Inputs:** When searching for arbitrary user strings using regex, always escape metacharacters using `Pattern.quote(userInput)`.
4. **Leverage Named Capturing Groups:** Use `(?<name>...)` instead of numeric indexes (`group(1)`, `group(2)`) to make complex patterns readable and refactor-proof.
5. **Use `matcher.results()` with Stream API:** In modern Java, process all matches functionally using `matcher.results().map(...)` rather than writing manual `while (matcher.find())` loops.

---

## 7. Common Mistakes & Pitfalls

| Mistake | Why It Is Wrong | Correct Idiom |
| :--- | :--- | :--- |
| `str.matches("pattern")` in hot loops | Compiles the pattern on every single call, causing massive CPU and GC overhead. | Pre-compile `static final Pattern`. |
| Unescaped metacharacters in `split()` | Writing `s.split(".")` or `s.split("|")` matches any character or empty strings. | `s.split("\\.")` or `s.split(Pattern.quote("."))`. |
| Nested greedy quantifiers `(a+)+` | Triggers ReDoS exponential backtracking on non-matching inputs. | Use possessive quantifiers `(a++)++` or atomic groups. |
| Forgetting double backslash in Java | Java string literals require `\\` to pass a single literal `\` to the regex engine. | Write `\\d` for digits, `\\s` for whitespace. |
| Confusing `matches()` with `find()` | `matches()` requires the ENTIRE input string to match; `find()` searches for substrings. | Use `find()` for substring searches. |
| Accessing `group(1)` without checking `find()` | Throws `IllegalStateException: No match found`. | Always check `if (matcher.find())` before calling `group()`. |

---

## 8. Performance & Security Notes

### 8.1 ReDoS Vulnerability Scanning
Always test complex regular expressions against worst-case inputs before deploying to production. Patterns containing nested repetitions (`(x+)*`, `(a|aa)+`, `(.*a)+`) are inherently susceptible to denial-of-service attacks. If input lengths cannot be bounded, use strict character classes and possessive quantifiers.

---

## 9. Comparison: Greedy vs Reluctant vs Possessive Quantifiers

| Type | Syntax | Match Behavior | Backtracks on Mismatch? |
| :--- | :--- | :--- | :--- |
| **Greedy** | `X*`, `X+`, `X?` | Consumes as much as possible, then backs off character-by-character. | ✅ Yes (backtracks) |
| **Reluctant (Lazy)** | `X*?`, `X+?`, `X??` | Consumes as little as possible, expanding one character at a time. | ✅ Yes (backtracks) |
| **Possessive** | `X*+`, `X++`, `X?+` | Consumes as much as possible, **never** yields or backtracks. | ❌ No (Zero backtracking) |

---

## 10. Interview Questions

<details>
<summary><strong>1. What is the difference between Pattern.matches() and Matcher.find()?</strong></summary>

`Pattern.matches(regex, input)` (and `str.matches(regex)`) attempts to match the **entire** input sequence against the pattern from start to end (as if anchored with `^...$`).
`Matcher.find()` scans the input sequence looking for the **next subsequence** that matches the pattern.
</details>

<details>
<summary><strong>2. Why is String.matches() considered an anti-pattern in high-throughput applications?</strong></summary>

`String.matches(regex)` internally calls `Pattern.compile(regex).matcher(this).matches()`. Compiling a regular expression creates an AST, parses character classes, and allocates multiple internal nodes. If invoked repeatedly in loops, this causes high CPU overhead and garbage generation. The correct idiom is to compile the pattern once as a `private static final Pattern` and reuse it.
</details>

<details>
<summary><strong>3. Explain Catastrophic Backtracking (ReDoS) and how possessive quantifiers solve it.</strong></summary>

Catastrophic backtracking occurs when an NFA regex engine encounters nested quantifiers with overlapping sets (such as `(a+)+$`) evaluated against non-matching input. The engine evaluates every permutation of how the subpatterns divide the characters, leading to $O(2^N)$ complexity. Possessive quantifiers (e.g. `(a++)++`) consume matching characters greedily and commit permanently to the match, disabling all backtracking.
</details>

<details>
<summary><strong>4. How do you escape arbitrary user input in a regular expression?</strong></summary>

Use `Pattern.quote(String s)`. This wraps the input string in `\Q` and `\E`, instructing the regex engine to treat all metacharacters (`.`, `*`, `+`, `?`, `[`, `{`, etc.) as raw literal characters.
</details>

<details>
<summary><strong>5. What is the difference between greedy (.*) and reluctant (.*?) quantifiers?</strong></summary>

A greedy quantifier consumes as much input as possible upfront and gradually gives up characters backwards to allow subsequent tokens to match. A reluctant quantifier consumes as few characters as possible upfront and expands forward character-by-character only as needed.
Example: On `"<a>foo</a><a>bar</a>"`, `<.*>` matches the entire string, while `<.*?>` matches only `"<a>"`.
</details>

<details>
<summary><strong>6. How do named capturing groups work in Java?</strong></summary>

Introduced in Java 7, named capturing groups use the syntax `(?<groupName>pattern)`. You can retrieve matched text using `matcher.group("groupName")` or reference it in replacement strings using `${groupName}`.
</details>

<details>
<summary><strong>7. How does the Java 9 Matcher.results() method improve regex handling?</strong></summary>

`matcher.results()` returns a `Stream<MatchResult>`. This allows developers to apply Java Stream operations (such as `.map()`, `.filter()`, and `.collect()`) directly to regex matches without writing boilerplate `while (matcher.find())` loops.
</details>

<details>
<summary><strong>8. Is the Pattern class thread-safe? Is the Matcher class thread-safe?</strong></summary>

`Pattern` is completely immutable and thread-safe; a single `Pattern` instance can be shared across all threads in an application. `Matcher` is stateful (maintains current match pointers, group state, and cursor positions) and is **not thread-safe**. Each thread must create its own `Matcher` via `pattern.matcher(input)`.
</details>

---

## 11. Practice Exercises

### Easy
1. **Zip Code Validator:** Write a regex validating 5-digit US postal codes with optional 4-digit extension (`12345` or `12345-6789`).
2. **Extract All Numbers:** Use `matcher.results()` to extract all integers from an unstructured text string into a `List<Integer>`.
3. **Word Counter:** Count total words in a sentence matching `\b\w+\b`.

### Medium
4. **IPv4 Address Validator:** Author a strict regex that validates IPv4 addresses (ensuring each octet is strictly between `0` and `255`).
5. **CamelCase to Kebab-Case Converter:** Convert CamelCase identifiers (`orderItemCount`) to kebab-case (`order-item-count`) using `matcher.replaceAll(Function)`.
6. **Markdown Link Extractor:** Extract URLs and anchor text from Markdown links (`[Anchor Text](https://url.com)`) using named capturing groups.

### Hard
7. **ReDoS Detector & Benchmark:** Write a test verifying that an input regex does not exhibit exponential backtracking by setting an execution deadline.
8. **Syntax Highlighter Lexer:** Implement a Java lexer that identifies keywords, string literals, comments, and numeric constants using a single composite regex with named groups.
9. **Zero-Copy Matcher:** Build a custom matcher utility that works directly over memory-mapped NIO `ByteBuffer` instances.

<details>
<summary><strong>Exercise Solutions</strong></summary>

```java
// Solution to Exercise 1: Zip Code Validator
private static final Pattern ZIP_PATTERN = Pattern.compile("^\\d{5}(-\\d{4})?$");
public static boolean isValidZip(String zip) {
    return zip != null && ZIP_PATTERN.matcher(zip).matches();
}

// Solution to Exercise 5: CamelCase to Kebab-Case
private static final Pattern CAMEL_PATTERN = Pattern.compile("(?<!^)(?=[A-Z])");
public static String toKebabCase(String input) {
    if (input == null) return null;
    return CAMEL_PATTERN.matcher(input).replaceAll("-").toLowerCase(java.util.Locale.ROOT);
}
```
</details>

---

## 12. Summary & Cheat Sheet

```
┌────────────────────────────────────────────────────────────────────────┐
│                        JAVA REGEX CHEAT SHEET                          │
├────────────────────────────────────────────────────────────────────────┤
│ Pattern:            Immutable, thread-safe, compile once (static final)│
│ Matcher:            Stateful, single-threaded cursor engine            │
│ matches()           Matches entire string from start to finish         │
│ find()              Scans for next matching subsequence                │
│ Named groups:       (?<name>...) retrieved via matcher.group("name")   │
│ Stream API:         matcher.results() -> Stream<MatchResult> (Java 9)  │
│ Lambda replace:     matcher.replaceAll(mr -> mr.group().toUpperCase()) │
│ Possessive:         X++, X*+ (Eliminates ReDoS backtracking)           │
│ Literal quote:      Pattern.quote(str) escapes all metacharacters      │
│ Escape requirement: \\d, \\s, \\w (Java strings require double backslash│
└────────────────────────────────────────────────────────────────────────┘
```

---

## 13. Further Reading
- Official Java 25 `java.util.regex.Pattern` Documentation
- OWASP Regular Expression Denial of Service (ReDoS) Prevention Guide
- *Mastering Regular Expressions (3rd Edition)* by Jeffrey E.F. Friedl
